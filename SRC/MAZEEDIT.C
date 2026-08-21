/*
 * MAZEEDIT.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZEEDIT.H"
#include "MAZE.H"
#include "UTILS.H"
#include "GRAPH256.H"
#include "MAZESPT.H"
#include "MAZEUTIL.H"
#include "MAZEDRAW.H"
#include "MVAGRAPH.H"
#include "MEDITSTR.H"
#include "HISCORE.H"
#include "SPRITGEN.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <ctype.h>

/* storage for the _maze_buff/_maze_abuff/_undo_buff/_undo_abuff globals
   declared extern in PACWARS.H */
MAZE_STRUCT far * _maze_buff;
MAZE_STRUCT far * _maze_abuff;
MAZE_STRUCT far * _undo_buff;
MAZE_STRUCT far * _undo_abuff;

/* storage for the _maze/_maze_attrib/_VSIZE/_HSIZE globals declared
   extern in PACWARS.H; allocated by alloc_maze_def_mem() below. */
void far * _maze[4][3];
void far * _maze_attrib[4][3];
int _VSIZE;
int _HSIZE;

/* storage for the _room_offset global declared extern in PACWARS.H */
int _room_offset;

/* storage for the block-copy/move clipboard globals declared extern in
   PACWARS.H (block_menu()'s state, MAZEEDIT.C) */
int _maze_srow;
int _maze_scol;
int _maze_erow;
int _maze_ecol;

/* -1/-1 is the "no undo available" sentinel undo_block() itself resets
   these to after consuming an undo, so that's the sensible pre-any-
   operation default too (rather than BSS-zero, which would falsely
   match room (0,0) before any block range has ever been stored). */
int _undo_hoff = -1;
int _undo_voff = -1;

/*
 * Allocates the 4x3 room grid's maze/attribute data blocks (_maze[][] and
 * _maze_attrib[][], PACWARS.H), one MAZE_STRUCT-sized calloc() each.
 * Real owner of _maze/_maze_attrib/_VSIZE/_HSIZE, confirmed via this
 * function's disassembly (MAZEUTIL.C's storage for those globals is
 * still correct -- alloc_maze_def_mem() just populates it at startup).
 *
 * NOTE: the original's out-of-memory check has a latent bug preserved
 * here only in spirit, not literally: it OR-accumulates each allocation's
 * segment across the whole loop rather than checking each pointer
 * individually, so once any allocation succeeds, a later failure
 * wouldn't actually be caught. Reproduced here as a plain per-allocation
 * NULL check instead, which is what the code clearly intends and is
 * indistinguishable in practice (calloc realistically only fails here
 * under true DOS out-of-memory conditions, which the caller already
 * handles identically either way).
 */
int alloc_maze_def_mem(void)
{
    int voff, hoff;
    void far * p;

    for (voff = 0; voff < 4; voff++) {
        for (hoff = 0; hoff < 3; hoff++) {
            p = calloc(1, sizeof(MAZE_STRUCT));
            _maze[voff][hoff] = p;
            if (p == NULL) {
                return 1;
            }
        }
    }

    for (voff = 0; voff < 4; voff++) {
        for (hoff = 0; hoff < 3; hoff++) {
            p = calloc(1, sizeof(MAZE_STRUCT));
            _maze_attrib[voff][hoff] = p;
            if (p == NULL) {
                return 1;
            }
        }
    }

    return 0;
}

/*
 * Missing entirely from the original stub generation pass (not even an
 * unk_func_ placeholder) -- its module's type-def list happened to have
 * exactly enough names to cover the rest of this file's local-function
 * count, so this address was silently never matched to any name.
 * Resolved via the Ghidra project while decompiling _main(), which calls
 * this: confirmed exported as `_alloc_maze_editor_mem` at 0F61:00A9
 * (dump) / 1f61:00a9 (Ghidra), returns signed int, no parameters.
 *
 * Allocates the maze editor's four scratch/undo buffers (PACWARS.H).
 */
int alloc_maze_editor_mem(void)
{
    _maze_buff = calloc(1, sizeof(MAZE_STRUCT));
    if (_maze_buff == NULL) {
        return 1;
    }
    _maze_abuff = calloc(1, sizeof(MAZE_STRUCT));
    if (_maze_abuff == NULL) {
        return 1;
    }
    _undo_buff = calloc(1, sizeof(MAZE_STRUCT));
    if (_undo_buff == NULL) {
        return 1;
    }
    _undo_abuff = calloc(1, sizeof(MAZE_STRUCT));
    if (_undo_abuff == NULL) {
        return 1;
    }
    return 0;
}

/*
 * Draws one animation frame of the two small pacman-icon sprites shown
 * cycling in the main menu's title bar. Real signature (5 int params)
 * recovered from its only call site in hilite_option() below -- the
 * PACWARS.TXT-derived stub had guessed a bare void(void).
 */
void display_menu_pacmen(int curr_pacman, int offset, int x1, int x2, int y)
{
    int sprite_num;

    if (offset < 0xf) {
        sprite_num = curr_pacman * 4 + offset % 2 + 4;
    } else {
        sprite_num = curr_pacman * 0xc + offset + 0x450;
    }
    _sprites[sprite_num].spritex = x1;
    _sprites[sprite_num].spritey = y;
    mix_sprite(sprite_num, 0x1b);

    sprite_num += (offset < 0xf) ? 2 : 6;
    _sprites[sprite_num].spritex = x2;
    _sprites[sprite_num].spritey = y;
    mix_sprite(sprite_num, 0x1b);
}

/* forward declaration: pacwars_menu() (below) calls hilite_option(),
   defined further down in this file. */
void hilite_option(int status, int option);

/*
 * The game's main menu (also invoked, confusingly, from MAZE.C's own
 * main() switch on the choice this returns). Draws the menu box and all
 * 6 options via hilite_option() below, then loops handling up/down/home/
 * end navigation until Enter or Escape, redrawing the title bar's
 * cycling pacman-icon animation periodically. Returns the newly-chosen
 * 0-based option index.
 */
int pacwars_menu(int curr_option)
{
    static int demo_pacman;
    int i;
    int count;
    int blink_phase;
    unsigned int inkey, ext;
    int key;

    blink_phase = 0;
    count = 0;

    hilite_option(-2, 0);
    for (i = 0; i < 6; i++) {
        hilite_option(-1, i);
    }

    kb_flush();
    set_key_vect(1, key_pause);
    hilite_option(1, curr_option);

    do {
        count++;
        if (count > 10) {
            count = 0;
            hilite_option(-3, demo_pacman | (blink_phase << 4));
            if (blink_phase >= 20) {
                demo_pacman++;
                if (demo_pacman >= 10) {
                    demo_pacman = 0;
                }
                blink_phase = 0;
            } else {
                blink_phase++;
            }
        }

        key = read_key();
        if (key == 1) {
            kb_event(&inkey, &ext);
            if (inkey == 0 && ext == 0x48 && curr_option > 0) {
                hilite_option(0, curr_option);
                curr_option--;
                hilite_option(1, curr_option);
            }
            if (inkey == 0 && ext == 0x50 && curr_option < 5) {
                hilite_option(0, curr_option);
                curr_option++;
                hilite_option(1, curr_option);
            }
            if (inkey == 0 && ext == 0x47 && curr_option > 0) {
                hilite_option(0, curr_option);
                curr_option = 0;
                hilite_option(1, 0);
            }
            if (inkey == 0 && ext == 0x4f && curr_option < 5) {
                hilite_option(0, curr_option);
                curr_option = 5;
                hilite_option(1, 5);
            }
        }
        delay(0xf);
    } while (esc == 0 && inkey != 0x1b && inkey != 0xd);

    if (inkey == 0x1b) {
        esc = 1;
    }
    set_key_vect(0, NULL);
    return curr_option;
}

/*
 * Draws the main menu: the title box + cycling pacman icons (status ==
 * -2 draws the boxes, status == -3 draws one icon-animation frame via
 * display_menu_pacmen() above), or one option row highlighted (status ==
 * 1) or not (status == 0/-1) otherwise. Real signature (int status, int
 * option) recovered from every call site -- the PACWARS.TXT-derived stub
 * had guessed (int curr_pacman, int offset, int x1, int x2, int y), which
 * doesn't match any of them.
 */
void hilite_option(int status, int option)
{
    static char pac_str[20] = "PacWars Version 1.6";
    static char far * menu_str[6] = {
        "Pacwars", "Edit Maze", "Animation", "Edit Characters",
        "Reset HiScore", "Pacwars (No Network)"
    };
    int longest;
    int box_width;
    int x;
    int fore, back;
    unsigned int len;

    longest = (int) strlen(menu_str[5]) + 8;
    box_width = longest * 8;

    if (status == -3) {
        x = (0x140 - box_width) / 2;
        display_menu_pacmen(option & 0xf, option >> 4, x + 4, x + box_width - 0x14, 0xc);
    } else if (status == -2) {
        x = (0x140 - box_width) / 2;
        trfbox(x, 10, box_width, 0x14, 0x1b);
        hline(x, 10, box_width, 0x1d);
        hline(x, 0xb, box_width, 0x1d);
        vline(x, 10, 0x14, 0x1d);
        vline(x + 1, 10, 0x14, 0x1d);
        hline(x, 0x1d, box_width, 0x19);
        hline(x + 1, 0x1c, box_width - 1, 0x19);
        vline(x + box_width - 1, 10, 0x14, 0x19);
        vline(x + box_width - 2, 0xb, 0x13, 0x19);

        len = strlen(pac_str);
        text256((_max_x + (int) (len + 6) * -8) / 2 + 0x18, 0x10, (unsigned char far *) pac_str, 0xe, 0x1b);

        x = (0x140 - box_width) / 2;
        trfbox(x, 0x1e, box_width, 0x96, 0x1b);
        hline(x, 0x1e, box_width, 0x1d);
        hline(x, 0x1f, box_width, 0x1d);
        vline(x, 0x1e, 0x96, 0x1d);
        vline(x + 1, 0x1e, 0x96, 0x1d);
        hline(x, 0xb3, box_width, 0x19);
        hline(x + 1, 0xb2, box_width - 1, 0x19);
        vline(x + box_width - 1, 0x1e, 0x96, 0x19);
        vline(x + box_width - 2, 0x1f, 0x95, 0x19);
    } else {
        len = strlen(menu_str[option]);
        x = (_max_x - (int) len * 8) / 2;
        fore = (status == 1) ? 0xf : 0xb;
        text256(x, option * 0x14 + 0x32, (unsigned char far *) menu_str[option], fore, 0x1b);
        back = (status == 1) ? 0xe : 0x1b;
        trbox(x - 3, option * 0x14 + 0x2f, (int) len * 8 + 6, 0xe, back);
    }
}

