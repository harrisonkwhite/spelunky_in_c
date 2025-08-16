#ifndef GAME_H
#define GAME_H

#include <zfwc.h>

#define SHOW_DEBUG_HITBOXES 1

#define HP_LIMIT 3

#define TILEMAP_ROOM_WIDTH 12
#define TILEMAP_ROOM_HEIGHT 8
#define TILEMAP_ROOMS_HOR 4
#define TILEMAP_ROOMS_VERT 4
#define TILEMAP_WIDTH (TILEMAP_ROOM_WIDTH * TILEMAP_ROOMS_HOR)
#define TILEMAP_HEIGHT (TILEMAP_ROOM_HEIGHT * TILEMAP_ROOMS_VERT)

#define TILE_SIZE 8

#define GOLD_INCR 250

#define VIEW_SCALE 8.0f

#define BG_ALPHA 0.8f

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
    ek_sprite_ladder_platform_tile,
    ek_sprite_gold_tile,
    ek_sprite_shooter_tile_left,
    ek_sprite_shooter_tile_right,
    ek_sprite_door_tile_0,
    ek_sprite_door_tile_1,
    ek_sprite_door_tile_2,
    ek_sprite_door_tile_3,
    ek_sprite_spike_tile,
    ek_sprite_player,
    ek_sprite_snake_enemy,
    ek_sprite_bg,
    ek_sprite_arrow,
    ek_sprite_whip,
    ek_sprite_rock_item,
} e_sprite;

typedef struct {
    e_texture tex;
    s_rect_s32 src_rect;
} s_sprite_info;

static s_sprite_info g_sprite_infos[] = {
    [ek_sprite_dirt_tile] = {
        .tex = ek_texture_level,
        .src_rect = {0, 0, 8, 8}
    },
    [ek_sprite_ladder_tile] = {
        .tex = ek_texture_level,
        .src_rect = {8, 0, 8, 8}
    },
    [ek_sprite_ladder_platform_tile] = {
        .tex = ek_texture_level,
        .src_rect = {16, 16, 8, 8}
    },
    [ek_sprite_gold_tile] = {
        .tex = ek_texture_level,
        .src_rect = {16, 0, 8, 8}
    },
    [ek_sprite_shooter_tile_left] = {
        .tex = ek_texture_level,
        .src_rect = {24, 0, 8, 8}
    },
    [ek_sprite_shooter_tile_right] = {
        .tex = ek_texture_level,
        .src_rect = {32, 0, 8, 8}
    },
    [ek_sprite_door_tile_0] = {
        .tex = ek_texture_level,
        .src_rect = {0, 8, 8, 8}
    },
    [ek_sprite_door_tile_1] = {
        .tex = ek_texture_level,
        .src_rect = {8, 8, 8, 8}
    },
    [ek_sprite_door_tile_2] = {
        .tex = ek_texture_level,
        .src_rect = {16, 8, 8, 8}
    },
    [ek_sprite_door_tile_3] = {
        .tex = ek_texture_level,
        .src_rect = {24, 8, 8, 8}
    },
    [ek_sprite_spike_tile] = {
        .tex = ek_texture_level,
        .src_rect = {40, 0, 8, 8}
    },
    [ek_sprite_player] = {
        .tex = ek_texture_level,
        .src_rect = {1, 17, 6, 7}
    },
    [ek_sprite_snake_enemy] = {
        .tex = ek_texture_level,
        .src_rect = {9, 17, 6, 6}
    },
    [ek_sprite_bg] = {
        .tex = ek_texture_level,
        .src_rect = {0, 24, 8, 8}
    },
    [ek_sprite_arrow] = {
        .tex = ek_texture_level,
        .src_rect = {9, 26, 6, 4}
    },
    [ek_sprite_whip] = {
        .tex = ek_texture_level,
        .src_rect = {16, 27, 8, 1}
    },
    [ek_sprite_rock_item] = {
        .tex = ek_texture_level,
        .src_rect = {26, 18, 4, 4}
    }
};

typedef enum {
    ek_font_pixel_very_small,
    ek_font_pixel_small,
    ek_font_pixel_med,
    ek_font_pixel_large,
    ek_font_pixel_very_large
} e_font;

static const s_char_array_view g_font_file_paths[] = {
    [ek_font_pixel_very_small] = ARRAY_FROM_STATIC("assets/fonts/very_small"),
    [ek_font_pixel_small] = ARRAY_FROM_STATIC("assets/fonts/small"),
    [ek_font_pixel_med] = ARRAY_FROM_STATIC("assets/fonts/med"),
    [ek_font_pixel_large] = ARRAY_FROM_STATIC("assets/fonts/large"),
    [ek_font_pixel_very_large] = ARRAY_FROM_STATIC("assets/fonts/very_large")
};

typedef enum {
    ek_tile_state_empty,
    ek_tile_state_dirt,
    ek_tile_state_ladder,
    ek_tile_state_ladder_platform,
    ek_tile_state_gold,
    ek_tile_state_shooter,
    ek_tile_state_entrance,
    ek_tile_state_exit,
    ek_tile_state_spike
} e_tile_state;

typedef enum {
    ek_tile_not_platform,
    ek_tile_safe_platform,
    ek_tile_death_platform
} e_tile_platform_type;

static bool g_tile_state_is_behind_ents[] = {
    [ek_tile_state_empty] = false,
    [ek_tile_state_dirt] = false,
    [ek_tile_state_ladder] = true,
    [ek_tile_state_ladder_platform] = true,
    [ek_tile_state_gold] = true,
    [ek_tile_state_shooter] = false,
    [ek_tile_state_entrance] = true,
    [ek_tile_state_exit] = true,
    [ek_tile_state_spike] = true
};

