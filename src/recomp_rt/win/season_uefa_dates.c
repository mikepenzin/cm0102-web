/* The real dates of the UEFA knockout rounds (O259, season_uefa.h). The knockout tables of season_uefa.c and
 * season_uefa_cup.c had fixed dates in every season: the Champions League round of 16 drawn on 22 November with
 * first legs on 20 February, its quarter-finals, semi-finals and final on the original's 2001/02 dates
 * (create_round_table stage 12, 0x58437f..0x584409), the league phase knockout on the 2024/25 dates (Q547), and the
 * UEFA Cup, Europa League and Conference League knockout on the 2004/05 dates (semi-finals 28 April and 5 May,
 * final 18 May). The 2004/05 dates fell on the Russian Cup's semi-final and final days (RusCup::create_round_table
 * 0x7e9530): in a 2008 game Zenit's Europa League semi-final and final moved the 2009/10 cup semi-final and final
 * past 1 June (FIXTURE_MANAGER::add_fixture 0x5aa680, find_fixture_clashes 0x5ad780, find_new_fixture_date
 * 0x5ab510), docs/research/2026-10-25-uefa-ko-dates.md.
 *
 * A round's dates: cup_set_round_draw_date 0x521e60 writes the draw date at +0 (s16 day of the year) and +2 (the
 * year offset), cup_set_round_match_date 0x521eb0 the first leg at +3 and +5 (and the time at +6 and the venue at
 * +0x13, which stay); +0x22 is the days from the first leg to the second. Both build the date with Date::Date
 * 0x549f70 (thiscall: day, month 0 to 11, year, weekday; ret 0x10), which moves to a weekday unless it is -1
 * (0x54a0f2); here -1, the real date. */
#include "win.h"
#include "season_uefa.h"

#include <stddef.h>

#define F_DATE 0x00549f70u
#define ROUND_SIZE 0x68u

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

/* Date::Date into a scratch block below the frame: the s16 day of the year and the s16 year. */
static int make_date(int day, int month, int year, int *doy, int *yr)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t tmp = cpu.esp;
    Cpu saved = cpu;
    cpu.ecx = tmp;
    uint32_t a[4] = {(uint32_t)day, (uint32_t)month, (uint32_t)year, 0xffffffffu};
    rt_callback(F_DATE, 4, a);
    if (rt_unwinding) return 0;
    cpu = saved;
    *doy = (int16_t)rd16(g_mem, tmp);
    *yr = (int16_t)rd16(g_mem, tmp + 2);
    cpu.esp = saved_esp;
    return 1;
}

int season_uefa_ko_dates(uint32_t table, char comp, int season, const int *kinds, int rounds)
{
    int n = 0;
    for (int r = 0; r < rounds && readable(table) && !rt_unwinding; r++) {
        const SeasonUefaKoDate *d = season_uefa_ko_date(comp, season, kinds[r]);
        if (!d) continue;
        uint32_t rec = table + ROUND_SIZE * (uint32_t)r;
        int doy, yr;
        if (!make_date(d->draw_day, d->draw_month, season + d->draw_year, &doy, &yr)) return n;
        wr16(g_mem, rec, (uint16_t)doy);
        g_mem[rec + 2] = (uint8_t)(yr - season);
        if (!make_date(d->day, d->month, season + d->year_off, &doy, &yr)) return n;
        wr16(g_mem, rec + 3, (uint16_t)doy);
        g_mem[rec + 5] = (uint8_t)(yr - season);
        if (d->gap) g_mem[rec + 0x22] = (uint8_t)d->gap;
        n++;
    }
    return n;
}
