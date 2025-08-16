#include "game.h"

//
// needed:
//
// - bombs, items that explode when thrown
// - bat enemy
// - lots of level variations
// - problem: UI sometimes obscures stuff in corners like doors!
//
// - money shouldnt be a tile (not necessary)
// - need control over player jump height

// climbing notes
// you mark what specific tile to latch onto when pressing right and while higher than it. the moment the top of your collider is less than the tile, you are locked back up into a latch state. only latch onto tile with nothing above it.

// Check tiles to the right. If the topmost tile has a 

#define POST_START_WAIT_TIME_MAX 60
#define LEAVING_WAIT_TIME_MAX 30

#define GRAVITY 0.4f

#define PARALLAX 0.2f

#define PLAYER_MOVE_SPD 1.5f
#define PLAYER_MOVE_SPD_LERP 0.3f
#define PLAYER_JUMP_HEIGHT 3.0f
#define PLAYER_CLIMB_SPD 1.0f
#define PLAYER_ORIGIN (s_v2){0.5f, 0.5f}
#define PLAYER_WHIP_OFFS 5.0f
#define PLAYER_WHIP_BREAK_TIME 5
#define PLAYER_THROW_SPD (s_v2){3.5f, 1.5f}

#define ARROW_SIZE (s_v2_s32){TILE_SIZE, TILE_SIZE / 2}
#define ARROW_ORIGIN (s_v2){0.5f, 0.5f}
#define ARROW_SPD 5.0f

#define SNAKE_ORIGIN (s_v2){0.5f, 0.5f}
#define SNAKE_MOVE_SPD 0.5f
#define SNAKE_MOVE_SPD_LERP 0.2f

#define DOOR_FRAME_INTERVAL 5
#define DOOR_FRAME_CNT 4

#define ITEM_DROP_ORIGIN (s_v2){0.5f, 0.5f}
#define ITEM_DROP_PICKUP_MAG 0.5f

#define DEATH_FADE_IN_LERP 0.4f

#define GENERAL_UI_FADE_IN_LERP 0.4f

#define GOLD_CNT_DISPLAY_ACCUM 10

static void Shake(s_level* const lvl, const float amount) {
    lvl->view_shake = MAX(lvl->view_shake, amount);
}

static s_rect GenTileCollider(const int tx, const int ty, const e_tile_state ts) {
    if (ts == ek_tile_state_spike) {
        return (s_rect){tx * TILE_SIZE, (ty * TILE_SIZE) + 4, TILE_SIZE, TILE_SIZE - 4};
    } else if (ts == ek_tile_state_gold) {
        return (s_rect){(tx * TILE_SIZE) + 1, (ty * TILE_SIZE) + 6, TILE_SIZE - 2, TILE_SIZE - 6};
    } else {
        return (s_rect){tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
    }
}

static bool CheckSolidTileCollision(const s_rect rect, const float y_shift, const s_tilemap* const tm, const bool ignore_shooters, const bool ignore_platforms) {
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state;

            if (!(*STATIC_ARRAY_ELEM(g_tile_states_solid, ts))) {
                continue;
            }

            if (ignore_shooters && ts == ek_tile_state_shooter) {
                continue;
            }

            const bool is_platform = *STATIC_ARRAY_ELEM(g_tile_states_platform, ts) != ek_tile_not_platform;

            if (ignore_platforms && is_platform) {
                continue;
            }

            const s_rect tr = GenTileCollider(tx, ty, ts);

            if (DoRectsInters(rect, tr)) {
                if (is_platform && rect.y + rect.height - y_shift > tr.y) {
                    continue;
                }

                return true;
            }
        }
    }

    return false;
}

static bool CheckDeathPlatformCollision(const s_rect rect, const float y_shift, const s_tilemap* const tm) {
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state;

            if (*STATIC_ARRAY_ELEM(g_tile_states_platform, ts) != ek_tile_death_platform) {
                continue;
            }

            const s_rect tr = GenTileCollider(tx, ty, ts);

            if (DoRectsInters(rect, tr)) {
                if (rect.y + rect.height - y_shift > tr.y) {
                    continue;
                }

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

            const s_rect tr = GenTileCollider(tx, ty, ts);

            /*if (*STATIC_ARRAY_ELEM(g_tile_states_platform, ts) && rect.y + rect.height > tr.y) {
                continue;
            }*/

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

static void MoveToSolidTile(s_v2* const pos, const e_cardinal_dir dir, const float dist_limit, const s_v2 collider_size, const s_v2 collider_origin, const s_tilemap* const tm) {
    const s_v2 pos_start = *pos;

    s_v2 jump;
    const float jump_dist = 1.0f / VIEW_SCALE;

    switch (dir) {
        case ek_cardinal_dir_right:
            jump = (s_v2){jump_dist, 0.0f};
            break;

        case ek_cardinal_dir_left:
            jump = (s_v2){-jump_dist, 0.0f};
            break;

        case ek_cardinal_dir_down:
            jump = (s_v2){0.0f, jump_dist};
            break;

        case ek_cardinal_dir_up:
            jump = (s_v2){0.0f, -jump_dist};
            break;
    }

    float dist = 0.0f;

    while ((dist_limit < 0.0f || dist + jump_dist <= dist_limit) && !CheckSolidTileCollision(GenCollider((s_v2){pos->x + jump.x, pos->y + jump.y}, collider_size, collider_origin), pos->y + jump_dist - pos_start.y, tm, false, false)) {
        pos->x += jump.x;
        pos->y += jump.y;
        dist += jump_dist;
    }

#if 0
    if (passed_death_platform && !*passed_death_platform) {
        if (CheckDeathPlatformCollision(GenCollider(*pos, collider_size, collider_origin), pos->y - pos_start.y, tm)) {
            assert(false);
            *passed_death_platform = true;
        }
    }
#endif

#if 0
    pos->x = roundf(pos->x);
    pos->y = roundf(pos->y);
#endif
}

static void ProcSolidTileCollisions(s_v2* const pos, s_v2* const vel, const s_v2 collider_size, const s_v2 collider_origin, const s_tilemap* const tm, const bool ignore_platforms) {
    const s_rect rect_hor = GenCollider((s_v2){pos->x + vel->x, pos->y}, collider_size, collider_origin);

    if (CheckSolidTileCollision(rect_hor, 0.0f, tm, false, ignore_platforms)) {
        MoveToSolidTile(pos, vel->x >= 0.0f ? ek_cardinal_dir_right : ek_cardinal_dir_left, ABS(vel->x), collider_size, collider_origin, tm);
        vel->x = 0.0f;
    }

    const s_rect rect_vert = GenCollider((s_v2){pos->x, pos->y + vel->y}, collider_size, collider_origin);

    if (CheckSolidTileCollision(rect_vert, vel->y, tm, false, ignore_platforms)) {
        MoveToSolidTile(pos, vel->y >= 0.0f ? ek_cardinal_dir_down : ek_cardinal_dir_up, ABS(vel->y), collider_size, collider_origin, tm);
        vel->y = 0.0f;
    }

    const s_rect rect_diag = GenCollider((s_v2){pos->x + vel->x, pos->y + vel->y}, collider_size, collider_origin);

    if (CheckSolidTileCollision(rect_diag, vel->y, tm, false, ignore_platforms)) {
        vel->x = 0.0f;
    }
}

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

static s_v2_s32 SnakeSize() {
    return RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_snake_enemy)->src_rect);
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

static s_particle* SpawnParticle(s_level* const lvl, const s_v2 pos, const s_v2 vel, const float rot) {
    int pt_index = -1;

    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->particles); i++) {
        const s_particle* const pt = STATIC_ARRAY_ELEM(lvl->particles, i);

        if (!pt->active) {
            pt_index = i;
            break;
        }
    }

    if (pt_index == -1) {
        LOG_WARNING("out of particle space");
        return NULL;
    }

    s_particle* const pt = STATIC_ARRAY_ELEM(lvl->particles, pt_index);

    *pt = (s_particle){
        .pos = pos,
        .vel = vel,
        .rot = rot,
        .blend = WHITE,
        .scale = V2_ONE,
        .active = true
    };

    return pt;
}

