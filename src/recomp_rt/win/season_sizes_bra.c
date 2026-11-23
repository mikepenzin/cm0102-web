/* Série A and Série B in two meetings with no play-off (0.7.2 league sizes, slice 17, D11; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).
 *
 * The classes. BraNatFirst and BraNatSecond play one meeting (+0x3c 1 at 0x42a5af and 0x42cb83) on 27 dates
 * (create_schedule 0x429b50 and 0x42c140, D 0x1b at 0x429b8d and 0x42c17d), then a play-off of the top 4 as stage 0
 * (advance_stage 0x42a950 and 0x42cdd0, create_play_off_stage 0x42a9e0 and 0x42ce60). The constructors set the stage
 * count +0x2c to 1 and +0x30 to -1 (0x4298be, 0x4298c1) before set_up_rules (0x4298d5). start_new_season 0x42a640
 * sets +0x30 to -1 (0x42a6f2) and calls set_up_rules (vtable +0x8c, 0x42a6f9), and never writes +0x2c.
 *
 * The real leagues. Série A plays a double round robin with no play-off from 2003 (row 2002: rows are keyed one year
 * before the real season), Série B from 2006 (row 2005). The grown rows of league_sizes.py give the meetings of the
 * row (+0x3c 2) and the dates of the rounds: the wrap of create_schedule runs inside set_up_rules (vtable +0x3c,
 * 0x42a629 and 0x42cbfd). When the template has no room the class's meetings come back (season_sizes.c, D4).
 *
 * So after set_up_rules: with 2 meetings the league has no stage. The stage count goes to 0, as the 3. Liga in the
 * German Regionalliga class (season_divisions.c, ger_regional_south): with a count of 1 League::update_for_date
 * 0x6847c0 calls advance_stage when the table is complete and the play-off is played; with 0 the season ends as a
 * league's with no stages (the history, the honours and the complete flag). The destructor 0x429a90 and
 * start_new_season read the stage slots (+0xc) only while +0x30 >= 0 (0x429ae0..0x429aff, 0x42a6c8..0x42a6e8). The
 * save keeps +0x2c 0, and League::load 0x687b10 then makes no stage array (0x688e49); nothing reads +0xc then.
 * Série A also loses its 4 play-off places (+0xbe 4 at 0x42a5db): with no division above, the top 4 rows got the
 * play-off status (League::update_positions_and_status 0x684cd0 reads +0xbe at 0x684db0) and the news that they
 * qualify for the play-offs (BraNatFirst::build_fate_news_text 0x42ac70, result 6). A top league with no play-off
 * writes 0 there (EngPrm 0x574bb8, SpaFirst 0x84fe12). Série B keeps its places: they move its clubs up.
 *
 * The champion. update_positions_and_status gives the top row status 5 and the result code 5 only with bit 1 of the
 * rule flags +0xd9 (0x684dc0, 0x6850f2..0x685159); League::League sets 3 (0x6822a7), SpaFirst 0x43. The classes set
 * 8 and 2 (0x42a618, 0x42cbd8): their champion is the play-off winner (code 1 in stage 0, 0x42ac1c, row +0x37 5).
 * With no play-off the flags get bit 1, and Série A loses bit 8 (status 6 for the +0xbe rows, 0x6851dd..0x6851ff),
 * so the club on top of the table is the champion (D2). The result handlers (vtable +0x44, BraNatFirst 0x42abc0,
 * BraNatSecond 0x42d040) drop code 5 in the league stage (stage -1: codes 6 and 3 at 0x42abcf, 0x42abf6; codes 0
 * and 3 at 0x42d04f, 0x42d06f), so the wraps below hand code 5 to the base League handler 0x686940, which writes the
 * manager's title record (0x868c50 at 0x686957), as the classes do for their play-off winner (0x42ac3d).
 *
 * With 1 meeting (the base game, the rows of 2001 and the Série B rows 2002 to 2004, or no room for the dates) the
 * class plays its play-off as the original. The base game and RECOMP_LEAGUE_SIZES=off never change a league here
 * (season_sizes_rules calls season_sizes_bra_rules only with the size engine on, and the wraps below act only on a
 * Série A or B object whose stage count that function set to 0). */
