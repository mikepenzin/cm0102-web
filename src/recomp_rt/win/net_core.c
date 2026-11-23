/* The game's sockets (net_core.h). Items 2 to 9, 11 and 13 of section 9 of
 * docs/research/2026-10-27-network-code-reading.md.
 *
 * Handles. The game keeps a socket as a dword and compares it with -1. Each new socket gets the next
 * number, so a handle that the game closes twice (0x8aec17, then 0x8aec24) is unknown the second time
 * and that close fails quietly with WSAENOTSOCK.
 *
 * Blocking. Every host socket is non-blocking. The game sets each socket that it reads non-blocking
 * (FIONBIO), and blocks only in connect (0x8ae74e, 0x8af4e6). connect waits at most NC_CONNECT_MS, so a
 * wrong address does not freeze the game.
 *
 * Whole sends. Network::send_packet 0x77ac30 sends a 4-byte length in four 1-byte sends, then the body,
 * and checks only for -2 from TcpIp::send_to_connection 0x8ae8d0, which can return a short count
 * (0x8ae966). So nc_send takes the whole buffer every time. What the system cannot take now waits in a
 * queue that each later call moves on. The queue also holds the length bytes until the body follows,
 * so a packet leaves in one send with TCP_NODELAY: no wait on the peer's delayed acknowledgement between
 * the length and the body. Any call other than a send to that socket sends what is held.
 *
 * UDP. The game binds each UDP socket to an adapter address on port 6667 (0x8aec05) and broadcasts to
 * the adapter's subnet broadcast. Windows delivers a subnet broadcast to such a socket. macOS does not
 * (tested on this Mac, 2026-10-27), and Linux delivers a broadcast only to sockets on any address. And when two games on one computer share port 6667, the
 * system gives a datagram sent to one address and port to only one of them. So a UDP socket of the game
 * is two host sockets: one on any address, port 6667, shared (SO_REUSEADDR and SO_REUSEPORT), that takes
 * the broadcasts, and one on the adapter address with a port of the system's choice, that sends. The
 * host's ACK goes back to the sender's address and port (0x8aed5a), so it reaches the right game. The
 * same on Windows, so every system behaves alike. The game opens one UDP socket for each adapter
 * (0x8aea80), and each shared socket gets every broadcast. So a shared socket keeps only the datagrams
 * whose sender is in its adapter's subnet, as a socket bound to the adapter address on Windows does.
 * Else a host would answer one request once for each adapter, and a joiner that gets one BYE for each
 * adapter would add a row for each copy after the first (0x77b448).
 *
 * Traffic. Each TCP connection counts bytes, sends and packets in both directions. A packet is a
 * 4-byte length and its body (0x77ac30, 0x77b45e).
 *
 * Join check (0.7.0 slice 4, net_core.h). The joiner's side runs inside connect, which the game calls on a
 * blocking socket and waits for (connect_to_host 0x8ae640, connect at 0x8ae74e). The host's side runs inside
 * accept: TcpIp::accept_connection 0x8ae840 polls accept on each listen socket while a player slot is free
 * (0x77af5a). A new connection waits in a list until its block is read, and accept says "would block" until
 * then. So the game sees only joiners that passed, as if the others never came.
 *
 * Control packets (0.7.0 slice 7, net_core.h). On a connection that passed the join check, both runtimes may put
 * a packet of their own between two packets of the game: the 4-byte length, the opcode NC_CONTROL_OP, which the
 * game never sends (Display::process_command_packet 0x564080 switches on 0..26 through the table at 0x567a1c), and
 * a line "BOSMAN PING <n>", "BOSMAN PONG <n>", "BOSMAN KICK" or "BOSMAN QUIT" (a host that ends at a window close). A send puts one only where the game's own stream is
 * between two packets, and nc_recv takes each one out where the game's reads are between two packets, before
 * the game reads the next length byte. So the game never sees them, and they pass the relay as any bytes.
 *
 * The relay (0.7.0 slice 5, relay.h). A joiner's connect to a lobby row gets a socket from the route hook in
 * place of the system's connect, and a host's listen socket gets the relay's human managers from the incoming
 * hook. Both are ordinary TCP sockets on 127.0.0.1, so everything above runs on them as it is. */
#include "net_core.h"
#include "../host_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SOCKS 256
#define FIRST_HANDLE 0x100u

typedef struct {                   /* the 4-byte length framing of one direction */
    int have;                      /* length bytes seen of the current packet, 0..4 */
    uint8_t len[4];
    uint32_t body;                 /* body bytes still to come */
    uint32_t size;                 /* the body length of the current packet, until its first byte is seen */
    uint32_t op_packets[32];       /* by the packet's first byte, the opcode of 0x564080 (0..26) */
    uint64_t op_bytes[32];
} Frame;

typedef struct {
    uint32_t handle;               /* 0: the entry is free */
    int stream;
    int blocking;                  /* as the game sees it */
    int connected;
    int dead;                      /* a WinSock code once the connection broke in a background send */
    int broadcast;                 /* SO_BROADCAST, for the UDP send socket */
    int accepted;                  /* a joiner's connection on this host */
    int listening;
    int kicked;                    /* the host removed this human manager (nc_kick): the socket is closed */
    int quit_at;                   /* a joiner's connection that broke: 1 + the bytes of the quit packet given */
    int told;                      /* a recv of the game reported that the connection closed or broke */
    int exit_seen;                 /* the host screen now shown holds the confirm of the joiner's own Exit Game */
    char tail[32];                 /* the last bytes from the host, for a text cut between two reads */
    int control;                   /* the connection passed the join check: control packets go both ways */
    uint32_t ping_seq;             /* host: the number of the last PING, and when it went (0: its PONG came) */
    uint64_t ping_sent, ping_at;   /* ping_at: when the next PING goes */
    int ping[NC_PING_KEEP], ping_n;/* host: the last round trips in ms */
    uint8_t wait[NC_CONTROL_MAX + 4];  /* a control packet that waits for the game's packet to end */
    int wait_len;
    int wait_ping;                 /* the waiting packet is a PING: its time starts when it goes */
    uint64_t part_since;           /* when part of a control packet came and the rest did not yet, 0: none */
    hn_sock s;
    hn_sock out;                   /* UDP: the send socket, else HN_INVALID */
    uint32_t net, mask;            /* UDP: the adapter's subnet, for the shared socket's datagrams */
    uint8_t *q;                    /* send queue */
    size_t qlen, qoff, qcap;
    uint32_t peer;
    uint16_t peer_port;
    uint64_t opened;
    NcCounts n, last;              /* last: the counts at the last log line */
    Frame fo, fi;
} Sock;

static Sock socks[MAX_SOCKS];
static uint32_t next_handle = FIRST_HANDLE;
static int error_code;
static FILE *log_file;
static uint64_t last_log;
static int log_ready;

