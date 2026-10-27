/* The zone fill table of the Argentine clubs (0.6.0, O259 net B2, O375; tools/seasons/make_arg_zones.py makes
 * season_arg_zones_table.c from docs/research/arg-zones-0.6.0/zones.tsv).
 *
 * ArgSecond::fill_league_table 0x407cd0 counts a club of the second division only when it has a zone record (club
 * +0x60, 0x407d7f). A season database may hold a club of the real division with none (club.dat +0x60 = -1); a
 * later database gives it Argentine Metropolitan Zone or Argentine Interior Zone. A row is the club in the database
 * of one season: the season, the record id, the long name there and the zone letter M or I. */
#ifndef RECOMP_SEASON_ARG_ZONES_H
#define RECOMP_SEASON_ARG_ZONES_H

typedef struct {
    int season, id;
    const char *name;
    char zone;
} SeasonArgZone;

extern const SeasonArgZone season_arg_zones[];
extern const int season_arg_zone_count;

/* The zone letter ('M' or 'I') of the club with this record id and long name in the database of the season, or 0.
 * No game memory: the ctest calls it. */
char season_arg_zone_of(int season, int id, const char *name);

#endif
