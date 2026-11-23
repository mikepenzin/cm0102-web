/* The Argentine Primera at its real size (season_sizes_arg.h). */
#include "win.h"
#include "season_sizes_arg.h"
#include "season_sizes_arg_second.h"
#include "season_sizes.h"
#include "season.h"
#include "season_league.h"
#include "season_change.h"
#include "season_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_PRM 0x9cf654u           /* the record id of the Primera (ArgPrm::create_table 0x4055ed) */
#define KEY_SECOND 0x9cf658u        /* the record id of the second division (ArgPrm::start_new_season 0x40576b) */
#define COMP_OBJECTS 0xadadfcu      /* g_comp_object_table: the object of each record id (0x405765..0x405775) */
#define CLUBS 0xae23b4u
#define CLUB_SIZE 0x245u
#define CLUB_NAME 4u
#define CLUB_DIVISION 0x57u
#define CLUB_REPUTATION 0x80u
#define COMP_RECORD 4u
#define STAGES 0xcu                 /* the stage array: [0] the Clausura, [1] the aggregate table (0x405aac, 0x405b9b) */
#define AVERAGES 0x18u              /* the records of the relegation averages, 0x13 bytes each (0x405f16) */
#define AVERAGE_COUNT 0x34u         /* s16: their count (0x405f02) */
#define COMP_MEETINGS 0x3cu
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define STAGE_INDEX 0x44u           /* s8: 0xff for the league itself, the stage index for a stage (0x685dc4) */
#define COMP_MATCHES 0xa7u
#define COMP_DATES 0xa9u            /* s16: the dates of the template (D) */
#define COMP_TABLE 0xb1u
#define COMP_COMPLETE 0xddu         /* 1 once check_complete 0x689ad0 found the table complete (0x689c06) */
#define COMP_FIXED 0xeau            /* the fixed fixture list (League::create_fixtures 0x6837fe) */
#define ROW_SIZE 0x3bu
#define DATE_SIZE 0x41u
#define AVG_SIZE 0x13u              /* +0 club id, +4 the average, +8 s16 the season, then three seasons of points and
                                     * matches (0x406651..0x4066fb) */
#define F_SCHEDULE 0x00404570u      /* ArgPrm::create_schedule, thiscall (stage, &count, &code, &flag) ret 0x10 */
#define F_NEW 0x00944cf1u           /* operator new, cdecl (size): the fixed list is new[] (count at -4, 0x683aa1) */
#define FIXED_SIZE 0x10u            /* an entry of the fixed list: +2 the edition, +8 home, +0xc away (0x683914) */
#define CLASS_CLUBS 20              /* ArgPrm::create_table 0x405552 */
#define MIN_CLUBS 2
#define MAX_CLUBS SEASON_ARG_MAX_CLUBS
#define SNAP_MAX (4 * MAX_CLUBS)

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    const char *v = getenv("RECOMP_SIZE_LOG");
    return v && *v && strcmp(v, "0");
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

static uint32_t prm_object(void)
{
    uint32_t objects = rd32(g_mem, COMP_OBJECTS);
    int32_t key = (int32_t)rd32(g_mem, KEY_PRM);
    return readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0;
}

static int is_prm(uint32_t obj) { return readable(obj) && !strcmp(season_change_file(obj), "arg_prm"); }

/* ---- The form of an edition ---- */

typedef struct {
    int season;             /* the rules row */
    int halves;             /* 1: Apertura and Clausura */
    int zones;              /* 2: two zones of n / 2; 0: one table */
    int extra;              /* derby or inter-zone rounds */
    int meet;
    int rounds;             /* the rounds of a half (or of the one table) */
    int matches;            /* the matches of a club in a half (or in the one table) */
    int split;              /* 1: halves of one table (2027): the Apertura plays its first half of rounds, the
                             * Clausura the second */
} Form;

static int form_of(uint32_t obj, int n, Form *f)
{
    memset(f, 0, sizeof *f);
    if (!season_sizes_on() || !readable(obj) || n < MIN_CLUBS || n > MAX_CLUBS) return 0;
    if (season_sizes_kept(obj)) return 0;     /* no room for the dates this edition: the class's form */
    f->season = season_sizes_row_season(obj, "arg_prm");
    const SeasonLeagueRule *r = f->season >= 0 ? season_league_row("arg_prm", f->season) : NULL;
    if (!r) return 0;
    f->halves = r->halves != 0;
    f->meet = r->meetings >= 1 && r->meetings <= 4 ? r->meetings : 1;
    f->extra = r->extra_rounds > 0 ? r->extra_rounds : 0;
    /* Two zones of one meeting with two inter-zone rounds (the rival in the round where both rest, and a round of
     * drawn opponents): an odd zone of 5 or more. Any other zone form plays as one table. */
    int z = n / 2;
    f->zones = r->zones == 2 && n % 2 == 0 && z % 2 == 1 && z >= 5 && f->meet == 1 && f->extra == 2 ? 2 : 0;
    if (f->zones) {
        f->rounds = z + 1;
        f->matches = z - 1 + 2;
    } else if (f->halves && f->extra == 1 && f->meet == 1 && n % 2 == 0) {
        /* 2027 on (gap row 49): one round-robin of n clubs and the derby round (n rounds), the Apertura the first
         * n / 2 rounds and the Clausura the rest, each a table of its own. */
        f->split = 1;
        f->rounds = n / 2;
        f->matches = n / 2;
    } else {
        int even = n + (n & 1);
        /* One derby round of one table: an even count (every club has a partner). */
        if (n & 1 || f->extra > 1 || f->meet != 1) f->extra = 0;
        f->rounds = f->meet * (even - 1) + f->extra;
        f->matches = f->meet * (n - 1) + f->extra;
    }
    return 1;
}

int season_arg_matches(uint32_t obj, int n)
{
    Form f;
    if (!form_of(obj, n, &f)) return (n - 1) * (int)g_mem[obj + COMP_MEETINGS];
    return f.matches;
}

/* ---- The date templates ---- */

static void put_template(uint32_t count_at, uint32_t old, uint32_t tmpl, int rounds)
{
    season_sizes_game_free(old);
    wr16(g_mem, count_at, (uint16_t)rounds);
    cpu.eax = tmpl;
}

static int in_schedule;

/* The records of a template with one round on each day: the class can date two rounds on one day (the Apertura of 2025
 * makes 21 and 28 September twice: Date 0x54a2d0 snaps each date to its weekday), which the plan refuses. The block
 * keeps its size; returns the records left. */
static int one_a_day(uint32_t block, int have, int base)
{
    int days[SEASON_TEMPLATE_MAX], n = 0;
    for (int i = 0; i < have && i < SEASON_TEMPLATE_MAX; i++) {
        uint32_t r = block + (uint32_t)i * DATE_SIZE;
        int d = season_template_day(base, (int16_t)rd16(g_mem, r + 2), (int16_t)rd16(g_mem, r)), twice = 0;
        for (int j = 0; j < n; j++) twice |= days[j] == d;
        if (twice) continue;
        if (n != i) memmove(g_mem + block + (uint32_t)n * DATE_SIZE, g_mem + r, DATE_SIZE);
        days[n++] = d;
    }
    if (n != have && logging())
        fprintf(stderr, "recomp: league sizes: arg_prm %d: %d class dates on a day of another, left out\n", base, have - n);
    return n;
}

/* No room for the dates (D4): the Primera keeps the class's count and form this edition, as a grown class does
 * (season_sizes_schedule), and its Clausura and fixtures stay the class's. */
static void no_room(uint32_t self)
{
    season_sizes_keep(self, 1);
    fprintf(stderr, "recomp: league sizes: arg_prm %d: no room for the dates of its row; the class's 20 clubs in two "
                    "halves stay\n", (int16_t)rd16(g_mem, self + COMP_YEAR));
}

/* Before the Apertura's dates: 1 when the Clausura's dates (create_schedule(0), the same call that create_stages
 * 0x405a0b makes later) hold the rounds of a half, so both halves take the row or neither does (D4). The plan is
 * pure (season_template_plan), so the Clausura's own call then gives the same dates. */
