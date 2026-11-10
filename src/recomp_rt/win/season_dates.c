/* Right ages and dates in every season (0.6.0 slice 11b, Q344): the year shifts of a new game (season_dates.h).
 * Evidence, measures and rules: docs/research/2026-10-17-season-date-shifts.md.
 *
 * Memory after the load 0x528540 (the loader's globals; the record layouts of include/database.h):
 *   index entries  0xae1a80, 22 rows of 0x43 bytes: +0x33 table id, +0x37 row count of the file
 *   staff          rows at [0xae23bc], 0x6e bytes, count [0xae2364]: the file rows first, then the reserve rows
 *   officials      rows at [0xae23e8], 0x2b bytes, count [0xae2390]
 *   staff history  rows at [0xae23cc], 0x11 bytes, count [0xae2374]; +0x04 the staff row (a pointer after the
 *                  id pass 0x5315a0), +0x08 u16 year
 *   comp history   rows at [0xae23d4], 0x1a bytes, count [0xae237c]: the club competition rows, then the nation
 *                  competition rows ([0xae23dc] points at the first of those); +0x04 the competition (a pointer
 *                  into the competitions at [0xae23d0], 0x6b bytes, club then nation competitions), +0x08 year
 * A date is 8 bytes: s16 day of the year, u16 year, u32 leap flag (Date.cpp, include/date.h). The year 1900
 * marks an empty date in the data; it stays. Only the rows of the files change, never the reserve rows.
 *
 * The rules for a game in season N (start year) with data of season D:
 * - a known Update Team database in its own season (N = D): the measured shifts of the table;
 * - a known Update Team database in another season: the whole database moves by N - D on top, so every person
 *   keeps the age that the database gives him and no history row lies after the start (a 2019/20 squad in a
 *   2010/11 game is not made of children, and its 2019 champions do not appear in 2010);
 * - the official data sets (3.9.60, 3.9.65, 3.9.68; D = 2001): real dates of birth, careers and history stay,
 *   and the contract ends move by N - 2001 (the 2002/03 tile: +1);
 * - a Custom database: the dates of birth are measured against the game's own data (3.9.68), shift d, so
 *   D = 2001 + d, and the rules of the Update Team databases apply (history only when D is 2010 or later,
 *   officials +1 when D is 2020 or later, the careers with the dates of birth). Too few matches: nothing moves.
 * - contract ends (+0x46) and national job dates (+0x29, +0x31), per row: an end in a year of D or later and
 *   2009 or later is already a real year and moves by N - D only; an earlier one is a year of a 2001 start and
 *   moves by N - 2001. No end lies more than 6 years after N (CONTRACT_YEARS).
 * - a date moved into a year that is not a leap year from a 29 February keeps a valid day (28 February); no
 *   year goes below 1901, so no date becomes empty. */
#include "win.h"
#include "season.h"
#include "season_dates.h"
#include "season_comps.h"
#include "season_history.h"
#include "season_records.h"
#include "seasons_db.h"
#include "season_screen.h"
#include "../host.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define INDEX 0xae1a80u
#define INDEX_ROWS 22u
#define INDEX_SIZE 0x43u
#define STAFF_PTR 0xae23bcu
#define STAFF_COUNT 0xae2364u
#define STAFF_SIZE 0x6eu
#define OFFICIALS_PTR 0xae23e8u
#define OFFICIALS_COUNT 0xae2390u
#define OFFICIAL_SIZE 0x2bu
#define HISTORY_PTR 0xae23ccu
#define HISTORY_COUNT 0xae2374u
#define HISTORY_SIZE 0x11u
#define COMP_HISTORY_PTR 0xae23d4u
#define COMP_HISTORY_COUNT 0xae237cu
#define NATION_HISTORY_PTR 0xae23dcu
#define COMP_HISTORY_SIZE 0x1au
#define COMPS_PTR 0xae23d0u
#define COMP_SIZE 0x6bu
#define CLUBS_PTR 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u

#define T_STAFF 6
#define T_OFFICIALS 7
#define T_FIRST_NAMES 13
#define T_SECOND_NAMES 14
#define T_HISTORY 17
#define T_COMP_HISTORY 19
#define T_NATION_HISTORY 20

#define EMPTY_YEAR 1900u
#define FIRST_REAL_YEAR 1901
#define LAST_ROW_YEAR 2001u        /* the history year where the Update Team databases keep the last champions */
#define REAL_CONTRACT_FROM 2010    /* a contract end of 2010 or later can be a real year: the ends of a 2001
                                    * start reach 2009 (3.9.68: 65 rows in 2008, 9 in 2009; September 2009,
                                    * D = 2009: 30 ends in 2009, a 2001-based tail) */
#define CONTRACT_YEARS 6           /* no contract end more than this many years after the start year */

static void log_load(void);
static void log_stats(void);

const SeasonDateShifts *season_dates_for(const char *name)
{
    for (int i = 0; name && i < season_date_shifts_count; i++)
        if (!strcmp(season_date_shifts[i].name, name)) return &season_date_shifts[i];
    return NULL;
}

static int is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

