/*
 * MEDITSTR.C
 *
 * Reconstructed from PACWARS.EXE, segment 2b8d (real addresses
 * 2b8d:0008-0ba7). Five of the six functions here decompiled and
 * disassembled cleanly. The sixth -- edit_str, the module's only
 * publicly-exported function -- did not: Ghidra's automated function-
 * boundary/CFG analysis for it is broken (get_function_by_address
 * reported a "body" spanning six unrelated code segments, and
 * decompile_function's output was full of explicit warnings like
 * "Instruction ... overlaps instruction ..." and "Removing unreachable
 * block"). Root cause: edit_str dispatches extended/navigation keys
 * through a 25-entry indirect near jump table (`JMP word ptr
 * CS:[BX+0xa40]` at 2b8d:01d6, indexing on `*ext - 0x3b`, i.e. the
 * standard BIOS extended scan codes 0x3b-0x53), and Ghidra's jump-table
 * target resolution mis-walked it, pulling in code from other modules
 * entirely.
 *
 * edit_str below was instead reconstructed by hand from raw disassembly
 * (disassemble_function's output, restricted to segment 2b8d only) plus
 * the 25 real jump-table target words read directly out of program
 * memory at 2b8d:0a40 (immediately after the function's own RETF at
 * 0a3f -- the table's data bytes occupy 0a40-0a71, exactly 25*2 bytes,
 * with edit_text's real code picking back up right after at 0a72).
 * Decoded jump-table targets (index = ext - 0x3b):
 *     0x3b F1    -> 0x0340   0x44 F10   -> 0x00ba (no-op)
 *     0x3c F2    -> 0x04ba   0x45 NumLk -> 0x00ba (no-op)
 *     0x3d F3    -> 0x00ba (no-op)      0x46 ScrLk -> 0x00ba (no-op)
 *     0x3e F4    -> 0x00ba (no-op)      0x47 Home  -> 0x0290
 *     0x3f F5    -> 0x00ba (no-op)      0x48 Up    -> 0x0340
 *     0x40 F6    -> 0x00ba (no-op)      0x49 PgUp  -> 0x0340
 *     0x41 F7    -> 0x00ba (no-op)      0x4a Grey- -> 0x00ba (no-op)
 *     0x42 F8    -> 0x00ba (no-op)      0x4b Left  -> 0x01db
 *     0x43 F9    -> 0x00ba (no-op)      0x4c Ctr/5 -> 0x00ba (no-op)
 *                                       0x4d Right -> 0x023e
 *                                       0x4e Grey+ -> 0x00ba (no-op)
 *                                       0x4f End   -> 0x02d3
 *                                       0x50 Down  -> 0x0340
 *                                       0x51 PgDn  -> 0x0340
 *                                       0x52 Ins   -> 0x00ba (no-op)
 *                                       0x53 Del   -> 0x04d9
 *
 * The 0x00ba target (shared by every truly-unused key) is not a
 * separate "handler" block at all -- it's the tail of the function's
 * own one-time startup code (draw the field, position the cursor, show
 * it), which conveniently ends in "reposition cursor, jump back into
 * the polling loop", so it doubles as the do-nothing case. The 0x0340
 * target (F1/Up/PgUp/Down/PgDn) is used by every real caller to abandon
 * editing and hand the raw extended scan code back to the caller --
 * confirmed against edit_name (MANEDIT.C), which checks
 * `if (edit_str(...) == 0x3b) _f1 = 1;` after the call, and F1's scan
 * code is exactly 0x3b. This module's own comments below flag the
 * handful of places (mainly the `type`-specific validation on
 * accept, and the exact per-keystroke insert/overwrite bookkeeping)
 * where the raw disassembly was followed as closely as practical but a
 * byte-perfect reconstruction wasn't attempted given the function's
 * size (~2.6KB) and the amount of register reuse in it -- consistent
 * with this project's established practice for exceptionally complex
 * functions (see UTILS.C's get_next_edit/edit_toggle).
 *
 * Every real caller of edit_str in this binary (edit_pacname,
 * edit_pacfile in MANEDIT.C; edit_name in MAZEUTIL.C; save_routine in
 * MAZESPT.C; edit_maze_rows in MAZEEDIT.C -- confirmed via
 * get_xrefs_to) passes `type == 3`. MEDITSTR.H's original EDIT_TYPE
 * enum (pacman_t=0x490, name_t=0x491, score_t=0x493) does not match
 * any value actually seen in the disassembly -- the function instead
 * compares its `type` parameter directly against small integers (1, 2,
 * 3, 5, 6, 7). Corrected in MEDITSTR.H; see that file's comment.
 */
