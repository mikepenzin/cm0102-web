/* Faults of the original that long games reach (0.6.2 slice 11, tools/recomp/original_faults.py). The fixes act in
 * every game, the base game too (Q23, Q24, Q27). The test switch RECOMP_ORIGINAL_FAULTS turns them off: "keep" all
 * of them, "keep:" with a list of groups some of them ("keep:money,age"). The groups are managers, money, age,
 * olympics, ire_super, nsl, holiday, callups, wc_pots, confed, club_regulars, match_numbers, squad_moves, squad_loans,
 * club_keys and callup_counts. */
#include "win.h"
#include "season.h"
#include "season_intl.h"
#include "season_names.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../rt_host.h"

#define CLUB_COUNT 0xae235cu     /* clubs, national teams and under-21 teams */
#define NATION_COUNT 0xae2350u

enum { MANAGERS, MONEY, AGE, OLYMPICS, IRE_SUPER, NSL, HOLIDAY, CALLUPS, WC_POTS, CONFED, CLUB_REGULARS, MATCH_NUMBERS,
       SQUAD_MOVES, SQUAD_LOANS, CLUB_KEYS, CALLUP_COUNTS, GROUPS };

static int fixes_on_group(int group)
{
    static int on[GROUPS] = {-1};
    if (on[0] < 0) {
        static const char *const names[GROUPS] = {"managers", "money", "age", "olympics", "ire_super", "nsl",
                                                   "holiday", "callups", "wc_pots", "confed", "club_regulars",
                                                   "match_numbers", "squad_moves", "squad_loans", "club_keys",
                                                   "callup_counts"};
        const char *v = getenv("RECOMP_ORIGINAL_FAULTS");
        if (v && strcmp(v, "keep") && strncmp(v, "keep:", 5))
            fprintf(stderr, "recomp: RECOMP_ORIGINAL_FAULTS=%s is not keep or keep:<groups>: every fix acts\n", v);
        const char *list = v && !strncmp(v, "keep:", 5) ? v + 5 : "";
        for (const char *p = list; *p; p += strcspn(p, ",") + (p[strcspn(p, ",")] == ',')) {
            size_t n = strcspn(p, ",");
            int known = 0;
            for (int g = 0; g < GROUPS; g++) known |= n == strlen(names[g]) && !strncmp(p, names[g], n);
            if (n && !known)        /* a typo would leave its fixes on, and a same-save run would still pass */
                fprintf(stderr,
                        "recomp: RECOMP_ORIGINAL_FAULTS: unknown group %.*s (managers, money, age, olympics, "
                        "ire_super, nsl, holiday, callups, wc_pots, confed, club_regulars, match_numbers, "
                        "squad_moves, squad_loans, club_keys, callup_counts)\n",
                        (int)n, p);
        }
        for (int g = 0; g < GROUPS; g++) {
            on[g] = 1;
            if (v && !strcmp(v, "keep")) on[g] = 0;
            if (v && !strncmp(v, "keep:", 5)) {
                size_t n = strlen(names[g]);
                for (const char *p = v + 5; *p; p += strcspn(p, ",") + (p[strcspn(p, ",")] == ',')) {
                    if (strcspn(p, ",") == n && !strncmp(p, names[g], n)) on[g] = 0;
                }
            }
        }
    }
    return on[group];
}

/* The background matches' own numbers (item 155, match_numbers.c). */
int win_match_numbers_fix_on(void)
{
    return fixes_on_group(MATCH_NUMBERS);
}

static int money_fixes_on(void)    /* the fixes of money above 2,147,483,647 (faults 27 and 28) */
{
    return fixes_on_group(MONEY);
}

/* A name of the game for a log line, with each byte above 0x7e as '?': the test scripts read the log as UTF-8, and
 * the game's names are Latin-1 ("S\xe3o Tom\xe9"). */
static const char *log_name(uint32_t at)
{
    static char out[64];
    size_t i = 0;
    for (const char *p = win_str(at); *p && i < sizeof out - 1; p++)
        out[i++] = (unsigned char)*p > 0x7e ? '?' : *p;
    out[i] = 0;
    return out;
}

/* `game_run_to_start_date` 0x5cc850 (called from create_game at 0x829fd9) plays the days of a new game up to its
 * start date, across a change of month. The fix stays off there, so a new game starts as the original's does (about
 * 95 of 213 national teams of the 3.9.68 data have no manager then) and changes only at the first month change in
 * play (Q152: the same-save set ends before it). */
static int creating;

void o_005cc850(void);

RT_WRAP(005cc850, run_to_start_date)
{
    creating = 1;
    o_005cc850();
    creating = 0;
}

/* 1 when a vacancy opens for this club although `club_can_be_managed` 0x544820 refuses it: a senior national team,
 * by the club id range of 0x544835..0x544857, with a nation (+0x53, 0x54482e) that exists. The original asks for more
 * than 150 players of the nation (0x544859), or 100 in `add_job_vacancy` (0x69bafb). A nation that no longer exists
 * has no continent (+0x71 null; the data load 0x5315a0 links an id of -1 as null): in the 3.9.68 data East and West
 * Germany, the Soviet Union, the Commonwealth of Soviet States, Czechoslovakia and Zaire (and Pays Basque and Timor,
 * not FIFA members then), and in a season game the old record of a renamed nation (season_intl.c dead_records). The original's pools leave such a nation out, so it gets
 * no manager either (Q43). */
uint32_t rt_national_vacancy(uint32_t club, uint32_t site)
{
    static const char *const why[] = {"the vacancy", "a retirement", "a move to another national team", "the month check",
                                       "a departure"};
    if (!fixes_on_group(MANAGERS) || creating || !club) return 0;
    uint32_t nation = rd32(g_mem, club + 0x53u);
    if (!nation || !rd32(g_mem, nation + 0x71u)) return 0;
    int32_t id = (int32_t)rd32(g_mem, club), clubs = (int32_t)rd32(g_mem, CLUB_COUNT);
    int32_t nations = (int32_t)rd32(g_mem, NATION_COUNT);
    if (id < clubs - 2 * nations || id >= clubs - nations) return 0;
    if (site)    /* site 0, the bound in add_job_vacancy: no line (it mostly follows one of the others) */
        fprintf(stderr, "recomp: national manager: a vacancy for %s after %s (%d players)\n", log_name(club + 4u),
                why[site < 5u ? site : 0u], (int32_t)rd32(g_mem, nation + 0x88u));
    return 1;
}

/* `database_get_staff_age` 0x53ab90 (cdecl: staff, date) gives the age in ax (0x53ac0a..0x53ac3a), and its callers keep
 * al. Above 127 it gives 127, so no caller sees a negative age (Q27). A null staff goes to the original's assert. */
void o_0053ab90(void);

RT_WRAP(0053ab90, staff_age)
{
    uint32_t staff = rt_arg(0);
    o_0053ab90();
    if (rt_unwinding || !fixes_on_group(AGE) || !staff) return;
    if ((int16_t)(cpu.eax & 0xffffu) > 127) cpu.eax = (cpu.eax & 0xffff0000u) | 127u;
}

/* Olympic squads (open item 102). `NationalTeams::select_squad` 0x76c120 picks the Olympic squad in slot 2 (the
 * under-21 side, 0x76c26d). A candidate in his nation's senior squad (+0x24 the senior side, flag 4 in +0x5e,
 * 0x76cdf1..0x76ce05) is asked of `national_teams_can_change_squad` 0x76db00 (nation, slot 0, player, removing) at
 * 0x76ce15. For the Olympics (fixture +0 is the key at 0x9cf7a0, 0x76ce2d) he may then leave the senior squad.
 * 0x76db00 refuses while the senior squad's window runs (its end at record +0x3a after today, 0x76deb5..0x76dee0),
 * but only for a squad marked selected (+0x44, 0x76de03). Only the human's squad screens set that mark
 * (set_squad_selected 0x76c0f0, from 0x779f7e, 0x784641, 0x78469a). So an AI senior squad lost its young players
 * to an Olympic pick made between its own pick and its match. The squads of a fixture are picked when it is 14 days
 * away (select_squads_for_fixture 0x76bab0 from the daily pass 0x76b100). A senior match in the 14 days before the
 * Olympics therefore meets it. In the run set game a2001offs3 (changing rules off, main 738c1269) Holland kept 10
 * of 26 for a World Cup qualifier on 31 August 2052 and stopped on the box match_eng..cpp 612.
 * Here, at 0x76ce1c (the jump that skips the candidate), the Olympic pick also skips a player of an AI senior squad
 * whose window ends today or later. On the last day of a window the original asks 0x5acee0 (not read) before it
 * allows a change (0x76dee5..0x76df87). The fix keeps the player on that day too. Other picks of slot 1 or 2 skip a
 * senior squad player anyway (0x76ce38). With the group off (keep:olympics) the line says what the original allows. */
#define NATIONAL_TEAMS 0xb74318u
#define OLYMPICS_KEY 0x9cf7a0u
#define TODAY 0xae2c90u

