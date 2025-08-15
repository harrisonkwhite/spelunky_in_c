#include "game.h"

#include <stdlib.h>

int main() {
    LOG("sahdjklashdk");

    const s_game_info game_info = {
        .window_init_size = {1280, 720},
        .window_title = ARRAY_FROM_STATIC("Spelunky in C"),

        .init_func = InitGame,
        .tick_func = GameTick,
        .render_func = RenderGame,
        .clean_func = CleanGame
    };

    return RunGame(&game_info) ? EXIT_SUCCESS : EXIT_FAILURE;
}
