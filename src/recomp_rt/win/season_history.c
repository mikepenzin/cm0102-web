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
#include "season.h"
#include "season_history.h"
#include "season_calendar.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_titles.h"
#include "season_bra_states.h"
#include "season_history_start.h"
#include "season_nl.h"

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
#define FRIENDLY_RECORDS 0x7fu     /* the last records belong to the friendly object (new-comp-records doc 1.6) */

#define NATIONAL_SIDES_PTR 0xae23b8u /* the first national side inside the club block (0x52715c to 0x527187) */
#define GROW_HISTORY 0x00527f60u   /* database_grow_comp_history_table (count): blank rows at the end, 1 on success */
#define GAME_DAY 0xae2c90u          /* s16, the day of the year of the game date (from 0) */
#define GAME_YEAR 0xae2c92u         /* u16, the year of the game date: the label year that 0x4afce0 writes */
#define FIX_BASE_GAME 1             /* 0.6.2 slice 7: the rows after the start also go in the base game */

/* --- Undo: the rows that the fill changed, by index (the grow moves the table), and the old row count. --- */

typedef struct { uint32_t index; uint8_t old[HISTORY_SIZE]; } Saved;
static Saved *saved;
static size_t saved_n, saved_cap;
static int filled;                  /* 1 while a fill is in memory */
static uint32_t old_count;          /* the row count before the fill */
static uint32_t n_added, n_cleared, n_unmatched, n_duplicates;
/* The rows that the game wrote since the last load or new game, by content (competition, label, four places):
 * the lookup 0x4afd80 sorts the whole table (qsort 0x945333 at 0x4afd9b), so an index does not last. Rows of one
 * label that the game writes in one year all stay. In memory only: a save of the calendar year of the start
 * between two such rows of one competition does not keep them apart. */
typedef struct { uint32_t comp, label, place[4], day; } Own;     /* day: year * 400 + day of the year */
static Own *own;
static size_t own_n, own_cap;

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
    own_n = 0;
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

/* The entry of database `name` for start year N: the entry of that start year, else the entry of an official data
 * set that holds from its start on (open item 149). */
static const SeasonHistDb *entry_for(const char *name, int N)
{
    for (int i = 0; name && i < season_history_dbs_count; i++)
        if (!strcmp(season_history_dbs[i].db, name) && season_history_dbs[i].start == N) return &season_history_dbs[i];
    for (int i = 0; name && i < season_history_dbs_count; i++) {
        const SeasonHistDb *e = &season_history_dbs[i];
        if (e->last && !strcmp(e->db, name) && e->start <= N && N <= e->last) return e;
    }
    return NULL;
}

