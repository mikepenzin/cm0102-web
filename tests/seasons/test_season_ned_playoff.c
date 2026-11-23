/* The plan of the Dutch nacompetitie (issue 150). The ties below are the real ties of the RSSSF season pages
 * (tablesn/ned06.html, ned2012.html, ned2019.html, ned2021.html to ned2026.html). */
#include "season_ned_playoff_plan.h"

#include <stdio.h>

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

/* The entrant of a side of a tie in the first round, or -1 for a winner. */
static int entrant(NedPoForm form, int bracket, int side)
{
    int list[NED_PO_MAX_LIST];
    int n = ned_po_entrants(form, bracket, list);
    return side >= 0 && side < n ? list[side] : -1;
}

static void check_tie(NedPoForm form, int bracket, int round, int tie, int want_first, int want_second)
{
    int a = -99, b = -99;
    CHECK(ned_po_tie(form, round, tie, &a, &b));
    if (want_first <= -1) CHECK(a == want_first);
    else CHECK(entrant(form, bracket, a) == want_first);
    if (want_second <= -1) CHECK(b == want_second);
    else CHECK(entrant(form, bracket, b) == want_second);
}

/* Every club enters once, each round's winners and new clubs fill it, and one club is left. */
static void check_shape(NedPoForm form)
{
    int total = 0, seen[NED_PO_UPPER + 2] = {0};
    for (int b = 0; b < ned_po_brackets(form); b++) {
        int list[NED_PO_MAX_LIST], n = ned_po_entrants(form, b, list), alive = 0, entered = 0;
        CHECK(n > 0);
        total += n;
        for (int i = 0; i < n; i++) seen[list[i]]++;
        for (int r = 0; r < ned_po_rounds(form); r++) {
            NedPoRound o;
            CHECK(ned_po_round(form, r, &o));
            if (o.add) CHECK(o.first == entered);
            entered += o.add;
            CHECK(o.clubs == alive + o.add);
            CHECK(o.ties * 2 == o.clubs);
            int used[NED_PO_MAX_LIST] = {0}, won[NED_PO_MAX_TIES] = {0};
            for (int t = 0; t < o.ties; t++) {
                int s[2];
                CHECK(ned_po_tie(form, r, t, &s[0], &s[1]));
                for (int k = 0; k < 2; k++) {
                    if (s[k] >= 0) {
                        CHECK(s[k] >= o.first && s[k] < o.first + o.add);
                        used[s[k]]++;
                    } else {
                        CHECK(r > 0 && -s[k] - 1 < NED_PO_MAX_TIES);
                        won[-s[k] - 1]++;
                    }
                }
            }
            for (int i = 0; i < o.add; i++) CHECK(used[o.first + i] == 1);
            for (int t = 0; t < alive; t++) CHECK(won[t] == 1);
            alive = o.ties;
        }
        CHECK(entered == n);
        CHECK(alive == 1);
    }
    CHECK(total == ned_po_lower_clubs(form) + ned_po_upper_clubs(form));
    for (int s = 0; s < ned_po_lower_clubs(form); s++) CHECK(seen[s] == 1);
    for (int k = 0; k < ned_po_upper_clubs(form); k++) CHECK(seen[NED_PO_UPPER + k] == 1);
}

