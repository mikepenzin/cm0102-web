/* The dates of international friendlies in a season game (row A18 of the 0.6.0 release test, O375, O390).
 *
 * Friendly::build_international_date_table 0x5ca4f0 (called once, from Friendly::Friendly 0x5c1f80 at 0x5c2180,
 * also when a save loads) writes 110 rows of 0x14 bytes at [this + 0x34ec], count [this + 0x34f0]: +0 a weekday
 * code, +1 the day, +2 the month (0 to 11), +3 the Date (8 bytes: s16 day of the year, s16 year, a word), +0xb a
 * year offset 0 to 3, +0xc the continent (the value of a key continent), +0x10 a key id. Its dates are the FIFA
 * windows of 2001. Then 0x5cc510 (called at 0x5cb98a) sets each Date with Date::set 0x54a130 for the World Cup year
 * of the cycle plus the offset (0x5cc56b..0x5cc5ac) and sorts the rows by date (qsort 0x945333 with 0x5cc610). The
 * arranger arrange_international_friendlies 0x5cbdf0 finds the rows of a date by bsearch and pairs nations of the
 * row's continent (docs/research/2026-10-18-national-team-comps.md, section 7).
 *
 * The cause. Those 2001 days often fall on the match days of the UEFA club competitions in a season game: the
 * qualifying rounds in July and August, the groups from September, the knockout rounds on the real dates of
 * season_uefa_dates.c (O378). NationalTeams::can_schedule_club_fixture 0x776fc0 then refuses a club's tie when more
 * than two of its players are on international duty within a day: FIXTURE_MANAGER::find_fixture_clashes 0x5ad780
 * gives clash 21 (0x5adb6b), and add_fixture 0x5aa680 moves the tie a week (0x5ab510 at 0x5aa96e). In a 2006 game
 * (gate (b) run on eee3e71a, 2009/10) the Champions League third qualifying round tie Aalborg v Borussia Dortmund
 * moved from 19 to 26 August (the August row), so its losers reached the Europa League after the play-off draw was
 * due (row A18, "drawn late"); in 2016/17 the second qualifying round tie Keflavik v Espanyol moved from 27 July (a
 * July row) and each round after it followed; in 2015/16 the Europa League tie Gent v Juventus on 26 April got clash
 * 21 too (an April row). The real FIFA calendar keeps its match days off the UEFA days.
 *
 * The fix, in a running season of a season game: each row takes the real FIFA match day of its month and year that
 * lies nearest to its own day (season_fifa_days, from the results of each day counted in two lists,
 * season_fifa_days_table.c). A month with no FIFA match day that year (July in every year, August from 2014, January and most Decembers) gives the row
 * none: its continent becomes -2, which no continent has, so the arranger pairs no one on it. (-1 would be wrong:
 * the arranger reads a continent of -1 as every continent, 0x5cc060..0x5cc070.) After the last year of
 * the list, the days of its last year move on by 364 days a year (plus 7 when that drifts over 3 days), as the UEFA
 * dates do (O381). The rows are sorted again. The base game keeps the original's table (Q152).
 *
 * The qualifying matches (O390, option Q). The qualifying groups of the World Cup and the European Championship are
 * leagues: their fixtures come from League::create_fixtures 0x6835c0 (add_fixture calls at 0x683a55 and 0x683dee)
 * and, for the South American section, WcSouthAmericanLeague::add_fixed_fixture (0x9225c1). Their days follow the
 * original's templates moved to each cycle, so they also fall on UEFA days: in a 2006 game the CONCACAF and
 * CONMEBOL matches of 5 August 2009 put Internazionale's internationals on duty for its Europa League tie of
 * 6 August (clash 21); real: CONCACAF 12 August, CONMEBOL 5 and 9 September. The Under 21 match of the same pair
 * comes the day before the senior match (ReserveComp::add_b_or_under21_fixture, the add_fixture call at 0x7e6a48),
 * so it is not moved here. In a running season of a season game, a fixture of two national sides from those calls,
 * in a competition that is not a finals tournament (national_teams_is_finals_comp 0x76d940), moves to the nearest
 * real FIFA match day of the same year, 2 or more days after today, on which no side of either nation (senior, B,
 * Under 21) has a national match yet and no continental club match is played (fix_man_get_fixture_list_for_date
 * 0x5ae6a0, all fixture types). A side's two matches of a
 * window so take two days. The search covers the whole year: a July matchday made after the June window has its
 * nearest free day in September (CONMEBOL on 6 July 2016 met a Champions League tie, CONCACAF on 12 July 2028 a
 * Europa League tie 71 days before the window). With no such day the fixture keeps its date (logged). The legs of a
 * two-legged qualifying tie (Cup::create_tie_fixtures) do not move one by one, so a second leg never comes before
 * its first: the first leg plans both on the FIFA days of one window (item 207, tie_snap below). */
#include "win.h"
#include "season.h"
#include "season_uefa_days.h"
#include "season_conmebol_days.h"
#include "season_stage_plan.h"
#include "season_tie_legs.h"
#include "season_names.h"
#include "season_intl.h"
#include "season_change.h"
#include "season_nl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 0x34ecu
#define ROW_COUNT 0x34f0u
#define ROW_SIZE 0x14u
#define F_DATE 0x00549f70u              /* Date::Date, thiscall (day, month 0 to 11, year, weekday; -1 the date) */
#define F_QSORT 0x00945333u
#define F_ROW_ORDER 0x005cc610u
#define F_FINALS 0x0076d940u             /* national_teams_is_finals_comp, stdcall (record) ret 4 */
#define F_DAY_LIST 0x005ae6a0u           /* fix_man_get_fixture_list_for_date, thiscall (Date *, slice, type): the
                                         * store's own list of the day (0x5abfd7..0x5abfda), not a copy */
#define FIXTURES 0xae2a58u
#define TYPE_COUNT 0xb64738u              /* s8: the fixture types of a day (key nation entries) */
#define GAME_DATE 0xae2c90u             /* s16 day of the year, s16 year */
#define FIFA_DOYS_MAX 128               /* FIFA days of one year, at most */
#define SNAP_DAYS 366                   /* the search for a FIFA day, either way: the whole year (the gap from
                                         * the June to the September window is 107 days, 168 in a World Cup year) */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define CLUB_SIZE 0x245u
#define NO_CONTINENT 0xfffffffeu        /* -2: no continent has it (-1 is every continent at 0x5cc060) */
#define NATION_CONTINENT 0x71u          /* nation record: its continent record, whose +0 is the continent id */
#define KEY_CONTINENTS 0x9cfa08u        /* the key continent ids, Africa to South America (season_intl.h) */


static int readable(uint32_t p) { return rt_readable(p); }

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int days_in(int y) { return is_leap(y) ? 366 : 365; }
#define day_of_year season_day_of_year
#define from_day_of_year season_from_day_of_year
static int fifa_day(int y, int m, int want) { return season_fifa_day(y, m, want); }

void o_005cc510(void);

static uint32_t friendly_obj;            /* the Friendly object of the game (0x5cc510's this) */
static int moved_season = -1;           /* the season whose start moved the rows */
static int pending_count;               /* the qualifiers that wait for the first running season (route M) */
static void move_pending(void);

/* The season of today: from 1 July, today's year. */
static int season_now(void)
{
    int y = (int16_t)rd16(g_mem, GAME_DATE + 2), m, d;
    season_from_day_of_year(y, (int16_t)rd16(g_mem, GAME_DATE), &m, &d);
    return m >= 7 ? y : y - 1;
}

static void snap_rows(uint32_t self);

/* 1 when season s keeps the original's international calendar: the start season of a 2001 start (O405: a 2001/02 new
 * game with changing rules is the base game in its first season, the same-save new-game set), and the run to its
 * start date. A start in a later year has no base game to match: its start season takes the real calendar as a
 * running one does (the 2011 start: the original's South American friendly day, 17 August 2011, moved Vaslui v
 * Benfica a week for duty, and the Europa League play-off draw came late, gate (a) run 8 on b6a4c3ad). */
static int keeps_original(int s)
{
    return s < season_start_year() || (s == season_start_year() && season_start_year() == SEASON_FIRST_YEAR);
}

int season_friendly_keeps_original_now(void) { return keeps_original(season_now()); }

/* Running seasons only (O405, the maintainer, 26 October 2026; keeps_original): in the start season of a 2001 start the
 * table stays the original's, as
 * that season keeps the original's qualifying and international dates. The table holds the rows of later seasons
 * from the start, and moving them changed a 2001/02 new game with changing rules in its first week (same-save
 * new-game set: Friendly.tmp and the national squads). So the rows move when the game is in a running season:
 * here when the table is made or loaded in one, else at the first day of the first running season
 * (season_friendly_dates_day, from the daily season pass 0x5d5ae0). */
RT_WRAP(005cc510, friendly_set_row_dates)
{
    uint32_t self = cpu.ecx;
    o_005cc510();
    if (rt_unwinding || !season_game_changes() || !readable(self)) return;
    friendly_obj = self;
    if (keeps_original(season_now())) return;           /* a new game makes the table on 1 January of its year */
    moved_season = season_now();
    snap_rows(self);
}

void season_friendly_dates_day(void)
{
    if (!season_game_changes()) return;
    int now = season_now();
    if (keeps_original(now)) return;
    if (friendly_obj && readable(friendly_obj) && now != moved_season) {
        moved_season = now;
        snap_rows(friendly_obj);
    }
    if (!rt_unwinding && pending_count) move_pending();
}

void season_fifa_stage_end(void);

/* A new or loaded game: also the stage bound and plan, which a create_fixtures that unwound did not end. */
static int held_count;                   /* item 215: the rows that snap_rows keeps for a Nations League */
void season_friendly_dates_reset(void)
{
    friendly_obj = 0, moved_season = -1, pending_count = 0, held_count = 0;
    season_fifa_stage_end();
}

