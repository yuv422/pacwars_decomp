/*
 * SPRITGEN.C
 *
 * The in-game sprite/pacman-mask editor ("sprite generator"): a small
 * paint-program built on top of GRAPH256.C-style primitives, used by the
 * maze/character editors to create and touch up the 80x54-cell sprite
 * bitmaps stored in *.SPT files.
 *
 * Reconstructed from Ghidra decompilation/disassembly of segment 1ae7 in
 * PACWARS.EXE. Only the 6 functions declared in SPRITGEN.H are called from
 * outside this module (confirmed via get_xrefs_to); everything else here
 * is file-scope static.
 *
 * Two stub functions present in the original stub-generation pass,
 * load_palette() and read_array(), were determined NOT to belong to this
 * module at all -- Ghidra places their real addresses in segment 2b31
 * (PICLOAD.C's module), so they have been removed here rather than
 * implemented. A third stub, a wrongly-signatured text256(), was a
 * fabrication of the same stub pass; Ghidra's own database already names
 * address 1ae7:275d (originally flagged unk_func_275D) as text256, with a
 * different (correct) signature, implemented below.
 */
#include "SPRITGEN.H"
#include "MEDITSTR.H"
#include "UTILS.H"
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys\stat.h>
#include <ctype.h>

/*
 * Working buffers and state, all confirmed static via get_xrefs_to (every
 * reference resolves to an address inside this module's own segment
 * 1ae7). Sizes: _max_w * _max_h = 80 * 54 = 4320 bytes, the largest
 * sprite this editor supports.
 */
static int _sprite_w = 0x14;              /* width of the sprite currently being edited */
static int _sprite_h = 0x14;              /* height of the sprite currently being edited */
static int _bitw = 7;                  /* on-screen pixel width of one sprite cell (grid zoom) */
static int _bith = 7;                  /* on-screen pixel height of one sprite cell (grid zoom) */

/*
 * Never written anywhere in the program (confirmed via get_xrefs_to --
 * every reference is a read), only ever initialized once at load time.
 * Values recovered directly from static memory (340e:112e/1130/1132/1134).
 */
static int _max_w = 80;
static int _max_h = 54;
static int _grid_w = 216;
static int _grid_h = 190;

static unsigned int _colour_index; /* currently selected palette colour */
static int _sprite_cursor;         /* linear cursor position (col * h + row) in edit_sprite */
static int _mask_cursor;           /* linear cursor position (col * h + row) in edit_mask */

static unsigned char _sprite_buffer[4320];
static unsigned char _scratch_buffer[4320];
static unsigned char _mask_buffer[4320];

/*
 * Scratch space for cut/paste (F3..F6 in edit_sprite). Ghidra's own
 * auto-detected size for this array is 3600 bytes, but copy_to_clip()/
 * copy_from_clip() address it as two 4320-byte slots selected by their
 * "index" parameter (0 or 1) -- confirmed by the disassembly, which
 * computes dest/src as _clipboard + index * 4320 + row * 80. The
 * auto-detected size is too small; declared correctly here.
 */
static unsigned char _clipboard[8640];

/* animate_sprite's own backup of the mask, separate from _scratch_buffer
 * (which it uses to back up the sprite pixels) since it must preserve
 * both across the frames it flips through. Recovered from the raw
 * disassembly, which addresses a second scratch block at 340e:c580. */
static unsigned char _anim_mask_save[4320];

/* forward declarations -- called out of textual definition order */
static int save_routine(char far *file_name);
static int edit_sprite(int xmargin, int ymargin, int far *sprite_index, int num_sprites, int init);
static int edit_mask(int xmargin, int ymargin);
static void swap_sprite_mask(void);
static void restore_mask(void);
static void copy_to_clip(int index, int w, int h, int xm, int ym);
static void copy_from_clip(int index, int w, int h, int xm, int ym);
static void hflip(int w, int h, int xm, int ym);
static void vflip(int w, int h, int xm, int ym);
static void rotate_90(int w, int h, int xm, int ym);
static void rotate_45(int w, int h, int xm, int ym);
static int move_sprite(int w, int h, int xm, int ym);
static int size_sprite(int xm, int ym);
static int animate_sprite(int xm, int ym, int sprite_index, int num_sprites, char far * far *sprite_files);
static void clear_sprite(int w, int h);
static void draw_sprite(int w, int h, int xmargin, int ymargin, int status);
static void draw_grid(int xm, int ym, int w, int h);
static void pixel_hlite(int status, int index, int xmargin, int ymargin, int h);
static void set_pixel_hlite(int index, int xmargin, int ymargin, int h, int colour);
static void draw_palette_colour(int colour);
static void control_text(char far *text_ptr);
static void grid_pos(int x, int y);
static void grid_size(void);
static void draw_file_name(char far *text_ptr);
static void draw_palette(void);
static int select_colour(void);
static void help_commands(void);
static void hlite(int status, int index, int xmargin, int ymargin, int w, int h, int xsep, int ysep);
static void trbox(int x, int y, int w, int h, int colour);
static void trfbox(int x, int y, int w, int h, int colour);
static void hline(int x, int y, int length, int colour);
static void vline(int x, int y, int length, int colour);
static void set_pixel(int x, int y, int colour);
static void read_sprite(char far *file_name);
static int write_sprite(char far *file_name);
static void text256(int x, int y, unsigned char far *text_ptr, int fore, int back);

/*
 * sprite_gen -- the top-level editor loop.
 *
 * Doesn't read the keyboard itself: it dispatches purely on the letter
 * returned by whichever sub-tool it last called (edit_sprite, edit_mask,
 * move_sprite, size_sprite, select_colour, animate_sprite), each of which
 * polls its own keys and returns a letter/ESC telling sprite_gen which
 * tool to hand off to next.
 */
