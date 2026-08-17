/*
 * DISPPIC.H
 *
 * Public prototypes exported by DISPPIC.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef DISPPIC_H
#define DISPPIC_H

#include "PACWARS.H"

void load_pic_files(char far * file_name1, char far * file_name2);
void load_pic_file(char far * file_name);
void save_pic_file(char far * file_name);
void reset_palette(void);
void rand_palette(void);
void buffer_palette(void);

#endif /* DISPPIC_H */
