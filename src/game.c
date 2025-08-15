#include "game.h"

bool InitGame(const s_game_init_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

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
