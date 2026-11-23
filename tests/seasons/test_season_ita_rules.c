/* The Italian rules of the league sizes that depend only on numbers (season_ita_rules.h; 0.7.2 gap rows 30 and 32). */
#include "season_ita_rules.h"

#include <stdio.h>

static int fails;

static void check(int got, int want, const char *what)
{
    if (got != want) {
        printf("FAIL %s: %d, want %d\n", what, got, want);
        fails++;
    }
}

int main(void)
{
    /* Row 30. The class's layout at 20 clubs with 4 up and 4 down: 1, then 2 for rows 1 to 3, 3 for rows 16 to 19
     * (0x647057..0x647077). */
    const int class20[20] = {1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3};
    for (int i = 0; i < 20; i++) check(ita_serie_b_zone(2002, 20, 4, 4, i), class20[i], "2002/03 at 20");
    /* 2003/04, 24 clubs: 5 promoted, 3 relegated directly (22nd to 24th). */
    check(ita_serie_b_zone(2003, 24, 5, 3, 0), 1, "2003/04 champion");
    check(ita_serie_b_zone(2003, 24, 5, 3, 4), 2, "2003/04 5th");
    check(ita_serie_b_zone(2003, 24, 5, 3, 5), 0, "2003/04 6th");
    check(ita_serie_b_zone(2003, 24, 5, 3, 20), 0, "2003/04 21st");
    check(ita_serie_b_zone(2003, 24, 5, 3, 21), 3, "2003/04 22nd");
    check(ita_serie_b_zone(2003, 24, 5, 3, 23), 3, "2003/04 24th");
    /* 2004/05, 22 clubs: 2 promoted directly, 3 relegated directly (20th to 22nd). */
    check(ita_serie_b_zone(2004, 22, 2, 3, 1), 2, "2004/05 2nd");
    check(ita_serie_b_zone(2004, 22, 2, 3, 2), 0, "2004/05 3rd");
    check(ita_serie_b_zone(2004, 22, 2, 3, 18), 0, "2004/05 19th");
    check(ita_serie_b_zone(2004, 22, 2, 3, 19), 3, "2004/05 20th");
    /* From 2005/06 the head-to-head table settles a tie: no zone, so no tie-break match, at any count. */
    for (int y = 2005; y <= 2030; y++)
        for (int n = y == 2018 ? 19 : 22, i = 0; i < n; i++) check(ita_serie_b_zone(y, n, 2, 3, i), 0, "no zone from 2005");
    check(ita_serie_b_zone(2019, 20, 2, 3, 0), 0, "2019/20 at 20");
    check(ita_serie_b_zone(2003, 24, 5, 3, 24), 0, "a row past the table");

    /* Open item 254. Serie A: to 2004/05 the class's zones (title, European places, relegation); 2005/06 to
     * 2021/22 none (head-to-head table); from 2022/23 the title and the relegation line only. */
    check(ita_serie_a_zone(2003, 18, 3, 0, 1), 1, "2003/04 title keeps the class");
    check(ita_serie_a_zone(2004, 20, 3, 3, 2), 2, "2004/05 European place keeps the class");
    check(ita_serie_a_zone(2004, 20, 3, 17, 0), 5, "2004/05 18th is relegated");
    check(ita_serie_a_zone(2004, 20, 3, 16, 0), 0, "2004/05 17th is safe");
    check(ita_serie_a_zone(2004, 20, 3, 18, 5), 5, "2004/05 a class 5 in the relegation rows");
    check(ita_serie_a_zone(2004, 20, 3, 14, 5), 0, "2004/05 a class 5 above the line goes to 0");
    check(ita_serie_a_zone(2004, 20, 3, 19, 0), 5, "2004/05 20th is relegated");
    for (int y = 2005; y <= 2021; y++)
        for (int i = 0; i < 20; i++) check(ita_serie_a_zone(y, 20, 3, i, i < 4 ? 2 : 0), 0, "no Serie A zone 2005 to 2021");
    for (int y = 2022; y <= 2030; y++)
        for (int i = 0; i < 20; i++)
            check(ita_serie_a_zone(y, 20, 3, i, i == 0 ? 1 : i < 4 ? 2 : 0), i == 0 ? 1 : i >= 17 ? 5 : 0,
                  "Serie A title and relegation from 2022");
    check(ita_serie_a_zone(2022, 20, 3, 20, 0), 0, "a row past the table");

    /* Row 32. compare_clubs_by_seeding_pot 0x640590 at 20 clubs: B 3rd to 6th pot 4, 7th to 14th 3, 15th to 18th 2. */
    const ItaCupClub base = {1, 1, 0, 1, 1, 0};
    ItaCupClub c = base;
    for (int p = 1; p <= 24; p++) {
        c.place = p;
        int want = p >= 3 && p <= 6 ? 4 : p >= 7 && p <= 14 ? 3 : p >= 15 && p <= 18 ? 2 : 1;
        check(ita_cup_pot(&c, 20), want, "B place at 20");
        check(ita_cup_pot(&c, 0), want, "B place, count unknown");
    }
    /* The last Serie B of 22 (2004/05 to 2017/18): 19th and 20th as 15th to 18th; 21st and 22nd pot 1. */
    for (int p = 15; p <= 22; p++) {
        c.place = p;
        check(ita_cup_pot(&c, 22), p <= 20 ? 2 : 1, "B place at 22");
    }
    c.place = 22;
    check(ita_cup_pot(&c, 24), 2, "22nd of 24");
    c.place = 18;
    check(ita_cup_pot(&c, 19), 1, "18th of 19");
    c.place = 17;
    check(ita_cup_pot(&c, 19), 2, "17th of 19");
    /* Relegated from Serie A into Serie B: pot 4. Promoted from below into Serie B: pot 2. No last division: 1. */
    ItaCupClub down = {1, 0, 1, 1, 0, 18};
    check(ita_cup_pot(&down, 22), 4, "down from A");
    ItaCupClub up = {1, 0, 0, 1, 0, 1};
    check(ita_cup_pot(&up, 22), 2, "up from C1");
    ItaCupClub up_to_a = {1, 1, 0, 0, 0, 3};
    check(ita_cup_pot(&up_to_a, 22), 4, "B 3rd now in Serie A");
    ItaCupClub stay_a = {1, 0, 1, 0, 0, 5};
    check(ita_cup_pot(&stay_a, 22), 1, "Serie A club");
    ItaCupClub none = {0, 0, 0, 1, 0, 0};
    check(ita_cup_pot(&none, 22), 1, "no last division");
    /* The count: part of a database table in the start season keeps the original; a played season stays. */
    check(ita_cup_b_count(17, 2004, 2004), 0, "17 from a 2004 database");
    check(ita_cup_b_count(24, 2004, 2004), 24, "24 from a 2004 database");
    check(ita_cup_b_count(19, 2019, 2018), 19, "19 played in 2018/19");
    check(ita_cup_b_count(22, 2006, 2005), 22, "22 played in 2005/06");
    check(ita_cup_b_count(0, 2006, 2005), 0, "no Serie B clubs");
    check(ita_cup_b_count(20, 2004, 2004), 20, "20 from a database");
    check(ita_cup_b_count(18, 2003, 2004), 0, "a season before the start");
    check(ita_cup_b_count(19, 2019, 2019), 0, "19 from a 2019 database keeps the original");

    /* Row 31 and open item 253. The C1 play-out as places (1st = 1): lower v higher of each tie, then the direct
     * relegation places. The class's shape (17th v 14th, 16th v 15th, 18th down) moves with the count:
     * 2004/05 C1/A of 19, 2012/13 groups of 17 and 16, Serie C groups of 20 from 2014/15. */
    const int shape[6][4] = {{18, 2004, 18, 1}, {19, 2004, 19, 1}, {17, 2012, 17, 1}, {16, 2012, 16, 1},
                             {20, 2014, 20, 1}, {18, 2015, 18, 1}};
    for (int k = 0; k < 6; k++) {
        ItaPlayout p;
        int n = shape[k][0];
        ita_c1_playout(shape[k][1], n, &p);
        check(p.ties, 2, "two ties");
        check(p.slot[0] + 1, n - 1, "lower of tie 1");
        check(p.slot[1] + 1, n - 4, "higher of tie 1");
        check(p.slot[2] + 1, n - 2, "lower of tie 2");
        check(p.slot[3] + 1, n - 3, "higher of tie 2");
        check(p.direct, shape[k][3], "one down directly");
    }
    ItaPlayout p;
    /* 2013/14 Prima Divisione: no relegation and no play-out (the league merged into the Serie C). */
    ita_c1_playout(2013, 16, &p);
    check(p.ties, 0, "2013/14 no play-out");
    check(p.direct, 0, "2013/14 none down");
    /* 2017/18 Serie C, groups of 19: 16th v 19th, 17th v 18th, none down directly. */
    ita_c1_playout(2017, 19, &p);
    check(p.ties, 2, "2017/18 two ties");
    check(p.slot[0] + 1, 19, "2017/18 19th");
    check(p.slot[1] + 1, 16, "2017/18 16th");
    check(p.slot[2] + 1, 18, "2017/18 18th");
    check(p.slot[3] + 1, 17, "2017/18 17th");
    check(p.direct, 0, "2017/18 none directly");
    /* 2018/19 Serie C: one tie 18th v 19th; a group of 20 sends its 20th down, a group of 19 none. */
    ita_c1_playout(2018, 20, &p);
    check(p.ties, 1, "2018/19 one tie");
    check(p.slot[0] + 1, 19, "2018/19 19th of 20");
    check(p.slot[1] + 1, 18, "2018/19 18th of 20");
    check(p.direct, 1, "2018/19 20th down");
    ita_c1_playout(2018, 19, &p);
    check(p.ties, 1, "2018/19 one tie of 19");
    check(p.slot[0] + 1, 19, "2018/19 19th of 19");
    check(p.direct, 0, "2018/19 none of 19");
    /* 2019/20 and on: the class's shape at 20. */
    ita_c1_playout(2019, 20, &p);
    check(p.ties * 10 + p.direct, 21, "2019/20 the class's shape");
    ita_c1_playout(2030, 20, &p);
    check(p.ties * 10 + p.direct, 21, "a later season keeps the last shape");
    ita_c1_playout(2004, 5, &p);
    check(p.ties * 10 + p.direct, 1, "a group under 6 has no play-out");

    /* The points gap: no tie when the two clubs are more than 8 points apart; the lower club goes down. In
     * 2016/17, 2017/18, 2018/19 and from 2020/21, not in 2015/16 or 2019/20. */
    const int gap_years[] = {2015, 2016, 2017, 2018, 2019, 2020, 2025, 2030}, gap_on[] = {0, 1, 1, 1, 0, 1, 1, 1};
    for (int k = 0; k < 8; k++) check(ita_c1_gap_rule(gap_years[k]), gap_on[k], "gap rule season");
    int pts[20];
    for (int i = 0; i < 20; i++) pts[i] = 60 - 2 * i;      /* 16th 30, 17th 28, 18th 26, 19th 24 */
    int gone[2], safe[2];
    ita_c1_playout(2020, 20, &p);
    check(ita_c1_playout_gap(&p, pts, gone, safe), 0, "6 and 2 points: both ties play");
    check(p.ties, 2, "both ties stay");
    pts[18] = 21;                                           /* 16th 30 v 19th 21: 9 points */
    check(ita_c1_playout_gap(&p, pts, gone, safe), 1, "9 points: tie 1 not played");
    check(gone[0] + 1, 19, "19th down");
    check(safe[0] + 1, 16, "16th safe");
    check(p.ties, 1, "one tie left");
    check(p.slot[0] + 1, 18, "18th plays");
    check(p.slot[1] + 1, 17, "17th plays");
    pts[18] = 22;                                           /* 8 points: the tie plays */
    ita_c1_playout(2020, 20, &p);
    check(ita_c1_playout_gap(&p, pts, gone, safe), 0, "8 points: the tie plays");
    pts[17] = 10;                                           /* 17th 28 v 18th 10, 16th 30 v 19th 22 */
    ita_c1_playout(2020, 20, &p);
    check(ita_c1_playout_gap(&p, pts, gone, safe), 1, "only tie 2 dropped");
    check(gone[0] + 1, 18, "18th down");
    check(safe[0] + 1, 17, "17th safe");
    check(p.slot[0] + 1, 19, "19th plays");
    pts[18] = 5;
    ita_c1_playout(2020, 20, &p);
    check(ita_c1_playout_gap(&p, pts, gone, safe), 2, "both dropped");
    check(p.ties, 0, "no tie left");

    if (!fails) printf("PASS season_ita_rules\n");
    return fails ? 1 : 0;
}
