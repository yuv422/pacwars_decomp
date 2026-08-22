/*
 * PACWARS.H
 *
 * Shared type definitions used across multiple source modules.
 * Reconstructed from the Borland TLINK debug symbol dump (PACWARS.TXT).
 * Structs whose true byte layout could not be fully resolved from the
 * dump (nested anonymous sub-structures with no recorded name) are
 * marked with a TODO comment rather than guessed.
 */
#ifndef PACWARS_H
#define PACWARS_H

#include <dos.h>
#include <dir.h>
#include <time.h>

typedef struct SBOMB_PACKET_tag {
    int present : 4;
    unsigned int man : 4;
    unsigned int hoffset : 4;
    unsigned int voffset : 4;
} SBOMB_PACKET;

typedef struct MAZE_LOG_PACKET_tag {
    int invisible : 7;
    int shot : 2;
    int shot_dir : 3;
    unsigned int warp_factor : 4;
    unsigned int sprite : 6;
    unsigned int missile : 4;
    unsigned int grenade : 3;
    unsigned int shield : 3;
    int x : 12;
    unsigned int shot_hoffset : 4;
    int y : 12;
    unsigned int shot_voffset : 4;
    int shot_x : 12;
    unsigned int hoffset : 4;
    int shot_y : 12;
    unsigned int voffset : 4;
} MAZE_LOG_PACKET;

typedef struct REGISTER_STRUCT_tag {
    char reg_str[15];
    char registered;
    char reg_name[41];
    char reg_mess[2][17];
} REGISTER_STRUCT;

typedef struct TOKEN_PACKET_tag {
    int present : 2;
    unsigned int count : 3;
    unsigned int man : 3;
    unsigned int hoffset : 4;
    unsigned int voffset : 4;
    char type;
    char warp_factor;
    char warp_dir;
    long time;
    char row;
    char col;
} TOKEN_PACKET;

typedef struct GOLD_PACKET_tag {
    int present : 2;
    unsigned int man : 3;
    unsigned int hoffset : 4;
    unsigned int voffset : 4;
    unsigned int _reserved : 3;
    char score;
    long time;
    int row;
    int col;
} GOLD_PACKET;

typedef struct MAZE_LOG_STRUCT_tag {
    char type;
    unsigned int sync;
    long time;
    char connection[5];
    unsigned int score[5];
    char hiscore[5];
    unsigned char men[5];
    char hit_man;
    char hit_score;
    GOLD_PACKET gold;
    TOKEN_PACKET token;
    SBOMB_PACKET sbomb;
    MAZE_LOG_PACKET status[5];
} MAZE_LOG_STRUCT;

typedef struct ANIMATE_tag {
    char row;
    char col;
    unsigned char rate;
    unsigned char offset;
    char frames;
    char prev_frame;
    char redraw;
    char far * sprites;
} ANIMATE;

typedef struct MAZE_STRUCT_tag {
    unsigned int def[25][30];
} MAZE_STRUCT;

typedef struct SPRITE_STRUCT_tag {
    int spritex;
    int spritey;
    unsigned char spritew;
    unsigned int spriteh;
    unsigned char far * sprite;
} SPRITE_STRUCT;

typedef struct address_tag {
    char far * p;
} address;

typedef struct ANIM_ELEMENT_tag {
    char row_off;
    char col_off;
    int block;
    int attribute;
} ANIM_ELEMENT;

typedef struct ANIM_FRAME_tag {
    int num_elements;
    ANIM_ELEMENT far * element_array;
} ANIM_FRAME;

typedef ANIM_FRAME far *ANIM_FRAME_PTR;

/*
 * frame: originally declared `ANIM_FRAME_PTR far * frame` (an extra level
 * of indirection -- array of far pointers to ANIM_FRAME). Disassembly of
 * MAZEANIM.C's room_animation_func (1a54:0432-044e) shows the per-frame
 * address computed as `frame_base + next_state * 6` (6 = sizeof(ANIM_FRAME),
 * confirmed via get_struct_layout) with no extra pointer dereference --
 * i.e. `frame` is a plain array of ANIM_FRAME structs, not an array of
 * pointers to them. Fixed to the single-indirection ANIM_FRAME_PTR
 * (= ANIM_FRAME far *) accordingly.
 */
typedef struct ANIM_SEQUENCE_tag {
    int frames;
    char rate;
    char w;
    char h;
    ANIM_FRAME_PTR frame;
} ANIM_SEQUENCE;

