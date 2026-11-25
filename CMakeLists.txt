cmake_minimum_required(VERSION 3.20)
project(cm0102-mac VERSION 0.7.3 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# The secure connection of online play (0.7.0 slice 5, src/recomp_rt/win/secure.c), in every build: the browser's
# game compiles it too, though a page opens no sockets and offers no online games: mbedTLS 3.6, the LTS branch
# (Apache-2.0), built from its release with the settings of src/recomp_rt/win/tls_config.h. For a build with
# no download, unpack the release and pass -DFETCHCONTENT_SOURCE_DIR_MBEDTLS=<its folder>.
include(FetchContent)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Build the Mbed TLS programs" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Build the Mbed TLS tests" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "Mbed TLS warnings stop the build" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Build a shared Mbed TLS" FORCE)
set(MBEDTLS_USER_CONFIG_FILE "${CMAKE_SOURCE_DIR}/src/recomp_rt/win/tls_config.h" CACHE FILEPATH
    "Mbed TLS settings of Bosman Edition" FORCE)
# DOWNLOAD_EXTRACT_TIMESTAMP exists from CMake 3.24; the Linux build's container (Ubuntu 22.04) has 3.22, which
# takes the option as part of URL_HASH and stops.
set(MBEDTLS_FETCH_EXTRA "")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    set(MBEDTLS_FETCH_EXTRA DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
    URL_HASH SHA256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
    ${MBEDTLS_FETCH_EXTRA})
FetchContent_MakeAvailable(mbedtls)

# The browser build (web/README.md) makes the recompiled game only: the hand port and its tests need the host's SDL2.
if(NOT EMSCRIPTEN)
find_package(SDL2 REQUIRED)


add_executable(cm0102
    src/main.c
    src/demo.c
    src/si/display.c
    src/si/os_compat.c
    src/si/sound.c
    src/si/input.c
    src/si/rgn_loader.c
    src/si/gui_utils.c
    src/si/gui_glyphs.c
    src/si/gui_font.c
    src/si/gui_text.c
    src/si/guio.c
    src/si/area.c
    src/si/scrman.c
    src/si/screen_stack.c
    src/si/scrman_input.c
    src/si/langlib.c
    src/si/gui_menu.c
    src/si/progress_box.c
    src/si/utils.c
    src/si/date.c
    src/si/exe_image.c
    src/si/random.c
    src/cm3/background.c
    src/cm3/menubar.c
    src/cm3/file_screens.c
    src/cm3/setup.c
    src/cm3/database.c
    src/cm3/database_links.c
    src/cm3/key_index.c
    src/cm3/key_ids_table.c
    src/cm3/key_nation.c
)

target_include_directories(cm0102 PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_libraries(cm0102 PRIVATE ${SDL2_LIBRARIES})

# Runs functions of the port for the differential test in tests/diff.
add_executable(port_driver
    tests/diff/port_driver.c
    tests/diff/port_scene.c
    tests/diff/port_stack.c
    tests/diff/port_helpers.c
    tests/diff/port_start.c
    tests/diff/port_leagues.c
    tests/diff/port_progress.c
    tests/diff/port_scroll.c
    tests/diff/port_select.c
    tests/diff/port_dates.c
    tests/diff/port_entry.c
    tests/diff/port_database.c
    src/si/display.c
    src/si/rgn_loader.c
    src/si/gui_utils.c
    src/si/gui_glyphs.c
    src/si/gui_font.c
    src/si/gui_text.c
    src/si/os_compat.c
    src/si/guio.c
    src/si/input.c
    src/si/area.c
    src/si/scrman.c
    src/si/screen_stack.c
    src/si/scrman_input.c
    src/si/langlib.c
    src/si/gui_menu.c
    src/si/progress_box.c
    src/si/utils.c
    src/si/date.c
    src/si/exe_image.c
    src/si/random.c
    src/cm3/background.c
    src/cm3/menubar.c
    src/cm3/file_screens.c
    src/cm3/setup.c
    src/cm3/database.c
    src/cm3/database_links.c
    src/cm3/key_index.c
    src/cm3/key_ids_table.c
    src/cm3/key_nation.c
)
target_include_directories(port_driver PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_libraries(port_driver PRIVATE ${SDL2_LIBRARIES})

if(APPLE)
    set_target_properties(cm0102 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Championship Manager 01/02"
        MACOSX_BUNDLE_GUI_IDENTIFIER "uk.co.sigames.cm0102"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# Test of the settings file reader of the recompiled game: ctest, or build and run options_test.
if(NOT WIN32)
    enable_testing()
    add_executable(options_test tests/options/test_options.c src/recomp_rt/win/options.c
        src/recomp_rt/host/posix.c)
    target_include_directories(options_test PRIVATE src/recomp_rt/win)
    add_test(NAME options COMMAND options_test)
    add_executable(dbfolders_test tests/dbfolders/test_dbfolders.c src/recomp_rt/win/dbfolders.c
        src/recomp_rt/host/posix.c)
    target_include_directories(dbfolders_test PRIVATE src/recomp_rt/win)
    add_test(NAME dbfolders COMMAND dbfolders_test)
    # The Open folder buttons (0.6.1 item 5): host_open_folder starts the file manager with the folder.
    add_executable(host_folder_test tests/host/test_open_folder.c src/recomp_rt/host/posix.c)
    add_test(NAME host_folder COMMAND host_folder_test)
    add_executable(folder_names_test tests/host/test_folder_names.c src/recomp_rt/win/folder_names.c src/recomp_rt/host/posix.c)
    add_test(NAME folder_names COMMAND folder_names_test)
    # Season databases in the download (0.6.0 slice 14): zip reader, SHA-256, recognition, first pick.
    add_executable(seasons_db_test tests/seasons/test_seasons_db.c src/recomp_rt/win/seasons_db.c
        src/recomp_rt/win/seasons_zip.c src/recomp_rt/win/seasons_table.c src/recomp_rt/win/dbfolders.c
        src/recomp_rt/host/posix.c)
    target_include_directories(seasons_db_test PRIVATE src/recomp_rt/win tests/seasons)
    target_compile_definitions(seasons_db_test PRIVATE
        SIGNATURES_TSV="${CMAKE_SOURCE_DIR}/docs/research/rules-0.6.0/databases-signatures.tsv"
        MANIFEST_DIR="${CMAKE_SOURCE_DIR}/tools/clean_files")
    add_test(NAME seasons_db COMMAND seasons_db_test)
    # The heap of the recompiled game: the join of the free blocks at the end of the arena (decision O121).
    add_executable(heap_test tests/recomp/test_heap.c src/recomp_rt/heap.c)
    target_include_directories(heap_test PRIVATE src/recomp_rt)
    add_test(NAME heap COMMAND heap_test)
    # Token-saving hooks and the log wrapper (docs/specs/2026-10-19-token-saving-spec.md, slice 1).
    add_test(NAME hooks COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/dev/test_hooks.py")
    # The game's sockets (0.7.0 slice 2): WinSock behaviour on the host's sockets over 127.0.0.1.
    add_executable(net_core_test tests/net/test_net_core.c src/recomp_rt/win/net_core.c
        src/recomp_rt/host/net_posix.c)
    target_include_directories(net_core_test PRIVATE src/recomp_rt src/recomp_rt/win)
    add_test(NAME net_core COMMAND net_core_test)
    # Network screens in widescreen mode (0.7.0 item 212): the inverse of the widescreen scale is exact.
    add_executable(net_scale_test tests/net/test_net_scale.c src/recomp_rt/win/display_scale.c)
    target_include_directories(net_scale_test PRIVATE src/recomp_rt/win)
    add_test(NAME net_scale COMMAND net_scale_test)
    # Online games in the game list (0.7.0 slice 4): lobby HTTP rules, JSON, game list rows, join check.
    find_package(Threads REQUIRED)
    add_executable(lobby_test tests/net/test_lobby.c src/recomp_rt/win/lobby_http.c src/recomp_rt/win/lobby.c
        src/recomp_rt/win/lobby_rows.c src/recomp_rt/win/net_core.c src/recomp_rt/win/secure.c
        src/recomp_rt/host/net_posix.c)
    target_include_directories(lobby_test PRIVATE src/recomp_rt src/recomp_rt/win)
    target_link_libraries(lobby_test PRIVATE Threads::Threads mbedtls)
    add_test(NAME lobby COMMAND lobby_test)
    # Joins through the relay (0.7.0 slice 5): WebSocket frames, the socket pair, a fake relay with a host and a
    # human manager over the game's sockets, and the secure connection against a local TLS server.
    add_executable(relay_test tests/net/test_relay.c src/recomp_rt/win/relay.c src/recomp_rt/win/websocket.c
        src/recomp_rt/win/secure.c src/recomp_rt/win/lobby_http.c src/recomp_rt/win/net_core.c
        src/recomp_rt/host/net_posix.c)
    target_include_directories(relay_test PRIVATE src/recomp_rt src/recomp_rt/win)
    target_link_libraries(relay_test PRIVATE Threads::Threads mbedtls)
    add_test(NAME relay COMMAND relay_test)
    add_test(NAME relay_tls COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/net/tls_check.py" $<TARGET_FILE:relay_test>)
    set_tests_properties(relay_tls PROPERTIES SKIP_RETURN_CODE 77)
    # Clear errors, connection test and ping (0.7.0 slice 7): control packets, failure reasons, texts, the test.
    add_executable(net_errors_test tests/net/test_net_errors.c src/recomp_rt/win/net_errors.c
        src/recomp_rt/win/net_test.c src/recomp_rt/win/lobby_http.c src/recomp_rt/win/lobby.c
        src/recomp_rt/win/lobby_rows.c src/recomp_rt/win/net_core.c src/recomp_rt/win/secure.c
        src/recomp_rt/host/net_posix.c)
    target_include_directories(net_errors_test PRIVATE src/recomp_rt src/recomp_rt/win)
    target_link_libraries(net_errors_test PRIVATE Threads::Threads mbedtls)
    add_test(NAME net_errors COMMAND net_errors_test)
    add_executable(invite_test tests/net/test_invite.c src/recomp_rt/win/invite.c src/recomp_rt/win/lobby_rows.c
        src/recomp_rt/win/lobby_http.c src/recomp_rt/win/lobby.c src/recomp_rt/win/secure.c src/recomp_rt/host/net_posix.c)
    target_include_directories(invite_test PRIVATE src/recomp_rt src/recomp_rt/win)
    target_link_libraries(invite_test PRIVATE Threads::Threads mbedtls)
    add_test(NAME invite COMMAND invite_test)
    add_executable(link_test tests/net/test_link.c src/recomp_rt/host/link.c src/recomp_rt/host/posix.c)
    target_include_directories(link_test PRIVATE src/recomp_rt)
    add_test(NAME link COMMAND link_test)
    add_executable(link_pass_test tests/net/test_link_pass.c src/recomp_rt/host/link_pass.c)
    target_include_directories(link_pass_test PRIVATE src/recomp_rt)
    target_link_libraries(link_pass_test PRIVATE Threads::Threads)
    add_test(NAME link_pass COMMAND link_pass_test)
    # The zone of the heap for the player search (0.6.1, playersearch.c).
    add_executable(heap_big_test tests/recomp/heap_guard_stub.c tests/recomp/test_heap_big.c src/recomp_rt/heap.c)
    target_include_directories(heap_big_test PRIVATE src/recomp_rt)
    add_test(NAME heap_big COMMAND heap_big_test)

    add_executable(heap_zone_test tests/recomp/heap_guard_stub.c tests/recomp/test_heap_zone.c src/recomp_rt/heap.c)
    target_include_directories(heap_zone_test PRIVATE src/recomp_rt)
    add_test(NAME heap_zone COMMAND heap_zone_test)
    # Item 153 (0.6.2): the same-save runs that hold for the base game only (smoke_run.py, BASE_GAME_RUNS).
    add_test(NAME same_save_base_game COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/smoke/base_game_runs_check.py")
    add_test(NAME same_save_rules_pair_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/smoke/rules_pair_table_check.py")
    add_test(NAME seasons_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_db_table.py" --check)
    add_test(NAME season_credits COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/check_credits.py")
    # Issue 138 (0.7.3): the site roadmap shows Done only for issues closed as completed.
    add_test(NAME site_roadmap COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/site/roadmap_check.py")
    # Right ages and dates in every season (0.6.0 slice 11b): the C table matches the date shift tables.
    add_test(NAME season_dates_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_dates_table.py" --check)
    # Open item 233 (0.7.2): the year fix table matches docs/research/history-years/wrong-rows.tsv.
    add_test(NAME season_history_years_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_years_table.py" --check)
    # International competitions (0.6.0 slice 27): the C table of the editions matches the rules table, and
    # the editions and the rules of the code sites.
    add_test(NAME season_intl_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_intl.py" --check)
    add_executable(season_intl_test tests/seasons/test_season_intl.c src/recomp_rt/win/season_intl_rules.c
        src/recomp_rt/win/season_intl_table.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_intl_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_intl COMMAND season_intl_test)
    # 0.6.2 open item 103: the host fields that the original's fixed host rows leave unset read as no host.
    # The Asian Cup of 16 (0.6.2 item 180): the 2004 edition of a 2001 game keeps 3 stages at the new game and has 16
    # teams from its draw.
    add_executable(season_asian16_test tests/recomp/heap_top_stub.c tests/seasons/test_season_asian16.c src/recomp_rt/win/season_intl_cups.c
        src/recomp_rt/win/season_intl_rules.c src/recomp_rt/win/season_intl_table.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_asian16_test PRIVATE src/recomp_rt/win src/recomp_rt include)
    add_test(NAME season_asian16 COMMAND season_asian16_test)
    # GitHub issue 136: the Asian Cup entrants after the hosts are the best sides by reputation, in every game.
    add_executable(season_asian_pick_test tests/recomp/heap_top_stub.c tests/seasons/test_season_asian_pick.c src/recomp_rt/win/season_intl_cups.c
        src/recomp_rt/win/season_intl_rules.c src/recomp_rt/win/season_intl_table.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_asian_pick_test PRIVATE src/recomp_rt/win src/recomp_rt include)
    add_test(NAME season_asian_pick COMMAND season_asian_pick_test)
    add_executable(season_intl_rows_test tests/seasons/test_season_intl_rows.c src/recomp_rt/win/season_intl_rows.c)
    target_include_directories(season_intl_rows_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_intl_rows COMMAND season_intl_rows_test)
    # World Cup qualifying places with hosts (0.6.2 slice 5, open item 59): 48 teams for every host set.
    add_executable(season_wcq26_places_test tests/seasons/test_season_wcq26_places.c
        src/recomp_rt/win/season_wcq26_places.c)
    target_include_directories(season_wcq26_places_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_wcq26_places COMMAND season_wcq26_places_test)
    add_executable(season_caf_pool_test tests/seasons/test_season_caf_pool.c
        src/recomp_rt/win/season_caf_pool_plan.c)
    target_include_directories(season_caf_pool_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_caf_pool COMMAND season_caf_pool_test)
    # Nations Leagues (0.6.0 slice 28): the C table of the editions and leagues matches the tables, and the
    # ranking, promotion and relegation and play-off path rules.
    add_test(NAME season_nl_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_nl.py" --check)
    add_test(NAME season_uefa_dates_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_uefa_dates.py"
        --check)
    add_executable(season_uefa_dates_test tests/seasons/test_season_uefa_dates.c
        src/recomp_rt/win/season_uefa_dates_table.c src/recomp_rt/win/season_uefa_ko_lookup.c)
    target_include_directories(season_uefa_dates_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_uefa_dates COMMAND season_uefa_dates_test)
    add_executable(season_nl_test tests/seasons/test_season_nl.c src/recomp_rt/win/season_nl_rules.c
        src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_nl_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_nl COMMAND season_nl_test)
    # Domestic cups (0.6.0 slice 21): the round types, the cup table, and the C table matches cup_formats.tsv.
    add_executable(season_cups_test tests/seasons/test_season_cups.c src/recomp_rt/win/season_cups.c
        src/recomp_rt/win/season_cups_table.c)
    target_include_directories(season_cups_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cups COMMAND season_cups_test)
    add_test(NAME season_cups_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_cups_table.py" --check)
    # The rules table reader (0.6.0 slice 12): make the table from the parts, then read it.
    add_executable(season_rules_test tests/seasons/test_season_rules.c src/recomp_rt/win/season_rules.c
        src/recomp_rt/host/posix.c)
    target_include_directories(season_rules_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_rules_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_rules.py"
        --out "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_rules_table PROPERTIES FIXTURES_SETUP rules_table)
    add_test(NAME season_rules COMMAND season_rules_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv"
        "${CMAKE_BINARY_DIR}/seasons")
    set_tests_properties(season_rules PROPERTIES FIXTURES_REQUIRED rules_table)
    # Play-off winners that go up with no play-off of the game (0.6.2 open item 63): Série B from 2025 to 2301.
    add_executable(season_playoff_winners_test tests/seasons/test_season_playoff_winners.c
        src/recomp_rt/win/season_playoff_winners.c src/recomp_rt/win/season_rules.c
        src/recomp_rt/win/season_league_table.c src/recomp_rt/host/posix.c)
    target_include_directories(season_playoff_winners_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_playoff_winners COMMAND season_playoff_winners_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_playoff_winners PROPERTIES FIXTURES_REQUIRED rules_table)
    # The league structure of the rules table: meetings, halves, zones, periods, splits (0.7.2 league sizes, slice 1).
    add_executable(season_league_test tests/seasons/test_season_league.c src/recomp_rt/win/season_league.c
        src/recomp_rt/win/season_rules.c src/recomp_rt/win/season_league_table.c src/recomp_rt/host/posix.c)
    target_include_directories(season_league_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_league COMMAND season_league_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_league PROPERTIES FIXTURES_REQUIRED rules_table)
    # The meetings of the J1 and K League tables (0.7.2 league sizes, slice 14).
    add_executable(season_sizes_asia_test tests/seasons/test_season_sizes_asia.c
        src/recomp_rt/win/season_sizes_asia_rows.c src/recomp_rt/win/season_league.c src/recomp_rt/win/season_rules.c
        src/recomp_rt/win/season_league_table.c src/recomp_rt/host/posix.c)
    target_include_directories(season_sizes_asia_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_sizes_asia COMMAND season_sizes_asia_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_sizes_asia PROPERTIES FIXTURES_REQUIRED rules_table)
    # The places of Japan and Finland that the rules table decides (0.7.3 slice 4).
    add_executable(season_sizes_places_test tests/seasons/test_season_sizes_places.c
        src/recomp_rt/win/season_sizes_places.c src/recomp_rt/win/season_league.c src/recomp_rt/win/season_rules.c
        src/recomp_rt/win/season_league_table.c src/recomp_rt/host/posix.c)
    target_include_directories(season_sizes_places_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_sizes_places COMMAND season_sizes_places_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_sizes_places PROPERTIES FIXTURES_REQUIRED rules_table)
    # Domestic cups that the rules table does not hold in some seasons (0.6.0 slice 21b).
    add_executable(season_cup_breaks_test tests/seasons/test_season_cup_breaks.c
        src/recomp_rt/win/season_cup_breaks.c src/recomp_rt/win/season_rules.c src/recomp_rt/host/posix.c)
    target_include_directories(season_cup_breaks_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cup_breaks COMMAND season_cup_breaks_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_cup_breaks PROPERTIES FIXTURES_REQUIRED rules_table)
    # The date template of a league of any size (0.7.2 league sizes, slice 2, D4 and D5).
    add_executable(season_template_test tests/seasons/test_season_template.c src/recomp_rt/win/season_template.c)
    target_include_directories(season_template_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_template COMMAND season_template_test)
    # The periods of the Eerste Divisie and the Belgian Second Division at any size (0.7.2 league sizes, slice 10, D6).
    add_executable(season_periods_test tests/seasons/test_season_periods.c src/recomp_rt/win/season_periods.c)
    target_include_directories(season_periods_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_periods COMMAND season_periods_test)
    add_executable(season_ned_playoff_test tests/seasons/test_season_ned_playoff.c src/recomp_rt/win/season_ned_playoff_plan.c)
    target_include_directories(season_ned_playoff_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_ned_playoff COMMAND season_ned_playoff_test)
    add_executable(season_bel_plan_test tests/seasons/test_season_bel_plan.c src/recomp_rt/win/season_bel_plan.c)
    target_include_directories(season_bel_plan_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_bel_plan COMMAND season_bel_plan_test)
    # The Irish calendar move of 2003 (gap row 2 of 0.7.2 league sizes).
    add_executable(season_calendar_test tests/seasons/test_season_calendar.c src/recomp_rt/win/season_calendar.c
        src/recomp_rt/win/season_template.c)
    target_include_directories(season_calendar_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_calendar COMMAND season_calendar_test)
    add_test(NAME season_rules_readers COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/rules_readers_check.py")
    # The Italian rules of the league sizes (0.7.2 gap rows 30 and 32).
    add_executable(season_ita_rules_test tests/seasons/test_season_ita_rules.c src/recomp_rt/win/season_ita_rules.c)
    target_include_directories(season_ita_rules_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_ita_rules COMMAND season_ita_rules_test)
    # The MLS of real life: conferences, unbalanced schedule, play-off (0.7.2 gap row 7).
    add_executable(season_mls_plan_test tests/seasons/test_season_mls_plan.c src/recomp_rt/win/season_mls_plan.c)
    target_include_directories(season_mls_plan_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_mls_plan COMMAND season_mls_plan_test)
    # The Swedish third tier in two groups (issue 106).
    add_executable(season_swe_groups_test tests/seasons/test_season_swe_groups.c src/recomp_rt/win/season_swe_groups_plan.c)
    target_include_directories(season_swe_groups_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_swe_groups COMMAND season_swe_groups_test)
    add_executable(season_closed_test tests/seasons/test_season_closed.c src/recomp_rt/win/season_closed.c
                   src/recomp_rt/win/season_mls_plan.c)
    target_include_directories(season_closed_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_closed COMMAND season_closed_test)
    # The size step at a season change (0.7.2 league sizes, slice 3, D13).
    add_executable(season_size_step_test tests/seasons/test_season_size_step.c src/recomp_rt/win/season_size_step.c)
    target_include_directories(season_size_step_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_size_step COMMAND season_size_step_test)
    add_executable(season_cro_gre_places_test tests/seasons/test_season_cro_gre_places.c
                   src/recomp_rt/win/season_cro_gre_places.c)
    target_include_directories(season_cro_gre_places_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cro_gre_places COMMAND season_cro_gre_places_test)
    # The cups of eastern Europe and Asia follow the divisions (0.7.2 league sizes, slice 6).
    add_executable(season_cup_sizes_east_test tests/seasons/test_season_cup_sizes_east.c)
    target_include_directories(season_cup_sizes_east_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cup_sizes_east COMMAND season_cup_sizes_east_test)
    add_test(NAME cup_sizes_run_self COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/cup_sizes_run.py" --self-test)
    # Russia and Turkey at their real counts (0.7.2 league sizes, slice 12): the RusFirst take loop.
    add_executable(season_sizes_rus_test tests/seasons/test_season_sizes_rus.c)
    target_include_directories(season_sizes_rus_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_sizes_rus COMMAND season_sizes_rus_test)
    # Gap rows 19 and 20 of slice 6: no rules row or season database reaches the Croatian, Korean or Greek fallback.
    add_test(NAME cup_east_reach COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/cup_east_reach_check.py")
    set_tests_properties(cup_east_reach PROPERTIES SKIP_RETURN_CODE 77)
    # Level clubs under head-to-head (0.6.2 open item 167): the order of two finished level clubs stays open while a
    # club with matches left can still end on their points.
    add_executable(season_level_ties_test tests/seasons/test_season_level_ties.c src/recomp_rt/win/season_level_ties.c)
    target_include_directories(season_level_ties_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_level_ties COMMAND season_level_ties_test)
    # The cups of Britain and Ireland follow the divisions (0.7.2 league sizes, slice 4, D14): the Irish Cup, the
    # Welsh League Cup and the FA Cup take every club of a larger division, and the counts of the League Cup and the
    # Scottish cups.
    add_executable(season_cup_sizes_uk_test tests/recomp/heap_top_stub.c tests/seasons/test_season_cup_sizes_uk.c
        src/recomp_rt/win/season_cup_sizes_uk.c)
    target_include_directories(season_cup_sizes_uk_test PRIVATE src/recomp_rt/win src/recomp_rt include)
    add_test(NAME season_cup_sizes_uk COMMAND season_cup_sizes_uk_test)
    # The rest of the domestic cups (0.6.0 slice 21c): four-club super cups, group stages and new cups.
    add_executable(season_cups_more_test tests/seasons/test_season_cups_more.c
        src/recomp_rt/win/season_cups_more.c src/recomp_rt/win/season_rules.c src/recomp_rt/host/posix.c)
    target_include_directories(season_cups_more_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cups_more COMMAND season_cups_more_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_cups_more PROPERTIES FIXTURES_REQUIRED rules_table)
    # Titles taken away after the season (0.6.2 item 108): the holder of a start season before the ruling.
    add_executable(season_titles_test tests/seasons/test_season_titles.c src/recomp_rt/win/season_titles.c)
    target_include_directories(season_titles_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_titles COMMAND season_titles_test)
    # The new competitions module (slice 21c review): the defs that may need a new record fit MAX_NEW.
    add_test(NAME season_comps_new_records COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/comps_new_records_check.py"
        "${CMAKE_SOURCE_DIR}/src/recomp_rt/win")
    # The names table and the club moves table (0.6.0 slice 15): the tools read every part and stop on a bad row.
    add_test(NAME season_names_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_names.py" --check)
    # 0.6.1 item 2: the abbreviation column of the names table (Select Team shows it).
    add_test(NAME season_names_abbr COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/names_abbr_check.py")
    # 0.6.2 open item 71: after the table each file keeps the names of its last season with a name, to 2301.
    add_executable(season_names_test tests/recomp/heap_top_stub.c tests/seasons/test_season_names.c src/recomp_rt/win/season_names.c
        src/recomp_rt/host/posix.c)
    target_include_directories(season_names_test PRIVATE src/recomp_rt/win src/recomp_rt include)
    add_test(NAME season_names COMMAND season_names_test "${CMAKE_BINARY_DIR}/seasons/names.tsv" "${CMAKE_BINARY_DIR}/seasons")
    # Future testing step 2: the history checker finds each planted fault of a small fixture save that it writes.
    # The steps of the match check (round AA, item 155): MATCH_EVENT only for a build with match_numbers.
    add_test(NAME smoke_match_steps COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/smoke/match_steps_test.py")
    add_test(NAME season_future_history_check COMMAND python3 -I
        "${CMAKE_SOURCE_DIR}/tests/seasons/future/history_check_test.py")
    add_test(NAME season_clubs_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_clubs.py" --check)
    # The club ids of the clubs files (O151): each row names a club record of its season's database. The check reads
    # the databases of test-fixtures and skips (code 77) where they are missing.
    add_test(NAME season_club_ids COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/club_ids_check.py")
    set_tests_properties(season_club_ids PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME season_club_records_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_club_records.py" --check)
    # The UEFA formats by season (0.6.0 slice 22): the committed table matches uefa.tsv, and its lookups.
    add_test(NAME season_uefa_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_uefa_table.py" --check)
    add_executable(season_uefa_test tests/seasons/test_season_uefa.c src/recomp_rt/win/season_uefa_table.c
        src/recomp_rt/win/season_uefa_lookup.c)
    target_include_directories(season_uefa_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_uefa COMMAND season_uefa_test)
    # Row A18: the UEFA group and league phase days of every running season against the FIFA days.
    add_executable(season_uefa_days_test tests/seasons/test_season_uefa_days.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c src/recomp_rt/win/season_uefa_table.c
        src/recomp_rt/win/season_uefa_lookup.c src/recomp_rt/win/season_uefa_dates_table.c
        src/recomp_rt/win/season_uefa_ko_lookup.c)
    target_include_directories(season_uefa_days_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_uefa_days COMMAND season_uefa_days_test)
    # The FIFA days after the table (0.6.2 slice 14, fault 16): the same days as the year by year steps, to 2300.
    add_executable(season_fifa_days_test tests/seasons/test_season_fifa_days.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_fifa_days_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_fifa_days COMMAND season_fifa_days_test)
    # The day rule of the test switch RECOMP_TEST_MARKET_WEEKLY (open item 193).
    add_executable(market_weekly_test tests/seasons/test_market_weekly.c src/recomp_rt/win/market_weekly_day.c)
    target_include_directories(market_weekly_test PRIVATE src/recomp_rt/win)
    add_test(NAME market_weekly COMMAND market_weekly_test)
    # Open item 163: the South American qualifying rounds on the FIFA windows of each cycle, 2006 to 2302.
    add_executable(season_conmebol_days_test tests/seasons/test_season_conmebol_days.c
        src/recomp_rt/win/season_conmebol_days.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_conmebol_days_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_conmebol_days COMMAND season_conmebol_days_test)
    # Item 196: the AFC World Cup qualifying first round (who plays and the days), pools of 38 to 50, 2006 to 2302.
    add_executable(season_afc_first_test tests/seasons/test_season_afc_first.c
        src/recomp_rt/win/season_afc_first_plan.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_afc_first_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_afc_first COMMAND season_afc_first_test)
    # Items 203 and 204: a qualifying stage's days planned in round order on the FIFA days.
    add_executable(season_stage_plan_test tests/seasons/test_season_stage_plan.c
        src/recomp_rt/win/season_stage_plan.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c src/recomp_rt/win/season_conmebol_days.c)
    target_include_directories(season_stage_plan_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_stage_plan COMMAND season_stage_plan_test)
    # Item 207: both legs of a qualifying tie on the FIFA days of one window, in order.
    add_executable(season_tie_legs_test tests/seasons/test_season_tie_legs.c
        src/recomp_rt/win/season_tie_legs.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_tie_legs_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_tie_legs COMMAND season_tie_legs_test)
    add_executable(season_concacaf26_days_test tests/seasons/test_season_concacaf26_days.c
        src/recomp_rt/win/season_concacaf26_days.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_concacaf26_days_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_concacaf26_days COMMAND season_concacaf26_days_test)
    add_executable(youth_pool_test tests/seasons/test_youth_pool.c src/recomp_rt/win/youth_pool_rule.c)
    target_include_directories(youth_pool_test PRIVATE src/recomp_rt/win)
    add_test(NAME youth_pool COMMAND youth_pool_test)
    # Item 198: the OFC World Cup qualifying preliminary round (who plays and the days), 2005 to 2302.
    add_executable(season_ofc_first_test tests/seasons/test_season_ofc_first.c
        src/recomp_rt/win/season_ofc_first_plan.c src/recomp_rt/win/season_uefa_days.c
        src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_ofc_first_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_ofc_first COMMAND season_ofc_first_test)
    # The UEFA league phase draw (0.6.0 slice 25): pots, opponents, nation rules and match days.
    add_executable(season_uefa_league_test tests/seasons/test_season_uefa_league.c
        src/recomp_rt/win/season_uefa_league_draw.c)
    target_include_directories(season_uefa_league_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_uefa_league COMMAND season_uefa_league_test)
    # The real round names (Q370): the committed table matches round-names.tsv, and its lookup.
    add_test(NAME season_round_names_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_round_names.py" --check)
    add_executable(season_round_names_test tests/seasons/test_season_round_names.c
        src/recomp_rt/win/season_round_names.c src/recomp_rt/win/season_round_names_table.c)
    target_include_directories(season_round_names_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_round_names COMMAND season_round_names_test)
    # The state of a Brazilian club (0.6.0 slice 20e, Q600): the committed table matches bra-city-states.tsv, and its
    # lookup.
    add_test(NAME season_bra_states_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_bra_states.py" --check)
    add_executable(season_bra_states_test tests/seasons/test_season_bra_states.c
        src/recomp_rt/win/season_bra_names.c src/recomp_rt/win/season_bra_states_table.c)
    target_include_directories(season_bra_states_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_bra_states COMMAND season_bra_states_test)
    add_executable(season_history_start_test tests/seasons/test_season_history_start.c
        src/recomp_rt/win/season_history_start.c)
    target_include_directories(season_history_start_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_history_start COMMAND season_history_start_test)
    # The zone fill table of the Argentine clubs (O259 net B2, O375): the committed table matches zones.tsv, its lookup,
    # and every club of the second division has a zone record or a row in each start (skips without the databases).
    add_test(NAME season_arg_zones_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_arg_zones.py" --check)
    add_executable(season_arg_zones_test tests/seasons/test_season_arg_zones.c
        src/recomp_rt/win/season_arg_zones.c src/recomp_rt/win/season_arg_zones_table.c)
    target_include_directories(season_arg_zones_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_arg_zones COMMAND season_arg_zones_test)
    add_test(NAME season_arg_zones_check COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/arg_zones_check.py")
    set_tests_properties(season_arg_zones_check PROPERTIES SKIP_RETURN_CODE 77)
    # The substitute rule lookup (0.6.0 slice 17).
    add_executable(season_subs_test tests/seasons/test_season_subs.c src/recomp_rt/win/season_subs_rule.c
        src/recomp_rt/win/season_rules.c src/recomp_rt/host/posix.c)
    target_include_directories(season_subs_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_subs COMMAND season_subs_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_subs PROPERTIES FIXTURES_REQUIRED rules_table)
    # The club cup editions after the rules table (0.6.2 slice 9a): every year to 2301, the Club World Cup every 4.
    add_executable(season_club_editions_test tests/seasons/test_season_club_editions.c
        src/recomp_rt/win/season_club_editions.c src/recomp_rt/win/season_rules.c src/recomp_rt/host/posix.c
        src/recomp_rt/win/season_uefa_days.c src/recomp_rt/win/season_fifa_days_table.c)
    target_include_directories(season_club_editions_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_club_editions COMMAND season_club_editions_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_club_editions PROPERTIES FIXTURES_REQUIRED rules_table)
    # The whole season part of a save (0.6.1 items 4 and 10): its write keeps inside its size.
    add_executable(season_part_test tests/recomp/heap_top_stub.c tests/seasons/test_season_part.c
        src/recomp_rt/win/season_change.c src/recomp_rt/win/season_friendly_dates.c src/recomp_rt/win/season_years.c
        src/recomp_rt/win/season_stage_plan.c src/recomp_rt/win/season_tie_legs.c
        src/recomp_rt/win/caps_wide.c src/recomp_rt/win/season_intl_marks.c
        src/recomp_rt/win/season_nl_rules.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_part_test PRIVATE src/recomp_rt/win src/recomp_rt)
    add_test(NAME season_part COMMAND season_part_test)
    # The key competition check of a load (0.6.1 item 10): the names of the player's cm0102.exe and its Data.
    add_executable(season_load_check_test tests/seasons/test_season_load_check.c
        src/recomp_rt/win/season_load_check.c src/cm3/key_comp_names.c src/cm3/key_ids_table.c)
    target_include_directories(season_load_check_test PRIVATE src/recomp_rt/win include)
    get_filename_component(CM0102_GAME_DIR "${CM0102_EXE}" DIRECTORY)
    add_test(NAME season_load_check COMMAND season_load_check_test "${CM0102_EXE}" "${CM0102_GAME_DIR}/Data")
    # The nation rules (0.6.0 slice 17): windows, foreign player limits and the work permit rule. The encoder reads
    # the research parts; its outputs are committed and checked here; the lookup runs on the C table.
    add_executable(season_nation_test tests/seasons/test_season_nation.c src/recomp_rt/win/season_nation.c
        src/recomp_rt/win/season_nation_table.c)
    target_include_directories(season_nation_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_nation COMMAND season_nation_test)
    # The row A18 list of the season part (0.6.1 item 4): its write keeps inside its size.
    add_executable(season_friendly_part_test tests/recomp/heap_top_stub.c tests/seasons/test_season_friendly_part.c
        src/recomp_rt/win/season_friendly_dates.c src/recomp_rt/win/season_stage_plan.c
        src/recomp_rt/win/season_tie_legs.c
        src/recomp_rt/win/season_nl_rules.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_friendly_part_test PRIVATE src/recomp_rt/win src/recomp_rt)
    add_test(NAME season_friendly_part COMMAND season_friendly_part_test)
    # The days a qualifier may take (0.6.2 item 168): only a national side of its nation takes the day.
    add_executable(season_side_busy_test tests/recomp/heap_top_stub.c tests/seasons/test_season_side_busy.c
        src/recomp_rt/win/season_friendly_dates.c src/recomp_rt/win/season_stage_plan.c
        src/recomp_rt/win/season_tie_legs.c
        src/recomp_rt/win/season_nl_rules.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_side_busy_test PRIVATE src/recomp_rt/win src/recomp_rt)
    add_test(NAME season_side_busy COMMAND season_side_busy_test)
    # Referee ranges outside the officials table (report of 25 November 2026): the picker's test and the repair.
    add_executable(referees_test tests/recomp/heap_top_stub.c tests/recomp/test_referees.c src/recomp_rt/win/referees.c)
    target_include_directories(referees_test PRIVATE src/recomp_rt/win src/recomp_rt)
    add_test(NAME referees COMMAND referees_test)
    # The snap of a qualifier to a FIFA day (open item 79): no two matches of a side on one day.
    add_executable(season_fifa_snap_test tests/recomp/heap_top_stub.c tests/seasons/test_season_fifa_snap.c src/recomp_rt/win/season_friendly_dates.c
        src/recomp_rt/win/season_stage_plan.c src/recomp_rt/win/season_tie_legs.c
        src/recomp_rt/win/season_nl_rules.c src/recomp_rt/win/season_nl_table.c)
    target_include_directories(season_fifa_snap_test PRIVATE src/recomp_rt/win src/recomp_rt)
    add_test(NAME season_fifa_snap COMMAND season_fifa_snap_test)
    add_test(NAME season_encode_rules COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/test_encode_rules.py")
    # The league rules (0.6.0 slice 19): play-off places, splits and calendars. The outputs are committed.
    add_test(NAME season_encode_leagues COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/test_encode_leagues.py")
    # Games to 2300 (future testing, step 1): every season function that reads no game memory, from 2001 to 2300, with
    # the days round 28 February 2100, 2200 and 2300. Known faults: tests/seasons/future/known_faults.tsv.
    add_executable(future_2300_test tests/seasons/future/test_future_2300.c
        src/recomp_rt/win/season_intl_rules.c src/recomp_rt/win/season_intl_table.c
        src/recomp_rt/win/season_nl_rules.c src/recomp_rt/win/season_nl_table.c
        src/recomp_rt/win/season_uefa_table.c src/recomp_rt/win/season_uefa_lookup.c
        src/recomp_rt/win/season_uefa_ko_lookup.c src/recomp_rt/win/season_uefa_dates_table.c
        src/recomp_rt/win/season_uefa_days.c src/recomp_rt/win/season_fifa_days_table.c
        src/recomp_rt/win/season_cups.c src/recomp_rt/win/season_cups_table.c src/si/date.c
        src/recomp_rt/win/season_years.c)
    target_include_directories(future_2300_test PRIVATE src/recomp_rt/win include)
    target_compile_options(future_2300_test PRIVATE -O2)    # 300 years of days: seconds, not half a minute
    add_test(NAME future_2300 COMMAND future_2300_test "${CMAKE_SOURCE_DIR}/tests/seasons/future/known_faults.tsv")
    # The expected rule of every competition (the input of the history checker): its columns and values.
    add_test(NAME future_competitions COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/future/competitions_check.py")
    # The run set and its release check (0.6.2 slice 15), without a game: tests/seasons/future/run_set_test.py.
    add_test(NAME future_run_set COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/future/run_set_test.py")
    # The speed benchmark (0.8.0 slice 1), without a game: tests/seasons/speed_bench_test.py.
    add_test(NAME speed_bench COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/speed_bench_test.py")
    # The network scripts watch the address their games meet on (item 216): tests/smoke/address_watch_test.py.
    add_test(NAME address_watch COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/smoke/address_watch_test.py")
    # A rules report run with --year and no --database plays the season database of its expected files (item 129).
    add_test(NAME rules_report_database COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/rules_report_database_check.py")
    # The club_list check of the rules report prints its lines (0.6.2 open item 142).
    add_test(NAME rules_report_club_list COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/rules_report_club_list_check.py")
    # A play-off between two divisions: the rows count the direct places only (0.6.2 open item 259).
    add_test(NAME rules_report_cross_playoff COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/rules_report_cross_playoff_check.py")
    add_executable(winfit_test tests/winfit/test_winfit.c)
    target_include_directories(winfit_test PRIVATE src/recomp_rt/win)
    add_test(NAME winfit COMMAND winfit_test)
    add_executable(winscale_test tests/winscale/test_winscale.c)
    target_include_directories(winscale_test PRIVATE src/recomp_rt/win)
    add_test(NAME winscale COMMAND winscale_test)
    add_executable(press_hold_test tests/winscale/test_press_hold.c)
    target_include_directories(press_hold_test PRIVATE src/recomp_rt/win)
    add_test(NAME press_hold COMMAND press_hold_test)
    add_executable(savename_test tests/savename/test_cp1252.c)
    target_include_directories(savename_test PRIVATE src/recomp_rt/win)
    add_test(NAME savename COMMAND savename_test)
    # File name parts that are not UTF-8 (0.6.0 slice 15): win_host_path turns them into UTF-8.
    add_executable(path_utf8_test tests/pathutf8/test_path_utf8.c)
    target_include_directories(path_utf8_test PRIVATE src/recomp_rt/win)
    add_test(NAME path_utf8 COMMAND path_utf8_test)
    add_executable(draw_test tests/recomp/heap_top_stub.c tests/draw/test_draw.c src/recomp_rt/win/draw.c)
    if(NOT APPLE)
        target_link_libraries(draw_test PRIVATE m)
    endif()
    target_include_directories(draw_test PRIVATE src/recomp_rt/win)
    add_test(NAME draw COMMAND draw_test)
    add_executable(pickscore_test tests/picker/test_pick_score.c)
    target_include_directories(pickscore_test PRIVATE src/recomp_rt/win)
    add_test(NAME pickscore COMMAND pickscore_test)
    add_executable(pdf_test tests/pdfprint/test_pdf.c src/recomp_rt/win/pdf.c)
    target_include_directories(pdf_test PRIVATE src/recomp_rt/win)
    add_test(NAME pdf COMMAND pdf_test)
    add_executable(groups_test tests/groups/test_groups.c src/recomp_rt/win/groups_layout.c)
    target_include_directories(groups_test PRIVATE src/recomp_rt/win)
    add_test(NAME groups COMMAND groups_test)
    add_executable(compare_test tests/compare/test_compare.c src/recomp_rt/win/compare_layout.c
        src/recomp_rt/win/groups_layout.c)
    target_include_directories(compare_test PRIVATE src/recomp_rt/win)
    target_link_libraries(compare_test PRIVATE m)
    add_test(NAME compare COMMAND compare_test)
endif()
endif()  # NOT EMSCRIPTEN


# The recompiled original (docs/specs/2026-10-14-recompile-spec.md). tools/recomp translates the
# player's own cm0102.exe into C in the build folder; that C is a build product and stays out of git.
# By default the first cm0102.exe in a folder above the repo (a git worktree sits deeper).
set(CM0102_EXE_DEFAULT "${CMAKE_SOURCE_DIR}/../cm0102.exe")
set(dir "${CMAKE_SOURCE_DIR}")
foreach(level RANGE 6)
    get_filename_component(dir "${dir}" DIRECTORY)
    if(EXISTS "${dir}/cm0102.exe")
        set(CM0102_EXE_DEFAULT "${dir}/cm0102.exe")
        break()
    endif()
endforeach()
set(CM0102_EXE "${CM0102_EXE_DEFAULT}" CACHE FILEPATH "The player's cm0102.exe")
if(EXISTS "$ENV{HOME}/.venvs/cm0102/bin/python")
    set(RECOMP_PYTHON_DEFAULT "$ENV{HOME}/.venvs/cm0102/bin/python")
else()
    set(RECOMP_PYTHON_DEFAULT "python3")
endif()
set(RECOMP_PYTHON "${RECOMP_PYTHON_DEFAULT}" CACHE FILEPATH "A Python with capstone, for tools/recomp")
# Competition history between 2001 and the start season (0.6.0 slice 11c): the C table matches plan.tsv and
# every row is valid. A Python test, so it runs on every platform.
enable_testing()
add_test(NAME season_history_table COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_table.py" --check)
add_test(NAME season_history_rows_table COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_rows_table.py" --check)
# 0.6.2 open item 106: the C table of the real UEFA season coefficients matches its TSV and the researched values.
add_test(NAME season_coeffs_table COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_coeff_table.py" --check)
# The .dmg window background shows the logo art alone, with no drawn BOSMAN EDITION line under it (0.6.1
# item 1). Needs Pillow, so a skip (77) without it.
add_test(NAME dmg_background COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tests/mac/dmg_background_check.py")
set_tests_properties(dmg_background PROPERTIES SKIP_RETURN_CODE 77)

if(EXISTS "${CM0102_EXE}")
    set(RECOMP_SHARDS 128)
    set(RECOMP_OUT "${CMAKE_BINARY_DIR}/recomp")
    set(RECOMP_GEN "${RECOMP_OUT}/gen/table.c" "${RECOMP_OUT}/gen/functions.h")
    math(EXPR RECOMP_LAST "${RECOMP_SHARDS} - 1")
    foreach(i RANGE ${RECOMP_LAST})
        string(LENGTH "${i}" n)
        if(n EQUAL 1)
            set(i "00${i}")
        elseif(n EQUAL 2)
            set(i "0${i}")
        endif()
        list(APPEND RECOMP_GEN "${RECOMP_OUT}/gen/fn_${i}.c")
    endforeach()
    file(GLOB RECOMP_WIN CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/recomp_rt/win/*.c")
    file(GLOB RECOMP_TOOL CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/recomp/*.py")
    set(RECOMP_RUNTIME_ARGS "")
    foreach(f ${RECOMP_WIN})
        list(APPEND RECOMP_RUNTIME_ARGS --runtime "${f}")
    endforeach()
    add_custom_command(
        OUTPUT "${RECOMP_OUT}/recomp.stamp"
        BYPRODUCTS ${RECOMP_GEN}
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/recomp/recomp.py"
            --exe "${CM0102_EXE}" --out "${RECOMP_OUT}"
            --functions "${CMAKE_SOURCE_DIR}/tools/recomp/functions.txt" --all --entries 0x947a13
            --shards ${RECOMP_SHARDS} ${RECOMP_RUNTIME_ARGS} --stamp "${RECOMP_OUT}/recomp.stamp"
        DEPENDS ${RECOMP_TOOL} "${CMAKE_SOURCE_DIR}/tools/recomp/functions.txt" ${RECOMP_WIN}
            "${CMAKE_SOURCE_DIR}/src/recomp_rt/rt.c" "${CMAKE_SOURCE_DIR}/src/recomp_rt/heap.c"
        COMMENT "Recompiling cm0102.exe to C"
        VERBATIM)
    add_custom_target(recomp_gen DEPENDS "${RECOMP_OUT}/recomp.stamp")

    # The host functions (src/recomp_rt/host.h): Windows when cross-compiled with mingw-w64, else POSIX.
    if(WIN32)
        set(RECOMP_HOST src/recomp_rt/host/win32.c src/recomp_rt/host/net_win32.c)
    elseif(EMSCRIPTEN)
        # The browser (web/): the POSIX host, the page's calls (web.c) and the sockets (a page opens none: the
        # online games are not offered there).
        set(RECOMP_HOST src/recomp_rt/host/posix.c src/recomp_rt/host/web.c src/recomp_rt/host/net_posix.c)
    else()
        set(RECOMP_HOST src/recomp_rt/host/posix.c src/recomp_rt/host/net_posix.c)
    endif()
    add_executable(cm0102_recomp ${RECOMP_GEN}
        src/recomp_rt/rt.c
        src/recomp_rt/heap.c
        ${RECOMP_HOST}
        src/recomp_rt/host/link.c
        src/recomp_rt/host/link_pass.c
        ${RECOMP_WIN}
        src/cm3/key_comp_names.c
        src/cm3/key_ids_table.c
    )
    # The key table of set_key_ids for the load check (season_load_check.c): the port's headers for these two only.
    set_source_files_properties(src/cm3/key_comp_names.c src/cm3/key_ids_table.c TARGET_DIRECTORY cm0102_recomp
        PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/include")
    add_dependencies(cm0102_recomp recomp_gen)
    # The host qsort of the player history lists (qsort6.c) sorts as the recompiled qsort 0x945333 does,
    # rows and eax, so "Player histories" is faster and the game stays the same (rule Q152). The same run
    # checks the host _ftol (crt.c) and the cache of calls through an address (rt.c) (0.6.2 item 110).
    get_filename_component(CM0102_EXE_DIR "${CM0102_EXE}" DIRECTORY)
    add_test(NAME qsort6 COMMAND cm0102_recomp --game "${CM0102_EXE_DIR}")
    set_tests_properties(qsort6 PROPERTIES ENVIRONMENT
        "RECOMP_QSORT_TEST=1;SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;RECOMP_NO_BOX=1;RECOMP_USER_DIR=${CMAKE_BINARY_DIR}/qsort6-user")
    # Reads of the original that depend on where memory lies (0.6.2 open item 138b, layout_reads.c): the original's
    # match player constructor gives the same record in blocks that held other bytes. With the fix off
    # (RECOMP_LAYOUT_FIXES=keep:players, the original's code) the records differ, so layout_reads_kept must fail.
    add_test(NAME layout_reads COMMAND cm0102_recomp --game "${CM0102_EXE_DIR}")
    set_tests_properties(layout_reads PROPERTIES ENVIRONMENT
        "RECOMP_LAYOUT_TEST=1;SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;RECOMP_NO_BOX=1;RECOMP_USER_DIR=${CMAKE_BINARY_DIR}/layout-user")
    add_test(NAME layout_reads_kept COMMAND cm0102_recomp --game "${CM0102_EXE_DIR}")
    set_tests_properties(layout_reads_kept PROPERTIES WILL_FAIL TRUE ENVIRONMENT
        "RECOMP_LAYOUT_TEST=1;RECOMP_LAYOUT_FIXES=keep:players;SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;RECOMP_NO_BOX=1;RECOMP_USER_DIR=${CMAKE_BINARY_DIR}/layout-kept-user")
    # The switch knows every group: three named groups give no warning, a name it does not know gives one.
    add_test(NAME layout_reads_names COMMAND cm0102_recomp --game "${CM0102_EXE_DIR}")
    set_tests_properties(layout_reads_names PROPERTIES
        PASS_REGULAR_EXPRESSION "unknown group bogus"
        FAIL_REGULAR_EXPRESSION "unknown group (captain|players|daily|stats|loans|squads|officials|training|contracts|schedules|fitness)"
        ENVIRONMENT "RECOMP_LAYOUT_TEST=1;RECOMP_LAYOUT_FIXES=keep:stats,loans,squads,officials,training,contracts,schedules,fitness,bogus;SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;RECOMP_NO_BOX=1;RECOMP_USER_DIR=${CMAKE_BINARY_DIR}/layout-names-user")
    add_test(NAME layout_reads_check COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tests/recomp/layout_reads_check.py"
        --exe "${CM0102_EXE}")
    # 0.6.0 slice 11c: the build stops when the history table is not the one plan.tsv gives, or a row is not valid.
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/history.ok"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_table.py" --check
            --stamp "${CMAKE_BINARY_DIR}/seasons/history.ok"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_table.py"
            "${CMAKE_SOURCE_DIR}/docs/research/history-0.6.0/plan.tsv"
            "${CMAKE_SOURCE_DIR}/docs/research/history-0.6.0/plan-official.tsv"
            "${CMAKE_SOURCE_DIR}/docs/research/history-0.6.0/plan-fixes.tsv"
            "${CMAKE_SOURCE_DIR}/src/recomp_rt/win/season_history_table.c"
        COMMENT "Checking the competition history table"
        VERBATIM)
    add_custom_target(season_history_check DEPENDS "${CMAKE_BINARY_DIR}/seasons/history.ok")
    add_dependencies(cm0102_recomp season_history_check)
    # 0.6.0 slice 12: the competition classes of the rules report (from the exe, not committed) and the
    # rules table of the game (Seasons/rules.tsv of the download; tests read it with RECOMP_RULES_TABLE).
    file(GLOB RULES_PARTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/docs/research/rules-0.6.0/*.tsv")
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/comp_classes.c"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_comp_classes.py"
            --exe "${CM0102_EXE}" --out "${CMAKE_BINARY_DIR}/seasons/comp_classes.c"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_comp_classes.py" "${CMAKE_SOURCE_DIR}/tools/recomp/pe.py"
            "${CMAKE_SOURCE_DIR}/docs/research/function-map/names.tsv"
        COMMENT "Finding the competition classes of cm0102.exe"
        VERBATIM)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/rules.tsv"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_rules.py"
            --out "${CMAKE_BINARY_DIR}/seasons/rules.tsv"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_rules.py" ${RULES_PARTS}
        COMMENT "Making the rules table"
        VERBATIM)
    # 0.6.0 slice 15: the names table (Seasons/names.tsv) and the club moves table (Seasons/clubs.tsv).
    file(GLOB CLUBS_PARTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/docs/research/rules-0.6.0/*-clubs.tsv")
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/names.tsv"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_names.py"
            --out "${CMAKE_BINARY_DIR}/seasons/names.tsv"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_names.py" "${CMAKE_SOURCE_DIR}/tools/seasons/make_rules.py"
            ${RULES_PARTS}
        COMMENT "Making the names table"
        VERBATIM)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/clubs.tsv"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_clubs.py"
            --out "${CMAKE_BINARY_DIR}/seasons/clubs.tsv"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_clubs.py" "${CMAKE_SOURCE_DIR}/tools/seasons/make_rules.py"
            ${RULES_PARTS} ${CLUBS_PARTS}
        COMMENT "Making the club moves table"
        VERBATIM)
    # 0.6.0 slice 15b: the club records table (Seasons/club_records.tsv).
    file(GLOB CLUB_RECORDS_PARTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/docs/research/club-records-0.6.0/*.tsv")
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/club_records.tsv"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_club_records.py"
            --out "${CMAKE_BINARY_DIR}/seasons/club_records.tsv"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_club_records.py" "${CMAKE_SOURCE_DIR}/tools/seasons/make_clubs.py"
            ${RULES_PARTS} ${CLUBS_PARTS} ${CLUB_RECORDS_PARTS}
        COMMENT "Making the club records table"
        VERBATIM)
    add_custom_target(seasons_rules ALL DEPENDS "${CMAKE_BINARY_DIR}/seasons/rules.tsv"
        "${CMAKE_BINARY_DIR}/seasons/names.tsv" "${CMAKE_BINARY_DIR}/seasons/clubs.tsv"
        "${CMAKE_BINARY_DIR}/seasons/club_records.tsv")
    target_sources(cm0102_recomp PRIVATE "${CMAKE_BINARY_DIR}/seasons/comp_classes.c")
    add_dependencies(cm0102_recomp seasons_rules)
    # The season change (0.6.0 slice 16): the committed wraps of start_new_season match the class table.
    add_test(NAME season_starts COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_start_wraps.py"
        --exe "${CM0102_EXE}" --classes "${CMAKE_BINARY_DIR}/seasons/comp_classes.c" --check)
    # League sizes (0.6.0 slice 18): the committed wraps of set_up_rules match tools/recomp/league_sizes.py.
    add_test(NAME season_size_wraps COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_size_wraps.py" --check)
    # The checks of the league size runs (0.7.2 league sizes, slice 2) on made-up templates.
    add_test(NAME league_size_run_self_test COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/league_size_run.py"
        --self-test)
    # Gap 47: every K League count change is a new club or a K League 2 move, played with no division (no K League 2 class).
    add_test(NAME kor_count_changes COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/kor_count_changes_check.py"
        "${CMAKE_BINARY_DIR}")
    # The Argentine Primera (0.7.2 league sizes, slice 15): the fixture plans of the zones and the derby round, and the
    # committed table of the zones, rivals and derbies of the research.
    add_executable(arg_prm_plan_test tests/seasons/test_arg_prm_plan.c src/recomp_rt/win/season_arg_plan.c)
    target_include_directories(arg_prm_plan_test PRIVATE src/recomp_rt/win)
    add_test(NAME arg_prm_plan COMMAND arg_prm_plan_test)
    add_test(NAME arg_prm_zones COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_arg_prm_zones.py" --check)
    # The checks of the size step runs (slice 3) and of the English exchange (slice 8, D15) on made-up logs.
    add_test(NAME size_step_run_self_test COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/size_step_run.py"
        --self-test)
    add_test(NAME over_four_run_self_test COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/over_four_run.py"
        --self-test)
    # The cups of western Europe take the real divisions (0.7.2 league sizes, slice 5): the counts with grown and
    # shrunk divisions on a made-up club table, and the run list of the season runs (tests/seasons/cup_size_run.py).
    add_executable(season_cup_west_test tests/recomp/heap_top_stub.c tests/seasons/test_season_cup_west.c src/recomp_rt/win/season_cup_sizes_west.c)
    target_include_directories(season_cup_west_test PRIVATE src/recomp_rt/win src/recomp_rt include)
    add_test(NAME season_cup_west COMMAND season_cup_west_test)
    add_test(NAME cup_size_run_list COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/cup_size_run.py" --list)
    # Cup lists (0.6.2 item 64): every cup list of the original starts cleared in a season game.
    add_test(NAME cup_list_clear COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tests/seasons/cup_list_clear_check.py"
        --exe "${CM0102_EXE}")
    target_include_directories(cm0102_recomp PRIVATE src/recomp_rt src/recomp_rt/win "${RECOMP_OUT}/gen"
        ${SDL2_INCLUDE_DIRS})
    target_compile_definitions(cm0102_recomp PRIVATE CM0102_DEFAULT_EXE="${CM0102_EXE}"
        CM0102_VERSION="${PROJECT_VERSION}")
    # The generated C: exact integer wrap, no strict aliasing, no warnings (it is not read by people).
    set_source_files_properties(${RECOMP_GEN} PROPERTIES GENERATED TRUE COMPILE_OPTIONS "-O1;-w")
    target_compile_options(cm0102_recomp PRIVATE -fno-strict-aliasing -fwrapv -O1)
    # ThinLTO on the Mac (0.6.2 item 110): the linker can inline small functions of the original into their
    # callers across the generated files (the CRT float to integer call, the random function, getters), which
    # made a game day about 9 % faster. The game stays the same (same-save set). The cache keeps a relink short.
    if(APPLE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
        option(CM0102_LTO "ThinLTO for the game on the Mac" ON)
        if(CM0102_LTO)
            target_compile_options(cm0102_recomp PRIVATE -flto=thin)
            target_link_options(cm0102_recomp PRIVATE -flto=thin "LINKER:-cache_path_lto,${CMAKE_BINARY_DIR}/lto-cache")
        endif()
    endif()
    # Every call of the original is a host call: give the main thread a deep stack.
    if(WIN32)
        include(packaging/windows/windows.cmake)
    elseif(EMSCRIPTEN)
        include(web/web.cmake)
    else()
        if(APPLE)
            target_link_options(cm0102_recomp PRIVATE "LINKER:-stack_size,0x10000000")
        endif()
        find_package(Threads REQUIRED)
        target_link_libraries(cm0102_recomp PRIVATE ${SDL2_LIBRARIES} m Threads::Threads)
    endif()
    # The secure connection of online play (src/recomp_rt/win/secure.c).
    target_link_libraries(cm0102_recomp PRIVATE mbedtls)
else()
    message(STATUS "cm0102_recomp: ${CM0102_EXE} not found, so the recompiled game is not built")
endif()

# The Mac download (docs/specs/2026-10-14-re-signed-edition-release-spec.md, slice 5).
#   cmake -B build -DCM0102_GAME_DIR=<clean files folder> [-DCM0102_ICNS=<icon.icns>] ...
#   cmake --build build --target mac_dmg
# tools/mac/make_app.sh builds the app for arm64 and x86_64, joins them with lipo, bundles SDL2, signs ad
# hoc and writes the .dmg. tools/mac/check_package.py checks the result (target mac_check).
if(APPLE AND TARGET cm0102_recomp)
    set(CM0102_GAME_DIR "" CACHE PATH "Folder with the clean files (cm0102.exe, Data, Pictures) for the Mac app")
    set(CM0102_ICNS "" CACHE FILEPATH "Optional .icns file for the app icon. No icon is committed.")
    set(CM0102_SDL2_PREFIX_ARM64 "/opt/homebrew" CACHE PATH "SDL2 prefix (lib and include) built for arm64")
    set(CM0102_SDL2_PREFIX_X86_64 "" CACHE PATH "SDL2 prefix built for x86_64. Empty: the app is arm64 only")
    set(CM0102_APP_OUT "${CMAKE_BINARY_DIR}/mac" CACHE PATH "Output folder of the .app and .dmg")
    add_custom_target(mac_dmg
        COMMAND "${CMAKE_SOURCE_DIR}/tools/mac/make_app.sh"
            --game "${CM0102_GAME_DIR}" --exe "${CM0102_EXE}" --icns "${CM0102_ICNS}"
            --sdl2-arm64 "${CM0102_SDL2_PREFIX_ARM64}" --sdl2-x86_64 "${CM0102_SDL2_PREFIX_X86_64}"
            --out "${CM0102_APP_OUT}" --version "${PROJECT_VERSION}"
        COMMENT "Making the Mac app and .dmg"
        VERBATIM USES_TERMINAL)
    set(CM0102_CHECK_ARCHS "")
    if(NOT CM0102_SDL2_PREFIX_X86_64)
        set(CM0102_CHECK_ARCHS --archs arm64)
    endif()
    add_custom_target(mac_check
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/mac/check_package.py"
            "${CM0102_APP_OUT}/CM-01-02-Bosman-Edition-${PROJECT_VERSION}-mac.dmg"
            --version "${PROJECT_VERSION}" ${CM0102_CHECK_ARCHS}
        COMMENT "Checking the Mac download"
        VERBATIM USES_TERMINAL)
endif()
