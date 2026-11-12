/* Group stages in domestic cups (0.6.0 slice 21c, docs/research/2026-10-18-cups-more.md, section 1): a group stage
 * that feeds a knockout stage, for a Cup class of the original in the seasons of season_cup_groups (season_cups.h):
 * the Scottish League Cup from 2016/17, the EFL Trophy from 2016/17 and the Turkish Cup in its group seasons.
 *
 * It is built as the Coppa Italia builds its stages (ItaCup: League_Stage groups, create_group_stage 0x63f640, then a
 * Cup_Stage knockout from the rounds of its create_schedule stage 7, create_knockout_stage 0x63f870) and as slice 22
 * builds the UEFA Cup groups on the Cup base (season_uefa_cup.c). A Cup object plays its own round table first (the
 * base rounds, while +0xb1 is 0), then its stages: Cup::update_for_date 0x51f890 updates every stage up to +0x30
 * (0x51f957) and, once +0xb1 is set and every stage up to +0x30 is complete, calls slot 10 (advance_stage) while
 * +0x30 < +0x2c - 1 (0x51f8da..0x51f8f5), else records the season. In a season with a group format:
 * - after the class's create_round_table (stage -1, vtable +0x3c; called by set_up_rounds at each new season and by the
 *   constructor) the object gets +0x2c = groups + 1 stages. With no base rounds +0xb1 is set at once, so the first
 *   update calls slot 10; with base rounds the table keeps its first base_rounds rounds, and the clubs of the later
 *   slots, which no base round takes, are marked 0xff after map_fixture_tree 0x518790 resets them (the cup counts its
 *   base rounds done when no club has 0, process_fixture 0x51a150 at 0x51b3c8), so the base rounds end as before;
 * - slot 10 with +0x30 = -1 draws the groups (League_Stage 0x68b100, one per group, in stages 0 to groups - 1): the
 *   winners of the last base round (entry byte +5 = 1, 0x51b2b9) and the later slots, or with no base rounds every
 *   club of the team list, less the byes (the clubs whose +0x1db is the Champions League or the UEFA Cup, the test of
 *   ScoLgeCup::collect_entrants 0x7f0400), in pots by division level and last place;
 * - slot 10 with every group complete draws the knockout (Cup_Stage 0x522e00 in stage groups): the group winners, the
 *   byes, the other places that go through and the best of the next place (points per match, goal difference, goals),
 *   seeded clubs against the others, not of their own group when a draw is found, on the rounds of the format.
 * The season's honours come from the knockout stage (the wraps of Cup::record_season_honours 0x519a90 in
 * season_uefa_cup.c and of 0x595160), and Cup::update_staff_records_for_status 0x519890 runs as for a cup with no
 * stages (it does nothing with +0x2c > 0, 0x5198a3), as slice 22 does for the UEFA Cup. A save keeps +0x2c, +0x30 and
 * the stages (Cup::save_cup_to_disk 0x5210f0); the stage array is made before a load (season_cup_groups_load).
 *
 * The EFL Trophy (EngAutoCup) is a Cup with two stages of its own: its regional half in a Cup_Stage
 * (create_section_stage 0x56bab0, from the clubs at +0xb2) and the final (advance_stage 0x56bc50,
 * create_final_stage 0x56bce0). In a group season the team list takes the clubs of both halves, the section stage
 * is not made, slot 10 is the group draw, its rank_teams 0x56c010 (it reads the team list of stage 0 at 0x56c06b)
 * does not run, and its slot 12 0x595160 (the honours of stage 1) takes the knockout stage.
 *
 * The base game (2001/02, changing rules off) and RECOMP_SEASON_CODE=off get none of it (Q152, Q342). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_cups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUP_STAGES 0xcu                 /* Comp ** */
#define CUP_STAGE_COUNT 0x2cu           /* s32 */
#define CUP_STAGE_NOW 0x30u             /* s32 */
#define CUP_YEAR 0x40u
#define CUP_ROUND_COUNT 0x3cu
#define CUP_TEAMS 0x3eu
#define CUP_ROUNDS 0xa3u
#define CUP_TEAM_LIST 0xa7u
#define CUP_BASE_DONE 0xb1u             /* u8: the base rounds are played */
#define AUTO_SECTION 0xb2u              /* EngAutoCup: the clubs of its other half (create_section_stage 0x56bad6) */
#define ROUND_SIZE 0x68u
#define GROUP_ROUND_SIZE 0x41u          /* a match day of a League round table (0x68a160) */
#define KEY_CL 0x9cf6e8u                /* key id globals that ScoLgeCup::collect_entrants tests against club +0x1db */
#define KEY_UC 0x9cf6f0u
#define CLUB_EUROPE 0x1dbu
#define CLUB_DIVISION 0x57u
#define CLUB_LAST_DIVISION 0x5bu
#define CLUB_LAST_PLACE 0x5fu
#define MAX_GROUPS 16
#define MAX_CLUBS 128

#define F_LEAGUE_ROW 0x006826d0u        /* League::get_table_row, thiscall (index) */
#define F_RANDOM 0x0090cfc0u            /* utils_random, cdecl (range) */
#define F_CALLOC 0x00945501u            /* calloc, cdecl (count, size) */
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_NEW 0x00944cf1u
#define F_LEAGUE_STAGE 0x0068b100u      /* League_Stage::League_Stage, thiscall, 22 arguments */
#define F_GROUP_NEWS 0x00684230u        /* League::send_group_draw_news, thiscall () */
#define F_CUP_STAGE 0x00522e00u         /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u      /* Cup::set_up_round, thiscall (round) */
#define F_DRAW_DATE 0x00521e60u
#define F_MATCH_DATE 0x00521eb0u
#define F_GROUP_DATE 0x0068a160u        /* league_round_set_date, cdecl (table, round, day, month, year+, weekday, time,
                                         * year, 0) */
