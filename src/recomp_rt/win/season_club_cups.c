/* New club cups of slice 26 (0.6.0, part 26c): the Copa Sudamericana, the Recopa Sudamericana, the Suruga Bank
 * Championship, the FIFA Intercontinental Cup and the AFC Champions League Two, through the new competitions
 * module (season_comps.h). Research: docs/research/2026-10-19-club-cups.md.
 *
 * Each is an object of the World_Club_Cup class (constructor 0x92b4b0, thiscall (year, record), ret 8, size 0xb2,
 * vtable 0x971250, Cup base 0x518640), the class of the Intercontinental Cup: a plain Cup, which plays any club
 * count and lets clubs enter in a later round (map_fixture_tree 0x519265..0x519504). Its constructor and its
 * start_new_season 0x92bda0 call the club picker 0x92b8b0 (0x92b53e, 0x92be50) and the round table, vtable +0x3c
 * 0x92b770 (0x92b586, and slot 35 0x858e70 at a season start). The wraps of both test the object's record (+4):
 * for a def's record they give the def's clubs and rounds, for every other object the original's. So the
 * original's Intercontinental Cup (record "Intercontinental Cup") is unchanged.
 *
 * The rounds: a knockout of the table's club count, with byes into the second round when the count is not a power
 * of two, one or two legs as the table's format says, on the days of the table's format text. The Intercontinental
 * Cup is the table's stepladder of four single matches. The clubs:
 *   - Copa Sudamericana: on each CONMEBOL seeding (assign_libertadores_places 0x4c6430, 10 January and the new game),
 *     the best clubs of each South American nation below the Libertadores places get the record's id in club
 *     +0x1db, as the Libertadores places do (0x4c66bc..0x4c66fc). Argentina and Brazil take the places of the
 *     table's row, the other nations share the rest. The picker takes the marked clubs.
 *   - Recopa: the Libertadores and the Sudamericana winners of the year before; Suruga Bank: the J.League Cup and
 *     the Sudamericana winners of the year before; Intercontinental Cup: the winners of the year of the OFC, AFC,
 *     CAF, CONMEBOL, CONCACAF and UEFA club competitions. Read from the history rows by record and year.
 *   - AFC Champions League Two: Asian clubs of the top reputations.
 * A club that the history or the marks cannot give comes from WorldClubChamp::pick_entrant 0x92ae60 (a continent's
 * clubs of the top reputations, formats doc 5.2). The picker runs again on the day before the first draw (a vtable
 * copy with its own update_for_date, slot 5), when the winners and the marks of the year are known.
 *
 * Years: the Sudamericana, the Recopa and the Suruga Bank match play one edition a calendar year; the object's
 * +0x40 is that year. The edition takes the dates of the row part (separated by "||") whose first date lies in that
 * year, from the row of that season or the one before, so the two editions of 2016/17 come from the dates. The
 * Intercontinental Cup and ACL Two play one edition a season. A year with no edition: the before hook sets +0x40
 * to the year of the next edition - 1 (start_new_season adds 1, 0x92be3e), or skips for a cup that never returns
 * (season_comps.h, the shared rule). Never in the base game (season_comps_on). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YEAR_AT 0x40u                   /* s16 */
#define RECORD_AT 4u                    /* the record of the object */
#define CLUB_COUNT_AT 0x3eu             /* s16: the Cup's club count (0x92b8c2) */
#define CLUB_LIST_AT 0xa7u              /* 6 bytes a club: pointer, seed, fate (Cup_Stage doc, formats doc 3.1) */
#define ROUNDS_AT 0xa3u                 /* the Cup's round table */
#define ROUND_COUNT_AT 0x3cu            /* s16 */
#define ROUND_SIZE 0x68u
#define CUP_VTABLE 0x971250u            /* World_Club_Cup */
#define CUP_SIZE 0xb2u
#define COMP_RECORDS 0xae23d0u
#define RECORD_SIZE 0x6bu
#define HISTORY 0xae23d4u               /* rows of 0x1a: +4 record, +8 year, +0xa winner, +0xe runner-up */
#define HISTORY_COUNT 0xae237cu
#define HISTORY_ROW 0x1au
#define CLUBS 0xae23b4u                 /* the club table, 0x245 bytes a club */
#define CLUB_TABLE_COUNT 0xae235cu
#define CLUB_ROW 0x245u
#define CLUB_CONTINENTAL 0x1dbu         /* the record id of the club's continental cup, -1 none (0x4c6377) */
#define CLUB_ROUND 0x1dfu               /* byte: its entry round (0x4c66ef) */
#define GAME_DAY 0xae2c90u              /* s16 day of the year from 0, s16 year */
#define NEVER 2999

/* Key ids (sweep-europe 1.1, research doc): the record id of each competition. */
#define KEY_LIBERTADORES 0x9cf63cu
#define KEY_CHAMPIONS_LEAGUE 0x9cf6e8u
#define KEY_CONCACAF 0x9cf72cu
#define KEY_ASIA 0x9cf64cu
#define KEY_OCEANIA 0x9cf7ccu
#define KEY_JAPAN_CUP 0x9cf664u
#define KEY_MEXICO 0x9cf3d0u            /* a nation in the CONMEBOL seeding rows (0x4c614c) */
#define KEY_ARGENTINA 0x9cf218u
#define KEY_BRAZIL 0x9cf260u
/* The continents of pick_entrants (0x92a9e3..0x92ab5a). */
#define CONT_AFRICA 0x9cfa08u
#define CONT_ASIA 0x9cfa0cu
#define CONT_EUROPE 0x9cfa10u
#define CONT_NORTH_AMERICA 0x9cfa14u
#define CONT_OCEANIA 0x9cfa18u
#define CONT_SOUTH_AMERICA 0x9cfa1cu

#define MAX_CLUBS 64

typedef struct ClubCup ClubCup;
struct ClubCup {
    const SeasonCompDef *def;
    int calendar;                       /* 1: one edition a calendar year; 0: one a season */
    int skip_first_clause;              /* 1: the dates start after the first ";" (ACL Two: its qualifying) */
    int (*pick)(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n);
};

static uint32_t make_cup(const SeasonCompDef *d, uint32_t rec, int year, int loading);
static int pick_sudamericana(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n);
static int pick_recopa(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n);
static int pick_suruga(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n);
static int pick_intercontinental(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n);
static int pick_acl_two(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n);

/* The defs (season_comps.c lists them). Entries: 2 the world (20 June, always made), 5 South America (10 January),
 * 6 Asia (28 August). */
const SeasonCompDef season_cup_sudamericana = {"sudamericana", "new_sudamericana", "South American CONMEBOL Cup", 5,
                                               make_cup, NULL, NULL, 0};
const SeasonCompDef season_cup_recopa = {"recopa", "new_recopa", "South American Recopa", 5, make_cup, NULL, NULL, 0};
const SeasonCompDef season_cup_suruga_bank = {"suruga_bank", "new_suruga_bank", "Suruga Bank Championship", 2,
                                              make_cup, "Intercontinental Cup", "Suruga Bank Championship", 10};
const SeasonCompDef season_cup_intercontinental = {"fifa_intercontinental_cup", "new_fifa_intercontinental_cup",
                                                   "FIFA Intercontinental Cup", 2, make_cup,
                                                   "FIFA Club World Championship", "FIFA Intercontinental Cup", 20};
const SeasonCompDef season_cup_acl_two = {"acl_two", "new_acl_two", "AFC Champions League Two", 6, make_cup,
                                          "Asian Club Championship", "ACL Two", 10};

