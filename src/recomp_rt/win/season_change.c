/* The season change and its news item (season_change.h). Evidence: docs/research/2026-10-17-season-change.md.
 *
 * Game memory that this file reads:
 * - g_comp_object_table [0xadadfc] with [0xadadf8] slots, and the league table entries at 0xb63d60 (35 of
 *   0x48 bytes): +0 nation record, +0xc competition count, +0x10 competition array, +0x15 update day (day of
 *   the year, as the game date +0; game_season_update_pass 0x5d5ae0 compares them at 0x5d5bc8), +0x35 season
 *   year (s16, moved at 0x5d5c0f).
 * - a competition object: +0 vtable, +4 record (club_comp.dat or nation_comp.dat, 0x6b bytes, from
 *   [0xae23d0]; the long name at +4), +0x40 season year (s16; start_new_season adds 1, for example 0x574c10).
 *
 * The news item: NewsItem::NewsItem 0x77bbe0 (thiscall: type, important flag). An item has 0xdf bytes (the
 * fixture store copies 0xdf, 0x5aaed0; 0x6842a8 allocates 0xdf): +0 type, +4 flag, +5 fifty slots of 4 bytes,
 * +0xcd date (s16 day of the year, s16 year), +0xd5 day slice, +0xd6, +0xda id, +0xde scope (written at
 * 0x786802 by News::send_to_all_managers). NewsItem::set_slot 0x785790 writes a slot. News::send_to_all_managers
 * 0x786780 on the news store [0xb7431c] copies the item into each human manager's inbox
 * (News::deliver_to_manager 0x785d40). HostCountry::post_host_news 0x5faec0 makes its news in the same way
 * (0x5faf66, 0x5faf81, 0x5fb049). The type NEWS_TYPE is new: NewsItem::get_category 0x785670 puts types 1
 * to 999 in category 2, the Competitions filter of the news screen (news_screens_build_news_screen 0x7881e0,
 * the label at 0x788614 and the value 2 at 0x788630). The original makes types up to 0x2f in that range. Code
 * that compares a news type is in NewsItem::write_text 0x77bc80, NewsItem::get_action_button 0x782b70,
 * NewsItem::is_important_for 0x784eb0, NewsItem::auto_respond 0x784100 and get_category (a search of the
 * compares with the first type of each range, 0x3e8, 0x7d0, 0xbb8, 0xfa0, 0x1388, 0x1770; the other hits
 * are money and counts). The runtime answers the first four for this type, so no switch of the original
 * meets it. The news screen build calls only write_text, get_action_button and NewsItem::get_date 0x789280
 * with an item. */
#include "win.h"
#include "season_comps.h"
#include "season.h"
#include "season_change.h"
#include "news_text.h"
#include "season_divisions.h"
#include "season_comps.h"
#include "season_cups.h"
#include "season_playoffs.h"
#include "season_rules.h"
#include "season_intl.h"
#include "season_uefa_league.h"
#include "caps_wide.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The rule kinds, in the order they run. Each later slice adds its kinds here (season_change.h). */
extern const SeasonRuleKind season_kind_names;
extern const SeasonRuleKind season_kind_sizes;
extern const SeasonRuleKind season_kind_subs;
extern const SeasonRuleKind season_kind_nation;
extern const SeasonRuleKind season_kind_cups;
extern const SeasonRuleKind season_kind_cup_breaks;  /* slice 21b: season_cup_breaks.c */
extern const SeasonRuleKind season_kind_new_cups;    /* slice 21c: season_new_cups.c */
extern const SeasonRuleKind season_kind_uefa;          /* slice 22: season_uefa.c */
extern const SeasonRuleKind season_kind_club_world;
extern const SeasonRuleKind season_kind_club_cups;
extern const SeasonRuleKind season_kind_comps;         /* slice 26: season_comps.h */
void season_comps_reset(void);                         /* slice 26: season_comps.h */
extern const SeasonRuleKind season_kind_playoffs;      /* slice 19: season_playoffs.c */
extern const SeasonRuleKind season_kind_intl;
extern const SeasonRuleKind season_kind_ita_serie_c;  /* slice 20b: season_divisions_ita.c */
extern const SeasonRuleKind season_kind_calendar;     /* gap row 2 of 0.7.2: season_calendar_game.c */
static const SeasonRuleKind season_kind_test;
static const SeasonRuleKind *const season_rule_kinds[] = {
    &season_kind_comps,                 /* slice 26: new competitions (season_comps.h), first */
    &season_kind_names,                 /* slice 16: competition names (slice 15's names table) */
    &season_kind_ita_serie_c,           /* slice 20b: the names of the Serie C groups, after the names */
    &season_kind_subs,                  /* slice 17: substitutes used and the bench */
    &season_kind_nation,                /* slice 17: transfer windows, foreign player limits, work permits */
    &season_kind_cups,                  /* slice 21: the round formats of the domestic cups */
    &season_kind_cup_breaks,            /* slice 21b: domestic cups that are not held in some seasons */
    &season_kind_new_cups,              /* slice 21c: the seasons of the new domestic cups */
    &season_kind_sizes,                 /* slice 18: news of league sizes and places (season_sizes.c) */
    &season_kind_uefa,                  /* slice 22: formats of the UEFA club competitions */
    &season_kind_club_world,            /* slice 26: seasons of the club competitions outside UEFA */
    &season_kind_club_cups,             /* slice 26c: years of the new club cups (season_club_cups.c) */
    &season_kind_playoffs,              /* slice 19: promotion play-offs of leagues with none (after sizes) */
    &season_kind_intl,                  /* slice 27: international competitions (season_intl.c) */
    &season_kind_divisions,             /* slice 20: new divisions (season_divisions.c) */
    &season_kind_calendar,              /* gap row 2 of 0.7.2: the Irish calendar move (season_calendar.h) */
    &season_kind_test,                  /* test only: RECOMP_SEASON_CHANGE_TEST=1 (below) */
};
#define KIND_COUNT ((int)(sizeof season_rule_kinds / sizeof season_rule_kinds[0]))

