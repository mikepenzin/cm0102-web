/* The Argentine second division at its real size (0.7.2 league sizes, slice 16; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D10 and D22).
 *
 * The class ArgSecond plays the 2001/02 form only: an Apertura of 25 clubs in one meeting in the league object (25
 * dates of create_schedule(-1) 0x406f00, one meeting at 0x407e62), then the other 24 in three Clausura zones of 8
 * (create_zone_stages 0x4081b0 reads rows 1 to 24 at 0x408233..0x40838a), a Reducido of 8 (0x408630) and two promotion
 * ties against the Primera (0x408910). fill_league_table 0x407cd0 takes exactly 25 (0x407ce2) and adds a fixed club as
 * the 25th (0x407dc0); init_relegation_averages 0x4093f0 and set_predicted_positions 0x409fe0 hold 25.
 *
 * In a season game with the sizes on (league_sizes.py CLASSES and GROWN, cap 38, mode shuffled), every edition whose
 * row is not that form plays one table of the row's count:
 * - the meetings of the row; a row of halves (2002/03 to 2006/07) plays both halves as one table of twice the
 *   meetings; a row of zones (2019/20 on) plays one table of one meeting, since no source gives the zone lists;
 * - the dates of both class templates (Apertura and Clausura), to the rounds of the count (D4, D5);
 * - no stages: the promotion places of the row go to the next clubs of the table (D2), the relegation places (at
 *   most 4, more with changing rules on) to the third division through the Primera's swap 0x4068d0;
 * - the averages, the predicted positions and the honours take the count.
 * The base game, RECOMP_LEAGUE_SIZES=off and the 2001/02 row keep the class. */
#ifndef RECOMP_SEASON_SIZES_ARG_SECOND_H
#define RECOMP_SEASON_SIZES_ARG_SECOND_H

#include <stdint.h>

/* 1 when an edition of the second division plays one table (its row's form), 0 for the class or any other league. */
int season_arg_second_one_table(uint32_t obj);

/* The meetings of the one table of the second division, else meetings. */
int season_arg_second_meetings(uint32_t obj, int meetings);

/* After season_sizes_rules of the second division (set_up_rules 0x407e50): the places and the stage count of the
 * edition. */
void season_arg_second_rules(uint32_t obj);

/* season_sizes_schedule, stage -1: the dates of both class templates for one table (template in eax, its count at
 * count_at). Returns 1 when it joined them. */
int season_arg_second_join(uint32_t self, uint32_t count_at);

/* 1 when the second division has no stage this edition (one table): ArgSecond::record_season_honours 0x408eb0 would
 * jump through the absent stage 0 (season_sizes_bra.c wraps it). */
int season_arg_second_no_stage(uint32_t obj);

/* tools/recomp/arg_second.py: the count at which fill_league_table adds its fixed club (24, 0x407dc0). */
uint32_t rt_arg_second_fixed_at(uint32_t obj, uint32_t original);

/* The promotion places of a rules row of the second division, or -1 (item 291). */
int season_arg_second_row_up(int row);
int season_arg_second_row_down(int row);

#endif
