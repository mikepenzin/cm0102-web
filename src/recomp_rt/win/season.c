/* The season of a game: start year, changing rules and the season part of a save (season.h).
 *
 * The save container (zip.cpp of the original, tools/save/savfile.py): three words (version, flags, count),
 * then 1000 rows of 0x10c bytes (offset, size, name of 260 bytes), then the parts. Version 2 and 4 store each
 * part with the byte run code of zwrite 0x932e10, version 1 and 3 store it as it is (zopen 0x9326e0 maps 1
 * to 3 and 2 to 4).
 *
 * The part bosman_season.dat, version 1, 16 bytes, little endian:
 *   +0  u32 format version (1)
 *   +4  u32 length of the part in bytes (16)
 *   +8  u32 start year
 *   +12 u32 changing rules (0 or 1)
 * A later version only adds fields after these and makes the length larger. A reader takes the fields that
 * it knows and that lie inside the length, and gives the others their old meaning.
 * Version 2 (0.6.0 slice 16) adds the news texts, the changed names and the last season point of the season
 * change after +16 (season_change.h). A game with none of them writes version 1. A later version keeps the
 * fields of version 2 first, so this reader takes them and leaves what follows.
 * Version 3 (0.6.2 slice 12) adds the caps above 255 after the row A18 list, and version 4 (item 66) the goals above
 * 255 after the caps (season_change.h, caps_wide.h). A game with no player above 255 caps or goals writes version 1
 * or 2 as before. Version 5 (0.6.2 slice 4b) writes both of those fields and then the marks of qualifying campaigns
 * that keep the original's shape (season_intl.h), only while a game holds such a campaign.
 *
 * The original opens a part by its exact name (zopen 0x9326e0, the name scan at 0x93293b) and extracts only
 * the parts whose name ends in .tmp (load_game_data 0x82a9c0 calls 0x933320 with "*.tmp" at 0x82acbc), so it
 * never reads this part. The runtime adds the part to the file of the save after the original's last part
 * (save_fifa_transfer_rules 0x8395b0), before the save deletes the old file and renames its temporary file to
 * the name (0.6.1 item 10), and reads it before the load 0x82a9c0 runs. Neither touches the game's memory. The
 * proof: docs/research/2026-10-17-season-save-part.md. */
#include "win.h"
#include "season.h"
#include "tier_a.h"
#include "season_names.h"
#include "season_change.h"
#include "season_calendar.h"
#include "caps_wide.h"
#include "season_dates.h"
#include "season_intl.h"
#include "season_divisions.h"
#include "season_load_check.h"
#include "options.h"
#include "../host.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* rt_season_year (rt.h, defined in rt.c with 2001): read by the year sites of tools/recomp/seasons.py. */
static int rules_now;                          /* changing rules of the game that runs */
static int new_year = SEASON_FIRST_YEAR;        /* the season of the next new game */
static int new_rules = -1;                      /* -1: the option changing_rules */
static int game_on;                             /* 1 from a made or loaded game to free_game_data */

#define GAME_DATE 0xae2c90u        /* the game date: s16 day of the year, s16 year, u32 leap year */
#define DATE_SET 0x0054a130u       /* Date::set, thiscall (day 1 to 31, month 0 to 11, year, weekday or -1) */

#define PART_NAME "bosman_season.dat"
#define PART_VERSION 1u
#define PART_VERSION_NEWS 2u       /* with the news texts of season_change.c */
#define PART_SIZE 16u
#define HEADER_ROWS 1000u
#define ROW_SIZE 0x10cu
#define ROW_NAME 8u
#define NAME_SIZE 260u
#define MAX_PARTS 999u             /* zopen 0x9326e0 refuses a new part when the count is 999: 0x932a3e
                                      cmp edi, 0x3e7 and jl to the add (bytes 81 ff e7 03 00 00 7c 26) */
/* The largest part that a reader takes. Version 1 has 16 bytes; version 2 adds the news texts (8 KB a season
 * at most, season_change.c) and the changed names. Up to 0.6.1 the limit was 1 MB, about 0.9 MB for 99 seasons.
 * 0.6.2 (slice 1) has no year limit, so the limit is 64 MB, more than 8 KB of news in each of 7000 seasons. It
 * only stops a damaged row from asking for a block of gigabytes. */
