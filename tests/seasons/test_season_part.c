/* The whole season part of a save (season_change.c): the write puts exactly the bytes that the size gives, and
 * a part read back writes the same bytes. The save (season.c add_part) allocates PART_SIZE + the size with
 * malloc and the write fills it, so a write past the size overruns that heap block. In 0.6.0 the empty row A18
 * list at the end of the part stored a count of 4 bytes that the size left out (0.6.1 items 4 and 10). On glibc
 * (Linux) that overrun hits the header of the next chunk when the block has no slack: a request of 16 * k + 5 to
 * 16 * k + 8 bytes. free() then stops the program ("double free or corruption"), after the original save wrote
 * the file and before the part went in, and the load of that save stops at the box "index.cpp 5809". A 2001/02
 * game first has news and renames in its part at the 2002 season change, so its saves from about June 2002
 * crashed on Linux. The test runs parts with 0 to 15 bytes of news text, so the block size takes every value
 * mod 16. ctest season_part. */
#include "win.h"
#include "season_rules.h"
#include "season_change.h"
#include "caps_wide.h"
#include "season_intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The game side that the part functions never call. */
Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args) { (void)target, (void)nargs, (void)args; return 0; }
uint32_t win_static_alloc(uint32_t size) { (void)size; return 0; }
void o_005cc510(void) {}
void o_00685640(void) {}
void o_005d5ae0(void) {}
void o_0060efd0(void) {}
void o_0077bc80(void) {}
void news_text_written(uint32_t item, uint32_t text, uint32_t full) { (void)item, (void)text, (void)full; }
void o_00782b70(void) {}
void o_00784100(void) {}
void o_00784eb0(void) {}
int season_start_year(void) { return 2001; }
int season_game_changes(void) { return 1; }
int season_changing_rules(void) { return 1; }
int season_code_on(void) { return 1; }
int season_day_of_year(int y, int m, int d) { (void)y, (void)m; return d; }
void season_from_day_of_year(int y, int doy, int *m, int *d) { (void)y; *m = 1, *d = doy; }
int season_fifa_day(int y, int m, int want) { (void)y, (void)m; return want; }
int season_intl_finals_busy(int conf, int y, int doy) { (void)conf, (void)y, (void)doy; return 0; }
long season_abs_day(int y, int m, int d) { return (long)y * 372 + m * 31 + d; }
int season_conmebol_real_day(long z) { (void)z; return 0; }
void season_from_abs_day(long z, int *y, int *m, int *d)
{
    *y = (int)(z / 372), *m = (int)(z % 372 / 31), *d = (int)(z % 31);
}
int season_gold_cup_busy(int y, int doy) { (void)y, (void)doy; return 0; }
const SeasonCompClass season_comp_classes[1];
const int season_comp_class_count = 0;
const SeasonCompClass *season_comps_class(uint32_t vtable) { (void)vtable; return NULL; }
const char *season_comps_file(uint32_t obj) { (void)obj; return NULL; }
const char *season_comps_file_of(uint32_t obj) { (void)obj; return NULL; }
void season_comps_reset(void) {}
void season_cup_groups_reset(void) {}
void season_intl_qual_reset(void) {}
void season_uefa_league_reset(void) {}
void season_playoffs_hold(void) {}
void season_playoffs_release(void) {}
void season_new_cups_day(void) {}
const SeasonRuleKind season_kind_club_cups, season_kind_club_world, season_kind_comps, season_kind_cup_breaks,
    season_kind_cups, season_kind_divisions, season_kind_intl, season_kind_ita_serie_c, season_kind_names,
    season_kind_nation, season_kind_new_cups, season_kind_playoffs, season_kind_sizes, season_kind_subs,
    season_kind_uefa, season_kind_calendar;

uint32_t season_change_part_size(void);
void season_change_part_write(uint8_t *out);
void season_change_reset(void);

#define NAME_ROOM 51u
#define ROOM 1024u
#define MARK 0xa5

