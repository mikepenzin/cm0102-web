/* France: a reserve side does not go up from the CFA into the National (open item 116, 0.7.0).
 *
 * The real rule, 2001/02 to 2025/26: a reserve side of a professional club plays at most in the CFA (National 2 from
 * 2017/18), tier 4. It never goes up to the National (tier 3). When it wins its group, the next club of the group
 * that may go up takes the place (Bordeaux B won CFA group D in 2007/08 and Bayonne went up; Rennes B won group A in
 * 2016/17 and Cholet went up). Sources, read on 3 November 2026: https://fr.wikipedia.org/wiki/%C3%89quipe_r%C3%A9serve
 * ("les équipes réserves ne pouvant pas monter en National"), https://fr.vikidia.org/wiki/%C3%89quipe_r%C3%A9serve
 * (the two cases), and the rules table research docs/research/rules-0.6.0/fra.md (the CFA rows: "reserve teams
 * cannot go up"). The FFF text itself was not read. 2026/27 (Ligue 3, tier 4 named National 1): the rule is not
 * known (the FFF regulation 2026-2027 refused the fetch), so the game keeps the bar of the seasons before.
 *
 * The original. The CFA is not played for promotion: at the start its clubs go to A Lower Division [0x9cf764], and
 * the record +0x60 of a club (its zone) marks the CFA [0x9cf5d8]. FraFirst::start_new_season 0x5bc4d0 runs the chain
 * of Ligue 1, Ligue 2 and the National (vtable +0xb0 at 0x5bc4f7) and then FraFirst::exchange_cfa_national_clubs
 * 0x5bc780 (at 0x5bc4ff). That function lists every club with a division, a nation and a zone (0x5bc83f..0x5bc852)
 * whose division is A Lower Division (0x5bc854..0x5bc85c) and whose zone is the CFA (0x5bc85e..0x5bc868), with no
 * name or reserve check. It sorts the list by reputation and shuffles the top 4 (comp_util_sort_and_shuffle_top_clubs
 * 0x4afbd0 at 0x5bc8a6), moves the first 6 (FraFirst +0x40 is 2001, 0x5bc8ae) or 4 into the National
 * (League::move_club_to_division 0x6830b0 at 0x5bc8e6), and sends the National rows of status 3 to A Lower Division
 * with the CFA zone (0x6831a0 at 0x5bc92f, +0x60 at 0x5bc957). It moves its count of clubs whatever the list holds
 * (0x5bc8bc..0x5bc8ef), so a list shorter than the count reads past its end. club_get_linked_b_team 0x540a50 links no
 * French club (its nation test 0x540af8..0x540b10 is Spain, Germany and Portugal).
 *
 * The fix. A French reserve side (season_clubs_reserve_name: a last word B, II or 2, a word U19 or U21, and the
 * other reserve forms) is no part of that list: for the call of 0x5bc780 its zone reads as none, then it gets the
 * CFA back. The next club of the list goes up, as the real rule. The name is the only link the data gives, and the
 * real rule bars every reserve side, so the test needs no club. When the clubs left in the list are fewer than the
 * count, the reserve sides with the highest reputation stay in it (logged), so the original never reads past the list
 * (no database holds so few CFA clubs); such a side goes down again at the next season change. The club moves of a
 * new game put no French reserve side into the National, Ligue 2 or Ligue 1 (the hook barred_at_start, asked through
 * season_second_barred in season_clubs.c).
 *
 * Old saves (Max, 3 November 2026: "do whatever real life does"; as Norway, item 111). A game of an older build can
 * hold reserve sides in the National, or above it. The real rule bars them there, so each goes down at the next season
 * change and the next club stays up. The shared wraps of season_second_teams.c call the hooks:
 * - Before each League::update_positions_and_status 0x684cd0 of the National or Ligue 2, a reserve side gets the
 *   original's mark 0xfc (st_mark), so the next club takes the title and promotion.
 * - Before the chain of Ligue 1 (0x689c20, vtable +0xb0 of FraFirst, called at 0x5bc4f7): a reserve side that a save
 *   of an older build left in a promotion place hands it on (st_shift_up); each reserve side of Ligue 1, Ligue 2 and
 *   the National gets status 3, and in exchange the best placed club of status 3 that is no reserve side gets 0xff
 *   and stays up. The chain moves its rows in pairs, so in Ligue 1 and Ligue 2 a reserve side with no such club stays
 *   one more season (logged). The exchange 0x5bc780 sends every National row of status 3 to the CFA, so a National
 *   reserve side with no such club goes down all the same, and one more CFA club takes its place (below).
 * - Each reserve side of Ligue 2 with status 3 then (sent down here, or placed in the relegation zone by the table)
 *   comes down into the National with the chain. Before the chain one more club of status 3 of the National stays
 *   up for each; after the exchange each goes on to A Lower Division with the CFA zone (0x6831a0, as 0x5bc92f and
 *   0x5bc957). Where fewer clubs could stay up than went on, the listed CFA clubs with the highest reputation that
 *   are no reserve side take the places left over (0x6830b0, as 0x5bc8e6).
 * - A reserve side of Ligue 1 goes down one division a season (the chain moves one division), so it reaches the CFA
 *   in three season changes, and it gets no 0xfc in Ligue 1 (it can still take the title in its last season there).
 *   Accepted: no game of an older build is known to hold one.
 * A club that stays up keeps the news and the history row of its earlier status (0x684cd0 sends them when it sets
 * a status).
 *
 * Season games only (season_game_changes): the base game (3.9.68, which has no French reserve side) keeps the
 * original. Test switches: RECOMP_FRA_SECOND=off lists the reserve sides as the original. RECOMP_FRA_SECOND_LOG=1
 * writes each French reserve side with its division and zone, the reserve sides held back and the clubs that go up.
 * RECOMP_TEST_FRA_SECOND_REP=1 (tests only) gives each listed French reserve side the highest reputation before the
 * call, so that the original would take them first. RECOMP_TEST_FRA_SECOND_OLD=<n> (tests only) plays as the original
 * until n French season changes are done, as a game of an older build. RECOMP_TEST_FRA_SECOND_NO_MARK=1 (tests only)
 * gives no 0xfc before 0x684cd0, as a save of an older build made after the table was complete.
 * RECOMP_TEST_FRA_SECOND_PROMOTE=1 (tests only) gives, at a season change played as the original, each reserve side
 * of the National a promotion place of the table (its status swapped with the lowest placed club of status 0), so
 * that an old save with reserve sides in Ligue 2 can be made.
 *
 * Which reserve sides the list holds: a start of November 2020 (France, season game) gave the CFA zone to the 12
 * reserve sides that the database holds in French CFA (the start moves its clubs to A Lower Division), and none to the
 * 31 of the French Lower Division. So the fault occurs in a normal game when such a side's reputation reaches the top
 * four of the list. Which code writes the zone at the start: not found. */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_FRANCE 0x9cf300u
