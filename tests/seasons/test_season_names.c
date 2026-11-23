/* Test of the names lookup after the table (0.6.2 open item 71, src/recomp_rt/win/season_names.c).
 *
 * Usage: season_names_test <names.tsv of the build> <scratch folder>
 * After the table's last season each file keeps the names of its last season with a name (the after-table rule), and
 * up to the last season nothing changes. A small table of the test checks a file named in the last season, a file
 * whose names end earlier, a gap inside the table, a second load and a load that fails. The names table of the
 * build checks every file and object to 2301. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_names.h"
#include "season_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The game side, which the lookup never calls. */
Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
void o_008386f0(void) {}
const SeasonCompClass season_comp_classes[1];
const int season_comp_class_count = 0;
int season_code_on(void) { return 1; }
int season_changing_rules(void) { return 1; }
int season_start_year(void) { return 2025; }
int season_change_point(void) { return 0; }
int season_change_renames(void) { return 0; }
void season_change_rename(uint32_t rec, const char *name) { (void)rec, (void)name; }
const char *season_change_data_name(uint32_t rec) { (void)rec; return NULL; }
const char *season_comps_file(uint32_t obj) { (void)obj; return NULL; }
void season_comps_before_comp_table(void) {}
int win_game_date(int *year, int *month, int *day) { (void)year, (void)month, (void)day; return 0; }

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(2); }
    fputs(text, f);
    fclose(f);
}

static int is(const char *got, const char *want)
{
    return want ? got && !strcmp(got, want) : !got;
}

#define NAME(file, season, n) season_names_get_nth(file, season, n, NULL)

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: season_names_test <names.tsv> <scratch folder>\n");
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof path, "%s/names-small.tsv", argv[2]);
    write_file(path, "#bosman-names\t2\t6\nseason\tfile\tn\tlong\tshort\tabbr\n"
                     "2025\tlast\t0\tLast 2025\tL25\tL\n2027\tlast\t0\tLast 2027\tL27\tL\n"
                     "2025\tearly\t0\tEarly 0\tE0\tE\n2025\tearly\t1\tEarly 1\tE1\tE\n"
                     "2024\tgap\t0\tGap 2024\tG24\tG\n2026\tgap\t0\tGap 2026\tG26\tG\n");
    CHECK(season_names_load(path) == 6, "the small table");
    CHECK(is(NAME("last", 2027, 0), "Last 2027") && is(NAME("last", 2028, 0), "Last 2027") &&
          is(NAME("last", 2301, 0), "Last 2027"), "a file named in the last season keeps its names");
    CHECK(is(NAME("early", 2026, 0), NULL) && is(NAME("early", 2027, 1), NULL), "inside the table nothing changes");
    CHECK(is(NAME("early", 2028, 0), "Early 0") && is(NAME("early", 2301, 1), "Early 1") &&
          is(NAME("early", 2301, 2), "Early 1"), "a file whose names end earlier keeps its last names");
    CHECK(is(NAME("gap", 2025, 0), NULL) && is(NAME("gap", 2300, 0), "Gap 2026"), "a gap inside the table stays");
    CHECK(is(NAME("none", 2300, 0), NULL), "a file with no row");
    snprintf(path, sizeof path, "%s/names-bad.tsv", argv[2]);
    write_file(path, "#bosman-names\t2\t2\nseason\tfile\tn\tlong\tshort\tabbr\n2027\tlast\t0\tLast\tL\tL\n2027\tbad\n");
    CHECK(season_names_load(path) == 0 && is(NAME("last", 2300, 0), NULL), "a load that fails keeps no name");

    /* The names table of the build: after the table every file and object gives the names of the file's last
     * season with a name, to 2301. */
    int n = season_names_load(argv[1]), files = 0, wrong = 0;
    CHECK(n > 1000, "the names table of the build: %d rows", n);
    FILE *f = fopen(argv[1], "rb");
    char line[512], seen[1024][64];
    int last[1024], most[1024];
    while (f && fgets(line, sizeof line, f)) {
        int season, k;
        char file[64];
        if (sscanf(line, "%d\t%63[^\t]\t%d", &season, file, &k) != 3) continue;
        int i = 0;
        while (i < files && strcmp(seen[i], file)) i++;
        if (i == files && files < 1024) snprintf(seen[files], 64, "%s", file), last[files] = 0, most[files++] = 0;
        if (i < files && season > last[i]) last[i] = season, most[i] = k;
        else if (i < files && season == last[i] && k > most[i]) most[i] = k;
    }
    if (f) fclose(f);
    for (int i = 0; i < files; i++)
        for (int k = 0; k <= most[i]; k++)
            for (int season = 2028; season <= 2301; season += 91) {
                const char *want = NAME(seen[i], last[i], k), *got = NAME(seen[i], season, k);
                if (!is(got, want) && wrong++ < 5) fprintf(stderr, "  %s %d %d: %s\n", seen[i], k, season, got ? got : "none");
            }
    CHECK(files > 100 && !wrong, "%d files, %d names after the table differ from the last season's", files, wrong);
    printf(failures ? "FAILED: %d\n" : "PASS\n", failures);
    return failures ? 1 : 0;
}
