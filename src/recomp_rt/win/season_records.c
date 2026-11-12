/* Real club records for missing top and second tier clubs (season_records.h; doc
 * docs/research/2026-10-19-club-records.md, evidence docs/research/2026-10-19-club-table-users.md).
 *
 * The data load 0x528540 makes the club table one block of exactly (clubs + national teams) rows, and the national
 * teams must stay its last 2 x nations rows (0x53b3d0, 0x53b900). So the new clubs go into the files that the load
 * reads: club.dat gets them after its clubs, and every reference to a national team moves by their number. The
 * load, the id pass 0x5315a0, the key ids 0x601a40, the squad trim 0x541e60, the extra staff 0x527240 and both trims
 * then treat them as clubs of the files. Record layouts: include/database.h. All work is in host memory. */
#include "win.h"
#include "season.h"
#include "season_dates.h"
#include "season_names.h"
#include "season_records.h"
#include "seasons_db.h"
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FORMAT 1
#define MAX_ROWS 4096
#define MAX_BYTES (1u << 20)
#define MAX_FILES 32
#define CLUB_SIZE 0x245
#define STAFF_SIZE 0x6e
#define STAFF_FILE_SIZE 0x9d
#define SQUAD 50
#define FILL_TO 22                  /* the squad that the fill makes (Q509) */
#define MIN_SQUAD 16                /* the least squad that the fill must reach (Q509): 11 and 5 substitutes */
#define FILL_KEEPERS 2
#define KEEPER_FROM 15              /* goalkeeper rating (player row +0x0f, 1 to 20) of a goalkeeper */
#define RESERVE_PER_CLUB 16u        /* 0x527240: up to 5 coaches, 7 scouts, 3 physios and a manager */
#define EMPTY_YEAR 1900
#define REAL_CONTRACT_FROM 2009
#define CONTRACT_YEARS 6

/* Table ids of index.dat (+0x33) and their record sizes (include/database.h). The staff table is 0x6e bytes in
 * format 2 and 0x9d in format 1 (the 3.9.68 data). */
enum { T_CLUB = 0, T_NAT_CLUB = 1, T_COLOUR = 2, T_NATION = 4, T_STADIUM = 5, T_STAFF = 6, T_NON_PLAYER = 9,
       T_PLAYER = 10, T_CLUB_COMP = 12, T_FIRST = 13, T_SECOND = 14, T_COMMON = 15, T_HISTORY = 17,
       T_COMP_HISTORY = 19, T_NATION_HISTORY = 20, T_CITY = 21, T_PREFS = 22, T_IDS = 23 };
static const int rec_size[T_IDS] = {0x245, 0x245, 0x3a, 0xc6, 0x122, 0x4e, 0x6e, 0x2b, 0, 0x44, 0x46, 0x65, 0x6b,
                                    0x3c, 0x3c, 0x3c, 0x6b, 0x11, 0x3a, 0x1a, 0x1a, 0x38, 0x34};

/* --- The table of Seasons/club_records.tsv --- */

typedef struct {
    int season, capacity, source_club;
    const char *division, *real, *name, *shrt, *city, *stadium, *colour1, *colour2, *source;
} RecordRow;

static char *text;
static RecordRow *rows;
static int nrows;

static int fail_table(const char *path, int line, const char *why)
{
    fprintf(stderr, "recomp: club records table %s, line %d: %s; no club records\n", path, line, why);
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
    if (!strcmp(s, "-1")) return lo <= -1 ? -1 : -2;
    if (!*s) return -2;
    long v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return -2;
        v = v * 10 + (*p - '0');
        if (v > hi) return -2;
    }
    return v < lo ? -2 : (int)v;
}

int season_records_load(const char *path)
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
    if (size < 0 || size > (long)MAX_BYTES) { fclose(f); return fail_table(path, 0, "the file is too large"); }
    text = malloc((size_t)size + 1);
    if (!text) { fclose(f); return fail_table(path, 0, "no memory"); }
    size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) return fail_table(path, 0, "a read failed");
    text[size] = 0;
    char *line = text, *next = end_line(line);
    int want = -1, version = 0, lineno = 2;
    if (!next || sscanf(line, "#bosman-club-records\t%d\t%d", &version, &want) != 2 || version != FORMAT ||
        want < 0 || want > MAX_ROWS)
        return fail_table(path, 1, "not a club records table of format 1");
    line = next + 1;
    next = end_line(line);
    if (!next || strcmp(line, "season\tdivision\treal\tname\tshort\tcity\tstadium\tcapacity\tcolour1\tcolour2\t"
                              "source\tsource_club"))
        return fail_table(path, 2, "the columns are not those of format 1");
    rows = calloc((size_t)want + 1, sizeof *rows);
    if (!rows) return fail_table(path, 0, "no memory");
    for (line = next + 1; *line; line = next + 1) {
        lineno++;
        next = end_line(line);
        if (!next) return fail_table(path, lineno, "the last line has no end");
        if (nrows == want) return fail_table(path, lineno, "more rows than line 1 says");
        char *c[12];
        if (!split(line, c, 12)) return fail_table(path, lineno, "a wrong number of columns");
        RecordRow *r = &rows[nrows];
        r->season = number(c[0], 2002, SEASON_LAST_YEAR);
        r->division = c[1];
        r->real = c[2];
        r->name = c[3];
        r->shrt = c[4];
        r->city = c[5];
        r->stadium = c[6];
        r->capacity = number(c[7], 0, 1000000);
        r->colour1 = c[8];
        r->colour2 = c[9];
        r->source = c[10];
        r->source_club = number(c[11], -1, 10000000);
        if (r->season < 0 || r->capacity < 0 || r->source_club < -1 || !*r->division || !*r->real || !*r->name ||
            !*r->shrt || strlen(r->name) > 50 || strlen(r->shrt) > 25 || strlen(r->division) > 50 ||
            strlen(r->stadium) > 50 || strlen(r->city) > 25)
            return fail_table(path, lineno, "a bad season, number or name");
        nrows++;
    }
    if (nrows != want) return fail_table(path, lineno, "fewer rows than line 1 says");
    return nrows;
}

int season_records_init(const char *game_dir)
{
    char path[1200];
    season_data_path(game_dir, "club_records.tsv", path, sizeof path);
    int n = season_records_load(path);
    if (n) fprintf(stderr, "recomp: club records table: %d clubs from %s\n", n, path);
    else fprintf(stderr, "recomp: club records table: none (%s)\n", path);
    return n;
}

/* --- A database in host memory --- */

typedef struct {
    char name[64];                  /* the file name as the folder holds it */
    uint8_t *data;
    size_t size;
    int changed;
} DbFile;

typedef struct {
    int table, file;                /* table id, index of its file in files[] */
    int32_t count, off, format;
    int pos;                        /* offset of the entry in index.dat */
} DbEntry;

typedef struct {
    char dir[1100];
    uint8_t *index;
    size_t index_size;
    DbFile files[MAX_FILES];
    int nfiles;
    DbEntry e[T_IDS];               /* by table id; table -1 when absent */
    int staff_size;                 /* 0x6e or 0x9d */
    /* Rows added after the rows of each table. */
    uint8_t *add[T_IDS];
    uint32_t add_n[T_IDS], add_cap[T_IDS];
} Db;

