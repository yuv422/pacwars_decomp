/*
 * MVAGRAPH.C
 *
 * Reconstructed from PACWARS.EXE via Ghidra decompilation/disassembly of
 * the real function bodies (cross-checked against the raw disassembly
 * throughout -- Ghidra's decompiler is unreliable for this module's
 * EGA/VGA port I/O and computed jump tables).
 *
 * Unlike GRAPH256.C/DISPPIC.C's linear byte-per-pixel VGA mode 0x13, every
 * drawing primitive here targets EGA/VGA *planar* mode: 8 pixels share
 * each byte/plane (x>>3 = byte offset, x&7 = bit position), spread across
 * 4 hardware bit-planes selected via the Sequencer (ports 0x3C4/0x3C5) and
 * Graphics Controller (ports 0x3CE/0x3CF) registers, with the byte-per-
 * scanline stride read out of the BIOS Data Area (MVA_ROW_STRIDE, in
 * PACWARS.H) rather than tracked as a project variable.
 *
 * The Arc/Ellipse/Segment/Quad cluster (Ellipse, EllipsePoint, EllipseFill,
 * FillSegment, Arc, ExplodeCenter, SegmentLine, SegmentFill, ArcXYPos,
 * ArcQuad, FillQuad, ArcPoint) is confirmed dead code -- zero callers
 * anywhere in PACWARS.EXE (leftover from the "MVA" business-graphics
 * library this engine was built on top of) -- and is left stubbed out
 * per an explicit decision not to spend further effort on it.
 */
#include "MVAGRAPH.H"
#include "MAZEUTIL.H"
#include "UTILS.H"
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys\stat.h>

/*
 * Forward declarations: FillPolygon() calls FillLine() and Circle() calls
 * CirclePoint()/CircleFill(), all defined further down this file.
 */
static void FillLine(int x1, int y1, int x2, int y2, unsigned int far * data, int line);
void CirclePoint(int cx, int cy, int px, int py, int colour);
void CircleFill(int cx, int cy, int px, int py);

/*
 * storage for the __mva_graph_text/__mva_text_table globals declared
 * extern in PACWARS.H -- see the comment there.
 */
int __mva_graph_text;
int __mva_text_table;

/*
 * BIOS INT 10h AX=1130h ("Get Font Information"): BH selects which font
 * pointer to return (2 = ROM 8x14 font, 3/4 = ROM 8x8 font low/high half,
 * 6 = ROM 8x16 font -- the table numbers SetTextFont() passes below).
 * Returns the font pointer in ES:BP; int86()/int86x() can't retrieve BP
 * (it isn't part of union REGS), so this needs raw inline assembly.
 * Borland's calling convention returns a far pointer in DX:AX
 * (segment:offset), which is exactly what's left in those registers when
 * the asm block falls through into the epilogue -- no explicit `return`
 * needed.
 */
static unsigned char far * get_bios_font(int table)
{
    asm {
        mov ax, 0x1130
        mov bh, byte ptr table
        int 0x10
        mov dx, es
        mov ax, bp
    }
}

int SetGraphText(int status)
{
    if (status == 0 || status == 1) {
        __mva_graph_text = status;
    }
    return __mva_graph_text;
}

/*
 * Selects one of the fixed-size bitmap fonts used by the graphics-mode
 * text primitives (GraphText/BitGraphText/ByteGraphText and friends),
 * populating __mva_text_height/__mva_text_width/__text_table_addr[] and
 * the character-grid size (__mvarows/__mvacols) to match. Ghidra's
 * decompiler fails outright on this function (a computed jump table at
 * 284b:015b it can't resolve, producing pseudocode built from unrelated
 * functions in a totally different module) -- reconstructed here from the
 * raw disassembly of the prologue, the jump table bytes, and each case
 * body instead.
 *
 * text_table == 3 and any value outside 0-4 are both genuine no-ops in
 * the original binary (the jump table sends them straight past every
 * assignment to the final "return the current table index" instruction)
 * -- not oversights here.
 */
int SetTextFont(int text_table)
{
    VAR_STRUCT far * var_ptr;
    int rows;

    var_ptr = get_mvavar();

    switch (text_table) {
    case 0:
        __mva_text_height = 16;
        __mva_text_width = 8;
        __text_table_addr[0] = get_bios_font(6);
        __mvacols = 80;
        rows = (var_ptr->screen_size == 4) ? 21 : 30;
        break;

    case 1:
        __mva_text_height = 14;
        __mva_text_width = 8;
        __text_table_addr[0] = get_bios_font(2);
        __mvacols = 80;
        rows = (var_ptr->screen_size == 4) ? 25 : 34;
        break;

    case 2:
        __mva_text_height = 8;
        __mva_text_width = 8;
        __text_table_addr[0] = get_bios_font(3);
        __text_table_addr[1] = get_bios_font(4);
        __mvacols = 80;
        rows = (var_ptr->screen_size == 4) ? 43 : 60;
        break;

    case 4:
        /* the custom font loaded by LoadGraphFont(), rather than a BIOS
           ROM font */
        __mva_text_height = 8;
        __mva_text_width = 5;
        __text_table_addr[0] = _GraphFontBuffer;
        __mvacols = 128;
        rows = (var_ptr->screen_size == 4) ? 43 : 60;
        break;

    default:
        return __mva_text_table;
    }

    __mvarows = rows;
    __mva_text_table = text_table;
    __mva_graph_text = 1;
    return __mva_text_table;
}

