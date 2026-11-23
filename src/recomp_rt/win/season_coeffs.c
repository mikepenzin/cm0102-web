/* The real UEFA season coefficients of the season databases (season_coeffs.h, 0.6.2 open item 106). */
#include "win.h"
#include "season_coeffs.h"

#include <stdio.h>
#include <string.h>

#define NATIONS_PTR 0xae23a8u
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define COEFFS 0xe0u
#define MAX_UNDO 256

/* The six values that the last fill replaced, to give back before a new shift. */
static struct { uint32_t at; double old[6]; } undo[MAX_UNDO];
static int n_undo;

void season_coeffs_undo(void)
{
    uint32_t base = rd32(g_mem, NATIONS_PTR), count = rd32(g_mem, NATION_COUNT);
    for (int i = n_undo - 1; i >= 0; i--)
        if (base && undo[i].at >= base && undo[i].at + 48 <= base + count * NATION_SIZE)
            for (int k = 0; k < 6; k++) wrf64(g_mem, undo[i].at + 8u * k, undo[i].old[k]);
    n_undo = 0;
}

void season_coeffs_forget(void) { n_undo = 0; }

void season_coeffs_fill(const char *name)
{
    const SeasonCoeffDb *db = NULL;
    for (int i = 0; name && i < season_coeff_dbs_count; i++)
        if (!strcmp(season_coeff_dbs[i].db, name)) db = &season_coeff_dbs[i];
    if (!db) return;
    uint32_t base = rd32(g_mem, NATIONS_PTR), count = rd32(g_mem, NATION_COUNT);
    if (!base || !count || count > 10000u) return;
    int written = 0, kept = 0, held = 0;
    for (int k = 0; k < db->n_rows; k++) {
        const SeasonCoeff *c = &db->rows[k];
        uint32_t rec = base + (uint32_t)c->id * NATION_SIZE;
        if (c->id >= count || rd32(g_mem, rec) != c->id) {
            kept++;
            continue;
        }
        double now[6];
        int is_found = 1, is_real = 1;
        for (int i = 0; i < 6; i++) {
            now[i] = rdf64(g_mem, rec + COEFFS + 8u * i);
            is_found &= now[i] == c->found[i];
            is_real &= now[i] == c->real[i];
        }
        if (!is_found && !is_real) {
            fprintf(stderr, "recomp: season coeffs: %s: %s keeps its own values (+0xe0 %.3f, +0x108 %.3f)\n",
                    db->db, c->nation, now[0], now[5]);
            kept++;
            continue;
        }
        if (n_undo >= MAX_UNDO) rt_fatal("season coeffs: the undo record is full");
        undo[n_undo].at = rec + COEFFS;
        memcpy(undo[n_undo++].old, now, sizeof now);
        for (int i = 0; i < 6; i++) wrf64(g_mem, rec + COEFFS + 8u * i, c->real[i]);
        if (is_real) held++;      /* no undo before this fill */
        else written++;
    }
    fprintf(stderr, "recomp: season coeffs: %s: the real six seasons written for %d nations, %d kept%s\n",
            db->db, written, kept, held ? ", some held them already" : "");
}
