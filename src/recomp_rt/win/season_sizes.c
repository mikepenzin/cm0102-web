/* League sizes, promotion and relegation in existing divisions (season_sizes.h). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_names.h"
#include "season_rules.h"
#include "season_sizes.h"
#include "season_playoffs.h"
#include "season_divisions.h"
#include "season_divisions_ita.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club) */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u         /* club_comp pointer */
#define COMP_RECORD 4u              /* object +4: the competition record */
#define COMP_YEAR 0x40u             /* s16: the edition (start_new_season adds 1, 0x574c10) */
#define COMP_CLUBS 0x3eu            /* s16: the club count of a league (League::check_complete 0x689b3d) */
#define COMP_MEETINGS 0x3cu         /* s16: meetings of each pair */
#define COMP_MATCHES 0xa7u          /* s16: matches per club, -1 until check_complete 0x689ad0 sets it */
#define COMP_TABLE 0xb1u            /* the table: 0x3b bytes a row, the club at +0, the status at +0x37 */
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define UP 0xbeu                    /* s8: promotion places (League::has_automatic_promotion 0x684790) */
#define UP_PLAYOFF 0xbfu            /* s8: promotion play-off places (inference, rules report doc) */
#define DOWN_PLAYOFF 0xc0u          /* s8: relegation play-off places (inference) */
#define DOWN 0xc1u                  /* s8: relegation places (League::has_relegation 0x6847b0) */
#define ABOVE 0x1cu                 /* s32: record id of the division above, -1 for none */
#define BELOW 0x20u                 /* s32: record id of the division below */
#define COMP_OBJECTS 0xadadfcu      /* g_comp_object_table: the object of each competition record */
#define COMP_SLOTS 0xadadf8u        /* its slot count, one per club competition record (0x8386f0) */
#define STATUS_DOWN 3               /* league_promote_and_relegate 0x689c80: moves down a row of 3 (0x689d26) */
#define MIN_CLUBS 2
#define MAX_CLUBS 60

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

/* A season game with the sizes on. Test switch RECOMP_LEAGUE_SIZES=off: the original's counts and places (the
 * club moves of slice 15 still run). */
static int sizes_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_LEAGUE_SIZES") && !strcmp(getenv("RECOMP_LEAGUE_SIZES"), "off");
    return !off && season_game_changes();
}

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SIZE_LOG") && *getenv("RECOMP_SIZE_LOG");
    return on;
}

/* The index of a file in season_size_files, or -1. */
static int size_index(const char *file)
{
    for (int i = 0; file && i < season_size_file_count; i++)
        if (!strcmp(season_size_files[i], file)) return i;
    return -1;
}

static int size_file(const char *file) { return size_index(file) >= 0; }

/* The largest count of a file's class, 0 when the class counts the clubs of its data (or is not here). */
static int cap_of(const char *file)
{
    int i = size_index(file);
    return i >= 0 ? season_size_caps[i] : 0;
}

/* The smallest count of a file's class (0: any count from MIN_CLUBS). Q282 (the lead, 2026-10-18): a 2019/20
 * start plays League One with 23 clubs (Bury removed), so its floor is 23 in that season. */
static int floor_of(const char *file)
{
    int i = size_index(file);
    if (i < 0) return 0;
    if (!strcmp(file, "eng_second") && season_start_year() == 2019) return 23;
    return season_size_floors[i];
}

/* A count within the class's floor and cap. */
static int within(const char *file, int n)
{
    int cap = cap_of(file), low = floor_of(file);
    if (cap > 0 && n > cap) n = cap;
    if (low > 0 && n < low) n = low;
    return n;
}

/* The row of a season, or the last row before it (Q248: after the last row the rules stay). */
static const SeasonRule *row_of_season(const char *file, int season)
{
    for (int s = season; s >= SEASON_FIRST_YEAR && s > season - 10; s--) {
        const SeasonRule *r = season_rules_find(s, file, 0);
        if (r) return r;
    }
    return NULL;
}

/* 1 when the real season of a row begins with the year ("2011", "2010/11", "2011-12 (first stage ...)"). */
static int real_starts(const SeasonRule *r, int year)
{
    char y[8];
    snprintf(y, sizeof y, "%d", year);
    return r && r->real_season && !strncmp(r->real_season, y, 4) &&
           (r->real_season[4] < '0' || r->real_season[4] > '9');
}

