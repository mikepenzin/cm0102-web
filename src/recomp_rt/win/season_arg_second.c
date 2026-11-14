/* The Argentine second division's table in a season game (0.6.0 fix: "virtual_staff..cpp 195").
 *
 * ArgSecond::fill_league_table 0x407cd0 (thiscall, no argument) sets +0x3e to 25 (0x407ce2), mallocs the table
 * (0x407ce8) and adds each club whose division (+0x57) is the record of key id [0x9cf658] and whose zone record
 * (+0x60) is set (0x407d78..0x407d8f), up to 25 (0x407da2..0x407daa). With exactly 24 it adds the fixed club of
 * key id [0x9d020c] (0x407dc0..0x407de5: it writes that club's +0x57 when it differs) as the 25th.
 *
 * In a season game slice 15's club moves give the division its real clubs (season_clubs.c). A club that comes
 * in from outside the Argentine zones has no zone record, so only 24 count, and the fixed club fills the place.
 * In the season databases that club is Huracán de Tres Arroyos with no nation and no players (Autumn 2013:
 * record 5215, nation -1, A Lower Division). Its first match then stopped at virtual_staff.cpp 195
 * (VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0, box at 0x90d4c1: fewer than 15 filled squad slots of +0xd7 after
 * the virtual players of 0x90dae0, which gives a club with no nation none): a 2013/14 game in its first August.
 *
 * So in a season game, for the call:
 * - a club of the division with no zone record counts: its +0x60 takes the zone record of another club of the
 *   division for the call and gets its own value back after it;
 * - when only 24 count and the fixed club has no nation or no squad, the key id points for the call at the
 *   Argentine club with no division and a zone record that has the highest reputation (+0x80).
 * The base game keeps the original's path. */
#include "win.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u      /* the national teams are the last 2 x nations rows */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_ZONE 0x60u
#define CLUB_REPUTATION 0x80u
#define CLUB_SQUAD 0xd7u
#define KEY_SECOND 0x9cf658u        /* the record id of the Argentine second division (0x407d88) */
#define KEY_FIXED 0x9d020cu         /* the club id of the 25th club (0x407dc5) */
#define TABLE_SIZE 25
#define MIN_SQUAD 11

static int readable(uint32_t p) { return rt_readable(p); }

static int squad_of(uint32_t club)
{
    int n = 0;
    for (uint32_t j = 0; j < 50; j++) n += rd32(g_mem, club + CLUB_SQUAD + 4 * j) != 0;
    return n;
}

static int in_second(uint32_t club, int32_t key)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) && (int32_t)rd32(g_mem, d) == key;
}

void o_00407cd0(void);

RT_WRAP(00407cd0, arg_second_fill_table)
{
    uint32_t base = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATION_COUNT);
    int32_t key = (int32_t)rd32(g_mem, KEY_SECOND);
    if (!season_game_changes() || !readable(base) || count <= 0 || key < 0) {
        o_00407cd0();
        return;
    }
    /* The zone record of a club of the division, for those that have none. */
    uint32_t zone = 0, nation = 0;
    int counted = 0, lent = 0;
    for (int32_t i = 0; i < count; i++) {
        uint32_t c = base + (uint32_t)i * CLUB_SIZE;
        if (!in_second(c, key)) continue;
        if (rd32(g_mem, c + CLUB_ZONE)) {
            if (!zone) zone = rd32(g_mem, c + CLUB_ZONE);
            if (!nation) nation = rd32(g_mem, c + CLUB_NATION);
            counted++;
        }
    }
    enum { MAX_LENT = 64 };
    uint32_t lent_at[MAX_LENT];
    for (int32_t i = 0; zone && i < count && lent < MAX_LENT; i++) {
        uint32_t c = base + (uint32_t)i * CLUB_SIZE;
        if (!in_second(c, key) || rd32(g_mem, c + CLUB_ZONE) || !rd32(g_mem, c + CLUB_NATION)) continue;
        wr32(g_mem, c + CLUB_ZONE, zone);
        lent_at[lent++] = c;
        counted++;
    }
    /* The fixed 25th club, when it would come in and cannot play. */
    int32_t fixed = (int32_t)rd32(g_mem, KEY_FIXED), swapped = -1;
    if (counted == TABLE_SIZE - 1 && fixed >= 0 && fixed < count) {
        uint32_t f = base + (uint32_t)fixed * CLUB_SIZE;
        if (!rd32(g_mem, f + CLUB_NATION) || squad_of(f) < MIN_SQUAD) {
            int32_t best = -1;
            int best_rep = -1;
            for (int32_t i = 0; nation && i < count; i++) {
                uint32_t c = base + (uint32_t)i * CLUB_SIZE;
                if (rd32(g_mem, c + CLUB_NATION) != nation || rd32(g_mem, c + CLUB_DIVISION) || !rd32(g_mem, c + CLUB_ZONE) ||
                    squad_of(c) < MIN_SQUAD)
                    continue;
                int rep = (int16_t)rd16(g_mem, c + CLUB_REPUTATION);
                if (rep > best_rep) best_rep = rep, best = i;
            }
            if (best >= 0) {
                swapped = fixed;
                wr32(g_mem, KEY_FIXED, (uint32_t)best);
            }
            fprintf(stderr, "recomp: argentina: the 25th club of the second division (record %d) has no nation or "
                            "squad: %s\n", fixed, best >= 0 ? "another club takes its place" : "no other club");
        }
    }
    o_00407cd0();
    for (int k = 0; k < lent; k++) wr32(g_mem, lent_at[k] + CLUB_ZONE, 0);
    if (swapped >= 0) wr32(g_mem, KEY_FIXED, (uint32_t)swapped);
    if (lent)
        fprintf(stderr, "recomp: argentina: %d clubs of the second division with no zone record count for its "
                        "table\n", lent);
}
