---
name: league-research
description: Researches how a league or competition changed over the seasons the Bosman Edition plays (2001/02 to the last season in the game), so a later release can add it to the rules table. Covers scope, the shared brief, one research agent per part, the second-source check, the reconcile, loose ends, source fill, Max's decisions and the second pass. Use for "add a league", "add a nation's lower tier", "research a competition", "how did this league change", "extend the rules table", "new competition for a release", or a spec slice that names rules-table research.
---

# League research

This is how the 0.6.0 rules table was made (slices 1 to 10 of `docs/specs/2026-10-16-seasons-0.6.0-spec.md`, 2026-10-16 and 2026-10-17). Follow the same steps for a new league, so its rows match the rest of the table.

## 1. Scope, before any agent starts

1. Read the spec section "Research decisions (Q268 to Q331)". These rules apply to every new league:
   - **No new tiers (Q331).** A nation never gets a tier below the lowest tier that the original runs for it. The real competitions of an existing tier come in. A tier that ends in real life gets `not held` rows.
   - **No new nations (Q288).** Leave out a competition that needs a nation a game cannot select.
   - **Places, not outcomes (Q272).** `up` and `down` are the places by the rules of the season. Withdrawals, expulsions and reprieves go in `notes`.
   - **Stopped seasons (Q273, Q274).** A season stopped early keeps its rule places. A season whose relegation was removed by rule keeps 0.
   - **Late editions (Q277).** A cup or tournament that real life played late plays at its normal time in the season it belongs to.
   - **Calendar-year leagues.** The 2001/02 row holds the year 2002. `real_season` holds the real label.
2. Find the original's code for the league: `docs/research/function-map/index.md` lists one source file per competition (for example `eng_conf.cpp`). Read the code before you rely on a name (repo `CLAUDE.md`).
3. If the league is new to the game (no code in the original), it needs Max's decision first. Grill him (grilling skill) and write the decision into the spec.
4. Count the clubs of each division in every season database: `tools/research/db_divisions.py <database folder> <out folder>`. Keep the output outside the repo (`/Users/max/Projects/cm0102/research-0.6.0/db-divisions/`), because it holds database content.

## 2. One shared brief

Add the league to the research brief, `/Users/max/Projects/cm0102/lead-notes/repo-moved/docs/research/research-brief-0.6.0.md` (outside the repo), or copy it for a new release. Every agent gets the same brief. It fixes:
- the TSV columns: `id season real_season nation comp_file comp tier name clubs up down playoffs split format subs_used bench foreign work_permit registration windows notes sources`;
- the `sources` form, `field=URL ; field=URL` or `all=URL`, with a page read on the day for every filled fact;
- the club moves file `<part>-clubs.tsv` (`season division club db_club_id db_club db_division source`) and its pool (clubs with no division, clubs in lower or spare divisions, clubs of database divisions whose tier has ended);
- the rules in section 1, and rule 2 of the brief: never name another modder, their patch or tool, or a forum poster, and name a database by month and year only.

## 3. The passes

Run one Opus agent per part for each pass. A part is one nation or a small group of nations, about 200 to 700 rows. Give each agent the brief and its part name only.

| Pass | Agent writes | What it does |
|---|---|---|
| Research | `<part>.tsv`, `<part>-clubs.tsv`, `<part>.md` | One row per competition per season, with sources, club moves and missing clubs |
| Check | `<part>.check.tsv`, `.check.md` | Each filled field against a page of another publisher: `agree`, `disagree`, `found`, `no second source` |
| Reconcile | `<part>.reconcile.tsv`, edits `<part>.tsv` | A third source for each `disagree`. Settled values go into the table. Conflicts go to Max |
| Loose ends | edits, `.md` section | Points that agents noticed but did not change |
| Source fill | edits `sources` | Run `tools/research/rules_lint.py`. Add a URL for each filled fact that has none |
| Decisions | edits, `.md` section "Pass 2" | Max's answers applied, tiers and nations cut, club moves down every tier, reserve teams listed |
| Second source | `<part>.check2.tsv`, edits | A second publisher for every fact that only one publisher states |

Then write one slice doc per group of parts (counts per part, links), and one list of questions for Max. Number each answer as a Q in the spec.

## 4. Lessons from 0.6.0

- **Sources.** Wikipedia season pages are a good first source. RSSSF, worldfootball.net, the league's own site and national newspapers make good second sources. Another language edition of Wikipedia is the same publisher. The WebFetch summary model can get numbers wrong: read the raw page with `curl -s -A "Mozilla/5.0"` and grep it. The champman0102 forum blocks WebFetch, but curl with a browser user agent reads it.
- **Never put an email address or personal data in a request header.** One agent did this for about 115 requests.
- **Agent limits.** At most 20 agents run at once. Keep a queue in the shared notes file. When the usage limit stops agents, resume each with SendMessage to its id: it keeps its context. Tell it to check what it already changed before it edits again.
- **Temporary files.** Each agent uses its own `mktemp -d` folder in the system temp folder, never `/tmp` itself or the home folder, and never runs `rm` with a wildcard or a variable path.
- **Common errors that the check found.** Relegation counts that held outcomes instead of places. Cup entrant counts made in two different ways. A season page whose prose and table disagree. Club matches by name to a different club with a similar name. Database record ids that name different clubs in different databases: always match within the database of that season.
- **Database facts.** A season database keeps the original's division sizes (for example Ligue 1 has 18 clubs in every database). Club moves therefore cascade down every tier.

## 5. In the game

The build slices of 0.6.0 (slices 11 to 29 of the spec) make the game read the rules table, the names table and the club moves. When they are built, add here how a new league's rows reach the game and which test proves them (the rules report of Q265).