static int connect_ms = NC_CONNECT_MS;
static int host_left;               /* nc_host_left */
static int hold_on;                 /* nc_hold_others: the joiners' connections other than hold_handle wait */
static uint32_t hold_handle;
static int host_broke, kicked;      /* nc_host_broke, nc_kicked (0.7.0 slice 7) */
static int host_quit;               /* the host's QUIT came: it ends on purpose, with no quit packet of the game */
static int connect_error, bind_error;   /* nc_connect_error, nc_bind_error */
static int ping_every = NC_PING_MS;

static const NcHooks *hooks;
static int in_tick;
void nc_set_hooks(const NcHooks *h) { hooks = h; }

/* The join check: this game's version and passwords, and the reason of the last refused connect. */
static char my_version[32], host_password[72], join_password[72], refused_version[32];
static int identity, refused;

void nc_set_identity(const char *version, const char *host_pw, const char *join_pw)
{
    snprintf(my_version, sizeof my_version, "%s", version ? version : "");
    snprintf(host_password, sizeof host_password, "%s", host_pw ? host_pw : "");
    snprintf(join_password, sizeof join_password, "%s", join_pw ? join_pw : "");
    identity = version != NULL;
}

void nc_set_host_password(const char *pw) { snprintf(host_password, sizeof host_password, "%s", pw ? pw : ""); }
void nc_set_join_password(const char *pw) { snprintf(join_password, sizeof join_password, "%s", pw ? pw : ""); }
const char *nc_join_password(void) { return join_password; }
void nc_set_refusal(int why) { refused = why; }

void nc_refusal_clear(void) { refused = NC_REFUSED_NONE, connect_error = 0; }

int nc_refusal(char *host_version, unsigned room)
{
    if (host_version && room) snprintf(host_version, room, "%s", refused_version);
    return refused;
}

int nc_error(void) { return error_code; }
void nc_set_connect_ms(int ms) { connect_ms = ms; }
void nc_set_error(int code) { error_code = code; }

static int fail(int code)
{
    error_code = code;
    return -1;
}

static int fail_host(void) { return fail(hn_error()); }

static Sock *find(uint32_t h)
{
    if (h < FIRST_HANDLE) return NULL;
    for (int i = 0; i < MAX_SOCKS; i++)
        if (socks[i].handle == h) return &socks[i];
    return NULL;
}

/* ---- traffic log ---- */

