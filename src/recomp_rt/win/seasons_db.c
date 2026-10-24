/* Season databases in the download (seasons_db.h). */
#include "seasons_db.h"
#include "seasons_zip.h"
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

/* From win.h (files.c). Declared here so that the tests can link this file with two stand-ins. */
extern char win_game_dir[1024];
const char *win_databases_root(void);

/* The 20 data files of dbfolders.c. dbfolders_valid checks the same list after an unpack. */
static const char *const data_files[] = {
    "city.dat", "club.dat", "club_comp.dat", "club_comp_history.dat", "colour.dat", "common_names.dat",
    "continent.dat", "first_names.dat", "index.dat", "nat_club.dat", "nation.dat", "nation_comp.dat",
    "nation_comp_history.dat", "officials.dat", "second_names.dat", "stadium.dat", "staff.dat",
    "staff_comp.dat", "staff_comp_history.dat", "staff_history.dat"};
enum { FILE_COUNT = sizeof data_files / sizeof *data_files };

/* ---- Names and the table ---- */

SeasonsDecade seasons_db_decade(int y)
{
    return y < 2010 ? SEASONS_DECADE_2000S : y < 2020 ? SEASONS_DECADE_2010S : SEASONS_DECADE_2020S;
}

const char *seasons_db_decade_name(SeasonsDecade decade)
{
    static const char *const names[] = {"2000s", "2010s", "2020s"};   /* Q346 */
    return (unsigned)decade < 3 ? names[decade] : "";
}

const SeasonKnownDb *seasons_db_known(const char *name)
{
    for (int i = 0; i < seasons_known_count; i++)
        if (!strcmp(seasons_known_dbs[i].name, name)) return &seasons_known_dbs[i];
    return NULL;
}

const SeasonKnownDb *seasons_db_for_season(int y)
{
    for (int i = 0; i < seasons_known_count; i++)
        if (seasons_known_dbs[i].role == SEASONS_ROLE_SEASON && seasons_known_dbs[i].season == y)
            return &seasons_known_dbs[i];
    return NULL;
}

/* ---- Recognition (Q264, Q271) ---- */

/* The path of the file with this name in dir, whatever its case. 0 when it is not there. */
static int find_file(const char *dir, const char *name, char *out, size_t room)
{
    HostDir *d = host_opendir(dir);
    if (!d) return 0;
    const char *e;
    int ok = 0;
    while ((e = host_readdir(d))) {
        if (!strcasecmp(e, name)) {
            ok = snprintf(out, room, "%s/%s", dir, e) < (int)room;
            break;
        }
    }
    host_closedir(d);
    return ok;
}

typedef struct {
    char path[1100];
    uint64_t club_size, staff_size;
    int64_t club_time, staff_time;
    int64_t checked;                /* the time (seconds) of the hash */
    const SeasonKnownDb *db;
} IdentCache;

/* The result per folder is kept in memory, and in the file .identify-cache in the Databases folder of the
 * save folder, so that the SHA-256 of a 45 MB database runs once for a folder and not at each start (it runs
 * at about 150 MB/s, so 49 folders would take about 13 seconds at each start).
 * - A row counts when the path, both sizes and both modification times are the same, and the hash ran at
 *   least 2 seconds after the last change of the files (a file that changes within a second of the hash with
 *   the same size could be missed otherwise).
 * - The first line holds a fingerprint of the signatures table. A release with another table ignores the old
 *   rows.
 * - A change of content that keeps both the size and the modification time is not seen. A tool that does
 *   that on purpose gets a wrong answer, which only matters for a database that the player edits.
 * The file is a help only: the game works without it. */
enum { CACHE_ROWS = 256, CACHE_SETTLE = 2 };
static IdentCache cache[CACHE_ROWS];
static int cache_used, cache_next;
static char cache_loaded[1100];     /* the Databases folder whose file is loaded */

static int64_t now_seconds(void) { return (int64_t)(host_unix_time_us() / 1000000u); }

/* The fingerprint of the table in the code (CRC-32 of names and hashes). */
static uint32_t table_fingerprint(void)
{
    uint32_t crc = 0;
    for (int i = 0; i < seasons_known_count; i++) {
        const SeasonKnownDb *k = &seasons_known_dbs[i];
        crc = seasons_crc32(crc, k->name, strlen(k->name));
        crc = seasons_crc32(crc, k->club_sha256, 64);
        crc = seasons_crc32(crc, k->staff_sha256, 64);
    }
    return crc;
}

