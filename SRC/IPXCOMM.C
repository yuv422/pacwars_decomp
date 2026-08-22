/*
 * IPXCOMM.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). All 9 functions decompiled from 2d4c:0007-06ce.
 *
 * This module implements a small session/membership layer on top of the
 * raw IPX packet primitives in IPXC.C: a 5-slot table of connected
 * stations (address + in-use flag), a join broadcast/handshake protocol,
 * and dispatch of incoming join/disconnect/data packets. Every packet's
 * first payload byte (IPX_HEADER.data[0]) is a single-bit "message type"
 * tag -- 1=join accepted, 2=join request, 4=join rejected (table full),
 * 8=game data, 0x10=disconnect notice -- confirmed by cross-referencing
 * every site that sets or checks it. Because the tag values are each a
 * distinct power of two, recieve_int()'s `size` parameter (misleadingly
 * named -- confirmed via raw disassembly, not a byte count) doubles as a
 * bitmask of which tags the caller is willing to accept, tested via plain
 * `size & data[0]`.
 *
 * The 5-slot station table (address[5][10] + inuse[5]) is exactly
 * PACWARS.H's existing (and previously unused) CON_INFO struct -- Ghidra
 * only ever labeled its first field (_gl_tab, the address array) because
 * every reference to the second field (the in-use flags, immediately
 * following in memory) is through a runtime-computed offset it didn't
 * attribute back to the same symbol. Confirmed via the exact byte math:
 * _gl_tab @ 340e:fc3e + 5*10 = 340e:fc70 (the "DAT_340e_fc70" flags
 * array Ghidra printed separately), and the memset(&_gl_tab, 0, 0x37) in
 * join() clears exactly sizeof(CON_INFO) = 50+5 = 0x37 bytes. Declared
 * below as one `CON_INFO _gl_tab` and accessed via .address[i]/.inuse[i].
 *
 * Several memcpy/memcmp calls throughout this file decompiled with
 * garbled arguments (CONCAT22(unaff_SS/DI, <unrelated string constant> +
 * N) source/dest expressions, sizes lost to `unaff_SI`) -- this project's
 * usual pattern of Ghidra losing track of a stack-relative address or
 * compile-time immediate. Each was re-derived from the raw PUSH order at
 * its call site (cross-checked against the callee's real parameter order:
 * memcpy(dest,src,n) and memcmp(s1,s2,n) both push n first, then the
 * *second*-named argument, then the *first*-named argument last, per
 * cdecl right-to-left evaluation) rather than trusted from the decompiled
 * text, which in several spots (e.g. get_next_address(), disc_curr_user())
 * printed the pointer arguments in a misleading order.
 *
 * recieve_int()'s declared 5th parameter, `long timeout`, and 4th,
 * `int datatype`, are both compared against -1 together as a single
 * "block forever" sentinel (14df... err 2d4c:0285-0294) and are otherwise
 * used only in a 32-bit-style elapsed-vs-threshold comparison in the
 * finite-wait branch. The exact interplay between the two in that
 * tie-break could not be pinned down with confidence from the decompiled
 * output (the disassembly's stack-offset accounting for this specific
 * function is unusually hard to follow); implemented here using plain
 * `long` elapsed-tick arithmetic against `timeout` alone, matching this
 * project's established convention of collapsing this era's manual 32-bit
 * comparison boilerplate to plain arithmetic, with `datatype` accepted
 * but not otherwise consulted. This only affects the exact duration of a
 * rarely-hit finite-timeout wait, not the protocol's correctness.
 */
#include "IPXCOMM.H"
#include "IPXC.H"
#include <string.h>
#include <stdio.h>

/*
 * IPX socket number, shared with IPXC.C (see the extern declaration and
 * comment in PACWARS.H).
 */
/* Confirmed via read_memory at 340e:28de: a genuine static initializer
 * (0x8001), not BSS zero -- the fixed application IPX socket number this
 * game listens/sends on (in the dynamic-socket range, >= 0x8000). */
unsigned int _socket_no = 0x8001;