static void SpawnItemDrop(s_level* const lvl, const s_v2 pos, const s_v2 vel, const e_item_type type) {
    int drop_index = -1;

    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->item_drops); i++) {
        const s_item_drop* const drop = STATIC_ARRAY_ELEM(lvl->item_drops, i);

        if (!drop->active) {
            drop_index = i;
            break;
        }
    }

    if (drop_index == -1) {
        LOG_WARNING("out of item drop space");
        return;
    }

    s_item_drop* const drop = STATIC_ARRAY_ELEM(lvl->item_drops, drop_index);

    *drop = (s_item_drop){
        .pos = pos,
        .vel = vel,
        .type = type,
        .active = true
    };
}

static s_v2 ItemDropSize(const s_v2 pos, const e_item_type type) {
    const e_sprite spr = *STATIC_ARRAY_ELEM(g_item_type_sprs, type);
    const s_v2_s32 size = RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, spr)->src_rect);
    return (s_v2){size.x, size.y};
}

static s_rect GenItemDropRect(const s_v2 pos, const e_item_type type) {
    const s_v2 size = ItemDropSize(pos, type);
    return (s_rect){
        pos.x - (size.x * ITEM_DROP_ORIGIN.x),
        pos.y - (size.y * ITEM_DROP_ORIGIN.y),
        size.x,
        size.y
    };
}

static s_v2_s32 PlayerSize() {
    return RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_player)->src_rect);
}

static s_rect GenPlayerRect(const s_v2 player_pos) {
    const s_v2_s32 size = RectS32Size(STATIC_ARRAY_ELEM(g_sprite_infos, ek_sprite_player)->src_rect);
    return (s_rect){player_pos.x - (size.x * PLAYER_ORIGIN.x), player_pos.y - (size.y * PLAYER_ORIGIN.y), size.x, size.y};
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

bool GenLevel(s_level* const lvl, const s_v2_s32 window_size, const s_game_run_state* const run_state, s_mem_arena* const temp_mem_arena) {
    assert(IS_ZERO(*lvl));

    s_tilemap* const tm = &lvl->tilemap;

    t_tilemap_room_types room_types = {0};
    GenTilemapRoomTypes(&room_types, &tm->starting_room_x);

    s_rgba_texture rooms_tex = {0};

    if (!LoadRGBATextureFromRawFile(&rooms_tex, temp_mem_arena, (s_char_array_view)ARRAY_FROM_STATIC("rooms.png"))) {
        return false;
    }

    assert(rooms_tex.tex_size.x % TILEMAP_ROOM_WIDTH == 0 && rooms_tex.tex_size.y % TILEMAP_ROOM_HEIGHT == 0);

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
            const int rooms_tex_tl_y = TILEMAP_ROOM_HEIGHT * RandRangeS32(0, rooms_tex.tex_size.y / TILEMAP_ROOM_HEIGHT);

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
                    } else if (tex_px_r == 255 && tex_px_g == 100 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_ladder_platform;
                    } else if (tex_px_r == 255 && tex_px_g == 255 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_gold;
                    } else if (tex_px_r == 0 && tex_px_g == 255 && tex_px_b == 0 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_shooter;
                    } else if (tex_px_r == 0 && tex_px_g == 255 && tex_px_b == 150 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_shooter;
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->shooter_facing_right = true;
                    } else if (tex_px_r == 0 && tex_px_g == 0 && tex_px_b == 255 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_entrance;
                        lvl->player.pos = tile_center_in_lvl;
                    } else if (tex_px_r == 255 && tex_px_g == 0 && tex_px_b == 255 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_exit;
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->door_frame_time = DOOR_FRAME_CNT * DOOR_FRAME_INTERVAL;
                    } else if (tex_px_r == 255 && tex_px_g == 255 && tex_px_b == 255 && tex_px_a == 255) {
                        STATIC_ARRAY_2D_ELEM(tm->tiles, ty, tx)->state = ek_tile_state_spike;
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

#if 0
    // Add random rock items.
    for (int ty = 1; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);
            const s_tile* const t_above = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty - 1, tx);

            if (t->state == ek_tile_state_dirt && t_above->state == ek_tile_state_empty) {
                if (RandPerc() < 0.05f) {
                    const s_v2 drop_pos = {(tx + 0.5f) * TILE_SIZE, (ty - 0.5f) * TILE_SIZE};
                    SpawnItemDrop(lvl, drop_pos, (s_v2){0}, ek_item_type_rock);
                }
            }
        }
    }