/* The Databases folder of a database folder: its parent, when that is called Databases. */
static int databases_of(const char *folder, char *out, size_t room)
{
    const char *slash = strrchr(folder, '/');
    if (!slash || slash == folder || (size_t)(slash - folder) >= room) return 0;
    snprintf(out, room, "%.*s", (int)(slash - folder), folder);
    const char *name = strrchr(out, '/');
    name = name ? name + 1 : out;
    return !strcasecmp(name, "Databases");
}

static int row_trusted(const IdentCache *c)
{
    int64_t last = c->club_time > c->staff_time ? c->club_time : c->staff_time;
    return c->checked >= last + CACHE_SETTLE;
}

static void cache_add(const IdentCache *row)
{
    for (int i = 0; i < cache_used; i++)
        if (!strcmp(cache[i].path, row->path)) { cache[i] = *row; return; }
    cache[cache_next] = *row;
    cache_next = (cache_next + 1) % CACHE_ROWS;
    if (cache_used < CACHE_ROWS) cache_used++;
}

static void cache_load(const char *parent)
{
    if (!strcmp(cache_loaded, parent)) return;
    snprintf(cache_loaded, sizeof cache_loaded, "%s", parent);
    char file[1200], line[2048];
    snprintf(file, sizeof file, "%s/.identify-cache", parent);
    FILE *f = host_fopen(file, "r");
    if (!f) return;
    char want[40];
    snprintf(want, sizeof want, "table %08x", (unsigned)table_fingerprint());
    if (!fgets(line, sizeof line, f) || strncmp(line, want, strlen(want))) { fclose(f); return; }
    while (fgets(line, sizeof line, f)) {
        IdentCache row;
        char *col[7];
        int n = 0;
        line[strcspn(line, "\r\n")] = 0;
        for (char *p = line; n < 7;) {
            col[n++] = p;
            char *t = strchr(p, '\t');
            if (!t) break;
            *t = 0;
            p = t + 1;
        }
        if (n != 7 || strlen(col[0]) >= sizeof row.path) continue;
        snprintf(row.path, sizeof row.path, "%s", col[0]);
        row.club_size = strtoull(col[1], NULL, 10);
        row.club_time = strtoll(col[2], NULL, 10);
        row.staff_size = strtoull(col[3], NULL, 10);
        row.staff_time = strtoll(col[4], NULL, 10);
        row.checked = strtoll(col[5], NULL, 10);
        row.db = NULL;
        if (strcmp(col[6], "-")) {
            row.db = seasons_db_known(col[6]);
            if (!row.db) continue;
        }
        cache_add(&row);
    }
    fclose(f);
}

static void cache_save(const char *parent)
{
    char file[1200], tmp[1210];
    snprintf(file, sizeof file, "%s/.identify-cache", parent);
    snprintf(tmp, sizeof tmp, "%s.tmp", file);
    FILE *f = host_fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "table %08x\n", (unsigned)table_fingerprint());
    size_t pl = strlen(parent);
    for (int i = 0; i < cache_used; i++) {
        const IdentCache *c = &cache[i];
        if (strncmp(c->path, parent, pl) || c->path[pl] != '/' || !row_trusted(c)) continue;
        fprintf(f, "%s\t%llu\t%lld\t%llu\t%lld\t%lld\t%s\n", c->path, (unsigned long long)c->club_size,
                (long long)c->club_time, (unsigned long long)c->staff_size, (long long)c->staff_time,
                (long long)c->checked, c->db ? c->db->name : "-");
    }
    int bad = ferror(f);
    if (fclose(f) || bad || host_replace(tmp, file)) host_unlink(tmp);
}

/* The sizes and times of club.dat and staff.dat of a folder. 0 when a file is missing. */
static int folder_files(const char *folder, char *club, size_t club_room, char *staff, size_t staff_room,
                        HostStat *cs, HostStat *ss)
{
    return find_file(folder, "club.dat", club, club_room) && find_file(folder, "staff.dat", staff, staff_room) &&
           !host_stat(club, cs) && !host_stat(staff, ss) && !cs->is_dir && !ss->is_dir;
}