static void ip_text(char *out, size_t room, uint32_t a)
{
    const uint8_t *b = (const uint8_t *)&a;
    snprintf(out, room, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

void nc_log_to(const char *path)
{
    if (log_file && log_file != stderr) fclose(log_file);
    log_file = NULL;
    if (!path || !*path) return;
    log_file = strcmp(path, "-") ? fopen(path, "a") : stderr;
    if (log_file) last_log = hn_ms();
}

/* The packets of one direction by opcode: "op=packets/bytes" for each opcode seen. */
static void log_ops(const Sock *k, const char *dir, const Frame *f)
{
    fprintf(log_file, "net ops h=%u %s", k->handle, dir);
    for (int i = 0; i < 32; i++)
        if (f->op_packets[i]) fprintf(log_file, " %d=%u/%llu", i, f->op_packets[i], (unsigned long long)f->op_bytes[i]);
    fprintf(log_file, "\n");
}

static void log_line(const Sock *k, const char *what, const NcCounts *c)
{
    char peer[20];
    ip_text(peer, sizeof peer, k->peer);
    fprintf(log_file, "net %s t=%llu h=%u peer=%s:%u secs=%llu out=%llu bytes %llu packets %llu sends in=%llu bytes %llu packets\n",
            what, (unsigned long long)hn_ms(), k->handle, peer, k->peer_port,
            (unsigned long long)((hn_ms() - k->opened) / 1000), (unsigned long long)c->bytes_out,
            (unsigned long long)c->packets_out, (unsigned long long)c->sends,
            (unsigned long long)c->bytes_in, (unsigned long long)c->packets_in);
    if (!strcmp(what, "close") || !strcmp(what, "end")) {
        log_ops(k, "out", &k->fo);
        log_ops(k, "in", &k->fi);
    }
    fflush(log_file);
}

/* Every NC_LOG_MS: one line for each connection that moved bytes, with the counts since the last line. */
static void log_tick(void)
{
    if (!log_file) return;
    uint64_t now = hn_ms();
    if (now - last_log < NC_LOG_MS) return;
    last_log = now;
    for (int i = 0; i < MAX_SOCKS; i++) {
        Sock *k = &socks[i];
        if (!k->handle || !k->connected) continue;
        NcCounts d = {k->n.bytes_out - k->last.bytes_out, k->n.bytes_in - k->last.bytes_in,
                      k->n.packets_out - k->last.packets_out, k->n.packets_in - k->last.packets_in,
                      k->n.sends - k->last.sends, 0};
        k->last = k->n;
        if (d.bytes_out || d.bytes_in) log_line(k, "tick", &d);
    }
}

void nc_log_all(const char *why)
{
    if (!log_file) return;
    for (int i = 0; i < MAX_SOCKS; i++)
        if (socks[i].handle && socks[i].connected) log_line(&socks[i], why, &socks[i].n);
}

static void log_at_exit(void) { nc_log_all("end"); }

/* Feeds n bytes of one direction to its framing. The count of packets that end in them. */
static unsigned frame_feed(Frame *f, const uint8_t *p, uint32_t n)
{
    unsigned done = 0;
    while (n) {
        if (f->body) {
            if (f->size) {         /* the opcode */
                f->op_packets[*p & 31]++, f->op_bytes[*p & 31] += f->size + 4;
                f->size = 0;
            }
            uint32_t take = n < f->body ? n : f->body;
            f->body -= take, p += take, n -= take;
            if (!f->body) done++;
            continue;
        }
        f->len[f->have++] = *p++, n--;
        if (f->have == 4) {
            f->have = 0;
            f->body = (uint32_t)f->len[0] | (uint32_t)f->len[1] << 8 | (uint32_t)f->len[2] << 16 |
                      (uint32_t)f->len[3] << 24;
            /* A length of 0 or above 50,000 is not a packet of the game (0x77ac43, 0x77acb4). */
            if (f->body > 50000u) f->body = 0;
            f->size = f->body;
        }
    }
    return done;
}

static int frame_between(const Frame *f) { return !f->have && !f->body; }

/* ---- send queue ---- */

/* Sends what the queue holds, as far as the system takes it. -1 when the connection broke. */
static int flush(Sock *k)
{
    while (k->qoff < k->qlen && !k->dead) {
        long r = hn_send(k->s, k->q + k->qoff, k->qlen - k->qoff);
        if (r > 0) {
            k->qoff += (size_t)r;
            continue;
        }
        if (r < 0 && hn_error() == HN_WOULDBLOCK) break;
        k->dead = r < 0 ? hn_error() : HN_CONNRESET;
    }
    if (k->qoff == k->qlen) k->qoff = k->qlen = 0;
    k->n.queued = k->qlen - k->qoff;
    return k->dead ? -1 : 0;
}

/* Every call moves the queues on, but a send leaves the bytes that it holds back for its own packet. */
static void serve_listeners(void);
static void close_pending(uint32_t h);
static void control_tick(void);

static void flush_all(const Sock *except)
{
    for (int i = 0; i < MAX_SOCKS; i++)
        if (socks[i].handle && &socks[i] != except && socks[i].qlen) flush(&socks[i]);
    log_tick();
    serve_listeners();
    control_tick();
    if (hooks && hooks->tick && !in_tick) {
        in_tick = 1;
        hooks->tick();
        in_tick = 0;
    }
}

int nc_peers(void)
{
    int n = 0;
    for (int i = 0; i < MAX_SOCKS; i++)
        if (socks[i].handle && socks[i].accepted && socks[i].connected && !socks[i].dead) n++;
    return n;
}

static int enqueue(Sock *k, const uint8_t *p, uint32_t n)
{
    if (k->qoff && k->qoff == k->qlen) k->qoff = k->qlen = 0;
    if (k->qlen - k->qoff + n > NC_QUEUE_MAX) {
        k->dead = HN_CONNRESET;
        return -1;
    }
    if (k->qoff > k->qcap / 2) {   /* move the rest to the front before the queue grows */
        memmove(k->q, k->q + k->qoff, k->qlen - k->qoff);
        k->qlen -= k->qoff, k->qoff = 0;
    }
    if (k->qlen + n > k->qcap) {
        size_t cap = k->qcap ? k->qcap : 4096;
        while (cap < k->qlen + n) cap *= 2;
        uint8_t *q = realloc(k->q, cap);
        if (!q) {
            k->dead = HN_CONNRESET;
            return -1;
        }
        k->q = q, k->qcap = cap;
    }
    memcpy(k->q + k->qlen, p, n);
    k->qlen += n;
    return 0;
}

/* ---- control packets (0.7.0 slice 7, see the head of this file) ---- */

#define CONTROL_TAG "BOSMAN "

/* Builds a control packet with the line text: its length, or 0 when the text is too long. */
static int control_packet(uint8_t *out, const char *text)
{
    size_t n = strlen(text) + 1 + strlen(CONTROL_TAG);
    if (n > NC_CONTROL_MAX) return 0;
    out[0] = (uint8_t)n, out[1] = out[2] = out[3] = 0;
    out[4] = NC_CONTROL_OP;
    memcpy(out + 5, CONTROL_TAG, strlen(CONTROL_TAG));
    memcpy(out + 5 + strlen(CONTROL_TAG), text, strlen(text));
    return (int)n + 4;
}

static void send_waiting(Sock *k);

/* Sends a control packet now when the game's stream is between two packets, else after the game's packet. A packet
 * that already waits gives way to the newer one. */
static void send_control(Sock *k, const char *text)
{
    if (!k->control || k->dead || k->kicked) return;
    k->wait_len = control_packet(k->wait, text);
    k->wait_ping = !strncmp(text, "PING", 4);
    send_waiting(k);
}

/* A control packet that waited for the end of the game's packet. */
static void send_waiting(Sock *k)
{
    if (!k->wait_len || !frame_between(&k->fo) || k->dead) return;
    if (enqueue(k, k->wait, (uint32_t)k->wait_len) == 0) flush(k);
    if (k->wait_ping) k->ping_sent = hn_ms();
    k->wait_len = k->wait_ping = 0;
}

static void control_arrived(Sock *k, const char *line)
{
    unsigned n;
    if (sscanf(line, "PING %u", &n) == 1) {
        char pong[24];
        snprintf(pong, sizeof pong, "PONG %u", n);
        send_control(k, pong);
    } else if (sscanf(line, "PONG %u", &n) == 1) {
        if (!k->ping_sent || n != k->ping_seq) return;
        int ms = (int)(hn_ms() - k->ping_sent);
        memmove(k->ping + 1, k->ping, sizeof k->ping - sizeof k->ping[0]);
        k->ping[0] = ms;
        if (k->ping_n < NC_PING_KEEP) k->ping_n++;
        k->ping_sent = 0;
    } else if (!strcmp(line, "QUIT")) {
        host_quit = 1;
        fprintf(stderr, "recomp: net: the host ends its game\n");
    } else if (!strcmp(line, "KICK")) {
        kicked = 1;
        fprintf(stderr, "recomp: net: the host kicked this computer\n");
    }
}

static int take_one(Sock *k);

/* Takes the control packets out of the stream where the game's reads are between two packets. 0: the next bytes
 * are the game's (or none came yet, or the connection ended: the caller's read sees it), 1: wait, part of a
 * packet came. A part whose rest does not come within NC_CONNECT_MS is dropped: the peer stopped in the middle of
 * it, and the caller's read then sees the end of the connection. */
static int take_control(Sock *k)
{
    for (;;) {
        int part = take_one(k);
        if (part < 0) continue;
        if (part == 3) {               /* no byte waits: the game's read must not run (see take_one) */
            k->part_since = 0;
            return 3;
        }
        if (!part) {
            k->part_since = 0;
            return 0;
        }
        if (!k->part_since) k->part_since = hn_ms();
        if (hn_ms() - k->part_since < NC_CONNECT_MS) return 1;
        k->part_since = 0;
        if (part == 2) return 0;       /* fewer than 5 bytes: they may be the game's, so the game reads them */
        uint8_t drop[NC_CONTROL_MAX + 4];
        long got = hn_recv(k->s, drop, sizeof drop);
        fprintf(stderr, "recomp: net: part of a control packet (%ld bytes) with no rest: dropped\n", got);
        k->part_since = 0;
        return 0;
    }
}

/* One step of take_control: -1 a control packet was taken, 0 the game's bytes or a close, 1 part of one came, 2 fewer
 * than 5 bytes came, which may start either, 3 no byte waits. With 3 the game's recv gives "would block" and does not
 * read: a control packet that came between the look and the read would else go to the game whole, as a packet of
 * opcode 0xbe (slice 9's quit leg, 2026-10-29: the host's game read a PONG). */
static int take_one(Sock *k)
{
    {
        uint8_t p[NC_CONTROL_MAX + 4];
        long got = hn_peek(k->s, p, sizeof p);
        if (got < 0 && hn_error() == HN_WOULDBLOCK) return 3;
        if (got <= 0) return 0;
        if (got < 5) return 2;
        size_t tag = strlen(CONTROL_TAG);
        if (p[4] != NC_CONTROL_OP) return 0;
        long seen = got < 5 + (long)tag ? got - 5 : (long)tag;
        if (seen > 0 && memcmp(p + 5, CONTROL_TAG, (size_t)seen)) return 0;
        if (got < 5 + (long)tag) return 1;
        uint32_t len = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
        if (len < 1 + tag || len > NC_CONTROL_MAX) return 0;
        if (got < (long)len + 4) return 1;
        if (hn_recv(k->s, p, len + 4) != (long)len + 4) return 0;
        char line[NC_CONTROL_MAX];
        snprintf(line, sizeof line, "%.*s", (int)(len - 1 - tag), (const char *)p + 5 + tag);
        control_arrived(k, line);
        return -1;
    }
}

/* The time between two PINGs on k. A joiner through the relay (0.66.255.<channel>, relay.h) gets one every
 * NC_PING_RELAY_MS: each PING and PONG is a WebSocket message, and a message every few seconds keeps the relay
 * awake, which is the free plan's tightest limit (server/README.md, research note section 15). */
static int ping_gap(const Sock *k)
{
    const uint8_t *b = (const uint8_t *)&k->peer;
    int relay = b[0] == 0 && b[1] == 66 && b[2] == 255;
    return relay && ping_every < NC_PING_RELAY_MS ? NC_PING_RELAY_MS : ping_every;
}

/* At every call of the game: the host's PING to each joiner every NC_PING_MS, and the packets that waited. */
static void control_tick(void)
{
    uint64_t now = hn_ms();
    for (int i = 0; i < MAX_SOCKS; i++) {
        Sock *k = &socks[i];
        /* Not after the game read the end (told): a write into a closed connection gets a reset (open item 130). */
        if (!k->handle || !k->control || !k->connected || k->dead || k->kicked || k->told) continue;
        send_waiting(k);
        if (!k->accepted || now < k->ping_at) continue;
        if (k->ping_sent && now - k->ping_sent < NC_PING_LOST_MS) continue;     /* its PONG is still on the way */
        char ping[24];
        snprintf(ping, sizeof ping, "PING %u", ++k->ping_seq);
        k->ping_sent = now;            /* send_waiting sets the time again when the PING goes */
        send_control(k, ping);
        k->ping_at = now + (uint64_t)ping_gap(k);
    }
}

int nc_ping_ms(uint32_t h)
{
    Sock *k = find(h);
    if (!k || !k->control) return -1;
    /* A PONG that came while the game made no call (a day's work) is read now, so the wait does not count. */
    if (!k->dead && !k->kicked && frame_between(&k->fi)) take_control(k);
    if (!k->ping_n) return -1;
    int best = k->ping[0];
    for (int i = 1; i < k->ping_n; i++)
        if (k->ping[i] < best) best = k->ping[i];
    /* A PONG that is late by more than two rounds counts as it is now: the line may have stopped. */
    if (k->ping_sent && hn_ms() - k->ping_sent > 2u * (uint64_t)ping_gap(k) && (int)(hn_ms() - k->ping_sent) > best)
        best = (int)(hn_ms() - k->ping_sent);
    return best;
}

void nc_set_ping_every(int ms) { ping_every = ms; }
int nc_kicked(void) { return kicked; }
int nc_host_broke(void) { return host_broke && !host_quit; }

/* At the program's end on a host: a window close or the script's quit ends the program with no quit packet of the
 * game (net_session.c saves first). Each joiner's runtime gets QUIT, so it says "the host left" and not "the
 * connection was lost". A crash sends nothing. */
static void quit_notice(void)
{
    uint64_t until = hn_ms() + NC_CLOSE_MS;
    for (int i = 0; i < MAX_SOCKS; i++) {
        Sock *k = &socks[i];
        if (!k->handle || !k->control || !k->accepted || !k->connected || k->dead || k->kicked) continue;
        send_control(k, "QUIT");
        while (k->qlen && !k->dead && flush(k) == 0 && k->qlen && hn_ms() < until) hn_wait(k->s, 0, 50);
        hn_set_linger(k->s, 0, 0);     /* the game's SO_LINGER {1, 0} (0x8ae4d4): the exit would reset, not close */
    }
}
int nc_connect_error(void) { return connect_error; }
int nc_bind_error(void) { return bind_error; }
void nc_bind_error_clear(void) { bind_error = 0; }

/* ---- calls ---- */

int nc_start(void)
{
    if (!log_ready) {
        log_ready = 1;
        const char *path = getenv("RECOMP_NET_LOG");
        if (path && *path && !log_file) nc_log_to(path);
        atexit(log_at_exit);
        atexit(quit_notice);
    }
    return hn_start() < 0 ? fail_host() : 0;
}

static Sock *new_sock(hn_sock s, int stream)
{
    for (int i = 0; i < MAX_SOCKS; i++)
        if (!socks[i].handle) {
            Sock *k = &socks[i];
            memset(k, 0, sizeof *k);
            k->handle = next_handle++;
            if (next_handle == NC_INVALID) next_handle = FIRST_HANDLE;
            k->stream = stream;
            k->blocking = 1;
            k->s = s;
            k->out = HN_INVALID;
            k->opened = hn_ms();
            return k;
        }
    return NULL;
}

uint32_t nc_socket(int af, int type)
{
    flush_all(NULL);
    if (af != 2) return fail(HN_AFNOSUPPORT), NC_INVALID;
    if (type != 1 && type != 2) return fail(10044), NC_INVALID;   /* WSAESOCKTNOSUPPORT */
    if (hn_start() < 0) return fail_host(), NC_INVALID;
    hn_sock s = hn_socket(type == 1);
    if (s == HN_INVALID) return fail_host(), NC_INVALID;
    Sock *k = new_sock(s, type == 1);
    if (!k) {
        hn_close(s);
        return fail(10024), NC_INVALID;                           /* WSAEMFILE */
    }
    return k->handle;
}

int nc_close(uint32_t h)
{
    Sock *k = find(h);
    if (!k) return fail(HN_NOTSOCK);
    flush_all(k);
    /* The bytes still queued, for example the quit packet 0x04 of disconnect_network 0x7fc690. */
    uint64_t until = hn_ms() + NC_CLOSE_MS;
    while (k->qlen && !k->dead && flush(k) == 0 && k->qlen && hn_ms() < until)
        hn_wait(k->s, 0, 50);
    if (log_file && k->connected) log_line(k, "close", &k->n);
    if (k->listening) close_pending(h);
    if (k->s != HN_INVALID) hn_close(k->s);
    if (k->out != HN_INVALID) hn_close(k->out);
    free(k->q);
    memset(k, 0, sizeof *k);
    return 0;
}

int nc_nonblocking(uint32_t h, int on)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    k->blocking = !on;
    return 0;
}

int nc_readable(uint32_t h, uint32_t *n)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (k->kicked) return fail(HN_CONNRESET);
    return hn_readable_bytes(k->s, n) < 0 ? fail_host() : 0;
}

