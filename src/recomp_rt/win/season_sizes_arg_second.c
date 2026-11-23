/* The Argentine second division at its real size (season_sizes_arg_second.h). */
#include "win.h"
#include "season_sizes_arg_second.h"
#include "season_sizes_arg.h"
#include "season_sizes.h"
#include "season_league.h"
#include "season_rules.h"
#include "season_change.h"
#include "season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_SECOND "arg_second"
#define CLASS_CLUBS 25              /* ArgSecond::fill_league_table 0x407ce2 */
#define CLASS_STAGES 6              /* ArgSecond::ArgSecond 0x406c61 */
#define COMP_STAGES 0x0cu            /* the stage array (0x406c72) */
#define COMP_STAGE_COUNT 0x2cu
#define COMP_STAGE_NOW 0x30u
#define COMP_MEETINGS 0x3cu
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define COMP_UP 0xbeu
#define COMP_UP_PLAYOFF 0xbfu
#define COMP_DOWN_PLAYOFF 0xc0u
#define COMP_DOWN 0xc1u
#define DATE_SIZE 0x41u
#define F_SCHEDULE 0x00406f00u      /* ArgSecond::create_schedule, thiscall (stage, &count, &code, &flag) ret 0x10 */
#define F_PREDICTED 0x0068a850u     /* League::set_predicted_positions, thiscall, no argument: any count */
#define F_RELEGATE 0x006831a0u      /* League::relegate_club_to_division, thiscall (club, record, 1), ret 0xc */
#define KEY_SECOND 0x9cf658u        /* the record id of the second division (0x4067f9) */
#define COMP_OBJECTS 0xadadfcu      /* g_comp_object_table (0x4067f0) */
#define COMP_RECORD 4u
#define COMP_TABLE 0xb1u
#define CLUB_DIVISION 0x57u
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define STATUS_DOWN 3               /* the rows that the swap sends down (0x406a11) */

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

static int is_second(uint32_t obj) { return readable(obj) && !strcmp(season_change_file(obj), FILE_SECOND); }

/* ---- The form of an edition ---- */

typedef struct {
    int season;             /* the rules row */
    int meetings;           /* of the one table */
    const char *why;        /* the row's form, for the log */
} Form;

/* 1 when the edition plays one table (the form of the row); 0 for the class: the base game, the sizes off and a row of
 * the class's own form (25 clubs, an Apertura and the Clausura zones: 2001/02). An edition whose dates had no room
 * (season_sizes_kept, D4) still plays one table, at the class's count and meetings on the joined dates: the class's
 * stages need exactly 25 rows (create_zone_stages 0x4081b0 reads rows 1 to 24) and a stage array that a one-table save
 * does not hold (League::load 0x688e44 allocates it only for +0x2c > 0). */
static int form_of(uint32_t obj, Form *f)
{
    memset(f, 0, sizeof *f);
    if (!season_sizes_on() || !is_second(obj)) return 0;
    f->season = season_sizes_row_season(obj, FILE_SECOND);
    const SeasonRule *r = f->season >= 0 ? season_rules_find(f->season, FILE_SECOND, 0) : NULL;
    const SeasonLeagueRule *l = f->season >= 0 ? season_league_row(FILE_SECOND, f->season) : NULL;
    if (!r || !l) return 0;
    if (r->clubs == CLASS_CLUBS && l->halves == 1) return 0;
    int m = l->meetings >= 1 && l->meetings <= 4 ? l->meetings : 1;
    if (l->halves == 1) {
        /* Apertura and Clausura (2002/03 to 2006/07): one table of both halves, every pair meets in each half. */
        f->meetings = m * 2 <= 4 ? m * 2 : 4;
        f->why = "two halves as one table";
    } else if (l->zones > 1) {
        /* Zones (2019/20 on): with changing rules on two zones of two meetings in one table (item 291, season_arg_stages.c);
         * with them off one table of one meeting, the form of 0.7.2. */
        SeasonArgReducido rd;
        int zoned = season_changing_rules() && season_arg_reducido_of(f->season, &rd) && rd.zone_last;
        f->meetings = zoned ? 2 : 1;
        f->why = zoned ? "two zones of two meetings" : "zones as one table of one meeting";
    } else {
        f->meetings = m;
        f->why = "one table";
    }
    return 1;
}

int season_arg_second_one_table(uint32_t obj)
{
    Form f;
    return form_of(obj, &f);
}

/* One table now: the row's form and the object's (no stages: +0x2c 0, set by season_arg_second_rules). A save of an
 * edition of the class keeps the class's stages to its end. */
