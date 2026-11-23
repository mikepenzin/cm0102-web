/* The host system under the runtime of the recompiled game: memory, files, folders, time and the
 * folders of the program and the user. host/posix.c serves macOS, host/win32.c serves 64-bit Windows.
 *
 * Host paths are UTF-8 with '/' between the parts. On macOS a path starts at the root ("/Users/max").
 * On Windows it starts with a drive ("C:/Users/max"). The functions that can fail set errno as the C
 * library does. */
#ifndef RECOMP_HOST_H
#define RECOMP_HOST_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

/* 1 when the host file system ignores case (Windows). The runtime then skips its own lookup of the
 * spelling of each part of a path. */
#ifdef _WIN32
#define HOST_FOLDS_CASE 1
#else
#define HOST_FOLDS_CASE 0
#endif

/* A block of size bytes that reads as zero and that the program can write anywhere. Pages may come
 * on first use. NULL on failure. */
void *host_reserve(size_t size);

/* The guard of RECOMP_GUARD (rt.c), a test: host_protect makes a page-aligned part of the reserved block fault on
 * any access (open 0) or open again (open 1), 0 on success, -1 where the host cannot (Windows, the browser).
 * host_on_fault calls note with the address of each such fault before the program stops as it would without it. */
int host_protect(void *addr, size_t size, int open);
void host_on_fault(void (*note)(const void *addr));

/* Files. host_open takes the O_ flags of <fcntl.h> and opens in binary mode. */
int host_open(const char *path, int flags, int mode);
long host_read(int fd, void *buf, uint32_t n);
long host_write(int fd, const void *buf, uint32_t n);
int64_t host_seek(int fd, int64_t off, int whence);
int host_truncate(int fd, int64_t size);     /* 0 on success */
int host_close(int fd);

/* fopen for a UTF-8 host path. */
FILE *host_fopen(const char *utf8_path, const char *mode);

/* mtime_ns and ctime_ns: 0 to 999999999. dev, ino and the ctime are 0 on Windows (the folder cache does not
 * run there: HOST_FOLDS_CASE). */
typedef struct {
    int is_dir;
    uint64_t size;
    int64_t mtime, mtime_ns, ctime, ctime_ns;
    uint64_t dev, ino;
} HostStat;
int host_stat(const char *path, HostStat *st);   /* 0 on success */
int host_mkdir(const char *path);                /* 0 on success */
int host_unlink(const char *path);               /* 0 on success */
int host_rmdir(const char *path);                /* removes an empty folder; 0 on success */
int host_rename(const char *from, const char *to);
/* Moves from to to and fails when to exists, also when it appears at that moment (link and unlink on POSIX,
 * the Windows rename). 0 on success. */
int host_rename_new(const char *from, const char *to);
/* Moves from over to, and replaces to when it exists (also on Windows). 0 on success. */
int host_replace(const char *from, const char *to);
/* Flushes the file to the disk. 0 on success. Call fflush first. */
int host_sync(FILE *f);

#ifdef __EMSCRIPTEN__
/* The browser build (host/web.c): nonzero while the page calls the runtime between two frames, outside the
 * game's turn (rt_web_choose_data_set). Nothing may wait for the browser then: JSPI stops such a wait with "trying
 * to suspend without WebAssembly.promising". host_sync, which is FS.syncfs of the browser's storage (fd_sync), then
 * only flushes, and the page saves the folder to the browser's storage itself after the call. */
extern int host_web_page_call;
#endif

/* Folder listing: each name, "." and ".." included, then NULL. */
typedef struct HostDir HostDir;
HostDir *host_opendir(const char *path);
const char *host_readdir(HostDir *d);
void host_closedir(HostDir *d);

/* The absolute form of a path that exists, with no "." or "..". 0 on failure. */
int host_realpath(const char *path, char *out, size_t room);
/* The path of the running program. 0 on failure. */
int host_program_path(char *out, size_t room);

