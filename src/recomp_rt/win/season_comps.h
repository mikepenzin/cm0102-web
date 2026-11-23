/* New competitions (0.6.0, shared by slices 20, 21, 24, 26, 28 and 29; owner slice 26). Design and evidence:
 * docs/research/2026-10-18-new-comps.md.
 *
 * A new competition is one that the original has no object for (the Copa Sudamericana, the Conference League,
 * the 3. Liga, the Nations League). This module makes its object, puts it into its league table entry and keeps
 * it across a save and a load. A slice adds one SeasonCompDef line per competition to season_comp_defs in
 * season_comps.c, in its own small block.
 *
 * The shared rule for a season with no edition (lead, 2026-10-18), for the original's competitions and the new
 * ones alike. Never remove an object: other code reads it with no null test (0x4c41a0..0x4c41d0, 0x56fb08).
 *   - A competition that comes back after a gap waits for its next edition: in the before hook of
 *     start_new_season set its season year +0x40 to (next edition year - the class's year step), and at a new
 *     game give the constructor that year. Its dates then lie in that year, it is not complete, the pass does not
 *     start it again (0x5d5ccb), and it draws on its start date. Example: season_kind_club_world.c.
 *   - Only a competition that never comes back uses season_change_skip() in a before hook.
 *   - Third case (slice 21b, accepted by the lead): a competition with a gap in the middle of a game whose
 *     start_new_season adds 1 to +0x40 and then builds the dates, the entrants and (for some League classes) the
 *     fixtures of the new edition may call season_change_skip() in each season of the gap and, in the before
 *     hook of the season S of its return, set +0x40 to S - 1, so start_new_season builds edition S with new
 *     dates. Evidence: FraLgeCup::start_new_season 0x5bd4be (+0x40 + 1) before set_up_rounds (vtable +0x8c at
 *     0x5bd50d) and the entrant pick 0x5bd4d0; WelPrmCup::start_new_season 0x928261, then League::create_fixtures
 *     at 0x9282f8. A wait would make those fixtures and entrants a season or more early. Example and the list of
 *     classes: season_kind_cup_breaks.c, docs/research/2026-10-18-cup-breaks.md, section 1.
 *   - With changing rules off, the rules of the start season stay (Q232): a competition that the start season
 *     does not hold never comes, one that it holds plays on.
 * Why: a skipped competition keeps the start date of its old year. A League class draws only when today equals
 * that start date (WorldClubChamp::update_for_date 0x92a73d..0x92a74f, Copa Mercosur 0x4c5d2c..0x4c5d3d,
 * Merconorte 0x4c2ce5..0x4c2cf6), so after a skip and a later start_new_season it never draws again; a Cup class
 * draws its past rounds at once (0x51fa45..0x51fa7d). Evidence: docs/research/2026-10-18-club-world-idle.md,
 * section 1.
 *
 * When an object is made (the module, not the slice, decides it): once per game, in the entry hook of its
 * entry (season_change.h) in the first season S from the start season on that the table holds it, with
 * season_change_add_comp. The registry runs that hook
 *   - for the start season with changing rules on or off: in the run to the start date of a new game for an
 *     entry with its update day before the start date, on the entry's update day in the game for one after
 *     it, at the end of the new game for one from 1 October (season_change.h, "The start season");
 *   - for a later season only with changing rules on. So with changing rules off a competition that starts
 *     after the start season never comes (Q232);
 *   - never in the base game (start 2001, changing rules off) or with RECOMP_SEASON_CODE=off (Q152, Q342).
 * The entry must exist in the game: a national entry needs its nation selected (+0x11c, the registry does not
 * test it for this hook); a continental entry needs at least one object of the original in its array (the
 * creators make none when no nation of the continent is selected, 0x6699d0).
 * A season that the table does not hold after the first edition is the slice's matter: its before hook keeps
 * the object waiting or calls season_change_skip().
 *
 * The record. Each new competition needs a competition record (0x6b bytes in the array at [0xae23d0]) for its
 * name, history and [0xadadfc] slot. The def names a record that the data holds and no object of the original
 * uses (for example "South American CONMEBOL Cup", record 60 in every Update Team database), and the module
 * gives it the competition's name through season_change_rename. Or the def gives a record that the data lacks
 * (field `like` set): the module then adds a new record with that long name at the database load of a new game
 * (docs/research/2026-10-18-new-comp-records.md). The new records go after the last nation competition and
 * before the 0x7f friendly records, so no index that game data holds moves; a save holds them in its
 * nation_comp.dat part, so a load needs no change. The module adds one only for a def that the game can make
 * (a season game whose start season, or with changing rules on a later season, holds the competition).
 *
 * Save and load (docs/research/2026-10-18-new-comps-load.md). The object saves itself like every object of an
 * entry array (key_nation_save_all_comps 0x6692f0 calls vtable +0x1c; the save packs every *.tmp file of
 * CM3_TEMP, 0x933610), to CM3_TEMP\<record name>_<record id>.tmp. A load makes only the original's objects
 * (the creators, key_nation_create_all_comps 0x669180 at 0x82c5aa). So the module wraps build_comp_object_table
 * 0x8386f0: at a load (call at 0x82c5f6), before the original fills [0xadadfc], it makes each def's object again
 * whose file is in CM3_TEMP and adds it to its entry. The constructor reads its state from the file itself
 * (Conmebol_Merc 0x4c471f, World_Club_Cup 0x92b52f), year and record included. The save part holds nothing for
 * this module: the file is the record that the object exists.
 *
 * Classes. A def's make function takes an original class (operator new with the class size, then the
 * constructor, thiscall (year, record), ret 8) and, where the class reads key id globals or fixed lists for its
 * clubs, a vtable copy with the club picker slot changed (season_comps_vtable). Only Conmebol_Liber takes its
 * clubs by its own record (club +0x1db, 0x4c3fb7); every other class picks the same clubs as the original's
 * object (doc section 5). */
