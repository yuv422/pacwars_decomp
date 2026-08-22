/*
 * MEM256.C
 *
 * Reconstructed from PACWARS.EXE, segment 2d0d (real addresses
 * 2d0d:0009-00d9). These are four low-level far-pointer block-copy
 * primitives supporting the 256-color graphics engine (GRAPH256.C):
 * a plain forward copy, a reverse (high-to-low) copy for use when
 * shifting/overlapping data, and two "strided" copies for moving
 * rectangular WxH blocks between buffers of different row pitch
 * (screen blits), one of which treats byte value 0 as a transparent
 * color key (sprite masking).
 *
 * All four compiled functions leave DS clobbered/restored via a
 * PUSH/POP around the copy (large-model far-pointer loads via
 * LES/LDS temporarily repoint DS), which is purely a register-
 * allocation detail with no observable effect in C and isn't
 * reproduced here.
 */
#include "MEM256.H"
#include <string.h>

/*
 * Plain forward copy of n bytes, dst<-src (2d0d:0009-0037): loads
 * ES:DI=dst, DS:SI=src, then REP MOVSW for n/2 words followed by a
 * MOVSB for a trailing odd byte -- exactly what large-model memcpy()
 * compiles to here (default char and void pointers are already far
 * pointers in this memory model), so implemented directly via the standard
 * library call. Confirmed via disassembly that the real function
 * reloads dst's original segment:offset into DX:AX right before
 * RETF, i.e. it genuinely returns dst.
 */
void far * memcpyzzz(void far * dst, void far * src, unsigned int n)
{
    memcpy(dst, src, n);
    return dst;
}

/*
 * Reverse copy of n bytes, processed from the last byte back to the
 * first (2d0d:0038-0069): DI/SI are set to dst+n-1/src+n-1, the
 * direction flag is set (STD), then REP MOVSB walks backward. Unlike
 * memcpyzzz this only ever moves byte-at-a-time and always goes
 * high-to-low regardless of whether dst/src actually overlap --
 * callers presumably use this specifically when they need a
 * shift-right-style copy (dst > src, overlapping) where a forward
 * copy would corrupt not-yet-read source bytes. Confirmed dst is
 * reloaded into DX:AX before return, same as memcpyzzz.
 */
void far * memcpyr(void far * dst, void far * src, unsigned int n)
{
    unsigned int i;
    char far * d = (char far *) dst;
    char far * s = (char far *) src;

    for (i = n; i > 0; i--) {
        d[i - 1] = s[i - 1];
    }
    return dst;
}

/*
 * Strided rectangular copy, h rows of w bytes each (2d0d:006a-00a0):
 * per row this is the same word+odd-byte copy as memcpyzzz, then both
 * dst and src are advanced by (w + their own wrap value) before the
 * next row -- i.e. dest_wrap/source_wrap are the *extra* bytes to
 * skip past each row's w bytes, not the total row pitch. Used for
 * blitting a WxH block between two buffers whose row widths differ
 * from the block width (e.g. copying between a screen buffer and an
 * off-screen bitmap of a different stride).
 *
 * Confirmed via disassembly that -- unlike memcpyzzz/memcpyr -- this
 * function does NOT reload dst's original value into DX:AX before
 * returning; the DX:AX pair at RETF holds whatever DS-restore
 * bookkeeping was left over from the PUSH/POP DX sequence, not a
 * meaningful pointer. No caller relies on this return value (the
 * compiler wouldn't have skipped setting it up otherwise), so `dst`
 * is still returned here for a sane, header-matching signature rather
 * than reproducing genuinely meaningless register garbage.
 */
void far * memcpyv(void far * dst, void far * src, unsigned int w, unsigned int h, unsigned int dest_wrap, unsigned int source_wrap)
{
    void far * dst0 = dst;
    char far * d = (char far *) dst;
    char far * s = (char far *) src;

    while (h > 0) {
        memcpy(d, s, w);
        d += (long) w + dest_wrap;
        s += (long) w + source_wrap;
        h--;
    }
    return dst0;
}

/*
 * Strided, colour-keyed rectangular copy, h rows of w bytes each
 * (2d0d:00a1-00d9): per row, each of the w source bytes is copied to
 * dst UNLESS it's 0, in which case it's skipped (dst left untouched,
 * both pointers still advance) -- the standard "0 = transparent"
 * masking convention for sprite blits. After each row, dst is
 * advanced by an extra `wrap` bytes (skipping to the next screen row);
 * src has no wrap parameter at all and is left to continue linearly,
 * i.e. the source sprite data is assumed tightly packed with no
 * padding between rows.
 *
 * Same situation as memcpyv: disassembly confirms dst is not reloaded
 * into DX:AX before RETF, so the real return value is unused/
 * meaningless register leftovers -- `dst` is returned here instead.
 */
void far * memcpya(void far * dst, void far * src, unsigned int w, unsigned int h, unsigned int wrap)
{
    void far * dst0 = dst;
    char far * d = (char far *) dst;
    char far * s = (char far *) src;
    unsigned int x;

    while (h > 0) {
        for (x = 0; x < w; x++) {
            if (*s != 0) {
                *d = *s;
            }
            d++;
            s++;
        }
        d += wrap;
        h--;
    }
    return dst0;
}
