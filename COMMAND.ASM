;%    .MODEL  MEMMOD,C
include asmgloba.h        
	LOCALS
	%MACS
	.LALL

	extrn   pwait: far

	.DATA
	db      4096 dup(?)
cstack  label   byte


	.CODE

dbase   dw      @Data

segoff  struc
offs    dw      ?
segm    dw      ?
segoff  ends

main_subr       dw      ?

their_8h        dd ?
their_10h       dd ?
their_13h       dd ?
their_1Bh       dd ?
their_21h       dd ?
their_23h       dd ?
their_24h       dd ?
their_28h       dd ?

dos_segment     dw ?                    ;segment of internal DOS flags
indos_offset    dw ?                    ;offset of INDOS flag
errflag_offset  dw ?                    ;offset of critical error flag
program_status  db 0                    ;popup status
flag_10h        db 0                    ;status of interrupt 10h
flag_13h        db 0                    ;status of interrupt 13h
zflag           db ?                    ;save and restore critical error.
dos_version     db ?                    ;dos major version.
main_countdown  db 20
ss_register     dw ?                    ;SS register storage
sp_register     dw ?                    ;SP register storage
my_psp          dw ?                    ;our PSP.
their_psp       dw ?                    ;PSP segment storage

tick_counter    dw ?
indos_counter   dw ?
errflag_counter dw ?
status_counter  dw ?
bp_counter      dw ?
main_counter    dw ?


their_dta       dd      ?
;
;------------------------------------------------------------------------------
;Interrupt 8 handling routine.
;------------------------------------------------------------------------------
timer:
	pushf                           ;call BIOS routine
	call their_8h
	inc tick_counter
	cmp program_status,0            ;are we already running?
	jne timer_status                ;yes, then suspend ticking.
	cmp flag_10h,0                  ;video flag set?
	jne timer_exit                  ;yes, then exit
	cmp flag_13h,0                  ;disk flag set?
	jne timer_exit                  ;yes, then exit
	push es                         ;save ES and DI
	push di
	mov es,dos_segment              ;check INDOS flag
	mov di,indos_offset
	cmp byte ptr es:[di],0
	jne timer_indos                 ;exit if it's set
	mov di,errflag_offset           ;check critical error flag
	cmp byte ptr es:[di],0
	jne timer_errflag               ;exit if it's set
	mov main_subr,offset cycle
	call main                       ;call body of program
	pop di
	pop es
timer_exit:
	iret
timer_indos:
	inc indos_counter
	pop di                          ;restore registers
	pop es
	iret
timer_errflag:
	inc errflag_counter
	pop di                          ;restore registers
	pop es
	iret
timer_status:
	inc status_counter
	iret

;
;------------------------------------------------------------------------------
;Interrupt 10h handling routine.
;------------------------------------------------------------------------------
video:
	pushf                           ;push flags onto stack
	inc flag_10h                    ;increment flag
	call their_10h                  ;call BIOS routine
	dec flag_10h                    ;decrement flag
	iret

;
;------------------------------------------------------------------------------
;Interrupt 13h handling routine.
;------------------------------------------------------------------------------
my_13:
	pushf                           ;push flags onto stack
	inc flag_13h                    ;set 'busy' flag
	call their_13h                  ;call BIOS routine
	pushf                           ;save output flags
	dec flag_13h                    ;clear flag
	popf                            ;restore output flags
	retf    2                       ;exit without destroying flags

;
;------------------------------------------------------------------------------
;Interrupt 28h handling routine.
;------------------------------------------------------------------------------
my_28:
	pushf                           ;call original routine
	call their_28h
	inc bp_counter
	cmp program_status,0            ;are we already running?
	jne bp_exit                     ;yes, don't enter it again.
	cmp flag_10h,0                  ;video flag set?
	jne bp_exit                     ;yes, then exit
	cmp flag_13h,0                  ;disk flag set?
	jne bp_exit                     ;yes, then exit
	push es                         ;save ES and DI
	push di
	mov es,dos_segment              ;check critical error flag
	mov di,errflag_offset
	cmp byte ptr es:[di],0
	pop di                          ;clean up the stack
	pop es
	jne bp_errflag
	mov main_subr,offset cycle
	call main                       ;call main routine
bp_exit:
	iret                            ;done - exit
bp_errflag:
	inc errflag_counter
	iret                            ;done - exit

