/*
 * MAZESPT.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MAZESPT.H"
#include "MAZEEDIT.H"
#include "MAZEUTIL.H"
#include "MANEDIT.H"
#include "MEM256.H"
#include "GRAPH256.H"
#include "MVAGRAPH.H"
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys\stat.h>

/*
 * Forward declarations: internal helpers (not in PACWARS.TXT's exported
 * symbol list, so not in MAZESPT.H) that are called by other functions
 * appearing earlier in this file than their own definitions.
 */
void load_block_array(int fd);
void save_block_array(int fd);

/*
 * do_under_asm()/do_over_asm() (2315:000e / 2315:0053) are hand-written
 * assembly routines that composite two SPRITE_STRUCT bitmaps pixel-by-
 * pixel: do_under_asm only writes into dest where dest is currently 0
 * (fills in still-transparent dest pixels, leaving whatever's already
 * opaque there untouched); do_over_asm only writes where src is nonzero
 * (standard transparent-colour-key overlay). Signature confirmed by
 * disassembling do_under_asm's own prologue (LDS SI,[BP+6] / LES
 * DI,[BP+0xa] / byte params at BP+0xe,BP+0x10 / word params at
 * BP+0x12,BP+0x14) and cross-checked against overlap_sprite()'s call-
 * site PUSH order. Both routines' bodies were missing from this
 * project's OVERLAP.ASM stub (a placeholder with no real code, same gap
 * class as other stub-generation misses documented elsewhere in this
 * project) and have been reconstructed there from the disassembly.
 */
extern void do_under_asm(unsigned char far * dest, unsigned char far * src,
                          int h, int w, int dest_wrap, int src_wrap);
extern void do_over_asm(unsigned char far * dest, unsigned char far * src,
                         int h, int w, int dest_wrap, int src_wrap);

/* storage for the _block_mem global declared extern in PACWARS.H */
unsigned char far * _block_mem;

/*
 * Scratch buffer for a sprite's {width; height} byte pair as read/written
 * by read_sprite_array()/write_sprite_array() in one 2-byte I/O call.
 * Confirmed file-local via get_xrefs_to (340e:e57a) -- only referenced
 * from those two functions in this file, so kept static rather than
 * exposed via a header.
 */
static unsigned char _sp_head[2];

/* storage for the shared comms/UI-flag/spike globals declared extern in
   PACWARS.H (see the comment there for xref evidence) */
int _comms;
int _esc;
int _spike;
int _spiked;

/*
 * Per-character name storage (10 playable characters, 13 bytes each --
 * curr_pacman * 0xd stride, confirmed via load_name()/save_name()'s raw
 * disassembly). Confirmed file-local to this module (only referenced
 * from load_names/save_names/load_name/save_name below). Default values
 * recovered byte-for-byte via read_memory at 340e:24be -- identical text
 * to the hardcoded roster-display defaults in MAZEUTIL.C's
 * display_pacmen(), confirming this is the real backing store for the
 * names shown/edited there and in MANEDIT.C.
 */
static char _pacname[10][13] = {
    "PacMan    ", "PacPsycho ", "PacRat    ", "PacTart   ",
    "PacBaby   ", "PacRambo  ", "PacPerv   ", "PacTurd   ",
    "PacBiggles", "PacBum    "
};

/*
 * Allocates the 1000-block sprite bitmap arena (64000 bytes = 1000 x 8x8)
 * and points each of the block sprite slots (_sprites[0x41]..
 * _sprites[0x428]) at its own 64-byte 8x8 tile within it.
 *
 * NOTE: Ghidra's decompiler showed the per-slot struct-field writes as
 * `&_sprites[0].sprite + i*0xb` (i.e. as if writing _sprites[0..999]) --
 * cross-checked against the raw disassembly, the real absolute
 * addresses used (e.g. `[DI + 0x3cfe]`) are `_sprites[0].sprite`'s
 * address plus 0x41*11, i.e. the loop actually writes _sprites[0x41 + i]
 * for i in 0..999, matching the 0x41..0x428 block-sprite range used
 * throughout MAZEEDIT.C (draw_blocks(), hilite_block(), etc).
 */
int alloc_block_mem(void)
{
    int i;

    _block_mem = calloc(1, 64000);
    if (_block_mem == NULL) {
        return 1;
    }
    for (i = 0; i < 1000; i++) {
        _sprites[0x41 + i].sprite = _block_mem + i * 0x40;
        _sprites[0x41 + i].spritew = 8;
        _sprites[0x41 + i].spriteh = 8;
    }
    return 0;
}

/*
 * Blits a sprite to the visible VGA page, clipped to the screen bounds,
 * via memcpyv() (MEM256.C). Real signature matches the PACWARS.TXT-
 * derived stub exactly.
 */
void display_sprite(int sprite_num)
{
    unsigned char far * dest;
    unsigned char far * src;
    int x, y, w, h;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) +
                                        _sc_width * _sprites[sprite_num].spritey +
                                        _sprites[sprite_num].spritex);
    src = _sprites[sprite_num].sprite;
    x = _sprites[sprite_num].spritex;
    y = _sprites[sprite_num].spritey;
    h = _sprites[sprite_num].spriteh;
    w = _sprites[sprite_num].spritew;

    if (x < 0) {
        w += x;
        src -= x;
        dest -= x;
    } else if (_max_x < x + w) {
        w = _max_x - x;
    }

    if (y < 0) {
        h += y;
        src -= y * _sprites[sprite_num].spritew;
        dest -= y * _sc_width;
    } else if (_max_y < y + h) {
        h -= (y + h) - _max_y;
    }

    if (w > 0 && h > 0) {
        memcpyv(dest, src, w, h, _sc_width - w, _sprites[sprite_num].spritew - w);
    }
}

/*
 * Same clip/blit setup as display_sprite() above, but draws pixel by
 * pixel instead of via memcpyv(): source pixels of colour 0 (the
 * transparent key) are replaced with `colour` instead of being skipped,
 * so the whole sprite bounding box gets painted solid. Real signature
 * matches the PACWARS.TXT-derived stub exactly.
 */
void mix_sprite(int sprite_num, int colour)
{
    unsigned char far * dest;
    unsigned char far * src;
    int x, y, w, h;
    int row, col;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) +
                                        _sc_width * _sprites[sprite_num].spritey +
                                        _sprites[sprite_num].spritex);
    src = _sprites[sprite_num].sprite;
    x = _sprites[sprite_num].spritex;
    y = _sprites[sprite_num].spritey;
    h = _sprites[sprite_num].spriteh;
    w = _sprites[sprite_num].spritew;

    if (x < 0) {
        w += x;
        src -= x;
        dest -= x;
    } else if (_max_x < x + w) {
        w = _max_x - x;
    }

    if (y < 0) {
        h += y;
        src -= y * _sprites[sprite_num].spritew;
        dest -= y * _sc_width;
    } else if (_max_y < y + h) {
        h -= (y + h) - _max_y;
    }

    if (w > 0 && h > 0) {
        for (row = 0; row < h; row++) {
            for (col = 0; col < w; col++) {
                *dest = (*src == 0) ? (unsigned char) colour : *src;
                dest++;
                src++;
            }
            dest += _sc_width - w;
        }
    }
}

/*
 * Same clip/blit setup as display_sprite() above, but clears (to colour
 * 0) every screen pixel where the sprite is opaque (nonzero), instead of
 * drawing it -- used to punch a sprite-shaped hole for effects like the
 * block-picker's highlight overlay. Real signature matches the
 * PACWARS.TXT-derived stub exactly.
 */
