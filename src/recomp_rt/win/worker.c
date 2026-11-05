/* The engine worker: a server drives the game at its human turns, with no person at the screen.
 *
 * Off unless RECOMP_WORKER_FD names a file descriptor (worker_proto.h has the protocol). The server starts the game
 * with the SDL dummy drivers and a RECOMP_SCRIPT that loads a save or makes a new game, as the test runs do
 * (tests/fixtures/fixtures.py). From then on the game stops at each human turn where the server must answer: it
 * sends the date and the club of the player and waits for commands.
 *
 * Where it stops: the human turn 0x5cde40 (game.cpp), which the day loop 0x5cccd0 calls once per slice after the
 * day's processing, until it returns 0 (docs/research/2026-10-14-assistant-and-days-inventory.md, 2a). The wrap is
 * in testrun.c. The worker runs before the original's turn, so the news screen of a waiting turn is not drawn.
 *
 * How days go on: the original's holiday. advance n starts the holiday of the manager with the setter 0x5fd2f0, as
 * holiday_next.c ends it; on holiday the screens end on their own (holiday.c; with faster_days on, in their first
 * pass). At the first human turn n or more game days later the worker ends the holiday with the same setter and
 * waits again. If the holiday ends another way (the game ends it), the worker waits at that turn.
 * NOT CHECKED: the value that the original passes to 0x5fd2f0 to start a holiday. The "Go on holiday ?" screen
 * 0x6b2d80 does it on Yes; that path was not read for this file, and the runtime has no copy of the original here.
 * The worker passes 1 and checks the holiday flag (record + 0x9a01) after the call. The Yes path may also do more
 * (the holiday stamp 0x5fdd30, the saved team selection 0x5fed50); the worker does none of that. Read 0x6b2d80
 * before this is used for more than a test.
 *
 * How it saves: the save function 0x82e430 (path, 0x16), the call of the Auto Save at 0x7617d8, into the save folder
 * of the game options (+0xf), as RECOMP_SAVE_AFTER_DAYS does (testrun.c). The answer names the host file.
 *
 * The state: the date at 0xae2c90 (win_game_date, savename.c) and the club of the manager of the player slot that
 * has the turn (manager + 0x39; long name at club + 4, short name at club + 0x38, savename.c). */
#include "win.h"
#include "cp1252.h"
#include "worker_proto.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HUMANS 0xb63c98u            /* the human manager object: +0 records, 0xa6ef bytes each */
#define RECORD_SIZE 0xa6efu
#define ON_HOLIDAY 0x9a01u          /* record: the holiday flag */
#define STAFF_COUNT 0xae2364u       /* humans are the last 16 staff ids */
#define PLAYER_MANAGER 0xb74c26u    /* per player slot, 0x300 bytes apart: the manager */
#define PLAYER_SLOT 0xb77c7au       /* s16: the player slot that has the turn */
#define GAME_DATE 0xae2c90u         /* s16 day of the year from 0, s16 year */
#define GAME_OPTIONS 0xae3138u      /* pointer to the game options; +0xf is the save folder */

#define SET_HOLIDAY 0x5fd2f0u       /* thiscall HUMANS (value, manager or 0), ret 8 */
#define SAVE_GAME 0x0082e430u       /* cdecl (path, flags) */
#define SAVE_FLAGS 0x16u            /* as the Auto Save at 0x7617d8 */

#define CLUB_CHARS 60