/* The row of the rules of a league object now (season_sizes.h). */
static const SeasonRule *row_now(uint32_t obj, const char *file)
{
    if (!season_changing_rules() || season_change_is_creating()) return row_of_season(file, season_start_year());
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    for (int s = year - 1; s <= year; s++) {
        const SeasonRule *r = season_rules_find(s, file, 0);
        if (real_starts(r, year)) return r;
    }
    return row_of_season(file, year);
}

int season_sizes_clubs(const char *file, int season)
{
    if (!sizes_on() || !size_file(file)) return -1;
    const SeasonRule *r = row_of_season(file, season);
    if (!r || r->clubs < MIN_CLUBS || r->clubs > MAX_CLUBS || cap_of(file) <= 0) return -1;
    return within(file, r->clubs);
}

/* A division that a cup takes whole after a capped number of lower clubs keeps the clubs that the database gives
 * it, up to its class count, when its row has fewer (season_sizes.h). DenCup::collect_entrants 0x551240 wants 92
 * clubs (0x5c at 0x551254): lower clubs of [0x9cf764] up to the room of rt_cup_room (0x55131c), then every club of
 * [0x9cf5b4], [0x9cf5b0] and [0x9cf5ac]. A club that the club moves cut from the First Division goes to no
 * division, so the cup is short by that many and the constructor stops at den_cup.cpp 75 (push 0x4b at
 * 0x550a62). A 2020/21 start (November 2020 database: First Division 16, row 12) stopped there. */
int season_sizes_keeps_database(const char *file)
{
    return sizes_on() && file && !strcmp(file, "den_first");
}

/* The clubs whose division is the record of the object. */
static int clubs_in_division(uint32_t obj)
{
    uint32_t rec = rd32(g_mem, obj + COMP_RECORD), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    if (!readable(rec) || !readable(table)) return -1;
    for (int i = 0; i < count; i++)
        n += rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION) == rec;
    return n;
}

static const char *record_name(uint32_t obj)
{
    uint32_t rec = rd32(g_mem, obj + COMP_RECORD);
    return readable(rec) ? win_str(rec + 4) : "?";
}

uint32_t rt_league_clubs(uint32_t obj, uint32_t original)
{
    if (!sizes_on() || !readable(obj)) return original;
    const char *file = season_change_file(obj);
    int have = clubs_in_division(obj);
    if (have < MIN_CLUBS || have > MAX_CLUBS) return original;
    int cap = season_ita_cap(obj, cap_of(file)), n = cap > 0 && have > cap ? cap : have;   /* slice 20b: Serie C */
    const SeasonRule *r = row_now(obj, file);
    int want = r && r->clubs >= MIN_CLUBS && r->clubs <= MAX_CLUBS ? r->clubs : (int)original;
    if (logging())
        fprintf(stderr, "recomp: league sizes: %s (%.50s) %d: %d clubs in the division, %d in the table%s, class %u "
                        "(holds %d)\n", file, record_name(obj), (int16_t)rd16(g_mem, obj + COMP_YEAR), have, want,
                r ? "" : " (no row)", original, cap);
    /* Matches per club go back to -1 only for a new count, so check_complete counts them again. A class that sets
     * its own value keeps it: ScoPrm::set_up_rules writes 38 (33 and the split) at 0x7f29d5 before create_table
     * runs (0x7f2b68, then 0x7f2ba6), and with -1 check_complete would count (12 - 1) x 3 = 33. */
    /* A league that goes back to the class count after a season at another count also counts again, when its value
     * is the one check_complete made for the old count ((old - 1) x meetings +0x3c): else it kept the old season's
     * matches and was never complete (C2 of 20 back to 18 in 2012/13, slice 20b). ScoPrm's 38 is not that value.
     * In a season start the old count is gone: start_new_season frees the table first (League::free_table 0x687970,
     * for example at 0x64b830), which sets +0x3e to 0 (0x687a0c) before create_table reaches this store. So the old
     * count is the one that the value itself gives: a value of (k - 1) x meetings for another count k is the last
     * season's. The Serie C groups of 20 back to 18 in 2015/16 kept 38, check_complete never found them complete,
     * and Italy never started 2016/17 (O278). */
    int old = (int16_t)rd16(g_mem, obj + 0x3e), m = (int16_t)rd16(g_mem, obj + COMP_MATCHES);
    int meet = (int)g_mem[obj + 0x3c];
    if (old == 0 && meet > 0 && m > 0 && m % meet == 0) old = m / meet + 1;
    if ((uint32_t)n != original || (old != n && old >= MIN_CLUBS && m == (old - 1) * meet))
        wr16(g_mem, obj + COMP_MATCHES, 0xffffu);
    return (uint32_t)n;
}

