/* Rule kind "cups" of the season change (season_change.h): the round formats of the domestic cups by season
 * (0.6.0 slice 21, docs/research/2026-10-17-cup-round-record.md).
 *
 * The cup classes of the original make their round table in create_round_table (a virtual of each class, called
 * with stage 0xff by set_up_rounds, vtable +0x8c): at a new game, in start_new_season, and again at times that
 * are not a season start (the FA Cup table was made anew by 1 June of its first season; a log of
 * every start_new_season showed none in between). So the kind does not write the table after a season
 * start. It wraps create_round_table of each cup class that has a row in the cups table and changes the table
 * that the function returns (0x68 bytes a round, five format fields, season_cups.h) before the caller stores it.
 * Whatever makes the table, it holds the format of the season:
 * - the season of a cup is its season year (cup+0x40, set before the table is made) with changing rules on,
 *   and the start year of the game with changing rules off (the rules stay, Q232);
 * - a season without a row, and a game that starts in 2001/02 with changing rules off, keep the original's
 *   rounds (the table has no row before 2002).
 * A saved game keeps its table in its cup file (Cup::save_cup_to_disk 0x5210f0 writes the rounds, Cup::
 * load_from_disk 0x51fc00 reads them without a call of create_round_table), so a load changes nothing.
 * The news sentence of a row comes in the after hook of the season change, once for the cup that starts that
 * season. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_cups.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUP_ROUND_COUNT 0x3cu       /* s16 */
#define CUP_YEAR 0x40u              /* s16 season year */
#define CUP_ROUND_TABLE 0xa3u       /* pointer to the rounds */
#define ROUND_SIZE 0x68u
#define ROUND_FIRST 0x0du           /* u16 */
#define ROUND_DECIDING 0x0fu        /* u16 */
#define ROUND_REPLAYS 0x20u
#define ROUND_LEGS 0x21u
#define ROUND_DAYS 0x22u

static int logging(void)
{
    const char *v = getenv("RECOMP_SEASON_CHANGE_LOG");
    return v && *v;
}

/* Test switch: RECOMP_CUPS=off leaves the cups as the original makes them (to find out whether a fault of a run
 * comes from this kind). */
static int cups_off(void)
{
    const char *v = getenv("RECOMP_CUPS");
    return v && !strcmp(v, "off");
}

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

/* Writes the format of the row to a table of n rounds. Returns the number of rounds that changed, or -1 when
 * the table has another round count than the row (nothing is written). A round that becomes two legs can end
 * after the draw date of the next round: Cup::draw_round 0x51c410 then finds a tie with no winner (0x51c47c..
 * 0x51c495), moves the draw date one day on (0x51c4e9..0x51c549) and keeps the later rounds after it
 * (0x51c54d..0x51c596), so the draw waits for the tie (slice 21b). */
static int apply_row(uint32_t table, int n, const SeasonCupRow *row)
{
    if (n != row->rounds || !readable(table) || !readable(table + (uint32_t)n * ROUND_SIZE)) return -1;
    int changed = 0;
    for (int i = 0; i < n; i++) {
        uint32_t at = table + (uint32_t)i * ROUND_SIZE;
        SeasonCupRound r = {(uint16_t)rd16(g_mem, at + ROUND_FIRST), (uint16_t)rd16(g_mem, at + ROUND_DECIDING),
                            g_mem[at + ROUND_REPLAYS], g_mem[at + ROUND_LEGS], g_mem[at + ROUND_DAYS]};
        if (!season_cups_apply(row->types[i], &r)) continue;
        wr16(g_mem, at + ROUND_FIRST, r.first);
        wr16(g_mem, at + ROUND_DECIDING, r.deciding);
        g_mem[at + ROUND_REPLAYS] = r.replays;
        g_mem[at + ROUND_LEGS] = r.legs;
        g_mem[at + ROUND_DAYS] = r.days;
        changed++;
    }
    return changed;
}

/* The season whose format a cup takes (module comment). */
static int cup_season(uint32_t cup)
{
    if (!season_changing_rules()) return season_start_year();
    int y = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    return y >= SEASON_FIRST_YEAR && y <= SEASON_LAST_YEAR ? y : season_start_year();
}

/* After create_round_table of a cup: self is the cup, stage and count_ptr are its first two arguments, the table
 * is the result (cpu.eax). want: the stage of the knockout table (0xff from set_up_rounds 0x4c15f0 for a Cup
 * class; 7 and 0x13 for the knockout stage of ItaCup and HolCup, slice 21b). */