static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int32_t gets32(const uint8_t *p) { return (int32_t)get32(p); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static int get16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static uint8_t *read_all(const char *dir, const char *name, size_t *size, char *found, size_t room)
{
    char path[1300];
    HostDir *d = host_opendir(dir);
    if (!d) return NULL;
    const char *e;
    found[0] = 0;
    while ((e = host_readdir(d)))
        if (!strcasecmp(e, name)) { snprintf(found, room, "%s", e); break; }
    host_closedir(d);
    if (!found[0]) return NULL;
    snprintf(path, sizeof path, "%s/%s", dir, found);
    FILE *f = host_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = n >= 0 ? malloc((size_t)n + 1) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    *size = buf ? (size_t)n : 0;
    return buf;
}

static void db_free(Db *db)
{
    free(db->index);
    for (int i = 0; i < db->nfiles; i++) free(db->files[i].data);
    for (int t = 0; t < T_IDS; t++) free(db->add[t]);
    memset(db, 0, sizeof *db);
}

/* Reads index.dat and every file it names. 0 with a log line when a table runs past its file. */
static int db_read(Db *db, const char *dir, char *log, size_t room)
{
    memset(db, 0, sizeof *db);
    snprintf(db->dir, sizeof db->dir, "%s", dir);
    for (int t = 0; t < T_IDS; t++) db->e[t].table = -1;
    char found[256];
    db->index = read_all(dir, "index.dat", &db->index_size, found, sizeof found);
    if (!db->index || db->index_size < 8) { snprintf(log, room, "no index.dat"); return 0; }
    for (size_t at = 8; at + 0x43 <= db->index_size; at += 0x43) {
        char fname[52];
        memcpy(fname, db->index + at, 51);
        fname[51] = 0;
        int t = gets32(db->index + at + 0x33);
        if (t < 0 || t >= T_IDS || t == 8) continue;
        int fi = -1;
        for (int i = 0; i < db->nfiles; i++)
            if (!strcasecmp(db->files[i].name, fname)) fi = i;
        if (fi < 0) {
            if (db->nfiles == MAX_FILES) { snprintf(log, room, "too many files"); return 0; }
            DbFile *f = &db->files[db->nfiles];
            f->data = read_all(dir, fname, &f->size, found, sizeof found);
            if (!f->data) { snprintf(log, room, "no %s", fname); return 0; }
            snprintf(f->name, sizeof f->name, "%s", found);
            fi = db->nfiles++;
        }
        DbEntry *e = &db->e[t];
        e->table = t;
        e->file = fi;
        e->count = gets32(db->index + at + 0x37);
        e->off = gets32(db->index + at + 0x3b);
        e->format = gets32(db->index + at + 0x3f);
        e->pos = (int)at;
    }
    if (db->e[T_STAFF].table < 0 || db->e[T_CLUB].table < 0 || db->e[T_NAT_CLUB].table < 0) {
        snprintf(log, room, "no staff or club table");
        return 0;
    }
    db->staff_size = db->e[T_STAFF].format == 1 ? STAFF_FILE_SIZE : STAFF_SIZE;
    for (int t = 0; t < T_IDS; t++) {
        DbEntry *e = &db->e[t];
        if (e->table < 0) continue;
        size_t rs = t == T_STAFF ? (size_t)db->staff_size : (size_t)rec_size[t];
        if (e->count < 0 || e->off < 0 || (size_t)e->off + (size_t)e->count * rs > db->files[e->file].size) {
            snprintf(log, room, "table %d runs past the end of %s", t, db->files[e->file].name);
            return 0;
        }
    }
    return 1;
}

static int rsize(const Db *db, int t) { return t == T_STAFF ? db->staff_size : rec_size[t]; }
static int count(const Db *db, int t) { return db->e[t].table < 0 ? 0 : db->e[t].count; }
static uint8_t *row(Db *db, int t, int i)
{
    return db->files[db->e[t].file].data + db->e[t].off + (size_t)i * (size_t)rsize(db, t);
}
static void touch(Db *db, int t) { db->files[db->e[t].file].changed = 1; }

/* A new row of table t (zeroed), after the rows of the file and the rows added before. */
static uint8_t *add_row(Db *db, int t)
{
    size_t rs = (size_t)rsize(db, t);
    if (db->add_n[t] == db->add_cap[t]) {
        uint32_t cap = db->add_cap[t] ? db->add_cap[t] * 2 : 64;
        uint8_t *p = realloc(db->add[t], cap * rs);
        if (!p) rt_fatal("club records: no memory");
        db->add[t] = p;
        db->add_cap[t] = cap;
    }
    uint8_t *r = db->add[t] + db->add_n[t]++ * rs;
    memset(r, 0, rs);
    touch(db, t);
    return r;
}
static int next_id(const Db *db, int t) { return count(db, t) + (int)db->add_n[t]; }

/* Text of a name field, as bytes up to the end or n. */
static void field(const uint8_t *p, int n, char *out)
{
    int k = 0;
    while (k < n && p[k]) { out[k] = (char)p[k]; k++; }
    out[k] = 0;
}

/* --- Lookups --- */

static int find_by_name(Db *db, int t, int off, int len, const char *name)
{
    char buf[128];
    for (int i = 0; i < count(db, t); i++) {
        field(row(db, t, i) + off, len, buf);
        if (!strcmp(buf, name)) return i;
    }
    return -1;
}

/* A name table as a hash of its text: row by text. */
typedef struct { uint32_t *slot; uint32_t mask; } NameHash;

static uint32_t hash_text(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

static void hash_names(Db *db, int t, NameHash *h)
{
    uint32_t n = (uint32_t)count(db, t) + 4096, cap = 1;
    while (cap < n * 2) cap <<= 1;
    h->slot = malloc(cap * sizeof *h->slot);
    if (!h->slot) rt_fatal("club records: no memory");
    memset(h->slot, 0xff, cap * sizeof *h->slot);
    h->mask = cap - 1;
    char buf[64];
    for (int i = count(db, t) - 1; i >= 0; i--) {   /* the first row of a text wins */
        field(row(db, t, i), 51, buf);
        uint32_t k = hash_text(buf) & h->mask;
        while (h->slot[k] != 0xffffffffu) {
            char other[64];
            field(row(db, t, (int)h->slot[k]), 51, other);
            if (!strcmp(other, buf)) break;
            k = (k + 1) & h->mask;
        }
        h->slot[k] = (uint32_t)i;
    }
}

/* The id of the name row with this text, a new row of the table when it has none. */
static int name_id(Db *db, int t, NameHash *h, const char *text, int nation_id)
{
    if (!*text) return -1;
    uint32_t k = hash_text(text) & h->mask;
    char buf[64];
    while (h->slot[k] != 0xffffffffu) {
        uint32_t i = h->slot[k];
        const uint8_t *r = i < (uint32_t)count(db, t) ? row(db, t, (int)i) : db->add[t] + (i - (uint32_t)count(db, t)) * 0x3c;
        field(r, 51, buf);
        if (!strcmp(buf, text)) return gets32(r + 0x33);
        k = (k + 1) & h->mask;
    }
    uint32_t used = (uint32_t)count(db, t) + db->add_n[t];
    if (used * 2 > h->mask) rt_fatal("club records: name hash full");
    int id = next_id(db, t);
    uint8_t *r = add_row(db, t);
    memcpy(r, text, strlen(text) < 50 ? strlen(text) : 50);
    put32(r + 0x33, (uint32_t)id);
    put32(r + 0x37, (uint32_t)nation_id);
    r[0x3b] = 1;
    h->slot[k] = used;
    return id;
}

/* The colour record of a research colour word. */
static int colour_id(Db *db, const char *word)
{
    static const char *const map[][2] = {
        {"Red", "Red 1"}, {"Maroon", "Red 3"}, {"Navy Blue", "Blue 2"}, {"Blue", "Blue 4"}, {"Sky Blue", "Blue 5"},
        {"Green", "Green 1"}, {"Yellow", "Yellow 1"}, {"Orange", "Orange 1"}, {"Purple", "Purple 1"},
        {"Pink", "Pink"}, {"Gold", "Gold"}, {"Black", "Black"}, {"White", "White"}};
    for (size_t i = 0; i < sizeof map / sizeof *map; i++)
        if (!strcasecmp(word, map[i][0])) {
            int c = find_by_name(db, T_COLOUR, 4, 51, map[i][1]);
            if (c >= 0) return gets32(row(db, T_COLOUR, c));
        }
    int w = find_by_name(db, T_COLOUR, 4, 51, "White");
    return w >= 0 ? gets32(row(db, T_COLOUR, w)) : -1;
}

/* --- Dates --- */

static int is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

/* The real year of a date of the source database, and the year that the merged file holds for it. */
static int contract_real_year(int y, const SeasonShiftPlan *p)
{
    return y + (y >= p->D && y >= REAL_CONTRACT_FROM ? p->contract_real : p->contract_old);
}

/* The file year that the start database's shift (shift_contract of season_dates.c) makes into the real year Y. */
static int contract_file_year(int Y, const SeasonShiftPlan *p)
{
    int a = Y - p->contract_real;
    if (a >= p->D && a >= REAL_CONTRACT_FROM) return a;
    int b = Y - p->contract_old;
    if (!(b >= p->D && b >= REAL_CONTRACT_FROM)) return b;
    return a;
}

/* --- The merge --- */

typedef struct {
    int added;                      /* new clubs */
    int players, skipped, names, cities, careers, held;
    int club_id[MAX_ROWS];          /* the club id of row i of the season, -1 when none */
    int row_of[MAX_ROWS];           /* index into rows[] */
    int n;
} MergeResult;

static Db merged;
static int merged_ok;
static MergeResult result;
static int result_season;
static uint8_t *out_data[MAX_FILES];
static size_t out_size[MAX_FILES];
static uint8_t *out_index;
static size_t out_index_size;

void season_records_free(void)
{
    for (int i = 0; i < MAX_FILES; i++) { free(out_data[i]); out_data[i] = NULL; out_size[i] = 0; }
    free(out_index);
    out_index = NULL;
    db_free(&merged);
    merged_ok = 0;
}

/* Shifts every reference to a national team (an id from c to c + nat - 1) by k. The fields are those that the id
 * pass resolves and that 0x5267c0 moves (table users doc, sections 1 and 3). */
static void shift_national(Db *db, int c, int nat, int k)
{
#define MOVE(p) do { int32_t v_ = gets32(p); if (v_ >= c && v_ < c + nat) put32((p), (uint32_t)(v_ + k)); } while (0)
    for (int t = T_CLUB; t <= T_NAT_CLUB; t++) {
        for (int i = 0; i < count(db, t); i++) {
            uint8_t *r = row(db, t, i);
            if (t == T_NAT_CLUB) MOVE(r);
            MOVE(r + 0xb3);
            MOVE(r + 0xb7);
            MOVE(r + 0xbb);
        }
        touch(db, t);
    }
    for (int i = 0; i < count(db, T_STAFF); i++) {
        uint8_t *r = row(db, T_STAFF, i);
        MOVE(r + 0x24);
        MOVE(r + 0x39);
        if (db->staff_size == STAFF_FILE_SIZE)
            for (int j = 0; j < 6; j++) MOVE(r + 0x61 + 4 * j);     /* the preference clubs of the file row */
    }
    touch(db, T_STAFF);
    for (int i = 0; i < count(db, T_PREFS); i++)
        for (int j = 0; j < 6; j++) MOVE(row(db, T_PREFS, i) + 4 + 4 * j);
    if (count(db, T_PREFS)) touch(db, T_PREFS);
    for (int i = 0; i < count(db, T_HISTORY); i++) MOVE(row(db, T_HISTORY, i) + 0x0a);
    if (count(db, T_HISTORY)) touch(db, T_HISTORY);
    for (int t = T_COMP_HISTORY; t <= T_NATION_HISTORY; t++) {
        for (int i = 0; i < count(db, t); i++)
            for (int j = 0; j < 4; j++) MOVE(row(db, t, i) + 0x0a + 4 * j);
        if (count(db, t)) touch(db, t);
    }
#undef MOVE
}

static int cmp_int(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return (x > y) - (x < y); }

/* The person key of a staff row: the texts of its names and its real date of birth. */
static uint64_t person_key(Db *db, const uint8_t *s, int birth_shift)
{
    uint64_t h = 1469598103934665603ull;
    for (int k = 0; k < 3; k++) {
        int t = T_FIRST + k, id = gets32(s + 4 + 4 * k);
        char buf[64] = "";
        if (id >= 0 && id < count(db, t)) field(row(db, t, id), 51, buf);
        for (const char *p = buf; *p; p++) h = (h ^ (uint8_t)*p) * 1099511628211ull;
        h = (h ^ 0x1f) * 1099511628211ull;
    }
    int y = get16(s + 0x12), d = get16(s + 0x10);
    if (y > EMPTY_YEAR) y += birth_shift;
    h = (h ^ (uint64_t)(uint16_t)y) * 1099511628211ull;
    h = (h ^ (uint64_t)(uint16_t)d) * 1099511628211ull;
    return h;
}

/* A looser key (review 2): the texts of the names, the nation (an id of the start database) and the real birth
 * year, so a person whose day of birth or day shift differs between two databases is still known. */
static uint64_t person_key2(Db *db, const uint8_t *s, int nation_id, int year)
{
    uint64_t h = 1469598103934665603ull;
    for (int k = 0; k < 3; k++) {
        int t = T_FIRST + k, id = gets32(s + 4 + 4 * k);
        char buf[64] = "";
        if (id >= 0 && id < count(db, t)) field(row(db, t, id), 51, buf);
        for (const char *p = buf; *p; p++) h = (h ^ (uint8_t)*p) * 1099511628211ull;
        h = (h ^ 0x1f) * 1099511628211ull;
    }
    h = (h ^ (uint64_t)(uint32_t)nation_id) * 1099511628211ull;
    h = (h ^ (uint64_t)(uint16_t)year) * 1099511628211ull;
    return h;
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* The squad of one new club: the players of the source club, less the people whom the start database holds. */
static void add_squad(Db *t, uint8_t *club, int club_id, Db *src, int src_club, const SeasonShiftPlan *ps,
                      const SeasonShiftPlan *pt, const uint64_t *people, size_t npeople, const uint64_t *people2,
                      NameHash names[3], int season, MergeResult *res)
{
    int slot = 0;
    for (int i = 0; i < count(src, T_STAFF) && slot < SQUAD; i++) {
        const uint8_t *s = row(src, T_STAFF, i);
        int player = gets32(s + 0x61);
        if (gets32(s + 0x39) != src_club || player < 0 || player >= count(src, T_PLAYER)) continue;
        /* The nation by name. */
        int nat[2] = {-1, -1};
        for (int k = 0; k < 2; k++) {
            int id = gets32(s + 0x1a + 4 * k);
            if (id < 0 || id >= count(src, T_NATION)) continue;
            char nm[64];
            field(row(src, T_NATION, id) + 4, 51, nm);
            int j = find_by_name(t, T_NATION, 4, 51, nm);
            nat[k] = j >= 0 ? gets32(row(t, T_NATION, j)) : -1;
        }
        uint64_t key = person_key(src, s, ps->birth);
        int held = bsearch(&key, people, npeople, sizeof *people, cmp_u64) != NULL;
        int year = get16(s + 0x12) > EMPTY_YEAR ? get16(s + 0x12) + ps->birth : 0;
        for (int dy = -1; dy <= 1 && !held && year; dy++) {
            uint64_t k2 = person_key2(src, s, nat[0], year + dy);
            held = bsearch(&k2, people2, npeople, sizeof *people2, cmp_u64) != NULL;
        }
        if (held) { res->skipped++; continue; }
        int staff_id = next_id(t, T_STAFF), player_id = next_id(t, T_PLAYER);
        uint8_t *n = add_row(t, T_STAFF);
        memcpy(n, s, STAFF_SIZE);               /* the first 0x61 bytes are the same in both formats */
        put32(n, (uint32_t)staff_id);
        for (int k = 0; k < 3; k++) {
            int id = gets32(s + 4 + 4 * k);
            char nm[64] = "";
            if (id >= 0 && id < count(src, T_FIRST + k)) field(row(src, T_FIRST + k, id) + 0, 51, nm);
            int nid = *nm ? name_id(t, T_FIRST + k, &names[k], nm, nat[0]) : id;
            put32(n + 4 + 4 * k, (uint32_t)nid);
            if (*nm && nid >= count(t, T_FIRST + k)) res->names++;
        }
        put32(n + 0x1a, (uint32_t)nat[0]);
        put32(n + 0x1e, (uint32_t)nat[1]);
        put32(n + 0x24, 0xffffffffu);           /* no national team */
        n[0x28] = 0;
        memset(n + 0x29, 0, 16);                /* no national job dates */
        put32(n + 0x39, (uint32_t)club_id);
        if (n[0x3d] == 12 || n[0x3d] == 13 || n[0x3d] == 15) n[0x3d] = 11;
        /* Dates: real by the source's shift, then less the start database's shift (season_dates.c adds it). */
        int by = get16(s + 0x12);
        if (by > EMPTY_YEAR) {
            int real = by + ps->birth, day = get16(s + 0x10);
            if (get32(s + 0x14) && !is_leap(real)) {     /* a day of the leap table in a common year */
                if (day >= 59) day--;
                put16(n + 0x10, day);
                put32(n + 0x14, 0);
            }
            put16(n + 0x12, real - pt->birth);
        }
        put16(n + 0x3e, 181);                   /* joined: 1 July of the start year */
        put16(n + 0x40, season - pt->joined);
        put32(n + 0x42, 0);
        int cy = get16(s + 0x48);
        int real_end = cy > EMPTY_YEAR ? contract_real_year(cy, ps) - ps->N + season : season + 1;
        if (real_end <= season) real_end = season + 1;
        if (real_end > season + CONTRACT_YEARS) real_end = season + CONTRACT_YEARS;
        put16(n + 0x46, 180);                   /* 30 June */
        put16(n + 0x48, contract_file_year(real_end, pt));
        put32(n + 0x4a, 0);
        n[0x5f] = 2;                            /* player data only */
        if (t->staff_size == STAFF_FILE_SIZE) {
            memset(n + 0x61, 0xff, 12 * 4);     /* no preferences */
            put32(n + 0x91, (uint32_t)player_id);
            put32(n + 0x95, 0xffffffffu);
            put32(n + 0x99, 0xffffffffu);
        } else {
            put32(n + 0x61, (uint32_t)player_id);
            put32(n + 0x65, (uint32_t)staff_id);
            put32(n + 0x69, 0xffffffffu);
            uint8_t *pr = add_row(t, T_PREFS);
            memset(pr, 0xff, 0x34);
            put32(pr, (uint32_t)staff_id);
        }
        uint8_t *pl = add_row(t, T_PLAYER);
        memcpy(pl, row(src, T_PLAYER, player), 0x46);
        put32(pl, (uint32_t)player_id);
        put32(club + 0xd7 + 4 * slot++, (uint32_t)staff_id);
        res->players++;
        /* The careers before the start season, the clubs by name and nation. */
        int staff_src = gets32(s);
        for (int h = 0; h < count(src, T_HISTORY); h++) {
            const uint8_t *hr = row(src, T_HISTORY, h);
            if (gets32(hr + 4) != staff_src) continue;
            int y = get16(hr + 8);
            int real = y > EMPTY_YEAR ? y + (season_dates_career_kept(ps->row, (uint32_t)i) ? ps->kept : ps->careers) : y;
            if (real >= season) continue;
            int cl = gets32(hr + 0x0a), to = -1;
            if (cl == src_club) to = club_id;
            else if (cl >= 0 && cl < count(src, T_CLUB)) {
                const uint8_t *c = row(src, T_CLUB, cl);
                char nm[64], natn[64] = "";
                field(c + 4, 51, nm);
                int cn = gets32(c + 0x53);
                if (cn >= 0 && cn < count(src, T_NATION)) field(row(src, T_NATION, cn) + 4, 51, natn);
                for (int j = 0; j < count(t, T_CLUB) && to < 0; j++) {
                    const uint8_t *tc = row(t, T_CLUB, j);
                    char tn[64];
                    field(tc + 4, 51, tn);
                    if (strcmp(tn, nm)) continue;
                    int tnat = gets32(tc + 0x53);
                    char tnn[64] = "";
                    if (tnat >= 0 && tnat < count(t, T_NATION)) field(row(t, T_NATION, tnat) + 4, 51, tnn);
                    if (!strcmp(tnn, natn)) to = gets32(tc);
                }
            }
            if (to < 0) continue;
            uint8_t *nh = add_row(t, T_HISTORY);
            memcpy(nh, hr, 0x11);
            put32(nh, (uint32_t)next_id(t, T_HISTORY) - 1);
            put32(nh + 4, (uint32_t)staff_id);
            put16(nh + 8, real > EMPTY_YEAR ? real - pt->careers : y);
            put32(nh + 0x0a, (uint32_t)to);
            res->careers++;
        }
    }
}

/* The median of n values (the lower middle value). */
static int median(int *v, int n)
{
    if (n <= 0) return 0;
    qsort(v, (size_t)n, sizeof *v, cmp_int);
    return v[(n - 1) / 2];
}

/* Writes each changed file again: its tables in the order of their offsets, each with its added rows, then any
 * bytes that followed the last table. */
static int rebuild(Db *db, char *log, size_t room)
{
    out_index = malloc(db->index_size);
    if (!out_index) return 0;
    memcpy(out_index, db->index, db->index_size);
    out_index_size = db->index_size;
    for (int f = 0; f < db->nfiles; f++) {
        if (!db->files[f].changed) continue;
        int order[T_IDS], n = 0;
        for (int t = 0; t < T_IDS; t++)
            if (db->e[t].table >= 0 && db->e[t].file == f) order[n++] = t;
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && db->e[order[j]].off < db->e[order[j - 1]].off; j--) {
                int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
            }
        /* The tables must cover the file from 0 to its end, one after the other. */
        size_t at = 0, total = 0;
        for (int i = 0; i < n; i++) {
            DbEntry *e = &db->e[order[i]];
            if ((size_t)e->off != at) { snprintf(log, room, "%s has bytes outside its tables", db->files[f].name); return 0; }
            at += (size_t)e->count * (size_t)rsize(db, order[i]);
            total += ((size_t)e->count + db->add_n[order[i]]) * (size_t)rsize(db, order[i]);
        }
        size_t tail = db->files[f].size - at;    /* bytes after the last table stay after it (September 2010 ODB:
                                                    164 bytes after the preferences of staff.dat) */
        total += tail;
        uint8_t *out = malloc(total ? total : 1);
        if (!out) return 0;
        size_t put = 0;
        for (int i = 0; i < n; i++) {
            int t = order[i];
            DbEntry *e = &db->e[t];
            size_t rs = (size_t)rsize(db, t), old = (size_t)e->count * rs;
            memcpy(out + put, db->files[f].data + e->off, old);
            memcpy(out + put + old, db->add[t], db->add_n[t] * rs);
            put32(out_index + e->pos + 0x37, (uint32_t)(e->count + (int)db->add_n[t]));
            put32(out_index + e->pos + 0x3b, (uint32_t)put);
            put += old + db->add_n[t] * rs;
        }
        memcpy(out + put, db->files[f].data + at, tail);
        out_data[f] = out;
        out_size[f] = total;
    }
    return 1;
}

typedef struct { char name[64]; Db db; SeasonShiftPlan plan; int ok; } Source;

int season_records_merge(const char *dir, int season, SeasonRecordsFind find_source, char *log, size_t room)
{
    season_records_free();
    memset(&result, 0, sizeof result);
    result_season = season;
    log[0] = 0;
    int first = -1, n = 0;
    for (int i = 0; i < nrows; i++)
        if (rows[i].season == season) {
            if (first < 0) first = i;
            result.row_of[n] = i;
            result.club_id[n] = -1;
            n++;
        }
    result.n = n;
    if (!n) return 0;
    Db *t = &merged;
    char folder[1100];
    if (dir[0]) snprintf(folder, sizeof folder, "%s", dir);
    else snprintf(folder, sizeof folder, "%s/Data", win_game_dir);
    if (!db_read(t, folder, log, room)) { db_free(t); return -1; }
    SeasonShiftPlan pt;
    season_dates_plan(dir, season, &pt);
    int c = count(t, T_CLUB), nat = count(t, T_NAT_CLUB);
    /* The national teams follow the new clubs: their ids and every reference to them in the rows of the files move
     * by the number of new clubs, before any row names a new club (whose ids c .. c + n - 1 were national team ids).
     * A row whose division this database lacks makes no club, so n counts the rows whose division it holds. */
    int will = 0;
    uint8_t skip[MAX_ROWS] = {0};
    for (int r = 0; r < n; r++) {
        const RecordRow *rr = &rows[result.row_of[r]];
        int d = find_by_name(t, T_CLUB_COMP, 4, 51, rr->division);
        if (d < 0) { skip[r] = 1; continue; }
        /* Review 1: another database of the year or a Custom data set can hold the club already: a club of the
         * division's nation with the record's name or its name in the club moves table makes no second one. */
        int nation_id = gets32(row(t, T_CLUB_COMP, d) + 0x5d);
        for (int i = 0; i < c && !skip[r]; i++) {
            const uint8_t *cl = row(t, T_CLUB, i);
            char nm[64];
            field(cl + 4, 51, nm);
            if (gets32(cl + 0x53) == nation_id && (!strcmp(nm, rr->name) || !strcmp(nm, rr->real))) {
                skip[r] = 1;
                result.held++;
                fprintf(stderr, "recomp: club records: %s: this database holds the club (record %d), no new record\n",
                        rr->name, gets32(cl));
            }
        }
        will += !skip[r];
    }
    if (will) shift_national(t, c, nat, will);
    NameHash names[3];
    for (int k = 0; k < 3; k++) hash_names(t, T_FIRST + k, &names[k]);
    /* The people of the start database, by names and real date of birth. */
    size_t npeople = (size_t)count(t, T_STAFF);
    uint64_t *people = malloc((npeople + 1) * sizeof *people);
    if (!people) rt_fatal("club records: no memory");
    uint64_t *people2 = malloc((npeople + 1) * sizeof *people2);
    if (!people2) rt_fatal("club records: no memory");
    for (size_t i = 0; i < npeople; i++) {
        const uint8_t *s = row(t, T_STAFF, (int)i);
        people[i] = person_key(t, s, pt.birth);
        int y = get16(s + 0x12) > EMPTY_YEAR ? get16(s + 0x12) + pt.birth : 0;
        people2[i] = person_key2(t, s, gets32(s + 0x1a), y);
    }
    qsort(people, npeople, sizeof *people, cmp_u64);
    qsort(people2, npeople, sizeof *people2, cmp_u64);

    Source *src = calloc(16, sizeof *src);
    int nsrc = 0;
    if (!src) rt_fatal("club records: no memory");
    int k = 0;                              /* new clubs so far */
    for (int r = 0; r < n; r++) {
        const RecordRow *rr = &rows[result.row_of[r]];
        int d = find_by_name(t, T_CLUB_COMP, 4, 51, rr->division);
        if (d < 0) fprintf(stderr, "recomp: club records: %s: no division %s in this database\n", rr->name, rr->division);
        if (skip[r]) continue;
        int div_id = gets32(row(t, T_CLUB_COMP, d)), nation_id = gets32(row(t, T_CLUB_COMP, d) + 0x5d);
        /* The template: the club of the division with the median reputation (Q362, Q506). */
        int reps[1024], nr = 0, template = -1, caps[1024], nc = 0;
        for (int i = 0; i < c && nr < 1024; i++)
            if (gets32(row(t, T_CLUB, i) + 0x57) == div_id) reps[nr++] = get16(row(t, T_CLUB, i) + 0x80);
        int rep = median(reps, nr);
        for (int i = 0; i < c && template < 0; i++)
            if (gets32(row(t, T_CLUB, i) + 0x57) == div_id && get16(row(t, T_CLUB, i) + 0x80) == rep) template = i;
        if (template < 0)
            for (int i = 0; i < c && template < 0; i++)
                if (gets32(row(t, T_CLUB, i) + 0x53) == nation_id) template = i;
        if (template < 0) template = 0;     /* never for a known database: a division with no club of its nation */
        for (int i = 0; i < c && nc < 1024; i++) {
            const uint8_t *cl = row(t, T_CLUB, i);
            int sid = gets32(cl + 0x69);
            if (gets32(cl + 0x57) == div_id && sid >= 0 && sid < count(t, T_STADIUM))
                caps[nc++] = gets32(row(t, T_STADIUM, sid) + 0x3c);
        }
        int club_id = c + k;
        uint8_t *cl = add_row(t, T_CLUB);
        memcpy(cl, row(t, T_CLUB, template), CLUB_SIZE);
        put32(cl, (uint32_t)club_id);
        memset(cl + 4, 0, 51);
        memcpy(cl + 4, rr->name, strlen(rr->name));
        memset(cl + 0x38, 0, 26);
        memcpy(cl + 0x38, rr->shrt, strlen(rr->shrt));
        put32(cl + 0x53, (uint32_t)nation_id);
        put32(cl + 0x57, 0xffffffffu);          /* slice 15's club moves give the division */
        put32(cl + 0x5b, (uint32_t)div_id);
        /* Review 2: +0x5f is the place in the last division (data: Chelsea 1, Arsenal 3, Fulham 12 in September 2010
         * ODB); UefaSeeding::collect_european_clubs 0x904040 reads it (0x904185) and the AFC and league cup picks
         * go by it. The record finished after the last club of the division, so no place-based pick takes it. */
        cl[0x5f] = (uint8_t)(nr + 1 < 255 ? nr + 1 : 255);
        put16(cl + 0x80, rep > 0 ? rep : 1);
        /* Review 1: a short name that another club of the nation has ("Krasnodar" and FC Krasnodar-2000) becomes
         * the long name cut to 25 characters, so tables tell the two apart. */
        for (int i = 0; i < c; i++) {
            const uint8_t *o = row(t, T_CLUB, i);
            char sh[32];
            field(o + 0x38, 26, sh);
            if (gets32(o + 0x53) == nation_id && !strcmp(sh, rr->shrt)) {
                memset(cl + 0x38, 0, 26);
                memcpy(cl + 0x38, rr->name, strlen(rr->name) < 25 ? strlen(rr->name) : 25);
                fprintf(stderr, "recomp: club records: %s: short name %s is that of record %d, the long name is used\n",
                        rr->name, rr->shrt, gets32(o));
                break;
            }
        }
        int c1 = colour_id(t, rr->colour1), c2 = colour_id(t, rr->colour2);
        int colours[6] = {c2, c1, c1, c2, -1, -1};
        for (int j = 0; j < 6; j++) put32(cl + 0x83 + 4 * j, (uint32_t)colours[j]);
        for (int j = 0x9b; j <= 0xd3; j += 4) put32(cl + j, 0xffffffffu);   /* staff, rivals, chairman .. assistant */
        for (int j = 0xd7; j < 0x1db; j += 4) put32(cl + j, 0xffffffffu);   /* squad, coaches, scouts, physios */
        for (int j = 0x1e0; j < 0x230; j += 4) put32(cl + j, 0xffffffffu);  /* team */
        cl[0x244] = 0;
        /* The stadium and its city. */
        int tsid = gets32(row(t, T_CLUB, template) + 0x69);
        int stadium_id = next_id(t, T_STADIUM);
        uint8_t *st = add_row(t, T_STADIUM);
        if (tsid >= 0 && tsid < count(t, T_STADIUM)) memcpy(st, row(t, T_STADIUM, tsid), 0x4e);
        put32(st, (uint32_t)stadium_id);
        memset(st + 4, 0, 51);
        memcpy(st + 4, rr->stadium, strlen(rr->stadium));
        int cap = rr->capacity > 0 ? rr->capacity : median(caps, nc);
        put32(st + 0x3c, (uint32_t)cap);
        put32(st + 0x40, (uint32_t)cap);
        put32(st + 0x44, (uint32_t)cap);
        put32(st + 0x48, 0xffffffffu);
        int city = -1;
        for (int i = 0; i < count(t, T_CITY) && city < 0; i++) {
            char nm[32];
            field(row(t, T_CITY, i) + 4, 26, nm);
            if (!strcmp(nm, rr->city) && gets32(row(t, T_CITY, i) + 0x1f) == nation_id) city = gets32(row(t, T_CITY, i));
        }
        if (city < 0 && *rr->city) {
            int tcity = gets32(st + 0x38);
            city = next_id(t, T_CITY);
            uint8_t *ci = add_row(t, T_CITY);
            if (tcity >= 0 && tcity < count(t, T_CITY)) memcpy(ci, row(t, T_CITY, tcity), 0x38);
            put32(ci, (uint32_t)city);
            memset(ci + 4, 0, 26);
            memcpy(ci + 4, rr->city, strlen(rr->city));
            put32(ci + 0x1f, (uint32_t)nation_id);
            result.cities++;
        }
        put32(st + 0x38, (uint32_t)city);
        put32(cl + 0x69, (uint32_t)stadium_id);
        put32(cl + 0x6e, 0xffffffffu);
        result.club_id[r] = club_id;
        k++;
        /* The squad. */
        if (!*rr->source || rr->source_club < 0) continue;
        int s = -1;
        for (int i = 0; i < nsrc; i++)
            if (!strcmp(src[i].name, rr->source)) s = i;
        if (s < 0 && nsrc < 16) {
            s = nsrc++;
            snprintf(src[s].name, sizeof src[s].name, "%s", rr->source);
            char sf[1100], sl[200];
            if (find_source && find_source(rr->source, sf, sizeof sf) && db_read(&src[s].db, sf, sl, sizeof sl)) {
                const SeasonKnownDb *known = seasons_db_known(rr->source);
                src[s].ok = src[s].db.staff_size == STAFF_SIZE && known;
                if (src[s].ok) season_dates_plan(sf, known->season, &src[s].plan);
            } else
                fprintf(stderr, "recomp: club records: the squad database %s is not here: %s gets no squad from it\n",
                        rr->source, rr->name);
        }
        if (s >= 0 && src[s].ok && rr->source_club < count(&src[s].db, T_CLUB))
            add_squad(t, t->add[T_CLUB] + (size_t)(k - 1) * CLUB_SIZE, club_id, &src[s].db, rr->source_club,
                      &src[s].plan, &pt, people, npeople, people2, names, season, &result);
    }
    for (int i = 0; i < nsrc; i++) db_free(&src[i].db);
    free(src);
    free(people);
    free(people2);
    for (int j = 0; j < 3; j++) free(names[j].slot);
    result.added = k;
    if (k != will) rt_fatal("club records: %d clubs made, %d planned", k, will);
    if (!k) { db_free(t); return 0; }
    if (!rebuild(t, log, room)) { season_records_free(); return -1; }
    merged_ok = 1;
    snprintf(log, room, "%d clubs, %d players (%d held by this database already), %d new names, %d new cities, "
                        "%d career rows; %d rows whose club this database holds", k, result.players, result.skipped,
             result.names, result.cities, result.careers, result.held);
    return k;
}

int season_records_merged(int i, const char **name, const uint8_t **data, size_t *size)
{
    if (i < 0 || !merged_ok) return 0;
    if (i == merged.nfiles) {
        *name = "index.dat";
        *data = out_index;
        *size = out_index_size;
        return 1;
    }
    if (i > merged.nfiles) return 0;
    *name = merged.files[i].name;
    *data = out_data[i];
    *size = out_size[i];
    return 1;
}

/* --- The game --- */

static int loaded_records;          /* new clubs of the last data load */
static int loaded_season;
static int loaded_file_clubs;       /* the club rows of the files (the first new club's id) */
static uint32_t loaded_staff_rows;  /* the staff rows of the merged files (the reserve rows follow) */
static int in_data_load;            /* 1 from before to after the data load 0x524d50 */

/* A source database: RECOMP_RECORDS_SOURCES (tests: a folder of unpacked databases, known by their content), else
 * the season database of its season in Databases or from Seasons/<name>.zip (seasons_db_prepare). */
static int find_source(const char *name, char *folder, size_t room)
{
    const SeasonKnownDb *known = seasons_db_known(name);
    if (!known) return 0;
    const char *v = getenv("RECOMP_RECORDS_SOURCES");
    if (v && *v) {
        HostDir *d = host_opendir(v);
        const char *e;
        int ok = 0;
        while (d && !ok && (e = host_readdir(d))) {
            if (e[0] == '.') continue;
            char p[1100];
            snprintf(p, sizeof p, "%s/%s", v, e);
            const SeasonKnownDb *k = seasons_db_identify(p);
            if (k && !strcmp(k->name, name) && strlen(p) < room) { snprintf(folder, room, "%s", p); ok = 1; }
        }
        if (d) host_closedir(d);
        return ok;
    }
    if (known->role != SEASONS_ROLE_SEASON) return 0;
    SeasonsPrepare r;
    memset(&r, 0, sizeof r);
    if (seasons_db_prepare(known->season, &r) != SEASONS_PREPARE_OK || strlen(r.path) >= room) return 0;
    snprintf(folder, room, "%s", r.path);
    return 1;
}

void season_records_before_load(const char *dir, int start_year)
{
    loaded_records = 0;
    loaded_season = 0;
    in_data_load = 1;
    win_memory_files_clear();
    season_records_free();
    const char *off = getenv("RECOMP_CLUB_RECORDS");     /* test switch: off adds no club */
    if (!season_code_on() || start_year == SEASON_FIRST_YEAR || !nrows || (off && !strcmp(off, "off"))) return;
    char log[400];
    int k = season_records_merge(dir, start_year, find_source, log, sizeof log);
    if (k < 0) {
        fprintf(stderr, "recomp: club records of %d: %s; no club records\n", start_year, log);
        return;
    }
    if (!k) return;
    fprintf(stderr, "recomp: club records of %d: %s\n", start_year, log);
    const char *dump = getenv("RECOMP_RECORDS_DUMP");    /* test switch: a folder for the merged files */
    for (int i = 0;; i++) {
        const char *name;
        const uint8_t *data;
        size_t size;
        if (!season_records_merged(i, &name, &data, &size)) break;
        if (!data) continue;
        char path[1300];
        snprintf(path, sizeof path, "%s/%s", merged.dir, name);
        win_memory_file(path, data, size);
        if (dump && *dump) {
            snprintf(path, sizeof path, "%s/%s", dump, name);
            FILE *f = host_fopen(path, "wb");
            if (f) { fwrite(data, 1, size, f); fclose(f); }
        }
    }
    loaded_records = k;
    loaded_season = start_year;
    loaded_file_clubs = count(&merged, T_CLUB);
    loaded_staff_rows = (uint32_t)count(&merged, T_STAFF) + merged.add_n[T_STAFF];
}

void season_records_after_load(void)
{
    in_data_load = 0;
    win_memory_files_clear();
    for (int i = 0; i < MAX_FILES; i++) { free(out_data[i]); out_data[i] = NULL; out_size[i] = 0; }
    free(out_index);
    out_index = NULL;
    db_free(&merged);
    merged_ok = 0;
}

int season_records_club(int season, const char *real)
{
    if (season != loaded_season || season != result_season) return -1;
    for (int r = 0; r < result.n; r++)
        if (result.club_id[r] >= 0 && !strcmp(rows[result.row_of[r]].real, real)) return result.club_id[r];
    return -1;
}

/* Review 1: only the data load sizes the reserve for the records (0x527240 runs at game creation, inside
 * 0x524d70); a saved game's load (0x524d10) keeps the reserve that it has without them. */
uint32_t season_records_reserve_extra(void) { return in_data_load ? (uint32_t)loaded_records * RESERVE_PER_CLUB : 0; }

/* --- The squad fill (Q509), in the game's memory after slice 15's club moves --- */

#define G_CLUBS 0xae23b4u
#define G_STAFF 0xae23bcu

static int median_of(int *v, int n) { return median(v, n); }

/* The squad fill (Q509, the lead's answer of 2026-10-20). The original has no squad fill at a new game (table
 * users doc, section 8). It runs at the start of 0x524d70, after slice 15's moves (season_clubs.c), so only a new
 * club that has a division gets players (lead decision O37: a record that the class count leaves out takes none).
 * A club with fewer than FILL_TO players takes players whom the database has at no club (+0x39 NULL; the id pass
 * has made the fields pointers), with player data only (+0x5f 2), aged 17 to 34 at the start (birth year +0x12,
 * shifted by slice 11b at the load): first FILL_KEEPERS goalkeepers when the squad has fewer (player row +0x0f 15
 * or more), then the good ones (current ability, player row +0x05, at or above the median of the players of the
 * division's clubs) youngest first, then the others by current ability. Its nation first, then the other nations
 * of its continent (nation +0x71). A person can join only one club: he leaves the pool when he is placed, and every
 * person of a source squad is a new staff row of a club already. The joined date becomes 1 July of the start year
 * and the contract ends on 30 June of the year after. */
void season_records_fill(int season)
{
    if (!loaded_records || season != loaded_season || rt_unwinding) return;
    uint32_t clubs = rd32(g_mem, G_CLUBS), staff = rd32(g_mem, G_STAFF);
    uint32_t n = loaded_staff_rows;
    if (!clubs || !staff || !n) return;
    int *cas = malloc(((size_t)n + 1) * sizeof *cas);
    if (!cas) rt_fatal("club records: no memory");
    int filled = 0, short_squads = 0, clubs_filled = 0;
    for (int r = 0; r < result.n; r++) {
        if (result.club_id[r] < 0) continue;
        uint32_t club = clubs + (uint32_t)result.club_id[r] * CLUB_SIZE;
        uint32_t div = rd32(g_mem, club + 0x57), nation = rd32(g_mem, club + 0x53);
        char name[52];
        memcpy(name, g_mem + club + 4, 51);
        name[51] = 0;
        if (!div) {
            fprintf(stderr, "recomp: club records: %s has no division: no squad fill\n", name);
            continue;
        }
        int slots = 0, players = 0, keepers = 0;
        for (int j = 0; j < SQUAD; j++) {
            uint32_t sp = rd32(g_mem, club + 0xd7 + 4 * (uint32_t)j);
            if (!sp) continue;
            slots = j + 1;
            players++;
            uint32_t pr = rd32(g_mem, sp + 0x61);
            if (pr && g_mem[pr + 0x0f] >= KEEPER_FROM) keepers++;
        }
        if (players >= FILL_TO) continue;
        int ncas = 0;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t sp = staff + i * STAFF_SIZE, cl = rd32(g_mem, sp + 0x39), pr = rd32(g_mem, sp + 0x61);
            if (cl && pr && rd32(g_mem, cl + 0x57) == div) cas[ncas++] = (int16_t)rd16(g_mem, pr + 5);
        }
        int good = median_of(cas, ncas);
        uint32_t continent = nation ? rd32(g_mem, nation + 0x71) : 0;
        int from_nation = 0, from_continent = 0;
        for (int wide = 0; wide < 2 && players < FILL_TO; wide++)
            while (players < FILL_TO && slots < SQUAD) {
                int want_keeper = keepers < FILL_KEEPERS, best_key = 0;
                uint32_t best = 0;
                for (uint32_t i = 0; i < n; i++) {
                    uint32_t sp = staff + i * STAFF_SIZE;
                    if (rd32(g_mem, sp + 0x39) || g_mem[sp + 0x5f] != 2) continue;
                    uint32_t nat = rd32(g_mem, sp + 0x1a), pr = rd32(g_mem, sp + 0x61);
                    if (!pr || !nat) continue;
                    if (wide ? nat == nation || !continent || rd32(g_mem, nat + 0x71) != continent : nat != nation) continue;
                    int by = (int)rd16(g_mem, sp + 0x12);
                    int age = by > EMPTY_YEAR ? season - by : 0;
                    if (age < 17 || age > 34) continue;
                    if (want_keeper && g_mem[pr + 0x0f] < KEEPER_FROM) continue;
                    int ca = (int16_t)rd16(g_mem, pr + 5);
                    int key = ca >= good ? 100000 + (60 - age) * 1000 + ca : ca;  /* good ones youngest first */
                    if (!best || key > best_key) best_key = key, best = sp;
                }
                if (!best) {
                    if (want_keeper) { keepers = FILL_KEEPERS; continue; }   /* no goalkeeper left: any player */
                    break;
                }
                if (g_mem[rd32(g_mem, best + 0x61) + 0x0f] >= KEEPER_FROM) keepers++;
                wr32(g_mem, best + 0x39, club);
                g_mem[best + 0x3d] = 11;
                wr16(g_mem, best + 0x3e, 181);                 /* joined 1 July of the start year */
                wr16(g_mem, best + 0x40, (uint32_t)season);
                wr32(g_mem, best + 0x42, 0);
                wr16(g_mem, best + 0x46, 180);                 /* contract to 30 June of the year after */
                wr16(g_mem, best + 0x48, (uint32_t)season + 1);
                wr32(g_mem, best + 0x4a, 0);
                wr32(g_mem, club + 0xd7 + 4 * (uint32_t)slots++, best);
                players++;
                filled++;
                if (wide) from_continent++;
                else from_nation++;
            }
        clubs_filled++;
        if (from_continent)
            fprintf(stderr, "recomp: club records: %s: %d players of its nation and %d of its continent with no club\n",
                    name, from_nation, from_continent);
        if (players < MIN_SQUAD) {
            short_squads++;
            fprintf(stderr, "recomp: club records: %s has %d players, fewer than %d\n", name, players, MIN_SQUAD);
        }
    }
    free(cas);
    fprintf(stderr, "recomp: club records: squad fill of %d clubs: %d players with no club joined, %d squads under %d\n",
            clubs_filled, filled, short_squads, MIN_SQUAD);
}

void o_005267c0(void);

/* database_trim_club_table 0x5267c0 (create_game, at 0x828395): after it, [0xae23fc][row] holds the row of each
 * club before the trim (table users doc, section 1 step 6), and the language names 0x5339a0 read that row of the
 * .lng club names. When the .lng files were written in the rows of the merged data at its load (season_lng.c,
 * guard review row A25), those rows are right as they are. Else (no file could be written) the original .lng rows
 * stand: in the merged data a national team stood loaded_records rows later than in the .lng file, and a new club
 * has no .lng row, so the entries become the .lng row of each national team and row 0 for a new club, and a
 * language change never reads past the .lng block. */
static void trim_rows(void);

RT_WRAP(005267c0, trim_club_table)
{
    o_005267c0();
    if (rt_unwinding) return;
    if (!season_lng_fitted()) trim_rows();
    else if (getenv("RECOMP_LNG_LOG")) {                /* test only: the .lng row of each added club */
        uint32_t index = rd32(g_mem, 0xae23fcu), n = rd32(g_mem, 0xae235cu), clubs = rd32(g_mem, 0xae23b4u);
        for (uint32_t i = 0; index && i < n; i++) {
            int32_t v = (int32_t)rd32(g_mem, index + 4 * i);
            if (v >= loaded_file_clubs && v < loaded_file_clubs + loaded_records)
                fprintf(stderr, "recomp: lng: added club %s has .lng row %d\n", win_str(clubs + 0x245u * i + 4), v);
        }
    }
}

static void trim_rows(void)
{
    if (!loaded_records) return;
    uint32_t index = rd32(g_mem, 0xae23fcu), n = rd32(g_mem, 0xae235cu);
    if (!index) return;
    int fixed = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t v = (int32_t)rd32(g_mem, index + 4 * i);
        if (v >= loaded_file_clubs + loaded_records) wr32(g_mem, index + 4 * i, (uint32_t)(v - loaded_records));
        else if (v >= loaded_file_clubs) wr32(g_mem, index + 4 * i, 0);
        else continue;
        fixed++;
    }
    fprintf(stderr, "recomp: club records: %d language name rows set after the club trim\n", fixed);
}