/* --- The journal: the stored bytes of every changed field, so a shift can be made again for another year. --- */

typedef struct { uint32_t addr; uint8_t len; uint8_t old[8]; } Change;
static Change *journal;
static size_t journal_n, journal_cap;

static void keep(uint32_t a, uint8_t len)
{
    if (journal_n == journal_cap) {
        size_t cap = journal_cap ? journal_cap * 2 : 1u << 16;
        Change *j = realloc(journal, cap * sizeof *j);
        if (!j) rt_fatal("season dates: no memory for the journal of %zu changes", cap);
        journal = j;
        journal_cap = cap;
    }
    Change *c = &journal[journal_n++];
    c->addr = a;
    c->len = len;
    memcpy(c->old, g_mem + a, len);
}

/* Gives back the stored bytes of every journaled field. No code of the original writes these fields between
 * the load and the creation of the game: Start New Game 0x81ce00 runs 0x5d8480 (Config::apply_and_save, the
 * settings object), 0x827d50 (the league flags of the nations, nation +0x11c, and the Select League(s) globals)
 * and 0x667150 (the league table of key_nation.cpp) after the load, and the Setup screens read the staff only.
 * So the bytes that come back are the bytes that the load gave (inference from those functions, src/cm3/setup.c
 * and src/cm3/key_nation.c port them; none touches staff +0x10..+0x4d, the officials or the history years). */
/* The competition history block when the shift journaled its years. Slice 11c grows that block after the
 * shift (season_history.c, the grow 0x527f60 reallocates it with 0x945667), so a restore finds the rows at the
 * new base: the journaled addresses inside the old block move by the same distance. */
static uint32_t journal_ch_base, journal_ch_end;

static void restore(void)
{
    uint32_t now = rd32(g_mem, COMP_HISTORY_PTR), moved = 0;
    while (journal_n) {
        Change *c = &journal[--journal_n];
        uint32_t a = c->addr;
        if (journal_ch_base && now != journal_ch_base && a >= journal_ch_base && a < journal_ch_end) {
            a = a - journal_ch_base + now;
            moved++;
        }
        memcpy(g_mem + a, c->old, c->len);
    }
    if (moved)
        fprintf(stderr, "recomp: season dates: %u history years restored in the moved history table (%08x, was "
                        "%08x)\n", moved, now, journal_ch_base);
    journal_ch_base = journal_ch_end = 0;
}

/* --- The loaded data and the shift that it holds. --- */

static int loaded;                  /* 1 from a database load to the end of the game or a saved game's load */
static char loaded_dir[1100];       /* the folder of that load ("" for the game's own Data) */
static int applied_year;            /* the start year whose shift the data holds (0: none) */
static int loaded_year;             /* the start year at that load */

typedef struct {
    const char *name;
    int D, N;                       /* data season, start year */
    int birth, joined, officials, careers, kept, history, last_rows, contract_old, contract_real;
    const SeasonDateShifts *row;    /* careers kept and last rows; NULL for none */
} Plan;

/* A date: adds s to its year when it is set, keeps a 29 February valid and the year at 1901 or later. */
static void shift_date(uint32_t a, int s)
{
    int y = (int)rd16(g_mem, a + 2);
    if (!s || y <= (int)EMPTY_YEAR) return;
    int ny = y + s;
    if (ny < FIRST_REAL_YEAR) ny = FIRST_REAL_YEAR;
    if (ny == y) return;
    keep(a, 8);
    wr16(g_mem, a + 2, (uint32_t)ny);
    /* A day of a leap year (flag set: the days of the leap table, 0x9b8460) in a year that is not one: the
     * days of the common table. 29 February becomes 28 February. The data writes flag 0 in most rows. */
    if (rd32(g_mem, a + 4) && !is_leap(ny)) {
        int d = (int16_t)rd16(g_mem, a);
        if (d >= 59) d--;
        wr16(g_mem, a, (uint32_t)d);
        wr32(g_mem, a + 4, 0);
    }
}

static void shift_year(uint32_t a, int s)
{
    int y = (int)rd16(g_mem, a);
    if (!s || y <= (int)EMPTY_YEAR) return;
    int ny = y + s < FIRST_REAL_YEAR ? FIRST_REAL_YEAR : y + s;
    if (ny == y) return;
    keep(a, 2);
    wr16(g_mem, a, (uint32_t)ny);
}

/* A contract end or national job date, by the rule of its row. */
static void shift_contract(uint32_t a, const Plan *p)
{
    int y = (int)rd16(g_mem, a + 2);
    if (y <= (int)EMPTY_YEAR) return;
    int real = y >= p->D && y >= REAL_CONTRACT_FROM;
    int ny = y + (real ? p->contract_real : p->contract_old);
    if (ny > p->N + CONTRACT_YEARS) ny = p->N + CONTRACT_YEARS;
    shift_date(a, ny - y);
}

/* The row count of a table in index.dat (the rows that came from the file). */
static uint32_t file_rows(int table)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < INDEX_ROWS; i++)
        if ((int32_t)rd32(g_mem, INDEX + i * INDEX_SIZE + 0x33) == table) n += rd32(g_mem, INDEX + i * INDEX_SIZE + 0x37);
    return n;
}

