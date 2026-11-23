/* The up of a short mouse press waits for the game (user32.c, 0.7.2, forum report of 13 November 2026: "I
 * sometimes click on tactics and it won't click into it").
 *
 * The window procedure of the original (0x68d630, WM_LBUTTONDOWN at 0x68d9bc, WM_LBUTTONUP at 0x68da03) sets the
 * held flag 0xb67a34 at the down and clears it at the up. Its pump 0x68d520 dispatches every message in the queue
 * at once. The hit test 0x560490 (called from the input poll 0x55f620) gives a widget only while 0xb67a34 is set.
 * The one-shot flag 0xb67a0c of the down is read in the poll only for a fixed click result (D+0x12f501, 0x55f68b),
 * and else cleared. So when the down and the up of one press reach the game in one pump, the poll sees no button
 * and the click does nothing. That happens for a tap of a trackpad (the down and the up come together) and for
 * any click that ends while one frame of the game is long (a screen change, a large redraw, a busy machine).
 *
 * The script clicks of the tests had the same fault (item 154) and wait since then: their up goes to the game
 * only after the poll has run twice with the down in the game. This rule does the same for the real mouse. The
 * up of a press waits until the input poll has run PRESS_HOLD_POLLS times with its down in the game, or until
 * PRESS_HOLD_LONGEST ms after the up came. Two polls, as for the script: a click outside a text entry that has
 * the cursor first ends the entry (0x5617c0 returns the entry's widget at once), and only the next poll gives
 * the down to the button under the pointer. A press that is longer than that is not changed. */
#ifndef PRESS_HOLD_H
#define PRESS_HOLD_H

#include <stdint.h>

#define PRESS_HOLD_POLLS 2
#define PRESS_HOLD_LONGEST 500u

typedef struct {
    int pressed;           /* 1 from the down to the up that goes to the game */
    uint32_t down_at;      /* the queue count after the down: the down is in the game once the head passes it */
    int polls;             /* input polls with the down in the game */
    int up_waits;          /* 1 while the up waits */
    uint64_t up_at;        /* when the up came */
} PressHold;

/* A down went into the queue; tail is the queue count after it. */
static inline void press_hold_down(PressHold *h, uint32_t tail)
{
    h->pressed = 1;
    h->down_at = tail;
    h->polls = 0;
    h->up_waits = 0;
}

/* An input poll ran; head is the queue count of the next message the game takes. */
static inline void press_hold_poll(PressHold *h, uint32_t head)
{
    if (h->pressed && (int32_t)(head - h->down_at) >= 0) h->polls++;
}

/* The up of the press came at now. 1: it waits (press_hold_due gives the moment). 0: it goes now. */
static inline int press_hold_up(PressHold *h, uint64_t now)
{
    if (!h->pressed || h->polls >= PRESS_HOLD_POLLS) {
        h->pressed = 0;
        return 0;
    }
    h->up_waits = 1;
    h->up_at = now;
    return 1;
}

/* 1 when the waiting up goes to the game now; the press then ends. */
static inline int press_hold_due(PressHold *h, uint64_t now)
{
    if (!h->up_waits || (h->polls < PRESS_HOLD_POLLS && now - h->up_at < PRESS_HOLD_LONGEST)) return 0;
    h->up_waits = 0;
    h->pressed = 0;
    return 1;
}

#endif
