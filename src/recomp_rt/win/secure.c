/* The secure connection (secure.h). */
#include "secure.h"
#include "../host_net.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SecureConn {
    hn_sock s;
    int tls;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
};

/* The root certificates, read once and then only read by the checks of all threads. */
static mbedtls_x509_crt roots;
static int roots_read, roots_n;

static void add_root(void *ctx, const unsigned char *data, size_t n, int pem)
{
    (void)ctx;
    if (pem) mbedtls_x509_crt_parse(&roots, data, n);    /* a bad certificate in a bundle is skipped */
    else mbedtls_x509_crt_parse_der(&roots, data, n);
}

static void read_roots(void)
{
    hn_lock();
    if (!roots_read) {
        roots_read = 1;
        mbedtls_x509_crt_init(&roots);
        hn_trust_roots(add_root, NULL);
        const char *test = getenv("RECOMP_TEST_TLS_CA");
        if (test && *test && mbedtls_x509_crt_parse_file(&roots, test) < 0)
            fprintf(stderr, "recomp: secure: cannot read %s\n", test);
        for (const mbedtls_x509_crt *c = &roots; c && c->raw.len; c = c->next) roots_n++;
        fprintf(stderr, "recomp: secure: %d root certificates\n", roots_n);
    }
    hn_unlock();
}