/*
 * Lets the player move a highlight cursor over the current room's 25x30
 * attribute grid and stamp attribute values (Normal/Foreground/
 * Background/Bounce/Kill/Warp) onto cells via set_block_attrib(). Real
 * signature (int curr_hoff, int curr_voff, int far * curr_row, int far *
 * curr_col) recovered from edit_maze()'s only call site below -- the
 * PACWARS.TXT-derived stub had guessed (int status, int option), which
 * doesn't match.
 */
/* forward declarations: every internal (non-MAZEEDIT.H-exported) helper
   function below is called by at least one other function that appears
   earlier in this file (choose_edit_maze/edit_maze/block_menu/
   select_block/edit_attributes and their own callees all call each
   other out of textual order), so they all need declaring up front. */
void get_room_pos(int hoff, int voff, int far * x, int far * y);
void draw_room(int hoffset, int voffset);
void create_room_sprite(unsigned char far * sprite, unsigned char far * sp_buff);
void edit_screen1(void);
void hilite_room(int status, int hoff, int voff);
void display_save(int status);
void display_filestatus(int status, int type);
void display_rooms(void);
void edit_maze_rows(char far * maze_str, unsigned int far * inkey, unsigned int far * ext);
void display_maze_rows(int status, char far * maze_str);
void display_scroll(void);
void button(int status, int x, int y, char far * text);
void block_menu(int curr_hoff, int curr_voff, int far * curr_row, int far * curr_col);
void copy_block(char type, int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2, int row3, int col3);
void store_block(int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2);
void restore_block(int curr_hoff, int curr_voff, int row, int col);
void undo_block(int curr_hoff, int curr_voff);
void set_block(int block, int hoff, int voff, int row, int col);
void set_block_attrib(int attrib, int hoff, int voff, int row, int col);
int get_block(int hoff, int voff, int row, int col);
void hilite_block(int status, int hoff, int voff, int row, int col);
void hilite_attrib(int status, int hoff, int voff, int row, int col);
void hilite_select_block(int status, int row, int col, int offset);
void draw_blocks(int offset);
void edit_block(int sprite);
void edit_block_range(int sprite, int w, int h);
void draw_maze_box(void);
void draw_block_box(void);
void draw_block_box2(void);
void draw_attrib_box(void);
void draw_block_range_box(void);
void clear_box(void);
void draw_block_range(int status, int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2);
void draw_block_range2(int status, int row1, int col1, int row2, int col2, int offset);

void edit_attributes(int curr_hoff, int curr_voff, int far * curr_row, int far * curr_col)
{
    int row, col;
    unsigned int inkey, ext;

    _hoffset = curr_hoff;
    _voffset = curr_voff;
    row = *curr_row;
    col = *curr_col;

    draw_attribs();
    draw_attrib_box();
    hilite_attrib(1, curr_hoff, curr_voff, row, col);

    do {
        if (read_key() == 1) {
            kb_event(&inkey, &ext);

            if (inkey == 0 && ext == 0x4d && col < 0x1d) {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                col++;
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x4b && col > 0) {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                col--;
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x48 && row > 0) {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                row--;
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x50 && row < 0x18) {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                row++;
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x47 && (col != 0 || row != 0)) {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                row = 0;
                col = 0;
                hilite_attrib(1, curr_hoff, curr_voff, 0, 0);
            }
            if (inkey == 0 && ext == 0x4f && (col != 0x1d || row != 0x18)) {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                row = 0x18;
                col = 0x1d;
                hilite_attrib(1, curr_hoff, curr_voff, 0x18, 0x1d);
            }

            inkey = toupper(inkey);
            if (inkey == 'N') {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                set_block_attrib(0, curr_hoff, curr_voff, row, col);
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 'F') {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                set_block_attrib(1, curr_hoff, curr_voff, row, col);
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 'G') {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                set_block_attrib(2, curr_hoff, curr_voff, row, col);
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 'B') {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                set_block_attrib(3, curr_hoff, curr_voff, row, col);
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 'K') {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                set_block_attrib(4, curr_hoff, curr_voff, row, col);
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 'W') {
                hilite_attrib(0, curr_hoff, curr_voff, row, col);
                set_block_attrib(5, curr_hoff, curr_voff, row, col);
                hilite_attrib(1, curr_hoff, curr_voff, row, col);
            }
        }
        delay(0xf);
    } while (esc == 0 && inkey != 0x1b);

    if (inkey == 0x1b || esc == 1) {
        esc = 0;
    }
    *curr_row = row;
    *curr_col = col;
}

/*
 * The maze editor's room-grid picker screen: draws the HSIZE x VSIZE grid
 * of rooms (plus the "how many V rooms" digit widget and the Save slot,
 * both modeled as extra cells at voff==-1), and loops handling arrow-key
 * navigation between rooms until Enter (open the highlighted room / save)
 * or Escape. Returns the chosen hoff (also written back through *hoff/
 * *voff), matching MAZEEDIT.H's existing declaration exactly.
 */
int choose_edit_maze(int far * hoff, int far * voff)
{
    int curr_hoff, curr_voff;
    int prev_vsize;
    char vsize_str[2];
    unsigned int inkey, ext;

    curr_hoff = *hoff;
    curr_voff = *voff;

    kb_flush();
    set_key_vect(1, key_pause);
    edit_screen1();
    display_rooms();
    display_save(-1);
    vsize_str[0] = (char) _VSIZE + '0';
    vsize_str[1] = 0;
    display_maze_rows(-1, vsize_str);
    display_scroll();
    hilite_room(1, curr_hoff, curr_voff);

    do {
        prev_vsize = _VSIZE;

        if (curr_voff == -1 && curr_hoff == 1) {
            /* editing the "how many V rooms" digit widget */
            edit_maze_rows(vsize_str, &inkey, &ext);
            _VSIZE = vsize_str[0] - '0';
            if (_VSIZE < 1 || _VSIZE > 4) {
                _VSIZE = prev_vsize;
            }
            inkey = 0;
            ext = 0x4b;
            if (prev_vsize != _VSIZE) {
                display_rooms();
            }
            vsize_str[0] = (char) _VSIZE + '0';
            vsize_str[1] = 0;
            display_maze_rows(-1, vsize_str);
        }

        if ((curr_voff != -1 || curr_hoff != 1) && read_key() == 1) {
            /*
             * NOTE: Ghidra's decompiler produced a large block of garbage
             * here (raw INT21/INT1A software-interrupt calls, __stklen
             * manipulation, zeroing a "mask_buffer", and literally a
             * re-invocation of _main()/_exit()) gated behind this branch.
             * The unconditional jump it decompiled from targets 0x1000:0072,
             * deep in the Borland C startup segment, and doesn't fit this
             * function's local addressing at all -- it's almost certainly
             * unrelated crt0 code that got mis-attributed into this
             * function's control-flow graph, not real program logic. Every
             * structurally-similar function in this codebase (choose_pacman,
             * pacwars_menu) just reads the pending key event here, so
             * that's what's reconstructed below instead of the bogus block.
             */
            kb_event(&inkey, &ext);

            if ((inkey == 0x1b || esc == 1) && (curr_voff != -1 || curr_hoff != 0)) {
                inkey = 0;
                ext = 0x47;
                esc = 0;
                kb_flush();
            }
            if (inkey == 0 && ext == 0x4d &&
                ((curr_voff == -1 && curr_hoff < 1) ||
                 (curr_voff > -1 && curr_hoff < _HSIZE - 1))) {
                hilite_room(0, curr_hoff, curr_voff);
                curr_hoff++;
                hilite_room(1, curr_hoff, curr_voff);
            }
            if (inkey == 0 && ext == 0x4b && curr_hoff > 0) {
                hilite_room(0, curr_hoff, curr_voff);
                curr_hoff--;
                hilite_room(1, curr_hoff, curr_voff);
            }
            if (inkey == 0 && ext == 0x48 && curr_voff > -1) {
                hilite_room(0, curr_hoff, curr_voff);
                if ((_room_offset == 1 && curr_voff == 1) ||
                    (_room_offset == 2 && curr_voff == 2)) {
                    _room_offset--;
                    display_rooms();
                }
                curr_voff--;
                if (curr_voff < 0) {
                    curr_hoff = 0;
                }
                hilite_room(1, curr_hoff, curr_voff);
            }
            if (inkey == 0 && ext == 0x50 && curr_voff < _VSIZE - 1) {
                hilite_room(0, curr_hoff, curr_voff);
                if ((_room_offset == 0 && curr_voff == 1) ||
                    (_room_offset == 1 && curr_voff == 2)) {
                    _room_offset++;
                    display_rooms();
                } else if (curr_voff == -1) {
                    curr_hoff++;
                }
                curr_voff++;
                hilite_room(1, curr_hoff, curr_voff);
            }
            if (inkey == 0 && ext == 0x47 && (curr_hoff != 0 || curr_voff != -1)) {
                hilite_room(0, curr_hoff, curr_voff);
                curr_hoff = 0;
                curr_voff = -1;
                if (_room_offset > 0) {
                    _room_offset = 0;
                    display_rooms();
                }
                hilite_room(1, 0, -1);
            }
            if (inkey == 0 && ext == 0x4f &&
                (curr_hoff != _HSIZE - 1 || curr_voff != _VSIZE - 1)) {
                hilite_room(0, curr_hoff, curr_voff);
                curr_hoff = _HSIZE - 1;
                curr_voff = _VSIZE - 1;
                _room_offset = _VSIZE - 2;
                display_rooms();
                hilite_room(1, curr_hoff, curr_voff);
            }
            if (inkey == 0xd && curr_voff == -1 && curr_hoff == 0) {
                display_filestatus(1, 1);
                save_all_sprites();
                display_filestatus(0, 1);
                inkey = 0;
            }
            if ((inkey == 0x1b || esc == 1) && (curr_voff != -1 || curr_hoff != 0)) {
                inkey = 0;
                esc = 0;
                curr_voff = -1;
                curr_hoff = 0;
            }
        }

        display_scroll();
        delay(0xf);
    } while (esc == 0 && inkey != 0x1b && inkey != 0xd);

    if (inkey == 0x1b) {
        esc = 1;
    }
    set_key_vect(0, NULL);
    *hoff = curr_hoff;
    *voff = curr_voff;
    return curr_hoff;
}

