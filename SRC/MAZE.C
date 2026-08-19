/*
 * MAZE.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZE.H"
#include "UTILS.H"
#include "MAZEUTIL.H"
#include "MVAGRAPH.H"
#include "MAZEREG.H"
#include "MAZESPT.H"
#include "DISPPIC.H"
#include "MAZEINIT.H"
#include "GRAPH256.H"
#include "MANEDIT.H"
#include "MAZEEDIT.H"
#include "HISCORE.H"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Forward declarations for local (non-exported) functions used in pacwars()
 * before their definitions appear later in this file. Without these, the
 * Borland compiler implicitly declares main_loop as returning int on its
 * first (call-site) sighting, then errors on the real `void main_loop(void)`
 * definition below as a conflicting redeclaration; key_poll needs one too
 * since its address is taken (as a function-pointer argument) rather than
 * called, which never triggers C's implicit-declaration-on-call fallback.
 */
void interrupt key_poll(void);
void main_loop(void);

/* storage for the shared globals declared extern in PACWARS.H */
int comms;
int pacman;
char curr_name[13];
unsigned int curr_score;
int esc;

void main(int argc, char *argv[])
{
    char pacman_img[128] = "PACMAN.IMG";
    char pacman2_img[128] = "PACMAN2.IMG";
    char pacwars_img[128] = "PACWARS.IMG";
    int si;
    int notitle = 0;
    int menu_choice = 0;
    int maze_hoff = 0;
    int maze_voff = 0;
    int saved_registered;

    /* uppercase every command-line argument up front */
    for (si = 1; si < argc; si++) {
        strupr(argv[si]);
    }

    decode_reg();

    if (argc == 2) {
        if (strcmp(argv[1], "SPRITES") == 0) {
            /* rebuild PACWARS.IMG by combining PACMAN.IMG + PACMAN2.IMG */
            get_filename(pacman_img);
            get_filename(pacman2_img);
            load_pic_files(pacman_img, pacman2_img);
            get_filename(pacwars_img);
            save_pic_file(pacwars_img);
            set_mode(3);
            exit(0);
        }
        if (strcmp(argv[1], "CONVERT") == 0) {
            printf("\nConverting...");
            conv_all_sprites();
            printf("\nFinished");
            exit(0);
        }
        if (strcmp(argv[1], "HISCORE") == 0) {
            if (init_hiscore() == -1) {
                printf("Cannot reset HISCORE.DAT\n");
            } else {
                printf("Hiscore Table Reset.\n");
            }
            exit(0);
        }
        if (strcmp(argv[1], "PACEDIT") == 0) {
            SetTextFont(2);
            load_all_sprites();
            alloc_maze_editor_mem();
            do {
                set_mode(0x13);
                clear_str((unsigned char far *) curr_name, 12);
                pacman = choose_edit_pacman(pacman) - 1;
                if (esc == 0) {
                    edit_pacman(pacman);
                }
            } while (esc == 0);
            set_mode(3);
            exit(0);
        }
        if (strcmp(argv[1], "PACPATH") == 0) {
            /*
             * Single-arg "PACWARS PACPATH" with no directory given:
             * print the usage message and fall through to a normal game
             * launch below, same as any other unrecognized single
             * argument. NOTE: the disassembly's exact jump target here
             * (14df:0295) lands a few bytes into what looks like a
             * neighboring instruction rather than a clean block start;
             * since every path from here converges on the same
             * observable behavior (print, then fall into the ON/OFF/
             * NOTITLE parsing below) this has no effect on the
             * reconstructed logic.
             */
            printf("FORMAT: PACWARS PACPATH <pacwars directory>\n");
            printf("Use .\\ for current directory");
        }
        /*
         * NOTE: the disassembly (14df:0297-02b9) does one more strcmp
         * here, comparing the last argument against "EDIT" -- but both
         * the equal and not-equal branches jump to the exact same
         * address, so the comparison's result is never actually used
         * for anything. Left out below as confirmed-dead code (most
         * likely a leftover from a removed or disabled flag).
         */
    } else if (argc == 3 && strcmp(argv[1], "PACPATH") == 0) {
        /* two-arg form: set the maze search path and exit immediately */
        set_maze_path(argv[2]);
        exit(0);
    }

    /* normal game launch: any remaining arguments are ON/OFF/NOTITLE flags */
    for (si = 1; si < argc; si++) {
        strupr(argv[si]);
        if (strcmp(argv[si], "OFF") == 0) {
            comms = 0;
        } else if (strcmp(argv[si], "ON") == 0) {
            comms = 1;
        } else if (strcmp(argv[si], "NOTITLE") == 0) {
            notitle = 1;
        }
    }

    if (notitle == 1) {
        printf("\nLoading... \n%d %d", 118, 118);
    } else {
        set_mode(0x13);
        get_filename(pacwars_img);
        load_pic_file(pacwars_img);
        /* wait for a keypress, checking every 15ms, for up to ~1.5s */
        for (si = 0; si < 100; si++) {
            delay(15);
            if (read_key() != 0) {
                break;
            }
        }
    }

    /* one-time setup, then show the menu and dispatch on the choice made */
    SetTextFont(2);
    load_all_sprites();
    setup_ir_sprite_buffer();
    load_font();
    alloc_maze_editor_mem();
    set_clip_window(0);
    set_mode(0x13);
    menu_choice = pacwars_menu(menu_choice);
    clear_str((unsigned char far *) curr_name, 12);

    while (esc == 0) {
        /*
         * NOTE: this is exactly the switch Ghidra's decompiler could not
         * resolve (it failed to decompile this function at all --
         * "Low-level Error: Overlapping input varnodes"). Reconstructed
         * from the raw jump table at 14df:0639 (6 words: 046D, 047F,
         * 058D, 04D5, 0535, 0570) and confirmed against the xrefs added
         * to each case target.
         */
        switch (menu_choice) {
        case 0:
            /* play a networked (IPX) game */
            set_clip_window(1);
            comms = 1;
            pacwars();
            set_clip_window(0);
            break;

        case 1:
            /* maze editor, looping until escape */
            saved_registered = reg->registered;
            reg->registered = 0;
            do {
                set_mode(0x13);
                choose_edit_maze(&maze_hoff, &maze_voff);  /* return value unused */
                if (esc == 0) {
                    edit_maze(maze_hoff, maze_voff);
                }
            } while (esc == 0);
            reg->registered = (char) saved_registered;
            break;

        case 2:
            /* back to the menu, nothing else to do */
            break;

        case 3:
            /* edit player name / character, looping until escape */
            saved_registered = reg->registered;
            reg->registered = 0;
            do {
                set_mode(0x13);
                clear_str((unsigned char far *) curr_name, 12);
                pacman = choose_pacman(pacman) - 1;
                if (esc == 0) {
                    edit_pacman(pacman);
                }
            } while (esc == 0);
            reg->registered = (char) saved_registered;
            break;

        case 4:
            /* reset high scores, with on-screen feedback */
            text256(112, 180, (unsigned char far *) "Resetting...", 14, 0);
            init_hiscore();
            delay(1000);
            text256(112, 180, (unsigned char far *) "         ", 14, 0);
            break;

        case 5:
            /* play a solo (non-networked) game */
            set_clip_window(1);
            comms = 0;
            pacwars();
            set_clip_window(0);
            break;
        }

        esc = 0;
        set_mode(0x13);
        menu_choice = pacwars_menu(menu_choice);
        clear_str((unsigned char far *) curr_name, 12);
    }

    set_mode(3);
    esc = 0;
    exit(0);
}

