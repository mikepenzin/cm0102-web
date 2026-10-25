/* The July and August dates of international friendlies in a season game (row A18 of the 0.6.0 release test, O375).
 *
 * Friendly::build_international_date_table 0x5ca4f0 (called once, from Friendly::Friendly 0x5c1f80 at 0x5c2180)
 * writes 110 rows of 0x14 bytes at [this + 0x34ec], count [this + 0x34f0]: +0 a weekday code, +1 the day, +2 the
 * month (0 to 11), +3 the Date (8 bytes: s16 day of the year, s16 year, a word), +0xb a year offset 0 to 3, +0xc the
 * continent (the value of a key continent), +0x10 a key id. Its dates are the FIFA windows of 2001. Then 0x5cc510
 * (called at 0x5cb98a) sets each Date with Date::set 0x54a130 for the World Cup year of the cycle plus the offset
 * (0x5cc56b..0x5cc5ac) and sorts the rows by date (qsort 0x945333 with 0x5cc610). The arranger
 * arrange_international_friendlies 0x5cbdf0 finds the rows of a date by bsearch and pairs nations of the row's
 * continent (docs/research/2026-10-18-national-team-comps.md, section 7).
 *
 * The August rows (South America 16/8, Africa 17/8 and 18/8, North America 25/8) fall on a Wednesday from about 13
 * to 21 August, the July rows (Oceania 10/7 and 25/7, Africa 24/7, North America 25/7, South America 26/7) in late
 * July. In a season game that is often a qualifying match day of the Champions League or the Europa League,
 * and NationalTeams::can_schedule_club_fixture 0x776fc0 then refuses the club's tie when more than two of its
 * players are on international duty within a day: FIXTURE_MANAGER::find_fixture_clashes 0x5ad780 gives clash 21
 * (0x5adb6b), and add_fixture 0x5aa680 moves the tie a week (0x5ab510 at 0x5aa96e). In a 2006 game (gate (b) run on
 * eee3e71a, 2009/10) the Champions League third qualifying round tie Aalborg v Borussia Dortmund moved from 19 to
 * 26 August, so its losers reached the Europa League after the play-off draw was due (row A18, "drawn late"); in
 * 2016/17 the second qualifying round tie Keflavik v Espanyol moved from 27 July and each round after it followed.
 *
 * The real FIFA calendar has one August date, a Wednesday, up to 2013, and none from 2014 (results of that day
 * counted in eloratings.net and the martj42 international results list, both read 25 October 2026; for 2013 and
 * 2014 also the FIFA International Match Calendar 2013-18), and no July date in any year from 2001 to 2026 (at most
 * three friendlies on a July day in both lists). In a season game each August row gets the real date of its year,
 * or none, and each July row none: such a row's continent becomes -1, which no nation has, so the arranger pairs no
 * one on it.
 * The rows are sorted again. The base game keeps the original's table (Q152). */
#include "win.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>

#define ROWS 0x34ecu
#define ROW_COUNT 0x34f0u
#define ROW_SIZE 0x14u
#define JULY 6
#define AUGUST 7
#define F_DATE 0x00549f70u              /* Date::Date, thiscall (day, month 0 to 11, year, weekday; -1 the date) */
#define F_QSORT 0x00945333u
#define F_ROW_ORDER 0x005cc610u

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

/* The real August date of year y (2001 to 2013), 0 for none. */
static int fifa_august_day(int y)
{
    static const int days[] = {15, 21, 20, 18, 17, 16, 22, 20, 12, 11, 10, 15, 14};   /* 2001 .. 2013 */
    return y >= 2001 && y <= 2013 ? days[y - 2001] : 0;
}

void o_005cc510(void);

RT_WRAP(005cc510, friendly_set_row_dates)
{
    uint32_t self = cpu.ecx;
    o_005cc510();
    if (rt_unwinding || !season_game_changes() || !readable(self)) return;
    uint32_t rows = rd32(g_mem, self + ROWS);
    int count = (int32_t)rd32(g_mem, self + ROW_COUNT), moved = 0, dropped = 0;
    if (!readable(rows) || count <= 0 || count > 400) return;
    Cpu saved = cpu;
    for (int i = 0; i < count && !rt_unwinding; i++) {
        uint32_t row = rows + ROW_SIZE * (uint32_t)i;
        if (g_mem[row + 2] != AUGUST && g_mem[row + 2] != JULY) continue;
        int year = (int16_t)rd16(g_mem, row + 3 + 2), day = g_mem[row + 2] == AUGUST ? fifa_august_day(year) : 0;
        if (!day) {
            wr32(g_mem, row + 0xc, 0xffffffffu);
            dropped++;
            continue;
        }
        g_mem[row + 1] = (uint8_t)day;
        cpu.ecx = row + 3;
        uint32_t a[4] = {(uint32_t)day, AUGUST, (uint32_t)year, 0xffffffffu};
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
        fprintf(stderr, "recomp: friendlies: %d August rows on the real FIFA date, %d July and August rows not used "
                        "(no FIFA date)\n", moved, dropped);
}
