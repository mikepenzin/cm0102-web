/* KERNEL32: version, process start (command line, environment, module), the heap, time, code pages,
 * libraries and the few process calls. Files are in files.c, exceptions in seh.c.
 * The values follow docs/research/2026-10-14-import-inventory.md section 1.1: the runtime is Windows
 * NT 5.1, so the CRT heap uses only HeapAlloc, HeapFree, HeapReAlloc and HeapSize. */
#include "win.h"
#include "host.h"

#include <SDL.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

uint32_t win_last_error;

/* Version: NT 5.1, build 2600 (Windows XP). */
RT_IMPORT(GetVersion) { rt_ret_stdcall(0x0A280105u, 0); }

RT_IMPORT(GetVersionExA)
{
    uint32_t info = rt_arg(0);
    wr32(g_mem, info + 4, 5);
    wr32(g_mem, info + 8, 1);
    wr32(g_mem, info + 12, 2600);
    wr32(g_mem, info + 16, 2);                 /* VER_PLATFORM_WIN32_NT */
    memset(g_mem + info + 20, 0, 128);
    rt_ret_stdcall(1, 1);
}

/* The CRT heap: one heap, inside the flat memory (heap.c). */
#define HEAP_HANDLE 0x00150000u
#define HEAP_ZERO_MEMORY 8u
#define HEAP_REALLOC_IN_PLACE_ONLY 0x10u

RT_IMPORT(HeapCreate) { rt_ret_stdcall(HEAP_HANDLE, 3); }
RT_IMPORT(HeapDestroy) { rt_ret_stdcall(1, 1); }

RT_IMPORT(HeapAlloc)
{
    uint32_t flags = rt_arg(1), size = rt_arg(2);
    rt_ret_stdcall(flags & HEAP_ZERO_MEMORY ? rt_alloc_zero(size) : rt_alloc(size), 3);
}

RT_IMPORT(HeapFree)
{
    rt_free(rt_arg(2));
    rt_ret_stdcall(1, 3);
}

RT_IMPORT(HeapReAlloc)
{
    uint32_t flags = rt_arg(1), block = rt_arg(2), size = rt_arg(3);
    uint32_t old = block ? rt_block_size(block) : 0;
    uint32_t n = rt_realloc(block, size, (flags & HEAP_REALLOC_IN_PLACE_ONLY) != 0);
    if (n && (flags & HEAP_ZERO_MEMORY) && size > old) memset(g_mem + n + old, 0, size - old);
    rt_ret_stdcall(n, 4);
}

RT_IMPORT(HeapSize) { rt_ret_stdcall(rt_block_size(rt_arg(2)), 3); }

/* GlobalAlloc with GMEM_FIXED: a heap block. GMEM_ZEROINIT is 0x40. */
RT_IMPORT(GlobalAlloc)
{
    uint32_t flags = rt_arg(0), size = rt_arg(1);
    rt_ret_stdcall(flags & 0x40 ? rt_alloc_zero(size) : rt_alloc(size), 2);
}

RT_IMPORT(GlobalFree)
{
    rt_free(rt_arg(0));
    rt_ret_stdcall(0, 1);
}

/* Memory that the runtime owns and never frees (command line, environment, COM objects, surfaces). */
#define STATIC_BASE 0x60000000u
#define STATIC_SIZE 0x02000000u
static uint32_t static_top = STATIC_BASE;

uint32_t win_static_alloc(uint32_t size)
{
    uint32_t a = static_top;
    static_top += (size + 15) & ~15u;
    if (static_top > STATIC_BASE + STATIC_SIZE) rt_fatal("runtime memory full");
    memset(g_mem + a, 0, size);
    return a;
}

/* Process start. */
/* The game starts a program in one place only: Restart Game. restart_game (0x68dbc0) runs its clean-up (0x5cc7d0),
 * closes its single-instance handle ([0xb67a28], CloseHandle) and calls WinExec with its own command line
 * (GetCommandLineA, SW_SHOWDEFAULT), then exit (0x945934). So WinExec starts this program again with the arguments of
 * this start (host_restart; in the browser the page loads again) and returns only when it could not: 2 (the file was
 * not found), after which the game ends as the original does. */
