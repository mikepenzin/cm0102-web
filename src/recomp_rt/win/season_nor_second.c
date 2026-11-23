/* Norway: a second team does not go up into the First Division (open item 111, 0.7.0).
 *
 * The real rule. NFF Kampreglement § 2-7 (1): a club's senior teams play in different divisions; (a) a second team
 * plays at most in the division below the club's higher team, and (b) the second team of a club in Eliteserien or
 * the 1. divisjon goes up no higher than the 2. divisjon. So no second team plays in the top two tiers, which are the
 * two divisions that the game plays. § 5-12 (5): the place of a team that may not go up goes to the best placed team
 * of the same group that may go up. The bar is older than the present text (example: Viking 2
 * won its group of the 2. divisjon in 2005 and FK Haugesund went up). Sources, read on 3 November 2026: the NFF
 * Kampreglement on Lovdata (https://lovdata.no/dokument/NFFF/nfffor/2014-03-09-1, amended to 24 February 2026; the
 * text of § 2-7 dates from 17 January 2023), and https://no.wikipedia.org/wiki/2._divisjon_fotball_for_menn (the
 * 2005 case, and the earlier rule: to 2014 the 2. divisjon team of an Eliteserien club went down to the 3. divisjon
 * when its club went down to the 1. divisjon). The wording of 2001 was not found.
 *
 * The original. The 2. divisjon is not played: its eight groups (keys 0x9cf6c4..0x9cf6e0) only hold clubs. At the
 * start of each season NorPrm::start_new_season 0x7927a0 calls nor_prm_swap_first_and_second_division_clubs
 * 0x792960 (at 0x7927bb). That function lists the Norwegian clubs (club +0x53 the nation [0x9cf408]) whose division
 * (club +0x57) is one of the eight groups (0x792a1b..0x792a7b; a club with no division is not listed, 0x792a1e),
 * sorts the list by reputation and shuffles the top 12 (comp_util_sort_and_shuffle_top_clubs 0x4afbd0 at
 * 0x792a9c), moves the first 4 into the First Division [0x9cf6c0] (League::move_club_to_division 0x6830b0 at
 * 0x792aca), then sends the relegated rows of the First Division (status 3) to group 1 (0x6831a0 at 0x792b1d).
 * The original links no Norwegian second team to its club: club_get_linked_b_team 0x540a50 holds key club pairs
 * of Spain, Germany and Portugal only (the nation test at 0x540af8..0x540b10). So a second team with a high
 * reputation goes up, and in a long game it can meet its own club.
 *
 * The fix. A Norwegian second or third team (season_clubs_reserve_name: a last word 2, II or B and the other
 * reserve forms; or a last word 3 or III) is no part of that list: for the call of 0x792960 its division reads as
 * none, then it gets its group back. This is the original's own rule for Spain (spa_first_exchange_second_division_
 * b_clubs drops a reserve side whose senior side plays in the tier from its list, 0x8504c9..0x8504ec), and the next
 * club of the list takes the place, as § 5-12 (5). The name is the only link the data gives: no field of the club
 * record names its club, and the real rule bars every second team from the top two tiers, so the test needs no
 * club.
 *
 * The club moves of a new game (season_clubs.c) put no Norwegian second or third team into Eliteserien or the First
 * Division either (season_nor_barred, through season_second_barred), in every fill group and for a club that the database has there.
 *
 * Old saves (Max, 3 November 2026: "do whatever real life does"). A game of 0.6.1 or earlier can hold second teams
 * in the First Division or in Eliteserien. The real rule bars them there, so each goes down at the next season
 * change and the next club stays up, as § 5-12 (5) fills a place. How the original sets the places: League::
 * update_positions_and_status 0x684cd0 (vtable +0x94) sorts the table (0x6827d0 at 0x684d18), gives each position
 * a zone from the places +0xbe..+0xc1 and sets a row's status (+0x37) when the row can no longer leave its zone:
 * 5 the title, 0 promotion, 1 the play-off, 3 relegation. A row of status 0xfc is no part of the zones from the
 * top (0x684dea..0x684df4, 0x684e46..0x684e53) and gets neither the title, promotion nor the play-off (0x68512e..
 * 0x685137, 0x68517f..0x68518b), so the next row takes its place; in the zone of relegation it gets 3 (0x68525e).
 * This is the original's own mark for a B side whose senior side plays above (set at 0x5dd942 for Germany, through
 * club_get_linked_b_team 0x540a50). The fix:
 * - Before each 0x684cd0 of the First Division, a second team gets status 0xfc (unless it has 3), so the next club
 *   takes the title, promotion and the play-off. NorFirst::create_play_off_stage 0x790e20 takes the row of
 *   status 1 (0x790e97); when a save comes from the days after the table was complete, the wrap of 0x790e20 first
 *   hands the places on to the next clubs (st_shift_up).
 * - At the season change, before the chain of NorPrm (vtable +0xb0, 0x689c20, called at 0x7927b3; it calls
 *   league_promote_and_relegate 0x689c80 in pairs, 0x689c60), each second team of the First Division gets status 3,
 *   and the best placed club of status 3 gets 0xff and stays up. 0x792960 sends every row of status 3 to group 1
 *   (0x792aef..0x792b1d). When the second teams outnumber the clubs that go down, the extra places of the First
 *   Division go to the clubs of the groups with the highest reputation (League::move_club_to_division 0x6830b0, as
 *   0x792aca), after the original's four, but never to a club that the original just sent down.
 * - A second team of Eliteserien gets status 3 in the same way, but only in exchange for a club of status 3 that
 *   stays up: the chain moves a row down only with a partner that goes up (pairwise), so a second team beyond the
 *   places that go down stays one more season (logged). Each one that reaches the First Division so goes on to group
 *   1 in the wrap of 0x792960 (League::relegate_club_to_division 0x6831a0, as 0x792b1d), so before the chain one
 *   more club of status 3 of the First Division stays up for it (or, with none left, one more club of the groups
 *   goes up). Such a club gets two relegation rows in its history (0x4cd030 in 0x689c80 and in 0x6831a0), as it
 *   goes down two divisions. 0x792b40 (at 0x7927c2) then shares the groups out by latitude, as for every club that
 *   goes down.
 * A club that stays up or takes a place keeps the news and the history row of its earlier status (0x684cd0 sends
 * them through vtable +0x40 when it sets a status).
 * Eliteserien gets no 0xfc: its play-off takes the row of status 2 (NorFirst 0x790e20), which a row of 0xfc never
 * gets (0x685274..0x68527f).
 *
 * Season games only (season_game_changes, as the other season code): the base game (3.9.68, which has no Norwegian
 * second team) keeps the original. The test switch RECOMP_NOR_SECOND=off lists and moves the second teams as the
 * original does (the log and the test switch below still work). RECOMP_NOR_SECOND_LOG=1 writes the clubs that stay
 * out, with the group they keep, the clubs that go up and down, and a count of the second teams before and after
 * each season change. RECOMP_TEST_NOR_SECOND_REP=1 (tests only) gives each second team of the eight groups the
 * highest reputation before the call, so that the original would take them first. RECOMP_TEST_NOR_SECOND_OLD=<n>
 * (tests only) plays as the original until n Norwegian season changes are done, as a game of an older build.
 * RECOMP_TEST_NOR_SECOND_NO_MARK=1 (tests only) gives no 0xfc before 0x684cd0, as a save of an older build made
 * after the table was complete, so that the wraps of 0x790e20 and the season change hand the places on.
 *
 * The wraps of 0x684cd0 and 0x689c20, the mark st_mark and the hand-on st_shift_up live in season_second_teams.c,
 * which the other nations share (open items 114 to 122). */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_nor_second.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_NORWAY 0x9cf408u
