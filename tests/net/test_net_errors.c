/* Clear errors, connection test and ping (0.7.0 slice 7, docs/specs/2026-10-27-online-play-0.7.0-spec.md):
 * - the control packets of net_core.c: the game reads only its own bytes, the host learns the ping, and the joiner
 *   tells a Kick, a broken connection and a host quit apart;
 * - the reasons of failed connects and binds;
 * - every text of net_errors.c;
 * - the ping and the "lobby does not answer" row of the game list, and pingMs in the lobby's JSON;
 * - the connection test against a fake lobby, one run for each result. */
#include "net_core.h"
#include "net_errors.h"
#include "lobby.h"
#include "lobby_rows.h"
#include "lobby_http.h"
#include "host_net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- control packets ---- */

static const uint8_t game1[] = {6, 0, 0, 0, 0, 'h', 'e', 'l', 'l', 'o'};     /* opcode 0, a host screen */
static const uint8_t game2[] = {3, 0, 0, 0, 3, 'a', 'b'};                    /* opcode 3 */
static const uint8_t from_joiner[] = {2, 0, 0, 0, 1, 'c'};                    /* a click */
enum { END_QUIT, END_KICK, END_CLOSE, END_EXIT };

/* The host: sends game1 with a PING before it, game2 one byte at a time (as Network::send_packet 0x77ac30 sends the
 * length, 0x77ad10..0x77ad46) with PINGs that must wait for the packet's end, reads the joiner's click with no
 * control bytes, waits for a ping, then ends the way that end says. Exit 0 when all of it held. */
static pid_t host(uint32_t me, uint16_t port, int end)
{
    pid_t pid = fork();
    if (pid) return pid;
    nc_set_identity("7.7.7", "", "");
    nc_set_ping_every(0);                              /* a PING at every call */
    nc_start();
    uint32_t l = nc_socket(2, 1);
    nc_setopt(l, NC_SOL_SOCKET, NC_SO_REUSEADDR, 1, 0);
    nc_nonblocking(l, 1);
    if (nc_bind(l, me, port) < 0 || nc_listen(l, 16) < 0) _exit(10);
    uint32_t a = NC_INVALID;
    for (uint64_t until = hn_ms() + 8000; a == NC_INVALID && hn_ms() < until; hn_sleep(2)) a = nc_accept(l, NULL, NULL);
    if (a == NC_INVALID) _exit(11);
    nc_nonblocking(a, 1);
    nc_setopt(a, NC_SOL_SOCKET, NC_SO_LINGER, 1, 0);   /* as the game's connections: SO_LINGER {1, 0} (0x8ae4d4) */
    if (nc_send(a, game1, sizeof game1) != (long)sizeof game1) _exit(12);
    for (unsigned i = 0; i < sizeof game2; i++) {
        if (nc_send(a, game2 + i, 1) != 1) _exit(13);
        hn_sleep(20);                                  /* the joiner may read the length bytes in between */
    }
    uint8_t got[16];
    unsigned have = 0;
    for (uint64_t until = hn_ms() + 8000; have < sizeof from_joiner && hn_ms() < until;) {
        long r = nc_recv(a, got + have, have < 4 ? 1 : (uint32_t)(sizeof from_joiner - have));
        if (r > 0) have += (unsigned)r;
        else hn_sleep(2);
    }
    if (have != sizeof from_joiner || memcmp(got, from_joiner, sizeof from_joiner)) _exit(14);
    for (uint64_t until = hn_ms() + 8000; nc_ping_ms(a) < 0 && hn_ms() < until; hn_sleep(2)) nc_recv(a, got, 1);
    if (nc_ping_ms(a) < 0 || nc_ping_ms(a) > 5000) _exit(15);
    nc_set_ping_every(60000);
    hn_sleep(100);
    if (end == END_QUIT) {
        static const uint8_t quit[5] = {1, 0, 0, 0, 4};
        if (nc_send(a, quit, 5) != 5) _exit(16);
        nc_close(a);
    } else if (end == END_KICK) {
        if (nc_kick(a) != 0) _exit(17);
    } else if (end == END_EXIT) {
        exit(0);                                       /* a window close: the runtime's QUIT at exit */
    } else nc_close(a);
    hn_sleep(300);
    _exit(0);
}