/* Item 215: the confederation of the Nations League whose nations a row of continent `cont` pairs, else NULL
 * (the key continents of 0x5ca4f0: Europe 0x9cfa10, North America 0x9cfa14). */
static const char *nl_confed(uint32_t cont)
{
    if (cont == rd32(g_mem, 0x9cfa10u)) return "UEFA";
    if (cont == rd32(g_mem, 0x9cfa14u)) return "CONCACAF";
    return NULL;
}

/* Item 215: a row that snap_rows keeps for a Nations League, by the fields that no re-dating changes (the weekday
 * code +0, the month +2, the year offset +0xb, the key id +0x10), and its continent. 0x5cbb00 dates the rows again
 * on 1 January for the next years of the cycle (0x5cbdd0) and keeps +0xc, and a load builds the table anew with
 * the continents of 0x5ca4f0. So each snap first gives the kept rows their continents back, and a game played on
 * and a game loaded meet the same rule: the North American row of 7 September (offset 1) kept in 2023 pairs nations
 * again in 2027. Host memory only: a load starts with none (season_friendly_dates_reset). */
#define HELD_MAX 16
static struct { uint8_t b0, month, offset; uint32_t key, cont; } held[HELD_MAX];

static int same_row(uint32_t row, int i)
{
    return g_mem[row] == held[i].b0 && g_mem[row + 2] == held[i].month && g_mem[row + 0xb] == held[i].offset &&
           rd32(g_mem, row + 0x10) == held[i].key && rd32(g_mem, row + 0xc) == NO_CONTINENT;
}

static void give_back(uint32_t rows, int count)
{
    for (int i = 0; i < count; i++) {
        uint32_t row = rows + ROW_SIZE * (uint32_t)i;
        for (int k = 0; k < held_count; k++)
            if (same_row(row, k)) {
                wr32(g_mem, row + 0xc, held[k].cont);
                held[k] = held[--held_count];
                break;
            }
    }
    held_count = 0;
}

static void snap_rows(uint32_t self)
{
    uint32_t rows = rd32(g_mem, self + ROWS);
    int count = (int32_t)rd32(g_mem, self + ROW_COUNT), moved = 0, kept = 0, dropped = 0, kept_free = 0;
    if (!readable(rows) || count <= 0 || count > 400) return;
    give_back(rows, count);
    Cpu saved = cpu;
    for (int i = 0; i < count && !rt_unwinding; i++) {
        uint32_t row = rows + ROW_SIZE * (uint32_t)i;
        int month = g_mem[row + 2] + 1, year = (int16_t)rd16(g_mem, row + 3 + 2);
        if (month < 1 || month > 12) continue;
        int day = fifa_day(year, month, g_mem[row + 1]);
        if (!day) {
            wr32(g_mem, row + 0xc, NO_CONTINENT);
            dropped++;
            continue;
        }
        /* Item 215: a day that a Nations League keeps for a league phase drawn after the day its friendlies are
         * arranged (season_nl_late_window). The arranger's guard reads only the stored fixtures, so before the
         * draw it paired the nations, and the matchday met the friendlies: in 2023 the North American row of
         * 7 September (offset 1, on the FIFA day 5 September) and the first matchday of CONCACAF 2023-24
         * (7 September; the game draws on 19 June, after the 2022-23 final). The confederation keeps those
         * windows, so the row pairs no one. Changing rules on only: the table's editions are those years'. */
        const char *confed = season_changing_rules() ? nl_confed(rd32(g_mem, row + 0xc)) : NULL;
        if (confed && season_nl_late_window(confed, year, month, day) && held_count < HELD_MAX) {
            held[held_count].b0 = g_mem[row], held[held_count].month = g_mem[row + 2];
            held[held_count].offset = g_mem[row + 0xb], held[held_count].key = rd32(g_mem, row + 0x10);
            held[held_count++].cont = rd32(g_mem, row + 0xc);
            wr32(g_mem, row + 0xc, NO_CONTINENT);
            kept_free++;
            if (getenv("RECOMP_SCREEN_LOG"))
                fprintf(stderr, "recomp: friendlies: the %s row of %d.%d.%d pairs no one (a %s Nations League "
                                "matchday drawn later)\n", confed, day, month, year, confed);
        }
        if (day == g_mem[row + 1] && (int16_t)rd16(g_mem, row + 3) == day_of_year(year, month, day)) {
            kept++;
            continue;
        }
        g_mem[row + 1] = (uint8_t)day;
        cpu.ecx = row + 3;
        uint32_t a[4] = {(uint32_t)day, (uint32_t)(month - 1), (uint32_t)year, 0xffffffffu};
        rt_callback(F_DATE, 4, a);
        cpu = saved;
        moved++;
    }
    if (rt_unwinding) return;
    uint32_t q[4] = {rows, (uint32_t)count, ROW_SIZE, F_ROW_ORDER};
    rt_callback(F_QSORT, 4, q);
    if (rt_unwinding) return;
    cpu = saved;
    if (getenv("RECOMP_SCREEN_LOG"))
        fprintf(stderr, "recomp: friendlies: %d rows on the real FIFA day of their month, %d already on it, %d not "
                        "used (no FIFA day that month), %d kept for a Nations League\n", moved, kept, dropped, kept_free);
}

/* ---- The qualifying matches ---- */

/* The real FIFA match days of year y, as days of the year (after the list: its last year moved on, as fifa_day). */
static int fifa_doys(int y, int *out, int max)
{
    static int cached_year = -1, cached_n, cached[FIFA_DOYS_MAX];
    if (y != cached_year) {
        cached_n = 0;
        for (int m = 1; m <= 12; m++)
            for (int want = 1; want <= 31; want++) {          /* every day: a window month holds up to 12 */
                int d = fifa_day(y, m, want);
                if (!d) break;
                int doy = day_of_year(y, m, d), seen = 0;
                for (int k = 0; k < cached_n; k++) seen |= cached[k] == doy;
                if (!seen && cached_n < FIFA_DOYS_MAX) cached[cached_n++] = doy;
            }
        cached_year = y;
    }
    int n = cached_n < max ? cached_n : max;
    for (int k = 0; k < n; k++) out[k] = cached[k];
    return n;
}

/* The store's own list of the fixtures of day doy of year y, slice 0 to 2, fixture type (F_DAY_LIST, not a copy).
 * The Date lies on the game stack for the call. */
static uint32_t day_list(int y, int doy, int slice, int type)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t date = cpu.esp;
    wr16(g_mem, date, (uint16_t)doy);
    wr16(g_mem, date + 2, (uint16_t)y);
    wr32(g_mem, date + 4, 0);
    Cpu saved = cpu;
    cpu.ecx = rd32(g_mem, FIXTURES);
    uint32_t args[3] = {date, (uint32_t)slice, (uint32_t)type};
    uint32_t list = rt_callback(F_DAY_LIST, 3, args);
    if (rt_unwinding) return 0;
    cpu = saved;
    cpu.esp = saved_esp;
    return list;
}

static int national_side(uint32_t club)
{
    uint32_t clubs = rd32(g_mem, CLUBS);
    int count = (int32_t)rd32(g_mem, CLUB_COUNT), nations = (int32_t)rd32(g_mem, NATION_COUNT);
    if (!readable(club) || !clubs || club < clubs) return 0;
    int i = (int)((club - clubs) / CLUB_SIZE);
    return i < count && i >= count - 2 * nations;
}

/* 1 when day doy of year y is taken for a qualifier of club a v club b: a side of the nation of a or b (senior, B
 * or Under 21: club +0x53, as the friendly arranger reads it at 0x5cbf59) has a national match that day, or the day
 * holds a continental club match. A continental club competition has no nation (record +0x5d, as
 * comp_active_key_nation_of_fixture 0x48dd70 reads it) and two club sides; the friendlies (priority 1) aside. A
 * qualifier (fixture priority +0x3a: 10) does not move such a tie (Champions League 8, Europa League 7) when
 * add_fixture 0x5aa680 stores it, and the tie then loses its internationals (Liverpool fielded 7 at Bordeaux on
 * 6 September 2008, match_eng.cpp 652). The day lists of all types (count: the s8 at 0xb64738, key nation entries).
 * self: the fixture that moves, when it is stored on that day (it is not a match beside itself).
 * The club match holds the day only when a club of it has a player of the nation of a or b (players_of): the
 * call-up takes only those players. Up to 0.6.2 item 210 any continental club match of any confederation held
 * it, and CONCACAF's qualifiers of day 85 of 2033 gave way to Al-Ahli SC v Khor Fakkan in the AFC Champions League
 * Elite (i163 replay of run set game a2021s1). */
static int players_of(uint32_t c, uint32_t n, uint32_t m);
static char busy_why[160];              /* what side_busy found last (for the stay line) */
static char duty_first[160];            /* the first such tie (for the stay line) */

/* ---- A qualifier outranks a friendly (0.6.2 item 208) ----
 *
 * The snap placed the game's friendlies first (snap_rows above: each row on the FIFA day of its month), so a
 * friendly could hold the FIFA day of a qualifier made later: in a replay of 2031 to 2033 the 18 pairs of a CAF round
 * planned for 4 June 2032 and the 5 of a CONMEBOL round planned for 24 September 2031 went to days that are not FIFA
 * days, each side having a game friendly on the planned day (run-logs/0.6.2-s203/probe-lines.txt). The original ranks
 * a friendly below a competition match: when add_fixture 0x5aa680 stores a match of higher priority (+0x3a) than its
 * clash, each clashing fixture of the Friendly competition (Friendly::is_friendly_comp 0x5c6710, at 0x5aa9be) is
 * cancelled with Friendly::cancel 0x5c6570 (at 0x5aa9d0, this the Friendly object [0xae2c74]), which posts the friendly's cancelled news and removes it
 * (0x5cb9e0 for national sides, remove_fixture 0x5abc30 for clubs). Real life: a friendly is arranged around the
 * competitive matches of its window. So in the snap a friendly does not hold a day (side_busy, side_near), and when a
 * qualifier's day is chosen, each friendly of a side of its two nations (senior, B or Under 21) from CLASH_DAYS before
 * to CLASH_DAYS after that day, not yet played (+0x43 -1), is cancelled with 0x5c6570 before add_fixture stores the
 * qualifier. Running seasons with changing rules only, as the snap. */
