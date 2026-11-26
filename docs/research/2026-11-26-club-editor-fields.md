# The club fields of the GK Save Game Editor in a saved game

26 November 2026. Asked for the club editor of Squad Hub (`web/savedit.js`, `web/editor.js`): a player should be able
to change what Graeme Kelly's "GK Save Game Editor" changes on its Edit Club screen. For each field: the save member,
offset, size and values, an address of the clean 3.9.68 `cm0102.exe` that reads or writes it, and the value on the save
"Hertha BSC 2011-12-11" (game date 11 March 2012) for Hertha BSC Berlin (club 4232) and Liverpool (5583). The values
in the player's screenshot (Everton) come from another game and are not compared here; the Hertha save has Everton
(3106) at Kings Dock (the fixed move of 2004, `FinanceTable::daily_update` 0x59b9c0).

Offsets are of the record in the save member. The save writes every pointer of a record as the id of the record it
points to, -1 for none (`save_database` 0x52dad0, `docs/research/2026-10-19-club-table-users.md` section 2), so a link
field in the save is a 4-byte signed id.

## The members

| Member | Record | Count on the save | Table id (index.dat) |
|---|---|---|---|
| `club.dat`, then `nat_club.dat` | 0x245 (581) | 10,911 + 426 | 0, 1 |
| `finance.dat` | 0x167 (359), one per club row, same order | 11,337 | written by `FinanceTable::save_finance_dat` 0x59b3b0 |
| `stadium.dat` | 0x4e (78) | 8,917 | 5 |
| `city.dat` | 0x38 (56) | 6,682 | 21 |
| `colour.dat` | 0x3a (58) | 34 | 2 |
| `staff.dat` | 0x6e (110) | 158,855 | 6 |

Each member's size is count x record size on the save (695,526 = 8,917 x 78; 374,192 = 6,682 x 56; 1,972 = 34 x 58).
A club's row in `club.dat` is its id (+0x00). A record is found by its id: row = id for every member above.

## The club record (club.dat)

