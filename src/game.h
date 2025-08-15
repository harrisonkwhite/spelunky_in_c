#ifndef GAME_H
#define GAME_H

#include <zfwc.h>

#define NO_WORLD_GEN_DEBUG 1
#define SHOW_DEBUG_HITBOXES 1

#define DEATH_MSG "YOU DIED!"

#define HP_LIMIT 3

#define TILEMAP_ROOM_WIDTH 12
#define TILEMAP_ROOM_HEIGHT 8
#define TILEMAP_ROOMS_HOR 4
#define TILEMAP_ROOMS_VERT 4
#define TILEMAP_WIDTH (TILEMAP_ROOM_WIDTH * TILEMAP_ROOMS_HOR)
#define TILEMAP_HEIGHT (TILEMAP_ROOM_HEIGHT * TILEMAP_ROOMS_VERT)

#define TILE_SIZE 16

#define CAMERA_SCALE 3.0f

typedef enum {
    ek_font_medodica_96,
    ek_font_medodica_128
} e_font;

static const s_char_array_view g_font_file_paths[] = {
    [ek_font_medodica_96] = ARRAY_FROM_STATIC("assets/fonts/medodica_96"),
    [ek_font_medodica_128] = ARRAY_FROM_STATIC("assets/fonts/medodica_128")
};

typedef enum {
    ek_tile_state_empty,
    ek_tile_state_dirt,
    ek_tile_state_ladder,
    ek_tile_state_gold,
    ek_tile_state_shooter,
    ek_tile_state_entrance,
    ek_tile_state_exit
} e_tile_state;

static bool g_tile_states_solid[] = {
    [ek_tile_state_empty] = false,
    [ek_tile_state_dirt] = true,
    [ek_tile_state_ladder] = false,
    [ek_tile_state_gold] = false,
    [ek_tile_state_shooter] = true,
    [ek_tile_state_entrance] = false,
    [ek_tile_state_exit] = false
};

typedef struct {
    e_tile_state state;

    bool shooter_shot;
    bool shooter_facing_right;
} s_tile;

typedef struct {
    int starting_room_x;
    s_tile tiles[TILEMAP_HEIGHT][TILEMAP_WIDTH];
} s_tilemap;

typedef struct {
    s_v2 pos;
    s_v2 vel;
    bool climbing;
    bool cant_climb;
} s_player;

typedef struct {
    s_v2 pos;
    bool right;
} s_arrow;

typedef enum {
    ek_enemy_type_snake
} e_enemy_type;

typedef struct {
    bool facing_right;
} s_snake_enemy_type_data;

typedef union {
    s_snake_enemy_type_data snake;
} u_enemy_type_data;

typedef struct {
    bool active; // screw bitsets amiright

    s_v2 pos;

    e_enemy_type type;
    u_enemy_type_data type_data;
} s_enemy;

typedef struct {
    s_rect rect;
    int dmg;
    bool enemy;
} s_hitbox;

typedef struct {
    s_tilemap tilemap;

    s_player player;

    s_enemy enemies[128];

    s_arrow arrows[16];
    int arrow_cnt;

    s_hitbox hitboxes[128];
    int hitbox_cnt;

    int hp;
    int gold_cnt;

    bool completed;
} s_level;

typedef struct {
    s_font_group fonts;
    s_level lvl;
} s_game;

bool InitGame(const s_game_init_context* const zfw_context);
e_game_tick_result GameTick(const s_game_tick_context* const zfw_context);
bool RenderGame(const s_game_render_context* const zfw_context);
void CleanGame(void* const dev_mem);

bool WARN_UNUSED_RESULT GenLevel(s_level* const lvl, s_mem_arena* const temp_mem_arena);
void UpdateLevel(s_level* const lvl, const s_game_tick_context* const zfw_context);
void RenderLevel(s_level* const lvl, const s_rendering_context* const rc);
bool RenderLevelUI(s_level* const lvl, const s_rendering_context* const rc, const s_font_group* const fonts, s_mem_arena* const temp_mem_arena);

#endif
