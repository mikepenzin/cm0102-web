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
static int last_row_season;        /* the latest season of a row, 0 until season_rules_last_season reads it */

static void drop_index(void);

static void drop(void)
{
    drop_index();
    free(text);
    free(rows);
    text = NULL;
    rows = NULL;
    nrows = 0;
    last_row_season = 0;
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

const SeasonRule *season_rules_find_scan(int season, const char *file, int n)
{
    for (int i = 0; i < nrows; i++)
        if (rows[i].season == season && (has_file(rows[i].comp_file, file) || new_file(&rows[i], file)) && n-- == 0)
            return &rows[i];
    return NULL;
}

/* 0.6.1: an index for season_rules_find, made at its first call after a load. The day code asks for the
 * rows of each competition many times a day, and the scan of every row with its file lists took about a
 * tenth of "Initialising Leagues" with all nations (docs/research/2026-10-29-new-game-speed.md). A key is
 * a season and a file, one of the comp_file list of a row or the new_ file of a new competition; its rows
 * are in table order, as the scan finds them. tests/seasons/test_season_rules.c compares the two. */
typedef struct {
    int season;
    char *file;                    /* NULL: a free slot */
    int *rows, n, room;
} FindKey;

static FindKey *keys;
static uint32_t key_slots;         /* a power of 2, at least twice the keys */
static int index_made;

static void drop_index(void)
{
    for (uint32_t i = 0; i < key_slots; i++) {
        free(keys[i].file);
        free(keys[i].rows);
    }
    free(keys);
    keys = NULL;
    key_slots = 0;
    index_made = 0;
}

static uint32_t key_hash(int season, const char *file, size_t len)
{
    uint32_t h = 2166136261u ^ (uint32_t)season;
    for (size_t i = 0; i < len; i++) h = (h ^ (uint8_t)file[i]) * 16777619u;
    return h;
}

static FindKey *key_slot(int season, const char *file, size_t len)
{
    for (uint32_t i = key_hash(season, file, len) & (key_slots - 1); ; i = (i + 1) & (key_slots - 1)) {
        FindKey *k = &keys[i];
        if (!k->file || (k->season == season && strlen(k->file) == len && !memcmp(k->file, file, len))) return k;
    }
}

static int key_add(int season, const char *file, size_t len, int row)
{
    FindKey *k = key_slot(season, file, len);
    if (!k->file) {
        if (!(k->file = malloc(len + 1))) return 0;
        memcpy(k->file, file, len);
        k->file[len] = 0;
        k->season = season;
    }
    if (k->n && k->rows[k->n - 1] == row) return 1;           /* the same file twice in one list */
    if (k->n == k->room) {
        int room = k->room ? 2 * k->room : 4;
        int *more = realloc(k->rows, (size_t)room * sizeof *more);
        if (!more) return 0;
        k->rows = more;
        k->room = room;
    }
    k->rows[k->n++] = row;
    return 1;
}

static int make_index(void)
{
    index_made = 1;
    uint32_t want = 0;
    for (int i = 0; i < nrows; i++) {
        want += 2;
        for (const char *p = rows[i].comp_file; (p = strchr(p, ';')); p++) want++;
    }
    for (key_slots = 64; key_slots < 2 * (want + (uint32_t)nrows); key_slots *= 2) {}
    if (!(keys = calloc(key_slots, sizeof *keys))) { key_slots = 0; return 0; }
    for (int i = 0; i < nrows; i++) {
        const SeasonRule *r = &rows[i];
        for (const char *p = r->comp_file; *p; ) {
            const char *e = strchr(p, ';');
            size_t len = e ? (size_t)(e - p) : strlen(p);
            if (!key_add(r->season, p, len, i)) goto fail;
            if (!e) break;
            p = e + 1;
        }
        if (!strcmp(r->comp_file, "new") && r->comp) {
            size_t len = 4;
            char *name = malloc(strlen(r->comp) + 5);
            if (!name) goto fail;
            memcpy(name, "new_", 4);
            for (const char *c = r->comp; *c; c++)
                name[len++] = *c == ' ' ? '_' : (*c >= 'A' && *c <= 'Z') ? (char)(*c - 'A' + 'a') : *c;
            int ok = key_add(r->season, name, len, i);
            free(name);
            if (!ok) goto fail;
        }
    }
    return 1;
fail:
    drop_index();
    index_made = 1;                /* out of memory: the scan */
    return 0;
}

const SeasonRule *season_rules_find(int season, const char *file, int n)
{
    if (!index_made) make_index();
    if (!keys) return season_rules_find_scan(season, file, n);
    FindKey *k = key_slot(season, file, strlen(file));
    return k->file && n >= 0 && n < k->n ? &rows[k->rows[n]] : NULL;
}

int season_rules_last_season(int season, const char *file)
{
    if (!last_row_season)
        for (int i = 0; i < nrows; i++)
            if (rows[i].season > last_row_season) last_row_season = rows[i].season;
    if (season >= SEASON_RULE_ENDED) return 0;
    for (int s = season < last_row_season ? season : last_row_season; s >= SEASON_RULE_FIRST; s--)
        if (season_rules_find(s, file, 0)) return s;
    return 0;
}

static int moved_on;

void season_rules_set_moved(int on) { moved_on = on != 0; }

int season_rules_moved(const char *file) { return moved_on && file && !strncmp(file, "ire_", 4); }

/* 1 when the real season of a row begins with the year ("2004", "2002–03 (short season)"). */
static int real_begins(const SeasonRule *r, int year)
{
    char y[8];
    snprintf(y, sizeof y, "%d", year);
    return r && r->real_season && !strncmp(r->real_season, y, 4) &&
           (r->real_season[4] < '0' || r->real_season[4] > '9');
}

const SeasonRule *season_rules_row_of(const char *file, int year)
{
    if (!file) return NULL;
    if (season_rules_moved(file)) {
        const SeasonRule *r = season_rules_find(year, file, 0);
        if (real_begins(r, year)) return r;
        for (int n = 0; (r = season_rules_find(year - 1, file, n)) != NULL; n++)
            if (real_begins(r, year)) return r;
    }
    int s = season_rules_last_season(year, file);
    return s ? season_rules_find(s, file, 0) : NULL;
}