#define KEY_PREMIER 0x9cf6bcu           /* Eliteserien */
#define KEY_FIRST 0x9cf6c0u             /* the Norwegian First Division */
#define KEY_GROUP1 0x9cf6c4u            /* the 8 groups of the Second Division, 0x9cf6c4..0x9cf6e0 */
#define GROUPS 8
#define KEY_THIRD 0x9cf738u            /* the Norwegian Third Division (not played) */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE ST_CLUB_SIZE
#define CLUB_NAME ST_CLUB_NAME
#define CLUB_DIVISION ST_CLUB_DIVISION
#define CLUB_REPUTATION ST_CLUB_REPUTATION
#define ROW_SIZE ST_ROW_SIZE
#define ROW_STATUS ST_ROW_STATUS
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */

#define readable st_readable
#define env_is st_env_is
#define league_of st_league_of
#define table_of st_table_of
#define comp_of st_comp_of
#define call_this st_call_this

/* 1 when a division record is one of the eight groups. */
static int in_group(uint32_t div)
{
    if (!readable(div)) return 0;
    int32_t id = (int32_t)rd32(g_mem, div);
    for (uint32_t k = 0; k < GROUPS; k++)
        if (id == (int32_t)rd32(g_mem, KEY_GROUP1 + 4u * k)) return 1;
    return 0;
}

static int norwegian(uint32_t club) { return st_nation_is(club, KEY_NORWAY); }

/* 1 for a second or third team: the reserve names of season_clubs_reserve_name, or a last word 3 or III (the data
 * has "Odds BK 3" and "Rosenborg 3"; § 2-7 places every lower team of a club below its higher team). */
