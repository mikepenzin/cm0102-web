/* Sockets of macOS and Linux (host_net.h). The errno of each failure becomes the WinSock code that the
 * game expects. A send to a closed peer gives an error, never SIGPIPE. */
#include "../host_net.h"

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

static _Thread_local int last_error;

static int fail(int code)
{
    last_error = code;
    return -1;
}

/* The WinSock code of an errno. */
static int wsa_of(int e)
{
    switch (e) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return HN_WOULDBLOCK;
    case EINPROGRESS: return 10036;     /* WSAEINPROGRESS */
    case EALREADY: return 10037;
    case EBADF:
    case ENOTSOCK: return HN_NOTSOCK;
    case EDESTADDRREQ: return 10039;
    case EMSGSIZE: return 10040;
    case EPROTOTYPE: return 10041;
    case ENOPROTOOPT: return HN_NOPROTOOPT;
    case EPROTONOSUPPORT: return 10043;
    case EOPNOTSUPP: return 10045;
    case EAFNOSUPPORT: return HN_AFNOSUPPORT;
    case EADDRINUSE: return 10048;
    case EADDRNOTAVAIL: return 10049;
    case ENETDOWN: return HN_NETDOWN;
    case ENETUNREACH: return 10051;
    case ECONNABORTED: return 10053;
    case EPIPE:
    case ECONNRESET: return HN_CONNRESET;
    case ENOBUFS: return 10055;
    case EISCONN: return 10056;
    case ENOTCONN: return HN_NOTCONN;
    case ETIMEDOUT: return HN_TIMEDOUT;
    case ECONNREFUSED: return 10061;
    case EHOSTDOWN: return 10064;
    case EHOSTUNREACH: return 10065;
    case EMFILE: return 10024;
    case EACCES: return 10013;
    case EFAULT: return HN_FAULT;
    default: return HN_INVAL;
    }
}

static int fail_errno(void) { return fail(wsa_of(errno)); }

int hn_start(void) { return 0; }
int hn_error(void) { return last_error; }

uint64_t hn_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}

static int make_nonblocking(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    return fl < 0 ? -1 : fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

/* A new descriptor: non-blocking, closed on exec, and no SIGPIPE (macOS has no MSG_NOSIGNAL). */
static int prepare(int fd)
{
    int one = 1;
    if (make_nonblocking(fd) < 0) return -1;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    (void)one;
    return 0;
}

hn_sock hn_socket(int stream)
{
    int fd = socket(AF_INET, stream ? SOCK_STREAM : SOCK_DGRAM, stream ? IPPROTO_TCP : IPPROTO_UDP);
    if (fd < 0) return fail_errno(), HN_INVALID;
    if (prepare(fd) < 0) {
        fail_errno();
        close(fd);
        return HN_INVALID;
    }
    return fd;
}

int hn_close(hn_sock s) { return close((int)s) < 0 ? fail_errno() : 0; }

int hn_set_flag(hn_sock s, int flag, int on)
{
    int level = SOL_SOCKET, name;
    switch (flag) {
    case HN_REUSEADDR: name = SO_REUSEADDR; break;
    case HN_REUSEPORT:
#ifdef SO_REUSEPORT
        name = SO_REUSEPORT; break;
#else
        return 0;
#endif
    case HN_BROADCAST: name = SO_BROADCAST; break;
    case HN_KEEPALIVE: name = SO_KEEPALIVE; break;
    case HN_NODELAY: level = IPPROTO_TCP; name = TCP_NODELAY; break;
    default: return fail(HN_NOPROTOOPT);
    }
    on = on != 0;
    return setsockopt((int)s, level, name, &on, sizeof on) < 0 ? fail_errno() : 0;
}

int hn_set_linger(hn_sock s, int on, int seconds)
{
    struct linger l = {on, seconds};
    return setsockopt((int)s, SOL_SOCKET, SO_LINGER, &l, sizeof l) < 0 ? fail_errno() : 0;
}

int hn_set_keepalive(hn_sock s, int idle_s, int interval_s, int count)
{
    int on = 1;
    if (setsockopt((int)s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof on) < 0) return fail_errno();
#ifdef TCP_KEEPIDLE
    if (setsockopt((int)s, IPPROTO_TCP, TCP_KEEPIDLE, &idle_s, sizeof idle_s) < 0) return fail_errno();
#elif defined(TCP_KEEPALIVE)
    if (setsockopt((int)s, IPPROTO_TCP, TCP_KEEPALIVE, &idle_s, sizeof idle_s) < 0) return fail_errno();
#endif
#ifdef TCP_KEEPINTVL
    if (setsockopt((int)s, IPPROTO_TCP, TCP_KEEPINTVL, &interval_s, sizeof interval_s) < 0) return fail_errno();
#endif
#ifdef TCP_KEEPCNT
    if (setsockopt((int)s, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof count) < 0) return fail_errno();
#endif
    (void)interval_s, (void)count;
    return 0;
}

static struct sockaddr_in sin_of(uint32_t addr, uint16_t port)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
#ifdef __APPLE__
    a.sin_len = sizeof a;
#endif
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = addr;
    return a;
}

