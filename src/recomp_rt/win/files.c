/* KERNEL32 files and folders over the host file system.
 *
 * Paths: on macOS C:\ is the host root, so the original's folder /Users/max/Projects/cm0102/clean-3968 is
 * C:\Users\max\Projects\cm0102\clean-3968. On Windows each drive is the host drive of that letter. The CD drive
 * (D:\ on macOS, a letter that Windows does not use there, host_cd_drive) is the folder that holds the
 * player's own cm0102.exe, which the CD check looks for (inventory 4.6). A relative path starts at
 * the current folder, which WinMain sets from the registry value Location. Each part of a path is
 * matched without regard to case, as Windows does; a part that does not exist keeps its spelling
 * (a new file).
 *
 * Writes: the game folder is never changed. A path has two host forms: H in the game folder and U in the
 * user folder (win_user_dir) under the same relative path. A read looks at U, then at H. A file opened
 * for writing, created or renamed to lives in U (a file that exists only in H is copied to U first when
 * it is opened for update). A folder is created in U. A delete or rename acts on U only. A file search
 * lists U and H, U winning on equal names. A path outside the game folder maps to U/_outside/<path>.
 *
 * Saved games: when the option save_folder names a folder, a file *.sav in the game folder maps to that
 * folder (save_dir) and not to U, and a search of the game folder lists the *.sav files of that folder
 * only. The game writes temp_save_file.sav and renames it to the name of the player, so both stay in
 * the same folder. Other files (settings.txt, game.cfg) stay in U. The runtime changes a save of U only
 * for the option save_backups (save_backup below): it keeps a copy of a save that the game writes over.
 * A Delete of a save without a new save, as the Delete button of the load list makes, deletes the
 * file and makes no backup. */
#include "win.h"
#include "host.h"
#include "options.h"
#include "dbfolders.h"
#include "cp1252.h"
#include "folder_names.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

char win_cwd[1024] = "C:\\";
char win_user_dir[1024];

static int exists(const char *p) { HostStat st; return host_stat(p, &st) == 0; }
static int is_dir(const char *p) { HostStat st; return host_stat(p, &st) == 0 && st.is_dir; }

/* The spelling of a name in a folder, ignoring case: found = the entry, or empty (folder_names.c). A
 * host that ignores case needs no lookup. */
static void ci_lookup(const char *dir, const char *part, char *found, size_t room)
{
    found[0] = 0;
    if (HOST_FOLDS_CASE) return;
    folder_lookup(dir[0] ? dir : "/", part, found, room);
}

/* The start of a host path is the game folder (the whole path or the part before a '/'). */
static int in_game_dir(const char *host, size_t gl)
{
    int same = HOST_FOLDS_CASE ? !strncasecmp(host, win_game_dir, gl) : !strncmp(host, win_game_dir, gl);
    return same && (host[gl] == '/' || host[gl] == 0);
}

/* The folder for saved games from the option save_folder, or NULL for the normal folder. Resolved at the
 * first call, after options_load. A leading ~ is the home folder. The folder is created when it is
 * missing. When that fails, a message goes to stderr and the normal folder stays in use. */
static void make_parents(const char *path);
static const char *save_dir(void)
{
    static int done;
    static char dir[1100];
    if (done) return dir[0] ? dir : NULL;
    done = 1;
    const char *opt = options_string(OPT_SAVE_FOLDER);
    if (!*opt) return NULL;
    char path[1100];
    if (opt[0] == '~' && (!opt[1] || opt[1] == '/' || opt[1] == '\\')) {
        char home[1024];
        if (!host_home_dir(home, sizeof home)) {
            fprintf(stderr, "cm0102_recomp: option save_folder: the home folder is unknown, "
                            "so the saved games stay in %s.\n", win_user_dir);
            return NULL;
        }
        snprintf(path, sizeof path, "%s%s", home, opt + 1);
    } else {
        snprintf(path, sizeof path, "%s", opt);
    }
    if (HOST_FOLDS_CASE) for (char *c = path; *c; c++) if (*c == '\\') *c = '/';
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = 0;
    if (!is_dir(path)) {
        char tmp[1100];
        snprintf(tmp, sizeof tmp, "%s/x", path);
        make_parents(tmp);
        host_mkdir(path);
    }
    if (!is_dir(path)) {
        fprintf(stderr, "cm0102_recomp: option save_folder: cannot create the folder %s, "
                        "so the saved games stay in %s.\n", path, win_user_dir);
        return NULL;
    }
    snprintf(dir, sizeof dir, "%s", path);
    return dir;
}

/* The folder that holds Databases (custom data sets, dbfolders.h): the save folder. */
const char *win_databases_root(void)
{
    const char *sd = save_dir();
    return sd ? sd : win_user_dir;
}

/* The folder of the saved games: the option save_folder, else the user folder. Printouts go in a
 * folder below it (print_pdf.c). */
const char *win_save_folder(void)
{
    const char *d = save_dir();
    return d ? d : win_user_dir;
}

static int has_sav_ending(const char *name)
{
    size_t n = strlen(name);
    return n >= 4 && !strcasecmp(name + n - 4, ".sav");
}

/* The user-folder form U of a host path H: the part below the game folder, else _outside/<H>. Names that
 * exist in U keep their spelling there. */
static int user_form(const char *host, char *out, size_t room)
{
    size_t gl = strlen(win_game_dir);
    const char *rel;
    char base[1100];
    snprintf(base, sizeof base, "%s", win_user_dir);
    if (in_game_dir(host, gl)) {
        rel = host + gl;
        const char *sd = save_dir();
        if (sd && rel[0] == '/' && !strchr(rel + 1, '/') && has_sav_ending(rel + 1))
            snprintf(base, sizeof base, "%s", sd);      /* a saved game */
    } else {
        snprintf(base + strlen(base), sizeof base - strlen(base), "/_outside");
        rel = host;
    }
    char copy[2048];
    snprintf(copy, sizeof copy, "%s", rel);
    /* A drive letter becomes a folder name: C:/Windows is _outside/C/Windows. */
    for (char *c = copy; HOST_FOLDS_CASE && *c; c++) if (*c == ':') memmove(c, c + 1, strlen(c));
    size_t len = (size_t)snprintf(out, room, "%s", base);
    char *save = NULL;
    for (char *part = strtok_r(copy, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        char found[256];
        ci_lookup(out, part, found, sizeof found);
        len += (size_t)snprintf(out + len, room - len, "/%s", found[0] ? found : part);
        if (len >= room) return 0;
    }
    return 1;
}

/* Creates every folder above the last part of a path. */
static void make_parents(const char *path)
{
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        host_mkdir(buf);
        *p = '/';
    }
}

static int copy_file(const char *from, const char *to)
{
    int in = host_open(from, O_RDONLY, 0);
    if (in < 0) return -1;
    make_parents(to);
    int out = host_open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { host_close(in); return -1; }
    char buf[65536];
    long r;
    int ok = 0;
    while ((r = host_read(in, buf, sizeof buf)) > 0) {
        if (host_write(out, buf, (uint32_t)r) != r) { ok = -1; break; }
    }
    if (r < 0) ok = -1;
    host_close(in);
    host_close(out);
    return ok;
}

/* Save backups (option save_backups). Before a new save takes the place of an old one, the old file goes to
 * backups/<name>.1.sav in its folder. The game does this in two calls: it deletes the old save, then renames
 * temp_save_file.sav to the name (trace of a save over samesave.sav). So the delete makes a backup only when
 * temp_save_file.sav lies beside the save (temp_save_beside). The Delete button of the load list has no
 * new save and no backup. backups/<name>.2.sav and .3.sav are the older copies,
 * and the copy older than .3 is deleted. The file temp_save_file.sav, which the game writes first and then
 * renames, gets no backups. Only a file in backups/ is ever deleted here: when a step fails, a message goes
 * to stderr and the game goes on as without the option. h is the host form of the file in the game folder,
 * u its user form (the save folder or the user folder). copy = 1 copies the save and keeps it in place.
 * The result is 1 when the backup is made. */