uint32_t rt_olympic_keeps_senior(uint32_t player, uint32_t nation, uint32_t slot, uint32_t fixture)
{
    if ((slot & 0xffu) == 0 || !player || !nation || !fixture) return 0;
    uint32_t teams = rd32(g_mem, NATIONAL_TEAMS);
    if (rd32(g_mem, fixture) != rd32(g_mem, OLYMPICS_KEY) || !teams) return 0;
    uint32_t senior = rd32(g_mem, teams + 4u) + 110u * rd32(g_mem, nation);     /* slot 0 record, as 0x76dd16 */
    int end_day = (int16_t)rd16(g_mem, senior + 0x3au), end_year = (int16_t)rd16(g_mem, senior + 0x3cu);
    int day = (int16_t)rd16(g_mem, TODAY), year = (int16_t)rd16(g_mem, TODAY + 2u);
    if (end_year < year || (end_year == year && end_day < day)) return 0;
    int on = fixes_on_group(OLYMPICS);
    if ((int8_t)g_mem[player + 0x18u] > 23) return (uint32_t)on;     /* too old for this pick anyway (0x76cee3) */
    char name[32];
    snprintf(name, sizeof name, "%s", log_name(nation + 4u));
    fprintf(stderr, "recomp: olympic squad: %s %s staff %d %s its senior squad on day %d of %d (window to day %d of "
                    "%d)\n", name, on ? "keeps" : "may take", (int32_t)rd32(g_mem, player), on ? "in" : "from", day,
            year, end_day, end_year);
    return (uint32_t)on;
}

/* The Irish Super Cup (open item 127, tools/recomp/original_faults.py). At 0x63b2ae the last loop of
 * `IreSuperCup::create_table` 0x63b0c0 has found a Premier Division club. The table (+0xb1) holds rows of 0x3b bytes
 * with the club at +0 (League::add_club_to_table 0x687430), and the loops have filled the first `rows` (ebx). A club
 * that is one of them is skipped, so the table holds four clubs and the edition can complete. With the group off
 * (keep:ire_super) the line says that the original takes the club again. */
uint32_t rt_ire_super_has(uint32_t cup, uint32_t club, uint32_t rows)
{
    uint32_t table = rd32(g_mem, cup + 0xb1u);
    int found = 0;
    for (uint32_t i = 0; table && i < rows && i < 4u && !found; i++) found = rd32(g_mem, table + 0x3bu * i) == club;
    if (!found) return 0;
    int on = fixes_on_group(IRE_SUPER);
    fprintf(stderr, "recomp: Irish Super Cup %d: %s %s, already in the table\n", (int16_t)rd16(g_mem, cup + 0x40u),
            on ? "skips" : "takes again", log_name(club + 4u));
    return (uint32_t)on;
}

/* Irish Super Cup, saves of a game that met the fault (open item 127). The broken edition never completes, so the
 * season pass never calls IreSuperCup::start_new_season 0x63b550 (all_stages_complete 0x48ce10 stays false), and
 * IreSuperCup::update_for_date 0x63b300 draws only while +0x40 is the game year ([0xde1f58]+0x439, 0x63b311..
 * 0x63b331). The cup is then gone for every later season. The real cup is played every season. In a game with no
 * fault, start_new_season runs after each final, so +0x40 is never below the game year (the trace of a2001offs3,
 * 2051 to 2057). So when the game year moves on, +0x40 is below it and the table holds one club twice (the state of
 * the fault, ire_super_broken), the wrap runs start_new_season. It runs outside the season pass, so
 * season_change_start runs no hooks, but only a broken cup gets here. When the cup is drawn anew (+0x45 0) and still
 * behind, the wrap sets +0x40 to the game year. The original then draws the cup on that day, as it does in a game
 * with no fault. The broken edition leaves no history row (checked on the
 * batch O save of a2001offs3). A load or a new game forgets the year seen (rt_ire_super_forget, from
 * season_dates_forget), so the restart waits for a year turn that the game plays.
 * A cup that the rules table parks in a later year (season_kind_cup_breaks.c, 2999) is never below the game year. */
void o_0063b300(void);
static int ire_super_seen;              /* the game year at the last update; 0 after a load or a new game */

void rt_ire_super_forget(void)
{
    ire_super_seen = 0;
}

/* 1 when the table of the cup (+0xb1, rows of 0x3b bytes, club at +0) holds one club twice in its four rows: the
 * state that the fault leaves. With changing rules on, the rules table stops the cup after 2001
 * (season_kind_cup_breaks.c skips start_new_season), and that cup keeps a whole table and a year below the game
 * year: it must stay stopped. */
static int ire_super_broken(uint32_t cup)
{
    uint32_t table = rd32(g_mem, cup + 0xb1u);
    if (!table) return 0;
    for (uint32_t i = 0; i < 4u; i++)
        for (uint32_t j = i + 1; j < 4u; j++) {
            uint32_t a = rd32(g_mem, table + 0x3bu * i);
            if (a && a == rd32(g_mem, table + 0x3bu * j)) return 1;
        }
    return 0;
}

RT_WRAP(0063b300, ire_super_update)
{
    uint32_t self = cpu.ecx;
    int game = (int16_t)rd16(g_mem, rd32(g_mem, 0xde1f58u) + 0x439u);
    int year = (int16_t)rd16(g_mem, self + 0x40u);
    int turned = ire_super_seen && game == ire_super_seen + 1;
    ire_super_seen = game;
    if (turned && year < game && ire_super_broken(self)) {
        int on = fixes_on_group(IRE_SUPER);
        fprintf(stderr, "recomp: Irish Super Cup: the cup of %d is %s in %d\n", year, on ? "restarted" : "left stopped",
                game);
        if (on) {
            Cpu saved = cpu;
            cpu.ecx = self;
            rt_callback(0x63b550u, 0, NULL);
            if (rt_unwinding) return;
            cpu = saved;
            if (!rd32(g_mem, self + 0x45u) && (int16_t)rd16(g_mem, self + 0x40u) < game)
                wr16(g_mem, self + 0x40u, (uint16_t)game);
        }
    }
    cpu.ecx = self;
    o_0063b300();
}

/* Australian NSL stages (open item 128, original_faults.py). `AusNsl::start_new_season` 0x411de0 keeps the stage
 * index (+0x30) and the stages of the season before, so from the second season the game builds no finals series
 * and no Grand Final (`AusNsl::advance_stage` 0x411f70 acts only while the index is below the count - 1). At
 * 0x411e1c each stage 0..index is freed by its slot 0 with 1, as `ArgPrm::start_new_season` 0x405720,
 * `BraNatFirst::start_new_season` 0x42a640 and the AusNsl destructor 0x4110e0 do, its slot of the array (+0xc) set to 0, and the index set to -1, as the ctor 0x410e90
 * leaves it. */
#define NSL_STAGES 0xcu
#define NSL_STAGE_COUNT 0x2cu
#define NSL_STAGE_NOW 0x30u

void rt_aus_nsl_new_season(uint32_t self)
{
    if (!fixes_on_group(NSL)) return;
    int32_t now = (int32_t)rd32(g_mem, self + NSL_STAGE_NOW), count = (int32_t)rd32(g_mem, self + NSL_STAGE_COUNT);
    uint32_t stages = rd32(g_mem, self + NSL_STAGES);
    if (now < 0) return;
    for (int32_t i = 0; stages && i <= now && i < count; i++) {
        uint32_t stage = rd32(g_mem, stages + 4u * (uint32_t)i);
        if (!stage) continue;
        wr32(g_mem, stages + 4u * (uint32_t)i, 0u);
        Cpu saved = cpu;
        cpu.ecx = stage;
        const uint32_t one = 1u;
        rt_callback(rd32(g_mem, rd32(g_mem, stage)), 1, &one);      /* the scalar deleting destructor, slot 0 */
        if (rt_unwinding) return;
        cpu = saved;
    }
    wr32(g_mem, self + NSL_STAGE_NOW, 0xffffffffu);
    fprintf(stderr, "recomp: aus nsl: a new season from stage %d of %d on day %d of %d\n", now, count,
            (int16_t)rd16(g_mem, TODAY), (int16_t)rd16(g_mem, TODAY + 2u));
}

/* Money (Q24). `Finance::get_transfer_budget` 0x598800 works on a 64-bit budget, but cuts three values to 32 bits.
 * The first two (0x598f4f, 0x598f8b, 0x598f92) re-extend the low word with cdq: here the high word is used. */
uint32_t rt_budget_hi(uint32_t lo, uint32_t hi)
{
    return money_fixes_on() ? hi : (uint32_t)((int32_t)lo >> 31);
}

/* The third is the floor R = balance - 10m (or 15m) (0x599135, 0x599185), a 32-bit value in [esp+0x5c] that the
 * budget falls back to (0x5991d0..0x599201: above R only a tenth counts). Above 2,147,483,647 the store keeps
 * 2,147,483,647 and the full value waits here for the three reads of R. A new call resets it (the wrap below). */
static int64_t floor_wide;
static int floor_on, floor_used;

uint32_t rt_budget_floor(uint32_t finance, uint32_t less)
{
    int64_t r = (int64_t)rd64(g_mem, finance) - (int64_t)less;
    floor_on = money_fixes_on() && r > INT32_MAX;
    if (!floor_on) return rd32(g_mem, finance);
    floor_wide = r;
    return (uint32_t)INT32_MAX + less;      /* the sub that follows leaves 2,147,483,647 */
}

uint32_t rt_budget_floor_low(uint32_t r)
{
    floor_used = floor_on && r == (uint32_t)INT32_MAX;
    return floor_used ? (uint32_t)floor_wide : r;
}

uint32_t rt_budget_floor_high(uint32_t lo)
{
    return floor_used ? (uint32_t)((uint64_t)floor_wide >> 32) : (uint32_t)((int32_t)lo >> 31);
}

int64_t rt_budget_floor_wide(uint32_t r)
{
    return floor_on && r == (uint32_t)INT32_MAX ? floor_wide : (int64_t)(int32_t)r;
}

