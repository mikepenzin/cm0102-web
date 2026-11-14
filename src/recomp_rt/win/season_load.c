/* Stage slots after a load (0.6.0 load sweep, docs/research/2026-10-21-load-sweep.md, O162).
 *
 * League::load 0x687b10 and Cup::load_from_disk 0x51fc00 read the stage count +0x2c and the current stage +0x30,
 * allocate the stage array +0xc with malloc when the constructor has not (League 0x688e3a..0x688e5b, Cup 0x520c8a)
 * and fill only the slots 0 to +0x30 (League 0x688ea1..0x689017). malloc does not clear, so the later slots hold
 * old heap bytes. The original never reads them before it builds them; the season code does (the qualifying
 * sections' stage_of in season_intl_qual.c and season_wcq26.c, the Euro qualifying play-off slot of O145). In a
 * new game those slots are a fresh array. So after a load that succeeds (eax 1), the slots after +0x30 are set
 * to 0, up to the size of the block, or up to +0x2c when the block is not one of the runtime's.
 *
 * Only in a season game (a start after 2001/02 or changing rules on): the base game keeps the original's bytes
 * (Q152). RECOMP_LOAD_CLEAR=off turns it off (test switch). */
#include "win.h"
#include "season.h"
#include "season_load.h"
#include "season_divisions.h"
#include "../rt_host.h"

#include <stdlib.h>
#include <string.h>

#define STAGES 0xcu                     /* Comp ** */
#define STAGE_COUNT 0x2cu               /* s32 */
#define STAGE_NOW 0x30u                 /* s32 */
#define MAX_SLOTS 64

static int readable(uint32_t p) { return rt_readable(p); }

static int season_game(void)
{
    const char *v = getenv("RECOMP_LOAD_CLEAR");
    if (v && !strcmp(v, "off")) return 0;
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

void season_load_clear_stages(uint32_t obj)
{
    if (!season_game() || !readable(obj)) return;
    uint32_t arr = rd32(g_mem, obj + STAGES);
    if (!readable(arr)) return;
    int32_t count = (int32_t)rd32(g_mem, obj + STAGE_COUNT), now = (int32_t)rd32(g_mem, obj + STAGE_NOW);
    int32_t slots = rt_is_block(arr) ? (int32_t)(rt_block_size(arr) / 4u) : count;
    if (slots > MAX_SLOTS) slots = MAX_SLOTS;
    if (now < -1) now = -1;
    for (int32_t i = now + 1; i < slots; i++) wr32(g_mem, arr + 4u * (uint32_t)i, 0);
}

/* League::load, thiscall (by_comp_id) ret 4, eax 1 when the file loads. */
void o_00687b10(void);
RT_WRAP(00687b10, league_load)
{
    uint32_t obj = cpu.ecx;
    o_00687b10();
    if (!rt_unwinding && cpu.eax) season_load_clear_stages(obj);
    if (!rt_unwinding && cpu.eax) season_divisions_loaded(obj);    /* the 3. Liga stage slot */
    if (!rt_unwinding && cpu.eax) season_club_world_loaded(obj);   /* the table of a Form K parent */
}