#include "win.h"
#include "../rt_host.h"
#include "season_change.h"
#include "season_playoffs.h"
#include "season_sizes.h"
#include "season_sizes_arg_second.h"
#include "season_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_STAGES 0x0cu           /* the stage array */
#define COMP_STAGE_COUNT 0x2cu      /* s32: the stages after the league stage */
#define COMP_STAGE_NOW 0x30u        /* s32: the stage being played, -1 for the league stage */
#define COMP_MEETINGS 0x3cu         /* s16: meetings of each pair */
#define COMP_YEAR 0x40u             /* s16: the edition */
#define UP 0xbeu                    /* s8: the top places (the play-off places of Série A) */
#define SERIE_A_PLAYOFF_PLACES 4    /* 0x42a5ad, 0x42a5db */
#define RULE_FLAGS 0xd9u            /* u16: bit 1 a champion row, 2 promotion, 8 play-off rows (0x684dc0, 0x685191) */
#define FLAG_CHAMPION 0x01u
#define FLAG_PLAYOFF_ROWS 0x08u
#define F_BASE_RESULT 0x00686940u   /* League +0x44: code 5 the title record, 0 promotion, 3 relegation */
#define F_LEAGUE_HONOURS 0x00684640u /* League::record_season_honours (League vtable 0x96d020 +0x30), thiscall () */
#define STATUS_CHAMPION 5

static int readable(uint32_t p) { return rt_readable(p); }

#define UP_PLAYOFF 0xbfu            /* s8: promotion play-off places (status 1, 0x685204..0x685213) */
enum { PO_CLASS = 0, PO_TOP8 = 1, PO_TIES = 2 };
static int playoff_kind(uint32_t self);
static int playoff_now(uint32_t self);

void season_sizes_bra_rules(uint32_t obj, const char *file)
{
    if (!readable(obj) || !file) return;
    int first = !strcmp(file, "bra_nat_first");
    if (!first && strcmp(file, "bra_nat_second")) return;
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) >= 0) return;    /* a stage is live: set_up_rules mid-season */
    int meetings = (int16_t)rd16(g_mem, obj + COMP_MEETINGS);
    int32_t count = (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT);
    /* The Série B play-offs of the rules table (below): with the stage array, its places and stage count 1. */
    int kind = first ? PO_CLASS : playoff_kind(obj);
    if (kind != PO_CLASS && !readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = season_sizes_game_malloc(4);     /* a loaded game of two meetings has none (above) */
        if (rt_unwinding) return;
        if (readable(arr)) {
            wr32(g_mem, arr, 0);
            wr32(g_mem, obj + COMP_STAGES, arr);
        }
    }
    if (kind != PO_CLASS && readable(rd32(g_mem, obj + COMP_STAGES))) {
        wr32(g_mem, obj + COMP_STAGE_COUNT, 1);
        g_mem[obj + UP] = kind == PO_TOP8 ? 0 : 2;
        g_mem[obj + UP_PLAYOFF] = kind == PO_TOP8 ? 8 : 4;
        uint16_t flags = rd16(g_mem, obj + RULE_FLAGS);
        wr16(g_mem, obj + RULE_FLAGS, kind == PO_TIES ? flags | FLAG_CHAMPION : flags & (uint16_t)~FLAG_CHAMPION);
        if (getenv("RECOMP_SIZE_LOG"))
            fprintf(stderr, "recomp: league sizes: %s %d: %d meetings, %s\n", file,
                    (int16_t)rd16(g_mem, obj + COMP_YEAR), meetings,
                    kind == PO_TOP8 ? "play-off of the top 8, the two finalists go up"
                                    : "2 up, play-off of 3rd to 6th, the two winners go up");
        return;
    }
    if (meetings >= 2) {
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        if (first && (int8_t)g_mem[obj + UP] == SERIE_A_PLAYOFF_PLACES) g_mem[obj + UP] = 0;
        uint16_t flags = rd16(g_mem, obj + RULE_FLAGS) | FLAG_CHAMPION;
        if (first) flags &= (uint16_t)~FLAG_PLAYOFF_ROWS;
        wr16(g_mem, obj + RULE_FLAGS, flags);
        if (count != 0 && getenv("RECOMP_SIZE_LOG"))
            fprintf(stderr, "recomp: league sizes: %s %d: %d meetings, no play-off\n", file,
                    (int16_t)rd16(g_mem, obj + COMP_YEAR), meetings);
        return;
    }
    /* One meeting after a season of two (no room for the dates): the play-off again, when the stage array is there
     * (a loaded game of a two-meeting season has none, League::load 0x688e49, and keeps the table's end). The
     * original's set_up_rules writes +0xbe 4 again. */
    if (count == 0 && readable(rd32(g_mem, obj + COMP_STAGES))) wr32(g_mem, obj + COMP_STAGE_COUNT, 1);
}