static int clausura_room(uint32_t self, const Form *f, int base)
{
    uint32_t scratch = season_sizes_game_malloc(16);
    if (!scratch) return 0;
    uint32_t a[4] = {0, scratch, scratch + 4, scratch + 8};
    in_schedule = 1;
    uint32_t second = call_this(F_SCHEDULE, self, 4, a);
    in_schedule = 0;
    if (rt_unwinding) return 0;
    int have = (int16_t)rd16(g_mem, scratch);
    season_sizes_game_free(scratch);
    if (!readable(second) || have < 1 || have > SEASON_TEMPLATE_MAX) {
        season_sizes_game_free(second);
        return 0;
    }
    int left = one_a_day(second, have, base);
    uint32_t t = f->rounds == left ? 0 : season_sizes_build_template(self, "arg_prm", second, left, f->rounds,
                                                                    SEASON_TEMPLATE_SEQUENTIAL);
    int ok = f->rounds == left || t;
    if (t) season_sizes_game_free(t);
    season_sizes_game_free(second);
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_prm %d: Clausura room: %d class dates, %d on days of their own, %d "
                        "rounds: %s\n", base, have, left, f->rounds, ok ? "room" : "no room");
    return ok;
}

void season_arg_schedule(uint32_t self, int stage, uint32_t count_at)
{
    if (in_schedule || (stage != -1 && stage != 0) || !readable(count_at)) return;
    if (stage == -1) season_sizes_keep(self, 0);    /* a new edition (set_up_rules, 0x40570c) */
    int n = season_sizes_count(self);
    Form f;
    if (n < MIN_CLUBS || !form_of(self, n, &f)) return;
    uint32_t old = cpu.eax;
    int have = (int16_t)rd16(g_mem, count_at);
    if (!readable(old) || have < 1) return;
    if (stage == 0 && !f.halves) {
        /* One table: the Clausura stage has no dates, so League_Stage::League_Stage makes no fixtures for it
         * (0x68b26d, 0x68b3b5: a template of one date, the day it is made). */
        season_sizes_game_free(old);
        wr16(g_mem, count_at, 0);
        cpu.eax = 0;
        if (logging())
            fprintf(stderr, "recomp: league sizes: arg_prm %d: one table of %d clubs, no Clausura\n",
                    (int16_t)rd16(g_mem, self + COMP_YEAR), n);
        return;
    }
    int base = (int16_t)rd16(g_mem, self + COMP_YEAR);
    if (f.halves && stage == -1 && f.rounds != have && !clausura_room(self, &f, base)) {
        no_room(self);
        return;
    }
    if (f.halves) {
        /* A half: its own 19 dates, fewer for the zones (D5). */
        if (f.rounds == have) return;
        int left = one_a_day(old, have, base);
        uint32_t t = season_sizes_build_template(self, "arg_prm", old, left, f.rounds, SEASON_TEMPLATE_SEQUENTIAL);
        if (rt_unwinding) return;
        if (t) put_template(count_at, old, t, f.rounds);
        else if (stage == -1) no_room(self);
        else {
            /* The Apertura has its dates; a Clausura with none would leave the season short. The class's own dates
             * stay and the stage plays the class's fixtures (create_stages then counts its rows as the class). */
            wr16(g_mem, count_at, (uint16_t)left);
            fprintf(stderr, "recomp: league sizes: arg_prm %d: no room for the Clausura's %d rounds\n", base, f.rounds);
        }
        return;
    }
    /* One table: the dates of both halves (the Clausura's of create_schedule(0)), down to the rounds of the row. */
    uint32_t scratch = season_sizes_game_malloc(16);
    if (!scratch) return;
    uint32_t a[4] = {0, scratch, scratch + 4, scratch + 8};
    in_schedule = 1;
    uint32_t second = call_this(F_SCHEDULE, self, 4, a);
    in_schedule = 0;
    if (rt_unwinding) return;
    int have2 = (int16_t)rd16(g_mem, scratch);
    season_sizes_game_free(scratch);
    if (!readable(second) || have2 < 1 || have + have2 > SEASON_TEMPLATE_MAX) {
        season_sizes_game_free(second);
        no_room(self);
        return;
    }
    uint32_t both = season_sizes_game_malloc((uint32_t)(have + have2) * DATE_SIZE);
    if (!both) {
        no_room(self);
        return;
    }
    memcpy(g_mem + both, g_mem + old, (size_t)have * DATE_SIZE);
    memcpy(g_mem + both + (uint32_t)have * DATE_SIZE, g_mem + second, (size_t)have2 * DATE_SIZE);
    season_sizes_game_free(second);
    int left = one_a_day(both, have + have2, base);
    uint32_t t = season_sizes_build_template(self, "arg_prm", both, left, f.rounds, SEASON_TEMPLATE_SEQUENTIAL);
    season_sizes_game_free(both);
    if (rt_unwinding) return;
    if (t) put_template(count_at, old, t, f.rounds);
    else no_room(self);
}

/* ---- The plan of an edition ---- */

static int same_name(uint32_t club, const char *name)
{
    return readable(club) && !strncmp(win_str(club + CLUB_NAME), name, 52);
}

/* The newest row up to season that the table holds for a kind (zones: 'Z'), or -1. */
static int list_season(int season, int kind)
{
    int best = -1;
    if (kind == 'Z') {
        for (int i = 0; i < season_arg_prm_zone_count; i++)
            if (season_arg_prm_zones[i].season <= season && season_arg_prm_zones[i].season > best) best = season_arg_prm_zones[i].season;
    } else {
        for (int i = 0; i < season_arg_pair_count; i++)
            if (season_arg_pairs[i].kind == kind && season_arg_pairs[i].season <= season &&
                season_arg_pairs[i].season > best)
                best = season_arg_pairs[i].season;
    }
    return best;
}

/* 1 when every drawn pair of a list is one of its rival pairs (2016: the rivals meet again in fecha 12), read from the
 * list alone, not from the clubs that a game holds. */
static int list_twice(int season)
{
    int draws = 0;
    for (int k = 0; k < season_arg_pair_count; k++) {
        const SeasonArgPair *d = &season_arg_pairs[k];
        if (d->kind != SEASON_ARG_DRAW || d->season != season) continue;
        int found = 0;
        for (int j = 0; j < season_arg_pair_count && !found; j++) {
            const SeasonArgPair *r = &season_arg_pairs[j];
            found = r->kind == SEASON_ARG_RIVAL && r->season == season &&
                    ((!strcmp(r->home, d->home) && !strcmp(r->away, d->away)) ||
                     (!strcmp(r->home, d->away) && !strcmp(r->away, d->home)));
        }
        if (!found) return 0;
        draws++;
    }
    return draws > 0;
}

static int index_of(const uint32_t *club, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (same_name(club[i], name)) return i;
    return -1;
}

/* The plan of the last Apertura (or one table) made: the Clausura plays it with the venues reversed. */
static struct {
    uint32_t obj;
    int year, n, rounds;
    uint32_t home[SEASON_ARG_MAX_ROUNDS][MAX_CLUBS / 2], away[SEASON_ARG_MAX_ROUNDS][MAX_CLUBS / 2];    /* club ids */
} last;

/* The zones of the last plan made (season_arg_zone_lists): the zone tables of the season take them. */
static struct { uint32_t obj; int year, z; uint32_t a[MAX_CLUBS / 2], b[MAX_CLUBS / 2]; } zplan;
static uint32_t zplan_obj;
static int zplan_year;

int season_arg_zone_lists(uint32_t obj, uint32_t *a, uint32_t *b)
{
    if (!readable(obj) || zplan.obj != obj || zplan.year != (int16_t)rd16(g_mem, obj + COMP_YEAR)) return 0;
    memcpy(a, zplan.a, sizeof(uint32_t) * (size_t)zplan.z);
    memcpy(b, zplan.b, sizeof(uint32_t) * (size_t)zplan.z);
    return zplan.z;
}

int season_arg_stage_kind(uint32_t obj, int *season)
{
    Form f;
    int n = readable(obj) ? (int16_t)rd16(g_mem, obj + COMP_CLUBS) : 0;
    if (!is_prm(obj) || !form_of(obj, n, &f)) return SEASON_ARG_KO_NONE;
    if (season) *season = f.season;
    if (f.zones) return f.halves ? SEASON_ARG_KO_ZONES : SEASON_ARG_KO_FINAL;
    if (f.split) return SEASON_ARG_KO_TABLE;
    if (f.season >= SEASON_ARG_PROMO_FIRST && f.season <= SEASON_ARG_PROMO_LAST) return SEASON_ARG_KO_PROMO;
    return SEASON_ARG_KO_NONE;
}

