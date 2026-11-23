/* Joins through the relay (relay.h). */
#include "relay.h"
#include "lobby_http.h"
#include "lobby_rows.h"
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAIT_MS 5                  /* the thread's step while it waits for bytes */
#define SEND_MS 5000               /* what one frame may take to leave */
#define RETRY_FIRST_MS 2000
#define RETRY_MAX_MS 30000
#define CLOSE_FLUSH_MS 1000        /* what an ended link waits to give the game its last bytes */
#define LINGER_MS 10000            /* what a joiner's ended stream waits for its game to close (see "Ends") */

/* A byte queue. */
typedef struct {
    uint8_t *p;
    size_t len, off, cap;
} Queue;

static int put(Queue *q, const void *b, size_t n)
{
    if (q->off == q->len) q->off = q->len = 0;
    if (q->len + n > q->cap) {
        if (q->off) {
            memmove(q->p, q->p + q->off, q->len - q->off);
            q->len -= q->off, q->off = 0;
        }
        size_t cap = q->cap ? q->cap : 4096;
        while (cap < q->len + n) cap *= 2;
        if (cap > q->cap) {
            uint8_t *bigger = realloc(q->p, cap);
            if (!bigger) return -1;
            q->p = bigger, q->cap = cap;
        }
    }
    memcpy(q->p + q->len, b, n);
    q->len += n;
    return 0;
}

static void drop(Queue *q)
{
    free(q->p);
    memset(q, 0, sizeof *q);
}

/* Writes what the queue holds to s, as far as s takes it. -1 when s is broken. */
static int give(Queue *q, hn_sock s)
{
    while (q->off < q->len) {
        long r = hn_send(s, q->p + q->off, q->len - q->off);
        if (r > 0) {
            q->off += (size_t)r;
            continue;
        }
        return r < 0 && hn_error() == HN_WOULDBLOCK ? 0 : -1;
    }
    q->off = q->len = 0;
    return 0;
}

/* Reads what s has into the batch q, at most up to RELAY_BATCH_MAX. 1: bytes came, 0: none, -1: s ended. */
static int take(Queue *q, hn_sock s)
{
    uint8_t buf[16384];
    int got = 0;
    while (q->len - q->off < RELAY_BATCH_MAX) {
        long r = hn_recv(s, buf, sizeof buf);
        if (r > 0) {
            if (put(q, buf, (size_t)r) < 0) return -1;
            got = 1;
            continue;
        }
        if (r < 0 && hn_error() == HN_WOULDBLOCK) break;
        return -1;
    }
    return got;
}

/* ---- Ends (0.6.2 open item 130) ----
 * The game's end of a stream is a loopback TCP connection (hn_pair). The game can still write when the relay ends
 * the stream: the host's PING of net_core.c goes at the game's next call (control_tick), a joiner's PONG at its
 * next read. A write into a socket that is already closed gets a reset. The game then reads "connection reset"
 * (10054) in place of the end and loses the bytes that it has not read yet: a joiner saw "the connection was
 * lost" in place of the host's QUIT (test 4 of the online spec, 2026-10-29). So the relay ends
 * its send side only (the game reads the end after the last bytes), reads and drops what the game still writes,
 * and closes when the game has closed its end (a joiner's stream: or after LINGER_MS, as its game writes nothing
 * after it read the end, net_core.c's quit packet). */

/* 1 when s is done: the game closed its end, or s broke. */
static int drained(hn_sock s)
{
    uint8_t buf[4096];
    for (int i = 0; i < 1024; i++) {
        long r = hn_recv(s, buf, sizeof buf);
        if (r > 0) continue;
        return r == 0 || hn_error() != HN_WOULDBLOCK;
    }
    return 0;
}

/* The host thread's ended streams that wait for the game. The host's game can go a long time without a read of
 * one human manager's stream (nc_hold_others in net_core.c, while it serves another one's screen), and its PINGs
 * go on, so these wait with no time limit until the game closes its end. Two slots for each channel; with no free
 * slot the oldest closes. */
#define LINGER_SLOTS (2 * RELAY_CHANNELS)
static struct {
    hn_sock s;
    uint64_t since;                /* 0: a free slot */
} lingering[LINGER_SLOTS];

static void tend_lingering(void)
{
    for (int i = 0; i < LINGER_SLOTS; i++)
        if (lingering[i].since && drained(lingering[i].s)) {
            hn_close(lingering[i].s);
            lingering[i].since = 0;
        }
}

