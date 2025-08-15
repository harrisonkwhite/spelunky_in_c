#include "game.h"

typedef enum {
    ek_tilemap_room_type_extra,
    ek_tilemap_room_type_left_right_exits,
    ek_tilemap_room_type_left_right_top_exits,
    ek_tilemap_room_type_left_right_bottom_exits,
    ek_tilemap_room_type_left_right_bottom_top_exits
} e_tilemap_room_type;

typedef e_tilemap_room_type t_tilemap_room_types[TILEMAP_ROOMS_VERT][TILEMAP_ROOMS_HOR];

static void GenTilemapRoomTypes(t_tilemap_room_types* const room_types) {
    assert(IS_ZERO(*room_types));

    s_v2_s32 pen = {RandRangeS32(0, TILEMAP_ROOMS_HOR), 0};

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
    GenTilemapRoomTypes(&room_types);

    s_rgba_texture rooms_tex = {0};

    if (!LoadRGBATextureFromRawFile(&rooms_tex, temp_mem_arena, (s_char_array_view)ARRAY_FROM_STATIC("rooms.png"))) {
        return false;
    }

    for (int ry = 0; ry < TILEMAP_ROOMS_VERT; ry++) {
        for (int rx = 0; rx < TILEMAP_ROOMS_HOR; rx++) {
            const e_tilemap_room_type rt = *STATIC_ARRAY_2D_ELEM(room_types, ry, rx);

            const int rooms_tex_x = TILEMAP_ROOM_WIDTH * rt;
            const int rooms_tex_y = 0;

            for (int tyo = 0; tyo < TILEMAP_ROOM_HEIGHT; tyo++) {
                for (int txo = 0; txo < TILEMAP_ROOM_WIDTH; txo++) {
                    const int rooms_tex_px_index = ((rooms_tex_y + tyo) * rooms_tex.tex_size.x) + rooms_tex_x + txo;

                    if (*U8Elem(rooms_tex.px_data, (rooms_tex_px_index * 4) + 3) > 0) {
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

bool InitGame(const s_game_init_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (!GenTilemap(&game->lvl.tilemap, zfw_context->temp_mem_arena)) {
        return false;
    }

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_space)) {
        ZERO_OUT(game->lvl.tilemap);

        if (!GenTilemap(&game->lvl.tilemap, zfw_context->temp_mem_arena)) {
            return false;
        }
    }

    return ek_game_tick_result_normal;
}

bool RenderGame(const s_game_render_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    const s_rendering_context* const rc = &zfw_context->rendering_context;

    Clear(rc, PURPLE);

    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = *STATIC_ARRAY_2D_ELEM(game->lvl.tilemap.states, ty, tx) = ek_tile_state_dirt;

            if (ts == ek_tile_state_empty) {
                continue;
            }

            const s_rect rect = {tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            RenderRectWithOutlineAndOpaqueFill(rc, rect, WHITE.rgb, BLACK, 1.0f);
        }
    }

    return true;
}

void CleanGame(void* const dev_mem) {
}