#define SAVE_BACKUPS 3

/* 1 when temp_save_file.sav lies in the folder of the user form u of a save. */
static int temp_save_beside(const char *u)
{
    const char *slash = strrchr(u, '/');
    char temp[2048];
    if (!slash) return 0;
    snprintf(temp, sizeof temp, "%.*s/temp_save_file.sav", (int)(slash - u), u);
    return exists(temp);
}
static int save_backup(const char *h, const char *u, int copy)
{
    size_t gl = strlen(win_game_dir);
    if (!options_bool(OPT_SAVE_BACKUPS) || !in_game_dir(h, gl)) return 0;
    const char *rel = h + gl;
    if (rel[0] != '/' || strchr(rel + 1, '/') || !has_sav_ending(rel + 1)) return 0;
    if (!strcasecmp(rel + 1, "temp_save_file.sav") || !exists(u) || is_dir(u)) return 0;
    const char *slash = strrchr(u, '/');
    char dir[2048], stem[512], from[2048], to[2048];
    snprintf(dir, sizeof dir, "%.*s/backups", (int)(slash - u), u);
    snprintf(stem, sizeof stem, "%.*s", (int)strlen(slash + 1) - 4, slash + 1);
    if (!is_dir(dir)) host_mkdir(dir);
    if (!is_dir(dir)) {
        fprintf(stderr, "cm0102_recomp: option save_backups: cannot create the folder %s.\n", dir);
        return 0;
    }
    snprintf(to, sizeof to, "%s/%s.%d.sav", dir, stem, SAVE_BACKUPS);
    if (exists(to) && host_unlink(to)) goto fail;
    for (int i = SAVE_BACKUPS - 1; i >= 1; i--) {
        snprintf(from, sizeof from, "%s/%s.%d.sav", dir, stem, i);
        snprintf(to, sizeof to, "%s/%s.%d.sav", dir, stem, i + 1);
        if (exists(from) && host_rename(from, to)) goto fail;
    }
    snprintf(to, sizeof to, "%s/%s.1.sav", dir, stem);
    if (copy ? copy_file(u, to) : host_rename(u, to)) goto fail;
    WIN_TRACE("save backup %s -> %s\n", u, to);
    return 1;
fail:
    fprintf(stderr, "cm0102_recomp: option save_backups: cannot make a backup of %s (%s).\n", u, strerror(errno));
    return 0;
}

void win_error_from_errno(void)
{
    switch (errno) {
    case ENOENT: win_last_error = ERROR_FILE_NOT_FOUND; break;
    case ENOTDIR: win_last_error = ERROR_PATH_NOT_FOUND; break;
    case EEXIST: win_last_error = ERROR_FILE_EXISTS; break;
    case EACCES: case EPERM: case EISDIR: win_last_error = ERROR_ACCESS_DENIED; break;
    case ENOSPC: win_last_error = 112; break;    /* ERROR_DISK_FULL */
    default: win_last_error = 31; break;          /* ERROR_GEN_FAILURE */
    }
}

/* The full Windows path of a name: drive letter, backslashes, no "." or "..", no trailing backslash
 * except after the drive. */
static void full_path(const char *in, char *out, size_t room)
{
    char buf[2048];
    char drive;
    const char *rest;
    if (isalpha((unsigned char)in[0]) && in[1] == ':') {
        drive = (char)toupper((unsigned char)in[0]);
        rest = in + 2;
        if (rest[0] != '\\' && rest[0] != '/') {
            /* A drive-relative path: relative to the current folder when on its drive, else to the root. */
            if (toupper((unsigned char)win_cwd[0]) == drive) snprintf(buf, sizeof buf, "%s\\%s", win_cwd + 2, rest);
            else snprintf(buf, sizeof buf, "\\%s", rest);
        } else {
            snprintf(buf, sizeof buf, "%s", rest);
        }
    } else if (in[0] == '\\' || in[0] == '/') {
        drive = (char)toupper((unsigned char)win_cwd[0]);
        snprintf(buf, sizeof buf, "%s", in);
    } else {
        drive = (char)toupper((unsigned char)win_cwd[0]);
        snprintf(buf, sizeof buf, "%s\\%s", win_cwd + 2, in);
    }
    /* Split into parts and resolve . and .. */
    char *parts[256];
    int n = 0;
    for (char *p = buf; *p; p++) if (*p == '/') *p = '\\';
    char *save = NULL;
    for (char *tok = strtok_r(buf, "\\", &save); tok; tok = strtok_r(NULL, "\\", &save)) {
        if (!strcmp(tok, ".")) continue;
        if (!strcmp(tok, "..")) { if (n) n--; continue; }
        if (n < 256) parts[n++] = tok;
    }
    size_t len = (size_t)snprintf(out, room, "%c:", drive);
    if (!n) { snprintf(out + len, room - len, "\\"); return; }
    for (int i = 0; i < n && len < room; i++) len += (size_t)snprintf(out + len, room - len, "\\%s", parts[i]);
}

/* The host folder of a drive's root, or NULL for a drive that does not exist. On macOS C: is the
 * host root (""); on Windows a drive is the host drive ("E:"). */
static const char *drive_root(char drive, char *buf, size_t room)
{
    if (drive == host_cd_drive()) {
        snprintf(buf, room, "%s", win_exe_path);
        char *slash = strrchr(buf, '/');
        if (slash) *slash = 0;
        return buf;
    }
    if (HOST_FOLDS_CASE && host_has_drive(drive)) { snprintf(buf, room, "%c:", drive); return buf; }
    if (!HOST_FOLDS_CASE && drive == 'C') return "";
    return NULL;
}

/* The game's names are in code page 1252 ("Copa Am\xe9rica"). A host file name is UTF-8, and macOS refuses a
 * name that is not (0.6.0: a league writes "<long name>_<id>.tmp", League::save_to_temp_file 0x6891c0 at
 * 0x689234, and stops at league.cpp 5095 when the open fails). So win_host_path gives a part that is not
 * UTF-8 to cp1252_to_utf8 (cp1252.h). */

int win_host_path(const char *winpath, char *out, size_t room)
{
    char full[2048], rootbuf[1024];
    full_path(winpath, full, sizeof full);
    const char *root = drive_root(full[0], rootbuf, sizeof rootbuf);
    if (!root) return 0;
    size_t len = (size_t)snprintf(out, room, "%s", root);
    char *save = NULL;
    for (char *part = strtok_r(full + 2, "\\", &save); part; part = strtok_r(NULL, "\\", &save)) {
        /* Windows drops trailing dots and spaces of a name. */
        size_t pl = strlen(part);
        while (pl > 1 && (part[pl - 1] == '.' || part[pl - 1] == ' ')) part[--pl] = 0;
        char found[256], utf8[1024];
        ci_lookup(len ? out : "", part, found, sizeof found);
        /* On Linux a file can have a name that is not UTF-8 (written by an older version); the lookup above
         * finds it first. macOS has no such files. */
        if (!found[0] && !utf8_ok(part)) {
            cp1252_to_utf8(part, utf8, sizeof utf8);
            part = utf8;
            ci_lookup(len ? out : "", part, found, sizeof found);
        }
        len += (size_t)snprintf(out + len, room - len, "/%s", found[0] ? found : part);
        if (len >= room) return 0;
    }
    if (!len) snprintf(out, room, "/");
    else if (len == 2 && out[1] == ':') snprintf(out + len, room - len, "/");   /* a Windows drive root */
    return 1;
}

void win_windows_path(const char *host, char *out, size_t room)
{
    /* A Windows host path starts with its drive; a macOS one lies on C:. */
    int has_drive = isalpha((unsigned char)host[0]) && host[1] == ':';
    size_t len = (size_t)snprintf(out, room, "%s%s", has_drive ? "" : "C:", host);
    for (size_t i = 0; i < len && i < room; i++) if (out[i] == '/') out[i] = '\\';
    if (len == 2 && room > 3) snprintf(out + 2, room - 2, "\\");
}


