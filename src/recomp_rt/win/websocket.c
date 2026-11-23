/* The WebSocket client (websocket.h). */
#include "websocket.h"
#include "lobby_http.h"
#include "secure.h"
#include "../host_net.h"

#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define HEAD_MAX 8192
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

enum { OP_CONT = 0, OP_TEXT = 1, OP_BINARY = 2, OP_CLOSE = 8, OP_PING = 9, OP_PONG = 10 };

struct WsConn {
    SecureConn *c;
    uint8_t *in;                   /* bytes read and not yet taken */
    size_t in_len, in_cap, taken;  /* taken: the frame that the last ws_poll gave, dropped at the next call */
    uint8_t *msg;                  /* the parts of a message that came in several frames */
    size_t msg_len, msg_cap;
    int msg_op;
    int ended, sent_close, close_code;
    uint32_t rng;
};

/* The masks and the key only need to be hard to guess for a machine on the path between the game and the server. */
static uint32_t next_random(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return *s = x ? x : 0x9e3779b9u;
}

static void random_bytes(uint32_t *s, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) out[i] = (uint8_t)(next_random(s) >> 11);
}

void ws_accept_key(const char *key, char out[29])
{
    char joined[64 + sizeof GUID];
    unsigned char sum[20];
    size_t n = 0;
    snprintf(joined, sizeof joined, "%s%s", key, GUID);
    mbedtls_sha1((const unsigned char *)joined, strlen(joined), sum);
    mbedtls_base64_encode((unsigned char *)out, 29, &n, sum, sizeof sum);
    out[n < 29 ? n : 28] = 0;
}

size_t ws_frame(uint8_t *out, int op, const void *p, size_t n, const uint8_t mask[4])
{
    size_t h = 0;
    out[h++] = (uint8_t)(0x80 | op);
    if (n < 126) out[h++] = (uint8_t)(0x80 | n);
    else if (n < 65536) {
        out[h++] = 0x80 | 126;
        out[h++] = (uint8_t)(n >> 8), out[h++] = (uint8_t)n;
    } else {
        out[h++] = 0x80 | 127;
        for (int i = 7; i >= 0; i--) out[h++] = (uint8_t)((uint64_t)n >> (8 * i));
    }
    memcpy(out + h, mask, 4);
    h += 4;
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) out[h + i] = b[i] ^ mask[i & 3];
    return h + n;
}

static int left_ms(uint64_t until)
{
    uint64_t now = hn_ms();
    return now >= until ? 0 : (int)(until - now);
}

static int send_op(WsConn *w, int op, const void *p, size_t n, int timeout_ms)
{
    uint8_t mask[4], small[256], *f = n + 14 <= sizeof small ? small : malloc(n + 14);
    if (!f) return -1;
    random_bytes(&w->rng, mask, 4);
    size_t len = ws_frame(f, op, p, n, mask);
    int r = sc_send_all(w->c, f, len, timeout_ms);
    if (f != small) free(f);
    return r;
}

/* The value of a header in a head that ends with its blank line, or NULL. */
static const char *header(const char *head, const char *name, char *out, size_t room)
{
    size_t n = strlen(name);
    for (const char *l = strstr(head, "\r\n"); l && l[2]; l = strstr(l + 2, "\r\n")) {
        const char *v = l + 2;
        if (strncasecmp(v, name, n) || v[n] != ':') continue;
        v += n + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t len = strcspn(v, "\r\n");
        if (len >= room) len = room - 1;
        memcpy(out, v, len);
        out[len] = 0;
        return out;
    }
    return NULL;
}