static bool g_tile_states_solid[] = {
    [ek_tile_state_empty] = false,
    [ek_tile_state_dirt] = true,
    [ek_tile_state_ladder] = false,
    [ek_tile_state_ladder_platform] = true,
    [ek_tile_state_gold] = false,
    [ek_tile_state_shooter] = true,
    [ek_tile_state_entrance] = false,
    [ek_tile_state_exit] = false,
    [ek_tile_state_spike] = false
};

static e_tile_platform_type g_tile_states_platform[] = {
    [ek_tile_state_empty] = ek_tile_not_platform,
    [ek_tile_state_dirt] = ek_tile_not_platform,
    [ek_tile_state_ladder] = ek_tile_not_platform,
    [ek_tile_state_ladder_platform] = ek_tile_safe_platform,
    [ek_tile_state_gold] = ek_tile_not_platform,
    [ek_tile_state_shooter] = ek_tile_not_platform,
    [ek_tile_state_entrance] = ek_tile_not_platform,
    [ek_tile_state_exit] = ek_tile_not_platform,
    [ek_tile_state_spike] = ek_tile_death_platform
};

static e_sprite g_tile_state_sprs[] = {
    [ek_tile_state_dirt] = ek_sprite_dirt_tile,
    [ek_tile_state_ladder] = ek_sprite_ladder_tile,
    [ek_tile_state_ladder_platform] = ek_sprite_ladder_platform_tile,
    [ek_tile_state_gold] = ek_sprite_gold_tile,
    [ek_tile_state_shooter] = ek_sprite_shooter_tile_left,
    [ek_tile_state_entrance] = ek_sprite_door_tile_0,
    [ek_tile_state_exit] = ek_sprite_door_tile_3,
    [ek_tile_state_spike] = ek_sprite_spike_tile
};

typedef struct {
    e_tile_state state;

    bool shooter_shot;
    bool shooter_facing_right;

    int door_frame_time;
} s_tile;

typedef struct {
    int starting_room_x;
    s_tile tiles[TILEMAP_HEIGHT][TILEMAP_WIDTH];
} s_tilemap;

typedef enum {
    ek_item_type_rock
} e_item_type;

static const e_sprite g_item_type_sprs[] = {
    [ek_item_type_rock] = ek_sprite_rock_item
};

typedef struct {
    bool active;
    e_item_type type;
    s_v2 pos;
    s_v2 vel;
} s_item_drop;

typedef struct {
    s_v2 pos;
    s_v2 vel;
    bool climbing;
    bool cant_climb;
    bool facing_right;
    bool latching;
    int whip_break_time;
    bool holding_item;
    e_item_type item_type_held;
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

typedef struct {
    bool active;
    s_v2 pos;
    s_v2 vel;
    float alpha;
    float rot;
} s_particle;

#define ENEMY_LIMIT 128

typedef enum {
    ek_level_update_end_result_normal,
    ek_level_update_end_result_next,
    ek_level_update_end_result_restart
} e_level_update_end_result;

typedef enum {
    ek_player_control_display_none,
    ek_player_control_display_down,
    ek_player_control_display_up
} e_player_control_display;

typedef struct {
    bool started;
    int post_start_wait_time;

    bool leaving;
    int leaving_wait_time;

    s_tilemap tilemap;

    s_player player;

    s_enemy enemies[ENEMY_LIMIT];

    s_item_drop item_drops[32];
    int item_drop_hovered_index;

    s_particle particles[128];

    s_arrow arrows[16];
    int arrow_cnt;

    s_hitbox hitboxes[128];
    int hitbox_cnt;

    int hp;

    s_v2 view_pos_no_shake;
    float view_shake;

    e_player_control_display control_display;

    float death_alpha;

    float general_ui_alpha;
} s_level;

typedef struct {
    int lvl_num;
    int gold_cnt;
} s_game_run_state;

typedef enum {
    ek_fade_none,
    ek_fade_restart,
    ek_fade_next
} e_fade;

typedef struct {
    s_texture_group textures;
    s_font_group fonts;
    s_surface lvl_surf;
    bool title;
    float title_alpha;
    bool title_flicker;
    int title_flicker_time;
    e_fade fade;
    float fade_alpha;
    s_level lvl;
    s_game_run_state run_state;
} s_game;

bool InitGame(const s_game_init_context* const zfw_context);
e_game_tick_result GameTick(const s_game_tick_context* const zfw_context);
bool RenderGame(const s_game_render_context* const zfw_context);
void CleanGame(void* const dev_mem);

bool WARN_UNUSED_RESULT GenLevel(s_level* const lvl, const s_v2_s32 window_size, s_mem_arena* const temp_mem_arena);
e_level_update_end_result UpdateLevel(s_level* const lvl, s_game_run_state* const run_state, const s_game_tick_context* const zfw_context);
void RenderLevel(const s_level* const lvl, const s_rendering_context* const rc, const s_texture_group* const textures);
bool RenderLevelUI(const s_level* const lvl, const s_game_run_state* const run_state, const s_rendering_context* const rc, const s_font_group* const fonts, s_mem_arena* const temp_mem_arena);

static inline void RenderSprite(const s_rendering_context* const rendering_context, const s_texture_group* const textures, const e_sprite spr, const s_v2 pos, const s_v2 origin, const s_v2 scale, const t_r32 rot, const u_v4 blend) {
    const s_sprite_info* const spr_info = STATIC_ARRAY_ELEM(g_sprite_infos, spr);
    RenderTexture(rendering_context, textures, spr_info->tex, spr_info->src_rect, pos, origin, scale, rot, blend);

}

#endif