void mask_sprite(int sprite_num)
{
    unsigned char far * dest;
    unsigned char far * src;
    int x, y, w, h;
    int row, col;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) +
                                        _sc_width * _sprites[sprite_num].spritey +
                                        _sprites[sprite_num].spritex);
    src = _sprites[sprite_num].sprite;
    x = _sprites[sprite_num].spritex;
    y = _sprites[sprite_num].spritey;
    h = _sprites[sprite_num].spriteh;
    w = _sprites[sprite_num].spritew;

    if (x < 0) {
        w += x;
        src -= x;
        dest -= x;
    } else if (_max_x < x + w) {
        w = _max_x - x;
    }

    if (y < 0) {
        h += y;
        src -= y * _sprites[sprite_num].spritew;
        dest -= y * _sc_width;
    } else if (_max_y < y + h) {
        h -= (y + h) - _max_y;
    }

    if (w > 0 && h > 0) {
        for (row = 0; row < h; row++) {
            for (col = 0; col < w; col++) {
                if (*src != 0) {
                    *dest = 0;
                }
                dest++;
                src++;
            }
            dest += _sc_width - w;
        }
    }
}

/*
 * Same clip/blit setup as display_sprite() above, but draws pixel by
 * pixel with the source's colour 0 treated as transparent (skipped)
 * rather than drawn -- a standard transparent-colour-key sprite blit.
 * Real signature matches the PACWARS.TXT-derived stub exactly.
 */
void or_sprite(int sprite_num)
{
    unsigned char far * dest;
    unsigned char far * src;
    int x, y, w, h;
    int row, col;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) +
                                        _sc_width * _sprites[sprite_num].spritey +
                                        _sprites[sprite_num].spritex);
    src = _sprites[sprite_num].sprite;
    x = _sprites[sprite_num].spritex;
    y = _sprites[sprite_num].spritey;
    h = _sprites[sprite_num].spriteh;
    w = _sprites[sprite_num].spritew;

    if (x < 0) {
        w += x;
        src -= x;
        dest -= x;
    } else if (_max_x < x + w) {
        w = _max_x - x;
    }

    if (y < 0) {
        h += y;
        src -= y * _sprites[sprite_num].spritew;
        dest -= y * _sc_width;
    } else if (_max_y < y + h) {
        h -= (y + h) - _max_y;
    }

    if (w > 0 && h > 0) {
        for (row = 0; row < h; row++) {
            for (col = 0; col < w; col++) {
                if (*src != 0) {
                    *dest = *src;
                }
                dest++;
                src++;
            }
            dest += _sc_width - w;
        }
    }
}

/*
 * Clears a sprite's screen footprint to colour 0 (used before moving a
 * sprite, to erase its previous position). Unlike the blit functions
 * above, this never touches the sprite's source bitmap at all -- only
 * its position/size. Real signature matches the PACWARS.TXT-derived
 * stub exactly.
 */
void erase_sprite(int sprite_num)
{
    unsigned char far * dest;
    int x, y, w, h;
    int row;

    dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) +
                                        _sc_width * _sprites[sprite_num].spritey +
                                        _sprites[sprite_num].spritex);
    x = _sprites[sprite_num].spritex;
    y = _sprites[sprite_num].spritey;
    h = _sprites[sprite_num].spriteh;
    w = _sprites[sprite_num].spritew;

    if (x < 0) {
        w += x;
        dest -= x;
    } else if (_max_x < x + w) {
        w = _max_x - x;
    }

    if (y < 0) {
        h += y;
        dest -= y * _sc_width;
    } else if (_max_y < y + h) {
        h += _max_y - (y + h);
    }

    if (w > 0 && h > 0) {
        for (row = 0; row < h; row++) {
            memset(dest, 0, w);
            dest += _sc_width;
        }
    }
}

/*
 * Reads a sprite's dimensions and bitmap from a .SPT file (1 byte width,
 * 1 byte height, then width*height raw pixel bytes), allocating a fresh
 * buffer for _sprites[sprite_num].sprite only if it doesn't already have
 * one (so this can also be used to reload/replace an existing slot).
 *
 * NOTE on open() flags (231e:0783-0794): the decompile showed the mode
 * argument as an opaque `(uchar *)_scratch_buffer + 0xf21` expression --
 * that's decompiler noise; the raw disassembly's actual pushed
 * immediates are `PUSH 0x180` and `PUSH -0x7fbf` (= 0x8041 unsigned).
 * 0x8041 = O_BINARY(0x8000) | O_DENYNONE(0x40) | O_RDONLY(0x1); 0x180 =
 * S_IREAD(0x100) | S_IWRITE(0x80), confirmed against this project's own
 * BORLANDC/INCLUDE/FCNTL.H and SYS/STAT.H.
 */
int read_sprite(int sprite_num, char far * file_name)
{
    char file_str[80];
    unsigned char sprite_w;
    unsigned char sprite_h;
    int status;
    int fd;

    status = -1;
    sprite_w = 0;
    sprite_h = 0;

    strcpy(file_str, file_name);
    get_filename(file_str);

    fd = open(file_str, O_RDONLY | O_BINARY | O_DENYNONE, S_IREAD | S_IWRITE);
    if (fd != -1) {
        read(fd, &sprite_w, 1);
        read(fd, &sprite_h, 1);
        _sprites[sprite_num].spritew = sprite_w;
        _sprites[sprite_num].spriteh = sprite_h;
        if (_sprites[sprite_num].sprite == NULL) {
            _sprites[sprite_num].sprite = calloc(1, (unsigned int) sprite_w * (unsigned int) sprite_h);
        }
        status = read(fd, _sprites[sprite_num].sprite, (unsigned int) sprite_w * (unsigned int) sprite_h);
        close(fd);
    }

    if ((unsigned int) status != (unsigned int) sprite_w * (unsigned int) sprite_h) {
        printf("Cannot Load %s", file_str);
        return -1;
    }
    return 0;
}

/*
 * Loads every non-block sprite (character/object animation frames and the
 * named "kill" death-animation sequences, sprite_num 0..120, 1065..1247)
 * from its own .SPT file via read_sprite().
 *
 * NOTE: Ghidra's decompile corrupted the filename argument identically
 * on all 302 individual read_sprite() calls in the original code (shown
 * as a bogus constant), while the accompanying sprite_num integer
 * arguments stayed reliable. The real filename address for each call was
 * recovered from the raw disassembly's `PUSH DS / PUSH <offset>`
 * sequence immediately before each call (each offset is DS:<offset>,
 * i.e. absolute 340e:<offset>), then resolved to exact text via
 * read_memory() over the full 340e:17dc string table. The result (302
 * (sprite_num, filename) pairs -- more than the 233 unique files because
 * several kill-animation frame slots intentionally reuse the same file)
 * is reconstructed here as a static table + loop, which is behaviorally
 * identical to the original's 302 unrolled calls but far more compact.
 */
