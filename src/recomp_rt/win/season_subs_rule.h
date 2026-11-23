/* The substitute rule of a competition file and season, from the rules table (0.6.0 slice 17; spec
 * docs/specs/2026-10-16-seasons-0.6.0-spec.md, slice 17). No game memory: the lookup is host code, so a test can
 * run it (tests/seasons/test_season_subs.c). The wraps and hooks that write the game's memory are in
 * season_kind_subs.c. */
#ifndef RECOMP_SEASON_SUBS_RULE_H
#define RECOMP_SEASON_SUBS_RULE_H

#define SEASON_SUBS_USED 1       /* the table gives the substitutes used */
#define SEASON_SUBS_BENCH 2      /* the table gives the named substitutes (the bench) */

/* The match engine holds at most 9 in each of the two nibbles of a fixture's word +0x4d: init_match 0x6b7fe6
 * (named, low nibble) and 0x6b8006 (used, high nibble) clamp a larger count to 9. So the rule never gives more. */
#define SEASON_SUBS_MAX 9

/* The first season of 5 substitutes (IFAB, 2020/21): an unknown count of a season from it on keeps no count of a
 * season before it (item 274). */
#define SEASON_SUBS_ERA 2020

/* The substitutes used and the named substitutes of the competition class file (eng_prm) in the season (the
 * start year: 2001 for 2001/02). It reads the first row of the file in that season that holds a count, one
 * count at a time: a row with "unknown", "n/a", "as the Champions League" or a description gives no count. A
 * count above SEASON_SUBS_MAX gives SEASON_SUBS_MAX. A count that the season's rows give only as "unknown" keeps the
 * latest count of an earlier season of the file (item 274): rows "as the Champions League" follow the main row, a
 * season with an n/a, not held or description row keeps none and stops the search back, and no count of a season
 * before SEASON_SUBS_ERA reaches a season from it on. *used and *bench (either may be NULL) are written only for
 * the counts found. Returns the bits SEASON_SUBS_USED and SEASON_SUBS_BENCH of the counts found, 0 when the file
 * has no such row (also for a season outside the table). */
int season_subs_rule(int season, const char *file, int *used, int *bench);

/* season_subs_rule, and the season of the row that gave each count found (*used_from, *bench_from; either may be
 * NULL), written only for the counts found. */
int season_subs_rule_from(int season, const char *file, int *used, int *bench, int *used_from, int *bench_from);

#endif
