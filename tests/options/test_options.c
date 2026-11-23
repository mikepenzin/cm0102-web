/* Test of the settings file reader (src/recomp_rt/win/options.c).
 * Usage: options_test   (it makes and removes its own temporary folder) */
#include "options.h"
#include "../../src/recomp_rt/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
static char dir[1024], path[1100];

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void put(const char *text)
{
    FILE *f = host_fopen(path, "w");
    if (!f) { printf("cannot write %s\n", path); exit(2); }
    fputs(text, f);
    fclose(f);
}

static char *get(void)
{
    static char buf[16384];
    FILE *f = host_fopen(path, "r");
    size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
    if (f) fclose(f);
    buf[n] = 0;
    return buf;
}

static int count(const char *hay, const char *needle)
{
    int n = 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += strlen(needle)) n++;
    return n;
}

/* The text has exactly one line "name = " for the option (the line may start the text). */
static int has_one_line(const char *text, OptionId id)
{
    char key[80];
    snprintf(key, sizeof key, "\n%s = ", options_name(id));
    return count(text, key) + (!strncmp(text, key + 1, strlen(key) - 1)) == 1;
}

/* The text has exactly one comment line for the option. */
static int has_one_comment(const char *text, OptionId id)
{
    char key[80];
    snprintf(key, sizeof key, "# %s = ", options_name(id));
    return count(text, key) == 1;
}

static void check_defaults(void)
{
    CHECK(options_enum(OPT_DISPLAY) == DISPLAY_WIDESCREEN);
    CHECK(options_bool(OPT_SPACE_TO_CONTINUE) == 1);
    CHECK(options_bool(OPT_SHORTCUTS) == 1);
    CHECK(options_bool(OPT_SHARP_PICTURE) == 1);
    CHECK(!strcmp(options_string(OPT_SAVE_FOLDER), ""));
    CHECK(options_bool(OPT_SOUND) == 1);
    CHECK(options_bool(OPT_CLICKABLE_NAMES) == 1);
    CHECK(options_bool(OPT_ATTRIBUTE_COLOURS) == 1);
    CHECK(options_bool(OPT_FASTER_DAYS) == 1);
    CHECK(options_bool(OPT_MATCH_COLUMNS) == 1);
    CHECK(options_bool(OPT_REMEMBER_WINDOW) == 1);
    CHECK(options_bool(OPT_SAVE_BACKUPS) == 1);
    CHECK(options_bool(OPT_CONDITION_COLOURS) == 1);
    CHECK(options_bool(OPT_SQUAD_VIEWS) == 1);
    CHECK(options_bool(OPT_ATTRIBUTE_GROUPS) == 1);
    CHECK(options_bool(OPT_FOREIGN_COLOURS) == 1);
    CHECK(options_bool(OPT_FULL_PLAYER_SEARCH) == 0);
    CHECK(options_bool(OPT_CHANGING_RULES) == 1);
    CHECK(options_bool(OPT_ONLINE_PLAY) == 1);
    CHECK(options_bool(OPT_SQUAD_NUMBERS_99) == 0);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960);
    CHECK(!strcmp(options_string(OPT_DATABASE), "3.9.60"));   /* item 11 of 0.6.1: the original data */
    CHECK(options_enum(OPT_SEASON) == 0 && !strcmp(options_string(OPT_SEASON), "2001/02"));
}

