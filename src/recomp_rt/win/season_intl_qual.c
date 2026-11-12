/* World Cup qualifying 2006 to 2022 (0.6.0 slice 27): the places of each confederation and the
 * inter-confederation play-offs of the cycles 2006, 2010, 2014, 2018 and 2022.
 *
 * Gate: season_intl_on() and the World Cup year y of the campaign, y = campaign +0x40 + 2, in 2006 to 2022.
 * The sections use the same year for their hosts (for example 0x910736, 0x920dae, 0x914003). Outside the gate
 * every function here gives the original's value, so the base game does not change.
 *
 * Places in the original (read in 3.9.68). The exempt count of a section is its hosts of year y plus the holder
 * (the holder is off in a season game, rt_intl_wc_holder).
 * - Europe (wc_europe_league 0x918200): exempt count +0xf6 (0x919e4b..0x919f7c). With 0 exempt there are 10
 *   groups (+0xff = 8 at 0x91a0c2), with 1 or 2 there are 9. The group winners qualify (status 7, 0x9189e1).
 *   The play-off (0x919910) takes the N + 1 best runners-up of stage +0x100: N = 8 for 0 or 1 exempt, else 6
 *   (0x919a7e..0x919a94; template 0x918923). One of them, drawn at random (0x90cfc0 at 0x919b4f), plays the
 *   Asian section (news at 0x919c10; getter 0x91def0). The play-off winners qualify (status 1, 0x918a84).
 * - South America (0x9202b0): direct places +0xbe = 4 - SA hosts (+0xee) - Oceania hosts (+0xef)
 *   (0x921098..0x9210ad), one play-off place (+0xbf = 1).
 * - CONCACAF (0x9149f0): final group (stage 6, 0x917830): direct places 3 - exempt (+0xba, 0x91788a), play-off
 *   places 0 (0x917878). Inference: the second byte of that rules array is the count of play-off places (as in
 *   the Asian final groups, 0x913761, where the play-off place is status 1).
 * - Africa (0x90f210): exempt count +0xba (0x9106ae..0x9107db). 0 exempt: first round of all 50 (0x910133,
 *   0x9101d5), then 5 groups of 5 (0x9109e0), 5 winners. 1 exempt: 4 groups, 4 winners. Total 5 with hosts.
 * - Asia (0x911bf0): exempt count +0xee (0x913f7b..0x9140ac). Final groups (0x913740): 2 direct places with 0
 *   exempt, else 1, and 1 play-off place. Stage 11 (0x913bc1) is the play-off, stage 12 (0x913d50) the tie
 *   with the European runner-up. Total 4.5 with hosts.
 * - Oceania (0x91df90): the winner of the final (0x91f640) plays the South American play-off team in stage 2
 *   (0x91f800). The section adds the winner of stage 2 (0x91e937), of either nation.
 *
 * Real places (int-world.tsv rows 29 to 249): UEFA 13 (plus host Germany 2006, Russia 2018), CONMEBOL 4.5
 * (plus host Brazil 2014), CONCACAF 3.5, CAF 5 (plus host South Africa 2010), AFC 4.5 (plus host Qatar 2022),
 * OFC 0.5. The changes here:
 * - E1: Europe with no host plays a play-off of 6 (3 places): 10 + 3 = 13 (2010, 2014, 2022). With one host
 *   it keeps 9 groups and a play-off of 8: 9 + 4 = 13 plus the host (2006, 2018). The group shapes of 2006,
 *   2010 and 2014 stay those of the original (the count is right, the format is not).
 * - E2, E3: no European runner-up goes to an Asian tie: the N best runners-up play the play-off, the next is out.
 * - A1: Asia keeps the format of no host (4 direct places, 3rd places to the play-off) after its pool leaves
 *   out the host (Qatar 2022).
 * - A2, O1: the ties. Asia stage 12 holds the AFC tie, Oceania stage 2 the other tie.
 * - C1: the 4th of the CONCACAF final group gets the play-off place.
 * - S1: South America keeps 4 direct places with a host (Brazil 2014, a pool of 9).
 * - F1: Africa keeps the format of no host (5 places) after its pool leaves out the host (South Africa 2010),
 *   when the pool holds at least 50 nations.
 *
 * Ties (int-world.tsv and the Wikipedia pages of the inter-confederation play-offs, read 2026-10-18):
 * - 2006: CONCACAF 4th v AFC, CONMEBOL 5th v OFC (int-world-57).
 * - 2010: AFC v OFC, CONCACAF 4th v CONMEBOL 5th (int-world-105).
 * - 2014: AFC v CONMEBOL (Jordan v Uruguay), CONCACAF v OFC (Mexico v New Zealand). The tsv (int-world-153)
 *   says only "drawn". Source:
 *   https://en.wikipedia.org/wiki/2014_FIFA_World_Cup_qualification_(inter-confederation_play-offs)
 * - 2018: CONCACAF v AFC (Honduras v Australia), OFC v CONMEBOL (New Zealand v Peru). The tsv (int-world-201)
 *   says only "drawn". Source:
 *   https://en.wikipedia.org/wiki/2018_FIFA_World_Cup_qualification_(inter-confederation_play-offs)
 * - 2022: AFC v CONMEBOL, CONCACAF v OFC (int-world-249). The real ties were single matches in Qatar in June
 *   2022. Here they keep two legs on the dates of the original's templates (inference: a tie after the World
 *   Cup draw would leave its slot to the draw fill of world_cup seed_teams 0x92f230).
 *
 * A partner that is not known when a tie is built (its section has not decided it) falls back to the
 * original's partner, so the tie always has two teams. Inference, not traced: whether the status bytes that the
 * partner scans read are final when the builders run (Asia stage 12 when stage 11 ends, 0x91351c; Oceania stage
 * 2 when its final ends, 0x91f17b). RECOMP_SEASON_INTL_LOG=1 logs each pairing.
 *
 * Sites in tools/recomp/seasons.py ("World Cup qualifying 2006 to 2022 (0.6.0 slice 27)"). */
