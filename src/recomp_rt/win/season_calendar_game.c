/* The Irish calendar move of 2003 in the game (season_calendar.h has the rules and the dates; gap row 2 of the 0.7.2
 * league sizes).
 *
 * The league table entry. key_nation_build_table 0x667150 writes entry 20 (0xb64300) from fixed code
 * (0x66834b..0x668443), with no field for the calendar: +0x15 the update day (day of Date(20 June, 2001)), +0x17 the
 * two-years flag (1, 0x668424; 0 in the calendar-year blocks, Japan 0x6685ff), +0x2a the start and +0x39 the end
 * (Date::Date 0x549f70: +0 s16 day of the year, +2 s16 year, +4 the leap flag, 0x54a06c..0x54a0e7). The creator
 * 0x834e60 writes the parts: +0x32 u16 day, +0x34 u8 month from 0, +0x35 u16 year, +0x37 u16 weekday, and +0x41..
 * +0x46 of the end (0x834eff..0x834f36). key_nation_create_all_comps 0x669180 calls the creator of each selected
 * nation and builds +0x2a and +0x39 again from the parts (0x6691b0..0x6691ff). game_season_update_pass 0x5d5ae0, on
 * the update day, steps +0x35 and +0x44 by +0x1d when +0x35 is not the game year (0x5d5bef..0x5d5c0f) and builds both
 * dates from the parts (0x5d5c0e..0x5d5c56); it then starts each complete competition (0x5d5ccb..0x5d5d1c).
 *
 * Nothing of the entry is in the save: a load builds it again with 0x667150 (0x82b845) and 0x669180 (0x82c5aa). So the
 * entry's state comes from the start year, changing rules and IrePrm's edition +0x40, which the save holds, and is
 * written after every 0x669180 (a new game and a load) and after each start_new_season of IrePrm (the rule kind
 * below). No save field (decision 9).
 *
 * The leagues: season_sizes.c asks season_calendar_template for the dates of IrePrm (0x639e90) and IreFirst
 * (0x634f40), and season_calendar_stage for the play-off of IreFirst (create_promotion_playoff_stage 0x635dc0 calls
 * create_schedule for stage 0, whose draw 24 April and match 27 April of the next year come from 0x63588a..0x6358af).
 * The cups: the wraps below move the round dates of the FAI Cup (IreChalCup::create_round_table 0x634360), the Munster
 * Cup (0x638a60), the President's Cup (0x639530) and the Leinster Cup (0x636760). The League Cup plays in September
 * (IreLgeCup::create_schedule 0x637120), inside both seasons. The awards: rt_ire_award_day and rt_ire_award_month
 * (tools/recomp/league_sizes.py PATCHES). The labels: the wraps of key_nation_format_club_season 0x669630 and
 * key_nation_format_comp_season 0x6697c0, and season_calendar_history_label for season_history.c. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_rules.h"
#include "season_calendar.h"
#include "season_sizes.h"
#include "season_template.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRIES 0xb63d60u
#define ENTRY_SIZE 0x48u
#define ENTRY_AT (ENTRIES + SEASON_CAL_ENTRY * ENTRY_SIZE)
#define E_UPDATE 0x15u              /* s16 day of the year */
#define E_TWO_YEARS 0x17u           /* u32 */
#define E_START 0x2au               /* Date */
#define E_START_PARTS 0x32u         /* u16 day, u8 month from 0 (+2), u16 year (+3), u16 weekday (+5) */
#define E_END 0x39u                 /* Date */
#define E_END_PARTS 0x41u
#define NO_WEEKDAY 0xffffu          /* Date::Date keeps the date (0x54a0f2) */
#define COMP_YEAR 0x40u             /* s16 the edition (start_new_season adds 1, 0x574c10) */
#define GAME_DATE 0xae2c90u         /* s16 day of the year, s16 year */
#define ROUND_SIZE 0x68u            /* a round of a cup round table (cup_set_round_draw_date 0x521e60: table + round x
                                     * 0x68; +0 s16 draw day, +2 s8 draw year from the year argument, +3 s16 match
                                     * day, +5 s8 match year, cup_set_round_match_date 0x521eb0) */
