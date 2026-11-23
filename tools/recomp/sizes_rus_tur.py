"""Russia and Turkey at their real counts (0.7.2 league sizes, slice 12; spec
docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D22).

Instruction rows (address: (the instruction in the original, C code)) for league_sizes.INSNS. The runtime is
src/recomp_rt/win/season_sizes_rus.c. The table allocations and the date templates of RusFirst, TurFirst and
TurSecond are the size engine's (league_sizes.py GROWN). Each rt_ function returns the original's constant in the
base game (Q152).
"""

RUS_FIRST_TAKE_MAX = 22   # season_sizes_rus.h: the pointers that the stack frame of 0x7ea6c0 holds

INSNS = {
    # RusFirst::create_table 0x7ea6c0: the bound of the take loop (18 pointers at [esp + 0x10]); jge at 0x7ea780.
    0x7EA77D: ("cmp bl, 0x12", "t0 = (ebx & 0xffu); t1 = rt_rus_first_take(ebp) & 0xffu; t2 = ((t0 - t1 - 0) & 0xffu); "
                               "SETF(K_SUB | SZ1, t0, t1, t2, 0);"),
}