static int failures;

static void check(int ok, const char *text)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", text);
    if (!ok) failures++;
}

static uint32_t put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); return 4; }

/* A part as a save of a 2001/02 game after the 2002 change holds it: one news field of 2002 with len bytes of
 * text, two renames (the French first and second division), the season point 2002 and an empty row A18 list,
 * which has no bytes. */
static uint32_t make_part(uint8_t *p, uint32_t len)
{
    uint32_t at = put32(p, 1);
    at += put32(p + at, 2002);
    at += put32(p + at, 0);
    at += put32(p + at, len);
    for (uint32_t i = 0; i < len; i++) p[at++] = (uint8_t)('a' + i);
    at += put32(p + at, 2);
    const char *names[2] = {"Division 1", "Division 2"};
    for (uint32_t r = 0; r < 2; r++) {
        at += put32(p + at, 40 + r);
        memset(p + at, 0, NAME_ROOM);
        memcpy(p + at, names[r], strlen(names[r]));
        at += NAME_ROOM;
    }
    at += put32(p + at, 2002);
    return at;
}

/* 0.6.2 slice 12 (fault 30): caps above 255 stay in the byte as 255 and in the store, and the season part of version
 * 3 keeps them. */
static void caps_test(void)
{
    char text[200];
    caps_wide_reset();
    uint32_t b = 253;
    b = caps_wide_add(7, b);
    check(b == 254 && caps_wide_full(7, b) == 254 && !caps_wide_count(), "caps 253 + 1: byte 254, nothing stored");
    b = caps_wide_add(7, b);
    check(b == 255 && caps_wide_full(7, b) == 255 && !caps_wide_count(), "caps 254 + 1: byte 255, nothing stored");
    b = caps_wide_add(7, b);
    check(b == 255 && caps_wide_full(7, b) == 256, "caps 255 + 1: byte 255 (not 0, so no second first cap), 256 caps");
    b = caps_wide_add(7, b);
    check(b == 255 && caps_wide_full(7, b) == 257 && caps_wide_count() == 1, "caps 256 + 1: 257 caps, one player");
    check(caps_wide_full(8, 255) == 255 && caps_wide_full(8, 40) == 40, "another player: his byte");
    check(caps_wide_full(7, 12) == 12, "a byte below 255 is the caps, whatever is stored");
    check(caps_wide_add(7, 254) == 255 && caps_wide_full(7, 255) == 255 && !caps_wide_count(),
          "a byte that reaches 255 again (a staff id used again): the old count goes");
    for (int i = 0; i < 300; i++) caps_wide_add(9, 255);
    check(caps_wide_full(9, 255) == 555, "300 caps after 255: 555");

    /* A part of version 2 with no caps writes the same bytes as before: the 0.6.1 layout. */
    {
        uint8_t in[ROOM], out[ROOM];
        caps_wide_reset();
        uint32_t n = make_part(in, 5);
        int read = season_change_part_read(in, n, 2);
        check(read == 1 && season_change_part_form() == 2 && season_change_part_size() == n,
              "a 0.6.1 part (version 2): read, version 2 again, same size");
        if (read) season_change_part_write(out);
        check(!memcmp(in, out, n) && !caps_wide_count(), "a 0.6.1 part: same bytes back, no caps above 255");
    }
    /* Version 3: news, renames, an empty row A18 list (its count 0 now written) and two players. */
    for (int list = 0; list < 2; list++) {
        static uint8_t in[ROOM], out[ROOM], again[ROOM];
        uint32_t at = make_part(in, 3);
        at += put32(in + at, (uint32_t)list);
        for (int e = 0; e < list; e++) {
            memset(in + at, 0, SEASON_FRIENDLY_ENTRY);
            put32(in + at, 99);
            at += SEASON_FRIENDLY_ENTRY;
        }
        uint32_t caps_at = at;
        at += put32(in + at, 2);
        at += put32(in + at, 1234);
        at += put32(in + at, 256);
        at += put32(in + at, 77);
        at += put32(in + at, 301);
        int read = season_change_part_read(in, at, 3);
        snprintf(text, sizeof text, "version 3 with %d row A18 entries: read, caps 256 and 301", list);
        check(read == 1 && caps_wide_full(1234, 255) == 256 && caps_wide_full(77, 255) == 301, text);
        uint32_t size = season_change_part_size();
        memset(out, MARK, sizeof out);
        if (read && size <= ROOM) season_change_part_write(out);
        int past = 0;
        for (uint32_t i = size; i < ROOM; i++)
            if (out[i] != MARK) past++;
        snprintf(text, sizeof text, "version 3 with %d row A18 entries: version 3, size %u of %u, same bytes back, "
                 "%d past", list, size, at, past);
        check(season_change_part_form() == 3 && size == at && !past && !memcmp(in, out, at), text);
        /* A reader of version 2 (the read of 0.6.0 and 0.6.1: the list to its end) takes the news, names and list. */
        caps_wide_reset();
        read = season_change_part_read(in, at, 2);
        snprintf(text, sizeof text, "version 3 with %d row A18 entries, read as version 2: read, the caps left", list);
        check(read == 1 && !caps_wide_count(), text);
        if (read) season_change_part_write(again);
        uint32_t kept = list ? caps_at : caps_at - 4;   /* version 2 writes no count for an empty list */
        check(season_change_part_size() == kept && !memcmp(in, again, kept),
              "  and its news, names and row A18 list are those of the part");
        /* Damaged caps fields. */
        put32(in + caps_at, 3);
        check(season_change_part_read(in, at, 3) == 0 && !caps_wide_count(), "a caps count larger than the part: damaged");
        put32(in + caps_at, 2);
        put32(in + caps_at + 8, 200);
        check(season_change_part_read(in, at, 3) == 0, "a stored count of 255 or less: damaged");
        put32(in + caps_at + 8, 256);
        put32(in + caps_at + 12, 1234);
        check(season_change_part_read(in, at, 3) == 0, "one player twice: damaged");
        put32(in + caps_at + 12, 77);
        put32(in + at, 0xdeadbeefu);
        check(season_change_part_read(in, at + 4, 3) == 0, "bytes after the caps in version 3: damaged");
        put32(in + at, 0);                    /* version 4 and later: an empty goals field after the caps */
        put32(in + at + 4, 0);                /* version 5: an empty marks field (0.6.2 slice 4b) */
        put32(in + at + 8, 0xdeadbeefu);
        check(season_change_part_read(in, at + 12, 6) == 1 && caps_wide_full(77, 255) == 301,
              "  but a later version keeps its own fields after the caps, goals and marks");
    }
    caps_wide_reset();
}