#define FRIENDLY_COMP 0x04u                 /* Friendly +4: the friendly competition record (0x5c6710) */
#define COMP_COUNT 0xae2378u                /* the competition count: an id from it - 0x7f on is a friendly (0x5c6710) */
#define F_CANCEL_FRIENDLY 0x005c6570u       /* Friendly::cancel, thiscall (Fixture *, Club *) ret 8 */
#define FRIENDLY_OBJ 0xae2c74u              /* the Friendly object (this of 0x5c6710 and 0x5c6570 at 0x5aa9b7) */

/* 1 when fixture f is of the Friendly competition: Friendly::is_friendly_comp 0x5c6710 (its record is Friendly +4, or
 * its id is the competition count - 0x7f or more). */
static int friendly_fixture(uint32_t f)
{
    uint32_t rec = rd32(g_mem, f + 0x14);
    if (!readable(rec)) return 0;
    uint32_t fo = rd32(g_mem, FRIENDLY_OBJ);
    if (readable(fo) && rec == rd32(g_mem, fo + FRIENDLY_COMP)) return 1;
    return (int32_t)rd32(g_mem, rec) >= (int32_t)rd32(g_mem, COMP_COUNT) - 0x7f;
}

/* 1 when the fixture of club h v club w takes a national side of club a or club b: one of them is a or b, or a
 * national side (senior, B or Under 21, national_side) of the nation of a or b (club +0x53). The old test compared
 * the nation of every club, but a league club has its nation at +0x53 too (VIRTUAL_STAFF top-up reads it at
 * 0x90db33), so every league match of that nation took the day (open item 168). A league match does not stop a
 * national match: the call-up move moves the league and cup fixtures of a club with more than 2 players called up
 * (0x7777e0 calls 0x5abb10 at 0x77795c for marks 1 and 2, 0x7778b7..0x7778bd, the count at 0x77793d); with 2 or
 * fewer away the club plays without them. */
int season_fifa_fixture_takes_side(uint32_t h, uint32_t w, uint32_t a, uint32_t b)
{
    if (h == a || h == b || w == a || w == b) return 1;
    uint32_t na = rd32(g_mem, a + 0x53), nb = rd32(g_mem, b + 0x53);
    uint32_t nh = national_side(h) ? rd32(g_mem, h + 0x53) : 0, nw = national_side(w) ? rd32(g_mem, w + 0x53) : 0;
    return (nh && (nh == na || nh == nb)) || (nw && (nw == na || nw == nb));
}

static int side_busy(uint32_t a, uint32_t b, int y, int doy, uint32_t self)
{
    int busy = 0, types = (int8_t)g_mem[TYPE_COUNT];
    uint32_t na = readable(a) ? rd32(g_mem, a + 0x53) : 0, nb = readable(b) ? rd32(g_mem, b + 0x53) : 0;
    if (types < 2 || types > 40) types = 2;
    for (int slice = 0; slice < 3 && !busy && !rt_unwinding; slice++)
        for (int type = 0; type < types && !busy && !rt_unwinding; type++) {
            uint32_t list = day_list(y, doy, slice, type);
            if (rt_unwinding) return 1;
            for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                 node = rd32(g_mem, node + 6), k++) {
                uint32_t f = rd32(g_mem, node);
                if (!readable(f) || f == self || friendly_fixture(f)) continue;     /* item 208 */
                uint32_t h = rd32(g_mem, f + 0x1c), w = rd32(g_mem, f + 0x20);
                uint32_t rec = rd32(g_mem, f + 0x14);
                int nat = season_fifa_fixture_takes_side(h, w, a, b);
                int cont = !nat && readable(rec) && !rd32(g_mem, rec + 0x5d) && g_mem[f + 0x3a] >= 2 &&
                           !national_side(h) && !national_side(w) &&     /* a continental club match */
                           ((!readable(h) && !readable(w)) ||                 /* no clubs yet: it holds */
                            (readable(h) && players_of(h, na, nb)) || (readable(w) && players_of(w, na, nb)));
                if (nat || cont) {
                    busy = 1;
                    int pl = nat ? 0 : (readable(h) ? players_of(h, na, nb) : 0) +
                                       (readable(w) ? players_of(w, na, nb) : 0);
                    int len = snprintf(busy_why, sizeof busy_why, "%s %.30s v %.30s (%.40s)",
                                       nat ? "national" : "club", readable(h) ? win_str(h + 4) : "-",
                                       readable(w) ? win_str(w + 4) : "-", readable(rec) ? win_str(rec + 4) : "?");
                    if (!nat && len > 0 && len < (int)sizeof busy_why)
                        snprintf(busy_why + len, sizeof busy_why - (size_t)len, ", %d players of the two nations", pl);
                }
            }
        }
    return busy;
}

/* 1 when club a or club b has a match within CLASH_DAYS days of day doy of year y (not on doy itself), other than
 * the fixture self (route M moves a stored fixture, which still lies on its old day). The store treats a second
 * match of a club close to a stored one as a clash: add_fixture 0x5aa680 calls find_fixture_clashes 0x5ad780 with
 * its flag 0 (0x5aa820, 0x5aa894), which takes the clubs' fixtures on the day itself, the day after and, below
 * priority 0x17, the day before (collect_day_clashes 0x5adba0 at 0x5adb11, 0x5adaf6, 0x5adad5; 0x5ada79..0x5ada8d),
 * and then moves the weaker match (resolve_same_day_clash 0x5ae380, called at 0x5aa8c5) to a weekday off the FIFA
 * days. CLASH_DAYS 2 is wider than the store's day, so the two matchdays of a window lie 3 days apart or more, as the
 * real ones do (Thursday and Sunday, Saturday and Tuesday). The snap moved the two
 * qualifiers of a window on their own to the nearest FIFA day: the 2030 UEFA qualifying took 15 November 2029 for
 * matchday 9 (13 November) and kept 16 November for matchday 10, a FIFA day; the store moved matchday 10 to
 * 21 November, the day before the Europa League's Thursday, and West Bromwich Albion fielded 9 there, its
 * internationals on duty (get_international_duty_club 0x7757d0 holds a player from the day before his side's match
 * to the day after, 0x77583d..0x775850; box match_eng.cpp 612; open item 93, run set game a2013s3). */
#define CLASH_DAYS 2

static int side_near(uint32_t a, uint32_t b, int y, int doy, uint32_t self)
{
    int types = (int8_t)g_mem[TYPE_COUNT];
    if (types < 2 || types > 40) types = 2;
    for (int off = -CLASH_DAYS; off <= CLASH_DAYS && !rt_unwinding; off++) {
        int yy = y, dd = doy + off;
        if (!off) continue;
        if (dd < 0) dd += days_in(--yy);
        else if (dd >= days_in(yy)) dd -= days_in(yy++);
        for (int slice = 0; slice < 3 && !rt_unwinding; slice++)
            for (int type = 0; type < types && !rt_unwinding; type++) {
                uint32_t list = day_list(yy, dd, slice, type);
                if (rt_unwinding) return 1;
                for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                     node = rd32(g_mem, node + 6), k++) {
                    uint32_t f = rd32(g_mem, node);
                    if (!readable(f) || f == self || friendly_fixture(f)) continue;     /* item 208 */
                    uint32_t h = rd32(g_mem, f + 0x1c), w = rd32(g_mem, f + 0x20);
                    if (h == a || h == b || w == a || w == b) return 1;
                }
            }
    }
    return 0;
}

/* 1 when club `team` has a fixture of any kind from `days` days before day doy of year y to `days` days after it
 * (the day lists of the store, as side_near). The AFC first round (season_afc_first.c) takes a FIFA window where
 * none of its teams has another match: the store would move one of the two (resolve_same_day_clash 0x5ae380), and
 * in the 2006 cycle Bhutan played its second leg on 13 October 2004 and the Asian Cup qualifying at Indonesia on the
 * 14th. */
int season_fifa_team_busy_near(uint32_t team, int y, int doy, int days)
{
    int types = (int8_t)g_mem[TYPE_COUNT];
    if (types < 2 || types > 40) types = 2;
    for (int off = -days; off <= days && !rt_unwinding; off++) {
        int yy = y, dd = doy + off;
        if (dd < 0) dd += days_in(--yy);
        else if (dd >= days_in(yy)) dd -= days_in(yy++);
        for (int slice = 0; slice < 3 && !rt_unwinding; slice++)
            for (int type = 0; type < types && !rt_unwinding; type++) {
                uint32_t list = day_list(yy, dd, slice, type);
                if (rt_unwinding) return 1;
                for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                     node = rd32(g_mem, node + 6), k++) {
                    uint32_t fx = rd32(g_mem, node);
                    if (readable(fx) && (rd32(g_mem, fx + 0x1c) == team || rd32(g_mem, fx + 0x20) == team)) return 1;
                }
            }
    }
    return 0;
}

