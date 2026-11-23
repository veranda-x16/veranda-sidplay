; Copied from the CommanderSID project (software/sid-tunes/testtune.s, commit 03007ec): CC0.
; X16-SID test tune, our own (HVSC tunes are copyrighted and not committed): a PSID v2 file.
;   song 1: VBI (PAL 50 Hz), voice 1 triangle, A4 - C#5 - E5, a new note every 10 frames (199.5 ms)
;   song 2: CIA timer at 100 Hz, G4 - D5, a new note every 25 calls (246 ms)
; Frequencies for the PAL clock (985248 Hz): Fn = f * 2^24 / 985248.

        .segment "HEADER"
        .byte "PSID"
        .dbyt 2                 ; version
        .dbyt $7C               ; data offset
        .dbyt $1000             ; load address
        .dbyt init
        .dbyt play
        .dbyt 2                 ; songs
        .dbyt 1                 ; start song
        .dbyt 0, $0002          ; speed: song 2 uses the CIA timer
name:   .byte "X16-SID test tune"
        .res 32 - (* - name)
author: .byte "X16-SID project"
        .res 32 - (* - author)
rel:    .byte "2026 CC0"
        .res 32 - (* - rel)
        .dbyt %0000000000010100 ; flags: PAL, 6581
        .byte 0, 0, 0, 0        ; start page, page length, 2nd/3rd SID

        .segment "CODE"
song    = $FB
step    = $FC
count   = $FD

init:   sta song
        lda #$0F
        sta $D418               ; volume
        lda #$00
        sta $D405               ; attack 2 ms, decay 6 ms
        lda #$F0
        sta $D406               ; sustain 15
        lda #$11
        sta $D404               ; triangle, gate on
        lda #0
        sta step
        lda song
        beq @vbi
        lda #<9851              ; CIA timer A latch: 985248 / 100 - 1
        sta $DC04
        lda #>9851
        sta $DC05
@vbi:   jsr note
        rts

play:   dec count
        bne @out
        inc step
        jsr note
@out:   rts

; set voice 1 to the note `step` of the current song and reload `count`
note:   ldx song
        lda len,x
        sta count
        lda step
        cmp notes_n,x
        bcc :+
        lda #0
        sta step
:       clc
        adc notes_at,x
        asl a
        tay
        lda freqs,y
        sta $D400
        lda freqs+1,y
        sta $D401
        rts

len:      .byte 10, 25
notes_n:  .byte 3, 2
notes_at: .byte 0, 3
freqs:    .word 7493, 9440, 11226      ; A4, C#5, E5
          .word 6675, 10001            ; G4, D5