/*
 * The maze editor's main block-placement screen for one room: lets the
 * player move a highlight cursor over the 25x30 block grid, place the
 * currently-selected block (Enter/Space), pick a new block from the
 * palette (P, via select_block()), edit cell attributes (A, via
 * edit_attributes()), or open the block-range menu (B, via block_menu())
 * -- looping until Escape. Real signature matches the PACWARS.TXT-
 * derived stub exactly.
 */
void edit_maze(int curr_hoff, int curr_voff)
{
    int block_offset;
    int row, col;
    int curr_block;
    unsigned int inkey, ext;

    block_offset = 0;
    row = 0;
    col = 0;
    curr_block = 0;
    inkey = 0;

    _hoffset = curr_hoff;
    _voffset = curr_voff;
    set_mode(0x13);
    kb_flush();
    set_key_vect(1, key_pause);
    draw_maze();
    draw_blocks(block_offset);
    draw_maze_box();
    hilite_block(1, curr_hoff, curr_voff, row, col);

    do {
        if (read_key() == 1) {
            kb_event(&inkey, &ext);

            if (inkey == 0 && ext == 0x4d) {
                if (col < 0x1d) {
                    hilite_block(0, curr_hoff, curr_voff, row, col);
                    col++;
                } else {
                    curr_hoff++;
                    if (curr_hoff >= _HSIZE) {
                        curr_hoff = 0;
                    }
                    col = 0;
                    _hoffset = curr_hoff;
                    _voffset = curr_voff;
                    set_clip_window(1);
                    draw_maze();
                    set_clip_window(0);
                }
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x4b) {
                if (col < 1) {
                    if (curr_hoff < 1) {
                        curr_hoff = _HSIZE;
                    }
                    curr_hoff--;
                    col = 0x1d;
                    _hoffset = curr_hoff;
                    _voffset = curr_voff;
                    set_clip_window(1);
                    draw_maze();
                    set_clip_window(0);
                } else {
                    hilite_block(0, curr_hoff, curr_voff, row, col);
                    col--;
                }
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x48) {
                if (row < 1) {
                    if (curr_voff < 1) {
                        curr_voff = _VSIZE;
                    }
                    curr_voff--;
                    row = 0x18;
                    _hoffset = curr_hoff;
                    _voffset = curr_voff;
                    set_clip_window(1);
                    draw_maze();
                    set_clip_window(0);
                } else {
                    hilite_block(0, curr_hoff, curr_voff, row, col);
                    row--;
                }
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x50) {
                if (row < 0x18) {
                    hilite_block(0, curr_hoff, curr_voff, row, col);
                    row++;
                } else {
                    curr_voff++;
                    if (curr_voff >= _VSIZE) {
                        curr_voff = 0;
                    }
                    row = 0;
                    _hoffset = curr_hoff;
                    _voffset = curr_voff;
                    set_clip_window(1);
                    draw_maze();
                    set_clip_window(0);
                }
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x47 && (col != 0 || row != 0)) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                row = 0;
                col = 0;
                hilite_block(1, curr_hoff, curr_voff, 0, 0);
            }
            if (inkey == 0 && ext == 0x4f && (col != 0x1d || row != 0x18)) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                row = 0x18;
                col = 0x1d;
                hilite_block(1, curr_hoff, curr_voff, 0x18, 0x1d);
            }

            if (inkey == 0xd || inkey == 0x20) {
                set_block(curr_block, curr_hoff, curr_voff, row, col);
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }

            if (inkey == 'P' || inkey == 'p') {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                curr_block = select_block(get_block(curr_hoff, curr_voff, row, col), &block_offset);
                hilite_block(1, curr_hoff, curr_voff, row, col);
                clear_box();
                draw_maze_box();
            }

            if (inkey == 'A' || inkey == 'a') {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                edit_attributes(curr_hoff, curr_voff, &row, &col);
                draw_maze();
                draw_blocks(block_offset);
            } else if (inkey == 'B' || inkey == 'b') {
                block_menu(curr_hoff, curr_voff, &row, &col);
                draw_blocks(block_offset);
                clear_box();
            } else {
                goto skip_menu_redraw;
            }
            draw_maze_box();
            hilite_block(1, curr_hoff, curr_voff, row, col);
        }
skip_menu_redraw:
        delay(0xf);
    } while (esc == 0 && inkey != 0x1b);

    if (inkey == 0x1b || esc == 1) {
        esc = 0;
    }
    set_key_vect(0, NULL);
}

/*
 * The block-range menu opened from edit_maze() via 'B': lets the player
 * mark a rectangular range of blocks ('B' to start, arrows to extend,
 * Enter to lock it in), then Store/Restore/Undo/Copy/Move/Delete it.
 * Real signature matches the PACWARS.TXT-derived stub exactly.
 *
 * NOTE: Ghidra's decompiler swapped/mismatched several of this
 * function's own hoff/voff argument registers when passing them through
 * to hilite_block()/draw_block_range()/store_block()/etc (a `curr_voff,
 * (int)voff` pattern where `voff` is actually a reinterpreted curr_row
 * pointer) -- almost certainly the same kind of far-pointer register
 * confusion seen elsewhere in this file, not real behavior. Every callee
 * here (store_block, restore_block, undo_block, copy_block, ...) takes
 * (curr_hoff, curr_voff) as its first two params consistently, so that's
 * what's passed below instead of literally reproducing the garbled args.
 */
void block_menu(int curr_hoff, int curr_voff, int far * curr_row, int far * curr_col)
{
    int row, col;
    int srow, scol, erow, ecol;
    int mode;
    unsigned int inkey, ext;

    row = *curr_row;
    col = *curr_col;
    mode = 0;

    draw_block_range_box();
    hilite_block(1, curr_hoff, curr_voff, row, col);
    ecol = col;
    erow = row;
    scol = col;
    srow = row;

    do {
        if (read_key() == 1) {
            kb_event(&inkey, &ext);

            if (inkey == 0 && ext == 0x4d && col < 0x1d) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                col++;
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x4b && col > 0) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                col--;
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x48 && row > 0) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                row--;
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x50 && row < 0x18) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                row++;
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (inkey == 0 && ext == 0x47 && (col != 0 || row != 0)) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                row = 0;
                col = 0;
                hilite_block(1, curr_hoff, curr_voff, 0, 0);
            }
            if (inkey == 0 && ext == 0x4f && (col != 0x1d || row != 0x18)) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                row = 0x18;
                col = 0x1d;
                hilite_block(1, curr_hoff, curr_voff, 0x18, 0x1d);
            }

            inkey = toupper(inkey);

            if (mode == 1) {
                /* already marking: erase the old range outline, move the
                   cursor, and redraw the outline extended to it */
                draw_block_range(0, curr_hoff, curr_voff, srow, scol, erow, ecol);
                hilite_block(1, curr_hoff, curr_voff, row, col);
                draw_block_range(1, curr_hoff, curr_voff, srow, scol, row, col);
                erow = row;
                ecol = col;
            } else {
                if (mode != 2) {
                    if (inkey != 'B') {
                        goto skip_range_update;
                    }
                    /* start marking a new range at the cursor */
                    mode = 1;
                    erow = row;
                    ecol = col;
                    srow = row;
                    scol = col;
                }
                hilite_block(1, curr_hoff, curr_voff, row, col);
                draw_block_range(mode, curr_hoff, curr_voff, srow, scol, erow, ecol);
            }