/*
 * Loads/saves the custom 8x5 font (FONT.DAT, a fixed 0x800-byte bitmap)
 * used by SetTextFont()'s text_table==4, allocating _GraphFontBuffer on
 * first use. Mirrors GRAPH256.C's load_font() (which loads the same file
 * for its own, unrelated font table) but keeps its own buffer.
 */
int LoadGraphFont(void)
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
    mk_filename(1, 0, file_name);

    handle = open(file_name, O_BINARY | O_RDONLY, S_IREAD | S_IWRITE);
    if (handle >= 0) {
        if (read(handle, _GraphFontBuffer, 0x800) == 0x800) {
            close(handle);
            return 1;
        }
    }

    return -1;
}

int SaveGraphFont(void)
{
    char file_name[80];
    int handle;

    strcpy(file_name, "FONT.DAT");
    mk_filename(1, 0, file_name);

    handle = open(file_name, O_BINARY | O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
    if (handle >= 0) {
        if (write(handle, _GraphFontBuffer, 0x800) == 0x800) {
            close(handle);
            return 1;
        }
    }

    return -1;
}

/*
 * Classic EGA/VGA single-pixel-set trick: point the Bit Mask register
 * (GC8) at the target bit and write 0 to clear it in every plane, then
 * point the Map Mask register (Seq2) at the colour value and write 0xFF
 * to set the colour's bits in exactly the selected planes.
 */
void SetPixel(int x, int y, int colour)
{
    unsigned char far * rgen;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * MVA_ROW_STRIDE + (x >> 3));

    outportb(0x3ce, 8);
    outportb(0x3cf, 0x80 >> (x & 7));
    *rgen = 0;

    outportb(0x3c4, 2);
    outportb(0x3c5, (unsigned char) colour);
    *rgen = 0xff;

    outportb(0x3c4, 2);
    outportb(0x3c5, 0xf);
    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);
}

/*
 * Reads a pixel back one plane at a time via the Read Map Select register
 * (GC4), reassembling the 4-bit colour value bit0=plane0..bit3=plane3.
 */
int ReadPixel(int x, int y)
{
    unsigned char far * rgen;
    unsigned int result;
    int plane;

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * MVA_ROW_STRIDE + (x >> 3));

    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);

    result = 0;
    for (plane = 3; plane >= 0; plane--) {
        outportb(0x3ce, 4);
        outportb(0x3cf, (unsigned char) plane);
        result <<= 1;
        if (*rgen & (0x80 >> (x & 7))) {
            result |= 1;
        }
    }

    outportb(0x3ce, 4);
    outportb(0x3cf, 0);
    return result;
}

/*
 * Bresenham line draw using the EGA/VGA Set/Reset trick: preload the
 * colour into every plane's Set/Reset register (GC0) and enable
 * Set/Reset on all 4 planes (GC1=0xF), so each per-pixel step below only
 * has to point the Bit Mask register (GC8) at the target bit and perform
 * any write at all -- the hardware substitutes the preloaded colour into
 * that bit on every enabled plane regardless of the byte written, which
 * is why the original code gets away with a bare increment rather than
 * writing an explicit value.
 */
static void DrawLine(int x1, int y1, int x2, int y2)
{
    unsigned char far * base;
    unsigned int row_stride;
    int dx, dy, xstep, ystep;
    int major, minor, err;
    long row;
    int i;
    unsigned char fg;

    row_stride = MVA_ROW_STRIDE;
    base = (unsigned char far *) MK_FP(VGA_SEGMENT, __disp_page << 15);
    fg = _char_attrib & 0xf;

    dx = ((x2 < x1) ? (x1 - x2) : (x2 - x1)) + 1;
    dy = ((y2 < y1) ? (y1 - y2) : (y2 - y1)) + 1;
    xstep = (x2 < x1) ? -1 : 1;
    ystep = (y2 < y1) ? -1 : 1;

    minor = dy;
    major = dx;
    if (dx < dy) {
        minor = dx;
        major = dy;
    }
    err = (minor - major) * 2;

    outportb(0x3ce, 0);
    outportb(0x3cf, fg);
    outportb(0x3ce, 1);
    outportb(0x3cf, 0xf);

    row = (long) y1 * row_stride;

    if (dx < dy) {
        for (i = 0; i < major; i++) {
            outportb(0x3ce, 8);
            outportb(0x3cf, 0x80 >> (x1 & 7));
            base[(x1 >> 3) + row] += 1;
            for (; err >= 0; err -= major * 2) {
                x1 += xstep;
            }
            row += (long) ystep * row_stride;
            err += minor * 2;
        }
    } else {
        for (i = 0; i < major; i++) {
            outportb(0x3ce, 8);
            outportb(0x3cf, 0x80 >> (x1 & 7));
            base[(x1 >> 3) + row] += 1;
            for (; err >= 0; err -= major * 2) {
                row += (long) ystep * row_stride;
            }
            x1 += xstep;
            err += minor * 2;
        }
    }

    outportb(0x3ce, 1);
    outportb(0x3cf, 0);
    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);
}

/*
 * Horizontal run fill, Write Mode 2 (GC5=2): the CPU data's low 4 bits
 * select the colour per plane directly, combined with the Bit Mask
 * register, so no Set/Reset preload is needed -- the left/right partial
 * bytes get their own Bit Mask (clipped to the run's start/end bit), and
 * any whole bytes in between are filled with memset() under a full 0xFF
 * mask.
 */
