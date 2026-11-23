/* Finland: second teams stay out of the Veikkausliiga and play below their first team (open item 118, 0.7.0).
 *
 * The real rule. Palloliitto, Jalkapallon kilpailumääräykset 2024 and 2026, § 6.12 (last sentence): the reserve
 * team (or a farm or partner club) of a club in the top tier may not go up into the top tier, nor play the separate
 * promotion play-off to it. § 3.24 (2024), § 3.25 (2026) and § 3.23 (2023): a reserve team plays in a lower tier
 * than its club's first team. Sources read on 3 November 2026: the 2023, 2024 and 2026 texts at
 * www-assets.palloliitto.fi (research notes of open item 111, FIN.md). No text before 2023 was found, so the build
 * applies the same rule in every season (Max, 3 November 2026: "do whatever real life does"; the gap is recorded
 * under item 118). The texts do not say which club takes a barred team's place. Max decided on 3 November 2026: the
 * next club in the table.
 *
 * The original. The game plays the Premier Division (Veikkausliiga, key [0x9cf7e8], FinPrm) and the First Division
 * (FinFirst, key [0x9cf7f8]), which has two groups: its own table (North) and the League_Stage of stage 0 (South,
 * FinFirst +0xc [0]). The four Second Division groups (keys 0x9cf7fc North, 0x9cf800, 0x9cf804, 0x9cf808) only hold
 * clubs. After the groups FinFirst::advance_stage 0x593f90 calls FinFirst::create_play_off_stages 0x594080
 * (at 0x594067). It builds:
 * - stage 1, the promotion play-off: rows 0 and 1 of each group (0x594295..0x5942ce: North 1st, South 2nd,
 *   North 2nd, South 1st). Its winner gets status 5 (FinFirst::update_staff_records_for_status 0x594810, stage 1
 *   code 1, 0x5948a3) and goes up. Its other finalist (status 2 in the stage's list, +0xa7 rows of 6 bytes, +5)
 *   plays stage 3;
 * - the clubs of the Second Division: it lists the Finnish clubs of the four groups (0x5940c4..0x59410d, at most 50),
 *   sorts them by reputation and shuffles the top 18 (0x4afbd0 at 0x594158), keeps 4 at +0xee (0x594179..0x59424a,
 *   two on each side of a latitude test) that go up at the season change, and 2 more for stage 2, the relegation
 *   play-off against the 8th of each group (0x5943c2..0x594483).
 * When the Premier Division is complete, advance_stage calls FinFirst::create_premier_play_off_stage 0x594610 (at
 * 0x594058): stage 3 is the stage-1 finalist (status 2 in stage 1's list, 0x59468c) against the Premier Division's
 * club of status 2 (0x5946c4). A First Division club that wins it gets status 0 (0x5949e5).
 * At the season change FinPrm::start_new_season 0x595d40 calls the chain FinPrm::process_promotion_relegation_chain
 * 0x596110 (vtable +0xb0, at 0x595d52). It calls league_promote_and_relegate 0x689c80 in pairs for the Premier
 * Division and the North table (0x59617d), then the South table (0x5961aa): each Premier row of status 3 goes down
 * with the first row of status 0 or 5 of the group (0x689d1a..0x689e07). Then FinPrm::refresh_first_division_clubs
 * 0x595f00 (at 0x595d5a) moves the Second Division winners of stage 2 and the 4 clubs of +0xee into the First
 * Division (0x6830b0 at 0x595f48, 0x59600e) and sends the rows of status 3 of both groups to Second Division North
 * (0x6831a0 at 0x595fb4). No step tests a second team, and club_get_linked_b_team 0x540a50 has no Finnish pair.
 *
 * The fix (season games only, season_game_changes, as the other season code):
 * - Stage 1 (wrap of 0x594080): a second team takes no part. For the call the clubs of rows 0 and 1 of each group
 *   are the two best placed clubs that are not second teams (only the club field of the rows changes, and only
 *   during the call), so the next club in the table plays.
 * - The Second Division picks (the same wrap): a second team whose first team does not play in the Premier Division
 *   is no part of the list (its division reads as none during the call, as Norway's 0x792960), so the next club of
 *   the list takes the place. A second team of a Premier Division club may go up into the First Division (HJK's
 *   Klubi 04 played the Ykkönen and the Ykkösliiga).
 * - Stage 3 (wrap of 0x594610): when the stage-1 finalist is a second team (a save of an older build), the best
 *   placed club of its group that is not a second team and not in stage 1 plays in its place.
 * - The season change (wrap of 0x596110, before the chain): a second team with an up status in a group (an older
 *   save) hands it to the next club of the table; a second team in the Premier Division (an older save) gets status
 *   3 in exchange for a club of status 3 that stays up (the chain moves in pairs); a second team of the First
 *   Division whose first team will not play in the Premier Division next season gets status 3, and the best placed
 *   club of status 3 of its group stays up.
 * - After refresh_first_division_clubs (wrap of 0x595f00): a second team now in the First Division whose first team
 *   is not in the Premier Division goes to Second Division North (League::relegate_club_to_division 0x6831a0, as
 *   0x595fb4). Each place left over goes to the club of the Second Division groups with the highest reputation that
 *   may play there and did not just come down (League::move_club_to_division 0x6830b0, as 0x59600e).
 * - The club moves of a new game (season_clubs.c) put no Finnish second team into the Premier Division. A second
 *   team that a new game leaves in the First Division above or beside its first team goes down at the first season
 *   change, as in an older save (the fill does not know yet where the first team ends; no test database has such a
 *   case).
 *
 * The link. No club field names a club's first team. A second team is a name of season_clubs_reserve_name (a last
 * word II, 2, B or Reserves), a last word III or 3, or a name of the table below; its first team is the Finnish
 * club named by the name without that last word, or by the table. No fuzzy match is made. Not in the table:
 * Atlantis Akatemia and KPV juniorit, the first teams of Atlantis FC and KPV in the old data (rules research
 * nor-fin.md); FC Jazz-juniorit, the youth club of FC Jazz that took the name FC Jazz in 2010 (en.wikipedia.org/
 * wiki/FC_Jazz; FC Jazz-juniorit ry runs the first team, fi.wikipedia.org/wiki/FC_Jazz); Oulun Palloseura Juniorit
 * and the Kerho 07 of the 2017 to 2021 data, for which no source says whether they are a reserve or a farm club
 * (open item 118). A second team with no first team found keeps only the bar from the Premier Division and its
 * play-off. In the 50 test databases every linked second team of the First and Second Divisions finds its first
 * team. The entries for SJK Seinäjoki II and III are needed
 * where the data names the first team Seinäjoen Jalkapallokerho.
 *
 * Test switches (tests/seasons/fin_second_run.py): RECOMP_FIN_SECOND=off plays as the original; RECOMP_FIN_SECOND_LOG
 * =1 writes what the code does and a census at each season change; RECOMP_TEST_FIN_SECOND_TOP=1 puts each second team
 * of the First Division at the top of its group before 0x594080 (as if it won the group; the rows move for good, also
 * with RECOMP_FIN_SECOND=off); RECOMP_TEST_FIN_SECOND_OLD
 * =<n> plays as the original until n Finnish season changes are done, and with RECOMP_TEST_FIN_SECOND_TOP=1 gives
 * stage 1's place to a second team in those changes (status 5), so the original sends it up into the Premier Division
 * (an older save, made in memory); RECOMP_TEST_FIN_SECOND_DROP=1 sends the first team of each second team of the
 * First Division down at the season change (it gets status 3, and the club of status 3 with the best place stays
 * up), so the second team must go down too; RECOMP_TEST_FIN_SECOND_LATE=1 plays a save of an older build made after
 * the play-off: a second team is the stage-1 finalist when stage 3 is built and holds stage 1's place (status 5) at
 * the season change; RECOMP_TEST_FIN_SECOND_REP=1 gives each second team of the Second Division groups the highest
 * reputation before 0x594080, so the original would pick it. The test switches act in season games whether the fix is
 * on or off, so a run with RECOMP_FIN_SECOND=off shows the fault. */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_fin_second.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_FINLAND 0x9cf2fcu
