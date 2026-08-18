/*
 * HISCORE.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "HISCORE.H"
#include "MAZEUTIL.H"
#include <fcntl.h>
#include <io.h>
#include <string.h>

/*
 * File handle for HISCORE.DAT, shared by open_hiscore/close_hiscore/
 * get_hiscore/init_hiscore in this module. Confirmed via Ghidra xref
 * analysis (_hiscorefd @ 340e:f3a6) to be referenced only from
 * functions defined in this file, so it is file-scope here rather
 * than exported through HISCORE.H.
 */
static int hiscorefd;

int open_hiscore(void)
{
    char file_name[MAXPATH];

    /*
     * NOTE: Ghidra's decompiler mis-rendered the open() mode argument
     * as "unaff_SS" (an uninitialized/stray register value) because
     * its 16-bit real-mode calling-convention analysis lost track of
     * the immediate word pushed at 2c47:0038. The actual disassembly
     * pushes the 16-bit constant 0x80C4 = O_BINARY|O_NOINHERIT|
     * O_DENYNONE|O_RDWR, confirmed against this project's Borland
     * FCNTL.H, so that combination is used here instead of the
     * decompiler's mistaken register reference.
     */
    strcpy(file_name, "hiscore.dat");
    get_filename(file_name);
    hiscorefd = open(file_name, O_BINARY | O_NOINHERIT | O_DENYNONE | O_RDWR);
    if (hiscorefd == -1) {
        return -1;
    }
    return 0;
}

void close_hiscore(void)
{
}

int get_hiscore(HISCORE far * hiscore_buff)
{
    return 0;
}

int set_hiscore(int score, char far * name, int pacman)
{
    return 0;
}

int init_hiscore(void)
{
    return 0;
}

