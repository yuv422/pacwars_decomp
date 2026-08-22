/*
 * MANEDIT.C
 *
 * Reconstructed from PACWARS.EXE, segment 1d69 (real addresses
 * 1d69:0005-1f6c). This is the "Pacman roster editor" UI: a screen that
 * lets the player pick one of the 10 playable characters, edit its name
 * and per-frame sprites (walk x2, death x6, bullet), tweak its death-
 * animation delay, and load/save/browse .PAC character files on disk.
 *
 * All 27 functions here decompiled cleanly (no Ghidra CFG/jump-table
 * bugs like MEDITSTR.C's edit_str) -- the only real ambiguity was three
 * call targets whose arguments the decompiler rendered as
 * `CONCAT22(unaff_SS, X)`/`CONCAT22(unaff_DI, X)` register noise; all
 * three were resolved by cross-checking the raw disassembly's actual
 * PUSH sequence against get_function_by_address on the call target
 * (see edit_pacman()'s comments for the one that needed real
 * interpretation: the F1/Home/PgUp-style "abandon and hand back the raw
 * key" pattern doesn't apply here, but the same "trust the pushes, not
 * the decompiler's register guess" principle does).
 *
 * Two globals needed correcting from what Ghidra's own type database
 * guessed:
 *   - _pacname (PACWARS.EXE's per-character name table) turned out to
 *     be used here too, not just in MAZESPT.C where it's defined --
 *     promoted from `static` to shared/extern there (see MAZESPT.H).
 *   - _name_buff was typed by Ghidra as an array of 130 far pointers
 *     (`char *32[130]`), but the actual compiled access pattern
 *     (`(char**)_name_buff + i*0xd`, i.e. a 13-byte stride matching
 *     _pacname's own per-name stride) shows it's really a flat
 *     `char[10][13]` scratch copy of the name table, not a pointer
 *     array. Corrected below; the sibling _man_buff/_death_buff/
 *     _bull_buff arrays *are* genuinely far-pointer arrays (Ghidra's
 *     typing checks out for those -- confirmed via their 4-byte-stride
 *     read/write pattern) and are declared as such.
 */
#include "MANEDIT.H"
#include "MAZESPT.H"
#include "MAZEUTIL.H"
#include "MEDITSTR.H"
#include "GRAPH256.H"
#include "MVAGRAPH.H"
#include "MAZE.H"
#include "SPRITGEN.H"
#include "UTILS.H"
#include <string.h>
#include <stdlib.h>
#include <alloc.h>
#include <dir.h>

/*
 * Full 200-entry sorted directory listing of *.PAC files, built by
 * load_all_files() and browsed by list_pacmen(). Confirmed file-local
 * (get_xrefs_to found only load_all_files referencing it directly; the
 * rest of this module reaches individual entries through the `file_name`
 * parameters those two functions pass around).
 */
static char _pac_files[200][13];

/*
 * Scratch copy of _pacname (see file banner) used by buffer_pacmen()/
 * restore_pacmen() while the character-select screen temporarily shows
 * simplified icons in place of each character's real sprites.
 */
static char _name_buff[10][13];

/*
 * Backup slots for each character's sprite bitmap pointers (walk x4,
 * death x12, bullet x2 -- indices match get_sprite_data()'s layout),
 * used the same way as _name_buff: buffer_pacmen() stashes the live
 * _sprites[].sprite pointers here and NULLs them out; restore_pacmen()
 * frees whatever got (re)allocated in the meantime and puts the
 * originals back.
 */
static unsigned char far * _man_buff[10][4];
static unsigned char far * _death_buff[10][12];
static unsigned char far * _bull_buff[10][2];

/*
 * Forward declarations -- every function below is called from at least
 * one other function defined earlier in this file, so BCC needs real
 * prototypes up front (its implicit-int-return assumption for
 * undeclared calls would otherwise conflict with these functions' real
 * `void`/static-ness once it reaches their definitions).
 */
static void edit_screen1(void);
static void display_save(int status);
static void display_filestatus(int status, int type);
void edit_screen(void);
void display_anim_delay(int delay_val);
void display_file_control(int edit_horiz);
void display_pacname(char far * name_str);
void display_pacfile(char far * name_str);
void hilite_edit(int status, int edit_pos, int edit_horiz);
void get_edit_pos(int edit_pos, int edit_horiz, int far * xpos, int far * ypos);
void edit_pacname(int curr_pacman, char far * name_str, unsigned int far * inkey, unsigned int far * ext);
void edit_pacfile(int curr_pacman, char far * name_str, unsigned int far * inkey, unsigned int far * ext);
int get_mirror_sprite_data(int curr_pacman, int edit_pos, int edit_horiz);
void scroll_pacmen(int dir);
void display_list_pacmen(int offset, int file_offset);
void list_message(int status);
int load_all_files(void);
void fill_list(int curr_pacman, int spos, int num);