#define KEY_PREMIER 0x9cf7e8u           /* Veikkausliiga */
#define KEY_FIRST 0x9cf7f8u             /* FinFirst; its table is the North group */
#define KEY_FIRST_N 0x9cf7ecu           /* the comp records of the two groups (FIN.md) */
#define KEY_FIRST_S 0x9cf7f0u
#define KEY_SECOND1 0x9cf7fcu           /* the 4 Second Division groups: North, West, South, East */
#define SECOND_GROUPS 4
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define COMPS 0xae23d0u
#define CLUB_SIZE 0x245u
#define COMP_SIZE 0x6bu
#define CLUB_NAME 0x04u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_REPUTATION 0x80u
#define LEAGUES 0xadadfcu
#define LEAGUE_STAGES 0x0cu
#define LEAGUE_CLUBS 0x3eu
#define LEAGUE_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define STAGE_LIST 0xa7u                /* cup stage: rows of 6 bytes, +0 the club, +5 the status */
#define LIST_ROW 6u
#define ST_TITLE 5
#define ST_UP 0
#define ST_DOWN 3
#define ST_NONE 0xff
#define F_MOVE_UP 0x006830b0u
#define F_MOVE_DOWN 0x006831a0u

static int readable(uint32_t p) { return rt_readable(p); }

static int env_is(const char *name, const char *value)
{
    const char *v = getenv(name);
    return v && !strcmp(v, value);
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = env_is("RECOMP_FIN_SECOND_LOG", "1");
    return log;
}

static int test_top(void)
{
    static int v = -1;
    if (v < 0) v = env_is("RECOMP_TEST_FIN_SECOND_TOP", "1");
    return v;
}

/* RECOMP_TEST_FIN_SECOND_LATE=1: a save of an older build made after a second team won its place in stage 1. */
static int test_late(void)
{
    static int v = -1;
    if (v < 0) v = env_is("RECOMP_TEST_FIN_SECOND_LATE", "1");
    return v;
}