int main(void)
{
    /* The seasons of each form. */
    for (int y = 2001; y <= 2004; y++) CHECK(ned_po_form(y) == NED_PO_GROUPS);
    for (int y = 2005; y <= 2018; y++) CHECK(ned_po_form(y) == NED_PO_TEN);
    CHECK(ned_po_form(2019) == NED_PO_NONE);
    for (int y = 2020; y <= 2024; y++) CHECK(ned_po_form(y) == NED_PO_SEVEN);
    CHECK(ned_po_form(2025) == NED_PO_LADDER);
    for (int y = 2026; y <= 2030; y++) CHECK(ned_po_form(y) == NED_PO_SEVEN);

    /* 2020/21 was single matches (ned2021.html). */
    CHECK(ned_po_legs(2020) == 1);
    CHECK(ned_po_legs(2018) == 2);
    CHECK(ned_po_legs(2021) == 2);
    CHECK(ned_po_legs(2025) == 2);

    CHECK(ned_po_brackets(NED_PO_TEN) == 2);
    CHECK(ned_po_brackets(NED_PO_SEVEN) == 1);
    CHECK(ned_po_brackets(NED_PO_LADDER) == 1);
    CHECK(ned_po_brackets(NED_PO_GROUPS) == 0);
    CHECK(ned_po_brackets(NED_PO_NONE) == 0);
    CHECK(ned_po_lower_clubs(NED_PO_TEN) == 8 && ned_po_upper_clubs(NED_PO_TEN) == 2);
    CHECK(ned_po_lower_clubs(NED_PO_SEVEN) == 6 && ned_po_upper_clubs(NED_PO_SEVEN) == 1);
    CHECK(ned_po_lower_clubs(NED_PO_LADDER) == 6 && ned_po_upper_clubs(NED_PO_LADDER) == 1);
    CHECK(ned_po_rounds(NED_PO_TEN) == 3 && ned_po_rounds(NED_PO_SEVEN) == 3 && ned_po_rounds(NED_PO_LADDER) == 4);
    CHECK(ned_po_days(NED_PO_TEN, 2) == 21 && ned_po_days(NED_PO_SEVEN, 1) == 17 && ned_po_days(NED_PO_LADDER, 2) == 28);

    check_shape(NED_PO_TEN);
    check_shape(NED_PO_SEVEN);
    check_shape(NED_PO_LADDER);

    /* 2011/12, seeds 0 to 7 are places 2 to 9. Bracket 0: 9th v 6th, the winner v the Eredivisie 17th, 5th v 2nd.
     * Bracket 1: 8th v 7th, the winner v the Eredivisie 16th, 4th v 3rd. */
    check_tie(NED_PO_TEN, 0, 0, 0, 7, 4);
    check_tie(NED_PO_TEN, 0, 1, 0, -1, NED_PO_UPPER + 1);
    check_tie(NED_PO_TEN, 0, 1, 1, 3, 0);
    check_tie(NED_PO_TEN, 0, 2, 0, -1, -2);
    check_tie(NED_PO_TEN, 1, 0, 0, 6, 5);
    check_tie(NED_PO_TEN, 1, 1, 0, -1, NED_PO_UPPER);
    check_tie(NED_PO_TEN, 1, 1, 1, 2, 1);

    /* 2022/23: 8th v 3rd, 7th v 4th, 6th v 5th; the first two winners meet, the third meets the Eredivisie 16th. */
    check_tie(NED_PO_SEVEN, 0, 0, 0, 5, 0);
    check_tie(NED_PO_SEVEN, 0, 0, 1, 4, 1);
    check_tie(NED_PO_SEVEN, 0, 0, 2, 3, 2);
    check_tie(NED_PO_SEVEN, 0, 1, 0, -1, -2);
    check_tie(NED_PO_SEVEN, 0, 1, 1, -3, NED_PO_UPPER);
    check_tie(NED_PO_SEVEN, 0, 2, 0, -1, -2);

    /* 2025/26: 8th v 6th, 9th v 5th; the winners meet the 3rd and the 4th; a semi-final; the final with the 16th. */
    check_tie(NED_PO_LADDER, 0, 0, 0, 4, 3);
    check_tie(NED_PO_LADDER, 0, 0, 1, 5, 2);
    check_tie(NED_PO_LADDER, 0, 1, 0, -1, 0);
    check_tie(NED_PO_LADDER, 0, 1, 1, -2, 1);
    check_tie(NED_PO_LADDER, 0, 2, 0, -1, -2);
    check_tie(NED_PO_LADDER, 0, 3, 0, -1, NED_PO_UPPER);

    /* The last round is the final (id 150): the stage gives its winner result code 1. */
    NedPoRound last;
    CHECK(ned_po_round(NED_PO_TEN, 2, &last) && last.id == 150);
    CHECK(ned_po_round(NED_PO_SEVEN, 2, &last) && last.id == 150);
    CHECK(ned_po_round(NED_PO_LADDER, 3, &last) && last.id == 150);
    CHECK(!ned_po_round(NED_PO_TEN, 3, &last));
    CHECK(!ned_po_round(NED_PO_GROUPS, 0, &last));

    if (failures) printf("%d failures\n", failures);
    else printf("season_ned_playoff: all passed\n");
    return failures != 0;
}
