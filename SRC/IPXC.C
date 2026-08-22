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

/*
 * Gap fill: see the comment in IPXC.H. Real address 2db9:0042, not yet
 * decompiled (raw asm, likely a direct IPX OpenSocket call via INT 0x7A).
 */
int open_socket(int socket_no, unsigned int far * out_value)
{
    return 0;
}

/*
 * Gap fill: see the comment in IPXC.H. Real address 2db9:0064, not yet
 * decompiled (raw asm, likely a direct IPX CloseSocket call via INT 0x7A).
 */
void close_socket(int socket_no)
{
}

