/* Invite links (invite.h). */
#include "invite.h"
#include "lobby_http.h"
#include "lobby_rows.h"
#include "../host_net.h"
#include "options.h"
#include "../host.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHEME "cm0102://join/"
#define CODE_CHARS "23456789ABCDEFGHJKMNPQRSTUVWXYZ"   /* the lobby's alphabet (server/src/validate.ts) */

void net_lobby_install(void);       /* net_lobby.c: sets the lobby address */

static void walk_to(const char *code, uint32_t fixed);

static int looking;                 /* a lookup waits for its answer */
static char asked[9];               /* its game code */
static LobbyRow last;
static int have_last;

int invite_parse(const char *text, char code[9])
{
    size_t n = strlen(SCHEME);
    if (!text) return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)text[i]) != SCHEME[i]) return 0;
    const char *p = text + n;
    char c[9];
    for (int i = 0; i < 8; i++) {
        int ch = toupper((unsigned char)p[i]);
        if (!ch || !strchr(CODE_CHARS, ch)) return 0;
        c[i] = (char)ch;
    }
    c[8] = 0;
    p += 8;
    if (*p == '/') p++;
    if (*p) return 0;
    memcpy(code, c, sizeof c);
    return 1;
}

void invite_open(const char *text)
{
    char code[9];
    if (!invite_parse(text, code)) {
        fprintf(stderr, "recomp: invite: not an invite link, ignored\n");
        return;
    }
    if (!options_bool(OPT_ONLINE_PLAY)) {
        fprintf(stderr, "recomp: invite: %s: Online play is off, so the game does not ask the lobby\n", code);
        return;
    }
    net_lobby_install();
    if (!lobby_usable()) {
        fprintf(stderr, "recomp: invite: %s: the game has no lobby address\n", code);
        return;
    }
    fprintf(stderr, "recomp: invite: the game asks the lobby for %s\n", code);
    lobby_game_fetch(code);
    memcpy(asked, code, sizeof asked);
    looking = 1;
}

void invite_poll(void)
{
    if (!looking) return;
    LobbyRow row;
    int result;
    if (!lobby_game_take(&row, &result)) return;
    memcpy(row.code, asked, sizeof asked);   /* a failed lookup gives no row: the code is the one that the link named */
    looking = 0;
    if (result != LH_OK) {
        fprintf(stderr, "recomp: invite: %s\n", result == LH_API_ERROR ? "the lobby has no open game with this code" :
                result == LH_FULL_TODAY ? "online games are full today" : "the game cannot reach the lobby");
        /* The join still goes through the game list, to an address that gives the reason (see invite_join). */
        walk_to(row.code, result == LH_API_ERROR ? LR_GONE_ROW : result == LH_FULL_TODAY ? LR_FULL_ROW : LR_DOWN_ROW);
        return;
    }
    fprintf(stderr, "recomp: invite: %s is \"%s\", hosted by %s, version %s%s\n", row.code, row.g.name,
            row.g.host_name, row.g.version, row.g.password ? ", with a password" : "");
    last = row, have_last = 1;
    invite_join(&row);
}

const LobbyRow *invite_last(void) { return have_last ? &last : NULL; }

