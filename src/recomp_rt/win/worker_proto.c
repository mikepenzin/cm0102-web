/* The engine worker protocol: parse, JSON lines and the line reader (worker_proto.h). */
#include "worker_proto.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* The length of s without the spaces at its end. */
static size_t trimmed(const char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    return n;
}

/* 1 when the word at s (length n) is w followed by the end or a space. */
static int word_is(const char *s, size_t n, const char *w)
{
    size_t k = strlen(w);
    return n >= k && memcmp(s, w, k) == 0 && (n == k || s[k] == ' ' || s[k] == '\t');
}

static WorkerKind bad(WorkerCommand *cmd, const char *why)
{
    cmd->kind = WK_BAD;
    cmd->error = why;
    return WK_BAD;
}

static int name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
           c == '_' || c == '.';
}

WorkerKind worker_parse(const char *line, WorkerCommand *cmd)
{
    memset(cmd, 0, sizeof *cmd);
    const char *s = skip_spaces(line);
    size_t n = trimmed(s);
    if (n == 0) return bad(cmd, "empty command");
    if (n == 5 && word_is(s, n, "state")) return cmd->kind = WK_STATE;
    if (n == 4 && word_is(s, n, "quit")) return cmd->kind = WK_QUIT;
    if (word_is(s, n, "save")) {
        const char *a = skip_spaces(s + 4);
        size_t len = n - (size_t)(a - s);
        if (len == 0) return bad(cmd, "save needs a name");
        if (len > WORKER_NAME_MAX) return bad(cmd, "save name too long");
        if (a[0] == '.') return bad(cmd, "save name starts with a dot");
        for (size_t i = 0; i < len; i++)
            if (!name_char(a[i])) return bad(cmd, "save name has a character other than letters, digits, space, - _ .");
        memcpy(cmd->name, a, len);
        cmd->name[len] = 0;
        return cmd->kind = WK_SAVE;
    }
    if (word_is(s, n, "advance")) {
        const char *a = skip_spaces(s + 7);
        size_t len = n - (size_t)(a - s);
        if (len == 0) return bad(cmd, "advance needs a number of days");
        int days = 0;
        for (size_t i = 0; i < len; i++) {
            if (a[i] < '0' || a[i] > '9') return bad(cmd, "advance takes a whole number of days");
            days = days * 10 + (a[i] - '0');
            if (days > WORKER_DAYS_MAX) return bad(cmd, "advance takes 1 to 400 days");
        }
        if (days < 1) return bad(cmd, "advance takes 1 to 400 days");
        cmd->days = days;
        return cmd->kind = WK_ADVANCE;
    }
    return bad(cmd, "unknown command");
}

/* A writer that stops at the end of the buffer: ok becomes 0 and stays 0. */
typedef struct {
    char *out;
    size_t room, len;
    int ok;
} Out;

static void put(Out *o, const char *fmt, ...)
{
    if (!o->ok) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(o->out + o->len, o->room - o->len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= o->room - o->len) { o->ok = 0; return; }
    o->len += (size_t)n;
}

/* A JSON string: '"', '\\' and the control characters escaped, other bytes (UTF-8) as they are. */
static void put_str(Out *o, const char *s)
{
    if (!s) { put(o, "null"); return; }
    put(o, "\"");
    for (; *s && o->ok; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') put(o, "\\%c", c);
        else if (c < 0x20) put(o, "\\u%04x", c);
        else put(o, "%c", c);
    }
    put(o, "\"");
}

static int finish(Out *o)
{
    put(o, "\n");
    return o->ok ? (int)o->len : -1;
}

static Out start(char *out, size_t room)
{
    Out o = {out, room, 0, room > 0};
    if (room) out[0] = 0;
    return o;
}

int worker_json_state(char *out, size_t room, int turn_event, const WorkerState *st)
{
    Out o = start(out, room);
    put(&o, turn_event ? "{\"event\":\"turn\"," : "{\"ok\":true,");
    put(&o, "\"year\":%d,\"month\":%d,\"day\":%d,\"club\":", st->year, st->month, st->day);
    put_str(&o, st->club);
    put(&o, "}");
    return finish(&o);
}

int worker_json_error(char *out, size_t room, const char *error)
{
    Out o = start(out, room);
    put(&o, "{\"ok\":false,\"error\":");
    put_str(&o, error);
    put(&o, "}");
    return finish(&o);
}

int worker_json_ok(char *out, size_t room)
{
    Out o = start(out, room);
    put(&o, "{\"ok\":true}");
    return finish(&o);
}

int worker_json_saved(char *out, size_t room, const char *host_path)
{
    Out o = start(out, room);
    put(&o, "{\"ok\":true,\"file\":");
    put_str(&o, host_path);
    put(&o, "}");
    return finish(&o);
}

int worker_json_until(char *out, size_t room, int serial_day)
{
    Out o = start(out, room);
    put(&o, "{\"ok\":true,\"until\":%d}", serial_day);
    return finish(&o);
}

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int worker_serial(int day_of_year, int year)
{
    int n = day_of_year;
    for (int y = 2000; y < year; y++) n += 365 + leap(y);
    for (int y = year; y < 2000; y++) n -= 365 + leap(y);
    return n;
}

int worker_read_line(WorkerLink *link, char *out, size_t room)
{
    for (;;) {
        char *nl = memchr(link->buf, '\n', link->len);
        if (nl) {
            size_t n = (size_t)(nl - link->buf), used = n + 1;
            if (n > 0 && link->buf[n - 1] == '\r') n--;
            if (n > WORKER_LINE_MAX || n + 1 > room) return -1;
            memcpy(out, link->buf, n);
            out[n] = 0;
            memmove(link->buf, link->buf + used, link->len - used);
            link->len -= used;
            return (int)n;
        }
        if (link->len >= WORKER_LINE_MAX + 2) return -1;     /* no newline in a full line */
        ssize_t got = read(link->fd, link->buf + link->len, sizeof link->buf - link->len);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return -1;
        link->len += (size_t)got;
    }
}

int worker_write_all(int fd, const char *text, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, text, len);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        text += n;
        len -= (size_t)n;
    }
    return 0;
}