skip_range_update:
            if (mode == 1 && inkey == 0xd) {
                /* lock the marked range in */
                mode = 2;
                hilite_block(0, curr_hoff, curr_voff, row, col);
                hilite_block(1, curr_hoff, curr_voff, srow, scol);
                draw_block_range(2, curr_hoff, curr_voff, srow, scol, erow, ecol);
                row = srow;
                col = scol;
            }
            if (inkey == 'S' && mode == 2) {
                store_block(curr_hoff, curr_voff, srow, scol, erow, ecol);
            }
            if (inkey == 'R' &&
                (_maze_srow > 0 || _maze_scol > 0 || _maze_erow > 0 || _maze_ecol > 0)) {
                restore_block(curr_hoff, curr_voff, row, col);
                hilite_block(1, curr_hoff, curr_voff, row, col);
                draw_block_range(mode, curr_hoff, curr_voff, srow, scol, erow, ecol);
            }
            if (inkey == 'U' && _undo_hoff == curr_hoff && _undo_voff == curr_voff) {
                undo_block(curr_hoff, curr_voff);
                mode = 0;
                hilite_block(1, curr_hoff, curr_voff, row, col);
            }
            if (mode == 2 && (inkey == 'C' || inkey == 'M' || inkey == 'D')) {
                copy_block((char) inkey, curr_hoff, curr_voff, srow, scol, erow, ecol, row, col);
                if (inkey == 'M') {
                    erow = row + (erow - srow);
                    ecol = col + (ecol - scol);
                    srow = (row > 0x17) ? 0x18 : row;
                    scol = (col > 0x1c) ? 0x1d : col;
                    if (erow > 0x17) {
                        erow = 0x18;
                    }
                    if (ecol > 0x1c) {
                        ecol = 0x1d;
                    }
                }
                if (inkey == 'D') {
                    mode = 0;
                    hilite_block(1, curr_hoff, curr_voff, srow, scol);
                    row = srow;
                    col = scol;
                } else {
                    hilite_block(1, curr_hoff, curr_voff, row, col);
                    draw_block_range(2, curr_hoff, curr_voff, srow, scol, erow, ecol);
                }
            }
            if (inkey == 0x1b && mode != 0) {
                hilite_block(0, curr_hoff, curr_voff, row, col);
                draw_block_range(0, curr_hoff, curr_voff, srow, scol, erow, ecol);
                mode = 0;
                esc = 0;
                inkey = 0;
                hilite_block(1, curr_hoff, curr_voff, srow, scol);
                row = srow;
                col = scol;
            }
        }

        delay(0xf);
        if (esc != 0 || inkey == 0x1b) {
            draw_block_range(0, curr_hoff, curr_voff, srow, scol, erow, ecol);
            hilite_block(0, curr_hoff, curr_voff, row, col);
            if (inkey == 0x1b || esc == 1) {
                esc = 0;
            }
            /* NOTE: the original never writes row/col back through
               curr_row/curr_col here -- edit_maze()'s cursor position is
               left unchanged by a visit to the block menu, even though
               the cursor visibly moves around on screen while marking a
               range. Preserved as-is rather than "fixed". */
            return;
        }
    } while (1);
}

/*
 * Copies (or moves/deletes, depending on `type`) the marked block range
 * (row1,col1)-(row2,col2) to a new position anchored at (row3,col3).
 * type=='M' (Move) also clears the source range; type=='D' (Delete) just
 * clears the source range and skips the paste entirely. Real signature
 * (char type, ... 8 int params) recovered from block_menu()'s call site
 * -- the PACWARS.TXT-derived stub had guessed a completely different
 * 4-param signature.
 *
 * NOTE: same decompiler memcpy corruption seen in restore_block()/
 * undo_block() below -- backs up the room into the undo buffers before
 * modifying it.
 */
void copy_block(char type, int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2, int row3, int col3)
{
    MAZE_STRUCT far * maze;
    MAZE_STRUCT far * amaze;
    unsigned int buff[25][30];
    unsigned int abuff[25][30];
    int srow, scol, erow, ecol;
    int row, col;
    int drow, dcol;

    srow = (row1 < row2) ? row1 : row2;
    erow = (row2 < row1) ? row1 : row2;
    scol = (col1 < col2) ? col1 : col2;
    ecol = (col2 < col1) ? col1 : col2;

    maze = (MAZE_STRUCT far *) maze_def(curr_hoff, curr_voff);
    amaze = (MAZE_STRUCT far *) attrib_maze_def(curr_hoff, curr_voff);

    memcpy(_undo_buff, maze, sizeof(MAZE_STRUCT));
    memcpy(_undo_abuff, amaze, sizeof(MAZE_STRUCT));
    _undo_hoff = curr_hoff;
    _undo_voff = curr_voff;

    for (row = srow; row <= erow; row++) {
        for (col = scol; col <= ecol; col++) {
            buff[row][col] = maze->def[row][col];
            abuff[row][col] = amaze->def[row][col];
            if (type == 'M' || type == 'D') {
                maze->def[row][col] = 0;
                amaze->def[row][col] = 0;
            }
        }
    }

    drow = row3;
    for (row = srow; row <= erow; row++) {
        dcol = col3;
        for (col = scol; col <= ecol; col++) {
            if (type != 'D' && drow < 0x19 && dcol < 0x1e) {
                maze->def[drow][dcol] = buff[row][col];
                amaze->def[drow][dcol] = abuff[row][col];
            }
            dcol++;
        }
        drow++;
    }

    if (type == 'M' || type == 'D') {
        draw_maze_area(curr_hoff, curr_voff, srow, scol, erow, ecol);
    }
    if (type != 'D') {
        draw_maze_area(curr_hoff, curr_voff, row3, col3, drow - 1, dcol - 1);
    }
}

/*
 * Copies the marked block range (row1,col1)-(row2,col2) of the current
 * room into the _maze_buff/_maze_abuff clipboard buffers, and records
 * the range bounds in _maze_srow/_maze_scol/_maze_erow/_maze_ecol (so
 * restore_block() below knows what to paste). Real signature (6 int
 * params, no `type`) recovered from block_menu()'s call site -- the
 * PACWARS.TXT-derived stub had guessed a `char type` first param that
 * doesn't match (that belongs to copy_block() above instead).
 */
void store_block(int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2)
{
    MAZE_STRUCT far * maze;
    MAZE_STRUCT far * amaze;
    int row, col;

    _maze_srow = (row1 < row2) ? row1 : row2;
    _maze_erow = (row2 < row1) ? row1 : row2;
    _maze_scol = (col1 < col2) ? col1 : col2;
    _maze_ecol = (col2 < col1) ? col1 : col2;

    maze = (MAZE_STRUCT far *) maze_def(curr_hoff, curr_voff);
    amaze = (MAZE_STRUCT far *) attrib_maze_def(curr_hoff, curr_voff);

    for (row = _maze_srow; row <= _maze_erow; row++) {
        for (col = _maze_scol; col <= _maze_ecol; col++) {
            _maze_buff->def[row][col] = maze->def[row][col];
            _maze_abuff->def[row][col] = amaze->def[row][col];
        }
    }
}

/*
 * Pastes the block range previously captured by store_block() (from
 * _maze_buff/_maze_abuff, bounds in _maze_srow.._maze_ecol) at a new
 * position anchored at (row,col). Real signature (4 int params)
 * recovered from block_menu()'s call site -- the PACWARS.TXT-derived
 * stub had guessed 6 params (row1,col1,row2,col2).
 *
 * NOTE: Ghidra's decompiler produced a nonsensical memcpy pair here
 * (source computed from deep inside an unrelated string literal) right
 * before _undo_hoff/_undo_voff get set -- almost certainly the same kind
 * of far-pointer register corruption seen throughout this file, standing
 * in for "back up the room's current state into the undo buffers before
 * pasting over it" (mirrored exactly by undo_block() below, which
 * restores from these same buffers in the reverse direction).
 */
void restore_block(int curr_hoff, int curr_voff, int row, int col)
{
    MAZE_STRUCT far * maze;
    MAZE_STRUCT far * amaze;
    int srow, scol;
    int drow, dcol;

    maze = (MAZE_STRUCT far *) maze_def(curr_hoff, curr_voff);
    amaze = (MAZE_STRUCT far *) attrib_maze_def(curr_hoff, curr_voff);

    memcpy(_undo_buff, maze, sizeof(MAZE_STRUCT));
    memcpy(_undo_abuff, amaze, sizeof(MAZE_STRUCT));
    _undo_hoff = curr_hoff;
    _undo_voff = curr_voff;

    drow = row;
    for (srow = _maze_srow; srow <= _maze_erow; srow++) {
        dcol = col;
        for (scol = _maze_scol; scol <= _maze_ecol; scol++) {
            if (drow < 0x19 && dcol < 0x1e) {
                maze->def[drow][dcol] = _maze_buff->def[srow][scol];
                amaze->def[drow][dcol] = _maze_abuff->def[srow][scol];
            }
            dcol++;
        }
        drow++;
    }
    draw_maze_area(curr_hoff, curr_voff, row, col, drow - 1, dcol - 1);
}

/*
 * Reverts the last copy_block()/restore_block() operation on the given
 * room by restoring it from the undo buffers, then invalidates the undo
 * state (-1/-1, matching what block_menu()'s 'U' handler checks against
 * to decide whether undo is currently available). Real signature (2 int
 * params) recovered from block_menu()'s call site -- the PACWARS.TXT-
 * derived stub had guessed 4 params (row, col too).
 *
 * NOTE: same decompiler memcpy corruption as restore_block() above --
 * reconstructed as the sensible reverse: restore the room from the undo
 * buffers (rather than backing the room up into them again).
 */
void undo_block(int curr_hoff, int curr_voff)
{
    MAZE_STRUCT far * maze;
    MAZE_STRUCT far * amaze;

    maze = (MAZE_STRUCT far *) maze_def(curr_hoff, curr_voff);
    amaze = (MAZE_STRUCT far *) attrib_maze_def(curr_hoff, curr_voff);

    memcpy(maze, _undo_buff, sizeof(MAZE_STRUCT));
    memcpy(amaze, _undo_abuff, sizeof(MAZE_STRUCT));

    _undo_hoff = -1;
    _undo_voff = -1;
    draw_maze_area(curr_hoff, curr_voff, 0, 0, 0x19, 0x1e);
}

/*
 * Draws every currently-visible room in the grid (bounded by _HSIZE/
 * _VSIZE and scrolled by _room_offset -- at most 2 rows are shown at
 * once). Real signature is zero params; the PACWARS.TXT-derived stub had
 * guessed (int curr_hoff, int curr_voff), which doesn't match any call
 * site (all real callers pass no arguments).
 */