static int zones_plan(const Form *f, const uint32_t *club, int n, int round[][SEASON_ARG_MAX_CLUBS / 2][2],
                      int *ids_at)
{
    int z = n / 2, zone[MAX_CLUBS], s = list_season(f->season, 'Z'), na = 0, nb = 0;
    for (int i = 0; i < n; i++) {
        zone[i] = -1;
        for (int k = 0; s >= 0 && k < season_arg_prm_zone_count; k++)
            if (season_arg_prm_zones[k].season == s && same_name(club[i], season_arg_prm_zones[k].club))
                zone[i] = season_arg_prm_zones[k].zone == 'A' ? 0 : 1;
        if (zone[i] == 0 && na >= z) zone[i] = 1;
        if (zone[i] == 1 && nb >= z) zone[i] = na < z ? 0 : -1;
        na += zone[i] == 0;
        nb += zone[i] == 1;
    }
    /* A club of no list (a promoted club, a club of another database) takes a place of the zone that has one: the
     * place of the club it replaces. */
    for (int i = 0; i < n; i++)
        if (zone[i] < 0) {
            zone[i] = na < z ? 0 : 1;
            na += zone[i] == 0;
            nb += zone[i] == 1;
        }
    int a[MAX_CLUBS / 2], b[MAX_CLUBS / 2], ia = 0, ib = 0;
    for (int i = 0; i < n; i++) {
        if (zone[i] == 0) a[ia++] = i; else b[ib++] = i;
    }
    if (ia != z || ib != z) return 0;
    /* The rival of each club of zone A in the round of its real fecha, the drawn opponents in their fecha. */
    int rs = list_season(f->season, SEASON_ARG_RIVAL), ds = list_season(f->season, SEASON_ARG_DRAW), draw_at = -1;
    int rival_b[MAX_CLUBS / 2], fecha[MAX_CLUBS / 2], draw_b[MAX_CLUBS / 2];
    char b_used[MAX_CLUBS / 2] = {0}, d_used[MAX_CLUBS / 2] = {0};
    for (int i = 0; i < z; i++) rival_b[i] = draw_b[i] = -1, fecha[i] = 1000 + i;
    for (int k = 0; k < season_arg_pair_count; k++) {
        const SeasonArgPair *p = &season_arg_pairs[k];
        if (p->kind == SEASON_ARG_DRAW && p->season == ds && draw_at < 0) draw_at = p->round - 1;
        if ((p->kind != SEASON_ARG_RIVAL || p->season != rs) && (p->kind != SEASON_ARG_DRAW || p->season != ds)) continue;
        int h = index_of(club, n, p->home), w = index_of(club, n, p->away);
        if (h < 0 || w < 0 || zone[h] == zone[w]) continue;
        int x = zone[h] == 0 ? h : w, y = zone[h] == 0 ? w : h, ax = 0, by = 0;
        while (a[ax] != x) ax++;
        while (b[by] != y) by++;
        if (p->kind == SEASON_ARG_RIVAL && rival_b[ax] < 0 && !b_used[by]) {
            rival_b[ax] = by;
            b_used[by] = 1;
            fecha[ax] = p->round;
        } else if (p->kind == SEASON_ARG_DRAW && draw_b[ax] < 0 && !d_used[by]) {
            draw_b[ax] = by;
            d_used[by] = 1;
        }
    }
    for (int i = 0, j = 0; i < z; i++)
        if (rival_b[i] < 0) {
            while (b_used[j]) j++;
            rival_b[i] = j;
            b_used[j] = 1;
        }
    /* Zone A in the order of the rival fechas. */
    int order[MAX_CLUBS / 2];
    for (int i = 0; i < z; i++) {
        int j = i;
        for (; j > 0 && fecha[order[j - 1]] > fecha[i]; j--) order[j] = order[j - 1];
        order[j] = i;
    }
    if (draw_at < 0 || draw_at > z) draw_at = z / 2;
    /* Drawn opponents for the clubs of no pair: a club of zone B not drawn yet and not the rival. When the last
     * clubs are left with their rivals only, every club of the zone takes the club after its rival instead (a
     * matching apart from the rivals). */
    int ok = 1, twice = rs >= 0 && ds == rs && list_twice(ds);
    if (twice)      /* the list meets each rival twice (2016, fecha 12): every club does */
        for (int i = 0; i < z; i++) draw_b[i] = rival_b[i];
    for (int i = 0; i < z && ok && !twice; i++) {
        if (draw_b[i] >= 0) continue;
        for (int j = 0; j < z && draw_b[i] < 0; j++)
            if (!d_used[j] && j != rival_b[i]) draw_b[i] = j, d_used[j] = 1;
        ok = draw_b[i] >= 0;
    }
    for (int i = 0; i < z && ok && !twice; i++) ok = draw_b[i] != rival_b[i];
    if (!ok)
        for (int i = 0; i < z; i++) draw_b[i] = (rival_b[i] + 1) % z;
    int rival[MAX_CLUBS / 2], draw[MAX_CLUBS / 2];
    for (int c = 0; c < z; c++) {
        rival[c] = rival_b[order[c]];
        draw[c] = draw_b[order[c]];
    }
    int rounds = season_arg_zone_plan(z, rival, draw, draw_at, round);
    for (int c = 0; c < z; c++) {
        ids_at[c] = a[order[c]];
        ids_at[z + c] = b[c];
    }
    zplan.obj = zplan_obj;
    zplan.year = zplan_year;
    zplan.z = z;
    for (int c = 0; c < z; c++) {
        zplan.a[c] = club[a[c]];
        zplan.b[c] = club[b[c]];
    }
    if (logging()) {
        /* The test (tests/seasons/league_size_run.py) reads the zones by club id, and the pairs that the list gave. */
        int from_list = 0, drawn_list = 0, in_list = 0;
        for (int i = 0; i < z; i++) from_list += fecha[i] < 1000;
        for (int i = 0; i < n; i++)
            for (int k = 0; s >= 0 && k < season_arg_prm_zone_count; k++)
                in_list += season_arg_prm_zones[k].season == s && same_name(club[i], season_arg_prm_zones[k].club);
        for (int k = 0; k < season_arg_pair_count; k++) {
            const SeasonArgPair *p = &season_arg_pairs[k];
            int h = p->kind == SEASON_ARG_DRAW && p->season == ds ? index_of(club, n, p->home) : -1;
            int w = h >= 0 ? index_of(club, n, p->away) : -1;
            if (w >= 0) {
                int x = zone[h] == 0 ? h : w, y = zone[h] == 0 ? w : h, ax = 0;
                while (ax < z && a[ax] != x) ax++;
                drawn_list += ax < z && draw_b[ax] >= 0 && b[draw_b[ax]] == y;
            }
        }
        fprintf(stderr, "recomp: league sizes: arg_prm zones %d: list %d, %d clubs of the list, rivals %d of the list, "
                        "drawn %d of the list, drawn round %d; A", f->season, s, in_list, from_list, drawn_list,
                draw_at + 1);
        for (int c = 0; c < z; c++) fprintf(stderr, " %u", rd32(g_mem, club[a[order[c]]]));
        fprintf(stderr, "; B");
        for (int c = 0; c < z; c++) fprintf(stderr, " %u", rd32(g_mem, club[b[c]]));
        fprintf(stderr, "; rivals");
        for (int c = 0; c < z; c++)
            fprintf(stderr, " %.30s~%.30s,", win_str(club[a[order[c]]] + CLUB_NAME), win_str(club[b[rival[c]]] + CLUB_NAME));
        fprintf(stderr, "; not in the division:");
        for (int k = 0; s >= 0 && k < season_arg_prm_zone_count; k++)
            if (season_arg_prm_zones[k].season == s && index_of(club, n, season_arg_prm_zones[k].club) < 0)
                fprintf(stderr, " %s,", season_arg_prm_zones[k].club);
        fprintf(stderr, "\n");
    }
    return rounds;
}

