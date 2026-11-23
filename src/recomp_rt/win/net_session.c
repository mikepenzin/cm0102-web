/* Drop in, drop out and host quit (0.7.0 slice 6, #45, docs/specs/2026-10-27-online-play-0.7.0-spec.md).
 *
 * What the original already does (docs/research/2026-10-27-network-code-reading.md, section 7):
 * - A human manager whose connection closes goes on holiday. Network::receive_packet 0x77af20 frees the slot and
 *   calls scrman_remove_player_records 0x7ff7b0, which reaches setup_on_network_player_left 0x838c00. It calls
 *   HUMAN_MANAGER::set_on_holiday(1, 0) (0x838c3e..0x838c41) for a manager who is made and not on holiday, and
 *   frees a manager row that is still being made (state 0 or 5 at +0x5f, 0x838c48). Away is this holiday.
 * - Rejoin. A joiner who connects again gets setup_on_network_player_joined 0x838a80. When a manager is
 *   computer controlled (+8 of the human manager object 0xb63c98) or on holiday (+0xc), it opens Manager Status
 *   (0x81ff10, 0x838a85..0x838a95, 0x838b86..0x838b9b). A click on a manager's row (command 0x18,
 *   setup_handle_manager_status_input 0x820950 at 0x820b9d) opens the password box (0x822ec0, input 0x8230b0,
 *   0x820bf2..0x820c01) when the manager has a password, else the confirm of resume_control (0x823420,
 *   0x820c36..0x820c45). The right password calls set_computer_controlled(0) and set_on_holiday(0)
 *   (0x823363..0x82337a) and gives the club back. A restored game does the same: setup_assign_restored_network_
 *   managers 0x8387e0 makes each manager with a password computer controlled until its owner returns
 *   (0x83888d..0x8388a5). So rejoin needs no new code: the runtime only has to see the drop (net_core.c,
 *   keepalive timers).
 * - Host quit. ScreenManager::disconnect_network 0x7fc690 sends the quit packet 0x04 to every joiner
 *   (0x7fc738..0x7fc766). On a joiner, opcode 4 ends the client loop run_network_client 0x7fdc00 (0x7fdeba), and
 *   the game list input returns -2 (0x81fe11..0x81fe31). The original shows nothing, and it has no save at a quit.
 *   A host that crashes or drops sends no 0x04: the joiner learns of it only at its next click (0x569456).
 *
 * What this file adds:
 * - The host's save when it closes the window (or the script quits), which skips the original's quit question. The
 *   close waits for the poll of the screen loop (0x7fcc14 in ScreenManager::monitor 0x7fc7e0), saves there, then
 *   ends the program (net_session_defer_quit). The save is the Auto Save's: save_game_data 0x82e430 with the flags
 *   0x16 and the path of _makepath 0x945b13 at 0x7617c9 (no drive, the save folder of the game options [0xae3138] +
 *   0xf, the game's file name + 0x113, ".sav"). Exit Game gets no save of the runtime: the original asks "Save Game
 *   Before Exit" when the game changed since its last save (0x762b9e..0x762c3d), and its No stands (decision of
 *   2026-10-28 for Q7).
 * - The joiner's box. After the client loop ends because the host left (net_core.c, nc_host_left), the game's
 *   message box 0x5e8290 says so. The original's network error box is the same box (0x56946b). Then Network Play
 *   goes back to the start screen.
 * - Kick (Q39): the list of the human managers online and the Kick of one player slot, for the Network tab of the
 *   Options box (options_screen.c).
 * - Clear errors (0.7.0 slice 7): the joiner's box names why the session ended. The host's KICK control packet
 *   (net_core.c) gives "kicked", a connection that broke with no quit packet gives "the connection was lost", and
 *   the host's quit packet gives "the host left" (net_errors.c). */
#include "win.h"
#include "invite.h"
#include "net_core.h"
#include "season.h"
#include "net_errors.h"
#include "../host_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NETWORK_HOST 0xb77c80u      /* 1 on the host, 0 on a joiner (setup_create_or_join_network_game 0x8388e0 at
                                     * 0x838993..0x83899f); scrman_remove_player_records 0x7ff7b0 tests it */