void display_rooms(void)
{
    int hoff, voff;
    int x, y;

    for (hoff = 0; hoff < _HSIZE; hoff++) {
        for (voff = _room_offset; voff < _VSIZE && voff - _room_offset < 2; voff++) {
            get_room_pos(hoff, voff, &x, &y);
            trfbox(x, y, 0x5a, 0x4b, 0);
            draw_room(hoff, voff);
        }
    }

    /* with only one V-room, blank out the leftover second grid slot */
    if (_VSIZE == 1) {
        for (hoff = 0; hoff < _HSIZE; hoff++) {
            get_room_pos(hoff, 1, &x, &y);
            trfbox(x, y, 0x5a, 0x4b, 0);
        }
    }
}

/*
 * Read-only attribute-viewer screen (draws the current room's attribute
 * overlay and waits for Escape/Space/Enter). Real signature matches the
 * PACWARS.TXT-derived stub exactly.
 */
void show_attribs(int curr_hoff, int curr_voff)
{
    unsigned int inkey, ext;

    _hoffset = curr_hoff;
    _voffset = curr_voff;
    draw_attribs();

    do {
        if (read_key() == 1) {
            kb_event(&inkey, &ext);
        }
        delay(0xf);
    } while (esc == 0 && inkey != 0x1b && inkey != 0x20 && inkey != 0xd);

    if (inkey == 0x1b || esc == 1) {
        esc = 0;
    }
}

/*
 * Draws one room's wall tiles: reads the room's maze data (maze_def()),
 * and for every nonzero cell, downsamples the matching wall block sprite
 * into a 3x3 preview tile (create_room_sprite()) and blits it via sprite
 * slot 0x4e0. Real signature (2 int params) recovered from draw_room()'s
 * only call site in display_rooms() above -- the PACWARS.TXT-derived
 * stub had guessed a bare void(void).
 */
void draw_room(int hoffset, int voffset)
{
    unsigned char sp_buff[9];
    MAZE_STRUCT far * maze;
    int x, y;
    int row, col;
    int wall_sp;

    _sprites[0x4e0].spritew = 3;
    _sprites[0x4e0].spriteh = 3;
    _sprites[0x4e0].sprite = (unsigned char far *) sp_buff;

    get_room_pos(hoffset, voffset, &x, &y);
    maze = (MAZE_STRUCT far *) maze_def(hoffset, voffset);

    for (row = 0; row < 0x19; row++) {
        _sprites[0x4e0].spritey = y + row * _sprites[0x4e0].spriteh;
        for (col = 0; col < 0x1e; col++) {
            if (maze->def[row][col] != 0) {
                _sprites[0x4e0].spritex = x + col * _sprites[0x4e0].spritew;
                wall_sp = maze->def[row][col] + 0x40;
                create_room_sprite(_sprites[wall_sp].sprite, _sprites[0x4e0].sprite);
                display_sprite(0x4e0);
            }
        }
    }
}

/*
 * The block palette picker opened from edit_maze() via 'P': lets the
 * player scroll through the palette (arrows/PgUp/PgDn/Home/End) and pick
 * a single block (Enter/Space), or mark a rectangular range of palette
 * blocks ('B', same drag-select pattern as block_menu() above) and edit
 * either the single block or the whole range's sprites in place ('E').
 * Real signature matches the PACWARS.TXT-derived stub exactly.
 */
int select_block(int curr_block, int far * offset)
{
    int row, col;
    int mode;
    int srow, scol, erow, ecol;
    unsigned int inkey, ext;

    mode = 0;
    srow = scol = erow = ecol = 0;

    row = curr_block / 0x14;
    if (row >= 0x28) {
        row = 0x28;
    }
    if (row < *offset || *offset + 10 <= row) {
        *offset = row;
        draw_blocks(*offset);
    }
    row = curr_block - *offset * 0x14;
    col = row / 0x14;
    row = row % 0x14;

    clear_box();
    draw_block_box();
    hilite_select_block(1, row, col, *offset);

    do {
        if (read_key() == 1) {
            kb_event(&inkey, &ext);

            if (inkey == 0 && ext == 0x4d) {
                hilite_select_block(0, row, col, *offset);
                if (col < 9) {
                    col++;
                } else if (*offset < 0x28) {
                    (*offset)++;
                    draw_blocks(*offset);
                }
                hilite_select_block(1, row, col, *offset);
            }
            if (inkey == 0 && ext == 0x4b) {
                hilite_select_block(0, row, col, *offset);
                if (col < 1) {
                    if (*offset > 0) {
                        (*offset)--;
                        draw_blocks(*offset);
                    }
                } else {
                    col--;
                }
                hilite_select_block(1, row, col, *offset);
            }
            if (inkey == 0 && ext == 0x48 && row > 0) {
                hilite_select_block(0, row, col, *offset);
                row--;
                hilite_select_block(1, row, col, *offset);
            }
            if (inkey == 0 && ext == 0x50 && row < 0x13) {
                hilite_select_block(0, row, col, *offset);
                row++;
                hilite_select_block(1, row, col, *offset);
            }
            if (inkey == 0 && ext == 0x49 && *offset > 0) {
                *offset -= 9;
                if (*offset < 0) {
                    *offset = 0;
                }
                draw_blocks(*offset);
                hilite_select_block(1, row, col, *offset);
            }
            if (inkey == 0 && ext == 0x51 && *offset < 0x28) {
                *offset += 9;
                if (*offset > 0x28) {
                    *offset = 0x28;
                }
                draw_blocks(*offset);
                hilite_select_block(1, row, col, *offset);
            }
            if (inkey == 0 && ext == 0x47 && (col != 0 || row != 0)) {
                hilite_select_block(0, row, col, *offset);
                row = 0;
                col = 0;
                hilite_select_block(1, 0, 0, *offset);
            }
            /* NOTE: End only fires when NEITHER coordinate is already at
               its max (col<9 && row<0x13), unlike Home's !=0||!=0 check
               above -- an asymmetric condition preserved as-is from the
               original rather than "fixed" to match Home's pattern. */
            if (inkey == 0 && ext == 0x4f && col < 9 && row < 0x13) {
                hilite_select_block(0, row, col, *offset);
                row = 0x13;
                col = 9;
                hilite_select_block(1, 0x13, 9, *offset);
            }

            if ((inkey == 0xd || inkey == 0x20) && mode == 0) {
                curr_block = (col + *offset) * 0x14 + row;
            }

            if ((inkey == 'E' || inkey == 'e') && (*offset != 0 || row != 0 || col != 0)) {
                if (mode == 0) {
                    edit_block(col * 0x14 + row + *offset * 0x14 + 0x40);
                    draw_maze();
                    draw_blocks(*offset);
                } else {
                    edit_block_range(scol * 0x14 + srow + *offset * 0x14 + 0x40,
                                      (ecol - scol) + 1, (erow - srow) + 1);
                    draw_maze();
                    draw_blocks(*offset);
                    draw_block_range2(mode, srow, scol, erow, ecol, *offset);
                }
                hilite_select_block(1, row, col, *offset);
                if (mode == 0) {
                    draw_block_box();
                } else {
                    draw_block_box2();
                }
            }

            inkey = toupper(inkey);

            if (mode == 1) {
                draw_block_range2(0, srow, scol, erow, ecol, *offset);
                hilite_select_block(1, row, col, *offset);
                draw_block_range2(1, srow, scol, row, col, *offset);
                erow = row;
                ecol = col;
            } else if (mode == 2) {
                hilite_select_block(1, row, col, *offset);
                draw_block_range2(2, srow, scol, erow, ecol, *offset);
            } else if (inkey == 'B') {
                mode = 1;
                hilite_select_block(1, row, col, *offset);
                draw_block_range2(1, row, col, row, col, *offset);
                draw_block_box2();
                srow = row;
                scol = col;
                erow = row;
                ecol = col;
            }

            if (mode == 1 && inkey == 0xd) {
                mode = 2;
                hilite_select_block(0, row, col, *offset);
                hilite_select_block(1, srow, scol, *offset);
                draw_block_range2(2, srow, scol, erow, ecol, *offset);
                row = srow;
                col = scol;
            }

            if (inkey == 0x1b && mode != 0) {
                hilite_select_block(0, row, col, *offset);
                draw_block_range2(0, srow, scol, erow, ecol, *offset);
                mode = 0;
                esc = 0;
                inkey = 0;
                hilite_select_block(1, srow, scol, *offset);
                draw_block_box();
                row = srow;
                col = scol;
            }
        }
        delay(0xf);
    } while (esc == 0 && inkey != 0x1b && (mode != 0 || (inkey != 0xd && inkey != 0x20)));

    hilite_select_block(0, row, col, *offset);
    draw_block_range2(0, srow, scol, erow, ecol, *offset);
    return curr_block;
}

/*
 * Downsamples a 24x24 wall-tile sprite into a 3x3 preview block by
 * sampling every 3rd pixel of every 3rd row (offset by one pixel). Real
 * signature (unsigned char far * sprite, unsigned char far * sp_buff)
 * recovered from draw_room()'s call above -- the PACWARS.TXT-derived
 * stub had guessed (int hoffset, int voffset), which doesn't match.
 */
void create_room_sprite(unsigned char far * sprite, unsigned char far * sp_buff)
{
    int row, col, i;

    for (row = 0; row < 3; row++) {
        i = row * 0x18 + 1;
        for (col = 0; col < 3; col++) {
            sp_buff[row * 3 + col] = sprite[i];
            i += 3;
        }
    }
}

/*
 * Draws the editor's "Maze / Editor" title box. Real signature is zero
 * params (and not static -- the PACWARS.TXT-derived stub had guessed
 * static with two far-pointer params, matching neither the real body nor
 * any call site).
 */
