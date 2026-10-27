# International competitions (world level), 2001/02 to 2027/28

Part `int-world` of slice 10 of `docs/specs/2026-10-16-seasons-0.6.0-spec.md`. Research date 2026-10-16.

Files:

- `int-world.tsv`: 324 rows. Each of the 27 seasons has 12 rows: `nation rules`, `world_cup`, the six qualifier files (`wc_europe_league`, `wc_south_american_league`, `wc_concacaf_cup`, `wc_african_cup`, `wc_asia_league`, `wc_oceania_league`), `inter-confederation play-offs`, `fifa_confed`, `olympics` and `host_country`.
- No clubs file. Club moves do not apply to national teams.

## Method

- Wikipedia pages read today as raw wikitext with `curl` (`action=raw`) and grepped: the World Cup pages 2002 to 2030, the qualification page of each cycle, the page of each confederation's qualifying, the round pages ("2018 FIFA World Cup qualification – AFC third round" and others), the inter-confederation play-off pages, the squad pages, the five Confederations Cup pages, the Olympic men's tournament and qualification pages 2004 to 2028, and the pages on the golden goal, substitutes, Football Australia and the renamed nations.
- Month placement: the season of a match comes from the match dates on the round pages (July to June is one season). Where a round page gave no dates, the row uses the dates in the confederation page text or its infobox.
- `real_season` holds the calendar year for a finals tournament ("2002") and the season label for qualifying rows.
- Columns that do not apply to national teams (`up`, `down`, `split`, `foreign`, `work_permit`, `windows`) are `n/a`. `registration` holds the squad size of a finals tournament. `nation` holds the confederation, or "World (FIFA)".
- `comp_file` of the play-off rows: the original has no play-off file of its own. The ties are built in the section files `wc_oceania_league` and `wc_europe_league` (settled in Pass 2, see below).

## Change points

- **Holders' place:** 2002 is the last World Cup with an automatic place for the holders. From 2006 the holders qualify like the others. Hosts keep their automatic place.
- **Golden goal:** the 2002 World Cup and the 2003 Confederations Cup use it (the 2003 final ended with a golden goal). It ceased in 2004 for most FIFA matches. No world-level competition of this part uses the silver goal (the silver goal belongs to UEFA competitions and Euro 2004: see `int-continental`).
- **World Cup:** 32 teams in 2002 to 2022, 8 groups of 4. 2022 is in November and December (season 2022/23). 2026 has 48 teams, 12 groups of 4, a round of 32 and 104 matches, with three hosts.
- **Squads and substitutes at the finals:** 23 players from 2002 to 2018, up to 26 in 2022 and 2026. Three substitutes to 2014, a fourth in extra time in 2018, five plus a sixth in extra time and a concussion substitute in 2022.
- **Qualifying slots:** UEFA 13 (14.5 with holders in 2002), CONMEBOL 4.5, CONCACAF 3 (2002) then 3.5, CAF 5, AFC 4.5, OFC 0.5, plus hosts. For 2026: UEFA 16, CONMEBOL 6 + 1, CONCACAF 3 + 3 hosts + 2, CAF 9 + 1, AFC 8 + 1, OFC 1 + 1.
- **Inter-confederation play-offs:** two two-leg ties in 2002 to 2018. Pairings were fixed by FIFA to 2010 and drawn from 2014. 2022: single matches in Qatar in June 2022. 2026: a six-team play-off tournament in Mexico in March 2026, two brackets of three, single matches.
- **UEFA play-offs:** two-leg ties to 2018. From 2022, single-match paths (semi-final and final) with places for Nations League group winners: 3 paths in 2022, 4 in 2026.
- **CONCACAF:** Hexagonal to 2018, Octagonal in 2022, three groups of four in 2026. The 2030 format is set (announced 6 February 2026): round 1 in September-October 2027, round 2 from October 2027 to March 2028, final round from June 2028. These rounds fall in 2027/28.
- **Australia:** in OFC for the 2006 qualifying, AFC member from 1 January 2006, first AFC qualifying in 2007/08. OFC wins its first direct World Cup place for 2026.
- **Confederations Cup:** 2003 (France), 2005 (Germany), 2009 (South Africa), 2013 (Brazil), 2017 (Russia). Abolished: FIFA confirmed it on 15 March 2019. No edition from 2018/19.
- **Olympics (men):** 16 teams in 2004 to 2024. Under-23 with up to 3 overage players, under-24 for Tokyo (played in July 2021, season 2021/22). 18-player squads, 22 in 2021. 2028 has 12 teams (approved by the IOC in April 2025), in July 2028, so it falls in 2028/29, after the table. Its qualifiers fall in 2026/27 and 2027/28.
- **Hosts:** 2010 chosen 15 May 2004, 2014 on 30 October 2007, 2018 and 2022 on 2 December 2010, 2026 on 13 June 2018, 2030 and 2034 on 11 December 2024. The 2030 hosts (Morocco, Portugal, Spain, Argentina, Paraguay, Uruguay) are qualified.
- **Nation names and members:** FR Yugoslavia becomes Serbia and Montenegro in February 2003. Serbia follows in June 2006. Montenegro joins UEFA on 26 January 2007 and FIFA on 31 May 2007, and plays from 2008/09. Gibraltar joins UEFA on 24 May 2013. Kosovo joins UEFA on 3 May 2016. Kosovo and Gibraltar join FIFA on 13 May 2016 and enter the 2018 qualifying. Kazakhstan plays in UEFA from 2002/03. North Macedonia from 12 February 2019. Eswatini from 19 April 2018. Czechia (UN short name 5 July 2016). Türkiye (UN request 31 May 2022). Russia is suspended from 28 February 2022 and is still out of the 2026 qualifying.