static const struct {
    int sprite_num;
    char far * file_name;
} _sprite_load_table[] = {
    {0, "PACGHOST.SPT"}, {1, "PACBLANK.SPT"}, {2, "PACIR.SPT"}, {3, "PACIR2.SPT"},
    {4, "PACMAN.SPT"}, {5, "PACMAN2.SPT"}, {6, "PACMAN3.SPT"}, {7, "PACMAN4.SPT"},
    {8, "PACTHING.SPT"}, {9, "PACTHNG2.SPT"}, {10, "PACTHNG3.SPT"}, {11, "PACTHNG4.SPT"},
    {12, "PACRAT.SPT"}, {13, "PACRAT1.SPT"}, {14, "PACRAT2.SPT"}, {15, "PACRAT3.SPT"},
    {16, "PACGIRL.SPT"}, {17, "PACGIRL2.SPT"}, {18, "PACGIRL3.SPT"}, {19, "PACGIRL4.SPT"},
    {20, "PACBLOB.SPT"}, {21, "PACBLOB2.SPT"}, {22, "PACBLOB3.SPT"}, {23, "PACBLOB4.SPT"},
    {24, "PACRAMBO.SPT"}, {25, "PACRMBO2.SPT"}, {26, "PACRMBO3.SPT"}, {27, "PACRMBO4.SPT"},
    {28, "PACPERV1.SPT"}, {29, "PACPERV2.SPT"}, {30, "PACPERV3.SPT"}, {31, "PACPERV4.SPT"},
    {32, "PACTURD1.SPT"}, {33, "PACTURD2.SPT"}, {34, "PACTURD3.SPT"}, {35, "PACTURD4.SPT"},
    {36, "BIGGLES1.SPT"}, {37, "BIGGLES2.SPT"}, {38, "BIGGLES3.SPT"}, {39, "BIGGLES4.SPT"},
    {40, "BUM3.SPT"}, {41, "BUM4.SPT"}, {42, "BUM1.SPT"}, {43, "BUM2.SPT"},
    {44, "BULLET.SPT"}, {45, "BULLET.SPT"}, {46, "BULLET2.SPT"}, {47, "BULLET2.SPT"},
    {48, "RATBULL1.SPT"}, {49, "RATBULL2.SPT"}, {50, "BULLET4.SPT"}, {51, "BULLET42.SPT"},
    {52, "BULLET5.SPT"}, {53, "BULLET5.SPT"}, {54, "BULLET6.SPT"}, {55, "BULLET62.SPT"},
    {56, "BULLET7.SPT"}, {57, "BULLET72.SPT"}, {58, "BULTURD1.SPT"}, {59, "BULTURD1.SPT"},
    {60, "BULBIGG1.SPT"}, {61, "BULBIGG2.SPT"}, {62, "BULTURD1.SPT"}, {63, "BULTURD1.SPT"},
    {1239, "SBULL1.SPT"}, {1240, "SBULL2.SPT"}, {1241, "SBULL3.SPT"}, {1242, "SBULL4.SPT"},
    {1243, "SBULL5.SPT"}, {1244, "SBULL6.SPT"}, {1245, "SBULL7.SPT"}, {1246, "SBULL8.SPT"},
    {1247, "SBULL9.SPT"}, {64, "PEXP1.SPT"}, {65, "WALL.SPT"}, {66, "WALL2.SPT"},
    {67, "WALL3.SPT"}, {68, "WALL4.SPT"}, {69, "WALL5.SPT"}, {70, "WALL6.SPT"},
    {71, "BUSH.SPT"}, {72, "WALL7.SPT"}, {73, "WALL8.SPT"}, {74, "SPIKEUP.SPT"},
    {75, "SPIKEDN.SPT"}, {76, "ZAP1.SPT"}, {77, "ZAP2.SPT"}, {78, "ZAP3.SPT"},
    {79, "ZAP4.SPT"}, {80, "SNAKE0.SPT"}, {81, "SNAKE1.SPT"}, {82, "SNAKE2.SPT"},
    {83, "SNAKE3.SPT"}, {84, "SNAKE4.SPT"}, {85, "SNAKE8.SPT"}, {86, "SNAKE10.SPT"},
    {87, "SNAKE12.SPT"}, {88, "SNAKE14.SPT"}, {89, "SNAKE16.SPT"}, {90, "SNAKE17.SPT"},
    {93, "LOG1.SPT"}, {94, "LOG2.SPT"}, {95, "LOG3.SPT"}, {96, "LOG4.SPT"},
    {97, "LOG5.SPT"}, {98, "LOG6.SPT"}, {99, "LOG7.SPT"}, {100, "BRICKY1.SPT"},
    {101, "BRICKY2.SPT"}, {102, "BRICKY3.SPT"}, {103, "BRICKY4.SPT"}, {104, "PL11L.SPT"},
    {105, "PL11R.SPT"}, {106, "PL12L.SPT"}, {107, "PL12R.SPT"}, {108, "PL13L.SPT"},
    {109, "PL13R.SPT"}, {110, "PT11L.SPT"}, {111, "PT11R.SPT"}, {112, "PT12L.SPT"},
    {113, "PT12R.SPT"}, {114, "PT13L.SPT"}, {115, "PT13R.SPT"}, {116, "PL31L.SPT"},
    {117, "PL31R.SPT"}, {118, "ONEWAY1.SPT"}, {119, "ONEWAY2.SPT"}, {120, "BOUNCE.SPT"},
    {1065, "GOLD1.SPT"}, {1066, "GOLD2.SPT"}, {1067, "GOLD3.SPT"}, {1068, "WARP.SPT"},
    {1069, "WARP.SPT"}, {1070, "ISHOT.SPT"}, {1071, "ISHOT.SPT"}, {1072, "BEER.SPT"},
    {1073, "BEER2.SPT"}, {1074, "SGLASS.SPT"}, {1075, "SGLASS2.SPT"}, {1076, "BOMB.SPT"},
    {1077, "BOMB2.SPT"}, {1078, "GLUE.SPT"}, {1079, "GLUE.SPT"}, {1080, "SBULL1.SPT"},
    {1081, "SBULL9.SPT"}, {1082, "SHIELD.SPT"}, {1083, "SHIELD1.SPT"}, {1084, "TBOMB.SPT"},
    {1085, "TBOMB.SPT"}, {1086, "QMARK.SPT"}, {1087, "QMARK.SPT"}, {1088, "DIAMOND.SPT"},
    {1089, "DIAMOND.SPT"}, {1090, "DIAMOND.SPT"}, {1091, "DIAMOND.SPT"}, {1092, "SBALL.SPT"},
    {1093, "SBALL.SPT"}, {1094, "SBALL.SPT"}, {1095, "SBALL1.SPT"}, {1096, "SBALL1.SPT"},
    {1097, "SBALL2.SPT"}, {1098, "SBALL3.SPT"}, {1099, "SBALL3.SPT"}, {1100, "SBALL.SPT"},
    {1101, "SBALL.SPT"}, {1102, "SBALL.SPT"}, {1103, "SBALL1.SPT"}, {1104, "SBALL1.SPT"},
    {1105, "SBALL2.SPT"}, {1106, "SBALL3.SPT"}, {1107, "SBALL3.SPT"}, {1108, "WICON.SPT"},
    {1109, "SICON.SPT"}, {1110, "GICON.SPT"}, {1111, "HICON.SPT"}, {1112, "SHICON.SPT"},
    {1113, "GRENADE.SPT"}, {1114, "CURSOR.SPT"}, {1115, "GRENADE.SPT"}, {1116, "GRENADE1.SPT"},
    {1117, "GRENEXP.SPT"}, {1118, "GRENEXP.SPT"}, {1119, "MANKILL1.SPT"}, {1120, "MANKILL2.SPT"},
    {1121, "MANKILL3.SPT"}, {1122, "MANKILL4.SPT"}, {1123, "MANKILL5.SPT"}, {1124, "MANKILL6.SPT"},
    {1125, "MANKILL7.SPT"}, {1126, "MANKILL8.SPT"}, {1127, "MANKILL9.SPT"}, {1128, "MANKILLA.SPT"},
    {1129, "MANKILLB.SPT"}, {1130, "MANKILLC.SPT"}, {1131, "PSYKILL1.SPT"}, {1132, "PSYKILL2.SPT"},
    {1133, "PSYKILL3.SPT"}, {1134, "PSYKILL4.SPT"}, {1135, "PSYKILL5.SPT"}, {1136, "PSYKILL6.SPT"},
    {1137, "PSYKILL7.SPT"}, {1138, "PSYKILL8.SPT"}, {1139, "PSYKILL9.SPT"}, {1140, "PSYKILLA.SPT"},
    {1141, "PSYKILLB.SPT"}, {1142, "PSYKILLC.SPT"}, {1143, "RATKILL1.SPT"}, {1144, "RATKILL2.SPT"},
    {1145, "RATKILL3.SPT"}, {1146, "RATKILL4.SPT"}, {1147, "RATKILL5.SPT"}, {1148, "RATKILL6.SPT"},
    {1149, "RATKILL7.SPT"}, {1150, "RATKILL8.SPT"}, {1151, "RATKILL9.SPT"}, {1152, "RATKILLA.SPT"},
    {1153, "RATKILLB.SPT"}, {1154, "RATKILLC.SPT"}, {1155, "GRLKILL1.SPT"}, {1156, "GRLKILL2.SPT"},
    {1157, "GRLKILL3.SPT"}, {1158, "GRLKILL4.SPT"}, {1159, "GRLKILL5.SPT"}, {1160, "GRLKILL6.SPT"},
    {1161, "GRLKILL7.SPT"}, {1162, "GRLKILL8.SPT"}, {1163, "GRLKILL9.SPT"}, {1164, "GRLKILLA.SPT"},
    {1165, "GRLKILLB.SPT"}, {1166, "GRLKILLC.SPT"}, {1167, "BLBKILL1.SPT"}, {1168, "BLBKILL2.SPT"},
    {1169, "BLBKILL3.SPT"}, {1170, "BLBKILL4.SPT"}, {1171, "BLBKILL5.SPT"}, {1172, "BLBKILL6.SPT"},
    {1173, "BLBKILL7.SPT"}, {1174, "BLBKILL8.SPT"}, {1175, "BLBKILL9.SPT"}, {1176, "BLBKILLA.SPT"},
    {1177, "BLBKILLB.SPT"}, {1178, "BLBKILLC.SPT"}, {1179, "RMBKILL1.SPT"}, {1180, "RMBKILL2.SPT"},
    {1181, "RMBKILL3.SPT"}, {1182, "RMBKILL4.SPT"}, {1183, "RMBKILL5.SPT"}, {1184, "RMBKILL6.SPT"},
    {1185, "RMBKILL7.SPT"}, {1186, "RMBKILL8.SPT"}, {1187, "RMBKILL9.SPT"}, {1188, "RMBKILLA.SPT"},
    {1189, "RMBKILLB.SPT"}, {1190, "RMBKILLC.SPT"}, {1191, "PRVKILL1.SPT"}, {1192, "PRVKILL2.SPT"},
    {1193, "PRVKILL3.SPT"}, {1194, "PRVKILL4.SPT"}, {1195, "PRVKILL5.SPT"}, {1196, "PRVKILL6.SPT"},
    {1197, "PRVKILL7.SPT"}, {1198, "PRVKILL8.SPT"}, {1199, "PRVKILL9.SPT"}, {1200, "PRVKILLA.SPT"},
    {1201, "PRVKILLB.SPT"}, {1202, "PRVKILLC.SPT"}, {1203, "TRDKILL1.SPT"}, {1204, "TRDKILL2.SPT"},
    {1205, "TRDKILL3.SPT"}, {1206, "TRDKILL4.SPT"}, {1207, "TRDKILL5.SPT"}, {1208, "TRDKILL6.SPT"},
    {1209, "TRDKILL7.SPT"}, {1210, "TRDKILL8.SPT"}, {1211, "TRDKILL9.SPT"}, {1212, "TRDKILLA.SPT"},
    {1213, "TRDKILLB.SPT"}, {1214, "TRDKILLC.SPT"}, {1215, "BGLKILL1.SPT"}, {1216, "BGLKILL2.SPT"},
    {1217, "BGLKILL3.SPT"}, {1218, "BGLKILL4.SPT"}, {1219, "BGLKILL5.SPT"}, {1220, "BGLKILL6.SPT"},
    {1221, "BGLKILL7.SPT"}, {1222, "BGLKILL8.SPT"}, {1223, "BGLKILL9.SPT"}, {1224, "BGLKILLA.SPT"},
    {1225, "BGLKILLB.SPT"}, {1226, "BGLKILLC.SPT"}, {1233, "BUMKILL1.SPT"}, {1234, "BUMKILL2.SPT"},
    {1235, "BUMKILL3.SPT"}, {1236, "BUMKILL4.SPT"}, {1237, "BUMKILL5.SPT"}, {1238, "BUMKILL6.SPT"},
    {1227, "BUMKILL7.SPT"}, {1228, "BUMKILL8.SPT"}, {1229, "BUMKILL9.SPT"}, {1230, "BUMKILLA.SPT"},
    {1231, "BUMKILLB.SPT"}, {1232, "BUMKILLC.SPT"}
};

