/* Test of the league structure of the rules table (0.7.2 league sizes, slice 1): season_league_meetings and the
 * structure fields of season_league_table.c (src/recomp_rt/win/season_league.c).
 *
 * Usage: season_league_test <rules.tsv made by tools/seasons/make_rules.py>
 * The meetings of a row (words of the format text, a count of matches, a hand row), the after-table rule (a season
 * after the last row takes the last row), a future row with unknown clubs (it takes the row before it), a file
 * that is not a league or has no row (0: the class's meetings stay), and for every row whose values are known: the
 * rounds are meetings x (n' - 1) + the extra rounds that the rest rounds of an odd count do not hold, and a split's
 * first section is its top group. */
#include "season_league.h"
#include "season_rules.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void meet(const char *file, int season, int want)
{
    int got = season_league_meetings(file, season);
    CHECK(got == want, "%s %d: meetings %d, want %d", file, season, got, want);
}

/* The club count of the table row that a rule row encodes (the first row of its file and season). */
static int clubs_of(const SeasonLeagueRule *e)
{
    const SeasonRule *r = season_rules_find(e->season, e->file, 0);
    return r ? r->clubs : -2;
}

int main(int argc, char **argv)
{
    if (argc != 2 || season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "usage: season_league_test <rules.tsv>\n");
        return 2;
    }
    meet("eng_prm", 2001, 2);           /* double round robin */
    meet("sco_first", 2001, 4);         /* 36 matches (each pair four times) */
    meet("sco_prm", 2001, 3);           /* split after 33 matches of 12 clubs */
    meet("cro_a1", 2007, 3);            /* triple round robin */
    meet("cro_a1", 2006, 3);            /* a third game set by the standings (MEETINGS_FIX) */
    meet("arg_prm", 2001, 1);           /* two halves of one meeting */
    meet("arg_prm", 2024, 1);           /* zones of one meeting */
    meet("jap_j2", 2001, 4);            /* a hand row: J2 of 12 clubs, 44 rounds */
    meet("jap_j2", 2010, 2);            /* a hand row: J2 of 19 clubs, 2 meetings */
    meet("ita_ser_a", 2027, 2);         /* unknown clubs: the row before */
    meet("ita_ser_a", 2300, 2);         /* after the table: the last row, then the row before */
    meet("aus_nsl", 2019, 3);           /* gap row 7: 26 of 11 clubs, 3 meetings (30) are nearest */
    meet("aus_nsl", 2020, 2);           /* 26 of 12 clubs (22 + 4 third meetings): 2 are nearest */
    meet("aus_nsl", 2024, 2);           /* 26 of 13 clubs (24 + 2) */
    meet("aus_nsl", 2027, 2);           /* unknown clubs: the 2026 row */
    meet("gre_cup", 2010, 0);           /* a cup */
    meet("no_such_file", 2010, 0);

    CHECK(season_league_row("arg_second", 2026) == season_league_rule("arg_second", 2025),
          "arg_second 2026 (unknown clubs): the structure of 2025");
    const SeasonLeagueRule *f27 = season_league_rule("arg_prm", 2026);  /* 2027: halves of one table of 30 */
    CHECK(f27 && f27->halves == 1 && f27->zones == 0 && f27->extra_rounds == 1 && f27->meetings == 1,
          "arg_prm 2026: halves of one table with a derby round");
    CHECK(season_league_row("eng_prm", 2400) == season_league_rule("eng_prm", 2027), "eng_prm 2400: the last row");
    CHECK(!season_league_row("no_such_file", 2010), "no row");
    const SeasonLeagueRule *a = season_league_rule("arg_prm", 2024);
    CHECK(a && a->halves == 1 && a->zones == 2 && a->zone_size[0] == 15 && a->zone_size[1] == 15 &&
          a->extra_rounds == 2 && a->rounds == 16, "arg_prm 2024: halves, two zones of 15, 2 inter-zone rounds, 16");
    const SeasonLeagueRule *n = season_league_rule("nir_prm", 2010);
    CHECK(n && n->meetings == 3 && n->split_after == 33 && n->split_size[0] == 6 && n->split_size[1] == 6 &&
          n->split_meetings[0] == 1 && n->split_meetings[1] == 1 && n->rounds == 33,
          "nir_prm 2010: 3 meetings, split after 33 into 6 and 6, one more meeting");
    /* Gap rows 1 to 6 of the 0.7.2 gaps list. Row 1: the 2002-03 First Division played one table of 12, each pair
     * twice (22 matches). Rows 3 to 5: a season after the last real one keeps the last known row. */
    const SeasonLeagueRule *i1 = season_league_rule("ire_first", 2002);
    CHECK(i1 && clubs_of(i1) == 12 && i1->meetings == 2 && i1->rounds == 22 && !i1->zones,
          "ire_first 2002: one table of 12, 2 meetings, 22 rounds");
    meet("ire_prm", 2002, 3);           /* 2002-03 (August to January): 27 matches of 10 */
    const SeasonLeagueRule *i5 = season_league_rule("ire_first", 2025);
    CHECK(i5 && clubs_of(i5) == 10 && i5->split_after == 0 && season_league_row("ire_first", 2026) == i5 &&
          season_league_row("ire_first", 2027) == i5, "ire_first 2026 and 2027: the 2025 row (10, no split)");
    CHECK(season_league_row("ita_ser_c2a", 2027) == season_league_rule("ita_ser_c2a", 2026),
          "ita_ser_c2a 2027 (unknown): the 2026 row");
    const SeasonLeagueRule *s3 = season_league_row("swe_prm", 2027);
    CHECK(s3 && clubs_of(s3) == 16, "swe_prm 2027: 16 clubs (last known)");
    /* Gap row 2: the season 2002/03 holds two Irish rows, the short 2002-03 season and the first calendar season
     * 2003. The edition +0x40 of each season takes its own row: 2002 the short season, 2003 the second row, 2004 the
     * 2003/04 row (real 2004), 2005 the 2004/05 row (real 2005). */
    struct { const char *file; int edition; const char *id; int meetings; } ed[] = {
        {"ire_prm", 2001, "wal-nir-irl-16", 3}, {"ire_prm", 2002, "wal-nir-irl-41", 3},
        {"ire_prm", 2003, "wal-nir-irl-41b", 4}, {"ire_prm", 2004, "wal-nir-irl-66", 4},
        {"ire_prm", 2005, "wal-nir-irl-91", 3}, {"ire_first", 2002, "wal-nir-irl-42", 2},
        {"ire_first", 2003, "wal-nir-irl-42b", 3}, {"ire_first", 2004, "wal-nir-irl-67", 3},
    };
    for (size_t k = 0; k < sizeof ed / sizeof ed[0]; k++) {
        const SeasonRule *r = season_league_edition_row(ed[k].file, ed[k].edition);
        CHECK(r && !strcmp(r->id, ed[k].id), "%s edition %d: row %s, want %s", ed[k].file, ed[k].edition,
              r ? r->id : "none", ed[k].id);
        int m = r ? season_league_meetings_of(ed[k].file, r) : -1;
        CHECK(m == ed[k].meetings, "%s edition %d: meetings %d, want %d", ed[k].file, ed[k].edition, m,
              ed[k].meetings);
    }
    CHECK(season_league_edition("ire_prm", 2003) == 2002 && season_league_edition("ire_prm", 2004) == 2003 &&
          season_league_edition("ire_prm", 2002) == 2002, "ire_prm editions 2002 to 2004: seasons 2002, 2002, 2003");
    /* Unchanged elsewhere: a calendar-year league takes row Y - 1, a two-year league row Y, and jap_j1 its 2026/27
     * row in 2026 (row Y first). */
    const SeasonRule *nor = season_league_edition_row("nor_prm", 2009), *eng = season_league_edition_row("eng_prm", 2009);
    CHECK(nor && nor->season == 2008 && eng && eng->season == 2009, "nor_prm 2009: row 2008; eng_prm 2009: row 2009");
    const SeasonRule *jap = season_league_edition_row("jap_j1", 2026);
    CHECK(jap && jap->season == 2026, "jap_j1 2026: row 2026/27");
    const SeasonLeagueRule *h = season_league_rule("hol_first", 2005);
    CHECK(h && h->periods == 6, "hol_first 2005: six periods");
    /* Gap rows 38, 39 and 41 (slice 11). */
    for (int y = 2002; y <= 2005; y++) {
        const SeasonLeagueRule *c = season_league_rule(y == 2003 ? "cro_a2b" : "cro_a2a", y);
        CHECK(c && c->split_after == 22 && c->split_size[0] == 6 && c->split_size[1] == 6 &&
              c->split_meetings[0] == 2 && c->split_meetings[1] == 2 && !c->split_halved[0] && !c->split_halved[1],
              "cro_a2a and cro_a2b %d: 22, then 6 and 6 twice, points kept", y);
    }
    for (int y = 2024; y <= 2026; y++) {
        const SeasonLeagueRule *g = season_league_rule("gre_prm", y);
        CHECK(g && g->split_after == 26 && g->split_size[1] == 4 && !g->split_halved[0] && g->split_halved[1] &&
              !g->split_halved[2], "gre_prm %d: only the 5th to 8th halve", y);
    }
    const SeasonLeagueRule *g23 = season_league_rule("gre_second", 2023), *g24 = season_league_rule("gre_second", 2024),
                           *g25 = season_league_rule("gre_second", 2025);
    CHECK(g23 && g23->split_after == 22 && g23->split_size[0] == 5 && g23->split_size[1] == 6 &&
          g23->split_size[2] == 1 && g23->split_meetings[0] == 2 && g23->split_meetings[1] == 2 &&
          g23->split_halved[0] && g23->split_halved[1], "gre_second 2023: 22, then 5, 6 and the 12th, halved");
    CHECK(g24 && g24->split_after == 18 && g24->split_size[0] == 5 && g24->split_size[1] == 5 &&
          g24->split_meetings[0] == 2 && g24->split_meetings[1] == 2 && g24->split_halved[0] && g24->split_halved[1],
          "gre_second 2024: 18, then 5 and 5 twice, halved");
    CHECK(g25 && g25->split_after == 18 && g25->split_size[0] == 4 && g25->split_size[1] == 6 &&
          g25->split_meetings[0] == 2 && g25->split_meetings[1] == 2 && g25->split_halved[0] && g25->split_halved[1],
          "gre_second 2025: 18, then 4 and 6 twice, halved");

    int checked = 0;
    for (int i = 0; i < season_league_rule_count; i++) {
        const SeasonLeagueRule *e = &season_league_rules[i];
        int k = 0;                                      /* the k-th row of the file and season: the groups */
        for (int j = 0; j < i; j++)
            k += season_league_rules[j].season == e->season && !strcmp(season_league_rules[j].file, e->file);
        const SeasonRule *r = season_rules_find(e->season, e->file, k);
        if (!e->meetings || !e->rounds || e->extra_rounds < 0 || !r) continue;
        int big = 0;
        for (int z = 0; z < e->zones; z++)
            if (e->zone_size[z] > big) big = e->zone_size[z];
        int count = e->zones ? big : r->clubs, sum = 0;
        for (int z = 0; z < e->zones; z++) sum += e->zone_size[z];
        CHECK(!e->zones || r->clubs <= 0 || sum == r->clubs, "%s %d: zones add up to %d, not %d", e->file, e->season,
              sum, r->clubs);
        if (count <= 0) continue;
        checked++;
        int rests = count % 2 ? e->meetings : 0, extra = e->extra_rounds > rests ? e->extra_rounds - rests : 0;
        CHECK(e->rounds == e->meetings * (count + count % 2 - 1) + extra, "%s %d: rounds %d", e->file, e->season,
              e->rounds);
        CHECK(e->split_after == 0 || e->split_size[0] == e->split_top, "%s %d: split sections", e->file, e->season);
    }
    CHECK(checked > 1000, "rows with rounds: %d", checked);
    printf(failures ? "FAILED: %d\n" : "PASS\n", failures);
    return failures ? 1 : 0;
}
