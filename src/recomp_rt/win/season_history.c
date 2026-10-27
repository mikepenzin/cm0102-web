/* Competition history between 2001 and the start season (0.6.0 slice 11c, Q347): season_history.h.
 * Evidence: docs/research/2026-10-17-competition-history-fill.md. Table: season_history_table.c, made by
 * tools/seasons/make_history_table.py from docs/research/history-0.6.0/plan.tsv.
 *
 * Memory after the load 0x528540 and the id pass 0x5315a0:
 *   history   rows at [0xae23d4], 0x1a bytes, count [0xae237c]; the last [0xae2384] rows are the nation part,
 *             [0xae23dc] points at its first row (0x527f60 sets it to base + (count - nation count) * 0x1a).
 *             +0x04 the competition (a pointer, 0 for a free row), +0x08 u16 label year, +0x0a first,
 *             +0x0e second, +0x12 third, +0x16 host (club pointers, 0 for none): id pass 0x532c11 to 0x532d00,
 *             the writer 0x4afce0.
 *   clubs     rows at [0xae23b4], 0x245 bytes, count [0xae235c]: the clubs, then the national sides (the
 *             loader grows the club block and sets [0xae23b8] inside it, 0x52715c to 0x527187). +0x04 the full
 *             name (51 bytes), +0x53 the nation (a pointer after the id pass, 0x5315de); nation +0x04 its name.
 *   comps     +0x04 the name (51 bytes). */
#include "win.h"
#include "season_history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_PTR 0xae23d4u
#define HISTORY_COUNT 0xae237cu
#define NATION_HISTORY_PTR 0xae23dcu
#define NATION_HISTORY_COUNT 0xae2384u
#define HISTORY_SIZE 0x1au
#define CLUBS_PTR 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define NAME_LEN 51u
#define CLUB_COMPS_PTR 0xae23d0u   /* the club competition records */
#define CLUB_COMP_COUNT 0xae2378u
#define CLUB_COMP_SIZE 0x6bu

#define GROW_HISTORY 0x00527f60u   /* database_grow_comp_history_table (count): blank rows at the end, 1 on success */

/* --- Undo: the rows that the fill changed, by index (the grow moves the table), and the old row count. --- */

typedef struct { uint32_t index; uint8_t old[HISTORY_SIZE]; } Saved;
static Saved *saved;
static size_t saved_n, saved_cap;
static int filled;                  /* 1 while a fill is in memory */
static uint32_t old_count;          /* the row count before the fill */
static uint32_t n_added, n_cleared, n_unmatched, n_duplicates;

static void save_row(uint32_t index)
{
    if (saved_n == saved_cap) {
        size_t cap = saved_cap ? saved_cap * 2 : 1024;
        Saved *s = realloc(saved, cap * sizeof *s);
        if (!s) rt_fatal("season history: no memory for %zu saved rows", cap);
        saved = s;
        saved_cap = cap;
    }
    saved[saved_n].index = index;
    memcpy(saved[saved_n].old, g_mem + rd32(g_mem, HISTORY_PTR) + index * HISTORY_SIZE, HISTORY_SIZE);
    saved_n++;
}

static void set_count(uint32_t count)
{
    uint32_t base = rd32(g_mem, HISTORY_PTR), nations = rd32(g_mem, NATION_HISTORY_COUNT);
    wr32(g_mem, HISTORY_COUNT, count);
    wr32(g_mem, NATION_HISTORY_PTR, base + (count - nations) * HISTORY_SIZE);     /* as 0x527fa2..0x527fb2 */
}

void season_history_undo(void)
{
    if (!filled) return;
    uint32_t base = rd32(g_mem, HISTORY_PTR);
    while (saved_n) {
        Saved *s = &saved[--saved_n];
        memcpy(g_mem + base + s->index * HISTORY_SIZE, s->old, HISTORY_SIZE);
    }
    set_count(old_count);
    filled = 0;
}

void season_history_forget(void)
{
    filled = 0;
    saved_n = 0;
}

void season_history_counts(uint32_t *added, uint32_t *cleared, uint32_t *unmatched)
{
    *added = n_added;
    *cleared = n_cleared;
    *unmatched = n_unmatched;
}

/* --- Names in memory. --- */

