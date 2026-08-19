/*
 * GRAPH256.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT) and Ghidra decompilation/disassembly of the real
 * function bodies.
 */
#include "GRAPH256.H"
#include "MAZEUTIL.H"
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys\stat.h>

/*
 * Forward declaration: fillpolygon() calls FillLine() (defined further
 * down this file) before its own definition point.
 */
static void FillLine(int x1, int y1, int x2, int y2, unsigned int far * data, int line, int colour);

/*
 * storage for the shared font globals declared extern in PACWARS.H --
 * see the comment there for why they live here.
 */
unsigned char far * _GraphFontBuffer;
int __mva_text_height;
int __mva_text_width;
unsigned char far * __text_table_addr[2];

/*
 * Tracks the scanline row last touched by FillLine() across calls for the
 * same polygon, so it can tell "still finishing this row's pair of edges"
 * from "started a new row" without an extra parameter. Confirmed via
 * Ghidra xref analysis to be referenced only from FillLine() below, so
 * file-scope here. No real name recoverable from PACWARS.TXT.
 */
static int fill_last_row;

void cls_256_screen(void)
{
    unsigned char far * rgen;
    unsigned int row;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, __disp_page << 15);
    for (row = 0; row < (unsigned int) _max_y; row++) {
        memset(rgen, 0, _max_x);
        rgen += _sc_width;
    }
}

void cls_row(int row)
{
    unsigned char far * rgen;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + row * 8 * _max_x);
    memset(rgen, 0, _max_x << 3);
}

/*
 * Standard axis-aligned bounding-box overlap test. The real disassembly
 * branches on (spritex1 vs spritex2) first and then repeats an identical
 * y-overlap check in each branch (the compiler didn't factor it out) --
 * simplified here to the equivalent non-duplicated form; verified the two
 * are logically identical by tracing every comparison/jump in the raw
 * disassembly.
 */
int collision_detect(int spritex1, int spritey1, int spritew1, int spriteh1, int spritex2, int spritey2, int spritew2, int spriteh2)
{
    if (spritex1 < spritex2 + spritew2 && spritex2 < spritex1 + spritew1 &&
        spritey1 < spritey2 + spriteh2 && spritey2 < spritey1 + spriteh1) {
        return 1;
    }
    return 0;
}

void trbox(int x, int y, int w, int h, int colour)
{
    hline(x, y, w, colour);
    hline(x, y + h - 1, w, colour);
    vline(x, y, h, colour);
    vline(x + w - 1, y, h, colour);
}

void trfbox(int x, int y, int w, int h, int colour)
{
    int i;

    for (i = 0; i < h; i++) {
        hline(x, y + i, w, colour);
    }
}

void hline(int x, int y, int length, int colour)
{
    unsigned char far * rgen;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);
    memset(rgen, colour, length);
}

void vline(int x, int y, int length, int colour)
{
    unsigned char far * rgen;
    int i;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);
    for (i = 0; i < length; i++) {
        *rgen = (unsigned char) colour;
        rgen += 320;
    }
}

void set_pixel(int x, int y, int colour)
{
    unsigned char far * rgen;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);
    *rgen = (unsigned char) colour;
}

/*
 * BIOS INT 10h AX=1000h ("Set Individual Palette Register"): BL=register
 * number, BH=value.
 */
void set_palette(int reg_no, int reg_val)
{
    union REGS regs;

    regs.h.bl = (unsigned char) reg_no;
    regs.h.bh = (unsigned char) reg_val;
    regs.x.ax = 0x1000;
    int86(0x10, &regs, &regs);
}

/*
 * Blits a string using the fixed-width bitmap font loaded by load_font().
 * Font data is one glyph per character code, __mva_text_height rows of
 * __mva_text_width bits (MSB-first) each, packed one byte per row.
 */
void text256(int x, int y, unsigned char far * text_ptr, int fore, int back)
{
    unsigned char far * out_ptr;
    unsigned char far * glyph;
    unsigned int row, col;
    unsigned char mask;

    out_ptr = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * 320 + x);

    while (*text_ptr != 0) {
        glyph = __text_table_addr[0] + (unsigned int) *text_ptr * __mva_text_height;

        for (row = 0; row < (unsigned int) __mva_text_height; row++) {
            mask = 0x80;
            for (col = 0; col < (unsigned int) __mva_text_width; col++) {
                out_ptr[row * 320 + col] = (glyph[row] & mask) ? (unsigned char) fore : (unsigned char) back;
                mask >>= 1;
            }
        }

        out_ptr += __mva_text_width;
        text_ptr++;
    }
}

/*
 * Loads FONT.DAT (a fixed 0x800-byte bitmap) into GraphFontBuffer,
 * allocating the buffer on first use. Open flags/mode decoded from the raw
 * immediates in the disassembly (-0x7fff / 0x180 = O_BINARY|O_RDONLY /
 * S_IREAD|S_IWRITE), same pattern as init_array()/open_hiscore() elsewhere
 * in this project.
 */
int load_font(void)
{
    char file_name[80];
    int handle;

    if (_GraphFontBuffer == NULL) {
        _GraphFontBuffer = (unsigned char far *) calloc(1, 0x800);
        if (_GraphFontBuffer == NULL) {
            printf("Not enough Memory");
            return -1;
        }
    }

    strcpy(file_name, "FONT.DAT");
    get_filename(file_name);

    handle = open(file_name, O_BINARY | O_RDONLY, S_IREAD | S_IWRITE);
    if (handle >= 0) {
        if (read(handle, _GraphFontBuffer, 0x800) == 0x800) {
            close(handle);
            return 1;
        }
    }

    return -1;
}