static const ClubCup cups[] = {
    {&season_cup_sudamericana, 1, 0, pick_sudamericana},
    {&season_cup_recopa, 1, 0, pick_recopa},
    {&season_cup_suruga_bank, 1, 0, pick_suruga},
    {&season_cup_intercontinental, 0, 0, pick_intercontinental},
    {&season_cup_acl_two, 0, 1, pick_acl_two},
};
#define CUP_COUNT ((int)(sizeof cups / sizeof cups[0]))

/* The cup whose record the object holds, or NULL (every object of the original). */
static const ClubCup *cup_of_object(uint32_t obj)
{
    if (!season_comps_on() || !obj) return NULL;
    uint32_t rec = rd32(g_mem, obj + RECORD_AT);
    for (int i = 0; i < CUP_COUNT; i++) {
        uint32_t o = season_comps_object(cups[i].def);
        if (o == obj) return &cups[i];
        if (rec && rec == season_comps_record(cups[i].def->record)) return &cups[i];
    }
    return NULL;
}

/* ---- Calls into the game ---- */

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_new(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(0x944e46u, 1, a);
    if (!rt_unwinding && p) memset(g_mem + p, 0, size);
    return rt_unwinding ? 0 : p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(0x9452cau, 1, a);
}

static uint32_t record_of_key(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    return id >= 0 && base ? base + (uint32_t)id * RECORD_SIZE : 0;
}

/* ---- Dates of the table's text ---- */

static const char *const month_names[12] = {"January", "February", "March", "April", "May", "June", "July",
                                            "August", "September", "October", "November", "December"};

static int is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

/* Day of the year from 0, with the month starts of Date.cpp (0x9b8448, 0x9b8460). */
static int day_of_year(int day, int month, int year)
{
    static const short start[12] = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333};
    return start[month - 1] + day + (month > 2 && is_leap(year));
}

typedef struct { int day, month, year; } Day;

static int day_key(Day d) { return d.year * 400 + d.month * 32 + d.day; }

/* The dates of a text part: each "<day>" with its own month, or followed by "and", "-" or an en dash and another
 * day; a missing month or year comes from the next date that has one. Returns the first and last date, 0 when
 * none. Numbers of other meaning (club counts, "2007 J.League Cup") have no month after them and are not days. */
static int text_dates(const char *t, size_t len, int default_year, Day *first, Day *last)
{
    Day d[32];
    int n = 0, joined[32] = {0};
    const char *end = t + len;
    for (const char *p = t; p < end && n < 32; p++) {
        if (*p < '0' || *p > '9' || (p > t && ((p[-1] >= '0' && p[-1] <= '9') || p[-1] == '-' || p[-1] == '.')))
            continue;
        const char *q = p;
        int v = 0, digits = 0;
        while (q < end && *q >= '0' && *q <= '9') v = v * 10 + (*q++ - '0'), digits++;
        if (digits > 2 || v < 1 || v > 31) { p = q - 1; continue; }
        Day x = {v, 0, 0};
        if (q < end && *q == ' ')
            for (int m = 0; m < 12; m++) {
                size_t ml = strlen(month_names[m]);
                if ((size_t)(end - q - 1) >= ml && !strncmp(q + 1, month_names[m], ml)) { x.month = m + 1; q += 1 + ml; break; }
            }
        if (x.month && q + 5 <= end && q[0] == ' ' && q[1] >= '1' && q[1] <= '2' && q[2] >= '0' && q[2] <= '9' &&
            q[3] >= '0' && q[3] <= '9' && q[4] >= '0' && q[4] <= '9') {
            x.year = (q[1] - '0') * 1000 + (q[2] - '0') * 100 + (q[3] - '0') * 10 + (q[4] - '0');
            q += 5;
        }
        /* A connector to another day: " and ", " - ", " – " (UTF-8 en dash). */
        const char *r = q;
        while (r < end && *r == ' ') r++;
        int link = 0;
        if (r + 3 <= end && !strncmp(r, "and", 3)) link = 3;
        else if (r < end && *r == '-') link = 1;
        else if (r + 3 <= end && (unsigned char)r[0] == 0xe2 && (unsigned char)r[1] == 0x80 && (unsigned char)r[2] == 0x93)
            link = 3;
        int linked = 0;
        if (link) {
            const char *s = r + link;
            while (s < end && *s == ' ') s++;
            linked = s < end && *s >= '0' && *s <= '9';
        }
        if (!x.month && !linked) { p = q - 1; continue; }
        joined[n] = linked;
        d[n++] = x;
        p = q - 1;
    }
    /* A day with no month must be joined to the next day (the one that gives the month). */
    for (int i = n - 2; i >= 0; i--) {
        if (!d[i].month && joined[i]) d[i].month = d[i + 1].month;
        if (!d[i].year && d[i].month) d[i].year = d[i + 1].year;
    }
    int k = 0;
    for (int i = 0; i < n; i++)
        if (d[i].month) {
            if (!d[i].year) d[i].year = default_year;
            d[k++] = d[i];
        }
    if (!k) return 0;
    *first = *last = d[0];
    for (int i = 1; i < k; i++) {
        if (day_key(d[i]) < day_key(*first)) *first = d[i];
        if (day_key(d[i]) > day_key(*last)) *last = d[i];
    }
    return 1;
}

/* ---- The edition of a year ---- */

typedef struct {
    int first, last;                    /* days of the year of `year` (last may be a later year: see last_year) */
    int year, last_year;                /* the calendar years of the first and the last date */
    int clubs;                          /* the club count of the part */
    int legs, final_legs;               /* 1 or 2 */
} Edition;

static const SeasonRule *row_of(const char *comp, int s)
{
    const SeasonRule *best = NULL;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (strcmp(r->comp, comp) || r->season > s) continue;
        if (!best || r->season > best->season) best = r;
    }
    return best;
}

/* Part `i` of a text split by `sep`: start and length, 0 when there is no such part. */
static int part_of(const char *t, const char *sep, int i, const char **start, size_t *len)
{
    const char *p = t;
    for (int k = 0; k < i; k++) {
        p = strstr(p, sep);
        if (!p) return 0;
        p += strlen(sep);
    }
    const char *e = strstr(p, sep);
    *start = p;
    *len = e ? (size_t)(e - p) : strlen(p);
    return 1;
}

static int has(const char *t, size_t len, const char *word)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%.*s", (int)(len < sizeof buf - 1 ? len : sizeof buf - 1), t);
    return strstr(buf, word) != NULL;
}

/* The edition of row r, part i: 1 when its dates are known. */
static int edition_of_part(const ClubCup *k, const SeasonRule *r, int i, int default_year, Edition *e)
{
    const char *t, *c;
    size_t len, clen;
    if (!r->format_text || !part_of(r->format_text, "||", i, &t, &len)) return 0;
    if (k->skip_first_clause) {
        const char *semi = memchr(t, ';', len);
        if (semi) { len -= (size_t)(semi + 1 - t); t = semi + 1; }
    }
    Day a, b;
    if (!text_dates(t, len, default_year, &a, &b)) return 0;
    /* A late edition (the Sudamericana of 2020, final 23 January 2021) plays at the normal time (Q277): its final on
     * 20 December, so the edition ends before the entry's update day of 10 January and its season start runs. */
    if (k->calendar && b.year > a.year) b = (Day){20, 12, a.year};
    e->year = a.year;
    e->last_year = b.year;
    e->first = day_of_year(a.day, a.month, a.year);
    e->last = day_of_year(b.day, b.month, b.year);
    e->clubs = r->clubs > 0 ? r->clubs : 0;
    if (r->clubs_text && part_of(r->clubs_text, ";", i, &c, &clen)) e->clubs = atoi(c);   /* "47; 44+10" */
    e->legs = has(t, len, "two-legged") || has(t, len, "two legs") ? 2 : 1;
    e->final_legs = e->legs == 2 && !has(t, len, "single-match final") ? 2 : 1;
    return e->clubs >= 2;
}

