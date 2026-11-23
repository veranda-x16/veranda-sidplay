; A known register shadow (Veranda SID Player's gate, tests/check_ui.py): a
; PSID v2 whose init writes every SID register once and whose play routine
; does nothing, so the card's shadow holds these values while it plays.
;   voice 1  A-4 ($1D45) pulse $800 + gate, ADSR $09/$A0
;   voice 2  C-5 ($22CE) saw + sync + gate, pulse $400, ADSR $22/$F4
;   voice 3  E-5 ($2BDA) noise + gate, pulse $C00, ADSR $00/$F0
;   filter   cutoff $7FF, resonance $F, voices 1 and 2, low + band pass, volume $F
; CC0 (written for the Veranda SID Player).
        .segment "HEADER"
        .byte "PSID"
        .dbyt 2
        .dbyt $7C
        .dbyt $1000
        .dbyt init
        .dbyt play
        .dbyt 1
        .dbyt 1
        .dbyt 0, 0
name:   .byte "Known shadow"
        .res 32 - (* - name)
author: .byte "Veranda SID Player gate"
        .res 32 - (* - author)
rel:    .byte "2026 CC0"
        .res 32 - (* - rel)
        .dbyt %0000000000010100 ; PAL, 6581
        .byte 0, 0, 0, 0

        .segment "CODE"
init:   ldx #0
:       lda regs,x
        sta $D400,x
        inx
        cpx #25
        bne :-
play:   rts

regs:   .byte $45, $1D, $00, $08, $41, $09, $A0
        .byte $CE, $22, $00, $04, $23, $22, $F4
        .byte $DA, $2B, $00, $0C, $81, $00, $F0
        .byte $07, $FF, $F3, $3F