/* The last match day of the qualifying stage whose fixtures are being made (League::create_fixtures 0x6835c0, see
 * season_fifa_stage_begin), or none (end_year 0). A qualifier does not move past it: the snap's search of the whole
 * year moved the AFC first round of the 2006 cycle from May 2005 (days 121 to 146) to the FIFA days of October and
 * November (280, 315, 319), past the start of its final round (day 230), so the final round was made on day 319 with
 * its days gone, the AFC fifth's tie with CONCACAF's fourth ended after the World Cup draw, and the World Cup nets of
 * rows A8 to A11 acted (gate (a) run 8 on 8e58c44a, 2003 and 2007 starts; gate (b) 2002 and 2006 starts). */
static int end_year, end_day;

/* 1 when day doy of year y lies in the days of a final of the confederation of club a or club b (0.6.2 slice 6,
 * season_intl_finals_busy). The qualifiers are made long before the finals' fixtures, so side_busy cannot see
 * them: the CAF qualifiers of 26 January 2013 (the original's day; the nearest FIFA day, 6 February, too) lie in
 * the ACN of 2013, whose finalists then lost their group days and the final. */
static int finals_busy(uint32_t a, uint32_t b, int y, int doy)
{
    uint32_t sides[2] = {a, b};
    for (int s = 0; s < 2; s++) {
        uint32_t nation = readable(sides[s]) ? rd32(g_mem, sides[s] + 0x53) : 0;
        uint32_t cont = readable(nation) ? rd32(g_mem, nation + NATION_CONTINENT) : 0;
        if (!readable(cont)) continue;
        for (int c = 0; c < SEASON_CONFS; c++)
            if (rd32(g_mem, cont) == rd32(g_mem, KEY_CONTINENTS + 4u * (uint32_t)c) &&
                (season_intl_finals_busy(c, y, doy) ||
                 (c == SEASON_CONF_NORTH_AMERICA && season_gold_cup_busy(y, doy))))   /* open item 55 */
                return 1;
    }
    return 0;
}

/* 1 when every day from e + 1 to c is a FIFA day (doys): c lies in the window that holds day e, its stage's last
 * match day, or starts the day after it. The stage bound alone sent qualifiers of a stage's last window back to an
 * earlier window: matchday 10 of the 2030 UEFA groups (16 November 2029, the stage's last day) could not take 18 to
 * 20 November when side_near refused 16 and 17 (matchday 9 on 15 November), and in a 2098 date jump matchday 9 of
 * the 2102 qualifying (12 November 2101) could not take the window of 17 November, the day after its stage's last
 * day; both went back to June, before matchday 5. */
static int window_from(const int *doys, int n, int d, int c)
{
    for (int day = d + 1; day <= c; day++) {
        int in = 0;
        for (int k = 0; k < n; k++) in |= doys[k] == day;
        if (!in) return 0;
    }
    return 1;
}

static int duty_ties(uint32_t n, uint32_t m, int y, int d);

/* 1 when club a and club b are sides of South American nations (nation +0x71, the continent's id at +0, against the
 * key continent id of confederation SEASON_CONF_SOUTH_AMERICA, as finals_busy reads them). */
static int south_american(uint32_t a, uint32_t b)
{
    uint32_t sides[2] = {a, b}, want = rd32(g_mem, KEY_CONTINENTS + 4u * SEASON_CONF_SOUTH_AMERICA);
    for (int s = 0; s < 2; s++) {
        uint32_t nation = readable(sides[s]) ? rd32(g_mem, sides[s] + 0x53) : 0;
        uint32_t cont = readable(nation) ? rd32(g_mem, nation + NATION_CONTINENT) : 0;
        if (!readable(cont) || rd32(g_mem, cont) != want) return 0;
    }
    return 1;
}

/* The day a qualifier of club home v club away on day d of year y moves to: the nearest free FIFA day of the same
 * year, 2 or more days after today, with no other match of the two sides within CLASH_DAYS of it (side_near; self:
 * the fixture that moves, when it is stored) and no continental club tie within a day of it whose club holds more
 * than two of the nations' players (duty_ties: the duty of 0x7757d0 reaches the day before and after), not after
 * day ey/ed (its stage's last match day; ey 0: none) unless in the FIFA window of that day (window_from), and not
 * in the days of a final of either side's confederation; d when it is on such a day already, free on the day itself
 * too (side_busy: open item 79, a matchday that the snap of the matchday before had moved onto its FIFA day stayed
 * there, and the store moved it off the window, to the day before a Europa League Thursday). With no such FIFA
 * day, and d in a final's days: the nearest such day that is not a FIFA day (open item 55). -1 when none is free. */
static int fifa_target_by(uint32_t home, uint32_t away, int y, int d, int ey, int ed, uint32_t self)
{
    int doys[FIFA_DOYS_MAX], n = fifa_doys(y, doys, FIFA_DOYS_MAX);
    uint32_t na = rd32(g_mem, home + 0x53), nb = rd32(g_mem, away + 0x53);
    /* On a free FIFA day already, or on a real South American matchday that the FIFA list lacks (open item 163:
     * 9 June 2012 in Euro 2012, season_conmebol_real_day). */
    int real = south_american(home, away) && season_conmebol_real_day(season_abs_day(y, 1, 1) + d);
    for (int k = 0; k <= n; k++)
        if ((k < n ? doys[k] == d : real) && !finals_busy(home, away, y, d) &&
            !side_busy(home, away, y, d, self) && !side_near(home, away, y, d, self) && !duty_ties(na, nb, y, d))
            return rt_unwinding ? -1 : d;
    int today_year = (int16_t)rd16(g_mem, GAME_DATE + 2), today = (int16_t)rd16(g_mem, GAME_DATE);
    for (int dist = 1; dist <= SNAP_DAYS && !rt_unwinding; dist++)
        for (int sign = -1; sign <= 1; sign += 2) {
            int c = d + sign * dist, ok = 0;
            if (c < 0 || c >= days_in(y) || (y == today_year && c < today + 2) || y < today_year) continue;
            /* past its stage's last day: only within the FIFA window that holds that day or starts the day after */
            if (ey && (y > ey || (y == ey && c > ed)) && c > d && !(y == ey && window_from(doys, n, ed, c))) continue;
            for (int k = 0; k < n; k++) ok |= doys[k] == c;
            if (!ok || finals_busy(home, away, y, c) || side_busy(home, away, y, c, self) ||
                side_near(home, away, y, c, self) || duty_ties(na, nb, y, c))
                continue;
            return rt_unwinding ? -1 : c;
        }
    /* Open item 55: no free FIFA day, and its own day lies in a final's days. It then takes the nearest day that is
     * not a FIFA day, with the same bounds, where neither side has a national match on that day or the day either
     * side of it, rather than meet the final. Not seen in a game: in long game seed 6 to 2030 no qualifier kept a
     * day in a final's days (run-logs/0.6.2-s6). Inference: a domestic league day can be taken, which leaves the
     * clubs without their internationals that day; the final losing its last fixture is worse. */
    if (rt_unwinding || !finals_busy(home, away, y, d)) return -1;
    for (int dist = 1; dist <= SNAP_DAYS && !rt_unwinding; dist++)
        for (int sign = -1; sign <= 1; sign += 2) {
            int c = d + sign * dist;
            if (c < 0 || c >= days_in(y) || (y == today_year && c < today + 2) || y < today_year) continue;
            if (ey && (y > ey || (y == ey && c > ed)) && c > d) continue;
            if (finals_busy(home, away, y, c) || side_busy(home, away, y, c, self) ||
                (c > 0 && side_busy(home, away, y, c - 1, self)) ||
                (c + 1 < days_in(y) && side_busy(home, away, y, c + 1, self)))
                continue;
            return rt_unwinding ? -1 : c;
        }
    return -1;
}

/* ---- The plan of the stage (items 203 and 204) ----
 *
 * fifa_target_by moves each qualifier on its own, as add_fixture 0x5aa680 stores it, so the rounds of a stage lost
 * their order: in the AFC third round of the 2034 cycle (a2021s1 replay, 2033) matchdays 5 and 6 went back to June,
 * before matchday 1 (item 203), and matchdays 7 and 8 kept days 1 or 2 days from matchdays 3 and 4 of the same sides,
 * so add_fixture's clash move (0x5aaa25) moved them 3 to 5 days (item 204). Real life: a stage's rounds play in order
 * on the FIFA windows (the AFC third round of the 2026 cycle: 10 matchdays in the windows of September 2024 to June
 * 2025, two a window; en.wikipedia.org, "2026 FIFA World Cup qualification – AFC third round").
 *
 * So the stage's days are planned once, from its round table (season_fifa_stage_begin: League::create_fixtures
 * 0x6835c0 dates each fixture from its round record, 0x6839f8..0x683a10 and 0x683d97..0x683db6), at the first fixture
 * the snap sees (the plan needs a pair: its confederation gives the South American real days and the finals' days).
 * The pure rule is season_stage_plan.c: rounds in order, at least SEASON_STAGE_GAP (CLASH_DAYS + 1) days apart, each a
 * FIFA day from today + 2 to the stage bound (window_from), else its own day, else another day between its neighbours;
 * the most rounds on FIFA days, then the least total move.
 *
 * The rule per fixture (planned_target): its round's planned day when the pair's checks allow it (finals_busy,
 * side_busy, side_near, duty_ties, as fifa_target_by); else the nearest FIFA day with those checks between the
 * round's share of the days between its neighbours' planned days (season_stage_window: each gap split in two halves
 * SEASON_STAGE_GAP apart), so it cannot pass, or come within the gap of, a fixture of another round; else the
 * nearest other day there with the same checks (no clash for add_fixture to move, item 204); else the planned day.
 * A fixture whose day is no round's (not from the round table) takes fifa_target_by, as before. */
#define NO_PLAN (-2)
static int stage_n, stage_state;            /* rounds; state 0 not planned yet, 1 planned, -1 no plan */
static long stage_own[SEASON_STAGE_MAX], stage_plan[SEASON_STAGE_MAX], stage_first, stage_last, stage_last_fifa;

