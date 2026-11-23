/* Club moves at a new game (0.6.0 slice 15, Q225, Q237, Q244, Q279, Q290; spec
 * docs/specs/2026-10-16-seasons-0.6.0-spec.md).
 *
 * The club moves table: Seasons/clubs.tsv of the download (tools/seasons/make_clubs.py makes it from the
 * clubs files of the research, docs/research/rules-0.6.0/<part>-clubs.tsv). One row per real club of a
 * division at the start of a season: the database division (its long name), the club record id and name.
 *
 * At a new game in a season game (season_names.h: start year not 2001, or changing rules on) the runtime
 * puts the clubs of each list in their division, in memory: the division of a club is the pointer at club
 * +0x57 to its club_comp record (resolve_database_indexes 0x5315a0 at 0x531625; the leagues take their clubs
 * by it, for example EngConf::create_table 0x56ecb0 at 0x56ed54..0x56ed64). The database files stay
 * unchanged. It runs once, at the start of database_trim_staff_tables 0x524d70 (called at 0x828389 in
 * create_game 0x828110): after the load (0x8282a7, and after the date shift of slice 11b, season_dates.c, at
 * the end of that load) and before the trims of the staff and club tables (0x524d70, 0x5267c0) and before the
 * competitions are made. A loaded game needs nothing: its save holds the clubs as they were moved.
 *
 * A division keeps the club count that its class takes (CLASS_SIZE in season_clubs.c, with the address of
 * each count), not the count of the database. A class that counts the clubs of the data, or takes groups,
 * keeps the database count. A real count that differs from the class count is slice 18. So:
 *   - a list with more clubs than the class takes keeps the clubs that the database has there first, then
 *     the others in the order of the list;
 *   - a club that the database has in a listed division and that no list keeps there leaves the division
 *     (no division, as the original's clubs with no division; Q290);
 *   - a division with too many clubs loses the last clubs of the club table, as the class would leave them out;
 *   - a division with too few clubs first takes back the clubs that the database had there and that no list
 *     keeps. The place of a listed club that the database lacks (Q359) takes a club of the same nation with no
 *     division. In the top two tiers that place stays open for slice 15b, except for a reserve or B side and in
 *     a class with a fixed count (a stand-in, the lead's interim decision). Other places take the nation's clubs
 *     with no division, then the clubs of the spare divisions "A Lower Division" and "A Premier Division"
 *     (keys 0x9cf764 and 0x9cf6b8), then the clubs of lower divisions (club_comp +0x69, the rank) that no list
 *     keeps (Q244, Q279). The divisions are filled from the highest rank down, so a lower division that gives
 *     a club takes one in its turn. Inside each group the order is that of the club table.
 * A club of a list must have the record id and the name of the row and the nation of the division. Else it is
 * the club of that name in the nation, else the club of the nation with the same plain name (season_clubs.c),
 * else the place is that of a missing club.
 *
 * The test switch RECOMP_SEASON_CODE=off and the base game (2001/02 with changing rules off) move no club.
 * RECOMP_CLUB_LOG=1 writes a line for each list to stderr. The test switch RECOMP_CLUB_MOVES=off moves no club
 * (the names stay). */
#ifndef RECOMP_SEASON_CLUBS_H
#define RECOMP_SEASON_CLUBS_H

#include <stdint.h>

/* Reads the table once at the start: clubs.tsv in the folder of RECOMP_RULES_TABLE (test runs) or
 * Seasons/clubs.tsv of the game folder. Host memory only. Returns the row count, 0 when the file is missing
 * or damaged (no club moves). */
int season_clubs_init(const char *game_dir);
int season_clubs_load(const char *path);

/* 1 for the name of a reserve or B side (a last word B, II, 2 or Reserves, a word such as U21, Jong at the start,
 * or "(reserve team)"). Also used for the Norwegian second teams (season_nor_second.c, open item 111). */
int season_clubs_reserve_name(const char *name);

/* The division (club_comp record) that the club moves of a new game plan for a club (a club record), or its
 * division +0x57 outside the moves. The new-game bars of the reserve-side rules read a first team's division here,
 * as Spain does (season_esp_barred), since the moves write +0x57 only at their end. */
uint32_t season_clubs_planned_division(uint32_t club);
/* The season of the club lists and club records of a division in a start season: an Irish division takes those of
 * its edition's row (item 260, season_sizes_edition), any other division those of the season. */
int season_clubs_list_season(const char *division, int season);

/* 1 when the club sits in a spare division (A Lower Division, A Premier Division: the nation's small clubs) that a
 * fill may take it from: not a Danish club (DenCup's pool). The size step of 0.7.2 slice 3 counts these clubs as
 * clubs with no division. */
int season_clubs_in_spare(uint32_t club);

#endif