void sprite_gen(int num_sprites, char far * far *sprite_files)
{
    int xm, ym, index;
    int mode;

    /* [0x1134] is _grid_h (a fixed constant, 190), not __disp_page --
     * an earlier pass here substituted the wrong global. ym is always
     * 200 - (190+1) = 9, a small fixed top margin. */
    ym = 200 - (_grid_h + 1);
    index = 0;
    xm = 0;
    mode = 'E';

    draw_palette();
    draw_grid(xm, ym, _sprite_w, _sprite_h);
    draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
    draw_file_name(sprite_files[index]);

    for (;;) {
        if (mode == 'E') {
            mode = edit_sprite(xm, ym, &index, num_sprites, 1);
        } else if (mode == 'K') {
            mode = edit_mask(xm, ym);
        } else if (mode == 'I') {
            help_commands();
        } else if (mode == 'P') {
            mode = select_colour();
        } else if (mode == 'C') {
            int w, h;

            w = _sprite_w;
            h = _sprite_h;
            read_sprite(sprite_files[index]);
            draw_grid(xm, ym, _sprite_w, _sprite_h);
            clear_sprite(w, h);
            draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
        } else if (mode == 'A') {
            mode = animate_sprite(xm, ym, index, num_sprites, sprite_files);
        } else if (mode == 'R') {
            mode = size_sprite(xm, ym);
        } else if (mode == 'M') {
            mode = move_sprite(_sprite_w, _sprite_h, xm, ym);
        } else if (mode == 'S') {
            mode = 'E';
        }
        if (mode == 0x1b) {
            break;
        }
    }
}

/*
 * save_routine -- prompts "Save Sprite... (Y/N)?", lets the user edit the
 * filename, and calls write_sprite(). Called from edit_sprite via F1/L
 * paths in the original binary; reconstructed from the raw disassembly
 * at 1ae7:01a0.
 */
static int save_routine(char far *file_name)
{
    char local_name[128];
    unsigned int inkey, ext;
    int len;

    set_colour(7, 0);
    cls_screen();
    set_colour(7, 0);
    rc_char(4, 0, ' ', 0x50);
    if (confirm_exit((unsigned char far *) "Save Sprite...  (Y/N)?") != 1) {
        return 1;
    }

    strcpy(local_name, file_name);
    len = strlen(local_name);
    clear_str((unsigned char far *) local_name, 12 - len);
    set_colour(7, 0);
    rc_text(4, 0, (unsigned char far *) "File Name :");

    if (edit_str(&inkey, &ext, 4, 12, file_name, edit_type_general, 0) == 0xd) {
        if (trim_spaces((unsigned char far *) file_name) > 0) {
            if (write_sprite(file_name) != 0) {
                rc_text(0, 21, (unsigned char far *) "Cannot Save to File :");
                rc_text(4, 12, (unsigned char far *) file_name);
            }
        }
    }
    return 1;
}

/*
 * edit_sprite -- the main pixel-painting tool.
 *
 * Cursor position is a single linear index into _sprite_buffer using
 * COLUMN-MAJOR order (cursor = col * _sprite_h + row), confirmed by every
 * arrow-key handler dividing/multiplying by _sprite_h rather than
 * _sprite_w. This convention is shared with edit_mask.
 *
 * Reconstructed from the raw disassembly at 1ae7:02da (Ghidra's own
 * function-body metadata for this address was corrupted, spanning nearly
 * the whole program; disassemble_bytes + a bounded re-disassembly over
 * 1ae7:02da..1ae7:07d5 was used to recover it).
 */