static uint32_t rows_of(int table, uint32_t count_at)
{
    uint32_t n = file_rows(table), have = rd32(g_mem, count_at);
    return n < have ? n : have;
}

static int hexval(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0;
}

/* 1 when the career of staff row i keeps the years of the data. */
static int career_kept(const SeasonDateShifts *s, uint32_t i)
{
    if (!s || !s->kept_hex || i >= s->kept_rows) return 0;
    const char *p = s->kept_hex + (i >> 3) * 2;
    int byte = hexval(p[0]) << 4 | hexval(p[1]);
    return byte >> (i & 7) & 1;
}

int season_dates_career_kept(const SeasonDateShifts *s, uint32_t i) { return career_kept(s, i); }

static int last_row_comp(const SeasonDateShifts *s, uint32_t id)
{
    for (uint32_t i = 0; s && i < s->last_count; i++)
        if (s->last_ids[i] == id) return 1;
    return 0;
}

static void apply(const Plan *p)
{
    uint32_t staff = rd32(g_mem, STAFF_PTR), n_staff = rows_of(T_STAFF, STAFF_COUNT);
    for (uint32_t i = 0; staff && i < n_staff; i++) {
        uint32_t r = staff + i * STAFF_SIZE;
        shift_date(r + 0x10, p->birth);
        shift_contract(r + 0x29, p);
        shift_contract(r + 0x31, p);
        shift_date(r + 0x3e, p->joined);
        shift_contract(r + 0x46, p);
    }
    uint32_t off = rd32(g_mem, OFFICIALS_PTR), n_off = rows_of(T_OFFICIALS, OFFICIALS_COUNT);
    for (uint32_t i = 0; off && i < n_off; i++) shift_date(off + i * OFFICIAL_SIZE + 0x0c, p->officials);

    uint32_t hist = rd32(g_mem, HISTORY_PTR), n_hist = rows_of(T_HISTORY, HISTORY_COUNT);
    uint32_t staff_all = rd32(g_mem, STAFF_COUNT);
    for (uint32_t i = 0; hist && i < n_hist; i++) {
        uint32_t r = hist + i * HISTORY_SIZE, who = rd32(g_mem, r + 4);
        uint32_t row = who >= staff && (who - staff) % STAFF_SIZE == 0 && (who - staff) / STAFF_SIZE < staff_all
                           ? (who - staff) / STAFF_SIZE : 0xffffffffu;
        shift_year(r + 8, career_kept(p->row, row) ? p->kept : p->careers);
    }

    /* The club rows and the nation rows of one array. */
    uint32_t ch = rd32(g_mem, COMP_HISTORY_PTR);
    uint32_t n_ch = file_rows(T_COMP_HISTORY) + file_rows(T_NATION_HISTORY), have = rd32(g_mem, COMP_HISTORY_COUNT);
    if (n_ch > have) n_ch = have;
    uint32_t nation_at = rd32(g_mem, NATION_HISTORY_PTR);
    if (nation_at && nation_at != ch + file_rows(T_COMP_HISTORY) * COMP_HISTORY_SIZE) {
        fprintf(stderr, "recomp: season dates: the nation competition history is not after the club rows "
                        "(%08x, %08x): only the club rows move\n", ch, nation_at);
        n_ch = file_rows(T_COMP_HISTORY);
    }
    uint32_t comps = rd32(g_mem, COMPS_PTR);
    journal_ch_base = ch;
    journal_ch_end = ch + rd32(g_mem, COMP_HISTORY_COUNT) * COMP_HISTORY_SIZE;
    for (uint32_t i = 0; ch && i < n_ch; i++) {
        uint32_t r = ch + i * COMP_HISTORY_SIZE, c = rd32(g_mem, r + 4);
        int last = p->last_rows && rd16(g_mem, r + 8) == LAST_ROW_YEAR && c >= comps && (c - comps) % COMP_SIZE == 0 &&
                   last_row_comp(p->row, (c - comps) / COMP_SIZE);
        shift_year(r + 8, last ? p->last_rows : p->history);
    }
    fprintf(stderr, "recomp: season dates: %s (data %d) in %d: birth %+d, joined %+d, contract %+d (real ends "
                    "%+d), officials %+d, careers %+d (kept %+d), competition history %+d (last rows %+d); %u staff, "
                    "%u careers, %u history rows, %zu changes\n", p->name, p->D, p->N, p->birth, p->joined,
            p->contract_old, p->contract_real, p->officials, p->careers, p->kept, p->history, p->last_rows, n_staff,
            n_hist, n_ch, journal_n);
}

/* --- A Custom database: the date of birth shift measured against the game's own data (3.9.68). --- */

typedef struct { uint64_t key; int16_t year; } Birth;

static uint64_t fnv(uint64_t h, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n && p[i]; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h * 0x100000001b3ull;
}

