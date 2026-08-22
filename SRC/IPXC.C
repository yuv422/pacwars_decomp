/*
 * IPXC.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). All functions decompiled from 2d1a:000a-0327.
 *
 * This module is the thin C layer between IPXCOMM.C's session protocol
 * and the raw NetWare IPX driver primitives implemented in IPX.ASM
 * (open_socket, close_socket, get_local_address, send_packet_asm,
 * listen_packet_asm, get_intwork_address -- see that file's banner
 * comment for the IPX function-number mapping). Every ECB/IPX_HEADER
 * pair follows the same setup idiom: clear ESDAddress, stamp the local
 * socket number into the ECB in network byte order via low_high(), clear
 * inUseFlag, then point fragDesc[0] at the caller's IPX_HEADER buffer
 * with size = payload size + 0x1e (0x1e = 30 = sizeof(IPX_HEADER) minus
 * its 400-byte data[] tail, i.e. the fixed header prefix every packet
 * carries ahead of its payload).
 *
 * Several calls into IPX.ASM's routines and into low_high()/fill_address()
 * decompiled with no visible arguments at all (e.g. `_get_local_address();`)
 * -- a known Ghidra limitation for far calls across code segments/
 * compilation units, where argument-passing isn't tracked once the
 * callee is in a different segment. Each was resolved by cross-checking
 * the callee's own real parameter list (via its own disassembly/ASM
 * reconstruction in IPX.ASM) against what the caller's stack setup
 * plausibly needed, rather than trusted from the empty-looking decompile.
 */
#include "IPXC.H"
#include <string.h>

/*
 * Converts a 16-bit value to network (big-endian) byte order at *intptr.
 * Used throughout this file (and by IPX.ASM's callers) instead of a
 * shared byte-swap macro. Reconstructed from 2d1a:000a-003c.
 */
void low_high(unsigned int far * intptr, unsigned int data)
{
    unsigned char far * p = (unsigned char far *) intptr;

    p[0] = (unsigned char) (data >> 8);
    p[1] = (unsigned char) data;
}

/*
 * Builds a 12-byte IPX_ADDRESS from a 10-byte network+node buffer (as
 * used throughout IPXCOMM.C's station table/_source_address) plus a
 * socket number (stored in network byte order via low_high()).
 * Reconstructed from 2d1a:003d-0094.
 */
void fill_address(IPX_ADDRESS far * address, char far * node, int socket_number)
{
    memcpy(address, node, 10);
    low_high((unsigned int far *) &address->socket, (unsigned int) socket_number);
}

/*
 * Marks the ECB as not-yet-in-use, stamps the outgoing packet's real
 * length into its header, and hands it to the driver's Send Packet
 * primitive. Reconstructed from 2d1a:0095-00d0.
 */
void send_packet(ECB far * ecb, IPX_HEADER far * ipxh, int size)
{
    ecb->inUseFlag = 0;
    low_high((unsigned int far *) &ipxh->length, size + 0x1e);
    send_packet_asm(ecb);
}

/*
 * Same idea as send_packet() but posts the ECB to the driver's Listen
 * For Packet primitive instead, to receive rather than transmit.
 * Reconstructed from 2d1a:00d1-010c.
 */
void receive_packet(ECB far * ecb, IPX_HEADER far * ipxh, int size)
{
    ecb->inUseFlag = 0;
    low_high((unsigned int far *) &ipxh->length, size + 0x1e);
    listen_packet_asm(ecb);
}

/*
 * Prepares a standing receive ECB: clears ESDAddress, stamps our socket
 * number, zeroes the immediate-address field (unused/don't-care for a
 * receive), and points its single fragment descriptor at the caller's
 * IPX_HEADER buffer. Reconstructed from 2d1a:010d-017f.
 */
void init_ipx_ecb_receive(ECB far * ecb, IPX_HEADER far * ipx, int size)
{
    ecb->ESDAddress = NULL;
    low_high((unsigned int far *) &ecb->sockNumber, _socket_no);
    ecb->inUseFlag = 0;
    memset(ecb->immediateAddress, 0, 6);

    ecb->fragmentCount = 1;
    ecb->fragDesc[0].address = ipx;
    ecb->fragDesc[0].size = size + 0x1e;
}

/*
 * Prepares a send ECB/IPX_HEADER pair addressed to `address` (a 10-byte
 * network+node buffer, e.g. a specific station or the broadcast address):
 * fills in the ECB the same way init_ipx_ecb_receive() does, but also
 * resolves the destination's "immediate address" (the local/physical
 * node to actually hand the packet to, via get_local_address()) into the
 * ECB, and fills in the IPX_HEADER's own length/type/destination/source
 * fields (source = this station's own _source_address). Reconstructed
 * from 2d1a:0180-0274.
 */
void init_ipx_ecb_send(ECB far * ecb, IPX_HEADER far * ipx, char far * address, int size)
{
    IPX_ADDRESS local_addr;
    char immediate[6];

    ecb->ESDAddress = NULL;
    low_high((unsigned int far *) &ecb->sockNumber, _socket_no);
    ecb->inUseFlag = 0;

    fill_address(&local_addr, address, _socket_no);
    get_local_address(&local_addr, immediate);
    memcpy(ecb->immediateAddress, immediate, 6);

    ecb->fragmentCount = 1;
    ecb->fragDesc[0].address = ipx;
    ecb->fragDesc[0].size = size + 0x1e;

    low_high((unsigned int far *) &ipx->length, size + 0x1e);
    ipx->packet_type = 0;
    fill_address(&ipx->destination, address, _socket_no);
    fill_address(&ipx->source, _source_address, _socket_no);
}

/*
 * Polls e->inUseFlag up to `delay` extra passes (delay+1 checks total,
 * matching the original's off-by-one loop structure exactly) waiting for
 * the driver to clear it (operation complete), returning the ECB's
 * completion code on success or -1 on timeout. Reconstructed from
 * 2d1a:0275-02a7.
 */
int wait_for_ecb(ECB far * e, int delay)
{
    int i;

    for (i = 0; i <= delay; i++) {
        if (e->inUseFlag == 0) {
            return (unsigned int) e->completionCode;
        }
    }
    return -1;
}

/*
 * Checks IPX is installed/caches the driver entry point (init_ipx_asm,
 * IPX.ASM), and if so fetches this station's own network+node address
 * into _source_address for every other function in this module (and
 * IPXCOMM.C) to use as packets' source address. Reconstructed from
 * 2d1a:02a8-02e7; the decompiled text showed an extra memcpy into an
 * unnamed local buffer right after the get_intwork_address() call with a
 * garbled/lost source expression -- get_intwork_address() (IPX.ASM)
 * already writes directly to whatever buffer it's given via ES:SI, so
 * that memcpy was dropped as either a dead store or decompiler noise;
 * writing straight into _source_address is the sensible, unambiguous
 * behavior every caller in IPXCOMM.C already expects.
 */
int init_net(void)
{
    if (init_ipx_asm() != 0) {
        get_intwork_address(_source_address);
        return 1;
    }
    return 0;
}

/*
 * Fills `address` with the standard IPX broadcast address (network =
 * 0.0.0.0, node = FF:FF:FF:FF:FF:FF) and returns the same pointer, for
 * chaining directly into init_ipx_ecb_send()'s `address` argument.
 * Reconstructed from 2d1a:02e8-0327.
 */
char far * broadcast_address(char far * address)
{
    memset(address, 0, 4);
    memset(address + 4, 0xff, 6);
    return address;
}