static int set_flag(Sock *k, int flag, int on)
{
    if (hn_set_flag(k->s, flag, on) < 0) return fail_host();
    if (k->out != HN_INVALID) hn_set_flag(k->out, flag, on);
    return 0;
}

int nc_setopt(uint32_t h, int level, int name, int value, int seconds)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (level == NC_IPPROTO_TCP && name == NC_TCP_NODELAY) return set_flag(k, HN_NODELAY, value);
    if (level != NC_SOL_SOCKET) return fail(HN_NOPROTOOPT);
    switch (name) {
    case NC_SO_REUSEADDR: return set_flag(k, HN_REUSEADDR, value);
    case NC_SO_KEEPALIVE: return set_flag(k, HN_KEEPALIVE, value);
    case NC_SO_BROADCAST:
        k->broadcast = value != 0;
        return set_flag(k, HN_BROADCAST, value);
    case NC_SO_LINGER:
        /* TCP only: the game's {1, 0} on UDP (0x8aeb9e) means nothing there. */
        if (k->stream && hn_set_linger(k->s, value, seconds) < 0) return fail_host();
        return 0;
    case NC_SO_DEBUG:              /* Linux refuses it to normal users; the game does not check it */
    case NC_SO_SNDBUF:             /* 15,000 bytes (0x8ae54c): the system's own size is larger */
    case NC_SO_RCVBUF:
        return 0;
    default:
        return fail(HN_NOPROTOOPT);
    }
}