/* Ends a stream of the host thread: the end goes to the game, and the socket waits in lingering. */
static void end_stream(hn_sock s)
{
    if (hn_shutdown_send(s) < 0) {
        hn_close(s);               /* the stream broke already: no end to give */
        return;
    }
    int at = 0;
    for (int i = 0; i < LINGER_SLOTS; i++) {
        if (!lingering[i].since) {
            at = i;
            break;
        }
        if (lingering[i].since < lingering[at].since) at = i;
    }
    if (lingering[at].since) hn_close(lingering[at].s);
    lingering[at].s = s, lingering[at].since = hn_ms();
}

int relay_url(char *out, size_t room, const char *lobby, const char *code, const char *query)
{
    char host[256];
    unsigned port;
    int tls;
    if (!lobby || !lobby_http_parse(lobby, host, sizeof host, &port, &tls)) return 0;
    char port_text[8] = "";
    if (port != (tls ? 443u : 80u)) snprintf(port_text, sizeof port_text, ":%u", port);
    int n = snprintf(out, room, "%s://%s%s/v1/games/%s/relay?%s", tls ? "wss" : "ws", host, port_text, code, query);
    return n > 0 && (size_t)n < room;
}

/* The integer of "ch" in a control message, or -1. */
static int channel_of(const char *t)
{
    const char *p = strstr(t, "\"ch\":");
    if (!p) return -1;
    int ch = atoi(p + 5);
    return ch >= 0 && ch < RELAY_CHANNELS ? ch : -1;
}

/* ---- host ---- */

typedef struct {
    int open;
    hn_sock mine;                  /* the relay's end; the game has the other */
    Queue batch, to_game;
    uint64_t first;                /* when the batch's first byte came */
    int ended;                     /* the game closed its end: kick once the batch is sent */
    unsigned frames_out, frames_in;   /* for the log: the free plan bills messages */
    uint64_t bytes_out, bytes_in;
    uint64_t opened, last_frame;
} Channel;

/* Shared with the host thread, under hn_lock. */
static struct {
    int started, active, gen, replaced;   /* replaced: 4001 for this code and token */
    int idle_gen;                  /* the gen that the thread last saw with no connection and nothing wanted */
    char lobby[256], code[9], token[96];
    struct {
        hn_sock s;
        int ch;
    } incoming[RELAY_CHANNELS];
    int incoming_n;
} host;

static Channel host_channels[RELAY_CHANNELS];   /* the host thread's only */

static void close_channel(Channel *c)
{
    if (!c->open) return;
    fprintf(stderr, "recomp: relay: channel %d closed after %llu s: sent %u frames, %llu bytes; got %u frames, %llu "
            "bytes\n", (int)(c - host_channels), (unsigned long long)((hn_ms() - c->opened) / 1000), c->frames_out,
            (unsigned long long)c->bytes_out, c->frames_in, (unsigned long long)c->bytes_in);
    give(&c->to_game, c->mine);
    end_stream(c->mine);
    drop(&c->batch), drop(&c->to_game);
    memset(c, 0, sizeof *c);
}

static void close_all(Channel *ch)
{
    for (int i = 0; i < RELAY_CHANNELS; i++) close_channel(&ch[i]);
    hn_lock();
    for (int i = 0; i < host.incoming_n; i++) hn_close(host.incoming[i].s);   /* the game did not take them */
    host.incoming_n = 0;
    hn_unlock();
}

/* The game ends of channel n that the game has not taken yet. Under hn_lock. */
static void forget_incoming(int n)
{
    for (int i = 0; i < host.incoming_n;)
        if (host.incoming[i].ch == n) {
            hn_close(host.incoming[i].s);
            memmove(host.incoming + i, host.incoming + i + 1, (size_t)(--host.incoming_n - i) * sizeof host.incoming[0]);
        } else i++;
}