uint32_t rt_league_dates(uint32_t obj, uint32_t original)
{
    if (!sizes_on() || !readable(obj)) return original;
    const char *file = season_change_file(obj);
    int have = clubs_in_division(obj), cap = cap_of(file);
    if (have < MIN_CLUBS || cap <= 0) return original;
    int n = have > cap ? cap : have, even = n + (n & 1), meetings = (int16_t)rd16(g_mem, obj + COMP_MEETINGS);
    if (meetings < 1 || meetings > 4) meetings = 2;
    int rounds = meetings * (even - 1);
    if (rounds <= 0 || rounds >= (int)original) return original;
    if (logging())
        fprintf(stderr, "recomp: league sizes: %s (%.50s): %d dates for %d clubs\n", file, record_name(obj), rounds, n);
    return (uint32_t)rounds;
}

/* The clubs whose division has the record id of a key id global. */
static int clubs_of_key(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    if (id < 0 || !readable(table)) return -1;
    for (int i = 0; i < count; i++) {
        uint32_t div = rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION);
        n += readable(div) && (int32_t)rd32(g_mem, div) == id;
    }
    return n;
}

static int top_clubs(uint32_t key_a, uint32_t key_b, uint32_t key_c)
{
    int a = key_a ? clubs_of_key(key_a) : 0, b = key_b ? clubs_of_key(key_b) : 0, c = key_c ? clubs_of_key(key_c) : 0;
    return a < 0 || b < 0 || c < 0 ? -1 : a + b + c;
}

uint32_t rt_cup_add(uint32_t original, uint32_t base, uint32_t entrants, uint32_t key_a, uint32_t key_b)
{
    if (!sizes_on()) return original;
    int top = top_clubs(key_a, key_b, 0);
    if (top < 0 || top > (int)entrants) return original;
    return (uint32_t)((int)entrants - top) - base;
}

uint32_t rt_cup_room(uint32_t original, uint32_t entrants, uint32_t key_a, uint32_t key_b, uint32_t key_c)
{
    if (!sizes_on()) return original;
    int top = top_clubs(key_a, key_b, key_c);
    if (top < 0) return original;
    int room = (int)entrants - top;
    if (room < 0) room = 0;
    if ((uint32_t)room * 6u != original && logging())
        fprintf(stderr, "recomp: league sizes: a cup takes %d lower clubs (the original %u)\n", room, original / 6u);
    return (uint32_t)room * 6u;
}

/* FraLgeCup::collect_entrants 0x5bd160: the first loop takes National clubs ([0x9cf5d4]) whose last division (+0x5b)
 * is set and is not the CFA ([0x9cf5d8]) up to 16 - 2 after 2001 (edx is 0 or -2 before `add edx, 0x10` at
 * 0x5bd244), the second loop National clubs from the CFA up to the room (0x5bd2ac). When the National holds fewer
 * clubs from the CFA than the room leaves them, the first loop takes those places too, so the cup gets its 54
 * (a 2025/26 start with the December 2025 database stopped at fra_lge_cup.cpp 73, 0x5bcb21). With enough clubs
 * from the CFA the limit is the original's. Returns the value to add to edx. */
uint32_t rt_fra_lge_first(uint32_t original, uint32_t edx)
{
    if (!sizes_on()) return original;
    int base = (int)edx + (int)original;
    int32_t national = (int32_t)rd32(g_mem, 0x9cf5d4u), cfa = (int32_t)rd32(g_mem, 0x9cf5d8u);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), from_cfa = 0;
    if (!readable(table)) return original;
    for (int i = 0; i < count; i++) {
        uint32_t at = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, at + CLUB_DIVISION), last = rd32(g_mem, at + 0x5bu);
        from_cfa += readable(div) && (int32_t)rd32(g_mem, div) == national && readable(last) && (int32_t)rd32(g_mem, last) == cfa;
    }
    int room = (int)(rt_cup_room(0x60u, 54u, 0x9cf5d0u, 0x9cf5ccu, 0u) / 6u), limit = room - from_cfa;
    if (limit < base) limit = base;
    return (uint32_t)(limit - (int)edx);
}

