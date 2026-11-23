/* World Cup qualifying from the 2026 cycle (0.6.0 slice 29): the places of each confederation for a World Cup of
 * 48 teams and the play-off tournament of six. Research and evidence: docs/research/2026-10-19-world-cup-48.md,
 * section "Qualifying" (the sites, read in 3.9.68).
 *
 * Gate (wcq26): season_intl_on(), the World Cup year y = campaign +0x40 + 2 is 2026 or later, and the rule
 * edition of that year has 48 teams (with changing rules off a game that starts before 2025/26 keeps the
 * original's shape, Q232). Outside the gate each site gives the original's value, so the base game does not
 * change. Slice 27's sites (season_intl_qual.c) act for 2006 to 2022 only.
 *
 * Places of 2026 (int-world-268 to int-world-297): UEFA 16, CONMEBOL 6 + 1, CONCACAF 3 + 2 (the three hosts
 * qualify as hosts), CAF 9 + 1, AFC 8 + 1, OFC 1 + 1; the play-off tournament of the six gives 2. The shapes:
 * - UEFA (wc_europe_league): 12 groups (pool of 50, capped at 0x919e41 / 0x91a04a: the last two groups of 5,
 *   the others of 4), the winners qualify; 4 single-match paths of 4 in March 2026 from the 12 runners-up and
 *   the 4 best 2024-25 Nations League group winners that were not in the top two of their group. The real 2025
 *   matchdays (groups of 5 from March, of 4 from September). Built on slice 28's 2022 code (season_nl_wc.c).
 * - CONMEBOL: one league of 10, the top 6 qualify, the 7th to the tournament.
 * - CONCACAF: the original's rounds with the three hosts left out (the third through the holder's place of the
 *   pool, 0x916394); the final group of 6 gives 3 direct places and 2 to the tournament.
 * - CAF: a first round of the weakest (2 x (pool - 45) teams), then 9 groups of 5 (8 in 2030: O265); the winners
 * qualify, the best
 *   runner-up goes to the tournament (real: 9 groups of 6 and a play-off of 4 runners-up, left out).
 * - AFC: the original's rounds; the two final groups of 5 give 4 direct places each, their fifths play the
 *   original's play-off (stage 11), its winner to the tournament; no tie with Europe (stage 12). With hosts
 *   8 less its hosts, group A the odd one; the row after them plays stage 11.
 * - OFC: the final's winner qualifies, its loser goes to the tournament. With a host the winner goes to the
 *   tournament.
 * - The tournament: Oceania stages 2 (semi-finals) and 3 (finals), single matches on 26 and 31 March. */
#include "win.h"
#include "season.h"
#include "season_intl.h"
#include "season_nl.h"
#include "season_wcq26.h"
#include "season_ofc_first.h"
#include "season_concacaf26.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPS 0xadadfcu
#define KEY_WORLD_CUP 0x9cf79cu
#define KEY_OCEANIA 0x9cf76cu
#define KEY_CONCACAF 0x9cf770u
#define KEY_ASIA 0x9cf774u
#define KEY_SOUTH_AMERICA 0x9cf778u
#define KEY_AFRICA 0x9cf77cu
#define KEY_EUROPE 0x9cf780u
#define VT_AFRICA 0x00970ad0u
#define VT_ASIA 0x00970b70u
#define VT_CONCACAF 0x00970c24u
#define VT_EUROPE 0x00970cc4u
#define VT_OCEANIA 0x00970d78u
#define VT_SOUTH_AMERICA 0x00970e2cu
#define NATIONS 0xae23a8u
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define GAME_DATE 0xae2c90u             /* s16 day of the year, s16 year */

#define GAME_NEW 0x00944cf1u
#define GAME_MALLOC 0x00944e46u
#define GAME_FREE 0x009452cau
#define GAME_RANDOM 0x0090cfc0u         /* cdecl (n): 0 to n - 1 (0x918ff4) */
#define TABLE_SORT 0x006827d0u
#define CUP_STAGE_NEW 0x00522e00u
#define CUP_START 0x0051c800u
#define NATIONAL_TEAM 0x0053b3d0u

#define F_RECORD 0x4u
#define F_STAGES 0xcu
#define F_STAGE_COUNT 0x2cu
#define F_STAGE 0x30u
#define F_TEAMS 0x3eu
#define F_YEAR 0x40u
#define F_TABLE 0xb1u
#define F_CUP_ROWS 0xa7u                /* Cup: rows of 6 bytes, +0 the team, +5 the status */
#define ROW_SIZE 0x3bu
#define ROW_POINTS 0x19u
#define ROW_STATUS 0x37u
#define ROUND_SIZE 0x68u
#define LEAGUE_ROUND 0x41u

/* Europe (wc_europe_league) */
#define EU_POOL 0xeeu                   /* the pool, sorted by reputation (0x91a0b0) */
#define EU_POOL_SIZE 0xf2u
#define EU_EXEMPT 0xf6u
#define EU_GRID 0xf7u                   /* the draw grid: group * 6 + place (0x918ef1, 0x91908b) */
#define EU_LAST_GROUP 0xffu
#define EU_RUNNERS_UP 0x100u
#define EU_PLAYOFF 0x101u
#define EU_GROUPS 12
#define EU_PATHS 4
#define EU_PATH_TEAMS 16

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

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + F_YEAR); }

/* The gate: a section whose campaign is for a World Cup of 48 teams in a season game. */
int season_wcq26(uint32_t self)
{
    if (!season_intl_on() || !readable(self)) return 0;
    int y = year_of(self) + 2;
    if (y < 2026) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition("world_cup", y);
    /* A campaign that a save of 0.6.1 or older made in the original's shape keeps it (0.6.2 slice 4b, item 58). */
    return e && e->teams == 48 && !season_intl_old_shape(self);
}

static int is(uint32_t self, uint32_t vtable) { return readable(self) && rd32(g_mem, self) == vtable; }

static uint32_t comp_of(uint32_t key)
{
    uint32_t table = rd32(g_mem, COMPS);
    if (!readable(table)) return 0;
    uint32_t c = rd32(g_mem, table + rd32(g_mem, key) * 4u);
    return readable(c) ? c : 0;
}

static uint32_t stage_of(uint32_t comp, int i)
{
    if (!comp) return 0;
    if (i < 0) return comp;
    if ((int)rd32(g_mem, comp + F_STAGE_COUNT) <= i) return 0;
    uint32_t stages = rd32(g_mem, comp + F_STAGES);
    uint32_t s = readable(stages) ? rd32(g_mem, stages + 4u * (uint32_t)i) : 0;
    return readable(s) ? s : 0;
}