#define MAX_DATES 64

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_CALENDAR_LOG") && *getenv("RECOMP_CALENDAR_LOG");
    return on;
}

static const char *const STATE_NAMES[] = {"winter", "short season", "calendar years"};

/* The state of an edition in this game. */
static int state_of(int edition)
{
    if (!season_sizes_on()) return SEASON_CAL_WINTER;
    return season_calendar_state(season_start_year(), season_changing_rules(), edition);
}

/* IrePrm, the first competition of the entry (the creator puts it at index 0, 0x83505d), or 0 when Ireland is not
 * selected. */
static uint32_t ire_prm(void)
{
    uint32_t comps[16];
    int n = season_change_entry_comps(SEASON_CAL_ENTRY, comps, 16);
    for (int i = 0; i < n; i++)
        if (!strcmp(season_change_file(comps[i]), "ire_prm")) return comps[i];
    return 0;
}

/* The state now: by IrePrm's edition, winter when Ireland is not selected. */
static int state_now(void)
{
    uint32_t p = ire_prm();
    return p ? state_of((int16_t)rd16(g_mem, p + COMP_YEAR)) : SEASON_CAL_WINTER;
}

static void write_date(uint32_t at, SeasonCalDate d)
{
    wr16(g_mem, at, (uint16_t)season_calendar_doy(d.year, d.month, d.day));
    wr16(g_mem, at + 2, (uint16_t)d.year);
    wr32(g_mem, at + 4, season_template_year_days(d.year) == 366);
}

static void write_parts(uint32_t at, SeasonCalDate d)
{
    wr16(g_mem, at, (uint16_t)d.day);
    g_mem[at + 2] = (uint8_t)(d.month - 1);
    wr16(g_mem, at + 3, (uint16_t)d.year);
    wr16(g_mem, at + 5, NO_WEEKDAY);
}

/* Writes the entry of a state for the season of year y (the edition). */
static void write_entry(int state, int y, const char *why)
{
    if (state == SEASON_CAL_WINTER) return;
    SeasonCalDate start, end, ps, pe;
    season_calendar_dates(state, y, &start, &end, &ps, &pe);
    wr16(g_mem, ENTRY_AT + E_UPDATE, (uint16_t)season_calendar_update_doy());
    wr32(g_mem, ENTRY_AT + E_TWO_YEARS, state == SEASON_CAL_MOVE);
    write_parts(ENTRY_AT + E_START_PARTS, ps);
    write_parts(ENTRY_AT + E_END_PARTS, pe);
    write_date(ENTRY_AT + E_START, start);
    write_date(ENTRY_AT + E_END, end);
    if (logging())
        fprintf(stderr, "recomp: calendar: Ireland %d: %s (%s), %d-%02d-%02d to %d-%02d-%02d, update day %d\n", y,
                STATE_NAMES[state], why, start.year, start.month, start.day, end.year, end.month, end.day,
                season_calendar_update_doy());
}

/* The rows of the moved files follow the move while it can happen in this game (season_rules_row_of). */
static void set_moved(void)
{
    season_rules_set_moved(season_sizes_on() &&
                           (season_start_year() >= SEASON_CAL_FIRST || season_changing_rules()));
}

/* key_nation_create_all_comps, cdecl (): a new game and a load (0x8295f5, 0x82c5aa). */
void o_00669180(void);
RT_WRAP(00669180, create_all_comps)
{
    set_moved();
    o_00669180();
    if (rt_unwinding) return;
    uint32_t p = ire_prm();
    if (!p) return;
    int edition = (int16_t)rd16(g_mem, p + COMP_YEAR);
    write_entry(state_of(edition), edition, "the competitions made");
}

