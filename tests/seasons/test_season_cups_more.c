/* Test of the host part of slice 21c (0.6.0, src/recomp_rt/win/season_cups_more.c): the four-club super cups,
 * with the rules table of the build.
 *
 * Usage: season_cups_more_test <rules.tsv made by tools/seasons/make_rules.py>
 * ctest makes the table first (test season_rules_table). The values come from the rules table parts
 * (docs/research/rules-0.6.0/ita.tsv, spa.tsv, rus-tur.tsv): the Supercoppa Italiana has four clubs from
 * 2023/24 to 2025/26 and two again from 2026/27, the Supercopa de Espana four clubs from 2019/20, with penalties
 * after 90 minutes from 2024/25, the Turkish Super Cup four clubs from 2025/26; the new cups (rus-tur.tsv,
 * nor-fin.tsv, eng.tsv, wal-nir-irl.tsv) and the knockout layout of a new cup. */
#include "season_cups.h"
#include "season_rules.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            failures++; \
            printf("FAIL line %d: ", __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
        } \
    } while (0)

static void four_clubs(void)
{
    const SeasonSuperFour *f = season_super_four("ita_super", 2022);
    CHECK(!f, "Supercoppa 2022/23 has two clubs");
    f = season_super_four("ita_super", 2023);
    CHECK(f && f->flags == CUP_PENALTIES, "Supercoppa 2023/24: four clubs, penalties after 90 minutes");
    CHECK(f && f->sf_day == 18 && f->sf_month == 0 && f->sf_year == 1 && f->f_day == 22, "Supercoppa 2023/24 dates");
    f = season_super_four("ita_super", 2025);
    CHECK(f && f->sf_month == 11 && f->sf_year == 0, "Supercoppa 2025/26 in December 2025");
    CHECK(!season_super_four("ita_super", 2026), "Supercoppa 2026/27 has two clubs again");
    CHECK(!season_super_four("spa_super", 2018), "Supercopa 2018/19 has two clubs");
    f = season_super_four("spa_super", 2019);
    CHECK(f && f->flags == (CUP_PENALTIES | CUP_EXTRA_TIME), "Supercopa 2019/20: extra time and penalties");
    f = season_super_four("spa_super", 2024);
    CHECK(f && f->flags == CUP_PENALTIES, "Supercopa 2024/25: penalties after 90 minutes");
    f = season_super_four("spa_super", 2026);
    CHECK(f && f->sf_month == 1, "Supercopa 2026/27 in February 2027");
    /* After the last row of the table the rule stays (Q248). */
    f = season_super_four("spa_super", 2035);
    CHECK(f && f->flags == CUP_PENALTIES, "Supercopa after the table");
    CHECK(!season_super_four("new_tur_super", 2024), "Turkish Super Cup 2024/25 has two clubs");
    CHECK(season_super_four("new_tur_super", 2025) != NULL, "Turkish Super Cup 2025/26 has four clubs");
    CHECK(!season_super_four("fra_super", 2023), "no four-club row for another cup");
}

static void picks(void)
{
    /* league: the league clubs by place (1 first), then cup winner and runner-up. out: the semi-final pairs, league
     * winner v cup runner-up, cup winner v league runner-up. A place a club already holds goes to the next league
     * club (O342). */
    uint32_t out[4];
    const uint32_t lg[6] = {1, 2, 3, 4, 5, 6};
    CHECK(season_super_four_pick(lg, 6, 7, 8, out) && out[0] == 1 && out[1] == 8 && out[2] == 7 && out[3] == 2,
          "four clubs: league winner v cup runner-up, cup winner v league runner-up");
    CHECK(season_super_four_pick(lg, 6, 1, 8, out) && out[1] == 8 && out[2] == 3, "cup winner first: the third");
    CHECK(season_super_four_pick(lg, 6, 2, 8, out) && out[1] == 8 && out[2] == 3, "cup winner second: the third");
    CHECK(season_super_four_pick(lg, 6, 3, 1, out) && out[1] == 4 && out[2] == 3,
          "cup winner third, the champion lost the final: the fourth (gate (a) run 5)");
    CHECK(season_super_four_pick(lg, 6, 1, 2, out) && out[0] == 1 && out[1] == 4 && out[2] == 3 && out[3] == 2,
          "both finalists in the top two: the third and the fourth");
    CHECK(season_super_four_pick(lg, 6, 2, 1, out) && out[1] == 4 && out[2] == 3, "the same, the other way round");
    CHECK(season_super_four_pick(lg, 6, 3, 4, out) && out[1] == 4 && out[2] == 3, "finalists third and fourth");
    CHECK(season_super_four_pick(lg, 6, 7, 0, out) && out[1] == 3 && out[2] == 7, "no cup runner-up: the third");
    CHECK(season_super_four_pick(lg, 6, 7, 7, out) && out[1] == 3 && out[2] == 7, "one cup club twice");
    const uint32_t two[2] = {1, 2};
    CHECK(!season_super_four_pick(two, 2, 1, 2, out), "two league clubs only: the two-club format");
    const uint32_t g[3] = {0, 2, 3};
    CHECK(!season_super_four_pick(g, 3, 4, 5, out), "no league winner");
    for (int a = 1; a <= 8; a++)
        for (int b = 0; b <= 8; b++) {
            if (!season_super_four_pick(lg, 6, (uint32_t)a, (uint32_t)b, out)) continue;
            int distinct = out[0] != out[1] && out[0] != out[2] && out[0] != out[3] && out[1] != out[2] &&
                           out[1] != out[3] && out[2] != out[3];
            CHECK(distinct, "every pick holds four different clubs");
        }
}