static uint32_t table_club(uint32_t st, int pos)
{
    if (!readable(st)) return 0;
    uint32_t table = rd32(g_mem, st + F_TABLE);
    int teams = (int16_t)rd16(g_mem, st + F_TEAMS);
    return readable(table) && pos < teams ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

static const char *team_name(uint32_t club)
{
    uint32_t nation = readable(club) ? rd32(g_mem, club + 0x53u) : 0;
    return readable(nation) ? win_str(nation + 4u) : "none";
}

/* ---- The places with hosts (O265, O259 A11; 0.6.2 decision A) ---- */

/* A World Cup with more than three hosts (2030: Morocco, Portugal, Spain, Argentina, Paraguay, Uruguay). The
 * sections take their hosts through find_hosts 0x5fa730, which gives two (pair 1 of the host row: 0x91630c,
 * 0x920eea, 0x919ee3), so the South American hosts played their league and the European paths took Portugal (a
 * December 2025 to 2030 run: 47 qualified and 1 filled). Decision O265: each confederation keeps its 2026 total
 * with its hosts counted in it: UEFA 14 + 2 hosts (12 group winners, 2 paths), CONMEBOL 3 + 3 hosts + 1 to the
 * tournament, CAF 8 + Morocco (8 groups), CONCACAF 6 + 2 to the tournament, AFC 8 + 1, OFC 1 + 1, the tournament
 * 2: 48 in all. Every site below takes all the hosts from season_intl_host_ids.
 * The editions after the table (0.6.2 decision A, fault 32) keep 48 teams and the same rule: each confederation's
 * 2026 total (UEFA 16, CONMEBOL 6, CONCACAF 6, CAF 9, AFC 8, OFC 1) less its hosts in that edition
 * (season_wcq26_places; the AFC and OFC from 0.6.2 slice 5, open item 59: Saudi Arabia 2034). 2026 (three
 * CONCACAF hosts) and 2030 give the shapes above; an edition with no host gives UEFA 12 + 4 paths, CONMEBOL 6,
 * CONCACAF 6 and CAF 9 groups: 48 with the AFC 8, the OFC 1 and the tournament 2. */
static int hosts_in(uint32_t self, uint32_t continent_key);

static int conf_hosts(uint32_t self, uint32_t continent_key)
{
    return season_wcq26(self) ? hosts_in(self, continent_key) : 0;
}

/* The hosts of the campaign's World Cup whose continent is the key continent (a global as 0x9cfa1c), as nation
 * records. Returns the count. */
static int hosts_on(uint32_t self, uint32_t continent_key, uint32_t *out, int max)
{
    int ids[6], n = season_intl_host_ids(rd32(g_mem, KEY_WORLD_CUP), year_of(self) + 2, ids), k = 0;
    for (int i = 0; i < n && k < max; i++) {
        uint32_t nation = rd32(g_mem, NATIONS) + (uint32_t)ids[i] * NATION_SIZE, c = rd32(g_mem, nation + 0x71u);
        if (readable(c) && rd32(g_mem, c) == rd32(g_mem, continent_key)) out[k++] = nation;
    }
    return k;
}

static int hosts_in(uint32_t self, uint32_t continent_key)
{
    uint32_t hosts[6];
    return hosts_on(self, continent_key, hosts, 6);
}

/* 1 when the nation id is a host of the campaign's World Cup. */
static int host_id(uint32_t self, int id)
{
    int ids[6], n = season_intl_host_ids(rd32(g_mem, KEY_WORLD_CUP), year_of(self) + 2, ids);
    for (int i = 0; i < n; i++)
        if (ids[i] == id) return 1;
    return 0;
}

#define CONTINENT_ASIA 0x9cfa0cu
#define CONTINENT_OCEANIA 0x9cfa18u

/* The continent key of each confederation (WCQ_UEFA to WCQ_OFC), as the sections compare them (Europe 0x919ee3,
 * South America 0x920eea, North America 0x91630c, Africa 0x9108aa, Asia 0x914041, Oceania 0x91fb62). */
static const uint32_t conf_continent[WCQ_CONFS] = {0x9cfa10u, 0x9cfa1cu, 0x9cfa14u, 0x9cfa08u, CONTINENT_ASIA,
                                                   CONTINENT_OCEANIA};
static const char *const conf_name[WCQ_CONFS] = {"UEFA", "CONMEBOL", "CONCACAF", "CAF", "AFC", "OFC"};

/* The direct places of confederation `conf` (hosts not counted) in the campaign's World Cup, from the hosts of
 * every confederation (season_wcq26_places: the AFC loses a place to a second OFC host). The test log names each
 * new count of a campaign with the game date, so a run shows that a section reads its hosts after their draw
 * (the rows after the table are drawn 5 years before the finals, each section reads them from its campaign's
 * start, 4 years before). */
static int places_of(uint32_t self, int conf, int *hosts_out)
{
    int h[WCQ_CONFS], direct[WCQ_CONFS];
    for (int c = 0; c < WCQ_CONFS; c++) h[c] = conf_hosts(self, conf_continent[c]);
    season_wcq26_places(h, direct);
    if (hosts_out) *hosts_out = h[conf];
    static int seen_year[WCQ_CONFS], seen[WCQ_CONFS];
    int y = year_of(self) + 2;
    if (logging() && (seen_year[conf] != y || seen[conf] != h[conf] + 1)) {
        seen_year[conf] = y;
        seen[conf] = h[conf] + 1;
        fprintf(stderr, "recomp: season intl: wc qualifying %d: %s reads %d hosts on day %d of %d: %d direct places, "
                        "%d with its hosts\n", y, conf_name[conf], h[conf], (int16_t)rd16(g_mem, GAME_DATE),
                (int16_t)rd16(g_mem, GAME_DATE + 2), direct[conf], direct[conf] + h[conf]);
    }
    return direct[conf];
}

/* The direct places of CONCACAF for the 2026 shape (season_concacaf26.c, item 214). */
int season_wcq26_concacaf_places(uint32_t self) { return places_of(self, WCQ_CONCACAF, NULL); }

/* ---- South America ---- */

/* 0x921098 `mov cl, 4` in set_up_rules 0x921040 (this in esi): the direct places +0xbe before the hosts are
 * taken off (0x92109a, 0x9210a2). 6 in the gate; +0xbf = 1 (0x9210e9) gives the 7th status 1. */
uint32_t rt_wcq26_south_america_direct(uint32_t self, uint32_t original)
{
    return season_wcq26(self) ? 6u : original;
}

#define SA_EXEMPT 0xeeu                 /* byte: the hosts and holder of South America (count_exempt_nations) */
#define CONTINENT_SOUTH_AMERICA 0x9cfa1cu
#define CONTINENT_NORTH_AMERICA 0x9cfa14u

void o_00920d10(void);
void o_00920eb0(void);

/* count_exempt_nations 0x920d10, thiscall (): the South American hosts and holder into +0xee, from find_hosts
 * (two). In the gate every South American host counts (2030: three), so set_up_rules 0x921040 gives 6 - 3 = 3
 * direct places (0x92109a, 0x9210a2) and the play-off place to the 4th (0x9210e9). */
RT_WRAP(00920d10, wcq26_conmebol_exempt)
{
    uint32_t self = cpu.ecx;
    o_00920d10();
    if (rt_unwinding || !conf_hosts(self, CONTINENT_SOUTH_AMERICA)) return;
    uint32_t hosts[6];
    int n = hosts_on(self, CONTINENT_SOUTH_AMERICA, hosts, 6);
    if (n > 0) {
        wr8(g_mem, self + SA_EXEMPT, (uint8_t)n);
        if (logging())
            fprintf(stderr, "recomp: season intl: wc qualifying %d: CONMEBOL leaves out %d hosts\n", year_of(self) + 2,
                    n);
    }
}

/* build_team_slots 0x920eb0, thiscall (): the table of 10 - +0xee South American nations, leaving out the two
 * hosts of find_hosts (0x920eea). In the gate every South American host is left out (2030: the third too): their
 * continent pointer is cleared while the original runs, so its continent test passes them over. */
RT_WRAP(00920eb0, wcq26_conmebol_slots)
{
    uint32_t self = cpu.ecx, hosts[6], saved[6];
    int n = season_wcq26(self) ? hosts_on(self, CONTINENT_SOUTH_AMERICA, hosts, 6) : 0;
    for (int k = 0; k < n; k++) {
        saved[k] = rd32(g_mem, hosts[k] + 0x71u);
        wr32(g_mem, hosts[k] + 0x71u, 0);
    }
    cpu.ecx = self;
    o_00920eb0();
    for (int k = 0; k < n; k++) wr32(g_mem, hosts[k] + 0x71u, saved[k]);
}

/* ---- CONCACAF ---- */

/* 0x916394 (after the holder of the pool builder 0x916250, this in esi): the team that the pool counts as
 * exempt in the holder's place (0x9163a1..0x9163ea, compared with the two hosts of find_hosts at 0x9163c1 and
 * 0x9163c5, left out of the fill at 0x9169eb). In the gate: the national team of the third host of the World
 * Cup (find_hosts gives two, 0x91630c), else the holder that slice 27 gives (none in a season game). */
uint32_t rt_wcq26_concacaf_exempt(uint32_t self, uint32_t holder)
{
    if (!season_wcq26(self)) return holder;
    int ids[6], n = season_intl_host_ids(rd32(g_mem, KEY_WORLD_CUP), year_of(self) + 2, ids);
    if (n < 3) return holder;
    uint32_t nation = rd32(g_mem, NATIONS) + (uint32_t)ids[2] * NATION_SIZE;
    uint32_t a[1] = {nation};
    uint32_t team = call_cdecl(NATIONAL_TEAM, 1, a);
    if (rt_unwinding || !readable(team) || rd32(g_mem, team + 0x53u) != nation) return holder;
    uint32_t c = rd32(g_mem, nation + 0x71u);                   /* a host of North America only (not 2030's) */
    if (!readable(c) || rd32(g_mem, c) != rd32(g_mem, 0x9cfa14u)) return holder;
    if (logging()) fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF leaves out host %s\n",
                           year_of(self) + 2, team_name(team));
    return team;
}

/* 0x91785c `sub al, cl` in the final group 0x917830 (this in esi): the direct places 3 - exempt +0xba. In the
 * gate the hosts qualify as hosts and CONCACAF keeps 6 places with its hosts: 6 - hosts direct places of the
 * final group of 6 (2026, three hosts: 3; 2030 and an edition with no host: all six, 3 - 0xfd = 6 in a byte).
 * Gives the count that is taken off. */
uint32_t rt_wcq26_concacaf_hosts(uint32_t self, uint32_t exempt)
{
    if (!season_wcq26(self)) return exempt;
    int h = conf_hosts(self, CONTINENT_NORTH_AMERICA);
    return (uint32_t)(uint8_t)((h > 6 ? 6 : h) - 3);
}

/* ---- Africa ---- */

/* The first round takes 2 x (pool - 45) of the weakest: its winners and the others make 45, 9 groups of 5 (in the
 * 2030 shape 2 x (pool - 40) and 8 groups). A
 * pool below 47 keeps the original's shape (no first round would remain). */
static int africa_groups(uint32_t self)       /* O265: 9 places less the African hosts (8 in 2030) */
{
    return places_of(self, WCQ_CAF, NULL);
}

static int africa_on(uint32_t self)
{
    if (!season_wcq26(self)) return 0;
    int pool = (int)rd32(g_mem, self + 0xb6u);
    if (pool >= 5 * africa_groups(self) + 2 && pool <= 10 * africa_groups(self)) return 1;
    /* The pool holds every African nation less the hosts (item 199, season_caf_pool.c; the original's builder
     * stopped at 52, 0x910896). A first round can take at most twice the group places, and below 47 no first
     * round is left: the original's shape and its 5 places
     * stay, and the fill gives the World Cup's open places (Q388). Logged once a campaign (review 2). */
    static int logged;
    if (logging() && logged != year_of(self)) {
        logged = year_of(self);
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Africa pool of %d, the original's shape stays\n",
                year_of(self) + 2, pool);
    }
    return 0;
}

/* 0x910133 (this in esi), 0x90f6f0 (template, this in edi): the first-round team count, 50 in the original. */
uint32_t rt_wcq26_africa_first(uint32_t self, uint32_t original)
{
    return africa_on(self) ? 2u * ((uint32_t)rd32(g_mem, self + 0xb6u) - 5u * (uint32_t)africa_groups(self)) : original;
}

/* 0x90f357, 0x91058a (the stage count, this in esi), 0x90f768 (template, edi), 0x90fc24 (slot 17, ecx), 0x910a4a
 * (the group count of the group builder 0x9109e0, esi): 5 groups in the original, 9 in the gate. */
uint32_t rt_wcq26_africa_groups(uint32_t self, uint32_t original)
{
    return africa_on(self) ? (uint32_t)africa_groups(self) : original;
}

/* After the pool (slice 27's F1 site 0x9108d8): in the gate the shape of no exempt nation (+0xba 0). The pool
 * count +0xb6 stays the real count. */
void rt_wcq26_africa_format(uint32_t self)
{
    if (!season_wcq26(self) || rd8(g_mem, self + 0xbau) == 0u) return;
    wr8(g_mem, self + 0xbau, 0);
}

/* ---- Asia ---- */

/* The AFC places (8 less its hosts, season_wcq26_places) go to its two final groups of 5, group A (stage 9) the
 * odd one: no host 4 and 4, Saudi Arabia (2034) 4 and 3. The final groups 0x913740 (this in esi) pass one array
 * of four status counts at [esp + 0x10] to both League ctors 0x68b100 (0x91389d, 0x913a5b), which copy it to
 * +0xbe (0x68b1b5..0x68b1cc): 1 direct place, 2 in the original with no host (0x91377f). 0x91377f writes the
 * count of group A; 0x9139c0, after group A is made (0x9138d4) and before group B (0x913a8e), the count of
 * group B. */
static uint32_t asia_group_places(uint32_t self, int g)
{
    return (uint32_t)season_wcq26_asia_group(places_of(self, WCQ_AFC, NULL), g);
}