static int lower_team(const char *name)
{
    if (season_clubs_reserve_name(name)) return 1;
    char last[8];
    size_t n = 0, w = 0;
    while (n < 51 && name[n]) n++;
    while (n && name[n - 1] == ' ') n--;
    while (n && name[n - 1] != ' ' && w < sizeof last - 1) last[w++] = (char)(name[--n] | 0x20);   /* reversed */
    last[w] = 0;
    return n && (!strcmp(last, "3") || !strcmp(last, "iii"));
}

/* RECOMP_TEST_NOR_SECOND_OLD=<n>: the season changes left to play as the original. Read once. */
static int old_left = -1;

/* RECOMP_NOR_SECOND=off (test switch): the original's list and club moves. Read once. */
static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = env_is("RECOMP_NOR_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_NOR_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = env_is("RECOMP_NOR_SECOND_LOG", "1");
    return log;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

static int barred_club(uint32_t club)
{
    return readable(club) && norwegian(club) && lower_team(win_str(club + CLUB_NAME));
}

/* Up to k clubs of status 3 (not second teams), the best placed first, stay up (0xff). Returns how many. */
static int stay_up(uint32_t league, int k, const char *what)
{
    int n, done = 0;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n && done < k; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_STATUS] != ST_DOWN || barred_club(rd32(g_mem, row))) continue;
        g_mem[row + ROW_STATUS] = ST_NONE;
        done++;
        if (log_on()) fprintf(stderr, "recomp: nor second: %.51s stays up in %s\n", win_str(rd32(g_mem, row) + CLUB_NAME),
                              what);
    }
    return done;
}

/* At the season change: each second team of the league goes down (status 3), and the best placed club of status 3
 * stays up (0xff) in its place. limit 1 (Eliteserien): a second team goes down only in exchange for such a club.
 * *down gets the second teams that go down. Returns those with no club staying up in exchange. */
static int season_down(uint32_t league, int limit, const char *what, int *down)
{
    int n;
    uint32_t table = table_of(league, &n);
    int stay[64], nstay = 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_STATUS] == ST_DOWN && !barred_club(rd32(g_mem, row))) stay[nstay++] = i;
    }
    int used = 0, extra = 0, k = 0;
    for (int i = n - 1; i >= 0; i--) {         /* the lowest placed second team first */
        uint32_t row = table + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ROW_STATUS] == ST_DOWN || !barred_club(club)) continue;
        if (limit && used == nstay) {
            if (log_on()) fprintf(stderr, "recomp: nor second: %.51s stays in %s one more season (no place goes "
                                  "down for it)\n", win_str(club + CLUB_NAME), what);
            continue;
        }
        g_mem[row + ROW_STATUS] = ST_DOWN;
        k++;
        if (log_on()) fprintf(stderr, "recomp: nor second: %.51s goes down from %s\n", win_str(club + CLUB_NAME), what);
        if (used < nstay) {
            uint32_t up = table + (uint32_t)stay[used++] * ROW_SIZE;
            g_mem[up + ROW_STATUS] = ST_NONE;
            if (log_on()) fprintf(stderr, "recomp: nor second: %.51s stays up in %s\n",
                                  win_str(rd32(g_mem, up) + CLUB_NAME), what);
        } else {
            extra++;
        }
    }
    *down = k;
    return extra;
}

/* The second teams of a division, and its clubs. */
static void census(uint32_t div, int *clubs, int *seconds)
{
    *clubs = *seconds = 0;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; readable(table) && readable(div) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) != div) continue;
        ++*clubs;
        *seconds += barred_club(club);
    }
}

static void log_census(const char *when)
{
    if (!log_on()) return;
    int c1, s1, c2, s2;
    census(comp_of(KEY_PREMIER), &c1, &s1);
    census(comp_of(KEY_FIRST), &c2, &s2);
    fprintf(stderr, "recomp: nor second: census %s: Eliteserien %d clubs, %d second teams; First Division %d clubs, "
            "%d second teams\n", when, c1, s1, c2, s2);
}

