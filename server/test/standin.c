/* A stand-in for the recompiled game in the tests of the server: it speaks the engine worker protocol with the
 * runtime's own code (src/recomp_rt/win/worker_proto.c) and plays pretend days, so the server can be tested
 * without the clean files. It is not the game: no data set, no match engine.
 *
 * Environment, as the server sets it for the game: RECOMP_WORKER_FD (the link), RECOMP_USER_DIR (the save
 * folder). STANDIN_DAY_MS: real milliseconds per game day (default 2).
 *
 * At the start it loads the first save of the user folder in name order, as the load steps of the server pick
 * the first row of the Load Game list; with no save it starts a new game on 14.7.2001 at Arsenal (the club and
 * date of the test fixture start.sav). A save is a real save container (tools/save/savfile.py), compressed, with
 * one part, standin.dat: s16 day of the year from 0, s16 year, then the club name. */
#include "worker_proto.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HEADER_SIZE 0x416ec
#define ENTRY_SIZE 0x10c
#define PART "standin.dat"

static WorkerLink link;
static int day_of_year = 194, year = 2001;     /* 14.7.2001 */
static char club[64] = "Arsenal";
static char user_dir[1024];
static int day_ms = 2;

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static void next_day(void)
{
    if (++day_of_year >= 365 + leap(year)) { day_of_year = 0; year++; }
    struct timespec ts = {day_ms / 1000, (long)(day_ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static void date_of(int *month, int *day)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = day_of_year, m = 0;
    while (m < 11 && d >= days[m] + (m == 1 && leap(year))) d -= days[m] + (m == 1 && leap(year)), m++;
    *month = m + 1;
    *day = d + 1;
}

static void send(const char *line, int n)
{
    if (n < 0 || worker_write_all(link.fd, line, (size_t)n) != 0) exit(1);
}

static void send_state(int turn_event)
{
    char line[WORKER_LINE_MAX];
    WorkerState st = {year, 0, 0, club[0] ? club : NULL};
    date_of(&st.month, &st.day);
    send(line, worker_json_state(line, sizeof line, turn_event, &st));
}

/* The run code of the save container (encoder 0x932e10, as tools/save/savfile.py encode). */
static size_t encode(const uint8_t *in, size_t n, uint8_t *out)
{
    size_t o = 0;
    for (size_t i = 0; i < n;) {
        size_t j = i + 1;
        while (j < n && in[j] == in[i] && j - i < 0x7f) j++;
        size_t count = j - i;
        if (count < 2 && in[i] < 0x81) out[o++] = in[i];
        else { out[o++] = (uint8_t)(count + 0x80); out[o++] = in[i]; }
        i = j;
    }
    return o;
}

static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

static int write_save(const char *path)
{
    uint8_t part[2 + 2 + 64 + 200] = {0}, packed[sizeof part * 2];
    part[0] = (uint8_t)day_of_year; part[1] = (uint8_t)(day_of_year >> 8);
    part[2] = (uint8_t)year; part[3] = (uint8_t)(year >> 8);
    memcpy(part + 4, club, strlen(club));                  /* then 200 zero bytes: a run for the encoder */
    size_t packed_len = encode(part, sizeof part, packed);
    uint8_t *header = calloc(1, HEADER_SIZE);
    if (!header) return -1;
    put32(header, 2);                                      /* version 2: compressed */
    put32(header + 8, 1);                                  /* one part */
    put32(header + 12, HEADER_SIZE);
    put32(header + 16, (uint32_t)sizeof part);
    memcpy(header + 20, PART, sizeof PART);
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(header, 1, HEADER_SIZE, f) == HEADER_SIZE && fwrite(packed, 1, packed_len, f) == packed_len;
    if (f && fclose(f) != 0) ok = 0;
    free(header);
    return ok ? 0 : -1;
}

static int read_save(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    static uint8_t blob[HEADER_SIZE + 4096];
    size_t n = fread(blob, 1, sizeof blob, f);
    fclose(f);
    if (n < HEADER_SIZE + 4 || blob[0] != 2 || blob[8] != 1 || strcmp((char *)blob + 20, PART) != 0) return -1;
    uint32_t off = blob[12] | blob[13] << 8 | blob[14] << 16 | (uint32_t)blob[15] << 24;
    uint32_t size = blob[16] | blob[17] << 8;
    uint8_t part[512] = {0};
    size_t o = 0;
    for (size_t i = off; i < n && o < size && o < sizeof part;) {
        uint8_t b = blob[i++];
        if (b < 0x81) { part[o++] = b; continue; }
        if (i >= n) break;
        uint8_t v = blob[i++];
        for (int k = 0; k < b - 0x80 && o < sizeof part; k++) part[o++] = v;
    }
    day_of_year = part[0] | part[1] << 8;
    year = part[2] | part[3] << 8;
    snprintf(club, sizeof club, "%.63s", (char *)part + 4);
    return 0;
}

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* Loads the first .sav of the user folder in name order. 0 when there is none. */
static int load_first(void)
{
    DIR *d = opendir(user_dir);
    if (!d) return 0;
    char *names[256];
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d)) && count < 256) {
        size_t len = strlen(e->d_name);
        if (len > 4 && strcmp(e->d_name + len - 4, ".sav") == 0) names[count++] = strdup(e->d_name);
    }
    closedir(d);
    if (!count) return 0;
    qsort(names, (size_t)count, sizeof names[0], by_name);
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", user_dir, names[0]);
    if (read_save(path) != 0) { fprintf(stderr, "standin: %s is not a stand-in save\n", path); exit(2); }
    fprintf(stderr, "standin: loaded %s\n", path);
    for (int i = 0; i < count; i++) free(names[i]);
    return 1;
}

int main(void)
{
    const char *fd = getenv("RECOMP_WORKER_FD"), *dir = getenv("RECOMP_USER_DIR"), *ms = getenv("STANDIN_DAY_MS");
    if (!fd || !dir) { fprintf(stderr, "standin: RECOMP_WORKER_FD and RECOMP_USER_DIR are needed\n"); return 2; }
    link.fd = atoi(fd);
    snprintf(user_dir, sizeof user_dir, "%s", dir);
    if (ms) day_ms = atoi(ms);
    if (!load_first()) fprintf(stderr, "standin: new game\n");
    for (;;) {                                             /* each pass: one human turn where the server answers */
        send_state(1);
        int go_on = 0;
        while (!go_on) {
            char text[WORKER_LINE_MAX + 1], line[WORKER_LINE_MAX + 2048];
            if (worker_read_line(&link, text, sizeof text) < 0) return 0;
            WorkerCommand cmd;
            switch (worker_parse(text, &cmd)) {
            case WK_STATE: send_state(0); break;
            case WK_SAVE: {
                char path[2048];
                snprintf(path, sizeof path, "%s/%s.sav", user_dir, cmd.name);
                if (write_save(path) != 0) send(line, worker_json_error(line, sizeof line, "the save wrote no file"));
                else send(line, worker_json_saved(line, sizeof line, path));
                break;
            }
            case WK_ADVANCE: {
                int until = worker_serial(day_of_year, year) + cmd.days;
                send(line, worker_json_until(line, sizeof line, until));
                while (worker_serial(day_of_year, year) < until) next_day();
                go_on = 1;
                break;
            }
            case WK_QUIT: send(line, worker_json_ok(line, sizeof line)); return 0;
            default: send(line, worker_json_error(line, sizeof line, cmd.error));
            }
        }
    }
}
