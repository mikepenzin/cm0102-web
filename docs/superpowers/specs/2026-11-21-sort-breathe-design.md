# The page breathes inside the game's sort (short design)

Approved by Max, 21 November 2026.

## Intent

The page freezes while the game works without asking for input. Max chose the cheaper path over moving the engine
into a Web Worker: let the game give the page a moment inside its long work, where it does not yet.

## What the measurement showed

The real engine (live build mwjsduvk, the base game files) in Chromium: a new game, then a holiday of two months.
Chrome's long tasks:

- game days (the holiday): none over 0.17 s; the game's random function (0x90cfc0) already lets the page breathe
  every 100 ms (`rt_web_breathe`, commit 21fde70);
- the new game: 1.8 s and 0.8 s. A CPU profile puts both in `PlayerStats::PlayerStats` (0x7bfd50, called from
  `create_game` 0x828110), 65 to 78% of the samples in the C library's qsort (0x945333) with the comparators
  0x7c6d90 and 0x7c6de0. qsort calls no random, so the page got no moment;
- left as they are: a redraw of 0.47 s and a step of 0.37 s.

## Design

- Wrap qsort's swap (0x9454d5: `swap(a, b, width)`, a byte loop, called by the partition and by the short sort, so in
  every sort, large or small) with `RT_WRAP`.
- In the browser build only, every 1024th call lets the page have a moment if it had none for 100 ms: the same
  `rt_web_breathe` as the random function. The native build calls the original and nothing else.
- Nothing of the game changes; only time passes.
- Added after the first measurement (Max had said to go ahead): swaps alone left 0.42 s, a list nearly in order is
  compared much and swapped little. PlayerStats' two comparators (0x7c6d90 by club, 0x7c6de0 by staff: cdecl(a, b) to
  -1, 0 or 1) count as sort steps too, one counter with the swap.

## Result (21 November 2026)

JSPI engines built here (Emscripten 7.0.1) from the commit before and with the change, the same new game and holiday,
`-seed 7`:

| Engine | Long tasks over 1 s | over 300 ms | Longest in PlayerStats |
|---|---|---|---|
| before | 1 | 3 | 1.43 s |
| swap only | 0 | 3 | 0.43 s |
| swap and comparators | 0 | 1 | 0.19 s |

The one left over 300 ms is 0.37 s at the leagues screen (not this change's). The dump of the first news screen is
the same in all three (hash 4257912697). Native: `smoke_run.py --same-save --days 3 --ref-build <before>` says
"ref-build: the save is the same as normal"; its "rules-on" step fails the same way on the build before the change.

## Approaches

- **A Web Worker for the engine:** the page never freezes, but drawing, input, the file system, the vault and the
  cloud sync all cross a thread boundary: weeks, and it puts what works on the iPhone at risk. Not now.
- **Breathe at qsort's entry:** fewer calls, but one large sort would still freeze. The swap covers both.
- **Breathe in the sort's swap** (chosen): one small wrapper, the same mechanism as the random function.

## Checks

- The same measurement after a rebuild: no long task over about 0.3 s in the new game's PlayerStats step.
- `-seed 7`: the game's dump of its first news screen is the same as before (hash 4257912697).
- Max: a new game on the iPhone.
