# pvz-console-edition - ReXGlue Recompiled Project
#
# SDK boilerplate is pinned alongside the project; guest code is generated.

cmake_minimum_required(VERSION 3.25)
project(pvz-console-edition VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep the generated guest symbols and internal target name stable.
set(REXGLUE_HOST_TARGET pvz)
include(cmake/rexglue.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    config/game.lock.json metadata/icons/app.ico metadata/icons/game.png)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/game.lock.json" PVZ_GAME_LOCK)
foreach(key title_id xex_sha256 main_pak_sha256 en_pak_sha256)
    string(JSON PVZ_${key} GET "${PVZ_GAME_LOCK}" ${key})
endforeach()
configure_file(src/game_content_lock.h.in ${CMAKE_CURRENT_BINARY_DIR}/game_content_lock.h @ONLY)

# Sources
set(PVZ_SOURCES
    src/native/pixel_to_grid_x.cpp
    src/gamepad_repeat.cpp
    src/main.cpp
    src/crt_compat.cpp
    src/camera_compat.cpp
    src/mouse_bridge.cpp
    src/challenge_mouse.cpp
    src/bonus_games.cpp
    src/service_mouse.cpp
    src/frame_capture.cpp
    src/hitch_profiler.cpp
    src/input_replay.cpp
    src/deterministic_replay.cpp
    src/state_dump.cpp
    src/replay_fixtures.cpp
    src/input_backend.cpp
    src/windows_identity.cpp
    src/display_sync.cpp
    src/game_settings.cpp
    src/key_bindings.cpp
    src/render_scale.cpp
    src/pc_menus.cpp
    src/game_edition.cpp
    src/controller_assets.cpp
    src/pc_game_fixes.cpp
    src/audio_fixes.cpp
    src/audio_diagnostics.cpp
    src/seed_image_fixes.cpp
    src/startup_video.cpp
)

if(WIN32)
    enable_language(RC)
    configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    add_executable(pvz WIN32 ${PVZ_SOURCES})
    target_sources(pvz PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    target_link_libraries(pvz PRIVATE bcrypt advapi32 comdlg32 windowscodecs ole32)
else()
    add_executable(pvz ${PVZ_SOURCES})
endif()
set_target_properties(pvz PROPERTIES OUTPUT_NAME "pvz-console-edition")
# The SDK builds its runtime with SSE4.1, but adds that flag only for GNU-style
# compilers; clang-cl would otherwise emulate SSSE3/SSE4.1 vector operations
# in the recompiled code through SSE2. RelWithDebInfo also limits inlining
# (/Ob1), which the many small recompiled helpers depend on.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
   AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(pvz PRIVATE -msse4.1 /Ob2)
endif()
target_include_directories(pvz PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

include(CTest)
if(BUILD_TESTING)
    add_executable(xbox_layout_test tests/xbox_layout_test.cpp)
    target_include_directories(xbox_layout_test PRIVATE src
        $<TARGET_PROPERTY:rex::runtime,INTERFACE_INCLUDE_DIRECTORIES>)
    add_test(NAME xbox_layout COMMAND xbox_layout_test)
    add_executable(mouse_input_test tests/mouse_input_test.cpp)
    target_include_directories(mouse_input_test PRIVATE src)
    add_test(NAME mouse_input COMMAND mouse_input_test)
    add_executable(widget_tree_test tests/widget_tree_test.cpp)
    target_include_directories(widget_tree_test PRIVATE src)
    add_test(NAME widget_tree COMMAND widget_tree_test)
    add_executable(replay_event_test tests/replay_event_test.cpp)
    target_include_directories(replay_event_test PRIVATE src)
    add_test(NAME replay_event COMMAND replay_event_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/replays)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME input_audit_manifest COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/input_audit_manifest_test.py)
        add_test(NAME state_dump_compare COMMAND ${Python3_EXECUTABLE} -I
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compare_state_dumps.py --self-test)
    endif()
    add_executable(state_dump_test src/state_dump.cpp)
    target_compile_definitions(state_dump_test PRIVATE PVZ_STATE_DUMP_TEST=1)
    target_include_directories(state_dump_test PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(state_dump_test PRIVATE rex::runtime)
    rexglue_apply_target_settings(state_dump_test)
    add_test(NAME state_dump_pool COMMAND state_dump_test)
    add_executable(pointer_mapping_test tests/pointer_mapping_test.cpp)
    target_include_directories(pointer_mapping_test PRIVATE src)
    add_test(NAME pointer_mapping COMMAND pointer_mapping_test)
    add_executable(input_activity_test tests/input_activity_test.cpp)
    target_include_directories(input_activity_test PRIVATE src)
    add_test(NAME input_activity COMMAND input_activity_test)
    add_executable(local_players_test tests/local_players_test.cpp)
    target_include_directories(local_players_test PRIVATE src)
    add_test(NAME local_players COMMAND local_players_test)
    add_executable(mouse_prompt_test tests/mouse_prompt_test.cpp)
    target_include_directories(mouse_prompt_test PRIVATE src)
    add_test(NAME mouse_prompt COMMAND mouse_prompt_test)
    add_executable(bridge_mnk_test tests/bridge_mnk_test.cpp)
    target_include_directories(bridge_mnk_test PRIVATE src)
    target_link_libraries(bridge_mnk_test PRIVATE rex::runtime)
    rexglue_apply_target_settings(bridge_mnk_test)
    # Register the same bridge switch without linking gameplay hooks.
    target_sources(bridge_mnk_test PRIVATE tests/bridge_mnk_cvars.cpp src/key_bindings.cpp src/render_scale.cpp)
    add_test(NAME bridge_mnk COMMAND bridge_mnk_test)
    if(WIN32)
        add_executable(game_edition_test tests/game_edition_test.cpp src/game_edition.cpp)
        target_include_directories(game_edition_test PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
        target_link_libraries(game_edition_test PRIVATE rex::runtime bcrypt)
        rexglue_apply_target_settings(game_edition_test)
        add_test(NAME game_edition COMMAND game_edition_test ${CMAKE_CURRENT_SOURCE_DIR}/assets
            "${CMAKE_CURRENT_SOURCE_DIR}/Plants vs. Zombies (World) (XBLA)/584109FF/000D0000/281104A67F7C961E2736A5F6F524D43FC81EB79158"
            "${CMAKE_CURRENT_SOURCE_DIR}/Plants vs. Zombies (USA) (En,Ja,Fr,De,Es,It).iso")
    endif()
endif()

rexglue_setup_target(pvz GPU_PLUGINS xenos)
add_custom_command(TARGET pvz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/pc_defaults.toml
        $<TARGET_FILE_DIR:pvz>/pc_defaults.toml
    # Our own mouse help illustrations (scripts/make_mouse_help_images.py).
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:pvz>/mouse
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/mouse/help_menu_image_mouse_pc.ptx
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/mouse/help_menu_image_mouse_console.ptx
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/mouse/vs_mouse_player1.ptx
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/mouse/vs_mouse_player2.ptx
        $<TARGET_FILE_DIR:pvz>/mouse)

add_executable(pvz_inspect EXCLUDE_FROM_ALL scripts/inspect_xex.cpp)
target_link_libraries(pvz_inspect PRIVATE rex::runtime)
rexglue_apply_target_settings(pvz_inspect)
if(WIN32)
    add_custom_command(TARGET pvz_inspect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:pvz_inspect> $<TARGET_FILE_DIR:pvz_inspect>
        COMMAND_EXPAND_LISTS)
endif()
