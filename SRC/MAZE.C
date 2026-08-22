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
#include "HOME.H"
#include "MAZEDRAW.H"
#include "MAZECOMM.H"
#include "MAZEANIM.H"
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
void interrupt key_pause(void);
void main_loop(void);
void create_ir_sprite(int sprite_num, unsigned char far * sp_buff);
void create_warp_sprite(int factor, unsigned char far * sprite, unsigned char far * sp_buff);
void create_mini_sprite(int dir, int mini_sp, unsigned char far * sprite, unsigned char far * sp_buff);
void display_sprite_restore(int x, int y, int sprite);
void clear_sprite_restore(int x, int y, int sprite);
void test_action(MAZE_LOG_STRUCT far * maze_log, int dir);
int test_room_change(int far * hoff, int far * voff, int far * x, int far * y, int w, int h);
int test_shot_room_change(MAZE_LOG_PACKET far * status, int w);
void init_bullet(MAZE_LOG_STRUCT far * maze_log, int far * x_dir, int far * y_dir, int far * bullet, int temp_bullet, int dir, int ship);
void display_men(MAZE_LOG_STRUCT far * maze_log);
void display_shots(MAZE_LOG_STRUCT far * maze_log);
void display_gold(MAZE_LOG_STRUCT far * maze_log, int offset);
void display_token(MAZE_LOG_STRUCT far * maze_log, int offset);
void clear_men(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log);
void clear_shots(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log);
void clear_gold(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log);
void clear_token(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log);
void test_shots(MAZE_LOG_STRUCT far * maze_log, int dir);
void test_pos(MAZE_LOG_STRUCT far * maze_log);

/* storage for the shared globals declared extern in PACWARS.H */
/* Confirmed via read_memory at 340e:2742 in PACWARS.EXE: this is a real
 * static initializer baked into the data segment (0001), not just a
 * runtime default -- the original source starts up assuming a network
 * (IPX comms) game until/unless overridden. */
int comms = 1;
int pacman;
char curr_name[13];
unsigned int curr_score;
int esc;
int _hoffset;
int _voffset;

/*
 * Key-input state flags set by key_poll()/key_pause() and consumed
 * elsewhere in this file. Confirmed file-local via get_xrefs_to on every
 * one of these addresses in PACWARS.EXE (340e:7010/7014/700e/7024/3a2a/
 * 710a/7112/7110/7012) -- all references come from functions defined in
 * this module, so these are plain file-scope globals, not PACWARS.H
 * externs.
 */
/*
 * Locked-target index for homing missiles: -1/negative means "no lock",
 * otherwise an index into maze_log->status[]. Confirmed file-local (all
 * xrefs to 340e:036b resolve to functions in this module: init_bullet,
 * test_shots, and others still pending in this file) via get_xrefs_to.
 */
/* -1 is the "no lock" sentinel; confirmed as a real static initializer
 * (0xffff) at 340e:036b, not just BSS zero. */
int _selected = -1;

int _cur_left;
int _cur_right;
int _cur_up;
int _cur_down;
int _space;
int _ikey;
int _wkey;
int _vkey;
int _enter;
int _tkey;
int _skey;
int _gkey;

/*
 * Latches while LCtrl (scan 0x1D) or LAlt (0x38) is held, cleared on their
 * break codes (0x9D/0xB8); while set, key_poll() ignores every other
 * make code (14df:2ce6-2cee). Must be a persistent (not per-call) global
 * since it tracks state across separate keyboard-interrupt invocations;
 * confirmed file-local to key_poll via get_xrefs_to on 340e:3a1c.
 */
int _ctrl_alt_held;

/*
 * Nonzero while an IR/cloak-vision overlay is active (display_men() draws
 * a ghosted/cloak sprite for hidden ships in this mode, and
 * restore_background()/overlap_sprite() force attrib=1 -- fully lit --
 * for background tiles while it's set). Confirmed file-local to MAZE.C
 * (all xrefs to 340e:0363 resolve to functions in this module) via
 * get_xrefs_to.
 */
int _visible;

/*
 * Frame counter for the local player's death explosion animation (set to
 * 0x3c = 60 frames when hit by a shot in test_shots(), counted down
 * elsewhere in this module -- main()/pacwars() and main_loop()). Confirmed
 * file-local to MAZE.C via get_xrefs_to on 340e:0343.
 */
int _explode_count;

/*
 * Nonzero while a station's smart-bomb token effect is active; the effect
 * target/type/room live in maze_log->sbomb (SBOMB_PACKET). Confirmed
 * file-local to MAZE.C via get_xrefs_to on 340e:0365.
 */
int _sbomb;

/*
 * Token-effect duration constant (frames/ticks a picked-up drunk/glue
 * token effect lasts), plus the state for those two effects and the
 * "drunk driving" wobble effect: _drunk/_drunk_time track the drunk
 * effect's active flag and expiry time, _glue/_glue_time the glue
 * effect's. All confirmed file-local to MAZE.C via get_xrefs_to (every
 * reference resolves to test_pos/test_action, both in this file).
 */
/* Confirmed via read_memory at 340e:0351: a genuine static initializer
 * (0x000a = 10), not BSS zero. */
int _token_time = 10;
int _drunk;
long _drunk_time;
int _glue;
long _glue_time;

/*
 * "Invisible shot" power-up state: _ishot_count is the remaining charge
 * count (already declared extern in PACWARS.H, owned elsewhere), _ishot is
 * the current-frame active flag consumed by test_shots()'s hit-detection
 * gating and cleared on every explosion tick. Confirmed file-local to
 * MAZE.C via get_xrefs_to on 340e:0359 (every reference resolves to
 * main_loop, the only function in this file that reads/writes it).
 */
int _ishot;

/*
 * Expiry timestamp (maze_log->time units) for the local player's active
 * IR/cloak-visibility power-up, set by main_loop() when _vkey triggers it
 * and checked against maze_log->time to clear _visible again. Confirmed
 * file-local to MAZE.C via get_xrefs_to on 340e:3a22 (both the write and
 * read resolve to main_loop).
 */
long _visible_time;

/*
 * Far pointers to the raw real-mode interrupt vector table entries for
 * INT 9 (keyboard) and INT 0x3C (a free/user-definable vector). Each IVT
 * entry is a 4-byte far pointer living at absolute address (int_num * 4)
 * in segment 0; confirmed by read_memory at 340e:04ed/04f1 in
 * PACWARS.EXE, which decode as the far pointers 0000:0024 and 0000:00f0
 * respectively (9*4 = 0x24, 0x3c*4 = 0xf0). set_key_vect() uses these to
 * directly read/write vector-table entries instead of going through
 * getvect()/setvect() for the save/restore step; INT 0x3Ch is used as a
 * stash slot for the original INT 9 handler so key_poll() can chain to it.
 */
typedef void interrupt (*KEY_VECTOR)(void);
#define INT9_VECT_SLOT  ((KEY_VECTOR far *) MK_FP(0, 9 * 4))
#define INT3C_VECT_SLOT ((KEY_VECTOR far *) MK_FP(0, 0x3c * 4))

