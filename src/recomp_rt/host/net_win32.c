/* Sockets of 64-bit Windows (host_net.h): WinSock 2, whose error codes are the ones the game expects.
 * The adapters come from GetAdaptersAddresses. The secure connection's root certificates come from the
 * system's ROOT store (crypt32). */
#define _WIN32_WINNT 0x0600        /* GetTickCount64 and OnLinkPrefixLength: Windows Vista and later */
#define CERT_CHAIN_PARA_HAS_EXTRA_FIELDS    /* dwUrlRetrievalTimeout */
#include "../host_net.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wincrypt.h>
#include <mstcpip.h>
#include <stdlib.h>
#include <string.h>

static _Thread_local int last_error;

static int fail(int code)
{
    last_error = code;
    return -1;
}

static int fail_wsa(void) { return fail(WSAGetLastError()); }

int hn_start(void)
{
    static int started;
    WSADATA d;
    if (!started && WSAStartup(MAKEWORD(2, 2), &d) == 0) started = 1;
    return started ? 0 : fail(HN_SYSNOTREADY);
}

int hn_error(void) { return last_error; }
uint64_t hn_ms(void) { return GetTickCount64(); }

static int make_nonblocking(SOCKET s)
{
    u_long on = 1;
    return ioctlsocket(s, FIONBIO, &on);
}

hn_sock hn_socket(int stream)
{
    SOCKET s = socket(AF_INET, stream ? SOCK_STREAM : SOCK_DGRAM, stream ? IPPROTO_TCP : IPPROTO_UDP);
    if (s == INVALID_SOCKET) return fail_wsa(), HN_INVALID;
    if (make_nonblocking(s)) {
        fail_wsa();
        closesocket(s);
        return HN_INVALID;
    }
    SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
    return (hn_sock)s;
}

int hn_close(hn_sock s) { return closesocket((SOCKET)s) ? fail_wsa() : 0; }

int hn_set_flag(hn_sock s, int flag, int on)
{
    int level = SOL_SOCKET, name;
    BOOL v = on != 0;
    switch (flag) {
    case HN_REUSEADDR: name = SO_REUSEADDR; break;
    case HN_REUSEPORT: return 0;        /* SO_REUSEADDR already shares the port on Windows */
    case HN_BROADCAST: name = SO_BROADCAST; break;
    case HN_KEEPALIVE: name = SO_KEEPALIVE; break;
    case HN_NODELAY: level = IPPROTO_TCP; name = TCP_NODELAY; break;
    default: return fail(HN_NOPROTOOPT);
    }
    return setsockopt((SOCKET)s, level, name, (const char *)&v, sizeof v) ? fail_wsa() : 0;
}

/* SIO_KEEPALIVE_VALS (Windows 2000 and later) sets the idle time and the interval. The count is the system's
 * (10 on Windows Vista and later). */
int hn_set_keepalive(hn_sock s, int idle_s, int interval_s, int count)
{
    struct tcp_keepalive k;
    DWORD n = 0;
    (void)count;
    k.onoff = 1;
    k.keepalivetime = (ULONG)idle_s * 1000u;
    k.keepaliveinterval = (ULONG)interval_s * 1000u;
    return WSAIoctl((SOCKET)s, SIO_KEEPALIVE_VALS, &k, sizeof k, NULL, 0, &n, NULL, NULL) ? fail_wsa() : 0;
}

int hn_set_linger(hn_sock s, int on, int seconds)
{
    struct linger l;
    l.l_onoff = (u_short)on;
    l.l_linger = (u_short)seconds;
    return setsockopt((SOCKET)s, SOL_SOCKET, SO_LINGER, (const char *)&l, sizeof l) ? fail_wsa() : 0;
}

static struct sockaddr_in sin_of(uint32_t addr, uint16_t port)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = addr;
    return a;
}

int hn_bind(hn_sock s, uint32_t addr, uint16_t port)
{
    struct sockaddr_in a = sin_of(addr, port);
    return bind((SOCKET)s, (struct sockaddr *)&a, sizeof a) ? fail_wsa() : 0;
}

