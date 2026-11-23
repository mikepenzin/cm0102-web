/* Online games in the game list (0.7.0 slice 4): the lobby's HTTP rules (lobby_http.c), the JSON of the lobby
 * (lobby.c), the rows of the game list (lobby_rows.c) and the join check of net_core.c. A child process plays
 * a fake lobby or a host. The real lobby's own tests are in server/test. */
#include "lobby.h"
#include "lobby_http.h"
#include "lobby_rows.h"
#include "net_core.h"
#include "host_net.h"

#include <arpa/inet.h>
#include <fcntl.h>
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

/* ---- a fake lobby: one canned answer for each path ---- */

static const char *answer_for(const char *path)
{
    if (!strncmp(path, "/ok", 3))
        return "HTTP/1.1 200 OK\r\nX-Bosman-Api: 1\r\nContent-Length: 11\r\n\r\n{\"ok\":true}";
    if (!strncmp(path, "/refused", 8))
        return "HTTP/1.1 409 Conflict\r\nX-Bosman-Api: 1\r\nContent-Length: 50\r\n\r\n"
               "{\"error\":\"version_mismatch\",\"hostVersion\":\"0.7.1\"}";
    if (!strncmp(path, "/full", 5))
        return "HTTP/1.1 503 Service Unavailable\r\nX-Bosman-Api: 1\r\nContent-Length: 47\r\n\r\n"
               "{\"error\":\"full_today\",\"retryAfterSeconds\":3600}";
    if (!strncmp(path, "/cf429", 6)) return "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 4\r\n\r\nbusy";
    if (!strncmp(path, "/cf1027", 7))
        return "HTTP/1.1 500 Internal\r\nContent-Length: 16\r\n\r\nerror code: 1027";
    if (!strncmp(path, "/ray1027", 8))
        return "HTTP/1.1 500 Internal\r\nContent-Length: 21\r\n\r\nRay ID: 8a1027f0c3e2b";
    if (!strncmp(path, "/cf500", 6)) return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 3\r\n\r\nbad";
    if (!strncmp(path, "/chunked", 8))
        return "HTTP/1.1 200 OK\r\nX-Bosman-Api: 1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\n{\"a\":\r\n2\r\n1}\r\n0\r\n\r\n";
    if (!strncmp(path, "/silent", 7)) return NULL;
    return "HTTP/1.1 404 Not Found\r\nX-Bosman-Api: 1\r\nContent-Length: 21\r\n\r\n{\"error\":\"not_found\"}";
}

static pid_t fake_lobby(uint16_t port)
{
    int l = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET, a.sin_port = htons(port), a.sin_addr.s_addr = htonl(0x7f000001u);
    if (bind(l, (struct sockaddr *)&a, sizeof a) || listen(l, 8)) {
        perror("fake lobby");
        exit(2);
    }
    pid_t pid = fork();
    if (pid) {
        close(l);
        return pid;
    }
    for (;;) {
        int c = accept(l, NULL, NULL);
        if (c < 0) continue;
        if (fork()) {              /* each connection in its own process: an answer may wait */
            close(c);
            while (waitpid(-1, NULL, WNOHANG) > 0) {}
            continue;
        }
        char req[4096];
        long n = recv(c, req, sizeof req - 1, 0);
        req[n > 0 ? n : 0] = 0;
        char path[256] = "";
        sscanf(req, "%*s %255s", path);
        const char *r = answer_for(path);
        if (!r) {
            sleep(30);             /* no answer: the caller's timeout */
            _exit(0);
        }
        send(c, r, strlen(r), 0);
        if (!strncmp(path, "/ok", 3)) sleep(20);  /* the answer ends at its Content-Length, not at the close */
        close(c);
        _exit(0);
    }
}

