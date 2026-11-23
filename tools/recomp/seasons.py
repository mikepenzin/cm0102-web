"""Year constants of the original that become run-time values, for a game that starts in another season.

The original starts every game in 2001/02 and writes 2001, 2002 and 2003 as constants in some instructions.
Each entry of PATCHES names the address of one instruction, the constant it holds and the C expression that
replaces it. rt_season_year is the start year of the game (src/recomp_rt/win/season.c): 2001 in the base game,
so every replaced instruction gives the original's result there. INSNS replaces a whole instruction, as in
widescreen.py. The evidence for each address is in docs/research/2026-10-17-first-season-branches.md
(0.6.0 slice 11).
"""

Y = "rt_season_year"
Y1 = "(rt_season_year + 1u)"
Y2 = "(rt_season_year + 2u)"

# address: (constant in the original, C expression)
PATCHES = {
    # The start date (doc table 1, sweep-season-end 3.1). The game date is 1 January of the start year: at the
    # start screen (setup_run_front_end 0x81b890, Date::set at 0x81b949) and after a game ends (free_game_data
    # 0x82ff40, Date::set at 0x8304fe). The league table takes the season of each entry from that year (0x667150).
    0x81B93C: (2001, Y),
    0x8304F1: (2001, Y),
    # The Europe clamp of setup_pick_start_date 0x830520: in the first season a European start date after
    # 1 September becomes 1 September (the compare and the two dates).
    0x83058A: (2001, Y),
    0x830597: (2001, Y),
    0x8305DB: (2001, Y),

    # First-season branches that mean "the first season of the game" (MOVE). Doc tables 2 and 3.
    # The database merges of group divisions into one division, in the first season of each new game.
    0x4224EC: (2001, Y),        # BelThird::fill_league_table: Third Division A and B
    0x5DB23E: (2001, Y),        # GerFirst::create_table: the four regional divisions
    0x7D240C: (2001, Y),        # PorSecondB::create_table: II Divisao B Norte, Centro, Sul
    0x856CDC: (2001, Y),        # SpaSecondB::load_group_clubs: Segunda B groups 1 to 4
    0x8FDDCE: (2001, Y),        # TurFirst::create_table: 2 Division Category B groups 1 to 5
    0x56FC5F: (2001, Y),        # EngFaCup::collect_entrants: the FA Trophy clubs from the database history
    # The run from 1 January of the start year to the start date plays no season: these skip that year.
    0x5836C4: (2001, Y),        # EuropeanAwards::daily_update
    0x5FA89B: (2001, Y),        # HostCountry::daily_update
    0x632E30: (2001, Y),        # InternationalAwards::daily_update
    0x79E8A0: (2001, Y),        # OldInternationalAwards::daily_update
    0x5C3065: (2001, Y),        # Friendly::plan_preseason_tournaments_and_tours
    0x931AE9: (2001, Y),        # YEAR_RATINGS::collect_best_players: half a year of ratings
    # The first calendar years of the game.
    0x59B56B: (2002, Y1),       # finance_get_days_to_season_start: split-season leagues end in Y + 1
    0x6EB28D: (2002, Y1),       # MATCH_PLAYER::setup: Japan (calendar year) has no full season before Y + 1
    0x5AEE52: (2001, Y),        # FIXTURE_MANAGER::get_club_first_fixture_year: Y + bit 0 of the club dates
    0x5AF218: (2001, Y),        # fix_man_record_club_fixture_years: a fixture in the first calendar year
    0x5AF243: (2001, Y),
    0x5AF288: (2001, Y),
    0x5AF2B9: (2001, Y),
    0x45582D: (2001, Y),        # club_screen_build_fragment_454950: the first season starts on 1 January
    0x45583C: (2001, Y),
    0x45F4E9: (2001, Y),        # club_screen_draw_fixtures_tab: no season before the first
    0x45F979: (2001, Y),
    0x45F980: (2001, Y),
    0x483322: (2001, Y),        # handle_club_screen_action: a friendly date before the game start
    # The first season year of a league table entry (+0x1b, doc table 4). The national create functions write
    # the game year there. Read by UefaSeeding::assign_uefa_cup_places at 0x904951.
    0x830FB0: (2001, Y),
    0x8312B0: (2001, Y),
    0x83161E: (2001, Y),
    0x8318C2: (2001, Y),
    0x831B69: (2001, Y),
    0x831E7C: (2001, Y),
    0x8320C9: (2001, Y),
    0x8371D1: (2001, Y),

    # International and club editions (doc table 5).
    0x58B385: (2001, Y),        # FifaConfed::FifaConfed: no Confederations Cup in the start year (the run-up)
    0x91F486: (2001, Y),        # WcOceaniaLeague::update_for_date: a campaign that began before the game
    0x92AF4E: (2001, Y),        # WorldClubChamp::pick_entrant: first edition, entrants from the history
    0x92B24B: (2001, Y),
    0x92B970: (2001, Y),        # WorldClubCup::pick_qualifiers: first edition, champions from the history
    0x92BA9C: (2001, Y),
    0x632540: (2001, Y),        # InterAmerCup::select_teams: first edition, champions from the history
    0x63266C: (2001, Y),
    # UEFA seeding in the first season of the game (doc table 6).
    0x9041EC: (2001, "rt_euro_cfg_year()"),  # UefaSeeding::collect_european_clubs: the start season's euro.cfg
    0x903B6F: (2001, Y),        # UefaSeeding::roll_coefficients: the coefficients of the database as they are
    0x9038AF: (2001, Y),        # UefaSeeding::pick_english_intertoto_club: no volunteer list yet
    0x9059B2: (2001, Y),        # uefa_check_registration_deadlines: no 1 February deadline in the run-up

    # The Select Start Season screen 0x81e8d0 writes a split season as "%s %02d/%02d" with year % 100 and
    # (year + 1) % 10 (0x81e9c9): 2010 shows "10/01". With % 100 it shows "10/11". For 2001 to 2008 both give
    # the same text, so the base game is unchanged.
    0x81E9C9: (10, "100u"),

    # Q358: the season formatters of key_nation.cpp write "%d/%d" (0x9eb024) with (year + 1) % 10, "2009/0".
    # rt_season_text_mod and rt_season_text_format (win/season.c) give % 100 and "%d/%02d", "2009/10"; with
    # RECOMP_SEASON_CODE=off the original's values.
    0x6696D3: (10, "rt_season_text_mod(10u)"),              # key_nation_format_club_season 0x669630
    0x6696E6: (0x9EB024, "rt_season_text_format(0x9eb024u)"),
    0x669869: (10, "rt_season_text_mod(10u)"),              # key_nation_format_comp_season 0x6697c0
    0x669876: (0x9EB024, "rt_season_text_format(0x9eb024u)"),

    # Slice 26: the club competitions outside UEFA (src/recomp_rt/win/season_kind_club_world.c, evidence in
    # docs/research/2026-10-18-club-world-idle.md, section 5). In a season game:
    0x929D9E: (2, "rt_club_world_step(2u)"),    # WorldClubChamp::start_new_season: one edition a year
    0x797DBE: (2, "rt_oceania_step(2u)"),       # OceaniaClubChamp::start_new_season: one edition a year
    0x929C39: (1, "rt_club_world_month(1u)"),   # WorldClubChamp::set_up_rules: start date Date(5, 1, year)
    0x929C3B: (5, "rt_club_world_day(5u)"),     # becomes Date(1, 6, year): 1 July, after the update day
    # Slice 26c (season_club_cups.c): the club count of the CONCACAF Champions Cup (Con_Champ) by season, 8 in
    # the original: the constructor's club list (calloc count 0x4c0cc2, +0x3e at 0x4c0cc4) and the picker's +0x3e
    # (0x4c11af). esi is the object; +0x40 is its year (set at 0x4c0c8d).
    0x4C0CC2: (8, "rt_concacaf_clubs(R16(esi + 0x40u), 8u)"),
    0x4C0CC4: (8, "rt_concacaf_clubs(R16(esi + 0x40u), 8u)"),
    0x4C11AF: (8, "rt_concacaf_clubs(R16(esi + 0x40u), 8u)"),
    # International finals (0.6.0 slice 27, docs/research/2026-10-18-international.md). The golden goal is bit
    # 0x80 of the rule flags of a knockout round (sweep-international section 7). The round template function
    # of each class holds the competition in edi (set at 0x92c299, 0x57ab99, 0x58b56f) and the round records in
    # esi. rt_intl_round_flags (win/season_intl_rules.c) clears the bit for an edition without golden goal; the
    # base game keeps the flags. The first argument is the class (season_intl.h): 0 World Cup, 1 European
    # Championship, 2 Confederations Cup.
    # MLS play-off and final rounds (season_mls.c, rule O284): flag 0x20 (the run-in shootout) becomes extra time
    # and penalties in a season game. UsaMls::create_schedule 0x9072d0.
    0x907FE7: (0x230, "rt_mls_round_flags(0x230u)"),
    0x908003: (0x30, "rt_mls_round_flags(0x30u)"),
    0x908009: (0x20, "rt_mls_round_flags(0x20u)"),
    0x908068: (0x230, "rt_mls_round_flags(0x230u)"),
    0x90808B: (0x30, "rt_mls_round_flags(0x30u)"),
    0x908091: (0x20, "rt_mls_round_flags(0x20u)"),
    0x9081C2: (0x220, "rt_mls_round_flags(0x220u)"),
    0x92C96B: (0x383, "rt_intl_round_flags(0u, R16(edi + 0x40u), 0x383u)"),    # World Cup last 16
    0x92CA46: (0x383, "rt_intl_round_flags(0u, R16(edi + 0x40u), 0x383u)"),    # quarter-final
    0x92CB15: (0x383, "rt_intl_round_flags(0u, R16(edi + 0x40u), 0x383u)"),    # semi-final
    0x92CBB9: (0x83, "rt_intl_round_flags(0u, R16(edi + 0x40u), 0x83u)"),      # third place
    0x57B002: (0x383, "rt_intl_round_flags(1u, R16(edi + 0x40u), 0x383u)"),    # Euro quarter-final
    0x57B0B2: (0x383, "rt_intl_round_flags(1u, R16(edi + 0x40u), 0x383u)"),    # semi-final
    0x57B150: (0x283, "rt_intl_round_flags(1u, R16(edi + 0x40u), 0x283u)"),    # final
    0x58B892: (0x383, "rt_intl_round_flags(2u, R16(edi + 0x40u), 0x383u)"),    # Confederations Cup semi-final
    0x58B90B: (0x83, "rt_intl_round_flags(2u, R16(edi + 0x40u), 0x83u)"),      # third place
    0x58B9A5: (0x83, "rt_intl_round_flags(2u, R16(edi + 0x40u), 0x83u)"),      # final

    # Euro of 24 teams (0.6.0 slice 27, win/season_intl_euro.c). From Euro 2016 the finals have 6 groups of 4
    # and a round of 16. euro_champ (ctor 0x57a890) is the League of group A, stage[0..2] are groups B to D and
    # stage[3] the knockout of 8. With 24 teams stage[0..4] are groups B to F and stage[5] the knockout of 16.
    # rt_intl_euro_teams gives the team count (16 in the original), rt_intl_euro_ko the knockout stage index (3).
    # Both give the original's value in the base game and for an edition of 16 teams. this is in esi or ecx.
    0x57A949: (0x10, "(4u * (rt_intl_euro_ko(esi, 3u) + 1u))"),     # ctor: malloc of the stage array
    0x57A951: (0x4, "(rt_intl_euro_ko(esi, 3u) + 1u)"),             # ctor: stage count +0x2c
    0x57AA72: (0x10, "rt_intl_euro_teams(esi, 0x10u)"),            # ctor: edition in the game year, +0x36
    0x57BD8B: (0x10, "rt_intl_euro_teams(esi, 0x10u)"),            # build_team_slots 0x57bd80: calloc of slots
    0x57BD93: (0x10, "rt_intl_euro_teams(esi, 0x10u)"),            # build_team_slots: slot room +0x38
    0x57B60B: (0x10, "rt_intl_euro_teams(esi, 0x10u)"),            # group A from the draw array (0x57b530)
    0x57B9CB: (0x10, "rt_intl_euro_teams(esi, 0x10u)"),            # groups B.. from the draw array (0x57b910)
    0x57C2E5: (0x3, "rt_intl_euro_ko(esi, 3u)"),                   # run_group_draw 0x57c1d0: League_Stage groups
    0x57B8FC: (0x3, "rt_intl_euro_ko(ecx, 3u)"),                   # stage step 0x57b880: knockout made at ko
    0x57B1D5: (0x3, "rt_intl_euro_ko(ecx, 3u)"),                   # stage news (vtable +0x44): group or knockout
    0x57B334: (0x3, "rt_intl_euro_ko(ecx, 3u)"),                   # result news (vtable +0x48)
    0x57B736: (0xC, "(4u * rt_intl_euro_ko(esi, 3u))"),            # vtable +0x30: the knockout stage[ko]
    0x57B255: (0xC, "(4u * rt_intl_euro_ko(esi, 3u))"),            # stage news: round record of stage[ko]

    # World Cup qualifying from the 2026 cycle (0.6.0 slice 29, win/season_wcq26.c; evidence in
    # docs/research/2026-10-19-world-cup-48.md, "Qualifying"). Each gives the original's value outside the gate (a
    # section whose campaign +0x40 + 2 is a World Cup of 48 teams, in a season game). this in the register named.
    0x921098: (0x4, "rt_wcq26_south_america_direct(esi, 4u)"),     # set_up_rules 0x921040: direct places, 6
    0x91377F: (0x2, "rt_wcq26_asia_direct(esi, 2u)"),               # final groups 0x913740: group A's places
    0x913BCE: (0x2, "rt_wcq26_asia_playoff_row(esi, 0, 2u)"),       # play-off 0x913b60: the row after them, stage 9
    0x913BDA: (0x2, "rt_wcq26_asia_playoff_row(esi, 1, 2u)"),       # and stage 10
    0x911CF6: (0xD, "rt_wcq26_asia_stages(esi, 0xdu)"),             # ctor: no stage 12 (tie with Europe)
    0x910133: (0x32, "rt_caf_first(esi, 0x32u)"),               # first round 0x910120: 2 x (pool - 45)
    0x90F6F0: (0x32, "rt_caf_first(edi, 0x32u)"),               # template 0x90f600: first round teams
    0x90F6F5: (0x19, "(rt_caf_first(edi, 0x32u) / 2u)"),        # and ties
    0x90F357: (0x5, "rt_wcq26_africa_groups(esi, 5u)"),             # ctor: stage count (9 groups)
    0x91058A: (0x5, "rt_wcq26_africa_groups(esi, 5u)"),             # start_new_season: stage count
    0x90F768: (0x5, "rt_wcq26_africa_groups(edi, 5u)"),             # template: group stages below 9
    0x90FC24: (0x5, "rt_wcq26_africa_groups(ecx, 5u)"),             # slot 17 0x90fbf0: winners of stages below 9
    # Every African nation enters (0.6.2 item 199, season_caf_pool.c): the pool builder 0x9106a0 (this in esi)
    # reads every nation of the table into an array of that size, and load_extra_data 0x910f00 (this in ebx)
    # allocates +0xb6 slots when that is more than the original's 52.
    0x910896: (0x34, "rt_caf_pool_cap(esi, 0x34u)"),
    0x9107ED: (0xD0, "rt_caf_pool_bytes(esi, 0xd0u)"),
    0x910FEC: (0xD0, "rt_caf_load_bytes(ebx, 0xd0u)"),
    # The last group rounds of the template 0x90f600 (this in edi) leave July and the play-off follows them (item
    # 211, rt_caf_round_date): the day and month pushes.
    0x90F9E6: (0x1E, "rt_caf_round_date(edi, 7, 0, 0x1eu)"),
    0x90F9E4: (0x5, "rt_caf_round_date(edi, 7, 1, 5u)"),
    0x90FA18: (0xE, "rt_caf_round_date(edi, 8, 0, 0xeu)"),
    0x90FA16: (0x6, "rt_caf_round_date(edi, 8, 1, 6u)"),
    0x90FA52: (0x11, "rt_caf_round_date(edi, 9, 0, 0x11u)"),
    0x90FA50: (0x6, "rt_caf_round_date(edi, 9, 1, 6u)"),
    0x90FB42: (0x1D, "rt_caf_round_date(edi, 10, 0, 0x1du)"),  # play-off of exempt 2 and 3: first leg
    0x90FB40: (0x8, "rt_caf_round_date(edi, 10, 1, 8u)"),
    0x90FB59: (0x14, "rt_caf_round_date(edi, 11, 0, 0x14u)"),  # and second leg
    0x90FB57: (0x9, "rt_caf_round_date(edi, 11, 1, 9u)"),
    0x91E076: (0x3, "rt_wcq26_oceania_stages(esi, 3u)"),            # ctor: stage count 4 (the tournament)
    0x91F002: (0xC, "(4u * rt_wcq26_oceania_stages(esi, 3u))"),     # start_new_season: malloc of the stage array
    0x91F004: (0x3, "rt_wcq26_oceania_stages(esi, 3u)"),            # start_new_season: stage count
    0x91A0C2: (0x8, "rt_wcq26_europe_index(esi, 8u)"),              # pool 0x919e30: last group stage +0xff
    0x91A0C9: (0x9, "rt_wcq26_europe_index(esi, 9u)"),              # runners-up stage +0x100
    0x91A0D0: (0xA, "rt_wcq26_europe_index(esi, 0xau)"),            # play-off stage +0x101
    0x918348: (0xB, "rt_wcq26_europe_index(esi, 0xbu)"),            # ctor: stage count 13
    0x91950C: (0xB, "rt_wcq26_europe_index(esi, 0xbu)"),            # start_new_season: stage count 13
    0x91A6CF: (0xA, "rt_wcq26_europe_index(esi, 0xau)"),            # group stages after the draw: 12 groups

    # World Cup of 48 teams (0.6.0 slice 29, win/season_wc48.c). From 2026 the finals have 12 groups of 4 and a
    # round of 32. world_cup (ctor 0x92bf50) is the League of group A, stage[0..6] are groups B to H and stage[7]
    # the knockout of 16. With 48 teams stage[0..10] are groups B to L and stage[11] the knockout of 32.
    # rt_intl_wc_teams gives the team count (32 in the original), rt_intl_wc_ko the knockout stage index (7).
    # Both give the original's value in the base game and for an edition of 32 teams. this is in esi or ecx.
    # Evidence: docs/research/2026-10-19-world-cup-48.md. The flags of the compares are read by the jumps after
    # them only (0x92ccf5 and 0x92cea5 by two jumps, both on the same compare).
    0x92BFF0: (0x20, "rt_intl_wc_teams(esi, 0x20u)"),              # ctor: calloc count of the slot array
    0x92BFFA: (0x20, "rt_intl_wc_teams(esi, 0x20u)"),              # ctor: slot room +0x38
    0x92C04D: (0x20, "(4u * (rt_intl_wc_ko(esi, 7u) + 1u))"),      # ctor: malloc of the stage array
    0x92C063: (0x8, "(rt_intl_wc_ko(esi, 7u) + 1u)"),              # ctor: stage count +0x2c
    0x92D52C: (0x7, "rt_intl_wc_ko(ecx, 7u)"),                     # stage step 0x92d4b0: knockout made at ko
    0x92D316: (0x1C, "(4u * rt_intl_wc_ko(esi, 7u))"),             # vtable +0x30 0x92d310: stage[ko]
    0x92CCF5: (0x7, "rt_intl_wc_ko(ecx, 7u)"),                     # stage news (vtable +0x44): group or knockout
    0x92CD86: (0x1C, "(4u * rt_intl_wc_ko(esi, 7u))"),             # stage news: round record of stage[ko]
    0x92CEA5: (0x7, "rt_intl_wc_ko(ecx, 7u)"),                     # result news (vtable +0x48)
    0x92D1D7: (0x20, "rt_intl_wc_teams(esi, 0x20u)"),              # group A from the draw array (0x92d0f0)
    0x92D5FB: (0x20, "rt_intl_wc_teams(esi, 0x20u)"),              # groups B.. from the draw array (0x92d540)
    0x92E907: (0x7, "rt_intl_wc_ko(esi, 7u)"),                     # run_group_draw 0x92e7f0: League_Stage groups

    # ACN and Asian Cup of 24 teams (0.6.0 slice 27, win/season_intl_cups.c). From 2019 both finals have 6 groups
    # of 4 and a round of 16. african_nations (ctor 0x401000) is the League of group A, stage[0..2] groups B to D,
    # stage[3] the knockout of 8. asia_nations (ctor 0x40e860): group A, stage[0..1] groups B and C, stage[2] the
    # knockout of 8. With 24 teams stage[0..4] are groups B to F and stage[5] the knockout of 16 in both.
    # rt_intl_cup_teams gives the team count, rt_intl_cup_groups the group count of the draw (also the size of a
    # pot), rt_intl_cup_ko the knockout stage index. Each gives the original's value in the base game, for an
    # edition of 16 or 12 teams and for another class (0x79fea6 is shared with the Olympics). this in esi or ecx.
    0x4010AC: (0x10, "(4u * (rt_intl_cup_ko(esi, 3u) + 1u))"),      # ACN ctor: malloc of the stage array
    0x4010C5: (0x4, "(rt_intl_cup_ko(esi, 3u) + 1u)"),              # ACN ctor: stage count +0x2c
    0x401B93: (0xC, "(4u * rt_intl_cup_ko(ecx, 3u))"),              # ACN vtable +0x30: stage[ko]
    0x401BA5: (0x3, "rt_intl_cup_ko(ecx, 3u)"),                     # ACN stage news (vtable +0x44)
    0x401C27: (0xC, "(4u * rt_intl_cup_ko(esi, 3u))"),              # ACN stage news: round record of stage[ko]
    0x79FEA6: (0x3, "rt_intl_cup_ko(ecx, 3u)"),                     # ACN and Olympics result news (vtable +0x48)
    0x401E8C: (0x3, "rt_intl_cup_ko(ecx, 3u)"),                     # ACN stage step 0x401e10: knockout at ko
    0x4022D3: (0x10, "rt_intl_cup_teams(esi, 0x10u)"),              # ACN entrants 0x4022c0: team count +0xf2
    0x40241D: (0x10, "rt_intl_cup_teams(esi, 0x10u)"),              # ACN entrants: enough nations found
    0x40244B: (0x40, "(4u * rt_intl_cup_teams(esi, 0x10u))"),       # ACN draw: malloc of the groups
    0x4024CA: (0x4, "rt_intl_cup_groups(esi, 4u)"),                 # ACN draw: groups
    0x4024DE: (0x4, "rt_intl_cup_groups(esi, 4u)"),                 # ACN draw: random place in a pot
    0x4024FD: (0x4, "rt_intl_cup_groups(esi, 4u)"),                 # ACN draw: random place in a pot, again
    0x402531: (0x4, "rt_intl_cup_groups(esi, 4u)"),                 # ACN draw: next pot
    0x402772: (0x3, "rt_intl_cup_ko(esi, 3u)"),                     # ACN update_for_date 0x4025a0: group stages
    0x40E90A: (0xC, "(4u * (rt_intl_cup_ko_ctor(esi, 2u) + 1u))"),  # Asian Cup ctor: malloc of the stage array
    0x40E923: (0x3, "(rt_intl_cup_ko_ctor(esi, 2u) + 1u)"),         # Asian Cup ctor: stage count +0x2c
    0x40F593: (0x8, "(4u * rt_intl_cup_ko(ecx, 2u))"),              # Asian Cup vtable +0x30: stage[ko]
    0x40F5A5: (0x2, "rt_intl_cup_ko(ecx, 2u)"),                     # Asian Cup stage news (vtable +0x44)
    0x40F627: (0x8, "(4u * rt_intl_cup_ko(esi, 2u))"),              # Asian Cup stage news: round record of stage[ko]
    0x40F136: (0x2, "rt_intl_cup_ko(ecx, 2u)"),                     # Asian Cup result news (vtable +0x48)
    0x40F88F: (0x2, "rt_intl_cup_ko(ecx, 2u)"),                     # Asian Cup stage step 0x40f810: knockout at ko
    0x40FDFE: (0x30, "(4u * rt_intl_cup_teams(esi, 0xcu))"),        # Asian Cup entrants 0x40fdf0: malloc of teams
    0x40FE02: (0xC, "rt_intl_cup_teams(esi, 0xcu)"),                # Asian Cup entrants: team count +0xf2
    0x41001C: (0x30, "(4u * rt_intl_cup_teams(esi, 0xcu))"),        # Asian Cup draw: malloc of the groups
    0x41009B: (0x3, "rt_intl_cup_groups(esi, 3u)"),                 # Asian Cup draw: groups
    0x4100AF: (0x3, "rt_intl_cup_groups(esi, 3u)"),                 # Asian Cup draw: random place in a pot
    0x4100CE: (0x3, "rt_intl_cup_groups(esi, 3u)"),                 # Asian Cup draw: random place in a pot, again
    0x410102: (0x3, "rt_intl_cup_groups(esi, 3u)"),                 # Asian Cup draw: next pot

    # Copa America of 16 teams (0.6.0 slice 27, win/season_intl_copa.c). The Copa America Centenario of 2016 and
    # the 2024 Copa America have 4 groups of 4 and quarter-finals. copa_america (ctor 0x5e0660) is the League of
    # group A, stage[0..1] groups B and C, stage[2] the knockout. With 16 teams stage[0..2] are groups B to D and
    # stage[3] the knockout. Group g takes the teams g, g + groups, ... of +0xee. rt_intl_copa_ko gives the
    # knockout stage index (2), rt_intl_copa_groups the group count (3). Both give the original's value in the
    # base game and for an edition of 12 teams. this in esi or ecx.
    0x5E0710: (0xC, "(4u * (rt_intl_copa_ko(esi, 2u) + 1u))"),     # ctor: malloc of the stage array
    0x5E0729: (0x3, "(rt_intl_copa_ko(esi, 2u) + 1u)"),             # ctor: stage count +0x2c
    0x5E0F56: (0x2, "rt_intl_copa_ko(ecx, 2u)"),                    # result news (vtable +0x48)
    0x5E13B5: (0x2, "rt_intl_copa_ko(ecx, 2u)"),                    # stage news (vtable +0x44)
    0x5E1437: (0x8, "(4u * rt_intl_copa_ko(esi, 2u))"),             # stage news: round record of stage[ko]
    0x5E169F: (0x2, "rt_intl_copa_ko(ecx, 2u)"),                    # stage step 0x5e1620: knockout at ko
    0x5E22EB: (0x2, "rt_intl_copa_ko(esi, 2u)"),                    # update_for_date 0x5e2120: group stages
    0x5E172E: (0xC, "(4u * rt_intl_copa_groups(esi, 3u))"),         # group builder 0x5e16b0: step in +0xee
    0x5E1735: (-0xC, "(0u - 4u * rt_intl_copa_groups(esi, 3u))"),   # group builder: the same step back

    # OFC Nations Cup of 8 teams (0.6.2 open item 45, win/season_intl_ofc.c). OceaniaNations (ctor 0x7988b0) is the
    # League of group A and stage[0] group B, each of 3 teams; with 8 teams each has 4. Group A takes the teams 0 to
    # size - 1 of +0xee, group B the next size. rt_intl_ofc_group gives the size: 4 when the draw holds 8 teams or the
    # one group of 4 (2008), 6 for the one group of 6 (2004, open item 136), else the original's 3, so the original's
    # value in the base game. this in esi.
    0x7995A9: (0xB1, "(0x3bu * rt_intl_ofc_group(esi, 3u))"),       # create_group_table 0x7995a0: malloc of the rows
    0x7995AE: (0x3, "rt_intl_ofc_group(esi, 3u)"),                  # create_group_table: the row count +0x3e
    0x799A80: (0xC, "(4u * rt_intl_ofc_group(esi, 3u))"),           # create_group_stage 0x799a60: malloc of its teams
    0x799AC2: (0xC, "(4u * rt_intl_ofc_group(esi, 3u))"),           # create_group_stage: group B's start in +0xee
    0x799ACB: (0x3, "rt_intl_ofc_group(esi, 3u)"),                  # create_group_stage: the teams it copies
    0x799B74: (0x3, "rt_intl_ofc_group(esi, 3u)"),                  # create_group_stage: League_Stage team count

    # World Cup qualifying 2006 to 2022 (0.6.0 slice 27, win/season_intl_qual.c). C1: the CONCACAF final group
    # (0x917830, this in esi from 0x91784e) gives its 4th the play-off place: the second byte of its rules
    # array (0x917878, 0 in the original; inference: the count of play-off places) becomes 1 in a season game.
    0x917878: (0x0, "rt_wcq_concacaf_playoff(esi)"),

    # Euro qualifying places (0.6.0 slice 27, win/season_intl_euroq.c). euro_champ_qual: in a season game the
    # places follow the Euro year (campaign +0x40 + 2). Each site gives the original's value in the base game and
    # for a Euro of 16 teams. 0x57f9e8 (in 0x57f920) puts back the direct places of a group after the ranking
    # stage: 2 for a Euro of 24 (this in esi). 0x57fb4f (in 0x57fa10) is the place that the ranking takes from
    # each group: the third for Euro 2016 (this in ebp). 0x57d70e (vtable +0x48 0x57d700) is the last group id
    # of the result news: 0x3e9 + the last group stage +0xff (7 in the original, 8 with 10 groups; this in edi).
    0x57F9E8: (0x1, "rt_euroq_direct(esi, 1u)"),
    0x57FB4F: (0x1, "rt_euroq_ranked_place(ebp, 1u)"),
    0x57D70E: (0x3F0, "(0x3e9u + (uint32_t)(int32_t)(int8_t)R8(edi + 0xffu))"),
}