void load_sprites(void)
{
    int i;

    for (i = 0; i < sizeof(_sprite_load_table) / sizeof(_sprite_load_table[0]); i++) {
        read_sprite(_sprite_load_table[i].sprite_num, _sprite_load_table[i].file_name);
    }
}

/*
 * Opens a sprite-array data file (e.g. msprites.dat) with a caller-
 * supplied base access mode ORed with the flags this project's other
 * file opens use (O_BINARY | O_DENYNONE), same pmode as read_sprite().
 * NOTE (231e:18db-18e7): the decompile showed the access-mode argument
 * as `unaff_SS`; the raw disassembly shows `MOV AX,0x8000 / OR AX,mode
 * / OR AX,0x40` -- i.e. the real expression is `mode | 0x8040`
 * (O_BINARY | O_DENYNONE), confirmed against FCNTL.H.
 */
int open_sprite_array(char far * file_name, int mode)
{
    char file_str[80];

    strcpy(file_str, file_name);
    get_filename(file_str);

    return open(file_str, mode | O_BINARY | O_DENYNONE, S_IREAD | S_IWRITE);
}

void close_sprite_array(int array_fd)
{
    close(array_fd);
}

void load_all_sprites(void)
{
    int array_fd;
    unsigned int sprite_num;

    alloc_block_mem();
    alloc_maze_def_mem();
    array_fd = open_sprite_array("msprites.dat", 1);
    for (sprite_num = 0; sprite_num < 0x41; sprite_num++) {
        read_sprite_array(sprite_num, array_fd);
    }
    load_block_array(array_fd);
    for (sprite_num = 0x429; sprite_num < 0x4e0; sprite_num++) {
        read_sprite_array(sprite_num, array_fd);
    }
    load_maze(array_fd);
    load_names(array_fd);
    close_sprite_array(array_fd);
}

/*
 * The 1000-block sprite arena (_block_mem, 64000 bytes) is read in two
 * 32000-byte halves -- confirmed via the raw disassembly (PUSH SI where
 * SI=0x7d00=32000, done twice, second time with the buffer offset
 * advanced by 32000) rather than one 64000-byte transfer, presumably to
 * stay under a signed 16-bit transfer-size limit in this DOS read().
 */
void load_block_array(int fd)
{
    read(fd, _block_mem, 32000);
    read(fd, _block_mem + 32000, 32000);
}

void save_block_array(int fd)
{
    write(fd, _block_mem, 32000);
    write(fd, _block_mem + 32000, 32000);
}

