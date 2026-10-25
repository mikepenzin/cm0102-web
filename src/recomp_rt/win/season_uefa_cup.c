/* The UEFA Cup with a group stage (0.6.0 slice 22, change point U2: 2004/05 to 2008/09), season_uefa.h.
 * Evidence: docs/research/2026-10-17-uefa-formats.md. Only when season_uefa_row gives a row with UEFA Cup groups:
 * never in the base game or with RECOMP_SEASON_CODE=off.
 *
 * The original UEFA Cup (Uefa_Cup, vtable 0x970790) is a Cup of one stage: the constructor 0x9022e0 sets the
 * stage count +0x2c to 0 and the current stage +0x30 to -1, and its round table (create_round_table 0x9025c0)
 * holds a qualifying round, rounds 1 to 4, the quarter-finals, the semi-finals and the final. Its entry table
 * +0xa7 (6 bytes an entry, club at +0) holds +0x3e = 145 slots: the seeded clubs, sorted with the qualifying
 * round first (build_entrants 0x902b90), then 3 slots for the Intertoto Cup winners (count - 0x1b, 0x633deb), 16
 * for the Champions League third qualifying round losers (count - 0x18, 0x585a02) and 8 for the Champions League
 * group thirds (count - 8 to count - 1, 0x586282). A round takes +0x1c new clubs from slot +0x1e.
 *
 * The format with groups keeps the entry table. Its round table has three rounds: Q1, Q2 and R1, which take the
 * seeded clubs, the Intertoto winners and the Champions League losers in slot order; the slots of the group
 * thirds stay for the knockout stage. The sizes: R1 and Q2 from the row, Q1 so that the three rounds take every
 * slot before the thirds (Q1/2 + Q2/2 + R1 = count - 8 - b2/2, b2 = +0xb2, the clubs of the original's extra
 * round, 0x902c85; with b2 > 0 the original's format stays). Then, as the Champions League does (its stages,
 * season_uefa.c), the object holds 9 stages: 8 groups of 5 clubs that meet once (League_Stage 0x68b100) in
 * stages 0 to 7, and a knockout stage of 32 clubs (Cup_Stage 0x522e00) in stage 8: the top three of each group
 * and the 8 Champions League group thirds. The Cup base plays the stages: Cup::update_for_date 0x51f890 calls
 * slot 10 when the rounds of the Cup end and when a stage ends while +0x30 < +0x2c - 1. Slot 10 of the UEFA
 * Cup is the empty function 0x5a8f60 (a "ret" that many vtables share), so its wrap acts only for a UEFA Cup
 * object with 9 stages. The save keeps +0x2c, +0x30 and the stages (see the wrap of Cup::load_from_disk).
 *
 * Slice 24, part b: the Conference League is a second object of this class (season_comps.c). Each wrap acts on
 * both by their kind (season_uefa_cup_kind, by record): the Conference League takes the ecl_* fields of the row,
 * its own entrants and qualifying rounds (season_uefa_qual.c), and for its knockout round play-offs the 8 thirds
 * of the Europa League groups. The format of 2004/05 to 2008/09 and the Intertoto hand-over are the Europa
 * League's only. */
#include "win.h"
#include "season.h"
#include "season_uefa.h"
#include "season_cups.h"
#include "season_uefa_league.h"
#include "season_playoffs.h"
#include "season_load.h"

#include <stdio.h>
#include <string.h>

#define COMP_TABLE 0xadadfcu
#define KEY_UC 0x9cf6f0u
#define UC_VTABLE 0x00970790u
#define RULING_BODIES 0xae28f0u

#define F_LEAGUE_ROW 0x006826d0u        /* League::get_table_row, thiscall (index) */
#define F_RANDOM 0x0090cfc0u            /* utils_random, cdecl (range) */
#define F_CALLOC 0x00945501u            /* cdecl (count, size) */
#define F_MALLOC 0x00944e46u            /* cdecl (size) */
#define F_FREE 0x009452cau              /* cdecl (block) */
#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */
#define F_QSORT 0x00945333u             /* qsort, cdecl (base, count, size, compare), as 0x585435 */
#define F_SEED_ORDER 0x00586f60u        /* european_cup_compare_clubs_by_seed: European seed, then reputation */
#define F_BOOKINGS 0x007e9180u          /* RulingBody::reset_bookings_for_club, thiscall (club) */
#define F_LEAGUE_STAGE 0x0068b100u      /* League_Stage::League_Stage, thiscall, 22 arguments */
#define F_GROUP_NEWS 0x00684230u        /* League::send_group_draw_news, thiscall () */
#define F_CUP_STAGE 0x00522e00u         /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u      /* Cup::set_up_round, thiscall (round) */
#define F_DRAW_DATE 0x00521e60u         /* cup_set_round_draw_date, cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u        /* cup_set_round_match_date, cdecl (..., weekday, time, year, venue) */
#define F_GROUP_DATE 0x0068a160u        /* league_round_set_date, cdecl (table, round, day, month, year+, weekday, time, year, 0) */
#define F_GROUP_SLOT 0x0068a1c0u        /* league_round_set_match_slot, cdecl (table, round, slot, weekday, kind, option, value) */

#define ROUND_SIZE 0x68u                /* a round of a Cup round table */
#define GROUP_ROUND_SIZE 0x41u          /* a match day of a League round table (0x68a160) */
#define MAX_STAGES 13                   /* the stage array (ensure_stage_array), as the Champions League's */
#define MAX_GROUPS 12
#define MAX_GROUP_CLUBS 5
#define MAX_GROUP_DAYS 6
#define KO_CLUBS 32
#define KO_ROUNDS 5
#define KO_TIES 16
#define KO_PO_CLUBS 16                  /* slice 24: the knockout round play-offs, then a round of 16 */
#define KO_PO_ALL 24                    /* their clubs and the 8 group winners */

/* The group stage of a row: 8 groups of 5 that meet once and send three (2004/05 to 2008/09), or 12 groups of 4
 * that meet twice and send two (the Europa League, 2009/10 on). The object holds groups + 1 stages: the groups
 * in stages 0 to groups - 1 and the knockout of 32 in stage groups. */
typedef struct {
    int groups, clubs, meetings, through, days;
    int ko_po;                          /* slice 24 (2021/22 on): 16, the clubs of the knockout round play-offs */
    int league;                         /* slice 25 (2024/25 on): the matches of a club in the league phase, else 0 */
} UcGroups;

/* The group stage of the cup of kind ('e' the Europa League, 'k' the Conference League) in row r. */
static int uc_format(int kind, const SeasonUefa *r, UcGroups *f)
{
    if (!r) return 0;
    if (kind == 'k')
        f->groups = r->ecl_groups, f->clubs = r->ecl_group_clubs, f->meetings = r->ecl_meetings,
        f->through = r->ecl_through, f->ko_po = r->ecl_ko_po;
    else
        f->groups = r->uc_groups, f->clubs = r->uc_group_clubs, f->meetings = r->uc_meetings,
        f->through = r->uc_through, f->ko_po = r->uc_ko_po;
    /* Slice 25: the league phase, one table of 36 (1 group, 24 through) with 8 or 6 match days, then the
     * knockout phase play-offs of places 9 to 24 and a round of 16 with places 1 to 8. */
    f->league = kind == 'k' ? r->ecl_league : r->uc_league;
    if (f->league) {
        f->days = f->league;
        return f->groups == 1 && f->clubs == SEASON_LEAGUE_CLUBS && f->through == 24 && f->ko_po == KO_PO_CLUBS &&
               f->days <= SEASON_LEAGUE_MAX_DAYS;
    }
    if (f->groups < 2 || f->groups > MAX_GROUPS || f->clubs < 3 || f->clubs > MAX_GROUP_CLUBS || f->meetings < 1 ||
        f->meetings > 2)
        return 0;
    /* An odd count rests one club on each match day (League::create_fixtures 0x6835c0 adds the resting place,
     * 0x683c1b): 5 clubs meet once on 5 days, 4 clubs twice on 6 days. */
    f->days = (f->clubs % 2 ? f->clubs : f->clubs - 1) * f->meetings;
    if (f->days > MAX_GROUP_DAYS) return 0;
    /* Slice 24, the knockout round play-offs (2021/22 on): the 8 runners-up meet the 8 Champions League thirds,
     * and the 8 winners of those ties meet the 8 group winners in the round of 16. */
    if (f->ko_po) return f->groups == 8 && f->through == 2 && f->ko_po == KO_PO_CLUBS;
    /* The knockout of 32: the clubs that go through and the 8 Champions League thirds. */
    return f->groups * f->through + 8 == KO_CLUBS;
}
#define WEEKDAY_UC 3                    /* the weekday of the UEFA Cup rounds of the original (0x9026a4) */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }
static int uc_inner;                    /* 1: the wrap of the round table passes to the original */

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

/* The Europa League or (slice 24) the Conference League: the kind 'e' or 'k', 0 for any other object. */
static int is_uc(uint32_t obj) { return season_uefa_cup_kind(obj); }
static const char *uc_name(uint32_t uc) { return is_uc(uc) == 'k' ? "Conference League" : "Europa League"; }
/* A UEFA Cup with a group stage holds 3 to 13 stages (the original's has none, +0x2c = 0 at 0x902344); with a
 * league phase (slice 25) 2: the league phase and the knockout stage. */
