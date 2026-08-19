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
    unsigned char far * element_array;  /* TODO: anonymous nested struct, verify layout */
} ANIM_FRAME;

typedef ANIM_FRAME far *ANIM_FRAME_PTR;

typedef struct ANIM_SEQUENCE_tag {
    int frames;
    char rate;
    char w;
    char h;
    ANIM_FRAME_PTR far * frame;
} ANIM_SEQUENCE;

typedef struct ANIM_OB_tag {
    char hoffset;
    char voffset;
    char row;
    char col;
    int offset;
    char animate;
    char redraw;
    int prev_frame;
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
 * Current VGA display page (0 or 1), selecting which half of the 0xA000
 * segment the many low-level graphics primitives (drawing, blitting,
 * text256, etc.) read/write to. Confirmed via Ghidra xref analysis to be
 * referenced from dozens of functions across many different modules
 * (MVAGRAPH.C-family drawing primitives, GRAPH256.C, DISPPIC.C, and more).
 * Written by ch_disp_page() in UTILS.C, where it is also defined.
 */
extern int __disp_page;

#endif /* PACWARS_H */
