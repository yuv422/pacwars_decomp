/*
 * MAZEINIT.C
 *
 * Reconstructed from PACWARS.EXE via Ghidra decompilation/disassembly of
 * the real function bodies. This module implements a tiny "menu program"
 * config-file mechanism: open_prog()/close_prog() open and close one of a
 * fixed list of candidate menu-program files (menu_prog[]), get_prog()
 * scans the open file for a given token and leaves the file positioned
 * right after it, and set_prog() writes a (now-uppercased) replacement
 * string at the current file position -- together, set_maze_path() uses
 * these to find the PATH-indicator token (_path_indic) in each candidate
 * file and overwrite it with the caller's path.
 */
#include "MAZEINIT.H"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration: set_maze_path() calls set_prog(), defined further
   down this file (and not exported via MAZEINIT.H). */
int set_prog(char far * path_name);

/*
 * Handle for whichever menu-program file is currently open via
 * open_prog(). Confirmed via Ghidra xref analysis to be referenced only
 * from this file's own functions, so file-scope here. No real name
 * recoverable from PACWARS.TXT (Ghidra shows only a raw DAT_ label).
 */
static int prog_handle;

/* Scratch read buffer used by get_prog()/set_prog(), allocated by
   set_maze_path() and freed before it returns. File-scope: referenced
   only from this file's functions. */
static unsigned char far * prog_buff;

/*
 * List of candidate program files set_maze_path() patches the MAZEPATH
 * token into, NULL-terminated. As shipped, this only lists the game's
 * own executable -- apparently a leftover from a more general "menu
 * program" mechanism (shared with a family of other MVA-based programs)
 * that this particular game never grew a second entry for.
 */
static char far * menu_prog[2] = { "PACWARS.EXE", NULL };

/* Token searched for by get_prog(), whose following bytes set_prog()
   overwrites with the caller's maze data path. */
static char far * _path_indic = "MAZEPATH";

/*
 * Finds the given file-loading candidate (menu_prog[i], a NUL-terminated
 * far string) whose config file contains the _path_indic token, and
 * overwrites what follows it with `path` (backslash-terminated). Loops
 * over every non-empty entry in menu_prog[]; any entry that fails to
 * open or doesn't contain the token is skipped with a diagnostic message
 * rather than aborting the whole scan.
 */
void set_maze_path(char far * path)
{
    char path_buf[128];
    unsigned int len;
    int i;

    if ((prog_buff = (unsigned char far *) calloc(1, 0x7c00)) == NULL) {
        printf("\nNot Enough Memory to set path");
        exit(1);
    }

    strcpy(path_buf, path);
    len = strlen(path_buf);
    if (path_buf[len - 1] != '\\') {
        strcat(path_buf, "\\");
    }

    for (i = 0; menu_prog[i] != NULL; i++) {
        if (!open_prog(menu_prog[i])) {
            printf("Cannot Open - %s", menu_prog[i]);
            continue;
        }

        if (get_prog(_path_indic) > 0) {
            if (set_prog(path_buf) == -1) {
                printf("Cannot set PATH to %s\n", path_buf);
            }
        }
        close_prog();
    }

    free(prog_buff);
}

int open_prog(char far * menu_name)
{
    prog_handle = open(menu_name, O_BINARY | O_RDWR);
    return prog_handle >= 0;
}

void close_prog(void)
{
    close(prog_handle);
}

/*
 * Scans the file open on prog_handle, 0x7c00 bytes at a time, for the
 * first occurrence of token_name. On a match, seeks the file to just
 * past the matched token (and its NUL terminator) and returns 1; on
 * reaching EOF without a match, returns 0. If a chunk boundary falls in
 * the middle of a potential match, re-seeks and re-reads a fresh chunk
 * starting at the current scan position so the comparison always has the
 * whole token available.
 */
long get_prog(char far * token_name)
{
    long posn;
    long num_bytes;
    unsigned char far * prog_ptr;
    unsigned int len;

    posn = 0;
    printf("Searching\n");

    for (;;) {
        num_bytes = read(prog_handle, prog_buff, 0x7c00);
        if (num_bytes <= 0) {
            break;
        }

        prog_ptr = prog_buff;
        posn = 0;

        while (posn < num_bytes) {
            while (posn < num_bytes && *prog_ptr != *token_name) {
                posn++;
                prog_ptr++;
            }

            len = strlen(token_name);
            if (posn + len > num_bytes) {
                lseek(prog_handle, -num_bytes, SEEK_CUR);
                lseek(prog_handle, posn, SEEK_CUR);
                num_bytes = read(prog_handle, prog_buff, 0x7c00);
                prog_ptr = prog_buff;
                posn = 0;
                continue;
            }

            if (posn < num_bytes) {
                if (memcmp(prog_ptr, token_name, len) == 0) {
                    lseek(prog_handle, -num_bytes, SEEK_CUR);
                    lseek(prog_handle, posn + len + 1, SEEK_CUR);
                    return 1;
                }
                prog_ptr++;
                posn++;
            }
        }
    }

    printf("Not found\n");
    return 0;
}

/* Uppercases path_name and writes it (including its NUL terminator) at
   the current file position on prog_handle. */
int set_prog(char far * path_name)
{
    int written;

    strupr(path_name);
    printf("File set to - %s\n", path_name);

    written = write(prog_handle, path_name, strlen(path_name) + 1);
    if (written == strlen(path_name) + 1) {
        return 1;
    }
    return -1;
}