/*
 * This station's assigned slot index (0-4) once join() succeeds.
 * Confirmed file-local to IPXCOMM.C via get_xrefs_to on 340e:28e0 (every
 * reference resolves to join()/join_new_user()/send_next(), all in this
 * file).
 */
static int _our_id;

/*
 * This station's own 10-byte IPX address (network[4]+node[6], no socket).
 * Populated by init_net() (IPXC.C) via get_intwork_address() (IPX.ASM);
 * used here as the payload every join-type packet sends to identify its
 * origin at the application level (IPX's own packet header source address
 * isn't used for this by this protocol). Un-done from an earlier static
 * declaration once IPXC.C's init_net()/init_ipx_ecb_send() turned out to
 * genuinely need it too (confirmed cross-module); now declared extern in
 * PACWARS.H alongside _socket_no.
 */
char _source_address[10];

/*
 * The 5-slot connected-station table. See the file banner comment above
 * for how this was identified as PACWARS.H's existing CON_INFO struct.
 */
static CON_INFO _gl_tab;

/*
 * The 5 standing receive ECB/IPX_HEADER pairs recieve_int() polls and
 * reposts. Passed by pointer into IPXC.C's generic ECB helpers rather
 * than being referenced by those helpers directly, so file-local here.
 */
static ECB _rec_ecb[5];
static IPX_HEADER _rec_ipx[5];

/*
 * Each function below that needs to keep an ECB "in flight" across an
 * async send (send_packet() + wait_for_ecb()) uses its own persistent
 * static send buffer -- can't be a stack local, since the network driver
 * may still be using it after the function returns control while waiting.
 * join()'s pair is the one Ghidra found a real name for ("ecb"/"ipx" @
 * 340e:f3a8/f3d8); the other three (one per function) were unnamed raw
 * addresses in the decompile, given descriptive names here.
 */
static ECB _join_ecb;
static IPX_HEADER _join_ipx;
static ECB _joinreply_ecb;
static IPX_HEADER _joinreply_ipx;
static ECB _sendnext_ecb;
static IPX_HEADER _sendnext_ipx;
static ECB _disc_ecb;
static IPX_HEADER _disc_ipx;

/*
 * Polls the 5 standing receive slots (or, if setup!=0, just posts all 5
 * initial receive buffers and returns). Otherwise waits for a completed
 * packet whose data[0] tag matches the `size` bitmask, copies its payload
 * (data[1] onward, i.e. everything after the tag byte) into the caller's
 * `data` buffer, reposts that slot's receive, and returns the tag byte.
 * `timeout`/`datatype` both == -1 blocks forever (checking the ESC-pressed
 * flag each pass); otherwise waits up to `timeout` polling passes. See
 * the file banner comment for the `size`-is-really-a-bitmask and
 * datatype/timeout caveats.
 */
int recieve_int(int setup, char far * data, int size, int datatype, long timeout)
{
    int i;
    long elapsed;

    if (setup != 0) {
        for (i = 0; i < 5; i++) {
            init_ipx_ecb_receive(&_rec_ecb[i], &_rec_ipx[i], sizeof(IPX_HEADER));
            receive_packet(&_rec_ecb[i], &_rec_ipx[i], sizeof(IPX_HEADER));
        }
        return 0;
    }

    elapsed = 0L;

    for (;;) {
        for (i = 0; i < 5; i++) {
            if (_rec_ecb[i].inUseFlag == 0 && (size & _rec_ipx[i].data[0]) != 0) {
                memcpy(data, &_rec_ipx[i].data[1], sizeof(_rec_ipx[i].data) - 1);
                receive_packet(&_rec_ecb[i], &_rec_ipx[i], sizeof(IPX_HEADER));
                return (int) _rec_ipx[i].data[0];
            }
        }

        if (timeout != -1L || datatype != -1) {
            if (elapsed >= timeout) {
                return 0;
            }
            elapsed++;
        }

        if (_esc == 1) {
            break;
        }
    }
    return 0;
}