#define F_GROUP_SLOT 0x0068a1c0u        /* league_round_set_match_slot, cdecl (table, round, slot, weekday, kind,
                                         * option, value) */

static int readable(uint32_t p) { return rt_readable(p); }

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

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !readable(p)) return 0;
    memset(g_mem + p, 0, size);
    return p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p)) call_cdecl(F_FREE, 1, a);
}

static int random_below(int n)
{
    if (n <= 1) return 0;
    uint32_t a[1] = {(uint32_t)n};
    int k = (int)call_cdecl(F_RANDOM, 1, a);
    return k < 0 || k >= n ? 0 : k;
}

static int season_game(void)
{
    const char *v = getenv("RECOMP_CUPS");
    if (v && !strcmp(v, "off")) return 0;
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

static int cup_season(uint32_t cup)
{
    if (!season_changing_rules()) return season_start_year();
    int y = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    return y >= SEASON_FIRST_YEAR && y <= SEASON_LAST_YEAR ? y : season_start_year();
}

/* Test only: RECOMP_CUP_GROUPS_TEST=1 gives the League Cup the test format (season_cups_more.c). */
static int test_on(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_CUP_GROUPS_TEST") && *getenv("RECOMP_CUP_GROUPS_TEST");
    return on;
}

static const char *format_file(uint32_t cup)
{
    const char *f = season_change_file(cup);
    if (test_on() && !strcmp(f, "eng_cc_cup")) return "test_eng_cc_cup";
    return f;
}

static const SeasonCupGroups *format_at(uint32_t cup, int season)
{
    return season_game() && readable(cup) ? season_cup_groups(format_file(cup), season) : NULL;
}

/* The object plays a group format: its season has one and it holds groups + 1 stages. */
static const SeasonCupGroups *layout(uint32_t cup)
{
    if (!readable(cup)) return NULL;                   /* the wrap of 0x5a8f60 asks for any ecx (rt.h, rt_readable) */
    const SeasonCupGroups *g = format_at(cup, cup_season(cup));
    return g && (int32_t)rd32(g_mem, cup + CUP_STAGE_COUNT) == g->groups + 1 ? g : NULL;
}

static int is_auto_cup(uint32_t cup) { return !strcmp(season_change_file(cup), "eng_auto_cup"); }

static uint32_t stage_at(uint32_t cup, int i) { return rd32(g_mem, rd32(g_mem, cup + CUP_STAGES) + 4u * (uint32_t)i); }

/* Deletes the stages up to +0x30 (as EuropeanCup::start_new_season 0x584e04..0x584e28). */
static void free_stages(uint32_t cup)
{
    uint32_t arr = rd32(g_mem, cup + CUP_STAGES);
    int32_t now = (int32_t)rd32(g_mem, cup + CUP_STAGE_NOW);
    for (int i = 0; readable(arr) && i <= now && i <= MAX_GROUPS && !rt_unwinding; i++) {
        uint32_t st = rd32(g_mem, arr + 4u * (uint32_t)i);
        if (readable(st)) {
            uint32_t a[1] = {1};
            call_this(rd32(g_mem, rd32(g_mem, st)), st, 1, a);     /* the scalar deleting destructor */
        }
        if (!rt_unwinding) wr32(g_mem, arr + 4u * (uint32_t)i, 0);
    }
    if (!rt_unwinding) wr32(g_mem, cup + CUP_STAGE_NOW, 0xffffffffu);
}

/* The big stage arrays made in this game: the cup and its array. A pair counts only while the cup still holds that
 * array; the list is cleared at each new game, load and end of a game (season_cup_groups_reset from
 * season_change_reset), because the classes' destructors free the arrays and malloc gives the addresses again. */
#define MAX_MADE 64
static struct { uint32_t cup, arr; } made[MAX_MADE];

void season_cup_groups_reset(void) { memset(made, 0, sizeof made); }

/* A stage array of MAX_GROUPS + 1 slots (the class's array, two slots in EngAutoCup, goes). */
static int big_stage_array(uint32_t cup)
{
    uint32_t arr = rd32(g_mem, cup + CUP_STAGES);
    for (int i = 0; i < MAX_MADE; i++)
        if (arr && made[i].cup == cup && made[i].arr == arr) return 1;
    uint32_t big = game_malloc(4u * (MAX_GROUPS + 1));
    if (!big) return 0;
    if (readable(arr)) {
        memcpy(g_mem + big, g_mem + arr, 8);
        game_free(arr);
    }
    wr32(g_mem, cup + CUP_STAGES, big);
    int slot = MAX_MADE - 1;
    for (int i = 0; i < MAX_MADE; i++)
        if (made[i].cup == cup || !made[i].cup) {
            slot = i;
            break;
        }
    made[slot].cup = cup, made[slot].arr = big;
    return 1;
}

/* The slot after the last that a base round takes (+0x1e first slot, +0x1c new clubs). */
static int base_slots(uint32_t cup, int rounds)
{
    uint32_t t = rd32(g_mem, cup + CUP_ROUNDS);
    int end = 0;
    for (int r = 0; readable(t) && r < rounds; r++) {
        int e = (int16_t)rd16(g_mem, t + (uint32_t)r * ROUND_SIZE + 0x1e) +
                (int16_t)rd16(g_mem, t + (uint32_t)r * ROUND_SIZE + 0x1c);
        if (e > end) end = e;
    }
    return end;
}

static int club_count(uint32_t cup)
{
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST);
    int n = 0, count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    for (int i = 0; readable(list) && i < count && i < 512; i++)
        if (readable(rd32(g_mem, list + 6u * (uint32_t)i))) n++;
    return n;
}