/* 0 when the game gets the new channel's connection, -1 when the channel must be kicked. */
static int open_channel(Channel *ch, int n)
{
    close_channel(&ch[n]);
    hn_sock game, mine;
    if (hn_pair(&game, &mine) < 0) {
        fprintf(stderr, "recomp: relay: no socket pair for channel %d\n", n);
        return -1;
    }
    hn_lock();
    forget_incoming(n);
    if (host.incoming_n < RELAY_CHANNELS) {
        host.incoming[host.incoming_n].s = game, host.incoming[host.incoming_n].ch = n;
        host.incoming_n++;
        game = HN_INVALID;
    }
    hn_unlock();
    if (game != HN_INVALID) {
        hn_close(game), hn_close(mine);
        return -1;
    }
    ch[n].open = 1, ch[n].mine = mine, ch[n].opened = hn_ms();
    fprintf(stderr, "recomp: relay: a human manager connects on channel %d\n", n);
    return 0;
}

static int kick(WsConn *w, int n)
{
    char m[32];
    snprintf(m, sizeof m, "{\"t\":\"kick\",\"ch\":%d}", n);
    return ws_send(w, 1, m, strlen(m), SEND_MS);
}

/* Sends every channel's batch. -1 when the relay connection broke. */
static int send_batches(WsConn *w, Channel *ch, uint64_t *last_sent, int force)
{
    static uint8_t frame[RELAY_BATCH_MAX + 16384 + 1];
    for (int i = 0; i < RELAY_CHANNELS; i++) {
        Channel *c = &ch[i];
        size_t n = c->open ? c->batch.len - c->batch.off : 0;
        /* At most one frame each RELAY_BATCH_MS for each channel, unless it ends or its batch is full. */
        if (n && !force && !c->ended && n < RELAY_BATCH_MAX && hn_ms() - c->last_frame < RELAY_BATCH_MS) n = 0;
        if (n) {
            frame[0] = (uint8_t)i;
            memcpy(frame + 1, c->batch.p + c->batch.off, n);
            c->batch.off = c->batch.len = 0;
            c->frames_out++, c->bytes_out += n, c->last_frame = hn_ms();
            if (ws_send(w, 0, frame, n + 1, SEND_MS) < 0) return -1;
            *last_sent = hn_ms();
        }
        if (c->open && c->ended) {
            if (kick(w, i) < 0) return -1;
            fprintf(stderr, "recomp: relay: the host closed channel %d\n", i);
            close_channel(c);
        }
    }
    return 0;
}