/* The data set (#39): a folder of other database files. A file Data/<name> of the game folder that also
 * exists in that folder is read from there. Every other file and every write is unchanged.
 * - The option database chooses the folder: 3.9.68 is the clean files' own Data, 3.9.60 and 3.9.65 are
 *   the folders Data-3.9.60 and Data-3.9.65 of the game folder (tools/clean_files/make_clean_files.py,
 *   DATA_SET_FOLDERS). The Setup Game screen sets the option (database.c).
 * - The value folder:<name> is a folder of Databases in the save folder (dbfolders.h).
 * - The test switch RECOMP_DATA_DIR names any folder, for example a 3.9.60 data set that
 *   make_clean_files.py --data-3960 made. It wins over the option.
 * A missing folder gets a message, and the game reads its own Data.
 * Pictures (.rgn) always come from the game's own Data (Q205): an Update Team folder has its own intro
 * pictures, for example its logo.RGN and eidos.RGN. */
static int data_dir_known;
static char data_dir[1024];

void win_data_set_changed(void) { data_dir_known = 0; }

static void data_dir_find(void)
{
    data_dir_known = 1;
    data_dir[0] = 0;
    const char *v = getenv("RECOMP_DATA_DIR");
    if (v && *v) {
        if (!host_realpath(v, data_dir, sizeof data_dir)) {
            fprintf(stderr, "cm0102_recomp: RECOMP_DATA_DIR: no folder %s\n", v);
            data_dir[0] = 0;
        }
        return;
    }
    int set = options_enum(OPT_DATABASE);
    if (set == DATABASE_3968) return;
    if (set == DATABASE_CUSTOM) {
        const char *folder = options_string(OPT_DATABASE) + 7;
        char found_path[1100];
        if (!dbfolders_path(win_databases_root(), folder, found_path, sizeof found_path) ||
            !host_realpath(found_path, data_dir, sizeof data_dir)) {
            fprintf(stderr, "cm0102_recomp: database = folder:%s: no usable folder in %s/Databases, "
                            "so the game reads its own Data\n", folder, win_databases_root());
            data_dir[0] = 0;
        }
        return;
    }
    char name[64], found[256], path[2048];
    snprintf(name, sizeof name, "Data-%s", options_string(OPT_DATABASE));
    ci_lookup(win_game_dir, name, found, sizeof found);
    snprintf(path, sizeof path, "%s/%s", win_game_dir, found[0] ? found : name);
    if (!host_realpath(path, data_dir, sizeof data_dir)) {
        fprintf(stderr, "cm0102_recomp: database = %s: no folder %s, so the game reads its own Data\n",
                options_string(OPT_DATABASE), path);
        data_dir[0] = 0;
    }
}

/* The host folder of the data set that a load reads, or "" for the game's own Data (season_dates.c). */
const char *win_data_set_dir(void)
{
    if (!data_dir_known) data_dir_find();
    return data_dir;
}

/* 1 when the data set folder holds its own copy of the Data file name (0.6.2 open item 100: euro.cfg). */
int win_data_set_has(const char *name)
{
    if (!data_dir_known) data_dir_find();
    if (!data_dir[0]) return 0;
    char found[256], path[2048];
    ci_lookup(data_dir, name, found, sizeof found);
    snprintf(path, sizeof path, "%s/%s", data_dir, found[0] ? found : name);
    return exists(path);
}

static void data_dir_swap(char *h, size_t room)
{
    if (!data_dir_known) data_dir_find();
    if (!data_dir[0]) return;
    size_t gl = strlen(win_game_dir);
    if (!in_game_dir(h, gl) || strncasecmp(h + gl, "/Data/", 6)) return;
    const char *name = h + gl + 6;
    if (!*name || strchr(name, '/')) return;
    size_t nl = strlen(name);
    if (nl > 4 && !strcasecmp(name + nl - 4, ".rgn")) return;
    char found[256], alt[2048];
    ci_lookup(data_dir, name, found, sizeof found);
    snprintf(alt, sizeof alt, "%s/%s", data_dir, found[0] ? found : name);
    if (exists(alt) && strlen(alt) < room) snprintf(h, room, "%s", alt);
}

/* The intro logo (Q206 to Q208): the game shows Data/logo.rgn, the Sports Interactive logo, first in its
 * intro. When the game folder has Data/bosman_logo.rgn (tools/release/make_intro_logo.py, in the
 * downloads only), the game reads that file in its place. Both pictures are 520x520, so the original
 * allocates the same memory. Without the file the game shows its own logo. */
static void intro_logo_swap(char *h, size_t room)
{
    size_t gl = strlen(win_game_dir);
    if (!in_game_dir(h, gl) || strncasecmp(h + gl, "/Data/", 6) || strcasecmp(h + gl + 6, "logo.rgn")) return;
    char dir[1100], found[256], alt[2048];
    snprintf(dir, sizeof dir, "%.*s", (int)(gl + 5), h);
    ci_lookup(dir, "bosman_logo.rgn", found, sizeof found);
    snprintf(alt, sizeof alt, "%s/%s", dir, found[0] ? found : "bosman_logo.rgn");
    if (exists(alt) && strlen(alt) < room) snprintf(h, room, "%s", alt);
}

/* Both host forms of a Windows path: H in the game folder, U in the user folder. */
static int win_paths(const char *name, char *h, char *u, size_t room)
{
    if (!win_host_path(name, h, room) || !user_form(h, u, room)) return 0;
    data_dir_swap(h, room);
    intro_logo_swap(h, room);
    return 1;
}

/* The host path that a read of the Windows path name opens: the user form U when it exists, else the game
 * folder form H. 0 when neither exists. The season part of a save (season.c) uses it. */
int win_existing_file(const char *name, char *out, size_t room)
{
    char h[2048], u[2048];
    if (!win_paths(name, h, u, sizeof h)) return 0;
    const char *p = exists(u) ? u : exists(h) ? h : NULL;
    if (!p || is_dir(p) || strlen(p) >= room) return 0;
    snprintf(out, room, "%s", p);
    return 1;
}

/* Handles. Files are 0x100, 0x104, ...; finds are 0x20000, 0x20004, ... */
#define MAX_FILES 256
#define FILE_BASE 0x100u
static int fds[MAX_FILES];
static int fd_used[MAX_FILES];

/* Saves above 2 GB. The save container keeps the offset of each part in an unsigned 32-bit word (savfile.py),
 * and the game's C library passes file positions as signed 32-bit values. The seek and tell sites take them as
 * unsigned (tools/recomp/save_size.py, SetFilePointer below), so positions reach FILE_LIMIT. A write that
 * would take a file past WRITE_LIMIT writes nothing and fails as on a full disk. WRITE_LIMIT leaves room for the
 * season part that season.c adds after the save (at most 2 MB stored), so that part always fits. During a save (win_save_begin to
 * win_save_end) such a write makes the save fail: the game cannot delete or rename a .sav file, so the earlier
 * save stays as it was, and win_save_end deletes the file that grew too large. A file below 2 GB takes the same
 * path as before. */
#define FILE_LIMIT 0xfffffffeu     /* 0xffffffff is the error result of SetFilePointer and ftell */
#define WRITE_LIMIT (FILE_LIMIT - 0x200000u)
static char *fd_path[MAX_FILES];   /* the host path of a handle that may write, or NULL */
static int in_save;
static char too_big_path[2048];    /* the file of the first refused write of this save, or "" */
static int too_big_told;           /* 1 when stderr has the first refused write (once per save) */
static int save_deleted;           /* 1 when this save deleted a .sav file before a refused write */

static int fd_of(uint32_t h)
{
    if (h < FILE_BASE || h >= FILE_BASE + 4 * MAX_FILES || (h & 3)) return -1;
    uint32_t i = (h - FILE_BASE) / 4;
    return fd_used[i] ? fds[i] : -1;
}

