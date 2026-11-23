/* The third-place tie whose loser comes late (0.6.2 open item 73). See season_cup_third_place.h. */
#include "win.h"
#include "season_cup_third_place.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDS 0x3cu                    /* s16: the round count */
#define ROUND_TABLE 0xa3u               /* the rounds, 0x68 bytes each */
#define ROUND_SIZE 0x68u
#define ROUND_CODE 7u                   /* s16: 0x8c the third place (0x51c6a9, 0x51caea) */
#define ROUND_TIES 0x1au                /* s16: the ties of the round */
#define TIE_TABLE 0xadu                 /* the tie array of each round */
#define TIE_SIZE 0x35u
#define TIE_STATE 0x34u                 /* 0 not drawn, 1 drawn, 2 pending, 3 set up (0x51c633, 0x51cc56, 0x51cc79) */
#define CODE_THIRD 0x8c
#define STATE_SET_UP 3
#define F_SET_HOME 0x0051e870u              /* Cup::set_home_teams_for_round, thiscall (short round, short tie) ret 8 */
#define F_CREATE_TIE_FIXTURES 0x0051cb90u   /* Cup::create_tie_fixtures, thiscall (round, tie, defer) ret 0xc */

static int readable(uint32_t p) { return rt_readable(p); }

/* The ties of the cup's last round when it is the third place, or 0 (and *ties 0). */
static uint32_t third_ties(uint32_t cup, int *last, int *n)
{
    *last = -1, *n = 0;
    uint32_t t = readable(cup) ? rd32(g_mem, cup + ROUND_TABLE) : 0, all = readable(cup) ? rd32(g_mem, cup + TIE_TABLE) : 0;
    int rounds = readable(cup) ? (int16_t)rd16(g_mem, cup + ROUNDS) : 0;
    if (!readable(t) || !readable(all) || rounds < 1 || rounds > 64) return 0;
    uint32_t r = t + ROUND_SIZE * (uint32_t)(rounds - 1);
    if ((int16_t)rd16(g_mem, r + ROUND_CODE) != CODE_THIRD) return 0;
    uint32_t ties = rd32(g_mem, all + 4u * (uint32_t)(rounds - 1));
    int count = (int16_t)rd16(g_mem, r + ROUND_TIES);
    if (!readable(ties) || count < 1) return 0;
    *last = rounds - 1;
    *n = count < THIRD_PLACE_TIES ? count : THIRD_PLACE_TIES;
    return ties;
}

/* A fault of the original that long games reach, so the fix acts in every game, the base game too, and the test
 * switch RECOMP_ORIGINAL_FAULTS=keep turns it off (original_faults.c). */
static int fix_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_ORIGINAL_FAULTS");
        on = !(v && !strcmp(v, "keep"));
    }
    return on;
}

void season_cup_third_place_before(uint32_t cup, ThirdPlaceTies *s)
{
    s->cup = cup;
    s->round = -1, s->n = 0;
    if (!fix_on()) return;
    uint32_t ties = third_ties(cup, &s->round, &s->n);
    for (int k = 0; k < s->n; k++) {
        uint32_t e = ties + TIE_SIZE * (uint32_t)k;
        /* Set up with a club still missing: create_tie_fixtures 0x51cb90 made no fixture for it
         * (0x51cc8c..0x51cc9b, only the news of 0x522950). */
        s->waiting[k] = g_mem[e + TIE_STATE] == STATE_SET_UP &&
                        ((int16_t)rd16(g_mem, e) == -1 || (int16_t)rd16(g_mem, e + 2) == -1);
    }
}

void season_cup_third_place_after(const ThirdPlaceTies *s)
{
    int round, n;
    uint32_t ties = third_ties(s->cup, &round, &n);
    if (!ties || round != s->round) return;
    for (int k = 0; k < n && k < s->n && !rt_unwinding; k++) {
        uint32_t e = ties + TIE_SIZE * (uint32_t)k;
        if (!s->waiting[k] || g_mem[e + TIE_STATE] != STATE_SET_UP || (int16_t)rd16(g_mem, e) == -1 ||
            (int16_t)rd16(g_mem, e + 2) == -1)
            continue;
        /* As the winner's late tie (0x51b67b..0x51b6a8): the home side of the tie by the round's rule, set at the
         * draw while a club was missing (0x51c5fb). Then the fixture that the round's set-up would have made: the
         * leg on the round's match date, or tomorrow when that date has passed (0x51cb90), clashes moved by
         * FIXTURE_MANAGER::add_fixture 0x5aa680. */
        Cpu saved = cpu;
        cpu.ecx = s->cup;
        uint32_t h[2] = {(uint32_t)round, (uint32_t)k};
        rt_callback(F_SET_HOME, 2, h);
        if (rt_unwinding) return;
        cpu = saved;
        cpu.ecx = s->cup;
        uint32_t a[3] = {(uint32_t)round, (uint32_t)k, 0};
        rt_callback(F_CREATE_TIE_FIXTURES, 3, a);
        if (rt_unwinding) return;
        cpu = saved;
        uint32_t record = rd32(g_mem, s->cup + 4);
        fprintf(stderr, "recomp: cup: %.40s third place tie %d: its clubs came after its set-up, fixture made\n",
                readable(record) ? win_str(record + 4) : "?", k);
    }
}
