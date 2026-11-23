/* Denmark: no second team in the national tournament (open item 119, 0.7.0).
 *
 * The real rule, season by season. The game plays the top three tiers: the Superliga, the 1st Division and the 2nd
 * Division. Sources, read on 3 November 2026: da.wikipedia "2. division (fodbold)" (https://da.wikipedia.org/wiki/
 * 2._division_(fodbold)), en.wikipedia "Danish 2nd Division", the Divisionsforeningen "Turneringsregler for Herre-DM"
 * 2022/23 and 2023/24 § 7.1 (search excerpts only; the PDFs refused the fetch), and the 0.6.0 rules research
 * (docs/research/rules-0.6.0/den-swe.md, "Reserve and B teams (Q289)", with the DBU table of 2005/06).
 * - From 2010/11: the Superliga second teams moved to a reserve league, and "klubberne ikke måtte have to hold i
 *   Danmarksturneringen" (da.wikipedia). Herre-DM § 7.1: no club takes part with more than one team. So no second
 *   team plays any of the three divisions.
 * - 2005/06 to 2009/10: the 2nd Division admitted up to six (from 2007/08 eight) second teams of Superliga clubs,
 *   which "kunne ikke rykke op i 1. division" (da.wikipedia). The second teams of Silkeborg and Vejle went down with
 *   their first teams in 2006/07 (the 0.6.0 research). So a second team may play the 2nd Division while its first
 *   team plays the Superliga, and never the 1st Division or the Superliga. The second team of another club may not
 *   play the 2nd Division: en.wikipedia gives the admission as "up to eight reserve teams of the Danish Superliga
 *   clubs", and the Silkeborg and Vejle teams left when their clubs left the Superliga.
 * - 2001/02 to 2004/05: no second team played the 2nd Division or above. DBU's annual reports (Årsberetning 2002 to
 *   2005, read through the Wayback Machine on 4 November 2026) show second teams that won a Danmarksserien pool stay
 *   down while the next first team went up: Silkeborg II in 2002 (Jetsmark IF up) and 2004 (Næsby BK up), Esbjerg II
 *   in 2003 and 2004. DBU 2005 and da.wikipedia give second-team promotion as new in 2005. So no second team, of a
 *   Superliga club or another, plays the three divisions.
 * Who takes a barred team's place. DBU's reports of 2006 to 2010 show the next first team of the table each time,
 * and no place left empty: 2005/06 West (Esbjerg II first, Aarhus Fremad up, Thisted to the play-off), 2006/07 East
 * (Brøndby II second, Hvidovre to the play-off), 2006/07 West (Ikast FS second, FC Fyn to the play-off), 2008/09 West
 * (OB II second, Brabrand to the play-off). A Superliga club that went down took its second team out of the 2nd
 * Division by force (Silkeborg II and Vejle II in 2007, DBU 2007). In 2010 the eight second teams left, the 2nd
 * Division sent one club down in place of six and took nine from below in place of six (dbu.dk, 6 March 2010): so
 * a place that a second team leaves goes first to a club that would go down, then to a club from below. The full
 * research: run-logs/0.6.2-s111c-den/research-den-second.md (outside the repo). Max, 4 November 2026: as in real life.
 *
 * The original. DenPrm::start_new_season 0x553aa0 (thiscall, this the Superliga) calls the promotion chain (vtable
 * +0xb0 = 0x689c20, at 0x553ab3), which moves the rows of status 0 or 5 of the 1st and 2nd Division up and the rows
 * of status 3 of the Superliga and the 1st Division down in pairs (league_promote_and_relegate 0x689c80). Then it
 * calls DenPrm::refresh_second_division_clubs 0x553cb0 (at 0x553abb) and adds 1 to the season year +0x40 only after
 * it (0x553b36). 0x553cb0 lists the Danish clubs ([0x9cf2c0]) whose division is A Lower Division [0x9cf764]
 * (0x553d66..0x553db1; the loop stops at the first national team, 0x53b900 at 0x553d67), sorts them by reputation
 * and shuffles the top 12 (comp_util_sort_and_shuffle_top_clubs 0x4afbd0 with 4 picks at 0x553dd8), moves the first 4
 * into the 2nd Division [0x9cf5b4] (League::move_club_to_division 0x6830b0 at 0x553e06), then sends every row of
 * status 3 of the 2nd Division's table to A Lower Division (0x6826d0 and 0x6831a0, 0x553e17..0x553e53). The table is
 * still that of the season just played: the chain moves the clubs, not the rows. Nothing tests a second team, and
 * club_get_linked_b_team 0x540a50 has no Danish pairs. So a second team of A Lower Division with a high reputation
 * goes up into the 2nd Division and on by the table. The databases from October 2017 hold such teams (Aalborg BK II,
 * Aarhus Fremad II, Hobro IK II, Næsby BK II, "Naestved IF (Naestved BK II)"), the Kvalifikationsrækken ones moved
 * into A Lower Division by DenPrm::DenPrm 0x5525e0 (0x552646..0x552689).
 *
 * The fix (season games with changing rules on; the base game and changing rules off keep the original).
 * - A second team: a Danish club whose name passes season_clubs_reserve_name (last word II, 2, B, Reserves, ...) or
 *   ends in 3 or III, except the first teams of FIRST_TEAMS ("Vejgaard B" is Vejgaard Boldspilklub). Its first team:
 *   the Danish club with the name less that word (exact, no fuzzy match), or the pair of PARENTS. Only the rule of
 *   2005/06 to 2009/10 needs the first team.
 * - During the season: before each 0x684cd0 (League::update_positions_and_status; the shared wrap of
 *   season_second_teams.c) of the 1st or 2nd Division, a second team that may not play the division above next season
 *   gets the original's mark 0xfc, so the next club takes the title, promotion and the play-off (0x684dea..0x68518b).
 * - Before the chain: when a save comes from the days after the table was complete, st_shift_up hands the places on.
 * - The wrap of 0x553cb0 (after the chain, so a first team's new division counts): in the order of the divisions,
 *   each second team above its allowed level leaves, and the next club takes its place.
 *   Superliga: it goes to A Lower Division (0x6831a0), and the best placed club of the 1st Division's table still in
 *   the 1st Division goes up (0x6830b0). 1st Division: the same, the place from the 2nd Division's table (that row
 *   gets 0xff, so 0x553cb0 does not send it down). 2nd Division: a second team of its table gets status 3, so
 *   0x553cb0 sends it down; one that the chain just brought down goes to A Lower Division at once. For each place,
 *   the best placed club of status 3 (not a second team) stays up (0xff); with none left, a club of A Lower Division
 *   with the highest reputation goes up after the call (never one just sent down).
 *   For the call a barred second team of A Lower Division reads as no division and gets it back after, so the next
 *   club of the shuffled list goes up (as the original's own rule for Spain, 0x8504c9..0x8504ec).
 * - The club moves of a new game put no barred second team into the three divisions (barred_at_start).
 * A second team above its level in 2005/06 to 2009/10 goes to A Lower Division, not to the 2nd Division: no database
 * of those seasons has a Danish second team, so the case needs an old save that cannot exist (logged if it does).
 *
 * Test switches (tests/seasons/den_second_run.py): RECOMP_DEN_SECOND=off (the original), RECOMP_DEN_SECOND_LOG=1 (the
 * clubs held, moved and the census of each change), RECOMP_TEST_DEN_SECOND_REP=1 (each second team of A Lower Division
 * gets the highest reputation before the call), RECOMP_TEST_DEN_SECOND_YEAR=<y> (the rule of season y in place of the
 * real one), RECOMP_TEST_DEN_SECOND_OLD=<n> (the original until n Danish season changes are done, as a game of an older
 * build), RECOMP_TEST_DEN_SECOND_PLACE=1 (with _OLD: after the original's change, the second teams of A Lower Division
 * swap divisions with the weakest club of the Superliga, the 1st and the 2nd Division in turn, as a long game of an
 * older build), RECOMP_TEST_DEN_SECOND_NO_MARK=1 (no 0xfc, as a save of an older build made after the table was
 * complete), RECOMP_TEST_DEN_SECOND_NO_STAY=1 (no club of status 3 stays up, so the clubs of A Lower Division take
 * every place of the 2nd Division that a second team leaves). */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_DENMARK 0x9cf2c0u