int hn_listen(hn_sock s, int backlog) { return listen((SOCKET)s, backlog) ? fail_wsa() : 0; }

hn_sock hn_accept(hn_sock s, uint32_t *addr, uint16_t *port)
{
    struct sockaddr_in a;
    int n = sizeof a;
    SOCKET c = accept((SOCKET)s, (struct sockaddr *)&a, &n);
    if (c == INVALID_SOCKET) return fail_wsa(), HN_INVALID;
    if (make_nonblocking(c)) {
        fail_wsa();
        closesocket(c);
        return HN_INVALID;
    }
    SetHandleInformation((HANDLE)c, HANDLE_FLAG_INHERIT, 0);
    if (addr) *addr = a.sin_addr.s_addr;
    if (port) *port = ntohs(a.sin_port);
    return (hn_sock)c;
}

int hn_wait(hn_sock s, int read, int ms)
{
    fd_set set, bad;
    struct timeval t = {ms / 1000, (ms % 1000) * 1000};
    FD_ZERO(&set);
    FD_ZERO(&bad);
    FD_SET((SOCKET)s, &set);
    FD_SET((SOCKET)s, &bad);
    /* A failed non-blocking connect shows in the except set. */
    int r = select(0, read ? &set : NULL, read ? NULL : &set, &bad, &t);
    if (r == SOCKET_ERROR) return fail_wsa();
    return r > 0;
}

int hn_connect(hn_sock s, uint32_t addr, uint16_t port, int timeout_ms)
{
    struct sockaddr_in a = sin_of(addr, port);
    if (connect((SOCKET)s, (struct sockaddr *)&a, sizeof a) == 0) return 0;
    int e = WSAGetLastError();
    if (e != WSAEWOULDBLOCK && e != WSAEINPROGRESS) return fail(e);
    int r = hn_wait(s, 0, timeout_ms);
    if (r < 0) return -1;
    if (r == 0) return fail(HN_TIMEDOUT);
    int err = 0, n = sizeof err;
    if (getsockopt((SOCKET)s, SOL_SOCKET, SO_ERROR, (char *)&err, &n)) return fail_wsa();
    return err ? fail(err) : 0;
}

long hn_send(hn_sock s, const void *buf, size_t n)
{
    int k = send((SOCKET)s, (const char *)buf, (int)n, 0);
    return k == SOCKET_ERROR ? fail_wsa() : k;
}

long hn_recv(hn_sock s, void *buf, size_t n)
{
    int k = recv((SOCKET)s, (char *)buf, (int)n, 0);
    return k == SOCKET_ERROR ? fail_wsa() : k;
}

long hn_sendto(hn_sock s, const void *buf, size_t n, uint32_t addr, uint16_t port)
{
    struct sockaddr_in a = sin_of(addr, port);
    int k = sendto((SOCKET)s, (const char *)buf, (int)n, 0, (struct sockaddr *)&a, sizeof a);
    return k == SOCKET_ERROR ? fail_wsa() : k;
}

int hn_shutdown_send(hn_sock s) { return shutdown((SOCKET)s, SD_SEND) == SOCKET_ERROR ? fail_wsa() : 0; }

long hn_peek(hn_sock s, void *buf, size_t n)
{
    int k = recv((SOCKET)s, (char *)buf, (int)n, MSG_PEEK);
    return k == SOCKET_ERROR ? fail_wsa() : k;
}

long hn_recvfrom(hn_sock s, void *buf, size_t n, uint32_t *addr, uint16_t *port)
{
    struct sockaddr_in a;
    int len = sizeof a;
    memset(&a, 0, sizeof a);
    int k = recvfrom((SOCKET)s, (char *)buf, (int)n, 0, (struct sockaddr *)&a, &len);
    /* A datagram longer than the buffer: Windows gives WSAEMSGSIZE with the first bytes, as the game's
     * own WinSock did. */
    if (k == SOCKET_ERROR) return fail_wsa();
    if (addr) *addr = a.sin_addr.s_addr;
    if (port) *port = ntohs(a.sin_port);
    return k;
}