# address: (the instruction in the original, C code)
INSNS = {
    # The 2002 World Cup qualifying data of Europe (load_2002_campaign 0x91b760) and South America (0x9216d0).
    # The constructors call it when the start year is not before the campaign (cmp at 0x91846f, 0x9204b4). In a
    # 2001 game the campaign is 2004, so the original never calls it (doc table 7). For a later start year the
    # data would go into a later campaign, and its asserts (0x91dbc4, 0x9223ab) want dates in 2000 and 2001.
    0x918473: ("jl 0x91847c", "if (cc_l(F) || rt_season_year != 2001u) goto L_91847c;"),
    0x9204B8: ("jl 0x9204c1", "if (cc_l(F) || rt_season_year != 2001u) goto L_9204c1;"),
    # Slice 26 (season_kind_club_world.c). WorldClubChamp::WorldClubChamp 0x929140: the first edition loop adds 1
    # to an even year (0x9291b0); in a season game it keeps the year that the runtime gives the constructor.
    0x9291B0: ("je 0x9291b8", "if (cc_e(F) && !rt_club_world_annual()) goto L_9291b8;"),
    # ConmebolMerc::update_for_date 0x4c5c90: no start_new_season on 1 June while the cup waits for a later
    # edition (the original skips it when +0x40 is the game year).
    0x4C5CDE: ("je 0x4c5ce7", "if (cc_e(F) || rt_club_world_waits(esi)) goto L_4c5ce7;"),

    # International finals (0.6.0 slice 27, docs/research/2026-10-18-international.md). Each class keeps its
    # edition year in the word +0x40 of the object (this in esi). The constructor rounds the game year up to the
    # class's cycle; the first instruction after that loop also gets the year of rt_intl_first_year (the first
    # edition of the table from the start season, win/season_intl_rules.c). start_new_season adds the step;
    # rt_intl_next_year gives the next edition of the table. The base game keeps the original's values. The
    # first argument is the class (season_intl.h): 0 World Cup, 1 European Championship, 2 Confederations Cup,
    # 3 Copa America, 4 African Cup of Nations, 5 Asian Cup, 6 Oceania Nations Cup, 7 Olympics. No instruction
    # after a step reads its flags before another instruction sets them.
    0x92BFEE: ("push 6", "W16(esi + 0x40u, rt_intl_first_year(0u, R16(esi + 0x40u))); esp -= 4; W32(esp, 6u);"),  # world_cup ctor 0x92bf50
    0x57A92B: ("push 1", "W16(esi + 0x40u, rt_intl_first_year(1u, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # euro_champ ctor 0x57a890
    0x58B392: ("push 1", "W16(esi + 0x40u, rt_intl_first_year(2u, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # fifa_confed ctor 0x58b310
    0x5E06FB: ("push 1", "W16(esi + 0x40u, rt_copa_first_year(esi, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # copa_america ctor 0x5e0660
    0x401097: ("push 1", "W16(esi + 0x40u, rt_intl_first_year(4u, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # african_nations ctor 0x401000
    0x40E8F5: ("push 1", "W16(esi + 0x40u, rt_intl_first_year(5u, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # asia_nations ctor 0x40e860
    0x798952: ("push 1", "W16(esi + 0x40u, rt_intl_first_year(6u, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # oceania_nations ctor 0x7988b0
    0x79F5C5: ("push 1", "W16(esi + 0x40u, rt_intl_first_year(7u, R16(esi + 0x40u))); esp -= 4; W32(esp, 1u);"),  # olympics ctor 0x79f530
    0x92D409: ("add word ptr [esi + 0x40], 4", "W16(esi + 0x40u, rt_intl_next_year(0u, R16(esi + 0x40u), 4u));"),  # world_cup::start_new_season 0x92d330
    0x57B7CA: ("add word ptr [esi + 0x40], 4", "W16(esi + 0x40u, rt_intl_next_year(1u, R16(esi + 0x40u), 4u));"),  # euro_champ 0x57b750
    0x58C050: ("add word ptr [esi + 0x40], 2", "W16(esi + 0x40u, rt_intl_next_year(2u, R16(esi + 0x40u), 2u));"),  # fifa_confed 0x58bff0
    0x5E1590: ("add word ptr [esi + 0x40], 2", "W16(esi + 0x40u, rt_copa_next_year(esi, R16(esi + 0x40u), 2u));"),  # copa_america 0x5e1530
    0x401D80: ("add word ptr [esi + 0x40], 2", "W16(esi + 0x40u, rt_intl_next_year(4u, R16(esi + 0x40u), 2u));"),  # african_nations 0x401d20
    0x40F780: ("add word ptr [esi + 0x40], 4", "W16(esi + 0x40u, rt_intl_next_year(5u, R16(esi + 0x40u), 4u));"),  # asia_nations 0x40f720
    0x799940: ("add word ptr [esi + 0x40], 2", "W16(esi + 0x40u, rt_intl_next_year(6u, R16(esi + 0x40u), 2u));"),  # oceania_nations 0x7998e0
    0x7A04B0: ("add word ptr [esi + 0x40], 4", "W16(esi + 0x40u, rt_intl_next_year(7u, R16(esi + 0x40u), 4u));"),  # olympics 0x7a0450

    # The World Cup holder (0.6.0 slice 27): from the 2006 World Cup the holder qualifies like the others
    # (int-world). Each qualifying section asks for the holder (comp_util_get_latest_placed_club 0x4afd80, place
    # 0 of the World Cup history) and leaves it out of its pool; rt_intl_wc_holder gives no holder in a season
    # game, at the instruction after each call. The sections of a 2001 game play the 2004 campaign (Europe,
    # South America, Asia) or the 2000 campaign, whose holder France is in none of their pools.
    0x919F47: ("add esp, 8", "eax = rt_intl_wc_holder(eax); esp += 8u;"),  # wc_europe_league
    0x920E38: ("add esp, 8", "eax = rt_intl_wc_holder(eax); esp += 8u;"),  # wc_south_american_league
    0x920F18: ("mov dword ptr [esp + 0x20], eax", "eax = rt_intl_wc_holder(eax); W32(esp + 0x20u, eax);"),  # wc_south_american_league, its pool
    0x9107A6: ("add esp, 8", "eax = rt_intl_wc_holder(eax); esp += 8u;"),  # wc_african_cup
    0x914077: ("add esp, 8", "eax = rt_intl_wc_holder(eax); esp += 8u;"),  # wc_asia_league
    0x916394: ("mov edx, eax", "eax = rt_wcq26_concacaf_exempt(esi, rt_intl_wc_holder(eax)); edx = eax;"),  # slice 29: the third host  # wc_concacaf_cup
    0x91FB98: ("add esp, 8", "eax = rt_intl_wc_holder(eax); esp += 8u;"),  # wc_oceania_league
    # world_cup build_team_slots 0x92da40 puts the holder in slot 0 (0x92da86) and counts it (0x92da89). From
    # 2006 the slot is not counted, so the first host takes slot 0; slots 1 and 2 are cleared, because the
    # checks at 0x92db02 and 0x92db57 compare slot 0 with them.
    0x92DA89: ("inc word ptr [esi + 0x36]", "if (rt_intl_wc_holder_slot(R16(esi + 0x40u))) W16(esi + 0x36u, R16(esi + 0x36u) + 1u); else { W32(R32(esi + 0x14u) + 6u, 0u); W32(R32(esi + 0x14u) + 0xcu, 0u); }"),
    # The World Cup Oceania section (wc_oceania_league, pool at 0x91fc66..0x91fcde) takes exactly 10 entrants
    # (0x91fbda) and leaves out New Caledonia (key nation 0x9cf530) and Papua New Guinea (0x9cf41c). With
    # Australia in Asia (win/season_intl.c) New Caledonia comes in, so the count of 10 stays.
    0x91FC89: ("mov ecx, dword ptr [0x9cf530]", "ecx = rt_intl_oceania_left_out(R32(0x9cf530u));"),
    # EuroChamp build_team_slots 0x57bd80: no host from find_hosts (0x57be34) leads to the box "euro_champ.cpp
    # 837" (0x57becc..0x57bf22). Euro 2020 and 2028 have no host place in the table, so in a season game the
    # builder goes on to the default nations (0x57bf2f) with no host (win/season_intl.c).
    # 0.6.2 slice 5 (win/season_intl.c). HostCountry::daily_update 0x5fa850 stores the draw of a host row 5 years
    # before its finals (random(3) at 0x5faaa0): a pair that names a nation that no longer exists is drawn again
    # (Q6, Q32). Cup::record_season_honours 0x519a90 writes no host for the classes whose slot +0x88 gives 0: at
    # 0x519c2f a season game takes the edition's first host (open item 18).
    0x5FAAAE: ("mov byte ptr [edx + 0x1e], al", "SYNC(); W8(edx + 0x1eu, rt_intl_host_pick(edx, eax & 0xffu));"),
    0x519C2F: ("mov ecx, dword ptr [esp + 0x10]", "SYNC(); ebx = rt_intl_history_host(esi, ebx); ecx = R32(esp + 0x10u);"),
    # Issue 146: the news of the qualified nations names the hosts of its own edition (win/season_intl.c), when it is
    # made and when its text is written (ebp part 0 the competition, edi and ebx parts 4 and 5, eax part 6 the year).
    0x777D07: ("mov dword ptr [esp + 0x24], eax", "W32(esp + 0x24u, eax); SYNC(); edi = rt_intl_news_text_host(ebp, eax, edi, 0u); ebx = rt_intl_news_text_host(ebp, eax, ebx, 1u);"),
    0x777B07: ("mov eax, dword ptr [esp + 0x18]", "SYNC(); rt_intl_news_hosts(R32(edi), esp + 0x18u, esp + 0x1cu); eax = R32(esp + 0x18u);"),
    0x57BE3D: ("jle 0x57becc", "if (cc_le(F)) { if (rt_intl_euro_no_host()) goto L_57bf2f; goto L_57becc; }"),
    # EuroChampQual::get_group_team_count 0x57d9d0 gives the size of each qualifying group by the hosts that the
    # pool leaves out (+0xf6): 1 (0x57d9de) or 2 (0x57d9e5), else the box "euro_champ_qual.cpp 581" (0x57da3d).
    # A Euro without a host place (2020, 2028) has 0: in a season game it goes to the branch of 1 host, whose
    # sizes (0x57da6e, 0x57da8e, 0x57da9b) rt_euroq_group_size gives from the pool for the formats of 24 teams.
    0x57D9DE: ("je 0x57da7b", "if (cc_e(F) || (rt_intl_euro_no_host() && R8(ecx + 0xf6u) == 0u)) goto L_57da7b;"),

    # Euro of 24 teams (0.6.0 slice 27, win/season_intl_euro.c). start_new_season 0x57b750 keeps the stage array
    # of the ctor (+0xc, count +0x2c). After the step (0x57b7ca) an edition of another size gets a new array.
    # The flags of "or eax, -1" are not read: the next instructions only move, and add esp at 0x57b7e1 sets them.
    0x57B7CF: ("or eax, 0xffffffff", "SYNC(); rt_intl_euro_stages(esi); eax = 0xffffffffu; SETF(K_RES | SZ4, 0, 0, eax, 0);"),

    # World Cup qualifying from the 2026 cycle (0.6.0 slice 29, win/season_wcq26.c). 0x919f90 (pool 0x919e30,
    # this in esi): after the pool size, the shape of no exempt nation (+0xf6 0). 0x91785c (final group 0x917830,
    # esi): the direct places are 3 less the hosts; in the gate the hosts are not taken off. The flags of that
    # sub are not read: 0x91785e..0x917882 only move, `sub bl, 3` at 0x917887 sets them.
    0x919F90: ("mov dword ptr [esi + 0xf2], eax", "W32(esi + 0xf2u, eax); rt_wcq26_europe_format(esi);"),
    0x91785C: ("sub al, cl", "t0 = (eax & 0xffu); t1 = rt_wcq26_concacaf_hosts(esi, ecx & 0xffu) & 0xffu; t2 = ((t0 - t1) & 0xffu); SETF(K_SUB | SZ1, t0, t1, t2, 0); eax = (eax & 0xffffff00u) | t2;"),
    0x910A4A: ("mov byte ptr [esp + 0x13], al", "W8(esp + 0x13u, rt_wcq26_africa_groups(esi, eax & 0xffu));"),
    0x910A4E: ("mov dword ptr [esp + 0x18], eax", "W32(esp + 0x18u, rt_caf_group_size(esi, eax));"),  # item 199
    # item 199: set_club_progress 0x9115a0 (this in esi), group position 6 of a group of 6 (season_caf_pool.c)
    0x911600: ("mov bl, byte ptr [esp + 0x18]", "ebx = (ebx & 0xffffff00u) | (rt_caf_band(esi, R8(esp + 0x10u), R8(esp + 0x18u)) & 0xffu);"),
    0x911631: ("mov dl, byte ptr [esp + 0x1c]", "edx = (edx & 0xffffff00u) | (rt_caf_band(esi, R8(esp + 0x10u), R8(esp + 0x1cu)) & 0xffu);"),
    0x911A64: ("mov cl, byte ptr [esp + 0x14]", "ecx = (ecx & 0xffffff00u) | (rt_caf_band(esi, R8(esp + 0x10u), R8(esp + 0x14u)) & 0xffu);"),

    # World Cup of 48 teams (0.6.0 slice 29, win/season_wc48.c). start_new_season 0x92d330 keeps the stage array
    # and the slot array of the ctor (+0xc, count +0x2c; +0x14, room +0x38). After the step (0x92d409) an edition
    # of another size gets new arrays. The flags of "or eax, -1" are not read: 0x92d411..0x92d419 only move, then
    # the call of build_team_slots 0x92da40.
    0x92D40E: ("or eax, 0xffffffff", "SYNC(); rt_intl_wc_stages(esi); eax = 0xffffffffu; SETF(K_RES | SZ4, 0, 0, eax, 0);"),

    # ACN and Asian Cup of 24 teams (0.6.0 slice 27, win/season_intl_cups.c). start_new_season (ACN 0x401d20,
    # Asian Cup 0x40f720) keeps the stage array of the ctor. After the step (0x401d80, 0x40f780) an edition of
    # another size gets a new array. The instruction after the step only moves this to ecx.
    0x401D85: ("mov ecx, esi", "SYNC(); rt_intl_cup_stages(esi); ecx = esi;"),
    0x40F785: ("mov ecx, esi", "SYNC(); rt_intl_cup_stages(esi); ecx = esi;"),
    # 0.6.2 open items 44 and 46 (win/season_intl_cups.c): every host plays. The entrants take the two hosts of
    # find_hosts first (ACN 0x402338, Asian Cup 0x40fea7). The hosts after the second (2027 ACN: Uganda; 2007
    # Asian Cup: Thailand, Vietnam) join them before the ACN sort (0x402446, its start dl and the count edi) and
    # before the Asian Cup pick from the list on the stack (0x4afbd0 at 0x40ffc8: list esp + 0x228, count ebp and
    # [esp + 0x14], hosts in +0xee ebx). [esp + 0x13] keeps the host count dl in both (0x40234c, 0x40feb5). The
    # replaced instructions only move; the flags are set again before they are read (0x40243d, 0x40ffbf).
    0x40242F: ("mov ecx, dword ptr [esi + 0xee]", "SYNC(); t0 = rt_intl_cup_acn_hosts(esi, edi, edx & 0xffu); edi = t0 >> 8; edx = (edx & 0xffffff00u) | ((edx + t0) & 0xffu); W8(esp + 0x13u, edx & 0xffu); ecx = R32(esi + 0xeeu);"),
    0x40FFB2: ("mov cl, byte ptr [esi + 0xf2]", "SYNC(); t0 = rt_intl_cup_asian_hosts(esi, esp + 0x228u, ebp, ebx); t1 = t0 & 0xffu; ebp = t0 >> 8; W32(esp + 0x14u, ebp); ebx += t1; edx = (edx & 0xffffff00u) | ((edx + t1) & 0xffu); W8(esp + 0x13u, edx & 0xffu); ecx = (ecx & 0xffffff00u) | R8(esi + 0xf2u);"),
    # GitHub issue 136 (win/season_intl_cups.c): 0x4afbd0 sorts the Asian list by reputation and shuffles the top
    # 3 * places, so the Asian Cup took weak sides at random. The site calls the original (the same random numbers)
    # and sorts the list again: the best sides play, in every game. Args list, count, places on the stack (pushed at
    # 0x40ffc5..0x40ffc7); the caller reads no register the call leaves (0x40ffcd, 0x40ffd2).
    0x40FFC8: ("call 0x4afbd0", "SYNC(); rt_intl_cup_asian_pick(esi, R32(esp), R32(esp + 4u), R32(esp + 8u)); RT_AFTER_CALL();"),

    # Copa America of 16 teams (0.6.0 slice 27, win/season_intl_copa.c). Group A (0x5e11e0) takes the teams
    # 3 * k of +0xee: 4 * k with 16 teams. start_new_season 0x5e1530 keeps the stage array of the ctor: after
    # the step (0x5e1590) an edition of another size gets a new array. The instruction only moves this to ecx.
    0x5E128E: ("lea ecx, [eax + eax*2]", "ecx = eax * rt_intl_copa_groups(esi, 3u);"),
    0x5E1595: ("mov ecx, esi", "SYNC(); rt_intl_copa_stages(esi); ecx = esi;"),
    # EuroChampQual (0.6.0 slice 27): the hosts that the qualifying pool leaves out (+0xf6) are counted at
    # 0x57f6b7..0x57f6e4 and may be 0 to 2 (box "euro_champ_qual.cpp 1245" at 0x57f6fa only above 2); the pool
    # is 0x33 - count (0x57f76f). A Euro without a host place (2020, 2028) keeps 0 and the whole pool of 51 (O260
    # A13): its group sizes come from the pool (0x57d9de below, rt_euroq_group_size) and its play-off from
    # rt_euroq_hosts (0x57d4f3: 8 teams), so 10 groups give 20 and the play-off 4: the 24 places, no host.

    # World Cup qualifying 2006 to 2022 (0.6.0 slice 27, win/season_intl_qual.c). Gate: a season game and the
    # World Cup year (campaign +0x40 + 2) in 2006 to 2022; outside it each site gives the original's value.
    # E1: the European play-off (template 0x918920 and builder 0x919910, this in edi) is sized by the exempt
    # count +0xf6: 0 or 1 give 8 teams, else 6. With no European host it becomes 6 (3 places, 10 + 3 = 13).
    0x918923: ("mov al, byte ptr [edi + 0xf6]", "eax = (eax & 0xffffff00u) | (rt_wcq_europe_playoff(edi, R8(edi + 0xf6u)) & 0xffu);"),
    0x919A7E: ("mov al, byte ptr [edi + 0xf6]", "eax = (eax & 0xffffff00u) | (rt_wcq_europe_playoff(edi, R8(edi + 0xf6u)) & 0xffu);"),
    # E2, E3: the play-off takes N of the N + 1 best runners-up (ebx = N + 1, pushed at 0x919b4e); the one at the
    # random index (0x90cfc0, cdecl) goes to the Asian tie (0x919c10). In a season game the index is the last
    # (the weakest is out) and no runner-up is kept for Asia ([esp + 0x28] 0, so 0x919bec never matches and the
    # team gets the news of 0x919c45). The add esp, 4 at 0x919b58 sets the flags.
    0x919B4F: ("call 0x90cfc0", "if (rt_wcq_europe_no_asia(edi)) eax = ebx - 1u; else { esp -= 4; W32(esp, 0x919b54u); SYNC(); f_0090cfc0(); LOAD(); RT_AFTER_CALL(); }"),
    0x919B5E: ("mov dword ptr [esp + 0x28], ecx", "W32(esp + 0x28u, rt_wcq_europe_no_asia(edi) ? 0u : ecx);"),
    # A1: the Asian pool builder 0x913f60 stores its kept count; after it the exempt count +0xee becomes 0, so
    # the final groups (0x913740), the play-off (0x913bc1) and the statuses (0x912642) take the format of no
    # host. The flags are not read before pop and ret.
    0x9141EA: ("mov dword ptr [esi + 0xf3], ecx", "W32(esi + 0xf3u, ecx); rt_wcq_asia_format(esi); rt_wcq26_asia_format(esi);"),
    # World Cup qualifying from the 2026 cycle (0.6.2 slice 5, open item 59, win/season_wcq26.c): the AFC and OFC
    # pools leave out every host. The count of hosts and holder +0xee that the pool size takes off (Asia 44 -
    # count, Oceania 10 - count) gets the hosts that the pool wraps left out (this in esi). Final groups 0x913740:
    # after group A is made the status counts at [esp + 0x10] get group B's direct places (this in esi; the
    # stack holds no push here, 0x913983). The flags of none of them are read.
    0x9140B2: ("movsx edx, byte ptr [esi + 0xee]", "edx = (uint32_t)(int32_t)(int8_t)rt_wcq26_pool_exempt(esi, R8(esi + 0xeeu));"),
    0x91FBD3: ("movsx edx, byte ptr [esi + 0xee]", "edx = (uint32_t)(int32_t)(int8_t)rt_wcq26_pool_exempt(esi, R8(esi + 0xeeu));"),
    0x9139C0: ("mov edi, ebx", "edi = ebx; W8(esp + 0x10u, rt_wcq26_asia_direct_b(esi, R8(esp + 0x10u)));"),
    # A2: Asia stage 12 (0x913d50, this in esi, the two teams at [ebp]) takes its partner from the European
    # section (0x91def0, ecx the Europe object). The partner of the cycle replaces it; the European runner-up
    # stays the fallback.
    0x913E42: ("call 0x91def0", "esp -= 4; W32(esp, 0x913e47u); SYNC(); f_0091def0(); LOAD(); RT_AFTER_CALL(); eax = rt_wcq_asia_partner(esi, ebp, eax);"),
    # O1: Oceania stage 2 (0x91f800, this in esi) found ebp teams in the array ebx: the OFC winner and the
    # South American play-off team. The pair of the cycle replaces them, then the compare with 2.
    0x91F8E9: ("cmp ebp, 2", "ebp = rt_wcq_oceania_pair(esi, ebx, ebp); t0 = ebp; t1 = 0x2u; t2 = t0 - t1; SETF(K_SUB | SZ4, t0, t1, t2, 0);"),
    # S1: the South American direct places +0xbe = 4 - ef - ee (0x921098..0x9210ad, this in esi). A host
    # (Brazil 2014) does not take a direct place in a season game: 4 direct places with a pool of 9.
    0x9210A2: ("sub cl, dl", "t0 = (ecx & 0xffu); t1 = rt_wcq_south_america_hosts(esi, edx & 0xffu) & 0xffu; t2 = ((t0 - t1) & 0xffu); SETF(K_SUB | SZ1, t0, t1, t2, 0); ecx = (ecx & 0xffffff00u) | t2;"),
    # F1: the African pool builder 0x9106a0 (this in esi) compares its count ebx with the pool size +0xb6. Before
    # the compare a pool of 50 or more takes the format of no host (South Africa 2010 stays out of the pool by
    # the compare at 0x9108aa).
    0x9108D8: ("cmp ebx, dword ptr [esi + 0xb6]", "rt_wcq_africa_format(esi, ebx); rt_wcq26_africa_format(esi); rt_caf_pool(esi, ebx); t0 = ebx; t1 = R32(esi + 0xb6u); t2 = t0 - t1; SETF(K_SUB | SZ4, t0, t1, t2, 0);"),

    # UEFA Nations League (0.6.0 slice 28, season_nl.c). EuroChampQual::update_for_date 0x57fd60 makes the group
    # stages 0 to 7 on its draw day (create_group_stage 0x57ed10 at 0x57ff2e, `cmp edi, 8`, this in esi). The
    # Nations League object makes one stage for each group after group one: G - 1. Other objects: 8.
    0x57FF34: ("cmp edi, 8", "t0 = edi; t1 = rt_nl_group_stages(esi, 8u); t2 = t0 - t1; SETF(K_SUB | SZ4, t0, t1, t2, 0);"),
    # Euro qualifying places (0.6.0 slice 27, win/season_intl_euroq.c). The places and stage indices of the
    # format: after set_up_rules sets +0xbf (0x57e963) and at the ctor epilogue (0x57d03b, also after a load: the
    # bytes +0xff..+0x101 are not saved). The play-off size by the host count +0xf6: 0x57f014 (best places),
    # 0x57f115 (play-off teams), both in 0x57ef50, and 0x57d4f3 (play-off template, in 0x57d190): Euro 2016 takes
    # the two-host shape (1 best third, 8 in the play-off). The group sizes of 0x57d9d0 (0x57da6e, 0x57da8e,
    # 0x57da9b; ecx this, the group at [esp + 0x204]): with 24 teams the pool count over the groups, so the
    # places sum to the pool. The draw 0x57dab0 sets the order and counts of 9 groups: 0x57db30 sets entry 9 of
    # the order ([esp + 0x21]) and its count ([esp + 0x2d]) for 10 groups (this in esi). With 10 groups there
    # is no ranking stage: 0x57ff50 and 0x57ff9c (in update_for_date 0x57fd60, this in esi) skip 0x57f920 and
    # 0x57fa10. The log of each qualifier before the add (0x57d605 group or best place, 0x57d69c play-off; the
    # Euro is [0xadadfc][key 0x9cf7a4], the stage at [esp + 0x10]). The flags of the replaced instructions are not
    # read before another instruction sets them.
    0x57E963: ("mov byte ptr [esi + 0xbf], 1", "W8(esi + 0xbfu, 1u); rt_euroq_rules(esi);"),
    0x57D03B: ("mov ecx, dword ptr [esp + 0x214]", "rt_euroq_rules(esi); ecx = R32(esp + 0x214u);"),
    0x57F014: ("mov al, byte ptr [esi + 0xf6]", "eax = (eax & 0xffffff00u) | (rt_euroq_hosts(esi, R8(esi + 0xf6u)) & 0xffu);"),
    0x57F115: ("mov al, byte ptr [esi + 0xf6]", "eax = (eax & 0xffffff00u) | (rt_euroq_hosts(esi, R8(esi + 0xf6u)) & 0xffu);"),
    0x57D4F3: ("mov al, byte ptr [edi + 0xf6]", "eax = (eax & 0xffffff00u) | (rt_euroq_hosts(edi, R8(edi + 0xf6u)) & 0xffu);"),
    0x57DA6E: ("mov ax, 5", "eax = (eax & 0xffff0000u) | (rt_euroq_group_size(ecx, R8(esp + 0x204u), 5u) & 0xffffu);"),
    0x57DA8E: ("mov ax, 5", "eax = (eax & 0xffff0000u) | (rt_euroq_group_size(ecx, R8(esp + 0x204u), 5u) & 0xffffu);"),
    0x57DA9B: ("mov ax, 6", "eax = (eax & 0xffff0000u) | (rt_euroq_group_size(ecx, R8(esp + 0x204u), 6u) & 0xffffu);"),
    0x57DB30: ("mov byte ptr [esp + 0x14], 0", "W8(esp + 0x14u, 0u); if (rt_euroq_ten_groups(esi)) { W8(esp + 0x21u, 9u); W8(esp + 0x2du, 0u); }"),
    0x57FF50: ("call 0x57f920", "if (!rt_euroq_no_ranking(esi)) { esp -= 4; W32(esp, 0x57ff55u); SYNC(); f_0057f920(); LOAD(); RT_AFTER_CALL(); }"),
    0x57FF9C: ("call 0x57fa10", "if (!rt_euroq_no_ranking(esi)) { esp -= 4; W32(esp, 0x57ffa1u); SYNC(); f_0057fa10(); LOAD(); RT_AFTER_CALL(); }"),
    0x57D605: ("push esi", "rt_euroq_log(R32(eax + edx * 4u), esi, R32(esp + 0x10u), 0u); esp -= 4; W32(esp, esi);"),
    0x57D69C: ("push esi", "rt_euroq_log(R32(ecx + eax * 4u), esi, R32(esp + 0x10u), 1u); esp -= 4; W32(esp, esi);"),
    # EuroChampQual ctor 0x57ce10 (0.6.0 slice 27): when the game year is past the campaign year (0x57d01a) it
    # loads the real groups, fixtures and results of the 2002 campaign (load_2002_campaign 0x580bd0, at 0x57d025:
    # its fixed list of 49 nations and the results of 2002 and 2003). That is right only for the 2002 campaign
    # (a 2002/03 or 2003/04 start). A later start (2019: campaign 2018 of 10 groups) loaded the 2002 data into
    # its own campaign and stopped at the box "euro_champ_qual.cpp 2358" (0x58325b). rt_euroq_skip_2002 skips
    # the load only in a season game (season_intl_on): the base game keeps the original's path, also when a
    # save of a later year is loaded (the ctor runs with the game year of the save).
    0x57D021: ("jle 0x57d02a", "if (cc_le(F) || rt_euroq_skip_2002(esi)) goto L_57d02a;"),
    # VIRTUAL_STAFF top-up 0x90dae0 (0.6.2 fault 65, win/season_national_fill.c): an under-21 side of a nation with
    # a reputation above 500 gets no virtual players (0x90dd5d..0x90dd73). In a season game such a side whose
    # nation has fewer players it can field than the squad check wants keeps them (the jump past the zeroing).
    0x90DD67: ("jle 0x90dd97", "if (cc_le(F) || rt_u21_keeps_virtual(esi)) goto L_90dd97;"),
}
