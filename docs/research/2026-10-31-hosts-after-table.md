# Hosts after the table (0.6.2 slice 5)

Decisions Q4, Q5, Q6 and Q32 of the 0.6.2 spec. Code: `src/recomp_rt/win/season_intl_rules.c` (the lookups,
ctest `season_intl`) and `season_intl.c` (the host rows of the game), `season_gold_cup.c` (the Gold Cup rows).

## 1. Announced hosts (Q5)

Each fact checked against a source on 31 October 2026.

| Edition | Hosts | Source |
|---|---|---|
| World Cup 2034 | Saudi Arabia | FIFA Extraordinary Congress, 11 December 2024, by acclamation (Al Jazeera, 11 December 2024: https://www.aljazeera.com/sports/2024/12/11/saudi-arabia-named-fifa-world-cup-2034-host-morocco-to-co-stage-2030) |
| Euro 2028 | England, Northern Ireland, Republic of Ireland, Scotland, Wales | UEFA Executive Committee, 10 October 2023 (https://www.uefa.com/insideuefa/news/0286-1923eef9a9a6-68c007509a1b-1000--euro-2028-and-2032-hosts-confirmed/) |
| Euro 2032 | Italy, Türkiye | the same UEFA decision |
| Olympics 2028 | United States (Los Angeles) | IOC Session, 13 September 2017 |
| Olympics 2032 | Australia (Brisbane) | IOC Session, 21 July 2021 |

Not taken: the men's Olympic tournament of 2028 has 12 teams (IOC Executive Board, April 2025; Wikipedia
"Football at the 2028 Summer Olympics"). The editions after the table keep the last table format (decision A), so
the game plays 16. A question for Max.

The Euros of 2020 (eleven host nations, no automatic place) and 2028 keep no host in the table, because a host of
the table takes an automatic place. Their history rows name the nation of the final: England (Wembley) in both
(`season_intl_history_host`).

## 2. The original's rows (Q4)

`HostCountry::init_default_table` 0x5f9970 (details: `run-logs/future-0/host-rule.md`, outside the repo):

| Class | Rows | Step | Moves on |
|---|---|---|---|
| World Cup | 1998 to 2026 | 4 | 32 |
| Euro | 2000 to 2028 | 4 | 32 |
| Olympics | 2000 to 2028 | 4 | 32 |
| Africa Cup of Nations | 2000 to 2014 | 2 | 16 |

`daily_update` 0x5fa850 moves a row on by its step on 1 January once it lies in the past (0x5faafa), resets a row
that is not fixed to "not decided" (0xfe), and draws its pair, random(3), 5 years before the finals on the row's
decision day (0x5faaa0..0x5faaae).

After the table, each edition takes the original's row of its slot: the rows in order, one an edition
(`season_intl_host_slot`). The World Cup of 2038 takes the 2006 row (moved on 32 years). The Africa Cup of Nations
plays in odd years after 2027 while its rows hold even years; its editions take the rows in order (2029 the 2012
row, 2031 the 2014 row, 2033 the 2000 row). `hosts_apply` writes a copy of each row on the first edition of its
slot after the table, with the original's pairs, choice and decision day, moving on as the original's row. The
original's own rows of these classes no longer move. A row whose draw day lies before the start of the game (a
date jump) is drawn at once.

## 3. Draws and nations that no longer exist (Q6, Q32)

A nation that no longer exists has no continent (+0x71 null). At 0x5faaae a drawn pair that names such a nation is
drawn again among the valid other pairs (`season_intl_pick_pair`, utils_random). With no valid pair, the row
becomes fixed with the rotation host.

Rotation (Q32): the Copa America, Asian Cup, OFC Nations Cup and Gold Cup, and a row with no valid pair. The nations
that hosted the class in the table and still exist, each once in the order of their first edition, one an edition
after the table. The Copa America: Colombia, Peru, Venezuela, Argentina, Chile, United States, Brazil. The Gold
Cup: United States, Canada, Costa Rica, Jamaica. Each host has a fixed row that moves on by all of them.

## 4. History rows and news

`Cup::record_season_honours` 0x519a90 asks for hosts only when slot +0x88 of the competition gives 1 (0x519b84):
the World Cup, Euro and Olympics (0x404480). The other finals give 0 (0x5dce00), so the original writes no host
into their history rows. At 0x519c2f a season game takes the first host of the edition's row (or of the edition,
for the OFC Nations Cup, which has no row). The Club World Cup keeps none: its row of every year holds a random
nation (0x5fab70), and from 0.6.2 the host of the edition while it plays (open item 125).

`daily_update` lists every row of the year 5 years ahead on its decision day for the "host chosen" news, fixed rows
too (0x5faa7f), and the news text of a row with no host is empty (0x77cb91). A row with no host gets a decision day
that never comes (0x7fff).