int hn_bind(hn_sock s, uint32_t addr, uint16_t port)
{
    struct sockaddr_in a = sin_of(addr, port);
    return bind((int)s, (struct sockaddr *)&a, sizeof a) < 0 ? fail_errno() : 0;
}

int hn_listen(hn_sock s, int backlog) { return listen((int)s, backlog) < 0 ? fail_errno() : 0; }

hn_sock hn_accept(hn_sock s, uint32_t *addr, uint16_t *port)
{
    struct sockaddr_in a;
    socklen_t n = sizeof a;
    int fd = accept((int)s, (struct sockaddr *)&a, &n);
    if (fd < 0) return fail_errno(), HN_INVALID;
    if (prepare(fd) < 0) {
        fail_errno();
        close(fd);
        return HN_INVALID;
    }
    if (addr) *addr = a.sin_addr.s_addr;
    if (port) *port = ntohs(a.sin_port);
    return fd;
}

int hn_wait(hn_sock s, int read, int ms)
{
    struct pollfd p = {(int)s, (short)(read ? POLLIN : POLLOUT), 0};
    int r;
    do r = poll(&p, 1, ms); while (r < 0 && errno == EINTR);
    if (r < 0) return fail_errno();
    return r > 0;
}

int hn_connect(hn_sock s, uint32_t addr, uint16_t port, int timeout_ms)
{
    struct sockaddr_in a = sin_of(addr, port);
    if (connect((int)s, (struct sockaddr *)&a, sizeof a) == 0) return 0;
    if (errno != EINPROGRESS && errno != EINTR) return fail_errno();
    int r = hn_wait(s, 0, timeout_ms);
    if (r < 0) return -1;
    if (r == 0) return fail(HN_TIMEDOUT);
    int err = 0;
    socklen_t n = sizeof err;
    if (getsockopt((int)s, SOL_SOCKET, SO_ERROR, &err, &n) < 0) return fail_errno();
    return err ? fail(wsa_of(err)) : 0;
}

long hn_send(hn_sock s, const void *buf, size_t n)
{
    ssize_t k = send((int)s, buf, n, SEND_FLAGS);
    return k < 0 ? fail_errno() : (long)k;
}

long hn_recv(hn_sock s, void *buf, size_t n)
{
    ssize_t k = recv((int)s, buf, n, 0);
    return k < 0 ? fail_errno() : (long)k;
}

int hn_shutdown_send(hn_sock s) { return shutdown((int)s, SHUT_WR) < 0 ? fail_errno() : 0; }

long hn_peek(hn_sock s, void *buf, size_t n)
{
    ssize_t k = recv((int)s, buf, n, MSG_PEEK);
    return k < 0 ? fail_errno() : (long)k;
}

long hn_sendto(hn_sock s, const void *buf, size_t n, uint32_t addr, uint16_t port)
{
    struct sockaddr_in a = sin_of(addr, port);
    ssize_t k = sendto((int)s, buf, n, SEND_FLAGS, (struct sockaddr *)&a, sizeof a);
    return k < 0 ? fail_errno() : (long)k;
}

long hn_recvfrom(hn_sock s, void *buf, size_t n, uint32_t *addr, uint16_t *port)
{
    struct sockaddr_in a;
    socklen_t len = sizeof a;
    memset(&a, 0, sizeof a);
    ssize_t k = recvfrom((int)s, buf, n, 0, (struct sockaddr *)&a, &len);
    if (k < 0) return fail_errno();
    if (addr) *addr = a.sin_addr.s_addr;
    if (port) *port = ntohs(a.sin_port);
    return (long)k;
}

int hn_readable_bytes(hn_sock s, uint32_t *n)
{
    int v = 0;
    if (ioctl((int)s, FIONREAD, &v) < 0) return fail_errno();
    *n = (uint32_t)v;
    return 0;
}

int hn_hostname(char *out, size_t room)
{
    char name[256];
    if (!room) return fail(HN_FAULT);
    if (gethostname(name, sizeof name) < 0) return fail_errno();
    name[sizeof name - 1] = 0;
    strncpy(out, name, room - 1);
    out[room - 1] = 0;
    return 0;
}