const SeasonKnownDb *seasons_db_identify(const char *folder)
{
    char club[1100], staff[1100], parent[1100];
    HostStat cs, ss;
    if (!folder_files(folder, club, sizeof club, staff, sizeof staff, &cs, &ss) || strlen(folder) >= sizeof cache[0].path)
        return NULL;
    int have_parent = databases_of(folder, parent, sizeof parent);
    if (have_parent) cache_load(parent);
    for (int i = 0; i < cache_used; i++) {
        IdentCache *c = &cache[i];
        if (!strcmp(c->path, folder) && c->club_size == cs.size && c->staff_size == ss.size &&
            c->club_time == cs.mtime && c->staff_time == ss.mtime && row_trusted(c))
            return c->db;
    }
    int64_t started = now_seconds();
    const SeasonKnownDb *found = NULL;
    char club_hex[65] = "", staff_hex[65] = "";
    int hashed = 0;
    for (int i = 0; i < seasons_known_count && !found; i++) {
        const SeasonKnownDb *k = &seasons_known_dbs[i];
        if (k->club_size != cs.size || k->staff_size != ss.size) continue;       /* step 1: sizes only */
        if (!hashed) {                                                           /* step 2: contents */
            uint64_t n;
            if (!seasons_sha_file(club, &n, club_hex) || !seasons_sha_file(staff, &n, staff_hex)) return NULL;
            hashed = 1;
        }
        if (!strcmp(k->club_sha256, club_hex) && !strcmp(k->staff_sha256, staff_hex)) found = k;
    }
    IdentCache row;
    snprintf(row.path, sizeof row.path, "%s", folder);
    row.club_size = cs.size; row.staff_size = ss.size;
    row.club_time = cs.mtime; row.staff_time = ss.mtime;
    row.db = found;
    row.checked = started;
    if (row_trusted(&row)) {          /* a folder that changed a moment ago is looked at again next time */
        cache_add(&row);
        if (hashed && have_parent) cache_save(parent);    /* only a hash is worth a write */
    }
    return found;
}

/* 1 when the folder has the file sizes of this database. A fast test that reads no content. */
static int sizes_match(const char *folder, const SeasonKnownDb *row)
{
    char club[1100], staff[1100];
    HostStat cs, ss;
    return folder_files(folder, club, sizeof club, staff, sizeof staff, &cs, &ss) && cs.size == row->club_size &&
           ss.size == row->staff_size;
}

/* ---- The list for the Choose Season screen ---- */

static int zip_path(const char *game_dir, const SeasonKnownDb *db, char *out, size_t room)
{
    return snprintf(out, room, "%s/Seasons/%s.zip", game_dir, db->name) < (int)room;
}