/* Item 232 (0.7.2, forum report of 13 November 2026): a second team in the Second Division whose club does not
 * allow it there.
 *
 * The real rule by season. Sources, read on 13 November 2026 (notes: run-logs/0.7.2-nor-b/research-nor.md):
 * the NFF Tinghefte of the Forbundsting of 7 to 9 March 2014, Forslag 17, pages 27 and 28 (https://www.fotball.no/
 * globalassets/nff-org/forbundstinget/tinghefte2014_materieorig_3_rev4.pdf), which prints the Kampreglement then in
 * force, and the NFF Kampreglement on Lovdata (above). no.wikipedia "2. divisjon fotball for menn" gives 2015 as
 * the first season of the new text.
 * - To the season 2014, Kampreglement § 2-10 (1): (a) a second team plays at most in the division below the club's
 *   higher team; (b) the second team of a tier 1 club goes no higher than the 2. divisjon; (c) the second team of a
 *   tier 2 club goes no higher than the 3. divisjon. So only the second team of an Eliteserien club plays the
 *   Second Division, and it goes down when its club goes down (the Tinghefte: "2. lag har blitt flyttet ned en
 *   divisjon"). Cases (no.wikipedia tables): Stabæk 2 in 2004, Molde 2 and HamKam 2 in 2006, Odd Grenland 2 and
 *   Start 2 in 2007, Fredrikstad 2 in 2009, Tromsø 2 in 2013. The text of 2001 to 2012 was not found; the cases of
 *   2004 to 2013 show the same rule, so it runs in every season to 2014.
 * - From the season 2015, § 2-7 (1): (b) the second team of a tier 1 or tier 2 club goes no higher than the
 *   2. divisjon; (a) as before. So the second team of an Eliteserien or First Division club may play the Second
 *   Division, and goes down when its club comes down into the Second Division (Vålerenga 2 stayed in the 2. divisjon
 *   in 2024 after its club went down from Eliteserien).
 * Who takes the place: the cases show a club of the same group (2004 to 2007) or the best twelfth (2009, 2013) that
 * stays up. The game does not play the Second Division and sends no club down from it, and 0x792b40 (at 0x7927c2)
 * shares its clubs out into the eight groups again, so no club needs to take the place.
 *
 * The fix (season games, season_game_changes as the rest of this file, after the call of 0x792960 in its wrap). The
 * rule is that of the season that starts (Eliteserien's year +0x40 still holds the season played, 0x7927f8 adds 1
 * after 0x792960 and 0x792b40), through season_rule_year: with changing rules off, the rule of the start year holds
 * in every season. The 3.9.68 base data has no club of nation Norway with a second-team name (its B sides have no
 * nation), so a base game is unchanged with changing rules on too.
 * each second team of the groups whose first team plays below Eliteserien (to 2014) or below the First Division
 * (from 2015) goes to the Third Division [0x9cf738], which the game does not play, through League::
 * relegate_club_to_division 0x6831a0 (as the original sends a club down at 0x792b1d). So does a third team (last
 * word 3 or III), which (a) places below the club's second team. A second team with no first team found stays.
 * Accepted effect: the game moves no club out of the Third Division (the original's 0x792960 takes only the groups),
 * so a second team sent there stays there when its club goes back up. The original treats every club of the Third
 * Division so.
 *
 * The first team of a second team: a pair of PARENTS, else the Norwegian club named as the second team less its last
 * word (case aside), else the one whose plain form is the same (plain_form: the letters folded to a to z, "-" and
 * "/" as spaces, the club-form words FK, SK, IF, IL, BK, IK, FF, TF, FC and Fotball left out). Over the 46 season
 * databases every second team of the groups finds its first team this way, except Gjøvik-Lyn 2 of Autumn 2013 and
 * March 2014 (its club there is Gjøvik FotballForening: the tie in those years, I don't know). */
static const char *const PARENTS[][4] = {
    {"\xc5lesund IF B", "Aalesunds FK"},
    {"\xc5lesund IF 2", "Aalesunds FK"},
    {"V\xe5lerenga Fotball II", "V\xe5lerenga Fotball", "V\xe5lerenga Fotball Elite"},
    {"Valerenga Oslo II", "Valerenga Oslo", "V\xe5lerenga Fotball"},
    {"Sparta Sarpsborg 2", "Sparta Sarpsborg", "Sarpsborg 08 FF", "Sarpsborg 08"},
    {"Lyn Fotball 2", "Lyn Oslo", "FC Lyn Oslo"},
    {"Lyn Fotball B", "FC Lyn Oslo", "Lyn Oslo"},
    {"Viking FK Stavanger II", "Viking FK", "Viking Stavanger"},
    {"Byasen Trondheim II", "By\xe5sen TF"},
    {"By\xe5sen IL II", "By\xe5sen TF"},
    {"Ranheim IL Fotball Topp 2", "Ranheim IL"},
};