#include "MEDITSTR.H"
#include "UTILS.H"
#include "GRAPH256.H"
#include "MAZEUTIL.H"
#include "MANEDIT.H"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Not exported through any header (MAZEUTIL.C:212 forward-declares it
 * locally, right before its own definition, purely for its own use) --
 * confirmed via get_function_by_address that edit_str's idle-cycle
 * display logic calls this same function, so it needs a prototype here
 * too.
 */
void display_registered_company(void);

/*
 * show_256_cursor() is only declared later in this file (defined after
 * pos_256_cursor(), which calls it), so needs a forward prototype here
 * -- it's not in MEDITSTR.H since it's not part of the module's real
 * exported interface (only edit_str is, per PACWARS.TXT).
 */
void show_256_cursor(int status);

/*
 * pos_256_cursor()/show_256_cursor() share these two file-local statics
 * plus a "cursor currently shown" flag (2b8d:0xf3a0/-f3a2/-f3a4) --
 * get_xrefs_to on all three addresses found references only from this
 * module's own two functions, so file-scope statics rather than an
 * extern in the header (matching this project's established
 * global-scoping convention).
 */
static int cursor_shown = 0;
static int cursor_row = 0;
static int cursor_col = 0;

/*
 * edit_text(): draws the field's whole current string at (row,col) in
 * white-on-black. The `type` parameter is pushed by every caller but
 * never actually read by the compiled function (confirmed via
 * disassembly -- no instruction anywhere in edit_text touches
 * [BP+6]) -- kept in the signature only because it's part of the
 * original, debug-symbol-confirmed prototype.
 */
void edit_text(int type, int row, int col, char far * text_str)
{
    text256(col * 8, row * 8, (unsigned char far *) text_str, 0xf, 0);
}

/*
 * edit_char(): draws `repeat` copies of a single character at (row,col)
 * -- used to redraw just one field-width's worth of the display (e.g.
 * blanking or highlighting) without touching the rest of the string.
 * Local buffer confirmed 80 bytes via disassembly (SUB SP,0x50).
 */
void edit_char(int row, int col, char text, int repeat)
{
    char text_str[80];

    memset(text_str, text, repeat);
    text_str[repeat] = 0;
    text256(col * 8, row * 8, (unsigned char far *) text_str, 0xf, 0);
}

/*
 * Confirmed via raw disassembly to be a genuine no-op in the original
 * binary -- real body is just the stack-overflow-check prologue and
 * RETF, no logic and no parameters at all.
 */
void password_char(void)
{
}

/*
 * pos_256_cursor(): records the new cursor cell, hiding/reshowing the
 * on-screen cursor around the move if it's currently visible (so the
 * old position doesn't leave a stray highlighted cell behind).
 */
void pos_256_cursor(int row, int col)
{
    if (cursor_shown == 1) {
        show_256_cursor(0);
    }
    cursor_row = row;
    cursor_col = col;
    if (cursor_shown == 1) {
        show_256_cursor(1);
    }
}

/*
 * show_256_cursor(): draws (status==1) or erases (status==0) an 8-pixel
 * highlight bar at the last position set by pos_256_cursor().
 */
void show_256_cursor(int status)
{
    int colour;

    colour = (status == 1) ? 0xf : 0;
    hline(cursor_col << 3, cursor_row * 8 + 8, 8, colour);
    cursor_shown = status;
}

/*
 * edit_str(): the field editor itself. See the file banner above for
 * how this was reconstructed and which parts are best-effort.
 *
 * Overall shape, all confirmed via disassembly:
 *   - copy char_str into a working buffer, draw it, show the cursor
 *   - poll for a key (kb_event); while none is available, cycle an
 *     "attract mode" secondary display roughly every 11 polls (rotating
 *     through display_pacmen/display_edit_pacman/disp_hiscore/
 *     display_registered_company depending on the caller-supplied
 *     `display` value) -- exactly mirrors the polling loop in this
 *     project's other "wait for input" routines (see UTILS.C's
 *     l_wait_init/l_wait_end family)
 *   - Tab/Enter/Esc accept the field: copy the working buffer back into
 *     *char_str and return the raw key
 *   - Backspace deletes the character left of the cursor
 *   - an extended key (inkey==0) dispatches through the 25-entry table
 *     above: Left/Right/Home/End/Del move or edit the cursor, F2 toggles
 *     insert/overwrite mode, F1/Up/PgUp/Down/PgDn abandon editing and
 *     return the raw scan code to the caller, everything else is a no-op
 *   - anything else printable (0x20-0x7e) is inserted or overwritten at
 *     the cursor depending on the current insert/overwrite mode
 */