#include "win.h"
#include "season_intl.h"
#include "season_wcq26.h"

#include <stdio.h>
#include <stdlib.h>

#define COMPS 0xadadfcu                 /* competition objects by key id ([0xadadfc][key], 0x90fc89) */
#define KEY_OCEANIA 0x9cf76cu           /* key ids (sweep-international section 2) */
#define KEY_CONCACAF 0x9cf770u
#define KEY_ASIA 0x9cf774u
#define KEY_SOUTH_AMERICA 0x9cf778u
#define STAGES 0xcu                     /* stage array of a competition, count at +0x2c */
#define STAGE_COUNT 0x2cu
#define TEAM_COUNT 0x3eu                /* word: teams of a League table or a Cup round */
#define CUP_ROWS 0xa7u                  /* Cup: rows of 6 bytes, +0 the team, +5 the status (0x91f888) */
#define CUP_ROW 6u
#define TABLE 0xb1u                     /* League: rows of 0x3b bytes, +0 the team, +0x37 the status (0x91f8bb) */
#define TABLE_ROW 0x3bu
#define TABLE_STATUS 0x37u
#define STATUS_PLAYOFF 1u               /* status 1: the play-off place or the winner of a tie */
#define CLUB_NATION 0x53u               /* the nation record of a national team (0x9107b1) */
#define NATION_NAME 0x4u

#define FIRST_YEAR 2006
#define LAST_YEAR 2022

enum { AFC, CONCACAF, CONMEBOL, OFC };

/* The ties of each cycle: the partner of the AFC tie (Asia stage 12) and the two teams of the other tie
 * (Oceania stage 2). The original puts the OFC winner in place 0 and the South American team in place 1
 * (0x91f8ab, 0x91f8e5); each place here falls back to the original's team of that place. The sources are in the
 * head comment. */
