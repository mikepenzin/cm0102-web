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
store takes the count of the data (IreFirst 0x63591f, IrePrm 0x63a74f store a register), so it has only its
set_up_rules row. PolFirst 0x7c9469 also stores a register, but it holds 8 (0x7c945a): two groups of 8, so the
class takes 16 in every season (CLASS_SIZE of season_clubs.c). set_up_rules (vtable +0x8c) writes the promotion and relegation places
+0xbe..+0xc1; the runtime wraps it (season_size_wraps.c, made by tools/seasons/make_size_wraps.py) and sets the
places of the season after it. Evidence: docs/research/2026-10-18-league-sizes.md.

The group classes (BelThird, FraLower, SpaLower, SpaSecondB, PorSecondB, SweSecond, TurSecondB, ItaSerC2a..c,
BraNatThird, GerRegional), DenSecond (one division of 16, while the real 2nd Divisions
are groups from 2005/06 with rows of 28 to 32 clubs and the places of the groups), FinFirst (the North and South groups are two objects of one class, 0x593a82, while
the table holds one division from 2002) and UsaMls (three groups of 4, season_mls.c) are not here: their sizes are groups.
"""

CLASSES = [
    ("ArgPrm", "arg_prm", 0x405670, [(0x405552, "esi", 20)]),
    ("ArgSecond", "arg_second", 0x407E50, [(0x407CE2, "esi", 25)]),
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

# Classes whose table and date template the runtime makes for any count up to a cap (GitHub issue 106; 0.7.2 league
# sizes, slice 2, D21): file: (cap, create_schedule address, template mode). recomp.py finds the table allocation of
# each row (table_sites: `push n x 0x3b` before the count store, n the class's largest count) and makes it
# rt_league_table_bytes, the rows of the season's count. make_size_wraps.py writes one wrap of create_schedule per row
# (season_size_wraps.c): the runtime gives the template exactly the rounds of the count (season_template.h, D4 and D5)
# and the meetings of the row (season_league_meetings). The mode is that of league_next_schedule_round 0x68a030:
# shuffled (League::League sets +0xd9 = 3 at 0x6822a7), sequential (flag 0x40) or mirrored (flag 0x80).
# - SwePrm (Allsvenskan, 14 to 2007, 16 from 2008): create_table 0x890430 allocates 14 rows (push 0x33a at
#   0x89043b, the object in esi from 0x890439) and stores 14 at 0x890442; create_schedule 0x88feb0 makes 26 dates
#   (D at 0x88feea, 0x69a = 26 x 0x41 bytes at 0x88fee5) for stage -1; set_up_rules 0x890550 sets 2 meetings
#   (+0x3c at 0x890563). Its constructor 0x88fb10 has no array of 14.
# - NorPrm (Eliteserien, 14 to 2007, 16 from 2008): create_table 0x7925e0 allocates 14 rows (push 0x33a at 0x7925eb,
#   the object in esi from 0x7925e9) and stores 14 at 0x7925f2; create_schedule 0x7917c0 makes 26 dates (D at
#   0x791813, 0x69a bytes at 0x79180e); set_up_rules 0x792700 sets 2 meetings (0x792713) and calls create_schedule
#   (vtable +0x3c, 0x792790) with &+0xa9 and &+0x3a. It does not write +0xd9: shuffled.
# - PorSecond (Segunda Liga, 16 to 24): create_table 0x7d1520 allocates 18 rows (push 0x426 at 0x7d152d, the object
#   in edi from 0x7d1529) and stores 18 at 0x7d1532; create_schedule 0x7d0c40 makes 34 dates (D 0x22 at 0x7d0c7a).
#   set_up_rules 0x7d16a0 also writes D: `mov word ptr [eax], 0x22` at 0x7d16bb with eax = +0xa9 (0x7d16aa), then
#   passes that eax to create_schedule (0x7d173d, call 0x7d1746), which writes it again; nothing reads +0xa9 between,
#   so the wrap's count after the original holds. It does not write +0xd9: shuffled.
# - RusFirst (Russian First Division, 17 to 22, slice 12): create_table 0x7ea6c0 allocates 18 rows (push 0x426 at
#   0x7ea6cc, the object in ebp from 0x7ea6c9) and stores 18 at 0x7ea6d3. Its take loop puts at most 18 clubs of the
#   division into a stack array (`cmp bl, 0x12` at 0x7ea77d, store at 0x7ea787), orders them (0x4afbd0 at 0x7ea7ac:
#   a qsort at 0x4afc0e, then random swaps within 3 x picks, 0x4afc13..0x4afc57), adds the first +0x3e (0x687430 at 0x7ea7cf) and moves the rest to division [0x9cf91c] (0x7ea7d6..0x7ea7ef). With
#   fewer than +0x3e it returns 0 with an empty table (0x7ea7a3). sizes_rus_tur.py gives the loop the count.
#   create_schedule 0x7ea220 makes 36 dates (D 0x24 at 0x7ea25a, 0x924 bytes at 0x7ea255) for 34 rounds; set_up_rules
#   0x7ea810 sets 2 meetings (+0x3c at 0x7ea81d) and does not write +0xd9: shuffled.
# - TurFirst (Süper Lig, 18 to 21, slice 12): create_table 0x8fddc0 stores 18 at 0x8fddd4 and allocates the table from
#   +0x3e (movsx at 0x8fde69, call at 0x8fde7d; TABLE_FROM_COUNT). It takes the clubs of the division in the club
#   order until +0x3e (0x8fdf2c..0x8fdf34) and fails unless it took exactly +0x3e (0x8fdf4a). create_schedule
#   0x8fd220 makes 34 dates (D 0x22 at 0x8fd25a); set_up_rules 0x8fdf90 sets 2 meetings (0x8fdf9d), no +0xd9.
# - TurSecond (the second tier, 17 to 20, slice 12): create_table 0x8ff150 allocates 20 rows (push 0x49c at 0x8ff15b,
#   the object in esi from 0x8ff159) and stores 20 at 0x8ff162; its take loop ends at +0x3e (0x8ff21b) and fails
#   unless it took exactly +0x3e (0x8ff235). create_schedule 0x8fe9b0 makes 38 dates (D 0x26 at 0x8fe9ea); set_up_rules
#   0x8ff280 sets 2 meetings (0x8ff28d), no +0xd9. The cap stays 20: the row only makes it smaller (D5).
# - ArgPrm (Primera, 20 to 2013/14, then 30, 28, 26, 24 and 30; slice 15): create_table 0x405540 allocates 20 rows
#   (push 0x49c at 0x40554b, the object in esi from 0x405549) and stores 20 at 0x405552; create_schedule 0x404570
#   makes 19 dates for the Apertura (stage -1, D at 0x4045aa) and 19 for the Clausura (stage 0, D at 0x404d9a), none
#   for the aggregate table (stage 1). Mode halves: the runtime of the class (season_sizes_arg.c) makes the dates of
#   each half, or of one table over both halves, and the fixed fixture lists of the zones and the derby round.
# - EngConf (Conference, 22 to 2005/06, 24 from 2006/07, 23 in 2020/21 and 2021/22; slice 8): create_table 0x56ecb0
#   allocates 22 rows (push 0x512 at 0x56ecbb, the object in esi) and stores 22 at 0x56ecc2; create_schedule 0x56de60
#   makes 42 dates (D 0x2a at 0x56de9d, 0xaaa bytes at 0x56de98) for stage -1; set_up_rules 0x56ede0 sets 2 meetings
#   (+0x3c at 0x56edf0) and does not write +0xd9: shuffled.
# - FraThird (National, 20, 21 in 2010/11, 18 from 2013/14, 17 in 2017/18 and 2024/25; slice 8): create_table 0x5c05d0
#   allocates 20 rows (push 0x49c at 0x5c05db) and stores 20 at 0x5c05e2; create_schedule 0x5bfe30 makes 38 dates (D
#   0x26 at 0x5bfe6a, 0x9a6 bytes at 0x5bfe65); set_up_rules 0x5c0700 sets flag 0x83 (0x5c0798): mirrored. Its D is
#   no longer in DATES: the wrap makes the template of any count, and a smaller count drops dates evenly (D5).
# - BraNatFirst, BraNatSecond (Série A and B, slice 17, D11): create_table 0x42a480 and 0x42ca70 allocate 28 rows (the
#   cap stays 28: no row holds more). create_schedule 0x429b50 and 0x42c140 make 27 dates for stage -1 (D 0x1b at
#   0x429b8d and 0x42c17d) and the play-off for stage 0. set_up_rules 0x42a5a0 and 0x42cb70 set 1 meeting (+0x3c at
#   0x42a5af, 0x42cb83) and +0xd9 8 and 2 (0x42a618, 0x42cbd8): neither 0x40 nor 0x80, so shuffled. Two meetings
#   from the 2003 Série A and the 2006 Série B; season_sizes_bra.c then plays no play-off.
# Slice 7 (the split engine, D7; src/recomp_rt/win/season_split.c): the pilots of the runtime split.
# - NirPrm (IFA Premiership, 10 in the class, 16 in 2003/04 to 2007/08, 12 with a split after 33 from 2008/09):
#   create_table allocates 10 rows (push 0x24e at 0x78ec6b) and stores 10 at 0x78ec72; create_schedule 0x78e330
#   makes 36 dates for stage -1 only (D 0x24 at 0x78e36a, 0x924 bytes at 0x78e365; any other stage returns 0,
#   0x78e33a); set_up_rules 0x78ed80 sets 4 meetings (0x78ed92) and +0xd9 = 3 (0x78edfd): shuffled.
# - WelFirst (Welsh Premier, 18 to 2009/10, 12 with a split after 22 from 2010/11, 16 from 2026/27): create_table
#   allocates 18 rows (push 0x426 at 0x925b3b); create_schedule 0x9250f0 makes 34 dates for stage -1 only (D 0x22 at
#   0x92512a, 0x8a2 bytes at 0x925125); set_up_rules 0x925c50 sets 2 meetings (0x925c9b) and does not write +0xd9:
#   shuffled. Its cap stays 18: the row only takes the template engine (a cap equal to the class's count).
# Slice 10. The table allocations are the pushes of 18 or 12 rows before the stores of CLASSES (sizes-check-north).
# - HolFirst (Eerste Divisie, 18 to 20): create_schedule 0x5f5790 (HolFirst vtable 0x96b86c +0x3c) makes 34 dates for
#   stage -1 (D 0x22 at 0x5f57ca); set_up_rules 0x5f6340 sets 2 meetings (0x5f6350) and does not write +0xd9:
#   shuffled. Its four periods scale to the rounds (PERIOD_PATCHES).
# - BelFirst (First Division, 16 and 18): create_schedule 0x41e010 (vtable 0x967940 +0x3c), 34 dates (0x41e04a);
#   set_up_rules 0x41e9e0 writes +0xd9 0x43 (0x41ea6b): sequential. The cap is the class's 18: the row gives 16 and
#   the split of Play-off I (slice 7, D7) with halved points.
# - BelSecond (Second Division, 8 to 19): create_schedule 0x41f340 (vtable 0x9679f4 +0x3c), 34 dates (0x41f37e);
#   set_up_rules 0x41fea0 writes +0xd9 0x43 (0x41ff34): sequential. Its three periods scale (PERIOD_PATCHES), and
#   BelThird reads its row count - 3 (BEL_THIRD_ROW).
# - DenPrm (Superliga, 12 and 14): create_schedule 0x552900 (vtable 0x9696e4 +0x3c), 33 dates (0x55293a);
#   set_up_rules 0x553a00 sets 3 meetings (0x553a0d) and writes no +0xd9: shuffled. The rows give 2 meetings and the
#   split after 26 or 22.
# - FinPrm (Veikkausliiga, 12 to 14): create_schedule 0x5954b0 (vtable 0x96a4c0 +0x3c), 33 dates (0x5954ea);
#   set_up_rules 0x595c90 sets 3 meetings (0x595ca2) and +0xd9 3 (0x595c94, 0x595cf3): shuffled. The rows give 2
#   meetings from 2003 and the split after 22.
GROWN = {
    "arg_prm": (30, 0x404570, "halves"),
    # - ArgSecond (second division, 25 in 2001/02 and 2017/18 to 2018/19, 20 to 2012/13, 22, 23, then 32 to 38; slice
    #   16, D10): fill_league_table 0x407cd0 allocates 25 rows (push 0x5c3 at 0x407cdb, the object in esi from
    #   0x407cd9) and stores 25 at 0x407ce2; create_schedule 0x406f00 makes 25 dates for the Apertura (stage -1, D 0x19
    #   at 0x406f36) and 14 for each Clausura zone (stages 0 to 2, D 0xe at 0x407530); set_up_rules 0x407e50 sets one
    #   meeting (0x407e62) and +0xd9 = 3 (0x407ec0): shuffled. Every row but 2001/02 plays one table of the row
    #   (season_sizes_arg_second.c): its template takes the dates of both halves, and the class's stages are not played.
    "arg_second": (38, 0x406F00, "shuffled"),
    "swe_prm": (16, 0x88FEB0, "shuffled"),
    "nor_prm": (16, 0x7917C0, "shuffled"),
    "por_second": (24, 0x7D0C40, "shuffled"),
    "rus_first": (22, 0x7EA220, "shuffled"),
    "tur_first": (21, 0x8FD220, "shuffled"),
    "tur_second": (20, 0x8FE9B0, "shuffled"),
    # Italy (0.7.2 slice 9). All four classes are sequential: set_up_rules sets flag 0x40 (ItaSerA 0x6416b7, ItaSerB
    # 0x64691a, ItaSerC1a 0x64b7e5, ItaSerC1b 0x64f9ae), so league_next_schedule_round 0x68a030 takes index + 1 with
    # no wrap (0x68a081..0x68a08f) and the template must hold exactly the rounds.
    # - ItaSerA (Serie A, 18 to 2003/04, 20 from 2004/05): create_table 0x641510 allocates 18 rows (push 0x426 at
    #   0x64151b) and stores 18 at 0x641522; create_schedule 0x6409b0 makes 34 dates (D 0x22 at 0x6409ed, block 0x8a2
    #   at 0x6409e8).
    # - ItaSerB (Serie B, 20; 24 in 2003/04, 22 from 2004/05 to 2017/18, 19 in 2018/19): create_table allocates 20
    #   rows (push 0x49c at 0x64678b) and stores 20 at 0x646792; create_schedule 0x645af0 makes 38 dates (D 0x26 at
    #   0x645b2d, block 0x9a6 at 0x645b28).
    # - ItaSerC1a and ItaSerC1b (Serie C1 groups, 18; 19 and 18 in 2004/05, 17 and 16 in 2012/13, 16 and 17 in
    #   2013/14; groups of the Serie C from 2014/15): create_table allocates 18 rows (0x64b67b, 0x64f82b) and
    #   create_schedule (0x64ad80, 0x64ef30) makes 34 dates (D 0x22 at 0x64adbd, 0x64ef6d). Slice 20b already patches
    #   both table pushes (new_divisions.py, rt_ita_table_bytes) and wraps both create_schedule functions
    #   (season_divisions_ita.c) for the groups of 20 of the Serie C era: TABLE_SITE_TAKEN and SCHEDULE_TAKEN.
    "ita_ser_a": (20, 0x6409B0, "sequential"),
    "ita_ser_b": (24, 0x645AF0, "sequential"),
    "ita_ser_c1a": (20, 0x64AD80, "sequential"),
    "ita_ser_c1b": (20, 0x64EF30, "sequential"),
    "eng_conf": (24, 0x56DE60, "shuffled"),
    "fra_third": (21, 0x5BFE30, "mirrored"),
    "nir_prm": (16, 0x78E330, "shuffled"),
    "wel_first": (18, 0x9250F0, "shuffled"),
    # Slice 17 (D11): the cap stays the class's 28; the row changes the meetings, so the template needs the rounds.
    "bra_nat_first": (28, 0x429B50, "shuffled"),
    "bra_nat_second": (28, 0x42C140, "shuffled"),
    # Slice 10 (Netherlands, Belgium, Denmark, Finland; periods D6, src/recomp_rt/win/season_sizes_periods.c).
    "hol_first": (20, 0x5F5790, "shuffled"),
    "bel_first": (18, 0x41E010, "sequential"),
    "bel_second": (19, 0x41F340, "sequential"),
    "den_prm": (14, 0x552900, "shuffled"),
    "fin_prm": (14, 0x5954B0, "shuffled"),
    # Slice 11: Croatia and Greece. The four classes set +0xd9 bit 0x40 in set_up_rules (sequential: 0x514958,
    # 0x515e5a, 0x5eaf04, 0x5ebe4d). CroA1 keeps the class's 16 (10 and 12 clubs in 3 and 4 meetings).
    "cro_a1": (16, 0x5140E0, "sequential"),
    "cro_a2a": (18, 0x5155E0, "sequential"),
    # Gap row 38: the South of 2002/03 to 2005/06 splits after 22 (the split engine and the template need a grown
    # class). CroA2b::set_up_rules sets +0xd9 bit 0x40 (0x516f71); create_schedule 0x516700. Its cap stays 16.
    "cro_a2b": (16, 0x516700, "sequential"),
    "gre_prm": (18, 0x5EA6D0, "sequential"),
    "gre_second": (21, 0x5EB570, "sequential"),
    # Gap row 7 of the 0.7.2 gaps list (A-League 2019 to 2026, 26 matches a club): the cap stays the class's 13; the
    # row changes the meetings, so the template needs the rounds. AusNsl::create_table 0x411b70 stores 13 (0x411b82);
    # create_schedule 0x4111a0 makes 24 dates for stage -1 (D 0x18 at 0x4111dd, 0x618 bytes at 0x4111d8) and the
    # finals series and grand final for the other stages; set_up_rules 0x411ce0 sets 3 meetings (+0x3c at 0x411d24)
    # and does not write +0xd9: shuffled.
    "aus_nsl": (13, 0x4111A0, "shuffled"),
    # Slice 13 (Northern Ireland and Ireland).
    # - NirFirst (NIFL Championship, 10 in the class, 17 in 2008/09, 16 from 2026/27; splits after 22 from 2016/17 and
    #   after 33 from 2021/22): create_table 0x78bc50 allocates 10 rows (push 0x24e at 0x78bc5b, the object in esi
    #   from 0x78bc59) and stores 10 at 0x78bc62; create_schedule 0x78b310 makes 36 dates for stage -1 (D 0x24 at
    #   0x78b34d, 0x924 bytes at 0x78b348) and the play-off for any other stage (0x78b321); set_up_rules 0x78bd70
    #   sets 4 meetings (0x78bd80) and does not write +0xd9: shuffled.
    "nir_first": (17, 0x78B310, "shuffled"),
    # - IreFirst (League of Ireland First Division, 8 in the 2012 to 2017 seasons): a class of DATA_COUNT.
    "ire_first": (12, 0x634F40, "shuffled"),
    # - IrePrm (League of Ireland Premier Division, 12 and 10; gap row 2 of 0.7.2: the calendar move of 2003 needs its
    #   own template, src/recomp_rt/win/season_calendar.h): a class of DATA_COUNT. create_schedule 0x639e90 makes 34
    #   dates for 3 meetings and 36 for any other (D = 0x22 + 2, 0x639ebc..0x639ed3) for stage -1 only (0x639eb6);
    #   set_up_rules 0x63a900 sets 3 meetings (0x63a912) and +0xd9 = 0x103 (0x63a98f): shuffled.
    "ire_prm": (12, 0x639E90, "shuffled"),
    # Slice 14 (Japan and Korea; src/recomp_rt/win/season_sizes_asia.c, sizes_asia.py). Calendar-year leagues.
    # - JapJ1 (J1, 16 in the class, 18 from 2005, 20 in 2021 and from 2024): create_table 0x6634e0 allocates 16 rows
    #   (push 0x3b0 at 0x6634eb) and stores 16 at 0x6634f2; create_schedule 0x662e30 makes 30 dates for stage -1 (D
    #   0x1e at 0x662e6d, 0x79e bytes at 0x662e68) and 1 for the final (stage 2, 0x6633d4); set_up_rules 0x663620 sets
    #   2 meetings (0x66362d) and +0xd9 8 (0x6636b2): shuffled. Halves and the final only by the row (D8).
    # - JapJ2 (J2, 12 to 22): create_table 0x665030 allocates 12 rows (push 0x2c4 at 0x66503b) and stores 12 at
    #   0x665042; create_schedule 0x664780 makes 44 dates (D 0x2c at 0x6647ba, 0xb2c bytes at 0x6647b5); set_up_rules
    #   0x665140 sets 4 meetings (0x665149) and does not write +0xd9: shuffled.
    # - KorLeague (K League, 10 to 16): create_table 0x66bba0 allocates 10 rows (push 0x24e at 0x66bbab) and stores 10
    #   at 0x66bbb2; create_schedule 0x66b130 makes 27 dates (D 0x1b at 0x66b16a, 0x6db bytes at 0x66b165);
    #   set_up_rules 0x66bcc0 sets 3 meetings (0x66bcf1) and does not write +0xd9: shuffled. The split by its row.
    "jap_j1": (20, 0x662E30, "shuffled"),
    "jap_j2": (22, 0x664780, "shuffled"),
    "kor_league": (16, 0x66B130, "shuffled"),
}
GROWN_MODES = ("shuffled", "sequential", "mirrored", "halves")
# Grown classes whose create_table allocates the table from the count (+0x3e) after the count store, so the table
# has the rows of the season with no patch: file: (address of `movsx eax, word ptr [reg + 0x3e]`, its bytes). The
# tool checks the bytes (table_sites).
TABLE_FROM_COUNT = {
    "tur_first": (0x8FDE69, b"\x0f\xbf\x45\x3e"),   # movsx eax, word ptr [ebp + 0x3e]
}
# Grown classes that count the clubs of the data (CLASSES rows with no store): create_table allocates the rows of
# that count, so there is no table site, and the count stays the data's (no cap in season_size_caps). The engine
# gives them the template of the count and the row's meetings; the class's own bounds and meetings take the row
# through PATCHES (season_sizes_irl.c). file: the largest count that the class takes.
# - IreFirst::create_table 0x635910 counts the clubs of division [0x9cf810] (0x635941..0x635969), takes only 9 to 12
#   (0x63596f, 0x635979), sets 4 meetings for 9 or 10 and 3 for 11 or 12 (0x635983..0x635997) and allocates the count
#   x 0x3b (0x63599d..0x6359b0). set_up_rules 0x635aa0 sets 4 meetings (0x635ab0) and calls create_schedule 0x634f40
#   before the constructor calls create_table (0x634cc1, then 0x634cfc; start_new_season 0x635b9d, then 0x635bdc), so
#   create_schedule makes 36 dates (D = 0x22 + 2 for meetings other than 3, 0x634f6d..0x634f84) and the table then
#   plays 33 or 36 of them. It does not write +0xd9: shuffled.
# - IrePrm::create_table 0x63a740 counts the clubs of division [0x9cf80c] (0x63a76e..0x63a799), takes only 10 or 12
#   (0x63a79f, 0x63a7a5), sets 4 meetings for 10 and 3 for 12 (0x63a7be, 0x63a7c6) and allocates the count x 0x3b
#   (0x63a7cc..0x63a7df), after set_up_rules called create_schedule (0x63a998).
DATA_COUNT = {"ire_first": 12, "ire_prm": 12}
# The rows that the class allocates: its largest original count (before the cap of GROWN), the cap for a class
# whose table follows the count (TABLE_FROM_COUNT), and 0 for DATA_COUNT.
HELD = {}
for _f, (_n, _sched, _mode) in GROWN.items():
    assert _mode in GROWN_MODES, _f
    if _f in DATA_COUNT:
        assert _f not in CAPS and _n == DATA_COUNT[_f], _f
        HELD[_f] = 0
        continue
    assert _n >= CAPS[_f], _f   # equal: only the meetings and dates change (TurSecond, Brazil)
    HELD[_f] = _n if _f in TABLE_FROM_COUNT else CAPS[_f]
    CAPS[_f] = _n

ROW_BYTES = 0x3B
# mov <reg>, ecx (8b /r, mod 11, rm ecx): the object register of create_table.
_MOV_FROM_ECX = {"ebx": b"\x8b\xd9", "ebp": b"\x8b\xe9", "esi": b"\x8b\xf1", "edi": b"\x8b\xf9"}


def table_sites(read):
    """{file: (address of the allocation push, object register, bytes)} of the GROWN rows. read(addr, size) gives
    the bytes of the original. The push is `push imm32` (68 xx xx xx xx) of HELD x 0x3b in the 16 bytes before a
    count store of the class, after a `mov reg, ecx` of the store's register in the 8 bytes before it (NorPrm:
    0x7925e9, 0x7925eb, 0x7925f2). No such push stops the build."""
    sites = {}
    for _cls, file, _rules, stores in CLASSES:
        if file not in GROWN or file in DATA_COUNT:
            continue
        if file in TABLE_FROM_COUNT:
            at, want = TABLE_FROM_COUNT[file]
            if read(at, len(want)) != want:
                raise SystemExit(f"league_sizes.py: {file}: no table allocation from the count at {at:#x}")
            continue
        want = (HELD[file] * ROW_BYTES).to_bytes(4, "little")
        found = []
        for at, reg, _n in stores:
            code = read(at - 16, 16)
            for k in range(len(code) - 5, -1, -1):
                if code[k] == 0x68 and code[k + 1:k + 5] == want:
                    push = at - 16 + k
                    if _MOV_FROM_ECX[reg] in read(push - 8, 8):
                        found.append((push, reg))
                    break
        if len(found) != 1:
            raise SystemExit(f"league_sizes.py: {file}: {len(found)} table allocations of {HELD[file]} rows "
                             f"(push {HELD[file] * ROW_BYTES:#x}) before its count stores, want 1")
        push, reg = found[0]
        sites[file] = (push, reg, HELD[file] * ROW_BYTES)
    return sites


# Table sites that another tool already patches for the grown count: file: (address, the tool and why it holds the
# count). D21: such a site is not patched twice. Any other overlap stops the build. rt_ita_table_bytes
# (season_divisions_ita.c) gives the 20 rows of the Serie C era, and otherwise rt_league_table_bytes.
TABLE_SITE_TAKEN = {
    "ita_ser_c1a": (0x64B67B, "new_divisions.py rt_ita_table_bytes"),
    "ita_ser_c1b": (0x64F82B, "new_divisions.py rt_ita_table_bytes"),
}
# Grown classes whose create_schedule another module wraps already (RT_WRAP takes an address once). That wrap calls
# season_sizes_schedule after the original, and make_size_wraps.py writes none: file: the module.
SCHEDULE_TAKEN = {
    "bel_second": "season_bel_playoffs.c bel_second_schedule",
    "ita_ser_c1a": "season_divisions_ita.c c1a_schedule",
    "ita_ser_c1b": "season_divisions_ita.c c1b_schedule",
}


def table_insns(read, taken=()):
    """The INSNS of the table allocations of table_sites. A site in taken (INSNS or PATCHES of another tool) is
    skipped when TABLE_SITE_TAKEN names it, else the build stops."""
    out = {}
    for f, (push, reg, size) in table_sites(read).items():
        if push in taken:
            if TABLE_SITE_TAKEN.get(f, (None,))[0] != push:
                raise SystemExit(f"league_sizes.py: {f}: the table allocation {push:#x} is patched by another tool; "
                                 f"name it in TABLE_SITE_TAKEN with the tool that holds the count")
            continue
        out[push] = (f"push {size:#x}", f"esp -= 4; W32(esp, rt_league_table_bytes({reg}, {size:#x}u));")
    return out


# The French classes play the second half from the middle of their date template (league_next_schedule_round
# 0x68a030, flag 0x80 at 0x68a090; the classes set flag 0x83 at 0x5bc480, 0x5bf138, 0x5c0798), so the template
# must have exactly the rounds of the count: 34 dates for 18 clubs, as FraFirst's own 2001 path (0x5bb6ad).
# FraThird (D at 0x5bfe6a) is a grown class (GROWN, slice 8): its wrap of create_schedule makes the template.
DATES = {
    0x5BB6B4: ("mov ebp, 0x26", "ebp = rt_league_dates(ebx, 0x26u);"),             # FraFirst::create_schedule
    0x5BE65A: ("mov word ptr [eax], 0x26", "W16(eax, rt_league_dates(edi, 0x26u));"),  # FraSecond
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
    # IreFirst::create_table 0x635910 (DATA_COUNT, slice 13): the least count it takes, and its meetings for a count
    # (esi the object). A season game with the sizes on takes the count of the data from 2 clubs and the meetings
    # that the engine set from the row (season_sizes_irl.c); the base game the original's.
    0x63596F: (9, "rt_ire_first_least(esi, 9u)"),
    0x63598F: (3, "rt_ire_first_meetings(esi, 3u)"),
    0x635997: (4, "rt_ire_first_meetings(esi, 4u)"),
    # IrePrm::create_table 0x63a740 (gap 43): for a table of 12 clubs it sets 3 relegation places (+0xc1) and 1
    # play-off place (+0xc0) at 0x63a8a9..0x63a8b5, the 12 to 10 of 2001/02. It runs in the constructor after
    # set_up_rules (0x639c3c, then 0x639c77) and again at each start_new_season (0x63aa3d), so every 12-club season
    # sends 3 down. A season game with the sizes on takes the direct places of the row (season_sizes_irl.c).
    0x63A8B5: (3, "rt_ire_prm_down(esi, 3u)"),
    # IrePrm::create_table (gap row 2 of 0.7.2, DATA_COUNT): the meetings for 10 and 12 clubs take those that the engine
    # set from the row, as IreFirst's (rt_ire_first_meetings reads only the object).
    0x63A7BE: (4, "rt_ire_first_meetings(esi, 4u)"),
    0x63A7C6: (3, "rt_ire_first_meetings(esi, 3u)"),
    # The Irish awards (gap row 2): IrelandAwards::daily_update 0x63bf10 and OldIrelandAwards::daily_update 0x79f250
    # give the awards on Date(1, May, year) (push 4, push 1 before Date::Date at 0x63bf2b, 0x79f26b); in the short and
    # calendar seasons on 16 February (src/recomp_rt/win/season_calendar_game.c).
    0x63BF27: (4, "rt_ire_award_month(4u)"),
    0x63BF29: (1, "rt_ire_award_day(1u)"),
    0x79F267: (4, "rt_ire_award_month(4u)"),
    0x79F269: (1, "rt_ire_award_day(1u)"),
    # Item 251: the relegation play-off place of the same branch (mov byte [esi + 0xc0], 1 at 0x63a8ae): 0 in a season
    # whose row has no play-off (season_sizes_irl.c).
    0x63A8AE: (1, "rt_ire_prm_playoff(esi, 1u)"),
}

# The smallest count of a class: its own count where a cup of its nation cannot hold a smaller division (the
# cups are slice 21). Each cup fills fixed places with every club of these divisions, or needs their sum:
# The cups of eastern Europe and Asia follow the divisions (0.7.2 slice 6, D14; cup_sizes_east.py and
# src/recomp_rt/win/season_cup_sizes_east.c): GreCup 0x5e9a80, CroCup 0x517bb0, RusCup 0x7e99a0, TurCup 0x8fcb90,
# PolLgeCup 0x7cad30, JapJCup 0x665a00, JapEmpCup 0x6624f0 and KorLeagueCup 0x66cf20. Their divisions have no floor.
# The cups of Britain and Ireland follow the divisions (0.7.2 league sizes, slice 4, D14;
# src/recomp_rt/win/season_cup_sizes_uk.c): EngCcCup 0x56ccc0, EngFaCup 0x56fab0, WelCup 0x924ab0, WelLgeCup
# 0x927010, NirCup 0x78ac90, ScoLgeCup 0x7f0400 and ScoChalCup 0x7ed010. Their divisions have no floor.
# The cups of western Europe follow the divisions (slice 5, D14): BelFaCup 0x41d8d0, ItaCCup 0x63dfe0, HolCup
# 0x5f4820, FraCup 0x5baa50 and DenCup. Slice 5 keeps the floors of bel_second and the Serie C1 groups.
# A division leaves this set when the slice of its cup lands (lead merge of slices 4 and 5, and the merge of
# slice 6: the floors any slice removed are gone). Slice 9 removes the Serie C1 groups: the regroup
# ita_ser_a_regroup_serie_c1_clubs 0x642630 and the play-out rows take the counts (season_sizes_ita.c).
# Slice 10 removes bel_second: BelThird reads the row count - 3 (BEL_THIRD_ROW).
FLOORS = set()
# The floor is the class's largest count.
FLOOR_COUNT = {f: CAPS[f] for f in FLOORS}
# The divisions whose places and exchange stay the class's, though the club moves give them the count of their row
# (0.7.2 slice 4, lead decision of 13 November 2026). A floor did both: sizes_rules (season_sizes.c) leaves the places
# of a division with a floor or next to one, and the wrap of league_promote_and_relegate 0x689c80 moves its clubs in
# pairs unless both counts stay within floor and cap. The places of these divisions belong to the slices of their
# leagues. The Northern Irish divisions left with items 271 and 272 (slice 13): their places are the rows'
# (season_sizes_nir.c), and the First Division's relegation places go to the Northern Irish lower division. file: the
# count of the old floor (the class's own count: HELD for a grown class, slice 7). The English divisions left in slice 8 (D15): the exchange of League Two and the Conference
# takes the places of the rows (season_sizes_eng.c). The Welsh Premier left with open item 250: it has no chain, and its
# row's relegation places go to no division at its season change (season_sizes.c, lone_exchange). The Scottish
# divisions left with gap row 11: their places are the rows', and the play-offs between two divisions are
# season_playoffs.c (the cross play-off).
FIXED_EXCHANGE = {}
assert not set(FIXED_EXCHANGE) & FLOORS

# Fixed rows that a class of another division reads in the table of a class here (open item 113): file: (count, why).
# The count of the file (+0x3e, the store of CLASSES) must reach the count here in every season, or the row holds no
# club. BelThird::create_promotion_playoff_stage 0x423880 puts the club of row 15 (+0x375, rows of 0x3b) of the
# Belgian Second Division into its play-off (0x423a6a..0x423a80), through BelThird +0x1c, the key of the division
# above. BelThird::advance_stage 0x423640 calls it once the Second Division is complete and BelThird's own stage
# counter +0x30 reaches 7 (0x4236bf..0x4236f1). BelSecond::create_table 0x41fd70 always makes 18 rows (0x426 bytes at
# 0x41fd7b), so the read stays inside the table, and stores the count at 0x41fd82: rt_league_clubs (season_sizes.c)
# gives the clubs of the division there, up to the cap, with no floor. The floor keeps that number of clubs: it is the
# target of the club moves at a new game (season_clubs.c class_size) and holds the exchange of the wrap of 0x689c80
# (season_sizes.c). So with a floor of 16 or more, row 15 holds a club. tools/seasons/make_size_wraps.py checks it.
# Slice 10 (D22): BelThird reads the row count - 3 (BEL_THIRD_ROW below), so bel_second needs no floor; none is left.
ROW_READS = {}

CUP_INSNS = {
    0x5BAB90: ("add ebx, 0xb8", "ebx = ebx + rt_cup_add(0xb8u, ebx, 222u, 0x9cf5d0u, 0x9cf5ccu);"),
    0x5BAC3C: ("add ebx, 0xb8", "ebx = ebx + rt_cup_add(0xb8u, ebx, 222u, 0x9cf5d0u, 0x9cf5ccu);"),
    0x5BACF0: ("add eax, 0xb8", "eax = eax + rt_cup_add(0xb8u, eax, 222u, 0x9cf5d0u, 0x9cf5ccu);"),
    # Slice 5, the cups of western Europe (src/recomp_rt/win/season_cup_sizes_west.c, its module comment).
    # BelFaCup::collect_entrants 0x41d8d0: the pool clubs (k of 0x4afbd0, the copy, edi is the pool's count at both)
    # and the place of the first third-tier club (edx is the clubs copied).
    0x41D9E4: ("push 0x2c", "t0 = rt_bel_cup_lower(0x2cu, edi); esp -= 4; W32(esp, t0);"),
    0x41D9F4: ("mov edi, 0x2c", "edi = rt_bel_cup_lower(0x2cu, edi);"),
    0x41DA43: ("mov eax, 0x108", "eax = rt_bel_cup_place(0x108u, edx);"),
    # HolCup::collect_entrants 0x5f4820: the copy that makes the empty places, the pool pick and the fill (the pool's
    # count is at [esp + 0x10] at all three).
    0x5F49E1: ("mov edx, 0x32", "edx = rt_hol_cup_lower(0x32u, esp);"),
    0x5F4A62: ("push 0x32", "t0 = rt_hol_cup_lower(0x32u, esp); esp -= 4; W32(esp, t0);"),
    0x5F4A7C: ("mov ebp, 0x32", "ebp = rt_hol_cup_lower(0x32u, esp);"),
    # ItaCCup::collect_entrants 0x63dfe0: the empty places take clubs before the second pass (esi the cup, edi the
    # clubs placed).
    0x63E109: ("mov eax, 0x50", "eax = rt_ita_c_cup_fill(0x50u, esi, edi);"),
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
}

# The cups of Britain and Ireland that take every club of their divisions (0.7.2 league sizes, slice 4, D14;
# season_cup_sizes_uk.c rt_cup_clubs): the list size and the count of the sum of the divisions, the original's
# constant in the base game.
_ENG_LEAGUE = "0x9cf5bcu, 0x9cf5c0u, 0x9cf5c4u, 0x9cf5c8u"
_SCO_LEAGUE = "0x9cf5dcu, 0x9cf5e0u, 0x9cf5e4u, 0x9cf5e8u"
_SCO_LOWER = "0x9cf5e0u, 0x9cf5e4u, 0x9cf5e8u, 0u"
CUP_COUNTS = {
    # EngCcCup::collect_entrants 0x56ccc0: 92 places of the four League divisions.
    0x56CCCD: ("push 0x228", f"esp -= 4; W32(esp, rt_cup_clubs(0x5cu, {_ENG_LEAGUE}) * 6u);"),
    0x56CCD4: ("mov word ptr [esi + 0x3e], 0x5c", f"W16(esi + 0x3eu, rt_cup_clubs(0x5cu, {_ENG_LEAGUE}));"),
    # ScoLgeCup::collect_entrants 0x7f0400: 40 in 1999, else 42, the four divisions; the list comes from the count.
    0x7F0417: ("mov word ptr [esi + 0x3e], 0x28", f"W16(esi + 0x3eu, rt_cup_clubs(0x28u, {_SCO_LEAGUE}));"),
    0x7F041F: ("mov word ptr [esi + 0x3e], 0x2a", f"W16(esi + 0x3eu, rt_cup_clubs(0x2au, {_SCO_LEAGUE}));"),
    # ScoChalCup::collect_entrants 0x7ed010: 30 places of the First, Second and Third Divisions.
    0x7ED01B: ("push 0xb4", f"esp -= 4; W32(esp, rt_cup_clubs(0x1eu, {_SCO_LOWER}) * 6u);"),
    0x7ED024: ("mov word ptr [esi + 0x3e], 0x1e", f"W16(esi + 0x3eu, rt_cup_clubs(0x1eu, {_SCO_LOWER}));"),
}
assert not set(CUP_COUNTS) & set(CUP_INSNS)
CUP_INSNS.update(CUP_COUNTS)

# The cup lists of the original (0.6.2 item 64; O151 before it for three cups). Each class takes a new list (6 bytes
# a place) from the allocator 0x944e46, which does not clear the block, and stores it to +0xa7. Rows: the address of
# that store: (object register, block size). A size of None is 6 bytes for each place of the count at +0x3e, which the
# class sets before the call (PolLgeCup 0x7cad47..0x7cad5e, ScoLgeCup 0x7f0417..0x7f0435, WcAfricanCup 0x910133..
# 0x910156, WcConcacafCup 0x915d59..0x915d65) or reads there (IntertotoCup 0x633798, to the size at 0x633810).
# tests/seasons/cup_list_clear_check.py finds every such store in the original and checks each size.
LIST_STORES = {
    0x40B63E: ("esi", 0x60),    # AsiaClubChamp::build_team_table
    0x40D4EE: ("esi", 0x60),    # AsiaCupWinner::create_team_entries
    0x410A6E: ("esi", 0xc),     # AsiaSuperCup::select_teams
    0x41D8F1: ("esi", 0x2a0),   # BelFaCup::collect_entrants
    0x42130E: ("esi", 0xc),     # BelSuper::select_teams
    0x427C3C: ("esi", 0x1aa),   # BraCup::build_team_table
    0x517BD5: ("edi", 0x120),   # CroCup::collect_entrants
    0x551264: ("esi", 0x228),   # DenCup::collect_entrants
    0x56B3E1: ("esi", 0xa8),    # EngAutoCup::collect_entrants
    0x56CCEA: ("esi", 0x228),   # EngCcCup::collect_entrants
    0x56D79E: ("esi", 0xc),     # EngCharity::select_teams
    0x56FBCF: ("esi", 0x390),   # EngFaCup::collect_entrants
    0x5712CF: ("esi", 0xc0),    # EngFaTrophy::select_teams
    0x57A51E: ("esi", 0xc),     # EurSuperCup::select_teams
    0x5849B1: ("esi", 0x14a),   # EuropeanCup::build_qualifying_entry_table
    0x592A44: ("esi", 0x1c8),   # FinCup::collect_entrants
    0x5BAA74: ("esi", 0x534),   # FraCup::collect_entrants
    0x5BD184: ("esi", 0x144),   # FraLgeCup::collect_entrants
    0x5BF78E: ("esi", 0xc),     # FraSuper::select_teams
    0x5D9395: ("esi", 0x180),   # GerCup::collect_entrants
    0x5EC44A: ("esi", 0xc),     # GreSuper::select_teams
    0x5F8C2E: ("esi", 0xc),     # HolSuper::select_teams
    0x6324A4: ("edi", 0xc),     # InterAmerCup::select_teams
    0x63381E: ("esi", None),    # IntertotoCup::collect_entrants
    0x6347F5: ("esi", 0xfc),    # IreChalCup::collect_entrants
    0x636AA2: ("esi", 0x60),    # IreLeinsterCup::select_clubs
    0x638E55: ("ebp", 0x78),    # IreMunsterCup::collect_entrants
    0x63C9EE: ("esi", 0xc),     # ItaC1Super::init_team_list
    0x65FA7E: ("esi", 0xc),     # ItaSuper::init_team_list
    0x662522: ("esi", 0x1e0),   # JapEmpCup::collect_entrants
    0x665A24: ("esi", 0xa8),    # JapJCup::collect_entrants
    0x66617E: ("esi", 0xc),     # JapSuper::select_teams
    0x66AB18: ("esi", 0xc0),    # KorFaCup::collect_entrants
    0x66DC2A: ("esi", 0xc),     # KorSuperCup::create_table
    0x7662AE: ("esi", 0x18),    # MiniCup::set_up_clubs
    0x78A1FE: ("esi", 0xc),     # NirCharity::select_teams
    0x78ACB5: ("esi", 0xc0),    # NirCup::collect_entrants
    0x78F8B8: ("esi", 0x300),   # NorCup::collect_entrants
    0x7C8579: ("esi", 0x138),   # PolCup::collect_entrants
    0x7CAD69: ("esi", None),    # PolLgeCup::collect_entrants
    0x7CCD2A: ("esi", 0xc),     # PolSuper::select_teams
    0x7CE244: ("esi", 0x3d8),   # PorCup::collect_entrants
    0x7D327E: ("esi", 0xc),     # PorSuper::select_teams
    0x7E9A7A: ("esi", 0x120),   # RusCup::select_entrants
    0x7ED034: ("esi", 0xb4),    # ScoChalCup::collect_entrants
    0x7EDB18: ("esi", 0x12c),   # ScoFaCup::collect_entrants
    0x7F0440: ("esi", None),    # ScoLgeCup::collect_entrants
    0x858ABE: ("esi", 0xc),     # SpaSuper::select_teams
    0x88D5C1: ("esi", 0xb4),    # SweCup::collect_entrants
    0x8FCBBC: ("esi", 0x180),   # TurCup::collect_entrants
    0x90A55E: ("esi", 0xc),     # UsaMlsAllStars::set_up_teams
    0x91015F: ("esi", None),    # WcAfricanCup::build_first_round_entries
    0x915D72: ("esi", None),    # WcConcacafCup::build_team_table
    0x924AD4: ("esi", 0xc0),    # WelCup::collect_entrants
    0x92B8D4: ("edi", 0xc),     # WorldClubCup::pick_qualifiers
}
# Left: Cup_Stage::Cup_Stage 0x5230e0 sizes its list from its argument (0x52310a); a stage takes only the clubs that
# qualified for it, and the cup entrants fill leaves it (season_cup_entrants.c, O258). Cup::load_from_disk 0x51fc00
# reads every place of the list from the save (fread at 0x5209dd).
LIST_STORES_LEFT = {0x523131: "Cup_Stage::Cup_Stage", 0x52095F: "Cup::load_from_disk"}

# Each cup list starts cleared in a season game (season_cup_places.c rt_cup_places_list): a place that the class
# does not write then reads as empty, and the cup places fills see it (Database.cpp 13471, item 64).
for _at, (_reg, _bytes) in LIST_STORES.items():
    _size = f"{_bytes:#x}u" if _bytes is not None else f"(uint32_t)(int16_t)R16({_reg} + 0x3eu) * 6u"
    CUP_INSNS[_at] = (f"mov dword ptr [{_reg} + 0xa7], eax", f"W32({_reg} + 0xa7u, rt_cup_places_list(eax, {_size}));")
# The lists of CUP_COUNTS: the count at +0x3e is set before the store (0x56ccd4 before 0x56ccea, 0x7ed024 before
# 0x7ed034), so the clear takes the size of the count. LIST_STORES keeps the original's size for
# tests/seasons/cup_list_clear_check.py.
for _at in (0x56CCEA, 0x7ED034):
    CUP_INSNS[_at] = ("mov dword ptr [esi + 0xa7], eax",
                      "W32(esi + 0xa7u, rt_cup_places_list(eax, (uint32_t)(int16_t)R16(esi + 0x3eu) * 6u));")

# address: (the instruction in the original, C code). The store keeps its width (W16) and its object register.
INSNS = {}
for _cls, _file, _rules, _stores in CLASSES:
    for _at, _reg, _n in _stores:
        INSNS[_at] = (f"mov word ptr [{_reg} + 0x3e], {_imm(_n)}",
                      f"W16({_reg} + 0x3eu, rt_league_clubs({_reg}, {_n:#x}u));")
INSNS.update(DATES)
# The split engine (0.7.2 slice 7, D7; src/recomp_rt/win/season_split.c). League::check_complete 0x689ad0 calls a table
# complete when every row's matches (s8 +0x11) reach +0xa7 (`mov di, word ptr [esi + 0xa7]` at 0x689b47, compared at
# 0x689b51..0x689b58; di is not read after the loop, 0x689b66). +0xa7 holds the most matches of a club of the season
# (it also sizes each row's array of positions, init_table_row 0x687570). Sections of a split that play different
# matches (6, 4 and 6 clubs that meet once) would never all reach it: rt_league_played_target gives 0 when every row
# reached the matches of its own section, else the original.
INSNS[0x689B47] = ("mov di, word ptr [esi + 0xa7]",
                   "edi = (edi & 0xffff0000u) | (rt_league_played_target(esi, R16(esi + 0xa7u)) & 0xffffu);")
# The sort 0x6827d0 writes each row's place at index played - 1 (0x682b0f, ebx the league, eax the played). After a
# split the place after match N stays as the split read it (rt_league_keep_split_place, slice 10 test runs).
INSNS[0x682B0F] = ("mov byte ptr [eax + esi - 1], cl",
                   "if (!rt_league_keep_split_place(ebx, eax)) W8(eax + esi + 0xffffffffu, ecx & 0xffu);")
# Gap 33: HolFirst::update_positions_and_status 0x5f6d20 pushes the period number for the news of a period winner
# (0x5f7330); with six periods the wrap shows the fifth and sixth as the fourth, so the number gets the shift back.
INSNS[0x5F6E05] = ("push edx", "t0 = edx + rt_hol_period_news(); esp -= 4; W32(esp, t0);")
# Gap 33: the stage array of HolFirst has room for eight (rt_hol_stage_room) but +0x2c keeps the class's 6: the
# League's update_for_date 0x6849aa ends the season when +0x30 reaches +0x2c - 1 (0x6849d1..0x6849da) and calls
# advance_stage (vtable +0x28) before; HolFirst::advance_stage 0x5f6650 makes the same test (0x5f6659..0x5f6662).
# With six periods the last stage is the second play-off group, 7 (rt_league_last_stage). A capacity of 8 for every
# season sent a four-period season past its play-off into an empty slot (call to 0x1 from 0x6848fb, g10 hol2010).
INSNS[0x6849D7] = ("dec eax", "eax = rt_league_last_stage(esi, eax - 1u);")
INSNS[0x5F665F] = ("dec eax", "eax = rt_league_last_stage(esi, eax - 1u);")
INSNS[0x4201AC] = ("dec edx", "edx = rt_league_last_stage(ecx, edx - 1u);")   # BelSecond::advance_stage
# League::check_complete 0x689ad0 with its first argument 1 (the season change entry and season_playoffs.c) makes the
# test too (0x689bdf..0x689be8): without this First Division B with two periods never ended its season (g13 bel2016).
INSNS[0x689BE5] = ("dec ecx", "ecx = rt_league_last_stage(esi, ecx - 1u);")
# League::load makes a stage array of +0x2c slots (0x688e4b); a HolFirst gets room for eight (rt_league_stage_bytes).
INSNS[0x688E4B] = ("lea ecx, [ebp*4]", "ecx = rt_league_stage_bytes(esi, ebp * 4u);")
# League::sort_table 0x6827d0 copies the table order +0xa3 into the global [0xb67a00] that the row comparators 0x683210
# and 0x683410 call first (0x68287c..0x682884). With no tie rules list (+0xad, 0x682824) and the first tie rule 6
# (+0xc5, 0x682835) it jumps past that copy to its own qsort with 0x683210 (0x682aa8..0x682ab9), so the comparator
# calls the order of the last sort: the Croatian classes take that path once their table is complete
# (Club_Records::update_league_position_records 0x447430 and Club_History::record_comp_season 0x442e80 sort it then),
# and the sections of a split were lost for the final table (slice 11). rt_split_sort_order sets the global to a split
# league's own order there; any other league keeps the original.
INSNS[0x682AAC] = ("movsx eax, word ptr [ebx + 0x3e]",
                   "rt_split_sort_order(ebx); eax = (uint32_t)(int32_t)(int16_t)R16(ebx + 0x3eu);")

# League::update_positions_and_status 0x684cd0 works out each row's best and worst places (+0x39, +0x3a) in its loop at
# 0x684ef7..0x6850dc from +0xa7 and every other row. After a split, rt_split_bounds runs the same rules by section with
# the section's matches and the code goes on at 0x6850e2 (the status marks); else the original's loop runs. eax is
# only the loop counter, and the flags of `xor eax, eax` are set again by the cmp at 0x684ef9 (and at 0x6850e4 on the
# jump). The locals after the loop are written first: [esp + 0x14] at 0x6850e8, [esp + 0x20] at 0x6850f8, [esp + 0x18]
# at 0x6852cc, [esp + 0x12] by UefaSeeding::get_nation_places 0x905c60 (0x6853f5) before 0x685426.
INSNS[0x684EF7] = ("xor eax, eax", "if (rt_split_bounds(esi)) goto L_6850e2; eax = 0;")
# FraCfa::FraCfa 0x5b91c0 fills a stack array of 72 (0x48) with every club of [0x9cf5cc] and [0x9cf5d0] and up to
# 34 clubs of A Lower Division in the CFA zone (`cmp word ptr [esp + 0x10], 0x22`), then makes four groups of 18 of
# an array it never clears. With those two divisions at 36 (18 and 18, 2024/25 on) two rows held stack values and
# the CFA fixtures named no club (virtual_staff.cpp 195). In a season game the cap is 72 less their clubs.
INSNS[0x5B92E8] = ("cmp word ptr [esp + 0x10], 0x22",
                   "t0 = R16(esp + 0x10u); t1 = rt_fra_cfa_lower_cap(0x22u) & 0xffffu; t2 = ((t0 - t1 - 0) & 0xffffu); "
                   "SETF(K_SUB | SZ2, t0, t1, t2, 0);")
# Slice 5 (season_cup_sizes_west.c, rt_fra_cfa_fill): after the loop, the places that the CFA zone cannot fill take
# French clubs from below the National; edi is the count for the shuffle and the groups.
INSNS[0x5B9320] = ("lea eax, [esp + 0x21c]", "edi = rt_fra_cfa_fill(esp + 0x21cu, edi); eax = esp + 0x21cu;")
# The French Lower Division (constructor 0x5bd600): the same after its loop of the National and lower clubs (40).
INSNS[0x5BD752] = ("lea edx, [esp + 0x1c]", "edi = rt_fra_lower_fill(esp + 0x1cu, edi); edx = esp + 0x1cu;")
INSNS.update(CUP_INSNS)
# Slice 10 (D22, src/recomp_rt/win/season_sizes_bel.c): BelThird::create_promotion_playoff_stage 0x423880 puts the
# club of row 15 of the Second Division (+0x375, the table in ecx from 0x423a78) into the third tier's promotion
# play-off; the third from the bottom is the row count - 3. esi is BelThird (+0x1c the id of the division above).
BEL_THIRD_ROW = {0x423A80: ("mov edx, dword ptr [ecx + 0x375]", "edx = R32(ecx + rt_bel_third_row(esi, 0x375u));")}
INSNS.update(BEL_THIRD_ROW)
# Slice 10 (D6, src/recomp_rt/win/season_sizes_periods.c): the period bounds of add_result (the object in esi) and the
# matches of each period stage (create_period_stage, the object in esi, the period in bl or al) scale to the rounds.
PERIOD_PATCHES = {
    0x5F65CE: (0x8, "rt_league_period_bound(esi, 0x8u)"),       # HolFirst::add_result 0x5f65b0: cmp ax, 8
    0x5F65DA: (0x10, "rt_league_period_bound(esi, 0x10u)"),     # cmp ax, 0x10
    0x5F65E6: (0x1A, "rt_league_period_bound(esi, 0x1au)"),     # cmp ax, 0x1a
    0x5F6BD6: (0xA, "rt_league_period_length(esi, ebx & 0xffu, 0xau)"),   # create_period_stage 0x5f6ad0: period 2
    0x5F6BE2: (0x8, "rt_league_period_length(esi, ebx & 0xffu, 0x8u)"),   # periods 0, 1 and 3
    0x42012E: (0xA, "rt_league_period_bound(esi, 0xau)"),       # BelSecond::add_result 0x420110: cmp ax, 0xa
    0x42013A: (0x16, "rt_league_period_bound(esi, 0x16u)"),     # cmp ax, 0x16
    # Gap 33: BelSecond with the row's two periods: the play-off after stage 2 (advance_stage 0x4201a0, ecx the
    # object), a final of the two period winners (create_playoff_group 0x420230, esi the object).
    0x42021C: (0x3, "rt_bel_playoff_stage(ecx, 0x3u)"),         # cmp eax, 3
    0x42028E: (0x4, "rt_bel_playoff_clubs(esi, 0x4u)"),         # cmp ebp, 4: the top-up
    0x42036B: (0x4, "rt_bel_playoff_clubs(esi, 0x4u)"),         # cmp ebx, 4: the list
    0x420412: (0x4, "rt_bel_playoff_clubs(esi, 0x4u)"),         # push 4: the group's clubs
    # Gap 33: HolFirst with six periods (the Eerste Divisie 2005/06 to 2008/09): room for eight stages, the play-off
    # groups at stages 6 and 7 (season_sizes_periods.c, rt_hol_playoff_stage: the original's 4 and 5 otherwise).
    0x5F54F5: (0x18, "rt_hol_stage_room(0x18u)"),               # constructor: push 0x18, the stage array
    0x5F6709: (0x4, "rt_hol_playoff_stage(esi, 0x4u)"),         # advance_stage 0x5f6650: cmp eax, 4
    0x5F68EC: (0x4, "rt_hol_playoff_stage(esi, 0x4u)"),         # create_playoff 0x5f6720: dates of group 1
    0x5F692C: (0x4, "rt_hol_playoff_stage(esi, 0x4u)"),         # group 1's stage number
    0x5F697A: (0x10, "4u * rt_hol_playoff_stage(esi, 0x4u)"),   # mov [ecx + 0x10], eax
    0x5F6992: (0x5, "rt_hol_playoff_stage(esi, 0x5u)"),         # dates of group 2
    0x5F69D0: (0x5, "rt_hol_playoff_stage(esi, 0x5u)"),         # group 2's stage number
    0x5F6A21: (0x14, "4u * rt_hol_playoff_stage(esi, 0x5u)"),   # mov [edx + 0x14], eax
    0x5F6A35: (0x10, "4u * rt_hol_playoff_stage(esi, 0x4u)"),   # cmp [eax + 0x10], ebx
    0x5F6A3A: (0x14, "4u * rt_hol_playoff_stage(esi, 0x5u)"),   # cmp [eax + 0x14], ebx
    0x5F6A3F: (0x5, "rt_hol_playoff_stage(esi, 0x5u)"),         # mov [esi + 0x30], 5
    0x5F604B: (0x4, "rt_hol_playoff_stage(edi, 0x4u)"),         # create_schedule 0x5f5790: cmp al, 4
    0x5F604F: (0x5, "rt_hol_playoff_stage(edi, 0x5u)"),         # cmp al, 5
    0x5F607A: (0x4, "rt_hol_playoff_stage(edi, 0x4u)"),         # the group's name: cmp al, 4
    0x5F6F9C: (0x4, "rt_hol_playoff_stage(ebx, 0x4u)"),         # status news 0x5f6ee0: cmp al, 4
    0x5F6FD1: (0x5, "rt_hol_playoff_stage(ebx, 0x5u)"),         # cmp al, 5
    0x5F7117: (0x4, "rt_hol_match_news_stage(ecx, 0x4u)"),      # match news 0x5f7110: cmp al, 4
    0x5F711D: (0x5, "rt_hol_match_news_stage(edi, 0x5u)"),      # cmp al, 5
    # The group goes in the play-off's own slot of the stage array (+0xc), so no slot before it stays empty.
    0x42042F: (0xC, "4u * rt_bel_playoff_stage(esi, 0x3u)"),    # mov [ecx + 0xc], eax
    0x420443: (0xC, "4u * rt_bel_playoff_stage(esi, 0x3u)"),    # cmp [edx + 0xc], ebx
    # The play-off's matches carry its stage number (+0x42 of the fixture), which League::add_result 0x685d30 checks
    # against +0x30 (league.cpp 3214 at 0x685e5e): create_playoff_group 0x420230 gets the dates (vtable +0x3c,
    # 0x4203a0) and makes the group (0x4203e1) with the play-off's own stage. create_schedule 0x41f340 makes the
    # play-off dates for that stage (0x41f975, obj in edi) and the match news 0x420790 tells it (0x420839, obj in esi).
    0x4203A0: (0x3, "rt_bel_playoff_stage(esi, 0x3u)"),         # push 3 (dates)
    0x4203E1: (0x3, "rt_bel_playoff_stage(esi, 0x3u)"),         # push 3 (the group's stage)
    0x41F975: (0x3, "rt_bel_playoff_stage(edi, 0x3u)"),         # cmp al, 3
    0x420839: (0x3, "rt_bel_playoff_stage(esi, 0x3u)"),         # cmp al, 3
    # Review: the news of a match (0x4209c0) and the +0x68 handlers (HolFirst 0x5f73d0, BelSecond 0x420d40; the
    # object from rt_period_handler, as esi holds the table there).
    0x420A09: (0x3, "rt_bel_playoff_stage(esi, 0x3u)"),         # BelSecond match news 0x4209c0: cmp al, 3
    0x420DE4: (0x3, "rt_bel_playoff_stage(rt_period_handler(), 0x3u)"),   # cmp cl, 3
    0x5F7480: (0x4, "rt_hol_playoff_stage(rt_period_handler(), 0x4u)"),   # cmp cl, 4
    0x5F7485: (0x5, "rt_hol_playoff_stage(rt_period_handler(), 0x5u)"),   # cmp cl, 5
    0x4205B9: (0xA, "rt_league_period_length(esi, eax & 0xffu, 0xau)"),   # create_period_stage 0x4204d0: period 0
    0x4205CB: (0xC, "rt_league_period_length(esi, eax & 0xffu, 0xcu)"),   # periods 1 and 2
}
assert not set(PERIOD_PATCHES) & set(PATCHES)
PATCHES.update(PERIOD_PATCHES)
# 0.7.2 slice 6: the cups of eastern Europe and Asia (cup_sizes_east.py).
import os as _os  # noqa: E402
import sys as _sys  # noqa: E402

if _os.path.dirname(_os.path.abspath(__file__)) not in _sys.path:
    _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import cup_sizes_east  # noqa: E402

INSNS.update(cup_sizes_east.INSNS)
# 0.7.2 slice 12: Russia and Turkey (sizes_rus_tur.py).
import sizes_rus_tur  # noqa: E402

assert CAPS["rus_first"] <= sizes_rus_tur.RUS_FIRST_TAKE_MAX
assert not set(sizes_rus_tur.INSNS) & set(INSNS)
INSNS.update(sizes_rus_tur.INSNS)
# 0.7.2 slice 11: the Croatian second tier at 18 (sizes_cro.py, D22).
import sizes_cro  # noqa: E402

INSNS.update(sizes_cro.INSNS)
# 0.7.2 slice 14: J1 halves of any count (sizes_asia.py).
import sizes_asia  # noqa: E402

assert not set(sizes_asia.INSNS) & set(INSNS)
INSNS.update(sizes_asia.INSNS)
