#include "game.h"

// climbing notes
// you mark what specific tile to latch onto when pressing right and while higher than it. the moment the top of your collider is less than the tile, you are locked back up into a latch state. only latch onto tile with nothing above it.

#define GRAVITY 0.2f

#define PLAYER_MOVE_SPD 1.5f
#define PLAYER_MOVE_SPD_LERP 0.2f
#define PLAYER_JUMP_HEIGHT 3.0f
#define PLAYER_CLIMB_SPD 1.0f
#define PLAYER_ORIGIN (s_v2){0.5f, 0.5f}
#define PLAYER_WHIP_OFFS 10.0f

#define ARROW_SIZE (s_v2_s32){TILE_SIZE, TILE_SIZE / 2}
#define ARROW_ORIGIN (s_v2){0.5f, 0.5f}
#define ARROW_SPD 4.0f

#define SNAKE_ORIGIN (s_v2){0.5f, 0.5f}
#define SNAKE_MOVE_SPD 0.5f
#define SNAKE_MOVE_SPD_LERP 0.1f
#define SNAKE_AHEAD_DIST 4.0f

typedef enum {
    ek_tilemap_room_type_extra,
    ek_tilemap_room_type_entry,
    ek_tilemap_room_type_left_right_exits,
    ek_tilemap_room_type_left_right_top_exits,
    ek_tilemap_room_type_left_right_bottom_exits,
    ek_tilemap_room_type_left_right_bottom_top_exits,
    ek_tilemap_room_type_end_with_left_right_exits,
    ek_tilemap_room_type_end_with_left_right_top_exits
} e_tilemap_room_type;

typedef e_tilemap_room_type t_tilemap_room_types[TILEMAP_ROOMS_VERT][TILEMAP_ROOMS_HOR];

static void GenTilemapRoomTypes(t_tilemap_room_types* const room_types, t_s32* const starting_room_x) {
    assert(IS_ZERO(*room_types));
    assert(IS_ZERO(*starting_room_x));

    *starting_room_x = RandRangeS32(0, TILEMAP_ROOMS_HOR);
    s_v2_s32 pen = {*starting_room_x, 0};
    *STATIC_ARRAY_2D_ELEM(*room_types, pen.y, pen.x) = ek_tilemap_room_type_entry;

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
                continue;
                //pen.y++;
            }
        } else if (rand == 3 || rand == 4) {
            pen.x--;

            if (pen.x == -1) {
                pen.x++;
                continue;
                //pen.y++;
            }
        } else {
            if (pen.y == 0 && pen.x == *starting_room_x) {
                continue;
            }

            pen.y++;
        }

        if (pen.y == TILEMAP_ROOMS_VERT) {
            if (*rt_old == ek_tilemap_room_type_left_right_exits) {
                *rt_old = ek_tilemap_room_type_end_with_left_right_exits;
            } else if (*rt_old == ek_tilemap_room_type_left_right_top_exits) {
                *rt_old = ek_tilemap_room_type_end_with_left_right_top_exits;
            }

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

static void SpawnEnemy(s_level* const lvl, const s_v2 pos, const e_enemy_type enemy_type) {
    int enemy_index = -1;

    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->enemies); i++) {
        const s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, i);

        if (!enemy->active) {
            enemy_index = i;
            break;
        }
    }

    if (enemy_index == -1) {
        LOG_WARNING("out of enemy space");
        return;
    }

    s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, enemy_index);

    *enemy = (s_enemy){
        .pos = pos,
        .type = enemy_type,
        .active = true
    };

    switch (enemy->type) {
        case ek_enemy_type_snake:
            enemy->snake_type.move_axis = RandPerc() < 0.5f ? 1 : -1;
            break;
    }
}

static s_v2_s32 PlayerSize() {
    return RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_player)->src_rect);
}

static s_rect GenPlayerRect(const s_v2 player_pos) {
    const s_v2_s32 size = RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_player)->src_rect);
    return (s_rect){player_pos.x - (size.x * PLAYER_ORIGIN.x), player_pos.y - (size.y * PLAYER_ORIGIN.y), size.x, size.y};
}

static s_v2_s32 SnakeSize() {
    return RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_snake_enemy)->src_rect);
}

static s_rect GenEnemyRect(const s_v2 enemy_pos, const e_enemy_type enemy_type) {
    switch (enemy_type) {
        case ek_enemy_type_snake:
            {
                const s_v2_s32 size = RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_snake_enemy)->src_rect);
                return (s_rect){enemy_pos.x - (size.x * SNAKE_ORIGIN.x), enemy_pos.y - (size.y * SNAKE_ORIGIN.y), size.x, size.y};
            }
    }
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