static const struct { int year; int afc_partner; int other[2]; } ties[] = {
    {2006, CONCACAF, {OFC, CONMEBOL}},
    {2010, OFC, {CONCACAF, CONMEBOL}},
    {2014, CONMEBOL, {OFC, CONCACAF}},
    {2018, CONCACAF, {OFC, CONMEBOL}},
    {2022, CONMEBOL, {OFC, CONCACAF}},
};
#define TIES ((int)(sizeof ties / sizeof ties[0]))

static const char *const conf_name[] = {"AFC", "CONCACAF", "CONMEBOL", "OFC"};

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

/* The World Cup year of a section's campaign, or 0 outside the gate. */
static int wc_year(uint32_t self)
{
    if (!season_intl_on() || !readable(self)) return 0;
    int y = (int16_t)rd16(g_mem, self + 0x40u) + 2;
    return y >= FIRST_YEAR && y <= LAST_YEAR ? y : 0;
}

static int tie_index(int y)
{
    for (int i = 0; i < TIES; i++)
        if (ties[i].year == y) return i;
    return -1;
}

static const char *team_name(uint32_t club)
{
    if (!readable(club)) return "none";
    uint32_t nation = rd32(g_mem, club + CLUB_NATION);
    return readable(nation) ? win_str(nation + NATION_NAME) : "?";
}

/* The competition object of a key global, or 0. */
static uint32_t comp_of(uint32_t key)
{
    uint32_t table = rd32(g_mem, COMPS);
    if (!readable(table)) return 0;
    uint32_t c = rd32(g_mem, table + rd32(g_mem, key) * 4u);
    return readable(c) ? c : 0;
}

/* Stage i of a competition, or 0 when it does not exist yet. */
static uint32_t stage_of(uint32_t comp, uint32_t i)
{
    if (!comp || rd32(g_mem, comp + STAGE_COUNT) <= i) return 0;
    uint32_t stages = rd32(g_mem, comp + STAGES);
    if (!readable(stages)) return 0;
    uint32_t s = rd32(g_mem, stages + i * 4u);
    return readable(s) ? s : 0;
}

/* The team with status 1 in a League table (rows at +0xb1), or 0. */
static uint32_t table_playoff(uint32_t league)
{
    if (!league) return 0;
    int n = (int16_t)rd16(g_mem, league + TEAM_COUNT);
    uint32_t rows = rd32(g_mem, league + TABLE);
    if (!readable(rows)) return 0;
    for (int i = 0; i < n; i++)
        if (rd8(g_mem, rows + (uint32_t)i * TABLE_ROW + TABLE_STATUS) == STATUS_PLAYOFF)
            return rd32(g_mem, rows + (uint32_t)i * TABLE_ROW);
    return 0;
}

/* The team with status 1 in a Cup round (rows at +0xa7), or 0. */
static uint32_t cup_winner(uint32_t cup)
{
    if (!cup) return 0;
    int n = (int16_t)rd16(g_mem, cup + TEAM_COUNT);
    uint32_t rows = rd32(g_mem, cup + CUP_ROWS);
    if (!readable(rows)) return 0;
    for (int i = 0; i < n; i++)
        if (rd8(g_mem, rows + (uint32_t)i * CUP_ROW + 5u) == STATUS_PLAYOFF)
            return rd32(g_mem, rows + (uint32_t)i * CUP_ROW);
    return 0;
}

/* The play-off team of a confederation (not AFC), or 0 when it is not known.
 * - OFC: the winner of the Oceania final, stage[1] (as 0x91f878..0x91f8ab).
 * - CONMEBOL: the South American League row with status 1 (as 0x91f8ad..0x91f8e5).
 * - CONCACAF: the final group, stage[6] (0x9170e7, stored at 0x9179e2 with the stage index of +0x30), row with
 *   status 1 (C1). The rows of a League_Stage are read as the Europe section reads its groups (0x919a06). */