static int groups_layout(uint32_t uc)
{
    int32_t n = is_uc(uc) ? (int32_t)rd32(g_mem, uc + 0x2c) : 0;
    return n >= 2 && n <= MAX_STAGES;
}
static int ko_slot(uint32_t uc) { return (int32_t)rd32(g_mem, uc + 0x2c) - 1; }
static uint32_t stage_at(uint32_t uc, int i) { return rd32(g_mem, rd32(g_mem, uc + 0xc) + 4u * (uint32_t)i); }

static int random_below(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    int k = (int)call_cdecl(F_RANDOM, 1, a);
    return k < 0 || k >= n ? 0 : k;
}

static uint32_t row_club(uint32_t stage, int pos)
{
    uint32_t a[1] = {(uint32_t)pos};
    uint32_t row = call_this(F_LEAGUE_ROW, stage, 1, a);
    return rt_unwinding || !readable(row) ? 0 : rd32(g_mem, row);
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (!rt_unwinding && readable(p)) memset(g_mem + p, 0, size);
    return rt_unwinding ? 0 : p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p)) call_cdecl(F_FREE, 1, a);
}

/* The stage array (13 slots, as the Champions League's, 0x583800) */
static int ensure_stage_array(uint32_t uc)
{
    if (readable(rd32(g_mem, uc + 0xc))) return 1;
    uint32_t arr = game_malloc(13 * 4);
    if (!arr) return 0;
    wr32(g_mem, uc + 0xc, arr);
    return 1;
}

/* Deletes the stages of the season that ends (as EuropeanCup::start_new_season 0x584e04..0x584e28). */
static void free_stages(uint32_t uc)
{
    uint32_t arr = rd32(g_mem, uc + 0xc);
    int32_t now = (int32_t)rd32(g_mem, uc + 0x30);
    for (int i = 0; readable(arr) && i <= now && i < 13 && !rt_unwinding; i++) {
        uint32_t st = rd32(g_mem, arr + 4u * (uint32_t)i);
        if (readable(st)) {
            uint32_t a[1] = {1};
            call_this(rd32(g_mem, rd32(g_mem, st)), st, 1, a);     /* the scalar deleting destructor */
        }
        if (!rt_unwinding) wr32(g_mem, arr + 4u * (uint32_t)i, 0);
    }
    if (!rt_unwinding) wr32(g_mem, uc + 0x30, 0xffffffffu);
}

static void set_date(uint32_t t, int r, int draw_day, int draw_month, int draw_year, int day, int month, int year_off,
                     int venue, uint32_t year)
{
    uint32_t d[7] = {t, (uint32_t)r, (uint32_t)draw_day, (uint32_t)draw_month, (uint32_t)draw_year, 4, year};
    call_cdecl(F_DRAW_DATE, 7, d);
    uint32_t m[9] = {t, (uint32_t)r, (uint32_t)day, (uint32_t)month, (uint32_t)year_off, WEEKDAY_UC, 2, year,
                     (uint32_t)venue};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m);
}

/* The record of the original's table with round code code, or 0. */
static uint32_t record_of(uint32_t t, int rounds, int code)
{
    for (int r = 0; r < rounds; r++)
        if ((int16_t)rd16(g_mem, t + (uint32_t)r * ROUND_SIZE + 7) == code) return t + (uint32_t)r * ROUND_SIZE;
    return 0;
}

static void set_round(uint32_t rec, int index, int code, int clubs, int fresh, int first, int flags, int gap)
{
    wr16(g_mem, rec + 7, (uint16_t)code);
    wr16(g_mem, rec + 9, (uint16_t)index);
    wr16(g_mem, rec + 0xd, (uint16_t)flags);
    wr16(g_mem, rec + 0x18, (uint16_t)clubs);
    wr16(g_mem, rec + 0x1a, (uint16_t)(clubs / 2));
    wr16(g_mem, rec + 0x1c, (uint16_t)fresh);
    wr16(g_mem, rec + 0x1e, (uint16_t)first);
    if (gap) g_mem[rec + 0x22] = (uint8_t)gap;
}

/* The original's round table of stage -1 (8 or 9 rounds). */
static uint32_t original_table(uint32_t uc, int *rounds)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t out = cpu.esp;
    for (int i = 0; i < 16; i += 4) wr32(g_mem, out + (uint32_t)i, 0);
    uint32_t a[4] = {0xffffffffu, out, out + 4, out + 8};
    uc_inner = 1;
    uint32_t t = call_this(rd32(g_mem, rd32(g_mem, uc) + 0x3c), uc, 4, a);
    uc_inner = 0;
    *rounds = (int16_t)rd16(g_mem, out);
    if (rt_unwinding) return 0;
    cpu.esp = saved_esp;
    return rt_unwinding || !readable(t) ? 0 : t;
}

/* ---- Stage -1: Q1, Q2 and R1 ---- */

void o_009025c0(void);

/* Slice 23: the round table of stage -1 for the access list (season_uefa_qual.c), made from the original's
 * qualifying round record (code 0xdc). It changes nothing in the object. Returns the table or 0. */
static uint32_t uc_access_table(uint32_t uc, int *n)
{
    int rounds = 0;
    uint32_t orig = original_table(uc, &rounds);
    uint32_t qr = orig ? record_of(orig, rounds, 0xdc) : 0;
    uint32_t t = !qr ? 0 : is_uc(uc) == 'k' ? season_uefa_ecl_qual_table(uc, qr, n) : season_uefa_uc_qual_table(uc, qr, n);
    game_free(orig);
    return rt_unwinding ? 0 : t;
}

/* A copy of the round table that the set-up made (+0xa3, +0x3c rounds), for a reader of stage -1 at any date: the
 * clubs' entry codes change in the season, so the layout is not made again. */
static uint32_t uc_saved_table(uint32_t uc, int *n)
{
    uint32_t src = rd32(g_mem, uc + 0xa3);
    int rounds = (int16_t)rd16(g_mem, uc + 0x3c);
    if (!readable(src) || rounds < 1 || rounds > 16) return 0;
    uint32_t t = game_malloc(ROUND_SIZE * (uint32_t)rounds);
    if (rt_unwinding || !t) return 0;
    memcpy(g_mem + t, g_mem + src, ROUND_SIZE * (uint32_t)rounds);
    *n = rounds;
    return t;
}

/* The access list at the set-up of a season: the qualifying rounds and a group stage of f. Returns 1 when it
 * returned the table to the caller. */
static int uc_access_round_table(uint32_t uc, const UcGroups *f, uint32_t out_rounds)
{
    int n = 0;
    uint32_t t = uc_access_table(uc, &n);
    if (rt_unwinding || !t) return 0;
    wr32(g_mem, uc + 0x2c, (uint32_t)(f->groups + 1));
    wr32(g_mem, uc + 0x30, 0xffffffffu);
    if (!ensure_stage_array(uc)) {
        wr32(g_mem, uc + 0x2c, 0);
        game_free(t);
        return 0;
    }
    /* As the original writes them (0x9025e7, 0x902601): the rounds and a name of 0 as words, the fourth a dword 0. */
    uint32_t out_name = rt_arg(2), out_fourth = rt_arg(3);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, (uint16_t)n);
    if (readable(out_name)) wr16(g_mem, out_name, 0);
    if (readable(out_fourth)) wr32(g_mem, out_fourth, 0);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    fprintf(stderr, "recomp: uefa: %s %u/%02u: %d qualifying rounds, %d groups of %d\n", uc_name(uc), year,
            (year + 1) % 100, n, f->groups, f->clubs);
    rt_ret_stdcall(t, 4);
    return 1;
}

/* The sizes of Q1, Q2 and R1 of row r for a UEFA Cup with count slots (see the head of the file), or 0. The
 * original's format of 2004/05 to 2008/09: 8 groups of 5. */
static int qual_sizes(uint32_t uc, const SeasonUefa *r, int *q1, int *q2, int *r1)
{
    int count = (int16_t)rd16(g_mem, uc + 0x3e), b2 = (int8_t)g_mem[uc + 0xb2];
    UcGroups f;
    *r1 = r ? season_uefa_uc_round(r, "R1") : 0, *q2 = r ? season_uefa_uc_round(r, "Q2") : 0;
    *q1 = 2 * (count - 8 - *r1) - *q2;
    return uc_format('e', r, &f) && f.groups == 8 && f.clubs == 5 && f.meetings == 1 &&
           season_uefa_uc_round(r, "KO") == KO_CLUBS && season_uefa_cl_round_of_16(r) && b2 == 0 && *r1 == 80 &&
           *q2 > 0 && *q2 % 2 == 0 && *q1 >= 2 && *q1 % 2 == 0 && count > 100;
}

