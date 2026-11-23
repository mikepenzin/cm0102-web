/* The kept folder names of the path lookup (src/recomp_rt/win/folder_names.c). Each lookup gives what a read of
 * the folder gives, after a file is created, renamed or deleted, with names matched as Windows does (case
 * ignored, an exact match first). The read counter shows that the kept names serve the lookups of a folder that
 * did not change. The recent window is first set so that every folder counts as old, then back to 2 seconds. */
#include "../../src/recomp_rt/win/folder_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
static char dir[512];

static void path(char *out, size_t room, const char *name) { snprintf(out, room, "%s/%s", dir, name); }

static void touch(const char *name)
{
    char p[1024];
    path(p, sizeof p, name);
    FILE *f = fopen(p, "w");
    if (f) fclose(f);
}

static void expect(const char *what, const char *part, const char *want)
{
    char found[256];
    folder_lookup(dir, part, found, sizeof found);
    if (strcmp(found, want)) {
        printf("FAIL %s: %s gives \"%s\", expected \"%s\"\n", what, part, found, want);
        fails++;
    }
}

static void expect_reads(const char *what, unsigned long want)
{
    if (folder_names_reads() != want) {
        printf("FAIL %s: %lu folder reads, expected %lu\n", what, folder_names_reads(), want);
        fails++;
    }
}

int main(void)
{
    snprintf(dir, sizeof dir, "%s/folder_names_test_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(dir)) { printf("FAIL mkdtemp\n"); return 1; }
    touch("Fixtures_2013.tmp");
    touch("samesave.sav");
    folder_names_recent(-1000000);          /* every folder counts as old */

    /* Case is ignored; one read serves the next lookups. */
    expect("first read", "FIXTURES_2013.TMP", "Fixtures_2013.tmp");
    expect_reads("first read", 1);
    expect("kept names", "fixtures_2013.tmp", "Fixtures_2013.tmp");
    expect("kept names, exact", "samesave.sav", "samesave.sav");
    expect("kept names, none", "missing.tmp", "");
    expect_reads("kept names", 1);

    /* A file the player adds outside the game, then renamed and deleted: each change reads the folder again. */
    char a[1024], b[1024];
    touch("Player.SAV");
    expect("created", "player.sav", "Player.SAV");
    path(a, sizeof a, "Player.SAV");
    path(b, sizeof b, "Renamed.sav");
    rename(a, b);
    expect("renamed, old name", "player.sav", "");
    expect("renamed, new name", "RENAMED.SAV", "Renamed.sav");
    path(a, sizeof a, "Fixtures_2013.tmp");
    unlink(a);
    expect("deleted", "fixtures_2013.tmp", "");
    if (folder_names_reads() < 4) {
        printf("FAIL changes: %lu folder reads, expected 4 or more\n", folder_names_reads());
        fails++;
    }

    /* A folder put in the place of another (another inode): its own names. */
    char other[600], moved[600];
    snprintf(other, sizeof other, "%s.other", dir);
    snprintf(moved, sizeof moved, "%s.moved", dir);
    if (!mkdir(other, 0755)) {
        char f[700];
        snprintf(f, sizeof f, "%s/Other.tmp", other);
        FILE *o = fopen(f, "w");
        if (o) fclose(o);
        rename(dir, moved);
        rename(other, dir);
        expect("replaced folder", "other.tmp", "Other.tmp");
        expect("replaced folder, old name", "renamed.sav", "");
        char cmd[1300];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", moved);
        if (system(cmd)) printf("note: %s not removed\n", moved);
    }

    /* Two names that differ only in case, on a host folder that keeps both: the exact one wins. */
    struct stat st;
    touch("Coarse.tmp");
    path(a, sizeof a, "COARSE.TMP");
    if (stat(a, &st)) {
        touch("coarse.TMP");
        expect("exact first", "coarse.TMP", "coarse.TMP");
        expect("exact first, other", "Coarse.tmp", "Coarse.tmp");
    } else
        printf("note: the folder ignores case, so two names that differ only in case are not tested\n");

    /* A folder that changed in the last 2 seconds is read at each lookup. */
    folder_names_recent(2);
    touch("Recent.tmp");
    unsigned long before = folder_names_reads();
    expect("recent", "recent.tmp", "Recent.tmp");
    expect("recent, again", "recent.tmp", "Recent.tmp");
    expect_reads("recent", before);         /* read without the kept names */

    /* After a reset the folder is read again. */
    folder_names_recent(-1000000);
    folder_names_reset();
    expect("after reset", "RECENT.TMP", "Recent.tmp");

    char cmd[1100];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd)) printf("note: %s not removed\n", dir);
    printf(fails ? "FAILED: %d\n" : "PASS\n", fails);
    return fails != 0;
}
