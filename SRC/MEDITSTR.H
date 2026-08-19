/*
 * MEDITSTR.H
 *
 * Public prototypes exported by MEDITSTR.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MEDITSTR_H
#define MEDITSTR_H

#include "PACWARS.H"

typedef enum {
	pacman_t = 0x490,
    name_t = 0x491,
    score_t = 0x493,
} EDIT_TYPE;

int edit_str(unsigned int far * inkey, unsigned int far * ext, int row, int col, char far * char_str, EDIT_TYPE type, int display);

#endif /* MEDITSTR_H */
