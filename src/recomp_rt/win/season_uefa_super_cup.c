/* The venue and date of the UEFA Super Cup by season (0.6.0 slice 23, change point U6: Stade Louis II in
 * Monaco to 2012, then a venue each year; season_uefa.h, the sc_* fields of the row). Evidence:
 * docs/research/2026-10-18-uefa-2009-2020.md.
 *
 * The original. EurSuperCup::create_round_table 0x57a3c0 (thiscall (stage, &rounds, &name, &league) ret 0x10)
 * makes one round and calls cup_set_round_match_date 0x521eb0 at 0x57a4a2 with 24 August and the venue flags
 * 0x8000, which the function writes as a dword to the round record +0x13 (0x521f08). When the fixture is made
 * (Cup::create_tie_fixtures 0x51cb90, 0x51ceb6), Stadium::set_fixture_venue 0x85c260 reads the flags: 0x8000
 * is the home ground (club +0x69) of the club [0x9d0264], the key id of "AS Monaco" (0x85c346..0x85c37a).
 * flag 0x10 is the stadium record of the cup's +0x24 ([0xae23b0] + id * 78, 0x85c5a7..0x85c5d3), which the Comp
 * constructor sets to -1 (0x48c5df) and the cup save keeps (0x5212a4).
 *
 * This file. In a season game whose row names a city other than Monaco, the wrap of 0x57a3c0 looks for the
 * stadium of the row by name in the stadium table ([0xae23b0], count [0xae2358], name at +4, 51 bytes): the
 * whole name, then the name before a bracket, then the name in the bracket ("Fortuna Arena (Eden Arena)"). When it
 * finds it, the round gets the flags 0x10 and the cup +0x24 its record index. The match date is the row's in
 * every season game. A stadium that the database does not hold by any name gets the national stadium of the nation of the row's
 * city (a fallback, the log names the stadium and the nation). Only when that fails does the original's venue stay.
 * The base game and RECOMP_SEASON_CODE=off get the original's round (season_uefa_row gives NULL). */
#include "win.h"
#include "season_nl.h"                     /* slice 28: the Finalissima */
#include "season.h"
#include "season_uefa.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define STADIUMS 0xae23b0u
#define STADIUM_COUNT 0xae2358u
#define STADIUM_SIZE 78u
#define CITIES 0xae23acu
#define CITY_COUNT 0xae2354u
#define CITY_SIZE 56u
#define NATIONS 0xae23a8u
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 290u
#define F_MATCH_DATE 0x00521eb0u        /* cdecl (table, round, day, month, year+, weekday, time, year, venue) */
#define VENUE_STADIUM 0x10u             /* the stadium record of the cup's +0x24 (0x85c5a7) */
#define VENUE_MONACO 0x8000u            /* 0x57a48d */

static int readable(uint32_t p) { return rt_readable(p); }

