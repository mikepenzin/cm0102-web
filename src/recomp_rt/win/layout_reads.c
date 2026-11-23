/* Reads of the original that depend on where memory lies (0.6.2 open item 138b, tools/recomp/layout_reads.py). The
 * fixes act in every game, the base game too. The test switch RECOMP_LAYOUT_FIXES turns them off: "keep" all of them,
 * "keep:" with a list of groups some of them ("keep:captain,daily"). The groups are captain, players, daily, stats,
 * loans, squads, officials, training, contracts, schedules and fitness. The ctests layout_reads, layout_reads_kept
 * (the match player constructor), layout_reads_names (the group names) and layout_reads_check (the original's code at
 * each site) test them. A save is no test of them: the save holds heap addresses, so two layouts give different saves
 * whatever the game decides. */
#include "win.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CAPTAIN, PLAYERS, DAILY, STATS, LOANS, SQUADS, OFFICIALS, TRAINING, CONTRACTS, SCHEDULES, FITNESS, GROUPS };

static const char *const names[GROUPS] = {"captain", "players", "daily", "stats", "loans", "squads", "officials",
                                           "training", "contracts", "schedules", "fitness"};

uint32_t rt_layout_fix(uint32_t group)
{
    static int on[GROUPS] = {-1};
    if (on[0] < 0) {
        const char *v = getenv("RECOMP_LAYOUT_FIXES");
        if (v && !*v) v = NULL;
        if (v && strcmp(v, "keep") && strncmp(v, "keep:", 5))
            fprintf(stderr, "recomp: RECOMP_LAYOUT_FIXES=%s is not keep or keep:<groups>: every fix acts\n", v);
        for (int g = 0; g < GROUPS; g++) on[g] = !(v && !strcmp(v, "keep"));
        if (v && !strcmp(v, "keep:"))
            fprintf(stderr, "recomp: RECOMP_LAYOUT_FIXES=keep: names no group "
                            "(captain, players, daily, stats, loans, squads, officials, training, "
                            "contracts, schedules, fitness)\n");
        if (v && !strncmp(v, "keep:", 5)) {
            for (const char *p = v + 5; *p; p += strcspn(p, ",") + (p[strcspn(p, ",")] == ',')) {
                size_t n = strcspn(p, ",");
                int known = 0;
                for (int g = 0; g < GROUPS; g++)
                    if (n == strlen(names[g]) && !strncmp(p, names[g], n)) on[g] = 0, known = 1;
                if (n && !known)        /* a typo would leave its fix on, and the check would still pass */
                    fprintf(stderr, "recomp: RECOMP_LAYOUT_FIXES: unknown group %.*s "
                                    "(captain, players, daily, stats, loans, squads, officials, training, "
                                    "contracts, schedules, fitness)\n",
                            (int)n, p);
            }
        }
    }
    return group < GROUPS && on[group];
}

/* RECOMP_LAYOUT_TEST (ctest layout_reads): before the game starts, the original's MATCH_PLAYER::MATCH_PLAYER 0x6eada0
 * makes a match player in two blocks that held other bytes before (0xaa and 0x55, as two heap layouts leave them).
 * The two records must be equal. With RECOMP_LAYOUT_FIXES=keep:players they differ in the bytes that the original
 * does not set (+0x1d to +0x22 among them): the ctest layout_reads_kept expects that failure. Gives the exit code. */
#define MATCH_PLAYER_CTOR 0x6eada0u
#define MATCH_PLAYER_SIZE 0x1beu

int layout_reads_self_test(void)
{
    uint32_t block[2];
    static const uint8_t old[2] = {0xaa, 0x55};
    for (int i = 0; i < 2; i++) {
        block[i] = rt_alloc(MATCH_PLAYER_SIZE);
        memset(g_mem + block[i], old[i], MATCH_PLAYER_SIZE);
        Cpu saved = cpu;
        cpu.ecx = block[i];
        rt_callback(MATCH_PLAYER_CTOR, 0, NULL);
        cpu = saved;
    }
    int differ = 0, first = -1;
    for (uint32_t o = 0; o < MATCH_PLAYER_SIZE; o++)
        if (g_mem[block[0] + o] != g_mem[block[1] + o]) {
            if (first < 0) first = (int)o;
            differ++;
        }
    if (differ)
        fprintf(stderr, "layout test: the match player records differ in %d bytes, the first at +0x%x\n", differ, first);
    else
        fprintf(stderr, "layout test: the match player records are equal\n");
    return differ ? 1 : 0;
}
