"""Reads of the original that depend on where memory lies (0.6.2 open item 138b; src/recomp_rt/win/layout_reads.c).

A base game (changing rules off) gave other results when only the Setup Game screen of the runtime changed (Setup
Game menu B, 25298081): the screen's blocks moved the database tables to other heap addresses, and the game read
an address as a number, a heap block that nothing had set and stack bytes that nothing had set. None of these is
a value of the game: the original's own layout is not fixed either (its heap is the CRT heap of the computer it runs
on). So each read takes a defined value in its place: the record's id for the address, 0, as fresh memory, for the
unset bytes, 0x240, the mask of the original's last places, for the place masks of a national squad, and its place
in the table for the id of a new official. The fixes act in every game, the base game too. The test switch
RECOMP_LAYOUT_FIXES=keep, or keep:captain, keep:players, keep:daily, keep:stats, keep:loans, keep:squads,
keep:officials, keep:training, keep:contracts, keep:schedules, keep:fitness, turns them off (layout_reads.c).

The captain (MATCH_MAN::choose_captain 0x6e1940). For each player of the list it takes the captain score
(match_man_get_captain_score 0x6e1cf0). When the manager's rating (non_player_attribute_rating 0x544410 with the
non-player record [pick + 0x1f] and its byte +0x17) is below the player's staff id modulo 20 (0x6e19a8..0x6e19b7),
the score is scaled by 1.1 - 0.01 x n (constants 0x9689c0 and 0x967898, 0x6e19c5..0x6e19e2), with n from 0 to 19.
The original takes n as the address of the manager's non-player record modulo 20 (0x6e19b9 `mov eax, dword ptr
[ebx + 0x1f]`, then `cdq`, `idiv ecx` with ecx 20). Here n is the record's id modulo 20 (non-player +0x00), as the
test before it takes the player's staff id (0x6e19a8 `mov eax, dword ptr [esi]`). The scale stays one value per
manager from 0.91 to 1.10. The captain goes into the pick (+0x3b, 0x6dc8ad and 0x6dcee0), and the player score of a
pick (match_man_score_player_for_role 0x6e2f20) gives the captain 100 more (0x6e5af9..0x6e5b04), so the eleven
depended on the address too: in a 2001 base game the Cook Islands took another goalkeeper in their first World Cup
qualifier, and the games went apart from there.

The match players (MATCH_PLAYER::MATCH_PLAYER 0x6eada0). The match engine's constructor 0x6b7e50 makes its 40 match
players at +0x4796 (0x1be bytes each) with the vector constructor 0x9463af (0x6b7e69..0x6b7e85). The constructor
sets most fields but not +0x1d to +0x22 (and others), and MATCH_PLAYER::setup 0x6eaea0 fills only the slots that
get a player. After a match, match_eng_update_post_match_reputations 0x6d1cc0 sums the minutes of each slot of a
side (0x6d2090..0x6d25b9): a bench slot counts unless its +0x1f is 0xff (0x6d20b3), and a slot with no player
(+0x69 0, 0x6d20c2) goes to the minutes test (0x6d2557..0x6d25ac), which reads +0x1f, +0x20 and +0x21. A side
with fewer than 20 players has such slots, and the engine is a heap block that held other data before, so the
average (0x6d25e3..0x6d25fd) and with it the club's new reputation (match_eng_compute_club_reputation 0x6d3de0,
0x6d4015; stored at 0x6b7472) depended on what lay there: in a 2001 base game a friendly on day 173 left club 350
at 1000 with one Setup Game screen and at 1001 with the other. Here the constructor first clears the record, as
fresh memory is: an empty slot then has 0 minutes and adds nothing (0x6d2594, `test al, al`).

The daily staff update (game_daily_staff_update 0x5ce2d0). Its loop over all staff (0x5ce32d..) keeps a running
value in the local [esp + 0x70]: for a staff with a club it reads the value (0x5ce830 or 0x5ce84f), adds his byte
+0x59 (or a part of it), stores it back (0x5ce859, 0x5ce864, 0x5ce888) and adds it to the range of a later draw
(0x5ce88c to [esp + 0x20], drawn at 0x5cf116). Nothing sets the local before the loop, so the first staff starts
from what an earlier call left on the stack, and in a 2001 base game that was a heap address (0x73106304 with one
Setup Game screen, 0x73104602 with the other): before the 2001/02 season the draw at 0x5cf116 had the range 31821
or 22541, and the games went apart. Here the local is 0 when the function starts, where it already clears
[esp + 0x8a0] (0x5ce311).

The simulated matches (SimulatedStats::play_daily_matches 0x84be20). For each pair of clubs it plays (0x84c249..)
it builds a fixture (0x4f bytes) on its stack at [esp + 0x40] and gives it to the match engine (init_match 0x6b7f20,
call at 0x84c3e8; the engine keeps it at +0x4792, 0x6b7f4e). It sets the id -2, the clubs, the competition type and
the result bytes (0x84c271..0x84c331, VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 for the squads), but not the match
flags word +0x38 (2026-10-17-cup-round-record.md, section 2) and some other fields. quick_simulate_match 0x6beeb0
reads the flags at 90 minutes (0x6bfe8e..0x6bfee6: bit 0x80 golden goal, bit 0x02 extra time), so a level match
went to extra time and golden goal by what an earlier call left on the stack. In a 2001 base game the word held the
low half of a heap address (0x17c0 with one Setup Game screen, 0xefd0 with the other), and on day 251 a level match
had golden goal with one and not with the other. Here the fixture is 0 before each match, as fresh memory is (0x84c266,
the first instruction of each match): the flags are 0, and a level simulated match ends level at 90 minutes.

The loans of the database (TRANSFER_MANAGER::add_config_loan 0x8bf400). It builds a contract (0x50 bytes) on its
stack at [esp + 0x28] (STAFF_CONTRACT::STAFF_CONTRACT 0x85e410 at 0x8bf424, init_from_current_contract 0x85e590 at
0x8bf58e) and gives it to CONTRACT_MANAGER::sign_loan_contract 0x4d01e0 (0x8bf5b4), which copies it into the contract
table (STAFF_CONTRACT::operator= 0x4cc810 at 0x4d0786; byte +0x3d also at 0x4d0516 and 0x4d0675). Neither function
sets +0x3d, the player's morale at the club (-100 to 100; the sign-contract code sets it by squad status, 75, 50, 40
or 0, at 0x4cfadf..0x4cfaf1). Media::apply_story_effect_flags_fragment 0x737070 draws with the range +0x3d + 150
(0x737109..0x737114) and changes the player's morale by the result. In a 2001 base game the loan of staff 6943 to
club 657 had 0x5e with one Setup Game screen and 0x40 with the other, and on day 269 the draw ranges were 244 and
214. Here the contract is 0 before the constructor, as fresh memory is (0x8bf420): the loan starts at morale 0.

The national squads (NationalTeams::select_squad 0x76c120). It gives each squad place a position mask in the local
array [esp + 0x70] (one word per place) and passes it to match_man_assign_players_to_positions 0x6e2000 (0x76d035,
0x76d047) and to the place test at 0x76d092. The squad size is the byte +0x42 of the competition's squad record
(0x76c74e..0x76c767). For 26 places it sets 26 masks (0x76c771..0x76c84c), for 24 places 24 (0x76c870..0x76c92c),
and for any other size 22 (0x76c939..0x76c9de, with 0x76c860 and 0x76c865). A squad of 23 places (as in the 2002
World Cup qualifiers of Oceania) has a 23rd mask that nothing set: [esp + 0x9c] held what an earlier call left on the
stack (0xc2b0 with one Setup Game screen, 0xa570 with the other), the place took another player, and on day 26 of
2002 another squad went to the qualifier. The array has 50 words ([esp + 0x70] to [esp + 0xd3], the place loop
runs to the size byte, 0x76d384..0x76d394), so a size from 27 to 50 would leave more masks unset. Here the masks
from the 23rd to the 50th are 0x240 for the other sizes (0x76c939). That is the mask the original gives the last
places of each size it knows: places 19 to 22 of 22, 21 to 24 of 24 and 22 to 26 of 26 (0x76c82c..0x76c84c,
0x76c914..0x76c92c, 0x76c9c6..0x76c9de). So a squad of 23 places names 23 players, as it does in real life, and the
23rd place takes an outfield player as the 24-place squad's 23rd place does. A mask of 0 would leave the place empty
(0x76d092), so a squad of 23 would have 22 players. A mask of 0xffff would make it a fourth goalkeeper: the scorer
match_man_score_player_for_role 0x6e2f20 tests bit 1, the goalkeeper, first (0x6e30a0).

The new officials (officials_manager_add_random_officials 0x79c5c0). OfficialsManager::set_up_officials 0x79bf60 tops
up each nation's officials with it (0x79c2d5, 0x79c328) when a game starts: in a 2001 base game the table grows from
664 to 1246 officials. It grows the table (0x79c603) and fills each new record (loop 0x79c6aa..0x79c772, esi the
record + 8): names, nation, the day and year of the birth date and ratings, from draws. It does not set the id +0,
the birth date's leap year flag +0x10 (Date::Date 0x549ef0 sets it, at +4 of the date) or the word +0x14, so they
hold what the heap block held before. Then it sorts the table (0x79c79b) with
officials_manager_compare_nation_then_quality_asc 0x79d480: by nation, by quality, and for equal quality by the id
(0x79d4c2). So new officials of one nation with equal quality took their order from heap bytes, and set_up_officials
then gave each official its place as its id (0x79c381). In a 2001 base game official 468 was another person with
each Setup Game screen (born 1966 or 1960, current ability 59 or 72), and on 15 June 2002 MATCH_OFFICIAL::init
0x6e9470 compared his ability with draws (0x6e94b3) and the games went apart. Here each new record is 0 before the
loop fills it, and its id is its place in the table (0x79c6aa), as set_up_officials later gives every official.
After the loop's last fill the leap year flag follows the year by the rule of Date::Date (0x79c76a).

The missed training records (AWOL::miss_training 0x41a360). It builds the record (13 bytes) on its stack at [esp + 0x18]
(at 0x41a56b, after the pushes at 0x41a4c2 and 0x41a4c3): the staff id +0 (0x41a57b), the date +4 (Date::Date 0x54a110
at 0x41a381, 0x417c10 at 0x41a57f, 8 bytes) and a flag byte +0xc that nothing sets. It adds the 13 bytes to the missed
training list (0x68d0d0 at 0x41a58e). The only other code that adds to the list is AWOL::load (0x418892, the 13 bytes of
a saved game), and no code writes the flag. A week later AWOL::daily_update 0x4190d0 tests the flag (0x4191d2): set, the
player's own professionalism decides whether he misses training again (0x4191fc..); clear, his squad mates count as well
(0x4192c1..). In a 2001 base game the flag differed with the two Setup Game screens on day 268 (with the fix of the
officials). Here the flag is 0 at 0x41a56b, as fresh memory is.

The staff contract table (CONTRACT_MANAGER). CONTRACT_MANAGER::initialise_all_staff_contracts 0x4c7560 makes the first
table with one slot per staff member (0x4c759a, malloc 0x944e46 at 0x4c75a4) and fills the slots of the staff who have a
job (0x4c7873). get_free_staff_contract_space 0x4c79a0 gives the next slot after the last one in use (both callers ask
for that, 0x4c786f and 0x4c7953), and when the table is full it grows it by 200 slots (0x4c79dd) with realloc 0x945667
(0x4c79f1), or malloc 0x944e46 (0x4c7a02) when there is no table. Neither clears the new slots, so they hold what the
heap held before. CONTRACT_MANAGER::add_staff_contract 0x4c7950 has STAFF_CONTRACT::generate_initial_contract 0x85e960
fill a slot (0x4c798d), and it does not set the morale +0x3d. So a contract started with heap bytes as its morale. In a
2001 base game the contract of the regen staff 4346 at club 1483 (player_regen_sign_for_club 0x7a6f10, 4 January 2007)
had morale 0 with one Setup Game screen and 0x5e with the other, and on 6 January 2007 game_daily_staff_update 0x5ce2d0
compared it with a draw (0x5d00ad..0x5d00c2). Here the first table is 0 when it is made (0x4c75ae) and the new slots are
0 when it grows (0x4c7a0c), as fresh memory is. A table loaded from a saved game has one slot per saved contract
(load_from_disk 0x4db5c0, the count at 0x4db7e2 and malloc at 0x4db7f2), each read from the save.

The staff contract constructor (STAFF_CONTRACT::STAFF_CONTRACT 0x85e410). It sets the dates, the club, the flag fields
and the values, among them the word +0x3b (0x85e454), but not the morale +0x3d or the bytes +0x00..+0x03, +0x0c..+0x24,
+0x35..+0x3a, +0x3e, +0x3f, +0x44 and +0x4f. 39 sites build a contract with it, on their stack or in memory fresh from
new, and pass it on, for example to CONTRACT_MANAGER::sign_loan_contract 0x4d01e0 (0x8bf5b4, 0x8c2684), which sets no
morale. In a 2001 base game (with the fixes above) staff 6037 had a second contract, at club 1505, while his own
contract stayed at his club; its morale was 0x5c with one Setup Game screen and 0x3f with the other on 23 October 2002,
and in April 2003 the games went apart in MATCH_PLAYER code that reads that contract (0x6ebaf6..). Here the contract is
0 before the constructor (0x85e428), as fresh memory is. The sign-contract code then sets the morale by the contract
type for some types (0x4cfad8..0x4cfaf1) or copies it from the existing contract (0x4cf99f); other contracts, loans
among them, keep morale 0.

The training schedules (TRAINING_SCHEDULE::TRAINING_SCHEDULE 0x8b4e10). The training manager holds 167 schedules of 0x9b
bytes, built with this constructor (TRAINING_MANAGER::TRAINING_MANAGER 0x8b0ec0, 0x8b0ef8; the training edit session
builds 11 more at 0x8b0a3a and the schedule editor one at 0x8b989b, all in memory fresh from new), which sets the
header, the version and the flags but not the five category levels +4..+8. TRAINING_MANAGER::set_default_schedules
0x8b3d40 gives the seven default schedules and each human manager's slots a type with TRAINING_SCHEDULE::set_type
0x8b4e50, which fills the levels for the types 0 to 7. Slot 0 of each human manager gets the type -1 (0x8b3e63), and
set_type then fills nothing (0x8b4e64), so its levels held what the heap block held before.
TRAINING_MANAGER::get_player_schedule 0x8b39f0 gives that slot to a player at a human manager's club whose training
entry names slot 0 (0x8b3b46), and the weekly training (0x8b1d90, get_category_level 0x8b4f50) reads the level. In a
2001 base game (with the fixes above) the first human slot (+0x43d) had the level -84 with one Setup Game screen and 7
with the other on 19 August 2004, and a draw range at 0x8b1f43 was 60 or 134. Here each schedule is 0 before its
constructor (0x8b4e13), as fresh memory is: the levels of that slot are 0 until the manager sets them. A loaded game
reads its schedules from training.dat.

The fitness incident records (AWOL::report_poor_fitness 0x41add0). It builds the record (0x11 bytes) on its stack at
[esp + 0xc] (at 0x41ae41): the staff id +0 (0x41ae38), the club id +4 (0x41ae2a), the date +8 (Date::Date 0x54a110 at
0x41adf4, 0x417c10 at 0x41ae3c, 8 bytes) and a flag byte +0x10 ([esp + 0x1c]) that nothing sets, and adds the 0x11 bytes
to the fitness incident list (0x68d0d0 at 0x41ae4b). When an incident expires, AWOL::daily_update 0x4190d0 tests the
flag (0x41958b): set, when the staff record's +0x61 is set (0x4195a5) a draw against his +0x5b and, when that gives 0,
one against +0x5d (0x4195b0..0x4195d0); clear, a draw against +0x5b and one of 5 (0x4195d4..). In a 2001 base game (with
the fixes above) the flag differed with the two Setup Game screens on 24 June 2012. Here the flag is 0 at 0x41ae41, as
fresh memory is, as for the missed training records.
"""

