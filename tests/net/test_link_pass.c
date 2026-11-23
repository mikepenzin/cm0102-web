/* One copy takes the cm0102:// links (0.7.0 slice 8, Q41, src/recomp_rt/host/link_pass.c). ctest runs it on macOS
 * and Linux. tests/net/link_container.sh also runs the Windows build under wine, in one process and as two
 * processes ("serve" and "pass"). The two-start run of the game checks the same channel with the real program. */
#include "host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#define pause_ms(ms) Sleep(ms)
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#define pause_ms(ms) usleep((ms) * 1000)
#endif

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Waits at most 1 s for a link. */
static int take(char *out, size_t room)
{
    for (int i = 0; i < 100; i++) {
        if (host_link_take(out, room)) return 1;
        pause_ms(10);
    }
    return 0;
}

/* Two processes (link_container.sh): "serve <dir>" listens and prints the first link within 10 s, "pass <dir>
 * <link>" gives a link and exits 0 when the copy that listens kept it. */
static int two_processes(int argc, char **argv)
{
    char got[128];
    if (argc == 3 && !strcmp(argv[1], "serve")) {
        if (host_link_listen(argv[2])) return 1;
        printf("listening\n");
        fflush(stdout);
        for (int i = 0; i < 10; i++)
            if (take(got, sizeof got)) {
                printf("took %s\n", got);
                return 0;
            }
        return 1;
    }
    if (argc == 4 && !strcmp(argv[1], "pass")) return host_link_pass(argv[2], argv[3]) ? 0 : 1;
    return 2;
}

/* A client that connects and sends nothing, to the channel of dir. Closed by silent_close. */
#ifdef _WIN32
typedef HANDLE Silent;
static Silent silent_open(const char *dir)
{
    HANDLE token;
    wchar_t *sid = NULL, name[160];
    union { TOKEN_USER u; char room[256]; } info;
    DWORD n;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, &info, sizeof info, &n);
    ConvertSidToStringSidW(info.u.User.Sid, &sid);
    uint32_t h = 2166136261u;
    for (const char *s = dir; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    _snwprintf(name, 160, L"\\\\.\\pipe\\cm0102-link-%ls-%08x", sid, (unsigned)h);
    return CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
}
static int silent_ok(Silent s) { return s != INVALID_HANDLE_VALUE; }
static void silent_close(Silent s) { CloseHandle(s); }
#else
typedef int Silent;
static Silent silent_open(const char *dir)
{
    struct sockaddr_un a = {0};
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s/link.sock", dir);
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(s, (struct sockaddr *)&a, sizeof a)) {
        close(s);
        return -1;
    }
    return s;
}
static int silent_ok(Silent s) { return s >= 0; }
static void silent_close(Silent s) { close(s); }

/* A socket file that a crashed copy left: bound, then closed without unlink. */
static void stale_socket(const char *dir)
{
    struct sockaddr_un a = {0};
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s/link.sock", dir);
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(s, (struct sockaddr *)&a, sizeof a);
    close(s);
}

/* In a child: with XDG_RUNTIME_DIR, the socket is there and not in the user folder. */
static void test_runtime_dir(const char *dir)
{
    char run[] = "/tmp/link-run-XXXXXX";
    if (!mkdtemp(run)) { CHECK(0, "mkdtemp"); return; }
    pid_t pid = fork();
    if (!pid) {
        char got[128], path[256];
        setenv("XDG_RUNTIME_DIR", run, 1);
        int ok = host_link_listen(dir) == 0 && host_link_pass(dir, "cm0102://join/K7M2Q9RT") == 1 && take(got, sizeof got);
        struct stat st;
        snprintf(path, sizeof path, "%s/link.sock", dir);
        ok = ok && stat(path, &st) != 0;
        _exit(ok ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "XDG_RUNTIME_DIR holds the socket");
    char cmd[300];
    snprintf(cmd, sizeof cmd, "rm -rf %s", run);
    system(cmd);
}
#endif

int main(int argc, char **argv)
{
    if (argc > 1) return two_processes(argc, argv);
    char dir[200], other[220], got[128], longer[300];
#ifdef _WIN32
    snprintf(dir, sizeof dir, "C:/users/test/cm0102-%lu", (unsigned long)GetCurrentProcessId());
#else
    char tmpl[] = "/tmp/link-pass-XXXXXX";
    if (!mkdtemp(tmpl)) return 2;
    snprintf(dir, sizeof dir, "%s", tmpl);
    unsetenv("XDG_RUNTIME_DIR");
    test_runtime_dir(dir);
    stale_socket(dir);
#endif
    snprintf(other, sizeof other, "%s-other", dir);

    CHECK(host_link_pass(dir, "cm0102://join/K7M2Q9RT") == 0, "no copy listens: the link stays here");
    CHECK(host_link_take(got, sizeof got) == 0, "nothing to take");

    CHECK(host_link_listen(dir) == 0, "the first copy listens (over a file that a crashed copy left)");
    CHECK(host_link_listen(dir) == -1, "a second copy does not listen");

    CHECK(host_link_pass(dir, "cm0102://join/K7M2Q9RT") == 1, "a second copy gives the link");
    CHECK(take(got, sizeof got) && !strcmp(got, "cm0102://join/K7M2Q9RT"), "the first copy takes it: %s", got);
    CHECK(host_link_take(got, sizeof got) == 0, "once");

    CHECK(host_link_pass(dir, "cm0102://join/ABCDEFGH/") == 1 && host_link_pass(dir, "cm0102://join/22222222") == 1,
          "two links close together");
    CHECK(take(got, sizeof got) && !strcmp(got, "cm0102://join/22222222"), "the last link wins: %s", got);
    CHECK(host_link_take(got, sizeof got) == 0, "one link to take");

    Silent s = silent_open(dir);
    CHECK(silent_ok(s), "a client that sends nothing connects");
    CHECK(host_link_pass(dir, "cm0102://join/K7M2Q9RT") == 1, "a client that sends nothing does not stop the channel");
    CHECK(take(got, sizeof got) && !strcmp(got, "cm0102://join/K7M2Q9RT"), "the link after it: %s", got);
    if (silent_ok(s)) silent_close(s);

    CHECK(host_link_pass(dir, "cm0102://join/a b") == 0, "a link with a space is refused");
    CHECK(host_link_pass(dir, "") == 0, "an empty link is refused");
    memset(longer, 'A', 199);
    longer[199] = 0;
    CHECK(host_link_pass(dir, longer) == 0, "a long text is not sent");
    pause_ms(200);
    CHECK(host_link_take(got, sizeof got) == 0, "nothing came from the refused links");

    CHECK(host_link_pass(other, "cm0102://join/K7M2Q9RT") == 0, "another user folder has its own channel");
#ifndef _WIN32
    memset(longer, 'd', sizeof longer - 1);
    longer[0] = '/';
    longer[sizeof longer - 1] = 0;
    CHECK(host_link_pass(longer, "cm0102://join/K7M2Q9RT") == 0, "a folder too long for a socket: no pass");

    char path[260];
    snprintf(path, sizeof path, "%s/link.sock", dir);
    unlink(path);
    snprintf(path, sizeof path, "%s/link.sock.lock", dir);
    unlink(path);
    rmdir(dir);
#endif
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("link_pass: all checks pass\n");
    return 0;
}