/* 1 when a row with the start years from and to holds for start year N (to 0: the start year of its entry). */
static int holds(uint16_t from, uint16_t to, int N)
{
    return !to || (from <= N && N <= to);
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

/* Open item 74 (0.6.2, the forum): the past editions of a competition whose record the season code adds
 * (season_comps.h, `like`), so no database holds its history. A row goes in when its label year is not after
 * the start year N (a game from 2021/22 plays the 2022 Finalissima itself). Source: Wikipedia, "Finalissima",
 * Results (31 October 2026): 1985 France 2-0 Uruguay, Parc des Princes, Paris; 1993 Argentina 1-1 Denmark,
 * Argentina on penalties, Mar del Plata; 2022 Argentina 3-0 Italy, Wembley Stadium, London. No host, as the
 * game's own edition writes none (the class's history row, season_gold_cup.c; competitions.tsv has_host 0). */
static const SeasonHistRow new_comp_rows[] = {
    {"Finalissima", 1985, {"France", "Uruguay", "", ""}, {"France", "Uruguay", "", ""}},
    {"Finalissima", 1993, {"Argentina", "Denmark", "", ""}, {"Argentina", "Denmark", "", ""}},
    {"Finalissima", 2022, {"Argentina", "Italy", "", ""}, {"Argentina", "Italy", "", ""}},
};
#define NEW_COMP_ROWS ((uint32_t)(sizeof new_comp_rows / sizeof new_comp_rows[0]))

/* The record that the season code added (season_comps.h: the last records of the nation block, before the 0x7f
 * friendly records) with this name in the data, or 0. Never in the base game: it adds none, and a Custom
 * database's own record of that name is left alone there (Q152). */
static uint32_t find_record(const char *want)
{
    /* the base game test of the module's own record pass (wanted, season_comps.c): at the load the rules flag
     * of the game is not set yet, so season_comps_on() would say no for a 2001 game with changing rules on */
    if (!season_code_on() || (season_start_year() == SEASON_FIRST_YEAR && !season_next_rules())) return 0;
    uint32_t base = rd32(g_mem, CLUB_COMPS_PTR), n = rd32(g_mem, CLUB_COMP_COUNT);
    uint32_t k = (uint32_t)season_comps_added_records();
    if (!base || n < FRIENDLY_RECORDS + k || n > 100000u) return 0;
    for (uint32_t i = n - FRIENDLY_RECORDS - k; i < n - FRIENDLY_RECORDS; i++) {
        const char *name = season_change_data_name(base + i * CLUB_COMP_SIZE);
        if (name && !strcmp(name, want)) return base + i * CLUB_COMP_SIZE;
    }
    return 0;
}

/* 1 when the table holds a row of this competition and label year. */
static int has_row(uint32_t hist, uint32_t count, uint32_t comp, uint16_t label)
{
    for (uint32_t i = 0; i < count; i++)
        if (rd32(g_mem, hist + i * HISTORY_SIZE + 4) == comp && rd16(g_mem, hist + i * HISTORY_SIZE + 8) == label)
            return 1;
    return 0;
}

static void fill_rows(const char *name, int N, int duplicates)
{
    season_history_undo();
    saved_n = 0;
    n_added = n_cleared = n_unmatched = n_duplicates = 0;
    uint32_t hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT);
    /* The rows of the added records that this game takes (none in the base game: it adds no record). */
    const SeasonHistRow *extra[NEW_COMP_ROWS];
    uint32_t n_extra = 0;
    for (uint32_t k = 0; hist && k < NEW_COMP_ROWS; k++) {
        uint32_t c = new_comp_rows[k].label <= N ? find_record(new_comp_rows[k].comp) : 0;
        if (c && !has_row(hist, count, c, new_comp_rows[k].label)) extra[n_extra++] = &new_comp_rows[k];
    }
    if (name || duplicates) drop_duplicates();       /* open item 227: also the 3.9.60 and 3.9.65 data */
    const SeasonHistDb *e = name ? entry_for(name, N) : NULL;
    if (!e && !n_extra) return;
    hist = rd32(g_mem, HISTORY_PTR);
    count = rd32(g_mem, HISTORY_COUNT);
    uint32_t clubs = rd32(g_mem, CLUBS_PTR), n_clubs = rd32(g_mem, CLUB_COUNT);
    if (!hist || !clubs) return;
    index_comps(hist, count);

    /* The rows to write: those whose competition is in memory and whose first club is found. A planned row
     * whose winner is not in memory stays out, and the database keeps its own rows of that competition and
     * year (dropped). */
    index_clubs(clubs, n_clubs);
    typedef struct { uint32_t comp, place[4]; uint16_t label; } New;
    uint32_t n_rows = (e ? e->n_rows : 0) + n_extra;
    New *nw = malloc((n_rows + 1) * sizeof *nw);
    uint32_t n_new = 0, n_dropped = 0;
    typedef struct { uint32_t comp; uint16_t label; } Key;
    Key *dropped = malloc((n_rows + 1) * sizeof *dropped);
    if (!nw || !dropped) rt_fatal("season history: no memory");
    for (uint32_t k = 0; k < n_rows; k++) {
        const SeasonHistRow *row = e && k < e->n_rows ? &e->rows[k] : extra[k - (e ? e->n_rows : 0)];
        if (!holds(row->from, row->to, N)) continue;
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
        for (uint32_t k = 0; e && k < e->n_clears; k++)
            if (e->clears[k].label == y && holds(e->clears[k].from, e->clears[k].to, N) &&
                find_comp(e->clears[k].comp) == c) { freed[n_freed++] = i; break; }
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
                    "found, %u rows in the table (at %08x, before the grow %08x)\n", e ? e->db : name ? name : "no plan",
            N, n_added, n_cleared,
            n_unmatched, rd32(g_mem, HISTORY_COUNT), hist, base_before);
    free(freed);
    free(nw);
    log_rows("load");
}