typedef struct ANIM_OB_tag {
    char hoffset;
    char voffset;
    char row;
    char col;
    char offset;
    char animate;
    char redraw;
    int far * prev_frame_buffer;
} ANIM_OB;

typedef struct OLD_MAZE_tag {
    unsigned char def[25][30];
} OLD_MAZE;

typedef struct HISCORE_tag {
    int pacman[10];
    char name[10][13];
    unsigned int score[10];
} HISCORE;

typedef struct POINT_tag {
    int x;
    int y;
} POINT;

typedef struct VAR_STRUCT_tag {
    char security_level;
    int curr_period;
    int curr_week;
    int sv_indic;
    int menu[3];
    int branch_num;
    char branch_name[12];
    int plmodel;
    int backlog;
    int screen_size;
    unsigned int cursor_size;
    char dbase_error[11];
    char dbase_prog[9];
    int first_in;
    char prog_name[13];
    int newweek_indic;
    long wait_time;
    char blink_status;
    char sv_buffer[512];
} VAR_STRUCT;

typedef struct ECBFragment_tag {
    void far * address;
    unsigned int size;
} ECBFragment;

typedef struct ECB_tag {
    unsigned int linkAddress[2];
    void far * ESDAddress;
    unsigned char inUseFlag;
    unsigned char completionCode;
    unsigned int sockNumber;
    char IPXWordkspace[4];
    char driverWorkspace[12];
    char immediateAddress[6];
    unsigned int fragmentCount;
    ECBFragment fragDesc[2];
} ECB;

typedef struct IPX_ADDRESS_tag {
    char network[4];
    char node[6];
    unsigned int socket;
} IPX_ADDRESS;

typedef struct IPX_HEADER_tag {
    unsigned int checksum;
    unsigned int length;
    char transCont;
    char packet_type;
    IPX_ADDRESS destination;
    IPX_ADDRESS source;
    char data[400];
} IPX_HEADER;

typedef struct CON_INFO_tag {
    char address[5][10];
    char inuse[5];
} CON_INFO;

typedef struct SET_STRUCT_tag {
    unsigned char save_posn[15];
    int encrypted;
    unsigned char sernum[16];
    long serial_val;
    int number_stat;
    int station_id;
    unsigned char machine_name[21];
    int label_width;
    int label_height;
    int num_labels;
    unsigned char report_heading[41];
    unsigned char filepaths[6][65];
    int mail_media;
    int backup_media;
    int num_backups;
    unsigned char tape;  /* TODO: anonymous nested struct, verify layout */
    unsigned char disk;  /* TODO: anonymous nested struct, verify layout */
    unsigned char password;  /* TODO: anonymous nested struct, verify layout */
    unsigned char cash_code[2];
    unsigned char misc_code[2];
    unsigned char admin_code[2];
    unsigned char serv_code[2];
    int plist_sort;
    unsigned char other_backup[10];  /* TODO: anonymous nested struct, verify layout */
} SET_STRUCT;

typedef struct ENV_STRUCT_tag {
    unsigned char user_id[4];
    int prompt_beep;
    int error_beep;
    int search_lines;
    int enter_action;
    unsigned char menu_fore;
    unsigned char menu_back;
    unsigned char high_fore;
    unsigned char high_back;
} ENV_STRUCT;

typedef struct MVASYS_STRUCT_tag {
    SET_STRUCT mvaset;
    ENV_STRUCT mvaenv;
    VAR_STRUCT mvavar;
} MVASYS_STRUCT;

typedef struct WAITLOG_STRUCT_tag {
    unsigned char id[4];
    long start_time;
    long end_time;
    long idle_time;
} WAITLOG_STRUCT;


typedef MAZE_STRUCT far *MAZE_PTR;
typedef ANIM_OB far *ANIM_OB_PTR;
typedef ANIM_ELEMENT far *ANIM_ELEMENT_PTR;
typedef ANIM_SEQUENCE far *ANIM_SEQ_PTR;
typedef VAR_STRUCT far *VAR_PTR;

/*
 * Shared core game-state globals. Confirmed via Ghidra xref analysis to be
 * read/written from many modules (MAZE.C, IPXC.C, MAZEINIT.C, MAZEUTIL.C,
 * MAZEEDIT.C, and others), so unlike a module-local static these need a
 * real extern declaration here. Defined (storage allocated) in MAZE.C,
 * alongside main()/pacwars(), the primary entry points that initialize them.
 */