uint32_t rt_wcq26_asia_direct(uint32_t self, uint32_t original)
{
    return season_wcq26(self) ? asia_group_places(self, 0) : original;
}

uint32_t rt_wcq26_asia_direct_b(uint32_t self, uint32_t current)
{
    return season_wcq26(self) ? asia_group_places(self, 1) : current;
}

/* 0x913bce (group A, stage 9 at +0x24), 0x913bda (group B, stage 10 at +0x28) in the play-off 0x913b60 (this in
 * esi): the row of each final group that plays stage 11, 2 (the third) in the original. In the gate the row after
 * the group's direct places (the fifth with 4 places). */
uint32_t rt_wcq26_asia_playoff_row(uint32_t self, int g, uint32_t original)
{
    return season_wcq26(self) ? asia_group_places(self, g) : original;
}

/* 0x911cf6 (ctor) and after each pool (slice 27's A1 site 0x9141ea): the stage count, 13 in the original
 * (stage 12 is the tie with Europe). 12 in the gate, so League::update_for_date 0x6847c0 ends the section after
 * stage 11 (0x6849aa..0x6849e8). The array holds 13 (malloc at 0x911cf1). */
uint32_t rt_wcq26_asia_stages(uint32_t self, uint32_t original)
{
    return season_wcq26(self) ? 12u : original;
}

void rt_wcq26_asia_format(uint32_t self)
{
    if (!season_wcq26(self)) return;
    wr32(g_mem, self + F_STAGE_COUNT, 12u);
    if (rd8(g_mem, self + 0xeeu)) wr8(g_mem, self + 0xeeu, 0);     /* the shape of no host, as slice 27's A1 */
}

/* ---- The AFC and OFC pools: every host out ---- */

/* The pool builders of Asia 0x913f60 and Oceania 0x91fa80 (thiscall (), from the ctor and start_new_season) count
 * into +0xee the hosts of find_hosts (two, pair 1 of the host row: 0x914013, 0x91fb34) of their continent
 * (0x91404b, 0x91fb6c) and the holder (0x9140ac, 0x91fbcd), make the pool 44 (Asia) or 10 (Oceania) less +0xee
 * (0x9140b2, 0x91fbd3), and take nations of their continent in nation order, less those two hosts and the holder
 * (0x91417d..0x91418b, 0x91fcb2..0x91fcc0). In the gate every host of the continent is out, as the CONMEBOL wrap
 * does: their continent pointer is cleared while the original runs, so neither the host count nor the pool takes
 * them, and the sites 0x9140b2 and 0x91fbd3 add them to the count. The pool builders return 0 when they find fewer
 * nations than the pool (0x9141ab, 0x91fce0), which the callers do not read (0x91e06c, 0x91eff6): logged. */
static int pool_hosts;                  /* the hosts the running pool builder leaves out */

static void pool_build(uint32_t self, uint32_t continent_key, int conf, void (*original)(void))
{
    uint32_t hosts[6], saved[6];
    int n = season_wcq26(self) ? hosts_on(self, continent_key, hosts, 6) : 0;
    if (n) places_of(self, conf, NULL);                     /* the test log of the read */
    for (int k = 0; k < n; k++) {
        saved[k] = rd32(g_mem, hosts[k] + 0x71u);
        wr32(g_mem, hosts[k] + 0x71u, 0);
    }
    pool_hosts = n;
    cpu.ecx = self;
    original();
    pool_hosts = 0;
    for (int k = 0; k < n; k++) wr32(g_mem, hosts[k] + 0x71u, saved[k]);
    if (rt_unwinding || !n) return;
    if (cpu.eax == 0)
        fprintf(stderr, "recomp: season intl: wc qualifying %d: the %s pool found fewer nations than %d\n",
                year_of(self) + 2, conf_name[conf], (int)rd32(g_mem, self + 0xf3u));
    else if (logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: %s pool of %d, %d hosts out\n", year_of(self) + 2,
                conf_name[conf], (int)rd32(g_mem, self + 0xf3u), n);
}

void o_00913f60(void);
void o_0091fa80(void);

RT_WRAP(00913f60, wcq26_asia_pool)
{
    pool_build(cpu.ecx, CONTINENT_ASIA, WCQ_AFC, o_00913f60);
}

RT_WRAP(0091fa80, wcq26_oceania_pool)
{
    uint32_t self = cpu.ecx;
    pool_build(self, CONTINENT_OCEANIA, WCQ_OFC, o_0091fa80);
    if (rt_unwinding) return;
    uint32_t eax = cpu.eax;
    season_ofc_first_pool(self);            /* item 198: every Oceania nation (season_ofc_first.c) */
    if (!rt_unwinding) cpu.eax = eax;
}

/* 0x9140b2 (Asia) and 0x91fbd3 (Oceania), this in esi: the count that the pool size takes off, +0xee. In the
 * gate the hosts that pool_build left out are added. Oceania keeps at most one of them in the count: its second
 * group has 4 teams for a count of 1 and 5 for any other (0x91f1c3..0x91f1d8) and the first always 5
 * (0x91ec0e), so a pool of 9 or 10. Two Oceania hosts (no host row names one) then need 9 others in the data. */
uint32_t rt_wcq26_pool_exempt(uint32_t self, uint32_t exempt)
{
    int n = pool_hosts;
    if (!n || !season_wcq26(self)) return exempt;
    if (is(self, VT_OCEANIA) && (int)exempt + n > 1) n = (int)exempt >= 1 ? 0 : 1;
    uint32_t count = (exempt + (uint32_t)n) & 0xffu;
    wr8(g_mem, self + 0xeeu, count);
    return count;
}

/* ---- Oceania: the stage count of the tournament ---- */

/* 0x91e076 (ctor), 0x91f004 and 0x91f002 (start_new_season, this in esi): 3 stages in the original (groups,
 * final, tie with South America); 4 in the gate (groups, final, the tournament's semi-finals and finals). */
uint32_t rt_wcq26_oceania_stages(uint32_t self, uint32_t original)
{
    return season_wcq26(self) ? 4u : original;
}

/* ---- Europe ---- */

static int europe_on(uint32_t self) { return is(self, VT_EUROPE) && season_wcq26(self); }

/* 1 when the object holds the 12-group shape (13 stages: groups -1 to 10, the runners-up table, the paths; the
 * sites 0x918348 and 0x91950c set it at the ctor and at start_new_season). A campaign that a save made in the
 * old shape (10 groups and the runners-up table, 11 stages: a save of a build before slice 29, the gate 2023 leg
 * loaded in July 2024) keeps the original's code, else its runners-up table 0x91a240 read stage 9 (that table) and
 * stage 10 (none) as groups and passed clubs with no nation to slot +0x68 0x91b510 (box Database.cpp 13126 on day
 * 243 of 2024). The wraps test this; the sites that make the shape test europe_on. */
static int europe_shape(uint32_t self)
{
    return europe_on(self) && (int)rd32(g_mem, self + F_STAGE_COUNT) == EU_GROUPS + 1;
}

/* 0x919f90 (build_entry_pool 0x919e30, after the pool size +0xf2, this in esi): in the gate the shape of no
 * exempt nation (+0xf6 0), so every switch on +0xf6 takes its first branch. */
void rt_wcq26_europe_format(uint32_t self)
{
    if (europe_on(self) && rd8(g_mem, self + EU_EXEMPT)) wr8(g_mem, self + EU_EXEMPT, 0);
}

/* The European pool. build_entry_pool 0x919e30 (from the ctor 0x918339 and start_new_season 0x9194cf; it
 * returns 1, or 0 when it finds fewer nations than its size, 0x91a08c) leaves out the European hosts that
 * find_hosts gives (two at most, 0x919ee3) and takes European nations in nation order until 50 (pool size 0x32
 * less the exempt at 0x919e41, `cmp ebx, 0x32` at 0x91a04a; an array of 50 at 0x919fa9), then sorts them by
 * reputation (0x4b07d0, 0x91a0b0). The pool is not in the save: the ctor builds it at a load too. In the gate
 * this wrap makes the pool again after the original: every European nation whose national team names it, less
 * Russia (suspended, Q288 and Q365: the 2026 qualifying had 54 entrants, int-world-279, and after the table the
 * rule stays, Q248), less an old record that a record with a later name replaces (superseded), and less every
 * host of the World Cup of the table (season_intl_host_ids: the three of 2026 are not European; Spain and Portugal
 * of 2030), sorted by the original's compare. 54 nations give 6 groups of 4
 * and 6 of 5 (europe_group_size), as the real 2026 draw. The fields that the original sets after its sort get
 * the gate's values: +0xf6 0, +0xff 10, +0x100 11, +0x101 12. */
#define POOL_MAX 60                     /* 12 groups of 5 (europe_group_size) */
#define CLUB_COMPARE 0x004b07d0u        /* compare_clubs_by_reputation_desc_nulls_last (0x91a0b0) */
#define GAME_QSORT 0x00945333u

/* Old names of a nation and their later names (slice 27's renames, season_intl.c nation_renames, int-world
 * "nation rules" rows). A season database keeps the old record beside a record with a later name: the October
 * 2023 nation.dat holds Yugoslavia, Serbia and Montenegro; the December 2025 one holds "FYR of Macedonia" (nation
 * 66, no clubs and no players) and "North Macedonia" (229). Slice 27 renames an old record only where no later
 * record exists, so an old record that a later one replaces stays out of the pool (the latest-name rule of slice
 * 15b's e7d8869 for the Nations League: an empty national team stops its first match at virtual_staff.cpp 195,
 * 0x90d4bc). */
static const char *const renamed[][2] = {
    {"Yugoslavia", "Serbia and Montenegro"}, {"Serbia and Montenegro", "Serbia"}, {"Czech Republic", "Czechia"},
    {"Swaziland", "Eswatini"}, {"FYR of Macedonia", "North Macedonia"}, {"Turkey", "T\xfcrkiye"},
    {"Netherlands Antilles", "Cura\xe7" "ao"},
};

static int nation_named(uint32_t nations, int count, const char *name)
{
    for (int id = 0; id < count; id++)
        if (!strcmp(win_str(nations + (uint32_t)id * NATION_SIZE + 4u), name)) return 1;
    return 0;
}

