/* The secure connection (0.7.0 slice 5, #45): a TCP connection with TLS, for the lobby's https:// calls
 * (lobby_http.c) and the relay's wss:// connections (websocket.c). The same calls also give a plain
 * connection, for the local copy of the server in tests (http://, ws://).
 *
 * TLS is mbedTLS 3.6 (the LTS branch, Apache-2.0), built from its release by CMake on Mac, Windows and Linux.
 * It checks the server's certificate against the system's own root certificates (hn_trust_roots) and the
 * host name. On Windows, a chain whose root the ROOT store does not hold yet goes to the system's own check
 * (hn_verify_server), which can fetch that root. The test variable RECOMP_TEST_TLS_CA names a PEM file of
 * one more root, for a test server with its own certificate.
 *
 * Each connection is used by one thread at a time. Every call returns at once except sc_open and
 * sc_send_all, which wait at most their timeout. */
#ifndef SECURE_H
#define SECURE_H

#include <stddef.h>

typedef struct SecureConn SecureConn;

/* Connects to host:port, with TLS when tls is 1. NULL on failure, with a short reason in why (room bytes). */
SecureConn *sc_open(const char *host, unsigned port, int tls, int timeout_ms, char *why, size_t room);
/* > 0: the bytes sent. -1: would block. -2: the connection is broken. */
long sc_send(SecureConn *c, const void *p, size_t n);
/* > 0: the bytes read. 0: the peer closed. -1: would block. -2: the connection is broken. */
long sc_recv(SecureConn *c, void *p, size_t n);
/* Sends all n bytes within timeout_ms. 0 on success. */
int sc_send_all(SecureConn *c, const void *p, size_t n, int timeout_ms);
/* 1 when sc_recv (read 1) or sc_send (read 0) can go on within ms, 0 when not, -1 on failure. */
int sc_wait(SecureConn *c, int read, int ms);
void sc_close(SecureConn *c);

#endif
