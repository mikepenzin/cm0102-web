/* The cm0102:// link handler (0.7.0 slice 8, src/recomp_rt/host/link.c). ctest runs it on macOS (no handler)
 * and Linux (the desktop file of an AppImage). tests/net/link_container.sh runs the Windows build under wine, where it
 * writes and removes the registry key of the current user. */
#include "host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600        /* RegGetValueW and RegDeleteTreeW: Windows Vista and later */
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define COMMAND_KEY L"Software\\Classes\\cm0102\\shell\\open\\command"

static int command(wchar_t *out, DWORD room)
{
    DWORD bytes = room * sizeof out[0];
    return RegGetValueW(HKEY_CURRENT_USER, COMMAND_KEY, NULL, RRF_RT_REG_SZ, NULL, out, &bytes) == ERROR_SUCCESS;
}

static void test_link(void)
{
    wchar_t exe[MAX_PATH], want[MAX_PATH + 16], have[MAX_PATH + 16];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    _snwprintf(want, MAX_PATH + 16, L"\"%ls\" \"%%1\"", exe);

    CHECK(host_link_handler(1) == 1, "on: written");
    CHECK(command(have, MAX_PATH + 16) && !wcscmp(have, want), "the command starts this program with the link");
    DWORD bytes = sizeof have;
    CHECK(RegGetValueW(HKEY_CURRENT_USER, L"Software\\Classes\\cm0102", L"URL Protocol", RRF_RT_REG_SZ, NULL, have,
                       &bytes) == ERROR_SUCCESS && !have[0], "URL Protocol is there and empty");
    CHECK(host_link_handler(1) == 1, "on again: no change");

    /* Another program's handler stays when Online play is off. */
    HKEY k;
    RegCreateKeyExW(HKEY_CURRENT_USER, COMMAND_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
    const wchar_t *other = L"\"C:\\Other\\other.exe\" \"%1\"";
    RegSetValueExW(k, NULL, 0, REG_SZ, (const BYTE *)other, (DWORD)((wcslen(other) + 1) * sizeof other[0]));
    RegCloseKey(k);
    CHECK(host_link_handler(0) == 0, "off with another program's handler");
    CHECK(command(have, MAX_PATH + 16) && !wcscmp(have, other), "another program's handler stays");
    CHECK(host_link_handler(1) == 1 && command(have, MAX_PATH + 16) && !wcscmp(have, want), "on takes it back");

    CHECK(host_link_handler(0) == 0, "off: removed");
    CHECK(!command(have, MAX_PATH + 16), "no command is left");
    CHECK(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\cm0102", 0, KEY_READ, &k) != ERROR_SUCCESS,
          "no key is left");
    CHECK(host_link_handler(0) == 0, "off again: nothing to do");
}

#elif defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>

static void read_file(const char *path, char *out, size_t room)
{
    out[0] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    size_t n = fread(out, 1, room - 1, f);
    out[n] = 0;
    fclose(f);
}

static void test_link(void)
{
    char dir[] = "/tmp/link-test-XXXXXX", file[256], text[4096];
    if (!mkdtemp(dir)) { CHECK(0, "mkdtemp"); return; }
    setenv("XDG_DATA_HOME", dir, 1);
    snprintf(file, sizeof file, "%s/applications/cm0102-bosman-edition.desktop", dir);

    unsetenv("APPIMAGE");
    CHECK(host_link_handler(1) == 0, "not an AppImage: nothing");
    CHECK(access(file, F_OK) != 0, "not an AppImage: no file");

    setenv("APPIMAGE", "/home/a b/Games/CM \"x\" $HOME `y` 100%\\z.AppImage", 1);
    CHECK(host_link_handler(1) == 1, "on: written");
    read_file(file, text, sizeof text);
    CHECK(strstr(text, "MimeType=x-scheme-handler/cm0102;\n") != NULL, "the scheme");
    CHECK(strstr(text, "NoDisplay=true\n") != NULL, "not in the menu");
    /* The path in quotes, with " ` $ \ escaped for the Exec key and each \ doubled again for the string value. */
    CHECK(strstr(text, "Exec=\"/home/a b/Games/CM \\\\\"x\\\\\" \\\\$HOME \\\\`y\\\\` 100%%\\\\\\\\z.AppImage\" %u\n")
          != NULL, "the Exec line: %s", text);
    struct stat st1, st2;
    stat(file, &st1);
    sleep(1);
    CHECK(host_link_handler(1) == 1, "on again");
    stat(file, &st2);
    CHECK(st1.st_mtime == st2.st_mtime, "on again: not written again");

    setenv("APPIMAGE", "/opt/new.AppImage", 1);
    CHECK(host_link_handler(1) == 1, "a moved AppImage");
    read_file(file, text, sizeof text);
    CHECK(strstr(text, "Exec=\"/opt/new.AppImage\" %u\n") != NULL, "the Exec line follows the AppImage");

    setenv("APPIMAGE", "/opt/other.AppImage", 1);
    CHECK(host_link_handler(0) == 0 && access(file, F_OK) == 0, "off in another AppImage: the file stays");
    setenv("APPIMAGE", "/opt/new.AppImage", 1);
    CHECK(host_link_handler(0) == 0, "off: removed");
    CHECK(access(file, F_OK) != 0, "off: no file");
    CHECK(host_link_handler(0) == 0, "off again");

    setenv("APPIMAGE", "/opt/a\nb.AppImage", 1);
    CHECK(host_link_handler(1) == -1 && access(file, F_OK) != 0, "a line break in the path: no file");
    char apps[300];
    snprintf(apps, sizeof apps, "%s/applications", dir);
    rmdir(apps);
    rmdir(dir);
}

#else

static void test_link(void)
{
    CHECK(host_link_handler(1) == 0 && host_link_handler(0) == 0, "macOS: the Info.plist names the link");
}

#endif

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--write")) return host_link_handler(1) == 1 ? 0 : 1;   /* link_container.sh */
    test_link();
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("link: all checks pass\n");
    return 0;
}