static int derby_plan(const Form *f, const uint32_t *club, int n, int year, int round[][SEASON_ARG_MAX_CLUBS / 2][2],
                      int *ids_at)
{
    int s = list_season(f->season, SEASON_ARG_DERBY), derby[MAX_CLUBS], derby_at = -1, perm[MAX_CLUBS];
    int listed[MAX_CLUBS], rep[MAX_CLUBS], kind = SEASON_ARG_DERBY;
    /* 2027 on: each club meets its clásico again (LPF format of 29 October 2026). The sources give no pairs and no
     * fecha: the pairs are the last known ones (the rivals of 2026), the round is the middle of the Clausura. */
    if (f->split) {
        int r = list_season(f->season, SEASON_ARG_RIVAL);
        if (r > s) s = r, kind = SEASON_ARG_RIVAL;
        derby_at = n / 2 + n / 4;
    }
    for (int i = 0; i < n; i++) {
        listed[i] = -1;
        rep[i] = (int16_t)rd16(g_mem, club[i] + CLUB_REPUTATION);
    }
    for (int k = 0; k < season_arg_pair_count; k++) {
        const SeasonArgPair *p = &season_arg_pairs[k];
        if (p->kind != kind || p->season != s) continue;
        if (derby_at < 0) derby_at = p->round - 1;
        int h = index_of(club, n, p->home), w = index_of(club, n, p->away);
        if (h >= 0 && w >= 0 && h != w && listed[h] < 0 && listed[w] < 0) {
            listed[h] = w;
            listed[w] = h;
        } else if (h >= 0 && w < 0 && listed[h] == -1) {
            listed[h] = -2;
        } else if (w >= 0 && h < 0 && listed[w] == -1) {
            listed[w] = -2;
        }
    }
    /* A club of no list takes the partner of the club it replaced; any left over meet by reputation. */
    int nr = 0, orphans = 0, replaced = season_arg_derby_pairs(n, listed, rep, derby);
    for (int i = 0; i < n; i++) nr += listed[i] < 0, orphans += listed[i] == -2;
    if (derby_at < 0 || derby_at > n - 1) derby_at = (n - 1) / 2;
    /* The order of the circle: a shuffle of the table order by the edition (no draw of the game's random numbers). */
    for (int i = 0; i < n; i++) perm[i] = i;
    uint32_t x = 0x9e3779b9u ^ (uint32_t)year;
    for (int i = n - 1; i > 0; i--) {
        x = x * 1664525u + 1013904223u;
        int j = (int)((x >> 8) % (uint32_t)(i + 1)), t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }
    int pd[MAX_CLUBS], inv[MAX_CLUBS];
    for (int i = 0; i < n; i++) inv[perm[i]] = i;
    for (int i = 0; i < n; i++) pd[i] = inv[derby[perm[i]]];
    for (int i = 0; i < n; i++) ids_at[i] = perm[i];
    if (logging()) {
        fprintf(stderr, "recomp: league sizes: arg_prm derby %d: list %d, %d pairs of the list, %d by replacement of %d, "
                        "derby round %d; pairs", f->season, s, (n - nr) / 2, replaced, orphans, derby_at + 1);
        for (int i = 0; i < n; i++)
            if (i < derby[i]) fprintf(stderr, " %u-%u", rd32(g_mem, club[i]), rd32(g_mem, club[derby[i]]));
        fprintf(stderr, "; not in the division:");
        for (int k = 0; k < season_arg_pair_count; k++) {
            const SeasonArgPair *p = &season_arg_pairs[k];
            if (p->kind != kind || p->season != s) continue;
            if (index_of(club, n, p->home) < 0) fprintf(stderr, " %s,", p->home);
            if (index_of(club, n, p->away) < 0) fprintf(stderr, " %s,", p->away);
        }
        fprintf(stderr, ";");
        for (int i = 0; i < n; i++)
            if (i < derby[i])
                fprintf(stderr, " %.30s~%.30s,", win_str(club[i] + CLUB_NAME), win_str(club[derby[i]] + CLUB_NAME));
        fprintf(stderr, "\n");
    }
    return season_arg_derby_plan(n, pd, derby_at, round);
}

static uint32_t building_stages;    /* the Primera whose create_stages runs (the Clausura stage gets its list) */

static uint32_t fixed_list_from(uint32_t league, int from, int rounds, int per_round, int reverse);

static uint32_t fixed_list(uint32_t league, int rounds, int per_round, int reverse)
{
    return fixed_list_from(league, 0, rounds, per_round, reverse);
}

/* The fixed list of rounds from .. from + rounds - 1 of the last plan. */
static uint32_t fixed_list_from(uint32_t league, int from, int rounds, int per_round, int reverse)
{
    int n = rounds * per_round;
    uint32_t a[1] = {4u + FIXED_SIZE * (uint32_t)n};
    uint32_t block = call_cdecl(F_NEW, 1, a);
    if (rt_unwinding || !readable(block)) return 0;
    memset(g_mem + block, 0, 4u + FIXED_SIZE * (uint32_t)n);
    wr32(g_mem, block, (uint32_t)n);
    uint32_t list = block + 4;
    for (int r = 0; r < rounds; r++)
        for (int k = 0; k < per_round; k++) {
            uint32_t e = list + FIXED_SIZE * (uint32_t)(r * per_round + k);
            wr16(g_mem, e + 2, rd16(g_mem, league + COMP_YEAR));
            wr32(g_mem, e + 8, reverse ? last.away[from + r][k] : last.home[from + r][k]);
            wr32(g_mem, e + 0xc, reverse ? last.home[from + r][k] : last.away[from + r][k]);
        }
    if (logging()) {
        fprintf(stderr, "recomp: league sizes: arg_prm fixtures %d %s:", (int16_t)rd16(g_mem, league + COMP_YEAR),
                reverse || from ? "clausura" : "apertura");
        for (int r = 0; r < rounds; r++) {
            fprintf(stderr, " |");
            for (int k = 0; k < per_round; k++)
                fprintf(stderr, " %u-%u", reverse ? last.away[from + r][k] : last.home[from + r][k],
                        reverse ? last.home[from + r][k] : last.away[from + r][k]);
        }
        fprintf(stderr, "\n");
    }
    return list;
}

uint32_t season_arg_fixtures(uint32_t league)
{
    if (!season_sizes_on() || !readable(league)) return 0;
    uint32_t prm;
    int half;
    if (is_prm(league) && g_mem[league + STAGE_INDEX] == 0xff) prm = league, half = 0;
    else if (building_stages && league != building_stages && g_mem[league + STAGE_INDEX] == 0) prm = building_stages, half = 1;
    else return 0;
    int n = (int16_t)rd16(g_mem, league + COMP_CLUBS), d = (int16_t)rd16(g_mem, league + COMP_DATES);
    int year = (int16_t)rd16(g_mem, league + COMP_YEAR);
    Form f;
    if (!form_of(prm, n, &f) || (!f.zones && !f.extra) || (half && !f.halves) || n % 2) return 0;
    if (d != f.rounds) {
        fprintf(stderr, "recomp: league sizes: arg_prm %d: %d dates for %d rounds: the class's fixtures\n", year, d,
                f.rounds);
        return 0;
    }
    uint32_t table = rd32(g_mem, league + COMP_TABLE), club[MAX_CLUBS];
    if (!readable(table)) return 0;
    for (int i = 0; i < n; i++) {
        club[i] = rd32(g_mem, table + (uint32_t)i * ROW_SIZE);
        if (!readable(club[i])) return 0;
    }
    if (half == 1) {
        /* The Clausura: the Apertura's matches with the venues reversed, when it holds the same clubs; in a split
         * season the second half of the plan's rounds. */
        if (last.obj != prm || last.year != year || last.n != n || last.rounds != d * (f.split ? 2 : 1)) return 0;
        int found = 0;      /* every club plays in the first round of both zones */
        for (int i = 0; i < n; i++)
            for (int k = 0; k < n / 2; k++)
                found += rd32(g_mem, club[i]) == last.home[0][k] || rd32(g_mem, club[i]) == last.away[0][k];
        if (found != n) return 0;
        return f.split ? fixed_list_from(league, d, d, n / 2, 0) : fixed_list(league, d, n / 2, 1);
    }
    static int round[SEASON_ARG_MAX_ROUNDS][SEASON_ARG_MAX_CLUBS / 2][2];
    int at[MAX_CLUBS];
    zplan_obj = prm;
    zplan_year = year;
    int rounds = f.zones ? zones_plan(&f, club, n, round, at) : derby_plan(&f, club, n, year, round, at);
    if (rounds != d * (f.split ? 2 : 1)) {
        fprintf(stderr, "recomp: league sizes: arg_prm %d: no plan of %d rounds for %d clubs: the class's fixtures\n",
                year, d, n);
        return 0;
    }
    last.obj = prm;
    last.year = year;
    last.n = n;
    last.rounds = rounds;
    for (int r = 0; r < rounds; r++)
        for (int k = 0; k < n / 2; k++) {
            last.home[r][k] = rd32(g_mem, club[at[round[r][k][0]]]);
            last.away[r][k] = rd32(g_mem, club[at[round[r][k][1]]]);
        }
    return fixed_list(league, d, n / 2, 0);
}