/* setup_set_game_start_date, cdecl (): create_game 0x828110 calls it (0x828474) before the creators (0x8295f5); it
 * takes the start of the main nation's entry (+0x2a, 0x830565). A game that starts in 2003 or later with Ireland as the
 * main nation starts on the start of its calendar season, as a Japanese start takes the entry's March (decision 7). The
 * entry is set back after the call: the creator's wrap above writes it for a selected Ireland. */
#define MAIN_NATION 0xae2d08u       /* the first selected nation of Select League(s) (0x81dd54..0x81e487) */
#define START_DATE 0xdd6ec8u        /* the game's start date, set from the main nation's entry (0x83056e..0x830573) */

/* RECOMP_CALENDAR_LOG: the start date that the call set, and the main nation's league table entry. */
static void log_start(void)
{
    if (!logging()) return;
    uint32_t main = rd32(g_mem, MAIN_NATION);
    int e = -1;
    for (int i = 0; i < 35 && main; i++)
        if (rd32(g_mem, ENTRIES + (uint32_t)i * ENTRY_SIZE) == main) e = i;
    fprintf(stderr, "recomp: calendar: game start date day %d of %d, main nation entry %d\n",
            (int16_t)rd16(g_mem, START_DATE), (int16_t)rd16(g_mem, START_DATE + 2), e);
}

void o_00830520(void);
RT_WRAP(00830520, set_game_start_date)
{
    int y = season_start_year();
    set_moved();
    if (state_of(y) != SEASON_CAL_YEAR) {
        o_00830520();
        if (!rt_unwinding) log_start();
        return;
    }
    uint8_t keep[ENTRY_SIZE];
    memcpy(keep, g_mem + ENTRY_AT, ENTRY_SIZE);
    write_entry(SEASON_CAL_YEAR, y, "the start date");
    o_00830520();
    memcpy(g_mem + ENTRY_AT, keep, ENTRY_SIZE);
    if (!rt_unwinding) log_start();
}

/* The rule kind: after IrePrm's start_new_season (the change of 20 June 2002 and of each February from 2003). The
 * original's pass has already stepped the dates. */
static void calendar_after(const SeasonChange *c)
{
    if (c->entry != SEASON_CAL_ENTRY || !c->started || !c->comp || strcmp(c->file, "ire_prm")) return;
    int edition = (int16_t)rd16(g_mem, c->comp + COMP_YEAR);
    write_entry(state_of(edition), edition, "the season change");
}

const SeasonRuleKind season_kind_calendar = {"calendar", NULL, NULL, NULL, NULL, NULL, calendar_after};

/* ---- The leagues ---- */

int season_calendar_template(uint32_t obj, const char *file, int base, int *days, int n, int *cut)
{
    *cut = 0;
    if (!season_calendar_league(file)) return 0;
    int state = state_of((int16_t)rd16(g_mem, obj + COMP_YEAR));
    if (state == SEASON_CAL_MOVE) {
        *cut = season_calendar_move_cut(base);
        if (logging()) fprintf(stderr, "recomp: calendar: %s %d: the short season plans from the dates before day %d\n",
                               file, base, *cut);
        return 1;
    }
    if (state != SEASON_CAL_YEAR) return 0;
    int shift = season_calendar_league_shift(base, days, n);
    for (int i = 0; i < n; i++) days[i] += shift;
    if (logging())
        fprintf(stderr, "recomp: calendar: %s %d: the class's dates move %d weeks\n", file, base, shift / 7);
    return 1;
}

/* A date of a round table record: the day from 1 January of base. */
static int round_day(uint32_t at, int base) { return season_template_day(base, (int8_t)g_mem[at + 2], (int16_t)rd16(g_mem, at)); }

static void set_round_day(uint32_t at, int base, int day)
{
    int yoff, doy;
    season_template_split(base, day, &yoff, &doy);
    wr16(g_mem, at, (uint16_t)doy);
    g_mem[at + 2] = (uint8_t)(int8_t)yoff;
}

