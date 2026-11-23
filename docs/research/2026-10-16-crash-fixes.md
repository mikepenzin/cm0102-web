# Research for #46: fixes for rare crashes in the original game

Date: 2026-10-16. Read-only research. Exe: `clean-3968/cm0102.exe`. Main code: memory = file offset + 0x400000 (checked against the PE header: .text VA 0x401000 at file 0x1000).

Decoder scripts (scratchpad, not the repo): `r46tools/decode.py` (hex lists against the exe, with capstone) and `r46tools/dis.py`.

Words: "crash guard" = a test that sends a NULL pointer to a path that does not crash. "Rule change" = a fix that changes a result in a game that would not crash.

## 0. A fact about our port that changes the picture

The original crashes on a NULL pointer because Windows does not map page 0. **Our port maps all 4 GB of guest memory, page 0 too.**

- Mac and Linux: `src/recomp_rt/host/posix.c:20` maps the 4 GB block read-write in one `mmap`.
- Windows: `src/recomp_rt/host/win32.c:37` commits any page of the block on its first access, page 0 included.

So in the port a NULL read gives zeros (or what an earlier NULL write left there), and a NULL write succeeds. The game goes on with wrong data where the original stopped. Only a call through a NULL function pointer stops the port, with `rt_fatal("call to %#x, which is not a recompiled function")` (`src/recomp_rt/rt.c:173`). Inference: most of the crashes below do not crash the port today. They give silent wrong data instead, and a NULL write (for example 0x8c3cad below) puts non-zero bytes at guest address 4, so later NULL reads see garbage.

This matters for the player text "fixes for crashes". The guards turn "goes on with garbage" into "skips the bad item", the same thing they do on Windows for the original.

## 1. Inventory

### 1a. NULL pointer crash sites

Each site reads, writes or calls through a pointer that can be NULL. Each function already has an exit or skip path that a guard can jump to.