#define KEY_PRM 0x9cf5acu               /* the Superliga (DenCup::collect_entrants 0x551240) */
#define KEY_FIRST 0x9cf5b0u             /* the 1st Division (0x553ad5) */
#define KEY_SECOND 0x9cf5b4u            /* the 2nd Division (0x553cbf, 0x553de7) */
#define KEY_LOWER 0x9cf764u             /* A Lower Division (0x553d8f) */
#define LEAGUE_YEAR 0x40u               /* s16, the season year; 0x553b36 adds 1 after 0x553cb0 */
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define TAG "den second"
#define LEVELS 3                        /* the divisions the game plays: 0 Superliga, 1 1st, 2 2nd Division */

/* Danish first teams whose names the reserve test reads as second teams (exact names of the databases). */
static const char *const FIRST_TEAMS[] = {"Vejgaard B"};

/* Second teams whose first team the name less its last word does not give (database name, first team). */
static const char *const PARENTS[][2] = {
    {"Aalborg Boldspilklub II", "Aalborg BK"},              /* October 2017 */
    {"Naestved IF (Naestved BK II)", "Naestved Boldklub"},  /* October 2022 to May 2026 */
};

static int log_on(void)
{
    static int v = -1;
    if (v < 0) v = st_env_is("RECOMP_DEN_SECOND_LOG", "1");
    return v;
}