static void host_thread(void *unused)
{
    (void)unused;
    Channel *ch = host_channels;
    WsConn *w = NULL;
    int gen = -1, backoff = RETRY_FIRST_MS;
    int gen_seen_before = 0;       /* this code and token had a connection before: a reconnect */
    uint64_t retry_at = 0, last_sent = 0, connected_at = 0;
    char lobby[256], code[9], token[96], url[512], query[160];
    for (;;) {
        tend_lingering();
        hn_lock();
        int active = host.active && !host.replaced, want = host.gen;
        memcpy(lobby, host.lobby, sizeof lobby), memcpy(code, host.code, sizeof code);
        memcpy(token, host.token, sizeof token);
        hn_unlock();
        uint64_t now = hn_ms();
        if (w && (!active || want != gen)) {
            /* The host stops (its game ends, relay_host_finish): the last bytes of each channel go first, for
             * example the QUIT of net_core.c, which the game wrote just before. */
            if (!active) {
                for (int i = 0; i < RELAY_CHANNELS; i++)
                    if (ch[i].open && !ch[i].ended) take(&ch[i].batch, ch[i].mine);
                send_batches(w, ch, &last_sent, 1);
            }
            close_all(ch);
            ws_close(w, 1000);
            w = NULL;
            fprintf(stderr, "recomp: relay: the host's connection closed\n");
        }
        if (!active) {
            hn_lock();
            host.idle_gen = want;
            hn_unlock();
            gen = -1, retry_at = 0, backoff = RETRY_FIRST_MS;
            hn_sleep(50);
            continue;
        }
        if (!w && (want != gen || now >= retry_at)) {
            if (want != gen) backoff = RETRY_FIRST_MS, gen_seen_before = 0;
            gen = want;
            snprintf(query, sizeof query, "role=host&token=%s", token);
            int status = 0;
            char body[256];
            w = relay_url(url, sizeof url, lobby, code, query) ? ws_open(url, 10000, &status, body, sizeof body) : NULL;
            if (w) {
                fprintf(stderr, "recomp: relay: the host is connected as %s\n", code);
                backoff = RETRY_FIRST_MS, last_sent = connected_at = hn_ms();
            } else {
                char why[32];
                lobby_json_word(body, "error", why, sizeof why);
                fprintf(stderr, "recomp: relay: the host cannot connect (%d %s), next try in %d s\n", status, why,
                        backoff / 1000);
                retry_at = hn_ms() + (uint64_t)backoff;
                backoff = backoff * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : backoff * 2;
            }
        }
        if (!w) {
            hn_sleep(50);
            continue;
        }

        ws_wait(w, WAIT_MS);
        WsMessage m;
        int r;
        while ((r = ws_poll(w, &m)) == 1) {
            if (m.text) {
                char t[64];
                size_t n = m.len < sizeof t - 1 ? m.len : sizeof t - 1;
                memcpy(t, m.data, n);
                t[n] = 0;
                int c = channel_of(t);
                /* The relay names the channels still open when a host connects (relay.ts): after a reconnect
                 * their streams broke with the old connection, so the game could not read them on. */
                int stale = hn_ms() - connected_at < RELAY_FRESH_MS && gen_seen_before;
                if (strstr(t, "\"open\"") && c >= 0 && (stale || open_channel(ch, c) < 0)) {
                    fprintf(stderr, "recomp: relay: channel %d is kicked: %s\n", c, stale ? "an old stream" : "no room");
                    if (kick(w, c) < 0) r = -1;
                    break;
                }
                else if (strstr(t, "\"close\"") && c >= 0) {
                    if (ch[c].open) fprintf(stderr, "recomp: relay: the human manager on channel %d left\n", c);
                    close_channel(&ch[c]);
                    hn_lock();
                    forget_incoming(c);
                    hn_unlock();
                }
            } else if (m.len >= 1 && m.data[0] < RELAY_CHANNELS && ch[m.data[0]].open) {
                Channel *c = &ch[m.data[0]];
                c->frames_in++, c->bytes_in += m.len - 1;
                if (c->to_game.len - c->to_game.off + m.len > RELAY_QUEUE_MAX || put(&c->to_game, m.data + 1, m.len - 1) < 0)
                    c->ended = 1;
            }
        }
        if (r < 0) {
            int code_now = ws_close_code(w);
            close_all(ch);
            ws_close(w, 0);
            w = NULL;
            fprintf(stderr, "recomp: relay: the relay closed the host's connection (%d)\n", code_now);
            if (code_now == 4001) {         /* replaced: another connection with this token took over */
                hn_lock();
                if (host.gen == gen) host.replaced = 1;
                hn_unlock();
            }
            gen_seen_before = 1;
            retry_at = hn_ms() + (uint64_t)backoff;
            continue;
        }
        now = hn_ms();
        int due = 0;
        for (int i = 0; i < RELAY_CHANNELS; i++) {
            Channel *c = &ch[i];
            if (!c->open) continue;
            if (give(&c->to_game, c->mine) < 0) c->ended = 1;
            size_t had = c->batch.len - c->batch.off;
            int t = c->ended ? 0 : take(&c->batch, c->mine);
            if (t < 0) c->ended = 1;
            if (!had && c->batch.len > c->batch.off) c->first = now;
            if (c->ended || (c->batch.len > c->batch.off &&
                             (now - c->first >= RELAY_BATCH_MS || c->batch.len - c->batch.off >= RELAY_BATCH_MAX ||
                              (now - c->last_frame >= RELAY_BATCH_MS && now - c->first >= RELAY_QUIET_MS))))
                due = 1;
        }
        /* One moment for all channels: a screen that the host sends to every human manager leaves together. */
        int broke = due && send_batches(w, ch, &last_sent, 0) < 0;
        if (!broke && now - last_sent >= RELAY_PING_MS) {
            broke = ws_send(w, 1, "ping", 4, SEND_MS) < 0;
            last_sent = now;
        }
        if (broke) {
            close_all(ch);
            ws_close(w, 0);
            w = NULL;
            fprintf(stderr, "recomp: relay: the host's connection broke\n");
            gen_seen_before = 1;
            retry_at = hn_ms() + (uint64_t)backoff;
        }
    }
}

void relay_host_set(const char *lobby, const char *code, const char *token)
{
    hn_lock();
    if ((!host.active && !host.replaced) || strcmp(host.code, code) || strcmp(host.token, token) || strcmp(host.lobby, lobby)) {
        snprintf(host.lobby, sizeof host.lobby, "%s", lobby);
        snprintf(host.code, sizeof host.code, "%s", code);
        snprintf(host.token, sizeof host.token, "%s", token);
        host.active = 1, host.replaced = 0;
        host.gen++;
        if (!host.started && hn_thread(host_thread, NULL) == 0) host.started = 1;
    }
    hn_unlock();
}