# address: (the instruction in the original, C code)
INSNS = {
    0x6E19B9: ("mov eax, dword ptr [ebx + 0x1f]",
               "eax = rt_layout_fix(0u) ? R32(R32(ebx + 0x1fu)) : R32(ebx + 0x1fu);"),
    0x6EADA0: ("mov eax, ecx", "if (rt_layout_fix(1u)) memset(M + ecx, 0, 0x1beu); eax = ecx;"),
    0x5CE311: ("mov dword ptr [esp + 0x8a0], 0", "W32(esp + 0x8a0u, 0u); if (rt_layout_fix(2u)) W32(esp + 0x70u, 0u);"),
    0x84C266: ("mov eax, dword ptr [edi - 4]",
               "if (rt_layout_fix(3u)) memset(M + esp + 0x40u, 0, 0x4fu); eax = R32(edi - 4u);"),
    0x8BF420: ("lea ecx, [esp + 0x28]",
               "if (rt_layout_fix(4u)) memset(M + esp + 0x28u, 0, 0x50u); ecx = (uint32_t)(esp + 0x28u);"),
    0x76C939: ("mov word ptr [esp + 0x74], ax",
               "if (rt_layout_fix(5u)) for (uint32_t i = 0x9cu; i < 0xd4u; i += 2u) W16(esp + i, 0x240u); "
               "W16(esp + 0x74u, (uint16_t)eax);"),
    0x79C6AA: ("push edi",
               "if (rt_layout_fix(6u)) { uint32_t r_ = esi - 8u; memset(M + r_, 0, 0x2bu); "
               "W32(r_, (r_ - R32(0xae23e8u)) / 0x2bu); } t0 = edi; esp -= 4; W32(esp, t0);"),
    0x79C76A: ("mov word ptr [esi + 4], ax",
               "W16(esi + 4u, (uint16_t)eax); if (rt_layout_fix(6u)) { uint32_t y_ = R16(esi + 6u); "
               "W32(esi + 8u, y_ % 4u == 0u && (y_ % 100u != 0u || y_ % 400u == 0u) ? 1u : 0u); }"),
    0x85E428: ("mov esi, ecx", "if (rt_layout_fix(8u)) memset(M + ecx, 0, 0x50u); esi = ecx;"),
    0x4C75AE: ("mov dword ptr [ebx], eax",
               "if (rt_layout_fix(8u) && eax) memset(M + eax, 0, R32(ebx + 4u) * 0x50u); W32(ebx, eax);"),
    0x4C7A0C: ("mov dword ptr [esi], eax",
               "if (rt_layout_fix(8u) && eax) memset(M + eax + (R32(esi + 4u) - 200u) * 0x50u, 0, 200u * 0x50u); "
               "W32(esi, eax);"),
    0x8B4E13: ("lea ecx, [esi + 0x18]",
               "if (rt_layout_fix(9u)) memset(M + esi, 0, 0x9bu); ecx = (uint32_t)(esi + 0x18u);"),
    0x41AE41: ("mov ecx, dword ptr [edi + 8]", "if (rt_layout_fix(10u)) W8(esp + 0x1cu, 0u); ecx = R32(edi + 8u);"),
    0x41A56B: ("mov eax, dword ptr [esi]", "if (rt_layout_fix(7u)) W8(esp + 0x24u, 0u); eax = R32(esi);"),
}