static uint32_t playoff_team(int conf)
{
    switch (conf) {
    case OFC: return cup_winner(stage_of(comp_of(KEY_OCEANIA), 1u));
    case CONMEBOL: return table_playoff(comp_of(KEY_SOUTH_AMERICA));
    case CONCACAF: return table_playoff(stage_of(comp_of(KEY_CONCACAF), 6u));
    }
    return 0;
}

/* The strongest nation of a confederation that is not in the World Cup slots yet: the partner of a tie when
 * neither the team of the cycle nor the original's partner is known (a 2005 start built the Asian tie of 2006
 * before the CONCACAF final group ended, and E3 leaves no European runner-up). The continent of a nation is
 * its pointer at +0x71, its id at +0 of that record, compared with the key continents (sweep section 8). The
 * reputation is the s16 at +0x8e (3.9.68 nation.dat: 9500 for Brazil). The team is national_team 0x53b3d0,
 * taken only when its own nation field names the nation (season_intl.c, team_nation). */
static uint32_t strongest_free(int conf)
{
    static const uint32_t continent_key[] = {0x9cfa0cu, 0x9cfa14u, 0x9cfa1cu, 0x9cfa18u};   /* AFC .. OFC */
    uint32_t nations = rd32(g_mem, 0xae23a8u), wc = comp_of(0x9cf79cu);
    int count = (int)rd32(g_mem, 0xae2350u);
    if (!readable(nations) || count <= 0 || count > 1000) return 0;
    uint32_t slots = wc ? rd32(g_mem, wc + 0x14u) : 0;
    int filled = wc ? (int16_t)rd16(g_mem, wc + 0x36u) : 0;
    uint32_t want = rd32(g_mem, continent_key[conf]), best = 0;
    int best_rep = -1;
    for (int id = 0; id < count; id++) {
        uint32_t nation = nations + (uint32_t)id * 0x122u, c = rd32(g_mem, nation + 0x71u);
        int rep = (int16_t)rd16(g_mem, nation + 0x8eu);
        if (!readable(c) || rd32(g_mem, c) != want || rep <= best_rep) continue;
        Cpu saved = cpu;
        uint32_t args[1] = {nation};
        uint32_t team = rt_callback(0x0053b3d0u, 1, args);
        if (rt_unwinding) return 0;
        cpu = saved;
        if (!readable(team) || rd32(g_mem, team + CLUB_NATION) != nation) continue;
        int in = 0;
        for (int k = 0; readable(slots) && k < filled && k < 64 && !in; k++)
            in = rd32(g_mem, slots + 6u * (uint32_t)k) == team;
        if (in) continue;
        best = team;
        best_rep = rep;
    }
    return best;
}

/* ---- Sites ---- */

/* E1 (0x918923, 0x919a7e): the exempt count that sizes the European play-off. 2 gives a play-off of 6. */
uint32_t rt_wcq_europe_playoff(uint32_t self, uint32_t exempt)
{
    return wc_year(self) && exempt == 0u ? 2u : exempt;
}

/* E2, E3 (0x919b4f, 0x919b5e): 1 when no European runner-up goes to an Asian tie. */
uint32_t rt_wcq_europe_no_asia(uint32_t self)
{
    return wc_year(self) ? 1u : 0u;
}

/* A1 (0x9141ea): after the Asian pool leaves out the hosts, the format of no host. */
void rt_wcq_asia_format(uint32_t self)
{
    if (!wc_year(self) || rd8(g_mem, self + 0xeeu) == 0u) return;
    if (logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Asia plays the format of no host (%u left out)\n",
                wc_year(self), rd8(g_mem, self + 0xeeu));
    wr8(g_mem, self + 0xeeu, 0);
}

