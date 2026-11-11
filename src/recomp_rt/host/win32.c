/* The host functions of host.h on 64-bit Windows. Paths are UTF-8 with '/' inside the runtime and
 * go to the wide (UTF-16) calls of Windows, so that any user name works.
 *
 * Memory: the 4 GB block is reserved, not committed. A vectored exception handler commits a page on
 * its first access. A read or write of a file commits its buffer first, because the system call
 * returns an error instead of an exception on a reserved page. */
#include "../host.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

/* Memory. */
static uint8_t *block;
static size_t block_size;
#define COMMIT_CHUNK 0x10000u

static int commit(const void *p, size_t n)
{
    const uint8_t *a = p;
    if (!block || a < block || a >= block + block_size) return 0;
    uintptr_t lo = ((uintptr_t)a - (uintptr_t)block) & ~(uintptr_t)(COMMIT_CHUNK - 1);
    uintptr_t hi = (uintptr_t)a - (uintptr_t)block + (n ? n : 1);
    if (hi > block_size) hi = block_size;
    return VirtualAlloc(block + lo, hi - lo, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

static LONG CALLBACK on_fault(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const uint8_t *at = (const uint8_t *)r->ExceptionInformation[1];
    if (!block || at < block || at >= block + block_size) return EXCEPTION_CONTINUE_SEARCH;
    MEMORY_BASIC_INFORMATION mi;
    if (VirtualQuery(at, &mi, sizeof mi) && mi.State == MEM_COMMIT) return EXCEPTION_CONTINUE_SEARCH;
    return commit(at, 1) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

void *host_reserve(size_t size)
{
    void *m = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    if (!m) return NULL;
    block = m;
    block_size = size;
    AddVectoredExceptionHandler(1, on_fault);
    return m;
}

/* The guard of RECOMP_GUARD is a test of macOS and Linux: the block here commits its pages on first use (on_fault). */
int host_protect(void *addr, size_t size, int open) { (void)addr; (void)size; (void)open; return -1; }
void host_on_fault(void (*note)(const void *addr)) { (void)note; }

/* UTF-8 and UTF-16. */
static int to_wide(const char *s, wchar_t *out, int room)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, room);
    if (!n) { errno = ENAMETOOLONG; return 0; }
    return 1;
}

static int from_wide(const wchar_t *s, char *out, size_t room)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, out, (int)room, NULL, NULL);
    if (!n) return 0;
    for (char *p = out; *p; p++) if (*p == '\\') *p = '/';
    return 1;
}

#define WPATH 4096

FILE *host_fopen(const char *utf8_path, const char *mode)
{
    wchar_t w[WPATH], m[16];
    if (!to_wide(utf8_path, w, WPATH)) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, m, 16) == 0) { errno = EINVAL; return NULL; }
    return _wfopen(w, m);
}

int host_open(const char *path, int flags, int mode)
{
    wchar_t w[WPATH];
    if (!to_wide(path, w, WPATH)) return -1;
    (void)mode;
    return _wopen(w, flags | _O_BINARY, _S_IREAD | _S_IWRITE);
}

long host_read(int fd, void *buf, uint32_t n)
{
    commit(buf, n);
    return _read(fd, buf, n);
}

long host_write(int fd, const void *buf, uint32_t n)
{
    commit(buf, n);
    return _write(fd, buf, n);
}

int64_t host_seek(int fd, int64_t off, int whence) { return _lseeki64(fd, off, whence); }
int host_truncate(int fd, int64_t size) { return _chsize_s(fd, size) ? -1 : 0; }
int host_close(int fd) { return _close(fd); }

int host_stat(const char *path, HostStat *out)
{
    wchar_t w[WPATH];
    struct _stat64 st;
    if (!to_wide(path, w, WPATH) || _wstat64(w, &st)) return -1;
    out->is_dir = (st.st_mode & _S_IFDIR) != 0;
    out->size = (uint64_t)st.st_size;
    out->mtime = (int64_t)st.st_mtime;
    return 0;
}

