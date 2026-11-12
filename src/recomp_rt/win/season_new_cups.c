/* New domestic cups (0.6.0 slice 21c, docs/research/2026-10-18-cups-more.md, section 3): the Turkish, Russian and
 * Norwegian Super Cups, the Russian Premier League Cup, the Conference League Cup and the All-Ireland cup, made by the
 * new competitions module (season_comps.h) with the formats of season_cups_more.c (season_cups.h).
 *
 * Each cup is an object of the class ItaSuper (constructor 0x65f670, thiscall (year, record) ret 8, size 0xb2 as
 * create_italian_comps 0x835320 allocates it; vtable 0x96c8b8, 40 slots as the Cup classes, sweep-europe 2.2), a Cup of
 * the original with one round table and one team list. The module makes it in the entry hook of the first season that
 * the format plays; this file gives it its own vtable copy (season_comps_def_vtable), so the season change, the
 * rules report and the wraps of season_super_cups.c know it by its file. The constructor calls the team picker
 * (init_team_list 0x65fa60) and create_round_table (0x65f920): their wraps (season_super_cups.c) call
 * season_new_cups_teams and season_new_cups_rounds below, which give the clubs of the cup's nation and its rounds.
 * The ruling body (+0x50, the byte that ItaSuper sets to Italy's 0x11 at 0x65f6c0) is that of the other objects of
 * the entry. A save keeps the object in its file (the module makes it again at a load, season_comps.h).
 *
 * A season with no edition: the before hook of start_new_season (kind "new_cups") skips the cup (season_change_skip)
 * and, when the format plays again, sets +0x40 to S - 1, so start_new_season (0x65fc90, inc word [esi+0x40]) builds
 * edition S with dates in the future (the third case of season_comps.h: a Cup that makes its fixtures at each draw).
 * An edition whose first match lies before the game date when the module wants to make it (a calendar-year cup at a
 * new game in July) is not made: the module asks again in the next season. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_cups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_DATE 0xae2c90u             /* s16 day of the year (0 = 1 January), s16 year */
#define CUP_YEAR 0x40u
#define CUP_TEAMS 0x3eu
#define CUP_TEAM_LIST 0xa7u
#define CUP_RULING_BODY 0x50u
#define ROUND_SIZE 0x68u
#define ITA_SUPER_VTABLE 0x0096c8b8u
#define ITA_SUPER_SIZE 0xb2u
#define CLUBS 0xae23b4u                 /* the club records, 0x245 bytes each */
#define CLUB_SIZE 0x245u
/* s32; the clubs are [0xae235c] - 2 * [0xae2350] (ItaCup::select_entrants 0x63fb2a) */
#define CLUB_COUNT 0xae235cu
#define CLUB_COUNT_LESS 0xae2350u
#define CLUB_DIVISION 0x57u             /* the comp record of the club's division (record +0 its id) */
#define CLUB_LAST_DIVISION 0x5bu        /* the division of the last season */
#define CLUB_LAST_PLACE 0x5fu           /* s8, the place in it (compare_clubs_by_seeding_pot 0x640590) */

#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */
#define F_ITA_SUPER 0x0065f670u         /* ItaSuper::ItaSuper, thiscall (year, record), ret 8 */
#define F_DRAW_DATE 0x00521e60u
#define F_MATCH_DATE 0x00521eb0u
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !readable(p)) return 0;
    memset(g_mem + p, 0, size);
    return p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p)) call_cdecl(F_FREE, 1, a);
}

/* ---- The defs (listed in season_comp_defs of season_comps.c) ---- */

static uint32_t new_make(const SeasonCompDef *d, uint32_t rec, int year, int loading);
static int new_held(const SeasonCompDef *d, int season);

/* Entries: 14 England, 20 Ireland, 24 Norway, 27 Russia, 32 Turkey (key_nation_create_all_comps 0x669180 calls the
 * creators in that order: create_english_comps 0x833750 is the 15th, create_turkish_comps 0x837d70 the 33rd). The
 * records: the Conference League Cup takes "English Conference Cup" of the data (record 95 in 3.9.68: key id
 * 0x9cf6a4, matched by set_key_ids 0x601a40, no object); the others get a new record like the cup of their nation. */