void main(int argc, char *argv[])
{
    char pacman_img[128] = "PACMAN.IMG";
    char pacman2_img[128] = "PACMAN2.IMG";
    char pacwars_img[128] = "PACWARS.IMG";
    int i;
    int notitle = 0;
    int menu_choice = 0;
    int maze_hoff = 0;
    int maze_voff = 0;
    int saved_registered;

    /* uppercase every command-line argument up front */
    for (i = 1; i < argc; i++) {
        strupr(argv[i]);
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
    for (i = 1; i < argc; i++) {
        strupr(argv[i]);
        if (strcmp(argv[i], "OFF") == 0) {
            comms = 0;
        } else if (strcmp(argv[i], "ON") == 0) {
            comms = 1;
        } else if (strcmp(argv[i], "NOTITLE") == 0) {
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
        for (i = 0; i < 100; i++) {
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

/*
 * Resets one player slot (_wstation, this station's player index) at the
 * start of a life/game: marks it connected, clears its sprite-packet
 * invisibility/offset fields, sets its starting position and warp state,
 * and seeds score/lives. Raw AND/OR bit-mask sequences at 14df:0786-07f8
 * mapped 1:1 to MAZE_LOG_PACKET bitfield member assignments (each
 * AND-only op is a plain "field = 0", each AND-then-OR pair is
 * "field = <the OR'd constant>", per the packed-bitfield store codegen
 * this project already established elsewhere).
 */
void start_man(MAZE_LOG_STRUCT far * maze_log)
{
    maze_log->connection[_wstation] = 1;
    maze_log->status[_wstation].invisible = 0;
    maze_log->status[_wstation].warp_factor = 7;
    maze_log->status[_wstation].x = 8;
    maze_log->status[_wstation].y = 8;
    maze_log->status[_wstation].hoffset = 0;
    maze_log->status[_wstation].voffset = 0;
    maze_log->score[_wstation] = 10;
    maze_log->men[_wstation] = 5;
}

/*
 * The core per-frame game loop for a single station's play session, run
 * once per call to pacwars() (14df:0816-575a, by far the largest function
 * in this module). Sets up the local player's starting sprite/position,
 * opens the IPX session, then loops forever (until _esc, the network-level
 * "session ending" flag, is set by some other station) doing: receive this
 * frame's merged network state, apply the local player's own gameplay
 * updates (movement, firing, pickups, power-ups, warp, explosion/respawn,
 * target-lock cycling), broadcast the result, then erase/redraw every
 * on-screen sprite that changed since the previous frame.
 *
 * Two full-size MAZE_LOG_STRUCT locals are used throughout: cur_log (the
 * live, currently-being-updated frame) and prev_log (a copy of the frame
 * before, used only so clear_men()/clear_shots()/clear_gold()/
 * clear_token()/restore_maze() know what to erase). Ghidra reports these
 * as three separate stack slots (auStack_11a[58], abStack_e0[60],
 * MStack_a4) because it sizes locals by the highest offset it sees
 * *directly* referenced from within main_loop's own instructions; most of
 * prev_log's fields are only ever touched indirectly, through pointers
 * passed into the four clear_*() calls, so Ghidra never saw enough direct
 * references to infer its true size. 58+60 = 118 = 0x76, exactly
 * sizeof(MAZE_LOG_STRUCT) (confirmed by the explicit `memset(&MStack_a4,
 * 0, 0x76)` at the top of the function) and status[5] (the struct's last
 * member) is exactly 60 bytes -- so abStack_e0 is really just
 * prev_log.status[], and auStack_11a is everything before it in the same
 * struct. Declared here as one proper prev_log local instead.
 *
 * A repeating `_memcpy(dst, <garbage-looking address>, unaff_SI)` pattern
 * (source expressions like `(char*)((int)&DAT_340e_0075 + 1)` or
 * `(char*)s_Borland_C_____Copyright...+8`, sizes lost to `unaff_SI`) shows
 * up at several points. Every one of these is either (a) immediately
 * followed by something that overwrites the same destination anyway (a
 * dead store -- e.g. the copy into cur_log.status[_wstation] right before
 * recieve_ipx(), which unconditionally overwrites all of cur_log a line
 * later), or (b) sitting where a self-copy would be a no-op and any other
 * source would corrupt already-correct, already-computed frame state that
 * every surrounding call (test_pos/test_shots/test_action/send_ipx) relies
 * on being intact. Both read as pure decompiler noise (the same
 * lost-immediate/lost-register-argument bug documented throughout this
 * project), so they're dropped here rather than guessed at. The one
 * genuine, load-bearing copy in this family -- refreshing the
 * prev_wstation_status snapshot used for the "my shot just got cleared"
 * score penalty below -- is kept, written as a plain struct assignment.
 *
 * local_2c (a "use the old animate_room()/maze_def() path instead of
 * room_animation()" switch) is initialized to 1 and never reassigned
 * anywhere in this function -- confirmed by reading every reference in the
 * full decompiled pseudocode. The local_2c==0 fallback path is therefore
 * dead code in this function (same kind of confirmed-dead branch already
 * documented and elided in main(), the "EDIT" strcmp comparison whose
 * result is never used); every one of the half-dozen call sites below just
 * calls room_animation()/clear_room_animation() unconditionally.
 *
 * Several small decompiler misreadings specific to this function, found by
 * hand-deriving the extraction formula `(<field> << (16-offset-width)) >>
 * (16-width)` for each bitfield compare against MAZE_LOG_PACKET's layout:
 *   - `(int)((byte)pPacked_bits[0] << 9) >> 9` is `status->invisible`
 *     (offset 0, width 7), not a raw byte compare -- used both to gate the
 *     ikey/"ishot" special-fire trigger (invisible==0) and its later
 *     expiry check (invisible==1).
 *   - the vertical shot room-wrap checks test `status->missile` (word at
 *     pPacked_bits+2, >>6&0xf), not `shot_dir` -- the original only wraps
 *     for missile values {0,1,7} (unguided/N/W) on the top edge and
 *     {3,4,5} (E/SE/S) on the bottom edge, leaving NE/SW/NW unhandled by
 *     this particular check; preserved as found rather than "fixed" to be
 *     symmetric.
 *   - home()'s first four arguments (for the in-flight homing-missile
 *     recompute, as opposed to init_bullet()'s initial call) resolve to
 *     the shot's own current position (shot_hoffset/shot_voffset/shot_x/
 *     shot_y), not the ship's -- makes sense, since by the time a missile
 *     is re-steering mid-flight it has moved away from the firing ship;
 *     the target arguments are the locked-on station's plain ship
 *     position (hoffset/voffset/x/y), matching init_bullet()'s usage.
 *   - the two ship-sprite-index position-carry snaps on a facing flip
 *     (`*(int*)(iVar8*0xb+0x3a16)`/`+0x3a18`) don't resolve to any real
 *     symbol by address arithmetic (0x3a16 is 0x18 below the established
 *     _sprites[0] base of 0x3a2e, and 0x18 isn't a multiple of
 *     sizeof(SPRITE_STRUCT)=0xb) -- reconstructed by symmetry with the
 *     *other* direction's flip, which resolves cleanly to
 *     `_sprites[cur_ship_sprite two slots away]` (`(SPRITE_STRUCT
 *     *)(_sprites+2)+iVar8`), i.e. "carry the old sprite index's position
 *     into the new one" in both directions.
 *   - every other AND-mask/OR-immediate sequence in this function targets
 *     exactly one MAZE_LOG_PACKET/GOLD_PACKET/SBOMB_PACKET bitfield's own
 *     bit range, so (per this project's established convention) those are
 *     translated directly to plain field assignments rather than
 *     reproduced as raw bit ops.
 *
 * The target-lock cycle (_tkey) and grenade "cooks off into a self-hit
 * smart bomb after 100 frames unthrown" (sbomb_arm_pending, tested at the
 * top of the *next* frame alongside test_action()) logic is rewritten from
 * gotos into structured loops/flags; behavior traced by hand and matches.
 */
void main_loop(void)
{
    MAZE_LOG_STRUCT cur_log;
    MAZE_LOG_STRUCT prev_log;
    MAZE_LOG_PACKET prev_wstation_status;
    MAZE_LOG_PACKET far * ws;
    int i;

    int ship_base;              /* local_4: facing-dependent ship sprite-set base */
    int default_shot_sprite;    /* local_6: fixed straight-shot sprite for this pacman */
    int my_shot_sprite;         /* local_8: current shot's sprite slot (set by init_bullet) */
    int cur_ship_sprite;        /* iVar8: ship_base + bob_frame, recomputed each frame */
    int bob_frame;               /* local_a: 0/1 walk-cycle toggle */
    int frame_tick;              /* local_c: 0..29 tick counter (bob + scoreboard refresh) */
    int facing;                  /* local_e: 0/1 facing-direction group, aka `dir` elsewhere */
    int shot_age;                 /* local_10: frames since the current shot was fired */
    int ishot_wait;                /* local_12: frames since invisible==1 (ishot) began */
    int cooldown_base;             /* local_14: base cooldown unit, always 5 */
    int warp_charge_pending;       /* local_1e: consume one _warp_count next frame */
    long action_time;              /* local_1a/local_1c: last warp-press/respawn timestamp */
    int warp_tick;                  /* local_20: frames since last warp_factor step */
    int warp_dir;                    /* local_22: +1/-1 direction for warp_factor stepping */
    long ishot_time;                  /* iStack_16/uStack_18: ishot-trigger timestamp */
    long shield_expiry;                /* local_24: shield effect expiry time */
    int x_dir, y_dir;                   /* uStack_26/iStack_28: home() steering out-params */
    int sbomb_arm_pending;               /* local_2a: arm a self-hit smart bomb next frame */
    int first_frame;                      /* local_2e: start_man() once, on the first frame */
    int reversed;                          /* drunk-effect direction reversal for this frame */
    int delta;

    ship_base = pacman * 4 + 4;
    default_shot_sprite = pacman * 2 + 0x2c;
    my_shot_sprite = default_shot_sprite;
    bob_frame = 0;
    frame_tick = 0;
    facing = 0;
    shot_age = 0;
    ishot_wait = 0;
    cooldown_base = 5;
    action_time = -10L;
    warp_charge_pending = 0;
    warp_tick = 0;
    warp_dir = -1;
    sbomb_arm_pending = 0;
    first_frame = 1;
    _selected = -1;
    cur_ship_sprite = ship_base;

    init_position(&_hoffset, &_voffset,
                  &_sprites[ship_base].spritex, &_sprites[ship_base].spritey);

    memset(&cur_log, 0, sizeof(MAZE_LOG_STRUCT));
    draw_maze();
    draw_scoreboard();

    if (open_ipx(&cur_log) == 0) {
        set_key_vect(0, NULL);
        set_mode(3);
        printf("\nError Joining");
        exit(0);
        return;
    }

    start_man(&cur_log);
    start_man(&prev_log);
    /* seed prev_log's copy of my own status with a value (127) that can
       never match a real invisible field, so the very first frame's
       comparisons against it behave predictably */
    prev_log.status[_wstation].invisible = 0x7f;
    room_animation(cur_log.sync, _hoffset, _voffset);

    for (;;) {
        if (_esc != 0) {
            if (recieve_ipx(&cur_log) == 0) {
                set_key_vect(0, NULL);
                set_mode(3);
                printf("\nError Getting Status");
                exit(0);
                return;
            }

            cur_log.connection[_wstation] = 0;
            curr_score = cur_log.score[_wstation];
            cur_log.score[_wstation] = 0;
            cur_log.hiscore[_wstation] = 0;
            cur_log.men[_wstation] = 0;

            ws = &cur_log.status[_wstation];
            ws->shot = 0;
            ws->grenade = 0;
            ws->missile = 0;
            ws->invisible = 0;

            if (cur_log.gold.man - 1 == _wstation) {
                cur_log.gold.man = 0;
            }
            cur_log.sbomb.present = 0;
            if (cur_log.hit_man == _wstation) {
                cur_log.hit_man = -1;
            }

            disconnect_ipx(&cur_log);

            _warp_count = 0;
            _ishot = 0;
            _ishot_count = 0;
            _missile_count = 0;
            _drunk = 0;
            _visible_count = 0;
            _shield_count = 0;
            _grenade_count = 0;
            _visible = 0;
            _sbomb = 0;
            _glue = 0;
            return;
        }

        cur_ship_sprite = ship_base + bob_frame;

        if (recieve_ipx(&cur_log) == 0) {
            set_key_vect(0, NULL);
            set_mode(3);
            printf("\nError Getting Status");
            exit(0);
            return;
        }

        ws = &cur_log.status[_wstation];

        /* my previous shot got cleared out from under me (destroyed by
           another station's shot) -- dock a point, but only for a plain
           unguided/non-grenade shot */
        if (prev_wstation_status.shot == 1 && ws->shot != prev_wstation_status.shot &&
            ws->missile == 0 && ws->grenade == 0) {
            add_score(_wstation, -1, &cur_log);
        }

        if (warp_charge_pending == 1) {
            warp_charge_pending = 0;
            _warp_count--;
        }

        prev_wstation_status = *ws;

        /* publish this frame's sprite index/room/position into the
           network packet */
        ws->sprite = cur_ship_sprite;
        ws->hoffset = _hoffset;
        ws->voffset = _voffset;
        ws->x = _sprites[cur_ship_sprite].spritex;
        ws->y = _sprites[cur_ship_sprite].spritey;

        cur_log.connection[_wstation] = 1;
        if (first_frame == 1) {
            start_man(&cur_log);
            first_frame = 0;
        }

        update_sync(&cur_log);
        update_time(&cur_log);

        if (_spiked == 1) {
            cur_log.men[_wstation]--;
            if (cur_log.hit_man == _wstation && cur_log.men[_wstation] == 0) {
                cur_log.hit_man = -1;
            }
            ws->invisible = facing * 6 + 2;
            ws->warp_factor = 0;
            _explode_count = 0x3c;
            _spiked = 0;
        }

        if (_sbomb == 1) {
            _sbomb = 0;
            cur_log.sbomb.present = 0;
        }

        if (sbomb_arm_pending == 1) {
            sbomb_arm_pending = 0;
            _sbomb = 1;
            cur_log.sbomb.present = 1;
            cur_log.sbomb.man = _wstation;
            cur_log.sbomb.hoffset = ws->hoffset;
            cur_log.sbomb.voffset = ws->voffset;
        }

        test_action(&cur_log, facing);
        test_shots(&cur_log, facing);
        test_pos(&cur_log);

        if (frame_tick == 0) {
            init_gold(&cur_log);
            init_token(&cur_log);
        }

        send_ipx(&cur_log);
        update_map_animation(cur_log.sync);

        clear_men(&cur_log, &prev_log);
        clear_shots(&cur_log, &prev_log);
        clear_gold(&cur_log, &prev_log);
        clear_token(&cur_log, &prev_log);
        clear_room_animation(cur_log.sync, _hoffset, _voffset);

        restore_maze(&prev_log);

        display_men(&cur_log);
        display_shots(&cur_log);
        display_gold(&cur_log, bob_frame);
        display_token(&cur_log, bob_frame);
        room_animation(cur_log.sync, _hoffset, _voffset);

        if (_explode_count == 0) {
            reversed = (_drunk == 1 && rand() % 3 == 0) ? 1 : 0;

            if ((reversed && _cur_down == 1) || (!reversed && _cur_left == 1)) {
                if (facing == 0) {
                    facing = 1;
                    ship_base += 2;
                    cur_ship_sprite += 2;
                    _sprites[cur_ship_sprite].spritex = _sprites[cur_ship_sprite - 2].spritex;
                    _sprites[cur_ship_sprite].spritey = _sprites[cur_ship_sprite - 2].spritey;
                } else if (_glue == 0) {
                    delta = testx(-1, &_sprites[cur_ship_sprite], _hoffset, _voffset);
                    _sprites[cur_ship_sprite].spritex -= delta;
                }
            }
            if ((reversed && _cur_up == 1) || (!reversed && _cur_right == 1)) {
                if (facing == 1) {
                    facing = 0;
                    ship_base -= 2;
                    cur_ship_sprite -= 2;
                    _sprites[cur_ship_sprite].spritex = _sprites[cur_ship_sprite + 2].spritex;
                    _sprites[cur_ship_sprite].spritey = _sprites[cur_ship_sprite + 2].spritey;
                } else if (_glue == 0) {
                    delta = testx(1, &_sprites[cur_ship_sprite], _hoffset, _voffset);
                    _sprites[cur_ship_sprite].spritex += delta;
                }
            }
            if (_glue == 0 && ((reversed && _cur_right == 1) || (!reversed && _cur_up == 1))) {
                delta = testy(-1, &_sprites[cur_ship_sprite], _hoffset, _voffset);
                _sprites[cur_ship_sprite].spritey -= delta;
            }
            if (_glue == 0 && ((reversed && _cur_left == 1) || (!reversed && _cur_down == 1))) {
                delta = testy(1, &_sprites[cur_ship_sprite], _hoffset, _voffset);
                _sprites[cur_ship_sprite].spritey += delta;
            }

            if (_ikey == 1 && ws->invisible == 0) {
                ishot_time = cur_log.time;
                if (cur_log.time - action_time >= (long) cooldown_base * 2) {
                    ws->invisible = 1;
                    ishot_wait = 0;
                    if (_ishot_count > 0) {
                        _ishot_count--;
                        _ishot = 1;
                    }
                }
            }

            if (_vkey == 1 && _visible_count > 0 && _visible == 0) {
                _visible = 1;
                _visible_time = cur_log.time + (long) cooldown_base * 2;
                _visible_count--;
                _vkey = 0;
            }

            if (_skey == 1 && _shield_count > 0 && ws->shield == 0) {
                ws->shield = 1;
                shield_expiry = cur_log.time + (long) cooldown_base * 2;
                _shield_count--;
                _skey = 0;
            }

            if (_gkey == 1 && _grenade_count > 0 && ws->grenade == 0 && ws->shot == 0 &&
                (_selected == -1 || (comms == 0 && _missile_count == 0))) {
                ws->grenade = 1;
                _gkey = 0;
            } else if (_gkey == 1 && _grenade_count > 0 && ws->grenade == 1 && ws->shot == 0 &&
                       (_selected == -1 || (comms == 0 && _missile_count == 0))) {
                ws->grenade = 0;
                _gkey = 0;
            }

            if (_drunk == 1 && cur_log.time >= _drunk_time) {
                _drunk = 0;
            }
            if (_visible == 1 && cur_log.time >= _visible_time) {
                _visible = 0;
            }
            if (ws->shield == 1 && cur_log.time >= shield_expiry) {
                ws->shield = 0;
            }
            if (_glue == 1 && cur_log.time >= _glue_time) {
                _glue = 0;
            }
        }

        if (ws->shot == 1) {
            shot_age++;

            if (ws->grenade == 0 || shot_age < 100) {
                if ((ws->missile == 0 && ws->grenade == 0 && shot_age > 99) || shot_age > 499) {
                    /* shot expired (unguided timeout) or hit the hard
                       500-frame cap regardless of type -- clear it and
                       drop any target lock */
                    ws->shot = 0;
                    ws->missile = 0;
                    show_selected(0, _selected);
                    _selected = -1;
                } else if (ws->grenade == 1 && shot_age == 0x32) {
                    ws->shot_dir = 0;
                } else if (ws->grenade == 1 && shot_age == 0x4b) {
                    ws->grenade = (ws->grenade + 1) & 7;
                } else {
                    /* in-flight: steer a homing missile, reposition the
                       shot sprite, test for room-wrap/wall-bounce */
                    if (ws->missile != 0) {
                        home(ws->shot_hoffset, ws->shot_voffset, ws->shot_x, ws->shot_y,
                             cur_log.status[_selected].hoffset, cur_log.status[_selected].voffset,
                             cur_log.status[_selected].x, cur_log.status[_selected].y,
                             &x_dir, &y_dir, _HSIZE - 1, _VSIZE - 1);

                        if (x_dir == 0 && y_dir == -1) { ws->missile = 1; }
                        else if (x_dir == 1 && y_dir == -1) { ws->missile = 2; }
                        else if (x_dir == 1 && y_dir == 0) { ws->missile = 3; }
                        else if (x_dir == 1 && y_dir == 1) { ws->missile = 4; }
                        else if (x_dir == 0 && y_dir == 1) { ws->missile = 5; }
                        else if (x_dir == -1 && y_dir == 1) { ws->missile = 6; }
                        else if (x_dir == -1 && y_dir == 0) { ws->missile = 7; }
                        else if (x_dir == -1 && y_dir == -1) { ws->missile = 8; }

                        ws->shot_x += x_dir;
                        ws->shot_y += y_dir;
                        ws->shot_dir = x_dir;
                    }

                    _sprites[my_shot_sprite].spritex = ws->shot_x;
                    _sprites[my_shot_sprite].spritey = ws->shot_y;

                    if (ws->shot == 1) {
                        test_shot_room_change(&cur_log.status[_wstation],
                                               _sprites[my_shot_sprite].spritew);
                    }

                    if ((ws->missile == 0 || ws->missile == 1 || ws->missile == 7) &&
                        ws->shot_y + (_sprites[my_shot_sprite].spriteh >> 1) == 1) {
                        ws->shot_voffset = (ws->shot_voffset != 0) ? ws->shot_voffset - 1 : _VSIZE - 1;
                        ws->shot_y = _max_y - _sprites[my_shot_sprite].spriteh;
                    } else if ((ws->missile == 3 || ws->missile == 4 || ws->missile == 5) &&
                               ws->shot_y + (_sprites[my_shot_sprite].spriteh >> 1) >= _max_y) {
                        ws->shot_voffset++;
                        if (ws->shot_voffset == _VSIZE) {
                            ws->shot_voffset = 0;
                        }
                        ws->shot_y = 0;
                    }

                    if (ws->missile == 0 &&
                        test_bounce(ws->shot_dir, &_sprites[my_shot_sprite],
                                    ws->shot_hoffset, ws->shot_voffset) == 1) {
                        ws->shot_dir = -ws->shot_dir;
                        ws->shot = 1;
                    }

                    if (ws->shot == 0) {
                        ws->missile = 0;
                    }
                    if (ws->shot == 0 && ws->grenade == 1) {
                        ws->shot_dir = 0;
                        ws->shot = 1;
                    }
                }
            } else {
                /* held a grenade >=100 frames without throwing it: it
                   "cooks off" -- clear the shot and arm a self-targeted
                   smart bomb for the top of next frame */
                sbomb_arm_pending = 1;
                if (ws->hoffset == _hoffset && ws->voffset == _voffset) {
                    _spiked = 1;
                }
                ws->shot = 0;
                ws->grenade = 0;
            }
        } else if (_space == 1 && ws->shot == 0 &&
                   (cur_log.score[_wstation] != 0 || ws->grenade != 0 ||
                    (_missile_count > 0 && _selected >= 0)) &&
                   (ws->invisible == 0 || _ishot == 1)) {
            shot_age = 0;
            init_bullet(&cur_log, &x_dir, &y_dir, &my_shot_sprite,
                        default_shot_sprite, facing, cur_ship_sprite);
            if (_grenade_count > 0 && ws->grenade == 1 && ws->shot == 1) {
                _grenade_count--;
            }
        }

        if (ws->warp_factor == 0) {
            if (_explode_count == 0 && _wkey == 1 && _warp_count > 0) {
                action_time = cur_log.time;
                ws->warp_factor = 6;
                warp_tick = 0;
                warp_dir = 1;
            }
        } else if (ws->warp_factor == 8) {
            init_position(&_hoffset, &_voffset,
                           &_sprites[cur_ship_sprite].spritex, &_sprites[cur_ship_sprite].spritey);
            draw_maze();
            room_animation(cur_log.sync, _hoffset, _voffset);
            ws->invisible = 0;
            _wkey = 0;
            warp_charge_pending = 1;
            ws->warp_factor--;
            warp_dir = -1;
            warp_tick = 0;
        } else {
            warp_tick++;
            if (warp_tick == 5) {
                ws->warp_factor += warp_dir;
                warp_tick = 0;
            }
        }

        if (test_room_change(&_hoffset, &_voffset,
                              &_sprites[cur_ship_sprite].spritex, &_sprites[cur_ship_sprite].spritey,
                              _sprites[cur_ship_sprite].spritew, _sprites[cur_ship_sprite].spriteh) != 0) {
            draw_maze();
            room_animation(cur_log.sync, _hoffset, _voffset);
        }

        if (_explode_count < 1) {
            frame_tick++;
            if (frame_tick == 0x1e) {
                bob_frame = (bob_frame != 1);
                _sprites[ship_base + bob_frame].spritex = _sprites[cur_ship_sprite].spritex;
                _sprites[ship_base + bob_frame].spritey = _sprites[cur_ship_sprite].spritey;
                frame_tick = 0;
            }
        } else {
            _explode_count--;
            _ishot = 0;
            if (_explode_count == 0) {
                if (cur_log.men[_wstation] == 0) {
                    _esc = 1;
                } else {
                    ws->shield = 0;
                    ws->grenade = 0;
                    action_time = cur_log.time;
                    init_position(&_hoffset, &_voffset, &_sprites[cur_ship_sprite].spritex,
                                   &_sprites[cur_ship_sprite].spritey);
                    draw_maze();
                    room_animation(cur_log.sync, _hoffset, _voffset);
                    ws->invisible = 0;
                    _drunk = 0;
                    _visible = 0;
                    _glue = 0;
                }
            } else if (_explode_count % 10 == 0) {
                ws->invisible++;
            }
        }

        /* cycle the locked-on target, skipping empty slots and myself */
        if (_tkey == 1 && (ws->missile != 0 || _missile_count > 0)) {
            _tkey = 0;
            if (_selected >= 0) {
                show_selected(0, _selected);
            }
            do {
                _selected++;
            } while (_selected < 5 &&
                     (cur_log.connection[_selected] == 0 || _selected == _wstation));
            if (_selected > 4) {
                _selected = -1;
            }
            if (_selected >= 0 && _selected < 5) {
                show_selected(1, _selected);
            }
        }

        if (frame_tick % 5 == 0) {
            fill_scoreboard(frame_tick / 5, &cur_log, bob_frame);
        }

        if (ws->invisible == 1) {
            ishot_wait++;
            if (ishot_wait == 0x32) {
                action_time = cur_log.time;
                if (cur_log.time - ishot_time >= (long) cooldown_base) {
                    ws->invisible = 0;
                    if (_ishot == 1) {
                        _ishot = 0;
                    }
                }
                ishot_wait = 0;
            }
        }
    }
}

/*
 * Wraps the viewport's room-grid position (hoff/voff, indices into the
 * HSIZE x VSIZE room grid) and the in-room pixel position (x/y) when a
 * sprite's bounding box (given its width/height w/h) crosses the edge of
 * the current room's viewport. Rewritten from the raw disassembly
 * (14df:255b-2667) into structured if/else-if -- the original binary
 * duplicates the "*x + w/2" (and "*y + h/2") sub-expression per branch and
 * uses goto-style fallthrough, but the four wrap cases (right, left, down,
 * up) are otherwise independent checks with no shared state beyond
 * `changed`, confirmed by tracing every branch target address by hand.
 * Returns 1 if either axis wrapped (a room change occurred), else 0.
 */
int test_room_change(int far * hoff, int far * voff, int far * x, int far * y, int w, int h)
{
    int changed = 0;

    if (*x + w / 2 >= _max_x) {
        (*hoff)++;
        if (*hoff == _HSIZE) {
            *hoff = 0;
        }
        *x = 2 - w / 2;
        changed = 1;
    } else if (*x + w / 2 == 1) {
        if (*hoff < 1) {
            *hoff = _HSIZE - 1;
        } else {
            (*hoff)--;
        }
        *x = _max_x - w / 2 - 2;
        changed = 1;
    }

    if (*y + h / 2 >= _max_y) {
        (*voff)++;
        if (*voff == _VSIZE) {
            *voff = 0;
        }
        *y = 0;
        changed = 1;
    } else if (*y + h / 2 == 1) {
        if (*voff <= 0) {
            *voff = _VSIZE - 1;
        } else {
            (*voff)--;
        }
        *y = _max_y - h;
        changed = 1;
    }

    return changed;
}

/*
 * Same room-wrap idea as test_room_change() but for a shot's own position
 * fields within its MAZE_LOG_PACKET (shot_x/shot_hoffset), gated on the
 * shot's direction (shot_dir: 1 = moving right, -1 = moving left).
 * Reconstructed from 14df:2668-277a. NOTE: preserved exactly as found --
 * the right-exit branch (shot_dir==1) falls through to `return 0` even
 * though it performs the same kind of wrap the left-exit branch (which
 * returns 1) does; this asymmetry is in the original binary (traced via
 * the JMP targets at 14df:26e9 vs 14df:2774) and is kept faithfully
 * rather than "fixed".
 */
int test_shot_room_change(MAZE_LOG_PACKET far * status, int w)
{
    if (status->shot_dir == 1 && _max_x <= status->shot_x + w) {
        status->shot_hoffset = (status->shot_hoffset + status->shot_dir) & 0xf;
        if (status->shot_hoffset == _HSIZE) {
            status->shot_hoffset = 0;
        }
        status->shot_x = 4;
    } else if (status->shot_dir == -1 && status->shot_x < 0) {
        if (status->shot_hoffset == 0) {
            status->shot_hoffset = (_HSIZE - 1) & 0xf;
        } else {
            status->shot_hoffset = (status->shot_hoffset + status->shot_dir) & 0xf;
        }
        status->shot_x = _max_x - w - 4;
        return 1;
    }
    return 0;
}

/*
 * Fires a bullet/missile from ship's own maze_log status slot. If a target
 * is locked on (_selected >= 0) and missiles remain, home() steers the
 * shot toward it (x_dir/y_dir are out-params filled in by home(), then
 * copied into shot_dir); otherwise it just goes straight in `dir`.
 * Positions the shot sprite at the firing ship's edge, checks for an
 * immediate spawn-into-a-wall collision (only for unguided/no-target
 * shots, matching the original's gating -- homing shots skip this check,
 * confirmed via the raw JG at 14df:2b0a bypassing the isblock() call
 * entirely), and finally either consumes a missile or, if out of ammo,
 * docks a point when no grenade is held.
 *
 * Reconstructed from 14df:277b-2bee. Bitfield mask/OR sequences mapped to
 * MAZE_LOG_PACKET members using the same byte-offset scheme verified
 * elsewhere in this file (status[] stride 0xc, struct base +0x3a);
 * argument order for home() confirmed by tracing all 14 PUSHes (10 plain
 * words + 2 far pointers = the 0x1c/28-byte stack cleanup) back to front
 * against HOME.H's declared parameter order.
 */
void init_bullet(MAZE_LOG_STRUCT far * maze_log, int far * x_dir, int far * y_dir, int far * bullet, int temp_bullet, int dir, int ship)
{
    MAZE_LOG_PACKET far * status = &maze_log->status[_wstation];
    MAZE_LOG_PACKET far * target;

    if (comms == 0) {
        _selected = 1;
    }

    if (_missile_count < 1 || _selected < 0) {
        /* No usable lock (or no missiles left): fire straight in `dir`
           rather than homing. */
        *bullet = temp_bullet;
        status->missile = 0;
        status->shot_dir = (dir == 0) ? 1 : -1;
    } else {
        /* Homing shot: steer toward the locked-on target. */
        target = &maze_log->status[_selected];
        home(status->hoffset, status->voffset, status->x, status->y,
             target->hoffset, target->voffset, target->x, target->y,
             x_dir, y_dir, _HSIZE - 1, _VSIZE - 1);

        status->shot_dir = *x_dir;
        status->missile = (*x_dir == 1) ? 3 : 7;
        /* 0x4d6: base sprite index for the missile graphic set, offset by
           the direction-selected missile variant computed above. */
        *bullet = status->missile + 0x4d6;
    }

    status->shot = 1;
    status->shot_hoffset = _hoffset & 0xf;
    status->shot_voffset = _voffset & 0xf;

    status->shot_x = _sprites[ship].spritex;
    if (status->shot_dir == -1) {
        status->shot_x -= _sprites[*bullet].spritew;
    } else {
        status->shot_x += _sprites[ship].spritew;
    }
    status->shot_y = _sprites[ship].spritey + _sprites[ship].spriteh - _sprites[*bullet].spriteh;

    _sprites[*bullet].spritex = status->shot_x;
    _sprites[*bullet].spritey = status->shot_y;

    if (_missile_count < 1 || _selected < 0) {
        if (isblock(&_sprites[*bullet], status->shot_hoffset, status->shot_voffset) > 0) {
            /* Spawned directly inside a wall -- cancel the shot. */
            status->shot = 0;
            status->missile = 0;
            return;
        }
    }

    test_shot_room_change(status, _sprites[*bullet].spritew);
    if (_missile_count < 1 || _selected < 0) {
        if (status->grenade == 0) {
            maze_log->score[_wstation]--;
        }
    } else {
        _missile_count--;
    }
}

/*
 * Installs (status == 1) or removes (status == 0) the keyboard poll ISR.
 * On install: stashes the current INT 9 handler into the free INT 0x3Ch
 * vector slot (a raw 4-byte IVT copy, 14df:2c06-2c19) so the new handler
 * can chain to the original via "int 3Ch", installs key_func as the INT 9
 * handler via setvect(), and zeroes all key-state flags. On restore: copies
 * the stashed handler out of the INT 0x3Ch slot back into INT 9's slot via
 * another raw IVT write (14df:2c68-2c7b) -- functionally identical to
 * setvect(9, <the handler saved above>), just performed as a direct
 * pointer write instead of the library call, matching the original binary.
 */
void set_key_vect(int status, void interrupt (*key_func)(void))
{
    if (status == 1) {
        *INT3C_VECT_SLOT = *INT9_VECT_SLOT;

        setvect(9, key_func);

        _cur_left = 0;
        _cur_right = 0;
        _cur_up = 0;
        _cur_down = 0;
        _space = 0;
        _ikey = 0;
        _wkey = 0;
        _vkey = 0;
        _enter = 0;
    } else if (status == 0) {
        *INT9_VECT_SLOT = *INT3C_VECT_SLOT;
    }
}

/*
 * INT 9 (keyboard) ISR installed by set_key_vect() while accepting player
 * input: reads the raw scan code from the keyboard controller (port
 * 0x60), updates one of the key-state flags above for the codes this
 * game cares about (a make code sets its flag to 1 and `handled` to 1;
 * its break code, make+0x80, clears the flag), then always chains to the
 * previously installed handler via "int 3Ch" (see set_key_vect's
 * comment), and -- faithfully preserving what looks like a quirk in the
 * original binary rather than "fixing" it -- additionally calls
 * key_pause() when a key WAS handled, which redundantly re-reads port
 * 0x60 and chains via "int 3Ch" a second time (14df:2ef0-2ef9: a direct
 * CALLF to key_pause, which Borland's `interrupt` calling convention
 * supports via an automatically-generated dummy-flags push, matching the
 * "PUSH 0" immediately before the call).
 *
 * Reconstructed from the raw disassembly at 14df:2c81-2f09 (Ghidra's
 * decompiler fails on this function with "Cannot marshal address space:
 * NO ADDRESS", so there is no decompiled pseudocode to cross-check
 * against). The scan-code dispatch is a mix of an explicit comparison
 * tree and, for codes 0x91-0xA2, a real CS-relative jump table at
 * 14df:2f0a (read via read_memory and decoded by hand, 18 entries); both
 * forms reduce to the same set of (scan code -> flag) assignments below,
 * written here as the equivalent switch the compiler most plausibly
 * optimized into that mixed tree/table form. The `interrupt` keyword
 * makes the compiler generate the register save/restore, DS reload, and
 * IRET seen at 2c81-2c8f/2eff-2f09 automatically.
 */
void interrupt key_poll(void)
{
    unsigned int scancode;
    int handled;

    scancode = inportb(0x60);
    handled = 0;

    if (scancode == 0x45 || scancode == 0xe1) {
        outportb(0x60, 0x9e);
    }

    if (scancode == 0x38 || scancode == 0x1d) {
        _ctrl_alt_held = 1;
    } else if (scancode == 0xb8 || scancode == 0x9d) {
        _ctrl_alt_held = 0;
    }

    if (_ctrl_alt_held == 0 || (scancode & 0x80) != 0) {
        switch (scancode) {
        case 0x01: esc = 1;        handled = 1; break;
        case 0x81: esc = 0;                     break;
        case 0x11: _wkey = 1;      handled = 1; break;
        case 0x91: _wkey = 0;                   break;
        case 0x14: _tkey = 1;      handled = 1; break;
        case 0x94: _tkey = 0;                   break;
        case 0x17: _ikey = 1;      handled = 1; break;
        case 0x97: _ikey = 0;                   break;
        case 0x1c: _enter = 1;     handled = 1; break;
        case 0x9c: _enter = 0;                  break;
        case 0x1f: _skey = 1;      handled = 1; break;
        case 0x9f: _skey = 0;                   break;
        case 0x22: _gkey = 1;      handled = 1; break;
        case 0xa2: _gkey = 0;                   break;
        case 0x2f: _vkey = 1;      handled = 1; break;
        case 0xaf: _vkey = 0;                   break;
        case 0x39: _space = 1;     handled = 1; break;
        case 0xb9: _space = 0;                  break;
        case 0x48: _cur_up = 1;    handled = 1; break;
        case 0xc8: _cur_up = 0;                 break;
        case 0x4b: _cur_left = 1;  handled = 1; break;
        case 0xcb: _cur_left = 0;               break;
        case 0x4d: _cur_right = 1; handled = 1; break;
        case 0xcd: _cur_right = 0;              break;
        case 0x50: _cur_down = 1;  handled = 1; break;
        case 0xd0: _cur_down = 0;               break;
        default:
            break;
        }
    }

    geninterrupt(0x3c);
    if (handled == 1) {
        key_pause();
    }
}

/*
 * Trivial INT 9 handler installed while the game is paused: reads and
 * acknowledges the scan code (sending the keyboard controller's re-enable
 * byte 0x9E when the code is the Pause/NumLock-related 0x45 or its E1
 * prefix 0xE1, matching the real BIOS-level keyboard ack sequence for
 * those codes) and otherwise just chains straight to the previously
 * installed handler via "int 3Ch" (see set_key_vect's comment) without
 * touching any of the key-state globals key_poll() maintains. The
 * `interrupt` keyword makes the Borland compiler generate the register
 * save/restore and DS reload seen at 14df:2f2e-2f3c and the trailing IRET
 * at 2f67 automatically -- only the body (2f3e-2f5c) is reconstructed
 * here.
 */
void interrupt key_pause(void)
{
    unsigned int scancode;

    scancode = inportb(0x60);
    if (scancode == 0x45 || scancode == 0xe1) {
        outportb(0x60, 0x9e);
    }
    geninterrupt(0x3c);
}

/*
 * Draws every connected player's ship sprite that's visible in the
 * current room (hoffset/voffset match the viewport). Picks the sprite
 * frame: a special "spawn" frame (0) for the local player while
 * respawn-invisible, the normal ship frame while alive, or a death
 * animation frame indexed off 0x45d while invisible >= 2. Copies that
 * frame's bitmap into a scratch buffer, optionally IR-ghosts it, overlays
 * shield/grenade icons and a warp-in-progress overlay, then restores the
 * background under it and blits it.
 *
 * Reconstructed from 14df:2f68-330e. Ghidra's decompile of this function
 * is mostly trustworthy for control flow/arithmetic but renders three of
 * the bitfield checks wrong (byte-level literal compares instead of the
 * narrower field extracts the raw disassembly actually performs) --
 * cross-checked and corrected here:
 *   - decompiled `pPacked_bits[3] == ' '` (0x20) is actually just
 *     `status->shield == 1` (14df:31a8-31b5: SHR 5/AND 7 extracts only
 *     the 3-bit shield field, not a full-byte compare).
 *   - decompiled `pPacked_bits[1] != 0` is actually `warp_factor > 0`
 *     (14df:3272-327e: SHR 4/AND 0xf extracts only the top nibble).
 *   - the two CONCAT22(unaff_SS, sp_buff)-style pointer args throughout
 *     are all just &sp_buff (a local array's address in large model),
 *     per this project's established CONCAT22/unaff_SS convention.
 */
void display_men(MAZE_LOG_STRUCT far * maze_log)
{
    MAZE_LOG_PACKET far * status;
    unsigned char sp_buff[100];
    int i;
    int sprite_num;
    int death_frame;
    int dir;
    int temp_visible;

    for (i = 0; i < 5; i++) {
        status = &maze_log->status[i];

        if (maze_log->connection[i] > 0 &&
            status->invisible >= 0 &&
            (status->invisible != 1 || i == _wstation || _visible == 1) &&
            status->hoffset == _hoffset &&
            status->voffset == _voffset) {

            death_frame = 0;
            if (i == _wstation && status->invisible == 1) {
                sprite_num = 0;
            } else if (status->invisible < 2) {
                sprite_num = status->sprite;
            } else {
                sprite_num = ((status->sprite - 4) >> 2) * 12 + status->invisible + 0x45d;
                death_frame = 1;
            }

            _sprites[0x4e0].spritex = status->x;
            _sprites[0x4e0].spritey = status->y;
            _sprites[0x4e0].spritew = _sprites[sprite_num].spritew;
            _sprites[0x4e0].spriteh = _sprites[sprite_num].spriteh;
            memcpy(sp_buff, _sprites[sprite_num].sprite,
                   (unsigned int) _sprites[sprite_num].spritew * _sprites[sprite_num].spriteh);

            if (_visible == 1) {
                create_ir_sprite(sprite_num, sp_buff);
            }

            dir = (((status->sprite - 4) & 3) > 1) ? 1 : 0;

            if (status->shield == 1) {
                create_mini_sprite(dir, 0x458, sp_buff, sp_buff);
            }
            if (status->grenade == 1 && status->shot == 0) {
                create_mini_sprite(dir, 0x45b, sp_buff, sp_buff);
            }
            if (status->warp_factor > 0) {
                create_warp_sprite(status->warp_factor, sp_buff, sp_buff);
            }

            if (death_frame) {
                temp_visible = _visible;
                _visible = 1;
            }
            _sprites[0x4e0].sprite = sp_buff;
            restore_background(&_sprites[0x4e0]);
            if (death_frame) {
                _visible = temp_visible;
            }
            display_sprite(0x4e0);
        }
    }
}

/*
 * Redraws whatever maze background block sprites lie under sp's bounding
 * box (called before a moving sprite is drawn at its new position, so the
 * old position's background is repainted first). Walks the 8x8-pixel
 * block grid cells sp overlaps; for each cell that has a wall/floor block
 * defined (maze->def[row][col] != 0), positions the corresponding block
 * sprite (index def[row][col] + 0x40, i.e. into the block-sprite range)
 * at that cell and composites it under sp via overlap_sprite(), using the
 * room's per-cell lighting attribute (amaze->def[row][col], darkened by 1
 * unless _visible/IR mode forces full brightness).
 *
 * Reconstructed from 14df:330f-34d1; matches the same maze_def()/
 * attrib_maze_def() -> MAZE_STRUCT far * idiom already established
 * elsewhere in this project (MAZEDRAW.C, MAZEEDIT.C, MAZESPT.C).
 */
void restore_background(SPRITE_STRUCT far * sp)
{
    MAZE_STRUCT far * maze;
    MAZE_STRUCT far * amaze;
    unsigned int right, bottom;
    int col0, row0, cols, rows;
    int row, col;
    int block;
    int attrib;

    maze = (MAZE_STRUCT far *) maze_def(_hoffset, _voffset);
    amaze = (MAZE_STRUCT far *) attrib_maze_def(_hoffset, _voffset);

    right = sp->spritex + sp->spritew;
    bottom = sp->spritey + sp->spriteh;
    col0 = sp->spritex / 8;
    row0 = sp->spritey / 8;
    cols = (int) right / 8 - col0 + ((right & 7) != 0);
    rows = (int) bottom / 8 - row0 + ((bottom & 7) != 0);

    if (cols > 0 && rows > 0) {
        for (row = row0; row < row0 + rows; row++) {
            for (col = col0; col < col0 + cols; col++) {
                if (row >= 0 && col >= 0 && row < 25 && col < 30 && maze->def[row][col] != 0) {
                    attrib = amaze->def[row][col];
                    if (attrib > 0) {
                        attrib--;
                    }
                    if (_visible == 1) {
                        attrib = 1;
                    }
                    block = maze->def[row][col] + 0x40;
                    _sprites[block].spritey = row << 3;
                    _sprites[block].spritex = col << 3;
                    overlap_sprite(sp, &_sprites[block], attrib);
                }
            }
        }
    }
}

/*
 * Renders sprite_num's bitmap into sp_buff with an "IR static" look:
 * wherever the source pixel is opaque (nonzero), substitutes the
 * corresponding pixel from one of the 16 pre-randomized noise buffers in
 * _ir_sprite_buf[] (setup_ir_sprite_buffer(), MAZESPT.C); transparent
 * source pixels stay transparent. The noise buffer is picked once
 * (rand() % 16) and held fixed for the whole 256-byte copy.
 *
 * Reconstructed from 14df:34d2-3530. The stub's declared return type is
 * void; Ghidra's decompile infers a bogus `int` return (whatever was
 * left in AX from the last loop iteration), a known decompiler artifact
 * for void functions with no explicit return value -- not preserved.
 */
void create_ir_sprite(int sprite_num, unsigned char far * sp_buff)
{
    int buf_idx;
    int i;

    buf_idx = rand() % 16;

    for (i = 0; i < 0x100; i++) {
        if (_sprites[sprite_num].sprite[i] == 0) {
            sp_buff[i] = 0;
        } else {
            sp_buff[i] = _ir_sprite_buf[buf_idx].sprite[i];
        }
    }
}

/*
 * Renders a "warping in/out" static effect into sp_buff from `sprite`:
 * each opaque source pixel has a 1-in-(factor*3) chance of surviving into
 * sp_buff (rand() % (factor*3) == 0), otherwise (or if factor*3 >= 8, or
 * the source pixel is transparent) sp_buff's pixel is cleared to 0.
 * Reconstructed from 14df:3531-358e; the factor*3 < 8 guard is a loop
 * invariant the original computed once (into DI) but re-checked every
 * iteration, preserved as-is.
 */
void create_warp_sprite(int factor, unsigned char far * sprite, unsigned char far * sp_buff)
{
    int threshold;
    int i;

    threshold = factor * 3;

    for (i = 0; i < 0x100; i++) {
        if (threshold < 8 && sprite[i] != 0) {
            if (rand() % threshold == 0) {
                sp_buff[i] = sprite[i];
            } else {
                sp_buff[i] = 0;
            }
        } else {
            sp_buff[i] = 0;
        }
    }
}

/*
 * Starts sp_buff as a full copy of the fixed-size 16x16 ship sprite
 * `sprite`, then overlays an 8x8 status icon (mini_sp -- e.g. the shield
 * icon 0x458 or grenade icon 0x45b used by display_men()) in the
 * bottom-left or bottom-right corner depending on facing direction
 * (`dir`), skipping transparent (0) icon pixels so the ship shows
 * through around the icon's silhouette.
 *
 * Reconstructed from 14df:358f-3619. Ghidra's decompile garbles the
 * memcpy() call's arguments entirely (CONCAT22(unaff_DI,(char*)0x100) as
 * a "source" makes no sense); the raw disassembly's PUSH order at
 * 35a7-35b6 (0x100, then sprite's segment:offset, then sp_buff's
 * segment:offset, matching this function's own BP+0xa/BP+0xe parameter
 * slots) confirms it's simply memcpy(sp_buff, sprite, 0x100).
 */
void create_mini_sprite(int dir, int mini_sp, unsigned char far * sprite, unsigned char far * sp_buff)
{
    int pos;
    int sh_pos;
    int row, col;
    unsigned char pixel;

    memcpy(sp_buff, sprite, 0x100);

    pos = (dir == 0 ? 7 : 1) + 0x70;
    sh_pos = 0;
    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            pixel = _sprites[mini_sp].sprite[sh_pos];
            sh_pos++;
            if (pixel != 0) {
                sp_buff[pos] = pixel;
            }
            pos++;
        }
        pos += 8;
    }
}

/*
 * Draws every station's in-flight shot that's in the current room
 * (shot_hoffset/shot_voffset match the viewport). Picks a sprite: for
 * unguided shots (missile==0) a small 2-frame "bolt" animation (base
 * 0x2c, +1 for the reverse-direction frame) normally, or a grenade-icon
 * pair (0x45b/0x45d, selected by the grenade field's range) when a
 * grenade is active; for guided missiles, the direction-selected missile
 * sprite (missile + 0x4d6, matching init_bullet()'s convention). Unguided
 * shots use display_sprite_restore() (erase-old/draw-new), while missiles
 * just reposition and OR-blit directly.
 *
 * Reconstructed from 14df:361a-3842; unlike display_men(), Ghidra's
 * decompile of this function correctly resolves all the bitfield
 * extracts (cross-checked against the raw disassembly's SHL/SAR/SHR/AND
 * sequences at each site), so it's followed closely here.
 */
void display_shots(MAZE_LOG_STRUCT far * maze_log)
{
    MAZE_LOG_PACKET far * status;
    int sprite;
    int i;
    int missile;
    int grenade;

    for (i = 0; i < 5; i++) {
        status = &maze_log->status[i];

        if (status->shot == 1 &&
            status->shot_hoffset == _hoffset &&
            status->shot_voffset == _voffset) {

            missile = status->missile;
            if (missile == 0) {
                grenade = status->grenade;
                if (grenade == 0) {
                    sprite = ((status->sprite - 4) >> 2) * 2 +
                             (status->shot_dir == -1 ? 1 : 0) + 0x2c;
                } else if (grenade < 2) {
                    sprite = (status->shot_dir == -1 ? 1 : 0) + 0x45b;
                } else {
                    sprite = (status->shot_dir == -1 ? 1 : 0) + 0x45d;
                }
            } else {
                sprite = missile + 0x4d6;
            }

            if (missile == 0) {
                display_sprite_restore(status->shot_x, status->shot_y, sprite);
            } else {
                _sprites[sprite].spritex = status->shot_x;
                _sprites[sprite].spritey = status->shot_y;
                or_sprite(sprite);
            }
        }
    }
}

/*
 * Draws the room's gold pickup, if present and in the current viewport,
 * via display_sprite_restore() (erase-old/draw-new, same as unguided
 * shots) using sprite index offset+0x429. Reconstructed from
 * 14df:3843-38a9; field bit-extracts cross-checked against GOLD_PACKET's
 * established layout in PACWARS.H and matched cleanly.
 */
void display_gold(MAZE_LOG_STRUCT far * maze_log, int offset)
{
    int sprite;

    sprite = offset + 0x429;

    if (maze_log->gold.present == 1 &&
        maze_log->gold.hoffset == _hoffset &&
        maze_log->gold.voffset == _voffset) {
        display_sprite_restore(maze_log->gold.col << 3, maze_log->gold.row << 3, sprite);
    }
}

/*
 * Draws the room's warp token, if present and in the current viewport.
 * Sprite index = token.type * 2 + offset + 0x42c (a 2-frame animation per
 * token type). Unlike display_gold(), this copies the bitmap into a
 * scratch buffer and goes through the full restore_background()/
 * display_sprite() pipeline (like display_men()/display_token()'s
 * sibling functions) because it can composite create_warp_sprite()'s
 * static overlay on top when the token has an active warp_factor.
 * Reconstructed from 14df:38aa-3a13; field offsets cross-checked against
 * TOKEN_PACKET's established layout in PACWARS.H.
 */
void display_token(MAZE_LOG_STRUCT far * maze_log, int offset)
{
    unsigned char sp_buff[100];
    int sprite;

    if (maze_log->token.present == 1 &&
        maze_log->token.hoffset == _hoffset &&
        maze_log->token.voffset == _voffset) {

        sprite = maze_log->token.type * 2 + offset + 0x42c;

        _sprites[0x4e0].spritex = maze_log->token.col << 3;
        _sprites[0x4e0].spritey = maze_log->token.row << 3;
        _sprites[0x4e0].spritew = _sprites[sprite].spritew;
        _sprites[0x4e0].spriteh = _sprites[sprite].spriteh;
        memcpy(sp_buff, _sprites[sprite].sprite,
               (unsigned int) _sprites[sprite].spritew * _sprites[sprite].spriteh);
        _sprites[0x4e0].sprite = sp_buff;

        if (maze_log->token.warp_factor > 0) {
            create_warp_sprite(maze_log->token.warp_factor, sp_buff, sp_buff);
        }

        restore_background(&_sprites[0x4e0]);
        display_sprite(0x4e0);
    }
}

/*
 * Erases (via clear_sprite_restore()) another station's ship sprite from
 * its OLD position (maze_log) when the new frame (new_log) shows it
 * changed -- connection state, invisible state, or room (hoffset/voffset)
 * differ -- and the old position was in the current viewport. Skips the
 * local station (_wstation, handled by display_men() drawing over it
 * fresh each frame) and any station not newly invisible-valid.
 * Reconstructed from 14df:3a14-3b98; matches the decompiled pseudocode
 * closely once the stack-offset-derived parameter binding (new_log at
 * BP+6, maze_log at BP+0xa, matching the stub's declared order) is
 * confirmed against every field access site.
 */
void clear_men(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
    MAZE_LOG_PACKET far * old_status;
    MAZE_LOG_PACKET far * new_status;
    int i;
    int changed;

    for (i = 0; i < 5; i++) {
        if (maze_log->connection[i] != 0 && i != _wstation) {
            old_status = &maze_log->status[i];
            new_status = &new_log->status[i];

            if (new_status->invisible >= 0) {
                changed = (maze_log->connection[i] != new_log->connection[i]) ||
                          (old_status->invisible != new_status->invisible) ||
                          (old_status->hoffset != new_status->hoffset) ||
                          (old_status->voffset != new_status->voffset);

                if (changed &&
                    old_status->hoffset == _hoffset &&
                    old_status->voffset == _voffset) {
                    clear_sprite_restore(old_status->x, old_status->y, old_status->sprite);
                }
            }
        }
    }
}

/*
 * Erases a station's unguided-shot sprite from its OLD position
 * (maze_log) when the new frame shows it's gone or moved rooms (shot
 * cleared, or shot_hoffset/shot_voffset changed), same idea as
 * clear_men(). Only unguided shots (missile==0) are cleared this way; the
 * old_status->shot==1 guard plus the OR-chain against new_status collapse
 * to "new shot is gone or in a different room" since old_status->shot is
 * already known to be 1 here (the mismatch-vs-old-shot sub-clause the
 * original OR'd in is therefore always true whenever new_status->shot==0,
 * so it's redundant and simplified away).
 * Reconstructed from 14df:3b99-3d60, sprite formula matches
 * display_shots()'s unguided-shot case exactly (same sprite being erased
 * that was drawn).
 */
void clear_shots(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
    MAZE_LOG_PACKET far * old_status;
    MAZE_LOG_PACKET far * new_status;
    int i;
    int sprite;

    for (i = 0; i < 5; i++) {
        old_status = &maze_log->status[i];
        new_status = &new_log->status[i];

        if (old_status->shot == 1) {
            if (new_status->shot == 0 ||
                old_status->shot_hoffset != new_status->shot_hoffset ||
                old_status->shot_voffset != new_status->shot_voffset) {

                if (old_status->shot_hoffset == _hoffset &&
                    old_status->shot_voffset == _voffset &&
                    old_status->missile == 0) {

                    sprite = ((old_status->sprite - 4) >> 2) * 2 +
                             (old_status->shot_dir == -1 ? 1 : 0) + 0x2c;
                    clear_sprite_restore(old_status->shot_x, old_status->shot_y, sprite);
                }
            }
        }
    }
}

/*
 * Erases the room's gold pickup sprite (sprite index 0x429, matching
 * display_gold()) once it's been picked up: old frame had it present,
 * new frame doesn't. Reconstructed from 14df:3d61-3de5.
 */
void clear_gold(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
    if (maze_log->gold.present != 0 && new_log->gold.present == 0) {
        if (maze_log->gold.present == 1 &&
            maze_log->gold.hoffset == _hoffset &&
            maze_log->gold.voffset == _voffset) {
            clear_sprite_restore(maze_log->gold.col << 3, maze_log->gold.row << 3, 0x429);
        }
    }
}

/*
 * Erases the room's warp token sprite (same index formula as
 * display_token(), type*2+0x42c) once it's gone or moved: if the token's
 * room changed (hoffset/voffset), also pokes update_radar() for both the
 * old and new frames (radar blips need updating either way, even though
 * this function only clears the on-screen sprite for the old room).
 * Clears when the room changed, OR the token is now gone/moved within
 * the room (present==0, or row/col changed) -- reconstructed from
 * 14df:3de6-3f13, using update_radar()'s already-established signature
 * from MAZEUTIL.C to resolve the two calls Ghidra's decompile garbled
 * into CONCAT22 nonsense.
 */
void clear_token(MAZE_LOG_STRUCT far * new_log, MAZE_LOG_STRUCT far * maze_log)
{
    int sprite;
    int token_changed;

    sprite = maze_log->token.type * 2 + 0x42c;
    token_changed = 0;

    if (maze_log->token.present != 0) {
        if (new_log->token.hoffset != maze_log->token.hoffset ||
            new_log->token.voffset != maze_log->token.voffset) {
            token_changed = 1;
            update_radar(0, maze_log, maze_log->gold.present, 0);
            update_radar(0, new_log, new_log->gold.present, new_log->token.present);
        }

        if ((token_changed || new_log->token.present == 0 ||
             new_log->token.row != maze_log->token.row ||
             new_log->token.col != maze_log->token.col) &&
            maze_log->token.hoffset == _hoffset &&
            maze_log->token.voffset == _voffset) {
            clear_sprite_restore(maze_log->token.col << 3, maze_log->token.row << 3, sprite);
        }
    }
}

/*
 * Shared "position a sprite, copy its bitmap into scratch, restore the
 * background under it, then blit" helper used by display_gold()/
 * display_shots() for the fixed-size sprite slot 0x4e0. Reconstructed
 * from 14df:3f14-3ff7 (same pattern as display_men()/display_token(),
 * minus any overlay compositing).
 */
void display_sprite_restore(int x, int y, int sprite)
{
    unsigned char sp_buff[100];

    _sprites[0x4e0].spritex = x;
    _sprites[0x4e0].spritey = y;
    _sprites[0x4e0].spritew = _sprites[sprite].spritew;
    _sprites[0x4e0].spriteh = _sprites[sprite].spriteh;
    memcpy(sp_buff, _sprites[sprite].sprite,
           (unsigned int) _sprites[sprite].spritew * _sprites[sprite].spriteh);
    _sprites[0x4e0].sprite = sp_buff;
    restore_background(&_sprites[0x4e0]);
    display_sprite(0x4e0);
}

/*
 * Same as display_sprite_restore() but blits a blank (all-zero) bitmap of
 * `sprite`'s size instead of copying its real bitmap -- used by
 * clear_men()/clear_shots()/clear_gold()/clear_token() to erase a sprite
 * that's no longer present. Reconstructed from 14df:3ff8-40cc.
 */
void clear_sprite_restore(int x, int y, int sprite)
{
    unsigned char sp_buff[100];

    _sprites[0x4e0].spritex = x;
    _sprites[0x4e0].spritey = y;
    _sprites[0x4e0].spritew = _sprites[sprite].spritew;
    _sprites[0x4e0].spriteh = _sprites[sprite].spriteh;
    memset(sp_buff, 0, (unsigned int) _sprites[sprite].spritew * _sprites[sprite].spriteh);
    _sprites[0x4e0].sprite = sp_buff;
    restore_background(&_sprites[0x4e0]);
    display_sprite(0x4e0);
}

/*
 * Per-frame collision testing for the local player's own in-flight shot
 * (wstation's status.shot) and ship, run once per game tick from
 * main_loop(). In order:
 *   1. My shot vs. any other station's shot in the same room -> both
 *      destroyed (mine cleared here; theirs is symmetric, handled when
 *      *their* test_shots() runs).
 *   2. My shot vs. any connected station's raised shield -> bounce (flip
 *      shot_dir, reposition at the shield edge, re-test room/blocks).
 *   3. My ship vs. gold pickup in the same room -> collect it, award a
 *      random 5-50 point bonus (gauss(45), forced positive, +5), and
 *      reschedule its respawn.
 *   4. My ship vs. a warp token in the same room -> if the token's type
 *      is a collectible effect (types 2/4/5 arm a smart-bomb variant via
 *      the shared maze_log->sbomb slot and immediately test_action() it;
 *      other types <12 are consumed with no further effect), consume and
 *      reschedule it; if type >= 12 it isn't consumed at all, just
 *      bounces the shot back and advances the token's own state machine
 *      (a rotating count that either re-randomizes its type or relocates
 *      it via init_position(), preserved exactly as compiled).
 *   5. Any station's shot vs. my ship -> I lose a life, the shooter is
 *      awarded points (a 10x bonus if I have an unclaimed shield-hit
 *      credit pending from an earlier bounce), and I respawn
 *      invisible/facing `dir` with a 60-frame explosion animation
 *      (checked first via a do-while so gold/token pickup in step 3/4 is
 *      skipped entirely on the frame I'm hit).
 *
 * Reconstructed from 14df:40cd-504e (a genuinely large function; no raw
 * disassembly was pulled given its size, but the decompile's bitfield
 * extracts all match this project's already-established MAZE_LOG_PACKET/
 * GOLD_PACKET/TOKEN_PACKET/SBOMB_PACKET layouts exactly, including the
 * `pPacked_bits[3] == ' '` (0x20) idiom already confirmed elsewhere
 * (display_men, 14df:31a8-31b5) to actually mean `shield == 1`, not a
 * literal byte compare).
 *
 * PRESERVED QUIRK: the gold- and token-pickup sprite-size lookups read
 * maze_log->status[5].shot_dir -- one past the end of the 5-element
 * status[] array -- instead of status[_wstation].shot_dir, because by
 * this point in the original function the register/variable that had
 * held the loop index from an earlier loop (reused here without being
 * reset to _wstation) has the value 5. This is an out-of-bounds read in
 * the original binary (it reads whatever bytes happen to follow
 * MAZE_LOG_STRUCT's status[] array in memory) that only affects which of
 * two near-identical sprite-frame variants is used for the collision
 * bounding box, so it's preserved verbatim rather than "fixed" to
 * _wstation.
 */
void test_shots(MAZE_LOG_STRUCT far * maze_log, int dir)
{
    MAZE_LOG_PACKET far * ws;
    MAZE_LOG_PACKET far * st;
    int sprite;
    int wx, wy, ww, wh;
    int sx, sy, sw, sh;
    int i;
    int shield_bounce;
    int count;
    int th, tv, tx, ty;

    ws = &maze_log->status[_wstation];

    /* 1. my shot vs. other stations' shots */
    if (ws->missile == 0) {
        sprite = pacman * 2 + (ws->shot_dir == -1 ? 1 : 0) + 0x2c;
    } else {
        sprite = ws->missile + 0x4d6;
    }
    wx = ws->shot_x;
    wy = ws->shot_y;
    ww = _sprites[sprite].spritew;
    wh = _sprites[sprite].spriteh;

    for (i = 0; i < 5; i++) {
        st = &maze_log->status[i];
        if (i != _wstation && st->shot == 1 &&
            st->shot_hoffset == _hoffset && st->shot_voffset == _voffset) {

            if (st->missile == 0) {
                sprite = ((st->sprite - 4) >> 2) * 2 + (st->shot_dir == -1 ? 1 : 0) + 0x2c;
            } else {
                sprite = st->missile + 0x4d6;
            }
            sx = st->shot_x;
            sy = st->shot_y;
            sw = _sprites[sprite].spritew;
            sh = _sprites[sprite].spriteh;

            if (collision_detect(wx, wy, ww, wh, sx, sy, sw, sh) == 1) {
                ws->shot = 0;
                ws->grenade = 0;
                ws->missile = 0;
                show_selected(0, _selected);
            }
        }
    }

    /* 2. my shot vs. other stations' shields */
    for (i = 0; i < 5; i++) {
        st = &maze_log->status[i];

        if (ws->shot == 1 && maze_log->connection[i] > 0 && st->shield == 1 &&
            ws->shot_hoffset == st->hoffset && ws->shot_voffset == st->voffset) {

            sprite = (st->invisible == 1) ? 0 : st->sprite;
            sx = st->x;
            sy = st->y;
            sw = _sprites[sprite].spritew;
            sh = _sprites[sprite].spriteh;

            if (ws->invisible < 2 && collision_detect(wx, wy, ww, wh, sx, sy, sw, sh) == 1) {
                shield_bounce = ((st->sprite - 4) & 3) / 2;
                if (shield_bounce == 0) {
                    shield_bounce = -1;
                }
                if (ws->shot_dir != -shield_bounce) {
                    ws->shot_dir = -ws->shot_dir;
                    sx = st->x + (ws->shot_dir == -1 ? -ww : sw);
                    ws->shot_x = sx;
                    test_shot_room_change(ws, ww);
                    if (test_for_block(sx, wy, ww, wh, ws->hoffset, ws->voffset) > 0) {
                        ws->missile = 0;
                        ws->shot = 0;
                        ws->grenade = 0;
                    }
                    ws->shot_x = sx;
                }
            }
        }
    }

    /* 3-4 setup: my ship's own bounding box, reused below */
    sprite = (ws->invisible == 1) ? 0 : ws->sprite;
    wx = ws->x;
    wy = ws->y;
    ww = _sprites[sprite].spritew;
    wh = _sprites[sprite].spriteh;

    /* 5. any station's shot vs. my ship (checked before gold/token pickup;
       a hit skips pickup entirely this frame, matching the original's
       do-while structure) */
    for (i = 0; i < 5; i++) {
        st = &maze_log->status[i];

        if (st->shot == 1 && st->shot_hoffset == _hoffset && st->shot_voffset == _voffset) {
            if (st->missile == 0) {
                sprite = ((st->sprite - 4) >> 2) * 2 + (st->shot_dir == -1 ? 1 : 0) + 0x2c;
            } else {
                sprite = st->missile + 0x4d6;
            }
            sx = st->shot_x;
            sy = st->shot_y;
            sw = _sprites[sprite].spritew;
            sh = _sprites[sprite].spriteh;

            if (ws->invisible < 2 && collision_detect(wx, wy, ww, wh, sx, sy, sw, sh) == 1) {
                maze_log->men[_wstation]--;
                if (i != _wstation) {
                    if (maze_log->hit_man == _wstation) {
                        add_score(i, maze_log->hit_score * 10, maze_log);
                        maze_log->hit_man = -1;
                    } else {
                        add_score(i, 10, maze_log);
                    }
                }
                ws->invisible = dir * 6 + 2;
                ws->warp_factor = 0;
                _explode_count = 0x3c;
                return;
            }
        }
    }

    /* 3. my ship vs. gold pickup */
    wx = maze_log->gold.col << 3;
    wy = maze_log->gold.row << 3;
    ww = _sprites[0x429].spritew;
    wh = _sprites[0x429].spriteh;

    if (maze_log->gold.present == 1 && ws->shot == 1 &&
        maze_log->gold.hoffset == ws->shot_hoffset &&
        maze_log->gold.voffset == ws->shot_voffset) {

        if (ws->missile == 0) {
            sprite = ((ws->sprite - 4) >> 2) * 2 + (maze_log->status[5].shot_dir == -1 ? 1 : 0) + 0x2c;
        } else {
            sprite = ws->missile + 0x4d6;
        }
        sx = ws->shot_x;
        sy = ws->shot_y;
        sw = _sprites[sprite].spritew;
        sh = _sprites[sprite].spriteh;

        if (ws->invisible < 2 && collision_detect(wx, wy, ww, wh, sx, sy, sw, sh) == 1) {
            maze_log->gold.present = 0;
            maze_log->gold.time = maze_log->time + rand() % 60 + 30;
            ws->shot = 0;
            ws->grenade = 0;
            ws->missile = 0;
            maze_log->hit_man = (char) _wstation;
            maze_log->hit_score = (char) gauss(0x2d);
            if (maze_log->hit_score < 0) {
                maze_log->hit_score = -maze_log->hit_score;
            }
            maze_log->hit_score += 5;
        }
    }

    /* 4. my ship vs. warp token */
    wx = maze_log->token.col << 3;
    wy = maze_log->token.row << 3;
    ww = _sprites[0x42c].spritew;
    wh = _sprites[0x42c].spriteh;

    if (maze_log->token.present == 1 && ws->shot == 1 &&
        maze_log->token.hoffset == ws->shot_hoffset &&
        maze_log->token.voffset == ws->shot_voffset) {

        if (ws->missile == 0) {
            sprite = ((ws->sprite - 4) >> 2) * 2 + (maze_log->status[5].shot_dir == -1 ? 1 : 0) + 0x2c;
        } else {
            sprite = ws->missile + 0x4d6;
        }
        sx = ws->shot_x;
        sy = ws->shot_y;
        sw = _sprites[sprite].spritew;
        sh = _sprites[sprite].spriteh;

        if (ws->invisible < 2 && collision_detect(wx, wy, ww, wh, sx, sy, sw, sh) == 1) {
            if (maze_log->token.type < 12) {
                maze_log->token.present = 0;
                maze_log->token.warp_factor = 0;
                maze_log->token.time = maze_log->time + rand() % 20 + 5;
                ws->shot = 0;
                ws->grenade = 0;
                ws->missile = 0;

                if (maze_log->token.type == 4) {
                    _sbomb = 1;
                    maze_log->sbomb.present = 1;
                    maze_log->sbomb.man = _wstation;
                    maze_log->sbomb.hoffset = maze_log->token.hoffset;
                    maze_log->sbomb.voffset = maze_log->token.voffset;
                    test_action(maze_log, dir);
                }
                if (maze_log->token.type == 2) {
                    _sbomb = 1;
                    maze_log->sbomb.present = 2;
                    maze_log->sbomb.man = _wstation;
                    maze_log->sbomb.hoffset = maze_log->token.hoffset;
                    maze_log->sbomb.voffset = maze_log->token.voffset;
                    test_action(maze_log, dir);
                }
                if (maze_log->token.type == 5) {
                    _sbomb = 1;
                    maze_log->sbomb.present = 3;
                    maze_log->sbomb.man = _wstation;
                    maze_log->sbomb.hoffset = maze_log->token.hoffset;
                    maze_log->sbomb.voffset = maze_log->token.voffset;
                    test_action(maze_log, dir);
                }
            } else {
                /* not consumed: bounce the shot and advance the token's
                   own rotating state instead */
                ws->shot_dir = -ws->shot_dir;

                count = ((maze_log->token.count) + 1) & 7;
                maze_log->token.count = count;

                if (count == 4) {
                    int r = rand();
                    if (r % 5 == 0) {
                        maze_log->token.type = 9;
                    } else if (maze_log->token.type < 16) {
                        maze_log->token.type = 10;
                    } else {
                        maze_log->token.type = 11;
                    }
                } else {
                    maze_log->token.type++;
                    if (rand() % 4 == 0) {
                        init_position(&th, &tv, &tx, &ty);
                        maze_log->token.hoffset = th & 0xf;
                        maze_log->token.voffset = tv & 0xf;
                        maze_log->token.col = (char) (tx >> 3);
                        maze_log->token.row = (char) (ty >> 3);
                        maze_log->token.time = maze_log->time + 20;
                        maze_log->token.warp_factor = 7;
                        maze_log->token.warp_dir = -1;
                    }
                }
            }
        }
    }
}

/*
 * Per-frame collision testing for the local player's ship walking over a
 * pickup (as opposed to test_shots(), which handles shooting one):
 *   - Gold: award score.col*10 wait -- score field * 10, reschedule
 *     respawn, mark it man = (_wstation+1)&7 (a "who last took it"-style
 *     tag, same idiom as the shot-based pickup in test_shots()).
 *   - Token: types >= 12 aren't collectible at all (sets the one-shot
 *     _spiked flag instead, e.g. walking onto a hazard token); types 0-11
 *     are consumed and dispatch a per-type effect (type 9 first
 *     re-rolls itself to a random 0-8 type before dispatch, matching the
 *     "roulette" token seen in test_shots()'s token state machine).
 *
 * Reconstructed from 14df:504f-5598 (no raw disassembly pulled given
 * size; bitfield extracts cross-checked against the same struct layouts
 * already verified via test_shots()). Ghidra's 32-bit `long` time
 * arithmetic (split into CARRY2-chained 16-bit half-word ops) is
 * collapsed back to plain `long` addition throughout -- provably
 * identical, just how the compiler lowers 32-bit math on this 16-bit
 * target.
 */
void test_pos(MAZE_LOG_STRUCT far * maze_log)
{
    MAZE_LOG_PACKET far * ws;
    int ship_sprite;
    int ww, wh;
    int sprite;

    ws = &maze_log->status[_wstation];

    /* gold pickup */
    if (maze_log->gold.present == 1 &&
        maze_log->gold.hoffset == _hoffset &&
        maze_log->gold.voffset == _voffset) {

        ship_sprite = (ws->invisible == 1) ? 0 : ws->sprite;
        ww = _sprites[ship_sprite].spritew;
        wh = _sprites[ship_sprite].spriteh;

        if (ws->invisible < 2 &&
            collision_detect(maze_log->gold.col << 3, maze_log->gold.row << 3,
                              _sprites[0x429].spritew, _sprites[0x429].spriteh,
                              ws->x, ws->y, ww, wh) == 1) {
            maze_log->gold.time = maze_log->time + rand() % 60 + 5;
            maze_log->gold.present = 0;
            add_score(_wstation, maze_log->gold.score * 10, maze_log);
            maze_log->gold.man = (_wstation + 1) & 7;
        }
    }

    /* token pickup */
    if (maze_log->token.present != 1) {
        return;
    }
    if (maze_log->token.hoffset != _hoffset) {
        return;
    }
    if (maze_log->token.voffset != _voffset) {
        return;
    }

    sprite = maze_log->token.type * 2 + 0x42c;
    ship_sprite = (ws->invisible == 1) ? 0 : ws->sprite;

    if (ws->invisible >= 2) {
        return;
    }

    if (collision_detect(maze_log->token.col << 3, maze_log->token.row << 3,
                          _sprites[sprite].spritew, _sprites[sprite].spriteh,
                          ws->x, ws->y,
                          _sprites[ship_sprite].spritew, _sprites[ship_sprite].spriteh) != 1) {
        return;
    }

    if (maze_log->token.type > 11) {
        _spiked = 1;
        return;
    }

    maze_log->token.time = maze_log->time + rand() % 20 + 5;
    maze_log->token.present = 0;
    maze_log->token.warp_factor = 0;
    maze_log->token.man = (_wstation + 1) & 7;

    if (maze_log->token.type == 9) {
        maze_log->token.type = (char) (rand() % 9);
    }

    if (maze_log->token.type == 0) {
        _warp_count += 2;
        return;
    }
    if (maze_log->token.type == 1) {
        _ishot_count += 2;
        return;
    }
    if (maze_log->token.type == 6) {
        _missile_count += 2;
        return;
    }
    if (maze_log->token.type == 2) {
        if (_drunk == 1 && maze_log->time <= _drunk_time) {
            _drunk_time += _token_time;
        } else {
            _drunk_time = maze_log->time + _token_time;
        }
        _drunk = 1;
        add_score(_wstation, 0x32, maze_log);
        return;
    }
    if (maze_log->token.type == 3) {
        _visible_count += 2;
        return;
    }
    if (maze_log->token.type == 7) {
        _shield_count += 2;
        return;
    }
    if (maze_log->token.type == 8) {
        _grenade_count += 2;
        return;
    }
    if (maze_log->token.type == 4) {
        _sbomb = 1;
        maze_log->sbomb.present = 1;
        maze_log->sbomb.man = _wstation;
        maze_log->sbomb.hoffset = maze_log->token.hoffset;
        maze_log->sbomb.voffset = maze_log->token.voffset;
        return;
    }
    if (maze_log->token.type == 10 || maze_log->token.type == 11) {
        add_score(_wstation, 500, maze_log);
        return;
    }
    if (maze_log->token.type == 5) {
        _glue = 1;
        _glue_time = maze_log->time + _token_time;
        return;
    }
    if (maze_log->token.type > 11) {
        _spiked = 1;
    }
}

/*
 * Resolves an active smart-bomb effect (maze_log->sbomb, armed by
 * test_shots()'s or test_pos()'s token-pickup handling) against the
 * local player, once it's in the same room and the player is alive:
 * present==1 kills the player exactly like being hit by a shot in
 * test_shots() (awarding the bomb's owner, sbomb.man, the same
 * hit-credit-bonus-or-flat-10 score logic and a 60-frame respawn), while
 * present==2/3 apply the drunk/glue effects (same idioms as test_pos()'s
 * token dispatch). Reconstructed from 14df:55a5-575a.
 */
void test_action(MAZE_LOG_STRUCT far * maze_log, int dir)
{
    MAZE_LOG_PACKET far * ws;

    ws = &maze_log->status[_wstation];

    if (maze_log->sbomb.present < 1) {
        return;
    }
    if (maze_log->sbomb.hoffset != _hoffset) {
        return;
    }
    if (maze_log->sbomb.voffset != _voffset) {
        return;
    }
    if (ws->invisible >= 2) {
        return;
    }

    if (maze_log->sbomb.present == 1) {
        maze_log->men[_wstation]--;
        if (maze_log->hit_man == _wstation) {
            add_score(maze_log->sbomb.man, maze_log->hit_score * 10, maze_log);
            maze_log->hit_man = -1;
        } else {
            add_score(maze_log->sbomb.man, 10, maze_log);
        }
        ws->invisible = dir * 6 + 2;
        _explode_count = 0x3c;
        ws->warp_factor = 0;
        return;
    }

    if (maze_log->sbomb.present == 2) {
        if (_drunk == 1 && maze_log->time <= _drunk_time) {
            _drunk_time += _token_time;
        } else {
            _drunk_time = maze_log->time + _token_time;
        }
        _drunk = 1;
        return;
    }

    if (maze_log->sbomb.present == 3) {
        _glue = 1;
        _glue_time = maze_log->time + _token_time;
        return;
    }
}

