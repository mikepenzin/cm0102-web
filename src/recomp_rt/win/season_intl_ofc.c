/* The OFC Nations Cup of 8 teams (0.6.2 open item 45). The finals table (season_intl_table.c) gives 8 teams in
 * 2002 (int-continental-21), 2012 (-120), 2016 (-164) and 2024 (-252), and after the table every edition takes the
 * 8 of the last row (0.6.2 decision A). Each of these editions has two groups of 4, the top two of each group to
 * the semi-finals, then the final. The base game (season_intl_on() 0) takes the original's path: each site and
 * wrap here gives the original's value or calls the original then. The editions of 6 (2004) and 4 (2008) have their
 * own formats, below.
 *
 * The class OceaniaNations (oceania_nations.cpp, ctor 0x7988b0, vtable 0x96e0cc) plays 6 nations. The League
 * object is group A, stage[0] (+0xc, count +0x2c = 2 at 0x798980) is group B and stage[1] the Cup_Stage of the
 * semi-finals. On its draw day update_for_date 0x79a100 calls:
 * - select_and_draw_teams 0x799e30: 6 team pointers at +0xee (+0xf2 the count as a word, 6 at 0x799e42; a block of
 *   0x18 at 0x799e4d). Australia and New Zealand (key ids at 0x9cf224 and 0x9cf3f4) go to places 0 and 3 in an
 *   order drawn by utils_random 0x90cfc0 (0x799f92), two Melanesian nations (0x53d900) to places 1 and 4 and two
 *   Polynesian nations (0x53d940) to places 2 and 5, each pair from the top of its list (0x4afbd0 at 0x79a03c and
 *   0x79a04d) in a drawn order (0x79a052, 0x79a096). From 2006 the wrap in season_intl.c (ofc_australia) puts the
 *   Oceania nation with the highest reputation in Australia's place;
 * - set_predicted_positions 0x79a320 (vtable +0x5c, 0x79a1df): 6 places 1, 2, 3, 3, 5, 5 (0x79a425..0x79a46b);
 * - create_group_table 0x7995a0: a table of 3 rows (+0x3e = 3 at 0x7995ae, 0xb1 bytes at 0x7995a9) with the teams
 *   0, 1 and 2 (League::init_table_row 0x687430 at 0x799659);
 * - create_group_stage 0x799a60 (0x79a2c7, group 0): a League_Stage (0x68b100 at 0x799b79) of 3 teams (0x799b74),
 *   the teams 3, 4 and 5 copied to a block of 0xc (0x799a80, the copy at 0x799ac2..0x799ae3).
 * Then +0xee is freed and +0xf2 cleared (0x79a2d3..0x79a2e1), so the draw and the groups are made on one day and a
 * save holds only the tables: the League save and load keep the row count +0x3e (0x689461, 0x688379). The group
 * template create_schedule 0x798b60 has 3 match days (0x798bc8), as the Asian Cup's groups of 4 (0x40eb00); the
 * League scheduler makes rows / 2 fixtures a day (0x6838d6..0x6838e5). Each day of the OFC template has one
 * kick-off slot (slot 0, 0x798ca2..0x798d0a; slot 1 ends the list, 0x798c33), the Asian Cup's two (0x40ec3e,
 * 0x40ec4f; slot 2 ends it, 0x40ebb5). The knockout create_knockout_stage 0x799c50 takes rows 0 and 1 of each
 * group (League table row 0x6826d0 at 0x799cb7..0x799ce6), so it is the same for groups of 4. The rule bytes of
 * set_up_rules 0x799690 (+0x43, +0xc2..+0xc7, +0x4a) are those of the Asian Cup's groups of 4 (0x40f4c0).
 *
 * With 8 teams: the draw adds two Oceania nations by the real split of direct and qualifying places (split_draw
 * below, 0.7.0 option b), one to the end of each group (places 3 and 7; the six move to 0 to 2 and 4 to 6). The
 * groups have 4 rows; the predicted places are 1, 2, 3, 3, 5, 5, 7, 7; a group's fourth gets the group stage codes
 * of a third (set_club_progress 0x79a490); on the draw day each group day gets a second kick-off slot, a copy of its
 * first, so that both matches of the day play at the same time. Sites in tools/recomp/seasons.py ("OFC Nations Cup
 * of 8 teams"). The places of the two added nations in the groups are a decision of this build: inference, not a
 * source.
 *
 * The edition of 4 teams (2008, int-continental-87; https://www.rsssf.org/tables/08oc.html, read 3 November 2026): New
 * Zealand, New Caledonia, Fiji and Vanuatu in one group, home and away, no knockout, 12 matches on 8 days from 17
 * October 2007 to 19 November 2008. The game plays it so (with changing rules off, in every edition when the start
 * season's edition is this one, Q232):
 * - set_up_rules 0x799690 (the constructor 0x798994 and start_new_season 0x799951 call it; a load does not, 0x798961):
 *   each pair meets twice (+0x3c, 1 at 0x7996d2; the League scheduler makes rows - 1 rounds that many times,
 *   0x683ac9..0x683ad3), and the stage count +0x2c (2 at 0x798980) becomes 0, so League::update_for_date 0x6847c0
 *   never calls advance_stage 0x7999d0 (it calls it while +0x30 < +0x2c - 1, 0x6849d1..0x6849e0) and ends the
 *   edition when the group's table is complete: the club history 0x442e80 (0x6849fa), the honours (vtable +0x30,
 *   0x684a8f) and the complete flag +0x4c (0x684a9e). The draw date (+0xb6 day, +0xb8 year offset; 5 March of the
 *   edition year at 0x7996b3..0x799739) becomes 1 October 2007, after the South Pacific Games of 2007 that chose the
 *   three other teams (inference: the sources give no draw). The next set_up_rules gives +0x2c its 2 again, with a
 *   new stage array (a load of a save of the edition of 4 makes the array of the saved count, none for 0:
 *   0x688e44..0x688e5b);
 * - the group template (create_schedule 0x798b60, stage -1, kept at +0xba): 6 match days in place of 3 (0x798bc8),
 *   so the League scheduler makes a double round robin of 2 fixtures a day. The days are the real days of the
 *   edition with two matches, or one match on each of two near days: 17 and 21 November 2007, 14 and 21 June, 6 and
 *   10 September 2008; for a game made after them (a 2008/09 start, made on 1 January 2008) the first 6 of these and
 *   15 October and 19 November 2008 that come at least a week after the game's day (15 October: an international
 *   match day of 2008, inference; 19 November: the real last match day). With fewer than 6 such days the original's
 *   template of 3 days stays and each pair meets once. The friendly days of the finals (season_intl_finals_busy)
 *   still cover the original's 19 June window only (finals_days, season_intl_rules.c): a friendly that clashes with
 *   a group day is moved by add_fixture 0x5aa680 as any clash;
 * - the draw: New Zealand (key id 0x9cf3f4, one of the original's six) and the three Oceania nations with the
 *   highest nation reputation (the real three came from the South Pacific Games, which the game does not play:
 *   inference); +0xf2 is 4, so create_group_table makes 4 rows, and the wrap of create_group_stage 0x799a60 (group
 *   B; the original calls it at 0x79a2c7) skips it; the predicted places are 1 to 4; the progress codes of the group
 *   (set_club_progress 0x79a490) are the places, as the original passes on the codes of its knockout stage
 *   (0x79a515..0x79a52b), not the codes of a group that goes on to a knockout (inference);
 * - the honours: slot +0x30 of the class is 0x595160, a jump to stage 1's (the knockout's Cup::record_season_honours
 *   0x519a90). With no stage the class takes League::record_season_honours 0x684640: the group's first is the winner,
 *   the second the runner-up (rows 0 and 1 of +0xb1, 0x68465b..0x68465d).
 * A game that passed the season change before the edition in a build before 0.7.0 holds it with +0x2c 2 and the
 * original's draw date: it keeps the original's two groups of 3 and their path (one_group_staged).
 *
 * The edition of 6 (2004, int-continental-32 and -43; https://www.rsssf.org/tables/04oc.html and
 * https://en.wikipedia.org/wiki/2004_OFC_Nations_Cup, read 5 November 2026): Australia and New Zealand with the four
 * winners of a first round of 10 (Solomon Islands, Tahiti, Vanuatu, Fiji) in one group in Adelaide, each pair once,
 * 15 matches on 29 and 31 May and 2, 4 and 6 June 2004; then the first two in a final of two legs, 9 October in
 * Honiara and 12 October in Sydney. The game plays it so in a season game (with changing rules off, in every edition
 * when the start season's edition is this one, Q232):
 * - set_up_rules: the stage count +0x2c becomes 1 (the final); the draw date stays the original's;
 * - the group template (six_template): the 5 real days, so the League scheduler makes a round robin of 3 fixtures a
 *   day;
 * - the draw: the original's six (the first round is not played: the six are the original's choice, inference);
 *   create_group_table makes 6 rows (the sites of seasons.py), create_group_stage (group B) is skipped, the predicted
 *   places are 1 to 6 and the group's progress codes are its places;
 * - advance_stage (ofc_advance): at the group's end the final of its first two, a Cup_Stage in stage[0];
 * - the honours: the final's (Cup::record_season_honours), from stage[0].
 * A save made in the edition by an older build keeps the original's two groups of 3 and its path. */