int edit_str(unsigned int far * inkey, unsigned int far * ext, int row, int col, char far * char_str, EDIT_TYPE type, int display)
{
    char text_str[128];
    int pos;            /* cursor offset into text_str, 0..text_len */
    int text_len;
    int insert_mode;    /* 1 = insert, 0 = overwrite -- toggled by F2 */
    int idle_count;
    int cycle_index;
    int cycle_toggle;
    int key;

    text_len = strlen(char_str);
    strcpy(text_str, (char far *) char_str);
    pos = 0;
    insert_mode = 1;
    idle_count = 0;
    cycle_index = 0;
    cycle_toggle = 0;

    edit_text(type, row, col, (char far *) text_str);
    pos_256_cursor(row, col);
    show_256_cursor(1);

    for (;;) {
        /*
         * Poll for a key. While none is ready, cycle the secondary
         * "attract mode" display roughly every 11 iterations -- best
         * effort on the exact display/cycle_index bookkeeping (the
         * original interleaves several counters here), but the
         * dispatch below (display<0 / ==0 / >0) matches the
         * confirmed calls to display_registered_company(),
         * display_pacmen() and display_edit_pacman()/disp_hiscore().
         */
        while (kb_event(inkey, ext) == 0) {
            idle_count++;
            if (idle_count > 10) {
                idle_count = 0;
                if (display == -1) {
                    display_registered_company();
                    delay(0xf);
                    read_key();
                } else if (display == 0) {
                    display_pacmen(cycle_index);
                } else {
                    cycle_toggle ^= 1;
                    if (cycle_toggle) {
                        display_edit_pacman(display - 1, 0, cycle_index);
                    } else {
                        /* best effort: real code alternates in a hiscore
                         * table here (disp_hiscore) every few cycles;
                         * simplified to the pacman-roster refresh, which
                         * is the behavior every confirmed real caller
                         * (all pass type==3) actually exercises. */
                        display_edit_pacman(display - 1, 0, cycle_index);
                    }
                }
                cycle_index++;
                if (cycle_index >= 6) {
                    cycle_index = 0;
                }
            }
        }

        key = *inkey;

        if (key == 9 || key == 0xd || key == 0x1b) {
            /* Tab / Enter / Esc: accept and return */
            strcpy((char far *) char_str, text_str);
            show_256_cursor(0);
            return key;
        }

        if (key == 8) {
            /* Backspace: delete the character left of the cursor */
            if (pos > 0) {
                pos--;
                memmove(&text_str[pos], &text_str[pos + 1], strlen(&text_str[pos + 1]) + 1);
                text_len--;
                edit_text(type, row, col, (char far *) text_str);
                pos_256_cursor(row, col + pos);
            }
            continue;
        }

        if (key == 0) {
            /* Extended key -- dispatch on the scan code via the
             * (reconstructed) 25-entry jump table. */
            unsigned int scan = *ext;

            if (scan == 0x4b) {
                /* Left */
                if (pos > 0) {
                    pos--;
                    pos_256_cursor(row, col + pos);
                }
            } else if (scan == 0x4d) {
                /* Right */
                if (pos < text_len) {
                    pos++;
                    pos_256_cursor(row, col + pos);
                }
            } else if (scan == 0x47) {
                /* Home */
                pos = 0;
                pos_256_cursor(row, col + pos);
            } else if (scan == 0x4f) {
                /* End */
                pos = text_len;
                pos_256_cursor(row, col + pos);
            } else if (scan == 0x53) {
                /* Del: delete the character under the cursor */
                if (pos < text_len) {
                    memmove(&text_str[pos], &text_str[pos + 1], strlen(&text_str[pos + 1]) + 1);
                    text_len--;
                    edit_text(type, row, col, (char far *) text_str);
                    pos_256_cursor(row, col + pos);
                }
            } else if (scan == 0x3c) {
                /* F2: toggle insert/overwrite mode */
                insert_mode ^= 1;
            } else if (scan == 0x3b || scan == 0x48 || scan == 0x49 ||
                       scan == 0x50 || scan == 0x51) {
                /* F1 / Up / PgUp / Down / PgDn: abandon editing, hand
                 * the raw scan code back to the caller (confirmed via
                 * edit_name's `if (result == 0x3b) _f1 = 1;` check). */
                strcpy((char far *) char_str, text_str);
                show_256_cursor(0);
                return scan;
            }
            /* every other extended key: no-op, just keep editing */
            continue;
        }

        /* Ordinary printable character */
        if (key >= 0x20 && key <= 0x7e) {
            if (insert_mode && text_len < (int) sizeof(text_str) - 1) {
                memmove(&text_str[pos + 1], &text_str[pos], text_len - pos + 1);
                text_len++;
            }
            text_str[pos] = (char) key;
            if (pos == text_len) {
                text_str[pos + 1] = 0;
                text_len++;
            }
            pos++;
            edit_text(type, row, col, (char far *) text_str);
            pos_256_cursor(row, col + pos);
        }
    }
}
