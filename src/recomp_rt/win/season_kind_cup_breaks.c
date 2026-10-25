/* Rule kind "cup_breaks" of the season change (season_change.h): the domestic cups of the original play an
 * edition only in the seasons that the rules table holds them (0.6.0 slice 21b, docs/research/
 * 2026-10-18-cup-breaks.md). Examples: the Coupe de la Ligue is not held from 2020/21, the Welsh Premier Cup from
 * 2008/09, the Norwegian Cup in 2020, the Leinster Cup from 2002/03 to 2008/09.
 *
 * The edition of a cup is its season year (+0x40). Every class below adds 1 to +0x40 in start_new_season before
 * it builds the rules, the round table and the dates of the new edition (FraLgeCup 0x5bd4be, WelPrmCup 0x928261,
 * NorCup 0x78fc2e, and the others in the table), and none of them calls start_new_season outside the season pass
 * (no call of vtable +0x8 in their files; the pass call is at 0x5d5d1c). season_cups.h maps an edition to its row.
 *
 * In a game (changing rules on, or the start season): in the before hook of start_new_season of season S, a cup
 * whose edition S the table does not hold calls season_change_skip(). The cup keeps its finished edition, makes
 * no fixtures and plays no matches (docs/research/2026-10-18-club-world-idle.md, section 3.2: all_stages_complete
 * stays true, the season-end work runs once). The pass asks again on the next update day, and the kind skips
 * again while the table does not hold the cup. A cup that the table holds again resumes: its year is the
 * edition before the gap, so the hook sets +0x40 to S - 1, and start_new_season builds edition S with dates in
 * the future. Why a skip and not a wait (+0x40 at the next edition, as season_kind_club_world.c does): six of
 * these classes make the fixtures of the new edition inside start_new_season (League::create_fixtures 0x6835c0
 * called at 0x9282f8 by WelPrmCup, and by KorLeagueCup, IreLgeCup, ItaCCup, NirLgeCup and WelLgeCup), and every
 * class picks its clubs there (FraLgeCup 0x5bd4d0). A wait would put the fixtures of the next edition into the
 * clubs' fixture lists a season or more early, with clubs of the season before.
 *
 * At a new game of a season game (start year not 2001; a 2001 game keeps the original's 2001/02): the
 * constructor builds the first edition from its year argument (thiscall: year, record; ret 8, every constructor
 * below). When the table does not hold that edition:
 *   - with changing rules on, the wrap gives the constructor the year of the next edition that the table holds, so
 *     the cup waits (its dates lie in the future; the club world doc, section 1);
 *   - with changing rules off, the cup stays off for the whole game (Q232: the rules of the start season stay), as
 *     a cup that the table never holds again: both get SEASON_CUPS_NEVER (2999).
 * For such an object the League::create_fixtures call of its constructor makes no fixtures (wrap below), so no
 * fixture of the year 2999 reaches a club's list, and a League class gets +0x45 (drawn) 0 after its constructor:
 * League::check_complete 0x689ad0 then returns false at 0x689af6 and League::update_for_date 0x6847c0 does nothing
 * at 0x684811 (IreSuperCup::update_for_date 0x63b300 draws only when +0x40 is the year at [0xde1f58]+0x439,
 * 0x63b324..0x63b331). Without that, a League with no fixtures could count as complete: the played counts of its
 * table rows (+0x11) are compared with the matches per club +0xa7 (0x689b47..0x689b58), and a table with no rows
 * passes. A Cup class never draws (its draw dates lie in 2999, Cup::update_for_date 0x51fa45..0x51fa6d), so
 * Cup::check_complete 0x521e00 never sees +0xb1 set. So the pass never calls start_new_season for the object
 * (all_stages_complete 0x48ce10 is false), and it cannot make the edition of 3000 with fixtures.
 *
 * The base game (2001/02, changing rules off) and RECOMP_SEASON_CODE=off get none of it (Q152, Q342): no hook
 * runs there, and the wraps test season_game(). RECOMP_CUPS=off turns this kind off too (a test switch of
 * slice 21). */
#include "win.h"
#include "season_records.h"
#include "season.h"
#include "season_change.h"
#include "season_cups.h"
#include "fixture_season_end.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YEAR_AT 0x40u                   /* s16 season year of a competition object */
#define NAME_AT 4u                      /* the long name of the competition record (object +4) */
#define DRAWN_AT 0x45u                  /* u32 1 when a League has drawn its fixtures (Comp::Comp 0x48c5bb sets 1) */
#define LEAGUE_CHECK_COMPLETE 0x689ad0u /* vtable +0x10 of every League class here */

