# Rules table, slice 9: the continental club competitions

Slice 9 of `docs/specs/2026-10-16-seasons-0.6.0-spec.md`, researched on 2026-10-16 and 2026-10-17. Brief: the 0.6.0 research brief (kept outside the repo). The rows cover the seasons 2001/02 to 2027/28 and follow Max's decisions Q268 to Q331 in the spec.

## Files

Each part has its rules table (`<part>.tsv`) and its notes (`<part>.md`, with the sections Reconcile, Loose ends, Source fill, Pass 2, Closing and the second-source sections). The checks are in `<part>.check.tsv` (first check) and `<part>.check2.tsv` (second source of pass 2 and the retry), and the reconcile decisions in `<part>.reconcile.tsv`. Parts with club moves also have `<part>-clubs.tsv`. All files are in `rules-0.6.0/`. The process is in the `league-research` skill.

## Counts

Field verdicts count the latest verdict of each checked field of a row that still exists. "No second source" includes fields that hold `unknown`. "Without a source" counts filled fields whose row cites no page for them (`tools/research/rules_lint.py`). "Club rows not in the database" includes the reserve and B teams that the build adds as new clubs (Q289).

| Part | Rows | Agree | Disagree (settled or listed) | Found | No second source | Unknown fields | Without a source | Club rows | Club rows not in the database |
|---|---|---|---|---|---|---|---|---|---|
| [uefa](rules-0.6.0/uefa.md) | 216 | 1387 | 26 | 10 | 215 | 37 | 0 | n/a | n/a |
| [club-world](rules-0.6.0/club-world.md) | 432 | 1199 | 34 | 7 | 549 | 672 | 0 | n/a | n/a |

## Method

1. An Opus agent wrote the rows of each part from pages read on the day.
2. A second Opus agent checked every filled field against a page of another publisher.
3. A third Opus agent read a third source for each disagreement and wrote the settled values into the table.
4. Loose-ends, consistency and source-fill agents closed the points the earlier agents found.
5. Pass 2 applied Max's decisions: no new tiers (Q331) and no new nations (Q288), conflicts settled by what happened in real life, club moves down every tier, and reserve teams listed.
6. A second-source agent and a retry looked for a second publisher for every fact that only one publisher stated.
7. Pass 3 applied Max's answers Q332 to Q339 (Brazilian state championships, the Suruga Bank Championship, the CONCACAF Champions Cup places, the English academy sides, play-offs against tiers that left the game).

The open questions for Max are in the list of questions for Max (kept outside the repo).
