/* The league phase of the UEFA club competitions, 2024/25 on (0.6.0 slice 25; research
 * docs/research/2026-10-19-uefa-league-phase.md).
 *
 * The draw (season_uefa_league_draw.c, no game memory, ctest season_uefa_league): 36 clubs in pots by seed. With
 * 8 matches (Champions League, Europa League) the pots are 4 of 9, and each club meets two clubs of each pot, one
 * at home and one away. With 6 matches (Conference League) the pots are 6 of 6, and each club meets one club of
 * each pot; of the pots 1 and 2, 3 and 4, 5 and 6 it plays one at home and one away. Two clubs of one nation do
 * not meet, and a club meets at most two clubs of one nation (regulations of 2024/25, Art. 16.02). When no draw
 * is found with these rules in many tries, the second rule is dropped, then the first (as Art. 16.03 lets UEFA
 * change them). Then the matches go into match days, each club once a match day. */
#ifndef RECOMP_SEASON_UEFA_LEAGUE_H
#define RECOMP_SEASON_UEFA_LEAGUE_H

#define SEASON_LEAGUE_CLUBS 36
#define SEASON_LEAGUE_MAX_DAYS 8
#define SEASON_LEAGUE_DAY_MATCHES (SEASON_LEAGUE_CLUBS / 2)

/* A number from 0 to n - 1 (the game's random number function in the game). */
typedef int (*SeasonLeagueRandom)(int n, void *ctx);

typedef struct {
    signed char home, away;         /* club indexes 0 to 35, in seed order: pot 1 first */
} SeasonLeagueMatch;

typedef struct {
    int days;                       /* the matches of a club: 8 or 6 */
    int relax;                      /* 0: both nation rules held; 1: the "at most two" rule dropped; 2: both */
    SeasonLeagueMatch match[SEASON_LEAGUE_MAX_DAYS][SEASON_LEAGUE_DAY_MATCHES];
} SeasonLeagueDraw;

/* Draws the league phase of 36 clubs in seed order. nation[i] is the nation of club i (any number, equal for one
 * nation). days 8 or 6. Returns 1 and fills out, or 0 when no draw is found (or rnd stops: it returns < 0). */
int season_league_draw(int days, const int *nation, SeasonLeagueRandom rnd, void *ctx, SeasonLeagueDraw *out);

/* The knockout phase play-offs and the round of 16 from the table places (regulations of 2024/25, Art. 19). The
 * play-off ties: places 9 or 10 against 23 or 24, 11 or 12 against 21 or 22, 13 or 14 against 19 or 20, 15 or 16
 * against 17 or 18, which of each pair by draw; the seeded club (9 to 16) plays the second leg at home. Then the
 * round of 16: 1 or 2 against the winner of the 15/16 tie, 3 or 4 the 13/14 tie, 5 or 6 the 11/12 tie, 7 or 8
 * the 9/10 tie. po[2k] (unseeded) and po[2k + 1] (seeded) are the places (1 to 36) of play-off tie k, r16[k]
 * the place (1 to 8) that the bracket gives the winner of tie k. The game plays the play-offs in these ties and
 * the round of 16 as a seeded draw (Q543), so r16 only orders the list. Returns 0 when rnd stops. */
int season_league_knockout(SeasonLeagueRandom rnd, void *ctx, int po[16], int r16[8]);

/* ---- The game only (season_uefa_league.c) ---- */
#include <stdint.h>

typedef struct {
    int day, month, year_off;       /* month 0 to 11; year_off 1 for a match day in the second calendar year */
} SeasonLeagueDay;

/* The match days of comp ('c' Champions League, 'e' Europa League, 'k' Conference League) in the season that
 * starts in year season, and their weekday argument of league_round_set_date 0x68a160. */
const SeasonLeagueDay *season_uefa_league_days(char comp, int season, int *weekday);

/* The round table of a league phase (days match days of 0x41 bytes, game memory), or 0. */
uint32_t season_uefa_league_table(uint32_t year, int days, const SeasonLeagueDay *dates, int weekday);

/* Sorts clubs by the European seed (0x586f60: the entry code, then reputation). 0 on failure. */
int season_uefa_league_sort(uint32_t *clubs, int n);

/* Makes the League_Stage of the league phase of competition object parent: draws the 36 clubs (sorted here by
 * seed: pots in that order), makes its fixtures, and returns the stage object for stage slot stage, or 0. */
uint32_t season_uefa_league_make(uint32_t parent, uint32_t *clubs, int days, int stage, uint32_t name,
                                 const SeasonLeagueDay *dates, int weekday);

/* League::send_group_draw_news of a stage. */
void season_uefa_league_news(uint32_t stage);
/* A load or a new game: no fixed list is pending (season_change_reset, load sweep). */
void season_uefa_league_reset(void);

/* The club at table place (1 is the top) of a stage, or 0. */
uint32_t season_uefa_league_club_at(uint32_t stage, int place);

/* The list of 24 clubs (game memory, 4 bytes each) and tier bytes of the knockout stage after the league phase:
 * slots 0 to 15 the play-off ties of the bracket, slots 16 to 23 places 1 to 8. Places 25 to 36 are out (club
 * +0x1db -1). 0 on failure. */
int season_uefa_league_knockout_list(uint32_t stage, uint32_t list, uint32_t tiers);

#endif
