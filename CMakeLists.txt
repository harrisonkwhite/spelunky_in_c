cmake_minimum_required(VERSION 3.28)
project(untitled_game C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(zeta_framework)

add_executable(untitled_game
    src/entry.c
    src/game.c

    src/game.h
)

target_link_libraries(untitled_game PRIVATE zfw_core)

set_target_properties(untitled_game PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_custom_target(pack_assets ALL
    COMMAND $<TARGET_FILE:zfw_asset_packer>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packing assets..."
    VERBATIM
)

add_dependencies(untitled_game pack_assets)