/*
 * One-shot format-conversion helper: loads the full old-style sprite set
 * (character frames 0x0..0x40, an extra 0x41..0x108 range not touched by
 * the normal load_all_sprites(), and the named 0x429..0x4df range) plus
 * the maze and names, then immediately calls save_all_sprites() to
 * rewrite everything back out in the current on-disk layout/order.
 */
void conv_all_sprites(void)
{
    int array_fd;
    unsigned int sprite_num;

    printf("\nLoading...");
    alloc_block_mem();
    alloc_maze_def_mem();
    array_fd = open_sprite_array("msprites.dat", O_RDONLY);
    for (sprite_num = 0; sprite_num < 0x41; sprite_num++) {
        read_sprite_array(sprite_num, array_fd);
    }
    for (sprite_num = 0x41; sprite_num < 0x109; sprite_num++) {
        read_sprite_array(sprite_num, array_fd);
    }
    for (sprite_num = 0x429; sprite_num < 0x4e0; sprite_num++) {
        read_sprite_array(sprite_num, array_fd);
    }
    load_maze(array_fd);
    load_names(array_fd);
    close_sprite_array(array_fd);
    printf("\nSaving...");
    save_all_sprites();
}

void save_all_sprites(void)
{
    int array_fd;
    unsigned int sprite_num;

    /*
     * NOTE (231e:1ac0-1ac8): forces sprite slot 1 (PacBlank) to a fixed
     * 16x16 size before writing, regardless of whatever size it
     * currently has in memory -- kept as-is, matches the original.
     */
    _sprites[1].spritew = 0x10;
    _sprites[1].spriteh = 0x10;

    array_fd = open_sprite_array("msprites.dat", O_WRONLY | O_CREAT | O_TRUNC);
    for (sprite_num = 0; sprite_num < 0x41; sprite_num++) {
        write_sprite_array(sprite_num, array_fd);
    }
    save_block_array(array_fd);
    for (sprite_num = 0x429; sprite_num < 0x4e0; sprite_num++) {
        write_sprite_array(sprite_num, array_fd);
    }
    save_maze(array_fd);
    save_names(array_fd);
    close_sprite_array(array_fd);
}

/*
 * Loads one playable character's name plus its 3 sprite-frame groups
 * (facing/animation frames, 4+12+2 = 18 sprites total) from its own
 * per-character file, using get_sprite_data() (MANEDIT.C) to look up
 * each group's base sprite_num for this character.
 */
void load_character(int curr_pacman, char far * file_name)
{
    int array_fd;
    unsigned int base, sprite_num;

    array_fd = open_sprite_array(file_name, O_RDONLY);
    if (array_fd != -1) {
        load_name(array_fd, curr_pacman);
        base = get_sprite_data(curr_pacman, 1, 0);
        for (sprite_num = base; sprite_num < base + 4; sprite_num++) {
            read_sprite_array(sprite_num, array_fd);
        }
        base = get_sprite_data(curr_pacman, 2, 0);
        for (sprite_num = base; sprite_num < base + 0xc; sprite_num++) {
            read_sprite_array(sprite_num, array_fd);
        }
        base = get_sprite_data(curr_pacman, 3, 0);
        for (sprite_num = base; sprite_num < base + 2; sprite_num++) {
            read_sprite_array(sprite_num, array_fd);
        }
        close_sprite_array(array_fd);
    }
}

void save_character(int curr_pacman, char far * file_name)
{
    int array_fd;
    unsigned int base, sprite_num;

    array_fd = open_sprite_array(file_name, O_WRONLY | O_CREAT | O_TRUNC);
    if (array_fd != -1) {
        save_name(array_fd, curr_pacman);
        base = get_sprite_data(curr_pacman, 1, 0);
        for (sprite_num = base; sprite_num < base + 4; sprite_num++) {
            write_sprite_array(sprite_num, array_fd);
        }
        base = get_sprite_data(curr_pacman, 2, 0);
        for (sprite_num = base; sprite_num < base + 0xc; sprite_num++) {
            write_sprite_array(sprite_num, array_fd);
        }
        base = get_sprite_data(curr_pacman, 3, 0);
        for (sprite_num = base; sprite_num < base + 2; sprite_num++) {
            write_sprite_array(sprite_num, array_fd);
        }
        /* NOTE (231e:1c48): save_name() is called again here, after the
           sprite writes -- redundant with the call above but matches
           the original's actual instruction sequence. */
        save_name(array_fd, curr_pacman);
        close_sprite_array(array_fd);
    }
}

void read_sprite_array(int sprite_num, int array_fd)
{
    int wh;

    read(array_fd, _sp_head, 2);
    _sprites[sprite_num].spritew = _sp_head[0];
    _sprites[sprite_num].spriteh = _sp_head[1];
    wh = (int) _sp_head[0] * (int) _sp_head[1];
    if (wh > 0) {
        if (_sprites[sprite_num].sprite == NULL) {
            _sprites[sprite_num].sprite = calloc(1, wh);
        }
        if (_sprites[sprite_num].sprite != NULL) {
            read(array_fd, _sprites[sprite_num].sprite, wh);
        }
    }
}

void write_sprite_array(int sprite_num, int array_fd)
{
    _sp_head[0] = _sprites[sprite_num].spritew;
    _sp_head[1] = (unsigned char) _sprites[sprite_num].spriteh;
    write(array_fd, _sp_head, 2);
    if ((int) _sp_head[0] * (int) _sp_head[1] > 0 && _sprites[sprite_num].sprite != NULL) {
        write(array_fd, _sprites[sprite_num].sprite,
              (unsigned int) _sp_head[0] * (unsigned int) _sp_head[1]);
    }
}

/*
 * NOTE (231e:1e0f-1e28): the decompile showed the read/write length as
 * a bogus `0x340e` (the string-table segment number, misread as a byte
 * count) -- the real count, confirmed against _pacname's own layout, is
 * sizeof(_pacname) = 130 (10 * 13).
 */
void load_names(int array_fd)
{
    read(array_fd, _pacname, sizeof(_pacname));
}

void save_names(int array_fd)
{
    write(array_fd, _pacname, sizeof(_pacname));
}

/*
 * NOTE: same `0x340e` decompiler artifact as load_names()/save_names()
 * above -- the real per-record count is sizeof(_pacname[0]) = 13.
 */
void load_name(int array_fd, int curr_pacman)
{
    read(array_fd, _pacname[curr_pacman], sizeof(_pacname[0]));
}

void save_name(int array_fd, int curr_pacman)
{
    write(array_fd, _pacname[curr_pacman], sizeof(_pacname[0]));
}

/*
 * Steps every animated maze-cell entry in a room's animation list (obj[],
 * terminated by hoffset==-1) whose underlying maze cell value has changed
 * since last drawn, and redraws it: obj[i].hoffset/voffset actually hold
 * the maze row/col of the animated cell (same repurposing as
 * update_room()'s use of these fields, MAZEUTIL.C), while obj[i].animate
 * is repurposed here as a per-entry cache of the last-drawn cell value
 * and obj[i].redraw selects the blit mode (or_sprite vs display_sprite).
 *
 * NOTE (231e:1ef0-1f16): spritey/spritex are computed from this sprite
 * slot's OWN previous spritew/spriteh (not yet updated this call) before
 * spritew/spriteh are reset from _sprites[0x41] below -- and spriteh is
 * actually set from _sprites[0x41].spritew (its width field, not
 * height). Harmless in practice since all block sprites are square
 * (8x8), but preserved exactly as compiled.
 */