| GK field | Offset | Size | Values | Code | Hertha | Liverpool |
|---|---|---|---|---|---|---|
| Professional Status | +0x64 | 1 | 1 Professional, 2 Semi-Professional, 3 Amateur; any other "Unknown" | club info tab `club_screen_draw_info_tab` 0x460ec6; `Finance::init_for_club` 0x596280..0x5962b0 makes an amateur club of a running division semi-professional at a new game | 1 | 1 |
| Training | +0x7f | 1 | 1..20 | season end 0x59d346 (stops at 20, 0x59d46b), club info 0x461506 | 20 | 20 |
| Reputation | +0x80 | 2, signed | 1..10,000 | `Finance::get_wealth_band` 0x5986dc, club info 0x461525 | 10,000 | 9,980 |
| Public Limited Company? | +0x82 | 1 | 0 no, not 0 yes (the data holds 1 and 255) | set to 1 with the news "become a PLC" (0x59e3de, text 0x59f50e); cleared when receivers come in (0x59eee3); `Finance::init_for_club` gives a PLC a higher maximum wage (0x596814) | 0 | 0 |
| Has Linked Club? | +0x244 | 1 | 1 when the club has a senior side or B team of its pairing table that plays in a league | written by `database_trim_senior_and_b_team_squads` 0x541ea2 / 0x541f65 at every new game, read by `club_get_linked_b_team` 0x540ae3. The game makes it; GK greys it | 1 | 0 |
| Euro Seeding | +0x1df | 1, signed | -1 (255) none; the European seed the UEFA and CONMEBOL seeding give a qualified club (1 for the first places, then by place) | written by `UefaSeeding::assign_champions_league_places` 0x904307, 0x9044db, `assign_uefa_cup_places` 0x904624, `assign_intertoto_places` 0x904ee1; read by the Champions League pot sort `european_cup_compare_clubs_by_seed` 0x586f6d (lower seed first, then reputation) | 1 | 1 |
| (the European cup the club entered) | +0x1db | 4 | club_comp id, -1 none (326 European Champions Cup) | set beside +0x1df (tested at 0x9042f9); read by the squad tab 0x45939d | 326 | 326 |
| Favourite Players/Staff | +0x9b, +0x9f, +0xa3 | 4 each | staff id, -1 none | `club_add_favoured_staff` 0x54788a..0x5478a6 | 131172 Fernando Cavenaghi, 3638 Marcel Nickel, 135308 Darren Burke | 130845 Julio Arca, 46531 Karl Thorpe, 90277 Andrea Pirlo |
| Disliked Players/Staff | +0xa7, +0xab, +0xaf | 4 each | staff id, -1 none (a slot can be empty before a full one) | `club_dislikes_staff` 0x5477e0..0x5477f0 | 137126 França, -1, -1 | 27649 Fabio Naldi, -1, 52404 David Beckham |
| Rival Clubs | +0xb3, +0xb7, +0xbb | 4 each | club id, -1 none | the match crowd tests the away club against the home club's rivals (`Stadium::calculate_match_attendance` 0x85d598) | 9443 Tennis Borussia Berlin, 8071 FC Hansa Rostock, 9973 1.FC Union Berlin | 5903 Manchester United, 3106 Everton, 731 Arsenal |
| Home Text, Home Back | +0x83, +0x87 | 4 each | colour id (row of colour.dat), -1 none | `database_get_club_kit_colours` 0x53b6ec: kit 1 takes +0x83 as the foreground and +0x87 as the background | 15 Blue 3, 1 White | 1 White, 4 Red 1 |
| Away Text, Away Back | +0x8b, +0x8f | 4 each | as above | 0x53b6fe (kit 2); with either -1 it shows the home pair swapped (0x53b730) | 1 White, 15 Blue 3 | 1 White, 0 Black |
| 3rd Text, 3rd Back | +0x93, +0x97 | 4 each | as above | 0x53b71c (kit 3), the same fallback | 0 Black, 4 Red 1 | -1, -1 |
| Stadium (link) | +0x69 | 4 | stadium id, -1 none | club info 0x4610d4 | 680 Olympiastadion | 68 Anfield |
| Club Owns Stadium? | +0x6d | 1 | 0 rented, 1 owned | club info 0x4610d1 ("(Rented Stadium)"); the season end buys the ground (0x59d12c) | 1 | 1 |
| Average Attendance | +0x73 | 4, signed | crowd | the home crowd of a match starts from it (0x85d2ef); the season end moves it (`game_update_league_attendances` 0x5d6541) | 78,513 | 62,520 |
| Minimum Attendance | +0x77 | 4, signed | crowd | the home crowd is at least this (0x85d537) | 30,000 | 25,200 |
| Maximum Attendance | +0x7b | 4, signed | crowd | the home crowd is at most this (0x85d52c); the season end raises it to a new average above it (0x5d6590) | 78,513 | 62,520 |
| Home Match Day | +0x72 | 1 | weekday 0 Monday, 1 Tuesday, 2 Wednesday, 3 Thursday, 4 Friday, 5 Saturday, 6 Sunday; 255 none | `FIXTURE_MANAGER::add_fixture` 0x5aa774: a league fixture of the home club that falls on a Saturday or Sunday moves to the nearest day of +0x72 when it is 4, 5 or 6 (`Date::get_weekday` 0x54a3f0, `Date::get_nearest_weekday` 0x54a2d0). Other values are not used there | 5 | 5 |

Bank Balance is not in the club record: the club record's cash +0x65 (4 bytes) only seeds the balance at a new game
(`Finance::init_for_club` 0x5962be); the balance of a running game is `finance.dat` +0x00 (below). Hertha +0x65 is
15,000,000, Liverpool 108,370,000.

Weekdays: `Date::get_weekday` 0x54a3f0 counts days from 1 January 1600 and returns 0 for Monday; worked by hand for
1 January 2002 (day 0 of 2002) it gives 1, a Tuesday, which is right. On the save, 765 club rows hold 5 and 534 hold 6,
9,482 hold 255. The player's screenshot shows "Thursday" for Everton; every English club of this save holds 5. GK's
day list may start at another day (5 is Thursday in a list that starts with Saturday); not confirmed.

Euro Seeding: only the writers above were read. What each seed number means past "1 is the first pot" is not
confirmed; the seeding writes it again each season for the clubs it qualifies, so an edit lasts until the next
seeding.

## The finance record (finance.dat)

The layout is in `docs/research/2026-11-26-club-finances.md`. The GK fields:

| GK field | Offset | Size | Values | Code | Hertha | Liverpool |
|---|---|---|---|---|---|---|
| Bank Balance | +0x00 | 8, signed | money; the cash checks read the low 32 bits signed, so keep it within -2,000,000,000..2,000,000,000 | 0x5987e1 | 59,516,431 | 32,264,435 |
| Current Maximum Wage | +0x10 | 4 | the highest weekly wage the club pays now | `Finance::update_wage_bill` 0x59842b | 65,497 | 111,379 |
| Highest Wage Allowed | +0x14 | 4 | the club's maximum weekly wage | written by `Finance::init_for_club` (0x596830..0x5968c2, from reputation and the PLC flag), `Finance::adjust_max_wage` 0x598361 (a signed percentage), `Finance::update_wage_bill` 0x59869d (= `Finance::get_max_wage` 0x5968d0 with no staff); read by `get_max_wage` 0x596bba (an offer is at least this, a free agent's at most twice it, 0x596ba7) | 79,554 | 132,966 |
| Bankrupt? | +0x165 | 1 | 0 normal, 1 receivers in, 2 out of receivership but still in debt | see below | 0 | 0 |

`FinanceTable::process_club_finance_events` 0x59ed70 sets +0x165 from the wealth band (`Finance::get_wealth_band`
0x5986b0, -2..2): band -2 (deep debt) posts the crisis news (`finance_post_financial_crisis_news` 0x59fc60), clears the
PLC flag (club +0x82) and sets +0x165 to 1 (0x59eef3); band -1 sets 2 (0x59ee4f); bands 0..2 set 0 (0x59f053), each
after the "receivers appeased" news (0x5a02f0) when it was 1. The club info tab shows "Bankrupt" when the band is -2
and +0x165 is 1 (0x46102e..0x461042), and a club with +0x165 = 1 must accept offers
(`transfer_manager_selling_club_must_accept` 0x8cd371). On the save 395 clubs hold 1, 753 hold 2.

Which GK checkbox maps to +0x165 is inference: no other byte of the record reads as bankrupt in the code. GK's labels
"Highest Wage Allowed" = +0x14 and "Current Maximum Wage" = +0x10 are inference too (the screenshot's pair, 49,550 above
30,449, fits a limit above the top wage).

The game works three of these out again: +0x10 and +0x14 at every contract change and in the daily finance pass
(`update_wage_bill` is called by `CONTRACT_MANAGER::sign_contract` 0x4cfe10, `remove_contract` 0x4d0bbe,
`FinanceTable::daily_update` 0x59bf7f and others), +0x165 at each run of 0x59ed70. An edit of them lasts until then;
the balance stays.

## The stadium record (stadium.dat)

| GK field | Offset | Size | Values | Code | Hertha (680) | Liverpool (68) |
|---|---|---|---|---|---|---|
| (id) | +0x00 | 4 | the row | | 680 | 68 |
| stadium name | +0x04 | 51 | Windows-1252, NUL-ended | club info 0x461205 | Olympiastadion | Anfield |
| (grammar byte) | +0x37 | 1 | passed with the name to the text code (0x461120) | | 255 | 255 |
| the city (link) | +0x38 | 4 | city id | club info 0x461111 | 551 Berlin | 1029 Liverpool |
| Capacity | +0x3c | 4, signed | places; 0 shows no capacity line | club info 0x4612e6; the season end adds to it (0x59d7fd) | 76,243 | 56,650 |
| Seating | +0x40 | 4, signed | seats; equal to capacity is "all seater" | club info 0x4612f1 | 76,243 | 56,650 |
| Expansion | +0x44 | 4, signed | the capacity the ground can grow to | the season end's expansion step (0x59d8e7, +1,500 at 0x59d882) | 1,950 | 79,500 |
| (a nearby stadium) | +0x48 | 4 | stadium id, -1 none | | 702 | 43 |
| Retractable Roof? | +0x4c | 1 | 0 no, not 0 yes (data 255, the game writes 1) | club info 0x4612f6; season end 0x59d644 | 1 | 0 |
| Undersoil Heating? | +0x4d | 1 | 0 no, not 0 yes | club info 0x4612fd; season end 0x59d5cb | 255 | 255 |