typedef struct {
    uint32_t home, away;
    int sa;                                 /* a South American pair: its real matchdays count as FIFA days */
} PlanPair;

/* 1 when day z is a FIFA day (or a real South American matchday of a South American pair) of its year. */
static int fifa_or_real(long z, int sa)
{
    int y, m, dd, doys[FIFA_DOYS_MAX], n;
    season_from_abs_day(z, &y, &m, &dd);
    n = fifa_doys(y, doys, FIFA_DOYS_MAX);
    int doy = day_of_year(y, m, dd);
    for (int k = 0; k < n; k++)
        if (doys[k] == doy) return 1;
    return sa && season_conmebol_real_day(z);
}

static int plan_day_ok(long z, void *ctx)
{
    const PlanPair *p = ctx;
    if (rt_unwinding || !fifa_or_real(z, p->sa)) return 0;
    int y, m, dd;
    season_from_abs_day(z, &y, &m, &dd);
    return !finals_busy(p->home, p->away, y, day_of_year(y, m, dd));
}

static void make_plan(uint32_t rec, uint32_t home, uint32_t away)
{
    stage_state = -1;
    if (!stage_n || !end_year) return;
    int ty = (int16_t)rd16(g_mem, GAME_DATE + 2), td = (int16_t)rd16(g_mem, GAME_DATE);
    stage_first = season_abs_day(ty, 1, 1) + td + 2;
    stage_last = season_abs_day(end_year, 1, 1) + end_day;
    /* window_from: the FIFA window that holds the stage's last day or starts the day after it, in its year */
    int doys[FIFA_DOYS_MAX], n = fifa_doys(end_year, doys, FIFA_DOYS_MAX), e = end_day;
    while (e + 1 < days_in(end_year) && window_from(doys, n, end_day, e + 1)) e++;
    stage_last_fifa = season_abs_day(end_year, 1, 1) + e;
    PlanPair p = {home, away, south_american(home, away)};
    int on = season_stage_plan(stage_own, stage_n, stage_first, stage_last, stage_last_fifa, plan_day_ok, &p,
                               stage_plan);
    if (rt_unwinding) return;
    stage_state = on < 0 ? -1 : 1;
    if (!getenv("RECOMP_SCREEN_LOG")) return;
    char list[1200] = "";
    size_t at = 0;
    for (int r = 0; r < stage_n && at < sizeof list - 1; r++) {
        int y, m, dd;
        season_from_abs_day(stage_plan[r], &y, &m, &dd);
        at += (size_t)snprintf(list + at, sizeof list - at, "%s%ld>%d", r ? " " : "",
                               stage_own[r] - season_abs_day(y, 1, 1), day_of_year(y, m, dd));
    }
    fprintf(stderr, "recomp: intl dates: stage plan of %.40s ending day %d of %d: %d rounds, %d on FIFA days%s: %s\n",
            win_str(rec + 4), end_day, end_year, stage_n, on < 0 ? 0 : on, on < 0 ? " (no plan: rounds keep the "
            "snap of each fixture)" : "", list);
}

/* 1 when the pair can play day c of year y: no final of their confederation, no national match of their nations
 * that day, no match of either side within CLASH_DAYS, and no continental club tie within a day with a club of more
 * than two of their players (the checks of fifa_target_by). */
static int pair_free(uint32_t home, uint32_t away, int y, int c, uint32_t self)
{
    uint32_t na = rd32(g_mem, home + 0x53), nb = rd32(g_mem, away + 0x53);
    return !finals_busy(home, away, y, c) && !side_busy(home, away, y, c, self) && !side_near(home, away, y, c, self) &&
           !duty_ties(na, nb, y, c) && !rt_unwinding;
}

/* The move text of a qualifier whose planned day is held, with what holds it (test log, RECOMP_SCREEN_LOG): the
 * first check of pair_free that refuses the day, and for side_busy and duty_ties the match. */
static char held_text[240] = " (stage plan: its planned day was not free)";

static void held_why(uint32_t home, uint32_t away, int y, int d)
{
    uint32_t na = rd32(g_mem, home + 0x53), nb = rd32(g_mem, away + 0x53);
    busy_why[0] = duty_first[0] = 0;
    const char *why = finals_busy(home, away, y, d) ? "a final"
                      : side_busy(home, away, y, d, 0) ? busy_why
                      : side_near(home, away, y, d, 0) ? "a match of a side within 2 days"
                      : duty_ties(na, nb, y, d) ? duty_first + (duty_first[0] == ':' ? 2 : 0) : "?";
    snprintf(held_text, sizeof held_text, " (stage plan: its planned day was not free: %s)", why);
}

/* The day of a qualifier of the planned stage on day d of year y (the rule above), NO_PLAN when the stage has no plan
 * or d is no round's day. *how: the text of the move line. */
static int planned_target(uint32_t rec, uint32_t home, uint32_t away, int y, int d, const char **how)
{
    if (!stage_n || stage_state < 0) return NO_PLAN;
    if (!stage_state) make_plan(rec, home, away);
    if (rt_unwinding) return -1;
    if (stage_state != 1) return NO_PLAN;
    long y0 = season_abs_day(y, 1, 1), z = y0 + d;
    int r = 0;
    while (r < stage_n && stage_own[r] != z) r++;
    if (r == stage_n) return NO_PLAN;
    long p = stage_plan[r], lo, hi;
    if (p < y0 || p >= y0 + days_in(y)) return NO_PLAN;          /* not in its year: cannot happen (the plan) */
    *how = " (stage plan)";
    if (pair_free(home, away, y, (int)(p - y0), 0)) return rt_unwinding ? -1 : (int)(p - y0);
    if (rt_unwinding) return -1;
    if (getenv("RECOMP_SCREEN_LOG")) {                 /* test log: what holds day p */
        held_why(home, away, y, (int)(p - y0));
        if (rt_unwinding) return -1;
    }
    season_stage_window(stage_plan, stage_n, r, stage_first, stage_last_fifa, &lo, &hi);
    if (lo < y0) lo = y0;
    if (hi > y0 + days_in(y) - 1) hi = y0 + days_in(y) - 1;
    int sa = south_american(home, away);
    for (int pass = 0; pass < 2; pass++) {                       /* FIFA days, then any other day */
        long top = pass && hi > stage_last ? stage_last : hi;  /* no other day after the stage's last */
        long reach = p - lo > top - p ? p - lo : top - p;
        for (long dist = 1; dist <= reach && !rt_unwinding; dist++)
            for (int sign = -1; sign <= 1; sign += 2) {
                long c = p + sign * dist;
                if (c < lo || c > top || fifa_or_real(c, sa) == pass) continue;
                if (pair_free(home, away, y, (int)(c - y0), 0)) {
                    *how = held_text;
                    return rt_unwinding ? -1 : (int)(c - y0);
                }
            }
    }
    if (rt_unwinding) return -1;
    *how = " (stage plan: no free day between its neighbours)";
    return (int)(p - y0);
}

static int fifa_target(uint32_t home, uint32_t away, int y, int d)
{
    return fifa_target_by(home, away, y, d, end_year, end_day, 0);
}

/* Players of nation n or m in the squad of club c (staff +0x1a or the second nationality +0x1e; +0x61 a player row). */
static int players_of(uint32_t c, uint32_t n, uint32_t m)
{
    int k = 0;
    for (uint32_t j = 0; j < 50; j++) {
        uint32_t sp = rd32(g_mem, c + 0xd7 + 4 * j);
        if (!readable(sp) || !readable(rd32(g_mem, sp + 0x61))) continue;
        uint32_t a = rd32(g_mem, sp + 0x1a), b = rd32(g_mem, sp + 0x1e);
        k += (a && (a == n || a == m)) || (b && (b == n || b == m));
    }
    return k;
}

/* The ties of continental club competitions within a day of day d of year y whose club holds more than two players
 * of nation n or m: the original's duty rule (NationalTeams::can_schedule_club_fixture 0x776fc0) meets those. */

static int duty_ties(uint32_t n, uint32_t m, int y, int d)
{
    duty_first[0] = 0;
    int count = 0, types = (int8_t)g_mem[TYPE_COUNT];
    if (types < 2 || types > 40) types = 2;
    for (int dd = d - 1; dd <= d + 1 && !rt_unwinding; dd++)
        for (int slice = 0; slice < 3 && !rt_unwinding; slice++)
            for (int type = 0; type < types && !rt_unwinding; type++) {
                uint32_t list = day_list(y, dd, slice, type);
                if (rt_unwinding) return count;
                for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                     node = rd32(g_mem, node + 6), k++) {
                    uint32_t f = rd32(g_mem, node), rec = readable(f) ? rd32(g_mem, f + 0x14) : 0;
                    if (!readable(rec) || rd32(g_mem, rec + 0x5d) || g_mem[f + 0x3a] < 2) continue;
                    uint32_t h = rd32(g_mem, f + 0x1c), w = rd32(g_mem, f + 0x20);
                    if (national_side(h) || national_side(w)) continue;
                    if ((readable(h) && players_of(h, n, m) > 2) || (readable(w) && players_of(w, n, m) > 2)) {
                        if (!count++)
                            snprintf(duty_first, sizeof duty_first, ": %.40s %.30s v %.30s on day %d", win_str(rec + 4),
                                     readable(h) ? win_str(h + 4) : "-", readable(w) ? win_str(w + 4) : "-", dd);
                    }
                }
            }
    return count;
}

/* The line of a qualifier that keeps its date (info): each FIFA day it could take and what holds it, and the
 * continental club ties of clubs with more than two of the nations' players within a day of the kept day. */