static void test_http(uint16_t port)
{
    char base[64];
    snprintf(base, sizeof base, "http://127.0.0.1:%u", port);
    LhAnswer *a = malloc(sizeof *a);
    /* The fake lobby holds the connection open 20 s, longer than the 10 s timeout: an answer read to the close
     * would time out and give unavailable. */
    CHECK(lobby_http(base, "GET", "/ok", NULL, NULL, 10000, a) == LH_OK && a->api && a->status == 200 &&
          !strcmp(a->body, "{\"ok\":true}"),
          "a lobby answer, read to its Content-Length while the connection stays open: %s", a->body);
    uint64_t t0;
    CHECK(lobby_http(base, "POST", "/refused", "tok", "{}", 3000, a) == LH_API_ERROR &&
          !strcmp(a->code, "version_mismatch") && strstr(a->body, "\"0.7.1\"}"),
          "an error answer gives its code: %s", a->code);
    CHECK(lobby_http(base, "GET", "/full", NULL, NULL, 3000, a) == LH_FULL_TODAY, "full_today");
    CHECK(lobby_http(base, "GET", "/cf429", NULL, NULL, 3000, a) == LH_UNAVAILABLE,
          "429 without the header (Cloudflare's rate limit): unavailable");
    CHECK(lobby_http(base, "GET", "/cf1027", NULL, NULL, 3000, a) == LH_FULL_TODAY, "error 1027: full");
    CHECK(lobby_http(base, "GET", "/ray1027", NULL, NULL, 3000, a) == LH_UNAVAILABLE, "1027 in a Ray ID: unavailable");
    CHECK(lobby_http(base, "GET", "/cf500", NULL, NULL, 3000, a) == LH_UNAVAILABLE, "another answer: unavailable");
    CHECK(lobby_http(base, "GET", "/chunked", NULL, NULL, 3000, a) == LH_OK && !strcmp(a->body, "{\"a\":1}"),
          "a chunked body: %s", a->body);
    t0 = hn_ms();
    /* The fake lobby holds the connection 30 s: an end before 15 s is the 800 ms timeout, with room for load. */
    CHECK(lobby_http(base, "GET", "/silent", NULL, NULL, 800, a) == LH_UNAVAILABLE && hn_ms() - t0 < 15000,
          "no answer: unavailable within the timeout (%llu ms)", (unsigned long long)(hn_ms() - t0));
    char closed[64];
    snprintf(closed, sizeof closed, "http://127.0.0.1:%u", port + 1);
    t0 = hn_ms();
    /* A refused connection ends at once, long before the 20 s timeout. */
    CHECK(lobby_http(closed, "GET", "/v1/games", NULL, NULL, 20000, a) == LH_UNAVAILABLE && hn_ms() - t0 < 10000,
          "a refused connection: unavailable at once");
    /* https goes over the secure connection (slice 5, tests/net/tls_check.py): a name that does not resolve is
     * unavailable. */
    CHECK(lobby_http("https://example.invalid", "GET", "/", NULL, NULL, 3000, a) == LH_UNAVAILABLE &&
          lobby_http_usable("https://example.invalid") && !lobby_http_usable("ftp://x") && !lobby_http_usable("") &&
          lobby_http_usable(base), "https is usable; a name that does not resolve is unavailable");
    free(a);
}

