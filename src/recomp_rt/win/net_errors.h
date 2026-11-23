/* The reasons of network failures and their texts (0.7.0 slice 7, #45, docs/specs/2026-10-27-online-play-0.7.0-spec.md).
 *
 * Each failure that a player can meet in a network game has one reason here, and each reason has one text that
 * says what went wrong and what to do next. The texts go in the original's own boxes: "Unable To Join" and "Unable
 * To Create" (confirm_push 0x591840, from setup_create_or_join_network_game 0x8388e0 at 0x838a67), the game's
 * message box 0x5e8290 (net_session.c), and the Network tab of the Options box (options_screen.c). All texts that
 * players read in a network game live in net_errors.c, so that they can be read and checked in one place. */
#ifndef NET_ERRORS_H
#define NET_ERRORS_H

#include <stddef.h>

typedef enum {
    NE_NONE,
    /* Joins (the "Unable To Join" box) */
    NE_LOBBY_DOWN,          /* the lobby gives no answer */
    NE_FULL_TODAY,          /* the free plan is used up until 00:00 UTC */
    NE_RELAY_DOWN,          /* the relay gives no answer (slice 5) */
    NE_GAME_GONE,           /* the online game is no longer open */
    NE_HOST_AWAY,           /* the host of an online game is not connected to the relay (slice 5) */
    NE_HOST_REFUSED,        /* a LAN host refused the connection: no game runs there now */
    NE_CONNECT_TIMEOUT,     /* a LAN host gave no answer within NC_CONNECT_MS */
    NE_HOST_UNREACHABLE,    /* no route to the host */
    NE_NO_NETWORK,          /* this computer has no network */
    NE_NO_ANSWER,           /* the host took the connection but did not answer the join check */
    NE_NOT_BOSMAN,          /* the host answered, but not as Bosman Edition */
    NE_VERSION,             /* the host runs another version: the text names it */
    NE_PASSWORD,            /* the game password is wrong */
    NE_ATTEMPTS,            /* too many wrong passwords in this minute (slice 5) */
    NE_GAME_FULL,           /* 16 human managers (slice 5) */
    NE_NAME,                /* the lobby's word filter refused the name (slice 5) */
    NE_JOIN_OTHER,          /* any other failed connect: the text names the error code */
    /* The end of a joiner's session (the game's message box) */
    NE_HOST_LEFT,           /* the host's quit packet came */
    NE_CONNECTION_LOST,     /* the connection to the host broke with no quit packet */
    NE_KICKED,              /* the host used Kick */
    /* Hosting (the "Unable To Create" box) */
    NE_CREATE_PORT,         /* another program holds port 6667 */
    NE_CREATE_OTHER,        /* the listen socket did not open for another reason: the text names the error code */
    NE_COUNT
} NetError;

/* The text of a reason. version: this game's version, host_version: the host's (NE_VERSION, NE_NOT_BOSMAN), code:
 * the error code of NE_JOIN_OTHER and NE_CREATE_OTHER. */
void net_error_text(char *out, size_t room, NetError e, const char *version, const char *host_version, int code);

/* The reason of a failed connect from its WinSock error code (net_core.c, nc_connect_error), or NE_NONE for 0. */
NetError net_error_of_connect(int code);

/* The connection test (net_test.c) and the host's place in the lobby, on the Network tab. */
typedef enum {
    NT_NONE,                /* never run */
    NT_RUNNING,
    NT_OFF,                 /* Online play is off */
    NT_NO_ADDRESS,          /* this build has no lobby address */
    NT_WORKS,
    NT_LOBBY_DOWN,
    NT_LOBBY_LIST_DOWN,     /* the lobby answers, but its list of games does not */
    NT_FULL_TODAY,
    NT_RELAY_DOWN,
    NT_RELAY_BLOCKED,       /* a proxy or firewall took the WebSocket request apart */
    NT_COUNT
} NetTest;

/* The connection test (net_test.c). online: the Online play option, base: the lobby address. It runs on a thread;
 * net_test_state gives NT_RUNNING until it ends, then its result, and NT_NONE before the first test. */
void net_test_start(int online, const char *base);
NetTest net_test_state(int *ms);

/* The result line of the connection test. ms: the time of the lobby's answer, for NT_WORKS. */
void net_test_text(char *out, size_t room, NetTest t, int ms);

/* The line of the host's place in the lobby: state is a LobbyHostState (lobby.h), code the game code when open,
 * refusal the lobby's error code when refused. NULL when there is nothing to say. */
const char *net_host_text(char *out, size_t room, int state, const char *code, const char *refusal);

/* The invite link of a game (0.7.1, #137): the lobby address, "/join/" and the game code. A "/" at the end of the
 * address goes. NULL when the code or the address is empty, or the text does not fit. */
const char *net_invite_link(char *out, size_t room, const char *lobby_address, const char *code);

#endif