static void stay_info(uint32_t rec, uint32_t home, uint32_t away, int d, int y, const char *how)
{
    int doys[FIFA_DOYS_MAX], n = fifa_doys(y, doys, FIFA_DOYS_MAX), reach = 0;
    int today_year = (int16_t)rd16(g_mem, GAME_DATE + 2), today = (int16_t)rd16(g_mem, GAME_DATE);
    char held[600] = "";
    size_t at = 0;
    for (int k = 0; k < n && !rt_unwinding; k++) {
        int c = doys[k];
        if (c == d || (y == today_year && c < today + 2) || y < today_year) continue;
        if (end_year && (y > end_year || (y == end_year && c > end_day)) && c > d &&
            !(y == end_year && window_from(doys, n, end_day, c)))
            continue;
        reach++;
        busy_why[0] = 0;
        if (side_near(home, away, y, c, 0)) snprintf(busy_why, sizeof busy_why, "a match of a side within %d days",
                                                     CLASH_DAYS);
        else if (!side_busy(home, away, y, c, 0)) snprintf(busy_why, sizeof busy_why, "free");
        if (at < sizeof held - 1)
            at += (size_t)snprintf(held + at, sizeof held - at, "%sday %d: %s", reach > 1 ? "; " : "", c, busy_why);
    }
    int ties = rt_unwinding ? 0 : duty_ties(rd32(g_mem, home + 0x53), rd32(g_mem, away + 0x53), y, d);
    fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s keeps day %d of %d (info): %d FIFA days in reach of its "
                    "stage%s%s; %d continental club ties within a day of it have a club with more than two of the "
                    "nations' players%s%s\n", win_str(rec + 4), win_str(home + 4), win_str(away + 4), d, y, reach,
            reach ? ", " : "", held, ties, ties ? duty_first : "", how);
}

/* Item 208: cancel the friendlies of a side of the nations of club a and club b from CLASH_DAYS before to CLASH_DAYS
 * after day doy of year y (the rule above). self: a stored qualifier that moves (not a friendly). */
#define CANCEL_MAX 64

/* 1 when fixture f is still in the store's lists of day doy of year y. A cancel can remove more than its own
 * fixture: Friendly::cancel 0x5c6570 removes every match of a friendly tournament (0x5c8260) and 0x5cb9e0 a second
 * fixture of the pair (0x5acbe0), so a friendly found before may be gone (review of item 208). */
static int still_stored(uint32_t f, int y, int doy)
{
    int types = (int8_t)g_mem[TYPE_COUNT];
    if (types < 2 || types > 40) types = 2;
    for (int slice = 0; slice < 3 && !rt_unwinding; slice++)
        for (int type = 0; type < types && !rt_unwinding; type++) {
            uint32_t list = day_list(y, doy, slice, type);
            if (rt_unwinding) return 0;
            for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                 node = rd32(g_mem, node + 6), k++)
                if (rd32(g_mem, node) == f) return 1;
        }
    return 0;
}

static void cancel_friendlies(uint32_t a, uint32_t b, int y, int doy, uint32_t self)
{
    uint32_t fo = rd32(g_mem, FRIENDLY_OBJ);
    if (!readable(fo)) return;
    uint32_t found[CANCEL_MAX];
    int found_y[CANCEL_MAX], found_d[CANCEL_MAX];
    int n = 0, types = (int8_t)g_mem[TYPE_COUNT];
    if (types < 2 || types > 40) types = 2;
    long today = season_abs_day((int16_t)rd16(g_mem, GAME_DATE + 2), 1, 1) + (int16_t)rd16(g_mem, GAME_DATE);
    for (int off = -CLASH_DAYS; off <= CLASH_DAYS && !rt_unwinding; off++) {
        int yy = y, dd = doy + off;
        if (dd < 0) dd += days_in(--yy);
        else if (dd >= days_in(yy)) dd -= days_in(yy++);
        if (season_abs_day(yy, 1, 1) + dd <= today) continue;      /* not today's match, nor one played (review) */
        for (int slice = 0; slice < 3 && !rt_unwinding; slice++)
            for (int type = 0; type < types && !rt_unwinding; type++) {
                uint32_t list = day_list(yy, dd, slice, type);
                if (rt_unwinding) return;
                for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                     node = rd32(g_mem, node + 6), k++) {
                    uint32_t f = rd32(g_mem, node);
                    if (!readable(f) || f == self || !friendly_fixture(f) || (int8_t)g_mem[f + 0x43] != -1) continue;
                    if (!season_fifa_fixture_takes_side(rd32(g_mem, f + 0x1c), rd32(g_mem, f + 0x20), a, b)) continue;
                    int seen = 0;
                    for (int i = 0; i < n; i++) seen |= found[i] == f;
                    if (!seen && n < CANCEL_MAX) found_y[n] = yy, found_d[n] = dd, found[n++] = f;
                }
            }
    }
    Cpu saved = cpu;
    for (int i = 0; i < n && !rt_unwinding; i++) {
        int stored = still_stored(found[i], found_y[i], found_d[i]);
        if (rt_unwinding) return;
        cpu = saved;
        if (!stored) continue;                                  /* removed by an earlier cancel */
        uint32_t f = found[i], h = rd32(g_mem, f + 0x1c), w = rd32(g_mem, f + 0x20);
        int fy = (int16_t)rd16(g_mem, f + 0x28), fd = (int16_t)rd16(g_mem, f + 0x2a);
        if (getenv("RECOMP_SCREEN_LOG"))
            fprintf(stderr, "recomp: intl dates: the friendly %.30s v %.30s on day %d of %d is cancelled for the "
                            "qualifier of %.30s v %.30s on day %d of %d\n", readable(h) ? win_str(h + 4) : "-",
                    readable(w) ? win_str(w + 4) : "-", fd, fy, win_str(a + 4), win_str(b + 4), doy, y);
        cpu.ecx = fo;
        uint32_t args[2] = {f, 0};
        rt_callback(F_CANCEL_FRIENDLY, 2, args);
        if (rt_unwinding) return;
        cpu = saved;
    }
}

static void log_move(uint32_t rec, uint32_t home, uint32_t away, int d, int c, int y, const char *how)
{
    if (!getenv("RECOMP_SCREEN_LOG")) return;
    int doys[FIFA_DOYS_MAX], n = c >= 0 ? fifa_doys(y, doys, FIFA_DOYS_MAX) : 0, fifa = 0;
    for (int k = 0; k < n; k++) fifa |= doys[k] == c;
    if (c >= 0)
        fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s moves from day %d to the %s %d of %d%s\n",
                win_str(rec + 4), win_str(home + 4), win_str(away + 4), d, fifa ? "FIFA day" : *how ? "day (not a "
                "FIFA day)" : "day (not a FIFA day: none was free, and its day lies in a final's days)", c, y, how);
    else
        stay_info(rec, home, away, d, y, how);
}

/* ---- The qualifiers of later seasons made in the start season (route M, lead 4, 26 October 2026) ----
 *
 * In the start season of a 2001 start the snap moves nothing, so that season stays the base game's (the same-save
 * new-game set).
 * A qualifier it makes for a later season waits in this list (competition record id, the two clubs' indexes, its
 * date). On the first day of the first running season (season_friendly_dates_day) each one moves to its FIFA day
 * as the original moves a stored fixture (League::move_fixtures_for_tv 0x685640: a copy of the fixture,
 * FIXTURE_MANAGER::remove_fixture 0x5abc30, the new date, add_fixture 0x5aa680). The list is in the season part of
 * the save (season_change.c), so a start-season save loaded later moves them once, and a save made after the move
 * holds none. */
#define F_REMOVE 0x005abc30u                /* FIXTURE_MANAGER::remove_fixture, thiscall (Fixture *) ret 4 */
#define F_ADD 0x005aa680u                   /* FIXTURE_MANAGER::add_fixture, thiscall (Fixture *, 1) ret 8 */
#define FIXTURE_SIZE 0x4f
#define PENDING_MAX 4096

typedef struct {
    uint32_t rec_id, home, away;            /* home and away: club indexes */
    int16_t year, day;
    int16_t end_year, end_day;              /* its stage's last match day (end_year 0: none, a save before it) */
} Pending;
static Pending pending[PENDING_MAX];

static int club_index(uint32_t club) { return (int)((club - rd32(g_mem, CLUBS)) / CLUB_SIZE); }

/* The stored fixture of a pending entry: the lists of its day (slices 0 to 2, fixture types 0 and 1). */
static uint32_t find_pending(const Pending *p)
{
    uint32_t clubs = rd32(g_mem, CLUBS);
    for (int slice = 0; slice < 3 && !rt_unwinding; slice++)
        for (int type = 0; type < 2 && !rt_unwinding; type++) {
            uint32_t list = day_list(p->year, p->day, slice, type);
            if (rt_unwinding) return 0;
            for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                 node = rd32(g_mem, node + 6), k++) {
                uint32_t f = rd32(g_mem, node), rec = readable(f) ? rd32(g_mem, f + 0x14) : 0;
                if (readable(rec) && rd32(g_mem, rec) == p->rec_id &&
                    rd32(g_mem, f + 0x1c) == clubs + CLUB_SIZE * p->home &&
                    rd32(g_mem, f + 0x20) == clubs + CLUB_SIZE * p->away)
                    return f;
            }
        }
    return 0;
}