RT_IMPORT(WinExec)
{
    fprintf(stderr, "recomp: Restart Game: the program starts again\n");
    fflush(stderr);
    host_restart(win_argv);
    fprintf(stderr, "recomp: Restart Game: the program could not start again, so it ends\n");
    rt_ret_stdcall(2, 2);
}

RT_IMPORT(GetCommandLineA)
{
    static uint32_t line;
    if (!line) {
        uint32_t room = (uint32_t)strlen(win_command_line) + 1;
        line = win_static_alloc(room);
        win_put_str(line, win_command_line, room);
    }
    rt_ret_stdcall(line, 0);
}

/* The environment: empty, so every CM3_* folder takes its default (inventory 4.6). */
RT_IMPORT(GetEnvironmentStrings)
{
    static uint32_t block;
    if (!block) block = win_static_alloc(4);
    rt_ret_stdcall(block, 0);
}
RT_IMPORT(GetEnvironmentStringsW) { rt_ret_stdcall(0, 0); }
RT_IMPORT(FreeEnvironmentStringsA) { rt_ret_stdcall(1, 1); }
RT_IMPORT(FreeEnvironmentStringsW) { rt_ret_stdcall(1, 1); }
RT_IMPORT(GetEnvironmentVariableA) { win_last_error = 203; rt_ret_stdcall(0, 3); }   /* ERROR_ENVVAR_NOT_FOUND */
RT_IMPORT(SetEnvironmentVariableA) { rt_ret_stdcall(1, 2); }

RT_IMPORT(GetStartupInfoA)
{
    uint32_t info = rt_arg(0);
    memset(g_mem + info, 0, 0x44);
    wr32(g_mem, info, 0x44);
    rt_ret_stdcall(0, 1);
}

#define STD_HANDLE_BASE 0xF0000000u
RT_IMPORT(GetStdHandle)
{
    uint32_t which = rt_arg(0);                  /* -10, -11, -12 */
    rt_ret_stdcall(STD_HANDLE_BASE + (uint32_t)(-(int32_t)which), 1);
}
RT_IMPORT(SetStdHandle) { rt_ret_stdcall(1, 2); }
RT_IMPORT(SetHandleCount) { rt_ret_stdcall(rt_arg(0), 1); }

RT_IMPORT(GetModuleFileNameA)
{
    uint32_t buf = rt_arg(1), room = rt_arg(2);
    char path[1100];
    win_windows_path(win_exe_path, path, sizeof path);
    win_put_str(buf, path, room);
    rt_ret_stdcall((uint32_t)strlen(win_str(buf)), 3);
}

#define KERNEL32_HANDLE 0x7c800000u
static int is_kernel32(const char *name)
{
    return !strcasecmp(name, "kernel32") || !strcasecmp(name, "kernel32.dll");
}

RT_IMPORT(GetModuleHandleA)
{
    uint32_t name = rt_arg(0);
    if (!name) { rt_ret_stdcall(rt_image_base(), 1); return; }
    if (is_kernel32(win_str(name))) { rt_ret_stdcall(KERNEL32_HANDLE, 1); return; }
    win_last_error = 126;                        /* ERROR_MOD_NOT_FOUND */
    rt_ret_stdcall(0, 1);
}

/* Libraries: only KERNEL32 loads. cpuinf32.dll is not shipped and the CRT's user32.dll message box is
 * not needed. */
RT_IMPORT(LoadLibraryA)
{
    const char *name = win_str(rt_arg(0));
    WIN_TRACE("LoadLibraryA(%s)\n", name);
    if (is_kernel32(name)) { rt_ret_stdcall(KERNEL32_HANDLE, 1); return; }
    win_last_error = 126;
    rt_ret_stdcall(0, 1);
}
RT_IMPORT(FreeLibrary) { rt_ret_stdcall(1, 1); }

static void host_IsProcessorFeaturePresent(void) { rt_ret_stdcall(0, 1); }