/* EngAutoCup: one team list of the clubs of both halves (+0xa7 and +0xb2, +0x3e clubs each). */
static void merge_auto_halves(uint32_t cup)
{
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST), other = rd32(g_mem, cup + AUTO_SECTION);
    int n = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    if (!readable(list) || !readable(other) || n <= 0 || n > 64) return;
    uint32_t all = game_malloc(6u * (uint32_t)(2 * n));
    if (!all) return;
    memcpy(g_mem + all, g_mem + list, 6u * (uint32_t)n);
    for (int i = 0; i < n; i++) wr32(g_mem, all + 6u * (uint32_t)(n + i), rd32(g_mem, other + 4u * (uint32_t)i));
    game_free(list);
    if (rt_unwinding) return;
    wr32(g_mem, cup + CUP_TEAM_LIST, all);
    wr16(g_mem, cup + CUP_TEAMS, (uint16_t)(2 * n));
}

/* After the class's create_round_table: self, stage and count_ptr as the wrap read them, the table in cpu.eax.
 * Only a set-up of the season's rounds acts: it asks for stage -1 with the cup's own round count +0x3c, as
 * Cup::set_up_rounds 0x4c15f0 (call at 0x4c160a) and the constructors do (ScoLgeCup 0x7efad3, EngAutoCup 0x56ab7e and
 * its set_up_rounds 0x56b71a, TurCup 0x8fc489, RusCup 0x7e9307: lea reg, [esi + 0x3c]). A caller that only reads the
 * tables passes its own buffer, for example Comp::build_match_date_list 0x48d500 (the Schedule tab, scout duty:
 * lea ecx, [esp + 0x16], call at 0x48d5c4); there the groups of a running season must stay (free_stages below would
 * delete them, and merge_auto_halves would read a list it already merged). */
void season_cup_groups_table(uint32_t cup, uint32_t stage, uint32_t count_ptr)
{
    if (stage != 0xffu || count_ptr != cup + CUP_ROUND_COUNT || !season_game() || !readable(cup)) return;
    const SeasonCupGroups *g = format_at(cup, cup_season(cup));
    int32_t had = (int32_t)rd32(g_mem, cup + CUP_STAGE_COUNT), class_count = is_auto_cup(cup) ? 2 : 0;
    Cpu saved = cpu;
    if (!g) {
        /* A season with no groups after one with groups: the class's stages again. */
        if (had != class_count && had > 2 && readable(rd32(g_mem, cup + CUP_STAGES))) {
            free_stages(cup);
            if (rt_unwinding) return;
            wr32(g_mem, cup + CUP_STAGE_COUNT, (uint32_t)class_count);
            fprintf(stderr, "recomp: cup groups: %s %d: no groups this season\n", format_file(cup), cup_season(cup));
            cpu = saved;
        }
        return;
    }
    if (is_auto_cup(cup)) merge_auto_halves(cup);
    if (rt_unwinding) return;
    int clubs = club_count(cup);
    if (clubs < 2 * g->groups) return;  /* the class has not picked its clubs yet (ScoLgeCup 0x7f0230) */
    if ((int32_t)rd32(g_mem, cup + CUP_STAGE_NOW) >= 0 && readable(rd32(g_mem, cup + CUP_STAGES))) free_stages(cup);
    if (rt_unwinding || !big_stage_array(cup)) return;
    wr32(g_mem, cup + CUP_STAGE_COUNT, (uint32_t)(g->groups + 1));
    wr32(g_mem, cup + CUP_STAGE_NOW, 0xffffffffu);
    int rounds = readable(count_ptr) ? (int16_t)rd16(g_mem, count_ptr) : 0;
    if (g->base_rounds > 0 && g->base_rounds < rounds) wr16(g_mem, count_ptr, (uint16_t)g->base_rounds);
    else if (g->base_rounds == 0) g_mem[cup + CUP_BASE_DONE] = 1;
    cpu = saved;
    fprintf(stderr, "recomp: cup groups: %s %d with %d groups after %d rounds (%d clubs)\n", format_file(cup),
            cup_season(cup), g->groups, g->base_rounds, clubs);
}

void season_cup_groups_read_test(uint32_t cup)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_CUP_GROUPS_READ") && *getenv("RECOMP_CUP_GROUPS_READ") == '1';
    if (!on || !readable(cup)) return;
    /* As Comp::build_match_date_list 0x48d500 at 0x48d5a1..0x48d5c4: stage -1 and three buffers on its own stack,
     * then the table goes back to the game's heap. */
    Cpu saved = cpu;
    cpu.esp -= 0x40;
    uint32_t buf = cpu.esp;
    uint32_t a[4] = {0xffffffffu, buf + 0x10, buf + 0x20, buf + 0x30};
    uint32_t t = call_this(rd32(g_mem, rd32(g_mem, cup) + 0x3c), cup, 4, a);
    if (rt_unwinding) return;
    cpu = saved;
    game_free(t);
    if (rt_unwinding) return;
    cpu = saved;
}

/* After map_fixture_tree 0x518790 of a cup with base rounds: the later slots count as done. */
static void mark_later_slots(uint32_t cup)
{
    const SeasonCupGroups *g = layout(cup);
    if (!g || g->base_rounds <= 0) return;
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST);
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    int from = base_slots(cup, (int16_t)rd16(g_mem, cup + CUP_ROUND_COUNT));
    for (int i = from; readable(list) && i < count && i < 512; i++) g_mem[list + 6u * (uint32_t)i + 5] = 0xff;
}

