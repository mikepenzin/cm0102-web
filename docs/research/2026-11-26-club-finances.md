# A club's finances in a saved game

26 November 2026. Asked by a player (a patch of CM 01/02 showed other clubs' finances, for example before applying
for a job). The game hides other clubs' finances; Squad Hub shows them (read only), as the Scout shows potential.
Read from the clean 3.9.68 `cm0102.exe`; checked on the save "Hertha BSC 2011-12-11".

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
