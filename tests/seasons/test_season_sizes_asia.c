/* Test of the meetings of the J1 and K League tables (0.7.2 league sizes, slice 14, D8 and D16;
 * src/recomp_rt/win/season_sizes_asia_rows.c).
 *
 * Usage: season_sizes_asia_test <rules.tsv made by tools/seasons/make_rules.py>
 * A row of halves gives the meetings of each half. The J1 table holds both halves, and the K League stages play as one
 * table of the same matches, so both play twice the row's meetings. A row of one table, and every other file, keeps
 * the row's meetings. */
#include "season_league.h"
#include "season_rules.h"
#include "season_sizes.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void table(const char *file, int season, int want)
{
    int got = season_sizes_asia_meetings(file, season, season_league_meetings(file, season));
    CHECK(got == want, "%s %d: table meetings %d, want %d", file, season, got, want);
}

int main(int argc, char **argv)
{
    if (argc != 2 || season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "usage: season_sizes_asia_test <rules.tsv>\n");
        return 2;
    }
    table("jap_j1", 2001, 2);       /* 2002: two halves of one meeting, 16 clubs */
    table("jap_j1", 2003, 2);
    table("jap_j1", 2004, 2);       /* 2005: one table of 18, two meetings */
    table("jap_j1", 2014, 2);       /* 2015: two stages of one meeting, 18 clubs */
    table("jap_j1", 2015, 2);
    table("jap_j1", 2016, 2);       /* 2017: one table */
    table("jap_j1", 2023, 2);       /* 2024: 20 clubs */
    table("jap_j2", 2001, 4);       /* 2002: 12 clubs, four meetings */
    table("jap_j2", 2007, 3);       /* 2008: 15 clubs, three */
    table("jap_j2", 2009, 2);       /* 2010: 19 clubs, two */
    table("kor_league", 2001, 3);   /* 2002: 10 clubs, three meetings */
    table("kor_league", 2002, 4);   /* 2003: 12 clubs, four */
    table("kor_league", 2003, 2);   /* 2004: two stages of one meeting: one table of two */
    table("kor_league", 2005, 2);   /* 2006 */
    table("kor_league", 2012, 2);   /* 2013: 14 clubs before the split */
    table("kor_league", 2013, 3);   /* 2014: 12 clubs, 33 rounds before the split */
    table("arg_prm", 2001, 1);      /* another file: the row's meetings of each half */
    table("eng_prm", 2001, 2);
    CHECK(season_sizes_asia_meetings("jap_j1", 2014, 0) == 0, "no meetings: the class's stay");
    CHECK(season_sizes_asia_meetings(NULL, 2014, 1) == 1, "no file");
    CHECK(season_sizes_asia_earlier("kor_league") == 17, "K League: 17 earlier starts");
    CHECK(season_sizes_asia_earlier("jap_j1") == 0 && season_sizes_asia_earlier(NULL) == 0, "other files: none");
    if (failures) fprintf(stderr, "%d failures\n", failures);
    else printf("PASS season_sizes_asia\n");
    return failures ? 1 : 0;
}