static void SpawnHitbox(s_level* const lvl, const s_v2 pos, const s_v2 size, const s_v2 origin, const int dmg, const bool enemy) {
    assert(dmg > 0);

    if (lvl->hitbox_cnt == STATIC_ARRAY_LEN(lvl->hitboxes)) {
        LOG_WARNING("out of hitbox space");
        return;
    }

    *STATIC_ARRAY_ELEM(lvl->hitboxes, lvl->hitbox_cnt) = (s_hitbox){
        .rect = GenCollider(pos, size, origin),
        .dmg = dmg,
        .enemy = enemy
    };

    lvl->hitbox_cnt++;
}

static void MoveToSolidTile(s_v2* const pos, const e_cardinal_dir dir, const s_v2 collider_size, const s_v2 collider_origin, const s_tilemap* const tm) {
    s_v2 jump;

    switch (dir) {
        case ek_cardinal_dir_right:
            jump = (s_v2){1.0f / g_view_scale, 0.0f};
            break;

        case ek_cardinal_dir_left:
            jump = (s_v2){-1.0f / g_view_scale, 0.0f};
            break;

        case ek_cardinal_dir_down:
            jump = (s_v2){0.0f, 1.0f / g_view_scale};
            break;

        case ek_cardinal_dir_up:
            jump = (s_v2){0.0f, -1.0f / g_view_scale};
            break;
    }

    while (!CheckSolidTileCollision(GenCollider((s_v2){pos->x + jump.x, pos->y + jump.y}, collider_size, collider_origin), tm, false)) {
        pos->x += jump.x;
        pos->y += jump.y;
    }

#if 0
    pos->x = roundf(pos->x);
    pos->y = roundf(pos->y);
#endif
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

bool GenLevel(s_level* const lvl, const s_v2_s32 window_size, s_mem_arena* const temp_mem_arena) {
    assert(IS_ZERO(*lvl));

    s_tilemap* const tm = &lvl->tilemap;

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

                    const s_v2 tile_center_in_lvl = (s_v2){(tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE};

                    if (tex_px_r == 0 && tex_px_g == 0 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_dirt;
                    } else if (tex_px_r == 255 && tex_px_g == 0 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_ladder;
                    } else if (tex_px_r == 255 && tex_px_g == 255 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_gold;
                    } else if (tex_px_r == 0 && tex_px_g == 255 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_shooter;
                    } else if (tex_px_r == 0 && tex_px_g == 0 && tex_px_b == 255 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_entrance;
                        lvl->player.pos = tile_center_in_lvl;
                    } else if (tex_px_r == 255 && tex_px_g == 0 && tex_px_b == 255 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_exit;
                    } else if (tex_px_r == 0 && tex_px_g == 255 && tex_px_b == 255 && tex_px_a == 255) {
                        SpawnEnemy(lvl, tile_center_in_lvl, ek_enemy_type_snake);
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

    //
    lvl->view_pos = lvl->player.pos;

    const s_v2_s32 view_size = {window_size.x / g_view_scale, window_size.y / g_view_scale};
    lvl->view_pos.x = CLAMP(lvl->view_pos.x, view_size.x / 2.0f, (TILEMAP_WIDTH * TILE_SIZE) - (view_size.x / 2.0f));
    lvl->view_pos.y = CLAMP(lvl->view_pos.y, view_size.y / 2.0f, (TILEMAP_HEIGHT * TILE_SIZE) - (view_size.y / 2.0f));

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

static inline bool DoesPlayerExist(const s_level* const lvl) {
    return lvl->hp > 0 && !lvl->completed;
}

void UpdateLevel(s_level* const lvl, const s_game_tick_context* const zfw_context) {
    lvl->hitbox_cnt = 0;

    if (DoesPlayerExist(lvl)) {
        //
        // Player Movement
        //
        {
            s_player* const player = &lvl->player;

            const int move_axis = IsKeyDown(&zfw_context->input_context, ek_key_code_right) - IsKeyDown(&zfw_context->input_context, ek_key_code_left);

            if (move_axis == 1) {
                player->facing_right = true;
            } else if (move_axis == -1) {
                player->facing_right = false;
            }

            const float vel_x_dest = PLAYER_MOVE_SPD * move_axis;
            player->vel.x = Lerp(player->vel.x, vel_x_dest, PLAYER_MOVE_SPD_LERP);

            /*if (CheckTileCollision(RectTranslated(GenPlayerRect(lvl->player.pos), (s_v2){1.0f, 0.0f}), &lvl->tilemap)) {
                stuck = true;
            } else {
                stuck = false;
            }*/

            bool possible_latch = false;

            if (IsKeyDown(&zfw_context->input_context, ek_key_code_right)) {
                const s_rect rect = GenPlayerRect(player->pos);

                int ty_min = rect.y / TILE_SIZE;
                int ty_max = ceilf((rect.y + rect.height) / TILE_SIZE);

                for (int ty = ty_min; ty < ty_max; ty++) {
                }
            }

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
                if (IsKeyPressed(&zfw_context->input_context, ek_key_code_up)) {
                    lvl->player.vel.y = -PLAYER_JUMP_HEIGHT;
                }
            }

            ProcSolidTileCollisions(&player->pos, &player->vel, (s_v2){PlayerSize().x, PlayerSize().y}, PLAYER_ORIGIN, &lvl->tilemap);

            lvl->player.pos = V2Sum(lvl->player.pos, lvl->player.vel);

            // Check for gold!
            s_v2_s32 gold_tile_pos = {0};

            if (CheckTileCollisionWithState(&gold_tile_pos, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_gold)) {
                STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, gold_tile_pos.y, gold_tile_pos.x)->state = ek_tile_state_empty;
                lvl->gold_cnt += GOLD_INCR;
            }

            //
            // Whipping
            //
            if (IsKeyPressed(&zfw_context->input_context, ek_key_code_x)) {
                const s_v2 hb_pos = {lvl->player.pos.x + (lvl->player.facing_right ? PLAYER_WHIP_OFFS : -PLAYER_WHIP_OFFS), lvl->player.pos.y};
                SpawnHitbox(lvl, hb_pos, (s_v2){PlayerSize().x, PlayerSize().y}, (s_v2){0.5f, 0.5f}, 1, false);
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

        //
        // End Level
        //
        if (IsKeyPressed(&zfw_context->input_context, ek_key_code_x)) {
            if (CheckTileCollisionWithState(NULL, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_exit)) {
                lvl->completed = true;
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

            SpawnHitbox(lvl, arrow->pos, (s_v2){ARROW_SIZE.x, ARROW_SIZE.y}, ARROW_ORIGIN, 1, true);
        }
    }

    //
    // Enemies
    //
    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->enemies); i++) {
        s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, i);

        if (!enemy->active) {
            continue;
        }

        const s_rect rect = GenEnemyRect(enemy->pos, enemy->type);

        switch (enemy->type) {
            case ek_enemy_type_snake:
                {
                    const float vel_x_dest = SNAKE_MOVE_SPD * enemy->snake_type.move_axis;
                    enemy->snake_type.vel.x = Lerp(enemy->snake_type.vel.x, vel_x_dest, SNAKE_MOVE_SPD_LERP);

                    const s_rect rect_ahead = RectTranslated(rect, (s_v2){SNAKE_AHEAD_DIST * enemy->snake_type.move_axis, 0.0f});

                    if (CheckSolidTileCollision(rect_ahead, &lvl->tilemap, false)) {
                        enemy->snake_type.move_axis *= -1;
                    }

                    enemy->snake_type.vel.y += GRAVITY;

                    ProcSolidTileCollisions(&enemy->pos, &enemy->snake_type.vel, (s_v2){SnakeSize().x, SnakeSize().y}, SNAKE_ORIGIN, &lvl->tilemap);

                    enemy->pos = V2Sum(enemy->pos, enemy->snake_type.vel);

                    SpawnHitbox(lvl, enemy->pos, (s_v2){SnakeSize().x, SnakeSize().y}, SNAKE_ORIGIN, 1, true);
                }

                break;
        }
    }

    //
    // Processing Player and enemy collisions with hitboxes
    //
    {
        const s_rect player_collider = GenPlayerRect(lvl->player.pos);

        s_rect enemy_colliders[ENEMY_LIMIT] = {0};

        for (int i = 0; i < ENEMY_LIMIT; i++) {
            const s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, i);

            if (!enemy->active) {
                continue;
            }

            *STATIC_ARRAY_ELEM(enemy_colliders, i) = GenEnemyRect(enemy->pos, enemy->type);
        }

        for (int i = 0; i < lvl->hitbox_cnt; i++) {
            const s_hitbox* const hb = STATIC_ARRAY_ELEM(lvl->hitboxes, i);

            if (hb->enemy) {
                if (DoesPlayerExist(lvl) && DoRectsInters(player_collider, hb->rect)) {
                    lvl->hp = MAX(lvl->hp - hb->dmg, 0);
                }
            } else {
                for (int j = 0; j < ENEMY_LIMIT; j++) {
                    s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, i);

                    if (!enemy->active) {
                        continue;
                    }

                    const s_rect enemy_collider = *STATIC_ARRAY_ELEM(enemy_colliders, j);

                    if (DoRectsInters(hb->rect, enemy_collider)) {
                        enemy->active = false;
                    }
                }
            }
        }
    }

    //
    // Player Death
    //
    assert(lvl->hp >= 0);

    if (lvl->hp == 0) {
    }

    //
    // View
    //
    s_v2 view_dest = {
        lvl->player.pos.x,
        lvl->player.pos.y
    };

    const s_v2_s32 view_size = {zfw_context->window_state.size.x / g_view_scale, zfw_context->window_state.size.y / g_view_scale};
    view_dest.x = CLAMP(view_dest.x, view_size.x / 2.0f, (TILEMAP_WIDTH * TILE_SIZE) - (view_size.x / 2.0f));
    view_dest.y = CLAMP(view_dest.y, view_size.y / 2.0f, (TILEMAP_HEIGHT * TILE_SIZE) - (view_size.y / 2.0f));

    const float view_lerp_factor = 0.2f;
    lvl->view_pos.x = Lerp(lvl->view_pos.x, view_dest.x, view_lerp_factor);
    lvl->view_pos.y = Lerp(lvl->view_pos.y, view_dest.y, view_lerp_factor);

    lvl->view_pos.x = CLAMP(lvl->view_pos.x, view_size.x / 2.0f, (TILEMAP_WIDTH * TILE_SIZE) - (view_size.x / 2.0f));
    lvl->view_pos.y = CLAMP(lvl->view_pos.y, view_size.y / 2.0f, (TILEMAP_HEIGHT * TILE_SIZE) - (view_size.y / 2.0f));
}