/* The round table of Q1, Q2 and R1 (game memory, as the original's), or 0. It changes nothing else. */
static uint32_t qual_table(uint32_t uc, int q1, int q2, int r1)
{
    int rounds = 0;
    uint32_t orig = original_table(uc, &rounds);
    uint32_t qr = orig ? record_of(orig, rounds, 0xdc) : 0, first = orig ? record_of(orig, rounds, 10) : 0;
    uint32_t t = qr && first ? game_malloc(3 * ROUND_SIZE) : 0;
    if (t) {
        memcpy(g_mem + t, g_mem + qr, ROUND_SIZE);
        memcpy(g_mem + t + ROUND_SIZE, g_mem + qr, ROUND_SIZE);
        memcpy(g_mem + t + 2 * ROUND_SIZE, g_mem + first, ROUND_SIZE);
    }
    game_free(orig);
    if (!t || rt_unwinding) return 0;
    /* Codes 250 and 260 are the "First" and "Second Qualifying Phase" of the round names (0x4b4850), as the
     * Champions League's (0x583d13, 0x583d88); 10 is "First Round". */
    set_round(t, 0, 250, q1, q1, 0, 0, 0);
    set_round(t + ROUND_SIZE, 1, 260, q2, q2 - q1 / 2, q1, 0, 0);
    set_round(t + 2 * ROUND_SIZE, 2, 10, r1, r1 - q2 / 2, q1 + q2 - q1 / 2, 0x400, 0);
    /* Q1 on 13 July, Q2 on the original's qualifying round dates (draw 28 July, 10 August, 0x902720..0x902740),
     * R1 on its first round dates (14 September); each second leg 14 days later (+0x22). */
    set_date(t, 0, 1, 6, 0, 13, 6, 0, 0, (uint32_t)(int16_t)rd16(g_mem, uc + 0x40));
    return rt_unwinding ? 0 : t;
}

static uint32_t ko_table(uint32_t uc, int ko_po);
static uint32_t group_table(uint32_t year, int days);
static uint32_t uc_group_name(int g);

/* The return addresses of the two calls that set up a season's rounds: the constructor (call at 0x9023cd) and
 * set_up_rules 0x9030b0 (call at 0x9030ca). Any other caller, such as Comp::build_match_date_list 0x48d500 (the
 * Schedule tab, call at 0x48d5c4), only reads the tables. */
#define UC_SETUP_CONSTRUCTOR 0x009023d0u
#define UC_SETUP_RULES 0x009030cdu

/* Uefa_Cup::create_round_table, thiscall (stage, &rounds, &name, &league) ret 0x10. At the set-up of a season
 * (stage -1, after build_entrants) the format of the season is chosen here. A UEFA Cup with groups gives every
 * caller the tables of its stages: Comp::build_match_date_list 0x48d500 asks for each stage from -1 to
 * +0x2c - 1 (0x48d5b0..0x48d5cb), and the original builds only stage -1 (0x9025ca), so stages 0 to groups - 1
 * come from here: the groups (a league table: league 1) and the knockout stage. */
RT_WRAP(009025c0, uc_round_table)
{
    uint32_t uc = cpu.ecx;
    int stage = (int8_t)rt_arg(0);
    if (uc_inner || !is_uc(uc)) {
        o_009025c0();
        return;
    }
    uint32_t out_rounds = rt_arg(1), out_name = rt_arg(2), out_league = rt_arg(3), ret = rd32(g_mem, cpu.esp);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    const SeasonUefa *r = season_uefa_row((int)year);
    UcGroups f;
    int kind = is_uc(uc), have_f = uc_format(kind, r, &f), q1, q2, r1;
    /* The seasons of the access list (slice 23: 2009/10 on; slice 22's groups of 2004/05 to 2008/09 have none). */
    int access = have_f && season_uefa_access_places(r->season, 'c', 1);
    if (stage == -1 && (ret == UC_SETUP_CONSTRUCTOR || ret == UC_SETUP_RULES)) {
        if (readable(rd32(g_mem, uc + 0xc)) && (int32_t)rd32(g_mem, uc + 0x30) >= 0) free_stages(uc);
        if (rt_unwinding) return;
        if (access && (int8_t)g_mem[uc + 0xb2] == 0 && uc_access_round_table(uc, &f, out_rounds)) return;
        if (rt_unwinding) return;
        /* Slice 24: the format of 2004/05 to 2008/09 is the Europa League's only. */
        if (kind != 'e' || !qual_sizes(uc, r, &q1, &q2, &r1)) {
            if ((int32_t)rd32(g_mem, uc + 0x2c) != 0) wr32(g_mem, uc + 0x2c, 0);
            if (r && (r->uc_groups || kind == 'k'))
                fprintf(stderr, "recomp: uefa: %s %u groups not possible here (b2 %d, %d slots)\n",
                        kind == 'k' ? "Conference League" : "UEFA Cup", year, (int8_t)g_mem[uc + 0xb2],
                        (int16_t)rd16(g_mem, uc + 0x3e));
            o_009025c0();
            return;
        }
        uint32_t t = qual_table(uc, q1, q2, r1);
        if (rt_unwinding) return;
        if (!t) {
            o_009025c0();
            return;
        }
        wr32(g_mem, uc + 0x2c, (uint32_t)(f.groups + 1));
        wr32(g_mem, uc + 0x30, 0xffffffffu);
        if (!ensure_stage_array(uc)) {
            wr32(g_mem, uc + 0x2c, 0);
            game_free(t);
            if (!rt_unwinding) o_009025c0();
            return;
        }
        if (readable(out_rounds)) wr16(g_mem, out_rounds, 3);
        if (readable(out_name)) wr16(g_mem, out_name, 0);       /* as the original (0x902601) */
        if (readable(out_league)) wr32(g_mem, out_league, 0);
        fprintf(stderr, "recomp: uefa: UEFA Cup %u/%02u with groups: Q1 %d, Q2 %d, R1 %d\n", year, (year + 1) % 100,
                q1, q2, r1);
        rt_ret_stdcall(t, 4);
        return;
    }
    if (!groups_layout(uc) || stage < -1 || stage > ko_slot(uc) || !have_f) {
        o_009025c0();
        return;
    }
    /* A reader: the tables of this season's format, with no change to the object. */
    uint32_t t = 0;
    int rounds = 0, name = 0, league = 0;
    if (stage == -1) {
        if (access) t = uc_saved_table(uc, &rounds);
        else if (kind == 'e' && qual_sizes(uc, r, &q1, &q2, &r1)) t = qual_table(uc, q1, q2, r1), rounds = 3;
    } else if (stage < f.groups && f.league) {
        int weekday = 0;
        const SeasonLeagueDay *days = season_uefa_league_days((char)kind, &weekday);
        t = season_uefa_league_table(year, f.days, days, weekday), rounds = f.days, name = (int)uc_group_name(0), league = 1;
    } else if (stage < f.groups) {
        t = group_table(year, f.days), rounds = f.days, name = (int)uc_group_name(stage), league = 1;
    } else {
        t = ko_table(uc, f.ko_po ? 1 : 0), rounds = KO_ROUNDS;
    }
    if (rt_unwinding) return;
    if (!t) {
        o_009025c0();
        return;
    }
    if (readable(out_rounds)) wr16(g_mem, out_rounds, (uint16_t)rounds);
    if (readable(out_name)) wr16(g_mem, out_name, (uint16_t)name);
    if (readable(out_league)) wr32(g_mem, out_league, (uint32_t)league);
    rt_ret_stdcall(t, 4);
}

/* ---- The group stage ---- */

/* The clubs of the group draw: those that won their last round, entry byte +5 = 1, as the Champions League's
 * qualifiers (draw_phase1_groups 0x585383..0x5853a6). Returns their count (it can be above max). */
static int uc_group_entrants(uint32_t uc, uint32_t *clubs, int max)
{
    int n = 0, count = (int16_t)rd16(g_mem, uc + 0x3e);
    uint32_t entries = rd32(g_mem, uc + 0xa7);
    /* Slice 23, the access list: also every club in the slots after the rounds and before the 8 slots of the
     * Champions League thirds (the group clubs, the Champions League losers for the groups). */
    int ecl = is_uc(uc) == 'k';
    int direct = ecl ? season_uefa_ecl_direct_first(uc) : season_uefa_uc_direct_first(uc);
    if (direct >= 0 && ecl) season_uefa_ecl_fill_drops(uc);
    else if (direct >= 0) season_uefa_uc_fill_drops(uc);
    if (rt_unwinding) return 0;
    for (int i = 0; readable(entries) && i < count; i++)
        if ((g_mem[entries + 6u * (uint32_t)i + 5] == 1 || (direct >= 0 && i >= direct && i < count - 8)) &&
            readable(rd32(g_mem, entries + 6u * (uint32_t)i))) {
            if (n < max) clubs[n] = rd32(g_mem, entries + 6u * (uint32_t)i);
            n++;
        }
    return n;
}

/* The name of group g: 0x41f + g, "Group A" to "Group P" (the name table 0x4b9a44; 0x43d + g, the names of
 * the Champions League's phase 1 at 0x583ed8, read "Phase 1 Group A"). */
static uint32_t uc_group_name(int g) { return 0x41fu + (uint32_t)g; }

/* The match days of the groups (Thursdays, month 0 to 11). 5 days: 21 October, 4 and 25 November, 2 and 16
 * December (UEFA Cup 2004/05). 6 days: 17 September, 1 and 22 October, 5 November, 2 and 17 December (Europa
 * League 2009/10, row uefa-69 of the rules table). */
static const int GROUP_DATES_5[5][2] = {{21, 9}, {4, 10}, {25, 10}, {2, 11}, {16, 11}};
static const int GROUP_DATES_6[6][2] = {{17, 8}, {1, 9}, {22, 9}, {5, 10}, {2, 11}, {17, 11}};

