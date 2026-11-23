/* Save names (#34, #139, #164): Save Game suggests "Club YYYY-MM-DD" from the player's club and the game date, or
 * "Nation YYYY-MM-DD" for a national team manager with no club, for the first save of a game; after that it offers
 * the name of the save in play, as the original does (this fork's rule for the browser, kept over 0.7.3's suggestion
 * at every save: see below). The player can edit the name in the text entry as before.
 *
 * Save Game (side bar handler, menu item 0x3fe at 0x763eda) copies the last save name, game options
 * + 0x113 (g_config at 0xae3138), into the name buffer 0xdd6ed4 (0x763f2d..0x763f5f). It then calls the file dialog
 * FUN_0058f500 (cdecl, 14 arguments) at 0x763fb3 with the buffer as argument 5 and the save routine
 * 0x838e10 as argument 6. That name is the save loaded or saved last: a native run (15 November 2026) loaded
 * "My Career.sav" and Save Game held "My Career", and after a save it held the name saved (load_game_data 0x82a9c0
 * and save_game_data 0x82e430 set the flag 0xdd7130 of a game with a save file, create_game 0x828110 clears it:
 * docs/research/data-map/Setup.cpp.md). A new game's first Save Game held an empty name. The wrap once wrote the
 * suggestion over it every time, so each save on a new game date was another file (a player's browser held 99 saves,
 * 3.4 GB). It now writes the suggestion only when the buffer is empty: a game not yet loaded or saved. Other dialogs
 * (printouts, scout searches) do not change.
 * The load dialogs and Save Game Before Exit at 0x762c13 pass another buffer and do not change either.
 *
 * The club: the manager of the player slot that has the turn (0xb74c26 + slot * 0x300, slot at
 * 0xb77c7a), club pointer at manager + 0x39, short name at club + 0x38, long name at club + 4. The side
 * bar reads the same fields at 0x762ff0..0x763017.
 *
 * The nation: a manager with no club and the job 5 or 12 at manager + 0x28 manages the national team at
 * manager + 0x24 (MANAGER_MANAGER::appoint_manager_to_club reads both at 0x69ebeb..0x69ebf9). The team's
 * nation is at team + 0x53 and the nation's name at nation + 4 (0x460db4..0x460db7). A manager with a club and a national team
 * keeps the club's name. A manager with neither job keeps the original name.
 *
 * The club name is in code page 1252 (club.dat). The path code passes a name to the host as it is, and
 * a host path is UTF-8, so the suggestion is converted to UTF-8 (cp1252.h). Without that a club like
 * Köln or Málaga fails to save. */
#include "win.h"
#include "cp1252.h"

#include <stdio.h>
#include <string.h>

#define GAME_DATE 0xae2c90u        /* s16 day of the year from 0, s16 year */
#define PLAYER_MANAGER 0xb74c26u   /* per player slot, 0x300 bytes apart: the manager */
#define PLAYER_SLOT 0xb77c7au      /* s16: the player slot that has the turn */
#define NAME_BUFFER 0xdd6ed4u      /* the text of the name entry of the file dialog */
#define SAVE_ROUTINE 0x00838e10u   /* the save callback of Save Game */
#define JOB_MANAGER 5              /* staff + 0x28: manager of the national team (0x69ebf5) */
#define JOB_PLAYER_MANAGER 12      /* the other job that 0x69ebf9 accepts */
#define CLUB_MAX 24                /* characters of the club name in the suggestion */
#define NATION_MAX 50              /* characters of the nation's name: the whole field */
#define NAME_BYTES 101             /* the bytes of either name as UTF-8 (cp1252.h), with the end */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* The game date: day of the year from 0 and year at GAME_DATE, as year, month and day of the month
 * from 1. */
int win_game_date(int *year, int *month, int *day)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11 && d >= days[m] + (m == 1 && leap(y))) d -= days[m] + (m == 1 && leap(y)), m++;
    *year = y, *month = m + 1, *day = d + 1;
    return 1;
}

/* The name of the manager's national team: the nation's name, else the team's long name. Empty when the
 * manager has no national team. */
static void nation_name(char *out, size_t size, uint32_t manager)
{
    int job = (int8_t)g_mem[manager + 0x28];
    uint32_t team = job == JOB_MANAGER || job == JOB_PLAYER_MANAGER ? rd32(g_mem, manager + 0x24) : 0;
    uint32_t nation = team ? rd32(g_mem, team + 0x53) : 0;
    out[0] = 0;
    if (nation) cp1252_file_name(out, size, (const char *)(g_mem + nation + 4), NATION_MAX);
    if (team && !out[0]) cp1252_file_name(out, size, (const char *)(g_mem + team + 4), NATION_MAX);
}

/* The suggested name in out, or 0 when the player has no club and no national team. */
static int suggest(char *out, size_t size)
{
    uint32_t slot = (uint32_t)(int16_t)rd16(g_mem, PLAYER_SLOT);
    uint32_t manager = rd32(g_mem, PLAYER_MANAGER + slot * 0x300u);
    if (!manager) return 0;
    uint32_t club = rd32(g_mem, manager + 0x39);
    char name[NAME_BYTES];
    name[0] = 0;
    if (club) {
        cp1252_file_name(name, sizeof name, (const char *)(g_mem + club + 0x38), CLUB_MAX);
        if (!name[0]) cp1252_file_name(name, sizeof name, (const char *)(g_mem + club + 4), CLUB_MAX);
    } else {
        nation_name(name, sizeof name, manager);
    }
    if (!name[0]) return 0;

    int year, month, day;
    win_game_date(&year, &month, &day);
    snprintf(out, size, "%s %04d-%02d-%02d", name, year, month, day);
    return 1;
}

void o_0058f500(void);

/* The file dialog, cdecl (14 arguments). */
RT_WRAP(0058f500, file_dialog)
{
    char name[NAME_BYTES + 16];
    if (rt_arg(5) == NAME_BUFFER && rt_arg(6) == SAVE_ROUTINE && !g_mem[NAME_BUFFER] && suggest(name, sizeof name)) {
        memcpy(g_mem + NAME_BUFFER, name, strlen(name) + 1);
        fprintf(stderr, "recomp: save name suggested: %s\n", name);
    }
    o_0058f500();
}