static void move_pending(void)
{
    int moved = 0, stayed = 0, gone = 0;
    for (int i = 0; i < pending_count && !rt_unwinding; i++) {
        uint32_t f = find_pending(&pending[i]);
        if (rt_unwinding) return;
        if (!f) {
            gone++;
            continue;
        }
        uint32_t home = rd32(g_mem, f + 0x1c), away = rd32(g_mem, f + 0x20), rec = rd32(g_mem, f + 0x14);
        int y = pending[i].year, d = pending[i].day,
            c = fifa_target_by(home, away, y, d, pending[i].end_year, pending[i].end_day, f);
        if (rt_unwinding) return;
        cancel_friendlies(home, away, y, c >= 0 ? c : d, f);                   /* item 208, also a kept day */
        if (rt_unwinding) return;
        f = find_pending(&pending[i]);                          /* a cancel may move the store's lists */
        if (rt_unwinding) return;
        if (!f) {
            gone++;
            continue;
        }
        if (c == d) continue;
        if (c < 0) {
            stayed++;
            int ey = end_year, ed = end_day;                                    /* its stage, for the stay line */
            end_year = pending[i].end_year, end_day = pending[i].end_day;
            log_move(rec, home, away, d, -1, y, " (made in the start season)");
            end_year = ey, end_day = ed;
            continue;
        }
        uint32_t saved_esp = cpu.esp;
        cpu.esp -= 0x60;
        uint32_t copy = cpu.esp;
        memcpy(g_mem + copy, g_mem + f, FIXTURE_SIZE);
        Cpu saved = cpu;
        cpu.ecx = rd32(g_mem, FIXTURES);
        uint32_t r[1] = {copy};
        rt_callback(F_REMOVE, 1, r);
        if (rt_unwinding) return;
        cpu = saved;
        wr16(g_mem, copy + 0x2a, (uint16_t)c);
        cpu.ecx = rd32(g_mem, FIXTURES);
        uint32_t a[2] = {copy, 1};
        rt_callback(F_ADD, 2, a);
        if (rt_unwinding) return;
        cpu = saved;
        cpu.esp = saved_esp;
        moved++;
        log_move(rec, home, away, d, c, y, " (made in the start season)");
    }
    if (getenv("RECOMP_SCREEN_LOG"))
        fprintf(stderr, "recomp: intl dates: %d qualifiers made in the start season: %d moved, %d stay, %d no longer "
                        "stored\n", pending_count, moved, stayed, gone);
    pending_count = 0;
}

/* The list in the season part: the count, then 20 bytes an entry (16 in a save before the stage bound: record id,
 * home, away, year, day; then the stage's last year and day). An empty list has no bytes, not even the count: the
 * save allocates the size of the part (season.c add_part), and in 0.6.0 the count of an empty list went 4 bytes
 * past that block (0.6.1 item 4). The reader takes no bytes as an empty list. */
#define ENTRY SEASON_FRIENDLY_ENTRY
uint32_t season_friendly_part_size(void) { return pending_count ? 4u + ENTRY * (uint32_t)pending_count : 0u; }

void season_friendly_part_write(uint8_t *out)
{
    if (!pending_count) return;
    uint32_t n = (uint32_t)pending_count;
    memcpy(out, &n, 4);
    for (int i = 0; i < pending_count; i++) {
        uint8_t *e = out + 4 + ENTRY * (uint32_t)i;
        memcpy(e, &pending[i].rec_id, 4);
        memcpy(e + 4, &pending[i].home, 4);
        memcpy(e + 8, &pending[i].away, 4);
        memcpy(e + 12, &pending[i].year, 2);
        memcpy(e + 14, &pending[i].day, 2);
        memcpy(e + 16, &pending[i].end_year, 2);
        memcpy(e + 18, &pending[i].end_day, 2);
    }
}

/* Reads the list from the season part (n bytes after its other fields; 0 in a save before row A18). */
int season_friendly_part_read(const uint8_t *in, uint32_t n)
{
    pending_count = 0;
    if (!n) return 1;
    uint32_t count;
    if (n < 4) return 0;
    memcpy(&count, in, 4);
    uint32_t size = count && (n - 4) / count >= ENTRY ? ENTRY : 16u;   /* 16: a save before the stage bound */
    if (count > PENDING_MAX || (n - 4) / size < count) return 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = in + 4 + size * i;
        memcpy(&pending[i].rec_id, e, 4);
        memcpy(&pending[i].home, e + 4, 4);
        memcpy(&pending[i].away, e + 8, 4);
        memcpy(&pending[i].year, e + 12, 2);
        memcpy(&pending[i].day, e + 14, 2);
        pending[i].end_year = pending[i].end_day = 0;
        if (size == ENTRY) {
            memcpy(&pending[i].end_year, e + 16, 2);
            memcpy(&pending[i].end_day, e + 18, 2);
        }
    }
    pending_count = (int)count;
    return 1;
}

/* ---- The two legs of a qualifying tie (item 207) ----
 *
 * Cup::create_tie_fixtures 0x51cb90 (wrapped in season_kind_cup_breaks.c) stores a two-leg tie's first leg by
 * add_fixture at 0x51cf58 and its second at 0x51d296. The first leg's day is the round record's (+3 day, +5 year
 * offset on the cup's year +0x40, 0x51ce6c..0x51ce80; tomorrow when that day has passed, 0x51cf13..0x51cf46), the
 * second leg's the gap +0x22 after it (0x51d13c..0x51d175). The round table is +0xa3, 0x68 bytes a round
 * (0x51cbd1..0x51cbdf), +0x21 the legs. In a running season of a season game, both sides national and the competition
 * not a finals tournament, the first leg plans both legs on the FIFA days of one window, in order, before the next
 * round's draw (season_tie_legs.c), with the pair's checks of fifa_target_by on each leg; the second leg takes its
 * planned day. */
#define TIE_FIRST_LEG 0x0051cf5du               /* return address of the first leg's add_fixture */
#define TIE_SECOND_LEG 0x0051d29bu              /* of the second leg's */
#define CUP_ROUNDS 0xa3u
#define CUP_ROUND_SIZE 0x68u
static uint32_t tie_cup;
static int tie_round;
static struct {
    uint32_t rec, home, away;
    int year, day;                              /* the second leg's planned day; year 0: none */
} tie_next;

void season_fifa_tie_begin(uint32_t cup, int round, uint32_t *saved_cup, int *saved_round)
{
    *saved_cup = tie_cup, *saved_round = tie_round;
    tie_cup = cup, tie_round = round;
    tie_next.year = 0;
}

void season_fifa_tie_end(uint32_t cup, int round)
{
    tie_cup = cup, tie_round = round;
    tie_next.year = 0;
}

static int tie_free(long a, long b, void *ctx)
{
    const PlanPair *p = ctx;
    int y, m, d;
    season_from_abs_day(a, &y, &m, &d);
    if (!pair_free(p->home, p->away, y, day_of_year(y, m, d), 0)) return 0;
    season_from_abs_day(b, &y, &m, &d);
    return pair_free(p->home, p->away, y, day_of_year(y, m, d), 0);
}

static void set_day(uint32_t fix, long z)
{
    int y, m, d;
    season_from_abs_day(z, &y, &m, &d);
    wr16(g_mem, fix + 0x28, (uint16_t)y);
    wr16(g_mem, fix + 0x2a, (uint16_t)day_of_year(y, m, d));
}

static void tie_snap(uint32_t fix, uint32_t ret)
{
    uint32_t home = rd32(g_mem, fix + 0x1c), away = rd32(g_mem, fix + 0x20), rec = rd32(g_mem, fix + 0x14);
    if (ret == TIE_SECOND_LEG) {
        if (tie_next.year && rec == tie_next.rec && home == tie_next.away && away == tie_next.home) {
            wr16(g_mem, fix + 0x28, (uint16_t)tie_next.year);
            wr16(g_mem, fix + 0x2a, (uint16_t)tie_next.day);
        }
        tie_next.year = 0;
        return;
    }
    tie_next.year = 0;
    uint32_t rounds = readable(tie_cup) ? rd32(g_mem, tie_cup + CUP_ROUNDS) : 0;
    int count = readable(tie_cup) ? (int16_t)rd16(g_mem, tie_cup + 0x3c) : 0;
    if (!readable(rounds) || tie_round < 0 || tie_round >= count) return;
    uint32_t rr = rounds + CUP_ROUND_SIZE * (uint32_t)tie_round;
    if (g_mem[rr + 0x21] != 2) return;
    int y = (int16_t)rd16(g_mem, fix + 0x28), d = (int16_t)rd16(g_mem, fix + 0x2a), fix_month, fix_day;
    from_day_of_year(y, d, &fix_month, &fix_day);
    if (keeps_original(fix_month >= 7 ? y : y - 1) || keeps_original(season_now())) return;
    if (!national_side(home) || !national_side(away) || !readable(rec)) return;
    Cpu saved = cpu;
    uint32_t a[1] = {rec};
    uint32_t finals = rt_callback(F_FINALS, 1, a) & 0xff;
    if (rt_unwinding) return;
    cpu = saved;
    if (finals) return;
    long leg1 = season_abs_day((int16_t)rd16(g_mem, tie_cup + 0x40) + (int8_t)g_mem[rr + 5], 1, 1) +
                (int16_t)rd16(g_mem, rr + 3),
         leg2 = leg1 + g_mem[rr + 0x22], out1, out2, draw = 0;
    if (tie_round + 1 < count) {                /* the next round's draw: +0 day, +2 year offset (0x521ea6) */
        uint32_t rn = rr + CUP_ROUND_SIZE;
        draw = season_abs_day((int16_t)rd16(g_mem, tie_cup + 0x40) + (int8_t)g_mem[rn + 2], 1, 1) +
               (int16_t)rd16(g_mem, rn);
    }
    int ty = (int16_t)rd16(g_mem, GAME_DATE + 2), td = (int16_t)rd16(g_mem, GAME_DATE);
    PlanPair p = {home, away, south_american(home, away)};
    int r = season_tie_legs(leg1, leg2, season_abs_day(ty, 1, 1) + td + 2, draw, plan_day_ok, tie_free, &p, &out1,
                            &out2);
    if (rt_unwinding) return;
    if (r > 0) {
        set_day(fix, out1);
        int ny, nm, nd;
        season_from_abs_day(out2, &ny, &nm, &nd);
        tie_next.rec = rec, tie_next.home = home, tie_next.away = away;
        tie_next.year = ny, tie_next.day = day_of_year(ny, nm, nd);
    }
    if (!r || !getenv("RECOMP_SCREEN_LOG")) return;
    int ly, lm, ld;
    season_from_abs_day(leg1, &ly, &lm, &ld);
    long l0 = season_abs_day(ly, 1, 1);
    if (r < 0)
        fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s, a two-leg tie of days %ld and %ld of %d, keeps its "
                        "days (no FIFA window fits)\n", win_str(rec + 4), win_str(home + 4), win_str(away + 4),
                leg1 - l0, leg2 - l0, ly);
    else
        fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s, a two-leg tie of days %ld and %ld of %d, plays on "
                        "the FIFA days %ld and %ld%s\n", win_str(rec + 4), win_str(home + 4), win_str(away + 4),
                leg1 - l0, leg2 - l0, ly, out1 - l0, out2 - l0, r == 2 ? " (no pair of the window was free)" : "");
}