/* --- The corrected rows of issues 145 and 165 (docs/research/history-rows/, season_history.h). --- */

/* The pointer of the first spelling of `names` (joined by '|') that a club or national side holds, or 0. */
static uint32_t find_spelling(uint32_t clubs, const char *names)
{
    char one[NAME_LEN + 1];
    for (const char *p = names; *p; ) {
        size_t n = strcspn(p, "|");
        int missing = 0;
        snprintf(one, sizeof one, "%.*s", (int)n, p);
        uint32_t c = find_club(clubs, one, "", &missing);
        if (c) return c;
        p += n + (p[n] == '|');
    }
    return 0;
}

/* 1 when the club at pointer c has one of the names of `names`. */
static int named(uint32_t c, const char *names)
{
    char have[NAME_LEN + 1];
    text_at(c ? c + 4 : 0, have, sizeof have);
    size_t len = strlen(have);
    for (const char *p = names; len && *p; ) {
        size_t n = strcspn(p, "|");
        if (n == len && !memcmp(p, have, n)) return 1;
        p += n + (p[n] == '|');
    }
    return 0;
}

/* The record of a competition of the tables: a record that the season code added, else one of the data. */
static uint32_t rows_comp(const char *want)
{
    uint32_t c = find_record(want);
    return c ? c : find_comp(want);
}

/* Clears row `index` for the undo record: its competition becomes 0, the free row of the original (0x4afce0). */
static void free_row(uint32_t hist, uint32_t index)
{
    save_row(index);
    wr32(g_mem, hist + index * HISTORY_SIZE + 4, 0);
}

/* The end of the fill in a season game: the corrections, then the rows of editions that never took place, then the
 * whole lists. A list row goes into a row that the list freed, else into a new row of 0x527f60. A name that a
 * database lacks leaves its place empty; a list row with no winner stays out. */
static int after_start_part(char part);

/* Test switch RECOMP_HISTORY_ROWS, read once: unset, all parts of apply_rows; off, none; else the letters of the parts
 * that act: f the corrections, h the clear of club hosts, c the clears, l the lists. The Q48 switch
 * RECOMP_HISTORY_AFTER_START=off also stops them all (the new-game pair rules-on and history-q48-off of smoke_run.py). */
static int rows_part(char part)
{
    static const char *mode;
    if (!mode) {
        const char *v = getenv("RECOMP_HISTORY_ROWS");
        int q48_off = !after_start_part('c') && !after_start_part('w');
        mode = q48_off ? "" : !v || !*v ? "fhcl" : !strcmp(v, "off") ? "" : v;
        if (q48_off) fprintf(stderr, "recomp: season history: corrected rows: off (RECOMP_HISTORY_AFTER_START=off)\n");
        else if (strcmp(mode, "fhcl"))
            fprintf(stderr, "recomp: season history: corrected rows: RECOMP_HISTORY_ROWS=%s, parts that act:%s%s%s%s\n", v,
                    strchr(mode, 'f') ? " f" : "", strchr(mode, 'h') ? " h" : "", strchr(mode, 'c') ? " c" : "",
                    strchr(mode, 'l') ? " l" : "");
    }
    return strchr(mode, part) != NULL;
}

