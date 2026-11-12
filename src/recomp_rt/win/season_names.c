/* Competition names of each season (season_names.h). The table lives in host memory. The names go into the
 * competition records of the game's memory only in a season game. */
#include "win.h"
#include "season.h"
#include "season_names.h"
#include "season_change.h"
#include "season_rules.h"
#include "season_comps.h"
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORMAT 1
#define MAX_ROWS 100000
#define MAX_BYTES (16u << 20)
#define LONG_AT 4u                  /* record +4: long name, 52 bytes */
#define LONG_SIZE 52u
#define NAME_LIMIT 50u              /* the longest long name: the limit of slice 16 (season_change_rename) */
#define SHORT_AT 56u                /* record +56: short name, 27 bytes */
#define SHORT_SIZE 27u
#define COMP_TABLE 0xadadfcu        /* g_comp_object_table: Comp ** */
#define COMP_SLOTS 0xadadf8u        /* its slot count, s32 */
#define ENTRIES 0xb63d60u           /* the 35 league table entries, 0x48 bytes each: +0xc count, +0x10 array */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35u
#define MAX_OBJECTS 4096

typedef struct {
    int season, n;
    const char *file, *long_name, *short_name;
} NameRow;

static char *text;
static NameRow *rows;
static int nrows;

static int fail(const char *path, int line, const char *why)
{
    fprintf(stderr, "recomp: names table %s, line %d: %s; the game keeps the database names\n", path, line, why);
    free(text);
    free(rows);
    text = NULL;
    rows = NULL;
    nrows = 0;
    return 0;
}

static char *end_line(char *line)
{
    char *nl = strchr(line, '\n');
    if (!nl) return NULL;
    *nl = 0;
    if (nl > line && nl[-1] == '\r') nl[-1] = 0;
    return nl;
}

/* Splits a line into exactly n fields at tabs. */
static int split(char *line, char **out, int n)
{
    int k = 0;
    out[k++] = line;
    for (char *p = line; *p; p++)
        if (*p == '\t') {
            if (k == n) return 0;
            *p = 0;
            out[k++] = p + 1;
        }
    return k == n;
}

static int number(const char *s, int lo, int hi)
{
    if (!*s) return -1;
    long v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
        if (v > hi) return -1;
    }
    return v < lo ? -1 : (int)v;
}

int season_names_load(const char *path)
{
    free(text);
    free(rows);
    text = NULL;
    rows = NULL;
    nrows = 0;
    FILE *f = host_fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > (long)MAX_BYTES) { fclose(f); return fail(path, 0, "the file is too large"); }
    text = malloc((size_t)size + 1);
    if (!text) { fclose(f); return fail(path, 0, "no memory"); }
    size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) return fail(path, 0, "a read failed");
    text[size] = 0;
    char *line = text, *next = end_line(line);
    int want = -1, version = 0, lineno = 1;
    if (!next || sscanf(line, "#bosman-names\t%d\t%d", &version, &want) != 2 || version != FORMAT || want < 0 ||
        want > MAX_ROWS)
        return fail(path, 1, "not a names table of format 1");
    line = next + 1;
    lineno = 2;
    next = end_line(line);
    if (!next || strcmp(line, "season\tfile\tn\tlong\tshort")) return fail(path, 2, "the columns are not those of format 1");
    rows = calloc((size_t)want + 1, sizeof *rows);
    if (!rows) return fail(path, 0, "no memory");
    for (line = next + 1; *line; line = next + 1) {
        lineno++;
        next = end_line(line);
        if (!next) return fail(path, lineno, "the last line has no end");
        if (nrows == want) return fail(path, lineno, "more rows than line 1 says");
        char *c[5];
        if (!split(line, c, 5)) return fail(path, lineno, "a wrong number of columns");
        NameRow *r = &rows[nrows];
        r->season = number(c[0], SEASON_RULE_FIRST, SEASON_RULE_LAST);
        r->file = c[1];
        r->n = number(c[2], 0, 999);
        r->long_name = c[3];
        r->short_name = c[4];
        if (r->season < 0 || r->n < 0 || !*r->file || !*r->long_name || !*r->short_name ||
            strlen(r->long_name) > NAME_LIMIT || strlen(r->short_name) >= SHORT_SIZE)
            return fail(path, lineno, "a bad season, number or name");
        nrows++;
    }
    if (nrows != want) return fail(path, lineno, "fewer rows than line 1 says");
    return nrows;
}

/* A file of the season data folder: next to RECOMP_RULES_TABLE in test runs, else Seasons/ of the game. */
void season_data_path(const char *game_dir, const char *name, char *out, size_t size)
{
    const char *v = getenv("RECOMP_RULES_TABLE");
    if (v && *v) {
        const char *slash = strrchr(v, '/');
        int len = slash ? (int)(slash - v) : 0;
        if (slash) snprintf(out, size, "%.*s/%s", len, v, name);
        else snprintf(out, size, "%s", name);
    } else {
        snprintf(out, size, "%s/Seasons/%s", game_dir, name);
    }
}