/* RECOMP_TEST_DEN_SECOND_OLD=<n>: the Danish season changes left to play as the original. Read once. */
static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_DEN_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_DEN_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int rule_on(void) { return !switched_off() && season_code_on() && season_changing_rules(); }

/* The season whose rule applies: y, or the test year. */
static int rule_year(int y)
{
    static int test = -1;
    if (test < 0) {
        const char *v = getenv("RECOMP_TEST_DEN_SECOND_YEAR");
        test = v ? atoi(v) : 0;
    }
    return test > 0 ? test : y;
}

static int danish(uint32_t club) { return st_readable(club) && st_nation_is(club, KEY_DENMARK); }

static uint32_t division_of(uint32_t club) { return rd32(g_mem, club + ST_CLUB_DIVISION); }

/* The level of a division record: 0 to 2 for the three divisions, LEVELS for any other. */
static int level_of(uint32_t div)
{
    if (!st_readable(div)) return LEVELS;
    static const uint32_t keys[LEVELS] = {KEY_PRM, KEY_FIRST, KEY_SECOND};
    for (int l = 0; l < LEVELS; l++)
        if (div == st_comp_of(keys[l])) return l;
    return LEVELS;
}

/* The last word of a name, lower case, and where it starts (0 when the name is one word). */
static size_t last_word(const char *name, char *out, size_t size)
{
    size_t n = strnlen(name, 51);
    while (n && name[n - 1] == ' ') n--;
    size_t end = n;
    while (n && name[n - 1] != ' ') n--;
    size_t w = 0;
    for (size_t i = n; i < end && w + 1 < size; i++) out[w++] = (char)(name[i] | (name[i] >= 'A' && name[i] <= 'Z' ? 0x20 : 0));
    out[w] = 0;
    return n;
}

static int first_team_name(const char *name)
{
    for (size_t i = 0; i < sizeof FIRST_TEAMS / sizeof *FIRST_TEAMS; i++)
        if (!strncmp(name, FIRST_TEAMS[i], 52)) return 1;
    return 0;
}

static int second_name(const char *name)
{
    if (first_team_name(name)) return 0;
    if (season_clubs_reserve_name(name)) return 1;
    char last[8];
    return last_word(name, last, sizeof last) && (!strcmp(last, "3") || !strcmp(last, "iii"));
}

/* 1 for a Danish second (or third) team. */
static int second_team(uint32_t club)
{
    return danish(club) && second_name(win_str(club + ST_CLUB_NAME));
}

/* The first team of a second team (0 when none is found): the pair of PARENTS, else the Danish club whose name is the
 * second team's name less its last word, exactly (case aside). */
static uint32_t first_team(uint32_t club)
{
    const char *name = win_str(club + ST_CLUB_NAME);
    char want[64];
    want[0] = 0;
    for (size_t i = 0; i < sizeof PARENTS / sizeof *PARENTS; i++)
        if (!strncmp(name, PARENTS[i][0], 52)) snprintf(want, sizeof want, "%s", PARENTS[i][1]);
    if (!want[0]) {
        char last[16];
        size_t at = last_word(name, last, sizeof last);
        while (at && name[at - 1] == ' ') at--;
        if (!at || at >= sizeof want) return 0;
        memcpy(want, name, at);
        want[at] = 0;
    }
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (c == club || !danish(c)) continue;
        const char *n = win_str(c + ST_CLUB_NAME);
        if (!strncasecmp(n, want, 52) && !second_name(n)) return c;
    }
    return 0;
}

static int first_team_in_superliga(uint32_t club)
{
    uint32_t p = first_team(club);
    return p && level_of(division_of(p)) == 0;
}