static uint64_t birth_key(const uint8_t *first, const uint8_t *second, int16_t day)
{
    uint64_t h = fnv(0xcbf29ce484222325ull, first, 51);
    h = fnv(h, second, 51);
    h = (h ^ (uint16_t)day) * 0x100000001b3ull;
    return h | 1;                   /* 0 marks a free slot */
}

/* The file of a folder by its name without regard to case, as files.c finds the files of the game. */
static uint8_t *read_ci(const char *dir, const char *name, long *size)
{
    char path[1300];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    HostStat st;
    if (host_stat(path, &st) != 0) {
        HostDir *d = host_opendir(dir);
        const char *e;
        path[0] = 0;
        while (d && (e = host_readdir(d)))
            if (!strcasecmp(e, name)) { snprintf(path, sizeof path, "%s/%s", dir, e); break; }
        if (d) host_closedir(d);
        if (!path[0]) return NULL;
    }
    int fd = host_open(path, O_RDONLY, 0);
    if (fd < 0) return NULL;
    long n = (long)host_seek(fd, 0, SEEK_END);
    uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
    if (buf && (host_seek(fd, 0, SEEK_SET) != 0 || host_read(fd, buf, (uint32_t)n) != n)) {
        free(buf);
        buf = NULL;
    }
    host_close(fd);
    *size = n;
    return buf;
}

/* The staff rows of a data folder with their names: the files, read once. */
typedef struct {
    uint8_t *index, *staff, *first, *second;
    long n_index, n_staff, n_first, n_second;
    uint32_t off, count, size;
} People;

static void people_free(People *p) { free(p->index); free(p->staff); free(p->first); free(p->second); }

static int people_read(const char *dir, People *p)
{
    memset(p, 0, sizeof *p);
    p->index = read_ci(dir, "index.dat", &p->n_index);
    p->staff = read_ci(dir, "staff.dat", &p->n_staff);
    p->first = read_ci(dir, "first_names.dat", &p->n_first);
    p->second = read_ci(dir, "second_names.dat", &p->n_second);
    if (!p->index || !p->staff || !p->first || !p->second || p->n_index < 8 + 22 * 0x43) return 0;
    for (int e = 0; e < 22; e++) {
        const uint8_t *q = p->index + 8 + e * 0x43;
        int32_t id, cnt, at, fmt;
        memcpy(&id, q + 0x33, 4); memcpy(&cnt, q + 0x37, 4); memcpy(&at, q + 0x3b, 4); memcpy(&fmt, q + 0x3f, 4);
        if (id == T_STAFF) { p->off = (uint32_t)at; p->count = (uint32_t)cnt; p->size = fmt == 2 ? 0x6e : 0x9d; }
    }
    return p->count && (uint64_t)p->off + (uint64_t)p->count * p->size <= (uint64_t)p->n_staff;
}

/* The key of row i, or 0 for a row with no set date of birth. *year gets the year. */
static uint64_t people_key(const People *p, uint32_t i, int *year)
{
    const uint8_t *r = p->staff + p->off + (size_t)i * p->size;
    int32_t f, s; int16_t day; uint16_t y;
    memcpy(&f, r + 4, 4); memcpy(&s, r + 8, 4); memcpy(&day, r + 0x10, 2); memcpy(&y, r + 0x12, 2);
    if (y <= EMPTY_YEAR || f < 0 || s < 0 || (long)f >= p->n_first / 0x3c || (long)s >= p->n_second / 0x3c) return 0;
    *year = y;
    return birth_key(p->first + (size_t)f * 0x3c, p->second + (size_t)s * 0x3c, day);
}

#define BIRTH_SLOTS (1u << 19)
#define MEASURE_MIN 2000           /* matched people that a measure needs */

/* The most common year difference of the people (first name, second name, day of birth) that the folder and
 * the game's own Data share. 1 with *shift, 0 when it cannot be measured. */
static int measure_folder(const char *folder, int *shift)
{
    char game_data[1100];
    snprintf(game_data, sizeof game_data, "%s/Data", win_game_dir);
    const SeasonKnownDb *ref = seasons_db_identify(game_data);
    if (!ref || strcmp(ref->name, "3.9.68")) {
        fprintf(stderr, "recomp: season dates: the game's own data is not the 3.9.68 data, so a Custom database "
                        "cannot be checked\n");
        return 0;
    }
    People a, b;
    Birth *tab = calloc(BIRTH_SLOTS, sizeof *tab);
    int ok = 0;
    if (!tab || !people_read(game_data, &a) || !people_read(folder, &b)) goto done;
    for (uint32_t i = 0; i < a.count; i++) {
        int year;
        uint64_t k = people_key(&a, i, &year);
        if (!k) continue;
        uint32_t h = (uint32_t)(k >> 20) & (BIRTH_SLOTS - 1);
        for (uint32_t probe = 0; probe < BIRTH_SLOTS; probe++, h = (h + 1) & (BIRTH_SLOTS - 1)) {
            if (!tab[h].key) { tab[h].key = k; tab[h].year = (int16_t)year; break; }
            if (tab[h].key == k) { tab[h].year = -1; break; }      /* a name seen twice does not count */
        }
    }
    uint32_t hist[81] = {0}, total = 0;       /* differences -20 to +60 */
    for (uint32_t i = 0; i < b.count; i++) {
        int year;
        uint64_t k = people_key(&b, i, &year);
        if (!k) continue;
        uint32_t h = (uint32_t)(k >> 20) & (BIRTH_SLOTS - 1);
        for (uint32_t probe = 0; probe < BIRTH_SLOTS && tab[h].key; probe++, h = (h + 1) & (BIRTH_SLOTS - 1)) {
            if (tab[h].key != k) continue;
            int d = tab[h].year - year;
            if (tab[h].year > 0 && d >= -20 && d <= 60) { hist[d + 20]++; total++; }
            break;
        }
    }
    int best = 0;
    for (int d = 1; d < 81; d++) if (hist[d] > hist[best]) best = d;
    fprintf(stderr, "recomp: season dates: Custom database: %u people also in the game's data, %u of them %+d "
                    "years\n", total, hist[best], best - 20);
    if (total >= MEASURE_MIN && hist[best] * 10 >= total * 8) {
        *shift = best - 20;
        ok = 1;
    }
done:
    people_free(&a); people_free(&b); free(tab);
    return ok;
}