void seasons_db_scan_in(const char *game_dir, const char *save_dir, SeasonsList *out)
{
    memset(out, 0, sizeof *out);
    for (int i = 0; i < SEASONS_COUNT; i++) {
        SeasonsEntry *s = &out->season[i];
        s->start_year = SEASONS_FIRST + i;
        snprintf(s->label, sizeof s->label, "%04d/%02d", s->start_year, (s->start_year + 1) % 100);
        s->decade = seasons_db_decade(s->start_year);
        if (s->start_year == SEASONS_FIRST) {     /* 2002/03 has Summer 2002 since 0.6.0 (Summer 2002 spec, slice 4) */
            s->database = "3.9.68";
            s->status = SEASONS_OFFICIAL;
        } else {
            const SeasonKnownDb *db = seasons_db_for_season(s->start_year);
            s->database = db ? db->name : NULL;
            s->status = db ? SEASONS_MISSING : SEASONS_NO_DATABASE;
        }
    }
    /* The official data sets of the game folder are the versions of 2001/02 (value = the option database). */
    static const char *const sets[] = {"3.9.60", "3.9.65"};
    for (int i = 0; i < 2; i++) {
        char path[1100];
        SeasonsEntry *s = &out->season[0];
        snprintf(path, sizeof path, "%s/Data-%s", game_dir, sets[i]);
        const SeasonKnownDb *db = seasons_db_known(sets[i]);
        if (db && dbfolders_valid(path) && s->extra_count < SEASONS_EXTRA_MAX) {
            SeasonsExtra *x = &s->extra[s->extra_count++];
            x->db = db;
            x->folder[0] = 0;
            x->data_set = 1;
            snprintf(x->value, sizeof x->value, "%s", sets[i]);
        }
    }
    static char names[DBFOLDER_MAX][DBFOLDER_NAME];     /* static: 16 KB, and the game runs this on one thread */
    int n = dbfolders_list(save_dir, names, DBFOLDER_MAX);
    for (int i = 0; i < n; i++) {
        char path[1100];
        if (!dbfolders_path(save_dir, names[i], path, sizeof path)) continue;
        const SeasonKnownDb *k = seasons_db_identify(path);
        if (!k) {
            if (out->custom_count < DBFOLDER_MAX) snprintf(out->custom[out->custom_count++], DBFOLDER_NAME, "%s", names[i]);
            continue;
        }
        if (k->role == SEASONS_ROLE_NOT_SEASON || k->role == SEASONS_ROLE_OFFICIAL_BASE || k->season < SEASONS_FIRST ||
            k->season >= SEASONS_FIRST + SEASONS_COUNT)
            continue;
        SeasonsEntry *s = &out->season[k->season - SEASONS_FIRST];
        if (k->role == SEASONS_ROLE_SEASON) {
            /* A folder with the neutral name wins over a copy under another name. */
            if (s->status != SEASONS_UNPACKED || !strcmp(names[i], k->name)) {
                s->status = SEASONS_UNPACKED;
                snprintf(s->folder, sizeof s->folder, "%s", names[i]);
            }
        } else if (s->extra_count < SEASONS_EXTRA_MAX) {
            int dup = 0;                          /* a copy of a data set that the game folder holds */
            for (int j = 0; j < s->extra_count; j++) dup |= s->extra[j].db == k;
            if (dup) continue;
            SeasonsExtra *x = &s->extra[s->extra_count++];
            x->db = k;
            x->data_set = 0;
            snprintf(x->folder, DBFOLDER_NAME, "%s", names[i]);
            snprintf(x->value, sizeof x->value, "folder:%s", names[i]);
        }
    }
    for (int i = 0; i < SEASONS_COUNT; i++) {
        SeasonsEntry *s = &out->season[i];
        char zp[1100];
        HostStat st;
        const SeasonKnownDb *db = s->database ? seasons_db_known(s->database) : NULL;
        if (s->status == SEASONS_MISSING && db && zip_path(game_dir, db, zp, sizeof zp) && !host_stat(zp, &st) && !st.is_dir)
            s->status = SEASONS_IN_DOWNLOAD;
    }
}

void seasons_db_scan(SeasonsList *out) { seasons_db_scan_in(win_game_dir, win_databases_root(), out); }

/* ---- First pick ---- */

static void say(SeasonsPrepare *r, const char *fmt, const char *a, const char *b)
{
    snprintf(r->error, sizeof r->error, fmt, a, b);
}

/* Removes a folder of plain files and the folder. A missing folder is fine. */
static void remove_flat_folder(const char *dir)
{
    char names[64][260];
    int n = 0;
    HostDir *d = host_opendir(dir);
    if (!d) return;
    const char *e;
    while ((e = host_readdir(d)) && n < 64) {
        if (!strcmp(e, ".") || !strcmp(e, "..")) continue;
        snprintf(names[n++], sizeof names[0], "%s", e);
    }
    host_closedir(d);
    for (int i = 0; i < n; i++) {
        char p[1400];
        snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        host_unlink(p);
    }
    host_rmdir(dir);
}

/* Removes the folders of base that nobody changed for an hour: what a run that stopped left. */
static void remove_stale_folders(const char *base)
{
    char names[32][260];
    int n = 0;
    HostDir *d = host_opendir(base);
    if (!d) return;
    const char *e;
    while ((e = host_readdir(d)) && n < 32)
        if (strcmp(e, ".") && strcmp(e, "..")) snprintf(names[n++], sizeof names[0], "%s", e);
    host_closedir(d);
    int64_t now = (int64_t)(host_unix_time_us() / 1000000u);
    for (int i = 0; i < n; i++) {
        char p[1400];
        HostStat st;
        snprintf(p, sizeof p, "%s/%s", base, names[i]);
        if (!host_stat(p, &st) && st.is_dir && st.mtime < now - 3600) remove_flat_folder(p);
    }
}

