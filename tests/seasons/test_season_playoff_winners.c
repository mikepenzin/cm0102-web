/* Test of season_playoffs_winners_up (0.6.2 open item 63, src/recomp_rt/win/season_playoff_winners.c).
 *
 * Usage: season_playoff_winners_test <rules.tsv made by tools/seasons/make_rules.py>
 * Série B sends 2 play-off winners up from 2025/26 to 2301 and none before. Every other file of the table, in every
 * season from 2001 to 2301, sends none: the other rows with "two winners" (Belgium and Portugal 2008/09) are other
 * forms, whose up column already counts the winners or whose play-off is not 4 places below the direct places.
 * season_playoffs_finalists_up (gap row 57 and open item 249): Série B 2002 to 2005 (rows 2001 to 2004) send the 2
 * finalists of a play-off of the top 8 up, and no other row of any file does. */
#include "season_playoffs.h"
#include "season_rules.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

int main(int argc, char **argv)
{
    if (argc != 2 || season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "usage: season_playoff_winners_test <rules.tsv>\n");
        return 2;
    }
    for (int season = 2001; season <= 2301; season++) {
        int got = season_playoffs_winners_up("bra_nat_second", season), want = season >= 2025 ? 2 : 0;
        CHECK(got == want, "bra_nat_second %d: %d, want %d", season, got, want);
    }
    for (int season = 2000; season <= 2301; season++) {
        int got = season_playoffs_finalists_up("bra_nat_second", season), want = season >= 2001 && season <= 2004 ? 2 : 0;
        CHECK(got == want, "bra_nat_second %d: finalists %d, want %d", season, got, want);
    }
    CHECK(!season_playoffs_finalists_up("bra_nat_first", 2001), "Série A 2002: a top-8 play-off for the title only");
    CHECK(!season_playoffs_winners_up("bel_second", 2008) && !season_playoffs_winners_up("por_second_b", 2008),
          "Belgium and Portugal 2008/09");
    int others = 0, checked = 0;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        char files[256];
        snprintf(files, sizeof files, "%s", r->comp_file);
        for (char *f = strtok(files, ";"); f; f = strtok(NULL, ";")) {
            if (!strcmp(f, "bra_nat_second")) continue;
            for (int season = r->season; season <= r->season + 274; season += 137) {     /* the row, 137 and 274 on */
                checked++;
                if (season_playoffs_winners_up(f, season) || season_playoffs_finalists_up(f, season)) {
                    if (others++ < 5) fprintf(stderr, "  %s %d\n", f, season);
                }
            }
        }
    }
    CHECK(checked > 10000 && !others, "other files: %d of %d send winners up", others, checked);
    printf(failures ? "FAILED: %d\n" : "PASS\n", failures);
    return failures ? 1 : 0;
}
