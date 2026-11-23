/* International caps and goals above 255 in the game (0.6.2 slice 12 and item 66, caps_wide.h,
 * tools/recomp/caps_wide.py). */
#include "win.h"
#include "caps_wide.h"
#include "../rt.h"

#include <stdio.h>

#define STAFF_ID 0x00u          /* staff +0: the staff id */
#define STAFF_CAPS 0x22u        /* staff +0x22: the caps byte */
#define STAFF_GOALS 0x23u       /* staff +0x23: the international goals byte */

/* 0x76b8ce inc al, with the caps byte of the staff in esi in al (0x76b8c8): the new byte. */
uint32_t rt_caps_add(uint32_t staff, uint32_t byte)
{
    uint32_t id = rd32(g_mem, staff + STAFF_ID), next = caps_wide_add(id, byte);
    if ((byte & 0xffu) == CAPS_BYTE_MAX)
        fprintf(stderr, "recomp: caps: staff %u has %u caps\n", id, caps_wide_full(id, CAPS_BYTE_MAX));
    return next;
}

/* The full caps of the staff record at staff. */
uint32_t rt_caps_full(uint32_t staff)
{
    return caps_wide_full(rd32(g_mem, staff + STAFF_ID), rd8(g_mem, staff + STAFF_CAPS));
}

/* 0x76b8d6 add dl, cl, with the goals byte of the staff in esi in dl (0x76b8cb) and his goals of the match in cl
 * (0x76b8d3): the new byte (item 66). */
uint32_t rt_goals_add(uint32_t staff, uint32_t byte, uint32_t goals)
{
    uint32_t id = rd32(g_mem, staff + STAFF_ID), next = goals_wide_add(id, byte, goals);
    if ((byte & 0xffu) + (goals & 0xffu) > CAPS_BYTE_MAX)
        fprintf(stderr, "recomp: goals: staff %u has %u international goals\n", id, goals_wide_full(id, CAPS_BYTE_MAX));
    return next;
}

/* The full international goals of the staff record at staff. */
uint32_t rt_goals_full(uint32_t staff)
{
    return goals_wide_full(rd32(g_mem, staff + STAFF_ID), rd8(g_mem, staff + STAFF_GOALS));
}

/* The full caps for a signed word (the most caps record of a national team, club record +0x2f9): at most 32767. */
uint32_t rt_caps_word(uint32_t staff)
{
    uint32_t caps = rt_caps_full(staff);
    return caps > 0x7fffu ? 0x7fffu : caps;
}

void o_0086cc50(void);

/* `Staff_Records::add_first_cap_record` 0x86cc50, thiscall (player, unused): a log line for each first-cap history
 * row (tests/seasons/caps_wide_run.py). Its one direct caller is 0x76b8c3, for a player whose caps byte is 0. */
RT_WRAP(0086cc50, first_cap_record)
{
    uint32_t staff = rt_arg(0);
    if (staff) fprintf(stderr, "recomp: caps: first cap of staff %u\n", rd32(g_mem, staff + STAFF_ID));
    o_0086cc50();
}
