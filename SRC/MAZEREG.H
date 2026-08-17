/*
 * MAZEREG.H
 *
 * Public prototypes exported by MAZEREG.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZEREG_H
#define MAZEREG_H

#include "PACWARS.H"

void decode_reg(void);
void encode_data(void far * data_ptr, int size);

#endif /* MAZEREG_H */