/* FraLgeCup::collect_entrants 0x5bd160, thiscall, ret 0, called by the constructor 0x5bc980 (0x5bca0a) and by
 * start_new_season 0x5bd420: both loops of National clubs skip a club whose last division (+0x5b) is not set
 * (0x5bd222..0x5bd227, 0x5bd2c4..0x5bd2c9). A database that leaves it unset for National clubs can then fill fewer
 * places than the room (rt_cup_room at 0x5bd2ac) even with enough clubs: the October 2026 Data Update in 2025/26
 * has 19 National clubs, 15 from another division, 2 from the CFA and 2 with none, so 17 of 18, 53 entrants, and
 * the constructor stopped at fra_lge_cup.cpp 73 (0x5bca3a; the box at 0x5bcb21). Only when the two loops would fall
 * short, the wrap gives as many of those clubs as the room needs their own division as the last one for the time
 * of the call (in the order of the club table), so the first loop takes them, and then puts the old value back. */
void o_005bd160(void);

RT_WRAP(005bd160, fra_lge_entrants)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!sizes_on() || !readable(table)) {
        o_005bd160();
        return;
    }
    int32_t national = (int32_t)rd32(g_mem, 0x9cf5d4u), cfa = (int32_t)rd32(g_mem, 0x9cf5d8u);
    int other = 0, from_cfa = 0, none = 0;
    for (int i = 0; i < count; i++) {
        uint32_t at = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, at + CLUB_DIVISION), last = rd32(g_mem, at + 0x5bu);
        if (!readable(div) || (int32_t)rd32(g_mem, div) != national) continue;
        if (!last) none++;
        else if (readable(last)) { if ((int32_t)rd32(g_mem, last) == cfa) from_cfa++; else other++; }
    }
    int room = (int)(rt_cup_room(0x60u, 54u, 0x9cf5d0u, 0x9cf5ccu, 0u) / 6u), need = room - other - from_cfa;
    if (need <= 0 || none == 0) {
        o_005bd160();
        return;
    }
    if (need > none) need = none;
    uint32_t set[64];
    int n = 0;
    for (int i = 0; i < count && n < need && n < 64; i++) {
        uint32_t at = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, at + CLUB_DIVISION);
        if (!readable(div) || (int32_t)rd32(g_mem, div) != national || rd32(g_mem, at + 0x5bu)) continue;
        wr32(g_mem, at + 0x5bu, div);
        set[n++] = at;
    }
    fprintf(stderr, "recomp: league sizes: French League Cup: %d National clubs with no last division count as from "
            "the National (%d from another division, %d from the CFA, room %d)\n", n, other, from_cfa, room);
    o_005bd160();
    for (int k = 0; k < n; k++) wr32(g_mem, set[k] + 0x5bu, 0);
}

/* EngFaCup::collect_entrants 0x56fab0 (O294) puts the National League clubs ([0x9cf69c]) in a stack array at
 * [esp + 0x3c] (store at 0x56fd98, count [esp + 0x18]) and the English clubs below it in a stack array at
 * [esp + 0x94] (store at 0x56fd7b, count [esp + 0x14], at most 300). It then copies byte [esp + 0x12] places from
 * the first (22 at 0x56fade, one less for each FA Trophy finalist in it, copy at 0x570046..0x57008b) and byte
 * [esp + 0x13] places from the second (38 at 0x56fae3, copy at 0x56fffc..0x57003e). Neither copy tests the
 * array's count, and the frame is never cleared. With fewer National League clubs than places (a 2025/26 start
 * with the December 2025 database, changing rules on) the cup took stack values: a NULL left an empty place, any
 * other value a club at a garbage address with no players (record 351, round 10, match_eng..cpp 652 and 612).
 * Here, at `movsx di, byte ptr [esp + 0x1b]` (0x56ffe4, two pushes after 0x56ffa6, so the counts are 8 bytes
 * higher), the places the National League cannot fill go to the clubs below it, and no copy takes more than its
 * array holds. Returns the count of the second copy. With enough clubs the counts are the original's. */