/* 1 when the data holds a record with a later name of this name's chain. */
static int superseded(uint32_t nations, int count, const char *name)
{
    for (int step = 0; step < 8 && name; step++) {
        const char *next = NULL;
        for (size_t k = 0; k < sizeof renamed / sizeof renamed[0]; k++)
            if (!strcmp(renamed[k][0], name)) next = renamed[k][1];
        if (!next) return 0;
        if (nation_named(nations, count, next)) return 1;
        name = next;
    }
    return 0;
}

void o_00919e30(void);

/* The European nations of a campaign's pool, in nation order (out may be NULL to count): every European nation
 * whose national team names it, less Russia from the 2026 cycle (it played the qualifying groups of 2022), less an
 * old record that a later one replaces, less the hosts of the World Cup of the campaign. It reads only the
 * nation, club and host tables, which a load restores in full, so the group sizes of the draw and of a loaded game
 * agree (europe_group_size): the pool size +0xf2 is not in the save
 * (League::load 0x687b10 succeeds at a load, and the ctor then skips build_entry_pool, 0x918328..0x918339), so
 * the group builders 0x9190db and 0x919722 of a loaded game read 0 there. log: 1 for the lines of the pool. */
static int europe_nations(uint32_t self, uint32_t *out, int max, int log)
{
    uint32_t nations = rd32(g_mem, NATIONS);
    int count = (int)rd32(g_mem, NATION_COUNT), hosts[6], n_hosts, n = 0, russia = 0;
    if (!readable(nations) || count <= 0 || count > 1000) return 0;
    n_hosts = season_intl_host_ids(rd32(g_mem, KEY_WORLD_CUP), year_of(self) + 2, hosts);
    for (int id = 0; id < count && n < max; id++) {
        uint32_t nation = nations + (uint32_t)id * NATION_SIZE, c = rd32(g_mem, nation + 0x71u);
        if (!readable(c) || rd32(g_mem, c) != rd32(g_mem, 0x9cfa10u)) continue;
        if (!strcmp(win_str(nation + 4u), "Russia") && season_wcq26(self)) {
            russia = 1;
            continue;
        }
        if (superseded(nations, count, win_str(nation + 4u))) {
            if (log)
                fprintf(stderr, "recomp: season intl: wc qualifying %d: Europe leaves out %s (a later record of it "
                                "is in the data)\n", year_of(self) + 2, win_str(nation + 4u));
            continue;
        }
        int host = 0;
        for (int k = 0; k < n_hosts; k++) host |= hosts[k] == id;
        if (host) continue;
        uint32_t args[1] = {nation};
        uint32_t team = call_cdecl(NATIONAL_TEAM, 1, args);
        if (rt_unwinding) return n;
        if (!readable(team) || rd32(g_mem, team + 0x53u) != nation) continue;
        if (out) out[n] = team;
        n++;
    }
    if (log)
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Europe pool of %d nations (%s, %d hosts out)\n",
                year_of(self) + 2, n,
                russia ? "Russia out" : season_wcq26(self) ? "no Russia in the data" : "Russia in", n_hosts);
    return n;
}

void season_wcq26_log_pool(uint32_t self, const char *what)
{
    if (!logging() || !season_intl_on() || !readable(self)) return;
    uint32_t pool = rd32(g_mem, self + EU_POOL);
    int n = (int)rd32(g_mem, self + EU_POOL_SIZE), kosovo = 0;
    char odd[400] = "";
    size_t len = 0;
    for (int k = 0; readable(pool) && k < n && k < 64; k++) {
        uint32_t team = rd32(g_mem, pool + 4u * (uint32_t)k), nation = readable(team) ? rd32(g_mem, team + 0x53u) : 0;
        const char *name = readable(nation) ? win_str(nation + 4u) : "(none)";
        kosovo |= !strcmp(name, "Kosovo");
        if ((!readable(nation) || !readable(rd32(g_mem, nation + 0x71u)) || !strcmp(name, "Yugoslavia")) &&
            len < sizeof odd - 60)
            len += (size_t)snprintf(odd + len, sizeof odd - len, " %s%s", name,
                                    readable(nation) && readable(rd32(g_mem, nation + 0x71u)) ? "" : " (no continent)");
    }
    fprintf(stderr, "recomp: season intl: %s %d: Europe pool of %d nations, Kosovo %s;%s\n", what,
            (int16_t)rd16(g_mem, self + F_YEAR), n, kosovo ? "in" : "out", len ? odd : " no odd nation");
}

/* The campaigns of 2006 to 2022 (open item 57 of 0.6.2). build_entry_pool 0x919e30 takes at most 50 less the
 * exempt nations (above), and get_group_size 0x918dd0 gives groups of 5, with one or two exempt nations groups of
 * 6 at the stages 2, 4 and 6, or 2, 4 and 7 (0x918de9..0x918e13). A 2001 game on 0.6.0 played the 2006
 * qualifying with 49 nations, without Liechtenstein, and the 2010 one with 50, without Serbia (the forum save of
 * 2041 and long game seed 5; real: 51 and 53 entrants). In a season game the pool of these campaigns is every
 * European nation (europe_nations, Russia in), sorted by the original's compare, in the original's group count
 * (+0xff, set by build_entry_pool from the exempt count: 10 groups with no exempt nation, 9 with one or two, 8
 * with three, 0x91a0c2..0x91a114). The exempt nations are the European hosts, as a season game has no holder
 * (rt_intl_wc_holder). Each group then holds 5 or 6 (wide_size): the grid of the draw 0x918ec0 holds 6 a group
 * (0x918ef1) and every group stage of the template 0x9185c0 has 10 rounds (0x91860e), which a group of 5 or 6
 * plays. A pool above 6 a group keeps its best by reputation. A data with fewer than 5 a group keeps the
 * original's pool and sizes. */
#define WIDE_FIRST 2006
#define WIDE_LAST 2022
#define WIDE_ROOM 6                     /* nations a group: the grid's row (0x918ef1) */

static int europe_wide(uint32_t self)
{
    if (!is(self, VT_EUROPE) || !season_intl_on() || season_wcq26(self)) return 0;
    int y = year_of(self) + 2;
    return y >= WIDE_FIRST && y <= WIDE_LAST;
}

static int wide_groups(uint32_t self) { return (int8_t)rd8(g_mem, self + EU_LAST_GROUP) + 2; }

/* 1 when the groups of a 2006 to 2022 campaign take the sizes of wide_size: its pool holds 5 to 6 a group. The
 * draw 0x91a5db and the group builders 0x91a5e2 and 0x9196d0 run in one call (0x91a5db..0x91a6f9), before the pool
 * size +0xf2 goes to 0 (0x91a73d). The save writes the pool and its size (0x91b109..0x91b154) and the load reads
 * them back (0x91ad13..0x91adb8), so a load before the draw keeps the wide pool and its sizes. */
static int wide_sized(uint32_t self)
{
    int groups = wide_groups(self), n = (int)rd32(g_mem, self + EU_POOL_SIZE);
    return europe_wide(self) && groups >= 1 && groups <= 10 && n >= 5 * groups && n <= WIDE_ROOM * groups;
}

/* The size of group g (0 = A) of a wide campaign: the last (pool - 5 * groups) groups of 6, the others of 5. */
static int wide_size(uint32_t self, int g)
{
    int groups = wide_groups(self), six = (int)rd32(g_mem, self + EU_POOL_SIZE) - 5 * groups;
    return g >= groups - six ? 6 : 5;
}

#define NATIONS_MAX 128                 /* europe_nations of any data (55 European nations in 2025) */

/* The pool +0xee of a campaign made again: its European nations (europe_nations), sorted by the original's compare
 * (0x91a0b0), the best `room` of them; the pool size +0xf2 the count. The original's pool block is freed. Returns
 * the count, or -1 when no pool was made. */
static int pool_again(uint32_t self, int room, int log)
{
    uint32_t teams[NATIONS_MAX];
    int n = europe_nations(self, teams, NATIONS_MAX, log);
    if (rt_unwinding) return -1;
    uint32_t pool = game_malloc(4u * (uint32_t)(n > POOL_MAX ? n : POOL_MAX));
    if (rt_unwinding || !pool) return -1;
    for (int k = 0; k < n; k++) wr32(g_mem, pool + 4u * (uint32_t)k, teams[k]);
    uint32_t qa[4] = {pool, (uint32_t)n, 4, CLUB_COMPARE};
    call_cdecl(GAME_QSORT, 4, qa);
    if (rt_unwinding) return -1;
    if (n > room) {
        if (log)
            fprintf(stderr, "recomp: season intl: wc qualifying %d: Europe keeps the best %d of %d nations\n",
                    year_of(self) + 2, room, n);
        n = room;
    }
    game_free(rd32(g_mem, self + EU_POOL));
    if (rt_unwinding) return -1;
    wr32(g_mem, self + EU_POOL, pool);
    wr32(g_mem, self + EU_POOL_SIZE, (uint32_t)n);
    return n;
}