static uint32_t group_table(uint32_t year, int days)
{
    const int (*dates)[2] = days == 6 ? GROUP_DATES_6 : GROUP_DATES_5;
    uint32_t t = game_malloc((uint32_t)days * GROUP_ROUND_SIZE);
    for (int d = 0; t && d < days && !rt_unwinding; d++) {
        uint32_t a[9] = {t, (uint32_t)d, (uint32_t)dates[d][0], (uint32_t)dates[d][1], 0, WEEKDAY_UC, 2, year, 0};
        call_cdecl(F_GROUP_DATE, 9, a);
        uint32_t s[7] = {t, (uint32_t)d, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
        if (!rt_unwinding) call_cdecl(F_GROUP_SLOT, 7, s);
    }
    return rt_unwinding ? 0 : t;
}

/* Puts the clubs (in pots of f->groups by seed) into the groups, one of each pot a group, two clubs of one
 * nation (club +0x53) in a group only when no other draw is found. */
static int draw_groups(const UcGroups *f, const uint32_t *clubs, uint32_t groups[MAX_GROUPS][MAX_GROUP_CLUBS])
{
    for (int relax = 0; relax < 2; relax++)
        for (int attempt = 0; attempt < 50; attempt++) {
            int ok = 1;
            for (int pot = 0; pot < f->clubs && ok; pot++) {
                int used[MAX_GROUPS] = {0};
                for (int k = 0; k < f->groups && ok; k++) {
                    uint32_t club = clubs[pot * f->groups + k];
                    int cand[MAX_GROUPS], n = 0;
                    for (int g = 0; g < f->groups; g++) {
                        if (used[g]) continue;
                        int same = 0;
                        for (int p = 0; p < pot && !relax; p++)
                            same |= rd32(g_mem, groups[g][p] + 0x53) == rd32(g_mem, club + 0x53);
                        if (!same) cand[n++] = g;
                    }
                    if (!n) {
                        ok = 0;
                        break;
                    }
                    int g = cand[random_below(n)];
                    if (rt_unwinding) return 0;
                    used[g] = 1;
                    groups[g][pot] = club;
                }
            }
            if (ok) return 1;
        }
    return 0;
}

/* Sorts n clubs by the European seed (0x586f60, as the original's draws 0x585435). */
static int sort_by_seed(uint32_t *clubs, int n)
{
    uint32_t list = game_malloc(4u * (uint32_t)n);
    if (!list) return 0;
    for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t q[4] = {list, (uint32_t)n, 4, F_SEED_ORDER};
    call_cdecl(F_QSORT, 4, q);
    if (rt_unwinding) return 0;
    for (int i = 0; i < n; i++) clubs[i] = rd32(g_mem, list + 4u * (uint32_t)i);
    game_free(list);
    return !rt_unwinding;
}

/* ---- The league phase (slice 25, 2024/25 on) ----
 *
 * The object holds 2 stages: the league phase of 36 clubs in stage 0 (season_uefa_league.c) and the knockout
 * stage in stage 1: the knockout phase play-offs of places 9 to 24 in the ties of the bracket (round 0 of
 * ko_table with ko_po, code 30), then the round of 16 with places 1 to 8 (a seeded draw), as slice 24's
 * knockout round play-offs. No group thirds go to the Conference League. */

static void draw_league_stage(uint32_t uc, const UcGroups *f)
{
    uint32_t clubs[SEASON_LEAGUE_CLUBS + 8];
    int n = uc_group_entrants(uc, clubs, SEASON_LEAGUE_CLUBS + 8);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    if (rt_unwinding) return;
    if (n != SEASON_LEAGUE_CLUBS) {
        fprintf(stderr, "recomp: uefa: %s league phase: %d clubs, not 36\n", uc_name(uc), n);
        return;
    }
    int weekday = 0;
    const SeasonLeagueDay *days = season_uefa_league_days((char)is_uc(uc), &weekday);
    uint32_t st = season_uefa_league_make(uc, clubs, f->league, 0, uc_group_name(0), days, weekday);
    if (rt_unwinding || !st) return;
    wr32(g_mem, rd32(g_mem, uc + 0xc), st);
    wr32(g_mem, uc + 0x30, 0);
    season_uefa_league_news(st);
    if (!rt_unwinding)
        fprintf(stderr, "recomp: uefa: %s %u/%02u: league phase drawn\n", uc_name(uc), year, (year + 1) % 100);
}

static void draw_league_knockout(uint32_t uc, uint32_t year)
{
    uint32_t league = stage_at(uc, 0);
    if (!readable(league)) return;
    uint32_t ca[2] = {KO_PO_ALL, 4};
    uint32_t list = call_cdecl(F_CALLOC, 2, ca);
    uint32_t tiers = rt_unwinding || !readable(list) ? 0 : game_malloc(KO_PO_ALL);
    if (rt_unwinding) return;
    uint32_t table = 0, obj = 0;
    if (tiers && season_uefa_league_knockout_list(league, list, tiers)) {
        uint32_t bodies = rd32(g_mem, RULING_BODIES);
        uint32_t body = readable(bodies) ? rd32(g_mem, rd32(g_mem, bodies) + 4u * (uint32_t)(int8_t)g_mem[uc + 0x50]) : 0;
        for (int i = 0; i < KO_PO_ALL && readable(body) && !rt_unwinding; i++) {
            uint32_t a[1] = {rd32(g_mem, list + 4u * (uint32_t)i)};
            call_this(F_BOOKINGS, body, 1, a);
        }
        table = rt_unwinding ? 0 : ko_table(uc, 1);
        uint32_t sz[1] = {0xb2};
        obj = table && !rt_unwinding ? call_cdecl(F_NEW, 1, sz) : 0;
    }
    if (rt_unwinding) return;
    if (readable(obj)) {
        uint32_t args[15] = {uc, KO_PO_ALL, list, KO_ROUNDS, rd32(g_mem, uc + 4), table, year, 1, 2, 0, 0x14, 1, 0, 0,
                             tiers};
        call_this(F_CUP_STAGE, obj, 15, args);
        if (rt_unwinding) return;
        wr32(g_mem, rd32(g_mem, uc + 0xc) + 4u, obj);
    }
    game_free(list);
    game_free(tiers);
    game_free(table);
    if (rt_unwinding || !readable(obj)) return;
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, obj, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, uc + 0x30, 1);
    fprintf(stderr, "recomp: uefa: %s knockout phase play-offs drawn\n", uc_name(uc));
}

static void draw_group_stage(uint32_t uc)
{
    UcGroups f;
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    if (!uc_format(is_uc(uc), season_uefa_row((int)year), &f) || f.groups + 1 != (int32_t)rd32(g_mem, uc + 0x2c)) {
        fprintf(stderr, "recomp: uefa: UEFA Cup groups: no format for %u with %d stages\n", year,
                (int32_t)rd32(g_mem, uc + 0x2c));
        return;
    }
    if (f.league) {
        draw_league_stage(uc, &f);
        return;
    }
    int want = f.groups * f.clubs;
    uint32_t clubs[MAX_GROUPS * MAX_GROUP_CLUBS];
    int n = uc_group_entrants(uc, clubs, MAX_GROUPS * MAX_GROUP_CLUBS);
    if (n != want) {
        fprintf(stderr, "recomp: uefa: UEFA Cup groups: %d clubs, not %d; no group stage\n", n, want);
        return;
    }
    if (!sort_by_seed(clubs, n)) return;
    uint32_t groups[MAX_GROUPS][MAX_GROUP_CLUBS];
    if (rt_unwinding || !draw_groups(&f, clubs, groups)) return;
    for (int g = 0; g < f.groups && !rt_unwinding; g++) {
        uint32_t members = game_malloc(4u * (uint32_t)f.clubs);
        uint32_t table = group_table(year, f.days);
        uint32_t sz[1] = {0xee};                                      /* as 0x5860b3 */
        uint32_t obj = members && table ? call_cdecl(F_NEW, 1, sz) : 0;
        if (rt_unwinding || !readable(obj)) return;
        for (int k = 0; k < f.clubs; k++) wr32(g_mem, members + 4u * (uint32_t)k, groups[g][k]);
        /* Two small byte tables on the game stack: the order of the table (4, 1, 2, 0, as 0x585ad9) and the
         * places that go through (+0xbe: the top two or three). */
        uint32_t saved_esp = cpu.esp;
        cpu.esp -= 16;
        uint32_t order = cpu.esp, places = cpu.esp + 8;
        g_mem[order] = 4, g_mem[order + 1] = 1, g_mem[order + 2] = 2, g_mem[order + 3] = 0;
        g_mem[places] = (uint8_t)f.through, g_mem[places + 1] = 0, g_mem[places + 2] = 0, g_mem[places + 3] = 0;
        /* The arguments as draw_phase2_groups gives them (0x5860dd..0x58611d): parent, clubs, list, meetings,
         * record, round table, match days, 3 and 1 (+0xc2, +0xc3: points of a win and a draw, inference), 8,
         * order, places, year, stage, name, 0xf, 2, 0, 8, -1, 0, 2. */
        uint32_t a[22] = {uc, (uint32_t)f.clubs, members, (uint32_t)f.meetings, rd32(g_mem, uc + 4), table,
                          (uint32_t)f.days, 3, 1, 8, order, places, year, (uint32_t)g, uc_group_name(g), 0xf, 2, 0, 8,
                          0xffffffffu, 0, 2};
        call_this(F_LEAGUE_STAGE, obj, 22, a);
        if (rt_unwinding) return;
        cpu.esp = saved_esp;
        wr32(g_mem, rd32(g_mem, uc + 0xc) + 4u * (uint32_t)g, obj);
        game_free(members);
        game_free(table);
        if (!rt_unwinding) call_this(F_GROUP_NEWS, obj, 0, NULL);
    }
    if (rt_unwinding) return;
    wr32(g_mem, uc + 0x30, (uint32_t)(f.groups - 1));    /* the last group, as draw_phase2_groups leaves it (0x586182) */
    fprintf(stderr, "recomp: uefa: %s %u/%02u: %d groups of %d drawn\n", is_uc(uc) == 'k' ? "Conference League" : "UEFA Cup",
            year, (year + 1) % 100, f.groups, f.clubs);
}

