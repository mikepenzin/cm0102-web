/* Test of the engine worker protocol (src/recomp_rt/win/worker_proto.c): the parse of commands, the JSON lines,
 * the serial days and the line reader over a socket pair.
 * Usage: worker_test */
#include "worker_proto.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void parse_cases(void)
{
    WorkerCommand c;
    CHECK(worker_parse("state", &c) == WK_STATE);
    CHECK(worker_parse("  quit \r", &c) == WK_BAD);          /* the reader drops '\r' only before the newline */
    CHECK(worker_parse("  quit  ", &c) == WK_QUIT);
    CHECK(worker_parse("advance 7", &c) == WK_ADVANCE && c.days == 7);
    CHECK(worker_parse("advance   400", &c) == WK_ADVANCE && c.days == 400);
    CHECK(worker_parse("advance 401", &c) == WK_BAD);
    CHECK(worker_parse("advance 0", &c) == WK_BAD);
    CHECK(worker_parse("advance", &c) == WK_BAD);
    CHECK(worker_parse("advance -3", &c) == WK_BAD);
    CHECK(worker_parse("advance 99999999999999999999", &c) == WK_BAD);
    CHECK(worker_parse("advanced 3", &c) == WK_BAD && strcmp(c.error, "unknown command") == 0);
    CHECK(worker_parse("save Arsenal 2001-07-14", &c) == WK_SAVE && strcmp(c.name, "Arsenal 2001-07-14") == 0);
    CHECK(worker_parse("save  game_1.v2 ", &c) == WK_SAVE && strcmp(c.name, "game_1.v2") == 0);
    CHECK(worker_parse("save ../x", &c) == WK_BAD);            /* '/' is not allowed: no path out of the folder */
    CHECK(worker_parse("save .hidden", &c) == WK_BAD);
    CHECK(worker_parse("save a\\b", &c) == WK_BAD);
    CHECK(worker_parse("save", &c) == WK_BAD);
    char longname[80];
    memset(longname, 'a', sizeof longname);
    memcpy(longname, "save ", 5);
    longname[5 + WORKER_NAME_MAX] = 0;
    CHECK(worker_parse(longname, &c) == WK_SAVE);
    longname[5 + WORKER_NAME_MAX] = 'a';
    longname[6 + WORKER_NAME_MAX] = 0;
    CHECK(worker_parse(longname, &c) == WK_BAD);
    CHECK(worker_parse("", &c) == WK_BAD);
    CHECK(worker_parse("statex", &c) == WK_BAD);
}

static void json_cases(void)
{
    char out[256];
    WorkerState st = {2001, 7, 14, "Arsenal"};
    CHECK(worker_json_state(out, sizeof out, 1, &st) > 0);
    CHECK(strcmp(out, "{\"event\":\"turn\",\"year\":2001,\"month\":7,\"day\":14,\"club\":\"Arsenal\"}\n") == 0);
    st.club = NULL;
    worker_json_state(out, sizeof out, 0, &st);
    CHECK(strcmp(out, "{\"ok\":true,\"year\":2001,\"month\":7,\"day\":14,\"club\":null}\n") == 0);
    st.club = "1. FC K\xc3\xb6ln \"B\"\\\x01";             /* UTF-8 stays, quote, backslash and control escaped */
    worker_json_state(out, sizeof out, 0, &st);
    CHECK(strstr(out, "\"club\":\"1. FC K\xc3\xb6ln \\\"B\\\"\\\\\\u0001\"}\n") != NULL);
    CHECK(worker_json_error(out, sizeof out, "unknown command") > 0);
    CHECK(strcmp(out, "{\"ok\":false,\"error\":\"unknown command\"}\n") == 0);
    CHECK(worker_json_ok(out, sizeof out) == 12 && strcmp(out, "{\"ok\":true}\n") == 0);
    worker_json_saved(out, sizeof out, "/srv/games/7/Test.sav");
    CHECK(strcmp(out, "{\"ok\":true,\"file\":\"/srv/games/7/Test.sav\"}\n") == 0);
    worker_json_until(out, sizeof out, 570);
    CHECK(strcmp(out, "{\"ok\":true,\"until\":570}\n") == 0);
    CHECK(worker_json_ok(out, 5) == -1);                      /* too small: -1, never a cut line */
    CHECK(worker_json_ok(out, 12) == -1);
    CHECK(worker_json_ok(out, 13) == 12);
}

static void serial_cases(void)
{
    CHECK(worker_serial(0, 2000) == 0);
    CHECK(worker_serial(194, 2001) == 366 + 194);             /* 14.7.2001: 2000 is a leap year */
    CHECK(worker_serial(0, 2001) - worker_serial(365, 2000) == 1);
    CHECK(worker_serial(0, 2005) == 366 + 365 + 365 + 365 + 366);
    CHECK(worker_serial(364, 1999) == -1);
}

static void link_cases(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { perror("socketpair"); failures++; return; }
    WorkerLink link = {.fd = sv[0]};
    char line[WORKER_LINE_MAX + 1];
    const char *two = "state\r\nadvance 3\n";
    CHECK(worker_write_all(sv[1], two, strlen(two)) == 0);
    CHECK(worker_read_line(&link, line, sizeof line) == 5 && strcmp(line, "state") == 0);
    CHECK(worker_read_line(&link, line, sizeof line) == 9 && strcmp(line, "advance 3") == 0);
    /* A line in two writes. */
    CHECK(worker_write_all(sv[1], "sa", 2) == 0);
    CHECK(worker_write_all(sv[1], "ve x\n", 5) == 0);
    CHECK(worker_read_line(&link, line, sizeof line) == 6 && strcmp(line, "save x") == 0);
    /* The worker's answer reaches the server end. */
    char out[64], back[64];
    int n = worker_json_ok(out, sizeof out);
    CHECK(worker_write_all(sv[0], out, (size_t)n) == 0);
    CHECK(read(sv[1], back, sizeof back) == n && memcmp(back, out, (size_t)n) == 0);
    /* A line that is too long ends the link. */
    char big[WORKER_LINE_MAX + 10];
    memset(big, 'x', sizeof big);
    CHECK(worker_write_all(sv[1], big, sizeof big) == 0);
    CHECK(worker_read_line(&link, line, sizeof line) == -1);
    close(sv[1]);
    /* The other end closed: -1. */
    WorkerLink fresh = {.fd = sv[0]};
    CHECK(worker_read_line(&fresh, line, sizeof line) == -1);
    close(sv[0]);
}

int main(void)
{
    parse_cases();
    json_cases();
    serial_cases();
    link_cases();
    if (failures) { printf("%d failures\n", failures); return 1; }
    printf("worker protocol: all checks passed\n");
    return 0;
}