int host_mkdir(const char *path)
{
    wchar_t w[WPATH];
    return to_wide(path, w, WPATH) ? _wmkdir(w) : -1;
}

int host_unlink(const char *path)
{
    wchar_t w[WPATH];
    return to_wide(path, w, WPATH) ? _wunlink(w) : -1;
}

int host_rmdir(const char *path)
{
    wchar_t w[WPATH];
    return to_wide(path, w, WPATH) ? _wrmdir(w) : -1;
}

int host_rename(const char *from, const char *to)
{
    wchar_t a[WPATH], b[WPATH];
    return to_wide(from, a, WPATH) && to_wide(to, b, WPATH) ? _wrename(a, b) : -1;
}

int host_replace(const char *from, const char *to)
{
    wchar_t a[WPATH], b[WPATH];
    if (!to_wide(from, a, WPATH) || !to_wide(to, b, WPATH)) return -1;
    return MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}

int host_sync(FILE *f)
{
    return _commit(_fileno(f));
}

/* Folders. */
struct HostDir {
    HANDLE h;
    WIN32_FIND_DATAW data;
    int have;
    char name[MAX_PATH * 4];
};

HostDir *host_opendir(const char *path)
{
    char pattern[WPATH];
    wchar_t w[WPATH];
    size_t n = strlen(path);
    snprintf(pattern, sizeof pattern, "%s%s*", path, n && path[n - 1] == '/' ? "" : "/");
    if (!to_wide(pattern, w, WPATH)) return NULL;
    HostDir *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->h = FindFirstFileW(w, &d->data);
    if (d->h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND) { free(d); errno = ENOENT; return NULL; }
        d->have = 0;                  /* an empty folder (a drive root can have no entries) */
    } else {
        d->have = 1;
    }
    return d;
}

const char *host_readdir(HostDir *d)
{
    while (d->have) {
        int ok = from_wide(d->data.cFileName, d->name, sizeof d->name);
        d->have = FindNextFileW(d->h, &d->data) != 0;
        if (ok) return d->name;
    }
    return NULL;
}

void host_closedir(HostDir *d)
{
    if (d->h != INVALID_HANDLE_VALUE) FindClose(d->h);
    free(d);
}

/* Paths of the system. */
int host_realpath(const char *path, char *out, size_t room)
{
    wchar_t w[WPATH], full[WPATH];
    if (!to_wide(path, w, WPATH)) return 0;
    DWORD n = GetFullPathNameW(w, WPATH, full, NULL);
    if (!n || n >= WPATH) return 0;
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) { errno = ENOENT; return 0; }
    /* No trailing separator, except after the drive. */
    while (n > 3 && (full[n - 1] == L'\\' || full[n - 1] == L'/')) full[--n] = 0;
    return from_wide(full, out, room);
}

int host_program_path(char *out, size_t room)
{
    wchar_t w[WPATH];
    DWORD n = GetModuleFileNameW(NULL, w, WPATH);
    if (!n || n >= WPATH) return 0;
    return from_wide(w, out, room);
}

int host_current_dir(char *out, size_t room)
{
    wchar_t w[WPATH];
    DWORD n = GetCurrentDirectoryW(WPATH, w);
    if (!n || n >= WPATH) return 0;
    return from_wide(w, out, room);
}

int host_user_dir(char *out, size_t room)
{
    wchar_t w[MAX_PATH];
    char base[WPATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, w) != S_OK || !from_wide(w, base, sizeof base)) {
        fprintf(stderr, "cm0102_recomp: Windows gives no AppData folder, so the user folder is unknown.\n");
        return 0;
    }
    return (size_t)snprintf(out, room, "%s/cm0102", base) < room;
}

int host_home_dir(char *out, size_t room)
{
    wchar_t w[MAX_PATH];
    char base[WPATH];
    if (SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, w) != S_OK || !from_wide(w, base, sizeof base)) return 0;
    return (size_t)snprintf(out, room, "%s", base) < room;
}