/* The 37 callers of 0x598800 (by the return address): these 14 keep the 64-bit result (the docs file), three of
 * them only to show it (0x463b20, 0x808ee9, 0x8ea2b4, with rt_cash_wide). The other 23 keep eax: the clamp gives
 * them 2,147,483,647 for a larger budget, where eax alone would be 0 or a wrong amount. */
static const uint32_t budget_wide_callers[] = {
    0x463afdu, 0x463b20u, 0x4d7541u, 0x6970a3u, 0x6a3b8cu, 0x7b6d5fu, 0x7b6e25u, 0x808ee9u, 0x8c0eb6u, 0x8e74a7u,
    0x8e83a1u, 0x8ea2b4u, 0x8f4b28u, 0x8f4bd5u,
};

void o_00598800(void);

static uint32_t news_club;    /* the club of the last budget above 2,147,483,647 that a caller got clamped */
static int64_t news_full;

static int money_log(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_MONEY_LOG") != NULL;
    return on;
}

RT_WRAP(00598800, get_transfer_budget)
{
    uint32_t from = rd32(g_mem, cpu.esp), finance = cpu.ecx;
    floor_on = floor_used = 0;
    o_00598800();
    if (rt_unwinding) return;
    int64_t v = (int64_t)(((uint64_t)cpu.edx << 32) | cpu.eax);
    int wide = 0;
    for (size_t i = 0; i < sizeof budget_wide_callers / sizeof budget_wide_callers[0]; i++)
        wide |= from == budget_wide_callers[i];
    if (money_fixes_on() && !wide) {
        if (v > INT32_MAX) {
            cpu.eax = (uint32_t)INT32_MAX, cpu.edx = 0;
            news_club = rd32(g_mem, finance + 8u);    /* rt_news_wide: a news item may carry this budget */
            news_full = v;
        } else if (rd32(g_mem, finance + 8u) == news_club) {
            news_full = 0;    /* the club's budget fits now: an amount of exactly 2,147,483,647 is its own */
        }
        else if (v < INT32_MIN) cpu.eax = 0x80000000u, cpu.edx = 0xffffffffu;
    }
    /* test switch (tests/seasons/original_faults_run.py money): the budgets of a club above 2,147,483,647 */
    if (money_log() && (int64_t)rd64(g_mem, finance) > INT32_MAX) {
        uint32_t club = rd32(g_mem, 0xae23b4u) + rd32(g_mem, finance + 8u) * 0x245u;
        fprintf(stderr, "recomp: money budget: %s from 0x%x: %lld, the caller sees %lld\n", log_name(club + 4u), from,
                (long long)v, wide ? (long long)v : (long long)(int32_t)cpu.eax);
    }
}

/* The money text. A Cash holds 32 bits, and `Cash::get_display_amount` 0x43f460 starts with `fild dword [ecx]`;
 * after that the original works on 64 bits (the rounding step 0x4426e0 and the text `cash_format_money`
 * 0x441c10 take an int64). A screen that puts a 64-bit amount into a Cash registers it here with the number of reads
 * of that site (one, or two for the club info balance, which formats it again when the text is long, 0x4639c8..0x4639e6,
 * and ends at 0x4639ee). Those reads take the full amount while the Cash keeps its low word. */
static uint32_t wide_at, wide_reads, wide_stored;
static int64_t wide_amount;

/* The Cash at cash holds stored; its next reads give amount. */
static void cash_wide(uint32_t cash, uint32_t stored, int64_t amount, uint32_t reads)
{
    wide_reads = money_fixes_on() && amount != (int64_t)(int32_t)stored ? reads : 0u;
    wide_at = cash;
    wide_stored = stored;
    wide_amount = amount;
}

void rt_cash_wide(uint32_t cash, uint32_t lo, uint32_t hi, uint32_t reads)
{
    cash_wide(cash, lo, (int64_t)(((uint64_t)hi << 32) | lo), reads);
}

/* The bid screen (0x8ea2c0): the Cash keeps the budget clamped to 2,147,483,647, because the fee colour reads it
 * again (0x8ea54c, 0x8eaf95, 0x8eb3b0); only the text read gives the full amount. Returns the value to store. */
uint32_t rt_cash_wide_clamped(uint32_t cash, uint32_t lo, uint32_t hi, uint32_t reads)
{
    int64_t v = (int64_t)(((uint64_t)hi << 32) | lo);
    uint32_t stored = !money_fixes_on() ? lo : v > INT32_MAX ? (uint32_t)INT32_MAX : v < INT32_MIN ? 0x80000000u : lo;
    cash_wide(cash, stored, v, reads);
    return stored;
}

void rt_cash_wide_end(void)
{
    wide_reads = 0;
}

int64_t rt_cash_amount(uint32_t cash)
{
    uint32_t lo = rd32(g_mem, cash);
    if (wide_reads && cash == wide_at && lo == wide_stored) {
        wide_reads--;
        if (money_log())     /* test switch: tests/seasons/original_faults_run.py money */
            fprintf(stderr, "recomp: money text: %lld\n", (long long)wide_amount);
        return wide_amount;
    }
    return (int64_t)(int32_t)lo;
}

/* `Finance::get_max_wage` 0x5968d0: the wage term plus ftol(1.3 x balance) (ftol at 0x5968e3, only eax kept at
 * 0x5968ee), in 32 bits at 0x597019. 0x5968e8 hands over the high word in edx; the sum takes it when the low word is
 * still the one of that product, and a sum above 2,147,483,647 is clamped to it. Any other sum, a debt among them,
 * is the original's. */
static uint32_t wage_high, wage_low, wage_set;

void rt_max_wage_high(uint32_t edx, uint32_t eax)
{
    wage_high = edx;
    wage_low = eax;
    wage_set = 1;
}

uint32_t rt_max_wage_sum(uint32_t term, uint32_t lo)
{
    int had = wage_set && lo == wage_low;
    wage_set = 0;
    if (!money_fixes_on() || !had) return term + lo;
    int64_t sum = (int64_t)(int32_t)term + (int64_t)(((uint64_t)wage_high << 32) | lo);
    return sum > INT32_MAX ? (uint32_t)INT32_MAX : term + lo;
}

/* News (Q46). A news item keeps 50 32-bit slots at item + 5 + 4 x slot (`NewsItem::set_slot` 0x785790), saved raw in
 * news.dat. The job offer (0x6a5021) and the finance news (0x59f36b) put a transfer budget in slot 2, clamped to
 * 2,147,483,647 by the wrap of 0x598800. When that budget was clamped for the same club, its full amount also goes into
 * slots 8 (low word) and 9 (high word), which these items do not use and which the constructor clears
 * (0x77bc1b..0x77bc29): below 2,147,483,647 the item, and so the save, is unchanged. */
#define NEWS_SLOT(item, n) ((item) + 5u + 4u * (n))

void rt_news_wide(uint32_t item, uint32_t club, uint32_t value)
{
    if (!money_fixes_on() || value != (uint32_t)INT32_MAX || club != news_club || news_full <= INT32_MAX) return;
    wr32(g_mem, NEWS_SLOT(item, 8u), (uint32_t)news_full);
    wr32(g_mem, NEWS_SLOT(item, 9u), (uint32_t)((uint64_t)news_full >> 32));
}

/* The text writers read slot 2 (0x6a51dd, 0x59f5ab) and later put it into a Cash (0x6a533d, 0x59f5c7). */
static int64_t news_pending;

void rt_news_read(uint32_t item, uint32_t value)
{
    uint64_t full = ((uint64_t)rd32(g_mem, NEWS_SLOT(item, 9u)) << 32) | rd32(g_mem, NEWS_SLOT(item, 8u));
    news_pending = money_fixes_on() && value == (uint32_t)INT32_MAX && (int64_t)full > INT32_MAX ? (int64_t)full : 0;
}

void rt_cash_news(uint32_t cash, uint32_t value)
{
    if (news_pending && value == (uint32_t)INT32_MAX) cash_wide(cash, value, news_pending, 1u);
    news_pending = 0;
}

/* Holiday offers (open item 132, original_faults.py). The settings of "Go on holiday ?" are the bytes at human record
 * +0x9b23 (HUMAN_MANAGER::get_holiday_option 0x5fea50): option 0 is 1 for "Reject all offers for players" and 2 for
 * "Reject all offers below asking price" (the confirm build 0x6b2d80, its input 0x6b35c1). A player's asking price is
 * staff +0x52 (TRANSFER_MANAGER::set_player_squad_status 0x8ce1e9). */
#define HUMANS 0xb63c98u          /* the human manager object: +0xc the count of humans on holiday */
#define STAFF_RECORDS 0xae23bcu
#define IS_HUMAN_CLUB 0x600a50u   /* cdecl (club, ignore_holiday, quick, manager, any) */
#define CLUB_MANAGER 0x53ca90u    /* cdecl (club) */
#define ON_HOLIDAY 0x5fd210u      /* thiscall HUMANS (manager) */
#define HOLIDAY_OPTION 0x5fea50u  /* thiscall HUMANS (option, manager) */
#define RELEASE_CLAUSE 0x867b50u  /* thiscall contract: a release clause in force */

/* The value of option 0 when a human manager on holiday runs the club, else 0. The test is the one that
 * `shortlist_manager_predict_selling_club_response` 0x84af00 makes at 0x84b05e..0x84b096, but with the human test of
 * `ai_respond_to_bid` 0x8c81ea (0x600a50 with ignore_holiday 1), so that a club on holiday passes it. A contract with
 * a release clause in force keeps the original's answer (0x84b05e), and so does every club while no human is on
 * holiday (the count at [0xb63c98] + 0xc, kept by `set_on_holiday` 0x5fd2f0). */