/* The join of an invite link (Q17): the original's own screens, in their own order, as a player clicks them.
 * Each of the three inputs takes the command word 0xdd6bde when no widget is clicked (-1):
 * - Setup Game, setup_input 0x81bd70: the word at 0x81c914; command 5 starts the network session
 *   (ScreenManager::start_network_session 0x7ff500, 0x81cb77) and opens Network Play (0x81cb94..0x81cba3). The
 *   input can return before it reads the word (0x81bd89: it opens Game Settings at the first start of a user
 *   folder), so the walk gives command 5 until Network Play's input runs, at most SETUP_TRIES times.
 * - Network Play, setup_handle_network_play_input 0x81ef70: the word at 0x81efae; command 0x13 (Join Network Game)
 *   opens the game list (build 0x81f8d0, input 0x81fbd0, 0x81f10c..0x81f124).
 * - The game list, setup_handle_select_network_game_input 0x81fbd0: its REQ asks the lobby too (net_lobby.c).
 *   When the list shows the game's row, the row's text goes into the Join field 0xdd70dc (0x32 bytes, 0x81f249),
 *   as a click on the row puts it there (0x81fd08..0x81fd63), and the command word gets Next (-3, net_password.c),
 *   which joins the game named in the field (0x81fd99..0x81fdc4). From there the join is the one of a click on the
 *   row: through the relay (or the LAN when the host is on it), the password box of slice 6, and the reasons of a
 *   refused join.
 * When the lookup failed, or the list shows no row within ROW_WAIT_MS, the Join field gets an address of
 * 0.66.255.0/24 that has no row (lobby_rows.h), so the reason shows in the original's "Unable To Join" box
 * (net_lobby.c, before_connect): the game is no longer open, full today, or the lobby does not answer.
 * The walk ends after WALK_MS, at the join, or when the player clicks a widget on one of the three screens. */
#define ROW_WAIT_MS 15000
#define WALK_MS 60000
#define SETUP_TRIES 3

enum { WALK_NONE, WALK_START, WALK_LIST };
static int walk, setup_tries;
static char walk_code[9];
static uint32_t walk_fixed;         /* the address to join in place of the row (host order), or 0 */
static uint64_t walk_since, list_since;

static void walk_to(const char *code, uint32_t fixed)
{
    snprintf(walk_code, sizeof walk_code, "%s", code);
    walk_fixed = fixed;
    walk = WALK_START, setup_tries = 0, walk_since = hn_ms(), list_since = 0;
    fprintf(stderr, "recomp: invite: the game opens Join Network Game for %s\n", code);
}

void invite_join(const LobbyRow *row) { walk_to(row->code, 0); }

static int walking(void)
{
    if (walk != WALK_NONE && hn_ms() - walk_since >= WALK_MS) {
        fprintf(stderr, "recomp: invite: the join of %s ends: the game list did not open\n", walk_code);
        walk = WALK_NONE;
    }
    return walk != WALK_NONE;
}

void invite_clicked(void)
{
    if (walk == WALK_NONE) return;
    fprintf(stderr, "recomp: invite: the player clicked: the join of %s ends\n", walk_code);
    walk = WALK_NONE;
}

uint16_t invite_command(int screen)
{
    if (!walking()) return 0;
    if (screen == INVITE_SETUP && walk == WALK_START) {
        if (++setup_tries > SETUP_TRIES) {
            fprintf(stderr, "recomp: invite: Network Play did not open: the join of %s ends\n", walk_code);
            walk = WALK_NONE;
            return 0;
        }
        return 5;
    }
    if (screen == INVITE_NETWORK_PLAY && walk == WALK_START) {
        walk = WALK_LIST;
        return 0x13;
    }
    return 0;
}

int invite_list_join(char *out, unsigned room)
{
    if (!walking()) return 0;
    walk = WALK_LIST;                       /* the game list may be open already */
    uint64_t now = hn_ms();
    if (!list_since) list_since = now;
    char text[50];
    uint32_t fixed = walk_fixed;
    if (!fixed && !lr_shown_text(walk_code, text)) {
        if (now - list_since < ROW_WAIT_MS) return 0;
        fprintf(stderr, "recomp: invite: the list shows no row for %s\n", walk_code);
        int r = lr_lobby_result();
        fixed = r == LH_FULL_TODAY ? LR_FULL_ROW : r == LH_UNAVAILABLE ? LR_DOWN_ROW : LR_GONE_ROW;
    }
    if (fixed) snprintf(text, sizeof text, "%u.%u.%u.%u", fixed >> 24, fixed >> 16 & 255, fixed >> 8 & 255, fixed & 255);
    walk = WALK_NONE;
    snprintf(out, room, "%s", text);
    fprintf(stderr, "recomp: invite: Join \"%s\"\n", text);
    return 1;
}

void invite_register(void)
{
    const char *test = getenv("RECOMP_USER_DIR");
    if (test && *test) return;
    host_link_handler(options_bool(OPT_ONLINE_PLAY));
}
