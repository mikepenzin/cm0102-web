/* Invite links (0.7.0 slice 8): the link text (invite.c), the answer for one game (lobby.c), and a link that
 * looks up its game in a fake lobby. The page itself is tested in server/test/join.test.ts. */
#include "invite.h"
#include "lobby.h"
#include "lobby_http.h"
#include "lobby_rows.h"
#include "options.h"
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

/* Stubs of the game side. */
static int online_play = 1;
static char address[64];
int options_bool(OptionId id) { return id == OPT_ONLINE_PLAY ? online_play : 0; }
void net_lobby_install(void) { lobby_set_address(address); }
int host_link_handler(int on) { (void)on; return 0; }

#define GAME "{\"game\":{\"code\":\"K7M2Q9RT\",\"name\":\"Sunday \\u00e9 League\",\"hostName\":\"Max\",\"version\":\"0.7.0\"," \
             "\"database\":\"Original 2001/02\",\"season\":2001,\"day\":\"2001-08-11\",\"humanClubs\":[\"Arsenal\"]," \
             "\"freeSeats\":14,\"managersOnline\":2,\"password\":true}}"

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
        char req[4096], path[256] = "", r[1024];
        long n = recv(c, req, sizeof req - 1, 0);
        req[n > 0 ? n : 0] = 0;
        sscanf(req, "%*s %255s", path);
        if (!strcmp(path, "/v1/games/K7M2Q9RT"))
            snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nX-Bosman-Api: 1\r\nContent-Length: %zu\r\n\r\n%s", strlen(GAME), GAME);
        else if (!strcmp(path, "/v1/games/22222222"))       /* an answer for another code */
            snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nX-Bosman-Api: 1\r\nContent-Length: %zu\r\n\r\n%s", strlen(GAME), GAME);
        else
            snprintf(r, sizeof r, "HTTP/1.1 404 Not Found\r\nX-Bosman-Api: 1\r\nContent-Length: 24\r\n\r\n"
                     "{\"error\":\"unknown_game\"}");
        send(c, r, strlen(r), 0);
        close(c);
    }
}

static void test_parse(void)
{
    char code[9];
    static const struct { const char *text; const char *code; } cases[] = {
        {"cm0102://join/K7M2Q9RT", "K7M2Q9RT"},
        {"cm0102://join/K7M2Q9RT/", "K7M2Q9RT"},            /* Windows and some browsers add a "/" */
        {"CM0102://JOIN/k7m2q9rt", "K7M2Q9RT"},
        {"cm0102://join/K7M2Q9R", NULL},                    /* 7 characters */
        {"cm0102://join/K7M2Q9RTX", NULL},
        {"cm0102://join/K7M2Q9R1", NULL},                   /* 1 is not in the alphabet */
        {"cm0102://join/K7M2Q9RO", NULL},                   /* nor O */
        {"cm0102://join/K7M2Q9RT//", NULL},
        {"cm0102://join/K7M2Q9RT?x=1", NULL},
        {"cm0102://join/../K7M2Q9", NULL},
        {"cm0102://host/K7M2Q9RT", NULL},
        {"cm0102:join/K7M2Q9RT", NULL},
        {"http://join/K7M2Q9RT", NULL},
        {"cm0102://join/", NULL},
        {"", NULL},
        {NULL, NULL},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        memset(code, 'x', sizeof code);
        int ok = invite_parse(cases[i].text, code);
        CHECK(ok == (cases[i].code != NULL), "parse %s gives %d", cases[i].text ? cases[i].text : "NULL", ok);
        if (ok && cases[i].code) CHECK(!strcmp(code, cases[i].code), "parse %s gives code %s", cases[i].text, code);
    }
}

static void test_json(void)
{
    LobbyRow r;
    CHECK(lobby_parse_game(GAME, &r) == 1, "the game answer parses");
    CHECK(!strcmp(r.code, "K7M2Q9RT") && !strcmp(r.g.name, "Sunday \xe9 League") && !strcmp(r.g.version, "0.7.0"),
          "the fields: %s %s %s", r.code, r.g.name, r.g.version);
    CHECK(r.g.password == 1 && r.g.clubs_n == 1 && r.g.season == 2001, "password, clubs, season");
    CHECK(lobby_parse_game("{\"game\":{\"name\":\"x\"}}", &r) == 0, "no code: not valid");
    CHECK(lobby_parse_game("{\"error\":\"unknown_game\"}", &r) == 0, "an error: not valid");
    CHECK(lobby_parse_game("{\"game\":", &r) == 0, "cut: not valid");
}

/* Polls as the game's event loop does, for at most ms. */
static void poll_for(int ms)
{
    for (int t = 0; t < ms; t += 10) {
        invite_poll();
        usleep(10000);
    }
}