/* The dates. The classes play Sundays and Wednesdays from the first Sunday of August to early December (27 dates,
 * 0x429b50 and 0x42c140), with no gap over 7 days, so its 8 gaps of a week take the first new dates
 * (season_template.c). The real seasons begin in late March to May (Série A 2003 on 29 March, Série B 2006 in
 * April), so the earlier starts go back up to 13 weeks, to early May: Série A of 24 clubs (46 rounds) takes 11,
 * Série A and B of 20 (38 rounds) 3 (lead decision, slice 17). */
#define BRA_EARLIER 13

int season_sizes_bra_earlier(const char *file)
{
    return file && (!strcmp(file, "bra_nat_first") || !strcmp(file, "bra_nat_second")) ? BRA_EARLIER
                                                                                        : SEASON_TEMPLATE_EARLIER;
}

/* 1 for a Série A or B object that plays no play-off this season: its stage count is 0 (season_sizes_bra_rules). The
 * meetings are not read: a game loaded in a season of two meetings has no stage array (League::load 0x688e49), and
 * when the next season has no room for the dates the class's 1 meeting comes back with the count still 0
 * (season_sizes_bra_rules). The original 0x408eb0 would then jump through the absent stage 0. */
static int no_playoff(uint32_t obj)
{
    if (!readable(obj) || rd32(g_mem, obj + COMP_STAGE_COUNT) != 0) return 0;
    const char *file = season_change_file(obj);
    return file && (!strcmp(file, "bra_nat_first") || !strcmp(file, "bra_nat_second"));
}

/* The result handler (thiscall: club, code, stage, a4, a5, a6; ret 0x18): code 5 of the league stage goes to the base
 * League handler, the rest to the class's. */
static void result(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    if ((rt_arg(1) & 0xff) != STATUS_CHAMPION || (rt_arg(2) & 0xff) != 0xff ||
        !(no_playoff(self) || playoff_now(self) == PO_TIES)) {
        original();
        return;
    }
    uint32_t a[6];
    for (int i = 0; i < 6; i++) a[i] = rt_arg(i);
    Cpu saved = cpu;
    cpu.ecx = self;
    rt_callback(F_BASE_RESULT, 6, a);
    if (rt_unwinding) return;
    cpu = saved;
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: %s %d: champion club %.40s (the top of the table)\n",
                season_change_file(self), (int16_t)rd16(g_mem, self + COMP_YEAR),
                readable(a[0]) ? win_str(a[0] + 4) : "?");
    rt_ret_stdcall(0, 6);
}

void o_0042abc0(void);
RT_WRAP(0042abc0, bra_first_result) { result(o_0042abc0); }

