/*
 * MAZECOMM.C
 *
 * Reconstructed from PACWARS.EXE via Ghidra decompilation/disassembly
 * (segment 27f9, real addresses 27f9:0008-009f), cross-checked against
 * the Borland TLINK debug symbol dump (PACWARS.TXT) for signatures.
 *
 * This is the thin session-management layer between MAZE.C's main_loop
 * and IPXCOMM.C's join()/recieve()/send_next()/disconnect() protocol
 * functions: every packet here is a whole MAZE_LOG_STRUCT (0x76 = 118
 * bytes, confirmed via disassembly -- a fixed size pushed as an
 * immediate at every call site, not sizeof(MAZE_LOG_STRUCT) computed by
 * the compiler), sent/received starting from its first real field
 * (&maze_log->type) since MAZE_LOG_STRUCT has no separate header to skip.
 *
 * _comms (340e:2742) gates every function here: 0 means single-player
 * (no IPX session), matching this project's existing PACWARS.H
 * declaration. Ghidra's decompiler rendered this as a raw `*(int*)0x2742`
 * in open_ipx()'s decompile text (rather than resolving the `_comms`
 * symbol like it did for the other three functions here) -- confirmed
 * via disassembly that it's the exact same address, so no different
 * meaning.
 */
#include "MAZECOMM.H"
#include "IPXCOMM.H"
#include "MAZESPT.H"

/*
 * Joins the network session (single station in single-player mode) and
 * sends this station's initial state to the others once the join
 * negotiation completes with fewer than 2 stations having answered yet
 * (27f9:0018-0073). _wstation (real address 340e:034d, already declared
 * extern in PACWARS.H) is set here from join()'s own return value --
 * confirmed via disassembly (`MOV [0x34d],AX` right after the join()
 * call), not obvious from the decompiled text alone.
 */
int open_ipx(MAZE_LOG_STRUCT far * maze_log)
{
    int num_connections;

    if (_comms == 0) {
        init_man(1, maze_log);
    } else {
        _wstation = join(&num_connections);
        if (_wstation == -1) {
            return 0;
        }
        if (num_connections < 2) {
            init_man(1, maze_log);
            send_next((char far *) &maze_log->type, 0x76);
        }
    }
    return 1;
}

int send_ipx(MAZE_LOG_STRUCT far * maze_log)
{
    if (_comms == 0) {
        return 1;
    }
    return send_next((char far *) &maze_log->type, 0x76);
}

int recieve_ipx(MAZE_LOG_STRUCT far * maze_log)
{
    if (_comms == 0) {
        return 1;
    }
    return recieve((char far *) &maze_log->type, 0x76);
}

/*
 * NOTE (27f9:0074-009f): when _comms != 0, this branch calls disconnect()
 * (a void function) and returns without ever assigning its own result --
 * the real compiled function returns whatever disconnect() happened to
 * leave in AX. This is a genuine quirk of the original binary (confirmed
 * via disassembly: no instruction sets AX again after the CALLF), not a
 * decompiler artifact, and is preserved as-is rather than "fixed" with a
 * made-up return value.
 */
int disconnect_ipx(MAZE_LOG_STRUCT far * maze_log)
{
    int result;

    if (_comms == 0) {
        result = 1;
    } else {
        disconnect((char far *) &maze_log->type, 0x76);
    }
    return result;
}