void DrawHLine(int x1, int y1, int width)
{
    unsigned char far * rgen;
    unsigned char lmask, rmask;
    unsigned char fg;
    int x2;
    int whole_bytes;
    int total_bytes;

    x2 = x1 + width - 1;
    whole_bytes = (x2 >> 3) - (x1 >> 3);
    total_bytes = whole_bytes + 1;

    lmask = 0xff >> (x1 & 7);
    rmask = 0xff00 >> ((x2 & 7) + 1);
    fg = _char_attrib & 0xf;

    if (total_bytes == 1) {
        lmask &= rmask;
        rmask = 0;
    }

    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y1 * MVA_ROW_STRIDE + (x1 >> 3));

    outportb(0x3ce, 5);
    outportb(0x3cf, 2);

    outportb(0x3ce, 8);
    outportb(0x3cf, lmask);
    *rgen = fg;
    rgen++;

    if (total_bytes > 2) {
        outportb(0x3ce, 8);
        outportb(0x3cf, 0xff);
        memset(rgen, fg, whole_bytes - 1);
        rgen += whole_bytes - 1;
    }

    if (total_bytes > 1) {
        outportb(0x3ce, 8);
        outportb(0x3cf, rmask);
        *rgen = fg;
    }

    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);
    outportb(0x3ce, 5);
    outportb(0x3cf, 0);
}

void DrawVLine(int x1, int y1, int height)
{
    unsigned char far * rgen;
    unsigned char fg;
    unsigned int row_stride;
    int i;

    if (height > 0) {
        fg = _char_attrib & 0xf;
        row_stride = MVA_ROW_STRIDE;
        rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y1 * row_stride + (x1 >> 3));

        outportb(0x3ce, 5);
        outportb(0x3cf, 2);
        outportb(0x3ce, 8);
        outportb(0x3cf, 0x80 >> (x1 & 7));

        for (i = 0; i < height; i++) {
            *rgen = fg;
            rgen += row_stride;
        }

        outportb(0x3ce, 8);
        outportb(0x3cf, 0xff);
        outportb(0x3ce, 5);
        outportb(0x3cf, 0);
    }
}

/*
 * Tracks the scanline row last touched by FillLine() across calls for the
 * same polygon (see the identical fill_last_row in GRAPH256.C). Confirmed
 * via Ghidra xref analysis to be referenced only from FillLine() below.
 */
static int mva_fill_last_row;

static void PolyLine(int num_points, POINT far * points)
{
    int i;

    if (num_points > 0) {
        for (i = 0; i < num_points - 1; i++) {
            DrawLine(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);
        }
    }
}

static void Polygon(int num_points, POINT far * points)
{
    int i;

    if (num_points > 0) {
        for (i = 0; i < num_points - 1; i++) {
            DrawLine(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);
        }
        DrawLine(points[i].x, points[i].y, points[0].x, points[0].y);
    }
}

/*
 * Scanline polygon fill -- identical structure to GRAPH256.C's
 * fillpolygon(), just against FillLine()/DrawHLine()'s planar-mode
 * implementations and the current _char_attrib colour instead of an
 * explicit colour parameter.
 */
static void FillPolygon(int num_points, POINT far * points)
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
            FillLine(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, fill_row_x, i);
        }
        FillLine(points[i].x, points[i].y, points[0].x, points[0].y, fill_row_x, i);
    }

    Polygon(num_points, points);
}

/*
 * Walks one polygon edge via Bresenham's algorithm; for each scanline row
 * it crosses, either records the X position in data[row] (first edge to
 * cross this row) or, if a position is already recorded, draws the
 * horizontal span between the two via DrawHLine() and clears the slot
 * back to the 0xFFFF "empty" sentinel. `line` == 0 resets
 * mva_fill_last_row at the start of each new polygon.
 */