static void wide_pool(uint32_t self)
{
    int groups = wide_groups(self), have = (int)rd32(g_mem, self + EU_POOL_SIZE);
    if (groups < 1 || groups > 10) return;
    int n = europe_nations(self, NULL, NATIONS_MAX, 0);
    if (rt_unwinding || n <= have || n < 5 * groups) return;
    n = pool_again(self, WIDE_ROOM * groups, logging());
    if (n >= 0 && logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Europe pool of %d nations (the original's %d) in "
                        "%d groups\n", year_of(self) + 2, n, have, groups);
}

/* The return address of build_entry_pool's call in the ctor 0x918200 (0x918339); start_new_season calls it at
 * 0x9194cf. */
#define POOL_CTOR_RET 0x0091833eu

/* 1 when the wide pool waits for start_new_season: a ctor's campaign whose World Cup before it lies after the game
 * year (a 2001 game makes the 2006 campaign at the new game, and start_new_season 0x9194cf makes its pool again
 * after the 2002 World Cup, before the draw). So a new game of 2001 keeps the original's memory to that point. */
static int wide_waits(uint32_t self, uint32_t ret)
{
    return ret == POOL_CTOR_RET && year_of(self) > (int16_t)rd16(g_mem, GAME_DATE + 2u) + 2;
}

RT_WRAP(00919e30, wcq26_europe_pool)
{
    uint32_t self = cpu.ecx, ret = rd32(g_mem, cpu.esp);
    o_00919e30();
    if (!rt_unwinding && !europe_on(self) && is(self, VT_EUROPE)) {
        uint32_t eax = cpu.eax;
        season_wcq26_log_pool(self, "wc qualifying (original)");
        cpu.eax = eax;
    }
    /* A pool of 2006 to 2022 when the original made one: it returns 0 and sets no group count when it finds too
     * few nations (0x91a08c). */
    if (!rt_unwinding && europe_wide(self)) {
        uint32_t eax = cpu.eax;
        if (eax == 1u && !wide_waits(self, ret)) wide_pool(self);
        if (!rt_unwinding) cpu.eax = eax;
        return;
    }
    if (rt_unwinding || !europe_on(self)) return;
    if (pool_again(self, POOL_MAX, logging()) < 0) return;
    wr8(g_mem, self + EU_EXEMPT, 0);
    wr8(g_mem, self + EU_LAST_GROUP, EU_GROUPS - 2);
    wr8(g_mem, self + EU_RUNNERS_UP, EU_GROUPS - 1);
    wr8(g_mem, self + EU_PLAYOFF, EU_GROUPS);
    cpu.eax = 1;
}

/* 0x91a0c2, 0x91a0c9, 0x91a0d0 (pool, esi; at a load see the ctor wrap below): the last group
 * stage +0xff, the runners-up stage +0x100 and the play-off stage +0x101 (8, 9, 10: ten groups). +2 in the gate:
 * twelve groups. 0x918348, 0x91950c: the stage count (0xb), 0x91a6cf: the group count of the draw (0xa). */
uint32_t rt_wcq26_europe_index(uint32_t self, uint32_t original)
{
    return europe_on(self) ? original + 2u : original;
}

void o_00918200(void);

/* The ctor 0x918200 (thiscall (year, record), ret 8), at a load: League::load 0x687b10 (0x918321) reads the
 * three at load_extra_data 0x91a9f0 (vtable +0x20, called at 0x687ccd; 8, 9, 10 at 0x91aa96..0x91aaa4) before it
 * reads the stage count +0x2c (0x688e19), so they cannot depend on the shape there. After the ctor the stage
 * count is the save's: an object of the 12-group shape gets 10, 11, 12. Else a loaded 2030 campaign drew its
 * pool into 10 groups (0x918ec0) and the group builder 0x919722 read groups 11 and 12 past the grid: an empty
 * slot to the group draw news (0x9198f0, 0x684298) and box Database.cpp 13126 at 0x53b4ac on day 84 of 2028.
 * An old-shape campaign (11 stages) keeps the original's 8, 9, 10. */
RT_WRAP(00918200, wcq26_europe_ctor)
{
    uint32_t self = cpu.ecx;
    o_00918200();
    if (rt_unwinding || !europe_shape(self)) return;
    wr8(g_mem, self + EU_LAST_GROUP, EU_GROUPS - 2);
    wr8(g_mem, self + EU_RUNNERS_UP, EU_GROUPS - 1);
    wr8(g_mem, self + EU_PLAYOFF, EU_GROUPS);
}

/* The size of group g (0 = A): the last (nations - 48) groups of 5, the others of 4 (the real 2026 groups G to L
 * of 5, A to F of 4; 50 nations give K and L of 5). The count is europe_nations, not the pool size +0xf2, which a
 * load does not restore. */
static int europe_group_size(uint32_t self, int g)
{
    int pool = europe_nations(self, NULL, POOL_MAX, 0), five = pool - 4 * EU_GROUPS;
    if (five < 0) five = 0;
    if (five > EU_GROUPS) five = EU_GROUPS;
    return g >= EU_GROUPS - five ? 5 : 4;
}

void o_00918dd0(void);

/* get_group_size, thiscall (stage as a byte), ret 4 (0x918ea8): 5 for every group with no exempt nation
 * (0x918e9e). A wide campaign of 2006 to 2022 takes wide_size. */
RT_WRAP(00918dd0, wcq26_europe_group_size)
{
    uint32_t self = cpu.ecx;
    int stage = (int8_t)rd8(g_mem, cpu.esp + 4u);
    if (wide_sized(self)) {
        rt_ret_stdcall((uint32_t)wide_size(self, stage + 1), 1);
        return;
    }
    if (!europe_shape(self)) {
        o_00918dd0();
        return;
    }
    rt_ret_stdcall((uint32_t)europe_group_size(self, stage + 1), 1);
}

static int random_below(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    int r = (int8_t)call_cdecl(GAME_RANDOM, 1, a);
    return r < 0 || r >= n ? 0 : r;
}

void o_00918ec0(void);

/* The draw 0x918ec0 (thiscall (), from 0x91a5db): its order list and its fill counts are 10 bytes on the stack
 * (0x918f18..0x918f45, 0x918ecc..0x918f13), too few for 12 groups. The same draw for any group count: the grid
 * +0xf7 of groups * 6 dwords (0x918ef1..0x918f57), then 6 pots: the order of the groups shuffled by 20 swaps of
 * two random groups (0x918ff3..0x91901d), and each group in that order takes the next nation of the sorted pool
 * while it has room (0x919035..0x9190a3), until the pool size +0xf2 is dealt (0x918fdf). */
RT_WRAP(00918ec0, wcq26_europe_draw)
{
    uint32_t self = cpu.ecx;
    if (!europe_shape(self)) {
        o_00918ec0();
        return;
    }
    int groups = (int8_t)rd8(g_mem, self + EU_LAST_GROUP) + 2, pool = (int)rd32(g_mem, self + EU_POOL_SIZE);
    if (groups < 1 || groups > 16) groups = EU_GROUPS;
    uint32_t grid = game_malloc((uint32_t)groups * 6u * 4u);
    if (rt_unwinding) return;
    wr32(g_mem, self + EU_GRID, grid);
    if (!grid) {
        rt_ret_stdcall(0, 0);
        return;
    }
    signed char order[16], fill[16] = {0};
    for (int g = 0; g < groups; g++) order[g] = (signed char)g;
    int dealt = 0;
    uint32_t list = rd32(g_mem, self + EU_POOL);
    for (int pot = 0; pot < 6 && dealt < pool; pot++) {
        for (int k = 0; k < 20; k++) {
            int a = random_below(groups);
            if (rt_unwinding) return;
            int b = random_below(groups);
            if (rt_unwinding) return;
            signed char t = order[a];
            order[a] = order[b];
            order[b] = t;
        }
        for (int i = 0; i < groups && dealt < pool; i++) {
            int g = order[i];
            if (fill[g] >= europe_group_size(self, g)) continue;
            wr32(g_mem, grid + 4u * (uint32_t)(g * 6 + fill[g]), rd32(g_mem, list + 4u * (uint32_t)dealt));
            dealt++;
            fill[g]++;
        }
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Europe draws %d nations into %d groups\n",
                year_of(self) + 2, dealt, groups);
    rt_ret_stdcall(0, 0);
}

/* The real 2025 matchdays (FIFA World Cup 2026 qualification (UEFA), schedule, read 2026-10-19 from Wikipedia,
 * after the UEFA calendar it cites): groups of 5 play all ten, groups of 4 the last six (from September). The
 * paths: semi-finals 26 March, finals 31 March 2026. Year: offset from the campaign year. */
static const SeasonNlDate europe_md[10] = {
    {21, 3, 1}, {24, 3, 1}, {6, 6, 1}, {9, 6, 1}, {4, 9, 1}, {7, 9, 1}, {9, 10, 1}, {12, 10, 1}, {13, 11, 1},
    {16, 11, 1},
};
static const SeasonNlDate europe_sf = {26, 3, 2}, europe_final = {31, 3, 2};

/* The template of wc_europe_league (slot 15, 0x9185c0), from slice 28's wrap (season_nl_wc.c) at its entry
 * state. Gives 1 when it completed the call. A group stage: 10 rounds for a group of 5, 6 for a group of 4 (a
 * league of n plays n + n % 2 - 1 rounds a leg, twice); the play-off: two single-match rounds of 16 and 8. */
/* The paths of a campaign: 4 (UEFA 16 places) less the European hosts (2030: 14 places and its two hosts, O265). */
static int europe_paths(uint32_t self)
{
    return places_of(self, WCQ_UEFA, NULL) - EU_GROUPS;     /* a host row names at most two European hosts */
}

int season_wcq26_europe_template(void)
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    if (!europe_shape(self)) return 0;
    int st = (int8_t)rd8(g_mem, sp + 4u);
    int group = st <= (int8_t)rd8(g_mem, self + EU_LAST_GROUP), playoff = st == (int8_t)rd8(g_mem, self + EU_PLAYOFF);
    if (!group && !playoff) return 0;
    int size = group ? europe_group_size(self, st + 1) : 0, rounds = group ? (size == 5 ? 10 : 6) : 2;
    uint32_t out_rounds = rd32(g_mem, sp + 8u), out_code = rd32(g_mem, sp + 0xcu), out_league = rd32(g_mem, sp + 0x10u);
    if (readable(out_league)) wr32(g_mem, out_league, group ? 1u : 0u);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, (uint32_t)rounds);
    if (readable(out_code)) wr16(g_mem, out_code, group ? (uint32_t)(0x3e9 + st) : 0u);
    uint32_t rec = game_malloc(group ? (uint32_t)rounds * LEAGUE_ROUND : 2u * ROUND_SIZE);
    if (rt_unwinding) return 1;
    if (rec && group) {
        for (int i = 0; i < rounds && !rt_unwinding; i++)
            season_nl_league_round(rec, i, europe_md[10 - rounds + i], year_of(self));
    } else if (rec) {
        int teams = 4 * europe_paths(self);
        season_nl_path_round(rec, 0, europe_sf, year_of(self), teams);
        if (!rt_unwinding) season_nl_path_round(rec, 1, europe_final, year_of(self), teams / 2);
    }
    if (rt_unwinding) return 1;
    rt_ret_stdcall(rec, 4);
    return 1;
}

/* The paths (create_play_off_stage 0x919910), from slice 28's wrap at its entry state. Gives 1 when it handled
 * the call. The 12 runners-up in the order of the runners-up table stage[+0x100] (0x91a140: a SubLeague of the
 * top four of each group, so a group of 5 leaves out the results against its fifth, as the real rule) make pots
 * 1 to 3; pot 4 holds the four best 2024-25 Nations League group winners by the overall ranking that were not in
 * the top two of their group (without the Nations League tables, the best thirds by points). Path p: pot 1 [p] v
 * pot 4 [3 - p] and pot 2 [p] v pot 3 [3 - p], the first at home; the winners meet in the final (decision Q564:
 * the real draw of the pairs is not made). */
