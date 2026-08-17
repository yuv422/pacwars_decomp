/*
 * MANEDIT.H
 *
 * Public prototypes exported by MANEDIT.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MANEDIT_H
#define MANEDIT_H

#include "PACWARS.H"

int choose_edit_pacman(int curr_pacman);
int edit_pacman(int curr_pacman);
int list_pacmen(int curr_pacman, char far * file_name);
void buffer_pacmen(void);
void restore_pacmen(void);
void hilite_list_pacman(int status, int pacman);
int get_sprite_data(int curr_pacman, int edit_pos, int edit_horiz);
void display_edit_pacman(int curr_pacman, int init, int offset);

#endif /* MANEDIT_H */