#endif

    //
    lvl->hp = HP_LIMIT;

    MoveToSolidTile(&lvl->player.pos, ek_cardinal_dir_down, -1.0f, (s_v2){PlayerSize().x, PlayerSize().y}, PLAYER_ORIGIN, &lvl->tilemap);

    lvl->view_pos_no_shake = lvl->player.pos;

    // Move enemies down.
    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->enemies); i++) {
        s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, i);

        if (!enemy->active) {
            continue;
        }

        if (enemy->type == ek_enemy_type_snake) {
            MoveToSolidTile(&enemy->pos, ek_cardinal_dir_down, -1.0f, (s_v2){SnakeSize().x, SnakeSize().y}, SNAKE_ORIGIN, &lvl->tilemap);
        }
    }

    //
    lvl->gold_display_cnt = run_state->gold_cnt;

    return true;
}

static void SpawnArrow(s_level* const lvl, const s_v2 pos, const bool right) {
    if (lvl->arrow_cnt < STATIC_ARRAY_LEN(lvl->arrows)) {
        s_arrow* const arrow = STATIC_ARRAY_ELEM(lvl->arrows, lvl->arrow_cnt);
        arrow->pos = pos;
        arrow->right = right;
        lvl->arrow_cnt++;
    } else {
        assert(false);
    }
}

static inline bool DoesPlayerExist(const s_level* const lvl) {
    return lvl->started && lvl->post_start_wait_time == POST_START_WAIT_TIME_MAX && !lvl->leaving && lvl->hp > 0;
}