extern int comms;                  /* nonzero when running as a network (IPX) game */
extern REGISTER_STRUCT far * reg;  /* pointer to the loaded registration record */
extern int pacman;                 /* index of the selected pacman character, 0-based */
extern char curr_name[13];         /* current player's name, as entered/edited */
extern unsigned int curr_score;    /* current player's score */
extern int esc;                    /* set nonzero to signal "return to menu" from play */

/*
 * F1-pressed latch, set by a key-vector interrupt handler installed in
 * edit_name() (MAZEUTIL.C) and consumed/reset by choose_pacman()
 * (MAZEUTIL.C) to bring up f1_instructions() without waiting for the next
 * normal read_key() poll. Storage defined in MAZEUTIL.C.
 */
extern int f1;

/*
 * Remaining-duration counters for the local workstation's active
 * power-ups, shown as small numbered icons on the scoreboard by
 * fill_scoreboard() (MAZEUTIL.C). Real owner not yet identified (no
 * write site found via xref analysis in the modules decompiled so far);
 * storage defined in MAZEUTIL.C for now, as the first module needing them.
 */
extern int _warp_count;
extern int _shield_count;
extern int _grenade_count;
extern int _ishot_count;
extern int _missile_count;
extern int _visible_count;

/*
 * Set to request that choose_pacman() (MAZEUTIL.C) immediately prompt for
 * a name edit the next time it runs, before its normal high-score-beaten
 * check. No write site was found anywhere in the modules decompiled so
 * far via Ghidra xref analysis (the reference in choose_pacman() itself
 * didn't turn up as an xref target either, likely a decompiler/analysis
 * gap rather than truly dead data, since choose_pacman() both reads and
 * clears it). Storage defined in MAZEUTIL.C for now.
 */
extern int edit_name_flag;

/*
 * Scratch working-copy buffers used by the maze editor (MAZEEDIT.C):
 * maze_buff/maze_abuff hold the currently-edited room's maze/attribute
 * grids, undo_buff/undo_abuff hold a pre-edit snapshot of the same for
 * undo_block(). Each is MAZE_STRUCT-sized (1500 bytes), allocated by
 * alloc_maze_editor_mem(). Storage defined in MAZEEDIT.C.
 */
extern MAZE_STRUCT far * _maze_buff;
extern MAZE_STRUCT far * _maze_abuff;
extern MAZE_STRUCT far * _undo_buff;
extern MAZE_STRUCT far * _undo_abuff;

/*
 * Current room/viewport offset within the maze (which 25x30 "page" of the
 * overall maze is currently on screen). Confirmed via Ghidra xref
 * analysis to be read from many modules (MAZE.C and the maze-drawing
 * family) and written by the maze editor (MAZEEDIT.C). Defined in
 * MAZE.C, alongside the other shared game-state globals above.
 */
extern int _hoffset;
extern int _voffset;

/*
 * Room-grid dimensions (how many rooms wide/tall the currently-loaded maze
 * is, in units of the 25x30-cell pages indexed by _maze/_maze_attrib/
 * _animate_maze below) and the local "workstation" slot index used to pick
 * this machine's entry out of the 5-slot network arrays (MAZE_LOG_STRUCT
 * etc). _VSIZE/_HSIZE are written by choose_edit_maze() (MAZEEDIT.C, the
 * maze editor's "how big a maze do you want" screen); storage for both is
 * defined in MAZEEDIT.C. _wstation's write site was not found via xref
 * analysis (likely set during IPX/network session setup); storage defined
 * in MAZEUTIL.C for now, as the first module needing it.
 */
extern int _VSIZE;
extern int _HSIZE;
extern int _wstation;

/*
 * NOTE: an earlier decompilation pass declared a second global here named
 * "_comms" (storage in MAZESPT.C), apparently not realizing this file
 * already declares the real one above ("comms", storage in MAZE.C, both
 * resolving to the same address, 340e:2742). Nothing in this project
 * distinguished a separate meaning for "_comms" -- MAZESPT.C's copy was
 * simply always zero regardless of the real game state -- so that
 * duplicate declaration and its uses in MAZESPT.C have been removed in
 * favour of "comms" above.
 *
 * "_esc" (below) is NOT the same situation: MAZE.C itself uses both
 * "esc" (the local "return to menu" UI flag, set/read throughout its
 * menu-handling code) and "_esc" (a distinct network-level "session
 * ending" flag checked in main_loop()'s per-frame network game loop,
 * confirmed at 340e:7114) -- these are two real, separate variables in
 * the original binary, not a duplicate.
 */
