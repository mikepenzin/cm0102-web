#ifndef RECOMP_FOLDER_NAMES_H
#define RECOMP_FOLDER_NAMES_H

#include <stddef.h>

/* found = the spelling of part in the folder path, an exact match first, else one that differs only in case;
 * empty when there is none or the folder cannot be read (folder_names.c). */
void folder_lookup(const char *path, const char *part, char *found, size_t room);

/* The number of folder reads into the kept names (tests). */
unsigned long folder_names_reads(void);

/* The window in seconds in which a changed folder is read at each lookup: 2, or what a test sets. */
void folder_names_recent(int seconds);

/* Forgets every kept folder (tests). */
void folder_names_reset(void);

#endif