#define KEY_LIGUE1 0x9cf5ccu
#define KEY_LIGUE2 0x9cf5d0u
#define KEY_NATIONAL 0x9cf5d4u
#define KEY_CFA 0x9cf5d8u
#define KEY_LOWER 0x9cf764u             /* A Lower Division */
#define CLUB_ZONE 0x60u
#define FRA_YEAR 0x40u                  /* FraFirst +0x40, the season's year (0x5bc8ae) */
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define MAX_ARRIVALS 64

/* RECOMP_TEST_FRA_SECOND_OLD=<n>: the French season changes left to play as the original. Read once. */
static int old_left = -1;

/* RECOMP_FRA_SECOND=off, or the first n season changes of RECOMP_TEST_FRA_SECOND_OLD (tests only). */
static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_FRA_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_FRA_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = st_env_is("RECOMP_FRA_SECOND_LOG", "1");
    return log;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

/* 1 when the record p (a club_comp record) has the id of the key. */
static int is_key(uint32_t p, uint32_t key)
{
    return st_readable(p) && (int32_t)rd32(g_mem, p) == (int32_t)rd32(g_mem, key);
}

static int barred(uint32_t club)
{
    return st_readable(club) && st_nation_is(club, KEY_FRANCE) &&
           season_clubs_reserve_name(win_str(club + ST_CLUB_NAME));
}

static const char *name_of(uint32_t club) { return win_str(club + ST_CLUB_NAME); }

/* The hook barred_at_start: no French reserve side in the National, Ligue 2 or Ligue 1 at a new game. */
static int fra_barred_at_start(uint32_t club, uint32_t div)
{
    if (switched_off() || !st_readable(div)) return 0;
    if (!is_key(div, KEY_NATIONAL) && !is_key(div, KEY_LIGUE2) && !is_key(div, KEY_LIGUE1)) return 0;
    return barred(club);
}