static void apply_rows(int N)
{
    if (!season_code_on() || (N == SEASON_FIRST_YEAR && !season_next_rules())) return;
    if (!rows_part('f') && !rows_part('h') && !rows_part('c') && !rows_part('l')) return;
    uint32_t hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT);
    uint32_t clubs = rd32(g_mem, CLUBS_PTR), n_clubs = rd32(g_mem, CLUB_COUNT);
    if (!hist || !clubs) return;
    if (!filled) {
        filled = 1;
        old_count = count;
    }
    index_comps(hist, count);
    index_clubs(clubs, n_clubs);
    uint32_t fixed = 0, cleared = 0, written = 0, lost = 0;

    for (int k = 0; rows_part('f') && k < season_history_row_fixes_count; k++) {
        const SeasonHistRowFix *x = &season_history_row_fixes[k];
        uint32_t c = rows_comp(x->comp), at = 0xa + 4u * x->field;
        for (uint32_t i = 0; c && i < count; i++) {
            uint32_t r = hist + i * HISTORY_SIZE;
            if (rd32(g_mem, r + 4) != c || rd16(g_mem, r + 8) != x->label || !named(rd32(g_mem, r + 0xa), x->guard))
                continue;
            uint32_t now = rd32(g_mem, r + at);
            if (x->write[0] ? named(now, x->accept) : !now) continue;
            uint32_t to = x->write[0] ? find_spelling(clubs, x->write) : 0;
            if (x->write[0] && !to) { lost++; continue; }
            save_row(i);
            wr32(g_mem, r + at, to);
            fixed++;
        }
    }
    /* A national team tournament never has a club as its host: the data of some databases points there (Olot). */
    uint32_t sides = rd32(g_mem, NATIONAL_SIDES_PTR);
    for (uint32_t i = 0; rows_part('h') && sides > clubs && i < count; i++) {
        uint32_t r = hist + i * HISTORY_SIZE, host = rd32(g_mem, r + 0x16);
        if (!rd32(g_mem, r + 4) || rd32(g_mem, r + 0xa) < sides || !host || host >= sides) continue;
        save_row(i);
        wr32(g_mem, r + 0x16, 0);
        fixed++;
    }
    for (int k = 0; rows_part('c') && k < season_history_clears_from_count; k++) {
        uint32_t c = rows_comp(season_history_clears_from[k].comp);
        for (uint32_t i = 0; c && i < count; i++) {
            uint32_t r = hist + i * HISTORY_SIZE;
            if (rd32(g_mem, r + 4) != c || rd16(g_mem, r + 8) < season_history_clears_from[k].from_label) continue;
            free_row(hist, i);
            cleared++;
        }
    }

    /* The lists: the rows of this start year first, so that a list with no row here leaves its competition alone. */
    typedef struct { uint32_t comp, place[4]; uint16_t label; } ListNew;
    ListNew *nw = malloc(((size_t)season_history_list_rows_count + 1) * sizeof *nw);
    uint32_t *freed = malloc(((size_t)count + 1) * sizeof *freed), n_new = 0, n_freed = 0;
    if (!nw || !freed) rt_fatal("season history: no memory");
    for (int k = 0; k < season_history_list_rows_count; k++) {
        const SeasonHistListRow *x = &season_history_list_rows[k];
        uint32_t c = rows_part('l') && x->from <= N && x->label <= N ? rows_comp(x->comp) : 0;
        if (!c) continue;
        ListNew *w = &nw[n_new];
        w->comp = c;
        w->label = x->label;
        for (int f = 0; f < 4; f++) {
            w->place[f] = x->place[f][0] ? find_spelling(clubs, x->place[f]) : 0;
            if (x->place[f][0] && !w->place[f]) lost++;
        }
        if (w->place[0]) n_new++;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t c = rd32(g_mem, hist + i * HISTORY_SIZE + 4), k;
        for (k = 0; c && k < n_new && nw[k].comp != c; k++) {}
        if (c && k < n_new) freed[n_freed++] = i;
    }
    /* The grow comes first: when it fails, the competitions keep the rows they hold. */
    int grown = 1;
    if (n_new > n_freed) {
        uint32_t grow = n_new - n_freed;
        grown = rt_callback(GROW_HISTORY, 1, &grow) && !rt_unwinding;
        if (!grown) fprintf(stderr, "recomp: season history: the history table could not grow by %u rows\n", grow);
        hist = rd32(g_mem, HISTORY_PTR);
    }
    for (uint32_t k = 0; grown && k < n_freed; k++) free_row(hist, freed[k]);
    if (grown) cleared += n_freed;
    for (uint32_t k = 0; grown && k < n_new; k++) {
        uint32_t index = k < n_freed ? freed[k] : count + (k - n_freed), r = hist + index * HISTORY_SIZE;
        wr32(g_mem, r + 4, nw[k].comp);
        wr16(g_mem, r + 8, nw[k].label);
        for (int f = 0; f < 4; f++) wr32(g_mem, r + 0xa + 4 * (uint32_t)f, nw[k].place[f]);
        written++;
        season_nl_history_record(nw[k].comp);
    }
    free(nw);
    free(freed);
    fprintf(stderr, "recomp: season history: corrected rows in %d: %u fields corrected, %u rows cleared, %u rows "
                    "written, %u names not found\n", N, fixed, cleared, written, lost);
}