/* The Série B play-offs of the rules table (gap rows 56 and 57, open item 249; real life wins over D2 and D11).
 *
 * The class's play-off. BraNatSecond::advance_stage 0x42cdd0 sets +0x30 to 0 and calls create_play_off_stage
 * 0x42ce60 (0x42ce4b..0x42ce53). That takes rows 0, 3, 1 and 2 of the table (0x42cef3..0x42cf27), gets the round
 * table of stage 0 from create_schedule (vtable +0x3c, 0x42cf35: 2 rounds of 0x68 bytes, 0x42c8c4; the semi-finals
 * with round id 0x82, fixed pairs 0x200, 2 legs 4 days apart, 0x42c96c..0x42c9b7, and the final 0x96, 0x42c9eb..
 * 0x42ca55), builds the Cup_Stage (0x522e00 at 0x42cf79: parent, 4 clubs, the list, the round count, [this+4], the
 * round table, the year, 0, 1, the second count of create_schedule, 0x14, 0, 0, 0, 0), stores it as stage 0
 * (0x42cf91), frees the round table (0x42cf93), sets up round 0 (Cup::set_up_round 0x51c800 at 0x42d010), frees the
 * list and sets +0x30 to 0 (0x42d01e). Its places: 4 up (+0xbe 4 at 0x42cbae, +0xbf 0 at 0x42cbb5), so the play-off
 * decides the title only (code 1 in stage 0: status 5 and the title record 0x868c50, 0x42d098..0x42d0ba).
 *
 * The real play-offs:
 * - Rows 2001 to 2004 (Série B 2002 to 2005): the top 8 play for 2 places with no direct place. 2002: two-legged
 *   quarter-finals, semi-finals and final, the finalists promoted. 2003 to 2005: two groups of 4 and a final group
 *   of 4, its top 2 promoted. The game plays the 2002 form in all four seasons (no league class here plays groups
 *   after its table, D2: the nearest form): 1 v 8, 4 v 5, 2 v 7, 3 v 6, the winners of the first two and of the
 *   last two meet (fixed pairs). Places: +0xbe 0, +0xbf 8, so League::update_positions_and_status 0x684cd0 gives
 *   the 8 rows status 1, the play-off status (0x685204..0x685213), and none moves up. The final's winner gets code 1
 *   (status 5 and the title record, the class's own code) and its loser code 2 (BraNatSecond::build_fate_news_text
 *   0x42d0d0: "loses its final", 0x42d1eb); the wrap gives the loser status 0 and both the promotion record.
 * - Rows 2025 on (Série B 2026 on): the champion and the runner-up go up, then 3rd v 6th and 4th v 5th in two legs,
 *   both winners go up. The stage is one round of the class's semi-finals (round count 1). A round of two ties as
 *   the last round gives code 1 to each tie's winner: the I liga's play-off of 3rd and 4th against the Ekstraklasa's
 *   13th and 14th is one such round (PolSecond::create_schedule 0x7cb4a0, one round with 2 ties at 0x7cbdc1..
 *   0x7cbdc5), and PolSecond's handler 0x7cc490 gives each tie's code-1 club status 0 and the promotion record
 *   (0x7cc58a..0x7cc5bc). Places: +0xbe 2, +0xbf 4 (status 1 for rows 3 to 6). The champion is the top of the table
 *   (bit 1 of the rule flags, as with no play-off), so the honours are the League's and code 1 of the play-off writes
 *   no title record: the wrap gives status 0 and the promotion record. Its news is the league stage's promotion
 *   text (code 0 of stage -1, BraNatSecond::build_fate_news_text to Comp 0x48c6d0, 0x42d0dd..0x42d10b).
 *
 * Status 0 and 5 move up (league_promote_and_relegate 0x689c80, 0x689d47..0x689d4e), so each play-off sends 2 clubs
 * up. The dates start from the day the
 * table is complete. The play-off must end before the season change of the league's entry: the Brazilian entry
 * changes late in the year, after 1 October, where season_playoffs_hold does not wait (LATE_DAY), and the change
 * runs only when every stage is complete (Comp::all_stages_complete 0x48ce10): a match the game moves past the update
 * day would keep the whole Brazilian chain in its old season for a year (the Segunda's final of 2012,
 * season_playoffs.c). So the play-off needs 5 days spare before the update day. With fewer days left, or a missing
 * club, the places go up directly (the nearest form, 0.6.0 Q247): rows 0 and 1, or 2 and 3. A game loaded in a
 * season of two meetings has no stage array (League::load makes it only for +0x2c > 0, 0x688e3a..0x688e5b), so
 * season_sizes_bra_rules makes one of one empty slot, as season_playoffs_rules does. */
