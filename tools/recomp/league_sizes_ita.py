"""Italy at its real league sizes (0.7.2 league sizes, slice 9, D22): the instructions of the Italian classes that
read a fixed count. The runtime is src/recomp_rt/win/season_sizes_ita.c, which names what each site does. INSNS
replaces a whole instruction, as in league_sizes.py: address: (the instruction in the original, C code).
"""

_SZ1_CMP = "t2 = ((t0 - t1) & 0xffu); SETF(K_SUB | SZ1, t0, t1, t2, 0);"
_SZ2_CMP = "t2 = ((t0 - t1) & 0xffffu); SETF(K_SUB | SZ2, t0, t1, t2, 0);"


def _zone_mov(reg, idx, off, serie_b, plus=0):
    row = f"{idx} + {plus}u" if plus else idx
    return (f"mov {reg[1]}l, byte ptr [esp + {idx} + {off:#x}]",
            f"{reg} = ({reg} & 0xffffff00u) | (rt_ita_zone(esi, {serie_b}u, {row}, esp + {idx} + {off:#x}u) & 0xffu);")


def _zone_cmp(idx, off, serie_b):
    return (f"cmp bl, byte ptr [esp + {idx} + {off:#x}]",
            f"t0 = (ebx & 0xffu); t1 = rt_ita_zone(esi, {serie_b}u, {idx} + 1u, esp + {idx} + {off:#x}u) & 0xffu; "
            + _SZ1_CMP)


INSNS = {
    # Tie-break zones. ItaSerA::update_for_date 0x642880: rows eax and eax + 1 (loop 0x6429c7..0x6429f9).
    0x6429E3: _zone_mov("ecx", "eax", 0x18, 0),
    0x6429E7: _zone_mov("edx", "eax", 0x19, 0, 1),
    # ItaSerA::create_tiebreak_playoffs 0x641a70: rows ecx and ecx + 1 (loop 0x641b8a..0x641c76).
    0x641BBA: _zone_mov("ebx", "ecx", 0x18, 0),
    0x641BBE: _zone_cmp("ecx", 0x19, 0),
    # ItaSerB::update_for_date 0x647050: rows eax and eax + 1 (loop 0x647102..0x64714a).
    0x647123: _zone_mov("edx", "eax", 0x14, 1),
    0x647127: _zone_mov("ecx", "eax", 0x15, 1, 1),
    # ItaSerB::create_tiebreak_playoff_stage 0x646bb0: rows ecx and ecx + 1 (loop 0x646c3d..).
    0x646C6D: _zone_mov("ebx", "ecx", 0x14, 1),
    0x646C71: _zone_cmp("ecx", 0x15, 1),
}

# C1 play-out rows and seeds: ItaSerC1a::create_playoff_stages 0x64ba70 and ItaSerC1b::create_play_off_stages
# 0x64fc30, the object in esi (0x64bc1f, 0x64fddf).
for _at, _dst, _base, _off in ((0x64BC2C, "eax", "edx", 0x3B0), (0x64BC3A, "edx", "ecx", 0x2FF),
                               (0x64BC49, "ecx", "eax", 0x375), (0x64BC58, "eax", "edx", 0x33A),
                               (0x64FDEC, "eax", "edx", 0x3B0), (0x64FDFA, "edx", "ecx", 0x2FF),
                               (0x64FE09, "ecx", "eax", 0x375), (0x64FE18, "eax", "edx", 0x33A)):
    INSNS[_at] = (f"mov {_dst}, dword ptr [{_base} + {_off:#x}]",
                  f"{_dst} = R32({_base} + rt_ita_c1_row(esi, {_off:#x}u));")
# Group C of the Serie C era (ItaSerC2a::create_playoff_stages 0x653e90, the same code, object in esi).
for _at, _dst, _base, _off in ((0x654021, "eax", "edx", 0x3B0), (0x65402F, "edx", "ecx", 0x2FF),
                               (0x65403E, "ecx", "eax", 0x375), (0x65404D, "eax", "edx", 0x33A)):
    INSNS[_at] = (f"mov {_dst}, dword ptr [{_base} + {_off:#x}]",
                  f"{_dst} = R32({_base} + rt_ita_c1_row(esi, {_off:#x}u));")