/* ---- The groups ---- */

static int is_bye(uint32_t club)
{
    int32_t e = (int32_t)rd32(g_mem, club + CLUB_EUROPE);
    return e >= 0 && (e == (int32_t)rd32(g_mem, KEY_CL) || e == (int32_t)rd32(g_mem, KEY_UC));
}

/* A strength for the pots: the level of the club's division (record +0x69), then its last place. */
static int strength(uint32_t club)
{
    uint32_t div = rd32(g_mem, club + CLUB_DIVISION), last = rd32(g_mem, club + CLUB_LAST_DIVISION);
    int level = readable(div) ? (int16_t)rd16(g_mem, div + 0x69) : 0;
    int place = readable(last) && last == div ? (int8_t)g_mem[club + CLUB_LAST_PLACE] : 40;
    return level * 100 - (place > 0 ? place : 40);
}

static void sort_by_strength(uint32_t *c, int n)
{
    for (int i = 1; i < n; i++) {
        uint32_t x = c[i];
        int s = strength(x), k = i;
        while (k > 0 && strength(c[k - 1]) < s) c[k] = c[k - 1], k--;
        c[k] = x;
    }
}

static uint32_t group_table(const SeasonCupGroups *g, int days, uint32_t year)
{
    uint32_t t = game_malloc((uint32_t)days * GROUP_ROUND_SIZE);
    for (int d = 0; t && d < days && !rt_unwinding; d++) {
        uint32_t a[9] = {t, (uint32_t)d, (uint32_t)g->days[d][0], (uint32_t)g->days[d][1], (uint32_t)g->days[d][2],
                         0xffffffffu, 2, year, 0};
        call_cdecl(F_GROUP_DATE, 9, a);
        uint32_t s[7] = {t, (uint32_t)d, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
        if (!rt_unwinding) call_cdecl(F_GROUP_SLOT, 7, s);
    }
    return rt_unwinding ? 0 : t;
}

/* A group draw that cannot finish (more than 10 match days, or no memory): the groups made so far go and the
 * class's rounds play, as when the clubs do not fit the groups. Without this +0x30 stays -1 and
 * Cup::update_for_date calls slot 10 every day (0x51f8da..0x51f8f5), so the cup never ends. */
static void groups_failed(uint32_t cup, const SeasonCupGroups *g, int group, int days)
{
    fprintf(stderr, "recomp: cup groups: %s: the draw of group %d failed (%d match days), the class's rounds play\n",
            format_file(cup), group, days);
    (void)g;
    wr32(g_mem, cup + CUP_STAGE_NOW, (uint32_t)(group - 1));     /* free_stages deletes the groups 0 to group - 1 */
    free_stages(cup);
    if (rt_unwinding) return;
    wr32(g_mem, cup + CUP_STAGE_COUNT, 0);
    g_mem[cup + CUP_BASE_DONE] = 0;
}

static void draw_groups(uint32_t cup, const SeasonCupGroups *g)
{
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST);
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    int from = g->base_rounds > 0 ? base_slots(cup, (int16_t)rd16(g_mem, cup + CUP_ROUND_COUNT)) : 0;
    uint32_t clubs[MAX_CLUBS];
    int n = 0;
    for (int i = 0; readable(list) && i < count && i < 512 && n < MAX_CLUBS; i++) {
        uint32_t club = rd32(g_mem, list + 6u * (uint32_t)i);
        if (!readable(club)) continue;
        int in = g->base_rounds == 0 || i >= from || g_mem[list + 6u * (uint32_t)i + 5] == 1;
        if (in && !(g->byes && is_bye(club))) clubs[n++] = club;
    }
    int sizes[MAX_GROUPS];
    if (!season_cup_group_sizes(n, g->groups, sizes)) {
        fprintf(stderr, "recomp: cup groups: %s: %d clubs for %d groups, the class's rounds play\n",
                format_file(cup), n, g->groups);
        wr32(g_mem, cup + CUP_STAGE_COUNT, 0);
        g_mem[cup + CUP_BASE_DONE] = 0;
        return;
    }
    sort_by_strength(clubs, n);
    /* Pots of `groups` clubs: one club of each pot a group, the place in the group by chance. */
    uint32_t members[MAX_GROUPS][10];
    int filled[MAX_GROUPS] = {0};
    for (int p = 0; p * g->groups < n && !rt_unwinding; p++) {
        int used[MAX_GROUPS] = {0};
        for (int k = 0; k < g->groups && p * g->groups + k < n; k++) {
            int cand[MAX_GROUPS], c = 0;
            for (int x = 0; x < g->groups; x++)
                if (!used[x] && filled[x] < sizes[x]) cand[c++] = x;
            if (!c) break;
            int x = cand[random_below(c)];
            if (rt_unwinding) return;
            used[x] = 1;
            members[x][filled[x]++] = clubs[p * g->groups + k];
        }
    }
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cup + CUP_YEAR);
    for (int x = 0; x < g->groups && !rt_unwinding; x++) {
        int days = season_cup_group_days(filled[x], g->meetings);
        uint32_t mem = game_malloc(4u * (uint32_t)filled[x]);
        uint32_t table = days <= 10 ? group_table(g, days, year) : 0;
        uint32_t sz[1] = {0xee};                                      /* as 0x5860b3 */
        uint32_t obj = mem && table ? call_cdecl(F_NEW, 1, sz) : 0;
        if (rt_unwinding) return;
        if (!readable(obj)) {
            game_free(mem);
            if (!rt_unwinding) game_free(table);
            if (!rt_unwinding) groups_failed(cup, g, x, days);
            return;
        }
        for (int k = 0; k < filled[x]; k++) wr32(g_mem, mem + 4u * (uint32_t)k, members[x][k]);
        uint32_t saved_esp = cpu.esp;
        cpu.esp -= 16;
        uint32_t order = cpu.esp, places = cpu.esp + 8;
        g_mem[order] = 4, g_mem[order + 1] = 1, g_mem[order + 2] = 2, g_mem[order + 3] = 0;
        g_mem[places] = (uint8_t)g->per_group, g_mem[places + 1] = 0, g_mem[places + 2] = 0, g_mem[places + 3] = 0;
        /* The arguments of season_uefa_cup.c (as EuropeanCup::draw_phase2_groups 0x5860dd..0x58611d): parent, clubs,
         * list, meetings, record, round table, match days, 3 and 1 points, 8, order, places, year, stage, the group
         * name (0x43d + stage, Group A to Group H, sweep-europe 0x583eda; groups after the eighth repeat the names),
         * 0xf, 2, 0, 8, -1, 0, 2. */
        uint32_t a[22] = {cup, (uint32_t)filled[x], mem, (uint32_t)g->meetings, rd32(g_mem, cup + 4), table,
                          (uint32_t)days, 3, 1, 8, order, places, year, (uint32_t)x, 0x43du + (uint32_t)(x % 8),
                          0xf, 2, 0, 8, 0xffffffffu, 0, 2};
        call_this(F_LEAGUE_STAGE, obj, 22, a);
        if (rt_unwinding) return;
        cpu.esp = saved_esp;
        wr32(g_mem, rd32(g_mem, cup + CUP_STAGES) + 4u * (uint32_t)x, obj);
        game_free(mem);
        game_free(table);
        if (!rt_unwinding) call_this(F_GROUP_NEWS, obj, 0, NULL);
    }
    if (rt_unwinding) return;
    wr32(g_mem, cup + CUP_STAGE_NOW, (uint32_t)(g->groups - 1));
    fprintf(stderr, "recomp: cup groups: %s %u/%02u: %d clubs drawn in %d groups\n", format_file(cup), year,
            (year + 1) % 100, n, g->groups);
}