/* 0.6.2 item 66: international goals above 255 stay in the byte as 255 and in the store, and version 4 keeps them
 * after the caps field. */
static void goals_test(void)
{
    char text[200];
    caps_wide_reset();
    uint32_t b = goals_wide_add(5, 250, 3);
    check(b == 253 && goals_wide_full(5, b) == 253 && !goals_wide_count(), "goals 250 + 3: byte 253, nothing stored");
    b = goals_wide_add(5, b, 2);
    check(b == 255 && goals_wide_full(5, b) == 255 && !goals_wide_count(), "goals 253 + 2: byte 255, nothing stored");
    b = goals_wide_add(5, 253, 4);
    check(b == 255 && goals_wide_full(5, b) == 257, "goals 253 + 4: byte 255 (not 1), 257 goals");
    b = goals_wide_add(5, b, 0);
    check(b == 255 && goals_wide_full(5, b) == 257, "goals 257 + 0: 257");
    b = goals_wide_add(5, b, 3);
    check(b == 255 && goals_wide_full(5, b) == 260 && goals_wide_count() == 1, "goals 257 + 3: 260, one player");
    check(caps_wide_full(5, 255) == 255 && !caps_wide_count(), "goals are not caps");
    check(goals_wide_add(5, 100, 1) == 101 && !goals_wide_count(), "a byte below 255 again: the old count goes");

    /* Version 4: the caps field (here empty) and then the goals. */
    static uint8_t in[ROOM], out[ROOM];
    uint32_t at = make_part(in, 2);
    at += put32(in + at, 0);                  /* the row A18 list */
    uint32_t caps_at = at;
    at += put32(in + at, 1);
    at += put32(in + at, 31);
    at += put32(in + at, 280);
    uint32_t goals_at = at;
    at += put32(in + at, 1);
    at += put32(in + at, 31);
    at += put32(in + at, 300);
    int read = season_change_part_read(in, at, 4);
    check(read == 1 && caps_wide_full(31, 255) == 280 && goals_wide_full(31, 255) == 300,
          "version 4: read, 280 caps and 300 goals");
    uint32_t size = season_change_part_size();
    memset(out, MARK, sizeof out);
    if (read && size <= ROOM) season_change_part_write(out);
    snprintf(text, sizeof text, "version 4: version %d, size %u of %u, same bytes back", season_change_part_form(),
             size, at);
    check(season_change_part_form() == 4 && size == at && !memcmp(in, out, at), text);
    put32(in + caps_at, 0);                   /* no caps above 255: the goals alone */
    memmove(in + caps_at + 4, in + goals_at, at - goals_at);
    at -= goals_at - caps_at - 4;
    read = season_change_part_read(in, at, 4);
    check(read == 1 && !caps_wide_count() && goals_wide_full(31, 255) == 300, "version 4 with goals only: read");
    size = season_change_part_size();
    if (read && size <= ROOM) season_change_part_write(out);
    check(season_change_part_form() == 4 && size == at && !memcmp(in, out, at),
          "version 4 with goals only: version 4, same bytes back (the caps count 0 is written)");
    put32(in + at - 4, 255);
    check(season_change_part_read(in, at, 4) == 0, "a stored goal count of 255: damaged");
    caps_wide_reset();
}

