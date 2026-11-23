/* League sizes, promotion and relegation in existing divisions (0.6.0 slice 18, spec
 * docs/specs/2026-10-16-seasons-0.6.0-spec.md, Q247, Q272, Q281). Research and evidence:
 * docs/research/2026-10-18-league-sizes.md.
 *
 * Only a season game (season_game_changes, slice 15) gets any of it. The base game and RECOMP_SEASON_CODE=off
 * keep the original's counts.
 *
 * The rules of a league in a season: the row of the rules table (season_rules.h) of its source file. A new
 * game, and every season of a game with changing rules off, takes the row of the start season (Q232). With
 * changing rules on a later season takes the row of the edition that the object holds (+0x40): the row whose
 * real season begins with that year (a calendar-year league holds edition Y + 1 in the row of season Y), else
 * the row of that season. After the last row the last row stays (Q248).
 *
 * - Club counts: a class holds no more clubs than its largest constant (its table and date template are made
 *   for that count, tools/recomp/league_sizes.py CAPS), and no fewer than its own count where a cup of the
 *   nation needs the whole division (FLOORS, the cups are slice 21). The club moves of a new game
 *   (season_clubs.c) fill each division of tools/recomp/league_sizes.py to the count of its start season row. create_table then takes the clubs that the division holds
 *   (rt_league_clubs, the patched stores of league_sizes.py). In later seasons the game's own results decide who
 *   moves (Q247), so a division holds the count that promotion and relegation give it. With a new count the
 *   matches per club (+0xa7) go back to -1, so League::check_complete 0x689ad0 counts them again (most classes
 *   keep the first season's value, 0x689b1d..0x689b36). The French classes get a date template of the rounds
 *   of their count (rt_league_dates).
 * - Promotion and relegation places: after each set_up_rules (season_size_wraps.c) the places of the row. A
 *   row's count can hold the winner of a play-off that the class plays (the rules report reads it the same way),
 *   so the direct places are the count less one when the class has play-off places.
 * - league_promote_and_relegate 0x689c80: when the places of two size-table divisions differ, the clubs move on
 *   their own only with changing rules on and when both divisions stay within their floor and cap, else in pairs
 *   (the sizes stay). An expected count of the caller that differs from the moves is dropped (Serie A and B pass
 *   4 and 4 at 0x642476).
 * - News: one sentence for each count or place that the game changes in a season (season_change.h). */
#ifndef RECOMP_SEASON_SIZES_H
#define RECOMP_SEASON_SIZES_H

#include <stdint.h>

/* The club count that create_table of a league stores (+0x3e): original in the base game, else the clubs that
 * the division holds (rt.h declares it for the translated code). */
uint32_t rt_league_clubs(uint32_t obj, uint32_t original);
uint32_t rt_league_dates(uint32_t obj, uint32_t original);

/* After set_up_rules of a league class: the promotion and relegation places of the season. */
void season_sizes_rules(uint32_t obj);

/* 1 in a season game with the size engine on (RECOMP_LEAGUE_SIZES=off turns it off). */
int season_sizes_on(void);

/* The most earlier starts of 7 days of a file's date template: 13 for the Brazilian Série A and B, else
 * SEASON_TEMPLATE_EARLIER (season_sizes_bra.c). */
int season_sizes_bra_earlier(const char *file);

/* After the places of season_sizes_rules, with the size engine on: Série A and B with the row's two meetings play
 * no play-off (0.7.2 league sizes, slice 17, D11; season_sizes_bra.c). */
void season_sizes_bra_rules(uint32_t obj, const char *file);

/* The meetings that the table of a league plays in a season, from the row's meetings: twice them for a row of halves
 * whose table holds both halves (J1, and the K League stages as one table; 0.7.2 league sizes, slice 14,
 * season_sizes_asia.c). Any other file: the row's meetings. */
int season_sizes_asia_meetings(const char *file, int season, int meetings);

/* The most earlier starts of 7 days of the K League's date template (17, season_sizes_asia_rows.c); 0 for any other
 * file. */
int season_sizes_asia_earlier(const char *file);

/* After the places of season_sizes_rules, with the size engine on: J1 plays its halves and final only in a season of
 * halves, else one table (slice 14, D8; season_sizes_asia.c). */
void season_sizes_asia_rules(uint32_t obj, const char *file);

/* The places of Japan and Finland that the rows decide (0.7.3 slice 4; season_sizes_places.c reads only the tables).
 * season_places_jap_cross: 1 when the rows of the season give J1's 16th and J2's 3rd one tie between the divisions.
 * season_places_jap_grows: 1 when J1 grows by exactly the winner of the four-club J2 play-off above `direct` places.
 * season_places_jap_j2_down: the clubs that J2 sends to no division after an edition before its count changes.
 * season_places_fin_no_playoff: 1 when the Veikkausliiga's row of the edition's own real season has no play-off. */
