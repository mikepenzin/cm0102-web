# Second-source check of part arg-usa

Check of `arg-usa.tsv` (378 rows) and a spot check of `arg-usa-clubs.tsv`. Research of 2026-10-16. Brief: the 0.6.0 research brief (kept outside the repo). The first agent's files are not changed. The verdicts are in `arg-usa.check.tsv`.

## Method

- One verdict per filled fact field of each row. A field with `n/a` gets no verdict. The fields checked are name, clubs, up, down, playoffs, split, format, subs_used, bench, foreign, work_permit, registration and windows.
- The first agent's sources are English Wikipedia for most facts, plus mediotiempo.com, lapaginamillonaria.com, si.com and mlssoccer.com for a few rule facts. Every second source below comes from another publisher than the first source of that fact. Where the only other page found was on the first source's site (mlssoccer.com for the MLS substitutes of 2022 to 2026), the verdict is `no second source` and the note says what that page states.
- All pages were read on 2026-10-16 with `curl` and `grep`, or with WebFetch asked to quote the sentence.
- Main second sources:
  - RSSSF Argentina season pages (`rsssf.org/tablesa/argNN.html`, `arg20NN.html`, `arg2015a.html`), second-level pages `arg2-NN.html` for 2001/02 to 2009/10, and the cup history `argcuphist.html`.
  - RSSSF USA season pages `rsssf.org/tablesu/usaNN.html` (2002 to 2025; no 2026 page).
  - AFA Boletín 5853 (Reglamento de los Torneos 2021 de la LPF, text PDF on afa.com.ar) and AFA Boletín 6579 (31 October 2024).
  - IFAB news on the permanent five-substitute option (gives the May 2020 temporary amendment and the rise of named substitutes from 12 to 15).
  - Diario de Cuyo (28 December 2017) and Perfil 442 (5 September 2026) on the Argentine foreign-player rule.
  - mlssoccer.com roster rules (2020 to 2023, 2025, 2026) and competition guidelines (2020 to 2026), mlssoccer.com 2006 Designated Player announcement, rsl.com on the 2010 Designated Player change, SBI Soccer (December 2007) on international slots.
  - ESPN (All-Star Game history, 2026 All-Star match, Leagues Cup 2019 and 2024, MLS substitutes 2021), Boston Globe and LA Galaxy on the 2027 calendar, ussoccer.com, USL Championship, USL League Two and Phoenix Rising FC on U.S. Open Cup fields, leaguescup.com and worldsoccertalk.com on the Leagues Cup and Campeones Cup.
- Substitutes before 2020: the verdict `agree` rests on the IFAB page. It states that five substitutes began as a temporary amendment in May 2020, so the earlier maximum was 3. It is not a league-specific source.

## Counts

| Verdict | Fields | Of which the first agent wrote `unknown` |
|---|---|---|
| agree | 1,065 | 0 |
| no second source | 607 | 498 |
| found | 43 | 26 |
| disagree | 7 | 0 |
| Total | 1,722 | 524 |

Of the 607 `no second source` verdicts, 498 are facts that stay unknown. The other 109 are filled facts that no page of another publisher confirmed. Most of these are Copa Argentina entrant totals for 2011/12 to 2017/18 (RSSSF gives matches, not totals), U.S. Open Cup totals for 2002 to 2016 and 2018, Argentine and MLS substitutes after 2021, 2026 and later rows, and the Leagues Cup and Campeones Cup editions of 2018 to 2023.

## Disagreements

