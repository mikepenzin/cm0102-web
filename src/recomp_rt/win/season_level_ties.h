/* Level clubs whose head-to-head order is still open (0.6.2 open item 167; season_level_ties.c). */
#ifndef RECOMP_SEASON_LEVEL_TIES_H
#define RECOMP_SEASON_LEVEL_TIES_H

#include <stdint.h>

/* One row of a league table, as League::update_positions_and_status 0x684cd0 reads it: matches played (s8 +0x11)
 * and points (s16 +0x19). */
typedef struct LevelRow {
    int played;
    int points;
} LevelRow;

/* 1 when two clubs that have played all their matches are level on points while a club with matches left can still
 * end on the same points: one on those points that can still leave them, or one below that can reach them exactly
 * with wins and draws (a draw 1 point) from its matches left. Under a head-to-head tie-break the order of the level
 * clubs then can still change. matches: the matches of a club (+0xa7), win: the points of a win (+0xc2). Pure. */
int level_ties_open(const LevelRow *rows, int n, int matches, int win);

/* Before and after League::update_positions_and_status 0x684cd0 (season_second_teams.c). In a season game, for a
 * league whose tie-breaks hold head-to-head, while level_ties_open, the call sees the league's tie decider (+0xe6)
 * set, so the original settles no finished club in that call (its test of two finished rows counts every one below
 * while a decider exists). before returns 1 when it set the decider; after clears it again. */
uint32_t season_level_ties_before(uint32_t league);
void season_level_ties_after(uint32_t league, uint32_t set);

#endif