/* 1 when the rule of season y bars the club from the division of level l (0 to 2). */
static int barred_in(uint32_t club, int l, int y)
{
    if (l < 0 || l >= LEVELS || !second_team(club)) return 0;
    y = rule_year(y);
    if (y >= 2010 || y < 2005) return 1;            /* Herre-DM § 7.1, and before 2005/06 no second team played */
    return l <= 1 || !first_team_in_superliga(club); /* 2005/06 to 2009/10: a Superliga club's, 2nd Division only */
}

/* The level and season the hooks of st_mark and st_shift_up read (they take the club only). */
static int cur_level, cur_year;

static int barred_up(uint32_t club) { return barred_in(club, cur_level - 1, cur_year + 1); }

static int season_of(uint32_t league) { return (int16_t)rd16(g_mem, league + LEAGUE_YEAR); }

static const char *const LEVEL_NAME[LEVELS] = {"the Superliga", "the 1st Division", "the 2nd Division"};

static void den_before_status(uint32_t league)
{
    static int no_mark = -1;
    if (no_mark < 0) no_mark = st_env_is("RECOMP_TEST_DEN_SECOND_NO_MARK", "1");
    if (no_mark || !rule_on()) return;
    for (int l = 1; l < LEVELS; l++)
        if (league == st_league_of(l == 1 ? KEY_FIRST : KEY_SECOND)) {
            cur_level = l;
            cur_year = season_of(league);
            st_mark(league, barred_up, TAG, LEVEL_NAME[l], log_on());
        }
}

static void census(const char *when, int y)
{
    if (!log_on()) return;
    int n[LEVELS + 1] = {0}, s[LEVELS + 1] = {0}, count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (!danish(club)) continue;
        int l = level_of(division_of(club));
        n[l]++;
        s[l] += second_team(club);
    }
    fprintf(stderr, "recomp: den second: census %s: Superliga %d clubs, %d second teams; 1st Division %d clubs, %d "
            "second teams; 2nd Division %d clubs, %d second teams; change into %d/%02d (rule of %d)\n", when, n[0], s[0],
            n[1], s[1], n[2], s[2], y, (y + 1) % 100, rule_year(y));
}

static void den_before_chain(uint32_t self)
{
    if (self != st_league_of(KEY_PRM) || !season_code_on() || !season_changing_rules()) return;
    cur_year = season_of(self);
    census("before", cur_year + 1);
    if (!rule_on()) return;
    for (int l = 1; l < LEVELS; l++) {
        cur_level = l;
        st_shift_up(st_league_of(l == 1 ? KEY_FIRST : KEY_SECOND), barred_up, TAG, "at the season change", log_on());
    }
}

/* The row of the club in a league's table, or -1. */
static int row_of(uint32_t league, uint32_t club)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, table + (uint32_t)i * ST_ROW_SIZE) == club) return i;
    return -1;
}

static const char *name_of(uint32_t club) { return win_str(club + ST_CLUB_NAME); }

/* The best placed club of the league's table that is still in division div, may play the level above in season y
 * and has no status 3 (the clubs just sent down left the table's division). Its row gets 0xff. 0 when none. The log
 * gives its place and why each club above it does not take the place. */
static uint32_t next_up(uint32_t league, uint32_t div, int above, int y)
{
    int n, left = 0, down = 0, barred = 0;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (!st_readable(club) || division_of(club) != div) {
            left++;
            continue;
        }
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN) {
            down++;
            continue;
        }
        if (barred_in(club, above, y)) {
            barred++;
            continue;
        }
        g_mem[row + ST_ROW_STATUS] = ST_NONE;
        if (log_on()) fprintf(stderr, "recomp: den second: next club: %.51s, place %d (above it %d left the division, "
                              "%d go down, %d are barred)\n", name_of(club), i + 1, left, down, barred);
        return club;
    }
    return 0;
}

/* The best placed club of status 3 of the 2nd Division's table that may stay there (0xff). 1 when one stays. */
static int stay_up(uint32_t league, uint32_t div, int y)
{
    static int none = -1;
    if (none < 0) none = st_env_is("RECOMP_TEST_DEN_SECOND_NO_STAY", "1");
    if (none) return 0;
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] != ST_DOWN || !st_readable(club) || division_of(club) != div ||
            barred_in(club, 2, y))
            continue;
        g_mem[row + ST_ROW_STATUS] = ST_NONE;
        if (log_on()) fprintf(stderr, "recomp: den second: %.51s stays up in the 2nd Division\n", name_of(club));
        return 1;
    }
    return 0;
}