const SeasonCompDef season_def_tur_super = {"Turkey:super cup", "new_tur_super", "Turkish Super Cup", 32, new_make,
                                            "Turkish FA Cup", "Super Cup", 10, 0, new_held};
const SeasonCompDef season_def_rus_super = {"Russia:super cup", "new_rus_super", "Russian Super Cup", 27, new_make,
                                            "Russian Cup", "Super Cup", 10, 0, new_held};
const SeasonCompDef season_def_nor_super = {"Norway:Norwegian Super Cup", "new_nor_super", "Norwegian Super Cup", 24,
                                            new_make, "Norwegian Cup", "Super Cup", 7, 0, new_held};
const SeasonCompDef season_def_rus_lge_cup = {"Russia:league cup", "new_rus_lge_cup", "Russian Premier League Cup", 27,
                                              new_make, "Russian Cup", "Premier League Cup", 8, 0, new_held};
const SeasonCompDef season_def_conf_cup = {"England:Conference League Cup", "new_conf_cup", "English Conference Cup",
                                           14, new_make, NULL, NULL, 0, 0, new_held};
const SeasonCompDef season_def_all_ireland = {"Northern Ireland and Republic of Ireland:All-Ireland cup",
                                              "new_all_ireland", "Setanta Sports Cup", 20, new_make,
                                              "Irish League Cup", "Setanta Cup", 5, 0, new_held};

/* The clubs of a knockout cup: up to `count` clubs of a division (key id global), by their place in it in the last
 * season (clubs new to it after them). */
typedef struct {
    const char *file;
    uint32_t division[2];               /* key id globals (data map); 0: none */
} NewCupClubs;

static const NewCupClubs cup_clubs[] = {
    {"new_rus_lge_cup", {0x9cf930u, 0}},             /* Russian Premier Division */
    {"new_conf_cup", {0x9cf69cu, 0}},                /* English Conference */
    {"new_all_ireland", {0x9cf80cu, 0x9cf8b0u}},     /* Irish Premier Division, Northern Ireland Premier Division */
};

/* The super cups: the key id globals of the league and the cup, and the club that meets the champion when it won
 * both: 1 the cup runner-up, 0 the league runner-up (the rows of the rules table). */
typedef struct {
    const char *file;
    uint32_t league, cup;
    int double_cup_runner_up;
} NewSuper;

static const NewSuper supers[] = {
    {"new_tur_super", 0x9cf8d8u, 0x9cf8f8u, 1},      /* Turkish Premier Division, Turkish FA Cup */
    {"new_rus_super", 0x9cf930u, 0x9cf934u, 0},      /* Russian Premier Division, Russian Cup */
    {"new_nor_super", 0x9cf6bcu, 0x9cf734u, 0},      /* Norwegian Premier Division, Norwegian Cup */
};

static const SeasonCompDef *making;     /* the def whose constructor runs */

const char *season_new_cups_making(void) { return making ? making->file : NULL; }

static int season_of(uint32_t cup)
{
    if (!season_changing_rules()) return season_start_year();
    int y = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    return y >= SEASON_FIRST_YEAR && y <= SEASON_LAST_YEAR ? y : season_start_year();
}

/* A day of the year (0 = 1 January) of a day and month (0 to 11), leap years ignored: only for an order. */
static int day_of_year(int day, int month)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return before[month < 0 ? 0 : month > 11 ? 11 : month] + day - 1;
}

/* The first match of an edition, as year * 400 + day of the year. */
static int first_match(const SeasonNewCup *f, int edition, int clubs)
{
    int ties0 = 1, rounds = 1;
    if (f->clubs > 2) clubs = f->clubs;             /* a row with a club count: its own rounds (12 clubs: 4) */
    if (f->clubs != 2 && !season_new_cup_layout(clubs, &ties0, &rounds)) rounds = 1;
    int i = rounds - 1 < 5 ? rounds - 1 : 4;
    while (i > 0 && f->round[i].day == 0) i--;      /* the first round that the row gives */
    const SeasonNewRound *r = &f->round[i];
    return (edition + r->year) * 400 + day_of_year(r->day, r->month);
}

static int game_day(void) { return (int16_t)rd16(g_mem, GAME_DATE + 2) * 400 + (int16_t)rd16(g_mem, GAME_DATE); }