/* 1 when the club is in the list of 0x5bc780 (0x5bc83f..0x5bc868). */
static int listed(uint32_t club)
{
    uint32_t div = rd32(g_mem, club + ST_CLUB_DIVISION), zone = rd32(g_mem, club + CLUB_ZONE);
    return div && rd32(g_mem, club + ST_CLUB_NATION) && zone && is_key(div, KEY_LOWER) && is_key(zone, KEY_CFA);
}

/* The highest reputation first; the club table's order between equals (qsort is not stable). */
static int by_reputation(const void *a, const void *b)
{
    uint32_t ca = *(const uint32_t *)a, cb = *(const uint32_t *)b;
    int ra = (int16_t)rd16(g_mem, ca + ST_CLUB_REPUTATION), rb = (int16_t)rd16(g_mem, cb + ST_CLUB_REPUTATION);
    return ra != rb ? rb - ra : (ca > cb) - (ca < cb);
}

/* The clubs and reserve sides of Ligue 1, Ligue 2 and the National (log only). */
static void log_census(const char *when)
{
    if (!log_on()) return;
    int count, clubs[3] = {0}, reserves[3] = {0};
    uint32_t table = st_clubs(&count), keys[3] = {KEY_LIGUE1, KEY_LIGUE2, KEY_NATIONAL};
    for (int i = 0; table && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, div = rd32(g_mem, club + ST_CLUB_DIVISION);
        for (int k = 0; k < 3; k++)
            if (div == st_comp_of(keys[k])) clubs[k]++, reserves[k] += barred(club);
    }
    fprintf(stderr, "recomp: fra second: census %s: Ligue 1 %d clubs, %d reserve sides; Ligue 2 %d clubs, %d reserve "
            "sides; the National %d clubs, %d reserve sides\n", when, clubs[0], reserves[0], clubs[1], reserves[1],
            clubs[2], reserves[2]);
}

/* Old saves: the reserve sides of Ligue 2 with status 3 before the chain (they come down into the National and go
 * on to the CFA after the exchange), and the clubs of status 3 of the National that stayed up for them. Set before
 * the chain of Ligue 1, used after the exchange of the same season change. */
static uint32_t arrivals[MAX_ARRIVALS];
static int narrivals, national_saved, national_alone;

/* Up to k clubs of status 3 that are no reserve side, the best placed first, stay up (0xff). Returns how many. */
static int stay_up(uint32_t league, int k, const char *what)
{
    int n, done = 0;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n && done < k; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] != ST_DOWN || barred(rd32(g_mem, row))) continue;
        g_mem[row + ST_ROW_STATUS] = ST_NONE;
        done++;
        if (log_on()) fprintf(stderr, "recomp: fra second: %.51s stays up in %s\n", name_of(rd32(g_mem, row)), what);
    }
    return done;
}

/* At the season change: each reserve side of the league goes down (status 3), and in exchange the best placed club
 * of status 3 stays up (0xff), so the chain or the exchange moves the same count of clubs. pairwise (Ligue 1 and
 * Ligue 2): a reserve side with no club to stay up for it stays one more season (logged), because the chain moves its
 * rows in pairs (league_promote_and_relegate 0x689c80, 0x689c60). The National: the exchange 0x5bc780 sends every row
 * of status 3 down and moves its own count up, so such a side goes down too. Returns those that went down with no
 * club staying up for them. */
static int send_down(uint32_t league, const char *what, int pairwise)
{
    int alone = 0;
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = n - 1; i >= 0; i--) {         /* the lowest placed reserve side first */
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN || !barred(club)) continue;
        if (!stay_up(league, 1, what)) {
            if (pairwise) {
                if (log_on()) fprintf(stderr, "recomp: fra second: %.51s stays in %s one more season (no place goes "
                                      "down for it)\n", name_of(club), what);
                continue;
            }
            alone++;
        }
        g_mem[row + ST_ROW_STATUS] = ST_DOWN;
        if (log_on()) fprintf(stderr, "recomp: fra second: %.51s goes down from %s\n", name_of(club), what);
    }
    return alone;
}

/* RECOMP_TEST_FRA_SECOND_PROMOTE=1 (tests only, at a season change played as the original): each reserve side of the
 * National takes a promotion place, its status swapped with the lowest placed club of status 0. */
