/* Super cups by season (0.6.0 slice 21c, docs/research/2026-10-18-cups-more.md, section 2): the four-club
 * Supercoppa Italiana (2023/24 to 2025/26) and Supercopa de Espana (from 2019/20), and the clubs of the new super
 * cups of slice 21c (season_comps.c), which take the class ItaSuper.
 *
 * The classes ItaSuper (vtable 0x96c8b8) and SpaSuper (vtable 0x96fd38) are the same code: the constructor
 * (0x65f670, 0x8586b0) and start_new_season (0x65fc90, 0x858cd0) call the team picker (init_team_list 0x65fa60,
 * select_teams 0x858aa0: +0x3e = 2 clubs in a list of 6-byte entries at +0xa7, from comp_util_get_latest_placed_club
 * 0x4afd80 of the league and the cup records) and then create_round_table (0x65f920, 0x858960; vtable +0x3c,
 * stage -1: one round, 0x68 bytes) and map_fixture_tree 0x518790. Both pickers are direct calls, so their wraps
 * test the cup and the season:
 * - a season whose rules table row gives 4 clubs (season_super_four): the picker's list becomes the four clubs
 *   (season_super_four_pick: a place that a club already holds goes to the next league club, O342) and the round
 *   table two rounds: semi-finals (code 130, flag 0x200, so the ties are the
 *   list's pairs, map_fixture_tree 0x519279) and the final (code 150), one match each, on the dates of the row;
 * - an object of a new super cup (season_comps.c, its own vtable copy): the picker takes the clubs of its nation.
 * The table is made only at a season start or a new game; a save keeps the list and the table (Cup::save_cup_to_disk
 * 0x5210f0, Cup::load_from_disk 0x51fc00), so a load changes nothing. The base game (2001/02, changing rules off)
 * and RECOMP_SEASON_CODE=off get none of it (Q152, Q342): the rules table has no four-club row before 2019 and the
 * wraps test season_game(). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_cups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_RECORDS 0xae23d0u          /* the comp records, 0x6b bytes each */
#define RECORD_SIZE 0x6bu
#define CUP_YEAR 0x40u                  /* s16 */
#define CUP_TEAMS 0x3eu                 /* s16 */
#define CUP_TEAM_LIST 0xa7u             /* 6-byte entries: club, two bytes */
#define ROUND_SIZE 0x68u

#define F_PLACED 0x004afd80u            /* comp_util_get_latest_placed_club, cdecl (record, place 0 to 2) */
#define F_MALLOC 0x00944e46u            /* malloc, cdecl (size) */
#define F_FREE 0x009452cau              /* free, cdecl (block) */
/* cup_set_round_draw_date, cdecl (table, round, day, month, year+, weekday, year) */
#define F_DRAW_DATE 0x00521e60u
#define F_MATCH_DATE 0x00521eb0u        /* cup_set_round_match_date, cdecl (table, round, day, month, year+, weekday,
                                         * kick-off slot, year, venue) */

static int readable(uint32_t p) { return rt_readable(p); }

