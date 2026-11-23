/* The replacements of the database load test: functions of the original (the CRT heap and files, and
 * the few game functions that touch the screen, the disk or the CD) and three imports.
 * tests/recomp/reference.py replaces the same functions in the Unicorn run with the same behaviour; keep
 * the two lists the same. The whole game (src/recomp_rt/win) recompiles these functions instead.
 *
 * The heap is replaced, not recompiled: the CRT heap of MSVC 6 (the small-block heap over HeapAlloc and
 * VirtualAlloc) would need its Win32 calls emulated for no gain, and a bump heap that never reuses a
 * block gives the same addresses in both runs, so the tables can be compared byte for byte. */
#include "rt.h"
#include "rt_host.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *rt_data_dir = ".";

/* The staff reserve of the load (staff_reserve.c): the original's 23,000 rows, as the Unicorn run of the
 * original has them. Without it the load program does not link (since b6926660, 17 October 2026). */
uint32_t rt_staff_reserve_size(void) { return 0x59d8u; }

#define MAX_COUNTS 64
static struct { const char *name; uint32_t n; } counts[MAX_COUNTS];

void rt_count(const char *name)
{
    for (int i = 0; i < MAX_COUNTS; i++) {
        if (!counts[i].name) counts[i].name = name;
        if (counts[i].name == name || !strcmp(counts[i].name, name)) { counts[i].n++; return; }
    }
}

void rt_print_counts(void)
{
    for (int i = 0; i < MAX_COUNTS && counts[i].name; i++) fprintf(stderr, " %s=%u", counts[i].name, counts[i].n);
    fprintf(stderr, "\n");
}

static const char *cstr(uint32_t a) { return (const char *)(g_mem + a); }

/* Heap */
RT_REPLACE(00944e46, malloc) { rt_count("malloc"); rt_ret_cdecl(rt_alloc(rt_arg(0))); }
RT_REPLACE(00944cf1, operator_new) { rt_count("operator_new"); rt_ret_cdecl(rt_alloc(rt_arg(0))); }
RT_REPLACE(00945501, calloc) { rt_count("calloc"); rt_ret_cdecl(rt_alloc(rt_arg(0) * rt_arg(1))); }
RT_REPLACE(009452ca, free) { rt_count("free"); rt_ret_cdecl(0); }
RT_REPLACE(00944c94, operator_delete) { rt_count("operator_delete"); rt_ret_cdecl(0); }

RT_REPLACE(00945667, realloc)
{
    rt_count("realloc");
    uint32_t block = rt_arg(0), size = rt_arg(1);
    if (block && size <= rt_block_size(block)) {
        rt_set_block_size(block, size);
        rt_ret_cdecl(block);
        return;
    }
    uint32_t old = block ? rt_block_size(block) : 0;
    uint32_t n = rt_alloc(size);
    if (block) memcpy(g_mem + n, g_mem + block, old < size ? old : size);
    rt_ret_cdecl(n);
}

/* Files. A FILE of MSVC is 0x20 bytes; the code reads its flags at +0xc (0x10 = end of file). */
#define MAX_FILES 64
static struct { uint32_t stream; FILE *f; } files[MAX_FILES];

static FILE *host_file(uint32_t stream)
{
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i].stream == stream && files[i].f) return files[i].f;
    rt_fatal("unknown FILE %#x", stream);
}

static void set_eof(uint32_t stream, int on)
{
    uint32_t flags = rd32(g_mem, stream + 0xc);
    wr32(g_mem, stream + 0xc, on ? flags | 0x10 : flags & ~0x10u);
}

RT_REPLACE(00945b00, fopen)
{
    rt_count("fopen");
    const char *name = cstr(rt_arg(0)), *mode = cstr(rt_arg(1));
    if (strpbrk(mode, "wa+")) { rt_ret_cdecl(0); return; }
    FILE *f = fopen(name, "rb");
    if (!f) { rt_ret_cdecl(0); return; }
    uint32_t stream = rt_alloc(0x20);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].f) { files[i].stream = stream; files[i].f = f; rt_ret_cdecl(stream); return; }
    }
    rt_fatal("too many open files");
}