#define PART_MAX (1u << 26)
#define INDEX_MAX (1u << 20)       /* the largest index.dat that lacks_key_comps reads (the 1 MB of 0.6.1) */

/* The test switch RECOMP_SEASON_CODE=off (Q342): the season code does nothing. The start year stays 2001, so
 * every year site of tools/recomp/seasons.py gives the original's result; the runtime reads and writes no
 * season part, takes no year from RECOMP_START_SEASON or the Choose Season screen, and changing rules stay
 * off. A base game with the switch must give the same save as the same game without it. */
int season_code_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_CODE");
        on = !(v && !strcmp(v, "off"));
        if (!on) fprintf(stderr, "recomp: season code off (RECOMP_SEASON_CODE=off)\n");
    }
    return on;
}

void season_init(void)
{
    if (!season_code_on()) return;
    tier_a();                            /* the Tier A switch: read once, at the start (tier_a.c) */
    if (season_date_jump()) fprintf(stderr, "recomp: date jump switch on: a new game starts up to %d\n", SEASON_JUMP_LAST);
    const char *v = getenv("RECOMP_START_SEASON");     /* test only, until the Choose Season screen */
    if (!v || !*v) return;
    int y = atoi(v);
    if (y < SEASON_FIRST_YEAR || y > season_last_start_year()) {
        fprintf(stderr, "recomp: RECOMP_START_SEASON=%s is not a year from %d to %d, so 2001 is used%s\n", v,
                SEASON_FIRST_YEAR, season_last_start_year(),
                season_date_jump() ? "" : " (a later year needs the date jump switch RECOMP_DATE_JUMP=1)");
        return;
    }
    if (y > SEASON_NEW_GAME_LAST) fprintf(stderr, "recomp: date jump: a new game in %d\n", y);
    new_year = y;
    rt_season_year = (uint32_t)y;
    fprintf(stderr, "recomp: a new game starts in %d/%02d\n", y, (y + 1) % 100);
}

/* Q358: the season formatters key_nation_format_club_season 0x669630 and key_nation_format_comp_season
 * 0x6697c0 write "%d/%d" (0x9eb024) with (year + 1) % 10 (0x6696d3..0x6696e6, 0x669869..0x669876): "2009/0".
 * With the season code on they write "%d/%02d" with (year + 1) % 100: "2009/10", "2001/02". The format lives
 * in runtime memory. */
uint32_t rt_season_text_mod(uint32_t original) { return season_code_on() ? 100u : original; }

uint32_t rt_season_text_format(uint32_t original)
{
    static uint32_t format;
    if (!season_code_on()) return original;
    if (!format) {
        format = win_static_alloc(8);
        win_put_str(format, "%d/%02d", 8);
    }
    return format;
}

int season_start_year(void) { return (int)rt_season_year; }
int season_game_on(void) { return game_on; }
int season_changing_rules(void) { return rules_now; }
/* Slice 26: the changing rules of the next new game, as create_game sets them (below). */
int season_next_rules(void)
{
    return !season_code_on() ? 0 : new_rules < 0 ? options_bool(OPT_CHANGING_RULES) : new_rules;
}

int season_set_new_game(int start_year, int changing_rules)
{
    if (!season_code_on() || start_year < SEASON_FIRST_YEAR || start_year > season_last_start_year()) return 0;
    new_year = start_year;
    new_rules = changing_rules < 0 ? -1 : !!changing_rules;
    if (game_on) return 1;          /* a game runs: the values apply at its end, to the next new game */
    rt_season_year = (uint32_t)start_year;
    /* The start screen set the game date to 1 January of the old start year (0x81b938). A new game takes its
     * season from the year of that date (key_nation_build_table 0x667150, called at 0x81cf37). */
    if ((int16_t)rd16(g_mem, GAME_DATE + 2) != start_year) {
        uint32_t ecx = cpu.ecx;
        cpu.ecx = GAME_DATE;
        uint32_t args[4] = {1, 0, (uint32_t)start_year, 0xffffffffu};
        rt_callback(DATE_SET, 4, args);        /* it gives back the registers, except while it unwinds */
        if (!rt_unwinding) cpu.ecx = ecx;
    }
    return 1;
}

