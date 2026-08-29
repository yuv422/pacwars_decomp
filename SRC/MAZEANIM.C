/*
 * MAZEANIM.C
 *
 * Reconstructed from PACWARS.EXE via Ghidra decompilation/disassembly of
 * segment 1a54 (real addresses 1a54:0005-08dc), cross-checked instruction
 * by instruction against the raw disassembly for every function in this
 * file given how much this module diverged from its own stub prototypes
 * (see MAZEANIM.H's file banner and the clear_room_frame/create_anim_sprite
 * comment there).
 *
 * This is a second, richer, per-room animated-cell subsystem alongside
 * MAZEUTIL.C's simpler update_room()/MAZESPT.C's animate_room()/
 * clear_animates() family -- both exist as genuinely separate compiled
 * code operating on differently-sized structs (see MAZEANIM.H). This
 * module's entry points (update_map_animation/clear_room_animation) are
 * called directly from MAZE.C's main_loop(), alongside (not instead of)
 * MAZEUTIL.C's update_map().
 *
 * Every "obj[i].hoffset != -1" loop below walks a per-room array of
 * ANIM_OBJECT records (see MAZEANIM.H) until a terminator entry.
 */
#include "MAZEANIM.H"
#include "MAZEUTIL.H"
#include "MAZESPT.H"
#include <alloc.h>

/*
 * Like MAZEUTIL.C's _animate_maze, these pointers are NOT runtime-
 * allocated -- each _animate_maze_rooms[voff][hoff] slot and every
 * _animate_buffer[] entry points directly at statically-compiled data
 * baked into the original .EXE's data segment (confirmed via
 * read_memory: the ANIM_OBJECT records occupy 340e:0f51-10e7,
 * immediately followed by the _animate_maze_rooms pointer array itself
 * at 340e:10f5; the ANIM_SEQUENCE records occupy 340e:0b00-0b51,
 * immediately followed by the _animate_buffer pointer array at
 * 340e:0b51). Recovered byte-for-byte below.
 *
 * _animate_buffer was previously sized `[1]` (its real populating site
 * hadn't been identified yet) -- that undersizing was a live bug once
 * real data is involved: the ANIM_OBJECT records below use `animate`
 * index values from 0 to 8, so alloc_room_animation()'s
 * `_animate_buffer[obj[i].animate]` would read out of bounds for
 * anything but index 0. Corrected to the real 9-entry table.
 *
 * Each ANIM_SEQUENCE's `frame` pointer (and every ANIM_FRAME's own
 * `element_array`) is now recovered too, two more struct levels deep --
 * ANIM_FRAME[frames], each with its own ANIM_ELEMENT far*-sized
 * element_array -- via read_memory across 340e:0638-0aff (the element
 * records) and 340e:0770-0b00 (the frame records), traversed in Ghidra
 * by typing ANIM_SEQUENCE -> ANIM_FRAME[nFrames] -> ANIM_ELEMENT[n] at
 * each real pointer target and reading the resulting struct data back
 * out. Every static array/variable name below matching the pattern
 * `_zaph`, `_cdrip`, `_torch1`, etc. is Ghidra's own label for that
 * exact real address (see 340e:0638-0b51 in the Ghidra listing) --
 * used verbatim as the C identifier, per this whole project's naming
 * convention.
 *
 * Frames whose real data has `num_elements == 0` never dereference
 * `element_array` (see update_frame()/room_frame()/clear_room_frame()'s
 * `for (i = 0; i < frame->num_elements; i++)` loops below), so those
 * are written as `{ 0, 0 }` here for clarity. The original binary
 * sometimes left a stale non-NULL pointer in that field for such
 * frames (e.g. _chad's frames 1-14 all point back at 340e:0710, frame
 * 0's own element) -- confirmed harmless since it's never read, and not
 * reproduced here since it has no behavioral effect.
 */
