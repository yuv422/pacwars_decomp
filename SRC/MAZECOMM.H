/*
 * MAZECOMM.H
 *
 * Public prototypes exported by MAZECOMM.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MAZECOMM_H
#define MAZECOMM_H

#include "PACWARS.H"

int open_ipx(MAZE_LOG_STRUCT far * maze_log);
int send_ipx(MAZE_LOG_STRUCT far * maze_log);
int recieve_ipx(MAZE_LOG_STRUCT far * maze_log);
int disconnect_ipx(MAZE_LOG_STRUCT far * maze_log);

#endif /* MAZECOMM_H */