int nc_bind(uint32_t h, uint32_t addr, uint16_t port)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (k->stream) {
        if (hn_bind(k->s, addr, port) == 0) return 0;
        bind_error = hn_error();       /* the reason of "could not be created" (0.7.0 slice 7) */
        return fail_host();
    }
    /* UDP: see the head of this file. */
    if (k->out != HN_INVALID) return fail(HN_INVAL);
    hn_set_flag(k->s, HN_REUSEADDR, 1);
    hn_set_flag(k->s, HN_REUSEPORT, 1);
    if (hn_bind(k->s, 0, port) < 0) return fail_host();
    hn_sock out = hn_socket(0);
    if (out == HN_INVALID) return fail_host();
    if (k->broadcast) hn_set_flag(out, HN_BROADCAST, 1);
    if (hn_bind(out, addr, 0) < 0) {
        int e = hn_error();
        hn_close(out);
        return fail(e);
    }
    k->out = out;
    uint32_t ad[32], mask[32];
    int n = hn_adapters(ad, mask, 32);
    for (int i = 0; i < n; i++)
        if (ad[i] == addr) k->net = addr & mask[i], k->mask = mask[i];
    return 0;
}

int nc_listen(uint32_t h, int backlog)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (hn_listen(k->s, backlog) < 0) return fail_host();
    k->listening = 1;
    return 0;
}

static void connected(Sock *k, uint32_t addr, uint16_t port)
{
    k->connected = 1;
    k->peer = addr, k->peer_port = port;
    k->opened = hn_ms();
    hn_set_flag(k->s, HN_NODELAY, 1);
    /* The game sets SO_KEEPALIVE (0x8ae50c, 0x8ae79e), whose system timers find a dead peer after about two
     * hours. These find it in under a minute (0.7.0 slice 6). */
    hn_set_keepalive(k->s, NC_KEEP_IDLE_S, NC_KEEP_INTERVAL_S, NC_KEEP_COUNT);
    if (log_file) log_line(k, "open", &k->n);
}

static int is_loopback(uint32_t a) { return ((const uint8_t *)&a)[0] == 127; }

/* Connections that wait for their join block (see the head of this file). */
#define PENDING 16
static struct {
    uint32_t listen;               /* the game's listen handle, 0: free */
    hn_sock s;
    uint32_t addr;
    uint16_t port;
    uint64_t since;
    int len, ready;                /* ready: it passed, and waits for the game's accept */
    int hello;                     /* it passed with a join block: control packets work (0.7.0 slice 7) */
    char buf[256];
} pending[PENDING];

static int check_pending(int i);

/* Takes each new connection of the game's listen sockets and reads the join blocks, at every call of the game,
 * so a joiner gets its answer while the host's game does not accept (a full game, or a game between two polls
 * of 0x77af5a). A connection from 127.x passes with no block. */
static void serve_listeners(void)
{
    for (int k = 0; identity && k < MAX_SOCKS; k++) {
        if (!socks[k].handle || !socks[k].listening) continue;
        for (;;) {
            int i = 0;
            while (i < PENDING && pending[i].listen) i++;
            if (i == PENDING) break;
            uint32_t a;
            uint16_t p;
            hn_sock c = hn_accept(socks[k].s, &a, &p);
            if (c == HN_INVALID) break;
            pending[i].listen = socks[k].handle, pending[i].s = c, pending[i].addr = a, pending[i].port = p;
            pending[i].since = hn_ms(), pending[i].len = 0, pending[i].ready = is_loopback(a);
            pending[i].hello = 0;
        }
        /* A human manager through the relay (relay.h): its join block comes through the relay like any bytes. */
        for (int i = 0; hooks && hooks->incoming && i < PENDING; i++) {
            if (pending[i].listen) continue;
            uint32_t a;
            uint16_t p;
            hn_sock c = hooks->incoming(&a, &p);
            if (c == HN_INVALID) break;
            pending[i].listen = socks[k].handle, pending[i].s = c, pending[i].addr = a, pending[i].port = p;
            pending[i].since = hn_ms(), pending[i].len = 0, pending[i].ready = 0;
        }
    }
    for (int i = 0; i < PENDING; i++)
        if (pending[i].listen && !pending[i].ready && check_pending(i) > 0) pending[i].ready = 1;
}

/* The game closed a listen socket: its waiting connections go too. */
static void close_pending(uint32_t h)
{
    for (int i = 0; i < PENDING; i++)
        if (pending[i].listen == h) {
            hn_close(pending[i].s);
            pending[i].listen = 0;
        }
}

static void send_line(hn_sock s, const char *line)
{
    size_t n = strlen(line);
    uint64_t until = hn_ms() + 500;
    while (n) {
        long r = hn_send(s, line, n);
        if (r > 0) {
            line += r, n -= (size_t)r;
            continue;
        }
        if ((r < 0 && hn_error() != HN_WOULDBLOCK) || hn_ms() >= until || hn_wait(s, 0, 50) < 0) return;
    }
}

/* The value of "key=" in a join block, cut to room - 1 bytes. */
static void block_value(const char *b, const char *key, char *out, size_t room)
{
    out[0] = 0;
    size_t k = strlen(key);
    for (const char *line = b; *line; line += strcspn(line, "\n") + (line[strcspn(line, "\n")] ? 1 : 0))
        if (!strncmp(line, key, k) && line[k] == '=') {
            size_t n = strcspn(line + k + 1, "\n");
            if (n >= room) n = room - 1;
            memcpy(out, line + k + 1, n);
            out[n] = 0;
            return;
        }
}

