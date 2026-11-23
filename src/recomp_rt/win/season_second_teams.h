/* Second teams: shared code of the nation rules (open items 111 and 114 to 122; season_second_teams.c).
 *
 * Each nation's file (season_<nation>_second.c) keeps its own rule. The wraps that more than one nation needs live
 * in season_second_teams.c only, because a function of the original takes one wrap:
 *   - League::update_positions_and_status 0x684cd0 (vtable +0x94): the hooks before_status run before it,
 *   - the promotion chain 0x689c20 (vtable +0xb0): the hooks before_chain run before it and after_chain after it,
 *   - the club moves of a new game (season_clubs.c) ask season_second_barred, which asks each nation.
 * A nation adds its hooks with SEASON_SECOND_HOOKS(name) in its own file, so no nation edits this file or
 * season_second_teams.c.
 * Each hook gets the league object (the this of the call) and acts only on its own leagues. */
#ifndef RECOMP_SEASON_SECOND_TEAMS_H
#define RECOMP_SEASON_SECOND_TEAMS_H

#include <stdint.h>

typedef struct SecondTeamHooks {
    void (*before_status)(uint32_t league);    /* before 0x684cd0 */
    void (*before_chain)(uint32_t league);     /* before 0x689c20 */
    void (*after_chain)(uint32_t league);      /* after 0x689c20 (not called while the program unwinds) */
    /* 1 when the club moves of a new game (season_clubs.c) may not put the club (a club record) in the division
     * (a club_comp record). Season games only: season_clubs.c runs only there. */
    int (*barred_at_start)(uint32_t club, uint32_t div);
} SecondTeamHooks;

/* Adds a nation's hooks (at most 16). Called before main by SEASON_SECOND_HOOKS. */
void season_second_register(const SecondTeamHooks *hooks);
#define SEASON_SECOND_HOOKS(var) \
    __attribute__((constructor)) static void season_second_register_##var(void) { season_second_register(&var); }

/* 1 when a nation's rule bars the club from the division at a new game (the hooks barred_at_start). */
int season_second_barred(uint32_t club, uint32_t div);

/* The club and league records the rules read. */
#define ST_CLUB_SIZE 0x245u
#define ST_CLUB_NAME 0x04u
#define ST_CLUB_NATION 0x53u
#define ST_CLUB_DIVISION 0x57u
#define ST_CLUB_REPUTATION 0x80u
#define ST_ROW_SIZE 0x3bu              /* a row of a league table: +0 the club, +0x37 the status */
#define ST_ROW_STATUS 0x37u
#define ST_ROW_POINTS 0x19u            /* s16 (season_bel_second.c) */
#define ST_TITLE 5                     /* the statuses that 0x684cd0 sets */
#define ST_UP 0
#define ST_PLAYOFF 1
#define ST_DOWN 3
#define ST_BARRED 0xfc                 /* the original's mark of a B side whose senior side plays above (0x5dd942) */
#define ST_NONE 0xff

int st_readable(uint32_t p);
int st_env_is(const char *name, const char *value);
/* The club table and its count (0 when none). */
uint32_t st_clubs(int *count);
/* 1 when the club's nation (club +0x53) is the nation of the key (such as 0x9cf408, Norway). */
int st_nation_is(uint32_t club, uint32_t nation_key);
/* The club_comp record of a division key, and the league object of the key (0 when none). */
uint32_t st_comp_of(uint32_t key);
uint32_t st_league_of(uint32_t key);
/* The season of the nation of a division key for the barred_at_start hooks: the year +0x40 of the key's league once
 * the leagues exist (the season played, or the new one after a season change adds 1), else the start year (a new
 * game). The size step (season_sizes.c up_ok) and the play-offs (season_playoffs.c barred) ask the hooks in later
 * seasons too, so the start year alone would give the wrong rule there. */
int st_season_year(uint32_t key);
/* The table of a league and its row count (0 when none). */
uint32_t st_table_of(uint32_t league, int *n);
/* A thiscall of the original from a wrap: the registers stay as they were unless the program unwinds. */
uint32_t st_call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args);

/* A rule's test for a club that may not take a place in the league. */
typedef int (*StBarred)(uint32_t club);

/* Before 0x684cd0: each barred row gets 0xfc (a row of status 3 keeps it), so the next club takes the title,
 * promotion and the play-off. log: a line "recomp: <tag>: <club> may not go up from <what>" per row. */
void st_mark(uint32_t league, StBarred barred, const char *tag, const char *what, int log);

/* A complete table where a barred club holds the title, promotion or the play-off (a save of an older build): the
 * clubs below take the places in table order, and each barred club gets 0xfc. Runs again with no change. */
void st_shift_up(uint32_t league, StBarred barred, const char *tag, const char *when, int log);

/* Tests only: each row of the league where side(club) holds gets the points of the middle club of the others, so a
 * planted case (an old save) does not go down or up by the table before the step under test. Call before 0x684cd0. */
void st_test_mid_table(uint32_t league, StBarred side);

#endif
