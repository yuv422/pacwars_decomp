/*
 * GRAPH256.H
 *
 * Public prototypes exported by GRAPH256.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef GRAPH256_H
#define GRAPH256_H

#include "PACWARS.H"

void cls_256_screen(void);
void cls_row(int row);
int collision_detect(int spritex1, int spritey1, int spritew1, int spriteh1, int spritex2, int spritey2, int spritew2, int spriteh2);
void trbox(int x, int y, int w, int h, int colour);
void trfbox(int x, int y, int w, int h, int colour);
void hline(int x, int y, int length, int colour);
void vline(int x, int y, int length, int colour);
void set_pixel(int x, int y, int colour);
void set_palette(int reg_no, int reg_val);
void text256(int x, int y, unsigned char far * text_ptr, int fore, int back);
int load_font(void);
void polyline(int num_points, POINT far * points, int colour);
void polygon(int num_points, POINT far * points, int colour);
void fillpolygon(int num_points, POINT far * points, int colour);
void drawline(int x1, int y1, int x2, int y2, int colour);

#endif /* GRAPH256_H */