/* ---- The stages ---- */

/* ArgPrm::create_stages 0x405950 (thiscall, no argument): the Clausura stage gets the matches of a half, or in one
 * table is complete with no dates; the aggregate table gets the matches of the season. check_complete 0x689ad0 then
 * finds each complete (rows +0x11 against +0xa7 at 0x689b47..0x689b58; the stages at 0x689bbc..0x689be8). */
void o_00405950(void);
RT_WRAP(00405950, arg_create_stages)
{
    uint32_t self = cpu.ecx;
    building_stages = self;
    o_00405950();
    building_stages = 0;
    if (rt_unwinding) return;
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    Form f;
    uint32_t stages = rd32(g_mem, self + STAGES);
    if (season_sizes_on() && is_prm(self)) {
        Cpu saved = cpu;
        season_arg_stages_made(self);           /* the zone tables and the count of the season's stages */
        if (rt_unwinding) return;
        cpu = saved;
        stages = rd32(g_mem, self + STAGES);
    }
    if (!form_of(self, n, &f) || !readable(stages)) return;
    uint32_t clausura = rd32(g_mem, stages), aggregate = rd32(g_mem, stages + 4);
    if (readable(clausura)) {
        int dates = (int16_t)rd16(g_mem, clausura + COMP_DATES);
        if (f.halves && dates != f.rounds) {
            /* No room for the Clausura's rounds (season_arg_schedule): its rows count the class's matches. */
            wr16(g_mem, clausura + COMP_MATCHES, (uint16_t)((n - 1) * (int)g_mem[self + COMP_MEETINGS]));
        } else if (dates <= 1 && !f.halves) {
            /* No dates (season_arg_schedule): complete, so check_complete returns 1 at once (0x689b09..0x689b11)
             * and gives it no places, no prize money and no news. */
            wr32(g_mem, clausura + COMP_COMPLETE, 1);
        } else {
            wr16(g_mem, clausura + COMP_MATCHES, (uint16_t)f.matches);
        }
    }
    if (readable(aggregate)) wr16(g_mem, aggregate + COMP_MATCHES, (uint16_t)(f.matches * (f.halves ? 2 : 1)));
}

/* League::record_season_honours 0x684640 (thiscall, no argument): ArgPrm::record_season_honours calls it for the
 * Clausura stage (vtable +0x30 at 0x405edc). A Clausura of no dates (one table) has no champion. */
void o_00684640(void);
RT_WRAP(00684640, league_record_honours)
{
    uint32_t self = cpu.ecx, prm = prm_object();
    uint32_t stages = readable(prm) && season_sizes_on() ? rd32(g_mem, prm + STAGES) : 0;
    if (readable(stages) && self == rd32(g_mem, stages) && (int16_t)rd16(g_mem, self + COMP_DATES) <= 1 &&
        rd32(g_mem, self + COMP_COMPLETE) == 1)
    {
        rt_ret_stdcall(0, 0);
        return;
    }
    /* A half with a title knockout: its winner and finalist are the champion and the runner-up. */
    uint32_t rows[SEASON_ARG_HONOURS_ROWS];
    if (prm && season_arg_honours_swap(self, rows)) {
        o_00684640();
        season_arg_honours_restore(self, rows);     /* also when the call unwinds */
        return;
    }
    o_00684640();
}

/* ---- The relegation averages ---- */

static int in_division(uint32_t self, int32_t id)
{
    uint32_t clubs = rd32(g_mem, CLUBS);
    if (id < 0 || !readable(clubs)) return 0;
    return rd32(g_mem, clubs + (uint32_t)id * CLUB_SIZE + CLUB_DIVISION) == rd32(g_mem, self + COMP_RECORD);
}

/* The records of the clubs of the division, and a free record (id -1, the edition) for each other place: the count of
 * the division. */
void season_arg_averages_fit(uint32_t self, int n, const char *file)
{
    uint32_t old = rd32(g_mem, self + AVERAGES);
    int have = (int16_t)rd16(g_mem, self + AVERAGE_COUNT), keep = 0, stale = 0;
    if (!readable(old) || have < 1 || have > 4 * MAX_CLUBS) return;
    for (int i = 0; i < have; i++) {
        int32_t id = (int32_t)rd32(g_mem, old + (uint32_t)i * AVG_SIZE);
        if (id == -1) continue;
        if (in_division(self, id)) keep++; else stale++;
    }
    if (have == n && !stale) {
        if (logging())
            fprintf(stderr, "recomp: league sizes: %s %d: averages of %d clubs (%d held, %d kept, 0 left the "
                            "division)\n", file, (int16_t)rd16(g_mem, self + COMP_YEAR), n, have, keep);
        return;
    }
    uint32_t block = season_sizes_game_malloc((uint32_t)n * AVG_SIZE);
    if (!block) return;
    int k = 0;
    for (int i = 0; i < have && k < n; i++) {
        uint32_t r = old + (uint32_t)i * AVG_SIZE;
        int32_t id = (int32_t)rd32(g_mem, r);
        if (id != -1 && in_division(self, id)) memcpy(g_mem + block + (uint32_t)(k++) * AVG_SIZE, g_mem + r, AVG_SIZE);
    }
    for (; k < n; k++) {    /* as the original's empty records (0x406538..0x40654a) */
        wr32(g_mem, block + (uint32_t)k * AVG_SIZE, 0xffffffffu);
        wr16(g_mem, block + (uint32_t)k * AVG_SIZE + 8, rd16(g_mem, self + COMP_YEAR));
    }
    season_sizes_game_free(old);
    wr32(g_mem, self + AVERAGES, block);
    wr16(g_mem, self + AVERAGE_COUNT, (uint16_t)n);
    if (logging())
        fprintf(stderr, "recomp: league sizes: %s %d: averages of %d clubs (%d held, %d kept, %d left the division)\n",
                file, (int16_t)rd16(g_mem, self + COMP_YEAR), n, have, keep, stale);
}

/* ArgPrm::update_relegation_averages 0x406570 (thiscall (new_season), ret 4). At the end of a season (0, from
 * ArgPrm::record_season_honours 0x405cac) the records first take the count of the division: the original keeps the
 * count of its first season (+0x34 at 0x405f02), so the clubs of a grown division found no record and the last two
 * records were not the last two clubs. */
/* The averages of the season just played, as ArgPrm::update_relegation_averages(0) left them at its end (each record's
 * +4, points over matches of three seasons, 0x4066dc..0x4066f8): start_new_season 0x405720 calls update(1) (0x40572e)
 * before the chain, which drops the oldest season (0x406669..0x4066a0) and sorts on the other two. The size step
 * (season_arg_step_order) ranks by these. */
static struct { uint32_t obj; int year, n; int32_t id[SNAP_MAX]; float avg[SNAP_MAX]; } snap;

void o_00406570(void);
RT_WRAP(00406570, arg_update_averages)
{
    uint32_t self = cpu.ecx;
    if (rt_arg(0) == 1 && season_sizes_on() && is_prm(self)) {
        uint32_t avg = rd32(g_mem, self + AVERAGES);
        int have = (int16_t)rd16(g_mem, self + AVERAGE_COUNT);
        snap.obj = self;
        snap.year = (int16_t)rd16(g_mem, self + COMP_YEAR);
        snap.n = 0;
        for (int k = 0; readable(avg) && k < have && snap.n < SNAP_MAX; k++) {
            uint32_t r = avg + (uint32_t)k * AVG_SIZE;
            int32_t id = (int32_t)rd32(g_mem, r);
            if (id < 0) continue;
            uint32_t bits = rd32(g_mem, r + 4);
            float f;
            memcpy(&f, &bits, sizeof f);
            snap.id[snap.n] = id;
            snap.avg[snap.n++] = f;
        }
    }
    if (rt_arg(0) == 0 && season_sizes_on() && is_prm(self)) {
        int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
        if (n >= MIN_CLUBS && n <= MAX_CLUBS) season_arg_averages_fit(self, n, "arg_prm");
        if (rt_unwinding) return;
    }
    o_00406570();
}

/* ArgPrm::init_relegation_averages 0x405ef0 (thiscall, no argument), called by the constructor (0x404457): it writes
 * 20 records of 2001 clubs (the last at 0x406441) into a block of the count's records (0x405f0a). A season game with
 * fewer than 20 clubs (a database short of clubs, D12) gets the original's empty records instead. */