/* Memory files (0.6.0 slice 15b): a handle whose fds entry is MEM_FD reads mem_file[mem_of[i]] at mem_pos[i]. */
#define MEM_FD (-2)
#define MAX_MEM_FILES 32
typedef struct { char path[1100]; const uint8_t *data; size_t size; } MemFile;
static MemFile mem_file[MAX_MEM_FILES];
static int mem_files;
static int mem_of[MAX_FILES];
static size_t mem_pos[MAX_FILES];

static void keep_drop(const char *path);

void win_memory_file(const char *host_path, const uint8_t *data, size_t size)
{
    keep_drop(NULL);                        /* item 151: a kept read handle must not hide a new memory file */
    if (mem_files == MAX_MEM_FILES || strlen(host_path) >= sizeof mem_file[0].path) rt_fatal("too many memory files");
    snprintf(mem_file[mem_files].path, sizeof mem_file[0].path, "%s", host_path);
    mem_file[mem_files].data = data;
    mem_file[mem_files].size = size;
    mem_files++;
}

void win_memory_files_clear(void)
{
    keep_drop(NULL);
    for (uint32_t i = 0; i < MAX_FILES; i++)
        if (fd_used[i] && fds[i] == MEM_FD) fd_used[i] = 0;
    mem_files = 0;
}

static int mem_find(const char *host)
{
    for (int m = 0; m < mem_files; m++)
        if (!strcasecmp(mem_file[m].path, host)) return m;
    return -1;
}

/* The slot of a memory file handle, or -1. */
static int mem_slot(uint32_t h)
{
    if (h < FILE_BASE || h >= FILE_BASE + 4 * MAX_FILES || (h & 3)) return -1;
    uint32_t i = (h - FILE_BASE) / 4;
    return fd_used[i] && fds[i] == MEM_FD ? (int)i : -1;
}

int win_is_file_handle(uint32_t h) { return fd_of(h) >= 0 || mem_slot(h) >= 0; }

/* Kept files. At the season change PlayerStats::end_season_for_key_nation 0x7c1350 archives the records
 * of each player. archive_player_stats 0x7c5fe0 calls FILE_LLIST::add_node 0x58e340 for each record. add_node
 * opens "player stats history.tmp" for read and write (0x58e5c1), appends, and closes it. These opens and
 * closes took about 5 % of a Tier A year (the profile of 0.6.2 slice 3). So while 0x7c1350 runs, a closed
 * handle of that file, opened with OPEN_EXISTING for read and write, keeps its host file open. The next
 * CreateFileA of the same host path with the same flags takes it at position 0, as a new open does. It is one
 * host file, read and written by the same calls, so the bytes are the same. A delete or rename of the path,
 * an unwind out of 0x7c1350 and its end close the kept file. RECOMP_KEEP_FILES=0 closes it at once.
 *
 * Item 151: the constructor PlayerStats::PlayerStats 0x7bfd50 ("Player histories" of a new game) also keeps the
 * file. Through FILE_LLIST::add_node_list 0x58e9b0 it opens the file 16,816 times in a 2025/26 start with four
 * nations, each time for write with OPEN_ALWAYS (access 0x40000000, disposition 4), and the opens with their
 * path lookups took about half of the step. A handle opened for write with OPEN_EXISTING or OPEN_ALWAYS keeps
 * its host file at the close, with the game's name, access and disposition. A new CreateFileA with the same
 * name, access and disposition takes the kept file at position 0 without the path lookup: the file exists
 * (the game closed it a moment ago), so the slow path would find the same user-folder file, set no flag of
 * a new file, and give ERROR_ALREADY_EXISTS for OPEN_ALWAYS. Every DeleteFileA, MoveFileA, CreateDirectoryA and
 * SetCurrentDirectoryA closes all kept files, since a path lookup may then give another file. A handle for read
 * is kept too (the last "Initialising game data", FOG_OF_WAR_MAP 0x5aff00), but only a file of the user folder:
 * a read of a file that exists only in the game folder would find the user-folder copy once a write makes one.
 * A new or cleared memory file closes the kept files too, since a read open would then take the memory file. */
#define KEEP_NAME "player stats history.tmp"
#define KEEP_MAX 4
static struct { char *path; int fd; int flags; char *name; uint32_t access, disposition; } kept[KEEP_MAX];
static char *fd_keep[MAX_FILES];              /* the host path of a handle of KEEP_NAME in the user folder */
static char *fd_name[MAX_FILES];              /* the game's name, access and disposition of that handle */
static uint32_t fd_access[MAX_FILES], fd_disposition[MAX_FILES];
static int keep_depth;
static uint32_t keep_esp;           /* item 151: the esp at the start of the outermost scope */

/* A scope runs, and esp is in it (an unwind can run an older frame's code while the scope's wrap waits). */
static int keep_live(void) { return keep_depth && cpu.esp < keep_esp; }
static int fd_flags[MAX_FILES];     /* the host open flags of a handle */

static int keep_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_KEEP_FILES");
        on = !(v && !strcmp(v, "0"));
    }
    return on;
}

/* 1 for a host path whose name is KEEP_NAME. */
static int kept_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return !strcasecmp(slash ? slash + 1 : path, KEEP_NAME);
}

/* Closes the kept file of path, or every kept file when path is NULL. */
static void keep_drop(const char *path)
{
    for (int k = 0; k < KEEP_MAX; k++) {
        if (!kept[k].path || (path && (HOST_FOLDS_CASE ? strcasecmp(kept[k].path, path) : strcmp(kept[k].path, path))))
            continue;
        host_close(kept[k].fd);
        free(kept[k].path);
        free(kept[k].name);
        kept[k].path = kept[k].name = NULL;
    }
}

/* A kept file for the game's name, access and disposition: its slot, or -1. */
static int keep_find_name(const char *name, uint32_t access, uint32_t disposition)
{
    for (int k = 0; k < KEEP_MAX; k++)
        if (kept[k].path && kept[k].name && kept[k].access == access && kept[k].disposition == disposition
            && !strcmp(kept[k].name, name))
            return k;
    return -1;
}

/* A kept host file for path and flags, at position 0, or -1. */
static int keep_take(const char *path, int flags)
{
    for (int k = 0; k < KEEP_MAX; k++) {
        if (!kept[k].path || kept[k].flags != flags || strcmp(kept[k].path, path)) continue;
        int fd = kept[k].fd;
        free(kept[k].path);
        free(kept[k].name);
        kept[k].path = kept[k].name = NULL;
        if (host_seek(fd, 0, SEEK_SET) == 0) return fd;
        host_close(fd);
        return -1;
    }
    return -1;
}

void o_007c1350(void);

/* PlayerStats::end_season_for_key_nation 0x7c1350, thiscall (key nation), ret 4. */
RT_WRAP(007c1350, keep_files_archive)
{
    if (!keep_depth++) keep_esp = cpu.esp;
    o_007c1350();
    if (--keep_depth == 0 || rt_unwinding) {
        keep_depth = 0;
        keep_drop(NULL);
    }
}

void o_007bfd50(void);

void o_005aff00(void);

/* FOG_OF_WAR_MAP::FOG_OF_WAR_MAP 0x5aff00 (item 151): the last "Initialising game data" of a new game reads the
 * history of each player through PlayerStats::build_career_history 0x7c3e40 and FILE_LLIST::get_nodes 0x58f120,
 * which opens the file for read (access 0x80000000, OPEN_EXISTING) 14,706 times in a 2025/26 start with four
 * nations. */
RT_WRAP(005aff00, keep_files_fog)
{
    if (!keep_depth++) keep_esp = cpu.esp;
    o_005aff00();
    if (--keep_depth == 0 || rt_unwinding) {
        keep_depth = 0;
        keep_drop(NULL);
    }
}