static int same_name(const char *a, const char *b, size_t n)
{
    size_t i = 0;
    for (; i < n && a[i] && b[i]; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return i == n && (!a[i] || i == n) && !b[i];
}

/* The record index of the stadium named name (n characters), or -1. */
static int find_stadium(const char *name, size_t n)
{
    uint32_t table = rd32(g_mem, STADIUMS);
    int32_t count = (int32_t)rd32(g_mem, STADIUM_COUNT);
    while (n && name[n - 1] == ' ') n--;
    for (int32_t i = 0; n && readable(table) && i < count && i < 100000; i++) {
        char rec[52];
        snprintf(rec, sizeof rec, "%.51s", win_str(table + STADIUM_SIZE * (uint32_t)i + 4));
        if (strlen(rec) == n && same_name(name, rec, n)) return i;
    }
    return -1;
}

/* A link of a record to another table. The wrap runs only in a running game, where the link is a pointer to the
 * record (the city's nation at +0x1f, the nation's stadium at +0x80). Returns the record index, or -1 for no link
 * (0 or 0xffffffff, so a null link is never record 0) and for a value outside the table. */
static int32_t record_index(uint32_t v, uint32_t table, uint32_t size, int32_t count)
{
    if (v == 0 || v == 0xffffffffu || !readable(table)) return -1;
    if (v >= table && v < table + size * (uint32_t)count && (v - table) % size == 0) return (int32_t)((v - table) / size);
    return -1;
}

/* Fallback when the database lacks the venue by every name. The row's city is a record of the city table (name
 * at +4, the nation at +0x1f). The national stadium of that nation is the link at +0x80 of its nation record
 * (read from a running game, as the Czech Republic and Estonia show). Returns the stadium record index or -1;
 * *nation_name gets the nation's name for the log. */
static int national_stadium_of_city(const char *city, char *nation_name, size_t n)
{
    uint32_t ct = rd32(g_mem, CITIES), nt = rd32(g_mem, NATIONS);
    int32_t cities = (int32_t)rd32(g_mem, CITY_COUNT), nations = (int32_t)rd32(g_mem, NATION_COUNT);
    int32_t stadiums = (int32_t)rd32(g_mem, STADIUM_COUNT);
    if (!readable(ct) || !readable(nt)) return -1;
    for (int32_t i = 0; i < cities && i < 100000; i++) {
        char rec[32];
        snprintf(rec, sizeof rec, "%.27s", win_str(ct + CITY_SIZE * (uint32_t)i + 4));
        if (strlen(rec) != strlen(city) || !same_name(city, rec, strlen(city))) continue;
        int32_t nation = record_index(rd32(g_mem, ct + CITY_SIZE * (uint32_t)i + 0x1f), nt, NATION_SIZE, nations);
        if (nation < 0) continue;
        int32_t id = record_index(rd32(g_mem, nt + NATION_SIZE * (uint32_t)nation + 0x80), rd32(g_mem, STADIUMS),
                                  STADIUM_SIZE, stadiums);
        if (id < 0) continue;
        snprintf(nation_name, n, "%.51s", win_str(nt + NATION_SIZE * (uint32_t)nation + 4));
        return id;
    }
    return -1;
}

static int stadium_of_row(const char *name)
{
    int id = find_stadium(name, strlen(name));
    const char *open = strchr(name, '(');
    if (id < 0 && open) id = find_stadium(name, (size_t)(open - name));
    if (id < 0 && open && strchr(open, ')')) id = find_stadium(open + 1, (size_t)(strchr(open, ')') - open - 1));
    return id;
}

/* The weekday of a date, 0 Monday to 6 Sunday (the game's weekdays: 2 Wednesday, 3 Thursday, 4 Friday, seen in
 * season_uefa_qual.c), month 0 to 11. */
static int weekday_of(int year, int month, int day)
{
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year - (month < 2);
    int sunday0 = (y + y / 4 - y / 100 + y / 400 + t[month] + day) % 7;     /* 0 Sunday */
    return (sunday0 + 6) % 7;
}

void o_0057a3c0(void);

RT_WRAP(0057a3c0, super_cup_round_table)
{
    uint32_t cup = cpu.ecx;
    /* Slice 28 part b: the Finalissima object of this class has its own round (season_gold_cup.c). */
    if (season_finalissima_round_table(o_0057a3c0)) return;
    o_0057a3c0();
    if (rt_unwinding || !readable(cup)) return;
    uint32_t t = cpu.eax;
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cup + 0x40);
    const SeasonUefa *r = season_uefa_row((int)year);
    if (!readable(t) || !r || !r->sc_city || r->sc_day <= 0 || rd32(g_mem, t + 0x13) != VENUE_MONACO) return;
    uint32_t venue = VENUE_MONACO;
    if (strcmp(r->sc_city, "Monaco")) {
        int id = stadium_of_row(r->sc_stadium);
        char nation[52] = "";
        int fallback = 0;
        if (id < 0) {
            id = national_stadium_of_city(r->sc_city, nation, sizeof nation);
            fallback = id >= 0;
        }
        if (id >= 0) {
            uint32_t table = rd32(g_mem, STADIUMS);
            venue = VENUE_STADIUM;
            wr32(g_mem, cup + 0x24, (uint32_t)id);
            fprintf(stderr, "recomp: uefa: Super Cup %u at %s, %s (stadium %d)\n", year,
                    fallback ? win_str(table + STADIUM_SIZE * (uint32_t)id + 4) : r->sc_stadium, r->sc_city, id);
            if (fallback)
                fprintf(stderr, "recomp: uefa: Super Cup %u: no stadium %s in the database; the national stadium of %s\n",
                        year, r->sc_stadium, nation);
        } else {
            fprintf(stderr, "recomp: uefa: Super Cup %u: no stadium %s in the database; the original's venue\n", year,
                    r->sc_stadium);
        }
    }
    /* Slice 24: no extra time from 2023 (the row's "penalties after 90 minutes"). The original writes the round
     * flags 0x83 at 0x57a4be: 0x2 extra time, 0x80 golden goal, 0x1 penalties (2026-10-17-cup-round-record.md,
     * section 2). With no extra time only 0x1 stays. */
    if (!r->sc_extra_time) {
        wr16(g_mem, t + 0xd, (uint16_t)((rd16(g_mem, t + 0xd) & ~0x83u) | 0x1u));
        fprintf(stderr, "recomp: uefa: Super Cup %u: penalties after 90 minutes\n", year);
    }
    /* The weekday of the real date, so the date function (0x549f70 from 0x521ed3), which moves a match to the
     * nearest day of the round's weekday, keeps the real day (the original passes 4, a Friday, at 0x57a48d). */
    Cpu saved = cpu;
    uint32_t m[9] = {t, 0, (uint32_t)r->sc_day, (uint32_t)r->sc_month, 0, (uint32_t)weekday_of((int)year, r->sc_month,
                     r->sc_day), 2, year, venue};
    rt_callback(F_MATCH_DATE, 9, m);
    if (rt_unwinding) return;
    cpu = saved;
}