static void new_cups(void)
{
    /* The Turkish Super Cup: first held in 2006/07, cancelled in 2011/12 (rus-tur.tsv). */
    CHECK(season_new_cup_held("new_tur_super", 2005) == 0, "Turkish Super Cup 2005/06");
    CHECK(season_new_cup_held("new_tur_super", 2006) == 1, "Turkish Super Cup 2006/07");
    CHECK(season_new_cup_held("new_tur_super", 2011) == 0, "Turkish Super Cup 2011/12");
    CHECK(season_new_cup_held("new_tur_super", 2040) == 1, "Turkish Super Cup after the table (Q248)");
    /* The Russian Super Cup: the edition of March 2003 is in row 2002/03, the edition of July 2012 in row 2012/13. */
    CHECK(season_new_cup_held("new_rus_super", 2002) == 0, "Russian Super Cup 2002");
    CHECK(season_new_cup_held("new_rus_super", 2003) == 1, "Russian Super Cup 2003");
    CHECK(season_new_cup_held("new_rus_super", 2011) == 1, "Russian Super Cup 2011");
    CHECK(season_new_cup_held("new_rus_super", 2012) == 1, "Russian Super Cup 2012");
    const SeasonNewCup *f = season_new_cup("new_rus_super", 2011);
    CHECK(f && f->round[0].month == 2, "Russian Super Cup 2011 in March");
    f = season_new_cup("new_rus_super", 2012);
    CHECK(f && f->round[0].month == 6, "Russian Super Cup 2012 in July");
    /* The Norwegian Super Cup: editions 2009, 2010, 2017 and 2018 (rows 2008/09, 2009/10, 2016/17, 2017/18). */
    CHECK(season_new_cup_held("new_nor_super", 2008) == 0, "Norwegian Super Cup 2008");
    CHECK(season_new_cup_held("new_nor_super", 2009) == 1, "Norwegian Super Cup 2009");
    CHECK(season_new_cup_held("new_nor_super", 2011) == 0, "Norwegian Super Cup 2011");
    CHECK(season_new_cup_held("new_nor_super", 2017) == 1, "Norwegian Super Cup 2017");
    CHECK(season_new_cup_held("new_nor_super", 2018) == 1, "Norwegian Super Cup 2018");
    CHECK(season_new_cup_held("new_nor_super", 2019) == 0, "Norwegian Super Cup 2019");
    CHECK(season_new_cup_next("new_nor_super", 2011) == 2017, "Norwegian Super Cup next from 2011");
    /* The edition that a season's row holds (a start with changing rules off makes that edition). */
    CHECK(season_new_cup_row_edition("new_rus_lge_cup", 2002) == 2003, "Russian Premier League Cup in row 2002/03");
    CHECK(season_new_cup_row_edition("new_rus_super", 2010) == 2011, "Russian Super Cup of March 2011 in row 2010/11");
    CHECK(season_new_cup_row_edition("new_rus_super", 2012) == 2012, "Russian Super Cup of July 2012 in row 2012/13");
    CHECK(season_new_cup_row_edition("new_nor_super", 2008) == 2009, "Norwegian Super Cup of 2009 in row 2008/09");
    CHECK(season_new_cup_row_edition("new_conf_cup", 2007) == 2007, "Conference League Cup in row 2007/08");
    /* The format played (open item 131). Changing rules on: the edition's own; the National League Cup of 2024/25
     * has no format the game can play. Off: the rules of the start season stay, so a 2008 game plays the cup of
     * 2008 in every season (the row of 2024 gave rounds of day 0 and the box "Date..cpp 52"). */
    f = season_new_cup_played("new_conf_cup", 2024, 1, 2008);
    CHECK(f && f->clubs < 0, "Conference League Cup 2024/25, changing rules on: not played");
    f = season_new_cup_played("new_conf_cup", 2024, 0, 2008);
    CHECK(f && f->clubs == 0 && f->round[0].day == 22, "Conference League Cup 2024/25 of a 2008 game, rules off");
    f = season_new_cup_played("new_conf_cup", 2040, 0, 2004);
    CHECK(f && f->clubs == 0, "Conference League Cup 2040/41 of a 2004 game, rules off");
    f = season_new_cup_played("new_rus_super", 2012, 0, 2010);
    CHECK(f && f->round[0].month == 2, "Russian Super Cup 2012 of a 2010 game, rules off: in March as in 2011");
    f = season_new_cup_played("new_rus_super", 2012, 1, 2010);
    CHECK(f && f->round[0].month == 6, "Russian Super Cup 2012 of a 2010 game, rules on: in July");
    f = season_new_cup_played("new_all_ireland", 2014, 0, 2010);
    CHECK(f && f->clubs == 12, "All-Ireland cup 2014/15 of a 2010 game, rules off: 12 clubs as in 2010");
    /* A start whose own row is not played makes no cup (new_held), so the format of a later edition is not asked. */
    CHECK(season_new_cup_held("new_conf_cup", 2024) == 0, "Conference League Cup 2024/25: a 2024 start makes none");
    CHECK(season_new_cup_next("new_nor_super", 2019) == SEASON_CUPS_NEVER, "Norwegian Super Cup after 2018");
    /* The Russian Premier League Cup: one edition, 2003 (row 2002/03). */
    CHECK(season_new_cup_held("new_rus_lge_cup", 2003) == 1, "Russian Premier League Cup 2003");
    CHECK(season_new_cup_held("new_rus_lge_cup", 2004) == 0, "Russian Premier League Cup 2004");
    f = season_new_cup("new_rus_lge_cup", 2003);
    CHECK(f && f->round[0].legs == 2 && f->round[1].legs == 2, "Russian Premier League Cup: two legs");
    /* The Conference League Cup: 2004/05, 2007/08 and 2008/09; the National League Cup from 2024/25 has
     * under-21 sides that the game does not hold (Q336), so the game does not play it. */
    CHECK(season_new_cup_held("new_conf_cup", 2004) == 1, "Conference League Cup 2004/05");
    CHECK(season_new_cup_held("new_conf_cup", 2005) == 0, "Conference League Cup 2005/06");
    CHECK(season_new_cup_held("new_conf_cup", 2008) == 1, "Conference League Cup 2008/09");
    CHECK(season_new_cup_held("new_conf_cup", 2024) == 0, "National League Cup 2024/25");
    /* The All-Ireland cup: the knockout seasons 2010/11 to 2013/14 (12 clubs, then 8). */
    CHECK(season_new_cup_held("new_all_ireland", 2010) == 1, "Setanta Cup 2010/11");
    CHECK(season_new_cup_held("new_all_ireland", 2014) == 0, "Setanta Cup 2014/15");
    f = season_new_cup("new_all_ireland", 2013);
    CHECK(f && f->clubs == 8 && f->extra == NEW_CUP_PAIRED, "Setanta Cup 2013/14: 8 clubs, paired first round");
    CHECK(season_new_cup_held("unknown_cup", 2010) == -1, "no row");
}

