/* The rows of the game list (0.7.0 slice 4, #45): the LAN games and the lobby's games, one row for each host.
 *
 * The game list (setup_build_select_network_game_screen 0x81f8d0) fills from the UDP datagrams that
 * Network::receive_packet 0x77af20 reads (research note docs/research/2026-10-27-network-code-reading.md,
 * section 2): an ACK adds a row with the sender's address (0x77b10d) and the game name of bytes 69..118
 * (0x77b12b), and a BYE removes the row of its sender (0x77b335). This module sits under those reads. It
 * changes no packet layout: it drops some datagrams and adds others in the 119-byte layout of the original.
 *
 * One row for each host. A host sends one ACK from each of its adapters that the joiner's request reaches
 * (section 11, finding 2). The ACK holds the host's computer name (bytes 19..68, from gethostname 0x77aa77) and
 * the game name, which together name the host's game. The first ACK of a game whose sender is on the subnet of
 * this computer's default route goes to the game at once. Any other first ACK waits HOLD_MS for a better one.
 * Later ACKs of the same game from another address are dropped, and so are BYEs from an address that has no
 * row (the original adds a row for a BYE that matches none, 0x77b448).
 *
 * Lobby rows. Each lobby game that is not on the LAN is an ACK from an address in 0.66.0.0/16 (LR_NET), which
 * no real host has (0.0.0.0/8 is "this network"). The row text is the game name with the season, the day and
 * the managers online. A connect to any address in 0.66.0.0/16 goes to lr_row_of (net_lobby.c) and never to
 * the system.
 * A lobby game is the same as a LAN game when its hostName is lr_host_key of the ACK's computer name and the
 * names match. Lobby rows go to the game LOBBY_WAIT_MS after the request at the earliest, so that the LAN
 * answers come first. When the lobby answers "full today", one row says so. When it gives no answer, one row says
 * that (0.7.0 slice 7). */
#ifndef LOBBY_ROWS_H
#define LOBBY_ROWS_H

#include "lobby.h"

#include <stdint.h>

#define LR_TAG_LEN 19
#define LR_DATAGRAM 119              /* 19 + 50 + 50 (0x77aa7c..0x77aafd) */
#define LR_NAME_AT 69
#define LR_HOST_AT 19
#define LR_NET 0x00420000u           /* 0.66.0.0, host order */
#define LR_FULL_ROW 0x0042ffffu      /* 0.66.255.255: the "full today" row */
#define LR_GONE_ROW 0x0042fffdu      /* 0.66.255.253: no row, so a join gives "no longer in the list" (invite.c). It is
                                       * not a relay peer: those are 0.66.255.0 to .15 */
#define LR_RELAY_PEERS 0x0042ff00u   /* 0.66.255.<channel>: a human manager through the relay, on the host (relay.h) */
#define LR_DOWN_ROW 0x0042fffeu      /* 0.66.255.254: the "lobby does not answer" row (0.7.0 slice 7) */
#define LR_HOLD_MS 300
#define LR_LOBBY_WAIT_MS 400

/* The game sorts its list by a plain byte compare of the names (0x77b1fc..0x77b222): the leading space puts
 * this row first. */
#define LR_FULL_TEXT " Online games are full today, try again later"
#define LR_DOWN_TEXT " The lobby does not answer, try again later"
/* The most rows that the game takes: the cap at 0x77b0eb, which tools/recomp/game_list.py raises from 9 to 50
 * (slice 4a). This constant must follow it. */
#define LR_CAP 50

/* 1 when the datagram has the tag ("CM3 LIST SERVER REQ", "...ACK", "...BYE"). */
int lr_is(const uint8_t *p, long n, const char *tag);

/* The lobby's hostName of a host: "PC-" and 8 hex digits of a hash of its computer name (bytes 19..68 of its
 * ACK). The lobby shows no computer name. */
void lr_host_key(char out[12], const char *computer_name);

/* The game sent a REQ: its list is empty again (the count resets at 0x81f913, 0x81fc30 and 0x81fde9). */
void lr_request(void);
/* A datagram that the game is about to read, from addr (network order). 0: drop it. */
int lr_arrived(const uint8_t *p, long n, uint32_t addr);
/* A datagram to give the game in place of a real one: its size, or -1. */
long lr_extra(uint8_t *buf, uint32_t room, uint32_t *addr);
/* The lobby's answer for the last request: rows and an LhResult. */
void lr_lobby(const LobbyRow *rows, int n, int result);
/* The lobby row of a joiner's connect to addr (network order): the row, or NULL when addr is not a lobby row.
 * *full is 1 for the "full today" row and 2 for the "lobby does not answer" row. */
const LobbyRow *lr_row_of(uint32_t addr, int *full);

/* The text of the row that the game list shows now for the lobby game with this code: its lobby row, or its
 * LAN row when the host is on this computer's LAN. 0 when the list shows no such row. For invite links
 * (invite.c), which put this text in the Join field as a click on the row does. */
int lr_shown_text(const char *code, char out[50]);
/* The LhResult of the lobby's last answer to the game list. */
int lr_lobby_result(void);

/* Tests: the adapters (network order) and the default route; else the system's own. */
void lr_set_adapters(const uint32_t *addr, const uint32_t *mask, int n, uint32_t route);

#endif
