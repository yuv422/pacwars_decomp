/*
 * MAZEEDIT.H
 *
 * Public prototypes exported by MAZEEDIT.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZEEDIT_H
#define MAZEEDIT_H

#include "PACWARS.H"

int alloc_maze_def_mem(void);
int alloc_maze_editor_mem(void);
int pacwars_menu(int curr_option);
int choose_edit_maze(int far * hoff, int far * voff);
void edit_maze(int curr_hoff, int curr_voff);
void show_attribs(int curr_hoff, int curr_voff);
int select_block(int curr_block, int far * offset);
int save_maze(int fd);
int load_maze(int fd);
int conv_maze(int fd);

#endif /* MAZEEDIT_H */