/*
 * get_edit_pos()/hilite_edit()'s per-edit-slot layout tables (edit_pos
 * 0=name, 1=walk sprite, 2=death sprite, 3=bullet sprite, 4=anim delay,
 * 5=filename, 6=file List/Load/Save buttons). Recovered byte-for-byte
 * via read_memory at 340e:14a4/14b2/14c0/14ce (the four local arrays
 * get_edit_pos/hilite_edit initialize via the compiler's F_SCOPY_
 * constant-array-initializer helper on every call in the original
 * binary; hoisted to file scope here since they're compile-time
 * constants).
 */
static const int edit_w[7]    = { 0x50, 0x10, 0x10, 0x10, 0x6a, 0x60, 0x36 };
static const int edit_h[7]    = { 0x0e, 0x14, 0x1c, 0x0e, 0x16, 0x0e, 0x16 };
static const int edit_xoff[7] = { 0, 0, 0, 0, 0, 0x58, 0x20 };
static const int edit_yoff[7] = { 0, 0, 0, 0, 0, 0x10, 0x08 };

/*
 * choose_edit_pacman(): the character-select strip shown before
 * entering the full editor -- Left/Right/Up/Down/Home/End move the
 * highlight among the 10 characters plus an 11th "Save" button (index
 * 0xb); Enter on a character returns curr_pacman+1, Enter on Save
 * writes every character to disk and stays in the loop, Esc aborts.
 */
int choose_edit_pacman(int curr_pacman)
{
    int offset = 0;
    int iter = 0;
    unsigned int inkey = 0;
    unsigned int ext = 0;
    int sel;
    int temp_pacman = 0;

    kb_flush();
    set_key_vect(1, key_pause);
    edit_screen1();
    display_pacmen(0);
    display_save(-1);
    hilite_pacman(1, curr_pacman);

    do {
        iter++;
        if (iter > 10) {
            display_pacmen(offset);
            offset ^= 1;
            iter = 0;
        }

        if (read_key() == 1) {
            kb_event(&inkey, &ext);
            sel = curr_pacman;

            if (inkey == 0 && ext == 0x4d && curr_pacman != 0xb) {
                hilite_pacman(0, curr_pacman);
                sel = 0xb;
                display_save(1);
                temp_pacman = curr_pacman;
            }
            if (inkey == 0 && ext == 0x4b && sel == 0xb) {
                display_save(0);
                hilite_pacman(1, temp_pacman);
                sel = temp_pacman;
            }
            if (inkey == 0 && ext == 0x48 && sel > 0 && sel < 0xb) {
                hilite_pacman(0, sel);
                sel--;
                hilite_pacman(1, sel);
            }
            if (inkey == 0 && ext == 0x50 && sel < 9) {
                hilite_pacman(0, sel);
                sel++;
                hilite_pacman(1, sel);
            }
            if (inkey == 0 && ext == 0x47 && sel > 0) {
                hilite_pacman(0, sel);
                sel = 0;
                hilite_pacman(1, 0);
            }
            if (inkey == 0 && ext == 0x4f && sel < 9) {
                hilite_pacman(0, sel);
                sel = 9;
                hilite_pacman(1, 9);
            }

            curr_pacman = sel;

            if (inkey == 0xd && sel == 0xb) {
                display_filestatus(1, 1);
                save_all_sprites();
                display_filestatus(0, 1);
                display_save(0);
                hilite_pacman(1, temp_pacman);
                inkey = 0;
                curr_pacman = temp_pacman;
            }
        }

        delay(0xf);
    } while (_esc == 0 && inkey != 0x1b && (inkey != 0xd || curr_pacman == 0xb));

    if (inkey == 0x1b) {
        _esc = 1;
    }
    set_key_vect(0, 0);
    return curr_pacman + 1;
}

/*
 * edit_pacman(): the main per-character editor screen. edit_pos steps
 * through 7 fields (0=name, 1=walk sprite, 2=death sprite, 3=bullet
 * sprite, 4=anim delay, 5=filename, 6=file buttons); edit_horiz[] holds
 * each field's own sub-selection (which of the N frames/buttons within
 * that field). Enter on fields 1-3 launches the pixel-level sprite
 * editor (sprite_gen()) on the selected frame and its mirrored twin;
 * Enter on field 6 runs List/Load/Save depending on edit_horiz[6].
 *
 * The `sprite_gen(1, &sprite_file_ptr)` call below passes the address of
 * a single-element far-pointer "array" (num_sprites==1), matching
 * sprite_gen's declared `(int num_sprites, char far * far * sprite_files)`
 * signature. An earlier pass (written before SPRITGEN.C itself was
 * decompiled) guessed this slot was left deliberately unused/dangling,
 * reasoning that this call edits a sprite already resident in the "gen"
 * buffer rather than loading one from disk. That guess was wrong: now
 * that sprite_gen() is decompiled, it's confirmed to unconditionally
 * call draw_file_name(sprite_files[0]) on entry (and to reload from that
 * same path on its 'C' key), so sprite_file_ptr must point at this
 * pacman's current file_name buffer, which is set immediately before
 * each call.
 */