The season-end rules and limits (expansion only up to 125,000, the board refuses a request above 75,000) are in
`docs/research/2026-10-16-stadium-research.md`. A stadium can be shared: an edit changes it for every club whose +0x69
names it (the club info tab looks for them at 0x4610f4).

## The city record (city.dat)

| GK field | Offset | Size | Values | Code | Berlin (551) | Liverpool (1029) |
|---|---|---|---|---|---|---|
| (id) | +0x00 | 4 | the row | | 551 | 1029 |
| city name | +0x04 | 26 | Windows-1252, NUL-ended | club info 0x4611ea | Berlin | Liverpool |
| (grammar byte) | +0x1e | 1 | 255 in every row | 0x46111c | 255 | 255 |
| (nation) | +0x1f | 4 | nation id | | 74 | 61 |
| (latitude, longitude) | +0x23, +0x2b | 8 each | doubles (Berlin 52.5, 13.4) | | | |
| City Attraction | +0x33 | 1, signed | 0..20 on the save; 0 means unknown | `staff_interest_in_job_at_club` 0x845464 reads it through club +0x69 and stadium +0x38, and with 0 takes half the club's reputation instead | 20 | 20 |
| (unknown) | +0x34 | 1 | 0..43, 254 in 3,960 rows | not read here | 1 | 2 |

A city is shared by its stadiums: Anfield and Kings Dock both name Liverpool (1029), so an edit of the attraction
changes it for Liverpool and Everton.

## The colour record (colour.dat)

| Offset | Size | What | Code |
|---|---|---|---|
| +0x00 | 4 | id, the row | |
| +0x04 | 51 | name | |
| +0x37, +0x38, +0x39 | 1 each | red, green, blue | `database_get_club_kit_colours` 0x53b740..0x53b766 passes them to 0x5e4800 |

The 34 colours of the save (id name R,G,B):

| | | | |
|---|---|---|---|
| 0 Black 0,0,0 | 1 White 255,255,255 | 2 Grey 1 128,128,128 | 3 Grey 2 112,112,144 |
| 4 Red 1 224,0,0 | 5 Red 2 176,0,0 | 6 Red 3 144,16,0 | 7 Orange 1 255,112,0 |
| 8 Orange 2 224,128,0 | 9 Yellow 1 255,240,0 | 10 Yellow 2 255,208,0 | 11 Green 1 0,128,48 |
| 12 Green 2 0,96,48 | 13 Blue 1 0,32,96 | 14 Blue 2 0,32,128 | 15 Blue 3 0,48,160 |
| 16 Blue 4 0,80,208 | 17 Blue 5 96,192,255 | 18 Purple 1 128,0,64 | 19 Purple 2 96,0,96 |
| 20 Purple 3 128,0,32 | 21 Brown 1 128,64,0 | 22 Brown 2 160,80,0 | 23 Pink 255,149,149 |
| 24 Gold 217,177,40 | 25 Grey 3 198,198,198 | 26 Purple 4 206,132,206 | 27 Green 3 0,136,136 |
| 28 Green 4 128,200,72 | 29 Amber 255,170,0 | 30 Cyan 1 16,168,168 | 31 Cyan 2 5,97,97 |
| 32 Red 4 223,30,122 | 33 Green 5 0,62,48 | | |

## Not confirmed

- That GK's "Bankrupt?" is finance +0x165, and which of +0x10 / +0x14 GK calls "Highest Wage Allowed" and "Current
  Maximum Wage" (the offsets and what the game does with them are confirmed; GK's mapping is not).
- What a Euro Seeding number above 1 means, and whether the game ever reads +0x1df of a club outside the European and
  CONMEBOL draws.
- Which day number GK writes for each name of its Home Match Day list (the game's numbering is confirmed).
- City +0x34 and stadium +0x37, city +0x1e: their meaning.
- The game sets no range on the attendance fields, capacity, seating or expansion when it reads them; the limits above
  are the ones its own rules use. Squad Hub should keep minimum <= average <= maximum and seating <= capacity, as the
  data does (not a rule found in code).

## Scripts

Read with `web/savedit.js` (`SaveGame`, `file.read(member)`), scripts kept outside the repo. The code was read with a
capstone disassembly of the clean 3.9.68 `cm0102.exe`.
