/* The bound of the RusFirst take loop (0.7.2 league sizes, slice 12; season_sizes_rus.h). */
#include <stdio.h>
#include "season_sizes_rus.h"

int main(void)
{
    static const int cases[][2] = {
        {0, 18}, {17, 18}, {18, 18}, {19, 19}, {20, 20}, {22, 22}, {23, 22}, {40, 22},
    };
    int bad = 0;
    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        int got = season_rus_first_take(cases[k][0]);
        if (got != cases[k][1]) {
            printf("FAIL count %d: take %d, want %d\n", cases[k][0], got, cases[k][1]);
            bad++;
        }
    }
    printf("%s season_sizes_rus\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
