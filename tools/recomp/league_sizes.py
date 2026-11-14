"""League sizes of the original that become run-time values (0.6.0 slice 18).

Each league class of one division sets its club count in create_table with one store,
`mov word ptr [reg + 0x3e], imm` (League::check_complete 0x689b3d reads +0x3e), and then takes the clubs whose
division (club +0x57) is its key id, in the order of the club table, up to that count (EngConf::create_table
0x56ecb0 at 0x56ed54..0x56ed7f; with fewer the table is not made, 0x56ed95..0x56edab). The runtime replaces
the immediate with rt_league_clubs(object, immediate) (src/recomp_rt/win/season_sizes.c): the original's
count in the base game, the count of the season in a season game.

CLASSES: one row per class: (class, source file, set_up_rules address, [create_table store addresses]).
The stores come from the translated code of each create_table (one store, or one per path where the class has
a year branch: FraFirst 0x5bc2bd, PolSecond 0x7cbdfd, ScoPrm 0x7f2820, NorFirst 0x7909a0). A class with no
store takes the count of the data (IreFirst 0x63591f, IrePrm 0x63a74f, PolFirst 0x7c9469 store a register), so
it has only its set_up_rules row. set_up_rules (vtable +0x8c) writes the promotion and relegation places
+0xbe..+0xc1; the runtime wraps it (season_size_wraps.c, made by tools/seasons/make_size_wraps.py) and sets the
places of the season after it. Evidence: docs/research/2026-10-18-league-sizes.md.

The group classes (BelThird, FraLower, SpaLower, SpaSecondB, PorSecondB, SweSecond, TurSecondB, ItaSerC2a..c,
BraNatThird, GerRegional), ArgSecond (25 clubs with a fix-up of the 25th at 0x407dc0 and zones that read
fixed rows, 0x4082fd and 0x4083d5: a 2010/11 start with 20 stopped at arg_second.cpp 85), DenSecond (one division of 16, while the real 2nd Divisions
are groups from 2005/06 with rows of 28 to 32 clubs and the places of the groups), FinFirst (the North and South groups are two objects of one class, 0x593a82, while
the table holds one division from 2002) and UsaMls (three groups of 4, season_mls.c) are not here: their sizes are groups.
"""

