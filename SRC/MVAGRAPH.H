/*
 * MVAGRAPH.H
 *
 * Public prototypes exported by MVAGRAPH.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef MVAGRAPH_H
#define MVAGRAPH_H

#include "PACWARS.H"

int SetGraphText(int status);
int SetTextFont(int text_table);
int LoadGraphFont(void);
int SaveGraphFont(void);
void SetPixel(int x, int y, int colour);
int ReadPixel(int x, int y);
void DrawLine(int x1, int y1, int x2, int y2);
void DrawHLine(int x1, int y1, int width);
void DrawVLine(int x1, int y1, int height);
void PolyLine(int num_points, POINT far * points);
void Polygon(int num_points, POINT far * points);
void FillPolygon(int num_points, POINT far * points);
void Circle(int x, int y, int r, int fill);
void Ellipse(int cx, int cy, int ra, int rb, int fill);
void FillSegment(int cx, int cy, int ra, int rb, int sa, int ea, int h, int border, int explode);
void Arc(int cx, int cy, int ra, int rb, int sa, int ea, int fill, int seg, int explode, int h, int border);
void GraphBox(int x, int y, int w, int h);
void FillBox(int x1, int y1, int width, int height);
void ClsGraphViewport(int y1, int h);
void GetBitBlock(int x, int y, int width, int height, unsigned char far * sc_buffer);
void SetBitBlock(int x, int y, int width, int height, unsigned char far * sc_buffer);
void GraphText(int x, int y, unsigned char far * text_ptr);
void GraphChar(int x, int y, unsigned char text_char, int repeat);
void BitGraphText(int x, int y, unsigned char far * text_ptr);
void BitGraphChar(int x, int y, unsigned char text_char, int repeat);
void GraphTextOut(int xpos1, int ypos, int xpos2, unsigned char far * text_buf);
void ByteGraphText(int x, int y, unsigned char far * text_ptr);
void ByteGraphChar(int x, int y, unsigned char text_char, int repeat);
void ByteGraphTextOut(int xpos1, int ypos, int num_bytes, unsigned char far * text_buf);
void grfbox(int row, int col, int width, int height);
void grbox_1x1(int row, int col, int width, int height);
void grbox_2x2(int row, int col, int width, int height);
void gvline_1(int row, int col, int height);
void gvline_2(int row, int col, int height);
void ghline_1(int row, int col, int length);
void ghline_2(int row, int col, int length);
void GraphCursor(int status, int row, int col);
void copy_screen(unsigned char far * dest, unsigned char far * source, int w, int h);
void SetFill(int fill, int border);
int FillArea(int sx, int sy, int dir, int prev_xl, int prev_xr);
void GraphBoxScroll(int x, int y, int w, int h, int direction, int num_lines);
void waitfor_interrupt(void);

#endif /* MVAGRAPH_H */
