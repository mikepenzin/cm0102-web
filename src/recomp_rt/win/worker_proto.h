/* The engine worker protocol (worker.c): how a server drives the recompiled game with no person at the screen.
 *
 * The server starts the game with RECOMP_WORKER_FD set to a file descriptor that it holds the other end of (a
 * socket pair; Node gives one as stdio entry 3). The game writes one JSON object per line and reads one command
 * per line. At each human turn where the game waits for the server, it first writes
 *     {"event":"turn","year":2001,"month":7,"day":14,"club":"Arsenal"}
 * and then reads commands until one lets the game go on:
 *     state            {"ok":true,"year":...,"month":...,"day":...,"club":...}   the game waits on
 *     save <name>      {"ok":true,"file":"<host path>"}                          the game waits on
 *     advance <days>   {"ok":true,"until":<serial day>}                          the game goes on
 *     quit             {"ok":true}                                               the program ends
 * A command that the game cannot do gets {"ok":false,"error":"..."} and the game waits on. "club" is null when
 * the player's manager has no club. A serial day counts from 1.1.2000 (day 0).
 *
 * This file holds the parts that do not touch the game: the parse of a command, the JSON lines and the line
 * reader. tests/worker runs them on their own. */
#ifndef RECOMP_WORKER_PROTO_H
#define RECOMP_WORKER_PROTO_H

#include <stddef.h>

#define WORKER_NAME_MAX 60          /* bytes of a save name, without ".sav" */
#define WORKER_DAYS_MAX 400         /* the longest advance: a little more than a season */
#define WORKER_LINE_MAX 512

typedef enum { WK_STATE, WK_SAVE, WK_ADVANCE, WK_QUIT, WK_BAD } WorkerKind;

typedef struct {
    WorkerKind kind;
    int days;                          /* advance */
    char name[WORKER_NAME_MAX + 1];    /* save */
    const char *error;                 /* WK_BAD: why */
} WorkerCommand;

/* Parses one line without its newline. A save name holds letters, digits, space, '-', '_' and '.', does not
 * start with '.' or a space, and gets no ".sav" from the server. */
WorkerKind worker_parse(const char *line, WorkerCommand *cmd);

typedef struct {
    int year, month, day;              /* the game date, month and day from 1 */
    const char *club;                  /* UTF-8, or NULL for no club */
} WorkerState;

/* The JSON lines, each ending with '\n'. They return the length, or -1 when room is too small. */
int worker_json_state(char *out, size_t room, int turn_event, const WorkerState *st);
int worker_json_error(char *out, size_t room, const char *error);
int worker_json_ok(char *out, size_t room);
int worker_json_saved(char *out, size_t room, const char *host_path);
int worker_json_until(char *out, size_t room, int serial_day);

/* Days from 1.1.2000 to day (from 0) of year. */
int worker_serial(int day_of_year, int year);

/* Lines over a file descriptor. worker_read_line blocks until a whole line came: it returns its length without
 * the newline (a '\r' before it is dropped), or -1 when the other end closed or the line is longer than
 * WORKER_LINE_MAX. worker_write_all returns 0 when every byte went out. */
typedef struct {
    int fd;
    char buf[WORKER_LINE_MAX * 2];
    size_t len;
} WorkerLink;

int worker_read_line(WorkerLink *link, char *out, size_t room);
int worker_write_all(int fd, const char *text, size_t len);

#endif