/* Reads n bytes as receive_packet 0x77af20 does: the length one byte at a time, then the body. */
static int read_packet(uint32_t c, uint8_t *out, unsigned n)
{
    unsigned have = 0;
    for (uint64_t until = hn_ms() + 8000; have < n && hn_ms() < until;) {
        long r = nc_recv(c, out + have, have < 4 ? 1 : n - have);
        if (r > 0) have += (unsigned)r;
        else if (r < 0 && nc_error() == HN_WOULDBLOCK) hn_sleep(2);
        else break;
    }
    return (int)have;
}

/* A control packet that arrives between the runtime's look for one and the game's read (slice 9's quit leg of
 * 2026-10-29: the host's game read a PONG as a packet of opcode 0xbe & 31 = 30). The host sends a PING at every call
 * for RACE_MS and no game bytes; the joiner reads as receive_packet 0x77af20 does, one byte at a time. Every byte
 * that the joiner's game gets is a control packet that reached it. */
#define RACE_MS 3000
static void test_control_race(void)
{
    uint32_t me;
    if (hn_route_address(&me) < 0) return;
    uint16_t port = (uint16_t)(24000 + getpid() % 4000 * 2 + 1);
    pid_t pid = fork();
    if (!pid) {
        nc_set_identity("7.7.7", "", "");
        nc_set_ping_every(0);
        nc_start();
        uint32_t l = nc_socket(2, 1);
        nc_setopt(l, NC_SOL_SOCKET, NC_SO_REUSEADDR, 1, 0);
        nc_nonblocking(l, 1);
        if (nc_bind(l, me, port) < 0 || nc_listen(l, 16) < 0) _exit(10);
        uint32_t a = NC_INVALID;
        for (uint64_t until = hn_ms() + 8000; a == NC_INVALID && hn_ms() < until; hn_sleep(2)) a = nc_accept(l, NULL, NULL);
        if (a == NC_INVALID) _exit(11);
        nc_nonblocking(a, 1);
        uint8_t b[8];
        for (uint64_t until = hn_ms() + RACE_MS + 500; hn_ms() < until; hn_sleep(1)) nc_recv(a, b, 1);
        nc_close(a);
        _exit(0);
    }
    hn_sleep(200);
    nc_set_identity("7.7.7", "", "");
    uint32_t c = nc_socket(2, 1);
    CHECK(nc_connect(c, me, port) == 0, "race: connect: %d", nc_error());
    nc_nonblocking(c, 1);
    long leaked = 0;
    uint8_t b[8];
    for (uint64_t until = hn_ms() + RACE_MS; hn_ms() < until;) {
        long r = nc_recv(c, b, 1);
        if (r > 0) leaked += r;
    }
    CHECK(leaked == 0, "race: the game read %ld bytes of control packets", leaked);
    nc_close(c);
    int status;
    waitpid(pid, &status, 0);
}

