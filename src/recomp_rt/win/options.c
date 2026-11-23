/* The reader of the settings file (options.h). */
#include "options.h"
#include "../host.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { KIND_SWITCH, KIND_TEXT, KIND_ENUM } Kind;

typedef struct {
    const char *name;
    Kind kind;
    const char *def;           /* the default, as written in the file */
    const char *values;        /* KIND_ENUM: the values, separated by "|" */
    const char *comment;       /* the one comment line before the option in the file */
} OptionDef;

static const OptionDef defs[OPT_COUNT] = {
    [OPT_DISPLAY] = {"display", KIND_ENUM, "widescreen", "widescreen|classic",
        "display = widescreen draws the game at 1280x800. display = classic draws it at 800x600."},
    [OPT_SPACE_TO_CONTINUE] = {"space_to_continue", KIND_SWITCH, "on", NULL,
        "space_to_continue = on lets the space bar press Continue Game."},
    [OPT_SHORTCUTS] = {"shortcuts", KIND_SWITCH, "on", NULL,
        "shortcuts = on adds keys for Back, for closing a popup and for full screen."},
    [OPT_SHARP_PICTURE] = {"sharp_picture", KIND_SWITCH, "on", NULL,
        "sharp_picture = on keeps text crisp and even when the picture fills the window. off gives a smoother, softer picture."},
    [OPT_SAVE_FOLDER] = {"save_folder", KIND_TEXT, "", NULL,
        "save_folder = the folder for saved games, for example in iCloud Drive. Empty uses the normal folder."},
    [OPT_SOUND] = {"sound", KIND_SWITCH, "on", NULL,
        "sound = off mutes the game. With sound = on, the game's own Sound setting still applies."},
    [OPT_CLICKABLE_NAMES] = {"clickable_names", KIND_SWITCH, "on", NULL,
        "clickable_names = on lets you click a player or club name to open its profile."},
    [OPT_ATTRIBUTE_COLOURS] = {"attribute_colours", KIND_SWITCH, "on", NULL,
        "attribute_colours = on colours attribute numbers by value."},
    [OPT_FASTER_DAYS] = {"faster_days", KIND_SWITCH, "on", NULL,
        "faster_days = on makes days pass faster while you are on holiday."},
    [OPT_MATCH_COLUMNS] = {"match_columns", KIND_SWITCH, "on", NULL,
        "match_columns = on shows position, condition and match rating together on the match screens (widescreen only), and position beside condition in the Condition view of the tactics screen (every mode)."},
    [OPT_REMEMBER_WINDOW] = {"remember_window", KIND_SWITCH, "on", NULL,
        "remember_window = on opens the window with the size, place and full screen state that it had at the last quit."},
    [OPT_SAVE_BACKUPS] = {"save_backups", KIND_SWITCH, "on", NULL,
        "save_backups = on keeps the last three copies of each save in the folder backups next to the saves."},
    [OPT_CONDITION_COLOURS] = {"condition_colours", KIND_SWITCH, "on", NULL,
        "condition_colours = on colours condition percentages and morale words: red, amber or green."},
    [OPT_DATABASE] = {"database", KIND_ENUM, "3.9.60", "3.9.68|3.9.60|3.9.65",
        "database = the data set of a new game: 3.9.60 (2001/02, original data), 3.9.65 (2001/02, December 2001 update), 3.9.68 (2001/02 with the summer 2002 transfers) or folder:<name> for a folder in Databases. A click on the chosen 2001/02 tile of Choose Season changes the first three."},
    [OPT_SQUAD_VIEWS] = {"squad_views", KIND_SWITCH, "on", NULL,
        "squad_views = on adds Team views (best XI, squad depth, compare players) to the Assistant drop-down of a squad screen, and the polygon to the comparison of two chosen players."},
    [OPT_ATTRIBUTE_GROUPS] = {"attribute_groups", KIND_SWITCH, "on", NULL,
        "attribute_groups = on shows the player profile's attributes in Technical, Mental and Physical groups."},
    [OPT_FOREIGN_COLOURS] = {"foreign_colours", KIND_SWITCH, "on", NULL,
        "foreign_colours = on shows in light violet the names of players who count against the foreign player limit of the club's next league or cup match, on the squad and tactics screens."},
    [OPT_FULL_PLAYER_SEARCH] = {"full_player_search", KIND_SWITCH, "off", NULL,
        "full_player_search = on makes the player search list every player, also players that your manager does not know."},
    [OPT_CHANGING_RULES] = {"changing_rules", KIND_SWITCH, "on", NULL,
        "changing_rules = on lets the rules of each league and cup change each July as a new game moves through the seasons. Each game keeps the value it started with."},
    /* 0.6.0 slice 13: the Choose Season screen writes it together with database. */
    [OPT_SEASON] = {"season", KIND_ENUM, "2001/02",
        "2001/02|2002/03|2003/04|2004/05|2005/06|2006/07|2007/08|2008/09|2009/10|2010/11|2011/12|2012/13|2013/14|2014/15|2015/16|2016/17|2017/18|2018/19|2019/20|2020/21|2021/22|2022/23|2023/24|2024/25|2025/26|2026/27",
        "season = the season of a new game, 2001/02 to 2026/27. The Choose Season screen sets it together with database."},
    /* 0.7.0 slice 4: read at each lobby request (net_lobby.c). */
    [OPT_ONLINE_PLAY] = {"online_play", KIND_SWITCH, "on", NULL,
        "online_play = on shows online games in the game list and puts a network game that you host in the online list. off shows LAN games only and never contacts the online service."},
    /* 0.7.0 slice 6 (Q38): the Network tab of the Options box sets it; the join check reads it (net_lobby.c). */
    [OPT_GAME_PASSWORD] = {"game_password", KIND_TEXT, "", NULL,
        "game_password = the password that a human manager types to join a network game that you host. Empty: no password."},
    /* 0.7.3 slice 13 (issue 140): read at each new number (squad_numbers.c). */
    [OPT_SQUAD_NUMBERS_99] = {"squad_numbers_99", KIND_SWITCH, "off", NULL,
        "squad_numbers_99 = on lets a new game keep database squad numbers from 51 to 99, and gives a signing a number up to 99 when 1 to 50 are taken. off gives no new number above 50. In a network game the host's setting applies."},
};

