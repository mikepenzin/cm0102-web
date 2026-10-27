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
 * Europa League tie 71 days before the window). With no such day the fixture keeps its date (logged). Two-legged
 * qualifying ties (Cup::create_tie_fixtures) keep theirs, so a second leg never comes before its first. */
#include "win.h"
#include "season.h"
#include "season_uefa_days.h"
#include "season_names.h"

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


static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

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

/* Running seasons only (O405, lead 4, 26 October 2026): in the start season the table stays the original's, as
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
    if (season_now() <= season_start_year()) return;    /* a new game makes the table on 1 January of its year */
    moved_season = season_now();
    snap_rows(self);
}

void season_friendly_dates_day(void)
{
    if (!season_game_changes()) return;
    int now = season_now();
    if (now <= season_start_year()) return;
    if (friendly_obj && readable(friendly_obj) && now != moved_season) {
        moved_season = now;
        snap_rows(friendly_obj);
    }
    if (!rt_unwinding && pending_count) move_pending();
}

void season_friendly_dates_reset(void) { friendly_obj = 0, moved_season = -1, pending_count = 0; }

static void snap_rows(uint32_t self)
{
    uint32_t rows = rd32(g_mem, self + ROWS);
    int count = (int32_t)rd32(g_mem, self + ROW_COUNT), moved = 0, kept = 0, dropped = 0;
    if (!readable(rows) || count <= 0 || count > 400) return;
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
                        "used (no FIFA day that month)\n", moved, kept, dropped);
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
 * 6 September 2008, match_eng.cpp 652). The day lists of all types (count: the s8 at 0xb64738, key nation entries). */
static int side_busy(uint32_t a, uint32_t b, int y, int doy)
{
    uint32_t na = rd32(g_mem, a + 0x53), nb = rd32(g_mem, b + 0x53);
    int busy = 0, types = (int8_t)g_mem[TYPE_COUNT];
    if (types < 2 || types > 40) types = 2;
    for (int slice = 0; slice < 3 && !busy && !rt_unwinding; slice++)
        for (int type = 0; type < types && !busy && !rt_unwinding; type++) {
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
            if (rt_unwinding) return 1;
            cpu = saved;
            cpu.esp = saved_esp;
            for (uint32_t node = readable(list) ? rd32(g_mem, list + 2) : 0, k = 0; readable(node) && k < 4000;
                 node = rd32(g_mem, node + 6), k++) {
                uint32_t f = rd32(g_mem, node);
                if (!readable(f)) continue;
                uint32_t h = rd32(g_mem, f + 0x1c), w = rd32(g_mem, f + 0x20);
                uint32_t nh = readable(h) ? rd32(g_mem, h + 0x53) : 0, nw = readable(w) ? rd32(g_mem, w + 0x53) : 0;
                if (h == a || h == b || w == a || w == b || (nh && (nh == na || nh == nb)) ||
                    (nw && (nw == na || nw == nb)))
                    busy = 1;
                uint32_t rec = rd32(g_mem, f + 0x14);
                if (readable(rec) && !rd32(g_mem, rec + 0x5d) && g_mem[f + 0x3a] >= 2 && !national_side(h) &&
                    !national_side(w))
                    busy = 1;                                   /* a continental club match */
            }
        }
    return busy;
}

/* The last match day of the qualifying stage whose fixtures are being made (League::create_fixtures 0x6835c0, see
 * season_fifa_stage_begin), or none (end_year 0). A qualifier does not move past it: the snap's search of the whole
 * year moved the AFC first round of the 2006 cycle from May 2005 (days 121 to 146) to the FIFA days of October and
 * November (280, 315, 319), past the start of its final round (day 230), so the final round was made on day 319 with
 * its days gone, the AFC fifth's tie with CONCACAF's fourth ended after the World Cup draw, and the World Cup nets of
 * rows A8 to A11 acted (gate (a) run 8 on 8e58c44a, 2003 and 2007 starts; gate (b) 2002 and 2006 starts). */
static int end_year, end_day;

/* The FIFA day a qualifier of club home v club away on day d of year y moves to: the nearest free one of the same
 * year, 2 or more days after today and not after day ey/ed (its stage's last match day; ey 0: none); d when it is on
 * one already; -1 when none is free. */
static int fifa_target_by(uint32_t home, uint32_t away, int y, int d, int ey, int ed)
{
    int doys[FIFA_DOYS_MAX], n = fifa_doys(y, doys, FIFA_DOYS_MAX);
    for (int k = 0; k < n; k++)
        if (doys[k] == d) return d;                             /* on a FIFA day already */
    int today_year = (int16_t)rd16(g_mem, GAME_DATE + 2), today = (int16_t)rd16(g_mem, GAME_DATE);
    for (int dist = 1; dist <= SNAP_DAYS && !rt_unwinding; dist++)
        for (int sign = -1; sign <= 1; sign += 2) {
            int c = d + sign * dist, ok = 0;
            if (c < 0 || c >= days_in(y) || (y == today_year && c < today + 2) || y < today_year) continue;
            if (ey && (y > ey || (y == ey && c > ed)) && c > d) continue;      /* past its stage's last day */
            for (int k = 0; k < n; k++) ok |= doys[k] == c;
            if (!ok || side_busy(home, away, y, c)) continue;
            return rt_unwinding ? -1 : c;
        }
    return -1;
}

