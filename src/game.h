#ifndef GAME_H
#define GAME_H

#include <zfwc.h>

#define NO_WORLD_GEN_DEBUG 1
#define SHOW_DEBUG_HITBOXES 0

#define HP_LIMIT 3

#define TILEMAP_ROOM_WIDTH 12
#define TILEMAP_ROOM_HEIGHT 8
#define TILEMAP_ROOMS_HOR 4
#define TILEMAP_ROOMS_VERT 4
#define TILEMAP_WIDTH (TILEMAP_ROOM_WIDTH * TILEMAP_ROOMS_HOR)
#define TILEMAP_HEIGHT (TILEMAP_ROOM_HEIGHT * TILEMAP_ROOMS_VERT)

#define TILE_SIZE 16

#define GOLD_INCR 250

extern float g_view_scale;

typedef enum {
    ek_texture_level,
    eks_texture_cnt
} e_texture;

static s_rgba_texture GenTextureRGBA(const t_s32 tex_index, s_mem_arena* const mem_arena) {
    switch ((e_texture)tex_index) {
        case ek_texture_level:
            return LoadRGBATextureFromPackedFile((s_char_array_view)ARRAY_FROM_STATIC("assets/textures/level"), mem_arena);
    }
}

typedef enum {
    ek_sprite_dirt_tile,
    ek_sprite_ladder_tile,
    ek_sprite_gold_tile,
    ek_sprite_shooter_tile,
    ek_sprite_entrance_tile,
    ek_sprite_exit_tile,
    ek_sprite_player,
    ek_sprite_snake_enemy
} e_sprite;

typedef struct {
    e_texture tex;
    s_rect_s32 src_rect;
} s_sprite_info;

static s_sprite_info g_sprite_infos[] = {
    [ek_sprite_dirt_tile] = {
        .tex = ek_texture_level,
        .src_rect = {0, 0, 16, 16}
    },
    [ek_sprite_ladder_tile] = {
        .tex = ek_texture_level,
        .src_rect = {16, 0, 16, 16}
    },
    [ek_sprite_gold_tile] = {
        .tex = ek_texture_level,
        .src_rect = {32, 0, 16, 16}
    },
    [ek_sprite_shooter_tile] = {
        .tex = ek_texture_level,
        .src_rect = {48, 0, 16, 16}
    },
    [ek_sprite_entrance_tile] = {
        .tex = ek_texture_level,
        .src_rect = {0, 16, 16, 16}
    },
    [ek_sprite_exit_tile] = {
        .tex = ek_texture_level,
        .src_rect = {16, 16, 16, 16}
    },
    [ek_sprite_player] = {
        .tex = ek_texture_level,
        .src_rect = {1, 33, 14, 14}
    },
    [ek_sprite_snake_enemy] = {
        .tex = ek_texture_level,
        .src_rect = {17, 33, 14, 14}
    }
};

typedef enum {
    ek_font_roboto_32,
    ek_font_roboto_48,
    ek_font_roboto_64,
    ek_font_roboto_96
} e_font;

static const s_char_array_view g_font_file_paths[] = {
    [ek_font_roboto_32] = ARRAY_FROM_STATIC("assets/fonts/roboto_32"),
    [ek_font_roboto_48] = ARRAY_FROM_STATIC("assets/fonts/roboto_48"),
    [ek_font_roboto_64] = ARRAY_FROM_STATIC("assets/fonts/roboto_64"),
    [ek_font_roboto_96] = ARRAY_FROM_STATIC("assets/fonts/roboto_96")
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

static e_sprite g_tile_state_sprs[] = {
    [ek_tile_state_dirt] = ek_sprite_dirt_tile,
    [ek_tile_state_ladder] = ek_sprite_ladder_tile,
    [ek_tile_state_gold] = ek_sprite_gold_tile,
    [ek_tile_state_shooter] = ek_sprite_shooter_tile,
    [ek_tile_state_entrance] = ek_sprite_entrance_tile,
    [ek_tile_state_exit] = ek_sprite_exit_tile
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
    bool facing_right;
    bool latching;
} s_player;

typedef struct {
    s_v2 pos;
    bool right;
} s_arrow;

typedef enum {
    ek_enemy_type_snake
} e_enemy_type;

typedef struct {
    s_v2 vel;
    int move_axis;
} s_snake_enemy_type_data;

typedef struct {
    bool active; // screw bitsets amiright

    s_v2 pos;

    e_enemy_type type;

    union {
        s_snake_enemy_type_data snake_type;
    };
} s_enemy;

typedef struct {
    s_rect rect;
    int dmg;
    bool enemy;
} s_hitbox;

#define ENEMY_LIMIT 128

typedef struct {
    s_tilemap tilemap;

    s_player player;

    s_enemy enemies[ENEMY_LIMIT];

    s_arrow arrows[16];
    int arrow_cnt;

    s_hitbox hitboxes[128];
    int hitbox_cnt;

    int hp;
    int gold_cnt;

    s_v2 view_pos;

    bool completed;
} s_level;

typedef struct {
    s_texture_group textures;
    s_font_group fonts;
    bool title;
    float title_alpha;
    s_level lvl;
} s_game;

bool InitGame(const s_game_init_context* const zfw_context);
e_game_tick_result GameTick(const s_game_tick_context* const zfw_context);
bool RenderGame(const s_game_render_context* const zfw_context);
void CleanGame(void* const dev_mem);

bool WARN_UNUSED_RESULT GenLevel(s_level* const lvl, const s_v2_s32 window_size, s_mem_arena* const temp_mem_arena);
void UpdateLevel(s_level* const lvl, const s_game_tick_context* const zfw_context);
void RenderLevel(s_level* const lvl, const s_rendering_context* const rc, const s_texture_group* const textures);
bool RenderLevelUI(s_level* const lvl, const s_rendering_context* const rc, const s_font_group* const fonts, s_mem_arena* const temp_mem_arena);

static inline void RenderSprite(const s_rendering_context* const rendering_context, const s_texture_group* const textures, const e_sprite spr, const s_v2 pos, const s_v2 origin, const s_v2 scale, const t_r32 rot, const u_v4 blend) {
    const s_sprite_info* const spr_info = STATIC_ARRAY_ELEM(g_sprite_infos, spr);
    RenderTexture(rendering_context, textures, spr_info->tex, spr_info->src_rect, pos, origin, scale, rot, blend);

}

#endif