/* Starts this program again with the arguments of this start (argv as main got it), and ends this one: the game's
 * Restart Game (restart_game 0x68dbc0 runs it through WinExec, kernel32.c). Returns only when it could not. In the
 * browser the page saves the user folder and loads again (loader.js, Module.rtRestart). */
void host_restart(char *const argv[]);
/* The current folder. 0 on failure. */
int host_current_dir(char *out, size_t room);
/* The folder for every write of the game, not yet created: ~/Library/Application Support/cm0102 or
 * %APPDATA%\cm0102. 0 with a message on stderr when the system gives no base folder. */
int host_user_dir(char *out, size_t room);
/* The home folder of the player, with "/" between parts. 0 when the system gives none. */
int host_home_dir(char *out, size_t room);

/* The drive letter of the CD drive that holds the player's cm0102.exe: D on macOS, a letter that the
 * system does not use on Windows. */
char host_cd_drive(void);
/* 1 when the host has this drive (Windows only; macOS has none). */
int host_has_drive(char letter);

/* The display report (display_report.c). host_system_name writes one line: the system, its version and the
 * processor. host_display_notes writes lines of facts that SDL does not give: on Windows the screen scale and
 * whether Windows stretches the window of the game, on Linux the desktop session and its scale settings. Each
 * line ends with a newline. Both write an empty text when they have nothing. */
void host_system_name(char *out, size_t room);
void host_display_notes(char *out, size_t room);

/* The computer's own folder picker (Q419, the Save folder row of the options box): 1 with the chosen folder in
 * out ("/" between parts, no "/" at the end), 0 when the player cancels, -1 when this computer has no picker
 * (Linux without zenity or kdialog). It blocks until the player answers. */
int host_pick_folder(const char *title, char *out, size_t room);
/* Opens a folder in the computer's file manager (0.6.1 item 5, the Open folder buttons of the options box and
 * Load Game): Finder on a Mac (open), Explorer on Windows (ShellExecute), the desktop's file manager on Linux
 * (xdg-open). 1 when the file manager was started, 0 when the folder does not exist or nothing could open it. It
 * does not wait for the file manager. The test variable RECOMP_OPEN_FOLDER_PROGRAM names another program to start
 * with the folder as its one argument (Mac and Linux). */
int host_open_folder(const char *path);

/* The cm0102:// link handler (0.7.0 slice 8, host/link.c): the computer starts this program with a link
 * cm0102://join/<code> from an invite link page. on 1 writes it for this program, for the current user only,
 * and writes nothing when it is there already. on 0 removes it when it starts this program (on Linux: this AppImage). Windows: the key
 * HKEY_CURRENT_USER\Software\Classes\cm0102. Linux, only in an AppImage: the file
 * cm0102-bosman-edition.desktop in ~/.local/share/applications. macOS: nothing, because the app's Info.plist
 * names the link and the system reads it. 1 when the handler is this program's now, 0 when there is none, -1
 * on a failure (with a message on stderr). */
int host_link_handler(int on);

/* One copy of the game takes the invite links of a user (0.7.0 slice 8, Q41, host/link_pass.c). host_link_pass
 * gives the link to the copy that listens on the channel of this user folder: 1 when that copy kept it, and the
 * caller then ends. host_link_listen makes this copy the one that listens, when no other copy does: 0 when it
 * listens. host_link_take gives a link that arrived, once: 1 with the link in out. */
int host_link_pass(const char *user_dir, const char *link);
int host_link_listen(const char *user_dir);
int host_link_take(char *out, size_t room);

/* Time. */
void host_gmtime(time_t t, struct tm *out);
void host_localtime(time_t t, struct tm *out);
int32_t host_bias_minutes(void);             /* minutes behind UTC now, as Windows gives it */
uint64_t host_unix_time_us(void);            /* microseconds since 1970 */

#endif
