"""Constants of ArgPrm (the Argentine Primera) that follow the club count in a season game (0.7.2 league sizes,
slice 15; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D9 and D22). Runtime: rt_arg_count
(src/recomp_rt/win/season_sizes_arg.c) gives the constant plus (count - 20): the original's constant in the base game
and whenever the Primera holds the class's 20 clubs.

- ArgPrm::record_season_honours 0x405ca0 sends down the clubs of the last two rows of the relegation averages
  (+0x34 - 2 at 0x405cb7, +0x34 - 1 at 0x405d77; the averages follow the count, season_sizes_arg.c) and gives each
  a place through set_club_progress (vtable +0x68, stage -2): 19 for the first (push 0x13 three times at
  0x405cf5..0x405cf9) and 20 for the second (push 0x14 at 0x405dae..0x405db2), the last two places of 20.
- ArgPrm::set_club_progress 0x406b80 caps the place, the best and the worst place of the stages -1, 0 and 1 at 18
  (cmp at 0x406ba4, 0x406bb1, 0x406bbc; mov at 0x406bab, 0x406bb6, 0x406bc1), two under the count of 20.
- ArgPrm::create_schedule 0x404570 makes the Clausura (stage 0) only when the Apertura's template (+0xa9, written by
  set_up_rules through create_schedule(-1)) holds 19 dates: cmp word ptr [edi + 0xa9], 0x13 at 0x4054a8, else the box
  arg_prm.cpp 457 (0x405503). rt_arg_apertura_dates gives the Apertura's own count in a season game, where the
  runtime made it the rounds of the row (16 for the zones of 2025, season_sizes_arg.c), and 19 in the base game.
"""

PATCHES = {
    0x4054A8: (0x13, "rt_arg_apertura_dates(edi, 0x13u)"),
    0x405CF5: (0x13, "rt_arg_count(0x13u)"),
    0x405CF7: (0x13, "rt_arg_count(0x13u)"),
    0x405CF9: (0x13, "rt_arg_count(0x13u)"),
    0x405DAE: (0x14, "rt_arg_count(0x14u)"),
    0x405DB0: (0x14, "rt_arg_count(0x14u)"),
    0x405DB2: (0x14, "rt_arg_count(0x14u)"),
    0x406BA4: (0x12, "rt_arg_count(0x12u)"),
    0x406BAB: (0x12, "rt_arg_count(0x12u)"),
    0x406BB1: (0x12, "rt_arg_count(0x12u)"),
    0x406BB6: (0x12, "rt_arg_count(0x12u)"),
    0x406BBC: (0x12, "rt_arg_count(0x12u)"),
    0x406BC1: (0x12, "rt_arg_count(0x12u)"),
}