void animate_room(ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
    int i;
    int row, col;
    int sprite_num;
    char cell;

    for (i = 0; obj[i].hoffset != -1; i++) {
        row = obj[i].hoffset;
        col = obj[i].voffset;
        cell = (char) maze_ptr->def[row][col];
        if (obj[i].animate != cell) {
            if (maze_ptr->def[row][col] == 0) {
                sprite_num = 1;
            } else {
                sprite_num = cell + 0x40;
            }
            _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
            _sprites[sprite_num].spritex = col * _sprites[sprite_num].spritew;
            _sprites[sprite_num].spritew = _sprites[0x41].spritew;
            _sprites[sprite_num].spriteh = _sprites[0x41].spritew;
            if (obj[i].redraw == 1) {
                or_sprite(sprite_num);
            } else {
                display_sprite(sprite_num);
            }
            obj[i].animate = cell;
        }
    }
}

/*
 * Erases every animated-cell entry in a room's animation list whose
 * redraw flag is set and whose cached state (obj[i].animate) differs
 * from the maze cell's current value -- unlike animate_room() above,
 * this uses the OLD cached state (not the new cell value) to pick which
 * sprite to erase, and never updates the cache afterward (this function
 * is for tearing a room's animations down, not stepping them forward).
 */
void clear_animates(ANIM_OB far * obj, MAZE_STRUCT far * maze_ptr)
{
    int i;
    int row, col;
    int sprite_num;
    unsigned int cell;

    for (i = 0; obj[i].hoffset != -1; i++) {
        row = obj[i].hoffset;
        col = obj[i].voffset;
        cell = maze_ptr->def[row][col];
        if (obj[i].redraw == 1 && obj[i].animate != (char) cell) {
            if (cell == 0) {
                sprite_num = 1;
            } else {
                sprite_num = obj[i].animate + 0x40;
            }
            _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
            _sprites[sprite_num].spritex = col * _sprites[sprite_num].spritew;
            _sprites[sprite_num].spritew = _sprites[0x41].spritew;
            _sprites[sprite_num].spriteh = _sprites[0x41].spritew;
            if (cell == 0) {
                display_sprite(sprite_num);
            } else {
                mask_sprite(sprite_num);
            }
        }
    }
}

/*
 * Crude Gaussian approximation via the Irwin-Hall method: sums 20
 * differences of two uniform rand() draws (approximating a normal
 * distribution), averages, then reduces mod std.
 */
int gauss(int std)
{
    long sum;
    int i;

    sum = 0;
    for (i = 0; i < 20; i++) {
        sum += rand() - rand();
    }
    return (int) (sum / 20) % std;
}

/*
 * Draws a status-bar icon (sprite index `sprite`) with an optional
 * single-digit count overlay at (x,y). num==0 just clears the icon slot
 * to background colour; num<0 draws the icon with no digit; num>0 draws
 * the icon plus its count, clamped to a single digit (9 max).
 *
 * NOTE: the original stored the digit character in a single shared
 * global byte (DAT_340e_e578) and relied on text256()'s null-terminated-
 * string scan finding a zero in whatever byte happened to follow it in
 * the data segment -- not reproducible in a freshly-linked binary, so a
 * proper 2-byte null-terminated local buffer is used here instead; the
 * visible behaviour (draw exactly one digit) is identical.
 */
void icon(int x, int y, int sprite, int num)
{
    int y_pos;
    int old_font;
    char digit_str[2];

    y_pos = y + 0x18;
    if (num == 0) {
        trfbox(x - 8, y_pos, 0x10, 8, 0x35);
    } else {
        _sprites[0x4e0].spritew = _sprites[sprite].spritew;
        _sprites[0x4e0].spriteh = _sprites[sprite].spriteh;
        _sprites[0x4e0].sprite = _sprites[sprite].sprite;
        _sprites[0x4e0].spritex = x - 8;
        _sprites[0x4e0].spritey = y_pos;
        mix_sprite(0x4e0, 0x35);
        old_font = SetTextFont(-1);
        SetTextFont(4);
        if (num > 0) {
            if (num >= 10) {
                num = 9;
            }
            digit_str[0] = (char) num + '0';
            digit_str[1] = '\0';
            text256(x + 1, y_pos, (unsigned char far *) digit_str, 0xf, 0x35);
        }
        SetTextFont(old_font);
    }
}

/*
 * Adds `amount` to player `man`'s score, then grants an extra life
 * ("man") for every full 100-point threshold newly crossed since the
 * last time hiscore[man] (a per-100-points counter) was updated.
 */
void add_score(int man, int amount, MAZE_LOG_STRUCT far * maze_log)
{
    int thou;

    maze_log->score[man] += amount;
    thou = maze_log->score[man] / 100;
    if (maze_log->hiscore[man] < thou) {
        maze_log->men[man] += (unsigned char) (thou - maze_log->hiscore[man]);
        maze_log->hiscore[man] = (char) thou;
    }
}

/*
 * Checks whether sprite `sp` is currently touching a "bounce" wall
 * attribute (3) along its leading edge in the direction of travel
 * (dir==1 moving right / dir==-1 moving left), scanning every 8px tile
 * row the sprite's height spans. Reconstructed directly from the raw
 * disassembly (231e:2348) rather than the decompile, which corrupted
 * every pointer-typed value in this function via CONCAT22/in_stack
 * register-splitting artifacts; the underlying arithmetic/branch
 * structure was otherwise sound and is preserved as-is.
 */