/* RECOMP_TEST_FIN_SECOND_REP=1: each second team of the Second Division groups gets the highest reputation before
 * 0x594080, so the original would pick it. */
static int test_rep(void)
{
    static int v = -1;
    if (v < 0) v = env_is("RECOMP_TEST_FIN_SECOND_REP", "1");
    return v;
}

static int test_drop(void)
{
    static int v = -1;
    if (v < 0) v = env_is("RECOMP_TEST_FIN_SECOND_DROP", "1");
    return v;
}

/* RECOMP_TEST_FIN_SECOND_OLD=<n>: the season changes left to play as the original. Read once. */
static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = env_is("RECOMP_FIN_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_FIN_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

static uint32_t comp_of(uint32_t key) { return rd32(g_mem, COMPS) + rd32(g_mem, key) * COMP_SIZE; }

static int div_is(uint32_t div, uint32_t key)
{
    return readable(div) && (int32_t)rd32(g_mem, div) == (int32_t)rd32(g_mem, key);
}

static int in_premier(uint32_t div) { return div_is(div, KEY_PREMIER); }
static int in_first(uint32_t div) { return div_is(div, KEY_FIRST) || div_is(div, KEY_FIRST_N) || div_is(div, KEY_FIRST_S); }

static int in_second(uint32_t div)
{
    for (uint32_t k = 0; k < SECOND_GROUPS; k++)
        if (div_is(div, KEY_SECOND1 + 4u * k)) return 1;
    return 0;
}

static uint32_t league_of(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!readable(leagues) || id < 0 || id > 0xffff) return 0;
    uint32_t league = rd32(g_mem, leagues + 4u * (uint32_t)id);
    return readable(league) ? league : 0;
}

static uint32_t table_of(uint32_t league, int *n)
{
    *n = 0;
    if (!readable(league)) return 0;
    uint32_t table = rd32(g_mem, league + LEAGUE_TABLE);
    int rows = (int16_t)rd16(g_mem, league + LEAGUE_CLUBS);
    if (!readable(table) || rows <= 0 || rows > 64) return 0;
    *n = rows;
    return table;
}

/* The South group: the League_Stage of FinFirst's stage 0. */
static uint32_t south_of(uint32_t first)
{
    if (!readable(first)) return 0;
    uint32_t stages = rd32(g_mem, first + LEAGUE_STAGES);
    if (!readable(stages)) return 0;
    uint32_t s = rd32(g_mem, stages);
    return readable(s) ? s : 0;
}

/* A Finnish club: club +0x53 is the nation of key 0x9cf2fc (KEY_FINLAND, include/key_index.h), as Norway's test
 * of key 0x9cf408. */
static int finnish(uint32_t club)
{
    if (!readable(club)) return 0;
    uint32_t nation = rd32(g_mem, club + CLUB_NATION);
    return readable(nation) && (int32_t)rd32(g_mem, nation) == (int32_t)rd32(g_mem, KEY_FINLAND);
}

/* Second teams that the name rule does not read, and their first teams (no fuzzy match). Names in latin-1, as the
 * data. Sources, read on 3 November 2026: HJK's reserve is Klubi 04 (fi.wikipedia.org/wiki/Klubi_04); SJK Akatemia,
 * Kerho 07 in 2013 to 2015, is SJK's reserve (en.wikipedia.org/wiki/SJK_Akatemia); FC Lahti's reserve side is its
 * Akatemia (en.wikipedia.org/wiki/FC_Lahti). Read on 4 November 2026: Vaasan Palloseura-Juniorit (Second Division
 * West in the 2015 to 2017 data) is the men's side of VPS Juniorit ry, VPS J Akatemia, which played the Kakkonen
 * until 2019 and is "Vaasan Palloseuran farmijoukkue", VPS's farm team (fi.wikipedia.org/wiki/VPS_Akatemia,
 * fi.wikipedia.org/wiki/VPS_Juniorit). Palloliitto rules 2024, section 6.12, bar a farm club of a top-tier club
 * from the top tier and its play-off, as a reserve. */
static const char *const HJK[] = {"HJK Helsinki", "Helsingin Jalkapalloklubi", "Helsingen Jalkapalloklubi", "HJK", 0};
static const char *const SJK[] = {"SJK Sein\xe4joki", "Sein\xe4joen Jalkapallokerho", "SJK", 0};
static const char *const LAHTI[] = {"FC Lahti", 0};
static const char *const VPS[] = {"Vaasan Palloseura", "VPS Vaasa", "VPS", 0};
static const struct { const char *name; const char *const *first; } LINKS[] = {
    {"HJK Reserves", HJK}, {"Klubi 04", HJK}, {"Klubi 04 Helsinki", HJK}, {"Klubi-04", HJK},
    {"Klubi-04 Helsinki", HJK}, {"HJK Klubi 04 Helsinki", HJK}, {"Talenttiklubi Klubi-04", HJK},
    {"SJK Sein\xe4joki II", SJK}, {"SJK Sein\xe4joki III", SJK}, {"SJK Akatemia", SJK}, {"SJK Kerho 07", SJK},
    {"FC Kerho 07", SJK},
    {"FC Lahti Akatemia", LAHTI}, {"Lahti Akatemia", LAHTI},
    {"Vaasan Palloseura-Juniorit", VPS}, {"VPS-Juniorit", VPS}, {"VPS-J", VPS}, {"VPS J Akatemia", VPS},
    {"VPS Akatemia", VPS},
};

