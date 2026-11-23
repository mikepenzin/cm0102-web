/* Test of host_open_folder (src/recomp_rt/host/posix.c, 0.6.1 item 5): the Open folder buttons start the file
 * manager with the save folder as its one argument. RECOMP_OPEN_FOLDER_PROGRAM names a script in place of open or
 * xdg-open; the script writes its arguments and LD_LIBRARY_PATH to a file.
 * Usage: host_folder_test   (it makes and removes its own temporary folder) */
#include "../../src/recomp_rt/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static char root[1024], logfile[1100];

/* The text of the script's file, after the script has run (at most 5 s). Empty when it did not run. */
static const char *script_output(void)
{
    static char text[4096];
    text[0] = 0;
    for (int i = 0; i < 500; i++) {
        FILE *f = fopen(logfile, "r");
        if (f) {
            /* The script writes "end" last, so a whole file ends with it. */
            size_t n = fread(text, 1, sizeof text - 1, f);
            fclose(f);
            text[n] = 0;
            if (n >= 4 && !strcmp(text + n - 4, "end\n")) break;
        }
        usleep(10000);
    }
    unlink(logfile);
    return text;
}

int main(void)
{
    snprintf(root, sizeof root, "%s/host_folder_test_%d", P_tmpdir, (int)getpid());
    mkdir(root, 0755);
    snprintf(logfile, sizeof logfile, "%s/args.txt", root);

    char script[1100];
    snprintf(script, sizeof script, "%s/fake-open.sh", root);
    FILE *f = fopen(script, "w");
    fprintf(f, "#!/bin/sh\n{ echo \"count $#\"; echo \"arg $1\"; echo \"lib ${LD_LIBRARY_PATH-unset}\"; "
               "echo end; } > \"%s.tmp\" && mv \"%s.tmp\" \"%s\"\n", logfile, logfile, logfile);
    fclose(f);
    chmod(script, 0755);
    setenv("RECOMP_OPEN_FOLDER_PROGRAM", script, 1);

    /* A save folder with spaces, quotes, a dollar sign and UTF-8 in its name: one argument, as it is. */
    char odd[1100], expect[1300];
    snprintf(odd, sizeof odd, "%s/My Saves 'CM' \"01-02\" $HOME K\xc3\xb6ln", root);
    CHECK(mkdir(odd, 0755) == 0);
    unsetenv("APPDIR");
    setenv("LD_LIBRARY_PATH", "/opt/lib", 1);
    CHECK(host_open_folder(odd) == 1);
    snprintf(expect, sizeof expect, "count 1\narg %s\nlib /opt/lib\nend\n", odd);
    const char *got = script_output();
    CHECK(!strcmp(got, expect));
    if (strcmp(got, expect)) printf("got:\n%s\nexpected:\n%s\n", got, expect);

    /* In an AppImage, the AppImage's library folder leaves LD_LIBRARY_PATH; the others stay in order. */
    setenv("APPDIR", "/tmp/.mount_CM0102x", 1);
    setenv("LD_LIBRARY_PATH", "/tmp/.mount_CM0102x/usr/lib:/opt/lib:/usr/local/lib", 1);
    CHECK(host_open_folder(root) == 1);
    snprintf(expect, sizeof expect, "count 1\narg %s\nlib /opt/lib:/usr/local/lib\nend\n", root);
    got = script_output();
    CHECK(!strcmp(got, expect));
    if (strcmp(got, expect)) printf("got:\n%s\nexpected:\n%s\n", got, expect);
    setenv("LD_LIBRARY_PATH", "/tmp/.mount_CM0102x/usr/lib", 1);
    CHECK(host_open_folder(root) == 1);
    snprintf(expect, sizeof expect, "count 1\narg %s\nlib \nend\n", root);
    got = script_output();
    CHECK(!strcmp(got, expect));
    unsetenv("APPDIR");

    /* A folder that does not exist, or a file: 0, and nothing starts. */
    char missing[1100];
    snprintf(missing, sizeof missing, "%s/not there", root);
    CHECK(host_open_folder(missing) == 0);
    CHECK(host_open_folder(script) == 0);
    CHECK(script_output()[0] == 0);

    /* No file manager program on this computer: 0. */
    setenv("RECOMP_OPEN_FOLDER_PROGRAM", "cm0102-no-such-file-manager", 1);
    CHECK(host_open_folder(root) == 0);

    rmdir(odd);
    unlink(script);
    rmdir(root);
    if (failures) printf("%d failures\n", failures);
    else printf("host_open_folder: all checks passed\n");
    return failures != 0;
}
