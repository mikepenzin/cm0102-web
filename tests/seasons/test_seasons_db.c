/* Test of the season databases (src/recomp_rt/win/seasons_db.c, seasons_zip.c, seasons_table.c).
 *
 * Usage:
 *   seasons_db_test                                  unit checks (ctest). It makes and removes its own folder.
 *   seasons_db_test prepare <game> <save> <year>     calls seasons_db_prepare_in and prints the result
 *   seasons_db_test scan <game> <save>               calls seasons_db_scan_in and prints the list
 *   seasons_db_test identify <folder>                prints the known database of a folder or "custom"
 * The commands are for tests/seasons/seasons_db_run.py, which tests the real zips. */
#include "seasons_db.h"
#include "seasons_zip.h"
#include "../../src/recomp_rt/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vectors.h"

/* Stand-ins for files.c. */
char win_game_dir[1024];
static char the_save[1024];
const char *win_databases_root(void) { return the_save; }

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void sha_of(const char *s, size_t n, size_t split, char hex[65])
{
    SeasonsSha sh;
    seasons_sha_init(&sh);
    seasons_sha_update(&sh, s, split);
    seasons_sha_update(&sh, s + split, n - split);
    seasons_sha_hex(&sh, hex);
}

static void test_sha_crc(void)
{
    char hex[65];
    sha_of("", 0, 0, hex);
    CHECK(!strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    sha_of("abc", 3, 1, hex);
    CHECK(!strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha_of(two, strlen(two), 17, hex);
    CHECK(!strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    char big[1000];                               /* a padding edge: 55, 56, 64 bytes */
    memset(big, 'a', sizeof big);
    sha_of(big, 55, 10, hex);
    CHECK(!strcmp(hex, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"));
    sha_of(big, 56, 0, hex);
    CHECK(!strcmp(hex, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
    sha_of(big, 64, 64, hex);
    CHECK(!strcmp(hex, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"));
    CHECK(seasons_crc32(0, "123456789", 9) == 0xcbf43926u);
    uint32_t c = seasons_crc32(0, "1234", 4);
    CHECK(seasons_crc32(c, "56789", 5) == 0xcbf43926u);
}

static void test_inflate(void)
{
    uint8_t out[9000];
    long n = seasons_inflate_buffer(fixed_deflate, sizeof fixed_deflate, out, sizeof out);
    CHECK(n == (long)strlen(fixed_src) && !memcmp(out, fixed_src, (size_t)n));
    n = seasons_inflate_buffer(stored_deflate, sizeof stored_deflate, out, sizeof out);
    CHECK(n == 512);
    int ok = n == 512;
    for (int i = 0; ok && i < 512; i++) ok = out[i] == (i & 255);
    CHECK(ok);
    n = seasons_inflate_buffer(dyn_deflate, sizeof dyn_deflate, out, sizeof out);
    CHECK(n == (long)dyn_size);
    char hex[65];
    if (n > 0) sha_of((const char *)out, (size_t)n, 5, hex); else hex[0] = 0;
    CHECK(!strcmp(hex, dyn_sha256));
    /* damaged data: a cut stream and a too small output buffer fail */
    CHECK(seasons_inflate_buffer(dyn_deflate, sizeof dyn_deflate / 2, out, sizeof out) < 0);
    CHECK(seasons_inflate_buffer(dyn_deflate, sizeof dyn_deflate, out, 100) < 0);
    uint8_t bad[sizeof dyn_deflate];
    memcpy(bad, dyn_deflate, sizeof bad);
    bad[0] = 0xff;                                /* block type 3 does not exist */
    CHECK(seasons_inflate_buffer(bad, sizeof bad, out, sizeof out) < 0);
}

static char root[1024];

static void path_of(char *out, size_t room, const char *a, const char *b)
{
    snprintf(out, room, "%s/%s", a, b);
}

static int read_all(const char *path, char *buf, size_t room)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, room, f);
    fclose(f);
    return (int)n;
}

static void test_zip(void)
{
    char zp[1100], p[1200], err[200] = "", hex[65];
    path_of(zp, sizeof zp, root, "small.zip");
    FILE *f = fopen(zp, "wb");
    fwrite(small_zip, 1, sizeof small_zip, f);
    fclose(f);
    SeasonsZip z;
    CHECK(seasons_zip_open(&z, zp, err, sizeof err) == 0);
    CHECK(z.count == 3);
    int club = -1, readme = -1;
    for (int i = 0; i < z.count; i++) {
        if (!strcmp(z.e[i].name, "Test db/club.dat")) club = i;
        if (!strcmp(z.e[i].name, "Test db/readme.txt")) readme = i;
    }
    CHECK(club >= 0 && readme >= 0 && z.e[club].method == 8 && z.e[readme].method == 0 && z.e[0].is_dir);
    path_of(p, sizeof p, root, "club.out");
    CHECK(seasons_zip_extract(&z, club, p, hex, err, sizeof err) == 0);
    CHECK(!strcmp(hex, zip_club_sha256));
    HostStat st;
    CHECK(host_stat(p, &st) == 0 && st.size == dyn_size);
    path_of(p, sizeof p, root, "readme.out");
    CHECK(seasons_zip_extract(&z, readme, p, NULL, err, sizeof err) == 0);
    char buf[64];
    int n = read_all(p, buf, sizeof buf);
    CHECK(n == 13 && !memcmp(buf, "stored entry\n", 13));
    seasons_zip_close(&z);

    /* A flipped bit in the packed data: the extract fails and leaves no file. */
    uint8_t *copy = malloc(sizeof small_zip);
    memcpy(copy, small_zip, sizeof small_zip);
    copy[200] ^= 0x10;
    path_of(zp, sizeof zp, root, "damaged.zip");
    f = fopen(zp, "wb");
    fwrite(copy, 1, sizeof small_zip, f);
    fclose(f);
    free(copy);
    CHECK(seasons_zip_open(&z, zp, err, sizeof err) == 0);
    path_of(p, sizeof p, root, "damaged.out");
    CHECK(seasons_zip_extract(&z, club, p, hex, err, sizeof err) != 0 && err[0]);
    CHECK(host_stat(p, &st) != 0);
    seasons_zip_close(&z);

    /* An entry that holds more data than its size says (a zip bomb): the unpack stops at the stated size. */
    copy = malloc(sizeof small_zip);
    memcpy(copy, small_zip, sizeof small_zip);
    for (size_t i = 0; i + 46 < sizeof small_zip; i++)
        if (!memcmp(copy + i, "PK\1\2", 4) && !memcmp(copy + i + 46, "Test db/club.dat", 16)) {
            copy[i + 24] = 100; copy[i + 25] = 0; copy[i + 26] = 0; copy[i + 27] = 0;
        }
    path_of(zp, sizeof zp, root, "bomb.zip");
    f = fopen(zp, "wb");
    fwrite(copy, 1, sizeof small_zip, f);
    fclose(f);
    free(copy);
    CHECK(seasons_zip_open(&z, zp, err, sizeof err) == 0);
    path_of(p, sizeof p, root, "bomb.out");
    err[0] = 0;
    CHECK(seasons_zip_extract(&z, club, p, hex, err, sizeof err) != 0 && strstr(err, "more data"));
    CHECK(host_stat(p, &st) != 0);
    seasons_zip_close(&z);

    /* A file that is not a zip, and a cut zip. */
    path_of(zp, sizeof zp, root, "text.zip");
    f = fopen(zp, "wb");
    fputs("this is not a zip file, it is only a few bytes of text", f);
    fclose(f);
    CHECK(seasons_zip_open(&z, zp, err, sizeof err) != 0 && err[0]);
    path_of(zp, sizeof zp, root, "cut.zip");
    f = fopen(zp, "wb");
    fwrite(small_zip, 1, sizeof small_zip / 2, f);
    fclose(f);
    CHECK(seasons_zip_open(&z, zp, err, sizeof err) != 0);
}

/* The table in the code equals the table file (tools/seasons/make_db_table.py also checks that). */
static void test_table(void)
{
#ifdef SIGNATURES_TSV
    FILE *f = fopen(SIGNATURES_TSV, "r");
    CHECK(f != NULL);
    if (!f) return;
    char line[1024];
    int rows = 0;
    if (!fgets(line, sizeof line, f)) return;           /* the heading */
    while (fgets(line, sizeof line, f)) {
        char *col[10];
        int n = 0;
        for (char *p = strtok(line, "\t\r\n"); p && n < 10; p = strtok(NULL, "\t\r\n")) col[n++] = p;
        CHECK(n == 10);
        if (n < 10) continue;
        CHECK(rows < seasons_known_count);
        const SeasonKnownDb *k = &seasons_known_dbs[rows];
        CHECK(!strcmp(k->name, col[0]));
        CHECK(k->club_size == strtoull(col[6], NULL, 10) && k->staff_size == strtoull(col[8], NULL, 10));
        CHECK(!strcmp(k->club_sha256, col[7]) && !strcmp(k->staff_sha256, col[9]));
        rows++;
    }
    fclose(f);
    CHECK(rows == seasons_known_count && rows == 52);
#endif
    /* 24 season databases, none for 2001 (the official data); every signature is unique. Summer 2002, Summer 2004
     * and Summer 2006 are our own (Q351; Summer 2002 spec, slice 4: the 2002/03 tile starts Summer 2002). Every
     * season from 2002/03 on has one, so the screen's "No database" tile cannot appear with these tables (this
     * loop fails first). Summer 2002 has the club.dat size of 3.9.68, Summer 2004 that of September 2005 and
     * Summer 2006 that of November 2007 (the template keeps every club), with other staff.dat sizes, so the check
     * also compares the hashes. */
    int have = 0;
    for (int y = SEASONS_FIRST; y < SEASONS_FIRST + SEASONS_COUNT; y++) {
        const SeasonKnownDb *k = seasons_db_for_season(y);
        have += k != NULL;
        if (y == 2001) CHECK(!k);
        else CHECK(k && k->season == y);
    }
    CHECK(have == 24);
    CHECK(seasons_db_for_season(2002) && !strcmp(seasons_db_for_season(2002)->name, "Summer 2002"));
    for (int i = 0; i < seasons_known_count; i++)
        for (int j = i + 1; j < seasons_known_count; j++)
            CHECK(seasons_known_dbs[i].club_size != seasons_known_dbs[j].club_size ||
                  seasons_known_dbs[i].staff_size != seasons_known_dbs[j].staff_size ||
                  strcmp(seasons_known_dbs[i].club_sha256, seasons_known_dbs[j].club_sha256) ||
                  strcmp(seasons_known_dbs[i].staff_sha256, seasons_known_dbs[j].staff_sha256));
    CHECK(seasons_db_known("October 2003") && !seasons_db_known("October 2003 Extra"));
    CHECK(seasons_db_decade(2001) == SEASONS_DECADE_2000S && seasons_db_decade(2009) == SEASONS_DECADE_2000S &&
          seasons_db_decade(2010) == SEASONS_DECADE_2010S && seasons_db_decade(2019) == SEASONS_DECADE_2010S &&
          seasons_db_decade(2020) == SEASONS_DECADE_2020S && seasons_db_decade(2025) == SEASONS_DECADE_2020S);
    CHECK(!strcmp(seasons_db_decade_name(SEASONS_DECADE_2010S), "2010s"));
}

static void test_prepare_without_zip(void)
{
    char game[1100], save[1100];
    path_of(game, sizeof game, root, "game");
    path_of(save, sizeof save, root, "save");
    mkdir(game, 0755);
    mkdir(save, 0755);
    SeasonsPrepare r;
    memset(&r, 0, sizeof r);
    CHECK(seasons_db_prepare_in(game, save, 2001, &r) == SEASONS_PREPARE_OFFICIAL);
    CHECK(seasons_db_prepare_in(game, save, 2002, &r) == SEASONS_PREPARE_NO_ZIP && r.error[0] && !r.name[0]);
    CHECK(seasons_db_prepare_in(game, save, 2004, &r) == SEASONS_PREPARE_NO_ZIP);
    CHECK(seasons_db_prepare_in(game, save, 2006, &r) == SEASONS_PREPARE_NO_ZIP);
    CHECK(seasons_db_prepare_in(game, save, 2026, &r) == SEASONS_PREPARE_NO_DATABASE && r.error[0]);
    CHECK(seasons_db_prepare_in(game, save, 2003, &r) == SEASONS_PREPARE_NO_ZIP && r.error[0] && !r.name[0]);
    /* A zip that is no season database: the unpack fails and leaves no folder. */
    char sd[1100], zp[1200];
    path_of(sd, sizeof sd, game, "Seasons");
    mkdir(sd, 0755);
    path_of(zp, sizeof zp, sd, "October 2003.zip");
    FILE *f = fopen(zp, "wb");
    fwrite(small_zip, 1, sizeof small_zip, f);
    fclose(f);
    CHECK(seasons_db_prepare_in(game, save, 2003, &r) == SEASONS_PREPARE_FAILED && r.error[0]);
    char dbs[1200], tmp[1200];
    path_of(dbs, sizeof dbs, save, "Databases/October 2003");
    path_of(tmp, sizeof tmp, save, "unpack-tmp/October 2003");
    HostStat st;
    CHECK(host_stat(dbs, &st) != 0 && host_stat(tmp, &st) != 0);
    SeasonsList *list = malloc(sizeof *list);
    seasons_db_scan_in(game, save, list);
    CHECK(list->season[0].status == SEASONS_OFFICIAL && !strcmp(list->season[0].database, "3.9.68"));
    CHECK(list->season[1].status == SEASONS_MISSING && !strcmp(list->season[1].database, "Summer 2002") &&
          list->season[1].extra_count == 0);
    CHECK(list->season[2].status == SEASONS_IN_DOWNLOAD && !strcmp(list->season[2].database, "October 2003"));
    CHECK(list->season[3].status == SEASONS_MISSING && !strcmp(list->season[3].database, "Summer 2004"));
    CHECK(list->season[4].status == SEASONS_MISSING && list->season[5].status == SEASONS_MISSING);
    CHECK(!strcmp(list->season[2].label, "2003/04") && !strcmp(list->season[24].label, "2025/26"));
    CHECK(list->custom_count == 0 && list->season[0].extra_count == 0);
    /* The data sets of the game folder are the versions of 2001/02. */
    static const char *const files[] = {
        "city.dat", "club.dat", "club_comp.dat", "club_comp_history.dat", "colour.dat", "common_names.dat",
        "continent.dat", "first_names.dat", "index.dat", "nat_club.dat", "nation.dat", "nation_comp.dat",
        "nation_comp_history.dat", "officials.dat", "second_names.dat", "stadium.dat", "staff.dat",
        "staff_comp.dat", "staff_comp_history.dat", "staff_history.dat"};
    char ds[1200], fp[1400];
    path_of(ds, sizeof ds, game, "Data-3.9.60");
    mkdir(ds, 0755);
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        path_of(fp, sizeof fp, ds, files[i]);
        FILE *g = fopen(fp, "wb");
        fputs("x", g);
        fclose(g);
    }
    path_of(ds, sizeof ds, game, "Data-3.9.65");     /* an incomplete set does not count */
    mkdir(ds, 0755);
    seasons_db_scan_in(game, save, list);
    CHECK(list->season[0].extra_count == 1 && list->season[0].extra[0].data_set &&
          !strcmp(list->season[0].extra[0].value, "3.9.60") && !strcmp(list->season[0].extra[0].db->name, "3.9.60"));
    free(list);
}

/* Removes a folder that this test made: files and folders up to four levels deep. */
static void remove_tree(const char *dir, int depth)
{
    char names[64][260];
    int n = 0;
    HostDir *d = host_opendir(dir);
    if (!d) return;
    const char *e;
    while ((e = host_readdir(d)) && n < 64)
        if (strcmp(e, ".") && strcmp(e, "..")) snprintf(names[n++], sizeof names[0], "%s", e);
    host_closedir(d);
    for (int i = 0; i < n; i++) {
        char p[1600];
        HostStat st;
        snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        if (host_stat(p, &st)) continue;
        if (st.is_dir) { if (depth < 4) remove_tree(p, depth + 1); }
        else host_unlink(p);
    }
    host_rmdir(dir);
}

int main(int argc, char **argv)
{
    if (argc >= 2) {
        if (!strcmp(argv[1], "identify") && argc == 3) {
            const SeasonKnownDb *k = seasons_db_identify(argv[2]);
            printf("%s\n", k ? k->name : "custom");
            return 0;
        }
        if (!strcmp(argv[1], "prepare") && argc == 5) {
            snprintf(win_game_dir, sizeof win_game_dir, "%s", argv[2]);
            SeasonsPrepare r;
            memset(&r, 0, sizeof r);
            seasons_db_prepare_in(argv[2], argv[3], atoi(argv[4]), &r);
            printf("status=%d name=%s unpacked=%d error=%s\n", (int)r.status, r.name, r.unpacked, r.error);
            return 0;
        }
        if (!strcmp(argv[1], "scan") && argc == 4) {
            SeasonsList *list = malloc(sizeof *list);
            seasons_db_scan_in(argv[2], argv[3], list);
            for (int i = 0; i < SEASONS_COUNT; i++) {
                const SeasonsEntry *s = &list->season[i];
                printf("%s decade=%s db=%s status=%d folder=%s extras=", s->label, seasons_db_decade_name(s->decade),
                       s->database ? s->database : "-", (int)s->status, s->folder);
                for (int k = 0; k < s->extra_count; k++) printf("%s%s", k ? "," : "", s->extra[k].db->name);
                printf("\n");
            }
            for (int i = 0; i < list->custom_count; i++) printf("custom %s\n", list->custom[i]);
            free(list);
            return 0;
        }
        fprintf(stderr, "seasons_db_test: bad arguments\n");
        return 2;
    }
    char tmpl[] = "/tmp/seasons_db_test_XXXXXX";
    if (!mkdtemp(tmpl)) return 2;
    snprintf(root, sizeof root, "%s", tmpl);
    test_sha_crc();
    test_inflate();
    test_table();
    test_zip();
    test_prepare_without_zip();
    remove_tree(root, 0);
    if (failures) {
        printf("%d checks failed\n", failures);
        return 1;
    }
    printf("seasons_db_test: all checks passed\n");
    return 0;
}