/* The plain form of a name (Windows-1252), less its last word when drop_last. */
static void plain_form(const char *name, int drop_last, char *out, size_t room)
{
    static const char *const forms[] = {"fk", "sk", "if", "il", "bk", "ik", "ff", "tf", "fc", "fotball"};
    char buf[128];
    size_t n = 0;
    for (size_t i = 0; i < 51 && name[i] && n + 3 < sizeof buf; i++) {
        unsigned char c = (unsigned char)name[i];
        const char *f = NULL;
        if (c == 0xe6 || c == 0xc6) f = "ae";
        else if (c == 0xf8 || c == 0xd8 || c == 0xf6 || c == 0xd6 || c == 0xf3 || c == 0xd3) f = "o";
        else if (c == 0xe5 || c == 0xc5 || c == 0xe4 || c == 0xc4 || c == 0xe1 || c == 0xe0) f = "a";
        else if (c == 0xe9 || c == 0xe8 || c == 0xc9) f = "e";
        else if (c == 0xfc || c == 0xdc) f = "u";
        if (f) { while (*f) buf[n++] = *f++; continue; }
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        buf[n++] = (char)((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? c : ' ');
    }
    buf[n] = 0;
    if (drop_last) {
        while (n && buf[n - 1] == ' ') buf[--n] = 0;
        while (n && buf[n - 1] != ' ') buf[--n] = 0;
    }
    out[0] = 0;
    size_t w = 0;
    char *save = NULL;
    for (char *t = strtok_r(buf, " ", &save); t; t = strtok_r(NULL, " ", &save)) {
        int skip = 0;
        for (size_t k = 0; k < sizeof forms / sizeof *forms; k++) skip |= !strcmp(t, forms[k]);
        if (skip || w + strlen(t) + 2 > room) continue;
        if (w) out[w++] = ' ';
        strcpy(out + w, t);
        w += strlen(t);
    }
}

/* The first team of a Norwegian second team (0 when none is found). */
static uint32_t first_team(uint32_t club)
{
    const char *name = win_str(club + CLUB_NAME);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!readable(table)) return 0;
    for (size_t r = 0; r < sizeof PARENTS / sizeof *PARENTS; r++) {
        if (strncmp(name, PARENTS[r][0], 52)) continue;
        for (int k = 1; k < 4 && PARENTS[r][k]; k++)
            for (int i = 0; i < count; i++) {
                uint32_t c = table + (uint32_t)i * CLUB_SIZE;
                if (norwegian(c) && !strncmp(win_str(c + CLUB_NAME), PARENTS[r][k], 52)) return c;
            }
        return 0;
    }
    size_t at = strnlen(name, 51);
    while (at && name[at - 1] == ' ') at--;
    while (at && name[at - 1] != ' ') at--;
    while (at && name[at - 1] == ' ') at--;
    if (!at) return 0;
    char want[64], plain[64], other[64];
    snprintf(want, sizeof want, "%.*s", (int)at, name);
    plain_form(name, 1, plain, sizeof plain);
    uint32_t by_plain = 0;
    for (int i = 0; i < count; i++) {
        uint32_t c = table + (uint32_t)i * CLUB_SIZE;
        if (c == club || !norwegian(c)) continue;
        const char *n = win_str(c + CLUB_NAME);
        if (lower_team(n)) continue;
        if (!strncasecmp(n, want, 52)) return c;
        if (plain[0]) {
            plain_form(n, 0, other, sizeof other);
            if (strcmp(other, plain)) continue;
            if (!by_plain) by_plain = c;
            else if (log_on()) fprintf(stderr, "recomp: nor second: %.51s: two first teams by the plain form, %.51s "
                                       "taken, %.51s left\n", name, win_str(by_plain + CLUB_NAME), n);
        }
    }
    return by_plain;
}

/* The season that starts at the change: Eliteserien's year +0x40 plus 1. */
static int season_starts(uint32_t prm_league)
{
    return readable(prm_league) ? (int16_t)rd16(g_mem, prm_league + 0x40u) + 1 : 0;
}

/* The year whose rule holds in season y: season_rule_year (the start year with changing rules off), or
 * RECOMP_TEST_NOR_SECOND_YEAR (tests only). */
static int rule_year_of(int y)
{
    const char *v = getenv("RECOMP_TEST_NOR_SECOND_YEAR");
    if (v && atoi(v) > 0) return atoi(v);
    return season_rule_year(y);
}

/* The level of a division, for the log. */
static const char *level_word(uint32_t d)
{
    if (readable(d) && d == comp_of(KEY_PREMIER)) return "Eliteserien";
    if (readable(d) && d == comp_of(KEY_FIRST)) return "the First Division";
    return in_group(d) ? "the Second Division" : "a lower division";
}

/* 1 for a third team (a last word 3 or III): § 2-7 (1) (a) places it below the club's second team, which plays the
 * Second Division at most, so it never plays the groups. */
static int third_team(const char *name)
{
    return lower_team(name) && !season_clubs_reserve_name(name);
}

