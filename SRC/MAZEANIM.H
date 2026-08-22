/*
 * MAZEANIM.H
 *
 * Public prototypes exported by MAZEANIM.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT,
 * corrected against Ghidra decompilation/disassembly of segment 1a54
 * (real addresses 1a54:0005-08dc) -- see MAZEANIM.C's file banner for
 * details on the several signature corrections this module needed.
 */
#ifndef MAZEANIM_H
#define MAZEANIM_H

#include "PACWARS.H"

/*
 * A room's animated-cell record for THIS module's richer, multi-element
 * sequence-based animation system -- distinct from (and not to be
 * confused with) PACWARS.H's ANIM_OB, which is a different, 11-byte
 * struct used by MAZEUTIL.C's update_room()/MAZESPT.C's animate_room()/
 * clear_animates() for a simpler single-byte-frame-table cycling scheme.
 * Both happen to share a conceptually similar purpose (per-room animated
 * maze cells) but are genuinely different compiled structs at different
 * call sites (confirmed via disassembly: update_room() iterates with an
 * 11-byte/0xb stride, MAZEANIM.C's functions all use a 14-byte/0xe
 * stride) -- reconstructed here as get_struct_layout(ANIM_OB) reported
 * it (that name in Ghidra's own struct DB corresponds to this 14-byte
 * version, not PACWARS.H's existing 11-byte ANIM_OB), under a new name
 * to avoid colliding with the already-verified ANIM_OB usage elsewhere.
 */
typedef struct ANIM_OBJECT_tag {
    char hoffset;                   /* offset 0; -1 marks end of a room's object list */
    char voffset;                   /* offset 1 */
    char row;                       /* offset 2; object's row within the room */
    char col;                       /* offset 3; object's col within the room */
    int offset;                     /* offset 4; phase offset into the animation sequence */
    char animate;                   /* offset 6; index into _animate_buffer[] */
    char redraw;                    /* offset 7; passed straight through to room_frame()'s redraw param */
    int last_frame;                 /* offset 8; cache of the last-selected sequence frame index */
    int far * prev_frame_buffer;    /* offset 10; per-object scratch buffer, alloc'd by alloc_room_animation() */
} ANIM_OBJECT;

/*
 * Per-room arrays of ANIM_OBJECT records (obj[i].hoffset==-1 terminated),
 * indexed [voff][hoff] (same convention as PACWARS.H's _animate_maze).
 * Real address 340e:10f5 (offset half; Ghidra only labeled that half,
 * the segment half -- 340e:10f7 -- showed up unlabeled as DAT_340e_10f7,
 * the same "only one half of a far-pointer array got a real symbol"
 * pattern documented elsewhere in this project). No xref to this data
 * was found from any module decompiled so far other than MAZEANIM.C
 * itself, so declared here rather than in the shared PACWARS.H.
 */
extern ANIM_OBJECT far * _animate_maze_rooms[4][3];

/*
 * Table of far pointers to the ANIM_SEQUENCE records selectable via an
 * ANIM_OBJECT's `animate` field (index * 4 bytes per entry). Real address
 * 340e:0b51 (Ghidra symbol `_animate_buffer`); get_xrefs_to on this
 * address returned nothing (the indexed access pattern isn't tracked as
 * a normal xref), and the table's populating/loading function wasn't
 * identified in this pass, so its true entry count is unconfirmed --
 * declared as an incomplete array type (valid for an extern declaration;
 * indexing still compiles correctly regardless of the unspecified bound).
 */
extern ANIM_SEQUENCE far * _animate_buffer[];

void alloc_map_animation(void);
void alloc_room_animation(ANIM_OBJECT far * obj);
void update_room_animation(unsigned int sync, ANIM_OBJECT far * obj, MAZE_STRUCT far * maze_ptr, MAZE_STRUCT far * amaze_ptr);
void update_frame(MAZE_STRUCT far * maze_ptr, MAZE_STRUCT far * amaze_ptr, int row, int col, ANIM_FRAME far * frame, int far * prev_frame, int ow);
void room_animation_func(unsigned int sync, ANIM_OBJECT far * obj, MAZE_STRUCT far * maze_ptr);
void room_animation(unsigned int sync, int hoff, int voff);
void clear_room_animation(unsigned int sync, int hoff, int voff);
void update_map_animation(unsigned int sync);
void room_frame(MAZE_STRUCT far * maze_ptr, int srow, int scol, int redraw, ANIM_FRAME far * frame);
void clear_room_animation_func(unsigned int sync, ANIM_OBJECT far * obj, MAZE_STRUCT far * maze_ptr);

/*
 * clear_room_frame()/create_anim_sprite(): the stub-generation pass that
 * originally built this file's prototypes ran out of confidently-matched
 * debug-info entries for this module (per the old "type-def list is
 * shorter than its true local-function count" TODOs it left behind) and
 * guessed wrong signatures for these two names -- (sync,hoff,voff) and
 * (sync,obj,maze_ptr) respectively -- while the two real functions at
 * those addresses were left as unk_func_0736/unk_func_08DD placeholders.
 * Ghidra's own function search resolves clear_room_frame to 1a54:0736
 * and create_anim_sprite to 1a54:08dd with the signatures below (matching
 * the unk_func_* placeholders' parameter lists exactly), confirming the
 * guessed versions never existed as real functions. Corrected here.
 */
void clear_room_frame(MAZE_STRUCT far * maze_ptr, int srow, int scol, ANIM_FRAME far * frame, int far * prev_frame, int ow);
void create_anim_sprite(int prev_sprite, int next_sprite, unsigned char far * sp_buff);

#endif /* MAZEANIM_H */