#include "win.h"
#include "season_intl.h"
#include "season_uefa_days.h"         /* season_day_of_year */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OFC_VTABLE 0x96e0ccu
#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_NAME 0x4u
#define NATION_CONTINENT 0x71u          /* continent record, its +0 the continent id */
#define NATION_REPUTATION 0x8eu         /* s16 (season_intl.c ofc_australia) */
#define CONTINENT_OCEANIA 0x9cfa18u     /* 0x799ef9 */
#define CLUB_REPUTATION 0x80u           /* s16: the field of the sort compare 0x4b07d0 (0x4b07f0) */

#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size) */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define GAME_QSORT 0x00945333u          /* qsort, cdecl (base, count, size, compare) */
#define CLUB_COMPARE 0x004b07d0u        /* compare of club pointers (0x79a3ff) */
#define ROUND_SLOT 0x0068a1c0u          /* league_round_set_match_slot, cdecl (rounds, round, slot 0 to 7, weekday,
                                         * kind, option, flag): flag at round * 0x41 + slot * 7 + 5, weekday +9,
                                         * kind +0xa, option +0xb (0x68a382..0x68a3c9) */
#define ROUND_SIZE 0x41u
#define GROUP_DAYS 3                    /* 0x798bc8 */
#define NATIONAL_TEAM 0x0053b3d0u       /* national team club of a nation record, cdecl (nation) */
#define SET_FINAL_POSITION 0x004a2540u  /* Comp_Stats::set_club_final_position, thiscall (club, position) */

#define TEAMS_OF 0xeeu                  /* team pointers, 4 bytes each */
#define TEAM_COUNT 0xf2u                /* word (0x799e42) */
#define GROUP_ROWS 0x3eu                /* word: the rows of group A's table (0x7995ae) */
#define GROUP_A_ROUNDS 0xbau            /* group A's template (set_up_rules 0x799747, freed at 0x7998ff) */

#define STAGES 0xcu                     /* the stage array (8 bytes at 0x798987) */
#define STAGE_COUNT 0x2cu               /* 2 at 0x798980 */
#define MEETINGS 0x3cu                  /* s16: the times each pair meets (1 at 0x7996d2); the League scheduler
                                         * makes rows - 1 rounds that many times (0x683ac9..0x683ad3) */
#define DRAW_DAY 0xb6u                  /* s16 day of the year (0x79971e) */
#define DRAW_OFFSET 0xb8u               /* s16 year offset from +0x40 (0x799739) */
#define TEAM_MATCHES 0xa7u              /* s16: the matches of each team, -1 until worked out (League ctor 0x682263) */
#define ROUNDS 0xa9u                    /* s16: the days of group A's template (set_up_rules 0x799740) */
#define GAME_DATE 0xae2c90u             /* s16 day of the year, s16 year */
#define KEY_NEW_ZEALAND 0x9cf3f4u       /* key nation id (0x799fc1) */
#define LEAGUE_HONOURS 0x00684640u      /* League::record_season_honours, thiscall () */

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) (0x799d0c) */
#define CUP_STAGE_NEW 0x00522e00u       /* Cup_Stage ctor, thiscall, 15 arguments (ret 0x3c at 0x5230b9) */
#define CUP_START 0x0051c800u           /* Cup::set_up_round, thiscall (0) (0x799e09) */
#define TABLE_ROW 0x006826d0u           /* League::get_table_row, thiscall (row): the row, +0 the club (0x799cb7) */
#define CUP_FIRST_DATE 0x00521e60u      /* cdecl (rec, round, day, month0, year offset, weekday, year) */
#define CUP_LAST_DATE 0x00521eb0u       /* cdecl (rec, round, day, month0, year offset, weekday, x, year, y) */
#define CUP_ROUND_SIZE 0x68u            /* knockout round record (season_cups.h) */
#define CURRENT_STAGE 0x30u             /* the last stage in play, -1 before group B (0x799c1d, 0x799947) */

#define TEAMS 8
#define GROUP 4
#define ONE_GROUP 4                     /* the edition of one group, home and away (2008) */
#define ONE_GROUP_DAYS 6
#define SIX 6                           /* the edition of one group of 6 and a final of two legs (2004) */
#define SIX_DAYS 5

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_INTL_LOG");
        on = v && *v == '1';
    }
    return on;
}

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(GAME_MALLOC, 1, a);
    if (readable(p)) memset(g_mem + p, 0, size);
    return readable(p) ? p : 0;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(GAME_FREE, 1, a);
}

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + 0x40u); }

static int is_ofc(uint32_t self) { return readable(self) && rd32(g_mem, self) == OFC_VTABLE; }

/* 1 when the class holds a drawn edition of 8 teams (the count that season_intl_ofc_draw wrote). */
static int drawn8(uint32_t self) { return is_ofc(self) && (int16_t)rd16(g_mem, self + TEAM_COUNT) == TEAMS; }

/* 1 when the class holds a drawn edition of one group of 4 (the count that season_intl_ofc_draw wrote). */
static int drawn4(uint32_t self) { return is_ofc(self) && (int16_t)rd16(g_mem, self + TEAM_COUNT) == ONE_GROUP; }

/* 1 for the edition of one group, home and away, in a season game (the table's 2008). */
static int one_group(uint32_t self)
{
    if (!season_intl_on() || !is_ofc(self)) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition("oceania_nations", year_of(self));
    return e && e->teams == ONE_GROUP;
}

/* 1 when the class holds that edition as this build sets it up (+0x2c 0, season_intl_ofc_rules, which runs from
 * set_up_rules 0x799690 at the start_new_season of the edition before). A game that passed that season change in a
 * build before 0.7.0 keeps the original's two groups (+0x2c 2, the draw on 5 March of the edition year), and the
 * original's path then goes on: a draw of one group of 4 there left group B (stage[0]) unbuilt while +0x2c stayed 2,
 * and League::update_for_date 0x6847c0 called the empty slot every day (0x6848f8; a player's 2001 game, loaded in
 * January 2008, stopped on 19 June 2008: "call to 0 ... from 0x6848fb"). */
static int one_group_staged(uint32_t self) { return one_group(self) && rd32(g_mem, self + STAGE_COUNT) == 0; }

/* 1 for the edition of one group of 6 and a final of two legs, in a season game (the table's 2004). */
static int six_group(uint32_t self)
{
    if (!season_intl_on() || !is_ofc(self)) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition("oceania_nations", year_of(self));
    return e && e->teams == SIX;
}

