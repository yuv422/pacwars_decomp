/*
 * MAZEREG.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZEREG.H"
#include <stdlib.h>

/*
 * The actual REGISTER_STRUCT instance `reg` points to, byte-for-byte as
 * it appears in the original binary's data segment (340e:27e0). `reg`
 * itself is never assigned anywhere at runtime -- every reference to it
 * across the whole program is a read (confirmed via xref analysis) -- so
 * in the original source this was almost certainly just a statically
 * initialized `REGISTER_STRUCT far *reg = &some_struct;` rather than
 * something set up during startup, and that's reproduced directly here.
 *
 * These bytes are XOR-encoded at rest (see encode_data() below); they
 * only become a meaningful REGISTER_STRUCT after decode_reg() runs once,
 * which is why decode_reg() is the very first thing main() calls.
 */
static unsigned char actual_reg[91] = {
    0xB6, 0xDF, 0x42, 0xA6, 0xAE, 0xAE, 0x97, 0xED, 0x56, 0x85,
    0x42, 0xC0, 0xD5, 0x52, 0x2E, 0x50, 0x15, 0x92, 0xA0, 0xB3,
    0x25, 0x28, 0x7E, 0x59, 0x96, 0x09, 0xBE, 0x4B, 0x7B, 0x8D,
    0x90, 0x87, 0x23, 0x8A, 0x4B, 0x60, 0xE2, 0x2A, 0x47, 0x9A,
    0xD4, 0x59, 0x2B, 0x7B, 0xB6, 0x4F, 0xE5, 0xF0, 0x08, 0x5C,
    0xE8, 0xFE, 0x61, 0xF3, 0xAA, 0x6C, 0x12, 0xB3, 0x3A, 0x8F,
    0x86, 0x3E, 0x99, 0x36, 0xD6, 0xC8, 0x52, 0x53, 0xC0, 0x0F,
    0xE2, 0x98, 0xB9, 0x3F, 0xD3, 0xC7, 0x6C, 0x1F, 0xD6, 0xB7,
    0x5A, 0xB9, 0x2D, 0xB7, 0xD8, 0xE2, 0xCB, 0x73, 0x6E, 0xBC,
    0x09
};

/* storage for the `reg` global declared extern in PACWARS.H */
REGISTER_STRUCT far * reg = (REGISTER_STRUCT far *) actual_reg;

void decode_reg(void)
{
    /*
     * "decode" and "encode" are the same operation here: encode_data()
     * is a toggle-style transform (applying it twice on the same bytes
     * restores the original), so decode_reg() just calls it once on the
     * whole REGISTER_STRUCT pointed to by `reg`. 0x5b (91) matches
     * sizeof(REGISTER_STRUCT) exactly (15 + 1 + 41 + 2*17).
     */
    encode_data(reg, sizeof(REGISTER_STRUCT));
}

void encode_data(void far * data_ptr, int size)
{
    unsigned char far * p = (unsigned char far *) data_ptr;
    unsigned int i;

    /*
     * A fixed-seed XOR stream cipher: reseeding rand() with the same
     * constant (0x5CE5) every call reproduces the exact same byte
     * sequence each time, which is what makes this reversible with a
     * single shared routine -- XOR-ing the same keystream back onto the
     * already-encoded bytes restores the original data. That's why
     * decode_reg() just calls this once rather than needing a separate
     * decode path.
     */
    srand(0x5ce5);
    for (i = 0; i < (unsigned int) size; i++) {
        p[i] ^= (unsigned char) (rand() % 0x100);
    }
}