/* The last word of a name, lower case, and the length of the name before it (0 when one word). */
static size_t last_word(const char *name, char *last, size_t size)
{
    size_t n = 0, w = 0;
    while (n < 51 && name[n]) n++;
    while (n && name[n - 1] == ' ') n--;
    size_t end = n;
    while (n && name[n - 1] != ' ') n--;
    for (size_t i = n; i < end && w < size - 1; i++) last[w++] = (char)(name[i] | 0x20);
    last[w] = 0;
    while (n && name[n - 1] == ' ') n--;
    return n;
}

static int link_of(const char *name)
{
    for (size_t i = 0; i < sizeof LINKS / sizeof LINKS[0]; i++)
        if (!strncmp(name, LINKS[i].name, 51)) return (int)i;
    return -1;
}

/* 1 for a second (or third) team by its name. */
static int second_name(const char *name)
{
    if (link_of(name) >= 0 || season_clubs_reserve_name(name)) return 1;
    char last[8];
    return last_word(name, last, sizeof last) && (!strcmp(last, "3") || !strcmp(last, "iii"));
}

static int second_team(uint32_t club) { return readable(club) && finnish(club) && second_name(win_str(club + CLUB_NAME)); }

/* The first team of a second team (0 when none is found). */
static uint32_t first_team(uint32_t club)
{
    const char *name = win_str(club + CLUB_NAME);
    const char *want[5] = {0};
    char base[52];
    int l = link_of(name);
    if (l >= 0) {
        for (int k = 0; LINKS[l].first[k] && k < 4; k++) want[k] = LINKS[l].first[k];
    } else {
        char last[16];
        size_t n = last_word(name, last, sizeof last);
        if (!n) return 0;
        memcpy(base, name, n);
        base[n] = 0;
        want[0] = base;
    }
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int k = 0; want[k]; k++)
        for (int i = 0; readable(table) && i < count; i++) {
            uint32_t c = table + (uint32_t)i * CLUB_SIZE;
            if (c != club && !strncmp(win_str(c + CLUB_NAME), want[k], 51) && finnish(c)) return c;
        }
    return 0;
}

static const char *cname(uint32_t club) { return win_str(club + CLUB_NAME); }

/* 1 when a second team may play in the First Division: its first team plays in the Premier Division, or none is
 * found. */
static int first_allowed(uint32_t club)
{
    uint32_t f = first_team(club);
    return !f || in_premier(rd32(g_mem, f + CLUB_DIVISION));
}

int season_fin_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !readable(club) || !readable(div) || !in_premier(div)) return 0;
    return second_team(club);
}

/* The statuses that 0x689c80 moves up (0x689d47..0x689d4e). */
static int up_status(int s) { return s == ST_TITLE || s == ST_UP; }

static void log_table(const char *what, uint32_t league)
{
    if (!log_on()) return;
    int n;
    uint32_t t = table_of(league, &n);
    fprintf(stderr, "recomp: fin second: table %s:", what);
    for (int i = 0; i < n; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE;
        fprintf(stderr, " [%d %.30s %d]", i + 1, cname(rd32(g_mem, row)), (int)g_mem[row + ROW_STATUS] == 0xff ? -1 :
                (int)g_mem[row + ROW_STATUS]);
    }
    fprintf(stderr, "\n");
}

/* A second team that holds an up status in a group (an older save: it won stage 1, status 5, or stage 3, status 0)
 * hands it to the next club in the table: the best placed club that is not a second team and has no up status and
 * no status 3. */
static void hand_on(uint32_t league, const char *what)
{
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        uint8_t s = g_mem[row + ROW_STATUS];
        if (!up_status(s) || !second_team(rd32(g_mem, row))) continue;
        g_mem[row + ROW_STATUS] = ST_NONE;
        int given = 0;
        for (int j = 0; j < n && !given; j++) {
            uint32_t to = table + (uint32_t)j * ROW_SIZE;
            uint8_t t = g_mem[to + ROW_STATUS];
            if (up_status(t) || t == ST_DOWN || second_team(rd32(g_mem, to))) continue;
            given = 1;
            g_mem[to + ROW_STATUS] = s;
            if (log_on())
                fprintf(stderr, "recomp: fin second: %.51s takes status %d in place of a second team (%.51s) in %s\n",
                        cname(rd32(g_mem, to)), s, cname(rd32(g_mem, row)), what);
        }
        /* No club can take it: 0x689c80 finds no partner for one Premier row of status 3, which so stays up. */
        if (!given && log_on())
            fprintf(stderr, "recomp: fin second: no club of %s takes the status of %.51s\n", what, cname(rd32(g_mem, row)));
    }
}