int hn_readable_bytes(hn_sock s, uint32_t *n)
{
    u_long v = 0;
    if (ioctlsocket((SOCKET)s, FIONREAD, &v)) return fail_wsa();
    *n = (uint32_t)v;
    return 0;
}

int hn_hostname(char *out, size_t room)
{
    char name[256];
    if (!room) return fail(HN_FAULT);
    if (hn_start() < 0) return -1;
    if (gethostname(name, sizeof name)) return fail_wsa();
    name[sizeof name - 1] = 0;
    strncpy(out, name, room - 1);
    out[room - 1] = 0;
    return 0;
}

int hn_adapters(uint32_t *addr, uint32_t *mask, int max)
{
    ULONG size = 16384;
    IP_ADAPTER_ADDRESSES *list = NULL;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG r = ERROR_BUFFER_OVERFLOW;
    for (int tries = 0; tries < 3 && r == ERROR_BUFFER_OVERFLOW; tries++) {
        free(list);
        list = malloc(size);
        if (!list) return 0;
        r = GetAdaptersAddresses(AF_INET, flags, NULL, list, &size);
    }
    int n = 0;
    if (r == NO_ERROR)
        for (IP_ADAPTER_ADDRESSES *a = list; a && n < max; a = a->Next) {
            if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u && n < max; u = u->Next) {
                if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
                uint32_t v = ((struct sockaddr_in *)u->Address.lpSockaddr)->sin_addr.s_addr;
                if (!v || v == htonl(INADDR_LOOPBACK)) continue;
                unsigned bits = u->OnLinkPrefixLength > 32 ? 32 : u->OnLinkPrefixLength;
                addr[n] = v;
                mask[n] = bits ? htonl(0xffffffffu << (32 - bits)) : 0;
                n++;
            }
        }
    free(list);
    return n;
}

typedef struct { void (*fn)(void *); void *arg; } ThreadStart;

static DWORD WINAPI thread_main(LPVOID p)
{
    ThreadStart t = *(ThreadStart *)p;
    free(p);
    t.fn(t.arg);
    return 0;
}

int hn_thread(void (*fn)(void *), void *arg)
{
    ThreadStart *t = malloc(sizeof *t);
    if (!t) return fail(WSAENOBUFS);
    t->fn = fn, t->arg = arg;
    HANDLE h = CreateThread(NULL, 0, thread_main, t, 0, NULL);
    if (!h) {
        free(t);
        return fail(WSAENOBUFS);
    }
    CloseHandle(h);
    return 0;
}

static SRWLOCK lock = SRWLOCK_INIT;
void hn_lock(void) { AcquireSRWLockExclusive(&lock); }
void hn_unlock(void) { ReleaseSRWLockExclusive(&lock); }
void hn_sleep(int ms) { Sleep((DWORD)ms); }

int hn_resolve(const char *name, uint32_t *addr)
{
    struct addrinfo hints, *res = NULL;
    if (hn_start() < 0) return -1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(name, NULL, &hints, &res) || !res) return fail(WSAHOST_NOT_FOUND);
    *addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return 0;
}

int hn_route_address(uint32_t *addr)
{
    if (hn_start() < 0) return -1;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return fail_wsa();
    struct sockaddr_in to = sin_of(htonl(0xc0000201u), 9), me;     /* 192.0.2.1, a documentation address */
    int len = sizeof me;
    int r = connect(s, (struct sockaddr *)&to, sizeof to) || getsockname(s, (struct sockaddr *)&me, &len);
    int e = WSAGetLastError();
    closesocket(s);
    if (r) return fail(e);
    *addr = me.sin_addr.s_addr;
    return *addr ? 0 : fail(HN_NETDOWN);
}

