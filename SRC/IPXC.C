/*
 * IPXC.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "IPXC.H"

void low_high(unsigned int far * intptr, unsigned int data)
{
}

void fill_address(IPX_ADDRESS far * address, char far * node, int socket_number)
{
}

void send_packet(ECB far * ecb, IPX_HEADER far * ipxh, int size)
{
}

void receive_packet(ECB far * ecb, IPX_HEADER far * ipxh, int size)
{
}

void init_ipx_ecb_receive(ECB far * ecb, IPX_HEADER far * ipx, int size)
{
}

void init_ipx_ecb_send(ECB far * ecb, IPX_HEADER far * ipx, char far * address, int size)
{
}

int wait_for_ecb(ECB far * e, int delay)
{
    return 0;
}

int init_net(void)
{
    return 0;
}

char far * broadcast_address(char far * address)
{
    return 0;
}