/* Version 5 (0.6.2 slice 4b): the marks of qualifying campaigns in the original's shape, after the caps and goals
 * fields. A mark holds the record id (the slot of the table [0xadadfc]) and the campaign year +0x40 of the object. */
static void marks_test(void)
{
    static uint8_t in[ROOM], out[ROOM];
    char text[200];
    const uint32_t table = 0x100000u, objects = 0x200000u;
    g_mem = calloc(0xb00000u, 1);
    if (!g_mem) {
        check(0, "version 5: memory");
        return;
    }
    wr32(g_mem, 0xadadfcu, table);
    wr32(g_mem, 0xadadf8u, 8);
    for (uint32_t id = 0; id < 8; id++) {
        wr32(g_mem, table + 4u * id, objects + 0x100u * id);
        wr16(g_mem, objects + 0x100u * id + 0x40u, (uint16_t)(2090 + id));
    }
    uint32_t at = make_part(in, 2);
    at += put32(in + at, 0);                  /* the row A18 list */
    at += put32(in + at, 0);                  /* no caps above 255 */
    at += put32(in + at, 0);                  /* no goals above 255 */
    at += put32(in + at, 2);                  /* two marks: record 3 in 2093 and record 5 in 2095 */
    at += put32(in + at, 3);
    at += put32(in + at, 2093);
    at += put32(in + at, 5);
    at += put32(in + at, 2095);
    int read = season_change_part_read(in, at, 5);
    check(read == 1 && season_intl_old_shape(objects + 0x300u) && season_intl_old_shape(objects + 0x500u) &&
              !season_intl_old_shape(objects + 0x400u), "version 5: read, records 3 and 5 keep the original's shape");
    uint32_t size = season_change_part_size();
    memset(out, MARK, sizeof out);
    if (read && size <= ROOM) season_change_part_write(out);
    snprintf(text, sizeof text, "version 5: version %d, size %u of %u, same bytes back", season_change_part_form(),
             size, at);
    int past = 0;
    for (uint32_t i = size; i < ROOM; i++)
        if (out[i] != MARK) past++;
    check(season_change_part_form() == 5 && size == at && !past && !memcmp(in, out, at), text);
    wr16(g_mem, objects + 0x500u + 0x40u, 2099);  /* record 5 starts its next campaign: its mark goes */
    check(!season_intl_old_shape(objects + 0x500u) && season_change_part_size() == at - 8,
          "version 5: a mark whose object moved to another year is not kept");
    wr16(g_mem, objects + 0x300u + 0x40u, 2097);
    check(season_change_part_form() < 5, "version 5: with no live mark the part keeps its old version");
    season_intl_mark_old(objects + 0x600u);
    check(season_intl_old_shape(objects + 0x600u) && season_change_part_form() == 5,
          "version 5: the load repair marks record 6 in its year 2096");
    read = season_change_part_read(in, at - 20, 4);   /* the same part without the marks field, as version 4 */
    check(read == 1 && !season_intl_old_shape(objects + 0x600u), "version 4 part: no marks");
    uint32_t many = at - 20;                  /* 17 whole marks: more than the 16 that the reader keeps */
    many += put32(in + many, 17);
    for (uint32_t i = 0; i < 17; i++) {
        many += put32(in + many, i % 8);
        many += put32(in + many, 2090 + i % 8);
    }
    check(season_change_part_read(in, many, 5) == 0, "version 5: 17 marks (all whole): damaged");
    free(g_mem);
    g_mem = NULL;
    season_change_reset();
}

