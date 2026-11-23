/* WSOCK32 (real sockets, net_core.c) and printing (GDI32, WINSPOOL, comdlg32: no printer). */
#include "win.h"
#include "net_core.h"
#include "../host_net.h"

#include <stdio.h>
#include <string.h>

/* WSOCK32 by ordinal (0.7.0 slice 2, #45): real sockets through net_core.c. The ordinals are those of the
 * game's import table 0x967214..0x967258 (docs/research/2026-10-27-network-code-reading.md, section 0):
 * 12 is ioctlsocket and 51 is gethostbyaddr. A Windows sockaddr_in is 16 bytes: family (u16), port
 * (big-endian), address (network order), 8 zero bytes. */
#define SOCKET_ERROR 0xffffffffu
#define FIONBIO 0x8004667eu
#define FIONREAD 0x4004667fu

static uint32_t result(long r) { return r < 0 ? SOCKET_ERROR : (uint32_t)r; }

/* The address and port of a Windows sockaddr_in in the game's memory. 0 when it is not IPv4. */
static int get_addr(uint32_t p, uint32_t len, uint32_t *addr, uint16_t *port)
{
    if (!p || (int32_t)len < 16) return nc_set_error(HN_FAULT), 0;
    if (rd16(g_mem, p) != 2) return nc_set_error(HN_AFNOSUPPORT), 0;
    *port = (uint16_t)(g_mem[p + 2] << 8 | g_mem[p + 3]);
    memcpy(addr, g_mem + p + 4, 4);
    return 1;
}

/* Writes a sockaddr_in and its length, where the game asks for them. The input value of the length is
 * not read: recvfrom's caller 0x77af20 never sets it (0x77af90). */
static void put_addr(uint32_t p, uint32_t len_p, uint32_t addr, uint16_t port)
{
    if (!p) return;
    memset(g_mem + p, 0, 16);
    wr16(g_mem, p, 2);
    g_mem[p + 2] = (uint8_t)(port >> 8), g_mem[p + 3] = (uint8_t)port;
    memcpy(g_mem + p + 4, &addr, 4);
    if (len_p) wr32(g_mem, len_p, 16);
}

RT_IMPORT(WSOCK32_ord115)                                                         /* WSAStartup */
{
    uint32_t data = rt_arg(1);
    net_lobby_install();
    if (nc_start() < 0) rt_ret_stdcall(HN_SYSNOTREADY, 2);
    else {
        /* WSADATA: wVersion, wHighVersion, then text the game does not read. The game keeps 398 bytes for
         * it at TcpIp + 0x328, before the "Winsock started" flag at + 0x4b6, so the runtime writes 398. */
        if (data) {
            memset(g_mem + data, 0, 398);
            wr16(g_mem, data, 0x0101);
            wr16(g_mem, data + 2, 0x0202);
            win_put_str(data + 4, "WinSock 2.0", 257);
            win_put_str(data + 261, "Running", 129);
            wr16(g_mem, data + 390, 0xffff);
            wr16(g_mem, data + 392, 65467);
        }
        rt_ret_stdcall(0, 2);
    }
}
RT_IMPORT(WSOCK32_ord116) { rt_ret_stdcall(0, 0); }                                /* WSACleanup */
RT_IMPORT(WSOCK32_ord111) { rt_ret_stdcall((uint32_t)nc_error(), 0); }             /* WSAGetLastError */
RT_IMPORT(WSOCK32_ord57)                                                           /* gethostname */
{
    uint32_t buf = rt_arg(0);
    int32_t len = (int32_t)rt_arg(1);
    char name[256];
    if (!buf || len <= 0) {
        nc_set_error(HN_FAULT);
        rt_ret_stdcall(SOCKET_ERROR, 2);
        return;
    }
    if (hn_hostname(name, sizeof name) < 0) snprintf(name, sizeof name, "localhost");
    win_put_str(buf, name, (uint32_t)len);
    rt_ret_stdcall(0, 2);
}
/* gethostbyaddr: an entry for every IPv4 address, with the address as its name, and no DNS look-up. The
 * game only tests the result for null (TcpIp::can_resolve_address 0x8aee00), and a look-up that waits
 * froze the game list. */