static int season_game(void)
{
    const char *v = getenv("RECOMP_CUPS");
    if (v && !strcmp(v, "off")) return 0;
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

/* The season of a cup: its year with changing rules on, the start year with changing rules off (Q232). */
static int cup_season(uint32_t cup)
{
    if (!season_changing_rules()) return season_start_year();
    int y = (int16_t)rd16(g_mem, cup + CUP_YEAR);
    return y >= SEASON_FIRST_YEAR && y <= SEASON_LAST_YEAR ? y : season_start_year();
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

/* The club of the latest history row of a key competition at a place (0 winner, 1 runner-up, 2 third), or 0. */
static uint32_t placed(uint32_t key_global, int place)
{
    int32_t key = (int32_t)rd32(g_mem, key_global);
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    if (key < 0 || !readable(base)) return 0;
    uint32_t a[2] = {base + (uint32_t)key * RECORD_SIZE, (uint32_t)place};
    uint32_t club = call_cdecl(F_PLACED, 2, a);
    return rt_unwinding || !readable(club) ? 0 : club;
}

/* The super cups: the class file or the file of a new competition, the key id globals of the league and the cup
 * whose clubs it takes (data map, docs/research/data-map/globals.tsv), and the club that meets the champion when
 * the champion also won the cup: 1 the cup runner-up (the original's ItaSuper 0x65fb6b and SpaSuper 0x858bab), 0
 * the league runner-up. */
typedef struct {
    const char *file;
    uint32_t league, cup;
    int double_cup_runner_up;
} SuperCup;

static const SuperCup cups[] = {
    {"ita_super", 0x9cf570u, 0x9cf71cu, 1},            /* Serie A, Coppa Italia */
    {"spa_super", 0x9cf624u, 0x9cf740u, 1},            /* Primera Division, Copa del Rey */
    {"new_tur_super", 0x9cf8d8u, 0x9cf8f8u, 1},        /* Turkish Premier Division, Turkish FA Cup */
};

/* The file of a cup: the def whose constructor runs (season_new_cups.c; the object still has the class vtable), else
 * the class or the def of its vtable. */
static const char *file_of(uint32_t cup)
{
    const char *f = season_new_cups_making();
    return f ? f : season_change_file(cup);
}

static const SuperCup *cup_of(const char *file)
{
    for (size_t i = 0; file && i < sizeof cups / sizeof cups[0]; i++)
        if (!strcmp(cups[i].file, file)) return &cups[i];
    return NULL;
}

static void put_list(uint32_t cup, const uint32_t *clubs, int n)
{
    uint32_t list = game_malloc(6u * (uint32_t)n);
    if (!list) return;
    for (int i = 0; i < n; i++) wr32(g_mem, list + 6u * (uint32_t)i, clubs[i]);   /* the two bytes stay 0 */
    game_free(rd32(g_mem, cup + CUP_TEAM_LIST));
    if (rt_unwinding) return;
    wr32(g_mem, cup + CUP_TEAM_LIST, list);
    wr16(g_mem, cup + CUP_TEAMS, (uint16_t)n);
}

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_COUNT_LESS 0xae2350u       /* the clubs are [0xae235c] - 2 * [0xae2350] */
#define CLUB_SIZE 0x245u
#define CLUB_LAST_DIVISION 0x5bu        /* the division of the last season */
#define CLUB_LAST_PLACE 0x5fu           /* s8, the place in it (compare_clubs_by_seeding_pot 0x640590) */
#define LEAGUE_MAX 48

/* The league's clubs of the last season by place, for the places that the cup finalists leave (O342): the first
 * three of the history rows (comp_util_get_latest_placed_club), then the clubs whose last division (+0x5b) is the
 * league by their last place (+0x5f), as season_new_cups.c division_clubs orders them. Returns the count. */
static int league_order(uint32_t key_global, uint32_t *out)
{
    int n = 0;
    for (int p = 0; p < 3; p++) {
        uint32_t c = placed(key_global, p);
        if (rt_unwinding) return 0;
        if (!c) break;
        out[n++] = c;
    }
    if (n < 3) return n;
    int32_t key = (int32_t)rd32(g_mem, key_global);
    uint32_t base = rd32(g_mem, CLUBS);
    int32_t clubs = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, CLUB_COUNT_LESS);
    int rank[LEAGUE_MAX], first = n;
    for (int32_t i = 0; readable(base) && i < clubs && i < 200000 && n < LEAGUE_MAX; i++) {
        uint32_t club = base + (uint32_t)i * CLUB_SIZE, rec = rd32(g_mem, club + CLUB_LAST_DIVISION);
        int place = (int8_t)g_mem[club + CLUB_LAST_PLACE];
        if (!readable(rec) || (int32_t)rd32(g_mem, rec) != key || place < 1) continue;
        if (club == out[0] || club == out[1] || club == out[2]) continue;
        int k = n++;
        while (k > first && rank[k - 1] > place) out[k] = out[k - 1], rank[k] = rank[k - 1], k--;
        out[k] = club, rank[k] = place;
    }
    return n;
}

/* After the picker of a super cup (it returned 1 with two clubs in the list). */
static void teams_made(uint32_t cup)
{
    if (cpu.eax != 1 || !season_game() || !readable(cup)) return;
    const char *file = file_of(cup);
    const SuperCup *k = cup_of(file);
    int season = cup_season(cup);
    const SeasonSuperFour *four = k ? season_super_four(file, season) : NULL;
    if (!four) return;
    Cpu saved = cpu;
    uint32_t league[LEAGUE_MAX];
    int count = league_order(k->league, league);
    uint32_t cw = placed(k->cup, 0), cru = placed(k->cup, 1);
    if (rt_unwinding) return;
    if (getenv("RECOMP_SUPER_TRACE") && *getenv("RECOMP_SUPER_TRACE")) {
        char line[600];
        int m = 0;
        for (int i = 0; i < count && i < 8 && m < (int)sizeof line - 60; i++)
            m += snprintf(line + m, sizeof line - (size_t)m, " %d:%.20s(last %d)", i + 1, win_str(league[i] + 4),
                          (int8_t)g_mem[league[i] + CLUB_LAST_PLACE]);
        fprintf(stderr, "recomp: super trace: %s %d league order%s; cup %.20s, %.20s\n", file, season,
                m ? line : " none", cw ? win_str(cw + 4) : "-", cru ? win_str(cru + 4) : "-");
    }
    uint32_t out[4];
    if (!season_super_four_pick(league, count, cw, cru, out)) {
        fprintf(stderr, "recomp: super cups: %s %d: four clubs not found, two clubs play\n", file, season);
        cpu = saved;
        return;
    }
    put_list(cup, out, 4);
    if (rt_unwinding) return;
    fprintf(stderr, "recomp: super cups: %s %d with four clubs: %.30s v %.30s, %.30s v %.30s (%d league clubs)\n", file,
            season, win_str(out[0] + 4), win_str(out[1] + 4), win_str(out[2] + 4), win_str(out[3] + 4), count);
    cpu = saved;
}

static void set_dates(uint32_t t, int round, int draw_day, int draw_month, int draw_year, int day, int month,
                      int year_off, uint32_t year)
{
    uint32_t d[7] = {t, (uint32_t)round, (uint32_t)draw_day, (uint32_t)draw_month, (uint32_t)draw_year, 0xffffffffu,
                     year};
    call_cdecl(F_DRAW_DATE, 7, d);
    uint32_t m[9] = {t, (uint32_t)round, (uint32_t)day, (uint32_t)month, (uint32_t)year_off, 0xffffffffu, 1, year, 0};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m);
}