/* 1 when the class holds that edition as this build sets it up (+0x2c 1, season_intl_ofc_rules). A save made during
 * the edition by an older build keeps the original's two groups (+0x2c 2), and the original's path then goes on. */
static int six_staged(uint32_t self) { return six_group(self) && rd32(g_mem, self + STAGE_COUNT) == 1; }

/* 1 on the draw day of that edition, while +0xf2 holds the 6 drawn teams. */
static int drawn6(uint32_t self) { return six_staged(self) && (int16_t)rd16(g_mem, self + TEAM_COUNT) == SIX; }

/* ---- The draw ---- */

static int nation_count(void) { return (int)rd32(g_mem, NATION_COUNT); }
static uint32_t nation_at(int id) { return rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE; }

static uint32_t continent_of(uint32_t nation)
{
    uint32_t c = rd32(g_mem, nation + NATION_CONTINENT);
    return readable(c) ? rd32(g_mem, c) : 0xffffffffu;
}

static uint32_t team_nation(uint32_t club) { return readable(club) ? rd32(g_mem, club + 0x53u) : 0; }

static int nation_in(const uint32_t *teams, int n, uint32_t nation)
{
    for (int k = 0; k < n; k++)
        if (team_nation(teams[k]) == nation) return 1;
    return 0;
}

/* The national team of the Oceania nation with the highest reputation that is not in yet, or 0. A team counts
 * only when its club names the nation (season_intl.c team_nation: a season database can give an empty row). */
static uint32_t strongest(const uint32_t *teams, int n)
{
    uint32_t best = 0, oceania = rd32(g_mem, CONTINENT_OCEANIA);
    int best_rep = -100000;
    for (int id = 0; id < nation_count(); id++) {
        uint32_t nation = nation_at(id);
        int rep = (int16_t)rd16(g_mem, nation + NATION_REPUTATION);
        if (rep <= best_rep || continent_of(nation) != oceania || nation_in(teams, n, nation)) continue;
        uint32_t a[1] = {nation};
        uint32_t club = call_cdecl(NATIONAL_TEAM, 1, a);
        if (rt_unwinding) return 0;
        if (team_nation(club) != nation) continue;
        best = club;
        best_rep = rep;
    }
    return best;
}

static void second_slots(uint32_t rounds);
static int split_draw(uint32_t self, uint32_t teams[TEAMS], uint32_t *first, uint32_t *second);
static uint32_t kept[2];                /* the teams of the qualifying places of the draw (ofc_hosts keeps them) */
static int n_kept;

static int club_reputation(uint32_t club) { return readable(club) ? (int16_t)rd16(g_mem, club + CLUB_REPUTATION) : 0; }

/* After the original's draw of 6 (and the Australia swap of season_intl.c), before the hosts come in: for an
 * edition of 8 the block at +0xee becomes 8 teams, group A in places 0 to 3 and group B in 4 to 7. The six keep
 * their groups (places 0 to 2 and 3 to 5 become 0 to 2 and 4 to 6). The two added nations come from the split of
 * direct and qualifying places (split_draw; every edition of 8 has one, in the table and after it); only when the
 * data has fewer than 8 Oceania nations with a national team are they chosen by the nation reputation, as
 * ofc_australia chooses. The first goes to the group whose first place (New Zealand, and
 * Australia or the nation in its place, in the order drawn at 0x799f92) has the lower team reputation, the field
 * that the original's sort of teams compares (0x4b07d0). When the data has fewer than two more Oceania nations the
 * six stay. */
static void draw_one_group(uint32_t self, uint32_t six);

void season_intl_ofc_draw(uint32_t self)
{
    if (!season_intl_on() || !is_ofc(self)) return;
    const SeasonIntlEdition *e = season_intl_rule_edition("oceania_nations", year_of(self));
    uint32_t six = rd32(g_mem, self + TEAMS_OF);
    n_kept = 0;
    if (e && e->teams == ONE_GROUP && readable(six) && (int16_t)rd16(g_mem, self + TEAM_COUNT) == 6) {
        if (one_group_staged(self)) {
            draw_one_group(self, six);
            return;
        }
        fprintf(stderr, "recomp: season intl: oceania_nations %d: set up by an older build (%d stages), the "
                        "original's two groups of 3\n", year_of(self), (int32_t)rd32(g_mem, self + STAGE_COUNT));
        return;
    }
    if (!e || e->teams != TEAMS || !readable(six) || (int16_t)rd16(g_mem, self + TEAM_COUNT) != 6) return;
    uint32_t teams[TEAMS] = {0};
    for (int k = 0; k < 6; k++) teams[k < 3 ? k : k + 1] = rd32(g_mem, six + 4u * (uint32_t)k);
    uint32_t first = 0, second = 0;
    if (!split_draw(self, teams, &first, &second)) {
        if (rt_unwinding) return;
        first = strongest(teams, TEAMS);
        if (rt_unwinding) return;
        teams[3] = first;                               /* so that the second pick leaves it out */
        second = first ? strongest(teams, TEAMS) : 0;
    }
    if (rt_unwinding) return;
    if (!second) {
        fprintf(stderr, "recomp: season intl: oceania_nations %d: no two more Oceania nations, 6 teams play\n",
                year_of(self));
        return;
    }
    int a_weaker = club_reputation(teams[0]) <= club_reputation(teams[4]);
    teams[3] = a_weaker ? first : second;
    teams[7] = a_weaker ? second : first;
    uint32_t list = game_malloc(4u * TEAMS);
    if (rt_unwinding || !list) return;
    for (int k = 0; k < TEAMS; k++) wr32(g_mem, list + 4u * (uint32_t)k, teams[k]);
    game_free(six);
    if (rt_unwinding) return;
    wr32(g_mem, self + TEAMS_OF, list);
    wr16(g_mem, self + TEAM_COUNT, TEAMS);
    second_slots(rd32(g_mem, self + GROUP_A_ROUNDS));
    if (rt_unwinding) return;
    if (logging())
        fprintf(stderr, "recomp: season intl: oceania_nations %d: 8 teams, %s and %s added\n", year_of(self),
                win_str(team_nation(first) + NATION_NAME), win_str(team_nation(second) + NATION_NAME));
}

/* The edition of one group: New Zealand from the original's six (else the strongest), then the three Oceania nations
 * with the highest reputation that are not in. With fewer than four in the data the six stay. */
static void draw_one_group(uint32_t self, uint32_t six)
{
    uint32_t teams[ONE_GROUP] = {0}, nz = 0;
    int nz_id = (int)rd32(g_mem, KEY_NEW_ZEALAND);
    for (int k = 0; k < 6; k++) {
        uint32_t t = rd32(g_mem, six + 4u * (uint32_t)k);
        if (nz_id >= 0 && nz_id < nation_count() && readable(t) && team_nation(t) == nation_at(nz_id)) nz = t;
    }
    for (int k = 0; k < ONE_GROUP; k++) {
        teams[k] = k == 0 && nz ? nz : strongest(teams, ONE_GROUP);
        if (rt_unwinding) return;
        if (!teams[k]) {
            fprintf(stderr, "recomp: season intl: oceania_nations %d: fewer than 4 Oceania nations, 6 teams play\n",
                    year_of(self));
            return;
        }
    }
    uint32_t list = game_malloc(4u * ONE_GROUP);
    if (rt_unwinding || !list) return;
    for (int k = 0; k < ONE_GROUP; k++) wr32(g_mem, list + 4u * (uint32_t)k, teams[k]);
    game_free(six);
    if (rt_unwinding) return;
    wr32(g_mem, self + TEAMS_OF, list);
    wr16(g_mem, self + TEAM_COUNT, ONE_GROUP);
    if (logging())
        fprintf(stderr, "recomp: season intl: oceania_nations %d: one group of 4: %s, %s, %s, %s\n", year_of(self),
                win_str(team_nation(teams[0]) + NATION_NAME), win_str(team_nation(teams[1]) + NATION_NAME),
                win_str(team_nation(teams[2]) + NATION_NAME), win_str(team_nation(teams[3]) + NATION_NAME));
}