| Site name | Site (memory) | Function | Original fault | Guard: exit to use | Clean 3.9.68? | Decisions |
|---|---|---|---|---|---|---|
| News item with no text | 0x4aeb0a | 0x4aeaf0, the "text with links" renderer (comp_text.cpp, see widescreen inventory l.120) | `mov edi,[[ebp-0xa38]]` then `repne scasb` on the text pointer. A NULL text pointer crashes. Cause unknown. | If edi == 0, skip the strlen (jump to 0x4aeb11). | Yes (no data link) | Guard only. Display code. |
| World Cup NULL site | 0x4b07b0 | 0x4b07b0, a sort compare of two teams by `[team+0x80]` | One of the two team pointers is NULL. | If a slot is NULL, copy the other pointer into it and compare again. This can give a duplicate team in the World Cup. | Yes (very rare) | Writes data, but only where the original crashes. |
| Row counter site | 0x8c3cad and 0x8c3e16 | 0x8c39f0, which adds 1 to the counter of the row that bsearch 0x946464 finds | After `call 0x946464` (a lookup; inference: bsearch) the code does `inc word [eax+4]` / `inc word [eax+6]`. Not found = NULL write. | Skip the increment if eax == 0. | Yes. More likely with all leagues active. | Guard only (skips a counter). |
| Competition table site | 0x48ce46 | 0x48ce10, a loop over the comp table at [0xadadfc], calling virtual +0x10 | A NULL comp in the table. `call [edx+0x10]` through NULL. | If esi == 0, return 1 from the loop at once (the rest of the comps are not visited). | Yes | Guard, but it ends the loop early. This one crashes our port too (call to 0). |
| Transfer offer site | 0x8e7bef | 0x8e7b50 | `[esi+0x61]` (player data) is NULL for a staff in an offer. | If NULL, jump to 0x8e7c2d (skip). | Seen in other data sets. Possible on clean (staff without player data). | Guard only. |
| transfer_offer type test | 0x8e8827 | 0x8e8800 | `mov ecx,[eax]` with eax = `[esi+0x53]` NULL. | If NULL, jump to 0x8e8845. | Yes | Guard only. |
| squad_manager site | 0x85a556 | 0x85a470 | `cmp byte [eax+0xf]` with eax = `[esi+0x61]` NULL. | If NULL, jump to 0x85a745 (the function's existing skip path). | Yes | Guard only. |
| staff_records sites | 0x86d606 and 0x8c94c5 | 0x86cd70 and 0x8c9390 | A part-exchange player whose contract ended before he moved has club `[+0x39]` NULL. | If NULL, jump to 0x86d6ae / 0x8c94ec (skip). | Yes (a normal game event) | Guard only. |
| Netherlands site (the guard at 0x5f74ac) | 0x5f74ac | 0x5f73d0 | `call 0x4a4850` returns 0. Inference: linked to the Dutch top division. | If eax == 0, pop and `ret 0x14` from the function. | Unknown. The cause is unknown. | Guard, but it ends the function. Inference: needs a check of what the caller does with no result. |
| virtual_staff 195 | 0x85d8b8 | 0x85d8a0 | `movsx eax,[edx+0x7e]` with edx = `[[ecx+0x20]+0x53]` NULL, after the virtual_staff.cpp 195 assert box. | Use 0 for the value if edx == 0. | Yes | Guard. The value 0 then goes into a sum (fild at 0x85d8c4). |
| transfer_manager | 0x84aebb | 0x84aeb0 | `mov eax,[eax]` on a NULL argument. | If NULL, jump to the function's exit 0x84aef1. | Yes | Guard only. |

### 1b. Faults that change results

| Part | Site | Original fault | Possible fix | Decisions |
|---|---|---|---|---|
| Creativity | 0x6eda39 in 0x6ec010 | For CA >= 125 (`cmp cx,0x7d` at 0x6eda35) and intrinsic creativity > 10, `jge 0x6eda66` skips the store to [esp+0x28], so the value left from before (inference: reflexes) is used. | `jge 0x6eda57`: use the raw intrinsic value. | **Rule change.** It changes every top player's creativity in every match. |
| Marking and position clamp | 0x6ef644 and 0x6ef692 in 0x6eecb0 | Two values (bytes +0x110, +0x111) come from a float with only a floor of 2.0 ([0x9689a8]), then `ftol` to a byte. Large values wrap to small ones. | Clamp before the byte store (100 or 75). | **Rule change** for every value above the clamp, wrapped or not. |
| Cash wrap | 0x5986e3 in 0x5986b0 | Club cash in a signed int wraps above 2,147 million. | If cash <= -1,500,000,000 (signed), set it to 2,000,000,000. | Rule change, but it fires only above 2.1 billion. |
| Argentina points | 0x409d7e in 0x409c20 (not 0x4093f0, see 2026-11-13-crash-hunt-132.md) | Average points, Argentine second division. | `jle` becomes `jmp`: the search loop never runs. | **Rule change on every game** with Argentina active. Not a guard. |

### 1c. Player search

- Site: 0x7afc4b in 0x7afa70, the player search list builder.
- The original adds a player to the results only if the human manager "knows" him: `call 0x5407e0(staff, manager, 0)` returns a score, and the player passes if it is >= 4000 (`cmp ax,0xfa0` at 0x7afc47). Else he passes only if a scout report exists (0x7f8d70), or he is from the same region (`[nation+0x76]`, 0x7afc93) and one of 0x544520 / 0x544590 / 0x76d9b0 is true. Inference: 0x5407e0 is the knowledge score of the world-knowledge system.
- Full search: `jge` becomes `jmp`, so every player passes.
- This is a design limit of the game, not a crash. It does not change the computer's decisions. It shows the human more players. The result list is larger, so the memory layout after a search differs (Q152 counts that like opening a screen).
- Clean 3.9.68 has `mov ecx,0xb74450` at 0x5a4207.

### 1d. Code tied to the 2001 start year

These are not bug fixes. They matter for new start years:

- The staff load (`call 0x54e630` at 0x52c2b0) reads dates of birth and contract dates (`[esi+0x12]`, `+0x18`, `+0x33`, `+0x48`, stride 0x6e). A new start year needs (start year - 2001) added to the years of these dates when the year is >= 1901. The start year is at [0x81b93d].
- The code at 0x7a3cfa, 0x7a3f77, 0x7a41ba, 0x7a47d0, 0x7a4c8c and 0x7a4ee5 uses a fixed two-digit year rule (yy < 10 = 20yy, else 19yy). A new start year needs a rule relative to the start year.
- With start year 2001 nothing changes. Not for 0.5.0.

### 1e. Error boxes

- The calls at 0x90d4bc (virtual_staff.cpp 195) and 0x6dd41c (match_eng 1225) open the error box 0x5e8290. To remove them only hides a message. It is not a fix.

## 2. What our recompile already has

A search of `tools/recomp/` and `src/` for every site above finds only the euro.cfg loop fix (`tools/recomp/widescreen.py:105`, INSNS at 0x905a4c). No crash guard from the table exists. 0x4aeaf0 already has an `RT_WRAP` in `display.c` (widescreen text fix), so that function has a hook.

## 3. The three items that the issue names

1. **A news item with no text.** This is the guard at 0x4aeb0a in the text-with-links renderer 0x4aeaf0. A news or text item with a NULL text pointer crashes the original. In our port it reads guest address 0 and shows an empty or garbage line (inference).
2. **Leap-year date of birth.** I found no code fault for this. The date code in 1d is about start years. What I found in the data:
   - Every date in the game holds its own leap flag (+4). The display functions (0x54a52d, 0x54a70f, 0x54aa08, 0x54cf70 and 8 more) pick the month table by that flag (normal 0x9b8448, leap 0x9b8460).
   - In clean `staff.dat` (169,342 staff), all 29,131 dates of birth in a leap year have flag 0. Their days use the non-leap numbering (highest day 364, never 365). So they show correctly.
   - Day 59 occurs 172 times in leap years against about 86 on the other days. Inference: the database tool stored 29 February as day 59, which shows as **1 March**. So about 86 players born on 29 February show 1 March. Both dates share day 59, so the data cannot tell them apart. A fix needs an outside list of real dates.
   - The 51,353 "unknown" dates (1 Feb 1900) have flag 1. 1900 is not a leap year, but these dates are before March, so the display is not affected.
   - I don't know of a code fault here. A one-day shift of birthdays in leap years of a long game (2004, 2008) comes from day-of-year dates and is how the original works.
3. **Player search.** See 1c. The search misses every player whom the human's manager does not "know": low knowledge score, no scout report, not from the same region. It is a design limit, not a crash.

## 4. Recommendation for 0.5.0

**A. Crash guards (always on, no option).** Each tests for NULL only where the original would read or write through NULL. In a game that does not crash, the test is false and the path is the same. So they cannot change decisions in such a game.

- News item with no text (0x4aeb0a)
- Row counter site (0x8c3cad, 0x8c3e16)
- transfer_offer type test (0x8e8827)
- Transfer offer site (0x8e7bef)
- squad_manager site (0x85a556)
- staff_records sites (0x86d606, 0x8c94c5)
- transfer_manager (0x84aebb)
- virtual_staff 195 (0x85d8b8)
- Competition table site (0x48ce46)

A guard adds a test, so the speed changes a little. It does not change the memory layout.

**B. Guards that need one more check first.**

- World Cup NULL (0x4b07b0): it makes a duplicate team. Recommended: include, because a duplicate is better than a crash.
- Netherlands site (0x5f74ac): the cause is unknown. Recommended: leave it out until its caller is read.

**C. Rule changes (Max decides). Recommended: not in 0.5.0, or as options with the default off.**

- Creativity: changes every match with a top player.
- Marking and position clamp: changes match ratings above the clamp.
- Cash wrap: only above 2.1 billion. Small value.
- Argentina jump: changes every game with Argentina.
- Full player search: no computer decisions change, but memory after a search does. It fits as a `settings.txt` option, default off.

**D. Leave out.** The start-year date code (1d) and the hidden error boxes. The leap-year date of birth needs data the game does not have.

## 5. How to build and test

**How to build.** Use one `INSNS` entry per site in a new `tools/recomp/crashguards.py`, the same way as `widescreen.py:105`. Each entry checks the original instruction text and adds `if (ptr == 0) goto L_exit;` before it. Cost: about 10 short entries and one test file. The news item guard can go in the existing RT_WRAP of 0x4aeaf0 instead.

**Unit-style test (recommended).** The repo can already recompile part of the game and run it with a driver: `tools/recomp/recomp.py --entries ... --runtime ... --compile` (see `tests/recomp/test_load.py`). Do these steps:

1. Recompile only the guarded functions with a test driver. Small functions suit this best: 0x4b07b0, 0x84aeb0, 0x85d8a0, 0x48ce10, 0x8e8800.
2. For each function, put a fake object in guest memory with the bad pointer set to 0.
3. Call the function and check that it goes to the exit path.
4. Fill guest page 0 with 0xCC so that a missed guard shows as a wrong result.
5. Run the same call once with a good pointer and compare against the Unicorn run of the original (`tests/recomp/reference.py`). The result must be identical.

**Forced condition in a real game.**

1. Make a test build option, for example an environment variable that only the test reads, that sets one pointer to NULL.
2. Example: blank the text pointer of the first news item in `start.sav` after load, then open the news screen. The original would crash. The guarded port must show the screen.
3. Run the staff_records case from a save with a part-exchange deal only if such a save exists. I don't know of one.

**Same-save check.** Run `smoke_run.py --same-save` with `start.sav` and `prematch.sav` before and after the guards. Results must be byte-identical, because no guard fires in those games.

## 6. Open decisions for Max

1. Ship the crash guards in group A always on, with no option? Recommended: yes. They act only where the original crashes.
2. World Cup NULL guard (makes a duplicate team)? Recommended: yes, in group A.
3. Rule changes (creativity, marking clamp, cash, Argentina)? Recommended: not in 0.5.0. If wanted, add them as options with the default off, in a later release.
4. Full player search? Recommended: a `settings.txt` option, default off. Write the player text as "Player search can show every player".
5. Leap-year date of birth? Recommended: remove it from the issue text. The fault is in the data (29 Feb stored as day 59, shown as 1 March). The game cannot recover the true date.
6. Change the issue text? Recommended: "Fixes for rare crashes in the original game, such as a crash on a news item and crashes in transfers and squad updates", plus a separate option issue for the full player search.