void season_calendar_stage(uint32_t obj, const char *file, int stage, uint32_t count_at)
{
    /* IreFirst's play-off (stage 0, one round): the draw the day after the table is complete and the first match 3
     * days after it, as the real play-offs (2003: last round 30 November, semi-finals 3 December). */
    if (!file || strcmp(file, "ire_first") || stage != 0 || !readable(count_at)) return;
    int state = state_of((int16_t)rd16(g_mem, obj + COMP_YEAR));
    uint32_t table = cpu.eax;
    if (state == SEASON_CAL_WINTER || !readable(table) || (int16_t)rd16(g_mem, count_at) != 1) return;
    int base = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    int today = season_template_day(base, (int16_t)rd16(g_mem, GAME_DATE + 2) - base, (int16_t)rd16(g_mem, GAME_DATE));
    set_round_day(table, base, today + 1);
    set_round_day(table + 3, base, today + 3);
    if (logging()) fprintf(stderr, "recomp: calendar: ire_first %d: play-off from day %d\n", base, today + 3);
}

/* ---- The cups ---- */

/* After a cup's create_round_table (thiscall: stage, &count, &code, &flag; ret 0x10; eax the table of count rounds,
 * 0x634386..0x634399): its dates moved into the room of the season (season_calendar_cup_plan). */
static void cup_dates(uint32_t self, uint32_t count_at)
{
    const char *file = season_change_file(self);
    uint32_t table = cpu.eax;
    if (!season_calendar_cup(file) || !readable(table) || !readable(count_at)) return;
    int base = (int16_t)rd16(g_mem, self + COMP_YEAR), state = state_of(base);
    int rounds = (int16_t)rd16(g_mem, count_at);
    if (state == SEASON_CAL_WINTER || rounds < 1 || 2 * rounds > MAX_DATES) return;
    int days[MAX_DATES], out[MAX_DATES];
    for (int r = 0; r < rounds; r++) {
        days[2 * r] = round_day(table + (uint32_t)r * ROUND_SIZE, base);
        days[2 * r + 1] = round_day(table + (uint32_t)r * ROUND_SIZE + 3, base);
    }
    int today = season_template_day(base, (int16_t)rd16(g_mem, GAME_DATE + 2) - base, (int16_t)rd16(g_mem, GAME_DATE));
    int how = season_calendar_cup_plan(days, 2 * rounds, today + 1, season_calendar_cup_cut(base, state),
                                       season_calendar_cup_target(file, state, base), out);
    if (!how) {
        fprintf(stderr, "recomp: calendar: %s %d: no room for its %d rounds before the update day; its dates stay\n",
                file, base, rounds);
        return;
    }
    for (int r = 0; r < rounds; r++) {
        set_round_day(table + (uint32_t)r * ROUND_SIZE, base, out[2 * r]);
        set_round_day(table + (uint32_t)r * ROUND_SIZE + 3, base, out[2 * r + 1]);
    }
    if (logging()) {
        int yo, doy;
        season_template_split(base, out[2 * rounds - 1], &yo, &doy);
        fprintf(stderr, "recomp: calendar: %s %d: %d rounds %s, last match day %d of %d\n", file, base, rounds,
                how == 1 ? "moved" : "pressed", doy, base + yo);
    }
}

/* IreChalCup::create_round_table (the FAI Cup): 28 November to 30 April */
void o_00634360(void);
RT_WRAP(00634360, fai_cup_rounds)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    o_00634360();
    if (!rt_unwinding) cup_dates(self, count_at);
}

/* IreMunsterCup +0x3c: 1 August to 9 May */
void o_00638a60(void);
RT_WRAP(00638a60, munster_cup_rounds)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    o_00638a60();
    if (!rt_unwinding) cup_dates(self, count_at);
}

/* IrePresCup +0x3c (the President's Cup): 5 July to 4 August */
void o_00639530(void);
RT_WRAP(00639530, pres_cup_rounds)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    o_00639530();
    if (!rt_unwinding) cup_dates(self, count_at);
}

/* IreLeinsterCup +0x3c: 1 August to 9 May */
void o_00636760(void);
RT_WRAP(00636760, leinster_cup_rounds)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    o_00636760();
    if (!rt_unwinding) cup_dates(self, count_at);
}

