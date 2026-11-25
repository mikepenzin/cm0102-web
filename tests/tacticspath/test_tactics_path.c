/* Unit check of tactics_path_of_other_machine (src/recomp_rt/win/tactics_path.h, used by win_host_path in files.c):
 * a saved game keeps the tactics folder of the computer where the game began (a human manager's tactic slot +0x18F1,
 * from FUN_0090bea0("tactics") at the start, docs/research/2026-10-16-tactics-inventory.md), and Save Formation writes
 * there (Formation::save_to_disk 0x5b6900). Such a path names the game folder's tactics folder here. */
#include "tactics_path.h"

#include <stdio.h>
#include <string.h>

static int failed;

static void check(const char *in, const char *want)
{
    char out[256];
    memset(out, 'x', sizeof out);
    int got = tactics_path_of_other_machine(in, 'D', out, sizeof out);
    if (want ? !got || strcmp(out, want) : got) {
        printf("FAIL %s: got %s\"%s\", want %s\"%s\"\n", in, got ? "" : "none ", got ? out : "", want ? "" : "none ", want ? want : "");
        failed = 1;
    }
}

int main(void)
{
    check("E:\\tactics\\matt.tct", "D:\\tactics\\matt.tct");                            /* the Bosman Edition on a PC */
    check("E:\\tactics", "D:\\tactics");                                                /* the folder itself (Load) */
    check("E:\\tactics\\*.tct", "D:\\tactics\\*.tct");                                  /* a search of it */
    check("C:\\Program Files (x86)\\Championship Manager 01-02\\tactics\\4-4-2X.tct",   /* the original game */
          "D:\\tactics\\4-4-2X.tct");
    check("C:\\GAMES\\CM0102\\TACTICS\\a.pct", "D:\\TACTICS\\a.pct");                   /* any case */
    check("D:\\tactics\\matt.tct", NULL);                                               /* the game folder's own */
    check("E:\\Data\\staff.dat", NULL);                                                 /* not tactics */
    check("E:\\tactics\\deeper\\a.tct", NULL);                                          /* a folder below it */
    check("E:\\mytactics\\a.tct", NULL);
    check("E:\\", NULL);
    {
        char small[8];
        if (tactics_path_of_other_machine("E:\\tactics\\long-name.tct", 'D', small, sizeof small)) {
            printf("FAIL a path that does not fit\n");
            failed = 1;
        }
    }
    printf(failed ? "FAILED\n" : "PASS\n");
    return failed;
}