static void set_round(uint32_t r, int index, int code, int clubs, int fresh, uint16_t flags)
{
    wr16(g_mem, r + 7, (uint16_t)code);
    wr16(g_mem, r + 9, (uint16_t)index);
    wr16(g_mem, r + 0xb, 0);
    wr16(g_mem, r + 0xd, flags);
    wr16(g_mem, r + 0xf, 0);
    wr16(g_mem, r + 0x11, 0);
    g_mem[r + 0x17] = 6;                                /* as from the quarter-finals in every class */
    wr16(g_mem, r + 0x18, (uint16_t)clubs);
    wr16(g_mem, r + 0x1a, (uint16_t)(clubs / 2));
    wr16(g_mem, r + 0x1c, (uint16_t)fresh);
    wr16(g_mem, r + 0x1e, 0);
    g_mem[r + 0x20] = 0;                                /* no replay */
    g_mem[r + 0x21] = 1;                                /* one match */
    g_mem[r + 0x22] = 0;
}

/* After create_round_table of a super cup: self is the cup, stage and count_ptr its first two arguments, the
 * table of one round is the result (cpu.eax). */
static void rounds_made(uint32_t cup, uint32_t stage, uint32_t count_ptr)
{
    if (stage != 0xffu || !season_game() || !readable(cup)) return;
    const char *file = file_of(cup);
    const SeasonSuperFour *f = season_super_four(file, cup_season(cup));
    if ((!f || (int16_t)rd16(g_mem, cup + CUP_TEAMS) != 4) && !strncmp(file, "new_", 4)) {
        season_new_cups_rounds(cup, file, count_ptr);   /* a new cup of slice 21c: its rounds */
        return;
    }
    if ((int16_t)rd16(g_mem, cup + CUP_TEAMS) != 4) return;
    uint32_t one = cpu.eax;
    if (!f || !readable(one) || !readable(count_ptr)) return;
    Cpu saved = cpu;
    uint32_t t = game_malloc(2 * ROUND_SIZE);
    if (!t) return;
    memcpy(g_mem + t, g_mem + one, ROUND_SIZE);
    memcpy(g_mem + t + ROUND_SIZE, g_mem + one, ROUND_SIZE);
    game_free(one);
    if (rt_unwinding) return;
    set_round(t, 0, 130, 4, 4, (uint16_t)(0x200u | f->flags));     /* 0x82 "Semi Final" (name function 0x4b4850) */
    set_round(t + ROUND_SIZE, 1, 150, 2, 0, f->flags);              /* 0x96 "Final" */
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cup + CUP_YEAR);
    /* The draw of the semi-finals on the first day of the month before them, the final's draw the day after them. */
    int dm = f->sf_month == 0 ? 11 : f->sf_month - 1, dy = f->sf_month == 0 ? f->sf_year - 1 : f->sf_year;
    set_dates(t, 0, 1, dm, dy, f->sf_day, f->sf_month, f->sf_year, year);
    if (!rt_unwinding) set_dates(t, 1, f->sf_day + 1, f->sf_month, f->sf_year, f->f_day, f->f_month, f->f_year, year);
    if (rt_unwinding) return;
    cpu = saved;
    cpu.eax = t;
    wr16(g_mem, count_ptr, 2);
    fprintf(stderr, "recomp: super cups: %s %u/%02u: semi-finals and final\n", file, year, (year + 1) % 100);
}