/* The edition of year y (a calendar year or a season), with the rules of y: 1 when held and its dates known. */
static int edition_now(const ClubCup *k, int y, Edition *e)
{
    if (!k->calendar) {
        const SeasonRule *r = row_of(k->def->comp, y);
        if (!r || r->clubs == SEASON_RULE_NOT_HELD || !strcmp(r->format, "not_held")) return 0;
        return edition_of_part(k, r, 0, y, e);
    }
    for (int s = y; s >= y - 1; s--) {
        const SeasonRule *r = row_of(k->def->comp, s);
        if (!r || r->season != s || r->clubs == SEASON_RULE_NOT_HELD || !strcmp(r->format, "not_held")) continue;
        for (int i = 0; i < 4; i++)
            if (edition_of_part(k, r, i, y, e) && e->year == y) return 1;
    }
    return 0;
}

/* The edition of year y in the game: with changing rules off the start season's edition, moved to year y (Q232). */
static int edition(const ClubCup *k, int y, Edition *e)
{
    if (season_changing_rules()) return edition_now(k, y, e);
    int s = season_start_year();
    for (int base = s; base <= s + 1; base++)
        if (edition_now(k, base, e)) {
            int shift = y - base;
            int f = e->first, l = e->last;
            /* The same day and month in the moved years. */
            Day a = {0}, b = {0};
            for (int m = 12; m >= 1; m--)
                if (!a.month && f >= day_of_year(1, m, e->year)) a = (Day){f - day_of_year(1, m, e->year) + 1, m, e->year};
            for (int m = 12; m >= 1; m--)
                if (!b.month && l >= day_of_year(1, m, e->last_year))
                    b = (Day){l - day_of_year(1, m, e->last_year) + 1, m, e->last_year};
            e->year += shift;
            e->last_year += shift;
            e->first = day_of_year(a.day, a.month, e->year);
            e->last = day_of_year(b.day, b.month, e->last_year);
            return 1;
        }
    return 0;
}

/* ---- The rounds ---- */

static int in_original;                 /* 1 while original_table calls the original (the wrap passes it on) */

static uint32_t original_table(uint32_t obj)
{
    static uint32_t out;
    if (!out) out = win_static_alloc(12);
    uint32_t a[4] = {0xffffffffu, out, out + 4, out + 8};
    in_original = 1;
    uint32_t t = call_this(0x92b770u, obj, 4, a);              /* the original's one round, 0x92b857..0x92b886 */
    in_original = 0;
    return rt_unwinding ? 0 : t;
}

typedef struct { int id, teams, ties, fresh, first, legs, match; } Round;

static int plan_knockout(const Edition *e, int n, Round *r);

/* The rounds of an edition of n clubs: a knockout with the byes in the first round, or the stepladder. Returns the
 * count. match: the day of the first leg, a day of the year of e->year (it may pass 365). */
static int plan_rounds(const ClubCup *k, const Edition *e, int n, Round *r)
{
    int count = 0;
    int last = e->last + (e->last_year > e->year ? 365 + is_leap(e->year) : 0);
    if (!strcmp(k->def->comp, "fifa_intercontinental_cup") && n == 6) {
        /* Club list: OFC, AFC, CAF, CONMEBOL, CONCACAF, UEFA (pick_intercontinental). The play-off of OFC and AFC,
         * then the African-Asian-Pacific Cup and the Derby of the Americas with CAF, CONMEBOL and CONCACAF new
         * (the play-off winner takes the empty slot), the Challenger Cup, the final with UEFA new. */
        int span = last - e->first;
        r[count++] = (Round){0x14, 2, 1, 2, 0, 1, e->first};
        r[count++] = (Round){0x78, 4, 2, 3, 2, 1, e->first + span / 3};
        r[count++] = (Round){0x82, 2, 1, 0, 0, 1, e->first + 2 * span / 3};
        r[count++] = (Round){0x96, 2, 1, 1, 5, 1, last};
        return count;
    }
    return plan_knockout(e, n, r);
}

/* A knockout of n clubs with the byes in the first round: the rounds and their days (see plan_rounds). */
static int plan_knockout(const Edition *e, int n, Round *r)
{
    int count = 0;
    int last = e->last + (e->last_year > e->year ? 365 + is_leap(e->year) : 0);
    int p = 1;
    while (p * 2 <= n) p *= 2;
    int teams = p;
    if (n > p) r[count++] = (Round){0x14, 2 * (n - p), n - p, 2 * (n - p), 0, e->legs, 0};
    for (; teams >= 2; teams /= 2) {
        int fresh = count == 0 ? teams : (n > p && count == 1 ? 2 * p - n : 0);
        int first = count == 1 && n > p ? 2 * (n - p) : 0;
        int id = teams == 2 ? 0x96 : teams == 4 ? 0x82 : teams == 8 ? 0x78 : 0x14;
        r[count++] = (Round){id, teams, teams / 2, fresh, first, teams == 2 ? e->final_legs : e->legs, 0};
    }
    /* Days: the final (its first leg) on the last day, or 7 days before for two legs; the rounds before it from
     * the first day, evenly. */
    int final_day = last - (r[count - 1].legs == 2 ? 7 : 0);
    for (int i = 0; i < count; i++)
        r[i].match = count == 1 ? e->first : i == count - 1 ? final_day : e->first + i * (final_day - e->first) / (count - 1);
    if (count == 1 && r[0].legs == 2 && last > e->first) r[0].match = e->first;
    return count;
}

/* The round table of the edition for an object with n clubs (the picker ran first), from the original's round as
 * the template. Returns it (the game's heap) and the round count, or 0. */
