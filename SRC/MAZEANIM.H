/*
 * MAZEANIM.H
 *
 * Public prototypes exported by MAZEANIM.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZEANIM_H
#define MAZEANIM_H

#include "PACWARS.H"

void alloc_map_animation(void);
void room_animation(unsigned int sync, int hoff, int voff);

/*
 * Gap fill (same class of bug documented elsewhere in this project): these
 * two exported functions -- clear_room_animation @ 1a54:05bc (the "clear"
 * counterpart to room_animation, same 3-param signature) and
 * update_map_animation @ 1a54:00d7 (1 param) -- exist in PACWARS.EXE's
 * MAZEANIM.C module but were missing from this header/the generated stub
 * set entirely. Added here (as stubs, pending full MAZEANIM.C
 * decompilation) so callers elsewhere in the project (main_loop, MAZE.C)
 * can compile against them.
 */
void clear_room_animation(unsigned int sync, int hoff, int voff);
void update_map_animation(unsigned int sync);

#endif /* MAZEANIM_H */
