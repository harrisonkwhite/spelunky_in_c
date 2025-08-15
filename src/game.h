#ifndef GAME_H
#define GAME_H

#include <zfwc.h>

#define TILEMAP_ROOM_WIDTH 12
#define TILEMAP_ROOM_HEIGHT 8
#define TILEMAP_ROOMS_HOR 4
#define TILEMAP_ROOMS_VERT 4
#define TILEMAP_WIDTH (TILEMAP_ROOM_WIDTH * TILEMAP_ROOMS_HOR)
#define TILEMAP_HEIGHT (TILEMAP_ROOM_HEIGHT * TILEMAP_ROOMS_VERT)

#define TILE_SIZE 16

typedef enum {
    ek_tile_state_empty,
    ek_tile_state_dirt
} e_tile_state;

typedef struct {
    e_tile_state states[TILEMAP_HEIGHT][TILEMAP_WIDTH];
} s_tilemap;

typedef struct {
    s_tilemap tilemap;
} s_level;

typedef struct {
    s_level lvl;
} s_game;

bool InitGame(const s_game_init_context* const zfw_context);
e_game_tick_result GameTick(const s_game_tick_context* const zfw_context);
bool RenderGame(const s_game_render_context* const zfw_context);
void CleanGame(void* const dev_mem);

#endif
