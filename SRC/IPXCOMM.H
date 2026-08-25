/*
 * IPXCOMM.H
 *
 * Public prototypes exported by IPXCOMM.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef IPXCOMM_H
#define IPXCOMM_H

#include "PACWARS.H"

int join(int far * num);
int num_of_connections(void);
int recieve_int(int setup, char far * data, int size, int datatype, long timeout);
void join_new_user(char far * data);
void disc_curr_user(char far * data);
void get_next_address(char far * out_address, int our_id);
int recieve(char far * data, int size);
int send_next(char far * data, int size);
void disconnect(char far * data, int size);

/*
 * Temporary diagnostic helper (not part of the original binary) for
 * tracking down the IPX join/connection bug. Formats printf-style and
 * writes straight to stdout (file handle 1) via the low-level write()
 * call, bypassing stdio buffering so nothing is lost if the game later
 * hangs. Redirect stdout to a file when launching under DOSBox (e.g.
 * `PACWARS.EXE > netlog.txt`) since the game runs in VGA graphics mode
 * and nothing shows on the visible screen. Safe to strip out once the
 * join process is confirmed working.
 */
void net_dbg(char * fmt, ...);

#endif /* IPXCOMM_H */
