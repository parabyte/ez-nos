; 3C501 (Ethernet) interrupt hooks

;%    .MODEL  MEMMOD,C
include asmgloba.h        
	LOCALS
	%MACS
	.LALL

	extrn   Stktop,Spsave,Sssave,ecint:proc,doret:proc,eoi:proc

	.CODE
dbase   dw      @Data           ; save loc for ds (must be in code segment)

; ec0vec - Ethernet interrupt handler
	public  ec0vec
	label   ec0vec far
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

	mov     ax,0            ; arg for service routine
	push    ax
	call    ecint
	pop     ax
	jmp     doret

; ec1vec - Ethernet interrupt handler
	public  ec1vec
	label   ec1vec far
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

	mov     ax,1            ; arg for service routine
	push    ax
	call    ecint
	pop     ax
	jmp     doret

; ec2vec - Ethernet interrupt handler
	public  ec2vec
	label   ec2vec far
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
	call    ecint
	pop     ax
	jmp     doret

	end
