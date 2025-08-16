#include "game.h"

float g_view_scale;

static void RefreshViewScale(const s_v2_s32 window_size) {
    g_view_scale = 8.0f;

    if (window_size.x > 1600 || window_size.y > 900) {
        g_view_scale = 5.0f;
    }

    if (window_size.x > 1920 || window_size.y > 1080) {
        g_view_scale = 6.0f;
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
    game->title_alpha = 1.0f;

    if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
        return false;
    }

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    RefreshViewScale(zfw_context->window_state.size);

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_x)) {
        game->title = false;
        game->lvl.started = true;
    }

    if (!game->title) {
        game->title_alpha = Lerp(game->title_alpha, 0.0f, 0.4f);
    }

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_r)) {
        ZERO_OUT(game->lvl);

        if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
            return false;
        }
    }

    const e_level_update_end_result res = UpdateLevel(&game->lvl, zfw_context);

    switch (res) {
        case ek_level_update_end_result_normal:
            break;

        case ek_level_update_end_result_restart:
            {
                ZERO_OUT(game->lvl);

                if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
                    return false;
                }

                game->title = true;
                game->title_alpha = 1.0f;
            }

            break;

        case ek_level_update_end_result_next:
            {
                const int lvl_index_old = game->lvl.index;

                ZERO_OUT(game->lvl);

                if (!GenLevel(&game->lvl, zfw_context->window_state.size, zfw_context->temp_mem_arena)) {
                    return false;
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

    if (game->title_alpha > 0.001f) {
        const s_rect screen_rect = {0.0f, 0.0f, rc->window_size.x, rc->window_size.y};
        RenderRect(rc, screen_rect, (u_v4){BLACK.rgb, 0.7f * game->title_alpha});

        const u_v4 col = {WHITE.rgb, game->title_alpha};

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("Spelunky in C"), &game->fonts, ek_font_roboto_96, (s_v2){rc->window_size.x / 2.0f, rc->window_size.y * 0.4f}, ALIGNMENT_CENTER, col, zfw_context->temp_mem_arena)) {
            return false;
        }

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("Press [X] to start."), &game->fonts, ek_font_roboto_64, (s_v2){rc->window_size.x / 2.0f, rc->window_size.y * 0.75f}, ALIGNMENT_CENTER, col, zfw_context->temp_mem_arena)) {
            return false;
        }
    }

    return true;
}

void CleanGame(void* const dev_mem) {
}