static uint32_t round_table(const ClubCup *k, uint32_t obj, int *rounds)
{
    int year = (int16_t)rd16(g_mem, obj + YEAR_AT), n = (int16_t)rd16(g_mem, obj + CLUB_COUNT_AT);
    Edition e;
    if (!edition(k, year, &e) || n < 2) return 0;
    Round r[8];
    int count = plan_rounds(k, &e, n, r);
    uint32_t tmpl = original_table(obj);
    if (!tmpl) return 0;
    uint32_t table = game_new((uint32_t)count * ROUND_SIZE);
    if (!table) { game_free(tmpl); return 0; }
    int base = year;                                            /* the year offsets count from +0x40 */
    int first_two = r[0].legs == 2 && count == 1 && e.last > e.first ? e.last - e.first : 7;
    for (int i = 0; i < count; i++) {
        uint32_t t = table + (uint32_t)i * ROUND_SIZE;
        memcpy(g_mem + t, g_mem + tmpl, ROUND_SIZE);
        int match = r[i].match;
        int draw = i == 0 ? match - 7 : r[i - 1].match + (r[i - 1].legs == 2 ? 7 : 0) + 1;
        if (draw > match - 1) draw = match - 1;
        int y = e.year, len = 365 + is_leap(e.year);
        int dy = draw, my = match, dyear = y, myear = y;
        if (dy < 0) { dyear--; dy += 365 + is_leap(dyear); }
        if (dy >= len) { dy -= len; dyear++; }
        if (my >= len) { my -= len; myear++; }
        wr16(g_mem, t + 0x00, (uint16_t)dy);
        g_mem[t + 0x02] = (uint8_t)(dyear - base);
        wr16(g_mem, t + 0x03, (uint16_t)my);
        g_mem[t + 0x05] = (uint8_t)(myear - base);
        wr16(g_mem, t + 0x07, (uint32_t)r[i].id);
        wr16(g_mem, t + 0x09, (uint32_t)i);
        wr16(g_mem, t + 0x18, (uint32_t)r[i].teams);
        wr16(g_mem, t + 0x1a, (uint32_t)r[i].ties);
        wr16(g_mem, t + 0x1c, (uint32_t)r[i].fresh);
        wr16(g_mem, t + 0x1e, (uint32_t)r[i].first);
        if (r[i].legs == 2) {                                   /* as the Libertadores, 0x4c343e..0x4c346a */
            wr16(g_mem, t + 0x0d, 4);
            wr16(g_mem, t + 0x0f, 1);
            g_mem[t + 0x21] = 2;
            g_mem[t + 0x22] = (uint8_t)(count == 1 ? first_two : 7);
        } else {
            wr16(g_mem, t + 0x0d, 3);                           /* extra time, penalties (0x92b857..0x92b886) */
            g_mem[t + 0x21] = 1;
        }
    }
    game_free(tmpl);
    if (rt_unwinding) return 0;
    *rounds = count;
    fprintf(stderr, "recomp: club cup %s %d: %d clubs, %d rounds, legs %d, final legs %d, days %d.%d to %d.%d\n",
            k->def->file, year, n, count, e.legs, e.final_legs, e.first, e.year, e.last, e.last_year);
    return table;
}

/* ---- Clubs ---- */

/* The club at `place` (0 winner, 1 runner-up) of the latest history row of record rec written in year y or
 * before, or 0. The latest row up to y gives the reigning champion of a competition whose edition of year y ends
 * after the pick (the CONCACAF Champions Cup plays from 28 September, after the Intercontinental Cup's pick). */
static uint32_t history_club(uint32_t rec, int y, int place)
{
    uint32_t rows = rd32(g_mem, HISTORY), best = 0;
    int32_t count = (int32_t)rd32(g_mem, HISTORY_COUNT);
    int best_year = -1;
    for (int32_t i = 0; rec && rows && i < count; i++) {
        uint32_t row = rows + (uint32_t)i * HISTORY_ROW;
        int year = (int16_t)rd16(g_mem, row + 8);
        if (rd32(g_mem, row + 4) != rec || year > y || year < best_year) continue;
        best = rd32(g_mem, row + 0xa + 4u * (uint32_t)place);
        best_year = year;
    }
    return best;
}

static int listed(const uint32_t *out, int n, uint32_t club)
{
    for (int i = 0; i < n; i++)
        if (out[i] == club) return 1;
    return 0;
}

static int add_winner(uint32_t *out, int n, uint32_t rec, int y, uint32_t obj, uint32_t continent)
{
    (void)obj;
    uint32_t c = history_club(rec, y, 0);
    if (!c || listed(out, n, c)) c = history_club(rec, y, 1);
    if (!c || listed(out, n, c)) c = season_comps_top_club(continent, out, n);
    if (c && !listed(out, n, c)) out[n++] = c;
    return n;
}

static int pick_recopa(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n)
{
    (void)k;
    int m = 0;
    m = add_winner(out, m, record_of_key(KEY_LIBERTADORES), year - 1, obj, CONT_SOUTH_AMERICA);
    if (m < n) m = add_winner(out, m, season_comps_record(cups[0].def->record), year - 1, obj, CONT_SOUTH_AMERICA);
    return m;
}

static int pick_suruga(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n)
{
    (void)k;
    int m = 0;
    m = add_winner(out, m, record_of_key(KEY_JAPAN_CUP), year - 1, obj, CONT_ASIA);
    if (m < n) m = add_winner(out, m, season_comps_record(cups[0].def->record), year - 1, obj, CONT_SOUTH_AMERICA);
    return m;
}

/* OFC, AFC, CAF, CONMEBOL, CONCACAF, UEFA: the club list order of plan_rounds. The winners of the edition's year
 * (the original has no African club competition: a club of Africa's top reputations). */
static int pick_intercontinental(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n)
{
    (void)k;
    static const struct { uint32_t key, continent; } from[6] = {
        {KEY_OCEANIA, CONT_OCEANIA}, {KEY_ASIA, CONT_ASIA}, {0, CONT_AFRICA},
        {KEY_LIBERTADORES, CONT_SOUTH_AMERICA}, {KEY_CONCACAF, CONT_NORTH_AMERICA}, {KEY_CHAMPIONS_LEAGUE, CONT_EUROPE}};
    int m = 0;
    for (int i = 0; i < 6 && m < n; i++) {
        int before = m;
        m = add_winner(out, m, from[i].key ? record_of_key(from[i].key) : 0, year, obj, from[i].continent);
        if (m == before) return m;                             /* keep the order: stop at a gap */
    }
    return m;
}

static int pick_acl_two(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n)
{
    (void)k;
    (void)obj;
    (void)year;
    int m = 0;
    while (m < n) {
        uint32_t c = season_comps_top_club(CONT_ASIA, out, m);
        if (!c) break;
        out[m++] = c;
    }
    return m;
}

/* The clubs with the Sudamericana's mark (club +0x1db = its record id), in club table order. */
static int pick_sudamericana(const ClubCup *k, uint32_t obj, int year, uint32_t *out, int n)
{
    (void)year;
    uint32_t rec = season_comps_record(k->def->record), clubs = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_TABLE_COUNT);
    int m = 0;
    for (int32_t i = 0; rec && clubs && i < count && m < n; i++) {
        uint32_t c = clubs + (uint32_t)i * CLUB_ROW;
        if (rd32(g_mem, c + CLUB_CONTINENTAL) == rd32(g_mem, rec)) out[m++] = c;
    }
    (void)obj;
    while (m < n) {                                             /* no marks yet (before the first seeding) */
        uint32_t c = season_comps_top_club(CONT_SOUTH_AMERICA, out, m);
        if (!c) break;
        out[m++] = c;
    }
    return m;
}

/* The club count of the edition of the object's year (the table), or 2. */
static int club_count(const ClubCup *k, int year)
{
    Edition e;
    int n = edition(k, year, &e) ? e.clubs : 2;
    return n < 2 ? 2 : n > MAX_CLUBS ? MAX_CLUBS : n;
}