uint32_t rt_fa_cup_takes(uint32_t esp)
{
    int league_take = (int8_t)rd8(g_mem, esp + 0x1au), lower_take = (int8_t)rd8(g_mem, esp + 0x1bu);
    int league_have = (int16_t)rd16(g_mem, esp + 0x20u), lower_have = (int16_t)rd16(g_mem, esp + 0x1cu);
    if (!season_game_changes() || (league_take <= league_have && lower_take <= lower_have)) return (uint32_t)lower_take;
    int league = league_take < league_have ? league_take : (league_have > 0 ? league_have : 0);
    int lower = lower_take + league_take - league;
    if (lower > lower_have) lower = lower_have > 0 ? lower_have : 0;
    fprintf(stderr, "recomp: league sizes: FA Cup takes %d National League clubs (has %d, places %d) and %d clubs "
            "below it (has %d, places %d)\n", league, league_have, league_take, lower, lower_have, lower_take);
    wr8(g_mem, esp + 0x1au, (uint8_t)league);
    wr8(g_mem, esp + 0x1bu, (uint8_t)lower);
    return (uint32_t)lower;
}

/* ItaCup::select_entrants 0x63faf0 fills a list of 32 Italian clubs (0x20 at 0x63fc21). Its first loop takes the
 * Serie A clubs, the clubs relegated from Serie A, and the clubs whose last division (+0x5b) is a Serie C1 group
 * ([0x9cf578], [0x9cf57c]) with last position (+0x5f) 3, 4 or 5 (0x63fc69..0x63fc9b). While the list is short, its
 * second loop takes C1 clubs (not in Serie A) whose last position is p, for p = 5, 6, ... (0x63fb16, 0x63fcf9..
 * 0x63fd30). A C1 club of last position 5 is then taken twice. The cup draws it twice, plays a tie with the other
 * club, and stops at Cup.cpp 1187 (Cup::process_fixture 0x51a150: Cup::find_tie 0x519c90 returns -1 at 0x51a336,
 * box at 0x51a396). The base game never showed it (inference: its data fills the list in the first loop). In a
 * season game the club moves change the Italian divisions (a 2010/11 start with the September 2010 database took
 * Foggia twice and stopped on day 330), so there the second loop starts at 6. */
uint32_t rt_ita_cup_first_position(uint32_t original)
{
    return season_game_changes() ? 6u : original;
}

/* Sets a count of places so that the row's count is the direct places, or the direct places and the winners
 * of the class's own play-off (the reading of tests/seasons/rules_report_check.py). */
static void set_places(uint32_t obj, uint32_t at, uint32_t playoff_at, int want, const char *what)
{
    if (want < 0 || want > 30) return;
    int now = (int8_t)g_mem[obj + at], playoff = (int8_t)g_mem[obj + playoff_at];
    /* A class with no places in this direction has its own way or none (Argentina relegates by the average of
     * three seasons, ArgPrm::set_up_rules 0x4056d1 writes 0): it stays. */
    if (now == 0 && playoff == 0) return;
    if (want == now || want == now + (playoff ? 1 : 0) || want == now + playoff) return;
    int direct = want - (playoff ? 1 : 0);
    if (direct < 0) direct = 0;
    g_mem[obj + at] = (uint8_t)direct;
    if (logging())
        fprintf(stderr, "recomp: league sizes: %s (%.50s) %d: %s places %d, the class gave %d (+%d play-off)\n",
                season_change_file(obj), record_name(obj), (int16_t)rd16(g_mem, obj + COMP_YEAR), what, direct, now,
                playoff);
}

/* The object of the division above or below (league +0x1c, +0x20: the record ids, -1 for none, EngFirst::
 * set_up_rules stores them at 0x572dea and 0x572df6), or 0. The object of a record is g_comp_object_table
 * [0xadadfc] at the record id, below its slot count [0xadadf8] (League::process_promotion_relegation_chain
 * 0x689c42). open_neighbour: 1 when that division is a league of the size table with no floor. */
static uint32_t neighbour(uint32_t obj, uint32_t at)
{
    int32_t id = (int32_t)rd32(g_mem, obj + at), slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t other = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(other) ? other : 0;
}

/* A class that counts the clubs of its data (IreFirst, IrePrm, PolFirst: cap 0) counts too: its moves stay in
 * pairs whenever the places of two divisions differ (the pair wrap below), so its places can change and its count
 * does not. */
static int open_neighbour(uint32_t obj, uint32_t at)
{
    uint32_t other = neighbour(obj, at);
    if (!other) return 0;
    const char *f = season_change_file(other);
    return size_file(f) && floor_of(f) == 0;
}

