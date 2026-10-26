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
 * The fix, in a season game: each row takes the real FIFA match day of its month and year that lies nearest to its
 * own day (season_fifa_days, from the results of each day counted in two lists, season_fifa_days_table.c). A month
 * with no FIFA match day that year (July in every year, August from 2014, January and most Decembers) gives the row
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

RT_WRAP(005cc510, friendly_set_row_dates)
{
    uint32_t self = cpu.ecx;
    o_005cc510();
    if (rt_unwinding || !season_game_changes() || !readable(self)) return;
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

/* From the add_fixture wrap (season_kind_cup_breaks.c): ret is the return address of the add_fixture call. */
void season_fifa_snap_fixture(uint32_t fix, uint32_t ret)
{
    static const uint32_t calls[] = {0x00683a5au, 0x00683df3u, 0x009225c6u};
    int from = 0;
    for (unsigned k = 0; k < sizeof calls / sizeof calls[0]; k++) from |= ret == calls[k];
    if (!from || !season_game_changes() || !readable(fix)) return;
    /* Running seasons only (O375, lead 4, 26 October 2026): the start season keeps the original's international
     * dates, as it keeps the original's qualifying dates (season_uefa_qual.c, run_dates). Its Champions League
     * groups keep the original's days, and in 2008 the qualifiers on the FIFA days of 6 and 10 September met them
     * (Liverpool fielded 7 at Bordeaux, match_eng.cpp 652). The season of today: from 1 July, today's year. */
    int now_year = (int16_t)rd16(g_mem, GAME_DATE + 2), now_month, now_day;
    from_day_of_year(now_year, (int16_t)rd16(g_mem, GAME_DATE), &now_month, &now_day);
    if ((now_month >= 7 ? now_year : now_year - 1) == season_start_year()) return;
    uint32_t home = rd32(g_mem, fix + 0x1c), away = rd32(g_mem, fix + 0x20), rec = rd32(g_mem, fix + 0x14);
    if (!national_side(home) || !national_side(away) || !readable(rec)) return;
    Cpu saved = cpu;
    uint32_t a[1] = {rec};
    uint32_t finals = rt_callback(F_FINALS, 1, a) & 0xff;
    if (rt_unwinding) return;
    cpu = saved;
    if (finals) return;
    int y = (int16_t)rd16(g_mem, fix + 0x28), d = (int16_t)rd16(g_mem, fix + 0x2a), doys[FIFA_DOYS_MAX];
    int n = fifa_doys(y, doys, FIFA_DOYS_MAX);
    for (int k = 0; k < n; k++)
        if (doys[k] == d) return;                               /* on a FIFA day already */
    int today_year = (int16_t)rd16(g_mem, GAME_DATE + 2), today = (int16_t)rd16(g_mem, GAME_DATE);
    for (int dist = 1; dist <= SNAP_DAYS && !rt_unwinding; dist++)
        for (int sign = -1; sign <= 1; sign += 2) {
            int c = d + sign * dist, ok = 0;
            if (c < 0 || c >= days_in(y) || (y == today_year && c < today + 2) || y < today_year) continue;
            for (int k = 0; k < n; k++) ok |= doys[k] == c;
            if (!ok || side_busy(home, away, y, c)) continue;
            if (rt_unwinding) return;
            wr16(g_mem, fix + 0x2a, (uint16_t)c);
            if (getenv("RECOMP_SCREEN_LOG"))
                fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s moves from day %d to the FIFA day %d of %d\n",
                        win_str(rec + 4), win_str(home + 4), win_str(away + 4), d, c, y);
            return;
        }
    if (!rt_unwinding && getenv("RECOMP_SCREEN_LOG"))
        fprintf(stderr, "recomp: intl dates: %.40s %.30s v %.30s stays on day %d of %d (no free FIFA day after today)\n",
                win_str(rec + 4), win_str(home + 4), win_str(away + 4), d, y);
}