#define ROW_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define COMP_CLUBS 0x3eu
#define COMP_RECORD 4u
#define STATUS_UP 0
#define F_PROMOTION_RECORD 0x00869480u /* Staff_Records::add_promotion_record, thiscall (club, manager, 0x64) */
#define STAFF_RECORDS 0x00dd7f00u   /* the Staff_Records object (0x42d05e) */
#define F_CUP_STAGE 0x00522e00u     /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u  /* Cup::set_up_round, thiscall (round) */
#define F_DRAW_DATE 0x00521e60u     /* cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u    /* cdecl (table, round, day, month, year+, weekday, a7, year, venue) */
#define F_NEW 0x00944cf1u           /* operator new, cdecl (size) */
#define NO_WEEKDAY 0xffffffffu      /* Date::Date 0x549f70 keeps the date for weekday -1 */
#define GAME_DATE 0x00ae2c90u       /* s16 day of the year, +2 s16 year */
#define ENTRIES 0x00b63d60u         /* the league table entries, 35 x 0x48 */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define ENTRY_UPDATE_DAY 0x15u
#define COMPS_MAX 256
#define ROUND_SIZE 0x68u
#define RD_ID 0x07u                 /* s16 round id: 0x78 quarter-finals, 0x82 semi-finals, 0x96 final */
#define RD_INDEX 0x09u
#define RD_TEAMS 0x18u
#define RD_TIES 0x1au
#define RD_NEW 0x1cu                /* new entrants of the round */
#define RD_FIRST 0x1eu
#define RD_LEG_DAYS 0x22u           /* days from the first leg to the second */
#define CODE_WON 1                  /* the final's winner, or each tie's winner of a last round of 2 ties */
#define CODE_FINAL_LOST 2           /* the final's loser (0x42d1eb) */
#define TOP8_CLUBS 8
#define TOP8_ROUNDS 3
#define TOP8_DAYS 23                /* the final's second leg on day 17, and 5 days spare (below) */
#define TIES_CLUBS 4
#define TIES_DAYS 13                /* the second legs on day 7, and 5 days spare */

/* The play-off of a Série B object of the size engine by its row, or PO_CLASS. */
static int playoff_kind(uint32_t self)
{
    if (!season_sizes_on() || !readable(self)) return PO_CLASS;
    const char *file = season_change_file(self);
    if (!file || strcmp(file, "bra_nat_second")) return PO_CLASS;
    int s = season_sizes_row_season(self, file);
    if (s < 0) return PO_CLASS;
    if (season_playoffs_finalists_up(file, s) == 2) return PO_TOP8;
    if (season_playoffs_winners_up(file, s) == 2) return PO_TIES;
    return PO_CLASS;
}

/* The kind with its stage this season: the stage count 1 (season_sizes_bra_rules keeps it only with a stage array). */
static int playoff_now(uint32_t self)
{
    int k = playoff_kind(self);
    return k != PO_CLASS && (int32_t)rd32(g_mem, self + COMP_STAGE_COUNT) == 1 && g_mem[self + UP_PLAYOFF] ? k
                                                                                                            : PO_CLASS;
}

static uint32_t row_club(uint32_t self, int i)
{
    uint32_t table = rd32(g_mem, self + ROW_TABLE);
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    return readable(table) && i >= 0 && i < n ? rd32(g_mem, table + (uint32_t)i * ROW_SIZE) : 0;
}

static int row_of_club(uint32_t self, uint32_t club)
{
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    for (int i = 0; i < n && i < 200; i++)
        if (row_club(self, i) == club) return i;
    return -1;
}

static void promotion_record(uint32_t self, uint32_t club)
{
    if (!readable(club)) return;
    uint32_t a[3] = {club, rd32(g_mem, self + 4), 0x64u};
    Cpu saved = cpu;
    cpu.ecx = rd32(g_mem, STAFF_RECORDS);
    rt_callback(F_PROMOTION_RECORD, 3, a);
    if (rt_unwinding) return;
    cpu = saved;
}

