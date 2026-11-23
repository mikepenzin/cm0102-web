/* Invite links (0.7.0 slice 8, #45, docs/specs/2026-10-27-online-play-0.7.0-spec.md).
 *
 * The invite link page of the lobby (server/src/join.ts) has a button that opens cm0102://join/<code>. The
 * computer starts the game with that link:
 * - Windows and Linux give it as an argument of the program (main.c takes it off the game's command line). When
 *   a copy of the game runs already, the new copy gives the link to it and ends (host_link_pass, Q41), and the
 *   copy that runs gets it in its event poll (user32.c).
 * - macOS sends it as an Apple event, also to a game that runs. SDL gives it as a drop event (user32.c).
 * The runtime looks up the game code in the lobby and gives the game it found to invite_join, which joins it
 * through the original's own screens (invite.c). */
#ifndef INVITE_H
#define INVITE_H

#include "lobby.h"

#include <stdint.h>

/* 1 when text is an invite link, "cm0102://join/<code>" with an optional "/" at the end, and code then holds
 * the game code in capitals (9 bytes). The scheme and "join" may be in any case. */
int invite_parse(const char *text, char code[9]);

/* A link from the command line or a drop event. An invite link starts a lookup of its game in the lobby, when
 * Online play is on. Any other text does nothing. */
void invite_open(const char *text);

/* Each pass of the game's event poll: when the lookup has an answer, it is logged, and a game that is open goes
 * to invite_join. Cheap while no lookup runs. */
void invite_poll(void);

/* The last game that an invite link found, or NULL. For tests and logs. */
const LobbyRow *invite_last(void);

/* Join this network game, as a click on its row in the game list (see invite.c). Called once for each invite link,
 * on the game's thread. */
void invite_join(const LobbyRow *row);

/* The inputs of the screens on the way (invite.c), for a call with no widget clicked: the command word to set for
 * that call, or 0. */
enum { INVITE_SETUP, INVITE_NETWORK_PLAY };
uint16_t invite_command(int screen);
/* Calls the input with the command word 0xdd6bde set to command, and gives the word its old value back
 * (net_password.c, which keeps that word for the join again of slice 6). */
void invite_call(void (*original)(void), uint16_t command);
/* The player clicked a widget on one of the screens on the way: the join of the link ends. */
void invite_clicked(void);
/* The game list's input with no widget: 1 when out holds the Join text, and the input must take Next. */
int invite_list_join(char *out, unsigned room);

/* At start: write the cm0102:// link handler of this computer when Online play is on, and remove it when it is
 * off (host_link_handler, host.h). A test run (RECOMP_USER_DIR set) touches nothing. */
void invite_register(void);

#endif
