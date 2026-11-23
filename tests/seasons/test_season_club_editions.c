/* Test of the club cup editions after the rules table (0.6.2 slice 9a, src/recomp_rt/win/season_club_editions.c),
 * with the rules table of the build, stepped to 2301.
 *
 * Usage: season_club_editions_test <rules.tsv made by tools/seasons/make_rules.py>
 * The values come from docs/research/rules-0.6.0/club-world.tsv: the last rows (2027/28) of the Copa Sudamericana,
 * the Recopa, the FIFA Intercontinental Cup and ACL Two say "unknown", the Suruga Bank Championship is not held from
 * 2020/21, the Club World Cup of 2024/25 has 32 clubs and the rows 2025/26 to 2027/28 do not hold it. */
#include "season_club_editions.h"
#include "season_rules.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) \
    do { if (!(cond)) { failures++; printf("FAIL line %d, expected: ", __LINE__); printf(__VA_ARGS__); \
         printf("\n"); } } while (0)

#define LAST 2301                       /* past 2300: no limit at 2300 */

static const SeasonClubCup sudamericana = {"sudamericana", 1, 0}, recopa = {"recopa", 1, 0},
                           suruga = {"suruga_bank", 1, 0}, intercontinental = {"fifa_intercontinental_cup", 0, 0},
                           acl_two = {"acl_two", 0, 1};

/* 1 when day d of month m of year y meets the duty of a national side: a FIFA day or the day beside it
 * (season_fifa_meets), or the day before that, when the linked Under 21 or 'B' match plays the day before the FIFA
 * day (ReserveComp::add_b_or_under21_fixture 0x7e6720) and its players are on duty from the day before it (0x7757d0;
 * open item 163). */
static int duty(int y, int m, int d)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (season_fifa_meets(y, m, d)) return 1;
    int last = len[m - 1] + (m == 2 && season_club_is_leap(y));
    return d < last ? season_fifa_meets(y, m, d + 1) : m < 12 ? season_fifa_meets(y, m + 1, 1) : season_fifa_meets(y + 1, 1, 1);
}

/* Day and month of a day of the year, as a number 100 * month + day. */
static int month_day(int doy, int year)
{
    for (int m = 12; m >= 1; m--)
        if (doy >= season_club_day_of_year(1, m, year)) return 100 * m + doy - season_club_day_of_year(1, m, year) + 1;
    return 0;
}