CLASSES = [
    ("ArgPrm", "arg_prm", 0x405670, [(0x405552, "esi", 20)]),
    ("AusNsl", "aus_nsl", 0x411CE0, [(0x411B82, "edi", 13)]),
    ("BelFirst", "bel_first", 0x41E9E0, [(0x41E8C2, "esi", 18)]),
    ("BelSecond", "bel_second", 0x41FEA0, [(0x41FD82, "esi", 18)]),
    ("BraNatFirst", "bra_nat_first", 0x42A5A0, [(0x42A492, "edi", 28)]),
    ("BraNatSecond", "bra_nat_second", 0x42CB70, [(0x42CA82, "edi", 28)]),
    ("CroA1", "cro_a1", 0x5148E0, [(0x5147C2, "esi", 16)]),
    ("CroA2a", "cro_a2a", 0x515DE0, [(0x515CC2, "esi", 16)]),
    ("CroA2b", "cro_a2b", 0x516F00, [(0x516DE2, "esi", 16)]),
    ("DenFirst", "den_first", 0x5523B0, [(0x5522A2, "esi", 16)]),
    ("DenPrm", "den_prm", 0x553A00, [(0x5538E2, "esi", 12)]),
    ("EngConf", "eng_conf", 0x56EDE0, [(0x56ECC2, "esi", 22)]),
    ("EngFirst", "eng_first", 0x572D90, [(0x572C01, "esi", 24)]),
    ("EngPrm", "eng_prm", 0x574B70, [(0x574A32, "esi", 20)]),
    ("EngSecond", "eng_second", 0x576780, [(0x576642, "esi", 24)]),
    ("EngThird", "eng_third", 0x5780C0, [(0x577F82, "esi", 24)]),
    ("FinPrm", "fin_prm", 0x595C90, [(0x595B82, "esi", 12)]),
    ("FraFirst", "fra_first", 0x5BC410, [(0x5BC2C5, "esi", 18), (0x5BC2CD, "esi", 20)]),
    ("FraSecond", "fra_second", 0x5BF0A0, [(0x5BEF21, "esi", 20)]),
    ("FraThird", "fra_third", 0x5C0700, [(0x5C05E2, "esi", 20)]),
    ("GerFirst", "ger_first", 0x5DB400, [(0x5DB244, "edi", 18)]),
    ("GerSecond", "ger_second", 0x5DF3E0, [(0x5DF2C2, "esi", 18)]),
    ("GrePrm", "gre_prm", 0x5EAE90, [(0x5EAD72, "esi", 14)]),
    ("GreSecond", "gre_second", 0x5EBDE0, [(0x5EBCC2, "esi", 14)]),
    ("HolFirst", "hol_first", 0x5F6340, [(0x5F6252, "edi", 18)]),
    ("HolPrm", "hol_prm", 0x5F8560, [(0x5F8442, "esi", 18)]),
    ("IreFirst", "ire_first", 0x635AA0, []),
    ("IrePrm", "ire_prm", 0x63A900, []),
    ("ItaSerA", "ita_ser_a", 0x641640, [(0x641522, "esi", 18)]),
    ("ItaSerB", "ita_ser_b", 0x6468A0, [(0x646792, "esi", 20)]),
    ("ItaSerC1a", "ita_ser_c1a", 0x64B770, [(0x64B682, "edi", 18)]),
    ("ItaSerC1b", "ita_ser_c1b", 0x64F920, [(0x64F832, "edi", 18)]),
    ("JapJ1", "jap_j1", 0x663620, [(0x6634F2, "edi", 16)]),
    ("JapJ2", "jap_j2", 0x665140, [(0x665042, "edi", 12)]),
    ("KorLeague", "kor_league", 0x66BCC0, [(0x66BBB2, "esi", 10)]),
    ("NirFirst", "nir_first", 0x78BD70, [(0x78BC62, "esi", 10)]),
    ("NirPrm", "nir_prm", 0x78ED80, [(0x78EC72, "esi", 10)]),
    ("NorFirst", "nor_first", 0x790B00, [(0x7909B1, "edi", 16), (0x790A73, "edi", 14)]),
    ("NorPrm", "nor_prm", 0x792700, [(0x7925F2, "esi", 14)]),
    ("PolFirst", "pol_first", 0x7C9690, []),
    ("PolSecond", "pol_second", 0x7CBF40, [(0x7CBE03, "esi", 20), (0x7CBE14, "esi", 18)]),
    ("PorPrm", "por_prm", 0x7CF930, [(0x7CF812, "esi", 18)]),
    ("PorSecond", "por_second", 0x7D16A0, [(0x7D1532, "edi", 18)]),
    ("RusFirst", "rus_first", 0x7EA810, [(0x7EA6D3, "ebp", 18)]),
    ("RusPrm", "rus_prm", 0x7EB740, [(0x7EB622, "esi", 16)]),
    ("ScoFirst", "sco_first", 0x7EEE50, [(0x7EED42, "esi", 10)]),
    ("ScoPrm", "sco_prm", 0x7F2970, [(0x7F2835, "esi", 10), (0x7F283D, "esi", 12)]),
    ("ScoSecond", "sco_second", 0x7F3F80, [(0x7F3E92, "edi", 10)]),
    ("ScoThird", "sco_third", 0x7F4F20, [(0x7F4E32, "edi", 10)]),
    ("SpaFirst", "spa_first", 0x84FDF0, [(0x84FC82, "edi", 20)]),
    ("SpaSecond", "spa_second", 0x855CC0, [(0x855B52, "edi", 22)]),
    ("SweFirst", "swe_first", 0x88F090, [(0x88EFA2, "edi", 16)]),
    ("SwePrm", "swe_prm", 0x890550, [(0x890442, "esi", 14)]),
    ("TurFirst", "tur_first", 0x8FDF90, [(0x8FDDD4, "ebp", 18)]),
    ("TurSecond", "tur_second", 0x8FF280, [(0x8FF162, "esi", 20)]),
    ("WelFirst", "wel_first", 0x925C50, [(0x925B42, "esi", 18)]),
]


def _imm(n):
    return f"{n:#x}" if n > 9 else str(n)