static void FillLine(int x1, int y1, int x2, int y2, unsigned int far * data, int line)
{
    int dx, dy, xstep, ystep;
    int major, minor, err;
    int i;
    unsigned int x;
    int width;

    dx = ((x2 < x1) ? (x1 - x2) : (x2 - x1)) + 1;
    dy = ((y2 < y1) ? (y1 - y2) : (y2 - y1)) + 1;
    xstep = (x2 < x1) ? -1 : 1;
    ystep = (y2 < y1) ? -1 : 1;

    if (line == 0) {
        mva_fill_last_row = -1;
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
        if (mva_fill_last_row != y1 || data[y1] == 0xFFFF) {
            if (data[y1] == 0xFFFF) {
                data[y1] = x1;
            } else {
                if (data[y1] < (unsigned int) x1) {
                    width = (x1 + 1) - data[y1];
                    x = data[y1];
                } else {
                    width = (data[y1] + 1) - x1;
                    x = x1;
                }
                DrawHLine(x, y1, width);
                data[y1] = 0xFFFF;
            }
            mva_fill_last_row = y1;
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

/* Midpoint circle algorithm, plotting (or filling) all 8 symmetric points
   per step via CirclePoint()/CircleFill(). */
void Circle(int x, int y, int r, int fill)
{
    int px, py;
    int d;
    int delta;

    px = r;
    py = 0;
    d = r * -2 + 3;

    do {
        if (fill == 1) {
            CircleFill(x, y, px, py);
        } else {
            CirclePoint(x, y, px, py, _char_attrib & 0xf);
        }

        if (d < 0) {
            delta = py * 4 + 6;
        } else {
            delta = (py - px) * 4 + 10;
            px--;
        }
        d += delta;
        py++;
    } while (py <= px);
}

void CirclePoint(int cx, int cy, int px, int py, int colour)
{
    SetPixel(cx + px, cy + py, colour);
    SetPixel(cx + py, cy + px, colour);
    SetPixel(cx - px, cy + py, colour);
    SetPixel(cx - py, cy + px, colour);
    SetPixel(cx + px, cy - py, colour);
    SetPixel(cx + py, cy - px, colour);
    SetPixel(cx - px, cy - py, colour);
    SetPixel(cx - py, cy - px, colour);
}

void CircleFill(int cx, int cy, int px, int py)
{
    DrawHLine(cx - px, cy + py, px << 1);
    DrawHLine(cx - px, cy - py, px << 1);
    DrawHLine(cx - py, cy + px, py << 1);
    DrawHLine(cx - py, cy - px, py << 1);
}

void Ellipse(int cx, int cy, int ra, int rb, int fill)
{
}

void EllipsePoint(int cx, int cy, int px, int py, int colour)
{
}

void EllipseFill(int cx, int cy, int px, int py)
{
}

void FillSegment(int cx, int cy, int ra, int rb, int sa, int ea, int h, int border, int explode)
{
}

void Arc(int cx, int cy, int ra, int rb, int sa, int ea, int fill, int seg, int explode, int h, int border)
{
}

void ExplodeCenter(int far * cx, int far * cy, int ra, int rb, int sa, int ea)
{
}

void SegmentLine(int cx, int cy, int px, int py, int q)
{
}

void SegmentFill(POINT far * lp, int far * q, int small)
{
}

void ArcXYPos(int far * x, int far * y, int ra, int rb, int angle, int q)
{
}

void ArcQuad(int cx, int cy, int ra, int rb, int q, unsigned int type, int sx, int sy, int ex, int ey)
{
}

void FillQuad(int cx, int cy, int q, int colour, int h, int border, unsigned int type)
{
}

void ArcPoint(int cx, int cy, int px, int py, int colour, int q)
{
}

void GraphBox(int x, int y, int w, int h)
{
    DrawHLine(x, y, w);
    DrawHLine(x, y + h - 1, w);
    DrawVLine(x, y, h);
    DrawVLine(x + w - 1, y, h);
}

/*
 * Write Mode 2 (GC5=2) filled box, background colour (_char_attrib>>4) --
 * same left/right partial-byte-plus-memset()'d-middle structure as
 * DrawHLine(), just repeated per row.
 */
void FillBox(int x1, int y1, int width, int height)
{
    unsigned char far * rgen;
    unsigned char lmask, rmask;
    unsigned char bg;
    int x2;
    int whole_bytes;
    int total_bytes;
    unsigned int row_stride;
    unsigned int row_skip;
    int row;

    x2 = x1 + width - 1;
    whole_bytes = (x2 >> 3) - (x1 >> 3);
    total_bytes = whole_bytes + 1;

    lmask = 0xff >> (x1 & 7);
    rmask = 0xff00 >> ((x2 & 7) + 1);

    if (height > 0) {
        bg = _char_attrib >> 4;
        if (total_bytes == 1) {
            lmask &= rmask;
            rmask = 0;
        }

        row_stride = MVA_ROW_STRIDE;
        rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y1 * row_stride + (x1 >> 3));
        row_skip = row_stride - total_bytes;

        outportb(0x3ce, 5);
        outportb(0x3cf, 2);

        for (row = 0; row < height; row++) {
            outportb(0x3ce, 8);
            outportb(0x3cf, lmask);
            *rgen = bg;
            rgen++;

            if (total_bytes > 2) {
                outportb(0x3ce, 8);
                outportb(0x3cf, 0xff);
                memset(rgen, bg, whole_bytes - 1);
                rgen += whole_bytes - 1;
            }

            if (total_bytes > 1) {
                outportb(0x3ce, 8);
                outportb(0x3cf, rmask);
                *rgen = bg;
                rgen++;
            }

            rgen += row_skip;
        }

        outportb(0x3ce, 8);
        outportb(0x3cf, 0xff);
        outportb(0x3ce, 5);
        outportb(0x3cf, 0);
    }
}

void ClsGraphViewport(int y1, int h)
{
    unsigned char far * base;
    unsigned char bg;
    unsigned int row_stride;

    if (h > 0) {
        row_stride = MVA_ROW_STRIDE;
        bg = _char_attrib >> 4;
        base = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y1 * row_stride);

        outportb(0x3ce, 5);
        outportb(0x3cf, 2);
        outportb(0x3ce, 8);
        outportb(0x3cf, 0xff);
        memset(base, bg, row_stride * h);
        outportb(0x3ce, 5);
        outportb(0x3cf, 0);
    }
}

/*
 * Saves a w x h block of screen into sc_buffer, one plane at a time via
 * the Read Map Select register (GC4) -- sc_buffer ends up holding
 * height rows of 4 back-to-back byte_width-byte planes each (no padding),
 * top row first. byte_width covers any partial bits at the left/right
 * edge by rounding the affected byte range up, not by clipping.
 */
void GetBitBlock(int x, int y, int width, int height, unsigned char far * sc_buffer)
{
    unsigned char far * scr;
    unsigned int row_stride;
    unsigned int byte_width;
    int row;
    int plane;

    row_stride = MVA_ROW_STRIDE;
    byte_width = (width + 7) >> 3;
    if (x & 7) {
        byte_width++;
    }

    scr = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + y * row_stride + (x >> 3));

    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);

    for (row = 0; row < height; row++) {
        for (plane = 0; plane < 4; plane++) {
            outportb(0x3ce, 4);
            outportb(0x3cf, (unsigned char) plane);
            memcpy(sc_buffer, scr, byte_width);
            sc_buffer += byte_width;
        }
        scr += row_stride;
    }

    outportb(0x3ce, 4);
    outportb(0x3cf, 0);
}

