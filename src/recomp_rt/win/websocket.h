/* A WebSocket client (RFC 6455) for the relay (0.7.0 slice 5, #45, relay.c), over the secure connection
 * (secure.h): wss:// on port 443 for the deployed server, ws:// for the local copy in tests.
 *
 * One thread uses a connection at a time. ws_poll never waits; ws_wait waits for bytes. The client answers a
 * ping with a pong and a close with a close by itself. */
#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stddef.h>
#include <stdint.h>

#define WS_MESSAGE_MAX ((1u << 20) + 64)   /* the relay's limit is 1 MiB and the channel byte */

typedef struct WsConn WsConn;

typedef struct {
    int text;                      /* 1: a text message, 0: binary */
    const uint8_t *data;           /* valid until the next ws_poll or ws_close */
    size_t len;
} WsMessage;

/* Opens url ("wss://host[:port]/path?query" or "ws://..."). NULL when the server refuses the upgrade or does
 * not answer: then *status is the HTTP status (0 when none came) and body holds the start of its answer. */
WsConn *ws_open(const char *url, int timeout_ms, int *status, char *body, size_t room);
/* Sends one message within timeout_ms. 0 on success, -1 when the connection is broken. */
int ws_send(WsConn *w, int text, const void *p, size_t n, int timeout_ms);
/* 1: *m holds a message. 0: none yet. -1: the connection ended; ws_close_code gives the server's code. */
int ws_poll(WsConn *w, WsMessage *m);
/* 1 when bytes may wait for ws_poll within ms, 0 when not. */
int ws_wait(WsConn *w, int ms);
/* The close code that the server sent (1006 when the connection broke with none). */
int ws_close_code(const WsConn *w);
/* Sends a close with code (0: none) and frees the connection. */
void ws_close(WsConn *w, int code);

/* The pieces, for tests. */
void ws_accept_key(const char *key, char out[29]);
/* Writes a client frame (masked with mask) of opcode op to out, which needs n + 14 bytes. Its length. */
size_t ws_frame(uint8_t *out, int op, const void *p, size_t n, const uint8_t mask[4]);

#endif