/* Reads the block of pending connection i. 1: it passed (the connection goes to the game), 0: not yet,
 * -1: it failed and is closed. */
static int check_pending(int i)
{
    for (;;) {
        if (pending[i].len >= (int)sizeof pending[i].buf - 1) break;
        long r = hn_recv(pending[i].s, pending[i].buf + pending[i].len, 1);
        if (r < 0 && hn_error() == HN_WOULDBLOCK) {
            if (hn_ms() - pending[i].since < NC_HELLO_MS) return 0;
            break;
        }
        if (r <= 0) break;
        pending[i].buf[++pending[i].len] = 0;
        if (pending[i].len < 2 || memcmp(pending[i].buf + pending[i].len - 2, "\n\n", 2)) continue;
        char version[32], password[72];
        block_value(pending[i].buf, "version", version, sizeof version);
        block_value(pending[i].buf, "password", password, sizeof password);
        char line[64];
        const char *why = NULL;
        if (strncmp(pending[i].buf, "BOSMAN HELLO 1\n", 15)) why = "not a join block";
        else if (strcmp(version, my_version)) {
            snprintf(line, sizeof line, "BOSMAN VERSION %s\n", my_version);
            why = "another version";
        } else if (host_password[0] && strcmp(password, host_password)) {
            snprintf(line, sizeof line, "BOSMAN PASSWORD\n");
            why = "a wrong password";
        }
        if (why) {
            if (strcmp(why, "not a join block")) send_line(pending[i].s, line);
            fprintf(stderr, "recomp: net: a joiner was refused: %s (version %s)\n", why, version);
            break;
        }
        send_line(pending[i].s, "BOSMAN OK\n");
        pending[i].hello = 1;
        return 1;
    }
    hn_close(pending[i].s);
    pending[i].listen = 0;
    return -1;
}

uint32_t nc_accept(uint32_t h, uint32_t *addr, uint16_t *port)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK), NC_INVALID;
    uint32_t a = 0;
    uint16_t p = 0;
    hn_sock s = HN_INVALID;
    int hello = 0;
    if (identity) {                /* flush_all above served the join blocks: take one that passed */
        for (int i = 0; s == HN_INVALID && i < PENDING; i++)
            if (pending[i].listen == h && pending[i].ready) {
                s = pending[i].s, a = pending[i].addr, p = pending[i].port, hello = pending[i].hello;
                pending[i].listen = 0;
            }
        if (s == HN_INVALID) return fail(HN_WOULDBLOCK), NC_INVALID;
    } else {
        s = hn_accept(k->s, &a, &p);
        if (s == HN_INVALID) return fail_host(), NC_INVALID;
    }
    Sock *c = new_sock(s, 1);
    if (!c) {
        hn_close(s);
        return fail(10024), NC_INVALID;
    }
    /* Windows gives an accepted socket the mode of the listen socket. The game's listen sockets are
     * non-blocking (0x8ae48d), and it never sets the accepted one (0x8ae840). */
    c->blocking = k->blocking;
    c->accepted = 1;
    c->control = hello;
    connected(c, a, p);
    if (addr) *addr = a;
    if (port) *port = p;
    return c->handle;
}

/* The joiner's side of the join check on a connected socket. 0 when the host said OK. */
static int hello(Sock *k)
{
    char block[160], line[64];
    snprintf(block, sizeof block, "BOSMAN HELLO 1\nversion=%s\npassword=%s\n\n", my_version, join_password);
    send_line(k->s, block);
    uint64_t until = hn_ms() + NC_HELLO_WAIT_MS;
    int n = 0;
    while (n < (int)sizeof line - 1) {
        long r = hn_recv(k->s, line + n, 1);                      /* one byte at a time: the game's bytes follow */
        if (r == 1) {
            if (line[n++] == '\n') break;
            continue;
        }
        if (r == 0 || hn_error() != HN_WOULDBLOCK || hn_ms() >= until || hn_wait(k->s, 1, 50) < 0) break;
    }
    line[n] = 0;
    if (!strcmp(line, "BOSMAN OK\n")) return 0;
    if (!strncmp(line, "BOSMAN VERSION ", 15)) {
        refused = NC_REFUSED_VERSION;
        snprintf(refused_version, sizeof refused_version, "%.*s", (int)strcspn(line + 15, "\n"), line + 15);
    } else if (!strcmp(line, "BOSMAN PASSWORD\n")) refused = NC_REFUSED_PASSWORD;
    else refused = n ? NC_REFUSED_NOT_BOSMAN : NC_REFUSED_NO_ANSWER;
    fprintf(stderr, "recomp: net: the host refused the join: %s\n", n ? line : "no answer\n");
    return -1;
}

int nc_connect(uint32_t h, uint32_t addr, uint16_t port)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (!k->stream) return fail(10045);                          /* WSAEOPNOTSUPP: the game never does */
    refused = NC_REFUSED_NONE;
    refused_version[0] = 0;
    host_left = host_broke = host_quit = kicked = connect_error = 0;
    if (hooks && hooks->connect && hooks->connect(addr, port) < 0) return -1;
    hn_sock routed = HN_INVALID;
    int how = hooks && hooks->route ? hooks->route(addr, port, &routed) : 0;
    if (how < 0) return -1;
    if (how > 0) {                 /* the relay's socket takes the place of the game's (relay.h) */
        hn_close(k->s);
        k->s = routed;
    } else if (hn_connect(k->s, addr, port, connect_ms) < 0) {
        connect_error = hn_error();    /* the reason of "could not be joined" (0.7.0 slice 7) */
        return fail_host();
    }
    if (identity && !is_loopback(addr) && hello(k) < 0) {
        /* The game keeps its handle and closes it after the failed join: give it a fresh socket. */
        hn_close(k->s);
        k->s = hn_socket(1);
        return fail(10061);                                       /* WSAECONNREFUSED */
    }
    k->control = identity && !is_loopback(addr);
    connected(k, addr, port);
    return 0;
}

/* A send into a connection whose peer is gone, before a recv of the game reported it. The original handles a close
 * at its recv: Network::receive_packet 0x77af20 frees the slot and runs the leave path (0x77b4e9..0x77b508). A send
 * that fails makes send_packet 0x77ac30 return 0 (0x77ad5f..0x77ad9d), and ScreenManager::send_clear_click 0x800000
 * then shows "scrman.cpp 2480" (0x8000e2..0x80013a), which stops the host: slice 9's four-player run, 2026-10-29,
 * where two joiners quit while the host worked out a day and then opened their news screens. WinSock takes the first
 * send after the peer closed and fails a send after the peer's reset came back (WSAECONNRESET), so the original can
 * show that box too when it sends two packets to a gone joiner with no recv between them. The runtime takes such a
 * send and drops its bytes until a recv of the game has reported the close, so the leave path runs in every case;
 * after that recv, a send fails as before. */