/*
 * Inverse of GetBitBlock(): restores a saved block from sc_buffer back to
 * the screen via the Map Mask register (Seq2) selecting one plane at a
 * time. Both the screen pointer and the sc_buffer pointer are
 * pre-advanced to their LAST row and walked backward one row per
 * iteration -- functionally a top-to-bottom restore, just implemented
 * bottom-up in the original binary.
 */
void SetBitBlock(int x, int y, int width, int height, unsigned char far * sc_buffer)
{
    unsigned char far * scr;
    unsigned int row_stride;
    unsigned int byte_width;
    unsigned char map_mask;
    int row;

    row_stride = MVA_ROW_STRIDE;
    byte_width = (width + 7) >> 3;
    if (x & 7) {
        byte_width++;
    }

    scr = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + (y + height - 1) * row_stride + (x >> 3));
    sc_buffer += byte_width * (height - 1) * 4;

    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);

    for (row = 0; row < height; row++) {
        for (map_mask = 1; map_mask <= 8; map_mask <<= 1) {
            outportb(0x3c4, 2);
            outportb(0x3c5, map_mask);
            memcpy(scr, sc_buffer, byte_width);
            sc_buffer += byte_width;
        }
        scr -= row_stride;
        sc_buffer -= byte_width * 8;
    }

    outportb(0x3c4, 2);
    outportb(0x3c5, 0xf);
}

/*
 * Dispatches to the fast byte-aligned blitter when possible (x on a byte
 * boundary and the current font is a fixed 8 pixels wide), otherwise
 * falls back to the bit-shifting blitter that handles any x position.
 */
void GraphText(int x, int y, unsigned char far * text_ptr)
{
    if ((x & 7) == 0 && __mva_text_width == 8) {
        ByteGraphText(x, y, text_ptr);
    } else {
        BitGraphText(x, y, text_ptr);
    }
}

void GraphChar(int x, int y, unsigned char text_char, int repeat)
{
    if ((x & 7) == 0 && __mva_text_width == 8) {
        ByteGraphChar(x, y, text_char, repeat);
    } else {
        BitGraphChar(x, y, text_char, repeat);
    }
}

/*
 * Renders a string into a local scratch bitmap -- each glyph row is
 * bit-shifted into place so the string can start at any x position, not
 * just a byte boundary -- then blits the whole thing in one go via
 * GraphTextOut().
 */
void BitGraphText(int x, int y, unsigned char far * text_ptr)
{
    unsigned char out_buf[2064];
    unsigned char far * out_ptr;
    unsigned char far * glyph;
    unsigned char mask;
    unsigned int shift;
    unsigned int row;
    int xpos2;
    int stride;
    unsigned int len;

    mask = 0xff00 >> __mva_text_width;

    if (*text_ptr != '\0') {
        shift = x & 7;
        len = strlen((char far *) text_ptr);
        xpos2 = x + len * __mva_text_width - 1;
        stride = (xpos2 >> 3) - (x >> 3);
        out_ptr = out_buf;
        memset(out_ptr, 0, (stride + 2) * __mva_text_height);

        while (*text_ptr != '\0') {
            glyph = __text_table_addr[0] + (unsigned int) *text_ptr * __mva_text_height;
            for (row = 0; row < (unsigned int) __mva_text_height; row++) {
                out_ptr[(stride + 1) * row] |= (glyph[row] & mask) >> shift;
                out_ptr[(stride + 1) * row + 1] |= (glyph[row] & mask) << (8 - shift);
            }
            text_ptr++;
            shift += __mva_text_width;
            if (shift > 7) {
                out_ptr++;
                shift %= 8;
            }
        }

        GraphTextOut(x, y, xpos2, out_buf);
    }
}

/* BitGraphText()'s sibling for repeating a single character. */
void BitGraphChar(int x, int y, unsigned char text_char, int repeat)
{
    unsigned char out_buf[2064];
    unsigned char far * out_ptr;
    unsigned char far * glyph;
    unsigned char mask;
    unsigned int shift;
    unsigned int row;
    int xpos2;
    int stride;

    mask = 0xff00 >> __mva_text_width;

    if (repeat >= 0) {
        shift = x & 7;
        xpos2 = x + repeat * __mva_text_width - 1;
        stride = (xpos2 >> 3) - (x >> 3);
        out_ptr = out_buf;
        memset(out_ptr, 0, (stride + 2) * __mva_text_height);

        glyph = __text_table_addr[0] + (unsigned int) text_char * __mva_text_height;

        while (repeat > 0) {
            for (row = 0; row < (unsigned int) __mva_text_height; row++) {
                out_ptr[(stride + 1) * row] |= (glyph[row] & mask) >> shift;
                out_ptr[(stride + 1) * row + 1] |= (glyph[row] & mask) << (8 - shift);
            }
            shift += __mva_text_width;
            repeat--;
            if (shift > 7) {
                out_ptr++;
                shift %= 8;
            }
        }

        GraphTextOut(x, y, xpos2, out_buf);
    }
}