static uint32_t holiday_option(uint32_t club, uint32_t contract)
{
    if (!fixes_on_group(HOLIDAY) || !club || !rd32(g_mem, rd32(g_mem, HUMANS) + 0xcu)) return 0;
    Cpu saved = cpu;
    uint32_t r = 0;
    uint32_t a[5] = {club, 1u, 1u, 0u, 0u};
    if (!rt_callback(IS_HUMAN_CLUB, 5, a) || rt_unwinding) goto done;
    if (contract) {
        cpu.ecx = contract;
        if (rt_callback(RELEASE_CLAUSE, 0, NULL) || rt_unwinding) goto done;
    }
    uint32_t m[1] = {club};
    uint32_t manager = rt_callback(CLUB_MANAGER, 1, m);
    if (rt_unwinding) goto done;
    uint32_t h[1] = {manager};
    cpu.ecx = rd32(g_mem, HUMANS);
    if (!rt_callback(ON_HOLIDAY, 1, h) || rt_unwinding) goto done;
    uint32_t o[2] = {0u, manager};
    cpu.ecx = rd32(g_mem, HUMANS);
    r = rt_callback(HOLIDAY_OPTION, 2, o) & 0xffu;
done:
    if (rt_unwinding) return 0;
    cpu = saved;
    return r == 1u || r == 2u ? r : 0;
}

/* The predictor 0x84af00 for a transfer (0x84b02f, the computer path of a club on holiday): 1 for "Reject all
 * offers" (the answer 2, reject, at 0x84b01c), 2 for "below asking price" when the player has an asking price
 * (staff +0x52 above 0, the price that the dead test takes at 0x84b0a8), else 0: the original's computer path at
 * 0x84b0ad. */
uint32_t rt_holiday_predict(uint32_t club, uint32_t contract, uint32_t player)
{
    uint32_t opt = holiday_option(club, contract);
    if (opt == 2u && (int32_t)rd32(g_mem, player + 0x52u) <= 0) return 0;
    return opt;
}

/* The predictor 0x84af00 for a loan (0x84af77): 1 when the club is on holiday with "Reject all offers" (the answer 2
 * at 0x84b162). */
uint32_t rt_holiday_refuses_loan(uint32_t club)
{
    return holiday_option(club, 0) == 1u;
}

/* The bid reply 0x8c8090 at 0x8c8264, after its human and holiday tests (0x8c81ea..0x8c8216): 1 when option 0 (al) is
 * "below asking price" and the bid's fee (offer +0x1b) is below the player's asking price, so the bid takes the
 * reject of "Reject all offers" at 0x8c8266. esi holds the offer slot. */
uint32_t rt_holiday_below_asking(uint32_t option, uint32_t slot)
{
    if (!fixes_on_group(HOLIDAY) || (option & 0xffu) != 2u) return 0;
    uint32_t offer = rd32(g_mem, slot);
    if (!offer) return 0;
    uint32_t player = rd32(g_mem, STAFF_RECORDS) + rd32(g_mem, offer + 0xcu) * 0x6eu;
    int32_t ask = (int32_t)rd32(g_mem, player + 0x52u), fee = (int32_t)rd32(g_mem, offer + 0x1bu);
    return ask > 0 && fee < ask;
}

/* Senior call-ups (open item 143, original_faults.py). `NationalTeams::select_squad` 0x76c120 refuses a candidate at a
 * club on another continent (club +0x53 nation, nation +0x71 continent, 0x76cc3d..0x76cc59 and 0x76cc9f..0x76ccbd):
 * for a friendly (fixture +0 the key at 0x9cf668) when his club's reputation (+0x80) x 0.8 (0x9689b0) is over the
 * opponent's (0x76ccdd..0x76cd3e), for another record when his division's reputation (club +0x57, +0x69) x 0.7
 * (0x968e40) is over the competition's (0x76cc61..0x76cc9a). 1 when the pick is for a senior squad (slot 0, the
 * byte argument at [esp + 0x8e4], 0x76cd9c) and the fix acts: the candidate then goes on as a player at a club of his
 * own continent does. 'B' and under-21 picks keep the original's rule. */
uint32_t rt_callup_released(uint32_t slot)
{
    return fixes_on_group(CALLUPS) && (slot & 0xffu) == 0u;
}

/* The draw of the 2002 World Cup (open item 166). `WorldCup::draw_groups` 0x92e940 first draws pot 1 at random: the
 * slots 0 and 1 go to groups 1 and 3, and for each other group it picks a slot of 2 to 7 (random 0..5 + 2, 0x92ea3b)
 * until the slot's byte +4 is not 0xff, then writes 0xff into it (0x92ea92). Only then it tests the year (0x92eb46)
 * and writes the real 2002 draw over the whole result (0x92eb52..0x92efe9). The pots are the array +0xee of 32 slots
 * of 5 bytes. `WorldCup::seed_teams` 0x92f230 writes them for every edition but 2002: for 2002 it returns at once
 * (0x92f240). The constructor 0x92bf50 never writes them, and operator new 0x944cf1 does not clear, so the 2002
 * pots hold what the heap held. The save writes them (0x92f150) and the load reads them back (0x92f000). One byte of
 * 0xff in the slots 2 to 7 leaves five slots for six picks, and the loop at 0x92ea57..0x92ea71 never ends: the game
 * stops on the draw day (+0xb6, 27 December 2001) in every load of that game. Before the 2002 draw the fix writes 0
 * into each byte +4 of the slots 2 to 7 that holds 0xff, so all six slots are free, as seed_teams leaves them for the
 * other editions (0x92f536). The 2002 result never uses the pots, and only the test for 0xff reads them, so a game
 * whose pots hold no 0xff draws as before and saves the same bytes. With the group off (keep:wc_pots) the pots stay
 * as they are. */
void rt_wc2002_pots(uint32_t cup)
{
    int ff = 0, on = fixes_on_group(WC_POTS);
    for (uint32_t k = 2; k < 8u; k++) {
        uint32_t at = cup + 0xeeu + 5u * k + 4u;
        if (rd8(g_mem, at) != 0xffu) continue;
        ff++;
        if (on) wr8(g_mem, at, 0);
    }
    if (ff)
        fprintf(stderr, "recomp: World Cup 2002 draw: %d pot 1 slots hold 0xff, the pots are %s\n", ff,
                on ? "cleared" : "kept");
}

/* The Confederations Cup in June (open items 157 to 159): season_intl_date_shift (season_intl_rules.c) moves an edition
 * that keeps the original's dates, 29 July to 9 August, to the Saturday on or before 18 June,
 * so all of it plays in June. It acts from the new game on:
 * the base game builds its 2003 edition there (the stage builder 0x58bdb0). */
int season_intl_confed_june(void)
{
    return fixes_on_group(CONFED);
}

/* Group club_regulars (open item 160, original_faults.py) has two patches, and keep:club_regulars turns off both:
 * these marks, and the new pick of a kept squad (rt_squad_kept below). The marks: a club of a nation with no league
 * keeps its regulars for a continental match. NationalTeams::mark_clubs_with_fixtures 0x7775c0 marks a club of a
 * match with no nation (mark 3) only when the club's nation has a league in the game (nation +0x11c at 0x7776fc and
 * 0x777721). 1 when the fix acts: the jumps at 0x777704 and 0x777729 then mark such a club (a club with no nation
 * stays unmarked, 0x7776fa and 0x77771f), and the squad pick 0x76c120 leaves out its regulars (0x76cd44..0x76cd73). */
uint32_t rt_callup_mark_all(void)
{
    return (uint32_t)fixes_on_group(CLUB_REGULARS);
}

/* Group callup_counts (open item 300 of 0.6.2, original_faults.py). NationalTeams::record_club_call_ups 0x7771c0
 * (thiscall: side, B flag, fixture; ret 0xc) sorts the side's 50 squad slots by club id (qsort 0x945333 at 0x777225
 * with 0x778ef0: players with a club first, by id) and counts the players of each club in a run. It writes a club's
 * count to the club's list of call-up records (this +0x20, 6 bytes a club: +0 the records of 9 bytes, +5 their
 * count; a record is a Date and +8 the count) only when the next counted player has another club (0x7772f7..0x7773fe).
 * After the 50th slot it leaves (0x77741a..0x777426 to 0x77748b), so the club that sorts last never gets a record,
 * unless a player with no club comes after it. NationalTeams::can_schedule_club_fixture 0x776fc0 reads these records
 * (more than two players within a day of a fixture: clash 21, 0x5adb5b..0x5adb6b), and so does the call-up move
 * NationalTeams::reschedule_clashing_club_fixtures 0x7777e0 (0x7778c3). So a club fixture made or moved after the
 * squad pick could land next to the side's match: in release set row t2006 (seed 73) Urawa Red Diamonds (club id
 * 7935, the highest in Japan's squad) had no record for Japan's friendly of 11 September 2007, and find_new_fixture_date
 * 0x5ab510 moved its AFC Champions League first leg of 30 August to 12 September, with 5 players in Japan's squad.
 * Here the loop's end (0x777426) goes back once to the write of a run (0x7772f7) with the club of the open run (esi),
 * and the write leaves at 0x777401, where the slot counter at esp + 0x13 is 50 only on that pass. The fix acts only in
 * a season game (season_game_changes): the base 3.9.68 game, a 2001 game with changing rules off, keeps the original
 * (decision L2). 1 when the fix acts and a run is open. */
uint32_t rt_callup_count_last(uint32_t club)
{
    return club && fixes_on_group(CALLUP_COUNTS) && season_game_changes();
}

