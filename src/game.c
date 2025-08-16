#include "game.h"

float g_view_scale;

static void RefreshViewScale(const s_v2_s32 window_size) {
    g_view_scale = 8.0f;

    if (window_size.x > 1600 || window_size.y > 900) {
        g_view_scale = 10.0f;
    }

    if (window_size.x > 1920 || window_size.y > 1080) {
        g_view_scale = 12.0f;
    }
}

bool InitGame(const s_game_init_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    RefreshViewScale(zfw_context->window_state.size);

    if (!InitTextureGroup(&game->textures, eks_texture_cnt, GenTextureRGBA, zfw_context->perm_mem_arena, zfw_context->gl_res_arena, zfw_context->temp_mem_arena)) {
        return false;
    }

    if (!InitFontGroupFromFiles(&game->fonts, (s_char_array_view_array_view)ARRAY_FROM_STATIC(g_font_file_paths), zfw_context->perm_mem_arena, zfw_context->gl_res_arena, zfw_context->temp_mem_arena)) {
        return false;
    }

    if (!InitSurface(&game->lvl_surf, (s_v2_s32){zfw_context->window_state.size.x / g_view_scale, zfw_context->window_state.size.y / g_view_scale}, zfw_context->gl_res_arena)) {
        return false;
    }

    game->title = true;

    if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
        return false;
    }

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    RefreshViewScale(zfw_context->window_state.size);

    const s_v2_s32 lvl_surf_size_ideal = {zfw_context->window_state.size.x / g_view_scale, zfw_context->window_state.size.y / g_view_scale};

    if (!V2S32sEqual(game->lvl_surf.size, lvl_surf_size_ideal)) {
        if (!ResizeSurface(&game->lvl_surf, lvl_surf_size_ideal)) {
            return ek_game_tick_result_error;
        }
    }

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_x)) {
        game->title = false;
        game->lvl.started = true;
    }

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_r)) {
        ZERO_OUT(game->lvl);

        if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
            return ek_game_tick_result_error;
        }

        game->lvl.started = true;
        game->lvl.index = 0;
    }

    const e_level_update_end_result res = UpdateLevel(&game->lvl, zfw_context);

    switch (res) {
        case ek_level_update_end_result_normal:
            break;

        case ek_level_update_end_result_restart:
            {
                ZERO_OUT(game->lvl);

                if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
                    return ek_game_tick_result_error;
                }

                game->title = true;
            }

            break;

        case ek_level_update_end_result_next:
            {
                const int lvl_index_old = game->lvl.index;

                ZERO_OUT(game->lvl);

                if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
                    return ek_game_tick_result_error;
                }

                game->lvl.started = true;
                game->lvl.index = lvl_index_old + 1;
            }

            break;
    }

    return ek_game_tick_result_normal;
}

bool RenderGame(const s_game_render_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    const s_rendering_context* const rc = &zfw_context->rendering_context;

    SetSurface(rc, &game->lvl_surf);

    Clear(rc, BLACK);
    RenderLevel(&game->lvl, rc, &game->textures);

    UnsetSurface(rc);

    SetSurfaceShaderProg(rc, &rc->basis->builtin_shader_progs, ek_builtin_shader_prog_surface_default);
    RenderSurface(rc, &game->lvl_surf, (s_v2){0}, (s_v2){g_view_scale, g_view_scale}, false);

    if (!RenderLevelUI(&game->lvl, &zfw_context->rendering_context, &game->fonts, zfw_context->temp_mem_arena)) {
        return false;
    }

    if (game->title) {
#if 0
        const s_rect screen_rect = {0.0f, 0.0f, rc->window_size.x, rc->window_size.y};
        RenderRect(rc, screen_rect, (u_v4){BLACK.rgb, 0.7f * game->title_alpha});
#endif

        const float bg_height = 256.0f;
        const s_rect bg_rect = {0.0f, (rc->window_size.y - bg_height) / 2.0f, rc->window_size.x, bg_height};
        const float bg_rect_outline_size = g_view_scale;
        RenderRect(rc, (s_rect){bg_rect.x, bg_rect.y - bg_rect_outline_size, bg_rect.width, bg_rect.height + (bg_rect_outline_size * 2.0f)}, WHITE);
        RenderRect(rc, bg_rect, BLACK);

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("SPELUNKY IN C"), &game->fonts, ek_font_roboto_96, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) - 48.0f}, ALIGNMENT_CENTER, WHITE, zfw_context->temp_mem_arena)) {
            return false;
        }

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("PRESS [X] TO START"), &game->fonts, ek_font_roboto_48, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) + 48.0f}, ALIGNMENT_CENTER, WHITE, zfw_context->temp_mem_arena)) {
            return false;
        }
    }

    return true;
}

void CleanGame(void* const dev_mem) {
}
