/* Test of the titles taken away after the season (0.6.2 item 108, src/recomp_rt/win/season_titles.c).
 *
 * Usage: season_titles_test
 * The history rows give today's record: Serie A 2004/05, the Chinese First Division A 2003 and the Polish Cup 2005
 * have no row (revoked, given to no club). A start season before the ruling takes the holder of that time. */
#include "season_titles.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            failures++; \
            printf("FAIL line %d: ", __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
        } \
    } while (0)

static int is(const char *got, const char *want)
{
    return got && !strcmp(got, want);
}

int main(void)
{
    const char *nat = NULL;
    /* Serie A 2004/05: Juventus 1st, AC Milan 2nd; the newest row of a 2005 start is 2003/04 (AC Milan). The
     * Supercoppa of 20 August 2005 was Juventus v Internazionale (it.wikipedia.org/wiki/Supercoppa_italiana_2005). */
    CHECK(is(season_title_then("Italian Serie A", 2005, 2004, "AC Milan", 0, &nat), "Juventus") && is(nat, "Italy"),
          "2005 start: Juventus hold Serie A");
    CHECK(is(season_title_then("Italian Serie A", 2005, 2004, "AC Milan", 1, &nat), "AC Milan"),
          "2005 start: AC Milan 2nd");
    CHECK(!season_title_then("Italian Serie A", 2005, 2004, "AC Milan", 2, &nat), "the third place stands");
    CHECK(!season_title_then("Italian Serie A", 2005, 2004, "Juventus", 0, &nat),
          "a database whose own 2004/05 row holds Juventus stands");
    /* Serie A 2005/06 was given to Internazionale on 26 July 2006: the row holds it, so a 2006 start reads it. */
    CHECK(!season_title_then("Italian Serie A", 2006, 2006, "Internazionale", 0, &nat), "2006 start: the row stands");
    CHECK(!season_title_then("Italian Serie A", 2004, 2003, "AC Milan", 0, &nat), "another start season");
    /* The Polish Cup 2004/05 (PZPN ruling of 2 September 2020): Groclin Dyskobolia beat Zaglebie Lubin. The newest
     * row of the September 2005 database is 2004 (Lech Poznan). */
    CHECK(is(season_title_then("Polish FA Cup", 2005, 2004, "KKS Lech Poznan", 0, &nat), "SSA Groclin Dyskobolia") &&
              is(nat, "Poland"),
          "2005 start: Dyskobolia hold the Polish Cup");
    CHECK(is(season_title_then("Polish FA Cup", 2005, 2004, "KKS Lech Poznan", 1, &nat), "SSA Zaglebie Lubin"),
          "2005 start: Zaglebie the cup runner-up");
    /* The Chinese First Division A 2003 (CFA ruling of 18 February 2013): Shanghai Shenhua 1st, Shanghai
     * International 2nd (Shanghai Zhongyuan in October 2003). The newest row of October 2003 is 2002 (Dalian Shide). */
    CHECK(is(season_title_then("Chinese First Division A", 2003, 2002, "Dalian Shide", 0, &nat), "Shanghai Shenhua") &&
              is(nat, "China PR"),
          "2003 start: Shanghai Shenhua hold the Chinese title");
    CHECK(is(season_title_then("Chinese First Division A", 2003, 2002, "Dalian Shide", 1, &nat), "Shanghai Zhongyuan"),
          "2003 start: Shanghai Zhongyuan 2nd");
    CHECK(!season_title_then("Chinese First Division A", 2004, 2004, "Shenzhen Pingan", 0, &nat),
          "2004 start: the 2004 row is newer");
    CHECK(!season_title_then(NULL, 2005, 2004, "AC Milan", 0, &nat) && !season_title_then("Italian Serie A", 2005, 2004, NULL, 0, &nat),
          "no competition or no row");
    CHECK(!season_title_then("Italian Serie A", 2005, 2004, "AC Milan", -1, &nat), "a place below 0");
    /* A row the game wrote later has a later label: AC Milan winning the game's own 2005/06 keeps that row. */
    CHECK(!season_title_then("Italian Serie A", 2005, 2006, "AC Milan", 0, &nat), "the game's own 2005/06 row stands");
    CHECK(!season_title_then("Polish FA Cup", 2005, 2006, "KKS Lech Poznan", 0, &nat),
          "the game's own 2005/06 cup row stands");
    CHECK(season_title_has("Polish FA Cup", 2005) && !season_title_has("Polish FA Cup", 2006) &&
              !season_title_has("Italian Serie B", 2005) && !season_title_has(NULL, 2005),
          "the competitions and seasons of the table");
    if (failures) {
        printf("FAILED: %d\n", failures);
        return 1;
    }
    printf("season titles: all checks pass\n");
    return 0;
}
