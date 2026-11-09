/* Shared parts of the Windows replacements of the recompiled game (src/recomp_rt/win). Every import is
 * stdcall: a replacement reads its arguments with rt_arg and returns with rt_ret_stdcall. */
#ifndef RECOMP_WIN_H
#define RECOMP_WIN_H

#include "rt.h"
#include "rt_host.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Strings in flat memory. */
static inline char *win_str(uint32_t a) { return (char *)(g_mem + a); }
static inline void win_put_str(uint32_t dst, const char *s, uint32_t room)
{
    if (!room) return;
    size_t n = strlen(s);
    if (n >= room) n = room - 1;
    memcpy(g_mem + dst, s, n);
    g_mem[dst + n] = 0;
}

/* Settings of the run (main.c). */
extern char win_game_dir[1024];        /* host folder of the original: cwd, registry Location */
extern char win_user_dir[1024];        /* host folder of every write: ~/Library/Application Support/cm0102 */
const char *win_save_folder(void);   /* files.c: the folder of the saved games (save_folder or the user folder) */
int win_game_date(int *year, int *month, int *day);   /* savename.c: the game date, month and day from 1; 0 when unknown */
extern char win_exe_path[1024];        /* host path of the player's cm0102.exe: the CD drive D:\ */
extern char win_command_line[1024];    /* the Windows command line: "cm0102.exe" and the options */
extern int win_trace;                  /* RECOMP_TRACE: print each import call */

/* GetLastError. */
extern uint32_t win_last_error;
#define ERROR_FILE_NOT_FOUND 2u
#define ERROR_PATH_NOT_FOUND 3u
#define ERROR_ACCESS_DENIED 5u
#define ERROR_INVALID_HANDLE 6u
#define ERROR_NO_MORE_FILES 18u
#define ERROR_ALREADY_EXISTS 183u
#define ERROR_FILE_EXISTS 80u
void win_error_from_errno(void);

/* Paths (files.c). A Windows path maps to a host path: C:\ is the host root, D:\ is the CD drive, a
 * relative path starts at the current folder. Each part is matched without regard to case. */
int win_host_path(const char *winpath, char *out, size_t room);
/* The host path that a read of a Windows path opens (the user folder first, then the game folder); 0 when
 * the file does not exist. */
int win_existing_file(const char *winpath, char *out, size_t room);
/* The Windows path of a host folder (C:\ plus the host path). */
void win_windows_path(const char *host, char *out, size_t room);
extern char win_cwd[1024];             /* current folder, as a Windows path */

/* Handles of files (files.c). */
int win_is_file_handle(uint32_t h);
int win_close_file(uint32_t h);
/* A save of the game (season.c): between the two calls, a write that would take a file past 4 GB makes the save
 * fail. win_save_end gives 0 for a good save, 1 when the save failed and the earlier save stays, 2 when the save
 * failed after the game deleted the earlier save of the name. */
void win_save_begin(void);
int win_save_end(void);
/* Memory files (season_records.c, 0.6.0 slice 15b): a read-only open of the host path gives these bytes in place
 * of the file on disk. The caller keeps the buffer until win_memory_files_clear. */
void win_memory_file(const char *host_path, const uint8_t *data, size_t size);
void win_memory_files_clear(void);
/* The .lng files fitted to the database (season_lng.c): at the end of the data load of a new game. fitted: 1 when
 * that load wrote them. */
void season_lng_fit_temp(void);
int season_lng_fitted(void);

/* Memory that the runtime owns and never frees, zeroed (kernel32.c). */
uint32_t win_static_alloc(uint32_t size);

/* Time (kernel32.c): 100 ns units since 1601 from the host clock; the SYSTEMTIME of a struct tm; the
 * host's minutes behind UTC. */
struct tm;
uint64_t win_filetime_now(void);
void win_put_systemtime(uint32_t p, const struct tm *t, uint32_t ms);
int32_t win_bias_minutes(void);
/* 1 when RECOMP_FIXED_CLOCK is set (test runs): repeatable clocks, see kernel32.c. */
int win_fixed_clock(void);

/* The settings file of the user folder: the display mode sets rt_screen_w and rt_screen_h (display.c). */
void win_read_settings(void);
/* The option database changed: the next file open finds the data set folder again (files.c). */
void win_data_set_changed(void);
const char *win_data_set_dir(void);     /* the host folder of the data set, "" for the game's own Data */
const char *win_databases_root(void);   /* the folder that holds Databases: the save folder */