int edit_pacman(int curr_pacman)
{
    char file_name[13];
    int edit_horiz[7];
    int edit_pos = 0;
    int offset = 0;
    int iter = 0;
    int anim_delay = 4;
    unsigned int inkey = 0;
    unsigned int ext = 0;
    int i, idx, idx2;
    int old_font;
    int milliseconds;
    char far * sprite_file_ptr = 0;

    strcpy(file_name, "NONAME.PAC");
    for (i = 0; i < 7; i++) {
        edit_horiz[i] = 0;
    }
    edit_horiz[1] = 1;
    edit_horiz[2] = 1;

    set_mode(0x13);
    kb_flush();
    set_key_vect(1, key_pause);
    edit_screen();
    edit_screen1();
    display_edit_pacman(curr_pacman, 1, offset);
    display_anim_delay(anim_delay);
    display_file_control(0);
    display_file_control(1);
    display_file_control(2);
    display_pacfile(file_name);
    hilite_edit(1, 0, 0);

    do {
        iter++;
        if (iter > 10) {
            display_edit_pacman(curr_pacman, 0, offset);
            offset++;
            if (offset > 5) {
                offset = 0;
            }
            iter = 0;
        }

        if (edit_pos == 0) {
            edit_pacname(curr_pacman, _pacname[curr_pacman], &inkey, &ext);
        }
        if (edit_pos == 5) {
            edit_pacfile(curr_pacman, file_name, &inkey, &ext);
        }

        if (edit_pos == 0 || edit_pos == 5 || read_key() == 1) {
            if (edit_pos != 0 && edit_pos != 5) {
                kb_event(&inkey, &ext);
            }

            if (inkey == 0 && ext == 0x4b &&
                ((edit_horiz[edit_pos] > 1 && (edit_pos == 1 || edit_pos == 2)) ||
                 (edit_horiz[edit_pos] > 0 && edit_pos == 6))) {
                hilite_edit(0, edit_pos, edit_horiz[edit_pos]);
                edit_horiz[edit_pos]--;
                hilite_edit(1, edit_pos, edit_horiz[edit_pos]);
            }
            if (inkey == 0 && ext == 0x4d &&
                (edit_pos == 1 || edit_pos == 2 || edit_pos == 6) &&
                ((edit_pos == 1 && edit_horiz[1] < 2) ||
                 (edit_pos == 2 && edit_horiz[2] < 6) ||
                 (edit_pos == 6 && edit_horiz[6] < 2))) {
                hilite_edit(0, edit_pos, edit_horiz[edit_pos]);
                edit_horiz[edit_pos]++;
                hilite_edit(1, edit_pos, edit_horiz[edit_pos]);
            }
            if (inkey == 0 && ext == 0x48 && edit_pos > 0) {
                hilite_edit(0, edit_pos, edit_horiz[edit_pos]);
                edit_pos--;
                hilite_edit(1, edit_pos, edit_horiz[edit_pos]);
            }
            if (inkey == 0 && ext == 0x50 && edit_pos < 6) {
                hilite_edit(0, edit_pos, edit_horiz[edit_pos]);
                edit_pos++;
                hilite_edit(1, edit_pos, edit_horiz[edit_pos]);
            }
            if (inkey == 0 && ext == 0x47 && edit_pos > 0) {
                hilite_edit(0, edit_pos, edit_horiz[edit_pos]);
                edit_pos = 0;
                hilite_edit(1, 0, edit_horiz[0]);
            }
            if (inkey == 0 && ext == 0x4f && edit_pos < 6) {
                hilite_edit(0, edit_pos, edit_horiz[edit_pos]);
                edit_pos = 6;
                hilite_edit(1, 6, edit_horiz[6]);
            }

            if (inkey == 0x2d && anim_delay < 10) {
                anim_delay++;
                display_anim_delay(anim_delay);
            }
            if (inkey == 0x2b && anim_delay > 0) {
                anim_delay--;
                display_anim_delay(anim_delay);
            }

            if (inkey == 0xd && edit_pos > 0 && edit_pos < 4) {
                load_font();
                old_font = SetTextFont(-1);
                SetTextFont(4);
                set_mode(0x13);
                idx = get_sprite_data(curr_pacman, edit_pos, edit_horiz[edit_pos] - 1);
                copy_sprite_to_gen(&_sprites[idx]);
                sprite_file_ptr = _pacname;
                sprite_gen(1, &sprite_file_ptr);
                copy_gen_to_sprite(&_sprites[idx]);
                idx2 = get_mirror_sprite_data(curr_pacman, edit_pos, edit_horiz[edit_pos] - 1);
                copy_gen_to_mirror_sprite(&_sprites[idx2]);
                SetTextFont(old_font);
                set_mode(0x13);
                edit_screen();
                edit_screen1();
                display_edit_pacman(curr_pacman, 1, offset);
                display_pacname(_pacname[curr_pacman]);
                display_anim_delay(anim_delay);
                display_file_control(0);
                display_file_control(1);
                display_file_control(2);
                display_pacfile(file_name);
                hilite_edit(1, edit_pos, edit_horiz[edit_pos]);
            }

            if (inkey == 0xd && edit_pos == 6) {
                if (edit_horiz[6] == 0) {
                    if (list_pacmen(0, file_name) != 0) {
                        load_character(curr_pacman, file_name);
                    }
                    set_mode(0x13);
                    edit_screen();
                    edit_screen1();
                    display_edit_pacman(curr_pacman, 1, offset);
                    display_pacname(_pacname[curr_pacman]);
                    display_anim_delay(anim_delay);
                    display_file_control(0);
                    display_file_control(1);
                    display_file_control(2);
                    display_pacfile(file_name);
                    hilite_edit(1, 6, edit_horiz[6]);
                } else if (edit_horiz[6] == 1) {
                    display_filestatus(1, 0);
                    load_character(curr_pacman, file_name);
                    display_filestatus(0, 0);
                    display_edit_pacman(curr_pacman, 1, offset);
                    display_pacname(_pacname[curr_pacman]);
                } else if (edit_horiz[6] == 2) {
                    display_filestatus(1, 1);
                    save_character(curr_pacman, file_name);
                    display_filestatus(0, 1);
                }
            }
        }

        milliseconds = (anim_delay < 6) ? anim_delay * 5 : anim_delay * 10;
        delay(milliseconds);
    } while (_esc == 0 && inkey != 0x1b);

    if (inkey == 0x1b || _esc == 1) {
        _esc = 0;
    }
    set_key_vect(0, 0);
    return 0;
}

