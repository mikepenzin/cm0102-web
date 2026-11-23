/* The texts of network failures (0.7.0 slice 7, #45): see net_errors.h. Each text says what went wrong, then what
 * to do next. The words follow GLOSSARY.md ("Network play"). */
#include "net_errors.h"

#include <stdio.h>
#include <string.h>

#define ON_YOUR_NETWORK "Games on your own network still work."

void net_error_text(char *out, size_t room, NetError e, const char *version, const char *host_version, int code)
{
    if (!room) return;
    if (!version) version = "";
    if (!host_version) host_version = "";
    switch (e) {
    case NE_LOBBY_DOWN:
        snprintf(out, room, "The lobby, which lists the games online, does not answer. Check your internet connection, "
                 "or try again later. " ON_YOUR_NETWORK);
        break;
    case NE_FULL_TODAY:
        snprintf(out, room, "Online games are full today, try again later. They open again at 00:00 UTC. "
                 ON_YOUR_NETWORK);
        break;
    case NE_RELAY_DOWN:
        snprintf(out, room, "The relay, which carries the games online, does not answer. Check your internet connection, "
                 "or try again later. " ON_YOUR_NETWORK);
        break;
    case NE_GAME_GONE:
        snprintf(out, room, "This network game is no longer in the list. Click Refresh and try again.");
        break;
    case NE_HOST_AWAY:
        snprintf(out, room, "The host of this network game is not connected. Try again in a moment.");
        break;
    case NE_HOST_REFUSED:
        snprintf(out, room, "No network game runs at this address now. Click Refresh, or ask the host to start the "
                 "game again.");
        break;
    case NE_CONNECT_TIMEOUT:
        snprintf(out, room, "The host did not answer. Check the address. If it is correct, the host's firewall may "
                 "block Bosman Edition. Ask the host to allow it.");
        break;
    case NE_HOST_UNREACHABLE:
        snprintf(out, room, "This computer cannot reach the host. Check the address and your network connection.");
        break;
    case NE_NO_NETWORK:
        snprintf(out, room, "This computer has no network connection. Connect to a network and try again.");
        break;
    case NE_NO_ANSWER:
        snprintf(out, room, "The host did not answer. It may be busy, or it may run another version of the game. Try "
                 "again in a moment.");
        break;
    case NE_NOT_BOSMAN:
        snprintf(out, room, "The host does not run Bosman Edition %s, so you cannot join its game. Ask the host which "
                 "version it runs.", version);
        break;
    case NE_VERSION:
        snprintf(out, room, "This network game runs Bosman Edition %s, and you have %s. Install Bosman Edition %s to "
                 "join it.", host_version, version, host_version);
        break;
    case NE_PASSWORD:
        snprintf(out, room, "The password of this network game is wrong. Ask the host for the game password.");
        break;
    case NE_ATTEMPTS:
        snprintf(out, room, "Too many wrong passwords for this network game. Wait one minute and try again.");
        break;
    case NE_GAME_FULL:
        snprintf(out, room, "This network game has 16 human managers. It cannot take more. Join another game.");
        break;
    case NE_NAME:
        snprintf(out, room, "The lobby did not accept the name of this computer. Change the computer's name and try "
                 "again.");
        break;
    case NE_JOIN_OTHER:
        snprintf(out, room, "The connection to the host failed (error %d). Check your network connection and try "
                 "again.", code);
        break;
    case NE_HOST_LEFT:
        snprintf(out, room, "The host left the game. You can join it again when the host starts it again.");
        break;
    case NE_CONNECTION_LOST:
        snprintf(out, room, "The connection to the host was lost. Your club is on holiday. Check your connection, "
                 "then join the game again to take your club back.");
        break;
    case NE_KICKED:
        snprintf(out, room, "The host removed you from the game. Your club is on holiday. Ask the host before you "
                 "join again.");
        break;
    case NE_CREATE_PORT:
        snprintf(out, room, "Another program on this computer uses port 6667, which network games need. It may be a "
                 "second copy of the game. Close it and try again.");
        break;
    case NE_CREATE_OTHER:
        if (code)
            snprintf(out, room, "The network game could not start on this computer (error %d). Check your network "
                     "connection, and check that your firewall allows Bosman Edition.", code);
        else
            snprintf(out, room, "The network game could not start on this computer. Check your network connection, "
                     "and check that your firewall allows Bosman Edition.");
        break;
    default:
        out[0] = 0;
    }
}