/* ---- The direct and qualifying places of an edition of 8 (0.7.0, open item 45 option b; Max, 3 November 2026) ----
 *
 * The real editions of 8 took the best-ranked Oceania nations straight to the finals and gave the last places to a
 * qualifying round of the lowest-ranked (run-logs/0.6.2-s45b/research-ofc.md):
 * - 2002 (https://www.rsssf.org/tables/02oc.html): 6 straight, the top six of the FIFA ranking of October 2001
 *   (Australia, New Zealand, Fiji, Tahiti, Solomon Islands, Vanuatu), and 2 from the round of the nations ranked 7
 *   to 12 (Papua New Guinea, New Caledonia);
 * - 2012, 2016 and 2024 (https://en.wikipedia.org/wiki/2012_OFC_Nations_Cup_qualification, ..._2016_OFC_Nations_Cup,
 *   ..._2024_OFC_Men's_Nations_Cup_qualification): 7 straight with the hosts, 1 from the round of the four lowest.
 * After the table each edition takes the split of the last row (2024, split_of); with changing rules off the split
 * of the start season's edition stays (Q232). Max chose (option b) that the game plays no round: the qualifying
 * places go to the strongest of the next nations of the ranking.
 *
 * The ranking is the game's own FIFA ranking: the FIFA_RANKINGS object (its pointer at [0xb74318] +0x1c, read at
 * 0x49c248..0x49c253 by the rankings screen) has at +0 its table, a record of 0x2c bytes for each nation id, +0x10
 * its position from 1
 * (create_default_table 0x58dd50 seeds the score from nation +0xa8, the ranking points of the data, and numbers the
 * positions by score at 0x58de5f..0x58de7d; update_rankings 0x58d460 numbers them again each month;
 * get_nation_strength_tier 0x58e190 reads +0x10 at 0x58e1ae). A nation with no position comes after those with one,
 * then the nation reputation and the nation id decide. The hosts of the edition (the table's, after the table the
 * rotation's) go straight; the next 8 - places - hosts Oceania nations with a national team go straight too; the
 * next `places` take the qualifying places.
 *
 * The draw keeps the original's draw of six (0x799e30: the group heads and the Melanesian and Polynesian places):
 * a team of the six that does not go straight gives its place to the best-ranked nation that goes straight and is
 * not in yet. The added places 3 and 7 take the qualifying places (2002), or the last nation that goes straight and
 * the qualifying place (2012 on), as season_intl_ofc_draw places them. */

#define FIFA_HOLDER 0xb74318u           /* +0x1c: FIFA_RANKINGS * (0x49c248, 0x49c253) */
#define FIFA_RECORD 0x2cu               /* a record a nation id (0x58e1a6..0x58e1ae) */
#define FIFA_POSITION 0x10u             /* s16, 1 the best (0x58de73) */
#define NO_POSITION 30000

static const struct {
    int year, places;
} splits[] = {{2002, 2}, {2012, 1}, {2016, 1}, {2024, 1}};

/* The qualifying places of the finals of `year`: the split of its rule edition, or of the last table edition before
 * it (an edition after the table, a copy of the 2024 row with its own year: continued, season_intl_rules.c), 0 for an
 * edition that is not of 8. */
static int split_of(int year)
{
    const SeasonIntlEdition *e = season_intl_rule_edition("oceania_nations", year);
    int places = 0, from = 0;
    if (!e || e->teams != TEAMS) return 0;
    for (size_t i = 0; i < sizeof splits / sizeof splits[0]; i++)
        if (splits[i].year <= e->year && splits[i].year > from) from = splits[i].year, places = splits[i].places;
    return places;
}

static int fifa_position(int id)
{
    uint32_t holder = rd32(g_mem, FIFA_HOLDER), rankings = readable(holder) ? rd32(g_mem, holder + 0x1cu) : 0;
    uint32_t table = readable(rankings) ? rd32(g_mem, rankings) : 0;
    uint32_t at = readable(table) ? table + (uint32_t)id * FIFA_RECORD + FIFA_POSITION : 0;
    if (!readable(at)) return NO_POSITION;
    int pos = (int16_t)rd16(g_mem, at);
    return pos > 0 ? pos : NO_POSITION;
}

typedef struct {
    uint32_t team;
    int position, rep, id, host;
} Ranked;

static int ranked_compare(const void *a, const void *b)
{
    const Ranked *x = a, *y = b;
    if (x->position != y->position) return x->position - y->position;
    if (x->rep != y->rep) return y->rep - x->rep;
    return x->id - y->id;
}

static int team_in(const uint32_t *list, int n, uint32_t team)
{
    for (int k = 0; k < n; k++)
        if (list[k] && team_nation(list[k]) == team_nation(team)) return 1;
    return 0;
}

/* See above. Gives 0 for an edition with no split, or when the data has fewer than 8 Oceania nations. */
static int split_draw(uint32_t self, uint32_t teams[TEAMS], uint32_t *first, uint32_t *second)
{
    int year = year_of(self), places = split_of(year);
    if (!places) return 0;
    /* The hosts, as ofc_hosts finds them. */
    const SeasonIntlEdition *e = season_intl_edition("oceania_nations", year);
    uint32_t hosts[6], oceania = rd32(g_mem, CONTINENT_OCEANIA);
    int nh = 0;
    char one[128];
    for (const char *p = e && e->hosts ? e->hosts : ""; *p && nh < 6;) {
        size_t len = strcspn(p, ";");
        snprintf(one, sizeof one, "%.*s", (int)len, p);
        p += len;
        if (*p == ';') p++;
        uint32_t t = season_intl_team_named(one);
        if (rt_unwinding) return 0;
        if (t && continent_of(team_nation(t)) == oceania) hosts[nh++] = team_nation(t);
    }
    Ranked list[64];
    int n = 0;
    for (int id = 0; id < nation_count() && n < 64; id++) {
        uint32_t nation = nation_at(id);
        if (continent_of(nation) != oceania) continue;
        uint32_t a[1] = {nation};
        uint32_t club = call_cdecl(NATIONAL_TEAM, 1, a);
        if (rt_unwinding) return 0;
        if (team_nation(club) != nation) continue;
        int host = 0;
        for (int h = 0; h < nh; h++) host |= hosts[h] == nation;
        list[n++] = (Ranked){club, fifa_position(id), (int16_t)rd16(g_mem, nation + NATION_REPUTATION), id, host};
    }
    qsort(list, (size_t)n, sizeof list[0], ranked_compare);
    uint32_t direct[TEAMS], qual[2];
    int nd = 0, nq = 0;
    for (int i = 0; i < n; i++)
        if (list[i].host) direct[nd++] = list[i].team;
    for (int i = 0; i < n; i++) {
        if (list[i].host) continue;
        if (nd < TEAMS - places) direct[nd++] = list[i].team;
        else if (nq < places) qual[nq++] = list[i].team;
    }
    if (nd + nq < TEAMS) {
        fprintf(stderr, "recomp: season intl: oceania_nations %d: %d Oceania nations for 8 places\n", year, n);
        return 0;
    }
    /* A team of the six that does not go straight gives its place to the best of those that do. */
    for (int k = 0; k < TEAMS; k++) {
        if (!teams[k] || team_in(direct, nd, teams[k])) continue;
        uint32_t t = 0;
        for (int d = 0; d < nd && !t; d++)
            if (!team_in(teams, TEAMS, direct[d])) t = direct[d];
        if (!t) continue;
        if (logging())
            fprintf(stderr, "recomp: season intl: oceania_nations %d: %s does not go straight, %s does\n", year,
                    win_str(team_nation(teams[k]) + NATION_NAME), win_str(team_nation(t) + NATION_NAME));
        teams[k] = t;
    }
    uint32_t rest = 0;
    for (int d = 0; d < nd && !rest; d++)
        if (!team_in(teams, TEAMS, direct[d])) rest = direct[d];
    *first = places == 2 ? qual[0] : rest;
    *second = places == 2 ? qual[1] : qual[0];
    if (!*first || !*second) return 0;
    for (int k = 0; k < nq; k++) kept[k] = qual[k];
    n_kept = nq;
    if (logging()) {
        fprintf(stderr, "recomp: season intl: oceania_nations %d: straight:", year);
        for (int d = 0; d < nd; d++)
            fprintf(stderr, "%s %s", d ? "," : "", win_str(team_nation(direct[d]) + NATION_NAME));
        fprintf(stderr, "; qualifying places:");
        for (int k = 0; k < nq; k++)
            fprintf(stderr, "%s %s", k ? "," : "", win_str(team_nation(qual[k]) + NATION_NAME));
        fprintf(stderr, "\n");
    }
    return 1;
}