void o_00405ef0(void);
RT_WRAP(00405ef0, arg_init_averages)
{
    uint32_t self = cpu.ecx;
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    if (!season_sizes_on() || n >= CLASS_CLUBS || n < 1) {
        o_00405ef0();
        return;
    }
    season_arg_averages_empty(self, n);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}

void season_arg_averages_empty(uint32_t self, int n)
{
    uint32_t block = season_sizes_game_malloc((uint32_t)n * AVG_SIZE);
    if (rt_unwinding) return;
    if (block)
        for (int k = 0; k < n; k++) {
            wr32(g_mem, block + (uint32_t)k * AVG_SIZE, 0xffffffffu);
            wr16(g_mem, block + (uint32_t)k * AVG_SIZE + 8, rd16(g_mem, self + COMP_YEAR));
        }
    wr32(g_mem, self + AVERAGES, block);
    wr16(g_mem, self + AVERAGE_COUNT, (uint16_t)(block ? n : 0));
}

/* The clubs that go down at the end of a season of the rules row (gap row 54; research notes of 15 November 2026 from
 * RSSSF arg02 to arg2026, es and en Wikipedia): the worst averages, then (from the 2023 season) the worst club of the
 * annual table that the averages did not send down. Rows are the game's editions (row 2014 the 2015 season, row 2015
 * the 2016 tournament, row 2016 the 2016/17 season, row 2022 the 2023 season). Rows to 2011 also had a promoción of
 * the 17th and 18th of the averages against the second division: rows 2002 to 2011 play it as two ties of the Primera
 * (season_arg_stages.c promo_advance), the 2001 edition as the second division's own ties (0x408910). Its losers go
 * down after the clubs of arg_down (arg_record_honours). Rows
 * from 2026 take the 2027 format (one by the averages, one by the annual table). The counts are
 * season_arg_down_places (season_arg_plan.c). */
#define F_SORT_TABLE 0x006827d0u    /* League::sort_table, thiscall (rows or 0), ret 4 */
#define F_RECORD_HONOURS 0x00684640u
#define ROW_POSITION 4u
#define ROW_STATUS 0x37u
#define STATUS_DOWN 3               /* 0x405e4f */

/* The clubs that go down at the end of the season of an ArgPrm (arg_down): the worst averages first, then the worst
 * club of the annual table that the averages kept. It first brings the averages to the season's points with
 * update_relegation_averages(0) (vtable +0x34, as 0x405cb0): that call only rewrites each club's slot of the current
 * season from the annual table and sorts (0x406589..0x40671b), so a second call gives the same records. Returns the
 * count (0 on unwinding), with the count by the averages, the lowest average kept and the annual table's place. */
static int relegated(uint32_t self, const Form *f, int32_t *down, int *by_avg_out, float *worst_kept_out,
                     int *worst_pos_out, int *rows_out)
{
    uint32_t stages = rd32(g_mem, self + STAGES);
    uint32_t vt = rd32(g_mem, self), a0[1] = {0};
    call_this(rd32(g_mem, vt + 0x34), self, 1, a0);           /* the averages of the season (0x405cb0) */
    if (rt_unwinding) return 0;
    int avg, tab, nd = 0;
    season_arg_down_places(f->season, &avg, &tab);
    uint32_t recs = rd32(g_mem, self + AVERAGES), clubs = rd32(g_mem, CLUBS);
    int have = (int16_t)rd16(g_mem, self + AVERAGE_COUNT);
    float worst_kept = 1e9f;
    for (int k = have - 1; readable(recs) && k >= 0; k--) {
        int32_t id = (int32_t)rd32(g_mem, recs + (uint32_t)k * AVG_SIZE);
        if (id < 0 || !in_division(self, id)) continue;
        uint32_t bits = rd32(g_mem, recs + (uint32_t)k * AVG_SIZE + 4);
        float a;
        memcpy(&a, &bits, sizeof a);
        if (nd < avg) down[nd++] = id;
        else if (a < worst_kept) worst_kept = a;
    }
    int by_avg = nd, worst_pos = -1, rows_n = 0;
    uint32_t annual = readable(stages) ? rd32(g_mem, stages + 4) : 0;
    if (tab && readable(annual)) {
        uint32_t a1[1] = {0};
        call_this(F_SORT_TABLE, annual, 1, a1);
        if (rt_unwinding) return 0;
        uint32_t rows = rd32(g_mem, annual + COMP_TABLE);
        rows_n = (int16_t)rd16(g_mem, annual + COMP_CLUBS);
        int pick = -1;
        for (int r = 0; readable(rows) && r < rows_n; r++) {
            uint32_t c = rd32(g_mem, rows + (uint32_t)r * ROW_SIZE);
            if (!readable(c)) continue;
            int32_t id = (int32_t)((c - clubs) / CLUB_SIZE), skip = 0;
            for (int i = 0; i < nd; i++) skip |= down[i] == id;
            int pos = g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_POSITION];
            if (!skip && in_division(self, id) && pos > worst_pos) worst_pos = pos, pick = id;
        }
        if (pick >= 0) down[nd++] = pick;
    }
    if (by_avg_out) *by_avg_out = by_avg;
    if (worst_kept_out) *worst_kept_out = worst_kept;
    if (worst_pos_out) *worst_pos_out = worst_pos;
    if (rows_out) *rows_out = rows_n;
    return nd;
}

/* The promoción of 2001/02 to 2011/12 (research notes of 15 November 2026, EN 2010-11 and 2011-12): the 17th and 18th
 * of the averages play two legs against the second division. club[0] the 17th, club[1] the 18th. */
int season_arg_promo_primera(uint32_t self, uint32_t *club)
{
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    Form f;
    int32_t down[MAX_CLUBS];
    if (!is_prm(self) || !form_of(self, n, &f)) return 0;
    int nd = relegated(self, &f, down, NULL, NULL, NULL, NULL);
    if (rt_unwinding) return 0;
    uint32_t recs = rd32(g_mem, self + AVERAGES), clubs = rd32(g_mem, CLUBS);
    int have = (int16_t)rd16(g_mem, self + AVERAGE_COUNT), got = 0;
    int32_t w[2];
    for (int k = have - 1; readable(recs) && k >= 0 && got < 2; k--) {
        int32_t id = (int32_t)rd32(g_mem, recs + (uint32_t)k * AVG_SIZE), skip = 0;
        if (id < 0 || !in_division(self, id)) continue;
        for (int i = 0; i < nd; i++) skip |= down[i] == id;
        if (!skip) w[got++] = id;                   /* w[0] the 18th, w[1] the 17th */
    }
    if (got < 2) return 0;
    club[0] = clubs + (uint32_t)w[1] * CLUB_SIZE;
    club[1] = clubs + (uint32_t)w[0] * CLUB_SIZE;
    return 2;
}

/* Items 286 and 291: the moves of a season that the place bytes do not hold.
 * The Primera: ArgPrm::set_up_rules writes 0 relegation places (0x4056d1), because the class sends down by its averages:
 * the row's direct places, or the last two records of the class (0x405cb3, 0x405d6a). A loser of the promoción goes down
 * after them (arg_record_honours), so the 17th and 18th of the averages are its 2 play-off places to row 2011.
 * The second division in the class's form (2001/02): the Apertura's winner (+0xbe 1) and the Reducido's winner (status 0
 * at 0x408d54) go up; 2 go down by the averages (0x408666) and the size step sends down the surplus over the next row.
 * In one table of rows 2002 to 2011 the two clubs after the direct places play the Primera's promoción (promo_advance). */
void season_arg_report_places(uint32_t obj, int *up, int *po_up, int *po_down, int *down)
{
    int year = readable(obj) ? (int16_t)rd16(g_mem, obj + COMP_YEAR) : 0;
    int n = readable(obj) ? (int16_t)rd16(g_mem, obj + COMP_CLUBS) : 0;
    if (is_prm(obj)) {
        Form f;
        int avg = SEASON_ARG_CLASS_DOWN, tab = 0, row = year;
        if (form_of(obj, n, &f)) {
            row = f.season;
            season_arg_down_places(row, &avg, &tab);
        }
        *down = avg + tab;
        if (row <= SEASON_ARG_PROMO_LAST) *po_down = 2;
        return;
    }
    if (!readable(obj) || strcmp(season_change_file(obj), "arg_second")) return;
    if (!season_arg_second_one_table(obj)) {
        *up += 1;
        *down = season_arg_second_class_down(n, season_changing_rules() ? season_sizes_clubs("arg_second", year + 1) : -1);
        return;
    }
    int row = season_sizes_row_season(obj, "arg_second");
    if (row >= SEASON_ARG_PROMO_FIRST && row <= SEASON_ARG_PROMO_LAST) *po_up = 2;
    int a, b;
    if (season_arg_second_zones_live(obj, n, &a, &b)) {     /* item 291: the final's and the Reducido's winners, and the zones */
        *up = 2;
        *down = season_arg_second_row_down(row);
    }
}

