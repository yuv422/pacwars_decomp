/*
 * MAZE.H
 *
 * Public prototypes exported by MAZE.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZE_H
#define MAZE_H

#include "PACWARS.H"

void main(int argc, char far * far * argv);
void pacwars(void);
void set_key_vect(int status, void interrupt (*key_func)(void));
void interrupt key_pause(void);
void restore_background(SPRITE_STRUCT far * sp);

#endif /* MAZE_H */