static int link_on = -1;            /* -1: not read yet */
static WorkerLink link;
static int advancing, until;        /* an advance runs until the serial day until */

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t current_manager(void)
{
    return rd32(g_mem, PLAYER_MANAGER + (uint32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

/* The index 0..15 of a human manager, or -1 (holiday_next.c). */
static int human_slot(uint32_t manager)
{
    if (!manager) return -1;
    int slot = (int)rd32(g_mem, manager) - (int)rd32(g_mem, STAFF_COUNT) + 16;
    return slot >= 0 && slot < 16 ? slot : -1;
}

static uint32_t record(int slot) { return rd32(g_mem, rd32(g_mem, HUMANS)) + (uint32_t)slot * RECORD_SIZE; }

static int on_holiday(int slot) { return slot >= 0 && rd32(g_mem, record(slot) + ON_HOLIDAY) != 0; }

static void set_holiday(uint32_t manager, uint32_t value)
{
    uint32_t args[2] = {value, manager};
    call_this(SET_HOLIDAY, rd32(g_mem, HUMANS), 2, args);
}

static int today(void)
{
    return worker_serial((int16_t)rd16(g_mem, GAME_DATE), (int16_t)rd16(g_mem, GAME_DATE + 2));
}

static void end_program(int code)
{
    if (rt_on_fatal) rt_on_fatal();
    SDL_Quit();
    exit(code);
}

static void send(const char *line, int n)
{
    if (n < 0 || worker_write_all(link.fd, line, (size_t)n) != 0) {
        fprintf(stderr, "recomp: worker: the server end is gone\n");
        end_program(1);
    }
}

static void send_state(int turn_event)
{
    char club[CLUB_CHARS * 3 + 1] = "", line[WORKER_LINE_MAX];
    WorkerState st = {0, 0, 0, NULL};
    win_game_date(&st.year, &st.month, &st.day);
    uint32_t manager = current_manager();
    uint32_t c = manager ? rd32(g_mem, manager + 0x39) : 0;
    if (c) {
        cp1252_file_name(club, sizeof club, (const char *)(g_mem + c + 4), CLUB_CHARS);
        if (!club[0]) cp1252_file_name(club, sizeof club, (const char *)(g_mem + c + 0x38), CLUB_CHARS);
        if (club[0]) st.club = club;
    }
    send(line, worker_json_state(line, sizeof line, turn_event, &st));
}

static void send_error(const char *why)
{
    char line[WORKER_LINE_MAX];
    send(line, worker_json_error(line, sizeof line, why));
}

static void save(const char *name)
{
    char path[512], host[2048], line[WORKER_LINE_MAX + 2048];
    uint32_t options = rd32(g_mem, GAME_OPTIONS);
    if (!options) { send_error("no game options: no game is loaded"); return; }
    snprintf(path, sizeof path, "%s%s.sav", (const char *)(g_mem + options + 0xf), name);
    uint32_t args[2] = {rt_alloc_str(path), SAVE_FLAGS};
    Cpu saved = cpu;
    uint32_t r = rt_callback(SAVE_GAME, 2, args);
    if (rt_unwinding) {
        fprintf(stderr, "recomp: worker: the save of %s unwound\n", path);
        end_program(1);
    }
    cpu = saved;
    rt_free(args[0]);
    fprintf(stderr, "recomp: worker: saved %s (result %u)\n", path, r);
    if (!win_existing_file(path, host, sizeof host)) { send_error("the save wrote no file"); return; }
    send(line, worker_json_saved(line, sizeof line, host));
}

void worker_turn(void)
{
    if (link_on < 0) {
        const char *v = getenv("RECOMP_WORKER_FD");
        link_on = v && *v;
        if (link_on) link.fd = atoi(v);
    }
    if (!link_on) return;
    uint32_t manager = current_manager();
    int slot = human_slot(manager);
    if (advancing) {
        if (on_holiday(slot) && today() < until) return;    /* the holiday goes on */
        advancing = 0;
        if (on_holiday(slot)) set_holiday(manager, 0);
    }
    send_state(1);
    for (;;) {
        char text[WORKER_LINE_MAX + 1], line[WORKER_LINE_MAX];
        if (worker_read_line(&link, text, sizeof text) < 0) {
            fprintf(stderr, "recomp: worker: the server closed the link\n");
            end_program(0);
        }
        WorkerCommand cmd;
        switch (worker_parse(text, &cmd)) {
        case WK_STATE:
            send_state(0);
            break;
        case WK_SAVE:
            save(cmd.name);
            break;
        case WK_ADVANCE:
            if (slot < 0) { send_error("no human manager has the turn"); break; }
            if (!on_holiday(slot)) set_holiday(manager, 1);
            if (!on_holiday(slot)) { send_error("the holiday did not start"); break; }
            advancing = 1;
            until = today() + cmd.days;
            send(line, worker_json_until(line, sizeof line, until));
            return;
        case WK_QUIT:
            send(line, worker_json_ok(line, sizeof line));
            end_program(0);
            break;
        default:
            send_error(cmd.error);
        }
    }
}