/*
 * list_pacmen(): full-screen scrollable browser over every *.PAC file
 * on disk (list built by load_all_files()). Enter copies the
 * highlighted filename into *file_name and returns 1; Esc (or the
 * global _esc flag) returns 0 without touching *file_name. Always
 * restores the character roster's real sprites/names (buffer_pacmen()
 * was called before entering, by the caller) via restore_pacmen()
 * before returning either way.
 */
int list_pacmen(int curr_pacman, char far * file_name)
{
    unsigned int inkey = 0;
    unsigned int ext = 0;
    int iter = 0;
    int offset = 0;
    int file_offset = 0;
    int total;

    set_mode(0x13);
    kb_flush();
    edit_screen();
    list_message(1);
    buffer_pacmen();
    total = load_all_files();
    fill_list(0, 0, 10);
    display_list_pacmen(0, 0);
    list_message(0);
    hilite_list_pacman(1, curr_pacman);

    for (;;) {
        iter++;
        if (iter > 10) {
            display_list_pacmen(offset, file_offset);
            offset++;
            if (offset > 5) {
                offset = 0;
            }
            iter = 0;
        }

        if (read_key() == 1) {
            kb_event(&inkey, &ext);

            if (inkey == 0 && ext == 0x48 && curr_pacman > 0) {
                hilite_list_pacman(0, curr_pacman);
                curr_pacman--;
                hilite_list_pacman(1, curr_pacman);
            } else if (inkey == 0 && ext == 0x48 && curr_pacman == 0 && file_offset > 0) {
                hilite_list_pacman(0, 0);
                file_offset--;
                scroll_pacmen(1);
                fill_list(file_offset, 0, 1);
                display_list_pacmen(0, file_offset);
                kb_flush();
                hilite_list_pacman(1, curr_pacman);
            }

            if (inkey == 0 && ext == 0x50 && curr_pacman < 9) {
                hilite_list_pacman(0, curr_pacman);
                curr_pacman++;
                hilite_list_pacman(1, curr_pacman);
            } else if (inkey == 0 && ext == 0x50 && curr_pacman == 9 && file_offset + 9 < total - 1) {
                hilite_list_pacman(0, 9);
                file_offset++;
                scroll_pacmen(-1);
                fill_list(file_offset + 9, 9, 1);
                display_list_pacmen(0, file_offset);
                kb_flush();
                hilite_list_pacman(1, curr_pacman);
            }

            if (inkey == 0 && ext == 0x49 && file_offset > 0) {
                file_offset = (file_offset < 9) ? 0 : file_offset - 9;
                hilite_list_pacman(0, curr_pacman);
                list_message(1);
                fill_list(file_offset, 0, 10);
                display_list_pacmen(0, file_offset);
                kb_flush();
                list_message(0);
                hilite_list_pacman(1, curr_pacman);
            }

            if (inkey == 0 && ext == 0x51 && file_offset < total - 10) {
                hilite_list_pacman(0, curr_pacman);
                file_offset += 9;
                if (file_offset + 0x13 > total) {
                    file_offset = total - 10;
                }
                list_message(1);
                fill_list(file_offset, 0, 10);
                display_list_pacmen(0, file_offset);
                kb_flush();
                list_message(0);
                hilite_list_pacman(1, curr_pacman);
            }

            if (inkey == 0 && ext == 0x47) {
                hilite_list_pacman(0, curr_pacman);
                if (file_offset > 0) {
                    file_offset = 0;
                    list_message(1);
                    fill_list(0, 0, 10);
                    display_list_pacmen(0, 0);
                    list_message(0);
                }
                curr_pacman = 0;
                hilite_list_pacman(1, 0);
            }

            if (inkey == 0 && ext == 0x4f) {
                hilite_list_pacman(0, curr_pacman);
                if (file_offset < total - 10) {
                    file_offset = total - 10;
                    list_message(1);
                    fill_list(file_offset, 0, 10);
                    display_list_pacmen(0, file_offset);
                    list_message(0);
                }
                curr_pacman = 9;
                hilite_list_pacman(1, 9);
            }

            if (inkey == 0xd) {
                strcpy(file_name, _pac_files[file_offset + curr_pacman]);
            }
        }

        delay(5);

        if (_esc != 0 || inkey == 0x1b || inkey == 0xd) {
            restore_pacmen();
            return (inkey == 0xd);
        }
    }
}

