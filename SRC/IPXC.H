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
void init_ipx_ecb_send(ECB far * ecb, IPX_HEADER far * ipx, char far * address, int size);
int wait_for_ecb(ECB far * e, int delay);
int init_net(void);
char far * broadcast_address(char far * address);

#endif /* IPXC_H */
