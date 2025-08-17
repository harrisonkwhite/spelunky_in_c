#include "game.h"

#define TITLE_FLICKER_INTERVAL 10
#define TITLE_FADE_LERP 0.4f

#define FADE_ALPHA_IN_LERP 0.25f
#define FADE_ALPHA_OUT_LERP 0.15f
#define FADE_ALPHA_PREC_THRESH 0.002f

bool InitGame(const s_game_init_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (!InitTextureGroup(&game->textures, eks_texture_cnt, GenTextureRGBA, zfw_context->perm_mem_arena, zfw_context->gl_res_arena, zfw_context->temp_mem_arena)) {
        return false;
    }

    if (!InitFontGroupFromFiles(&game->fonts, (s_char_array_view_array_view)ARRAY_FROM_STATIC(g_font_file_paths), zfw_context->perm_mem_arena, zfw_context->gl_res_arena, zfw_context->temp_mem_arena)) {
        return false;
    }

    game->title = true;
    game->title_alpha = 1.0f;

    if (!GenLevel(&game->lvl, zfw_context->window_state.size, &game->run_state, zfw_context->temp_mem_arena)) {
        return false;
    }

    game->run_state = (s_game_run_state){
        .lvl_num = 1
    };

    return true;
}

e_game_tick_result GameTick(const s_game_tick_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    if (game->title) {
        if (game->title_flicker_time < TITLE_FLICKER_INTERVAL) {
            game->title_flicker_time++;
        } else {
            game->title_flicker = !game->title_flicker;
            game->title_flicker_time = 0;
        }

        if (IsKeyPressed(&zfw_context->input_context, ek_key_code_enter)) {
            game->title = false;
            game->lvl.started = true;
        }
    } else {
        game->title_alpha = Lerp(game->title_alpha, 0.0f, TITLE_FADE_LERP);
    }

    e_level_update_end_result res = UpdateLevel(&game->lvl, &game->run_state, zfw_context);

    switch (res) {
        case ek_level_update_end_result_normal:
            break;

        case ek_level_update_end_result_restart:
            game->fade = ek_fade_restart;
            break;

        case ek_level_update_end_result_next:
            game->fade = ek_fade_next;
            break;
    }

    if (IsKeyPressed(&zfw_context->input_context, ek_key_code_r)) {
        game->fade = ek_fade_restart;
    }

    if (game->fade != ek_fade_none) {
        if (game->fade_alpha <= 1.0f - FADE_ALPHA_PREC_THRESH) {
            game->fade_alpha = Lerp(game->fade_alpha, 1.0f, FADE_ALPHA_IN_LERP);
        } else {
            switch (game->fade) {
                case ek_fade_restart:
                    ZERO_OUT(game->lvl);

                    game->title = true;
                    game->title_alpha = 1.0f;
                    game->title_flicker = false;
                    game->title_flicker_time = 0;
                    game->run_state = (s_game_run_state){
                        .lvl_num = 1
                    };

                    if (!GenLevel(&game->lvl, zfw_context->window_state.size, &game->run_state, zfw_context->temp_mem_arena)) {
                        return ek_game_tick_result_error;
                    }

                    break;

                case ek_fade_next:
                    ZERO_OUT(game->lvl);

                    game->run_state.lvl_num++;

                    if (!GenLevel(&game->lvl, zfw_context->window_state.size, &game->run_state, zfw_context->temp_mem_arena)) {
                        return ek_game_tick_result_error;
                    }

                    game->lvl.started = true;

                    break;
            }

            game->fade = ek_fade_none;
        }
    } else {
        if (game->fade_alpha >= FADE_ALPHA_PREC_THRESH) {
            game->fade_alpha = Lerp(game->fade_alpha, 0.0f, FADE_ALPHA_OUT_LERP);
        }
    }

    return ek_game_tick_result_normal;
}

bool RenderGame(const s_game_render_context* const zfw_context) {
    s_game* const game = zfw_context->dev_mem;

    const s_rendering_context* const rc = &zfw_context->rendering_context;

#if 0
    SetSurface(rc, &game->lvl_surf);
#endif

    Clear(rc, BLACK_SPECIAL);
    RenderLevel(&game->lvl, rc, &game->textures);

#if 0
    UnsetSurface(rc);

    SetSurfaceShaderProg(rc, &rc->basis->builtin_shader_progs, ek_builtin_shader_prog_surface_default);
    RenderSurface(rc, &game->lvl_surf, (s_v2){0}, (s_v2){VIEW_SCALE, VIEW_SCALE}, false);
#endif

    if (!RenderLevelUI(&game->lvl, &game->run_state, &zfw_context->rendering_context, &game->fonts, zfw_context->temp_mem_arena)) {
        return false;
    }

    if (game->title_alpha >= 0.001f) {
#if 0
        const s_rect screen_rect = {0.0f, 0.0f, rc->window_size.x, rc->window_size.y};
        RenderRect(rc, screen_rect, (u_v4){BLACK_SPECIAL.rgb, 0.7f * game->title_alpha});
#endif

        const float bg_height = 540.0f;
        const s_rect bg_rect = {0.0f, (rc->window_size.y - bg_height) / 2.0f, rc->window_size.x, bg_height};
        const float bg_rect_outline_size = VIEW_SCALE;
        RenderRect(rc, (s_rect){bg_rect.x, bg_rect.y - bg_rect_outline_size, bg_rect.width, bg_rect_outline_size}, (u_v4){WHITE_SPECIAL.rgb, game->title_alpha});
        RenderRect(rc, (s_rect){bg_rect.x, bg_rect.y + bg_rect.height, bg_rect.width, bg_rect_outline_size}, (u_v4){WHITE_SPECIAL.rgb, game->title_alpha});
        RenderRect(rc, bg_rect, (u_v4){BLACK_SPECIAL.rgb, BG_ALPHA * game->title_alpha});

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("SPELUNKY (IN C)"), &game->fonts, ek_font_pixel_very_large, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) - 168.0f}, ALIGNMENT_CENTER, (u_v4){WHITE_SPECIAL.rgb, game->title_alpha}, zfw_context->temp_mem_arena)) {
            return false;
        }

        if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("[RIGHT]/[LEFT]/[DOWN]/[UP] TO MOVE\n[X] TO ATTACK\n[Z] TO INTERACT"), &game->fonts, ek_font_pixel_small, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) + 20.0f}, ALIGNMENT_CENTER, (u_v4){WHITE_SPECIAL.rgb, game->title_alpha}, zfw_context->temp_mem_arena)) {
            return false;
        }

        {
            const u_v4 col = {game->title_flicker ? YELLOW.rgb : WHITE_SPECIAL.rgb, game->title_alpha};

            if (!RenderStr(rc, (s_char_array_view)ARRAY_FROM_STATIC("PRESS [ENTER] TO START"), &game->fonts, ek_font_pixel_small, (s_v2){rc->window_size.x / 2.0f, (rc->window_size.y / 2.0f) + 184.0f}, ALIGNMENT_CENTER, col, zfw_context->temp_mem_arena)) {
                return false;
            }
        }
    }

    if (game->fade_alpha >= FADE_ALPHA_PREC_THRESH) {
        RenderRect(rc, (s_rect){0.0f, 0.0f, rc->window_size.x, rc->window_size.y}, (u_v4){BLACK_SPECIAL.rgb, game->fade_alpha});
    }

    return true;
}

void CleanGame(void* const dev_mem) {
}
