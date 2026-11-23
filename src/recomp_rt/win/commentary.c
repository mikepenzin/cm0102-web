/* Faster commentary (#31): a sixth commentary speed, "Fastest", twice as fast as Very Fast.
 *
 * The speed is a percentage of each line's delay: Very Fast 20, Fast 55, Medium 90, Slow 125,
 * Very Slow 150. The human manager record keeps it (+0x9ad2, getter 0x5fcff0, setter 0x5fd100) and the
 * match screen build copies it into the match (byte +0x2db0, 0x719c4e). The line delay is
 * line_delay * speed / 100 (0x6d8bc7 in 0x6d7b00). The match input 0x71a240 stores the data of any item
 * with action id 0xc, so an item with data 10 needs no input code.
 * See docs/research/2026-10-15-commentary-speed-inventory.md.
 *
 * The runtime adds the item "Fastest" (data 10) above Very Fast in the speed popup. The original skips
 * an extra pause after special events only at Very Fast (0x6b5686 in the commentary step 0x6b55c0), so
 * during that step the match shows 20, and during a line's delay (0x6d7b00) 10 again.
 *
 * The speed changes how often other matches of the day step while a line waits. In the original
 * those steps take numbers from the human match, so its result depends on the speed. Since item 155
 * they take numbers from a second stream (match_numbers.c), and the speed, Fastest too, does not change it. */
#include "win.h"

#include <string.h>

extern const uint32_t rt_site_speed_very_fast;
extern const uint32_t rt_site_speed_copy;

#define WIDGET_CREATE 0x55f990u
#define SPEED_GET 0x5fcff0u          /* thiscall on [HUMANS], (manager or 0 for the current one) */
#define HUMANS 0xb63c98u
#define MATCH_SPEED 0x2db0u
#define FASTEST 10
#define VERY_FAST 20

static uint32_t match;                 /* the match of the last match screen build, 0: none */
static int in_step;                    /* 1 while the commentary step runs with Fastest shown as Very Fast */

/* The Very Fast item create: create the Fastest item first, with the same arguments except the text,
 * the data and the current-item style. */
void commentary_widget_before(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_speed_very_fast) return;
    uint32_t args[18];
    for (int i = 0; i < 18; i++) args[i] = rt_arg(i);
    static uint32_t text;
    if (!text) text = rt_alloc_str("  Fastest");
    Cpu saved = cpu;
    uint32_t zero[1] = {0};
    cpu.ecx = rd32(g_mem, HUMANS);
    int current = (int16_t)rt_callback(SPEED_GET, 1, zero) == FASTEST;
    cpu = saved;
    args[0] = current ? 0x2002 : 2;
    args[13] = text;
    args[16] = FASTEST;
    rt_callback(WIDGET_CREATE, 18, args);
    cpu = saved;
}

void o_005fcff0(void);
void o_006b55c0(void);
void o_006d7b00(void);

/* The speed getter. At the copy in the match screen build, esi is the match. */
RT_WRAP(005fcff0, speed_get)
{
    if (rd32(g_mem, cpu.esp) == rt_site_speed_copy) match = cpu.esi;
    o_005fcff0();
}

/* The commentary step, thiscall (human game record). */
RT_WRAP(006b55c0, commentary_step)
{
    win_game_work();                    /* a match runs: no idle sleep (user32.c, #58) */
    if (!match || g_mem[match + MATCH_SPEED] != FASTEST) {
        o_006b55c0();
        return;
    }
    g_mem[match + MATCH_SPEED] = VERY_FAST;
    in_step = 1;
    o_006b55c0();
    in_step = 0;
    if (g_mem[match + MATCH_SPEED] == VERY_FAST) g_mem[match + MATCH_SPEED] = FASTEST;
}

/* Shows a commentary line and sets its delay from the speed. */
RT_WRAP(006d7b00, commentary_line)
{
    if (!in_step || g_mem[match + MATCH_SPEED] != VERY_FAST) {
        o_006d7b00();
        return;
    }
    g_mem[match + MATCH_SPEED] = FASTEST;
    o_006d7b00();
    g_mem[match + MATCH_SPEED] = VERY_FAST;
}
