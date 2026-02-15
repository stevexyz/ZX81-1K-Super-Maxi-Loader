 DEVICE NOSLOT64K ; DEVICE ZX81
 SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION ; advanced DeZog capabilities
 ORG $4000

ERR_NR  db $00       ; usable
FLAGS   db $00       ; usable
ERR_SP  dw $0000     ; usable
RAMTOP  dw $0000     ; usable
MODE    db $00       ; usable
PPC     dw $0000     ; usable
VERSN   db $00       ; usable
E_PPC   dw $0000     ; usable
D_FILE  dw dfile     ; !! not usable (display management)
DF_CC   dw $0000     ; usable if not using rst 10h or similar
VARS    dw $0000     ; usable if not using basic vars
DEST    dw $0000     ; usable
E_LINE  dw $0000     ; usable
CH_ADD  dw $0000     ; usable
X_PTR   dw $0000     ; usable
STKBOT  dw $0000     ; usable
STKEND  dw $0000     ; usable
BREG    db $00       ; usable if not using floating point
MEM     dw $0000     ; usable
NOTUSED db $00       ; usable
DF_SZ   db $00       ; usable
S_TOP   dw $0000     ; usable
LAST_K  dw $FFFF     ; !! not usable (row/column of last char pressed)
DB_ST   db $FF       ; !! not usable (debounce)
MARGIN  db $37       ; usable? (display management)
NXTLIN  dw $0000     ; usable
OLDPPC  dw $0000     ; usable
FLAGX   db $00       ; usable
STRLEN  dw $0000     ; usable
T_ADDR  dw $0000     ; usable
SEED    dw $0000     ; usable
FRAMES  dw $FFFF     ; !! not usable (display management)
;---
; this part is used for the "loop forever example program"
;COORDS  dw $0000     ; usable
;PR_CC   db $00       ; usable
;S_POSN  dw $0000     ; usable
  out ($FE), a ; back to slow mode showing display
loopforever:
  jp loopforever
;---
CDFLAG  db %01000000 ; !! not usable (fast/slow flags)
 assert($==$403c) ; let's see if rom sysvars are ending correctly!

 INCLUDE "zx81_charset.inc" ; ZX81 is not ascii based
dfile: ; collapsed display for 1k
  halt ; sync screen output
  db _cH,_cE,_cL,_cL,_cO, _NL ; *HELLO* (in standard and inverted colors)
  db _iW,_iO,_iR,_iL,_iD, _NL ; *WORLD* (no exclamation point available on ZX81!)
  jp (hl) ; fill blank the the rest of the screen

  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "this is a wasted memory space that can be used"
  db "other free bytes!!"

;---------------------------------------------------------
; keep below line to have ret instruction properly placed 
 dup ($43F4-$) : nop : edup : ret : db _cS
;---------------------------------------------------------