static void sizes_rules(uint32_t obj)
{
    if (!sizes_on() || !readable(obj)) return;
    const char *file = season_change_file(obj);
    /* A floor division keeps its places too: its count must stay, and some of its exchanges are fixed code (League
     * Two with the Conference: 2 down and 1 back gave 23 clubs in 2023/24 of a 2022/23 game). */
    if (!size_file(file) || floor_of(file) > 0) return;
    const SeasonRule *r = row_now(obj, file);
    if (!r) return;
    /* Only towards a division of the size table with no floor: elsewhere the exchange is the class's own code
     * with fixed counts (the German Regionalliga split needs exactly 36 clubs, cmp bl, 0x24 at 0x5dbc3b, box
     * ger_first.cpp 1049 at 0x5dbcd9: the 2. Bundesliga with 2 relegation places against the Regionalliga's 4
     * promotions left it short in June 2017). */
    if (open_neighbour(obj, ABOVE)) set_places(obj, UP, UP_PLAYOFF, r->up, "promotion");
    if (open_neighbour(obj, BELOW)) set_places(obj, DOWN, DOWN_PLAYOFF, r->down, "relegation");
}

void season_sizes_rules(uint32_t obj)
{
    sizes_rules(obj);
    if (!rt_unwinding) season_playoffs_rules(obj);   /* slice 19: a play-off in a league with none */
    if (!rt_unwinding) season_divisions_rules(obj);  /* slice 20: the places towards a new division */
}

/* ---- Promotion and relegation of a size change ---- */

/* The table rows of a league with a status. */
static int rows_with(uint32_t league, int a, int b)
{
    if (!readable(league)) return -1;
    uint32_t table = rd32(g_mem, league + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, league + COMP_CLUBS), k = 0;
    if (!readable(table) || n <= 0 || n > 200) return -1;
    for (int i = 0; i < n; i++) {
        int s = g_mem[table + (uint32_t)i * ROW_SIZE + ROW_STATUS];
        k += s == a || s == b;
    }
    return k;
}

/* The rows that the places of a league send up (up 1) or down at the season change. League::update_positions_and_
 * status 0x684cd0 gives the +0xbe rows from the top status 5 or 0 and the +0xc1 rows from the bottom status 3
 * (0x684e11..0x684ef5, 0x685149..0x68525e). A play-off of +0xbf (+0xc0) places sends its one winner up (its loser
 * down), the reading of set_places. -1 for no league. */
static int place_moves(uint32_t league, int up)
{
    if (!readable(league)) return -1;
    int direct = (int8_t)g_mem[league + (up ? UP : DOWN)];
    int playoff = (int8_t)g_mem[league + (up ? UP_PLAYOFF : DOWN_PLAYOFF)];
    return direct < 0 ? -1 : direct + (playoff > 0 ? 1 : 0);
}

/* The expected count of a call of league_promote_and_relegate 0x689c80: up 1 the clubs that the lower league sends
 * up, up 0 the clubs that the upper league sends down. ItaSerA's chain 0x642410 pushes 4 and 4 for Serie A and B
 * (0x642476, 0x642478), the places of the base game. In a season game the places change (2003/04 Serie A 3 down
 * and Serie B 4 up, 2016/17 3 and 3), so there the count comes from the places of the season. */
uint32_t rt_promote_expected(uint32_t league, uint32_t up, uint32_t original)
{
    if (!sizes_on()) return original;
    int n = place_moves(league, up != 0);
    return n >= 0 ? (uint32_t)n : original;
}

void o_00689c80(void);

/* league_promote_and_relegate 0x689c80, stdcall (upper, lower, expire_contracts, pairwise, expected_up,
 * expected_down), ret 0x18. It moves the rows of status 3 of the upper league down (0x689d26, 0x689e52) and the
 * rows of status 0 or 5 of the lower league up (0x689d47..0x689d4e, 0x689efd..0x689f03). In pairs (pairwise,
 * 0x689cf9) a row moves only with a partner, so the sizes stay. The expected counts (0xff: none) are compared
 * with the moves at 0x689f47 and 0x689fbb (asserts league.cpp 5596 and 5602). */
