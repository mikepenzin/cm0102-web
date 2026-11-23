/* Test of the editions of the international finals (0.6.0 slice 27, src/recomp_rt/win/season_intl_rules.c and
 * the table season_intl_table.c that tools/seasons/make_intl.py makes).
 *
 * Usage: season_intl_test
 * The test gives the season state itself (start year, changing rules, season code). The checks: the base game
 * and RECOMP_SEASON_CODE=off keep the original's years and flags; the first edition of a later start season;
 * the next edition with changing rules on and the original's step with it off; the Confederations Cup waits
 * after 2017; the Tokyo Olympics in 2020 (Q366); the golden goal to the 2003 Confederations Cup and none at
 * Euro 2004 (Q367); the hosts of some editions. */
#include "season.h"
#include "season_intl.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <strings.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* The season state of the test (season.c in the game). */
static int code_on = 1, start_year = SEASON_FIRST_YEAR, changing = 0;
int season_code_on(void) { return code_on; }
int season_start_year(void) { return start_year; }
int season_changing_rules(void) { return changing; }
static int creating;
int season_change_is_creating(void) { return creating; }
/* The fix of original-faults group confed (original_faults.c): on unless a test turns it off. */
static int confed_june = 1;
int season_intl_confed_june(void) { return confed_june; }

static void game(int on, int year, int rules)
{
    code_on = on;
    start_year = year;
    changing = rules;
}

/* The nation names of a database for season_intl_later_held (a list that ends with NULL). */
static int held_in(const char *name, void *ctx)
{
    for (const char *const *n = ctx; *n; n++)
        if (!strcasecmp(*n, name)) return 1;
    return 0;
}

/* Day of the year (0 is 1 January) of day d of month m (1 to 12). */
static int doy_of(int y, int m, int d)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return before[m - 1] + (m > 2 && leap) + d - 1;
}

static long days_from_civil(int y, int m, int d)    /* days from 1 January 1970 */
{
    long n = 0;
    for (int k = 1970; k < y; k++) n += 365 + ((k % 4 == 0 && k % 100 != 0) || k % 400 == 0);
    for (int k = y; k < 1970; k++) n -= 365 + ((k % 4 == 0 && k % 100 != 0) || k % 400 == 0);
    return n + doy_of(y, m, d);
}

/* A template date (month from 0) moved by shift days, then to its weekday code (0 Monday) as Date::Date does
 * (0x54a2d0: the nearest day of the code, up to 3 days either way; season_weekday_date). */
static long snapped(int y, int day, int month0, int shift, int code)
{
    long t = days_from_civil(y, month0 + 1, day) + shift;
    int k = (int)((((t + 3) % 7 + 7) % 7 - code + 7) % 7);     /* 1 January 1970: a Thursday */
    return t - (k > 3 ? k - 7 : k);
}

static int rename_row(const char *old)
{
    for (int i = 0; i < season_intl_rename_count; i++)
        if (!strcmp(season_intl_renames[i].old, old)) return i;
    return -1;
}

static int no_colombia(const char *file, const char *name) { (void)file; return strcmp(name, "Colombia"); }
static int none_alive(const char *file, const char *name) { (void)file; (void)name; return 0; }