#define GAME_DATE 0xae2c90u             /* s16 day of the year (0 = 1 January), s16 year */
#define COMP_TABLE 0xadadfcu            /* Comp ** */
#define COMP_SLOTS 0xadadf8u            /* s32 */
#define COMP_RECORDS 0xae23d0u          /* the club comp records, then the nation comp records */
#define COMP_RECORD_COUNT 0xae2378u     /* s32: the records of that array (set_key_ids reads it at 0x604241) */
#define RECORD_SIZE 0x6bu
#define ENTRIES 0xb63d60u               /* league table entries */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define ENTRY_DAY 0x15u                 /* s16 */
#define MAX_COMPS 4096
/* Days of the year, day 0 is 1 January. In a leap year each limit is one day earlier in the calendar (151 is
 * 31 May, 273 is 30 September). No update day lies within a week of these limits (the table in
 * docs/research/2026-10-17-season-change.md), so the limits are kept the same in every year. */
#define POINT_FIRST 151                 /* 1 June */
#define POINT_LAST 272                  /* 30 September */
#define LATE_DAY 273                    /* 1 October */
#define NAME_AT 4u                      /* the long name of a competition record */
#define NAME_ROOM 51                    /* 50 characters and the 0 */
#define TEXT_ROOM 0x2000u               /* the news text of one season */

#define NEWS_STORE 0xb7431cu            /* News * */
#define NEWS_ITEM_NEW 0x0077bbe0u       /* NewsItem::NewsItem, thiscall (type, flag) */
#define NEWS_SET_SLOT 0x00785790u       /* NewsItem::set_slot, thiscall (slot, value) */
#define NEWS_SEND_ALL 0x00786780u       /* News::send_to_all_managers, thiscall (item) */
#define TEXT_CLEAR 0x004afa40u          /* Text::clear, thiscall () */
#define TEXT_APPEND 0x004ae770u         /* Text::append_string, thiscall (text) */
#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size); 0x5d5d74 */
#define GAME_DELETE 0x00944c94u         /* operator delete, cdecl (block); 0x5d5d6a */
#define NEWS_ITEM_ROOM 0x100u           /* an item has 0xdf bytes */
#define NEWS_TYPE 0x300u                /* new: category 2 (Competitions), not used by the original */
#define NEWS_MAGIC 0x4e534f42u          /* "BOSN" in slot 0: an item of this file */

static int log_on = -1;                 /* RECOMP_SEASON_CHANGE_LOG=1: each change and each news text */

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    if (log_on < 0) log_on = getenv("RECOMP_SEASON_CHANGE_LOG") && *getenv("RECOMP_SEASON_CHANGE_LOG");
    return log_on;
}

static int game_day(void) { return (int16_t)rd16(g_mem, GAME_DATE); }
static int game_year(void) { return (int16_t)rd16(g_mem, GAME_DATE + 2); }

void season_change_latin1(char *s)
{
    unsigned char *in = (unsigned char *)s, *out = (unsigned char *)s;
    while (*in) {
        if ((in[0] == 0xc2 || in[0] == 0xc3) && in[1] >= 0x80 && in[1] <= 0xbf) {
            *out++ = (unsigned char)(((in[0] & 0x1f) << 6) | (in[1] & 0x3f));
            in += 2;
        } else {
            *out++ = *in++;
        }
    }
    *out = 0;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* ---- The competition objects ---- */

static const SeasonCompClass *class_of(uint32_t vtable)
{
    int lo = 0, hi = season_comp_class_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (season_comp_classes[mid].vtable == vtable) return &season_comp_classes[mid];
        if (season_comp_classes[mid].vtable < vtable) lo = mid + 1;
        else hi = mid - 1;
    }
    return season_comps_class(vtable);     /* slice 28: a vtable copy of a new competition (season_comps.h) */
}

const char *season_change_file(uint32_t obj)
{
    const char *f = season_comps_file(obj);            /* slice 24: a new competition of an original class */
    if (f) return f;
    const SeasonCompClass *c = readable(obj) ? class_of(rd32(g_mem, obj)) : NULL;
    if (!c && readable(obj)) {                          /* slice 21c: a new competition with its vtable copy */
        const char *f = season_comps_file_of(obj);
        if (f) return f;
    }
    return c ? c->file : "?";
}

static uint32_t entry_at(int e) { return ENTRIES + (uint32_t)e * ENTRY_SIZE; }

static int add_unique(uint32_t *out, int n, int max, uint32_t obj)
{
    if (!readable(obj) || n >= max) return n;
    for (int k = 0; k < n; k++)
        if (out[k] == obj) return n;
    out[n] = obj;
    return n + 1;
}

int season_change_entry_comps(int entry, uint32_t *out, int max)
{
    if (entry < 0 || entry >= ENTRY_COUNT) return 0;
    uint32_t arr = rd32(g_mem, entry_at(entry) + 0x10);
    int32_t count = (int32_t)rd32(g_mem, entry_at(entry) + 0xc);
    int n = 0;
    for (int32_t k = 0; readable(arr) && k < count && k < 256; k++)
        n = add_unique(out, n, max, rd32(g_mem, arr + 4u * (uint32_t)k));
    return n;
}

int season_change_start_day(uint32_t comp)
{
    uint32_t objs[256];
    for (int e = 0; comp && e < ENTRY_COUNT; e++) {
        int n = season_change_entry_comps(e, objs, 256);
        for (int i = 0; i < n; i++)
            if (objs[i] == comp) return (int16_t)rd16(g_mem, entry_at(e) + ENTRY_DAY);
    }
    return -1;
}

int season_change_comps(uint32_t *out, int max)
{
    int n = 0;
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int32_t slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    for (int32_t i = 0; readable(table) && i < slots && i < MAX_COMPS; i++)
        n = add_unique(out, n, max, rd32(g_mem, table + 4u * (uint32_t)i));
    for (int e = 0; e < ENTRY_COUNT; e++) {
        uint32_t arr = rd32(g_mem, entry_at(e) + 0x10);
        int32_t count = (int32_t)rd32(g_mem, entry_at(e) + 0xc);
        for (int32_t k = 0; readable(arr) && k < count && k < 256; k++)
            n = add_unique(out, n, max, rd32(g_mem, arr + 4u * (uint32_t)k));
    }
    return n;
}

static int in_entry(int e, uint32_t obj)
{
    uint32_t arr = rd32(g_mem, entry_at(e) + 0x10);
    int32_t count = (int32_t)rd32(g_mem, entry_at(e) + 0xc);
    for (int32_t k = 0; readable(arr) && k < count && k < 256; k++)
        if (rd32(g_mem, arr + 4u * (uint32_t)k) == obj) return 1;
    return 0;
}

