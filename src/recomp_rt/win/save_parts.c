/* The parts of a save merged across seasons (0.6.2 slice 13, fault 29, Q26).
 *
 * A save is a container of at most 999 parts: `zopen` 0x9326e0 refuses a new part when the count is 999 (cmp edi,
 * 0x3e7 at 0x932a3e). The save writes each data file of the game as a part, and `zadd_matching_files` 0x933610
 * (called at 0x82f538 and 0x82f56d with "*.tmp") adds every temporary file of the game's temp folder as a part of its
 * own. The game keeps a temporary file for each season: fixtures_<year>.tmp, transfers_<year>.tmp, and a file for
 * each named edition of a competition ("2023 CONCACAF Gold Cup_437.tmp"). None is deleted, so a game gains about 3.8
 * parts a season (193 parts at the end of 2021/22) and the save fails at about 2232 with "save_game_data(): unable to
 * pack the temporary files" (0x82fea5).
 *
 * Here the temporary files of one type go into one part. The type of a file is its name with each run of digits as
 * '#' ("fixtures_#.tmp"); a type with two files or more is one part, named after the type with ".parts"
 * ("fixtures_#.tmp.parts"). The original's load leaves such a part alone: its name test 0x933a00 compares the pattern
 * after "*." with the text after the last dot of the name (0x933aa2..0x933ab9), here "parts". A type with one file
 * stays a part of its own, as in the original. The count of types stops growing when the season tables end, so the
 * part count stops growing.
 *
 * A merged part holds "BEPARTS1", the count of files (u32), and for each file in name order the length of its name
 * (u32), the name, its size (u32) and its bytes. The load (`zextract_matching_files` 0x933320, at 0x82acbc) writes
 * these files back into the temp folder, so the game finds every file it wrote, and the disk check of the load
 * (`zget_matching_size` 0x933860, at 0x82ac50) counts their sizes. A merged part holds less than 4 GB (the row keeps
 * a u32 size): the files of a type past that stay parts of their own. A save with no merged part (0.6.1 and before)
 * loads as before. A build before slice 13 does not know a merged part: such a save stops its load at the box
 * "Cup..cpp 5246", because the files of the competitions are missing.
 */
#include "win.h"
#include "../rt.h"
#include "../host.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ZOPEN 0x009326e0u          /* cdecl (archive, name, write, header_arg, header_type): the part, or 0 */
#define ZWRITE 0x00932e10u         /* cdecl (data, size, count, part) */
#define ZCLOSE 0x00932b00u         /* cdecl (&part) */
#define PART_BUFFER 0x41705u       /* the buffer of an open part, which 0x933610 fills (0x9336de) */
#define PART_CHUNK 0x30d40u        /* and its size, 200,000 bytes (0x9336d9) */

#define HEADER_ROWS 1000u          /* the container: version, flags, count, then 1000 rows (savfile.py) */
#define ROW_SIZE 0x10cu
#define ROW_NAME 8u
#define NAME_SIZE 260u

#define MAGIC "BEPARTS1"
#define ENDING ".parts"
#define MAX_FILES 100000
#define MERGED_MAX 0xf0000000ull   /* the bytes of one merged part at most, below the u32 size of its row */

static int parts_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SAVE_PARTS");      /* test switch: "keep" writes the original's parts */
        on = !(v && !strcmp(v, "keep"));
    }
    return on;
}

static int ends_with(const char *s, const char *end)
{
    size_t a = strlen(s), b = strlen(end);
    return a >= b && !strcasecmp(s + a - b, end);
}

/* The type of a file: its name with each run of digits as '#'. 0 when it does not fit. */
static int type_of(const char *name, char *out, size_t room)
{
    size_t o = 0;
    for (const char *p = name; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            if (p > name && p[-1] >= '0' && p[-1] <= '9') continue;
            if (o + 1 >= room) return 0;
            out[o++] = '#';
        } else {
            if (o + 1 >= room) return 0;
            out[o++] = *p;
        }
    }
    out[o] = 0;
    return 1;
}

typedef struct {
    char *name, *host, *type;
    uint64_t size;
    int merged;
} File;

typedef struct {
    File *files;
    int count;
} List;