/* 1 when the rule of season y bars a second team from the Second Division while its first team plays div. */
static int pair_barred(uint32_t first_div, int y)
{
    if (readable(first_div) && first_div == comp_of(KEY_PREMIER)) return 0;
    if (readable(first_div) && first_div == comp_of(KEY_FIRST)) return y > 0 && y <= 2014;
    return 1;
}

/* The second teams of the groups, those with no first team, and those that the rule of season y bars there. */
static void log_pairs(const char *when, int season, int y)
{
    if (!log_on()) return;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0, n = 0, none = 0, shared = 0;
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!in_group(rd32(g_mem, club + CLUB_DIVISION)) || !barred_club(club)) continue;
        uint32_t p = first_team(club);
        n++;
        if (third_team(win_str(club + CLUB_NAME))) {
            shared++;
        } else if (!p) {
            none++;
            fprintf(stderr, "recomp: nor second: %.51s has no first team\n", win_str(club + CLUB_NAME));
        } else if (pair_barred(rd32(g_mem, p + CLUB_DIVISION), y)) {
            shared++;
        }
    }
    fprintf(stderr, "recomp: nor second: pairs %s (season %d, rule of %d): %d second teams of the groups, %d with no "
            "first team, %d barred there\n", when, season, y, n, none, shared);
}

/* After 0x792960: each second team of the groups that the rule of season y bars there goes to the Third Division. */
static void pairs_down(uint32_t self, int y)
{
    static int off = -1;
    if (off < 0) off = env_is("RECOMP_NOR_SECOND_PAIRS", "off");    /* test switch: this step alone off */
    if (off) return;
    int32_t id = (int32_t)rd32(g_mem, KEY_THIRD);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0;
    uint32_t third = id >= 0 ? comp_of(KEY_THIRD) : 0;
    for (int i = 0; i < count && !rt_unwinding; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!in_group(rd32(g_mem, club + CLUB_DIVISION)) || !barred_club(club)) continue;
        uint32_t p = first_team(club);
        int is_third = third_team(win_str(club + CLUB_NAME));   /* barred with or without a first team */
        if (!is_third && (!p || !pair_barred(rd32(g_mem, p + CLUB_DIVISION), y))) {
            if (log_on())
                fprintf(stderr, "recomp: nor second: %.51s may play the Second Division (rule of %d: %s)\n",
                        win_str(club + CLUB_NAME), y, p ? level_word(rd32(g_mem, p + CLUB_DIVISION)) : "no first team");
            continue;
        }
        if (!readable(third)) {
            if (log_on()) fprintf(stderr, "recomp: nor second: %.51s stays: the data has no Third Division\n",
                                  win_str(club + CLUB_NAME));
            continue;
        }
        uint32_t a[3] = {club, third, 1};
        call_this(F_MOVE_DOWN, self, 3, a);
        if (log_on()) {
            uint32_t d = p ? rd32(g_mem, p + CLUB_DIVISION) : 0;
            fprintf(stderr, "recomp: nor second: %.51s goes down to the Third Division (rule of %d: %.51s plays %s, "
                    "%.51s%s)\n", win_str(club + CLUB_NAME), y, p ? win_str(p + CLUB_NAME) : "no first team",
                    p ? level_word(d) : "a lower division", readable(d) ? win_str(d + 4) : "none",
                    is_third ? ", a third team" : "");
        }
    }
}

/* The places of the First Division that the clubs of the groups take beyond the original's four (more second teams
 * than clubs of status 3), and the Eliteserien second teams that the chain sends down to it. */
static int first_extra, prm_down;

int season_nor_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !readable(club) || !readable(div) || !norwegian(club)) return 0;
    int32_t id = (int32_t)rd32(g_mem, div);
    if (id != (int32_t)rd32(g_mem, KEY_PREMIER) && id != (int32_t)rd32(g_mem, KEY_FIRST)) return 0;
    return lower_team(win_str(club + CLUB_NAME));
}

