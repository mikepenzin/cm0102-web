/* The plan of the Swedish third tier in two groups (issue 106, season_swe_groups_plan.c). */
#include <stdio.h>
#include <string.h>

#include "season_swe_groups_plan.h"

static int fails;

static void check(int got, int want, const char *what)
{
    if (got == want) return;
    fprintf(stderr, "FAIL %s: got %d, want %d\n", what, got, want);
    fails++;
}

static void plan(const char *split, int clubs, int up, int down, int on, int size, int up_each, int down_each)
{
    SweGroupsPlan p;
    memset(&p, 0, sizeof p);
    check(season_swe_groups_plan(split, clubs, up, down, &p), on, split);
    if (!on) return;
    check(p.clubs, clubs, "clubs");
    check(p.size, size, "size");
    check(p.rounds, 2 * (size - 1), "rounds");
    check(p.up, up_each, "up of a group");
    check(p.down, down_each, "down of a group");
}

int main(void)
{
    /* Rows of the rules table: den-swe-184 (2004/05), den-swe-190, den-swe-196, den-swe-262. */
    plan("6 regional groups of 12", 72, 18, 12, 0, 0, 0, 0);
    plan("2 groups of 14 (Norra, S\xc3\xb6" "dra)", 28, 2, 6, 1, 14, 1, 3);
    plan("2 groups of 14 (Norra, S\xc3\xb6" "dra)", 28, 4, 4, 1, 14, 2, 2);
    plan("2 groups of 16 (Norra, S\xc3\xb6" "dra)", 32, 2, 6, 1, 16, 1, 3);
    /* A row that does not add up, or has no text, keeps the class's six groups. */
    plan("2 groups of 16 (Norra, S\xc3\xb6" "dra)", 28, 2, 6, 0, 0, 0, 0);
    plan("2 groups of 1", 2, 0, 0, 0, 0, 0, 0);
    plan(NULL, 28, 2, 6, 0, 0, 0, 0);
    plan("unknown", 28, 2, 6, 0, 0, 0, 0);

    /* The deal: the most northern half goes to Norra, a tie keeps the club order. */
    int north[6] = {5, -3, 40, 5, -20, 0}, group[6];
    season_swe_groups_deal(north, 6, 3, group);
    int want[6] = {0, 1, 0, 0, 1, 1};
    for (int i = 0; i < 6; i++) check(group[i], want[i], "group of a club");

    if (fails) return 1;
    puts("season_swe_groups: pass");
    return 0;
}
