/* The mbedTLS settings of Bosman Edition (secure.c), read after mbedTLS's own default settings
 * (MBEDTLS_USER_CONFIG_FILE, set in CMakeLists.txt).
 *
 * TLS 1.2 only. mbedTLS 3.6 does TLS 1.3 through its PSA crypto layer, whose key store is shared by all
 * threads and is not safe without MBEDTLS_THREADING_C. The lobby thread and the relay threads each make
 * connections at the same time. TLS 1.2 keeps all state in each connection. Cloudflare serves TLS 1.2. */
#undef MBEDTLS_SSL_PROTO_TLS1_3
#undef MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
#undef MBEDTLS_USE_PSA_CRYPTO

/* A client only, no DTLS, no mbedTLS sockets or timers (the runtime has its own, host_net.h). */
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_SSL_PROTO_DTLS
#undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
#undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
#undef MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
#undef MBEDTLS_SSL_DTLS_CONNECTION_ID
#undef MBEDTLS_SSL_DTLS_SRTP
#undef MBEDTLS_SSL_COOKIE_C
#undef MBEDTLS_SSL_TICKET_C
#undef MBEDTLS_SSL_CACHE_C
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_SELF_TEST