int season_wcq26_europe_paths(void)
{
    uint32_t self = cpu.ecx;
    if (!europe_shape(self)) return 0;
    int groups = (int8_t)rd8(g_mem, self + EU_LAST_GROUP) + 2, nations = (int)rd32(g_mem, NATION_COUNT);
    int paths = europe_paths(self);
    if (groups > 16 || nations <= 0 || nations > 4096) {
        rt_ret_stdcall(0, 0);
        return 1;
    }
    char *top2 = calloc((size_t)nations, 1);
    if (!top2) {
        rt_ret_stdcall(0, 0);
        return 1;
    }
    uint32_t third[16] = {0}, z[1] = {0};
    int third_points[16] = {0};
    for (int g = 0; g < groups; g++) {
        uint32_t st = stage_of(self, g - 1);
        if (!readable(st)) continue;
        call_this(TABLE_SORT, st, 1, z);
        if (rt_unwinding) { free(top2); return 1; }
        for (int r = 0; r < 2; r++) {
            int n = season_nl_nation_of_club(table_club(st, r));
            if (n >= 0 && n < nations) top2[n] = 1;
        }
        third[g] = table_club(st, 2);
        uint32_t table = rd32(g_mem, st + F_TABLE);
        third_points[g] = readable(table) ? (int16_t)rd16(g_mem, table + 2u * ROW_SIZE + ROW_POINTS) : 0;
    }
    uint32_t ru_stage = stage_of(self, (int8_t)rd8(g_mem, self + EU_RUNNERS_UP)), ru[16] = {0};
    int n_ru = 0;
    if (readable(ru_stage)) {
        call_this(TABLE_SORT, ru_stage, 1, z);
        if (rt_unwinding) { free(top2); return 1; }
        for (int r = 0; r < groups && n_ru < EU_GROUPS; r++) {
            uint32_t c = table_club(ru_stage, r);
            if (c) ru[n_ru++] = c;
        }
    }
    if (n_ru < EU_GROUPS) {
        n_ru = 0;
        for (int g = 0; g < groups && n_ru < EU_GROUPS; g++) {
            uint32_t c = table_club(stage_of(self, g - 1), 1);
            if (c) ru[n_ru++] = c;
        }
    }
    uint32_t extra[EU_PATHS] = {0};
    int n_extra = 0;
    SeasonNlRow rows[NL_MAX_NATIONS];
    int n = season_nl_held_results(year_of(self), rows);
    if (rt_unwinding) { free(top2); return 1; }
    for (int i = 0; i < n && n_extra < paths; i++) {
        int id = rows[i].nation;
        /* A host of the World Cup plays no qualifying (O259 A11: the 2030 paths took Portugal). */
        if (rows[i].pos != 1 || id < 0 || id >= nations || top2[id] || host_id(self, id)) continue;
        uint32_t t = season_nl_nation_team(id);
        if (rt_unwinding) { free(top2); return 1; }
        if (t) extra[n_extra++] = t;
    }
    for (int k = n_extra; k < paths; k++) {
        int best = -1;
        for (int g = 0; g < groups; g++)
            if (third[g] && (best < 0 || third_points[g] > third_points[best])) best = g;
        if (best < 0) break;
        extra[n_extra++] = third[best];
        third[best] = 0;
    }
    free(top2);
    if (n_ru < EU_GROUPS || n_extra < paths) {
        fprintf(stderr, "recomp: season intl: wc qualifying %d: no paths (%d runners-up, %d more)\n",
                year_of(self) + 2, n_ru, n_extra);
        rt_ret_stdcall(0, 0);
        return 1;
    }
    uint32_t list = game_malloc(4u * EU_PATH_TEAMS), out = game_malloc(8);         /* room for 4 paths */
    if (rt_unwinding) return 1;
    if (!list || !out) {
        if (list) game_free(list);
        if (out) game_free(out);
        rt_ret_stdcall(0, 0);
        return 1;
    }
    for (int p = 0; p < paths; p++) {
        uint32_t four[4] = {ru[p], extra[paths - 1 - p], ru[paths + p], ru[3 * paths - 1 - p]};
        for (int k = 0; k < 4; k++) wr32(g_mem, list + 4u * (uint32_t)(4 * p + k), four[k]);
        if (logging())
            fprintf(stderr, "recomp: season intl: wc qualifying %d: path %c: %s v %s, %s v %s\n", year_of(self) + 2,
                    'A' + p, team_name(four[0]), team_name(four[1]), team_name(four[2]), team_name(four[3]));
    }
    int po = (int8_t)rd8(g_mem, self + EU_PLAYOFF);
    uint32_t xa[4] = {(uint32_t)po, out, out + 4u, 0};
    uint32_t tmpl = call_this(rd32(g_mem, rd32(g_mem, self) + 0x3cu), self, 4, xa);
    if (rt_unwinding) return 1;
    uint32_t na[1] = {0xb2};
    uint32_t cup = tmpl ? call_cdecl(GAME_NEW, 1, na) : 0;
    if (rt_unwinding) return 1;
    if (readable(cup)) {
        uint32_t ca[15] = {self, (uint32_t)(4 * paths), list, (uint32_t)(int16_t)rd16(g_mem, out),
                           rd32(g_mem, self + F_RECORD),
                           tmpl, (uint32_t)year_of(self), (uint32_t)po, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u),
                           0x14, 1, 0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return 1;
    } else {
        cup = 0;
    }
    game_free(list);
    game_free(tmpl);
    game_free(out);
    if (rt_unwinding) return 1;
    if (cup) {
        wr32(g_mem, rd32(g_mem, self + F_STAGES) + 4u * (uint32_t)po, cup);
        call_this(CUP_START, cup, 1, z);
        if (rt_unwinding) return 1;
        wr32(g_mem, self + F_STAGE, (uint32_t)po);
    }
    rt_ret_stdcall(0, 0);
    return 1;
}

/* get_asia_play_off_team 0x91def0 (from Asia stage 12): no European team in the gate (stage 12 is not played). */
int season_wcq26_europe_no_asia(void)
{
    if (!europe_shape(cpu.ecx)) return 0;
    rt_ret_stdcall(0, 0);
    return 1;
}

/* ---- The play-off tournament (Oceania stages 2 and 3) ---- */

static int oceania_on(uint32_t self) { return is(self, VT_OCEANIA) && season_wcq26(self); }

/* The team with status 1 in a League table, the n-th (from 0). */
static uint32_t table_status(uint32_t league, int nth)
{
    if (!readable(league)) return 0;
    int n = (int16_t)rd16(g_mem, league + F_TEAMS);
    uint32_t rows = rd32(g_mem, league + F_TABLE);
    for (int i = 0; readable(rows) && i < n && i < 64; i++)
        if (rd8(g_mem, rows + (uint32_t)i * ROW_SIZE + ROW_STATUS) == 1u && nth-- == 0)
            return rd32(g_mem, rows + (uint32_t)i * ROW_SIZE);
    return 0;
}

/* A team of a Cup round with status 1 (want 1) or another status (want 0). */
static uint32_t cup_row(uint32_t cup, int want)
{
    if (!readable(cup)) return 0;
    int n = (int16_t)rd16(g_mem, cup + F_TEAMS);
    uint32_t rows = rd32(g_mem, cup + F_CUP_ROWS);
    for (int i = 0; readable(rows) && i < n && i < 64; i++) {
        uint32_t team = rd32(g_mem, rows + (uint32_t)i * 6u);
        if (team && (rd8(g_mem, rows + (uint32_t)i * 6u + 5u) == 1u) == (want == 1)) return team;
    }
    return 0;
}

/* The best runner-up of the African groups (Cup stages 0 to 8): points (+0x19 of the row), then the reputation
 * of the nation (decision Q565: the real play-off of the four best runners-up is left out). */
static uint32_t africa_runner_up(void)
{
    uint32_t af = comp_of(KEY_AFRICA), best = 0, z[1] = {0};
    int best_points = -1, best_rep = -1;
    for (int g = 0; g < 9; g++) {
        uint32_t st = stage_of(af, g);
        if (!readable(st) || (int16_t)rd16(g_mem, st + F_TEAMS) < 2) continue;
        call_this(TABLE_SORT, st, 1, z);
        if (rt_unwinding) return 0;
        uint32_t table = rd32(g_mem, st + F_TABLE), club = table_club(st, 1);
        if (!readable(table) || !club) continue;
        int points = (int16_t)rd16(g_mem, table + ROW_SIZE + ROW_POINTS);
        uint32_t nation = rd32(g_mem, club + 0x53u);
        int rep = readable(nation) ? (int16_t)rd16(g_mem, nation + 0x8eu) : 0;
        if (points > best_points || (points == best_points && rep > best_rep)) {
            best = club;
            best_points = points;
            best_rep = rep;
        }
    }
    return best;
}

/* The place of a team that a section has not given (a game that starts after the qualifying began, Q388): the
 * strongest nation of the continent (key continent global) that is not in the World Cup slots and not in
 * taken[], after the nations that the confederation's direct places would take. direct counts the hosts and
 * qualifiers of the continent already in the slots, so the first (direct - in slots) free nations by
 * reputation are passed over: a December 2025 start put Brazil, which the draw fill takes into the finals,
 * into the tournament before this. */
static uint32_t strongest_free(uint32_t continent_key, int direct, const uint32_t *taken, int n_taken)
{
    uint32_t nations = rd32(g_mem, NATIONS), wc = comp_of(KEY_WORLD_CUP);
    int count = (int)rd32(g_mem, NATION_COUNT);
    if (!readable(nations) || count <= 0 || count > 1000) return 0;
    uint32_t slots = wc ? rd32(g_mem, wc + 0x14u) : 0;
    int filled = wc ? (int16_t)rd16(g_mem, wc + 0x36u) : 0;
    uint32_t want = rd32(g_mem, continent_key);
    enum { MAX = 64 };
    uint32_t free_team[MAX];
    int free_rep[MAX], n_free = 0, in_slots = 0;
    for (int id = 0; id < count; id++) {
        uint32_t nation = nations + (uint32_t)id * NATION_SIZE, c = rd32(g_mem, nation + 0x71u);
        if (!readable(c) || rd32(g_mem, c) != want) continue;
        uint32_t args[1] = {nation};
        uint32_t team = call_cdecl(NATIONAL_TEAM, 1, args);
        if (rt_unwinding) return 0;
        if (!readable(team) || rd32(g_mem, team + 0x53u) != nation) continue;
        int in = 0;
        for (int k = 0; readable(slots) && k < filled && k < 64 && !in; k++)
            in = rd32(g_mem, slots + 6u * (uint32_t)k) == team;
        if (in) {
            in_slots++;
            continue;
        }
        for (int k = 0; k < n_taken && !in; k++) in = taken[k] == team;
        if (in) continue;
        if (n_free >= MAX) continue;
        int rep = (int16_t)rd16(g_mem, nation + 0x8eu), at = n_free++;
        while (at > 0 && free_rep[at - 1] < rep) {      /* sorted, best first */
            free_team[at] = free_team[at - 1];
            free_rep[at] = free_rep[at - 1];
            at--;
        }
        free_team[at] = team;
        free_rep[at] = rep;
    }
    int skip = direct - in_slots;
    if (skip < 0) skip = 0;
    if (skip >= n_free) skip = n_free - 1;
    return n_free > 0 ? free_team[skip] : 0;
}