/* ---- The knockout stage of 32 ---- */

/* The rounds of 32 and 16 and the quarter-finals, semi-finals and final, from the original's records. Dates of
 * 2004/05: 17 and 24 February, 10 and 17 March, 7 and 14 April, 28 April and 5 May, final 18 May (venue 4, as
 * the original's final 0x902b07). */
static uint32_t ko_table(uint32_t uc, int ko_po)
{
    int rounds = 0;
    uint32_t orig = original_table(uc, &rounds);
    if (!orig) return 0;
    static const int codes[KO_ROUNDS] = {30, 40, 0x78, 0x82, 0x96};
    uint32_t t = game_malloc(KO_ROUNDS * ROUND_SIZE);
    for (int r = 0; t && r < KO_ROUNDS; r++) {
        uint32_t rec = record_of(orig, rounds, codes[r]);
        if (!rec) {
            game_free(t);
            t = 0;
            break;
        }
        memcpy(g_mem + t + (uint32_t)r * ROUND_SIZE, g_mem + rec, ROUND_SIZE);
    }
    game_free(orig);
    if (!t || rt_unwinding) return 0;
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    int clubs = KO_CLUBS;
    for (int r = 0; r < KO_ROUNDS; r++, clubs /= 2)
        set_round(t + (uint32_t)r * ROUND_SIZE, r, codes[r], clubs, r == 0 ? KO_CLUBS : 0, 0, r == 0 ? 0x200 : 0,
                  r < KO_ROUNDS - 1 ? 7 : 0);
    if (ko_po) {
        /* Slice 24: round 0 (code 30, named "Knockout Round Play-offs" from 2021/22 by the round names table) has
         * the 16 clubs of list slots 0 to 15 in fixed ties (0x200); the round of 16 takes the 8 winners and the 8
         * group winners of slots 16 to 23 (+0x1c new clubs from +0x1e), a seeded draw (0x400: 0x51d2d0 pairs the
         * new clubs, with the higher tier byte +4, with the winners) that avoids two clubs of one nation (+0xb 4,
         * as the qualifying rounds of season_uefa_qual.c). Then the quarter-finals, semi-finals and final. */
        set_round(t, 0, codes[0], KO_PO_CLUBS, KO_PO_CLUBS, 0, 0x200, 7);
        set_round(t + ROUND_SIZE, 1, codes[1], 16, KO_PO_ALL - KO_PO_CLUBS, KO_PO_CLUBS, 0x400, 7);
        g_mem[t + ROUND_SIZE + 0xb] = 4;
        for (int r = 2, n = 8; r < KO_ROUNDS; r++, n /= 2)
            set_round(t + (uint32_t)r * ROUND_SIZE, r, codes[r], n, 0, 0, 0, r < KO_ROUNDS - 1 ? 7 : 0);
    }
    wr16(g_mem, t + (KO_ROUNDS - 1) * ROUND_SIZE + 0xd, 0x83);       /* the final's flags, as 0x902b38 */
    static const int dates[KO_ROUNDS][6] = {
        {17, 11, 0, 17, 1, 1}, {25, 1, 1, 10, 2, 1}, {18, 2, 1, 7, 3, 1}, {15, 3, 1, 28, 3, 1}, {6, 4, 1, 18, 4, 1}};
    for (int r = 0; r < KO_ROUNDS && !rt_unwinding; r++)
        set_date(t, r, dates[r][0], dates[r][1], dates[r][2], dates[r][3], dates[r][4], dates[r][5],
                 r == KO_ROUNDS - 1 ? 4 : 0, year);
    if (!rt_unwinding) season_uefa_no_away_goals(t, KO_ROUNDS, (int)year);
    /* O259: the real dates of the season (season_uefa_dates.c); a round with no row keeps the dates above. */
    static const int kinds[KO_ROUNDS] = {SEASON_UEFA_KO_PO, SEASON_UEFA_KO_R16, SEASON_UEFA_KO_QF, SEASON_UEFA_KO_SF,
                                         SEASON_UEFA_KO_FINAL};
    int dated = rt_unwinding ? 0 : season_uefa_ko_dates(t, is_uc(uc) == 'k' ? 'k' : 'u', (int)year, kinds, KO_ROUNDS);
    if (!rt_unwinding && season_uefa_ko_dates_log(is_uc(uc) == 'k' ? 'k' : 'u', (int)year))
        fprintf(stderr, "recomp: uefa: %s %u/%02u: knockout dates of %d of %d rounds from the dates table\n",
                uc_name(uc), year, (year + 1) % 100, dated, KO_ROUNDS);
    return rt_unwinding ? 0 : t;
}

/* Pairs each unseeded club with a seeded one: not of its own group, of another nation when a draw is found. */
static int pair_ko(const uint32_t *seed, const int *seed_group, const uint32_t *unseed, const int *unseed_group,
                   int *out, int ties)
{
    for (int relax = 0; relax < 3; relax++)
        for (int attempt = 0; attempt < 50; attempt++) {
            int used[KO_TIES] = {0}, ok = 1;
            for (int i = 0; i < ties && ok; i++) {
                int cand[KO_TIES], n = 0;
                for (int j = 0; j < ties; j++) {
                    if (used[j]) continue;
                    if (relax < 2 && unseed_group[i] >= 0 && unseed_group[i] == seed_group[j]) continue;
                    if (relax < 1 && rd32(g_mem, unseed[i] + 0x53) == rd32(g_mem, seed[j] + 0x53)) continue;
                    cand[n++] = j;
                }
                if (!n) {
                    ok = 0;
                    break;
                }
                out[i] = cand[random_below(n)];
                if (rt_unwinding) return 0;
                used[out[i]] = 1;
            }
            if (ok) return 1;
        }
    return 0;
}

/* Slice 24 (2021/22 on): the knockout stage of 24 clubs. The 8 runners-up are seeded, the 8 Champions League
 * thirds (their slots, count - 8) are not; each third meets a runner-up of another nation when a draw is found,
 * the runner-up at home in the second leg. List slots 0 to 15 hold those ties, slots 16 to 23 the 8 group
 * winners, who enter the round of 16 (ko_table). The tier array of the stage (argument 15 of the Cup_Stage
 * constructor, entry byte +4: 0x522fa6..0x522fc6) gives the winners 2 and the others 1, so the seeded draw of
 * the round of 16 pairs each winner with a play-off winner. The thirds and fourths of the groups are out. */
/* Slice 24: the Conference League takes the 8 Europa League group thirds (the third row of each of its 8 group
 * tables, League::get_table_row 0x6826d0) into its last 8 slots, as the Champions League thirds go to the Europa
 * League (0x586282): the entry, the list +0x14 and its id at +0x1db. Both group stages end on the same match
 * day, so the tables are final whichever cup draws first (a 2021/22 game drew the Conference League groups
 * before the Europa League's); the Europa League's own draw keeps the id of a third taken here. Returns the
 * thirds put, or -1 when the Europa League has no 8 groups. */
static int ecl_take_el_thirds(uint32_t ecl)
{
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int32_t key = (int32_t)rd32(g_mem, KEY_UC);
    uint32_t el = readable(table) && key >= 0 ? rd32(g_mem, table + 4u * (uint32_t)key) : 0;
    UcGroups fe;
    if (is_uc(el) != 'e' || !groups_layout(el) || !uc_format('e', season_uefa_row((int16_t)rd16(g_mem, el + 0x40)), &fe) ||
        fe.groups != 8 || ko_slot(el) != fe.groups)
        return -1;
    if ((int32_t)rd32(g_mem, el + 0x30) < fe.groups)
        fprintf(stderr, "recomp: uefa: Conference League: the Europa League knockout is not drawn yet\n");
    int count = (int16_t)rd16(g_mem, ecl + 0x3e), put = 0;
    uint32_t e = rd32(g_mem, ecl + 0xa7), list = rd32(g_mem, ecl + 0x14);
    for (int g = 0; g < 8 && readable(e) && !rt_unwinding; g++) {
        uint32_t st = stage_at(el, g), at = e + 6u * (uint32_t)(count - 8 + g);
        uint32_t club = readable(st) ? row_club(st, 2) : 0;
        if (rt_unwinding || !readable(club) || readable(rd32(g_mem, at))) continue;
        wr32(g_mem, at, club);
        g_mem[at + 5] = 0;
        int16_t m = (int16_t)rd16(g_mem, ecl + 0x36);
        if (readable(list) && m < (int16_t)rd16(g_mem, ecl + 0x38)) {
            wr32(g_mem, list + 6u * (uint32_t)m, club);
            g_mem[list + 6u * (uint32_t)m + 4] = 5;
            wr16(g_mem, ecl + 0x36, (uint16_t)(m + 1));
        }
        wr32(g_mem, club + 0x1db, (uint32_t)season_uefa_ecl_id());
        put++;
    }
    fprintf(stderr, "recomp: uefa: Conference League: %d Europa League thirds in the slots from %d\n", put, count - 8);
    return put;
}

