/* The game's sockets (0.7.0 slice 2, #45). net.c reads the WSOCK32 arguments from the game's memory and
 * calls these. They keep the game's socket handles and the traffic counts, and give the game the
 * WinSock behaviour that the original's code expects
 * (docs/research/2026-10-27-network-code-reading.md, sections 4 and 9).
 *
 * An address is IPv4 in network byte order, a port a plain number. A call that fails returns -1
 * (NC_INVALID for a handle) and sets the error that nc_error gives (the game's WSAGetLastError). */
#ifndef NET_CORE_H
#define NET_CORE_H

#include <stdint.h>

#define NC_INVALID 0xffffffffu

/* The WinSock option numbers that the game passes (level 0xffff, SOL_SOCKET; level 6, IPPROTO_TCP). */
#define NC_SOL_SOCKET 0xffff
#define NC_IPPROTO_TCP 6
#define NC_SO_DEBUG 0x1
#define NC_SO_REUSEADDR 0x4
#define NC_SO_KEEPALIVE 0x8
#define NC_SO_BROADCAST 0x20
#define NC_SO_LINGER 0x80
#define NC_SO_SNDBUF 0x1001
#define NC_SO_RCVBUF 0x1002
#define NC_TCP_NODELAY 0x1

/* The connect timeout. The original's connect 0x8ae74e waits with no limit. */
#define NC_CONNECT_MS 4000
/* What a close waits to send the bytes still queued. The host closes up to 16 connections at its quit. */
#define NC_CLOSE_MS 250
/* A connection whose queue grows past this is treated as broken. */
#define NC_QUEUE_MAX (16u << 20)

int nc_error(void);
void nc_set_connect_ms(int ms);    /* tests: the connect timeout, NC_CONNECT_MS by default */
void nc_set_error(int code);

int nc_start(void);
uint32_t nc_socket(int af, int type);
int nc_close(uint32_t h);
int nc_nonblocking(uint32_t h, int on);
int nc_readable(uint32_t h, uint32_t *n);
/* value: the int option value. For NC_SO_LINGER: on and seconds. */
int nc_setopt(uint32_t h, int level, int name, int value, int seconds);
int nc_bind(uint32_t h, uint32_t addr, uint16_t port);
int nc_listen(uint32_t h, int backlog);
uint32_t nc_accept(uint32_t h, uint32_t *addr, uint16_t *port);
int nc_connect(uint32_t h, uint32_t addr, uint16_t port);
/* Takes the whole buffer and returns n, or -1 when the connection is broken. Never a short count. */
long nc_send(uint32_t h, const void *buf, uint32_t n);
long nc_recv(uint32_t h, void *buf, uint32_t n);
long nc_sendto(uint32_t h, const void *buf, uint32_t n, uint32_t addr, uint16_t port);
/* The packets with opcode 0 that the game read from all TCP connections so far. Each host screen starts with
 * one: Display::process_command_packet 0x564080, case 0 at 0x564131, calls Display::reset 0x55f7c0. The click
 * rule of the input poll (user32.c) reads it. */
uint64_t nc_resets_in(void);
long nc_recvfrom(uint32_t h, void *buf, uint32_t n, uint32_t *addr, uint16_t *port);

/* The runtime's lobby and game list (net_lobby.c) under the game's UDP reads and its connects (0.7.0 slice 4).
 * Each hook may be NULL. Without hooks every call behaves as above. */
typedef struct {
    void (*sent)(const uint8_t *p, uint32_t n, uint32_t addr, uint16_t port);   /* a datagram the game sent */
    int (*arrived)(const uint8_t *p, long n, uint32_t addr);   /* a datagram the game reads: 0 drops it */
    long (*extra)(uint8_t *buf, uint32_t room, uint32_t *addr, uint16_t *port);  /* one to add, or -1 */
    int (*connect)(uint32_t addr, uint16_t port);  /* before a connect: -1 refuses it (the hook sets the error) */
    void (*tick)(void);                            /* at each call of the game */
    /* 0.7.0 slice 5, the relay (relay.h). route: before a connect that the connect hook let through. 1: *s is a
     * connected socket that takes the place of the system's connect, 0: connect as usual, -1: refused (the hook
     * sets the error). incoming: a new connection for the game's listen socket, or HN_INVALID. */
    int (*route)(uint32_t addr, uint16_t port, intptr_t *s);
    intptr_t (*incoming)(uint32_t *addr, uint16_t *port);
} NcHooks;
void nc_set_hooks(const NcHooks *h);

/* The join check (0.7.0 slice 4). When an identity is set, a joiner's connect to an address that is not
 * 127.x sends one line block before the game sees the connection:
 *     "BOSMAN HELLO 1\nversion=<v>\npassword=<p>\n\n"
 * and the host's accept answers "BOSMAN OK\n", "BOSMAN VERSION <host version>\n" or "BOSMAN PASSWORD\n"
 * before it gives the game the connection. The game's own bytes come only after "OK". The loopback test
 * 0x8af290 connects to 127.0.0.1 and gets no check. A host waits NC_HELLO_MS for the block, then closes. */
