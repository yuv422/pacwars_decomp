/*
 * MAZEINIT.H
 *
 * Public prototypes exported by MAZEINIT.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZEINIT_H
#define MAZEINIT_H

#include "PACWARS.H"

void set_maze_path(char far * path);
int open_prog(char far * menu_name);
void close_prog(void);
long get_prog(char far * token_name);

#endif /* MAZEINIT_H */
