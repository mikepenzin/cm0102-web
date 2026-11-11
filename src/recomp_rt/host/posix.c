/* The host functions of host.h on macOS and Linux (POSIX). */
#include "../host.h"

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <sys/resource.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

void *host_reserve(size_t size)
{
    void *m = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    return m == MAP_FAILED ? NULL : m;
}

#ifdef __EMSCRIPTEN__
int host_protect(void *addr, size_t size, int open) { (void)addr; (void)size; (void)open; return -1; }
void host_on_fault(void (*note)(const void *addr)) { (void)note; }
#else
int host_protect(void *addr, size_t size, int open)
{
    return mprotect(addr, size, open ? PROT_READ | PROT_WRITE : PROT_NONE);
}

static void (*fault_note)(const void *addr);

static void on_fault(int sig, siginfo_t *info, void *context)
{
    (void)context;
    if (fault_note) fault_note(info->si_addr);
    signal(sig, SIG_DFL);                    /* the access runs again on return and stops the program as before */
}

void host_on_fault(void (*note)(const void *addr))
{
    fault_note = note;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);            /* macOS reports an access to a protected page as SIGBUS */
}
#endif

int host_open(const char *path, int flags, int mode) { return open(path, flags, mode); }
long host_read(int fd, void *buf, uint32_t n) { return (long)read(fd, buf, n); }
long host_write(int fd, const void *buf, uint32_t n) { return (long)write(fd, buf, n); }
int64_t host_seek(int fd, int64_t off, int whence) { return (int64_t)lseek(fd, (off_t)off, whence); }
int host_truncate(int fd, int64_t size) { return ftruncate(fd, (off_t)size); }
int host_close(int fd) { return close(fd); }

int host_stat(const char *path, HostStat *out)
{
    struct stat st;
    if (stat(path, &st)) return -1;
    out->is_dir = S_ISDIR(st.st_mode);
    out->size = (uint64_t)st.st_size;
    out->mtime = (int64_t)st.st_mtime;
    return 0;
}

int host_mkdir(const char *path) { return mkdir(path, 0755); }
int host_unlink(const char *path) { return unlink(path); }
int host_rmdir(const char *path) { return rmdir(path); }
int host_rename(const char *from, const char *to) { return rename(from, to); }
int host_replace(const char *from, const char *to) { return rename(from, to); }
int host_sync(FILE *f)
{
#ifdef __EMSCRIPTEN__
    if (host_web_page_call) return 0;            /* fd_sync would wait for the browser (host.h) */
#endif
    return fsync(fileno(f));
}

HostDir *host_opendir(const char *path) { return (HostDir *)opendir(path); }

const char *host_readdir(HostDir *d)
{
    struct dirent *e = readdir((DIR *)d);
    return e ? e->d_name : NULL;
}

void host_closedir(HostDir *d) { closedir((DIR *)d); }

int host_realpath(const char *path, char *out, size_t room)
{
    char buf[PATH_MAX];
    if (!realpath(path, buf)) return 0;
    return (size_t)snprintf(out, room, "%s", buf) < room;
}

int host_program_path(char *out, size_t room)
{
    char exe[PATH_MAX];
#ifdef __APPLE__
    uint32_t n = sizeof exe;
    if (_NSGetExecutablePath(exe, &n)) return 0;
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return 0;
    exe[n] = 0;
#endif
    return host_realpath(exe, out, room);
}

int host_current_dir(char *out, size_t room) { return getcwd(out, room) != NULL; }

int host_user_dir(char *out, size_t room)
{
    const char *home = getenv("HOME");
    if (!home || !*home) {
        fprintf(stderr, "cm0102_recomp: HOME is not set, so the user folder is unknown.\n");
        return 0;
    }
#ifdef __APPLE__
    return (size_t)snprintf(out, room, "%s/Library/Application Support/cm0102", home) < room;
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return (size_t)snprintf(out, room, "%s/cm0102", xdg) < room;
    return (size_t)snprintf(out, room, "%s/.local/share/cm0102", home) < room;
#endif
}

int host_home_dir(char *out, size_t room)
{
    const char *home = getenv("HOME");
    return home && *home && (size_t)snprintf(out, room, "%s", home) < room;
}

char host_cd_drive(void) { return 'D'; }
int host_has_drive(char letter) { (void)letter; return 0; }

void host_gmtime(time_t t, struct tm *out) { gmtime_r(&t, out); }
void host_localtime(time_t t, struct tm *out) { localtime_r(&t, out); }

int32_t host_bias_minutes(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    return (int32_t)(-t.tm_gmtoff / 60);
}

uint64_t host_unix_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000u + (uint64_t)tv.tv_usec;
}

FILE *host_fopen(const char *utf8_path, const char *mode)
{
    return fopen(utf8_path, mode);
}