static int playing_one_table(uint32_t obj)
{
    return season_arg_reducido_marked(obj) ||
           (season_arg_second_one_table(obj) && rd32(g_mem, obj + COMP_STAGE_COUNT) == 0);
}

int season_arg_second_row_down(int row)
{
    const SeasonRule *r = season_rules_find(row, FILE_SECOND, 0);
    return r ? r->down : -1;
}

int season_arg_second_row_up(int row)
{
    const SeasonRule *r = season_rules_find(row, FILE_SECOND, 0);
    return r ? r->up : -1;
}

int season_arg_second_meetings(uint32_t obj, int meetings)
{
    Form f;
    return form_of(obj, &f) ? f.meetings : meetings;
}

/* ---- The places and the stages ---- */

void season_arg_second_rules(uint32_t obj)
{
    if (!is_second(obj)) return;
    Form f;
    if (!form_of(obj, &f)) {
        /* The class's own stages after an edition of one table: a 2001 edition with changing rules on, or a one-table
         * save loaded with the sizes off. A loaded one-table save has no stage array (0x688e44). The base game never
         * has +0x2c 0 (the constructor sets 6 at 0x406c61), so it allocates nothing here. */
        if (rd32(g_mem, obj + COMP_STAGE_COUNT) == 0) {
            if (!readable(rd32(g_mem, obj + COMP_STAGES))) {
                uint32_t a = season_sizes_game_malloc(4u * CLASS_STAGES);   /* 0x18 bytes, as 0x406c55..0x406c72 */
                if (rt_unwinding || !a) return;
                memset(g_mem + a, 0, 4u * CLASS_STAGES);
                wr32(g_mem, obj + COMP_STAGES, a);
            }
            wr32(g_mem, obj + COMP_STAGE_COUNT, CLASS_STAGES);
        }
        return;
    }
    const SeasonRule *r = season_rules_find(f.season, FILE_SECOND, 0);
    /* No stages: the table is the season (League::update_for_date 0x6847c0 calls advance_stage only while
     * +0x30 < +0x2c - 1, 0x6849d1..0x6849e0), so the Clausura zones of 24 fixed rows (create_zone_stages 0x4081b0,
     * 0x408233..0x40838a), the Reducido of 8 (0x408630) and the promotion ties (0x408910) are not played. The
     * promotion places of the row go to the next clubs of the table (D2, D10); sizes_rules set them. The clubs of the
     * relegation places go down to the third division through the Primera's swap (0x4068d0, at most 4 at 0x4069fb) and,
     * with changing rules on, arg_swap below: the class relegates by its averages in the Reducido's stage (0x408630),
     * which is not played. */
    wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
    wr32(g_mem, obj + COMP_STAGE_NOW, 0xffffffffu);
    int down = season_arg_second_down_places(r ? r->down : -1, season_changing_rules());
    g_mem[obj + COMP_UP_PLAYOFF] = 0;
    g_mem[obj + COMP_DOWN_PLAYOFF] = 0;
    g_mem[obj + COMP_DOWN] = (uint8_t)down;
    season_arg_reducido_rules(obj);     /* item 291: its ties are the stages of the edition */
    if (rt_unwinding) return;
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_second %d: row %d, %s, %d meetings, %d up, %d down, no stages\n",
                (int16_t)rd16(g_mem, obj + COMP_YEAR), f.season, f.why, f.meetings, (int8_t)g_mem[obj + COMP_UP], down);
}

/* tools/recomp/arg_second.py: ArgSecond::fill_league_table 0x407cd0 adds the fixed club of [0x9d020c] when it found
 * 24 (cmp bl, 0x18 at 0x407dc0), and then the table has 25. One table of the row has no fixed club. */
uint32_t rt_arg_second_fixed_at(uint32_t obj, uint32_t original)
{
    return playing_one_table(obj) ? 0xffu : original;
}

/* ---- The places past the swap (item 286) ---- */

/* arg_prm_swap_second_and_third_division_clubs 0x4068d0 (thiscall, no argument; ArgPrm::start_new_season calls it at
 * 0x40573f) sends at most 4 rows of status 3 down (cmp bl, 4 at 0x4069fb) and brings as many clubs up. The row of
 * 2017/18 has 6 places. With changing rules on the other rows of status 3 go down as the swap sends its own
 * (0x406a1c..0x406a38), and the size step, which runs at the free_table of 0x405789, fills the division to its next
 * row with other clubs of the third division. No club that went down this season change comes back in that step. A
 * third division with no club to exchange makes the swap move nobody (0x4069c9..0x4069e1): then no club goes down
 * here either, because no club could take its place. */
