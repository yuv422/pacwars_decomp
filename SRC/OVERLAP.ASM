.model large
.data
.code

                public  _do_under_asm
                public  _do_over_asm

;=============================================================================
; _do_under_asm / _do_over_asm
;
; Pixel-by-pixel compositing helpers used by overlap_sprite() (MAZESPT.C)
; to merge two SPRITE_STRUCT bitmaps. Reconstructed from the raw
; disassembly at 2315:000e (_do_under_asm) and 2315:0053 (_do_over_asm) in
; PACWARS.EXE -- this module (OVERLAP.ASM/OVERLAP.OBJ) existed in the
; project's stub set as an empty placeholder (no real routines), the same
; "gap" class of bug documented elsewhere in this project's decompiled
; modules (a real routine present in the original binary but missing from
; the generated stub file).
;
; C prototypes (MAZESPT.C):
;   void do_under_asm(unsigned char far * dest, unsigned char far * src,
;                      int h, int w, int dest_wrap, int src_wrap);
;   void do_over_asm(unsigned char far * dest, unsigned char far * src,
;                     int h, int w, int dest_wrap, int src_wrap);
;
; Both walk an h-row by w-column block of two far-pointer buffers:
;   _do_under_asm copies src->dest only where *dest is currently 0
;     (fills in still-transparent destination pixels, leaving whatever
;     is already opaque in dest untouched).
;   _do_over_asm copies src->dest only where *src is nonzero (standard
;     transparent-colour-key overlay: dest is untouched wherever src is
;     the transparent colour 0).
; After each row, dest/src are advanced by dest_wrap/src_wrap (the
; caller-supplied "own width minus copy width" remainder) to skip to the
; start of the next row in each buffer.
;
; Stack frame (large model far call, cdecl): confirmed by disassembling
; _do_under_asm's own prologue --
;   [BP+6]  dest (far ptr, 4 bytes: offset @ BP+6, segment @ BP+8)
;   [BP+0Ah] src  (far ptr, 4 bytes: offset @ BP+0Ah, segment @ BP+0Ch)
;   [BP+0Eh] h    (word on stack, only low byte used)
;   [BP+10h] w    (word on stack, only low byte used)
;   [BP+12h] dest_wrap (word)
;   [BP+14h] src_wrap  (word)
;=============================================================================

_do_under_asm   PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    ds
                push    es

                lds     si, [bp+6]              ; DS:SI = dest
                les     di, [bp+0Ah]             ; ES:DI = src
                mov     bl, 0                   ; row counter
                mov     bh, byte ptr [bp+0Eh]    ; h
                mov     ch, byte ptr [bp+10h]    ; w

row_loop_u:
                cmp     bl, bh
                jz      done_u
                mov     cl, 0                   ; column counter

col_loop_u:
                cmp     cl, ch
                jz      next_row_u

                cmp     byte ptr [si], 0        ; *dest == 0 ?
                jz      copy_u                  ; yes -> fill it from src
                inc     si
                inc     di
                inc     cl
                jmp     col_loop_u

copy_u:
                mov     al, es:[di]
                mov     byte ptr [si], al
                inc     si
                inc     di
                inc     cl
                jmp     col_loop_u

next_row_u:
                add     si, word ptr [bp+12h]   ; dest_wrap
                add     di, word ptr [bp+14h]   ; src_wrap
                inc     bl
                jmp     row_loop_u

done_u:
                pop     es
                pop     ds
                pop     di
                pop     si
                pop     bp
                retf

_do_under_asm   ENDP


_do_over_asm    PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    ds
                push    es

                lds     si, [bp+6]              ; DS:SI = dest
                les     di, [bp+0Ah]             ; ES:DI = src
                mov     bl, 0                   ; row counter
                mov     bh, byte ptr [bp+0Eh]    ; h
                mov     ch, byte ptr [bp+10h]    ; w

row_loop_o:
                cmp     bl, bh
                jz      done_o
                mov     cl, 0                   ; column counter

col_loop_o:
                cmp     cl, ch
                jz      next_row_o

                cmp     byte ptr es:[di], 0     ; *src == 0 ?
                jnz     copy_o                  ; no (opaque) -> overwrite dest
                inc     si
                inc     di
                inc     cl
                jmp     col_loop_o

copy_o:
                mov     al, es:[di]
                mov     byte ptr [si], al
                inc     si
                inc     di
                inc     cl
                jmp     col_loop_o

next_row_o:
                add     si, word ptr [bp+12h]   ; dest_wrap
                add     di, word ptr [bp+14h]   ; src_wrap
                inc     bl
                jmp     row_loop_o

done_o:
                pop     es
                pop     ds
                pop     di
                pop     si
                pop     bp
                retf

_do_over_asm    ENDP

end