1. **arg-usa-34, down (Primera B Nacional 2007/08).** First agent: "2 direct; 2 relegated in total (infobox)". RSSSF: 3 relegated. Ben Hur and Almirante Brown went down directly, and Nueva Chicago lost the promotion/relegation playoff to Los Andes. First URL: https://en.wikipedia.org/wiki/2007–08_Primera_B_Nacional. Second URL: https://www.rsssf.org/tablesa/arg2-08.html
2. **arg-usa-35, down (Primera B Nacional 2008/09).** First agent: "2 relegated in total (infobox)". RSSSF: 3 relegated. Almagro and Talleres went down directly, and Los Andes lost the 2nd/3rd level playoff to Deportivo Merlo. First URL: https://en.wikipedia.org/wiki/2008–09_Primera_B_Nacional. Second URL: https://www.rsssf.org/tablesa/arg2-09.html
3. **arg-usa-127, format (Trofeo de Campeones 2020).** First agent: Boca qualified twice, the runners-up played a semi-final, and the final was "played March 2023". The RSSSF list of winners has no 2020 edition: it runs 2019, then 2021 to 2025. La Nación of 23 February 2023 says that River beat Banfield on 22 February 2023, and that the final against Boca had a planned date of 15 March 2023 that could still move. I do not know whether the final was ever played. First URL: https://en.wikipedia.org/wiki/Trofeo_de_Campeones_de_la_Liga_Profesional. Second URLs: https://www.rsssf.org/tablesa/argcuphist.html and https://www.lanacion.com.ar/deportes/futbol/boca-river-cuando-se-jugarian-las-dos-finales-pendientes-por-el-trofeo-de-campeones-2020-y-la-nid23022023/
4. **arg-usa-186, format (Supercopa Internacional 2025).** First agent: "not held". RSSSF lists a 2025 edition, Rosario Central 3-1 Estudiantes at Santiago del Estero. The page says it was "played in 2026", on 26 September 2026. First URL: https://en.wikipedia.org/wiki/Supercopa_Internacional. Second URLs: https://www.rsssf.org/tablesa/argcuphist.html and https://www.rsssf.org/tablesa/arg2025.html
5. **arg-usa-187, format (Supercopa Internacional 2026).** First agent: held, with the venue "2026 Santiago del Estero". RSSSF labels that match the 2025 edition and lists no 2026 edition yet. The rule (league champion v Trofeo de Campeones winner) agrees. Items 4 and 5 differ on the label only. The first agent files these by the year of play. RSSSF files them by edition, and so does the first agent for the 2022 edition, which was played in January 2023. First URL: https://en.wikipedia.org/wiki/Supercopa_Internacional. Second URL: https://www.rsssf.org/tablesa/arg2025.html
6. **arg-usa-235, bench (MLS 2020).** First agent: "12 (matchday roster of 23)", from si.com of 21 June 2020. The 2020 MLS competition guidelines give a maximum of 20 players on the match roster, 11 starters and nine substitutes. The si.com figure may describe a plan made before the season restarted. First URL: https://www.si.com/soccer/2020/06/21/mls-2020-season-begin-july-expanded-rosters-substitutions. Second URL: https://www.mlssoccer.com/news/2020-mls-competition-guidelines
7. **arg-usa-376, windows (United States 2026).** First agent: "Primary window mid-February to mid-May; secondary window early July to early August". The MLS roster rules (as of 3 February 2026) give a primary window from 26 January to 26 March 2026 and a secondary window from 13 July to 2 September 2026. First URL: https://en.wikipedia.org/wiki/Major_League_Soccer. Second URL: https://www.mlssoccer.com/about/roster-rules-and-regulations

## Found values

Facts that the first agent marked `unknown`, or left partly unknown, and that a page read today gives.