static int bio_send(void *ctx, const unsigned char *b, size_t n)
{
    long r = hn_send(((SecureConn *)ctx)->s, b, n);
    if (r >= 0) return (int)r;
    return hn_error() == HN_WOULDBLOCK ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int bio_recv(void *ctx, unsigned char *b, size_t n)
{
    long r = hn_recv(((SecureConn *)ctx)->s, b, n);
    if (r >= 0) return (int)r;
    return hn_error() == HN_WOULDBLOCK ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int left_ms(uint64_t until)
{
    uint64_t now = hn_ms();
    return now >= until ? 0 : (int)(until - now);
}

static void say(char *why, size_t room, const char *what, int code)
{
    if (!why || !room) return;
    if (code) {
        char t[100];
        mbedtls_strerror(code, t, sizeof t);
        snprintf(why, room, "%s (%s)", what, t);
    } else snprintf(why, room, "%s", what);
}

/* 0 when the server's certificate is trusted. mbedTLS checked it against the roots during the handshake. */
static int check_server(SecureConn *c, const char *host, char *why, size_t room)
{
    uint32_t flags = mbedtls_ssl_get_verify_result(&c->ssl);
    if (!flags) return 0;
#ifdef _WIN32
    /* The ROOT store fills itself on demand: ask Windows, which fetches a root it does not hold yet. */
    const unsigned char *der[8];
    size_t len[8];
    int n = 0;
    for (const mbedtls_x509_crt *x = mbedtls_ssl_get_peer_cert(&c->ssl); x && n < 8; x = x->next)
        der[n] = x->raw.p, len[n] = x->raw.len, n++;
    if (hn_verify_server(der, len, n, host) == 0) return 0;
#else
    (void)host;
#endif
    char t[200];
    mbedtls_x509_crt_verify_info(t, sizeof t, "", flags);
    t[strcspn(t, "\n")] = 0;
    say(why, room, roots_n ? "the server's certificate is not trusted" : "this computer has no root certificates", 0);
    fprintf(stderr, "recomp: secure: %s: %s\n", host, t);
    return -1;
}

SecureConn *sc_open(const char *host, unsigned port, int tls, int timeout_ms, char *why, size_t room)
{
    uint64_t until = hn_ms() + (uint64_t)timeout_ms;
    uint32_t addr;
    if (why && room) why[0] = 0;
    if (hn_start() < 0 || hn_resolve(host, &addr) < 0) return say(why, room, "the name does not resolve", 0), NULL;
    SecureConn *c = calloc(1, sizeof *c);
    if (!c) return say(why, room, "no memory", 0), NULL;
    c->tls = tls;
    c->s = hn_socket(1);
    if (c->s == HN_INVALID) {
        free(c);
        return say(why, room, "no socket", 0), NULL;
    }
    hn_set_flag(c->s, HN_NODELAY, 1);
    if (hn_connect(c->s, addr, (uint16_t)port, left_ms(until)) < 0) {
        hn_close(c->s);
        free(c);
        return say(why, room, "no connection", 0), NULL;
    }
    if (!tls) return c;

    read_roots();
    mbedtls_ssl_init(&c->ssl);
    mbedtls_ssl_config_init(&c->conf);
    mbedtls_entropy_init(&c->entropy);
    mbedtls_ctr_drbg_init(&c->drbg);
    int r = mbedtls_ctr_drbg_seed(&c->drbg, mbedtls_entropy_func, &c->entropy, (const unsigned char *)"bosman", 6);
    if (!r) r = mbedtls_ssl_config_defaults(&c->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT);
    if (!r) {
#ifdef _WIN32
        mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);   /* check_server decides */
#else
        mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
#endif
        mbedtls_ssl_conf_ca_chain(&c->conf, &roots, NULL);
        mbedtls_ssl_conf_rng(&c->conf, mbedtls_ctr_drbg_random, &c->drbg);
        r = mbedtls_ssl_setup(&c->ssl, &c->conf);
    }
    if (!r) r = mbedtls_ssl_set_hostname(&c->ssl, host);
    if (r) {
        say(why, room, "TLS setup failed", r);
        sc_close(c);
        return NULL;
    }
    mbedtls_ssl_set_bio(&c->ssl, c, bio_send, bio_recv, NULL);
    while ((r = mbedtls_ssl_handshake(&c->ssl)) != 0) {
        int read = r == MBEDTLS_ERR_SSL_WANT_READ;
        if (!read && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (r == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) check_server(c, host, why, room);
            else say(why, room, "the TLS handshake failed", r);
            sc_close(c);
            return NULL;
        }
        if (!left_ms(until) || hn_wait(c->s, read, left_ms(until)) <= 0) {
            say(why, room, "the TLS handshake timed out", 0);
            sc_close(c);
            return NULL;
        }
    }
    if (check_server(c, host, why, room) < 0) {
        sc_close(c);
        return NULL;
    }
    return c;
}

long sc_send(SecureConn *c, const void *p, size_t n)
{
    if (!c->tls) {
        long r = hn_send(c->s, p, n);
        return r >= 0 ? r : hn_error() == HN_WOULDBLOCK ? -1 : -2;
    }
    int r = mbedtls_ssl_write(&c->ssl, p, n);
    if (r >= 0) return r;
    return r == MBEDTLS_ERR_SSL_WANT_WRITE || r == MBEDTLS_ERR_SSL_WANT_READ ? -1 : -2;
}

long sc_recv(SecureConn *c, void *p, size_t n)
{
    if (!c->tls) {
        long r = hn_recv(c->s, p, n);
        return r >= 0 ? r : hn_error() == HN_WOULDBLOCK ? -1 : -2;
    }
    int r = mbedtls_ssl_read(&c->ssl, p, n);
    if (r > 0) return r;
    /* A server that ends with no close_notify ends the stream too (Connection: close). */
    if (r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == MBEDTLS_ERR_SSL_CONN_EOF) return 0;
    return r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE ? -1 : -2;
}

int sc_send_all(SecureConn *c, const void *p, size_t n, int timeout_ms)
{
    const unsigned char *b = p;
    uint64_t until = hn_ms() + (uint64_t)timeout_ms;
    while (n) {
        /* mbedTLS: after "would block", the next write must give the same bytes again. */
        long r = sc_send(c, b, n);
        if (r > 0) {
            b += r, n -= (size_t)r;
            continue;
        }
        if (r == -2 || !left_ms(until) || sc_wait(c, 0, left_ms(until)) <= 0) return -1;
    }
    return 0;
}

int sc_wait(SecureConn *c, int read, int ms)
{
    if (c->tls && read && (mbedtls_ssl_get_bytes_avail(&c->ssl) || mbedtls_ssl_check_pending(&c->ssl))) return 1;
    return hn_wait(c->s, read, ms);
}

void sc_close(SecureConn *c)
{
    if (!c) return;
    if (c->tls) {
        mbedtls_ssl_close_notify(&c->ssl);             /* one try: the socket does not wait */
        mbedtls_ssl_free(&c->ssl);
        mbedtls_ssl_config_free(&c->conf);
        mbedtls_ctr_drbg_free(&c->drbg);
        mbedtls_entropy_free(&c->entropy);
    }
    hn_close(c->s);
    free(c);
}