#ifndef RECOMP_SEASON_COMPS_H
#define RECOMP_SEASON_COMPS_H

#include <stdint.h>

#include "season_change.h"

typedef struct SeasonCompDef SeasonCompDef;

struct SeasonCompDef {
    const char *comp;          /* the rules table `comp` of its rows (comp_file "new"): "sudamericana" */
    const char *file;          /* its name in the rules report and the logs: "new_sudamericana" */
    const char *record;        /* the long name in the data of the record it takes (see above) */
    int entry;                 /* the league table entry whose array holds it, 0 to 34 */
    /* Makes the object in the game's heap for season year `year` with the record at `rec` (the address in the
     * array at [0xae23d0]) and returns it, or 0. `loading` is 1 at a load: the object then reads its state
     * from its saved file, as the original's constructors do. Called only by the module. */
    uint32_t (*make)(const SeasonCompDef *d, uint32_t rec, int year, int loading);
    /* A new record when the data has no record `record` (NULL: the def takes a record of the data). The new
     * record copies the continent, nation and colour ids (+0x59, +0x5d, +0x61, +0x65) of the data record `like`
     * (for example "UEFA Cup") and takes `short_name` (+0x38, at most 26 characters) and `level` (+0x69, the
     * level of the competition: UEFA Cup 18, Inter-Toto Cup 8; 0x79d447). */
    const char *like;
    const char *short_name;
    int level;
    /* Slice 28: 1 when an edition starts before the update day of its entry in the season that the table gives
     * it (the Gold Cup of July 2021 in 2021/22, entry 1 on 15 August): the object is also made in the season
     * before a season that holds the competition. Only with changing rules on: with them off a competition that
     * the start season does not hold stays off (Q398). */
    int early;
    /* Slice 21c: the rule of season S of the entry hook, 1 held, 0 not held, -1 no row; NULL: the table by `comp`
     * (season_comps_held). A cup whose editions do not map one to one to the rows (a calendar-year cup in the row
     * before, or one that the game cannot play in some seasons) gives its own. */
    int (*held)(const SeasonCompDef *d, int season);
};

/* The competitions. Each slice adds its lines in season_comps.c. */
extern const SeasonCompDef *const season_comp_defs[];
extern const int season_comp_def_count;

/* 1 in a season game (see above). */
int season_comps_on(void);
/* The comp records that the module added to the data (they come last, before the friendly records). */
int season_comps_added_records(void);

/* The rules of a competition of the table by its `comp` (any comp_file): 1 when the table holds it in season s,
 * 0 when it is not held, -1 with no row. After the last row the rule stays (Q248). */
int season_comps_held(const char *comp, int s);

/* The first season from s on that the table holds the competition, 2999 when none, 0 with no row. */
int season_comps_next_edition(const char *comp, int s);