| Row | Field | Value | URL |
|---|---|---|---|
| arg-usa-12, -13 | bench | 7 (inference: Superfinal line-ups name 3 used and 4 unused substitutes per side) | rsssf.org/tablesa/arg2013.html, arg2014.html |
| arg-usa-15 | bench | 7 (2016 final line-ups) | rsssf.org/tablesa/arg2016.html |
| arg-usa-16 | bench | 7 (inference from the 2016/17 Copa Argentina final line-ups) | rsssf.org/tablesa/arg2017.html |
| arg-usa-68, -69, -70 | bench | 7 (Copa Argentina final line-ups, 2014/15 to 2016/17) | rsssf.org/tablesa/arg2015.html, arg2016.html, arg2017.html |
| arg-usa-20, -155 | bench | 12: match sheet of 11 starters and 12 substitutes (2021 Torneo and Copa LPF, art. 33.2) | AFA Boletín 5853 |
| arg-usa-50 | bench | 12 for the one-match promotion and relegation ties of the 2024 Primera Nacional. The league rounds are an inference | AFA Boletín 6579 |
| arg-usa-209 | registration | Lista de Buena Fe in the COMET system (provisional list by 11 Feb 2021, final list by 18 Feb 2021). A list of 20 professionals under art. 205, with at least 8 of them on each match sheet. With 6 foreign contracts, 2 of the foreigners need 10 senior caps | AFA Boletín 5853 |
| arg-usa-209 | windows | Registration for the Copa LPF 2021 closed on 18 Feb 2021 at 18:00. Later windows are not given | AFA Boletín 5853 |
| arg-usa-236 to -239, -241 | bench | 9 named substitutes, match roster of 20 (2021 to 2024, 2026) | mlssoccer.com competition guidelines of each year |
| arg-usa-276 to -279, -281, -283 to -288, -291 to -294 | format | Extra time, then penalties (example in each note) | rsssf.org/tablesu/usaYYYY.html |
| arg-usa-282 | format | Extra time used in 2013; no shoot-out in the rounds listed | rsssf.org/tablesu/usa2013.html |
| arg-usa-292 | clubs | 100 (2023 U.S. Open Cup) | ussoccer.com, 2023 first-round story |
| arg-usa-293 | clubs | 96 (2024) | phxrisingfc.com, relaying U.S. Soccer |
| arg-usa-294 | clubs | 96 (2025), 16 of them MLS | uslchampionship.com |
| arg-usa-296 | clubs | 96 (2027), 32 of them Open Division | uslleaguetwo.com |
| arg-usa-357 | foreign | 2007: up to 7 foreign players, at most 4 of them senior internationals | sbisoccer.com, December 2007 |
| arg-usa-370 | windows | 12 Feb to 5 May 2020; 7 Jul to 5 Aug 2020 | mlssoccer.com 2020 roster rules |
| arg-usa-371 | windows | 10 Mar to 1 Jun 2021; 7 Jul to 5 Aug 2021 | mlssoccer.com 2021 roster rules |
| arg-usa-372 | windows | 10 Feb to 4 May 2022; 7 Jul to 4 Aug 2022 | mlssoccer.com 2022 roster rules |
| arg-usa-373 | windows | 31 Jan to 24 Apr 2023; 5 Jul to 2 Aug 2023 | mlssoccer.com 2023 roster rules |
| arg-usa-375 | windows | 31 Jan to 23 Apr 2025; 24 Jul to 21 Aug 2025 | mlssoccer.com 2025 roster rules |

## Other notes for the lead

- The 2019 Trofeo de Campeones de la Superliga (arg-usa-126) was played on 14 December 2019 at Mar del Plata (Racing 2-0 Tigre). This confirms the first agent's inference about the year.
- arg-usa-13 (Primera 2013/14): a one-match tie-break for 18th place (Colón 0-1 Atlético de Rafaela at Rosario) relegated Colón. The verdict is `agree` because 3 went down in total, but the row says "playoffs: none".
- The 2023 relegation tie-break (arg-usa-22) ended 1-0 in normal time. So RSSSF does not show the "extra time, then penalties" part.
- The U.S. Open Cup was not held in 2020 and 2021. U.S. Soccer speaks of "a two-year hiatus due to the pandemic". The 2019 field had 84 clubs and the 2017 field had 99.
- The 2026 Argentine foreign-player rule is in dispute. Boca played six foreigners in a Copa Argentina match, and Vélez protested. Boca cites a change to the 2026 LPF text.
- The LPF regulation PDFs on ligaprofesional.ar for 2023 and 2025 have text only in the table of contents. The AFA Boletín 5853 PDF has full text.
- I found nothing on work permits for either nation.

## Spot check of arg-usa-clubs.tsv

I checked one season per division name against RSSSF. All four lists match club for club:

| Season | Division | Clubs | Second URL | Result |
|---|---|---|---|---|
| 2016/17 | Primera División | 30 | rsssf.org/tablesa/arg2017.html (relegation table) | same 30 clubs |
| 2008/09 | Primera B Nacional | 20 | rsssf.org/tablesa/arg2-09.html | same 20 clubs |
| 2021/22 | Primera Nacional (2022) | 37 | rsssf.org/tablesa/arg2022.html | same 37 clubs |
| 2019/20 | Major League Soccer (2020) | 26 | rsssf.org/tablesu/usa2020.html | same 26 clubs |

For MLS 2019/20 I also checked the database side in `October 2019.clubs.tsv`. Portland Timbers, Atlanta United and Inter Miami have no first-team record there (only Portland Timbers 2 and Atlanta United 2), and FC Dallas and New York Red Bulls sit under their old names. This matches the clubs file.
