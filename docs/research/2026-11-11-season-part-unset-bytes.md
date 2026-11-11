# Unset bytes in the season part

Date 2026-11-11. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Code: `src/recomp_rt/win/season_change.c`, `src/recomp_rt/win/season_friendly_dates.c`, `src/recomp_rt/win/season.c`.

## Result

- Two runs with the same clicks wrote different season parts (`bosman_season.dat`). Each changed name in the part is a field of 51 bytes. The runtime set the name and its 0 and left the rest of the field as the host heap held it. In a native run those bytes held pieces of host folder and file names ("gamedir-check", "savedit-try.mjs", "build"), right after names such as "Spanish Cup", "Olympic Games", "African Cup of Nations" and "Conference League". About 150 bytes of the part differed between two runs.
- The fix sets these bytes to 0. A load also drops the bytes after each name's 0, so a save of an older build loses them at its next save.
- A second fault in the same writer: `season_friendly_part_write` wrote its count of 4 bytes when no qualifier was waiting, but `season_friendly_part_size` counted 0 bytes for that list. The write went 4 bytes past the end of the part's buffer (`add_part` in `season.c` allocates exactly the part's size). Those bytes never reached the file, because only the part's size is stored. The fix writes nothing for an empty list.
- The game's own data does not change. The original never reads the season part (section 3), and the runtime reads each name only up to its 0 (section 2).

## 1. The bytes

`SeasonRename` in `season_change.c` holds `u32 index` and `char original[51]`, the competition's name in the data. `season_change_rename` grows the array with `realloc` (16 rows, then 32, 64 and so on) and fills a new row with `snprintf(original, 51, ...)`, which writes the name and its 0 and nothing after them. `realloc` does not clear new rows: they hold whatever the host program freed there before. `season_change_part_write` copies all 51 bytes of each row into the part (`memcpy(out + at + 4, renames[i].original, 51)`).

The load (`season_change_part_read`) copied all 51 bytes from the file and set only byte 50 to 0. So the host bytes of one save went on into every later save of that game.

## 2. Who reads a changed name

`original` is read only as a 0-ended string:

- The wrap of `match_club_comp_key_id` 0x60efd0 (`RT_WRAP(0060efd0, ...)`, `season_change.c`) copies it into game memory with `win_put_str`, which copies the bytes up to the 0 and then a 0. So the original's 0x60efd0 gets the same string with or without the fix, and no byte after the 0 ever reached game memory. 0x60efd0 compares the name it gets with fixed names through `_stricmp`, called for each record by `set_key_ids` 0x601a40 in the loop at 0x60425d (`2026-10-17-season-change.md`, "A changed name must not break a load"; `function-map/index.cpp.md`).
- `season_change_data_name` returns it as a string. Its callers in `season_comps.c`, `season_uefa.c` and `season_bra_states.c` compare or print it as a string.
- The season part, through `season_change_part_write`.

## 3. The original never reads the season part

From `2026-10-17-season-save-part.md`, sections 2 and 3:

- `zopen` 0x9326e0 opens a part by its exact name (the name scan at 0x93293b..0x932981).
- `load_game_data` 0x82a9c0 extracts only the parts whose names match `*.tmp` (`zextract_matching_files` 0x933320 at 0x82acbc, matcher 0x933a00) and opens other parts by name. `bosman_season.dat` is neither.
- The runtime adds the part on the host side after `save_game_data` 0x82e430 has returned 1, and reads it on the host side before 0x82a9c0 runs. Neither step touches the game's memory.

The original's code was not read again for this note: the container of this session had no clean files. The addresses come from the two notes named above, which read it.

## 4. The other fields of the part

Every other field that `season_change_part_write` and `season_friendly_part_write` write is set before the write:

| Field | Set by |
|---|---|
| Header: version, length, start year, changing rules (16 bytes) | `add_part` in `season.c`, each with `put32` |
| News count; each news item's season, posted flag and length | `news_new` sets every field; `put32` writes them |
| News text, `length` bytes | `news_append`; exactly `length` bytes are copied |
| Rename count and record index | `season_change_rename`, and now the whole row is cleared first |
| Last season point | `season_point` |
| Friendly list: count, then record id, home, away, year, day, stage end year and day (20 bytes) | `season_fifa_snap_fixture` sets every field of a `Pending` in one compound literal; the write copies field by field, so no struct padding reaches the part |

The row of the part in the container header is cleared with `memset` before it is filled (`add_part`).

## 5. The fix

- `season_change_rename`: clears the new row (`memset`) before it sets the index and the name.
- `season_change_part_read`: clears each row, then copies the name up to its 0, at most 50 bytes. A save of an older build loads with the same names.
- `season_friendly_part_write`: writes nothing when no qualifier waits. Its size, 0 for an empty list, is unchanged, so the part has the same bytes as before.

## 6. Checks

- A test harness (not in the repo) built the real `season_change.c` and `season_friendly_dates.c` with stubs for the rest. It filled the host heap with host text and a fill byte, freed it, made 150 changed names (the array grows to 256 rows), and wrote the part. It ran twice with other heap contents.
  - Before the fix: 150 of 150 name fields had set bytes after the name, and the two parts differed in 5177 bytes. AddressSanitizer reported a heap-buffer-overflow, a write of 4 bytes in `season_friendly_part_write` called from `season_change_part_write`.
  - After the fix: the two parts are equal, every name field is 0 after its name, and AddressSanitizer reports nothing. A load and save gives the same bytes. A part with host bytes after a name loads with the same name and saves with 0. The part after the fix equals the part before the fix with only the bytes after each name set to 0. With 3 waiting qualifiers the list follows at the end of the part, as before.
- `tests/smoke/smoke_run.py --same-save` (new game) gets the run `rules-on-again`. It plays the game of `rules-on` again and must equal it in every part, the season part included. Before, the season part of `rules-on` was compared only with `rules-off`, where it is an accepted difference, so no run compared two season parts.
- Not run in this session (no clean files in the container): the native runs. Two runs of the same new game saved at the first human turn should give equal saves:

  ```
  python3 tests/smoke/smoke_run.py build/cm0102_recomp $G --same-save --days 0 --runs rules-on-again
  ```

  The run compares `rules-on-again` with `rules-on` part by part (the parts of a save come from `tools/save/savfile.py`'s format). The full `--same-save` check must stay green.

## 7. Unknowns

- Whether the 4 bytes past the end ever broke a save or a later `free`. That depends on how the host allocator rounds the block. No report names it.
