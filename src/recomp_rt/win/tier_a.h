#ifndef RECOMP_TIER_A_H
#define RECOMP_TIER_A_H

/* The Tier A switch RECOMP_TIER_A=1 (test only, 0.6.2 slice 3, tier_a.c): a game that plays only the
 * competitions, to reach far years quickly. Read once at the start (season_init), off by default. */
int tier_a(void);

/* 1 when the switch is on and the game is made (0xdd711c, tier_a.c): the Tier A skips act. */
int tier_a_in_play(void);

#endif