#define TEXT_MAX 1100
static char text_value[OPT_COUNT][TEXT_MAX];   /* the current text of each option */
static int present[OPT_COUNT];                 /* the file has a line for the option */
static int loaded;
static char file_path[TEXT_MAX];               /* the settings file of options_load */

static void lower(char *s)
{
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* 1 or 0 for a switch word, -1 for any other word. */
static int parse_switch(const char *v)
{
    static const char *const yes[] = {"on", "yes", "true"}, *const no[] = {"off", "no", "false"};
    for (int i = 0; i < 3; i++) {
        if (!strcmp(v, yes[i])) return 1;
        if (!strcmp(v, no[i])) return 0;
    }
    return -1;
}

/* The index of word v in the list "a|b|c", or -1. */
static int parse_enum(const char *list, const char *v)
{
    size_t n = strlen(v);
    int index = 0;
    for (const char *p = list; ; index++) {
        const char *end = strchr(p, '|');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == n && !strncmp(p, v, n)) return index;
        if (!end) return -1;
        p = end + 1;
    }
}

static void reset(void)
{
    for (int id = 0; id < OPT_COUNT; id++) {
        snprintf(text_value[id], TEXT_MAX, "%s", defs[id].def);
        present[id] = 0;
    }
    loaded = 1;
}

/* A custom data set: "folder:" and a name. The name keeps its case. */
static int is_custom_database(int id, const char *v)
{
    return id == OPT_DATABASE && !strncmp(v, "folder:", 7) && v[7];
}

