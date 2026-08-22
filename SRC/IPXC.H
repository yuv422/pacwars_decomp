/*
 * IPXC.H
 *
 * Public prototypes exported by IPXC.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef IPXC_H
#define IPXC_H

#include "PACWARS.H"

void send_packet(ECB far * ecb, IPX_HEADER far * ipxh, int size);
void receive_packet(ECB far * ecb, IPX_HEADER far * ipxh, int size);

/*
 * Has a real body in IPXC.C but was missing from this header (so nothing
 * outside IPXC.C could call it) -- IPXCOMM.C's recieve_int() needs it to
 * post the 5 standing receive buffers. Added here to export it.
 */
void init_ipx_ecb_receive(ECB far * ecb, IPX_HEADER far * ipx, int size);

void init_ipx_ecb_send(ECB far * ecb, IPX_HEADER far * ipx, char far * address, int size);
int wait_for_ecb(ECB far * e, int delay);
int init_net(void);
char far * broadcast_address(char far * address);

/*
 * Gap fill (same class of bug documented elsewhere in this project): these
 * two functions -- open_socket @ 2db9:0042 and close_socket @ 2db9:0064 --
 * are real, distinct routines in PACWARS.EXE (raw asm IPX socket-open/
 * -close wrappers, in a different code segment from the rest of IPXC.C's
 * functions, param_count 0 per Ghidra since they use registers directly
 * rather than a C calling convention) called by IPXCOMM.C's join()/
 * disconnect(), but no stub for either exists anywhere in this project.
 * Real signature per the raw disassembly at the call sites in join():
 * open_socket pushes (&out_value, socket_no) and reads the result from AX;
 * close_socket pushes just (socket_no). Declared here as stubs, pending a
 * future decompilation pass of IPXC.C's raw-asm routines, so IPXCOMM.C can
 * compile against them.
 */
int open_socket(int socket_no, unsigned int far * out_value);
void close_socket(int socket_no);

#endif /* IPXC_H */