void o_0065fa60(void);
void o_00858aa0(void);
void o_0065f920(void);
void o_00858960(void);

/* ItaSuper::init_team_list and SpaSuper::select_teams, thiscall () ret 0, returning 1 when the list holds its
 * clubs. */
RT_WRAP(0065fa60, super_ita_teams)
{
    uint32_t self = cpu.ecx;
    const char *file = season_game() && readable(self) ? file_of(self) : NULL;
    if (file && !strncmp(file, "new_", 4)) {
        /* A new cup of slice 21c (an ItaSuper object): the clubs of its nation in place of the class's picker,
         * which reads Italy's key ids (0x9cf570 at 0x65faee) and stops the game when Italy is not loaded. */
        Cpu saved = cpu;
        season_new_cups_teams(self, file);
        if (rt_unwinding) return;
        cpu = saved;
        cpu.eax = 1;
        teams_made(self);                               /* four clubs where the row gives them */
        if (rt_unwinding) return;
        rt_ret_stdcall(1, 0);
        return;
    }
    o_0065fa60();
    if (!rt_unwinding) teams_made(self);
}

RT_WRAP(00858aa0, super_spa_teams)
{
    uint32_t self = cpu.ecx;
    o_00858aa0();
    if (!rt_unwinding) teams_made(self);
}

/* ItaSuper and SpaSuper create_round_table, thiscall (char stage, short *rounds, short *first id, int *out) ret
 * 0x10, vtable +0x3c. */
RT_WRAP(0065f920, super_ita_rounds)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_0065f920();
    if (!rt_unwinding) rounds_made(self, stage, count);
}

RT_WRAP(00858960, super_spa_rounds)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_00858960();
    if (!rt_unwinding) rounds_made(self, stage, count);
}

/* Test log (O326, RECOMP_SUPER_TRACE=1): for the Supercopa, at each Cup::update_for_date 0x51f890 whose state
 * changed, the date, the cup head (+0x2c to +0x4c, +0xb1), the team list and each round's dates and counts. */
void season_super_cups_trace(uint32_t cup, uint32_t today)
{
    static int on = -1;
    static uint32_t last_sum;
    if (on < 0) on = getenv("RECOMP_SUPER_TRACE") && *getenv("RECOMP_SUPER_TRACE");
    if (!on || !readable(cup) || !readable(today) || strcmp(season_change_file(cup), "spa_super")) return;
    char line[1600];
    int n = snprintf(line, sizeof line, "head 2c=%d 30=%d 3c=%d 3e=%d 40=%d 44=%d 48=%08x 4c=%d b1=%d b2=%d",
                     (int32_t)rd32(g_mem, cup + 0x2c), (int32_t)rd32(g_mem, cup + 0x30),
                     (int16_t)rd16(g_mem, cup + 0x3c),
                     (int16_t)rd16(g_mem, cup + 0x3e), (int16_t)rd16(g_mem, cup + 0x40), (int8_t)g_mem[cup + 0x44],
                     rd32(g_mem, cup + 0x48), (int32_t)rd32(g_mem, cup + 0x4c), g_mem[cup + 0xb1], g_mem[cup + 0xb2]);
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST);
    for (int i = 0; i < 4 && readable(list); i++)
        n += snprintf(line + n, sizeof line - (size_t)n, " t%d=%08x/%d/%d", i, rd32(g_mem, list + 6u * (uint32_t)i),
                      g_mem[list + 6u * (uint32_t)i + 4], g_mem[list + 6u * (uint32_t)i + 5]);
    uint32_t t = rd32(g_mem, cup + 0xa3);
    int rounds = (int16_t)rd16(g_mem, cup + 0x3c);
    for (int r = 0; r < rounds && r < 4 && readable(t); r++) {
        uint32_t a = t + ROUND_SIZE * (uint32_t)r;
        n += snprintf(line + n, sizeof line - (size_t)n, " r%d draw %d+%d match %d+%d code %d ties %d new %d first %d",
                      r, (int16_t)rd16(g_mem, a), g_mem[a + 2], (int16_t)rd16(g_mem, a + 3), g_mem[a + 5],
                      (int16_t)rd16(g_mem, a + 7), (int16_t)rd16(g_mem, a + 0x1a), (int16_t)rd16(g_mem, a + 0x1c),
                      (int16_t)rd16(g_mem, a + 0x1e));
    }
    for (uint32_t o = 0x50; o < 0xa3 && n < (int)sizeof line - 12; o += 4)
        n += snprintf(line + n, sizeof line - (size_t)n, "%s%08x", o == 0x50 ? " body " : ".", rd32(g_mem, cup + o));
    uint32_t sum = 0;
    for (const char *c = line; *c; c++) sum = sum * 31u + (uint8_t)*c;
    if (sum == last_sum) return;
    last_sum = sum;
    fprintf(stderr, "recomp: super trace: day %08x %08x: %s\n", rd32(g_mem, today), rd32(g_mem, today + 4), line);
}