static void draw_knockout_po(uint32_t uc, const UcGroups *f, uint32_t year, int groups)
{
    uint32_t seed[8], unseed[8], winner[8];
    int seed_group[8], unseed_group[8], nw = 0, ns = 0;
    int ecl = is_uc(uc) == 'k';
    int32_t ecl_id = season_uefa_ecl_id();
    if (ecl && ecl_take_el_thirds(uc) < 0) {
        fprintf(stderr, "recomp: uefa: Conference League knockout: the Europa League has no 8 groups\n");
        return;
    }
    if (rt_unwinding) return;
    for (int g = 0; g < f->groups; g++) {
        uint32_t st = stage_at(uc, g);
        if (!readable(st)) return;
        for (int p = 0; p < f->clubs && !rt_unwinding; p++) {
            uint32_t club = row_club(st, p);
            if (rt_unwinding) return;
            if (p >= 2) {
                /* A Europa League third that the Conference League has taken keeps its id (slice 24). */
                if (readable(club) && !(p == 2 && ecl_id >= 0 && (int32_t)rd32(g_mem, club + 0x1db) == ecl_id))
                    wr32(g_mem, club + 0x1db, 0xffffffffu);
                continue;
            }
            if (!readable(club)) return;
            if (p == 0 && nw < 8) winner[nw++] = club;
            else if (p == 1 && ns < 8) seed[ns] = club, seed_group[ns++] = g;
        }
    }
    int count = (int16_t)rd16(g_mem, uc + 0x3e);
    uint32_t entries = rd32(g_mem, uc + 0xa7);
    for (int k = 0; k < 8; k++) {
        unseed[k] = readable(entries) ? rd32(g_mem, entries + 6u * (uint32_t)(count - 8 + k)) : 0;
        unseed_group[k] = -1;
        if (!readable(unseed[k])) {
            fprintf(stderr, "recomp: uefa: %s knockout: no %s third in slot %d\n", uc_name(uc),
                    ecl ? "Europa League" : "Champions League", count - 8 + k);
            return;
        }
    }
    if (nw != 8 || ns != 8) {
        fprintf(stderr, "recomp: uefa: %s knockout: %d winners and %d runners-up\n", uc_name(uc), nw, ns);
        return;
    }
    int pair[KO_TIES];
    if (!pair_ko(seed, seed_group, unseed, unseed_group, pair, 8)) return;
    uint32_t ca[2] = {KO_PO_ALL, 4};
    uint32_t list = call_cdecl(F_CALLOC, 2, ca);
    if (rt_unwinding || !readable(list)) return;
    uint32_t tiers = game_malloc(KO_PO_ALL);
    if (rt_unwinding) return;
    if (!tiers) {
        game_free(list);
        return;
    }
    for (int i = 0; i < 8; i++) {
        wr32(g_mem, list + 8u * (uint32_t)i, unseed[i]);
        wr32(g_mem, list + 8u * (uint32_t)i + 4, seed[pair[i]]);
        wr32(g_mem, list + 4u * (uint32_t)(KO_PO_CLUBS + i), winner[i]);
    }
    for (int i = 0; i < KO_PO_ALL; i++) g_mem[tiers + (uint32_t)i] = i < KO_PO_CLUBS ? 1 : 2;
    uint32_t bodies = rd32(g_mem, RULING_BODIES);
    uint32_t body = readable(bodies) ? rd32(g_mem, rd32(g_mem, bodies) + 4u * (uint32_t)(int8_t)g_mem[uc + 0x50]) : 0;
    for (int i = 0; i < KO_PO_ALL && readable(body) && !rt_unwinding; i++) {
        uint32_t a[1] = {rd32(g_mem, list + 4u * (uint32_t)i)};
        call_this(F_BOOKINGS, body, 1, a);
    }
    uint32_t table = rt_unwinding ? 0 : ko_table(uc, 1);
    uint32_t sz[1] = {0xb2};
    uint32_t obj = table && !rt_unwinding ? call_cdecl(F_NEW, 1, sz) : 0;
    if (rt_unwinding) return;
    if (!readable(obj)) {                       /* no stage: give back what this draw allocated */
        game_free(list);
        game_free(tiers);
        game_free(table);
        return;
    }
    uint32_t args[15] = {uc, KO_PO_ALL, list, KO_ROUNDS, rd32(g_mem, uc + 4), table, year, (uint32_t)groups, 2, 0, 0x14,
                         1, 0, 0, tiers};
    call_this(F_CUP_STAGE, obj, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, uc + 0xc) + 4u * (uint32_t)groups, obj);
    game_free(list);
    game_free(tiers);
    game_free(table);
    uint32_t r0[1] = {0};
    if (!rt_unwinding) call_this(F_SET_UP_ROUND, obj, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, uc + 0x30, (uint32_t)groups);
    fprintf(stderr, "recomp: uefa: %s knockout round play-offs drawn\n", uc_name(uc));
}

/* The draw of the round of 32. Three through a group (8 groups): the 8 winners and 8 runners-up are seeded,
 * the 8 thirds and the 8 Champions League thirds are not. Two through a group (12 groups, the Europa League):
 * the 12 winners and 4 Champions League thirds are seeded, the 12 runners-up and the other 4 Champions League
 * thirds are not; the thirds are split by the European seed (inference: the seed stands in for the group
 * record that the real draw uses). The clubs below the places that go through are out (+0x1db = -1). */
static void draw_knockout(uint32_t uc)
{
    UcGroups f;
    int groups = ko_slot(uc);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    if (!uc_format(is_uc(uc), season_uefa_row((int)year), &f) || f.groups != groups) return;
    if (f.league) {
        draw_league_knockout(uc, year);
        return;
    }
    if (f.ko_po) {
        draw_knockout_po(uc, &f, year, groups);
        return;
    }
    uint32_t seed[KO_TIES], unseed[KO_TIES];
    int seed_group[KO_TIES], unseed_group[KO_TIES], ns = 0, nu = 0;
    for (int g = 0; g < f.groups; g++) {
        uint32_t st = stage_at(uc, g);
        if (!readable(st)) return;
        for (int p = 0; p < f.clubs && !rt_unwinding; p++) {
            uint32_t club = row_club(st, p);
            if (rt_unwinding) return;
            if (p >= f.through) {                                     /* out, as the fourths of 0x5862fe */
                if (readable(club)) wr32(g_mem, club + 0x1db, 0xffffffffu);
                continue;
            }
            if (!readable(club)) return;
            int seeded = p == 0 || (f.through == 3 && p == 1);
            if (seeded && ns < KO_TIES) seed[ns] = club, seed_group[ns++] = g;
            else if (!seeded && nu < KO_TIES) unseed[nu] = club, unseed_group[nu++] = g;
        }
    }
    int count = (int16_t)rd16(g_mem, uc + 0x3e);
    uint32_t entries = rd32(g_mem, uc + 0xa7), thirds[8];
    for (int k = 0; k < 8; k++) {                                     /* the slots of the Champions League thirds */
        thirds[k] = readable(entries) ? rd32(g_mem, entries + 6u * (uint32_t)(count - 8 + k)) : 0;
        if (!readable(thirds[k])) {
            fprintf(stderr, "recomp: uefa: UEFA Cup knockout: no Champions League third in slot %d\n", count - 8 + k);
            return;
        }
    }
    if (!sort_by_seed(thirds, 8)) return;
    for (int k = 0; k < 8; k++) {
        if (ns < KO_TIES && k < KO_TIES - f.groups * (f.through == 3 ? 2 : 1)) seed[ns] = thirds[k], seed_group[ns++] = -1;
        else if (nu < KO_TIES) unseed[nu] = thirds[k], unseed_group[nu++] = -1;
    }
    if (ns != KO_TIES || nu != KO_TIES) {
        fprintf(stderr, "recomp: uefa: UEFA Cup knockout: %d seeded and %d unseeded clubs\n", ns, nu);
        return;
    }
    int pair[KO_TIES];
    if (!pair_ko(seed, seed_group, unseed, unseed_group, pair, KO_TIES)) return;
    uint32_t ca[2] = {KO_CLUBS, 4};
    uint32_t list = call_cdecl(F_CALLOC, 2, ca);
    if (rt_unwinding || !readable(list)) return;
    for (int i = 0; i < KO_TIES; i++) {                               /* the seeded club plays the second leg at home */
        wr32(g_mem, list + 8u * (uint32_t)i, unseed[i]);
        wr32(g_mem, list + 8u * (uint32_t)i + 4, seed[pair[i]]);
    }
    uint32_t bodies = rd32(g_mem, RULING_BODIES);
    uint32_t body = readable(bodies) ? rd32(g_mem, rd32(g_mem, bodies) + 4u * (uint32_t)(int8_t)g_mem[uc + 0x50]) : 0;
    for (int i = 0; i < KO_CLUBS && readable(body) && !rt_unwinding; i++) {
        uint32_t a[1] = {rd32(g_mem, list + 4u * (uint32_t)i)};
        call_this(F_BOOKINGS, body, 1, a);
    }
    uint32_t table = rt_unwinding ? 0 : ko_table(uc, 0);
    uint32_t sz[1] = {0xb2};
    uint32_t obj = table && !rt_unwinding ? call_cdecl(F_NEW, 1, sz) : 0;
    if (rt_unwinding) return;
    if (!readable(obj)) {                       /* slice 24: no stage, so give back the list and the table */
        game_free(list);
        game_free(table);
        return;
    }
    uint32_t args[15] = {uc, KO_CLUBS, list, KO_ROUNDS, rd32(g_mem, uc + 4), table, year, (uint32_t)groups, 2, 0, 0x14,
                         1, 0, 0, 0};
    call_this(F_CUP_STAGE, obj, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, uc + 0xc) + 4u * (uint32_t)groups, obj);
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    if (!rt_unwinding) call_this(F_SET_UP_ROUND, obj, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, uc + 0x30, (uint32_t)groups);
    fprintf(stderr, "recomp: uefa: UEFA Cup round of 32 drawn\n");
}