/* Drives. The CD drive is the first letter from D that the system does not use. */
char host_cd_drive(void)
{
    static char cd;
    if (!cd) {
        DWORD used = GetLogicalDrives();
        cd = 'D';
        for (char c = 'D'; c <= 'Z'; c++) if (!(used & (1u << (c - 'A')))) { cd = c; break; }
    }
    return cd;
}

int host_has_drive(char letter)
{
    if (letter < 'A' || letter > 'Z') return 0;
    return (GetLogicalDrives() & (1u << (letter - 'A'))) != 0;
}

/* Time. */
void host_gmtime(time_t t, struct tm *out)
{
    __time64_t t64 = t;
    if (_gmtime64_s(out, &t64)) memset(out, 0, sizeof *out);
}

void host_localtime(time_t t, struct tm *out)
{
    __time64_t t64 = t;
    if (_localtime64_s(out, &t64)) memset(out, 0, sizeof *out);
}

int32_t host_bias_minutes(void)
{
    TIME_ZONE_INFORMATION tz;
    DWORD r = GetTimeZoneInformation(&tz);
    if (r == TIME_ZONE_ID_INVALID) return 0;
    return (int32_t)tz.Bias + (int32_t)(r == TIME_ZONE_ID_DAYLIGHT ? tz.DaylightBias : r == TIME_ZONE_ID_STANDARD ? tz.StandardBias : 0);
}

uint64_t host_unix_time_us(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
    return t / 10u - 11644473600000000ull;
}

/* The display report (host.h). The newer Windows calls come from the system DLLs by name, so the game
 * still starts on a Windows that lacks them. */
typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW *);
typedef const char *(CDECL *WineVersionFn)(void);

void host_system_name(char *out, size_t room)
{
    OSVERSIONINFOW v;
    memset(&v, 0, sizeof v);
    v.dwOSVersionInfoSize = sizeof v;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn get = ntdll ? (RtlGetVersionFn)(void *)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    if (get) get(&v);
    WineVersionFn wine = ntdll ? (WineVersionFn)(void *)GetProcAddress(ntdll, "wine_get_version") : NULL;
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    const char *cpu = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x86_64"
                    : si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? "arm64" : "other";
    /* Windows 11 still says 10.0; its builds start at 22000. */
    const char *name = v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000 ? "Windows 11" : "Windows";
    snprintf(out, room, "%s %lu.%lu build %lu, %s%s%s\n", name, (unsigned long)v.dwMajorVersion,
             (unsigned long)v.dwMinorVersion, (unsigned long)v.dwBuildNumber, cpu,
             wine ? ", Wine " : "", wine ? wine() : "");
}

typedef HANDLE (WINAPI *GetContextFn)(void);
typedef HANDLE (WINAPI *SetContextFn)(HANDLE);
typedef int (WINAPI *AwarenessFn)(HANDLE);
typedef HRESULT (WINAPI *MonitorDpiFn)(HMONITOR, int, UINT *, UINT *);
#define PER_MONITOR_AWARE_V2 ((HANDLE)(intptr_t)-4)
#define PER_MONITOR_AWARE ((HANDLE)(intptr_t)-3)

typedef struct { char *out; size_t room, len; int n, unaware; MonitorDpiFn dpi; SetContextFn set; } Notes;

static void notes_add(Notes *s, const char *fmt, ...)
{
    if (s->len >= s->room) return;
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(s->out + s->len, s->room - s->len, fmt, ap);
    va_end(ap);
    if (k > 0) s->len += (size_t)k;
}