void o_004068d0(void);
RT_WRAP(004068d0, arg_swap)
{
    uint32_t self = cpu.ecx;
    o_004068d0();
    if (rt_unwinding || !season_sizes_on() || !season_changing_rules()) return;
    uint32_t objects = rd32(g_mem, COMP_OBJECTS), third = season_arg_third_record();
    int32_t key = (int32_t)rd32(g_mem, KEY_SECOND);
    uint32_t second = readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0;
    if (!third || !is_second(second) || !playing_one_table(second)) return;
    uint32_t rows = rd32(g_mem, second + COMP_TABLE), record = rd32(g_mem, second + COMP_RECORD);
    int n = (int16_t)rd16(g_mem, second + COMP_CLUBS), swapped = 0;
    for (int r = 0; readable(rows) && r < n; r++) {
        uint32_t row = rows + (uint32_t)r * ROW_SIZE, club = rd32(g_mem, row);
        swapped += g_mem[row + ROW_STATUS] == STATUS_DOWN && readable(club) &&
                   rd32(g_mem, club + CLUB_DIVISION) != record;
    }
    if (!swapped) return;
    for (int r = 0; readable(rows) && r < n; r++) {
        uint32_t row = rows + (uint32_t)r * ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ROW_STATUS] != STATUS_DOWN || !readable(club)) continue;
        season_sizes_step_bar(club);
        if (rd32(g_mem, club + CLUB_DIVISION) != record) continue;
        uint32_t a[3] = {club, third, 1};
        call_this(F_RELEGATE, self, 3, a);
        if (rt_unwinding) return;
        if (logging())
            fprintf(stderr, "recomp: league sizes: arg_second %d: club %u goes down past the swap's %d\n",
                    (int16_t)rd16(g_mem, second + COMP_YEAR), rd32(g_mem, club), SEASON_ARG_SWAP_DOWN);
    }
}

/* ---- The dates ---- */

/* Before the engine plans the template of one table (season_sizes_schedule): the class's dates of the Apertura
 * (create_schedule(-1), 25 dates from D at 0x406f36) and of the Clausura zones (create_schedule(0), 14 dates, D at
 * 0x407530), so that one table of 20 to 38 clubs has the dates of the whole season. create_schedule reads only the year
 * (+0x40) and writes only through its arguments. The template in eax, its count at count_at. Returns 1 when joined. */
int season_arg_second_join(uint32_t self, uint32_t count_at)
{
    if (!season_arg_second_one_table(self) || !readable(count_at)) return 0;
    uint32_t old = cpu.eax;
    int have = (int16_t)rd16(g_mem, count_at);
    if (!readable(old) || have < 1) return 0;
    uint32_t scratch = season_sizes_game_malloc(16);
    if (!scratch) return 0;
    uint32_t a[4] = {0, scratch, scratch + 4, scratch + 8};
    uint32_t second = call_this(F_SCHEDULE, self, 4, a);
    if (rt_unwinding) return 0;
    int have2 = (int16_t)rd16(g_mem, scratch);
    season_sizes_game_free(scratch);
    if (!readable(second) || have2 < 1) {
        season_sizes_game_free(second);
        return 0;
    }
    uint32_t both = season_sizes_game_malloc((uint32_t)(have + have2) * DATE_SIZE);
    if (!both) {
        season_sizes_game_free(second);
        return 0;
    }
    memcpy(g_mem + both, g_mem + old, (size_t)have * DATE_SIZE);
    memcpy(g_mem + both + (uint32_t)have * DATE_SIZE, g_mem + second, (size_t)have2 * DATE_SIZE);
    season_sizes_game_free(second);
    season_sizes_game_free(old);
    wr16(g_mem, count_at, (uint16_t)(have + have2));
    cpu.eax = both;
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_second %d: the dates of the Apertura (%d) and the Clausura (%d)\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), have, have2);
    return 1;
}

/* ---- The functions of 25 clubs ---- */

/* ArgSecond::init_relegation_averages 0x4093f0 (thiscall, no argument; the constructor at 0x406df0): it takes a block
 * of the count's records (+0x3e at 0x4093fe, calloc 0x945501 at 0x40940a) and writes 25 records of 2001 clubs, the
 * last at 0x409adf. One table of any count (25 too: 2017/18 and 2018/19) gets the empty records of the original's averages (id -1, the edition),
 * as season_sizes_arg.c gives the Primera; update_relegation_averages 0x409c20 fills a free record for each club. */
void o_004093f0(void);
RT_WRAP(004093f0, arg_second_init_averages)
{
    uint32_t self = cpu.ecx;
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    if (!playing_one_table(self) || n < 1) {
        o_004093f0();
        return;
    }
    season_arg_averages_empty(self, n);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}