/* The ties built in this run, for the draw of the finals (season_intl.c, fill_slots): a tie can end after the
 * World Cup draw. In a 2001 game that jumps to 2010 the Oceania final ended so late that the CONCACAF v CONMEBOL
 * tie (Oceania stage 2) was built after mid November 2009 and played after the draw of December 2009, which then
 * held 31 teams and stopped at "world_cup.cpp 1438" (0x92f24c, 0x92f2ac). Not in the save: after a load the
 * ties are read again from their stages (season_intl_wc_ties). */
static struct { int year; uint32_t team[2]; } built[8];
static int n_built;

static void note_tie(int y, uint32_t a, uint32_t b)
{
    for (int i = 0; i < n_built; i++)
        if (built[i].year == y && built[i].team[0] == a && built[i].team[1] == b) return;
    if (n_built == (int)(sizeof built / sizeof built[0])) {           /* the oldest goes */
        for (int i = 1; i < n_built; i++) built[i - 1] = built[i];
        n_built--;
    }
    built[n_built].year = y;
    built[n_built].team[0] = a;
    built[n_built].team[1] = b;
    n_built++;
}

/* A load and a new game start with no ties (a second load in the same run kept the first game's teams). */
void season_intl_qual_reset(void) { n_built = 0; }

/* The two teams of a tie stage (a Cup round of 2 teams), 1 when found. */
static int stage_pair(uint32_t st, uint32_t *pair)
{
    if (!readable(st) || (int16_t)rd16(g_mem, st + TEAM_COUNT) != 2) return 0;
    uint32_t rows = rd32(g_mem, st + CUP_ROWS);
    if (!readable(rows)) return 0;
    pair[0] = rd32(g_mem, rows);
    pair[1] = rd32(g_mem, rows + CUP_ROW);
    return readable(pair[0]) && readable(pair[1]);
}

int season_intl_wc_ties(int year, uint32_t *teams, int max)
{
    int n = 0;
    for (int i = 0; i < n_built && n < max; i++)
        if (built[i].year == year) {
            teams[2 * n] = built[i].team[0];
            teams[2 * n + 1] = built[i].team[1];
            n++;
        }
    if (n) return n;
    /* After a load the list is empty, but the save holds the tie stages: the AFC tie, Asia stage 12, and the
     * other tie, Oceania stage 2 (load sweep). */
    static const struct { uint32_t key, stage; } tie_stages[2] = {{KEY_ASIA, 12u}, {KEY_OCEANIA, 2u}};
    for (int i = 0; i < 2 && n < max; i++) {
        uint32_t comp = comp_of(tie_stages[i].key), pair[2];
        /* Only a built stage: the tie stage goes into slot +0x30 (Asia 0x913eba, Oceania 0x91f9d4). */
        if (!comp || wc_year(comp) != year || (int32_t)rd32(g_mem, comp + 0x30u) < (int32_t)tie_stages[i].stage ||
            !stage_pair(stage_of(comp, tie_stages[i].stage), pair)) continue;
        teams[2 * n] = pair[0];
        teams[2 * n + 1] = pair[1];
        n++;
    }
    return n;
}

/* The team of a real tie (O259 A16): in a start after the qualifying began, the tie of the real draw
 * (season_intl_real_tie) when the data holds its team, else 0. */
static uint32_t real_tie_team(int y, int k)
{
    if (!season_intl_late_start("world_cup", y)) return 0;
    const char *name = season_intl_real_tie(y, k);
    return name ? season_intl_team_named(name) : 0;
}

/* A2 (0x913e42): the partner of the Asian team in stage 12. asian_at holds the Asian team (ebp, stored at
 * 0x913e31), original is the European runner-up of 0x91def0. In a start after the qualifying began both teams
 * are the real tie's: the sections that the game plays from its start did not decide them (O259 A16). */