/* The best placed row of status 3 that is not a second team gets 0xff (stays up). Returns 1 when one did. */
static int stay_up(uint32_t league, const char *what)
{
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_STATUS] != ST_DOWN || second_team(rd32(g_mem, row))) continue;
        g_mem[row + ROW_STATUS] = ST_NONE;
        if (log_on()) fprintf(stderr, "recomp: fin second: %.51s stays up in %s\n", cname(rd32(g_mem, row)), what);
        return 1;
    }
    return 0;
}

/* The row of a club in a league's table (0 when none). */
static uint32_t row_of(uint32_t league, uint32_t club)
{
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, table + (uint32_t)i * ROW_SIZE) == club) return table + (uint32_t)i * ROW_SIZE;
    return 0;
}

/* 1 when a club will play in the Premier Division next season, by the statuses before the chain. */
static int premier_next(uint32_t club, uint32_t prm, uint32_t north, uint32_t south)
{
    uint32_t r = row_of(prm, club);
    if (r) return g_mem[r + ROW_STATUS] != ST_DOWN;
    if (!(r = row_of(north, club))) r = row_of(south, club);
    return r && (g_mem[r + ROW_STATUS] == ST_UP || g_mem[r + ROW_STATUS] == ST_TITLE);
}

/* The places of the First Division that the clubs of the Second Division take after refresh (second teams that go
 * down with no club of status 3 left to stay up). */
static int first_extra;

static void census(const char *when)
{
    if (!log_on()) return;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), cp = 0, sp = 0, cf = 0, sf = 0, bad = 0;
    for (int i = 0; readable(table) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
        if (!finnish(club)) continue;
        int s = second_name(cname(club));
        if (in_premier(div)) cp++, sp += s;
        else if (in_first(div)) {
            cf++;
            sf += s;
            bad += s && !first_allowed(club);
        }
    }
    fprintf(stderr, "recomp: fin second: census %s: Premier %d clubs, %d second teams; First Division %d clubs, %d "
            "second teams, %d below no first team of the Premier\n", when, cp, sp, cf, sf, bad);
}

/* The test switch TOP: each second team of a group moves to the top row of its table (whole rows, kept). */
static void test_to_top(uint32_t league)
{
    int n;
    uint32_t table = table_of(league, &n);
    int top = 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        if (!second_team(rd32(g_mem, row))) continue;
        uint8_t keep[ROW_SIZE];
        memcpy(keep, g_mem + row, ROW_SIZE);
        memmove(g_mem + table + (uint32_t)(top + 1) * ROW_SIZE, g_mem + table + (uint32_t)top * ROW_SIZE,
                (size_t)(i - top) * ROW_SIZE);
        memcpy(g_mem + table + (uint32_t)top * ROW_SIZE, keep, ROW_SIZE);
        if (log_on()) fprintf(stderr, "recomp: fin second: test: %.51s to row %d\n", cname(rd32(g_mem, table +
                              (uint32_t)top * ROW_SIZE)), top + 1);
        top++;
    }
}

/* Stage 1 of a group: the clubs of rows 0 and 1 become the two best placed clubs of rows 0 to 6 that are not second
 * teams (0x594080 reads row 7 for stage 2, 0x5943c2, so no club of row 7 or below is taken). The club fields that
 * change are saved in undo (row, club), and *nu counts them. */
static void stage1_clubs(uint32_t league, uint32_t *undo, int *nu, const char *what)
{
    int n;
    uint32_t table = table_of(league, &n);
    if (n < 2) return;
    uint32_t pick[2];
    int np = 0;
    for (int i = 0; i < n && i < 7 && np < 2; i++) {
        uint32_t club = rd32(g_mem, table + (uint32_t)i * ROW_SIZE);
        if (!second_team(club)) pick[np++] = club;
    }
    for (int r = 0; r < np; r++) {
        uint32_t row = table + (uint32_t)r * ROW_SIZE, was = rd32(g_mem, row);
        if (was == pick[r]) continue;
        undo[2 * *nu] = row;
        undo[2 * *nu + 1] = was;
        ++*nu;
        wr32(g_mem, row, pick[r]);
        if (log_on() && second_team(was))
            fprintf(stderr, "recomp: fin second: %.51s may not play the promotion play-off (%s)\n", cname(was), what);
        if (log_on()) fprintf(stderr, "recomp: fin second: %.51s plays the promotion play-off in place of a second "
                              "team (%s)\n", cname(pick[r]), what);
    }
}

