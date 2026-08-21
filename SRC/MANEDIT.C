/*
 * MANEDIT.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MANEDIT.H"

int choose_edit_pacman(int curr_pacman)
{
    return 0;
}

int edit_pacman(int curr_pacman)
{
    return 0;
}

int list_pacmen(int curr_pacman, char far * file_name)
{
    return 0;
}

void buffer_pacmen(void)
{
}

void restore_pacmen(void)
{
}

void scroll_pacmen(int dir)
{
}

void display_list_pacmen(int offset, int file_offset)
{
}

void hilite_list_pacman(int status, int pacman)
{
}

int get_sprite_data(int curr_pacman, int edit_pos, int edit_horiz)
{
    return 0;
}

int get_mirror_sprite_data(int curr_pacman, int edit_pos, int edit_horiz)
{
    return 0;
}

/*
 * NOTE: the PACWARS.TXT-derived stub generator mis-assigned this
 * module-local function the name "edit_screen1" -- that name actually
 * belongs to MAZEEDIT.C's edit_screen1() (confirmed decompiled at
 * 1f61:2571, the maze editor's title-box drawer; Ghidra only has one
 * function named edit_screen1, and it's that one), which caused a
 * duplicate-symbol link error once MAZEEDIT.C's real body was written.
 * Renamed here to a placeholder pending MANEDIT.C's own decompilation
 * pass, which should identify this function's real name/address.
 */
void unk_func_manedit_1(void)
{
}

void edit_screen(void)
{
}

void display_edit_pacman(int curr_pacman, int init, int offset)
{
}

/*
 * NOTE: same stub-generator name collision as unk_func_manedit_1() above
 * -- "display_save" actually belongs to MAZEEDIT.C's display_save()
 * (decompiled at 1f61:2673). Renamed to a placeholder pending MANEDIT.C's
 * own decompilation pass.
 */
void unk_func_manedit_2(int status)
{
}

/*
 * NOTE: same stub-generator name collision -- "display_filestatus"
 * actually belongs to MAZEEDIT.C's display_filestatus() (decompiled at
 * 1f61:2748). Renamed to a placeholder pending MANEDIT.C's own
 * decompilation pass.
 */
void unk_func_manedit_3(int status, int type)
{
}

void display_anim_delay(int delay)
{
}

void display_file_control(int edit_horiz)
{
}

void edit_pacname(int curr_pacman, char far * name_str, unsigned int far * inkey, unsigned int far * ext)
{
}

void edit_pacfile(int curr_pacman, char far * name_str, unsigned int far * inkey, unsigned int far * ext)
{
}

void display_pacname(char far * name_str)
{
}

void display_pacfile(char far * name_str)
{
}

void hilite_edit(int status, int edit_pos, int edit_horiz)
{
}

void get_edit_pos(int edit_pos, int edit_horiz, int far * xpos, int far * ypos)
{
}

int load_all_files(void)
{
    return 0;
}

int sort_func(void far * a, void far * b)
{
    return 0;
}

void fill_list(int curr_pacman, int spos, int num)
{
}

void edit_screen2(void)
{
}

void list_message(int status)
{
}