/* A message box of the game (0x5e8290, cdecl (title, text, 0)) with plain words for the player. The texts
 * live in runtime memory (win_static_alloc), not in the game's heap. */
#define MESSAGE_BOX 0x005e8290u
#define BOX_TEXT 400u
static void season_box(const char *title, const char *text)
{
    static uint32_t title_at, text_at;
    if (!title_at) {
        title_at = win_static_alloc(64);
        text_at = win_static_alloc(BOX_TEXT);
    }
    snprintf((char *)g_mem + title_at, 64, "%s", title);
    snprintf((char *)g_mem + text_at, BOX_TEXT, "%s", text);
    fprintf(stderr, "recomp: season box: %s: %s\n", title, text);
    uint32_t args[3] = {title_at, text_at, 0};
    rt_callback(MESSAGE_BOX, 3, args);
}

/* The part of a game: written only when the game is not 2001 with changing rules off, or when the season
 * code holds news or changed names (season_change.c): a load needs the names in the data (season_change_rename).
 * Also when a player has more than 255 caps (caps_wide.c, in the size): a 2001 game keeps them too. */
static int part_needed(void)
{
    return rt_season_year != SEASON_FIRST_YEAR || rules_now || season_change_part_size() != 0;
}

static int read_at(int fd, int64_t at, void *buf, uint32_t n)
{
    return host_seek(fd, at, SEEK_SET) == at && host_read(fd, buf, n) == (long)n;
}

static int write_at(int fd, int64_t at, const void *buf, uint32_t n)
{
    return host_seek(fd, at, SEEK_SET) == at && host_write(fd, buf, n) == (long)n;
}

/* The byte run code of zwrite 0x932e10 (the same as tools/save/savfile.py encode). */
static uint32_t encode(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t o = 0;
    for (uint32_t i = 0; i < n;) {
        uint32_t j = i + 1;
        while (j < n && in[j] == in[i] && j - i < 0x7f) j++;
        uint32_t count = j - i;
        if (count < 2 && in[i] < 0x81) {
            out[o++] = in[i];
        } else {
            out[o++] = (uint8_t)(count + 0x80);
            out[o++] = in[i];
        }
        i = j;
    }
    return o;
}

/* The decoder of zread 0x932c30: n bytes out of at most room stored bytes. 0 when the bytes end early. */
static int decode(const uint8_t *in, uint32_t room, uint8_t *out, uint32_t n)
{
    uint32_t o = 0, i = 0;
    while (o < n && i < room) {
        uint8_t b = in[i++];
        if (b < 0x81) { out[o++] = b; continue; }
        if (i >= room) return 0;
        for (uint32_t k = 0; k < (uint32_t)(b - 0x80) && o < n; k++) out[o++] = in[i];
        i++;
    }
    return o == n;
}

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* Adds the part to the save file at host path. 0 on success. */
static int add_part(const char *host)
{
    int fd = host_open(host, O_RDWR, 0);
    if (fd < 0) return 1;
    int bad = 1;
    uint8_t head[12], row[ROW_SIZE];
    if (!read_at(fd, 0, head, 12)) goto done;
    uint32_t version = get32(head), count = get32(head + 8);
    if (version < 1 || version > 4 || count > HEADER_ROWS) goto done;
    uint32_t at = count;                        /* the row of the part: a new row, or the row it has */
    for (uint32_t k = 0; k < count; k++) {
        if (!read_at(fd, 12 + (int64_t)k * ROW_SIZE, row, ROW_SIZE)) goto done;
        if (!strncmp((const char *)row + ROW_NAME, PART_NAME, NAME_SIZE)) { at = k; break; }
    }
    if (at == count && count >= MAX_PARTS) goto done;
    uint32_t extra = season_change_part_size(), size = PART_SIZE + extra;
    if (extra > PART_MAX - PART_SIZE) goto done;
    uint8_t *data = malloc(size), *stored = malloc(2 * (size_t)size);
    if (!data || !stored) {
        free(data);
        free(stored);
        goto done;
    }
    put32(data, (uint32_t)season_change_part_form());
    put32(data + 4, size);
    put32(data + 8, rt_season_year);
    put32(data + 12, (uint32_t)rules_now);
    if (extra) season_change_part_write(data + PART_SIZE);
    /* Test switch (tests/seasons/save_crash_run.py): the program stops here, as 0.6.0 did on Linux when the
     * write went past the block (glibc free(), 0.6.1 item 10). */
    if (getenv("RECOMP_TEST_PART_CRASH")) abort();
    uint32_t n = size;
    if (version == 2 || version == 4) n = encode(data, size, stored);
    else memcpy(stored, data, size);
    free(data);
    int64_t end = host_seek(fd, 0, SEEK_END);
    /* The row holds an unsigned 32-bit offset: the part must end below 4 GB, as the parts of the game (files.c). */
    if (end < 12 + (int64_t)HEADER_ROWS * ROW_SIZE || end + n > 0xfffffffe) goto done;
    /* The bytes first, then the row, then the count: a write that stops half way leaves the old file whole.
     * A part that is there already gets the new bytes at the end, and its row points to them. */
    int wrote = write_at(fd, end, stored, n);
    free(stored);
    if (!wrote) goto done;
    memset(row, 0, sizeof row);
    put32(row, (uint32_t)end);
    put32(row + 4, size);
    memcpy(row + ROW_NAME, PART_NAME, sizeof PART_NAME);
    if (!write_at(fd, 12 + (int64_t)at * ROW_SIZE, row, ROW_SIZE)) goto done;
    if (at == count) {
        put32(head + 8, count + 1);
        if (!write_at(fd, 8, head + 8, 4)) goto done;
    }
    bad = 0;
done:
    host_close(fd);
    return bad;
}


