"""Functions of the original that the runtime calls by address with rt_callback.

RT_WRAP and RT_REPLACE stop the build when their function is missing. A plain rt_callback call has no
such check, so a wrong address would only fail at run time. This list closes the gap in two ways:

- Each address below must be a translated function that starts with the bytes written here (its first
  eight bytes in cm0102.exe 3.9.68).
- Each address of code that a file in CALLBACK_FILES writes in C code (not in a comment) must be in this
  list. So a new call needs a line here, with the bytes from the original.
"""
import os
import re

CALLBACK_FILES = ("picker.c", "assistant.c", "commentary.c", "holiday_next.c", "views.c", "print_pdf.c", "best_xi.c",
                  "tactics.c", "groups.c", "compare.c", "compare_screen.c", "names.c", "pdf.c", "dbfolders.c",
                  "foreign.c", "season.c", "season_dates.c", "season_history.c",
                  "season_change.c", "season_kind_club_world.c", "season_comps.c", "season_bra_states.c",
                  "season_super_cups.c", "season_new_cups.c", "season_cup_groups.c",
                  "season_club_cups.c", "season_playoffs.c", "season_nl.c", "season_nl_euro.c", "season_nl_wc.c",
                  "season_gold_cup.c", "season_divisions.c", "season_tur_groups.c", "screen_sweep.c")
                                # in src/recomp_rt/win