/*
 * Blits a pre-rendered glyph-row bitmap (row-major, total_bytes columns
 * per row -- as built by BitGraphText()/BitGraphChar()) to the screen,
 * one byte column at a time: each column gets a background pass (erase
 * to _char_attrib's background nibble across the whole column) followed
 * by a foreground pass (only the glyph's "on" bits, clipped by lmask/
 * rmask for the first/last column, which may be partial bytes).
 */
void GraphTextOut(int xpos1, int ypos, int xpos2, unsigned char far * text_buf)
{
    unsigned char far * rgen;
    unsigned char lmask, rmask;
    unsigned char bg, fg;
    int whole_bytes;
    int total_bytes;
    unsigned int row_stride;
    unsigned int row;
    int col;

    row_stride = MVA_ROW_STRIDE;
    whole_bytes = (xpos2 >> 3) - (xpos1 >> 3);
    total_bytes = whole_bytes + 1;

    lmask = 0xff >> (xpos1 & 7);
    rmask = 0xff00 >> ((xpos2 & 7) + 1);
    bg = _char_attrib >> 4;
    fg = _char_attrib & 0xf;

    if (total_bytes == 1) {
        lmask &= rmask;
        rmask = 0;
    }

    outportb(0x3ce, 5);
    outportb(0x3cf, 2);

    /* first (leftmost, possibly partial) byte column */
    rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + ypos * row_stride + (xpos1 >> 3));
    for (row = 0; row < (unsigned int) __mva_text_height; row++) {
        outportb(0x3ce, 8);
        outportb(0x3cf, lmask);
        *rgen = bg;
        outportb(0x3ce, 8);
        outportb(0x3cf, text_buf[row * total_bytes] & lmask);
        *rgen = fg;
        rgen += row_stride;
    }

    /* whole middle byte columns (neither the first nor the last) */
    for (col = 1; col < whole_bytes; col++) {
        rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + ypos * row_stride + (xpos1 >> 3) + col);
        for (row = 0; row < (unsigned int) __mva_text_height; row++) {
            outportb(0x3ce, 8);
            outportb(0x3cf, 0xff);
            *rgen = bg;
            outportb(0x3ce, 8);
            outportb(0x3cf, text_buf[row * total_bytes + col]);
            *rgen = fg;
            rgen += row_stride;
        }
    }

    /* last (rightmost, possibly partial) byte column */
    if (total_bytes > 1) {
        rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + ypos * row_stride + (xpos1 >> 3) + whole_bytes);
        for (row = 0; row < (unsigned int) __mva_text_height; row++) {
            outportb(0x3ce, 8);
            outportb(0x3cf, rmask);
            *rgen = bg;
            outportb(0x3ce, 8);
            outportb(0x3cf, text_buf[row * total_bytes + whole_bytes] & rmask);
            *rgen = fg;
            rgen += row_stride;
        }
    }

    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);
    outportb(0x3ce, 5);
    outportb(0x3cf, 0);
}

/*
 * Byte-aligned sibling of BitGraphText() -- only usable when x is a byte
 * boundary and the font is a fixed 8 pixels wide, but simpler/faster
 * since every glyph column lands on its own screen byte with no bit
 * shifting needed.
 */
void ByteGraphText(int x, int y, unsigned char far * text_ptr)
{
    unsigned char out_buf[2064];
    unsigned char far * out_ptr;
    unsigned char far * glyph;
    unsigned int len;
    unsigned int row;

    if (*text_ptr != '\0') {
        len = strlen((char far *) text_ptr);
        out_ptr = out_buf;
        memset(out_ptr, 0, len);

        while (*text_ptr != '\0') {
            glyph = __text_table_addr[0] + (unsigned int) *text_ptr * __mva_text_height;
            for (row = 0; row < (unsigned int) __mva_text_height; row++) {
                out_ptr[len * row] = glyph[row];
            }
            out_ptr++;
            text_ptr++;
        }

        ByteGraphTextOut(x, y, len, out_buf);
    }
}

/*
 * ByteGraphText()'s sibling for repeating a single character. When
 * repeating 5 or more times, every column of a given row holds the same
 * glyph byte, so a per-row memset() fills the whole run in one shot
 * instead of the byte-by-byte loop used for short runs.
 */
void ByteGraphChar(int x, int y, unsigned char text_char, int repeat)
{
    unsigned char out_buf[2064];
    unsigned char far * out_ptr;
    unsigned char far * glyph;
    unsigned int row;
    int num_bytes;

    num_bytes = repeat;

    if (repeat >= 0) {
        out_ptr = out_buf;
        memset(out_ptr, 0, repeat * __mva_text_height);

        glyph = __text_table_addr[0] + (unsigned int) text_char * __mva_text_height;

        if (repeat < 5) {
            while (repeat > 0) {
                for (row = 0; row < (unsigned int) __mva_text_height; row++) {
                    out_ptr[num_bytes * row] = glyph[row];
                }
                out_ptr++;
                repeat--;
            }
        } else {
            for (row = 0; row < (unsigned int) __mva_text_height; row++) {
                memset(out_buf + repeat * row, glyph[row], repeat);
            }
        }

        ByteGraphTextOut(x, y, num_bytes, out_buf);
    }
}