static int fifa_target(uint32_t home, uint32_t away, int y, int d)
{
    return fifa_target_by(home, away, y, d, end_year, end_day);
}

static void log_move(uint32_t rec, uint32_t home, uint32_t away, int d, int c, int y, const char *how)
{
    if (!getenv("RECOMP_SCREEN_LOG")) return;
    if (c >= 0)
        fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s moves from day %d to the FIFA day %d of %d%s\n",
                win_str(rec + 4), win_str(home + 4), win_str(away + 4), d, c, y, how);
    else
        fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s stays on day %d of %d (no free FIFA day after "
                        "today)%s\n", win_str(rec + 4), win_str(home + 4), win_str(away + 4), d, y, how);
}

/* ---- The qualifiers of later seasons made in the start season (route M, lead 4, 26 October 2026) ----
 *
 * In the start season the snap moves nothing, so that season stays the base game's (the same-save new-game set).
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
            uint32_t saved_esp = cpu.esp;
            cpu.esp -= 16;
            uint32_t date = cpu.esp;
            wr16(g_mem, date, (uint16_t)p->day);
            wr16(g_mem, date + 2, (uint16_t)p->year);
            wr32(g_mem, date + 4, 0);
            Cpu saved = cpu;
            cpu.ecx = rd32(g_mem, FIXTURES);
            uint32_t args[3] = {date, (uint32_t)slice, (uint32_t)type};
            uint32_t list = rt_callback(F_DAY_LIST, 3, args);
            if (rt_unwinding) return 0;
            cpu = saved;
            cpu.esp = saved_esp;
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
            c = fifa_target_by(home, away, y, d, pending[i].end_year, pending[i].end_day);
        if (rt_unwinding) return;
        if (c == d) continue;
        if (c < 0) {
            stayed++;
            log_move(rec, home, away, d, -1, y, " (made in the start season)");
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
 * home, away, year, day; then the stage's last year and day). */
#define ENTRY 20u
uint32_t season_friendly_part_size(void) { return pending_count ? 4u + ENTRY * (uint32_t)pending_count : 0u; }

void season_friendly_part_write(uint8_t *out)
{
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

/* From the add_fixture wrap (season_kind_cup_breaks.c): ret is the return address of the add_fixture call. */
void season_fifa_snap_fixture(uint32_t fix, uint32_t ret)
{
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
    if ((fix_month >= 7 ? y : y - 1) <= season_start_year()) return;
    uint32_t home = rd32(g_mem, fix + 0x1c), away = rd32(g_mem, fix + 0x20), rec = rd32(g_mem, fix + 0x14);
    if (!national_side(home) || !national_side(away) || !readable(rec)) return;
    Cpu saved = cpu;
    uint32_t a[1] = {rec};
    uint32_t finals = rt_callback(F_FINALS, 1, a) & 0xff;
    if (rt_unwinding) return;
    cpu = saved;
    if (finals) return;
    /* Made in the start season: it waits for the first running season (route M), the start season unchanged. */
    if (season_now() <= season_start_year()) {
        if (pending_count < PENDING_MAX)
            pending[pending_count++] = (Pending){rd32(g_mem, rec), (uint32_t)club_index(home),
                                                 (uint32_t)club_index(away), (int16_t)y, (int16_t)d,
                                                 (int16_t)end_year, (int16_t)end_day};
        return;
    }
    int c = fifa_target(home, away, y, d);
    if (rt_unwinding || c == d) return;
    if (c >= 0) wr16(g_mem, fix + 0x2a, (uint16_t)c);
    log_move(rec, home, away, d, c, y, "");
}

/* League::create_fixtures 0x6835c0 of a league (the wrap in season_uefa_league.c): its last match day bounds the
 * snap of its fixtures. The round table +0xba holds +0xa9 records of 0x41 bytes (0x683a82, 0x683a7a); a record's date
 * is its day of the year (+0) in the league's year +0x40 plus the year offset at +2 (0x6839f8..0x683a10). */
void season_fifa_stage_begin(uint32_t league)
{
    end_year = end_day = 0;
    uint32_t t = rd32(g_mem, league + 0xba);
    int n = (int16_t)rd16(g_mem, league + 0xa9), base = (int16_t)rd16(g_mem, league + 0x40);
    if (!readable(t) || n < 1 || n > 200) return;
    for (int r = 0; r < n; r++) {
        uint32_t rec = t + 0x41u * (uint32_t)r;
        int y = base + (int16_t)rd16(g_mem, rec + 2), d = (int16_t)rd16(g_mem, rec);
        if (y > end_year || (y == end_year && d > end_day)) end_year = y, end_day = d;
    }
}

void season_fifa_stage_end(void) { end_year = end_day = 0; }