void season_history_fill(const char *name, int N, int duplicates)
{
    fill_rows(name, N, duplicates);
    if (!rt_unwinding) apply_rows(N);
}

/* --- Rows after the start (0.6.2 slice 7, open items 3 and 17; the head of season_history.h). --- */

/* Test switch RECOMP_HISTORY_AFTER_START, read once: off (neither part), create (only the drop at the creation),
 * write (only the work at 0x4afce0); unset: both. */
static int after_start_part(char part)
{
    static int mode = -1;
    if (mode < 0) {
        const char *v = getenv("RECOMP_HISTORY_AFTER_START");
        mode = !v || !*v ? 3 : !strcmp(v, "off") ? 0 : !strcmp(v, "create") ? 1 : !strcmp(v, "write") ? 2 : 3;
        if (mode != 3) fprintf(stderr, "recomp: season history: rows after the start: %s only\n", v);
    }
    return part == 'c' ? (mode & 1) != 0 : (mode & 2) != 0;
}

int season_history_after_start_on(void)
{
    if (!season_code_on()) return 0;
    if (FIX_BASE_GAME) return 1;
    return season_start_year() != SEASON_FIRST_YEAR || season_changing_rules();
}

static uint32_t n_after;

/* 1 when the game wrote row r in this program; *day gets the day it wrote it. */
static int own_row(uint32_t r, uint32_t *day)
{
    for (size_t k = 0; k < own_n; k++) {
        const Own *o = &own[k];
        if (o->comp != rd32(g_mem, r + 4) || o->label != rd16(g_mem, r + 8)) continue;
        int f = 0;
        while (f < 4 && o->place[f] == rd32(g_mem, r + 0xa + 4 * (uint32_t)f)) f++;
        if (f < 4) continue;
        if (day) *day = o->day;
        return 1;
    }
    return 0;
}

/* The number of rows of the competition with this label. */
static uint32_t rows_of_label(uint32_t hist, uint32_t count, uint32_t comp, int label)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t r = hist + i * HISTORY_SIZE;
        n += rd32(g_mem, r + 4) == comp && (int)rd16(g_mem, r + 8) == label;
    }
    return n;
}

/* 1 when the competition holds two or more rows a year in most of the 10 years before the start that have rows (the
 * halves of a split season, two groups of one division). */
static int split_season(uint32_t hist, uint32_t count, uint32_t comp, int S)
{
    int years = 0, two = 0;
    for (int y = S - 10; y < S; y++) {
        uint32_t k = rows_of_label(hist, count, comp, y);
        years += k > 0;
        two += k >= 2;
    }
    return years && two * 2 > years;
}