# The largest count that a class holds: its largest constant. Most classes allocate the table for that count
# (push n x 0x3b before the store, for example 0x574a2b for EngPrm) and their date template has exactly the
# rounds of that count (create_schedule, for example D = 38 at 0x57369a for EngPrm). A bigger count needs new
# code (docs/research/2026-10-18-league-sizes.md, "Leagues whose size change the code cannot hold").
CAPS = {f: max(n for _a, _r, n in stores) for _c, f, _u, stores in CLASSES if stores}

# The French classes play the second half from the middle of their date template (league_next_schedule_round
# 0x68a030, flag 0x80 at 0x68a090; the classes set flag 0x83 at 0x5bc480, 0x5bf138, 0x5c0798), so the template
# must have exactly the rounds of the count: 34 dates for 18 clubs, as FraFirst's own 2001 path (0x5bb6ad).
DATES = {
    0x5BB6B4: ("mov ebp, 0x26", "ebp = rt_league_dates(ebx, 0x26u);"),             # FraFirst::create_schedule
    0x5BE65A: ("mov word ptr [eax], 0x26", "W16(eax, rt_league_dates(edi, 0x26u));"),  # FraSecond
    0x5BFE6A: ("mov word ptr [eax], 0x26", "W16(eax, rt_league_dates(edi, 0x26u));"),  # FraThird
}

# Cups with a fixed entrant count that take a capped number of lower clubs and then every club of the top
# divisions. A smaller top division leaves the cup short and the constructor stops at its box. The cap of lower
# clubs (6 bytes a place) becomes the entrant count less the clubs of the top divisions: rt_cup_room(cap, entrants,
# key 1, key 2), the original's cap in the base game and whenever the top divisions hold their original counts.
# - PorCup::collect_entrants 0x7ce220: 164 (0xa4 at 0x7ce234); at most 128 lower clubs (cmp edi, 0x300 at
#   0x7ce2f8 and 0x7ce3a6), then the Second League [0x9cf610] (0x7ce42d) and the Premier League [0x9cf60c]
#   (0x7ce49e). A 2010/11 start with 16 and 16 stopped at por_cup.cpp 74.
# - WelCup::collect_entrants 0x924ab0: 32 (0x20 at 0x924ac4); at most 14 Welsh clubs of division [0x9cf960]
#   (cmp eax, 0x54 at 0x924b81), then the Welsh Premier [0x9cf94c] (0x924be3). A 2010/11 start with 12 stopped
#   at wel_cup.cpp 70.
# - DenCup::collect_entrants 0x551240: 92 (0x5c at 0x551254); at most 48 lower clubs (cmp eax, 0x120 at
#   0x55131c), then the Second [0x9cf5b4], First [0x9cf5b0] and Premier [0x9cf5ac] Divisions.
# - FraLgeCup::collect_entrants 0x5bd160: 54 (0x36 at 0x5bd174); National clubs up to 16 places (cmp eax, 0x60
#   at 0x5bd2ac), then Ligue 2 [0x9cf5d0] (0x5bd33d) and Ligue 1 [0x9cf5cc].
# - FraCup::collect_entrants 0x5baa50: 222 (0xde at 0x5baa64); National and lower clubs up to a count of
#   ebx + 0xb8 (0x5bab90, 0x5bac3c) or eax + 0xb8 (0x5bacf0), the register -2 after 2001 (184 or 182), then
#   Ligue 2 [0x9cf5d0] (0x5bad74) and Ligue 1 [0x9cf5cc] (0x5baddd). These three are whole instructions (CUP_INSNS):
#   the register plus rt_cup_add(0xb8, register, ...) is 222 less both divisions (the original's sum in the base
#   game). The compare after each add sets the flags again (0x5bab96, 0x5bac42, 0x5bacf5).
# Evidence and the other cups: docs/research/2026-10-18-league-sizes.md, "Cups".
PATCHES = {
    0x7CE2F8: (0x300, "rt_cup_room(0x300u, 164u, 0x9cf610u, 0x9cf60cu, 0u)"),
    0x7CE3A6: (0x300, "rt_cup_room(0x300u, 164u, 0x9cf610u, 0x9cf60cu, 0u)"),
    0x924B81: (0x54, "rt_cup_room(0x54u, 32u, 0x9cf94cu, 0u, 0u)"),
    0x55131C: (0x120, "rt_cup_room(0x120u, 92u, 0x9cf5b4u, 0x9cf5b0u, 0x9cf5acu)"),
    0x5BD2AC: (0x60, "rt_cup_room(0x60u, 54u, 0x9cf5d0u, 0x9cf5ccu, 0u)"),
}