void polyline(int num_points, POINT far * points, int colour)
{
    int i;

    if (num_points > 0) {
        for (i = 0; i < num_points - 1; i++) {
            drawline(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, colour);
        }
    }
}

void polygon(int num_points, POINT far * points, int colour)
{
    int i;

    if (num_points > 0) {
        for (i = 0; i < num_points - 1; i++) {
            drawline(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, colour);
        }
        drawline(points[i].x, points[i].y, points[0].x, points[0].y, colour);
    }
}

/*
 * Scanline polygon fill: for each edge, FillLine() records/pairs up
 * crossing X positions per row in fill_row_x (one slot per possible
 * scanline, 0..479) and draws the horizontal spans via hline(); the
 * outline is then redrawn on top via polygon().
 */
void fillpolygon(int num_points, POINT far * points, int colour)
{
    unsigned int fill_row_x[480];
    int pmin, pmax;
    int i;

    pmin = 480;
    pmax = 0;
    for (i = 0; i < num_points; i++) {
        if (points[i].y < pmin) {
            pmin = points[i].y;
        }
        if (points[i].y > pmax) {
            pmax = points[i].y;
        }
    }

    memset(fill_row_x + pmin, 0xFF, (pmax + 1 - pmin) * 2);

    if (num_points > 0) {
        for (i = 0; i < num_points - 1; i++) {
            FillLine(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, fill_row_x, i, colour);
        }
        FillLine(points[i].x, points[i].y, points[0].x, points[0].y, fill_row_x, i, colour);
    }

    polygon(num_points, points, colour);
}

/*
 * Walks one polygon edge from (x1,y1) to (x2,y2) via Bresenham's
 * algorithm; for each scanline row it crosses, either records the X
 * position in data[row] (first edge to cross this row) or, if a position
 * is already recorded, draws the horizontal span between the two via
 * hline() and clears the slot back to the 0xFFFF "empty" sentinel.
 * `line` == 0 resets fill_last_row at the start of each new polygon.
 */
static void FillLine(int x1, int y1, int x2, int y2, unsigned int far * data, int line, int colour)
{
    int dx, dy, xstep, ystep;
    int major, minor, err;
    int i;
    unsigned int x;
    int length;

    dx = ((x2 < x1) ? (x1 - x2) : (x2 - x1)) + 1;
    dy = ((y2 < y1) ? (y1 - y2) : (y2 - y1)) + 1;
    xstep = (x2 < x1) ? -1 : 1;
    ystep = (y2 < y1) ? -1 : 1;

    if (line == 0) {
        fill_last_row = -1;
    }

    if (y1 == y2) {
        data[y1] = 0xFFFF;
        return;
    }

    minor = dy;
    major = dx;
    if (dx < dy) {
        minor = dx;
        major = dy;
    }
    err = (minor - major) * 2;

    for (i = 0; i < major; i++) {
        if (fill_last_row != y1 || data[y1] == 0xFFFF) {
            if (data[y1] == 0xFFFF) {
                data[y1] = x1;
            } else {
                if (data[y1] < (unsigned int) x1) {
                    length = (x1 + 1) - data[y1];
                    x = data[y1];
                } else {
                    length = (data[y1] + 1) - x1;
                    x = x1;
                }
                hline(x, y1, length, colour);
                data[y1] = 0xFFFF;
            }
            fill_last_row = y1;
        }

        if (dx < dy) {
            for (; err >= 0; err -= major * 2) {
                x1 += xstep;
            }
            y1 += ystep;
        } else {
            for (; err >= 0; err -= major * 2) {
                y1 += ystep;
            }
            x1 += xstep;
        }
        err += minor * 2;
    }
}

/*
 * Bresenham line draw, writing colour bytes directly into VGA memory at
 * A000:(disp_page offset + y*320 + x).
 */
void drawline(int x1, int y1, int x2, int y2, int colour)
{
    unsigned char far * base;
    int dx, dy, xstep, ystep;
    int major, minor, err;
    int row;
    int i;

    dx = ((x2 < x1) ? (x1 - x2) : (x2 - x1)) + 1;
    dy = ((y2 < y1) ? (y1 - y2) : (y2 - y1)) + 1;
    xstep = (x2 < x1) ? -1 : 1;
    ystep = (y2 < y1) ? -1 : 1;

    base = (unsigned char far *) MK_FP(VGA_SEGMENT, __disp_page << 15);

    minor = dy;
    major = dx;
    if (dx < dy) {
        minor = dx;
        major = dy;
    }
    err = (minor - major) * 2;
    row = y1 * 320;

    if (dx < dy) {
        for (i = 0; i < major; i++) {
            base[x1 + row] = (unsigned char) colour;
            for (; err >= 0; err -= major * 2) {
                x1 += xstep;
            }
            row += ystep * 320;
            err += minor * 2;
        }
    } else {
        for (i = 0; i < major; i++) {
            base[x1 + row] = (unsigned char) colour;
            for (; err >= 0; err -= major * 2) {
                row += ystep * 320;
            }
            x1 += xstep;
            err += minor * 2;
        }
    }
}