/* A kept squad is picked again for a match when one of its players is a regular of a club with a continental match
 * within a day (open item 160, group club_regulars, original_faults.py). The squad check of a national fixture
 * 0x76bab0 (called from NationalTeams::daily_update at 0x76b30e) keeps a side's squad for its next match when that
 * match's check comes at most 14 days after the window end (0x54cdb0 at 0x76bb99 and 0x76be0c, cmp 0xe) and the
 * squad is set (+0x43); else it calls select_squad 0x76c120
 * (0x76bc23 and the away side's call after 0x76be83). A kept squad never meets the pick's gate (0x76cd44..0x76cd73).
 * nt: NationalTeams (ebp there), side: the senior side of the fixture (+0x1c home at 0x76bba8, +0x20 away at
 * 0x76be1b), record: the nation's squad record (edi there; +0x3a the window end, s16 day of the year and s16 year),
 * fixture: esi there (+0x28 s16 year, +0x2a s16 day of the year). Only when no match of the window is still to come
 * (today on or after the window end: 0x54cdb0 at 0x76bb99 gives today minus that day, and a kept squad's window can
 * hold a match before this one) and the fixture is on the day two weeks ahead, the day for which daily_update 0x76b100
 * builds the marks (0x54c770 with 2 at 0x76b126, 0x76b257..0x76b2bf; 0x76bab0 also runs for a new fixture from
 * 0x5aab15, with the marks of another day). A mark 3 that a later mark 1 or 2 of the day before or after overwrites
 * (0x777779) is not seen: that club's league match moves instead (0x7777e0).
 * Gives 0 when the fix acts, the fixture's key is one that reaches the gate (see rt_squad_kept_at) and a player of
 * the nation's squad (+0xd7 of its senior side, with flag 4 in staff +0x5e; 50 staff pointers, as the sort
 * 0x778ef0 reads them) is at a club (staff +0x39) with mark 3 in the list of mark_clubs_with_fixtures 0x7775c0 (nt
 * +0x24, a byte per club below the national sides) and has a contract whose playing-time mood (+0x3d) is over -10
 * (the gate's test, CONTRACT_MANAGER::get_contract 0x4cf5a0: staff row [0xae2d0c] + id x 79, +0 the contract index;
 * contract [[0xae1050]] + index x 80). Else 1: the squad is kept, as in the original. */
#define SQUAD_AT 0xd7u
#define SQUAD_SLOTS 50
#define STAFF_CLUB 0x39u
#define STAFF_ROWS 0xae2d0cu
#define STAFF_COUNT 0xae2364u
#define CONTRACTS 0xae1050u
#define MOOD 0x3du

static int pointer(uint32_t p) { return rt_readable(p); }

static long abs_day(int year, int doy)          /* days from 1 January 1900, for the dates of the game */
{
    long n = doy;
    for (int y = 1900; y < year; y++) n += 365 + ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
    return n;
}

#define TODAY 0xae2c90u
#define AHEAD 14                    /* daily_update's day: two weeks ahead (0x54c770 with 2 at 0x76b126) */

/* The same for the B squad (slot 1: window end +0x4e, the keep tests 0x76bc76 and 0x76bec7) and the under-21 squad
 * (slot 2: window end +0x62, the keep tests 0x76bd34 and 0x76bf85), open item 299 of 0.6.2. The window is the record
 * offset of the squad's window end (s16 day of the year, s16 year). The squad read is the one the new pick
 * (select_squad 0x76c120) replaces: slots 0 and 1 the nation's senior side (0x53b3d0: club clubs - 2 x nations + the
 * nation id, 0x53b4c2..0x53b4e0), slot 2 its under-21 side (0x53b4f0: clubs - nations + the id, 0x53b5e2..0x53b5fe),
 * whatever the fixture's side (esi +0x1c or +0x20 is the senior side for a B match and for an under-21 match linked to
 * a senior one, flag 0x200 at 0x76bcf8). The senior and B squads share the senior side's slots (+0xd7), so a player
 * counts only with his squad's flag in staff +0x5e (the pick's clear tests it at 0x76c63b..0x76c63f): 8 for B, 4 for
 * the senior and under-21 squads. The new pick acts only for a fixture whose key (fixture +0) is one of the three that
 * reach the gate (0x76cc1f..0x76cc3b: the friendly 0x9cf668, 'B' 0x9cf794, Under 21 0x9cf798); for any other key the
 * pick takes a club's regular all the same. */
#define SQUAD_FLAG 0x5eu
#define SIDE_SIZE 0x245u

uint32_t rt_squad_kept(uint32_t nt, uint32_t side, uint32_t record, uint32_t fixture)
{
    return rt_squad_kept_at(nt, side, record, fixture, 0x3au);
}

uint32_t rt_squad_kept_at(uint32_t nt, uint32_t side, uint32_t record, uint32_t fixture, uint32_t window)
{
    if (!fixes_on_group(CLUB_REGULARS) || !pointer(nt) || !pointer(side) || !pointer(record) || !pointer(fixture))
        return 1;
    uint32_t key = rd32(g_mem, fixture);
    if (key != rd32(g_mem, 0x9cf668u) && key != rd32(g_mem, 0x9cf794u) && key != rd32(g_mem, 0x9cf798u)) return 1;
    int ty = (int16_t)rd16(g_mem, TODAY + 2u), td = (int16_t)rd16(g_mem, TODAY);
    int wy = (int16_t)rd16(g_mem, record + window + 2u), wd = (int16_t)rd16(g_mem, record + window);
    int fy = (int16_t)rd16(g_mem, fixture + 0x28u), fd = (int16_t)rd16(g_mem, fixture + 0x2au);
    if (ty < 1900 || wy < 1900 || fy < 1900 || ty > 3000 || wy > 3000 || fy > 3000) return 1;
    long today = abs_day(ty, td);
    if (today < abs_day(wy, wd) || abs_day(fy, fd) != today + AHEAD) return 1;
    uint32_t marks = rd32(g_mem, nt + 0x24u), nation = rd32(g_mem, side + 0x53u);
    int nations = (int)rd32(g_mem, NATION_COUNT), all = (int)rd32(g_mem, CLUB_COUNT), clubs = all - 2 * nations;
    if (!pointer(marks) || !pointer(nation)) return 1;
    int32_t nid = (int32_t)rd32(g_mem, nation);
    int32_t at = (window == 0x62u ? all - nations : clubs) + nid;
    if (nid < 0 || nid >= nations || at < 0 || at >= all) return 1;
    uint32_t squad = rd32(g_mem, 0xae23b4u) + (uint32_t)at * SIDE_SIZE;
    uint8_t flag = window == 0x4eu ? 8u : 4u;
    for (uint32_t k = 0; k < SQUAD_SLOTS; k++) {
        uint32_t st = rd32(g_mem, squad + SQUAD_AT + 4u * k);
        if (!pointer(st) || !(g_mem[st + SQUAD_FLAG] & flag)) continue;
        uint32_t club = rd32(g_mem, st + STAFF_CLUB);
        if (!pointer(club)) continue;
        int32_t id = (int32_t)rd32(g_mem, club);
        if (id < 0 || id >= clubs || g_mem[marks + (uint32_t)id] != 3u) continue;
        int32_t sid = (int32_t)rd32(g_mem, st);
        if (sid < 0 || sid >= (int32_t)rd32(g_mem, STAFF_COUNT)) continue;
        int32_t ci = (int32_t)rd32(g_mem, rd32(g_mem, STAFF_ROWS) + (uint32_t)sid * 79u);
        if (ci < 0) continue;
        uint32_t contract = rd32(g_mem, rd32(g_mem, CONTRACTS)) + (uint32_t)ci * 80u;
        if ((int8_t)g_mem[contract + MOOD] > -10) {
            if (getenv("RECOMP_DUTY_LOG")) {
                char who[64];
                snprintf(who, sizeof who, "%s", log_name(squad + 4u));
                fprintf(stderr, "recomp: duty: %s picks its squad again: a regular of %s has a continental match "
                                "within a day\n", who, log_name(club + 4u));
            }
            return 0;
        }
    }
    return 1;
}

/* The day before a linked match (open item 299 of 0.6.2, group club_regulars). add_fixture 0x5aa680 adds, for a senior
 * national match that is not a tournament match, a linked B or under-21 match of the same nations on the day before
 * (ReserveComp::add_b_or_under21_fixture 0x7e6720, called at 0x5aab09), with flags +0x4d & 0x300 (0x100 B, 0x200
 * under-21) on both fixtures. The squad check 0x76bab0 skips the linked fixture (+0x40 = 0 and bit 1 of +0x4e,
 * 0x76bad1..0x76badc) and does its keep test and pick with the senior match's (home 0x76bc4a..0x76bdcf, away
 * 0x76be9b..0x76c021), on the day of the marks two weeks before the senior match. NationalTeams::daily_update 0x76b100
 * marks the clubs with a match on that day and the days before and after it (mark_clubs_with_fixtures 0x7775c0 at
 * 0x76b257, 0x76b28b, 0x76b2bf). So a club that plays the day before the linked match, two days before the senior
 * match, is not marked, and the B or under-21 pick takes its regulars: in release set row t2002 (seed 73) Vélez
 * Sarsfield played the Copa Sudamericana on 1 September 2003 with three players in the under-21 squad of Argentina,
 * whose linked match was on 2 September (the friendly on 3 September).
 *
 * Here, for the linked part of a senior fixture with a linked match (flags & 0x300, home a senior side), the marks also
 * hold that day (0x7775c0 with the Date of the senior match's day minus 2: s16 day of the year at +0, s16 year at +2,
 * the leap flag at +4, as Date 0x549ef0 writes them, 0x549efa..0x549f5a). The keep test (rt_squad_kept_at) and the
 * pick's gate (0x76cd44..0x76cd73) then see them. The marks come back as they were before the next senior pick (on 0,
 * at 0x76bdd4, 0x76c026 and the end 0x76c043), so senior picks are as before. */