void o_00792960(void);
RT_WRAP(00792960, nor_second_exchange)
{
    static int log = -1, test_rep = -1;
    if (log < 0) {
        log = env_is("RECOMP_NOR_SECOND_LOG", "1");
        test_rep = env_is("RECOMP_TEST_NOR_SECOND_REP", "1");
    }
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!season_game_changes() || !readable(table) || count <= 0) {
        o_00792960();
        return;
    }
    int on = rule_on();
    uint32_t self = cpu.ecx, first = comp_of(KEY_FIRST);
    int season = season_starts(self), year = rule_year_of(season);
    log_pairs("before", season, year);
    /* The second teams of the groups, with their divisions, and the clubs of the First Division before the call. */
    int *held = malloc(sizeof *held * (size_t)count);
    uint32_t *div_of = malloc(sizeof *div_of * (size_t)count);
    char *was_first = calloc((size_t)count, 1);
    int nheld = 0;
    for (int i = 0; held && div_of && was_first && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
        was_first[i] = div == first;
        if (!in_group(div) || !norwegian(club) || !lower_team(win_str(club + CLUB_NAME))) continue;
        if (test_rep) wr16(g_mem, club + CLUB_REPUTATION, 10000);
        if (switched_off()) continue;
        held[nheld] = i;
        div_of[nheld++] = div;
        wr32(g_mem, club + CLUB_DIVISION, 0);
    }
    o_00792960();
    for (int k = 0; k < nheld; k++) {
        uint32_t club = table + (uint32_t)held[k] * CLUB_SIZE;
        wr32(g_mem, club + CLUB_DIVISION, div_of[k]);
        if (log) fprintf(stderr, "recomp: nor second: %.51s stays in %.51s\n", win_str(club + CLUB_NAME),
                         win_str(div_of[k] + 4));
    }
    if (on && !rt_unwinding) {
        uint32_t group1 = comp_of(KEY_GROUP1);
        /* A second team in the First Division now came down from Eliteserien: it goes on to group 1. Its place went
         * to a club of status 3 that stayed up before the chain (wrap of 0x689c20). */
        int arrived = 0;
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE;
            if (rd32(g_mem, club + CLUB_DIVISION) != first || !barred_club(club)) continue;
            uint32_t a[3] = {club, group1, 1};
            call_this(F_MOVE_DOWN, self, 3, a);
            arrived++;
            if (log) fprintf(stderr, "recomp: nor second: %.51s goes down from the First Division\n",
                             win_str(club + CLUB_NAME));
        }
        if (arrived > prm_down) first_extra += arrived - prm_down;
        /* Places left over go to the clubs of the groups with the highest reputation. The original sorts its list
         * by reputation and shuffles the top 12 (0x4afbd0 at 0x792a9c), this takes no random number. A club that
         * the original just sent down from the First Division is not one of them. */
        for (; first_extra > 0 && !rt_unwinding; first_extra--) {
            int best = -1, best_rep = -1;
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * CLUB_SIZE;
                if ((was_first && was_first[i]) || !in_group(rd32(g_mem, club + CLUB_DIVISION)) || !norwegian(club) ||
                    barred_club(club))
                    continue;
                int rep = (int16_t)rd16(g_mem, club + CLUB_REPUTATION);
                if (rep > best_rep) best = i, best_rep = rep;
            }
            if (best < 0) break;
            uint32_t a[3] = {table + (uint32_t)best * CLUB_SIZE, first, 1};
            call_this(F_MOVE_UP, self, 3, a);
        }
        first_extra = prm_down = 0;
        pairs_down(self, year);
    }
    if (!rt_unwinding) log_pairs("after", season, year);
    if (log && was_first && !rt_unwinding)
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE;
            if (!was_first[i] && rd32(g_mem, club + CLUB_DIVISION) == first)
                fprintf(stderr, "recomp: nor second: %.51s goes up to the First Division%s\n",
                        win_str(club + CLUB_NAME), lower_team(win_str(club + CLUB_NAME)) ? " (a second team)" : "");
        }
    free(held);
    free(div_of);
    free(was_first);
    if (!rt_unwinding) {
        log_census("after");
        if (old_left > 0) old_left--;
    }
}

/* Before League::update_positions_and_status 0x684cd0 (season_second_teams.c): the second teams of the First
 * Division get 0xfc. */
static void nor_before_status(uint32_t league)
{
    static int no_mark = -1;
    if (no_mark < 0) no_mark = env_is("RECOMP_TEST_NOR_SECOND_NO_MARK", "1");
    if (league == league_of(KEY_FIRST) && !no_mark && rule_on())
        st_mark(league, barred_club, "nor second", "the First Division", log_on());
}

/* RECOMP_TEST_NOR_SECOND_UP=1 (tests only, the old-save runs): the best placed second team of the First Division
 * that has no row of the status takes the status from the lowest placed club that has it, which takes the second
 * team's status. So an old save holds a second team in a promotion or play-off place with any results. */