static void test_promote(uint32_t national)
{
    int n;
    uint32_t table = st_table_of(national, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        if (s == ST_UP || s == ST_TITLE || !barred(rd32(g_mem, row))) continue;
        for (int j = n - 1; j >= 0; j--) {
            uint32_t other = table + (uint32_t)j * ST_ROW_SIZE;
            if (g_mem[other + ST_ROW_STATUS] != ST_UP || barred(rd32(g_mem, other))) continue;
            g_mem[other + ST_ROW_STATUS] = s;
            g_mem[row + ST_ROW_STATUS] = ST_UP;
            if (log_on()) fprintf(stderr, "recomp: fra second: test: %.51s takes the promotion place of %.51s\n",
                                  name_of(rd32(g_mem, row)), name_of(rd32(g_mem, other)));
            break;
        }
    }
}

/* Before 0x684cd0 of the National or Ligue 2: each reserve side gets the original's mark 0xfc, so the next club takes
 * the title and promotion. */
static void fra_before_status(uint32_t league)
{
    static int no_mark = -1;
    if (no_mark < 0) no_mark = st_env_is("RECOMP_TEST_FRA_SECOND_NO_MARK", "1");
    if (no_mark || !rule_on()) return;
    if (league == st_league_of(KEY_NATIONAL))
        st_mark(league, barred, "fra second", "the National", log_on());
    else if (league == st_league_of(KEY_LIGUE2))
        st_mark(league, barred, "fra second", "Ligue 2", log_on());
}

/* Before the chain of Ligue 1 (FraFirst::start_new_season 0x5bc4d0 calls it through vtable +0xb0 at 0x5bc4f7; the
 * vtable 0x96aa24, set at 0x5bb1a7, holds 0x689c20 there). */
static void fra_before_chain(uint32_t league)
{
    if (league != st_league_of(KEY_LIGUE1)) return;     /* also the chain's own calls for the leagues below */
    narrivals = national_saved = national_alone = 0;
    log_census("before");
    uint32_t ligue2 = st_league_of(KEY_LIGUE2), national = st_league_of(KEY_NATIONAL);
    if (!rule_on()) {
        if (switched_off() && season_game_changes() && st_env_is("RECOMP_TEST_FRA_SECOND_PROMOTE", "1"))
            test_promote(national);
        return;
    }
    st_shift_up(national, barred, "fra second", "at the season change", log_on());
    st_shift_up(ligue2, barred, "fra second", "at the season change", log_on());
    send_down(league, "Ligue 1", 1);
    send_down(ligue2, "Ligue 2", 1);
    national_alone = send_down(national, "the National", 0);
    int n;
    uint32_t table = st_table_of(ligue2, &n);
    for (int i = 0; i < n && narrivals < MAX_ARRIVALS; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN && barred(club)) arrivals[narrivals++] = club;
    }
    national_saved = stay_up(national, narrivals, "the National");
}

static const SecondTeamHooks fra_hooks = {fra_before_status, fra_before_chain, NULL, fra_barred_at_start};
SEASON_SECOND_HOOKS(fra_hooks)

/* After the exchange (old saves): each reserve side that came down from Ligue 2 goes on to A Lower Division with the
 * CFA zone, as the original sends a club down (0x6831a0 at 0x5bc92f, +0x60 at 0x5bc957). The places that no club of
 * status 3 stayed up for, and those of National reserve sides that went down alone, go to the listed CFA clubs with
 * the highest reputation that are no reserve side. */
static void after_exchange(uint32_t self, uint32_t table, int count, const char *was_national)
{
    uint32_t national = st_comp_of(KEY_NATIONAL), lower = st_comp_of(KEY_LOWER), cfa = st_comp_of(KEY_CFA);
    int sent = 0;
    for (int k = 0; k < narrivals && !rt_unwinding; k++) {
        uint32_t club = arrivals[k];
        if (rd32(g_mem, club + ST_CLUB_DIVISION) != national) continue;
        uint32_t a[3] = {club, lower, 1};
        st_call_this(F_MOVE_DOWN, self, 3, a);
        if (rt_unwinding) return;
        wr32(g_mem, club + CLUB_ZONE, cfa);
        sent++;
        if (log_on()) fprintf(stderr, "recomp: fra second: %.51s goes on from the National to the CFA\n",
                              name_of(club));
    }
    if (sent < national_saved && log_on())
        fprintf(stderr, "recomp: fra second: %d clubs stayed up in the National for %d reserve sides that came down\n",
                national_saved, sent);
    for (int extra = sent - national_saved + national_alone; extra > 0 && !rt_unwinding; extra--) {
        int best = -1, best_rep = -1;
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (was_national[i] || !listed(club) || barred(club)) continue;
            int rep = (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION);
            if (rep > best_rep) best = i, best_rep = rep;
        }
        if (best < 0) {
            if (log_on()) fprintf(stderr, "recomp: fra second: no CFA club for %d places left in the National\n", extra);
            break;
        }
        uint32_t club = table + (uint32_t)best * ST_CLUB_SIZE, a[3] = {club, national, 1};
        st_call_this(F_MOVE_UP, self, 3, a);
        if (log_on()) fprintf(stderr, "recomp: fra second: %.51s takes a place left in the National\n", name_of(club));
    }
}

