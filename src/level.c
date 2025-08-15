#include "game.h"

#define GRAVITY 0.4f

#define PLAYER_MOVE_SPD 2.0f
#define PLAYER_MOVE_SPD_LERP 0.2f
#define PLAYER_JUMP_HEIGHT 4.0f
#define PLAYER_CLIMB_SPD 1.0f
#define PLAYER_SIZE (s_v2_s32){TILE_SIZE, TILE_SIZE}
#define PLAYER_ORIGIN (s_v2){0.5f, 0.5f}

#define ARROW_SIZE (s_v2_s32){TILE_SIZE, TILE_SIZE / 2}
#define ARROW_ORIGIN (s_v2){0.5f, 0.5f}
#define ARROW_SPD 8.0f

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
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_dirt;
                    } else if (tex_px_r == 255 && tex_px_g == 0 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_ladder;
                    } else if (tex_px_r == 255 && tex_px_g == 255 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_gold;
                    } else if (tex_px_r == 0 && tex_px_g == 255 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_shooter;
                    } else {
                        assert(tex_px_r == 0 && tex_px_g == 0 && tex_px_b == 0 && tex_px_a == 0 && "forgettinga tile!");
                    }
                }
            }
        }
    }

    // Add boundaries.
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, 0)->state = ek_tile_state_dirt;
        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, TILEMAP_WIDTH - 1)->state = ek_tile_state_dirt;
    }

    for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
        STATIC_ARRAY_2D_ELEM(tm->tiles, 0, tx)->state = ek_tile_state_dirt;
        STATIC_ARRAY_2D_ELEM(tm->tiles, TILEMAP_HEIGHT - 1, tx)->state = ek_tile_state_dirt;
    }

    return true;
}

static s_rect GenPlayerRect(const s_v2 player_pos) {
    return (s_rect){player_pos.x - (PLAYER_SIZE.x * PLAYER_ORIGIN.x), player_pos.y - (PLAYER_SIZE.y * PLAYER_ORIGIN.y), PLAYER_SIZE.x, PLAYER_SIZE.y};
}

static s_rect GenArrowRect(const s_v2 arrow_pos) {
    return (s_rect){arrow_pos.x - (ARROW_SIZE.x * ARROW_ORIGIN.x), arrow_pos.y - (ARROW_SIZE.y * ARROW_ORIGIN.y), ARROW_SIZE.x, ARROW_SIZE.y};
}

