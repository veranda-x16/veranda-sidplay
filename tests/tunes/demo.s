; A short demo tune for the Veranda SID Player's pictures and gates: four
; bars (C, Am, F, G), a pulse bass with its width sweeping and the filter
; on it, a saw arpeggio, a noise hi-hat. Song 2: faster, the arpeggio a
; pulse. CC0 (written for the Veranda SID Player).
; Frequencies for the PAL clock (985248 Hz): Fn = f * 2^24 / 985248.
        .segment "HEADER"
        .byte "PSID"
        .dbyt 2
        .dbyt $7C
        .dbyt $1000
        .dbyt init
        .dbyt play
        .dbyt 2
        .dbyt 1
        .dbyt 0, 0
name:   .byte "Veranda Demo"
        .res 32 - (* - name)
author: .byte "Veranda SID Player"
        .res 32 - (* - author)
rel:    .byte "2026 CC0"
        .res 32 - (* - rel)
        .dbyt %0000000000010100 ; PAL, 6581
        .byte 0, 0, 0, 0

        .segment "CODE"
tick    = $F0                   ; ticks into the step
step    = $F1                   ; 0-63: bar * 16 + step
arp     = $F2
pw      = $F3                   ; 2
cut     = $F5
cutd    = $F6
speed   = $F7
wave2   = $F8

init:   ldx #6
        cmp #1
        bne :+
        ldx #4
:       stx speed
        ldx #$21
        cmp #1
        bne :+
        ldx #$41
:       stx wave2
        lda #0
        sta tick
        sta step
        sta arp
        sta pw
        lda #4
        sta pw+1
        lda #$20
        sta cut
        lda #1
        sta cutd
        lda #$09                ; voice 1: bass
        sta $D405
        lda #$A9
        sta $D406
        lda #$00                ; voice 2: arpeggio
        sta $D40C
        lda #$C0
        sta $D40D
        lda #$00
        sta $D409
        lda #$08
        sta $D40A
        lda #$02                ; voice 3: hi-hat
        sta $D413
        lda #$00
        sta $D414
        lda #$80
        sta $D40E
        lda #$70
        sta $D40F
        lda #$00
        sta $D415
        lda #$81                ; resonance 8, voice 1
        sta $D417
        lda #$1F                ; low pass, volume 15
        sta $D418
        rts

play:   ; the pulse width and the filter sweep every tick
        clc
        lda pw
        adc #24
        sta pw
        lda pw+1
        adc #0
        and #$0F
        sta pw+1
        lda pw
        sta $D402
        lda pw+1
        sta $D403
        lda cut
        clc
        adc cutd
        sta cut
        cmp #$90
        bcc :+
        lda #$FF
        sta cutd
:       lda cut
        cmp #$18
        bcs :+
        lda #1
        sta cutd
:       lda cut
        sta $D416
        ; the arpeggio: a chord note every 2 ticks
        lda tick
        and #1
        bne @noarp
        ldx arp
        inx
        cpx #3
        bcc :+
        ldx #0
:       stx arp
        lda step
        lsr a
        lsr a
        lsr a
        lsr a                   ; the bar
        sta $F9
        asl a
        adc $F9                 ; bar * 3
        adc arp
        asl a
        tax
        lda chords,x
        sta $D407
        lda chords+1,x
        sta $D408
        lda wave2
        sta $D40B
@noarp: lda tick
        bne @mid
        ; a new step: the bass, the hi-hat
        lda step
        lsr a
        lsr a
        lsr a
        lsr a
        asl a
        tax
        lda step
        and #3
        cmp #2
        bne :+
        inx                     ; (an octave up on the third of four)
        inx
        inx
        inx
        inx
        inx
        inx
        inx
:       lda bass,x
        sta $D400
        lda bass+1,x
        sta $D401
        lda #$41
        sta $D404
        lda step
        and #1
        beq :+
        lda #$81
        sta $D412
:       jmp @next
@mid:   cmp #2
        bne @next
        lda #$40                ; the bass's gate off, the hat's
        sta $D404
        lda #$80
        sta $D412
@next:  inc tick
        lda tick
        cmp speed
        bcc @out
        lda #0
        sta tick
        inc step
        lda step
        and #63
        sta step
@out:   rts

; the bass: C2, A1, F2, G2, then an octave up
bass:   .word 1114, 937, 1487, 1669
        .word 2227, 1873, 2974, 3338
; the chords: C E G, A C E, F A C, G B D
chords: .word 4455, 5613, 6675
        .word 3746, 4455, 5613
        .word 5947, 7493, 8910
        .word 6675, 8410, 10001
