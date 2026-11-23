/* The cups of eastern Europe and Asia follow the divisions (0.7.2 league sizes, slice 6, D14):
 * season_cup_sizes_east.h, the counts that src/recomp_rt/win/season_cup_sizes_east.c gives each cup.
 *
 * With the original's counts each value is the constant of the original, so a season game with the class counts
 * plays the cup as before:
 *   GreCup::collect_entrants 0x5e9a80     16 lower picks (push 0x10 at 0x5e9b60) = 44 - 14 - 14
 *   RusCup::select_entrants 0x7e99a0      14 lower picks (push 0xe at 0x7e9b74) = 32 - 18
 *   TurCup::collect_entrants 0x8fcb90     26 lower picks (mov edx, 0x1a at 0x8fcd34) = 64 - 18 - 20
 *   JapEmpCup::collect_entrants 0x6624f0  47 lower picks (push 0x2f at 0x6626b9) = 80 - 5 - 12 - 16
 *   JapJCup::collect_entrants 0x665a00    J2 up to the count 28 (0x665aba), here 28 - J1 16 = 12
 *   PolLgeCup::collect_entrants 0x7cad30  second tier up to the count 34 (0x7cae01), here 34 - 16 = 18
 *   KorLeagueCup::select_teams 0x66cf20   10 places (0x66cf30), no club left out of 10 */
#include "season_cup_sizes_east.h"

#include <stdio.h>

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                       printf("\n"); } } while (0)

static void originals(void)
{
    CHECK(east_lower_picks(44, 14 + 14, 60, 1) == 16, "GreCup: %d", east_lower_picks(44, 28, 60, 1));
    CHECK(east_lower_picks(32, 18, 40, 1) == 14, "RusCup: %d", east_lower_picks(32, 18, 40, 1));
    CHECK(east_lower_picks(64, 18 + 20, 30, 1) == 26, "TurCup: %d", east_lower_picks(64, 38, 30, 1));
    CHECK(east_lower_picks(75, 12 + 16, 90, 1) == 47, "JapEmpCup: %d", east_lower_picks(75, 28, 90, 1));
    CHECK(east_bound(28, 16) == 12, "JapJCup: %d", east_bound(28, 16));
    CHECK(east_bound(34, 16) == 18, "PolLgeCup: %d", east_bound(34, 16));
    CHECK(east_bound(36, 16) == 20, "PolLgeCup 2001: %d", east_bound(36, 16));
    CHECK(east_skip(10, 10) == 0, "KorLeagueCup: %d", east_skip(10, 10));
}

/* Grown divisions: the lower clubs give up their places, every club of the top divisions plays. */
static void grown(void)
{
    CHECK(east_lower_picks(44, 16 + 18, 60, 1) == 10, "GreCup 2010 (16 and 18): %d", east_lower_picks(44, 34, 60, 1));
    CHECK(east_lower_picks(32, 22, 40, 1) == 10, "RusCup First 22: %d", east_lower_picks(32, 22, 40, 1));
    CHECK(east_lower_picks(64, 21 + 18, 30, 1) == 25, "TurCup Super Lig 21: %d", east_lower_picks(64, 39, 30, 1));
    CHECK(east_lower_picks(75, 22 + 18, 90, 1) == 35, "JapEmpCup J2 22, J1 18: %d", east_lower_picks(75, 40, 90, 1));
    CHECK(east_bound(28, 18) == 10, "JapJCup J1 18: %d", east_bound(28, 18));
    CHECK(east_bound(34, 18) == 16, "PolLgeCup Ekstraklasa 18: %d", east_bound(34, 18));
    CHECK(east_skip(16, 10) == 6, "KorLeagueCup K League 16: %d", east_skip(16, 10));
    /* More top clubs than places: no lower club, but the copy loops of the original run at least once. */
    CHECK(east_lower_picks(32, 34, 40, 1) == 1, "RusCup First 34: %d", east_lower_picks(32, 34, 40, 1));
    CHECK(east_bound(28, 30) == 0, "JapJCup J1 30: %d", east_bound(28, 30));
}

/* Shrunk divisions: more lower clubs, up to the clubs that the pool holds. */
static void shrunk(void)
{
    CHECK(east_lower_picks(44, 14 + 10, 60, 1) == 20, "GreCup 2024 (14 and 10): %d", east_lower_picks(44, 24, 60, 1));
    CHECK(east_lower_picks(44, 14 + 10, 18, 1) == 18, "GreCup, pool of 18: %d", east_lower_picks(44, 24, 18, 1));
    CHECK(east_lower_picks(32, 17, 40, 1) == 15, "RusCup First 17: %d", east_lower_picks(32, 17, 40, 1));
    CHECK(east_lower_picks(64, 18 + 17, 30, 1) == 29, "TurCup second tier 17: %d", east_lower_picks(64, 35, 30, 1));
    CHECK(east_lower_picks(64, 18 + 14, 30, 1) == 30, "TurCup, pool of 30: %d", east_lower_picks(64, 32, 30, 1));
    CHECK(east_bound(34, 14) == 20, "PolLgeCup Ekstraklasa 14: %d", east_bound(34, 14));
    CHECK(east_skip(9, 10) == 0, "KorLeagueCup K League 9: %d", east_skip(9, 10));
    CHECK(east_lower_picks(44, 24, 0, 1) == 0, "an empty pool gives no pick: %d", east_lower_picks(44, 24, 0, 1));
}

/* A short list moves its clubs to the last places (CroCup's First Division clubs stay last, the empty places
 * that season_cup_entrants.c fills come first). */
static void tail(void)
{
    CHECK(east_tail_place(48, 48, 0) == 0 && east_tail_place(48, 48, 47) == 47, "a full list stays");
    CHECK(east_tail_place(48, 22, 0) == 26 && east_tail_place(48, 22, 21) == 47, "22 of 48: %d to %d",
          east_tail_place(48, 22, 0), east_tail_place(48, 22, 21));
    CHECK(east_tail_place(34, 33, 0) == 1, "33 of 34: %d", east_tail_place(34, 33, 0));
}

/* The test seam RECOMP_CUP_SIZE_TEST: "file:+n,file:-n". */
static void seam(void)
{
    const char *s = "gre_prm:+2,gre_second:+4,jap_j2:-3";
    CHECK(east_seam_delta(s, "gre_prm") == 2, "gre_prm: %d", east_seam_delta(s, "gre_prm"));
    CHECK(east_seam_delta(s, "gre_second") == 4, "gre_second: %d", east_seam_delta(s, "gre_second"));
    CHECK(east_seam_delta(s, "jap_j2") == -3, "jap_j2: %d", east_seam_delta(s, "jap_j2"));
    CHECK(east_seam_delta(s, "jap_j1") == 0, "jap_j1: %d", east_seam_delta(s, "jap_j1"));
    CHECK(east_seam_delta(s, "gre") == 0, "a part of a name: %d", east_seam_delta(s, "gre"));
    CHECK(east_seam_delta(NULL, "gre_prm") == 0, "no seam");
    CHECK(east_seam_delta("kor_league:6", "kor_league") == 6, "no sign: %d", east_seam_delta("kor_league:6", "kor_league"));
}

int main(void)
{
    originals();
    grown();
    shrunk();
    tail();
    seam();
    printf("%s: %d failures\n", fails ? "FAIL" : "OK", fails);
    return fails != 0;
}