static BOOL CALLBACK note_monitor(HMONITOR m, HDC dc, LPRECT r, LPARAM p)
{
    (void)dc; (void)r;
    Notes *s = (Notes *)p;
    MONITORINFOEXW mi;
    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(m, (MONITORINFO *)&mi)) return TRUE;
    s->n++;
    DEVMODEW dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    int real = EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm);
    /* The scale of the screen as Windows sets it, read as a program that handles the scale itself. */
    UINT dx = 0, dy = 0;
    /* Windows 10 1703 and later have V2, 1607 has only the first per-screen context. Without either the call
     * gives 96 for an unaware game, so the scale then comes from the real pixels against the size the game sees. */
    HANDLE old = s->set ? s->set(PER_MONITOR_AWARE_V2) : NULL;
    if (!old && s->set) old = s->set(PER_MONITOR_AWARE);
    if (!old || !s->dpi || s->dpi(m, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy) != S_OK) dx = 0;
    if (old) s->set(old);
    long seen_w = (long)(mi.rcMonitor.right - mi.rcMonitor.left);
    int from_pixels = 0;
    if (!dx && real && s->unaware && seen_w > 0) {
        dx = (UINT)((long)dm.dmPelsWidth * 96 / seen_w);
        from_pixels = 1;
    }
    notes_add(s, "Windows screen %d%s: %ld x %ld as the game sees it", s->n,
              (mi.dwFlags & MONITORINFOF_PRIMARY) ? " (main)" : "",
              (long)(mi.rcMonitor.right - mi.rcMonitor.left), (long)(mi.rcMonitor.bottom - mi.rcMonitor.top));
    if (real) notes_add(s, ", %lu x %lu real pixels at %lu Hz", (unsigned long)dm.dmPelsWidth,
                        (unsigned long)dm.dmPelsHeight, (unsigned long)dm.dmDisplayFrequency);
    if (dx) notes_add(s, ", scale %u%%%s", dx * 100u / 96u, from_pixels ? " (from the pixels)" : "");
    else notes_add(s, ", scale unknown");
    notes_add(s, "\n");
    if (dx > 96 && s->unaware) notes_add(s, "  Windows stretches the game window on this screen.\n");
    return TRUE;
}

void host_display_notes(char *out, size_t room)
{
    Notes s;
    memset(&s, 0, sizeof s);
    s.out = out;
    s.room = room;
    out[0] = 0;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    GetContextFn get = user32 ? (GetContextFn)(void *)GetProcAddress(user32, "GetThreadDpiAwarenessContext") : NULL;
    AwarenessFn awareness = user32 ? (AwarenessFn)(void *)GetProcAddress(user32, "GetAwarenessFromDpiAwarenessContext") : NULL;
    s.set = user32 ? (SetContextFn)(void *)GetProcAddress(user32, "SetThreadDpiAwarenessContext") : NULL;
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    s.dpi = shcore ? (MonitorDpiFn)(void *)GetProcAddress(shcore, "GetDpiForMonitor") : NULL;
    int level = get && awareness ? awareness(get()) : (IsProcessDPIAware() ? 1 : 0);
    static const char *const level_name[] = {"no (Windows scales the window)", "system", "per screen"};
    s.unaware = level == 0;
    notes_add(&s, "Game handles the Windows scale: %s\n", level >= 0 && level <= 2 ? level_name[level] : "unknown");
    EnumDisplayMonitors(NULL, NULL, note_monitor, (LPARAM)&s);
    DISPLAY_DEVICEW dd;
    for (DWORD i = 0; ; i++) {
        memset(&dd, 0, sizeof dd);
        dd.cb = sizeof dd;
        if (!EnumDisplayDevicesW(NULL, i, &dd, 0)) break;
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        char name[256];
        if (!from_wide(dd.DeviceString, name, sizeof name)) continue;
        notes_add(&s, "Graphics: %s\n", name);
    }
    if (shcore) FreeLibrary(shcore);
}

/* The folder picker: the shell's Browse For Folder box (shell32, as SHGetFolderPathW above). The classic box
 * needs no COM set-up. */
int host_pick_folder(const char *title, char *out, size_t room)
{
    wchar_t wtitle[256], path[MAX_PATH];
    if (!to_wide(title, wtitle, 256)) wtitle[0] = 0;
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof bi);
    bi.lpszTitle = wtitle;
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    LPITEMIDLIST id = SHBrowseForFolderW(&bi);
    if (!id) return 0;
    int ok = SHGetPathFromIDListW(id, path) && from_wide(path, out, room);
    ILFree(id);
    if (!ok) return 0;
    size_t n = strlen(out);
    while (n > 3 && out[n - 1] == '/') out[--n] = 0;
    return 1;
}