/* ArgSecond::update_relegation_averages 0x409c20 (thiscall (new_season), ret 4): the records take the clubs of the
 * division first, when the count of the division changed since they were made (+0x34, 0x409c39). */
void o_00409c20(void);
RT_WRAP(00409c20, arg_second_update_averages)
{
    uint32_t self = cpu.ecx;
    if (playing_one_table(self)) {
        int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
        if (n >= 2 && n <= SEASON_ARG_MAX_CLUBS) season_arg_averages_fit(self, n, FILE_SECOND);
        if (rt_unwinding) return;
    }
    /* The class's loop reads each stage after the first as a league table (0x409d31..0x409d80). The ties of a Reducido are
     * Cup_Stages and hold no table: the averages come from the league's own table. */
    int reducido = season_arg_reducido_marked(self);
    uint32_t now = rd32(g_mem, self + COMP_STAGE_NOW);
    if (reducido) wr32(g_mem, self + COMP_STAGE_NOW, 0xffffffffu);
    o_00409c20();
    if (reducido) wr32(g_mem, self + COMP_STAGE_NOW, now);
}

/* ArgSecond::set_predicted_positions 0x409fe0 (thiscall, no argument): a block of 25 pointers (push 0x64 at 0x40a022),
 * count pointers copied into it (0x40a098..0x40a0c3), 25 sorted (push 0x19 at 0x40a0c5) and 25 fixed slots read
 * (0x40a0cd..). One table of another count takes the base League's positions of any count (0x68a850: a block of
 * +0x3e pointers, 0x68a892..0x68a89a). */
void o_00409fe0(void);
RT_WRAP(00409fe0, arg_second_predicted)
{
    uint32_t self = cpu.ecx;
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    if (!playing_one_table(self) || n == CLASS_CLUBS) {
        o_00409fe0();
        return;
    }
    call_this(F_PREDICTED, self, 0, NULL);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}

/* ArgSecond::record_season_honours 0x408eb0 (thiscall, no argument; also the Brazilian classes', whose wrap is in
 * season_sizes_bra.c): a jump to the honours of stage 0 (vtable +0x30 of [+0xc][0], 0x408eb0..0x408eb7), the Clausura
 * zone. One table has no stage: the wrap takes the honours of the table itself (League::record_season_honours
 * 0x684640). */
int season_arg_second_no_stage(uint32_t obj)
{
    /* Not by the sizes: a one-table save loaded with the sizes off ends its season with no stage too. The base game
     * never has +0x2c 0. */
    if (season_arg_reducido_marked(obj)) return 1;      /* the stages are the Reducido's ties: the table is the season */
    return is_second(obj) && rd32(g_mem, obj + COMP_STAGE_COUNT) == 0 &&
           (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) < 0;
}

/* ArgSecond::advance_stage 0x4080c0 (thiscall, no argument) makes the class's next stage; League::update_for_date 0x6847c0
 * calls it while a stage is to come and the stages made are complete (0x6849d1..0x6849e3). */
void o_004080c0(void);
RT_WRAP(004080c0, arg_second_advance)
{
    uint32_t self = cpu.ecx;
    if (!season_arg_reducido_marked(self)) {
        o_004080c0();
        return;
    }
    season_arg_reducido_advance(self);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

/* ArgSecond's result handler 0x408cd0 (thiscall (club, result, stage, ...), ret 0x18): stage 3 is the class's Reducido,
 * whose winner gets status 0 in the table (0x408d54) and so goes up in the Primera's chain (0x406839). The last tie of an
 * edition's Reducido is that stage; the earlier ties only send their winner to the next round. */
void o_00408cd0(void);
RT_WRAP(00408cd0, arg_second_result)
{
    uint32_t self = cpu.ecx;
    int stage = (int8_t)(rt_arg(2) & 0xff);
    if (!season_arg_reducido_marked(self) || stage < 0) {
        o_00408cd0();
        return;
    }
    /* Only a winner that goes up reaches the class. The loser: the class records a place in its promotion ties
     * (0x408d75..0x408d94), which this edition does not play. The earlier ties only send their winner on. */
    if (!season_arg_reducido_result(self, stage, (int8_t)(rt_arg(1) & 0xff), rt_arg(0))) {
        if (rt_unwinding) return;
        rt_ret_stdcall(0xffffffffu, 6);         /* ret 0x18, as the class's own stages, 0x408d6e */
        return;
    }
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_second reducido %d: result 1 of club %u in the final (stage %d)\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), rd32(g_mem, rt_arg(0)), stage);
    wr32(g_mem, cpu.esp + 4 + 4 * 2, 3);
    o_00408cd0();
}