static int edit_sprite(int xmargin, int ymargin, int far *sprite_index, int num_sprites, int init)
{
    unsigned int inkey, ext;
    int key;
    int row, col;

    if (init == 1) {
        _sprite_cursor = 0;
        control_text((char far *) "Edit");
        grid_size();
        draw_palette_colour((int) _colour_index);
    }
    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
    grid_pos(_sprite_cursor / _sprite_h + 1, _sprite_cursor % _sprite_h + 1);

    for (;;) {
        row = _sprite_cursor % _sprite_h;
        col = _sprite_cursor / _sprite_h;

        kb_event(&inkey, &ext);
        key = toupper(inkey);

        if (key == 'C') {
            /* Clear: snapshot current sprite, fill with current colour */
            memcpy(_scratch_buffer, _sprite_buffer, 4320);
            memset(_sprite_buffer, (unsigned char) _colour_index, 4320);
            draw_sprite(_sprite_w, _sprite_h, xmargin, ymargin, 1);
        } else if (key == '+') {
            if (*sprite_index < num_sprites - 1) {
                (*sprite_index)++;
            }
        } else if (inkey == 0x1b) {
            pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
            return key;
        } else if (inkey == 0xd || inkey == ' ') {
            /* stamp the current palette colour at the cursor */
            _sprite_buffer[row * 80 + col] = (unsigned char) _colour_index;
            draw_sprite(_sprite_w, _sprite_h, xmargin, ymargin, 0);
            set_pixel_hlite(_sprite_cursor, xmargin, ymargin, _sprite_h, (int) _colour_index);
            continue;
        } else if (key == '-') {
            if (*sprite_index > 0) {
                (*sprite_index)--;
            }
        } else if (key == '9') {
            rotate_90(_sprite_w, _sprite_h, xmargin, ymargin);
        } else if (key == 'A') {
            pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
            return key;
        } else if (inkey >= 'H' && inkey <= 'V') {
            switch (inkey) {
            case 'H':
                hflip(_sprite_w, _sprite_h, xmargin, ymargin);
                break;
            case 'L':
            case 'M':
            case 'R':
            case 'S':
                pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                return inkey;
            case 'P':
                /* pick colour under cursor, then exit into the palette */
                _colour_index = _sprite_buffer[row * 80 + col];
                pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                return inkey;
            case 'U':
                /* undo: restore from the Clear snapshot */
                memcpy(_sprite_buffer, _scratch_buffer, 4320);
                draw_sprite(_sprite_w, _sprite_h, xmargin, ymargin, 1);
                break;
            case 'V':
                vflip(_sprite_w, _sprite_h, xmargin, ymargin);
                break;
            default:
                break;
            }
        } else if (inkey == 0) {
            switch (ext) {
            case 0x3b: /* F1 */
                pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                return 'K';
            case 0x3c: /* F2 */
                pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                return 'K';
            case 0x3d: /* F3: set clipboard slot 0 */
                copy_to_clip(0, _sprite_w, _sprite_h, xmargin, ymargin);
                beep(0x65, 0x259);
                break;
            case 0x3e: /* F4: set clipboard slot 1 */
                copy_to_clip(1, _sprite_w, _sprite_h, xmargin, ymargin);
                beep(0x65, 0x259);
                beep(0x65, 0);
                beep(0x65, 0x259);
                break;
            case 0x3f: /* F5: get clipboard slot 0 */
                copy_from_clip(0, _sprite_w, _sprite_h, xmargin, ymargin);
                pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                return 'E';
            case 0x40: /* F6: get clipboard slot 1 */
                copy_from_clip(1, _sprite_w, _sprite_h, xmargin, ymargin);
                pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                return 'E';
            case 0x47: /* Home */
                if (_sprite_cursor > 0) {
                    pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                    _sprite_cursor = 0;
                    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x48: /* Up */
                if (row > 0) {
                    pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                    _sprite_cursor--;
                    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x4b: /* Left */
                if (col > 0) {
                    pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                    _sprite_cursor -= _sprite_h;
                    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x4d: /* Right */
                if (col < _sprite_w - 1) {
                    pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                    _sprite_cursor += _sprite_h;
                    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x4f: /* End */
                if (_sprite_w * _sprite_h - 1 > _sprite_cursor) {
                    pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                    _sprite_cursor = _sprite_w * _sprite_h - 1;
                    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x50: /* Down */
                if (row < _sprite_h - 1) {
                    pixel_hlite(0, _sprite_cursor, xmargin, ymargin, _sprite_h);
                    _sprite_cursor++;
                    pixel_hlite(1, _sprite_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            default:
                break;
            }
        }
    }
}

/*
 * edit_mask -- the transparency-mask painting tool. Works exactly like
 * edit_sprite's cursor scheme, but temporarily swaps _sprite_buffer and
 * _mask_buffer (via swap_sprite_mask()) so the mask can be painted with
 * the same primitives; restore_mask() (bound to most exit keys) swaps
 * them back.
 *
 * Ghidra mis-split this function's tail (an indirect
 * "JMP word ptr CS:[BX+0xc99]" case-dispatch jump table it didn't trace)
 * into 6 separate pseudo-functions (FUN_1ae7_0ac5/0b43/0b83/0baf/0c5d/
 * 0c95); those are not real functions, just case bodies folded in below.
 */
static int edit_mask(int xmargin, int ymargin)
{
    unsigned int inkey, ext;
    int key;
    int row, col;

    /* The disassembly reads _colour_index into a local here and then
     * unconditionally forces it to 0xf (white) for the duration of mask
     * editing, showing that fixed colour in the preview swatch; nothing
     * in this function restores the original value afterwards, so this
     * override is left in place on return, matching the original. */
    swap_sprite_mask();
    draw_sprite(_sprite_w, _sprite_h, xmargin, ymargin, 1);
    control_text((char far *) "Mask");
    grid_size();
    _colour_index = 0xf;
    draw_palette_colour((int) _colour_index);
    pixel_hlite(1, _mask_cursor, xmargin, ymargin, _sprite_h);
    grid_pos(_mask_cursor / _sprite_h + 1, _mask_cursor % _sprite_h + 1);

    for (;;) {
        row = _mask_cursor % _sprite_h;
        col = _mask_cursor / _sprite_h;

        kb_event(&inkey, &ext);
        key = toupper(inkey);

        if (key == 'C') {
            restore_mask();
            return key;
        } else if (inkey == 0xd || inkey == ' ') {
            _mask_buffer[row * 80 + col] ^= 1;
            draw_sprite(_sprite_w, _sprite_h, xmargin, ymargin, 0);
            set_pixel_hlite(_mask_cursor, xmargin, ymargin, _sprite_h,
                             _mask_buffer[row * 80 + col] != 0 ? 0xf : 8);
        } else if (key == 'E') {
            restore_mask();
            return key;
        } else if (key == 'G') {
            /* Generate: build the mask from the sprite's own colour 0 */
            row = 0;
            for (col = 0; col < _sprite_w; col++) {
                for (row = 0; row < _sprite_h; row++) {
                    _mask_buffer[row * 80 + col] =
                        (_sprite_buffer[row * 80 + col] != 0) ? 1 : 0;
                }
            }
            draw_sprite(_sprite_w, _sprite_h, xmargin, ymargin, 1);
        } else if (key == 'X') {
            restore_mask();
            return key;
        } else if (inkey == 0) {
            switch (ext) {
            case 0x4b: /* Left */
                if (col > 0) {
                    pixel_hlite(0, _mask_cursor, xmargin, ymargin, _sprite_h);
                    _mask_cursor -= _sprite_h;
                    pixel_hlite(1, _mask_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x3b: /* F1 */
                help_commands();
                break;
            case 0x48: /* Up */
                if (row > 0) {
                    pixel_hlite(0, _mask_cursor, xmargin, ymargin, _sprite_h);
                    _mask_cursor--;
                    pixel_hlite(1, _mask_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x4d: /* Right */
                if (col < _sprite_w - 1) {
                    pixel_hlite(0, _mask_cursor, xmargin, ymargin, _sprite_h);
                    _mask_cursor += _sprite_h;
                    pixel_hlite(1, _mask_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            case 0x50: /* Down */
                if (row < _sprite_h - 1) {
                    pixel_hlite(0, _mask_cursor, xmargin, ymargin, _sprite_h);
                    _mask_cursor++;
                    pixel_hlite(1, _mask_cursor, xmargin, ymargin, _sprite_h);
                }
                break;
            default:
                break;
            }
        }
    }
}

/* Swaps _sprite_buffer and _mask_buffer via _scratch_buffer; used by both
 * edit_mask's entry (park the sprite, bring in the mask to paint) and
 * restore_mask (put them back). Both call sites in the original binary
 * perform this exact 3-way copy (just written starting from opposite
 * ends), which is algebraically the same swap either way. */
static void swap_sprite_mask(void)
{
    memcpy(_scratch_buffer, _sprite_buffer, 4320);
    memcpy(_sprite_buffer, _mask_buffer, 4320);
    memcpy(_mask_buffer, _scratch_buffer, 4320);
}

static void restore_mask(void)
{
    swap_sprite_mask();
}

/* xm/ym are accepted (matching the call site's argument count) but never
 * actually used to address the clipboard -- confirmed by the raw
 * disassembly, which only ever indexes by row/w/h/index. */
static void copy_to_clip(int index, int w, int h, int xm, int ym)
{
    int row;

    for (row = 0; row < h; row++) {
        memcpy(&_clipboard[index * 4320 + row * 80], &_sprite_buffer[row * 80], w);
    }
}

static void copy_from_clip(int index, int w, int h, int xm, int ym)
{
    int row, col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            if (_clipboard[index * 4320 + row * 80 + col] != 0) {
                _sprite_buffer[row * 80 + col] = _clipboard[index * 4320 + row * 80 + col];
            }
        }
    }
    draw_sprite(w, h, xm, ym, 1);
}

static void hflip(int w, int h, int xm, int ym)
{
    int row, col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            _scratch_buffer[row * 80 + col] = _sprite_buffer[row * 80 + (w - col - 1)];
        }
    }
    for (row = 0; row < h; row++) {
        memcpy(&_sprite_buffer[row * 80], &_scratch_buffer[row * 80], w);
    }
    draw_sprite(w, h, xm, ym, 1);
}

static void vflip(int w, int h, int xm, int ym)
{
    int row;

    for (row = 0; row < h; row++) {
        memcpy(&_scratch_buffer[row * 80], &_sprite_buffer[(h - row - 1) * 80], w);
    }
    for (row = 0; row < h; row++) {
        memcpy(&_sprite_buffer[row * 80], &_scratch_buffer[row * 80], w);
    }
    draw_sprite(w, h, xm, ym, 1);
}

/* Transpose-style 90-degree rotation: _scratch[row][col] = _sprite[col][h-row-1]. */
static void rotate_90(int w, int h, int xm, int ym)
{
    int row, col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            _scratch_buffer[row * 80 + col] = _sprite_buffer[col * 80 + (h - row - 1)];
        }
    }
    for (row = 0; row < h; row++) {
        memcpy(&_sprite_buffer[row * 80], &_scratch_buffer[row * 80], w);
    }
    draw_sprite(w, h, xm, ym, 1);
}

/*
 * rotate_45 -- shears each successive row by one extra column to the
 * left, wrapping the vacated tail back onto the front (a crude 45-degree
 * "twist" effect rather than a true rotation), matching the raw
 * disassembly at 1ae7:0f83 exactly.
 */
static void rotate_45(int w, int h, int xm, int ym)
{
    int shift, colstart;
    int row, col;

    shift = 0;
    _sprite_w += w / 2;
    _sprite_h += h / 2;
    colstart = _sprite_w / 2 + 1;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            _scratch_buffer[(shift + col) * 80 + (colstart + col)] =
                _sprite_buffer[row * 80 + col];
        }
        if ((row & 1) == 0) {
            colstart--;
        }
        shift++;
    }
    for (row = 0; row < _sprite_w; row++) {
        memcpy(&_sprite_buffer[row * 80], &_scratch_buffer[row * 80], w);
    }
    draw_grid(xm, ym, _sprite_w, _sprite_h);
    draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
}

/*
 * move_sprite -- shifts the whole sprite by one cell in a direction,
 * wrapping the edge row/column around to the opposite side.
 */
static int move_sprite(int w, int h, int xm, int ym)
{
    unsigned int inkey, ext;
    int key;
    int i;

    control_text((char far *) "Move");

    for (;;) {
        kb_event(&inkey, &ext);
        key = toupper(inkey);

        if (key == 'E') {
            return key;
        } else if (inkey == 0x1b) {
            return 'E';
        } else if (inkey == 0xd || inkey == ' ') {
            return 'E';
        } else if (inkey == 0) {
            switch (ext) {
            case 0x4b: /* Left */
                for (i = 0; i < h; i++) {
                    _scratch_buffer[i * 80 + w - 1] = _sprite_buffer[i * 80];
                    memcpy(&_scratch_buffer[i * 80], &_sprite_buffer[i * 80 + 1], w - 1);
                }
                for (i = 0; i < h; i++) {
                    memcpy(&_sprite_buffer[i * 80], &_scratch_buffer[i * 80], w);
                }
                draw_sprite(w, h, xm, ym, 1);
                break;
            case 0x3b: /* F1 */
                help_commands();
                break;
            case 0x48: /* Up */
                memcpy(&_scratch_buffer[(h - 1) * 80], _sprite_buffer, w);
                for (i = 0; i < h - 1; i++) {
                    memcpy(&_scratch_buffer[i * 80], &_sprite_buffer[(i + 1) * 80], w);
                }
                for (i = 0; i < h; i++) {
                    memcpy(&_sprite_buffer[i * 80], &_scratch_buffer[i * 80], w);
                }
                draw_sprite(w, h, xm, ym, 1);
                break;
            case 0x4d: /* Right */
                for (i = 0; i < h; i++) {
                    _scratch_buffer[i * 80] = _sprite_buffer[i * 80 + w - 1];
                    memcpy(&_scratch_buffer[i * 80 + 1], &_sprite_buffer[i * 80], w - 1);
                }
                for (i = 0; i < h; i++) {
                    memcpy(&_sprite_buffer[i * 80], &_scratch_buffer[i * 80], w);
                }
                draw_sprite(w, h, xm, ym, 1);
                break;
            case 0x50: /* Down */
                memcpy(_scratch_buffer, &_sprite_buffer[(h - 1) * 80], w);
                for (i = 0; i < h - 1; i++) {
                    memcpy(&_scratch_buffer[(i + 1) * 80], &_sprite_buffer[i * 80], w);
                }
                for (i = 0; i < h; i++) {
                    memcpy(&_sprite_buffer[i * 80], &_scratch_buffer[i * 80], w);
                }
                draw_sprite(w, h, xm, ym, 1);
                break;
            default:
                break;
            }
        } else if (key == 'P' || key == 'R' || key == 'S' || key == 'X') {
            return key;
        }
    }
}

/*
 * size_sprite -- grows/shrinks the sprite's width/height by one cell,
 * clamped to [1, _max_w] / [1, _max_h].
 */
static int size_sprite(int xm, int ym)
{
    unsigned int inkey, ext;
    int key;

    control_text((char far *) "Size");

    for (;;) {
        kb_event(&inkey, &ext);
        key = toupper(inkey);

        if (key == 'E' || inkey == 0x1b || inkey == 0xd || inkey == ' ' ||
            key == 'M' || key == 'P' || key == 'X') {
            return 'E';
        } else if (inkey == 0) {
            switch (ext) {
            case 0x4b: /* Left: shrink height */
                if (_sprite_h > 1) {
                    _sprite_h--;
                    draw_grid(xm, ym, _sprite_w, _sprite_h);
                    draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
                }
                break;
            case 0x3b: /* F1 */
                help_commands();
                break;
            case 0x48: /* Up: shrink width */
                if (_sprite_w > 1) {
                    _sprite_w--;
                    draw_grid(xm, ym, _sprite_w, _sprite_h);
                    draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
                }
                break;
            case 0x4d: /* Right: grow height */
                if (_sprite_h < _max_h) {
                    _sprite_h++;
                    draw_grid(xm, ym, _sprite_w, _sprite_h);
                    draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
                }
                break;
            case 0x50: /* Down: grow width */
                if (_sprite_w < _max_w) {
                    _sprite_w++;
                    draw_grid(xm, ym, _sprite_w, _sprite_h);
                    draw_sprite(_sprite_w, _sprite_h, xm, ym, 1);
                }
                break;
            default:
                break;
            }
            grid_size();
        }
    }
}

/*
 * animate_sprite -- steps forward through the sprite file list at a
 * user-adjustable delay, redrawing each frame; ESC/CR/Space/F1..F6 style
 * navigation keys hand back to the caller as usual.
 */
static int animate_sprite(int xm, int ym, int sprite_index, int num_sprites, char far * far *sprite_files)
{
    int index, orig_w, orig_h, last_w, last_h;
    int anim_delay;
    unsigned int inkey, ext;

    index = sprite_index;
    anim_delay = 0x64;
    control_text((char far *) "Anim");
    memcpy(_scratch_buffer, _sprite_buffer, 4320);
    memcpy(_anim_mask_save, _mask_buffer, 4320);
    orig_w = _sprite_w;
    orig_h = _sprite_h;

    for (;;) {
        index++;
        if (index >= num_sprites) {
            index = 0;
        }
        last_w = _sprite_w;
        last_h = _sprite_h;

        if (index == sprite_index) {
            memcpy(_sprite_buffer, _scratch_buffer, 4320);
        } else {
            read_sprite(sprite_files[index]);
        }

        draw_sprite(_sprite_w, _sprite_h, xm, ym, 2);
        delay(anim_delay);

        if (read_key() != 0) {
            kb_event(&inkey, &ext);
            if (inkey == '+') {
                anim_delay = (anim_delay > 5) ? anim_delay - 5 : 0;
            } else if (inkey == '-') {
                anim_delay += 5;
            } else {
                kb_flush();
                memcpy(_sprite_buffer, _scratch_buffer, 4320);
                memcpy(_mask_buffer, _anim_mask_save, 4320);
                _sprite_w = orig_w;
                _sprite_h = orig_h;
                clear_sprite(last_w, last_h);
                draw_sprite(_sprite_w, _sprite_h, xm, ym, 0);
                return 'E';
            }
        }
    }
}

static void clear_sprite(int w, int h)
{
    trfbox(320 - (_max_w + 6), 0, w + 4, h + 4, 0);
}

/*
 * draw_sprite -- does two separate things:
 *  1. Blits _sprite_buffer 1:1 (unscaled) as a small raw preview, always
 *     at the SAME fixed screen position (320 - (_max_w+6), 0) --
 *     independent of xmargin/ymargin. status != 2 also clears/borders
 *     that preview area first; status == 2 (used by animate_sprite)
 *     skips the clear/border and just blits.
 *  2. If status == 1, also redraws the big per-cell zoomed grid via
 *     set_pixel_hlite(), which IS positioned by xmargin/ymargin.
 * Confirmed against the raw disassembly at 1ae7:1702 -- xmargin/ymargin
 * are never read for the preview blit itself, only for the zoomed grid.
 */
static void draw_sprite(int w, int h, int xmargin, int ymargin, int status)
{
    unsigned char far *base;
    int row, col;
    int x_anchor, y_anchor;

    x_anchor = 320 - (_max_w + 6);
    y_anchor = 0;
    base = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + (y_anchor + 2) * 320 + x_anchor + 2);

    if (status != 2) {
        trfbox(x_anchor, y_anchor, w + 5, h + 5, 0);
        trbox(x_anchor, y_anchor, w + 4, h + 4, 0xa);
    }

    for (row = 0; row < h; row++) {
        memcpy(base + row * 320, &_sprite_buffer[row * 80], w);
    }

    if (status == 1) {
        for (col = 0; col < w; col++) {
            for (row = 0; row < h; row++) {
                set_pixel_hlite(col * h + row, xmargin, ymargin, h,
                                 _sprite_buffer[row * 80 + col]);
            }
        }
    }
}

/*
 * draw_grid -- clears the fixed-size grid area and draws a w-by-h array
 * of ruled cells sized to fit within it, picking the larger of
 * _grid_w/w or _grid_h/h as the (square) cell size -- whichever still
 * fits both dimensions -- and storing it in _bitw/_bith for the other
 * drawing routines. Simplified from the raw disassembly's branchier
 * min/max derivation of the same value; behaviourally equivalent.
 */
static void draw_grid(int xm, int ym, int w, int h)
{
    int i;
    int cell;
    int by_w, by_h;

    trfbox(xm, ym, _grid_w + 1, _grid_h + 1, 0);

    by_w = _grid_w / w;
    by_h = _grid_h / h;
    cell = (by_w < by_h) ? by_w : by_h;
    _bitw = cell;
    _bith = cell;

    for (i = 0; i <= w; i++) {
        vline(xm + i * _bitw, ym, h * _bith + 1, 8);
    }
    for (i = 0; i <= h; i++) {
        hline(xm, ym + i * _bith, w * _bitw + 1, 8);
    }
}

/* status: 1 to highlight (colour 0xe), 0 to un-highlight (colour 8). Draws
 * an OUTLINE, not a filled box -- confirmed via the raw disassembly at
 * 1ae7:18f7, whose call target 0x1000:cfef resolves to trbox(), not
 * trfbox(). An earlier pass here wrongly used trfbox(), which painted a
 * solid block over the cell (both a solid highlight instead of an outline,
 * and a solid grey box left behind on un-highlight that clobbered the
 * actual sprite pixel colour underneath it). */
static void pixel_hlite(int status, int index, int xmargin, int ymargin, int h)
{
    int x, y;

    x = xmargin + (index / h) * _bitw;
    y = ymargin + (index % h) * _bith;
    trbox(x, y, _bitw + 1, _bith + 1, status == 1 ? 0xe : 8);
}

static void set_pixel_hlite(int index, int xmargin, int ymargin, int h, int colour)
{
    int x, y;

    x = xmargin + (index / h) * _bitw;
    y = ymargin + (index % h) * _bith;
    trfbox(x + 1, y + 1, _bitw - 1, _bith - 1, colour);
}

/* Draws the single "current colour" preview swatch at its fixed screen
 * position (always the same spot -- this is not part of the 256-entry
 * palette grid drawn by draw_palette(); it's the small indicator shown
 * by edit_sprite/edit_mask/select_colour next to the grid). */
static void draw_palette_colour(int colour)
{
    int x, y;

    x = 0xe6;
    y = _max_h + 6;
    trbox(x, y, 20, 15, 0xa);
    trfbox(x + 1, y + 1, 18, 13, colour);
}

static void control_text(char far *text_ptr)
{
    text256(0xd4, 0, (unsigned char far *) text_ptr, 0xe, 3);
}

static void grid_pos(int x, int y)
{
    char buf[10];

    sprintf(buf, "x=%2d y=%2d", x, y);
    text256(0x46, 0, (unsigned char far *) buf, 7, 0);
}

static void grid_size(void)
{
    char buf[10];

    sprintf(buf, "w=%2d h=%2d", _sprite_w, _sprite_h);
    text256(0x7d, 0, (unsigned char far *) buf, 7, 0);
}

static void draw_file_name(char far *text_ptr)
{
    text256(0, 0, (unsigned char far *) "            ", 0, 0);
    text256(0, 0, (unsigned char far *) text_ptr, 0xe, 2);
}

/*
 * draw_palette -- draws the full 256-colour selection grid as an 8-column
 * by 32-row array of 5x3 pixel swatches (i / 32 = column, i % 32 = row),
 * anchored at (262, _max_h + 8) with a 7x4-pixel cell pitch (5+2 wide,
 * 3+1 tall) and a border box around the whole grid. Recovered from the
 * raw disassembly at 1ae7:1aff -- this is NOT a loop over
 * draw_palette_colour() (that function draws an unrelated, fixed-position
 * single swatch; conflating the two was an earlier mistake in this file).
 */
static void draw_palette(void)
{
    int i;
    int col, row;
    int x0, y0;

    x0 = 320 - ((5 + 2) * 8) - 2;   /* 262 */
    y0 = _max_h + 8;

    trbox(x0 - 2, y0 - 2, (5 + 2) * 8 + 2, (3 + 1) * 32 + 3, 0xb);

    for (i = 0; i < 0x100; i++) {
        col = i / 0x20;
        row = i % 0x20;
        trfbox(x0 + col * 7, y0 + row * 4, 5, 3, i);
    }
}

/*
 * select_colour -- an 8x32 palette grid; moves a highlight box around
 * with the arrow keys and returns the selected index in _colour_index.
 */
static int select_colour(void)
{
    unsigned int inkey, ext;
    int key;
    int index;

    control_text((char far *) "Pal ");
    draw_palette_colour((int) _colour_index);
    index = _colour_index;
    hlite(1, index, 0x106, _max_h + 8, 5, 3, 2, 1);

    for (;;) {
        kb_event(&inkey, &ext);
        key = toupper(inkey);

        if (key == 'E' || inkey == 0x1b || inkey == 0xd || inkey == ' ') {
            _colour_index = index;
            hlite(0, index, 0x106, _max_h + 8, 5, 3, 2, 1);
            return 'E';
        } else if (inkey == 0) {
            switch (ext) {
            case 0x4b: /* Left: column-- (index -= 32) */
                if (index / 0x20 > 0) {
                    hlite(0, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                    index -= 0x20;
                    hlite(1, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                }
                break;
            case 0x3b: /* F1 */
                help_commands();
                break;
            case 0x48: /* Up: row-- (index -= 1) */
                if (index % 0x20 > 0) {
                    hlite(0, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                    index--;
                    hlite(1, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                }
                break;
            case 0x4d: /* Right: column++ (index += 32) */
                if (index / 0x20 < 7) {
                    hlite(0, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                    index += 0x20;
                    hlite(1, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                }
                break;
            case 0x50: /* Down: row++ (index += 1) */
                if (index % 0x20 < 0x1f) {
                    hlite(0, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                    index++;
                    hlite(1, index, 0x106, _max_h + 8, 5, 3, 2, 1);
                }
                break;
            default:
                break;
            }
        }
    }
}

/*
 * help_commands -- a two-screen keyboard-shortcut popup, dismissed by any
 * keypress. Key/description text recovered verbatim from the static
 * template arrays the original code memcpy's onto the stack (340e:1136,
 * 1172, 11ae, 11d6, 11fe) -- reproduced here as local initialized arrays
 * instead of runtime-copied templates (functionally identical).
 */
static void help_commands(void)
{
    static char far *keys1[15] = {
        "F1", "F2", "    G", "F3,F4", "F5,F6", "", "E", "    Space ",
        "    C", "    U", "    H", "    V", "    9", "    +", "    -"
    };
    static char far *desc1[15] = {
        "    Commands", "    Mask", "          Generate", "          Set Clipboard",
        "          Get Clipboard", "", "    Edit", "          Set Pixel",
        "          Clear", "          Undo", "          Flip Horiz",
        "          Flip Vert", "          Rotate 90", "          Next Sprite",
        "          Prev Sprite"
    };
    static char far *keys2[10] = {
        "A", "    +", "    -", "    Space", "", "M", "P", "R", "S", "X"
    };
    static char far *desc2[10] = {
        "    Animate", "          Faster", "          Slower", "          Stop",
        "", "    Move", "    Palette", "    Resize", "    Save", "    Exit"
    };
    unsigned char far *saved;
    int w, h, xm, ym;
    unsigned int inkey, ext;
    int i;

    w = 0x118;
    h = 0x9a;
    xm = (320 - w) / 2;
    ym = (200 - h) / 2;

    saved = (unsigned char far *) malloc((unsigned long) w * h);
    if (saved == NULL) {
        return;
    }
    for (i = 0; i < h; i++) {
        memcpy(saved + i * w,
               (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + (ym + i) * 320 + xm),
               w);
    }

    trbox(xm, ym, w, h, 1);
    trfbox(xm + 4, ym + 4, w - 8, h - 8, 0xe);
    xm += 10;
    ym += 10;

    for (i = 0; i < 0xf; i++) {
        text256(xm + 10, ym + i * 9, (unsigned char far *) desc1[i], 0xb, 1);
        text256(xm + 10, ym + i * 9, (unsigned char far *) keys1[i], 0xe, 1);
    }
    ym += 0x96;
    for (i = 0; i < 0xa; i++) {
        text256(xm + 10, ym + i * 9, (unsigned char far *) desc2[i], 0xb, 1);
        text256(xm + 10, ym + i * 9, (unsigned char far *) keys2[i], 0xe, 1);
    }

    {
        char prompt[] = " Press Space Bar ";
        text256((320 - (int) strlen(prompt) * 5) / 2, ym + h - 0xee, (unsigned char far *) prompt, 0xe, 2);
    }

    kb_event(&inkey, &ext);

    for (i = 0; i < h; i++) {
        memcpy((unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + (ym + i) * 320 + xm),
               saved + i * w, w);
    }
    free(saved);
}

/* status: 1 to highlight (colour 0xe), 0 to un-highlight (colour 8). */
/* Draws an outline (not filled) around palette cell `index`, which is
 * laid out column-major: index / 0x20 selects the column, index % 0x20
 * the row within it -- matching draw_palette()'s own cell placement. */
static void hlite(int status, int index, int xmargin, int ymargin, int w, int h, int xsep, int ysep)
{
    int x, y;

    x = xmargin + (index / 0x20) * (w + xsep);
    y = ymargin + (index % 0x20) * (h + ysep);
    trbox(x - 1, y - 1, w + 2, h + 2, status == 1 ? 0xe : 0);
}

static void trbox(int x, int y, int w, int h, int colour)
{
    hline(x, y, w, colour);
    hline(x, y + h - 1, w, colour);
    vline(x, y, h, colour);
    vline(x + w - 1, y, h, colour);
}

static void trfbox(int x, int y, int w, int h, int colour)
{
    int row;

    for (row = 0; row < h; row++) {
        hline(x, y + row, w, colour);
    }
}

static void hline(int x, int y, int length, int colour)
{
    unsigned char far *dest;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);
    memset(dest, colour, length);
}

static void vline(int x, int y, int length, int colour)
{
    unsigned char far *dest;
    int i;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);
    for (i = 0; i < length; i++) {
        *dest = (unsigned char) colour;
        dest += 320;
    }
}

static void set_pixel(int x, int y, int colour)
{
    unsigned char far *dest;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);
    *dest = (unsigned char) colour;
}

/*
 * read_sprite -- loads a *.SPT file: 1-byte width, 1-byte height, then
 * height rows of width bytes of pixel data, then (if present -- older
 * files may not have it) height rows of width bytes of mask data. A
 * missing/short mask block defaults the whole mask to 0.
 */
static void read_sprite(char far *file_name)
{
    char local_name[128];
    int handle, i, n;

    strcpy(local_name, file_name);
    handle = open(local_name, O_BINARY | O_RDWR, 0x180);
    if (handle == -1) {
        return;
    }

    read(handle, (char far *) &_sprite_w, 1);
    read(handle, (char far *) &_sprite_h, 1);

    for (i = 0; i < _sprite_h; i++) {
        n = read(handle, &_sprite_buffer[i * 80], _sprite_w);
        if (n == 0) {
            break;
        }
        if (n == -1) {
            close(handle);
            exit(2);
        }
    }

    for (i = 0; i < _sprite_h; i++) {
        n = read(handle, &_mask_buffer[i * 80], _sprite_w);
        if (n == 0 || n == -1) {
            memset(_mask_buffer, 0, 0x10e0);
            break;
        }
    }

    close(handle);
}

/* Companion to read_sprite(); writes width/height/pixels/mask in the same
 * layout. Returns 0 on success, -1 on any I/O failure. */
static int write_sprite(char far *file_name)
{
    char local_name[128];
    int handle, i, n;

    strcpy(local_name, file_name);
    handle = open(local_name, O_BINARY | O_CREAT | O_RDWR, 0x180);
    if (handle == -1) {
        return -1;
    }

    write(handle, (char far *) &_sprite_w, 1);
    write(handle, (char far *) &_sprite_h, 1);

    for (i = 0; i < _sprite_h; i++) {
        n = write(handle, &_sprite_buffer[i * 80], _sprite_w);
        if (n == 0 || n == -1) {
            close(handle);
            return -1;
        }
    }

    for (i = 0; i < _sprite_h; i++) {
        n = write(handle, &_mask_buffer[i * 80], _sprite_w);
        if (n == 0 || n == -1) {
            close(handle);
            return -1;
        }
    }

    close(handle);
    return 0;
}

/*
 * text256 -- 1-bit-per-pixel glyph blitter used throughout this module
 * for on-screen labels (this module's own static copy, structurally
 * matching GRAPH256.C's text256() but kept separate/static so this file
 * does not need to #include GRAPH256.H and clash over the shared font
 * globals).
 */
static void text256(int x, int y, unsigned char far *text_ptr, int fore, int back)
{
    unsigned char far *dest;
    unsigned char far *glyph;
    unsigned char row_bits;
    int row, col;
    unsigned char ch;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);

    while (*text_ptr != 0) {
        ch = *text_ptr;
        glyph = __text_table_addr[0] + (unsigned int) ch * __mva_text_height;

        for (row = 0; row < __mva_text_height; row++) {
            row_bits = glyph[row];
            for (col = 0; col < __mva_text_width; col++) {
                if (row_bits & (0x80 >> col)) {
                    dest[row * 320 + col] = (unsigned char) fore;
                } else if (back != -1) {
                    dest[row * 320 + col] = (unsigned char) back;
                }
            }
        }

        dest += __mva_text_width;
        text_ptr++;
    }
}

/*
 * copy_sprite_to_gen -- loads a SPRITE_STRUCT's compact per-row bitmap
 * into _sprite_buffer, setting _sprite_w/_sprite_h from the struct.
 */
void copy_sprite_to_gen(SPRITE_STRUCT far *sprite)
{
    int i;
    unsigned char far *src;

    _sprite_w = sprite->spritew;
    _sprite_h = sprite->spriteh;
    src = sprite->sprite;
    for (i = 0; i < _sprite_h; i++) {
        memcpy(&_sprite_buffer[i * 80], src, _sprite_w);
        src += _sprite_w;
    }
}

/* Inverse of copy_sprite_to_gen: writes _sprite_buffer back out into the
 * sprite's compact per-row bitmap (dimensions taken from the current
 * globals, not re-read from the struct). */
void copy_gen_to_sprite(SPRITE_STRUCT far *sprite)
{
    int i;
    unsigned char far *dst;

    dst = sprite->sprite;
    for (i = 0; i < _sprite_h; i++) {
        memcpy(dst, &_sprite_buffer[i * 80], _sprite_w);
        dst += _sprite_w;
    }
}

/*
 * copy_sprite_range_to_gen -- used by MAZEEDIT.C's edit_block_range() to
 * place one sprite's bitmap into a (row_offset, col_offset) cell of a
 * larger multi-block grid inside _sprite_buffer, growing _sprite_w/
 * _sprite_h to the resulting bounding box.
 */
void copy_sprite_range_to_gen(int row_offset, int col_offset, SPRITE_STRUCT far *sprite)
{
    int i, w, h;
    unsigned char far *src;

    w = sprite->spritew;
    h = sprite->spriteh;
    _sprite_w = w;
    _sprite_h = h;
    src = sprite->sprite;

    for (i = 0; i < h; i++) {
        memcpy(&_sprite_buffer[(row_offset * h + i) * 80 + col_offset * w], src, w);
        src += w;
    }

    _sprite_w = sprite->spritew * (col_offset + 1);
    _sprite_h = (row_offset + 1) * sprite->spriteh;
}

/* Inverse of copy_sprite_range_to_gen: pulls one (row_offset, col_offset)
 * cell back out of _sprite_buffer into the sprite's own bitmap. */
void copy_gen_to_sprite_range(int row_offset, int col_offset, SPRITE_STRUCT far *sprite)
{
    int i;
    unsigned char far *dst;

    _sprite_w = sprite->spritew;
    _sprite_h = sprite->spriteh;
    dst = sprite->sprite;

    for (i = 0; i < _sprite_h; i++) {
        memcpy(dst, &_sprite_buffer[(row_offset * _sprite_h + i) * 80 + col_offset * _sprite_w], _sprite_w);
        dst += _sprite_w;
    }
}

/*
 * copy_gen_to_mirror_sprite -- horizontally flips _sprite_buffer and
 * writes the result into a second SPRITE_STRUCT; used by MANEDIT.C's
 * edit_pacman() to derive a pacman's mirrored (facing-the-other-way)
 * walk sprite from the one just edited.
 */
void copy_gen_to_mirror_sprite(SPRITE_STRUCT far *sprite)
{
    int i, j;
    unsigned char far *dst;

    for (i = 0; i < _sprite_h; i++) {
        for (j = 0; j < _sprite_w; j++) {
            _scratch_buffer[i * 80 + j] = _sprite_buffer[i * 80 + (_sprite_w - j - 1)];
        }
    }

    dst = sprite->sprite;
    for (i = 0; i < _sprite_h; i++) {
        memcpy(dst, &_scratch_buffer[i * 80], _sprite_w);
        dst += _sprite_w;
    }
}