void season_history_drop_after_start(int S, int official)
{
    n_after = 0;
    if (!season_history_after_start_on() || !after_start_part('c')) return;
    uint32_t hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT);
    if (!hist) return;
    uint32_t *drop = malloc((count + 1) * sizeof *drop), n = 0;
    if (!drop) rt_fatal("season history: no memory");
    for (uint32_t i = 0; i < count; i++) {
        uint32_t r = hist + i * HISTORY_SIZE, comp = rd32(g_mem, r + 4);
        int label = (int)rd16(g_mem, r + 8);
        if (!comp || label < S) continue;
        /* The official data labels a split season by the year where it starts (label 2001: the Apertura 2001 and
         * the Clausura 2002, docs/research/2026-10-17-competition-history-fill.md), so its rows of the label S are
         * the season that the game plays and labels S + 1: a competition with two or more rows of S and of S - 1. */
        if (label == S && !(official && rows_of_label(hist, count, comp, S) >= 2 &&
                            rows_of_label(hist, count, comp, S - 1) >= 2))
            continue;
        drop[n++] = i;
    }
    if (n && !filled) {
        filled = 1;
        old_count = count;
    }
    for (uint32_t k = 0; k < n; k++) {
        save_row(drop[k]);
        wr32(g_mem, hist + drop[k] * HISTORY_SIZE + 4, 0);
    }
    n_after = n;
    free(drop);
    if (n_after)
        fprintf(stderr, "recomp: season history: %u rows after the start season %d cleared\n", n_after, S);
}

void o_004afce0(void);

/* The state of history row r of record comp (season_bra_states_other_state): 1 for a championship of another state
 * than the one the game plays on that record, 0 for that state, -1 not known. By its winner, else its runner-up. */
static int row_state(uint32_t comp, uint32_t r)
{
    int k = season_bra_states_other_state(comp, rd32(g_mem, r + 0xa));
    return k < 0 ? season_bra_states_other_state(comp, rd32(g_mem, r + 0xe)) : k;
}

/* comp_util_add_history_record, cdecl (Comp *comp, Club *first, Club *second, Club *third, Club *host): the row of
 * an edition that the game has played. It reuses a free row at the end of the table or grows it (0x527f60), and
 * writes the competition record [comp + 4] (0x4afd51), the game year [0xae2c92] as the label (0x4afd57,
 * 0x4afd62) and the four places from its arguments (0x4afd6a to 0x4afd7b). Before it, the rows of the data for
 * the same edition go: every other row of the competition with a later label (the game never writes a label after
 * the game year), and in the calendar year of the start the row of this label, unless the data holds two or more
 * rows of it (the Argentine and Mexican first divisions keep the two halves of a season under one label, and in
 * the Update Team databases those halves ended before the start: Autumn 2013 holds the Inicial 2012 and the Final
 * 2013 under 2013).
 * A second edition in one calendar year (a final that moves across the new year: the Emperor's Cup on 1 January
 * 2005 and on 31 December 2005, the Turkish Super Cup from August to January) takes the next free label, so each
 * edition keeps a row of its own. The label goes in through the game year [0xae2c92], which only 0x4afd57 reads
 * in 0x4afce0 and its callee 0x527f60; it is set back right after the call. The first-season clear runs to S + 1:
 * after it every row of a later label is a row of the game. */