/* Fills the object's club list: a new list of n clubs at a new pick, the same n at a repick. */
static int fill_clubs(const ClubCup *k, uint32_t obj, int again)
{
    int year = (int16_t)rd16(g_mem, obj + YEAR_AT);
    int n = again ? (int16_t)rd16(g_mem, obj + CLUB_COUNT_AT) : club_count(k, year);
    if (n < 2 || n > MAX_CLUBS) return 0;
    uint32_t out[MAX_CLUBS] = {0};
    int m = k->pick(k, obj, year, out, n);
    if (rt_unwinding) return 0;
    /* Net (guard review row A27): the pickers give the edition's count from the clubs of the data, in every tested
     * database (club_cups_check.py 2002-on, 2010-on, 2016-on, 2024-on: no short pick). Only a database with fewer
     * clubs of a continent than the edition needs can reach the fill below, which takes the clubs of the highest
     * reputation of South America, then Europe; the log names such a pick. */
    if (m < n)
        fprintf(stderr, "recomp: club cup %s %d: the pick gave %d of %d clubs, the net fills the rest\n",
                k->def->file, year, m, n);
    for (uint32_t cont = CONT_SOUTH_AMERICA; m < n;) {         /* never fewer clubs than the tree holds */
        uint32_t c = season_comps_top_club(cont, out, m);
        if (!c) { if (cont == CONT_EUROPE) break; cont = CONT_EUROPE; continue; }
        out[m++] = c;
    }
    if (m < n) return 0;
    uint32_t list = again ? rd32(g_mem, obj + CLUB_LIST_AT) : game_new(6u * (uint32_t)n);
    if (!list) return 0;
    for (int i = 0; i < n; i++) {
        uint32_t e = list + 6u * (uint32_t)i;
        wr32(g_mem, e, out[i]);
        if (!again) g_mem[e + 4] = 0, g_mem[e + 5] = 0;        /* as 0x92bd66..0x92bd92 */
    }
    if (!again) {
        wr16(g_mem, obj + CLUB_COUNT_AT, (uint16_t)n);
        wr32(g_mem, obj + CLUB_LIST_AT, list);
    }
    fprintf(stderr, "recomp: club cup %s %d: %s %d clubs\n", k->def->file, year, again ? "picked again" : "picked", n);
    return 1;
}

/* ---- Wraps ---- */

void o_0092b8b0(void);
void o_0092b770(void);

/* World_Club_Cup::pick_clubs (this), returns >= 0 on success (0x92b543). */
RT_WRAP(0092b8b0, world_club_cup_pick)
{
    const ClubCup *k = cup_of_object(cpu.ecx);
    if (!k) { o_0092b8b0(); return; }
    uint32_t obj = cpu.ecx;
    int ok = fill_clubs(k, obj, 0);
    if (rt_unwinding) return;
    rt_ret_cdecl(ok ? 1u : 0xffffffffu);
}

/* World_Club_Cup::create_schedule (char stage, short *rounds, short *code, int *is_league), ret 0x10. */
RT_WRAP(0092b770, world_club_cup_schedule)
{
    const ClubCup *k = in_original ? NULL : cup_of_object(cpu.ecx);
    if (!k) { o_0092b770(); return; }
    uint32_t obj = cpu.ecx, rounds_at = rt_arg(1), code_at = rt_arg(2), league_at = rt_arg(3);
    int rounds = 0;
    uint32_t table = round_table(k, obj, &rounds);
    if (rt_unwinding) return;
    if (!table) {                                               /* the original's one round */
        o_0092b770();
        return;
    }
    wr16(g_mem, rounds_at, (uint16_t)rounds);
    wr16(g_mem, code_at, 0);
    if (league_at) wr32(g_mem, league_at, 0);
    rt_ret_stdcall(table, 4);
}

/* update_for_date of the vtable copy (slot 5, Cup::update_for_date 0x51f890, thiscall (Date *today, int), ret 8):
 * on the day before the first draw the picker runs again, then the Cup's own update. */
static void cup_update(void)
{
    uint32_t obj = cpu.ecx, today = rt_arg(0);
    const ClubCup *k = cup_of_object(obj);
    uint32_t table = rd32(g_mem, obj + ROUNDS_AT);
    if (k && table && (int16_t)rd16(g_mem, obj + ROUND_COUNT_AT) > 0) {
        int year = (int16_t)rd16(g_mem, obj + YEAR_AT);
        int draw = (int16_t)rd16(g_mem, table) - 1, dyear = year + (int8_t)g_mem[table + 2];
        if (draw < 0) { dyear--; draw += 365 + is_leap(dyear); }
        if ((int16_t)rd16(g_mem, today) == draw && (int16_t)rd16(g_mem, today + 2) == dyear) {
            Cpu saved = cpu;
            fill_clubs(k, obj, 1);
            if (rt_unwinding) return;
            cpu = saved;
        }
    }
    rt_jump(0x51f890u);
}

/* ---- The object ---- */

static uint32_t make_cup(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    (void)loading;
    uint32_t size[1] = {CUP_SIZE};
    uint32_t obj = call_cdecl(0x944cf1u, 1, size);
    if (rt_unwinding || !obj) return 0;
    uint32_t args[2] = {(uint32_t)year, rec};
    call_this(0x92b4b0u, obj, 2, args);                         /* it picks and builds its rounds (wraps above) */
    if (rt_unwinding) return 0;
    uint32_t vt = season_comps_vtable(d, CUP_VTABLE, 40);
    if (vt) {
        season_comps_set_slot(vt, 5, "club_cup_update", cup_update);
        wr32(g_mem, obj, vt);
    }
    return obj;
}

/* ---- The Sudamericana places: the CONMEBOL seeding ---- */

void o_004c6430(void);

/* Conmebol_Seeding::assign_libertadores_places (this, list, count), ret 8. After the original, while the sorted list
 * of the seeding lives (it is freed at 0x4c6267), the Sudamericana places: the first clubs of each nation that hold
 * no place (pick_club_by_position 0x4c6730 (list, count, nation id, 1, 1, 0), ret 0x18, as 0x4c66cc..0x4c66d7). The
 * seeding rows: 10 bytes at the object, +0 the nation id, count +0x6e (0x4c6154). */
/* The places of a nation in the "places:" list of a row's text ("places: ARG 6+1, BRA 7+1": 6), or -1. The code
 * may stand earlier in the text with another meaning ("all but ARG and BRA"). */
static int row_places(const char *t, const char *code)
{
    const char *list = t ? strstr(t, "places:") : NULL;
    const char *p = list ? strstr(list, code) : NULL;
    size_t n = strlen(code);
    if (!p || p[n] != ' ' || p[n + 1] < '0' || p[n + 1] > '9') return -1;
    return atoi(p + n + 1);
}

/* ---- Step L1: the Libertadores places by nation (docs/research/2026-10-19-club-cups.md section 6) ---- */

#define SEEDING_ROWS 0x6eu              /* byte: the count of 10-byte rows (0x4c6154) */
#define LIBERTADORES_PLACES 31          /* the original's sum: 4 + 4 + 1 + 1 + 7 x 3 (0x4c60f4..0x4c6176) */
#define LIBERTADORES_CLUBS 32           /* collect_entrants 0x4c3f40 takes exactly 32 (0x4c3f51, box at 0x4c3fe2) */

static int is_key(uint32_t id, uint32_t key) { return id == rd32(g_mem, key); }

/* The row of the Libertadores edition of calendar year y: it starts in January or February y, so in season y - 1
 * (the season of its first match, Q276); with changing rules off the start season's row (Q232). */
static const SeasonRule *libertadores_row(int y)
{
    return row_of("libertadores", season_changing_rules() ? y - 1 : season_start_year());
}

/* Before the original: byte +8 of each seeding row (read at 0x4c66b7) from the row's places ("ARG 6+1, BRA 7+1":
 * the "+1" is the holder, which the original places first, 0x4c6458). Mexico keeps 1 place to 2015/16, then none
 * (it left the competition). The other nations share the rest of 31 in row order. */