/* FinFirst::create_play_off_stages 0x594080, thiscall, no arguments. */
void o_00594080(void);
RT_WRAP(00594080, fin_second_playoffs)
{
    uint32_t self = cpu.ecx, table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!season_game_changes() || self != league_of(KEY_FIRST) || !readable(table) || count <= 0) {
        o_00594080();
        return;
    }
    uint32_t south = south_of(self);
    if (log_on())       /* the name test and the link, for each Finnish club of the played and the picked divisions */
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
            if (!finnish(club) || !(in_premier(div) || in_first(div) || in_second(div))) continue;
            uint32_t f = second_team(club) ? first_team(club) : 0;
            fprintf(stderr, "recomp: fin second: name: %.51s: %s%.51s\n", cname(club),
                    !second_team(club) ? "first team" : f ? "second team of " : "second team, no first team found",
                    f ? cname(f) : "");
        }
    if (test_rep())
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE;
            if (in_second(rd32(g_mem, club + CLUB_DIVISION)) && second_team(club)) wr16(g_mem, club + CLUB_REPUTATION, 10000);
        }
    if (test_top()) {
        test_to_top(self);
        test_to_top(south);
    }
    log_table("First Division North before the play-offs", self);
    log_table("First Division South before the play-offs", south);
    int on = rule_on();
    uint32_t undo[16];
    int nu = 0;
    int *held = NULL;
    uint32_t *div_of = NULL;
    int nheld = 0;
    if (on) {
        stage1_clubs(self, undo, &nu, "North");
        stage1_clubs(south, undo, &nu, "South");
        held = malloc(sizeof *held * (size_t)count);
        div_of = malloc(sizeof *div_of * (size_t)count);
        for (int i = 0; held && div_of && i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
            if (!in_second(div) || !second_team(club) || first_allowed(club)) continue;
            held[nheld] = i;
            div_of[nheld++] = div;
            wr32(g_mem, club + CLUB_DIVISION, 0);
        }
    }
    o_00594080();
    for (int k = nu - 1; k >= 0; k--) wr32(g_mem, undo[2 * k], undo[2 * k + 1]);
    for (int k = 0; k < nheld; k++) {
        uint32_t club = table + (uint32_t)held[k] * CLUB_SIZE;
        wr32(g_mem, club + CLUB_DIVISION, div_of[k]);
        if (log_on()) fprintf(stderr, "recomp: fin second: %.51s may not go up from %.51s (its first team does not play "
                              "in the Premier Division)\n", cname(club), win_str(div_of[k] + 4));
    }
    free(held);
    free(div_of);
    if (log_on() && !rt_unwinding) {
        uint32_t stages = rd32(g_mem, self + LEAGUE_STAGES);
        uint32_t s1 = readable(stages) ? rd32(g_mem, stages + 4) : 0;
        if (readable(s1)) {
            int n = (int16_t)rd16(g_mem, s1 + LEAGUE_CLUBS);
            uint32_t list = rd32(g_mem, s1 + STAGE_LIST);
            fprintf(stderr, "recomp: fin second: promotion play-off:");
            for (int i = 0; readable(list) && i < n && i < 8; i++) {
                uint32_t c = rd32(g_mem, list + (uint32_t)i * LIST_ROW);
                fprintf(stderr, " [%.40s%s]", readable(c) ? cname(c) : "?", second_team(c) ? " (a second team)" : "");
            }
            fprintf(stderr, "\n");
        }
        for (int k = 0; k < 4; k++) {
            uint32_t id = rd32(g_mem, self + 0xee + 4u * (uint32_t)k);
            if ((int32_t)id >= 0 && (int)id < count)
                fprintf(stderr, "recomp: fin second: %.51s will go up from the Second Division%s\n",
                        cname(table + id * CLUB_SIZE), second_team(table + id * CLUB_SIZE) ? " (a second team)" : "");
        }
    }
}

/* FinFirst::create_premier_play_off_stage 0x594610, thiscall, no arguments. */
void o_00594610(void);
RT_WRAP(00594610, fin_second_premier_playoff)
{
    uint32_t self = cpu.ecx;
    uint32_t stages = readable(self) ? rd32(g_mem, self + LEAGUE_STAGES) : 0;
    uint32_t s1 = readable(stages) ? rd32(g_mem, stages + 4) : 0;
    uint32_t slot = 0, was = 0;
    if (self == league_of(KEY_FIRST) && readable(s1) && season_game_changes()) {
        int n = (int16_t)rd16(g_mem, s1 + LEAGUE_CLUBS);
        uint32_t list = rd32(g_mem, s1 + STAGE_LIST);
        for (int i = 0; readable(list) && i < n && i < 8 && !slot; i++)
            if (g_mem[list + (uint32_t)i * LIST_ROW + 5] == 2) slot = list + (uint32_t)i * LIST_ROW;
        if (slot && test_late()) {
            /* The test: the finalist of an older save is the second team of the North group with the best place. */
            int t;
            uint32_t table = table_of(self, &t);
            for (int i = 0; i < t; i++)
                if (second_team(rd32(g_mem, table + (uint32_t)i * ROW_SIZE))) {
                    wr32(g_mem, slot, rd32(g_mem, table + (uint32_t)i * ROW_SIZE));
                    if (log_on()) fprintf(stderr, "recomp: fin second: test: %.51s is the stage-1 finalist\n",
                                          cname(rd32(g_mem, slot)));
                    break;
                }
        }
        was = slot ? rd32(g_mem, slot) : 0;
        if (slot && second_team(was) && rule_on()) {
            /* The best placed club of its group that is not a second team and not in stage 1. */
            uint32_t group = row_of(self, was) ? self : south_of(self), pick = 0;
            int t;
            uint32_t table = table_of(group, &t);
            for (int i = 0; i < t && !pick; i++) {
                uint32_t c = rd32(g_mem, table + (uint32_t)i * ROW_SIZE);
                int in_s1 = 0;
                for (int j = 0; j < n && j < 8; j++) in_s1 |= rd32(g_mem, list + (uint32_t)j * LIST_ROW) == c;
                if (!in_s1 && !second_team(c)) pick = c;
            }
            if (pick) {
                wr32(g_mem, slot, pick);
                if (log_on()) fprintf(stderr, "recomp: fin second: %.51s plays the Premier play-off in place of a "
                                      "second team (%.51s)\n", cname(pick), cname(was));
            } else {
                slot = 0;
            }
        } else {
            slot = 0;
        }
    }
    o_00594610();
    if (slot) wr32(g_mem, slot, was);
}

