/* The pick's rotation keeps enough players for the match it picks for (season games only).
 *
 * MATCH_MAN::pick_team 0x6dbc50 rotates an AI club of reputation 6000 or more (club +0x80, 0x6dbdc7) with more than
 * 30 players (0x53c590, 0x6dbe0c): it looks for the club's next fixture in the 7 days after the match
 * (FIXTURES search 0x5acbe0, called at 0x6dbe7e) and weighs the two matches (0x6dbe8c..0x6dc518). When the next match
 * counts more, it sets the pick's mode +0x1a to 0, 1 or 2 (0x6dc544, 0x6dc572, 0x6dc593). Its first loop
 * (0x6dc6b9..0x6dc88a) takes the players who can play the next match (bans 0x55d270 and eligibility 0x8cf5e0 with the
 * next fixture, 0x6dc74e and 0x6dc75e), and 0x6e2000 keeps the best 14 of them back. The second loop (0x6dc9f1) picks
 * the match from the rest: a player kept back is skipped (0x6dcae7), and the current match's eligibility comes only
 * there (0x8cf5e0 with the current fixture, 0x6dcbe6). Nothing checks that the rest can field 11.
 *
 * In gate (a) run 8 (2005 start, September 2005 database, on ec579543), AJ Auxerre (45 players, 25 registered for the
 * UEFA Cup) played its UEFA Cup group match at Siroki Brijeg on day 348 of 2005, with a Coupe de France tie on day 350.
 * The pick took mode 0 and kept 14 registered players back for the cup tie. Of the rest, 7 were registered (code 0 of
 * EUROPE_RULES::get_ineligibility_code 0x5832e0) and 24 were not (code 8). With 7 players, init_match 0x6b7f20 stopped
 * at match_eng.cpp 652 (0x6b8aca).
 *
 * The original keeps back at most 14 players who can play the current match too. So in a season game, when fewer than
 * 25 squad players can play the current match (not injured, 0x62c400 not 2, and eligible, 0x8cf5e0 code 0), the pick
 * finds no next fixture and does not rotate: the search at 0x6dbe7e returns 0 fixtures. The base game keeps the
 * original (Q152).
 *
 * The reserve side (open item 237). For a club's reserve side (pick +0x19 not 0) pick_team skips the rotation test
 * (0x6dbdbe) and keeps back players for the first team by the pick's mode +0x1a, which its caller sets
 * (0x6dc59f..0x6dc6a4): 14 for mode 0, 7 for 1, 2 for 2, none for 3. Its first loop (0x6dc6b9..0x6dc87c) takes the
 * players who can play (not injured, 0x62c400 at 0x6dc6f4; no ban, 0x55d270 at 0x6dc71f; no national duty, 0x7757d0
 * at 0x6dc7b1 and 0x6dc7d0; not away on loan, 0x4cf6d0 and 0x4cf5a0 at 0x6dc7e4..0x6dc81f), and 0x6e1940 at 0x6dc8a6
 * keeps the best of them back. The second loop picks the side from the rest; for a reserve side it also drops a
 * player who is recovering (0x62c400 at 0x6dcb88). As for the first team above, nothing checks that the rest can
 * field 11. init_match 0x6b7f20 then stops at match_eng.cpp 612 or 652 (0x6b8973, 0x6b8aca).
 *
 * The case: since item 229 a season game gets the database's loans, and a player on loan keeps his squad slot at his
 * parent club (the loan is a second contract). In a 2014 start (October 2014, all nations, changing rules on, seed 1,
 * session sz02) GNK Dinamo Zagreb had all 50 slots filled on day 269 of 2014, 18 of those players away on loan and 4
 * injured or recovering. The pick kept 14 back for the first team, and its Reserve side (record 87) had 10. The
 * original's answer to a short side, the virtual players of VIRTUAL_STAFF::top_up_club_squad 0x90dae0, needs a free
 * slot, and there was none.
 *
 * The fix, in a season game, for a club's reserve side: when the pick gives fewer than 11 players, it runs again
 * with the next mode (fewer players kept back), up to mode 3, until the side has 11. pick_team writes every slot of
 * its selection (0x6e0d00 at 0x6dd6de), so a later pick replaces an earlier one. The mode is set back after the pick.
 * Logged. The base game keeps the original (Q152). Test switch RECOMP_TEST_RESERVE_PICK=original: the original's
 * short side in a season game too (the proof of tests/seasons/reserve_pick_run.py).
 *
 * International duty (open item 262). Both loops of pick_team leave out a player whom
 * NationalTeams::get_international_duty_club 0x7757d0 holds on the match date (0x6dc7b1, 0x6dc7d0, 0x6dcbc9), for
 * any side, and nothing checks that the rest can field 11. The case: in a 2026 start (Italy, changing rules on, seed
 * 1, the base data, session sz09) Guiseley had all 50 squad slots filled on day 268 of 2027, most of them with
 * youths of the under-21 squads of the Republic of Ireland, Wales and Northern Ireland. In a friendly (fixture +0
 * -2, no competition) 38 of them were on duty and 2 injured, the pick kept nobody back (mode 3) and gave 10, and
 * init_match stopped at match_eng.cpp 652 (0x6b8aca). The reserve fix above did not act: the side is a first team,
 * and nobody was kept back. The top-up 0x90dae0 again had no free slot.
 *
 * The cause is the club list of these new players (youth_club_lists.c), and that file fixes it. This step stays for
 * the squads that the fix cannot change: a save of a season game of 0.7.0 or 0.7.1 (both have the youth pool and
 * season_national_fill.c) can already hold such a club, and the fix gives no player back to his old club.
 *
 * The step, in a season game, for a club side: when the pick still gives fewer than 11 (after the reserve step), it
 * runs once more with the duty test of those three calls answered "no duty", so the players on duty play the
 * club's match. Logged with the count on duty. Test switch RECOMP_TEST_DUTY_PICK=original: the original's short
 * side (the proof of tests/seasons/duty_pick_run.py). */