/* ---- The knockout ---- */

static uint32_t row_at(uint32_t stage, int pos)
{
    uint32_t a[1] = {(uint32_t)pos};
    uint32_t row = call_this(F_LEAGUE_ROW, stage, 1, a);
    return rt_unwinding || !readable(row) ? 0 : row;
}

/* Points a match, then goal difference, then goals: a row's figures of the table section +0xad of its stage
 * (league_compare_table_rows 0x683210: points at +0x19, goals for +0x15 and against +0x17, 0xe bytes a section). */
static double row_score(uint32_t stage, uint32_t row, int clubs, int meetings)
{
    uint32_t at = row + 0xeu * (uint32_t)(int32_t)rd32(g_mem, stage + 0xad);
    int points = (int16_t)rd16(g_mem, at + 0x19);
    int gf = (int16_t)rd16(g_mem, at + 0x15), ga = (int16_t)rd16(g_mem, at + 0x17);
    int games = (clubs - 1) * meetings;
    return (games > 0 ? 1000.0 * points / games : 0) + (gf - ga) + gf / 1000.0;
}

static void build_ko_table(uint32_t t, const SeasonCupGroups *g, int ko_clubs, int rounds, uint32_t year)
{
    int teams = ko_clubs;
    for (int r = 0; r < rounds && !rt_unwinding; r++, teams /= 2) {
        const SeasonNewRound *spec = &g->ko[rounds - 1 - r];
        uint32_t rec = t + (uint32_t)r * ROUND_SIZE;
        int code = season_new_cup_round_code(rounds, r);
        wr16(g_mem, rec + 7, (uint16_t)code);
        wr16(g_mem, rec + 9, (uint16_t)r);
        wr16(g_mem, rec + 0xb, 0);
        /* The first round: the list's pairs (map_fixture_tree 0x519279). */
        uint16_t pairs = r == 0 ? 0x200u : 0;
        if (spec->legs == 2) {
            wr16(g_mem, rec + 0xd, pairs);
            wr16(g_mem, rec + 0xf, spec->flags);
            wr16(g_mem, rec + 0x11, spec->flags);
        } else {
            wr16(g_mem, rec + 0xd, (uint16_t)(pairs | spec->flags));
            wr16(g_mem, rec + 0xf, 0);
            wr16(g_mem, rec + 0x11, 0);
        }
        g_mem[rec + 0x17] = (uint8_t)(code >= 120 ? 6 : 4);
        wr16(g_mem, rec + 0x18, (uint16_t)teams);
        wr16(g_mem, rec + 0x1a, (uint16_t)(teams / 2));
        wr16(g_mem, rec + 0x1c, (uint16_t)(r == 0 ? teams : 0));
        wr16(g_mem, rec + 0x1e, 0);
        g_mem[rec + 0x20] = 0;
        g_mem[rec + 0x21] = (uint8_t)spec->legs;
        g_mem[rec + 0x22] = (uint8_t)(spec->legs == 2 ? spec->gap : 0);
        int dd = spec->day - 10, dm = spec->month, dy = spec->year;
        if (dd < 1) {
            dd += 28;
            if (--dm < 0) dm = 11, dy--;
        }
        uint32_t d[7] = {t, (uint32_t)r, (uint32_t)dd, (uint32_t)dm, (uint32_t)(int32_t)dy, 0xffffffffu, year};
        call_cdecl(F_DRAW_DATE, 7, d);
        uint32_t m[9] = {t, (uint32_t)r, (uint32_t)spec->day, (uint32_t)spec->month, (uint32_t)(int32_t)spec->year,
                         0xffffffffu, 1, year, r == rounds - 1 ? 4u : 0u};
        if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m);
    }
}