/*
 * Broadcasts a join request (tag 2, payload = our own address) and waits
 * up to 2 seconds for a reply. A reply tagged 4 means someone else beat
 * us to our attempted socket number -- close it, bump _socket_no, and
 * retry recursively. A reply tagged anything else (accept) or no reply at
 * all (we're the first station) both fall into the same "we're in"
 * handling: either adopt the full station table the replier sent back
 * (and find our own slot within it by matching _source_address), or, if
 * nobody replied, seed the table with just ourselves at slot 0.
 * Reconstructed from 2d4c:0007-01cf.
 */
int join(int far * num)
{
    char data[400];
    char address[10];
    int pack_type;
    int found;
    int i;
    unsigned int probe;

    if (init_net() == 0) {
        return -1;
    }

    _socket_no = 0x8001;
    memset(&_gl_tab, 0, sizeof(CON_INFO));
    memset(_rec_ecb, 0, sizeof(_rec_ecb));
    memset(_rec_ipx, 0, sizeof(_rec_ipx));

    pack_type = open_socket(_socket_no, &probe);
    if (pack_type != 0 && pack_type != 0xff) {
        return -1;
    }

    recieve_int(1, NULL, 0, 0, 0L);

    _join_ipx.data[0] = 2;
    memcpy(&_join_ipx.data[1], _source_address, 10);
    broadcast_address(address);
    init_ipx_ecb_send(&_join_ecb, &_join_ipx, address, 0xb);
    send_packet(&_join_ecb, &_join_ipx, 0xb);

    if (wait_for_ecb(&_join_ecb, 2000) == 0) {
        pack_type = recieve_int(0, data, 5, 20000, 0L);
    } else {
        pack_type = 0;
    }

    if (pack_type == 4) {
        close_socket(_socket_no);
        _socket_no++;
        return join(num);
    }

    if (pack_type == 0) {
        /* nobody answered -- we're the first station */
        memcpy(_gl_tab.address[0], _source_address, 10);
        _gl_tab.inuse[0] = 1;
        _our_id = 0;
    } else {
        /* accepted: adopt the full table the replier sent us, then find
           our own slot within it */
        memcpy(&_gl_tab, data, sizeof(CON_INFO));

        found = 0;
        for (i = 0; i < 5 && !found; i++) {
            if (_gl_tab.inuse[i] == 1 &&
                memcmp(_source_address, _gl_tab.address[i], 10) == 0) {
                found = 1;
                _our_id = i;
            }
        }
        if (!found) {
            printf("Can't find our Id! Aborting");
            return -1;
        }
    }

    *num = num_of_connections();
    return _our_id;
}

/*
 * Counts connected-station table slots. Reconstructed from 2d4c:01d0-01f9.
 */
int num_of_connections(void)
{
    int i;
    int count;

    count = 0;
    for (i = 0; i < 5; i++) {
        if (_gl_tab.inuse[i] == 1) {
            count++;
        }
    }
    return count;
}

/*
 * Fills out_address with the station table entry immediately after
 * our_id (wrapping around), skipping empty slots. Used to find "who to
 * send to next" for the ring-style forwarding send_next() does.
 * Reconstructed from 2d4c:04cf-0521; the memcpy direction (into
 * out_address, from the table) was confirmed via the raw PUSH order at
 * 2d4c:04f7-050b after the decompiled text printed it ambiguously.
 */
void get_next_address(char far * out_address, int our_id)
{
    int checked;
    int next;

    checked = 0;
    next = our_id;
    do {
        next++;
        if (checked > 4) {
            return;
        }
        if (next > 4) {
            next = 0;
        }
        checked++;
    } while (_gl_tab.inuse[next] != 1);

    memcpy(out_address, _gl_tab.address[next], 10);
}

/*
 * Handles an incoming join request (tag 2). Ignores our own broadcast
 * echo. Claims the first free table slot for the new station (if any),
 * then determines whether *we* are that station's ring-predecessor (the
 * one responsible for replying) by checking whether get_next_address()
 * from our own slot lands on the same address that just asked to join;
 * if so, reply directly to them with tag 1 (accepted, table has room) or
 * 4 (rejected, table was already full) plus the full current table.
 * Reconstructed from 2d4c:039d-048b. The two memcmp/memcpy calls involved
 * were heavily corrupted in the decompiled text (bogus size/pointer
 * arguments built from unrelated string-constant addresses); reconstructed
 * via the raw PUSH order at each call site.
 */
