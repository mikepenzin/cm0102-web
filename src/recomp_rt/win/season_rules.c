/* The rules table of the game (season_rules.h). The file format is that of tools/seasons/make_rules.py:
 * line 1 "#bosman-rules<TAB>1<TAB><row count>", line 2 the column names, then one row per line with tabs.
 * The reader keeps the file in one host block and points the fields of each row into it. It never touches
 * the game's memory, so the table cannot change a game. */
#include "season_rules.h"
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORMAT 1
#define MAX_ROWS 100000
#define MAX_BYTES (64u << 20)

/* The columns of the file, in order. */
static const char *const COLUMNS[] = {
    "id", "season", "nation", "comp_file", "comp", "tier", "name",
    "clubs", "up", "down", "subs_used", "bench", "playoffs", "split", "format",
    "clubs_text", "up_text", "down_text", "subs_used_text", "bench_text", "playoffs_text", "split_text",
    "format_text", "real_season", "foreign", "work_permit", "registration", "windows", "notes", "sources",
};
#define NCOL ((int)(sizeof COLUMNS / sizeof COLUMNS[0]))

static char *text;                 /* the file, with a NUL at the end of each field */
static SeasonRule *rows;
static int nrows;

/* An index of the rows by season for season_rules_find: the first row of each season and the next row of the same
 * season, in the order of the table, so a find reads the rows of its season only (about 280 of 7,102) and finds
 * the row that the scan of the whole table finds. Built at the first find after a load. */
#define INDEX_FIRST 1900
#define INDEX_SPAN 300
static int *next_same;
static int first_of[INDEX_SPAN];
static int indexed = -1;            /* 1: built, 0: a season outside the span (the finds scan the table), -1: not built */

static void drop_index(void)
{
    free(next_same);
    next_same = NULL;
    indexed = -1;
}

static void drop(void)
{
    free(text);
    free(rows);
    text = NULL;
    rows = NULL;
    nrows = 0;
    drop_index();
}

static void build_index(void)
{
    int last[INDEX_SPAN];
    drop_index();
    indexed = 0;
    for (int k = 0; k < INDEX_SPAN; k++) first_of[k] = last[k] = -1;
    if (nrows <= 0 || !(next_same = malloc((size_t)nrows * sizeof *next_same))) return;
    for (int i = 0; i < nrows; i++) {
        int k = rows[i].season - INDEX_FIRST;
        if (k < 0 || k >= INDEX_SPAN) {
            free(next_same);
            next_same = NULL;
            return;
        }
        next_same[i] = -1;
        if (last[k] >= 0) next_same[last[k]] = i;
        else first_of[k] = i;
        last[k] = i;
    }
    indexed = 1;
}

/* A count of the file: digits, or a code. -100 when the field fits neither. */
static int count_of(const char *s)
{
    static const struct { const char *code; int value; } codes[] = {
        {"na", SEASON_RULE_NA}, {"unknown", SEASON_RULE_UNKNOWN}, {"not_held", SEASON_RULE_NOT_HELD},
        {"see_each", SEASON_RULE_SEE_EACH}, {"as_cl", SEASON_RULE_AS_CL}, {"text", SEASON_RULE_TEXT},
    };
    if (*s >= '0' && *s <= '9') {
        long v = 0;
        for (const char *p = s; *p; p++) {
            if (*p < '0' || *p > '9') return -100;
            v = v * 10 + (*p - '0');
            if (v > SEASON_RULE_MAX_COUNT) return -100;
        }
        return (int)v;
    }
    for (size_t i = 0; i < sizeof codes / sizeof codes[0]; i++)
        if (!strcmp(s, codes[i].code)) return codes[i].value;
    return -100;
}

static int tier_of(const char *s)
{
    if (*s >= '0' && *s <= '9') return count_of(s);
    if (!strcmp(s, "cup")) return SEASON_TIER_CUP;
    if (!strcmp(s, "super_cup")) return SEASON_TIER_SUPER_CUP;
    if (!strcmp(s, "na")) return SEASON_TIER_NA;
    static const char *const other[] = {"state_1", "international", "national_teams", "world", "nation"};
    for (size_t i = 0; i < sizeof other / sizeof other[0]; i++)
        if (!strcmp(s, other[i])) return SEASON_TIER_OTHER;
    return -100;
}

/* Ends the line at its '\n' and drops a '\r' before it (a table saved on Windows). Returns the '\n' or NULL. */
static char *end_line(char *line)
{
    char *nl = strchr(line, '\n');
    if (!nl) return NULL;
    *nl = 0;
    if (nl > line && nl[-1] == '\r') nl[-1] = 0;
    return nl;
}

/* Splits one line into NCOL fields in place. Returns 1 when it has exactly NCOL. */
static int split(char *line, char **f)
{
    int n = 0;
    f[n++] = line;
    for (char *p = line; *p; p++) {
        if (*p != '\t') continue;
        if (n == NCOL) return 0;
        *p = 0;
        f[n++] = p + 1;
    }
    return n == NCOL;
}

static int fail(const char *path, int line, const char *what)
{
    fprintf(stderr, "recomp: rules table %s, line %d: %s. The game plays without the table.\n", path, line, what);
    drop();
    return 0;
}