static void test_control(void)
{
    uint32_t me;
    if (hn_route_address(&me) < 0) {
        printf("net_errors: no default route, the control packet checks are skipped\n");
        return;
    }
    uint16_t port = (uint16_t)(24000 + getpid() % 4000 * 2);
    static const char *const ends[] = {"quit", "kick", "close", "exit"};
    for (int end = END_QUIT; end <= END_EXIT; end++) {
        pid_t h = host(me, port, end);
        hn_sleep(200);
        nc_set_identity("7.7.7", "", "");
        uint32_t c = nc_socket(2, 1);
        CHECK(nc_connect(c, me, port) == 0, "%s: connect: %d", ends[end], nc_error());
        nc_nonblocking(c, 1);
        uint8_t got[16];
        CHECK(read_packet(c, got, sizeof game1) == (int)sizeof game1 && !memcmp(got, game1, sizeof game1),
              "%s: the game reads the host screen with no control bytes", ends[end]);
        CHECK(read_packet(c, got, sizeof game2) == (int)sizeof game2 && !memcmp(got, game2, sizeof game2),
              "%s: a packet sent one byte at a time stays whole: the PINGs waited for its end", ends[end]);
        CHECK(nc_send(c, from_joiner, sizeof from_joiner) == (long)sizeof from_joiner, "%s: the click", ends[end]);
        uint8_t q[5];
        int n = read_packet(c, q, 5);
        CHECK(n == 5 && q[0] == 1 && q[4] == 4 && nc_host_left(), "%s: the client loop gets the quit packet (%d)",
              ends[end], n);
        CHECK(nc_kicked() == (end == END_KICK), "%s: nc_kicked %d", ends[end], nc_kicked());
        CHECK(nc_host_broke() == (end == END_KICK || end == END_CLOSE), "%s: nc_host_broke %d", ends[end],
              nc_host_broke());
        int status;
        waitpid(h, &status, 0);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "%s: the host's checks (exit %d: 14 the click had control "
              "bytes, 15 no ping)", ends[end], WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        nc_close(c);
        port += 2;
    }
    nc_set_identity(NULL, NULL, NULL);
}

/* A host that passes the join check, sends part of a control packet and closes: it stopped in the middle. */
static pid_t broken_host(uint32_t me, uint16_t port)
{
    int l = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET, a.sin_port = htons(port), a.sin_addr.s_addr = me;
    if (bind(l, (struct sockaddr *)&a, sizeof a) || listen(l, 4)) return -1;
    pid_t pid = fork();
    if (pid) {
        close(l);
        return pid;
    }
    int c = accept(l, NULL, NULL);
    char block[256];
    int n = 0;
    while (n < (int)sizeof block - 1 && recv(c, block + n, 1, 0) == 1)
        if (++n >= 2 && block[n - 1] == '\n' && block[n - 2] == '\n') break;
    static const char part[] = "BOSMAN OK\n\x10\0\0\0\xbe" "BOSM";
    send(c, part, sizeof part - 1, 0);
    usleep(300000);
    close(c);
    _exit(0);
}

static void test_broken_control(void)
{
    uint32_t me;
    if (hn_route_address(&me) < 0) return;
    uint16_t port = (uint16_t)(26000 + getpid() % 2000 * 2);
    pid_t h = broken_host(me, port);
    CHECK(h > 0, "broken host");
    hn_sleep(100);
    nc_set_identity("7.7.7", "", "");
    uint32_t c = nc_socket(2, 1);
    CHECK(nc_connect(c, me, port) == 0, "connect to the broken host: %d", nc_error());
    nc_nonblocking(c, 1);
    uint8_t q[5];
    uint64_t t0 = hn_ms();
    int n = 0;
    while (n < 5 && hn_ms() - t0 < 15000) {
        long r = nc_recv(c, q + n, 1);
        if (r == 1) n++;
        else if (r < 0 && nc_error() == HN_WOULDBLOCK) hn_sleep(5);
        else break;
    }
    CHECK(n == 5 && q[4] == 4 && nc_host_broke() && !nc_kicked() && hn_ms() - t0 < NC_CONNECT_MS + 3000,
          "a host that stops in the middle of a control packet: the part is dropped, and the game gets the quit "
          "packet after about NC_CONNECT_MS (%d bytes, %llu ms)", n, (unsigned long long)(hn_ms() - t0));
    nc_close(c);
    waitpid(h, NULL, 0);
    nc_set_identity(NULL, NULL, NULL);
}

/* ---- connect and bind errors ---- */