/* ---- Wraps ---- */

void o_005a8f60(void);
void o_0051fc00(void);
void o_00519a90(void);
void o_009030e0(void);

/* The empty function 0x5a8f60: slot 10 (advance_stage) of the UEFA Cup's vtable and of many others. */
RT_WRAP(005a8f60, empty_or_uc_advance)
{
    uint32_t uc = cpu.ecx;
    if (season_playoffs_advance(uc)) {               /* slice 19: a league's play-off (season_playoffs.c) */
        if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
        return;
    }
    if (!groups_layout(uc)) {
        if (season_cup_groups_advance(uc)) {            /* slice 21c: a domestic cup with groups */
            if (!rt_unwinding) rt_ret_cdecl(0);
            return;
        }
        o_005a8f60();
        return;
    }
    int32_t now = (int32_t)rd32(g_mem, uc + 0x30);
    if (now == -1) draw_group_stage(uc);
    else if (now == ko_slot(uc) - 1) draw_knockout(uc);
    else if (now >= 0 && now < ko_slot(uc) - 1) wr32(g_mem, uc + 0x30, (uint32_t)(now + 1));
    if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
}

/* A season whose UEFA Cup has groups: the year of the object (+0x40, the game year that the constructor gives
 * it at 0x902317, before the load) or the year before (a load from January to June). */
static int groups_near(uint32_t uc)
{
    int year = (int16_t)rd16(g_mem, uc + 0x40), k = is_uc(uc) == 'k';
    const SeasonUefa *a = season_uefa_row(year), *b = season_uefa_row(year - 1);
    return (a && (k ? a->ecl_groups : a->uc_groups)) || (b && (k ? b->ecl_groups : b->uc_groups));
}

/* Cup::load_from_disk, thiscall (by_comp_id) ret 4: the stage array of a UEFA Cup before its stages load. The
 * constructor calls it at a new game too (0x90235f), so the array is made only for a season with groups: a game
 * of the original's format (a 2001 start with changing rules on) allocates nothing (Q152). */
RT_WRAP(0051fc00, cup_load)
{
    uint32_t obj = cpu.ecx;
    if (is_uc(obj) && groups_near(obj) && !readable(rd32(g_mem, obj + 0xc))) {
        uint32_t saved_ecx = cpu.ecx;
        ensure_stage_array(obj);
        if (rt_unwinding) return;
        cpu.ecx = saved_ecx;
    }
    if (!is_uc(obj)) {                                  /* slice 21c: a domestic cup with groups */
        uint32_t saved_ecx = cpu.ecx;
        season_cup_groups_load(obj);
        if (rt_unwinding) return;
        cpu.ecx = saved_ecx;
    }
    o_0051fc00();
    if (!rt_unwinding && cpu.eax) season_load_clear_stages(obj);  /* load sweep: the slots after +0x30 */
}

/* Cup::record_season_history 0x519a90 (slot 12): a UEFA Cup with groups records its season from the knockout
 * stage, as EuropeanCup::record_season_honours 0x584ad0 does (a jump to slot +0x30 of the stage). */
RT_WRAP(00519a90, cup_honours)
{
    uint32_t uc = cpu.ecx;
    uint32_t ko = groups_layout(uc) && (int32_t)rd32(g_mem, uc + 0x30) == ko_slot(uc) ? stage_at(uc, ko_slot(uc)) : 0;
    if (!readable(ko)) ko = season_cup_groups_ko(uc);  /* slice 21c: a domestic cup with groups */
    if (!readable(ko)) {
        o_00519a90();
        return;
    }
    cpu.ecx = ko;
    rt_jump(rd32(g_mem, rd32(g_mem, ko) + 0x30));
}

/* Uefa_Cup slot 17 0x9030e0: the base 0x519890 does nothing for a cup with stages (+0x2c, 0x5198a3); here it
 * runs as for the original's UEFA Cup (the title holder of 0x519974, the staff records). A round index past the
 * round table is not read (0x5199e0..0x5199e6).
 *
 * The club's European mark (O270). 0x9030e0 clears club +0x1db for the outcomes 1, 2 and -1 (0x9030e8..0x9030f4):
 * in the original's UEFA Cup, a knockout from start to final, each of them ends the club's cup. With groups the
 * round table ends at the last qualifying round, so its winners get outcome 1 there (the round codes of the access
 * list, 220 to 270, as 0x4b0870 rounds them) and lost the mark, though they go on into the groups. The registration
 * of 2 February (process_euro_registration 0x85a9b0, phase 3, at 0x85aa5c) then left them out, and in the knockout
 * rounds only the players registered in August could play: Leeds United had 10 on 14 February 2012 and the match
 * stopped at match_eng.cpp 652 (0x6b8aca). Such a winner keeps its mark; the group stage takes it off the clubs
 * that go out (draw_knockout, draw_knockout_po). */
RT_WRAP(009030e0, uc_result)
{
    uint32_t uc = cpu.ecx;
    if (!groups_layout(uc)) {
        o_009030e0();
        return;
    }
    uint32_t club = rt_arg(0), fixture = rt_arg(4), mark = readable(club) ? rd32(g_mem, club + 0x1db) : 0;
    int outcome = (int8_t)rt_arg(1), code = readable(fixture) ? (int16_t)rd16(g_mem, fixture + 0x30) : 0;
    int base = code < 1000 ? code - code % 10 : code;
    int keep = outcome == 1 && base >= 220 && base <= 270;
    uint32_t stages = rd32(g_mem, uc + 0x2c);
    wr32(g_mem, uc + 0x2c, 0);
    o_009030e0();
    wr32(g_mem, uc + 0x2c, stages);
    if (keep && !rt_unwinding) wr32(g_mem, club + 0x1db, mark);
}

void o_00903120(void);

/* Uefa_Cup slot 22 0x903120, collect_club_list (out, phase), ret 8: the clubs that the registration of each deadline
 * registers (process_euro_registration 0x85a9b0 at 0x85aa47, phases 1 to 3 on 4 August, 1 September and
 * 2 February, uefa_check_registration_deadlines 0x9058e0). It takes the clubs of one round of the round table: the
 * round count +0x3c less 8, 7 or 4 (0x9031c2..0x9031f1), so in the original's UEFA Cup, a knockout of eight
 * rounds, phase 3 lists the clubs of the round of 16 in February. With groups the round table holds only the
 * qualifying rounds, so phase 3 listed qualifying clubs, and the clubs of the knockout rounds were not registered
 * again (O270: Leeds United had 10 players to play on 14 February 2012, the box match_eng.cpp 612/652). Here
 * phase 3 lists the clubs whose European mark +0x1db is the cup's record id: the clubs still in it (the group
 * stage takes the mark off the clubs that go out, draw_knockout, and a Champions League third gets it,
 * season_uefa.c). The array is the game's own (0x944e46), which the caller frees (0x85aaa9). */
RT_WRAP(00903120, uc_registration_clubs)
{
    uint32_t uc = cpu.ecx, out = rt_arg(0), phase = rt_arg(1) & 0xffu, rec = rd32(g_mem, uc + 4);
    if (phase != 3 || !groups_layout(uc) || !readable(out) || !readable(rec)) {
        o_00903120();
        return;
    }
    uint32_t id = rd32(g_mem, rec), table = rd32(g_mem, 0xae23b4u);
    int32_t clubs = (int32_t)rd32(g_mem, 0xae235cu), n = 0;
    for (int32_t i = 0; readable(table) && i < clubs; i++)
        n += rd32(g_mem, table + (uint32_t)i * 0x245u + 0x1db) == id;
    uint32_t list = game_malloc(4u * (uint32_t)(n ? n : 1));
    if (rt_unwinding) return;
    int32_t k = 0;
    for (int32_t i = 0; readable(list) && i < clubs && k < n; i++) {
        uint32_t club = table + (uint32_t)i * 0x245u;
        if (rd32(g_mem, club + 0x1db) == id) wr32(g_mem, list + 4u * (uint32_t)k++, club);
    }
    wr32(g_mem, out, list);
    rt_ret_stdcall(readable(list) ? (uint32_t)k : 0u, 2);
}