void RenderLevel(s_level* const lvl, const s_rendering_context* const rc, const s_texture_group* const textures) {
    const s_v2_s32 view_size = {rc->window_size.x / g_view_scale, rc->window_size.y / g_view_scale};

    s_matrix_4x4 lvl_view_mat = IdentityMatrix4x4();
#if NO_WORLD_GEN_DEBUG

    TranslateMatrix4x4(&lvl_view_mat, (s_v2){(-lvl->view_pos.x + view_size.x * 0.5f) * g_view_scale, (-lvl->view_pos.y + view_size.y * 0.5f) * g_view_scale});
    ScaleMatrix4x4(&lvl_view_mat, g_view_scale);
#endif
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

            const s_v2 pos = {tx * TILE_SIZE, ty * TILE_SIZE};

            RenderSprite(rc, textures, *STATIC_ARRAY_ELEM(g_tile_state_sprs, ts), pos, (s_v2){0}, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
        }
    }

    //
    // Player
    //
    if (DoesPlayerExist(lvl)) {
        const s_rect rect = GenPlayerRect(lvl->player.pos);
        RenderSprite(rc, textures, ek_sprite_player, lvl->player.pos, PLAYER_ORIGIN, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
    }

    //
    // Enemies
    //
    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->enemies); i++) {
        const s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, i);

        if (!enemy->active) {
            continue;
        }

        RenderSprite(rc, textures, ek_sprite_snake_enemy, enemy->pos, SNAKE_ORIGIN, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
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
    // Hitboxes (Debug)
    //
#if SHOW_DEBUG_HITBOXES
    for (int i = 0; i < lvl->hitbox_cnt; i++) {
        const s_hitbox* const hb = STATIC_ARRAY_ELEM(lvl->hitboxes, i);
        RenderRect(rc, hb->rect, (u_v4){RED.rgb, 0.5f});
    }
#endif

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
    snprintf(gold_str, sizeof(gold_str), "Gold: %d", lvl->gold_cnt);

    const s_v2 gold_str_pos = {rc->window_size.x * 0.025f, rc->window_size.y * 0.05f};

    if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC(gold_str), fonts, ek_font_roboto_64, gold_str_pos, ALIGNMENT_TOP_LEFT, WHITE, temp_mem_arena)) {
        return false;
    }

    //
    // Death
    //
    if (lvl->hp == 0) {
        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("You died!"), fonts, ek_font_roboto_96, (s_v2){rc->window_size.x / 2.0f, rc->window_size.y / 2.0f}, ALIGNMENT_CENTER, WHITE, temp_mem_arena)) {
            return false;
        }
    }

    //
    // Level Completion
    //
    if (lvl->completed) {
        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("LEVEL COMPLETED"), fonts, ek_font_roboto_96, (s_v2){rc->window_size.x / 2.0f, rc->window_size.y / 2.0f}, ALIGNMENT_CENTER, WHITE, temp_mem_arena)) {
            return false;
        }
    }

    return true;
}