/* The season whose change made the object (the entry hook, which writes "The X is played for the first time."
 * or "The X is played again."),
 * or 0: not a def's object, or made at a load or before this run's season changes (0.6.2 item 135). */
int season_comps_made_in(uint32_t comp);

/* The object of a def in the game that runs, or 0 (not made, or the game has none). */
uint32_t season_comps_object(const SeasonCompDef *d);
/* The record of a def's competition: its object's, or at a load before the objects exist, the record whose saved
 * file the load extracted (0.6.2 open item 91); 0 when the game has none. */
uint32_t season_comps_record_of(const SeasonCompDef *d);

/* Slice 24: the file of the def whose record the object has (its long name in the data), or NULL. A new
 * competition can take a class of the original, so season_change_file asks here first: the class would give
 * the file of the original's competition and its rules (the names kind would rename its record). */
const char *season_comps_file(uint32_t obj);

/* The club of the highest reputation (club +0x80) of a continent (key id global, for example 0x9cfa0c for Asia)
 * that is not in skip[0..n), or 0. The walk of WorldClubChamp::pick_entrant 0x92afbd..0x92aff4: [0xae235c] -
 * 2 x [0xae2350] clubs of the club table [0xae23b4] (so no national team), club +0x53 the nation, nation +0x71 the
 * continent, continent +0 its id. No random number: the original's pick takes a short list of about 6 clubs and
 * shows an error box when all are picked (0x92b0fd). */
uint32_t season_comps_top_club(uint32_t continent_key, const uint32_t *skip, int n);

/* 1 when rec (a record address) is the record of a def of this game: a slice's constructor wrap then leaves the
 * object's year alone (season_kind_club_world.c, the new cups on the World_Club_Cup class). */
int season_comps_is_def_record(uint32_t rec);

/* The address of a competition record by its long name in the data (the name before any season_change_rename),
 * or 0. */
uint32_t season_comps_record(const char *name);

/* The copy of a class's vtable in runtime memory for def d, so a new competition can take an original class and
 * change some slots. One copy per def for the whole run: each call fills it again from vtable and gives the same
 * address. slots: the count to copy (Cup classes 40, League classes 45, sweep-europe 2.2). Returns the address
 * of the copy, or 0. */
uint32_t season_comps_vtable(const SeasonCompDef *d, uint32_t vtable, int slots);

/* Slice 21c: the def's own copy of a class vtable, made once per run (the same address for every game), so that
 * season_change_file and the rules report know the object by its vtable: season_comps_file_of gives the def's file
 * and season_comps_base_vtable the class's vtable. */
uint32_t season_comps_def_vtable(const SeasonCompDef *d, uint32_t vtable, int slots);

/* The file of a def for an object with a def's vtable copy, or NULL. */
const char *season_comps_file_of(uint32_t obj);

/* The class vtable of a def's vtable copy, or 0. */
uint32_t season_comps_base_vtable(uint32_t vtable);

/* 1 when the def holds its competition in season s (its `held`, else the table by comp), 0 not, -1 no row. */
int season_comps_def_held(const SeasonCompDef *d, int s);

/* Slice 28: the class of a vtable copy (the original class and kind, the def's file), or NULL for another
 * vtable. The rules report and season_change_file use it, so a new competition shows its def's file. */
#include "season_rules.h"
const SeasonCompClass *season_comps_class(uint32_t vtable);

/* Points slot `slot` of a vtable copy at a runtime function (rt_host_fn gives it a game address). */
void season_comps_set_slot(uint32_t vtable, int slot, const char *name, void (*fn)(void));

/* The rule kind "comps" (entry and loaded hooks). season_change.c lists it first in season_rule_kinds, so a
 * slice's own kind meets the object in the same season. */
extern const SeasonRuleKind season_kind_comps;

/* 1 when the records that the module added at the last database load of a new game differ from those that the next
 * new game needs (another season or changing rules): Start New Game then loads the data again (season_dates.c). */
int season_comps_records_stale(void);

/* season_change_reset calls it: before a new game, a load and the end of a game. */
void season_comps_reset(void);

/* The load step of build_comp_object_table 0x8386f0 (each def's object made again from its save file). The one wrap
 * of 0x8386f0 is in season_names.c and calls this before the original. */
void season_comps_before_comp_table(void);

#endif
