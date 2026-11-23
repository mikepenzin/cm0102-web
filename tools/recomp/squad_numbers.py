"""Squad numbers 1 to 99 (GitHub issue 140, 0.7.3 slice 13; src/recomp_rt/win/squad_numbers.c).

SQUAD_MANAGER::set_squad_number 0x85a850 takes -50 to 50 (0x85a87f, 0x85a888) and shows a box for any other number. It
takes -99 to 99 in both modes, because a save made with the option on can hold such numbers with the option off.

A new game cuts a database number above 50 to 50 in two places, and both now use the option's limit: the contract build
of CONTRACT_MANAGER (0x4c78ae..0x4c78c5, written negative at 0x4c7904) and the loader 0x859e20 (0x859f6b..0x859f6f).

assign_club_squad_numbers 0x859fe0 keeps the holder of each number in int[50] at [esp + 0x110], indexed by the number
(0x85a293, 0x85a29a, 0x85a328). Its last loops write the numbers back with the sign of the club: 1 to 50 at a human
club (0x85a40b..0x85a427), -1 to -50 at another (0x85a440..0x85a458). Numbers 51 to 99 use the runtime's table, and the
loops run on to 99.

allocate_squad_number 0x85a470 keeps 50 flags on the stack (written at 0x85a500). It takes a number for the position,
then 12 to 50 (0x85a676..0x85a686), then 11 down to 1 (0x85a6e6..0x85a6f3). When those are all taken, the patch at
0x85a6f3 goes on with the first free number from 51 to 99, through the original's own write at 0x85a700. The flag write
at 0x85a740 stays inside the table. The search also reads the club's contracts, because a player who holds a number
above 50 is outside the list of 50 that 0x541f90 gives.

move_player_to_club 0x8c9720 saves the player's number (0x8c9dfe, 0x8c9f1c) and sets it at the new club (0x8ca7d4). With
the option off, a number above 50 does not go with him.

squad_numbers_screen_build 0x475520 draws the numbers as two strips of 25 buttons, 1 to 25 above the player list and 26
to 50 below it (0x477640..0x477821). With the option on, the runtime draws each strip with two rows, 1 to 50 above and
51 to 99 below, and the list between them is shorter (0x47785a, 0x477862). The event handler 0x47a9e0 takes the number
from the button's data, so a drop needs no change. Its search for the first free number (0x47bad5..0x47bb2e) stays at 50:
it reads the screen's list of at most 50 players, so a player with no number always leaves a free number from 1 to 50.
"""

INSNS = {
    # set_squad_number: the range. The cmp before each jump stays and its flags are not read.
    0x85A882: ("jl 0x85a940", "if ((int8_t)ebx < -99) goto L_85a940;"),
    0x85A88B: ("jg 0x85a940", "if ((int8_t)ebx > 99) goto L_85a940;"),
    # The contract build of a new game: the highest database number that the game keeps. The cmp at 0x4c78bc stays.
    0x4C78BE: ("mov ecx, 0x32", "ecx = rt_squad_number_top();"),
    0x4C78C3: ("jg 0x4c78c8", "if ((int32_t)(int8_t)eax > (int32_t)rt_squad_number_top()) goto L_4c78c8;"),
    # The new-game loader: the same limit.
    0x859F6D: ("jle 0x859f76", "if ((int32_t)(int8_t)eax <= (int32_t)rt_squad_number_top()) goto L_859f76;"),
    0x859F6F: ("mov eax, 0x32", "eax = rt_squad_number_top();"),
    # assign_club_squad_numbers: the table of holders.
    0x859FEE: ("mov ecx, 0x32", "rt_squad_assign_begin(); ecx = 0x32u;"),
    0x85A293: ("mov eax, dword ptr [esp + ecx*4 + 0x10c]", "eax = rt_squad_assign_taken(esp, ecx);"),
    0x85A29A: ("lea esi, [esp + ecx*4 + 0x10c]", "esi = rt_squad_assign_slot(esp, ecx);"),
    0x85A328: ("mov dword ptr [esi], eax", "rt_squad_assign_keep(esi, ebx, eax);"),
    0x85A40B: ("mov eax, dword ptr [esi]", "eax = rt_squad_assign_holder(esi, (int32_t)(int8_t)ebx + 1);"),
    0x85A427: ("jl 0x85a40b", "if ((int8_t)ebx < 99) goto L_85a40b;"),
    0x85A440: ("mov eax, dword ptr [esi]", "eax = rt_squad_assign_holder(esi, -(int32_t)(int8_t)ebx);"),
    0x85A458: ("jg 0x85a440", "if ((int8_t)ebx > -100) goto L_85a440;"),
    # allocate_squad_number: the flags, and the numbers above 50 when 1 to 50 are taken.
    0x85A48F: ("mov ecx, 0xc", "rt_squad_allocate_begin(); ecx = 0xcu;"),
    0x85A500: ("mov byte ptr [esp + eax + 0x1b], 1", "if (!rt_squad_allocate_mark(eax)) W8(esp + eax + 0x1bu, 1);"),
    0x85A6F3: ("jns 0x85a6e6",
               "if (!cc_s(F)) goto L_85a6e6; "
               "{ uint32_t n_ = rt_squad_allocate_high(ebp); "
               "if (n_) { ebx = (ebx & 0xffffff00u) | (n_ - 1u); goto L_85a700; } }"),
    0x85A740: ("mov byte ptr [esp + eax + 0x18], 1", "if ((int8_t)eax < 50) W8(esp + eax + 0x18u, 1);"),
    # squad_numbers_screen_build 0x475520: the two strips of numbers and the list between them.
    0x477640: ("movsx eax, word ptr [0xb77c7a]",
               "{ uint32_t area_ = rt_squad_screen_strips(R32(esp + 0x14u), R32(esp + 0x38u)); "
               "if (area_ != 0x80000000u) { ebp = area_; ebx = 51u; goto L_477827; } } "
               "eax = (uint32_t)(int32_t)(int16_t)R16(0xb77c7au);"),
    0x47785A: ("mov esi, 0x1fe", "esi = rt_squad_screen_list_bottom();"),
    0x477862: ("mov edi, 0xeb", "edi = rt_squad_screen_list_top();"),
    # move_player_to_club 0x8c9720: the number that the player takes to the new club (set at 0x8ca7d4).
    0x8CA7C7: ("mov ecx, dword ptr [esp + 0x2c]", "ecx = rt_squad_carried(R32(esp + 0x2cu));"),
}
