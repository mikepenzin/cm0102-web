# Corrected competition history rows (issues 145 and 165, release 0.7.3)

The game applies these rows at the database load of every season game, in `season_history_apply_rows` (`src/recomp_rt/win/season_history.c`). The base game does not take them. `tools/seasons/make_history_rows_table.py` makes `src/recomp_rt/win/season_history_rows_table.c` from the three tables here, and ctest `season_history_rows_table` checks that the two agree.

## The tables

- `lists.tsv`: the whole history of a competition. All rows of the competition go, and the listed rows are written. A row holds for a start year from `from_start` on, so an edition that the game plays itself is not written twice. Names joined by `|` are the spellings of the databases.
- `corrections.tsv`: one field of one edition. The row must hold `guard_first` as its winner. The field takes `write` unless it already holds a name of `accept`.
- `clears.tsv`: rows of a competition from a label year on, for editions that never took place.

## What the data held

| Report | Cause | Correction |
|---|---|---|
| World Cup 2026 host Canada | Our research wrote the first-named host, and a played edition takes a host of the first two | United States, the nation of the final, in the research row, at the load and in a played edition |
| Copa Libertadores 2022 | The Update Team data names Palmeiras as runner-up | Clube Atlético Paranaense |
| Euro 2020 host "Olot" | The data points the host at a club | England, the nation of the final |
| Euro 2024 third France | The data names a third, and the tournament had no third-place match | No third. Euro 2000, 2004 and 2008 hold the same fault in some databases |
| Conference League history | December 2025, May 2026 and October 2026 hold its winners under the Cup Winners' Cup, which ended in 1999 | The rows move to the Conference League record |
| Nations League history | No database holds it | Four editions on the record that the game plays the competition on |
| Greek Super Cup empty | The 3.9.68 data holds no rows, and other databases hold rows in wrong years | The 11 official editions |

Some databases point the host of other national team tournaments at a club (Olot, Östertelge BoIS, Vittoriosa Stars). The World Cups from 2002 and the Euros from 2000 take the real host from `corrections.tsv`. On any older edition the host becomes empty, because a club never hosts a national team tournament.

## Decisions

- A tournament with several hosts names the nation of the final, as the Euro 2020 and 2028 rows already do. Euro 2000, Euro 2008, Euro 2012 and World Cup 2002 keep either co-host that the data names.
- The Greek Super Cup list holds the official editions only. One database holds matches of 1948 to 1976, which the source calls unofficial, and they go.
- The 2025 Greek Super Cup was played on 3 January 2026. Wikipedia and the October 2026 database label it 2025, so the row keeps that label and holds from a 2026 start.
- The game plays no 2021 Nations League finals in a 2021 start: its first edition there is 2022-23 (`season_nl_first`). So the 2021 row holds from a 2021 start, although the final was in October 2021.
- A 2022 start plays the 2024-25 edition first, so it never holds the 2023 finals. The row of 2023 is not written there, because those finals lie after the start of that game.

## A planned change of play

The Conference League rows give the game a title holder from a 2022 start on. The holder then takes the place of the real access rule (`season_uefa_seeding.c`, rows Y and K of the rules table): in a 2026 start, Crystal Palace enter the Europa League. Before 0.7.3 the record held no row, so the place went unused. The base game and every 2001 start keep this place unused, because no Conference League row holds there.

A new 2001 game with changing rules on writes the 8 Greek Super Cup rows of 1980 to 1996, and the history table grows by 8 rows for them (0x527f60). That game then plays differently from the 0.7.2 build: in the new-game same-save check, `rules-on-q48` differs from `rules-on-q48-ref` with the lists part on and equals it with the lists part off (lead bisect point 5795241a, 19 November 2026). I do not know the mechanism. The same check shows that the corrections, the club host clear and the clears change nothing there. The base game takes no row, so it stays the original. `tests/smoke/smoke_run.py` runs `rules-on-q48` with the lists part off (`RECOMP_HISTORY_ROWS=fhc`), and `tests/seasons/history_rows_run.py` (run r2001) checks the rows.