int season_dates_custom_check(const char *folder, int *shift)
{
    static struct { char path[1100]; uint64_t size; int64_t mtime; int ok, shift; } cache[8];
    static int used;
    char staff[1200];
    HostStat st;
    snprintf(staff, sizeof staff, "%s/staff.dat", folder);
    if (host_stat(staff, &st) != 0) memset(&st, 0, sizeof st);
    for (int i = 0; i < used; i++)
        if (!strcmp(cache[i].path, folder) && cache[i].size == st.size && cache[i].mtime == st.mtime) {
            *shift = cache[i].shift;
            return cache[i].ok;
        }
    int d = 0, ok = measure_folder(folder, &d);
    int slot = used < 8 ? used++ : 0;
    snprintf(cache[slot].path, sizeof cache[slot].path, "%s", folder);
    cache[slot].size = st.size; cache[slot].mtime = st.mtime; cache[slot].ok = ok; cache[slot].shift = d;
    *shift = d;
    return ok;
}

/* The plan for the loaded data in start year N. 0 when nothing moves. */
static int plan_for_folder(const char *dir, int N, Plan *p)
{
    memset(p, 0, sizeof *p);
    p->N = N;
    char folder[1100];
    if (dir[0]) snprintf(folder, sizeof folder, "%s", dir);
    else snprintf(folder, sizeof folder, "%s/Data", win_game_dir);
    const SeasonKnownDb *k = seasons_db_identify(folder);
    if (k && (k->role == SEASONS_ROLE_OFFICIAL_BASE || k->role == SEASONS_ROLE_OFFICIAL_VERSION)) {
        p->name = k->name;
        p->D = SEASON_FIRST_YEAR;
        p->contract_old = p->contract_real = N - SEASON_FIRST_YEAR;
        return 1;
    }
    const SeasonDateShifts *row = k ? season_dates_for(k->name) : NULL;
    if (row && k->season >= SEASON_FIRST_YEAR) {
        int e = N - k->season;
        p->name = k->name;
        p->D = k->season;
        p->row = row;
        p->birth = row->birth + e;
        p->joined = row->joined + e;
        p->officials = row->officials + e;
        p->careers = row->staff_history + e;
        p->kept = e;
        p->history = row->comp_history + e;
        p->last_rows = row->last_rows ? row->last_rows + e : 0;
        p->contract_old = row->contract + e;
        p->contract_real = e;
        return 1;
    }
    if (k) return 0;                        /* the Empty database */
    int d = 0;
    if (!season_dates_custom_check(folder, &d)) {
        fprintf(stderr, "recomp: season dates: the ages of this Custom database could not be checked: nothing moves\n");
        return 0;
    }
    int D = SEASON_FIRST_YEAR + d, e = N - D;
    p->name = "Custom database";
    p->D = D;
    p->birth = p->joined = p->careers = d + e;
    p->kept = e;
    p->officials = (D >= 2020) + e;
    p->history = (D >= 2010 ? d : 0) + e;
    p->contract_old = d + e;
    p->contract_real = e;
    return 1;
}

static int plan_for(int N, Plan *p) { return plan_for_folder(loaded_dir, N, p); }

int season_dates_plan(const char *folder, int N, SeasonShiftPlan *out)
{
    Plan p;
    memset(out, 0, sizeof *out);
    if (N == SEASON_FIRST_YEAR || !plan_for_folder(folder, N, &p)) {
        out->D = SEASON_FIRST_YEAR;
        out->N = N;
        return 0;
    }
    out->D = p.D;
    out->N = p.N;
    out->birth = p.birth;
    out->joined = p.joined;
    out->careers = p.careers;
    out->kept = p.kept;
    out->contract_old = p.contract_old;
    out->contract_real = p.contract_real;
    out->row = p.row;
    return 1;
}

