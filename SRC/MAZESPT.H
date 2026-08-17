/*
 * MAZESPT.H
 *
 * Public prototypes exported by MAZESPT.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZESPT_H
#define MAZESPT_H

#include "PACWARS.H"

void display_sprite(int sprite_num);
void mix_sprite(int sprite_num, int colour);
void mask_sprite(int sprite_num);
void or_sprite(int sprite_num);
void erase_sprite(int sprite_num);
int read_sprite(int sprite_num, char far * file_name);
void load_sprites(void);
int open_sprite_array(char far * file_name, int mode);
void close_sprite_array(int array_fd);
void load_all_sprites(void);
void conv_all_sprites(void);
void save_all_sprites(void);
void load_character(int curr_pacman, char far * file_name);
void save_character(int curr_pacman, char far * file_name);
void read_sprite_array(int sprite_num, int array_fd);
void write_sprite_array(int sprite_num, int array_fd);
void load_names(int array_fd);
void save_names(int array_fd);
void load_name(int array_fd, int curr_pacman);
void save_name(int array_fd, int curr_pacman);
void animate_room(ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr);
void clear_animates(ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr);
int gauss(int std);
void icon(int x, int y, int sprite, int num);
void add_score(int man, int amount, MAZE_LOG_STRUCT far * maze_log);
int test_bounce(int dir, SPRITE_STRUCT far * sp, int hoff, int voff);
int isblock(SPRITE_STRUCT far * sp, int hoff, int voff);
int test_for_block(int x, int y, int w, int h, int hoff, int voff);
int testx(int dir, SPRITE_STRUCT far * sp, int hoff, int voff);
int testy(int dir, SPRITE_STRUCT far * sp, int hoff, int voff);
void init_gold(MAZE_LOG_STRUCT far * maze_log);
void init_token(MAZE_LOG_STRUCT far * maze_log);
void init_man(int station_num, MAZE_LOG_STRUCT far * maze_log);
void init_position(int far * hoff, int far * voff, int far * x, int far * y);
void overlap_sprite(SPRITE_STRUCT far * s1, SPRITE_STRUCT far * s2, int attrib);

#endif /* MAZESPT_H */