NetError net_error_of_connect(int code)
{
    switch (code) {
    case 0: return NE_NONE;
    case 10060: return NE_CONNECT_TIMEOUT;     /* WSAETIMEDOUT */
    case 10061: return NE_HOST_REFUSED;        /* WSAECONNREFUSED */
    case 10051:                                /* WSAENETUNREACH */
    case 10064:                                /* WSAEHOSTDOWN */
    case 10065: return NE_HOST_UNREACHABLE;    /* WSAEHOSTUNREACH */
    case 10050: return NE_NO_NETWORK;          /* WSAENETDOWN */
    default: return NE_JOIN_OTHER;
    }
}

void net_test_text(char *out, size_t room, NetTest t, int ms)
{
    if (!room) return;
    switch (t) {
    case NT_RUNNING: snprintf(out, room, "Testing the lobby and the relay ..."); break;
    case NT_OFF: snprintf(out, room, "Online play is off. Turn it on to join and host games online."); break;
    case NT_NO_ADDRESS: snprintf(out, room, "This copy of the game has no lobby address. Online games cannot work."); break;
    case NT_WORKS: snprintf(out, room, "Online play works. The lobby answered in %d ms.", ms); break;
    case NT_LOBBY_DOWN:
        snprintf(out, room, "The lobby does not answer. Check your internet connection. " ON_YOUR_NETWORK);
        break;
    case NT_LOBBY_LIST_DOWN:
        snprintf(out, room, "The lobby answers, but its list of games does not. Try again later. " ON_YOUR_NETWORK);
        break;
    case NT_FULL_TODAY:
        snprintf(out, room, "Online games are full today. They open again at 00:00 UTC. " ON_YOUR_NETWORK);
        break;
    case NT_RELAY_DOWN:
        snprintf(out, room, "The lobby works, but the relay does not answer. Try again later. " ON_YOUR_NETWORK);
        break;
    case NT_RELAY_BLOCKED:
        snprintf(out, room, "A firewall or proxy on your network blocks the relay, so you cannot join games online "
                 "here. " ON_YOUR_NETWORK);
        break;
    default: out[0] = 0;
    }
}

/* The LobbyHostState values of lobby.h, which this file does not include. */
enum { HS_NONE, HS_REGISTERING, HS_OPEN, HS_REFUSED, HS_FULL_TODAY, HS_UNAVAILABLE, HS_CLOSED };

const char *net_host_text(char *out, size_t room, int state, const char *code, const char *refusal)
{
    if (!room) return NULL;
    if (!refusal) refusal = "";
    switch (state) {
    case HS_REGISTERING: snprintf(out, room, "Your game goes into the lobby ..."); break;
    case HS_OPEN: snprintf(out, room, "Your game is in the lobby. Game code %s", code ? code : ""); break;
    case HS_FULL_TODAY: snprintf(out, room, "Your game is on the LAN only: online games are full today."); break;
    case HS_UNAVAILABLE: snprintf(out, room, "Your game is on the LAN only: the lobby does not answer."); break;
    case HS_REFUSED:
        if (!strcmp(refusal, "name_not_allowed"))
            snprintf(out, room, "Your game is on the LAN only: the lobby did not accept its name.");
        else if (!strcmp(refusal, "too_many_games"))
            snprintf(out, room, "Your game is on the LAN only: your connection already has 2 online games.");
        else if (!strcmp(refusal, "game_already_open"))
            snprintf(out, room, "Your game is on the LAN only: this game is already open online.");
        else if (!strcmp(refusal, "lobby_full"))
            snprintf(out, room, "Your game is on the LAN only: the lobby is full. Try again later.");
        else snprintf(out, room, "Your game is on the LAN only: the lobby refused it (%s).", refusal);
        break;
    default: return NULL;
    }
    return out;
}

const char *net_invite_link(char *out, size_t room, const char *lobby_address, const char *code)
{
    if (!room) return NULL;
    out[0] = 0;
    if (!lobby_address || !lobby_address[0] || !code || !code[0]) return NULL;
    size_t n = strlen(lobby_address);
    while (n && lobby_address[n - 1] == '/') n--;
    int w = snprintf(out, room, "%.*s/join/%s", (int)n, lobby_address, code);
    if (!n || w < 0 || (size_t)w >= room) {
        out[0] = 0;
        return NULL;
    }
    return out;
}