/* Every year after the last table edition has an edition with the last edition's clubs, legs, days and months. */
static void every_year(const SeasonClubCup *k, int last_expected)
{
    int last = season_club_last_edition(k);
    CHECK(last == last_expected, "%s: last table edition %d (got %d)", k->comp, last_expected, last);
    CHECK(season_club_continues(k), "%s continues after the table", k->comp);
    SeasonClubEdition t;
    if (!season_club_edition_in_table(k, last, &t)) { CHECK(0, "%s: the table edition of %d", k->comp, last); return; }
    int bad = 0;
    for (int y = last + 1; y <= LAST; y++) {
        SeasonClubEdition e;
        int ok = season_club_edition(k, y, &e);
        int want_year = t.year + (y - last);
        if (!ok || e.clubs != t.clubs || e.legs != t.legs || e.final_legs != t.final_legs || e.year != want_year ||
            e.last_year != t.last_year + (y - last) || month_day(e.first, e.year) != month_day(t.first, t.year) ||
            (month_day(t.last, t.last_year) != 229 && month_day(e.last, e.last_year) != month_day(t.last, t.last_year)) ||
            season_club_next_edition(k, y) != y) {
            if (bad++ < 3)
                CHECK(0, "%s %d: an edition as the table's %d moved (ok %d, clubs %d, year %d, days %d to %d)",
                      k->comp, y, last, ok, ok ? e.clubs : 0, ok ? e.year : 0, ok ? e.first : 0, ok ? e.last : 0);
        }
        if (k->calendar && ok && e.year != y && bad++ < 3) CHECK(0, "%s %d: a calendar cup's edition in %d", k->comp, y, y);
    }
    if (bad > 3) printf("%s: %d years failed in all\n", k->comp, bad);
    /* The next edition from a year inside the table that holds none after it is the first after the table. */
    CHECK(season_club_next_edition(k, last + 1) == last + 1, "%s: the next edition from %d is %d", k->comp, last + 1,
          last + 1);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: season_club_editions_test <rules.tsv>\n");
        return 2;
    }
    if (season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    /* Q2: the four cups every year after the table (faults 7 and 11). */
    every_year(&sudamericana, 2027);
    every_year(&recopa, 2027);
    every_year(&intercontinental, 2026);
    every_year(&acl_two, 2026);

    /* Inside the table nothing changes: the Sudamericana of 2027 from the row 2026/27. */
    SeasonClubEdition e;
    CHECK(season_club_edition(&sudamericana, 2027, &e) && e.row && e.row->season == 2026,
          "the Sudamericana of 2027 from the row 2026/27");
    CHECK(season_club_edition(&intercontinental, 2024, &e) && e.clubs == 6 && e.row->season == 2024,
          "the FIFA Intercontinental Cup of 2024/25 with 6 clubs");
    CHECK(!season_club_edition(&intercontinental, 2023, &e), "no FIFA Intercontinental Cup in 2023/24");
    /* The row 2026/27 gives "26 August – December 2026", no last day (spec review): the days of 2025/26 (14 September
     * to 17 December) moved a year, never all rounds on one day; after the table the same. */
    for (int y = 2024; y <= LAST; y++)
        if (!season_club_edition(&intercontinental, y, &e) || e.last - e.first < 60 ||
            month_day(e.last, e.last_year) < 1201) {
            CHECK(0, "the FIFA Intercontinental Cup of %d from September to December (days %d to %d)", y, e.first,
                  e.last);
            break;
        }
    CHECK(season_club_next_edition(&intercontinental, 2001) == 2024, "the first FIFA Intercontinental Cup is 2024/25");

    /* Q7: the Suruga Bank Championship ends in 2019 and never comes back. */
    CHECK(season_club_last_edition(&suruga) == 2019, "the last Suruga Bank Championship is 2019");
    CHECK(!season_club_continues(&suruga), "the Suruga Bank Championship does not continue");
    for (int y = 2020; y <= LAST; y++)
        if (season_club_edition(&suruga, y, &e)) { CHECK(0, "no Suruga Bank Championship in %d", y); break; }
    CHECK(season_club_next_edition(&suruga, 2020) == 0, "no Suruga Bank Championship from 2020");
    CHECK(season_club_next_edition(&suruga, 2019) == 2019, "a Suruga Bank Championship in 2019");

    /* Q3: the Club World Cup every 4 years from 2029 (season 2028/29) with the 32 clubs of 2024/25. */
    for (int s = 2025; s <= LAST; s++) {
        const SeasonRule *r = season_club_world_row("world_club_champ", s);
        int held = r && r->clubs != SEASON_RULE_NOT_HELD && strcmp(r->format, "not_held");
        int want = s >= 2028 && (s - 2024) % 4 == 0;
        if (held != want || (held && (r->clubs != 32 || r->season != 2024))) {
            CHECK(0, "Club World Cup in season %d: held %d, 32 clubs of the row 2024/25", s, want);
            break;
        }
    }
    const SeasonRule *cwc = season_club_world_row("world_club_champ", 2019);
    CHECK(cwc && cwc->season == 2019 && cwc->clubs == 7, "the Club World Cup of 2019/20 from its own row");
    const SeasonRule *oc = season_club_world_row("oceania_club_champ", 2200);
    CHECK(oc && oc->season == 2027, "the OFC Champions League in 2200 from its last row");

    /* The Libertadores places: every table row has a "places:" text (the 2027/28 row too), and after the table the
     * last row's. */
    for (int s = 2001; s <= LAST; s++) {
        const SeasonRule *r = season_club_places_row("libertadores", s);
        int want = s <= 2027 ? s : 2027;
        if (!r || r->season != want || !strstr(r->format_text, "ARG ") || !strstr(r->format_text, "BRA ")) {
            CHECK(0, "the Libertadores places of season %d from the row %d", s, want);
            break;
        }
    }

    /* Open item 95: no leg of a round on a day that meets a FIFA day (the Copa Sudamericana of 9 August 2011, the day
     * before the FIFA day of 10 August, lost the players on duty), and the rounds in order, each after the second
     * leg of the one before. */
    const SeasonClubCup *all[] = {&sudamericana, &recopa, &suruga, &intercontinental, &acl_two};
    int bad_days = 0, checked = 0;
    for (int c = 0; c < 5; c++)
        for (int y = 2002; y <= LAST; y++) {
            if (!season_club_edition(all[c], y, &e)) continue;
            SeasonClubRound r[8];
            int n = e.clubs > 64 ? 64 : e.clubs, count = season_club_plan_rounds(all[c]->comp, &e, n, r), end = -1000;
            for (int i = 0; i < count; i++) {
                int gap = season_club_second_leg(&e, &r[i], count);
                for (int leg = 0; leg <= (gap ? 1 : 0); leg++) {
                    int doy = r[i].match + leg * gap, yy = e.year;
                    while (doy >= 365 + season_club_is_leap(yy)) doy -= 365 + season_club_is_leap(yy++);
                    int m, d;
                    season_from_day_of_year(yy, doy, &m, &d);
                    checked++;
                    if (duty(yy, m, d) && bad_days++ < 5)
                        CHECK(0, "%s %d round %d: no match on %d.%d.%d (a FIFA day, the day beside it or two days before it)",
                              all[c]->comp, y, i, d, m, yy);
                }
                /* round_table draws a round on the day after the last leg before it, so its match comes 2 days on */
                if (r[i].match < end + 2 && bad_days++ < 5)
                    CHECK(0, "%s %d round %d at least 2 days after the round before", all[c]->comp, y, i);
                end = r[i].match + gap;
            }
            if (count && r[0].match < e.first && bad_days++ < 5)
                CHECK(0, "%s %d round 0 not before the edition's first day", all[c]->comp, y);
        }
    /* With changing rules off a start season's edition moves to every later year (season_club_cups.c edition()). */
    for (int c = 0; c < 5; c++)
        for (int base = 2002; base <= 2027; base++) {
            SeasonClubEdition b;
            if (!season_club_edition(all[c], base, &b)) continue;
            for (int y = base; y <= LAST; y++) {
                e = b;
                season_club_edition_move(&e, y - base);
                SeasonClubRound r[8];
                int n = e.clubs > 64 ? 64 : e.clubs, count = season_club_plan_rounds(all[c]->comp, &e, n, r);
                for (int i = 0; i < count; i++) {
                    int gap = season_club_second_leg(&e, &r[i], count);
                    for (int leg = 0; leg <= (gap ? 1 : 0); leg++) {
                        int doy = r[i].match + leg * gap, yy = e.year;
                        while (doy >= 365 + season_club_is_leap(yy)) doy -= 365 + season_club_is_leap(yy++);
                        int m, d;
                        season_from_day_of_year(yy, doy, &m, &d);
                        checked++;
                        if (duty(yy, m, d) && bad_days++ < 5)
                            CHECK(0, "%s of %d moved to %d, round %d: no match on %d.%d.%d", all[c]->comp, base, y, i,
                                  d, m, yy);
                    }
                }
            }
        }
    if (bad_days > 5) printf("%d legs on FIFA days in all, of %d\n", bad_days, checked);
    if (bad_days) failures++;

    /* Open item 163 (run set pass 4, a2001nation): the Copa Sudamericana of 2011 played a leg on 8 August 2011, two
     * days before the FIFA day of 10 August, and Paraguay's Under 21 side played Cameroon on 9 August. No leg of 2011
     * from 8 to 11 August. */
    if (season_club_edition(&sudamericana, 2011, &e)) {
        SeasonClubRound r[8];
        int n = e.clubs > 64 ? 64 : e.clubs, count = season_club_plan_rounds("sudamericana", &e, n, r);
        for (int i = 0; i < count; i++) {
            int gap = season_club_second_leg(&e, &r[i], count);
            for (int leg = 0; leg <= (gap ? 1 : 0); leg++) {
                int doy = r[i].match + leg * gap, md = month_day(doy, 2011);
                CHECK(md < 808 || md > 811, "sudamericana 2011 round %d leg %d: not on %d August 2011", i, leg + 1,
                      md % 100);
            }
        }
    } else {
        CHECK(0, "sudamericana: an edition of 2011");
    }

    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("club cup editions: all checks passed to %d\n", LAST);
    return 0;
}