/* Byte-aligned sibling of GraphTextOut() -- no partial-byte masking
   needed, so every column just gets a plain background-then-foreground
   pair of writes with the Bit Mask register left at 0xFF throughout. */
void ByteGraphTextOut(int xpos1, int ypos, int num_bytes, unsigned char far * text_buf)
{
    unsigned char far * rgen;
    unsigned char bg, fg;
    unsigned int row_stride;
    unsigned int row;
    int col;

    row_stride = MVA_ROW_STRIDE;
    bg = _char_attrib >> 4;
    fg = _char_attrib & 0xf;

    outportb(0x3ce, 5);
    outportb(0x3cf, 2);

    for (col = 0; col < num_bytes; col++) {
        rgen = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + ypos * row_stride + (xpos1 >> 3) + col);
        for (row = 0; row < (unsigned int) __mva_text_height; row++) {
            outportb(0x3ce, 8);
            outportb(0x3cf, 0xff);
            *rgen = bg;
            outportb(0x3ce, 8);
            outportb(0x3cf, text_buf[row * num_bytes + col]);
            *rgen = fg;
            rgen += row_stride;
        }
    }

    outportb(0x3ce, 8);
    outportb(0x3cf, 0xff);
    outportb(0x3ce, 5);
    outportb(0x3cf, 0);
}

/*
 * grfbox/grbox_1x1/grbox_2x2/gvline_1/gvline_2/ghline_1/ghline_2 are all
 * thin wrappers translating a character-cell (row,col) grid position --
 * using the current font's __mva_text_width/__mva_text_height -- into
 * pixel coordinates for the underlying pixel-based primitives, used by
 * the box/line-drawing UI chrome (menus, dialogs, and similar).
 */
void grfbox(int row, int col, int width, int height)
{
    FillBox(col * __mva_text_width + __mva_text_width / 2,
            row * __mva_text_height + __mva_text_height / 2,
            width * __mva_text_width - __mva_text_width + 1,
            height * __mva_text_height - __mva_text_height + 1);
}

void grbox_1x1(int row, int col, int width, int height)
{
    GraphBox(col * __mva_text_width + __mva_text_width / 2,
             row * __mva_text_height + __mva_text_height / 2,
             width * __mva_text_width - __mva_text_width + 1,
             height * __mva_text_height - __mva_text_height + 1);
}

/* Double-lined box: an outer GraphBox() a couple of pixels further out,
   plus an inner one, giving the appearance of a 2-pixel-thick border. */
void grbox_2x2(int row, int col, int width, int height)
{
    int x, y;

    x = col * __mva_text_width + __mva_text_width / 2;
    y = row * __mva_text_height + __mva_text_height / 2;

    GraphBox(x - 2, y - 2,
             width * __mva_text_width - __mva_text_width + 4,
             height * __mva_text_height - __mva_text_height + 2);
    GraphBox(x, y,
             width * __mva_text_width - __mva_text_width,
             height * __mva_text_height - __mva_text_height - 2);
}

void gvline_1(int row, int col, int height)
{
    int x, y;

    x = col * __mva_text_width + __mva_text_width / 2;
    y = row * __mva_text_height;
    DrawVLine(x, y, height * __mva_text_height);
}

/* Double-lined vertical rule: two single-pixel lines one pixel either
   side of centre. */
void gvline_2(int row, int col, int height)
{
    int x, y1, h;

    x = col * __mva_text_width + __mva_text_width / 2;
    y1 = row * __mva_text_height;
    h = height * __mva_text_height;
    DrawVLine(x - 1, y1, h);
    DrawVLine(x + 1, y1, h);
}

void ghline_1(int row, int col, int length)
{
    int x, y;

    x = col * __mva_text_width;
    y = row * __mva_text_height + __mva_text_height / 2;
    DrawHLine(x, y, length * __mva_text_width);
}

/* Double-lined horizontal rule: two single-pixel lines two pixels
   apart. */
void ghline_2(int row, int col, int length)
{
    int x1, y1;

    x1 = col * __mva_text_width;
    y1 = row * __mva_text_height + __mva_text_height / 2;
    DrawHLine(x1, y1 - 2, length * __mva_text_width);
    DrawHLine(x1, y1, length * __mva_text_width);
}

/*
 * Draws (status!=0) or erases (status==0) a text-cursor underline two
 * pixels tall beneath the given character cell. Erasing temporarily
 * switches the drawing colour to background-on-background (so the two
 * DrawHLine() calls paint over the cursor with the cell's background
 * colour) and restores the real _char_attrib afterward.
 */
/*
 * Fill/border colour pair used by the flood-fill family below
 * (SetFill/FillArea/ScanLeft/ScanRight). Confirmed via Ghidra xref
 * analysis to be referenced only from those functions, so file-scope
 * here.
 */
static int fill_colour;
static int border_colour;

void GraphCursor(int status, int row, int col)
{
    unsigned char saved_attrib;
    int x1, y2;

    saved_attrib = _char_attrib;
    if (status == 0) {
        set_colour(_char_attrib >> 4, _char_attrib >> 4);
    }

    x1 = col * __mva_text_width;
    y2 = row * __mva_text_height + __mva_text_height;
    DrawHLine(x1, y2 - 1, __mva_text_width);
    DrawHLine(x1, y2 - 2, __mva_text_width);

    _char_attrib = saved_attrib;
}