static void table_made_stage(uint32_t self, uint32_t stage, uint32_t count_ptr, uint32_t want)
{
    if (stage != want || !season_code_on() || cups_off() || !readable(count_ptr)) return;
    const char *file = season_change_file(self);
    int season = cup_season(self);
    const SeasonCupRow *row = season_cups_row(file, season);
    if (!row) return;
    int changed = apply_row(cpu.eax, (int16_t)rd16(g_mem, count_ptr), row);
    if (logging())
        fprintf(stderr, "recomp: cups: %s (0x%x) season %d, row %d: %s %d\n", file, self, season, row->season,
                changed < 0 ? "not applied, another round count, rounds" : "rounds changed", changed < 0 ? 0 : changed);
}

static void table_made(uint32_t self, uint32_t stage, uint32_t count_ptr)
{
    table_made_stage(self, stage, count_ptr, 0xffu);
}

/* The news of a row: with changing rules on, the cup that starts the season of a row says so. A row is a change by
 * design (the table holds only seasons in which a format changes), so the sentence does not check the rounds. */
static void cups_after(const SeasonChange *c)
{
    if (!c->started || cups_off()) return;
    const SeasonCupRow *row = season_cups_row(c->file, c->season);
    if (!row || row->season != c->season) return;
    /* ItaCup and HolCup are League classes: their knockout stage is made later in the season (slice 21b). */
    int league_cup = !strcmp(c->file, "ita_cup") || !strcmp(c->file, "hol_cup");
    if (!league_cup && (int16_t)rd16(g_mem, c->comp + CUP_ROUND_COUNT) != row->rounds) return;
    season_change_news("%s", row->sentence);
}

const SeasonRuleKind season_kind_cups = {"cups", NULL, NULL, NULL, NULL, NULL, cups_after};

/* create_round_table of the cup classes that have a row in the cups table (thiscall: this, char stage, short
 * *round_count, short *first_id, int *out; ret 0x10). It is slot +0x3c of the class vtable (EngFaCup 0x969b28 calls
 * 0x56f320 at 0x56f13b through it). The vtables: EngFaCup 0x969b28, EngCcCup 0x969934, EngFaTrophy 0x969bc8,
 * ScoFaCup 0x96f414, BelFaCup 0x9678a0, FraCup 0x96a864, FraLgeCup 0x96aad8, NorCup 0x96dd80; each slot +0x3c holds
 * the address of its wrap below. The recompiler reads the RT_WRAP lines as they are, so each is written out. */