#ifdef __linux__
/* The game makes a host call for every call of the original, so it needs a deep stack (the Mac build
 * asks the linker for 256 MB). On Linux the stack limit is set before the program starts: raise it,
 * then start the program again. RECOMP_STACK_SET stops a second start. */
__attribute__((constructor)) static void raise_stack(void)
{
    const rlim_t want = 0x10000000;
    struct rlimit r;
    if (getenv("RECOMP_STACK_SET") || getrlimit(RLIMIT_STACK, &r)) return;
    if (r.rlim_cur == RLIM_INFINITY || r.rlim_cur >= want) return;
    if (r.rlim_max != RLIM_INFINITY && r.rlim_max < want) r.rlim_cur = r.rlim_max;
    else r.rlim_cur = want;
    if (setrlimit(RLIMIT_STACK, &r)) return;
    setenv("RECOMP_STACK_SET", "1", 1);
    extern char **environ;
    /* Keep the arguments: read them from /proc/self/cmdline. */
    static char buf[16384];
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = 0;
    char *args[256];
    int k = 0;
    for (char *p = buf; p < buf + n && k < 255; p += strlen(p) + 1) args[k++] = p;
    args[k] = NULL;
    execve("/proc/self/exe", args, environ);
}
#endif

/* The display report (host.h). */
#include <sys/utsname.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
static void sysctl_text(const char *name, char *out, size_t room)
{
    size_t n = room;
    if (sysctlbyname(name, out, &n, NULL, 0) || !n) snprintf(out, room, "?");
}
#endif

void host_system_name(char *out, size_t room)
{
    struct utsname u;
    if (uname(&u)) memset(&u, 0, sizeof u);
#ifdef __APPLE__
    char version[64], model[64];
    sysctl_text("kern.osproductversion", version, sizeof version);
    sysctl_text("hw.model", model, sizeof model);
    int translated = 0;
    size_t n = sizeof translated;
    if (sysctlbyname("sysctl.proc_translated", &translated, &n, NULL, 0)) translated = 0;
    snprintf(out, room, "macOS %s, %s, %s%s\n", version, model, u.machine,
             translated ? " (the game runs under Rosetta)" : "");
#else
    char name[256] = "";
    FILE *f = fopen("/etc/os-release", "r");
    char line[256];
    while (f && fgets(line, sizeof line, f)) {
        if (strncmp(line, "PRETTY_NAME=", 12)) continue;
        char *v = line + 12;
        if (*v == '"') v++;
        v[strcspn(v, "\"\n")] = 0;
        snprintf(name, sizeof name, "%s", v);
    }
    if (f) fclose(f);
    snprintf(out, room, "%.200s, %s %s, %s\n", name[0] ? name : "Linux", u.sysname, u.release, u.machine);
#endif
}

void host_display_notes(char *out, size_t room)
{
    out[0] = 0;
#ifndef __APPLE__
    static const char *const names[] = {"XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "WAYLAND_DISPLAY", "DISPLAY",
                                        "GDK_SCALE", "GDK_DPI_SCALE", "QT_SCALE_FACTOR", "SDL_VIDEODRIVER",
                                        "SteamDeck"};
    size_t len = 0;
    for (size_t i = 0; i < sizeof names / sizeof *names && len < room; i++) {
        const char *v = getenv(names[i]);
        len += (size_t)snprintf(out + len, room - len, "%s: %s\n", names[i], v && *v ? v : "(not set)");
    }
    if (len < room)
        snprintf(out + len, room - len, "AppImage: %s\n", getenv("APPIMAGE") ? "yes" : "no");
#endif
}

/* The folder picker: AppleScript's choose folder on a Mac, zenity or kdialog on Linux. Their answer is the
 * path on one line; a cancel gives no line. */
int host_pick_folder(const char *title, char *out, size_t room)
{
    char cmd[512];
#ifdef __APPLE__
    snprintf(cmd, sizeof cmd, "osascript -e 'POSIX path of (choose folder with prompt \"%s\")' 2>/dev/null", title);
#else
    if (!system("command -v zenity >/dev/null 2>&1"))
        snprintf(cmd, sizeof cmd, "zenity --file-selection --directory --title='%s' 2>/dev/null", title);
    else if (!system("command -v kdialog >/dev/null 2>&1"))
        snprintf(cmd, sizeof cmd, "kdialog --getexistingdirectory ~ --title '%s' 2>/dev/null", title);
    else
        return -1;
#endif
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    char line[2048] = "";
    char *got = fgets(line, sizeof line, f);
    pclose(f);
    if (!got) return 0;
    size_t n = strcspn(line, "\r\n");
    line[n] = 0;
    while (n > 1 && line[n - 1] == '/') line[--n] = 0;
    if (!n || n >= room) return 0;
    memcpy(out, line, n + 1);
    return 1;
}