/* ---- The rules report (slice 21c) ----
 *
 * One line "cupmore" for each cup of this slice: season, phase, file, year (+0x40), clubs (+0x3e), rounds of its
 * round table (+0x3c), stage count (+0x2c), current stage (+0x30), the group stages (League_Stage, type byte 2 at
 * +0x43) and their clubs, the clubs and rounds of its knockout stage (Cup_Stage, type byte 1; +0x3e clubs, +0x3c
 * rounds), then the winners of the history rows of its record with the label years season and season + 1 (the
 * history table at [0xae23d4], 0x1a bytes a row, count [0xae237c]: +4 the record, +8 the label year, +0xa the
 * winner; club +4 the name), "-" for none. It only reads memory. */
#define COMP_TABLE 0xadadfcu
#define COMP_SLOTS 0xadadf8u

static int reported(const char *file)
{
    static const char *const files[] = {"ita_super", "spa_super", "sco_lge_cup", "eng_auto_cup", "tur_cup",
                                        "rus_cup", "eng_cc_cup"};
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++)
        if (!strcmp(files[i], file)) return 1;
    return !strncmp(file, "new_", 4);
}

static void winner_of(uint32_t rec, int label, char *out, size_t room)
{
    snprintf(out, room, "-");
    uint32_t rows = rd32(g_mem, 0xae23d4u);
    int32_t n = (int32_t)rd32(g_mem, 0xae237cu);
    for (int32_t i = 0; readable(rows) && readable(rec) && i < n && i < 200000; i++) {
        uint32_t row = rows + 0x1au * (uint32_t)i;
        if (rd32(g_mem, row + 4) != rec || (int16_t)rd16(g_mem, row + 8) != label) continue;
        uint32_t club = rd32(g_mem, row + 0xa);
        if (readable(club)) snprintf(out, room, "%.50s", win_str(club + 4));
    }
    for (char *p = out; *p; p++)
        if (*p == '\t' || *p == '\n') *p = ' ';
}

void season_cups_more_report(void *file, int season, const char *phase)
{
    FILE *f = (FILE *)file;
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int32_t slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    for (int32_t i = 0; f && readable(table) && i < slots && i < 100000; i++) {
        uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)i);
        if (!readable(obj)) continue;
        const char *name = season_change_file(obj);
        if (!reported(name)) continue;
        season_cup_groups_read_test(obj);       /* test only: RECOMP_CUP_GROUPS_READ=1 (season_cup_groups.c) */
        int stages = (int32_t)rd32(g_mem, obj + 0x2c), now = (int32_t)rd32(g_mem, obj + 0x30);
        uint32_t arr = rd32(g_mem, obj + 0xc);
        int group_clubs = 0, groups = 0, ko_clubs = 0, ko_rounds = 0;
        for (int s = 0; s <= now && s < stages && s < 64 && readable(arr); s++) {
            uint32_t st = rd32(g_mem, arr + 4u * (uint32_t)s);
            if (!readable(st)) continue;
            if (g_mem[st + 0x43] == 2) groups++, group_clubs += (int16_t)rd16(g_mem, st + 0x3e);
            else if (g_mem[st + 0x43] == 1)
                ko_clubs = (int16_t)rd16(g_mem, st + 0x3e), ko_rounds = (int16_t)rd16(g_mem, st + 0x3c);
        }
        char w0[64], w1[64];
        uint32_t rec = rd32(g_mem, obj + 4);
        winner_of(rec, season, w0, sizeof w0);
        winner_of(rec, season + 1, w1, sizeof w1);
        fprintf(f, "cupmore\t%d\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n", season, phase, name,
                (int16_t)rd16(g_mem, obj + CUP_YEAR), (int16_t)rd16(g_mem, obj + CUP_TEAMS),
                (int16_t)rd16(g_mem, obj + 0x3c), stages, now, groups, group_clubs, ko_clubs, ko_rounds, w0, w1);
    }
}
