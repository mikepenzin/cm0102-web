/* The connection test (0.7.0 slice 7, #45, docs/specs/2026-10-27-online-play-0.7.0-spec.md): the Test button on the
 * Network tab of the Options box (options_screen.c). It checks, in order, on a thread of its own so that the game
 * never waits:
 * 1. GET /v1/health: the Worker answers (server/README.md, "Endpoints"). It uses no Durable Object.
 * 2. GET /v1/games: the lobby answers with its list.
 * 3. The relay: a WebSocket request (GET with Upgrade: websocket) for the relay of a game code that no game has.
 *    The Worker passes it to the relay object only when the Upgrade header came through (server/src/index.ts,
 *    case "relay"), and the relay answers unknown_game (no key stored, server/src/relay.ts) or, for a game that
 *    has the code, bad_ticket. Either answer says that the relay works. upgrade_required says that something on
 *    the way took the Upgrade header off: a proxy or a firewall that would stop every relay connection. The
 *    request stores nothing on the server.
 * Each step uses lobby_http (lobby_http.h), so it goes over the same connection as the game's own lobby calls. */
#include "net_errors.h"
#include "lobby_http.h"
#include "../host_net.h"

#include <stdio.h>
#include <string.h>

/* Each step may take as long as the game's own lobby calls (LH_TIMEOUT_MS, lobby_http.h), so that the test does not
 * fail where play goes on (item 213: one try on a 4G line failed its second step at 6 s, after a stall). */
#define TEST_MS LH_TIMEOUT_MS
#define PROBE_PATH "/v1/games/22222222/relay?role=manager&ticket=connection-test"

static struct {
    int running;
    NetTest result;
    int ms;
    char base[256];
} st;                                   /* under hn_lock */

static void finish(NetTest t, int ms)
{
    hn_lock();
    st.result = t, st.ms = ms, st.running = 0;
    hn_unlock();
    char line[200];
    net_test_text(line, sizeof line, t, ms);
    fprintf(stderr, "recomp: connection test: %s\n", line);
}

static void test_thread(void *unused)
{
    (void)unused;
    static LhAnswer a;                  /* this thread's only: one test runs at a time */
    char base[256];
    hn_lock();
    memcpy(base, st.base, sizeof base);
    hn_unlock();

    uint64_t t0 = hn_ms();
    LhResult r = lobby_http(base, "GET", "/v1/health", NULL, NULL, TEST_MS, &a);
    int ms = (int)(hn_ms() - t0);
    if (r == LH_FULL_TODAY) return finish(NT_FULL_TODAY, 0);
    if (r != LH_OK) return finish(NT_LOBBY_DOWN, 0);

    r = lobby_http(base, "GET", "/v1/games", NULL, NULL, TEST_MS, &a);
    if (r == LH_FULL_TODAY) return finish(NT_FULL_TODAY, 0);
    if (r != LH_OK) return finish(NT_LOBBY_LIST_DOWN, 0);

    r = lobby_http(base, "UPGRADE", PROBE_PATH, NULL, NULL, TEST_MS, &a);
    if (r == LH_FULL_TODAY) return finish(NT_FULL_TODAY, 0);
    if (r == LH_API_ERROR && (!strcmp(a.code, "unknown_game") || !strcmp(a.code, "bad_ticket") ||
                              !strcmp(a.code, "host_not_connected")))
        return finish(NT_WORKS, ms);
    if (r == LH_API_ERROR && !strcmp(a.code, "upgrade_required")) return finish(NT_RELAY_BLOCKED, 0);
    fprintf(stderr, "recomp: connection test: the relay gave %d %s\n", a.status, a.code);
    finish(NT_RELAY_DOWN, 0);
}

void net_test_start(int online, const char *base)
{
    hn_lock();
    int busy = st.running;
    hn_unlock();
    if (busy) return;
    if (!online) return finish(NT_OFF, 0);
    if (!base || !*base || !lobby_http_usable(base)) return finish(NT_NO_ADDRESS, 0);
    hn_lock();
    st.running = 1;
    st.result = NT_RUNNING;
    snprintf(st.base, sizeof st.base, "%s", base);
    hn_unlock();
    fprintf(stderr, "recomp: connection test: starts\n");
    if (hn_thread(test_thread, NULL) != 0) finish(NT_LOBBY_DOWN, 0);
}

NetTest net_test_state(int *ms)
{
    hn_lock();
    NetTest t = st.result;
    if (ms) *ms = st.ms;
    hn_unlock();
    return t;
}
