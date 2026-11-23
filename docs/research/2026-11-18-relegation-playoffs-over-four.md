# Relegation play-offs over a play-off of four (issue 160, item 276)

Release 0.7.3, slice 11. In this shape, four clubs of the lower division play off, and the winner meets a club of the upper division for one place.

## How the list was made

The list comes from `docs/research/2026-10-18-league-rules-encoded.tsv`. A candidate is a league row with a relegation play-off place (`po_down`) in a season where the tier below has four or more promotion play-off places (`po_up`). The `playoffs_text` of each candidate row then decides whether the shape is real.

## Leagues that play this form

The game reads the seasons from the rows. The lower row needs 4 play-off places directly below its direct places, and the upper row needs 1 relegation play-off place. The form plays only with changing rules on.

J2 has three forms, and its rows pick one in each season. Rows 2003 to 2007 have 1 play-off place and play the tie of the 3rd against the J1 16th. Rows 2017, 2018 and 2021 play this form. The other rows with 4 play-off places have no J1 relegation play-off place, so J2 plays its plain play-off of four.

| Lower division | Upper division | Rows | Places | Semi-finals | Lower final | Last tie | Row ids |
|---|---|---|---|---|---|---|---|
| nor_first | nor_prm | 2011 to 2027 | 3rd to 6th v 14th | one match | one match | two legs | nor-fin-102, nor-fin-103 and later |
| ire_first | ire_prm | 2019 to 2025 | 2nd to 5th v 9th | two legs | one match | one match, neutral | wal-nir-irl-466, wal-nir-irl-467 and later |
| jap_j2 | jap_j1 | 2017, 2018, 2021 | 3rd to 6th v 16th | one match | one match | one match at the J1 club | aus-jap-kor-206, aus-jap-kor-233 and later |
| fra_third | fra_second | 2026, 2027 | 3rd to 6th v 16th | one match | one match | two legs | fra-228, fra-229 and later |

Norway, Ireland, J1 and J2 are calendar-year leagues, so an edition Y plays row Y - 1. In Norway, rows 2019 and 2020 (nor-fin-183, nor-fin-193) play the last tie as one match on neutral ground.

## Candidates that do not have this shape

| Pair | Rows | Why it is left out |
|---|---|---|
| arg_prm, arg_second | 2004 to 2006 | Two promotion ties against two Primera clubs. Slice 5 owns Argentina. |
| bel_first, bel_second | 2008 | The two winners and two top-flight clubs play a group of four. Slice 9 owns the Belgian play-offs. |
| gre_prm, gre_second | 2013 | The second tier plays a round of 8 from two groups for 3 places. |
| hol_prm, hol_first | 2001 to 2003, 2020 to 2025 | Six Eerste Divisie clubs take part. Slice 10 owns the nacompetitie. |

## What the original does

- Norway: `NorFirst::create_play_off_stage` 0x790e20 builds one tie, the row of status 1 (0x790e97) against the upper row of status 2 (0x790f3c).
- Ireland: `IreFirst::create_promotion_playoff_stage` 0x635dc0 builds the same tie (0x635e37).
- J2 and Ligue 3: the classes have no stage. Their `advance_stage` is the empty function 0x5a8f60.

## Limits

- The rows do not give the legs of each Irish round. Sources show that the format changed between seasons: the semi-finals were one match in 2020 only, and the 2025 last tie was one match. I don't know the legs of every season, so the game plays one form for all Irish rows.
- The rows do not give the legs of the Ligue 3 semi-finals and final. The game plays one match in each.
- In J2 a level semi-final in real life sends the higher place through. The game plays extra time and penalties.
- Item 276 also asks for a check of the down counts of Greece. This slice did not check Greece 2013.

## Sources

- The rules table rows named above, with their own sources in `docs/research/rules-0.6.0/`.
- [2020 League of Ireland First Division](https://en.wikipedia.org/wiki/2020_League_of_Ireland_First_Division)
- [2024 League of Ireland First Division](https://en.wikipedia.org/wiki/2024_League_of_Ireland_First_Division)
- [Extratime: the 2020 promotion play-off structure](https://www.extratime.ie/articles/24208/longford-town-the-first-to-host-shamrock-rovers-ii-as-new-promotion-play-off-structure-revealed/)