static void move(uint32_t fn, uint32_t self, uint32_t club, uint32_t div)
{
    uint32_t a[3] = {club, div, 1};
    st_call_this(fn, self, 3, a);
}

/* RECOMP_TEST_DEN_SECOND_PLACE=1: the second teams of A Lower Division swap divisions with the weakest club of the
 * Superliga, the 1st and the 2nd Division in turn (the state of a long game of an older build). Test only. */
static void test_place(void)
{
    int count;
    uint32_t table = st_clubs(&count), lower = st_comp_of(KEY_LOWER);
    int l = 0;
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (division_of(club) != lower || !second_team(club)) continue;
        uint32_t div = st_comp_of(l == 0 ? KEY_PRM : l == 1 ? KEY_FIRST : KEY_SECOND), weak = 0;
        int rep = 1 << 30;
        for (int j = 0; j < count; j++) {
            uint32_t c = table + (uint32_t)j * ST_CLUB_SIZE;
            int r = (int16_t)rd16(g_mem, c + ST_CLUB_REPUTATION);
            if (division_of(c) == div && danish(c) && !second_team(c) && r < rep) weak = c, rep = r;
        }
        if (!weak) continue;
        wr32(g_mem, weak + ST_CLUB_DIVISION, lower);
        wr32(g_mem, club + ST_CLUB_DIVISION, div);
        fprintf(stderr, "recomp: den second: test: %.51s placed in %s for %.51s\n", name_of(club), LEVEL_NAME[l],
                name_of(weak));
        if (++l == LEVELS) break;
    }
}

static int season_den_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !season_changing_rules() || !danish(club)) return 0;
    return barred_in(club, level_of(div), season_start_year());
}

