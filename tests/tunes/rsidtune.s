; Copied from the CommanderSID project (software/sid-tunes/rsidtune.s, commit 03007ec): CC0.
; X16-SID RSID test tune, our own (CC0): needs a complete C64, so the card's C64 mode plays it.
;   song 1: a CIA1 timer A interrupt at 100 Hz through the KERNAL's $0314 vector (it then goes on
;           to the KERNAL's own handler at $EA31): voice 1 triangle, C5 - E5 - G5, a new note every
;           10 interrupts (100 ms)
;   song 2: a CIA2 timer A NMI at about 4 kHz through $0318 that flips $D418 between volume 15 and
;           0, as sample players do (a square wave from the volume register)
; Frequencies for the PAL clock (985248 Hz): Fn = f * 2^24 / 985248.

        .segment "HEADER"
        .byte "RSID"
        .dbyt 2                 ; version
        .dbyt $7C               ; data offset
        .dbyt $1000             ; load address
        .dbyt init
        .dbyt 0                 ; play: 0 (RSID)
        .dbyt 2                 ; songs
        .dbyt 1                 ; start song
        .dbyt 0, 0              ; speed: 0 (RSID)
name:   .byte "X16-SID RSID test"
        .res 32 - (* - name)
author: .byte "X16-SID project"
        .res 32 - (* - author)
rel:    .byte "2026 CC0"
        .res 32 - (* - rel)
        .dbyt %0000000000010100 ; flags: PAL, 6581
        .byte 0, 0, 0, 0

        .segment "CODE"
step    = $FB
count   = $FC
vol     = $FD

init:   sei
        cmp #1
        beq digi
        lda #$0F
        sta $D418
        lda #$00
        sta $D405               ; attack 2 ms, decay 6 ms
        lda #$F0
        sta $D406               ; sustain 15
        lda #$11
        sta $D404               ; triangle, gate
        lda #0
        sta step
        jsr note
        lda #<irq
        sta $0314
        lda #>irq
        sta $0315
        lda #<9851              ; CIA1 timer A: 985248 / 100 - 1
        sta $DC04
        lda #>9851
        sta $DC05
        lda #$11                ; start, force load, continuous
        sta $DC0E
        cli
        rts

irq:    dec count
        bne :+
        inc step
        jsr note
:       jmp $EA31               ; the KERNAL's handler acknowledges CIA1 and returns

note:   lda #10
        sta count
        ldx step
        cpx #3
        bcc :+
        ldx #0
        stx step
:       txa
        asl a
        tax
        lda freqs,x
        sta $D400
        lda freqs+1,x
        sta $D401
        rts

digi:   lda #<nmi
        sta $0318
        lda #>nmi
        sta $0319
        lda #$0F
        sta vol
        sta $D418
        lda #<245               ; CIA2 timer A: 985248 / 246 = 4005 Hz
        sta $DD04
        lda #>245
        sta $DD05
        lda #$81                ; timer A NMI on
        sta $DD0D
        lda #$11
        sta $DD0E
        cli
        rts

nmi:    pha                     ; (the KERNAL's NMI entry did SEI, JMP ($0318))
        lda $DD0D               ; acknowledge
        lda vol
        eor #$0F
        sta vol
        sta $D418
        pla
        rti

freqs:  .word 8915, 11231, 13357        ; C5, E5, G5
