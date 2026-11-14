/* cm0102_recomp: runs the recompiled original from its entry point, WinMainCRTStartup (0x947a13).
 *
 * Usage: cm0102_recomp [--game <folder>] [options of the original, for example -windowed]
 * The game folder holds the player's own cm0102.exe, Data and Pictures. It is the registry value
 * Location, the base of every path and the CD drive (files.c). Default: Contents/Resources/game inside
 * the Mac app bundle, then the folder "game" beside the program (the Windows installer), then the first
 * folder above the program that holds cm0102.exe and Data, else the current folder. The game folder is
 * read only: every write goes to the user folder (files.c): ~/Library/Application Support/cm0102 on
 * macOS, %APPDATA%\cm0102 on Windows.
 * Environment: RECOMP_TRACE=1 (print import calls), RECOMP_SCRIPT and RECOMP_DUMP
 * (user32.c), RECOMP_NO_BOX=1 (message boxes only on stderr), RECOMP_USER_DIR (another user folder,
 * for test runs). */
#include "win.h"
#include "host.h"
#include "season.h"
#include "season_rules.h"
#include "season_names.h"
#include "season_clubs.h"
#include "season_records.h"

#include <SDL.h>
#include <stdlib.h>

char win_game_dir[1024];
char win_exe_path[1024];
char win_command_line[1024];
char **win_argv;
int win_trace;

/* The game's stack: 16 MB below the runtime's memory. */
#define GAME_STACK_BASE 0x10000000u
#define GAME_STACK_SIZE 0x01000000u
#define GAME_STACK_TOP (GAME_STACK_BASE + GAME_STACK_SIZE - 0x100u)

void f_00947a13(void);

static void at_end(void)
{
    const char *dump = getenv("RECOMP_DUMP");
    if (dump) win_dump_screen(dump);
}

static int has_game(const char *dir)
{
    char p[1200];
    HostStat st;
    snprintf(p, sizeof p, "%s/cm0102.exe", dir);
    if (host_stat(p, &st) || st.is_dir) return 0;
    snprintf(p, sizeof p, "%s/Data", dir);
    return host_stat(p, &st) == 0 && st.is_dir;
}

/* The default game folder: Contents/Resources/game in the Mac app bundle, then the folder "game" beside
 * the program (where the Windows installer puts the clean files), then each folder upwards from the
 * program, else the current folder. */
static void default_game_dir(char *out, size_t room)
{
    char real[1024];
    if (host_program_path(real, sizeof real)) {
        char beside[1100];
        char *slash = strrchr(real, '/');
        if (slash) {
            char bundle[1024];
            snprintf(beside, sizeof beside, "%.*s/../Resources/game", (int)(slash - real), real);
            if (host_realpath(beside, bundle, sizeof bundle) && has_game(bundle)) { snprintf(out, room, "%s", bundle); return; }
            snprintf(beside, sizeof beside, "%.*s/game", (int)(slash - real), real);
            if (has_game(beside)) { snprintf(out, room, "%s", beside); return; }
        }
        for (; (slash = strrchr(real, '/')) != NULL;) {
            *slash = 0;
            if (!real[0]) break;
            if (has_game(real)) { snprintf(out, room, "%s", real); return; }
        }
    }
    if (!host_current_dir(out, room)) snprintf(out, room, ".");
}

int main(int argc, char **argv)
{
    win_argv = argv;
    char game[1024] = "";
    int first = 1;
    if (argc > 1 && !strcmp(argv[1], "--game")) {
        if (argc < 3) {
            fprintf(stderr, "cm0102_recomp: --game needs the folder of the original game.\n");
            return 2;
        }
        snprintf(game, sizeof game, "%s", argv[2]);
        first = 3;
    } else {
        default_game_dir(game, sizeof game);
    }
    char exe_path[1100];
    snprintf(exe_path, sizeof exe_path, "%s/cm0102.exe", game);
    if (!host_realpath(game, win_game_dir, sizeof win_game_dir)
        || !host_realpath(exe_path, win_exe_path, sizeof win_exe_path)) {
        fprintf(stderr, "cm0102_recomp: cannot find cm0102.exe in the folder %s. "
                        "Give the folder of the original game with --game <folder>.\n", game);
        return 2;
    }
    const char *user = getenv("RECOMP_USER_DIR");   /* test runs keep the player's save folder clean */
    if (user && *user) snprintf(win_user_dir, sizeof win_user_dir, "%s", user);
    else if (!host_user_dir(win_user_dir, sizeof win_user_dir)) return 2;
    {
        char tmp[1100];
        snprintf(tmp, sizeof tmp, "%s", win_user_dir);
        for (char *p = tmp + 1; ; p++) {
            if (*p != '/' && *p) continue;
            char c = *p;
            *p = 0;
            host_mkdir(tmp);
            if (!c) break;
            *p = c;
        }
        HostStat st;
        if (host_stat(win_user_dir, &st) || !st.is_dir) {
            fprintf(stderr, "cm0102_recomp: cannot create the user folder %s.\n", win_user_dir);
            return 2;
        }
    }
    win_trace = getenv("RECOMP_TRACE") && *getenv("RECOMP_TRACE");
    win_read_settings();
    season_init();                       /* RECOMP_START_SEASON (season.c) */
    season_rules_init(win_game_dir);     /* Seasons/rules.tsv (season_rules.c) */
    season_names_init(win_game_dir);     /* Seasons/names.tsv (season_names.c) */
    season_clubs_init(win_game_dir);     /* Seasons/clubs.tsv (season_clubs.c) */
    season_records_init(win_game_dir);   /* Seasons/club_records.tsv (season_records.c) */
    season_report_init();                /* RECOMP_SEASON_JUMP, RECOMP_RULES_REPORT (season_report.c) */

    /* The command line as Windows gives it: the quoted program path, then the options. */
    char exe_win[1100];
    win_windows_path(win_exe_path, exe_win, sizeof exe_win);
    size_t len = (size_t)snprintf(win_command_line, sizeof win_command_line, "\"%s\"", exe_win);
    for (int i = first; i < argc && len < sizeof win_command_line; i++)
        len += (size_t)snprintf(win_command_line + len, sizeof win_command_line - len, " %s", argv[i]);

#ifdef _WIN32
    /* Windows display scaling (125 %, 150 %): the game draws in real pixels, so that Windows does not
     * stretch the window and blur the text. Window sizes and places stay in scaled points, as on a Mac,
     * so that window.txt, the first window size and the mouse keep their units. It must come before
     * SDL_Init. */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "cm0102_recomp: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    rt_heap_bump = 0;
    rt_init(win_exe_path);
    rt_on_fatal = at_end;

    /* The current folder starts as the game folder, as for a shortcut to the game. */
    win_windows_path(win_game_dir, win_cwd, sizeof win_cwd);

    /* The thread information block at fs:0: the exception chain is empty, then the stack limits. */
    wr32(g_mem, RT_FS_BASE, 0xffffffffu);
    wr32(g_mem, RT_FS_BASE + 4, GAME_STACK_BASE + GAME_STACK_SIZE);
    wr32(g_mem, RT_FS_BASE + 8, GAME_STACK_BASE);
    wr32(g_mem, RT_FS_BASE + 0x18, RT_FS_BASE);

    cpu.esp = GAME_STACK_TOP;
    cpu.esp -= 4;
    wr32(g_mem, cpu.esp, RT_RETURN_ADDR);
    f_00947a13();
    fprintf(stderr, "cm0102_recomp: the entry point returned %u\n", cpu.eax);
    at_end();
    win_save_window();
    SDL_Quit();
    return (int)cpu.eax;
}
