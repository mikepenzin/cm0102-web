/* Test of the places of Japan and Finland that the rules table decides (0.7.3 slice 4, open items 283 and 284;
 * src/recomp_rt/win/season_sizes_places.c).
 *
 * Usage: season_sizes_places_test <rules.tsv made by tools/seasons/make_rules.py>
 * The rows are those of docs/research/rules-0.6.0/aus-jap-kor.tsv and nor-fin.tsv. */
#include "season_league.h"
#include "season_rules.h"
#include "season_sizes.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

int main(int argc, char **argv)
{
    if (argc != 2 || season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "usage: season_sizes_places_test <rules.tsv>\n");
        return 2;
    }
    /* J1 16th v J2 3rd: rows 2003 to 2007 (the real seasons 2004 to 2008), and no other season. */
    for (int s = 2001; s <= 2027; s++) {
        int want = s >= 2003 && s <= 2007;
        CHECK(season_places_jap_cross(s) == want, "row %d: J1 16th v J2 3rd %d, want %d", s, season_places_jap_cross(s),
              want);
    }
    /* The four-club J2 play-off of 2023: J1 sends 1 down, takes 3 up and grows from 18 to 20. */
    CHECK(season_places_jap_grows(2022, 2), "row 2022: J1 grows by the winner of the J2 play-off");
    CHECK(!season_places_jap_grows(2021, 2), "row 2021: J1 keeps 18 clubs");
    CHECK(!season_places_jap_grows(2023, 2), "row 2023: J1 keeps 20 clubs");
    CHECK(!season_places_jap_grows(2019, 2), "row 2019: J1 grows with no play-off winner");
    CHECK(!season_places_jap_grows(2022, 3), "row 2022: three direct places do not fit");
    /* J2 sends clubs down to no division only in the edition before its count changes: 2023, 22 to 20 clubs. */
    for (int y = 2002; y <= 2027; y++) {
        int want = y == 2023 ? 2 : 0;
        CHECK(season_places_jap_j2_down(y) == want, "edition %d: J2 %d down, want %d", y, season_places_jap_j2_down(y),
              want);
    }
    /* The Veikkausliiga plays no relegation play-off in the real seasons 2002, 2006 and 2011 to 2014. Edition 2001
     * falls back to the row of real 2002, so it keeps the class's play-off. */
    for (int y = 2001; y <= 2028; y++) {
        int want = y == 2002 || y == 2006 || (y >= 2011 && y <= 2014);
        CHECK(season_places_fin_no_playoff(y) == want, "edition %d: no play-off %d, want %d", y,
              season_places_fin_no_playoff(y), want);
    }
    if (failures) fprintf(stderr, "%d failures\n", failures);
    else printf("PASS season_sizes_places\n");
    return failures ? 1 : 0;
}
