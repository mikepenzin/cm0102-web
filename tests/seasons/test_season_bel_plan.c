/* The rules of the Belgian play-offs by season (season_bel_plan.c). The facts are those of the rules in that file:
 * RSSSF belg2010 to belg2019 and the en, nl and fr Wikipedia season pages. */
#include "season_bel_plan.h"

#include <stdio.h>

static int fails;

static void want(const char *what, int season, int got, int expected)
{
    if (got == expected) return;
    printf("FAIL %s, season %d: got %d, want %d\n", what, season, got, expected);
    fails++;
}

static void forms(void)
{
    for (int y = 2001; y <= 2027; y++) {
        SeasonBelForm f;
        int on = season_bel_form(y, &f), played = y >= 2009 && y <= 2018;
        want("a season with the rounds", y, on, played);
        if (!played) continue;
        int one = y >= 2016;
        want("legs of the final", y, f.final_legs, one ? 1 : 2);
        want("legs of the Europa match", y, f.europa_legs, one ? 1 : 2);
        want("second-division clubs", y, f.b_clubs, one ? 3 : 0);
        want("stages", y, f.stages, one ? 4 : 2);
    }
}

static void europa_place(void)
{
    /* 2014/15 and 2017/18: the cup winner ended 2nd, so the 5th played. */
    want("Europa place", 2014, season_bel_europa_place(2014, 2), 5);
    want("Europa place", 2017, season_bel_europa_place(2017, 2), 5);
    /* 2009/10 and 2010/11: the cup winner ended 2nd, and the 4th still had the place. */
    want("Europa place", 2009, season_bel_europa_place(2009, 2), 4);
    want("Europa place", 2010, season_bel_europa_place(2010, 2), 4);
    /* 2012/13, 2013/14 and 2016/17: the cup winner ended 5th or 6th. */
    want("Europa place", 2012, season_bel_europa_place(2012, 5), 4);
    want("Europa place", 2016, season_bel_europa_place(2016, 6), 4);
    /* 2015/16 and 2018/19: the cup winner played below Play-off I (0: no place in the top six). */
    want("Europa place", 2015, season_bel_europa_place(2015, 0), 4);
    want("Europa place", 2018, season_bel_europa_place(2018, 0), 4);
}

static void groups(void)
{
    static const int a[] = {7, 9, 12, 14}, b[] = {8, 10, 11, 13, 15};
    for (int i = 0; i < 4; i++) want("group of a place", a[i], season_bel_group_of_place(a[i]), 0);
    for (int i = 0; i < 5; i++) want("group of a place", b[i], season_bel_group_of_place(b[i]), 1);
    want("group of a place", 6, season_bel_group_of_place(6), -1);
    want("group of a place", 16, season_bel_group_of_place(16), -1);
    want("group of second-division club 1", 1, season_bel_group_of_b(0), 0);
    want("group of second-division club 2", 2, season_bel_group_of_b(1), 1);
    want("group of second-division club 3", 3, season_bel_group_of_b(2), 0);
    want("group of second-division club 4", 4, season_bel_group_of_b(3), -1);
}

static void entrants(void)
{
    int row[2];
    /* The rows of an 18-club table: the 17th in 2005/06 and 2006/07. */
    want("top-flight clubs", 2005, season_bel_top_entrants(2005, 18, row), 1);
    want("row", 2005, row[0], 16);
    want("top-flight clubs", 2006, season_bel_top_entrants(2006, 18, row), 1);
    want("row", 2006, row[0], 16);
    want("top-flight clubs", 2007, season_bel_top_entrants(2007, 18, row), 0);
    /* 2008/09: the 15th and 16th of 18. */
    want("top-flight clubs", 2008, season_bel_top_entrants(2008, 18, row), 2);
    want("row", 2008, row[0], 14);
    want("row", 2008, row[1], 15);
    /* 2009/10: the 15th of 16. */
    want("top-flight clubs", 2009, season_bel_top_entrants(2009, 16, row), 1);
    want("row", 2009, row[0], 14);
    /* 2010/11 to 2014/15: the winner of the relegation play-off, one of the last two rows. */
    for (int y = 2010; y <= 2014; y++) {
        want("top-flight clubs", y, season_bel_top_entrants(y, 16, row), SEASON_BEL_PO3_WINNER);
        want("relegation play-off", y, season_bel_relegation_playoff(y), 1);
    }
    want("relegation play-off", 2009, season_bel_relegation_playoff(2009), 0);
    want("relegation play-off", 2015, season_bel_relegation_playoff(2015), 0);
    want("top-flight clubs", 2015, season_bel_top_entrants(2015, 16, row), 0);
    want("top-flight clubs", 2004, season_bel_top_entrants(2004, 18, row), 0);
}

static void final_round(void)
{
    /* A second-division club wins: every top-flight club goes down. */
    want("clubs down", 1, season_bel_final_round_down(1, -1, 0), 1);
    want("clubs down", 2, season_bel_final_round_down(2, -1, 1), 1);
    /* A top-flight club wins: it stays, and the other one goes down. */
    want("the winner stays", 1, season_bel_final_round_down(1, 0, 0), 0);
    want("the winner stays", 2, season_bel_final_round_down(2, 1, 1), 0);
    want("the other goes down", 2, season_bel_final_round_down(2, 1, 0), 1);
}

static void second_up(void)
{
    want("who goes up", 2014, season_bel_second_up(2014), SEASON_BEL_UP_BOTH);
    want("who goes up", 2015, season_bel_second_up(2015), SEASON_BEL_UP_CHAMPION);
    for (int y = 2016; y <= 2019; y++) want("who goes up", y, season_bel_second_up(y), SEASON_BEL_UP_FINAL);
    want("who goes up", 2020, season_bel_second_up(2020), SEASON_BEL_UP_BOTH);
}

int main(void)
{
    forms();
    europa_place();
    groups();
    entrants();
    final_round();
    second_up();
    printf(fails ? "FAILED\n" : "PASS\n");
    return fails != 0;
}
