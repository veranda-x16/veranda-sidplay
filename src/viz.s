; viz.s -- a scope's picture: one voice's waveform as the card's registers
; describe it (sidplay.s's draw_scopes and scope_sample: the phase steps
; with the frequency a sample and carries on, so the wave moves; the
; waveform from the control register; the height from the envelope), drawn
; as a trace into scope_buf, its frame and a dotted middle line included.
;
; scope_buf: 40 rows of a header byte (128 + 24, as a GEOS compacted
; bitmap's row: WBitmapUp could draw it) and 24 bytes; scope_blit puts them
; on the screen.
;
; Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
; GPL-3.0-or-later (see COPYING).

.export _scope_render, _curve_render, _scope_blit, _scope_buf, _sc_level, _sc_ctrl, _sc_pw, _sc_step
.export _sc_tab, _blit_rows

W       = 24            ; bytes a row
S       = W + 1         ; with its header
H       = 40            ; rows
MID     = 20
N       = 95            ; samples, 2 pixels apart from x 2

.segment "ZEROPAGE"
pp:     .res 2          ; a row's address
ph:     .res 2          ; the phase
yy:     .res 1
py:     .res 1          ; the sample before's row
xx:     .res 1
t0:     .res 1
t1:     .res 1
sn:     .res 1

.segment "BSS"
_scope_buf: .res S * H
_sc_level:  .res 1      ; the height: 0-17 pixels from the middle
_sc_ctrl:   .res 1
_sc_pw:     .res 1      ; the pulse width's top 8 bits
_sc_step:   .res 2
phase:      .res 6      ; each voice's
lfsr:       .res 1
cnt:        .res 1
mask:       .res 1
bx:         .res 1
src:        .res 1      ; 0 the waveform, 1 the table
idx:        .res 1
_sc_tab:    .res N      ; curve_render's rows, a sample each
_blit_rows: .res 1      ; scope_blit's rows (0: H)

.segment "CODE"

; void curve_render(void): a curve from sc_tab (95 rows, a sample each, 2
; pixels apart from x 2) in the same frame, without the middle line (the
; filter's response)
_curve_render:
	lda #1
	sta src
	stz sn
	bra start

; void scope_render(unsigned char v)
_scope_render:
	stz src
	asl a
	sta sn                  ; (2 v)
	tax
	lda phase,x
	sta ph
	lda phase+1,x
	sta ph+1
start:	; the empty picture: the frame, the dotted middle
	lda #<_scope_buf
	sta pp
	lda #>_scope_buf
	sta pp+1
	ldx #0
row:	ldy #0
	lda #$80 + W
	sta (pp),y
	lda #$FF
	cpx #0
	beq fill
	cpx #H - 1
	beq fill
	lda #0
	cpx #MID
	bne fill
	ldy src
	bne fill
	lda #$22
fill:	ldy #W
:	sta (pp),y
	dey
	bne :-
	cpx #0
	beq nx
	cpx #H - 1
	beq nx
	ldy #1
	lda (pp),y
	ora #$80
	sta (pp),y
	ldy #W
	lda (pp),y
	ora #$01
	sta (pp),y
nx:	clc
	lda pp
	adc #S
	sta pp
	bcc :+
	inc pp+1
:	inx
	cpx #H
	bne row
	; the trace
	lda #MID
	sta py
	lda #2
	sta xx
	lda #N
	sta cnt
	stz idx
	lda src
	beq smp
	lda _sc_tab
	sta py
smp:	lda src
	beq wave
	ldx idx
	lda _sc_tab,x
	inc idx
	bra have
wave:	clc
	lda ph
	adc _sc_step
	sta ph
	lda ph+1
	adc _sc_step+1
	sta ph+1
	jsr sample              ; a = the row
have:	sta yy
	; this column's byte and bit (x even: x and x + 1 share the byte)
	lda xx
	lsr a
	lsr a
	lsr a
	inc a                   ; (the header)
	sta bx
	lda xx
	and #7
	tax
	lda bits,x
	sta mask
	; noise: dots; the others: the column from the row before to this one
	lda src
	bne span0
	lda _sc_ctrl
	bmi dot
span0:
	lda py
	cmp yy
	bcc down
	lda yy                  ; up: rows yy to py
	sta t1
	lda py
	bra span
down:	lda py                  ; down: rows py to yy
	sta t1
	lda yy
span:	sec
	sbc t1
	sta t0                  ; rows - 1
	ldy t1
	lda rowlo,y
	clc
	adc bx
	sta pp
	lda rowhi,y
	adc #0
	sta pp+1
	ldx t0
col:	lda (pp)
	ora mask
	sta (pp)
	clc
	lda pp
	adc #S
	sta pp
	bcc :+
	inc pp+1
:	dex
	bpl col
dot:	ldy yy                  ; x and x + 1 at the row
	lda rowlo,y
	clc
	adc bx
	sta pp
	lda rowhi,y
	adc #0
	sta pp+1
	lda mask
	lsr a
	ora mask
	ora (pp)
	sta (pp)
	lda yy
	sta py
	inc xx
	inc xx
	dec cnt
	beq done
	jmp smp
done:	lda src
	bne :+
	ldx sn
	lda ph
	sta phase,x
	lda ph+1
	sta phase+1,x
:	rts

;---------------------------------------------------------------
; void scope_blit(unsigned int vram): scope_buf's 40 rows of 24 bytes into
; the foreground bitmap (VRAM $00000, 80 bytes a row) from address vram
; (the scope's first row and card): what WriteRow does row by row (2,400
; cycles a row), at about 330. Veranda's interrupt keeps VERA's address
; registers and CTRL (kernel/res/irq.s), so the main loop may use them;
; the kernel sets them again at each call.
;---------------------------------------------------------------
VERA_ADDR_L = $9F20
VERA_ADDR_M = $9F21
VERA_ADDR_H = $9F22
VERA_DATA0  = $9F23
VERA_CTRL   = $9F25

_scope_blit:
	sta t0                  ; the address
	stx t1
	lda #<(_scope_buf + 1)
	sta pp
	lda #>(_scope_buf + 1)
	sta pp+1
	lda _blit_rows
	bne :+
	lda #H
:	sta cnt
	stz VERA_CTRL
blrow:	lda t0
	sta VERA_ADDR_L
	lda t1
	sta VERA_ADDR_M
	lda #$10                ; increment 1, VRAM $0xxxx
	sta VERA_ADDR_H
	ldy #0
:	lda (pp),y
	sta VERA_DATA0
	iny
	cpy #W
	bne :-
	clc
	lda pp
	adc #S
	sta pp
	bcc :+
	inc pp+1
:	clc
	lda t0
	adc #80
	sta t0
	bcc :+
	inc t1
:	dec cnt
	bne blrow
	rts

; a = the sample's row, from ph+1, _sc_ctrl and _sc_level
sample:	lda _sc_level
	bne :+
	lda #MID
	rts
:	lda _sc_ctrl
	bit #$80
	beq pulse
	lda lfsr                ; noise
	bne :+
	lda #$A5
:	lsr a
	bcc :+
	eor #$B8
:	sta lfsr
	bra amp
pulse:	bit #$40
	beq saw
	lda ph+1
	cmp _sc_pw
	bcc :+
	lda #$10
	bra amp
:	lda #$F0
	bra amp
saw:	bit #$20
	beq tri
	lda ph+1
	bra amp
tri:	lda ph+1
	bpl :+
	eor #$FF
:	asl a
amp:	; s = a - 128; row = MID - s * level / 128
	sec
	sbc #$80
	sta t0                  ; (signed)
	bpl :+
	eor #$FF
	inc a
:	sta t1                  ; |s|, 0-128
	; |s| * level (8 x 8 bits: a high, yy low), then / 128
	lda _sc_level
	sta yy
	lda #0
	ldx #8
	lsr yy
mul:	bcc :+
	clc
	adc t1
:	ror a
	ror yy
	dex
	bne mul
	asl yy
	rol a                   ; a = |s| * level / 128 (0-17)
	sta t1
	lda t0
	bmi neg
	lda #MID
	sec
	sbc t1
	rts
neg:	lda #MID
	clc
	adc t1
	rts

.segment "RODATA"
bits:	.byte $80, $40, $20, $10, $08, $04, $02, $01
rowlo:
.repeat H, I
	.byte <(_scope_buf + I * S)
.endrep
rowhi:
.repeat H, I
	.byte >(_scope_buf + I * S)
.endrep
