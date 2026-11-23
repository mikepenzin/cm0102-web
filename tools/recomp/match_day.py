"""A weather postponement keeps the match day's place in its fixture list (0.6.2 item 84;
src/recomp_rt/win/match_day_place.c has the trace).

MATCH_DAY::MATCH_DAY 0x6b3c10 walks the store's fixture list of a day and slice with the list's own cursor. The clash
search of a postponed match's copy (0x5ab510 at 0x6b3d5f, add_fixture at 0x6b3d71) walks the same list with the same
cursor (0x5adba0, 0x5ade64..0x5ade6c), so the rest of the list could be skipped. The place is kept at 0x6b3d2d (the
first instruction of the copy, after the weather check's test at 0x6b3d25) and set back at 0x6b3d76 (the first
instruction after the add). The list is at esp + 0x18 (0x6b3ccf) at both.
"""

INSNS = {
    0x6B3D2D: ("mov ecx, 0x13", "rt_match_day_keep_place(R32(esp + 0x18u)); ecx = 0x13u;"),
    0x6B3D76: ("mov esi, dword ptr [esp + 0x10]", "rt_match_day_set_place(R32(esp + 0x18u)); esi = R32(esp + 0x10u);"),
}