/* The edition that the entry hook of season S makes. With changing rules on: edition S (the hook of every season
 * runs, and a calendar-year cup's edition S lies in the entry's season S). With changing rules off only the start
 * season's hook runs (season_change.c, the pass of 0x5d5ae0), so it makes the edition that the start season's row
 * holds (Q232): for a cup in the row of the season before, edition S + 1 (the Russian Premier League Cup of March
 * 2003 in the row of 2002/03). */
static int hook_edition(const SeasonCompDef *d, int season)
{
    /* The module's record step at database_load 0x528540 of a new game runs before create_game 0x828110 sets the
     * game's flag (season_changing_rules is 0 until then): it takes the flag of the next new game, as `wanted` in
     * season_comps.c does. */
    int rules = season_game_on() ? season_changing_rules() : season_next_rules();
    return rules ? season : season_new_cup_row_edition(d->file, season);
}

static int new_held(const SeasonCompDef *d, int season)
{
    return season_new_cup_held(d->file, hook_edition(d, season));
}

static uint32_t new_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    if (!loading) year = hook_edition(d, year);
    const SeasonNewCup *f = season_new_cup(d->file, year);
    if (!loading && (!f || first_match(f, year, 32) <= game_day())) {
        fprintf(stderr, "recomp: new cups: %s %d not made, its first match is past\n", d->file, year);
        return 0;
    }
    /* A cup whose clubs the game does not hold (a nation or division not loaded) is not made: its picker has no
     * fall-back (the class's own picker reads Italy's key ids and stops the game at ita_super.cpp 203). */
    if (!loading && !season_new_cups_clubs_ready(d->file, year)) {
        fprintf(stderr, "recomp: new cups: %s %d not made, its clubs are not in the game\n", d->file, year);
        return 0;
    }
    Cpu saved = cpu;
    uint32_t size[1] = {ITA_SUPER_SIZE};
    uint32_t obj = rt_callback(F_NEW, 1, size);
    if (rt_unwinding || !readable(obj)) return 0;
    /* Neither ItaSuper 0x65f670 nor Cup::Cup 0x518640 sets the club list, and operator new does not clear it.
     * A picker that finds no clubs keeps the list, and put_list (season_super_cups.c) frees it (load sweep). */
    wr32(g_mem, obj + CUP_TEAM_LIST, 0);
    wr16(g_mem, obj + CUP_TEAMS, 0);
    uint32_t args[2] = {(uint32_t)year & 0xffffu, rec};
    making = d;
    cpu.ecx = obj;
    rt_callback(F_ITA_SUPER, 2, args);
    making = NULL;
    if (rt_unwinding) return 0;
    cpu = saved;
    uint32_t vt = season_comps_def_vtable(d, ITA_SUPER_VTABLE, 40);
    if (vt) wr32(g_mem, obj, vt);
    uint32_t others[64];
    int n = season_change_entry_comps(d->entry, others, 64);
    for (int i = 0; i < n; i++)
        if (others[i] != obj && readable(others[i])) {
            g_mem[obj + CUP_RULING_BODY] = g_mem[others[i] + CUP_RULING_BODY];
            break;
        }
    return obj;
}

/* ---- The clubs ---- */

static uint32_t placed(uint32_t key_global, int place)
{
    int32_t key = (int32_t)rd32(g_mem, key_global);
    uint32_t base = rd32(g_mem, 0xae23d0u);
    if (key < 0 || !readable(base)) return 0;
    uint32_t a[2] = {base + (uint32_t)key * 0x6bu, (uint32_t)place};
    uint32_t club = call_cdecl(0x004afd80u, 2, a);
    return rt_unwinding || !readable(club) ? 0 : club;
}

static int division_is(uint32_t club, uint32_t field, uint32_t key_global)
{
    uint32_t rec = rd32(g_mem, club + field);
    return readable(rec) && (int32_t)rd32(g_mem, rec) == (int32_t)rd32(g_mem, key_global);
}

/* The clubs of a division this season, best first: by the place of the last season in it, then the others. */
static int division_clubs(uint32_t key_global, uint32_t *out, int max)
{
    uint32_t base = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, CLUB_COUNT_LESS);
    int n = 0;
    int rank[64];
    for (int32_t i = 0; readable(base) && i < count && i < 200000 && n < max && n < 64; i++) {
        uint32_t club = base + (uint32_t)i * CLUB_SIZE;
        if (!division_is(club, CLUB_DIVISION, key_global)) continue;
        int place = (int8_t)g_mem[club + CLUB_LAST_PLACE];
        int r = division_is(club, CLUB_LAST_DIVISION, key_global) && place > 0 ? place : 100 + n;
        int k = n++;
        while (k > 0 && rank[k - 1] > r) out[k] = out[k - 1], rank[k] = rank[k - 1], k--;
        out[k] = club, rank[k] = r;
    }
    return n;
}