static void libertadores_places(uint32_t seeding, int y)
{
    const SeasonRule *r = libertadores_row(y);
    if (!r) return;
    int arg = row_places(r->format_text, "ARG"), bra = row_places(r->format_text, "BRA");
    if (arg < 0 || bra < 0) return;
    int rows = (int8_t)g_mem[seeding + SEEDING_ROWS], others = 0, mexico = r->season <= 2015;
    for (int i = 0; i < rows && i < 11; i++) {
        uint32_t id = rd32(g_mem, seeding + 10u * (uint32_t)i);
        if (!is_key(id, KEY_MEXICO) && !is_key(id, KEY_ARGENTINA) && !is_key(id, KEY_BRAZIL)) others++;
    }
    int rest = LIBERTADORES_PLACES - arg - bra - mexico, k = 0;
    for (int i = 0; i < rows && i < 11; i++) {
        uint32_t id = rd32(g_mem, seeding + 10u * (uint32_t)i);
        int places;
        if (is_key(id, KEY_ARGENTINA)) places = arg;
        else if (is_key(id, KEY_BRAZIL)) places = bra;
        else if (is_key(id, KEY_MEXICO)) places = mexico;
        else places = others ? rest / others + (k++ < rest % others) : 0;
        g_mem[seeding + 10u * (uint32_t)i + 8] = (uint8_t)(places < 0 ? 0 : places);
    }
}

static int marked_count(uint32_t id)
{
    uint32_t clubs = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_TABLE_COUNT);
    int n = 0;
    for (int32_t i = 0; clubs && i < count; i++)
        if (rd32(g_mem, clubs + (uint32_t)i * CLUB_ROW + CLUB_CONTINENTAL) == id) n++;
    return n;
}

/* After the original: exactly 32 places. A nation with too few clubs leaves free places (0x4c6730 gives 0): the
 * next clubs of the other nations take them, in row order. */
static void libertadores_top_up(uint32_t seeding, uint32_t list, uint32_t count)
{
    uint32_t id = rd32(g_mem, KEY_LIBERTADORES);
    int n = marked_count(id), rows = (int8_t)g_mem[seeding + SEEDING_ROWS];
    for (int pass = 0; n < LIBERTADORES_CLUBS && pass < 8; pass++)
        for (int i = 0; i < rows && i < 11 && n < LIBERTADORES_CLUBS; i++) {
            uint32_t nation = rd32(g_mem, seeding + 10u * (uint32_t)i);
            if (is_key(nation, KEY_MEXICO)) continue;
            uint32_t a[6] = {list, count, nation, 1, 1, 0};
            uint32_t club = call_this(0x4c6730u, seeding, 6, a);
            if (rt_unwinding) return;
            if (!club || (int32_t)rd32(g_mem, club + CLUB_CONTINENTAL) != -1) continue;
            wr32(g_mem, club + CLUB_CONTINENTAL, id);
            g_mem[club + CLUB_ROUND] = 2;                       /* as the nation places, 0x4c66ef */
            n++;
        }
    fprintf(stderr, "recomp: libertadores places: %d clubs\n", n);
}

RT_WRAP(004c6430, assign_libertadores_places)
{
    uint32_t seeding = cpu.ecx, list = rt_arg(0), count = rt_arg(1);
    int y = (int16_t)rd16(g_mem, GAME_DAY + 2);
    if (season_comps_on()) libertadores_places(seeding, y);
    o_004c6430();
    if (rt_unwinding || !season_comps_on()) return;
    {
        Cpu saved = cpu;
        libertadores_top_up(seeding, list, count);
        if (rt_unwinding) return;
        cpu = saved;
    }
    const ClubCup *k = &cups[0];
    Edition e;
    uint32_t rec = season_comps_record(k->def->record);
    if (!rec || !edition(k, y, &e)) return;
    Cpu saved = cpu;
    const SeasonRule *r = NULL;
    for (int s = y; s >= y - 1 && !r; s--) r = row_of(k->def->comp, s);
    int arg = r ? row_places(r->format_text, "ARG") : -1, bra = r ? row_places(r->format_text, "BRA") : -1;
    int rows = (int8_t)g_mem[seeding + 0x6e], others = 0;
    for (int i = 0; i < rows && i < 11; i++) {
        uint32_t id = rd32(g_mem, seeding + 10u * (uint32_t)i);
        if (id != rd32(g_mem, KEY_MEXICO) && id != rd32(g_mem, KEY_ARGENTINA) && id != rd32(g_mem, KEY_BRAZIL)) others++;
    }
    int rest = e.clubs - (arg > 0 ? arg : 0) - (bra > 0 ? bra : 0);
    int marked = 0;
    for (int round = 0; marked < e.clubs && round < 16; round++)
        for (int i = 0; i < rows && i < 11 && marked < e.clubs; i++) {
            uint32_t id = rd32(g_mem, seeding + 10u * (uint32_t)i);
            int places;
            if (id == rd32(g_mem, KEY_MEXICO)) continue;
            if (id == rd32(g_mem, KEY_ARGENTINA)) places = arg >= 0 ? arg : 4;
            else if (id == rd32(g_mem, KEY_BRAZIL)) places = bra >= 0 ? bra : 4;
            else places = others ? (rest + others - 1) / others : 0;
            if (round >= places) continue;
            uint32_t a[6] = {list, count, id, 1, 1, 0};
            uint32_t club = call_this(0x4c6730u, seeding, 6, a);
            if (rt_unwinding) return;
            if (!club || (int32_t)rd32(g_mem, club + CLUB_CONTINENTAL) != -1) continue;
            wr32(g_mem, club + CLUB_CONTINENTAL, rd32(g_mem, rec));
            g_mem[club + CLUB_ROUND] = 1;
            marked++;
        }
    cpu = saved;
    fprintf(stderr, "recomp: club cup new_sudamericana %d: %d clubs marked (ARG %d, BRA %d)\n", y, marked, arg, bra);
}

/* ---- The CONCACAF Champions Cup's club count (part 26c; research: the shared notes, counts research section 3) ----
 *
 * Con_Champ (constructor 0x4c0c40, vtable 0x968aa0, Cup base) plays 8 clubs in 3 one-match rounds (round table
 * 0x4c0f00, stage -1 only: 0x4c0f0a; draw 1 August, matches 28 September, 1 and 4 October). The table gives 16
 * (2001/02, 2002/03, 2017/18 to 2022/23), 8 (2003/04 to 2007/08), 24 (2008/09 to 2016/17) and 27 (2023/24 on).
 * The count goes to the constructor's club list and +0x3e (seasons.py sites 0x4c0cc2, 0x4c0cc4), to the picker's
 * +0x3e (0x4c11af) and, for a later edition, to +0x3e before start_new_season, which makes the list of +0x3e clubs
 * (0x4c14de..0x4c14f5) and the round table and tree again (slot 35). The round table wrap gives a knockout with
 * the byes in the first round (plan_rounds). The picker wrap keeps the original's 8 clubs (Mexico, the MLS and US
 * Open Cup winners, Central America, the Caribbean, 0x4c1203..0x4c136b) at the end of the list, where the byes
 * enter, and fills the first places with the North American clubs of the highest reputation. Decisions (club-cups
 * doc): one match a round, 3 days apart, in the original's weeks; 15 clubs (2021/22) play as 16; the group
 * seasons play as a knockout of the same count. */

#define CON_CHAMP_VTABLE 0x968aa0u
#define CON_CHAMP_ORIGINAL 8

static int concacaf_count(int year)
{
    if (!season_comps_on()) return 0;
    int s = season_changing_rules() ? year : season_start_year();
    for (int y = s; y >= SEASON_RULE_FIRST; y--) {               /* after the last known count it stays (Q248) */
        const SeasonRule *r = row_of("concacaf_champions", y);
        if (!r) return 0;
        if (r->season != y) { y = r->season + 1; continue; }
        if (r->clubs >= 2) {
            int n = r->clubs == 15 ? 16 : r->clubs;
            return n < CON_CHAMP_ORIGINAL ? CON_CHAMP_ORIGINAL : n > MAX_CLUBS ? MAX_CLUBS : n;
        }
    }
    return 0;
}