void pacwars(void)
{
    long session_start = 0L;
    long session_end = 10L;
    long elapsed;

    SetTextFont(2);
    set_mode(0x13);
    curr_score = 0;
    clear_str((unsigned char far *) curr_name, 12);
    pacman = choose_pacman(0) - 1;
    srand((unsigned int) time(NULL));

    for (;;) {
        set_key_vect(1, key_poll);

        /*
         * NOTE: Ghidra decompiled this as a fabricated bool expression
         * (`-1 < (int)(local_4 - (local_6 < uVar5))`, etc.) that doesn't
         * correspond to any real intermediate value in the disassembly.
         * The actual machine code (14df:06d9-06ee) does a plain 32-bit
         * signed subtraction of two `long`s and compares the result
         * against 10, which is what's reproduced below: registered
         * players only get pause_time() if their previous play session
         * (session_end - session_start, refreshed at the two time()
         * calls below) lasted under 10 -- likely seconds, given time()'s
         * resolution. Unregistered players on a comms (network) game
         * always get it.
         */
        if (comms == 1) {
            if (reg->registered == 1) {
                elapsed = session_end - session_start;
                if (elapsed < 10L) {
                    pause_time();
                }
            } else {
                pause_time();
            }
        }

        if (esc == 0) {
            /*
             * NOTE: Ghidra decompiled this call as `main_loop(0x1000)`,
             * but the disassembly at 14df:0709 pushes nothing before the
             * CALLF, and main_loop's own recovered signature takes no
             * parameters -- 0x1000 was a hallucinated argument, not a
             * real one.
             */
            time(&session_start);
            main_loop();
        }

        esc = 0;
        set_key_vect(0, NULL);
        set_mode(0x13);
        pacman = choose_pacman(pacman) - 1;
        time(&session_end);

        if (esc != 0) {
            break;
        }
    }

    set_mode(3);
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

