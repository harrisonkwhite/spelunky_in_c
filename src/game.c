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

static bool WARN_UNUSED_RESULT GenTilemap(s_tilemap* const tm) {
    assert(IS_ZERO(*tm));

    t_tilemap_room_types room_types = {0};
    GenTilemapRoomTypes(&room_types);

    return true;
}

bool InitGame(const s_game_init_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (!GenTilemap(&game->lvl.tilemap)) {
        return false;
    }

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (true || IsKeyPressed(&zfw_context->input_context, ek_key_code_space)) {
        ZERO_OUT(game->lvl.tilemap);

        if (!GenTilemap(&game->lvl.tilemap)) {
            return false;
        }

        LOG("%f", RandPerc());
    }

    return ek_game_tick_result_normal;
}

bool RenderGame(const s_game_render_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;
    const s_rendering_context* const rc = &zfw_context->rendering_context;

    RenderRect(rc, (s_rect){0.0f, 0.0f, 32.0f, 32.0f}, RED);

    return true;
}

void CleanGame(void* const dev_mem) {
}