#define MARK_CLUBS 0x7775c0u
static uint8_t *link_copy;
static uint32_t link_marks, link_count;

void rt_link_marks(uint32_t nt, uint32_t fixture, uint32_t on)
{
    if (link_count) {                                   /* back as they were */
        if (pointer(link_marks)) memcpy(g_mem + link_marks, link_copy, link_count);
        link_count = 0;
    }
    if (!on || !fixes_on_group(CLUB_REGULARS) || !pointer(nt) || !pointer(fixture)) return;
    if (!(rd16(g_mem, fixture + 0x4du) & 0x300u)) return;
    uint32_t key = rd32(g_mem, fixture);                /* the linked copy itself (0x7e6773 copies the flags) */
    if (key == rd32(g_mem, 0x9cf794u) || key == rd32(g_mem, 0x9cf798u)) return;
    int nations = (int)rd32(g_mem, NATION_COUNT), clubs = (int)rd32(g_mem, CLUB_COUNT) - 2 * nations;
    uint32_t home = rd32(g_mem, fixture + 0x1cu), marks = rd32(g_mem, nt + 0x24u);
    if (!pointer(home) || !pointer(marks) || clubs <= 0) return;
    int32_t id = (int32_t)rd32(g_mem, home);
    if (id < clubs || id >= clubs + nations) return;   /* not a senior side */
    int fy = (int16_t)rd16(g_mem, fixture + 0x28u), fd = (int16_t)rd16(g_mem, fixture + 0x2au) - 2;
    if (fy < 1900 || fy > 3000) return;
    if (fd < 0) {
        fy--;
        fd += 365 + ((fy % 4 == 0 && fy % 100 != 0) || fy % 400 == 0);
    }
    uint8_t *copy = realloc(link_copy, (size_t)clubs);
    if (!copy) return;
    link_copy = copy;
    memcpy(link_copy, g_mem + marks, (size_t)clubs);
    link_marks = marks, link_count = (uint32_t)clubs;
    Cpu saved = cpu;
    cpu.esp -= 16;
    uint32_t date = cpu.esp;
    wr16(g_mem, date, (uint16_t)fd);
    wr16(g_mem, date + 2u, (uint16_t)fy);
    wr32(g_mem, date + 4u, (uint32_t)((fy % 4 == 0 && fy % 100 != 0) || fy % 400 == 0));
    cpu.ecx = nt;
    uint32_t a[1] = {date};
    rt_callback(MARK_CLUBS, 1, a);
    if (rt_unwinding) {
        link_count = 0;
        return;
    }
    cpu = saved;
    /* 0x7775c0 writes the day's mark over the one there (0x77770f, 0x777734, 0x777779): a mark 3 of the senior days
     * stays 3, and a club marked before keeps its mark, so the new day only adds marks. */
    for (uint32_t i = 0; i < link_count; i++) {
        uint8_t was = link_copy[i], now = g_mem[marks + i];
        if (was == 3u || (was && now != 3u)) g_mem[marks + i] = was;
    }
}

/* Issue 132: the known crash sites of the original. The fixes and guards are in
 * tools/recomp/crashguards.py (0x84aebb, 0x85d8b8, 0x409e25), and two lookups of HolFirst::set_club_progress 0x5f73d0
 * (0x5f73e1, 0x5f74a7) are log-only in tools/recomp/original_faults.py. Each logs its first hit in a run, so that
 * the logs of the run set show whether the port reaches it. Notes: run-logs/0.8.0-s817/site-*.md. */
void rt_crash_site(uint32_t site)
{
    static uint32_t seen[16];
    static unsigned n;
    for (unsigned i = 0; i < n; i++)
        if (seen[i] == site) return;
    if (n < sizeof seen / sizeof seen[0]) seen[n++] = site;
    fprintf(stderr, "recomp: crash site %#x reached, day %d, year %d (issue 132)\n", site, (int16_t)rd16(g_mem, 0xae2c90u),
            (int16_t)rd16(g_mem, 0xae2c92u));
}

/* The move of a one-contract player to or from the linked side (open item 187, tools/recomp/original_faults.py at
 * 0x4cc03d). CONTRACT_MANAGER::get_squad_placement 0x4cbb60 reads bit 0x40 of the player's contract +0x35 (0x4cbcb8):
 * set gives 3 (on the B side), clear gives 4 (at the senior club). CONTRACT_MANAGER::toggle_squad_placement 0x4cbd20
 * sets or clears it on its other paths (0x4cbdb6, 0x4cbe6a, 0x4cbffe..0x4cc016). Its path for a player with one
 * contract (0x4cc03d..0x4cc091) moves him and leaves the bit. Here that path sets the bit for a move to the B side and
 * clears it for a move back. The direction: at placement 4 the path leaves from the club asked about (edi = ebx,
 * 0x4cbede), at placement 3 it goes to it (ebp = ebx, 0x4cbed5). The manager is [esp + 0x10] of 0x4cbd20, the staff
 * is esi. */
#define CONTRACT_INDEX 0xae2d0cu  /* per staff id, 0x4f bytes: +0 the contract index, +4 the second contract index */
#define STAFF_COUNT 0xae2364u

void rt_squad_move_set(uint32_t manager, uint32_t staff, int down)
{
    if (!fixes_on_group(SQUAD_MOVES) || !manager || !staff) return;
    uint32_t id = rd32(g_mem, staff);
    if (id >= rd32(g_mem, STAFF_COUNT)) return;
    int32_t index = (int32_t)rd32(g_mem, rd32(g_mem, CONTRACT_INDEX) + id * 0x4fu);
    if (index < 0) return;
    uint32_t contract = rd32(g_mem, manager) + (uint32_t)index * 0x50u;
    if (down) g_mem[contract + 0x35u] |= 0x40u;
    else g_mem[contract + 0x35u] &= 0xbfu;
}

/* RECOMP_SQUAD_MOVE_TEST=1 (tests/seasons/squad_move_run.py): at the first start report (season_report.c) the game
 * data is loaded. The test takes the first senior club with a linked B side (club_get_linked_b_team 0x540a50 gives the
 * B side and sets the flag) and a player of it with one contract and the placement 4 (get_squad_placement), moves him
 * to the reserves twice (CONTRACT_MANAGER::move_to_reserves 0x4cc0f0, as Shortlist_Manager::assess_club_squad 0x83cb40
 * does) and prints how many of the B side's 50 squad slots (club +0xd7) hold him. One is right. With the fault kept
 * the second move adds him again and gives two. */
#define CLUBS 0xae23b4u
#define CLUB_SIZE 0x245u
#define CONTRACTS 0xae1050u      /* the CONTRACT_MANAGER */

static uint32_t call_this(uint32_t self, uint32_t target, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(target, n, args);
    if (!rt_unwinding) cpu = saved;   /* an unwind leaves cpu as it is (rt.c) */
    return r;
}

static int squad_slots(uint32_t club, uint32_t staff)
{
    int n = 0;
    for (uint32_t k = 0; k < 50u; k++) n += rd32(g_mem, club + 0xd7u + 4u * k) == staff;
    return n;
}

void rt_squad_move_test(void)
{
    static int done;
    if (done || !getenv("RECOMP_SQUAD_MOVE_TEST")) return;
    done = 1;
    uint32_t manager = rd32(g_mem, CONTRACTS), clubs = rd32(g_mem, CLUBS), flag = rt_alloc(4);
    uint32_t count = rd32(g_mem, CLUB_COUNT), index = rd32(g_mem, CONTRACT_INDEX);
    for (uint32_t c = 0; manager && clubs && c < count; c++) {
        uint32_t club = clubs + c * CLUB_SIZE;
        wr32(g_mem, flag, 0);
        uint32_t args[3] = {club, flag, 1};
        uint32_t b = rt_callback(0x540a50u, 3, args);
        if (rt_unwinding) return;
        if (!b || !rd32(g_mem, flag)) continue;          /* the flag: this club is the senior side (0x4cbbe6) */
        /* The B side must not lead back to the club (as Hertha BSC Amateure in item 187): then get_squad_placement
           0x4cbb60 of a player at the B side with his contract at the club answers from bit 0x40 (0x4cbcff), not
           from his club +0x39 (0x4cbbfd). */
        uint32_t bargs[3] = {b, flag, 1};
        uint32_t back = rt_callback(0x540a50u, 3, bargs);
        if (rt_unwinding) return;
        if (back == club) continue;
        for (uint32_t k = 0; k < 50u; k++) {
            uint32_t p = rd32(g_mem, club + 0xd7u + 4u * k);
            if (!p || rd32(g_mem, p + 0x39u) != club || !rd32(g_mem, p + 0x61u)) continue;
            uint32_t id = rd32(g_mem, p);
            if (id >= rd32(g_mem, STAFF_COUNT) || (int32_t)rd32(g_mem, index + id * 0x4fu) < 0 ||
                (int32_t)rd32(g_mem, index + id * 0x4fu + 4u) >= 0)
                continue;
            uint32_t pa[2] = {p, club};
            uint32_t before = call_this(manager, 0x4cbb60u, 2, pa) & 0xffu;
            if (rt_unwinding) return;
            if (before != 4u) continue;              /* 4: at the senior club of a linked side (0x4cbbf6) */
            call_this(manager, 0x4cc0f0u, 2, pa);
            if (rt_unwinding) return;
            uint32_t after = call_this(manager, 0x4cbb60u, 2, pa) & 0xffu;
            if (rt_unwinding) return;
            call_this(manager, 0x4cc0f0u, 2, pa);
            if (rt_unwinding) return;
            fprintf(stderr, "recomp: squad move test: club %u B side %u staff %u placement %u then %u, "
                    "B side slots %d, club slots %d\n", rd32(g_mem, club), rd32(g_mem, b), id, before, after,
                    squad_slots(b, p), squad_slots(club, p));
            return;
        }
    }
    fprintf(stderr, "recomp: squad move test: no senior club with a one-way B side and a one-contract player at placement 4\n");
}

