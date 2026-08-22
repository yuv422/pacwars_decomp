; IPX.ASM
;
; Reconstructed from PACWARS.EXE (segment 2db9) via Ghidra disassembly.
; These are the raw NetWare IPX driver primitives: once _init_ipx_asm has
; cached the driver's far entry point in NETWARE_INT_7A_FUNC (from the
; INT 2Fh/AH=7Ah installation check), every other routine here just loads
; BX with the IPX "function number" (0=Open Socket, 1=Close Socket,
; 2=Get Local Target, 3=Send Packet, 4=Listen For Packet, 9=Get
; Internetwork Address) and the function-specific registers, then calls
; the driver directly via CALLF [NETWARE_INT_7A_FUNC] instead of going
; through INT 7Ah each time.
;
; open_socket/close_socket byte-swap (or don't) the socket number to/from
; network (big-endian) byte order via manual DH/DL juggling rather than a
; shared helper -- close_socket notably does NOT swap DX before the call,
; unlike open_socket, matching PACWARS.EXE's actual compiled bytes exactly
; (2db9:006d: `MOV DX,[BP+6]` with no swap). Preserved as found rather
; than "fixed" to be symmetric with open_socket.
;
; get_local_address loads ES via two back-to-back LES instructions (for
; its two far-pointer parameters); the second LES overwrites the segment
; the first one set, so ES ends up holding the *second* parameter's
; segment when CALLF executes, not the first's. This matches the raw
; bytes at 2db9:0082-0088 exactly. It's harmless for every call site in
; this project (both pointers passed are always local stack variables in
; the same calling function, hence the same segment either way), so it's
; preserved as-is rather than "fixed".
.model large
.data

NETWARE_INT_7A_FUNC DD 0h

.code

public _init_ipx_asm
public _get_intwork_address
public _get_local_address
public _listen_packet_asm
public _send_packet_asm
public _close_socket
public _open_socket

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


;-----------------------------------------------------------------------
; void far get_intwork_address(char far *out_address)
;
; IPX function 9 (Get Internetwork Address): fills a 10-byte buffer
; (4-byte network number + 6-byte node number) with this station's own
; address. Reconstructed from 2db9:002d-0041.
;-----------------------------------------------------------------------
_get_intwork_address    PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    es

                les     si, [bp+6]              ; ES:SI = out_address
                mov     bx, 9
                call    dword ptr [NETWARE_INT_7A_FUNC]

                pop     es
                pop     di
                pop     si
                pop     bp
                retf

_get_intwork_address    ENDP


;-----------------------------------------------------------------------
; int far open_socket(int socket_no, unsigned int far *out_value)
;
; IPX function 0 (Open Socket): AL=0 (dynamic/short-lived socket type),
; DX=socket_no in network byte order. Returns the driver's status in AX
; and stores the (possibly driver-assigned) socket number, still in
; network byte order, into *out_value. Reconstructed from 2db9:0042-0063.
;-----------------------------------------------------------------------
_open_socket    PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    ds

                mov     bx, [bp+6]              ; BX = socket_no
                mov     dh, bl                  ; DX = byte-swapped socket_no
                mov     dl, bh                  ;      (network byte order)
                mov     bx, 0
                mov     al, 0
                call    dword ptr [NETWARE_INT_7A_FUNC]

                lds     si, [bp+8]              ; DS:SI = out_value
                mov     [si], dx
                mov     ah, 0

                pop     ds
                pop     di
                pop     si
                pop     bp
                retf

_open_socket    ENDP


;-----------------------------------------------------------------------
; void far close_socket(int socket_no)
;
; IPX function 1 (Close Socket): DX=socket_no. Reconstructed from
; 2db9:0064-0078.
;-----------------------------------------------------------------------
_close_socket   PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    ds

                mov     bx, 1
                mov     dx, [bp+6]              ; DX = socket_no (not swapped)
                call    dword ptr [NETWARE_INT_7A_FUNC]

                pop     ds
                pop     di
                pop     si
                pop     bp
                retf

_close_socket   ENDP


;-----------------------------------------------------------------------
; int far get_local_address(IPX_ADDRESS far *address, char far *immediate_out)
;
; IPX function 2 (Get Local Target): given a destination internetwork
; address, computes the 6-byte "immediate address" (the local/physical
; node to actually hand the packet to next) needed by an ECB before
; sending. Reconstructed from 2db9:0079-0092.
;-----------------------------------------------------------------------
_get_local_address      PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    es

                mov     bx, 2
                les     si, [bp+6]              ; ES:SI = address
                les     di, [bp+0ah]             ; ES:DI = immediate_out
                                                 ; (also resets ES to
                                                 ; immediate_out's segment
                                                 ; -- see file banner)
                call    dword ptr [NETWARE_INT_7A_FUNC]
                mov     ah, 0

                pop     es
                pop     di
                pop     si
                pop     bp
                retf

_get_local_address      ENDP


;-----------------------------------------------------------------------
; void far send_packet_asm(ECB far *ecb)
;
; IPX function 3 (Send Packet): ES:SI = ECB address. Reconstructed from
; 2db9:0093-00a7.
;-----------------------------------------------------------------------
_send_packet_asm        PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    es

                les     si, [bp+6]              ; ES:SI = ecb
                mov     bx, 3
                call    dword ptr [NETWARE_INT_7A_FUNC]

                pop     es
                pop     di
                pop     si
                pop     bp
                retf

_send_packet_asm        ENDP


;-----------------------------------------------------------------------
; void far listen_packet_asm(ECB far *ecb)
;
; IPX function 4 (Listen For Packet): ES:SI = ECB address; posts the ECB
; to receive the next packet on its socket. Reconstructed from
; 2db9:00a8-00bc.
;-----------------------------------------------------------------------
_listen_packet_asm      PROC    FAR

                push    bp
                mov     bp, sp
                push    si
                push    di
                push    es

                les     si, [bp+6]              ; ES:SI = ecb
                mov     bx, 4
                call    dword ptr [NETWARE_INT_7A_FUNC]

                pop     es
                pop     di
                pop     si
                pop     bp
                retf

_listen_packet_asm      ENDP

end