/*
 * ScanLeft()/ScanRight(): walk left/right from (*x,*y) one pixel at a
 * time until hitting a pixel matching border_colour or fill_colour (or
 * the screen edge), leaving *x on the last "inside" pixel before the
 * boundary. Used by FillArea()'s scanline flood fill below.
 *
 * (Ghidra's own function list already names the ScanRight half at
 * 284b:2b7d correctly -- the stub file previously also carried a
 * duplicate "unk_func_2B7D" placeholder for this same address with a
 * stale "name not recoverable" TODO; that duplicate has been merged into
 * this one real definition.)
 */
static void ScanLeft(int far * x, int far * y)
{
    int p;

    do {
        (*x)--;
        p = ReadPixel(*x, *y);
        if (p == border_colour || p == fill_colour) {
            break;
        }
    } while (*x >= 0);
    (*x)++;
}

static void ScanRight(int far * x, int far * y)
{
    int p;

    do {
        (*x)++;
        p = ReadPixel(*x, *y);
        if (p == border_colour || p == fill_colour) {
            break;
        }
    } while (*x < 0x280);
    (*x)--;
}

/*
 * VRAM-to-VRAM block copy, one scanline at a time via memcpyb() under
 * Write Mode 1 (GC5=1) -- in Write Mode 1, a byte read from the screen
 * latches all 4 planes, and any subsequent byte write echoes that latch
 * back out, so a plain byte copy transfers whole pixels without needing
 * to loop over planes. Used by GraphBoxScroll() below.
 *
 * NOTE: memcpyb() (MAZEUTIL.C) is confirmed, via Ghidra disassembly, to
 * be a genuinely empty function in the shipped binary -- see the comment
 * there. That means this copy (and GraphBoxScroll()'s scroll-copy that
 * depends on it) really was a no-op in PACWARS.EXE as shipped; the port
 * I/O and loop structure here are reproduced faithfully regardless.
 */
void copy_screen(unsigned char far * dest, unsigned char far * source, int w, int h)
{
    unsigned int row_stride;
    int row;

    row_stride = MVA_ROW_STRIDE;

    outportb(0x3ce, 5);
    outportb(0x3cf, 1);

    for (row = 0; row < h; row++) {
        memcpyb(dest, source, w);
        dest += row_stride;
        source += row_stride;
    }

    outportb(0x3ce, 5);
    outportb(0x3cf, 0);
}

void SetFill(int fill, int border)
{
    fill_colour = fill;
    border_colour = border;
}

/*
 * Recursive scanline flood fill: finds the run of non-border/non-fill
 * pixels on the current row containing (sx,sy) via ScanLeft()/
 * ScanRight(), draws it, then recurses into the row above and below for
 * any sub-runs not already covered by the caller's [prev_xl,prev_xr]
 * range. prev_xr is accepted (matching the real recovered signature) but
 * genuinely unused in the body, same as in the shipped binary.
 */
int FillArea(int sx, int sy, int dir, int prev_xl, int prev_xr)
{
    int xl, xr, y;
    int i, j;
    int p;

    y = sy;
    xl = sx;
    xr = sx;
    ScanLeft(&xl, &y);
    ScanRight(&xr, &y);
    DrawHLine(xl, y, xr - xl + 1);

    for (i = xl; i <= xr; i++) {
        p = ReadPixel(i, y + dir);
        if (p != border_colour && p != fill_colour) {
            i = FillArea(i, y + dir, dir, xl, xr);
        }
    }

    for (j = xl; j < prev_xl; j++) {
        p = ReadPixel(j, y - dir);
        if (p != border_colour && p != fill_colour) {
            j = FillArea(j, y - dir, -dir, xl, xr);
        }
    }

    return xr;
}

/*
 * Scrolls a w x h pixel box up (direction==0) or down (direction==1) by
 * num_lines, via repeated copy_screen() calls, then fills the newly
 * exposed edge with FillBox(). direction values other than 0/1 are a
 * no-op in the original binary.
 */
void GraphBoxScroll(int x, int y, int w, int h, int direction, int num_lines)
{
    unsigned char far * base;
    unsigned char far * dest;
    unsigned char far * source;
    unsigned int row_stride;
    int fill_height;
    int row;

    fill_height = h;

    if (num_lines != 0) {
        fill_height = num_lines;
        row_stride = MVA_ROW_STRIDE;

        if (direction == 0) {
            base = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + (x >> 3) + y * row_stride);
            copy_screen(base, base + (long) num_lines * row_stride, w >> 3, h - num_lines);
            y = y + h - num_lines;
        } else if (direction == 1) {
            dest = (unsigned char far *) MK_FP(VGA_SEGMENT, (__disp_page << 15) + (x >> 3) + (y + h - 1) * row_stride);
            source = dest - (long) num_lines * row_stride;
            for (row = 0; row < h - num_lines; row++) {
                copy_screen(dest, source, w >> 3, 1);
                dest -= row_stride;
                source -= row_stride;
            }
        } else {
            return;
        }
    }

    FillBox(x, y, w, fill_height);
}

/* Waits out the remainder of the current vertical retrace, then waits
   for the next one to begin -- port 0x3DA bit 3 is the CRTC's vertical
   retrace status bit. */
void waitfor_interrupt(void)
{
    while ((inport(0x3da) & 8) == 0) {
    }
    while ((inport(0x3da) & 8) != 0) {
    }
}

