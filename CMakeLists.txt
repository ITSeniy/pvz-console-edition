# pvz-console-edition - ReXGlue Recompiled Project
#
# SDK boilerplate is pinned alongside the project; guest code is generated.

cmake_minimum_required(VERSION 3.25)
project(pvz-console-edition VERSION 0.1.0 LANGUAGES CXX)

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
    src/main.cpp
    src/crt_compat.cpp
    src/camera_compat.cpp
    src/mouse_bridge.cpp
    src/challenge_mouse.cpp
    src/service_mouse.cpp
    src/frame_capture.cpp
    src/input_replay.cpp
    src/replay_fixtures.cpp
    src/input_backend.cpp
    src/windows_identity.cpp
    src/display_sync.cpp
    src/game_settings.cpp
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
    target_link_libraries(pvz PRIVATE bcrypt advapi32)
else()
    add_executable(pvz ${PVZ_SOURCES})
endif()
set_target_properties(pvz PROPERTIES OUTPUT_NAME "pvz-console-edition")
target_include_directories(pvz PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

include(CTest)
if(BUILD_TESTING)
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
    endif()
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
    target_sources(bridge_mnk_test PRIVATE tests/bridge_mnk_cvars.cpp)
    add_test(NAME bridge_mnk COMMAND bridge_mnk_test)
    if(WIN32)
        add_executable(game_edition_test tests/game_edition_test.cpp src/game_edition.cpp)
        target_include_directories(game_edition_test PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
        target_link_libraries(game_edition_test PRIVATE rex::runtime bcrypt)
        rexglue_apply_target_settings(game_edition_test)
        add_test(NAME game_edition COMMAND game_edition_test ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    endif()
endif()

rexglue_setup_target(pvz GPU_PLUGINS xenos)
add_custom_command(TARGET pvz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/pc_defaults.toml
        $<TARGET_FILE_DIR:pvz>/pc_defaults.toml)

add_executable(pvz_inspect EXCLUDE_FROM_ALL scripts/inspect_xex.cpp)
target_link_libraries(pvz_inspect PRIVATE rex::runtime)
rexglue_apply_target_settings(pvz_inspect)
if(WIN32)
    add_custom_command(TARGET pvz_inspect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:pvz_inspect> $<TARGET_FILE_DIR:pvz_inspect>
        COMMAND_EXPAND_LISTS)
endif()