int season_rules_load(const char *path)
{
    drop();
    FILE *f = host_fopen(path, "rb");
    if (!f) return 0;
    char *buf = NULL;
    size_t size = 0, room = 0;
    for (;;) {
        if (size + 65536 + 1 > room) {
            room = room ? room * 2 : 1u << 20;
            if (room > MAX_BYTES) { fclose(f); free(buf); return fail(path, 0, "the file is too large"); }
            char *b = realloc(buf, room);
            if (!b) { fclose(f); free(buf); return fail(path, 0, "no memory"); }
            buf = b;
        }
        size_t got = fread(buf + size, 1, 65536, f);
        size += got;
        if (got < 65536) break;
    }
    int bad = ferror(f);
    fclose(f);
    if (bad) { free(buf); return fail(path, 0, "a read failed"); }
    buf[size] = 0;
    text = buf;

    /* Line 1: the format and the row count. Line 2: the columns. */
    char *line = text, *next;
    int want = -1, lineno = 1, version = 0;
    next = end_line(line);
    if (!next) return fail(path, 1, "no header");
    if (sscanf(line, "#bosman-rules\t%d\t%d", &version, &want) != 2 || version != FORMAT || want < 0
        || want > MAX_ROWS)
        return fail(path, 1, "not a rules table of format 1");
    line = next + 1;
    lineno = 2;
    next = end_line(line);
    if (!next) return fail(path, 2, "no column line");
    char *f2[NCOL];
    if (!split(line, f2)) return fail(path, 2, "the columns are not those of format 1");
    for (int i = 0; i < NCOL; i++)
        if (strcmp(f2[i], COLUMNS[i])) return fail(path, 2, "the columns are not those of format 1");

    rows = calloc((size_t)want + 1, sizeof *rows);
    if (!rows) return fail(path, 0, "no memory");
    for (line = next + 1; *line; line = next + 1) {
        lineno++;
        next = end_line(line);
        if (!next) return fail(path, lineno, "the last line has no end");
        if (nrows == want) return fail(path, lineno, "more rows than line 1 says");
        char *c[NCOL];
        if (!split(line, c)) return fail(path, lineno, "a wrong number of columns");
        SeasonRule *r = &rows[nrows];
        r->id = c[0];
        r->season = count_of(c[1]);
        r->nation = c[2];
        r->comp_file = c[3];
        r->comp = c[4];
        r->tier_code = c[5];
        r->tier = tier_of(c[5]);
        r->name = c[6];
        int *counts[] = {&r->clubs, &r->up, &r->down, &r->subs_used, &r->bench};
        for (int i = 0; i < 5; i++) *counts[i] = count_of(c[7 + i]);
        r->playoffs = c[12];
        r->split = c[13];
        r->format = c[14];
        r->clubs_text = c[15];
        r->up_text = c[16];
        r->down_text = c[17];
        r->subs_used_text = c[18];
        r->bench_text = c[19];
        r->playoffs_text = c[20];
        r->split_text = c[21];
        r->format_text = c[22];
        r->real_season = c[23];
        r->foreign = c[24];
        r->work_permit = c[25];
        r->registration = c[26];
        r->windows = c[27];
        r->notes = c[28];
        r->sources = c[29];
        if (!*r->id || r->season < SEASON_RULE_FIRST || r->season > SEASON_RULE_LAST)
            return fail(path, lineno, "a bad id or season");
        if (r->tier == -100) return fail(path, lineno, "a bad tier");
        for (int i = 0; i < 5; i++)
            if (*counts[i] == -100) return fail(path, lineno, "a bad count");
        nrows++;
    }
    if (nrows != want) return fail(path, lineno, "fewer rows than line 1 says");
    drop_index();
    return nrows;
}

int season_rules_init(const char *game_dir)
{
    char path[1200];
    const char *v = getenv("RECOMP_RULES_TABLE");      /* test runs: the table of the build folder */
    if (v && *v) snprintf(path, sizeof path, "%s", v);
    else snprintf(path, sizeof path, "%s/Seasons/rules.tsv", game_dir);
    int n = season_rules_load(path);
    if (n) fprintf(stderr, "recomp: rules table: %d rows from %s\n", n, path);
    else fprintf(stderr, "recomp: rules table: none (%s)\n", path);
    return n;
}

int season_rules_count(void) { return nrows; }

const SeasonRule *season_rules_row(int i) { return i >= 0 && i < nrows ? &rows[i] : NULL; }

/* 1 when the ";" list holds the name. */
static int has_file(const char *list, const char *file)
{
    size_t n = strlen(file);
    for (const char *p = list; *p; ) {
        const char *e = strchr(p, ';');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len == n && !memcmp(p, file, n)) return 1;
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

/* Slice 24: a row of a new competition (comp_file "new") has the file of its def in season_comps.c: "new_" and its
 * comp in lower case with "_" for a space ("Conference League" gives new_conference_league), as
 * tests/seasons/rules_report_check.py reads it. */
static int new_file(const SeasonRule *r, const char *file)
{
    if (strcmp(r->comp_file, "new") || strncmp(file, "new_", 4) || !r->comp) return 0;
    const char *p = r->comp, *f = file + 4;
    for (; *p && *f; p++, f++) {
        char c = *p == ' ' ? '_' : (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
        if (c != *f) return 0;
    }
    return !*p && !*f;
}

const SeasonRule *season_rules_find(int season, const char *file, int n)
{
    if (indexed < 0) build_index();
    if (indexed == 1) {
        if (season < INDEX_FIRST || season >= INDEX_FIRST + INDEX_SPAN) return NULL;
        for (int i = first_of[season - INDEX_FIRST]; i >= 0; i = next_same[i])
            if ((has_file(rows[i].comp_file, file) || new_file(&rows[i], file)) && n-- == 0) return &rows[i];
        return NULL;
    }
    for (int i = 0; i < nrows; i++)
        if (rows[i].season == season && (has_file(rows[i].comp_file, file) || new_file(&rows[i], file)) && n-- == 0)
            return &rows[i];
    return NULL;
}
