#ifndef GAME_H
#define GAME_H

#include <zfwc.h>

bool InitGame(const s_game_init_context* const zfw_context);
e_game_tick_result GameTick(const s_game_tick_context* const zfw_context);
bool RenderGame(const s_game_render_context* const zfw_context);
void CleanGame(void* const dev_mem);

#endif