/* A club in two key ids (open item 295, group club_keys). set_key_ids 0x601a40 gives C.D. Logrones, C.D. Logrones B,
 * C.D. Badajoz and C.D. Badajoz B two key ids each: 0x9d0170 and 0x9d04c8, 0x9d0174 and 0x9d04cc, 0x9d03b4 and 0x9d04c0,
 * 0x9d03b8 and 0x9d04c4. database_trim_club_table 0x5267c0 calls remap_club_key_id 0x60ba80 for each row in order
 * (0x526d94), and 0x60ba80 rewrites the first key id that holds the old row and returns. The second key id keeps the
 * old row, so it names another club after the trim. In a game on the October 2026 database Levante U.D. B took row
 * 3230, the old row of C.D. Logrones, and club_get_linked_b_team 0x540a50 gave it the pair of 0x9d04c8: old row 6679,
 * past the end of the trimmed table. match_day_can_quick_simulate 0x6b66a0 passed the club of that row to
 * human_manager_is_human_club 0x600a50 (0x6b6756), which read no nation and stopped at the box Database.cpp 13126
 * (0x53b3d0). Here every key id of the old row follows it. The rows go up in order and a new row is never above its
 * old row, so a key id that already moved never holds the old row of a later call. The chain of 0x60ba80 holds
 * 0x9d00a4..0x9d0524 without 0x9d0300 and 0x9d034c, and 0xb63c9c..0xb63d00. A saved game needs no fix: set_key_ids
 * runs again at its load (0x52d8fa). Only a season game takes the fix (decision L2): the base game, a 2001 start with
 * changing rules off, keeps the original's key ids. The trim runs while the game is made, so the rules are those of the
 * next new game. */
static int club_keys_season_game(void)
{
    if (!season_code_on()) return 0;
    return season_start_year() != SEASON_FIRST_YEAR || (season_game_on() ? season_changing_rules() : season_next_rules());
}

static void club_key_follow(uint32_t key, uint32_t old_row, uint32_t new_row)
{
    if (rd32(g_mem, key) != old_row) return;
    wr32(g_mem, key, new_row);
    if (getenv("RECOMP_SEASON_LOG"))
        fprintf(stderr, "recomp: club key %#x follows row %u to row %u\n", key, old_row, new_row);
}

void rt_club_key_twins(uint32_t old_row, uint32_t new_row, uint32_t hit)
{
    if (!hit || old_row == new_row || !club_keys_season_game() || !fixes_on_group(CLUB_KEYS)) return;
    for (uint32_t key = 0x9d00a4u; key <= 0x9d0524u; key += 4u)
        if (key != 0x9d0300u && key != 0x9d034cu) club_key_follow(key, old_row, new_row);
    for (uint32_t key = 0xb63c9cu; key <= 0xb63d00u; key += 4u) club_key_follow(key, old_row, new_row);
}

/* RECOMP_CLUB_KEY_TEST=1 (tests/seasons/club_key_twins_run.py, open item 295): at the first start report, the two key
 * ids of each of the four clubs, and the B side that club_get_linked_b_team 0x540a50 gives the clubs on the old rows. */
void rt_club_key_test(void)
{
    static int done;
    if (done || !getenv("RECOMP_CLUB_KEY_TEST")) return;
    done = 1;
    static const uint32_t pairs[4][2] = {{0x9d0170u, 0x9d04c8u}, {0x9d0174u, 0x9d04ccu}, {0x9d03b4u, 0x9d04c0u}, {0x9d03b8u, 0x9d04c4u}};
    static const uint32_t rows[] = {1215u, 1216u, 3230u, 6678u, 6679u};
    uint32_t clubs = rd32(g_mem, 0xae23b4u), count = rd32(g_mem, CLUB_COUNT);
    for (int i = 0; i < 4; i++)
        fprintf(stderr, "recomp: club key test: keys %#x %#x hold rows %d %d\n", pairs[i][0], pairs[i][1],
                (int)rd32(g_mem, pairs[i][0]), (int)rd32(g_mem, pairs[i][1]));
    for (size_t i = 0; i < sizeof rows / sizeof rows[0] && rows[i] < count; i++) {
        uint32_t club = clubs + rows[i] * 581u, args[3] = {club, 0u, 0u};
        uint32_t b = rt_callback(0x540a50u, 3, args);
        if (rt_unwinding) return;
        uint32_t row = b ? (b - clubs) / 581u : 0u;
        if (!b) fprintf(stderr, "recomp: club key test: row %u '%.50s' has no B side\n", rows[i], (const char *)(g_mem + club + 4u));
        else if (row >= count) fprintf(stderr, "recomp: club key test: row %u '%.50s' has a B side past the table, row %u\n", rows[i],
                                       (const char *)(g_mem + club + 4u), row);
        else fprintf(stderr, "recomp: club key test: row %u '%.50s' has the B side row %u '%.50s'\n", rows[i],
                     (const char *)(g_mem + club + 4u), row, (const char *)(g_mem + b + 4u));
    }
    fprintf(stderr, "recomp: club key test: done\n");
}

/* A loan to the club's own linked B side (open item 280, group squad_loans). CONTRACT_MANAGER::sign_loan_contract
 * 0x4d01e0 adds the player to the loan club (0x4d041f) and does not take him out of his own club's squad (it never
 * calls database_remove_staff_from_club 0x53c190), so a loaned player is in both squads. Nothing in the original stops
 * a loan from a club to its own linked B side (club_get_linked_b_team 0x540a50, a fixed table of pairs): no offer code
 * of TRANSFER_MANAGER tests the link. After such a loan Shortlist_Manager::assess_club_squad 0x83cb40 still moves him
 * between the two sides: move_to_reserves 0x4cc0f0 or move_to_first_team 0x4cc0b0 call
 * CONTRACT_MANAGER::toggle_squad_placement 0x4cbd20, which removes him from one side and adds him to the other
 * (database_add_staff_to_club 0x53bd10). He is already in both, so the add puts him in a second slot of the side he
 * goes to. In run C of the 0.7.2 full check FC Energie Cottbus II held one player in two squad slots on day 206 of
 * 2007, and MATCH_MAN::pick_team 0x6dbc50 stopped at its duplicate test (match_man..cpp 1225, 0x6dd3c1). With the
 * placement bit set after the loan the move went up instead and doubled him at FC Energie Cottbus. Here
 * toggle_squad_placement makes no move while the player is on loan from one side of a linked pair to the other (*out
 * cleared as at 0x4cbd3f). The callers: move_to_first_team and move_to_reserves (returns 0x4cc0df and 0x4cc11f, from
 * assess_club_squad, TRANSFER_MANAGER::add_config_loan 0x8bf61b and player_regen_sign_for_club 0x7a7098), which get 0;
 * the handlers of the staff profile and of the compare screen (issue 159; 0x888760 and 0x88a11d), which get 1,
 * because they read 0 as a full squad and show "There is no space in the squad" (0x88876d, text 0xa8b2cc); the
 * call at 0x4cb02b stays as it is (a player with a loan contract jumps past it, 0x4caf59..0x4caf71). The human
 * does not get the move at all: the profile build 0x870700 adds the Action item from get_squad_placement (0x8713c9,
 * the answers 1 to 4 at 0x8713ce..0x8715fa), and for such a loan that one call answers 0, so the menu has no item that
 * would do nothing. The loan contract is slot +4 of the contract index (sign_loan_contract 0x4d0444..0x4d0467), and end_loan
 * 0x4d07e0 sets it to -1 (0x4d0983..0x4d09aa), so the moves come back when the loan ends. */
#define TOGGLE_PROFILE_RET 0x888765u      /* staff_screens_handle_profile_action 0x888260 */
#define TOGGLE_COMPARE_RET 0x88a122u      /* staff_screens_handle_compare_action 0x889c30 */
#define TOGGLE_OTHER_RET 0x4cb030u        /* the call at 0x4cb02b */
#define PROFILE_MENU_RET 0x8713ceu        /* staff_screens_build_profile_screen 0x870700 */

static uint32_t contract_club(uint32_t manager, int32_t index)
{
    if (index < 0) return 0xffffffffu;
    return rd32(g_mem, rd32(g_mem, manager) + (uint32_t)index * 0x50u + 4u);
}

static uint32_t linked_side(uint32_t club)
{
    uint32_t args[3] = {club, 0, 1};
    return rt_callback(0x540a50u, 3, args);
}