/* GetDiskFreeSpaceExA(folder, free to caller, total, total free): 20 GB free of 40 GB. */
static void host_GetDiskFreeSpaceExA(void)
{
    uint64_t free_bytes = 20ull << 30, total = 40ull << 30;
    if (rt_arg(1)) wr64(g_mem, rt_arg(1), free_bytes);
    if (rt_arg(2)) wr64(g_mem, rt_arg(2), total);
    if (rt_arg(3)) wr64(g_mem, rt_arg(3), free_bytes);
    rt_ret_stdcall(1, 4);
}

RT_IMPORT(GetProcAddress)
{
    uint32_t module = rt_arg(0), name = rt_arg(1);
    uint32_t result = 0;
    if (module == KERNEL32_HANDLE && name >= 0x10000) {
        const char *n = win_str(name);
        if (!strcmp(n, "IsProcessorFeaturePresent")) result = rt_host_fn(n, host_IsProcessorFeaturePresent);
        else if (!strcmp(n, "GetDiskFreeSpaceExA")) result = rt_host_fn(n, host_GetDiskFreeSpaceExA);
        WIN_TRACE("GetProcAddress(%s) = %#x\n", n, result);
    }
    if (!result) win_last_error = 127;           /* ERROR_PROC_NOT_FOUND */
    rt_ret_stdcall(result, 2);
}

/* Code pages: 1252, single byte. */
RT_IMPORT(GetACP) { rt_ret_stdcall(1252, 0); }
RT_IMPORT(GetOEMCP) { rt_ret_stdcall(850, 0); }
RT_IMPORT(GetCPInfo)
{
    uint32_t info = rt_arg(1);
    memset(g_mem + info, 0, 20);
    wr32(g_mem, info, 1);                        /* MaxCharSize */
    wr8(g_mem, info + 4, '?');                   /* DefaultChar */
    rt_ret_stdcall(1, 2);
}

/* The machine. */
RT_IMPORT(GetSystemInfo)
{
    uint32_t p = rt_arg(0);
    memset(g_mem + p, 0, 36);
    wr32(g_mem, p + 4, 4096);
    wr32(g_mem, p + 8, 0x10000);
    wr32(g_mem, p + 0xc, 0x7ffeffff);
    wr32(g_mem, p + 0x10, 1);
    wr32(g_mem, p + 0x14, 1);
    wr32(g_mem, p + 0x18, 586);                  /* PROCESSOR_INTEL_PENTIUM */
    wr32(g_mem, p + 0x1c, 0x10000);
    wr16(g_mem, p + 0x20, 6);
    rt_ret_stdcall(0, 1);
}

/* Memory: below 2 GB in each field, so that the signed shifts of 0x7a16d0 and 0x7a1710 stay positive. */
RT_IMPORT(GlobalMemoryStatus)
{
    uint32_t p = rt_arg(0);
    wr32(g_mem, p, 32);
    wr32(g_mem, p + 4, 20);
    wr32(g_mem, p + 8, 0x40000000u);             /* total physical: 1 GB */
    wr32(g_mem, p + 0xc, 0x38000000u);           /* free physical */
    wr32(g_mem, p + 0x10, 0x60000000u);          /* total page file */
    wr32(g_mem, p + 0x14, 0x30000000u);          /* free page file */
    wr32(g_mem, p + 0x18, 0x7ffe0000u);
    wr32(g_mem, p + 0x1c, 0x70000000u);
    rt_ret_stdcall(0, 1);
}

RT_IMPORT(IsBadReadPtr) { rt_ret_stdcall(0, 2); }
RT_IMPORT(IsBadWritePtr) { rt_ret_stdcall(0, 2); }
RT_IMPORT(IsBadCodePtr) { rt_ret_stdcall(0, 1); }

/* The single-instance mutex of WinMain: always new. */
RT_IMPORT(CreateMutexA)
{
    win_last_error = 0;
    rt_ret_stdcall(0x00001000u, 3);
}

RT_IMPORT(GetLastError) { rt_ret_stdcall(win_last_error, 0); }

