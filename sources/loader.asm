  DEVICE NOSLOT64K ; DEVICE ZX81
  SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION ; for DeZog

  ORG $4009 
; VERSN
startup: 
  nop
  ld h, d ; 40h
  jr init1    
  db $F9, $D4 ; basic instruction to start
  db $1C, $7E 
  db $8F      
  db 0        
  db $12      

; E_LINE
  dw eline 
  dup(11) : nop : edup

; NOTUSED
init1:
  out ($FD), a ; avoid NMI interruption (call SET_FAST)
  jr init2			

; LAST_K
  dup(3) : db $FF : edup
;MARGIN
  db 37
; NXTLIN
  dw startup

; OLDPPC  
init2:
  ld de, load_loop ; de is pointing to target "phased" address
  ld l, RELOCATE_START mod 256

; T_ADDR
  add a,a : inc c

; SEED
  ld bc, (RELOCATE_END - RELOCATE_START) +$100 ; +$100 since frames will be decremented
  ldir ; relocate loader (with stack)
  ld hl, stack_pos

  db 62 ; ld a, CDFLAG
; CDFLAG
  db %01000000
  assert( $==$403c ) ; end of rom sysvars

  ld sp, hl ; sp to prepared stack with return addresses

  ; optional block to setup initial values for all registers
  ld bc,$4242 ; bc' value
  ld de,$4242 ; de' value
  ld hl,$4242 ; hl' value
  exx
  pop de ; load_loop, will be put on stack to jump back and continue loading until the end
  ld hl,$4242
  push hl
  pop af ; a' and f' values
  ex af,af'
  ld ix, $0281 ; video routine
  ld iy, $4000 ; sysvars pointer
  ld a, $1E 
  ld i, a ; low-res characters set

  ld hl, 0x4000 ; start loading of snapshot tape data at 4000h

  ; probably this 9 bytes can be replaced with a ret to the load_loop jp with some audio adaptation (as a small gap), but reliability to be tested and very minor loader improvement
wait_high_signal:
  in a, ($FE)
  rla 
  jp c, $34C ; IN_BYTE (return will be at load_loop)
  jr wait_high_signal

; this block will be relocated at the end of the memory
relocate_lenght = 12
RELOCATE_START: 
  phase $4400 - relocate_lenght ; relocated to the end of memory
load_loop:
    ld (hl), c ; this instruction will be overwritten by ret at the end of the load
    inc hl
    push de ; prepare return to load_loop
    jp $34C ; IN_BYTE ROM routine
stack_pos:
    dw load_loop ; filler for IN_BYTE routine stack (push DE of GET_BIT)
    dw load_loop ; IN_BYTE return to load_loop
    dw $4036 ; start address after load ($4036 is the smallest useful space in sysvars)
    ; The loaded code must:
    ; 1) Load with system variables intact (D_FILE and friends must be valid)
    ; 2) Contain a "ret" instruction exactly at $43F4 (end of max loadable area)
    ; 3) Have at least one extra unused byte at $43F5
    ; 4) On entry:
    ;    - switch back to SLOW mode: out ($FE),a
    ;    - perform "exx" if alternate regs are meant to be used
    ; Register state on entry:
    ;   PC = user selected value
    ;   SP = $4400
    ;   A  = loader-dependent
    ;   F  = 00101001b
    ;   B  = 0
    ;   C  = last byte loaded (56=38h in the example program)
    ;   DE = $43F4
    ;   HL = $43F5
    ;   AF', BC', DE', HL' = user-selected values
    ;   IX = $0281, IY = $4000, ROM defaults (for video and sysvars access) or user selected values
    ;   I = $1E ROM default (for video characters) or selected value
relocated_end:
  dephase
RELOCATE_END:
  assert (RELOCATE_END-RELOCATE_START)==relocate_lenght
  assert relocated_end==$4400

  db $80 ; end of basic vars
eline: 