int season_names_init(const char *game_dir)
{
    char path[1200];
    season_data_path(game_dir, "names.tsv", path, sizeof path);
    int n = season_names_load(path);
    if (n) fprintf(stderr, "recomp: names table: %d names from %s\n", n, path);
    else fprintf(stderr, "recomp: names table: none (%s)\n", path);
    return n;
}

const char *season_names_get_nth(const char *comp_file, int season, int n, const char **short_name)
{
    const NameRow *exact = NULL, *last = NULL;
    for (int i = 0; i < nrows; i++) {
        const NameRow *r = &rows[i];
        if (r->season != season || strcmp(r->file, comp_file)) continue;
        if (r->n == n) exact = r;
        if (!last || r->n > last->n) last = r;
    }
    /* A gap in the numbers (a row with no shown name) gives no name. An object after the last row takes the
     * name of the last row. */
    const NameRow *best = exact ? exact : last && n > last->n ? last : NULL;
    if (short_name) *short_name = best ? best->short_name : NULL;
    return best ? best->long_name : NULL;
}

static const char *current_name_of(const char *comp_file, int n);

const char *season_names_get_object(const char *comp_file, int season, int n)
{
    if (!season_game_changes() || season == SEASON_FIRST_YEAR) return NULL;
    const char *name = season_names_get_nth(comp_file, season, n, NULL);
    const char *now = name ? current_name_of(comp_file, n) : NULL;
    return name && now && !strncmp(name, now, LONG_SIZE) ? NULL : name;
}

const char *season_names_get(const char *comp_file, int season)
{
    return season_names_get_object(comp_file, season, 0);
}

int season_game_changes(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

static int readable(uint32_t p) { return rt_readable(p); }

int season_comp_objects(uint32_t *out, int max)
{
    int n = 0;
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int32_t slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    for (int32_t i = 0; readable(table) && i < slots && n < max; i++) {
        uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)i);
        if (readable(obj)) out[n++] = obj;
    }
    for (uint32_t e = 0; e < ENTRY_COUNT; e++) {
        uint32_t entry = ENTRIES + e * ENTRY_SIZE;
        int32_t count = (int32_t)rd32(g_mem, entry + 0xc);
        uint32_t arr = rd32(g_mem, entry + 0x10);
        for (int32_t k = 0; readable(arr) && k < count && k < 256 && n < max; k++) {
            uint32_t obj = rd32(g_mem, arr + 4u * (uint32_t)k);
            if (!readable(obj)) continue;
            int dup = 0;
            for (int s = 0; s < n && !dup; s++) dup = out[s] == obj;
            if (!dup) out[n++] = obj;
        }
    }
    return n;
}