static void TriggerHorShooters(s_level* const lvl, const int tx_center, const int ty) {
    const float shake_amount = 0.5f;

    // Check to right.
    for (int tx = tx_center + 1; ; tx++) {
        s_tile* const tile = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

        if (tile->state == ek_tile_state_shooter && !tile->shooter_shot && !tile->shooter_facing_right) {
            SpawnArrow(lvl, (s_v2){(tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE}, false);
            Shake(lvl, shake_amount);

            tile->shooter_shot = true;
            break;
        }

        if (*STATIC_ARRAY_ELEM(g_tile_states_solid, tile->state)) {
            break;
        }
    }

    // Check to left.
    for (int tx = tx_center - 1; ; tx--) {
        s_tile* const tile = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

        if (tile->state == ek_tile_state_shooter && !tile->shooter_shot && tile->shooter_facing_right) {
            SpawnArrow(lvl, (s_v2){(tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE}, true);
            Shake(lvl, shake_amount);
            tile->shooter_shot = true;
            break;
        }

        if (*STATIC_ARRAY_ELEM(g_tile_states_solid, tile->state)) {
            break;
        }
    }
}

static void SpawnBloodParticles(s_level* const lvl, const s_v2 pos, const bool big) {
    const int pt_cnt = big ? RandRangeS32Incl(5, 7) : RandRangeS32Incl(4, 5);

    const float base_dir_offs = RandPerc() * TAU;

    for (int i = 0; i < pt_cnt; i++) {
        const float dir = (((float)i / pt_cnt) * TAU) + RandRange(-PI * 0.2f, PI * 0.2f) + base_dir_offs;
        const s_v2 vel = LenDir(RandRange(2.0f, 4.0f), dir);

        s_particle* const pt = SpawnParticle(lvl, pos, vel, TAU * RandPerc());

        if (!pt) {
            return;
        }

        pt->blend = RED;

        const float scale = RandRange(1.0f, 1.5f);
        pt->scale = (s_v2){scale, scale};
    }
}

static void SpawnGoldParticles(s_level* const lvl, const s_v2 pos) {
    const int pt_cnt = RandRangeS32Incl(3, 4);

    const float base_dir_offs = RandPerc() * TAU;

    for (int i = 0; i < pt_cnt; i++) {
        const float dir = (((float)i / pt_cnt) * TAU) + RandRange(-PI * 0.3f, PI * 0.3f) + base_dir_offs;
        const s_v2 vel = LenDir(RandRange(1.5f, 3.0f), dir);

        s_particle* const pt = SpawnParticle(lvl, pos, vel, TAU * RandPerc());

        if (!pt) {
            return;
        }

        pt->blend = YELLOW;

        const float scale = RandRange(1.0f, 1.5f);
        pt->scale = (s_v2){scale, scale};
    }
}

e_level_update_end_result UpdateLevel(s_level* const lvl, s_game_run_state* const run_state, const s_game_tick_context* const zfw_context) {
    e_level_update_end_result end_res = ek_level_update_end_result_normal;

    lvl->interact_popup_type = ek_interact_popup_type_none;

    lvl->item_drop_hovered_index = -1;

    const int player_hp_old = lvl->hp;

    if (lvl->started) {
        if (lvl->post_start_wait_time < POST_START_WAIT_TIME_MAX) {
            lvl->post_start_wait_time++;

            if (lvl->post_start_wait_time >= POST_START_WAIT_TIME_MAX / 2) {
                // LOL THIS IS SO BAD
                for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
                    for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
                        s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

                        if (t->state != ek_tile_state_entrance) {
                            continue;
                        }

                        if (t->door_frame_time < DOOR_FRAME_INTERVAL * DOOR_FRAME_CNT) {
                            t->door_frame_time++;
                        }

                        break; // only one i guess
                    }
                }
            }
        } else {
            for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
                for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
                    s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

                    if (t->state != ek_tile_state_entrance) {
                        continue;
                    }

                    if (t->door_frame_time > 0) {
                        t->door_frame_time--;
                    }

                    break; // only one i guess
                }
            }

            lvl->general_ui_alpha = Lerp(lvl->general_ui_alpha, 1.0f, GENERAL_UI_FADE_IN_LERP);
        }
    }

    if (lvl->leaving) {
        if (lvl->leaving_wait_time < LEAVING_WAIT_TIME_MAX) {
            lvl->leaving_wait_time++;

            for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
                for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
                    s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

                    if (t->state != ek_tile_state_exit) {
                        continue;
                    }

                    if (t->door_frame_time > 0) {
                        t->door_frame_time--;
                    }

                    break; // only one i guess
                }
            }
        } else {
            end_res = ek_level_update_end_result_next;
        }
    }

    //
    lvl->hitbox_cnt = 0;

    if (DoesPlayerExist(lvl)) {
        //
        // Player Movement
        //
        int move_axis = IsKeyDown(&zfw_context->input_context, ek_key_code_right) - IsKeyDown(&zfw_context->input_context, ek_key_code_left);

        {
            s_player* const player = &lvl->player;

            const bool holding_down = IsKeyDown(&zfw_context->input_context, ek_key_code_down);

            if (player->latching) {
                move_axis = 0;
            } 

            const float vel_x_dest = PLAYER_MOVE_SPD * move_axis;
            player->vel.x = Lerp(player->vel.x, vel_x_dest, PLAYER_MOVE_SPD_LERP);

            s_v2_s32 possible_latch_targ = {-1, -1};

            if (move_axis != 0) {
                const s_rect rect = GenPlayerRect(player->pos);

                const int tx = (player->pos.x / TILE_SIZE) + move_axis;

                int ty_min = rect.y / TILE_SIZE;
                int ty_max = ceilf((rect.y + rect.height) / TILE_SIZE);

                for (int ty = ty_min; ty < ty_max; ty++) {
                    if (tx < 0 || tx >= TILEMAP_WIDTH || ty < 0 || ty >= TILEMAP_HEIGHT) {
                        continue;
                    }

                    const s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

                    const s_rect tile_rect = GenTileCollider(tx, ty, t->state);

                    if (move_axis == 1) {
                        if (rect.x + rect.width + 1.0f < tile_rect.x) {
                            break;
                        }
                    } else {
                        assert(move_axis == -1); // SAHDKJLASH

                        if (rect.x - 1.0f >= tile_rect.x + tile_rect.width) {
                            break;
                        }
                    }

                    if (tile_rect.y <= rect.y && *STATIC_ARRAY_ELEM(g_tile_states_solid, t->state) && *STATIC_ARRAY_ELEM(g_tile_states_platform, t->state) == ek_tile_not_platform) {
                        break;
                    }

                    if (tile_rect.y > rect.y && *STATIC_ARRAY_ELEM(g_tile_states_solid, t->state) && *STATIC_ARRAY_ELEM(g_tile_states_platform, t->state) == ek_tile_not_platform) {
                        possible_latch_targ = (s_v2_s32){tx, ty};
                        break;
                    }
                }
            }

            const bool on_ground = CheckSolidTileCollision(RectTranslated(GenPlayerRect(lvl->player.pos), (s_v2){0.0f, 1.0f}), 1.0f, &lvl->tilemap, false, false);
            const bool touching_ladder = CheckTileCollisionWithState(NULL, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_ladder) || CheckTileCollisionWithState(NULL, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_ladder_platform);

            if (on_ground) {
                lvl->player.cant_climb = false;
            }

            if (touching_ladder) {
                if (!lvl->player.cant_climb && (holding_down || IsKeyDown(&zfw_context->input_context, ek_key_code_up))) {
                    lvl->player.climbing = true;
                }
            } else {
                if (lvl->player.climbing) {
                    lvl->player.climbing = false;
                    lvl->player.cant_climb = true;
                }
            }

            if (lvl->player.climbing) {
                lvl->player.vel.y = 0.0f;

                if (IsKeyDown(&zfw_context->input_context, ek_key_code_up)) {
                    lvl->player.vel.y = -PLAYER_CLIMB_SPD;
                } else if (holding_down) {
                    lvl->player.vel.y = PLAYER_CLIMB_SPD;
                }
            } else {
                if (!lvl->player.latching) {
                    lvl->player.vel.y += GRAVITY;
                }
            }

            if (!lvl->player.climbing && (on_ground || lvl->player.latching)) {
                if (IsKeyPressed(&zfw_context->input_context, ek_key_code_up)) {
                    lvl->player.vel.y = -PLAYER_JUMP_HEIGHT;
                    lvl->player.latching = false;
                }
            }

            ProcSolidTileCollisions(&player->pos, &player->vel, (s_v2){PlayerSize().x, PlayerSize().y}, PLAYER_ORIGIN, &lvl->tilemap, holding_down);

            lvl->player.pos = V2Sum(lvl->player.pos, lvl->player.vel);

            if (CheckDeathPlatformCollision(GenPlayerRect(player->pos), lvl->player.vel.y, &lvl->tilemap)) {
                lvl->hp = 0; // OUCH
            }

            //
            // Update latch state.
            //
            if (!lvl->player.climbing && possible_latch_targ.x != -1 && possible_latch_targ.y != -1) {
                const s_rect new_rect = GenPlayerRect(lvl->player.pos);
                const e_tile_state ts = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, possible_latch_targ.y, possible_latch_targ.x)->state;
                const s_rect possible_latch_targ_tile_rect = GenTileCollider(possible_latch_targ.x, possible_latch_targ.y, ts);

                if (new_rect.y >= possible_latch_targ_tile_rect.y) {
                    lvl->player.pos.x = roundf(lvl->player.pos.x);
                    lvl->player.latching = true;
                    lvl->player.pos.y -= new_rect.y - possible_latch_targ_tile_rect.y;
                    lvl->player.vel = (s_v2){0};
                }
            }

            s_v2_s32 gold_tile_pos = {0};

            if (CheckTileCollisionWithState(&gold_tile_pos, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_gold)) {
                SpawnGoldParticles(lvl, (s_v2){(gold_tile_pos.x + 0.5f) * TILE_SIZE, (gold_tile_pos.y + 0.75f) * TILE_SIZE});
                Shake(lvl, 0.5f);
                STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, gold_tile_pos.y, gold_tile_pos.x)->state = ek_tile_state_empty;
                run_state->gold_cnt += GOLD_INCR;
            }

            //
            // Whipping or Item Use
            //
            if (lvl->player.whip_break_time > 0) {
                lvl->player.whip_break_time--;
            }

            if (lvl->player.whip_break_time == 0) {
                if (move_axis == 1) {
                    player->facing_right = true;
                } else if (move_axis == -1) {
                    player->facing_right = false;
                }
            }

            if (IsKeyPressed(&zfw_context->input_context, ek_key_code_x)) {
                if (lvl->player.holding_item) {
                    // Throw!
                    const s_v2 throw_vel = {PLAYER_THROW_SPD.x * (lvl->player.facing_right ? 1.0f : -1.0f), -PLAYER_THROW_SPD.y};
                    SpawnItemDrop(lvl, lvl->player.pos, throw_vel, lvl->player.item_type_held);
                    lvl->player.holding_item = false;

                    Shake(lvl, 0.25f);
                } else {
                    if (lvl->player.whip_break_time == 0) {
                        lvl->player.whip_break_time = PLAYER_WHIP_BREAK_TIME;

                        const s_v2 hb_pos = {lvl->player.pos.x + (lvl->player.facing_right ? PLAYER_WHIP_OFFS : -PLAYER_WHIP_OFFS), lvl->player.pos.y - 1.0f};
                        SpawnHitbox(lvl, hb_pos, (s_v2){PlayerSize().x * 2.0f, PlayerSize().y * 1.25}, (s_v2){0.5f, 0.5f}, 1, false);

                        Shake(lvl, 0.25f);
                    }
                }
            }
        }

        //
        // Shooter Tiles
        //
        TriggerHorShooters(lvl, lvl->player.pos.x / TILE_SIZE, lvl->player.pos.y / TILE_SIZE);

        //
        // End Level
        //
        if (CheckTileCollisionWithState(NULL, GenPlayerRect(lvl->player.pos), &lvl->tilemap, ek_tile_state_exit)) {
            lvl->interact_popup_type = ek_interact_popup_type_enter;

            if (IsKeyPressed(&zfw_context->input_context, ek_key_code_z)) {
                lvl->leaving = true;
            }
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
                    const int tx = enemy->pos.x / TILE_SIZE;
                    const int ty = enemy->pos.y / TILE_SIZE;

                    const s_tile* const tile_ahead = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx + enemy->snake_type.move_axis);
                    const s_tile* const tile_ahead_below = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty + 1, tx + enemy->snake_type.move_axis);

                    if (*STATIC_ARRAY_ELEM(g_tile_states_solid, tile_ahead->state) || !*STATIC_ARRAY_ELEM(g_tile_states_solid, tile_ahead_below->state)) {
                        bool can_switch = true;

                        if (enemy->snake_type.move_axis == 1) {
                            if ((int)enemy->pos.x % TILE_SIZE < TILE_SIZE / 4) {
                                can_switch = false;
                            }
                        } else {
                            if ((int)enemy->pos.x % TILE_SIZE >= (TILE_SIZE / 4) * 3) {
                                can_switch = false;
                            }
                        }

                        if (can_switch) {
                            enemy->snake_type.move_axis *= -1;
                        }
                    }

                    const float vel_x_dest = SNAKE_MOVE_SPD * enemy->snake_type.move_axis;
                    enemy->snake_type.vel.x = Lerp(enemy->snake_type.vel.x, vel_x_dest, SNAKE_MOVE_SPD_LERP);

                    enemy->snake_type.vel.y += GRAVITY;

                    ProcSolidTileCollisions(&enemy->pos, &enemy->snake_type.vel, (s_v2){SnakeSize().x, SnakeSize().y}, SNAKE_ORIGIN, &lvl->tilemap, false);

                    enemy->pos = V2Sum(enemy->pos, enemy->snake_type.vel);

                    SpawnHitbox(lvl, enemy->pos, (s_v2){SnakeSize().x, SnakeSize().y}, SNAKE_ORIGIN, 1, true);
                }

                break;
        }
    }

    //
    // Item Drops
    //
    {
        for (int i = 0; i < STATIC_ARRAY_LEN(lvl->item_drops); i++) {
            s_item_drop* const drop = STATIC_ARRAY_ELEM(lvl->item_drops, i);

            if (!drop->active) {
                continue;
            }

            {
                const s_rect collider = GenItemDropRect(drop->pos, drop->type);

                if (CheckSolidTileCollision((s_rect){collider.x, collider.y + 1.0f, collider.width, collider.height}, 1.0f, &lvl->tilemap, false, false)) {
                    drop->vel.x = Lerp(drop->vel.x, 0.0f, 0.15f);
                }
            }

            drop->vel.y += GRAVITY;

            ProcSolidTileCollisions(&drop->pos, &drop->vel, ItemDropSize(drop->pos, drop->type), ITEM_DROP_ORIGIN, &lvl->tilemap, false);

            drop->pos = V2Sum(drop->pos, drop->vel);

            if (Mag(drop->vel) >= ITEM_DROP_PICKUP_MAG) {
                SpawnHitbox(lvl, drop->pos, ItemDropSize(drop->pos, drop->type), ITEM_DROP_ORIGIN, 1, false);
                TriggerHorShooters(lvl, drop->pos.x / TILE_SIZE, drop->pos.y / TILE_SIZE);
            }
        }

        // Item drop collection
        if (!lvl->player.holding_item) {
            const s_rect player_collider = GenPlayerRect(lvl->player.pos);

            for (int i = 0; i < STATIC_ARRAY_LEN(lvl->item_drops); i++) {
                s_item_drop* const drop = STATIC_ARRAY_ELEM(lvl->item_drops, i);

                if (!drop->active || Mag(drop->vel) >= ITEM_DROP_PICKUP_MAG) {
                    continue;
                }

                // Collection by player
                const s_rect drop_collider = GenItemDropRect(drop->pos, drop->type);

                if (DoRectsInters(player_collider, drop_collider)) {
                    lvl->item_drop_hovered_index = i;
                    lvl->interact_popup_type = ek_interact_popup_type_equip;

                    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_z)) {
                        lvl->player.holding_item = true;
                        lvl->player.item_type_held = drop->type;
                        drop->active = false;
                    }

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

            s_rect colliders[2];

            *STATIC_ARRAY_ELEM(colliders, 0) = GenArrowRect(arrow->pos);

            arrow->pos.x += ARROW_SPD * (arrow->right ? 1.0f : -1.0f);

            *STATIC_ARRAY_ELEM(colliders, 1) = GenArrowRect(arrow->pos);

            const s_rect collider_for_checking = GenSpanningRect((s_rect_array_view)ARRAY_FROM_STATIC(colliders));

            if (CheckSolidTileCollision(collider_for_checking, 0.0f, &lvl->tilemap, true, true)) {
                *arrow = *STATIC_ARRAY_ELEM(lvl->arrows, lvl->arrow_cnt - 1);
                lvl->arrow_cnt--;
                i--;
            }

            SpawnHitbox(lvl, RectPos(collider_for_checking), RectSize(collider_for_checking), (s_v2){0}, 1, true);
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
                    s_enemy* const enemy = STATIC_ARRAY_ELEM(lvl->enemies, j);

                    if (!enemy->active) {
                        continue;
                    }

                    const s_rect enemy_collider = *STATIC_ARRAY_ELEM(enemy_colliders, j);

                    if (DoRectsInters(hb->rect, enemy_collider)) {
                        Shake(lvl, 2.0f);
                        SpawnBloodParticles(lvl, enemy->pos, false);
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
        if (player_hp_old > 0) {
            SpawnBloodParticles(lvl, lvl->player.pos, true);
            Shake(lvl, 3.0f);
        }

        if (IsKeyPressed(&zfw_context->input_context, ek_key_code_x)) {
            end_res = ek_level_update_end_result_restart;
        }

        lvl->death_alpha = Lerp(lvl->death_alpha, 1.0f, DEATH_FADE_IN_LERP);
    } 

    //
    // Particles
    //
    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->particles); i++) {
        s_particle* const pt = STATIC_ARRAY_ELEM(lvl->particles, i);

        if (!pt->active) {
            continue;
        }

        pt->vel = V2Scaled(pt->vel, 0.8f);

        pt->pos = V2Sum(pt->pos, pt->vel);

        if (Mag(pt->vel) < 0.01f) {
            pt->blend.a *= 0.7f;

            if (pt->blend.a < 0.01f) {
                pt->active = false;
            }
        }
    }

    //
    // View
    //
    const s_v2 view_dest = lvl->player.pos;

    const float view_lerp_factor = 0.3f;
    lvl->view_pos_no_shake.x = Lerp(lvl->view_pos_no_shake.x, view_dest.x, view_lerp_factor);
    lvl->view_pos_no_shake.y = Lerp(lvl->view_pos_no_shake.y, view_dest.y, view_lerp_factor);

    lvl->view_shake *= 0.8f;

    //
    // money yueah wokokrjhakj
    //
    assert(lvl->gold_display_cnt <= run_state->gold_cnt);

    if (lvl->gold_display_cnt < run_state->gold_cnt) {
        lvl->gold_display_cnt += MIN(GOLD_CNT_DISPLAY_ACCUM, run_state->gold_cnt - lvl->gold_display_cnt);
    }

    //
    // Interactp opopoipoio;
    //hsdkljashjkldhakshdlk
    if (lvl->interact_popup_type != ek_interact_popup_type_none) {
        lvl->interact_popup_type_cache = lvl->interact_popup_type;
        lvl->interact_popup_alpha = Lerp(lvl->interact_popup_alpha, 1.0f, 0.5f);
    } else {
        lvl->interact_popup_alpha = Lerp(lvl->interact_popup_alpha, 0.0f, 0.5f);
    }

    return end_res;
}

