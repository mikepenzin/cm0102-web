/* Joins through the relay (0.7.0 slice 5): WebSocket frames (websocket.c), the socket pair (hn_pair), and a
 * host and a human manager in this process that meet through a fake relay on 127.0.0.1 (relay.c, net_core.c).
 * The fake relay follows server/README.md ("Relay") and counts the frames that the host sends, so the test sees
 * the batches. The real relay's own tests are in server/test.
 *
 *   relay_test                 the tests above
 *   relay_test --https <url>   one lobby_http GET of <url>/v1/health: exit 0 when the lobby answers
 *                              (tests/net/tls_check.py runs it against a local TLS server) */
#include "relay.h"
#include "websocket.h"
#include "lobby_http.h"
#include "net_core.h"
#include "host_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void test_frames(void)
{
    char key[29];
    ws_accept_key("dGhlIHNhbXBsZSBub25jZQ==", key);              /* the example of RFC 6455, section 1.3 */
    CHECK(!strcmp(key, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), "accept key %s", key);
    static uint8_t out[70000 + 14], in[70000];
    const uint8_t mask[4] = {1, 2, 3, 4};
    for (size_t i = 0; i < sizeof in; i++) in[i] = (uint8_t)i;
    size_t n = ws_frame(out, 2, in, 5, mask);
    CHECK(n == 11 && out[0] == 0x82 && out[1] == 0x85 && !memcmp(out + 2, mask, 4) && out[6] == (0 ^ 1) &&
          out[10] == (4 ^ 1), "a 5-byte binary frame");
    n = ws_frame(out, 1, in, 200, mask);
    CHECK(n == 208 && out[0] == 0x81 && out[1] == 0xfe && out[2] == 0 && out[3] == 200, "a 200-byte text frame");
    n = ws_frame(out, 2, in, 70000, mask);
    CHECK(n == 70014 && out[1] == 0xff && out[7] == 0x01 && out[8] == 0x11 && out[9] == 0x70 &&
          out[14 + 69999] == (uint8_t)((69999 & 0xff) ^ mask[69999 & 3]), "a 70000-byte frame");
}

static void test_pair(void)
{
    hn_sock a, b;
    CHECK(hn_pair(&a, &b) == 0, "hn_pair");
    char t[8] = "";
    CHECK(hn_send(a, "ping", 4) == 4 && hn_wait(b, 1, 1000) == 1 && hn_recv(b, t, sizeof t) == 4 && !memcmp(t, "ping", 4),
          "a to b");
    CHECK(hn_send(b, "pong", 4) == 4 && hn_wait(a, 1, 1000) == 1 && hn_recv(a, t, sizeof t) == 4 && !memcmp(t, "pong", 4),
          "b to a");
    CHECK(hn_recv(a, t, sizeof t) < 0 && hn_error() == HN_WOULDBLOCK, "non-blocking");
    hn_close(a);
    CHECK(hn_wait(b, 1, 1000) == 1 && hn_recv(b, t, sizeof t) == 0, "the end of a");
    hn_close(b);
}

static void test_urls(void)
{
    char u[256];
    CHECK(relay_url(u, sizeof u, "http://127.0.0.1:8787", "ABCD2345", "role=host&token=x") &&
          !strcmp(u, "ws://127.0.0.1:8787/v1/games/ABCD2345/relay?role=host&token=x"), "%s", u);
    CHECK(relay_url(u, sizeof u, "https://bosman.example.workers.dev", "ABCD2345", "role=manager&ticket=t") &&
          !strcmp(u, "wss://bosman.example.workers.dev/v1/games/ABCD2345/relay?role=manager&ticket=t"), "%s", u);
    CHECK(!relay_url(u, sizeof u, "", "ABCD2345", "x"), "no lobby address");
}

/* ---- the fake relay ---- */

enum { NEW, HOST, MANAGER, GONE };

typedef struct {
    hn_sock s;
    int role, ch;
    uint8_t in[1 << 18];
    size_t n;
} Peer;

