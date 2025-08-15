#include "game.h"

#define PLAYER_MOVE_SPD 2.0f
#define PLAYER_MOVE_SPD_LERP 0.2f
#define PLAYER_SIZE (s_v2_s32){32.0f, 32.0f}

static s_rect GenPlayerRect(const s_v2 player_pos) {
    return (s_rect){player_pos.x - (PLAYER_SIZE.x / 2.0f), player_pos.y - (PLAYER_SIZE.y / 2.0f)};
}

void UpdateLevel(s_level* const lvl, s_game_tick_context* const zfw_context) {
    //
    // Player Movement
    //
    {
        const int move_axis = IsKeyDown(&zfw_context->input_context, ek_key_code_right) - IsKeyDown(&zfw_context->input_context, ek_key_code_left);

        const float vel_x_dest = PLAYER_MOVE_SPD * move_axis;
        lvl->player.vel.x = Lerp(lvl->player.vel.x, vel_x_dest, PLAYER_MOVE_SPD_LERP);

        lvl->player.pos = V2Sum(lvl->player.pos, lvl->player.vel);
    }
}

void RenderLevel(s_level* const lvl, const s_rendering_context* const rc) {
    s_matrix_4x4 lvl_view_mat = IdentityMatrix4x4();
    ScaleMatrix4x4(&lvl_view_mat, CAMERA_SCALE);
    SetViewMatrix(rc, &lvl_view_mat);

    //
    // Tilemap
    //
    for (int ty = 0; ty < TILEMAP_HEIGHT; ty++) {
        for (int tx = 0; tx < TILEMAP_WIDTH; tx++) {
            const e_tile_state ts = *STATIC_ARRAY_2D_ELEM(lvl->tilemap.states, ty, tx);

            if (ts == ek_tile_state_empty) {
                continue;
            }

            const s_rect rect = {tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            RenderRectWithOutlineAndOpaqueFill(rc, rect, WHITE.rgb, BLACK, 1.0f);
        }
    }

    //
    // Player
    //
    {
        const s_rect rect = GenPlayerRect(lvl->player.pos);
        RenderRectWithOutlineAndOpaqueFill(rc, rect, WHITE.rgb, BLACK, 1.0f);
    }
}