/* From the add_fixture wrap (season_kind_cup_breaks.c): ret is the return address of the add_fixture call. */
void season_fifa_snap_fixture(uint32_t fix, uint32_t ret)
{
    if (ret == TIE_FIRST_LEG || ret == TIE_SECOND_LEG) {
        if (season_game_changes() && readable(fix)) tie_snap(fix, ret);
        return;
    }
    static const uint32_t calls[] = {0x00683a5au, 0x00683df3u, 0x009225c6u};
    int from = 0;
    for (unsigned k = 0; k < sizeof calls / sizeof calls[0]; k++) from |= ret == calls[k];
    if (!from || !season_game_changes() || !readable(fix)) return;
    /* Running seasons only (O375, O405, lead 4, 26 October 2026): a match of the start season keeps the original's
     * date, as that season keeps the original's qualifying dates (season_uefa_qual.c, run_dates). In 2008 the
     * qualifiers on the FIFA days of 6 and 10 September met the start season's Champions League groups (Liverpool
     * fielded 7 at Bordeaux, match_eng.cpp 652). The season of the match's own date (from 1 July, its year); a match
     * before the start season (the run to the start date of a new game, 0x5cc850) keeps its date too. */
    int y = (int16_t)rd16(g_mem, fix + 0x28), d = (int16_t)rd16(g_mem, fix + 0x2a), fix_month, fix_day;
    from_day_of_year(y, d, &fix_month, &fix_day);
    if (keeps_original(fix_month >= 7 ? y : y - 1)) return;
    uint32_t home = rd32(g_mem, fix + 0x1c), away = rd32(g_mem, fix + 0x20), rec = rd32(g_mem, fix + 0x14);
    if (!national_side(home) || !national_side(away) || !readable(rec)) return;
    Cpu saved = cpu;
    uint32_t a[1] = {rec};
    uint32_t finals = rt_callback(F_FINALS, 1, a) & 0xff;
    if (rt_unwinding) return;
    cpu = saved;
    if (finals) return;
    /* Made in the start season: it waits for the first running season (route M), the start season unchanged. */
    if (keeps_original(season_now())) {
        if (pending_count < PENDING_MAX)
            pending[pending_count++] = (Pending){rd32(g_mem, rec), (uint32_t)club_index(home),
                                                 (uint32_t)club_index(away), (int16_t)y, (int16_t)d,
                                                 (int16_t)end_year, (int16_t)end_day};
        return;
    }
    const char *how = "";
    int c = planned_target(rec, home, away, y, d, &how);
    if (c == NO_PLAN) c = fifa_target(home, away, y, d), how = "";
    if (rt_unwinding) return;
    cancel_friendlies(home, away, y, c >= 0 ? c : d, 0);
    if (rt_unwinding || c == d) return;
    if (c >= 0) wr16(g_mem, fix + 0x2a, (uint16_t)c);
    log_move(rec, home, away, d, c, y, how);
}

/* League::create_fixtures 0x6835c0 of a league (the wrap in season_uefa_league.c): its last match day bounds the
 * snap of its fixtures. The round table +0xba holds +0xa9 records of 0x41 bytes (0x683a82, 0x683a7a); a record's date
 * is its day of the year (+0) in the league's year +0x40 plus the year offset at +2 (0x6839f8..0x683a10). */
void season_fifa_stage_begin(uint32_t league)
{
    end_year = end_day = 0;
    stage_n = 0, stage_state = 0;
    uint32_t t = rd32(g_mem, league + 0xba);
    int n = (int16_t)rd16(g_mem, league + 0xa9), base = (int16_t)rd16(g_mem, league + 0x40);
    if (!readable(t) || n < 1 || n > 200) return;
    for (int r = 0; r < n; r++) {
        uint32_t rec = t + 0x41u * (uint32_t)r;
        int y = base + (int16_t)rd16(g_mem, rec + 2), d = (int16_t)rd16(g_mem, rec);
        if (y > end_year || (y == end_year && d > end_day)) end_year = y, end_day = d;
        /* The round days, sorted and each once (a stage of more rounds than the plan holds has no plan). */
        long z = season_abs_day(y, 1, 1) + d;
        int at = 0, seen = 0;
        while (at < stage_n && stage_own[at] < z) at++;
        seen = at < stage_n && stage_own[at] == z;
        if (seen) continue;
        if (stage_n == SEASON_STAGE_MAX) {
            stage_state = -1;
            continue;
        }
        memmove(stage_own + at + 1, stage_own + at, sizeof stage_own[0] * (size_t)(stage_n - at));
        stage_own[at] = z;
        stage_n++;
    }
}

void season_fifa_stage_end(void) { end_year = end_day = 0, stage_n = 0, stage_state = 0; }

/* ---- No TV move for qualifiers on FIFA days (0.6.2 item 205) ----
 *
 * League::move_fixtures_for_tv 0x685640, thiscall (League *, Date *) ret 4, is called by League::create_fixtures
 * (0x683eab, for each round) and by League::update_for_date 0x6847c0 (0x684b0d, each day). It finds the round of
 * the date in the round table (+0xba, +0xa9 records of 0x41 bytes), and for each TV slot of the round (+5, stride 7,
 * +0xb not 0xff, kind +0xa 0 to 2) it moves the league's fixtures still on that date to the slot's weekday (+9),
 * with a remove and a new add_fixture (0x685c7e). Its callers do not read eax (0x683eab, 0x684b0d), and it destroys
 * no argument (the epilogue 0x685cc7..0x685cdf: ret 4).
 *
 * In a running season the snap above places each qualifier of two national sides on a FIFA day, or keeps the
 * template day when the window is full. The TV move then took the fixtures that the snap left on their template
 * day and put them one day from the next round, and add_fixture 0x5aa680 moved them again for the clash (severity
 * 10, 0x5ab510 at 0x5aa96e): the OFC groups of 2033 played on 13, 20 and 26 April in place of 13, 16 and 18 April
 * (run-logs/0.6.2-s205/rep-jan). National teams play qualifiers on the dates of the draw, not on dates moved for
 * television. So a league the snap acts on gets no TV move: a running season of a game with changing rules, a
 * competition record (League +4, whose +0 is the id: the TV move's own news reads it at 0x68576e..0x68577b, and
 * League_Stage sets it at 0x68b173; +0x24 is the host stadium id, 0x685c4d) that is not a finals competition
 * (national_teams_is_finals_comp 0x76d940, as the snap), and a table (+0xb1 rows of 0x3b bytes, the club at +0,
 * the count at +0x3e) of national sides only. The start season of a 2001 start keeps the original
 * (keeps_original, the same-save new-game set). The skip also leaves out the TV move's news item and its two
 * utils_random calls (0x6859f7, 0x685a09), in running seasons only. */
#define LEAGUE_COMP 0x04u
#define LEAGUE_CLUBS 0x3eu
#define LEAGUE_TABLE 0xb1u
#define TABLE_ROW 0x3bu

void o_00685640(void);

static int snapped_league(uint32_t league)
{
    if (!season_game_changes() || !readable(league) || keeps_original(season_now())) return 0;
    int n = (int16_t)rd16(g_mem, league + LEAGUE_CLUBS);
    uint32_t table = rd32(g_mem, league + LEAGUE_TABLE), rec = rd32(g_mem, league + LEAGUE_COMP);
    if (n < 2 || n > 64 || !readable(table) || !readable(rec)) return 0;
    for (int r = 0; r < n; r++)
        if (!national_side(rd32(g_mem, table + TABLE_ROW * (uint32_t)r))) return 0;
    Cpu saved = cpu;
    uint32_t a[1] = {rec};
    uint32_t finals = rt_callback(F_FINALS, 1, a) & 0xff;
    if (rt_unwinding) return 0;
    cpu = saved;
    return !finals;
}

RT_WRAP(00685640, league_move_fixtures_for_tv)
{
    uint32_t league = cpu.ecx;
    int skip = snapped_league(league);
    if (rt_unwinding) return;
    if (!skip) {
        o_00685640();
        return;
    }
    if (getenv("RECOMP_SEASON_INTL_LOG") && rd32(g_mem, cpu.esp) == 0x00683eabu) {    /* at create, not each day */
        uint32_t date = rd32(g_mem, cpu.esp + 4), rec = rd32(g_mem, league + LEAGUE_COMP);
        fprintf(stderr, "recomp: intl dates: no TV move of %.40s on day %d of %d\n", win_str(rec + 4),
                readable(date) ? (int16_t)rd16(g_mem, date) : -1, readable(date) ? (int16_t)rd16(g_mem, date + 2) : -1);
    }
    cpu.esp += 8;                                       /* ret 4: the return address and the Date */
}