static void test_json(void)
{
    LobbyGame g;
    memset(&g, 0, sizeof g);
    snprintf(g.game_id, sizeof g.game_id, "be-12345678");
    snprintf(g.name, sizeof g.name, "K\xf6ln \"League\"");          /* Latin-1 ö and quotes */
    snprintf(g.host_name, sizeof g.host_name, "PC-0000abcd");
    snprintf(g.version, sizeof g.version, "0.7.0");
    snprintf(g.database, sizeof g.database, "3.9.68");
    g.season = 2002;
    snprintf(g.day, sizeof g.day, "2002-08-14");
    g.clubs_n = 2;
    snprintf(g.clubs[0], LOBBY_TEXT, "M\xe1laga");
    snprintf(g.clubs[1], LOBBY_TEXT, "Arsenal");
    g.free_seats = 14, g.managers_online = 2;
    g.away_n = 1;
    snprintf(g.away[0], LOBBY_TEXT, "Arsenal");
    char json[4096];
    CHECK(lobby_game_json(json, sizeof json, &g, "pw", NULL, NULL) > 0 &&
          strstr(json, "\"name\":\"K\\u00f6ln \\\"League\\\"\"") && strstr(json, "\"password\":\"pw\"") &&
          strstr(json, "\"humanClubs\":[\"M\\u00e1laga\",\"Arsenal\"]") && strstr(json, "\"season\":2002") &&
          strstr(json, "\"awayClubs\":[\"Arsenal\"]") && !strstr(json, "hostToken"), "registration JSON: %s", json);

    /* The list answer, with UTF-8 text as the lobby sends it. */
    const char *list = "{\"games\":[{\"code\":\"FP26DG7H\",\"name\":\"K\xc3\xb6ln\",\"hostName\":\"PC-0000abcd\","
                       "\"version\":\"0.7.0\",\"database\":\"3.9.68\",\"season\":2002,\"day\":\"2002-08-14\","
                       "\"humanClubs\":[\"Arsenal\",\"Lyon\"],\"awayClubs\":[\"Lyon\"],\"freeSeats\":14,\"managersOnline\":2,"
                       "\"password\":true,\"extra\":{\"a\":[1,2]}},{\"code\":\"B\",\"name\":\"xĀ\"}],"
                       "\"heartbeatSeconds\":30}";
    LobbyRow rows[4];
    int n = lobby_parse_games(list, rows, 4);
    CHECK(n == 2 && !strcmp(rows[0].code, "FP26DG7H") && !strcmp(rows[0].g.name, "K\xf6ln") &&
          rows[0].g.season == 2002 && rows[0].g.managers_online == 2 && rows[0].g.password &&
          rows[0].g.clubs_n == 2 && !strcmp(rows[0].g.clubs[1], "Lyon") &&
          rows[0].g.away_n == 1 && !strcmp(rows[0].g.away[0], "Lyon") && !strcmp(rows[1].g.name, "x?"),
          "list parse: n=%d name=%s", n, n > 0 ? rows[0].g.name : "");
    CHECK(lobby_parse_games("{\"games\":[{\"code\":", rows, 4) < 0, "a cut answer is refused");
    static char deep[40000];
    memcpy(deep, "{\"x\":", 5);
    memset(deep + 5, '[', 30000);
    CHECK(lobby_parse_games(deep, rows, 4) < 0, "a deep answer is refused, not a stack overflow");

    char clean[LOBBY_TEXT];
    lobby_clean(clean, sizeof clean, "  a\x81" "b\tc  ");
    CHECK(!strcmp(clean, "a?b?c"), "clean: %s", clean);
}

/* ---- game list rows ---- */