WsConn *ws_open(const char *url, int timeout_ms, int *status, char *body, size_t room)
{
    uint64_t until = hn_ms() + (uint64_t)timeout_ms;
    *status = 0;
    if (body && room) body[0] = 0;
    /* ws:// and wss:// have the form of http:// and https:// (lobby_http_parse) up to the path. */
    int secure = !strncmp(url, "wss://", 6);
    if (!secure && strncmp(url, "ws://", 5)) return NULL;
    const char *rest = url + (secure ? 6 : 5), *path = strchr(rest, '/');
    char base[300], host[256];
    unsigned port;
    int tls;
    if (!path || (size_t)(path - rest) + 8 >= sizeof base) return NULL;
    snprintf(base, sizeof base, "%s%.*s", secure ? "https://" : "http://", (int)(path - rest), rest);
    if (!lobby_http_parse(base, host, sizeof host, &port, &tls)) return NULL;

    WsConn *w = calloc(1, sizeof *w);
    if (!w) return NULL;
    w->rng = (uint32_t)hn_ms() * 2654435761u ^ (uint32_t)(uintptr_t)w;
    char why[160];
    w->c = sc_open(host, port, tls, left_ms(until), why, sizeof why);
    if (!w->c) {
        fprintf(stderr, "recomp: relay: %s: %s\n", host, why);
        free(w);
        return NULL;
    }
    uint8_t raw[16];
    char key[32], expect[29], port_text[8] = "";
    size_t n = 0;
    random_bytes(&w->rng, raw, sizeof raw);
    mbedtls_base64_encode((unsigned char *)key, sizeof key, &n, raw, sizeof raw);
    key[n] = 0;
    ws_accept_key(key, expect);
    if (port != (tls ? 443u : 80u)) snprintf(port_text, sizeof port_text, ":%u", port);
    char *head = malloc(HEAD_MAX + 1);
    if (!head) goto failed;
    int len = snprintf(head, HEAD_MAX, "GET %s HTTP/1.1\r\nHost: %s%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                       "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nUser-Agent: Bosman-Edition\r\n\r\n",
                       path, host, port_text, key);
    if (len <= 0 || len >= HEAD_MAX || sc_send_all(w->c, head, (size_t)len, left_ms(until)) < 0) goto failed;

    /* The answer's head, one read at a time: the frames that follow it stay for ws_poll. */
    size_t got = 0;
    char *end = NULL;
    while (!end) {
        if (got == HEAD_MAX) goto failed;
        long r = sc_recv(w->c, head + got, HEAD_MAX - got);
        if (r > 0) {
            got += (size_t)r;
            head[got] = 0;
            end = strstr(head, "\r\n\r\n");
            continue;
        }
        if (r != -1 || !left_ms(until) || sc_wait(w->c, 1, left_ms(until)) <= 0) goto failed;
    }
    size_t head_len = (size_t)(end + 4 - head);
    *end = 0;
    *status = strncmp(head, "HTTP/1.", 7) ? 0 : atoi(head + 9);
    char v[64];
    if (*status != 101) {
        /* A refusal: its JSON body (server/README.md, "Relay"), as much as came within a short wait. */
        size_t want = header(head, "Content-Length", v, sizeof v) ? strtoul(v, NULL, 10) : 0;
        uint64_t short_until = hn_ms() + 1000 < until ? hn_ms() + 1000 : until;
        while (got - head_len < want && got < HEAD_MAX) {
            long r = sc_recv(w->c, head + got, HEAD_MAX - got);
            if (r > 0) got += (size_t)r;
            else if (r != -1 || !left_ms(short_until) || sc_wait(w->c, 1, left_ms(short_until)) <= 0) break;
        }
        if (body && room) {
            size_t b = got - head_len < room - 1 ? got - head_len : room - 1;
            memcpy(body, head + head_len, b);
            body[b] = 0;
        }
        goto failed;
    }
    if (!header(head, "Sec-WebSocket-Accept", v, sizeof v) || strcmp(v, expect)) {
        fprintf(stderr, "recomp: relay: %s: the upgrade answer is not a WebSocket\n", host);
        goto failed;
    }
    w->in_cap = 65536;
    w->in = malloc(w->in_cap);
    if (!w->in) goto failed;
    w->in_len = got - head_len;
    memcpy(w->in, head + head_len, w->in_len);
    free(head);
    return w;
failed:
    free(head);
    sc_close(w->c);
    free(w->in);
    free(w);
    return NULL;
}

int ws_send(WsConn *w, int text, const void *p, size_t n, int timeout_ms)
{
    if (w->ended) return -1;
    if (send_op(w, text ? OP_TEXT : OP_BINARY, p, n, timeout_ms) < 0) {
        w->ended = 1, w->close_code = 1006;
        return -1;
    }
    return 0;
}

/* The length of the whole frame at the start of the bytes, 0 when it has not all come, -1 when it is too long
 * or masked. Sets its parts. */