#define NC_HELLO_MS 5000
/* What a joiner waits for the answer. The host answers at each network call of its game; a game that works
 * out a day makes none for a few seconds. The original's connect waited with no limit. */
#define NC_HELLO_WAIT_MS 8000
void nc_set_identity(const char *version, const char *host_password, const char *join_password);
/* 0.7.0 slice 6: the passwords alone, from the Network tab (host) and the password box (joiner). */
void nc_set_host_password(const char *pw);
void nc_set_join_password(const char *pw);
/* The relay (slice 5): the join password for the lobby's join call, and a refusal that came from the lobby. */
const char *nc_join_password(void);
void nc_set_refusal(int why);
enum { NC_REFUSED_NONE, NC_REFUSED_VERSION, NC_REFUSED_PASSWORD, NC_REFUSED_NOT_BOSMAN, NC_REFUSED_NO_ANSWER };
/* Why the last connect was refused, and the host's version for NC_REFUSED_VERSION. */
int nc_refusal(char *host_version, unsigned room);
void nc_refusal_clear(void);
/* The connections that this host accepted and that are open (the human managers who joined). */
int nc_peers(void);

/* Drop in, drop out and host quit (0.7.0 slice 6). The keepalive timers of every game connection: a peer
 * that stops answering breaks the connection in about NC_KEEP_IDLE_S + NC_KEEP_INTERVAL_S * NC_KEEP_COUNT seconds
 * (Windows uses its own count of 10). */
#define NC_KEEP_IDLE_S 20
#define NC_KEEP_INTERVAL_S 5
#define NC_KEEP_COUNT 4
/* 1 when the joiner's connection to its host ended: the host's quit packet 0x04 came, or the connection broke.
 * When it broke between two packets, the game reads the quit packet 0x04 in place of the close, so the
 * client loop 0x7fdc00 ends as at a quit. A new connect clears it. */
int nc_host_left(void);
/* Host: until the next call with NC_INVALID, a read of the game on a joiner's connection other than handle gives
 * "would block" between two packets (net_session.c, the screen loop's reads). */
void nc_hold_others(uint32_t handle);
void nc_host_left_clear(void);
/* The joiners' connections that this host accepted and that are open: handles, addresses, ports. The count. */
int nc_peer_list(uint32_t *handle, uint32_t *addr, uint16_t *port, int max);
/* The host removes a joiner: its connection closes, and the game's next call on the handle fails, so the
 * original's leave path runs (setup_on_network_player_left 0x838c00). 0 on success. */
int nc_kick(uint32_t handle);

/* Clear errors and ping (0.7.0 slice 7). Control packets: see net_core.c. The host sends a PING to each joiner every
 * NC_PING_MS, and the joiner's runtime answers PONG at once. A ping is the shortest of the last NC_PING_KEEP round
 * trips, so that a host that was busy with a day does not count. */
#define NC_CONTROL_OP 0xbe
#define NC_CONTROL_MAX 48           /* the longest control packet body */
#define NC_PING_MS 3000
#define NC_PING_RELAY_MS 30000      /* through the relay (slice 5): see ping_gap in net_core.c */
#define NC_PING_KEEP 4
#define NC_PING_LOST_MS 10000       /* a PING with no PONG for this long is lost: the next one goes */
/* The ping of a joiner's connection on this host in ms, or -1 when none is known yet. */
int nc_ping_ms(uint32_t handle);
void nc_set_ping_every(int ms);     /* tests: the time between two PINGs, NC_PING_MS by default */
/* Joiner: 1 when the host's KICK came on the connection to the host. A new connect clears it. */
int nc_kicked(void);
/* Joiner: 1 when the connection to the host broke with no quit packet (nc_host_left is 1 too). */
int nc_host_broke(void);
/* The WinSock error of the last connect that failed before the join check, 0 when none. */
int nc_connect_error(void);
/* The WinSock error of the last TCP bind that failed (the host's listen sockets, 0x8ae5bd), 0 when none. */
int nc_bind_error(void);
void nc_bind_error_clear(void);

/* The adapter table of 0x8aee20: at most max addresses and their subnet broadcasts. The count. */
int nc_adapters(uint32_t *addr, uint32_t *broadcast, int max);

/* Traffic log (RECOMP_NET_LOG=<file>, or "-" for stderr): set by nc_start from the environment, or by a
 * test. Each TCP connection prints a line when it opens and closes, and one line every
 * NC_LOG_MS while it moves bytes. */
#define NC_LOG_MS 10000
void nc_log_to(const char *path);
void nc_log_all(const char *why);

/* The counts of one connection, for tests. 0 when the handle is unknown. */
typedef struct {
    uint64_t bytes_out, bytes_in, packets_out, packets_in, sends, queued;
} NcCounts;
int nc_counts(uint32_t h, NcCounts *c);

#endif
