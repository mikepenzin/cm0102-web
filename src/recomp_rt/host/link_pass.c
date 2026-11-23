/* host_link_pass, host_link_listen and host_link_take (host.h): one copy of the program takes the cm0102:// links
 * of a user (0.7.0 slice 8, Q41).
 *
 * On Windows and Linux a click on an invite link starts the program again. The first copy listens on a local
 * channel of its own user: a named pipe on Windows, a Unix socket elsewhere. A copy that starts with a link gives
 * the link to the copy that listens and ends, before it reads the settings or opens a window. The channel never
 * uses a network address, so no other computer can reach it, and only its own user can connect to it. The name
 * comes from the user folder, so test runs with their own user folder (RECOMP_USER_DIR) do not meet the player's
 * copy.
 *
 * The message is the link and a line end, at most LINK_MAX bytes. The copy that listens answers "ok\n" when it
 * keeps the link, and the game opens it at its next event poll. A second link before that poll replaces the
 * first: the last link wins. The copy that listens waits READ_MS for a message, so a client that sends nothing
 * does not stop it. A copy that gets no answer within REPLY_MS starts as before and opens the link itself. */
#include "../host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINK_MAX 128
#define READ_MS 500
#define REPLY_MS 2000

/* FNV-1a of the user folder: part of the name of the channel. */
static uint32_t name_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

/* A message as a link: up to the line end, printable ASCII only. 0 when it is not one. */
static int clean_link(const char *in, size_t n, char *out, size_t room)
{
    size_t i = 0;
    for (; i < n && in[i] != '\n'; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c < 0x21 || c > 0x7e || i + 1 >= room) return 0;
        out[i] = (char)c;
    }
    if (i == n || i == 0) return 0;          /* no line end, or empty */
    out[i] = 0;
    return 1;
}

#ifdef _WIN32

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600        /* GetNamedPipeServerProcessId: Windows Vista and later */
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>

static char slot[LINK_MAX];
static volatile LONG have;                   /* 1: slot holds a link that the game did not take yet */
static CRITICAL_SECTION lock;
static wchar_t pipe_name[160];
static wchar_t *user_sid;                    /* the SID of this user as text (LocalFree) */

/* The SID of the user of this process as text. NULL on failure. */
static wchar_t *own_sid(HANDLE process)
{
    HANDLE token;
    wchar_t *text = NULL;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return NULL;
    union { TOKEN_USER u; char room[256]; } info;
    DWORD n;
    if (GetTokenInformation(token, TokenUser, &info, sizeof info, &n)) ConvertSidToStringSidW(info.u.User.Sid, &text);
    CloseHandle(token);
    return text;
}

/* \\.\pipe\cm0102-link-<user SID>-<hash of the user folder>. 0 when the SID is unknown. */
static int make_name(const char *user_dir)
{
    if (!user_sid) user_sid = own_sid(GetCurrentProcess());
    if (!user_sid) return 0;
    _snwprintf(pipe_name, sizeof pipe_name / sizeof pipe_name[0], L"\\\\.\\pipe\\cm0102-link-%ls-%08x", user_sid,
               (unsigned)name_hash(user_dir));
    pipe_name[sizeof pipe_name / sizeof pipe_name[0] - 1] = 0;
    return 1;
}

/* 1 when the process at the other end of the pipe runs as this user. Another user could make a pipe with the
 * same name first. */
static int server_is_ours(HANDLE p, ULONG *pid)
{
    HANDLE proc;
    int ours = 0;
    if (!GetNamedPipeServerProcessId(p, pid)) return 0;
    proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *pid);
    if (!proc) return 0;
    wchar_t *sid = own_sid(proc);
    ours = sid && user_sid && !wcscmp(sid, user_sid);
    if (sid) LocalFree(sid);
    CloseHandle(proc);
    return ours;
}

int host_link_pass(const char *user_dir, const char *link)
{
    char msg[LINK_MAX + 1], reply[4] = "";
    size_t n = strlen(link);
    ULONG pid = 0;
    if (n + 1 > LINK_MAX || !make_name(user_dir)) return 0;
    /* SECURITY_IDENTIFICATION: the other end may learn who connects, but may not act as this user. */
    DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION;
    HANDLE p = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, flags, NULL);
    if (p == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY && WaitNamedPipeW(pipe_name, REPLY_MS))
        p = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, flags, NULL);
    if (p == INVALID_HANDLE_VALUE) return 0;     /* no copy listens */
    if (!server_is_ours(p, &pid)) {
        CloseHandle(p);
        return 0;
    }
    snprintf(msg, sizeof msg, "%s\n", link);
    DWORD done = 0;
    int ok = WriteFile(p, msg, (DWORD)(n + 1), &done, NULL) && done == n + 1;
    if (ok) {
        DWORD avail = 0, waited = 0;
        while (waited < REPLY_MS && PeekNamedPipe(p, NULL, 0, NULL, &avail, NULL) && avail < 3) Sleep(20), waited += 20;
        ok = avail >= 3 && ReadFile(p, reply, 3, &done, NULL) && done == 3 && !memcmp(reply, "ok\n", 3);
    }
    CloseHandle(p);
    if (ok) AllowSetForegroundWindow(pid);       /* the copy that runs may bring its window to the front */
    return ok;
}

