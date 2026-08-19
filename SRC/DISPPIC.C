/*
 * DISPPIC.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT) and Ghidra decompilation/disassembly of the real
 * function bodies.
 */
#include "DISPPIC.H"
#include "UTILS.H"
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <mem.h>
#include <stdlib.h>
#include <sys\stat.h>

/*
 * Local (non-exported) helpers used above their definitions further down
 * this file -- forward-declared here rather than reordered, matching the
 * convention already used in MAZE.C for main_loop/key_poll.
 */
static void set_400(void);
static int init_array(char far * file_name, int far * fd);
static int read_array(int pic, int far * fd);
static int read_array_400(int far * fd);
static int write_array_400(int far * fd);
static int close_array(int far * fd);
static int load_palette(int far * fd);
static int save_palette(int far * fd);
static void set_palette_buf(char far * pal);

/*
 * Two 768-byte (256 colours * 3 RGB components) VGA DAC palette buffers.
 * Confirmed via Ghidra xref analysis to be referenced only from functions
 * in this file (load_palette/save_palette use _pal_buf; reset_palette/
 * buffer_palette use _play_pal_buf), so file-scope here. Real names
 * recovered from PACWARS.TXT.
 */
static char _pal_buf[768];
static char _play_pal_buf[768];

void load_pic_files(char far * file_name1, char far * file_name2)
{
    int fd;

    set_400();

    init_array(file_name1, &fd);
    read_array(0, &fd);
    close_array(&fd);

    init_array(file_name2, &fd);
    read_array(1, &fd);
    close_array(&fd);
}

void load_pic_file(char far * file_name)
{
    int fd;

    set_400();
    init_array(file_name, &fd);
    read_array_400(&fd);
    close_array(&fd);
}

void save_pic_file(char far * file_name)
{
    int fd;

    init_array(file_name, &fd);
    write_array_400(&fd);
    close_array(&fd);
}

/*
 * Opens (creating if necessary) the given picture file for read/write in
 * binary mode with share-deny-none, matching the raw immediate flags word
 * pushed in the disassembly (0x8144 = O_BINARY | O_CREAT | O_DENYNONE |
 * O_RDWR) -- Ghidra's decompiler mis-rendered this same immediate as a
 * stray register (unaff_SS) plus a bogus third pointer argument, the same
 * failure mode seen earlier in open_hiscore()/init_hiscore().
 */
static int init_array(char far * file_name, int far * fd)
{
    *fd = open(file_name, O_BINARY | O_CREAT | O_DENYNONE | O_RDWR, S_IREAD | S_IWRITE);
    return (*fd == -1) ? -1 : 0;
}

/*
 * Reads a 64000-byte interleaved (4 bytes/pixel-group) picture block for
 * slot `pic` (0 or 1) and de-interleaves it into the 4 VGA hardware bit
 * planes at A000:(pic*16000), one plane at a time via the Sequencer Map
 * Mask (3C4/3C5 index 2) and Graphics Controller Read Map Select (3CE/3CF
 * index 2) registers. The file is read in two 32000-byte halves (a max
 * per-call transfer size in this environment) into a malloc'd scratch
 * buffer before being split out plane-by-plane.
 */
static int read_array(int pic, int far * fd)
{
    void far * block;
    unsigned char far * src;
    unsigned char far * dst;
    unsigned int i;
    unsigned int plane;
    int stat;

    load_palette(fd);
    block = malloc(64000);

    for (i = 0; i < 2; i++) {
        stat = read(*fd, (char far *) block + i * 32000, 32000);
    }

    for (plane = 0; plane < 4; plane++) {
        dst = (unsigned char far *) MK_FP(0xA000, pic * 16000);
        src = (unsigned char far *) block + plane;

        outportb(0x3C4, 2);
        outportb(0x3C5, 1 << plane);
        outportb(0x3CE, 2);
        outportb(0x3CF, plane);

        for (i = 0; i < 16000; i++) {
            *dst = *src;
            dst++;
            src += 4;
        }
    }

    free(block);
    return stat;
}

/*
 * "Mode 400" variant: each plane is already stored contiguously in the
 * file (32000 bytes/plane, no de-interleaving needed), read straight from
 * the file into A000:0000 one plane at a time.
 */
static int read_array_400(int far * fd)
{
    unsigned char far * dst;
    unsigned int plane;

    load_palette(fd);

    for (plane = 0; plane < 4; plane++) {
        dst = (unsigned char far *) MK_FP(0xA000, 0);

        outportb(0x3C4, 2);
        outportb(0x3C5, 1 << plane);
        outportb(0x3CE, 2);
        outportb(0x3CF, plane);

        read(*fd, dst, 32000);
    }

    return 0;
}

/* Mirror of read_array_400(): writes each of the 4 planes at A000:0000
 * back out to the file, 32000 bytes/plane. */
