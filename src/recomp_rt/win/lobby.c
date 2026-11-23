/* The lobby client (lobby.h). The lobby thread starts at the first wish and runs until the program ends.
 *
 * Host entry (server/README.md, Endpoints):
 * - register: POST /v1/games with every field. The answer gives the code, the host token and the heartbeat
 *   time. After full_today or unavailable the thread tries again after RETRY_MS. Any other refusal (a name that
 *   the word filter refuses, two games from this address, a game that is open already) ends the try.
 * - heartbeat: POST /v1/games/<code>/heartbeat every heartbeatSeconds, with the latest fields. unknown_game:
 *   register again with code and hostToken. If that gives not_host, the game expired: register as a new game.
 * - close: DELETE /v1/games/<code>.
 * List: GET /v1/games.
 * One game (the invite link, slice 8): GET /v1/games/<code>. */
#include "lobby.h"
#include "lobby_http.h"
#include "../host_net.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETRY_MS 60000u
#define TICK_MS 50
#define JSON_MAX 4096                /* the lobby's limit for a request body */

static char base[256];

/* Shared with the lobby thread, under hn_lock. */
static struct {
    int started;
    int want_open, want_close, have_game;
    LobbyGame game;
    char password[65];
    LobbyHostState state;
    char code[9], token[96];
    char refusal[32];                    /* the error code of a refused entry */
    int want_list, list_ready, list_result, list_n;
    LobbyRow list[LOBBY_ROWS];
    char want_game[9], game_code[9];     /* game_code: the last code asked for; an older answer is dropped */
    int game_ready, game_result;
    LobbyRow game_row;
} sh;

/* ---- text ---- */