int season_places_jap_cross(int season);
int season_places_jap_grows(int season, int direct);
int season_places_jap_j2_down(int edition);
int season_places_fin_no_playoff(int edition);

/* After the class's rules of the Veikkausliiga: no relegation play-off place in a season with none (season_sizes_fin.c). */
void season_sizes_fin_rules(uint32_t obj, const char *file);

/* The season of the rules row of a league's edition (+0x40) of a year: the row whose real season begins with the
 * year (row Y first, then row Y - 1, the row of a calendar-year league), else the year. */
int season_sizes_edition(const char *file, int year);

/* The club count of a division of a league class in the row of the edition of a season (the club moves of a new
 * game; a calendar-year league takes the row whose real season is that year), or -1 when the class is not in the
 * size table or the row holds no usable count. */
int season_sizes_clubs(const char *file, int season);

/* The Croatian South (season_sizes_cro.c, open item 255). In a season of one group it is not held: the before hook
 * of its season start keeps it idle (1: skip the start). */
void season_sizes_cro_rules(uint32_t obj, const char *file);
void season_sizes_cro_loaded(void);
int season_sizes_cro_behind(uint32_t comp, int year);
uint32_t season_sizes_gre_lower_record(void);
int season_sizes_gre_lower(uint32_t record);
int season_sizes_cro_south_before(uint32_t comp, int season);
void season_sizes_cro_new_game(int start);
int season_sizes_cro_empty(uint32_t obj, const char *file, int season);

/* The source files of the league classes of tools/recomp/league_sizes.py (season_size_wraps.c). */
extern const char *const season_size_files[];
extern const int season_size_file_count;
extern const int season_size_caps[];         /* the largest count of each file, 0: the class counts its data */
extern const int season_size_floors[];       /* the smallest count, 0: none (a cup of the nation needs it) */
extern const int season_size_fixed[];        /* the count whose exchange stays the class's, 0: none (FIXED_EXCHANGE) */

/* 1 in a season game with the sizes on (not RECOMP_LEAGUE_SIZES=off): the cups of season_cup_sizes_uk.c. */
int season_sizes_on(void);

/* 1 for League Two (eng_third): it takes the relegation places of its row towards the Conference, whether the game
 * plays the Conference or not; the wrap of EngPrm::exchange_conference_clubs_unplayed 0x574e60 moves up as many
 * Conference clubs (season_sizes_eng.c, 0.7.2 league sizes slice 8, D15). */
int season_sizes_eng_below(const char *file);

/* The grown classes of tools/recomp/league_sizes.py GROWN (0.7.2 league sizes, slice 2, D21; season_size_wraps.c):
 * create_table allocates the rows of the season's count up to the cap (rt_league_table_bytes), and the wrap of
 * create_schedule gives the date template exactly the rounds of the count (season_template.h, D4 and D5). */
typedef struct {
    const char *file;
    int held;               /* the rows that the class allocates (its largest original count) */
    int cap;                /* the largest count of its rows */
    int mode;               /* SEASON_TEMPLATE_SHUFFLED, _SEQUENTIAL or _MIRRORED (league_next_schedule_round 0x68a030), or
                             * _HALVES (the class makes its own templates, season_sizes_arg.h) */
} SeasonGrown;
extern const SeasonGrown season_grown[];
extern const int season_grown_count;

/* 1 when a division of file's class plays with fewer clubs than its row (a size-table class with a cap and no floor,
 * with the sizes on): the club moves of a new game then take no reserve side for it (D12). */
int season_sizes_plays_short(const char *file);

/* The row's count of a season for a grown class that counts the clubs of its data (league_sizes.py DATA_COUNT), and
 * for ire_prm when IrePrm takes it (item 260), for the club moves of a new game; -1 for any other class or with the
 * sizes off (0.7.2 slice 13). */
int season_sizes_data_clubs(const char *file, int season);

/* The relegation places ("down") of the row of the object's edition, the direct places of the research; -1 with the
 * sizes off or no count (gap 43, season_sizes_irl.c). */
int season_sizes_row_down(uint32_t obj, const char *file);

/* The promotion places ("up") of the row of the object's edition, the direct places of the research, when that row's
 * real season begins in the edition's year; -1 with the sizes off, no count or a row of another real season (item
 * 251b, season_sizes_irl.c). */
int season_sizes_row_up(uint32_t obj, const char *file);

/* The play-off code of the row of the object's edition ("none", "text", ...) when that row's real season begins in the
 * edition's year; NULL with the sizes off, no row, or a row of another real season (edition 2003 plays row 2003,
 * whose real season is 2004). */
const char *season_sizes_row_playoffs(uint32_t obj, const char *file);

