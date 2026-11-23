/* The options of the settings file settings.txt in the user folder.
 *
 * The file has one line "name = value" per option, and comment lines that start with "#". The runtime
 * reads every option once at start (options_load). At start it keeps every line as it is, and it adds a
 * line, with one comment line before it, for each option that the file lacks, at the end of the file.
 * A missing file is written with every option at its default. If the file is missing and
 * settings.txt.bak exists (a write that stopped half way), the .bak file is used. A file that exists but
 * cannot be opened is left alone.
 * Later, options_set rewrites the line of one option only. It keeps every other line, comment and
 * line end, and it refuses a file that is not plain text with LF or CRLF line ends.
 *
 * A switch accepts on/off, yes/no and true/false in any case. A value that does not fit the option
 * gets a message on stderr, and the option keeps its default. */
#ifndef RECOMP_OPTIONS_H
#define RECOMP_OPTIONS_H

typedef enum {
    OPT_DISPLAY,               /* enum: widescreen (default), classic; an old hd gives the default */
    OPT_SPACE_TO_CONTINUE,     /* switch, on */
    OPT_SHORTCUTS,             /* switch, on */
    OPT_SHARP_PICTURE,         /* switch, on */
    OPT_SAVE_FOLDER,           /* text, empty: the normal save folder */
    OPT_SOUND,                 /* switch, on (the game's own Sound setting applies as well) */
    OPT_CLICKABLE_NAMES,       /* switch, on */
    OPT_ATTRIBUTE_COLOURS,     /* switch, on */
    OPT_FASTER_DAYS,           /* switch, on */
    OPT_MATCH_COLUMNS,         /* switch, on */
    OPT_REMEMBER_WINDOW,       /* switch, on */
    OPT_SAVE_BACKUPS,          /* switch, on */
    OPT_CONDITION_COLOURS,     /* switch, on */
    OPT_DATABASE,              /* enum: 3.9.68, 3.9.60 (default since 0.6.1), 3.9.65, or folder:<name>: the data set of a new game */
    OPT_SQUAD_VIEWS,           /* switch, on: the Team views screen (views.c) */
    OPT_ATTRIBUTE_GROUPS,      /* switch, on */
    OPT_FOREIGN_COLOURS,       /* switch, on: foreign player colour (foreign.c) */
    OPT_FULL_PLAYER_SEARCH,    /* switch, off: the player search lists every player (playersearch.c) */
    OPT_CHANGING_RULES,        /* switch, on: the default of changing rules for a new game (season.c) */
    OPT_SEASON,                /* enum: 2001/02 (default) to 2026/27: the season of a new game (season_screen.c) */
    OPT_ONLINE_PLAY,           /* switch, on: online games in the game list and a host in the lobby (net_lobby.c) */
    OPT_GAME_PASSWORD,         /* text, empty: the password of a network game that this computer hosts (net_lobby.c) */
    OPT_SQUAD_NUMBERS_99,      /* switch, off: squad numbers up to 99 (squad_numbers.c) */
    OPT_COUNT
} OptionId;

/* Values of OPT_DISPLAY for options_enum. */
enum { DISPLAY_WIDESCREEN = 0, DISPLAY_CLASSIC = 1 };

/* Values of OPT_DATABASE for options_enum. */
enum { DATABASE_3968 = 0, DATABASE_3960 = 1, DATABASE_3965 = 2, DATABASE_CUSTOM = 3 };   /* custom: the value "folder:<name>", a folder in Databases */

/* Reads the settings file at path, or writes it when it is missing, and adds the lines that it lacks.
 * Call it once, at start. Before it, and when the file cannot be read, every getter gives the default. */
void options_load(const char *path);

/* The value of a switch option: 1 or 0. */
int options_bool(OptionId id);
/* The value of a text option. Never NULL. The text has no spaces at its ends. */
const char *options_string(OptionId id);
/* The value of an enum option: the index in its list of values. */
int options_enum(OptionId id);

/* Sets option id to value, for a switch (on/off, yes/no, true/false), a choice (one of its values, or
 * folder:<name> for database) or a text. The running game reads the new value at once through the getters.
 * The settings file gets it too: the option's line is replaced, or added with its comment when it is
 * missing. Every other line, comment, line end and unknown line stays. The file is written to
 * settings.txt.tmp and moved over the old file, so a failed write leaves the old file whole.
 * Returns 1 when the value is set and the file is written, 0 when the value does not fit the option (nothing
 * changes), and -1 when the value is set in memory but the file could not be written (a message is printed). */
int options_set(OptionId id, const char *value);

/* options_set for an enum option only: returns 0 for a switch or a text. */
int options_set_enum(OptionId id, const char *value);

/* The name of an option in the file, for messages. */
const char *options_name(OptionId id);

#endif