void edit_screen1(void)
{
    trfbox(0x14, 0, 0x40, 0x18, 0x1a);
    text256(0x18, 4, (unsigned char far *) "Maze  ", 0xf, 0x1a);
    text256(0x18, 0xc, (unsigned char far *) "Editor", 0xe, 0x1a);
}

/*
 * Highlights (status==1) or un-highlights (status==0) one cell of the
 * room-grid picker. voff==-1 selects one of the two special header
 * cells instead of a room: hoff==1 is the VSIZE digit widget, hoff==0
 * is the Save slot. Real signature (int status, int hoff, int voff)
 * recovered from every call site -- the stub had guessed void(void).
 */
void hilite_room(int status, int hoff, int voff)
{
    int x, y;
    int colour;

    if (voff == -1 && hoff == 1) {
        /* maze_str is unused by display_maze_rows() except when
           status==-1 (the initial draw, done directly by
           choose_edit_maze()), so a blank string is fine here. */
        display_maze_rows(status, "");
    } else if (voff == -1 && hoff == 0) {
        display_save(status);
    } else {
        get_room_pos(hoff, voff, &x, &y);
        colour = (status == 1) ? 0xe : 0;
        trbox(x - 2, y - 2, 0x5e, 0x4f, colour);
    }
}

/*
 * Draws the Save-slot box: status==-1 draws the box shell plus the
 * " Save" label (its one-time initial draw), status==1/0 draw the
 * highlighted/un-highlighted border only. Real signature (1 int param)
 * recovered from hilite_room() above and choose_edit_maze() -- the
 * PACWARS.TXT-derived stub had guessed 3 params.
 */
void display_save(int status)
{
    if (status == -1) {
        trfbox(0x82, 7, 0x36, 0x10, 2);
        trbox(0x84, 9, 0x32, 0xc, 0xf);
        text256(0x86, 0xb, (unsigned char far *) " Save", 0xe, 2);
    }
    if (status == 1) {
        trbox(0x7f, 4, 0x3c, 0x16, 0xe);
    }
    if (status == 0) {
        trbox(0x7f, 4, 0x3c, 0x16, 0);
    }
}

/*
 * Shows one of a small set of loading/saving status messages. When
 * status==1, `type` selects which message ("Loading...", "Saving... ",
 * or "(Max 3X4) ", type 0-2); when status==0, the "(Max 3X4) " message
 * is always shown regardless of type. Real signature (2 int params)
 * recovered from every call site -- the PACWARS.TXT-derived stub had
 * guessed only 1 param.
 */
void display_filestatus(int status, int type)
{
    static char text_str[33] = "Loading...\0Saving... \0(Max 3X4) ";
    unsigned char far * msg;

    if (status == 1) {
        msg = (unsigned char far *) (text_str + type * 0xb);
    } else if (status == 0) {
        msg = (unsigned char far *) (text_str + 0x16);
    } else {
        return;
    }
    text256(0xdc, 0x18, msg, 0xb, 0);
}

/*
 * Stamps `block` into the room's maze grid at (row,col). Real signature
 * (int block, int hoff, int voff, int row, int col) recovered from
 * edit_maze()'s call site -- the PACWARS.TXT-derived stub had guessed
 * (int status, int type), which doesn't match.
 */
void set_block(int block, int hoff, int voff, int row, int col)
{
    MAZE_STRUCT far * maze;

    maze = (MAZE_STRUCT far *) maze_def(hoff, voff);
    maze->def[row][col] = block;
}

/*
 * Stamps `attrib` into the room's attribute grid at (row,col). Real
 * signature matches the PACWARS.TXT-derived stub exactly.
 */
void set_block_attrib(int attrib, int hoff, int voff, int row, int col)
{
    MAZE_STRUCT far * amaze;

    amaze = (MAZE_STRUCT far *) attrib_maze_def(hoff, voff);
    amaze->def[row][col] = attrib;
}

/*
 * Reads the room's maze grid at (row,col). Real signature (int hoff,
 * int voff, int row, int col), returning the block number, recovered
 * from every call site -- the PACWARS.TXT-derived stub had guessed an
 * extra `int attrib` first param that doesn't match (get_block() only
 * reads the maze grid, not the attribute grid).
 */
int get_block(int hoff, int voff, int row, int col)
{
    MAZE_STRUCT far * maze;

    maze = (MAZE_STRUCT far *) maze_def(hoff, voff);
    return maze->def[row][col];
}

/*
 * Draws (or erases, status==0) the highlight box over one maze-grid
 * cell, first drawing whatever block sprite (or blank sprite slot 1) is
 * actually stamped there via sprite slot 1/block+0x40. Real signature
 * (int status, int hoff, int voff, int row, int col) recovered from
 * every call site -- the PACWARS.TXT-derived stub had guessed only 4
 * params (missing status).
 */
void hilite_block(int status, int hoff, int voff, int row, int col)
{
    MAZE_STRUCT far * maze;
    int sprite_num;
    int block;

    maze = (MAZE_STRUCT far *) maze_def(hoff, voff);
    block = maze->def[row][col];

    if (block == 0) {
        sprite_num = 1;
        _sprites[1].spritey = row * _sprites[0x41].spriteh;
        _sprites[1].spritex = col * (unsigned int) _sprites[0x41].spritew;
        _sprites[1].spritew = _sprites[0x41].spritew;
        _sprites[1].spriteh = _sprites[0x41].spriteh;
    } else {
        sprite_num = block + 0x40;
        _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
        _sprites[sprite_num].spritex = col * (unsigned int) _sprites[sprite_num].spritew;
    }
    display_sprite(sprite_num);

    if (status == 1) {
        _sprites[0x45a].spritey = row * _sprites[0x41].spriteh;
        _sprites[0x45a].spritex = col * (unsigned int) _sprites[0x41].spritew;
        or_sprite(0x45a);
    }
}

/*
 * Draws (or erases, status==0) the highlight box over one attribute-grid
 * cell. If the cell has an attribute set, draws the small coloured
 * attribute-overlay box for it (draw_attrib()); otherwise falls back to
 * showing whatever block sprite (or blank slot 1) is stamped there, same
 * as hilite_block() above. Real signature matches the PACWARS.TXT-
 * derived stub exactly.
 */
void hilite_attrib(int status, int hoff, int voff, int row, int col)
{
    MAZE_STRUCT far * maze;
    MAZE_STRUCT far * amaze;
    int sprite_num;
    int attrib;
    int block;

    maze = (MAZE_STRUCT far *) maze_def(hoff, voff);
    amaze = (MAZE_STRUCT far *) attrib_maze_def(hoff, voff);
    attrib = amaze->def[row][col];

    if (attrib > 0) {
        draw_attrib(row, col, attrib);
        sprite_num = 1;
        display_sprite(sprite_num);
    } else {
        block = maze->def[row][col];
        if (block == 0) {
            sprite_num = 1;
            _sprites[1].spritey = row * _sprites[1].spriteh;
            _sprites[1].spritex = col * (unsigned int) _sprites[1].spritew;
        } else {
            sprite_num = block + 0x40;
            _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
            _sprites[sprite_num].spritex = col * (unsigned int) _sprites[sprite_num].spritew;
        }
        display_sprite(sprite_num);
    }

    if (status == 1) {
        _sprites[0x45a].spritey = row * _sprites[0x41].spriteh;
        _sprites[0x45a].spritex = col * (unsigned int) _sprites[0x41].spritew;
        or_sprite(0x45a);
    }
}

/*
 * Draws (or erases, status==0) the highlight box over one palette-grid
 * cell (row,col), scrolled by `offset` (in units of 10 palette rows);
 * (row,col,offset)==(0,0,0) is the special "none" slot at the top of the
 * palette (sprite 1, offscreen to the right at x==0xf0). Real signature
 * (int status, int row, int col, int offset) recovered from every call
 * site -- the PACWARS.TXT-derived stub had guessed (int status, int
 * hoff, int voff, int row, int col), which doesn't match.
 */
void hilite_select_block(int status, int row, int col, int offset)
{
    int sprite_num;

    if (offset == 0 && row == 0 && col == 0) {
        sprite_num = 1;
        _sprites[1].spritey = 0;
        _sprites[1].spritex = 0xf0;
        _sprites[1].spritew = _sprites[0x41].spritew;
        _sprites[1].spriteh = _sprites[0x41].spriteh;
    } else {
        sprite_num = (offset + col) * 0x14 + row + 0x40;
        _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
        _sprites[sprite_num].spritex = col * (unsigned int) _sprites[sprite_num].spritew + 0xf0;
    }
    display_sprite(sprite_num);

    if (status == 1) {
        _sprites[0x45a].spritey = row * _sprites[0x41].spriteh;
        _sprites[0x45a].spritex = col * (unsigned int) _sprites[0x41].spritew + 0xf0;
        or_sprite(0x45a);
    }
}

/*
 * Converts a room grid cell (hoff, voff) into its screen pixel position,
 * accounting for the current scroll offset (_room_offset). Real
 * signature (int hoff, int voff, int far * x, int far * y) recovered
 * from every call site -- completely different from the PACWARS.TXT-
 * derived stub's guessed (int status, int row, int col, int offset).
 */
void get_room_pos(int hoff, int voff, int far * x, int far * y)
{
    *x = hoff * 0x5d + 0x14;
    *y = voff * 0x4e + 0x2d - _room_offset * 0x4e;
}

/*
 * Draws the whole 10x20 visible slice of the block palette, scrolled by
 * `offset` (in units of 10 palette rows), including the special "none"
 * slot at the very top when offset==0. Real signature (1 int param)
 * recovered from every call site -- the PACWARS.TXT-derived stub had
 * guessed (int hoff, int voff, int far * x, int far * y), which doesn't
 * match any of them.
 */