static void shift_for(int N)
{
    season_history_undo();          /* slice 11c: its rows first, then the shift that they follow */
    restore();
    applied_year = 0;
    Plan p;
    if (N == SEASON_FIRST_YEAR || !season_code_on() || !plan_for(N, &p)) return;
    apply(&p);
    applied_year = N;
    log_stats();
    season_history_fill(p.name, N); /* slice 11c: the history between 2001 and the start */
}

void season_dates_forget(void)
{
    season_history_forget();
    journal_ch_base = journal_ch_end = 0;
    loaded = 0;
    applied_year = 0;
    journal_n = 0;
}

void season_dates_at_create(void)
{
    if (!loaded || rt_unwinding) return;
    int N = season_start_year();
    if (N == (applied_year ? applied_year : SEASON_FIRST_YEAR)) return;
    fprintf(stderr, "recomp: season dates: the game is made in %d, the data was loaded for %d: the shift is made "
                    "again\n", N, applied_year ? applied_year : SEASON_FIRST_YEAR);
    uint32_t eax = cpu.eax;
    shift_for(N);
    loaded_year = N;
    log_load();
    cpu.eax = eax;
}

void o_00524d50(void);

/* The load of the database files, cdecl (path, flag): 0x528540 with the file source. Called by Start New Game
 * (0x81ce00, only when no nation is loaded) and two other new game paths (0x827110, 0x828110); a saved game
 * loads through 0x524d10 (the stream source) and is not touched. Nonzero when the data is loaded. */
RT_WRAP(00524d50, database_load_files)
{
    season_records_before_load(win_data_set_dir(), season_start_year());   /* slice 15b: the merged files */
    o_00524d50();
    season_records_after_load();
    if (rt_unwinding) return;
    season_dates_forget();
    if (!cpu.eax) return;
    uint32_t eax = cpu.eax;
    loaded = 1;
    loaded_year = season_start_year();
    snprintf(loaded_dir, sizeof loaded_dir, "%s", win_data_set_dir());
    fprintf(stderr, "recomp: season dates: data loaded from %s in %d\n", loaded_dir[0] ? "a database folder"
            : "the game's Data", season_start_year());
    shift_for(season_start_year());
    /* Test switch RECOMP_DATES_RESHIFT=<year>: the shift is made again for that year and then for the start year,
     * as season_dates_at_create does when the game is made in another year than the load. The data must end as
     * after one shift (tests/seasons/season_history_run.py, run "reshift"). */
    const char *re = getenv("RECOMP_DATES_RESHIFT");
    if (re && atoi(re) > SEASON_FIRST_YEAR && season_start_year() != SEASON_FIRST_YEAR) {
        fprintf(stderr, "recomp: season dates: test shift for %d, then %d again\n", atoi(re), season_start_year());
        shift_for(atoi(re));
        shift_for(season_start_year());
    }
    log_load();
    cpu.eax = eax;
}

#define FREE_TABLES 0x00528080u    /* database_free_all_tables: frees every table, [0xae23a8] becomes 0 */

void o_0081ce00(void);

/* Start New Game, cdecl, no arguments: it loads the database only when no nation is loaded ([0xae23a8] is 0,
 * 0x81ce06). After Back from Select League(s) the data of the first start stays in memory. When the player has
 * chosen another season or database since that load, the tables are freed first, so this start loads the data
 * of its own season and database. The original frees the same way in two places: load_game frees the tables
 * before it reads a save (0x82abe1..0x82abf2: when [0xae23a8] is set, 0x528080, then 0x524d10), and the loader
 * frees the old tables itself (0x528540 calls 0x528080 at 0x5285a8). After the load 0x81ce00 builds the Select
 * League(s) globals again (0x827d50, then key_nation_build_table 0x667150 with the year of the game date, called
 * at 0x81cf32 and 0x81cf37). Not while a game runs. The base game never gets here with data in memory from
 * another choice. */
RT_WRAP(0081ce00, start_new_game)
{
    season_screen_apply_page_choice();                     /* the browser build: Use of the Saved games dialog */
    if (rt_unwinding) return;
    if (loaded && !season_game_on() && season_code_on() && rd32(g_mem, 0xae23a8u) &&
        (loaded_year != season_start_year() || strcmp(loaded_dir, win_data_set_dir()) ||
         season_comps_records_stale())) {                  /* slice 26: the records of new competitions */
        fprintf(stderr, "recomp: season dates: the data of %d (%s) is freed for a start in %d with %s\n", loaded_year,
                loaded_dir[0] ? "a database folder" : "the game's Data", season_start_year(),
                win_data_set_dir()[0] ? "a database folder" : "the game's Data");
        season_dates_forget();
        rt_callback(FREE_TABLES, 0, NULL);
        if (rt_unwinding) return;
    }
    o_0081ce00();
}

/* Test log of the data after a shift: how many contract ends lie from 2 years before the start to 6 years after, the
 * latest joined year, the latest career year, the officials born from 1975 on and the set national job dates. */