/* Reads the part of the save file at host path into year and rules. 1 when the file has the part, 0 when it
 * has none (or the file is no container: the original's load then reports that itself), -1 when the part is
 * damaged. A part of a later version is read: its first fields keep their meaning (see the top). With news 1
 * the news texts of version 2 go to season_change.c (a load); a damaged news field makes the part damaged. */
static int read_part(const char *host, int *year, int *rules, int news)
{
    int fd = host_open(host, O_RDONLY, 0);
    if (fd < 0) return 0;
    int result = 0;
    uint8_t head[12], row[ROW_SIZE], *stored = NULL, *data = NULL;
    if (!read_at(fd, 0, head, 12)) goto done;
    uint32_t version = get32(head), count = get32(head + 8);
    if (version < 1 || version > 4 || count > HEADER_ROWS) goto done;
    for (uint32_t k = 0; k < count; k++) {
        if (!read_at(fd, 12 + (int64_t)k * ROW_SIZE, row, ROW_SIZE)) goto done;
        if (strncmp((const char *)row + ROW_NAME, PART_NAME, NAME_SIZE)) continue;
        result = -1;
        uint32_t offset = get32(row), size = get32(row + 4);
        if (size < 16 || size > PART_MAX) break;
        int64_t end = host_seek(fd, 0, SEEK_END);
        if (end <= (int64_t)offset) break;
        uint32_t room = end - offset < 2 * (int64_t)PART_MAX ? (uint32_t)(end - offset) : 2 * PART_MAX;
        stored = malloc(room);
        data = malloc(size);
        if (!stored || !data || !read_at(fd, offset, stored, room)) break;
        if (version == 2 || version == 4) {
            if (!decode(stored, room, data, size)) break;
        } else {
            if (room < size) break;
            memcpy(data, stored, size);
        }
        uint32_t form = get32(data), length = get32(data + 4);
        if (form < 1 || length < 16 || length > size) break;
        uint32_t y = get32(data + 8), r = get32(data + 12);
        if (y > SEASON_YEAR_MAX || !season_year_ok((int)y) || r > 1) break;   /* the first test: no u32 to int wrap */
        /* Version 2 and every later version hold whole news fields after +16 (see the top). A load that
         * cannot read them is refused: the changed names in them are needed to load the game's data
         * (season_change_rename). */
        if (news && form >= PART_VERSION_NEWS &&
            (length < PART_SIZE + 12 || !season_change_part_read(data + PART_SIZE, length - PART_SIZE, form))) {
            season_change_reset();
            break;
        }
        *year = (int)y;
        *rules = (int)r;
        result = 1;
        break;
    }
done:
    free(stored);
    free(data);
    host_close(fd);
    return result;
}