static int exists_dir(const char *path)
{
    HostStat st;
    return !host_stat(path, &st) && st.is_dir;
}

/* An entry name that is safe to take the file name from: no folder climbing, no drive, no backslash. */
static int safe_entry_name(const char *name, const char **base)
{
    if (name[0] == '/' || strchr(name, '\\') || strchr(name, ':')) return 0;
    for (const char *p = name; *p;) {
        const char *q = strchr(p, '/');
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len == 2 && p[0] == '.' && p[1] == '.') return 0;
        if (!q) break;
        p = q + 1;
    }
    const char *slash = strrchr(name, '/');
    *base = slash ? slash + 1 : name;
    return **base != 0;
}

/* Unpacks the zip into tmp (a new folder) and checks club.dat and staff.dat against the row. 0 on success,
 * else r->error is set. The caller removes tmp. */
static int unpack_zip(const char *zip, const char *tmp, const SeasonKnownDb *row, SeasonsPrepare *r)
{
    SeasonsZip z;
    char err[160] = "";
    if (seasons_zip_open(&z, zip, err, sizeof err)) {
        say(r, "Could not read %s: %s.", row->name, err);
        return -1;
    }
    int seen[FILE_COUNT] = {0}, done = 0;
    char club_hex[65] = "", staff_hex[65] = "";
    uint64_t club_size = 0, staff_size = 0;
    for (int i = 0; i < z.count; i++) {
        const SeasonsZipEntry *e = &z.e[i];
        const char *base;
        if (e->is_dir) continue;
        if (!safe_entry_name(e->name, &base)) {
            say(r, "The file for %s holds a name that is not allowed.", row->name, "");
            seasons_zip_close(&z);
            return -1;
        }
        int k;
        for (k = 0; k < FILE_COUNT; k++) if (!strcasecmp(base, data_files[k])) break;
        if (k == FILE_COUNT) continue;              /* a readme or a picture: not part of a database */
        if (seen[k]) {
            say(r, "The file for %s holds %s twice.", row->name, data_files[k]);
            seasons_zip_close(&z);
            return -1;
        }
        seen[k] = 1;
        char out[1400], hex[65];
        snprintf(out, sizeof out, "%s/%s", tmp, data_files[k]);
        if (seasons_zip_extract(&z, i, out, hex, err, sizeof err)) {
            say(r, "Could not unpack %s: %s.", row->name, err);
            seasons_zip_close(&z);
            return -1;
        }
        if (!strcmp(data_files[k], "club.dat")) { memcpy(club_hex, hex, 65); club_size = e->size; }
        if (!strcmp(data_files[k], "staff.dat")) { memcpy(staff_hex, hex, 65); staff_size = e->size; }
        done++;
        if (r->progress) r->progress(r->progress_ctx, done, FILE_COUNT);
    }
    seasons_zip_close(&z);
    for (int k = 0; k < FILE_COUNT; k++) {
        if (!seen[k]) {
            say(r, "The file for %s lacks %s.", row->name, data_files[k]);
            return -1;
        }
    }
    if (club_size != row->club_size || staff_size != row->staff_size || strcmp(club_hex, row->club_sha256) ||
        strcmp(staff_hex, row->staff_sha256)) {
        say(r, "The data in the file for %s is not the expected season database.", row->name, "");
        return -1;
    }
    return 0;
}