void o_0090dae0(void);

/* Test log (RECOMP_CLUB_LOG=1): VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 tops up both clubs of a fixture with
 * virtual players through 0x90dae0 (club, ...) and stops at virtual_staff.cpp 195 when a squad still has fewer than
 * 15 filled slots of +0xd7 (0x90d2f0, the count loop of 0x32 slots). The wrap names such a club. It only reads. */
RT_WRAP(0090dae0, virtual_staff_top_up)
{
    uint32_t club = rt_arg(0), fixture = rt_arg(3);
    o_0090dae0();
    if (rt_unwinding || !getenv("RECOMP_CLUB_LOG") || !club) return;
    int n = 0;
    for (uint32_t j = 0; j < SQUAD; j++) n += rd32(g_mem, club + 0xd7 + 4 * j) != 0;
    if (n < 15) {
        char name[52];
        memcpy(name, g_mem + club + 4, 51);
        name[51] = 0;
        fprintf(stderr, "recomp: club records: %s (club %d, nation %08x, division %08x) has %d squad slots after the "
                        "virtual players\n", name, (int)rd32(g_mem, club), rd32(g_mem, club + 0x53), rd32(g_mem, club + 0x57), n);
        /* The fixture (arg 3: its record +0x14) and, for a national team, the nation's squad record of
         * NationalTeams [0xb74318] (+4 + nation id * 0x6e; the senior slot +0x32..+0x45, end date +0x3a, picked
         * +0x43: select_squads_for_fixture 0x76bab0 at 0x76bb65..0x76bc23). */
        uint32_t fx = fixture, rec = fx ? rd32(g_mem, fx + 0x14) : 0, nat = rd32(g_mem, club + 0x53);
        char rn[52] = "";
        if (rec) { memcpy(rn, g_mem + rec + 4, 51); rn[51] = 0; }
        fprintf(stderr, "recomp: club records:   fixture %08x of %s, its bytes +0x40 %02x +0x43 %02x +0x4d %04x\n", fx, rn,
                fx ? g_mem[fx + 0x40] : 0, fx ? g_mem[fx + 0x43] : 0, fx ? rd16(g_mem, fx + 0x4d) : 0);
        fprintf(stderr, "recomp: club records:   club pointer %08x, fixture bytes:", club);
        for (uint32_t k = 0; fx && k < 0x50; k++) fprintf(stderr, "%s%02x", k % 16 ? " " : " |", g_mem[fx + k]);
        fputc('\n', stderr);
        /* The competition objects of g_comp_object_table [0xadadfc] (by record id) whose record is the fixture's. */
        uint32_t objs = rd32(g_mem, 0xadadfcu);
        for (uint32_t id = 0; objs && rec && id < 1024; id++) {
            uint32_t o = rd32(g_mem, objs + 4 * id);
            if (rt_readable(o) && rd32(g_mem, o + 4) == rec)
                fprintf(stderr, "recomp: club records:   object %08x of record %u, vtable %08x, clubs %d\n", o, id,
                        rd32(g_mem, o), (int16_t)rd16(g_mem, o + 0x3e));
        }
        uint32_t nt = rd32(g_mem, 0xb74318u);
        if (nt && nat) {
            uint32_t sq = rd32(g_mem, nt + 4) + rd32(g_mem, nat) * 0x6eu;
            fprintf(stderr, "recomp: club records:   squad record of nation %u:", rd32(g_mem, nat));
            for (uint32_t k = 0x32; k < 0x46; k++) fprintf(stderr, " %02x", g_mem[sq + k]);
            fputc('\n', stderr);
        }
    }
}


