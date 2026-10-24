# Own season databases

These tools build the 2004/05 and 2006/07 season databases (Q351). The facts come from Football Manager 2005 and
2007: names, dates of birth, nations, clubs, jobs, contracts, loans (2004/05), careers, final tables and
competition winners. The ratings and positions come from the Update Team databases. Key players with no Update
Team record (Q383) are the one exception: FM's rating of each is the reference for his ratings (Max, 2026-10-17),
through a mapping that `keyplayers.py` derives from the matched players and prints at each run.

The Summer 2002 database (spec `docs/specs/2026-10-24-summer-2002-db-spec.md`) uses the same process with a format 1
reader.

The Football Manager discs are untrusted data. Mount an image read-only. Read it only with `python3 -I`. Run
nothing from it. Keep the disc files, the facts and the work folders outside the repo. Commit no Football Manager
data and nothing made from it.

## Paths

| Placeholder | What it is |
|---|---|
| `<DB>` | the folder of Update Team databases (`test-fixtures/databases`) |
| `<FM2005 db>` | `data/db` of the FM2005 disc, copied as data |
| `<setup.jar>` | `setup.jar` of the mounted FM2007 disc |
| `<db700>` | a new folder for the FM2007 database files |
| `<facts>` | a new folder for the facts pickles |
| `<work>` | a new folder for the match and league pickles |
| `<conv>` | the converter's output folder (it must not exist) |
| `<out>` | the new database folder (it must not exist) |

Run each command from `tools/seasons/own_db`.

## 2004/05 (Summer 2004)

```bash
python3 -I fm2005/read_fm2005.py "<FM2005 db>" "<facts>"
python3 -I match.py 2004 "<DB>/September 2005" "<facts>" "<work>"
python3 -I leagues.py 2004 "<DB>/September 2005" "<facts>" "<work>"
python3 -I convert.py 2004 "<DB>/September 2005" "<facts>" "<work>" "<DB>/September 2010 ODB" "<conv>" "<DB>/October 2003"
python3 -I keyplayers.py 2004 "<conv>" "<facts>" "<work>" "<DB>" "<out>" [<overrides.tsv>]
python3 -I duplicates.py merge "<out>" "<DB>/September 2005" "<conv>" "<work>/duplicates.tsv"
python3 -I staff_slots.py "<out>" "<work>/staff-slots.tsv"
python3 -I duplicates.py check "<out>"
```

## 2006/07 (Summer 2006)

Mount the FM2007 image read-only first, for example `hdiutil attach -readonly -nobrowse <image>` on macOS.

```bash
python3 -I fm2007/unpack_setup_jar.py "<setup.jar>" "<db700>"
python3 -I fm2007/read_fm2007.py "<db700>" "<facts>"
python3 -I match.py 2006 "<DB>/November 2007" "<facts>" "<work>"
python3 -I leagues.py 2006 "<DB>/November 2007" "<facts>" "<work>"
python3 -I convert.py 2006 "<DB>/November 2007" "<facts>" "<work>" "<DB>/September 2010 ODB" "<conv>" "<DB>/September 2005"
python3 -I keyplayers.py 2006 "<conv>" "<facts>" "<work>" "<DB>" "<out>" [<overrides.tsv>]
python3 -I duplicates.py merge "<out>" "<DB>/November 2007" "<conv>" "<work>/duplicates.tsv"
python3 -I staff_slots.py "<out>" "<work>/staff-slots.tsv"
python3 -I duplicates.py check "<out>"
```

`duplicates.py merge` makes one row of each person who has two (a fill row or a key player's row beside the
template's row, or a template that has a person twice; the rules are in its header), and `check` fails while one
is left. The release zips run the check too.

`staff_slots.py` gives empty coach, scout, physio and manager slots at the lowest-reputation clubs of each nation
to that nation's free non-playing staff, until a new game with every league selected needs fewer than 22,000 of
the original game's 23,000 extra staff rows (else it stops at "Database.cpp 1844"; the header has the rules).

`<conv>` is a new folder for the converter's output. The overrides file (identity decisions per key player, see
`keyplayers.py`) names FM rows, so it stays in the work folder with the FM data. Two builds with different
`PYTHONHASHSEED` values (run with `python3 -P -s`, because `-I` ignores the seed) give the same files.