/* PlayerStats::PlayerStats 0x7bfd50, thiscall, the "Player histories" step of a new game (item 151). */
RT_WRAP(007bfd50, keep_files_histories)
{
    if (!keep_depth++) keep_esp = cpu.esp;
    o_007bfd50();
    if (--keep_depth == 0 || rt_unwinding) {
        keep_depth = 0;
        keep_drop(NULL);
    }
}

int win_close_file(uint32_t h)
{
    int m = mem_slot(h);
    if (m >= 0) {
        fd_used[m] = 0;
        return 1;
    }
    int fd = fd_of(h);
    if (fd < 0) return 0;
    uint32_t i = (h - FILE_BASE) / 4;
    int k = KEEP_MAX;
    if (keep_live() && keep_on() && fd_keep[i] && !(fd_flags[i] & (O_TRUNC | O_EXCL)))
        for (k = 0; k < KEEP_MAX && kept[k].path; k++) {}
    if (k < KEEP_MAX) {
        kept[k].path = fd_keep[i];          /* the path and the game's name move to the kept file */
        kept[k].fd = fd;
        kept[k].flags = fd_flags[i];
        kept[k].name = fd_name[i];
        kept[k].access = fd_access[i];
        kept[k].disposition = fd_disposition[i];
        fd_keep[i] = NULL;
        fd_name[i] = NULL;
    } else
        host_close(fd);
    free(fd_path[i]);
    fd_path[i] = NULL;
    free(fd_keep[i]);
    fd_keep[i] = NULL;
    free(fd_name[i]);
    fd_name[i] = NULL;
    fd_used[i] = 0;
    return 1;
}

#define INVALID_HANDLE 0xffffffffu
#define STD_HANDLE_BASE 0xF0000000u

static int is_std(uint32_t h) { return h >= STD_HANDLE_BASE + 10 && h <= STD_HANDLE_BASE + 12; }

/* CreateFileA(name, access, share, security, disposition, flags, template) */
RT_IMPORT(CreateFileA)
{
    const char *name = win_str(rt_arg(0));
    uint32_t access = rt_arg(1), disposition = rt_arg(4);
    char h[2048], u[2048];
    int kk = keep_live() && keep_on() && (disposition == 3 || disposition == 4)
             ? keep_find_name(name, access, disposition) : -1;
    if (kk >= 0) {                                      /* the kept file, as the path lookup would find it */
        for (uint32_t i = 0; i < MAX_FILES; i++) {
            if (fd_used[i]) continue;
            int fd = kept[kk].fd;
            if (host_seek(fd, 0, SEEK_SET) != 0) break;    /* the slow path below opens it again */
            fd_used[i] = 1;
            fds[i] = fd;
            fd_flags[i] = kept[kk].flags;
            fd_path[i] = (access & 0x40000000u) ? strdup(kept[kk].path) : NULL;
            fd_keep[i] = kept[kk].path;
            fd_name[i] = kept[kk].name;
            fd_access[i] = access;
            fd_disposition[i] = disposition;
            kept[kk].path = kept[kk].name = NULL;
            win_last_error = disposition == 4 ? ERROR_ALREADY_EXISTS : 0;
            WIN_TRACE("CreateFileA(%s, %#x, %u) = %d (kept)\n", name, access, disposition, fd);
            rt_ret_stdcall(FILE_BASE + 4 * i, 7);
            return;
        }
        keep_drop(NULL);
    }
    if (!win_paths(name, h, u, sizeof h)) {
        win_last_error = ERROR_PATH_NOT_FOUND;
        rt_ret_stdcall(INVALID_HANDLE, 7);
        return;
    }
    int flags = (access & 0x40000000u) ? ((access & 0x80000000u) ? O_RDWR : O_WRONLY) : O_RDONLY;
    switch (disposition) {
    case 1: flags |= O_CREAT | O_EXCL; break;           /* CREATE_NEW */
    case 2: flags |= O_CREAT | O_TRUNC; break;          /* CREATE_ALWAYS */
    case 3: break;                                      /* OPEN_EXISTING */
    case 4: flags |= O_CREAT; break;                    /* OPEN_ALWAYS */
    case 5: flags |= O_TRUNC; break;                    /* TRUNCATE_EXISTING */
    }
    int in_u = exists(u), in_h = exists(h);
    int existed = in_u || in_h;
    int write_intent = (access & 0x40000000u) || disposition == 1 || disposition == 2 || disposition == 5;
    if (disposition == 4 && !existed) write_intent = 1;
    const char *target = in_u || !write_intent ? (in_u ? u : h) : u;
    if (existed && is_dir(in_u ? u : h)) {
        win_last_error = ERROR_ACCESS_DENIED;
        rt_ret_stdcall(INVALID_HANDLE, 7);
        return;
    }
    if (disposition == 1 && existed) {
        win_last_error = ERROR_FILE_EXISTS;
        rt_ret_stdcall(INVALID_HANDLE, 7);
        return;
    }
    if (write_intent) {
        target = u;
        if (!in_u && in_h && disposition != 2 && disposition != 5) {
            /* Update of a file of the game folder: work on a copy in the user folder. */
            if (copy_file(h, u)) { win_error_from_errno(); rt_ret_stdcall(INVALID_HANDLE, 7); return; }
        } else if (flags & O_CREAT) {
            make_parents(u);
        } else if (!in_u && !in_h) {
            win_last_error = ERROR_FILE_NOT_FOUND;
            rt_ret_stdcall(INVALID_HANDLE, 7);
            return;
        }
        if (!in_u && in_h && (disposition == 2 || disposition == 5)) make_parents(u);
        if (disposition == 5 && !in_u) flags |= O_CREAT;
        if (in_u && (disposition == 2 || disposition == 5)) save_backup(h, u, 1);
    }
    int mem = write_intent ? -1 : mem_find(target);
    if (mem >= 0) {
        for (uint32_t i = 0; i < MAX_FILES; i++) {
            if (!fd_used[i]) {
                fd_used[i] = 1;
                fds[i] = MEM_FD;
                mem_of[i] = mem;
                mem_pos[i] = 0;
                win_last_error = 0;
                rt_ret_stdcall(FILE_BASE + 4 * i, 7);
                return;
            }
        }
        rt_fatal("too many open files");
    }
    int fd = keep_live() && !(flags & (O_CREAT | O_TRUNC | O_EXCL)) ? keep_take(target, flags) : -1;
    if (fd < 0) fd = host_open(target, flags, 0644);
    WIN_TRACE("CreateFileA(%s -> %s, %#x, %u) = %d\n", name, target, access, disposition, fd);
    if (fd < 0) {
        win_error_from_errno();
        rt_ret_stdcall(INVALID_HANDLE, 7);
        return;
    }
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (!fd_used[i]) {
            fd_used[i] = 1;
            fds[i] = fd;
            fd_flags[i] = flags;
            fd_path[i] = write_intent ? strdup(target) : NULL;
            /* Item 151: a handle of KEEP_NAME in the user folder can be kept at its close. */
            fd_keep[i] = target == u && kept_name(target) ? strdup(target) : NULL;
            fd_name[i] = fd_keep[i] ? strdup(name) : NULL;
            fd_access[i] = access;
            fd_disposition[i] = disposition;
            win_last_error = existed && (disposition == 2 || disposition == 4) ? ERROR_ALREADY_EXISTS : 0;
            rt_ret_stdcall(FILE_BASE + 4 * i, 7);
            return;
        }
    }
    rt_fatal("too many open files");
}

