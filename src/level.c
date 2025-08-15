#include "game.h"

#define GRAVITY 0.4f

#define PLAYER_MOVE_SPD 2.0f
#define PLAYER_MOVE_SPD_LERP 0.2f
#define PLAYER_JUMP_HEIGHT 4.0f
#define PLAYER_CLIMB_SPD 1.0f
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

                    const t_u8 tex_px_r = *U8Elem(rooms_tex.px_data, (tex_px_index * 4) + 0);
                    const t_u8 tex_px_g = *U8Elem(rooms_tex.px_data, (tex_px_index * 4) + 1);
                    const t_u8 tex_px_b = *U8Elem(rooms_tex.px_data, (tex_px_index * 4) + 2);
                    const t_u8 tex_px_a = *U8Elem(rooms_tex.px_data, (tex_px_index * 4) + 3);

                    const int tx = (rx * TILEMAP_ROOM_WIDTH) + txo;
                    const int ty = (ry * TILEMAP_ROOM_HEIGHT) + tyo;

                    if (tex_px_r == 0 && tex_px_g == 0 && tex_px_b == 0 && tex_px_a == 255) {
                        *STATIC_ARRAY_2D_ELEM(tm->states, ty, tx) = ek_tile_state_dirt;
                    } else if (tex_px_r == 255 && tex_px_g == 0 && tex_px_b == 0 && tex_px_a == 255) {
                        *STATIC_ARRAY_2D_ELEM(tm->states, ty, tx) = ek_tile_state_ladder;
                    }
                }
            }
        }
    }

    // Add boundaries.
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        *STATIC_ARRAY_2D_ELEM(tm->states, ty, 0) = ek_tile_state_dirt;
        *STATIC_ARRAY_2D_ELEM(tm->states, ty, TILEMAP_WIDTH - 1) = ek_tile_state_dirt;
    }

    for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
        *STATIC_ARRAY_2D_ELEM(tm->states, 0, tx) = ek_tile_state_dirt;
        *STATIC_ARRAY_2D_ELEM(tm->states, TILEMAP_HEIGHT - 1, tx) = ek_tile_state_dirt;
    }

    return true;
}

static s_rect GenPlayerRect(const s_v2 player_pos) {
    return (s_rect){player_pos.x - (PLAYER_SIZE.x / 2.0f), player_pos.y - (PLAYER_SIZE.y / 2.0f), PLAYER_SIZE.x, PLAYER_SIZE.y};
}

static s_rect GenTileRect(const int tx, const int ty) {
    return (s_rect){tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
}

static bool CheckSolidTileCollision(const s_rect rect, const s_tilemap* const tm) {
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = *STATIC_ARRAY_2D_ELEM(tm->states, ty, tx);

            if (!(*STATIC_ARRAY_ELEM(g_tile_states_solid, ts))) {
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

static bool CheckTileCollisionWithState(const s_rect rect, const s_tilemap* const tm, const e_tile_state state) {
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = *STATIC_ARRAY_2D_ELEM(tm->states, ty, tx);

            if (ts != state) {
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

    if (CheckSolidTileCollision(rect_hor, tm)) {
        vel->x = 0.0f;
    }

    const s_rect rect_vert = RectTranslated(rect_base, (s_v2){0.0f, vel->y});

    if (CheckSolidTileCollision(rect_vert, tm)) {
        vel->y = 0.0f;
    }

    const s_rect rect_diag = RectTranslated(rect_base, *vel);

    if (CheckSolidTileCollision(rect_diag, tm)) {
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

        /*if (CheckTileCollision(RectTranslated(GenPlayerRect(lvl->player.pos), (s_v2){1.0f, 0.0f}), &lvl->tilemap)) {
            stuck = true;
        } else {
            stuck = false;
        }*/

        const bool touching_ladder = CheckTileCollisionWithState(GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_ladder);

        if (IsKeyDown(&zfw_context->input_context, ek_key_code_up)) {
            if (touching_ladder) {
                lvl->player.vel.y = -PLAYER_CLIMB_SPD;
                lvl->player.climbing = true;
            }
        }

        if (!touching_ladder) {
            lvl->player.climbing = false;
        }

        if (!lvl->player.climbing) {
            lvl->player.vel.y += GRAVITY;
        }

        if (CheckSolidTileCollision(RectTranslated(GenPlayerRect(lvl->player.pos), (s_v2){0.0f, 1.0f}), &lvl->tilemap)) {
            if (IsKeyPressed(&zfw_context->input_context, ek_key_code_space)) {
                lvl->player.vel.y = -PLAYER_JUMP_HEIGHT;
            }
        }

        ProcTileCollisions(&player->vel, GenPlayerRect(player->pos), &lvl->tilemap);

        lvl->player.pos = V2Sum(lvl->player.pos, lvl->player.vel);

        
    }
}

void RenderLevel(s_level* const lvl, const s_rendering_context* const rc) {
    const s_v2_s32 view_size = {rc->window_size.x / CAMERA_SCALE, rc->window_size.y / CAMERA_SCALE};

    s_matrix_4x4 lvl_view_mat = IdentityMatrix4x4();
    TranslateMatrix4x4(&lvl_view_mat, (s_v2){(-lvl->player.pos.x + (view_size.x / 2.0f)) * CAMERA_SCALE, (-lvl->player.pos.y + (view_size.y / 2.0f)) * CAMERA_SCALE});
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

            u_v3 col;

            switch (ts) {
                case ek_tile_state_dirt:
                    col = WHITE.rgb;
                    break;

                case ek_tile_state_ladder:
                    col = RED.rgb;
                    break;
            }

            RenderRectWithOutlineAndOpaqueFill(rc, rect, col, BLACK, 1.0f);
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
