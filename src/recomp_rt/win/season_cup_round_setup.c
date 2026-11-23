/* A cup round set up before it was drawn (0.6.2 open item 88).
 *
 * A Cup keeps two counters: +0xac the next round to draw (Cup::draw_round 0x51c410 moves it on at 0x51c6b2 for every
 * round but the third place, code 0x8c) and +0xab the next round to set up. Cup::set_up_round 0x51c800, thiscall
 * (round), draws the round of its argument when that round's first tie has state 0 (not drawn; 0x51c859..0x51c86c),
 * then sets up round +0xab (0x51c871): in one pass over its ties (0x51c900..0x51c95f) a tie of state 1 gets
 * create_tie_fixtures 0x51cb90 with defer (state 2) and then, at state 2, its fixture (state 3); a tie of state 0 gets
 * nothing. It then moves +0xab on, but not past a third place (0x51cae0..0x51caf3), and lifts +0xac to it
 * (0x51caf9..0x51cb09).
 *
 * So when the argument is not +0xab, round +0xab is set up whether it was drawn or not. Cup::update_for_date 0x51f890
 * sets up the third place on its own day (the last round, code 0x8c: 0x51f986..0x51f9ef and 0x51fab6..). In a base game
 * (2001, changing rules off, seed 2, Tier A; run-logs/0.6.2-s88) a group match of the 2008 Oceania Champions Cup moved
 * from 12 to 19 January, so the semi-final stage was made on 19 January, the day the final and the third place are
 * drawn. The set-up of the third place (round 2) then set up round +0xab = 1, the final, whose tie was not drawn (state
 * 0): the final got nothing, +0xab passed it, and no later call draws or sets it up. The cup never ended, so the 2010
 * edition never started (call trace: set_up_round(2) with +0xab 1 and the final's tie at state 0).
 *
 * Here, before the original, a round +0xab that is not the argument and whose first tie has state 0 is drawn first, as
 * set_up_round draws its argument. Its ties then go through the set-up as every drawn round's do; a tie with a club
 * still missing gets state 3 and its fixture when the club comes (Cup::process_fixture 0x51a150 at 0x51b67b for the
 * winner's tie, season_cup_third_place.c for the third place). In play where every round is drawn before its set-up,
 * round +0xab has a tie state above 0 and nothing changes. A fault of the original that long games reach: the fix acts
 * in every game, the base game too, and RECOMP_ORIGINAL_FAULTS=keep turns it off (original_faults.c). */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDS 0x3cu                    /* s16: the round count */
#define TIE_TABLE 0xadu                 /* the tie array of each round */
#define TIE_STATE 0x34u                 /* 0 not drawn, 1 drawn, 2 pending, 3 set up (0x51c633, 0x51cc56, 0x51cc79) */
#define SET_UP 0xabu                    /* s8: the next round to set up (0x51c871) */
#define DRAWN 0x45u                     /* 1 after the competition's draw (Comp::Comp 0x48c5bb, as season_kind_cup_breaks.c);
                                         * set_up_round returns at once while it is 0 (0x51c828..0x51c838) */
#define F_DRAW_ROUND 0x0051c410u        /* Cup::draw_round, thiscall (round) ret 4 */

static int readable(uint32_t p) { return rt_readable(p); }

static int fix_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_ORIGINAL_FAULTS");
        on = !(v && !strcmp(v, "keep"));
    }
    return on;
}

void o_0051c800(void);

RT_WRAP(0051c800, cup_set_up_round)
{
    uint32_t cup = cpu.ecx;
    int arg = (int)rt_arg(0);
    int rounds = readable(cup) ? (int16_t)rd16(g_mem, cup + ROUNDS) : 0, next = readable(cup) ? (int8_t)g_mem[cup + SET_UP] : -1;
    uint32_t all = readable(cup) ? rd32(g_mem, cup + TIE_TABLE) : 0;
    uint32_t ties = readable(all) && next >= 0 && next < rounds && rounds <= 64 ? rd32(g_mem, all + 4u * (uint32_t)next) : 0;
    if (fix_on() && readable(ties) && next != arg && arg >= 0 && arg < rounds && rd32(g_mem, cup + DRAWN) &&
        g_mem[ties + TIE_STATE] == 0) {
        Cpu saved = cpu;
        uint32_t a[1] = {(uint32_t)next};
        rt_callback(F_DRAW_ROUND, 1, a);
        if (rt_unwinding) return;
        cpu = saved;
        uint32_t record = rd32(g_mem, cup + 4);
        /* draw_round may wait instead (a club of a tie not known yet: the draw date a day on, 0x51c4e9) */
        int drawn = readable(rd32(g_mem, all + 4u * (uint32_t)next)) &&
                    g_mem[rd32(g_mem, all + 4u * (uint32_t)next) + TIE_STATE] != 0;
        fprintf(stderr, "recomp: cup: %.40s round %d %s before its set-up (set_up_round of round %d)\n",
                readable(record) ? win_str(record + 4) : "?", next, drawn ? "is drawn" : "waits for its draw", arg);
    }
    o_0051c800();
}