/* The clubs of North and South America in the game (the picker's sources): the count never asks for more, so the
 * picker never leaves an empty place (review 1, item 2). The walk of season_comps_top_club. */
static int americas_clubs(void)
{
    uint32_t clubs = rd32(g_mem, CLUBS), na = rd32(g_mem, CONT_NORTH_AMERICA), sa = rd32(g_mem, CONT_SOUTH_AMERICA);
    int32_t count = (int32_t)rd32(g_mem, CLUB_TABLE_COUNT) - 2 * (int32_t)rd32(g_mem, 0xae2350u);
    int n = 0;
    for (int32_t i = 0; clubs && i < count; i++) {
        uint32_t nation = rd32(g_mem, clubs + (uint32_t)i * CLUB_ROW + 0x53);
        uint32_t cont = nation ? rd32(g_mem, nation + 0x71) : 0;
        if (cont && (rd32(g_mem, cont) == na || rd32(g_mem, cont) == sa)) n++;
    }
    return n;
}

uint32_t rt_concacaf_clubs(uint32_t year, uint32_t original)
{
    int n = concacaf_count((int16_t)year);
    if (n > CON_CHAMP_ORIGINAL) {
        int have = americas_clubs();
        if (n > have) n = have > CON_CHAMP_ORIGINAL ? have : CON_CHAMP_ORIGINAL;
    }
    return n ? (uint32_t)n : original;
}

static int is_con_champ(uint32_t obj) { return obj && rd32(g_mem, obj) == CON_CHAMP_VTABLE; }

void o_004c11a0(void);
void o_004c0f00(void);

/* Con_Champ::pick_clubs (this), returns 1 (0x4c1433). The original writes club list slots 0 to 7. */
RT_WRAP(004c11a0, con_champ_pick)
{
    uint32_t obj = cpu.ecx;
    o_004c11a0();
    if (rt_unwinding || !is_con_champ(obj)) return;
    int n = (int16_t)rd16(g_mem, obj + CLUB_COUNT_AT);
    uint32_t list = rd32(g_mem, obj + CLUB_LIST_AT);
    /* An original place stays empty when its list of ConChamp::select_qualified_clubs is shorter than its share
     * (Mexico 2, the regional groups of nation +0x75 = 7 and 6, 3 and 1, the MLS 1; 0x4c12be..0x4c1309) or the MLS
     * record has no placed club (0x4afd80, 0x4c13a6). Place 1 is the newest winner of the US Open Cup (key 0x9cf728):
     * four databases of 2010/11 and 2011/12 hold no US Open Cup row (a 2010 start in gate run 8), and the history
     * fill gives them the researched rows (season_history.h, plan.tsv gap rows). The log names a place that still
     * finds no club, and the fill below gives it one (guard review row A27). */
    for (int i = 0; list && i < CON_CHAMP_ORIGINAL; i++)
        if (!rd32(g_mem, list + 6u * (uint32_t)i))
            fprintf(stderr, "recomp: concacaf champions %d: the original's place %d has no club, the net fills it\n",
                    (int16_t)rd16(g_mem, obj + YEAR_AT), i);
    if (n <= CON_CHAMP_ORIGINAL || n > MAX_CLUBS || !list || !concacaf_count((int16_t)rd16(g_mem, obj + YEAR_AT)))
        return;
    uint32_t out[MAX_CLUBS] = {0};
    for (int i = 0; i < CON_CHAMP_ORIGINAL; i++) out[n - CON_CHAMP_ORIGINAL + i] = rd32(g_mem, list + 6u * (uint32_t)i);
    /* Every empty place (the added ones, and an original place that found no club) gets the next club of North,
     * then South America, then Europe: never a null club (review 1, item 2; the count is capped above). */
    int filled = 0;
    for (int i = 0; i < n; i++) {
        if (out[i]) continue;
        uint32_t skip[MAX_CLUBS];
        int k = 0;
        for (int j = 0; j < n; j++) if (out[j]) skip[k++] = out[j];
        uint32_t c = season_comps_top_club(CONT_NORTH_AMERICA, skip, k);
        if (!c) c = season_comps_top_club(CONT_SOUTH_AMERICA, skip, k);
        if (!c) c = season_comps_top_club(CONT_EUROPE, skip, k);
        if (c) out[i] = c, filled++;
    }
    for (int i = 0; i < n; i++) {
        uint32_t e = list + 6u * (uint32_t)i;
        wr32(g_mem, e, out[i]);
        g_mem[e + 4] = 0;
        g_mem[e + 5] = 0;
    }
    fprintf(stderr, "recomp: concacaf champions %d: %d clubs (%d added)\n", (int16_t)rd16(g_mem, obj + YEAR_AT), n,
            filled);
}

/* Con_Champ::create_round_table (char stage, short *rounds, short *code, int *is_league), ret 0x10: stage -1. */
RT_WRAP(004c0f00, con_champ_rounds)
{
    uint32_t obj = cpu.ecx, rounds_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xffu);
    int n = is_con_champ(obj) ? (int16_t)rd16(g_mem, obj + CLUB_COUNT_AT) : 0;
    o_004c0f00();
    uint32_t tmpl = cpu.eax;
    if (rt_unwinding || stage != -1 || n <= CON_CHAMP_ORIGINAL || n > MAX_CLUBS || !tmpl ||
        !concacaf_count((int16_t)rd16(g_mem, obj + YEAR_AT)))
        return;
    Cpu saved = cpu;
    /* The knockout of n clubs (one leg a round): plan_rounds gives teams, ties and byes. */
    Edition e = {0, 60, 0, 0, n, 1, 1};
    Round r[8];
    int count = plan_knockout(&e, n, r);
    uint32_t table = game_new((uint32_t)count * ROUND_SIZE);
    if (rt_unwinding || !table) return;
    int first = (int16_t)rd16(g_mem, tmpl + 0x03), draw0 = (int16_t)rd16(g_mem, tmpl + 0x00);
    for (int i = 0; i < count; i++) {
        uint32_t t = table + (uint32_t)i * ROUND_SIZE;
        int src = i == count - 1 ? 2 : i == count - 2 ? 1 : 0;   /* the original's final, semi-final, first round */
        memcpy(g_mem + t, g_mem + tmpl + (uint32_t)src * ROUND_SIZE, ROUND_SIZE);
        int match = first + 3 * i;
        wr16(g_mem, t + 0x00, (uint16_t)(i == 0 ? draw0 : match - 2));
        wr16(g_mem, t + 0x03, (uint16_t)match);
        wr16(g_mem, t + 0x07, (uint32_t)r[i].id);
        wr16(g_mem, t + 0x09, (uint32_t)i);
        wr16(g_mem, t + 0x18, (uint32_t)r[i].teams);
        wr16(g_mem, t + 0x1a, (uint32_t)r[i].ties);
        wr16(g_mem, t + 0x1c, (uint32_t)r[i].fresh);
        wr16(g_mem, t + 0x1e, (uint32_t)r[i].first);
    }
    game_free(tmpl);
    if (rt_unwinding) return;
    cpu = saved;
    wr16(g_mem, rounds_at, (uint16_t)count);
    cpu.eax = table;
    fprintf(stderr, "recomp: concacaf champions %d: %d clubs, %d rounds\n", (int16_t)rd16(g_mem, obj + YEAR_AT), n,
            count);
}