static void list_add(const char *name, const char *host, void *ctx)
{
    List *l = ctx;
    char type[NAME_SIZE];
    if (l->count >= MAX_FILES) return;
    File *f = realloc(l->files, sizeof(File) * (size_t)(l->count + 1));
    if (!f) return;
    l->files = f;
    int typed = type_of(name, type, sizeof type - strlen(ENDING));
    HostStat st;
    uint64_t size = host_stat(host, &st) == 0 ? st.size : 0;
    l->files[l->count++] = (File){strdup(name), strdup(host), typed ? strdup(type) : NULL, size, 0};
}

static void list_free(List *l)
{
    for (int i = 0; i < l->count; i++) {
        free(l->files[i].name);
        free(l->files[i].host);
        free(l->files[i].type);
    }
    free(l->files);
    memset(l, 0, sizeof *l);
}

static int by_type_name(const void *a, const void *b)
{
    const File *x = a, *y = b;
    int t = strcmp(x->type ? x->type : "", y->type ? y->type : "");
    return t ? t : strcmp(x->name, y->name);
}

/* ---- The save ---- */

static List hidden;                /* the files of merged parts while the original's pack runs */

static int is_hidden(const char *name)
{
    for (int i = 0; i < hidden.count; i++)
        if (hidden.files[i].merged && !strcasecmp(hidden.files[i].name, name)) return 1;
    return 0;
}

typedef struct {
    uint32_t part, fill;           /* the open part (game memory) and the bytes in its buffer */
    int bad;
} Writer;

static void flush(Writer *w)
{
    if (!w->fill || w->bad) return;
    uint32_t args[4] = {w->part + PART_BUFFER, 1, w->fill, w->part};
    if (rt_callback(ZWRITE, 4, args) != w->fill || rt_unwinding) w->bad = 1;
    w->fill = 0;
}

static void put(Writer *w, const void *data, uint32_t n)
{
    const uint8_t *p = data;
    while (n && !w->bad) {
        uint32_t take = PART_CHUNK - w->fill < n ? PART_CHUNK - w->fill : n;
        memcpy(g_mem + w->part + PART_BUFFER + w->fill, p, take);
        w->fill += take, p += take, n -= take;
        if (w->fill == PART_CHUNK) flush(w);
    }
}

static void put32(Writer *w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    put(w, b, 4);
}

/* Writes the files first..first+n-1 (one type) as one part. 0 on success. */
static int write_merged(uint32_t archive, uint32_t arg3, uint32_t arg4, const File *files, int n)
{
    static uint32_t name_at, slot_at;
    static uint8_t chunk[PART_CHUNK];
    if (!name_at) {
        name_at = win_static_alloc(NAME_SIZE);
        slot_at = win_static_alloc(4);
    }
    snprintf((char *)g_mem + name_at, NAME_SIZE, "%s" ENDING, files[0].type);
    uint32_t open_args[5] = {archive, name_at, 1, arg3, arg4};
    Writer w = {rt_callback(ZOPEN, 5, open_args), 0, 0};
    if (rt_unwinding || !w.part) return 1;
    uint64_t total = 12;
    put(&w, MAGIC, 8);
    put32(&w, (uint32_t)n);
    for (int i = 0; i < n && !w.bad; i++) {
        int fd = host_open(files[i].host, O_RDONLY, 0);
        int64_t size = fd < 0 ? -1 : host_seek(fd, 0, SEEK_END);
        total += 8 + strlen(files[i].name) + (uint64_t)(size < 0 ? 0 : size);
        if (size < 0 || total > MERGED_MAX || host_seek(fd, 0, SEEK_SET) != 0) {
            if (fd >= 0) host_close(fd);
            w.bad = 1;
            break;
        }
        uint32_t len = (uint32_t)strlen(files[i].name);
        put32(&w, len);
        put(&w, files[i].name, len);
        put32(&w, (uint32_t)size);
        for (int64_t left = size; left > 0 && !w.bad;) {
            uint32_t want = left < (int64_t)sizeof chunk ? (uint32_t)left : (uint32_t)sizeof chunk;
            if (host_read(fd, chunk, want) != (long)want) w.bad = 1;
            else put(&w, chunk, want);
            left -= want;
        }
        host_close(fd);
    }
    flush(&w);
    if (rt_unwinding) return 1;
    wr32(g_mem, slot_at, w.part);
    rt_callback(ZCLOSE, 1, &slot_at);
    return w.bad || rt_unwinding;
}