# The club count of the play-out stage (the play-off count +0xbf, open item 253).
for _at in (0x64BCB1, 0x64FE71, 0x6540A6):
    INSNS[_at] = ("movsx dx, byte ptr [esi + 0xbf]",
                  "edx = (edx & 0xffff0000u) | (rt_ita_playout_count(esi, (uint32_t)(int32_t)(int8_t)R8(esi + 0xbfu)) "
                  "& 0xffffu);")
# A play-out of no tie (open item 253): no play-out stage. The stage's block reads as 0 (0x64bc7d), so the builder
# skips the stage constructor (je 0x64bc8e), and the empty slot (0x64bceb) jumps over the error box and set_up_round to
# the end (0x64bd63), where +0x30 stays 0 (advance_stage 0x64ba5b set it). C1/B and C2/A have the same code.
for _keep, _slot, _end in ((0x64BC7D, 0x64BCEB, 0x64BD63), (0x64FE3D, 0x64FEAB, 0x64FF23),
                           (0x654072, 0x6540E0, 0x654158)):
    INSNS[_keep] = ("mov dword ptr [esp + 0x1c], eax",
                    "eax = rt_ita_playout_block(esi, eax); W32(esp + 0x1cu, eax);")
    INSNS[_slot] = ("mov ecx, dword ptr [edx + 4]",
                    f"ecx = R32(edx + 4u); if (ecx == 0 && rt_ita_playout_none(esi)) goto L_{_end:x};")
for _first in (0x64BC61, 0x64FE21, 0x654056):
    for _k, _seed in enumerate((0x10, 0xD, 0xF, 0xE)):
        INSNS[_first + 5 * _k] = (f"mov byte ptr [esp + {0x14 + _k:#x}], {_seed:#x}",
                                  f"W8(esp + {0x14 + _k:#x}u, rt_ita_c1_seed(esi, {_seed:#x}u));")

# The C1 regroup, ita_ser_a_regroup_serie_c1_clubs 0x642630 (season_divisions_ita.c wraps it with
# season_sizes_ita_regroup_begin and _end).
INSNS.update({
    0x64269C: ("cmp dl, 0x24", "t0 = (edx & 0xffu); t1 = rt_ita_regroup_max(0x24u) & 0xffu; " + _SZ1_CMP),
    0x64269F: ("mov dword ptr [esp + esi*4 + 0x14], eax", "W32(rt_ita_regroup_buf(esp + 0x14u) + esi * 4u, eax);"),
    0x6426B2: ("movsx eax, dl", "eax = rt_ita_regroup_count((uint32_t)(int32_t)(int8_t)(edx & 0xffu));"),
    # After push 0x53fd40 and push 4 (0x6426b5, 0x6426ba) the array is at esp + 0x1c.
    0x6426BC: ("lea ecx, [esp + 0x1c]", "ecx = rt_ita_regroup_buf(esp + 0x1cu);"),
    0x6426D0: ("lea esi, [esp + 0x14]", "esi = rt_ita_regroup_buf(esp + 0x14u);"),
    0x6426D4: ("cmp ax, 0x12", "t0 = (eax & 0xffffu); t1 = rt_ita_regroup_split(0x12u) & 0xffffu; " + _SZ2_CMP),
    0x64275F: ("cmp ax, 0x24", "t0 = (eax & 0xffffu); t1 = rt_ita_regroup_end(0x24u) & 0xffffu; " + _SZ2_CMP),
})

# The C2 exchange, ItaSerA::exchange_serie_c2_clubs_unplayed 0x641fc0: C1/A at [esp + 0x10], C1/B at [esp + 0x14].
INSNS.update({
    0x64210C: ("movsx edi, word ptr [ecx + 0x3e]", "edi = rt_ita_c1_rows(ecx, R32(esp + 0x14u));"),
    0x642122: ("cmp byte ptr [eax + 0x37], 3",
               "t0 = rt_ita_row_status(R32(esp + 0x10u), esi, eax) & 0xffu; t1 = 3u; " + _SZ1_CMP),
    0x642161: ("cmp byte ptr [eax + 0x37], 3",
               "t0 = rt_ita_row_status(R32(esp + 0x14u), esi, eax) & 0xffu; t1 = 3u; " + _SZ1_CMP),
})

PATCHES = {}