/* A new team list, as the class's picker makes one (ItaSuper::init_team_list 0x65fa60 mallocs it at 0x65fa72 and
 * stores it at 0x65fa7e without freeing the old one). */
static void put_list(uint32_t cup, const uint32_t *clubs, int n)
{
    uint32_t list = game_malloc(6u * (uint32_t)n);
    if (!list || rt_unwinding) return;
    for (int i = 0; i < n; i++) wr32(g_mem, list + 6u * (uint32_t)i, clubs[i]);
    wr32(g_mem, cup + CUP_TEAM_LIST, list);
    wr16(g_mem, cup + CUP_TEAMS, (uint16_t)n);
}

/* The clubs of edition `edition` of a new cup in clubs[] (32 at most), the clubs of the first round first. Returns
 * their count; fewer than 2: the cup cannot play (its nation's clubs are not in the game). */
static int pick_clubs(const char *file, int edition, uint32_t *clubs)
{
    const SeasonNewCup *f = season_new_cup(file, edition);
    if (!f) return 0;
    int n = 0;
    if (f->clubs == 2) {
        const NewSuper *k = NULL;
        for (size_t i = 0; i < sizeof supers / sizeof supers[0]; i++)
            if (!strcmp(supers[i].file, file)) k = &supers[i];
        if (!k) return 0;
        clubs[0] = placed(k->league, 0);
        clubs[1] = placed(k->cup, 0);
        if (clubs[1] == clubs[0]) clubs[1] = k->double_cup_runner_up ? placed(k->cup, 1) : placed(k->league, 1);
        if (rt_unwinding) return 0;
        n = clubs[0] && clubs[1] && clubs[0] != clubs[1] ? 2 : 0;
    } else {
        const NewCupClubs *k = NULL;
        for (size_t i = 0; i < sizeof cup_clubs / sizeof cup_clubs[0]; i++)
            if (!strcmp(cup_clubs[i].file, file)) k = &cup_clubs[i];
        if (!k) return 0;
        uint32_t a[32], b[32];
        int na = division_clubs(k->division[0], a, 32), nb = k->division[1] ? division_clubs(k->division[1], b, 32) : 0;
        int want = f->clubs > 0 ? f->clubs : 32;
        if (k->division[1]) {                       /* half from each league, the rest from the other */
            int ta = want / 2 < na ? want / 2 : na, tb = want - ta < nb ? want - ta : nb;
            if (ta + tb < want) ta = want - tb < na ? want - tb : na;
            for (int i = 0; i < ta; i++) clubs[n++] = a[i];
            for (int i = 0; i < tb; i++) clubs[n++] = b[i];
        } else {
            for (int i = 0; i < na && n < want; i++) clubs[n++] = a[i];
        }
        /* The best clubs get the byes: the list holds the clubs of the first round first (the lowest places). */
        for (int i = 0; i < n / 2; i++) {
            uint32_t t = clubs[i];
            clubs[i] = clubs[n - 1 - i], clubs[n - 1 - i] = t;
        }
    }
    return n;
}

int season_new_cups_clubs_ready(const char *file, int edition)
{
    uint32_t clubs[32];
    Cpu saved = cpu;
    int n = pick_clubs(file, edition, clubs);
    if (!rt_unwinding) cpu = saved;
    return n >= 2;
}

int season_new_cups_teams(uint32_t cup, const char *file)
{
    int edition = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    uint32_t clubs[32];
    int n = pick_clubs(file, edition, clubs);
    if (rt_unwinding) return 0;
    if (n < 2) {
        /* The list of the last edition stays (the module makes no cup without its clubs, new_make). */
        fprintf(stderr, "recomp: new cups: %s %d: %d clubs found, the last list stays\n", file, edition, n);
        return 0;
    }
    put_list(cup, clubs, n);
    if (rt_unwinding) return 0;
    fprintf(stderr, "recomp: new cups: %s %d with %d clubs\n", file, edition, n);
    return 1;
}

