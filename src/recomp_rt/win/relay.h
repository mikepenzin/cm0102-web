/* Joins through the relay (0.7.0 slice 5, #45, docs/specs/2026-10-27-online-play-0.7.0-spec.md).
 *
 * The relay is a byte pipe on the server (server/README.md, "Relay"). The game never sees it: each human
 * manager's connection is an ordinary TCP socket on 127.0.0.1 (hn_pair), and a thread of this module passes
 * the bytes of the other end to the relay and back. So the game's 4-byte length framing, its packets and the
 * join check (net_core.h) go through unchanged, and nc_recv reads the same bytes as on a LAN.
 *
 * Host. A host whose game is open in the lobby holds one secure WebSocket to the relay
 * (wss://<lobby>/v1/games/<code>/relay?role=host&token=<hostToken>). For each human manager the relay opens a
 * channel ({"t":"open","ch":n}). The thread makes a socket pair for it and gives one end to the game's listen
 * socket as a new connection (relay_take_joiner, from net_core's serve_listeners), with the address
 * 0.66.255.<n> (LR_RELAY_PEERS, lobby_rows.h). A channel that the relay names at once after the host connects
 * again belongs to a stream that broke with the old connection: the host kicks it. A relay connection that
 * another connection with the same token replaced (close code 4001) is not opened again for that code and
 * token. The bytes that the host's game sends on a channel go in one binary frame, the channel byte and the
 * batch, at most one frame each RELAY_BATCH_MS: the free plan bills WebSocket messages (server/README.md, "Free
 * plan"). Bytes after a quiet RELAY_BATCH_MS go after RELAY_QUIET_MS, so the host's 5-byte answer to a click
 * (opcode 9) does not wait: the joiner's client loop polls the second widget of a press only after it (two-game
 * run through the deployed relay, 2026-10-28). Later bytes wait up to RELAY_BATCH_MS. A channel sends a batch only when it has bytes, and a quiet connection
 * sends "ping" every RELAY_PING_MS. When the host's game closes a channel's socket, the relay gets a kick for
 * it. When the relay closes a channel, the game's socket of it ends.
 *
 * Human manager. A connect to a lobby row (net_lobby.c) asks the lobby for a ticket (POST
 * /v1/games/<code>/join), opens wss://<lobby>/v1/games/<code>/relay?role=manager&ticket=<ticket>, and gives the
 * game one end of a socket pair. Its thread passes the bytes both ways. Its own bytes wait at most RELAY_JOIN_MS:
 * they are a few clicks, and a longer wait lets the host's next screen come before the second click of one press,
 * which the click rule of slice 2b (user32.c) then holds back (two-game run, 2026-10-28: 3 clicks of 4 at 50 ms).
 * When the relay closes the connection (the host left, a kick), the game's socket ends. */
#ifndef RELAY_H
#define RELAY_H

#include "../host_net.h"

#include <stddef.h>
#include <stdint.h>

#define RELAY_BATCH_MS 50
#define RELAY_QUIET_MS 2              /* the bytes that the game writes at once still go in one frame */
#define RELAY_JOIN_MS 10
#define RELAY_PING_MS 30000
#define RELAY_BATCH_MAX (256u << 10)  /* a batch this large goes at once */
#define RELAY_CHANNELS 16
#define RELAY_QUEUE_MAX (4u << 20)   /* bytes for the game that it does not read: the channel ends */
#define RELAY_FRESH_MS 1000          /* see "Host" above: channels that the relay names this soon after a connect */

/* Host, on the game's thread. Each call with another code or token starts a new relay connection. */
void relay_host_set(const char *lobby, const char *code, const char *token);
void relay_host_stop(void);
/* At the program's end: stops the host's relay connection and waits at most ms until the last bytes of each
 * channel (the QUIT of net_core.c) went to the relay and the connection closed. */
void relay_host_finish(int ms);
/* The next human manager's connection for the game's listen socket, or HN_INVALID. *addr is
 * LR_RELAY_PEERS + channel in network order. */
hn_sock relay_take_joiner(uint32_t *addr, uint16_t *port);

typedef enum {
    RJ_OK,
    RJ_GONE,            /* the game is not open (unknown_game, bad_ticket) */
    RJ_VERSION,         /* host_version holds the host's version */
    RJ_PASSWORD,
    RJ_ATTEMPTS,        /* too many wrong passwords this minute */
    RJ_FULL,            /* 16 human managers are connected */
    RJ_HOST_AWAY,       /* the host is not connected to the relay */
    RJ_NAME,            /* the word filter refused the name, or the lobby refused a field */
    RJ_FULL_TODAY,
    RJ_RELAY_DOWN,      /* the lobby gave a ticket, and the relay did not answer */
    RJ_UNAVAILABLE      /* the lobby did not answer */
} RelayJoin;

/* Human manager, on the game's thread in its connect. Waits at most timeout_ms for the ticket and as long again
 * for the relay. RJ_OK: *s is the game's end of the connection. */
RelayJoin relay_join(const char *lobby, const char *code, const char *version, const char *name,
                     const char *password, int timeout_ms, hn_sock *s, char *host_version, size_t room);

/* Tests: the URL of the relay for a lobby address, or 0 when the address is not one. */
int relay_url(char *out, size_t room, const char *lobby, const char *code, const char *query);

#endif