static uint32_t ip(int a, int b, int c, int d) { return htonl((uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d); }

static void ack(uint8_t *d, const char *host, const char *name)
{
    memset(d, 0, LR_DATAGRAM);
    memcpy(d, "CM3 LIST SERVER ACK", 19);
    snprintf((char *)d + LR_HOST_AT, 50, "%s", host);
    snprintf((char *)d + LR_NAME_AT, 50, "%s", name);
}

static int drain(char names[][50], uint32_t *addrs, int max)
{
    uint8_t d[LR_DATAGRAM];
    uint32_t a;
    int n = 0;
    while (n < max && lr_extra(d, sizeof d, &a) == LR_DATAGRAM) {
        snprintf(names[n], 50, "%s%s", lr_is(d, LR_DATAGRAM, "CM3 LIST SERVER BYE") ? "BYE " : "",
                 (const char *)d + (lr_is(d, LR_DATAGRAM, "CM3 LIST SERVER BYE") ? LR_HOST_AT : LR_NAME_AT));
        addrs[n++] = a;
    }
    return n;
}

static void test_rows(void)
{
    /* This computer: en0 192.168.68.106/24 (the default route) and a virtual adapter 10.211.55.2/24. */
    uint32_t ad[2] = {ip(10, 211, 55, 2), ip(192, 168, 68, 106)}, mk[2] = {ip(255, 255, 255, 0), ip(255, 255, 255, 0)};
    lr_set_adapters(ad, mk, 2, ad[1]);
    uint8_t d[LR_DATAGRAM], bye[LR_DATAGRAM];
    char names[16][50];
    uint32_t addrs[16];

    /* One host, two adapters: the default route's subnet goes at once, the other is dropped. */
    lr_request();
    ack(d, "maxs-mac.local", "Bosman");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(10, 211, 55, 2)) == 0, "the other subnet's ACK waits");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 110)) == 1, "the default route's ACK goes at once");
    hn_sleep(LR_HOLD_MS + 50);
    CHECK(drain(names, addrs, 16) == 0, "the waiting ACK of the same host is dropped");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 110)) == 1, "the game drops its own repeat");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(10, 211, 55, 2)) == 0, "a later ACK of another adapter is dropped");
    memset(bye, 0, sizeof bye);
    memcpy(bye, "CM3 LIST SERVER BYE", 19);
    CHECK(lr_arrived(bye, LR_DATAGRAM, ip(10, 211, 55, 2)) == 0, "a BYE from an address with no row is dropped");
    CHECK(lr_arrived(bye, LR_DATAGRAM, ip(192, 168, 68, 110)) == 1, "the BYE of the row goes to the game");

    /* A host seen only on another subnet goes after the hold. */
    lr_request();
    ack(d, "pc-two", "Second");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(10, 211, 55, 9)) == 0, "held");
    CHECK(drain(names, addrs, 16) == 0, "not before the hold");
    hn_sleep(LR_HOLD_MS + 50);
    CHECK(drain(names, addrs, 16) == 1 && !strcmp(names[0], "Second") && addrs[0] == ip(10, 211, 55, 9),
          "after the hold");

    /* A host with Wi-Fi and Ethernet in one subnet answers twice: one row. */
    lr_request();
    ack(d, "MacBook-Pro", "Bosman");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 20)) == 1 && lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 21)) == 0,
          "two adapters of a host in one subnet");

    /* The cap of the game (0x77b0eb): a row past it is not counted, and its BYE does not reach the game. */
    lr_request();
    for (int i = 0; i < LR_CAP; i++) {
        char name[8];
        snprintf(name, sizeof name, "G%d", i);
        ack(d, "pc", name);
        CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 30 + i)) == 1, "row %d", i);
    }
    ack(d, "pc", "Past");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 99)) == 0, "past the cap");
    CHECK(lr_arrived(bye, LR_DATAGRAM, ip(192, 168, 68, 99)) == 0, "the BYE of a row past the cap");
    LobbyRow one[1];
    memset(one, 0, sizeof one);
    snprintf(one[0].g.host_name, LOBBY_TEXT, "PC-99999999");
    snprintf(one[0].g.name, LOBBY_TEXT, "Online");
    lr_lobby(one, 1, LH_OK);
    hn_sleep(LR_LOBBY_WAIT_MS);
    CHECK(drain(names, addrs, 16) == 0, "no lobby row past the cap");

    /* Lobby rows: the LAN game is not shown twice, another game is, then the full-today row. */
    lr_request();
    ack(d, "maxs-mac.local", "Bosman");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 110)) == 1, "LAN row");
    LobbyRow rows[2];
    memset(rows, 0, sizeof rows);
    lr_host_key(rows[0].g.host_name, "maxs-mac.local");
    snprintf(rows[0].g.name, LOBBY_TEXT, "Bosman");
    snprintf(rows[0].g.version, 25, "0.7.0");
    snprintf(rows[1].g.host_name, LOBBY_TEXT, "PC-12345678");
    snprintf(rows[1].g.name, LOBBY_TEXT, "Sunday League With A Long Name For The List");
    snprintf(rows[1].g.version, 25, "0.7.1");
    snprintf(rows[1].g.day, 11, "2003-10-14");
    rows[1].g.season = 2003, rows[1].g.managers_online = 3, rows[1].g.password = 1;
    rows[1].g.away_n = 1;                                            /* slice 6: one human manager is Away */
    lr_lobby(rows, 2, LH_OK);
    CHECK(drain(names, addrs, 16) == 0, "the lobby rows wait for the LAN answers");
    hn_sleep(LR_LOBBY_WAIT_MS);
    int n = drain(names, addrs, 16);
    CHECK(n == 1 && strstr(names[0], "  2003/04  3 online, 1 away, password") && strlen(names[0]) <= 49 &&
          !strncmp(names[0], "Sunday Leagu", 12), "one lobby row: %d %s", n, n ? names[0] : "");
    int full;
    const LobbyRow *r = n ? lr_row_of(addrs[0], &full) : NULL;
    CHECK(r && !full && !strcmp(r->g.version, "0.7.1") && ((const uint8_t *)&addrs[0])[0] == 0,
          "the row's address is in 0.66.0.0/16 and gives its game");
    CHECK(!lr_row_of(ip(192, 168, 68, 110), &full) && !full, "a LAN address is not a lobby row");

    /* A LAN ACK of a game that is already a lobby row: the lobby row leaves, the LAN row comes. */
    lr_request();
    lr_lobby(rows, 1, LH_FULL_TODAY);
    hn_sleep(LR_LOBBY_WAIT_MS);
    n = drain(names, addrs, 16);
    CHECK(n == 2 && !strcmp(names[0], LR_FULL_TEXT) && names[0][0] == ' ' && !strncmp(names[1], "Bosman  ", 8),
          "full row first, then the lobby row");
    lr_row_of(addrs[0], &full);
    CHECK(full, "the full row");
    ack(d, "maxs-mac.local", "Bosman");
    CHECK(lr_arrived(d, LR_DATAGRAM, ip(192, 168, 68, 110)) == 0, "the LAN ACK waits for the BYE");
    n = drain(names, addrs, 16);
    CHECK(n == 2 && !strncmp(names[0], "BYE PC-", 7) && addrs[0] != ip(192, 168, 68, 110) &&
          !strcmp(names[1], "Bosman") && addrs[1] == ip(192, 168, 68, 110), "BYE of the lobby row, then the LAN row");

    /* Two lobby games whose long names give the same row text get different texts. */
    lr_request();
    LobbyRow two[2];
    memset(two, 0, sizeof two);
    for (int i = 0; i < 2; i++) {
        snprintf(two[i].g.host_name, LOBBY_TEXT, "PC-0000000%d", i);
        snprintf(two[i].g.name, LOBBY_TEXT, "The Very Long League Name Of Season %d", i);
        snprintf(two[i].g.day, 11, "2003-10-14");
        two[i].g.season = 2003;
    }
    lr_lobby(two, 2, LH_OK);
    hn_sleep(LR_LOBBY_WAIT_MS);
    n = drain(names, addrs, 16);
    CHECK(n == 2 && strcmp(names[0], names[1]) && strlen(names[1]) <= 49, "two texts: %s | %s", names[0], names[1]);
}