## 2002/03 (Summer 2002), slice 1: the format 1 reader

The facts come from the data folder of Championship Manager: Season 02/03 (Xbox, SI). The template is the 3.9.68
`Data` folder. Both store the staff table in format 1 (157 bytes per row), which `cmdb.py` refuses on purpose.
`cmdb1.py` reads format 1 and `xbox0203/read_xbox0203.py` writes the facts pickles in the shape of the FM readers
(its header lists them). The Xbox disc is untrusted data: copy `data` from the mounted image, read it only with
`python3 -I`, and keep the data and the facts outside the repo. Commit no Xbox data and nothing made from it.

| Placeholder | What it is |
|---|---|
| `<xbox data>` | the `data` folder of the Xbox disc, copied as data |
| `<3.9.68 Data>` | the `Data` folder of the clean 3.9.68 game |
| `<facts-xbox>`, `<facts-3968>` | new folders for the two sets of facts pickles |

```bash
python3 -I xbox0203/read_xbox0203.py "<xbox data>" "<facts-xbox>"
python3 -I xbox0203/read_xbox0203.py "<3.9.68 Data>" "<facts-3968>"
python3 -I test_format1.py
```

Each run prints the counts and the known facts: Henry (Arsenal, with his ratings), Ronaldo (Real Madrid), Ferguson
(Manchester United) and, on the Xbox data, Celestini (Olympique Marseille).

## 2002/03 (Summer 2002), slice 2: the change list

`xbox0203/change_list.py` writes what the Xbox facts change in the 3.9.68 template. Max reads it before the build.
It writes one TSV per kind: club moves, releases, dates, ratings, league differences, names and competitions,
unclear people and template duplicate pairs. `summary.md` gives the counts and the rows that need his decision.
The header of the script gives the rules. `<changes>` is a new folder outside the repo.

```bash
python3 -I xbox0203/change_list.py "<facts-xbox>" "<facts-3968>" "<xbox data>" "<3.9.68 Data>" "<changes>" --league-check "<all-results.tsv>"
python3 -I test_change_list.py
```

The list applies Max's answers Q21 to Q28: each row is applied, kept (an answer keeps the 3.9.68 value) or held
(no answer takes it). The league rows that no answer settles go to `leagues-second-look.tsv`, sorted by nation.
`--league-check` takes the check of every league row with two public sources (`xbox-0203-work/league-check/
all-results.tsv`, outside the repo) and applies Max's answers Q33 to Q37 to it (`league_check`).

## 2002/03 (Summer 2002), slice 3: the build

`xbox0203/build_2002.py` copies the 3.9.68 files and patches the format 1 rows in place with the applied rows of
the list: staff clubs, dates and jobs, ratings, the club lists, divisions and the Florentia Viola name. It adds no
row. Then `duplicates.merge_folder` merges the template's duplicate pairs (Q28) and keeps the pairs of two people
(`duplicates.kept_apart`) on `<changes>/kept-pairs.tsv`. Then `staff_slots.py` fills empty staff slots (first
season 2002, Q12) and `duplicates.py check` runs with that list. The header of the script gives the steps.

The Turkish groups (Q11) need three free 3.9.68 competition rows, and 3.9.68 has two: the five "Turkish 2.
Division Group" rows are key names of the game (an error box at load when one is missing) and the last division
of 53 clubs. With `--exe <cm0102.exe>` the script proves this and stops. `--no-turkish-groups` builds without
the groups, and the clubs keep their 3.9.68 groups.

| Placeholder | What it is |
|---|---|
| `<out>` | the new database folder, named `Summer 2002` |
| `<work>` | a new folder for the reports (merge, ids, staff slots, the duplicate check) |