/* FinPrm::process_promotion_relegation_chain 0x596110, thiscall (pairwise) ret 4. */
void o_00596110(void);
RT_WRAP(00596110, fin_second_chain)
{
    uint32_t prm = cpu.ecx;
    if (!season_game_changes() || prm != league_of(KEY_PREMIER)) {
        o_00596110();
        return;
    }
    census("before");
    first_extra = 0;
    uint32_t first = league_of(KEY_FIRST), south = south_of(first);
    uint32_t groups[2] = {first, south};
    const char *gname[2] = {"First Division North", "First Division South"};
    if ((old_left > 0 && test_top()) || test_late()) {
        /* An older save: stage 1's place (status 5) went to the second team with the best place in the groups. */
        uint32_t from = 0, to = 0;
        for (int g = 0; g < 2; g++) {
            int n;
            uint32_t t = table_of(groups[g], &n);
            for (int i = 0; i < n; i++) {
                uint32_t row = t + (uint32_t)i * ROW_SIZE;
                if (g_mem[row + ROW_STATUS] == ST_TITLE && !from) from = row;
                if (!to && second_team(rd32(g_mem, row)) && g_mem[row + ROW_STATUS] != ST_DOWN) to = row;
            }
        }
        if (from && to) {
            g_mem[from + ROW_STATUS] = ST_NONE;
            g_mem[to + ROW_STATUS] = ST_TITLE;
            if (log_on()) fprintf(stderr, "recomp: fin second: test: %.51s has status 5 (stage 1's place)\n",
                                  cname(rd32(g_mem, to)));
        }
    }
    if (test_drop()) {
        /* The first team of each second team of the First Division goes down; the best placed club of status 3
         * that is not such a first team stays up in its place, so the pairs keep their count. */
        /* f: the first teams that go down; mine: every first team of a second team of the groups or of the picks,
         * none of which stays up in exchange. */
        uint32_t f[8], mine[16];
        int nf = 0, nm = 0;
        uint32_t clubs = rd32(g_mem, CLUBS);
        int count = (int)rd32(g_mem, CLUB_COUNT);
        for (int g = 0; g < 3; g++) {
            int n = 0;
            uint32_t t = g < 2 ? table_of(groups[g], &n) : 0;
            for (int i = 0; i < (g < 2 ? n : 4); i++) {
                uint32_t c;
                if (g < 2) {
                    c = rd32(g_mem, t + (uint32_t)i * ROW_SIZE);
                } else {
                    uint32_t id = readable(first) ? rd32(g_mem, first + 0xee + 4u * (uint32_t)i) : 0xffffffffu;
                    c = (int32_t)id >= 0 && (int)id < count ? clubs + id * CLUB_SIZE : 0;
                }
                uint32_t ft = c && second_team(c) ? first_team(c) : 0, fr = ft ? row_of(prm, ft) : 0;
                if (!fr) continue;
                if (nm < 16) mine[nm++] = ft;
                if (g_mem[fr + ROW_STATUS] != ST_DOWN && nf < 8) f[nf++] = ft;
            }
        }
        int n;
        uint32_t t = table_of(prm, &n);
        for (int k = 0; k < nf; k++)
            for (int i = 0; i < n; i++) {
                uint32_t row = t + (uint32_t)i * ROW_SIZE, c = rd32(g_mem, row);
                int linked = 0;
                for (int j = 0; j < nm; j++) linked |= mine[j] == c;
                if (g_mem[row + ROW_STATUS] != ST_DOWN || linked || second_team(c)) continue;
                g_mem[row + ROW_STATUS] = ST_NONE;
                g_mem[row_of(prm, f[k]) + ROW_STATUS] = ST_DOWN;
                if (log_on()) fprintf(stderr, "recomp: fin second: test: %.51s goes down, %.51s stays up\n",
                                      cname(f[k]), cname(c));
                break;
            }
    }
    log_table("Premier Division at the season change", prm);
    log_table("First Division North at the season change", first);
    log_table("First Division South at the season change", south);
    if (rule_on()) {
        for (int g = 0; g < 2; g++) hand_on(groups[g], gname[g]);
        /* An older save: a second team in the Premier Division goes down in exchange for a club of status 3. */
        int n;
        uint32_t t = table_of(prm, &n);
        for (int i = n - 1; i >= 0; i--) {
            uint32_t row = t + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
            if (!second_team(club) || g_mem[row + ROW_STATUS] == ST_DOWN) continue;
            if (!stay_up(prm, "the Premier Division")) {
                if (log_on()) fprintf(stderr, "recomp: fin second: %.51s stays in the Premier Division one more "
                                      "season (no place goes down for it)\n", cname(club));
                continue;
            }
            g_mem[row + ROW_STATUS] = ST_DOWN;
            if (log_on()) fprintf(stderr, "recomp: fin second: %.51s goes down from the Premier Division\n",
                                  cname(club));
        }
        /* A second team of the First Division whose first team will not play in the Premier Division. */
        for (int g = 0; g < 2; g++) {
            uint32_t tg = table_of(groups[g], &n);
            for (int i = n - 1; i >= 0; i--) {
                uint32_t row = tg + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
                if (g_mem[row + ROW_STATUS] == ST_DOWN || !second_team(club)) continue;
                uint32_t f = first_team(club);
                if (!f || premier_next(f, prm, first, south)) continue;
                g_mem[row + ROW_STATUS] = ST_DOWN;
                if (log_on()) fprintf(stderr, "recomp: fin second: %.51s goes down from the %s (its first team %.51s "
                                      "will not play in the Premier Division)\n", cname(club), gname[g], cname(f));
                if (!stay_up(groups[g], gname[g])) first_extra++;
            }
        }
    }
    uint32_t table = rd32(g_mem, CLUBS);
    int count = readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0;
    char *was_prm = log_on() && count > 0 ? calloc((size_t)count, 1) : NULL;
    for (int i = 0; was_prm && i < count; i++)
        was_prm[i] = (char)in_premier(rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION));
    o_00596110();
    for (int i = 0; was_prm && !rt_unwinding && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!was_prm[i] && in_premier(rd32(g_mem, club + CLUB_DIVISION)))
            fprintf(stderr, "recomp: fin second: %.51s goes up to the Premier Division%s\n", cname(club),
                    second_team(club) ? " (a second team)" : "");
    }
    free(was_prm);
}