/* The start year of a save file for the Choose Season screen (slice 13): only reads the file. */
int season_of_save(const char *host, int *year)
{
    int rules = 0;
    *year = SEASON_FIRST_YEAR;
    return read_part(host, year, &rules, 0);
}

void o_0082e430(void);

/* 1 while the original's save 0x82e430 runs; part_added when the part is in its file (save_last_part). */
static int saving, part_added;

void o_008395b0(void);

/* save_fifa_transfer_rules, cdecl (archive): the last part of every save (one caller, 0x82f5b0). The archive
 * starts with the name of its file, the name that the rename at 0x82f684 moves: temp_save_file.sav with the
 * option bit 0x20000000 (a fresh game.cfg has it), else the name of the save. The part goes in here, before the
 * save deletes the old file (0x82f5e3) and renames the new one (0x82f684): a program that stops while the part
 * goes in leaves the old save whole, and only a whole new file takes its name. Before 0.6.1 the part went in
 * after the rename, so a crash at that point left a save of renamed records without its part, which loads to
 * the box "index.cpp 5809" (0.6.1 items 4 and 10). */
RT_WRAP(008395b0, save_last_part)
{
    char path[1024], host[2048];
    snprintf(path, sizeof path, "%.1023s", win_str(rt_arg(0)));
    o_008395b0();
    if (rt_unwinding || !saving || !part_needed()) return;
    if (win_existing_file(path, host, sizeof host) && !add_part(host)) part_added = 1;
}

/* A save above 4 GB fails (files.c, win_save_begin): the box says so, the result is that of a failed save. */
static int save_too_big(void)
{
    int failed = win_save_end();
    if (!failed) return 0;
    if (rt_unwinding) return 1;
    season_box("Save Game", failed == 1 ? "The game is not saved: the saved game would be larger than 4 GB. Your "
               "earlier saved game is not changed." : "The game is not saved: the saved game would be larger than "
               "4 GB. The game deleted the earlier saved game of this name before it saved.");
    cpu.eax = 0;
    return 1;
}

/* The save of a game, cdecl (path, flags): 1 when the file is written. The file at path is whole when it
 * returns: with the option bit 0x20000000 the save writes temp_save_file.sav and renames it to path first
 * (0x82e5cb, 0x82f684). The test saves of testrun.c and the Auto Save (0x7617d8) come here too. */
RT_WRAP(0082e430, save_game)
{
    win_save_begin();
    if (!season_code_on()) {
        o_0082e430();
        save_too_big();
        return;
    }
    char path[1024];
    snprintf(path, sizeof path, "%.1023s", win_str(rt_arg(0)));
    saving = 1, part_added = 0;
    season_calendar_log_entry("save");
    o_0082e430();
    saving = 0;
    if (save_too_big() || rt_unwinding) return;
    if (cpu.eax != 1 || !part_needed() || part_added) return;
    /* The part is not in the file yet: a save that did not reach its last part, or an add that failed. */
    char host[2048];
    if (win_existing_file(path, host, sizeof host) && !add_part(host)) return;
    /* The file stays a whole save without the part, which would load as a 2001/02 game: the save fails. */
    fprintf(stderr, "cm0102_recomp: the season of this game (%u, changing rules %s) could not be added to the "
                    "save %s.\n", rt_season_year, rules_now ? "on" : "off", path);
    char text[BOX_TEXT];
    snprintf(text, sizeof text, "The game could not write its season (%u/%02u) into the saved game. Do not use "
             "this save: it would load as a 2001/02 game. Save the game again, perhaps under another name.",
             rt_season_year, (rt_season_year + 1) % 100);
    season_box("Save Game", text);
    if (!rt_unwinding) cpu.eax = 0;             /* the result of a failed save */
}

#define RECORD 0x6bu
#define MAX_RECORDS 8192

static const char *exe_name(uint32_t address) { return win_str(address); }

/* 1 when the save at host, which has no season part, lacks a key competition name: its load would stop at the
 * box "index.cpp 5809" (season_load_check.c). It reads the long names of club_comp.dat and nation_comp.dat.
 * Only for the English names: set_key_ids takes the records' names when the language of the names [0x9b5984]
 * is 0 (0x603c52), else the names of eng.lng, which no rename changes. A save keeps that language as the last
 * 4 bytes of index.dat (written at 0x531488 before the close at 0x531499, read at 0x52894b in database_load).
 * 0 when unsure. */