void o_0056f320(void);
RT_WRAP(0056f320, cups_eng_fa_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0056f320();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_0056c570(void);
RT_WRAP(0056c570, cups_eng_cc_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0056c570();
    if (!rt_unwinding) table_made(self, stage, count);
    if (!rt_unwinding) season_cup_groups_table(self, stage, count);     /* slice 21c: a group stage */
}

void o_00570ec0(void);
RT_WRAP(00570ec0, cups_eng_fa_trophy)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_00570ec0();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_007ed5d0(void);
RT_WRAP(007ed5d0, cups_sco_fa_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_007ed5d0();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_0041d310(void);
RT_WRAP(0041d310, cups_bel_fa_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0041d310();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_005ba3f0(void);
RT_WRAP(005ba3f0, cups_fra_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_005ba3f0();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_005bcc40(void);
RT_WRAP(005bcc40, cups_fra_lge_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_005bcc40();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_0078f2e0(void);
RT_WRAP(0078f2e0, cups_nor_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0078f2e0();
    if (!rt_unwinding) table_made(self, stage, count);
}

/* Slice 21b: the Spanish, Turkish and Russian cups (create_round_table, slot +0x3c, stage 0xff from
 * set_up_rounds: SpaCup 0x84d220 in vtable 0x96f9c8, TurCup 0x8fc670 in 0x97048c, RusCup 0x7e9530 in 0x96f124),
 * and the knockout stage of the Coppa Italia and the KNVB Cup. ItaCup and HolCup are League classes with a group
 * stage. Their create_schedule (vtable +0x3c: ItaCup 0x63ec50 in vtable 0x96c318, HolCup 0x5f36e0 in vtable
 * 0x96b7b8; thiscall: stage, short *round_count, short *first_id, int *out; ret 0x10) makes the round table of
 * the knockout stage for stage 7 (ItaCup, 5 rounds at 0x63ed91; called at 0x63f9c0 by create_knockout_stage
 * 0x63f870) and stage 0x13 (HolCup, 6 rounds at 0x5f381d; called at 0x5f4708 by create_knockout_stage
 * 0x5f4480). The table goes to a Cup_Stage object (constructor 0x522e00 at 0x63fa10 and 0x5f4755), which plays
 * it with the Cup code of the other classes, so the same five fields hold the format. The same stages are also
 * asked for by set_predicted_positions (ItaCup 0x640210 at 0x640332, HolCup 0x5f5120 at 0x5f5242), which sorts
 * the clubs by the table that the call returns: the wrap changes that table too (only the five format fields,
 * not the round count or the dates). */
void o_0084d220(void);
RT_WRAP(0084d220, cups_spa_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0084d220();
    if (!rt_unwinding) table_made(self, stage, count);
}

void o_008fc670(void);
RT_WRAP(008fc670, cups_tur_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_008fc670();
    if (!rt_unwinding) table_made(self, stage, count);
    if (!rt_unwinding) season_cup_groups_table(self, stage, count);     /* slice 21c: a group stage */
}

void o_007e9530(void);
RT_WRAP(007e9530, cups_rus_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_007e9530();
    if (!rt_unwinding) table_made(self, stage, count);
    if (!rt_unwinding) season_cup_groups_table(self, stage, count);     /* slice 21c: a group stage */
}

void o_0063ec50(void);
RT_WRAP(0063ec50, cups_ita_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0063ec50();
    if (!rt_unwinding) table_made_stage(self, stage, count, 7u);
}

void o_005f36e0(void);
RT_WRAP(005f36e0, cups_hol_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_005f36e0();
    if (!rt_unwinding) table_made_stage(self, stage, count, 0x13u);
}

/* ---- The Russian Cup's new edition after a late final (O151, gate (a) pre-run on 53fba30a, 2008 start) ----
 *
 * RusCup::update_for_date 0x7e9ec0 (vtable +0x14 of 0x96f124; thiscall: date, flag; ret 8) starts the next edition
 * (vtable +8, RusCup::start_new_season 0x7e9d10, at 0x7e9f12) only when the flag is set, all_stages_complete
 * (vtable +0xc, 0x7e9ed6) is true and the date is exactly 1 June of its year (0x549f70 with day 1, month 5 at
 * 0x7e9ee4..0x7e9f0c). The cup plays rounds 1 to 3 in June, September and October of year Y and rounds 4 to 6 from
 * March to 21 May of Y+1 (RusCup::create_round_table 0x7e9530). A final that the game moves past 1 June leaves the
 * edition complete one day too late: the next chance is then the season pass at Russia's entry (update day 360,
 * 0x5d5cc6), after the June, September and October rounds of the new edition. In a 2008 game (September 2008
 * database, Select All, changing rules on) the second semi-final of 2009/10 moved from 4 May (day 124, the day of
 * the European semi-finals) to day 145, the final from day 137 to day 152 (2 June), and the 2010 edition started
 * on day 360 of 2010; the league check found the cup at edition 2009 on 1 August 2010. In a season game the cup
 * now also starts its edition on the first update day after 1 June on which it is complete and still holds the
 * year before, up to the day before its first draw (23 June, 0x7e95f8). The base game keeps the original's one
 * day (Q152). */
#define RUS_CUP_VTABLE 0x0096f124u
#define COMP_YEAR_AT 0x40u

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static uint32_t call_vfunc(uint32_t self, uint32_t slot)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(rd32(g_mem, rd32(g_mem, self) + slot), 0, NULL);
    if (!rt_unwinding) cpu = saved;
    return r;
}

void o_007e9ec0(void);
RT_WRAP(007e9ec0, rus_cup_late_final)
{
    uint32_t self = cpu.ecx, date = rt_arg(0), flag = rt_arg(1);
    o_007e9ec0();
    if (rt_unwinding || !flag || !season_game_changes() || !readable(self) || !readable(date)) return;
    if (rd32(g_mem, self) != RUS_CUP_VTABLE) return;
    int day = (int16_t)rd16(g_mem, date), year = (int16_t)rd16(g_mem, date + 2);
    int june1 = 151 + leap(year), first_draw = 173 + leap(year);     /* day of the year from 0: 1 June, 23 June */
    if (day <= june1 || day >= first_draw || (int16_t)rd16(g_mem, self + COMP_YEAR_AT) >= year) return;
    uint32_t eax = cpu.eax;
    if (!(call_vfunc(self, 0xc) & 0xffu) || rt_unwinding) return;
    int before = (int16_t)rd16(g_mem, self + COMP_YEAR_AT);
    call_vfunc(self, 8);
    if (rt_unwinding) return;
    cpu.eax = eax;
    fprintf(stderr, "recomp: cups: rus_cup %d complete after 1 June: edition %d starts on day %d of %d\n", before,
            (int16_t)rd16(g_mem, self + COMP_YEAR_AT), day, year);
}
