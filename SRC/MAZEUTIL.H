/*
 * MAZEUTIL.H
 *
 * Public prototypes exported by MAZEUTIL.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZEUTIL_H
#define MAZEUTIL_H

#include "PACWARS.H"

void set_clip_window(int status);
void update_sync(MAZE_LOG_STRUCT far * maze_log);
void update_time(MAZE_LOG_STRUCT far * maze_log);
void update_map(unsigned char sync);
void far * maze_def(int hoff, int voff);
void far * attrib_maze_def(int hoff, int voff);
void display_pacmen(int offset);
void display_registered(void);
void hilite_pacman(int status, int pacman);
int choose_pacman(int curr_pacman);
void disp_hiscore(int sp_off, HISCORE far * hiscore);
int copy_protect(void);
void mk_filename(int a, int b, char far * file_name);
void pause_time(void);
void draw_scoreboard(void);
void show_selected(int status, int selected);
void fill_scoreboard(int i, MAZE_LOG_STRUCT far * maze_log, int offset);
void update_radar(int offset, MAZE_LOG_STRUCT far * maze_log, int gold_present, int token_present);
void get_filename(char far * file_name);
void beep_sound(unsigned int freq, int wait_length);
void memcpyb(unsigned char far * dest, unsigned char far * source, int width);

#endif /* MAZEUTIL_H */