static ANIM_OBJECT anim_obj_empty[] = {
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OBJECT anim_obj_room_0_0[] = {
    { 0, 0, 16, 19, 0, 0, 1, 0, 0 },
    { 0, 0, 20, 23, 0, 0, 1, 0, 0 },
    { 0, 0, 16, 26, 4, 0, 1, 0, 0 },
    { 0, 0, 17, 22, 4, 1, 1, 0, 0 },
    { 0, 0, 11, 29, 4, 1, 1, 0, 0 },
    { 0, 0, 17, 29, 0, 1, 1, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OBJECT anim_obj_room_0_1[] = {
    { 1, 0, 11, 0, 4, 1, 1, 0, 0 },
    { 1, 0, 17, 0, 0, 1, 1, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OBJECT anim_obj_room_0_2[] = {
    { 3, 1, 8, 23, 0, 3, 0, 0, 0 },
    { 3, 1, 18, 15, 0, 4, 0, 0, 0 },
    { 3, 1, 18, 16, 0, 5, 0, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OBJECT anim_obj_room_1_1[] = {
    { 3, 1, 11, 13, 0, 4, 0, 0, 0 },
    { 3, 1, 11, 14, 0, 5, 0, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static ANIM_OBJECT anim_obj_room_1_2[] = {
    { 3, 2, 11, 4, 6, 2, 1, 0, 0 },
    { 3, 2, 18, 8, 3, 2, 1, 0, 0 },
    { 3, 2, 16, 14, 0, 2, 1, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * CORRECTED naming: this is the torch_flicker-only room (4 entries, all
 * animate==6). Previously named anim_obj_room_2_0 and placed at
 * _animate_maze_rooms[2][0] -- confirmed via read_memory that its real
 * address (340e:1077) is actually the [2][1] slot's pointer target, not
 * [2][0]'s (340e:10bd, which holds the drip/blink room below instead).
 * The two arrays' table placement had been swapped; renamed/reordered to
 * match the real memory layout rather than just swapping the table
 * entries, so the array names stay truthful. This was the root cause of
 * the reported "acid drip and blink animation frames are displayed on
 * the wrong room" bug.
 */
static ANIM_OBJECT anim_obj_room_2_1[] = {
    { 3, 1, 13, 1, 0, 6, 1, 0, 0 },
    { 3, 1, 4, 10, 1, 6, 1, 0, 0 },
    { 3, 1, 4, 20, 0, 6, 1, 0, 0 },
    { 3, 1, 17, 28, 1, 6, 1, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * CORRECTED naming: the chemical_drip + chad_chalk ("blink") room.
 * Previously named anim_obj_room_2_1 and placed at
 * _animate_maze_rooms[2][1] -- its real address (340e:10bd) is actually
 * the [2][0] slot's pointer target. See the comment above.
 */
static ANIM_OBJECT anim_obj_room_2_0[] = {
    { 3, 1, 2, 6, 0, 7, 1, 0, 0 },
    { 3, 1, 21, 19, 0, 8, 1, 0, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* storage for the _animate_maze_rooms global declared extern in
   MAZEANIM.H, indexed [voff][hoff] -- see the comment above. */
ANIM_OBJECT far * _animate_maze_rooms[4][3] = {
    { anim_obj_room_0_0, anim_obj_room_0_1, anim_obj_room_0_2 },
    { anim_obj_empty,    anim_obj_room_1_1, anim_obj_room_1_2 },
    { anim_obj_room_2_0, anim_obj_room_2_1, anim_obj_empty    },
    { anim_obj_empty,    anim_obj_empty,    anim_obj_empty    }
};

/* --- zap_horiz (_zap_horiz, 340e:0b00, pFrame -> _zaph @ 340e:0770) ---
 * Elements confirmed via read_memory at 340e:0638-0668. */
static ANIM_ELEMENT _zaph1[3] = { { 0, 0, 12, 0 }, { 0, 1, 13, 0 }, { 0, 2, 12, 0 } };
static ANIM_ELEMENT _zaph2[3] = { { 0, 0, 13, 0 }, { 0, 1, 12, 0 }, { 0, 2, 13, 0 } };
static ANIM_ELEMENT _zaph3[3] = { { 0, 0,  0, 0 }, { 0, 1,  0, 0 }, { 0, 2,  0, 0 } };

static ANIM_FRAME _zaph[8] = {
    { 3, _zaph1 },
    { 3, _zaph2 },
    { 3, _zaph1 },
    { 3, _zaph2 },
    { 3, _zaph3 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 }
};

/* --- zap_vert (_zap_vert, 340e:0b09, pFrame -> _zapv @ 340e:07a0) ---
 * Elements confirmed via read_memory at 340e:066e-069e. Frame 4's
 * elements are real (num_elements == 3), just all block == 0 -- an
 * explicit "blank tile" draw state, distinct from frames 5-7 which
 * skip drawing entirely (num_elements == 0). */
static ANIM_ELEMENT _zapv1[3] = { { 0, 0, 14, 0 }, { 1, 0, 15, 0 }, { 2, 0, 14, 0 } };
static ANIM_ELEMENT _zapv2[3] = { { 0, 0, 15, 0 }, { 1, 0, 14, 0 }, { 2, 0, 15, 0 } };
static ANIM_ELEMENT _zapv3[3] = { { 0, 0,  0, 0 }, { 1, 0,  0, 0 }, { 2, 0,  0, 0 } };

static ANIM_FRAME _zapv[8] = {
    { 3, _zapv1 },
    { 3, _zapv2 },
    { 3, _zapv1 },
    { 3, _zapv2 },
    { 3, _zapv3 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 }
};

/* --- pipe_vert (_pipe_vert, 340e:0b12, pFrame -> _pipe @ 340e:07d0) ---
 * Elements confirmed via read_memory at 340e:06a4-06ce. */
static ANIM_ELEMENT _pipe1[4] = { { 0, 0, 31, 0 }, { 1, 0, 0, 0 }, { 2, 0, 0, 0 }, { 3, 0, 35, 0 } };
static ANIM_ELEMENT _pipe2[4] = { { 0, 0, 30, 0 }, { 1, 0, 29, 0 }, { 2, 0, 33, 0 }, { 3, 0, 34, 0 } };

static ANIM_FRAME _pipe[8] = {
    { 4, _pipe1 },
    { 4, _pipe1 },
    { 4, _pipe1 },
    { 4, _pipe2 },
    { 4, _pipe2 },
    { 4, _pipe2 },
    { 4, _pipe2 },
    { 4, _pipe2 }
};

/* --- snake_seq (_snake_seq, 340e:0b1b, pFrame -> _snake_tongue @ 340e:0800) ---
 * Elements confirmed via read_memory at 340e:06ec-06f2. */
static ANIM_ELEMENT _snake1[1] = { { 0, 0, 20, 0 } };
static ANIM_ELEMENT _snake2[1] = { { 0, 0, 26, 0 } };

static ANIM_FRAME _snake_tongue[2] = {
    { 1, _snake1 },
    { 1, _snake2 }
};

/* --- plant_teeth (_plant_teeth, 340e:0b24, pFrame -> _plantt @ 340e:080c) ---
 * Elements confirmed via read_memory at 340e:06d4-06da. */
static ANIM_ELEMENT _planta[1] = { { 0, 0, 40, 0 } };
static ANIM_ELEMENT _plantb[1] = { { 0, 0, 52, 0 } };

static ANIM_FRAME _plantt[2] = {
    { 1, _planta },
    { 1, _plantb }
};

/* --- plant_leaf (_plant_leaf, 340e:0b2d, pFrame -> _plantl @ 340e:0818) ---
 * Elements confirmed via read_memory at 340e:06e0-06e6. */
static ANIM_ELEMENT _plantc[1] = { { 0, 0, 41, 0 } };
static ANIM_ELEMENT _plantd[1] = { { 0, 0, 53, 0 } };

static ANIM_FRAME _plantl[2] = {
    { 1, _plantc },
    { 1, _plantd }
};

/* --- torch_flicker (_torch_flicker, 340e:0b36, pFrame -> _torch_flick @ 340e:0824) ---
 * Elements confirmed via read_memory at 340e:06f8-070a. Note the
 * attribute field is nonzero here (1) unlike every sequence above --
 * a real color/attribute change accompanying the flicker. */
static ANIM_ELEMENT _torch1[1] = { { 0, 0,  57, 1 } };
static ANIM_ELEMENT _torch2[1] = { { 0, 0, 299, 1 } };
static ANIM_ELEMENT _torch3[1] = { { 0, 0,  59, 1 } };
static ANIM_ELEMENT _torch4[1] = { { 0, 0, 319, 1 } };

static ANIM_FRAME _torch_flick[4] = {
    { 1, _torch1 },
    { 1, _torch2 },
    { 1, _torch3 },
    { 1, _torch4 }
};

/* --- chemical_drip (_chemical_drip, 340e:0b3f, pFrame -> _drip @ 340e:083c) ---
 * 102 frames, of which only 14 carry a real element (num_elements == 1);
 * every other frame is a genuine `{ 0, 0 }` no-draw tick -- an
 * intermittent drip effect, not a rendering gap. The 14 real elements
 * are contiguous in the original data segment (340e:071c-076f, one
 * ANIM_ELEMENT apart) under the single Ghidra label _cdrip; confirmed
 * via read_memory across both halves of the 612-byte frame table
 * (340e:083c len 300, 340e:0968 len 312). */
static ANIM_ELEMENT _cdrip[14] = {
    { 0, 0, 252, 2 }, { 0, 0, 232, 2 }, { 0, 0, 233, 2 }, { 0, 0, 234, 2 },
    { 0, 0, 235, 2 }, { 0, 0, 236, 2 }, { 0, 0, 237, 2 }, { 0, 0, 257, 2 },
    { 0, 0, 277, 2 }, { 0, 0, 297, 2 }, { 0, 0, 317, 2 }, { 0, 0, 337, 2 },
    { 0, 0, 357, 2 }, { 0, 0, 377, 2 }
};

static ANIM_FRAME _drip[102] = {
    { 1, &_cdrip[0] },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, &_cdrip[1] },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, &_cdrip[2] },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, &_cdrip[3] },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, &_cdrip[4] },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, &_cdrip[5] },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, &_cdrip[6] },
    { 1, &_cdrip[7] },
    { 1, &_cdrip[8] },
    { 1, &_cdrip[9] },
    { 1, &_cdrip[10] },
    { 1, &_cdrip[11] },
    { 1, &_cdrip[12] },
    { 1, &_cdrip[13] }
};

/* --- chad_chalk (_chad_chalk, 340e:0b48, pFrame -> _chad @ 340e:0aa0) ---
 * Elements confirmed via read_memory at 340e:0710/0716 (16-frame table
 * re-read and decoded fresh this pass: frames 1-14 are all `{ 0, 0 }`
 * in the source's data too, since they carry num_elements == 0 in the
 * original -- see the header comment above on why the original
 * binary's stale non-NULL pointer there isn't reproduced). */
static ANIM_ELEMENT _chad_el1[1] = { { 0, 0, 356, 2 } };
static ANIM_ELEMENT _chad_el2[1] = { { 0, 0, 316, 2 } };

static ANIM_FRAME _chad[16] = {
    { 1, _chad_el1 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 1, _chad_el2 }
};

static ANIM_SEQUENCE _zap_horiz     = { 8,   32, 3, 1, _zaph };
static ANIM_SEQUENCE _zap_vert      = { 8,   32, 1, 3, _zapv };
static ANIM_SEQUENCE _pipe_vert     = { 8,   32, 1, 4, _pipe };
static ANIM_SEQUENCE _snake_seq     = { 2,   32, 1, 1, _snake_tongue };
static ANIM_SEQUENCE _plant_teeth   = { 2,   32, 1, 1, _plantt };
static ANIM_SEQUENCE _plant_leaf    = { 2,   32, 1, 1, _plantl };
static ANIM_SEQUENCE _torch_flicker = { 4,   16, 1, 1, _torch_flick };
static ANIM_SEQUENCE _chemical_drip = { 102,  2, 1, 1, _drip };
static ANIM_SEQUENCE _chad_chalk    = { 16,  64, 1, 1, _chad };

/* storage for the _animate_buffer global declared extern in
   MAZEANIM.H. Names match Ghidra's own labels for these exact
   addresses (340e:0b00-0b51) verbatim, per this project's naming
   convention. */
ANIM_SEQUENCE far * _animate_buffer[9] = {
    &_zap_horiz, &_zap_vert, &_pipe_vert,
    &_snake_seq, &_plant_teeth, &_plant_leaf,
    &_torch_flicker, &_chemical_drip, &_chad_chalk
};

/*
 * Allocates every room's per-object animation scratch buffer
 * (obj[i].prev_frame_buffer, sized to that object's animation sequence's
 * frame dimensions) across the whole room grid. Reconstructed from
 * 1a54:0005-004e.
 */
void alloc_map_animation(void)
{
    int voff, hoff;

    for (voff = 0; voff < _VSIZE; voff++) {
        for (hoff = 0; hoff < _HSIZE; hoff++) {
            alloc_room_animation(_animate_maze_rooms[voff][hoff]);
        }
    }
}

/*
 * For every animated-cell entry in one room's object list, looks up its
 * animation sequence (via the `animate` index into _animate_buffer[]) and
 * allocates a zeroed scratch buffer sized for that sequence's frame
 * dimensions (w*2*h bytes -- the *2 confirmed via disassembly at
 * 1a54:0091, not otherwise explained by anything in ANIM_SEQUENCE's own
 * fields). Reconstructed from 1a54:004f-00d6.
 */
void alloc_room_animation(ANIM_OBJECT far * obj)
{
    int i;
    ANIM_SEQUENCE far * seq;
    long size;

    for (i = 0; obj[i].hoffset != -1; i++) {
        seq = _animate_buffer[obj[i].animate];
        size = (long) (seq->w << 1) * seq->h;
        obj[i].prev_frame_buffer = (int far *) calloc(1, (size_t) size);
    }
}

/*
 * Refreshes every room's animated maze-cell/attribute values (as opposed
 * to room_animation_func()'s sprite-based visual refresh below) for the
 * given sync tick. Reconstructed from 1a54:00d7-013b.
 */
void update_map_animation(unsigned int sync)
{
    int voff, hoff;

    for (voff = 0; voff < _VSIZE; voff++) {
        for (hoff = 0; hoff < _HSIZE; hoff++) {
            update_room_animation(sync, _animate_maze_rooms[voff][hoff],
                                  (MAZE_STRUCT far *) maze_def(hoff, voff),
                                  (MAZE_STRUCT far *) attrib_maze_def(hoff, voff));
        }
    }
}

/*
 * Steps every animated-cell entry in one room's object list to whichever
 * frame its own phase offset and the global sync tick select, and writes
 * that frame's element values directly into the room's live maze/
 * attribute cell arrays (see update_frame() below) -- unconditionally,
 * unlike room_animation_func()'s change-detected sprite refresh.
 * Reconstructed from 1a54:013c-0243.
 */
void update_room_animation(unsigned int sync, ANIM_OBJECT far * obj, MAZE_STRUCT far * maze_ptr, MAZE_STRUCT far * amaze_ptr)
{
    int i;
    ANIM_SEQUENCE far * seq;
    int frame_index, next_state;

    for (i = 0; obj[i].hoffset != -1; i++) {
        seq = _animate_buffer[obj[i].animate];
        frame_index = (sync % (seq->rate * seq->frames)) / seq->rate;
        next_state = (frame_index + obj[i].offset) % seq->frames;
        update_frame(maze_ptr, amaze_ptr, obj[i].row, obj[i].col,
                    &seq->frame[next_state], obj[i].prev_frame_buffer, seq->w);
    }
}

/*
 * Applies one animation frame's elements to the live maze: for each
 * element, saves the maze cell's current tile value into prev_frame
 * (flattened [row_off*ow + col_off], so clear_room_frame()/
 * clear_room_animation_func() can later tell what changed) before
 * overwriting it with the element's own tile (`block`) and attribute
 * values. Reconstructed from 1a54:0244-0353.
 */
void update_frame(MAZE_STRUCT far * maze_ptr, MAZE_STRUCT far * amaze_ptr, int row, int col, ANIM_FRAME far * frame, int far * prev_frame, int ow)
{
    int i;
    ANIM_ELEMENT far * e;
    int r, c;

    for (i = 0; i < frame->num_elements; i++) {
        e = &frame->element_array[i];
        r = row + e->row_off;
        c = col + e->col_off;
        prev_frame[e->row_off * ow + e->col_off] = maze_ptr->def[r][c];
        maze_ptr->def[r][c] = (unsigned int) e->block;
        amaze_ptr->def[r][c] = (unsigned int) e->attribute;
    }
}

/*
 * Refreshes every room's animated-object sprites for the given sync tick
 * (the sprite-visible counterpart to update_map_animation() above).
 * Reconstructed from 1a54:0354-039c.
 */
void room_animation(unsigned int sync, int hoff, int voff)
{
    room_animation_func(sync, _animate_maze_rooms[voff][hoff], (MAZE_STRUCT far *) maze_def(hoff, voff));
}

/*
 * For every animated-cell entry in one room's object list, selects the
 * sequence frame the current sync tick calls for and, if it differs from
 * the entry's cached last_frame, draws it via room_frame() and updates
 * the cache. Reconstructed from 1a54:039d-04c0.
 */
void room_animation_func(unsigned int sync, ANIM_OBJECT far * obj, MAZE_STRUCT far * maze_ptr)
{
    int i;
    ANIM_SEQUENCE far * seq;
    int frame_index, next_state;

    for (i = 0; obj[i].hoffset != -1; i++) {
        seq = _animate_buffer[obj[i].animate];
        frame_index = (sync % (seq->rate * seq->frames)) / seq->rate;
        next_state = (frame_index + obj[i].offset) % seq->frames;
        if (obj[i].last_frame != next_state) {
            room_frame(maze_ptr, obj[i].row, obj[i].col, obj[i].redraw, &seq->frame[next_state]);
            obj[i].last_frame = next_state;
        }
    }
}

/*
 * Draws one animation frame's elements as sprites: for each element,
 * picks sprite slot 1 (a blank/clear sprite) if the underlying maze cell
 * is currently empty, otherwise slot (element's own block value + 0x40)
 * -- confirmed via disassembly (1a54:053b-0545) that the non-blank case
 * re-reads the element record itself, not the maze cell a second time --
 * sizes/positions it from the reference block sprite at slot 0x41, then
 * either or_sprite()s it in (redraw==1, additive/masked blit) or
 * display_sprite()s it (straight blit). Reconstructed from 1a54:04c1-05bb.
 */
void room_frame(MAZE_STRUCT far * maze_ptr, int srow, int scol, int redraw, ANIM_FRAME far * frame)
{
    int i;
    int row, col;
    int sprite_num;
    unsigned char ref_w;
    unsigned int ref_h;
    ANIM_ELEMENT far * e;

    ref_w = _sprites[0x41].spritew;
    ref_h = _sprites[0x41].spriteh;

    for (i = 0; i < frame->num_elements; i++) {
        e = &frame->element_array[i];
        row = srow + e->row_off;
        col = scol + e->col_off;

        if (maze_ptr->def[row][col] == 0) {
            sprite_num = 1;
        } else {
            /* CORRECTED (1a54:053b-0545): the real binary reloads the
               element pointer and reads e->block here, NOT a second
               maze_ptr->def[row][col] lookup. The def[] check just above
               only tests "is this cell blank" -- the actual sprite index
               comes from the element's own stored value. Reading def[]
               again is observably different when another animated
               object's update_frame() has since overwritten this same
               cell (e.g. two overlapping gate elements), which produced
               the reported "duplicated gate animation" symptom. */
            sprite_num = e->block + 0x40;
        }

        _sprites[sprite_num].spritey = row * ref_h;
        _sprites[sprite_num].spritex = col * ref_w;
        _sprites[sprite_num].spritew = ref_w;
        _sprites[sprite_num].spriteh = ref_h;

        if (redraw == 1) {
            or_sprite(sprite_num);
        } else {
            display_sprite(sprite_num);
        }
    }
}

/*
 * Tears down every room's animated-object sprites for the given sync
 * tick (the clearing counterpart to room_animation() above). Reconstructed
 * from 1a54:05bc-0604.
 */
void clear_room_animation(unsigned int sync, int hoff, int voff)
{
    clear_room_animation_func(sync, _animate_maze_rooms[voff][hoff], (MAZE_STRUCT far *) maze_def(hoff, voff));
}

/*
 * For every animated-cell entry in one room's object list whose redraw
 * flag is set and whose selected frame differs from its cached
 * last_frame, erases it via clear_room_frame() below (note: unlike
 * room_animation_func(), this does NOT update the last_frame cache
 * afterward -- matching clear_room_frame()'s role of tearing an
 * animation down rather than stepping it forward). Reconstructed from
 * 1a54:0605-0735.
 */
void clear_room_animation_func(unsigned int sync, ANIM_OBJECT far * obj, MAZE_STRUCT far * maze_ptr)
{
    int i;
    ANIM_SEQUENCE far * seq;
    int frame_index, next_state;

    for (i = 0; obj[i].hoffset != -1; i++) {
        seq = _animate_buffer[obj[i].animate];
        frame_index = (sync % (seq->rate * seq->frames)) / seq->rate;
        next_state = (frame_index + obj[i].offset) % seq->frames;
        if (obj[i].redraw != 0 && obj[i].last_frame != next_state) {
            clear_room_frame(maze_ptr, obj[i].row, obj[i].col, &seq->frame[next_state],
                             obj[i].prev_frame_buffer, seq->w);
        }
    }
}

/*
 * Erases one animation frame's elements: for each element whose maze
 * cell has changed since prev_frame was captured (by update_frame()
 * above), either display_sprite()s a blank tile (if the cell is now
 * empty) or composites a "what's left after removing this animation's
 * contribution" bitmap via create_anim_sprite() into a 64-byte scratch
 * buffer and mask_sprite()s that in. Reconstructed from 1a54:0736-08dc
 * (the local scratch buffer's true size -- 64 bytes -- was confirmed via
 * the function's stack frame layout, SUB SP,0x4e down to the buffer's own
 * BP-0x4e address leaving exactly 0x40 bytes before the next local; the
 * decompiler's own type inference had under-sized it to 40).
 */
void clear_room_frame(MAZE_STRUCT far * maze_ptr, int srow, int scol, ANIM_FRAME far * frame, int far * prev_frame, int ow)
{
    int i;
    int row, col;
    int sprite_num, prev_sprite;
    unsigned char ref_w;
    unsigned int ref_h;
    ANIM_ELEMENT far * e;
    unsigned char sp_buffer[64];

    ref_w = _sprites[0x41].spritew;
    ref_h = _sprites[0x41].spriteh;

    for (i = 0; i < frame->num_elements; i++) {
        e = &frame->element_array[i];
        row = srow + e->row_off;
        col = scol + e->col_off;

        if (maze_ptr->def[row][col] != (unsigned int) prev_frame[e->row_off * ow + e->col_off]) {
            if (maze_ptr->def[row][col] == 0) {
                sprite_num = 1;
            } else {
                prev_sprite = prev_frame[e->row_off * ow + e->col_off] + 0x40;
                sprite_num = 0x4e0;
                _sprites[0x4e0].sprite = (unsigned char far *) sp_buffer;
            }

            _sprites[sprite_num].spritey = row * ref_h;
            _sprites[sprite_num].spritex = col * ref_w;
            _sprites[sprite_num].spritew = ref_w;
            _sprites[sprite_num].spriteh = ref_h;

            if (maze_ptr->def[row][col] == 0) {
                display_sprite(sprite_num);
            } else {
                create_anim_sprite(prev_sprite, maze_ptr->def[row][col] + 0x40, (unsigned char far *) sp_buffer);
                mask_sprite(sprite_num);
            }
        }
    }
}

/*
 * Builds a "surviving pixels" composite bitmap into sp_buff: for each of
 * the 64 bytes of an 8x8 block sprite, copies the previous sprite's byte
 * through wherever the next sprite's byte is 0 (transparent), else writes
 * 0 -- used by clear_room_frame() above to erase only the parts of a
 * previous animation frame's sprite that the next frame won't redraw
 * over anyway. Reconstructed from 1a54:08dd-0932.
 */
void create_anim_sprite(int prev_sprite, int next_sprite, unsigned char far * sp_buff)
{
    int i;

    for (i = 0; i < 0x40; i++) {
        if (_sprites[next_sprite].sprite[i] == 0) {
            sp_buff[i] = _sprites[prev_sprite].sprite[i];
        } else {
            sp_buff[i] = 0;
        }
    }
}
