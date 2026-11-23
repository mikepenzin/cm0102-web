/* Test of the nation rules lookup (0.6.0 slice 17, src/recomp_rt/win/season_nation.c and the table
 * season_nation_table.c that tools/seasons/encode_rules.py makes).
 *
 * The checks: the fold of the rows of a nation up to a season (a row of "keep" leaves the earlier rule, the
 * rules stay after the last row), the English windows of 2002/03 (two windows from the first season with FIFA
 * windows) and of 2023/24 (14 June to 1 September, 1 January to 1 February), the Brexit flag from 2020/21, the
 * German non-European limits of 2004 to 2006, the Turkish limits of 2001 and 2013, a nation with no windows
 * (Sweden to 2003), the nation lookup, the global of each nation, and that the table is in order. */
#include "season_nation.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void window(const SeasonNationState *s, int i, int sd, int sm, int ed, int em, int line)
{
    CHECK(s->win[i].sd == sd && s->win[i].sm == sm && s->win[i].ed == ed && s->win[i].em == em,
          "line %d: window %d is %d.%d-%d.%d, want %d.%d-%d.%d", line, i, s->win[i].sd, s->win[i].sm, s->win[i].ed,
          s->win[i].em, sd, sm, ed, em);
}

int main(void)
{
    SeasonNationState s;

    /* England: the 2001/02 row has no windows to give (the game keeps its one window). */
    CHECK(season_nation_state("England", 2001, &s) == 1, "England 2001 has a row");
    CHECK(s.windows == SEASON_NATION_KEEP, "England 2001 windows %d", s.windows);
    CHECK(s.squad_non_eu == SEASON_NATION_NONE && s.squad_foreign == SEASON_NATION_NONE, "England 2001: no quota");
    CHECK(!s.brexit, "England 2001: no Brexit");
    season_nation_state("England", 2002, &s);
    CHECK(s.windows == 2, "England 2002 windows %d", s.windows);
    window(&s, 0, 1, 6, 31, 8, __LINE__);
    window(&s, 1, 1, 1, 31, 1, __LINE__);
    season_nation_state("England", 2005, &s);
    CHECK(s.windows == 2, "England 2005 keeps the windows of 2002 (%d)", s.windows);
    season_nation_state("England", 2023, &s);
    CHECK(s.windows == 2, "England 2023 windows %d", s.windows);
    window(&s, 0, 14, 6, 1, 9, __LINE__);
    window(&s, 1, 1, 1, 1, 2, __LINE__);
    season_nation_state("England", 2025, &s);
    CHECK(s.windows == 3, "England 2025 has the Club World Cup window (%d)", s.windows);
    window(&s, 0, 1, 6, 10, 6, __LINE__);
    window(&s, 1, 16, 6, 1, 9, __LINE__);
    season_nation_state("England", 2019, &s);
    CHECK(!s.brexit, "England 2019: no Brexit");
    season_nation_state("England", 2020, &s);
    CHECK(s.brexit == 1, "England 2020: Brexit");
    season_nation_state("England", 2099, &s);
    CHECK(s.brexit == 1 && s.windows == 2, "England after the last row keeps the rules");

    /* Germany: 5 non-European players to 2003, 5 under contract from 2004, 4 in 2005, no limit from 2006. */
    season_nation_state("Germany", 2003, &s);
    CHECK(s.squad_non_eu == 5 && s.sign_non_eu == SEASON_NATION_KEEP, "Germany 2003: %d %d", s.squad_non_eu, s.sign_non_eu);
    season_nation_state("Germany", 2004, &s);
    CHECK(s.squad_non_eu == 5 && s.sign_non_eu == 5, "Germany 2004: %d %d", s.squad_non_eu, s.sign_non_eu);
    season_nation_state("Germany", 2005, &s);
    CHECK(s.squad_non_eu == 4 && s.sign_non_eu == 4, "Germany 2005: %d %d", s.squad_non_eu, s.sign_non_eu);
    season_nation_state("Germany", 2006, &s);
    CHECK(s.squad_non_eu == SEASON_NATION_NONE && s.sign_non_eu == SEASON_NATION_NONE && s.sign_foreign == SEASON_NATION_NONE,
          "Germany 2006: no limit");

    /* Turkey: 5+1+2 in 2001 (6 in the match squad, 8 registered), 6+0+4 in 2013 (10 signed). */
    season_nation_state("Turkey", 2001, &s);
    CHECK(s.squad_foreign == 6 && s.sign_foreign == 8, "Turkey 2001: %d %d", s.squad_foreign, s.sign_foreign);
    season_nation_state("Turkey", 2013, &s);
    CHECK(s.squad_foreign == 6 && s.sign_foreign == 10, "Turkey 2013: %d %d", s.squad_foreign, s.sign_foreign);
    /* Gap row 6: 2027/28 is not announced, so the last known rule (14 foreigners signed, 2026/27) stays. */
    season_nation_state("Turkey", 2026, &s);
    CHECK(s.sign_foreign == 14, "Turkey 2026: %d", s.sign_foreign);
    season_nation_state("Turkey", 2027, &s);
    CHECK(s.sign_foreign == 14, "Turkey 2027: %d", s.sign_foreign);
    season_nation_state("Brazil", 2023, &s);
    CHECK(s.squad_foreign == 9, "Brazil 2023: %d", s.squad_foreign);

    /* Croatia (issue 135): all foreigners count to 2012/13 (4 fielded in 2007/08); from 2013/14 only players
     * not of the EU or EEA (HNS Glasnik 28/2013): 6, 8 from 2017/18, 6 from 2019/20, 5 from 2026/27. */
    season_nation_state("Croatia", 2012, &s);
    CHECK(s.squad_foreign == 4 && s.squad_non_eu == SEASON_NATION_KEEP, "Croatia 2012: %d %d", s.squad_foreign, s.squad_non_eu);
    season_nation_state("Croatia", 2013, &s);
    CHECK(s.squad_non_eu == 6 && s.squad_foreign == SEASON_NATION_NONE, "Croatia 2013: %d %d", s.squad_non_eu, s.squad_foreign);
    season_nation_state("Croatia", 2017, &s);
    CHECK(s.squad_non_eu == 8 && s.squad_foreign == SEASON_NATION_NONE, "Croatia 2017: %d %d", s.squad_non_eu, s.squad_foreign);
    season_nation_state("Croatia", 2020, &s);
    CHECK(s.squad_non_eu == 6 && s.squad_foreign == SEASON_NATION_NONE, "Croatia 2020: %d %d", s.squad_non_eu, s.squad_foreign);
    season_nation_state("Croatia", 2026, &s);
    CHECK(s.squad_non_eu == 5 && s.squad_foreign == SEASON_NATION_NONE, "Croatia 2026: %d %d", s.squad_non_eu, s.squad_foreign);

    /* The match squad record under a state (bytes +0..+2 foreign, +3..+5 non-EU, 0xff none). Croatia's filler
     * 0x518620 writes 4 foreign at +0; the lookup 0x8cf6b0 with the collapse flag moves it to +2 (0x8cf794). */
    {
        uint8_t rec[6] = {4, 0xff, 0xff, 0xff, 0xff, 0xff};
        season_nation_state("Croatia", 2020, &s);
        CHECK(season_nation_squad_record(rec, &s, 1) == 1 && rec[0] == 0xff && rec[3] == 6 && rec[1] == 0xff &&
              rec[2] == 0xff && rec[4] == 0xff && rec[5] == 0xff,
              "Croatia 2020 top: %02x %02x %02x %02x %02x %02x", rec[0], rec[1], rec[2], rec[3], rec[4], rec[5]);
        uint8_t col[6] = {0xff, 0xff, 4, 0xff, 0xff, 0xff};
        season_nation_squad_record(col, &s, 1);
        CHECK(col[2] == 0xff && col[5] == 6, "Croatia 2020 collapsed: %02x %02x", col[2], col[5]);
        uint8_t low[6] = {4, 0xff, 0xff, 0xff, 0xff, 0xff};
        season_nation_squad_record(low, &s, 0);
        CHECK(low[0] == 0xff && low[3] == 4, "Croatia 2020 lower division keeps the game's number as non-EU: %02x %02x",
              low[0], low[3]);
        uint8_t old[6] = {4, 0xff, 0xff, 0xff, 0xff, 0xff};
        season_nation_state("Croatia", 2010, &s);
        CHECK(season_nation_squad_record(old, &s, 1) == 0 && old[0] == 4 && old[3] == 0xff, "Croatia 2010: foreign 4 stays");
        uint8_t eng[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 3};
        season_nation_state("Germany", 2005, &s);
        CHECK(season_nation_squad_record(eng, &s, 1) == 1 && eng[5] == 4 && eng[0] == 0xff, "Germany 2005: non-EU 4");
        uint8_t pol[6] = {0xff, 0xff, 5, 0xff, 0xff, 0xff};
        season_nation_state("Poland", 2010, &s);
        season_nation_squad_record(pol, &s, 1);
        CHECK(pol[2] == 0xff && pol[5] == 0xff, "Poland 2010: no limit of either kind");
    }
    /* Only Croatia's rows move a foreign limit to the non-EU slots (a non-EU number with no foreign limit). */
    for (int i = 0; i < season_nation_count; i++)
        for (int y = 2001; y <= 2027; y++) {
            season_nation_state(season_nations[i].name, y, &s);
            int moves = s.squad_foreign == SEASON_NATION_NONE && s.squad_non_eu != SEASON_NATION_KEEP &&
                        s.squad_non_eu != SEASON_NATION_NONE;
            if (moves && strcmp(season_nations[i].name, "Croatia"))
                CHECK(0, "%s %d would move a foreign limit to the non-EU slots", season_nations[i].name, y);
        }

    /* Brexit for one signing (2021/22 = season 2021, the date in 2021, the rule from 2021). */
    CHECK(season_brexit_blocks("England", 2021, 2021, 2021, 1, "France", NULL) == 1, "a French player at an English club: the permit decision");
    CHECK(season_brexit_blocks("England", 2021, 2021, 2021, 1, "Republic of Ireland", NULL) == 0, "an Irish player at an English club goes through");
    CHECK(season_brexit_blocks("England", 2021, 2021, 2021, 1, "France", "Republic of Ireland") == 0, "a second Irish nationality goes through");
    CHECK(season_brexit_blocks("England", 2021, 2021, 2021, 1, "Wales", NULL) == 0, "a Welsh player goes through");
    CHECK(season_brexit_blocks("Republic of Ireland", 2021, 2021, 2021, 1, "France", NULL) == 0, "a club in Ireland is never touched");
    CHECK(season_brexit_blocks("Scotland", 2021, 2021, 2021, 1, "Spain", NULL) == 1, "Scotland");
    CHECK(season_brexit_blocks("England", 2019, 2020, 2021, 1, "France", NULL) == 0, "2019/20: before the rule, a French player goes through");
    CHECK(season_brexit_blocks("England", 2020, 2021, 2021, 1, "France", NULL) == 1, "January 2021 in season 2020/21");
    CHECK(season_brexit_blocks("England", 2021, 2021, 2021, 0, "Brazil", NULL) == 0, "a player not of the EU is left to the game's own rule");
    CHECK(season_brexit_blocks("Germany", 2021, 2021, 2021, 1, "France", NULL) == 0, "Germany");

    /* Sweden has no windows until the table knows its dates. */
    season_nation_state("Sweden", 2002, &s);
    CHECK(s.windows == SEASON_NATION_WINDOWS_NONE, "Sweden 2002 windows %d", s.windows);

    /* A nation with a game-side meaning of its window indexes is left alone. */
    season_nation_state("France", 2024, &s);
    CHECK(s.windows == SEASON_NATION_KEEP, "France 2024 windows are not rewritten (%d)", s.windows);
    season_nation_state("Argentina", 2020, &s);
    CHECK(s.windows == SEASON_NATION_KEEP, "Argentina windows are not rewritten (%d)", s.windows);

    /* Unknown nation, and a season before every row. */
    CHECK(season_nation_state("Nowhere", 2010, &s) == 0 && s.windows == SEASON_NATION_KEEP, "unknown nation");
    CHECK(season_nation_state("England", 2000, &s) == 0 && s.windows == SEASON_NATION_KEEP, "before the first row");

    /* The nations, their globals, and the order of the rows (the fold needs seasons in order). */
    const SeasonNation *n = season_nation_find("England");
    CHECK(n && n->key_global == 0x9cf2e4u, "England global");
    n = season_nation_find("Italy");
    CHECK(n && (n->flags & SEASON_NATION_PARTIAL_WINDOWS), "Italy keeps its free-transfer window");
    CHECK(season_nation_find("Spain") && !(season_nation_find("Spain")->flags & SEASON_NATION_PARTIAL_WINDOWS), "Spain");
    CHECK(!season_nation_find("Nowhere") && !season_nation_find(NULL), "no such nation");
    CHECK(season_nation_count == 27, "27 nations (%d)", season_nation_count);
    for (int i = 0; i < season_nation_rule_count; i++) {
        const SeasonNationRule *r = &season_nation_rules[i];
        CHECK(season_nation_find(r->nation) != NULL, "row %d: nation %s is not in the nation list", i, r->nation);
        if (i > 0 && !strcmp(season_nation_rules[i - 1].nation, r->nation))
            CHECK(season_nation_rules[i - 1].season < r->season, "row %d: seasons not in order", i);
        CHECK(r->windows <= SEASON_NATION_MAX_WINDOWS, "row %d: %d windows", i, r->windows);
        for (int w = 0; w < (r->windows > 0 ? r->windows : 0); w++)
            CHECK(r->win[w].sd >= 1 && r->win[w].sd <= 31 && r->win[w].sm >= 1 && r->win[w].sm <= 12 &&
                  r->win[w].ed >= 1 && r->win[w].ed <= 31 && r->win[w].em >= 1 && r->win[w].em <= 12,
                  "row %d: window %d out of range", i, w);
    }
    /* Open item 258 (gap row 2 of 0.7.2): the Irish windows after the calendar move. The rows hold the real season
     * of the year after their season (row 2004/05 "2005"), so while the move is on, the season S of the game reads
     * row S - 1 from 2003 (season_nation_row_season). 2003 and 2004 are not stated: the game's own window stays
     * (IRELAND_RULES 0x63c210: 10 May to 31 January, 0x63c2e4..0x63c30f). 2005: 7 January to 4 March and July. */
    CHECK(season_nation_row_season("Republic of Ireland", 2005, 1) == 2004 &&
          season_nation_row_season("Republic of Ireland", 2003, 1) == 2002 &&
          season_nation_row_season("Republic of Ireland", 2002, 1) == 2002 &&
          season_nation_row_season("Republic of Ireland", 2005, 0) == 2005 &&
          season_nation_row_season("England", 2005, 1) == 2005, "Irish seasons read the row of their real season");
    SeasonNationState ie;
    season_nation_state("Republic of Ireland", season_nation_row_season("Republic of Ireland", 2004, 1), &ie);
    CHECK(ie.windows == SEASON_NATION_KEEP, "Ireland 2004: not stated, the game's own window stays");
    season_nation_state("Republic of Ireland", season_nation_row_season("Republic of Ireland", 2005, 1), &ie);
    CHECK(ie.windows == 2 && ie.win[0].sd == 7 && ie.win[0].sm == 1 && ie.win[0].ed == 4 && ie.win[0].em == 3 &&
          ie.win[1].sd == 1 && ie.win[1].sm == 7 && ie.win[1].ed == 30 && ie.win[1].em == 7,
          "Ireland 2005: 7 January to 4 March, 1 to 30 July");
    season_nation_state("Republic of Ireland", season_nation_row_season("Republic of Ireland", 2010, 1), &ie);
    CHECK(ie.windows == 2 && ie.win[0].sd == 1 && ie.win[0].sm == 12 && ie.win[0].ed == 22 && ie.win[0].em == 2,
          "Ireland 2010: 1 December to 22 February");
    season_nation_state("Republic of Ireland", season_nation_row_season("Republic of Ireland", 2023, 1), &ie);
    CHECK(ie.windows == 2 && ie.win[1].sd == 1 && ie.win[1].ed == 28 && ie.win[1].em == 7, "Ireland 2023: 1 to 28 July");
    if (failures) {
        fprintf(stderr, "%d checks failed\n", failures);
        return 1;
    }
    puts("ok");
    return 0;
}