/* ---- Step A1: the AFC places of Japan, South Korea and Australia (part 26c) ----
 *
 * The Asian Club Championship's picker `select_qualified_clubs` 0x40b950 (thiscall, returns 1 at 0x40bea7) fills
 * 32 entrants at +0xb2 (West 0 to 15, East 16 to 31) with one club a nation: the holder (slot 0 or 16), the
 * champions of Japan (record [0x9cf65c]) and South Korea ([0x9cfa00]) when selected, then one club of each Asian
 * nation (0x40bc08..0x40be90). The table gives Japan, South Korea and, from 2006/07 (slice 27 moves Australia to
 * the AFC), Australia more places ("places: JPN 4, KOR 4, AUS 2"; the "+ 1 play-off" places are not counted).
 * After the original, the next clubs of these nations by league order (division level +0x69 of club +0x5b, high
 * first, then the league position club +0x5f, as the CONMEBOL seeding reads them, 0x4c63a9..0x4c63bd) take the
 * East places of other nations, from the last East place back. Slots 0 and 16 (the holder) stay. The format of the
 * competition stays the original's (Form A, the groups first, is not built: club-cups doc). */

#define AFC_ENTRANTS 0xb2u              /* Club *[32] */
#define NATIONS_TABLE 0xae23a8u         /* nation records of 0x122 bytes, by id (0x40bc46..0x40bc54) */
#define NATION_SIZE 0x122u
#define KEY_JAPAN 0x9cf36cu
#define KEY_KOREA 0x9cf484u
#define KEY_AUSTRALIA 0x9cf224u

static uint32_t nation_of_key(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t base = rd32(g_mem, NATIONS_TABLE);
    return id >= 0 && base ? base + (uint32_t)id * NATION_SIZE : 0;
}

/* The club of nation that is not in skip[0..n) and comes first in the league order, or 0. */
static uint32_t next_league_club(uint32_t nation, const uint32_t *skip, int n)
{
    uint32_t clubs = rd32(g_mem, CLUBS), best = 0;
    int32_t count = (int32_t)rd32(g_mem, CLUB_TABLE_COUNT) - 2 * (int32_t)rd32(g_mem, 0xae2350u);
    int best_level = -100000, best_pos = 1000;
    for (int32_t i = 0; clubs && nation && i < count; i++) {
        uint32_t c = clubs + (uint32_t)i * CLUB_ROW, div = rd32(g_mem, c + 0x5b);
        if (rd32(g_mem, c + 0x53) != nation || !div) continue;
        int level = (int16_t)rd16(g_mem, div + 0x69), pos = g_mem[c + 0x5f] ? g_mem[c + 0x5f] : 999;
        if (level < best_level || (level == best_level && pos >= best_pos)) continue;
        if (listed(skip, n, c)) continue;
        best = c, best_level = level, best_pos = pos;
    }
    return best;
}

void o_0040b950(void);

RT_WRAP(0040b950, asia_club_champ_entrants)
{
    uint32_t obj = cpu.ecx;
    o_0040b950();
    if (rt_unwinding || !season_comps_on()) return;
    uint32_t list = rd32(g_mem, obj + AFC_ENTRANTS);
    int year = (int16_t)rd16(g_mem, obj + YEAR_AT);
    const SeasonRule *r = row_of("asian_champions", season_changing_rules() ? year : season_start_year());
    if (!list || !r || !r->format_text) return;
    static const struct { const char *code; uint32_t key; } nat[3] = {
        {"JPN", KEY_JAPAN}, {"KOR", KEY_KOREA}, {"AUS", KEY_AUSTRALIA}};
    uint32_t picked[32];
    for (int i = 0; i < 32; i++) picked[i] = rd32(g_mem, list + 4u * (uint32_t)i);
    int added[3] = {0, 0, 0}, slot = 31;
    for (int k = 0; k < 3; k++) {
        int want = row_places(r->format_text, nat[k].code);
        uint32_t nation = nation_of_key(nat[k].key);
        if (want <= 0 || !nation) continue;
        int have = 0;
        for (int i = 0; i < 32; i++)
            if (picked[i] && rd32(g_mem, picked[i] + 0x53) == nation) have++;
        while (have < want) {
            /* the last East place of a club of another nation (not the holder slot 16) */
            while (slot > 16) {
                uint32_t c = picked[slot], cn = c ? rd32(g_mem, c + 0x53) : 0;
                int ours = 0;
                for (int j = 0; j < 3; j++) ours |= cn && cn == nation_of_key(nat[j].key);
                if (!ours) break;
                slot--;
            }
            if (slot <= 16) break;
            uint32_t c = next_league_club(nation, picked, 32);
            if (!c) break;
            picked[slot--] = c;
            have++;
            added[k]++;
        }
    }
    for (int i = 16; i < 32; i++) wr32(g_mem, list + 4u * (uint32_t)i, picked[i]);
    fprintf(stderr, "recomp: asian champions %d: places added JPN %d, KOR %d, AUS %d\n", year, added[0], added[1],
            added[2]);
}

/* ---- Years with no edition (the shared rule of season_comps.h) ---- */

static void cups_before(const SeasonChange *c)
{
    if (is_con_champ(c->comp) && c->started) {                  /* the next edition's club count, see above */
        int year = (int16_t)rd16(g_mem, c->comp + YEAR_AT);
        uint32_t list = rd32(g_mem, c->comp + CLUB_LIST_AT);    /* test log: the winner of the edition (fate 1) */
        for (int i = 0; list && concacaf_count(year) && i < (int16_t)rd16(g_mem, c->comp + CLUB_COUNT_AT) &&
                        i < MAX_CLUBS; i++) {
            uint32_t club = rd32(g_mem, list + 6u * (uint32_t)i);
            if (club && g_mem[list + 6u * (uint32_t)i + 5] == 1)
                fprintf(stderr, "recomp: concacaf champions %d: winner %.50s\n", year, win_str(club + 4));
        }
        int n = (int)rt_concacaf_clubs((uint32_t)(year + 1), 0);
        if (n) wr16(g_mem, c->comp + CLUB_COUNT_AT, (uint16_t)n);
    }
    const ClubCup *k = cup_of_object(c->comp);
    if (!k || !c->started) return;
    int next = (int16_t)rd16(g_mem, c->comp + YEAR_AT) + 1;     /* start_new_season adds 1 (0x92be3e) */
    /* Test log: the winner of the edition that ends (fate 1 in the club list, Cup slot 12 0x519aca reads it). */
    uint32_t list = rd32(g_mem, c->comp + CLUB_LIST_AT);
    for (int i = 0; list && i < (int16_t)rd16(g_mem, c->comp + CLUB_COUNT_AT) && i < MAX_CLUBS; i++) {
        uint32_t club = rd32(g_mem, list + 6u * (uint32_t)i);
        if (club && g_mem[list + 6u * (uint32_t)i + 5] == 1)
            fprintf(stderr, "recomp: club cup %s %d: winner %.50s\n", k->def->file, next - 1, win_str(club + 4));
    }
    Edition e;
    int y = next;
    while (y < SEASON_RULE_LAST + 2 && !edition(k, y, &e)) y++;
    if (y >= SEASON_RULE_LAST + 2) {
        season_change_skip();                                   /* it never comes back */
        fprintf(stderr, "recomp: club cup %s: no edition after %d\n", k->def->file, next - 1);
        return;
    }
    if (y != next) {
        wr16(g_mem, c->comp + YEAR_AT, (uint16_t)(y - 1));
        fprintf(stderr, "recomp: club cup %s waits for %d\n", k->def->file, y);
    }
}

const SeasonRuleKind season_kind_club_cups = {"club_cups", NULL, NULL, NULL, NULL, cups_before, NULL};
