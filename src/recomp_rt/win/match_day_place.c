/* A weather postponement keeps the match day's place in its fixture list (0.6.2 item 84, tools/recomp/match_day.py).
 *
 * MATCH_DAY::MATCH_DAY 0x6b3c10 collects the matches of one date and slice. For each competition group it takes the
 * fixture list of that day and slice from the fixture store (0x5ae6a0 calls 0x5abf10, which returns the store's own
 * LLIST, store[day * 3 + slice][group], 0x5abfd7..0x5abfda) and walks it with the list's cursor: head +2 into the
 * cursor +0xa at 0x6b3ce3..0x6b3ce8, then LLIST::get_next 0x68d1b0 (it returns the node's fixture and moves the cursor
 * to node +6) at 0x6b3ced and 0x6b3e85. A match whose weather check 0x923630 (0x6b3d20) says postponed is copied, the
 * original marked -2 (0x6b3d43), its copy given a new date by find_new_fixture_date 0x5ab510 (0x6b3d5f) and added
 * with add_fixture 0x5aa680 (0x6b3d71). Both look for clashes with find_fixture_clashes 0x5ad780, whose day scan
 * 0x5adba0 takes the store's list of each day it checks from 0x5abf10 (0x5ade53) and walks it with the same cursor
 * (0x5ade64..0x5ade6c); a scan that reaches the end leaves it at 0, and one that stops early leaves it part way. When
 * it checks the postponed match's own day and slice, the cursor of the list that MATCH_DAY walks moves, so the next
 * call of 0x68d1b0 returns 0 (or a later match) and the rest of the list is never played.
 * Nothing plays a past fixture again, so a league with such a match never completes (League::check_complete 0x689ad0:
 * every row has played (clubs - 1) x rounds) and never starts its next season. In the Tier A game a2021s1 the
 * weather postponed SC Kufu-98 v SJK on 18 October 2046 (day 291), the next match of that list, HJK v JIPPO, was
 * skipped, and the Veikkausliiga and Ykkösliiga (one league table entry) held edition 2046 for ever (item 84). Of
 * 107 postponements of 2041 to 2046 in that game, one lost the place.
 *
 * Here the place of the list is kept before the copy (0x6b3d2d) and set back after its add (0x6b3d76), when the
 * kept node is still in the list. The cursor goes in the date search: 0x5ab510 checks its candidate days with
 * find_fixture_clashes from two days before to two days after (0x5adba0 per day), and a candidate is today + 2 or
 * later (0x5ab930), so the match's own day is among the days it scans. add_fixture's moves remove fixtures only of the
 * days it checks around the new date (0x5abc30 at 0x5aa9e0 and 0x5aaa15), from today + 1 on, never today's. The walk
 * from the head still checks the node before it is set back. When the place did not change, nothing is written: the
 * game is the original's. The test switch RECOMP_MATCH_DAY_PLACE=off keeps the original's place. */
#include "win.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_HEAD 0x2u           /* LLIST: count (word) +0, head +2, tail +6, cursor +0xa; node: data +0, next +6 */
#define LIST_CURSOR 0xau
#define NODE_NEXT 0x6u
#define TODAY 0x00ae2c90u        /* Date: day of the year +0, year +2 */

static uint32_t kept_list, kept_node;

static int readable(uint32_t p) { return rt_readable(p); }

/* Test switch RECOMP_MATCH_DAY_PLACE=off (read once): the original's place, so a test can show the fault. */
static int fix_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_MATCH_DAY_PLACE");
        on = !(v && !strcmp(v, "off"));
    }
    return on;
}

/* 0x6b3d2d, list at the caller's esp + 0x18 (0x6b3ccf). */
void rt_match_day_keep_place(uint32_t list)
{
    kept_list = fix_on() && readable(list) ? list : 0;
    kept_node = kept_list ? rd32(g_mem, list + LIST_CURSOR) : 0;
}

/* 0x6b3d76, after the add of the postponed match's copy. */
void rt_match_day_set_place(uint32_t list)
{
    if (!kept_list || list != kept_list) return;
    kept_list = 0;
    uint32_t now = rd32(g_mem, list + LIST_CURSOR);
    if (now == kept_node) return;
    if (kept_node) {
        uint32_t n = rd32(g_mem, list + LIST_HEAD);
        for (int i = 0; readable(n) && n != kept_node && i < 100000; i++) n = rd32(g_mem, n + NODE_NEXT);
        if (n != kept_node) {
            fprintf(stderr, "recomp: match day: the place in a fixture list after a weather postponement is gone\n");
            return;
        }
    }
    wr32(g_mem, list + LIST_CURSOR, kept_node);
    fprintf(stderr, "recomp: match day: a weather postponement on day %d of %d moved the place in its fixture list, "
                    "set back\n", (int16_t)rd16(g_mem, TODAY), (int16_t)rd16(g_mem, TODAY + 2u));
}

/* Test switch RECOMP_TEST_POSTPONE=<home club id>,<away club id> (tests/seasons/match_day_place_run.py, off by default,
 * read once): Weather::check_postponement 0x923630 (thiscall (fixture) ret 4; MATCH_DAY tests bit 0 of al at 0x6b3d25)
 * postpones that match once (al 5), when MATCH_DAY asks (return address 0x6b3d25). The weather's own rolls (0x90cfc0)
 * do not
 * come back the same after a load, so the test names the match that the run postponed. */
void o_00923630(void);

RT_WRAP(00923630, weather_check)
{
    static int home = -2, away = -2, done;
    if (home == -2) {
        const char *v = getenv("RECOMP_TEST_POSTPONE");
        home = away = -1;
        if (v && sscanf(v, "%d,%d", &home, &away) == 2)
            fprintf(stderr, "recomp: test: the weather postpones club %d v club %d once\n", home, away);
    }
    uint32_t fix = rt_arg(0);
    if (home >= 0 && !done && rd32(g_mem, cpu.esp) == 0x006b3d25u && readable(fix)) {
        uint32_t h = rd32(g_mem, fix + 0x1cu), a = rd32(g_mem, fix + 0x20u);
        if (readable(h) && readable(a) && (int32_t)rd32(g_mem, h) == home && (int32_t)rd32(g_mem, a) == away) {
            done = 1;
            fprintf(stderr, "recomp: test: the weather postpones club %d v club %d on day %d\n", home, away,
                    (int16_t)rd16(g_mem, TODAY));
            rt_ret_stdcall(5, 1);       /* 5: a result of the original with the postponed bit (0, 5, 9 or 0x12) */
            return;
        }
    }
    o_00923630();
}
