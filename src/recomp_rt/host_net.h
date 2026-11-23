/* Sockets of the host system (0.7.0 slice 2, #45): BSD sockets on macOS and Linux (host/net_posix.c),
 * WinSock 2 on 64-bit Windows (host/net_win32.c). The game's WSOCK32 calls reach these through
 * win/net_core.c, which keeps the game's handles.
 *
 * Every socket that hn_socket and hn_accept return is non-blocking. An address is an IPv4 address in
 * network byte order, as the bytes of a sockaddr_in hold it (s_addr). A port is a plain number.
 * A call that fails returns -1 (HN_INVALID for a socket), and hn_error gives the WinSock error code of
 * that failure: HN_WOULDBLOCK when the call would wait. */
#ifndef RECOMP_HOST_NET_H
#define RECOMP_HOST_NET_H

#include <stddef.h>
#include <stdint.h>

typedef intptr_t hn_sock;
#define HN_INVALID ((hn_sock)-1)

/* WinSock error codes that the runtime uses by name. */
#define HN_WOULDBLOCK 10035     /* WSAEWOULDBLOCK */
#define HN_NOTSOCK 10038        /* WSAENOTSOCK */
#define HN_INVAL 10022          /* WSAEINVAL */
#define HN_FAULT 10014          /* WSAEFAULT */
#define HN_NOPROTOOPT 10042     /* WSAENOPROTOOPT */
#define HN_AFNOSUPPORT 10047    /* WSAEAFNOSUPPORT */
#define HN_NETDOWN 10050        /* WSAENETDOWN */
#define HN_CONNRESET 10054      /* WSAECONNRESET */
#define HN_NOTCONN 10057        /* WSAENOTCONN */
#define HN_TIMEDOUT 10060       /* WSAETIMEDOUT */
#define HN_SYSNOTREADY 10091    /* WSASYSNOTREADY */

/* Socket switches for hn_set_flag. */
enum { HN_REUSEADDR, HN_REUSEPORT, HN_BROADCAST, HN_KEEPALIVE, HN_NODELAY };

int hn_start(void);                                   /* 0 when sockets can be used */
int hn_error(void);                                   /* the WinSock code of the last failure */
uint64_t hn_ms(void);                                 /* a clock in milliseconds */

hn_sock hn_socket(int stream);                        /* stream 1: TCP, 0: UDP */
int hn_close(hn_sock s);
int hn_set_flag(hn_sock s, int flag, int on);         /* HN_REUSEPORT is 0 and does nothing where the host has none */
int hn_set_linger(hn_sock s, int on, int seconds);
/* TCP keepalive with these timers: a peer that stops answering (a crash, a pulled cable, a lost Wi-Fi) breaks the
 * connection after about idle_s + interval_s * count seconds. 0 on success. */
int hn_set_keepalive(hn_sock s, int idle_s, int interval_s, int count);
int hn_bind(hn_sock s, uint32_t addr, uint16_t port);
int hn_listen(hn_sock s, int backlog);
hn_sock hn_accept(hn_sock s, uint32_t *addr, uint16_t *port);
/* Connects within timeout_ms. 0 when the connection is up. */
int hn_connect(hn_sock s, uint32_t addr, uint16_t port, int timeout_ms);
long hn_send(hn_sock s, const void *buf, size_t n);
long hn_recv(hn_sock s, void *buf, size_t n);         /* 0 when the peer closed */
/* As hn_recv, but the bytes stay in the socket for the next read (MSG_PEEK). 0.7.0 slice 7: net_core.c reads its
 * own control packets with it. */
long hn_peek(hn_sock s, void *buf, size_t n);
/* Ends the send side: the peer reads the bytes sent so far, then the end (0.7.0 slice 7, the kick notice). */
int hn_shutdown_send(hn_sock s);
long hn_sendto(hn_sock s, const void *buf, size_t n, uint32_t addr, uint16_t port);
long hn_recvfrom(hn_sock s, void *buf, size_t n, uint32_t *addr, uint16_t *port);
/* 1 when the socket can be read (read 1) or written (read 0) within ms, 0 when not, -1 on failure. */
int hn_wait(hn_sock s, int read, int ms);
int hn_readable_bytes(hn_sock s, uint32_t *n);        /* FIONREAD */

/* The name of this computer, cut to fit room with its zero. 0 on success. */
int hn_hostname(char *out, size_t room);
/* The IPv4 addresses of this computer that are up, not loopback and not 0, with their masks. At most
 * max. The count. */
int hn_adapters(uint32_t *addr, uint32_t *mask, int max);

/* For the lobby client (0.7.0 slice 4, win/lobby.c), which makes its HTTP calls on its own thread so that
 * the game never waits on the network. hn_error keeps one code for each thread. */
int hn_thread(void (*fn)(void *), void *arg);         /* starts a detached thread; 0 when it runs */
void hn_lock(void);                                   /* one lock for the lobby client's shared state */
void hn_unlock(void);
void hn_sleep(int ms);
/* The IPv4 address of a host name or a dotted address, in network order. A DNS look-up can wait: call it on
 * the lobby thread only. 0 on success. */
int hn_resolve(const char *name, uint32_t *addr);
/* The address of this computer that the default route uses. No packet is sent. 0 on success. */
int hn_route_address(uint32_t *addr);

/* For the relay (0.7.0 slice 5, win/relay.c). Two connected TCP sockets on 127.0.0.1, both non-blocking. The
 * game gets one as an ordinary socket, and the relay thread passes the bytes of the other to the relay. 0 on
 * success. */
int hn_pair(hn_sock *a, hn_sock *b);

/* The trusted root certificates of this system, for the secure connection (win/secure.c). Calls add once for
 * each certificate (pem 0: DER bytes) or certificate bundle (pem 1: PEM text with its zero). The count of
 * calls. macOS and Linux: the first bundle file found. Windows: the "ROOT" store. */
int hn_trust_roots(void (*add)(void *ctx, const unsigned char *data, size_t n, int pem), void *ctx);
/* Windows only: checks a server's certificate chain (DER, the server's own first) for host with the system's
 * own check, which also fetches a root that the store does not hold yet. 0 when the system trusts it. -1
 * elsewhere. */
int hn_verify_server(const unsigned char *const *der, const size_t *len, int n, const char *host);

#endif