void draw_blocks(int offset)
{
    int sprite_num;
    int row, col;

    sprite_num = (offset > 0) ? offset * 0x14 + 0x40 : offset * 0x14 + 0x41;

    for (col = 0; col < 10; col++) {
        for (row = 0; row < 0x14; row++) {
            if (offset == 0 && col == 0 && row == 0) {
                _sprites[1].spritey = 0;
                _sprites[1].spritex = 0xf0;
                _sprites[1].spritew = 8;
                _sprites[1].spriteh = 8;
                display_sprite(1);
            } else {
                _sprites[sprite_num].spritey = row * _sprites[0x41].spriteh;
                _sprites[sprite_num].spritex = col * (unsigned int) _sprites[0x41].spritew + 0xf0;
                display_sprite(sprite_num);
                sprite_num++;
            }
            if (sprite_num == 0x429) {
                return;
            }
        }
    }
}

/*
 * Opens the sprite editor (SPRITGEN.C, not yet decompiled) on a single
 * block sprite. Real signature matches the PACWARS.TXT-derived stub in
 * shape (1 int param), just renamed to match its real meaning (a sprite
 * index, not an "offset").
 *
 * NOTE: sprite_gen()'s second parameter is declared in SPRITGEN.H as an
 * array of far string pointers (char far * far *), but the real call
 * here passes the address of a single local char buffer holding one
 * name string directly -- SPRITGEN.H's stub signature (guessed, like
 * many in this project, from an incomplete type-def list) looks
 * suspect, but fixing it is out of scope here since sprite_gen() itself
 * hasn't been decompiled yet; cast through to match its current
 * declaration.
 */
void edit_block(int sprite)
{
    static char name[6] = "BLOCK";
    int font;

    font = SetTextFont(-1);
    SetTextFont(4);
    set_mode(0x13);
    copy_sprite_to_gen(_sprites + sprite);
    sprite_gen(1, (char far * far *) name);
    copy_gen_to_sprite(_sprites + sprite);
    SetTextFont(font);
    set_mode(0x13);
}

/*
 * Opens the sprite editor (SPRITGEN.C, not yet decompiled) on a whole
 * w x h rectangular range of block sprites at once, starting at sprite
 * index `sprite` (row-major, MAZEEDIT.H's block-grid stride of 0x14).
 * Real signature (3 int params) recovered from select_block()'s call
 * site -- the PACWARS.TXT-derived stub had guessed only 1 param.
 */
void edit_block_range(int sprite, int w, int h)
{
    static char name[12] = "BLOCK RANGE";
    int font;
    int row, col;

    font = SetTextFont(-1);
    SetTextFont(4);
    set_mode(0x13);

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            copy_sprite_range_to_gen(row, col, _sprites + sprite + row + col * 0x14);
        }
    }
    sprite_gen(1, (char far * far *) name);
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            copy_gen_to_sprite_range(row, col, _sprites + sprite + row + col * 0x14);
        }
    }

    SetTextFont(font);
    set_mode(0x13);
}

/*
 * Runs the single-digit text-entry widget for the VSIZE ("how many V
 * rooms") field. Real signature (char far * maze_str, unsigned int far *
 * inkey, unsigned int far * ext) matches the PACWARS.TXT-derived stub's
 * guess almost exactly -- a rare case where the guess was already right
 * (just with the wrong parameter names/types, corrected here).
 */
void edit_maze_rows(char far * maze_str, unsigned int far * inkey, unsigned int far * ext)
{
    int key;

    /* NOTE: same non-enumerated edit-type value 3 seen in MAZEUTIL.C's
       edit_name() -- see the comment there. */
    key = edit_str(inkey, ext, 1, 0x1c, maze_str, (EDIT_TYPE) 3, -1);
    if (key == 0x1b) {
        esc = 1;
    }
}

/*
 * Writes the currently-loaded maze (_HSIZE/_VSIZE plus every room's
 * maze/attribute data) to an already-open file. Real signature matches
 * the PACWARS.TXT-derived stub exactly.
 *
 * NOTE: Ghidra's decompiler showed each write()'s byte-count argument as
 * a stray register value (in one case literally the string-segment
 * constant 0x340e) rather than a real length -- reconstructed here as
 * the obviously-intended sizeof(int)/sizeof(MAZE_STRUCT), matching what
 * load_maze()/conv_maze() below read back.
 */
int save_maze(int fd)
{
    int hoff, voff;

    write(fd, &_HSIZE, sizeof(int));
    write(fd, &_VSIZE, sizeof(int));

    for (hoff = 0; hoff < 3; hoff++) {
        for (voff = 0; voff < 4; voff++) {
            write(fd, maze_def(hoff, voff), sizeof(MAZE_STRUCT));
        }
    }
    for (hoff = 0; hoff < 3; hoff++) {
        for (voff = 0; voff < 4; voff++) {
            write(fd, attrib_maze_def(hoff, voff), sizeof(MAZE_STRUCT));
        }
    }
    return 0;
}

/*
 * Reads a maze saved by save_maze() above (current format) from an
 * already-open file, replacing the currently-loaded maze. Real signature
 * matches the PACWARS.TXT-derived stub exactly.
 */
int load_maze(int fd)
{
    int hoff, voff;

    read(fd, &_HSIZE, sizeof(int));
    read(fd, &_VSIZE, sizeof(int));

    for (hoff = 0; hoff < 3; hoff++) {
        for (voff = 0; voff < 4; voff++) {
            read(fd, maze_def(hoff, voff), sizeof(MAZE_STRUCT));
        }
    }
    for (hoff = 0; hoff < 3; hoff++) {
        for (voff = 0; voff < 4; voff++) {
            read(fd, attrib_maze_def(hoff, voff), sizeof(MAZE_STRUCT));
        }
    }
    return 0;
}

/*
 * Reads an OLDER maze file format (each cell stored as 1 byte instead of
 * the current format's 2-byte int) from an already-open file, widening
 * each byte back out into the current MAZE_STRUCT layout. Real signature
 * matches the PACWARS.TXT-derived stub exactly.
 */
int conv_maze(int fd)
{
    unsigned char old[25][30];
    int hoff, voff;
    int row, col;
    MAZE_STRUCT far * maze;

    read(fd, &_HSIZE, sizeof(int));
    read(fd, &_VSIZE, sizeof(int));

    for (hoff = 0; hoff < 3; hoff++) {
        for (voff = 0; voff < 4; voff++) {
            maze = (MAZE_STRUCT far *) maze_def(hoff, voff);
            read(fd, old, sizeof(old));
            for (row = 0; row < 0x19; row++) {
                for (col = 0; col < 0x1e; col++) {
                    maze->def[row][col] = old[row][col];
                }
            }
        }
    }
    for (hoff = 0; hoff < 3; hoff++) {
        for (voff = 0; voff < 4; voff++) {
            maze = (MAZE_STRUCT far *) attrib_maze_def(hoff, voff);
            read(fd, old, sizeof(old));
            for (row = 0; row < 0x19; row++) {
                for (col = 0; col < 0x1e; col++) {
                    maze->def[row][col] = old[row][col];
                }
            }
        }
    }
    return 0;
}

/*
 * Draws the "how many V rooms" digit widget: status==-1 draws the digit
 * text plus the "V Rooms" label and clears the file-status message
 * (its one-time initial draw), status==1/0 draw the highlighted/un-
 * highlighted border only. Real signature (int status, char far *
 * maze_str) recovered from every call site -- the PACWARS.TXT-derived
 * stub had guessed 3 params of the wrong types entirely.
 */
void display_maze_rows(int status, char far * maze_str)
{
    if (status == -1) {
        text256(0xe0, 8, (unsigned char far *) maze_str, 0xf, 0);
        text256(0xf0, 8, (unsigned char far *) "V Rooms", 0xb, 0);
        display_filestatus(0, 0);
    }
    if (status == 1) {
        trbox(0xd5, 5, 0x16, 0xe, 0xe);
    }
    if (status == 0) {
        trbox(0xd5, 5, 0x16, 0xe, 0);
    }
}

/*
 * Draws the up/down room-grid scroll buttons, enabled based on whether
 * there's more content to scroll to (_room_offset > 0 for up, VSIZE -
 * _room_offset > 2 for down). Real signature is zero params; the
 * PACWARS.TXT-derived stub had guessed (int status, char far *
 * maze_str), which doesn't match any call site.
 */
void display_scroll(void)
{
    int up_status, down_status;

    up_status = (_room_offset > 0);
    down_status = (_VSIZE - _room_offset > 2);

    button(up_status, 0, 0x2d, up_status ? "\x18" : "");
    button(down_status, 0, 0x4b, down_status ? "\x19" : "");
}

/*
 * Draws a single 14x14 button: status==1 draws a highlighted, labeled
 * button (a single glyph char, e.g. the up/down scroll-arrow glyphs
 * 0x18/0x19), status==0 draws a plain blank (disabled) box. Real
 * signature (int status, int x, int y, char far * text) recovered from
 * display_scroll()'s call sites above -- the PACWARS.TXT-derived stub
 * had guessed a bare void(void).
 */
void button(int status, int x, int y, char far * text)
{
    if (status == 1) {
        trfbox(x, y, 0xe, 0xe, 0x19);
        hline(x, y, 0xe, 0x1d);
        vline(x, y, 0xe, 0x1d);
        hline(x, y + 0xd, 0xe, 0x17);
        vline(x + 0xd, y, 0xe, 0x17);
        text256(x + 3, y + 3, (unsigned char far *) text, 0x1d, 0x19);
    } else {
        trfbox(x, y, 0xe, 0xe, 0);
    }
}