RT_IMPORT(WSOCK32_ord51)
{
    static uint32_t entry;         /* hostent (16 bytes), name (16), aliases {0}, list {addr, 0}, addr */
    uint32_t a = rt_arg(0), len = rt_arg(1), type = rt_arg(2), ip;
    char text[16];
    if (!a || len != 4 || type != 2) {
        nc_set_error(HN_AFNOSUPPORT);
        rt_ret_stdcall(0, 3);
        return;
    }
    if (!entry) entry = win_static_alloc(64);
    memcpy(&ip, g_mem + a, 4);
    const uint8_t *b = g_mem + a;
    snprintf(text, sizeof text, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    uint32_t name = entry + 16, aliases = entry + 32, list = entry + 36, addr = entry + 44;
    win_put_str(name, text, 16);
    wr32(g_mem, entry, name);
    wr32(g_mem, entry + 4, aliases);
    wr16(g_mem, entry + 8, 2);
    wr16(g_mem, entry + 10, 4);
    wr32(g_mem, entry + 12, list);
    wr32(g_mem, aliases, 0);
    wr32(g_mem, list, addr);
    wr32(g_mem, list + 4, 0);
    memcpy(g_mem + addr, &ip, 4);
    rt_ret_stdcall(entry, 3);
}
RT_IMPORT(WSOCK32_ord23) { rt_ret_stdcall(nc_socket((int)rt_arg(0), (int)rt_arg(1)), 3); }   /* socket */
RT_IMPORT(WSOCK32_ord3) { rt_ret_stdcall(result(nc_close(rt_arg(0))), 1); }                 /* closesocket */
RT_IMPORT(WSOCK32_ord1)                                                                     /* accept */
{
    uint32_t addr = 0;
    uint16_t port = 0;
    uint32_t s = nc_accept(rt_arg(0), &addr, &port);
    if (s != NC_INVALID) put_addr(rt_arg(1), rt_arg(2), addr, port);
    rt_ret_stdcall(s, 3);
}
RT_IMPORT(WSOCK32_ord2)                                                                     /* bind */
{
    uint32_t addr;
    uint16_t port;
    long r = get_addr(rt_arg(1), rt_arg(2), &addr, &port) ? nc_bind(rt_arg(0), addr, port) : -1;
    rt_ret_stdcall(result(r), 3);
}
RT_IMPORT(WSOCK32_ord4)                                                                     /* connect */
{
    uint32_t addr;
    uint16_t port;
    long r = get_addr(rt_arg(1), rt_arg(2), &addr, &port) ? nc_connect(rt_arg(0), addr, port) : -1;
    rt_ret_stdcall(result(r), 3);
}
RT_IMPORT(WSOCK32_ord13) { rt_ret_stdcall(result(nc_listen(rt_arg(0), (int)rt_arg(1))), 2); } /* listen */
RT_IMPORT(WSOCK32_ord16)                                                                     /* recv */
{
    rt_ret_stdcall(result(nc_recv(rt_arg(0), g_mem + rt_arg(1), rt_arg(2))), 4);
}
RT_IMPORT(WSOCK32_ord17)                                                                     /* recvfrom */
{
    uint32_t addr = 0;
    uint16_t port = 0;
    long r = nc_recvfrom(rt_arg(0), g_mem + rt_arg(1), rt_arg(2), &addr, &port);
    if (r >= 0) put_addr(rt_arg(4), rt_arg(5), addr, port);
    rt_ret_stdcall(result(r), 6);
}
RT_IMPORT(WSOCK32_ord19)                                                                     /* send */
{
    rt_ret_stdcall(result(nc_send(rt_arg(0), g_mem + rt_arg(1), rt_arg(2))), 4);
}
RT_IMPORT(WSOCK32_ord20)                                                                     /* sendto */
{
    uint32_t addr;
    uint16_t port;
    long r = get_addr(rt_arg(4), rt_arg(5), &addr, &port) ?
        nc_sendto(rt_arg(0), g_mem + rt_arg(1), rt_arg(2), addr, port) : -1;
    rt_ret_stdcall(result(r), 6);
}
RT_IMPORT(WSOCK32_ord21)                                                                     /* setsockopt */
{
    uint32_t level = rt_arg(1), name = rt_arg(2), val = rt_arg(3);
    int32_t len = (int32_t)rt_arg(4);
    long r;
    if (!val || len < 4) r = (nc_set_error(HN_FAULT), -1);
    else if (level == NC_SOL_SOCKET && name == NC_SO_LINGER)      /* struct linger: u16 on, u16 seconds */
        r = nc_setopt(rt_arg(0), (int)level, (int)name, (int)rd16(g_mem, val), (int)rd16(g_mem, val + 2));
    else
        r = nc_setopt(rt_arg(0), (int)level, (int)name, (int)rd32(g_mem, val), 0);
    rt_ret_stdcall(result(r), 5);
}
RT_IMPORT(WSOCK32_ord12)                                                                     /* ioctlsocket */
{
    uint32_t cmd = rt_arg(1), arg = rt_arg(2);
    long r;
    if (!arg) r = (nc_set_error(HN_FAULT), -1);
    else if (cmd == FIONBIO) r = nc_nonblocking(rt_arg(0), rd32(g_mem, arg) != 0);
    else if (cmd == FIONREAD) {
        uint32_t n = 0;
        r = nc_readable(rt_arg(0), &n);
        if (r == 0) wr32(g_mem, arg, n);
    } else r = (nc_set_error(HN_INVAL), -1);
    rt_ret_stdcall(result(r), 3);
}
RT_IMPORT(WSOCK32_ord9)                                                                      /* htons */
{
    uint32_t v = rt_arg(0) & 0xffffu;
    rt_ret_stdcall(((v & 0xff) << 8) | (v >> 8), 1);
}

/* TcpIp::scan_adapter_addresses 0x8aee20, stdcall (table, short *count), ret 8 (0x8af28b); ECX holds the
 * TcpIp object and is not used. The original runs ipconfig (or winipcfg, ip.exe) into ~net.out through
 * system 0x9473ea, may switch a full-screen game to an 800x600 window, and reads the text back by line
 * count (section 5 of the research note). This gives the same table from the system's own list: for each
 * IPv4 adapter that is up, not loopback and not 0, 32 bytes with a sockaddr_in of the address at +0
 * (0x8af114..0x8af130) and one of the subnet broadcast at +16 (0x8af22c..0x8af267), family 2, port 0. At
 * most 20 entries, the size of the table in TcpIp (+8) and on the callers' stacks (0x77a980, 0x8ae370). */
RT_REPLACE(008aee20, scan_adapter_addresses)
{
    uint32_t table = rt_arg(0), count = rt_arg(1), addr[20], bcast[20];
    int n = nc_adapters(addr, bcast, 20);
    for (int i = 0; i < n; i++) {
        uint32_t e = table + 32u * (uint32_t)i;
        memset(g_mem + e, 0, 32);
        wr16(g_mem, e, 2);
        memcpy(g_mem + e + 4, &addr[i], 4);
        wr16(g_mem, e + 16, 2);
        memcpy(g_mem + e + 20, &bcast[i], 4);
    }
    wr16(g_mem, count, (uint32_t)n);
    rt_ret_stdcall(0, 2);
}

/* Printing: the print function 0x84c830 is replaced by the PDF writer (print_pdf.c), so PrintDlgA and
 * the GDI32 calls are not reached; they fail. EnumPrintersA answers that one printer exists (0x84c7f0
 * returns "returned != 0"), so the default of Print To is Printer, which the game shows as PDF. */
RT_IMPORT(EnumPrintersA)
{
    if (rt_arg(6)) wr32(g_mem, rt_arg(6), 1);       /* returned: the number of printers */
    rt_ret_stdcall(1, 7);
}
RT_IMPORT(PrintDlgA) { rt_ret_stdcall(0, 1); }
RT_IMPORT(GetDeviceCaps) { rt_ret_stdcall(0, 2); }
RT_IMPORT(CreateFontA) { rt_ret_stdcall(0, 14); }
RT_IMPORT(GetTextMetricsA) { rt_ret_stdcall(0, 2); }
RT_IMPORT(StartDocA) { rt_ret_stdcall(0xffffffffu, 2); }
RT_IMPORT(StartPage) { rt_ret_stdcall(0xffffffffu, 1); }
RT_IMPORT(EndPage) { rt_ret_stdcall(0xffffffffu, 1); }
RT_IMPORT(EndDoc) { rt_ret_stdcall(0xffffffffu, 1); }
RT_IMPORT(SelectObject) { rt_ret_stdcall(0, 2); }
RT_IMPORT(TextOutA) { rt_ret_stdcall(0, 5); }
RT_IMPORT(DeleteObject) { rt_ret_stdcall(0, 1); }