static void layouts(void)
{
    int ties0 = 0, rounds = 0;
    CHECK(season_new_cup_layout(16, &ties0, &rounds) && ties0 == 8 && rounds == 4, "16 clubs: 4 rounds");
    CHECK(season_new_cup_layout(12, &ties0, &rounds) && ties0 == 4 && rounds == 4, "12 clubs: 4 ties, 4 byes");
    CHECK(season_new_cup_layout(22, &ties0, &rounds) && ties0 == 6 && rounds == 5, "22 clubs: 6 ties, 10 byes");
    CHECK(season_new_cup_layout(2, &ties0, &rounds) && ties0 == 1 && rounds == 1, "2 clubs: the final");
    CHECK(!season_new_cup_layout(1, &ties0, &rounds), "1 club");
    CHECK(season_new_cup_round_code(5, 0) == 10 && season_new_cup_round_code(5, 1) == 20 &&
          season_new_cup_round_code(5, 2) == 120 && season_new_cup_round_code(5, 4) == 150, "round codes");
}

static void groups(void)
{
    const SeasonCupGroups *g = season_cup_groups("sco_lge_cup", 2015);
    CHECK(!g, "Scottish League Cup 2015/16 has no groups");
    g = season_cup_groups("sco_lge_cup", 2016);
    CHECK(g && g->groups == 8 && g->ko_clubs == 16 && g->byes == 1 && g->base_rounds == 0,
          "Scottish League Cup 2016/17");
    CHECK(season_cup_groups("sco_lge_cup", 2030) == g, "Scottish League Cup after the table");
    g = season_cup_groups("tur_cup", 2005);
    CHECK(g && g->base_rounds == 3 && g->groups == 4 && g->ko_clubs == 8, "Turkish Cup 2005/06");
    CHECK(!season_cup_groups("tur_cup", 2011), "Turkish Cup 2011/12 has no groups");
    g = season_cup_groups("tur_cup", 2012);
    CHECK(g && g->base_rounds == 4 && g->groups == 2 && g->meetings == 2 && g->ko_clubs == 4, "Turkish Cup 2012/13");
    g = season_cup_groups("tur_cup", 2014);
    CHECK(g && g->base_rounds == 2 && g->groups == 8 && g->ko_clubs == 16, "Turkish Cup 2014/15");
    CHECK(!season_cup_groups("tur_cup", 2017), "Turkish Cup 2017/18 has no groups");
    CHECK(season_cup_groups("tur_cup", 2024) != NULL, "Turkish Cup 2024/25");
    CHECK(!season_cup_groups("tur_cup", 2026), "Turkish Cup 2026/27 has no groups");
    g = season_cup_groups("eng_auto_cup", 2016);
    CHECK(g && g->groups == 16 && g->per_group == 2 && g->ko_clubs == 32, "EFL Trophy 2016/17");
    /* Every format: the knockout is a power of 2 that its rounds hold, the group days are in order. */
    const char *files[] = {"sco_lge_cup", "eng_auto_cup", "tur_cup", "test_eng_cc_cup", "new_all_ireland"};
    for (int f = 0; f < 5; f++)
        for (int y = 2001; y <= 2027; y++) {
            g = season_cup_groups(files[f], y);
            if (!g) continue;
            int ties0 = 0, rounds = 0;
            CHECK(season_new_cup_layout(g->ko_clubs, &ties0, &rounds) && 2 * ties0 == g->ko_clubs && rounds <= 5 &&
                  g->ko[rounds - 1].day > 0, "%s %d: knockout of %d clubs", files[f], y, g->ko_clubs);
            for (int r = 0; r + 1 < rounds; r++) {
                const SeasonNewRound *a = &g->ko[rounds - 1 - r], *b = &g->ko[rounds - 2 - r];
                CHECK(a->year * 400 + a->month * 31 + a->day < b->year * 400 + b->month * 31 + b->day,
                      "%s %d: knockout round %d after round %d", files[f], y, r, r + 1);
            }
            for (int d = 0; d + 1 < 10 && g->days[d + 1][0]; d++)
                CHECK(g->days[d][2] * 400 + g->days[d][1] * 31 + g->days[d][0] <
                      g->days[d + 1][2] * 400 + g->days[d + 1][1] * 31 + g->days[d + 1][0],
                      "%s %d: group day %d", files[f], y, d);
        }
    int sizes[16];
    CHECK(season_cup_group_sizes(36, 8, sizes) && sizes[0] == 5 && sizes[3] == 5 && sizes[4] == 4 && sizes[7] == 4,
          "36 clubs in 8 groups: four of 5, four of 4");
    CHECK(season_cup_group_sizes(48, 16, sizes) && sizes[15] == 3, "48 clubs in 16 groups of 3");
    CHECK(!season_cup_group_sizes(15, 8, sizes), "fewer than 2 clubs a group");
    CHECK(season_cup_group_days(5, 1) == 5 && season_cup_group_days(4, 1) == 3 && season_cup_group_days(4, 2) == 6 &&
          season_cup_group_days(3, 1) == 3, "group days");
}