/* DenPrm::refresh_second_division_clubs 0x553cb0, thiscall (this the Superliga), no arguments. */
void o_00553cb0(void);
RT_WRAP(00553cb0, den_second_exchange)
{
    static int test_rep = -1, test_place_on = -1;
    if (test_rep < 0) {
        test_rep = st_env_is("RECOMP_TEST_DEN_SECOND_REP", "1");
        test_place_on = st_env_is("RECOMP_TEST_DEN_SECOND_PLACE", "1");
    }
    int count;
    uint32_t table = st_clubs(&count);
    if (!season_code_on() || !season_changing_rules() || !table) {
        o_00553cb0();
        return;
    }
    uint32_t self = cpu.ecx;
    int on = rule_on(), y = season_of(self) + 1;    /* the season that starts */
    uint32_t prm = st_comp_of(KEY_PRM), first = st_comp_of(KEY_FIRST), second = st_comp_of(KEY_SECOND);
    uint32_t lower = st_comp_of(KEY_LOWER), first_league = st_league_of(KEY_FIRST);
    uint32_t second_league = st_league_of(KEY_SECOND);
    char *was_top = calloc((size_t)count, 1);       /* in one of the three divisions before the call */
    uint32_t *held = malloc(sizeof *held * (size_t)count);
    if (!was_top || !held) {
        fprintf(stderr, "recomp: den second: no memory for the season change: the original's list runs\n");
        free(was_top);
        free(held);
        o_00553cb0();
        return;
    }
    for (int i = 0; i < count; i++) was_top[i] = level_of(division_of(table + (uint32_t)i * ST_CLUB_SIZE)) < LEVELS;
    int need = 0, nheld = 0;
    if (on) {
        /* The Superliga, then the 1st Division: each barred second team goes to A Lower Division and the next club of
         * the division below's table takes its place, so each place passes down one division. short counts the
         * places of the division of the loop to fill, need those of the 2nd Division. */
        int short_places = 0;
        for (int l = 0; l < 2 && !rt_unwinding; l++) {
            uint32_t div = l ? first : prm, below = l ? second : first, below_league = l ? second_league : first_league;
            for (int i = 0; i < count && !rt_unwinding; i++) {
                uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
                if (division_of(club) != div || !barred_in(club, l, y)) continue;
                if (y < 2010)
                    fprintf(stderr, "recomp: den second: %.51s plays %s in %d/%02d (an old save): it goes to A Lower "
                            "Division\n", name_of(club), LEVEL_NAME[l], y, (y + 1) % 100);
                move(F_MOVE_DOWN, self, club, lower);
                if (log_on()) fprintf(stderr, "recomp: den second: %.51s goes down from %s\n", name_of(club),
                                      LEVEL_NAME[l]);
                short_places++;
            }
            int moved = 0;
            for (; short_places > 0 && !rt_unwinding; short_places--) {
                uint32_t up = next_up(below_league, below, l, y);
                if (!up) {
                    fprintf(stderr, "recomp: den second: no club of %s may take a place of %s\n", LEVEL_NAME[l + 1],
                            LEVEL_NAME[l]);
                    break;
                }
                move(F_MOVE_UP, self, up, div);
                moved++;
                if (log_on()) fprintf(stderr, "recomp: den second: %.51s goes up to %s in place of a second team\n",
                                      name_of(up), LEVEL_NAME[l]);
            }
            short_places = moved;                       /* the places that the division below now lacks */
        }
        need = short_places;
        /* The 2nd Division: a second team of its table gets status 3 (0x553cb0 sends it down), one that the chain
         * brought down goes to A Lower Division now. */
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (division_of(club) != second || !barred_in(club, 2, y)) continue;
            int n, r = row_of(second_league, club);
            uint32_t rows = st_table_of(second_league, &n);
            if (r >= 0 && g_mem[rows + (uint32_t)r * ST_ROW_SIZE + ST_ROW_STATUS] == ST_DOWN) continue;
            if (r >= 0) {
                g_mem[rows + (uint32_t)r * ST_ROW_SIZE + ST_ROW_STATUS] = ST_DOWN;
            } else {
                move(F_MOVE_DOWN, self, club, lower);
            }
            if (log_on()) fprintf(stderr, "recomp: den second: %.51s goes down from the 2nd Division\n", name_of(club));
            need++;
        }
        while (need > 0 && stay_up(second_league, second, y)) need--;
        /* A barred second team of A Lower Division is no part of the list, nor one that left a division above in
         * this call (before 2010/11 a Superliga club's second team may play the 2nd Division, but one sent down from
         * the 1st Division or the Superliga goes to A Lower Division, see the top). */
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (division_of(club) != lower || !danish(club) || !second_team(club)) continue;
            if (test_rep) wr16(g_mem, club + ST_CLUB_REPUTATION, 10000);
            if (rule_year(y) >= 2005 && rule_year(y) < 2010 && log_on()) {
                uint32_t p = first_team(club);
                fprintf(stderr, "recomp: den second: first team of %.51s: %.51s (%s)\n", name_of(club),
                        p ? name_of(p) : "none", !p ? "-" : level_of(division_of(p)) == 0 ? "the Superliga" : "below");
            }
            if (!barred_in(club, 2, y) && !was_top[i]) continue;
            held[nheld++] = club;
            wr32(g_mem, club + ST_CLUB_DIVISION, 0);
        }
    } else if (test_rep) {
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (division_of(club) == lower && second_team(club)) wr16(g_mem, club + ST_CLUB_REPUTATION, 10000);
        }
    }
    if (!rt_unwinding) o_00553cb0();
    for (int k = 0; k < nheld; k++) {
        wr32(g_mem, held[k] + ST_CLUB_DIVISION, lower);
        if (log_on()) fprintf(stderr, "recomp: den second: %.51s stays in A Lower Division\n", name_of(held[k]));
    }
    /* Places left over: the clubs of A Lower Division with the highest reputation (the original sorts by reputation and
     * shuffles the top 12; this takes no random number), never a club that was in the three divisions. */
    for (; on && need > 0 && !rt_unwinding; need--) {
        uint32_t best = 0;
        int best_rep = -1;
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (was_top[i] || division_of(club) != lower || !danish(club) || barred_in(club, 2, y)) continue;
            int rep = (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION);
            if (rep > best_rep) best = club, best_rep = rep;
        }
        if (!best) break;
        move(F_MOVE_UP, self, best, second);
        if (log_on()) fprintf(stderr, "recomp: den second: %.51s goes up to the 2nd Division in place of a second "
                              "team\n", name_of(best));
    }
    if (log_on() && !rt_unwinding)
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (!was_top[i] && division_of(club) == second)
                fprintf(stderr, "recomp: den second: %.51s goes up to the 2nd Division%s\n", name_of(club),
                        second_team(club) ? " (a second team)" : "");
        }
    if (!rt_unwinding && old_left > 0 && --old_left == 0 && test_place_on) test_place();
    free(was_top);
    free(held);
    if (!rt_unwinding) census("after", y);
}

static const SecondTeamHooks den_hooks = {den_before_status, den_before_chain, NULL, season_den_barred};
SEASON_SECOND_HOOKS(den_hooks)