static void test_connect_errors(void)
{
    uint32_t me;
    if (hn_route_address(&me) < 0) return;
    uint16_t port = (uint16_t)(28000 + getpid() % 2000 * 2);
    uint32_t c = nc_socket(2, 1);
    CHECK(nc_connect(c, me, port) < 0 && nc_connect_error() == 10061 &&
          net_error_of_connect(nc_connect_error()) == NE_HOST_REFUSED, "no game at the address: refused (%d)",
          nc_connect_error());
    nc_close(c);

    nc_set_connect_ms(300);
    c = nc_socket(2, 1);
    uint32_t nowhere = htonl(0xc0000201u);             /* 192.0.2.1, TEST-NET-1: no host answers */
    NetError e = nc_connect(c, nowhere, port) < 0 ? net_error_of_connect(nc_connect_error()) : NE_NONE;
    CHECK(e == NE_CONNECT_TIMEOUT || e == NE_HOST_UNREACHABLE || e == NE_NO_NETWORK,
          "an address with no host: timeout or unreachable (%d)", nc_connect_error());
    nc_close(c);
    nc_set_connect_ms(NC_CONNECT_MS);

    /* Unable To Create: a second listen socket on the same address and port, as a second copy of the game. */
    uint32_t l1 = nc_socket(2, 1), l2 = nc_socket(2, 1);
    nc_setopt(l1, NC_SOL_SOCKET, NC_SO_REUSEADDR, 1, 0);
    nc_setopt(l2, NC_SOL_SOCKET, NC_SO_REUSEADDR, 1, 0);
    nc_bind_error_clear();
    CHECK(nc_bind(l1, me, port) == 0 && nc_listen(l1, 16) == 0 && nc_bind_error() == 0, "the first host's socket");
    CHECK(nc_bind(l2, me, port) < 0 && nc_bind_error() == 10048, "the second copy: port in use (%d)", nc_bind_error());
    nc_close(l1);
    nc_close(l2);
}

/* ---- texts ---- */

static void check_text(const char *t, const char *what)
{
    size_t n = strlen(t);
    CHECK(n > 10 && n < 300, "%s: length %zu", what, n);
    CHECK(!strchr(t, '!') && !strstr(t, "  ") && !strstr(t, "\xe2\x80\x94") && !strchr(t, ';'),
          "%s: no exclamation mark, double space, dash or semicolon: %s", what, t);
    CHECK(t[n - 1] == '.' || strstr(what, "host") || strstr(what, "running"), "%s ends with a full stop: %s", what, t);
}