void relay_host_stop(void)
{
    hn_lock();
    if (host.active) host.active = 0, host.gen++;
    host.replaced = 0;
    hn_unlock();
}

void relay_host_finish(int ms)
{
    relay_host_stop();
    uint64_t until = hn_ms() + (uint64_t)ms;
    for (;;) {
        hn_lock();
        int done = !host.started || host.idle_gen == host.gen;
        hn_unlock();
        if (done || hn_ms() >= until) return;
        hn_sleep(10);
    }
}

hn_sock relay_take_joiner(uint32_t *addr, uint16_t *port)
{
    hn_sock s = HN_INVALID;
    hn_lock();
    if (host.incoming_n) {
        s = host.incoming[0].s;
        uint32_t a = LR_RELAY_PEERS + (uint32_t)host.incoming[0].ch;
        const uint8_t b[4] = {(uint8_t)(a >> 24), (uint8_t)(a >> 16), (uint8_t)(a >> 8), (uint8_t)a};
        memcpy(addr, b, 4);
        *port = (uint16_t)(50000 + host.incoming[0].ch);
        memmove(host.incoming, host.incoming + 1, (size_t)(--host.incoming_n) * sizeof host.incoming[0]);
    }
    hn_unlock();
    return s;
}

/* ---- human manager ---- */

typedef struct {
    WsConn *w;
    hn_sock mine;
} Link;

static void joiner_thread(void *arg)
{
    Link *l = arg;
    Queue batch = {0}, to_game = {0};
    uint64_t first = 0, last_sent = hn_ms(), opened = hn_ms(), bytes_out = 0, bytes_in = 0;
    unsigned frames_out = 0, frames_in = 0;
    int ended = 0, code = 0;
    while (!ended) {
        ws_wait(l->w, WAIT_MS);
        WsMessage m;
        int r;
        while ((r = ws_poll(l->w, &m)) == 1)
            if (!m.text) {
                frames_in++, bytes_in += m.len;
                if (to_game.len - to_game.off + m.len > RELAY_QUEUE_MAX || put(&to_game, m.data, m.len) < 0) {
                    r = -1;
                    break;
                }
            }
        if (r < 0) {
            code = ws_close_code(l->w);
            break;
        }
        if (give(&to_game, l->mine) < 0) ended = 1;
        size_t had = batch.len - batch.off;
        if (!ended && take(&batch, l->mine) < 0) ended = 1;
        uint64_t now = hn_ms();
        if (!had && batch.len > batch.off) first = now;
        size_t n = batch.len - batch.off;
        if (n && (ended || now - first >= RELAY_JOIN_MS || n >= RELAY_BATCH_MAX)) {
            frames_out++, bytes_out += n;
            if (ws_send(l->w, 0, batch.p + batch.off, n, SEND_MS) < 0) code = 1006, ended = 1;
            batch.off = batch.len = 0, last_sent = now;
        }
        if (!ended && now - last_sent >= RELAY_PING_MS) {
            if (ws_send(l->w, 1, "ping", 4, SEND_MS) < 0) code = 1006, ended = 1;
            last_sent = now;
        }
    }
    if (code) {
        /* The relay ended it (4000: the host left, 4003: a kick): the game reads what came, then the end. */
        fprintf(stderr, "recomp: relay: the relay closed the connection (%d)\n", code);
        uint64_t until = hn_ms() + CLOSE_FLUSH_MS;
        while (to_game.off < to_game.len && give(&to_game, l->mine) == 0 && hn_ms() < until) hn_wait(l->mine, 0, 20);
    } else fprintf(stderr, "recomp: relay: the game closed its connection to the host\n");
    fprintf(stderr, "recomp: relay: the connection closed after %llu s: sent %u frames, %llu bytes; got %u frames, "
            "%llu bytes\n", (unsigned long long)((hn_ms() - opened) / 1000), frames_out,
            (unsigned long long)bytes_out, frames_in, (unsigned long long)bytes_in);
    ws_close(l->w, code ? 0 : 1000);
    uint64_t until = hn_ms() + LINGER_MS;   /* see "Ends" */
    if (hn_shutdown_send(l->mine) == 0)
        while (!drained(l->mine) && hn_ms() < until) hn_wait(l->mine, 1, 20);
    hn_close(l->mine);
    drop(&batch), drop(&to_game);
    free(l);
}