;
;------------------------------------------------------------------------------
;Interrupt 21h handling routine.
;------------------------------------------------------------------------------
my_21:
	pushf                           ;save the flags
	or      ah,ah                   ;Doing function zero?
	je      jump_to_dos             ;If yes, take the jump
	cmp     ah,4bh                  ;Doing EXEC function?
	je      jump_to_dos             ;If yes, take the jump
	popf

	pushf
	call    cs:their_21h            ;Do the DOS function

	pushf                           ;Save the result flags
	cmp     cs:program_status,0     ;are we already running?
	jne     no_recursion            ;yes, don't recurse.
	dec     cs:main_countdown
	jne     no_recursion
	mov     cs:main_countdown,20
	mov     main_subr,offset cycle
	call    main                    ;Safe to access disk now
no_recursion:
	popf                            ;Recover DOS result flags

	sti                             ;Must return with interrupts on
	retf    2                       ;Return with DOS result flags
jump_to_dos:
	popf
	jmp     cs:their_21h

;
;------------------------------------------------------------------------------
;Interrupt 24h handling routine (DOS 3.X only).
;------------------------------------------------------------------------------
my_24:
	mov al,3                        ;fail the call in progress
ioexit:
	iret                            ;give control back to DOS

cycle:
	xor     ax,ax                   ;push a null pointer.
	push    ax
	push    ax
	call    pwait
	add     sp,4
	ret

;
;------------------------------------------------------------------------------
;MAIN is the routine called periodically.
;------------------------------------------------------------------------------
main:
	inc main_counter
	mov program_status,1            ;set program active flag
	cli                             ;make sure interrupts are off
	mov ss_register,ss              ;save stack registers
	mov sp_register,sp
	mov ss,cs:dbase                 ; establish interrupt data segment
	mov sp,offset cstack
	sti                             ;enable interrupts
	PUSHALL
       ; push ax
       ; push bx
       ; push cx
       ; push dx
       ; push si
       ; push di
	push ds
	push es
       ; push bp
;
;Set DS and ES segment registers.
;
	push cs                         ;set DS to code segment
	pop ds
	assume  ds:seg dos_version
;
;Save the current active PSP address and activate this PSP.
;
	mov zflag,0                     ;clear flag
	cmp dos_version,2               ;DOS version 2.X?
	jne main5
	mov es,dos_segment              ;point ES:DI to INDOS
	mov di,indos_offset
	cmp byte ptr es:[di],0          ;INDOS clear?
	je main5                        ;yes, then branch
	mov di,errflag_offset           ;point ES:DI to error flag
	cmp byte ptr es:[di],0          ;critical error flag clear?
	jne main5                       ;no, then branch
	mov byte ptr es:[di],1          ;set critical error flag manually
	mov zflag,1                     ;set change flag
main5:
	mov ah,51h                      ;get current PSP segment
	int 21h
	mov their_psp,bx                        ;save it

	mov ah,50h                      ;make this the active PSP
	mov bx,my_psp
	int 21h

	cmp zflag,0                     ;ZFLAG clear?
	je main6                        ;yes, then branch
	mov di,errflag_offset           ;point ES:DI to error flag
	mov byte ptr es:[di],0          ;restore error flag value
main6:
;
;Reset the interrupt 1Bh, 23h, and 24h vectors.
;
	call ioset                      ;reset interrupt vectors
;
;Save the current dta and subdirectory
;
	mov     ah,2fh                  ;get disk transfer address
	int     21h
	mov     their_dta.segm,es
	mov     their_dta.offs,bx

;
;Call the commutator loop of net until nothing gets queued up.
;
	mov     ds,cs:dbase     ; establish interrupt data segment
	assume  ds:nothing
	call    main_subr
;
;Restore the current dta and subdirectory
;
	lds     dx,their_dta
	mov     ah,1ah
	int     21h

;
;Restore interrupt vectors and former active PSP.
;
	mov ah,50h                      ;restore active PSP label
	mov bx,their_psp
	int 21h
	call ioreset                    ;restore interrupt vectors
;
;Restore registers and stack before exit.
;
       ; pop bp                          ;restore registers and exit
	pop es
	pop ds
	POPALL
       ; pop di
       ; pop si
       ; pop dx
       ; pop cx
       ; pop bx
       ; pop ax
	cli                             ;interrupts off
	mov ss,ss_register              ;switch to original stack
	mov sp,sp_register
	sti                             ;interrupts on
	mov program_status,0            ;clear status flag
	ret

