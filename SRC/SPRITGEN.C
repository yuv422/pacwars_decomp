/*
 * SPRITGEN.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "SPRITGEN.H"

void sprite_gen(int num_sprites, char far * far * sprite_files)
{
}

void save_routine(char far * file_name)
{
}

int edit_sprite(int xmargin, int ymargin, int far * sprite, int argc, int init)
{
    return 0;
}

int edit_mask(int xmargin, int ymargin)
{
    return 0;
}

void restore_mask(void)
{
}

void copy_to_clip(int index, int w, int h, int xm, int ym)
{
}

void copy_from_clip(int index, int w, int h, int xm, int ym)
{
}

void hflip(int w, int h, int xm, int ym)
{
}

void vflip(int w, int h, int xm, int ym)
{
}

void rotate_90(int w, int h, int xm, int ym)
{
}

void rotate_45(int w, int h, int xm, int ym)
{
}

int move_sprite(int w, int h, int xm, int ym)
{
    return 0;
}

int size_sprite(int xm, int ym)
{
    return 0;
}

int animate_sprite(int xm, int ym, int sprite, int num_sprites, char far * far * sprite_files)
{
    return 0;
}

void clear_sprite(int w, int h)
{
}

void draw_sprite(int w, int h, int xmargin, int ymargin, int status)
{
}

void draw_grid(int xm, int ym, int w, int h)
{
}

void pixel_hlite(int status, int index, int xmargin, int ymargin, int h)
{
}

void set_pixel_hlite(int index, int xmargin, int ymargin, int h, int colour)
{
}

void draw_palette_colour(int colour)
{
}

void control_text(char far * text_ptr)
{
}

void grid_pos(int x, int y)
{
}

void grid_size(void)
{
}

void draw_file_name(char far * text_ptr)
{
}

void draw_palette(void)
{
}

int select_colour(void)
{
    return 0;
}

void help_commands(void)
{
}

void hlite(int status, int index, int xmargin, int ymargin, int w, int h, int xsep, int ysep)
{
}

static void trbox(int x, int y, int w, int h, int colour)
{
}

static void trfbox(int x, int y, int w, int h, int colour)
{
}

static void hline(int x, int y, int length, int colour)
{
}

static void vline(int x, int y, int length, int colour)
{
}

static void set_pixel(int x, int y, int colour)
{
}

static void read_sprite(char far * file_name)
{
}

void copy_sprite_to_gen(SPRITE_STRUCT far * sprite)
{
}

void copy_gen_to_sprite(SPRITE_STRUCT far * sprite)
{
}

int write_sprite(int row, int col, SPRITE_STRUCT far * sprite)
{
    return 0;
}

static void text256(int row, int col, SPRITE_STRUCT far * sprite)
{
}

static int load_palette(SPRITE_STRUCT far * sprite)
{
    return 0;
}

static void read_array(char far * file_name)
{
}

/* TODO: real name not recoverable from PACWARS.TXT for this local
   function (module's type-def list is shorter than its true local-
   function count) -- offset 0AE7:275D. Verify/rename by
   cross-referencing the Ghidra project. */
void unk_func_275D(int x, int y, unsigned char far * text_ptr, int fore, int back)
{
}

