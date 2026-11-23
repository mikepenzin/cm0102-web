/* The rows of the game list (lobby_rows.h). */
#include "lobby_rows.h"
#include "lobby_http.h"
#include "../host_net.h"

#include <stdio.h>
#include <string.h>

#define GAMES 256
#define QUEUE 256

typedef struct {
    char key_host[12];               /* lr_host_key of the computer name, or the lobby's hostName */
    char key_name[LOBBY_TEXT];       /* the game name as the lobby keeps it (lobby_clean, 40 characters) */
    uint32_t addr;                   /* network order */
    int rank;                        /* LAN: how near the sender is (rank) */
    int lobby;                       /* a lobby row */
    int delivered;                   /* the game has the row */
    int over;                        /* past the cap: never delivered */
    uint64_t seen;
    uint8_t d[LR_DATAGRAM];          /* LAN, held: the ACK */
    char text[50];                   /* a lobby row: its text in the list */
} Game;

static Game games[GAMES];
static int games_n;
static struct {
    uint8_t d[LR_DATAGRAM];
    uint32_t addr;
} queue[QUEUE];
static int q_head, q_n;

static LobbyRow rows[LOBBY_ROWS];
static int rows_n, lobby_result, lobby_waiting, full_shown;
static uint64_t asked_at;

static uint32_t ad[32], mk[32], route;
static int ad_n, fixed_adapters;

int lr_is(const uint8_t *p, long n, const char *tag)
{
    return n >= LR_TAG_LEN && !memcmp(p, tag, LR_TAG_LEN);
}

static uint32_t to_host(uint32_t net)
{
    const uint8_t *b = (const uint8_t *)&net;
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}

static uint32_t to_net(uint32_t host)
{
    uint32_t net;
    uint8_t *b = (uint8_t *)&net;
    b[0] = (uint8_t)(host >> 24), b[1] = (uint8_t)(host >> 16), b[2] = (uint8_t)(host >> 8), b[3] = (uint8_t)host;
    return net;
}

void lr_host_key(char out[12], const char *computer_name)
{
    uint32_t h = 2166136261u;                          /* FNV-1a */
    for (const unsigned char *p = (const unsigned char *)computer_name; *p; p++) h = (h ^ *p) * 16777619u;
    snprintf(out, 12, "PC-%08x", h);
}

/* A text field of n bytes of a datagram, with an end. */
static void field(char *out, const uint8_t *p, int n)
{
    memcpy(out, p, (size_t)n);
    out[n] = 0;
}

static void key_of_ack(const uint8_t *p, char host[12], char name[LOBBY_TEXT])
{
    char t[51];
    field(t, p + LR_HOST_AT, 50);
    lr_host_key(host, t);
    field(t, p + LR_NAME_AT, 50);
    lobby_clean(name, LOBBY_TEXT, t);
}

void lr_set_adapters(const uint32_t *a, const uint32_t *m, int n, uint32_t r)
{
    fixed_adapters = 1;
    ad_n = n < 32 ? n : 32;
    memcpy(ad, a, (size_t)ad_n * 4);
    memcpy(mk, m, (size_t)ad_n * 4);
    route = r;
}

/* 0 for the subnet of the default route, 1 + its place for another adapter's subnet, 99 for none. */
static int rank(uint32_t addr)
{
    int best = 99;
    for (int i = 0; i < ad_n; i++)
        if (mk[i] && (addr & mk[i]) == (ad[i] & mk[i])) {
            int r = ad[i] == route ? 0 : i + 1;
            if (r < best) best = r;
        }
    return best;
}

static void push(const uint8_t *d, uint32_t addr)
{
    if (q_n == QUEUE) return;
    int at = (q_head + q_n++) % QUEUE;
    memcpy(queue[at].d, d, LR_DATAGRAM);
    queue[at].addr = addr;
}

/* A BYE from addr, as Network::~Network 0x77a890 sends it: the tag and the host name. */
static void push_bye(uint32_t addr, const char *host)
{
    uint8_t d[LR_DATAGRAM];
    memset(d, 0, sizeof d);
    memcpy(d, "CM3 LIST SERVER BYE", LR_TAG_LEN);
    snprintf((char *)d + LR_HOST_AT, 50, "%s", host);
    push(d, addr);
}

static Game *find(const char *host, const char *name)
{
    for (int i = 0; i < games_n; i++)
        if (!strcmp(games[i].key_host, host) && !strcmp(games[i].key_name, name)) return &games[i];
    return NULL;
}

static void drop(Game *g) { *g = games[--games_n]; }

/* The rows that the game has now. It takes no ACK when it has LR_CAP (0x77b0eb), so a row past the cap is never
 * delivered, and its BYE is dropped as one from an address with no row. */
static int shown(void)
{
    int n = full_shown;
    for (int i = 0; i < games_n; i++) n += games[i].delivered;
    return n;
}