#define NETWORK_SESSION 0xb77c90u   /* the screen manager's Network object (+0x3840 of 0xb74450), 0 with none */
#define GAME_OPTIONS 0xae3138u      /* +0xf the save folder, +0x113 the game's file name (0x7617ab..0x7617bb) */
#define SAVE_GAME 0x0082e430u       /* cdecl (path, flags) */
#define SAVE_FLAGS 0x16u            /* the Auto Save's (0x7617d5) */
#define MAKEPATH 0x00945b13u        /* cdecl (out, drive, folder, name, extension), as at 0x7617c9 */
#define MESSAGE_BOX 0x005e8290u     /* cdecl (title, text, 0) */

static int turns;                   /* human turns of the game that runs: it has started */

void net_session_turn(void) { turns++; }

int net_session_hosting(void)
{
    return rd32(g_mem, NETWORK_HOST) == 1 && rd32(g_mem, NETWORK_SESSION) != 0;
}

/* The path of the Auto Save in game memory, made by the game's own _makepath as at 0x7617c9, else 0 when the game
 * options are not there. A game that was never saved has no file name yet: it takes "Network Game". */
static uint32_t save_path(void)
{
    static uint32_t out, fallback, extension;
    uint32_t options = rd32(g_mem, GAME_OPTIONS);
    if (!options) return 0;
    if (!out) {
        out = win_static_alloc(0x200);
        fallback = win_static_alloc(16);
        extension = win_static_alloc(8);
        snprintf((char *)g_mem + fallback, 16, "Network Game");
        snprintf((char *)g_mem + extension, 8, ".sav");
    }
    uint32_t name = options + 0x113;
    uint32_t args[5] = {out, 0, options + 0xf, g_mem[name] ? name : fallback, extension};
    rt_callback(MAKEPATH, 5, args);
    g_mem[out + 0x1ff] = 0;
    return out;
}

static int must_save(void) { return net_session_hosting() && turns > 0 && season_game_on() && !rt_unwinding; }

static void save_now(void)
{
    uint32_t path = save_path();
    if (!path) return;
    fprintf(stderr, "recomp: net: the host quits: saving %s\n", (const char *)g_mem + path);
    uint32_t args[2] = {path, SAVE_FLAGS};
    uint32_t r = rt_callback(SAVE_GAME, 2, args);
    fprintf(stderr, "recomp: net: the host's save ended (result %u)\n", r);
}

/* At free_game_data 0x82ff40 (season.c): the game ends, and the next one starts with no human turn. */
void net_session_game_ends(void)
{
    turns = 0;
    nc_hold_others(NC_INVALID);    /* the hold of the screen loop's reads (below) ends with the game */
}

/* A window close, or the script's quit, while this game hosts a game that has started. The program does not end
 * in the event loop: that runs inside any call of the game that pumps messages, also in the middle of a day. It
 * ends at the next poll of the screen loop (net_session_screen_idle), after the save, or after QUIT_WAIT_MS with no
 * save when the game does not come back to its screen loop. */
#define QUIT_WAIT_MS 20000u
static uint64_t quit_asked;

int net_session_defer_quit(void)
{
    if (quit_asked || !must_save()) return 0;
    quit_asked = hn_ms();
    fprintf(stderr, "recomp: net: the host quits: the save waits for the screen loop\n");
    return 1;
}

int net_session_screen_idle(void)
{
    if (!quit_asked) return 0;
    if (must_save()) save_now();
    return 1;
}

int net_session_quit_overdue(void)
{
    if (!quit_asked || hn_ms() - quit_asked < QUIT_WAIT_MS) return 0;
    fprintf(stderr, "recomp: net: the host quits with no save: the screen loop did not come back\n");
    return 1;
}

static void box(const char *title, const char *text)
{
    static uint32_t title_at, text_at;
    if (!title_at) {
        title_at = win_static_alloc(64);
        text_at = win_static_alloc(400);
    }
    snprintf((char *)g_mem + title_at, 64, "%s", title);
    snprintf((char *)g_mem + text_at, 400, "%s", text);
    fprintf(stderr, "recomp: net box: %s: %s\n", title, text);
    uint32_t args[3] = {title_at, text_at, 0};
    rt_callback(MESSAGE_BOX, 3, args);
}