/* ---- The rounds ---- */

static void set_dates(uint32_t t, int round, const SeasonNewRound *r, uint32_t year)
{
    /* The draw 10 days before the first match. */
    int dd = r->day - 10, dm = r->month, dy = r->year;
    if (dd < 1) {
        dd += 28;
        dm--;
        if (dm < 0) dm = 11, dy--;
    }
    uint32_t d[7] = {t, (uint32_t)round, (uint32_t)dd, (uint32_t)dm, (uint32_t)(int32_t)dy, 0xffffffffu, year};
    call_cdecl(F_DRAW_DATE, 7, d);
    uint32_t m[9] = {t, (uint32_t)round, (uint32_t)r->day, (uint32_t)r->month, (uint32_t)(int32_t)r->year, 0xffffffffu,
                     1, year, 0};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m);
}

static void set_round(uint32_t rec, int index, int code, int clubs, int fresh, int first, const SeasonNewRound *r)
{
    wr16(g_mem, rec + 7, (uint16_t)code);
    wr16(g_mem, rec + 9, (uint16_t)index);
    wr16(g_mem, rec + 0xb, 0);
    if (r->legs == 2) {
        wr16(g_mem, rec + 0xd, 0);                     /* both legs, then the deciding flags on the second (0x521d76) */
        wr16(g_mem, rec + 0xf, r->flags);
        wr16(g_mem, rec + 0x11, r->flags);
    } else {
        wr16(g_mem, rec + 0xd, r->flags);
        wr16(g_mem, rec + 0xf, 0);
        wr16(g_mem, rec + 0x11, 0);
    }
    g_mem[rec + 0x17] = (uint8_t)(code >= 120 ? 6 : 4);
    wr16(g_mem, rec + 0x18, (uint16_t)clubs);
    wr16(g_mem, rec + 0x1a, (uint16_t)(clubs / 2));
    wr16(g_mem, rec + 0x1c, (uint16_t)fresh);
    wr16(g_mem, rec + 0x1e, (uint16_t)first);
    g_mem[rec + 0x20] = 0;
    g_mem[rec + 0x21] = (uint8_t)r->legs;
    g_mem[rec + 0x22] = (uint8_t)(r->legs == 2 ? r->gap : 0);
}

int season_new_cups_rounds(uint32_t cup, const char *file, uint32_t count_ptr)
{
    int edition = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    const SeasonNewCup *f = season_new_cup(file, edition);
    uint32_t one = cpu.eax;
    if (!f || !readable(one) || !readable(count_ptr)) return 0;
    int clubs = (int16_t)rd16(g_mem, cup + CUP_TEAMS), ties0 = 1, rounds = 1;
    if (f->clubs == 2 || clubs == 2) {
        if (clubs != 2) return 1;
        Cpu saved = cpu;
        set_round(one, 0, 150, 2, 2, 0, &f->round[0]);
        set_dates(one, 0, &f->round[0], (uint32_t)edition);
        if (rt_unwinding) return 1;
        cpu = saved;
        return 1;
    }
    if (!season_new_cup_layout(clubs, &ties0, &rounds) || rounds > 5) {
        fprintf(stderr, "recomp: new cups: %s %d: no rounds for %d clubs\n", file, edition, clubs);
        return 1;
    }
    Cpu saved = cpu;
    uint32_t t = game_malloc((uint32_t)rounds * ROUND_SIZE);
    if (!t) return 1;
    for (int r = 0; r < rounds; r++) memcpy(g_mem + t + (uint32_t)r * ROUND_SIZE, g_mem + one, ROUND_SIZE);
    game_free(one);
    if (rt_unwinding) return 1;
    int size = 2 * ties0, byes = clubs - 2 * ties0, teams = size;
    for (int r = 0; r < rounds && !rt_unwinding; r++) {
        const SeasonNewRound *spec = &f->round[rounds - 1 - r];
        if (r == 1) teams = byes ? ties0 + byes : size / 2;
        else if (r > 1) teams /= 2;
        int fresh = r == 0 ? size : (r == 1 ? byes : 0), first = r == 1 ? size : 0;
        set_round(t + (uint32_t)r * ROUND_SIZE, r, season_new_cup_round_code(rounds, r), teams, fresh, first, spec);
        set_dates(t, r, spec, (uint32_t)edition);
    }
    if (rt_unwinding) return 1;
    cpu = saved;
    cpu.eax = t;
    wr16(g_mem, count_ptr, (uint16_t)rounds);
    fprintf(stderr, "recomp: new cups: %s %d: %d rounds for %d clubs\n", file, edition, rounds, clubs);
    return 1;
}

