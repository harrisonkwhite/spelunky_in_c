#include "game.h"

bool InitGame(const s_game_init_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (!GenLevel(&game->lvl, zfw_context->temp_mem_arena)) {
        return false;
    }

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    UpdateLevel(&game->lvl, zfw_context);

    return ek_game_tick_result_normal;
}

bool RenderGame(const s_game_render_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    const s_rendering_context* const rc = &zfw_context->rendering_context;

    Clear(rc, PURPLE);
    RenderLevel(&game->lvl, &zfw_context->rendering_context);

    return true;
}

void CleanGame(void* const dev_mem) {
}
