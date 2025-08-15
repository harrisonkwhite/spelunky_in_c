#include "game.h"

#define GRAVITY 0.4f

#define PLAYER_MOVE_SPD 2.0f
#define PLAYER_MOVE_SPD_LERP 0.2f
#define PLAYER_JUMP_HEIGHT 4.0f
#define PLAYER_SIZE (s_v2_s32){TILE_SIZE, TILE_SIZE}

typedef enum {
    ek_tilemap_room_type_extra,
    ek_tilemap_room_type_left_right_exits,
    ek_tilemap_room_type_left_right_top_exits,
    ek_tilemap_room_type_left_right_bottom_exits,
    ek_tilemap_room_type_left_right_bottom_top_exits
} e_tilemap_room_type;

typedef e_tilemap_room_type t_tilemap_room_types[TILEMAP_ROOMS_VERT][TILEMAP_ROOMS_HOR];

static void GenTilemapRoomTypes(t_tilemap_room_types* const room_types, t_s32* const starting_room_x) {
    assert(IS_ZERO(*room_types));
    assert(IS_ZERO(*starting_room_x));

    *starting_room_x = RandRangeS32(0, TILEMAP_ROOMS_HOR);
    s_v2_s32 pen = {*starting_room_x, 0};

    while (true) {
        e_tilemap_room_type* const rt_old = STATIC_ARRAY_2D_ELEM(*room_types, pen.y, pen.x);

        if (*rt_old == ek_tilemap_room_type_extra) {
            *rt_old = ek_tilemap_room_type_left_right_exits;
        }

        const s_v2_s32 pen_old = pen;

        const int rand = RandRangeS32Incl(1, 5);

        if (rand == 1 || rand == 2) {
            pen.x++;

            if (pen.x == TILEMAP_ROOMS_HOR) {
                pen.x--;
                pen.y++;
            }
        } else if (rand == 3 || rand == 4) {
            pen.x--;

            if (pen.x == -1) {
                pen.x++;
                pen.y++;
            }
        } else {
            pen.y++;
        }

        if (pen.y == TILEMAP_ROOMS_VERT) {
            break;
        }

        e_tilemap_room_type* const rt = STATIC_ARRAY_2D_ELEM(*room_types, pen.y, pen.x);

        if (pen.y == pen_old.y + 1) {
            *rt = ek_tilemap_room_type_left_right_top_exits;

            if (*rt_old == ek_tilemap_room_type_left_right_exits) {
                *rt_old = ek_tilemap_room_type_left_right_bottom_exits;
            } else if (*rt_old == ek_tilemap_room_type_left_right_top_exits) {
                *rt_old = ek_tilemap_room_type_left_right_bottom_top_exits;
            } else {
                assert(false);
            }
        }
    }
}

static bool WARN_UNUSED_RESULT GenTilemap(s_tilemap* const tm, s_mem_arena* const temp_mem_arena) {
    assert(IS_ZERO(*tm));

    t_tilemap_room_types room_types = {0};
    GenTilemapRoomTypes(&room_types, &tm->starting_room_x);

    s_rgba_texture rooms_tex = {0};

    if (!LoadRGBATextureFromRawFile(&rooms_tex, temp_mem_arena, (s_char_array_view)ARRAY_FROM_STATIC("rooms.png"))) {
        return false;
    }

#if 0
    for (int py = 0; py < rooms_tex.tex_size.y; py++) {
        for (int px = 0; px < rooms_tex.tex_size.x; px++) {
            const int px_index = (py * rooms_tex.tex_size.x) + px;
            const int r = *U8Elem(rooms_tex.px_data, (px_index * 4) + 0);
            const int g = *U8Elem(rooms_tex.px_data, (px_index * 4) + 1);
            const int b = *U8Elem(rooms_tex.px_data, (px_index * 4) + 2);
            const int a = *U8Elem(rooms_tex.px_data, (px_index * 4) + 3);

            LOG("rgba: %d, %d, %d, %d", r, g, b, a);
        }
    }
#endif

    for (int ry = 0; ry < TILEMAP_ROOMS_VERT; ry++) {
        for (int rx = 0; rx < TILEMAP_ROOMS_HOR; rx++) {
            const e_tilemap_room_type rt = *STATIC_ARRAY_2D_ELEM(room_types, ry, rx);

            const int rooms_tex_tl_x = TILEMAP_ROOM_WIDTH * rt;
            const int rooms_tex_tl_y = 0;

            for (int tyo = 0; tyo < TILEMAP_ROOM_HEIGHT; tyo++) {
                for (int txo = 0; txo < TILEMAP_ROOM_WIDTH; txo++) {
                    const int tex_x = rooms_tex_tl_x + txo;
                    const int tex_y = rooms_tex_tl_y + tyo;
                    const int tex_px_index = (tex_y * rooms_tex.tex_size.x) + tex_x;

                    const t_u8 tex_px_alpha = *U8Elem(rooms_tex.px_data, (tex_px_index * 4) + 3);

                    if (tex_px_alpha > 0) {
                        const int tx = (rx * TILEMAP_ROOM_WIDTH) + txo;
                        const int ty = (ry * TILEMAP_ROOM_HEIGHT) + tyo;
                        *STATIC_ARRAY_2D_ELEM(tm->states, ty, tx) = ek_tile_state_dirt;
                    }
                }
            }
        }
    }

    return true;
}