/* ---- The awards ---- */

uint32_t rt_ire_award_day(uint32_t original)
{
    int state = state_now(), day, month0;
    if (state == SEASON_CAL_WINTER) return original;
    season_calendar_award(state, &day, &month0);
    return (uint32_t)day;
}

uint32_t rt_ire_award_month(uint32_t original)
{
    int state = state_now(), day, month0;
    if (state == SEASON_CAL_WINTER) return original;
    season_calendar_award(state, &day, &month0);
    return (uint32_t)month0;
}

/* ---- The labels ---- */

/* key_nation_format_club_season 0x669630 and key_nation_format_comp_season 0x6697c0, cdecl (year, club or record,
 * text, size): "%d/%d" when the entry of the club's or competition's nation has the two-years flag (0x669684,
 * 0x669813), else "%d". After the move the seasons up to 2002/03 keep two years (decision 6): for such a year the
 * call sees Ireland's flag set. The flag of another nation's entry is not read, so the call is the same for it. */
static void with_two_years(void (*original)(void))
{
    int year = (int)(rt_arg(0) & 0xffffu);
    if (!season_calendar_two_years(state_now(), year) || rd32(g_mem, ENTRY_AT + E_TWO_YEARS)) {
        original();
        return;
    }
    wr32(g_mem, ENTRY_AT + E_TWO_YEARS, 1);
    original();
    wr32(g_mem, ENTRY_AT + E_TWO_YEARS, 0);
}

void o_00669630(void);
RT_WRAP(00669630, format_club_season) { with_two_years(o_00669630); }

void o_006697c0(void);
RT_WRAP(006697c0, format_comp_season) { with_two_years(o_006697c0); }

int season_calendar_history_label(uint32_t obj)
{
    if (!readable(obj)) return 0;
    int edition = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    return season_calendar_label(season_change_file(obj), state_of(edition), edition);
}

/* RECOMP_CALENDAR_LOG: the entry's fields at a save and after a load (season.c), for the save and load check. */
void season_calendar_log_entry(const char *when)
{
    if (!logging()) return;
    uint32_t p = ire_prm();
    fprintf(stderr, "recomp: calendar: entry at %s on day %d of %d: edition %d, update day %d, two years %u, start %d/%d, "
                    "end %d/%d, start year %d, end year %d\n", when, (int16_t)rd16(g_mem, GAME_DATE),
            (int16_t)rd16(g_mem, GAME_DATE + 2), p ? (int16_t)rd16(g_mem, p + COMP_YEAR) : 0,
            (int16_t)rd16(g_mem, ENTRY_AT + E_UPDATE), rd32(g_mem, ENTRY_AT + E_TWO_YEARS),
            (int16_t)rd16(g_mem, ENTRY_AT + E_START), (int16_t)rd16(g_mem, ENTRY_AT + E_START + 2),
            (int16_t)rd16(g_mem, ENTRY_AT + E_END), (int16_t)rd16(g_mem, ENTRY_AT + E_END + 2),
            (int16_t)rd16(g_mem, ENTRY_AT + E_START_PARTS + 3), (int16_t)rd16(g_mem, ENTRY_AT + E_END_PARTS + 3));
}

void season_calendar_log_history(uint32_t obj, int label)
{
    if (!logging() || !readable(obj)) return;
    const char *file = season_change_file(obj);
    if (!season_calendar_league(file) && !season_calendar_cup(file) && strncmp(file, "ire_", 4)) return;
    fprintf(stderr, "recomp: calendar: history row of %s edition %d: label %d on day %d of %d\n", file,
            (int16_t)rd16(g_mem, obj + COMP_YEAR), label, (int16_t)rd16(g_mem, GAME_DATE),
            (int16_t)rd16(g_mem, GAME_DATE + 2));
}

int season_calendar_edition(void)
{
    uint32_t p = ire_prm();
    return p ? (int16_t)rd16(g_mem, p + COMP_YEAR) : 0;
}