RT_REPLACE(00945bfe, fread)
{
    rt_count("fread");
    uint32_t buf = rt_arg(0), size = rt_arg(1), count = rt_arg(2), stream = rt_arg(3);
    uint32_t want = size * count;
    size_t got = fread(g_mem + buf, 1, want, host_file(stream));
    if (got < want) set_eof(stream, 1);
    rt_ret_cdecl(size ? (uint32_t)got / size : 0);
}

RT_REPLACE(00945a09, fclose)
{
    rt_count("fclose");
    uint32_t stream = rt_arg(0);
    FILE *f = host_file(stream);
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i].stream == stream) { files[i].stream = 0; files[i].f = NULL; }
    fclose(f);
    rt_ret_cdecl(0);
}

RT_REPLACE(00946a22, fseek)
{
    rt_count("fseek");
    uint32_t stream = rt_arg(0);
    int32_t offset = (int32_t)rt_arg(1);
    int origin = (int)rt_arg(2);
    set_eof(stream, 0);
    rt_ret_cdecl(fseek(host_file(stream), offset, origin) ? 0xffffffffu : 0);
}

RT_REPLACE(00946aae, ftell) { rt_count("ftell"); rt_ret_cdecl((uint32_t)ftell(host_file(rt_arg(0)))); }

RT_REPLACE(009469e0, fgetc)
{
    rt_count("fgetc");
    uint32_t stream = rt_arg(0);
    int c = fgetc(host_file(stream));
    if (c == EOF) set_eof(stream, 1);
    rt_ret_cdecl(c == EOF ? 0xffffffffu : (uint32_t)c);
}

RT_REPLACE(009478d8, access) { rt_count("access"); rt_ret_cdecl(access(cstr(rt_arg(0)), F_OK) ? 0xffffffffu : 0); }

/* The search for *.lng files to copy finds nothing. */
RT_REPLACE(004c0aa0, findfirst) { rt_count("findfirst"); rt_ret_cdecl(1); }
RT_REPLACE(004c0b50, findnext) { rt_count("findnext"); rt_ret_cdecl(1); }
RT_REPLACE(004c0c10, findclose) { rt_count("findclose"); rt_ret_cdecl(0); }

/* FUN_0090bde0(folder key, name): the path of a data file, found without regard to case. */
RT_REPLACE(0090bde0, data_path)
{
    rt_count("data_path");
    char wanted[260];
    snprintf(wanted, sizeof wanted, "%s", cstr(rt_arg(1)));
    for (char *p = wanted; *p; p++) *p = (char)tolower((unsigned char)*p);
    char best[260] = "";
    DIR *d = opendir(rt_data_dir);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (strcasecmp(e->d_name, wanted) == 0 && (!best[0] || strcmp(e->d_name, best) < 0))
            snprintf(best, sizeof best, "%s", e->d_name);
    }
    if (d) closedir(d);
    char path[600];
    if (best[0]) snprintf(path, sizeof path, "%s/%s", rt_data_dir, best);
    else snprintf(path, sizeof path, "%s", wanted);
    rt_ret_cdecl(rt_alloc_str(path));
}

/* The error box and the version text for its message: no box. */
RT_REPLACE(0090d130, error_place) { rt_count("error_place"); rt_ret_cdecl(0xad4394); }
RT_REPLACE(005e8290, error_box) { rt_count("error_box"); rt_ret_cdecl(0); }

/* The progress box, thiscall with 4 arguments: draws nothing. */
RT_REPLACE(00802f80, progress) { rt_count("progress"); rt_ret_stdcall(0, 4); }

/* Free memory and free disk in MB: plenty. */
RT_REPLACE(007a1710, free_memory_mb) { rt_count("free_memory_mb"); rt_ret_cdecl(1000); }
RT_REPLACE(007a1820, free_disk_mb) { rt_count("free_disk_mb"); rt_ret_cdecl(1000); }

RT_IMPORT(GlobalAlloc) { rt_count("GlobalAlloc"); rt_ret_stdcall(rt_alloc(rt_arg(1)), 2); }
RT_IMPORT(GlobalFree) { rt_count("GlobalFree"); rt_ret_stdcall(0, 1); }
RT_IMPORT(DeleteFileA) { rt_count("DeleteFileA"); rt_ret_stdcall(1, 1); }   /* deletes nothing */