/* Sets option id from the word in the file. A word that does not fit keeps the default. */
static void set_value(const char *path, int id, const char *value)
{
    const OptionDef *d = &defs[id];
    char word[TEXT_MAX];
    snprintf(word, sizeof word, "%s", value);
    if (d->kind == KIND_TEXT) {
        snprintf(text_value[id], TEXT_MAX, "%s", word);
        return;
    }
    if (is_custom_database(id, word)) {
        snprintf(text_value[id], TEXT_MAX, "%s", word);
        return;
    }
    lower(word);
    int ok = d->kind == KIND_SWITCH ? parse_switch(word) >= 0 : parse_enum(d->values, word) >= 0;
    if (ok) {
        snprintf(text_value[id], TEXT_MAX, "%s", word);
    } else {
        snprintf(text_value[id], TEXT_MAX, "%s", d->def);
        fprintf(stderr, "cm0102_recomp: %s: unknown value \"%s\" for %s, so the default %s is used.\n", path,
                value, d->name, d->def);
    }
}

/* Reads one line. Sets present[] and the value for a known option. Other lines have no effect. */
static void read_line(const char *path, char *line)
{
    char *p = line;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '#' || !*p) return;
    char key[64];
    size_t n = 0;
    while (isalnum((unsigned char)*p) || *p == '_') {
        if (n < sizeof key - 1) key[n++] = (char)tolower((unsigned char)*p);
        p++;
    }
    key[n] = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (!n || *p != '=') return;
    p++;
    while (isspace((unsigned char)*p)) p++;
    char *end = p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1])) end--;
    *end = 0;
    for (int id = 0; id < OPT_COUNT; id++) {
        if (strcmp(key, defs[id].name)) continue;
        present[id] = 1;
        set_value(path, id, p);
        return;
    }
}

void options_load(const char *path)
{
    reset();
    snprintf(file_path, sizeof file_path, "%s", path);
    int missing_file = 0, ends_with_newline = 1;
    FILE *f = host_fopen(path, "r");
    int open_err = f ? 0 : errno;
    if (!f && open_err == ENOENT) {
        /* A write that stopped half way on Windows can leave the old file as settings.txt.bak. Use it. */
        char bak[TEXT_MAX + 8];
        snprintf(bak, sizeof bak, "%s.bak", path);
        if (host_rename(bak, path) == 0) f = host_fopen(path, "r");
    }
    if (!f) {
        if (open_err != ENOENT) return;      /* the file exists and cannot be opened: write nothing */
        missing_file = 1;
    } else {
        char line[TEXT_MAX + 128];
        int c, len = 0, too_long = 0;
        /* Read by character, so that a long line is cut and not read as two. */
        while ((c = fgetc(f)) != EOF) {
            ends_with_newline = c == '\n';
            if (c == '\n') {
                line[len] = 0;
                if (!too_long) read_line(path, line);
                len = 0;
                too_long = 0;
            } else if (len < (int)sizeof line - 1) {
                line[len++] = (char)c;
            } else {
                too_long = 1;
            }
        }
        if (len && !too_long) {
            line[len] = 0;
            read_line(path, line);
        }
        fclose(f);
    }

    /* The test switch RECOMP_SEASON_CODE=off (Q342, season.c) writes no season line: the file stays as a
     * build without the season code leaves it. */
    const char *season_code = getenv("RECOMP_SEASON_CODE");
    int no_season_line = season_code && !strcmp(season_code, "off") && !present[OPT_SEASON];
    if (no_season_line) present[OPT_SEASON] = 1;
    int missing = 0;
    for (int id = 0; id < OPT_COUNT; id++) missing += !present[id];
    if (no_season_line) present[OPT_SEASON] = 0;
    if (!missing) return;

    /* A new file gets a header. A file of the player only gets the lines that it lacks, at the end. */
    f = host_fopen(path, missing_file ? "w" : "a");
    if (!f) return;
    if (missing_file) {
        fputs("# CM 01/02: Bosman Edition settings.\n"
              "# Each option has one line: name = value. A switch takes on or off. Yes, no, true and false work too.\n",
              f);
    } else if (!ends_with_newline) {
        fputc('\n', f);
    }
    for (int id = 0; id < OPT_COUNT; id++) {
        if (present[id] || (no_season_line && id == OPT_SEASON)) continue;
        fprintf(f, "# %s\n%s = %s\n", defs[id].comment, defs[id].name, defs[id].def);
    }
    fclose(f);
}

int options_bool(OptionId id)
{
    if (!loaded) reset();
    return id < OPT_COUNT && defs[id].kind == KIND_SWITCH && parse_switch(text_value[id]) == 1;
}