RT_WRAP(004afce0, comp_util_add_history_record)
{
    uint32_t obj = rt_arg(0), hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT);
    uint32_t comp = obj ? rd32(g_mem, obj + 4) : 0;
    int year = (int)rd16(g_mem, GAME_YEAR), S = season_start_year(), n = 0;
    uint32_t today = (uint32_t)year * 400u + (uint32_t)(int16_t)rd16(g_mem, GAME_DAY);
    int on = comp && hist && season_history_after_start_on() && after_start_part('w');
    if (on && year <= S + 1) {
        int m = 0;
        for (uint32_t i = 0; year == S && i < count; i++) {
            uint32_t r = hist + i * HISTORY_SIZE;
            m += rd32(g_mem, r + 4) == comp && (int)rd16(g_mem, r + 8) == year && !own_row(r, NULL);
        }
        if (m) {                /* the data rows of the start year's label (season_history_start.c) */
            uint32_t *at = malloc((count + 1) * sizeof *at);
            int *state = malloc((count + 1) * sizeof *state);
            m = 0;
            unsigned char *clear = malloc(count + 1);
            if (!at || !state || !clear) rt_fatal("season history: no memory");
            for (uint32_t i = 0; i < count; i++) {
                uint32_t r = hist + i * HISTORY_SIZE;
                if (rd32(g_mem, r + 4) != comp || (int)rd16(g_mem, r + 8) != year || own_row(r, NULL)) continue;
                at[m] = r;
                state[m++] = row_state(comp, r);
            }
            season_history_start_clear(m, state, clear);
            for (int j = 0; j < m; j++)
                if (clear[j]) {
                    wr32(g_mem, at[j] + 4, 0);
                    n++;
                }
            free(at);
            free(state);
            free(clear);
        }
        for (uint32_t i = 0; i < count; i++) {
            uint32_t r = hist + i * HISTORY_SIZE;
            if (rd32(g_mem, r + 4) != comp || (int)rd16(g_mem, r + 8) <= year || own_row(r, NULL)) continue;
            wr32(g_mem, r + 4, 0);
            n++;
        }
        if (n && getenv("RECOMP_HISTORY_LOG"))
            fprintf(stderr, "recomp: season history: %d data rows of label %d or later cleared before the game's "
                            "row\n", n, year);
    }
    /* A second edition in one calendar year: the label is held by a row of the game (every row from S + 1 on, after
     * the drop at the creation, or a row this program wrote) from another day. Not for a split season: two rows a
     * year in most of the 10 years before the start, or in the year before. */
    int label = year;
    /* The Irish calendar move (gap row 2 of 0.7.2, season_calendar.h): a league or cup that moves to calendar years
     * writes its edition in the short and the calendar seasons, so the short season shows "2002/03", the 2003 season
     * "2003", and no later label moves by one (the second-edition rule would give the 2003 season 2004 and each later
     * season the year after its own). */
    int fixed = obj ? season_calendar_history_label(obj) : 0;
    if (fixed) label = fixed;
    else if (on && !split_season(hist, count, comp, S) && rows_of_label(hist, count, comp, year - 1) < 2) {
        while (label < year + 8) {
            int game_row = 0, same_day = 0, any = 0;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t r = hist + i * HISTORY_SIZE, day = 0;
                if (rd32(g_mem, r + 4) != comp || (int)rd16(g_mem, r + 8) != label) continue;
                any = 1;
                int mine = own_row(r, &day);
                game_row |= mine || label >= S + 1;
                same_day |= mine && day == today;
            }
            if (!any || !game_row || same_day) break;
            label++;
        }
        if (label != year && getenv("RECOMP_HISTORY_LOG"))
            fprintf(stderr, "recomp: season history: a second edition in %d gets the label %d\n", year, label);
    }
    season_calendar_log_history(obj, label);
    Own o = {comp, (uint32_t)label, {rt_arg(1), rt_arg(2), rt_arg(3), rt_arg(4)}, today};
    if (label != year) wr16(g_mem, GAME_YEAR, (uint32_t)label);
    o_004afce0();
    if (label != year) wr16(g_mem, GAME_YEAR, (uint32_t)year);
    if (rt_unwinding || !comp) return;
    if (own_n == own_cap) {
        size_t cap = own_cap ? own_cap * 2 : 64;
        Own *m = realloc(own, cap * sizeof *m);
        if (!m) rt_fatal("season history: no memory for the game's rows");
        own = m;
        own_cap = cap;
    }
    own[own_n++] = o;
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

void season_history_log_create(void)
{
    log_rows("create");
}