int main(void)
{
    snprintf(dir, sizeof dir, "%s/options_test_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
    snprintf(path, sizeof path, "%s/settings.txt", dir);

    /* 1. A missing file is written with every option at its default. */
    options_load(path);
    check_defaults();
    for (int id = 0; id < OPT_COUNT; id++) {
        CHECK(has_one_line(get(), (OptionId)id));
        CHECK(has_one_comment(get(), (OptionId)id));
    }
    CHECK(strstr(get(), "\nfaster_days = on\n") != NULL);
    CHECK(strstr(get(), "\ndisplay = widescreen\n") != NULL);
    CHECK(strstr(get(), "display = classic draws it at 800x600.\n") != NULL);
    CHECK(strstr(get(), "\nsave_folder = \n") != NULL);
    CHECK(strstr(get(), "\nsave_backups = on\n") != NULL);
    CHECK(strstr(get(), "\nsquad_views = on\n") != NULL);
    CHECK(strstr(get(), "\nattribute_groups = on\n") != NULL);
    CHECK(strstr(get(), "\nforeign_colours = on\n") != NULL);
    CHECK(strstr(get(), "\nfull_player_search = off\n") != NULL);
    CHECK(strstr(get(), "full_player_search = on makes the player search list every player") != NULL);
    CHECK(strstr(get(), "\nchanging_rules = on\n") != NULL);
    CHECK(strstr(get(), "changing_rules = on lets the rules of each league and cup change each July") != NULL);
    CHECK(strstr(get(), "\nonline_play = on\n") != NULL);
    CHECK(strstr(get(), "\nsquad_numbers_99 = off\n") != NULL);
    CHECK(strstr(get(), "squad_numbers_99 = on lets a new game keep database squad numbers") != NULL);
    CHECK(strstr(get(), "online_play = on shows online games in the game list") != NULL);
    CHECK(strstr(get(), "\nseason = 2001/02\n") != NULL);
    CHECK(strstr(get(), "season = the season of a new game, 2001/02 to 2026/27.") != NULL);
    static char first[16384];
    snprintf(first, sizeof first, "%s", get());
    options_load(path);                         /* a second start changes nothing */
    CHECK(!strcmp(first, get()));
    CHECK(!strcmp(options_string(OPT_SAVE_FOLDER), ""));

    /* 2. Unknown lines and the order of the player's lines stay. */
    const char *mine = "# my notes\nzebra = 1\n\nDisplay = Classic\nSOUND = No\n"
                       "save_folder =  /Users/me/My Saves \nfuture_option=yes\n";
    put(mine);
    options_load(path);
    const char *t = get();
    CHECK(options_enum(OPT_DISPLAY) == DISPLAY_CLASSIC);
    CHECK(options_bool(OPT_SOUND) == 0);
    CHECK(!strcmp(options_string(OPT_SAVE_FOLDER), "/Users/me/My Saves"));
    CHECK(options_bool(OPT_SHORTCUTS) == 1);
    CHECK(!strncmp(t, mine, strlen(mine)));
    CHECK(count(t, "zebra = 1") == 1 && count(t, "future_option=yes") == 1);
    CHECK(count(t, "\nsound = ") == 0 && count(t, "\ndisplay = ") == 0 && count(t, "\nsave_folder = ") == 1);
    CHECK(strstr(t, "\nshortcuts = on\n") != NULL);

    /* 3. An old file with only display = classic keeps classic and gains the new lines. */
    const char *old = "# CM 01/02: Bosman Edition settings.\n"
                      "# display = widescreen draws the game at 1280x800. display = classic draws it at 800x600.\n"
                      "display = classic\n";
    put(old);
    options_load(path);
    t = get();
    CHECK(options_enum(OPT_DISPLAY) == DISPLAY_CLASSIC);
    CHECK(options_bool(OPT_FASTER_DAYS) == 1 && options_bool(OPT_MATCH_COLUMNS) == 1);
    CHECK(!strncmp(t, old, strlen(old)));
    CHECK(count(t, "\ndisplay = classic\n") == 1 && count(t, "\ndisplay = ") == 1);
    for (int id = 0; id < OPT_COUNT; id++) {
        if (id == OPT_DISPLAY) continue;
        CHECK(has_one_line(t, (OptionId)id));
        CHECK(has_one_comment(t, (OptionId)id));
    }
    static char again[16384];
    snprintf(again, sizeof again, "%s", t);
    options_load(path);                         /* the added lines are not added twice */
    CHECK(!strcmp(again, get()));

    /* 4. A last line without a newline gets one before the added lines. */
    put("display = classic");
    options_load(path);
    CHECK(!strncmp(get(), "display = classic\n# ", 19));

    /* 5. Words for the switches, and a bad value keeps the default. */
    put("space_to_continue = FALSE\nshortcuts = yes\nsharp_picture = off\nsound = true\n"
        "clickable_names = maybe\ndisplay = tiny\nattribute_colours = off\r\nfaster_days = off\r\n"
        "condition_colours = off\nsquad_views = no\nattribute_groups = no\nforeign_colours = off\n");
    options_load(path);
    CHECK(options_bool(OPT_SPACE_TO_CONTINUE) == 0);
    CHECK(options_bool(OPT_SHORTCUTS) == 1);
    CHECK(options_bool(OPT_SHARP_PICTURE) == 0);
    CHECK(options_bool(OPT_SOUND) == 1);
    CHECK(options_bool(OPT_CLICKABLE_NAMES) == 1);
    CHECK(options_enum(OPT_DISPLAY) == DISPLAY_WIDESCREEN);
    CHECK(options_bool(OPT_ATTRIBUTE_COLOURS) == 0);
    CHECK(options_bool(OPT_FASTER_DAYS) == 0);
    CHECK(options_bool(OPT_CONDITION_COLOURS) == 0);
    CHECK(options_bool(OPT_SQUAD_VIEWS) == 0);
    CHECK(options_bool(OPT_ATTRIBUTE_GROUPS) == 0);
    CHECK(options_bool(OPT_FOREIGN_COLOURS) == 0);
    CHECK(options_bool(OPT_MATCH_COLUMNS) == 1);

    /* 5b. full_player_search (0.5.0 slice 5): off by default, on when the file says so. */
    put("full_player_search = on\n");
    options_load(path);
    CHECK(options_bool(OPT_FULL_PLAYER_SEARCH) == 1);
    put("full_player_search = maybe\n");
    options_load(path);
    CHECK(options_bool(OPT_FULL_PLAYER_SEARCH) == 0);

    /* 5b2. online_play (0.7.0 slice 4): on by default, off when the file says so. */
    put("online_play = off\n");
    options_load(path);
    CHECK(options_bool(OPT_ONLINE_PLAY) == 0);

    /* 5b3. game_password (0.7.0 slice 6): empty by default, the text of the file when it has one. */
    CHECK(options_string(OPT_GAME_PASSWORD)[0] == 0);
    put("game_password = Sunday 7\n");
    options_load(path);
    CHECK(!strcmp(options_string(OPT_GAME_PASSWORD), "Sunday 7"));

    /* 5c. changing_rules (0.6.0 slice 11): on by default, off when the file says so. A bad word keeps on. */
    put("changing_rules = off\n");
    options_load(path);
    CHECK(options_bool(OPT_CHANGING_RULES) == 0);
    put("changing_rules = sometimes\n");
    options_load(path);
    CHECK(options_bool(OPT_CHANGING_RULES) == 1);

    /* 5d. season (0.6.0 slice 13): the 26 seasons; a bad word keeps 2001/02. The Choose Season screen writes it. */
    put("season = 2010/11\n");
    options_load(path);
    CHECK(options_enum(OPT_SEASON) == 9 && !strcmp(options_string(OPT_SEASON), "2010/11"));
    put("season = 2025/26\n");
    options_load(path);
    CHECK(options_enum(OPT_SEASON) == 24);
    put("season = 2026/27\n");
    options_load(path);
    CHECK(options_enum(OPT_SEASON) == 25);
    put("season = 2027/28\n");
    options_load(path);
    CHECK(options_enum(OPT_SEASON) == 0);
    put("season = 2010\n");
    options_load(path);
    CHECK(options_enum(OPT_SEASON) == 0);
    CHECK(options_set(OPT_SEASON, "2003/04") == 1 && options_enum(OPT_SEASON) == 2);
    CHECK(strstr(get(), "season = 2003/04\n") == get());
    CHECK(options_set(OPT_SEASON, "2000/01") == 0 && options_enum(OPT_SEASON) == 2);

    /* 6. The HD mode of 0.4.0 to 0.5.0 is gone: an old display = hd gives widescreen, and the line stays. */
    put("display = hd\n");
    options_load(path);
    CHECK(options_enum(OPT_DISPLAY) == DISPLAY_WIDESCREEN);
    CHECK(!strcmp(options_string(OPT_DISPLAY), "widescreen"));
    CHECK(strstr(get(), "display = hd\n") == get());
    CHECK(options_set(OPT_DISPLAY, "hd") == 0);

    /* 7. The database option: its values, a bad value, and the Setup Game choice that writes the file. */
    put("database = 3.9.60\n");
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960);
    put("database = 3.9.99\n");
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960);
    const char *before = "# my notes\nDatabase = 3.9.65\r\nsound = off\n";
    put(before);
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3965);
    CHECK(options_set_enum(OPT_DATABASE, "3.9.60") == 1);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960);
    t = get();
    const char *after = "# my notes\ndatabase = 3.9.60\r\nsound = off\n";
    CHECK(!strncmp(t, after, strlen(after)));
    CHECK(count(t, "database = ") == 1);
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960 && options_bool(OPT_SOUND) == 0);
    CHECK(options_set_enum(OPT_DATABASE, "3.9.99") == 0);
    CHECK(options_set_enum(OPT_SOUND, "off") == 0);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960);
    /* A custom data set is "folder:<name>" (Q143). The name keeps its case, spaces and letters. */
    put("database = folder:May 2026\n");
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_CUSTOM);
    CHECK(!strcmp(options_string(OPT_DATABASE), "folder:May 2026"));
    put("database = folder:Zo\xc3\xab Update\nsound = off\n");
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_CUSTOM && !strcmp(options_string(OPT_DATABASE), "folder:Zo\xc3\xab Update"));
    put("database = folder:\n");
    options_load(path);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_3960);
    put("# x\ndatabase = 3.9.60\r\nsound = off\n");
    options_load(path);
    CHECK(options_set_enum(OPT_DATABASE, "folder:Upper Case") == 1);
    CHECK(options_enum(OPT_DATABASE) == DATABASE_CUSTOM);
    t = get();
    {
        const char *want = "# x\ndatabase = folder:Upper Case\r\nsound = off\n";   /* the whole line, not a prefix */
        CHECK(!strncmp(t, want, strlen(want)));
    }
    options_load(path);
    CHECK(!strcmp(options_string(OPT_DATABASE), "folder:Upper Case"));
    CHECK(options_set_enum(OPT_DATABASE, "folder:") == 0);
    CHECK(options_set_enum(OPT_DATABASE, "3.9.60") == 1 && options_enum(OPT_DATABASE) == DATABASE_3960);
    put("display = classic");
    options_load(path);
    host_unlink(path);                          /* a file without the line gets it at the end */
    put("display = classic");
    CHECK(options_set_enum(OPT_DATABASE, "3.9.65") == 1);
    t = get();
    const char *added = "display = classic\n# database = ";
    CHECK(!strncmp(t, added, strlen(added)) && strstr(t, "\ndatabase = 3.9.65\n") != NULL);

    /* A file above 64 KB is rewritten whole, and no temporary file stays. */
    size_t big_size = 200000;
    char *big = malloc(big_size + 64);
    memset(big, 'x', big_size);
    big[0] = '#';
    big[big_size - 1] = '\n';
    strcpy(big + big_size, "database = 3.9.65\nsound = off\n");
    put(big);
    options_load(path);
    CHECK(options_set_enum(OPT_DATABASE, "3.9.60") == 1);
    FILE *bf = fopen(path, "rb");
    char *bt = calloc(big_size + 4096, 1);
    size_t bn = bf ? fread(bt, 1, big_size + 4095, bf) : 0;   /* options_load added the missing options to the end */
    if (bf) fclose(bf);
    CHECK(bn >= big_size + strlen("database = 3.9.60\nsound = off\n") && bt[0] == '#' && bt[big_size - 2] == 'x');
    CHECK(strstr(bt, "\ndatabase = 3.9.60\nsound = off\n") != NULL && !strstr(bt, "3.9.65"));
    free(bt);
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    HostStat st;
    CHECK(host_stat(tmp, &st) != 0);
    free(big);

    /* 8. options_set: switches, choices and text, through the temporary file and a rename. */
    char tmp2[1100], bak2[1100], snap[16384];
    snprintf(tmp2, sizeof tmp2, "%s.tmp", path);
    snprintf(bak2, sizeof bak2, "%s.bak", path);
    /* 8a. A switch: the line changes, the comments, the other lines and the CRLF stay. */
    put("# my notes\r\nzebra = 1\r\nSound = On\r\n# about shortcuts\r\nshortcuts = yes\r\nfuture_option=yes\r\n");
    options_load(path);
    CHECK(options_bool(OPT_SOUND) == 1);
    CHECK(options_set(OPT_SOUND, "OFF") == 1);
    CHECK(options_bool(OPT_SOUND) == 0);                         /* the running game has it at once */
    t = get();
    CHECK(strstr(t, "# my notes\r\nzebra = 1\r\nsound = off\r\n# about shortcuts\r\nshortcuts = yes\r\nfuture_option=yes\r\n") == t);
    CHECK(count(t, "\nsound = ") == 1 && count(t, "\nSound") == 0);
    CHECK(host_stat(tmp2, &st) != 0 && host_stat(bak2, &st) != 0);   /* no leftover file */
    CHECK(options_set(OPT_SHORTCUTS, "no") == 1 && options_bool(OPT_SHORTCUTS) == 0);
    CHECK(strstr(get(), "# about shortcuts\r\nshortcuts = off\r\n") != NULL);
    CHECK(options_set(OPT_SHORTCUTS, "True") == 1 && options_bool(OPT_SHORTCUTS) == 1);
    CHECK(strstr(get(), "shortcuts = on\r\n") != NULL);
    options_load(path);                                          /* the reader sees what the writer wrote */
    CHECK(options_bool(OPT_SOUND) == 0 && options_bool(OPT_SHORTCUTS) == 1);
    /* 8b. Every option, one by one: only that line changes. */
    put("# x\nzebra = 1\n");
    options_load(path);
    for (int id = 0; id < OPT_COUNT; id++) {
        const OptionId oid = (OptionId)id;
        if (!strcmp(options_name(oid), "display")) {
            CHECK(options_set(oid, "classic") == 1 && options_enum(oid) == DISPLAY_CLASSIC);
        } else if (!strcmp(options_name(oid), "save_folder")) {
            CHECK(options_set(oid, "  /Users/me/My Saves ") == 1 && !strcmp(options_string(oid), "/Users/me/My Saves"));
        } else if (!strcmp(options_name(oid), "database")) {
            CHECK(options_set(oid, "folder:Mixed Case") == 1 && options_enum(oid) == DATABASE_CUSTOM);
        } else if (!strcmp(options_name(oid), "season")) {
            CHECK(options_set(oid, "2026/27") == 1 && options_enum(oid) == 25);
        } else {
            CHECK(options_set(oid, "off") == 1 && options_bool(oid) == 0);
        }
        CHECK(has_one_line(get(), oid) && has_one_comment(get(), oid));
    }
    CHECK(strstr(get(), "# x\nzebra = 1\n") == get());
    CHECK(strstr(get(), "\ndisplay = classic\n") && strstr(get(), "\nsave_folder = /Users/me/My Saves\n") &&
          strstr(get(), "\ndatabase = folder:Mixed Case\n") && strstr(get(), "\nsquad_views = off\n"));
    options_load(path);
    CHECK(options_enum(OPT_DISPLAY) == DISPLAY_CLASSIC && !strcmp(options_string(OPT_SAVE_FOLDER), "/Users/me/My Saves") &&
          options_bool(OPT_ATTRIBUTE_GROUPS) == 0 && options_enum(OPT_DATABASE) == DATABASE_CUSTOM);
    CHECK(options_set(OPT_SAVE_FOLDER, "") == 1 && !strcmp(options_string(OPT_SAVE_FOLDER), ""));   /* empty text */
    CHECK(strstr(get(), "\nsave_folder = \n") != NULL);
    /* 8c. A missing line is added at the end with its comment, in the file's own line end. */
    put("display = classic");
    options_load(path);
    host_unlink(path);
    put("display = classic");
    CHECK(options_set(OPT_FASTER_DAYS, "off") == 1);
    t = get();
    CHECK(strstr(t, "display = classic\n# faster_days = on makes") == t && strstr(t, "\nfaster_days = off\n") != NULL);
    CHECK(has_one_line(t, OPT_FASTER_DAYS) && has_one_comment(t, OPT_FASTER_DAYS));
    put("display = classic\r\nsound = on");
    options_load(path);
    host_unlink(path);
    put("display = classic\r\nsound = on");
    CHECK(options_set(OPT_CLICKABLE_NAMES, "off") == 1);
    t = get();
    CHECK(strstr(t, "sound = on\r\n# clickable_names = on lets") && strstr(t, "\r\nclickable_names = off\r\n"));
    CHECK(count(t, "\r\n") == count(t, "\n"));                    /* no bare line feed added */
    /* 8d. A value that does not fit changes nothing, in memory or in the file. */
    put("# keep\nsound = on\ndatabase = 3.9.60\ndisplay = classic\n");
    options_load(path);
    snprintf(snap, sizeof snap, "%s", get());
    CHECK(options_set(OPT_SOUND, "maybe") == 0 && options_bool(OPT_SOUND) == 1);
    CHECK(options_set(OPT_DISPLAY, "tiny") == 0 && options_enum(OPT_DISPLAY) == DISPLAY_CLASSIC);
    CHECK(options_set(OPT_DATABASE, "3.9.99") == 0 && options_set(OPT_DATABASE, "folder:") == 0);
    CHECK(options_set(OPT_SOUND, NULL) == 0 && options_set(OPT_SOUND, "") == 0 && options_set(OPT_COUNT, "on") == 0);
    CHECK(options_set(OPT_SAVE_FOLDER, "/a\nsound = off") == 0 && options_set(OPT_SAVE_FOLDER, "/a\r/b") == 0);
    {
        char longv[1300];
        memset(longv, 'a', sizeof longv - 1);
        longv[sizeof longv - 1] = 0;
        CHECK(options_set(OPT_SAVE_FOLDER, longv) == 0);
    }
    CHECK(options_set_enum(OPT_SOUND, "off") == 0 && options_bool(OPT_SOUND) == 1);
    CHECK(!strcmp(snap, get()) && options_bool(OPT_SOUND) == 1);
    /* 8e. The same value leaves the file as it was. */
    CHECK(options_set(OPT_SOUND, "on") == 1 && !strcmp(snap, get()));
    CHECK(options_set(OPT_DISPLAY, "classic") == 1 && !strcmp(snap, get()));
    /* 8f. A file that is gone is written again with the option. */
    host_unlink(path);
    CHECK(options_set(OPT_SOUND, "off") == 1);
    CHECK(strstr(get(), "# sound = off mutes") != NULL && strstr(get(), "\nsound = off\n") != NULL);
    /* 8g. Failures leave the old file whole. The value stays set in memory, and the call says -1. */
    put("# keep me\nsound = on\r\ndisplay = classic\n");
    options_load(path);
    snprintf(snap, sizeof snap, "%s", get());
    CHECK(mkdir(tmp2, 0755) == 0);                                /* the temporary file cannot open */
    CHECK(options_set(OPT_SOUND, "off") == -1 && options_bool(OPT_SOUND) == 0);
    CHECK(!strcmp(snap, get()));
    rmdir(tmp2);
    CHECK(options_set(OPT_SOUND, "off") == 1 && strstr(get(), "# keep me\nsound = off\r\ndisplay = classic\n") != NULL);
    put("# keep me\nsound = on\n");
    options_load(path);
    snprintf(snap, sizeof snap, "%s", get());
    if (geteuid() != 0) {                                         /* a folder that cannot be written */
        CHECK(chmod(dir, 0555) == 0);
        CHECK(options_set(OPT_SOUND, "off") == -1 && options_bool(OPT_SOUND) == 0);
        CHECK(options_set(OPT_FASTER_DAYS, "off") == -1 && options_bool(OPT_FASTER_DAYS) == 0);
        CHECK(chmod(dir, 0755) == 0);
        CHECK(!strcmp(snap, get()));
        CHECK(host_stat(tmp2, &st) != 0 && host_stat(bak2, &st) != 0);
        CHECK(options_set(OPT_SOUND, "on") == 1 && options_bool(OPT_SOUND) == 1);
    } else {
        printf("skip: read-only folder test (running as root)\n");
    }
    /* A file that cannot be read (here a folder) is not replaced. */
    host_unlink(path);
    CHECK(mkdir(path, 0755) == 0);
    CHECK(options_set(OPT_SOUND, "off") == -1 && options_bool(OPT_SOUND) == 0);
    CHECK(host_stat(path, &st) == 0 && st.is_dir);
    rmdir(path);
    /* A stale temporary file from an earlier crash is overwritten and gone after the write. */
    put("# keep\nsound = on\n");
    options_load(path);
    { FILE *stale = host_fopen(tmp2, "w"); fputs("junk junk junk", stale); fclose(stale); }
    CHECK(options_set(OPT_SOUND, "off") == 1 && host_stat(tmp2, &st) != 0);
    CHECK(strstr(get(), "# keep\nsound = off\n") == get());
    /* 8h. A file above 64 KB keeps every byte but the option's line. */
    {
        size_t sz = 150000;
        char *big2 = malloc(sz + 64);
        memset(big2, 'y', sz);
        big2[0] = '#';
        big2[sz - 1] = '\n';
        strcpy(big2 + sz, "sound = on\n");
        put(big2);
        options_load(path);
        CHECK(options_set(OPT_SOUND, "off") == 1);
        FILE *bf2 = fopen(path, "rb");
        char *got2 = calloc(sz + 8192, 1);
        size_t gn = bf2 ? fread(got2, 1, sz + 8191, bf2) : 0;
        if (bf2) fclose(bf2);
        CHECK(gn > sz && !memcmp(got2, big2, sz) && strstr(got2 + sz, "sound = off\n") && !strstr(got2 + sz, "sound = on\n"));
        free(got2);
        free(big2);
    }

    /* 9. Review fixes. */
    /* 9a. A file that exists but cannot be opened (chmod a-r) is never replaced, by options_set or options_load. */
    put("# keep\nsound = on\n");
    options_load(path);
    snprintf(snap, sizeof snap, "%s", get());
    if (geteuid() != 0) {
        CHECK(chmod(path, 0200) == 0);
        CHECK(options_set(OPT_SOUND, "off") == -1 && options_bool(OPT_SOUND) == 0);
        options_load(path);
        CHECK(chmod(path, 0644) == 0);
        CHECK(!strcmp(snap, get()));
        CHECK(host_stat(tmp2, &st) != 0);
    }
    /* 9b. A file with a NUL (UTF-16) or a bare CR is refused and stays as it is. */
    {
        static const char utf16[] = "\xff\xfes\0o\0u\0n\0d\0 \0=\0 \0o\0n\0\n\0";
        FILE *w = host_fopen(path, "wb");
        fwrite(utf16, 1, sizeof utf16 - 1, w);
        fclose(w);
        options_load(path);              /* the start may add missing lines at the end: take the file after it */
        char before[4096] = {0}, got[4096] = {0};
        FILE *r = host_fopen(path, "rb");
        size_t bn = fread(before, 1, sizeof before, r);
        fclose(r);
        CHECK(bn >= sizeof utf16 - 1 && !memcmp(before, utf16, sizeof utf16 - 1));
        CHECK(options_set(OPT_SOUND, "off") == -1);
        r = host_fopen(path, "rb");
        size_t gn = fread(got, 1, sizeof got, r);
        fclose(r);
        CHECK(gn == bn && !memcmp(got, before, bn));
    }
    put("# keep\rsound = on\rdisplay = classic\r");
    options_load(path);
    snprintf(snap, sizeof snap, "%s", get());
    CHECK(options_set(OPT_SOUND, "off") == -1 && !strcmp(snap, get()));
    put("# keep\r\nsound = on\r\nx = 1\ry = 2\r\n");   /* one bare CR among CRLF lines is refused too */
    options_load(path);
    snprintf(snap, sizeof snap, "%s", get());
    CHECK(options_set(OPT_SOUND, "off") == -1 && !strcmp(snap, get()));
    /* 9c. A blank line before the option's line stays, and the option is not doubled. */
    put("# keep\n\nsound = on\n\n\ndisplay = classic\n");
    options_load(path);
    CHECK(options_set(OPT_SOUND, "off") == 1);
    CHECK(strstr(get(), "# keep\n\nsound = off\n\n\ndisplay = classic\n") == get());
    CHECK(count(get(), "\nsound = ") == 1);
    put("\n\nsound = on\n");
    options_load(path);
    CHECK(options_set(OPT_SOUND, "off") == 1 && strstr(get(), "\n\nsound = off\n") == get());
    /* 9d. A missing settings file with a .bak file: the .bak file comes back, no defaults are written. */
    put("# mine\nsound = off\ndisplay = classic\n");
    options_load(path);
    CHECK(host_rename(path, bak2) == 0);
    options_load(path);
    CHECK(options_bool(OPT_SOUND) == 0 && options_enum(OPT_DISPLAY) == DISPLAY_CLASSIC);
    CHECK(strstr(get(), "# mine\nsound = off\ndisplay = classic\n") == get() && host_stat(bak2, &st) != 0);

    host_unlink(path);
    rmdir(dir);
    if (failures) printf("options_test: %d failures\n", failures);
    else printf("options_test: ok\n");
    return failures ? 1 : 0;
}