/* 1 when a club is in one of the group stages of the cup. */
static int in_groups(uint32_t cup, const SeasonCupGroups *g, uint32_t club)
{
    for (int x = 0; x < g->groups; x++) {
        uint32_t st = stage_at(cup, x);
        int clubs = readable(st) ? (int16_t)rd16(g_mem, st + 0x3e) : 0;
        for (int pos = 0; pos < clubs && pos < 10; pos++) {
            uint32_t row = row_at(st, pos);
            if (rt_unwinding) return 0;
            if (row && rd32(g_mem, row) == club) return 1;
        }
    }
    return 0;
}

/* A knockout that cannot be drawn: the cup ends after its groups (+0x2c = groups, so Cup::update_for_date records
 * the season once every group is complete, 0x51f8da..0x51f8f5), with a log line. Without this +0x30 stays at the
 * last group and the update calls slot 10 every day, so the cup never ends or starts a new season (0x5d5ccb). */
static void ko_failed(uint32_t cup, const SeasonCupGroups *g, const char *why)
{
    fprintf(stderr, "recomp: cup groups: %s: the knockout draw failed (%s), the cup ends after its groups\n",
            format_file(cup), why);
    wr32(g_mem, cup + CUP_STAGE_COUNT, (uint32_t)g->groups);
}

static void draw_knockout(uint32_t cup, const SeasonCupGroups *g)
{
    int ties0 = 0, rounds = 0, ko = g->ko_clubs;
    /* The clubs in order: the group winners, the byes, the other places that go through, the best of the next place
     * by score, then the rest by place and score. */
    uint32_t order[MAX_CLUBS];
    int group_of[MAX_CLUBS], n = 0;
    struct { uint32_t club; int group; double score; int place; } next[MAX_CLUBS];
    int nn = 0;
    for (int x = 0; x < g->groups; x++) {
        uint32_t st = stage_at(cup, x), row = readable(st) ? row_at(st, 0) : 0;
        if (rt_unwinding) return;
        if (!row) {
            ko_failed(cup, g, "no group table");
            return;
        }
        order[n] = rd32(g_mem, row), group_of[n++] = x;
    }
    /* The byes: the clubs of the team list that the group draw left out (it read their European mark, is_bye, on
     * its day; the mark is not read again here, so a club is never both in a group and a bye). */
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST);
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    for (int i = 0; g->byes && readable(list) && i < count && i < 512 && n < MAX_CLUBS; i++) {
        uint32_t club = rd32(g_mem, list + 6u * (uint32_t)i);
        if (readable(club) && !in_groups(cup, g, club)) order[n] = club, group_of[n++] = -1;
        if (rt_unwinding) return;
    }
    for (int place = 1; place < 10; place++)
        for (int x = 0; x < g->groups; x++) {
            uint32_t st = stage_at(cup, x);
            int clubs = (int16_t)rd16(g_mem, st + 0x3e);
            if (place >= clubs) continue;
            uint32_t row = row_at(st, place);
            if (rt_unwinding) return;
            if (!row) {
                ko_failed(cup, g, "no group table");
                return;
            }
            if (place < g->per_group) order[n] = rd32(g_mem, row), group_of[n++] = x;
            else if (nn < MAX_CLUBS)
                next[nn].club = rd32(g_mem, row), next[nn].group = x, next[nn].place = place,
                next[nn++].score = row_score(st, row, clubs, g->meetings);
        }
    for (int i = 1; i < nn; i++)                        /* by place, then score */
        for (int k = i; k > 0 && (next[k].place < next[k - 1].place ||
                                  (next[k].place == next[k - 1].place && next[k].score > next[k - 1].score)); k--) {
            __typeof__(next[0]) t = next[k];
            next[k] = next[k - 1], next[k - 1] = t;
        }
    for (int i = 0; i < nn && n < MAX_CLUBS; i++) order[n] = next[i].club, group_of[n++] = next[i].group;
    if (n < ko) {
        while (ko > n) ko /= 2;
        fprintf(stderr, "recomp: cup groups: %s: %d clubs for a knockout of %d, a knockout of %d plays\n",
                format_file(cup), n, g->ko_clubs, ko);
    }
    if (ko < 2 || !season_new_cup_layout(ko, &ties0, &rounds) || rounds > 5 || 2 * ties0 != ko) {
        ko_failed(cup, g, "no knockout size");
        return;
    }
    /* Seeded clubs (the first half) against the others, not of the same group when a draw is found. */
    int half = ko / 2, pair[64];
    for (int relax = 0; relax < 2; relax++) {
        int ok = 1, used[64] = {0};
        for (int i = 0; i < half && ok; i++) {
            int cand[64], c = 0;
            for (int j = 0; j < half; j++)
                if (!used[j] && (relax || group_of[i] < 0 || group_of[i] != group_of[half + j])) cand[c++] = j;
            if (!c) {
                ok = 0;
                break;
            }
            pair[i] = cand[random_below(c)];
            if (rt_unwinding) return;
            used[pair[i]] = 1;
        }
        if (ok) break;
        if (relax) {
            ko_failed(cup, g, "no pairing");
            return;
        }
    }
    uint32_t ca[2] = {(uint32_t)ko, 4};
    uint32_t clubs = call_cdecl(F_CALLOC, 2, ca);
    if (rt_unwinding) return;
    if (!readable(clubs)) {
        ko_failed(cup, g, "no memory");
        return;
    }
    for (int i = 0; i < half; i++) {
        wr32(g_mem, clubs + 8u * (uint32_t)i, order[i]);
        wr32(g_mem, clubs + 8u * (uint32_t)i + 4, order[half + pair[i]]);
    }
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cup + CUP_YEAR);
    uint32_t table = game_malloc((uint32_t)rounds * ROUND_SIZE);
    if (rt_unwinding) return;
    if (!table) {
        game_free(clubs);
        if (!rt_unwinding) ko_failed(cup, g, "no memory");
        return;
    }
    build_ko_table(table, g, ko, rounds, year);
    uint32_t sz[1] = {0xb2};
    uint32_t obj = rt_unwinding ? 0 : call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding) return;
    if (!readable(obj)) {
        game_free(clubs);
        if (!rt_unwinding) game_free(table);
        if (!rt_unwinding) ko_failed(cup, g, "no memory");
        return;
    }
    /* The arguments of season_uefa_cup.c (as EngAutoCup::create_final_stage 0x56bce0): parent, clubs, list, rounds,
     * record, round table, year, stage, 2, 0, 0x14, 1, 0, 0, 0. */
    uint32_t args[15] = {cup, (uint32_t)ko, clubs, (uint32_t)rounds, rd32(g_mem, cup + 4), table, year,
                         (uint32_t)g->groups, 2, 0, 0x14, 1, 0, 0, 0};
    call_this(F_CUP_STAGE, obj, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, cup + CUP_STAGES) + 4u * (uint32_t)g->groups, obj);
    game_free(clubs);
    game_free(table);
    uint32_t r0[1] = {0};
    if (!rt_unwinding) call_this(F_SET_UP_ROUND, obj, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, cup + CUP_STAGE_NOW, (uint32_t)g->groups);
    fprintf(stderr, "recomp: cup groups: %s %u/%02u: knockout of %d clubs drawn\n", format_file(cup), year,
            (year + 1) % 100, ko);
}