static void text_at(uint32_t a, char *out, size_t n)
{
    size_t i = 0;
    if (a)
        for (; i + 1 < n && i < NAME_LEN && g_mem[a + i]; i++) out[i] = (char)g_mem[a + i];
    out[i] = 0;
}

static uint32_t name_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

/* Open addressing over the clubs by full name; each slot holds club index + 1. */
static uint32_t *slots;
static uint32_t n_slots;

static void index_clubs(uint32_t clubs, uint32_t n)
{
    n_slots = 1;
    while (n_slots < 2 * n + 16) n_slots <<= 1;
    free(slots);
    slots = calloc(n_slots, sizeof *slots);
    if (!slots) rt_fatal("season history: no memory for the club index");
    char name[64];
    for (uint32_t i = 0; i < n; i++) {
        text_at(clubs + i * CLUB_SIZE + 4, name, sizeof name);
        if (!name[0]) continue;
        uint32_t h = name_hash(name) & (n_slots - 1);
        while (slots[h]) h = (h + 1) & (n_slots - 1);
        slots[h] = i + 1;
    }
}

/* The pointer of the club or national side of this full name (the one of this nation when several share the
 * name), 0 for an empty name, or 0 with *missing set when no club has it. */
static uint32_t find_club(uint32_t clubs, const char *want, const char *nation, int *missing)
{
    if (!want[0]) return 0;
    uint32_t first = 0;
    char name[64], nat[64];
    for (uint32_t h = name_hash(want) & (n_slots - 1); slots[h]; h = (h + 1) & (n_slots - 1)) {
        uint32_t c = clubs + (slots[h] - 1) * CLUB_SIZE;
        text_at(c + 4, name, sizeof name);
        if (strcmp(name, want)) continue;
        if (!first) first = c;
        uint32_t np = rd32(g_mem, c + 0x53);
        text_at(np ? np + 4 : 0, nat, sizeof nat);
        if (!nation[0] || !strcmp(nat, nation)) return c;
    }
    if (!first) *missing = 1;
    return first;
}

/* The competitions that hold history rows: pointer by name. */
typedef struct { uint32_t ptr; char name[64]; } Comp;
static Comp *comps;
static uint32_t n_comps;

static void index_comps(uint32_t hist, uint32_t n)
{
    free(comps);
    comps = NULL;
    n_comps = 0;
    uint32_t cap = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = rd32(g_mem, hist + i * HISTORY_SIZE + 4), k;
        if (!c) continue;
        for (k = 0; k < n_comps && comps[k].ptr != c; k++) {}
        if (k < n_comps) continue;
        if (n_comps == cap) {
            cap = cap ? cap * 2 : 256;
            Comp *m = realloc(comps, cap * sizeof *m);
            if (!m) rt_fatal("season history: no memory for the competitions");
            comps = m;
        }
        comps[n_comps].ptr = c;
        text_at(c + 4, comps[n_comps].name, sizeof comps[n_comps].name);
        n_comps++;
    }
}

static void log_rows(const char *when);

static uint32_t find_comp(const char *want)
{
    for (uint32_t k = 0; k < n_comps; k++)
        if (!strcmp(comps[k].name, want)) return comps[k].ptr;
    /* A club competition with no history row in the database (the gap rows of plan.tsv: the US Open Cup of four
     * databases from 2010/11, whose newest winner Con_Champ::pick_clubs 0x4c11a0 reads for its place 1 through
     * 0x4afd80): the club competition records, [0xae23d0], 0x6b bytes, count [0xae2378], the long name at +4. */
    uint32_t base = rd32(g_mem, CLUB_COMPS_PTR), n = rd32(g_mem, CLUB_COMP_COUNT);
    char name[NAME_LEN + 1];
    for (uint32_t i = 0; base && i < n && i < 100000u; i++) {
        text_at(base + i * CLUB_COMP_SIZE + 4, name, sizeof name);
        if (!strcmp(name, want)) return base + i * CLUB_COMP_SIZE;
    }
    return 0;
}

static const SeasonHistDb *entry_for(const char *name, int N)
{
    for (int i = 0; name && i < season_history_dbs_count; i++)
        if (!strcmp(season_history_dbs[i].db, name) && season_history_dbs[i].start == N) return &season_history_dbs[i];
    return NULL;
}

/* A history row as a sort key: competition, label year, the four places, then its index. */
typedef struct { uint32_t comp, label, place[4], index; } RowKey;