static long send_to_gone(Sock *k, uint32_t n)
{
    if (k->told) return fail(k->dead ? k->dead : HN_CONNRESET);
    k->n.sends++;
    return (long)n;
}

long nc_send(uint32_t h, const void *buf, uint32_t n)
{
    Sock *k = find(h);
    flush_all(k);
    if (!k) return fail(HN_NOTSOCK);
    if (!k->stream) return nc_sendto(h, buf, n, k->peer, k->peer_port);
    if (!k->connected) return fail(HN_NOTCONN);
    if (k->dead) return send_to_gone(k, n);
    if (enqueue(k, buf, n) < 0) return send_to_gone(k, n);
    k->n.sends++;
    k->n.bytes_out += n;
    k->n.packets_out += frame_feed(&k->fo, buf, n);
    /* The packet trace (RECOMP_NET_TRACE=1, net_session.c): each click packet (opcode 6) that a joiner sends. */
    static int trace = -1;
    if (trace < 0) trace = getenv("RECOMP_NET_TRACE") != NULL;
    if (trace && frame_between(&k->fo) && n > 1 && ((const uint8_t *)buf)[0] == 6 && !k->accepted) {
        char hex[40];
        int at = 0;
        for (uint32_t i = 0; i < n && i < 10; i++) at += snprintf(hex + at, sizeof hex - (size_t)at, " %02x",
                                                                   ((const uint8_t *)buf)[i]);
        fprintf(stderr, "recomp: trace: %llu click packet sent:%s\n", (unsigned long long)hn_ms(), hex);
    }
    /* Hold the length bytes until the body follows. */
    if (frame_between(&k->fo) && flush(k) < 0) return send_to_gone(k, n);
    send_waiting(k);
    k->n.queued = k->qlen - k->qoff;
    return (long)n;
}

static uint64_t resets_in;   /* opcode 0 packets that the game read from all TCP connections (nc_resets_in) */

/* A joiner's connection to its host, not the loopback test's. */
static int to_host(const Sock *k) { return k->stream && k->connected && !k->accepted && !is_loopback(k->peer); }

/* The text of the confirm of Exit Game in a network game (0xa5a820, pushed at 0x764392 for player 0..15 when the
 * player is not 0, research note 2026-10-14-start-screen-helpers.md section 8). The host sends it in the screen
 * of the joiner's own Exit Game. */
static const char exit_text[] = "Exit the network game";

static void watch_exit(Sock *k, const uint8_t *p, uint32_t n)
{
    size_t keep = sizeof exit_text - 2, have = strlen(k->tail);
    char b[sizeof k->tail + 4096];
    while (n) {
        uint32_t take = n < 4096 ? n : 4096;
        memcpy(b, k->tail, have);
        memcpy(b + have, p, take);
        size_t len = have + take;
        for (size_t i = 0; i + sizeof exit_text - 1 <= len; i++)
            if (!memcmp(b + i, exit_text, sizeof exit_text - 1)) k->exit_seen = 1;
        have = len < keep ? len : keep;
        memcpy(k->tail, b + len - have, have);
        k->tail[have] = 0;
        for (size_t i = 0; i < have; i++)
            if (!k->tail[i]) k->tail[i] = 1;    /* a zero byte in the data must not end the kept text */
        p += take, n -= take;
    }
}

/* The quit packet 0x04 of disconnect_network 0x7fc690 (0x7fc738..0x7fc766), with its length. */
static const uint8_t quit_packet[5] = {1, 0, 0, 0, 4};

long nc_recv(uint32_t h, void *buf, uint32_t n)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (k->kicked) return k->told = 1, fail(HN_CONNRESET);
    if (k->quit_at >= 1 && k->quit_at <= (int)sizeof quit_packet) {
        uint32_t take = (uint32_t)sizeof quit_packet - (uint32_t)(k->quit_at - 1);
        if (take > n) take = n;
        memcpy(buf, quit_packet + k->quit_at - 1, take);
        k->quit_at += (int)take;
        frame_feed(&k->fi, buf, take);
        return (long)take;
    }
    if (k->quit_at) return k->told = 1, fail(k->dead ? k->dead : HN_CONNRESET);
    /* The runtime's own packets come out before the game reads the next length byte (0.7.0 slice 7). */
    if (k->control && n && frame_between(&k->fi)) {
        uint64_t until = hn_ms() + NC_CONNECT_MS;
        while (take_control(k)) {
            if (!k->blocking || hn_ms() >= until) return fail(HN_WOULDBLOCK);
            hn_sleep(5);             /* hn_wait returns at once while part of a packet waits */
        }
    }
    /* The host's screen loop takes one human manager's packet at a time (nc_hold_others). */
    if (hold_on && k->accepted && k->handle != hold_handle && frame_between(&k->fi)) return fail(HN_WOULDBLOCK);
    /* Bytes that arrived before a send broke the connection still count, for example the host's quit
     * packet 0x04 (0x7fc738). */
    long r = hn_recv(k->s, buf, n);
    if (r < 0 && k->blocking && hn_error() == HN_WOULDBLOCK && !k->dead && hn_wait(k->s, 1, NC_CONNECT_MS) > 0)
        r = hn_recv(k->s, buf, n);
    int broke = r == 0 || (r < 0 && (hn_error() != HN_WOULDBLOCK || k->dead));
    if (broke && to_host(k) && frame_between(&k->fi) && n && !k->fi.op_packets[4] && !k->exit_seen) {
        /* The host quit with no quit packet: it crashed, or its line dropped. The joiner's loop 0x7fdc00 would
         * see it only at its next click, through "Unable to send data" (0x569456). It gets the host's own quit
         * packet in place of the close, and ends as at a quit (0x7fdeba). */
        if (!k->dead) k->dead = r == 0 ? HN_CONNRESET : hn_error();
        fprintf(stderr, "recomp: net: the connection to the host broke (%d): the joiner gets the quit packet\n", k->dead);
        host_left = 1;
        host_broke = 1;
        k->quit_at = 1;
        return nc_recv(h, buf, n);
    }
    if (r < 0 && k->dead && hn_error() == HN_WOULDBLOCK) return k->told = 1, fail(k->dead);
    if (r < 0) {
        if (hn_error() != HN_WOULDBLOCK) k->told = 1;
        return fail_host();
    }
    if (r == 0 && n) k->told = 1;   /* the peer closed */
    k->n.bytes_in += (uint64_t)r;
    uint64_t resets = k->fi.op_packets[0], quits = k->fi.op_packets[4];
    k->n.packets_in += frame_feed(&k->fi, buf, (uint32_t)r);
    resets_in += k->fi.op_packets[0] - resets;
    if (to_host(k)) {
        if (k->fi.op_packets[0] != resets) k->exit_seen = 0, k->tail[0] = 0;   /* a new host screen */
        watch_exit(k, buf, (uint32_t)r);
        /* A quit packet ends the joiner's loop. The host sends it at its own quit (0x7fc738), and also when the
         * joiner says Yes to its own Exit Game: the menu bar input 0x7615d0 returns -12 for a remote channel
         * (0x762af8..0x762b04), and the screen loop sends 0x04 to that joiner (0x7fcd79..0x7fce2e). */
        if (k->fi.op_packets[4] != quits) host_left = !k->exit_seen;
    }
    return r;
}