static void test_lookup(uint16_t port)
{
    snprintf(address, sizeof address, "http://127.0.0.1:%u", port);

    invite_open("cm0102://join/K7M2Q9RT");
    poll_for(1500);
    const LobbyRow *g = invite_last();
    CHECK(g && !strcmp(g->code, "K7M2Q9RT") && !strcmp(g->g.host_name, "Max"), "the link finds its game");

    invite_open("cm0102://join/ABCDEFGH");                 /* not open */
    poll_for(1500);
    CHECK(invite_last() == g && !strcmp(g->code, "K7M2Q9RT"), "a game that is not open is not joined");

    invite_open("cm0102://join/22222222");                 /* the lobby answers with another game */
    poll_for(1500);
    CHECK(invite_last() == g && !strcmp(g->code, "K7M2Q9RT"), "an answer for another code is not joined");

    /* A second link before the first answer: only the second link's answer comes. */
    lobby_game_fetch("K7M2Q9RT");
    lobby_game_fetch("ABCDEFGH");
    int answers = 0, last_result = -1;
    for (int t = 0; t < 1500; t += 10) {
        LobbyRow r;
        int res;
        if (lobby_game_take(&r, &res)) answers++, last_result = res;
        usleep(10000);
    }
    CHECK(answers == 1 && last_result == LH_API_ERROR, "an older answer is dropped: %d answers, result %d", answers,
          last_result);

    online_play = 0;
    address[0] = 0;                                        /* any request would fail the next check */
    invite_open("cm0102://join/K7M2Q9RT");
    int got_row, result;
    LobbyRow row;
    poll_for(300);
    got_row = lobby_game_take(&row, &result);
    CHECK(!got_row, "Online play off: no lookup");
    online_play = 1;
}

/* The walk of a link (invite.c) and the text of the game's row (lobby_rows.c). */
static void test_walk(void)
{
    LobbyRow row;
    memset(&row, 0, sizeof row);
    snprintf(row.code, sizeof row.code, "K7M2Q9RT");
    snprintf(row.g.name, sizeof row.g.name, "Sunday");
    snprintf(row.g.host_name, sizeof row.g.host_name, "PC-12345678");
    snprintf(row.g.version, sizeof row.g.version, "0.7.0");
    snprintf(row.g.day, sizeof row.g.day, "2001-08-11");
    row.g.season = 2001, row.g.managers_online = 1;
    const uint32_t ad[1] = {htonl(0xc0a80105u)}, mk[1] = {htonl(0xffffff00u)};
    lr_set_adapters(ad, mk, 1, ad[0]);
    lr_request();
    char t[50] = "";
    CHECK(!lr_shown_text("K7M2Q9RT", t), "no row before the lobby answers");
    lr_lobby(&row, 1, LH_OK);
    usleep((LR_LOBBY_WAIT_MS + 50) * 1000);
    uint8_t d[LR_DATAGRAM];
    uint32_t from;
    while (lr_extra(d, sizeof d, &from) >= 0) {}
    CHECK(lr_shown_text("K7M2Q9RT", t) && !strncmp(t, "Sunday  2001/02 11 Aug", 22), "the row's text: %s", t);
    CHECK(!lr_shown_text("ZZZZ2222", t), "no row for another code");

    char join[50];
    invite_join(&row);
    CHECK(invite_command(INVITE_SETUP) == 5, "Setup Game: Network Play");
    CHECK(invite_command(INVITE_SETUP) == 5, "Setup Game again, when its input returned before the word");
    CHECK(invite_command(INVITE_NETWORK_PLAY) == 0x13, "Network Play: Join Network Game");
    CHECK(invite_command(INVITE_NETWORK_PLAY) == 0, "Network Play once only");
    CHECK(invite_list_join(join, sizeof join) && !strncmp(join, "Sunday  2001/02", 15), "the list joins: %s", join);
    CHECK(!invite_list_join(join, sizeof join) && !invite_command(INVITE_SETUP), "then the walk ends");

    invite_join(&row);
    for (int i = 0; i < 3; i++) invite_command(INVITE_SETUP);
    CHECK(!invite_command(INVITE_SETUP) && !invite_command(INVITE_NETWORK_PLAY), "Setup Game three times at most");

    invite_join(&row);
    invite_clicked();
    CHECK(!invite_command(INVITE_SETUP) && !invite_list_join(join, sizeof join), "a click of the player ends it");

    LobbyRow other = row;
    snprintf(other.code, sizeof other.code, "ZZZZ2222");
    invite_join(&other);
    CHECK(!invite_list_join(join, sizeof join), "the list waits for the row");
    invite_clicked();
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    hn_start();
    test_parse();
    test_json();
    test_walk();
    uint16_t port = (uint16_t)(42000 + getpid() % 2000);
    pid_t lobby = fake_lobby(port);
    usleep(100000);
    test_lookup(port);
    kill(lobby, SIGKILL);
    waitpid(lobby, NULL, 0);
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("invite: all checks pass\n");
    return 0;
}