/* Test log (RECOMP_CLUB_LOG=2): FIXTURE_MANAGER::add_fixture 0x5aa680 (its wrap in season_kind_cup_breaks.c calls
 * this), for a fixture whose record (+0x14) has the long name of RECOMP_FIXTURE_RECORD: the caller (return
 * address) and the fixture's clubs (+0x1c, +0x20). It only reads. */
void season_records_fixture_log(uint32_t fx, uint32_t ret)
{
    const char *want = getenv("RECOMP_FIXTURE_RECORD"), *v = getenv("RECOMP_CLUB_LOG");
    if (!v || strcmp(v, "2") || fx < 0x10000u) return;
    uint32_t rec = rd32(g_mem, fx + 0x14);
    char rn[52] = "";
    if (rt_readable(rec)) { memcpy(rn, g_mem + rec + 4, 51); rn[51] = 0; }
    /* A club pointer outside the club table: the fixture names no club. */
    uint32_t base = rd32(g_mem, 0xae23b4u), end = base + rd32(g_mem, 0xae235cu) * 0x245u;
    int bad = 0;
    for (int k = 0; k < 2; k++) {
        uint32_t c = rd32(g_mem, fx + 0x1c + 4u * (uint32_t)k);
        bad |= c < base || c >= end || (c - base) % 0x245u;
    }
    if ((want && *want && !strcmp(rn, want)) || bad)
        fprintf(stderr, "recomp: club records: add_fixture from %08x: %s, clubs %08x %08x%s\n", ret, rn,
                rd32(g_mem, fx + 0x1c), rd32(g_mem, fx + 0x20), bad ? " (not a club)" : "");
}