/* The classes with a season that the table does not hold (rules table rows with not_held, comp_file of the
 * original, tier cup or super cup) and their constructors. The note says the base class and whether the
 * constructor makes fixtures itself ("with fixtures": a call of League::create_fixtures 0x6835c0 inside the
 * constructor, for example 0x927970 in WelPrmCup), and the editions that the table does not hold. */
typedef struct {
    const char *file;
    uint32_t ctor;
} BreakClass;

static const BreakClass classes[] = {
    {"bel_super", 0x420ef0u},           /* Cup, not held 2020/21 */
    {"bra_champ_cup", 0x4259a0u},       /* League, from 2003 */
    {"fra_lge_cup", 0x5bc980u},         /* Cup, from 2020/21 */
    {"ger_lge_cup", 0x5dc0e0u},         /* Cup, from 2008/09 */
    {"gre_super", 0x5ec040u},           /* Cup, held 2007/08 and from 2025/26 */
    {"hol_super", 0x5f8820u},           /* Cup, not held 2020/21 */
    {"ire_leinster_cup", 0x6364a0u},    /* Cup, 2002/03 to 2008/09 and 2019/20 to 2021/22 */
    {"ire_lge_cup", 0x636dd0u},         /* League with fixtures, 2002/03 and from 2020/21 */
    {"ire_munster_cup", 0x6387a0u},     /* Cup, 2020/21 */
    {"ire_pres_cup", 0x639280u},        /* Cup, 2003/04 to 2012/13 and 2019/20 */
    {"ire_super_cup", 0x63acd0u},       /* League, from 2002/03 */
    {"ita_c1_super", 0x63c5e0u},        /* Cup, 2019/20 */
    {"ita_c_cup", 0x63cc40u},           /* League with fixtures, 2020/21 */
    {"jap_emp_cup", 0x661d30u},         /* Cup, 2026 */
    {"jap_j_cup", 0x665380u},           /* Cup, 2026 */
    {"jap_super", 0x665d70u},           /* Cup, 2026 */
    {"kor_league_cup", 0x66bf00u},      /* League with fixtures, 2003 and from 2012 */
    {"kor_super_cup", 0x66d820u},       /* Cup, 2003 and 2007 to 2025 */
    {"nir_charity", 0x789df0u},         /* Cup, to 2013/14 and 2018/19 to 2021/22 */
    {"nir_lge_cup", 0x78c780u},         /* League with fixtures, 2020/21 */
    {"nor_cup", 0x78f020u},             /* Cup, 2020 */
    {"pol_lge_cup", 0x7ca4e0u},         /* Cup, 2002/03 to 2005/06 and from 2009/10 */
    {"pol_super", 0x7cc920u},           /* Cup, 2002/03, 2003/04, 2005/06, 2011/12, 2013/14 */
    {"sco_chal_cup", 0x7ec980u},        /* Cup, 2020/21 */
    {"swe_cup", 0x88cbb0u},             /* Cup, 2012 */
    {"usa_open_cup", 0x90aeb0u},        /* Cup, 2020 and 2021 */
    {"wel_cup", 0x924420u},             /* Cup, 2020/21 */
    {"wel_lge_cup", 0x925e90u},         /* League with fixtures, 2020/21 */
    {"wel_prm_cup", 0x9277f0u},         /* League with fixtures, from 2008/09 */
};
#define CLASS_COUNT ((int)(sizeof classes / sizeof classes[0]))

static const BreakClass *class_of(const char *file)
{
    for (int i = 0; file && i < CLASS_COUNT; i++)
        if (!strcmp(classes[i].file, file)) return &classes[i];
    return NULL;
}

static int logging(void)
{
    const char *v = getenv("RECOMP_SEASON_CHANGE_LOG");
    return v && *v;
}

/* RECOMP_CUPS=off: the test switch of the cup kinds (season_kind_cups.c). */
static int cups_off(void)
{
    const char *v = getenv("RECOMP_CUPS");
    return v && !strcmp(v, "off");
}