/* ---- The season change: kind "new_cups" ---- */

static int new_file(const char *file) { return file && !strncmp(file, "new_", 4) && season_new_cup(file, 9999); }

static int season_game(void)
{
    const char *v = getenv("RECOMP_CUPS");
    if (v && !strcmp(v, "off")) return 0;
    return season_comps_on();
}

static void new_before(const SeasonChange *c)
{
    if (!season_game() || !c->started || !new_file(c->file)) return;
    int held = season_new_cup_held(c->file, c->season);
    int year = (int16_t)rd16(g_mem, c->comp + CUP_YEAR);
    if (held == 0) {
        season_change_skip();
        fprintf(stderr, "recomp: new cups: %s is not held in %d (year stays %d)\n", c->file, c->season, year);
    } else if (held == 1 && year + 1 < c->season) {
        wr16(g_mem, c->comp + CUP_YEAR, (uint16_t)(c->season - 1));
        fprintf(stderr, "recomp: new cups: %s resumes in %d (year %d to %d)\n", c->file, c->season, year,
                c->season - 1);
    }
}

/* One sentence when a new cup stops or comes back (Q258); the module says when it is played for the first time. */
static void new_after(const SeasonChange *c)
{
    if (!season_game() || !new_file(c->file)) return;
    int s = c->season, now = season_new_cup_held(c->file, s), before = season_new_cup_held(c->file, s - 1);
    if (now < 0 || before < 0 || now == before) return;
    uint32_t rec = rd32(g_mem, c->comp + 4);
    char name[64];
    snprintf(name, sizeof name, "%.50s", readable(rec) ? win_str(rec + 4) : "");
    if (!*name) return;
    if (now)
        season_change_news("The %s is played again in %d.", name, s);
    else if (season_new_cup_next(c->file, s) == SEASON_CUPS_NEVER)
        season_change_news("The %s is no longer played.", name);
    else
        season_change_news("The %s is not played in %d.", name, s);
}

const SeasonRuleKind season_kind_new_cups = {"new_cups", NULL, NULL, NULL, NULL, new_before, new_after};

/* ---- The ruling body of the All-Ireland cup ----
 *
 * The cup has the clubs of the Republic of Ireland and of Northern Ireland and one ruling body, that of the other
 * objects of its entry (new_make): the Irish body. Discipline::hold_bans_for_loan 0x5563f0 holds the bans of a
 * domestic body whose nation (+0x17) is not the loan club's (cmp at 0x55647b, je 0x55647d), and
 * Discipline::restore_held_bans 0x556570 drops them by the same test (cmp at 0x55671c, je 0x55671e). So a player of
 * an Irish club on loan at a Northern Irish club had his Irish bans held, met the Irish body in the cup, and the
 * body found the held ban (the box ruling_body..cpp 661, 0x7e833c). While the cup is in the game, the two nations
 * count as one for these tests: the bans of the cup's body stay with the player in both. The base game has no such
 * cup, so its tests are the original's. */
#define KEY_IRELAND 0x9cf2dcu
#define KEY_NORTHERN_IRELAND 0x9cf3e4u

static int all_ireland_in_game(void)
{
    uint32_t objs[1024];
    int n = season_change_comps(objs, 1024);
    for (int i = 0; i < n; i++)
        if (!strcmp(season_change_file(objs[i]), "new_all_ireland")) return 1;
    return 0;
}

/* 1 when a body of nation `body` applies at a club of nation `club` (nation ids) through the All-Ireland cup. */
uint32_t rt_ban_nation_applies(uint32_t club, uint32_t body)
{
    if (!season_code_on() || club == body) return 0;
    uint32_t ire = rd32(g_mem, KEY_IRELAND), ni = rd32(g_mem, KEY_NORTHERN_IRELAND);
    if (!((club == ire && body == ni) || (club == ni && body == ire))) return 0;
    return (uint32_t)all_ireland_in_game();
}