uint32_t rt_wcq_asia_partner(uint32_t self, uint32_t asian_at, uint32_t original)
{
    int y = wc_year(self), t = tie_index(y);
    if (t < 0) return original;
    uint32_t real_afc = real_tie_team(y, 0), real_partner = rt_unwinding ? 0 : real_tie_team(y, 1);
    if (rt_unwinding) return original;
    if (real_afc && real_partner) {
        wr32(g_mem, asian_at, real_afc);
        note_tie(y, real_afc, real_partner);
        if (logging())
            fprintf(stderr, "recomp: season intl: wc play-off %d: %s v %s (the real tie)\n", y, team_name(real_afc),
                    team_name(real_partner));
        return real_partner;
    }
    uint32_t asian = rd32(g_mem, asian_at);
    uint32_t p = playoff_team(ties[t].afc_partner);
    if (!p) {
        if (logging())
            fprintf(stderr, "recomp: season intl: wc play-off %d: %s team not known, the original partner or the strongest free nation plays\n",
                    y, conf_name[ties[t].afc_partner]);
        p = original ? original : strongest_free(ties[t].afc_partner);
    }
    note_tie(y, asian, p);
    if (logging())
        fprintf(stderr, "recomp: season intl: wc play-off %d: %s v %s\n", y, team_name(asian), team_name(p));
    return p;
}

/* O1 (0x91f8e9): the two teams of Oceania stage 2. teams is the array of 0x91f800 (ebx), found the count of
 * the original's picks (ebp). Gives the count of teams in the array. */
uint32_t rt_wcq_oceania_pair(uint32_t self, uint32_t teams, uint32_t found)
{
    static const int original[2] = {OFC, CONMEBOL};
    int y = wc_year(self), t = tie_index(y);
    if (t < 0 || !readable(teams)) return found;
    uint32_t pick[2];
    pick[0] = real_tie_team(y, 2);
    pick[1] = rt_unwinding ? 0 : real_tie_team(y, 3);
    if (rt_unwinding) return found;
    if (pick[0] && pick[1]) {
        wr32(g_mem, teams, pick[0]);
        wr32(g_mem, teams + 4u, pick[1]);
        note_tie(y, pick[0], pick[1]);
        if (logging())
            fprintf(stderr, "recomp: season intl: wc play-off %d: %s v %s (the real tie)\n", y, team_name(pick[0]),
                    team_name(pick[1]));
        return 2u;
    }
    for (int i = 0; i < 2; i++) {
        int conf = ties[t].other[i];
        pick[i] = playoff_team(conf);
        if (!pick[i] && conf != original[i]) {
            pick[i] = playoff_team(original[i]);
            if (logging())
                fprintf(stderr, "recomp: season intl: wc play-off %d: %s team not known, the %s team plays\n", y,
                        conf_name[conf], conf_name[original[i]]);
        }
    }
    for (int i = 0; i < 2; i++)
        if (!pick[i]) pick[i] = strongest_free(ties[t].other[i]);
    if (!pick[0] || !pick[1]) return found;     /* as the original: its box at 0x91f917 */
    wr32(g_mem, teams, pick[0]);
    wr32(g_mem, teams + 4u, pick[1]);
    note_tie(y, pick[0], pick[1]);
    if (logging())
        fprintf(stderr, "recomp: season intl: wc play-off %d: %s v %s\n", y, team_name(pick[0]), team_name(pick[1]));
    return 2u;
}

/* C1 (0x917878): the play-off places of the CONCACAF final group (0 in the original). */
uint32_t rt_wcq_concacaf_playoff(uint32_t self)
{
    if (season_wcq26(self)) return 2u;          /* slice 29: the 4th and 5th go to the 2026 play-off tournament */
    return wc_year(self) ? 1u : 0u;
}

/* S1 (0x9210a2): the South American hosts that the direct places lose (+0xee in dl). */
uint32_t rt_wcq_south_america_hosts(uint32_t self, uint32_t hosts)
{
    return wc_year(self) ? 0u : hosts;
}

/* F1 (0x9108d8): after the African pool (count in ebx, sorted later at 0x9108fc) leaves out the hosts, the
 * format of no host when the pool holds at least 50 nations: exempt +0xba 0, pool size +0xb6 50. The first round
 * then takes the 50 of the pool (0x9101d9). */
