/*
 * MEDITSTR.H
 *
 * Public prototypes exported by MEDITSTR.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MEDITSTR_H
#define MEDITSTR_H

#include "PACWARS.H"

/*
 * The stub-generation pass guessed pacman_t=0x490/name_t=0x491/
 * score_t=0x493 for this enum, but those values don't match anything
 * edit_str's compiled body actually does -- disassembly shows it
 * comparing its `type` parameter directly against small integers (1,
 * 2, 3, 5, 6, 7), and every real caller in the binary (edit_pacname,
 * edit_pacfile, edit_name, save_routine, edit_maze_rows -- confirmed
 * via get_xrefs_to) passes the literal value 3. Only that one value is
 * exercised by any current caller, so it's the only member given a
 * real name here; the others are declared for completeness (matching
 * observed comparisons in edit_str) but their true symbolic names
 * weren't recoverable from this pass's evidence.
 */
typedef enum {
    edit_type_1 = 1,
    edit_type_2 = 2,
    edit_type_general = 3,   /* the only value any real caller passes */
    edit_type_5 = 5,
    edit_type_6 = 6,
    edit_type_7 = 7,
} EDIT_TYPE;

int edit_str(unsigned int far * inkey, unsigned int far * ext, int row, int col, char far * char_str, EDIT_TYPE type, int display);

#endif /* MEDITSTR_H */