int season_intl_ofc_kept(uint32_t team)
{
    for (int k = 0; k < n_kept; k++)
        if (kept[k] && team_nation(kept[k]) == team_nation(team)) return 1;
    return 0;
}

/* ---- The edition of one group: rules, template, group B, honours ---- */

/* After set_up_rules 0x799690 (the wrap in season_intl.c, inside its date shift, before its late draw): the stage
 * count and the draw date of the edition of one group; the stage count of 2 back for the next edition. */
void season_intl_ofc_rules(uint32_t self)
{
    if (!season_intl_on() || !is_ofc(self)) return;
    /* +0xa7, the matches of each team of the group table: only the League constructor sets it to -1 (0x682263), and
     * each League step that meets -1 works it out as (rows - 1) x meetings, first init_table_row 0x687453 on the draw
     * day (0x799659), which also sizes each row's form array from it (0x687570); the table sort (0x6827ef) and the
     * load (0x688c29) do the same. So an edition of another size kept the count of the edition before: the groups of
     * 3 of 2004 kept the 3 of the groups of 4 of 2002, and the 2004 edition then stopped after its groups (a Tier A
     * game of 2001 had no knockout by July 2004); the home-and-away group of 2008 (6) kept 3. As season_sizes.c does
     * for a league whose size changes, each edition works it out again. A new game is unchanged: the constructor set
     * -1 just before (set_up_rules from the constructor, 0x798994). */
    wr16(g_mem, self + TEAM_MATCHES, 0xffffu);
    if (one_group(self)) {
        int year = year_of(self);
        wr32(g_mem, self + STAGE_COUNT, 0);
        if ((int16_t)rd16(g_mem, self + ROUNDS) == ONE_GROUP_DAYS) wr16(g_mem, self + MEETINGS, 2);
        wr16(g_mem, self + DRAW_DAY, (uint16_t)season_day_of_year(year - 1, 10, 1));
        wr16(g_mem, self + DRAW_OFFSET, (uint16_t)-1);
        if (logging()) fprintf(stderr, "recomp: season intl: oceania_nations %d: one group, draw 1 October %d\n", year,
                               year - 1);
        return;
    }
    if (six_group(self)) {
        /* One stage, the final, in stage[0] (ofc_advance); the draw date stays the original's. */
        wr32(g_mem, self + STAGE_COUNT, 1);
        if (logging()) fprintf(stderr, "recomp: season intl: oceania_nations %d: one group of 6 and a final\n",
                               year_of(self));
        return;
    }
    if (rd32(g_mem, self + STAGE_COUNT) == 2) return;
    /* After the edition of one group or of the group of 6 (or a load of its save, whose stage array has the saved
     * count, 0 or 1: 0x688e44..0x688e5b) */
    uint32_t stages = game_malloc(8);
    if (rt_unwinding || !stages) return;
    game_free(rd32(g_mem, self + STAGES));
    if (rt_unwinding) return;
    wr32(g_mem, self + STAGES, stages);
    wr32(g_mem, self + STAGE_COUNT, 2);
}

/* The real match days of the edition of one group, the year as an offset from the edition year. */
static const SeasonNlDate one_group_days[] = {
    {17, 11, -1}, {21, 11, -1}, {14, 6, 0}, {21, 6, 0}, {6, 9, 0}, {10, 9, 0}, {15, 10, 0}, {19, 11, 0},
};

/* The template of group A for the edition of one group: thiscall (stage, short *rounds, short *id, int *flag), ret
 * 0x10, as the original's group template (flag 1, 3 days, id 0x3e9 + stage, a block of 0x41 a day at
 * 0x798ba6..0x798bd0). Gives 0 for another stage or edition. */
