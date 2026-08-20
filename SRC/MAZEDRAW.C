/*
 * MAZEDRAW.C
 *
 * Reconstructed from PACWARS.EXE via Ghidra decompilation/disassembly of
 * the real function bodies. Draws the 25-row x 30-column maze grid (each
 * cell a 8x8 sprite) for the room currently selected by _hoffset/
 * _voffset, plus the "attribute" overlay (small coloured boxes with a
 * single-letter label marking special maze cells).
 */
#include "MAZEDRAW.H"
#include "GRAPH256.H"
#include "MAZESPT.H"
#include "MAZEUTIL.H"
#include "MVAGRAPH.H"

/* storage for the _sprites global declared extern in PACWARS.H; see the
   comment there -- load_all_sprites() (ALLOC_BLOCK_MEM.C, not yet
   decompiled) is the intended real populator. */
SPRITE_STRUCT _sprites[1249];

/*
 * Draws every non-empty cell of the current room's maze as its
 * corresponding sprite (maze cell values are 1-based sprite indices,
 * offset by 0x40 into the shared sprite table).
 */
void draw_maze(void)
{
    unsigned int far * maze;
    int row, col;
    int sprite_num;

    maze = (unsigned int far *) maze_def(_hoffset, _voffset);
    cls_256_screen();

    for (row = 0; row < 25; row++) {
        for (col = 0; col < 30; col++) {
            if (maze[row * 30 + col] != 0) {
                sprite_num = maze[row * 30 + col] + 0x40;
                _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
                _sprites[sprite_num].spritex = col * _sprites[sprite_num].spritew;
                display_sprite(sprite_num);
            }
        }
    }
}

/*
 * Redraws just the [row1,row2] x [col1,col2] sub-rectangle of the maze
 * grid for the given room (curr_hoff/curr_voff, not necessarily the
 * currently-displayed one) -- used to patch a small area rather than
 * repainting the whole screen. Empty cells are drawn using sprite slot 1
 * sized to match the "blank tile" sprite (0x41) rather than being
 * skipped, unlike draw_maze().
 */
void draw_maze_area(int curr_hoff, int curr_voff, int row1, int col1, int row2, int col2)
{
    unsigned int far * maze;
    int row, col;
    int sprite_num;

    maze = (unsigned int far *) maze_def(curr_hoff, curr_voff);

    if (row2 > 0x17) {
        row2 = 0x18;
    }
    if (col2 > 0x1c) {
        col2 = 0x1d;
    }

    for (row = row1; row <= row2; row++) {
        for (col = col1; col <= col2; col++) {
            if (maze[row * 30 + col] == 0) {
                sprite_num = 1;
                _sprites[1].spritew = _sprites[0x41].spritew;
                _sprites[1].spriteh = _sprites[0x41].spriteh;
            } else {
                sprite_num = maze[row * 30 + col] + 0x40;
            }
            _sprites[sprite_num].spritey = row * _sprites[sprite_num].spriteh;
            _sprites[sprite_num].spritex = col * _sprites[sprite_num].spritew;
            display_sprite(sprite_num);
        }
    }
}

/*
 * Draws draw_attrib() boxes over every positive-valued cell of the
 * current room's attribute grid (a second maze layer marking special
 * cells -- gold spots, bomb spots, and similar -- separate from the
 * sprite-tile layer draw_maze() uses). Temporarily switches to the 8x5
 * graphics font (text_table 4) for the single-letter labels, restoring
 * whatever font was active before.
 */
void draw_attribs(void)
{
    int far * attribs;
    int row, col;
    int attrib;
    int font;

    attribs = (int far *) attrib_maze_def(_hoffset, _voffset);
    font = SetTextFont(-1);
    SetTextFont(4);

    for (row = 0; row < 25; row++) {
        for (col = 0; col < 30; col++) {
            attrib = attribs[row * 30 + col];
            if (attrib != 0 && attrib > 0) {
                draw_attrib(row, col, attrib);
            }
        }
    }

    SetTextFont(font);
}

/*
 * Draws one attribute-grid cell as an 8x8 coloured box (trfbox) with a
 * single-letter label (text256) centred in it, both looked up by attrib
 * (1-based) in small compiled-in tables. Switches to the 8x5 graphics
 * font only if it isn't already selected.
 */
void draw_attrib(int row, int col, int attrib)
{
    static char attrib_letter[5][2] = { {'F', '\0'}, {'G', '\0'}, {'B', '\0'}, {'K', '\0'}, {'W', '\0'} };
    static char attrib_colour[5] = { 12, 3, 2, 0, 0 };
    int font;

    font = SetTextFont(-1);
    if (font != 4) {
        SetTextFont(4);
    }

    trfbox(col * 8, row * 8, 8, 8, attrib_colour[attrib - 1]);
    text256(col * 8 + 2, row * 8, (unsigned char far *) attrib_letter[attrib - 1], 14, attrib_colour[attrib - 1]);

    if (font != 4) {
        SetTextFont(font);
    }
}

/*
 * Redraws just the maze cells that a logged missile/shot's sprite
 * covered, for whichever of the 5 log slots recorded an active shot
 * (shot==1) with a nonzero missile type in the room currently on screen
 * (shot_hoffset/shot_voffset matching _hoffset/_voffset). Used to erase
 * a shot's sprite by repainting the maze tiles underneath it. Empty
 * cells fall back to a fixed 8x8 sprite slot 1, same as
 * draw_maze_area().
 */
void restore_maze(MAZE_LOG_STRUCT far * maze_log)
{
    unsigned int far * maze;
    int ws;
    int sprite_num;
    int shot_x, shot_y;
    unsigned int right, bottom;
    int row0, col0;
    int row, col;

    maze = (unsigned int far *) maze_def(_hoffset, _voffset);

    for (ws = 0; ws < 5; ws++) {
        if (maze_log->status[ws].shot == 1 &&
            maze_log->status[ws].missile != 0 &&
            maze_log->status[ws].shot_hoffset == _hoffset &&
            maze_log->status[ws].shot_voffset == _voffset) {

            sprite_num = maze_log->status[ws].missile + 0x4d6;
            shot_x = maze_log->status[ws].shot_x;
            shot_y = maze_log->status[ws].shot_y;

            right = shot_x + _sprites[sprite_num].spritew;
            bottom = shot_y + _sprites[sprite_num].spriteh;
            col0 = shot_x / 8;
            row0 = shot_y / 8;

            for (row = row0; row < row0 + (bottom / 8 - row0) + ((bottom & 7) != 0); row++) {
                for (col = col0; col < col0 + (right / 8 - col0) + ((right & 7) != 0); col++) {
                    if (row >= 0 && col >= 0 && row < 0x19 && col < 0x1e) {
                        if (maze[row * 30 + col] == 0) {
                            sprite_num = 1;
                            _sprites[1].spritew = 8;
                            _sprites[1].spriteh = 8;
                        } else {
                            sprite_num = maze[row * 30 + col] + 0x40;
                        }
                        _sprites[sprite_num].spritey = row << 3;
                        _sprites[sprite_num].spritex = col << 3;
                        display_sprite(sprite_num);
                    }
                }
            }
        }
    }
}