/* Sound (dsound.c): 0 mutes every sound while the window lacks focus, 1 makes them heard again. */
void win_sound_focus(int focused);

/* Window, input and screen (user32.c, ddraw.c). */
void win_video_init(void);
/* Stores the window size, place and full screen state in window.txt (option remember_window). Call it at quit. */
void win_save_window(void);
void win_present(void);                /* show the primary surface */
void win_poll_events(void);            /* SDL events to the message queue */
void win_game_work(void);              /* user32.c: game work ran in this pass, no idle sleep (#58) */
/* user32.c: a hook on the game's random function 0x90cfc0 (cdecl (range)): called after each call with the esp of
 * the call (the return address is at [esp]), the caller's edi and the result, returns the result to use. NULL
 * when no code asks for it (season_uefa_qual.c, the Champions League group draw). */
extern uint32_t (*win_random_hook)(uint32_t esp, uint32_t edi, uint32_t result);
uint32_t win_back_buffer(void);        /* flat address of the RGB565 back buffer, or 0 */
void win_dump_screen(const char *path);
extern uint32_t win_wndproc;

/* Places on the screen of the game for the keys (keys.c). Each gives the place to click in *x, *y and
 * returns 1, or returns 0 when the key has nothing to press now. */
extern uint32_t keys_display;                  /* the screen manager of the last input poll */
int keys_typing(void);                         /* 1 while a text entry takes keys */
int keys_continue_game(int *x, int *y);        /* Continue Game in the left bar */
int keys_back(int *x, int *y);                 /* the Back arrow in the left bar */
int keys_outside_popup(int *x, int *y);        /* a place outside the open popup */
int keys_left_bar_label(const char *prefix);   /* a left bar button with this text (waits.c) */
void keys_left_bar_labels(char *out, size_t size); /* the texts of the left bar buttons, for the log */
extern uint32_t win_hwnd;

/* Holiday until next match (holiday_next.c). */
int holiday_widget_before(void);               /* at each widget create: 1 for the Go on Holiday item */
void holiday_widget_after(void);               /* after that create: adds the new item */
void screen_sweep_turn(void);                  /* RECOMP_SCREEN_SWEEP, at each human turn (screen_sweep.c) */
void holiday_next_turn(void);                  /* at each human turn, before the original */
/* The Bosman Edition Options box (options_screen.c). */
uint32_t options_screen_build_fn(void);        /* the build function of the box, for the wait state "options" */
int options_screen_open(int from);             /* pushes the box (from: 1 start screen, 2 game); 0 when it fails */
void options_screen_after_poll(uint32_t ret, uint32_t display, uint32_t widget);   /* after each input poll */
int options_screen_widget_before(void);        /* at each widget create: 1 for the Game Settings item */
void options_screen_widget_after(void);        /* after that create: adds the Bosman Edition Options item */
uint32_t win_press_count(void);                /* user32.c: the count of mouse presses so far */
void win_sharp_picture_changed(void);          /* user32.c: applies the option sharp_picture at once */
void win_picture_way(char *out, size_t n);    /* user32.c: how the picture is scaled now, for the display report */
int win_short_screen(void);                    /* user32.c: widescreen on a screen less than 800 pixels high */
/* The Assistant's picks (picker.c). picker_compute picks without writing; picker_pick stores the pick as
 * the club's selection. kind 1: best team, 2: most match-fit, 3: under-21. Both return the players picked. */
#define PICK_SLOTS 20
#include "pick_score.h"                /* PICK_UNSCORED: the value of a player whose key attributes are all hidden */
typedef struct {
    int slots;                     /* 11 and the subs */
    uint16_t masks[PICK_SLOTS];    /* the position mask of each slot */
    uint32_t staff[PICK_SLOTS];    /* 0 for an empty slot */
    int fit[PICK_SLOTS];           /* 1 to 20 */
    int score[PICK_SLOTS];         /* key attribute average x fit / 20, times 1200 */
    /* The candidates outside the eleven: the subs in slot order, then the others by their best value. For
     * each, the slot of the 11 with his highest value: the value (as score), the fit and the slot's position
     * bit with its side bits. */
    int bench;
    uint32_t bench_staff[50];
    int bench_fit[50];
    int bench_score[50];
    uint16_t bench_mask[50];
} Pick;
int picker_compute(uint32_t club, uint32_t team_index, int kind, Pick *out);
int picker_compute_view(uint32_t club, uint32_t team_index, int kind, const uint16_t *formation, uint32_t viewer,
                        Pick *out);