/* text as a JSON string's inside: '"', '\' and controls escaped. */
static void json_text(char *out, size_t room, const char *text)
{
    size_t n = 0;
    for (; *text && n + 7 < room; text++) {
        unsigned char c = (unsigned char)*text;
        if (c == '"' || c == '\\') out[n++] = '\\', out[n++] = (char)c;
        else if (c < 0x20) n += (size_t)snprintf(out + n, room - n, "\\u%04x", c);
        else out[n++] = (char)c;
    }
    out[n] = 0;
}

RelayJoin relay_join(const char *lobby, const char *code, const char *version, const char *name,
                     const char *password, int timeout_ms, hn_sock *s, char *host_version, size_t room)
{
    *s = HN_INVALID;
    if (host_version && room) host_version[0] = 0;
    char path[64], json[512], v[64], n[128], p[160];
    json_text(v, sizeof v, version), json_text(n, sizeof n, name), json_text(p, sizeof p, password ? password : "");
    snprintf(path, sizeof path, "/v1/games/%s/join", code);
    snprintf(json, sizeof json, "{\"version\":\"%s\",\"name\":\"%s\",\"password\":\"%s\"}", v, n, p);
    LhAnswer *a = malloc(sizeof *a);
    if (!a) return RJ_UNAVAILABLE;
    LhResult r = lobby_http(lobby, "POST", path, NULL, json, timeout_ms, a);
    RelayJoin out = RJ_UNAVAILABLE;
    char ticket[256] = "", why[40] = "";
    if (r == LH_OK) lobby_json_word(a->body, "ticket", ticket, sizeof ticket);
    else if (r == LH_FULL_TODAY) out = RJ_FULL_TODAY;
    else if (r == LH_API_ERROR) {
        snprintf(why, sizeof why, "%s", a->code);
        if (!strcmp(why, "unknown_game")) out = RJ_GONE;
        else if (!strcmp(why, "version_mismatch")) {
            out = RJ_VERSION;
            if (host_version && room) lobby_json_word(a->body, "hostVersion", host_version, room);
        } else if (!strcmp(why, "wrong_password")) out = RJ_PASSWORD;
        else if (!strcmp(why, "too_many_attempts")) out = RJ_ATTEMPTS;
        else if (!strcmp(why, "name_not_allowed") || !strcmp(why, "bad_request")) out = RJ_NAME;
    }
    if (!ticket[0]) {
        fprintf(stderr, "recomp: relay: the lobby gave no ticket for %s (%d %s)\n", code, a->status, why);
        free(a);
        return out;
    }
    char query[300], url[600];
    snprintf(query, sizeof query, "role=manager&ticket=%s", ticket);
    int status = 0;
    WsConn *w = relay_url(url, sizeof url, lobby, code, query) ?
        ws_open(url, timeout_ms, &status, a->body, sizeof a->body) : NULL;
    if (!w) {
        lobby_json_word(a->body, "error", why, sizeof why);
        fprintf(stderr, "recomp: relay: the relay refused the join to %s (%d %s)\n", code, status, why);
        out = !strcmp(why, "relay_full") ? RJ_FULL : !strcmp(why, "host_not_connected") ? RJ_HOST_AWAY :
              !strcmp(why, "bad_ticket") || !strcmp(why, "unknown_game") ? RJ_GONE : RJ_RELAY_DOWN;
        free(a);
        return out;
    }
    free(a);
    Link *l = malloc(sizeof *l);
    hn_sock game = HN_INVALID, mine = HN_INVALID;
    if (!l || hn_pair(&game, &mine) < 0) {
        free(l);
        ws_close(w, 1000);
        return RJ_UNAVAILABLE;
    }
    l->w = w, l->mine = mine;
    if (hn_thread(joiner_thread, l) < 0) {
        ws_close(w, 1000);
        hn_close(game), hn_close(mine);
        free(l);
        return RJ_UNAVAILABLE;
    }
    fprintf(stderr, "recomp: relay: joined %s through the relay\n", code);
    *s = game;
    return RJ_OK;
}