int season_cup_groups_advance(uint32_t cup)
{
    const SeasonCupGroups *g = layout(cup);
    if (!g) return 0;
    int32_t now = (int32_t)rd32(g_mem, cup + CUP_STAGE_NOW);
    Cpu saved = cpu;
    if (now == -1) draw_groups(cup, g);
    else if (now == g->groups - 1) draw_knockout(cup, g);
    if (!rt_unwinding) cpu = saved;
    return 1;
}

uint32_t season_cup_groups_ko(uint32_t cup)
{
    const SeasonCupGroups *g = layout(cup);
    if (!g || (int32_t)rd32(g_mem, cup + CUP_STAGE_NOW) != g->groups) return 0;
    uint32_t ko = stage_at(cup, g->groups);
    return readable(ko) ? ko : 0;
}

/* Before Cup::load_from_disk of a cup of a group format: its stage array (the constructors make none before the load,
 * for example ScoLgeCup 0x7efa4c, EngAutoCup 0x56aa9e). The season of the object or the one before (a load from
 * January to June). */
void season_cup_groups_load(uint32_t cup)
{
    if (!season_game() || !readable(cup) || readable(rd32(g_mem, cup + CUP_STAGES))) return;
    int year = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    if (!format_at(cup, season_changing_rules() ? year : season_start_year()) &&
        !format_at(cup, season_changing_rules() ? year - 1 : season_start_year()))
        return;
    Cpu saved = cpu;
    big_stage_array(cup);
    if (!rt_unwinding) cpu = saved;
}

/* ---- Wraps ---- */

void o_007efcb0(void);
void o_0056ade0(void);
void o_00519890(void);
void o_0056bab0(void);
void o_0056bc50(void);
void o_0056c010(void);
void o_00595160(void);
void o_0056b2b0(void);

/* ScoLgeCup and EngAutoCup create_round_table, thiscall (char stage, short *rounds, short *first id, int *out) ret
 * 0x10, vtable +0x3c (0x96f568, 0x969894). The other classes of a format call season_cup_groups_table from the wraps
 * of season_kind_cups.c. */
RT_WRAP(007efcb0, groups_sco_lge_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_007efcb0();
    if (!rt_unwinding) season_cup_groups_table(self, stage, count);
}

RT_WRAP(0056ade0, groups_eng_auto_cup)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0056ade0();
    if (!rt_unwinding) season_cup_groups_table(self, stage, count);
}

/* After Cup::map_fixture_tree 0x518790 (thiscall () ret 0, vtable +0x94) of self: one wrap for slices 21c and 23,
 * in season_uefa_qual.c, calls this. */
void season_cup_groups_mapped(uint32_t self)
{
    if (rt_unwinding || !readable(self)) return;
    uint32_t eax = cpu.eax;
    mark_later_slots(self);
    cpu.eax = eax;
}

/* Cup::update_staff_records_for_status, slot 17 (thiscall, 6 arguments, ret 0x18): as for a cup with no stages. */
RT_WRAP(00519890, groups_staff_records)
{
    uint32_t self = cpu.ecx;
    if (!layout(self)) {
        o_00519890();
        return;
    }
    uint32_t count = rd32(g_mem, self + CUP_STAGE_COUNT);
    wr32(g_mem, self + CUP_STAGE_COUNT, 0);
    o_00519890();
    wr32(g_mem, self + CUP_STAGE_COUNT, count);
}

/* EngAutoCup::create_section_stage, thiscall () ret 0: not in a group season. */
RT_WRAP(0056bab0, groups_auto_section)
{
    if (layout(cpu.ecx)) {
        rt_ret_cdecl(0);
        return;
    }
    o_0056bab0();
}