static Peer peers[8];
static hn_sock listener;
static volatile int fr_stop, host_frames, host_bytes, kicks, manager_frames;
static volatile int fr_close_manager;     /* the test asks: close the human managers with 4000 */

static void raw_send(hn_sock s, const void *p, size_t n)
{
    const uint8_t *b = p;
    while (n) {
        long r = hn_send(s, b, n);
        if (r > 0) b += r, n -= (size_t)r;
        else if (hn_wait(s, 0, 100) < 0) return;
    }
}

static void server_frame(hn_sock s, int op, const void *p, size_t n)
{
    uint8_t h[10];
    size_t k = 0;
    h[k++] = (uint8_t)(0x80 | op);
    if (n < 126) h[k++] = (uint8_t)n;
    else if (n < 65536) h[k++] = 126, h[k++] = (uint8_t)(n >> 8), h[k++] = (uint8_t)n;
    else {
        h[k++] = 127;
        for (int i = 7; i >= 0; i--) h[k++] = (uint8_t)((uint64_t)n >> (8 * i));
    }
    raw_send(s, h, k);
    raw_send(s, p, n);
}

static void control(const char *t, int ch)
{
    char m[48];
    snprintf(m, sizeof m, "{\"t\":\"%s\",\"ch\":%d}", t, ch);
    for (int i = 0; i < 8; i++)
        if (peers[i].role == HOST) server_frame(peers[i].s, 1, m, strlen(m));
}

static void drop_peer(Peer *p, int code)
{
    if (p->role == MANAGER) control("close", p->ch);
    if (p->role == HOST) {         /* as the relay: the host left, so every human manager goes with 4000 */
        p->role = GONE;
        for (int i = 0; i < 8; i++)
            if (peers[i].role == MANAGER) drop_peer(&peers[i], 4000);
    }
    if (code) {
        uint8_t c[2] = {(uint8_t)(code >> 8), (uint8_t)code};
        server_frame(p->s, 8, c, 2);
    }
    hn_close(p->s);
    p->role = GONE, p->n = 0;
}

static void answer(hn_sock s, int status, const char *json)
{
    char r[512];
    int n = snprintf(r, sizeof r, "HTTP/1.1 %d X\r\nX-Bosman-Api: 1\r\nContent-Type: application/json\r\n"
                     "Content-Length: %zu\r\nConnection: close\r\n\r\n%s", status, strlen(json), json);
    raw_send(s, r, (size_t)n);
}