/* Item 291: the row of 2001/02 sends down the worst coefficients. The records hold each club's average (+4, as
 * season_arg_step_order reads the Primera's); a club with no record goes after those that have one. */
void season_arg_second_step_order(uint32_t obj, uint32_t *club, int n)
{
    if (!season_sizes_on() || !readable(obj) || n < 1 || n > SNAP_MAX || season_arg_second_one_table(obj)) return;
    uint32_t clubs = rd32(g_mem, CLUBS), recs = rd32(g_mem, obj + AVERAGES);
    int have = (int16_t)rd16(g_mem, obj + AVERAGE_COUNT);
    if (!readable(clubs) || !readable(recs) || have < 1) return;
    float key[SNAP_MAX];
    int has[SNAP_MAX];
    for (int i = 0; i < n; i++) {
        has[i] = 0;
        key[i] = 0;
        int32_t id = (int32_t)((club[i] - clubs) / CLUB_SIZE);
        for (int k = 0; k < have && !has[i]; k++) {
            uint32_t r = recs + (uint32_t)k * AVG_SIZE, bits = rd32(g_mem, r + 4);
            if ((int32_t)rd32(g_mem, r) != id) continue;
            memcpy(&key[i], &bits, sizeof key[i]);
            has[i] = 1;
        }
    }
    for (int i = 1; i < n; i++) {       /* insertion: stable */
        uint32_t c = club[i];
        float kv = key[i];
        int hv = has[i], j = i;
        for (; j > 0 && (has[j - 1] < hv || (has[j - 1] == hv && hv && key[j - 1] < kv)); j--)
            club[j] = club[j - 1], key[j] = key[j - 1], has[j] = has[j - 1];
        club[j] = c;
        key[j] = kv;
        has[j] = hv;
    }
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_second step order %d by the averages: worst %u, then %u\n",
                (int16_t)rd16(g_mem, obj + COMP_YEAR), rd32(g_mem, club[n - 1]), n > 1 ? rd32(g_mem, club[n - 2]) : 0u);
}

/* The clubs (club records) that will go down at the end of the season, for the Clausura's knockout (gap row of
 * art. 14.1.2 of the LPF reglamento 2025 and 2026: a club in the relegation zone does not play it). Returns the count. */
int season_arg_relegated(uint32_t self, uint32_t *club, int max)
{
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    Form f;
    int32_t down[MAX_CLUBS];
    if (!is_prm(self) || !form_of(self, n, &f)) return 0;
    int nd = relegated(self, &f, down, NULL, NULL, NULL, NULL);
    uint32_t clubs = rd32(g_mem, CLUBS);
    int k = 0;
    for (int i = 0; i < nd && k < max; i++) club[k++] = clubs + (uint32_t)down[i] * CLUB_SIZE;
    return k;
}

/* ArgPrm::record_season_honours 0x405ca0 (thiscall, no argument): the original sends down the clubs of the last two
 * records of the averages (+0x34 - 2 and - 1 at 0x405cb3, 0x405d6a): vtable +0x40 (club, 3, -1, 0, 0) and +0x68 (club,
 * -2, place, place, place), status 3 in the rows of the league, stages[0] and stages[1] (0x405e23..0x405ec8), then
 * League::record_season_honours 0x684640 for the league and stages[0] +0x30. In a season game with the sizes on the
 * same steps run for the clubs of the row (arg_down). */
void o_00405ca0(void);
RT_WRAP(00405ca0, arg_record_honours)
{
    uint32_t self = cpu.ecx;
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    Form f;
    uint32_t stages = rd32(g_mem, self + STAGES);
    if (!is_prm(self) || !form_of(self, n, &f) || !readable(stages)) {
        o_00405ca0();
        return;
    }
    uint32_t vt = rd32(g_mem, self), clubs = rd32(g_mem, CLUBS), recs, annual = rd32(g_mem, stages + 4);
    int32_t down[MAX_CLUBS];
    int by_avg = 0, worst_pos = -1, rows_n = 0;
    float worst_kept = 1e9f;
    int nd = relegated(self, &f, down, &by_avg, &worst_kept, &worst_pos, &rows_n);
    if (rt_unwinding) return;
    /* The promoción's losers go down after them (the stages of the season are complete here). */
    int n_rel = nd;
    uint32_t pd[2], pu[2];
    int np = season_arg_promo_results(self, pd, pu);
    /* The 18th's tie last in pd: first in down, so the 18th takes the lower place (n - i below). */
    for (int i = np - 1; i >= 0 && nd < MAX_CLUBS; i--) down[nd++] = (int32_t)((pd[i] - clubs) / CLUB_SIZE);
    recs = rd32(g_mem, self + AVERAGES);
    int have = (int16_t)rd16(g_mem, self + AVERAGE_COUNT);
    /* The worst last, as the original (count - 2 first, then count - 1). */
    for (int i = nd - 1; i >= 0; i--) {
        uint32_t club = clubs + (uint32_t)down[i] * CLUB_SIZE, place = (uint32_t)(n - i);
        uint32_t s1[5] = {club, 3, 0xffffffffu, 0, 0}, s2[5] = {club, 0xfffffffeu, place, place, place};
        call_this(rd32(g_mem, vt + 0x40), self, 5, s1);
        if (rt_unwinding) return;
        call_this(rd32(g_mem, vt + 0x68), self, 5, s2);
        if (rt_unwinding) return;
    }
    uint32_t tables[3] = {self, rd32(g_mem, stages), annual};
    for (int t = 0; t < 3; t++) {
        uint32_t rows = readable(tables[t]) ? rd32(g_mem, tables[t] + COMP_TABLE) : 0;
        int m = readable(tables[t]) ? (int16_t)rd16(g_mem, tables[t] + COMP_CLUBS) : 0;
        for (int r = 0; readable(rows) && r < m; r++) {
            uint32_t c = rd32(g_mem, rows + (uint32_t)r * ROW_SIZE);
            for (int i = 0; i < nd; i++)
                if (c == clubs + (uint32_t)down[i] * CLUB_SIZE) g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_STATUS] = STATUS_DOWN;
        }
    }
    if (logging()) {
        fprintf(stderr, "recomp: league sizes: arg_prm relegation %d: row %d, %d by the averages:",
                (int16_t)rd16(g_mem, self + COMP_YEAR), f.season, by_avg);
        for (int i = 0; i < by_avg; i++) {
            float a = 0;
            for (int k = 0; readable(recs) && k < have; k++)
                if ((int32_t)rd32(g_mem, recs + (uint32_t)k * AVG_SIZE) == down[i]) {
                    uint32_t bits = rd32(g_mem, recs + (uint32_t)k * AVG_SIZE + 4);
                    memcpy(&a, &bits, sizeof a);
                }
            fprintf(stderr, " %u:%.4f", rd32(g_mem, clubs + (uint32_t)down[i] * CLUB_SIZE), (double)a);
        }
        fprintf(stderr, "; lowest average kept %.4f; %d by the annual table:", (double)worst_kept, n_rel - by_avg);
        for (int i = by_avg; i < n_rel; i++) fprintf(stderr, " %u:%d", rd32(g_mem, clubs + (uint32_t)down[i] * CLUB_SIZE), worst_pos);
        fprintf(stderr, " of %d\n", rows_n);
        uint32_t tw[2], tl[2];
        int nt = season_arg_promo_ties(self, tw, tl);
        for (int i = 0; i < nt; i++)
            fprintf(stderr, "recomp: league sizes: arg_prm promocion %d: tie %d: winner %u, loser %u\n",
                    (int16_t)rd16(g_mem, self + COMP_YEAR), i, rd32(g_mem, tw[i]), rd32(g_mem, tl[i]));
        if (np)
            for (int i = 0; i < np; i++)
                fprintf(stderr, "recomp: league sizes: arg_prm promocion %d: %u %.30s goes down, %u %.30s goes up\n",
                        (int16_t)rd16(g_mem, self + COMP_YEAR), rd32(g_mem, pd[i]), win_str(pd[i] + CLUB_NAME),
                        rd32(g_mem, pu[i]), win_str(pu[i] + CLUB_NAME));
        fprintf(stderr, "recomp: league sizes: arg_prm averages %d, the worst 4 last:", (int16_t)rd16(g_mem, self + COMP_YEAR));
        int shown = 0, at[4];
        for (int k = have - 1; readable(recs) && k >= 0 && shown < 4; k--) {
            int32_t id = (int32_t)rd32(g_mem, recs + (uint32_t)k * AVG_SIZE);
            if (id >= 0 && in_division(self, id)) at[shown++] = k;
        }
        for (int i = shown - 1; i >= 0; i--) {
            uint32_t r = recs + (uint32_t)at[i] * AVG_SIZE, bits = rd32(g_mem, r + 4);
            float a;
            memcpy(&a, &bits, sizeof a);
            fprintf(stderr, " %u:%.4f", rd32(g_mem, clubs + rd32(g_mem, r) * CLUB_SIZE), (double)a);
        }
        fprintf(stderr, "\n");
    }
    call_this(F_RECORD_HONOURS, self, 0, NULL);
    if (rt_unwinding) return;
    uint32_t clausura = rd32(g_mem, stages);
    if (readable(clausura)) call_this(rd32(g_mem, rd32(g_mem, clausura) + 0x30), clausura, 0, NULL);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

