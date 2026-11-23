/* host_link_handler (host.h): the cm0102:// link of invite links (0.7.0 slice 8, #45).
 *
 * The handler lets a web page start this program, so it stays small: one key or one file, for the current
 * user, with no administrator rights. The game writes it at each start while Online play is on, so that it
 * follows the program when the player moves it or installs a new version, and removes it when Online play is
 * off. Where each one is and how a player removes it: the spec's slice 8, "As built"
 * (docs/specs/2026-10-27-online-play-0.7.0-spec.md). */
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600        /* RegGetValueW and RegDeleteTreeW: Windows Vista and later */
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define KEY L"Software\\Classes\\cm0102"
#define COMMAND_KEY KEY L"\\shell\\open\\command"

/* The command of the handler: "<this program>" "%1". 0 when the path is too long. */
static int our_command(wchar_t *out, DWORD room)
{
    wchar_t exe[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(NULL, exe, sizeof exe / sizeof exe[0]);
    if (!n || n >= sizeof exe / sizeof exe[0]) return 0;
    return _snwprintf(out, room, L"\"%ls\" \"%%1\"", exe) > 0;
}

/* The command that the key holds now, or an empty text. */
static void current_command(wchar_t *out, DWORD room)
{
    DWORD bytes = room * sizeof out[0];
    out[0] = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, COMMAND_KEY, NULL, RRF_RT_REG_SZ, NULL, out, &bytes) != ERROR_SUCCESS)
        out[0] = 0;
}

static int set_text(const wchar_t *key, const wchar_t *name, const wchar_t *value)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return 0;
    LONG r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof value[0]));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

int host_link_handler(int on)
{
    wchar_t want[MAX_PATH * 2 + 16], have[MAX_PATH * 2 + 16];
    if (!our_command(want, sizeof want / sizeof want[0])) {
        fprintf(stderr, "recomp: link: the program path is too long for the cm0102:// link\n");
        return -1;
    }
    current_command(have, sizeof have / sizeof have[0]);
    if (on) {
        if (!wcscmp(want, have)) return 1;
        if (!set_text(KEY, NULL, L"URL:CM 01/02: Bosman Edition invite link") || !set_text(KEY, L"URL Protocol", L"")
            || !set_text(COMMAND_KEY, NULL, want)) {
            fprintf(stderr, "recomp: link: cannot write the cm0102:// link in the registry\n");
            return -1;
        }
        fprintf(stderr, "recomp: link: cm0102:// links open this program (HKEY_CURRENT_USER\\Software\\Classes\\cm0102)\n");
        return 1;
    }
    if (wcscmp(want, have)) return 0;           /* none, or another program's: leave it */
    if (RegDeleteTreeW(HKEY_CURRENT_USER, KEY) != ERROR_SUCCESS) {           /* the key and all under it */
        fprintf(stderr, "recomp: link: cannot remove the cm0102:// link from the registry\n");
        return -1;
    }
    fprintf(stderr, "recomp: link: the cm0102:// link is removed\n");
    return 0;
}

#elif defined(__linux__)

#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILE_NAME "cm0102-bosman-edition.desktop"

extern char **environ;

/* The folder of the user's desktop files: $XDG_DATA_HOME/applications, else ~/.local/share/applications.
 * make 1 creates the missing folders. 0 when there is no home folder. */
static int apps_dir(char *out, size_t room, int make)
{
    const char *data = getenv("XDG_DATA_HOME");
    char home[1024];
    if (data && data[0] == '/') snprintf(out, room, "%s/applications", data);
    else if (host_home_dir(home, sizeof home)) snprintf(out, room, "%s/.local/share/applications", home);
    else return 0;
    if (make)
        for (char *p = out + 1; ; p++) {
            if (*p != '/' && *p) continue;
            char c = *p;
            *p = 0;
            mkdir(out, 0755);
            if (!c) break;
            *p = c;
        }
    return 1;
}

/* The Exec value for the AppImage at path (Desktop Entry Specification, "The Exec key"): the path in double
 * quotes, with \ " ` $ escaped, then each \ doubled for the string value, and % as %%. 0 when it does not fit
 * or the path holds a line break. */
static int exec_value(const char *path, char *out, size_t room)
{
    size_t n = 0;
#define PUT(c) do { if (n + 1 >= room) return 0; out[n++] = (c); } while (0)
    PUT('"');
    for (const char *p = path; *p; p++) {
        if (*p == '\n' || *p == '\r') return 0;
        if (*p == '\\') { PUT('\\'); PUT('\\'); PUT('\\'); PUT('\\'); }
        else if (*p == '"' || *p == '`' || *p == '$') { PUT('\\'); PUT('\\'); PUT(*p); }
        else if (*p == '%') { PUT('%'); PUT('%'); }
        else PUT(*p);
    }
    PUT('"');
#undef PUT
    out[n] = 0;
    return 1;
}

/* update-desktop-database refreshes mimeinfo.cache in the folder, which some desktops read. It is not on
 * every system, and the handler works without it on GNOME and KDE. */
static void refresh(const char *dir)
{
    char *argv[] = {"update-desktop-database", (char *)dir, NULL};
    pid_t pid;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    if (posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ) == 0) waitpid(pid, NULL, 0);
    posix_spawn_file_actions_destroy(&fa);
}

int host_link_handler(int on)
{
    const char *appimage = getenv("APPIMAGE");
    char dir[1100], file[1200], exec[4200], want[5000], have[5000] = "";
    if (!appimage || appimage[0] != '/') return 0;     /* only the AppImage has a fixed path to write */
    if (!apps_dir(dir, sizeof dir, 0)) return -1;
    snprintf(file, sizeof file, "%s/" FILE_NAME, dir);
    if (!exec_value(appimage, exec, sizeof exec)) {
        fprintf(stderr, "recomp: link: the AppImage path cannot go in a desktop file\n");
        return -1;
    }
    snprintf(want, sizeof want,
             "[Desktop Entry]\n"
             "Type=Application\n"
             "Name=CM 01/02: Bosman Edition\n"
             "Comment=Opens invite links to network games\n"
             "Exec=%s %%u\n"
             "NoDisplay=true\n"
             "Terminal=false\n"
             "MimeType=x-scheme-handler/cm0102;\n", exec);
    FILE *f = fopen(file, "rb");
    if (f) {
        size_t n = fread(have, 1, sizeof have - 1, f);
        have[n] = 0;
        fclose(f);
    }
    if (on) {
        if (!strcmp(want, have)) return 1;
        char tmp[1300];
        snprintf(tmp, sizeof tmp, "%s.new", file);
        apps_dir(dir, sizeof dir, 1);
        f = fopen(tmp, "wb");
        int ok = f && fputs(want, f) >= 0;
        if (f && fclose(f)) ok = 0;
        if (!ok || rename(tmp, file)) {
            remove(tmp);
            fprintf(stderr, "recomp: link: cannot write %s\n", file);
            return -1;
        }
        refresh(dir);
        fprintf(stderr, "recomp: link: cm0102:// links open this AppImage (%s)\n", file);
        return 1;
    }
    if (strcmp(want, have)) return 0;           /* none, or another AppImage's: leave it */
    if (remove(file)) {
        fprintf(stderr, "recomp: link: cannot remove %s\n", file);
        return -1;
    }
    refresh(dir);
    fprintf(stderr, "recomp: link: the cm0102:// link is removed (%s)\n", file);
    return 0;
}

#else

/* macOS: the app's Info.plist (CFBundleURLTypes) names the link, and Launch Services reads it. */
int host_link_handler(int on)
{
    (void)on;
    return 0;
}

#endif
