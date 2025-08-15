cmake_minimum_required(VERSION 3.28)
project(spelunky_in_c C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(zeta_framework)

add_executable(spelunky_in_c
    src/entry.c
    src/game.c
    src/level.c

    src/game.h
)

target_link_libraries(spelunky_in_c PRIVATE zfw_core)

set_target_properties(spelunky_in_c PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_custom_target(pack_assets ALL
    COMMAND $<TARGET_FILE:zfw_asset_packer>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packing assets..."
    VERBATIM
)

add_dependencies(spelunky_in_c pack_assets)