static void log_stats(void)
{
    if (!getenv("RECOMP_DATES_LOG")) return;
    int N = season_start_year();
    uint32_t staff = rd32(g_mem, STAFF_PTR), n = rows_of(T_STAFF, STAFF_COUNT);
    uint32_t set = 0, inside = 0, nat_set = 0, nat_inside = 0;
    int joined_max = 0, career_max = 0;
    uint32_t joined_n[3] = {0, 0, 0};
    for (uint32_t i = 0; staff && i < n; i++) {
        uint32_t r = staff + i * STAFF_SIZE;
        int c = (int)rd16(g_mem, r + 0x48), j = (int)rd16(g_mem, r + 0x40), m = (int)rd16(g_mem, r + 0x33);
        /* From N - 2: a 2001 start has contract ends of 1999 and 2000 too (they end before the start) */
        if (c > (int)EMPTY_YEAR && rd32(g_mem, r + 0x39)) { set++; inside += c >= N - 2 && c <= N + CONTRACT_YEARS; }
        if (m > (int)EMPTY_YEAR) { nat_set++; nat_inside += m >= N - 2 && m <= N + CONTRACT_YEARS; }
        if (j > (int)EMPTY_YEAR && rd32(g_mem, r + 0x39)) {
            if (j > joined_max && j < 2100) joined_max = j;
            joined_n[j >= N - 1 && j <= N ? 0 : j < N - 1 ? 1 : 2]++;
        }
    }
    uint32_t hist = rd32(g_mem, HISTORY_PTR), n_hist = rows_of(T_HISTORY, HISTORY_COUNT);
    for (uint32_t i = 0; hist && i < n_hist; i++) {
        int y = (int)rd16(g_mem, hist + i * HISTORY_SIZE + 8);
        if (y > career_max && y < 2100) career_max = y;
    }
    uint32_t off = rd32(g_mem, OFFICIALS_PTR), n_off = rows_of(T_OFFICIALS, OFFICIALS_COUNT), young = 0, born = 0;
    for (uint32_t i = 0; off && i < n_off; i++) {
        int y = (int)rd16(g_mem, off + i * OFFICIAL_SIZE + 0x0e);
        if (y > (int)EMPTY_YEAR) { born++; young += y >= 1975; }
    }
    fprintf(stderr, "recomp: dates log: stats contracts %u of %u in %d to %d; national dates %u of %u; joined max %d, "
                    "%u in %d or %d, %u earlier, %u later; career max %d; officials born from 1975 %u of %u\n",
            inside, set, N - 2, N + CONTRACT_YEARS, nat_inside, nat_set, joined_max, joined_n[0], N - 1, N, joined_n[1],
            joined_n[2], career_max, young, born);
}

static const int16_t month_start[2][12] = {
    {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333},     /* 0x9b8448 */
    {-1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},     /* 0x9b8460 */
};

static void day_month(uint32_t a, int *day, int *month)
{
    int16_t d = (int16_t)rd16(g_mem, a);
    int leap = rd32(g_mem, a + 4) != 0;
    int m = 11;
    while (m > 0 && d <= month_start[leap][m]) m--;
    *month = m + 1;
    *day = d - month_start[leap][m];
}

static void text_at(uint32_t a, char *out, size_t room)
{
    size_t i = 0;
    /* The log is plain ASCII: a letter of code page 1252 above 0x7f becomes '?'. */
    if (a) for (; i + 1 < room && g_mem[a + i]; i++) out[i] = g_mem[a + i] < 0x80 ? (char)g_mem[a + i] : '?';
    out[i] = 0;
}

/* The first and last career years of each staff row with this name. They are read at the load: at the first
 * turn the staff history rows no longer point at these staff rows (seen in the test; inference: the new game
 * builds its own career records from the table). */
static void log_careers(const char *want)
{
    uint32_t staff = rd32(g_mem, STAFF_PTR), n = rows_of(T_STAFF, STAFF_COUNT);
    uint32_t hist = rd32(g_mem, HISTORY_PTR), n_hist = rows_of(T_HISTORY, HISTORY_COUNT);
    for (uint32_t i = 0; staff && i < n; i++) {
        uint32_t r = staff + i * STAFF_SIZE;
        char first[64], second[64], full[140];
        text_at(rd32(g_mem, r + 4), first, sizeof first);
        text_at(rd32(g_mem, r + 8), second, sizeof second);
        snprintf(full, sizeof full, "%s %s", first, second);
        if (strcmp(full, want)) continue;
        int day, month, lo = 9999, hi = 0;
        day_month(r + 0x10, &day, &month);
        for (uint32_t h = 0; hist && h < n_hist; h++) {
            uint32_t hr = hist + h * HISTORY_SIZE;
            if (rd32(g_mem, hr + 4) != r) continue;
            int y = (int)rd16(g_mem, hr + 8);
            if (y < lo) lo = y;
            if (y > hi) hi = y;
        }
        fprintf(stderr, "recomp: dates log: career %s born %d.%d: %d to %d\n", full, day, month,
                lo == 9999 ? 0 : lo, hi);
    }
}