const char *options_string(OptionId id)
{
    if (!loaded) reset();
    return id < OPT_COUNT ? text_value[id] : "";
}

int options_enum(OptionId id)
{
    if (!loaded) reset();
    if (id >= OPT_COUNT || defs[id].kind != KIND_ENUM) return 0;
    if (is_custom_database((int)id, text_value[id])) return DATABASE_CUSTOM;
    return parse_enum(defs[id].values, text_value[id]);
}

/* The option name of a line, in lower case, or "" for a comment or another line. */
static void option_key(const char *line, char *key, size_t room)
{
    const char *p = line;
    size_t n = 0;
    while (*p == ' ' || *p == '\t') p++;    /* not a line break: a blank line is not part of the next line */
    while (isalnum((unsigned char)*p) || *p == '_') {
        if (n < room - 1) key[n++] = (char)tolower((unsigned char)*p);
        p++;
    }
    while (*p == ' ' || *p == '\t') p++;
    key[*p == '=' ? n : 0] = 0;
}

/* The value to write for option id, from the word that the caller gives: switches become on or off,
 * enums become lower case, text loses the spaces at its ends. Returns 0 when the word does not fit. */
static int normalise(int id, const char *value, char *out)
{
    const OptionDef *d = &defs[id];
    if (!value || strlen(value) >= TEXT_MAX) return 0;
    snprintf(out, TEXT_MAX, "%s", value);
    /* A line break in a value would write a second line into the file. */
    for (const char *c = out; *c; c++)
        if ((unsigned char)*c < 0x20 && *c != '\t') return 0;
    char *end = out + strlen(out);
    while (end > out && isspace((unsigned char)end[-1])) *--end = 0;
    char *start = out;
    while (isspace((unsigned char)*start)) start++;
    memmove(out, start, strlen(start) + 1);
    if (d->kind == KIND_TEXT) return 1;
    if (is_custom_database(id, out)) return 1;
    lower(out);
    if (d->kind == KIND_SWITCH) {
        int on = parse_switch(out);
        if (on < 0) return 0;
        snprintf(out, TEXT_MAX, "%s", on ? "on" : "off");
        return 1;
    }
    return parse_enum(d->values, out) >= 0;
}

/* The whole file at path in one block (NUL at the end), or NULL with *missing set to 1 when the file
 * does not exist, or NULL with *missing 0 when it cannot be read. */
static char *read_whole(const char *path, size_t *size, int *missing)
{
    *missing = 0;
    FILE *f = host_fopen(path, "rb");
    if (!f) { *missing = errno == ENOENT; return NULL; }
    size_t cap = 65536, n = 0;
    char *text = malloc(cap + 1);
    while (text) {
        size_t got = fread(text + n, 1, cap - n, f);
        n += got;
        if (n < cap) break;
        char *bigger = realloc(text, cap * 2 + 1);
        if (!bigger) { free(text); text = NULL; break; }
        text = bigger;
        cap *= 2;
    }
    int ok = text && !ferror(f);
    fclose(f);
    if (!ok) { free(text); return NULL; }
    text[n] = 0;
    *size = n;
    return text;
}

/* Puts the new file in place of the old one. The old file stays whole when this fails. */
static int replace_file(const char *tmp_path, const char *path)
{
    if (host_replace(tmp_path, path) == 0) return 1;
    /* Fallback: move the old file aside, then restore it if the second move fails. */
    char bak_path[TEXT_MAX + 8];
    snprintf(bak_path, sizeof bak_path, "%s.bak", path);
    host_unlink(bak_path);
    if (host_rename(path, bak_path) != 0) return 0;
    if (host_rename(tmp_path, path) != 0) {
        host_rename(bak_path, path);
        return 0;
    }
    host_unlink(bak_path);
    return 1;
}