/*
 * buffer_pacmen(): stash every character's name and sprite bitmap
 * pointers into the _*_buff scratch arrays and NULL out the live
 * _sprites[].sprite fields, so the roster/list screens can safely swap
 * in their own simplified preview sprites without disturbing (or
 * leaking) the real per-character animation data. Paired with
 * restore_pacmen().
 */
void buffer_pacmen(void)
{
    int i, j, idx;

    for (i = 0; i < 10; i++) {
        strcpy(_name_buff[i], _pacname[i]);

        idx = get_sprite_data(i, 1, 0);
        for (j = 0; j < 4; j++) {
            _man_buff[i][j] = _sprites[idx].sprite;
            _sprites[idx].sprite = 0;
            idx++;
        }
        idx = get_sprite_data(i, 2, 0);
        for (j = 0; j < 12; j++) {
            _death_buff[i][j] = _sprites[idx].sprite;
            _sprites[idx].sprite = 0;
            idx++;
        }
        idx = get_sprite_data(i, 3, 0);
        for (j = 0; j < 2; j++) {
            _bull_buff[i][j] = _sprites[idx].sprite;
            _sprites[idx].sprite = 0;
            idx++;
        }
    }
}

void restore_pacmen(void)
{
    int i, j, idx;

    for (i = 0; i < 10; i++) {
        strcpy(_pacname[i], _name_buff[i]);

        idx = get_sprite_data(i, 1, 0);
        for (j = 0; j < 4; j++) {
            free(_sprites[idx].sprite);
            _sprites[idx].sprite = _man_buff[i][j];
            idx++;
        }
        idx = get_sprite_data(i, 2, 0);
        for (j = 0; j < 12; j++) {
            free(_sprites[idx].sprite);
            _sprites[idx].sprite = _death_buff[i][j];
            idx++;
        }
        idx = get_sprite_data(i, 3, 0);
        for (j = 0; j < 2; j++) {
            free(_sprites[idx].sprite);
            _sprites[idx].sprite = _bull_buff[i][j];
            idx++;
        }
    }
}

/*
 * scroll_pacmen(): shifts every character's name and live sprite
 * pointers one slot toward index 0 (dir==-1) or index 9 (dir==1),
 * freeing whichever end slot gets vacated. Used by list_pacmen() while
 * scrolling the on-disk file browser up/down a row at a time, to slide
 * the on-screen roster preview along with it without a full reload.
 */