static int write_array_400(int far * fd)
{
    unsigned char far * src;
    unsigned int plane;

    save_palette(fd);

    for (plane = 0; plane < 4; plane++) {
        src = (unsigned char far *) MK_FP(0xA000, 0);

        outportb(0x3C4, 2);
        outportb(0x3C5, 1 << plane);
        outportb(0x3CE, 4);
        outportb(0x3CF, plane);

        write(*fd, src, 32000);
    }

    return 0;
}

static int close_array(int far * fd)
{
    close(*fd);
    return 0;
}

/*
 * Reads a 768-byte palette from the file into _pal_buf, then loads it
 * into the VGA DAC via BIOS INT 10h AX=1012h (Set Block of DAC Color
 * Registers), starting at register 0 for all 256 registers.
 */
static int load_palette(int far * fd)
{
    char buf[768];
    union REGS regs;
    struct SREGS sregs;
    int stat;

    stat = read(*fd, buf, 768);
    if (stat < 768) {
        return -1;
    }

    regs.h.ah = 0x10;
    regs.h.al = 0x12;
    regs.x.bx = 0;
    regs.x.cx = 256;
    regs.x.dx = (int) buf;
    sregs.es = FP_SEG(buf);
    int86x(0x10, &regs, &regs, &sregs);

    memcpy(_pal_buf, buf, 768);
    return 0;
}

static int save_palette(int far * fd)
{
    int stat;

    stat = write(*fd, _pal_buf, 768);
    return (stat < 768) ? -1 : 0;
}

/*
 * Sets up VGA mode 0x13 for "mode 400"-style 4-plane unchained access:
 * disables Chain-4 and Odd/Even addressing (Sequencer index 4, Graphics
 * Controller indices 5 and 6), enables all 4 planes for writing (Sequencer
 * index 2), clears the current display page's 64000-byte VGA buffer, and
 * reprograms the CRTC (indices 9, 0x14, 0x17) for the taller addressing
 * mode this format needs.
 */
static void set_400(void)
{
    unsigned char val;

    set_mode(0x13);

    outportb(0x3C4, 4);
    val = inportb(0x3C5);
    outportb(0x3C5, (val & 0xF7) | 0x04);

    outportb(0x3CE, 5);
    val = inportb(0x3CF);
    outportb(0x3CF, val & 0xEF);

    outportb(0x3CE, 6);
    val = inportb(0x3CF);
    outportb(0x3CF, val & 0xFD);

    outportb(0x3C4, 2);
    outportb(0x3C5, 0x0F);

    memset((void far *) MK_FP(0xA000, __disp_page << 15), 0, 64000);

    outportb(0x3D4, 9);
    val = inportb(0x3D5);
    outportb(0x3D5, val & 0xE0);

    outportb(0x3D4, 0x14);
    val = inportb(0x3D5);
    outportb(0x3D5, val & 0xBF);

    outportb(0x3D4, 0x17);
    val = inportb(0x3D5);
    outportb(0x3D5, val | 0x40);
}

void reset_palette(void)
{
    set_palette_buf(_play_pal_buf);
}

/*
 * Builds a randomized 768-byte palette by picking bytes from a small
 * fixed table of 13 DAC intensity values (rather than fully random
 * garbage), then loads it via set_palette_buf(). Table bytes recovered
 * directly from the binary's data segment (340e:286e); no real name was
 * recoverable for it from PACWARS.TXT.
 */
void rand_palette(void)
{
    static unsigned char seed_values[13] = {
        4, 6, 12, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43
    };
    unsigned char rand_buf[768];
    int i;

    for (i = 0; i < 768; i++) {
        rand_buf[i] = seed_values[rand() % 13];
    }

    set_palette_buf((char far *) rand_buf);
}

/*
 * Loads the given 768-byte palette into the VGA DAC via BIOS INT 10h
 * AX=1012h (Set Block of DAC Color Registers), all 256 registers starting
 * at 0. Copies to a local buffer first since `pal` may be a far pointer
 * into arbitrary memory and int86x's segregs need a matching seg:off pair.
 */
static void set_palette_buf(char far * pal)
{
    char local_pal[768];
    union REGS regs;
    struct SREGS sregs;

    memcpy(local_pal, pal, 768);

    regs.h.ah = 0x10;
    regs.h.al = 0x12;
    regs.x.bx = 0;
    regs.x.cx = 256;
    regs.x.dx = (int) local_pal;
    sregs.es = FP_SEG(local_pal);
    int86x(0x10, &regs, &regs, &sregs);
}

/*
 * Reads the VGA DAC's current 256-register palette via BIOS INT 10h
 * AX=1017h (Read Block of DAC Color Registers) and caches it in
 * _play_pal_buf for later restoration by reset_palette().
 */
void buffer_palette(void)
{
    char local_pal[768];
    union REGS regs;
    struct SREGS sregs;

    regs.h.ah = 0x10;
    regs.h.al = 0x17;
    regs.x.bx = 0;
    regs.x.cx = 256;
    regs.x.dx = (int) local_pal;
    sregs.es = FP_SEG(local_pal);
    int86x(0x10, &regs, &regs, &sregs);

    memcpy(_play_pal_buf, local_pal, 768);
}