void rt_wcq_africa_format(uint32_t self, uint32_t pool)
{
    if (!wc_year(self) || rd8(g_mem, self + 0xbau) == 0u) return;
    if (pool < 50u) {
        if (logging())
            fprintf(stderr, "recomp: season intl: wc qualifying %d: Africa pool of %u, the host format stays\n",
                    wc_year(self), pool);
        return;
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Africa plays the format of no host (%u left out)\n",
                wc_year(self), rd8(g_mem, self + 0xbau));
    wr8(g_mem, self + 0xbau, 0);
    wr32(g_mem, self + 0xb6u, 50u);
}

/* ---- The end of the CONMEBOL and CONCACAF sections (O259 A16, A8) ---- */

/* The Asian tie is built when the Asian section reaches stage 12, about 16 to 22 October of the year before the
 * finals, and the Oceania tie in mid-November (the 2001 to 2027 jump of 2026-10-23). The original's templates end
 * the South American league (0x920590) and the CONCACAF final group (stage 6 of 0x914de0, rounds from 0x91539f)
 * in November, so a tie took its partner before that section ended: "team not known" in 2006, 2014, 2018, a
 * United States that then qualified directly (2014) and a Paraguay that did (2022), each a short draw. Both
 * sections ended in mid-October before the play-offs of 2005, 2009, 2013 and 2017 (CONMEBOL 10 to 15 October,
 * the CONCACAF hexagonal 10 to 15 October; https://en.wikipedia.org/wiki/2014_FIFA_World_Cup_qualification and
 * the pages of 2006, 2010, 2018). So in a season game the rounds of both templates move earlier by whole weeks
 * (the weekday stays) until the last one is on or before the real last matchday of the cycle (real_end below).
 * The Asian tie (from about 16 October) then finds its partner. The Oceania tie is built when the Oceania
 * final ends, which falls from mid-September (2014 cycle) to mid-November, so it waits for its partners below
 * (wcq_oceania_advance). A round
 * record of a League template (league_round_set_date 0x68a160) is 0x41 bytes: the day of the year at +0 (s16,
 * 0x68a1a5) and the year less the base year at +2 (s16, 0x68a1ac); the base year is the campaign year +0x40
 * (0x9206a0). */
static long day_number(int y, int doy)       /* days from 1 January 1970 to day doy of year y */
{
    long d = 0;
    for (int k = 1970; k < y; k++) d += (k % 4 == 0 && (k % 100 != 0 || k % 400 == 0)) ? 366 : 365;
    return d + doy;
}

static int days_of_year(int y) { return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365; }

/* The real last matchday of the South American league and of the CONCACAF final round, the same day in each
 * cycle (O323; read 2026-10-23): 12 October 2005, 14 October 2009, 15 October 2013, 10 October 2017
 * (https://en.wikipedia.org/wiki/2006_FIFA_World_Cup_qualification_(CONMEBOL) and the pages of 2010, 2014 and
 * 2018, the matchday 18 section; the CONCACAF pages of the same years, the last round). The 2002 cycle (14
 * November 2001, the play-off on 20 and 25 November) keeps the original's template: wc_year starts at 2006. The
 * 2022 cycle ended on 29 and 30 March 2022 with the play-offs in June, but the game keeps the original's tie dates
 * of November 2021 for it, so its partners end by 10 October 2021 (a data default). Day of the year from 0. */
static int real_end(int y)
{
    static const struct { int year, month, day; } ends[] = {
        {2006, 10, 12}, {2010, 10, 14}, {2014, 10, 15}, {2018, 10, 10}, {2022, 10, 10},
    };
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    for (size_t i = 0; i < sizeof ends / sizeof ends[0]; i++)
        if (ends[i].year == y) {
            int py = y - 1, leap = py % 4 == 0 && (py % 100 != 0 || py % 400 == 0);
            return before[ends[i].month - 1] + (ends[i].month > 2 ? leap : 0) + ends[i].day - 1;
        }
    return -1;
}