/* Waits for an overlapped call of p. 1 when it ends within ms. */
static int finish(HANDLE p, OVERLAPPED *o, BOOL started, DWORD ms, DWORD *n)
{
    if (started) return GetOverlappedResult(p, o, n, FALSE);
    DWORD e = GetLastError();
    if (e == ERROR_PIPE_CONNECTED) return 1;
    if (e != ERROR_IO_PENDING) return 0;
    if (WaitForSingleObject(o->hEvent, ms) == WAIT_OBJECT_0) return GetOverlappedResult(p, o, n, FALSE);
    CancelIo(p);
    GetOverlappedResult(p, o, n, TRUE);
    return 0;
}

/* One pipe handle for all clients: the name exists while the program runs, so a second copy always finds it. */
static DWORD WINAPI listen_thread(LPVOID pipe)
{
    HANDLE p = pipe;
    OVERLAPPED o;
    memset(&o, 0, sizeof o);
    o.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!o.hEvent) return 0;
    for (;;) {
        DWORD n = 0;
        ResetEvent(o.hEvent);
        if (!finish(p, &o, ConnectNamedPipe(p, &o), INFINITE, &n)) {
            DisconnectNamedPipe(p);
            Sleep(100);
            continue;
        }
        char buf[LINK_MAX], link[LINK_MAX];
        DWORD got = 0;
        while (got < sizeof buf && !memchr(buf, '\n', got)) {
            ResetEvent(o.hEvent);
            if (!finish(p, &o, ReadFile(p, buf + got, (DWORD)(sizeof buf - got), NULL, &o), READ_MS, &n) || !n) break;
            got += n;
        }
        if (clean_link(buf, got, link, sizeof link)) {
            EnterCriticalSection(&lock);
            memcpy(slot, link, sizeof slot);
            InterlockedExchange(&have, 1);
            LeaveCriticalSection(&lock);
            ResetEvent(o.hEvent);
            finish(p, &o, WriteFile(p, "ok\n", 3, NULL, &o), READ_MS, &n);
            FlushFileBuffers(p);
        }
        DisconnectNamedPipe(p);
    }
}

int host_link_listen(const char *user_dir)
{
    if (!make_name(user_dir)) return -1;
    /* Only this user may use the pipe (D:P, one entry, generic all for this SID). */
    wchar_t sddl[200];
    _snwprintf(sddl, sizeof sddl / sizeof sddl[0], L"D:P(A;;GA;;;%ls)", user_sid);
    sddl[sizeof sddl / sizeof sddl[0] - 1] = 0;
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, FALSE};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL))
        return -1;
    /* FILE_FLAG_FIRST_PIPE_INSTANCE fails when another copy has the pipe: then this copy does not listen. */
    HANDLE p = CreateNamedPipeW(pipe_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 64,
                                LINK_MAX, 0, &sa);
    LocalFree(sa.lpSecurityDescriptor);
    if (p == INVALID_HANDLE_VALUE) return -1;
    InitializeCriticalSection(&lock);
    HANDLE t = CreateThread(NULL, 0, listen_thread, p, 0, NULL);
    if (!t) {
        CloseHandle(p);
        return -1;
    }
    CloseHandle(t);
    return 0;
}