static void test_texts(void)
{
    char t[400], what[32];
    for (int e = NE_NONE + 1; e < NE_COUNT; e++) {
        net_error_text(t, sizeof t, (NetError)e, "0.7.0", "0.7.1", 10013);
        snprintf(what, sizeof what, "error %d", e);
        check_text(t, what);
    }
    net_error_text(t, sizeof t, NE_VERSION, "0.7.0", "0.7.1", 0);
    CHECK(strstr(t, "runs Bosman Edition 0.7.1, and you have 0.7.0"), "the version text names both: %s", t);
    net_error_text(t, sizeof t, NE_CREATE_OTHER, "0.7.0", "", 10013);
    CHECK(strstr(t, "10013"), "the other create text names the code: %s", t);
    net_error_text(t, sizeof t, NE_CREATE_OTHER, "0.7.0", "", 0);
    CHECK(!strstr(t, "error"), "with no code, no error number: %s", t);
    /* net_errors.c copies the values of LobbyHostState. */
    CHECK(LOBBY_NONE == 0 && LOBBY_REGISTERING == 1 && LOBBY_OPEN == 2 && LOBBY_REFUSED == 3 && LOBBY_FULL_TODAY == 4 &&
          LOBBY_UNAVAILABLE == 5 && LOBBY_CLOSED == 6, "LobbyHostState has the values that net_host_text reads");
    net_error_text(t, sizeof t, NE_FULL_TODAY, "", "", 0);
    CHECK(!strncmp(t, "Online games are full today, try again later", 44), "the spec's full today text (Q12): %s", t);
    net_error_text(t, sizeof t, NE_HOST_LEFT, "", "", 0);
    CHECK(!strncmp(t, "The host left the game", 22), "the spec's host left text (Q7): %s", t);
    for (int r = NT_RUNNING; r < NT_COUNT; r++) {
        net_test_text(t, sizeof t, (NetTest)r, 42);
        snprintf(what, sizeof what, r == NT_RUNNING ? "test running" : "test %d", r);
        check_text(t, what);
    }
    net_test_text(t, sizeof t, NT_WORKS, 42);
    CHECK(strstr(t, "works") && strstr(t, "42 ms"), "the test says that online play works: %s", t);
    for (int s = 1; s <= 5; s++) {
        snprintf(what, sizeof what, "host %d", s);
        CHECK(net_host_text(t, sizeof t, s, "K7M2Q9RT", s == 3 ? "too_many_games" : "") != NULL, "%s has a text", what);
        check_text(t, what);
    }
    net_host_text(t, sizeof t, 2, "K7M2Q9RT", "");
    CHECK(strstr(t, "K7M2Q9RT"), "the open host line shows the game code: %s", t);
    CHECK(!net_host_text(t, sizeof t, 0, "", "") && !net_host_text(t, sizeof t, 6, "", ""), "no line when closed");
    /* The invite link (#137): the page that server/src/join.ts serves. */
    CHECK(net_invite_link(t, sizeof t, "https://online.cm0102bosman.co.uk", "K7M2Q9RT") &&
              !strcmp(t, "https://online.cm0102bosman.co.uk/join/K7M2Q9RT"), "invite link: %s", t);
    CHECK(net_invite_link(t, sizeof t, "http://127.0.0.1:8787//", "K7M2Q9RT") &&
              !strcmp(t, "http://127.0.0.1:8787/join/K7M2Q9RT"), "a / at the end of the address goes: %s", t);
    CHECK(!net_invite_link(t, sizeof t, "https://x.test", "") && !net_invite_link(t, sizeof t, "", "K7M2Q9RT") &&
              !net_invite_link(t, sizeof t, NULL, "K7M2Q9RT") && !net_invite_link(t, 10, "https://x.test", "K7M2Q9RT"),
          "no link with no code, no address or no room");
}

/* ---- game list rows and JSON ---- */

static void test_rows(void)
{
    static LobbyRow rows[2];
    memset(rows, 0, sizeof rows);
    snprintf(rows[0].g.host_name, LOBBY_TEXT, "PC-12345678");
    snprintf(rows[0].g.name, LOBBY_TEXT, "Sunday League");
    snprintf(rows[0].g.day, 11, "2003-10-14");
    rows[0].g.season = 2003, rows[0].g.managers_online = 3, rows[0].g.ping_ms = 80;
    snprintf(rows[1].g.host_name, LOBBY_TEXT, "PC-87654321");
    snprintf(rows[1].g.name, LOBBY_TEXT, "A Very Long League Name For The Game List");
    snprintf(rows[1].g.day, 11, "2003-10-14");
    rows[1].g.season = 2003, rows[1].g.managers_online = 12, rows[1].g.ping_ms = 1230, rows[1].g.away_n = 2;
    rows[1].g.password = 1;
    uint32_t ad[1] = {htonl(0xc0a84401u)}, mk[1] = {htonl(0xffffff00u)};
    lr_set_adapters(ad, mk, 1, ad[0]);
    lr_request();
    lr_lobby(rows, 2, LH_OK);
    hn_sleep(LR_LOBBY_WAIT_MS + 50);
    uint8_t d[LR_DATAGRAM];
    uint32_t a;
    char names[4][50];
    int n = 0;
    while (n < 4 && lr_extra(d, sizeof d, &a) == LR_DATAGRAM) snprintf(names[n++], 50, "%s", (const char *)d + LR_NAME_AT);
    CHECK(n == 2 && !strcmp(names[0], "Sunday League  2003/04 14 Oct  3 online, 80 ms"), "the ping in the row: %s",
          n ? names[0] : "");
    CHECK(n == 2 && strlen(names[1]) <= 49 && strstr(names[1], "12 online, 2 away, password") &&
          !strstr(names[1], " ms") && !strncmp(names[1], "A Very L", 8),
          "a long row drops the day, then the ping: %s", n > 1 ? names[1] : "");

    lr_request();
    lr_lobby(rows, 0, LH_UNAVAILABLE);
    hn_sleep(LR_LOBBY_WAIT_MS + 50);
    n = 0;
    uint32_t addr = 0;
    while (n < 4 && lr_extra(d, sizeof d, &a) == LR_DATAGRAM) {
        snprintf(names[n++], 50, "%s", (const char *)d + LR_NAME_AT);
        addr = a;
    }
    int full = 0;
    CHECK(n == 1 && !strcmp(names[0], LR_DOWN_TEXT) && names[0][0] == ' ' && !lr_row_of(addr, &full) && full == 2,
          "the lobby gives no answer: one row says so, and its Join gives that reason (%d %s)", n, n ? names[0] : "");

    LobbyGame g;
    memset(&g, 0, sizeof g);
    snprintf(g.game_id, sizeof g.game_id, "be-12345678");
    snprintf(g.name, sizeof g.name, "League");
    snprintf(g.host_name, sizeof g.host_name, "PC-0000abcd");
    snprintf(g.version, sizeof g.version, "0.7.0");
    snprintf(g.database, sizeof g.database, "3.9.68");
    snprintf(g.day, sizeof g.day, "2002-08-14");
    g.season = 2002, g.ping_ms = 120;
    char json[4096];
    CHECK(lobby_game_json(json, sizeof json, &g, NULL, NULL, NULL) > 0 && strstr(json, "\"pingMs\":120"),
          "the entry carries pingMs: %s", json);
    LobbyRow r[1];
    CHECK(lobby_parse_games("{\"games\":[{\"code\":\"FP26DG7H\",\"name\":\"L\",\"pingMs\":95}]}", r, 1) == 1 &&
          r[0].g.ping_ms == 95, "the list gives pingMs");
}