int hn_adapters(uint32_t *addr, uint32_t *mask, int max)
{
    struct ifaddrs *list, *i;
    int n = 0;
    if (getifaddrs(&list) < 0) return 0;
    for (i = list; i && n < max; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
        uint32_t a = ((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr;
        if (!a || a == htonl(INADDR_LOOPBACK)) continue;
        addr[n] = a;
        mask[n] = i->ifa_netmask ? ((struct sockaddr_in *)i->ifa_netmask)->sin_addr.s_addr : 0xffffffffu;
        n++;
    }
    freeifaddrs(list);
    return n;
}

static void *thread_main(void *p)
{
    void **a = p;
    void (*fn)(void *) = (void (*)(void *))a[0];
    void *arg = a[1];
    free(a);
    fn(arg);
    return NULL;
}

int hn_thread(void (*fn)(void *), void *arg)
{
    void **a = malloc(2 * sizeof *a);
    pthread_t t;
    if (!a) return fail(10055);
    a[0] = (void *)fn, a[1] = arg;
    if (pthread_create(&t, NULL, thread_main, a)) {
        free(a);
        return fail(10055);
    }
    pthread_detach(t);
    return 0;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void hn_lock(void) { pthread_mutex_lock(&lock); }
void hn_unlock(void) { pthread_mutex_unlock(&lock); }

void hn_sleep(int ms)
{
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L};
    while (nanosleep(&t, &t) < 0 && errno == EINTR) {}
}

int hn_resolve(const char *name, uint32_t *addr)
{
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(name, NULL, &hints, &res) || !res) return fail(11001);    /* WSAHOST_NOT_FOUND */
    *addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return 0;
}

int hn_route_address(uint32_t *addr)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return fail_errno();
    struct sockaddr_in to = sin_of(htonl(0xc0000201u), 9), me;     /* 192.0.2.1, a documentation address */
    socklen_t len = sizeof me;
    int r = connect(fd, (struct sockaddr *)&to, sizeof to) < 0 || getsockname(fd, (struct sockaddr *)&me, &len) < 0;
    int e = errno;
    close(fd);
    if (r) return fail(wsa_of(e));
    *addr = me.sin_addr.s_addr;
    return *addr ? 0 : fail(HN_NETDOWN);
}

int hn_pair(hn_sock *a, hn_sock *b)
{
    int l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), c = -1, s = -1;
    struct sockaddr_in at = sin_of(htonl(INADDR_LOOPBACK), 0), mine, peer;
    socklen_t len = sizeof at;
    if (l < 0) return fail_errno();
    if (bind(l, (struct sockaddr *)&at, sizeof at) < 0 || listen(l, 1) < 0 ||
        getsockname(l, (struct sockaddr *)&at, &len) < 0)
        goto failed;
    c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c < 0 || connect(c, (struct sockaddr *)&at, sizeof at) < 0) goto failed;
    len = sizeof peer;
    s = accept(l, (struct sockaddr *)&peer, &len);
    len = sizeof mine;
    /* Only the connection of c: another program on this computer could connect first. */
    if (s < 0 || getsockname(c, (struct sockaddr *)&mine, &len) < 0 || peer.sin_port != mine.sin_port) goto failed;
    if (prepare(c) < 0 || prepare(s) < 0) goto failed;
    close(l);
    int one = 1;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    *a = c, *b = s;
    return 0;
failed:
    fail_errno();
    close(l);
    if (c >= 0) close(c);
    if (s >= 0) close(s);
    return -1;
}

int hn_trust_roots(void (*add)(void *ctx, const unsigned char *data, size_t n, int pem), void *ctx)
{
    /* macOS, then Debian, Ubuntu, Arch and the Steam Deck, Fedora, openSUSE, Alpine. */
    static const char *const files[] = {"/etc/ssl/cert.pem", "/etc/ssl/certs/ca-certificates.crt",
                                        "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/ca-bundle.pem",
                                        "/etc/ssl/certs/ca-bundle.crt"};
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        FILE *f = fopen(files[i], "rb");
        if (!f) continue;
        size_t room = 1u << 20, n = 0;
        unsigned char *b = malloc(room + 1);
        size_t r;
        while (b && (r = fread(b + n, 1, room - n, f)) > 0) {
            n += r;
            if (n == room) {
                unsigned char *bigger = realloc(b, room * 2 + 1);
                if (!bigger) break;
                b = bigger, room *= 2;
            }
        }
        fclose(f);
        if (!b) continue;
        b[n] = 0;
        if (n) add(ctx, b, n + 1, 1);
        free(b);
        if (n) return 1;
    }
    return 0;
}

int hn_verify_server(const unsigned char *const *der, const size_t *len, int n, const char *host)
{
    (void)der, (void)len, (void)n, (void)host;
    return -1;
}