/* The LAN row of this game. A host with two adapters, also two in one subnet (Wi-Fi and Ethernet), answers
 * from each, so the computer name and the game name alone name the row. Two computers with the same name that
 * host games with the same name on one LAN show as one row: a rare case, and the cost of the common one. */
static Game *find_lan(const char *host, const char *name)
{
    for (int i = 0; i < games_n; i++) {
        Game *g = &games[i];
        if (!g->lobby && !strcmp(g->key_host, host) && !strcmp(g->key_name, name)) return g;
    }
    return NULL;
}

/* A LAN row goes to the game. A lobby row of the same game leaves first, so the ACK then waits in the queue
 * behind its BYE. 1 when the caller gives the ACK to the game now. */
static int deliver_lan(Game *g, const uint8_t *d, uint32_t addr, int now)
{
    for (int i = 0; i < games_n; i++) {
        Game *o = &games[i];
        if (o != g && o->lobby && o->delivered && !strcmp(o->key_host, g->key_host) &&
            !strcmp(o->key_name, g->key_name)) {
            push_bye(o->addr, o->key_host);
            drop(o);
            if (g == &games[games_n]) g = o;           /* drop moved g into the freed place */
            now = 0;
            break;
        }
    }
    g->lobby = 0, g->addr = addr;
    if (shown() >= LR_CAP) {
        g->over = 1;                                   /* the game would drop it: see shown */
        return 0;
    }
    g->delivered = 1;
    if (!now) push(d, addr);
    return now;
}

void lr_request(void)
{
    games_n = q_head = q_n = 0;
    rows_n = lobby_waiting = full_shown = 0;
    asked_at = hn_ms();
    if (!fixed_adapters) {
        ad_n = hn_adapters(ad, mk, 32);
        if (hn_route_address(&route) < 0) route = 0;
    }
}

int lr_arrived(const uint8_t *p, long n, uint32_t addr)
{
    if (n >= LR_DATAGRAM && lr_is(p, n, "CM3 LIST SERVER ACK")) {
        char host[12], name[LOBBY_TEXT];
        key_of_ack(p, host, name);
        int r = rank(addr);
        Game *g = find_lan(host, name);
        if (g) {
            if (g->delivered) return g->addr == addr;    /* the game drops a second ACK of an address itself */
            if (g->over) return 0;
            if (r < g->rank) {
                g->rank = r, g->addr = addr;
                memcpy(g->d, p, LR_DATAGRAM);
            }
            return r == 0 ? deliver_lan(g, p, addr, 1) : 0;   /* the best place: no need to wait */
        }
        if (games_n == GAMES) return 0;
        Game *lan = &games[games_n++];
        memset(lan, 0, sizeof *lan);
        memcpy(lan->key_host, host, sizeof host);
        memcpy(lan->key_name, name, sizeof name);
        lan->addr = addr, lan->rank = r, lan->seen = hn_ms();
        memcpy(lan->d, p, LR_DATAGRAM);
        if (r != 0) return 0;                            /* held for LR_HOLD_MS */
        return deliver_lan(lan, p, addr, 1);
    }
    if (lr_is(p, n, "CM3 LIST SERVER BYE")) {
        for (int i = 0; i < games_n; i++)
            if (!games[i].lobby && games[i].addr == addr) {
                int had = games[i].delivered;
                drop(&games[i]);
                return had;
            }
        return 0;
    }
    return 1;
}

static const char *const months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* The text of a lobby row: the name, then the season, the day, the managers online, the ping (0.7.0 slice 7) and
 * Away, in 49 bytes. A number from 2 goes at the end of a row whose text another row has. When the name would keep
 * fewer than 8 characters, the day goes, and then the ping. */
static void row_text(char *out, const LobbyGame *g, int number)
{
    char more[80];
    int y = 0, m = 0, d = 0;
    sscanf(g->day, "%d-%d-%d", &y, &m, &d);
    for (int cut = 0; cut < 3; cut++) {
        int n = snprintf(more, sizeof more, "  %d/%02d", g->season, (g->season + 1) % 100);
        if (cut == 0 && m >= 1 && m <= 12 && d >= 1)
            n += snprintf(more + n, sizeof more - (size_t)n, " %d %s", d, months[m - 1]);
        n += snprintf(more + n, sizeof more - (size_t)n, "  %d online", g->managers_online);
        if (cut < 2 && g->ping_ms > 0) n += snprintf(more + n, sizeof more - (size_t)n, ", %d ms", g->ping_ms);
        if (g->away_n) n += snprintf(more + n, sizeof more - (size_t)n, ", %d away", g->away_n);
        if (g->password) n += snprintf(more + n, sizeof more - (size_t)n, ", password");
        if (number > 1) snprintf(more + n, sizeof more - (size_t)n, " (%d)", number);
        if (49 - (int)strlen(more) >= 8) break;
    }
    int room = 49 - (int)strlen(more);
    snprintf(out, 50, "%.*s%s", room > 8 ? room : 8, g->name, more);
}