## Unknowns

- Bench size at the World Cup finals except 2014 (12, every non-starter). The other years are marked as an inference.
- Substitutes in qualifying from 2020/21: IFAB allowed five from 2020 and made it permanent in 2022. The use in each qualifying competition was not checked.
- Substitutes at the 2026 finals: not stated on the page read. The row gives five plus one as an inference.
- The date when FIFA and UEFA changed the team names North Macedonia, Czechia, Türkiye and Eswatini. The country renames have dates. The football name changes do not.
- The 2030 qualifying formats of AFC, CAF, CONMEBOL and OFC, and whether any of them starts in 2026/27 or 2027/28. The UEFA 2030 format (two leagues) is announced, but it is seeded from the 2028/29 Nations League, so it starts after the table (inference).
- The 2030 team count. 48 is the current size. CONMEBOL's president announced 64 teams on 20 July 2026. No FIFA decision was found.
- Months of the 2004 Olympic qualifiers, the 2027 U-23 Africa Cup of Nations and the 2028 AFC U-23 Asian Cup.
- The 2004 Olympic age rule is not quoted on the 2004 page. The row gives the 2008 rule as an inference.

## Notes for the build slices

- Slice 27 needs, per season: whether the World Cup, the Confederations Cup and the Olympics run, their host, size and months, and which qualifying rounds run in that season. The `format` column of each qualifier row says which rounds fall in the season.
- Slice 29 needs the 2025/26 rows: 48-team finals, the 2026 qualifying formats and the play-off tournament.
- The original plays Australia in OFC. From 2005/06 (1 January 2006) Australia belongs to AFC.
- The original holds the name Yugoslavia (change point F19). The names table must change it to Serbia and Montenegro in 2002/03 and to Serbia in 2006/07.
- Missing clubs: none. This part has no club divisions.

## Reconcile

A third agent took the 11 `disagree` fields and the 11 `found` fields of `int-world.check.tsv`. The decisions are in `int-world.reconcile.tsv`. The settled values and their URLs are in `int-world.tsv`. Row order and ids did not change.

| Decision | Fields |
|---|---|
| second | 11 |
| first | 0 |
| conflict | 0 |
| found, accepted | 8 |
| found, accepted in part or with a correction | 3 |

Third publishers: the FIFA match list (`api.fifa.com`), the FIFA and CAF statement of 10 November 2023 on Eritrea, VOA (AFP), Minivan News and a football-in-Oceania site that quotes the OFC press release.

Changes:

- int-world-3: the UEFA groups start on 16 August 2000 (Estonia v Andorra). One statistics site lists this match as a friendly. FIFA and RSSSF list it as a qualifier.
- int-world-119: AFC Olympic qualifying also runs in 2010/11 (rounds 1 and 2, February to June 2011).
- int-world-172: 30 CONMEBOL matches in 2015/16, not 32.
- int-world-206, 218, 230, 242: the next finals are the 2022 World Cup (season 2022/23).
- int-world-248: 8 teams in the groups, 6 completed.
- int-world-270, 282, 294: 53 teams in the CAF groups, group E has 5.
- Found values: bench 12 at the 2002, 2006, 2010 and 2018 finals, 15 in 2022, five substitutes plus one in extra time in 2026. The bench value 12 comes from a page that says "previous tournaments" in general.
- int-world-35 and int-world-23: the 2004 Olympic qualifying starts in 2002/03 (CAF from 8 November 2002, AFC from 5 April 2003). The check gave 24 November as the end of the CAF preliminary round. The page gives 29 November. The OFC month is not on the page.
- int-world-244: the Brazil v Argentina match was declared void. The suspension date is not on the page read, so it is not in the row.
- int-world-193 and 205: FIFA changed the names between two rankings: Eswatini between 16 August and 20 September 2018, North Macedonia between 4 April and 14 June 2019. int-world-181: FIFA still said "Czech Republic" in the ranking of 27 May 2021.

Conflicts: none.

These lines of the Unknowns section above are now partly answered: the bench size at the finals, the substitutes in 2026, the months of the 2004 Olympic qualifiers and the FIFA dates for Eswatini and North Macedonia. The UEFA dates and the FIFA dates for Czechia and Türkiye stay unknown.


## Source fill

Method: the lint of 2026-10-16 listed each filled fact field without a `field=URL` entry. For each one, I read the pages the row already cites today (curl, text search for the value). If no cited page states the value, I read the general page of the competition. I added `field=URL` to `sources` only where the page text holds the value. I changed no values.

Sources added: 163 fields (name 163).

- Rows with `format` = not held cite the general page of the competition for `name`: https://en.wikipedia.org/wiki/FIFA_World_Cup, https://en.wikipedia.org/wiki/FIFA_World_Cup_qualification (it names each confederation and the inter-confederation play-offs) and https://en.wikipedia.org/wiki/Football_at_the_Summer_Olympics.
- The label "Host countries" cites https://en.wikipedia.org/wiki/FIFA_World_Cup_hosts or a cited page that uses the words "host countries".
- A name with a part in brackets, for example "(UEFA)" or "(men)", counts as stated when the page holds the main name and the bracket text.

Disagreements: none. Fields with no source: none.

## Pass 2

Date 2026-10-17. Decision agent, by the "Pass 2" rules of the 0.6.0 research brief (kept outside the repo) and Q268 to Q331 of the spec.

1. **No new tiers (Q331), no new nations (Q288).** No change. This part has no tiers. Every row is a file of the original (`world_cup`, the six `wc_*` sections, `fifa_confed`, `olympics`, `host_country`) or the inter-confederation play-offs, which the original builds inside the section files (point 3). No row needs a nation that the game cannot select. The national teams come from the season database, and the build adds no nation record. Kosovo has club records in the databases from October 2016, Gibraltar from October 2015 and Montenegro from September 2011 (`research-0.6.0/db-divisions/*.clubs.tsv`). A member that a season database lacks does not play in that season.
2. **Conflicts and open points.** The Reconcile section has no conflict. No point in this part waits for Max. The Unknowns section lists facts that no page gave. They are not decisions, and the second-source pass takes the single-source facts.
3. **`comp_file` of the 27 inter-confederation play-off rows** (int-world-9, 21, 33 and every twelfth row to 321): `unknown (inference ...)` becomes `wc_oceania_league; wc_europe_league (ties built in the section files)`. Evidence:
   - 0x91f800 (in `wc_oceania_league`, read today): it reads the competition with key id 0x9cf778 (World Cup South American Qualifying Section), takes the Oceania winner and the South American play-off nation, and builds a two-team stage (call 0x522e00).
   - 0x919c10 (in 0x919910, `wc_europe_league`): one European runner-up plays the Asian section (from `2026-10-16-sweep-international.md`, section 5).
   - The original thus plays the 2002 pairings only (UEFA v AFC, CONMEBOL v OFC). The real pairings of 2006 (CONCACAF v AFC, CONMEBOL v OFC), 2010 (AFC v OFC, CONCACAF v CONMEBOL), the drawn pairings of 2014, 2018 and 2022, and the six-team tournament of 2026 need new code (inference from the code above).