/* ReadFile(handle, buffer, count, &read, overlapped) */
RT_IMPORT(ReadFile)
{
    uint32_t h = rt_arg(0), buf = rt_arg(1), count = rt_arg(2), done = rt_arg(3);
    int fd = fd_of(h), m = mem_slot(h);
    uint32_t got = 0;
    if (m >= 0) {
        const MemFile *f = &mem_file[mem_of[m]];
        size_t left = mem_pos[m] < f->size ? f->size - mem_pos[m] : 0;
        got = count < left ? count : (uint32_t)left;
        if (got) memcpy(g_mem + buf, f->data + mem_pos[m], got);
        mem_pos[m] += got;
    } else if (fd >= 0) {
        while (got < count) {
            long r = host_read(fd, g_mem + buf + got, count - got);
            if (r <= 0) break;
            got += (uint32_t)r;
        }
    } else if (!is_std(h)) {
        win_last_error = ERROR_INVALID_HANDLE;
        if (done) wr32(g_mem, done, 0);
        rt_ret_stdcall(0, 5);
        return;
    }
    if (done) wr32(g_mem, done, got);
    rt_ret_stdcall(1, 5);
}

/* WriteFile(handle, buffer, count, &written, overlapped) */
RT_IMPORT(WriteFile)
{
    uint32_t h = rt_arg(0), buf = rt_arg(1), count = rt_arg(2), done = rt_arg(3);
    int fd = fd_of(h);
    uint32_t put = 0;
    if (fd >= 0) {
        int64_t at = host_seek(fd, 0, SEEK_CUR);
        if (at >= 0 && at + count > WRITE_LIMIT) {
            const char *path = fd_path[(h - FILE_BASE) / 4];
            if (in_save && !too_big_path[0] && path) snprintf(too_big_path, sizeof too_big_path, "%s", path);
            if (!too_big_told) fprintf(stderr, "cm0102_recomp: a write would take %s past 4 GB, so it is refused.\n", path ? path : "a file");
            too_big_told = 1;
            win_last_error = 112;                   /* ERROR_DISK_FULL */
            if (done) wr32(g_mem, done, 0);
            rt_ret_stdcall(0, 5);
            return;
        }
        while (put < count) {
            long r = host_write(fd, g_mem + buf + put, count - put);
            if (r <= 0) break;
            put += (uint32_t)r;
        }
        if (put < count) win_error_from_errno();
    } else if (is_std(h)) {
        put = (uint32_t)fwrite(g_mem + buf, 1, count, stderr);
    } else {
        win_last_error = ERROR_INVALID_HANDLE;
        if (done) wr32(g_mem, done, 0);
        rt_ret_stdcall(0, 5);
        return;
    }
    if (done) wr32(g_mem, done, put);
    rt_ret_stdcall(put == count, 5);
}

/* The distance of a SetFilePointer call without a high word. Windows takes it as signed, so the game's C
 * library (_lseek 0x94c552, from fseek 0x946a22 and ftell 0x946aae) cannot reach a position from 2 GB on. A move
 * from the start of the file to a negative distance fails on Windows, and the game makes none below 2 GB, so the
 * runtime takes the distance of FILE_BEGIN as unsigned: positions up to 4 GB. A relative move stays signed. */
static int64_t seek_distance(uint32_t low, uint32_t method)
{
    return method == 0 ? (int64_t)low : (int64_t)(int32_t)low;
}

/* SetFilePointer(handle, low, &high, method) */
RT_IMPORT(SetFilePointer)
{
    uint32_t h = rt_arg(0), low = rt_arg(1), high_at = rt_arg(2), method = rt_arg(3);
    int fd = fd_of(h), m = mem_slot(h);
    int64_t off = high_at ? (int64_t)((uint64_t)rd32(g_mem, high_at) << 32 | low) : seek_distance(low, method);
    if (m >= 0) {
        int64_t base = method == 0 ? 0 : method == 1 ? (int64_t)mem_pos[m] : (int64_t)mem_file[mem_of[m]].size;
        if (!high_at && base + off > FILE_LIMIT) {
            win_last_error = 87;                    /* ERROR_INVALID_PARAMETER */
            rt_ret_stdcall(0xffffffffu, 4);
            return;
        }
        if (base + off < 0) {
            win_last_error = 131;                   /* ERROR_NEGATIVE_SEEK */
            rt_ret_stdcall(0xffffffffu, 4);
            return;
        }
        mem_pos[m] = (size_t)(base + off);
        if (high_at) wr32(g_mem, high_at, (uint32_t)((uint64_t)mem_pos[m] >> 32));
        win_last_error = 0;
        rt_ret_stdcall((uint32_t)mem_pos[m], 4);
        return;
    }
    if (fd < 0) {
        win_last_error = ERROR_INVALID_HANDLE;
        rt_ret_stdcall(0xffffffffu, 4);
        return;
    }
    int64_t was = host_seek(fd, 0, SEEK_CUR);
    int64_t r = host_seek(fd, off, method == 0 ? SEEK_SET : method == 1 ? SEEK_CUR : SEEK_END);
    if (r >= 0 && !high_at && r > FILE_LIMIT) {
        /* Windows fails a move to a position that does not fit the low word and leaves the pointer. */
        host_seek(fd, was, SEEK_SET);
        win_last_error = 87;                        /* ERROR_INVALID_PARAMETER */
        rt_ret_stdcall(0xffffffffu, 4);
        return;
    }
    if (r < 0) {
        win_last_error = 131;                       /* ERROR_NEGATIVE_SEEK */
        rt_ret_stdcall(0xffffffffu, 4);
        return;
    }
    if (high_at) wr32(g_mem, high_at, (uint32_t)((uint64_t)r >> 32));
    win_last_error = 0;
    rt_ret_stdcall((uint32_t)r, 4);
}

RT_IMPORT(SetEndOfFile)
{
    int fd = fd_of(rt_arg(0));
    int ok = fd >= 0 && host_truncate(fd, host_seek(fd, 0, SEEK_CUR)) == 0;
    rt_ret_stdcall((uint32_t)ok, 1);
}

RT_IMPORT(FlushFileBuffers) { rt_ret_stdcall(1, 1); }

RT_IMPORT(GetFileType)
{
    uint32_t h = rt_arg(0);
    if (is_std(h)) { rt_ret_stdcall(2, 1); return; }      /* FILE_TYPE_CHAR */
    if (win_is_file_handle(h)) { rt_ret_stdcall(1, 1); return; }  /* FILE_TYPE_DISK */
    win_last_error = ERROR_INVALID_HANDLE;
    rt_ret_stdcall(0, 1);
}

RT_IMPORT(CloseHandle)
{
    uint32_t h = rt_arg(0);
    win_close_file(h);
    rt_ret_stdcall(1, 1);
}

RT_IMPORT(GetFileAttributesA)
{
    const char *name = win_str(rt_arg(0));
    char h[2048], u[2048];
    HostStat st;
    if (!win_paths(name, h, u, sizeof h) || (host_stat(u, &st) && host_stat(h, &st))) {
        win_last_error = ERROR_FILE_NOT_FOUND;
        rt_ret_stdcall(0xffffffffu, 1);
        return;
    }
    rt_ret_stdcall(st.is_dir ? 0x10u : 0x20u, 1);
}

/* 1 while a save that had a refused write (WRITE_LIMIT) would delete or rename the .sav file name. */
static int save_refused(const char *name)
{
    return in_save && too_big_path[0] && has_sav_ending(name);
}

void win_save_begin(void)
{
    in_save = 1;
    too_big_path[0] = 0;
    too_big_told = 0;
    save_deleted = 0;
}

int win_save_end(void)
{
    in_save = 0;
    if (!too_big_path[0]) return 0;
    int deleted = save_deleted;
    /* The file holds parts that could not be written. Its handles are closed: the game closes the container
     * after each part (zclose 0x932b00). */
    if (host_unlink(too_big_path)) fprintf(stderr, "cm0102_recomp: cannot delete %s (%s).\n", too_big_path, strerror(errno));
    fprintf(stderr, "cm0102_recomp: the save would pass 4 GB, so it is not written.%s\n",
            deleted ? " The game deleted the earlier save of this name before it wrote." : " The earlier save stays.");
    too_big_path[0] = 0;
    return deleted ? 2 : 1;
}

/* Delete and rename act on the user folder only. A file that exists only in the game folder stays. A save
 * that the game deletes to make room for a new one goes to backups/; a save that the player deletes does not. */