static s_rect GenTileRect(const int tx, const int ty) {
    return (s_rect){tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
}

static bool CheckSolidTileCollision(const s_rect rect, const s_tilemap* const tm, const bool ignore_shooters) {
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state;

            if (!(*STATIC_ARRAY_ELEM(g_tile_states_solid, ts))) {
                continue;
            }

            if (ignore_shooters && ts == ek_tile_state_shooter) {
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

static bool CheckTileCollisionWithState(s_v2_s32* const tp, const s_rect rect, const s_tilemap* const tm, const e_tile_state state) {
    assert(!tp || IS_ZERO(*tp));

    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state;

            if (ts != state) {
                continue;
            }

            const s_rect tr = GenTileRect(tx, ty);

            if (DoRectsInters(rect, tr)) {
                if (tp) {
                    *tp = (s_v2_s32){tx, ty};
                }

                return true;
            }
        }
    }

    return false;
}

static s_rect GenCollider(const s_v2 pos, const s_v2 size, const s_v2 origin) {
    return (s_rect){pos.x - (size.x * origin.x), pos.y - (size.y * origin.y), size.x, size.y};
}

static void MoveToSolidTile(s_v2* const pos, const e_cardinal_dir dir, const s_v2 collider_size, const s_v2 collider_origin, const s_tilemap* const tm) {
    s_v2 jump;

    switch (dir) {
        case ek_cardinal_dir_right:
            jump = (s_v2){1.0f, 0.0f};
            break;

        case ek_cardinal_dir_left:
            jump = (s_v2){-1.0f, 0.0f};
            break;

        case ek_cardinal_dir_down:
            jump = (s_v2){0.0f, 1.0f};
            break;

        case ek_cardinal_dir_up:
            jump = (s_v2){0.0f, -1.0f};
            break;
    }

    while (!CheckSolidTileCollision(GenCollider((s_v2){pos->x + jump.x, pos->y + jump.y}, collider_size, collider_origin), tm, false)) {
        pos->x += jump.x;
        pos->y += jump.y;
    }
}

static void ProcSolidTileCollisions(s_v2* const pos, s_v2* const vel, const s_v2 collider_size, const s_v2 collider_origin, const s_tilemap* const tm) {
    const s_rect rect_hor = GenCollider((s_v2){pos->x + vel->x, pos->y}, collider_size, collider_origin);

    if (CheckSolidTileCollision(rect_hor, tm, false)) {
        MoveToSolidTile(pos, vel->x >= 0.0f ? ek_cardinal_dir_right : ek_cardinal_dir_left, collider_size, collider_origin, tm);
        vel->x = 0.0f;
    }

    const s_rect rect_vert = GenCollider((s_v2){pos->x, pos->y + vel->y}, collider_size, collider_origin);

    if (CheckSolidTileCollision(rect_vert, tm, false)) {
        MoveToSolidTile(pos, vel->y >= 0.0f ? ek_cardinal_dir_down : ek_cardinal_dir_up, collider_size, collider_origin, tm);
        vel->y = 0.0f;
    }

    const s_rect rect_diag = GenCollider((s_v2){pos->x + vel->x, pos->y + vel->y}, collider_size, collider_origin);

    if (CheckSolidTileCollision(rect_diag, tm, false)) {
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

    lvl->hp = HP_LIMIT;

    return true;
}

static void SpawnArrow(s_level* const lvl, const s_v2 pos, const bool right) {
    if (lvl->arrow_cnt < STATIC_ARRAY_LEN(lvl->arrows)) {
        s_arrow* const arrow = STATIC_ARRAY_ELEM(lvl->arrows, lvl->arrow_cnt);
        arrow->pos = pos;
        arrow->right = false;
        lvl->arrow_cnt++;
    } else {
        assert(false);
    }
}

void UpdateLevel(s_level* const lvl, const s_game_tick_context* const zfw_context) {
    if (lvl->hp > 0) {
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

            const bool on_ground = CheckSolidTileCollision(RectTranslated(GenPlayerRect(lvl->player.pos), (s_v2){0.0f, 1.0f}), &lvl->tilemap, false);
            const bool touching_ladder = CheckTileCollisionWithState(NULL, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_ladder);

            if (on_ground) {
                lvl->player.cant_climb = false;
            }

            if (touching_ladder) {
                if (!lvl->player.cant_climb && IsKeyDown(&zfw_context->input_context, ek_key_code_up)) {
                    lvl->player.climbing = true;
                }
            } else {
                if (lvl->player.climbing) {
                    lvl->player.climbing = false;
                    lvl->player.cant_climb = true;
                }
            }

            if (lvl->player.climbing) {
                if (IsKeyDown(&zfw_context->input_context, ek_key_code_up)) {
                    lvl->player.vel.y = -PLAYER_CLIMB_SPD;
                } else {
                    lvl->player.vel.y = 0.0f;
                }
            } else {
                lvl->player.vel.y += GRAVITY;
            }

            if (on_ground) {
                if (IsKeyPressed(&zfw_context->input_context, ek_key_code_space)) {
                    lvl->player.vel.y = -PLAYER_JUMP_HEIGHT;
                }
            }

            ProcSolidTileCollisions(&player->pos, &player->vel, (s_v2){PLAYER_SIZE.x, PLAYER_SIZE.y}, PLAYER_ORIGIN, &lvl->tilemap);

            lvl->player.pos = V2Sum(lvl->player.pos, lvl->player.vel);

            // Check for gold!
            s_v2_s32 gold_tile_pos = {0};

            if (CheckTileCollisionWithState(&gold_tile_pos, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_gold)) {
                STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, gold_tile_pos.y, gold_tile_pos.x)->state = ek_tile_state_empty;
                lvl->gold_cnt++;
            }
        }

        //
        // Shooter Tiles
        //
        {
            const int player_tile_x = lvl->player.pos.x / TILE_SIZE;
            const int player_tile_y = lvl->player.pos.y / TILE_SIZE;

            // Check to player right.
            for (int tx = player_tile_x + 1; ; tx++) {
                s_tile* const tile = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, player_tile_y, tx);

                if (tile->state == ek_tile_state_shooter && !tile->shooter_shot && !tile->shooter_facing_right) {
                    SpawnArrow(lvl, (s_v2){(tx + 0.5f) * TILE_SIZE, (player_tile_y + 0.5f) * TILE_SIZE}, false);
                    tile->shooter_shot = true;
                    break;
                }

                if (*STATIC_ARRAY_ELEM(g_tile_states_solid, tile->state)) {
                    break;
                }
            }

            // Check to player left.
            for (int tx = player_tile_x + 1; ; tx++) {
                s_tile* const tile = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, player_tile_y, tx);

                if (tile->state == ek_tile_state_shooter && !tile->shooter_shot && tile->shooter_facing_right) {
                    SpawnArrow(lvl, (s_v2){(tx + 0.5f) * TILE_SIZE, (player_tile_y + 0.5f) * TILE_SIZE}, true);
                    tile->shooter_shot = true;
                    break;
                }

                if (*STATIC_ARRAY_ELEM(g_tile_states_solid, tile->state)) {
                    break;
                }
            }
        }
    }

    //
    // Arrows
    //
    {
        const s_rect player_collider = GenPlayerRect(lvl->player.pos);

        for (int i = 0; i < lvl->arrow_cnt; i++) {
            s_arrow* const arrow = STATIC_ARRAY_ELEM(lvl->arrows, i);
            arrow->pos.x += ARROW_SPD * (arrow->right ? 1.0f : -1.0f);

            const s_rect collider = GenArrowRect(arrow->pos);

            if (CheckSolidTileCollision(collider, &lvl->tilemap, true)) {
                *arrow = *STATIC_ARRAY_ELEM(lvl->arrows, lvl->arrow_cnt - 1);
                lvl->arrow_cnt--;
                i--;
            }

            if (lvl->hp > 0 && DoRectsInters(collider, player_collider)) {
                lvl->hp = 0; // TEMP
            }
        }
    }

    //
    // Player Death
    //
    assert(lvl->hp >= 0);

    if (lvl->hp == 0) {
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
            const e_tile_state ts = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx)->state;

            if (ts == ek_tile_state_empty) {
                continue;
            }

            const s_rect rect = {tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};

            u_v3 col;

            switch (ts) {
                case ek_tile_state_dirt:
                    col = BROWN.rgb;
                    break;

                case ek_tile_state_ladder:
                    col = RED.rgb;
                    break;

                case ek_tile_state_gold:
                    col = YELLOW.rgb;
                    break;

                case ek_tile_state_shooter:
                    col = GRAY.rgb;
                    break;

                default:
                    assert(false && "forgetting tile type");
                    break;
            }

            RenderRectWithOutlineAndOpaqueFill(rc, rect, col, BLACK, 1.0f);
        }
    }

    //
    // Player
    //
    if (lvl->hp > 0) {
        const s_rect rect = GenPlayerRect(lvl->player.pos);
        RenderRectWithOutlineAndOpaqueFill(rc, rect, WHITE.rgb, BLACK, 1.0f);
    }

    //
    // Arrows
    //
    for (int i = 0; i < lvl->arrow_cnt; i++) {
        const s_arrow* const arrow = STATIC_ARRAY_ELEM(lvl->arrows, i);
        const s_rect rect = GenArrowRect(arrow->pos);
        RenderRectWithOutlineAndOpaqueFill(rc, rect, LIGHT_GRAY.rgb, BLACK, 1.0f);
    }

    //
    // Resetting View Matrix
    //
    {
        s_matrix_4x4 identity_mat = IdentityMatrix4x4();
        SetViewMatrix(rc, &identity_mat);
    }
}

bool RenderLevelUI(s_level* const lvl, const s_rendering_context* const rc, const s_font_group* const fonts, s_mem_arena* const temp_mem_arena) {
    //
    // Gold
    //
    char gold_str[8];
    snprintf(gold_str, sizeof(gold_str), "GOLD: %d", lvl->gold_cnt);

    if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC(gold_str), fonts, ek_font_medodica_96, (s_v2){0}, ALIGNMENT_TOP_LEFT, WHITE, temp_mem_arena)) {
        return false;
    }

    //
    // Death
    //
    if (lvl->hp == 0) {
        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC(DEATH_MSG), fonts, ek_font_medodica_128, (s_v2){rc->window_size.x / 2.0f, rc->window_size.y / 2.0f}, ALIGNMENT_CENTER, WHITE, temp_mem_arena)) {
            return false;
        }
    }

    return true;
}