int hn_pair(hn_sock *a, hn_sock *b)
{
    if (hn_start() < 0) return -1;
    SOCKET l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), c = INVALID_SOCKET, s = INVALID_SOCKET;
    struct sockaddr_in at = sin_of(htonl(INADDR_LOOPBACK), 0), mine, peer;
    int len = sizeof at;
    if (l == INVALID_SOCKET) return fail_wsa();
    if (bind(l, (struct sockaddr *)&at, sizeof at) || listen(l, 1) || getsockname(l, (struct sockaddr *)&at, &len))
        goto failed;
    c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c == INVALID_SOCKET || connect(c, (struct sockaddr *)&at, sizeof at)) goto failed;
    len = sizeof peer;
    s = accept(l, (struct sockaddr *)&peer, &len);
    len = sizeof mine;
    /* Only the connection of c: another program on this computer could connect first. */
    if (s == INVALID_SOCKET || getsockname(c, (struct sockaddr *)&mine, &len) || peer.sin_port != mine.sin_port)
        goto failed;
    if (make_nonblocking(c) || make_nonblocking(s)) goto failed;
    closesocket(l);
    SetHandleInformation((HANDLE)c, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
    BOOL one = TRUE;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    *a = (hn_sock)c, *b = (hn_sock)s;
    return 0;
failed:
    fail_wsa();
    closesocket(l);
    if (c != INVALID_SOCKET) closesocket(c);
    if (s != INVALID_SOCKET) closesocket(s);
    return -1;
}

int hn_trust_roots(void (*add)(void *ctx, const unsigned char *data, size_t n, int pem), void *ctx)
{
    HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT");
    if (!store) return 0;
    int n = 0;
    for (PCCERT_CONTEXT c = NULL; (c = CertEnumCertificatesInStore(store, c)) != NULL; n++)
        add(ctx, c->pbCertEncoded, c->cbCertEncoded, 0);
    CertCloseStore(store, 0);
    return n;
}

int hn_verify_server(const unsigned char *const *der, const size_t *len, int n, const char *host)
{
    if (n < 1) return -1;
    HCERTSTORE extra = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
    if (!extra) return -1;
    PCCERT_CONTEXT leaf = NULL;
    for (int i = 0; i < n; i++)
        CertAddEncodedCertificateToStore(extra, X509_ASN_ENCODING, der[i], (DWORD)len[i],
                                         CERT_STORE_ADD_USE_EXISTING, i ? NULL : &leaf);
    int ok = 0;
    wchar_t name[256];
    if (leaf && MultiByteToWideChar(CP_UTF8, 0, host, -1, name, 256) > 0) {
        LPSTR usage[] = {(LPSTR)szOID_PKIX_KP_SERVER_AUTH};
        CERT_CHAIN_PARA para;
        memset(&para, 0, sizeof para);
        para.cbSize = sizeof para;
        para.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
        para.RequestedUsage.Usage.cUsageIdentifier = 1;
        para.RequestedUsage.Usage.rgpszUsageIdentifier = usage;
        para.dwUrlRetrievalTimeout = 5000;     /* a root that Windows fetches: the join waits at most this */
        PCCERT_CHAIN_CONTEXT chain = NULL;
        if (CertGetCertificateChain(NULL, leaf, NULL, extra, &para, 0, NULL, &chain)) {
            SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl;
            memset(&ssl, 0, sizeof ssl);
            ssl.cbSize = sizeof ssl;
            ssl.dwAuthType = AUTHTYPE_SERVER;
            ssl.pwszServerName = name;
            CERT_CHAIN_POLICY_PARA policy;
            memset(&policy, 0, sizeof policy);
            policy.cbSize = sizeof policy;
            policy.pvExtraPolicyPara = &ssl;
            CERT_CHAIN_POLICY_STATUS status;
            memset(&status, 0, sizeof status);
            status.cbSize = sizeof status;
            ok = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain, &policy, &status) && !status.dwError;
            CertFreeCertificateChain(chain);
        }
    }
    if (leaf) CertFreeCertificateContext(leaf);
    CertCloseStore(extra, 0);
    return ok ? 0 : -1;
}