static const SeasonCompClass *class_of(uint32_t vtable)
{
    int lo = 0, hi = season_comp_class_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (season_comp_classes[mid].vtable == vtable) return &season_comp_classes[mid];
        if (season_comp_classes[mid].vtable < vtable) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

/* The long name that the n-th competition object of a file has now (n from 0, counted as in
 * season_names_apply). NULL when the game has no such object. */
static const char *names_file(uint32_t obj);

static const char *current_name_of(const char *comp_file, int n)
{
    static uint32_t objs[MAX_OBJECTS];
    static char name[LONG_SIZE + 1];
    int count = season_comp_objects(objs, MAX_OBJECTS), k = 0;
    for (int i = 0; i < count; i++) {
        uint32_t rec = rd32(g_mem, objs[i] + 4);
        const char *file = names_file(objs[i]);
        if (!file || !readable(rec) || strcmp(file, comp_file)) continue;
        if (k++ != n) continue;
        memcpy(name, g_mem + rec + LONG_AT, LONG_SIZE);
        name[LONG_SIZE] = 0;
        return name;
    }
    return NULL;
}

static void put_name(uint32_t at, uint32_t size, const char *name)
{
    memset(g_mem + at, 0, size);
    memcpy(g_mem + at, name, strnlen(name, size - 1));
}

/* The one place that renames a record: the long name through season_change_rename (slice 16), which keeps
 * the name in the data for the key match at a load (0x60efd0) and writes the season part. The short name is
 * not matched at a load, so it goes in directly. */
static int rename_record(uint32_t rec, const char *long_name, const char *short_name)
{
    int changed = strncmp(win_str(rec + LONG_AT), long_name, LONG_SIZE) ||
                  strncmp(win_str(rec + SHORT_AT), short_name, SHORT_SIZE);
    if (!changed) return 0;
    if (strncmp(win_str(rec + LONG_AT), long_name, LONG_SIZE)) season_change_rename(rec, long_name);
    put_name(rec + SHORT_AT, SHORT_SIZE, short_name);
    return 1;
}

/* The file of an object's class and its number among the objects of that file (from 0), as
 * season_names_apply counts. 0 when the object is not one of the game's competition objects. */
/* The file of an object for the names table: a new competition that takes a class of the original has the file
 * of its def (season_comps_file: the Conference League is a Uefa_Cup object, not the second "uefa_cup"), else the
 * file of its class. NULL for an object of no known class. */
static const char *names_file(uint32_t obj)
{
    const char *f = season_comps_file(obj);
    const SeasonCompClass *c = f ? NULL : class_of(rd32(g_mem, obj));
    return f ? f : c ? c->file : NULL;
}

static int object_place(uint32_t obj, const char **file, int *n)
{
    static uint32_t objs[MAX_OBJECTS];
    const char *f = names_file(obj);
    if (!f) return 0;
    int count = season_comp_objects(objs, MAX_OBJECTS), k = 0;
    for (int i = 0; i < count; i++) {
        const char *fi = names_file(objs[i]);
        if (!fi || strcmp(fi, f)) continue;
        if (objs[i] == obj) {
            *file = f;
            *n = k;
            return 1;
        }
        k++;
    }
    return 0;
}

int season_names_rename_object(uint32_t obj, int season, char *old, size_t room)
{
    const char *file = NULL, *short_name = NULL;
    int n = 0;
    uint32_t rec = rd32(g_mem, obj + 4);
    if (!season_game_changes() || !readable(rec) || !object_place(obj, &file, &n)) return 0;
    const char *long_name = season_names_get_nth(file, season, n, &short_name);
    if (!long_name) return 0;
    if (old && room) snprintf(old, room, "%.*s", (int)LONG_SIZE - 1, win_str(rec + LONG_AT));
    return rename_record(rec, long_name, short_name);
}

int season_names_apply(int season)
{
    if (!season_game_changes() || !nrows) return 0;
    static uint32_t objs[MAX_OBJECTS];
    int n = season_comp_objects(objs, MAX_OBJECTS), changed = 0;
    /* The k-th object of a file takes the k-th name (rules_report_check.py counts the same way). */
    struct { const char *file; int count; } seen[512];
    int nseen = 0;
    for (int i = 0; i < n; i++) {
        uint32_t obj = objs[i], rec = rd32(g_mem, obj + 4);
        const char *file = names_file(obj);
        if (!file || !readable(rec)) continue;
        int k = 0, s;
        for (s = 0; s < nseen && strcmp(seen[s].file, file); s++) {}
        if (s < nseen) k = seen[s].count++;
        else if (nseen < 512) seen[nseen].file = file, seen[nseen++].count = 1;
        const char *short_name = NULL;
        const char *long_name = season_names_get_nth(file, season, k, &short_name);
        if (long_name) changed += rename_record(rec, long_name, short_name);
    }
    fprintf(stderr, "recomp: names of %d/%02d: %d competition records renamed\n", season, (season + 1) % 100,
            changed);
    return changed;
}

/* The season of the game date: a season starts on 1 July. */
static int season_of_date(void)
{
    int y = 0, m = 0, d = 0;
    if (!win_game_date(&y, &m, &d)) return season_start_year();
    int s = m >= 7 ? y : y - 1;
    return s < season_start_year() ? season_start_year() : s;
}

void o_008386f0(void);

/* build_comp_object_table 0x8386f0, cdecl, no arguments: one slot per club competition record (count
 * 0xae2378). A new game calls it at 0x829640, a load at 0x82c5f6 (return addresses 0x829645 and 0x82c5fb).
 * A load reads the season part before the original's load runs (load_game 0x82a9c0 in season.c), so here the
 * renames of the part are known. A save whose part holds names has them in its data already, so a load
 * changes no name, and nor does a part with news fields and no names; a part of version 1 (or a save made
 * before the names went into the part) takes the names of the season of the game date. */
RT_WRAP(008386f0, build_comp_table)
{
    int load = rd32(g_mem, cpu.esp) == 0x82c5fbu;
    /* Slice 26 first: at a load it makes each new record's object again before the original fills [0xadadfc]. */
    season_comps_before_comp_table();
    if (rt_unwinding) return;
    o_008386f0();
    if (rt_unwinding) return;
    if (!season_game_changes()) return;
    if (load && season_change_renames()) {
        fprintf(stderr, "recomp: names of the save: %d competition records keep the names of the season part\n",
                season_change_renames());
        return;
    }
    /* A part with news fields (version 2 on: its season point is above 0) and no renames: no name has changed
     * in this game, and the data of the save holds the names. The names of the season of the date would
     * rename a competition before its update day, and its "is now called" news would not come (load sweep). */
    if (load && season_change_point() > 0) {
        fprintf(stderr, "recomp: names of the save: no renames, the names of the data stay\n");
        return;
    }
    /* A new game: the date is 1 January of the start year here, so this gives the start season. */
    season_names_apply(season_changing_rules() ? season_of_date() : season_start_year());
}