```bash
python3 -I xbox0203/build_2002.py "<changes>" "<xbox data>" "<3.9.68 Data>" "<out>" "<work>" --no-turkish-groups
python3 -I duplicates.py check "<out>" "<changes>/kept-pairs.tsv"
python3 -I test_build_2002.py
python3 -I test_summer_2002.py [<work root>]
```

`test_build_2002.py` checks the writer, the steps and the format 1 merge on synthetic folders.
`test_summer_2002.py` is the pipeline seam: it reads `<work root>/changes`, `build/Summer 2002`,
`build/summer-2002-work` and `disc/data` (default: `xbox-0203-work` beside the main checkout), checks every
applied row, the duplicate check and the known facts, and builds twice with `PYTHONHASHSEED` 1 and 2 (with
`python3 -P -s`) to show the same files. It skips when the data is absent. The zips come from
`release-dbs/make_zip2002.sh`, outside the repo.

## Mapping of FM ratings (key players only)

`keyplayers.py` maps each FM value to the median Update Team value of the matched players with the same FM value
(`value_map`), made non-decreasing. Above and below the values that at least 100 matched players have, the map goes
on with slope 1 from the last such value. Potential ability and each attribute have their own map in the same
way. FM's position ratings go across as they are. Attributes map byte by byte: `CM_ATTRS` in `keyplayers.py`
gives the FM attribute of each CM 01/02 attribute byte. The tool prints the maps to its log; keep the log with the
FM data, outside the repo.

## After a rebuild

1. Print the club table rows with `python3 -I club_rows.py <year> "<template>" "<work>"`. `--write` puts them into the part files and replaces the rows of that season with the same labels.
2. Put the new sizes and SHA-256 of `index.dat`, `club.dat` and `staff.dat` in `docs/research/rules-0.6.0/databases-signatures.tsv`.
3. Measure the date shifts with `python3 -I ../measure_date_shifts.py <3.9.68 Data> <folder with the new database> --only "Summer 2006"`, or "Summer 2004". Copy the printed row into `databases-date-shifts.tsv`. Set careers_kept and last_rows to 0.
4. Make the C tables with `python3 -I ../make_db_table.py` and `python3 -I ../make_dates_table.py`.
5. Zip the database folder under its name (for example `Summer 2006/`) into the own databases folder that `make_release.sh --own-dbs` takes.
6. Run `ctest -R "seasons_table|season_dates_table"` and `tests/seasons/season_dates_run.py --only <year>`.

The settings of each season are in `season_2004.py` and `season_2006.py`. Each file gives the template, the real
lists with one source each, the play-off winners and the part file of each list.

## Club tables of Spain, Germany, Scotland, Austria and Switzerland

`club_rows.py` writes these lists with the labels below. `make_clubs.py` (slice 15 branch) maps a label to a
database division only through its TARGETS list, so slice 15 adds these lines to TARGETS:

```python
    ("aut-sui", r"Austrian Bundesliga", "Austrian Premier Division"),
    ("aut-sui", r"Swiss Super League", "Swiss National Division A"),
    ("ger", r"Bundesliga", "German First Division"),
    ("ger", r"2\. Bundesliga", "German Second Division"),
    ("sco", r"Scottish Premier League", "Scottish Premier Division"),
    ("sco", r"Scottish (First|Second|Third) Division", "="),
    ("spa", r"La Liga", "Spanish First Division"),
    ("spa", r"Segunda División", "Spanish Second Division"),
```

Switzerland has 12 places in the game and 10 real clubs in 2004/05 and 2006/07.

## Long holiday test (original start)

```bash
python3 tools/seasons/own_db/long_test.py build/cm0102_recomp <game folder> <out folder> [england-germany|all]
```

The game folder is a 3.9.68 game with the season database in its Data folder, as a player installs it. The test
plays from July 2001 to 1 September 2004 on holiday, saves on each 1 July and on 1 September, loads each save in
the next step, and checks the squads of Real Madrid, Bayern and Arsenal in each save (`save_check.py`). It passes
with no error box, every save written and loaded, and every squad check passed (header of `long_test.py`).