static void test_up(uint32_t league, uint8_t status)
{
    static int on = -1;
    if (on < 0) on = env_is("RECOMP_TEST_NOR_SECOND_UP", "1");
    int n;
    uint32_t table = on && league ? table_of(league, &n) : 0;
    if (!table) return;
    int second = -1, other = -1;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        int barred = barred_club(rd32(g_mem, row));
        if (barred && (g_mem[row + ROW_STATUS] == status || (status == ST_UP && g_mem[row + ROW_STATUS] == ST_TITLE)))
            return;                                                      /* a second team has it already */
        if (barred && second < 0) second = i;
        if (!barred && g_mem[row + ROW_STATUS] == status) other = i;
    }
    if (second < 0 || other < 0) return;
    uint32_t a = table + (uint32_t)second * ROW_SIZE, b = table + (uint32_t)other * ROW_SIZE;
    g_mem[b + ROW_STATUS] = g_mem[a + ROW_STATUS];
    g_mem[a + ROW_STATUS] = status;
    fprintf(stderr, "recomp: nor second: test: %.51s takes status %d from %.51s\n", win_str(rd32(g_mem, a) + CLUB_NAME),
            status, win_str(rd32(g_mem, b) + CLUB_NAME));
}

/* NorFirst::create_play_off_stage 0x790e20, thiscall, no arguments. */
void o_00790e20(void);
RT_WRAP(00790e20, nor_second_playoff)
{
    if (cpu.ecx == league_of(KEY_FIRST) && rule_on()) test_up(cpu.ecx, ST_PLAYOFF);
    if (cpu.ecx == league_of(KEY_FIRST) && rule_on())
        st_shift_up(cpu.ecx, barred_club, "nor second", "before the play-off", log_on());
    o_00790e20();
}

/* Before and after the promotion chain 0x689c20 (season_second_teams.c): NorPrm::start_new_season calls it first
 * (0x7927b3). For Eliteserien the second teams go down before it. */
/* RECOMP_TEST_NOR_SECOND_DOWN="<club>,<club>" (tests only): the rows of these First Division clubs get status 3
 * before the chain, so 0x792960 sends them down into the groups. */
static void test_down(void)
{
    const char *v = getenv("RECOMP_TEST_NOR_SECOND_DOWN");
    uint32_t first = league_of(KEY_FIRST);
    if (!v || !*v || !first) return;
    int n;
    uint32_t table = table_of(first, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        const char *name = win_str(rd32(g_mem, row) + CLUB_NAME);
        size_t len = strnlen(name, 51);
        if (!len) continue;
        for (const char *p = v; (p = strstr(p, name)) != NULL; p++)
            if ((p == v || p[-1] == ',') && (p[len] == ',' || !p[len])) {
                g_mem[row + ROW_STATUS] = ST_DOWN;
                fprintf(stderr, "recomp: nor second: test: %.51s goes down\n", name);
                break;
            }
    }
}

static char *was_prm;
static int was_count;

static void nor_before_chain(uint32_t self)
{
    free(was_prm);                       /* left by a chain that unwound (no after_chain) */
    was_prm = NULL;
    if (self != league_of(KEY_PREMIER) || !season_game_changes()) return;
    log_census("before");
    first_extra = prm_down = 0;
    uint32_t prm = comp_of(KEY_PREMIER), table = rd32(g_mem, CLUBS);
    int count = readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0;
    was_prm = log_on() && count > 0 ? calloc((size_t)count, 1) : NULL;
    was_count = count;
    for (int i = 0; was_prm && i < count; i++)
        was_prm[i] = rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION) == prm;
    test_down();
    if (switched_off() && !env_is("RECOMP_NOR_SECOND", "off")) test_up(league_of(KEY_FIRST), ST_UP);
    if (rule_on()) {
        uint32_t first = league_of(KEY_FIRST);
        st_shift_up(first, barred_club, "nor second", "at the season change", log_on());
        int down;
        first_extra = season_down(first, 0, "the First Division", &down);
        season_down(self, 1, "Eliteserien", &prm_down);
        /* Each Eliteserien second team that goes down leaves the First Division again (wrap of 0x792960), so one
         * more club of status 3 stays up there. */
        first_extra += prm_down - stay_up(first, prm_down, "the First Division");
    }
}

static void nor_after_chain(uint32_t self)
{
    if (self != league_of(KEY_PREMIER) || !was_prm) return;
    uint32_t prm = comp_of(KEY_PREMIER), table = rd32(g_mem, CLUBS);
    int count = readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0;
    for (int i = 0; i < was_count && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!was_prm[i] && rd32(g_mem, club + CLUB_DIVISION) == prm)
            fprintf(stderr, "recomp: nor second: %.51s goes up to Eliteserien%s\n", win_str(club + CLUB_NAME),
                    barred_club(club) ? " (a second team)" : "");
    }
    free(was_prm);
    was_prm = NULL;
}

static const SecondTeamHooks nor_hooks = {nor_before_status, nor_before_chain, nor_after_chain, season_nor_barred};
SEASON_SECOND_HOOKS(nor_hooks)
