/* The CONCACAF World Cup qualifying in the 2026 shape (0.6.2 item 214, season_concacaf26.c). */
#ifndef RECOMP_SEASON_CONCACAF26_H
#define RECOMP_SEASON_CONCACAF26_H

#include <stdint.h>

/* 1 when the CONCACAF section `cc` (WcConcacafCup) plays the 2026 shape: its stage count +0x2c is 9. */
int season_cc26_on(uint32_t cc);

/* Called by the wrap of WcConcacafCup::create_round_table 0x914de0 (season_intl_qual.c) after the original made the
 * table `table` of stage `stage`, with its round count at `count`: the first round of the 2026 shape for stage -1.
 * Returns 1 when it changed the table. */
int season_cc26_round_table(uint32_t cc, int stage, uint32_t table, uint32_t count);

/* The two CONCACAF teams of the play-off tournament (season_wcq26.c tournament_six): the best two of the third
 * round's groups after their direct places, ranked across the groups. Returns 1 for a section in the 2026 shape. */
int season_cc26_tournament(uint32_t cc, uint32_t out[2]);

#endif