/* Test log (RECOMP_CLUB_LOG=2): League::create_fixtures 0x6835c0 (its wrap in season_uefa_league.c calls this):
 * a league whose table (+0xb1, rows of 0x3b, count +0x3e) holds a row that is not a club of the club table. It
 * names the object, its vtable, record and count, the caller and the bad rows. It only reads. */
void season_records_league_log(uint32_t league, uint32_t ret)
{
    const char *v = getenv("RECOMP_CLUB_LOG");
    if (!v || strcmp(v, "2") || league < 0x10000u) return;
    uint32_t table = rd32(g_mem, league + 0xb1), rec = rd32(g_mem, league + 4);
    int n = (int16_t)rd16(g_mem, league + 0x3e);
    uint32_t base = rd32(g_mem, 0xae23b4u), end = base + rd32(g_mem, 0xae235cu) * 0x245u;
    if (table < 0x10000u || n <= 0 || n > 64) return;
    int bad = 0;
    for (int i = 0; i < n; i++) {
        uint32_t c = rd32(g_mem, table + (uint32_t)i * 0x3bu);
        bad += c < base || c >= end || (c - base) % 0x245u;
    }
    if (!bad) return;
    char rn[52] = "";
    if (rt_readable(rec)) { memcpy(rn, g_mem + rec + 4, 51); rn[51] = 0; }
    fprintf(stderr, "recomp: club records: create_fixtures from %08x: league %08x vtable %08x %s, %d rows, %d not "
                    "clubs:", ret, league, rd32(g_mem, league), rn, n, bad);
    for (int i = 0; i < n; i++) {
        uint32_t c = rd32(g_mem, table + (uint32_t)i * 0x3bu);
        if (c < base || c >= end || (c - base) % 0x245u) fprintf(stderr, " row %d %08x;", i, c);
    }
    fputc('\n', stderr);
}