/* ---- The Intertoto Cup of 3 rounds, its 11 winners in the UEFA Cup Q2 (change point U3: 2006 to 2008) ----
 *
 * The original Intertoto Cup (Intertoto_Cup, vtable 0x96bb8c) plays 5 rounds (create_round_table 0x633300:
 * codes 10, 20, 30, the semi-finals and the final; 40, 32 and 24 clubs in the first three rounds, which take
 * 40, 12 and 8 new clubs of its 60 entrants from slots 0, 40 and 52). Its update_for_date 0x633d40 copies the
 * clubs that won the last round (entry byte +5 = 1) into the UEFA Cup's entry table from slot count - 0x1b
 * (0x633deb), 3 of them (0x633df3). In this format its first three rounds are the whole cup: 40, 36 and 22
 * clubs with 40, 16 and 4 new clubs (the seeding still gives 60, 0x905003), and the 11 winners of the third
 * round go to the end of the UEFA Cup's Q2 slots. The UEFA Cup's entry table grows by 8 slots for them
 * (161 are allocated, 0x902b9d): build_entrants 0x902b90 is wrapped to move the seeded clubs after the Q2
 * slots up by 11 and to make the list +0x14 (count slots, 0x902dff) as long. */

#define UIC_VTABLE 0x0096bb8cu
#define UIC_WINNERS 11
#define UC_SLOTS_ORIGINAL 145           /* 0x902ba7 */
#define UC_SLOTS_UIC 153                /* 145 and 8 more Intertoto winners; the table holds 161 (0x902b9d) */

static int uic_to_q2(const SeasonUefa *r)
{
    return r && r->uic_rounds == 3 && r->uic_winners == UIC_WINNERS && r->uic_entry && !strcmp(r->uic_entry, "Q2") &&
           r->uc_groups == 8 && season_uefa_cl_round_of_16(r);
}

/* The UEFA Cup takes the 11 winners: it has 9 stages, 153 slots, and the Intertoto Cup has 3 rounds. */
static int uic_handover(uint32_t uc, uint32_t uic)
{
    return groups_layout(uc) && (int16_t)rd16(g_mem, uc + 0x3e) == UC_SLOTS_UIC && readable(uic) &&
           (int16_t)rd16(g_mem, uic + 0x3c) == 3 && readable(rd32(g_mem, uc + 0xa3));
}

/* 0x633deb: the slot of the first winner, counted back from the UEFA Cup's slot count (0x1b in the original):
 * the last 11 new slots of its Q2 (round 1 of its table: +0x1e first slot, +0x1c new clubs). */
uint32_t rt_uefa_uic_back(uint32_t uc, uint32_t uic)
{
    if (!uic_handover(uc, uic)) return 0x1bu;
    uint32_t q2 = rd32(g_mem, uc + 0xa3) + ROUND_SIZE;
    int start = (int16_t)rd16(g_mem, q2 + 0x1e) + (int16_t)rd16(g_mem, q2 + 0x1c) - UIC_WINNERS;
    return (uint32_t)((int16_t)rd16(g_mem, uc + 0x3e) - start);
}

/* 0x633df3: the number of winners (3 in the original). */
uint32_t rt_uefa_uic_winners(uint32_t uc, uint32_t uic) { return uic_handover(uc, uic) ? UIC_WINNERS : 3u; }

void o_00633300(void);
void o_00902b90(void);

/* Intertoto_Cup::create_round_table, thiscall (stage, &rounds, &name, &league) ret 0x10: stage -1 at the
 * constructor and at each new season. The table of 5 rounds stays allocated; the cup plays its first 3. */
RT_WRAP(00633300, uic_round_table)
{
    uint32_t uic = cpu.ecx, out_rounds = rt_arg(1);
    int8_t stage = (int8_t)rt_arg(0);
    o_00633300();
    if (rt_unwinding || stage != -1 || !readable(uic) || rd32(g_mem, uic) != UIC_VTABLE) return;
    uint32_t t = cpu.eax;
    if (!readable(t) || !uic_to_q2(season_uefa_row((int16_t)rd16(g_mem, uic + 0x40)))) return;
    if ((int16_t)rd16(g_mem, t + 7) != 10 || (int16_t)rd16(g_mem, t + ROUND_SIZE + 7) != 20 ||
        (int16_t)rd16(g_mem, t + 2 * ROUND_SIZE + 7) != 30)
        return;
    set_round(t, 0, 10, 40, 40, 0, rd16(g_mem, t + 0xd), 0);
    set_round(t + ROUND_SIZE, 1, 20, 36, 16, 40, rd16(g_mem, t + ROUND_SIZE + 0xd), 0);
    set_round(t + 2 * ROUND_SIZE, 2, 30, 22, 4, 56, rd16(g_mem, t + 2 * ROUND_SIZE + 0xd), 0);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, 3);
    fprintf(stderr, "recomp: uefa: Intertoto Cup %d with 3 rounds\n", (int16_t)rd16(g_mem, uic + 0x40));
}

/* Uefa_Cup::build_entrants, thiscall () returning 1: the gap of the Intertoto winners in the Q2 slots. */
#define UC_ENTRANTS_CONSTRUCTOR 0x00902383u     /* the return address of the call at 0x90237e in 0x9022e0 */

RT_WRAP(00902b90, uc_build_entrants)
{
    uint32_t uc = cpu.ecx;
    /* Slice 23: the entrants of the access list (season_uefa_qual.c) in place of the original's. */
    int kind = is_uc(uc);
    /* They free the cup's old entry table +0xa7 first. The constructor 0x9022e0 calls here (return 0x902383)
     * before anything sets +0xa7: Comp::Comp 0x48c580 sets the list +0x14 to 0 (0x48c5d1), but neither it nor
     * Cup::Cup 0x518640 sets +0xa7, so it holds what operator new left. A Conference League made at a season
     * change takes a used heap block, and its stale +0xa7 was freed again ("free of ... not a heap block", a
     * 2020 start with Greece and changing rules on). Only for a cup of the access list: the base game's UEFA
     * Cup takes the original's build_entrants, which writes +0xa7 itself. */
    const SeasonUefa *row = season_uefa_row((int16_t)rd16(g_mem, uc + 0x40));
    if (kind && rd32(g_mem, cpu.esp) == UC_ENTRANTS_CONSTRUCTOR && row && season_uefa_access_places(row->season, 'c', 1))
        wr32(g_mem, uc + 0xa7, 0);
    if ((kind == 'e' && season_uefa_uc_entrants(uc)) || (kind == 'k' && season_uefa_ecl_entrants(uc))) {
        rt_ret_cdecl(1);
        return;
    }
    if (rt_unwinding) return;
    /* Slice 24: the Conference League has no entrants of the original (it would take the Europa League's). */
    if (kind == 'k') {
        fprintf(stderr, "recomp: uefa: Conference League %d: the access list gives no layout\n",
                (int16_t)rd16(g_mem, uc + 0x40));
        wr16(g_mem, uc + 0x36, 0);
        rt_ret_cdecl(1);
        return;
    }
    o_00902b90();
    if (rt_unwinding || cpu.eax != 1 || is_uc(uc) != 'e') return;
    const SeasonUefa *r = season_uefa_row((int16_t)rd16(g_mem, uc + 0x40));
    if (!uic_to_q2(r) || g_mem[uc + 0xb2] != 0 || (int16_t)rd16(g_mem, uc + 0x3e) != UC_SLOTS_ORIGINAL) return;
    /* The sizes of uc_round_table with 153 slots: Q1 and the seeded clubs of Q2 come before the winners. */
    int r1 = season_uefa_uc_round(r, "R1"), q2 = season_uefa_uc_round(r, "Q2");
    int q1 = 2 * (UC_SLOTS_UIC - 8 - r1) - q2;
    int before = q1 + q2 - q1 / 2 - UIC_WINNERS, seeded = UC_SLOTS_ORIGINAL - 0x1b;     /* 118 (0x902cb0) */
    if (r1 != 80 || q2 <= 0 || q2 % 2 || q1 < 2 || before < q1 || before > seeded) return;
    Cpu saved = cpu;
    int16_t listed = (int16_t)rd16(g_mem, uc + 0x36);
    uint32_t list = game_malloc(6u * UC_SLOTS_UIC);
    if (rt_unwinding || !list) return;
    uint32_t old = rd32(g_mem, uc + 0x14);
    if (readable(old) && listed > 0) memcpy(g_mem + list, g_mem + old, 6u * (uint32_t)listed);
    game_free(old);
    if (rt_unwinding) return;
    wr32(g_mem, uc + 0x14, list);
    wr16(g_mem, uc + 0x38, UC_SLOTS_UIC);
    uint32_t e = rd32(g_mem, uc + 0xa7);
    memmove(g_mem + e + 6u * (uint32_t)(before + UIC_WINNERS), g_mem + e + 6u * (uint32_t)before,
            6u * (uint32_t)(seeded - before));
    memset(g_mem + e + 6u * (uint32_t)before, 0, 6u * UIC_WINNERS);
    memset(g_mem + e + 6u * (uint32_t)(seeded + UIC_WINNERS), 0, 6u * (uint32_t)(UC_SLOTS_UIC - seeded - UIC_WINNERS));
    wr16(g_mem, uc + 0x3e, UC_SLOTS_UIC);
    cpu = saved;
}