RT_IMPORT(DeleteFileA)
{
    const char *name = win_str(rt_arg(0));
    char h[2048], u[2048];
    int ok = 0;
    if (save_refused(name)) {
        errno = ENOSPC;
    } else if (win_paths(name, h, u, sizeof h)) {
        keep_drop(NULL);                                /* item 151: a kept name may now give another file */
        if (temp_save_beside(u) && save_backup(h, u, 0)) ok = 1;   /* a new save follows: the old moved to backups/ */
        else if (exists(u)) ok = host_unlink(u) == 0;
        /* A save without the temp file (no option bit 0x20000000) deletes the old .sav before it writes (0x82e62d). */
        size_t nl = strlen(name);
        if (ok && in_save && has_sav_ending(name) && (nl < 18 || strcasecmp(name + nl - 18, "temp_save_file.sav")))
            save_deleted = 1;
        else if (exists(h)) { errno = EACCES; }
        else errno = ENOENT;
    }
    WIN_TRACE("DeleteFileA(%s) = %d\n", name, ok);
    if (!ok) win_error_from_errno();
    rt_ret_stdcall((uint32_t)ok, 1);
}

RT_IMPORT(MoveFileA)
{
    char fh[2048], fu[2048], th[2048], tu[2048];
    int ok = win_paths(win_str(rt_arg(0)), fh, fu, sizeof fh) && win_paths(win_str(rt_arg(1)), th, tu, sizeof th);
    if (save_refused(win_str(rt_arg(0))) || save_refused(win_str(rt_arg(1)))) {
        win_last_error = 112;                       /* ERROR_DISK_FULL */
        ok = 0;
    } else if (ok && (exists(tu) || exists(th))) { win_last_error = ERROR_ALREADY_EXISTS; ok = 0; }
    else if (ok && exists(fu)) {
        keep_drop(NULL);
        make_parents(tu);
        if (host_rename(fu, tu)) { win_error_from_errno(); ok = 0; }
    } else if (ok && exists(fh) && !is_dir(fh)) {
        /* The source is in the game folder: the new name appears in the user folder. */
        if (copy_file(fh, tu)) { win_error_from_errno(); ok = 0; }
    } else if (ok) {
        errno = exists(fh) ? EACCES : ENOENT;
        win_error_from_errno();
        ok = 0;
    }
    WIN_TRACE("MoveFileA(%s, %s) = %d\n", win_str(rt_arg(0)), win_str(rt_arg(1)), ok);
    rt_ret_stdcall((uint32_t)ok, 2);
}

RT_IMPORT(CreateDirectoryA)
{
    char h[2048], u[2048], parent[2048];
    keep_drop(NULL);                                    /* item 151: the kept names (keep_find_name) */
    int ok = win_paths(win_str(rt_arg(0)), h, u, sizeof h);
    if (ok && (exists(u) || exists(h))) { win_last_error = ERROR_ALREADY_EXISTS; rt_ret_stdcall(0, 2); return; }
    if (ok) {
        char *slash;
        snprintf(parent, sizeof parent, "%s", h);
        slash = strrchr(parent, '/');
        if (slash && slash != parent) *slash = 0;
        int parent_h = is_dir(parent);
        snprintf(parent, sizeof parent, "%s", u);
        slash = strrchr(parent, '/');
        if (slash) *slash = 0;
        if (!parent_h && !is_dir(parent)) ok = 0, win_last_error = ERROR_PATH_NOT_FOUND;
        else {
            make_parents(u);
            ok = host_mkdir(u) == 0;
            if (!ok) win_last_error = ERROR_PATH_NOT_FOUND;
        }
    } else win_last_error = ERROR_PATH_NOT_FOUND;
    rt_ret_stdcall((uint32_t)ok, 2);
}

RT_IMPORT(SetCurrentDirectoryA)
{
    const char *name = win_str(rt_arg(0));
    char h[2048], u[2048], full[1024];
    keep_drop(NULL);                                    /* item 151: a relative kept name now means another file */
    if (!win_paths(name, h, u, sizeof h) || !(is_dir(u) || is_dir(h))) {
        win_last_error = ERROR_PATH_NOT_FOUND;
        rt_ret_stdcall(0, 1);
        return;
    }
    full_path(name, full, sizeof full);
    snprintf(win_cwd, sizeof win_cwd, "%s", full);
    WIN_TRACE("SetCurrentDirectoryA(%s) -> %s\n", name, win_cwd);
    rt_ret_stdcall(1, 1);
}

/* Returns the length, or the room needed (with the NUL) when the buffer is too small. */
RT_IMPORT(GetCurrentDirectoryA)
{
    uint32_t room = rt_arg(0), buf = rt_arg(1);
    uint32_t len = (uint32_t)strlen(win_cwd);
    if (room <= len) { rt_ret_stdcall(len + 1, 2); return; }
    win_put_str(buf, win_cwd, room);
    rt_ret_stdcall(len, 2);
}

/* GetFullPathNameA(name, room, buffer, &file part) */
RT_IMPORT(GetFullPathNameA)
{
    uint32_t room = rt_arg(1), buf = rt_arg(2), part = rt_arg(3);
    char full[1024];
    full_path(win_str(rt_arg(0)), full, sizeof full);
    uint32_t len = (uint32_t)strlen(full);
    if (room <= len) { rt_ret_stdcall(len + 1, 4); return; }
    win_put_str(buf, full, room);
    if (part) {
        char *slash = strrchr(full, '\\');
        wr32(g_mem, part, slash && slash[1] ? buf + (uint32_t)(slash + 1 - full) : 0);
    }
    rt_ret_stdcall(len, 4);
}

/* Drives: the fixed disk of the game folder (C: on macOS) and the CD. */
RT_IMPORT(GetLogicalDriveStringsA)
{
    uint32_t room = rt_arg(0), buf = rt_arg(1);
    char game[1100], a, b;
    win_windows_path(win_game_dir, game, sizeof game);
    a = (char)toupper((unsigned char)game[0]);
    b = host_cd_drive();
    if (a > b) { char t = a; a = b; b = t; }
    const char drives[9] = { a, ':', '\\', 0, b, ':', '\\', 0, 0 };   /* the CD letter is never the game's */
    uint32_t size = sizeof drives;
    if (room < size) { rt_ret_stdcall(size, 2); return; }
    memcpy(g_mem + buf, drives, size);
    rt_ret_stdcall(size - 1, 2);
}

RT_IMPORT(GetDriveTypeA)
{
    uint32_t root = rt_arg(0);
    char drive = root ? (char)toupper((unsigned char)win_str(root)[0]) : win_cwd[0];
    char rootbuf[1024];
    uint32_t type = drive == host_cd_drive() ? 5u : drive_root(drive, rootbuf, sizeof rootbuf) ? 3u : 1u;
    rt_ret_stdcall(type, 1);   /* CDROM, FIXED, NO_ROOT_DIR */
}

/* GetDiskFreeSpaceA(root, sectors per cluster, bytes per sector, free clusters, clusters) */
RT_IMPORT(GetDiskFreeSpaceA)
{
    if (rt_arg(1)) wr32(g_mem, rt_arg(1), 8);
    if (rt_arg(2)) wr32(g_mem, rt_arg(2), 512);
    if (rt_arg(3)) wr32(g_mem, rt_arg(3), 0x200000);
    if (rt_arg(4)) wr32(g_mem, rt_arg(4), 0x400000);
    rt_ret_stdcall(1, 5);
}

/* File times. */
static uint64_t filetime_of(int64_t t) { return ((uint64_t)t + 11644473600ull) * 10000000ull; }

RT_IMPORT(FileTimeToLocalFileTime)
{
    uint64_t ft = rd64(g_mem, rt_arg(0));
    wr64(g_mem, rt_arg(1), ft - (uint64_t)((int64_t)win_bias_minutes() * 600000000ll));
    rt_ret_stdcall(1, 2);
}

