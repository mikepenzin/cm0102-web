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
#include "season_ned_playoff.h"
#include "season_playoffs.h"

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
 * season (clubs new to it after them). `cup`: the national cup of each division's nation, for a row with
 * NEW_CUP_CUP_WINNERS (item 145). */
typedef struct {
    const char *file;
    uint32_t division[2];               /* key id globals (data map); 0: none */
    uint32_t cup[2];
} NewCupClubs;

/* The national cups of the All-Ireland cup (item 145): the cup of each nation in the key nation table
 * (key_nation_build_table 0x667150 reads 0x9cf840 and 0x9cf8c0), the FAI Cup ("Irish Senior Challenge Cup",
 * IreChalCup 0x6340a0) and the Irish Cup ("northern irish cup"). */
static const NewCupClubs cup_clubs[] = {
    {"new_rus_lge_cup", {0x9cf930u, 0}, {0, 0}},                    /* Russian Premier Division */
    {"new_conf_cup", {0x9cf69cu, 0}, {0, 0}},                       /* English Conference */
    {"new_all_ireland", {0x9cf80cu, 0x9cf8b0u}, {0x9cf840u, 0x9cf8c0u}},   /* Irish and Northern Ireland Premier
                                                                     * Divisions, the FAI Cup and the Irish Cup */
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
    return season_rule_year((int16_t)rd16(g_mem, cup + CUP_YEAR));
}

/* A day of the year (0 = 1 January) of a day and month (0 to 11), leap years ignored: only for an order. */
static int day_of_year(int day, int month)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return before[month < 0 ? 0 : month > 11 ? 11 : month] + day - 1;
}

/* The first match of an edition, as year * 400 + day of the year. An edition with a group stage (item 145): its first
 * group match day. */