4. **Club moves (Q279, Q280, Q290) and reserve teams (Q289).** Do not apply. National teams have no divisions.
5. **Part rules.** None names this part. Q277 (a late edition plays in its normal slot) changes no row. Every finals row already sits in the season of its first match (Q276), for example the 2022 World Cup in 2022/23 and the Tokyo Olympics in 2021/22.

Lint (`tools/research/rules_lint.py`): 0 problems before and after.


## Second source, pass 2

Date 2026-10-17. Scope: the 237 check lines with verdict `no second source`. 202 of them hold `unknown` or an inference (135 bench, 40 subs_used, 8 clubs, 8 playoffs, 8 format, 3 registration), so this pass does not check them. The other 35 lines are in `int-world.check2.tsv`.

| Verdict | Lines |
|---|---|
| agree | 14 |
| disagree, settled | 3 |
| disagree, still split (conflict) | 1 |
| no second source | 17 |

Second publishers: IFAB (Laws of the Game 2002/03, 2004/05, 2008/09 and 2012/13), the FIFA regulations of the 2013 and 2017 Confederations Cups, FIFA media releases, Fox Sports, MLS, ESPN, Goal, SABC News, Inside World Football, Around the Rings, EFE (Emol), WGBH (NPR) and Al Jazeera.

Changes in `int-world.tsv` (row order and ids kept):

- int-world-190 `subs_used`: `3` becomes `3, plus a fourth in extra time (IFAB trial)`. MLS says the Confederations Cup ran the fourth-substitute trial. ABC News (29 June 2017) says teams could make a fourth substitution in extra time, and Portugal used one in the semi-final. The FIFA regulations of November 2016 still said three.
- int-world-121 `notes`: `No change found this season.` becomes the admission of South Sudan to FIFA on 25 May 2012 (WGBH, then FIFA).
- int-world-276 `notes`: the season holds a host decision. On 4 October 2023 the FIFA Council named the bid of Morocco, Portugal and Spain the sole 2030 candidacy, with one match each in Uruguay, Argentina and Paraguay, subject to the FIFA Congress of 2024 (Al Jazeera, then FIFA).

Conflict:

- int-world-37 `notes`: the AFC date (23 March 2005) agrees. No page read states the OFC date of 17 April 2005. The OFC release, dated 25 April 2005, says the Executive Committee decided in Noumea "on Saturday". The NZ Herald says the meeting was set for 16 April. The date is 16, 17 or 23 April 2005, and the sources stay split. The value is not changed. The move takes effect on 1 January 2006 in every source, so the build does not depend on this date.

Found values, not written: the FIFA regulations give 12 substitutes on the start list at the 2013 and 2017 Confederations Cups (bench 12 for int-world-142 and int-world-190). The `bench` field of those rows stays `unknown`.

No second source: the 17 lines are claims of absence, for example "No change found this season" or "no host decision found this season". No page of another publisher lists every change or every host decision per season. Three of them (int-world-301, 312, 313) are about seasons that are in progress or still to come.

Lint (`tools/research/rules_lint.py`): 0 problems before and after.

## Closing

Date: 2026-10-17. Closing agent. `rules_lint.py`: 0 problems before and after.

- int-world-142 (2013 Confederations Cup) and int-world-190 (2017 Confederations Cup), `bench`: unknown changed to 12. The FIFA regulations name all 23 players on the start list, 11 selected players and 12 substitutes (2013: the start-list clause, 2017: art. 31). URLs, read on 2026-10-17: https://web.archive.org/web/20130418025516/http://www.fifa.com/mm/document/tournament/competition/01/99/46/55/regulationsconfederationscup2013_e.pdf and https://web.archive.org/web/20161124222327/http://resources.fifa.com/mm/document/tournament/competition/02/85/41/57/fcc2017regulationsen_neutral.pdf
