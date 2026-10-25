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
 * dates do (O381). The rows are sorted again. The base game keeps the original's table (Q152). */
#include "win.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>

#define ROWS 0x34ecu
#define ROW_COUNT 0x34f0u
#define ROW_SIZE 0x14u
#define F_DATE 0x00549f70u              /* Date::Date, thiscall (day, month 0 to 11, year, weekday; -1 the date) */
#define F_QSORT 0x00945333u
#define F_ROW_ORDER 0x005cc610u
#define NO_CONTINENT 0xfffffffeu        /* -2: no continent has it (-1 is every continent at 0x5cc060) */

typedef struct {
    short year;
    signed char month, day;             /* month 1 to 12 */
} FifaDay;
extern const FifaDay season_fifa_days[];
extern const int season_fifa_day_count;

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int days_in(int y) { return is_leap(y) ? 366 : 365; }
static int day_of_year(int y, int m, int d)     /* m 1 to 12, 0 is 1 January */
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return before[m - 1] + d - 1 + (m > 2 && is_leap(y));
}
static void from_day_of_year(int y, int doy, int *m, int *d)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int k = 0;
    for (; k < 11; k++) {
        int n = len[k] + (k == 1 && is_leap(y));
        if (doy < n) break;
        doy -= n;
    }
    *m = k + 1;
    *d = doy + 1;
}

/* The real FIFA day of month m (1 to 12) in year y nearest to day `want`, or 0 when the month has none. */
static int fifa_day(int y, int m, int want)
{
    int last = 0;
    for (int i = 0; i < season_fifa_day_count; i++)
        if (season_fifa_days[i].year > last) last = season_fifa_days[i].year;
    int best = 0;
    if (y <= last) {
        for (int i = 0; i < season_fifa_day_count; i++) {
            const FifaDay *f = &season_fifa_days[i];
            if (f->year == y && f->month == m && (!best || abs(f->day - want) < abs(best - want))) best = f->day;
        }
        return best;
    }
    /* After the list: the last year's days, 364 days a year later (plus 7 when the drift passes 3 days). */
    int shift = 364 * (y - last), drift = 0;
    for (int k = last; k < y; k++) drift += days_in(k) - 364;
    while (drift > 3) shift += 7, drift -= 7;
    for (int i = 0; i < season_fifa_day_count; i++) {
        const FifaDay *f = &season_fifa_days[i];
        if (f->year != last) continue;
        int doy = day_of_year(last, f->month, f->day) + shift, yy = last;
        while (doy >= days_in(yy)) doy -= days_in(yy), yy++;
        int mm, dd;
        from_day_of_year(yy, doy, &mm, &dd);
        if (yy == y && mm == m && (!best || abs(dd - want) < abs(best - want))) best = dd;
    }
    return best;
}

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
