# A club's finances in a saved game

26 November 2026. Asked by a player (a patch of CM 01/02 showed other clubs' finances, for example before applying
for a job). The game hides other clubs' finances; Squad Hub shows them, as the Scout shows potential.
Read from the clean 3.9.68 `cm0102.exe`; checked on the save "Hertha BSC 2011-12-11". Squad Hub shows them and,
since the same day, changes a club's balance, reputation and training facilities (below).

## Where the save keeps them

`FinanceTable::save_finance_dat` 0x59b3b0 writes the member `finance.dat`: one record of 0x167 (359) bytes for each
club of the game's club table, the count at [0xae235c] (0x59b41c: zwrite of 0x167 x count). `load_from_save`
0x59b2a0 reads it back. The table holds the clubs of `club.dat` and then the national teams of `nat_club.dat`: the
Hertha save has 10,911 + 426 = 11,337 records (4,069,983 bytes). A record's club index is at +0x08 (set by
`Finance::init_for_club` 0x5961d0 at 0x59625a); every sampled record of the save holds its own index.

## One record

| Offset | Size | What | Code |
|---|---|---|---|
| +0x00 | 8 | the balance | every income and payment adds to it (0x5987e1, 0x599835) |
| +0x08 | 4 | the club index | 0x59625a |
| +0x0c | 4 | the weekly wage bill: the sum of the wages | `update_wage_bill` 0x598370 (0x598437) |
| +0x10 | 4 | the top weekly wage | 0x59842b |
| +0x20 | 0x50 | last month | `get_last_month` 0x4671b0 |
| +0x70 | 0x50 | this month | `get_this_month` 0x4671c0 |
| +0xc0 | 0x50 | last year | `get_last_year` 0x4671d0 |
| +0x110 | 0x50 | this year | `get_this_year` 0x4671e0 |

A block of 0x50 bytes, as the functions that add to this month and this year write it:

| Offset | Size | What | Code |
|---|---|---|---|
| +0x00 | 8 | players bought | `pay_players_bought` 0x599890 |
| +0x08 | 8 | players sold | `add_players_sold` 0x599800 |
| +0x10 | 4 | other costs | `pay_other_costs` 0x5999e0 |
| +0x18 | 4 | league fines | `pay_league_fine` 0x599850 |
| +0x20 | 4 | wages | `pay_wages` 0x599920 |
| +0x28 | 4 | bonuses | `pay_bonuses` 0x599960 |
| +0x2c | 4 | signing-on fees | `pay_sign_on_fee` 0x5998e0 |
| +0x38 | 4 | prize money | `add_prize_money` 0x5999a0 |
| +0x48 | 4 | TV money | `add_tv_revenue` 0x5987c0 |

The other fields of a block (gate receipts, sponsors and the like) are written elsewhere and are not read here.

The Hertha save (game date 11 March 2012): Hertha BSC Berlin (4232) balance 59,516,431, wage bill 889,495 a week,
top wage 65,497, players sold this year 19,491,130, TV money 5,985,625; Liverpool (5583) balance 32,264,435, wage
bill 1,823,957, top wage 111,379.

## The transfer budget is worked out, not kept

`Finance::init_for_club` 0x5961d0 sets only the balance (+0x00, from the club record's cash +0x65 and its reputation
+0x80: 0x5962be, 0x5962c9, stored at 0x596587), the club index, +0x160 (0.1, 0x5962b4), +0x165, +0x166 and zeros; no
budget field. `Finance::get_transfer_budget` 0x598800 works the budget out each time it is asked: from the balance
(0x598807, 0x598fb5), the wage bill +0x0c (0x598b6b), this year's block +0x110 (0x598f1f), the squad's values, the
deals in progress, and the club's nation +0x53 (0x598b3f), attendance +0x73 (0x598cf1), training facilities +0x7f
(0x598c51) and reputation +0x80 (0x598cbe). A change of the balance therefore changes the budget the board gives. The
game's cash checks read the balance as a signed 32-bit figure (0x5986e3, docs/research/2026-10-16-crash-fixes.md), so
Squad Hub keeps a balance within -2,000,000,000..2,000,000,000.

## The club record

club.dat and then nat_club.dat, 581 (0x245) bytes a club, the club table at [0xae23b4] (0x5986b4: index x 0x245). The
fields Squad Hub shows:

| Offset | Size | What | Code |
|---|---|---|---|
| +0x00 | 4 | the id, equal to the row | 0x5267c0, 0x53b900 (docs/research/2026-10-19-club-table-users.md) |
| +0x04 | 51 | the name | |
| +0x53 | 4 | the nation | 0x5267c0, 0x598b3f |
| +0x73 | 4 | the attendance figure | create_game step 0x23 (0x829984) sets it from the reputation when it is 0 or less; 0x598cf1 |
| +0x7f | 1 | training facilities, 1..20 | create_game step 0x23: reputation / 500 cut to 1..20; 0x598c51 |
| +0x80 | 2 | reputation, up to 10,000 | 0x5962c9, 0x5986dc, 0x598cbe |

The Hertha save: Hertha BSC Berlin nation 74, attendance 78,513, training facilities 20, reputation 10,000; Liverpool
attendance 62,520, training 20, reputation 9,980.

Squad Hub changes the balance (finance.dat +0x00, 8 bytes), the reputation (1..10,000) and the training facilities
(1..20) of any club, as `writeClub` in `web/savedit.js`; the change is saved with the game file.