int season_change_add_comp(int entry, uint32_t obj)
{
    if (entry < 0 || entry >= ENTRY_COUNT || !readable(obj) || in_entry(entry, obj)) return 0;
    uint32_t e = entry_at(entry), old = rd32(g_mem, e + 0x10);
    int32_t count = (int32_t)rd32(g_mem, e + 0xc);
    if (count < 0 || count >= 255) return 0;
    uint32_t size[1] = {4u * (uint32_t)(count + 1)};
    uint32_t arr = call_cdecl(GAME_NEW, 1, size);
    if (rt_unwinding || !readable(arr)) return 0;
    for (int32_t k = 0; k < count; k++) wr32(g_mem, arr + 4u * (uint32_t)k, rd32(g_mem, old + 4u * (uint32_t)k));
    wr32(g_mem, arr + 4u * (uint32_t)count, obj);
    wr32(g_mem, e + 0x10, arr);
    wr32(g_mem, e + 0xc, (uint32_t)(count + 1));
    if (readable(old)) {
        uint32_t b[1] = {old};
        call_cdecl(GAME_DELETE, 1, b);
    }
    return !rt_unwinding;
}

/* ---- The news texts of the game: one per season, kept in the save part ---- */

typedef struct {
    int season;
    char *text;                         /* sentences, one a line */
    uint32_t len;
    int posted;                         /* 1: its news item is sent */
} SeasonNews;

static SeasonNews *news;
static int news_count, news_room;
static int hook_season = -1;            /* the season of the hook that runs now; -1: no hook runs */
static int season_point;                /* the last season whose season point ran (0: none) */
static int creating;                    /* 1 while create_game 0x828110 runs (season_change_creating) */

int season_change_point(void) { return season_point; }

static SeasonNews *news_of(int season)
{
    for (int i = 0; i < news_count; i++)
        if (news[i].season == season) return &news[i];
    return NULL;
}

static SeasonNews *news_new(int season)
{
    if (news_count == news_room) {
        int room = news_room ? 2 * news_room : 8;
        SeasonNews *p = realloc(news, (size_t)room * sizeof *p);
        if (!p) return NULL;
        news = p;
        news_room = room;
    }
    SeasonNews *s = &news[news_count++];
    s->season = season;
    s->text = NULL;
    s->len = 0;
    s->posted = 0;
    return s;
}

static void news_append(SeasonNews *s, const char *line)
{
    uint32_t n = (uint32_t)strlen(line), extra = s->len ? 1u : 0u;
    if (s->len + extra + n > TEXT_ROOM) return;                   /* the text is full: the line is left out */
    char *p = realloc(s->text, s->len + extra + n + 1);
    if (!p) return;
    if (extra) p[s->len++] = '\n';
    memcpy(p + s->len, line, n + 1);
    s->text = p;
    s->len += n;
}