/* A play-off club goes up: status 0 and the promotion record. */
static void goes_up(uint32_t self, uint32_t club, const char *why)
{
    int i = row_of_club(self, club);
    if (i < 0) return;
    g_mem[rd32(g_mem, self + ROW_TABLE) + (uint32_t)i * ROW_SIZE + ROW_STATUS] = STATUS_UP;
    promotion_record(self, club);
    if (!rt_unwinding && getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: bra_nat_second %d: place %d (%.40s) goes up, %s\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), i + 1, win_str(club + 4), why);
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int year_days(int y) { return leap(y) ? 366 : 365; }

/* The days from today to the update day of the league's table entry, or -1 when not found. */
static int days_to_change(uint32_t obj)
{
    uint32_t buf[COMPS_MAX];
    for (int e = 0; e < ENTRY_COUNT; e++) {
        int n = season_change_entry_comps(e, buf, COMPS_MAX);
        for (int i = 0; i < n; i++) {
            if (buf[i] != obj) continue;
            int upd = (int16_t)rd16(g_mem, ENTRIES + (uint32_t)e * ENTRY_SIZE + ENTRY_UPDATE_DAY);
            int today = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
            if (upd < 0) return -1;
            int left = upd - today;
            if (left < 0) left += year_days(y);
            return left;
        }
    }
    return -1;
}

/* Round r of table: drawn on day draw_in and first legs on day match_in from today (as season_playoffs.c). */
static void set_dates(uint32_t table, int round, int draw_in, int match_in, uint32_t year)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int when[2] = {draw_in, match_in};
    for (int k = 0; k < 2 && !rt_unwinding; k++) {
        int d = (int16_t)rd16(g_mem, GAME_DATE) + when[k], y = (int16_t)rd16(g_mem, GAME_DATE + 2), m = 0;
        while (d >= year_days(y)) d -= year_days(y), y++;
        while (m < 11 && d >= len[m] + (m == 1 && leap(y))) d -= len[m] + (m == 1 && leap(y)), m++;
        if (!k) {
            uint32_t a[7] = {table, (uint32_t)round, (uint32_t)(d + 1), (uint32_t)m, (uint32_t)(y - (int)year),
                             NO_WEEKDAY, year};
            call_cdecl(F_DRAW_DATE, 7, a);
        } else {
            /* a7 2 and venue 0 as the class's own dates (0x42c95a..0x42c967). */
            uint32_t a[9] = {table, (uint32_t)round, (uint32_t)(d + 1), (uint32_t)m, (uint32_t)(y - (int)year),
                             NO_WEEKDAY, 2, year, 0};
            call_cdecl(F_MATCH_DATE, 9, a);
        }
    }
}

/* No play-off: the places go up directly and the season closes with no stage (stage count 0 and +0x30 -1, so the
 * season ends as one of two meetings, and the honours are the League's, 0x408eb0 below). */
static void direct_places(uint32_t self, int first, const char *why)
{
    fprintf(stderr, "recomp: league sizes: bra_nat_second %d: %s; places %d and %d go up\n",
            (int16_t)rd16(g_mem, self + COMP_YEAR), why, first + 1, first + 2);
    wr32(g_mem, self + COMP_STAGE_COUNT, 0);
    wr32(g_mem, self + COMP_STAGE_NOW, 0xffffffffu);
    goes_up(self, row_club(self, first), "no play-off");
    if (!rt_unwinding) goes_up(self, row_club(self, first + 1), "no play-off");
}