void scroll_pacmen(int dir)
{
    int i, j, src, dst;

    if (dir == -1) {
        for (i = 1; i < 10; i++) {
            strcpy(_pacname[i - 1], _pacname[i]);

            src = get_sprite_data(i, 1, 0);
            dst = get_sprite_data(i - 1, 1, 0);
            for (j = 0; j < 4; j++) {
                if (i == 1) {
                    free(_sprites[dst].sprite);
                }
                _sprites[dst].sprite = _sprites[src].sprite;
                _sprites[src].sprite = 0;
                dst++;
                src++;
            }
            src = get_sprite_data(i, 2, 0);
            dst = get_sprite_data(i - 1, 2, 0);
            for (j = 0; j < 12; j++) {
                if (i == 1) {
                    free(_sprites[dst].sprite);
                }
                _sprites[dst].sprite = _sprites[src].sprite;
                _sprites[src].sprite = 0;
                dst++;
                src++;
            }
            src = get_sprite_data(i, 3, 0);
            dst = get_sprite_data(i - 1, 3, 0);
            for (j = 0; j < 2; j++) {
                if (i == 1) {
                    free(_sprites[dst].sprite);
                }
                _sprites[dst].sprite = _sprites[src].sprite;
                _sprites[src].sprite = 0;
                dst++;
                src++;
            }
        }
    } else if (dir == 1) {
        for (i = 8; i >= 0; i--) {
            strcpy(_pacname[i + 1], _pacname[i]);

            src = get_sprite_data(i, 1, 0);
            dst = get_sprite_data(i + 1, 1, 0);
            for (j = 0; j < 4; j++) {
                if (i == 8) {
                    free(_sprites[dst].sprite);
                }
                _sprites[dst].sprite = _sprites[src].sprite;
                _sprites[src].sprite = 0;
                dst++;
                src++;
            }
            src = get_sprite_data(i, 2, 0);
            dst = get_sprite_data(i + 1, 2, 0);
            for (j = 0; j < 12; j++) {
                if (i == 8) {
                    free(_sprites[dst].sprite);
                }
                _sprites[dst].sprite = _sprites[src].sprite;
                _sprites[src].sprite = 0;
                dst++;
                src++;
            }
            src = get_sprite_data(i, 3, 0);
            dst = get_sprite_data(i + 1, 3, 0);
            for (j = 0; j < 2; j++) {
                if (i == 8) {
                    free(_sprites[dst].sprite);
                }
                _sprites[dst].sprite = _sprites[src].sprite;
                _sprites[src].sprite = 0;
                dst++;
                src++;
            }
        }
    }
}

/*
 * display_list_pacmen(): draws one page (10 rows) of the file-browser
 * roster strip -- a small preview sprite, the character's name, and
 * the on-disk filename for that row. Falls back to sprite slot 1 (and
 * forces it to a generic 16x16 size) when a row's own preview sprite
 * pointer is NULL, i.e. that character slot has no sprite loaded.
 */
void display_list_pacmen(int offset, int file_offset)
{
    int i, idx;

    for (i = 0; i < 10; i++) {
        idx = i * 4 + (offset & 1) + 4;
        if (_sprites[idx].sprite == 0) {
            idx = 1;
            _sprites[1].spritew = 0x10;
            _sprites[1].spriteh = 0x10;
        }
        _sprites[idx].spritex = 0x14;
        _sprites[idx].spritey = i * 0x12 + 7;
        display_sprite(idx);
        text256(0x34, i * 0x12 + 0xd, (unsigned char far *) _pacname[i], 0xf, 0);
        text256(0xa6, i * 0x12 + 0xd, (unsigned char far *) _pac_files[file_offset + i], 0xe, 0);
        idx = i * 0xc + offset + 0x45f;
        _sprites[idx].spritex = 0x88;
        _sprites[idx].spritey = i * 0x12 + 7;
        display_sprite(idx);
    }
}

void hilite_list_pacman(int status, int pacman)
{
    int colour = (status == 1) ? 0xe : 0;
    trbox(0x12, pacman * 0x12 + 6, 0xf8, 0x13, colour);
}

/*
 * get_sprite_data()/get_mirror_sprite_data(): map (character, field,
 * sub-index) to a flat index into the shared _sprites[] array --
 * edit_pos 1=walk (4 frames), 2=death (12 frames), 3=bullet (2 frames).
 * The mirror variants point at each frame's horizontally-flipped
 * counterpart, stored a fixed few slots after the normal one. Any
 * other edit_pos isn't reachable from any real call site in this
 * binary (always guarded by `0 < edit_pos && edit_pos < 4` at the
 * caller) -- the original compiled function left its result register
 * genuinely uninitialized in that case; returning 0 here instead is
 * strictly safer and never observably different for any real caller.
 */
int get_sprite_data(int curr_pacman, int edit_pos, int edit_horiz)
{
    int idx = 0;

    if (edit_pos == 1) {
        idx = curr_pacman * 4 + edit_horiz + 4;
    } else if (edit_pos == 2) {
        idx = curr_pacman * 0xc + edit_horiz + 0x45f;
    } else if (edit_pos == 3) {
        idx = curr_pacman * 2 + 0x2c;
    }
    return idx;
}

