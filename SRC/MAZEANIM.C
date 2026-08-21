/*
 * MAZEANIM.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZEANIM.H"

void alloc_map_animation(void)
{
}

void alloc_room_animation(ANIM_OB far * obj)
{
}

void update_room_animation(unsigned int sync)
{
}

void update_frame(unsigned int sync, ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr, MAZE_STRUCT far * amaze_ptr)
{
}

void room_animation_func(MAZE_STRUCT far * maze_ptr, MAZE_STRUCT far * amaze_ptr, int row, int col, ANIM_FRAME far * frame, int far * prev_frame, int ow)
{
}

void room_animation(unsigned int sync, int hoff, int voff)
{
}

/*
 * Gap fill: see the comment in MAZEANIM.H. Real address 1a54:05bc, not
 * yet decompiled.
 */
void clear_room_animation(unsigned int sync, int hoff, int voff)
{
}

/*
 * Gap fill: see the comment in MAZEANIM.H. Real address 1a54:00d7, not
 * yet decompiled.
 */
void update_map_animation(unsigned int sync)
{
}

void room_frame(unsigned int sync, ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
}

void clear_room_animation_func(MAZE_STRUCT far * maze_ptr, int srow, int scol, int redraw, ANIM_FRAME far * frame)
{
}

void clear_room_frame(unsigned int sync, int hoff, int voff)
{
}

void create_anim_sprite(unsigned int sync, ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
}

/* TODO: real name not recoverable from PACWARS.TXT for this local
   function (module's type-def list is shorter than its true local-
   function count) -- offset 0A54:0736. Verify/rename by
   cross-referencing the Ghidra project. */
void unk_func_0736(MAZE_STRUCT far * maze_ptr, int srow, int scol, ANIM_FRAME far * frame, int far * prev_frame, int ow)
{
}

/* TODO: real name not recoverable from PACWARS.TXT for this local
   function (module's type-def list is shorter than its true local-
   function count) -- offset 0A54:08DD. Verify/rename by
   cross-referencing the Ghidra project. */
void unk_func_08DD(int prev_sprite, int next_sprite, unsigned char far * sp_buff)
{
}