static void ack_of(uint8_t *d, const char *host, const char *text)
{
    memset(d, 0, LR_DATAGRAM);
    memcpy(d, "CM3 LIST SERVER ACK", LR_TAG_LEN);
    snprintf((char *)d + LR_HOST_AT, 50, "%s", host);
    snprintf((char *)d + LR_NAME_AT, 50, "%s", text);
}

long lr_extra(uint8_t *buf, uint32_t room, uint32_t *addr)
{
    uint64_t now = hn_ms();
    for (int i = 0; i < games_n; i++) {
        Game *g = &games[i];
        if (!g->lobby && !g->delivered && !g->over && now - g->seen >= LR_HOLD_MS) deliver_lan(g, g->d, g->addr, 0);
    }
    if (lobby_waiting && now - asked_at >= LR_LOBBY_WAIT_MS) {
        lobby_waiting = 0;
        uint8_t d[LR_DATAGRAM];
        if (lobby_result == LH_FULL_TODAY && shown() < LR_CAP) {
            ack_of(d, "", LR_FULL_TEXT);
            push(d, to_net(LR_FULL_ROW));
            full_shown = 1;
        } else if (lobby_result == LH_UNAVAILABLE && shown() < LR_CAP) {
            ack_of(d, "", LR_DOWN_TEXT);            /* 0.7.0 slice 7: Join on it gives the reason */
            push(d, to_net(LR_DOWN_ROW));
            full_shown = 1;
        }
        char used[LOBBY_ROWS][50];
        int used_n = 0;
        for (int i = 0; i < rows_n; i++) {
            char name[LOBBY_TEXT], text[50];
            lobby_clean(name, LOBBY_TEXT, rows[i].g.name);
            if (find(rows[i].g.host_name, name) || games_n == GAMES) continue;   /* on the LAN */
            if (shown() >= LR_CAP) break;
            /* The join takes the first row whose text equals the Join text (0x77ab2a..0x77ab88), and a long name
             * is cut to make room for the details: a second row with the same text gets a number. */
            row_text(text, &rows[i].g, 1);
            for (int k = 2, again = 1; again && k < 100; k++) {
                again = 0;
                for (int u = 0; u < used_n; u++)
                    if (!strcmp(used[u], text)) again = 1;
                if (again) row_text(text, &rows[i].g, k);
            }
            memcpy(used[used_n++], text, sizeof text);
            Game *g = &games[games_n++];
            memset(g, 0, sizeof *g);
            snprintf(g->key_host, sizeof g->key_host, "%s", rows[i].g.host_name);
            memcpy(g->key_name, name, sizeof name);
            g->lobby = g->delivered = 1;
            memcpy(g->text, text, sizeof text);
            g->addr = to_net(LR_NET | (uint32_t)i);
            ack_of(d, rows[i].g.host_name, text);
            push(d, g->addr);
        }
    }
    if (!q_n || room < LR_DATAGRAM) return -1;
    memcpy(buf, queue[q_head].d, LR_DATAGRAM);
    *addr = queue[q_head].addr;
    q_head = (q_head + 1) % QUEUE, q_n--;
    return LR_DATAGRAM;
}

int lr_shown_text(const char *code, char out[50])
{
    for (int i = 0; i < rows_n; i++) {
        if (strcmp(rows[i].code, code)) continue;
        char name[LOBBY_TEXT];
        lobby_clean(name, LOBBY_TEXT, rows[i].g.name);
        const Game *g = find(rows[i].g.host_name, name);
        if (!g || !g->delivered) return 0;
        snprintf(out, 50, "%.49s", g->lobby ? g->text : (const char *)g->d + LR_NAME_AT);
        return 1;
    }
    return 0;
}

int lr_lobby_result(void) { return lobby_result; }

void lr_lobby(const LobbyRow *r, int n, int result)
{
    rows_n = n < LOBBY_ROWS ? n : LOBBY_ROWS;
    memcpy(rows, r, (size_t)rows_n * sizeof rows[0]);
    lobby_result = result;
    lobby_waiting = 1;
}

const LobbyRow *lr_row_of(uint32_t addr, int *full)
{
    uint32_t h = to_host(addr);
    *full = 0;
    if ((h & 0xffff0000u) != LR_NET) return NULL;
    if (h == LR_FULL_ROW || h == LR_DOWN_ROW) {
        *full = h == LR_FULL_ROW ? 1 : 2;
        return NULL;
    }
    uint32_t i = h & 0xffffu;
    return i < (uint32_t)rows_n ? &rows[i] : NULL;
}
