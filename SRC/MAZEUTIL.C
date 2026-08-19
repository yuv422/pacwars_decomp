/*
 * MAZEUTIL.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZEUTIL.H"
#include <string.h>

/*
 * Referenced only from get_filename() below (confirmed via Ghidra xref
 * analysis), so file-scope here rather than exported. 140 bytes, holding
 * one or more back-to-back null-terminated strings; its compiled-in
 * default is just the marker "MAZEPATH" followed by zeroed/unused space.
 */
static char net_path[140] = "MAZEPATH";

/* storage for the _max_x/_max_y/_sc_width globals declared extern in
   PACWARS.H; set_clip_window() itself is still a stub pending its own
   decompilation pass. */
int _max_x;
int _max_y;
int _sc_width;

void set_clip_window(int status)
{
}

void update_sync(MAZE_LOG_STRUCT far * maze_log)
{
}

void update_time(MAZE_LOG_STRUCT far * maze_log)
{
}

void update_map(unsigned char sync)
{
}

void update_room(unsigned char sync, ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
}

void far * maze_def(int hoff, int voff)
{
    return 0;
}

void far * attrib_maze_def(int hoff, int voff)
{
    return 0;
}

void display_pacmen(int offset)
{
}

void display_registered(void)
{
}

void display_curr_score(void)
{
}

void hilite_pacman(int status, int pacman)
{
}

void display_instructions(void)
{
}

void score_pacmen(void)
{
}

int choose_pacman(int curr_pacman)
{
    return 0;
}

void disp_hiscore(int sp_off, HISCORE far * hiscore)
{
}

void edit_name(int offset, int sp_off, HISCORE far * hiscore)
{
}

void display_curr_name(char far * name_str)
{
}

void pause_box(char far * name_str)
{
}

int copy_protect(void)
{
    return 0;
}

void mk_filename(void)
{
}

void pause_time(void)
{
}

void reg_text(int far * x, int far * y, int far * w, int far * h)
{
}

void display_tokens(int x, int y, char far * letter, int pos)
{
}

void draw_scoreboard(void)
{
}

void show_selected(int status, int selected)
{
}

void fill_scoreboard(int i, MAZE_LOG_STRUCT far * maze_log, int offset)
{
}

void update_radar(int offset, MAZE_LOG_STRUCT far * maze_log, int gold_present, int token_present)
{
}

void f1_instructions(int offset)
{
}

/* TODO: real name not recoverable from PACWARS.TXT for this local
   function (module's type-def list is shorter than its true local-
   function count) -- offset 1611:180F. Verify/rename by
   cross-referencing the Ghidra project. */
void unk_func_180F(void)
{
}

void get_filename(char far * file_name)
{
    char temp_name[13];

    /*
     * net_path holds a run of back-to-back null-terminated strings; this
     * skips past net_path's *own* first string (its compiled-in default
     * is just the marker "MAZEPATH") and reads whatever string
     * immediately follows it in the buffer, then rebuilds file_name as
     * that string + the originally-passed name appended on.
     *
     * I could not find anywhere in the program that writes a second
     * string into net_path -- it isn't set_maze_path() (that function
     * manipulates the DOS PATH environment variable for launching helper
     * programs and never touches net_path at all) -- so I can't confirm
     * what real value ends up here at runtime. With net_path left at its
     * compiled-in default, the space right after "MAZEPATH\0" is just
     * zero padding (an empty string), so as compiled, the net effect is
     * simply "leave file_name unchanged."
     */
    strcpy(temp_name, file_name);
    strcpy(file_name, net_path + 1 + strlen(net_path));
    strcat(file_name, temp_name);
}

void beep_sound(unsigned int freq, int wait_length)
{
}

void memcpyb(void)
{
}

