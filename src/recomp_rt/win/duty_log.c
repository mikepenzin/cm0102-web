/* Test log of the original's call-up move (0.6.2 open items 157 to 162). RECOMP_DUTY_LOG=1 logs and only reads memory.
 *
 * NationalTeams::reschedule_clashing_club_fixtures 0x7777e0 (thiscall (Date *), ret 4; called by
 * NationalTeams::daily_update at 0x76b579 and at 0x77a070) counts, for each club, the players of its squad that a
 * national side holds on the date (0x7778b1..0x77793b), and for a count over 2 (0x77793d) calls
 * FIXTURE_MANAGER::reschedule_club_fixtures_near_date 0x5abb10 (thiscall (club, Date *), ret 8; 0x77795c). That one
 * takes the club's fixtures from the day before the date to the day after (0x54c8e0, 0x54c5b0, 0x5acbe0 at 0x5abb8f)
 * and for each removes it (0x5abc30), finds a new date (0x5ab510), adds it again (0x5aa680, flag 1) and queues the
 * news (0x5aac70 at 0x5abbeb). The log prints each call of 0x7777e0 and 0x5abb10 and, inside 0x5abb10, each removal and
 * each move with its old and new date. */
#include "win.h"
#include "../rt.h"
#include "duty_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIX_COMP 0x14u
#define FIX_HOME 0x1cu
#define FIX_AWAY 0x20u
#define FIX_YEAR 0x28u
#define FIX_DAY 0x2au
#define FIX_PRIORITY 0x3au
#define TODAY 0x00ae2c90u

static int readable(uint32_t p) { return rt_readable(p); }

static int inside;

int duty_log_on(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("RECOMP_DUTY_LOG");
        v = e && !strcmp(e, "1");
    }
    return v;
}

static const char *club_name(uint32_t c) { return readable(c) ? win_str(c + 4u) : "?"; }

static void fixture_text(uint32_t fix, char *out, size_t n)
{
    uint32_t rec = rd32(g_mem, fix + FIX_COMP);
    snprintf(out, n, "%.40s %.40s v %.40s (priority %d)", readable(rec) ? win_str(rec + 4u) : "?",
             club_name(rd32(g_mem, fix + FIX_HOME)), club_name(rd32(g_mem, fix + FIX_AWAY)),
             (int8_t)g_mem[fix + FIX_PRIORITY]);
}

/* RECOMP_DUTY_LOG_CLUB=<name>: after each call-up check, the mark of that club in the list of
 * NationalTeams::mark_clubs_with_fixtures 0x7775c0 (this +0x24, a byte per club: 0 no match within a day, 1 its own
 * league, 2 another competition of its nation, 3 a competition with no nation; 0x77770f, 0x777734, 0x777779). */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u

static void log_mark(uint32_t self)
{
    const char *name = getenv("RECOMP_DUTY_LOG_CLUB");
    uint32_t marks = rd32(g_mem, self + 0x24u), table = rd32(g_mem, CLUBS);
    if (!name || !*name || !readable(marks) || !readable(table)) return;
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; i < count; i++) {
        if (strcmp(win_str(table + (uint32_t)i * CLUB_SIZE + 4u), name)) continue;
        fprintf(stderr, "recomp: duty: %s has mark %d\n", name, g_mem[marks + (uint32_t)i]);
        /* Its call-up records, as 0x7777e0 counts them (0x7778c3..0x77793b): this +0x20 + 6 x club, +0 the
         * records of 9 bytes (+0 s16 day, +2 s16 year, +8 count), +5 their number. */
        uint32_t lists = rd32(g_mem, self + 0x20u);
        if (!readable(lists)) return;
        uint32_t e = lists + (uint32_t)i * 6u, recs = rd32(g_mem, e);
        int n = (int8_t)g_mem[e + 5u];
        for (int k = 0; k < n && readable(recs); k++) {
            uint32_t r = recs + 9u * (uint32_t)k;
            fprintf(stderr, "recomp: duty:   record day %d of %d, count %d\n", (int16_t)rd16(g_mem, r),
                    (int16_t)rd16(g_mem, r + 2u), (int8_t)g_mem[r + 8u]);
        }
        return;
    }
}

void o_007777e0(void);
RT_WRAP(007777e0, duty_reschedule)
{
    uint32_t self = cpu.ecx;
    if (duty_log_on()) {
        uint32_t d = rt_arg(0);
        fprintf(stderr, "recomp: duty: call-up check for day %d of %d (today day %d of %d, from 0x%x)\n",
                (int16_t)rd16(g_mem, d), (int16_t)rd16(g_mem, d + 2u), (int16_t)rd16(g_mem, TODAY),
                (int16_t)rd16(g_mem, TODAY + 2u), rd32(g_mem, cpu.esp));
    }
    o_007777e0();
    if (duty_log_on() && !rt_unwinding) log_mark(self);
}

void o_005abb10(void);
RT_WRAP(005abb10, duty_reschedule_club)
{
    if (duty_log_on()) {
        uint32_t c = rt_arg(0), d = rt_arg(1);
        fprintf(stderr, "recomp: duty: %s has more than 2 players called up for day %d of %d\n", club_name(c),
                (int16_t)rd16(g_mem, d), (int16_t)rd16(g_mem, d + 2u));
    }
    inside++;
    o_005abb10();
    inside--;
}

void duty_log_remove(uint32_t fix, int removed)
{
    if (!duty_log_on() || !inside || !readable(fix)) return;
    char t[200];
    fixture_text(fix, t, sizeof t);
    fprintf(stderr, "recomp: duty: near %s on day %d of %d: %s\n", t, (int16_t)rd16(g_mem, fix + FIX_DAY),
            (int16_t)rd16(g_mem, fix + FIX_YEAR), removed ? "removed to move" : "not removed, stays");
}

void duty_log_moved(uint32_t fix, uint32_t old_date)
{
    if (!duty_log_on() || !inside || rt_unwinding || !readable(fix)) return;
    char t[200];
    fixture_text(fix, t, sizeof t);
    fprintf(stderr, "recomp: duty: moved %s from day %d of %d to day %d of %d\n", t,
            readable(old_date) ? (int16_t)rd16(g_mem, old_date) : -1,
            readable(old_date) ? (int16_t)rd16(g_mem, old_date + 2u) : -1, (int16_t)rd16(g_mem, fix + FIX_DAY),
            (int16_t)rd16(g_mem, fix + FIX_YEAR));
}