uint32_t rt_arg_apertura_dates(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on() || !is_prm(obj) || season_sizes_kept(obj)) return original;
    int d = (int16_t)rd16(g_mem, obj + COMP_DATES);
    return d >= 1 ? (uint32_t)d : original;
}

void season_arg_step_order(uint32_t obj, uint32_t *club, int n)
{
    if (!season_sizes_on() || !readable(obj) || n < 1 || n > SNAP_MAX) return;
    uint32_t clubs = rd32(g_mem, CLUBS);
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    if (!readable(clubs) || snap.obj != obj || snap.year != year) {
        fprintf(stderr, "recomp: league sizes: arg_prm %d: no averages of the season for the step; the table's order\n",
                year);
        return;
    }
    /* Best first: the highest average; a club with none (it never played the division) after them, in table order. */
    float key[SNAP_MAX];
    int has[SNAP_MAX];
    for (int i = 0; i < n; i++) {
        has[i] = 0;
        key[i] = 0;
        for (int k = 0; k < snap.n; k++)
            if (clubs + (uint32_t)snap.id[k] * CLUB_SIZE == club[i]) {
                has[i] = 1;
                key[i] = snap.avg[k];
                break;
            }
    }
    for (int i = 1; i < n; i++) {       /* insertion: stable */
        uint32_t c = club[i];
        float kv = key[i];
        int hv = has[i], j = i;
        for (; j > 0 && (has[j - 1] < hv || (has[j - 1] == hv && hv && key[j - 1] < kv)); j--)
            club[j] = club[j - 1], key[j] = key[j - 1], has[j] = has[j - 1];
        club[j] = c;
        key[j] = kv;
        has[j] = hv;
    }
    if (logging()) {
        fprintf(stderr, "recomp: league sizes: arg_prm step order %d by the averages:", year);
        for (int i = 0; i < n; i++)
            fprintf(stderr, " %u:%.50s:%s%.4f|", rd32(g_mem, club[i]), win_str(club[i] + CLUB_NAME), has[i] ? "" : "none ",
                    (double)key[i]);
        fprintf(stderr, "\n");
    }
}

uint32_t rt_arg_count(uint32_t original)
{
    if (!season_sizes_on()) return original;
    uint32_t prm = prm_object();
    int n = readable(prm) ? (int16_t)rd16(g_mem, prm + COMP_CLUBS) : 0;
    if (n < MIN_CLUBS || n > MAX_CLUBS || n == CLASS_CLUBS) return original;
    int v = (int)original + n - CLASS_CLUBS;
    return v > 0 ? (uint32_t)v : original;
}

/* ---- The size step (D13) ---- */

/* ArgPrm::process_promotion_relegation_chain 0x4067f0 (thiscall (arg), ret 4; vtable +0xb0, called by
 * start_new_season 0x405737): it moves the clubs itself (0x6830b0 at 0x406864, 0x6831a0 at 0x4068bc), not through
 * league_promote_and_relegate 0x689c80, so the step gets the pair here. */
void o_004067f0(void);
RT_WRAP(004067f0, arg_chain)
{
    uint32_t self = cpu.ecx;
    if (season_sizes_on() && is_prm(self)) {
        uint32_t table = rd32(g_mem, COMP_OBJECTS);
        int32_t id = (int32_t)rd32(g_mem, KEY_SECOND);
        if (readable(table) && id >= 0) season_arg_reducido_chain(rd32(g_mem, table + 4u * (uint32_t)id));
        /* The promoción's winners of the second division go up: status 0 in its table (0x406839); its losers stay
         * down: 0xfe, as the second division's own ties (0x408cd0). Its stages still exist here (start_new_season
         * frees them after the chain, 0x4057b5). With the sizes off arg_record_honours sent no loser down, so no
         * winner goes up either. */
        uint32_t pd[2], pu[2], tw[2], tl[2];
        int np = season_arg_promo_results(self, pd, pu), nt = season_arg_promo_ties(self, tw, tl);
        uint32_t objects = rd32(g_mem, COMP_OBJECTS);
        int32_t key = (int32_t)rd32(g_mem, KEY_SECOND);
        uint32_t second = nt && readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0;
        uint32_t rows = readable(second) ? rd32(g_mem, second + COMP_TABLE) : 0;
        int m = readable(second) ? (int16_t)rd16(g_mem, second + COMP_CLUBS) : 0;
        for (int r = 0; readable(rows) && r < m; r++) {
            uint32_t c = rd32(g_mem, rows + (uint32_t)r * ROW_SIZE);
            for (int i = 0; i < nt; i++)
                if (c == tl[i]) g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_STATUS] = 0xfe;
            for (int i = 0; i < np; i++)
                if (c == pu[i]) g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_STATUS] = 0;
        }
    }
    if (season_sizes_on() && is_prm(self) && logging()) {
        /* The clubs the chain moves: ArgPrm rows of status 3 go down (0x406895), ArgSecond rows of status 0 or 5 go
         * up (0x406839); 0xfe marks the clubs a promoción kept (0x408cd0). */
        uint32_t objects = rd32(g_mem, COMP_OBJECTS);
        int32_t key = (int32_t)rd32(g_mem, KEY_SECOND);
        uint32_t lg[2] = {self, readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0};
        fprintf(stderr, "recomp: league sizes: arg_prm chain %d:", (int16_t)rd16(g_mem, self + COMP_YEAR));
        for (int t = 0; t < 2; t++) {
            uint32_t rows = readable(lg[t]) ? rd32(g_mem, lg[t] + COMP_TABLE) : 0;
            int m = readable(lg[t]) ? (int16_t)rd16(g_mem, lg[t] + COMP_CLUBS) : 0;
            fprintf(stderr, t ? "; second:" : " primera:");
            for (int r = 0; readable(rows) && r < m; r++) {
                uint32_t row = rows + (uint32_t)r * ROW_SIZE, c = rd32(g_mem, row);
                int st = g_mem[row + ROW_STATUS];
                if (readable(c) && (t ? (st == 0 || st == 5 || st == 0xfe) : (st == 3 || st == 0xfe)))
                    fprintf(stderr, " %u:%d:%d", rd32(g_mem, c), g_mem[row + ROW_POSITION], st);
            }
        }
        fprintf(stderr, "\n");
    }
    o_004067f0();
    if (rt_unwinding || !season_sizes_on() || !is_prm(self)) return;
    uint32_t objects = rd32(g_mem, COMP_OBJECTS);
    int32_t key = (int32_t)rd32(g_mem, KEY_SECOND);
    uint32_t second = readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0;
    if (readable(second)) season_sizes_step_note(self, second);
}

uint32_t season_arg_third_record(void)
{
    /* The record of a key id: [0xae23d0] + id x 0x6b, as 0x40689d..0x4068b6. */
    int32_t id = (int32_t)rd32(g_mem, 0x9cf688u);
    uint32_t table = rd32(g_mem, 0xae23d0u);
    return season_sizes_on() && id >= 0 && readable(table) ? table + (uint32_t)id * 0x6bu : 0;
}

int season_arg_third(uint32_t record)
{
    return season_sizes_on() && readable(record) && rd32(g_mem, record) == rd32(g_mem, 0x9cf688u);
}