/* The play-off stage of kind k, as create_play_off_stage 0x42ce60 builds the class's. */
static void build_playoff(uint32_t self, int k)
{
    int top8 = k == PO_TOP8, first = top8 ? 0 : 2, n = top8 ? TOP8_CLUBS : TIES_CLUBS;
    static const int order8[TOP8_CLUBS] = {0, 7, 3, 4, 1, 6, 2, 5};
    static const int order4[TIES_CLUBS] = {2, 5, 3, 4};
    const int *order = top8 ? order8 : order4;
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    uint32_t clubs[TOP8_CLUBS] = {0};
    for (int i = 0; i < n; i++)
        if (!readable(clubs[i] = row_club(self, order[i]))) {
            direct_places(self, first, "a play-off club is missing");
            return;
        }
    int left = days_to_change(self);
    if (left < (top8 ? TOP8_DAYS : TIES_DAYS)) {
        char why[80];
        snprintf(why, sizeof why, "%d days to the season change, no time for the play-off", left);
        direct_places(self, first, why);
        return;
    }
    /* The class's round table of stage 0 (vtable +0x3c: stage, &rounds, &second count, NULL). */
    uint32_t outs = season_sizes_game_malloc(8);
    if (!outs) {
        if (!rt_unwinding) direct_places(self, first, "no memory");
        return;
    }
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t vt = rd32(g_mem, self);
    uint32_t got = call_this(rd32(g_mem, vt + 0x3c), self, 4, ra);
    int rounds = (int16_t)rd16(g_mem, outs);
    uint32_t second = rd16(g_mem, outs + 4);
    season_sizes_game_free(outs);
    if (rt_unwinding) return;
    if (!readable(got) || rounds != 2) {
        season_sizes_game_free(got);
        direct_places(self, first, "no round table");
        return;
    }
    uint32_t table = got;
    if (top8) {
        /* Quarter-finals, semi-finals and final: the class's semi-finals twice and its final. */
        table = season_sizes_game_malloc(TOP8_ROUNDS * ROUND_SIZE);
        if (!table) {
            season_sizes_game_free(got);
            if (!rt_unwinding) direct_places(self, first, "no memory");
            return;
        }
        memcpy(g_mem + table, g_mem + got, ROUND_SIZE);
        memcpy(g_mem + table + ROUND_SIZE, g_mem + got, ROUND_SIZE);
        memcpy(g_mem + table + 2 * ROUND_SIZE, g_mem + got + ROUND_SIZE, ROUND_SIZE);
        season_sizes_game_free(got);
        static const int ids[TOP8_ROUNDS] = {0x78, 0x82, 0x96}, teams[TOP8_ROUNDS] = {8, 4, 2};
        for (int r = 0; r < TOP8_ROUNDS; r++) {
            uint32_t at = table + (uint32_t)r * ROUND_SIZE;
            wr16(g_mem, at + RD_ID, (uint16_t)ids[r]);
            wr16(g_mem, at + RD_INDEX, (uint16_t)r);
            wr16(g_mem, at + RD_TEAMS, (uint16_t)teams[r]);
            wr16(g_mem, at + RD_TIES, (uint16_t)(teams[r] / 2));
            wr16(g_mem, at + RD_NEW, (uint16_t)(r ? 0 : TOP8_CLUBS));
            wr16(g_mem, at + RD_FIRST, 0);
            g_mem[at + RD_LEG_DAYS] = 3;
        }
        set_dates(table, 0, 1, 2, year);            /* legs on days 2 and 5 */
        if (!rt_unwinding) set_dates(table, 1, 6, 8, year);    /* days 8 and 11 */
        if (!rt_unwinding) set_dates(table, 2, 12, 14, year);  /* days 14 and 17 */
        rounds = TOP8_ROUNDS;
    } else {
        set_dates(table, 0, 1, 3, year);            /* the class's semi-finals alone: legs on days 3 and 7 */
        rounds = 1;
    }
    uint32_t list = rt_unwinding ? 0 : season_sizes_game_malloc(4u * (uint32_t)n);
    if (!list) {
        season_sizes_game_free(table);
        if (!rt_unwinding) direct_places(self, first, "no memory");
        return;
    }
    for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        season_sizes_game_free(list);
        season_sizes_game_free(table);
        if (!rt_unwinding) direct_places(self, first, "no memory");
        return;
    }
    uint32_t args[15] = {self, (uint32_t)n, list, (uint32_t)rounds, rd32(g_mem, self + COMP_RECORD), table, year, 0,
                         1, second, 0x14, 0, 0, 0, 0};
    call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, self + COMP_STAGES), stage);
    season_sizes_game_free(table);
    season_sizes_game_free(list);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, self + COMP_STAGE_NOW, 0);
    fprintf(stderr, "recomp: league sizes: bra_nat_second %u: play-off of places %d to %d drawn on day %d, %s\n", year,
            first + 1, first + n, (int16_t)rd16(g_mem, GAME_DATE),
            top8 ? "the two finalists go up" : "3rd v 6th and 4th v 5th, the two winners go up");
}