void season_change_news(const char *fmt, ...)
{
    if (hook_season < 0) return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    for (char *p = line; *p; p++)
        if (*p == '<' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
    SeasonNews *s = news_of(hook_season);
    if (!s) s = news_new(hook_season);
    if (!s) return;
    news_append(s, line);
    if (logging())
        fprintf(stderr, "recomp: season change: news %d/%02d: %s\n", hook_season, (hook_season + 1) % 100, line);
}

/* ---- The names that the game has changed ---- */

/* Each record whose long name the season code changed: its index and its name in the data. set_key_ids
 * 0x601a40 finds the key competitions by name at every load of the data (match_club_comp_key_id 0x60efd0,
 * _stricmp with fixed strings; the club comps and then the nation comps, from [0xae23d0], loop at 0x60425d).
 * A save holds the names of now, so a load would miss "UEFA Cup" and stop at the box "index.cpp 5809". The
 * wrap of 0x60efd0 gives it the name in the data instead. */
typedef struct {
    uint32_t index;
    char original[NAME_ROOM];
} SeasonRename;

static SeasonRename *renames;
static int rename_count, rename_room;

static int record_index(uint32_t rec, uint32_t *index)
{
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    int32_t count = (int32_t)rd32(g_mem, COMP_RECORD_COUNT);
    if (!readable(base) || rec < base || (rec - base) % RECORD_SIZE || count <= 0) return 0;
    *index = (rec - base) / RECORD_SIZE;
    return *index < (uint32_t)count && *index < 100000u;
}

int season_change_renames(void) { return rename_count; }

static const SeasonRename *rename_of(uint32_t index)
{
    for (int i = 0; i < rename_count; i++)
        if (renames[i].index == index) return &renames[i];
    return NULL;
}

void season_change_rename(uint32_t rec, const char *name)
{
    uint32_t index;
    if (!readable(rec)) return;
    if (record_index(rec, &index) && !rename_of(index)) {
        if (rename_count == rename_room) {
            int room = rename_room ? 2 * rename_room : 16;
            SeasonRename *p = realloc(renames, (size_t)room * sizeof *p);
            if (!p) return;                     /* no room to keep the name in the data: no change */
            renames = p;
            rename_room = room;
        }
        /* Open item 152: the whole row is cleared first. The save writes all NAME_ROOM bytes of the name, and
         * the bytes after its end came from the realloc block, so two runs of the same game gave other bytes. */
        memset(&renames[rename_count], 0, sizeof renames[rename_count]);
        renames[rename_count].index = index;
        snprintf(renames[rename_count].original, NAME_ROOM, "%.*s", NAME_ROOM - 1, win_str(rec + NAME_AT));
        rename_count++;
    } else if (!record_index(rec, &index)) {
        return;                                 /* not a competition record: a load could not match it */
    }
    char now[NAME_ROOM];
    snprintf(now, sizeof now, "%s", name);
    memset(g_mem + rec + NAME_AT, 0, NAME_ROOM);
    memcpy(g_mem + rec + NAME_AT, now, strlen(now));
}

/* Slice 26: see season_change.h. */
const char *season_change_data_name(uint32_t rec)
{
    uint32_t index;
    if (!record_index(rec, &index)) return NULL;
    const SeasonRename *r = rename_of(index);
    return r ? r->original : win_str(rec + NAME_AT);
}

void o_0060efd0(void);

/* match_club_comp_key_id, cdecl (name, index): a renamed record is matched by its name in the data. The two
 * other callers (0x60400e, 0x6041b7) pass names that they read from a file, which never match a record. */
RT_WRAP(0060efd0, match_comp_key_id)
{
    uint32_t name = rt_arg(0), index;
    const SeasonRename *r = rename_count && readable(name) && record_index(name - NAME_AT, &index) ?
        rename_of(index) : NULL;
    if (!r) {
        o_0060efd0();
        return;
    }
    static uint32_t buf;
    if (!buf) buf = win_static_alloc(NAME_ROOM);
    win_put_str(buf, r->original, NAME_ROOM);
    uint32_t slot = cpu.esp + 4;
    wr32(g_mem, slot, buf);
    o_0060efd0();
    if (!rt_unwinding) wr32(g_mem, slot, name);   /* cdecl: the caller's argument slot gets its value back */
}

/* ---- The save part ---- */

void season_friendly_dates_reset(void);
uint32_t season_friendly_part_size(void);       /* season_friendly_dates.c */
void season_friendly_part_write(uint8_t *out);
int season_friendly_part_read(const uint8_t *in, uint32_t n);

void season_change_reset(void)
{
    for (int i = 0; i < news_count; i++) free(news[i].text);
    news_count = 0;
    rename_count = 0;
    season_comps_reset();               /* slice 26 */
    season_cup_groups_reset();          /* slice 21c: the big stage arrays of the cups with groups */
    season_intl_qual_reset();           /* load sweep: the World Cup play-off ties of the last game */
    season_uefa_league_reset();         /* load sweep: the league phase list of a stage being made */
    season_friendly_dates_reset();      /* row A18: the Friendly object of the last game */
    season_point = 0;
    season_intl_marks_reset();                  /* 0.6.2 slice 4b: no qualifying campaign in the original's shape */
    creating = 0;                               /* also when a longjmp left create_game */
}

/* Version 3 (0.6.2 slice 12) when the game has caps above 255, version 4 (item 66) when it has goals above 255
 * (caps_wide.c): the row A18 list then always has its count, so the readers of 0.6.0 and 0.6.1 take the list and
 * leave the caps and goals after it (season_friendly_part_read of
 * those versions reads the entries that the count gives and ignores the bytes after them). Else version 2 or 1, as
 * before. */
/* Version 5 (0.6.2 slice 4b) when qualifying campaigns keep the original's shape (season_intl_old_shape): the row
 * A18 list with its count, the caps and the goals fields (counts 0 when empty), then the marks field
 * (season_intl.h). */
#define PART_FORM_MARKS 5
static int part_marks(void) { return season_intl_marks_size() != 0; }
static int part_caps(void) { return caps_wide_form() != 0 || part_marks(); }
static uint32_t caps_size(void) { return part_marks() ? caps_wide_part_size_all() : caps_wide_part_size(); }

uint32_t season_change_part_size(void)
{
    uint32_t pending = season_friendly_part_size(), caps = caps_size() + season_intl_marks_size();
    if (!news_count && !rename_count && season_point <= season_start_year() && !pending && !caps) return 0;
    if (caps && !pending) pending = 4;          /* the count 0 of an empty list */
    uint32_t n = 4 + 4 + (uint32_t)rename_count * (4 + NAME_ROOM) + 4 + pending + caps;
    for (int i = 0; i < news_count; i++) n += 12 + news[i].len;
    return n;
}

int season_change_part_form(void)
{
    if (part_marks()) return PART_FORM_MARKS;
    return part_caps() ? caps_wide_form() : season_change_part_size() ? 2 : 1;
}

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

void season_change_part_write(uint8_t *out)
{
    put32(out, (uint32_t)news_count);
    uint32_t at = 4;
    for (int i = 0; i < news_count; i++) {
        put32(out + at, (uint32_t)news[i].season);
        put32(out + at + 4, (uint32_t)news[i].posted);
        put32(out + at + 8, news[i].len);
        if (news[i].len) memcpy(out + at + 12, news[i].text, news[i].len);
        at += 12 + news[i].len;
    }
    put32(out + at, (uint32_t)rename_count);
    at += 4;
    for (int i = 0; i < rename_count; i++) {
        put32(out + at, renames[i].index);
        memcpy(out + at + 4, renames[i].original, NAME_ROOM);
        at += 4 + NAME_ROOM;
    }
    put32(out + at, (uint32_t)season_point);
    at += 4;
    uint32_t pending = season_friendly_part_size();
    season_friendly_part_write(out + at);       /* row A18: the qualifiers that wait for the first running season */
    if (!part_caps()) return;
    if (!pending) {
        put32(out + at, 0);
        pending = 4;
    }
    if (!part_marks()) {
        caps_wide_part_write(out + at + pending);   /* version 3 or 4 */
        return;
    }
    caps_wide_part_write_all(out + at + pending);
    season_intl_marks_write(out + at + pending + caps_wide_part_size_all());
}

int season_change_part_read(const uint8_t *in, uint32_t n, uint32_t form)
{
    season_change_reset();
    caps_wide_reset();
    if (n < 4) return 0;
    uint32_t count = get32(in), at = 4;
    if (count > (n - 4) / 12) return 0;            /* 12 bytes a season at least: no limit of seasons (0.6.2 fault 31) */
    for (uint32_t i = 0; i < count; i++) {
        if (n - at < 12) return 0;
        uint32_t season = get32(in + at), posted = get32(in + at + 4), len = get32(in + at + 8);
        if (season > SEASON_YEAR_MAX || !season_year_ok((int)season) || posted > 1 || len > TEXT_ROOM ||
            n - at - 12 < len || news_of((int)season))
            return 0;
        SeasonNews *s = news_new((int)season);
        char *text = s ? malloc(len + 1) : NULL;
        if (!text) return 0;
        memcpy(text, in + at + 12, len);
        text[len] = 0;
        for (uint32_t k = 0; k < len; k++)
            if (!text[k] || text[k] == '<') text[k] = ' ';
        s->text = text;
        s->len = len;
        s->posted = (int)posted;
        at += 12 + len;
    }
    if (n - at < 4) return 0;
    uint32_t names = get32(in + at);
    at += 4;
    if (names > 100000u || (n - at) / (4 + NAME_ROOM) < names) return 0;
    if (names > (uint32_t)rename_room) {
        SeasonRename *p = realloc(renames, (size_t)names * sizeof *p);
        if (!p) return 0;
        renames = p;
        rename_room = (int)names;
    }
    for (uint32_t i = 0; i < names; i++) {
        renames[i].index = get32(in + at);
        memcpy(renames[i].original, in + at + 4, NAME_ROOM);
        renames[i].original[NAME_ROOM - 1] = 0;
        /* Open item 152: a save made before the fix holds unset bytes after the name: the next save writes zeros. */
        size_t end = strlen(renames[i].original);
        memset(renames[i].original + end, 0, NAME_ROOM - end);
        at += 4 + NAME_ROOM;
    }
    rename_count = (int)names;
    if (n - at < 4) return 0;
    uint32_t point = get32(in + at);
    if (point && (point > SEASON_YEAR_MAX || !season_year_ok((int)point))) return 0;
    season_point = (int)point;
    at += 4;
    if (form < 3) return season_friendly_part_read(in + at, n - at);   /* row A18; nothing more in a save before it */
    /* Version 3 and later: the row A18 list with its count and whole entries, then the caps (and goals) above 255. */
    if (n - at < 4) return 0;
    uint32_t pending = get32(in + at);
    if (pending > (n - at - 4) / SEASON_FRIENDLY_ENTRY) return 0;
    uint32_t list = 4 + pending * SEASON_FRIENDLY_ENTRY;
    if (!season_friendly_part_read(in + at, list)) return 0;
    if (!caps_wide_part_read(in + at + list, n - at - list, form)) {   /* the caps, and from version 4 the goals */
        season_change_reset();
        return 0;
    }
    if (form < PART_FORM_MARKS) return 1;
    /* Version 5: the marks after the caps and goals fields (u32 count, then 8 bytes an entry each). */
    uint32_t c = at + list, left = n - c;
    for (int f = 0; f < 2; f++) {
        if (left < 4 || get32(in + c) > (left - 4) / 8) {
            season_change_reset();
            return 0;
        }
        uint32_t size = 4 + 8 * get32(in + c);
        c += size;
        left -= size;
    }
    if (!season_intl_marks_read(in + c, left)) {
        season_change_reset();
        return 0;
    }
    return 1;
}

/* ---- The news item ---- */

/* The news screen draws the body of an item with Text::draw_wrapped 0x4aeaf0 (call at 0x78901a: x 0x78 to 0x302,
 * font 3). It wraps into a global array of 50 lines of 501 bytes at 0xadae1c (0x4aec26) and never checks the line
 * count (the index [ebp-0x1b0] goes up at 0x4aeff5). Each '\n' ends a line (0x4aec7c). A longer body wrote line 51
 * over 0xae0ff6..0xae11ea: the ConmebolSeeding pointer at 0xae1000, so the Libertadores seeding of the next
 * January marked 1 club and the game stopped at conmebol_liber.cpp 663 (open item 69: the 2020/21 news of a 2013
 * start, 27 September 2020). So the news of a season goes in parts, one item each, of at most PART_LINES lines with
 * the heading. The lines of a sentence come from its length, at LINE_CHARS characters a line. That is about half
 * of what a line of the 651-pixel box (0x302 - 0x78 + 1) holds in font 3, so the room covers the word that wraps
 * to the next line and a larger Fonts setting (inference: the widths were not measured per character). The
 * count calls no game code, so the send at the season change uses no game memory (Q152), and write_text splits
 * the text in the same way at any later load. */
#define PART_LINES 40                   /* of the 50 that the line array holds */
#define HEAD_LINES 2                    /* "These changes come in with ...:" and the empty line after it */
#define LINE_CHARS 40
#define MAX_PARTS 32

static int sentence_lines(uint32_t n) { return 1 + (int)(n / LINE_CHARS); }

/* The parts of a season's text: the offset of the first sentence of each part. Returns the count, 1 or more. */
static int news_parts(const SeasonNews *s, uint32_t *start)
{
    int parts = 1, lines = HEAD_LINES;
    start[0] = 0;
    if (!s || !s->len) return 1;
    for (uint32_t at = 0; at < s->len;) {
        const char *nl = memchr(s->text + at, '\n', s->len - at);
        uint32_t n = nl ? (uint32_t)(nl - (s->text + at)) : s->len - at;
        int k = sentence_lines(n);
        if (lines + k > PART_LINES && lines > HEAD_LINES && parts < MAX_PARTS) {
            start[parts++] = at;
            lines = HEAD_LINES;
        }
        lines += k;
        at += n + 1;
    }
    return parts;
}

/* One news item for each part of the season's news to every human manager. The part goes in slot 2 of the item,
 * at +13: set_slot 0x785790 stores slot k at +5 + 4k (0x7857ac). The text stays in the list: write_text reads it
 * and splits it again in the same way. */
static void news_send_part(int season, int part);

static void news_send(int season)
{
    uint32_t start[MAX_PARTS];
    int parts = news_parts(news_of(season), start);
    for (int k = 0; k < parts && !rt_unwinding; k++) news_send_part(season, k);
}

static void news_send_part(int season, int part)
{
    static uint32_t item;
    if (!item) item = win_static_alloc(NEWS_ITEM_ROOM);
    uint32_t store = rd32(g_mem, NEWS_STORE);
    if (!readable(store)) return;
    uint32_t a[2] = {NEWS_TYPE, 0};
    call_this(NEWS_ITEM_NEW, item, 2, a);
    if (rt_unwinding) return;
    uint32_t s0[2] = {0, NEWS_MAGIC}, s1[2] = {1, (uint32_t)season};
    call_this(NEWS_SET_SLOT, item, 2, s0);
    if (rt_unwinding) return;
    call_this(NEWS_SET_SLOT, item, 2, s1);
    if (rt_unwinding) return;
    uint32_t s2[2] = {2, (uint32_t)part};
    call_this(NEWS_SET_SLOT, item, 2, s2);
    if (rt_unwinding) return;
    uint32_t b[1] = {item};
    call_this(NEWS_SEND_ALL, store, 1, b);
    if (logging() && !rt_unwinding)
        fprintf(stderr, "recomp: season change: news item for %d/%02d part %d sent on day %d of %d\n", season,
                (season + 1) % 100, part + 1, game_day(), game_year());
}

static int our_item(uint32_t item)
{
    return readable(item) && rd32(g_mem, item) == NEWS_TYPE && rd32(g_mem, item + 5) == NEWS_MAGIC;
}

static void text_put(uint32_t text, const char *s)
{
    static uint32_t buf;
    if (!buf) buf = win_static_alloc(TEXT_ROOM + 0x100);
    win_put_str(buf, s, TEXT_ROOM + 0x100);
    uint32_t a[1] = {buf};
    call_this(TEXT_APPEND, text, 1, a);
}

void o_0077bc80(void);
void o_00782b70(void);
void o_00784eb0(void);
void o_00784100(void);

/* NewsItem::write_text, thiscall (manager, text, full_text, flag): the headline (full_text 0) or the body. */
RT_WRAP(0077bc80, news_write_text)
{
    uint32_t item = cpu.ecx, text = rt_arg(1);
    if (!our_item(item) || !readable(text)) {
        uint32_t full_text = rt_arg(2);
        o_0077bc80();
        if (!rt_unwinding) news_text_written(item, text, full_text);
        return;
    }
    int season = (int)rd32(g_mem, item + 9), full = rt_arg(2) != 0;
    char head[160], of[40] = "";
    const SeasonNews *s = news_of(season);
    uint32_t start[MAX_PARTS];
    int parts = news_parts(s, start), part = (int)rd32(g_mem, item + 13);
    if (part < 0 || part >= parts) part = 0;   /* an item of an older version: slot 2 is 0 (0x77bbe0 clears it) */
    char body[TEXT_ROOM + 1] = "The rules of the new season are in force.";
    if (s && s->len) {
        uint32_t end = part + 1 < parts ? start[part + 1] - 1 : s->len;
        memcpy(body, s->text + start[part], end - start[part]);
        body[end - start[part]] = 0;
    }
    if (parts > 1) snprintf(of, sizeof of, " (part %d of %d)", part + 1, parts);
    call_this(TEXT_CLEAR, text, 0, NULL);
    if (rt_unwinding) return;
    if (!full) {
        snprintf(head, sizeof head, "New rules for the %d/%02d season%s", season, (season + 1) % 100, of);
        text_put(text, head);
    } else {
        snprintf(head, sizeof head, "These changes come in with the %d/%02d season%s:\n\n", season, (season + 1) % 100,
                 of);
        text_put(text, head);
        if (!rt_unwinding) text_put(text, body);
    }
    if (rt_unwinding) return;
    if (logging() && full)
        fprintf(stderr, "recomp: season change: news shown for %d/%02d, dated day %d of %d: %s\n", season,
                (season + 1) % 100, (int16_t)rd16(g_mem, item + 0xcd), (int16_t)rd16(g_mem, item + 0xcf), body);
    rt_ret_stdcall(1, 4);
}

/* NewsItem::get_action_button, thiscall (manager, label, label size, handler): no button. */
RT_WRAP(00782b70, news_action_button)
{
    if (!our_item(cpu.ecx)) {
        o_00782b70();
        return;
    }
    uint32_t label = rt_arg(1), handler = rt_arg(3);
    if (readable(label)) g_mem[label] = 0;
    if (readable(handler)) wr32(g_mem, handler, 0);
    rt_ret_stdcall(0, 4);
}

/* NewsItem::is_important_for, thiscall (manager): not important. */
RT_WRAP(00784eb0, news_important)
{
    if (!our_item(cpu.ecx)) {
        o_00784eb0();
        return;
    }
    rt_ret_stdcall(0, 1);
}

/* NewsItem::auto_respond, thiscall (manager, club): the item needs no reply. */
RT_WRAP(00784100, news_auto_respond)
{
    if (!our_item(cpu.ecx)) {
        o_00784100();
        return;
    }
    rt_ret_stdcall(0, 2);
}

/* ---- The season change ---- */

static int in_pass;                     /* 1 while a pass with hooks runs */
static int pass_season;                 /* S of that pass */
static int due[ENTRY_COUNT];            /* the entries on their update day in that pass */
static uint32_t started[MAX_COMPS];     /* the competitions whose start_new_season ran in that pass */
static int started_count;
static int stack[32], depth;            /* the entry of each start_new_season that runs, inner last */
static int pass_start;                  /* 1: S of that pass is the start season of the game */
static int hooks_start;                 /* 1 while the hooks of the start season run (season_change_is_start) */

static int skip_now;                    /* 1: a before hook called season_change_skip */
static int in_before;                   /* 1 while the before hooks run */

int season_change_is_start(void) { return hooks_start; }
void season_change_skip(void) { if (in_before) skip_now = 1; }

/* No kind is called in the base game (2001 with changing rules off) or with RECOMP_SEASON_CODE=off. */
static int kinds_on(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}
int season_change_is_creating(void) { return creating; }
void season_change_creating(int on) { creating = on; }

static void run_hook(int k, int which, const SeasonChange *c)
{
    const SeasonRuleKind *kind = season_rule_kinds[k];
    void (*fn)(const SeasonChange *) = which == 0 ? kind->entry : which == 1 ? kind->before : kind->after;
    if (fn) fn(c);
}

static void hooks(int which, uint32_t comp, int entry, int is_started)
{
    SeasonChange c = {pass_season, comp, comp ? season_change_file(comp) : "", entry,
                      entry >= 0 ? rd32(g_mem, entry_at(entry)) : 0, is_started, pass_start, creating};
    hook_season = pass_start ? -1 : pass_season;  /* the start season gives no news */
    hooks_start = pass_start;
    for (int k = 0; k < KIND_COUNT && !rt_unwinding; k++) run_hook(k, which, &c);
    hook_season = -1;
    hooks_start = 0;
}

static int entry_for(uint32_t comp)
{
    for (int e = 0; e < ENTRY_COUNT; e++)
        if (due[e] && in_entry(e, comp)) return e;
    for (int e = 0; e < ENTRY_COUNT; e++)
        if (in_entry(e, comp)) return e;
    return -1;
}

static uint32_t held[8];

void season_change_hold(uint32_t comp, int on)
{
    for (int i = 0; i < 8; i++)
        if (on ? !held[i] : held[i] == comp) {
            held[i] = on ? comp : 0;
            return;
        }
}

int season_change_start(uint32_t comp, int after)
{
    for (int i = 0; comp && i < 8; i++)
        if (held[i] == comp) return !after;
    if (!in_pass) return 0;
    if (!after) {
        int e = depth > 0 ? stack[depth - 1] : entry_for(comp);
        if (depth < 32) stack[depth] = e;
        depth++;
        if (logging()) {
            uint32_t rec = rd32(g_mem, comp + 4);
            fprintf(stderr, "recomp: season change: %s (%.50s) starts %d, entry %d%s\n", season_change_file(comp),
                    readable(rec) ? win_str(rec + NAME_AT) : "?", pass_season, e, pass_start ? " (start season)" : "");
        }
        skip_now = 0;
        in_before = 1;
        hooks(1, comp, e, 1);
        in_before = 0;
        int skip = skip_now;
        skip_now = 0;
        if (skip && depth <= 32) stack[depth - 1] = -2 - e;     /* the after hook gets started 0 */
        if (skip && logging()) fprintf(stderr, "recomp: season change: %s skips %d\n", season_change_file(comp), pass_season);
        return skip;
    } else {
        int e = depth > 0 && depth <= 32 ? stack[depth - 1] : -1, skipped = e <= -2;
        if (skipped) e = -2 - e;
        if (depth > 0) depth--;
        if (started_count < MAX_COMPS) started[started_count++] = comp;
        hooks(2, comp, e, !skipped);
        return 0;
    }
}

static void test_rename(void);

void season_change_new_game(void)
{
    if (!season_code_on()) return;
    season_point = season_start_year();         /* the start season's point lay in the run to the start date */
    test_rename();                              /* test only: a stand-in for a name of slice 15 */
    if (!kinds_on()) return;
    for (int k = 0; k < KIND_COUNT && !rt_unwinding; k++)
        if (season_rule_kinds[k]->new_game) season_rule_kinds[k]->new_game(season_start_year());
    if (season_start_year() == SEASON_FIRST_YEAR) return;
    /* The entries whose start season began before the game (update day from 1 October): their entry hook and
     * the after hook of each competition, once (season_change.h, "The start season"). */
    pass_season = season_start_year();
    pass_start = 1;
    creating = 1;
    for (int e = 0; e < ENTRY_COUNT && !rt_unwinding; e++) {
        if ((int16_t)rd16(g_mem, entry_at(e) + ENTRY_DAY) < LATE_DAY) continue;
        if (logging())
            fprintf(stderr, "recomp: season change: entry %d starts %d/%02d before the game (start season)\n", e,
                    pass_season, (pass_season + 1) % 100);
        hooks(0, 0, e, 0);
        uint32_t comps[256];
        int n = season_change_entry_comps(e, comps, 256);
        for (int i = 0; i < n && !rt_unwinding; i++) hooks(2, comps[i], e, 0);
    }
    creating = 0;
    pass_start = 0;
}

void season_change_loaded(void)
{
    if (!season_code_on()) return;
    if (season_point < season_start_year()) season_point = season_start_year();   /* a version 1 part */
    if (!kinds_on()) return;
    for (int k = 0; k < KIND_COUNT && !rt_unwinding; k++)
        if (season_rule_kinds[k]->loaded) season_rule_kinds[k]->loaded(season_start_year());
}

void o_005d5ae0(void);

static void update_pass(void);

/* game_season_update_pass, cdecl (): at the end of every day. Slice 19: an entry's change waits for its play-off
 * (season_playoffs.c). */
void season_friendly_dates_day(void);   /* season_friendly_dates.c: row A18, the friendly table of a running season */
void season_new_cups_day(void);         /* season_new_cups.c: the late start of the All-Ireland cup */

RT_WRAP(005d5ae0, season_update_pass)
{
    season_friendly_dates_day();
    if (rt_unwinding) return;
    int held = season_playoffs_hold();
    update_pass();
    season_playoffs_release(held);
    if (rt_unwinding) return;
    season_new_cups_day();
}

int season_change_late_start(uint32_t comp)
{
    if (!readable(comp) || in_pass) return 0;
    uint32_t vt = rd32(g_mem, comp), rec = rd32(g_mem, comp + 4);
    if (!readable(vt) || !readable(rec)) return 0;
    /* 0x5d5cc6..0x5d5ce0: a complete competition (vtable +0xc returns al) for which +0x2c returns 0 */
    int complete = (int)(call_this(rd32(g_mem, vt + 0xcu), comp, 0, NULL) & 0xffu);
    if (rt_unwinding || !complete) return 0;
    uint32_t busy = call_this(rd32(g_mem, vt + 0x2cu), comp, 0, NULL);
    if (rt_unwinding || busy) return 0;
    int day = game_day(), season = game_year() + (day >= LATE_DAY ? 1 : 0);
    int with_hooks = kinds_on() && season_hooks_due(season);
    if (with_hooks) {
        pass_season = season;
        pass_start = season == season_start_year();
        started_count = 0;
        depth = 0;
        in_pass = 1;
    }
    /* 0x5d5ce6..0x5d5d15: the attendances of a league division (comp_util_is_league_division 0x4b0700 with the
     * record id, then game_update_league_attendances 0x5d6280 for the competition and each one of its +0x20 chain) */
    uint32_t id = rd32(g_mem, rec);
    if (call_cdecl(0x004b0700u, 1, &id) && !rt_unwinding) {
        uint32_t obj = comp;
        for (int i = 0; readable(obj) && i < 64 && !rt_unwinding; i++) {
            call_cdecl(0x005d6280u, 1, &obj);
            int32_t next = (int32_t)rd32(g_mem, obj + 0x20u);
            if (next == -1) break;
            obj = rd32(g_mem, rd32(g_mem, COMP_TABLE) + 4u * (uint32_t)next);
        }
    }
    /* 0x5d5d17..0x5d5d1c: start_new_season (vtable +8, the wrap of season_starts.c runs the hooks), then 0x5d5d1f..
     * 0x5d5d29: Discipline::start_new_season_for_comp 0x556210 (thiscall [0xae28f0], the record) */
    if (!rt_unwinding) call_this(rd32(g_mem, vt + 8u), comp, 0, NULL);
    if (!rt_unwinding) call_this(0x00556210u, rd32(g_mem, 0xae28f0u), 1, &rec);
    in_pass = 0;
    pass_start = 0;
    return !rt_unwinding;
}

static void update_pass(void)
{
    if (!kinds_on()) {
        o_005d5ae0();
        return;
    }
    int day = game_day(), year = game_year(), any = 0;
    int season = year + (day >= LATE_DAY ? 1 : 0);
    for (int e = 0; e < ENTRY_COUNT; e++) {
        due[e] = (int16_t)rd16(g_mem, entry_at(e) + ENTRY_DAY) == day;
        any |= due[e];
    }
    /* The start season: its hooks run with any changing rules, in the run to the start date of a new game
     * (game_run_to_start_date 0x5cc850, called from create_game at 0x829fd9) and on the update days of the
     * start year after the start date (25 July, 15 August). A later season: only with changing rules on. */
    int start = season == season_start_year();
    if (!any || !season_hooks_due(season)) {
        o_005d5ae0();
        return;
    }
    pass_season = season;
    pass_start = start;
    started_count = 0;
    depth = 0;
    if (logging())
        for (int e = 0; e < ENTRY_COUNT; e++)
            if (due[e])
                fprintf(stderr, "recomp: season change: entry %d starts %d/%02d on day %d of %d%s\n", e, season,
                        (season + 1) % 100, day, year, start ? " (start season)" : "");
    /* The season point: the season hooks, before the entries and competitions of the day. */
    if (day >= POINT_FIRST && day <= POINT_LAST && season > season_point) {
        season_point = season;
        if (logging()) fprintf(stderr, "recomp: season change: season point of %d/%02d on day %d of %d%s\n", season,
                               (season + 1) % 100, day, year, start ? " (start season)" : "");
        hook_season = start ? -1 : season;
        hooks_start = start;
        for (int k = 0; k < KIND_COUNT && !rt_unwinding; k++)
            if (season_rule_kinds[k]->season) season_rule_kinds[k]->season(season);
        hook_season = -1;
        hooks_start = 0;
        if (rt_unwinding) return;
    }
    for (int e = 0; e < ENTRY_COUNT && !rt_unwinding; e++)
        if (due[e]) hooks(0, 0, e, 0);
    if (rt_unwinding) return;
    in_pass = 1;
    o_005d5ae0();
    in_pass = 0;
    if (rt_unwinding) return;
    /* The competitions of the day's entries that did not start. */
    for (int e = 0; e < ENTRY_COUNT; e++) {
        if (!due[e]) continue;
        uint32_t comps[256];
        int n = season_change_entry_comps(e, comps, 256);
        for (int i = 0; i < n && !rt_unwinding; i++) {
            int seen = 0;
            for (int k = 0; k < started_count && !seen; k++) seen = started[k] == comps[i];
            if (!seen) hooks(2, comps[i], e, 0);
        }
    }
    if (rt_unwinding) return;
    /* One news item per season, sent at its season point. */
    for (int i = 0; i < news_count; i++) {
        if (news[i].posted || news[i].season > season_point) continue;
        news[i].posted = 1;
        news_send(news[i].season);
        if (rt_unwinding) return;
    }
}

/* ---- Test kind: RECOMP_SEASON_CHANGE_TEST=1 ---- */

/* Adds a sentence for each competition that starts on an update day from 1 October (the calendar-year
 * leagues that start the next season early), so tests/seasons/season_change_run.py can check that such a
 * sentence waits for the item of its season. Off without the variable. */
static int test_on(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SEASON_CHANGE_TEST") && !strcmp(getenv("RECOMP_SEASON_CHANGE_TEST"), "1");
    return on;
}

static void test_after(const SeasonChange *c)
{
    if (!test_on() || !c->started || game_day() < LATE_DAY) return;
    uint32_t rec = rd32(g_mem, c->comp + 4);
    season_change_news("Test: %.50s starts its %d season on day %d of %d.", readable(rec) ? win_str(rec + NAME_AT) : "?",
                       c->season, game_day(), game_year());
}

static void test_season(int season)
{
    if (test_on()) fprintf(stderr, "recomp: season change: test: season hook %d on day %d of %d\n", season, game_day(),
                           game_year());
}

static int test_mode(const char *mode)
{
    const char *v = getenv("RECOMP_SEASON_CHANGE_TEST");
    return v && !strcmp(v, mode);
}

/* RECOMP_SEASON_CHANGE_TEST=rename: a new game renames the record "UEFA Cup" to "Test Cup", as a name of
 * slice 15 would (slice 15 calls season_change_rename at a new game, not through a kind), so
 * tests/seasons/season_core_run.py (--only rename) can check that a 2001/02 game with changing rules off and a
 * changed name writes the season part, and that its save loads. */
static void test_rename(void)
{
    if (!test_mode("rename")) return;
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    int32_t count = (int32_t)rd32(g_mem, COMP_RECORD_COUNT);
    for (int32_t i = 0; readable(base) && i < count && i < 100000; i++) {
        uint32_t rec = base + (uint32_t)i * RECORD_SIZE;
        if (!strcmp(win_str(rec + NAME_AT), "UEFA Cup")) {
            season_change_rename(rec, "Test Cup");
            fprintf(stderr, "recomp: season change: test: record %d renamed to Test Cup\n", (int)i);
            return;
        }
    }
}

/* RECOMP_SEASON_CHANGE_TEST=write: a kind that writes the game in every hook (it renames each competition it
 * meets to "Test Write"). The base game calls no kind, so its save must stay the same
 * (tests/smoke/smoke_run.py, the run kinds-base). */
static void test_write_comp(const SeasonChange *c)
{
    if (!test_mode("write") || !c->comp) return;
    uint32_t rec = rd32(g_mem, c->comp + 4);
    if (readable(rec)) season_change_rename(rec, "Test Write");
}

static void test_write_game(int start_year)
{
    (void)start_year;
    if (!test_mode("write")) return;
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    if (readable(base)) season_change_rename(base, "Test Write");
}

static void test_after_all(const SeasonChange *c)
{
    test_after(c);
    test_write_comp(c);
}

static const SeasonRuleKind season_kind_test = {"test", test_write_game, test_write_game, test_season, NULL,
                                                test_write_comp, test_after_all};