/* ---- the connection test ---- */

enum { LOBBY_WORKS, LOBBY_LIST_FAILS, LOBBY_FULL, RELAY_FAILS, RELAY_STRIPPED, RELAY_ANSWERS_101, LOBBY_LIST_SLOW };
#define SLOW_LIST_S 7     /* item 213: a step slower than the old 6 s of the test, faster than LH_TIMEOUT_MS (10 s) */

static const char *reply(const char *code, int status)
{
    static char r[400];
    char body[100];
    snprintf(body, sizeof body, "{\"error\":\"%s\",\"message\":\"x\"}", code);
    snprintf(r, sizeof r, "HTTP/1.1 %d X\r\nX-Bosman-Api: 1\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "\r\n%s", status, strlen(body), body);
    return r;
}

/* A lobby on 127.0.0.1:port. upgrade_seen: a pipe that gets 'u' when the relay request had Upgrade: websocket. */
static pid_t fake_lobby(uint16_t port, int mode, int upgrade_seen)
{
    int l = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET, sa.sin_port = htons(port), sa.sin_addr.s_addr = htonl(0x7f000001u);
    if (bind(l, (struct sockaddr *)&sa, sizeof sa) || listen(l, 8)) return -1;
    pid_t pid = fork();
    if (pid) {
        close(l);
        return pid;
    }
    for (;;) {
        int c = accept(l, NULL, NULL);
        if (c < 0) continue;
        char req[4096];
        long n = recv(c, req, sizeof req - 1, 0);
        req[n > 0 ? n : 0] = 0;
        char path[256] = "";
        sscanf(req, "%*s %255s", path);
        const char *r;
        static const char ok[] = "HTTP/1.1 200 OK\r\nX-Bosman-Api: 1\r\nContent-Length: 11\r\n\r\n{\"ok\":true}";
        static const char list[] = "HTTP/1.1 200 OK\r\nX-Bosman-Api: 1\r\nContent-Length: 12\r\n\r\n{\"games\":[]}";
        if (!strcmp(path, "/v1/health")) r = mode == LOBBY_FULL ? reply("full_today", 503) : ok;
        else if (!strcmp(path, "/v1/games")) {
            if (mode == LOBBY_LIST_SLOW) sleep(SLOW_LIST_S);     /* a slow line: the answer comes late */
            r = mode == LOBBY_LIST_FAILS ? reply("unavailable", 503) : list;
        }
        else if (strstr(path, "/relay?")) {
            if (strstr(req, "Upgrade: websocket\r\n") && strstr(req, "Connection: Upgrade\r\n") && upgrade_seen >= 0)
                if (write(upgrade_seen, "u", 1) != 1) _exit(3);
            r = mode == RELAY_FAILS ? "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n"
                : mode == RELAY_STRIPPED ? reply("upgrade_required", 426)
                : mode == RELAY_ANSWERS_101 ? "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n"
                : reply("unknown_game", 404);
        } else r = reply("not_found", 404);
        send(c, r, strlen(r), 0);
        close(c);
    }
}

static NetTest run_test(int online, const char *base)
{
    net_test_start(online, base);
    NetTest t = NT_RUNNING;
    for (uint64_t until = hn_ms() + 30000; (t = net_test_state(NULL)) == NT_RUNNING && hn_ms() < until;) hn_sleep(10);
    return t;
}

static void test_connection_test(void)
{
    uint16_t port = (uint16_t)(32000 + getpid() % 4000 * 2);
    char base[64];
    snprintf(base, sizeof base, "http://127.0.0.1:%u", port);
    CHECK(net_test_state(NULL) == NT_NONE, "no test before the first");
    CHECK(run_test(0, base) == NT_OFF, "Online play off: the test says so and sends nothing");
    CHECK(run_test(1, "") == NT_NO_ADDRESS, "no lobby address");
    CHECK(run_test(1, base) == NT_LOBBY_DOWN, "the lobby is down: nothing listens");
    static const struct { int mode; NetTest want; const char *what; } cases[] = {
        {LOBBY_WORKS, NT_WORKS, "lobby and relay work"},
        {LOBBY_LIST_FAILS, NT_LOBBY_LIST_DOWN, "the lobby's list fails"},
        {LOBBY_FULL, NT_FULL_TODAY, "full today"},
        {RELAY_FAILS, NT_RELAY_DOWN, "the relay fails"},
        {RELAY_STRIPPED, NT_RELAY_BLOCKED, "a proxy takes the Upgrade header off"},
        {RELAY_ANSWERS_101, NT_RELAY_DOWN, "an answer that is not the relay's"},
        /* Item 213: each step may take as long as the game's own lobby calls (LH_TIMEOUT_MS), so a list that
         * answers after 7 s on a slow line passes, as it does in play. */
        {LOBBY_LIST_SLOW, NT_WORKS, "the lobby's list answers after 7 s"},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int seen[2];
        CHECK(pipe(seen) == 0, "pipe");
        snprintf(base, sizeof base, "http://127.0.0.1:%u", port);
        pid_t lobby = fake_lobby(port, cases[i].mode, seen[1]);
        CHECK(lobby > 0, "fake lobby");
        hn_sleep(100);
        int ms = -1;
        NetTest t = run_test(1, base);
        net_test_state(&ms);
        CHECK(t == cases[i].want, "%s: result %d, want %d", cases[i].what, t, cases[i].want);
        if (cases[i].mode == LOBBY_WORKS) {
            char u = 0;
            CHECK(read(seen[0], &u, 1) == 1 && u == 'u', "the relay request is a WebSocket request");
            CHECK(ms >= 0 && ms < 6000, "the lobby's time: %d ms", ms);
        }
        kill(lobby, SIGKILL);
        waitpid(lobby, NULL, 0);
        close(seen[0]);
        close(seen[1]);
        port += 2;
    }
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    hn_start();
    nc_start();
    test_texts();
    test_rows();
    test_connect_errors();
    test_connection_test();
    test_control();
    test_control_race();
    test_broken_control();
    printf("%s\n", failures ? "net_errors: FAILED" : "net_errors: ok");
    return failures != 0;
}
