/* The Veikkausliiga in the seasons with no relegation play-off (0.7.3 slice 4, open item 284). The rows of the real
 * seasons 2002, 2006 and 2011 to 2014 (docs/research/rules-0.6.0/nor-fin.tsv, rows nor-fin-7, 47 and 97 to 127) give
 * the division no play-off.
 *
 * The original plays one every season. FinPrm::set_up_rules 0x595c90 writes 1 relegation play-off place (+0xc0 at
 * 0x595ce7), and FinFirst's constructor 0x592eb0 sets 4 stages (+0x2c at 0x592f1a, 16 bytes of slots at 0x592f24).
 * FinFirst::advance_stage 0x593f90 builds the last stage when the stage counter +0x30 passes 2 (0x594045..0x594058):
 * FinFirst::create_premier_play_off_stage 0x594610 pairs the stage-1 finalist with the Premier Division's row of
 * status 2 (0x5946c4). With +0xc0 at 0 no row has status 2, so that stage must not be built: its second club slot
 * would stay unset (0x5946d1 jumps past the store at 0x5946e4).
 *
 * So in a season with no play-off the Premier Division gets +0xc0 0 and FinFirst 3 stages. League::update_for_date
 * calls advance_stage only while +0x30 is below +0x2c - 1 (0x6849d1..0x6849da), so it stops after stage 2. The winner
 * of stage 1 still goes up (status 5, 0x5948a3), and the size step brings the division to its row.
 *
 * The slots. FinFirst::start_new_season 0x593c20 frees the slots and allocates +0x2c x 4 bytes (0x593c8e..0x593cad)
 * before it calls set_up_rules (0x593cb9). After a season of 3 stages the block holds 3 slots, so the next season of 4
 * stages takes a new block here. No stage exists then (+0x30 is -1 at 0x593c81). League::load 0x687b10 allocates the
 * slots of the saved count, so a game saved in a season of 3 stages loads with its 3 slots.
 *
 * The base game, changing rules off and RECOMP_LEAGUE_SIZES=off change nothing: the count is lowered only with the
 * size engine and changing rules on. The
 * wrap raises a count of 3 in every game, because only this file writes 3: a game saved in such a season and continued
 * with the engine off gets its fourth stage back. */
#include "win.h"
#include "../rt_host.h"
#include "season.h"
#include "season_change.h"
#include "season_sizes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_STAGES 0x0cu           /* the stage slots */
#define COMP_STAGE_COUNT 0x2cu      /* s32 */
#define COMP_STAGE_NOW 0x30u        /* s32: -1 before the first stage */
#define COMP_YEAR 0x40u             /* s16: the edition */
#define DOWN_PLAYOFF 0xc0u          /* s8: relegation play-off places (0x595ce7) */
#define STAGES_PLAYOFF 4            /* the class's count (0x592f1a) */
#define STAGES_NONE 3               /* groups, promotion play-off, relegation play-off of the First Division */

static int readable(uint32_t p) { return rt_readable(p); }

/* 1 when the edition of the object plays no relegation play-off. Only with changing rules on: with them off the game
 * stays as 0.7.2 plays it (spec L2). While the game is made the row is the start season's, as row_now of season_sizes.c. */
static int no_playoff(uint32_t obj)
{
    if (!season_sizes_on() || !season_changing_rules()) return 0;
    int year = season_change_is_creating() ? season_start_year() : (int16_t)rd16(g_mem, obj + COMP_YEAR);
    return season_places_fin_no_playoff(year);
}

void season_sizes_fin_rules(uint32_t obj, const char *file)
{
    if (!readable(obj) || !file || strcmp(file, "fin_prm") || !no_playoff(obj)) return;
    g_mem[obj + DOWN_PLAYOFF] = 0;
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: fin_prm %d: no relegation play-off\n",
                (int16_t)rd16(g_mem, obj + COMP_YEAR));
}

/* FinFirst::set_up_rules (thiscall, no arguments; the constructor calls it at 0x592f31, start_new_season at 0x593cb9).
 * Only before the first stage: the rules of an edition, never a stage count under a live stage. */
void o_00593b80(void);
RT_WRAP(00593b80, fin_first_rules)
{
    uint32_t self = cpu.ecx;
    o_00593b80();
    if (rt_unwinding || !readable(self) || (int32_t)rd32(g_mem, self + COMP_STAGE_NOW) != -1) return;
    int32_t count = (int32_t)rd32(g_mem, self + COMP_STAGE_COUNT);
    int off = no_playoff(self);
    if (off && count == STAGES_PLAYOFF) {
        wr32(g_mem, self + COMP_STAGE_COUNT, STAGES_NONE);
    } else if (!off && count == STAGES_NONE) {
        uint32_t slots = season_sizes_game_malloc(4u * STAGES_PLAYOFF);
        if (rt_unwinding || !slots) return;
        uint32_t old = rd32(g_mem, self + COMP_STAGES);
        if (readable(old)) season_sizes_game_free(old);
        if (rt_unwinding) return;
        wr32(g_mem, self + COMP_STAGES, slots);
        wr32(g_mem, self + COMP_STAGE_COUNT, STAGES_PLAYOFF);
    } else {
        return;
    }
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: fin_first %d: %d stages\n", (int16_t)rd16(g_mem, self + COMP_YEAR),
                (int32_t)rd32(g_mem, self + COMP_STAGE_COUNT));
}