/* Item 145: the All-Ireland cup (the Setanta Cup) in every edition from its first, of 2005 (row 2004/05), to the
 * knockout of 2010/11 (docs/research/2026-11-05-setanta-cup.md). For each edition: held or not, the clubs (count and
 * the rule of each league), the groups (count, sizes, meetings, who goes through, the mix of the leagues), the
 * knockout and the dates of the first group match day, the draw and the final. */
static void setanta(void)
{
    static const struct {
        int edition, held, clubs, picks0, picks1, extra, groups, size0, per_group, ko, nation_pots;
        int day0, month0, year0, draw_day, draw_month, final_day, final_month;
    } want[] = {
        {2004, 1, 6, 2, 2, NEW_CUP_CUP_WINNERS, 2, 3, 1, 2, 0, 15, 2, 1, 13, 11, 21, 4},
        {2005, 1, 8, 4, 4, 0, 2, 4, 2, 4, 1, 20, 1, 1, 12, 11, 22, 3},
        {2006, 1, 8, 4, 4, 0, 2, 4, 2, 4, 1, 26, 1, 1, 7, 11, 12, 4},
        {2007, 1, 8, 4, 4, 0, 2, 4, 2, 4, 1, 26, 1, 1, 17, 0, 10, 4},
        {2008, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2009, 1, 9, 4, 4, NEW_CUP_HOLDERS, 3, 3, 1, 4, 0, 29, 7, 0, 28, 6, 15, 4},
        {2010, 1, 12, 0, 0, NEW_CUP_PAIRED, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 21, 4},
    };
    for (size_t i = 0; i < sizeof want / sizeof want[0]; i++) {
        int e = want[i].edition;
        CHECK(season_new_cup_held("new_all_ireland", e) == want[i].held, "Setanta Cup %d/%02d held %d", e,
              (e + 1) % 100, want[i].held);
        const SeasonCupGroups *g = season_cup_groups("new_all_ireland", e);
        if (!want[i].held) {
            CHECK(!g, "Setanta Cup %d/%02d: no groups", e, (e + 1) % 100);
            continue;
        }
        const SeasonNewCup *f = season_new_cup("new_all_ireland", e);
        CHECK(f && f->clubs == want[i].clubs && f->picks[0] == want[i].picks0 && f->picks[1] == want[i].picks1 &&
                  f->extra == want[i].extra,
              "Setanta Cup %d/%02d: %d clubs by the rule of the edition", e, (e + 1) % 100, want[i].clubs);
        CHECK(f && f->round[0].day == want[i].final_day && f->round[0].month == want[i].final_month &&
                  f->round[0].year == 1 && f->round[0].legs == 1,
              "Setanta Cup %d/%02d: the final", e, (e + 1) % 100);
        if (!want[i].groups) {
            CHECK(!g, "Setanta Cup %d/%02d: a knockout, no groups", e, (e + 1) % 100);
            continue;
        }
        int sizes[16] = {0};
        CHECK(g && g->groups == want[i].groups && g->base_rounds == 0 && g->meetings == 2 &&
                  g->per_group == want[i].per_group && g->byes == 0 && g->ko_clubs == want[i].ko &&
                  g->nation_pots == want[i].nation_pots,
              "Setanta Cup %d/%02d: %d groups, home and away, %d through, a knockout of %d", e, (e + 1) % 100,
              want[i].groups, want[i].per_group, want[i].ko);
        if (!g) continue;
        CHECK(season_cup_group_sizes(want[i].clubs, g->groups, sizes) && sizes[0] == want[i].size0 &&
                  sizes[g->groups - 1] == want[i].size0,
              "Setanta Cup %d/%02d: groups of %d", e, (e + 1) % 100, want[i].size0);
        int days = season_cup_group_days(want[i].size0, g->meetings);
        CHECK(days == 6 && g->days[days - 1][0] > 0 && (days == 10 || g->days[days][0] == 0),
              "Setanta Cup %d/%02d: 6 group match days", e, (e + 1) % 100);
        CHECK(g->days[0][0] == want[i].day0 && g->days[0][1] == want[i].month0 && g->days[0][2] == want[i].year0,
              "Setanta Cup %d/%02d: the first group match day", e, (e + 1) % 100);
        CHECK(g->draw[0] == want[i].draw_day && g->draw[1] == want[i].draw_month,
              "Setanta Cup %d/%02d: the draw", e, (e + 1) % 100);
        /* The draw comes before the first group match day, the last group match day before the knockout. */
        CHECK(g->draw[2] * 400 + g->draw[1] * 31 + g->draw[0] < g->days[0][2] * 400 + g->days[0][1] * 31 + g->days[0][0],
              "Setanta Cup %d/%02d: draw before the groups", e, (e + 1) % 100);
        int ties0 = 0, rounds = 0;
        CHECK(season_new_cup_layout(g->ko_clubs, &ties0, &rounds) &&
                  g->ko[rounds - 1].year * 400 + g->ko[rounds - 1].month * 31 + g->ko[rounds - 1].day >
                      g->days[days - 1][2] * 400 + g->days[days - 1][1] * 31 + g->days[days - 1][0],
              "Setanta Cup %d/%02d: the knockout after the groups", e, (e + 1) % 100);
        CHECK(g->ko[0].day == want[i].final_day && g->ko[0].month == want[i].final_month,
              "Setanta Cup %d/%02d: the final of the knockout stage", e, (e + 1) % 100);
    }
    /* The late start on 20 June (season_new_cups_day): only that day, only when the entry has another day, only for
     * an edition behind the year that the game plays. */
    CHECK(season_new_cup_late(170, 50, 2004, 2005, 1), "late start: 20 June 2005, edition 2004, entry on 20 February");
    CHECK(!season_new_cup_late(171, 50, 2004, 2005, 1), "late start: not on 21 June");
    CHECK(!season_new_cup_late(170, 170, 2004, 2005, 1), "late start: not when the entry's own day is 20 June");
    CHECK(!season_new_cup_late(170, 50, 2005, 2005, 1), "late start: not when the edition is the year (started)");
    CHECK(!season_new_cup_late(170, 50, 2007, 2008, 0), "late start: not in a season without the cup (2008)");
    /* Each final of the rows is before 20 June of the year after its edition, and the first date of the next
     * edition (its draw, its first group day or its first round) is after that 20 June: the late start never meets a
     * running edition or past dates. Dates as month (0 to 11) x 31 + day, years after the edition year. */
    const int late = 5 * 31 + 20;
    for (int e = 2004; e <= 2013; e++) {
        const SeasonNewCup *f = season_new_cup("new_all_ireland", e);
        const SeasonCupGroups *gr = season_cup_groups("new_all_ireland", e);
        if (season_new_cup_held("new_all_ireland", e) == 1 && f) {
            const SeasonNewRound *fin = gr ? &gr->ko[0] : &f->round[0];
            CHECK(fin->year == 1 && fin->month * 31 + fin->day < late, "Setanta Cup %d/%02d: the final before 20 June",
                  e, (e + 1) % 100);
        }
        int n = e + 1;
        const SeasonNewCup *fn = season_new_cup("new_all_ireland", n);
        const SeasonCupGroups *gn = season_cup_groups("new_all_ireland", n);
        if (season_new_cup_held("new_all_ireland", n) != 1 || !fn) continue;
        int y, m, d;
        if (gn && gn->draw[0] > 0) y = gn->draw[2], m = gn->draw[1], d = gn->draw[0];
        else if (gn) y = gn->days[0][2], m = gn->days[0][1], d = gn->days[0][0];
        else {
            int r = 4;
            while (r > 0 && fn->round[r].day == 0) r--;
            y = fn->round[r].year, m = fn->round[r].month, d = fn->round[r].day;
        }
        CHECK(y >= 1 || m * 31 + d > late, "Setanta Cup %d/%02d: the first date after 20 June %d", n, (n + 1) % 100, n);
    }
    /* 2009/10: two-legged semi-finals, 14 days apart. */
    const SeasonCupGroups *g = season_cup_groups("new_all_ireland", 2009);
    CHECK(g && g->ko[1].legs == 2 && g->ko[1].gap == 14 && g->ko[1].day == 3 && g->ko[1].month == 3,
          "Setanta Cup 2009/10: two-legged semi-finals from 3 April 2010");
    /* With changing rules off, a 2006 game plays the 2006/07 format every season, also 2010/11 (open item 131). */
    const SeasonNewCup *f = season_new_cup_played("new_all_ireland", 2010, 0, 2006);
    CHECK(f && f->clubs == 8, "Setanta Cup 2010/11 of a 2006 game, rules off: 8 clubs as in 2006/07");
    CHECK(season_new_cup_next("new_all_ireland", 2008) == 2009, "Setanta Cup: next edition after 2008/09");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: season_cups_more_test <rules.tsv>\n");
        return 2;
    }
    if (season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    four_clubs();
    picks();
    new_cups();
    layouts();
    groups();
    setanta();
    if (failures) printf("%d failures\n", failures);
    else printf("season_cups_more: all checks passed\n");
    return failures ? 1 : 0;
}
