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

#endif /* IPXCOMM_H */
