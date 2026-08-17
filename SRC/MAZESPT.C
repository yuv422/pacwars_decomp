/*
 * MAZESPT.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZESPT.H"

int alloc_block_mem(void)
{
    return 0;
}

void display_sprite(int sprite_num)
{
}

void mix_sprite(int sprite_num, int colour)
{
}

void mask_sprite(int sprite_num)
{
}

void or_sprite(int sprite_num)
{
}

void erase_sprite(int sprite_num)
{
}

int read_sprite(int sprite_num, char far * file_name)
{
    return 0;
}

void load_sprites(void)
{
}

int open_sprite_array(char far * file_name, int mode)
{
    return 0;
}

void close_sprite_array(int array_fd)
{
}

void load_all_sprites(void)
{
}

void load_block_array(int fd)
{
}

void save_block_array(int fd)
{
}

void conv_all_sprites(void)
{
}

void save_all_sprites(void)
{
}

void load_character(int curr_pacman, char far * file_name)
{
}

void save_character(int curr_pacman, char far * file_name)
{
}

void read_sprite_array(int sprite_num, int array_fd)
{
}

void write_sprite_array(int sprite_num, int array_fd)
{
}

void load_names(int array_fd)
{
}

void save_names(int array_fd)
{
}

void load_name(int array_fd, int curr_pacman)
{
}

void save_name(int array_fd, int curr_pacman)
{
}

void animate_room(ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
}

void clear_animates(ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
}

int gauss(int std)
{
    return 0;
}

void icon(int x, int y, int sprite, int num)
{
}

void add_score(int man, int amount, MAZE_LOG_STRUCT far * maze_log)
{
}

int test_bounce(int dir, SPRITE_STRUCT far * sp, int hoff, int voff)
{
    return 0;
}

int isblock(SPRITE_STRUCT far * sp, int hoff, int voff)
{
    return 0;
}

int test_for_block(int x, int y, int w, int h, int hoff, int voff)
{
    return 0;
}

int testx(int dir, SPRITE_STRUCT far * sp, int hoff, int voff)
{
    return 0;
}

int testy(int dir, SPRITE_STRUCT far * sp, int hoff, int voff)
{
    return 0;
}

void init_gold(MAZE_LOG_STRUCT far * maze_log)
{
}

void init_token(MAZE_LOG_STRUCT far * maze_log)
{
}

void init_man(int station_num, MAZE_LOG_STRUCT far * maze_log)
{
}

void init_position(int far * hoff, int far * voff, int far * x, int far * y)
{
}

void overlap_sprite(SPRITE_STRUCT far * s1, SPRITE_STRUCT far * s2, int attrib)
{
}

/* TODO: real name not recoverable from PACWARS.TXT for this local
   function (module's type-def list is shorter than its true local-
   function count) -- offset 131E:2E86. Verify/rename by
   cross-referencing the Ghidra project. */
void unk_func_2E86(void)
{
}

