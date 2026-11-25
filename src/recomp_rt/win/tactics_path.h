#pragma once
/* The tactics folder of another computer (the browser fork; tests/tacticspath).
 *
 * At the start of a game the game keeps an absolute path to its tactics folder in each human manager's tactic slots
 * (slot +0x18F1, from FUN_0090bea0("tactics"); docs/research/2026-10-16-tactics-inventory.md), and the saved game
 * carries it. Tactics, Save As writes there (Formation::save_to_disk 0x5b6900) and Load lists it. A game begun on
 * another computer therefore names that computer's folder: "E:\tactics" from the Bosman Edition on a PC whose game
 * folder took the letter E:, "C:\Program Files (x86)\Championship Manager 01-02\tactics" from the original game. Here
 * E: does not exist (the save fails: "could not be saved") and C:\Program Files is outside the game folder (the tactic
 * would land where Saved games and Cloud Saves do not look).
 *
 * tactics_path_of_other_machine: for a full Windows path ("X:\a\b", as full_path in files.c makes it) on a drive other
 * than the game folder's, whose last part, or the part before the last, is a folder named tactics (any case), the
 * same folder or file in the game folder's tactics folder: "<game drive>:\tactics[\<name>]". 1 with out filled, else
 * 0 (out unchanged in meaning). */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static inline int tactics_path_of_other_machine(const char *path, char game_drive, char *out, size_t room)
{
    if (!isalpha((unsigned char)path[0]) || path[1] != ':' || toupper((unsigned char)path[0]) == toupper((unsigned char)game_drive))
        return 0;
    /* The last two parts: "...\<folder>\<name>" or "...\<folder>". */
    const char *last = strrchr(path, '\\');
    if (!last || !last[1]) return 0;
    const char *name = last + 1, *folder = NULL;
    size_t folder_len = 0;
    if (!strcasecmp(name, "tactics")) {
        folder = name;
        folder_len = strlen(name);
        name = NULL;
    } else {
        const char *before = last;
        while (before > path + 2 && before[-1] != '\\') before--;
        folder_len = (size_t)(last - before);
        if (folder_len != 7 || strncasecmp(before, "tactics", 7)) return 0;
        folder = before;
    }
    int n = name ? snprintf(out, room, "%c:\\%.*s\\%s", game_drive, (int)folder_len, folder, name)
                 : snprintf(out, room, "%c:\\%.*s", game_drive, (int)folder_len, folder);
    return n > 0 && (size_t)n < room;
}
