/*
 * HISCORE.H
 *
 * Public prototypes exported by HISCORE.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef HISCORE_H
#define HISCORE_H

#include "PACWARS.H"

int get_hiscore(HISCORE far * hiscore_buff);
int set_hiscore(int score, char far * name, int pacman);
int init_hiscore(void);

#endif /* HISCORE_H */