/* 1 when a game of the season code runs or is made: a start year that is not 2001, or changing rules on. */
static int season_game(void)
{
    return season_code_on() && !cups_off() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

/* ---- New game: the constructors ---- */

static uint32_t never_obj;              /* the object whose constructor runs with SEASON_CUPS_NEVER */

static void ctor(const char *file, void (*original)(void))
{
    uint32_t self = cpu.ecx;
    if (season_game() && season_change_is_creating() && season_start_year() != SEASON_FIRST_YEAR) {
        uint32_t slot = cpu.esp + 4;                        /* thiscall (year, record), ret 8 */
        uint32_t arg = rd32(g_mem, slot);
        int year = (int16_t)(arg & 0xffffu);
        int next = season_cups_next(file, year);
        if (next && next != year && !season_changing_rules()) next = SEASON_CUPS_NEVER;     /* Q232 */
        if (next && next != year) {
            wr32(g_mem, slot, (arg & 0xffff0000u) | ((uint32_t)next & 0xffffu));
            if (next == SEASON_CUPS_NEVER) never_obj = self;
            fprintf(stderr, "recomp: cup_breaks: %s waits for %d (edition %d not held, start season %d)\n", file,
                    next, year, season_start_year());
        }
    }
    original();
    if (never_obj && !rt_unwinding && rd32(g_mem, rd32(g_mem, self) + 0x10) == LEAGUE_CHECK_COMPLETE)
        wr32(g_mem, self + DRAWN_AT, 0);                    /* never complete: see the module comment */
    never_obj = 0;
}

void o_00420ef0(void);
void o_004259a0(void);
void o_005bc980(void);
void o_005dc0e0(void);
void o_005ec040(void);
void o_005f8820(void);
void o_006364a0(void);
void o_00636dd0(void);
void o_006387a0(void);
void o_00639280(void);
void o_0063acd0(void);
void o_0063c5e0(void);
void o_0063cc40(void);
void o_00661d30(void);
void o_00665380(void);
void o_00665d70(void);
void o_0066bf00(void);
void o_0066d820(void);
void o_00789df0(void);
void o_0078c780(void);
void o_0078f020(void);
void o_007ca4e0(void);
void o_007cc920(void);
void o_007ec980(void);
void o_0088cbb0(void);
void o_0090aeb0(void);
void o_00924420(void);
void o_00925e90(void);
void o_009277f0(void);

/* The recompiler reads the RT_WRAP lines as they are, so each is written out. */
RT_WRAP(00420ef0, bel_super_ctor) { ctor("bel_super", o_00420ef0); }
RT_WRAP(004259a0, bra_champ_cup_ctor) { ctor("bra_champ_cup", o_004259a0); }
RT_WRAP(005bc980, fra_lge_cup_ctor) { ctor("fra_lge_cup", o_005bc980); }
RT_WRAP(005dc0e0, ger_lge_cup_ctor) { ctor("ger_lge_cup", o_005dc0e0); }
RT_WRAP(005ec040, gre_super_ctor) { ctor("gre_super", o_005ec040); }
RT_WRAP(005f8820, hol_super_ctor) { ctor("hol_super", o_005f8820); }
RT_WRAP(006364a0, ire_leinster_cup_ctor) { ctor("ire_leinster_cup", o_006364a0); }
RT_WRAP(00636dd0, ire_lge_cup_ctor) { ctor("ire_lge_cup", o_00636dd0); }
RT_WRAP(006387a0, ire_munster_cup_ctor) { ctor("ire_munster_cup", o_006387a0); }
RT_WRAP(00639280, ire_pres_cup_ctor) { ctor("ire_pres_cup", o_00639280); }
RT_WRAP(0063acd0, ire_super_cup_ctor) { ctor("ire_super_cup", o_0063acd0); }
RT_WRAP(0063c5e0, ita_c1_super_ctor) { ctor("ita_c1_super", o_0063c5e0); }
RT_WRAP(0063cc40, ita_c_cup_ctor) { ctor("ita_c_cup", o_0063cc40); }
RT_WRAP(00661d30, jap_emp_cup_ctor) { ctor("jap_emp_cup", o_00661d30); }
RT_WRAP(00665380, jap_j_cup_ctor) { ctor("jap_j_cup", o_00665380); }
RT_WRAP(00665d70, jap_super_ctor) { ctor("jap_super", o_00665d70); }
RT_WRAP(0066bf00, kor_league_cup_ctor) { ctor("kor_league_cup", o_0066bf00); }
RT_WRAP(0066d820, kor_super_cup_ctor) { ctor("kor_super_cup", o_0066d820); }
RT_WRAP(00789df0, nir_charity_ctor) { ctor("nir_charity", o_00789df0); }
RT_WRAP(0078c780, nir_lge_cup_ctor) { ctor("nir_lge_cup", o_0078c780); }
RT_WRAP(0078f020, nor_cup_ctor) { ctor("nor_cup", o_0078f020); }
RT_WRAP(007ca4e0, pol_lge_cup_ctor) { ctor("pol_lge_cup", o_007ca4e0); }
RT_WRAP(007cc920, pol_super_ctor) { ctor("pol_super", o_007cc920); }
RT_WRAP(007ec980, sco_chal_cup_ctor) { ctor("sco_chal_cup", o_007ec980); }
RT_WRAP(0088cbb0, swe_cup_ctor) { ctor("swe_cup", o_0088cbb0); }
RT_WRAP(0090aeb0, usa_open_cup_ctor) { ctor("usa_open_cup", o_0090aeb0); }
RT_WRAP(00924420, wel_cup_ctor) { ctor("wel_cup", o_00924420); }
RT_WRAP(00925e90, wel_lge_cup_ctor) { ctor("wel_lge_cup", o_00925e90); }
RT_WRAP(009277f0, wel_prm_cup_ctor) { ctor("wel_prm_cup", o_009277f0); }

/* League::create_fixtures 0x6835c0 (thiscall, no arguments, ret at 0x683edf; returns non-zero on success, which
 * the callers test, for example 0x927975). While a constructor runs with SEASON_CUPS_NEVER it makes no fixture and
 * returns 1: for the object itself and for the stage objects that the constructor makes (BraChampCup 0x4259a0
 * makes its group stages in the constructor, and a run of a 2008 game with Brazil showed a fixture block of 2999
 * until this covered them). Every other call runs the original. The one wrap of 0x6835c0 is in
 * season_uefa_league.c (slices 21b and 25): it calls this first, and returns 1 at once when this returns 1. */
int season_cup_breaks_no_fixtures(void)
{
    if (!never_obj) return 0;
    if (logging()) fprintf(stderr, "recomp: cup_breaks: no fixtures for the cup that waits for ever (0x%x)\n", cpu.ecx);
    return 1;
}

/* BraChampCup::start_preliminary_stage 0x427320 (thiscall, no arguments, ret at 0x42741d): it makes the fixtures
 * of the Copa dos Campeoes (League::create_fixtures at 0x42736b). The Brazilian league starts it from outside the
 * season pass: bra_nat_first_fill_champions_cup_entrants 0x42bba0 fills the entrants and calls it at 0x42be2a
 * on the object of [0xadadfc][[0x9cf9b8]], whatever the cup's year. So a cup that waits for ever (a 2008 game:
 * six fixtures of 2999, run n2008 of tests/seasons/cup_breaks_run.py) or that skips its seasons (it would make
 * fixtures of its old year, which the fixture manager moves to new dates, FIXTURE_MANAGER::add_fixture 0x5aa709..
 * 0x5aa729) would play again. The wrap does not run it while the table holds neither the edition of the game
 * year nor the next one (the cup is a calendar-year cup), or the cup waits for ever. */
void o_00427320(void);
RT_WRAP(00427320, cup_breaks_bra_champ_start)
{
    int year = (int16_t)rd16(g_mem, cpu.ecx + YEAR_AT);
    int now = (int16_t)rd16(g_mem, 0xae2c92u);              /* the game year */
    if (season_game() && (year >= SEASON_CUPS_NEVER || (season_cups_held("bra_champ_cup", now) == 0 &&
                                                         season_cups_held("bra_champ_cup", now + 1) == 0))) {
        static int logged_year;                             /* the caller runs each day: one line a year */
        if (logged_year != now) {
            logged_year = now;
            fprintf(stderr, "recomp: cup_breaks: bra_champ_cup does not start (year %d, game year %d)\n", year, now);
        }
        rt_ret_stdcall(cpu.eax, 0);
        return;
    }
    o_00427320();
}

/* ---- The season change ---- */

static const char *name_of(uint32_t comp, char *buf, size_t n)
{
    uint32_t rec = rd32(g_mem, comp + NAME_AT);
    snprintf(buf, n, "%.50s", rec >= 0x10000u && rec < 0xfff00000u ? win_str(rec + NAME_AT) : "");
    return buf;
}

/* The edition that starts in the season of the change: the season S of the hook (season_change.h). For every
 * class here S is the year that start_new_season gives the cup (+0x40 + 1) when the cup did not skip a season
 * (log line "starts" of RECOMP_SEASON_CHANGE_LOG, docs/research/2026-10-18-cup-breaks.md). */
static void breaks_before(const SeasonChange *c)
{
    if (!season_game() || !c->started) return;
    const BreakClass *k = class_of(c->file);
    if (!k) return;
    int edition = c->season;
    int held = season_cups_held(k->file, edition);
    int year = (int16_t)rd16(g_mem, c->comp + YEAR_AT);
    if (logging())
        fprintf(stderr, "recomp: cup_breaks: %s season %d, year %d, held %d\n", k->file, edition, year, held);
    if (held < 0) return;
    if (!held) {
        season_change_skip();
        fprintf(stderr, "recomp: cup_breaks: %s is not held in %d (year stays %d)\n", k->file, edition, year);
        return;
    }
    if (year + 1 < edition) {                               /* it skipped a season before: resume in step */
        wr16(g_mem, c->comp + YEAR_AT, (uint16_t)(edition - 1));
        fprintf(stderr, "recomp: cup_breaks: %s resumes in %d (year %d to %d)\n", k->file, edition, year,
                edition - 1);
    }
}

/* One sentence when the table starts or stops holding the cup in the season of the change (Q258). It runs for a
 * cup that starts and for one that skips or waits (started 0), so a cup that comes back gets its sentence too. */
static void breaks_after(const SeasonChange *c)
{
    if (!season_game()) return;
    const BreakClass *k = class_of(c->file);
    if (!k) return;
    int s = c->season;
    int now = season_cups_held(k->file, s), before = season_cups_held(k->file, s - 1);
    if (now < 0 || before < 0 || now == before) return;
    char name[64], when[32];
    name_of(c->comp, name, sizeof name);
    if (!*name) return;
    if (season_cups_calendar(k->file)) snprintf(when, sizeof when, "in %d", s);
    else snprintf(when, sizeof when, "in the %d/%02d season", s, (s + 1) % 100);
    if (now)
        season_change_news("The %s is played again %s.", name, when);
    else if (season_cups_next(k->file, s) == SEASON_CUPS_NEVER)
        season_change_news("The %s is no longer played.", name);
    else
        season_change_news("The %s is not played %s.", name, when);
}

/* Test log (RECOMP_SEASON_CHANGE_LOG): the year of each block of the fixture store [0xae2a58] (+0x1130 s16 year,
 * +0x112c the next block, a ring; holiday_next.c reads them the same way). A query for a date makes the block of
 * its year too (fix_man_comp_has_fixture_on_date 0x5ae6d0 calls 0x5abf10 at 0x5ae799), so a block of 2999 can
 * exist with no fixture in it; the wrap of add_fixture below logs each fixture itself. It only reads memory. */
static void log_fixture_years(const char *when)
{
    /* Also with RECOMP_CUPS=off, so a run can tell the fixtures of other kinds from those of this one. */
    if (!logging() || !season_code_on()) return;
    char buf[512];
    int n = snprintf(buf, sizeof buf, "recomp: cup_breaks: fixture years %s:", when);
    uint32_t first = rd32(g_mem, 0xae2a58u), b = first;
    for (int i = 0; b >= 0x10000u && b < 0xfff00000u && i < 40 && n < (int)sizeof buf - 16; i++) {
        n += snprintf(buf + n, sizeof buf - (size_t)n, " %d", (int16_t)rd16(g_mem, b + 0x1130));
        b = rd32(g_mem, b + 0x112c);
        if (b == first) break;
    }
    fprintf(stderr, "%s\n", buf);
}

/* FIXTURE_MANAGER::add_fixture 0x5aa680 (thiscall: the fixture, then a flag; the fixture's year at +0x28 and day
 * at +0x2a, as insert_fixture 0x5ab259 and 0x5ab2f0 read them). Test log only: a fixture of the year 2999 or later,
 * or any fixture while a constructor runs with SEASON_CUPS_NEVER, gives a line "fixture added", which
 * tests/seasons/cup_breaks_run.py does not allow. The call runs the original through fixture_season_end.c (O288). */
void season_playoffs_fixture_log(uint32_t f);
void o_005aa680(void);
RT_WRAP(005aa680, cup_breaks_add_fixture)
{
    season_records_fixture_log(rt_arg(0), rd32(g_mem, cpu.esp));     /* test log of slice 15b's fix */
    season_playoffs_fixture_log(rt_arg(0));
    if (logging() && season_code_on()) {
        uint32_t f = rt_arg(0);
        int year = f >= 0x10000u && f < 0xfff00000u ? (int16_t)rd16(g_mem, f + 0x28) : 0;
        if (year >= SEASON_CUPS_NEVER || never_obj)
            fprintf(stderr, "recomp: cup_breaks: fixture added for %d (call from 0x%x)%s\n", year,
                    rd32(g_mem, cpu.esp),
                    never_obj ? " in a constructor that waits for ever" : "");
    }
    fixture_season_end_add_fixture();
}

static void breaks_new_game(int start_year)
{
    (void)start_year;
    log_fixture_years("new game");
}

static void breaks_season(int season)
{
    char when[32];
    snprintf(when, sizeof when, "season %d", season);
    log_fixture_years(when);
}

const SeasonRuleKind season_kind_cup_breaks = {"cup_breaks", breaks_new_game, NULL, breaks_season, NULL,
                                               breaks_before, breaks_after};
