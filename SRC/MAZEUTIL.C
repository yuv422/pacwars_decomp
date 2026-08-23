/*
 * MAZEUTIL.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZEUTIL.H"
#include "GRAPH256.H"
#include "MAZESPT.H"
#include "MVAGRAPH.H"
#include "MEDITSTR.H"
#include "UTILS.H"
#include "MAZE.H"
#include "HISCORE.H"
#include <string.h>
#include <stdio.h>

/*
 * Referenced only from get_filename() below (confirmed via Ghidra xref
 * analysis), so file-scope here rather than exported. 140 bytes, holding
 * one or more back-to-back null-terminated strings; its compiled-in
 * default is just the marker "MAZEPATH" followed by zeroed/unused space.
 */
static char net_path[140] = "MAZEPATH";

/* storage for the _max_x/_max_y/_sc_width globals declared extern in
   PACWARS.H */
int _max_x = 240;
int _max_y = 200;
int _sc_width = 320;

/* storage for the _wstation global declared extern in PACWARS.H --
   _VSIZE/_HSIZE/_maze/_maze_attrib's real owners (choose_edit_maze /
   alloc_maze_def_mem, MAZEEDIT.C) are now decompiled, so their storage
   moved there; _wstation's and _animate_maze's real write sites are
   still unidentified, so they stay here for now. */
int _wstation;

/* storage for the f1 global declared extern in PACWARS.H */
int f1;

/* storage for the power-up count globals declared extern in PACWARS.H */
int _warp_count;
int _shield_count;
int _grenade_count;
int _ishot_count;
int _missile_count;
int _visible_count;

/* storage for the edit_name_flag global declared extern in PACWARS.H */
int edit_name_flag;

/*
 * _animate_maze's pointers are NOT runtime-allocated at all -- unlike
 * MAZEANIM.C's unrelated ANIM_OBJECT-based room-animation system (which
 * does calloc() a fresh per-object scratch buffer at startup), the
 * original binary just points each of these 12 slots directly at
 * statically-compiled ANIM_OB[] data baked into the .EXE's data segment
 * (340e:00f9-0308, immediately followed by the _animate_maze array
 * itself at 340e:0309 -- confirmed via read_memory). Recovered
 * byte-for-byte below.
 *
 * Field values decoded per update_room()'s own usage above: hoffset/
 * voffset are the maze-cell coordinates being animated (def[hoffset]
 * [voffset]); row/col set the animation's rate/phase; offset is the
 * frame count (the modulus); animate/redraw aren't read by update_room
 * itself (possibly consumed elsewhere, or vestigial) but are preserved
 * as compiled. Lists are hoffset==-1 terminated, matching update_room's
 * own loop condition.
 *
 * Each entry's prev_frame_buffer -- despite the name/comment it shares
 * with MAZEANIM.H's unrelated ANIM_OBJECT struct, where it really is a
 * runtime scratch pointer -- is, for THIS struct, read directly as
 * compile-time animation data by update_room() (`frame_table[frame_
 * index]`): the sequence of maze tile IDs the cell cycles through.
 * Every real pointer in the original binary lands inside one 87-byte
 * table immediately preceding this data (340e:00a2-00f8) -- recovered
 * as one flat array below (anim_frame_tiles) since the individual
 * ANIM_OB entries reference overlapping/adjacent offsets within it
 * rather than cleanly separate arrays.
 */
static unsigned char anim_frame_tiles[87] = {
    /* 0x00a2 */ 0,0,0,0,12,13,12,13,0,0,0,0,13,12,13,12,
    /* 0x00b2 */ 0,0,0,0,14,15,14,15,0,0,0,0,15,14,15,14,
    /* 0x00c2 */ 0,0,0,29,29,29,29,29,31,31,31,30,30,30,30,30,
    /* 0x00d2 */ 0,0,0,33,33,33,33,33,35,35,35,32,32,32,32,32,
    /* 0x00e2 */ 54,55,20,26,40,52,40,52,41,53,41,53,255,0,0,0,
    /* 0x00f2 */ 0,0,0,0,0,0,0
};

/*
 * Empty-room terminators. The original binary actually has TWO distinct
 * lone-{-1,...} records for this: room (1,0) gets its own at 340e:0243,
 * while rooms (2,0)-(2,2) and (3,0)-(3,2) all share a second one at
 * 340e:02fe. Byte-identical content, but kept as separate compiled
 * objects here to match that real layout (rather than collapsing all 7
 * empty slots onto one shared array).
 */