void o_00933610(void);

/* zadd_matching_files, cdecl (archive, folder, pattern, header_arg, header_type): 1 when every file is a part. */
RT_WRAP(00933610, add_matching_files)
{
    uint32_t archive = rt_arg(0), folder = rt_arg(1), pattern = rt_arg(2), arg3 = rt_arg(3), arg4 = rt_arg(4);
    if (!parts_on() || strcasecmp(win_str(pattern), "*.tmp")) {
        o_00933610();
        return;
    }
    List l = {0};
    char dir[1024];
    snprintf(dir, sizeof dir, "%.1023s", win_str(folder));
    win_list_files(dir, "*.tmp", list_add, &l);
    qsort(l.files, (size_t)l.count, sizeof(File), by_type_name);
    int merged = 0, single = 0;
    for (int i = 0; i < l.count;) {
        int j = i + 1;
        while (j < l.count && l.files[i].type && l.files[j].type && !strcmp(l.files[i].type, l.files[j].type)) j++;
        /* The files of the type in name order while the part stays below MERGED_MAX; the rest stay single. */
        int in = 0;
        uint64_t bytes = 12;
        for (int k = i; k < j && l.files[i].type; k++) {
            uint64_t more = 8 + strlen(l.files[k].name) + l.files[k].size;
            if (bytes + more > MERGED_MAX) break;
            bytes += more;
            in++;
        }
        if (in < 2) in = 0;
        for (int k = i; k < j; k++) l.files[k].merged = k - i < in;
        if (in) merged++;
        single += j - i - in;
        i = j;
    }
    if (!merged) {                 /* the original's parts: a game of one season, or the test switch */
        if (getenv("RECOMP_SAVE_PARTS_LOG"))
            fprintf(stderr, "recomp: save parts: %d temporary files in %s, none merged\n", l.count, dir);
        list_free(&l);
        o_00933610();
        return;
    }
    uint32_t ok = 1;
    if (single) {                  /* the original adds the other files; with none it would fail (0x93365a) */
        hidden = l;
        win_find_hide(is_hidden);
        o_00933610();
        win_find_hide(NULL);
        memset(&hidden, 0, sizeof hidden);
        if (rt_unwinding) {
            list_free(&l);
            return;
        }
        ok = cpu.eax;
    }
    for (int i = 0; i < l.count && ok;) {
        int j = i + 1;
        while (j < l.count && l.files[j].merged && l.files[i].merged && !strcmp(l.files[i].type, l.files[j].type)) j++;
        if (l.files[i].merged && write_merged(archive, arg3, arg4, l.files + i, j - i)) {
            fprintf(stderr, "cm0102_recomp: the save could not write the part %s" ENDING "\n", l.files[i].type);
            ok = 0;
        }
        if (rt_unwinding) break;
        i = j;
    }
    if (getenv("RECOMP_SAVE_PARTS_LOG"))       /* test switch: tests/seasons/save_parts_run.py */
        fprintf(stderr, "recomp: save parts: %d temporary files in %s, %d in %d merged parts, %d single\n", l.count,
                dir, l.count - single, merged, single);
    list_free(&l);
    if (!rt_unwinding) cpu.eax = ok;
}

/* ---- The load ---- */

/* A reader of one part of the container, from its offset. */
typedef struct {
    int fd, compressed;
    uint8_t buf[1 << 16];
    uint32_t have, at;             /* bytes in buf, read position */
    uint32_t repeat;               /* bytes of a run still to give */
    uint8_t value;
} Reader;

static int raw_byte(Reader *r, uint8_t *b)
{
    if (r->at == r->have) {
        long n = host_read(r->fd, r->buf, sizeof r->buf);
        if (n <= 0) return 0;
        r->have = (uint32_t)n, r->at = 0;
    }
    *b = r->buf[r->at++];
    return 1;
}