int get_mirror_sprite_data(int curr_pacman, int edit_pos, int edit_horiz)
{
    int idx = 0;

    if (edit_pos == 1) {
        idx = curr_pacman * 4 + edit_horiz + 6;
    } else if (edit_pos == 2) {
        idx = curr_pacman * 0xc + edit_horiz + 0x465;
    } else if (edit_pos == 3) {
        idx = curr_pacman * 2 + 0x2d;
    }
    return idx;
}

/*
 * edit_screen1()/display_save()/display_filestatus(): same names as
 * three functions in MAZEEDIT.C's maze editor (confirmed real,
 * distinct compiled functions at different addresses -- see the
 * now-superseded unk_func_manedit_1/2/3 comments this replaced).
 * Declared static so both modules can define their own same-named
 * function without a duplicate-symbol link error, since MAZEEDIT.C's
 * versions are already non-static/exported.
 */
static void edit_screen1(void)
{
    trfbox(0xe2, 0x1e, 0x40, 0x18, 0x1a);
    text256(0xe6, 0x22, (unsigned char far *) "PacWars", 0xf, 0x1a);
    text256(0xe6, 0x2a, (unsigned char far *) "Editor", 0xe, 0x1a);
}

void edit_screen(void)
{
    trbox(0, 0, 0x140, 200, 0xf);
}

void display_edit_pacman(int curr_pacman, int init, int offset)
{
    int i, idx, x, y;

    get_edit_pos(1, 0, &x, &y);
    idx = curr_pacman * 4 + offset % 2 + 4;
    _sprites[idx].spritex = x;
    _sprites[idx].spritey = y;
    display_sprite(idx);

    if (init == 1) {
        for (i = 0; i < 2; i++) {
            get_edit_pos(1, i + 1, &x, &y);
            idx = curr_pacman * 4 + i + 4;
            _sprites[idx].spritex = x;
            _sprites[idx].spritey = y;
            display_sprite(idx);
        }
    }

    get_edit_pos(2, 0, &x, &y);
    idx = curr_pacman * 0xc + offset + 0x45f;
    _sprites[idx].spritex = x;
    _sprites[idx].spritey = y;
    display_sprite(idx);

    if (init == 1) {
        for (i = 0; i < 6; i++) {
            get_edit_pos(2, i + 1, &x, &y);
            idx = curr_pacman * 0xc + i + 0x45f;
            _sprites[idx].spritex = x;
            _sprites[idx].spritey = y;
            display_sprite(idx);
        }
    }

    /* "Currently selected" pointer glyph (0x18, a custom-font marker
     * character) under the selected death-animation frame, blank under
     * the rest -- byte values confirmed via read_memory at 340e:1573/
     * 340e:1575. */
    for (i = 0; i < 6; i++) {
        get_edit_pos(2, i + 1, &x, &y);
        text256(x + 4, y + 0x10, (unsigned char far *) (i == offset ? "\x18" : " "), 0xf, 0);
    }

    get_edit_pos(3, 0, &x, &y);
    idx = (curr_pacman * 4 + offset % 2) / 4 * 2 + 0x2c;
    _sprites[idx].spritex = x + 4;
    _sprites[idx].spritey = y;
    display_sprite(idx);
}

static void display_save(int status)
{
    if (status == -1) {
        trfbox(0xe6, 0x46, 0x36, 0x10, 2);
        trbox(0xe8, 0x48, 0x32, 0xc, 0xf);
        text256(0xea, 0x4a, (unsigned char far *) " Save", 0xe, 2);
    }
    if (status == 1) {
        trbox(0xe3, 0x43, 0x3c, 0x16, 0xe);
    }
    if (status == 0) {
        trbox(0xe3, 0x43, 0x3c, 0x16, 0);
    }
}

static void display_filestatus(int status, int type)
{
    static char * const labels[3] = { "Loading...", "Saving... ", "          " };
    int x, y;

    get_edit_pos(6, 1, &x, &y);
    x += 0x32;
    y += -0x32;
    if (status == 1) {
        text256(x, y, (unsigned char far *) labels[type], 0xb, 0);
    } else if (status == 0) {
        text256(x, y, (unsigned char far *) labels[2], 0xb, 0);
    }
}

void display_anim_delay(int delay_val)
{
    int x, y, i, back;

    get_edit_pos(4, 0, &x, &y);
    trfbox(x, y, 0x6a, 0x10, 3);
    trbox(x + 2, y + 2, 0x66, 0xc, 0xf);
    y += 4;
    text256(x + 4, y, (unsigned char far *) "-", 0xe, 3);
    x += 0xc;
    for (i = 0; i < 10; i++) {
        back = (i < 10 - delay_val) ? 0xf : 3;
        text256(x + i * 8, y, (unsigned char far *) " ", 0, back);
    }
    x += 0x50;
    text256(x, y, (unsigned char far *) "+", 0xe, 3);
}