static int by_key(const void *a, const void *b)
{
    const RowKey *x = a, *y = b;
    if (x->comp != y->comp) return x->comp < y->comp ? -1 : 1;
    if (x->label != y->label) return x->label < y->label ? -1 : 1;
    for (int f = 0; f < 4; f++)
        if (x->place[f] != y->place[f]) return x->place[f] < y->place[f] ? -1 : 1;
    return x->index < y->index ? -1 : x->index > y->index;
}

/* Clears the second and later copy of each row that the table holds twice (the head of season_history.h): its
 * competition becomes 0, the free row of the original (0x4afce0), and the undo record keeps the old bytes. */
static void drop_duplicates(void)
{
    uint32_t hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT);
    if (!hist || !count) return;
    RowKey *keys = malloc((size_t)count * sizeof *keys);
    if (!keys) rt_fatal("season history: no memory");
    uint32_t n = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t r = hist + i * HISTORY_SIZE;
        RowKey *k = &keys[n];
        k->comp = rd32(g_mem, r + 4);
        if (!k->comp) continue;
        k->label = rd16(g_mem, r + 8);
        for (int f = 0; f < 4; f++) k->place[f] = rd32(g_mem, r + 0xa + 4 * (uint32_t)f);
        k->index = i;
        n++;
    }
    qsort(keys, n, sizeof *keys, by_key);
    for (uint32_t i = 1; i < n; i++) {
        const RowKey *a = &keys[i - 1], *b = &keys[i];
        if (a->comp != b->comp || a->label != b->label || memcmp(a->place, b->place, sizeof a->place)) continue;
        save_row(b->index);
        wr32(g_mem, hist + b->index * HISTORY_SIZE + 4, 0);
        n_duplicates++;
    }
    free(keys);
    if (n_duplicates) {
        filled = 1;
        old_count = count;
        fprintf(stderr, "recomp: season history: %u duplicate rows cleared\n", n_duplicates);
    }
}

void season_history_fill(const char *name, int N)
{
    season_history_undo();
    saved_n = 0;
    n_added = n_cleared = n_unmatched = n_duplicates = 0;
    drop_duplicates();
    const SeasonHistDb *e = entry_for(name, N);
    if (!e) return;
    uint32_t hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT);
    uint32_t clubs = rd32(g_mem, CLUBS_PTR), n_clubs = rd32(g_mem, CLUB_COUNT);
    if (!hist || !clubs) return;
    index_comps(hist, count);

    /* The rows to write: those whose competition is in memory and whose first club is found. A planned row
     * whose winner is not in memory stays out, and the database keeps its own rows of that competition and
     * year (dropped). */
    index_clubs(clubs, n_clubs);
    typedef struct { uint32_t comp, place[4]; uint16_t label; } New;
    New *nw = malloc((e->n_rows + 1) * sizeof *nw);
    uint32_t n_new = 0, n_dropped = 0;
    typedef struct { uint32_t comp; uint16_t label; } Key;
    Key *dropped = malloc((e->n_rows + 1) * sizeof *dropped);
    if (!nw || !dropped) rt_fatal("season history: no memory");
    for (uint32_t k = 0; k < e->n_rows; k++) {
        const SeasonHistRow *row = &e->rows[k];
        uint32_t c = find_comp(row->comp);
        if (!c) continue;
        int missing[4] = {0, 0, 0, 0};
        New *x = &nw[n_new];
        x->comp = c;
        x->label = row->label;
        for (int f = 0; f < 4; f++) {
            x->place[f] = find_club(clubs, row->place[f], row->nation[f], &missing[f]);
            if (missing[f]) {
                n_unmatched++;
                if (getenv("RECOMP_HISTORY_LOG"))
                    fprintf(stderr, "recomp: season history: %s %u: no club \"%s\" in memory\n", row->comp,
                            row->label, row->place[f]);
            }
        }
        if (!x->place[0]) {                         /* no winner: the row stays out, the database row stays */
            dropped[n_dropped].comp = c;
            dropped[n_dropped++].label = row->label;
            continue;
        }
        n_new++;
    }

    /* The rows to clear (their indices), so that the free rows can take the new ones. */
    uint32_t *freed = malloc((count + 1) * sizeof *freed), n_freed = 0;
    if (!freed) rt_fatal("season history: no memory");
    for (uint32_t i = 0; i < count; i++) {
        uint32_t r = hist + i * HISTORY_SIZE, c = rd32(g_mem, r + 4);
        if (!c) continue;
        uint16_t y = (uint16_t)rd16(g_mem, r + 8);
        int keep = 0;
        for (uint32_t k = 0; k < n_dropped && !keep; k++) keep = dropped[k].comp == c && dropped[k].label == y;
        if (keep) continue;
        for (uint32_t k = 0; k < e->n_clears; k++)
            if (e->clears[k].label == y && find_comp(e->clears[k].comp) == c) { freed[n_freed++] = i; break; }
    }
    free(dropped);

    old_count = count;
    uint32_t base_before = hist;
    if (n_new > n_freed) {
        uint32_t grow = n_new - n_freed;
        if (!rt_callback(GROW_HISTORY, 1, &grow) || rt_unwinding) {
            fprintf(stderr, "recomp: season history: the history table could not grow by %u rows\n", grow);
            free(freed);
            free(nw);
            return;
        }
        hist = rd32(g_mem, HISTORY_PTR);
    }
    filled = 1;
    for (uint32_t k = 0; k < n_freed; k++) {
        save_row(freed[k]);
        wr32(g_mem, hist + freed[k] * HISTORY_SIZE + 4, 0);
    }
    n_cleared = n_freed;
    for (uint32_t k = 0; k < n_new; k++) {
        uint32_t index = k < n_freed ? freed[k] : count + (k - n_freed);
        uint32_t r = hist + index * HISTORY_SIZE;
        wr32(g_mem, r + 4, nw[k].comp);
        wr16(g_mem, r + 8, nw[k].label);
        for (int f = 0; f < 4; f++) wr32(g_mem, r + 0xa + 4 * (uint32_t)f, nw[k].place[f]);
    }
    n_added = n_new;
    fprintf(stderr, "recomp: season history: %s in %d: %u rows written, %u database rows cleared, %u names not "
                    "found, %u rows in the table (at %08x, before the grow %08x)\n", e->db, N, n_added, n_cleared,
            n_unmatched, rd32(g_mem, HISTORY_COUNT), hist, base_before);
    free(freed);
    free(nw);
    log_rows("load");
}

