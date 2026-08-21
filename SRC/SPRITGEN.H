/*
 * SPRITGEN.H
 *
 * Public prototypes exported by SPRITGEN.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef SPRITGEN_H
#define SPRITGEN_H

#include "PACWARS.H"

void sprite_gen(int num_sprites, char far * far * sprite_files);
void copy_sprite_to_gen(SPRITE_STRUCT far * sprite);
void copy_gen_to_sprite(SPRITE_STRUCT far * sprite);

/*
 * Missing entirely from the original stub generation pass (not even an
 * unk_func_ placeholder), same class of bug as alloc_maze_editor_mem()/
 * button() in MAZEEDIT.C. Confirmed real, exported functions via Ghidra
 * (_copy_sprite_range_to_gen @ 1ae7:24b7, _copy_gen_to_sprite_range @
 * 1ae7:2550, in this module's own segment). Signature recovered from
 * their only call sites so far, in MAZEEDIT.C's edit_block_range();
 * still pending full decompilation as part of SPRITGEN.C.
 */
void copy_sprite_range_to_gen(int row_offset, int col_offset, SPRITE_STRUCT far * sprite);
void copy_gen_to_sprite_range(int row_offset, int col_offset, SPRITE_STRUCT far * sprite);

#endif /* SPRITGEN_H */