static int one_group_template(uint32_t self)
{
    uint32_t sp = cpu.esp;
    int stage = (int8_t)rd8(g_mem, sp + 4u);
    if (stage != -1 || !one_group(self)) return 0;
    int year = year_of(self), ty = (int16_t)rd16(g_mem, GAME_DATE + 2), td = (int16_t)rd16(g_mem, GAME_DATE);
    long from = season_intl_abs_day(year - 1, season_day_of_year(year - 1, 10, 1));
    if (ty >= 1900 && season_intl_abs_day(ty, td) > from) from = season_intl_abs_day(ty, td);
    SeasonNlDate days[ONE_GROUP_DAYS];
    int n = 0;
    for (size_t i = 0; i < sizeof one_group_days / sizeof one_group_days[0] && n < ONE_GROUP_DAYS; i++) {
        SeasonNlDate d = one_group_days[i];
        int y = year + d.year;
        if (season_intl_abs_day(y, season_day_of_year(y, d.month, d.day)) >= from + 7) days[n++] = d;
    }
    if (n < ONE_GROUP_DAYS) {
        fprintf(stderr, "recomp: season intl: oceania_nations %d: %d match days to come, the original's template\n",
                year, n);
        return 0;
    }
    uint32_t rec = game_malloc(ONE_GROUP_DAYS * ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    uint32_t rounds = rd32(g_mem, sp + 8u), id = rd32(g_mem, sp + 0xcu), flag = rd32(g_mem, sp + 0x10u);
    if (readable(flag)) wr32(g_mem, flag, 1);
    if (readable(rounds)) wr16(g_mem, rounds, ONE_GROUP_DAYS);
    if (readable(id)) wr16(g_mem, id, (uint16_t)(0x3e9 + stage));
    for (int i = 0; i < ONE_GROUP_DAYS && !rt_unwinding; i++) season_nl_league_round(rec, i, days[i], year);
    if (rt_unwinding) return 1;
    if (logging())
        fprintf(stderr, "recomp: season intl: oceania_nations %d: one group, home and away, from %d/%d/%d\n", year,
                days[0].day, days[0].month, year + days[0].year);
    rt_ret_stdcall(rec, 4);
    return 1;
}

/* ---- The edition of one group of 6 and a final of two legs (2004, 0.6.2 open item 136) ---- */

/* The real match days of the group of 6 (Adelaide) and the first leg of the final (Honiara), offsets from the edition
 * year; the second leg (Sydney, 12 October) comes SECOND_LEG days after the first. */
static const SeasonNlDate six_days[SIX_DAYS] = {{29, 5, 0}, {31, 5, 0}, {2, 6, 0}, {4, 6, 0}, {6, 6, 0}};
static const SeasonNlDate first_leg = {9, 10, 0};
#define SECOND_LEG 3
#define LEAD_DAYS 7                     /* a day of the edition at least this many days after the game's day */

static long today_abs(void)
{
    int ty = (int16_t)rd16(g_mem, GAME_DATE + 2), td = (int16_t)rd16(g_mem, GAME_DATE);
    return ty >= 1900 ? season_intl_abs_day(ty, td) : 0;
}

static long abs_of(SeasonNlDate d, int year)
{
    return season_intl_abs_day(year + d.year, season_day_of_year(year + d.year, d.month, d.day));
}

/* The date of a day number, as an offset from `year`. */
static SeasonNlDate date_of(long day, int year)
{
    int y = year - 1;
    while (season_intl_abs_day(y + 1, 0) <= day) y++;
    int doy = (int)(day - season_intl_abs_day(y, 0)), m = 1;
    while (m < 12 && season_day_of_year(y, m + 1, 1) <= doy) m++;
    return (SeasonNlDate){doy - season_day_of_year(y, m, 1) + 1, m, y - year};
}

/* The group template (stage -1, kept at +0xba): the 5 real days, each pair once, 3 fixtures a day (the League
 * scheduler makes rows - 1 rounds and rows / 2 fixtures a day, 0x683ac9..0x683ad3, 0x6838d6..0x6838e5). A game made
 * later than a week before the first day takes the same days moved on to a week after its day. The table's first day
 * (29 May) moves the class's dates while the builder runs (season_intl.c stage_builder, the 19 June of the original's
 * template for an edition of a year divisible by 4, 0x798f6d..0x798fd3); these days are the table's own, so the
 * shift is undone here. Gives 0 for another stage or edition. Only set_up_rules makes this template (0x799740), and
 * season_intl_ofc_rules then sets +0x2c to 1, so the test is the edition, not six_staged: a load does not call
 * set_up_rules (0x798961), so a save of an older build keeps its template of 3 days with its +0x2c of 2. */
static int six_template(uint32_t self)
{
    uint32_t sp = cpu.esp;
    int stage = (int8_t)rd8(g_mem, sp + 4u);
    if (stage != -1 || !six_group(self)) return 0;
    int year = year_of(self);
    long move = today_abs() + LEAD_DAYS - abs_of(six_days[0], year);
    if (move < 0) move = 0;
    uint32_t rec = game_malloc(SIX_DAYS * ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    uint32_t rounds = rd32(g_mem, sp + 8u), id = rd32(g_mem, sp + 0xcu), flag = rd32(g_mem, sp + 0x10u);
    if (readable(flag)) wr32(g_mem, flag, 1);
    if (readable(rounds)) wr16(g_mem, rounds, SIX_DAYS);
    if (readable(id)) wr16(g_mem, id, (uint16_t)(0x3e9 + stage));
    int shift = season_intl_shift_now();
    season_intl_shift_more(-shift);
    for (int i = 0; i < SIX_DAYS && !rt_unwinding; i++)
        season_nl_league_round(rec, i, date_of(abs_of(six_days[i], year) + move, year), year);
    season_intl_shift_more(shift);
    if (rt_unwinding) return 1;
    if (logging())
        fprintf(stderr, "recomp: season intl: oceania_nations %d: one group of 6, %ld days later than the real days\n",
                year, move);
    rt_ret_stdcall(rec, 4);
    return 1;
}

/* The final's round record (one round of 0x68 bytes, as the original's knockout template 0x798d92..0x798e2f and its
 * final 0x798e35..0x798ebb): the round value 0x96 of a final, 2 teams, two legs (+0x21) SECOND_LEG days apart
 * (+0x22), the first leg with no extra time, the second with extra time and penalties (+0x0f 3; the flags of
 * season_cups.c type 'L': away goals count). +0x0d holds only the draw bit 0x200: Cup::draw_round 0x51c410 then
 * neither sorts (0x400, 0x51c4cd), nor draws by nation (+0x0b bit 4, 0x51c5d4), nor shuffles (0x51e220, skipped at
 * 0x51c5e4..0x51c5ec), so the tie keeps the list's order, first team at home (as season_cup_groups.c for its first
 * round). Cup::set_home_teams_for_round 0x51e870 swaps only for +0x0b bits 1 and 8 or a host nation (+0x24, 0x51eac3),
 * none of which this cup has (+0x24 is -1 in a 2004 run). The draw on the first of the month of the first leg. */
static uint32_t final_round(int year, SeasonNlDate leg)
{
    uint32_t rec = game_malloc(CUP_ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    uint32_t f[7] = {rec, 0, 1, (uint32_t)(leg.month - 1), (uint32_t)leg.year, 0xffffffffu, (uint32_t)year};
    call_cdecl(CUP_FIRST_DATE, 7, f);
    if (rt_unwinding) return rec;
    uint32_t l[9] = {rec, 0, (uint32_t)leg.day, (uint32_t)(leg.month - 1), (uint32_t)leg.year, 0xffffffffu, 1,
                     (uint32_t)year, 0};
    call_cdecl(CUP_LAST_DATE, 9, l);
    if (rt_unwinding) return rec;
    wr16(g_mem, rec + 7u, 0x96);
    wr16(g_mem, rec + 9u, 0);
    wr16(g_mem, rec + 0xbu, 0);
    wr16(g_mem, rec + 0xdu, 0x200);
    wr16(g_mem, rec + 0xfu, 3);
    wr8(g_mem, rec + 0x17u, 0xa);
    wr16(g_mem, rec + 0x18u, 2);
    wr16(g_mem, rec + 0x1au, 1);
    wr16(g_mem, rec + 0x1cu, 2);
    wr16(g_mem, rec + 0x1eu, 0);
    wr8(g_mem, rec + 0x20u, 0);
    wr8(g_mem, rec + 0x21u, 2);
    wr8(g_mem, rec + 0x22u, SECOND_LEG);
    wr32(g_mem, rec + 0x5cu, 0);
    wr32(g_mem, rec + 0x60u, 0);
    wr32(g_mem, rec + 0x64u, 0);
    return rec;
}

static uint32_t table_club(uint32_t self, int row)
{
    uint32_t a[1] = {(uint32_t)row};
    uint32_t r = call_this(TABLE_ROW, self, 1, a);
    return !rt_unwinding && readable(r) ? rd32(g_mem, r) : 0;
}

/* A save that crossed builds (the browser's report of 25 November 2026: a 2001 game with changing rules on, saved in
 * April 2008). The 2008 edition was set up (set_up_rules, when the 2004 edition ended) by a build without the edition
 * of one group, so it kept +0x2c 2, the original's draw date (5 March), one meeting and the +0xa7 of the groups of 3
 * of 2004 (2, the fault season_intl_ofc_rules mends for a new edition), and drawn by this build, which drew the group
 * of 4 and made no group B (ofc_group_b). With +0xa7 2 the group was complete after two of its three rounds (16 and
 * 18 June 2008); update_for_date then called the original's advance (+0x30 < +0x2c - 1, 0x6849d1..0x6849e0), which
 * steps +0x30 from -1 to 0, group B's place (0x799a4b..0x799a4c), and on its next pass called stage[0], a null
 * pointer, through vtable +0x14 (0x6848ee..0x6848f8): "call to 0, which is not a recompiled function (from 0x6848fb)".
 * So the edition of one group with +0x2c above 0, +0x30 -1 and no stage in slot 0 takes the stage count 0 that
 * season_intl_ofc_rules gives it, and +0xa7 the group's own count, (rows +0x3e - 1) x meetings +0x3c (the League's
 * rule, 0x684817..0x684830), or -1 before the draw so that the draw works it out (init_table_row 0x687453). The League
 * then ends the edition after the group's last round (0x6849e8..0x684a9e) with the group's honours
 * (season_intl_ofc_honours). It runs at the load (season_intl_ofc_loaded), and in advance_stage for an object the load
 * did not reach. The meetings and the draw date stay the older build's: the group plays once each, on the days it has.
 * A save whose older build drew the original's two groups of 3 has group B in slot 0 and keeps the original's path. */
static int one_group_unstaged(uint32_t self)
{
    uint32_t stages = rd32(g_mem, self + STAGES);
    return one_group(self) && (int32_t)rd32(g_mem, self + STAGE_COUNT) > 0 &&
           (int32_t)rd32(g_mem, self + CURRENT_STAGE) == -1 && (!readable(stages) || !rd32(g_mem, stages));
}

static void one_group_repair(uint32_t self, const char *when)
{
    int rows = (int16_t)rd16(g_mem, self + GROUP_ROWS), meetings = (int16_t)rd16(g_mem, self + MEETINGS);
    int drawn = rd32(g_mem, self + 0x45u) != 0 && rows > 1 && meetings > 0;
    wr32(g_mem, self + STAGE_COUNT, 0);
    wr16(g_mem, self + TEAM_MATCHES, (uint16_t)(drawn ? (rows - 1) * meetings : -1));
    fprintf(stderr, "recomp: season intl: oceania_nations %d: one group with two stages and no group B (a save of an "
                    "older build): no stage, %d matches each (%s)\n", year_of(self),
            (int16_t)rd16(g_mem, self + TEAM_MATCHES), when);
}

#define ENTRIES 0x00b63d60u             /* league table entries, 0x48 bytes: +0xc the count, +0x10 the competitions */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35

void season_intl_ofc_loaded(void)
{
    if (!season_intl_on()) return;
    for (uint32_t e = 0; e < ENTRY_COUNT; e++) {
        uint32_t at = ENTRIES + e * ENTRY_SIZE, arr = rd32(g_mem, at + 0x10u);
        int32_t n = (int32_t)rd32(g_mem, at + 0xcu);
        for (int32_t k = 0; readable(arr) && k < n && k < 256; k++) {
            uint32_t obj = rd32(g_mem, arr + 4u * (uint32_t)k);
            if (is_ofc(obj) && one_group_unstaged(obj)) one_group_repair(obj, "load");
        }
    }
}

void o_007999d0(void);

/* advance_stage 0x7999d0 (vtable +0x28), thiscall (), ret 0: League::update_for_date 0x6847c0 calls it when the group
 * table and every stage up to +0x30 are complete and +0x30 < +0x2c - 1 (0x6849aa..0x6849e0). The original steps +0x30
 * and at stage 1 calls create_knockout_stage 0x799c50, which takes rows 0 and 1 of both groups. The edition of the
 * group of 6 has no group B (+0x30 stays -1, set at 0x799947; create_group_stage 0x799c1d sets it) and one stage: the
 * final of the group's first two, made here as the original makes its knockout (0x799cb2..0x799e15): a Cup_Stage of 2
 * teams (the group's second first in the list, so at home in the first leg, as the real first leg was in Honiara,
 * the runner-up's home; see final_round), its round record, Cup::set_up_round, in
 * stage[0], and +0x30 0. The stage number (Cup_Stage +0x44, 0x522e5b) is 0, its index: a fixture keeps it (+0x42), and
 * League::add_result takes the stage of a result from the array by it, with an error box when it is above +0x30
 * (0x685dd0..0x685e5e). The class's callbacks read a stage number below 1 as a group (set_club_progress 0x79a49e,
 * build_fate_news_text 0x799306, update_staff_records_for_status 0x799765), so in this edition, which has no group B
 * (stage 0 in the original), their wraps give them 1 for stage 0. The final's first leg is on 9 October of the edition
 * year, or a week after today when that comes later. The edition of one group set up by an older build: see
 * one_group_unstaged. */
RT_WRAP(007999d0, ofc_advance)
{
    uint32_t self = cpu.ecx;
    if (one_group_unstaged(self)) {         /* not repaired at the load (season_intl_ofc_loaded) */
        one_group_repair(self, "advance");
        rt_ret_cdecl(0);
        return;
    }
    if (!six_staged(self) || (int32_t)rd32(g_mem, self + CURRENT_STAGE) != -1) {
        o_007999d0();
        return;
    }
    int year = year_of(self);
    SeasonNlDate leg = first_leg;
    if (abs_of(leg, year) < today_abs() + LEAD_DAYS) leg = date_of(today_abs() + LEAD_DAYS, year);
    uint32_t teams = game_malloc(8), rec = 0, cup = 0, second = 0, first = 0;
    if (rt_unwinding) return;
    if (teams) {
        second = table_club(self, 1);
        if (!rt_unwinding) first = table_club(self, 0);
        if (rt_unwinding) return;
        wr32(g_mem, teams, second);
        wr32(g_mem, teams + 4u, first);
        if (!rt_unwinding) rec = final_round(year, leg);
    }
    if (rt_unwinding) return;
    if (rec) {
        uint32_t na[1] = {0xb2};
        cup = call_cdecl(GAME_NEW, 1, na);
        if (rt_unwinding) return;
    }
    if (readable(cup)) {
        /* The arguments of 0x799d2f..0x799d4c: the rounds, the stage number, 4, the template id (0 for the original's
         * knockout template, 0x798d4e), 0x14. */
        uint32_t ca[15] = {self, 2, teams, 1, rd32(g_mem, self + 4u), rec, (uint32_t)year, 0, 4, 0, 0x14, 0, 0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return;
    } else {
        cup = 0;
    }
    game_free(teams);
    if (!rt_unwinding) game_free(rec);
    if (rt_unwinding) return;
    if (!readable(cup)) {
        fprintf(stderr, "recomp: season intl: oceania_nations %d: no final stage\n", year);
        rt_ret_cdecl(0);
        return;
    }
    wr32(g_mem, rd32(g_mem, self + STAGES), cup);
    uint32_t zero[1] = {0};
    call_this(CUP_START, cup, 1, zero);
    if (rt_unwinding) return;
    wr32(g_mem, self + CURRENT_STAGE, 0);
    if (logging())
        fprintf(stderr, "recomp: season intl: oceania_nations %d: final of two legs from %d/%d/%d, club %d (second, "
                        "home first) v club %d (first)\n", year, leg.day, leg.month, year + leg.year,
                readable(second) ? (int)rd32(g_mem, second) : -1, readable(first) ? (int)rd32(g_mem, first) : -1);
    rt_ret_cdecl(0);
}

void o_00799300(void);
void o_00799760(void);

/* build_fate_news_text 0x799300 (thiscall (club, fate, stage as a byte, ...), ret 0x28) and
 * update_staff_records_for_status 0x799760 (thiscall (club, status, stage as a byte, ...), ret 0x18): the stage byte
 * at +0xc of the entry stack, below 1 a group (0x799306, 0x799765). The final of the group of 6 is stage 0
 * (ofc_advance): it gets the knockout's 1. */
static void final_stage_byte(uint32_t self)
{
    if (six_staged(self) && (int8_t)rd8(g_mem, cpu.esp + 0xcu) == 0) wr8(g_mem, cpu.esp + 0xcu, 1);
}

RT_WRAP(00799300, ofc_fate_news)
{
    final_stage_byte(cpu.ecx);
    o_00799300();
}

RT_WRAP(00799760, ofc_staff_records)
{
    final_stage_byte(cpu.ecx);
    o_00799760();
}

void o_00799a60(void);

/* create_group_stage 0x799a60, thiscall (group), ret 4 (0x799c43): group B. None in the edition of one group, of 4
 * or of 6. */
RT_WRAP(00799a60, ofc_group_b)
{
    if (season_intl_on() && (drawn4(cpu.ecx) || drawn6(cpu.ecx))) {
        rt_ret_stdcall(1, 1);
        return;
    }
    o_00799a60();
}

/* Slot +0x30 0x595160 (the wrap in season_cup_groups.c), thiscall () ret 0: 1 when the class is the edition of one
 * group with no stage (+0x2c 0): League::record_season_honours has run and the call has returned; 1 too for the
 * edition of the group of 6, whose final is stage[0], not stage 1: its Cup::record_season_honours has run. */
int season_intl_ofc_honours(uint32_t self)
{
    /* The log of the matches of each team at the edition's end (tests/seasons/ofc_team_matches_run.py). */
    if (logging() && season_intl_on() && is_ofc(self))
        fprintf(stderr, "recomp: season intl: oceania_nations %d: group A %d rows, %d meetings, %d matches each "
                        "(+0xa7)\n", year_of(self), (int16_t)rd16(g_mem, self + GROUP_ROWS),
                (int16_t)rd16(g_mem, self + MEETINGS), (int16_t)rd16(g_mem, self + TEAM_MATCHES));
    if (six_staged(self)) {
        /* The final in stage[0] (ofc_advance): the original jumps to slot +0x30 of stage 1 (0x595160..0x595168). */
        uint32_t cup = rd32(g_mem, rd32(g_mem, self + STAGES));
        if (!readable(cup)) {               /* the original's jump would read stage[1], past the array of 1 */
            fprintf(stderr, "recomp: season intl: oceania_nations %d: no final, no honours\n", year_of(self));
            rt_ret_cdecl(0);
            return 1;
        }
        uint32_t r = call_this(rd32(g_mem, rd32(g_mem, cup) + 0x30u), cup, 0, NULL);
        if (!rt_unwinding) rt_ret_cdecl(r);
        return 1;
    }
    if (!season_intl_on() || !is_ofc(self) || rd32(g_mem, self + STAGE_COUNT) != 0) return 0;
    call_this(LEAGUE_HONOURS, self, 0, NULL);
    if (!rt_unwinding) rt_ret_cdecl(0);
    return 1;
}

/* ---- Sites of seasons.py ---- */

/* The size of a group: 3 in the original (create_group_table 0x7995a9, 0x7995ae; create_group_stage 0x799a80,
 * 0x799ac2, 0x799acb, 0x799b74), 4 when the draw holds 8 teams or the one group of 4. +0xf2 keeps the count until
 * create_group_stage has run (0x79a2c7, then cleared at 0x79a2e1). */
uint32_t rt_intl_ofc_group(uint32_t self, uint32_t original)
{
    return drawn8(self) || drawn4(self) ? (uint32_t)GROUP : drawn6(self) ? (uint32_t)SIX : original;
}

/* ---- Predicted positions (vtable +0x5c, 0x79a320) ---- */

void o_0079a320(void);

/* The original sorts the teams with 0x4b07d0 (0x79a414) and gives 6 places 1, 2, 3, 3, 5, 5 (0x79a425..0x79a46b).
 * With 8 teams: 1, 2, 3, 3, 5, 5, 7, 7; with the one group of 4: 1, 2, 3, 4; with the group of 6: 1 to 6. */
RT_WRAP(0079a320, ofc_predicted)
{
    static const int pos8[TEAMS] = {1, 2, 3, 3, 5, 5, 7, 7}, pos4[ONE_GROUP] = {1, 2, 3, 4},
                     pos6[SIX] = {1, 2, 3, 4, 5, 6};
    uint32_t self = cpu.ecx, stats = readable(self) ? rd32(g_mem, self + 8u) : 0;
    uint32_t teams = readable(self) ? rd32(g_mem, self + TEAMS_OF) : 0;
    int n = drawn8(self) ? TEAMS : drawn4(self) ? ONE_GROUP : drawn6(self) ? SIX : 0;
    const int *pos = n == TEAMS ? pos8 : n == SIX ? pos6 : pos4;
    if (!n || !readable(stats) || !readable(teams)) {
        o_0079a320();
        return;
    }
    uint32_t list = game_malloc(4u * (uint32_t)n);
    if (rt_unwinding) return;
    if (list) {
        memcpy(g_mem + list, g_mem + teams, 4u * (uint32_t)n);
        uint32_t qa[4] = {list, (uint32_t)n, 4, CLUB_COMPARE};
        call_cdecl(GAME_QSORT, 4, qa);
        if (rt_unwinding) return;
        for (int k = 0; k < n; k++) {
            uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k);
            if (!club) continue;
            uint32_t pa[2] = {club, (uint32_t)pos[k]};
            call_this(SET_FINAL_POSITION, stats, 2, pa);
            if (rt_unwinding) return;
        }
        game_free(list);
        if (rt_unwinding) return;
    }
    rt_ret_stdcall(cpu.eax, 0);
}

/* ---- Group stage codes (vtable +0x68, 0x79a490) ---- */

void o_0079a490(void);

/* thiscall (club, stage as a byte, a, b, c), ret 0x14. In a group stage (stage below 1, 0x79a49e) the original maps
 * the codes of a group of 3: a 1, 2, 3 to 1, 3, 5 (0x79a4d5..0x79a506), b 1 and 2 to 1 and 3 to 5
 * (0x79a4a5..0x79a4b7), c 1 and 2 to 3 and 3 to 5 (0x79a4bd..0x79a4d3), and passes any other value on. A group of 4 (group A's table of 4 rows, +0x3e) gives its
 * fourth the codes of a third. */
RT_WRAP(0079a490, ofc_progress)
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    /* The edition of one group (+0x2c 0, no knockout) and the group of 6: the stage byte 1 takes the original's path
     * of a knockout stage (0x79a49e..0x79a4a3), so the codes are the group's places. The original maps a place in a
     * group of 3 to a place among all 6 teams (1, 3, 5); in one group of 6 the place in the group is that place. */
    if (season_intl_on() && is_ofc(self) && (rd32(g_mem, self + STAGE_COUNT) == 0 || six_staged(self)) &&
        (int8_t)rd8(g_mem, sp + 8u) < 1) {
        wr8(g_mem, sp + 8u, 1);
        o_0079a490();
        return;
    }
    if (season_intl_on() && is_ofc(self) && (int16_t)rd16(g_mem, self + GROUP_ROWS) == GROUP &&
        (int8_t)rd8(g_mem, sp + 8u) < 1)
        for (uint32_t at = 0xcu; at <= 0x14u; at += 4u)
            if (rd8(g_mem, sp + at) == GROUP) wr8(g_mem, sp + at, 3);
    o_0079a490();
}

/* ---- The group template (create_schedule 0x798b60) ---- */

/* The original's group templates (stage -1 group A, 0 group B; 0x798b90, 0x798eec) give each of the 3 days slot 0
 * only. For a draw of 8 each day also gets slot 1, a copy of slot 0, and slot 2 ends the list, as the Asian Cup's
 * days (0x40ebb5, 0x40ec3e, 0x40ec4f). */
static void second_slots(uint32_t rounds)
{
    if (!readable(rounds)) return;
    for (int r = 0; r < GROUP_DAYS; r++) {
        uint32_t s0 = rounds + (uint32_t)r * ROUND_SIZE + 5u;
        uint32_t one[7] = {rounds, (uint32_t)r, 1, (uint32_t)(int8_t)rd8(g_mem, s0 + 4u),
                           (uint32_t)(int8_t)rd8(g_mem, s0 + 5u), (uint32_t)(int8_t)rd8(g_mem, s0 + 6u),
                           rd32(g_mem, s0)};
        uint32_t end[7] = {rounds, (uint32_t)r, 2, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
        call_cdecl(ROUND_SLOT, 7, one);
        if (!rt_unwinding) call_cdecl(ROUND_SLOT, 7, end);
        if (rt_unwinding) return;
    }
}

/* thiscall (stage as a byte, short *rounds, short *id, int *flag), ret 0x10. Called by the wrap of 0x798b60 in
 * season_intl.c inside its date shift, at the entry state. Group B's template is made on the draw day
 * (create_group_stage 0x799afe), after the draw: with 8 drawn it gets the second slots. Group A's is made at the new
 * season (set_up_rules 0x799740, kept at +0xba), before the draw: season_intl_ofc_draw gives it the second slots. The
 * edition of one group takes its own template of 6 days (one_group_template). */
void season_intl_ofc_template(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    int stage = (int8_t)rd8(g_mem, cpu.esp + 4u);
    if (one_group_template(self) || six_template(self)) return;
    original();
    if (rt_unwinding || stage > 0 || !drawn8(self)) return;
    second_slots(cpu.eax);
}