/* EngAutoCup::advance_stage, slot 10, thiscall () ret 0. */
RT_WRAP(0056bc50, groups_auto_advance)
{
    if (season_cup_groups_advance(cpu.ecx)) {
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    o_0056bc50();
}

/* EngAutoCup::rank_teams, slot 23, thiscall () ret 0: not in a group season (it reads stage 0's team list). */
RT_WRAP(0056c010, groups_auto_rank)
{
    if (layout(cpu.ecx)) {
        rt_ret_cdecl(0);
        return;
    }
    o_0056c010();
}

/* 0x595160, slot 12 of EngAutoCup (and of other classes): a jump to slot 12 of stage 1 (0x595168); in a group season
 * the knockout stage's. */
RT_WRAP(00595160, groups_stage_honours)
{
    uint32_t self = cpu.ecx, ko = season_cup_groups_ko(self);
    if (!ko) {
        o_00595160();
        return;
    }
    cpu.ecx = ko;
    rt_jump(rd32(g_mem, rd32(g_mem, ko) + 0x30));
}

/* EngAutoCup::build_fate_news_text 0x56b2b0, slot +0x48, thiscall (10 arguments) ret 0x28. For a full text (argument
 * 9, tested at 0x56b2b5) it calls the tie writer 0x4b0b80 with the fixture of argument 5 and no test of it. Comp's
 * version 0x48c6d0 calls 0x4b0b80 only for a fixture (test at 0x48c6d7). In the original every EFL Trophy fate item
 * comes from a tie; an item of a group stage here has none, and NewsItem::write_text 0x77bc80 (call at 0x77defe)
 * then made 0x4b0b80 read a fixture at 0 (box match_stats.CPP 234 from 0x7295dc, gate 2022, 25 December 2022). Only
 * a full text with no fixture goes to 0x48c6d0; a call with a fixture runs the original as before (the base game
 * never has a zero fixture there, because the original boxes on it). */
RT_WRAP(0056b2b0, groups_auto_fate_news)
{
    /* The cause (O260 A19): in a group season the stages 0 to groups - 1 are the League_Stage groups of
     * season_cup_groups.c (draw_groups gives each its number), and their fate items have no tie. The classes of the
     * original with groups split their fate text by stage: ItaCup's 0x63f130 tests the stage (argument 3, cmp al, 7
     * at 0x63f134: its groups are stages 0 to 6) and writes a tie only for the knockout. So an EFL Trophy item of a
     * group stage takes Comp's text 0x48c6d0 (the version of ScoLgeCup, TurCup and Cup_Stage), and the knockout
     * stage (number groups) keeps EngAutoCup's own text with its tie. */
    /* RECOMP_TEST_GROUP_FATE (tests only): "original" turns the routing and the safety net off; "log" logs every call;
     * "full" and "full-original" also ask for the full text (argument 9, here the headline text of argument 10) of
     * each group stage item, as the news screen does when it opens one (the retest of the old box, O277). */
    static int original = -1, logged, every, full;
    if (original < 0) {
        const char *v = getenv("RECOMP_TEST_GROUP_FATE");
        original = v && (!strcmp(v, "original") || !strcmp(v, "full-original"));
        every = v && !strcmp(v, "log");
        full = v && !strncmp(v, "full", 4);
    }
    if (every)
        fprintf(stderr, "recomp: cup groups: fate text call: stages %d of %d, stage %d, fate %d, fixture %08x, "
                "full %08x\n",
                (int32_t)rd32(g_mem, cpu.ecx + CUP_STAGE_COUNT), layout(cpu.ecx) ? layout(cpu.ecx)->groups : -1,
                (int8_t)g_mem[cpu.esp + 0xcu], (int8_t)g_mem[cpu.esp + 8u], rd32(g_mem, cpu.esp + 0x14u),
                rd32(g_mem, cpu.esp + 0x24u));
    if (full && layout(cpu.ecx) && (int8_t)g_mem[cpu.esp + 0xcu] < layout(cpu.ecx)->groups &&
        !rd32(g_mem, cpu.esp + 0x24u)) {
        wr32(g_mem, cpu.esp + 0x24u, rd32(g_mem, cpu.esp + 0x28u));
        fprintf(stderr, "recomp: cup groups: test: full text of a group stage %d item (fate %d, fixture %08x), %s "
                "path\n",
                (int8_t)g_mem[cpu.esp + 0xcu], (int8_t)g_mem[cpu.esp + 8u], rd32(g_mem, cpu.esp + 0x14u),
                original ? "old" : "new");
    }
    if (original) {
        o_0056b2b0();
        return;
    }
    const SeasonCupGroups *g = layout(cpu.ecx);
    int stage = (int8_t)g_mem[cpu.esp + 0xcu];
    if (g && stage >= 0 && stage < g->groups) {
        if (!logged++)
            fprintf(stderr, "recomp: cup groups: EFL Trophy fate text of group stage %d (fate %d, fixture %s) by "
                    "Comp's text\n", stage, (int8_t)g_mem[cpu.esp + 8u], rd32(g_mem, cpu.esp + 0x14u) ? "set" : "none");
        rt_jump(0x0048c6d0u);
        return;
    }
    /* Safety net: a full text with no fixture (gate 2022, 25 December 2022, before the cause was found). */
    if (rd32(g_mem, cpu.esp + 0x24u) && !rd32(g_mem, cpu.esp + 0x14u)) {
        fprintf(stderr, "recomp: cup groups: EFL Trophy fate text of stage %d with no fixture\n", stage);
        rt_jump(0x0048c6d0u);
        return;
    }
    o_0056b2b0();
}
