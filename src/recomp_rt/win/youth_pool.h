/* The youth pool of each nation (0.6.2 slice 20, open item 52): the rule, without game memory, for the runtime
 * (youth_pool.c) and the test (tests/seasons/test_youth_pool.c). */
#ifndef YOUTH_POOL_H
#define YOUTH_POOL_H

#define YOUTH_POOL_SQUAD 23        /* a nation with fewer players fills no squad (SQUAD of the history checker) */
#define YOUTH_POOL_OLDER_MAX 349   /* 0x15d: a nation with more players at the start always gets young new players */

/* The age in whole years on the day (day of the year from 0, as the game date 0xae2c90) of a person born on
 * bday of byear. */
int youth_pool_age(int year, int day, int byear, int bday);

/* 1 when a nation with this many players has too few young ones: fewer than 1 in 20 under 21 or fewer than 1
 * in 10 under 23. At the start of a game no nation with a squad of players is that short (the lowest shares in
 * the 2001 data are 6 % under 21 and 13 % under 23). */
int youth_pool_short(int players, int under_21, int under_23);

/* 1 when a nation with this many players will have none under 21 or none under 23 in a year: none under 20 or
 * none under 22 today. */
int youth_pool_runs_out(int players, int under_20, int under_22);

/* 1 when the new player rule of the original (0x7abac0, 0x7abe87..0x7abeab) gives a nation the older ages, 23 to 27 in
 * most cases: the nation is not loaded (league flags +0x11c are 0) and it had 349 players or fewer at the start
 * (+0x88, counted once by database_trim_staff_tables 0x524d70). */
int youth_pool_older_rule(int league_flags, int start_players);

#endif
