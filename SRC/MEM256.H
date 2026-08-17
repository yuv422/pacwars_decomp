/*
 * MEM256.H
 *
 * Public prototypes exported by MEM256.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MEM256_H
#define MEM256_H

#include "PACWARS.H"

void far * memcpyzzz(void far * dst, void far * src, unsigned int n);
void far * memcpyr(void far * dst, void far * src, unsigned int n);
void far * memcpyv(void far * dst, void far * src, unsigned int w, unsigned int h, unsigned int dest_wrap, unsigned int source_wrap);
void far * memcpya(void far * dst, void far * src, unsigned int w, unsigned int h, unsigned int wrap);

#endif /* MEM256_H */