int main(void)
{
    char text[200];
    for (uint32_t len = 0; len < 16; len++) {
        uint8_t in[ROOM], out[ROOM];
        uint32_t n = make_part(in, len);
        season_change_reset();
        int read = season_change_part_read(in, n, 2);
        uint32_t size = season_change_part_size();
        memset(out, MARK, sizeof out);
        if (read && size <= ROOM) season_change_part_write(out);
        int past = 0;
        for (uint32_t i = size; i < ROOM; i++)
            if (out[i] != MARK) past++;
        /* The block of add_part: 16 bytes of head, then this part. */
        snprintf(text, sizeof text, "news text of %2u bytes: block of %u bytes (mod 16 = %2u), size %u, "
                 "%d bytes written past it, same bytes back", len, 16 + size, (16 + size) % 16, size, past);
        check(read == 1 && size == n && !past && !memcmp(in, out, n), text);
    }
    /* 0.6.2 faults 23 and 31: a part with news in each season from 2001 to 2301 (301 seasons, more than 200) and the
     * season point 2301 reads back, and writes the same bytes. Up to 0.6.1 the read refused more than 200 seasons and
     * a season or point after 2100. */
    {
        static uint8_t in[16384], out[16384];
        uint32_t at = put32(in, 301);
        for (uint32_t y = 2001; y <= 2301; y++) {
            at += put32(in + at, y);
            at += put32(in + at, 1);
            at += put32(in + at, 3);
            memcpy(in + at, "abc", 3);
            at += 3;
        }
        at += put32(in + at, 0);
        at += put32(in + at, 2301);
        season_change_reset();
        int read = season_change_part_read(in, at, 2);
        uint32_t size = season_change_part_size();
        if (read && size <= sizeof out) season_change_part_write(out);
        snprintf(text, sizeof text, "news in 301 seasons, 2001 to 2301, season point 2301: read %d, size %u of %u, "
                 "same bytes back", read, size, at);
        check(read == 1 && size == at && !memcmp(in, out, at), text);
        put32(in, 302);
        season_change_reset();
        check(season_change_part_read(in, at, 2) == 0, "a news count larger than the part holds: damaged");
    }
    caps_test();
    goals_test();
    marks_test();
    season_change_reset();
    caps_wide_reset();
    check(season_change_part_size() == 0, "after a reset: no part");
    printf("%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