static int first_match(const SeasonNewCup *f, const char *file, int edition, int clubs)
{
    const SeasonCupGroups *g = season_cup_groups(file, edition);
    if (g) return (edition + g->days[0][2]) * 400 + day_of_year(g->days[0][0], g->days[0][1]);
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
/* The changing rules flag of the game. The module's record step at database_load 0x528540 of a new game runs before
 * create_game 0x828110 sets the game's flag (season_changing_rules is 0 until then): it takes the flag of the next
 * new game, as `wanted` in season_comps.c does. */
static int rules_flag(void)
{
    return season_game_on() ? season_changing_rules() : season_next_rules();
}

static int hook_edition(const SeasonCompDef *d, int season)
{
    return rules_flag() ? season : season_new_cup_row_edition(d->file, season);
}

static int new_held(const SeasonCompDef *d, int season)
{
    return season_new_cup_held(d->file, hook_edition(d, season));
}

static uint32_t new_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    if (!loading) year = hook_edition(d, year);
    const SeasonNewCup *f = season_new_cup(d->file, year);
    if (!loading && (!f || first_match(f, d->file, year, 32) <= game_day())) {
        fprintf(stderr, "recomp: new cups: %s %d not made, its first match is past\n", d->file, year);
        return 0;
    }
    /* A cup whose clubs the game does not hold (a nation or division not loaded) is not made: its picker has no
     * fall-back (the class's own picker reads Italy's key ids and stops the game at ita_super.cpp 203). */
    if (!loading && !season_new_cups_clubs_ready(d->file, year, rec)) {
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

/* The format of edition `edition` of a cup that the game plays. With changing rules on: the edition's own. With
 * changing rules off the rules of the start season stay (season_rule_year, season_hooks_due): a cup that the start
 * season made plays every season in the format of the edition that the start season's row holds (hook_edition).
 * Open item 131: before, a 2008 game with changing rules off took the format of each later edition, so in 2024 the
 * Conference League Cup took the row "not played" (clubs -1, no rounds): set_dates gave each round a date of day 0,
 * and the game stopped at the box "Date..cpp 52" (run set game rr2008-egr, 15 June 2024). */
static const SeasonNewCup *played_format(const char *file, int edition)
{
    return season_new_cup_played(file, edition, rules_flag(), season_start_year());
}

static int has_club(const uint32_t *list, int n, uint32_t club)
{
    for (int i = 0; i < n; i++)
        if (list[i] == club) return 1;
    return 0;
}

/* The last winner of a competition record (comp_util_get_latest_placed_club 0x4afd80, place 0), or 0. */
static uint32_t record_winner(uint32_t rec)
{
    if (!readable(rec)) return 0;
    uint32_t a[2] = {rec, 0};
    uint32_t club = call_cdecl(0x004afd80u, 2, a);
    return rt_unwinding || !readable(club) ? 0 : club;
}

/* Item 145: the clubs of a row with picks, by the rules of its edition: picks[i] clubs of division i by last place,
 * the national cup winners (NEW_CUP_CUP_WINNERS) or the holders of this cup (NEW_CUP_HOLDERS, `rec` its record).
 * A place whose club already has one goes to the next club of the division (cup winners: the cup's division;
 * holders: the first division). Returns the count. */
static int pick_by_rule(const SeasonNewCup *f, const NewCupClubs *k, uint32_t rec, uint32_t *clubs)
{
    uint32_t list[2][32];
    int count[2] = {0, 0}, next[2] = {0, 0}, n = 0;
    for (int d = 0; d < 2; d++)
        if (k->division[d]) count[d] = division_clubs(k->division[d], list[d], 32);
    /* A cup of two leagues needs the clubs of both (a game with the Republic of Ireland and not Northern Ireland has
     * no Irish League): with fewer clubs than its picks in a division, the edition is not made (new_make). */
    for (int d = 0; d < 2; d++)
        if (count[d] < f->picks[d]) return 0;
    for (int d = 0; d < 2; d++)
        while (next[d] < f->picks[d] && next[d] < count[d] && n < 32) clubs[n++] = list[d][next[d]++];
    for (int d = 0; d < 2 && (f->extra & NEW_CUP_CUP_WINNERS); d++) {
        uint32_t club = placed(k->cup[d], 0);
        if (rt_unwinding) return 0;
        /* A cup winner of a lower division or of the other nation's league: in, as real life (2005: Longford Town). */
        if (!club || has_club(clubs, n, club)) {
            while (next[d] < count[d] && has_club(clubs, n, list[d][next[d]])) next[d]++;
            club = next[d] < count[d] ? list[d][next[d]++] : 0;
        }
        if (club && n < 32) clubs[n++] = club;
    }
    if (f->extra & NEW_CUP_HOLDERS) {
        uint32_t club = record_winner(rec);
        if (rt_unwinding) return 0;
        if (!club || has_club(clubs, n, club)) {
            while (next[0] < count[0] && has_club(clubs, n, list[0][next[0]])) next[0]++;
            club = next[0] < count[0] ? list[0][next[0]++] : 0;
        }
        if (club && n < 32) clubs[n++] = club;
    }
    return n;
}

/* The clubs of edition `edition` of a new cup in clubs[] (32 at most), the clubs of the first round first. Returns
 * their count; fewer than 2: the cup cannot play (its nation's clubs are not in the game). `rec`: the cup's record (0
 * when not known), for the holders. */
static int pick_clubs(const char *file, int edition, uint32_t rec, uint32_t *clubs)
{
    const SeasonNewCup *f = played_format(file, edition);
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
        if (f->picks[0] || f->picks[1]) return pick_by_rule(f, k, rec, clubs);
        uint32_t a[32], b[32];
        int na = division_clubs(k->division[0], a, 32), nb = k->division[1] ? division_clubs(k->division[1], b, 32) : 0;
        int want = f->clubs > 0 ? f->clubs : 32;
        if (k->division[1]) {                       /* half from each league, the rest from the other */
            int ta = want / 2 < na ? want / 2 : na, tb = want - ta < nb ? want - ta : nb;
            if (ta + tb < want) ta = want - tb < na ? want - tb : na;
            /* Open item 148: the two leagues in turn by place (1st, 1st, 2nd, 2nd, ...), so the byes go to the best of
             * both. The 2011 byes went to the two champions and the two cup winners (BBC, RTE, Irish Times; the
             * Setanta doc); before, all four went to League of Ireland clubs and the Irish League champion played the
             * first round. */
            for (int i = 0; i < ta || i < tb; i++) {
                if (i < ta) clubs[n++] = a[i];
                if (i < tb) clubs[n++] = b[i];
            }
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

int season_new_cups_clubs_ready(const char *file, int edition, uint32_t record)
{
    uint32_t clubs[32];
    Cpu saved = cpu;
    int n = pick_clubs(file, edition, record, clubs);
    if (!rt_unwinding) cpu = saved;
    return n >= 2;
}

int season_new_cups_teams(uint32_t cup, const char *file)
{
    int edition = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    uint32_t clubs[32];
    int n = pick_clubs(file, edition, rd32(g_mem, cup + 4), clubs);
    if (rt_unwinding) return 0;
    if (n < 2) {
        /* The list of the last edition stays (the module makes no cup without its clubs, new_make). */
        fprintf(stderr, "recomp: new cups: %s %d: %d clubs found, the last list stays\n", file, edition, n);
        return 0;
    }
    put_list(cup, clubs, n);
    if (rt_unwinding) return 0;
    fprintf(stderr, "recomp: new cups: %s %d with %d clubs\n", file, edition, n);
    /* Item 145: each club with the division it came from (1 or 2 in the cup's list, 0 another), for the run checks. */
    const NewCupClubs *k = NULL;
    for (size_t i = 0; i < sizeof cup_clubs / sizeof cup_clubs[0]; i++)
        if (!strcmp(cup_clubs[i].file, file)) k = &cup_clubs[i];
    for (int i = 0; k && k->division[1] && i < n; i++) {
        int from = division_is(clubs[i], CLUB_DIVISION, k->division[0]) ? 1
                 : division_is(clubs[i], CLUB_DIVISION, k->division[1]) ? 2 : 0;
        fprintf(stderr, "recomp: new cups: %s %d club %d: %.50s (division %d, last place %d)\n", file, edition, i + 1,
                win_str(clubs[i] + 4), from, (int8_t)g_mem[clubs[i] + CLUB_LAST_PLACE]);
    }
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
    const SeasonNewCup *f = played_format(file, edition);
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

/* ---- The draw day of a knockout edition (open item 148) ----
 *
 * The knockout editions of the All-Ireland cup (2010/11 on) took their clubs when the module made the cup's season, in
 * the entry of the Republic of Ireland (20), before the Northern Irish Premier Division records its final places: its
 * start_new_season 0x78ee30 calls the chain 0x689c20, whose League::record_final_positions 0x68aa30 writes club +0x5b
 * and +0x5f (0x68aa5d) in a later entry. So the Irish League clubs came in list order, not by place (item 145 found the
 * same in the group editions and picks them on the draw day, season_cup_groups.c). A knockout's clubs meet at the draw
 * of its first round: Cup::update_for_date 0x51f890 calls Cup::draw_round 0x51c410 on that round's draw day
 * (0x51fa7d), when both leagues hold the places of the season just ended. Before that draw the cup picks again and
 * writes the clubs in the same slots of its team list: map_fixture_tree 0x518790 gave the slots their rounds (the clubs
 * of the first round first, the byes after them, as pick_clubs orders them), so a list of the same count keeps the
 * tree. A pick of another count (a league with fewer clubs than at the season change) keeps the list. A cup with one
 * league (the Conference League Cup, the Russian Premier League Cup) had its places at the change and is not touched. */
#define CUP_STAGE_COUNT 0x2cu
#define CUP_BASE_DONE 0xb1u
#define CUP_TIES 0xadu
#define TIE_STATE 0x34u

static const NewCupClubs *two_leagues(const char *file)
{
    for (size_t i = 0; i < sizeof cup_clubs / sizeof cup_clubs[0]; i++)
        if (!strcmp(cup_clubs[i].file, file)) return cup_clubs[i].division[1] ? &cup_clubs[i] : NULL;
    return NULL;
}

static void draw_day_pick(uint32_t cup, int round)
{
    if (round != 0) return;
    const char *file = season_change_file(cup);
    const NewCupClubs *k = file && !strncmp(file, "new_", 4) ? two_leagues(file) : NULL;
    if (!k) return;
    if (g_mem[cup + CUP_BASE_DONE] || (int32_t)rd32(g_mem, cup + CUP_STAGE_COUNT) != 0) return;   /* groups */
    uint32_t ties = rd32(g_mem, cup + CUP_TIES), first = readable(ties) ? rd32(g_mem, ties) : 0;
    if (!readable(first) || g_mem[first + TIE_STATE] != 0) return;      /* drawn already */
    int edition = (int16_t)rd16(g_mem, cup + CUP_YEAR), count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST), clubs[32];
    if (!readable(list) || count < 2 || count > 32) return;
    int n = pick_clubs(file, edition, rd32(g_mem, cup + 4), clubs);
    if (rt_unwinding) return;
    if (n != count) {
        fprintf(stderr, "recomp: new cups: %s %d: %d clubs on the draw day, %d in the list; the list stays\n", file,
                edition, n, count);
        return;
    }
    for (int i = 0; i < n; i++) wr32(g_mem, list + 6u * (uint32_t)i, clubs[i]);
    fprintf(stderr, "recomp: new cups: %s %d: clubs picked again on the draw day\n", file, edition);
    for (int i = 0; i < n; i++) {
        int from = division_is(clubs[i], CLUB_DIVISION, k->division[0]) ? 1
                 : division_is(clubs[i], CLUB_DIVISION, k->division[1]) ? 2 : 0;
        fprintf(stderr, "recomp: new cups: %s %d club %d: %.50s (division %d, last place %d)\n", file, edition, i + 1,
                win_str(clubs[i] + 4), from, (int8_t)g_mem[clubs[i] + CLUB_LAST_PLACE]);
    }
}

/* ---- The paired first round (open item 148, the 2011 to 2014 rule) ----
 *
 * In the All-Ireland editions of 2010/11 to 2013/14 each tie of the first round drawn paired a League of Ireland club
 * with an Irish League club; the later rounds were open (docs/research/2026-11-05-setanta-cup.md). The first round of
 * these rows is an open draw: Cup::draw_round 0x51c410 calls Cup::randomize_round_teams 0x51e220 (no 0x400 seeding at
 * +0xd, no nation rule at +0xb, no 0x200 keep), which takes every side out, shuffles them with utils_random 0x90cfc0
 * and puts each in a random tie. The nation draw 0x51d9f0 does not fit: it keeps clubs of one nation apart by club
 * +0x53, and Derry City, a League of Ireland club, is of Northern Ireland. So, for a row with NEW_CUP_PAIRED, the
 * wrap of 0x51c410 draws round 0 by division before the original runs: the clubs of each league in a random order
 * (0x90cfc0, as the original), tie t gets the t-th club of each league (the home sides come from 0x51e870 after the
 * draw). Then the wrap of 0x51e220
 * skips its shuffle for that one call, and draw_round goes on as before (home sides 0x51e870, second legs 0x522610,
 * the draw news). Round 0 has no source ties (+0xa, +0xc, +0xe, +0x10 all -1), so no winner link moves. A round whose
 * sides are not 2 x ties known clubs, half from each league (a game with one of the two nations), keeps the open
 * draw and says so in the log. */
#define TIE_SIZE 0x35u
#define F_RANDOM 0x0090cfc0u            /* utils_random, cdecl (n): 0 to n - 1 */

static uint32_t paired_cup;             /* the cup and round that paired_draw drew, for the wrap of 0x51e220 */
static int paired_round = -1;

static int game_random(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    return (int)call_cdecl(F_RANDOM, 1, a);
}

static void shuffle(int16_t *v, int n)
{
    for (int i = 0; i < n - 1 && !rt_unwinding; i++) {
        int j = i + game_random(n - i);
        if (rt_unwinding) return;
        int16_t t = v[i];
        v[i] = v[j], v[j] = t;
    }
}

/* Returns 1 when round 0 is drawn. */
static int paired_draw(uint32_t cup, int round)
{
    if (round != 0) return 0;
    const char *file = season_change_file(cup);
    const NewCupClubs *k = file && !strncmp(file, "new_", 4) ? two_leagues(file) : NULL;
    if (!k) return 0;
    const SeasonNewCup *f = played_format(file, (int16_t)rd16(g_mem, cup + CUP_YEAR));
    if (!f || !(f->extra & NEW_CUP_PAIRED)) return 0;
    if (g_mem[cup + CUP_BASE_DONE] || (int32_t)rd32(g_mem, cup + CUP_STAGE_COUNT) != 0) return 0;
    uint32_t recs = rd32(g_mem, cup + 0xa3u), arr = rd32(g_mem, cup + CUP_TIES);
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST);
    if (!readable(recs) || !readable(arr) || !readable(list)) return 0;
    uint32_t ties = rd32(g_mem, arr);
    int n = (int16_t)rd16(g_mem, recs + 0x1au), count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    if (!readable(ties) || n < 1 || n > 16) return 0;
    int16_t a[32], b[32];
    int na = 0, nb = 0;
    for (int t = 0; t < n; t++) {
        uint32_t tie = ties + TIE_SIZE * (uint32_t)t;
        if (g_mem[tie + TIE_STATE] != 0) return 0;
        for (int side = 0; side < 2; side++) {
            int16_t idx = (int16_t)rd16(g_mem, tie + 2u * (uint32_t)side);
            if ((int16_t)rd16(g_mem, tie + 0xau + 2u * (uint32_t)side) != -1 ||
                (int16_t)rd16(g_mem, tie + 0xeu + 2u * (uint32_t)side) != -1)
                return 0;                                       /* a winner to come */
            if (idx < 0 || idx >= count) return 0;
            uint32_t club = rd32(g_mem, list + 6u * (uint32_t)idx);
            if (!readable(club)) return 0;
            if (division_is(club, CLUB_DIVISION, k->division[0])) {
                a[na++] = idx;
            } else if (division_is(club, CLUB_DIVISION, k->division[1])) {
                b[nb++] = idx;
            } else {
                fprintf(stderr, "recomp: new cups: %s: a first-round club of neither league, an open draw\n", file);
                return 0;
            }
        }
    }
    if (na != n || nb != n) {
        fprintf(stderr, "recomp: new cups: %s %d: %d and %d clubs of the two leagues in the first round, an open draw\n",
                file, (int16_t)rd16(g_mem, cup + CUP_YEAR), na, nb);
        return 0;
    }
    shuffle(a, n);
    shuffle(b, n);
    if (rt_unwinding) return 0;
    for (int t = 0; t < n; t++) {
        uint32_t tie = ties + TIE_SIZE * (uint32_t)t;
        wr16(g_mem, tie, (uint16_t)a[t]);
        wr16(g_mem, tie + 2u, (uint16_t)b[t]);
    }
    return 1;
}

/* The ties of round 0 as draw_round left them (after the home sides), for the run checks. */
static void log_first_round(uint32_t cup)
{
    const char *file = season_change_file(cup);
    uint32_t recs = rd32(g_mem, cup + 0xa3u), arr = rd32(g_mem, cup + CUP_TIES), list = rd32(g_mem, cup + CUP_TEAM_LIST);
    if (!file || !readable(recs) || !readable(arr) || !readable(list) || !readable(rd32(g_mem, arr))) return;
    uint32_t ties = rd32(g_mem, arr);
    int n = (int16_t)rd16(g_mem, recs + 0x1au), edition = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    fprintf(stderr, "recomp: new cups: %s %d: first round paired by league\n", file, edition);
    for (int t = 0; t < n && t < 16; t++) {
        uint32_t tie = ties + TIE_SIZE * (uint32_t)t;
        int16_t hi = (int16_t)rd16(g_mem, tie), wi = (int16_t)rd16(g_mem, tie + 2u);
        uint32_t h = hi >= 0 ? rd32(g_mem, list + 6u * (uint32_t)hi) : 0, w = wi >= 0 ? rd32(g_mem, list + 6u * (uint32_t)wi) : 0;
        fprintf(stderr, "recomp: new cups: %s %d tie %d: %.50s v %.50s\n", file, edition, t + 1,
                readable(h) ? win_str(h + 4) : "?", readable(w) ? win_str(w + 4) : "?");
    }
}

static int season_game(void);

void o_0051c410(void);

/* Cup::draw_round, thiscall (round) ret 4: a new cup of two leagues picks its clubs again before its first draw, and
 * a row with NEW_CUP_PAIRED draws its first round by league. */
RT_WRAP(0051c410, new_cups_draw_round)
{
    uint32_t cup = cpu.ecx;
    int drawn = 0, round = (int16_t)rt_arg(0);
    if (season_game() && readable(cup)) {
        Cpu saved = cpu;
        draw_day_pick(cup, round);
        if (rt_unwinding) return;
        drawn = paired_draw(cup, round);
        if (rt_unwinding) return;
        cpu = saved;
    }
    if (drawn) paired_cup = cup, paired_round = round;
    season_playoffs_draw_round(cup, round);         /* item 273: the semi-finals of the National League play-off */
    season_ned_playoff_draw_round(cup, round);      /* issue 150: the ties of the Dutch nacompetitie */
    o_0051c410();
    paired_cup = 0, paired_round = -1;
    if (drawn && !rt_unwinding) log_first_round(cup);
}

void o_0051e220(void);

/* Cup::randomize_round_teams, thiscall (round) ret 4: the round that paired_draw drew keeps its ties. */
RT_WRAP(0051e220, new_cups_open_draw)
{
    if (paired_cup && cpu.ecx == paired_cup && (int16_t)rt_arg(0) == paired_round) {
        paired_cup = 0, paired_round = -1;
        rt_ret_stdcall(0, 1);
        return;
    }
    o_0051e220();
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

/* One sentence when a new cup stops or comes back (Q258). The module writes the sentence of the change that makes
 * it. The sentence names the game season (item 145: "not played in 2008" read as if the Setanta Cup of 2008, which
 * was played, had not been). */
static void new_after(const SeasonChange *c)
{
    if (!season_game() || !new_file(c->file)) return;
    int s = c->season, now = season_new_cup_held(c->file, s), before = season_new_cup_held(c->file, s - 1);
    if (now < 0 || before < 0 || now == before) return;
    /* 0.6.2 item 135: the module made the object at this change and wrote its one sentence (first time, or again
     * for a cup with real editions before). The Conference Cup of 2004/05 got two. */
    if (season_comps_made_in(c->comp) == s) return;
    uint32_t rec = rd32(g_mem, c->comp + 4);
    char name[64];
    snprintf(name, sizeof name, "%.50s", readable(rec) ? win_str(rec + 4) : "");
    if (!*name) return;
    if (now)
        season_change_news("The %s is played again in the %d/%02d season.", name, s, (s + 1) % 100);
    else if (season_new_cup_next(c->file, s) == SEASON_CUPS_NEVER)
        season_change_news("The %s is no longer played.", name);
    else
        season_change_news("The %s is not played in the %d/%02d season.", name, s, (s + 1) % 100);
}

const SeasonRuleKind season_kind_new_cups = {"new_cups", NULL, NULL, NULL, NULL, new_before, new_after};

/* ---- The late start of the All-Ireland cup ----
 *
 * The cup is an object of Ireland's league table entry 20 (new_make), and game_season_update_pass 0x5d5ae0 starts a
 * competition only on its entry's update day and only when it is complete (vtable +0xc at 0x5d5ccb). The original's
 * entry 20 has its update day on 20 June (key_nation_build_table 0x667150, 0x66834b..0x668443), after each final of the
 * cup (the latest is 21 May, season_cups_more.c). From the calendar move of 2003 the entry's update day is 20 February
 * (season_calendar.h), inside each edition (draw in December or January, matches from January or February to May),
 * so the cup missed every change it was running at: the release set lost the editions of 2005/06 and 2007/08 in a
 * 2004 game (row sp2004: edition 2006 at the end of 2007/08). On 20 June, the cup's old update day, a cup of an entry
 * whose update day is another day starts the season of the game year when it is complete, its edition is before that
 * season and the table holds it then (new_held, as the module makes it): the same start as the pass (resumes,
 * not-held seasons and the hooks through the before hook above). The next first match of each row is in July or later
 * (2009/10: 28 July 2009), so no edition starts with past dates (test_season_cups_more.c checks the rows). With the
 * entry on 20 June (the original calendar: changing rules off and a start in 2001 or 2002) the pass starts the cup and
 * this does nothing. A game that starts in 2003 or later has the calendar move with changing rules off too, and its
 * cup plays every season in its start format (season_new_cup_played): there the late start runs with no hooks, as
 * the pass would, and the cup no longer stops after its first edition. The day is the entry's fixed day 170
 * (SEASON_NEW_CUP_LATE_DAY), so 19 June in a leap year, as the original. */
void season_new_cups_day(void)
{
    if (!season_game()) return;
    int year = (int16_t)rd16(g_mem, GAME_DATE + 2), day = (int16_t)rd16(g_mem, GAME_DATE);
    if (day != SEASON_NEW_CUP_LATE_DAY) return;
    uint32_t objs[64], cup = 0;
    int n = season_change_entry_comps(season_def_all_ireland.entry, objs, 64);
    for (int i = 0; i < n && !cup; i++)
        if (readable(objs[i]) && !strcmp(season_change_file(objs[i]), season_def_all_ireland.file)) cup = objs[i];
    if (!cup) return;
    int edition = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    if (!season_new_cup_late(day, season_change_start_day(cup), edition, year, new_held(&season_def_all_ireland, year)))
        return;
    if (season_change_late_start(cup))
        fprintf(stderr, "recomp: new cups: %s starts %d late (edition %d to %d, the entry's update day is day %d)\n",
                season_def_all_ireland.file, year, edition, (int16_t)rd16(g_mem, cup + CUP_YEAR),
                season_change_start_day(cup));
}

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