static int back_to_start;           /* the host left: Network Play goes back once more, to the start screen */

void o_007fdc00(void);

/* ScreenManager::run_network_client, thiscall (ScreenManager *), no arguments: the joiner's whole session. */
RT_WRAP(007fdc00, run_network_client)
{
    nc_host_left_clear();
    o_007fdc00();
    net_password_reset();          /* the join passed: the hello carried the password, and the next host gets none */
    if (rt_unwinding || !nc_host_left()) return;
    int kicked = nc_kicked();      /* read before the clear */
    nc_host_left_clear();
    uint32_t eax = cpu.eax;
    char text[400];
    net_error_text(text, sizeof text, kicked ? NE_KICKED : nc_host_broke() ? NE_CONNECTION_LOST : NE_HOST_LEFT,
                   NULL, NULL, 0);
    box("Network Game", text);
    cpu.eax = eax;
    back_to_start = 1;
}

void o_0081ef70(void);

/* setup_handle_network_play_input, cdecl (short item, -1 for none): the screen result. After the client loop the
 * game list input returns -2 (0x81fe31), and the screen loop goes back one screen (0x7fd423), to Network Play
 * (build 0x81edd0). Its first input after the host left gives -2 as its Back does, so the joiner is on the start
 * screen (0x81bab0, which pushes Network Play at 0x81cb99). */
RT_WRAP(0081ef70, network_play_input)
{
    if (back_to_start) {
        back_to_start = 0;
        fprintf(stderr, "recomp: net: the host left: back to the start screen\n");
        rt_ret_cdecl(0xfffffffeu);
        return;
    }
    uint16_t invite = (int16_t)rt_arg(0) == -1 ? invite_command(INVITE_NETWORK_PLAY) : 0;
    if (invite) {                  /* an invite link: Join Network Game (invite.c) */
        invite_call(o_0081ef70, invite);
        return;
    }
    if ((int16_t)rt_arg(0) >= 0) invite_clicked();
    o_0081ef70();
}

/* The human managers online, for the Kick list of the Network tab (Q39). The screen manager keeps up to 16
 * channels, 0x300 bytes apart, with the count of their records at 0xb77c5a + 2 * channel. A record is 0x30 bytes
 * from 0xb74c24 + 0x300 * channel: +0 the player slot (s16; 0 is this computer), +2 the manager's staff row or 0
 * while the manager is being made (scrman_add_player_record 0x7ff630 at 0x7ff6d8..0x7ff6f2). The name is the one
 * of Manager Status (database_format_staff_name 0x53dba0, called at 0x820210 with 200 and style 1), the club the
 * staff row's club at +0x39 with its short name at +0x38 (net_lobby.c). */
#define CHANNEL_RECORDS 0xb74c24u
#define CHANNEL_COUNTS 0xb77c5au
#define STAFF_NAME 0x0053dba0u
#define STAFF_STATE 0x5fu
#define STAFF_CLUB 0x39u
#define CLUB_SHORT 0x38u

int net_session_managers(NetManager *out, int max)
{
    static uint32_t buf;
    int n = 0;
    if (!net_session_hosting()) return 0;
    if (!buf) buf = win_static_alloc(200);
    for (uint32_t ch = 0; ch < 16; ch++) {
        int count = (int16_t)rd16(g_mem, CHANNEL_COUNTS + ch * 2);
        for (int rec = 0; rec < count && rec < 16 && n < max; rec++) {
            uint32_t r = CHANNEL_RECORDS + ch * 0x300u + (uint32_t)rec * 0x30u;
            int slot = (int16_t)rd16(g_mem, r);
            if (slot <= 0 || slot > 16) continue;
            /* One row for each player slot, that is each joining computer: Kick closes its connection, so every
             * manager of that computer leaves. A computer with two managers has a record for each (the second
             * through add_manager_record 0x7ff8f0, called at 0x8233dc): the row names both. */
            NetManager *m = NULL;
            for (int i = 0; i < n; i++)
                if (out[i].slot == slot) m = &out[i];
            if (!m) {
                m = &out[n++];
                memset(m, 0, sizeof *m);
                m->slot = slot;
            }
            uint32_t staff = rd32(g_mem, r + 2);
            if (!staff || g_mem[staff + STAFF_STATE] != 1) continue;   /* being made: no name or club yet */
            g_mem[buf] = 0;
            uint32_t args[4] = {staff, buf, 200, 1};
            rt_callback(STAFF_NAME, 4, args);
            size_t at = strlen(m->name);
            snprintf(m->name + at, sizeof m->name - at, "%s%s", at ? " and " : "", (const char *)g_mem + buf);
            uint32_t club = rd32(g_mem, staff + STAFF_CLUB);
            at = strlen(m->club);
            if (club) snprintf(m->club + at, sizeof m->club - at, "%s%s", at ? ", " : "",
                               (const char *)g_mem + club + CLUB_SHORT);
            m->managers++;
        }
    }
    return n;
}

