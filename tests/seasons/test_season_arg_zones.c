/* The zone fill table of the Argentine clubs (season_arg_zones.c, O259 net B2). */
#include "season_arg_zones.h"

#include <stdio.h>

static int fails;

static void expect(int season, int id, const char *name, char want)
{
    char got = season_arg_zone_of(season, id, name);
    if (got != want) {
        printf("FAIL  %d %d %s: got %c, want %c\n", season, id, name, got ? got : '-', want ? want : '-');
        fails++;
    }
}

int main(void)
{
    expect(2009, 11838, "Club Atl\xe9tico Boca Unidos", 'I');       /* September 2009 and 2010: no zone record */
    expect(2010, 11838, "Club Atl\xe9tico Boca Unidos", 'I');
    expect(2012, 5886, "Club Atletico Crucero del Norte", 'I');
    expect(2014, 4397, "Gimnasia y Esgrima de Mendoza", 'I');
    expect(2022, 9267, "Racing de C\xf3rdoba", 'I');
    expect(2011, 11838, "Club Atl\xe9tico Boca Unidos", 0);          /* the 2011 database holds the zone */
    expect(2009, 11838, "Another Club", 0);                          /* the name must match */
    expect(2009, 1, "Club Atl\xe9tico Boca Unidos", 0);              /* the id must match */
    if (season_arg_zone_count < 50) {
        printf("FAIL  %d rows\n", season_arg_zone_count);
        fails++;
    }
    printf(fails ? "FAILED: %d\n" : "PASS\n", fails);
    return fails != 0;
}