/* FraFirst::exchange_cfa_national_clubs 0x5bc780, thiscall (FraFirst), no arguments. */
void o_005bc780(void);
RT_WRAP(005bc780, fra_second_exchange)
{
    static int test_rep = -1;
    if (test_rep < 0) test_rep = st_env_is("RECOMP_TEST_FRA_SECOND_REP", "1");
    int count;
    uint32_t table = st_clubs(&count), self = cpu.ecx;
    uint32_t *held = NULL, *zone_of = NULL;
    char *was_national = NULL;
    if (season_game_changes() && table && count > 0 && st_readable(self)) {
        held = malloc(sizeof *held * (size_t)count);
        zone_of = malloc(sizeof *zone_of * (size_t)count);
        was_national = calloc((size_t)count, 1);
    }
    if (!held || !zone_of || !was_national) {   /* the base game, or no memory: the original */
        free(held);
        free(zone_of);
        free(was_national);
        narrivals = national_saved = national_alone = 0;
        o_005bc780();
        if (!rt_unwinding && old_left > 0) old_left--;
        return;
    }
    int on = rule_on();
    uint32_t national = st_comp_of(KEY_NATIONAL);
    int places = (int16_t)rd16(g_mem, self + FRA_YEAR) == 2001 ? 6 : 4;     /* 0x5bc8ae..0x5bc8b8 */
    int nheld = 0, others = 0;
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        was_national[i] = rd32(g_mem, club + ST_CLUB_DIVISION) == national;
        int reserve = barred(club);
        if (reserve && log_on()) {
            uint32_t d = rd32(g_mem, club + ST_CLUB_DIVISION), z = rd32(g_mem, club + CLUB_ZONE);
            fprintf(stderr, "recomp: fra second: reserve side %.51s in %.51s, zone %.51s\n", name_of(club),
                    st_readable(d) ? win_str(d + 4) : "none", st_readable(z) ? win_str(z + 4) : "none");
        }
        if (!listed(club)) continue;
        if (!reserve) {
            others++;
            continue;
        }
        if (test_rep) wr16(g_mem, club + ST_CLUB_REPUTATION, 10000);
        if (log_on()) fprintf(stderr, "recomp: fra second: listed reserve side %.51s, reputation %d\n", name_of(club),
                              (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION));
        if (on) held[nheld++] = club;
    }
    /* The list must keep the count of clubs: the reserve sides with the highest reputation stay in it when too few
     * others are left. */
    int keep = places - others;
    if (keep > 0 && nheld) {
        qsort(held, (size_t)nheld, sizeof *held, by_reputation);
        if (keep > nheld) keep = nheld;
        for (int k = 0; k < keep && log_on(); k++)
            fprintf(stderr, "recomp: fra second: %.51s stays in the list (only %d other CFA clubs for %d places)\n",
                    name_of(held[k]), others, places);
        memmove(held, held + keep, sizeof *held * (size_t)(nheld - keep));
        nheld -= keep;
    }
    for (int k = 0; k < nheld; k++) {
        zone_of[k] = rd32(g_mem, held[k] + CLUB_ZONE);
        wr32(g_mem, held[k] + CLUB_ZONE, 0);
    }
    o_005bc780();
    for (int k = 0; k < nheld; k++) {
        wr32(g_mem, held[k] + CLUB_ZONE, zone_of[k]);
        if (log_on()) fprintf(stderr, "recomp: fra second: %.51s stays in the CFA\n", name_of(held[k]));
    }
    if (on && !rt_unwinding) after_exchange(self, table, count, was_national);
    narrivals = national_saved = national_alone = 0;
    if (log_on() && !rt_unwinding) {
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (!was_national[i] && rd32(g_mem, club + ST_CLUB_DIVISION) == national)
                fprintf(stderr, "recomp: fra second: %.51s goes up to the National%s\n", name_of(club),
                        barred(club) ? " (a reserve side)" : "");
        }
        log_census("after");
    }
    free(held);
    free(zone_of);
    free(was_national);
    if (!rt_unwinding && old_left > 0) old_left--;
}