void season_history_log_turn(void)
{
    static int done;
    if (done) return;
    done = 1;
    log_rows("turn");
}

/* 0.6.2 item 108: the title holder of a start season that began before a ruling took the title away
 * (season_titles.c). comp_util_get_latest_placed_club 0x4afd80, cdecl (competition record, char k): it sorts the
 * history rows by competition and by year down (0x945333, compare 0x540780: 0xffff matches any year,
 * 0x5407bc..0x5407c5), finds the competition (0x946464), walks back to its first row, the newest (0x4afde5..0x4afe02),
 * and returns +0x0a, +0x0e or +0x12 for k 0, 1 or 2 (0x4afe07..0x4afe2c). Its callers are the holder reads: the super
 * cups (ItaSuper::init_team_list 0x65fa60), the UEFA places of the cup winners (assign_uefa_cup_places 0x904580,
 * season_uefa_seeding.c) and others. When the newest row of the competition has the label year and the winner of the
 * table (the row before the one the ruling took away), it returns the club of that time with that name and nation.
 * A row the game writes later has a later label, so from then on the original's answer stands. The base game (2001
 * start) and RECOMP_SEASON_CODE=off have no row. */
void o_004afd80(void);

static uint32_t club_named(const char *want, const char *nation)
{
    uint32_t base = rd32(g_mem, CLUBS_PTR), n = rd32(g_mem, CLUB_COUNT);
    char name[64], nat[64];
    for (uint32_t i = 0; base && i < n && i < 200000u; i++) {
        uint32_t c = base + i * CLUB_SIZE, np;
        text_at(c + 4, name, sizeof name);
        if (strcmp(name, want)) continue;
        np = rd32(g_mem, c + 0x53);             /* a pointer after the id pass 0x5315de, an id before */
        text_at(np >= 0x10000u ? np + 4 : 0, nat, sizeof nat);
        if (!strcmp(nat, nation)) return c;
    }
    return 0;
}

/* The newest history row of competition record comp (the first after the sort of 0x4afd80), or 0. */
static uint32_t newest_row(uint32_t comp)
{
    uint32_t hist = rd32(g_mem, HISTORY_PTR), count = rd32(g_mem, HISTORY_COUNT), best = 0;
    for (uint32_t i = 0; hist && i < count; i++) {
        uint32_t r = hist + i * HISTORY_SIZE;
        if (rd32(g_mem, r + 4) == comp && (!best || (int16_t)rd16(g_mem, r + 8) > (int16_t)rd16(g_mem, best + 8)))
            best = r;
    }
    return best;
}

RT_WRAP(004afd80, comp_util_get_latest_placed_club)
{
    uint32_t rec = rt_arg(0);
    int k = (int8_t)rt_arg(1), S = season_start_year();
    o_004afd80();
    if (rt_unwinding || !season_code_on() || !rec || k < 0 || k > 1) return;
    char comp[64], row[64];
    text_at(rec + 4, comp, sizeof comp);
    if (!season_title_has(comp, S)) return;
    uint32_t r = newest_row(rec), winner = r ? rd32(g_mem, r + 0x0a) : 0;
    if (!winner) return;
    text_at(winner + 4, row, sizeof row);
    const char *nation = NULL, *then = season_title_then(comp, S, (int16_t)rd16(g_mem, r + 8), row, k, &nation);
    uint32_t club = then ? club_named(then, nation) : 0;
    static uint32_t logged[8][2];
    for (int i = 0; then && i < 8; i++)
        if (!logged[i][k] || logged[i][k] == rec) {
            if (!logged[i][k])
                fprintf(stderr, "recomp: history: %s %d, place %d: %s%s (the holder of that time; the newest row: %d %s)\n",
                        comp, S, k + 1, then, club ? "" : " not found, the newest row stands",
                        (int16_t)rd16(g_mem, r + 8), row);
            logged[i][k] = rec;
            break;
        }
    if (club) cpu.eax = club;
}
