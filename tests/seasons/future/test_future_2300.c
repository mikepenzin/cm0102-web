/* Future test, step 1 (games to 2300): every season function that reads no game memory, stepped from 2001 to 2300,
 * with the days round 28 February 2100, 2200 and 2300 (not leap years).
 *
 * Usage: future_2300_test KNOWN_FAULTS.tsv
 * Each check has a key. A check that fails prints "FAULT key: detail". The test passes when the failed keys are the
 * keys of the known-fault list: a new fault fails the test, and so does a known fault that no longer fails (remove
 * it from the list). The expected rules are those of tests/seasons/future/competitions.tsv and the after-table rule:
 * after its last table edition a competition continues on its last cycle. */
#include "season.h"
#include "season_intl.h"
#include "season_nl.h"
#include "season_uefa.h"
#include "season_uefa_days.h"
#include "season_cups.h"
#include "date.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define END_YEAR 2301           /* one year past 2300: no limit at 2300 (0.6.2 spec, year limits) */

/* ---- The season state of the test (season.c in the game) ---- */

static int code_on = 1, start_year = SEASON_FIRST_YEAR, changing = 1;
int season_code_on(void) { return code_on; }
int season_start_year(void) { return start_year; }
int season_changing_rules(void) { return changing; }
int season_change_is_creating(void) { return 0; }
int season_intl_confed_june(void) { return 1; }    /* original-faults group confed acts (original_faults.c) */
int g_left_held;                                       /* scrman_input.c: date.c clears it on a bad date */

static void game(int year, int rules)
{
    start_year = year;
    changing = rules;
}

/* ---- Faults ---- */

#define MAX_KEYS 512
static char failed[MAX_KEYS][96];
static int failed_count;