/* ---- the join check ---- */

/* A host. With go >= 0 its game calls no accept, only other calls (a full game), until a byte comes on go. */
static pid_t host_late(uint16_t port, const char *version, const char *password, int go)
{
    pid_t pid = fork();
    if (pid) return pid;
    nc_set_identity(version, password, "");
    nc_start();
    uint32_t l = nc_socket(2, 1);
    nc_setopt(l, NC_SOL_SOCKET, NC_SO_REUSEADDR, 1, 0);
    nc_nonblocking(l, 1);
    uint32_t me;
    if (hn_route_address(&me) < 0) _exit(3);
    if (nc_bind(l, me, port) < 0 || nc_listen(l, 16) < 0) _exit(4);
    char byte;
    uint64_t limit = hn_ms() + 60000;                   /* a safety end only: the order below decides */
    while (go >= 0 && read(go, &byte, 1) != 1) {
        if (hn_ms() > limit) _exit(5);
        nc_nonblocking(l, 1);                           /* any call of the game: the runtime answers the joiner */
        hn_sleep(5);
    }
    uint64_t until = hn_ms() + 6000;
    while (hn_ms() < until) {
        uint32_t a = nc_accept(l, NULL, NULL);
        if (a != NC_INVALID) {
            nc_send(a, "\4\0\0\0game", 8);            /* the game's first bytes follow the OK */
            hn_sleep(300);
            _exit(0);
        }
        hn_sleep(5);
    }
    _exit(1);
}

static pid_t host(uint16_t port, const char *version, const char *password)
{
    return host_late(port, version, password, -1);
}

/* A host that takes the connection and never answers, as the original's own WinSock code does. */
static pid_t silent_host(uint16_t port)
{
    uint32_t me;
    hn_route_address(&me);
    pid_t pid = fork();
    if (pid) return pid;
    int l = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET, a.sin_port = htons(port), a.sin_addr.s_addr = me;
    if (bind(l, (struct sockaddr *)&a, sizeof a) || listen(l, 4)) _exit(2);
    int c = accept(l, NULL, NULL);
    sleep(NC_HELLO_WAIT_MS / 1000 + 30);      /* far past the joiner's wait: its end is the wait, not the close */
    close(c);
    _exit(0);
}