/*
 * Draws the small bottom-right instructions box shown while editing the
 * maze grid (edit_maze()): one line per key/action. Real signature is
 * zero params (a fixed, hardcoded message); the PACWARS.TXT-derived stub
 * had guessed (int status, int x, int y, char far * text), which doesn't
 * match any call site.
 */
void draw_maze_box(void)
{
    static char far * text_str[] = {
        "P Pick Object", "A Attributes", "B Block", NULL
    };
    int i, count;
    int y;
    int font;

    for (count = 0; text_str[count] != NULL; count++) {
    }
    y = _max_y - count * 8;
    trfbox(0xf0, y, 0x50, count * 8, 0);
    trbox(0xf0, y, 0x50, count * 8, 0xe);
    y += 4;

    font = SetTextFont(-1);
    SetTextFont(4);
    for (i = 0; text_str[i] != NULL; i++) {
        text256(0xf4, y, (unsigned char far *) text_str[i], 0xf, 0);
        y += 8;
    }
    SetTextFont(font);
}

/*
 * Draws the instructions box shown while a single block is selected in
 * select_block(): "E Edit Object" / "B Block Start". Real signature
 * matches the PACWARS.TXT-derived stub exactly.
 */
void draw_block_box(void)
{
    static char far * text_str[] = {
        "E Edit Object", "B Block Start", NULL
    };
    int i, count;
    int y;
    int font;

    for (count = 0; text_str[count] != NULL; count++) {
    }
    y = _max_y - count * 8;
    trfbox(0xf0, y, 0x50, count * 8, 0);
    trbox(0xf0, y, 0x50, count * 8, 0xe);
    y += 4;

    font = SetTextFont(-1);
    SetTextFont(4);
    for (i = 0; text_str[i] != NULL; i++) {
        text256(0xf4, y, (unsigned char far *) text_str[i], 0xf, 0);
        y += 8;
    }
    SetTextFont(font);
}

/*
 * Draws the instructions box shown while marking/holding a block range
 * in select_block(): "K Block End" / "E Edit Block". Real signature
 * matches the PACWARS.TXT-derived stub exactly.
 */
void draw_block_box2(void)
{
    static char far * text_str[] = {
        "K Block End", "E Edit Block", NULL
    };
    int i, count;
    int y;
    int font;

    for (count = 0; text_str[count] != NULL; count++) {
    }
    y = _max_y - count * 8;
    trfbox(0xf0, y, 0x50, count * 8, 0);
    trbox(0xf0, y, 0x50, count * 8, 0xe);
    y += 4;

    font = SetTextFont(-1);
    SetTextFont(4);
    for (i = 0; text_str[i] != NULL; i++) {
        text256(0xf4, y, (unsigned char far *) text_str[i], 0xf, 0);
        y += 8;
    }
    SetTextFont(font);
}

/*
 * Draws the instructions box shown while editing attributes
 * (edit_attributes()): the 6 attribute-key labels plus a blank spacer
 * line and the 4 arrow-key labels. Real signature matches the
 * PACWARS.TXT-derived stub exactly.
 */
void draw_attrib_box(void)
{
    static char far * text_str[] = {
        "N Normal", "F Foreground", "G Background", "B Bounce", "K Kill",
        "W Warp", "", "U Up", "D Down", "L Left", "R Right", NULL
    };
    int i, count;
    int y;
    int font;

    for (count = 0; text_str[count] != NULL; count++) {
    }
    y = _max_y - count * 8;
    trfbox(0xf0, y, 0x50, count * 8, 0);
    trbox(0xf0, y, 0x50, count * 8, 0xe);
    y += 4;

    font = SetTextFont(-1);
    SetTextFont(4);
    for (i = 0; text_str[i] != NULL; i++) {
        text256(0xf4, y, (unsigned char far *) text_str[i], 0xf, 0);
        y += 8;
    }
    SetTextFont(font);
}

/*
 * Draws the instructions box shown in block_menu(): "B Block Start" /
 * "K Block End" plus the range-operation key labels (Copy/Move/Delete/
 * Store/Restore/Undo). Real signature matches the PACWARS.TXT-derived
 * stub exactly.
 */
void draw_block_range_box(void)
{
    static char far * text_str[] = {
        "B Block Start", "K Block End", "C Copy", "M Move", "D Delete",
        "S Store", "R Restore", "U Undo", NULL
    };
    int i, count;
    int y;
    int font;

    for (count = 0; text_str[count] != NULL; count++) {
    }
    y = _max_y - count * 8;
    trfbox(0xf0, y, 0x50, count * 8, 0);
    trbox(0xf0, y, 0x50, count * 8, 0xe);
    y += 4;

    font = SetTextFont(-1);
    SetTextFont(4);
    for (i = 0; text_str[i] != NULL; i++) {
        text256(0xf4, y, (unsigned char far *) text_str[i], 0xf, 0);
        y += 8;
    }
    SetTextFont(font);
}

/*
 * Blanks out the bottom-right instructions box area (a fixed max size
 * covering every draw_*_box() variant above). Real signature matches
 * the PACWARS.TXT-derived stub exactly.
 */
void clear_box(void)
{
    trfbox(0xf0, _max_y - 0x28, 0x50, 0x28, 0);
}

/*
 * Draws (status==1/2, different highlight colours for "marking" vs
 * "locked") or erases (status==0) the outline around a rectangular block
 * range in the maze grid, used by block_menu() above while dragging out
 * a range selection. status==0 erases via hilite_block() cell-by-cell
 * (redrawing each cell's real content) rather than a plain box, since
 * the outline sits on top of live maze content. Real signature (7 int
 * params) recovered from block_menu()'s call sites -- the PACWARS.TXT-
 * derived stub had guessed zero params.
 */
void draw_block_range(int status, int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2)
{
    int srow, erow, scol, ecol;
    int row, col;
    int colour;

    srow = (row1 < row2) ? row1 : row2;
    erow = (row2 < row1) ? row1 : row2;
    scol = (col1 < col2) ? col1 : col2;
    ecol = (col2 < col1) ? col1 : col2;

    if (status == 0) {
        for (row = srow; row <= erow; row++) {
            if (row == srow || row == erow) {
                for (col = scol; col <= ecol; col++) {
                    hilite_block(0, curr_hoff, curr_voff, row, col);
                }
            } else {
                hilite_block(0, curr_hoff, curr_voff, row, scol);
                hilite_block(0, curr_hoff, curr_voff, row, ecol);
            }
        }
    } else {
        colour = (status == 1) ? 0xe : 0xc;
        trbox(scol << 3, srow << 3, ((ecol - scol) + 1) * 8, ((erow - srow) + 1) * 8, colour);
    }
}

/*
 * Same as draw_block_range() above but for the palette grid in
 * select_block() (offset 0xf0 to the right, scrolled by `offset` instead
 * of tied to a room). status==0 erases via hilite_select_block()
 * cell-by-cell; status==1/2 draw a clipped outline (only the left/right
 * edges that are still on-screen after clamping to the palette's visible
 * column range get their vertical border drawn). Real signature (6 int
 * params: status, row1, col1, row2, col2, offset) recovered from
 * select_block()'s call sites -- the PACWARS.TXT-derived stub had
 * guessed (int status, int curr_hoff, int curr_voff, int row1, int col1,
 * int row2, int col2), which doesn't match (this operates on the
 * palette, not a room, and has no curr_hoff/curr_voff).
 */
void draw_block_range2(int status, int row1, int col1, int row2, int col2, int offset)
{
    int srow, erow, scol, ecol;
    int row, col;
    int x, y, w, h;
    int x2;
    int colour;

    srow = (row1 < row2) ? row1 : row2;
    erow = (row2 < row1) ? row1 : row2;
    scol = (col1 < col2) ? col1 : col2;
    ecol = (col2 < col1) ? col1 : col2;

    if (status == 0) {
        for (row = srow; row <= erow; row++) {
            if (row == srow || row == erow) {
                for (col = scol; col <= ecol; col++) {
                    hilite_select_block(0, row, col, offset);
                }
            } else {
                hilite_select_block(0, row, scol, offset);
                hilite_select_block(0, row, ecol, offset);
            }
        }
    } else {
        x = scol * 8 + 0xf0;
        y = srow * 8;
        h = ((erow - srow) + 1) * 8;
        x2 = x + ((ecol - scol) + 1) * 8 - 1;
        if (x < 0xf1) {
            x = 0xf0;
        }
        if (x2 > 0x13e) {
            x2 = 0x13f;
        }
        w = (x2 - x) + 1;
        colour = (status == 1) ? 0xe : 0xc;

        hline(x, y, w, colour);
        hline(x, y + h - 1, w, colour);
        if (scol * 8 + 0xf0 == x) {
            vline(x, y, h, colour);
        }
        if (scol * 8 + ((ecol - scol) + 1) * 8 + 0xef == x2) {
            vline(x + w - 1, y, h, colour);
        }
    }
}

/*
 * NOTE: not found anywhere in Ghidra's function list under this name (or
 * any obvious variant) -- searched exhaustively, no match. Its stub
 * signature was guessed from PACWARS.TXT's module-local type-def list
 * the same way as every other function in this file, but unlike those,
 * there's no corresponding real function address to decompile against.
 * Possibilities: it's genuinely dead code the compiler stripped, a
 * module-local helper Ghidra's analysis merged into a neighboring
 * function without a separate symbol, or it belongs to a different
 * module than PACWARS.TXT's table implies (the same class of stub-
 * generation error seen elsewhere in this project, e.g. alloc_maze_
 * editor_mem() and button() being silently missing, or MANEDIT.C's
 * name collisions with this file). Left as a stub pending further
 * investigation; not linked to or called from anywhere else in
 * MAZEEDIT.C.
 */
void edit_animate(int status, int row1, int col1, int row2, int col2, int offset)
{
}

