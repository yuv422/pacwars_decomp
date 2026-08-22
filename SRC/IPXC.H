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
 * Low-level helper used internally by init_ipx_ecb_send()/close_socket
 * (network-byte-order 16-bit store) and init_ipx_ecb_send()/fill_address
 * (builds an IPX_ADDRESS). Not part of PACWARS.EXE's exported symbol
 * table (both are file-local to IPXC.C in the original), but declared
 * here rather than kept static since they're used across this header's
 * declared functions too.
 */
void low_high(unsigned int far * intptr, unsigned int data);
void fill_address(IPX_ADDRESS far * address, char far * node, int socket_number);

/*
 * Raw NetWare IPX driver primitives, implemented in IPX.ASM (real
 * addresses 2db9:002d/0042/0064/0079/0093/00a8 -- a different code
 * segment from the rest of this module, using registers directly rather
 * than being compiled from C). See IPX.ASM's file banner comment for the
 * IPX function-number-per-routine mapping.
 */
int init_ipx_asm(void);
void get_intwork_address(char far * out_address);
int open_socket(int socket_no, unsigned int far * out_value);
void close_socket(int socket_no);
int get_local_address(IPX_ADDRESS far * address, char far * immediate_out);
void send_packet_asm(ECB far * ecb);
void listen_packet_asm(ECB far * ecb);

#endif /* IPXC_H */
