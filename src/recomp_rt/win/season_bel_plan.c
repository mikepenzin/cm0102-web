/* The rules of the Belgian play-offs by season (issue 150). Sources: RSSSF tablesb/belg2010 to belg2019, en, nl and fr
 * Wikipedia season pages of the Belgian Pro League, First Division A, Second Division and First Division B, and
 * "Belgian Second Division play-offs", read 18 November 2026.
 *
 * - Play-off II final: the two group winners, two legs 2009/10 to 2015/16, one match 2016/17 to 2018/19.
 * - Europa League play-off: the final's winner against a Play-off I club, two legs 2009/10 to 2015/16, one match
 *   2016/17 to 2018/19. The Play-off I club is the 4th. From 2014/15 it is the 5th when the cup winner ends in the top
 *   four (2014/15 Club Brugge 2nd, Charleroi 5th played; 2017/18 Standard 2nd, Genk 5th played). In 2009/10 and 2010/11
 *   the cup winner ended 2nd and the 4th kept the tie.
 * - Second-division clubs in Play-off II, 2016/17 to 2018/19: group A the 7th, 9th, 12th, 14th and the clubs ranked 1
 *   and 3, group B the 8th, 10th, 11th, 13th, 15th and the club ranked 2.
 * - The relegation play-off of the 15th and 16th: 2010/11 to 2014/15. The loser goes down, the winner plays the second
 *   division's final round and must win it to stay up.
 * - Top-division clubs in the final round: the 17th in 2005/06 and 2006/07, the 15th and 16th in 2008/09, the 15th in
 *   2009/10. */
#include "season_bel_plan.h"

#include <stddef.h>

#define FIRST_FINAL 2009
#define LAST_TWO_LEGS 2015
#define LAST_FINAL 2018
#define FIRST_FIFTH 2014        /* the first season when the cup winner's place moves the tie to the 5th */

int season_bel_form(int season, SeasonBelForm *f)
{
    if (season < FIRST_FINAL || season > LAST_FINAL) return 0;
    int two = season <= LAST_TWO_LEGS;
    if (f) {
        f->final_legs = two ? 2 : 1;
        f->europa_legs = two ? 2 : 1;
        f->b_clubs = two ? 0 : 3;
        f->stages = two ? 2 : 4;    /* the final and the Europa match, after the two groups when they are stages */
    }
    return 1;
}

int season_bel_europa_place(int season, int cup_winner_place)
{
    return season >= FIRST_FIFTH && cup_winner_place >= 1 && cup_winner_place <= 4 ? 5 : 4;
}

int season_bel_group_of_place(int place)
{
    switch (place) {
    case 7: case 9: case 12: case 14: return 0;
    case 8: case 10: case 11: case 13: case 15: return 1;
    default: return -1;
    }
}

int season_bel_group_of_b(int b)
{
    return b == 0 || b == 2 ? 0 : b == 1 ? 1 : -1;
}

int season_bel_relegation_playoff(int season)
{
    return season >= 2010 && season <= 2014;
}

int season_bel_top_entrants(int season, int n, int row[2])
{
    if (season_bel_relegation_playoff(season)) return SEASON_BEL_PO3_WINNER;
    if (season == 2005 || season == 2006) {
        row[0] = n - 2;
        return 1;
    }
    if (season == 2008) {
        row[0] = n - 4;
        row[1] = n - 3;
        return 2;
    }
    if (season == 2009) {
        row[0] = n - 2;
        return 1;
    }
    return 0;
}

/* 2015/16: no final round (reform year; RSSSF belg2016). 2016/17 to 2019/20: the final of First Division B. */
int season_bel_second_up(int season)
{
    return season == 2015 ? SEASON_BEL_UP_CHAMPION
         : season >= 2016 && season <= 2019 ? SEASON_BEL_UP_FINAL : SEASON_BEL_UP_BOTH;
}

int season_bel_final_round_down(int entrants, int winner, int k)
{
    return k >= 0 && k < entrants && k != winner;
}
