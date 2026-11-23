"""A constant of ArgSecond (the Argentine second division) that follows the form of the season in a season game (0.7.2
league sizes, slice 16; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D10). Runtime:
src/recomp_rt/win/season_sizes_arg_second.c.

- ArgSecond::fill_league_table 0x407cd0 takes the clubs of division [0x9cf658] up to its count (+0x3e, 0x407da2..
  0x407daa) and, when it found exactly 24 (cmp bl, 0x18 at 0x407dc0), moves the fixed club of [0x9d020c] into the
  division and adds it as the 25th (0x407dc5..0x407e0d). Any count other than +0x3e then fails the table
  (0x407e0f..0x407e1c, box arg_second.cpp 85 at 0x406ce0). In a season game the count is the row's
  (rt_league_clubs at 0x407ce2, league_sizes.py), so a table of 24 would get a 25th club and fail.
  rt_arg_second_fixed_at gives 24 for the class and 0xff (no count) for one table of the row: no fixed club.
"""

PATCHES = {
    0x407DC0: (0x18, "rt_arg_second_fixed_at(esi, 0x18u)"),
}