static size_t frame_at(const uint8_t *b, size_t n, int *fin, int *op, const uint8_t **payload, uint64_t *len)
{
    if (n < 2) return 0;
    size_t h = 2;
    uint64_t l = b[1] & 0x7f;
    if (l == 126) {
        if (n < 4) return 0;
        l = (uint64_t)b[2] << 8 | b[3], h = 4;
    } else if (l == 127) {
        if (n < 10) return 0;
        l = 0;
        for (int i = 0; i < 8; i++) l = l << 8 | b[2 + i];
        h = 10;
    }
    *fin = b[0] & 0x80, *op = b[0] & 0x0f, *len = l;
    if (l > WS_MESSAGE_MAX || (b[1] & 0x80)) return (size_t)-1;     /* a server must not mask (RFC 6455, 5.1) */
    if (n < h + l) return 0;
    *payload = b + h;
    return h + (size_t)l;
}

static int end_with(WsConn *w, int code)
{
    if (!w->sent_close && code != 1006) {
        uint8_t c[2] = {(uint8_t)(code >> 8), (uint8_t)code};
        send_op(w, OP_CLOSE, c, 2, 200);
        w->sent_close = 1;
    }
    w->ended = 1;
    if (!w->close_code) w->close_code = code;
    return -1;
}

int ws_poll(WsConn *w, WsMessage *m)
{
    if (w->taken) {
        memmove(w->in, w->in + w->taken, w->in_len - w->taken);
        w->in_len -= w->taken, w->taken = 0;
    }
    if (w->ended) return -1;
    for (;;) {
        int fin, op;
        const uint8_t *payload = NULL;
        uint64_t len = 0;
        size_t size = frame_at(w->in, w->in_len, &fin, &op, &payload, &len);
        if (size == (size_t)-1) return end_with(w, len > WS_MESSAGE_MAX ? 1009 : 1002);
        if (!size) {
            if (w->in_len == w->in_cap) {
                size_t cap = w->in_cap * 2;
                if (cap > WS_MESSAGE_MAX + 16) cap = WS_MESSAGE_MAX + 16;
                uint8_t *bigger = cap > w->in_cap ? realloc(w->in, cap) : NULL;
                if (!bigger) return end_with(w, 1009);
                w->in = bigger, w->in_cap = cap;
            }
            long r = sc_recv(w->c, w->in + w->in_len, w->in_cap - w->in_len);
            if (r > 0) {
                w->in_len += (size_t)r;
                continue;
            }
            if (r == -1) return 0;
            return end_with(w, 1006);              /* the connection broke, or closed with no close frame */
        }
        if (op == OP_PING) {
            send_op(w, OP_PONG, payload, (size_t)len, 1000);
        } else if (op == OP_CLOSE) {
            w->close_code = len >= 2 ? payload[0] << 8 | payload[1] : 1005;
            return end_with(w, w->close_code == 1005 ? 1000 : w->close_code);
        } else if (op == OP_TEXT || op == OP_BINARY || op == OP_CONT) {
            if (fin && op != OP_CONT && !w->msg_len) {
                m->text = op == OP_TEXT, m->data = payload, m->len = (size_t)len;
                w->taken = size;
                return 1;
            }
            if (op != OP_CONT) w->msg_op = op, w->msg_len = 0;
            if (w->msg_len + len > WS_MESSAGE_MAX) return end_with(w, 1009);
            if (w->msg_len + len > w->msg_cap) {
                size_t cap = w->msg_len + (size_t)len + 4096;
                uint8_t *bigger = realloc(w->msg, cap);
                if (!bigger) return end_with(w, 1009);
                w->msg = bigger, w->msg_cap = cap;
            }
            memcpy(w->msg + w->msg_len, payload, (size_t)len);
            w->msg_len += (size_t)len;
            if (fin) {
                m->text = w->msg_op == OP_TEXT, m->data = w->msg, m->len = w->msg_len;
                w->msg_len = 0;                    /* the bytes stay until the next message's first part */
                w->taken = size;
                return 1;
            }
        }
        /* A frame with no message for the caller (a pong, a part): drop it and read on. */
        memmove(w->in, w->in + size, w->in_len - size);
        w->in_len -= size;
    }
}

int ws_wait(WsConn *w, int ms)
{
    int fin, op;
    const uint8_t *payload;
    uint64_t len;
    if (w->ended || frame_at(w->in + w->taken, w->in_len - w->taken, &fin, &op, &payload, &len)) return 1;
    return sc_wait(w->c, 1, ms) != 0;
}

int ws_close_code(const WsConn *w) { return w->close_code ? w->close_code : 1006; }

void ws_close(WsConn *w, int code)
{
    if (!w) return;
    if (!w->ended && code) end_with(w, code);
    sc_close(w->c);
    free(w->in);
    free(w->msg);
    free(w);
}
