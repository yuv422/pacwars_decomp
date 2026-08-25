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
 * distinct power of two, recieve_int()'s `datatype` parameter doubles as
 * a bitmask of which tags the caller is willing to accept, tested via
 * plain `datatype & data[0]`; `size` is a genuine byte count, used as the
 * length of the payload memcpy'd into the caller's buffer.
 *
 * (An earlier pass at this file had `size` and `datatype` swapped --
 * `size` used as the bitmask and a fixed 399-byte copy regardless of the
 * caller's real buffer size -- which happened to work by coincidence at
 * every call site except join()'s reply-wait, but silently overran the
 * ~0x76-byte MAZE_LOG_STRUCT destination passed to recieve_ipx()'s
 * per-frame calls during real network play. See recieve_int()'s own
 * comment and each corrected call site for the details.)
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
 * recieve_int()'s declared 5th parameter, `long timeout`, alone gates the
 * "block forever" sentinel: 2d4c:0285-0294 is two sequential CMP-against
 * -1 tests on timeout's high word (BP+0x12) then low word (BP+0x10) --
 * `datatype` (BP+0xe) doesn't participate, it's only ever used for the
 * tag bitmask test earlier in the loop. (An earlier pass at this
 * function, before that stack-offset accounting was pinned down, treated
 * `timeout != -1 || datatype != -1` as the "apply a real timeout" guard;
 * functionally close enough at most call sites, but see recieve_int()'s
 * own comment for why it mattered at join()'s reply-wait call.)
 */
#include "IPXCOMM.H"
#include "IPXC.H"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <io.h>

/* See IPXCOMM.H for what this is. */
void net_dbg(char * fmt, ...)
{
    char buf[64];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    write(1, buf, strlen(buf));
}

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
 * packet whose data[0] tag matches the `datatype` bitmask, copies `size`
 * bytes of its payload (data[1] onward, i.e. everything after the tag
 * byte) into the caller's `data` buffer, reposts that slot's receive, and
 * returns the tag byte. `timeout` == -1 blocks forever (checking the
 * ESC-pressed flag each pass); otherwise waits up to `timeout` polling
 * passes.
 *
 * CORRECTED from an earlier pass at this function (see prior git history
 * if needed): `size`/`datatype` were swapped -- `size` was being used as
 * the tag bitmask and a fixed sizeof(...)-1 (399) was copied regardless
 * of the caller's real payload size, and the "block forever" test used
 * `datatype!=-1` as well as `timeout!=-1`. Re-derived from the raw
 * disassembly at 2d4c:01fa-039c:
 *   - `MOV DI,[BP+0xc]` (size) is used as the `memcpy` length a few lines
 *     later (`PUSH DI; ...; CALLF _memcpy`), not as a bitmask.
 *   - `TEST word ptr [BP+0xe],AX` (datatype, AX = the received tag byte)
 *     is the actual bitmask test, confirming `datatype` -- not `size` --
 *     gates which tags are accepted.
 *   - the "block forever" check (2d4c:0285-0294) is two sequential
 *     CMP-against--1 tests on the HIGH word (BP+0x12) then LOW word
 *     (BP+0x10) of the 32-bit `timeout` alone; `datatype` never
 *     participates in it.
 * The old, swapped version happened to behave correctly at 3 of its 4
 * call sites purely because both ends of the confusion canceled out (the
 * literal bitmask value was passed positionally where the mislabeled
 * body expected it) -- but it meant every payload copy pulled a fixed
 * 399 bytes into `data` regardless of the caller's real buffer size. For
 * recieve_ipx()'s frame-by-frame game-data packets that overruns the
 * caller's ~0x76-byte MAZE_LOG_STRUCT destination by ~280 bytes on every
 * single received packet in any actual (comms!=0) network game -- a
 * stack buffer overflow into main_loop()'s other locals, consistent with
 * the erratic movement/scoreboard corruption only seen in real IPX play.
 * All non-inert call sites (join()'s reply-wait, and both calls inside
 * recieve()) have been updated alongside this fix; see their own
 * comments for the corrected argument values.
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
            if (_rec_ecb[i].inUseFlag == 0 && (datatype & _rec_ipx[i].data[0]) != 0) {
                memcpy(data, &_rec_ipx[i].data[1], size);
                receive_packet(&_rec_ecb[i], &_rec_ipx[i], sizeof(IPX_HEADER));
                return (int) _rec_ipx[i].data[0];
            }
        }

        if (timeout != -1L) {
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

    net_dbg("join: start\r\n");

    if (init_net() == 0) {
        net_dbg("join: init_net FAIL\r\n");
        return -1;
    }

    _socket_no = 0x8001;
    memset(&_gl_tab, 0, sizeof(CON_INFO));
    memset(_rec_ecb, 0, sizeof(_rec_ecb));
    memset(_rec_ipx, 0, sizeof(_rec_ipx));

    pack_type = open_socket(_socket_no, &probe);
    net_dbg("join: socket %x -> %d\r\n", _socket_no, pack_type);
    if (pack_type != 0 && pack_type != 0xff) {
        net_dbg("join: socket FAIL\r\n");
        return -1;
    }

    recieve_int(1, NULL, 0, 0, 0L);

    _join_ipx.data[0] = 2;
    memcpy(&_join_ipx.data[1], _source_address, 10);
    broadcast_address(address);
    init_ipx_ecb_send(&_join_ecb, &_join_ipx, address, 0xb);
    send_packet(&_join_ecb, &_join_ipx, 0xb);
    net_dbg("join: broadcast sent\r\n");

    if (wait_for_ecb(&_join_ecb, 2000) == 0) {
        /* CORRECTED (2d4c:00e3-010e): the real pushes are size=0x38
           (0x37 = sizeof(CON_INFO) + 1 tag byte -- the max reply
           payload), datatype=5 (bitmask for tags 1=accept | 4=reject),
           timeout=0x4e20 (20000L). The previous version passed
           (size=5, datatype=20000, timeout=0L) -- the bitmask value (5)
           landed in the right spot by coincidence against the old,
           swapped recieve_int() body, but timeout=0L meant this call
           returned "nobody answered" after a single instant check,
           never giving a real peer time to reply. */
        pack_type = recieve_int(0, data, 0x38, 5, 20000L);
    } else {
        pack_type = 0;
        net_dbg("join: send never completed\r\n");
    }
    net_dbg("join: reply pack_type=%d\r\n", pack_type);

    if (pack_type == 4) {
        net_dbg("join: socket busy, retry\r\n");
        close_socket(_socket_no);
        _socket_no++;
        return join(num);
    }

    if (pack_type == 0) {
        /* nobody answered -- we're the first station */
        net_dbg("join: alone, host slot 0\r\n");
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
            net_dbg("join: id not in table\r\n");
            printf("Can't find our Id! Aborting");
            return -1;
        }
        net_dbg("join: accepted slot %d\r\n", _our_id);
    }

    *num = num_of_connections();
    net_dbg("join: done id=%d n=%d\r\n", _our_id, *num);
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

    net_dbg("jnu: request seen\r\n");

    claimed = 0;
    for (i = 0; i < 5; i++) {
        if (_gl_tab.inuse[i] == 0) {
            memcpy(_gl_tab.address[i], data, 10);
            _gl_tab.inuse[i] = 1;
            claimed = 1;
            net_dbg("jnu: claimed slot %d\r\n", i);
            break;
        }
    }
    if (!claimed) {
        net_dbg("jnu: table full\r\n");
    }

    get_next_address(next_address, _our_id);

    if (memcmp(next_address, data, 10) == 0) {
        net_dbg("jnu: reply tag=%d\r\n", claimed ? 1 : 4);
        _joinreply_ipx.data[0] = claimed ? 1 : 4;
        init_ipx_ecb_send(&_joinreply_ecb, &_joinreply_ipx, next_address, 0x38);
        memcpy(&_joinreply_ipx.data[1], &_gl_tab, sizeof(CON_INFO));
        send_packet(&_joinreply_ecb, &_joinreply_ipx, 0x38);
    } else {
        net_dbg("jnu: not our reply\r\n");
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
            net_dbg("disc: slot %d\r\n", i);
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

    /* CORRECTED (2d4c:0532-0546): real pushes are size=0xb (11, the
       join/disconnect payload -- a 10-byte address plus slack), datatype
       =0x12 (bitmask for tags 2=join-request | 0x10=disconnect),
       timeout=1L (one quick pass, non-blocking as the comment says). The
       previous version's (size=0x12, datatype=0, timeout=0L) happened to
       filter the right tags against the old swapped recieve_int() body
       (0x12 landed in the bitmask role by coincidence) but copied a
       fixed 399 bytes into `scratch` every call regardless. */
    do {
        pack_type = recieve_int(0, scratch, 0xb, 0x12, 1L);
        if (pack_type == 2) {
            join_new_user(scratch);
        } else if (pack_type == 0x10) {
            disc_curr_user(scratch);
        }
    } while (pack_type != 0);

    /* CORRECTED (2d4c:0577-058c): real pushes are size=<this function's
       own `size` parameter, forwarded as-is -- the caller's real
       destination capacity>, datatype=0x1a (bitmask for tags 2 | 8=data
       | 0x10), timeout=1L. The do/while re-issues this call immediately
       whenever it returns "nothing yet" (pack_type!=8), so a short
       per-call timeout plus outer retrying is how the original blocks
       "until data arrives" -- not one huge inner block. The previous
       version passed (size=0x1a, datatype=-1, timeout=-1L): the bitmask
       (0x1a) again landed right by coincidence, and datatype=-1 +
       timeout=-1 both being -1 tripped the old (also wrong) "block
       forever inside a single call" condition, so it happened to still
       wait indefinitely -- but every successful receive copied a fixed
       399 bytes into `data` regardless of the caller's real buffer size.
       For recieve_ipx()'s frame-by-frame calls `data` points into the
       middle of a ~0x76-byte MAZE_LOG_STRUCT on main_loop()'s stack, so
       that fixed-399-byte copy overran it by roughly 280 bytes on every
       single received game-data packet during real (comms!=0) network
       play -- the most likely source of the movement/firing/scoreboard
       corruption only seen there. */
    do {
        pack_type = recieve_int(0, data, size, 0x1a, 1L);
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

