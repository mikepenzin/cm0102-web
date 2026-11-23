/* The up of a short mouse press waits for the game (press_hold.h, 0.7.2 tactics click). */
#include "press_hold.h"

#include <stdio.h>
#include <string.h>

static int failed;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failed = 1; } } while (0)

int main(void)
{
    PressHold h;

    /* A tap: the down and the up in one batch, before the game took the down. The up waits. */
    memset(&h, 0, sizeof h);
    press_hold_down(&h, 11);                   /* the down is message 10, the queue count after it is 11 */
    CHECK(press_hold_up(&h, 1000));
    CHECK(!press_hold_due(&h, 1000));
    press_hold_poll(&h, 10);                   /* a poll before the pump took the down: it does not count */
    CHECK(h.polls == 0);
    press_hold_poll(&h, 11);                   /* the poll after the pump: the hit test sees the button */
    CHECK(!press_hold_due(&h, 1001));
    press_hold_poll(&h, 11);
    CHECK(press_hold_due(&h, 1002));           /* two polls: the up goes */
    CHECK(!press_hold_due(&h, 1003));          /* once */
    press_hold_poll(&h, 11);
    CHECK(h.polls == 2);                       /* the press ended: polls no longer count */

    /* A normal click: the game polled twice before the up. The up goes at once. */
    memset(&h, 0, sizeof h);
    press_hold_down(&h, 5);
    press_hold_poll(&h, 5);
    press_hold_poll(&h, 5);
    CHECK(!press_hold_up(&h, 2000));
    CHECK(!press_hold_due(&h, 2000));

    /* The game does not poll (a long stretch of work): the up goes PRESS_HOLD_LONGEST ms after it came. */
    memset(&h, 0, sizeof h);
    press_hold_down(&h, 7);
    CHECK(press_hold_up(&h, 3000));
    CHECK(!press_hold_due(&h, 3000 + PRESS_HOLD_LONGEST - 1));
    CHECK(press_hold_due(&h, 3000 + PRESS_HOLD_LONGEST));

    /* An up with no down of the real mouse (a key click took the down): it goes at once. */
    memset(&h, 0, sizeof h);
    CHECK(!press_hold_up(&h, 4000));

    /* The queue count wraps. */
    memset(&h, 0, sizeof h);
    press_hold_down(&h, 0xffffffffu);
    press_hold_poll(&h, 0xfffffffeu);
    CHECK(h.polls == 0);
    press_hold_poll(&h, 0u);
    CHECK(h.polls == 1);

    if (!failed) printf("press_hold: all checks passed\n");
    return failed;
}