int main(void)
{
    /* The base game and the season code off: the original's values. */
    game(1, 2001, 0);
    CHECK(!season_intl_on(), "the base game is not a season game");
    CHECK(rt_intl_first_year(INTL_COPA, 2001) == 2001, "base game: Copa America year");
    CHECK(rt_intl_next_year(INTL_CONFED, 2017, 2) == 2019, "base game: Confederations Cup step");
    CHECK(rt_intl_round_flags(INTL_EURO, 2008, 0x383) == 0x383, "base game: golden goal stays");
    CHECK(rt_intl_wc_holder(0x1234) == 0x1234, "base game: the holder is exempt");
    CHECK(rt_intl_wc_holder_slot(2006), "base game: the holder has slot 0");
    game(0, 2010, 1);
    CHECK(!season_intl_on(), "season code off");
    CHECK(rt_intl_first_year(INTL_ACN, 2012) == 2012, "season code off: ACN year");
    CHECK(rt_intl_next_year(INTL_ACN, 2012, 2) == 2014, "season code off: ACN step");

    /* A 2001 start with changing rules on: the first editions are the original's, the steps the table's. */
    game(1, 2001, 1);
    CHECK(season_intl_on(), "2001 with changing rules on is a season game");
    CHECK(rt_intl_first_year(INTL_EURO, 2004) == 2004, "2001 on: Euro first edition stays");
    CHECK(rt_intl_next_year(INTL_CONFED, 2003, 2) == 2005, "2001 on: Confederations Cup 2005");
    CHECK(rt_intl_next_year(INTL_CONFED, 2005, 2) == 2009, "2001 on: Confederations Cup 2009");
    CHECK(rt_intl_next_year(INTL_CONFED, 2017, 2) == 2999, "2001 on: no Confederations Cup after 2017 (never year)");
    CHECK(rt_intl_next_year(INTL_CONFED, 2999, 2) == 2999, "2001 on: the ended object stays");
    CHECK(rt_intl_next_year(INTL_CONFED, 2200, 2) == 2999, "2001 on: an object of a 0.6.1 save at 2200 ends");
    CHECK(rt_intl_next_year(INTL_COPA, 2001, 2) == 2004, "2001 on: Copa America 2004");
    CHECK(rt_intl_next_year(INTL_COPA, 2015, 2) == 2016, "2001 on: Copa America Centenario");
    CHECK(rt_intl_next_year(INTL_ASIAN, 2004, 4) == 2007, "2001 on: Asian Cup 2007");
    CHECK(rt_intl_next_year(INTL_ACN, 2012, 2) == 2013, "2001 on: ACN moves to odd years");
    CHECK(rt_intl_next_year(INTL_ACN, 2023, 2) == 2026, "2001 on: ACN 2025 plays in January 2026");
    CHECK(rt_intl_next_year(INTL_OLYMPICS, 2016, 4) == 2020, "2001 on: Olympics 2020 (Q366)");
    CHECK(rt_intl_next_year(INTL_WORLD_CUP, 2018, 4) == 2022, "2001 on: World Cup 2022");
    CHECK(rt_intl_next_year(INTL_OFC, 2016, 2) == 2024, "2001 on: no OFC Nations Cup in 2020");
    CHECK(rt_intl_round_flags(INTL_WORLD_CUP, 2002, 0x383) == 0x383, "2001 on: golden goal in 2002");
    CHECK(rt_intl_round_flags(INTL_CONFED, 2003, 0x83) == 0x83, "2001 on: golden goal in the 2003 Confed Cup");
    CHECK(rt_intl_round_flags(INTL_EURO, 2008, 0x383) == 0x303, "2001 on: no golden goal at Euro 2008");
    CHECK(rt_intl_round_flags(INTL_CONFED, 2005, 0x83) == 0x3, "2001 on: no golden goal in 2005");
    CHECK(rt_intl_round_flags(INTL_WORLD_CUP, 2002, 0x3) == 0x3, "2001 on: a round without golden goal");

    CHECK(rt_intl_wc_holder(0x1234) == 0, "2001 on: no holder exemption in the sections");
    CHECK(rt_intl_wc_holder_slot(2002), "2001 on: the holder has slot 0 in 2002");
    CHECK(!rt_intl_wc_holder_slot(2006), "2001 on: no holder slot in 2006");

    /* A later start: the first edition from the start season. */
    game(1, 2005, 0);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2006) == 2006, "2005: World Cup 2006");
    CHECK(rt_intl_first_year(INTL_CONFED, 2005) == 2009, "2005: the 2005 Confederations Cup lies before");
    CHECK(rt_intl_first_year(INTL_ASIAN, 2008) == 2007, "2005: Asian Cup 2007");
    CHECK(rt_intl_first_year(INTL_COPA, 2005) == 2007, "2005: Copa America 2007");
    CHECK(rt_intl_first_year(INTL_ACN, 2006) == 2006, "2005: ACN 2006");
    CHECK(rt_intl_next_year(INTL_COPA, 2007, 2) == 2009, "2005 off: the original's step");
    CHECK(rt_intl_round_flags(INTL_WORLD_CUP, 2006, 0x383) == 0x303, "2005 off: no golden goal");
    game(1, 2002, 0);
    CHECK(rt_intl_round_flags(INTL_EURO, 2008, 0x383) == 0x383, "2002 off: the golden goal of 2002/03 stays");
    game(1, 2016, 1);
    CHECK(rt_intl_first_year(INTL_EURO, 2016) == 2020, "2016: Euro 2016 lies before the start");
    CHECK(rt_intl_first_year(INTL_COPA, 2017) == 2019, "2016: Copa America 2019");
    CHECK(rt_intl_first_year(INTL_ACN, 2016) == 2017, "2016: ACN 2017");
    CHECK(rt_intl_first_year(INTL_OLYMPICS, 2016) == 2016, "2016: Olympics in August 2016");
    game(1, 2018, 1);
    CHECK(rt_intl_first_year(INTL_CONFED, 2019) == 2999, "2018: no Confederations Cup (never year)");
    game(1, 2026, 1);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2026) == 2030, "2026: the 2030 World Cup (slice 29, Q560)");
    game(1, 2030, 1);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2030) == 2034, "2030: the World Cup continues every 4 years");

    /* After the table (0.6.2 decision A, open items 1, 13, 15, 32 and 34): each class continues on its last
     * cycle with the last edition's format; the ended Confederations Cup waits for the never year 2999. */
    game(1, 2001, 1);
    CHECK(rt_intl_next_year(INTL_WORLD_CUP, 2030, 4) == 2034, "after the table: World Cup 2034");
    CHECK(rt_intl_next_year(INTL_EURO, 2028, 4) == 2032, "after the table: Euro 2032");
    CHECK(rt_intl_next_year(INTL_COPA, 2024, 2) == 2028, "after the table: Copa America 2028");
    CHECK(rt_intl_next_year(INTL_ACN, 2027, 2) == 2029, "after the table: ACN 2029, every 2 years");
    CHECK(rt_intl_next_year(INTL_ASIAN, 2027, 4) == 2031, "after the table: Asian Cup 2031");
    CHECK(rt_intl_next_year(INTL_OFC, 2024, 2) == 2028, "after the table: OFC Nations Cup 2028 (Max)");
    CHECK(rt_intl_next_year(INTL_OLYMPICS, 2024, 4) == 2028, "after the table: Olympics 2028");
    CHECK(rt_intl_next_year(INTL_WORLD_CUP, 2298, 4) == 2302, "after the table: World Cup 2302");
    CHECK(rt_intl_next_year(INTL_ACN, 2299, 2) == 2301, "after the table: ACN 2301");
    /* A 0.6.0 or 0.6.1 object that waits at 2200 goes to the next edition after it (slice 4b repairs the dates). */
    CHECK(rt_intl_next_year(INTL_WORLD_CUP, 2200, 4) == 2202, "an object at 2200: World Cup 2202");
    const SeasonIntlEdition *a = season_intl_edition("world_cup", 2034);
    CHECK(a && a->teams == 48 && a->season == 2033 && a->start_year == 2034 && a->month == 6 && a->day == 8 &&
              !a->golden && !strcmp(a->hosts, "Saudi Arabia"), "World Cup 2034: 48 teams, season 2033/34, 8 June, "
                                                               "Saudi Arabia (Q5)");
    /* 0.6.2 slice 5: the hosts after the table. */
    CHECK(!strcmp(season_intl_edition("euro_champ", 2032)->hosts, "Italy;Turkey/T\xfcrkiye"), "Euro 2032: Italy, Turkey");
    CHECK(!strcmp(season_intl_edition("olympics", 2028)->hosts, "United States/USA"), "Olympics 2028: United States");
    CHECK(!strcmp(season_intl_edition("olympics", 2032)->hosts, "Australia"), "Olympics 2032: Australia");
    CHECK(!*season_intl_edition("world_cup", 2038)->hosts, "World Cup 2038: the host row gives the host (Q4)");
    CHECK(season_intl_host_slot("world_cup", 2038) == 2 && season_intl_host_slot("world_cup", 2006) == 2,
          "World Cup 2038 takes the original's 2006 row (moved on 32 years)");
    CHECK(season_intl_host_slot("african_nations", 2029) == 6 && season_intl_host_slot("african_nations", 2045) == 6,
          "ACN 2029 and 2045 take the row of 2012");
    CHECK(season_intl_host_slot("goldcup", 2028) == -1, "the Copa America rotates (Q32)");
    CHECK(!strcmp(season_intl_edition("goldcup", 2028)->hosts, "Colombia") &&
              !strcmp(season_intl_edition("goldcup", 2032)->hosts, "Peru"),
          "Copa America 2028 and 2032: the table's hosts in order (Q32)");
    CHECK(!strcmp(season_intl_edition("oceania_nations", 2028)->hosts, "New Zealand"), "OFC 2028: New Zealand");
    CHECK(!strcmp(season_intl_rotation_host("asia_nations", 1, NULL), "Indonesia"), "Asian Cup rotation: each co-host");
    CHECK(!strcmp(season_intl_rotation_host("goldcup", 0, no_colombia), "Peru"), "a host that no longer exists is left out");
    CHECK(!season_intl_rotation_host("goldcup", 0, none_alive), "no live host: none");
    CHECK(season_intl_rotation_step("goldcup", 2028) == 0 && season_intl_rotation_step("goldcup", 2036) == 2,
          "rotation steps count the editions after the table");
    {
        int all[3] = {1, 1, 1}, two[3] = {0, 1, 1}, one[3] = {0, 0, 1}, none[3] = {0, 0, 0};
        CHECK(season_intl_pick_pair(all, 1, 0) == 1, "Q6: a valid drawn pair stays");
        CHECK(season_intl_pick_pair(two, 0, 0) == 1 && season_intl_pick_pair(two, 0, 1) == 2,
              "Q6: a pair with a nation that no longer exists is drawn again from the others");
        CHECK(season_intl_pick_pair(one, 0, 5) == 2, "Q6: the one valid pair");
        CHECK(season_intl_pick_pair(none, 1, 0) == -1, "Q6: no valid pair, the host rotates (Q32)");
    }
    CHECK(!season_intl_table_edition("world_cup", 2034), "World Cup 2034 is not a table row");
    CHECK(season_intl_table_edition("world_cup", 2030) == season_intl_edition("world_cup", 2030),
          "a table edition is the table row");
    CHECK(!season_intl_edition("world_cup", 2032), "no World Cup in 2032");
    CHECK(!season_intl_edition("fifa_confed", 2021), "no Confederations Cup after 2017");
    a = season_intl_edition("african_nations", 2029);
    CHECK(a && a->teams == 24 && a->season == 2028 && a->start_year == 2029, "ACN 2029: 24 teams, June 2029");
    a = season_intl_edition("asia_nations", 2031);
    CHECK(a && a->teams == 24 && a->season == 2030 && a->month == 1, "Asian Cup 2031: 24 teams, January");
    a = season_intl_edition("goldcup", 2032);
    CHECK(a && a->teams == 16, "Copa America 2032: 16 teams, as 2024");
    a = season_intl_edition("euro_champ", 2300);
    CHECK(a && a->teams == 24 && a->season == 2299, "Euro 2300: 24 teams");
    CHECK(!season_intl_edition("euro_champ", 32768), "no edition past the 16-bit year");
    game(1, 2098, 1);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2098) == 2102, "2098: the 2098 World Cup lies before, 2102");
    CHECK(rt_intl_first_year(INTL_EURO, 2098) == 2100, "2098: Euro 2100");
    CHECK(rt_intl_first_year(INTL_ACN, 2098) == 2099, "2098: ACN 2099 (season 2098/99)");
    CHECK(rt_intl_first_year(INTL_CONFED, 2098) == 2999, "2098: no Confederations Cup");
    a = season_intl_rule_edition("world_cup", 2102);
    CHECK(a && a->teams == 48, "2098 on: World Cup 2102 with 48 teams");
    game(1, 2298, 0);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2298) == 2302, "2298: World Cup 2302, none in the past");
    CHECK(rt_intl_first_year(INTL_OLYMPICS, 2298) == 2300, "2298: Olympics 2300");
    a = season_intl_rule_edition("world_cup", 2302);
    CHECK(a && a->teams == 48, "2298 off: the start season's 48 teams");

    /* No class waits after its table (the idle year 2200 of 0.6.0 and 0.6.1): from every start with changing
     * rules on, each next edition comes on the class's cycle, and only the Confederations Cup ends. With changing
     * rules off the first edition of a start after the table is on the class's cycle too (then the original's
     * step, checked above). */
    {
        static const int cycle[INTL_CLASSES] = {4, 4, 0, 4, 2, 4, 4, 4};
        int waits = 0;
        for (int start = 2001; start <= 2301; start += 7)
            for (int rules = 0; rules <= 1; rules++) {
                game(1, start, rules);
                for (uint32_t c = 0; c < INTL_CLASSES; c++) {
                    uint32_t y = rt_intl_first_year(c, (uint32_t)start);
                    if (!rules) {               /* a 2001 start keeps the original's year (start_edition_2001) */
                        const char *f = season_intl_class_file(c);
                        if (start != 2001 && (c == INTL_CONFED ? start > 2017 && y != 2999
                                                               : !season_intl_edition(f, (int)y)))
                            waits++;
                        continue;
                    }
                    for (int k = 0; k < 120 && y < 2400u; k++) {
                        uint32_t n = rt_intl_next_year(c, y, cycle[c] ? (uint32_t)cycle[c] : 2u);
                        if (c == INTL_CONFED) {
                            if (rules && y > 2017 && y != 2999) waits++;     /* off: every 2 years (Q232) */
                        } else if (n <= y || (y > 2030 && rules && n - y != (uint32_t)cycle[c])) {
                            waits++;
                        }
                        if (n <= y) break;
                        y = n;
                    }
                }
            }
        CHECK(waits == 0, "no class waits after its table, only the Confederations Cup ends");
    }

    /* Open item 96: after its table an edition takes the first day of the table's last edition in another year, so
     * the day falls on any weekday, and Date::Date then moves each template date up to 3 days to its weekday code
     * (0x54a2d0). The template codes of neighbouring rounds differ by a day in places (the knockout of 48 at 8 days
     * from the original's), so in some years one date moved 3 days forward and the next 3 days back: the World Cup
     * of 2066 met its groups with its round of 32, and the game moved matches a week at a time to 4 August. The shift
     * of such an edition keeps the first group day on the template's weekday code (0 Monday to 6 Sunday: World Cup
     * 4 at 0x92c335, Euro 6 at 0x57ac35, Copa America 3 at 0x5e09a5, ACN 6 at 0x401385, Asian Cup 4 at 0x40eba5, OFC
     * 4 in November at 0x798c23 and 0 in June at 0x798f7e, Confederations Cup 5 at 0x58b601, Olympics 2 at
     * 0x79f875). */
    {
        static const struct { uint32_t cls; int day, month0, code; } first[] = {
            {INTL_WORLD_CUP, 12, 5, 4}, {INTL_EURO, 9, 5, 6}, {INTL_COPA, 6, 6, 3}, {INTL_ACN, 8, 1, 6},
            {INTL_ASIAN, 13, 9, 4}, {INTL_OLYMPICS, 13, 8, 2}, {INTL_OFC, 10, 10, 4}, {INTL_OFC, 19, 5, 0},
            {INTL_CONFED, 29, 6, 5},
        };
        int off = 0, n = 0;
        for (int rules = 0; rules <= 1; rules++) {
            game(1, 2030, rules);
            for (size_t c = 0; c < sizeof first / sizeof first[0]; c++)
                for (int y = 2031; y <= 2301; y++) {
                    if (!season_intl_edition(season_intl_class_file(first[c].cls), y)) continue;
                    if (first[c].cls == INTL_OFC && (y % 4 == 2) != (first[c].month0 == 10)) continue;
                    int d = first[c].day, m0 = first[c].month0, yy = y;
                    season_intl_shift_date(season_intl_date_shift(first[c].cls, y), &d, &m0, &yy);
                    struct tm t = {0};
                    t.tm_year = yy - 1900, t.tm_mon = m0, t.tm_mday = d, t.tm_hour = 12;
                    time_t when = timegm(&t);
                    int wd = (gmtime(&when)->tm_wday + 6) % 7;      /* 0 Monday */
                    n++;
                    off += wd != first[c].code;
                }
        }
        char text[120];
        snprintf(text, sizeof text, "after the table: the first group day on its weekday code (%d of %d off)", off, n);
        CHECK(n > 300 && off == 0, "%s", text);

        /* The fault itself: the World Cup of 48 with each date moved to its code as Date::Date does (0x54a2d0, as
         * season_weekday_date). The first and last days of each knockout round (day, month from 0, code:
         * season_wc48.c knockout_template) must keep their order, each round's first day before its last. With 8
         * June on a Tuesday (2038, 2066, 2094) the round of 32 got 26 June as its first and its last day, and the
         * game moved its 16 matches a week at a time. Groups I to L end 2 days after the third matchday of A to D
         * (24 June, code 2). */
        static const int ko[6][2][3] = {
            {{27, 5, 5}, {3, 6, 5}}, {{4, 6, 4}, {8, 6, 1}}, {{9, 6, 2}, {12, 6, 5}},
            {{13, 6, 6}, {16, 6, 2}}, {{17, 6, 3}, {18, 6, 4}}, {{18, 6, 4}, {19, 6, 5}},
        };
        int bad = 0, worst = 0, bad_year = 0;
        for (int rules = 0; rules <= 1; rules++) {
            game(1, 2030, rules);
            for (int y = 2031; y <= 2301; y++) {
                if (!season_intl_edition(season_intl_class_file(INTL_WORLD_CUP), y)) continue;
                int s = season_intl_date_shift(INTL_WORLD_CUP, y), wrong = 0;
                long before = snapped(y, 24, 5, s, 2) + 2;
                for (int r = 0; r < 6; r++) {
                    long a = snapped(y, ko[r][0][0], ko[r][0][1], s, ko[r][0][2]);
                    long b = snapped(y, ko[r][1][0], ko[r][1][1], s, ko[r][1][2]);
                    wrong += a >= b || a <= before;
                    before = a;     /* the template's round of 32 ends after the round of 16 begins, in the table too */
                    int july = (int)(b - days_from_civil(y, 7, 1) + 1);
                    if (r == 5 && july > worst) worst = july;
                }
                if (wrong && !bad_year) bad_year = y;
                bad += wrong != 0;
            }
        }
        snprintf(text, sizeof text, "after the table: World Cup rounds in order (%d bad, the first %d), final by "
                 "21 July (latest %d July)", bad, bad_year, worst);
        CHECK(bad == 0 && worst <= 21, "%s", text);
    }

    /* Finals dates. */
    game(1, 2001, 0);
    CHECK(season_intl_date_shift(INTL_ACN, 2019) == 0, "base game: no date shift");
    /* 0.6.2 open items 157 to 159: the original's Confederations Cup plays from 29 July (0x58b601) to 9 August
     * (0x58b984) of every odd year, in the European club qualifiers and its own Club World Championship. The real
     * editions played in June (2003: 18 to 29 June). With the fix of group confed every edition of the base game
     * starts on the Saturday on or before 18 June, by a whole shift that Date::Date's weekday move keeps, and its
     * last match (9 August, code 2: 0x58b984) is in June too. */
    {
        int bad = 0, bad_year = 0;
        for (int y = 2003; y <= 2301; y += 2) {
            int s = season_intl_date_shift(INTL_CONFED, y);
            long first = snapped(y, 29, 6, s, 5), last = snapped(y, 9, 7, s, 2);
            if (first < days_from_civil(y, 6, 12) || first > days_from_civil(y, 6, 18) ||
                first != days_from_civil(y, 7, 29) + s || last > days_from_civil(y, 6, 30)) {
                if (!bad++) bad_year = y;
            }
        }
        CHECK(bad == 0, "base game: %d Confederations Cups not from 12 to 18 June to the end of June (first %d)", bad,
              bad_year);
        confed_june = 0;
        CHECK(season_intl_date_shift(INTL_CONFED, 2011) == 0, "base game, keep:confed: the original's dates");
        confed_june = 1;
        CHECK(season_intl_date_shift(INTL_ACN, 2011) == 0, "base game: the fix moves only the Confederations Cup");
        game(0, 2010, 1);       /* an original-faults fix acts with the season code off too */
        CHECK(snapped(2011, 29, 6, season_intl_date_shift(INTL_CONFED, 2011), 5) <= days_from_civil(2011, 6, 21),
              "season code off: the Confederations Cup 2011 in June");
        game(1, 2001, 0);
    }
    game(1, 2010, 1);
    CHECK(season_intl_date_shift(INTL_ACN, 2019) == 133, "ACN 2019: 8 February to 21 June");
    CHECK(season_intl_date_shift(INTL_ACN, 2026) == -49, "ACN 2025/26: 8 February 2026 to 21 December 2025");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2022) == 161, "World Cup 2022: 12 June to 20 November");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2010) == -1, "World Cup 2010: 12 June to 11 June");
    CHECK(season_intl_date_shift(INTL_ASIAN, 2011) == -279, "Asian Cup 2011: 13 October to 7 January");
    CHECK(season_intl_date_shift(INTL_OFC, 2012) == -18, "OFC 2012: 19 June (2012 %% 4 is 0) to 1 June");
    CHECK(season_intl_date_shift(INTL_OFC, 2008) == 0, "OFC 2008: no date in the table");
    CHECK(season_intl_date_shift(INTL_COPA, 2009) == 0, "Copa America 2009: not an edition of the table");
    game(1, 2001, 1);
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2002) == 0, "2001 on: the 2002 World Cup keeps its dates");
    CHECK(season_intl_date_shift(INTL_ACN, 2002) == 0, "2001 on: the ACN 2002 keeps its dates");
    CHECK(season_intl_date_shift(INTL_ACN, 2004) != 0, "2001 on: the ACN 2004 moves to the table's dates");
    /* The Confederations Cup 2003 of a 2001 game plays as in the base game: from 14 June (group confed). */
    CHECK(snapped(2003, 29, 6, season_intl_date_shift(INTL_CONFED, 2003), 5) == days_from_civil(2003, 6, 14),
          "2001 on: the Confederations Cup 2003 from Saturday 14 June, as the base game");
    confed_june = 0;
    CHECK(season_intl_date_shift(INTL_CONFED, 2003) == 0, "2001 on, keep:confed: the Confederations Cup 2003 keeps "
                                                          "its dates");
    confed_june = 1;
    CHECK(rt_intl_round_flags(INTL_EURO, 2004, 0x383) == 0x383, "2001 on: Euro 2004, built at the new game, keeps its flags");
    creating = 1;
    CHECK(rt_intl_wc_holder(0x1234) == 0x1234, "2001 on: the sections made at the new game keep the holder");
    creating = 0;
    game(1, 2010, 0);
    CHECK(season_intl_rule_edition("euro_champ", 2016)->teams == 16, "2010 off: Euro 2016 keeps 16 teams (Q232)");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2022) == -2,
          "2010 off: World Cup 2022 in June, as 2014 (the first edition from 2010), on Friday 10 June (item 96)");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2014) == 0, "2010 off: World Cup 2014 on its table day");
    game(1, 2015, 0);
    CHECK(season_intl_rule_edition("euro_champ", 2020)->teams == 24, "2015 off: Euro 2020 with 24 teams");
    game(1, 2010, 1);
    CHECK(season_intl_rule_edition("euro_champ", 2016)->teams == 24, "2010 on: Euro 2016 with 24 teams");
    int d = 8, m = 1, y = 2026;
    season_intl_shift_date(-49, &d, &m, &y);
    CHECK(d == 21 && m == 11 && y == 2025, "shift into the year before: %d.%d.%d", d, m + 1, y);
    d = 28, m = 1, y = 2020;
    season_intl_shift_date(1, &d, &m, &y);
    CHECK(d == 29 && m == 1 && y == 2020, "leap day");

    /* The table. */
    const SeasonIntlEdition *e = season_intl_edition("olympics", 2020);
    CHECK(e && e->season == 2020 && e->month == 7 && !strcmp(e->hosts, "Japan"), "Tokyo Olympics in 2020/21");
    CHECK(!season_intl_edition("olympics", 2021), "no Olympics in 2021");
    e = season_intl_edition("euro_champ", 2008);
    CHECK(e && !strcmp(e->hosts, "Austria;Switzerland") && e->teams == 16, "Euro 2008 hosts");
    e = season_intl_edition("euro_champ", 2020);
    CHECK(e && !*e->hosts && e->teams == 24, "Euro 2020: no host places, 24 teams");
    e = season_intl_edition("world_cup", 2010);
    CHECK(e && !strcmp(e->hosts, "South Africa") && e->month == 6 && e->day == 11, "World Cup 2010");
    e = season_intl_edition("asia_nations", 2004);
    CHECK(e && !strcmp(e->hosts, "China PR/China"), "Asian Cup 2004 host names");
    for (int i = 1; i < season_intl_edition_count; i++) {
        const SeasonIntlEdition *a = &season_intl_editions[i - 1], *b = &season_intl_editions[i];
        int c = strcmp(a->file, b->file);
        CHECK(c < 0 || (c == 0 && a->year < b->year), "table order at %s", b->row);
    }

    /* Nation records (the names of the databases' nation.dat, O188). October 2022: a dead Yugoslavia (continent
     * Europe) beside Serbia, dead FYR of Macedonia and Swaziland beside North Macedonia and eSwatini. */
    static const char *const oct2022[] = {"Yugoslavia", "Serbia", "Montenegro", "FYR of Macedonia",
                                          "North Macedonia", "Swaziland", "eSwatini", "Czech Republic", "Turkey",
                                          "Kosovo", NULL};
    CHECK(season_intl_later_held(rename_row("Yugoslavia"), held_in, (void *)oct2022), "October 2022: Yugoslavia dead");
    CHECK(season_intl_later_held(rename_row("FYR of Macedonia"), held_in, (void *)oct2022), "October 2022: FYR dead");
    CHECK(season_intl_later_held(rename_row("Swaziland"), held_in, (void *)oct2022),
          "October 2022: Swaziland dead beside eSwatini (case)");
    CHECK(!season_intl_later_held(rename_row("Czech Republic"), held_in, (void *)oct2022), "Czech Republic is live");
    CHECK(!season_intl_later_held(rename_row("Turkey"), held_in, (void *)oct2022), "Turkey is live");
    /* March 2014: Yugoslavia is the Serbian side beside a new Montenegro; it is renamed, not dead. */
    static const char *const mar2014[] = {"Yugoslavia", "Montenegro", "FYR of Macedonia", "Swaziland", NULL};
    CHECK(!season_intl_later_held(rename_row("Yugoslavia"), held_in, (void *)mar2014), "March 2014: Yugoslavia live");
    CHECK(!season_intl_later_held(rename_row("Serbia and Montenegro"), held_in, (void *)mar2014), "no S&M chain");
    /* September 2011 ODB: a dead Yugoslavia beside Serbia, two steps down the chain. */
    static const char *const sep2011[] = {"Yugoslavia", "Serbia", "Montenegro", NULL};
    CHECK(season_intl_later_held(rename_row("Yugoslavia"), held_in, (void *)sep2011), "September 2011: dead");
    CHECK(!season_intl_later_held(-1, held_in, (void *)sep2011), "no row");
    /* Kosovo: UEFA from 2016/17 (no continent in the April 2015, October 2015 and April 2016 databases). */
    CHECK(season_intl_uefa_from("Kosovo") == 2016, "Kosovo joins UEFA in 2016");
    /* Martinique: CONCACAF in every season of the game, though the databases from October 2016 give no continent. */
    CHECK(season_intl_concacaf_from("Martinique") == 1991 && season_intl_concacaf_from("martinique") == 1991,
          "Martinique plays in CONCACAF");
    CHECK(season_intl_concacaf_from("Guadeloupe") <= 2001 && season_intl_concacaf_from("French Guiana") <= 2001,
          "Guadeloupe and French Guiana: CONCACAF in every season");
    CHECK(season_intl_concacaf_from("Saint-Martin") == 2002 && season_intl_concacaf_from("Sint Maarten") == 2002,
          "Saint-Martin and Sint Maarten from 2002/03");
    CHECK(season_intl_concacaf_from("Bonaire") == 2013, "Bonaire from 2013/14");
    CHECK(!season_intl_concacaf_from("Suriname") && !season_intl_concacaf_from("Netherlands Antilles") &&
          !season_intl_concacaf_from("Curaçao") && !season_intl_concacaf_from(NULL),
          "no other nation (Suriname and Curacao are FIFA members, the Netherlands Antilles were dissolved)");
    /* Item 200: Timor (the 3.9.68 name, no continent) plays in the AFC from 2005/06. */
    CHECK(season_intl_afc_from("Timor") == 2005 && season_intl_afc_from("Timor-Leste") == 2005,
          "Timor joins the AFC in 2005");
    CHECK(!season_intl_afc_from("Australia") && !season_intl_afc_from("Brunei Darussalam") &&
          !season_intl_afc_from(NULL), "no other nation");
    CHECK(!season_intl_uefa_from("Gibraltar") && !season_intl_uefa_from("Serbia") && !season_intl_uefa_from(NULL),
          "no other nation");

    /* The days of the finals that a qualifier of a confederation does not take (0.6.2 slice 6, faults 2 and 19:
     * the CAF qualifiers of 26 January 2013 met the ACN of 19 January to 10 February 2013 in long game seed 6). */
    game(1, 2001, 0);
    CHECK(!season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 1, 26)), "base game: no finals days");
    game(1, 2001, 1);
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 1, 26)), "2001 on: ACN 2013 on 26 January");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 2, 6)), "2001 on: ACN 2013 on 6 February");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 1, 17)), "2001 on: two days before the ACN");
    CHECK(!season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 1, 16)), "2001 on: three days before");
    CHECK(!season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 3, 22)), "2001 on: March 2013 is free");
    CHECK(!season_intl_finals_busy(SEASON_CONF_ASIA, 2013, doy_of(2013, 1, 26)), "2001 on: the ACN is not the AFC's");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2002, doy_of(2002, 2, 20)), "2001 on: ACN 2002 on the original's days");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2019, doy_of(2019, 7, 15)), "2001 on: ACN 2019 of 24 in July");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2025, doy_of(2025, 12, 28)), "2001 on: ACN 2025/26 in December");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2026, doy_of(2026, 1, 10)), "2001 on: ACN 2025/26 in January");
    CHECK(season_intl_finals_busy(SEASON_CONF_ASIA, 2019, doy_of(2019, 1, 30)), "2001 on: Asian Cup 2019");
    CHECK(season_intl_finals_busy(SEASON_CONF_SOUTH_AMERICA, 2004, doy_of(2004, 7, 17)), "2001 on: Copa America 2004");
    CHECK(season_intl_finals_busy(SEASON_CONF_SOUTH_AMERICA, 2016, doy_of(2016, 6, 20)), "2001 on: Copa Centenario");
    CHECK(season_intl_finals_busy(SEASON_CONF_NORTH_AMERICA, 2016, doy_of(2016, 6, 20)), "2001 on: its CONCACAF guests");
    CHECK(!season_intl_finals_busy(SEASON_CONF_SOUTH_AMERICA, 2016, doy_of(2016, 9, 6)), "2001 on: September 2016 free");
    CHECK(season_intl_finals_busy(SEASON_CONF_EUROPE, 2016, doy_of(2016, 7, 8)), "2001 on: Euro 2016");
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2014, doy_of(2014, 7, 1)), "2001 on: the World Cup for all");
    CHECK(season_intl_finals_busy(SEASON_CONF_OCEANIA, 2022, doy_of(2022, 12, 10)), "2001 on: the World Cup 2022");
    CHECK(!season_intl_finals_busy(SEASON_CONF_EUROPE, 2009, doy_of(2009, 6, 20)), "2001 on: no Confederations Cup");
    CHECK(!season_intl_finals_busy(SEASON_CONF_EUROPE, 2008, doy_of(2008, 8, 20)), "2001 on: no Olympics");
    CHECK(!season_intl_finals_busy(-1, 2013, doy_of(2013, 1, 26)) && !season_intl_finals_busy(SEASON_CONFS, 2013, 25),
          "no confederation");
    /* Changing rules off from 2013: the ACN of 2015 (the first from 2013/14) every two years on its days. */
    game(1, 2013, 0);
    CHECK(season_intl_finals_busy(SEASON_CONF_AFRICA, 2017, doy_of(2017, 1, 20)), "2013 off: ACN 2017 on 2015's days");
    CHECK(!season_intl_finals_busy(SEASON_CONF_AFRICA, 2016, doy_of(2016, 1, 20)), "2013 off: no ACN in 2016");
    CHECK(!season_intl_finals_busy(SEASON_CONF_AFRICA, 2013, doy_of(2013, 1, 26)), "2013 off: ACN 2013 lies before");

    /* The days of the CONCACAF Gold Cup (open item 55): its own editions (season_nl_table.c), every two years after
     * the last. The game asks only when it has a Gold Cup object (season_gold_cup.c). */
    game(1, 2001, 0);
    CHECK(!season_intl_gold_busy(2017, doy_of(2017, 7, 10)), "base game: no Gold Cup days");
    game(1, 2001, 1);
    CHECK(season_intl_gold_busy(2017, doy_of(2017, 7, 10)), "2001 on: Gold Cup 2017 from 7 July");
    CHECK(season_intl_gold_busy(2017, doy_of(2017, 7, 5)), "2001 on: two days before the Gold Cup");
    CHECK(!season_intl_gold_busy(2017, doy_of(2017, 7, 4)), "2001 on: three days before");
    CHECK(!season_intl_gold_busy(2016, doy_of(2016, 7, 10)), "2001 on: no Gold Cup in 2016");
    CHECK(season_intl_gold_busy(2017, doy_of(2017, 8, 6)), "2001 on: the last day of the 2017 span (7 July + 30)");
    CHECK(!season_intl_gold_busy(2017, doy_of(2017, 8, 7)), "2001 on: the day after the span");
    CHECK(season_intl_gold_busy(2019, doy_of(2019, 7, 10)), "2001 on: Gold Cup 2019 of 16 from 15 June");
    CHECK(season_intl_gold_busy(2029, doy_of(2029, 6, 25)), "2001 on: after the table, every two years on 2027's days");
    CHECK(!season_intl_gold_busy(2030, doy_of(2030, 6, 25)), "2001 on: none in 2030");
    CHECK(season_intl_gold_edition(2017) && season_intl_gold_edition(2017)->teams == 12, "2001 on: the 2017 edition");
    CHECK(!season_intl_gold_edition(2018), "2001 on: no 2018 edition");
    game(1, 2013, 0);
    CHECK(season_intl_gold_busy(2017, doy_of(2017, 7, 10)), "2013 off: 2015's Gold Cup every two years");
    CHECK(!season_intl_gold_busy(2016, doy_of(2016, 7, 10)), "2013 off: no Gold Cup in 2016");
    CHECK(!season_intl_gold_busy(2013, doy_of(2013, 7, 10)), "2013 off: the 2013 Gold Cup lies before the game");

    /* Issue 146: the news of the qualified nations names real hosts for the Euros with an empty host row. */
    CHECK(season_intl_news_hosts("euro_champ", 2020) && !strcmp(season_intl_news_hosts("euro_champ", 2020), "England"),
          "Euro 2020 news host");
    CHECK(season_intl_news_hosts("euro_champ", 2028) &&
          !strcmp(season_intl_news_hosts("euro_champ", 2028), "England;Republic of Ireland"), "Euro 2028 news hosts");
    CHECK(!season_intl_news_hosts("euro_champ", 2024), "Euro 2024 keeps its host row");
    CHECK(!season_intl_news_hosts("euro_champ", 2032), "Euro 2032 keeps its host row");
    CHECK(!season_intl_news_hosts("world_cup", 2030), "World Cup 2030 keeps its host row");
    {
        const SeasonIntlEdition *wc = season_intl_edition("world_cup", 2030);
        CHECK(wc && !strcmp(wc->hosts, "Morocco;Portugal;Spain;Argentina;Paraguay;Uruguay"), "World Cup 2030 hosts");
        CHECK(season_intl_announced_hosts("euro_champ", 2032) &&
              !strncmp(season_intl_announced_hosts("euro_champ", 2032), "Italy;Turkey", 12), "Euro 2032 hosts");
    }

    if (failures) {
        fprintf(stderr, "season_intl_test: %d failures\n", failures);
        return 1;
    }
    printf("season_intl_test: all checks pass\n");
    return 0;
}