static int reputation(uint32_t team)
{
    uint32_t nation = readable(team) ? rd32(g_mem, team + 0x53u) : 0;
    return readable(nation) ? (int16_t)rd16(g_mem, nation + 0x8eu) : -1;
}

/* Builds stage `st` of the Oceania object as a Cup_Stage of 4 in the list's pairs, one single-match round (the
 * template wrap below), as slice 28's paths (season_nl_wc.c) and the original's tie (0x91f993..0x91f9b9). */
static void build_round(uint32_t self, int st, const uint32_t *teams)
{
    uint32_t list = game_malloc(16), out = game_malloc(8), z[1] = {0};
    if (rt_unwinding) return;
    if (!list || !out) {
        if (list) game_free(list);
        if (out) game_free(out);
        return;
    }
    for (int k = 0; k < 4; k++) wr32(g_mem, list + 4u * (uint32_t)k, teams[k]);
    uint32_t xa[4] = {(uint32_t)st, out, out + 4u, 0};
    uint32_t tmpl = call_this(rd32(g_mem, rd32(g_mem, self) + 0x3cu), self, 4, xa);
    if (rt_unwinding) return;
    uint32_t na[1] = {0xb2};
    uint32_t cup = tmpl ? call_cdecl(GAME_NEW, 1, na) : 0;
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, 4, list, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + F_RECORD), tmpl,
                           (uint32_t)year_of(self), (uint32_t)st, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 1,
                           0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return;
    } else {
        cup = 0;
    }
    game_free(list);
    game_free(tmpl);
    game_free(out);
    if (rt_unwinding || !cup) return;
    wr32(g_mem, rd32(g_mem, self + F_STAGES) + 4u * (uint32_t)st, cup);
    call_this(CUP_START, cup, 1, z);
}

/* The status of a team in a Cup's rows (+0xa7, 6 bytes: +0 the team, +5 the status), or -1 when not in them. */
static int cup_status(uint32_t cup, uint32_t team)
{
    uint32_t rows = readable(cup) ? rd32(g_mem, cup + F_CUP_ROWS) : 0;
    int n = readable(cup) ? (int16_t)rd16(g_mem, cup + F_TEAMS) : 0;
    for (int i = 0; readable(rows) && i < n && i < 64; i++)
        if (rd32(g_mem, rows + (uint32_t)i * 6u) == team) return rd8(g_mem, rows + (uint32_t)i * 6u + 5u);
    return -1;
}

/* The six of the tournament, best first by nation reputation (the real seeding used the FIFA ranking of
 * November 2025: decision Q565): the first two are the seeds. The sections keep their tables until their
 * start_new_season of 25 July, so the finals take the same six again (no state of this file goes into a save). */
/* CONCACAF with no host (2030, O265, and after the table): its two places in the tournament go to the two best
 * thirds of its semi-final groups (stages 3 to 5, 0x917320), by points, then goal difference, as the original's
 * tables sort (0x6827d0). */
static void concacaf_thirds(uint32_t out[2])
{
    uint32_t cc = comp_of(KEY_CONCACAF), z[1] = {0}, team[3] = {0, 0, 0};
    int score[3] = {0, 0, 0};
    for (int g = 0; g < 3; g++) {
        uint32_t st = stage_of(cc, 3 + g);
        if (!readable(st)) continue;
        call_this(TABLE_SORT, st, 1, z);
        if (rt_unwinding) return;
        uint32_t table = rd32(g_mem, st + F_TABLE);
        team[g] = table_club(st, 2);
        uint32_t row = table + 2u * ROW_SIZE;          /* goals for +0x15 and against +0x17 (0x683210) */
        if (readable(table))
            score[g] = 1000 * (int16_t)rd16(g_mem, row + ROW_POINTS) + 500 + (int16_t)rd16(g_mem, row + 0x15u) -
                       (int16_t)rd16(g_mem, row + 0x17u);
    }
    for (int k = 0; k < 2; k++) {
        int best = -1;
        for (int g = 0; g < 3; g++)
            if (team[g] && (best < 0 || score[g] > score[best])) best = g;
        out[k] = best >= 0 ? team[best] : 0;
        if (best >= 0) team[best] = 0;
    }
}

static int tournament_six(uint32_t self, uint32_t six[6])
{
    /* A start after the qualifying began (O259 A9, O274): the real six, in the order of the real bracket. */
    int y = year_of(self) + 2, real = 0;
    if (season_intl_late_start("world_cup", y))
        for (int k = 0; k < 6; k++) {
            const char *name = season_intl_real_tie(y, k);
            six[k] = name ? season_intl_team_named(name) : 0;
            if (rt_unwinding) return 0;
            real += six[k] != 0;
        }
    if (real == 6) return 1;
    static const int of[6] = {WCQ_OFC, WCQ_CONMEBOL, WCQ_CONCACAF, WCQ_CONCACAF, WCQ_CAF, WCQ_AFC};
    /* strongest_free passes over the nations of a confederation that its direct places and its hosts take
     * (2026: CONCACAF 3 places and its 3 hosts). */
    int direct[6];
    for (int k = 0; k < 6; k++) {
        int h = 0;
        direct[k] = places_of(self, of[k], &h) + h;
    }
    uint32_t t[6];
    /* The OFC final's loser; with an OFC host the OFC has no direct place and its winner plays (open item 59). */
    t[0] = cup_row(stage_of(self, 1), places_of(self, WCQ_OFC, NULL) ? 0 : 1);
    t[1] = table_status(comp_of(KEY_SOUTH_AMERICA), 0);                      /* CONMEBOL 7th */
    t[2] = table_status(stage_of(comp_of(KEY_CONCACAF), 6), 0);              /* CONCACAF 4th and 5th */
    t[3] = table_status(stage_of(comp_of(KEY_CONCACAF), 6), 1);
    /* CONCACAF with no host (2030, after the table): all six of the final group qualify, its two places go to
     * the best thirds of the semi-final groups. With one host the sixth and the best third. */
    int cc_hosts = conf_hosts(self, CONTINENT_NORTH_AMERICA);
    if (season_cc26_tournament(comp_of(KEY_CONCACAF), t + 2)) {
        /* item 214: the 2026 shape, the best two after the direct places of the third round */
    } else if (cc_hosts == 0) {
        concacaf_thirds(t + 2);
    } else if (cc_hosts == 1) {
        uint32_t thirds[2];
        concacaf_thirds(thirds);
        t[3] = thirds[0];
    }
    if (rt_unwinding) return 0;
    t[4] = africa_runner_up();
    if (rt_unwinding) return 0;
    t[5] = cup_row(stage_of(comp_of(KEY_ASIA), 11), 1);                      /* the AFC play-off winner */
    if (logging() && !season_cc26_on(comp_of(KEY_CONCACAF))) {  /* test log: the CONCACAF final group, statuses */
        uint32_t fg = stage_of(comp_of(KEY_CONCACAF), 6), rows = readable(fg) ? rd32(g_mem, fg + F_TABLE) : 0;
        int n = readable(fg) ? (int16_t)rd16(g_mem, fg + F_TEAMS) : 0;
        fprintf(stderr, "recomp: season intl: wc play-off %d: CONCACAF final group of %d:", year_of(self) + 2, n);
        for (int i = 0; readable(rows) && i < n && i < 8; i++)
            fprintf(stderr, " %s %u;", team_name(rd32(g_mem, rows + (uint32_t)i * ROW_SIZE)),
                    rd8(g_mem, rows + (uint32_t)i * ROW_SIZE + ROW_STATUS));
        fputc('\n', stderr);
    }
    for (int k = 0; k < 6; k++)
        if (!t[k]) {
            t[k] = strongest_free(conf_continent[of[k]], direct[k], t, 6);
            if (rt_unwinding) return 0;
            if (logging())
                fprintf(stderr, "recomp: season intl: wc play-off %d: %s team not known, %s plays\n", year_of(self) + 2,
                        conf_name[of[k]], team_name(t[k]));
        }
    int order[6] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < 6; i++)
        for (int j = i + 1; j < 6; j++)
            if (reputation(t[order[j]]) > reputation(t[order[i]])) {
                int x = order[i];
                order[i] = order[j];
                order[j] = x;
            }
    for (int k = 0; k < 6; k++) six[k] = t[order[k]];
    return 1;
}

/* The semi-finals: 3rd v 6th and 4th v 5th of the six; each winner plays a seed in the final. */
static void build_semi_finals(uint32_t self)
{
    uint32_t six[6];
    if (!tournament_six(self, six)) return;
    uint32_t semis[4] = {six[2], six[5], six[3], six[4]};
    if (logging())
        fprintf(stderr, "recomp: season intl: wc play-off %d: seeds %s, %s; semi-finals %s v %s, %s v %s\n",
                year_of(self) + 2, team_name(six[0]), team_name(six[1]), team_name(semis[0]), team_name(semis[1]),
                team_name(semis[2]), team_name(semis[3]));
    build_round(self, 2, semis);
}

/* The winner of tie k of a single-match Cup_Stage in the list's pairs (rows 2k and 2k + 1). */
static uint32_t tie_winner(uint32_t cup, int k)
{
    uint32_t rows = readable(cup) ? rd32(g_mem, cup + F_CUP_ROWS) : 0;
    for (int i = 2 * k; readable(rows) && i < 2 * k + 2; i++)
        if (rd8(g_mem, rows + (uint32_t)i * 6u + 5u) == 1u) return rd32(g_mem, rows + (uint32_t)i * 6u);
    return 0;
}