/* 1 while the player is on loan from one side of a linked pair to the other. Keeps cpu. *own and *loan: the two clubs. */
static int loan_inside_pair(uint32_t manager, uint32_t staff, uint32_t *own, uint32_t *loan)
{
    uint32_t count = rd32(g_mem, STAFF_COUNT), clubs_n = rd32(g_mem, CLUB_COUNT);
    uint32_t id = staff >= 0x10000u ? rd32(g_mem, staff) : 0xffffffffu;
    if (!fixes_on_group(SQUAD_LOANS) || !manager || id >= count) return 0;
    uint32_t index = rd32(g_mem, CONTRACT_INDEX) + id * 0x4fu;
    uint32_t own_id = contract_club(manager, (int32_t)rd32(g_mem, index));
    uint32_t loan_id = contract_club(manager, (int32_t)rd32(g_mem, index + 4u));
    if (own_id >= clubs_n || loan_id >= clubs_n || own_id == loan_id) return 0;
    uint32_t clubs = rd32(g_mem, CLUBS);
    *own = clubs + own_id * CLUB_SIZE;
    *loan = clubs + loan_id * CLUB_SIZE;
    Cpu saved = cpu;
    uint32_t a = linked_side(*own);
    uint32_t b = rt_unwinding ? 0 : linked_side(*loan);
    if (rt_unwinding) return 0;
    cpu = saved;
    return a == *loan || b == *own;
}

void o_004cbb60(void);
RT_WRAP(004cbb60, squad_loan_menu)        /* get_squad_placement, thiscall (staff, club), ret 8 */
{
    uint32_t own, loan;
    if (rd32(g_mem, cpu.esp) != PROFILE_MENU_RET || !loan_inside_pair(cpu.ecx, rt_arg(0), &own, &loan)) {
        if (!rt_unwinding) o_004cbb60();
        return;
    }
    cpu.eax &= 0xffffff00u;               /* no placement: the profile adds no move item */
    cpu.esp += 4 + 8;
}

void o_004cbd20(void);
RT_WRAP(004cbd20, squad_loan_move)
{
    uint32_t ret = rd32(g_mem, cpu.esp), staff = rt_arg(0), out = rt_arg(2), own, loan;
    if (ret == TOGGLE_OTHER_RET || !loan_inside_pair(cpu.ecx, staff, &own, &loan)) {
        if (!rt_unwinding) o_004cbd20();
        return;
    }
    uint32_t id = rd32(g_mem, staff);
    static uint32_t logged[64];         /* one line per player (the assessment repeats while the loan lasts) */
    static unsigned next;
    int seen = 0;
    for (unsigned k = 0; k < 64u; k++) seen |= logged[k] == id + 1u;
    if (!seen) {
        logged[next++ % 64u] = id + 1u;
        char own_name[64];
        snprintf(own_name, sizeof own_name, "%s", log_name(own + 4u));
        fprintf(stderr, "recomp: squad loan: staff %u is on loan from %s to %s, a side of the same pair: no squad "
                        "move\n", id, own_name, log_name(loan + 4u));
    }
    if (out) wr32(g_mem, out, 0);
    cpu.eax = ret == TOGGLE_PROFILE_RET || ret == TOGGLE_COMPARE_RET;   /* no move: thiscall, ret 0xc */
    cpu.esp += 4 + 0xc;
}

/* RECOMP_SQUAD_LOAN_TEST=1 (tests/seasons/squad_loan_run.py, issue 159): at the first start report the test loans a
 * player of the first senior club with a one-way linked B side to that B side, as TRANSFER_MANAGER::add_config_loan
 * does (STAFF_CONTRACT 0x85e590, the end date at +0x2d, sign_loan_contract 0x4d01e0 at 0x8bf5b4). The human manager
 * then takes each side in turn and uses the move item of the staff profile's Action menu: the profile opens
 * (staff_screens_open_staff_profile 0x8702a0), the build 0x870700 adds the item, and its handler 0x888260 gets the
 * index of the widget of item type 3 (read at 0x888719..0x88871c, the call of toggle_squad_placement at 0x888760). A
 * line gives the item (-1: the menu has none) and the slots of each side after each move. */
#define HUMAN_STAFF 0xb74c26u            /* per channel, 0x300 bytes apart: the human manager (0x8882cd) */
#define SCREEN_CHANNEL 0xb77c7au         /* s16 */
#define SCREEN_DISPLAY 0xb74c4cu         /* per channel: the screen manager (0x888713) */
#define WIDGET_ITEM_TYPE 0xba966u        /* per widget, 0x18c bytes apart: the item type (0x88871c) */
#define WIDGET_COUNT 0x12e9a0u           /* s16 on the screen manager: the widgets of this build (0x55f8ef, 0x55f999) */

static int loan_test_click(uint32_t human, uint32_t club, uint32_t player)
{
    uint32_t channel = (uint32_t)(int16_t)rd16(g_mem, SCREEN_CHANNEL) * 0x300u;
    uint32_t old_club = rd32(g_mem, human + 0x39u);
    wr32(g_mem, human + 0x39u, club);
    uint32_t open[5] = {player, 1, 1, 0, 0};    /* the Profile tab, as the name click at 0x4173a6 */
    rt_callback(0x8702a0u, 5, open);
    if (rt_unwinding) return -1;
    screen_sweep_build_now();
    if (rt_unwinding) return -1;
    uint32_t display = rd32(g_mem, SCREEN_DISPLAY + channel);
    int found = -1;
    uint32_t widgets = rd16(g_mem, display + WIDGET_COUNT);
    for (uint32_t k = 0; k < widgets && k < 0x4afu && found < 0; k++)
        if ((int16_t)rd16(g_mem, display + WIDGET_ITEM_TYPE + k * 0x18cu) == 3) found = (int)k;
    if (found >= 0) {
        uint32_t click[1] = {(uint32_t)found};
        rt_callback(0x888260u, 1, click);
        if (rt_unwinding) return -1;
    }
    wr32(g_mem, human + 0x39u, old_club);
    return found;
}

/* One loan and four moves: a player of `from` goes on loan to `to`, the other side of a linked pair. 0: no such player. */
static int loan_test_case(uint32_t human, uint32_t from, uint32_t to)
{
    uint32_t manager = rd32(g_mem, CONTRACTS), index = rd32(g_mem, CONTRACT_INDEX);
    for (uint32_t k = 0; k < 50u; k++) {
        uint32_t p = rd32(g_mem, from + 0xd7u + 4u * k);
        if (!p || rd32(g_mem, p + 0x39u) != from || !rd32(g_mem, p + 0x61u)) continue;
        uint32_t id = rd32(g_mem, p);
        if (id >= rd32(g_mem, STAFF_COUNT) || (int32_t)rd32(g_mem, index + id * 0x4fu) < 0 ||
            (int32_t)rd32(g_mem, index + id * 0x4fu + 4u) >= 0 || squad_slots(to, p))
            continue;
        /* His contract must be with `from`: a player of a B side can hold his contract at the senior club (item 187). */
        if (contract_club(manager, (int32_t)rd32(g_mem, index + id * 0x4fu)) != rd32(g_mem, from)) continue;
        uint32_t contract = rt_alloc(0x50);
        uint32_t make[6] = {p, to, 0xae2c90u, 0, 1, 1};
        call_this(contract, 0x85e590u, 6, make);
        if (rt_unwinding) return 0;
        memcpy(g_mem + contract + 0x2du, g_mem + 0xae2c90u, 5);
        wr16(g_mem, contract + 0x2fu, (uint16_t)(rd16(g_mem, 0xae2c92u) + 1u));   /* the loan ends in a year */
        uint32_t sign[1] = {contract};
        call_this(manager, 0x4d01e0u, 1, sign);
        if (rt_unwinding) return 0;
        fprintf(stderr, "recomp: squad loan test: staff %u loaned from club %u to club %u, slots %d and %d\n", id,
                rd32(g_mem, from), rd32(g_mem, to), squad_slots(from, p), squad_slots(to, p));
        for (int move = 0; move < 4; move++) {
            uint32_t side = move % 2 ? to : from;
            int item = loan_test_click(human, side, p);
            if (rt_unwinding) return 0;
            fprintf(stderr, "recomp: squad loan test: staff %u move %d by the manager of club %u: item %d, slots %d "
                    "and %d\n", id, move, rd32(g_mem, side), item, squad_slots(from, p), squad_slots(to, p));
        }
        return 1;
    }
    return 0;
}

void rt_squad_loan_test(void)
{
    static int done;
    if (done || !getenv("RECOMP_SQUAD_LOAN_TEST")) return;
    done = 1;
    uint32_t clubs = rd32(g_mem, CLUBS), flag = rt_alloc(4), count = rd32(g_mem, CLUB_COUNT);
    uint32_t human = rd32(g_mem, HUMAN_STAFF + (uint32_t)(int16_t)rd16(g_mem, SCREEN_CHANNEL) * 0x300u);
    if (human < 0x10000u || !clubs || !rd32(g_mem, CONTRACTS)) {
        fprintf(stderr, "recomp: squad loan test: no human manager\n");
        return;
    }
    /* A loan along the link of 0x540a50 and one against it. The data holds no one-contract player of a true B side, so
       the second loan is between clubs that the fixed table of 0x540a50 links, whatever their names. */
    int down = 0, up = 0;
    for (uint32_t c = 0; c < count && !(down && up); c++) {
        uint32_t club = clubs + c * CLUB_SIZE;
        wr32(g_mem, flag, 0);
        uint32_t args[3] = {club, flag, 1};
        uint32_t b = rt_callback(0x540a50u, 3, args);
        if (rt_unwinding) return;
        if (!b || !rd32(g_mem, flag)) continue;          /* the flag: this club is the senior side (0x4cbbe6) */
        if (!down) down = loan_test_case(human, club, b);
        if (rt_unwinding) return;
        if (!up) up = loan_test_case(human, b, club);
        if (rt_unwinding) return;
    }
    fprintf(stderr, "recomp: squad loan test: done, %d loans\n", down + up);
}
