/* The qsort of the original's CRT (MSVC 6, 0x945333) in host code. qsort6.c has the details. */
#ifndef QSORT6_H
#define QSORT6_H

#include <stdint.h>

/* The game stack that the original's qsort writes: mem is the game memory, q its ebp (esp at entry
 * minus 4), rows the game address of the rows, then the caller's ebp, ebx, esi and edi and the compare. */
typedef struct {
    uint8_t *mem;
    uint32_t q, rows, ebp, ebx, esi, edi, cmp;
    int sorted;         /* item 151b: 1 when the rows are already in strict order (qsort6_strict), so the
                           steps are known without a compare */
} qsort6_stack;

/* The stack bytes that the original's qsort can write: from esp - QSORT6_BELOW to esp + QSORT6_ABOVE. */
#define QSORT6_BELOW 0x140u
#define QSORT6_ABOVE 0xcu

/* Sorts num rows of width bytes at base with the compare 0x7c6d40, exactly as 0x945333 sorts them:
 * the same compares in the same order and the same swaps, so rows that compare equal end in the same
 * order. It also writes the game stack st as 0x945333 does. Gives back the row that 0x945333 leaves in
 * eax, or NULL when it leaves eax unchanged (fewer than 2 rows). */
uint8_t *qsort6(uint8_t *base, uint32_t num, uint32_t width, const qsort6_stack *st);

/* The compare of the player history lists, 0x7c6d40: the id at +0, then the signed byte at +4, where
 * 0xff in either row counts as equal. */
int qsort6_history_cmp(const uint8_t *a, const uint8_t *b);

/* 1 when every id is below 0x80000000 and each row compares below the next: then the compare 0x7c6d40 is a
 * strict order of the rows (item 151b, qsort6.c). */
int qsort6_strict(const uint8_t *base, uint32_t num, uint32_t width);

/* Item 151b (qsort6_insert.c): the sort of rows whose first num - 1 are in strict order and whose last row
 * compares strictly with its neighbours at its place (the caller tests both). before holds a copy of the rows
 * as they are. Sets *done and gives back eax as qsort6 would, with the same rows and stack words; when it
 * gives up (*done 0) the rows are as they were and the caller sorts with qsort6. */
uint8_t *qsort6_insert(uint8_t *base, uint32_t num, uint32_t width, const qsort6_stack *st, const uint8_t *before,
                       int *done);
extern int qsort6_insert_max_visit;     /* the parts a walk may visit; the self-test lowers it */

/* RECOMP_QSORT_TEST (crt.c): qsort6 against the recompiled qsort; the exit code, 0 when all are equal. */
int qsort6_self_test(void);

#endif