#include "win.h"
#include "season.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQUAD_SLOTS 50
#define KEPT_BACK 14
#define INJURY 0x62c400u                /* thiscall [0xb63d28] (staff), 2 = cannot play */
#define INJURIES 0xb63d28u
#define ELIGIBILITY 0x8cf5e0u           /* thiscall [0xde1f54] (staff, fixture), 0 = can play */
#define TRANSFERS 0xde1f54u
#define NEXT_SEARCH_RET 0x6dbe83u       /* the return address of the next-fixture search in 0x6dbc50 */

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t no_rotation_pick;       /* the pick (0x6dbc50 this) whose next-fixture search finds nothing */
static int no_rotation_free;

/* The squad players who can play the pick's fixture, as the second loop of 0x6dbc50 tests them. */
static int can_play(uint32_t pick)
{
    uint32_t squad = rd32(g_mem, pick + 0x27), fixture = rd32(g_mem, pick + 0x37);
    int n = 0;
    for (uint32_t i = 0; i < SQUAD_SLOTS && !rt_unwinding; i++) {
        uint32_t p = rd32(g_mem, squad + 4 * i);
        if (!readable(p) || !rd32(g_mem, p + 0x61)) continue;
        uint32_t a[2] = {p, fixture};
        if ((call_this(INJURY, rd32(g_mem, INJURIES), 1, a) & 0xff) == 2 || rt_unwinding) continue;
        if ((call_this(ELIGIBILITY, rd32(g_mem, TRANSFERS), 2, a) & 0xff) == 0 && !rt_unwinding) n++;
    }
    return n;
}

void o_006dbc50(void);

static void pick_rotation(void)
{
    uint32_t pick = cpu.ecx, fixture = rd32(g_mem, pick + 0x37), club = rd32(g_mem, pick + 0x2f);
    if (!season_game_changes() || g_mem[pick + 0x19] || g_mem[pick + 0x1a] != 3 || !readable(fixture) ||
        rd32(g_mem, fixture) == 0xfffffffeu || !readable(club) || (int16_t)rd16(g_mem, club + 0x80) < 6000 ||
        !readable(rd32(g_mem, pick + 0x27))) {
        o_006dbc50();
        return;
    }
    Cpu saved = cpu;
    int n = can_play(pick);
    if (rt_unwinding) return;
    cpu = saved;
    if (n >= KEPT_BACK + 11) {
        o_006dbc50();
        return;
    }
    no_rotation_pick = pick;
    no_rotation_free = n;
    o_006dbc50();
    no_rotation_pick = 0;
}

static const int kept_back[4] = {14, 7, 2, 0};   /* by mode +0x1a, 0x6dc5e4..0x6dc6a4 */

static int reserve_original(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("RECOMP_TEST_RESERVE_PICK"); v = e && !strcmp(e, "original"); }
    return v;
}

/* A club, not a national side, by the test of 0x53b900: the club index (row of 0x245 bytes from [0xae23b4]) is
 * below [0xae235c] - 2 * [0xae2350]; the national sides take the last rows. pick_team keeps nobody back for a
 * national side (0x53b900 at 0x6dc5d4). */
static int club_side(uint32_t club)
{
    uint32_t base = rd32(g_mem, 0xae23b4u);
    int32_t clubs = (int32_t)rd32(g_mem, 0xae235cu) - 2 * (int32_t)rd32(g_mem, 0xae2350u);
    return readable(base) && club >= base && (club - base) % 0x245u == 0 &&
           (int32_t)((club - base) / 0x245u) < clubs;
}

static int reserve_again;               /* a later pick of the short-side fix is running */

/* The international duty test of the pick (open item 262). pick_team 0x6dbc50 leaves out a player whom
 * NationalTeams::get_international_duty_club 0x7757d0 holds on the match date: in its first loop (calls at 0x6dc7b1
 * and 0x6dc7d0, then 0x6dc87c) and in its second loop (call at 0x6dcbc9, then 0x6dccb1, which skips him unless the
 * pass is the stored-selection pass 1). 0x7757d0 holds a player from the day before his side's match to the day
 * after (0x77583d..0x775850). With duty_free set, those three calls (by their return addresses) find no duty; every
 * other call of 0x7757d0 runs the original. */
