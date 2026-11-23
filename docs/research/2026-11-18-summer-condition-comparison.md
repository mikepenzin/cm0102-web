# Summer condition: the original 3.9.68 against Bosman Edition (issue 148)

Issue 148 reports that a player's condition fell between two summer matches a week apart, with no change to training. Decision L12 of the 0.7.3 spec asks for a comparison on the original game first.

## Result

The original 3.9.68 game lowers condition in the summer in the same way as Bosman Edition. The two builds differ by no more than two runs of the original differ from each other. Under L12 the issue closes with no code change.

## The code

All addresses are in `cm0102.exe` 3.9.68.

- `Injury::get_condition` 0x62c590 gives the shown condition as fitness (record +8) times match condition (record +0xa) over 10000 (0x62c5e9 to 0x62c5fb).
- `Injury::update_player_fitness` 0x62c680 brings a record up to today. It subtracts 66 fitness for each day of one day count (0x62cb85 to 0x62cba1) and 55 fitness for each day of another (0x62ccea to 0x62ccfa). The split of the days depends on the club's dates from 0x669bd0 (0x62c976 to 0x62cb31).
- So a player with full match condition can lose shown condition on days with no match. The code does not need a change of training for this.

Bosman Edition keeps this code. The runtime wrap of 0x62c680 (`src/recomp_rt/win/picker.c`) only skips the update during a pick and a player search. The wrap of 0x62c590 (`src/recomp_rt/win/columns.c`) only notes the returned value for the colours.

## The test

- Start: the saved game `prematch.sav`, Arsenal on 16 July 2001. Both builds loaded this one file.
- Play: Go on Holiday to 25 July 2001, then save.
- Original: the unmodified `cm0102.exe` (SHA-256 `0c78e7da...bff86`), run twice under wine 6.0.3 in a Linux container. The container reported the game folder as a CD drive, because the game asks for its CD.
- Bosman Edition: the 0.7.2 reference build 68870f30 with changing rules off, run with seeds 1, 2 and 3. The `src` folder of the 0.7.3 base is the same as in that build.
- Measure: the fitness records of `injury.dat` in each save, for the 1120 players that all five runs brought up to 25 July.

Random numbers differ between the runs, so single players differ. The runs of one build show how large that difference is.

| Run | Shown condition fell | Mean change of shown condition (of 10000) | Rested players | Rested players whose condition fell | Mean fitness change, rested |
|---|---|---|---|---|---|
| Original 1 | 273 (24.4 %) | 177.5 | 830 | 61 (7.3 %) | 384.2 |
| Original 2 | 248 (22.1 %) | 179.3 | 845 | 64 (7.6 %) | 385.3 |
| Bosman Edition, seed 1 | 256 (22.9 %) | 177.1 | 832 | 59 (7.1 %) | 386.4 |
| Bosman Edition, seed 2 | 242 (21.6 %) | 193.8 | 849 | 59 (6.9 %) | 392.0 |
| Bosman Edition, seed 3 | 256 (22.9 %) | 207.5 | 843 | 58 (6.9 %) | 394.2 |

A rested player has match condition 10000 at the start and at the end of the run.

The next table compares the fitness of the 749 players who rested in all five runs, in pairs of runs.

| Pairs of runs | Pairs | Equal fitness | Mean absolute difference |
|---|---|---|---|
| Original against original | 1 | 68.8 % | 31.6 |
| Original against Bosman Edition | 6 | 65.7 % | 28.6 (24.0 to 31.7) |
| Bosman Edition against Bosman Edition | 3 | 65.2 % | 21.1 (20.1 to 22.1) |

## Limits

- The save of the report is not available, so this test does not replay the reported match pair.
- Only 6 of the 1120 players started between 50 % and 75 % condition, near the reported keeper's 60 %. In each of the five runs 1 of the 6 fell, and that player had not rested. The result for low condition rests on the code above.
- The test covers one save and nine days of July 2001.
- The original has two runs, so its run-to-run spread is a rough figure.
