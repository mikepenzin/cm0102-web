/* Promotion play-offs in leagues whose class has none (0.6.0 slice 19). See season_playoffs.c. */
#ifndef RECOMP_SEASON_PLAYOFFS_H
#define RECOMP_SEASON_PLAYOFFS_H

#include <stdint.h>

/* The wrap of the empty function 0x5a8f60 (advance_stage, season_uefa_cup.c) calls it first: 1 when obj is a
 * league of this kind whose table is complete and the play-off stage is built now (the wrap then returns). */
int season_playoffs_advance(uint32_t obj);
/* The same play-off for a league whose class has its own play-off code (the I liga's own play-off, season_pol_first.c). */
int season_playoffs_build_for(uint32_t obj);

/* 1 when the rules row of file and season gives a play-off that this kind plays (4 places in a row just below
 * the direct places); *direct (may be NULL) gets the direct promotion places. */
int season_playoffs_shape(const char *file, int season, int *direct);
/* The draw and match days of a round of a stage, in days from today (season_ned_playoff.c). */
void season_playoffs_round_dates(uint32_t table, int round, int draw_in, int match_in, uint32_t a7, uint32_t venue,
                                 uint32_t year);
/* The semi-finals of the play-off of six before Cup::draw_round 0x51c410 builds them (item 273). */
void season_playoffs_draw_round(uint32_t cup, int round);

/* The play-off winners that the rules row of file and season sends up although this kind plays no play-off for
 * them: 2 for 4 places just below the direct places in two ties whose winners both go up (Série B from 2025:
 * "two-legged ties 3rd v 6th and 4th v 5th, the two winners go up"), else 0. The game plays no such play-off, so
 * the nearest form (Q247) sends the first two play-off places up directly: season_sizes.c adds them to the direct
 * places. Without them Série A sent 4 down and took 2 up, and lost 2 clubs every season (0.6.2 open item 63).
 * season_playoff_winners.c, host code only. */
int season_playoffs_winners_up(const char *file, int season);

/* 2 when the rules row of file and season sends the two finalists of a play-off of its top 8 up and has no direct
 * places beyond them (Série B 2002 to 2005, rows 2001 to 2004: "top 8 ... finalists promoted" or "top 2 promoted",
 * up 2, play-off places 1 to 8), else 0. season_sizes_bra.c plays it. Host code only. */
int season_playoffs_finalists_up(const char *file, int season);

/* The club of the Segunda Liga that played the tie with the Primeira Liga in an edition, from the rows of its last
 * table in order (club, n), or 0 when that edition has no tie. The size step gives no extra place to this club while it
 * stays in the Segunda Liga: it lost the tie (issue 154). */
uint32_t season_playoffs_tie_club(uint32_t lower, int edition, const uint32_t *club, int n);

/* After set_up_rules of a league (season_sizes_rules calls it): the places and the stage count of the play-off
 * of the season, or none. */
void season_playoffs_rules(uint32_t obj);

/* Around game_season_update_pass 0x5d5ae0 (season_change.c): an entry whose play-off stage is not complete
 * waits for it (season_playoffs.c). hold returns 1 when it changed an update day; release puts it back. */
int season_playoffs_hold(void);
void season_playoffs_release(int held);

/* Before the European places (collect_european_clubs 0x904040, season_uefa_seeding.c), open item 94: Uefa_Seeding
 * reads each club's division and place of the last season (club +0x5b, +0x5f), which the season change writes
 * (League::record_final_positions 0x68aa30, vtable +0xa4, from the chain 0x689c20 or a class's own, SpaFirst
 * 0x850700). An entry that waits for a play-off past its update day (above) changes its season after the places:
 * in a 2021 game the European places came on day 171 of 2022 and Spain's change on day 176, so the Champions League
 * took the Segunda's top clubs and the bottom of the Primera. For each waiting entry whose top league has not changed
 * its season and whose table is complete (check_complete, vtable +0x10), this writes the final places of that league
 * and of each division below it along +0x20, as the chain 0x689c20 does (SpaFirst 0x850700 also writes its Segunda B
 * groups, which no European place reads). Only a play-off stage is left; the chain writes the same values again at the
 * change. Only in a season game with changes. */
void season_playoffs_final_places(void);

#endif