/* The socket of a player slot: the index at +0x4b8 + 2 * slot of the Network object (s16, -1 with none; release_
 * slot_socket 0x77ac00 at 0x77ac13), then the handle at that index of the connection table at +0 of the object
 * (TcpIp::send_to_connection 0x8ae8d0 at 0x8ae8d1..0x8ae8f6), 16 entries (0x77a7eb). */
static uint32_t slot_socket(int slot)
{
    uint32_t net = rd32(g_mem, NETWORK_SESSION);
    if (!net_session_hosting() || slot < 1 || slot > 16) return NC_INVALID;
    int index = (int16_t)rd16(g_mem, net + 0x4b8u + (uint32_t)slot * 2u);
    uint32_t table = rd32(g_mem, net);
    if (index < 0 || index >= 16 || !table) return NC_INVALID;
    return rd32(g_mem, table + (uint32_t)index * 4u);
}

int net_session_kick(int slot)
{
    uint32_t h = slot_socket(slot);
    if (h == NC_INVALID) return -1;
    fprintf(stderr, "recomp: net: Kick of player slot %d\n", slot);
    return nc_kick(h);
}

/* 0.7.0 slice 7: the ping of a player slot's connection (net_core.c control packets), -1 when none is known. */
int net_session_ping(int slot)
{
    uint32_t h = slot_socket(slot);
    return h == NC_INVALID ? -1 : nc_ping_ms(h);
}


/* The packet trace (RECOMP_NET_TRACE=1, test runs): what the host's screen loop ScreenManager::monitor 0x7fc7e0 does
 * with each packet of a human manager. A line at each start of the loop (with its caller), one for each packet that
 * its read reads (Network::receive_packet, call at 0x7fcaed): the out slot, the first body bytes and, for each
 * channel, the slot of its first record, its record count and the flags +0x16, +0x1a and +0x1e of that record, which
 * the loop reads as "has run", "takes no input" and "done" (0x7fc889..0x7fc898, 0x7fcb48, 0x7fd25a..0x7fd27b); and
 * one with the result of Display::process_command_packet for it (call at 0x7fcb68), the widget that the loop gives
 * the channel's input, or -1. A joiner prints each click packet that it sends (net_core.c). */
#define SCREENS 0xb74450u
#define NET_BUFFER (SCREENS + 0x37feu)     /* the screen manager's network buffer, the packet body */
#define CURRENT_CHANNEL (SCREENS + 0x382au)

static int tracing(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_NET_TRACE") != NULL;
    return on;
}

static int trace_channels(char *line, size_t room, int at)
{
    for (uint32_t ch = 0; ch < 16 && at < (int)room; ch++) {
        int count = (int16_t)rd16(g_mem, CHANNEL_COUNTS + ch * 2);
        if (count <= 0) continue;
        uint32_t r = SCREENS + 0x7d4u + ch * 0x300u;
        at += snprintf(line + at, room - (size_t)at, " ch%u=slot%d,n%d,%u%u%u", ch, (int16_t)rd16(g_mem, r), count,
                       rd32(g_mem, r + 0x16), rd32(g_mem, r + 0x1a), rd32(g_mem, r + 0x1e));
    }
    return at;
}