void nc_hold_others(uint32_t handle)
{
    hold_on = handle != NC_INVALID;
    hold_handle = handle;
}

int nc_host_left(void) { return host_left; }
void nc_host_left_clear(void) { host_left = 0; }

int nc_peer_list(uint32_t *handle, uint32_t *addr, uint16_t *port, int max)
{
    int n = 0;
    for (int i = 0; i < MAX_SOCKS && n < max; i++) {
        const Sock *k = &socks[i];
        if (!k->handle || !k->accepted || !k->connected || k->dead || k->kicked) continue;
        if (handle) handle[n] = k->handle;
        if (addr) addr[n] = k->peer;
        if (port) port[n] = k->peer_port;
        n++;
    }
    return n;
}

int nc_kick(uint32_t h)
{
    Sock *k = find(h);
    if (!k || !k->accepted || !k->connected || k->kicked) return -1;
    char ip[24];
    ip_text(ip, sizeof ip, k->peer);
    fprintf(stderr, "recomp: net: the host kicks the human manager at %s:%u\n", ip, k->peer_port);
    /* The joiner's runtime learns why its connection ends (0.7.0 slice 7): a KICK control packet, sent before the
     * close when the game's stream is between two packets. */
    if (k->control && !k->dead && frame_between(&k->fo)) {
        uint8_t p[NC_CONTROL_MAX + 4];
        int len = control_packet(p, "KICK");
        if (len && enqueue(k, p, (uint32_t)len) == 0) {
            uint64_t until = hn_ms() + NC_CLOSE_MS;
            while (k->qlen && !k->dead && flush(k) == 0 && k->qlen && hn_ms() < until) hn_wait(k->s, 0, 50);
        }
        /* A close with unread bytes from the joiner sends a reset, and the joiner's system may then drop the KICK
         * before its game reads it. So: end the send side, and read until the joiner closes, for a short time. */
        if (!k->qlen && hn_shutdown_send(k->s) == 0) {
            uint8_t sink[512];
            uint64_t until = hn_ms() + 2 * NC_CLOSE_MS;
            while (hn_ms() < until) {
                long r = hn_recv(k->s, sink, sizeof sink);
                if (r == 0 || (r < 0 && hn_error() != HN_WOULDBLOCK)) break;
                if (r < 0) hn_wait(k->s, 1, 50);
            }
        }
    }
    if (log_file) log_line(k, "close", &k->n);
    /* The game sets SO_LINGER {0, 0} (0x8ae4d4), so a close would send a reset, and the joiner's system would drop the
     * KICK packet that its game has not read. A plain close sends it first (0.7.0 slice 6). */
    hn_set_linger(k->s, 0, 0);
    /* The game's next recv or send on this handle fails, and Network::receive_packet 0x77af20 frees the slot and
     * calls the disconnect callback (0x77b4e9..0x77b508): the original's leave path puts the club on holiday. */
    hn_close(k->s);
    k->s = HN_INVALID;
    k->kicked = 1;
    k->dead = HN_CONNRESET;
    k->qlen = k->qoff = 0;
    return 0;
}

uint64_t nc_resets_in(void) { return resets_in; }

long nc_sendto(uint32_t h, const void *buf, uint32_t n, uint32_t addr, uint16_t port)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (k->stream) return nc_send(h, buf, n);
    long r = hn_sendto(k->out != HN_INVALID ? k->out : k->s, buf, n, addr, port);
    if (r < 0) return fail_host();
    k->n.sends++, k->n.bytes_out += (uint64_t)r, k->n.packets_out++;
    if (hooks && hooks->sent) hooks->sent(buf, n, addr, port);
    return r;
}

long nc_recvfrom(uint32_t h, void *buf, uint32_t n, uint32_t *addr, uint16_t *port)
{
    Sock *k = find(h);
    flush_all(NULL);
    if (!k) return fail(HN_NOTSOCK);
    if (k->stream) {
        long r = nc_recv(h, buf, n);
        if (r >= 0 && addr) *addr = k->peer;
        if (r >= 0 && port) *port = k->peer_port;
        return r;
    }
    long r = -1;
    uint32_t from = 0, a = 0;
    uint16_t p = 0;
    int dropped = 0;
    for (int tries = 0; r < 0 && tries < 64; tries++) {
        dropped = 0;               /* 1 only when the last try read a datagram that the hook dropped */
        r = k->out != HN_INVALID ? hn_recvfrom(k->out, buf, n, &a, &p) : -1;
        /* The shared socket: drop the datagrams from other adapters' subnets (see the head of this file). */
        for (int i = 0; r < 0 && i < 64; i++) {
            r = hn_recvfrom(k->s, buf, n, &from, &p);
            if (r >= 0 && k->mask && (from & k->mask) != k->net) r = -1;
            else if (r < 0) break;
            else a = from;
        }
        if (r < 0) break;
        /* The game list (lobby_rows.c) may drop a datagram: then read the next one. */
        dropped = hooks && hooks->arrived && !hooks->arrived(buf, r, a);
        if (dropped) r = -1;
    }
    if (r < 0 && dropped) nc_set_error(HN_WOULDBLOCK);    /* 64 dropped datagrams: the next call reads on */
    if (r < 0 && hooks && hooks->extra) {
        r = hooks->extra(buf, n, &a, &p);
        if (r >= 0) {
            if (addr) *addr = a;
            if (port) *port = p;
            k->n.packets_in++;
            return r;
        }
        return dropped ? -1 : fail_host();   /* the last real read's error: would block, or a real one */
    }
    if (r < 0) return dropped ? -1 : fail_host();
    if (addr) *addr = a;
    if (port) *port = p;
    k->n.bytes_in += (uint64_t)r, k->n.packets_in++;
    return r;
}

int nc_adapters(uint32_t *addr, uint32_t *broadcast, int max)
{
    uint32_t mask[32];
    if (max > 32) max = 32;
    int n = hn_adapters(addr, mask, max);
    for (int i = 0; i < n; i++) broadcast[i] = addr[i] | ~mask[i];   /* as 0x8af24c..0x8af267 */
    return n;
}

int nc_counts(uint32_t h, NcCounts *c)
{
    Sock *k = find(h);
    if (!k) return 0;
    *c = k->n;
    c->queued = k->qlen - k->qoff;
    return 1;
}