/* Item 251 (season_sizes_irl.c): in the set_up_rules wrap of IrePrm and IreFirst, the season's play-off between the
 * divisions; and 1 when the create_table of an Irish class takes n clubs. */
void season_sizes_irl_rules(uint32_t obj, const char *file);
int season_sizes_irl_takes(const char *file, int n);

/* Items 271 and 272 (season_sizes_nir.c): in the set_up_rules wrap of NirPrm and NirFirst, the places of the row and
 * the season's play-off; the record of the lower division of the nation of a lowest division (the Northern Irish lower
 * division for nir_first), or 0; and 1 when a record is such a lower division. Only with the sizes on. */
void season_sizes_nir_rules(uint32_t obj, const char *file);
uint32_t season_sizes_nation_lower(const char *file);
int season_sizes_in_nation_lower(uint32_t record);
/* The relegation places that the size step will use for a lowest division with a nation's lower division: the row's
 * places as far as that division's clubs can replace them (as a lone division), -1 with changing rules off. */
int season_sizes_lower_shown(uint32_t obj, const char *file);

/* After create_schedule of a grown class (thiscall: stage, &count, &code, &flag): for stage -1 in a season game,
 * the meetings of the row (+0x3c), the matches per club (+0xa7) and a template of the rounds of the count, which
 * replaces the class's (the pointer in eax, the count at count_at). */
void season_sizes_schedule(uint32_t obj, int stage, uint32_t count_at);

/* 1 when the date template of a grown league object this edition is the engine's (a season game with the sizes on,
 * and the template had room for the count): its +0x3c then holds the meetings of the row (slice 13: IreFirst). */
int season_sizes_planned(uint32_t obj);

/* The meetings of a class that sets them by its count in create_table (slice 13, season_sizes_irl.c: IreFirst 4 for 9
 * or 10 clubs, else 3, 0x635983..0x635997) for a row with no meetings value (row_meetings 0), or 0: the engine's
 * meetings stay. */
int season_sizes_irl_class_meetings(const char *file, int n, int row_meetings);

/* The season of the rules row of a league object this edition (the row of the start season with changing rules off),
 * or -1. */
int season_sizes_row_season(uint32_t obj, const char *file);

/* The dates of a split league after its split (slice 7, season_split.c): create_schedule of the object for stage -1
 * with the row's meetings, whose wrap plans the season again and gives the dates after the table's rounds in a new
 * block (count to *count, the class's name code to *code). 0 when the plan gives none. */
uint32_t season_sizes_split_dates(uint32_t obj, int meetings, int *count, uint32_t *code);

/* The periods of the Eerste Divisie and the Belgian Second Division (slice 10, D6, season_sizes_periods.c): before
 * the class's update_positions_and_status (vtable +0x94), a row with no periods sets every period flag, so no club
 * wins a period; the target of check_complete for a period stage of an odd count (0 when its matches are played). */
void season_periods_winners(uint32_t obj);
uint32_t season_periods_played_target(uint32_t obj, uint32_t original);

/* The row of the Belgian Second Division that the third tier's promotion play-off takes: the count less 3, row 15 in
 * the base game (slice 10, season_sizes_bel.c). */
int season_sizes_bel_third_row(uint32_t second);

/* A template of `rounds` dates from the class's `have` dates at old (D4, D5; season_template_plan in `mode`), in a new
 * block of the game's heap; 0 when the plan has no room (the log says why). It does not free old. */
uint32_t season_sizes_build_template(uint32_t obj, const char *file, uint32_t old, int have, int rounds, int mode);

/* The club count of a grown league this edition (the clubs of its division up to its cap), 0 outside a season game
 * with the sizes on. */
int season_sizes_count(uint32_t obj);
/* 1 when a human manages the club. */
int season_sizes_is_human(uint32_t club);

/* The objects whose template had no room this edition (D4) keep the class's count and form: on 1 marks the object,
 * 0 clears it; season_sizes_kept gives the mark of the object's edition. */
void season_sizes_keep(uint32_t obj, int on);
int season_sizes_kept(uint32_t obj);

/* The game's malloc 0x944e46 (cleared) and free 0x9452ca. */
uint32_t season_sizes_game_malloc(uint32_t size);
void season_sizes_game_free(uint32_t p);

/* The size step of a division pair that a class's own chain moves (not league_promote_and_relegate 0x689c80): the
 * Argentine Primera and second division (ArgPrm::process_promotion_relegation_chain 0x4067f0). */
void season_sizes_step_note(uint32_t upper, uint32_t lower);
/* Item 286: the size step of this season change takes this club into no division: it has just gone down. */
void season_sizes_step_bar(uint32_t club);

/* The name of a period stage id that the original does not know (gap 33, season_sizes_periods.c), or NULL. */
const char *season_sizes_period_name(int id, int abbreviated);

#endif