/* Writes the option to the settings file. 1 when the file is written or already has the value. */
static int write_option(int id, const char *value)
{
    if (!file_path[0]) return 1;
    size_t n = 0;
    int missing = 0;
    char *text = read_whole(file_path, &n, &missing);
    if (!text) {
        if (!missing) {
            fprintf(stderr, "cm0102_recomp: cannot read %s, so %s is not saved\n", file_path, defs[id].name);
            return 0;
        }
        text = calloc(1, 1);      /* the file is gone: write a new one with this option */
        if (!text) return 0;
    }
    /* Refuse a file that is not plain text: a NUL (UTF-16) or a bare CR (old Mac line ends). */
    int plain = strlen(text) == n;
    for (size_t k = 0; plain && k < n; k++)
        if (text[k] == '\r' && (k + 1 >= n || text[k + 1] != '\n')) plain = 0;
    if (!plain) {
        free(text);
        fprintf(stderr, "cm0102_recomp: %s is not plain text with LF or CRLF line ends, so %s is not saved\n",
                file_path, defs[id].name);
        return 0;
    }
    /* Build the new text in memory: the old text, with the option's lines replaced. */
    size_t matches = 0;
    for (char *line = text; *line;) {
        char *end = strchr(line, '\n');
        char key[64];
        option_key(line, key, sizeof key);
        matches += !strcmp(key, defs[id].name);
        line += end ? (size_t)(end - line) + 1 : strlen(line);
    }
    size_t room = n + matches * (strlen(defs[id].name) + strlen(value) + 8) + strlen(defs[id].comment) +
                  strlen(defs[id].name) + strlen(value) + 64;
    char *out = malloc(room);
    if (!out) { free(text); return 0; }
    const char *nl = strchr(text, '\n');
    const char *eol = nl && nl > text && nl[-1] == '\r' ? "\r\n" : "\n";   /* the file's own line end for added lines */
    size_t o = 0;
    int found = 0;
    for (char *line = text; *line;) {
        char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) + 1 : strlen(line);
        char key[64];
        option_key(line, key, sizeof key);
        if (!strcmp(key, defs[id].name)) {
            const char *own = end ? (len > 1 && end[-1] == '\r' ? "\r\n" : "\n") : "";
            o += (size_t)snprintf(out + o, room - o, "%s = %s%s", defs[id].name, value, own);
            found = 1;
        } else {
            memcpy(out + o, line, len);
            o += len;
        }
        line += len;
    }
    if (!found) {
        if (n && text[n - 1] != '\n') { memcpy(out + o, eol, strlen(eol)); o += strlen(eol); }
        o += (size_t)snprintf(out + o, room - o, "# %s%s%s = %s%s", defs[id].comment, eol, defs[id].name, value, eol);
    }
    int same = o == n && !memcmp(out, text, n);
    free(text);
    if (same) { free(out); return 1; }

    char tmp_path[TEXT_MAX + 8];
    snprintf(tmp_path, sizeof tmp_path, "%s.tmp", file_path);
    FILE *f = host_fopen(tmp_path, "wb");
    if (!f) {
        free(out);
        fprintf(stderr, "cm0102_recomp: cannot write %s, so %s is not saved\n", tmp_path, defs[id].name);
        return 0;
    }
    int write_ok = fwrite(out, 1, o, f) == o && fflush(f) == 0 && host_sync(f) == 0 && !ferror(f);
    if (fclose(f) != 0) write_ok = 0;
    free(out);
    if (write_ok) write_ok = replace_file(tmp_path, file_path);
    if (!write_ok) {
        host_unlink(tmp_path);
        fprintf(stderr, "cm0102_recomp: cannot write %s, so %s is not saved\n", file_path, defs[id].name);
    }
    return write_ok;
}

int options_set(OptionId id, const char *value)
{
    if (!loaded) reset();
    char word[TEXT_MAX];
    if (id >= OPT_COUNT || !normalise((int)id, value, word)) return 0;
    snprintf(text_value[id], TEXT_MAX, "%s", word);       /* the running game has the value now */
    int saved = write_option((int)id, word);
    if (saved) present[id] = 1;
    return saved ? 1 : -1;
}

int options_set_enum(OptionId id, const char *value)
{
    if (id >= OPT_COUNT || defs[id].kind != KIND_ENUM) return 0;
    return options_set(id, value);
}

const char *options_name(OptionId id)
{
    return id < OPT_COUNT ? defs[id].name : "";
}
