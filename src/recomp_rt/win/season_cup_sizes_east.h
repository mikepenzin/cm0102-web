/* The cups of eastern Europe and Asia follow the divisions (0.7.2 league sizes, slice 6, D14): the counts of
 * season_cup_sizes_east.c, with no game memory, so tests/seasons/test_season_cup_sizes_east.c checks them.
 *
 * Each cup keeps its entrant count and its round table. Every club of the cup's divisions plays, and the lower
 * clubs take the places that are left. With the class counts each value is the original's constant. */
#ifndef RECOMP_SEASON_CUP_SIZES_EAST_H
#define RECOMP_SEASON_CUP_SIZES_EAST_H

#include <stdlib.h>
#include <string.h>

/* The lower clubs that a cup picks from its pool: the places less the clubs of its divisions, at most the pool,
 * and at least `least` while the pool holds a club (the copy loops of the original run once before their test,
 * for example 0x8fcd39..0x8fcd60). */
static inline int east_lower_picks(int places, int top, int pool, int least)
{
    int k = places - top;
    if (k < least) k = least;
    if (k > pool) k = pool;
    return k < 0 ? 0 : k;
}

/* The limit of a loop that fills a list before the clubs of the division that goes last: the places less those
 * clubs, not below 0. */
static inline int east_bound(int places, int top)
{
    return places > top ? places - top : 0;
}

/* The clubs that a list of fixed places leaves out. */
static inline int east_skip(int clubs, int places)
{
    return clubs > places ? clubs - places : 0;
}

/* The place of entry i of a list of `count` places whose first `used` places hold clubs, once the clubs move to the
 * last places. */
static inline int east_tail_place(int count, int used, int i)
{
    return count - used + i;
}

/* The change of the clubs of a division that the test seam RECOMP_CUP_SIZE_TEST gives ("gre_prm:+2,jap_j2:-3"):
 * the clubs a cup sees in that division while it collects its entrants. 0 when the seam does not name the file. */
static inline int east_seam_delta(const char *spec, const char *file)
{
    size_t n = file ? strlen(file) : 0;
    for (const char *p = spec; p && *p && n;) {
        if (!strncmp(p, file, n) && p[n] == ':') return atoi(p + n + 1);
        p = strchr(p, ',');
        if (p) p++;
    }
    return 0;
}

#endif