void RenderLevel(const s_level* const lvl, const s_rendering_context* const rc, const s_texture_group* const textures) {
    const s_v2_s32 view_size = {rc->window_size.x / VIEW_SCALE, rc->window_size.y / VIEW_SCALE};

    s_matrix_4x4 lvl_view_mat = IdentityMatrix4x4();

    s_v2 view_pos;

    if (lvl->view_shake > 0.0f) {
        view_pos = V2Sum(lvl->view_pos_no_shake, (s_v2){RandRange(-lvl->view_shake, lvl->view_shake), RandRange(-lvl->view_shake, lvl->view_shake)});
    } else {
        view_pos = lvl->view_pos_no_shake;
    }

    TranslateMatrix4x4(&lvl_view_mat, (s_v2){(-view_pos.x + (view_size.x / 2.0f)) * VIEW_SCALE, (-view_pos.y + view_size.y * 0.5f) * VIEW_SCALE});
    ScaleMatrix4x4(&lvl_view_mat, VIEW_SCALE);
    SetViewMatrix(rc, &lvl_view_mat);

    //
    // Background
    //
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const s_v2 pos = {tx * TILE_SIZE, ty * TILE_SIZE};
            RenderSprite(rc, textures, ek_sprite_bg, V2Sum(pos, (s_v2){(lvl->view_pos_no_shake.x * PARALLAX) - (view_size.x * PARALLAX * 0.5f), (lvl->view_pos_no_shake.y * PARALLAX) - (view_size.y * PARALLAX * 0.5f)}), (s_v2){0}, (s_v2){1.0f, 1.0f}, 0.0f, GRAY);
        }
    }

    //
    // Tilemap (Behind)
    //
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

            if (t->state == ek_tile_state_empty || !g_tile_state_is_behind_ents[t->state]) {
                continue;
            }

            const s_v2 pos = {tx * TILE_SIZE, ty * TILE_SIZE};

            e_sprite spr;

            if (t->state == ek_tile_state_entrance || t->state == ek_tile_state_exit) {
                const int frame_index = MIN(t->door_frame_time / DOOR_FRAME_INTERVAL, DOOR_FRAME_CNT - 1);
                spr = ek_sprite_door_tile_0 + frame_index;
            } else {
                spr = *STATIC_ARRAY_ELEM(g_tile_state_sprs, t->state);
            }

            RenderSprite(rc, textures, spr, pos, (s_v2){0}, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
        }
    }

    //
    // Player
    //
    if (DoesPlayerExist(lvl)) {
        const s_rect rect = GenPlayerRect(lvl->player.pos);

        e_sprite spr = ek_sprite_player;

#if 0
        if (lvl->player.latching) {
            spr = lvl->player.facing_right ? ek_sprite_player_latch_onto_right : ek_sprite_player_latch_onto_left;
        }
#endif

        RenderSprite(rc, textures, spr, lvl->player.pos, PLAYER_ORIGIN, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);

        if (lvl->player.whip_break_time > 0) {
            const float dir_offs = (PI * 0.8f * ((float)lvl->player.whip_break_time / PLAYER_WHIP_BREAK_TIME));
            const float dir = lvl->player.facing_right ? dir_offs - (PI * 0.2f) : PI + (PI * 0.2f) - dir_offs;
            const s_v2 pos_offs = LenDir(2.0f, dir);
            RenderSprite(rc, textures, ek_sprite_whip, V2Sum(lvl->player.pos, pos_offs), (s_v2){0.0f, 0.5f}, (s_v2){1.0f, lvl->player.facing_right ? 1.0f : -1.0f}, dir, WHITE);
        }

        if (lvl->player.holding_item) {
            RenderSprite(rc, textures, *STATIC_ARRAY_ELEM(g_item_type_sprs, lvl->player.item_type_held), lvl->player.pos, ITEM_DROP_ORIGIN, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
        }
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
    // Item Drops
    //
    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->item_drops); i++) {
        const s_item_drop* const drop = STATIC_ARRAY_ELEM(lvl->item_drops, i);

        if (!drop->active) {
            continue;
        }

#if 0
        if (lvl->item_drop_hovered_index == i) {
            // should use shader for this but im outta time
            const s_rect drop_rect = GenItemDropRect(drop->pos, drop->type);
            RenderRect(rc, (s_rect){drop_rect.x - 1.0f, drop_rect.y - 1.0f, drop_rect.width + 2.0f, drop_rect.height + 2.0f}, WHITE);
        }
#endif

        RenderSprite(rc, textures, *STATIC_ARRAY_ELEM(g_item_type_sprs, drop->type), drop->pos, ITEM_DROP_ORIGIN, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
    }

    //
    // Arrows
    //
    for (int i = 0; i < lvl->arrow_cnt; i++) {
        const s_arrow* const arrow = STATIC_ARRAY_ELEM(lvl->arrows, i);
        RenderSprite(rc, textures, ek_sprite_arrow, arrow->pos, ARROW_ORIGIN, (s_v2){arrow->right ? -1.0f : 1.0f, 1.0f}, 0.0f, WHITE);
    }

    //
    // Tilemap (Front)
    //
    {
        const s_v2_s32 top_left = {
            floorf((view_pos.x - (view_size.x / 2.0f)) / TILE_SIZE),
            floorf((view_pos.y - (view_size.y / 2.0f)) / TILE_SIZE)
        };

        const s_v2_s32 bottom_right = {
            ceilf((view_pos.x + (view_size.x / 2.0f)) / TILE_SIZE),
            ceilf((view_pos.y + (view_size.y / 2.0f)) / TILE_SIZE)
        };

        LOG("%d", top_left.x);

        for (int ty = top_left.y; ty < bottom_right.y; ty++) {
            for (int tx = top_left.x; tx < bottom_right.x; tx++) {
                const s_v2 pos = {tx * TILE_SIZE, ty * TILE_SIZE};

                if (tx < 0 || ty < 0 || tx >= TILEMAP_WIDTH || ty >= TILEMAP_HEIGHT) {
                    RenderSprite(rc, textures, ek_sprite_dirt_tile, pos, (s_v2){0}, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
                    continue;
                }

                const s_tile* const t = STATIC_ARRAY_2D_ELEM(lvl->tilemap.tiles, ty, tx);

                if (t->state == ek_tile_state_empty || g_tile_state_is_behind_ents[t->state]) {
                    continue;
                }

                e_sprite spr;

                if (t->state == ek_tile_state_shooter) {
                    spr = t->shooter_facing_right ? ek_sprite_shooter_tile_right : ek_sprite_shooter_tile_left;
                } else {
                    spr = *STATIC_ARRAY_ELEM(g_tile_state_sprs, t->state);
                }

                RenderSprite(rc, textures, spr, pos, (s_v2){0}, (s_v2){1.0f, 1.0f}, 0.0f, WHITE);
            }
        }
    }

    //
    // Particles
    //
    for (int i = 0; i < STATIC_ARRAY_LEN(lvl->particles); i++) {
        const s_particle* const pt = STATIC_ARRAY_ELEM(lvl->particles, i);

        if (!pt->active) {
            continue;
        }

        RenderTexture(rc, &rc->basis->builtin_textures, ek_builtin_texture_pixel, (s_rect_s32){0}, pt->pos, (s_v2){0.5f, 0.5f}, pt->scale, pt->rot, pt->blend);
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

bool RenderLevelUI(const s_level* const lvl, const s_game_run_state* const run_state, const s_rendering_context* const rc, const s_font_group* const fonts, s_mem_arena* const temp_mem_arena) {
    //
    // Gold
    //
    if (DoesPlayerExist(lvl)) {
        const s_v2 bg_rect_size = {320.0f, 96.0f};
        const s_rect bg_rect = {
            0.0f,
            rc->window_size.y - bg_rect_size.y,
            bg_rect_size.x,
            bg_rect_size.y
        };

        const float bg_rect_outline_size = VIEW_SCALE;

        RenderRect(rc, (s_rect){bg_rect.x, bg_rect.y - bg_rect_outline_size, bg_rect.width + bg_rect_outline_size, bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->general_ui_alpha});
        RenderRect(rc, (s_rect){bg_rect.x + bg_rect.width, bg_rect.y - bg_rect_outline_size, bg_rect_outline_size, bg_rect_outline_size + bg_rect.height}, (u_v4){WHITE.rgb, lvl->general_ui_alpha});
        RenderRect(rc, bg_rect, (u_v4){BLACK.rgb, BG_ALPHA * lvl->general_ui_alpha});

        char gold_str[16];
        snprintf(gold_str, sizeof(gold_str), "$%d", lvl->gold_display_cnt);

        //const s_v2 gold_str_pos = {26.0f, rc->window_size.y - 12.0f};
        s_v2 gold_str_pos = RectCenter(bg_rect);
        gold_str_pos.y -= 6.0f;

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC(gold_str), fonts, ek_font_pixel_med, gold_str_pos, ALIGNMENT_CENTER, (u_v4){WHITE.rgb, lvl->general_ui_alpha}, temp_mem_arena)) {
            return false;
        }
    }

    //
    // Level
    //
    if (DoesPlayerExist(lvl)) {
        const s_v2 bg_rect_size = {280.0f, 96.0f};
        const s_rect bg_rect = {
            rc->window_size.x - bg_rect_size.x,
            rc->window_size.y - bg_rect_size.y,
            bg_rect_size.x,
            bg_rect_size.y
        };

        const float bg_rect_outline_size = VIEW_SCALE;

        RenderRect(rc, (s_rect){bg_rect.x - bg_rect_outline_size, bg_rect.y - bg_rect_outline_size, bg_rect.width + bg_rect_outline_size, bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->general_ui_alpha});
        RenderRect(rc, (s_rect){bg_rect.x - bg_rect_outline_size, bg_rect.y - bg_rect_outline_size, bg_rect_outline_size, bg_rect_outline_size + bg_rect.height}, (u_v4){WHITE.rgb, lvl->general_ui_alpha});
        RenderRect(rc, bg_rect, (u_v4){BLACK.rgb, lvl->general_ui_alpha * BG_ALPHA});

        char lvl_str[16];
        snprintf(lvl_str, sizeof(lvl_str), "LVL %d", run_state->lvl_num);

        //const s_v2 lvl_str_pos = {rc->window_size.x - 16.0f, rc->window_size.y - 8.0f};
        s_v2 lvl_str_pos = RectCenter(bg_rect);
        lvl_str_pos.y -= 6.0f;

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC(lvl_str), fonts, ek_font_pixel_med, lvl_str_pos, ALIGNMENT_CENTER, (u_v4){WHITE.rgb, lvl->general_ui_alpha}, temp_mem_arena)) {
            return false;
        }
    }

    //
    // Interact
    //
    if (lvl->interact_popup_alpha >= 0.001f) {
        const s_v2 bg_rect_size = {360.0f, 96.0f};
        const s_rect bg_rect = {
            (rc->window_size.x - bg_rect_size.x) / 2.0f,
            rc->window_size.y - bg_rect_size.y,
            bg_rect_size.x,
            bg_rect_size.y
        };

        const float bg_rect_outline_size = VIEW_SCALE;

        RenderRect(rc, (s_rect){bg_rect.x - bg_rect_outline_size, bg_rect.y - bg_rect_outline_size, bg_rect.width + (bg_rect_outline_size * 2.0f), bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->general_ui_alpha * lvl->interact_popup_alpha});
        RenderRect(rc, (s_rect){bg_rect.x - bg_rect_outline_size, bg_rect.y - bg_rect_outline_size, bg_rect_outline_size, bg_rect.height + bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->general_ui_alpha * lvl->interact_popup_alpha});
        RenderRect(rc, (s_rect){bg_rect.x + bg_rect.width, bg_rect.y - bg_rect_outline_size, bg_rect_outline_size, bg_rect.height + bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->general_ui_alpha * lvl->interact_popup_alpha});
        RenderRect(rc, bg_rect, (u_v4){BLACK.rgb, lvl->general_ui_alpha * BG_ALPHA * lvl->interact_popup_alpha});

        char interact_str[16] = {0};

        assert(lvl->interact_popup_type_cache != ek_interact_popup_type_none);

        switch (lvl->interact_popup_type_cache) {
            case ek_interact_popup_type_equip:
                snprintf(interact_str, sizeof(interact_str), "[Z] EQUIP");
                break;

            case ek_interact_popup_type_enter:
                snprintf(interact_str, sizeof(interact_str), "[Z] ENTER");
                break;
        }

        //const s_v2 interact_str_pos = {rc->window_size.x / 2.0f, rc->window_size.y - 12.0f};
        s_v2 interact_str_pos = RectCenter(bg_rect);
        interact_str_pos.y -= 6.0f;

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC(interact_str), fonts, ek_font_pixel_med, interact_str_pos, ALIGNMENT_CENTER, (u_v4){WHITE.rgb, lvl->general_ui_alpha * lvl->interact_popup_alpha}, temp_mem_arena)) {
            return false;
        }
    }

    //
    // Death
    //
    if (lvl->hp == 0) {
        const float bg_height = 200.0f;
        const s_rect bg_rect = {0.0f, (rc->window_size.y - bg_height) / 2.0f, rc->window_size.x, bg_height};
        const float bg_rect_outline_size = VIEW_SCALE;

        RenderRect(rc, (s_rect){bg_rect.x, bg_rect.y - bg_rect_outline_size, bg_rect.width, bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->death_alpha});
        RenderRect(rc, (s_rect){bg_rect.x, bg_rect.y + bg_rect.height, bg_rect.width, bg_rect_outline_size}, (u_v4){WHITE.rgb, lvl->death_alpha});
        RenderRect(rc, bg_rect, (u_v4){BLACK.rgb, lvl->death_alpha * BG_ALPHA});

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("YOU DIED"), fonts, ek_font_pixel_large, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) - 44.0f}, ALIGNMENT_CENTER, (u_v4){WHITE.rgb, lvl->death_alpha}, temp_mem_arena)) {
            return false;
        }

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("PRESS [X] TO RESTART"), fonts, ek_font_pixel_small, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) + 44.0f}, ALIGNMENT_CENTER, (u_v4){WHITE.rgb, lvl->death_alpha}, temp_mem_arena)) {
            return false;
        }
    }

    return true;
}