void display_file_control(int edit_horiz)
{
    static char * const labels[3] = { " List", " Load", " Save" };
    int x, y;

    get_edit_pos(6, edit_horiz, &x, &y);
    trfbox(x, y, 0x36, 0x10, 2);
    trbox(x + 2, y + 2, 0x32, 0xc, 0xf);
    text256(x + 4, y + 4, (unsigned char far *) labels[edit_horiz], 0xe, 2);
}

void edit_pacname(int curr_pacman, char far * name_str, unsigned int far * inkey, unsigned int far * ext)
{
    int x, y, key;

    get_edit_pos(0, 0, &x, &y);
    key = edit_str(inkey, ext, y / 8, x / 8, name_str, edit_type_general, curr_pacman + 1);
    if (key == 0x1b) {
        _esc = 1;
    }
}

void edit_pacfile(int curr_pacman, char far * name_str, unsigned int far * inkey, unsigned int far * ext)
{
    int x, y, key;

    get_edit_pos(5, 0, &x, &y);
    key = edit_str(inkey, ext, y / 8, x / 8, name_str, edit_type_general, curr_pacman + 1);
    if (key == 0x1b) {
        _esc = 1;
    }
}

void display_pacname(char far * name_str)
{
    int x, y;
    get_edit_pos(0, 0, &x, &y);
    text256(x, y, (unsigned char far *) name_str, 0xf, 0);
}

void display_pacfile(char far * name_str)
{
    int x, y;
    get_edit_pos(5, 0, &x, &y);
    text256(x, y, (unsigned char far *) name_str, 0xf, 0);
}

void hilite_edit(int status, int edit_pos, int edit_horiz)
{
    int x, y, colour;

    get_edit_pos(edit_pos, edit_horiz, &x, &y);
    colour = (status == 1) ? 0xe : 0;
    trbox(x - 3, y - 3, edit_w[edit_pos] + 6, edit_h[edit_pos], colour);
}

void get_edit_pos(int edit_pos, int edit_horiz, int far * xpos, int far * ypos)
{
    *xpos = 0x18;
    *ypos = 0x18;
    if (edit_pos == 1 || edit_pos == 2) {
        *xpos += edit_horiz * 0x14;
    }
    if (edit_pos == 6) {
        *xpos += edit_horiz * 0x46;
    }
    *ypos += edit_pos * 0x18;
    *xpos += edit_xoff[edit_pos];
    *ypos += edit_yoff[edit_pos];
}

int sort_func(const void far * a, const void far * b)
{
    return strcmp((char far *) a, (char far *) b);
}

/*
 * load_all_files(): (re)builds _pac_files[] from every *.PAC file in
 * the game's character directory (get_filename() prepends the real
 * path), sorted via qsort()+sort_func(). Returns the number of files
 * found (capped at 200).
 */
int load_all_files(void)
{
    struct ffblk ff;
    char pac_path[80];
    int width = 0;
    int found;

    strcpy(pac_path, "*.PAC");
    get_filename(pac_path);
    found = findfirst(pac_path, &ff, 0);
    while (width < 200 && found == 0) {
        clear_str((unsigned char far *) _pac_files[width], 0xc);
        strncpy(_pac_files[width], ff.ff_name, strlen(ff.ff_name));
        found = findnext(&ff);
        width++;
    }
    qsort(_pac_files, width, sizeof(_pac_files[0]),
          (int (*)(const void far *, const void far *)) sort_func);
    return width;
}

/*
 * fill_list(): loads character data for _pac_files[spos..spos+num-1]
 * into the corresponding _sprites[]/_pacname[] slots, ahead of
 * display_list_pacmen() drawing that page. The `curr_pacman` parameter
 * is part of the real exported signature (per PACWARS.TXT) but is
 * never actually read by the compiled function -- confirmed via
 * disassembly; every real call site passes 0 for it anyway (see
 * list_pacmen()).
 */
void fill_list(int curr_pacman, int spos, int num)
{
    int i;

    for (i = spos; i < spos + num; i++) {
        load_character(i, _pac_files[i]);
    }
}

/* Identical to edit_screen() in the real compiled binary -- both just
 * paint a full-screen white box. Kept as a separate function to match
 * PACWARS.TXT's exported symbol list. */
void edit_screen2(void)
{
    trbox(0, 0, 0x140, 200, 0xf);
}

void list_message(int status)
{
    static char * const mess[2] = { "          ", "Loading..." };
    text256(0x14, 0xbe, (unsigned char far *) mess[status], 0xb, 0);
}
