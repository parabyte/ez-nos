; ASY (8250/16450/16550A "comm" port) interrupt hooks
; Copyright 1991 Phil Karn, KA9Q

;%    .MODEL  MEMMOD,C
include asmgloba.h        
	%MACS

	extrn   doretch:proc,asyint:far,eoi:proc

	.DATA
	extrn   Sssave:word
	extrn   Spsave:word
	extrn   Stktop

	.CODE
dbase   dw      @Data

; asy0vec - asynch channel 0 interrupt handler
	public  asy0vec
	label   asy0vec far
	push    ds              ; save on user stack
	mov     ds,cs:dbase     ; establish interrupt data segment

	mov     Sssave,ss       ; stash user stack context
	mov     Spsave,sp

	mov     ss,cs:dbase
	lea     sp,Stktop

      ;  push    ax              ; save user regs on interrupt stack
      ;  push    bx
      ;  push    cx
      ;  push    dx
      ;  push    bp
      ;  push    si
      ;  push    di
	PUSHALL
	push    es
	call    eoi

	mov     ax,0            ; arg for service routine
	push    ax
	call    asyint
	inc	sp
	inc	sp
	jmp     doretch

; asy1vec - asynch channel 1 interrupt handler
	public  asy1vec
	label   asy1vec far
	push    ds              ; save on user stack
	mov     ds,cs:dbase     ; establish interrupt data segment

	mov     Sssave,ss       ; stash user stack context
	mov     Spsave,sp

	mov     ss,cs:dbase
	lea     sp,Stktop

      ;  push    ax              ; save user regs on interrupt stack
      ;  push    bx
      ;  push    cx
      ;  push    dx
      ;  push    bp
      ;  push    si
      ;  push    di
	PUSHALL
	push    es
	call    eoi

	mov     ax,1            ; arg for service routine
	push    ax
	call    asyint
	inc	sp
	inc	sp
	jmp     doretch

; asy2vec - asynch channel 2 interrupt handler
	public  asy2vec
	label   asy2vec far
	push    ds              ; save on user stack
	mov     ds,cs:dbase     ; establish interrupt data segment

	mov     Sssave,ss       ; stash user stack context
	mov     Spsave,sp

	mov     ss,cs:dbase
	lea     sp,Stktop

       ; push    ax              ; save user regs on interrupt stack
       ; push    bx
       ; push    cx
       ; push    dx
       ; push    bp
       ; push    si
       ; push    di
	PUSHALL
	push    es
	call    eoi

	mov     ax,2            ; arg for service routine
	push    ax
	call    asyint
	inc	sp
	inc	sp
	jmp     doretch

; asy3vec - asynch channel 3 interrupt handler
	public  asy3vec
	label   asy3vec far
	push    ds              ; save on user stack
	mov     ds,cs:dbase     ; establish interrupt data segment

	mov     Sssave,ss       ; stash user stack context
	mov     Spsave,sp

	mov     ss,cs:dbase
	lea     sp,Stktop

       ; push    ax              ; save user regs on interrupt stack
       ; push    bx
       ; push    cx
       ; push    dx
       ; push    bp
       ; push    si
       ; push    di
	PUSHALL
	push    es
	call    eoi

	mov     ax,3            ; arg for service routine
	push    ax
	call    asyint
	inc	sp
	inc	sp
	jmp     doretch

; asy4vec - asynch channel 4 interrupt handler
	public  asy4vec
	label   asy4vec far
	push    ds              ; save on user stack
	mov     ds,cs:dbase     ; establish interrupt data segment

	mov     Sssave,ss       ; stash user stack context
	mov     Spsave,sp

	mov     ss,cs:dbase
	lea     sp,Stktop

       ; push    ax              ; save user regs on interrupt stack
       ; push    bx
       ; push    cx
       ; push    dx
       ; push    bp
       ; push    si
       ; push    di
	PUSHALL
	push    es
	call    eoi

	mov     ax,4            ; arg for service routine
	push    ax
	call    asyint
	inc	sp
	inc	sp
	jmp     doretch

; asy5vec - asynch channel 5 interrupt handler
	public  asy5vec
	label   asy5vec far
	push    ds              ; save on user stack
	mov     ds,cs:dbase     ; establish interrupt data segment

	mov     Sssave,ss       ; stash user stack context
	mov     Spsave,sp

	mov     ss,cs:dbase
	lea     sp,Stktop

       ; push    ax              ; save user regs on interrupt stack
       ; push    bx
       ; push    cx
       ; push    dx
       ; push    bp
       ; push    si
       ; push    di
	PUSHALL
	push    es
	call    eoi

	mov     ax,5            ; arg for service routine
	push    ax
	call    asyint
	inc	sp
	inc	sp
	jmp     doretch

	end
