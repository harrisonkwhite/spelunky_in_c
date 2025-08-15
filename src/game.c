#include "game.h"

bool InitGame(const s_game_init_context* const zfw_context) {
    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    return ek_game_tick_result_normal;
}

bool RenderGame(const s_game_render_context* const zfw_context) {
    return true;
}

void CleanGame(void* const dev_mem) {
}