/* The decoder of zread 0x932c30 (savfile.py decode), one byte at a time. */
static int get_byte(Reader *r, uint8_t *b)
{
    if (!r->compressed) return raw_byte(r, b);
    if (r->repeat) {
        r->repeat--;
        *b = r->value;
        return 1;
    }
    uint8_t c;
    if (!raw_byte(r, &c)) return 0;
    if (c < 0x81) {
        *b = c;
        return 1;
    }
    if (!raw_byte(r, &r->value)) return 0;
    r->repeat = c - 0x80u - 1u;
    *b = r->value;
    return 1;
}

static int get(Reader *r, void *out, uint32_t n)
{
    uint8_t *p = out;
    for (uint32_t i = 0; i < n; i++)
        if (!get_byte(r, p + i)) return 0;
    return 1;
}

static int get32(Reader *r, uint32_t *v)
{
    uint8_t b[4];
    if (!get(r, b, 4)) return 0;
    *v = b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return 1;
}

/* * and ? without regard to case, as the finds (files.c). */
static int glob(const char *pat, const char *s)
{
    for (; *pat; pat++, s++) {
        if (*pat == '*') {
            for (const char *t = s;; t++) {
                if (glob(pat + 1, t)) return 1;
                if (!*t) return 0;
            }
        }
        if (!*s) return 0;
        if (*pat != '?' && tolower((unsigned char)*pat) != tolower((unsigned char)*s)) return 0;
    }
    return !*s;
}

typedef struct {
    uint32_t offset, size;
    char name[NAME_SIZE];
} Row;

/* The rows of merged parts whose type matches pattern, and in *singles the count of other rows that match it. -1 when
 * the file is no container. */
static int merged_rows(const char *host, const char *pattern, Row **rows, int *compressed, int *singles)
{
    *rows = NULL;
    *singles = 0;
    int fd = host_open(host, O_RDONLY, 0), n = 0;
    if (fd < 0) return -1;
    uint8_t head[12], row[ROW_SIZE];
    uint32_t version = 0, count = 0;
    if (host_read(fd, head, 12) == 12) {
        memcpy(&version, head, 4);
        memcpy(&count, head + 8, 4);
    }
    if (version < 1 || version > 4 || count > HEADER_ROWS) {
        host_close(fd);
        return -1;
    }
    *compressed = version == 2 || version == 4;
    for (uint32_t k = 0; k < count; k++) {
        if (host_read(fd, row, ROW_SIZE) != ROW_SIZE) {
            n = -1;
            break;
        }
        char name[NAME_SIZE];
        memcpy(name, row + ROW_NAME, NAME_SIZE - 1);
        name[NAME_SIZE - 1] = 0;
        if (!ends_with(name, ENDING)) {
            *singles += glob(pattern, name);
            continue;
        }
        name[strlen(name) - strlen(ENDING)] = 0;
        if (!glob(pattern, name)) continue;
        Row *more = realloc(*rows, sizeof(Row) * (size_t)(n + 1));
        if (!more) {
            n = -1;
            break;
        }
        *rows = more;
        memcpy(&more[n].offset, row, 4);
        memcpy(&more[n].size, row + 4, 4);
        snprintf(more[n].name, NAME_SIZE, "%s", name);
        n++;
    }
    host_close(fd);
    return n;
}

