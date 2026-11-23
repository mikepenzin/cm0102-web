"""Network screens in widescreen mode (0.7.0 item 212, src/recomp_rt/win/net_scale.c): every coordinate on the
network is a classic one. The area create 0x55fba0 and ScreenManager::set_widget_rect 0x800890 write classic
x1, y1, x2, y2 into their packets (rt_net_classic_x/y, the exact inverse of display.c's scale; the identity in classic
mode). The packet applier's opcode 8 sub-op 1 widens the four words of a widget move before it uses them
(rt_net_rect_in16)."""

INSNS = {}
for _addr, _off, _axis, _far in ((0x55FC29, 0x214, "x", 0), (0x55FCB7, 0x218, "y", 0), (0x55FD45, 0x21C, "x", 1),
                                 (0x55FDD3, 0x220, "y", 1),      # Area create: x1, y1, x2, y2 into the block 0xae2908
                                 (0x800A9C, 0x218, "x", 0), (0x800B1D, 0x21C, "y", 0), (0x800B9E, 0x220, "x", 1),
                                 (0x800C1F, 0x224, "y", 1)):     # set_widget_rect: x1, y1, x2, y2 of opcode 8 sub-op 1
    INSNS[_addr] = (f"mov dx, word ptr [esp + {_off:#x}]",
                    f"edx = (edx & 0xffff0000u) | (rt_net_classic_{_axis}((uint32_t)(int32_t)(int16_t)R16(esp + {_off:#x}u), "
                    f"{_far}) & 0xffffu);")
INSNS[0x567157] = ("movsx ecx, word ptr [esp + 0x2c]",
                   "rt_net_rect_in16(esp + 0x28u, esp + 0x30u, esp + 0x38u, esp + 0x2cu); "
                   "ecx = (uint32_t)(int32_t)(int16_t)(R16(esp + 0x2cu));")
# The Display's drag in a joiner's click (Display::send_widget_click 0x568560): the joiner loads classic values, the
# host's opcode 6 case widens them where it stores them.
for _addr, _reg, _src, _axis, _far in ((0x569146, "edi", "edx", "x", 0), (0x569148, "ebx", "edx + 4", "y", 0),
                                       (0x56914B, "ebp", "edx + 8", "x", 1), (0x56914E, "eax", "edx + 0xc", "y", 1)):
    _c = _src.replace(" + 0xc", " + 0xcu").replace(" + 4", " + 4u").replace(" + 8", " + 8u")
    INSNS[_addr] = (f"mov {_reg}, dword ptr [{_src}]", f"{_reg} = rt_net_classic_{_axis}(R32({_c}), {_far});")
for _addr, _dst, _reg, _axis, _far in ((0x566D33, "eax", "edx", "x", 0), (0x566D3C, "eax + 4", "ecx", "y", 0),
                                       (0x566D3F, "eax + 8", "edx", "x", 1), (0x566D42, "eax + 0xc", "edi", "y", 1)):
    _c = _dst.replace(" + 0xc", " + 0xcu").replace(" + 4", " + 4u").replace(" + 8", " + 8u")
    INSNS[_addr] = (f"mov dword ptr [{_dst}], {_reg}", f"W32({_c}, rt_net_screen_{_axis}({_reg}, {_far}));")