/* Squad depth (picker.c, for squad_depth.c; Q106, Q107, Q161): rows of the 11 slots of a formation,
 * row_of[s] is the row of slot s (0 to rows - 1). Each row lists the k + 2 players of the squad with the
 * highest value in its k slots, unavailable players too. xi is the best XI of the same formation: its
 * players in the row's slots come before every other available player of that row. */
#define DEPTH_PLAYERS (11 + 2)
typedef struct {
    int n[11];
    uint32_t staff[11][DEPTH_PLAYERS];
    int fit[11][DEPTH_PLAYERS];
    int score[11][DEPTH_PLAYERS];  /* as Pick.score: the best of the row's slots */
    int out[11][DEPTH_PLAYERS];    /* not 0: the player cannot play the next match: 1 injured, 2 away with a
                                      national team, 3 the squad screen's test (a ban, a loan, the rules) */
} Depth;
int picker_depth(uint32_t club, uint32_t team_index, const uint16_t *formation, uint32_t viewer, const Pick *xi,
                 const int *row_of, int rows, Depth *out);
int picker_pick(uint32_t club, uint32_t team_index, int kind);
/* The one masking rule of best XI, squad depth and the comparison: the value that a player's screens
 * show, 1 to 20, or 0 when the game hides it from the viewer (0 hides nothing). */
int picker_shown_attribute(uint32_t viewer, uint32_t staff, uint32_t k);
int picker_attribute_known(uint32_t viewer, uint32_t staff, uint32_t k);
int picker_check_turn(void);                   /* at each human turn: the picker check; 1 when done */
/* The Team views screen (views.c): pushes it for a club and team (0 = first team) of a squad screen. */
void views_open(uint32_t club, uint32_t team);
uint32_t columns_attribute_colour(int value);   /* 1 to 20: the attribute colour (RGB565), else 0 */
void views_widget_drawn(uint32_t widget);      /* after the game draws a widget: the pitch */
/* Faster commentary (commentary.c): at each widget create, before the rect is scaled. */
void commentary_widget_before(void);

/* The wait step of the script (waits.c). */
int waits_parse(const char *name, int *kind, uint32_t *value);   /* 0: no such state */
int waits_reached(int kind, uint32_t value, uint32_t since);    /* since: waits_poll_count() at the start */
uint32_t waits_poll_count(void);
void waits_describe(char *out, size_t size);
void waits_poll(void);                         /* at each input poll */

/* COM (com.c). An object starts with its vtable and a reference count at +4. The list of methods ends
 * with a NULL function. */
typedef struct { uint32_t offset; const char *name; RtFn fn; } WinMethod;
uint32_t win_make_vtable(const char *iface, int slots, const WinMethod *methods);
uint32_t win_make_object(uint32_t vtable, uint32_t size);
void win_com_addref(void);
void win_com_release(void);

/* Logging of import calls. */
#define WIN_TRACE(...) do { if (win_trace) fprintf(stderr, __VA_ARGS__); } while (0)

/* Clickable names and cursors (names.c). The SDL types stay out of this header. */
struct SDL_Renderer;
struct SDL_Surface;
int win_cursor_shape(uint32_t handle);          /* an SDL_SystemCursor for a cursor handle of the game */
void names_after_poll(void);                    /* after each input poll: the name under the mouse */
void names_draw(struct SDL_Renderer *renderer); /* the line under it, on the shown frame */
void names_draw_surface(struct SDL_Surface *s); /* the same line on a screen dump */
/* The SDL window, renderer and picture texture of user32.c, NULL before the window opens. */
struct SDL_Window;
struct SDL_Texture;
struct SDL_Window *win_sdl_window(void);
struct SDL_Renderer *win_sdl_renderer(void);
struct SDL_Texture *win_sdl_texture(void);
/* The display report (display_report.c). display_report_event notes a change of the window. display_report_write
 * writes display-report.txt in the user folder, puts its path in path and, when text is not NULL, the report in
 * *text; 0 when the file cannot be written. display_report_key is the work of the report key. */
void display_report_event(const char *what);
int display_report_write(const char *when, char *path, size_t room, char **text);
void display_report_key(void);

#endif
