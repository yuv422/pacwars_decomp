/*
 * MAZEDRAW.H
 *
 * Public prototypes exported by MAZEDRAW.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZEDRAW_H
#define MAZEDRAW_H

#include "PACWARS.H"

void draw_maze(void);
void draw_maze_area(int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2);
void draw_attribs(void);
void draw_attrib(int row, int col, int attrib);
void restore_maze(MAZE_LOG_STRUCT far * maze_log);

#endif /* MAZEDRAW_H */