/* Records a failed check once per key; prints the first detail of each key. */
static void fault(const char *key, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void fault(const char *key, const char *fmt, ...)
{
    for (int i = 0; i < failed_count; i++)
        if (!strcmp(failed[i], key)) return;
    if (failed_count < MAX_KEYS) snprintf(failed[failed_count++], sizeof failed[0], "%s", key);
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "FAULT %s: ", key);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* ---- The civil calendar (H. Hinnant's algorithms), the reference of every date check ---- */

static long civil_days(int y, int m, int d)        /* m 1 to 12; days from 1 January 1970 */
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

static void civil_date(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

static int civil_weekday(long z) { return (int)(((z + 3) % 7 + 7) % 7); }      /* 0 Monday; 1 January 1970 Thu */
static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* ---- Dates: src/si/date.c (Date.cpp of the original), the nations league weekday, the FIFA day helpers ---- */

static void test_dates(void)
{
    for (long z = civil_days(2001, 1, 1); z <= civil_days(END_YEAR, 12, 31); z++) {
        int y, m, d;
        civil_date(z, &y, &m, &d);
        cm_date_t cd;
        date_init(&cd, (s16)d, (s8)(m - 1), (u16)y, -1);
        long doy = z - civil_days(y, 1, 1);
        if (cd.day != doy || (int)cd.leap != is_leap(y))
            fault("date.day_of_year", "%d-%02d-%02d: day %d leap %u, want %ld %d", y, m, d, cd.day, cd.leap, doy,
                  is_leap(y));
        if (date_weekday(&cd) != civil_weekday(z))
            fault("date.weekday", "%d-%02d-%02d: weekday %d, want %d", y, m, d, date_weekday(&cd), civil_weekday(z));
        cm_date_t next = date_add_days(&cd, 1);
        int ny, nm, nd;
        civil_date(z + 1, &ny, &nm, &nd);
        if (next.year != ny || next.day != z + 1 - civil_days(ny, 1, 1))
            fault("date.add_day", "%d-%02d-%02d + 1: %u day %d, want %d-%02d-%02d", y, m, d, next.year, next.day,
                  ny, nm, nd);
        cm_date_t back = date_sub_days(&next, 1);
        if (back.year != cd.year || back.day != cd.day)
            fault("date.sub_day", "%d-%02d-%02d: back %u day %d", y, m, d, back.year, back.day);
        if (season_nl_weekday(d, m, y) != civil_weekday(z))
            fault("nl.weekday", "%d-%02d-%02d: %d, want %d", y, m, d, season_nl_weekday(d, m, y), civil_weekday(z));
        if (season_day_of_year(y, m, d) != doy)
            fault("uefa_days.day_of_year", "%d-%02d-%02d: %d, want %ld", y, m, d, season_day_of_year(y, m, d), doy);
        int bm, bd;
        season_from_day_of_year(y, (int)doy, &bm, &bd);
        if (bm != m || bd != d)
            fault("uefa_days.from_day_of_year", "%d day %ld: %d-%d, want %d-%d", y, doy, bm, bd, m, d);
    }
    /* 28 February and 1 March of 2100, 2200 and 2300: no 29 February. */
    for (int y = 2100; y <= END_YEAR; y += 100) {
        cm_date_t feb28, mar1;
        date_init(&feb28, 28, 1, (u16)y, -1);
        date_init(&mar1, 1, 2, (u16)y, -1);
        cm_date_t after = date_add_days(&feb28, 1);
        if (after.day != mar1.day || after.year != mar1.year)
            fault("date.century", "%d: the day after 28 February is day %d, 1 March is day %d", y, after.day,
                  mar1.day);
        cm_date_t monday;
        date_init(&monday, 1, 2, (u16)y, DATE_MONDAY);
        long want = civil_days(y, 3, 1);
        int k = civil_weekday(want);
        want -= k > 3 ? k - 7 : k;
        if (monday.day != want - civil_days(y, 1, 1))
            fault("date.century_weekday", "%d: the Monday nearest 1 March is day %d, want %ld", y, monday.day,
                  want - civil_days(y, 1, 1));
        int sd = 28, sm = 1, sy = y;
        season_intl_shift_date(1, &sd, &sm, &sy);
        if (sd != 1 || sm != 2 || sy != y)
            fault("intl.shift_date_century", "%d: 28 February + 1 gives %d.%d.%d", y, sd, sm + 1, sy);
    }
}

/* ---- International finals (season_intl_rules.c, season_intl_table.c) ---- */

/* The original's step of each class (start_new_season), and the cycle after the table (the after-table rule: its last
 * cycle; the ACN on 2 years, its normal step; the OFC Nations Cup on 4 from 2028, decided). 0: ended. */
static const struct { uint32_t cls; const char *file; int step, cycle, end; } classes[] = {
    {INTL_WORLD_CUP, "world_cup", 4, 4, 0},
    {INTL_EURO, "euro_champ", 4, 4, 0},
    {INTL_CONFED, "fifa_confed", 2, 0, 2017},
    {INTL_COPA, "goldcup", 2, 4, 0},
    {INTL_ACN, "african_nations", 2, 2, 0},
    {INTL_ASIAN, "asia_nations", 4, 4, 0},
    {INTL_OFC, "oceania_nations", 2, 4, 0},
    {INTL_OLYMPICS, "olympics", 4, 4, 0},
};
#define CLASS_COUNT ((int)(sizeof classes / sizeof classes[0]))

#define MAX_EDITIONS 400

/* The expected editions of a class from a start season, with changing rules on. */
static int expected_on(int c, int start, int *out)
{
    int n = 0, last = 0;
    for (int i = 0; i < season_intl_edition_count; i++) {
        const SeasonIntlEdition *e = &season_intl_editions[i];
        if (strcmp(e->file, classes[c].file)) continue;
        if (e->season >= start && n < MAX_EDITIONS) out[n++] = e->year;
        last = e->year;
    }
    if (classes[c].end) return n;
    int from = n ? out[n - 1] : last;
    while (from + classes[c].cycle <= start) from += classes[c].cycle;
    for (int y = from + classes[c].cycle; y <= END_YEAR && n < MAX_EDITIONS; y += classes[c].cycle)
        if (!n || y > out[n - 1]) out[n++] = y;
    return n;
}

/* The editions that the code gives: the constructor's year, then start_new_season's, to END_YEAR. */
static int code_editions(int c, int first, int *out)
{
    int n = 0;
    uint32_t y = rt_intl_first_year(classes[c].cls, (uint32_t)first);
    while ((int)y <= END_YEAR && n < MAX_EDITIONS) {
        out[n++] = (int)y;
        uint32_t next = rt_intl_next_year(classes[c].cls, y, (uint32_t)classes[c].step);
        if (next <= y) break;               /* checked below as a repeat */
        y = next;
    }
    return n;
}

/* The original's first year of a class in a 2001/02 game: the table's first edition. */
static int first_2001(int c)
{
    const SeasonIntlEdition *e = season_intl_first(classes[c].file, SEASON_FIRST_YEAR);
    return e ? e->year : 2002;
}

/* The first day of an edition, as the template function dates it, moved by the table's shift. */
static void finals_day(int c, int year, int *d, int *m0, int *y)
{
    static const int first_day[][2] = {{12, 5}, {9, 5}, {29, 6}, {6, 6}, {8, 1}, {13, 9}, {10, 10}, {13, 8}};
    *d = first_day[c][0], *m0 = first_day[c][1], *y = year;
    if (classes[c].cls == INTL_OFC && year % 4 != 2) *d = 19, *m0 = 5;
    season_intl_shift_date(season_intl_date_shift(classes[c].cls, year), d, m0, y);
}

static void test_intl(void)
{
    static const int starts[] = {2001, 2002, 2005, 2010, 2016, 2018, 2021, 2024, 2025, 2026, 2030, 2050, 2099};
    for (int c = 0; c < CLASS_COUNT; c++) {
        for (size_t s = 0; s < sizeof starts / sizeof starts[0]; s++) {
            for (int rules = 0; rules <= 1; rules++) {
                game(starts[s], rules);
                if (!season_intl_on()) continue;
                int got[MAX_EDITIONS], want[MAX_EDITIONS], ng, nw;
                int first = starts[s] == SEASON_FIRST_YEAR ? first_2001(c) : starts[s];
                ng = code_editions(c, first, got);
                char key[96];
                if (rules) {
                    nw = expected_on(c, starts[s], want);
                    int k = 0;
                    while (k < ng && k < nw && got[k] == want[k]) k++;
                    if (k < ng || k < nw) {
                        snprintf(key, sizeof key, "intl.editions.%s", classes[c].file);
                        if (k < nw && k == ng)
                            fault(key, "start %d: stops after %d, want %d next (then every %d years)", starts[s],
                                  k ? got[k - 1] : 0, want[k], classes[c].cycle);
                        else if (k < nw && got[k] >= 2200)
                            fault(key, "start %d: stops after %d (the next year is %d), want %d next (then every %d "
                                  "years)", starts[s], k ? got[k - 1] : 0, got[k], want[k], classes[c].cycle);
                        else if (k < nw && got[k] > want[k])
                            fault(key, "start %d: skips %d (gives %d)", starts[s], want[k], got[k]);
                        else
                            fault(key, "start %d: edition %d is not expected (want %d)", starts[s], got[k],
                                  k < nw ? want[k] : 0);
                    }
                } else if (classes[c].cls != INTL_CONFED) {
                    /* Changing rules off: the rules of the start season stay, the original's step, no end. */
                    if (!ng || got[ng - 1] + classes[c].step <= END_YEAR) {
                        snprintf(key, sizeof key, "intl.editions_off.%s", classes[c].file);
                        fault(key, "start %d: the last edition is %d", starts[s], ng ? got[ng - 1] : 0);
                    }
                }
                for (int k = 1; k < ng; k++)
                    if (got[k] <= got[k - 1]) {
                        snprintf(key, sizeof key, "intl.twice.%s", classes[c].file);
                        fault(key, "start %d: %d after %d", starts[s], got[k], got[k - 1]);
                    }
                for (int k = 0; k < ng; k++) {
                    int d, m0, y;
                    finals_day(c, got[k], &d, &m0, &y);
                    if (y < got[k] - 1 || y > got[k] || (y == got[k] - 1 && m0 < 6)) {
                        snprintf(key, sizeof key, "intl.date.%s", classes[c].file);
                        fault(key, "start %d rules %d: the %d edition starts on %d.%d.%d", starts[s], rules, got[k],
                              d, m0 + 1, y);
                    }
                    uint32_t flags = rt_intl_round_flags(classes[c].cls, (uint32_t)got[k], 0x383);
                    if (rules && got[k] > 2004 && (flags & 0x80)) {
                        snprintf(key, sizeof key, "intl.golden_goal.%s", classes[c].file);
                        fault(key, "start %d rules %d: golden goal in %d", starts[s], rules, got[k]);
                    }
                }
            }
        }
    }
    /* The World Cup holder: slot 0 only to 2002, in every later year. */
    game(2010, 1);
    for (int y = 2006; y <= END_YEAR; y++)
        if (rt_intl_wc_holder_slot((uint32_t)y)) fault("intl.wc_holder_slot", "%d gives the holder a slot", y);
}

/* ---- UEFA (season_uefa_lookup.c, season_uefa_ko_lookup.c, season_uefa_days.c) ---- */

static int season_day(int season, int day, int month0, int year_off, long *z)
{
    *z = civil_days(season + year_off, month0 + 1, day);
    return *z >= civil_days(season, 7, 1) && *z <= civil_days(season + 1, 6, 30);
}

static void test_uefa(void)
{
    for (int start = 2001; start <= 2099; start += 7) {
        for (int rules = 0; rules <= 1; rules++) {
            game(start, rules);
            const SeasonUefa *last = NULL;
            for (int s = start; s <= END_YEAR; s++) {
                const SeasonUefa *r = season_uefa_row(s);
                if (!r && !(start == SEASON_FIRST_YEAR && !rules))
                    fault("uefa.row", "start %d rules %d: no row for %d", start, rules, s);
                if (rules && last && s > 2031 && r != last)
                    fault("uefa.row_after_table", "start %d: %d has another row than %d", start, s, s - 1);
                last = r;
            }
        }
    }
    /* The UEFA Super Cup venue (Q28): the row's venue to the last table row, then each year a venue of an earlier
     * row, never the venue of the year before. */
    for (int rules = 0; rules <= 1; rules++) {
        game(2013, rules);
        const char *prev = NULL;
        int last_season = season_uefa_rows[season_uefa_row_count - 1].season;
        for (int y = 2013; y <= END_YEAR; y++) {
            const char *stadium = NULL, *city = NULL;
            const SeasonUefa *r = season_uefa_row(y);
            if (!season_uefa_super_cup_venue(y, &stadium, &city) || !stadium || !city) {
                fault("uefa.super_cup_venue", "rules %d: no venue in %d", rules, y);
                continue;
            }
            if ((!rules || y <= last_season) && (strcmp(stadium, r->sc_stadium) || strcmp(city, r->sc_city)))
                fault("uefa.super_cup_venue", "rules %d: %d at %s, the row gives %s", rules, y, stadium, r->sc_stadium);
            if (rules && y > last_season) {
                int known = 0;
                for (int i = 0; i < season_uefa_row_count && !known; i++)
                    known = !strcmp(season_uefa_rows[i].sc_stadium, stadium) && !strcmp(season_uefa_rows[i].sc_city, city);
                if (!known) fault("uefa.super_cup_venue", "%d at %s, %s: no past final there", y, stadium, city);
                if (prev && !strcmp(prev, stadium))
                    fault("uefa.super_cup_venue_repeat", "%d at %s, as the year before", y, stadium);
            }
            prev = stadium;
        }
    }
    static const char comps[] = {'c', 'u', 'k'};
    for (size_t c = 0; c < sizeof comps; c++) {
        for (int s = 2028; s <= END_YEAR; s++) {
            long prev_leg = 0;
            for (int round = SEASON_UEFA_KO_PO; round <= SEASON_UEFA_KO_FINAL; round++) {
                const SeasonUefaKoDate *k = season_uefa_ko_date(comps[c], s, round);
                const SeasonUefaKoDate *k27 = season_uefa_ko_date(comps[c], 2027, round);
                if (!k27) continue;
                char key[64];
                snprintf(key, sizeof key, "uefa.ko_date.%c", comps[c]);
                if (!k) {
                    fault(key, "%d round %d: no date", s, round);
                    continue;
                }
                long draw, leg, leg27;
                int in = season_day(s, k->day, k->month, k->year_off, &leg);
                season_day(s, k->draw_day, k->draw_month, k->draw_year, &draw);
                season_day(2027, k27->day, k27->month, k27->year_off, &leg27);
                if (!in) fault(key, "%d round %d: first leg %d.%d.%d is outside the season", s, round, k->day,
                               k->month + 1, s + k->year_off);
                if (leg + k->gap > civil_days(s + 1, 5, 31))
                    fault(key, "%d round %d: last day %ld days after 31 May", s, round,
                          leg + k->gap - civil_days(s + 1, 5, 31));
                if (draw >= leg) fault(key, "%d round %d: the draw is not before the first leg", s, round);
                if (leg <= prev_leg) fault(key, "%d round %d: not after the round before", s, round);
                if (round != SEASON_UEFA_KO_FINAL && civil_weekday(leg) != civil_weekday(leg27))
                    fault(key, "%d round %d: weekday %d, 2027/28 has %d", s, round, civil_weekday(leg),
                          civil_weekday(leg27));
                prev_leg = leg;
            }
        }
        /* Every season to 2060, then every fourth and the seasons round each century: the FIFA day lookup costs
         * more each year after its table (3 ms a call at 2300), so every season to 2300 takes 25 seconds. */
        for (int s = 2024; s <= END_YEAR; s += s < 2060 || s % 100 >= 98 || s % 100 <= 1 ? 1 : 4) {
            int wd;
            const SeasonLeagueDay *days = season_uefa_league_days(comps[c] == 'u' ? 'e' : comps[c], s, &wd);
            int n = comps[c] == 'k' ? 6 : 8;
            long prev = 0;
            char key[64];
            snprintf(key, sizeof key, "uefa.league_days.%c", comps[c]);
            for (int i = 0; i < n; i++) {
                long z;
                if (!season_day(s, days[i].day, days[i].month, days[i].year_off, &z))
                    fault(key, "%d day %d: %d.%d.%d is outside the season", s, i, days[i].day, days[i].month + 1,
                          s + days[i].year_off);
                if (civil_weekday(z) != wd) fault(key, "%d day %d: weekday %d, want %d", s, i, civil_weekday(z), wd);
                if (i && z < prev + 7) fault(key, "%d day %d: less than a week after the day before", s, i);
                if (s > 2026 && season_fifa_meets(s + days[i].year_off, days[i].month + 1, days[i].day))
                    fault(key, "%d day %d: meets a FIFA day", s, i);
                prev = z;
            }
        }
    }
}

/* ---- Domestic cup formats (season_cups.c, season_cups_table.c) ---- */

/* Every cup of the table keeps its last format to 2300. The fall-back of the cup kind to the start year after
 * 2099 (season_kind_cups.c cup_season) reads game memory: the fast run checks it. */
static void test_cups(void)
{
    for (int i = 0; i < season_cup_row_count; i++) {
        const char *file = season_cup_rows[i].file;
        if (i && !strcmp(file, season_cup_rows[i - 1].file)) continue;
        const SeasonCupRow *last = NULL;
        for (int k = i; k < season_cup_row_count && !strcmp(season_cup_rows[k].file, file); k++)
            last = &season_cup_rows[k];
        for (int y = last->season; y <= END_YEAR; y++)
            if (season_cups_row(file, y) != last) {
                fault("cups.row_after_table", "%s %d: not the last row (%d)", file, y, last->season);
                break;
            }
    }
}

/* ---- Year limits (season_years.c, 0.6.2 slice 1) ---- */

/* From each start to END_YEAR: a competition takes the rules of its own year with changing rules on, the start
 * year's with them off, and the season hooks run in every season after the start with changing rules on. Faults 6,
 * 10 and 25 of 0.6.2 failed here at 2100. The season part and its news: ctest season_part. */
static void test_years(void)
{
    static const int starts[] = {2001, 2002, 2025, 2098, 2099, 2198, 2298, 2300};
    for (size_t s = 0; s < sizeof starts / sizeof starts[0]; s++) {
        for (int rules = 0; rules <= 1; rules++) {
            game(starts[s], rules);
            for (int y = starts[s]; y <= END_YEAR; y++) {
                int want = rules ? y : starts[s];
                if (season_rule_year(y) != want)
                    fault("years.rule_year", "start %d rules %d: the competition of %d takes the rules of %d, want %d",
                          starts[s], rules, y, season_rule_year(y), want);
                int due = y == starts[s] ? y != SEASON_FIRST_YEAR : rules;
                if (season_hooks_due(y) != due)
                    fault("years.hooks", "start %d rules %d: the season hooks of %d %s", starts[s], rules, y,
                          due ? "do not run" : "run");
            }
            if (season_hooks_due(starts[s] - 1)) fault("years.hooks", "start %d: hooks before the start", starts[s]);
            if (season_rule_year(0) != starts[s])
                fault("years.rule_year", "start %d: a year field 0 does not take the start year", starts[s]);
        }
    }
    /* The date jump switch: off by default (this test runs without RECOMP_DATE_JUMP). */
    if (season_date_jump() || season_last_start_year() != SEASON_NEW_GAME_LAST)
        fault("years.jump_default", "the latest start year is %d with the switch %d", season_last_start_year(),
              season_date_jump());
    if (SEASON_JUMP_LAST < END_YEAR - 1 || SEASON_YEAR_MAX < END_YEAR)
        fault("years.limits", "date jump to %d, years to %d", SEASON_JUMP_LAST, SEASON_YEAR_MAX);
}

/* ---- Known faults ---- */

int main(int argc, char **argv)
{
    unsetenv("RECOMP_DATE_JUMP");          /* the switch is checked off (test_years) */
    if (argc != 2) {
        fprintf(stderr, "usage: %s KNOWN_FAULTS.tsv\n", argv[0]);
        return 2;
    }
    if (!getenv("FUTURE_ONLY") || strstr(getenv("FUTURE_ONLY"), "dates")) test_dates();
    if (!getenv("FUTURE_ONLY") || strstr(getenv("FUTURE_ONLY"), "intl")) test_intl();
    if (!getenv("FUTURE_ONLY") || strstr(getenv("FUTURE_ONLY"), "uefa")) test_uefa();
    if (!getenv("FUTURE_ONLY") || strstr(getenv("FUTURE_ONLY"), "cups")) test_cups();
    if (!getenv("FUTURE_ONLY") || strstr(getenv("FUTURE_ONLY"), "years")) test_years();

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    char line[512], known[MAX_KEYS][96];
    int known_count = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\t\r\n")] = 0;
        if (known_count < MAX_KEYS) snprintf(known[known_count++], sizeof known[0], "%s", line);
    }
    fclose(f);

    int bad = 0;
    for (int i = 0; i < failed_count; i++) {
        int k = 0;
        while (k < known_count && strcmp(known[k], failed[i])) k++;
        if (k == known_count) {
            fprintf(stderr, "NEW FAULT %s (not in %s)\n", failed[i], argv[1]);
            bad++;
        }
    }
    for (int k = 0; k < known_count; k++) {
        int i = 0;
        while (i < failed_count && strcmp(known[k], failed[i])) i++;
        if (i == failed_count) {
            fprintf(stderr, "FIXED %s: it passes now; remove it from %s\n", known[k], argv[1]);
            bad++;
        }
    }
    printf("future_2300: %d checks failed, %d known, %d new or fixed\n", failed_count, known_count, bad);
    return bad ? 1 : 0;
}