/* --- Test log: RECOMP_HISTORY_LOG=<competition>;<competition>;... At the first human turn, every row of each
 * competition from the label year 1999 on, in year order, and the same right after a fill ("load": before the
 * game keeps only the rows of the selected nations, 0x539ae0): "year first / second". It only reads memory. --- */

typedef struct { uint16_t year; char first[64], second[64]; } LogRow;

static int by_year(const void *a, const void *b)
{
    const LogRow *x = a, *y = b;
    return x->year != y->year ? x->year - y->year : strcmp(x->first, y->first);
}

static void log_rows(const char *when)
{
    const char *v = getenv("RECOMP_HISTORY_LOG");
    if (!v || !*v) return;
    uint32_t hist = rd32(g_mem, HISTORY_PTR), n = rd32(g_mem, HISTORY_COUNT);
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", v);
    for (char *p = strtok(buf, ";"); p; p = strtok(NULL, ";")) {
        LogRow *rows = calloc(n + 1, sizeof *rows);
        uint32_t m = 0;
        if (!rows) return;
        for (uint32_t i = 0; hist && i < n; i++) {
            uint32_t r = hist + i * HISTORY_SIZE, c = rd32(g_mem, r + 4);
            char name[64];
            text_at(c ? c + 4 : 0, name, sizeof name);
            uint16_t y = (uint16_t)rd16(g_mem, r + 8);
            if (!c || strcmp(name, p) || y < 1999) continue;
            rows[m].year = y;
            uint32_t a = rd32(g_mem, r + 0xa), b = rd32(g_mem, r + 0xe);
            text_at(a ? a + 4 : 0, rows[m].first, 64);
            text_at(b ? b + 4 : 0, rows[m].second, 64);
            m++;
        }
        qsort(rows, m, sizeof *rows, by_year);
        fprintf(stderr, "recomp: history log %s: %s:", when, p);
        for (uint32_t i = 0; i < m; i++) fprintf(stderr, " %u %s / %s;", rows[i].year, rows[i].first, rows[i].second);
        fprintf(stderr, "\n");
        free(rows);
    }
}

void season_history_log_turn(void)
{
    static int done;
    if (done) return;
    done = 1;
    log_rows("turn");
}