RT_IMPORT(FileTimeToSystemTime)
{
    uint64_t ft = rd64(g_mem, rt_arg(0));
    time_t s = (time_t)(ft / 10000000ull) - (time_t)11644473600ll;
    struct tm t;
    host_gmtime(s, &t);
    win_put_systemtime(rt_arg(1), &t, (uint32_t)(ft / 10000ull % 1000));
    rt_ret_stdcall(1, 2);
}

/* Finds. A find holds the matching names of one folder, read at FindFirstFileA. */
/* The original does not close every find (the CD check), and Windows has no small limit. */
#define MAX_FINDS 4096
#define FIND_BASE 0x20000u
typedef struct { int used; char **names; char **paths; int count, next; } Find;
static Find finds[MAX_FINDS];

/* * and ? without regard to case. As on Windows, "*.*" matches every name, also one without a dot. */
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

static int wild_match(const char *pat, const char *s)
{
    if (!strcmp(pat, "*.*")) return 1;
    return glob(pat, s);
}

/* WIN32_FIND_DATAA: attributes 0, times 4/0xc/0x14, size 0x1c/0x20, name 0x2c, short name 0x130. */
static void fill_find_data(Find *f, uint32_t data, int idx)
{
    HostStat st;
    const char *path = f->paths[idx];
    memset(g_mem + data, 0, 0x140);
    if (host_stat(path, &st) == 0) {
        wr32(g_mem, data, st.is_dir ? 0x10u : 0x20u);
        uint64_t ft = filetime_of(st.mtime);
        wr64(g_mem, data + 4, ft);
        wr64(g_mem, data + 0xc, ft);
        wr64(g_mem, data + 0x14, ft);
        wr32(g_mem, data + 0x1c, (uint32_t)(st.size >> 32));
        wr32(g_mem, data + 0x20, (uint32_t)st.size);
    }
    win_put_str(data + 0x2c, f->names[idx], 260);
}

static void free_find(Find *f)
{
    for (int i = 0; i < f->count; i++) { free(f->names[i]); free(f->paths[i]); }
    free(f->names);
    free(f->paths);
    memset(f, 0, sizeof *f);
}

/* Names that a find leaves out while it is set (save_parts.c: the files of a merged part, during the original's
 * pack 0x933610). */
static int (*find_hidden)(const char *name);

void win_find_hide(int (*hidden)(const char *name))
{
    find_hidden = hidden;
}

/* Adds the matching names of one folder; a name already listed (ignoring case) is skipped. */
enum { SAV_ALL, SAV_SKIP, SAV_ONLY };      /* which names of a folder a find takes: by the ending .sav */
static void find_add_dir(Find *f, const char *host, const char *mask, int is_root, int sav)
{
    HostDir *d = host_opendir(host);
    if (!d) return;
    const char *name;
    while ((name = host_readdir(d))) {
        if (is_root && (!strcmp(name, ".") || !strcmp(name, ".."))) continue;
        if (!wild_match(mask, name)) continue;
        if (find_hidden && find_hidden(name)) continue;
        if ((sav == SAV_SKIP && has_sav_ending(name)) || (sav == SAV_ONLY && !has_sav_ending(name))) continue;
        int dup = 0;
        for (int i = 0; i < f->count && !dup; i++) dup = !strcasecmp(f->names[i], name);
        if (dup) continue;
        f->names = realloc(f->names, sizeof(char *) * (size_t)(f->count + 1));
        f->paths = realloc(f->paths, sizeof(char *) * (size_t)(f->count + 1));
        char path[2400];
        snprintf(path, sizeof path, "%s/%s", host, name);
        f->names[f->count] = strdup(name);
        f->paths[f->count++] = strdup(path);
    }
    host_closedir(d);
}

RT_IMPORT(FindFirstFileA)
{
    const char *pattern = win_str(rt_arg(0));
    uint32_t data = rt_arg(1);
    char full[1024], dirpart[1024], h[2048], u[2048];
    full_path(pattern, full, sizeof full);
    char *slash = strrchr(full, '\\');
    const char *mask = slash + 1;
    snprintf(dirpart, sizeof dirpart, "%.*s", (int)(slash - full) + (slash == full + 2 ? 1 : 0), full);
    Find *f = NULL;
    int i;
    for (i = 0; i < MAX_FINDS; i++) if (!finds[i].used) { f = &finds[i]; break; }
    if (!f) rt_fatal("too many finds");
    if (!win_paths(dirpart, h, u, sizeof h) || !(is_dir(u) || is_dir(h))) {
        win_last_error = ERROR_PATH_NOT_FOUND;
        rt_ret_stdcall(INVALID_HANDLE, 2);
        return;
    }
    f->used = 1;
    int is_root = strlen(dirpart) <= 3;
    const char *sd = save_dir();
    int game_dir = sd && (HOST_FOLDS_CASE ? !strcasecmp(h, win_game_dir) : !strcmp(h, win_game_dir));
    find_add_dir(f, u, mask, is_root, game_dir ? SAV_SKIP : SAV_ALL);   /* the user folder first: it wins on equal names */
    find_add_dir(f, h, mask, is_root, game_dir ? SAV_SKIP : SAV_ALL);
    if (game_dir) find_add_dir(f, sd, mask, 0, SAV_ONLY);       /* the saved games of the save folder */
    WIN_TRACE("FindFirstFileA(%s) in %s + %s: %d matches\n", pattern, u, h, f->count);
    if (!f->count) {
        free_find(f);
        win_last_error = ERROR_FILE_NOT_FOUND;
        rt_ret_stdcall(INVALID_HANDLE, 2);
        return;
    }
    fill_find_data(f, data, 0);
    f->next = 1;
    rt_ret_stdcall(FIND_BASE + 4 * (uint32_t)i, 2);
}

static Find *find_of(uint32_t h)
{
    if (h < FIND_BASE || h >= FIND_BASE + 4 * MAX_FINDS || (h & 3)) return NULL;
    Find *f = &finds[(h - FIND_BASE) / 4];
    return f->used ? f : NULL;
}

RT_IMPORT(FindNextFileA)
{
    Find *f = find_of(rt_arg(0));
    if (!f || f->next >= f->count) {
        win_last_error = f ? ERROR_NO_MORE_FILES : ERROR_INVALID_HANDLE;
        rt_ret_stdcall(0, 2);
        return;
    }
    fill_find_data(f, rt_arg(1), f->next++);
    rt_ret_stdcall(1, 2);
}

RT_IMPORT(FindClose)
{
    Find *f = find_of(rt_arg(0));
    if (f) free_find(f);
    rt_ret_stdcall(f != NULL, 1);
}

int win_list_files(const char *folder, const char *mask, void (*each)(const char *name, const char *host, void *ctx),
                   void *ctx)
{
    char h[2048], u[2048];
    if (!win_paths(folder, h, u, sizeof h)) return -1;
    Find f;
    memset(&f, 0, sizeof f);
    find_add_dir(&f, u, mask, 0, SAV_ALL);      /* as FindFirstFileA: the user folder first */
    find_add_dir(&f, h, mask, 0, SAV_ALL);
    int n = f.count;
    for (int i = 0; i < n; i++) each(f.names[i], f.paths[i], ctx);
    free_find(&f);
    return n;
}

/* Open item 192 (history_trim.c): deletes the user-folder file of a Windows path, as DeleteFileA does for a file that
 * is not a save. 1 when a file was deleted. */
int win_delete_file(const char *winpath)
{
    char h[2048], u[2048];
    if (!win_paths(winpath, h, u, sizeof h) || !exists(u)) return 0;
    keep_drop(NULL);                                /* item 151: a kept name may now give another file */
    return host_unlink(u) == 0;
}

int win_create_file(const char *winpath)
{
    char h[2048], u[2048];
    if (!win_paths(winpath, h, u, sizeof h)) return -1;
    make_parents(u);
    return host_open(u, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}