int host_link_take(char *out, size_t room)
{
    if (!InterlockedCompareExchange(&have, 0, 0)) return 0;
    EnterCriticalSection(&lock);
    snprintf(out, room, "%s", slot);
    InterlockedExchange(&have, 0);
    LeaveCriticalSection(&lock);
    return 1;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0                            /* macOS: SO_NOSIGPIPE on the socket (no_sigpipe) */
#endif

static char slot[LINK_MAX];
static int have;                                  /* __atomic: 1 while slot holds a link that the game did not take */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int server = -1;
static ino_t server_inode;
static struct sockaddr_un where;

/* $XDG_RUNTIME_DIR/cm0102-link-<hash>.sock (Linux, a folder of the user only), else <user folder>/link.sock.
 * 0 when the path does not fit in a socket address. */
static int make_address(const char *user_dir)
{
    const char *run = getenv("XDG_RUNTIME_DIR");
    int n;
    memset(&where, 0, sizeof where);
    where.sun_family = AF_UNIX;
    if (run && run[0] == '/')
        n = snprintf(where.sun_path, sizeof where.sun_path, "%s/cm0102-link-%08x.sock", run, (unsigned)name_hash(user_dir));
    else n = snprintf(where.sun_path, sizeof where.sun_path, "%s/link.sock", user_dir);
    return n > 0 && (size_t)n < sizeof where.sun_path;
}

/* A peer that closes early must not end the game with SIGPIPE, and a program that the game starts (xdg-open)
 * gets no copy of the socket. */
static void prepare(int s)
{
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    fcntl(s, F_SETFD, FD_CLOEXEC);
}

/* A socket connected to the copy that listens, or -1 with errno from connect. */
static int connect_to(void)
{
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    prepare(s);
    if (connect(s, (struct sockaddr *)&where, sizeof where)) {
        int e = errno;
        close(s);
        errno = e;
        return -1;
    }
    return s;
}

int host_link_pass(const char *user_dir, const char *link)
{
    char msg[LINK_MAX + 1], reply[4] = "";
    size_t n = strlen(link), got = 0;
    if (n + 1 > LINK_MAX || !make_address(user_dir)) return 0;
    int s = connect_to();
    if (s < 0) return 0;                          /* no copy listens */
    snprintf(msg, sizeof msg, "%s\n", link);
    int ok = send(s, msg, n + 1, MSG_NOSIGNAL) == (ssize_t)(n + 1);
    while (ok && got < 3) {
        struct pollfd pf = {s, POLLIN, 0};
        if (poll(&pf, 1, REPLY_MS) <= 0) break;
        ssize_t r = recv(s, reply + got, 3 - got, 0);
        if (r <= 0) break;
        got += (size_t)r;
    }
    close(s);
    return ok && got == 3 && !memcmp(reply, "ok\n", 3);
}

static void *listen_thread(void *unused)
{
    (void)unused;
    for (;;) {
        int c = accept(server, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR || errno == ECONNABORTED || errno == EMFILE || errno == ENFILE || errno == ENOBUFS
                || errno == ENOMEM) {
                usleep(100000);
                continue;
            }
            return NULL;
        }
        prepare(c);
        char buf[LINK_MAX], link[LINK_MAX];
        size_t got = 0;
        while (got < sizeof buf) {
            struct pollfd pf = {c, POLLIN, 0};
            if (poll(&pf, 1, READ_MS) <= 0) break;
            ssize_t r = recv(c, buf + got, sizeof buf - got, 0);
            if (r <= 0) break;
            got += (size_t)r;
            if (memchr(buf, '\n', got)) break;
        }
        if (clean_link(buf, got, link, sizeof link)) {
            pthread_mutex_lock(&lock);
            memcpy(slot, link, sizeof slot);
            __atomic_store_n(&have, 1, __ATOMIC_RELEASE);
            pthread_mutex_unlock(&lock);
            send(c, "ok\n", 3, MSG_NOSIGNAL);
        }
        close(c);
    }
}

/* At the end: remove the socket file, when it is still this copy's. */
static void remove_socket(void)
{
    struct stat st;
    if (server >= 0 && !stat(where.sun_path, &st) && st.st_ino == server_inode) unlink(where.sun_path);
}

/* Under the lock: listen, when no copy does. */
static int start_listening(void)
{
    int probe = connect_to();
    if (probe >= 0) {                             /* another copy listens */
        close(probe);
        return -1;
    }
    if (errno == ECONNREFUSED) unlink(where.sun_path);   /* a file that a copy left when it crashed */
    else if (errno != ENOENT) return -1;
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    prepare(s);
    mode_t old = umask(077);                      /* only this user may connect */
    int bad = bind(s, (struct sockaddr *)&where, sizeof where) || listen(s, 4);
    umask(old);
    struct stat st;
    if (bad || stat(where.sun_path, &st)) {
        close(s);
        return -1;
    }
    server = s;
    server_inode = st.st_ino;
    pthread_t t;
    if (pthread_create(&t, NULL, listen_thread, NULL)) {
        unlink(where.sun_path);
        close(s);
        server = -1;
        return -1;
    }
    pthread_detach(t);
    atexit(remove_socket);
    return 0;
}

int host_link_listen(const char *user_dir)
{
    if (!make_address(user_dir)) return -1;
    /* The lock file stops two copies that start at once from removing each other's socket. */
    char lock_path[sizeof where.sun_path + 8];
    snprintf(lock_path, sizeof lock_path, "%s.lock", where.sun_path);
    int l = open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (l < 0) return -1;
    if (flock(l, LOCK_EX)) {
        close(l);
        return -1;
    }
    int r = start_listening();
    flock(l, LOCK_UN);
    close(l);
    return r;
}

int host_link_take(char *out, size_t room)
{
    if (!__atomic_load_n(&have, __ATOMIC_ACQUIRE)) return 0;
    pthread_mutex_lock(&lock);
    snprintf(out, room, "%s", slot);
    __atomic_store_n(&have, 0, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&lock);
    return 1;
}

#endif