extern int _esc;

/*
 * Countdown/state globals for the "spike" hazard tile type: _spike holds
 * the current spike tile's attribute-check return value (set elsewhere,
 * read by test_for_block()/testy() in MAZESPT.C); _spiked is a one-shot
 * "player just touched a spike" flag written by testy() and read/written
 * by other not-yet-decompiled modules (confirmed via get_xrefs_to).
 * Storage defined in MAZESPT.C.
 */
extern int _spike;
extern int _spiked;

/*
 * Scroll offset (in room-grid rows) for the maze editor's room-grid
 * picker screen (choose_edit_maze(), MAZEEDIT.C) -- at most 2 rows of
 * rooms are shown at once, out of up to 4 (_VSIZE). Storage defined in
 * MAZEEDIT.C, its real owner (confirmed via get_xrefs_to: read/written
 * only by choose_edit_maze/display_rooms/get_room_pos/display_scroll,
 * all in MAZEEDIT.C).
 */
extern int _room_offset;

/*
 * IPX socket number this station is using, allocated/incremented by
 * join() (IPXCOMM.C) on each connection retry. Confirmed cross-module via
 * get_xrefs_to on 340e:28de: referenced from join()/disconnect()
 * (IPXCOMM.C, segment 2d4c) AND from init_ipx_ecb_send()/
 * init_ipx_ecb_receive() (IPXC.C, segment 2d1a), so unlike most of this
 * module's other new globals this one can't be file-local. Storage
 * defined in IPXCOMM.C.
 */
extern unsigned int _socket_no;

/*
 * This station's own 10-byte IPX address (network[4]+node[6], no socket
 * number). Same cross-module story as _socket_no above: get_xrefs_to on
 * 340e:fc34 shows join()/join_new_user()/disconnect() (IPXCOMM.C) AND
 * init_net()/init_ipx_ecb_send() (IPXC.C) all touching it. Storage
 * defined in IPXCOMM.C.
 */
extern char _source_address[10];

/*
 * Block-copy/move clipboard state for the maze editor's block_menu()
 * (MAZEEDIT.C): _maze_srow/_maze_scol/_maze_erow/_maze_ecol are the
 * normalized (top-left/bottom-right) bounds of the last block range
 * marked via store_block(), and _undo_hoff/_undo_voff identify which
 * room that range belongs to (so undo_block() can tell whether an undo
 * is still valid for the room currently being edited). Storage defined
 * in MAZEEDIT.C, confirmed via get_xrefs_to to be used only by
 * MAZEEDIT.C's block_menu/store_block/restore_block/undo_block/
 * copy_block.
 */
/*
 * Backing bitmap storage for the 1000 block sprites (_sprites[0x41]
 * through _sprites[0x428], each 8x8 pixels = 64 bytes), allocated as one
 * 64000-byte block by alloc_block_mem() (MAZESPT.C). Storage defined in
 * MAZESPT.C, confirmed via get_xrefs_to to be used only by MAZESPT.C's
 * alloc_block_mem/load_block_array/save_block_array.
 */
extern unsigned char far * _block_mem;

extern int _maze_srow;
extern int _maze_scol;
extern int _maze_erow;
extern int _maze_ecol;
extern int _undo_hoff;
extern int _undo_voff;

/*
 * Per-room-grid-cell far pointers to the maze/attribute/animation data for
 * each of the game's rooms, indexed [voff][hoff]. Allocated (4 rows x 3
 * cols, each MAZE_STRUCT-sized: 25*30*2 = 0x5dc bytes) by
 * alloc_maze_def_mem() in MAZEEDIT.C via calloc(); confirmed via
 * disassembly of that function (the decompiler mis-split each far pointer
 * store into two separate 16-bit "DAT_" writes -- one for the offset
 * half, one for the segment half two bytes later -- rather than
 * recognizing a single far-pointer array, a known decompiler limitation
 * for this binary). Storage for _maze/_maze_attrib is defined in
 * MAZEEDIT.C. _animate_maze is NOT runtime-allocated at all -- each of
 * its 12 slots points at statically-compiled ANIM_OB[] data baked into
 * the original .EXE's data segment (confirmed via read_memory at
 * 340e:00f9-0308); that data, and the pointers themselves, are
 * reproduced byte-for-byte in MAZEUTIL.C, which is where storage is
 * defined.
 */