void o_0090d2f0(void);

/* VIRTUAL_STAFF::fill_fixture_squads, thiscall (Fixture *fixture, short slot, char no_check), ret 0xc. It tops up
 * both clubs with virtual players (0x90dae0 at 0x90d3bd and 0x90d3f4), then, unless no_check, counts the filled
 * squad slots +0xd7 of both (0x90d429..0x90d469) and below 15 shows the box "virtual_staff.cpp 195" (0x90d4bc).
 * After the box it goes on: release_fixture 0x90d6b0 (0x90d4d1), then itself again with no_check 1 (0x90d4dc),
 * so the match is played with the squad it has. The top-up gives no virtual players to the side of a nation with
 * a reputation above 500 (0x90dd5d..0x90dd73), so an under-21 side of such a nation with few eligible players
 * stays short: in a 2023 start (October 2023, all nations) United States U21 had 11 on day 243 of 2024, in an
 * Under 21 International. In a season game the call takes no_check 1 at once: the same match as after the box,
 * with no modal box that stops the game. The base game keeps the original's check (Q152). */
void season_national_sizes(uint32_t fixture);   /* season_national_fill.c */
void season_national_fill(uint32_t fixture);

RT_WRAP(0090d2f0, virtual_staff_fill_check)
{
    uint32_t fixture = rt_arg(0);
    int season = season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
    if (season) g_mem[cpu.esp + 0xc] = 1;   /* no_check; the wrap of 0x90dae0 above names a short club (RECOMP_CLUB_LOG) */
    if (season) season_national_sizes(fixture);   /* a national side whose nation has no squad sizes */
    o_0090d2f0();
    if (season && !rt_unwinding) season_national_fill(fixture);   /* a national side that stays short */
}