SeasonsPrepareStatus seasons_db_prepare_in(const char *game_dir, const char *save_dir, int year, SeasonsPrepare *r)
{
    void (*progress)(void *, int, int) = r->progress;
    void *progress_ctx = r->progress_ctx;
    memset(r, 0, sizeof *r);
    r->progress = progress;
    r->progress_ctx = progress_ctx;
    if (year < SEASONS_FIRST || year >= SEASONS_FIRST + SEASONS_COUNT) {
        r->status = SEASONS_PREPARE_NO_DATABASE;
        snprintf(r->error, sizeof r->error, "There is no such season.");
        return r->status;
    }
    if (year == SEASONS_FIRST) return r->status = SEASONS_PREPARE_OFFICIAL;
    const SeasonKnownDb *row = seasons_db_for_season(year);
    if (!row) {
        r->status = SEASONS_PREPARE_NO_DATABASE;
        snprintf(r->error, sizeof r->error, "No database exists for this season yet.");
        return r->status;
    }

    /* 1. A folder of Databases that holds this database. The exact name wins. */
    static char names[DBFOLDER_MAX][DBFOLDER_NAME];
    int n = dbfolders_list(save_dir, names, DBFOLDER_MAX), pick = -1;
    for (int i = 0; i < n; i++) {
        char path[1100];
        if (!dbfolders_path(save_dir, names[i], path, sizeof path) || !sizes_match(path, row) ||
            seasons_db_identify(path) != row)
            continue;
        if (pick < 0 || !strcmp(names[i], row->name)) pick = i;
    }
    if (pick >= 0) {
        snprintf(r->name, sizeof r->name, "%s", names[pick]);
        dbfolders_path(save_dir, r->name, r->path, sizeof r->path);
        return r->status = SEASONS_PREPARE_OK;
    }

    /* 2. Unpack the zip of the download, once, into a temporary folder, then rename it. */
    char zip[1100];
    HostStat st;
    if (!zip_path(game_dir, row, zip, sizeof zip) || host_stat(zip, &st) || st.is_dir) {
        r->status = SEASONS_PREPARE_NO_ZIP;
        say(r, "The game folder has no file for the season database %s. Install the game again.", row->name, "");
        return r->status;
    }
    char base[1100], tmp[1200], dbs[1100];
    snprintf(base, sizeof base, "%s/unpack-tmp", save_dir);
    /* Each run has its own temporary folder, so two runs of one season cannot mix files. */
    snprintf(tmp, sizeof tmp, "%s/%s.%lu-%llu", base, row->name, (unsigned long)getpid(),
             (unsigned long long)host_unix_time_us());
    snprintf(dbs, sizeof dbs, "%s/Databases", save_dir);
    r->status = SEASONS_PREPARE_FAILED;
    if (!exists_dir(base)) host_mkdir(base);
    remove_stale_folders(base);                   /* what an earlier run left, if the game stopped in it */
    if (host_mkdir(tmp)) {
        say(r, "Could not make a folder for %s in the save folder.", row->name, "");
        return r->status;
    }
    if (unpack_zip(zip, tmp, row, r)) {
        remove_flat_folder(tmp);
        host_rmdir(base);                         /* only when empty */
        return r->status;
    }
    if (!exists_dir(dbs)) host_mkdir(dbs);
    /* Claim a free name with mkdir, which fails when the name exists: a folder of the player is never replaced,
     * also not when another run takes the name at the same moment. The empty folder is then replaced by the
     * rename (on Windows the rename needs the target to be gone, so it is removed first). */
    char dest[1300];
    r->name[0] = 0;
    for (int k = 1; k < 10; k++) {
        char name[DBFOLDER_NAME];
        if (k == 1) snprintf(name, sizeof name, "%s", row->name);
        else snprintf(name, sizeof name, "%s (%d)", row->name, k);
        snprintf(dest, sizeof dest, "%s/%s", dbs, name);
        if (!host_mkdir(dest)) { snprintf(r->name, sizeof r->name, "%s", name); break; }
    }
    int moved = 0;
    if (r->name[0]) {
#ifdef _WIN32
        host_rmdir(dest);
#endif
        moved = !host_rename(tmp, dest);
        if (!moved) host_rmdir(dest);
    }
    if (!moved) {
        remove_flat_folder(tmp);
        host_rmdir(base);
        r->name[0] = 0;
        say(r, "Could not move the unpacked season database %s into the Databases folder.", row->name, "");
        return r->status;
    }
    host_rmdir(base);                             /* only when empty */
    snprintf(r->path, sizeof r->path, "%s", dest);
    r->unpacked = 1;
    return r->status = SEASONS_PREPARE_OK;
}

SeasonsPrepareStatus seasons_db_prepare(int start_year, SeasonsPrepare *r)
{
    return seasons_db_prepare_in(win_game_dir, win_databases_root(), start_year, r);
}