static s_rect GenPlayerRect(const s_v2 player_pos) {
    return (s_rect){player_pos.x - (PLAYER_SIZE.x / 2.0f), player_pos.y - (PLAYER_SIZE.y / 2.0f), PLAYER_SIZE.x, PLAYER_SIZE.y};
}

static s_rect GenTileRect(const int tx, const int ty) {
    return (s_rect){tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
}

static bool CheckTileCollision(const s_rect rect, const s_tilemap* const tm) {
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = *STATIC_ARRAY_2D_ELEM(tm->states, ty, tx);

            if (ts == ek_tile_state_empty) {
                continue;
            }

            const s_rect tr = GenTileRect(tx, ty);

            if (DoRectsInters(rect, tr)) {
                return true;
            }
        }
    }

    return false;
}

static void ProcTileCollisions(s_v2* const vel, const s_rect rect_base, const s_tilemap* const tm) {
    const s_rect rect_hor = RectTranslated(rect_base, (s_v2){vel->x, 0.0f});

    if (CheckTileCollision(rect_hor, tm)) {
        vel->x = 0.0f;
    }

    const s_rect rect_vert = RectTranslated(rect_base, (s_v2){0.0f, vel->y});

    if (CheckTileCollision(rect_vert, tm)) {
        vel->y = 0.0f;
    }

    const s_rect rect_diag = RectTranslated(rect_base, *vel);

    if (CheckTileCollision(rect_diag, tm)) {
        vel->x = 0.0f;
    }
}

bool GenLevel(s_level* const lvl, s_mem_arena* const temp_mem_arena) {
    assert(IS_ZERO(*lvl));

    if (!GenTilemap(&lvl->tilemap, temp_mem_arena)) {
        return false;
    }

    lvl->player.pos = (s_v2){
        (lvl->tilemap.starting_room_x + 0.5f) * TILEMAP_ROOM_WIDTH * TILE_SIZE,
        0.5f * TILEMAP_ROOM_HEIGHT * TILE_SIZE
    };

    return true;
}

void UpdateLevel(s_level* const lvl, const s_game_tick_context* const zfw_context) {
    //
    // Player Movement
    //
    {
        s_player* const player = &lvl->player;

        const int move_axis = IsKeyDown(&zfw_context->input_context, ek_key_code_right) - IsKeyDown(&zfw_context->input_context, ek_key_code_left);

        const float vel_x_dest = PLAYER_MOVE_SPD * move_axis;
        player->vel.x = Lerp(player->vel.x, vel_x_dest, PLAYER_MOVE_SPD_LERP);

        lvl->player.vel.y += GRAVITY;

        if (IsKeyPressed(&zfw_context->input_context, ek_key_code_up)) {
            lvl->player.vel.y = -PLAYER_JUMP_HEIGHT;
        }

        ProcTileCollisions(&player->vel, GenPlayerRect(player->pos), &lvl->tilemap);

        lvl->player.pos = V2Sum(lvl->player.pos, lvl->player.vel);
    }
}

void RenderLevel(s_level* const lvl, const s_rendering_context* const rc) {
    s_matrix_4x4 lvl_view_mat = IdentityMatrix4x4();
    ScaleMatrix4x4(&lvl_view_mat, CAMERA_SCALE);
    SetViewMatrix(rc, &lvl_view_mat);

    //
    // Tilemap
    //
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = *STATIC_ARRAY_2D_ELEM(lvl->tilemap.states, ty, tx);

            if (ts == ek_tile_state_empty) {
                continue;
            }

            const s_rect rect = {tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            RenderRectWithOutlineAndOpaqueFill(rc, rect, WHITE.rgb, BLACK, 1.0f);
        }
    }

    //
    // Player
    //
    {
        const s_rect rect = GenPlayerRect(lvl->player.pos);
        RenderRectWithOutlineAndOpaqueFill(rc, rect, WHITE.rgb, BLACK, 1.0f);
    }
}