# address: (bytes of the first eight, name)
CALLBACKS = {
    # O288 (fixture_season_end.c, not in CALLBACK_FILES: it compares return addresses of 0x5abb10's calls): the free
    # day before a season change for a match moved for a call-up.
    0x549EF0: ("668b442404568b74", "Date::Date (day of the year, year), ret 8"),
    0x54A3F0: ("565733ffb81f85eb", "Date::get_weekday, 0 Monday..6 Sunday"),
    0x5AF2F0: ("8b44240485c07476", "FIXTURE_MANAGER::get_time_slice (comp, weekday), ret 8"),
    0x5AD780: ("6aff68c997950064", "FIXTURE_MANAGER::find_fixture_clashes (fixture, &severity, flag), ret 0xc"),
    0x68D280: ("8b4102568b742408", "LLIST::remove_node (node), ret 4"),
    # O260 A15 (call_up_news.c): the repeats of the national squads before the call-up news.
    0x76C060: ("51535532c0565788", "national_teams_remove_duplicate_squad_entries (nation), ret 4"),
    # Slice 21c (season_super_cups.c): the clubs and rounds of the super cups.
    0x521E60: ("8b4c24148b442418", "cup_set_round_draw_date (table, round, day, month, year+, weekday, year), cdecl"),
    0x521EB0: ("8b4c24148b442418", "cup_set_round_match_date (table, round, day, month, year+, weekday, slot, year, venue)"),
    0x65F670: ("6aff68b9bd950064", "ItaSuper::ItaSuper (this, year, record), ret 8: the new cups of season_new_cups.c"),
    # Slice 21c (season_cup_groups.c): the group stages of the domestic cups.
    0x6826D0: ("8b4424048d14c500", "League::get_table_row (this, index)"),
    0x48C6D0: ("8b44241453565785", "Comp::build_fate_news_text, 10 arguments: EFL Trophy news, no fixture"),
    0x90CFC0: ("51568b74240c85f6", "utils_random (range), cdecl"),
    0x945501: ("558bec568b75080f", "calloc (count, size), cdecl"),
    0x68B100: ("6aff68ebca950064", "League_Stage::League_Stage (this, 22 arguments)"),
    0x684230: ("64a1000000006aff", "League::send_group_draw_news (this)"),
    # Slice 30a (screen_sweep.c, RECOMP_SCREEN_SWEEP): the competition and history screens of every competition.
    0x48F4A0: ("81ecf0000000558b", "comp_screens_open_comp_screen (record, int, int, keep)"),
    0x498780: ("81ecf0000000558b", "comp_screens_open_history_screen (record, keep)"),
    0x55F7C0: ("81ec000200005355", "the reset of a screen manager before a build (thiscall, 1)"),
    0x55F300: ("51558be957b95044", "the screen draw of a screen manager (thiscall, -1, -1, -1, -1)"),
    # Slice 28 (season_nl.c): the UEFA Nations League, an object of the class euro_champ_qual.
    # Slice 28 part b (season_gold_cup.c): the Gold Cup (copa_america) and the Finalissima (Eur_Super_Cup).
    0x5FA730: ("8b01535556668b71", "HostCountry::find_hosts (this, key, year, &host1, &host2, 1)"),
    0x5E0660: ("6aff6829a8950064", "copa_america::copa_america (this, year, record)"),
    0x57A100: ("6aff683987950064", "Eur_Super_Cup::Eur_Super_Cup (this, year, record)"),
    0x4AFD80: ("81ec1c020000a17c", "comp_util_get_latest_placed_club (record, place)"),
    0x945333: ("558bec81ecf80000", "qsort (base, count, size, compare)"),
    0x4B07D0: ("8b4424048b542408", "compare of club pointers (a, b)"),
    0x57CE10: ("6aff686788950064", "EuroChampQual::EuroChampQual (this, year, record)"),
    0x57E8F0: ("6aff68a888950064", "EuroChampQual::set_up_rules (this)"),
    0x57EB20: ("81ec000200005356", "EuroChampQual::start_new_season (this)"),
    0x57FD60: ("6aff68a689950064", "EuroChampQual::update_for_date (this, today, full)"),
    0x90FBE0: ("8b41048b0d1843b7", "EuroChampQual slot 12: clear_competition_for_comp_clubs (this)"),
    0x521F10: ("668b4c240c81ec00", "cup round: match slot (rec, round, slot, weekday, time, 3, flag)"),
    0x53B3D0: ("8b44240481ec0002", "database_get_national_team (nation)"),
    0x549F70: ("81ec000200005657", "Date::Date (this, day, month0, year, weekday)"),
    # Row A18 (season_friendly_dates.c, not in CALLBACK_FILES: it compares return addresses of add_fixture's calls):
    # the compare function of 0x5cc510's qsort of the friendly date rows, and the qualifying matches on FIFA days.
    0x5CC610: ("8b442404568b7424", "friendly date row compare (row a, row b), the qsort of 0x5cc5dd"),
    0x76D940: ("8b44240485c07454", "national_teams_is_finals_comp (record), stdcall"),
    0x5AE6A0: ("8b54240c8b442404", "fix_man_get_fixture_list_for_date (this, Date *, slice, type)"),
    0x68A160: ("8b4c24148b442418", "league_round_set_date (rounds, i, day, month0, yoff, weekday, flag, year, 0)"),
    0x68A1C0: ("81ec000200005356", "league_round_set_match_slot (rounds, i, 0, -1, -1, -1, 0)"),
    # Slice 26 (season_kind_club_world.c): the knockout of the FIFA Club World Championship.
    0x92A8B0: ("81ec08020000568b", "WorldClubChamp::pick_entrants (this): the 12 entrants into +0xee"),
    0x92AE60: ("b844210000e8a6a7", "WorldClubChamp::pick_entrant (this, continent, nation, picked, count, record)"),
    0x929390: ("81ec000200005356", "WorldClubChamp::create_schedule (this, stage, &rounds, &code, &is_league)"),
    # Slice 26c: the 32-club Club World Cup (Form G) with the Libertadores builders.
    0x4C37A0: ("81ec04020000568b", "Conmebol_Liber group A: the parent's group from entrants 0 to 3 (this)"),
    0x4C3B10: ("64a1000000006aff", "Conmebol_Liber group i + 1 at stage slot i (this, i), ret 4"),
    0x4C3A80: ("8b512c8b413081ec", "Conmebol_Liber::advance_stage (this): at 7 the knockout 0x4c3d10"),
    0x6835C0: ("6aff6881c9950064", "League::create_fixtures (this)"),
    0x6827D0: ("83ec4c535556578b", "League table sort (this, 0)"),
    0x90CFC0: ("51568b74240c85f6", "random (n), cdecl"),
    # Slice 26c (season_club_cups.c): the new club cups on the World_Club_Cup class.
    0x92B4B0: ("6aff68395f960064", "World_Club_Cup::World_Club_Cup (this, year, record), ret 8"),
    0x92B770: ("8a44240481ec0002", "World_Club_Cup::create_schedule (this, stage, &rounds, &code, &is_league), ret 0x10"),
    0x51F890: ("6aff689877950064", "Cup::update_for_date (this, today, arg2), ret 8: the jump of the cups' vtable copy"),
    0x4C6730: ("83ec0c0fbf442414", "Conmebol_Seeding::pick_club_by_position (this, list, count, nation, n, skip, filter), ret 0x18"),
    0x51C800: ("6aff68d976950064", "Cup::set_up_round (this, round)"),
    0x9452CA: ("558bec51568b7508", "free (block)"),
    0x632080: ("6aff6819b2950064", "Inter_Amer_Cup::Inter_Amer_Cup (this, year, record), ret 8: the test def of season_comps.c"),
    0x9022E0: ("6aff68d951960064", "Uefa_Cup::Uefa_Cup (this, year, record), ret 8: the Conference League of season_comps.c (slice 24)"),
    0x944E46: ("ff350830de00ff74", "operator new of the stage arrays (size), cdecl (0x9291e9)"),
    0x45CBE0: ("5355568b74241085", "can the player play the fixture (squad screen test)"),
    0x5431E0: ("8b44240481ec0002", "shown attribute of a player (staff, index 1 to 31)"),
    0x543280: ("8b4424045685c00f", "is an attribute shown to a manager (manager, staff, index)"),
    0x544380: ("8b4424040fbe5424", "player_attribute_rating (player data, byte): groups.c, One On Ones"),
    0x528080: ("a1a423ae005633f6", "database_free_all_tables (season_dates.c: a reload for another season)"),
    0x527F60: ("a17c23ae00568b74", "database_grow_comp_history_table (season_history.c: the added history rows)"),
    0x54C5B0: ("83ec10a06924ae00", "date plus days"),
    0x55F990: ("81ec00020000568b", "popup item create"),
    0x55FBA0: ("a10c29ae008b1510", "popup area create"),
    0x8AC600: ("a0201ede0083ec38", "draw_pitch (l, t, r, b, *ol, *ot, *or, *ob, across, owner, float scale): best_xi.c"),
    0x5ACBE0: ("81ec240200008b84", "fixture search"),
    0x5E8290: ("83ec6c8b0d307ab6", "message box (title, text, flags)"),
    0x5FCFF0: ("8b44240481ec0002", "commentary speed of a manager"),
    0x5FD2F0: ("81ec00020000568b", "set the holiday flag"),
    0x5FED50: ("8b44240481ec0002", "selection saved at the start of a holiday"),
    0x6B2D60: ("6a006a006a016840", "push the Go on holiday confirm"),
    0x5FBD00: ("8b44240481ec0002", "tactic of a manager"),
    0x62C400: ("81ec08020000568b", "injury state"),
    0x62C590: ("568b742408578bf9", "condition"),
    0x68D040: ("568bf18b460285c0", "list free"),
    0x68D1B0: ("8b510a85d274098b", "list first"),
    0x6E1CF0: ("558bec83e4f883ec", "captain value"),
    0x7757D0: ("6aff68f0de950064", "international duty"),
    0x7FF010: ("0fbf812a38000081", "screen node slot"),
    0x8040A0: ("8b4424048b54242c", "cell button create"),
    0x41C250: ("5657b95044b700e8", "club background picture"),
    0x53B640: ("66a1f0bdae0081ec", "club colours"),
    0x542100: ("8b44241081ec0002", "club name"),
    0x5ED780: ("64a1000000006aff", "tab bar"),
    0x5EDCC0: ("81ecd4070000566a", "Back and Next bar"),
    0x7FEC60: ("0fbf812a38000081", "screen stack: cannot go back"),
    0x7FEBB0: ("0fbf812a38000081", "screen stack: at the end"),
    0x7FE600: ("81ec08020000538b", "screen push"),
    0x7FF270: ("81ec00020000538b", "set a screen node slot"),
    0x804030: ("8b4424308b54242c", "button with its own rectangle"),
    0x803F30: ("8b44240c8b542408", "screen heading"),
    0x75DB00: ("81ec640300000fbf", "side bar build"),
    0x7615D0: ("81eca4020000538b", "side bar input"),
    0x7643D0: ("83ec5866a17a7cb7", "game actions input (Back, profiles)"),
    0x896A90: ("81ec000200005355", "store a selection slot"),
    0x896FE0: ("81ec00020000538b", "clear a selection"),
    0x897760: ("8b44240481ec0002", "store the captain"),
    0x5428A0: ("8a4c240481ec0002", "name of an attribute (1 to 31, text, size, 0)"),
    0x53EE50: ("b8a4110000e8b667", "positions text of a player (data, text, size, 1)"),
    0x543440: ("81ec000200005355", "shown value of an attribute, masked (viewer, staff, attribute)"),
    0x543540: ("8b54240481ec0402", "shown value of an attribute (staff, data, attribute)"),
    0x5E5AD0: ("b804200000e836fb", "width of a text in screen pixels (font, text)"),
    0x7F7920: ("0fbf057a7cb70081", "player of the comparison (scout manager, 1 or 2)"),
    0x8B1750: ("81ec340400005355", "training manager: train a player"),
    0x41C790: ("5657b95044b700e8", "staff picture as the screen's picture"),
    0x53DBA0: ("81ec000200005355", "full name of a staff record"),
    0x53B900: ("8b44240481ec0002", "is a club a national side (club)"),
    0x48DEE0: ("53568b74240c8b56", "fixture category of a club (club)"),
    0x5AF5B0: ("8b442404538b4053", "second fixture category of a club (club)"),
    0x8D2FA0: ("8b1584f69c00568b", "competition of a fixture: kind, nation, continent (fixture, 3 out pointers)"),
    0x8CF6B0: ("515355568b742418", "foreign player rule of a fixture (fixture, record, 1)"),
    0x53D980: ("8b44240883ec1085", "is a person foreign for a nation (person, nation, 1)"),
    0x53D740: ("8b442404b102568b", "is a person an EU player for a nation (person, nation)"),
    0x54A130: ("81ec000200005657", "set a date (day, month, year, weekday), thiscall: the game date of a new season"),
    # The news item of a season change (season_change.c, 0.6.0 slice 16).
    0x77BBE0: ("6aff68fee0950064", "news item constructor (type, flag), thiscall"),
    0x785790: ("8a44240481ec0002", "news item: set a slot (slot, value), thiscall"),
    0x786780: ("81ec000200005556", "news store: send an item to every human manager (item), thiscall"),
    0x4AFA40: ("568bf1578b0685c0", "text: clear, thiscall"),
    0x4AE770: ("81ec000200005355", "text: append a string (text), thiscall"),
    0x944CF1: ("6a01ff742408e85c", "operator new (size), cdecl: an entry's competition array (season_change.c)"),
    0x944C94: ("ff742404e82d0600", "operator delete (block), cdecl"),
    # season_playoffs.c (slice 19): the play-off stage of a league with none
    0x522E00: ("6aff683b78950064", "Cup_Stage::Cup_Stage, thiscall, 15 arguments"),
    0x51C800: ("6aff68d976950064", "Cup::set_up_round, thiscall (round)"),
    0x521E60: ("8b4c24148b442418", "cup_set_round_draw_date, cdecl"),
    0x521EB0: ("8b4c24148b442418", "cup_set_round_match_date, cdecl"),
    # season_nl_euro.c (O320): the Euro 2028 host places
    0x57C120: ("668b413681ec0002", "EuroChamp::add_qualified_team (team), thiscall"),
    0x571980: ("8a44240481ec0802", "EngFirst +0x3c: the round table of a stage, thiscall (stage, &rounds, &name, flag)"),
    0x576C50: ("8a44240c5355563c", "EngFirst +0x44: a stage result (jumped to from the wrap of 0x686940)"),
    0x944E46: ("ff350830de00ff74", "malloc, cdecl"),
    0x9452CA: ("558bec51568b7508", "free, cdecl"),
    0x5A8F60: ("c390909090909090", "the empty function (compared with vtable +0x28, wrapped in season_uefa_cup.c)"),
    0x686940: ("8a4424083c057519", "League +0x44, base (compared with vtable +0x44, wrapped in season_playoffs.c)"),
    # season_divisions.c (slice 20): the 3. Liga in the German Regional object
    0x689C80: ("81ec100200008b84", "league_promote_and_relegate, stdcall (upper, lower, expire, pairwise, up, down)"),
    0x6830B0: ("8b0df07edd005556", "League::move_club_to_division, thiscall (club, record, 1), ret 0xc"),
    0x6831A0: ("568b742408578b7c", "League::relegate_club_to_division, thiscall (club, record, 1), ret 0xc"),
    0x4AFBD0: ("81ec000200005356", "sort a club list for the new clubs of a tier, cdecl (list, count, need)"),
    0x540A50: ("8b4c240481ec0002", "the senior side of a club, cdecl (club, &flag, 1)"),
    0x945333: ("558bec81ecf80000", "qsort, cdecl (base, count, width, compare)"),
    0x4B07D0: ("8b4424048b542408", "the compare of GerRegional's predicted positions (passed to qsort)"),
    0x4A2540: ("81ec000200005657", "set a predicted position, thiscall (club, position), ret 8"),
    0x5EB2B0: ("6aff6829aa950064", "GreSecond::GreSecond, thiscall (year, record), ret 8 (the second Greek group)"),
    0x53FD40: ("81ec080200008b84", "the compare of the Spanish and Portuguese group deals (passed to qsort)"),
    # season_bra_states.c (slice 20e): the state championships of Brazil
    0x687430: ("81ec080200005355", "League: add a club to the table row (row, club, 0, 0), thiscall"),
    0x435340: ("6aff683948950064", "BraRegNorth::BraRegNorth, thiscall (year, record), ret 8"),
    # season_tur_groups.c (Summer 2002 slice 5): the Turkish Category B in three groups
    0x53FE50: ("81ec080200008b84", "the compare of the Turkish group deal 0x8fe580 (passed to qsort)"),
    0x8FF7E0: ("8a44240481ec0802", "TurSecondB::create_schedule, thiscall (stage, &count, &code, &flag), ret 0x10"),
    0x7D2CD0: ("81ec04020000558b", "PorSecondB::set_predicted_positions (own table and slots 0, 1), thiscall"),
    0x4241B0: ("8b4424048b490856", "BelThird::set_club_progress (three groups), thiscall, ret 0x14"),
    0x684640: ("83ec0c5355568bf1", "League::record_season_honours, thiscall"),
    0x48C6D0: ("8b44241453565785", "Comp::build_fate_news_text, thiscall, ret 0x28"),
}


def check(image, funcs, win_dir):
    """The problems of the list, as strings. funcs: the translated function addresses."""
    problems = []
    for addr, (head, name) in sorted(CALLBACKS.items()):
        if addr not in funcs:
            problems.append("%#x (%s) is not a translated function" % (addr, name))
        elif image.read(addr, 8).hex() != head:
            problems.append("%#x (%s) starts with %s, not %s" % (addr, name, image.read(addr, 8).hex(), head))
    for fname in CALLBACK_FILES:
        text = open(os.path.join(win_dir, fname)).read()
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        text = re.sub(r"//[^\n]*", "", text)
        for lit in sorted(set(re.findall(r"\b0x[0-9a-fA-F]{5,8}(?=[uUlL]*\b)", text))):
            v = int(lit, 16)
            if image.is_code(v) and v not in CALLBACKS:
                problems.append("%s uses code address %#x that is not in tools/recomp/callbacks.py" % (fname, v))
    return problems