static void end_by_october(uint32_t self, uint32_t tpl, int rounds, const char *what)
{
    int y = wc_year(self), base = (int16_t)rd16(g_mem, self + 0x40u), end = real_end(y);
    if (!y || end < 0 || !readable(tpl) || rounds <= 0 || rounds > 64) return;
    long last = 0, target = day_number(y - 1, end);
    for (int r = 0; r < rounds; r++) {
        uint32_t rec = tpl + (uint32_t)r * 0x41u;
        long d = day_number(base + (int16_t)rd16(g_mem, rec + 2u), (int16_t)rd16(g_mem, rec));
        if (d > last) last = d;
    }
    if (last <= target) return;
    long shift = (last - target + 6) / 7 * 7;
    for (int r = 0; r < rounds; r++) {
        uint32_t rec = tpl + (uint32_t)r * 0x41u;
        int yr = base + (int16_t)rd16(g_mem, rec + 2u);
        long doy = (int16_t)rd16(g_mem, rec) - shift;
        while (doy < 0) doy += days_of_year(--yr);
        wr16(g_mem, rec, (uint16_t)doy);
        wr16(g_mem, rec + 2u, (uint16_t)(yr - base));
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: %s ends %ld days earlier (by day %d of %d)\n", y,
                what, shift, end + 1, y - 1);
}

void o_00920590(void);
void o_00914de0(void);

/* WcSouthAmericanLeague::create_schedule, thiscall (stage, short *round_count, short *, int *) ret 0x10, returns
 * the template. */
RT_WRAP(00920590, wcq_conmebol_schedule)
{
    uint32_t self = cpu.ecx, count = rt_arg(1);
    o_00920590();
    if (rt_unwinding || !readable(count)) return;
    uint32_t eax = cpu.eax;
    end_by_october(self, eax, (int16_t)rd16(g_mem, count), "CONMEBOL");
    cpu.eax = eax;
}

/* WcConcacafCup::create_round_table, thiscall (stage, short *round_count, short *, int *) ret 0x10: the final
 * group, stage 6. */
RT_WRAP(00914de0, wcq_concacaf_rounds)
{
    uint32_t self = cpu.ecx, stage = rt_arg(0) & 0xffu, count = rt_arg(1);
    o_00914de0();
    if (rt_unwinding || stage != 6u || !readable(count)) return;
    uint32_t eax = cpu.eax;
    end_by_october(self, eax, (int16_t)rd16(g_mem, count), "the CONCACAF final group");
    cpu.eax = eax;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* WcOceaniaLeague::advance_stage, thiscall () ret 0. From the final (stage 1, +0x30) to the play-off the original
 * waits until the South American section is complete (its slot +0x10 with (1, 1), 0x91f0d9..0x91f0f8: no
 * advance while it gives 0). The tie of a cycle can have a CONCACAF partner (season_intl_qual.c's ties table), so
 * in a season game it waits for the CONCACAF section too (the same slot of WcConcacafCup). Without it the 2014
 * cycle built its tie on 13 September 2013, before the CONCACAF final group ended, and the CONMEBOL team played
 * both ties (O259 A16). Returns 1 to wait (the wrap of 0x91f0d0 in season_wcq26.c then returns without the
 * original). */
int season_intl_oceania_wait(uint32_t self)
{
    int y = wc_year(self), t = tie_index(y);
    if (t >= 0 && (int32_t)rd32(g_mem, self + 0x30u) == 1 &&
        (ties[t].other[0] == CONCACAF || ties[t].other[1] == CONCACAF)) {
        uint32_t cc = comp_of(KEY_CONCACAF);
        if (readable(cc)) {
            uint32_t args[2] = {1, 1};
            uint32_t done = call_this(rd32(g_mem, rd32(g_mem, cc) + 0x10u), cc, 2, args) & 0xffu;
            return !rt_unwinding && !done;
        }
    }
    return 0;
}