static int join(uint16_t port, const char *version, const char *password, char *got, int room)
{
    nc_set_identity(version, "", password);
    uint32_t me;
    hn_route_address(&me);
    uint32_t c = nc_socket(2, 1);
    int r = nc_connect(c, me, port);
    got[0] = 0;
    if (r == 0) {
        nc_nonblocking(c, 1);
        long n = -1;
        for (int i = 0; i < 200 && n < 0; i++, hn_sleep(5)) n = nc_recv(c, got, (uint32_t)room - 1);
        got[n > 0 ? n : 0] = 0;
    }
    nc_close(c);
    return r;
}

static void test_join(uint16_t port)
{
    uint32_t me;
    CHECK(hn_route_address(&me) == 0, "the join check needs a default route");
    if (hn_route_address(&me) < 0) return;
    char got[64], v[32];
    int status;
    pid_t h = host(port, "0.7.0", "");
    hn_sleep(200);
    CHECK(join(port, "0.7.0", "", got, sizeof got) == 0 && !memcmp(got, "\4\0\0\0game", 8) &&
          nc_refusal(v, sizeof v) == NC_REFUSED_NONE, "same version: joined, the game's bytes after the OK");
    waitpid(h, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the host accepted");

    h = host(port, "0.7.1", "");
    hn_sleep(200);
    CHECK(join(port, "0.7.0", "", got, sizeof got) < 0 && nc_refusal(v, sizeof v) == NC_REFUSED_VERSION &&
          !strcmp(v, "0.7.1") && nc_error() == 10061, "another version: refused with the host's version (%s)", v);
    waitpid(h, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 1, "the host's game never saw the joiner");

    h = host(port, "0.7.0", "secret");
    hn_sleep(200);
    CHECK(join(port, "0.7.0", "guess", got, sizeof got) < 0 && nc_refusal(NULL, 0) == NC_REFUSED_PASSWORD,
          "a wrong password: refused");
    waitpid(h, &status, 0);
    h = host(port, "0.7.0", "secret");
    hn_sleep(200);
    CHECK(join(port, "0.7.0", "secret", got, sizeof got) == 0, "the right password: joined");
    waitpid(h, &status, 0);

    /* The host's game calls no accept until this side says so, after the join: so the OK came while the host's
     * game did not accept. An order of events, not a time (a time bound failed once under load). */
    int go[2];
    CHECK(pipe(go) == 0, "pipe");
    fcntl(go[0], F_SETFL, O_NONBLOCK);
    h = host_late(port, "0.7.0", "", go[0]);
    hn_sleep(200);
    CHECK(join(port, "0.7.0", "", got, sizeof got) == 0 && nc_refusal(NULL, 0) == NC_REFUSED_NONE,
          "the answer comes while the host's game does not accept");
    CHECK(write(go[1], "g", 1) == 1, "go");
    waitpid(h, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the host's game accepted the joiner after the go");
    close(go[0]);
    close(go[1]);

    /* No answer: refused with its own reason within NC_HELLO_WAIT_MS. */
    h = silent_host(port);
    hn_sleep(200);
    uint64_t t0 = hn_ms();
    CHECK(join(port, "0.7.0", "", got, sizeof got) < 0 && nc_refusal(NULL, 0) == NC_REFUSED_NO_ANSWER &&
          hn_ms() - t0 < NC_HELLO_WAIT_MS + 15000, "no answer: refused as no answer");
    kill(h, SIGKILL);
    waitpid(h, &status, 0);
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    uint16_t port = (uint16_t)(30000 + getpid() % 10000 * 2);
    pid_t lobby = fake_lobby(port);
    hn_sleep(100);
    test_http(port);
    kill(lobby, SIGKILL);
    waitpid(lobby, NULL, 0);
    test_json();
    test_rows();
    test_join((uint16_t)(port + 7));
    if (failures) printf("%d failures\n", failures);
    else printf("lobby: all checks pass\n");
    return failures != 0;
}