;

intset:
;enter with al = interrupt number, cs:dx = offset of new interrupt,
;       cs:di -> place to store old interrupt.

	push    es                      ;get the old interrupt into es:bx
	push    ds                      ;now set the new interrupt to ds:dx.
	mov     bx,cs
	mov     ds,bx

	mov     ah,35h
	int     21h
	mov     [di].segm,es            ;and store it into ds:di.
	mov     [di].offs,bx

	mov     ah,25h
	int     21h

	pop     ds
	pop     es

	ret


intreset:
;enter with al = interrupt number, di -> old interrupt.
	push    ds
	mov     ah,25h
	lds     dx,cs:[di]
	int     21h
	pop     ds
	ret


;------------------------------------------------------------------------------
;IOSET vectors interrupts 1Bh, 23h and 24h to internal handlers.  IORESET
;restores the original vector values.
;------------------------------------------------------------------------------
ioset:
	mov     al,1bh
	mov     di,offset their_1Bh
	mov     dx,offset ioexit
	call    intset

	mov     al,23h
	mov     di,offset their_23h
	mov     dx,offset ioexit
	call    intset

	mov     al,24h
	mov     di,offset their_24h
	mov     dx,offset my_24
	call    intset

	ret

;
ioreset:
	mov     al,24h
	mov     di,offset their_24h
	call    intreset

	mov     al,23h
	mov     di,offset their_23h
	call    intreset

	mov     al,1Bh
	mov     di,offset their_1Bh
	call    intreset

	ret


	public  start_back
start_back      proc
;
;Remember our psp.
;
	mov ah,51h                      ;get current PSP segment
	int 21h
	mov my_psp,bx                   ;save it

;
;Determine which version of DOS is running.
;
init3:
	mov ah,30h                      ;DOS function 30h
	int 21h
	mov dos_version,al              ;major version number
;
;Get and save the address of the INDOS flag.
;
	mov ah,34h                      ;function 34h
	int 21h                         ;get address
	mov dos_segment,es              ;save segment
	mov indos_offset,bx             ;save offset
;
;Get and save the address of the critical error flag.
;
	mov ax,3E80h                    ;CMP opcode
	mov cx,2000h                    ;max search length
	mov di,bx                       ;start at INDOS address
init4:
	repne scasw                     ;do the search
	jcxz init5                      ;branch if search failed
	cmp byte ptr es:[di+5],0BCh     ;verify this is it
	je found                        ;branch if it is
	jmp init4                       ;resume loop if it's not
init5:
	mov cx,2000h                    ;search again
	inc bx                          ;search odd addresses this time
	mov di,bx
init6:
	repne scasw                     ;look for the opcode
	jcxz notfound                   ;not found if loop expires
	cmp byte ptr es:[di+5],0BCh     ;verify this is it
	je found
	jmp init6
notfound:
	xor     ax,ax
	ret
found:
	mov ax,es:[di]                  ;get flag offset address
	mov errflag_offset,ax           ;save it

;
;Save and replace all required interrupt vectors.
;
	mov     al,08h
	mov     dx,offset timer
	mov     di,offset their_8h
	call    intset

	mov     al,10h
	mov     dx,offset video
	mov     di,offset their_10h
	call    intset

	mov     al,13h
	mov     dx,offset my_13
	mov     di,offset their_13h
	call    intset

	mov     al,28h
	mov     dx,offset my_28
	mov     di,offset their_28h
	call    intset

	mov     al,21h
	mov     dx,offset my_21
	mov     di,offset their_21h
	call    intset

	mov     ax,1
	ret
start_back      endp

	public  stop_back
stop_back       proc
	mov     cs:program_status,1

	mov     al,08h
	mov     di,offset their_8h
	call    intreset

	mov     al,10h
	mov     di,offset their_10h
	call    intreset

	mov     al,13h
	mov     di,offset their_13h
	call    intreset

	mov     al,28h
	mov     di,offset their_28h
	call    intreset

	mov     al,21h
	mov     di,offset their_21h
	call    intreset

	mov     cs:program_status,0

	mov dx,ds
	mov ax,offset tick_counter
	ret
stop_back       endp

	end
