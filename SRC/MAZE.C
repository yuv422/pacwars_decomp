/*
 * MAZE.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZE.H"

void main(int argc, char far * far * argv)
{
}

void pacwars(void)
{
}

void start_man(MAZE_LOG_STRUCT far * maze_log)
{
}

void main_loop(void)
{
}

int test_room_change(int far * hoff, int far * voff, int far * x, int far * y, int w, int h)
{
    return 0;
}

int test_shot_room_change(MAZE_LOG_PACKET far * status, int w)
{
    return 0;
}

void init_bullet(MAZE_LOG_STRUCT far * maze_log, int far * x_dir, int far * y_dir, int far * bullet, int temp_bullet, int dir, int ship)
{
}

void set_key_vect(int status, void interrupt (*key_func)(void))
{
}

void interrupt key_poll(void)
{
}

void interrupt key_pause(void)
{
}

void display_men(MAZE_LOG_STRUCT far * maze_log)
{
}

void restore_background(SPRITE_STRUCT far * sp)
{
}

void create_ir_sprite(int sprite_num, unsigned char far * sp_buff)
{
}

void create_warp_sprite(int factor, unsigned char far * sprite, unsigned char far * sp_buff)
{
}

void create_mini_sprite(int dir, int mini_sp, unsigned char far * sprite, unsigned char far * sp_buff)
{
}

void display_shots(MAZE_LOG_STRUCT far * maze_log)
{
}

void display_gold(MAZE_LOG_STRUCT far * maze_log, int offset)
{
}

void display_token(MAZE_LOG_STRUCT far * maze_log, int offset)
{
}

void clear_men(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
}

void clear_shots(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
}

void clear_gold(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
}

void clear_token(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
}

void display_sprite_restore(int x, int y, int sprite)
{
}

void clear_sprite_restore(int x, int y, int sprite)
{
}

void test_shots(MAZE_LOG_STRUCT far * maze_log, int dir)
{
}

void test_pos(MAZE_LOG_STRUCT far * maze_log)
{
}

void test_action(MAZE_LOG_STRUCT far * maze_log, int dir)
{
}