void o_007fc7e0(void);

RT_WRAP(007fc7e0, monitor_trace)
{
    if (tracing()) {
        char line[700];
        int at = snprintf(line, sizeof line, "recomp: trace: %llu screen loop starts (from 0x%x):",
                          (unsigned long long)hn_ms(), rd32(g_mem, cpu.esp));
        trace_channels(line, sizeof line, at);
        fprintf(stderr, "%s\n", line);
    }
    o_007fc7e0();
}

/* One human manager's packet at a time. The input of a screen first checks that the screen is not out of date: Manager
 * Status (setup_handle_manager_status_input 0x820950) compares the human managers in use and count_idle_channels
 * 0x7fff70 with the counts that its build saved, and returns -11 (build again) when they differ, before it looks at
 * the widget (0x8209ee..0x820a38). A Finish changes those counts for every channel. The screen loop gives each other
 * channel's input -1 in its passes with no packet, one channel a pass (0x7fcb8b..0x7fcc1d), and that builds its
 * screen again. A packet of another channel in the very next pass found its screen out of date, and the original
 * dropped its click: through the relay three joiners' Finish clicks came within 5 ms, and the host stayed on Manager
 * Status (slice 9's quit leg, 2026-10-30, packet trace: "channel 3 gets 22", "widget 22 gives -11"). Over WinSock,
 * packets of three sockets almost never come in the same few milliseconds. So after the screen loop's read (call at
 * 0x7fcaed) gives a packet of one human manager, the other joiners' connections wait (nc_hold_others) until the loop
 * has made one read with no packet for each channel and one more; the order of the packets stays the same. */
static int hold_reads;              /* reads with no packet still to come before the other joiners' packets */

void o_0077af20(void);

RT_WRAP(0077af20, screen_loop_read)
{
    uint32_t ret = rd32(g_mem, cpu.esp), out = rt_arg(1);
    o_0077af20();
    if (rt_unwinding || ret != 0x7fcaf2u) return;
    if (cpu.eax && net_session_hosting()) {
        int channels = 0;
        for (uint32_t ch = 0; ch < 16; ch++) channels += (int16_t)rd16(g_mem, CHANNEL_COUNTS + ch * 2) > 0;
        hold_reads = channels + 1;
        nc_hold_others(slot_socket((int16_t)rd16(g_mem, out)));
    } else if (!cpu.eax && hold_reads && !--hold_reads) {
        nc_hold_others(NC_INVALID);
    }
    if (!tracing() || !cpu.eax) return;
    char line[700];
    uint32_t body = rd32(g_mem, NET_BUFFER);
    int at = snprintf(line, sizeof line, "recomp: trace: %llu packet of slot %d:", (unsigned long long)hn_ms(),
                      (int16_t)rd16(g_mem, out));
    for (int i = 0; i < 10; i++) at += snprintf(line + at, sizeof line - (size_t)at, " %02x", g_mem[body + (uint32_t)i]);
    at += snprintf(line + at, sizeof line - (size_t)at, " |");
    trace_channels(line, sizeof line, at);
    fprintf(stderr, "%s\n", line);
}

void o_00564080(void);

RT_WRAP(00564080, process_trace)
{
    uint32_t ret = rd32(g_mem, cpu.esp);
    o_00564080();
    if (tracing() && !rt_unwinding && ret == 0x7fcb6du)
        fprintf(stderr, "recomp: trace: channel %d gets %d\n", (int16_t)rd16(g_mem, CURRENT_CHANNEL), (int16_t)cpu.eax);
}

void o_00820950(void);

/* setup_handle_manager_status_input, cdecl (s16 widget): Finish gives -5 (0x820cf5..0x820d06). */
RT_WRAP(00820950, status_input_trace)
{
    int16_t widget = (int16_t)rt_arg(0);
    o_00820950();
    if (tracing() && !rt_unwinding && (widget != -1 || (int32_t)cpu.eax != 0))
        fprintf(stderr, "recomp: trace: Manager Status input of channel %d: widget %d gives %d\n",
                (int16_t)rd16(g_mem, CURRENT_CHANNEL), widget, (int32_t)cpu.eax);
}