void o_0042ce60(void);
RT_WRAP(0042ce60, bra_second_playoff)
{
    int k = playoff_now(cpu.ecx);
    if (k == PO_CLASS || !readable(rd32(g_mem, cpu.ecx + COMP_STAGES))) {
        o_0042ce60();
        return;
    }
    build_playoff(cpu.ecx, k);
    if (!rt_unwinding) rt_ret_stdcall(0, 0);
}

void o_0042d040(void);
RT_WRAP(0042d040, bra_second_result)
{
    uint32_t self = cpu.ecx, club = rt_arg(0);
    int code = rt_arg(1) & 0xff, stage = rt_arg(2) & 0xff;
    int k = stage == 0 ? playoff_now(self) : PO_CLASS;
    if (k == PO_TIES && code == CODE_WON) {
        goes_up(self, club, "the winner of its play-off tie");
        if (!rt_unwinding) rt_ret_stdcall(0, 6);
        return;
    }
    result(o_0042d040);
    if (rt_unwinding || k != PO_TOP8) return;
    if (code == CODE_WON) {
        promotion_record(self, club);
        if (!rt_unwinding && getenv("RECOMP_SIZE_LOG"))
            fprintf(stderr, "recomp: league sizes: bra_nat_second %d: %.40s wins the play-off final and goes up\n",
                    (int16_t)rd16(g_mem, self + COMP_YEAR), readable(club) ? win_str(club + 4) : "?");
    } else if (code == CODE_FINAL_LOST) {
        goes_up(self, club, "the loser of the play-off final");
    }
}

/* The news (thiscall: club index, result, stage, 7 more; ret 0x28). The ties of rows 2025 on: the winners get the
 * league stage's promotion text (code 0 of stage -1, 0x42d0dd..0x42d10b), the losers no text. */
void o_0042d0d0(void);
RT_WRAP(0042d0d0, bra_second_news)
{
    int code = rt_arg(1) & 0xff, stage = rt_arg(2) & 0xff;
    if (stage != 0 || playoff_now(cpu.ecx) != PO_TIES) {
        o_0042d0d0();
        return;
    }
    if (code != CODE_WON) {
        rt_ret_stdcall(0, 10);
        return;
    }
    wr32(g_mem, cpu.esp + 4 + 4 * 1, STATUS_UP);
    wr32(g_mem, cpu.esp + 4 + 4 * 2, 0xffffffffu);
    o_0042d0d0();
}

/* The honours. League::update_for_date 0x6847c0 calls vtable +0x30 when the season is complete (0x684a8f). The
 * Brazilian classes hold 0x408eb0 there, which jumps to +0x30 of stage 0 (0x408eb0..0x408eb7: [this+0xc][0]), the
 * play-off; the base League holds record_season_honours 0x684640, which takes rows 0 and 1 of the table +0xb1
 * (0x68465b..0x68465d). With no play-off stage 0 is not there (a jump to 0), so a Série A or B season with no
 * play-off takes the League's, as the OFC Nations Cup with no stage (season_intl_ofc.c). Série B from row 2025 has
 * a play-off for places only, so its honours are the League's too. So does an edition of one table of the Argentine
 * second division (ArgSecond holds 0x408eb0 too, slice 16). Other classes that hold 0x408eb0 keep it. */
void o_00408eb0(void);
RT_WRAP(00408eb0, staged_honours)
{
    if (no_playoff(cpu.ecx) || season_arg_second_no_stage(cpu.ecx)    /* slice 16: one table, no stage */
        || playoff_now(cpu.ecx) == PO_TIES) {                          /* the champion is the top of the table */
        rt_jump(F_LEAGUE_HONOURS);
        return;
    }
    o_00408eb0();
}