static int lacks_key_comps(const char *host)
{
    int fd = host_open(host, O_RDONLY, 0);
    if (fd < 0) return 0;
    static char store[MAX_RECORDS][52];
    static const char *names[MAX_RECORDS];
    int count = 0, files = 0, english = 0;
    uint8_t head[12], row[ROW_SIZE], *stored = NULL, *data = NULL;
    if (!read_at(fd, 0, head, 12)) goto done;
    uint32_t version = get32(head), rows = get32(head + 8);
    if (version < 1 || version > 4 || rows > HEADER_ROWS) goto done;
    for (uint32_t k = 0; k < rows; k++) {
        if (!read_at(fd, 12 + (int64_t)k * ROW_SIZE, row, ROW_SIZE)) goto done;
        const char *name = (const char *)row + ROW_NAME;
        int index = !strncmp(name, "index.dat", NAME_SIZE);
        if (!index && strncmp(name, "club_comp.dat", NAME_SIZE) && strncmp(name, "nation_comp.dat", NAME_SIZE)) continue;
        uint32_t offset = get32(row), size = get32(row + 4);
        if (size > MAX_RECORDS * RECORD || (index && size > INDEX_MAX)) goto done;
        uint32_t room = version == 2 || version == 4 ? 2 * size : size;
        free(stored), free(data);
        stored = malloc(room + 1), data = malloc(size + 1);
        if (!stored || !data) goto done;
        int64_t end = host_seek(fd, 0, SEEK_END);
        if (end < (int64_t)offset) goto done;
        if ((int64_t)offset + room > end) room = (uint32_t)(end - offset);
        if (!read_at(fd, offset, stored, room)) goto done;
        if (version == 2 || version == 4) {
            if (!decode(stored, room, data, size)) goto done;
        } else {
            if (room < size) goto done;
            memcpy(data, stored, size);
        }
        if (index) {
            english = size >= 4 && get32(data + size - 4) == 0;
            continue;
        }
        for (uint32_t r = 0; r + RECORD <= size && count < MAX_RECORDS; r += RECORD, count++) {
            memcpy(store[count], data + r + 4, 51);
            store[count][51] = 0;
            names[count] = store[count];
        }
        files++;
    }
done:
    free(stored);
    free(data);
    host_close(fd);
    int missing = files == 2 && english ? season_key_comps_missing(names, count, exe_name) : 0;
    if (missing)
        fprintf(stderr, "recomp: the save %s has no season part and lacks %d key competition names\n", host, missing);
    return missing > 0;
}

void o_0082a9c0(void);

/* The load of a saved game, cdecl (path): nonzero when the game is loaded. The season comes from the save
 * before any code of the load runs: the load builds the league table from the game year (0x82b845). */