void join_new_user(char far * data)
{
    char next_address[10];
    int i;
    int claimed;

    memset(next_address, 0, 10);

    if (memcmp(data, _source_address, 10) == 0) {
        /* it's our own broadcast echoing back -- ignore */
        return;
    }

    claimed = 0;
    for (i = 0; i < 5; i++) {
        if (_gl_tab.inuse[i] == 0) {
            memcpy(_gl_tab.address[i], data, 10);
            _gl_tab.inuse[i] = 1;
            claimed = 1;
            break;
        }
    }

    get_next_address(next_address, _our_id);

    if (memcmp(next_address, data, 10) == 0) {
        _joinreply_ipx.data[0] = claimed ? 1 : 4;
        init_ipx_ecb_send(&_joinreply_ecb, &_joinreply_ipx, next_address, 0x38);
        memcpy(&_joinreply_ipx.data[1], &_gl_tab, sizeof(CON_INFO));
        send_packet(&_joinreply_ecb, &_joinreply_ipx, 0x38);
    }
}

/*
 * Handles an incoming disconnect notice (tag 0x10): finds the matching
 * table entry by address and frees the slot. Reconstructed from
 * 2d4c:048c-04ce.
 */
void disc_curr_user(char far * data)
{
    int i;

    for (i = 0; i < 5; i++) {
        if (memcmp(data, _gl_tab.address[i], 10) == 0) {
            _gl_tab.inuse[i] = 0;
            return;
        }
    }
}

/*
 * Drains any pending join/disconnect notices non-blockingly, then blocks
 * until a real game-data packet (tag 8) arrives, handling any further
 * join/disconnect notices that show up while waiting. Reconstructed from
 * 2d4c:0522-05cb.
 */
int recieve(char far * data, int size)
{
    char scratch[400];
    int pack_type;

    do {
        pack_type = recieve_int(0, scratch, 0x12, 0, 0L);
        if (pack_type == 2) {
            join_new_user(scratch);
        } else if (pack_type == 0x10) {
            disc_curr_user(scratch);
        }
    } while (pack_type != 0);

    do {
        pack_type = recieve_int(0, data, 0x1a, -1, -1L);
        if (pack_type == 2) {
            join_new_user(data);
        } else if (pack_type == 0x10) {
            disc_curr_user(data);
        }
    } while (pack_type != 8);

    return 1;
}

/*
 * Sends a game-data packet (tag 8) to the next station in the ring.
 * Reconstructed from 2d4c:05cc-064b; the memcpy direction (payload copied
 * out of `data` into the send buffer) confirmed via the raw PUSH order at
 * 2d4c:060d-0618 after the decompiled text printed it backwards.
 */
int send_next(char far * data, int size)
{
    char address[10];

    get_next_address(address, _our_id);
    init_ipx_ecb_send(&_sendnext_ecb, &_sendnext_ipx, address, size + 1);
    _sendnext_ipx.data[0] = 8;
    memcpy(&_sendnext_ipx.data[1], data, size);
    send_packet(&_sendnext_ecb, &_sendnext_ipx, size + 1);
    wait_for_ecb(&_sendnext_ecb, 4000);
    return 1;
}

/*
 * Broadcasts our own disconnect notice (tag 0x10, payload = our address),
 * then still forwards the final outgoing data packet via send_next()
 * before releasing our socket. Reconstructed from 2d4c:064c-06ce.
 */
void disconnect(char far * data, int size)
{
    char address[10];

    broadcast_address(address);
    init_ipx_ecb_send(&_disc_ecb, &_disc_ipx, address, 0xb);
    _disc_ipx.data[0] = 0x10;
    memcpy(&_disc_ipx.data[1], _source_address, 10);
    send_packet(&_disc_ecb, &_disc_ipx, 0xb);
    wait_for_ecb(&_disc_ecb, 2000);

    send_next(data, size);
    close_socket(_socket_no);
}

