/* The Russian First Division at its real count (0.7.2 league sizes, slice 12; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D22). season_sizes_rus.c, tools/recomp/sizes_rus_tur.py. */
#ifndef RECOMP_SEASON_SIZES_RUS_H
#define RECOMP_SEASON_SIZES_RUS_H

/* RusFirst::create_table 0x7ea6c0 takes the clubs of the division into a stack array of 18 pointers at esp + 0x10
 * (`cmp bl, 0x12` at 0x7ea77d, store at 0x7ea787). The array runs into the text buffer at esp + 0x58, which only the
 * error path of a failed allocation uses (0x7ea6eb..0x7ea741, before the loop). The next buffer starts at
 * esp + 0x158 (frame 0x248, sub esp at 0x7ea6c0), so 22 pointers (to esp + 0x68) are safe. */
#define RUS_FIRST_TAKE 18
#define RUS_FIRST_TAKE_MAX 22

/* The bound of the take loop for a table of `count` clubs: the original's 18, or the count when it is larger, up to
 * RUS_FIRST_TAKE_MAX. A smaller count keeps 18: the loop takes the clubs of the division up to 18, orders them
 * (0x4afbd0: a qsort, then random swaps) and adds the first `count` (0x7ea7bf), so a division of `count` clubs still
 * enters in full. */
static inline int season_rus_first_take(int count)
{
    if (count <= RUS_FIRST_TAKE) return RUS_FIRST_TAKE;
    return count > RUS_FIRST_TAKE_MAX ? RUS_FIRST_TAKE_MAX : count;
}

#endif