void lobby_clean(char *out, unsigned room, const char *in)
{
    unsigned n = 0;
    if (!room) return;
    while (*in == ' ') in++;
    for (; *in && n + 1 < room; in++) {
        unsigned char c = (unsigned char)*in;
        out[n++] = (char)((c >= 0x20 && c <= 0x7e) || c >= 0xa0 ? c : '?');
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = 0;
}

typedef struct {
    char *p;
    unsigned room, n;
} Out;

static void put(Out *o, const char *fmt, const char *s)
{
    int r = snprintf(o->p + o->n, o->room > o->n ? o->room - o->n : 0, fmt, s);
    if (r > 0) o->n += (unsigned)r;
}

/* A Latin-1 text as a JSON string. Bytes from 0x80 are \u00XX. */
static void put_str(Out *o, const char *s)
{
    char e[8];
    put(o, "%s", "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') snprintf(e, sizeof e, "\\%c", c);
        else if (c < 0x20 || c >= 0x7f) snprintf(e, sizeof e, "\\u%04x", c);
        else e[0] = (char)c, e[1] = 0;
        put(o, "%s", e);
    }
    put(o, "%s", "\"");
}

static void put_int(Out *o, const char *key, int v)
{
    char t[48];
    snprintf(t, sizeof t, ",\"%s\":%d", key, v);
    put(o, "%s", t);
}

static void put_field(Out *o, const char *key, const char *v)
{
    char t[48];
    snprintf(t, sizeof t, ",\"%s\":", key);
    put(o, "%s", t);
    put_str(o, v);
}

/* The fields that a heartbeat may carry. */
static void put_state(Out *o, const LobbyGame *g)
{
    put_int(o, "season", g->season);
    put_field(o, "day", g->day);
    put(o, "%s", ",\"humanClubs\":[");
    for (int i = 0; i < g->clubs_n; i++) {
        if (i) put(o, "%s", ",");
        put_str(o, g->clubs[i]);
    }
    put(o, "%s", "]");
    put(o, "%s", ",\"awayClubs\":[");
    for (int i = 0; i < g->away_n; i++) {
        if (i) put(o, "%s", ",");
        put_str(o, g->away[i]);
    }
    put(o, "%s", "]");
    put_int(o, "freeSeats", g->free_seats);
    put_int(o, "managersOnline", g->managers_online);
    put_int(o, "pingMs", g->ping_ms);
}

int lobby_game_json(char *out, unsigned room, const LobbyGame *g, const char *password, const char *code,
                    const char *token)
{
    Out o = {out, room, 0};
    put(&o, "%s", "{\"gameId\":");
    put_str(&o, g->game_id);
    put_field(&o, "name", g->name);
    put_field(&o, "hostName", g->host_name);
    put_field(&o, "version", g->version);
    put_field(&o, "database", g->database);
    put_state(&o, g);
    if (password && *password) put_field(&o, "password", password);
    if (code && *code) put_field(&o, "code", code);
    if (token && *token) put_field(&o, "hostToken", token);
    put(&o, "%s", "}");
    return o.n < room ? (int)o.n : -1;
}

/* password: the new game password after a change on the Network tab (0.7.0 slice 6), "" to remove it, NULL when it
 * did not change. */
static int heartbeat_json(char *out, unsigned room, const LobbyGame *g, const char *password)
{
    Out o = {out, room, 0};
    put(&o, "%s", "{\"v\":1");
    put_state(&o, g);
    if (password) put_field(&o, "password", password);
    put(&o, "%s", "}");
    return o.n < room ? (int)o.n : -1;
}

/* ---- JSON reading: only what the list answer and the register answer need ---- */

typedef struct {
    const char *p;
    int depth;                       /* of the containers that skip is in: an answer may nest at most JSON_DEPTH */
} J;

#define JSON_DEPTH 32

static void ws(J *j)
{
    while (isspace((unsigned char)*j->p)) j->p++;
}

/* A string as Latin-1 (code points above 0xff become '?'), cut to room - 1 bytes. 0 when it is not one. */
static int read_str(J *j, char *out, unsigned room)
{
    unsigned n = 0;
    ws(j);
    if (*j->p != '"') return 0;
    j->p++;
    while (*j->p && *j->p != '"') {
        unsigned c = (unsigned char)*j->p++;
        if (c == '\\') {
            char e = *j->p ? *j->p++ : 0;
            if (e == 'u') {
                char hex[5] = {0};
                for (int i = 0; i < 4 && *j->p; i++) hex[i] = *j->p++;
                c = (unsigned)strtoul(hex, NULL, 16);
            } else c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' ? '\b' : e == 'f' ? '\f' : (unsigned char)e;
        } else if (c >= 0xc0) {                          /* UTF-8 */
            int more = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
            c &= 0x3fu >> more;
            for (int i = 0; i < more && (*j->p & 0xc0) == 0x80; i++) c = c << 6 | (*j->p++ & 0x3fu);
        }
        if (n + 1 < room) out[n++] = (char)(c > 0xff ? '?' : c);
    }
    if (*j->p != '"') return 0;
    j->p++;
    if (room) out[n < room ? n : room - 1] = 0;
    return 1;
}

static int skip(J *j);

static int skip_container(J *j, char close)
{
    if (++j->depth > JSON_DEPTH) return 0;
    j->p++;
    ws(j);
    if (*j->p == close) {
        j->depth--;
        return j->p++, 1;
    }
    for (;;) {
        if (close == '}') {
            char k[2];
            if (!read_str(j, k, sizeof k)) return 0;
            ws(j);
            if (*j->p++ != ':') return 0;
        }
        if (!skip(j)) return 0;
        ws(j);
        if (*j->p == ',') {
            j->p++;
            ws(j);
            continue;
        }
        if (*j->p == close) {
            j->depth--;
            return j->p++, 1;
        }
        return 0;
    }
}

static int skip(J *j)
{
    char t[2];
    ws(j);
    switch (*j->p) {
    case '"': return read_str(j, t, sizeof t);
    case '{': return skip_container(j, '}');
    case '[': return skip_container(j, ']');
    case 0: return 0;
    default:
        while (*j->p && !strchr(",}] \t\r\n", *j->p)) j->p++;
        return 1;
    }
}

/* A number, cut to an int. Another value is skipped and *v stays. */
static int read_int(J *j, int *v)
{
    ws(j);
    char *end;
    long x = strtol(j->p, &end, 10);
    if (end == j->p) return skip(j);
    j->p = end;
    while (*j->p && !strchr(",}] \t\r\n", *j->p)) j->p++;    /* a fraction or exponent */
    *v = (int)x;
    return 1;
}

/* Calls each(key) for each key of the object at j, with j after the colon. each reads or skips the value. */
static int object(J *j, int (*each)(J *, const char *, void *), void *ctx)
{
    ws(j);
    if (*j->p != '{') return 0;
    j->p++;
    ws(j);
    if (*j->p == '}') return j->p++, 1;
    for (;;) {
        char key[32];
        if (!read_str(j, key, sizeof key)) return 0;
        ws(j);
        if (*j->p++ != ':') return 0;
        if (!each(j, key, ctx)) return 0;
        ws(j);
        if (*j->p == ',') {
            j->p++;
            continue;
        }
        if (*j->p == '}') return j->p++, 1;
        return 0;
    }
}

static int game_key(J *j, const char *k, void *ctx)
{
    LobbyRow *r = ctx;
    LobbyGame *g = &r->g;
    if (!strcmp(k, "code")) return read_str(j, r->code, sizeof r->code);
    if (!strcmp(k, "name")) return read_str(j, g->name, sizeof g->name);
    if (!strcmp(k, "hostName")) return read_str(j, g->host_name, sizeof g->host_name);
    if (!strcmp(k, "version")) return read_str(j, g->version, sizeof g->version);
    if (!strcmp(k, "database")) return read_str(j, g->database, sizeof g->database);
    if (!strcmp(k, "day")) return read_str(j, g->day, sizeof g->day);
    if (!strcmp(k, "season")) return read_int(j, &g->season);
    if (!strcmp(k, "freeSeats")) return read_int(j, &g->free_seats);
    if (!strcmp(k, "managersOnline")) return read_int(j, &g->managers_online);
    if (!strcmp(k, "pingMs")) return read_int(j, &g->ping_ms);
    if (!strcmp(k, "password")) {
        ws(j);
        g->password = !strncmp(j->p, "true", 4);
        return skip(j);
    }
    if (!strcmp(k, "humanClubs") || !strcmp(k, "awayClubs")) {
        int away = k[0] == 'a';
        char (*list)[LOBBY_TEXT] = away ? g->away : g->clubs;
        int *count = away ? &g->away_n : &g->clubs_n;
        ws(j);
        if (*j->p != '[') return skip(j);
        j->p++;
        ws(j);
        while (*j->p && *j->p != ']') {
            char club[LOBBY_TEXT];
            if (!read_str(j, club, sizeof club)) return 0;
            if (*count < LOBBY_CLUBS) memcpy(list[(*count)++], club, sizeof club);
            ws(j);
            if (*j->p == ',') j->p++, ws(j);
        }
        if (*j->p != ']') return 0;
        j->p++;
        return 1;
    }
    return skip(j);
}

typedef struct {
    LobbyRow *rows;
    int max, n;
} List;

static int list_key(J *j, const char *k, void *ctx)
{
    List *l = ctx;
    if (strcmp(k, "games")) return skip(j);
    ws(j);
    if (*j->p != '[') return skip(j);
    j->p++;
    ws(j);
    while (*j->p == '{') {
        LobbyRow r;
        memset(&r, 0, sizeof r);
        if (!object(j, game_key, &r)) return 0;
        if (l->n < l->max && r.code[0] && r.g.name[0]) l->rows[l->n++] = r;
        ws(j);
        if (*j->p == ',') j->p++, ws(j);
    }
    if (*j->p != ']') return 0;
    j->p++;
    return 1;
}

int lobby_parse_games(const char *json, LobbyRow *rows, int max)
{
    List l = {rows, max, 0};
    J j = {json, 0};
    return object(&j, list_key, &l) ? l.n : -1;
}

static int one_key(J *j, const char *k, void *ctx)
{
    return strcmp(k, "game") ? skip(j) : object(j, game_key, ctx);
}

int lobby_parse_game(const char *json, LobbyRow *row)
{
    J j = {json, 0};
    memset(row, 0, sizeof *row);
    return object(&j, one_key, row) && row->code[0] && row->g.name[0];
}

typedef struct {
    char *code, *token;
    int beat;
} Registered;

static int register_key(J *j, const char *k, void *ctx)
{
    Registered *r = ctx;
    if (!strcmp(k, "code")) return read_str(j, r->code, 9);
    if (!strcmp(k, "hostToken")) return read_str(j, r->token, 96);
    if (!strcmp(k, "heartbeatSeconds")) return read_int(j, &r->beat);
    return skip(j);
}

/* ---- the lobby thread ---- */

static LhAnswer answer;            /* the lobby thread's only */

static void say(const char *what, LhResult r)
{
    fprintf(stderr, "recomp: lobby: %s: %s%s%s (%d)\n", what,
            r == LH_OK ? "ok" : r == LH_FULL_TODAY ? "full today" : r == LH_UNAVAILABLE ? "unavailable" : "refused",
            answer.code[0] ? " " : "", answer.code, answer.status);
}

static void set_state(LobbyHostState s)
{
    hn_lock();
    sh.state = s;
    hn_unlock();
}

static void lobby_thread(void *unused)
{
    (void)unused;
    char code[9] = "", token[96] = "", path[64], json[JSON_MAX];
    uint64_t next_beat = 0, retry_at = 0;
    unsigned beat_ms = 30000;
    int open = 0;
    LobbyGame g;
    char password[65], sent_password[65] = "";   /* the password now, and the one the lobby has */
    for (;;) {
        hn_lock();
        int want_close = sh.want_close, want_open = sh.want_open && sh.have_game, want_list = sh.want_list;
        g = sh.game;
        memcpy(password, sh.password, sizeof password);
        sh.want_list = 0;
        hn_unlock();
        uint64_t now = hn_ms();

        if (want_close) {
            if (open) {
                snprintf(path, sizeof path, "/v1/games/%s", code);
                say("close", lobby_http(base, "DELETE", path, token, NULL, LH_TIMEOUT_MS, &answer));
            }
            open = 0, code[0] = token[0] = 0;
            hn_lock();
            sh.want_close = sh.want_open = sh.have_game = 0;
            sh.state = LOBBY_CLOSED;
            hn_unlock();
        } else if (want_open && !open && now >= retry_at) {
            set_state(LOBBY_REGISTERING);
            LhResult r = LH_UNAVAILABLE;
            if (lobby_game_json(json, sizeof json, &g, password, code, token) > 0)
                r = lobby_http(base, "POST", "/v1/games", NULL, json, LH_TIMEOUT_MS, &answer);
            if (r == LH_API_ERROR && !strcmp(answer.code, "not_host") && code[0]) {
                code[0] = token[0] = 0;                  /* the game expired: a new game, with a new code */
                if (lobby_game_json(json, sizeof json, &g, password, NULL, NULL) > 0)
                    r = lobby_http(base, "POST", "/v1/games", NULL, json, LH_TIMEOUT_MS, &answer);
            }
            say("register", r);
            Registered reg = {code, token, 30};
            J j = {answer.body, 0};
            if (r == LH_OK && object(&j, register_key, &reg) && code[0] && token[0]) {
                open = 1;
                memcpy(sent_password, password, sizeof sent_password);
                beat_ms = reg.beat > 0 && reg.beat <= 60 ? (unsigned)reg.beat * 1000u : 30000u;
                next_beat = hn_ms() + beat_ms;
                fprintf(stderr, "recomp: lobby: the game is open as %s\n", code);
                hn_lock();
                memcpy(sh.code, code, sizeof code);
                memcpy(sh.token, token, sizeof token);
                sh.state = LOBBY_OPEN;
                hn_unlock();
            } else if (r == LH_FULL_TODAY || r == LH_UNAVAILABLE) {
                retry_at = hn_ms() + RETRY_MS;
                set_state(r == LH_FULL_TODAY ? LOBBY_FULL_TODAY : LOBBY_UNAVAILABLE);
            } else {
                hn_lock();
                sh.want_open = 0;                        /* refused: the game stays on the LAN */
                sh.state = LOBBY_REFUSED;
                memcpy(sh.refusal, answer.code, sizeof sh.refusal);
                hn_unlock();
            }
        } else if (open && now >= next_beat) {
            snprintf(path, sizeof path, "/v1/games/%s/heartbeat", code);
            int changed = strcmp(password, sent_password) != 0;
            LhResult r = heartbeat_json(json, sizeof json, &g, changed ? password : NULL) > 0 ?
                lobby_http(base, "POST", path, token, json, LH_TIMEOUT_MS, &answer) : LH_UNAVAILABLE;
            next_beat = hn_ms() + beat_ms;
            if (r == LH_OK && changed) memcpy(sent_password, password, sizeof sent_password);
            if (r != LH_OK) say("heartbeat", r);
            if (r == LH_API_ERROR && !strcmp(answer.code, "unknown_game")) {
                open = 0;                                /* register again at once, with code and token */
                retry_at = 0;
            } else if (r == LH_API_ERROR && !strcmp(answer.code, "not_host")) {
                open = 0, retry_at = 0, code[0] = token[0] = 0;
            }
        }

        if (want_list) {
            LhResult r = lobby_http(base, "GET", "/v1/games", NULL, NULL, LH_TIMEOUT_MS, &answer);
            static LobbyRow rows[LOBBY_ROWS];
            int n = r == LH_OK ? lobby_parse_games(answer.body, rows, LOBBY_ROWS) : 0;
            if (n < 0) n = 0, r = LH_UNAVAILABLE;
            if (r != LH_OK) say("list", r);
            else fprintf(stderr, "recomp: lobby: list: %d games\n", n);
            hn_lock();
            memcpy(sh.list, rows, (size_t)n * sizeof rows[0]);
            sh.list_n = n, sh.list_result = r, sh.list_ready = 1;
            hn_unlock();
        }

        hn_lock();
        char want_game[9];
        memcpy(want_game, sh.want_game, sizeof want_game);
        sh.want_game[0] = 0;
        hn_unlock();
        if (want_game[0]) {
            snprintf(path, sizeof path, "/v1/games/%s", want_game);
            LhResult r = lobby_http(base, "GET", path, NULL, NULL, LH_TIMEOUT_MS, &answer);
            static LobbyRow row;
            if (r == LH_OK && (!lobby_parse_game(answer.body, &row) || strcmp(row.code, want_game))) r = LH_UNAVAILABLE;
            say("game", r);
            hn_lock();
            if (!sh.want_game[0] && !strcmp(sh.game_code, want_game)) {
                sh.game_row = row;
                sh.game_result = r, sh.game_ready = 1;
            }
            hn_unlock();
        }
        hn_sleep(TICK_MS);
    }
}

/* Under hn_lock. */
static void start_thread(void)
{
    if (sh.started) return;
    sh.started = 1;
    if (hn_thread(lobby_thread, NULL) < 0) fprintf(stderr, "recomp: lobby: no thread, online games are off\n");
}

void lobby_set_address(const char *b) { snprintf(base, sizeof base, "%s", b ? b : ""); }
int lobby_usable(void) { return lobby_http_usable(base); }

void lobby_host_set(const LobbyGame *g, const char *password)
{
    if (!lobby_usable()) return;
    hn_lock();
    if (!sh.have_game) sh.want_open = 1;
    /* A later change goes to the lobby at the next heartbeat (0.7.0 slice 6). */
    snprintf(sh.password, sizeof sh.password, "%s", password ? password : "");
    sh.have_game = 1;
    sh.game = *g;
    start_thread();
    hn_unlock();
}

void lobby_host_close(void)
{
    hn_lock();
    if (sh.have_game) sh.want_close = 1;
    hn_unlock();
}

LobbyHostState lobby_host_state(char *code_out)
{
    hn_lock();
    LobbyHostState s = sh.state;
    if (code_out) memcpy(code_out, sh.code, 9);
    hn_unlock();
    return s;
}

int lobby_host_relay(char code[9], char token[96])
{
    hn_lock();
    int open = sh.state == LOBBY_OPEN && sh.code[0] && sh.token[0];
    memcpy(code, sh.code, 9);
    memcpy(token, sh.token, 96);
    hn_unlock();
    return open;
}

const char *lobby_address(void) { return base; }

void lobby_host_refusal(char *out, unsigned room)
{
    hn_lock();
    snprintf(out, room, "%s", sh.state == LOBBY_REFUSED ? sh.refusal : "");
    hn_unlock();
}

void lobby_list_fetch(void)
{
    if (!lobby_usable()) return;
    hn_lock();
    sh.want_list = 1;
    start_thread();
    hn_unlock();
}

int lobby_list_take(LobbyRow *rows, int max, int *result)
{
    int n = -1;
    hn_lock();
    if (sh.list_ready) {
        n = sh.list_n < max ? sh.list_n : max;
        memcpy(rows, sh.list, (size_t)n * sizeof rows[0]);
        *result = sh.list_result;
        sh.list_ready = 0;
    }
    hn_unlock();
    return n;
}

void lobby_game_fetch(const char *code)
{
    if (!lobby_usable()) return;
    hn_lock();
    snprintf(sh.want_game, sizeof sh.want_game, "%s", code);
    memcpy(sh.game_code, sh.want_game, sizeof sh.game_code);
    sh.game_ready = 0;
    start_thread();
    hn_unlock();
}

int lobby_game_take(LobbyRow *row, int *result)
{
    int got = 0;
    hn_lock();
    if (sh.game_ready) {
        *row = sh.game_row;
        *result = sh.game_result;
        sh.game_ready = 0;
        got = 1;
    }
    hn_unlock();
    return got;
}

void lobby_finish(int ms)
{
    uint64_t until = hn_ms() + (uint64_t)ms;
    for (;;) {
        hn_lock();
        int waiting = sh.started && sh.want_close;
        hn_unlock();
        if (!waiting || hn_ms() >= until) return;
        hn_sleep(20);
    }
}