/* The finals: seed 1 v the winner of semi-final 1, seed 2 v the winner of semi-final 2, the seed at home. The
 * seeds are the two of the six that did not play the semi-finals (the rows of stage 2), so a reputation that
 * changes between 26 and 31 March does not take a semi-finalist into the seeds (review 2). The two seeds keep
 * the order of the day's sort (the pairing of a seed with a semi-final was a draw in 2026). */
static void build_finals(uint32_t self)
{
    uint32_t six[6], semis = stage_of(self, 2), seeds[2] = {0, 0};
    if (!tournament_six(self, six)) return;
    int n = 0;
    for (int k = 0; k < 6 && n < 2; k++)
        if (cup_status(semis, six[k]) < 0) seeds[n++] = six[k];
    if (n < 2) {                                         /* not found: the sort of today, logged */
        seeds[0] = six[0];
        seeds[1] = six[1];
        if (logging())
            fprintf(stderr, "recomp: season intl: wc play-off %d: the seeds are not the two out of the semi-finals\n",
                    year_of(self) + 2);
    }
    uint32_t finals[4] = {seeds[0], tie_winner(semis, 0), seeds[1], tie_winner(semis, 1)};
    if (logging())
        fprintf(stderr, "recomp: season intl: wc play-off %d: finals %s v %s, %s v %s\n", year_of(self) + 2,
                team_name(finals[0]), team_name(finals[1]), team_name(finals[2]), team_name(finals[3]));
    build_round(self, 3, finals);
}

static int on_or_after(int day, int month1, int year)
{
    int now_year = (int16_t)rd16(g_mem, GAME_DATE + 2), now_day = (int16_t)rd16(g_mem, GAME_DATE);
    if (now_year != year) return now_year > year;
    return now_day >= season_nl_day_of_year(day, month1, year);
}

void o_0091f0d0(void);

/* advance_stage of the Oceania section, thiscall () (vtable +0x28, 0x91f0d0), called by
 * League::update_for_date 0x6847c0 when the stages up to +0x30 are complete. The original waits at +0x30 = 1
 * for the South American section (0x91f0d9..0x91f0f8), then builds the tie with South America (0x91f800). In the
 * gate: at +0x30 = 1 it waits for 1 March of the World Cup year (every section ends in 2025: doc), then builds
 * the semi-finals as stage 2; at +0x30 = 2 the finals as stage 3. The other steps: the original. */
RT_WRAP(0091f0d0, wcq26_oceania_advance)
{
    uint32_t self = cpu.ecx;
    int now = readable(self) ? (int)rd32(g_mem, self + F_STAGE) : -1;
    if (season_ofc_first_waiting(self)) {                       /* item 198: the preliminary ties in slot 0 */
        rt_ret_stdcall(0, 0);
        return;
    }
    if (!oceania_on(self) || (now != 1 && now != 2)) {
        if (season_intl_oceania_wait(self)) {                  /* O259 A16: a CONCACAF partner not decided */
            rt_ret_stdcall(0, 0);
            return;
        }
        if (rt_unwinding) return;
        cpu.ecx = self;
        o_0091f0d0();
        return;
    }
    if (now == 1 && !on_or_after(1, 3, year_of(self) + 2)) {
        rt_ret_stdcall(0, 0);
        return;
    }
    wr32(g_mem, self + F_STAGE, (uint32_t)now + 1u);
    if (now == 1) build_semi_finals(self);
    else build_finals(self);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

void o_0091e2e0(void);

/* create_schedule of the Oceania section, thiscall (stage as a byte, short *rounds, short *code, int *is_league),
 * ret 0x10. In the gate stages 2 and 3 are one single-match round of 4 teams (slice 28's path round: extra time
 * and penalties, bit 0x200 for the list's pairs) on 26 and 31 March of the World Cup year. */
RT_WRAP(0091e2e0, wcq26_oceania_template)
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    int st = (int8_t)rd8(g_mem, sp + 4u);
    if (!oceania_on(self) || (st != 2 && st != 3)) {
        o_0091e2e0();
        if (!rt_unwinding) season_intl_oceania_tie_dates(self, st, cpu.eax);
        return;
    }
    uint32_t out_rounds = rd32(g_mem, sp + 8u), out_code = rd32(g_mem, sp + 0xcu), out_league = rd32(g_mem, sp + 0x10u);
    if (readable(out_league)) wr32(g_mem, out_league, 0);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, 1);
    if (readable(out_code)) wr16(g_mem, out_code, 0);
    uint32_t rec = game_malloc(ROUND_SIZE);
    if (rt_unwinding) return;
    SeasonNlDate d = {st == 2 ? 26 : 31, 3, 2};
    if (rec) season_nl_path_round(rec, 0, d, year_of(self), 4);
    if (rt_unwinding) return;
    rt_ret_stdcall(rec, 4);
}

void o_0091e840(void);

/* The status records of the Oceania section (vtable +0x44, slot 17, 0x91e840), thiscall (team, status, stage,
 * ...), ret 0x18. Stage 1 status 1 gives a record only (0x91e8b2..0x91e8d6); stage 2 status 1 adds the team to
 * the World Cup (0x91e937, add_qualified_team 0x92e740). In the gate the OFC final's winner qualifies (stage 1
 * as 2), a semi-final winner of the tournament gets a record only (stage 2 as 1) and a final winner qualifies
 * (stage 3 as 2). With an OFC host the OFC has no direct place (open item 59): the final's winner keeps its
 * record of stage 1 and plays the tournament (tournament_six). */
RT_WRAP(0091e840, wcq26_oceania_status)
{
    if (oceania_on(cpu.ecx)) {
        int st = (int8_t)rd8(g_mem, cpu.esp + 0xcu);
        int to = st == 1 ? (places_of(cpu.ecx, WCQ_OFC, NULL) ? 2 : 1) : st == 2 ? 1 : st == 3 ? 2 : st;
        if (to != st) wr8(g_mem, cpu.esp + 0xcu, (uint32_t)to);
        if (logging() && rd8(g_mem, cpu.esp + 8u) == 1u && to == 2)
            fprintf(stderr, "recomp: season intl: wc qualifying %d: %s qualifies (Oceania stage %d)\n",
                    year_of(cpu.ecx) + 2, team_name(rd32(g_mem, cpu.esp + 4u)), st);
    }
    o_0091e840();
}

/* The semi-final groups 0x917320 (from advance 0x9170f8, this in esi) make three groups of four. Each takes as
 * its first team the first central seed of the pool slots 25 to 34 (0x917425..0x91744e; it clears the slot), into
 * a block of 16 bytes from malloc (0x917382) that it does not clear: with no seed left it keeps what the block
 * held, the seed of the group before. The central part of the pool holds 10 - +0xbc nations (0x916a47); with the
 * three hosts out (+0xbc 3, all of them in region 7) its two groups of two (0x916d5b, 0x916e86) and the round
 * 0x917110 leave two seeds for three groups, and a 2023 start with changing rules off put Costa Rica into two
 * groups. In the gate the empty seed slots take the strongest nations of North America (continent 0x9cfa14) that
 * are out of the early rounds (concacaf_in_play). */
/* 1 when a team still plays in the section or is in the World Cup: a pool slot that has not played (not in the
 * first round's rows, or a first round winner), the leader of a central group of stage 0 or 1, a winner of the
 * round of stage 2, a World Cup slot. The losers of the early rounds are free. */
static int concacaf_in_play(uint32_t self, uint32_t team)
{
    uint32_t list = rd32(g_mem, self + 0xb2u), wc = comp_of(KEY_WORLD_CUP);
    for (int i = 0; readable(list) && i < 35; i++)
        if (rd32(g_mem, list + 4u * (uint32_t)i) == team) {
            int st = cup_status(self, team);
            if (st < 0 || st == 1) return 1;
        }
    if (cup_status(self, team) == 1 || cup_status(stage_of(self, 2), team) == 1) return 1;
    for (int s = 0; s < 2; s++)
        if (table_club(stage_of(self, s), 0) == team) return 1;
    uint32_t slots = wc ? rd32(g_mem, wc + 0x14u) : 0;
    for (int k = 0; readable(slots) && k < (int16_t)rd16(g_mem, wc + 0x36u) && k < 64; k++)
        if (rd32(g_mem, slots + 6u * (uint32_t)k) == team) return 1;
    return 0;
}

static void concacaf_fill_seeds(uint32_t self)
{
    uint32_t list = rd32(g_mem, self + 0xb2u), nations = rd32(g_mem, NATIONS);
    int count = (int)rd32(g_mem, NATION_COUNT), have = 0;
    if (!readable(list) || !readable(nations) || count <= 0 || count > 1000) return;
    for (int i = 25; i < 35; i++) have += rd32(g_mem, list + 4u * (uint32_t)i) != 0;
    for (int i = 25; i < 35 && have < 3; i++) {
        if (rd32(g_mem, list + 4u * (uint32_t)i)) continue;
        uint32_t best = 0;
        int best_rep = -1;
        for (int id = 0; id < count; id++) {
            uint32_t nation = nations + (uint32_t)id * NATION_SIZE, c = rd32(g_mem, nation + 0x71u);
            int rep = (int16_t)rd16(g_mem, nation + 0x8eu);
            if (!readable(c) || rd32(g_mem, c) != rd32(g_mem, 0x9cfa14u) || rep <= best_rep) continue;
            uint32_t args[1] = {nation};
            uint32_t team = call_cdecl(NATIONAL_TEAM, 1, args);
            if (rt_unwinding) return;
            if (!readable(team) || rd32(g_mem, team + 0x53u) != nation || concacaf_in_play(self, team)) continue;
            best = team;
            best_rep = rep;
        }
        if (!best) return;
        wr32(g_mem, list + 4u * (uint32_t)i, best);
        have++;
        fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF semi-final seed %s (the pool gave too few)\n",
                year_of(self) + 2, team_name(best));     /* always: the rule acts (O302) */
    }
}

void o_00917320(void);

RT_WRAP(00917320, wcq26_concacaf_semis)
{
    uint32_t self = cpu.ecx;
    if (season_wcq26(self)) concacaf_fill_seeds(self);
    if (rt_unwinding) return;
    cpu.ecx = self;
    o_00917320();
}