/* Writes the files of the merged part at offset into the Windows folder dir. 0 on success. */
static int extract(const char *host, int compressed, const Row *row, const char *dir, const char *pattern, int *files)
{
    static Reader r;
    static uint8_t chunk[1 << 16];
    memset(&r, 0, sizeof r);
    r.compressed = compressed;
    r.fd = host_open(host, O_RDONLY, 0);
    if (r.fd < 0) return 1;
    int bad = host_seek(r.fd, row->offset, SEEK_SET) != (int64_t)row->offset;
    char magic[8];
    uint32_t n = 0, used = 12;
    if (!bad && (!get(&r, magic, 8) || memcmp(magic, MAGIC, 8) || !get32(&r, &n) || n > MAX_FILES)) bad = 1;
    for (uint32_t i = 0; i < n && !bad; i++) {
        uint32_t len, size;
        char name[NAME_SIZE], path[1400];
        if (!get32(&r, &len) || len == 0 || len >= NAME_SIZE || !get(&r, name, len)) {
            bad = 1;
            break;
        }
        name[len] = 0;
        /* Only a file of the part's own type that the pattern takes, as the original's name test (0x933a00): a
         * damaged or made-up part writes no other file into the folder. */
        char type[NAME_SIZE];
        if (strchr(name, '\\') || strchr(name, '/') || strchr(name, ':') || !glob(pattern, name) ||
            !type_of(name, type, sizeof type) || strcmp(type, row->name) || !get32(&r, &size)) {
            bad = 1;
            break;
        }
        used += 8 + len;
        if ((uint64_t)used + size > row->size) {
            bad = 1;
            break;
        }
        used += size;
        /* As _makepath 0x945b13 (the paths of 0x933320 and 0x933610): a separator only after a folder that has none. */
        size_t dl = strlen(dir);
        snprintf(path, sizeof path, "%s%s%s", dir, dl && dir[dl - 1] != '\\' && dir[dl - 1] != '/' ? "\\" : "", name);
        int out = win_create_file(path);
        if (out < 0) {
            bad = 1;
            break;
        }
        for (uint32_t left = size; left && !bad;) {
            uint32_t take = left < sizeof chunk ? left : (uint32_t)sizeof chunk;
            if (!get(&r, chunk, take) || host_write(out, chunk, take) != (long)take) bad = 1;
            left -= take;
        }
        host_close(out);
        (*files)++;
    }
    if (!bad && used != row->size) bad = 1;
    host_close(r.fd);
    return bad;
}

void o_00933320(void);

/* zextract_matching_files, cdecl (archive, folder, pattern): 1 when a file was written. The load fails on 0
 * (0x82acc6). */
RT_WRAP(00933320, extract_matching_files)
{
    char archive[1024], dir[1024], pattern[NAME_SIZE], host[2048];
    snprintf(archive, sizeof archive, "%.1023s", win_str(rt_arg(0)));
    snprintf(dir, sizeof dir, "%.1023s", win_str(rt_arg(1)));
    snprintf(pattern, sizeof pattern, "%.259s", win_str(rt_arg(2)));
    o_00933320();
    if (rt_unwinding || !win_existing_file(archive, host, sizeof host)) return;
    Row *rows;
    int compressed = 0, singles, n = merged_rows(host, pattern, &rows, &compressed, &singles), files = 0, bad = 0;
    /* The original gives 0 when it could not write a file of a part of its own (0x93352d..0x9335ce): the load
     * fails then, merged parts or not. */
    int failed = n > 0 && singles && !cpu.eax;
    for (int i = 0; i < n && !bad && !failed; i++) {
        if (extract(host, compressed, &rows[i], dir, pattern, &files)) {
            fprintf(stderr, "cm0102_recomp: the part %s" ENDING " of the save %s is damaged, so the game does not "
                            "load it.\n", rows[i].name, archive);
            bad = 1;
        }
    }
    free(rows);
    if (n > 0 && getenv("RECOMP_SAVE_PARTS_LOG"))   /* test switch: tests/seasons/save_parts_run.py */
        fprintf(stderr, "recomp: save parts: %d merged parts gave %d temporary files in %s\n", n, files, dir);
    if (bad || failed) cpu.eax = 0;
    else if (files) cpu.eax = 1;
}

void o_00933860(void);

/* zget_matching_size, cdecl (archive, pattern): the bytes of the matching parts, for the disk check of the load. */
RT_WRAP(00933860, matching_size)
{
    char archive[1024], pattern[NAME_SIZE], host[2048];
    snprintf(archive, sizeof archive, "%.1023s", win_str(rt_arg(0)));
    snprintf(pattern, sizeof pattern, "%.259s", win_str(rt_arg(1)));
    o_00933860();
    if (rt_unwinding || !win_existing_file(archive, host, sizeof host)) return;
    Row *rows;
    int compressed, singles, n = merged_rows(host, pattern, &rows, &compressed, &singles);
    uint64_t total = cpu.eax;
    for (int i = 0; i < n; i++) total += rows[i].size;
    free(rows);
    cpu.eax = total > 0x7fffffffu ? 0x7fffffffu : (uint32_t)total;
}