RT_WRAP(00689c80, promote_relegate)
{
    if (!sizes_on()) {
        o_00689c80();
        return;
    }
    uint32_t upper = rt_arg(0), lower = rt_arg(1), pairwise = rt_arg(3);
    int down = rows_with(upper, STATUS_DOWN, STATUS_DOWN), up = rows_with(lower, 0, 5);
    int exp_up = (int8_t)(rt_arg(4) & 0xff), exp_down = (int8_t)(rt_arg(5) & 0xff);
    /* Only a pair of two size-table classes: only there can the places of this slice differ. Any other pair keeps
     * the original's call. */
    int ours = size_file(season_change_file(upper)) && size_file(season_change_file(lower));
    int change = ours && down >= 0 && up >= 0 && down != up, single = 0;
    /* When the places of the two divisions differ, the clubs move on their own only with changing rules on, between
     * two classes of the size table, and when both divisions hold counts within their floor and cap after the moves:
     * the upper the clubs it holds now less down and plus up, the lower the clubs it holds now plus down and less up
     * (before its own exchange with the division below, the next call of the chain). Else the clubs move in pairs,
     * so no size changes (with changing rules off the start season's sizes stay, Q232). So a change that needs a
     * division above its cap for a moment stays in pairs: Ligue 1 from 20 to 18 in 2023/24 would put 22 in Ligue 2
     * until its exchange with the National, which holds 20. The
     * French chain passes no pairs (FraFirst::start_new_season pushes 0 at 0x5bc4f1, League::process_promotion_
     * relegation_chain passes it at 0x689c59), so pairs are set here. */
    if (change && season_changing_rules()) {
        const char *fu_name = season_change_file(upper), *fl_name = season_change_file(lower);
        int cu = cap_of(fu_name), cl = cap_of(fl_name);
        int lu = floor_of(fu_name) > MIN_CLUBS * 4 ? floor_of(fu_name) : MIN_CLUBS * 4;
        int ll = floor_of(fl_name) > MIN_CLUBS * 4 ? floor_of(fl_name) : MIN_CLUBS * 4;
        int nu = clubs_in_division(upper) - down + up, nl = clubs_in_division(lower) + down - up;
        single = cu > 0 && cl > 0 && nu >= lu && nu <= cu && nl >= ll && nl <= cl;
        if (logging() && !single)
            fprintf(stderr, "recomp: league sizes: %s %d clubs and %s %d clubs after the season change do not fit "
                            "(%d to %d and %d to %d): the clubs move in pairs\n", fu_name, nu, fl_name, nl, lu, cu, ll, cl);
    }
    if (logging())
        fprintf(stderr, "recomp: league sizes: %s (%.50s) down %d, %s (%.50s) up %d, pairs %u, expected %d up %d down%s\n",
                season_change_file(upper), record_name(upper), down, season_change_file(lower), record_name(lower), up,
                pairwise, exp_up, exp_down, !change ? "" : single ? ": each club moves on its own" : ": in pairs");
    if (change) wr32(g_mem, cpu.esp + 4 + 4 * 3, single ? 0u : 1u);
    /* The moves that the call makes: each row on its own, or pairs. The callers with counts pass the moves of the
     * places (rt_promote_expected). In pairs only as many clubs move as the smaller count, so when this wrap sets
     * pairs for places that differ, both counts become the smaller one. */
    int pairs = rd32(g_mem, cpu.esp + 4 + 4 * 3) != 0, n = down < up ? down : up;
    int moved_up = pairs ? n : up, moved_down = pairs ? n : down;
    if (change && pairs && exp_up != -1 && exp_down != -1) {
        exp_up = exp_down = exp_up < exp_down ? exp_up : exp_down;
        wr32(g_mem, cpu.esp + 4 + 4 * 4, (uint32_t)exp_up);
        wr32(g_mem, cpu.esp + 4 + 4 * 5, (uint32_t)exp_down);
    }
    /* A count that still differs from the rows is a fault: the statuses of the table do not match its places (a
     * play-off with no result, a status set twice). The log names it and the check is dropped, so the season goes
     * on (asserts league.cpp 5596 and 5602). */
    if (down >= 0 && up >= 0 && ((exp_up != -1 && exp_up != moved_up) || (exp_down != -1 && exp_down != moved_down))) {
        fprintf(stderr, "recomp: league sizes: fault: %s (%.50s) and %s (%.50s) expect %d up and %d down by their "
                        "places, the rows move %d and %d; the check is dropped\n", season_change_file(upper),
                record_name(upper), season_change_file(lower), record_name(lower), exp_up, exp_down, moved_up,
                moved_down);
        if (exp_up != -1 && exp_up != moved_up) wr32(g_mem, cpu.esp + 4 + 4 * 4, 0xffu);
        if (exp_down != -1 && exp_down != moved_down) wr32(g_mem, cpu.esp + 4 + 4 * 5, 0xffu);
    }
    o_00689c80();
}

