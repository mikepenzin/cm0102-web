/* The Choose Season screen and the season lines of Setup Game (0.6.0 slice 13, season_screen.c). */
#ifndef RECOMP_SEASON_SCREEN_H
#define RECOMP_SEASON_SCREEN_H

#include <stddef.h>
#include <stdint.h>

/* Pushes the Choose Season screen over Setup Game. */
void season_screen_open(void);

/* The build function of the screen, for the wait state "seasons" (waits.c). */
uint32_t season_screen_build_fn(void);

/* The widget draw wrap (names.c, through views_widget_drawn) calls this after the game draws widget w. */
void season_screen_widget_drawn(uint32_t w);

/* The start year of the next new game from the options database and season: 3.9.60 and 3.9.65 are 2001/02,
 * 3.9.68 is 2001/02 or 2002/03 (season), a known database of Databases is its own season and a Custom
 * database is the season that the player named (season). A missing folder is 2001/02, as files.c then loads
 * 3.9.68. It hashes a folder only the first time (seasons_db_identify). */
int season_screen_year(void);

/* The name of the database of the next new game, for Setup Game: "3.9.68 data", "October 2003" or the name
 * of a Custom database folder. */
void season_screen_database_text(char *out, size_t room);

/* The credit line of a database by its name (Q147, Q269). */
const char *season_screen_credit(const char *name);

/* The changing rules switch of the next new game, on the Choose Season screen. choice is -1 until the player
 * clicks it: then the option changing_rules applies. on gives the value that the screen and the top row
 * of Setup Game show. */
int season_screen_rules_choice(void);
int season_screen_rules_on(void);
void season_screen_set_rules(int on);           /* the right button of Setup Game's top row (database.c) */

/* The widget draw wrap (names.c, through views_widget_drawn) calls this after the game draws widget w: the
 * value of the changing rules button of Setup Game (database.c). */
void setup_widget_drawn(uint32_t w);

/* Start New Game (season_dates.c) calls this first: a data set and season that the page chose (the browser
 * build's rt_web_choose_data_set) move the game date there, inside the game's turn. Nothing without a choice. */
void season_screen_apply_page_choice(void);

/* The build function of the Extras screen of Setup Game (database.c), for the wait state "extras". */
uint32_t setup_extras_build_fn(void);

#endif