# The smallest count of a class: its own count where a cup of its nation cannot hold a smaller division (the
# cups are slice 21). Each cup fills fixed places with every club of these divisions, or needs their sum:
# EngCcCup 0x56ccc0 (92, the four League divisions), EngFaCup 0x56fab0 (22 Conference places), WelLgeCup 0x927010
# (18 places, box wel_lge_cup.cpp 743), BelFaCup 0x41d8d0 (a sort of 44 at 0x41d9e4), GreCup 0x5e9a80 (a sort of 16
# at 0x5e9b60), CroCup 0x517bb0 (48, no check), RusCup 0x7e99a0 (48, box rus_cup.cpp 78), TurCup 0x8fcb90 (64 from
# fixed places), NirCup 0x78ac90 (Premier and First from place 12), ItaCCup 0x63dfe0 (90 C1 and C2 places), HolCup
# 0x5f4820 (86 places), JapJCup 0x665a00 (28 = J2 + J1), JapEmpCup 0x6624f0 (J1 16 sorted at 0x6628af),
# KorLeagueCup 0x66cf20 (10 places, box line 703), ScoLgeCup 0x7f0400 and ScoChalCup 0x7ed010 (sums of the four
# divisions), PolLgeCup 0x7cad30 (Second + First = 34), FraCup 0x5baa50 (the whole National, then lower clubs to
# 182: a National of 18 gave 2 more squadless lower clubs, and a 2022/23 game stopped in January 2023 at
# virtual_staff.cpp 195, VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0).
FLOORS = {"eng_conf", "eng_first", "eng_prm", "eng_second", "eng_third", "wel_first", "bel_first", "bel_second",
          "gre_second", "cro_a1", "cro_a2a", "cro_a2b", "rus_first", "rus_prm", "tur_first", "tur_second",
          "nir_first", "nir_prm", "ita_ser_c1a", "ita_ser_c1b", "hol_first", "hol_prm", "jap_j1", "jap_j2",
          "kor_league", "sco_first", "sco_prm", "sco_second", "sco_third", "pol_second", "fra_third"}
# The floor is the class's count after 2001 where it has two (PolSecond 18 at 0x7cbe14; PolLgeCup takes 34 = 18 + 16
# after 2001, 0x7cad4f), else its largest count.
FLOOR_COUNT = {f: (18 if f == "pol_second" else CAPS[f]) for f in FLOORS}

