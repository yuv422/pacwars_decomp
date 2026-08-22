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

/* storage for _animate_maze_rooms/_animate_buffer, declared extern in
   MAZEANIM.H -- this is the first (and so far only) module needing them.
   _animate_buffer's real populating site wasn't identified in this pass
   (see MAZEANIM.H's comment); it's presumably filled in by a not-yet-
   decompiled loader elsewhere, so no initializer is given here. */
ANIM_OBJECT far * _animate_maze_rooms[4][3];
ANIM_SEQUENCE far * _animate_buffer[1];

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
 * is empty, otherwise slot (maze_cell_value + 0x40), sizes/positions it
 * from the reference block sprite at slot 0x41, then either or_sprite()s
 * it in (redraw==1, additive/masked blit) or display_sprite()s it
 * (straight blit). Reconstructed from 1a54:04c1-05bb.
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
            sprite_num = maze_ptr->def[row][col] + 0x40;
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