static ANIM_OB anim_room_1_0[] = {
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OB anim_room_empty[] = {
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OB anim_room_0_0[] = {
    { 16, 19, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[0] },
    { 16, 20, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[8] },
    { 16, 21, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[0] },
    { 17, 22, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 18, 22, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[24] },
    { 19, 22, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 11, 29, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 12, 29, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[24] },
    { 13, 29, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 17, 29, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 18, 29, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[24] },
    { 19, 29, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 20, 23, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[0] },
    { 20, 24, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[8] },
    { 20, 25, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[0] },
    { 16, 26, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[0] },
    { 16, 27, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[8] },
    { 16, 28, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[0] },
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OB anim_room_0_1[] = {
    { 11, 0, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 12, 0, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[24] },
    { 13, 0, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 17, 0, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { 18, 0, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[24] },
    { 19, 0, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[16] },
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OB anim_room_0_2[] = {
    { 8, 23, 8, 0, 2, -1, 0, (int far *) &anim_frame_tiles[66] },
    { 18, 15, 4, 0, 8, -1, 0, (int far *) &anim_frame_tiles[68] },
    { 18, 16, 4, 0, 8, -1, 0, (int far *) &anim_frame_tiles[72] },
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OB anim_room_1_1[] = {
    { 11, 13, 4, 0, 8, -1, 0, (int far *) &anim_frame_tiles[68] },
    { 11, 14, 4, 0, 8, -1, 0, (int far *) &anim_frame_tiles[72] },
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OB anim_room_1_2[] = {
    { 12, 4, 8, 1, 8, -1, 1, (int far *) &anim_frame_tiles[32] },
    { 19, 8, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[32] },
    { 17, 14, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[32] },
    { 11, 4, 8, 1, 8, -1, 0, (int far *) &anim_frame_tiles[40] },
    { 18, 8, 8, 0, 8, -1, 0, (int far *) &anim_frame_tiles[40] },
    { 16, 14, 8, 4, 8, -1, 0, (int far *) &anim_frame_tiles[40] },
    { 13, 4, 8, 1, 8, -1, 1, (int far *) &anim_frame_tiles[48] },
    { 20, 8, 8, 0, 8, -1, 1, (int far *) &anim_frame_tiles[48] },
    { 18, 14, 8, 4, 8, -1, 1, (int far *) &anim_frame_tiles[48] },
    { 14, 4, 8, 1, 8, -1, 0, (int far *) &anim_frame_tiles[56] },
    { 21, 8, 8, 0, 8, -1, 0, (int far *) &anim_frame_tiles[56] },
    { 19, 14, 8, 4, 8, -1, 0, (int far *) &anim_frame_tiles[56] },
    { -1, 0, 0, 0, 0, 0, 0, 0 }
};

/* storage for the _animate_maze global declared extern in PACWARS.H,
   indexed [voff][hoff] -- see the comment above for how this data was
   recovered. */
ANIM_OB far * _animate_maze[4][3] = {
    { anim_room_0_0, anim_room_0_1, anim_room_0_2 },
    { anim_room_1_0, anim_room_1_1, anim_room_1_2 },
    { anim_room_empty, anim_room_empty, anim_room_empty },
    { anim_room_empty, anim_room_empty, anim_room_empty }
};

/* forward declarations: update_map() (below) calls maze_def(),
   attrib_maze_def(), and update_room(), all defined further down in this
   file. */
void far * maze_def(int hoff, int voff);
void far * attrib_maze_def(int hoff, int voff);
void update_room(unsigned char sync, ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr);

/* forward declarations for other same-file functions used before their
   own definitions further down: display_registered_company() (below)
   calls reg_text(); display_instructions() (below) calls
   display_tokens(). */
void reg_text(int x, int y, char far * letter, int pos);
void display_tokens(int offset);

/*
 * Sets the clipping viewport width used by the low-level graphics
 * primitives: the full screen width (_sc_width) normally, or a narrower
 * 0xf0-pixel-wide play area (status != 0) that leaves room for the
 * scoreboard/status display along the right edge.
 */
void set_clip_window(int status)
{
    if (status == 0) {
        _max_x = _sc_width;
    } else {
        _max_x = 0xf0;
    }
}

/*
 * Bumps this workstation's sync counter once per call, but only for the
 * local machine's own slot: connection[] holds a run of nonzero bytes for
 * each connected station, and the count of consecutive nonzero entries
 * before the first zero identifies which slot (0-4) is "us" by matching
 * _wstation.
 */
void update_sync(MAZE_LOG_STRUCT far * maze_log)
{
    int i;

    for (i = 0; i < 5 && maze_log->connection[i] == '\0'; i++) {
    }
    if (i == _wstation) {
        maze_log->sync++;
    }
}

/*
 * Advances this workstation's game clock (maze_log->time) once every 101
 * calls, and decays an active warp effect's countdown (token.warp_factor,
 * stepped by token.warp_dir) once every 6 calls while a warp is active.
 * Only runs for the local machine's slot, identified the same way as in
 * update_sync() above. The two tick counters are file-local statics,
 * confirmed via Ghidra xref analysis to be read/written only from within
 * this function.
 */
void update_time(MAZE_LOG_STRUCT far * maze_log)
{
    static int time_tick;
    static int warp_tick;
    int i;

    for (i = 0; i < 5 && maze_log->connection[i] == '\0'; i++) {
    }
    if (i == _wstation) {
        if (time_tick > 100) {
            maze_log->time++;
            time_tick = 0;
        } else {
            time_tick++;
        }

        if (maze_log->token.warp_factor > 0) {
            if (warp_tick > 5) {
                maze_log->token.warp_factor += maze_log->token.warp_dir;
                warp_tick = 0;
            } else {
                warp_tick++;
            }
        }
    }
}

/*
 * Refreshes every room in the current maze's room grid, redrawing each
 * room's animated cells (see update_room() below) for the given sync tick.
 */
void update_map(unsigned char sync)
{
    int voff, hoff;

    for (voff = 0; voff < _VSIZE; voff++) {
        for (hoff = 0; hoff < _HSIZE; hoff++) {
            update_room(sync, _animate_maze[voff][hoff], (MAZE_STRUCT far *) maze_def(hoff, voff));
        }
    }
}

/*
 * Steps every animated-cell entry in one room's animation list (obj[],
 * terminated by an entry with hoffset == -1) to whichever frame its own
 * rate/phase (row, col) and the global sync tick select, and writes that
 * frame's tile value into the room's maze cell array (maze_ptr->def).
 */
void update_room(unsigned char sync, ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
    int i;
    int period;
    int frame_index;
    char far * frame_table;

    /*
     * NOTE: the frame byte read below is sign-extended before being stored
     * into the (unsigned int) maze cell, per the disassembly's CBW at
     * 2611:01e2 -- hence char rather than unsigned char here, matching
     * the signedness of obj[i].offset/hoffset/voffset (also CBW-extended)
     * rather than obj[i].row/col/sync (MOV reg,0 zero-extended instead).
     */
    for (i = 0; obj[i].hoffset != -1; i++) {
        period = 0xff / obj[i].row + 1;
        frame_index = (sync / period + obj[i].col) % obj[i].offset;
        frame_table = (char far *) obj[i].prev_frame_buffer;
        maze_ptr->def[obj[i].hoffset][obj[i].voffset] = frame_table[frame_index];
    }
}

/*
 * Looks up the given room's maze-cell data block by room-grid coordinates.
 * See the _maze comment in PACWARS.H.
 */
void far * maze_def(int hoff, int voff)
{
    return _maze[voff][hoff];
}

/*
 * Looks up the given room's attribute-cell data block by room-grid
 * coordinates. See the _maze_attrib comment in PACWARS.H.
 */
void far * attrib_maze_def(int hoff, int voff)
{
    return _maze_attrib[voff][hoff];
}

/*
 * Forward declaration: display_pacmen() (below) calls
 * display_registered_company(), which -- like display_registered(), which
 * also calls it -- is not in PACWARS.TXT's exported-symbol list (so it
 * gets no MAZEUTIL.H prototype), but its address (2611:03b0) falls between
 * display_registered (2611:02e3) and hilite_pacman (2611:043d), so it's
 * defined further down in this file, in that same relative position.
 */
void display_registered_company(void);

/*
 * Draws the pacman roster screen: one row per playable character (10
 * total), each showing that character's sprite plus its name, with the
 * registered-company byline redrawn every 3rd row.
 */
void display_pacmen(int offset)
{
    static char pacname[10][13] = {
        "PacMan    ", "PacPsycho ", "PacRat    ", "PacTart   ",
        "PacBaby   ", "PacRambo  ", "PacPerv   ", "PacTurd   ",
        "PacBiggles", "PacBum    "
    };
    int i;
    int sprite_num;

    for (i = 0; i < 10; i++) {
        sprite_num = i * 4 + offset + 4;
        _sprites[sprite_num].spritex = 0x14;
        _sprites[sprite_num].spritey = i * 0x11 + 2;
        display_sprite(sprite_num);
        text256(0x2c, i * 0x11 + 8, (unsigned char far *) pacname[i], 0xf, 0);
        if (i % 3 == 0) {
            display_registered_company();
        }
    }
}

/*
 * Draws the registration-status box: the registered owner's name (or, if
 * unregistered, the shareware reg_str message plus the "not registered"
 * reg_name text) inside a bordered box in the lower-left corner.
 */
void display_registered(void)
{
    trfbox(0, 0xb0, 0x90, 0x18, 0x1a);
    trbox(1, 0xb1, 0x8e, 0x16, 0x1e);
    text256(4, 0xb4, (unsigned char far *) reg, 0xe, 0x1a);
    if (reg->registered == 1) {
        display_registered_company();
    } else {
        text256(4, 0xbc, (unsigned char far *) reg->reg_name, 0xf, 0x1a);
    }
}

/*
 * When registered, scrolls the registered owner's name across the
 * registration box one character at a time via reg_text() (below), which
 * handles the per-character colour cycling/positioning.
 */
void display_registered_company(void)
{
    static char letter;
    int font;
    int pos;

    font = SetTextFont(-1);
    SetTextFont(4);
    if (reg->registered == 1) {
        for (pos = 0; pos < (int) strlen(reg->reg_name); pos++) {
            letter = reg->reg_name[pos];
            reg_text(4, 0xbc, &letter, pos);
        }
    }
    SetTextFont(font);
}

/*
 * Draws the bordered score box in the top-right of the screen, showing
 * the "Score" label and the current score (curr_score, stored internally
 * in tens and multiplied back out for display) right-justified in a
 * 6-digit field.
 */
void display_curr_score(void)
{
    char sc_str[10];

    sprintf(sc_str, "%6ld", (long) curr_score * 10L);

    trfbox(0xa0, 2, 0x48, 0x20, 0x35);
    hline(0xa2, 4, 0x44, 0x36);
    vline(0xa2, 4, 0x1c, 0x36);
    hline(0xa2, 0x1f, 0x44, 0x34);
    vline(0xe5, 4, 0x1c, 0x34);
    text256(0xb0, 8, (unsigned char far *) "Score", 0x1e, 0x35);
    text256(0xa8, 0x14, (unsigned char far *) sc_str, 0xe, 0x35);
}

/*
 * Draws (status != 0) or erases (status == 0) the highlight box around
 * the given pacman roster row.
 */
void hilite_pacman(int status, int pacman)
{
    int colour;

    colour = (status == 1) ? 0xe : 0;
    trbox(0x12, pacman * 0x11 + 1, 0x6e, 0x12, colour);
}

/*
 * Full-screen scrolling instructions display, cycling the token legend
 * (display_tokens() below) between its two pages every ~12 frames until
 * ESC or Enter is pressed (or the game-wide esc flag is set elsewhere).
 */
void display_instructions(void)
{
    unsigned int inkey, ext;
    int key;
    int count;
    int offset;

    offset = 0;
    count = 0;
    set_mode(0x13);
    trbox(0, 0, 0x140, 200, 0xe);

    do {
        if (count > 10) {
            display_tokens(offset);
            offset ^= 1;
            count = 0;
            delay(0x4b);
        } else {
            count++;
        }
        key = read_key();
        if (key == 1) {
            kb_event(&inkey, &ext);
        }
    } while (_esc == 0 && inkey != 0x1b && inkey != 0xd);
}

/*
 * Draws one row of the high-score table's pacman-character sprite plus
 * its score and name, for score_pacmen()'s only caller, disp_hiscore()
 * below. Real signature (offset, sp_off, hiscore) recovered from the
 * call site -- the PACWARS.TXT-derived stub had guessed a bare void(void).
 */
void score_pacmen(int offset, int sp_off, HISCORE far * hiscore)
{
    char score_str[7];
    int sprite_num;
    int y;

    sprite_num = hiscore->pacman[offset] * 4 + sp_off + 4;
    _sprites[sprite_num].spritex = 0x96;
    _sprites[sprite_num].spritey = offset * 0x14 + 0x46;
    display_sprite(sprite_num);

    y = offset * 0x14 + 0x4e;
    sprintf(score_str, "%6ld", (long) hiscore->score[offset] * 10L);
    text256(0xa8, y, (unsigned char far *) score_str, 0xf, 0);
    text256(0xe0, y, (unsigned char far *) hiscore->name[offset], 0xe, 0);
}

/* forward declarations: choose_pacman() (below) calls several functions
   defined further down in this file. */
void disp_hiscore(int sp_off, HISCORE far * hiscore);
void edit_name(char far * name_str);
void display_curr_name(char far * name_str);
void f1_instructions(void);

/*
 * Main pacman-character selection screen: draws the roster, current
 * score/name/registration box and high-score table, then loops handling
 * arrow-key navigation (up/down step, home/end jump), M to rename, and F1
 * (either the real key or the f1 latch set by edit_name()'s key vector)
 * to show the instructions screen, redrawing the flipped high-score page
 * periodically. On entry, offers a name edit either because
 * edit_name_flag was set externally, or -- on a networked game -- because
 * the player's current score would place in the top 6. Returns the
 * newly-chosen 1-based pacman index, or -1 if the high-score file
 * couldn't be written (in which case the whole game exits back to DOS,
 * mirroring the original's behaviour).
 */
int choose_pacman(int curr_pacman)
{
    HISCORE hiscore;
    int offset;
    int count;
    int need_edit;
    int rank;
    unsigned int inkey, ext;

    offset = 0;
    count = 0;
    need_edit = 0;

    kb_flush();
    set_key_vect(1, key_pause);

    display_pacmen(0);
    display_curr_score();
    display_curr_name(curr_name);
    display_registered();

    memset(&hiscore, 0, sizeof(HISCORE));
    get_hiscore(&hiscore);
    disp_hiscore(0, &hiscore);
    f1_instructions();

    if (edit_name_flag == 1) {
        edit_name(curr_name);
        edit_name_flag = 0;
    } else if (comms == 1) {
        for (rank = 0; rank < 6; rank++) {
            if (curr_score > 10 && hiscore.score[rank] < curr_score) {
                need_edit = 1;
            }
        }
        if (need_edit == 1) {
            edit_name(curr_name);
            if (set_hiscore(curr_score, curr_name, pacman) == -1) {
                set_mode(3);
                printf("Cannot open HISCORE.DAT\n");
                _esc = 1;
                set_key_vect(0, NULL);
                return -1;
            }
        }
    }

    hilite_pacman(1, curr_pacman);

    do {
        count++;
        if (count > 10) {
            display_pacmen(offset);
            memset(&hiscore, 0, sizeof(HISCORE));
            get_hiscore(&hiscore);
            disp_hiscore(offset, &hiscore);
            offset ^= 1;
            count = 0;
        }

        if (f1 == 1 || read_key() == 1) {
            if (f1 == 0) {
                kb_event(&inkey, &ext);
            }

            if (inkey == 0 && ext == 0x48 && curr_pacman > 0) {
                hilite_pacman(0, curr_pacman);
                curr_pacman--;
                hilite_pacman(1, curr_pacman);
            }
            if (inkey == 0 && ext == 0x50 && curr_pacman < 9) {
                hilite_pacman(0, curr_pacman);
                curr_pacman++;
                hilite_pacman(1, curr_pacman);
            }
            if (inkey == 0 && ext == 0x47 && curr_pacman > 0) {
                hilite_pacman(0, curr_pacman);
                curr_pacman = 0;
                hilite_pacman(1, 0);
            }
            if (inkey == 0 && ext == 0x4f && curr_pacman < 9) {
                hilite_pacman(0, curr_pacman);
                curr_pacman = 9;
                hilite_pacman(1, 9);
            }
            if (f1 == 0 && inkey == 0 && ext == 0x4d) {
                hilite_pacman(0, curr_pacman);
                edit_name(curr_name);
                hilite_pacman(1, curr_pacman);
            }
            if (f1 == 1 || (inkey == 0 && ext == 0x3b)) {
                f1 = 0;
                display_instructions();
                set_mode(0x13);
                display_pacmen(0);
                display_registered();
                display_curr_score();
                display_curr_name(curr_name);
                memset(&hiscore, 0, sizeof(HISCORE));
                get_hiscore(&hiscore);
                disp_hiscore(offset, &hiscore);
                f1_instructions();
                hilite_pacman(1, curr_pacman);
            }
        }

        display_registered_company();
        delay(0xf);
    } while (_esc == 0 && inkey != 0x1b && inkey != 0xd);

    if (inkey == 0x1b) {
        _esc = 1;
    }
    set_key_vect(0, NULL);
    return curr_pacman + 1;
}

/*
 * Draws every nonzero-score slot of the given high-score table via
 * score_pacmen() above.
 */
void disp_hiscore(int sp_off, HISCORE far * hiscore)
{
    int offset;

    for (offset = 0; offset < 6; offset++) {
        if (hiscore->score[offset] != 0) {
            score_pacmen(offset, sp_off, hiscore);
        }
    }
}

/*
 * Lets the player edit a name string in place via edit_str() (MEDITSTR.C,
 * not yet decompiled), inside a small bordered box. Real signature (just
 * the name buffer) recovered from the call site -- the PACWARS.TXT-
 * derived stub had guessed (int offset, int sp_off, HISCORE far *
 * hiscore), which doesn't match any real call site. Setting the shared
 * esc/f1 latches on Escape/F1 mirrors what the real edit_str() key codes
 * (0x1b, 0x3b) trigger here.
 */
void edit_name(char far * name_str)
{
    unsigned int inkey, ext;
    int key;

    trbox(0x9d, 0x2d, 0x96, 0xe, 0xe);
    /* NOTE: the literal edit-type value 3 doesn't match any of
       MEDITSTR.H's EDIT_TYPE enumerators (pacman_t/name_t/score_t =
       0x490/0x491/0x493) -- passed through as-is pending edit_str()'s own
       decompilation pass, which may reveal the enum needs a 4th value. */
    key = edit_str(&inkey, &ext, 6, 0x1a, name_str, (EDIT_TYPE) 3, 0);
    if (key == 0x1b) {
        _esc = 1;
    }
    trbox(0x9d, 0x2d, 0x96, 0xe, 0);
    if (key == 0x3b) {
        f1 = 1;
    }
}

void display_curr_name(char far * name_str)
{
    text256(0xa0, 0x30, (unsigned char far *) "Name:", 0xb, 0);
    text256(0xd0, 0x30, (unsigned char far *) name_str, 0xf, 0);
}

/*
 * Draws a bordered pop-up box, sized to fit the registered owner's message
 * (reg->reg_mess[0]) when unregistered or a fixed size when registered,
 * centred on screen. Returns the box's outer top-left via x/y (real
 * signature: 4 int far * out-params -- the PACWARS.TXT-derived stub had
 * guessed a single char far * name_str) and the box's inner content
 * position (after the border/margin) back through the same x/y pointers.
 */
void pause_box(int far * x, int far * y, int far * w, int far * h)
{
    if (reg->registered == 1) {
        *w = 0x40;
        *h = 0x28;
    } else {
        *w = ((int) strlen(reg->reg_mess[0]) + 4) * 8;
        *h = 0x48;
    }
    *x = (0x140 - *w) / 2;
    *y = (200 - *h) / 2;

    trfbox(*x, *y, *w, *h, 0x1a);
    trbox(*x + 2, *y + 2, *w - 4, *h - 4, 0xf);

    *x += 0x10;
    *y += 8;
    if (reg->registered != 1) {
        text256(*x, *y, (unsigned char far *) reg->reg_mess[0], 0xb, 0x1a);
    }
    *y += 10;
}

int copy_protect(void)
{
    return 1;
}

/*
 * Real signature recovered from its call sites in MVAGRAPH.C's
 * LoadGraphFont()/SaveGraphFont() (which pass (1, 0, file_name)) --
 * Ghidra's own guess at this function's signature was a bare `void(void)`,
 * contradicted by every call site's raw argument pushes.
 *
 * Confirmed via Ghidra disassembly that this function's body in the
 * shipped binary is genuinely empty -- just the standard prologue, stack
 * overflow check, and epilogue, same situation as memcpyb() above.
 */
void mk_filename(int a, int b, char far * file_name)
{
}

/*
 * Shows a 5-second "please wait" countdown box (used between short play
 * sessions on a network game -- see pacwars()'s comment in MAZE.C),
 * scrolling the registered owner's name (or the shareware nag message,
 * via display_registered_company()) underneath the countdown digit the
 * whole time. Exits early if the player presses Escape while registered.
 */
void pause_time(void)
{
    int text_x, text_y, w, h;
    int box_x, box_y;
    int msg_x, msg_y;
    int count;
    int i, pos;
    char buf[4];
    char letter;

    w = 0x40;
    h = 0x28;
    count = 5;
    pause_box(&text_x, &text_y, &w, &h);

    box_x = (0x140 - w) / 2;
    box_y = (200 - h) / 2;
    msg_x = box_x + (w - 8) / 2;
    msg_y = box_y + (h - 8) / 2;
    if (reg->registered != 1) {
        msg_y += 10;
    }

    do {
        sprintf(buf, "%d", count);
        text256(msg_x, msg_y, (unsigned char far *) buf, 0xf, 0x1a);
        count--;

        for (i = 0; i < 0x2d; i++) {
            if (reg->registered == 1) {
                display_registered_company();
            } else {
                for (pos = 0; pos < (int) strlen(reg->reg_name); pos++) {
                    letter = reg->reg_name[pos];
                    reg_text(text_x, text_y, &letter, pos);
                }
            }
            delay(0xf);
            if (reg->registered == 1 && _esc == 1) {
                return;
            }
        }
    } while (count >= 1);
}

/*
 * Draws one character of a scrolling/colour-cycling text run (used by
 * display_registered_company() above to print the registered owner's
 * name letter-by-letter). Real signature (int x, int y, char far *
 * letter, int pos) recovered from its only call site -- the PACWARS.TXT-
 * derived stub had guessed (int far * x, int far * y, int far * w, int
 * far * h), which doesn't match. On the first character of a run
 * (pos == 0), advances a 0-22 wraparound row counter that seeds the
 * colour for the whole run; the colour itself then cycles 0x22-0x38
 * every character, across calls.
 */
void reg_text(int x, int y, char far * letter, int pos)
{
    static int colour = 0x22;
    static int row_counter;

    if (pos == 0) {
        if (row_counter == 0x16) {
            row_counter = 0;
        } else {
            row_counter++;
        }
        colour = 0x37 - row_counter;
    }

    text256(x + pos * __mva_text_width, y, (unsigned char far *) letter, colour, 0x1a);

    if (colour == 0x38) {
        colour = 0x22;
    } else {
        colour++;
    }
}

/*
 * Draws the token/power-up legend used by display_instructions() above:
 * one sprite + name + key-binding row per token type, in one of two
 * alternating half-screen pages (offset 0 or 1) selected by the caller.
 * Real signature (just the page offset) recovered from the call site --
 * the PACWARS.TXT-derived stub had guessed (int x, int y, char far *
 * letter, int pos), which doesn't match. The two label tables are
 * compile-time string-pointer arrays (F_SCOPY_-copied in the original),
 * recovered here as static initialized arrays; a handful of packey
 * entries have no key binding to show and are empty strings.
 */
void display_tokens(int offset)
{
    static char far * pacname[11] = {
        "Warp", "Invisible Shot", "Beer", "Infra Red Specs", "Smart Bomb",
        "Glue", "Homing Missile", "Shield", "Grenade", "Pot Luck", "Pearl"
    };
    static char far * packey[11] = {
        "W", "I (Space Fire)", "500 Points", "V", "", "",
        "T (Target/Space)", "S (Activate)", "G (Space Fire)", "", "5000 Points"
    };
    int i;
    int sprite_num;
    int sx;

    for (i = 0; i < 11; i++) {
        sprite_num = i * 2 + offset + 0x42c;
        sx = (i == 6) ? 4 : 0;
        _sprites[sprite_num].spritex = sx + 0x11;
        _sprites[sprite_num].spritey = i * 0x11 + 8;
        display_sprite(sprite_num);
        text256(0x30, i * 0x11 + 0xe, (unsigned char far *) pacname[i], 0xf, 0);
        text256(0xb4, i * 0x11 + 0xe, (unsigned char far *) packey[i], 0xe, 0);
    }
}

/*
 * Draws the scoreboard panel's border (occupying the strip from _max_x to
 * _sc_width, the area set_clip_window() carves out for it) plus a grid of
 * small "life" boxes, one per room column x room row of the currently
 * loaded maze.
 */
void draw_scoreboard(void)
{
    int x0, y0;
    int col, row;

    trfbox(_max_x, 0, _sc_width - _max_x, _max_y, 0x35);
    hline(_max_x + 2, 2, _sc_width - _max_x - 4, 0x36);
    vline(_max_x + 2, 2, _max_y - 4, 0x36);
    hline(_max_x + 2, _max_y - 3, _sc_width - _max_x - 4, 0x34);
    vline(_sc_width - 3, 2, _max_y - 4, 0x34);

    x0 = _max_x + 6;
    y0 = _max_y - 4 - _VSIZE * 8;
    for (col = 0; col < _HSIZE; col++) {
        for (row = 0; row < _VSIZE; row++) {
            trbox(x0 + col * 9, y0 + row * 7, 10, 8, 0xf);
        }
    }
}

/*
 * Draws (status != 0) or erases the "currently selected workstation"
 * checkmark icon next to a workstation's row in the scoreboard, widening
 * the clip window briefly since the scoreboard strip normally sits
 * outside the play-area clip set by set_clip_window(1).
 */
void show_selected(int status, int selected)
{
    int x;
    int sprite;
    int num;

    x = _max_x;
    if (selected <= _wstation) {
        selected++;
    }
    _max_x = _sc_width;

    if (status == 1) {
        num = -1;
        sprite = 0x457;
    } else {
        num = 0;
        sprite = 1;
    }
    icon(x + 0xe, selected * 0x18 + 8, sprite, num);
    _max_x = 0xf0;
}

/* forward declaration: fill_scoreboard() (below) calls update_radar(),
   defined further down in this file. */
void update_radar(int offset, MAZE_LOG_STRUCT far * maze_log, int gold_present, int token_present);

/*
 * Draws one row of the scoreboard's per-workstation strip (ship sprite +
 * "last hit by" overlay + power-up icons for the local station + score +
 * lives remaining), or blanks the row if that workstation slot has no
 * connection. For workstation 0 specifically, also draws the shared
 * radar widget and gold/last-hit-man status icons in the scoreboard's
 * bottom corner (see update_radar() below).
 */
void fill_scoreboard(int i, MAZE_LOG_STRUCT far * maze_log, int offset)
{
    char text_str[14];
    int x, y;
    int radar_y;
    int sprite;
    int icon_sprite, icon_num;

    x = _max_x + 6;
    if (i == _wstation) {
        y = 8;
    } else {
        y = (i <= _wstation) ? i + 1 : i;
        y = y * 0x18 + 0x10;
    }

    if (i < 5 && maze_log->connection[i] == '\0') {
        trfbox(x, y, _sc_width - _max_x - 0xc, 0x18, 0x35);
    } else if (i < 5 && maze_log->connection[i] != '\0') {
        if (i == _wstation || maze_log->status[i].invisible < 2) {
            sprite = ((maze_log->status[i].sprite - 4) & 0xfffc) + offset + 4;
        } else {
            sprite = (maze_log->status[i].invisible - 2) % 6 + 0x45f
                     + ((maze_log->status[i].sprite - 4) >> 2) * 0xc;
        }
        _sprites[0x4e0].spritew = _sprites[sprite].spritew;
        _sprites[0x4e0].spriteh = _sprites[sprite].spriteh;
        _sprites[0x4e0].sprite = _sprites[sprite].sprite;
        _max_x = _sc_width;
        _sprites[0x4e0].spritex = x;
        _sprites[0x4e0].spritey = y;
        mix_sprite(0x4e0, 0x35);

        /* "last hit by" ship-silhouette overlay, badge sprite 0x42b sized
           box with the current sprite's bitmap (or blank, sprite 1) */
        if (maze_log->hit_man == i && maze_log->connection[i] != '\0') {
            sprite = 0x42b;
        } else {
            sprite = 1;
        }
        _sprites[0x4e0].spritex = _max_x + 0x16;
        _sprites[0x4e0].spritey = y + 0x10;
        _sprites[0x4e0].spritew = _sprites[0x42b].spritew;
        _sprites[0x4e0].spriteh = _sprites[0x42b].spriteh;
        _sprites[0x4e0].sprite = _sprites[sprite].sprite;
        mix_sprite(0x4e0, 0x35);

        if (i == _wstation) {
            icon_num = (_warp_count > 0) ? _warp_count : 1;
            icon_sprite = (_warp_count > 0) ? 0x454 : 0;
            icon(x + 0xc, y, icon_sprite, icon_num);

            icon_num = (_ishot_count > 0) ? _ishot_count : 1;
            icon_sprite = (_ishot_count > 0) ? 0x455 : 0;
            icon(x + 0x1a, y, icon_sprite, icon_num);

            icon_num = (_visible_count > 0) ? _visible_count : 1;
            icon_sprite = (_visible_count > 0) ? 0x456 : 0;
            icon(x + 0x28, y, icon_sprite, icon_num);

            icon_num = (_grenade_count > 0) ? _grenade_count : 1;
            icon_sprite = (_grenade_count > 0) ? 0x459 : 0;
            icon(x + 0x2e, y - 9, icon_sprite, icon_num);

            icon_num = (_shield_count > 0) ? _shield_count : 1;
            icon_sprite = (_shield_count > 0) ? 0x458 : 0;
            icon(x + 0x36, y, icon_sprite, icon_num);

            icon_num = (_missile_count > 0) ? _missile_count : 1;
            icon_sprite = (_missile_count > 0) ? 0x457 : 0;
            icon(x + 0x44, y, icon_sprite, icon_num);
        }
        _max_x = 0xf0;

        if (maze_log->connection[i] == '\0') {
            text256(x + 0x1a, y + 5, (unsigned char far *) "      ", 0xe, 0x35);
        } else {
            sprintf(text_str, "%6ld", (long) maze_log->score[i] * 10L);
            text256(x + 0x1a, y + 5, (unsigned char far *) text_str, 0xe, 0x35);
        }

        if (maze_log->connection[i] == '\0') {
            text256(x + 0x3a, y + 0xf, (unsigned char far *) "  ", 0xf, 0x35);
        } else {
            sprintf(text_str, "%2d", maze_log->men[i]);
            text256(x + 0x3a, y + 0xf, (unsigned char far *) text_str, 0xf, 0x35);
        }
    }

    if (i == 0) {
        /* gold status icon + percentage, top-right corner */
        sprite = (maze_log->gold.present == 1) ? (offset + 0x429) : 1;
        _sprites[0x4e0].spritex = _sc_width - 0x16;
        _sprites[0x4e0].spritey = _max_y - 0x22;
        _sprites[0x4e0].spritew = _sprites[0x429 + offset].spritew;
        _sprites[0x4e0].spriteh = _sprites[0x429 + offset].spriteh;
        _sprites[0x4e0].sprite = _sprites[sprite].sprite;
        _max_x = _sc_width;
        mix_sprite(0x4e0, 0x35);
        _max_x = 0xf0;

        if (maze_log->gold.present == 1) {
            sprintf(text_str, "%4d", maze_log->gold.score * 100);
            text256(_sc_width - 0x24, _max_y - 0xe, (unsigned char far *) text_str, 0xe, 0x35);
        } else {
            text256(_sc_width - 0x24, _max_y - 0xe, (unsigned char far *) "    ", 0xe, 0x35);
        }

        update_radar(offset, maze_log, maze_log->gold.present, maze_log->token.present);

        /* last-hit-man's ship silhouette + hit-score percentage, below
           the radar widget */
        x = _max_x + 6;
        radar_y = _max_y - _HSIZE * 10;
        y = radar_y - 0x18;
        if (maze_log->hit_man < 0) {
            sprite = 1;
        } else {
            sprite = ((maze_log->status[(int) maze_log->hit_man].sprite - 4) & 0xfffc) + offset + 4;
        }
        _sprites[0x4e0].spritew = _sprites[4].spritew;
        _sprites[0x4e0].spriteh = _sprites[4].spriteh;
        _sprites[0x4e0].sprite = _sprites[sprite].sprite;
        _max_x = _sc_width;
        _sprites[0x4e0].spritex = x;
        _sprites[0x4e0].spritey = y;
        mix_sprite(0x4e0, 0x35);
        _max_x = 0xf0;

        if (maze_log->hit_man < 0) {
            text256(_sc_width - 0x24, radar_y - 0x10, (unsigned char far *) "    ", 0xe, 0x35);
        } else {
            sprintf(text_str, "%4d", maze_log->hit_score * 100);
            text256(_sc_width - 0x24, radar_y - 0x10, (unsigned char far *) text_str, 0xe, 0x35);
        }
    }
}

/*
 * Draws the gold and token blips on the radar widget at their real maze
 * position (converted to 6x4-pixel grid cells), using a plain colour
 * normally and a special "blinking" colour from a 2-entry table (indexed
 * by the caller's page-flip offset) while that item's presence flag is
 * actively toggling (gold_present == 1 / token_present == 1).
 */
void update_radar(int offset, MAZE_LOG_STRUCT far * maze_log, int gold_present, int token_present)
{
    static char gold_blink_colour[2] = { 0x0e, 0x2b };
    static char token_blink_colour[2] = { 0x0f, 0x1c };
    int x0, y0;

    x0 = _max_x + 8;
    y0 = _max_y - _VSIZE * 8 - 2;

    if (gold_present == 0) {
        trfbox(x0 + maze_log->gold.hoffset * 9, y0 + maze_log->gold.voffset * 7, 6, 4, 0x35);
    }
    if (token_present == 0) {
        trfbox(x0 + maze_log->token.hoffset * 9, y0 + maze_log->token.voffset * 7, 6, 4, 0x35);
    }
    if (gold_present == 1) {
        trfbox(x0 + maze_log->gold.hoffset * 9, y0 + maze_log->gold.voffset * 7, 6, 4,
               gold_blink_colour[offset]);
    }
    if (token_present == 1) {
        trfbox(x0 + maze_log->token.hoffset * 9, y0 + maze_log->token.voffset * 7, 6, 4,
               token_blink_colour[offset]);
    }
}

void f1_instructions(void)
{
    text256(0x96, 0xbe, (unsigned char far *) "F1 Inventory", 0xe, 0);
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
    sound(freq);
    delay(wait_length);
    nosound();
}

/*
 * Real signature recovered from its call site in MVAGRAPH.C's
 * copy_screen() (a far-pointer-to-far-pointer byte copy, intended for use
 * under VGA write mode 1, where a read latches all 4 planes so a plain
 * byte-at-a-time copy would transfer whole pixels).
 *
 * Confirmed via Ghidra disassembly that this function's body in the
 * shipped binary is genuinely empty -- just the standard prologue, stack
 * overflow check, and epilogue, no actual copy loop. This isn't a gap in
 * the decompilation; copy_screen() (and by extension GraphBoxScroll()'s
 * scrolling box fill/erase in MVAGRAPH.C, its only caller) really did
 * ship with a no-op copy in PACWARS.EXE. Left empty here to match.
 */
void memcpyb(unsigned char far * dest, unsigned char far * source, int width)
{
}