static void log_person(const char *want)
{
    uint32_t staff = rd32(g_mem, STAFF_PTR), n = rows_of(T_STAFF, STAFF_COUNT);
    uint32_t clubs = rd32(g_mem, CLUBS_PTR), n_clubs = rd32(g_mem, CLUB_COUNT);
    int found = 0;
    for (uint32_t i = 0; staff && i < n; i++) {
        uint32_t r = staff + i * STAFF_SIZE;
        char first[64], second[64], full[140];
        text_at(rd32(g_mem, r + 4), first, sizeof first);
        text_at(rd32(g_mem, r + 8), second, sizeof second);
        snprintf(full, sizeof full, "%s %s", first, second);
        if (strcmp(full, want)) continue;
        found++;
        int day, month;
        day_month(r + 0x10, &day, &month);
        char club[64] = "";
        uint32_t c = rd32(g_mem, r + 0x39);
        if (clubs && c >= clubs && c < clubs + n_clubs * CLUB_SIZE) text_at(c + 4, club, sizeof club);
        fprintf(stderr, "recomp: dates log: person %s: born %d.%d.%d, age %d, joined %d, contract %d, club %s\n",
                full, day, month, (int)rd16(g_mem, r + 0x12), (int16_t)rd16(g_mem, r + 0x18),
                (int)rd16(g_mem, r + 0x40), (int)rd16(g_mem, r + 0x48), club[0] ? club : "none");
    }
    if (!found) fprintf(stderr, "recomp: dates log: person %s: not found\n", want);
}

/* An official (officials.dat: +0x04 first name, +0x08 second name, +0x0c date of birth). */
static void log_official(const char *want)
{
    uint32_t off = rd32(g_mem, OFFICIALS_PTR), n = rows_of(T_OFFICIALS, OFFICIALS_COUNT);
    int found = 0;
    for (uint32_t i = 0; off && i < n; i++) {
        uint32_t r = off + i * OFFICIAL_SIZE;
        char first[64], second[64], full[140];
        text_at(rd32(g_mem, r + 4), first, sizeof first);
        text_at(rd32(g_mem, r + 8), second, sizeof second);
        snprintf(full, sizeof full, "%s %s", first, second);
        if (strcmp(full, want)) continue;
        int day, month;
        day_month(r + 0x0c, &day, &month);
        fprintf(stderr, "recomp: dates log: official %s: born %d.%d.%d\n", full, day, month, (int)rd16(g_mem, r + 0x0e));
        found++;
    }
    if (!found) fprintf(stderr, "recomp: dates log: official %s: not found\n", want);
}

/* At the end of a database load: the careers of the people of RECOMP_DATES_LOG. */
static void log_load(void)
{
    const char *v = getenv("RECOMP_DATES_LOG");
    if (!v || !*v) return;
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", v);
    for (char *p = strtok(buf, ";"); p; p = strtok(NULL, ";"))
        if (strncmp(p, "comp:", 5) && strncmp(p, "ref:", 4)) log_careers(p);
}

static void log_comp(const char *want)
{
    uint32_t ch = rd32(g_mem, COMP_HISTORY_PTR), n = rd32(g_mem, COMP_HISTORY_COUNT);
    uint32_t clubs = rd32(g_mem, CLUBS_PTR), n_clubs = rd32(g_mem, CLUB_COUNT);
    int years[3] = {0, 0, 0};
    char winners[3][64] = {"", "", ""};
    for (uint32_t i = 0; ch && i < n; i++) {
        uint32_t r = ch + i * COMP_HISTORY_SIZE, comp = rd32(g_mem, r + 4);
        char name[64];
        text_at(comp ? comp + 4 : 0, name, sizeof name);
        if (strcmp(name, want)) continue;
        int y = (int)rd16(g_mem, r + 8);
        for (int k = 0; k < 3; k++) {
            if (y <= years[k]) continue;
            for (int j = 2; j > k; j--) { years[j] = years[j - 1]; memcpy(winners[j], winners[j - 1], 64); }
            years[k] = y;
            uint32_t c = rd32(g_mem, r + 0xa);
            winners[k][0] = 0;
            if (clubs && c >= clubs && c < clubs + n_clubs * CLUB_SIZE) text_at(c + 4, winners[k], 64);
            break;
        }
    }
    fprintf(stderr, "recomp: dates log: competition %s: %d %s; %d %s; %d %s\n", want, years[0], winners[0],
            years[1], winners[1], years[2], winners[2]);
}

void season_dates_log_turn(void)
{
    static int done;
    const char *v = getenv("RECOMP_DATES_LOG");
    if (done || !v || !*v) return;
    done = 1;
    char shifts[32];
    snprintf(shifts, sizeof shifts, applied_year ? "for %d" : "none", applied_year);
    fprintf(stderr, "recomp: dates log: date %d/%d, start year %d, shifts %s\n", (int16_t)rd16(g_mem, 0xae2c90u),
            (int16_t)rd16(g_mem, 0xae2c92u), season_start_year(), shifts);
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", v);
    for (char *p = strtok(buf, ";"); p; p = strtok(NULL, ";")) {
        if (!strncmp(p, "comp:", 5)) log_comp(p + 5);
        else if (!strncmp(p, "ref:", 4)) log_official(p + 4);
        else log_person(p);
    }
}