/* A request of a NEW peer. 1 when the peer stays (a WebSocket). */
static int request(Peer *p, char *head)
{
    char key[64] = "", accept[29];
    char *k = strstr(head, "Sec-WebSocket-Key: ");
    if (k) sscanf(k + 19, "%63s", key);
    if (!strncmp(head, "POST /v1/games/GOOD0001/join", 28)) answer(p->s, 200, "{\"ticket\":\"t-1\",\"expireSeconds\":60}");
    else if (!strncmp(head, "POST /v1/games/AWAY0001/join", 28)) answer(p->s, 200, "{\"ticket\":\"t-a\",\"expireSeconds\":60}");
    else if (!strncmp(head, "POST /v1/games/OLDV0001/join", 28))
        answer(p->s, 409, "{\"error\":\"version_mismatch\",\"message\":\"x\",\"hostVersion\":\"9.9.9\"}");
    else if (!strncmp(head, "POST /v1/games/", 15)) answer(p->s, 404, "{\"error\":\"unknown_game\",\"message\":\"x\"}");
    else if (!strncmp(head, "GET /v1/games/GOOD0001/relay?role=host&token=tok ", 49) && key[0]) {
        p->role = HOST;
    } else if (!strncmp(head, "GET /v1/games/GOOD0001/relay?role=manager&ticket=t-1 ", 53) && key[0]) {
        int host = 0, used[16] = {0};
        for (int i = 0; i < 8; i++) {
            host |= peers[i].role == HOST;
            if (peers[i].role == MANAGER) used[peers[i].ch] = 1;
        }
        if (!host) {
            answer(p->s, 409, "{\"error\":\"host_not_connected\",\"message\":\"x\"}");
            return 0;
        }
        p->role = MANAGER;
        while (used[p->ch]) p->ch++;
    } else {
        answer(p->s, 409, "{\"error\":\"host_not_connected\",\"message\":\"x\"}");
        return 0;
    }
    ws_accept_key(key, accept);
    char r[256];
    int n = snprintf(r, sizeof r, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
    raw_send(p->s, r, (size_t)n);
    if (p->role == MANAGER) control("open", p->ch);
    return 1;
}

/* One client frame of a WebSocket peer, from its bytes. The frame's size, or 0 when it has not all come. */
static size_t client_frame(Peer *p)
{
    if (p->n < 6) return 0;
    uint64_t len = p->in[1] & 0x7f;
    size_t h = 2;
    if (len == 126) len = (uint64_t)p->in[2] << 8 | p->in[3], h = 4;
    else if (len == 127) {
        len = 0;
        for (int i = 0; i < 8; i++) len = len << 8 | p->in[2 + i];
        h = 10;
    }
    if (p->n < h + 4 + len) return 0;
    uint8_t *mask = p->in + h, *d = p->in + h + 4;
    for (uint64_t i = 0; i < len; i++) d[i] ^= mask[i & 3];
    int op = p->in[0] & 15;
    if (op == 8) {
        drop_peer(p, 0);
        return 0;
    }
    if (op == 1 && len == 4 && !memcmp(d, "ping", 4)) server_frame(p->s, 1, "pong", 4);
    else if (p->role == HOST && op == 1 && strstr((char *)d, "kick")) {
        int ch = atoi(strstr((char *)d, "\"ch\":") + 5);
        kicks++;
        for (int i = 0; i < 8; i++)
            if (peers[i].role == MANAGER && peers[i].ch == ch) drop_peer(&peers[i], 4003);
    } else if (p->role == HOST && op == 2 && len >= 1) {
        host_frames++, host_bytes += (int)len - 1;
        for (int i = 0; i < 8; i++)
            if (peers[i].role == MANAGER && (peers[i].ch == d[0] || d[0] == 255)) server_frame(peers[i].s, 2, d + 1, len - 1);
    } else if (p->role == MANAGER && op == 2) {
        static uint8_t f[1 << 18];
        manager_frames++;
        f[0] = (uint8_t)p->ch;
        memcpy(f + 1, d, len);
        for (int i = 0; i < 8; i++)
            if (peers[i].role == HOST) server_frame(peers[i].s, 2, f, len + 1);
    }
    return h + 4 + (size_t)len;
}

static void fake_relay(void *unused)
{
    (void)unused;
    while (!fr_stop) {
        hn_wait(listener, 1, 2);
        hn_sock c = hn_accept(listener, NULL, NULL);
        for (int i = 0; c != HN_INVALID && i < 8; i++)
            if (peers[i].role == GONE || (!peers[i].s && peers[i].role == NEW)) {
                memset(&peers[i], 0, sizeof peers[i]);
                peers[i].s = c, c = HN_INVALID;
            }
        if (fr_close_manager) {
            fr_close_manager = 0;
            for (int i = 0; i < 8; i++)
                if (peers[i].role == MANAGER) drop_peer(&peers[i], 4000);
        }
        for (int i = 0; i < 8; i++) {
            Peer *p = &peers[i];
            if (!p->s || p->role == GONE) continue;
            long r = hn_recv(p->s, p->in + p->n, sizeof p->in - p->n);
            if (r == 0 || (r < 0 && hn_error() != HN_WOULDBLOCK)) {
                drop_peer(p, 0);
                continue;
            }
            if (r > 0) p->n += (size_t)r;
            if (p->role == NEW) {
                char *end;
                p->in[p->n] = 0;
                if (!(end = strstr((char *)p->in, "\r\n\r\n"))) continue;
                char *body = strstr((char *)p->in, "Content-Length: ");
                size_t want = body ? strtoul(body + 16, NULL, 10) : 0;
                if (p->n < (size_t)(end + 4 - (char *)p->in) + want) continue;
                if (!request(p, (char *)p->in)) {
                    hn_close(p->s);
                    p->role = GONE;
                }
                p->n = 0;
                continue;
            }
            size_t used;
            while (p->role != GONE && (used = client_frame(p)) > 0) {
                memmove(p->in, p->in + used, p->n - used);
                p->n -= used;
            }
        }
    }
}

/* ---- the host and the human manager in this process ---- */

static intptr_t incoming(uint32_t *addr, uint16_t *port) { return relay_take_joiner(addr, port); }

static long read_for(hn_sock s, char *buf, size_t room, size_t want, int ms)
{
    size_t n = 0;
    uint64_t until = hn_ms() + (uint64_t)ms;
    while (n < want && hn_ms() < until) {
        long r = hn_recv(s, buf + n, room - n);
        if (r > 0) n += (size_t)r;
        else if (r == 0) return n ? (long)n : 0;
        else hn_wait(s, 1, 10);
    }
    return (long)n;
}

static uint32_t nc_accept_for(uint32_t listen, int ms, uint32_t *addr)
{
    uint64_t until = hn_ms() + (uint64_t)ms;
    for (;;) {
        uint16_t port;
        uint32_t h = nc_accept(listen, addr, &port);
        if (h != NC_INVALID || hn_ms() >= until) return h;
        hn_sleep(5);
    }
}

static void test_relay(void)
{
    unsigned port = 0;
    for (unsigned p = 47800; p < 47900 && !port; p++) {
        listener = hn_socket(1);
        if (hn_bind(listener, 0x0100007fu, (uint16_t)p) == 0 && hn_listen(listener, 8) == 0) port = p;
        else hn_close(listener);
    }
    CHECK(port, "a free port for the fake relay");
    char base[64];
    snprintf(base, sizeof base, "http://127.0.0.1:%u", port);
    hn_thread(fake_relay, NULL);

    /* Refused joins. */
    hn_sock s;
    char v[32];
    CHECK(relay_join(base, "GONE0001", "1.0", "PC-1", "", 3000, &s, v, sizeof v) == RJ_GONE, "unknown_game");
    CHECK(relay_join(base, "OLDV0001", "1.0", "PC-1", "", 3000, &s, v, sizeof v) == RJ_VERSION && !strcmp(v, "9.9.9"),
          "version_mismatch: %s", v);
    CHECK(relay_join(base, "GOOD0001", "1.0", "PC-1", "", 3000, &s, v, sizeof v) == RJ_HOST_AWAY,
          "no host on the relay yet");

    /* The host: a listen socket of the game, with the join check, and its relay connection. */
    nc_set_identity("1.0", "", "");
    nc_set_ping_every(3600000);    /* after the first, no PING of slice 7 comes between the packets counted here */
    NcHooks hooks = {0};
    hooks.incoming = incoming;
    nc_set_hooks(&hooks);
    uint32_t l = nc_socket(2, 1);
    CHECK(nc_bind(l, 0x0100007fu, 0) == 0 && nc_listen(l, 16) == 0 && nc_nonblocking(l, 1) == 0, "host listens");
    relay_host_set(base, "GOOD0001", "tok");
    hn_sleep(300);

    /* A human manager joins. Its game end does what net_core's hello does. */
    CHECK(relay_join(base, "GOOD0001", "1.0", "PC-1", "", 3000, &s, v, sizeof v) == RJ_OK, "the join");
    const char *hello = "BOSMAN HELLO 1\nversion=1.0\npassword=\n\n";
    raw_send(s, hello, strlen(hello));
    uint32_t addr = 0;
    uint32_t a = nc_accept_for(l, 3000, &addr);
    const uint8_t *ab = (const uint8_t *)&addr;
    CHECK(a != NC_INVALID && ab[0] == 0 && ab[1] == 66 && ab[2] == 255 && ab[3] == 0,
          "the host's game accepts the human manager from 0.66.255.0 (%u.%u.%u.%u)", ab[0], ab[1], ab[2], ab[3]);
    char buf[70000];
    long n = read_for(s, buf, sizeof buf, 10, 2000);
    CHECK(n == 10 && !memcmp(buf, "BOSMAN OK\n", 10), "the join check passes through the relay (%ld)", n);

    /* The host sends five packets at once: one batch, one frame. The game's 4-byte lengths go through. */
    /* Slice 7's first PING goes at the host's next call after the accept: let it pass before the count. */
    uint32_t avail;
    nc_readable(a, &avail);
    read_for(s, buf, sizeof buf, 1000, 300);
    hn_sleep(100);                 /* a quiet time after the last frame */
    int frames0 = host_frames;
    uint64_t t0 = hn_ms();
    for (int i = 0; i < 5; i++) {
        uint8_t pkt[4 + 100] = {100, 0, 0, 0, (uint8_t)i};
        CHECK(nc_send(a, pkt, sizeof pkt) == (long)sizeof pkt, "host send %d", i);
    }
    n = read_for(s, buf, sizeof buf, 520, 3000);
    uint64_t took = hn_ms() - t0;
    CHECK(n == 520 && buf[0] == 100 && buf[4] == 0 && buf[104 + 4] == 1 && buf[416 + 4] == 4, "the packets arrive (%ld)", n);
    CHECK(host_frames - frames0 == 1, "five packets in one frame (%d)", host_frames - frames0);
    CHECK(took < RELAY_BATCH_MS, "after a quiet time the frame goes at once (%llu ms)", (unsigned long long)took);
    /* The next packets within RELAY_BATCH_MS wait for it: at most one frame each RELAY_BATCH_MS. */
    frames0 = host_frames;
    t0 = hn_ms();
    for (int i = 0; i < 3; i++) {
        uint8_t pkt[4 + 10] = {10, 0, 0, 0, 9};
        nc_send(a, pkt, sizeof pkt);
        hn_sleep(5);
    }
    n = read_for(s, buf, sizeof buf, 42, 3000);
    took = hn_ms() - t0;
    CHECK(n == 42 && host_frames - frames0 == 1 && took >= RELAY_BATCH_MS / 2,
          "packets soon after a frame wait for the batch (%ld bytes, %d frames, %llu ms)", n, host_frames - frames0,
          (unsigned long long)took);

    /* A large screen: 70000 bytes in one frame. */
    static uint8_t big[70000];
    for (size_t i = 0; i < sizeof big; i++) big[i] = (uint8_t)(i * 7);
    CHECK(nc_send(a, big, sizeof big) == (long)sizeof big, "host sends 70000 bytes");
    n = read_for(s, buf, sizeof buf, sizeof big, 3000);
    CHECK(n == (long)sizeof big && !memcmp(buf, big, sizeof big), "70000 bytes arrive whole (%ld)", n);

    /* The human manager sends a click. */
    uint8_t click[4 + 8] = {8, 0, 0, 0, 6, 1, 2, 3, 4, 5, 6, 7};
    raw_send(s, click, sizeof click);
    n = 0;
    uint64_t until = hn_ms() + 2000;
    while (n < (long)sizeof click && hn_ms() < until) {
        long r = nc_recv(a, buf + n, sizeof buf - (size_t)n);
        if (r > 0) n += r;
        else hn_sleep(5);
    }
    CHECK(n == (long)sizeof click && !memcmp(buf, click, sizeof click), "the click arrives at the host (%ld)", n);
    CHECK(nc_peers() == 1, "the host counts one human manager (%d)", nc_peers());

    /* The host's game closes the connection: a kick, and the human manager's socket ends. */
    nc_close(a);
    n = read_for(s, buf, sizeof buf, 1, 3000);
    CHECK(n == 0 && kicks == 1, "the host's close kicks the channel (read %ld, kicks %d)", n, kicks);
    hn_close(s);

    /* A second join, then the relay closes the human managers (the host left). */
    hn_sleep(100);
    CHECK(relay_join(base, "GOOD0001", "1.0", "PC-1", "", 3000, &s, v, sizeof v) == RJ_OK, "a second join");
    raw_send(s, hello, strlen(hello));
    a = nc_accept_for(l, 3000, &addr);
    CHECK(a != NC_INVALID && read_for(s, buf, sizeof buf, 10, 2000) == 10, "the second join passes");
    fr_close_manager = 1;
    n = read_for(s, buf, sizeof buf, 1, 3000);
    CHECK(n == 0, "the relay's close ends the human manager's socket (%ld)", n);
    /* Open item 130: a game that writes after the end (a joiner's PONG, the host's PING) still reads the end, not a
     * reset (relay.c, "Ends"). */
    hn_send(s, "late", 4);
    hn_sleep(200);
    n = hn_recv(s, buf, sizeof buf);
    CHECK(n == 0, "a write after the end, and the human manager's socket still ends (%ld, %d)", n,
          n < 0 ? hn_error() : 0);
    hn_close(s);
    until = hn_ms() + 2000;
    while (hn_ms() < until && nc_recv(a, buf, sizeof buf) != 0) hn_sleep(10);
    CHECK(nc_recv(a, buf, sizeof buf) == 0, "and the host's socket of it");
    uint8_t late[4 + 5] = {5, 0, 0, 0, 7, 1, 2, 3, 4};
    nc_send(a, late, sizeof late);
    hn_sleep(200);
    n = nc_recv(a, buf, sizeof buf);
    CHECK(n == 0, "a write after the end, and the host's socket still ends (%ld, %d)", n, n < 0 ? nc_error() : 0);
    nc_close(a);

    /* The host's last bytes before its end (net_core.c's QUIT at a window close) reach the human manager, and its
     * socket then ends normally, not with a reset (test 4 of the spec, 2026-10-29: "the connection was lost"). */
    hn_sleep(200);
    CHECK(relay_join(base, "GOOD0001", "1.0", "PC-1", "", 3000, &s, v, sizeof v) == RJ_OK, "a third join");
    raw_send(s, hello, strlen(hello));
    a = nc_accept_for(l, 3000, &addr);
    CHECK(a != NC_INVALID && read_for(s, buf, sizeof buf, 10, 2000) == 10, "the third join passes");
    nc_readable(a, &avail);        /* slice 7's first PING, read off before the count */
    read_for(s, buf, sizeof buf, 1000, 300);
    hn_sleep(150);
    uint8_t last[4 + 6] = {6, 0, 0, 0, 0xbe, 'Q', 'U', 'I', 'T', '\n'};
    raw_send(s, "pong", 4);        /* bytes of the human manager's game that nobody reads before the end */
    CHECK(nc_send(a, last, sizeof last) == (long)sizeof last, "the host sends its last bytes");
    relay_host_finish(1000);       /* at once, as at the host's exit */
    n = 0;
    long r = 1;
    until = hn_ms() + 3000;
    while (hn_ms() < until) {
        r = hn_recv(s, buf + n, sizeof buf - (size_t)n);
        if (r > 0) n += r;
        else if (r == 0 || hn_error() != HN_WOULDBLOCK) break;
        else hn_wait(s, 1, 20);
    }
    CHECK(n == (long)sizeof last && !memcmp(buf, last, sizeof last), "the host's last bytes arrive (%ld)", n);
    CHECK(r == 0, "then the socket ends normally (%ld, error %d)", r, r < 0 ? hn_error() : 0);
    hn_close(s);
    nc_close(a);
    nc_close(l);
    hn_sleep(100);
    fr_stop = 1;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--https")) {
        static LhAnswer a;
        LhResult r = lobby_http(argv[2], "GET", "/v1/health", NULL, NULL, 5000, &a);
        printf("https: result %d status %d body %s\n", (int)r, a.status, a.body);
        return r == LH_OK ? 0 : 1;
    }
    hn_start();
    test_frames();
    test_pair();
    test_urls();
    test_relay();
    printf(failures ? "relay: %d failures\n" : "relay: all passed\n", failures);
    return failures != 0;
}
