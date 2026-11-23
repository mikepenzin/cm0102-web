/* Test of the substitute rule lookup (0.6.0 slice 17, src/recomp_rt/win/season_subs_rule.c).
 *
 * Usage: season_subs_test <rules.tsv made by tools/seasons/make_rules.py>
 * ctest makes the table first (test season_rules_table). The checks: known rows of the table (the English
 * Premier Division in the seasons of 3 and 5, 3 and 7, 3 and 9, 5 and 9 substitutes), a count above what the
 * match engine holds (12 on the bench for the World Cup, clamped to 9), a file with no row, a file whose
 * count is unknown, the first row of a file that has qualifying rows after it (the European Champions Cup),
 * a season outside the table, and an unknown count that keeps the latest earlier count of the file (item 274: the
 * Major League Soccer, the A-League, Argentina, the Champions League, the EFL Cup and the EFL Trophy). */
#include "season_rules.h"
#include "season_subs_rule.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void expect(int season, const char *file, int want_mask, int want_used, int want_bench)
{
    int used = -1, bench = -1;
    int mask = season_subs_rule(season, file, &used, &bench);
    CHECK(mask == want_mask, "%d %s: mask %d, want %d", season, file, mask, want_mask);
    if (want_mask & SEASON_SUBS_USED) CHECK(used == want_used, "%d %s: used %d, want %d", season, file, used, want_used);
    else CHECK(used == -1, "%d %s: used was written (%d)", season, file, used);
    if (want_mask & SEASON_SUBS_BENCH) CHECK(bench == want_bench, "%d %s: bench %d, want %d", season, file, bench, want_bench);
    else CHECK(bench == -1, "%d %s: bench was written (%d)", season, file, bench);
}

/* The seasons of the rows that gave the counts (season_subs_rule_from). */
static void expect_from(int season, const char *file, int want_used_from, int want_bench_from)
{
    int used = -1, bench = -1, used_from = -1, bench_from = -1;
    season_subs_rule_from(season, file, &used, &bench, &used_from, &bench_from);
    CHECK(used_from == want_used_from, "%d %s: used from %d, want %d", season, file, used_from, want_used_from);
    CHECK(bench_from == want_bench_from, "%d %s: bench from %d, want %d", season, file, bench_from, want_bench_from);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: season_subs_test <rules.tsv>\n");
        return 2;
    }
    CHECK(season_rules_load(argv[1]) > 0, "the table loads");
    const int both = SEASON_SUBS_USED | SEASON_SUBS_BENCH;
    expect(2001, "eng_prm", both, 3, 5);
    expect(2007, "eng_prm", both, 3, 5);
    expect(2008, "eng_prm", both, 3, 7);
    expect(2021, "eng_prm", both, 3, 9);
    expect(2022, "eng_prm", both, 5, 9);
    expect(2027, "eng_prm", both, 5, 9);
    /* 12 named for the World Cup in 2001: the match engine clamps to 9 (0x6b7feb), so the lookup does too. */
    expect(2001, "world_cup", both, 3, SEASON_SUBS_MAX);
    /* A file that the table does not hold, and a season outside it. */
    expect(2001, "no_such_file", 0, 0, 0);
    expect(2100, "eng_prm", 0, 0, 0);
    expect(2000, "eng_prm", 0, 0, 0);
    /* The first numeric row of a file with qualifying rows after it: the Champions League in 2012/13. */
    expect(2012, "european_cup", both, 3, 7);
    /* Item 274: an unknown count in a season row keeps the latest earlier count of the file, and the lookup says
     * which season gave it. The MLS bench of 2026 is unknown (9 in 2025), its substitutes are known (5). */
    expect(2026, "usa_mls", both, 5, 9);
    expect_from(2026, "usa_mls", 2026, 2025);
    /* The A-League substitutes of 2026 are unknown (5 in 2024; 2025 is unknown too), its bench is known (7, the
     * A-League 2026/27 competition rules). */
    expect(2026, "aus_nsl", both, 5, 7);
    expect_from(2026, "aus_nsl", 2024, 2026);
    /* Argentina 2027: both unknown (2026 too), both from 2025 (substitutes 5, bench 12 clamped to 9). */
    expect(2027, "arg_prm", both, 5, SEASON_SUBS_MAX);
    expect_from(2027, "arg_prm", 2025, 2025);
    /* The Champions League of 2027: its main row is unknown and its qualifying rows say "as the Champions League",
     * which follow the main row, so the main row takes 2026 (12 named, clamped to 9, and 5). */
    expect(2027, "european_cup", both, 5, SEASON_SUBS_MAX);
    expect_from(2027, "european_cup", 2026, 2026);
    /* The search back stops at a season with an n/a row: the Dutch Super Cup of 2021 (2020 is n/a, 2016 and 2017
     * have counts) keeps its own values. */
    expect(2021, "hol_super", 0, 0, 0);
    /* No count of a season before 2020 reaches 2020 or later (5 substitutes from 2020, IFAB): the Russian Premier
     * League of 2020 is unknown, 2019 has 3, so it keeps its own values. */
    expect(2020, "rus_prm", 0, 0, 0);
    /* Item 274, the EFL Cup (forum report: 5 on the bench in 2026/27). The EFL Cup rows give 9 named and 5 used
     * (2025/26: EFL Cup Rules rule 8.1; 2026/27 sourced), and 2027 is unknown, so 2027 takes 2026. The EFL Trophy
     * Rules 2025/26 give 7 named and 5 used, and 2026 is unknown, so 2026 takes 2025. The Trophy of 2023 has no count
     * from 2020 on before it (2020 to 2023 unknown, 2011 the last count), so it keeps its own values. */
    expect(2026, "eng_cc_cup", both, 5, 9);
    expect(2027, "eng_cc_cup", both, 5, 9);
    expect_from(2027, "eng_cc_cup", 2026, 2026);
    expect(2026, "eng_auto_cup", both, 5, 7);
    expect_from(2026, "eng_auto_cup", 2025, 2025);
    expect(2023, "eng_auto_cup", 0, 0, 0);
    /* A count of the season itself gives its own season. */
    expect_from(2022, "eng_prm", 2022, 2022);
    /* NULL arguments do nothing. */
    CHECK(season_subs_rule(2001, NULL, NULL, NULL) == 0, "NULL file");
    int u = -1;
    CHECK(season_subs_rule(2001, "eng_prm", &u, NULL) == both && u == 3, "NULL bench pointer");
    if (failures) {
        fprintf(stderr, "%d checks failed\n", failures);
        return 1;
    }
    puts("ok");
    return 0;
}
