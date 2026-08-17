/*
 * MVAGRAPH.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "MVAGRAPH.H"

int SetGraphText(int status)
{
    return 0;
}

int SetTextFont(int text_table)
{
    return 0;
}

int LoadGraphFont(void)
{
    return 0;
}

int SaveGraphFont(void)
{
    return 0;
}

void SetPixel(int x, int y, int colour)
{
}

int ReadPixel(int x, int y)
{
    return 0;
}

void DrawLine(int x1, int y1, int x2, int y2)
{
}

void DrawHLine(int x1, int y1, int width)
{
}

void DrawVLine(int x1, int y1, int height)
{
}

void PolyLine(int num_points, POINT far * points)
{
}

void Polygon(int num_points, POINT far * points)
{
}

void FillPolygon(int num_points, POINT far * points)
{
}

void FillLine(int x1, int y1, int x2, int y2, unsigned int far * data, int line)
{
}

void Circle(int x, int y, int r, int fill)
{
}

void CirclePoint(int cx, int cy, int px, int py, int colour)
{
}

void CircleFill(int cx, int cy, int px, int py)
{
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
}

void FillBox(int x1, int y1, int width, int height)
{
}

void ClsGraphViewport(int y1, int h)
{
}

void GetBitBlock(int x, int y, int width, int height, unsigned char far * sc_buffer)
{
}

void SetBitBlock(int x, int y, int width, int height, unsigned char far * sc_buffer)
{
}

void GraphText(int x, int y, unsigned char far * text_ptr)
{
}

void GraphChar(int x, int y, unsigned char text_char, int repeat)
{
}

void BitGraphText(int x, int y, unsigned char far * text_ptr)
{
}

void BitGraphChar(int x, int y, unsigned char text_char, int repeat)
{
}

void GraphTextOut(int xpos1, int ypos, int xpos2, unsigned char far * text_buf)
{
}

void ByteGraphText(int x, int y, unsigned char far * text_ptr)
{
}

void ByteGraphChar(int x, int y, unsigned char text_char, int repeat)
{
}

void ByteGraphTextOut(int xpos1, int ypos, int num_bytes, unsigned char far * text_buf)
{
}

void grfbox(int row, int col, int width, int height)
{
}

void grbox_1x1(int row, int col, int width, int height)
{
}

void grbox_2x2(int row, int col, int width, int height)
{
}

void gvline_1(int row, int col, int height)
{
}

void gvline_2(int row, int col, int height)
{
}

void ghline_1(int row, int col, int length)
{
}

void ghline_2(int row, int col, int length)
{
}

void GraphCursor(int status, int row, int col)
{
}

void ScanLeft(int row, int col, int w, int h, int row2)
{
}

void copy_screen(unsigned char far * dest, unsigned char far * source, int w, int h)
{
}

void SetFill(int fill, int border)
{
}

int FillArea(int sx, int sy, int dir, int prev_xl, int prev_xr)
{
    return 0;
}

void ScanRight(int far * x, int far * y)
{
}

/* TODO: real name not recoverable from PACWARS.TXT for this local
   function (module's type-def list is shorter than its true local-
   function count) -- offset 184B:2B7D. Verify/rename by
   cross-referencing the Ghidra project. */
void unk_func_2B7D(int far * x, int far * y)
{
}

void GraphBoxScroll(int x, int y, int w, int h, int direction, int num_lines)
{
}

void waitfor_interrupt(void)
{
}