void o_006db500(void);

/* Test log (RECOMP_CLUB_LOG=1): the team pick 0x6db500, thiscall, called by CM3_MATCH_ENGINE::init_match 0x6b7f20
 * once per side (0x6b89b9 for the away side, args: engine, [club+0xcf], club+0xd7, ..., club, opponent, fixture).
 * init_match shows match_eng.cpp 612 (home) or 652 (away) when it returns fewer than 11 (0x6b89be). The line names
 * the side, its squad slots, the national squad size of NationalTeams (0x775520) and the fixture's competition
 * (record +0x14). It only reads. */
RT_WRAP(006db500, team_pick_log)
{
    uint32_t club = rt_arg(2) - 0xd7, fixture = rt_arg(7);
    o_006db500();
    if (rt_unwinding || (int8_t)cpu.eax >= 11 || !getenv("RECOMP_CLUB_LOG")) return;
    int n = 0;
    for (uint32_t j = 0; j < SQUAD; j++) n += rd32(g_mem, club + 0xd7 + 4 * j) != 0;
    char name[52] = "", cname[52] = "", hname[52] = "";
    uint32_t rec = rd32(g_mem, fixture + 0x14), home = rd32(g_mem, fixture + 0x1c);
    if (club >= 0x10000u) memcpy(name, g_mem + club + 4, 51);
    if (rec >= 0x10000u) memcpy(cname, g_mem + rec + 4, 51);
    if (home >= 0x10000u) memcpy(hname, g_mem + home + 4, 51);
    fprintf(stderr, "recomp: club records: team pick of %s (club %08x) gives %d players from %d squad slots", name, club,
            (int8_t)cpu.eax, n);
    uint32_t nt = rd32(g_mem, 0xb74318u), sizes = nt >= 0x10000u ? rd32(g_mem, nt + 4) : 0;
    uint32_t sides[2] = {club, rt_arg(6)};
    for (int k = 0; k < 2; k++) {                     /* 0x7754c0 (+0x42) and 0x775520 (+0x43), kinds 0 to 2 */
        uint32_t nat = sides[k] >= 0x10000u ? rd32(g_mem, sides[k] + 0x53) : 0;
        if (sizes < 0x10000u || nat < 0x10000u || rd32(g_mem, nat) >= 0x10000u) continue;
        fprintf(stderr, ", %s national squad sizes", k ? "opponent" : "its");
        for (uint32_t t = 0; t < 3; t++)
            fprintf(stderr, " %d/%d", g_mem[sizes + 110u * rd32(g_mem, nat) + 20 * t + 0x42],
                    g_mem[sizes + 110u * rd32(g_mem, nat) + 20 * t + 0x43]);
    }
    fprintf(stderr, ", record %d %s, home %s, round %d\n", rec >= 0x10000u ? (int)rd32(g_mem, rec) : -1, cname, hname,
            (int16_t)rd16(g_mem, fixture + 0x30));
}
