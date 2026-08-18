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
#include "UTILS.H"
#include "EXT3_3.H"
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <sys\stat.h>

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
    close(hiscorefd);
}

int get_hiscore(HISCORE far * hiscore_buff)
{
    int nread;

    if (open_hiscore() == -1) {
        return -1;
    }
    nread = read(hiscorefd, hiscore_buff, sizeof(HISCORE));
    close_hiscore();
    if (nread == sizeof(HISCORE)) {
        return 1;
    }
    return -1;
}

int set_hiscore(int score, char far * name, int pacman)
{
    HISCORE hiscore_buff;
    int pos;
    int i;

    if (open_hiscore() == -1) {
        set_mode(3);
        printf("\nError Opening");
        return -1;
    }
    if (read(hiscorefd, &hiscore_buff, sizeof(HISCORE)) != sizeof(HISCORE)) {
        set_mode(3);
        printf("\nError Reading");
        close_hiscore();
        return -1;
    }

    /* find the insertion point: first slot whose score is beaten by the new one */
    for (pos = 0; pos < 10 && (unsigned int) score <= hiscore_buff.score[pos]; pos++) {
    }

    if (pos < 10) {
        /* shift lower entries down to make room, dropping the last one */
        for (i = 8; i >= pos; i--) {
            hiscore_buff.score[i + 1] = hiscore_buff.score[i];
            hiscore_buff.pacman[i + 1] = hiscore_buff.pacman[i];
            strcpy(hiscore_buff.name[i + 1], hiscore_buff.name[i]);
        }
        hiscore_buff.score[pos] = score;
        hiscore_buff.pacman[pos] = pacman;
        strcpy(hiscore_buff.name[pos], name);

        lseek(hiscorefd, 0L, SEEK_SET);
        if (write(hiscorefd, &hiscore_buff, sizeof(HISCORE)) != sizeof(HISCORE)) {
            set_mode(3);
            printf("\nError Writing");
            close_hiscore();
            return -1;
        }
        dosCommit(hiscorefd);
    }

    close_hiscore();
    return 0;
}

int init_hiscore(void)
{
    char file_name[MAXPATH];
    HISCORE hiscore_buff;

    strcpy(file_name, "hiscore.dat");
    get_filename(file_name);
    memset(&hiscore_buff, 0, sizeof(HISCORE));

    /*
     * O_CREAT|O_TRUNC here (vs. the plain O_RDWR open_hiscore() uses)
     * confirmed from the raw immediate 0x8302 pushed at 2c47:0291 =
     * O_BINARY|O_CREAT|O_TRUNC|O_WRONLY -- this function resets the
     * table by (re)creating the file fresh and writing a zeroed
     * HISCORE buffer, unlike open_hiscore()'s "open the existing file"
     * path. The 0x180 mode pushed at 2c47:028e = S_IREAD|S_IWRITE,
     * confirmed against this project's SYS\STAT.H.
     */
    hiscorefd = open(file_name, O_BINARY | O_CREAT | O_TRUNC | O_WRONLY,
                      S_IREAD | S_IWRITE);
    if (hiscorefd == -1) {
        return -1;
    }
    if (write(hiscorefd, &hiscore_buff, sizeof(HISCORE)) != sizeof(HISCORE)) {
        close_hiscore();
        return -1;
    }
    close_hiscore();
    return 0;
}