/* ---- News (rule kind "sizes") ---- */

/* The count and places that each league held last season, so the news names only what the game applies. A new
 * game and a loaded game note the counts of their own objects (sizes_new_game, sizes_loaded), so a game loaded
 * after another game never compares with the objects of the other game. */
#define MAX_SEEN 512
static struct { uint32_t obj; int clubs, up, down; } seen[MAX_SEEN];
static int nseen;

static int seen_at(uint32_t obj, int add)
{
    for (int i = 0; i < nseen; i++)
        if (seen[i].obj == obj) return i;
    if (!add || nseen >= MAX_SEEN) return -1;
    seen[nseen].obj = obj;
    seen[nseen].clubs = -1;
    return nseen++;
}

static void note(uint32_t obj)
{
    int i = seen_at(obj, 1);
    if (i < 0) return;
    seen[i].clubs = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    seen[i].up = (int8_t)g_mem[obj + UP];
    seen[i].down = (int8_t)g_mem[obj + DOWN];
}

static void sizes_after(const SeasonChange *c)
{
    if (!sizes_on() || !c->started || !size_file(c->file) || !readable(c->comp)) return;
    int i = seen_at(c->comp, 0);
    if (i >= 0 && seen[i].clubs >= 0 && !c->start) {
        const char *name = record_name(c->comp);
        int clubs = (int16_t)rd16(g_mem, c->comp + COMP_CLUBS), up = (int8_t)g_mem[c->comp + UP],
            down = (int8_t)g_mem[c->comp + DOWN];
        if (clubs != seen[i].clubs)
            season_change_news("The %.50s now has %d clubs (%d last season).", name, clubs, seen[i].clubs);
        if (up != seen[i].up)
            season_change_news("%d club%s now go%s up from the %.50s (%d last season).", up, up == 1 ? "" : "s",
                               up == 1 ? "es" : "", name, seen[i].up);
        if (down != seen[i].down)
            season_change_news("%d club%s now go%s down from the %.50s (%d last season).", down, down == 1 ? "" : "s",
                               down == 1 ? "es" : "", name, seen[i].down);
    }
    note(c->comp);
}

/* At a new game the constructors' set_up_rules run before g_comp_object_table is filled (build_comp_object_table
 * 0x8386f0, called at 0x829640 in create_game), so no neighbour is known there: the places of the start season
 * are set again once the game is made. */
static void sizes_new_game(int start_year)
{
    (void)start_year;
    if (!sizes_on()) return;
    uint32_t comps[512];
    int n = season_change_comps(comps, 512);
    nseen = 0;
    for (int i = 0; i < n && !rt_unwinding; i++) {
        /* Only the classes whose set_up_rules season_size_wraps.c wraps: those are the leagues that these rules
         * meet in every later season. Another class keeps its own stages: GerRegional's constructor sets the
         * stage count +0x2c to 1 (0x5dcc3d) for its South group, and the play-off rules dropped that stage
         * (+0x30 -1) while its fixtures stayed, so a German start with the regional divisions stopped at
         * league.cpp 3214 (League::add_result 0x685d30, stage test 0x685ddb..0x685de7) on its first match day
         * (slice 20, found in 2007 and 2008 starts). */
        if (!size_file(season_change_file(comps[i]))) continue;
        season_sizes_rules(comps[i]);
        note(comps[i]);
    }
}

/* After a load: the counts of the loaded game. It only reads the objects. */
static void sizes_loaded(int start_year)
{
    (void)start_year;
    nseen = 0;
    if (!sizes_on()) return;
    uint32_t comps[512];
    int n = season_change_comps(comps, 512);
    for (int i = 0; i < n && !rt_unwinding; i++)
        if (size_file(season_change_file(comps[i]))) note(comps[i]);
}

const SeasonRuleKind season_kind_sizes = {"sizes", sizes_new_game, sizes_loaded, NULL, NULL, NULL, sizes_after};