#define DUTY 0x7757d0u
static int duty_free;

static int duty_original(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("RECOMP_TEST_DUTY_PICK"); v = e && !strcmp(e, "original"); }
    return v;
}

void o_007757d0(void);

RT_WRAP(007757d0, duty_of_pick)
{
    uint32_t ret = rd32(g_mem, cpu.esp);
    if (!duty_free || (ret != 0x6dc7b6u && ret != 0x6dc7d5u && ret != 0x6dcbceu)) {
        o_007757d0();
        return;
    }
    cpu.eax = 0;                        /* no duty: thiscall, ret 0x14 */
    cpu.esp += 4 + 0x14;
}

/* The players of the club's squad slots that 0x7757d0 holds today, for the log. */
static int on_duty(uint32_t pick)
{
    uint32_t club = rd32(g_mem, pick + 0x2f), squad = rd32(g_mem, pick + 0x27);
    int n = 0;
    for (uint32_t i = 0; readable(squad) && i < SQUAD_SLOTS && !rt_unwinding; i++) {
        uint32_t p = rd32(g_mem, squad + 4 * i), a[5] = {p, 0xae2c90u, club, 0, 0};
        if (readable(p) && call_this(DUTY, rd32(g_mem, 0xb74318u), 5, a)) n++;
    }
    return n;
}

RT_WRAP(006dbc50, pick_team_season)
{
    uint32_t pick = cpu.ecx, opponent = rt_arg(0), mode = rt_arg(1), club = rd32(g_mem, pick + 0x2f);
    uint8_t keep = g_mem[pick + 0x1a], reserve = g_mem[pick + 0x19];
    int keep_back = reserve && keep < 3 && !reserve_original(), duty = !duty_original();
    if (reserve_again || !season_game_changes() || !(keep_back || duty) || !club_side(club)) {
        pick_rotation();
        return;
    }
    pick_rotation();
    int first = (int8_t)cpu.eax, n = first, kept_n = first, duty_n = 0, freed = 0;
    if (rt_unwinding || n >= 11) return;
    Cpu after = cpu;
    uint8_t m = keep;
    while (n < 11 && !rt_unwinding) {
        if (keep_back && m < 3) m++;
        else if (duty && !freed) {
            duty_n = on_duty(pick);
            if (rt_unwinding || !duty_n) break;
            kept_n = n;
            freed = 1;
        } else break;
        uint32_t a[2] = {opponent, mode};
        g_mem[pick + 0x1a] = m;
        reserve_again = 1;
        duty_free = freed;
        cpu.ecx = pick;
        n = (int8_t)rt_callback(0x6dbc50u, 2, a);
        duty_free = 0;
        reserve_again = 0;
    }
    if (rt_unwinding) return;
    g_mem[pick + 0x1a] = keep;
    cpu = after;
    cpu.eax = (after.eax & 0xffffff00u) | (uint8_t)n;
    if (n == first) return;             /* no player more: no change to log */
    uint32_t fixture = rd32(g_mem, pick + 0x37), rec = readable(fixture) ? rd32(g_mem, fixture + 0x14) : 0;
    int day = (int16_t)rd16(g_mem, 0xae2c90u), year = (int16_t)rd16(g_mem, 0xae2c92u);   /* today: a friendly's
                                                                                           * fixture has no date */
    const char *match = readable(rec) ? win_str(rec + 4) : "friendly";
    if (m != keep)
        fprintf(stderr, "recomp: reserve pick: %.30s keeps %d players back for its first team, not %d, and fields %d, "
                        "not %d, in the %.30s match on day %d of %d\n", win_str(club + 4), kept_back[m],
                kept_back[keep], freed ? kept_n : n, first, match, day, year);
    if (freed)
        fprintf(stderr, "recomp: duty pick: %.30s fields %d, not %d, in the %.30s match on day %d of %d: %d of its "
                        "players are on international duty that day and play\n", win_str(club + 4), n, kept_n, match,
                day, year, duty_n);
}

void o_005acbe0(void);

RT_WRAP(005acbe0, pick_rotation_next)
{
    uint32_t ret = rd32(g_mem, cpu.esp), self = no_rotation_pick;
    o_005acbe0();
    if (rt_unwinding || !self || ret != NEXT_SEARCH_RET || !(cpu.eax & 0xffff)) return;
    uint32_t fixture = rd32(g_mem, self + 0x37), club = rd32(g_mem, self + 0x2f), rec = rd32(g_mem, fixture + 0x14);
    fprintf(stderr, "recomp: pick rotation: %.30s keeps its players for the %.30s match on day %d of %d: %d players can "
                    "play it, fewer than %d\n", win_str(club + 4), readable(rec) ? win_str(rec + 4) : "?",
            (int16_t)rd16(g_mem, fixture + 0x2a), (int16_t)rd16(g_mem, fixture + 0x28), no_rotation_free, KEPT_BACK + 11);
    cpu.eax &= 0xffff0000u;             /* no fixture in the next 7 days: no rotation */
}
