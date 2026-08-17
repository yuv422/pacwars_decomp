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

#endif /* SPRITGEN_H */