/* FinPrm::refresh_first_division_clubs 0x595f00, thiscall, no arguments. */
void o_00595f00(void);
RT_WRAP(00595f00, fin_second_refresh)
{
    uint32_t self = cpu.ecx, table = rd32(g_mem, CLUBS);
    int count = readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0;
    if (!season_game_changes() || self != league_of(KEY_PREMIER) || count <= 0) {
        o_00595f00();
        return;
    }
    char *was_first = calloc((size_t)count, 1);
    for (int i = 0; was_first && i < count; i++)
        was_first[i] = (char)in_first(rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION));
    o_00595f00();
    if (rule_on() && !rt_unwinding) {
        uint32_t second1 = comp_of(KEY_SECOND1), firstc = comp_of(KEY_FIRST);
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE;
            if (!in_first(rd32(g_mem, club + CLUB_DIVISION)) || !second_team(club) || first_allowed(club)) continue;
            uint32_t a[3] = {club, second1, 1};
            Cpu saved = cpu;
            cpu.ecx = self;
            rt_callback(F_MOVE_DOWN, 3, a);
            if (!rt_unwinding) cpu = saved;
            first_extra++;
            if (log_on()) fprintf(stderr, "recomp: fin second: %.51s goes down from the First Division (its first team "
                                  "does not play in the Premier Division)\n", cname(club));
        }
        /* The places left over: the club of the Second Division groups with the highest reputation that may play in
         * the First Division and did not just come down from it. No random number is taken. */
        for (; first_extra > 0 && !rt_unwinding; first_extra--) {
            int best = -1, best_rep = -1;
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * CLUB_SIZE;
                if ((was_first && was_first[i]) || !in_second(rd32(g_mem, club + CLUB_DIVISION)) || !finnish(club) ||
                    (second_team(club) && !first_allowed(club)))
                    continue;
                int rep = (int16_t)rd16(g_mem, club + CLUB_REPUTATION);
                if (rep > best_rep) best = i, best_rep = rep;
            }
            if (best < 0) break;
            uint32_t club = table + (uint32_t)best * CLUB_SIZE, a[3] = {club, firstc, 1};
            Cpu saved = cpu;
            cpu.ecx = self;
            rt_callback(F_MOVE_UP, 3, a);
            if (!rt_unwinding) cpu = saved;
            if (log_on()) fprintf(stderr, "recomp: fin second: %.51s takes a place of the First Division\n", cname(club));
        }
        first_extra = 0;
    }
    if (log_on() && was_first && !rt_unwinding)
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE;
            if (!was_first[i] && in_first(rd32(g_mem, club + CLUB_DIVISION)))
                fprintf(stderr, "recomp: fin second: %.51s goes up to the First Division%s\n", cname(club),
                        second_team(club) ? " (a second team)" : "");
        }
    free(was_first);
    if (!rt_unwinding) {
        census("after");
        if (old_left > 0) old_left--;
    }
}