RT_IMPORT(ExitProcess)
{
    uint32_t code = rt_arg(0);
    WIN_TRACE("ExitProcess(%u)\n", code);
    win_save_window();
    if (rt_on_fatal) rt_on_fatal();
    SDL_Quit();
    exit((int)code);
}
RT_IMPORT(GetCurrentProcess) { rt_ret_stdcall(0xffffffffu, 0); }
RT_IMPORT(TerminateProcess)
{
    uint32_t code = rt_arg(1);
    SDL_Quit();
    exit((int)code);
}

/* Time. FILETIME counts 100 ns from 1601-01-01. The Unix epoch is 11644473600 s later.
 *
 * Test runs only: RECOMP_FIXED_CLOCK=1 makes the clocks repeatable, so that two runs with the same
 * -seed and the same script give the same saved game. The original reads the date and time of day
 * (GetLocalTime and GetSystemTime, through time()) to seed rand() at 0x696d51 and for timers. With
 * the variable, these give 2001-07-14 12:00:00 UTC in every call, with a time zone offset of 0. The
 * millisecond clock (GetSystemTimeAsFileTime, 0x947299) must still move, because the holiday hold
 * (0x76224c) waits for it. It starts at the same date and counts the real time since the program
 * started. Without the variable, every clock is the clock of the host. */
#define FIXED_CLOCK_UNIX 995112000ull      /* 2001-07-14 12:00:00 UTC */
static int fixed_clock = -1;
static uint64_t fixed_clock_start_us;

int win_fixed_clock(void)
{
    if (fixed_clock < 0) {
        const char *v = getenv("RECOMP_FIXED_CLOCK");
        fixed_clock = v && *v && strcmp(v, "0") != 0;
        fixed_clock_start_us = host_unix_time_us();
    }
    return fixed_clock;
}

static uint64_t clock_us(void)
{
    if (win_fixed_clock()) return FIXED_CLOCK_UNIX * 1000000ull + (host_unix_time_us() - fixed_clock_start_us);
    return host_unix_time_us();
}

uint64_t win_filetime_now(void)
{
    return (clock_us() + 11644473600000000ull) * 10u;
}

RT_IMPORT(GetSystemTimeAsFileTime)
{
    wr64(g_mem, rt_arg(0), win_filetime_now());
    rt_ret_stdcall(0, 1);
}

void win_put_systemtime(uint32_t p, const struct tm *t, uint32_t ms)
{
    wr16(g_mem, p, (uint32_t)t->tm_year + 1900);
    wr16(g_mem, p + 2, (uint32_t)t->tm_mon + 1);
    wr16(g_mem, p + 4, (uint32_t)t->tm_wday);
    wr16(g_mem, p + 6, (uint32_t)t->tm_mday);
    wr16(g_mem, p + 8, (uint32_t)t->tm_hour);
    wr16(g_mem, p + 10, (uint32_t)t->tm_min);
    wr16(g_mem, p + 12, (uint32_t)t->tm_sec);
    wr16(g_mem, p + 14, ms);
}

static void system_time(int local)
{
    uint64_t us = win_fixed_clock() ? FIXED_CLOCK_UNIX * 1000000ull : host_unix_time_us();
    time_t s = (time_t)(us / 1000000u);
    struct tm t;
    if (local && !win_fixed_clock()) host_localtime(s, &t); else host_gmtime(s, &t);
    win_put_systemtime(rt_arg(0), &t, (uint32_t)(us % 1000000u / 1000u));
}

RT_IMPORT(GetLocalTime) { system_time(1); rt_ret_stdcall(0, 1); }
RT_IMPORT(GetSystemTime) { system_time(0); rt_ret_stdcall(0, 1); }

/* The host's offset from UTC now, as one bias without daylight rules (TIME_ZONE_ID_UNKNOWN). */
int32_t win_bias_minutes(void) { return win_fixed_clock() ? 0 : host_bias_minutes(); }

RT_IMPORT(GetTimeZoneInformation)
{
    uint32_t p = rt_arg(0);
    memset(g_mem + p, 0, 172);
    wr32(g_mem, p, (uint32_t)win_bias_minutes());
    rt_ret_stdcall(0, 1);
}
