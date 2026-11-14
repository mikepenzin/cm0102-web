cmake_minimum_required(VERSION 3.20)
project(cm0102-mac VERSION 0.5.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

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
    # Season databases in the download (0.6.0 slice 14): zip reader, SHA-256, recognition, first pick.
    add_executable(seasons_db_test tests/seasons/test_seasons_db.c src/recomp_rt/win/seasons_db.c
        src/recomp_rt/win/seasons_zip.c src/recomp_rt/win/seasons_table.c src/recomp_rt/win/dbfolders.c
        src/recomp_rt/host/posix.c)
    target_include_directories(seasons_db_test PRIVATE src/recomp_rt/win tests/seasons)
    target_compile_definitions(seasons_db_test PRIVATE
        SIGNATURES_TSV="${CMAKE_SOURCE_DIR}/docs/research/rules-0.6.0/databases-signatures.tsv")
    add_test(NAME seasons_db COMMAND seasons_db_test)
    # The heap of the recompiled game: the join of the free blocks at the end of the arena (decision O121).
    add_executable(heap_test tests/recomp/test_heap.c src/recomp_rt/heap.c)
    target_include_directories(heap_test PRIVATE src/recomp_rt)
    add_test(NAME heap COMMAND heap_test)
    # Token-saving hooks and the log wrapper (docs/specs/2026-10-19-token-saving-spec.md, slice 1).
    add_test(NAME hooks COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/dev/test_hooks.py")
    add_test(NAME seasons_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_db_table.py" --check)
    add_test(NAME season_credits COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/check_credits.py")
    # Right ages and dates in every season (0.6.0 slice 11b): the C table matches the date shift tables.
    add_test(NAME season_dates_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_dates_table.py" --check)
    # International competitions (0.6.0 slice 27): the C table of the editions matches the rules table, and
    # the editions and the rules of the code sites.
    add_test(NAME season_intl_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_intl.py" --check)
    add_executable(season_intl_test tests/seasons/test_season_intl.c src/recomp_rt/win/season_intl_rules.c
        src/recomp_rt/win/season_intl_table.c)
    target_include_directories(season_intl_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_intl COMMAND season_intl_test)
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
    # Domestic cups that the rules table does not hold in some seasons (0.6.0 slice 21b).
    add_executable(season_cup_breaks_test tests/seasons/test_season_cup_breaks.c
        src/recomp_rt/win/season_cup_breaks.c src/recomp_rt/win/season_rules.c src/recomp_rt/host/posix.c)
    target_include_directories(season_cup_breaks_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cup_breaks COMMAND season_cup_breaks_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_cup_breaks PROPERTIES FIXTURES_REQUIRED rules_table)
    # The rest of the domestic cups (0.6.0 slice 21c): four-club super cups, group stages and new cups.
    add_executable(season_cups_more_test tests/seasons/test_season_cups_more.c
        src/recomp_rt/win/season_cups_more.c src/recomp_rt/win/season_rules.c src/recomp_rt/host/posix.c)
    target_include_directories(season_cups_more_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_cups_more COMMAND season_cups_more_test "${CMAKE_BINARY_DIR}/seasons/rules-test.tsv")
    set_tests_properties(season_cups_more PROPERTIES FIXTURES_REQUIRED rules_table)
    # The new competitions module (slice 21c review): the defs that may need a new record fit MAX_NEW.
    add_test(NAME season_comps_new_records COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/comps_new_records_check.py"
        "${CMAKE_SOURCE_DIR}/src/recomp_rt/win")
    # The names table and the club moves table (0.6.0 slice 15): the tools read every part and stop on a bad row.
    add_test(NAME season_names_table COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_names.py" --check)
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
    # The nation rules (0.6.0 slice 17): windows, foreign player limits and the work permit rule. The encoder reads
    # the research parts; its outputs are committed and checked here; the lookup runs on the C table.
    add_executable(season_nation_test tests/seasons/test_season_nation.c src/recomp_rt/win/season_nation.c
        src/recomp_rt/win/season_nation_table.c)
    target_include_directories(season_nation_test PRIVATE src/recomp_rt/win)
    add_test(NAME season_nation COMMAND season_nation_test)
    add_test(NAME season_encode_rules COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/test_encode_rules.py")
    # The league rules (0.6.0 slice 19): play-off places, splits and calendars. The outputs are committed.
    add_test(NAME season_encode_leagues COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tests/seasons/test_encode_leagues.py")
    add_executable(winfit_test tests/winfit/test_winfit.c)
    target_include_directories(winfit_test PRIVATE src/recomp_rt/win)
    add_test(NAME winfit COMMAND winfit_test)
    add_executable(winscale_test tests/winscale/test_winscale.c)
    target_include_directories(winscale_test PRIVATE src/recomp_rt/win)
    add_test(NAME winscale COMMAND winscale_test)
    add_executable(savename_test tests/savename/test_cp1252.c)
    target_include_directories(savename_test PRIVATE src/recomp_rt/win)
    add_test(NAME savename COMMAND savename_test)
    # File name parts that are not UTF-8 (0.6.0 slice 15): win_host_path turns them into UTF-8.
    add_executable(path_utf8_test tests/pathutf8/test_path_utf8.c)
    target_include_directories(path_utf8_test PRIVATE src/recomp_rt/win)
    add_test(NAME path_utf8 COMMAND path_utf8_test)
    add_executable(draw_test tests/draw/test_draw.c src/recomp_rt/win/draw.c)
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
        set(RECOMP_HOST src/recomp_rt/host/win32.c)
    elseif(EMSCRIPTEN)
        set(RECOMP_HOST src/recomp_rt/host/posix.c src/recomp_rt/host/web.c)
    else()
        set(RECOMP_HOST src/recomp_rt/host/posix.c)
    endif()
    add_executable(cm0102_recomp ${RECOMP_GEN}
        src/recomp_rt/rt.c
        src/recomp_rt/heap.c
        ${RECOMP_HOST}
        ${RECOMP_WIN}
    )
    add_dependencies(cm0102_recomp recomp_gen)
    # 0.6.0 slice 11c: the build stops when the history table is not the one plan.tsv gives, or a row is not valid.
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/seasons/history.ok"
        COMMAND "${RECOMP_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_table.py" --check
            --stamp "${CMAKE_BINARY_DIR}/seasons/history.ok"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/seasons/make_history_table.py"
            "${CMAKE_SOURCE_DIR}/docs/research/history-0.6.0/plan.tsv"
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
    target_include_directories(cm0102_recomp PRIVATE src/recomp_rt src/recomp_rt/win "${RECOMP_OUT}/gen"
        ${SDL2_INCLUDE_DIRS})
    target_compile_definitions(cm0102_recomp PRIVATE CM0102_DEFAULT_EXE="${CM0102_EXE}"
        CM0102_VERSION="${PROJECT_VERSION}")
    # The generated C: exact integer wrap, no strict aliasing, no warnings (it is not read by people).
    set_source_files_properties(${RECOMP_GEN} PROPERTIES GENERATED TRUE COMPILE_OPTIONS "-O1;-w")
    target_compile_options(cm0102_recomp PRIVATE -fno-strict-aliasing -fwrapv -O1)
    # Every call of the original is a host call: give the main thread a deep stack.
    if(WIN32)
        include(packaging/windows/windows.cmake)
    elseif(EMSCRIPTEN)
        include(web/web.cmake)
    else()
        if(APPLE)
            target_link_options(cm0102_recomp PRIVATE "LINKER:-stack_size,0x10000000")
        endif()
        target_link_libraries(cm0102_recomp PRIVATE ${SDL2_LIBRARIES} m)
    endif()
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