RT_WRAP(0082a9c0, load_game)
{
    season_dates_forget();                      /* slice 11b: the data comes from the save */
    caps_wide_reset();                          /* 0.6.2 slice 12: the season part below gives the caps back */
    if (!season_code_on()) {
        o_0082a9c0();
        if (!rt_unwinding && cpu.eax) game_on = 1;
        return;
    }
    char host[2048], path[1024];
    int year = SEASON_FIRST_YEAR, rules = 0;
    snprintf(path, sizeof path, "%.1023s", win_str(rt_arg(0)));
    season_change_reset();
    int found = win_existing_file(path, host, sizeof host), part = found ? read_part(host, &year, &rules, 1) : 0;
    /* A save of renamed records without its part (in 0.6.0, a crash after the rename of the save and before
     * the part went in: 0.6.1 items 4 and 10) cannot give the names back, and its load would stop at the box
     * "index.cpp 5809" (season_load_check.c). The load refuses it before any code of the original runs. */
    if (found && part == 0 && lacks_key_comps(host)) {
        season_change_reset();
        fprintf(stderr, "cm0102_recomp: the save %s has renamed competitions but no season part, so the game does "
                "not load it.\n", path);
        season_box("Load Game", "This save is incomplete. Load an earlier save.");
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    if (part < 0) {
        season_change_reset();
        caps_wide_reset();
        /* A damaged part: the season of the game is not known, so the game is not loaded. The load fails
         * before any code of the original runs, as its own early failures do (0x82aa28), and the callers
         * report a failed load (for example 0x81bef1). */
        fprintf(stderr, "cm0102_recomp: the season part of the save %s is damaged, so the game does not load it.\n",
                path);
        season_box("Load Game", "This saved game cannot be loaded. Its season data is damaged, or it comes from "
                   "a newer version of the Bosman Edition.");
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    rt_season_year = (uint32_t)year;
    rules_now = rules;
    if (year != SEASON_FIRST_YEAR || rules)
        fprintf(stderr, "recomp: the save is a game of %d/%02d with changing rules %s\n", year, (year + 1) % 100,
                rules ? "on" : "off");
    o_0082a9c0();
    if (rt_unwinding) return;
    if (cpu.eax) {
        game_on = 1;
        season_change_loaded();
        season_calendar_log_entry("load");
    } else {                                    /* a failed load: back to the values of a new game */
        rt_season_year = (uint32_t)new_year;
        rules_now = 0;
        season_change_reset();
        caps_wide_reset();
    }
}

void o_00828110(void);

/* The creation of a new game, cdecl (flag): nonzero when the game is made. Its changing rules are those of
 * the last season_set_new_game, or the option. */
RT_WRAP(00828110, create_game)
{
    rt_season_year = (uint32_t)new_year;
    rules_now = !season_code_on() ? 0 : new_rules < 0 ? options_bool(OPT_CHANGING_RULES) : new_rules;
    season_change_reset();
    caps_wide_reset();
    season_dates_at_create();                   /* slice 11b: the dates of the loaded data fit this year */
    season_intl_at_create();                    /* slice 27: Australia in Asia from 2006/07 */
    season_change_creating(1);
    season_divisions_test_extra();              /* slice 20 test switch, before the trims of create_game (item 226) */
    o_00828110();
    season_change_creating(0);
    if (!rt_unwinding && cpu.eax) {
        game_on = 1;
        season_change_new_game();
    }
}

void o_0082ff40(void);

/* The end of a game, cdecl (flag): it frees the game and sets the game date to 1 January of the start
 * year (0x8304ec). The values go back to those of a new game first, so that date is the new game's. */
RT_WRAP(0082ff40, free_game)
{
    net_session_game_ends();                    /* 0.7.0 slice 6: the game ends */
    game_on = 0;
    season_dates_forget();
    rt_season_year = (uint32_t)new_year;
    rules_now = 0;
    season_change_reset();
    caps_wide_reset();
    o_0082ff40();
}

#define FIXTURES 0xae2a58u         /* the fixture store: one block per calendar year, +0x1130 s16 year,
                                    * +0x112c the next block (holiday_next.c); the last points to the first */

/* Test log (RECOMP_SEASON_LOG=1): at each human turn where it changes, the game date, the start year and
 * the calendar years of the fixture store. A game that starts in 2010 must hold no year before 2010. It
 * only reads memory. */
void season_log_turn(void)
{
    static int on = -1;
    static char last[512];
    if (on < 0) on = getenv("RECOMP_SEASON_LOG") && *getenv("RECOMP_SEASON_LOG");
    if (!on) return;
    char now[512];
    size_t n = (size_t)snprintf(now, sizeof now, "start year %u, changing rules %s, fixture years", rt_season_year,
                                rules_now ? "on" : "off");
    /* Every 32-bit address lies inside g_mem (4 GB and a page, rt_init), so b + 0x1130 stays inside. */
    uint32_t store = rd32(g_mem, FIXTURES);
    for (uint32_t b = store, k = 0; b && k < 100 && n < sizeof now - 8; b = rd32(g_mem, b + 0x112c), k++) {
        if (k && b == store) break;            /* the blocks form a ring */
        n += (size_t)snprintf(now + n, sizeof now - n, " %d", (int16_t)rd16(g_mem, b + 0x1130));
    }
    if (!strcmp(now, last)) return;
    snprintf(last, sizeof last, "%s", now);
    fprintf(stderr, "recomp: season log: date %d/%d, %s\n", (int16_t)rd16(g_mem, GAME_DATE),
            (int16_t)rd16(g_mem, GAME_DATE + 2), now);
}
