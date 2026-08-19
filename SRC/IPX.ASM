.model large
.data

NETWARE_INT_7A_FUNC DD 0h
 
.code


_init_ipx_asm   PROC    FAR

                push    si
                push    di
                push    ds

                mov     ax, 7A00h               ; AH=7Ah : IPX installation check
                int     2Fh                     ; DOS multiplex interrupt
                cmp     al, 0FFh                ; AL=FFh -> IPX/SPX is resident
                jnz     short not_installed

;-- IPX is installed: ES:DI holds the driver's far entry point --------
                mov     ax, es
                mov     word ptr NETWARE_INT_7A_FUNC,   di
                mov     word ptr NETWARE_INT_7A_FUNC+2, ax
                mov     ax, 1

                ; NOTE: this is a genuine JP (jump-if-parity), not a
                ; mis-disassembled JMP. AL-0FFh above produced a result
                ; of 0 (even parity -> PF=1), and none of the three MOV
                ; instructions since then touch the flags, so the
                ; compiler reused that stale parity flag to skip the
                ; not_installed block below instead of emitting a
                ; separate unconditional jump. Verified against the raw
                ; opcode bytes (7A 0F) at 2DB9:0018.
                jp      short done

;-- IPX is not installed: zero out the cached entry point -------------
not_installed:
                mov     word ptr NETWARE_INT_7A_FUNC,   0
                mov     word ptr NETWARE_INT_7A_FUNC+2, 0
                mov     ax, 0

done:
                pop     ds
                pop     di
                pop     si
                retf

_init_ipx_asm   ENDP

end
