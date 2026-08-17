/*
 * MAZEEDIT.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZEEDIT.H"

int alloc_maze_def_mem(void)
{
    return 0;
}

void display_menu_pacmen(void)
{
}

int pacwars_menu(int curr_option)
{
    return 0;
}

void hilite_option(int curr_pacman, int offset, int x1, int x2, int y)
{
}

void edit_attributes(int status, int option)
{
}

int choose_edit_maze(int far * hoff, int far * voff)
{
    return 0;
}

void edit_maze(int curr_hoff, int curr_voff)
{
}

void block_menu(int curr_hoff, int curr_voff, int far * curr_row, int far * curr_col)
{
}

void copy_block(int curr_hoff, int curr_voff, int far * curr_row, int far * curr_col)
{
}

void store_block(char type, int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2, int row3, int col3)
{
}

void restore_block(int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2)
{
}

void undo_block(int curr_hoff, int curr_voff, int row, int col)
{
}

void display_rooms(int curr_hoff, int curr_voff)
{
}

void show_attribs(int curr_hoff, int curr_voff)
{
}

void draw_room(void)
{
}

int select_block(int curr_block, int far * offset)
{
    return 0;
}

void create_room_sprite(int hoffset, int voffset)
{
}

static void edit_screen1(unsigned char far * sprite, unsigned char far * sp_buff)
{
}

void hilite_room(void)
{
}

static void display_save(int status, int hoff, int voff)
{
}

static void display_filestatus(int status)
{
}

void set_block(int status, int type)
{
}

void set_block_attrib(int block, int hoff, int voff, int row, int col)
{
}

int get_block(int attrib, int hoff, int voff, int row, int col)
{
    return 0;
}

void hilite_block(int hoff, int voff, int row, int col)
{
}

void hilite_attrib(int status, int hoff, int voff, int row, int col)
{
}

void hilite_select_block(int status, int hoff, int voff, int row, int col)
{
}

void get_room_pos(int status, int row, int col, int offset)
{
}

void draw_blocks(int hoff, int voff, int far * x, int far * y)
{
}

void edit_block(int offset)
{
}

void edit_block_range(int sprite)
{
}

void edit_maze_rows(int sprite, int w, int h)
{
}

int save_maze(int fd)
{
    return 0;
}

int load_maze(int fd)
{
    return 0;
}

int conv_maze(int fd)
{
    return 0;
}

void display_maze_rows(char far * maze_str, unsigned int far * inkey, unsigned int far * ext)
{
}

void display_scroll(int status, char far * maze_str)
{
}

void button(void)
{
}

void draw_maze_box(int status, int x, int y, char far * text)
{
}

void draw_block_box(void)
{
}

void draw_block_box2(void)
{
}

void draw_attrib_box(void)
{
}

void draw_block_range_box(void)
{
}

void clear_box(void)
{
}

void draw_block_range(void)
{
}

void draw_block_range2(int status, int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2)
{
}

void edit_animate(int status, int row1, int col1, int row2, int col2, int offset)
{
}

