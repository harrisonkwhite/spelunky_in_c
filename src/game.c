#include "game.h"

typedef enum {
    ek_tilemap_room_type_extra,
    ek_tilemap_room_type_left_right_exits,
    ek_tilemap_room_type_left_right_top_exits,
    ek_tilemap_room_type_left_right_bottom_exits,
    ek_tilemap_room_type_left_right_bottom_top_exits
} e_tilemap_room_type;

static bool WARN_UNUSED_RESULT GenTilemap(s_tilemap* const tm) {
    assert(IS_ZERO(*tm));

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

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_space)) {
        ZERO_OUT(game->lvl.tilemap);

        if (!GenTilemap(&game->lvl.tilemap)) {
            return false;
        }
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