CUP_INSNS = {
    0x5BAB90: ("add ebx, 0xb8", "ebx = ebx + rt_cup_add(0xb8u, ebx, 222u, 0x9cf5d0u, 0x9cf5ccu);"),
    0x5BAC3C: ("add ebx, 0xb8", "ebx = ebx + rt_cup_add(0xb8u, ebx, 222u, 0x9cf5d0u, 0x9cf5ccu);"),
    0x5BACF0: ("add eax, 0xb8", "eax = eax + rt_cup_add(0xb8u, eax, 222u, 0x9cf5d0u, 0x9cf5ccu);"),
    # FraLgeCup::collect_entrants: the limit of its first National loop (16 - 2 after 2001), raised by the
    # places that National clubs from the CFA cannot fill; `cmp edi, edx` at 0x5bd247 sets the flags again.
    0x5BD244: ("add edx, 0x10", "edx = edx + rt_fra_lge_first(0x10u, edx);"),
    # ItaCup::select_entrants 0x63faf0: the first position of its second loop (C1 clubs by last position). The
    # first loop already takes C1 clubs of last position 3 to 5, so a season game starts the second loop at 6.
    0x63FB16: ("mov byte ptr [esp + 0x13], 5", "W8(esp + 0x13u, rt_ita_cup_first_position(5u));"),
    # ItaSerA::process_promotion_relegation_chain 0x642410: the expected counts of Serie A and B for
    # league_promote_and_relegate 0x689c80 (call 0x64248e), down then up, the places of the base game. In a season
    # game the places of the season: Serie A ([0x9cf570]) down, Serie B ([0x9cf574]) up.
    0x642476: ("push 4", "esp -= 4; W32(esp, rt_promote_expected(R32(R32(0xadadfcu) + R32(0x9cf570u) * 4u), 0u, 4u));"),
    0x642478: ("push 4", "esp -= 4; W32(esp, rt_promote_expected(R32(R32(0xadadfcu) + R32(0x9cf574u) * 4u), 1u, 4u));"),
    # EngFaCup::collect_entrants 0x56fab0: the National League places that its clubs cannot fill go to the clubs
    # below it, and no copy takes more than its stack array holds (O294, season_sizes.c rt_fa_cup_takes).
    0x56FFE4: ("movsx di, byte ptr [esp + 0x1b]", "edi = (edi & 0xffff0000u) | (rt_fa_cup_takes(esp) & 0xffffu);"),
    # The list stores of IreLeinsterCup::select_clubs 0x636a80, IreMunsterCup::collect_entrants 0x638e30 and
    # PolCup::collect_entrants 0x7c8550: the new list (0x944e46) is not cleared, so a place that the class does not
    # write keeps stale bytes. In a season game the list starts cleared, so the cup places fill sees each empty
    # place (season_cup_places.c rt_cup_places_list).
    0x636AA2: ("mov dword ptr [esi + 0xa7], eax", "W32(esi + 0xa7u, rt_cup_places_list(eax, 0x60u));"),
    0x638E55: ("mov dword ptr [ebp + 0xa7], eax", "W32(ebp + 0xa7u, rt_cup_places_list(eax, 0x78u));"),
    0x7C8579: ("mov dword ptr [esi + 0xa7], eax", "W32(esi + 0xa7u, rt_cup_places_list(eax, 0x138u));"),
}

# address: (the instruction in the original, C code). The store keeps its width (W16) and its object register.
INSNS = {}
for _cls, _file, _rules, _stores in CLASSES:
    for _at, _reg, _n in _stores:
        INSNS[_at] = (f"mov word ptr [{_reg} + 0x3e], {_imm(_n)}",
                      f"W16({_reg} + 0x3eu, rt_league_clubs({_reg}, {_n:#x}u));")
INSNS.update(DATES)
# FraCfa::FraCfa 0x5b91c0 fills a stack array of 72 (0x48) with every club of [0x9cf5cc] and [0x9cf5d0] and up to
# 34 clubs of A Lower Division in the CFA zone (`cmp word ptr [esp + 0x10], 0x22`), then makes four groups of 18 of
# an array it never clears. With those two divisions at 36 (18 and 18, 2024/25 on) two rows held stack values and
# the CFA fixtures named no club (virtual_staff.cpp 195). In a season game the cap is 72 less their clubs.
INSNS[0x5B92E8] = ("cmp word ptr [esp + 0x10], 0x22",
                   "t0 = R16(esp + 0x10u); t1 = rt_fra_cfa_lower_cap(0x22u) & 0xffffu; t2 = ((t0 - t1 - 0) & 0xffffu); "
                   "SETF(K_SUB | SZ2, t0, t1, t2, 0);")
# FraLower::FraLower 0x5bd600 fills a stack array of 40 (0x28) with every club of the National [0x9cf5d4] and up to
# 20 French clubs of A Lower Division [0x9cf764] with no zone (`cmp word ptr [esp + 0x10], 0x14`), shuffles the ones
# it took (0x4afb40 at 0x5bd758) and makes two tables of 20 (0x5be230 at 0x5bd769, the second at +0x50). With a
# National of 19 (October 2026 Data Update, 2025/26) the last place kept a stack value that the table took as a club.
# In a season game the cap is 40 less the National's clubs.
INSNS[0x5BD71D] = ("cmp word ptr [esp + 0x10], 0x14",
                   "t0 = R16(esp + 0x10u); t1 = rt_fra_lower_cap(0x14u) & 0xffffu; t2 = ((t0 - t1 - 0) & 0xffffu); "
                   "SETF(K_SUB | SZ2, t0, t1, t2, 0);")
INSNS.update(CUP_INSNS)