int test_bounce(int dir, SPRITE_STRUCT far * sp, int hoff, int voff)
{
    MAZE_STRUCT far * attrib_ptr;
    int row0, col0, w8, h;
    int i;

    if (sp->spritex > 0 && sp->spritex + sp->spritew < _max_x) {
        attrib_ptr = (MAZE_STRUCT far *) attrib_maze_def(hoff, voff);
        row0 = sp->spritey >> 3;
        col0 = sp->spritex >> 3;
        h = sp->spriteh >> 3;
        if (sp->spritey & 7) {
            h++;
        }
        w8 = sp->spritew >> 3;

        for (i = 0; i < h; i++) {
            if (dir == 1) {
                if (attrib_ptr->def[row0 + i][col0] == 3) {
                    return 1;
                }
                if (attrib_ptr->def[row0 + i][col0 + w8] == 3) {
                    return 1;
                }
            }
            if (dir == -1) {
                if (attrib_ptr->def[row0 + i][col0] == 3) {
                    return 1;
                }
                if ((sp->spritex & 7) == 0 && attrib_ptr->def[row0 + i][col0 - 1] == 3) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int isblock(SPRITE_STRUCT far * sp, int hoff, int voff)
{
    return test_for_block(sp->spritex, sp->spritey, (unsigned int) sp->spritew, sp->spriteh, hoff, voff);
}

/*
 * Scans the w x h pixel box at (x,y) (converted to 8px tiles) for a
 * blocking maze cell, wrapping across room boundaries (_HSIZE x _VSIZE
 * grid) as needed. Returns: a positive maze-cell value if a solid,
 * non-bounce/non-warp (attribute-code 0x36/0x37 excluded) obstacle is
 * hit; -1 if only a "soft" block (attribute 1 or 2) was found; _spike if
 * a spike-attribute (4) cell was found; 0 if the path is clear.
 *
 * NOTE (231e:24d8-24f6): the disassembly shows maze_def(hoff,voff) and
 * attrib_maze_def(hoff,voff) called once at the very top with their
 * results discarded -- both get recomputed inside the loop below before
 * first use, so this priming call has no effect. Kept for fidelity
 * (reconstructed from the raw disassembly, not the decompile, which
 * mistyped hoff/voff as pointers throughout due to calling-convention
 * confusion).
 */
int test_for_block(int x, int y, int w, int h, int hoff, int voff)
{
    MAZE_STRUCT far * maze_ptr;
    MAZE_STRUCT far * attrib_ptr;
    int row, col;
    int cur_row, cur_voff;
    int i, j;
    int cell, attrib;
    int block_sp;

    block_sp = 0;

    maze_def(hoff, voff);
    attrib_maze_def(hoff, voff);

    row = y / 8;
    col = x / 8;
    h >>= 3;
    if (y & 7) {
        h++;
    }
    w >>= 3;
    if (x & 7) {
        w++;
    }

    if (col < 0) {
        hoff--;
        col += 0x1e;
        if (hoff < 0) {
            hoff = _HSIZE - 1;
        }
    }
    if (row < 0) {
        voff--;
        row += 0x19;
        if (voff < 0) {
            voff = _VSIZE - 1;
        }
    }

    for (i = 0; i < w; i++) {
        if (col == 0x1e) {
            hoff++;
            col = 0;
            if (_HSIZE <= hoff) {
                hoff = 0;
            }
        }
        maze_ptr = (MAZE_STRUCT far *) maze_def(hoff, voff);
        attrib_ptr = (MAZE_STRUCT far *) attrib_maze_def(hoff, voff);
        cur_voff = voff;
        cur_row = row;

        for (j = 0; j < h; j++) {
            if (cur_row == 0x19) {
                cur_voff++;
                cur_row = 0;
                if (_VSIZE <= cur_voff) {
                    cur_voff = 0;
                }
                maze_ptr = (MAZE_STRUCT far *) maze_def(hoff, cur_voff);
                attrib_ptr = (MAZE_STRUCT far *) attrib_maze_def(hoff, cur_voff);
            }
            cell = maze_ptr->def[cur_row][col];
            attrib = attrib_ptr->def[cur_row][col];
            if (cell != 0x36 && cell != 0x37) {
                if (attrib == 1 || attrib == 2) {
                    if (block_sp == 0) {
                        block_sp = -1;
                    }
                } else if (attrib == 4) {
                    block_sp = _spike;
                } else if (cell > 0) {
                    return cell;
                }
            }
            cur_row++;
        }
        col++;
    }
    return block_sp;
}

/*
 * Tests whether sprite `sp` could move `dir` pixels horizontally without
 * hitting a block, by tentatively moving it, checking isblock(), then
 * moving it back. Reconstructed from the raw disassembly (231e:2687):
 * the decompile corrupted the isblock() call's arguments via CONCAT22,
 * but the raw PUSH order confirms it's simply isblock(sp, hoff, voff)
 * using this function's own parameters unchanged.
 */
int testx(int dir, SPRITE_STRUCT far * sp, int hoff, int voff)
{
    int result;

    sp->spritex += dir;
    result = isblock(sp, hoff, voff);
    sp->spritex -= dir;

    if (result == 0) {
        return 1;
    }
    if (result == -1) {
        return 1;
    }
    return 0;
}

/*
 * Vertical counterpart to testx() above, with extra handling for the
 * spike hazard (sets the one-shot _spiked flag when moving up (dir==-1)
 * into a spike cell) and one-way platforms (attribute 0x36 blocks
 * upward movement, 0x37 blocks downward movement). Reconstructed from
 * the raw disassembly (231e:26d4) for the same CONCAT22-corruption
 * reasons as testx().
 */
int testy(int dir, SPRITE_STRUCT far * sp, int hoff, int voff)
{
    int result;

    if (sp->spritey & 7) {
        return 1;
    }
    sp->spritey += dir;
    result = isblock(sp, hoff, voff);
    sp->spritey -= dir;

    if (result == 0) {
        return 1;
    }
    if (result == -1) {
        return 1;
    }
    if (dir == -1 && result == _spike) {
        _spiked = 1;
    }
    if (dir == -1 && result == 0x36) {
        return 1;
    }
    if (dir == 1 && result == 0x37) {
        return 1;
    }
    return 0;
}

/*
 * Respawns a gold pickup once its timer (maze_log->gold.time) has
 * elapsed: if no gold is currently present and (single-player, or more
 * than one station is connected), picks a new random valid position and
 * a random score value (5-20 via gauss()); otherwise just marks gold
 * absent and reschedules a shorter retry. Field accesses use the real
 * GOLD_PACKET bitfields (PACWARS.H) in place of the decompile's manual
 * bit-mask/shift sequences on a synthetic "wPacked_bits" word -- the
 * struct's bitfield layout was already confirmed to match the compiled
 * layout when GOLD_PACKET was first defined for this project.
 *
 * NOTE: the expiry check is compiled as a 32-bit subtract-and-test on
 * the raw `long time` words; reconstructed here as the equivalent plain
 * `gold.time <= maze_log->time` comparison, which Borland C compiles to
 * the same long-compare sequence.
 */
void init_gold(MAZE_LOG_STRUCT far * maze_log)
{
    int num_connected;
    int i;
    int score;
    int th, tv, tx, ty;

    num_connected = 0;
    for (i = 0; i <= 4; i++) {
        if (maze_log->connection[i] > 0) {
            num_connected++;
        }
    }

    if (maze_log->gold.time <= maze_log->time) {
        if (maze_log->gold.present == 0 && (_comms == 0 || num_connected > 1)) {
            init_position(&th, &tv, &tx, &ty);
            maze_log->gold.hoffset = th & 0xf;
            maze_log->gold.voffset = tv & 0xf;
            maze_log->gold.col = tx >> 3;
            maze_log->gold.row = ty >> 3;
            maze_log->gold.present = 1;
            maze_log->hit_man = -1;
            maze_log->gold.man = 0;
            score = gauss(0xf);
            if (score < 0) {
                score = -score;
            }
            maze_log->gold.score = (char) score + 5;
            maze_log->gold.time = maze_log->time + (rand() % 20) + 10;
        } else {
            maze_log->gold.present = 0;
            maze_log->gold.time = maze_log->time + (rand() % 30) + 5;
        }
    }
}

/*
 * Respawns a token pickup, same overall shape as init_gold() above but
 * with more outcomes: a freshly (re)spawned token additionally rolls a
 * type (0-9 normal, or a rarer 1-in-10 chance of a "big" type 0xc/0x10
 * with an extra +20 respawn delay); if a token is already present but
 * has "expired" without being fully consumed (type<12, i.e. not one of
 * the special big types, or its count field is still nonzero), it's
 * cleared and rescheduled for a short retry; otherwise (an unconsumed
 * "big" token whose count already reached 0) it's downgraded to a
 * plain type-10 token with a minimal 2-tick reschedule.
 */
void init_token(MAZE_LOG_STRUCT far * maze_log)
{
    int num_connected;
    int i;
    int r;
    int th, tv, tx, ty;

    num_connected = 0;
    for (i = 0; i <= 4; i++) {
        if (maze_log->connection[i] > 0) {
            num_connected++;
        }
    }

    if (maze_log->token.time <= maze_log->time) {
        if (maze_log->token.present == 0 && (_comms == 0 || num_connected > 1)) {
            init_position(&th, &tv, &tx, &ty);
            maze_log->token.count = 0;
            maze_log->token.hoffset = th & 0xf;
            maze_log->token.voffset = tv & 0xf;
            maze_log->token.col = (char) (tx >> 3);
            maze_log->token.row = (char) (ty >> 3);
            maze_log->token.present = 1;
            maze_log->token.man = 0;
            maze_log->token.warp_factor = 7;
            maze_log->token.warp_dir = -1;
            maze_log->token.time = maze_log->time + (rand() % 20) + 10;

            r = rand();
            if (r % 10 == 0) {
                r = rand();
                maze_log->token.type = (r % 3 == 0) ? 0x10 : 0xc;
                maze_log->token.time += 20;
            } else {
                r = rand();
                maze_log->token.type = (char) (r % 10);
            }
        } else if (maze_log->token.type < 12 || maze_log->token.count != 0) {
            maze_log->token.present = 0;
            maze_log->token.time = maze_log->time + (rand() % 5) + 5;
        } else {
            maze_log->token.type = 10;
            maze_log->token.time = maze_log->time + 2;
        }
    }
}

/*
 * Initializes a newly-connected player's slot (indexed by this
 * machine's own station index, _wstation): records the station number,
 * resets score/lives, and primes the gold/token respawn timers with a
 * short random initial delay.
 */
void init_man(int station_num, MAZE_LOG_STRUCT far * maze_log)
{
    maze_log->connection[_wstation] = (char) station_num;
    maze_log->score[_wstation] = 10;
    maze_log->men[_wstation] = 5;
    maze_log->hit_man = -1;
    maze_log->gold.time = maze_log->time + (rand() % 20) + 5;
    maze_log->token.time = maze_log->time + (rand() % 20) + 5;
}

/*
 * Picks a random room and a random 16x16-pixel (2x2 tile) position
 * within it whose 4 corner tiles are all either empty (maze cell 0) or
 * walkable (attribute 1 or 2), retrying up to 200 times or until ESC is
 * pressed. On success, writes the room/pixel coordinates out through
 * the 4 far pointers; on failure/abort, leaves them untouched.
 */
void init_position(int far * hoff, int far * voff, int far * x, int far * y)
{
    int i;
    int room_hoff, room_voff;
    MAZE_STRUCT far * maze_ptr;
    MAZE_STRUCT far * attrib_ptr;
    int row, col;

    i = 0;
    room_hoff = rand() % _HSIZE;
    room_voff = rand() % _VSIZE;
    maze_ptr = (MAZE_STRUCT far *) maze_def(room_hoff, room_voff);
    attrib_ptr = (MAZE_STRUCT far *) attrib_maze_def(room_hoff, room_voff);

    do {
        row = (rand() % 0x14) + 1;
        col = (rand() % 0x19) + 1;

        if ((maze_ptr->def[row][col] == 0 || attrib_ptr->def[row][col] == 1 || attrib_ptr->def[row][col] == 2) &&
            (maze_ptr->def[row][col + 1] == 0 || attrib_ptr->def[row][col + 1] == 1 || attrib_ptr->def[row][col + 1] == 2) &&
            (maze_ptr->def[row + 1][col] == 0 || attrib_ptr->def[row + 1][col] == 1 || attrib_ptr->def[row + 1][col] == 2) &&
            (maze_ptr->def[row + 1][col + 1] == 0 || attrib_ptr->def[row + 1][col + 1] == 1 || attrib_ptr->def[row + 1][col + 1] == 2)) {
            *hoff = room_hoff;
            *voff = room_voff;
            *x = col * 8;
            *y = row * 8;
            return;
        }
        if (i > 199) {
            return;
        }
        i++;
    } while (_esc == 0);
}

/*
 * Composites sprite s2's bitmap onto sprite s1's bitmap buffer wherever
 * their on-screen bounding boxes overlap (attrib==1 selects "under" --
 * only where s1 is currently transparent -- vs "over" -- unconditional).
 * Reconstructed from the raw disassembly: the decompile's CONCAT22
 * artifacts obscured the two SPRITE_STRUCT far-pointer parameters, but
 * the clipping arithmetic itself decompiled cleanly and is preserved
 * as-is. do_under_asm()/do_over_asm() signatures were independently
 * confirmed by disassembling do_under_asm's own prologue (see the
 * forward declarations above).
 */
void overlap_sprite(SPRITE_STRUCT far * s1, SPRITE_STRUCT far * s2, int attrib)
{
    unsigned char far * p1;
    unsigned char far * p2;
    int w, h;

    p1 = s1->sprite;
    p2 = s2->sprite;

    if (s2->spritex < s1->spritex) {
        p2 += s1->spritex - s2->spritex;
        w = s2->spritew - (s1->spritex - s2->spritex);
    } else {
        p1 += s2->spritex - s1->spritex;
        w = s2->spritew;
    }
    if (s1->spritex + s1->spritew < s2->spritex + s2->spritew) {
        w -= (s2->spritex + s2->spritew) - (s1->spritex + s1->spritew);
    }

    if (s2->spritey < s1->spritey) {
        p2 += (s1->spritey - s2->spritey) * s2->spritew;
        h = s2->spriteh - (s1->spritey - s2->spritey);
    } else {
        p1 += (s2->spritey - s1->spritey) * s1->spritew;
        h = s2->spriteh;
    }
    if (s1->spritey + s1->spriteh < s2->spritey + s2->spriteh) {
        h -= (s2->spritey + s2->spriteh) - (s1->spritey + s1->spriteh);
    }

    if (attrib == 1) {
        do_under_asm(p1, p2, h, w, s1->spritew - w, s2->spritew - w);
    } else {
        do_over_asm(p1, p2, h, w, s1->spritew - w, s2->spritew - w);
    }
}

/*
 * Was generated as a placeholder unk_func_2E86 (see MAZESPT.H's earlier
 * gap note): its module's type-def list didn't record this function at
 * all. Resolved via the Ghidra project while decompiling _main(), which
 * calls this: confirmed exported as `_setup_ir_sprite_buffer` at
 * 131E:2E86 (dump) / 231e:2e86 (Ghidra), returns void, no parameters.
 */
/*
 * Fills 16 "IR noise" pixel buffers by copying from the two IR source
 * sprites (_sprites[2]/_sprites[3], PACIR.SPT/PACIR2.SPT, alternating
 * even/odd index), with each byte independently replaced with 0 (25%
 * chance, via rand()%4==0) to produce a staticky/interference look.
 *
 * NOTE: the copy loop always runs a fixed 256 (0x100) iterations
 * regardless of the actual allocated size (src spritew*spriteh, which
 * may be smaller or larger than 256) -- preserved exactly as compiled.
 * The destination buffer table (DAT_340e_702d in the decompile) is NOT
 * file-local: create_ir_sprite() (MAZE.C) also reads it (confirmed via a
 * fresh get_xrefs_to once that function was decompiled), so it's declared
 * here (matching its real owner, this file) but exported via MAZESPT.H
 * rather than kept static. SPRITE_STRUCT is reused purely for its
 * confirmed 11-byte stride, even though only the .sprite field is ever
 * populated.
 */
SPRITE_STRUCT _ir_sprite_buf[16];

void setup_ir_sprite_buffer(void)
{
    int i, j;
    int src;
    long size;
    int r;

    for (i = 0; i < 16; i++) {
        src = i % 2 + 2;
        size = (long) _sprites[src].spritew * _sprites[src].spriteh;
        _ir_sprite_buf[i].sprite = calloc(1, (size_t) size);
        if (_ir_sprite_buf[i].sprite != NULL) {
            for (j = 0; j < 0x100; j++) {
                r = rand();
                if (r % 4 == 0) {
                    _ir_sprite_buf[i].sprite[j] = 0;
                } else {
                    _ir_sprite_buf[i].sprite[j] = _sprites[src].sprite[j];
                }
            }
        }
    }
}

