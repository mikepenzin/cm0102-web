/* The key competition check of a load (season_load_check.c, 0.6.1 item 10) on the names of the player's own
 * cm0102.exe and the competition records of its Data folder. ctest season_load_check.
 * Usage: season_load_check_test <cm0102.exe> <Data folder> */
#include "season_load_check.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD 0x6b
#define NAME_AT 4
#define MAX_NAMES 4096

static char store[MAX_NAMES][52];
static const char *names[MAX_NAMES];
static int count, failures;

/* The exe: a name address maps to its section in the file (the PE section table). */
static unsigned char *exe;
static long exe_size;

static uint32_t u32_at(long at) { return at + 4 <= exe_size ? (uint32_t)exe[at] | (uint32_t)exe[at + 1] << 8 |
                                  (uint32_t)exe[at + 2] << 16 | (uint32_t)exe[at + 3] << 24 : 0; }

static const char *name_at(uint32_t address)
{
    long pe = u32_at(0x3c);
    uint32_t sections = u32_at(pe + 4) & 0xffff, optional = u32_at(pe + 20) & 0xffff, base = u32_at(pe + 52);
    for (uint32_t i = 0; i < sections; i++) {
        long row = pe + 24 + optional + 40 * (long)i;
        uint32_t va = base + u32_at(row + 12), raw_size = u32_at(row + 16), raw = u32_at(row + 20);
        if (address >= va && address < va + raw_size && raw + raw_size <= (uint32_t)exe_size)
            return (const char *)exe + raw + (address - va);
    }
    return NULL;
}

static int load_exe(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f || fseek(f, 0, SEEK_END) || (exe_size = ftell(f)) <= 0x400 || fseek(f, 0, SEEK_SET)) return 0;
    exe = malloc((size_t)exe_size + 1);
    int ok = exe && fread(exe, 1, (size_t)exe_size, f) == (size_t)exe_size;
    fclose(f);
    if (ok) exe[exe_size] = 0;
    return ok;
}

static void read_records(const char *dir, const char *file)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    FILE *f = fopen(path, "rb");
    unsigned char rec[RECORD];
    if (!f) {
        printf("FAIL  cannot open %s\n", path);
        exit(1);
    }
    while (count < MAX_NAMES && fread(rec, 1, RECORD, f) == RECORD) {
        memcpy(store[count], rec + NAME_AT, 51);
        store[count][51] = 0;
        names[count] = store[count];
        count++;
    }
    fclose(f);
}

static int find(const char *name)
{
    for (int i = 0; i < count; i++)
        if (!strcmp(names[i], name)) return i;
    return -1;
}

static void check(int ok, const char *text)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", text);
    if (!ok) failures++;
}

int main(int argc, char **argv)
{
    char text[200];
    if (argc != 3 || !load_exe(argv[1])) {
        printf("FAIL  usage: season_load_check_test <cm0102.exe> <Data folder>\n");
        return 1;
    }
    read_records(argv[2], "club_comp.dat");
    read_records(argv[2], "nation_comp.dat");
    int n = season_key_comps_missing(names, count, name_at);
    snprintf(text, sizeof text, "the %d records of the 3.9.68 data: %d key competitions missing", count, n);
    check(count > 400 && n == 0, text);

    /* The case of a name does not matter (_stricmp). */
    int premier = find("English Premier Division");
    check(premier >= 0, "the data holds English Premier Division");
    if (premier < 0) return 1;
    for (char *p = store[premier]; *p; p++) *p = (char)toupper((unsigned char)*p);
    n = season_key_comps_missing(names, count, name_at);
    check(n == 0, "ENGLISH PREMIER DIVISION still sets its key id");

    /* The names of a 2002/03 season game: a save of these records without its season part. */
    snprintf(store[premier], 52, "FA Premier League");
    n = season_key_comps_missing(names, count, name_at);
    snprintf(text, sizeof text, "English Premier Division renamed FA Premier League: %d key competitions missing", n);
    check(n >= 1, text);
    int ligue = find("French First Division");
    check(ligue >= 0, "the data holds French First Division");
    if (ligue >= 0) snprintf(store[ligue], 52, "Ligue 1");
    int more = season_key_comps_missing(names, count, name_at);
    snprintf(text, sizeof text, "and French First Division renamed Ligue 1: %d missing", more);
    check(more > n, text);

    printf("%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
