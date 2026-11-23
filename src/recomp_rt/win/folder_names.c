#include "folder_names.h"
#include "../host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* The spelling of a name in a folder, ignoring case, for the path lookup of files.c (ci_lookup).
 *
 * Each file that the game opens looks up every part of its path. A read of the whole folder took 19 % of a
 * Tier A year (the profile of 0.6.2 slice 3), because the user folder holds hundreds of part files. So the
 * names of a folder are kept with its device, inode, modification time and change time. A lookup reads the
 * folder again when one of them changes. Creating, deleting or renaming an entry changes the times, by the
 * game or by the player outside the game. A tool can set the modification time, but not the change time.
 * A folder that changed within RECENT seconds of now is read at each lookup, so a change in the same tick
 * of a coarse clock (1 s on some file systems) is not missed. The result is the result of the read.
 * RECOMP_FOLDER_CACHE=0 reads the folder at each lookup, as before. */
#define CACHE_FOLDERS 64
#define RECENT 2
static int recent = RECENT;              /* seconds; tests set it (folder_names_recent) */

typedef struct {
    char *dir;
    char **names;
    int count;
    HostStat st;
    unsigned long used;
} FolderNames;

static FolderNames folders[CACHE_FOLDERS];
static unsigned long reads;                 /* folder reads into the cache (tests) */
static unsigned long folder_clock;

static int cache_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_FOLDER_CACHE");
        on = !(v && !strcmp(v, "0"));
    }
    return on;
}

static void folder_free(FolderNames *f)
{
    for (int i = 0; i < f->count; i++) free(f->names[i]);
    free(f->names);
    free(f->dir);
    memset(f, 0, sizeof *f);
}

/* Reads the folder into f; 0 when it cannot be read. */
static int folder_read(FolderNames *f, const char *dir, const HostStat *st)
{
    HostDir *d = host_opendir(dir);
    if (!d) return 0;
    int room = 0, ok = 1;
    const char *e;
    folder_free(f);
    f->dir = strdup(dir);
    while (ok && (e = host_readdir(d))) {
        if (f->count == room) {
            room = room ? 2 * room : 64;
            char **p = realloc(f->names, (size_t)room * sizeof *p);
            if (!p) { ok = 0; break; }
            f->names = p;
        }
        if (!(f->names[f->count] = strdup(e))) ok = 0;
        else f->count++;
    }
    host_closedir(d);
    f->st = *st;
    reads++;
    return ok && f->dir != NULL;            /* a list cut short is not kept: the caller frees it */
}

static void ci_scan(const char *dir, const char *part, char *found, size_t room)
{
    HostDir *d = host_opendir(dir);
    if (!d) return;
    const char *e;
    while ((e = host_readdir(d))) {
        if (!strcmp(e, part)) { snprintf(found, room, "%s", e); break; }
        if (!found[0] && !strcasecmp(e, part)) snprintf(found, room, "%s", e);
    }
    host_closedir(d);
}

void folder_lookup(const char *path, const char *part, char *found, size_t room)
{
    found[0] = 0;
    HostStat st;
    int64_t now = (int64_t)time(NULL);
    if (!cache_on() || host_stat(path, &st) || !st.is_dir || st.mtime >= now - recent || st.ctime >= now - recent) {
        ci_scan(path, part, found, room);
        return;
    }
    FolderNames *f = NULL, *oldest = &folders[0];
    for (int i = 0; i < CACHE_FOLDERS; i++) {
        if (folders[i].dir && !strcmp(folders[i].dir, path)) { f = &folders[i]; break; }
        if (folders[i].used < oldest->used) oldest = &folders[i];
    }
    if (!f || f->st.mtime != st.mtime || f->st.mtime_ns != st.mtime_ns || f->st.ctime != st.ctime ||
        f->st.ctime_ns != st.ctime_ns || f->st.dev != st.dev || f->st.ino != st.ino) {
        if (!f) f = oldest;
        if (!folder_read(f, path, &st)) {
            folder_free(f);
            ci_scan(path, part, found, room);
            return;
        }
    }
    f->used = ++folder_clock;
    for (int i = 0; i < f->count; i++) {
        if (!strcmp(f->names[i], part)) { snprintf(found, room, "%s", f->names[i]); return; }
        if (!found[0] && !strcasecmp(f->names[i], part)) snprintf(found, room, "%s", f->names[i]);
    }
}

unsigned long folder_names_reads(void) { return reads; }

void folder_names_recent(int seconds) { recent = seconds; }

void folder_names_reset(void)
{
    for (int i = 0; i < CACHE_FOLDERS; i++) folder_free(&folders[i]);
    folder_clock = 0;
}