extern void far * _maze[4][3];
extern void far * _maze_attrib[4][3];
extern ANIM_OB far * _animate_maze[4][3];

/*
 * Master sprite table -- one SPRITE_STRUCT per possible sprite index (see
 * SPRITE_STRUCT_tag above), populated by load_all_sprites() in
 * ALLOC_BLOCK_MEM.C (not yet decompiled) from the game's sprite data
 * files. Referenced from many modules. Storage is defined in MAZEDRAW.C
 * for now, since it was the first module to need it; the real "owner"
 * (load_all_sprites) populates it but doesn't allocate its storage.
 */
extern SPRITE_STRUCT _sprites[1249];

/* Segment of VGA display memory (mode 0x13 and the "mode 400" 4-plane
   layouts alike), as used throughout DISPPIC.C and GRAPH256.C. */
#define VGA_SEGMENT 0xA000

/*
 * Current VGA display page (0 or 1), selecting which half of the
 * VGA_SEGMENT segment the many low-level graphics primitives (drawing,
 * blitting, text256, etc.) read/write to. Confirmed via Ghidra xref
 * analysis to be referenced from dozens of functions across many different
 * modules (MVAGRAPH.C-family drawing primitives, GRAPH256.C, DISPPIC.C,
 * and more). Written by ch_disp_page() in UTILS.C, where it is also
 * defined.
 */
extern int __disp_page;

/*
 * Logical screen/viewport dimensions used by the clipping-aware low-level
 * graphics primitives (fill, blit, sprite, and scoreboard code) across many
 * modules. Written by set_clip_window() in MAZEUTIL.C, where they are also
 * defined.
 */
extern int _max_x;     /* current viewport width, in pixels */
extern int _max_y;     /* current viewport height, in pixels */
extern int _sc_width;  /* row stride (bytes) used when clearing/scrolling */

/*
 * The 256-colour bitmap font subsystem shared by GRAPH256.C's text256() and
 * MVAGRAPH.C's text/font routines (SetTextFont, LoadGraphFont,
 * SaveGraphFont, ByteGraphText and friends). Confirmed via Ghidra xref
 * analysis to be referenced from many modules. Defined in GRAPH256.C for
 * now, since it was the first module to need them; the real "owner"
 * (SetTextFont, in MVAGRAPH.C) populates __text_table_addr/_GraphFontBuffer
 * but doesn't allocate their storage.
 */
extern unsigned char far * _GraphFontBuffer;       /* loaded font bitmap data */
extern int __mva_text_height;                      /* glyph height, in rows */
extern int __mva_text_width;                       /* glyph width, in columns */
extern unsigned char far * __text_table_addr[2];   /* font table(s), indexed by SetTextFont's selection */

/*
 * Current drawing colour/attribute (low nibble = 16-colour EGA/VGA
 * attribute) used by MVAGRAPH.C's planar-mode primitives (DrawLine,
 * DrawHLine, DrawVLine, Circle, Ellipse, Arc, and more) and referenced
 * from several other modules besides. Written by set_colour() in
 * UTILS.C, where it is also defined.
 */
extern unsigned char _char_attrib;

/*
 * Number of bytes per scanline row in MVAGRAPH.C's EGA/VGA planar drawing
 * mode. The original code reads this directly out of the BIOS Data Area
 * (0040:004A, "number of screen columns") rather than tracking its own
 * copy -- for the 640-pixel-wide planar mode these primitives draw into,
 * that BIOS word happens to already equal the needed byte stride (640/8).
 */
#define MVA_ROW_STRIDE (*(unsigned int far *) MK_FP(0x0040, 0x004A))

/*
 * Text-mode screen size, in character rows/columns. Confirmed via Ghidra
 * xref analysis to be read/written from many UTILS.C functions (cls_screen,
 * size_screen, std_form, the F-key status bars, and more) as well as
 * written by SetTextFont() in MVAGRAPH.C when switching into graphics-mode
 * text. Written (and given its initial value) by load_mvaenv()/
 * size_screen() in UTILS.C, where storage is also defined.
 */
extern int __mvarows;
extern int __mvacols;

/*
 * Graphics-mode text state, both written by SetGraphText()/SetTextFont()
 * in MVAGRAPH.C, where they are also defined.
 */
extern int __mva_graph_text;   /* nonzero once graphics-mode text has been selected (SetGraphText) */
extern int __mva_text_table;   /* index of the font table last selected via SetTextFont (0, 1, 2, or 4) */

#endif /* PACWARS_H */
