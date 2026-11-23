; card.s -- the X16-SID Rev B card: its window, its mailbox and the
; interrupt's part of the player (sidp.h says what C sees).
;
; Ported from the CommanderSID project's software/common/mbox.inc and
; software/rev-b/sidplay.s (commit 03007ec, with the uncommitted changes as
; of 2026-11-16): the same frames (LEN_LO LEN_HI CMD PAYLOAD CRC-8, poly $07
; over CMD and the payload), the same timeouts, the same ARM lease.
;
; The card sits at IO5 ($9FA0) or IO6 ($9FC0), a jumper on the card. Every
; instruction that touches it is an absolute address on IO5 that
; card_probe patches to the window being tried (the CARD macro keeps a
; list of them in this file's RODATA): no indexed stores, whose dummy cycle
; could read (and so pop) the mailbox's data register.
;
; Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
; GPL-3.0-or-later (see COPYING).

.export _card_probe, _card_reset, _card_release, _card_request, _card_store
.export _card_snap, _card_irq, _card_base
.export _card_req, _card_rx, _sid, _card_status
.export _env_hi, _irq_frames, _el_jif, _el_sec, _irq_play
.export _card_keep_on, _irq_word

RAM_BANK = $00

.segment "ZEROPAGE"     ; (the linker places it beside cc65's own)
ptr:    .res 2          ; payload
cnt:    .res 2
crc:    .res 1
tmo:    .res 3          ; timeout

.segment "RODATA"
patches:                ; (only the CARD macro's words go here)

; (a symbol defined in a macro ends ca65's cheap local scope: this file
; uses no patches_labels)
.macro CARD op, reg
	.local L
L:	op $9FA0 + reg
	.pushseg
	.segment "RODATA"
	.word L + 1
	.popseg
.endmacro
.macro CARDX op, reg
	.local L
L:	op $9FA0 + reg, x
	.pushseg
	.segment "RODATA"
	.word L + 1
	.popseg
.endmacro
.macro CARDY op, reg
	.local L
L:	op $9FA0 + reg, y
	.pushseg
	.segment "RODATA"
	.word L + 1
	.popseg
.endmacro

MB      = $1D           ; the mailbox's data register
STATUS  = $1F           ; status (read), commands (write)
CMD_ARM    = $84
CMD_DISARM = $85
CMD_RESET  = $80
ST_RX   = $04

.segment "BSS"
_card_base:   .res 1    ; the window's low byte: $A0 (IO5), $C0 (IO6); 0: none
_card_req:    .res 16   ; a request's payload
_card_rx:     .res 128  ; the last reply: LEN_LO LEN_HI CMD payload ...
_sid:         .res 29   ; the registers $00-$1C as the card shows them
_card_status: .res 1
_card_keep_on: .res 1   ; the interrupt renews the ARM
_irq_frames:  .res 2    ; frames (the interrupt's count)
_irq_play:    .res 1    ; 1 playing, 2 paused: the interrupt counts the time while 1
_el_jif:      .res 1    ; the song's time: jiffies, seconds
_el_sec:      .res 2
_env_hi:      .res 3    ; the envelopes (8.8 fixed point, sidplay.s's model)
env_lo:       .res 3
env_st:       .res 3
env_gate:     .res 3
irq_t:        .res 1
rxi:          .res 1

.segment "DATA"
cur:          .byte $A0 ; the low byte the code is patched to now

.segment "CODE"

; the patched instructions' low bytes from cur to a
setbase:
	sta irq_t
	sec
	sbc cur
	beq setbase_r
	sta crc                 ; (the difference)
	ldx #0
setbase_l:	cpx #<(patches_end - patches)
	beq setbase_d
	lda patches,x
	sta ptr
	lda patches+1,x
	sta ptr+1
	lda (ptr)
	clc
	adc crc
	sta (ptr)
	inx
	inx
	bra setbase_l
setbase_d:	lda irq_t
	sta cur
setbase_r:	rts

;---------------------------------------------------------------
; unsigned char card_probe(unsigned char lo): the window $9F00 + lo
; (lo $A0 or $C0) armed, then up to about 0.1 s for the card's signature
; in STATUS (%101xxxxx). 1: a Rev B card answers (the code stays patched
; to it); 0: none (open bus is $9F, the emulator's $9FBF '6').
;---------------------------------------------------------------
_card_probe:
	php
	sei                     ; (the interrupt's ARM must not go to a window being tried)
	stz _card_keep_on
	jsr setbase
	plp
	lda #CMD_ARM
	CARD sta, STATUS
	ldy #0
	ldx #0
card_probe_w:	CARD lda, STATUS
	and #$E0
	cmp #$A0
	beq card_probe_ok
	dex
	bne card_probe_w
	dey
	bne card_probe_w
	stz _card_base
	lda #0
	tax
	rts
card_probe_ok:	lda cur
	sta _card_base
	lda #1
	sta _card_keep_on
	ldx #0
	rts

; MB_RESET: nothing an earlier program left reaches this one; then ARM again
_card_reset:
	lda #CMD_RESET
	CARD sta, STATUS
	lda #CMD_ARM
	CARD sta, STATUS
	rts

; the lease let go (the card answers reads again after an ARM)
_card_release:
	stz _card_keep_on
	lda _card_base
	beq card_release_r
	lda #CMD_DISARM
	CARD sta, STATUS
card_release_r:	rts

;---------------------------------------------------------------
; the mailbox
;---------------------------------------------------------------
; a byte out, into the CRC (keeps x, y)
.macro OUTB
	CARD sta, MB
	phx
	eor crc
	tax
	lda crc8,x
	sta crc
	plx
.endmacro

; wait for RX data; c set after the timeout (about 0.3 s, three times
; mbox.inc's: replies that take long are the C64's start)
wait:	lda #3
	sta tmo+2
	stz tmo
	stz tmo+1
wait_w:	CARD lda, STATUS
	and #$E0 | ST_RX        ; the signature too: a disarmed card leaves the bus open
	cmp #$A0 | ST_RX
	beq wait_ok
	dec tmo
	bne wait_w
	dec tmo+1
	bne wait_w
	dec tmo+2
	bne wait_w
	sec
	rts
wait_ok:	clc
	rts

; one frame in: its first 128 bytes into card_rx; c set on a timeout
recv:	jsr wait
	bcs recv_out
	stz rxi
	jsr recv_byte
	sta cnt
	jsr recv_byte
	sta cnt+1
	clc                     ; CMD + CRC
	lda cnt
	adc #2
	sta cnt
	bcc recv_l
	inc cnt+1
recv_l:	lda cnt
	ora cnt+1
	beq recv_done
	jsr recv_byte
	lda cnt
	bne :+
	dec cnt+1
:	dec cnt
	bra recv_l
recv_done:	clc
recv_out:	rts
recv_byte:	CARD lda, MB
	ldy rxi
	bmi :+                  ; 128 kept: the rest is read and dropped
	sta _card_rx,y
	inc rxi
:	rts

;---------------------------------------------------------------
; unsigned char card_request(unsigned char cmd, unsigned char len):
; send cmd with len bytes of card_req, wait for its reply. 0: the reply
; (card_rx); 1: no answer; 2: another reply (an error)
;---------------------------------------------------------------
_card_request:
	sta cnt                 ; (len)
	jsr popa                ; cmd
	pha
	lda #CMD_ARM            ; every request renews the lease (CARD_KEEP)
	CARD sta, STATUS
	lda cnt
	CARD sta, MB
	lda #0
	CARD sta, MB
	stz crc
	pla
	pha
	OUTB
	ldy #0
card_request_l:	cpy cnt
	beq card_request_c
	lda _card_req,y
	OUTB
	iny
	bra card_request_l
card_request_c:	lda crc
	CARD sta, MB
	jsr recv
	pla
	bcs card_request_none
	ora #$80
	cmp _card_rx+2
	bne card_request_bad
	lda #0
	tax
	rts
card_request_none:	lda #1
	ldx #0
	rts
card_request_bad:	lda #2
	ldx #0
	rts

;---------------------------------------------------------------
; unsigned char card_store(unsigned char bank, unsigned int n):
; STORE ($06): the store offset in card_req[0..3], then n bytes from
; $A000 in RAM bank `bank`. 0: stored; else no answer or an error
;---------------------------------------------------------------
_card_store:
	sta cnt
	stx cnt+1
	jsr popa
	tay                     ; the bank
	lda RAM_BANK
	pha
	sty RAM_BANK
	lda #CMD_ARM
	CARD sta, STATUS
	clc                     ; LEN = n + 4
	lda cnt
	adc #4
	CARD sta, MB
	lda cnt+1
	adc #0
	CARD sta, MB
	stz crc
	lda #$06
	OUTB
	ldy #0
card_store_o:	lda _card_req,y
	OUTB
	iny
	cpy #4
	bne card_store_o
	stz ptr
	lda #$A0
	sta ptr+1
	ldy #0
card_store_d:	lda cnt
	ora cnt+1
	beq card_store_c
	lda (ptr),y
	OUTB
	iny
	bne :+
	inc ptr+1
:	lda cnt
	bne :+
	dec cnt+1
:	dec cnt
	bra card_store_d
card_store_c:	pla
	sta RAM_BANK
	lda crc
	CARD sta, MB
	jsr recv
	bcs card_store_bad
	lda _card_rx+2
	cmp #$86                ; STORE's reply
	bne card_store_bad
	lda #0
	tax
	rts
card_store_bad:	lda #1
	ldx #0
	rts

;---------------------------------------------------------------
; unsigned int irq_word(volatile unsigned int *w): a word the interrupt
; writes, both bytes from the same moment
;---------------------------------------------------------------
_irq_word:
	sta ptr
	stx ptr+1
	ldy #1
	php
	sei
	lda (ptr),y
	tax
	lda (ptr)
	plp
	rts

;---------------------------------------------------------------
; void card_snap(void): the registers $00-$1C (the card's shadow of what
; was written, and the SID's own OSC3/ENV3 read back) into sid[], STATUS
; into card_status. $1D is never read here: a read pops the mailbox.
;---------------------------------------------------------------
_card_snap:
	ldx #$1C
card_snap_l:	CARDX lda, 0
	sta _sid,x
	dex
	bpl card_snap_l
	CARD lda, STATUS
	sta _card_status
	rts

;---------------------------------------------------------------
; the interrupt: Veranda's intFirstVector (DECISIONS V41), first in every
; interrupt, before Veranda saves anything; at VSYNC: the frame count, the
; lease renewed, the song's time while it plays, the three envelopes. It
; touches neither VERA's address registers nor the zero page, maps no bank,
; and returns c clear (Veranda's own interrupt work goes on). (Not
; intTopVector: a dialog box clears that one while it is open, as GEOS
; does, and the lease would lapse under every box.)
;---------------------------------------------------------------
VERA_ISR = $9F27

_card_irq:
	lda VERA_ISR
	and #$01                ; VSYNC
	beq card_irq_out
	inc _irq_frames
	bne :+
	inc _irq_frames+1
:	lda _card_keep_on
	beq card_irq_out
	lda #CMD_ARM
	CARD sta, STATUS
	lda _irq_play
	cmp #1
	bne card_irq_e
	inc _el_jif
	lda _el_jif
	cmp #60
	bcc card_irq_e
	stz _el_jif
	inc _el_sec
	bne card_irq_e
	inc _el_sec+1
card_irq_e:
	jsr env_step
card_irq_out:
	clc
	rts

; the three envelopes one jiffy on, from the shadow registers: attack to
; $FF at the attack rate, decay to the sustain level, release to 0
; (sidplay.s's env_step; X = voice, Y = its registers' base)
env_step:
	ldx #0
	ldy #0
env_step_v:	CARDY lda, 4            ; control
	and #1
	beq env_step_release
	lda env_gate,x
	bne env_step_held
	lda #1                  ; gate on: attack
	sta env_gate,x
	sta env_st,x
env_step_held:	lda env_st,x
	cmp #1
	bne env_step_decay
	CARDY lda, 5            ; attack rate
	lsr a
	lsr a
	lsr a
	lsr a
	asl a
	phy
	tay
	clc
	lda env_lo,x
	adc env_attack,y
	sta env_lo,x
	lda _env_hi,x
	adc env_attack+1,y
	sta _env_hi,x
	ply
	bcs env_step_full
	cmp #$FF
	bcc env_step_next
env_step_full:	lda #$FF
	sta _env_hi,x
	stz env_lo,x
	lda #2
	sta env_st,x
	bra env_step_next
env_step_decay:	CARDY lda, 6            ; sustain level S * $11
	and #$F0
	sta irq_t
	lsr a
	lsr a
	lsr a
	lsr a
	ora irq_t
	sta irq_t
	lda _env_hi,x
	cmp irq_t
	bcc env_step_next
	beq env_step_next
	CARDY lda, 5
	and #$0F
	jsr env_step_down
	lda _env_hi,x
	cmp irq_t
	bcs env_step_next
	lda irq_t
	sta _env_hi,x
	stz env_lo,x
	bra env_step_next
env_step_release:
	stz env_gate,x
	stz env_st,x
	CARDY lda, 6
	and #$0F
	jsr env_step_down
env_step_next:	tya
	clc
	adc #7
	tay
	inx
	cpx #3
	beq :+
	jmp env_step_v
:	rts
env_step_down:	asl a                   ; level -= env_decay[a], not below 0
	phy
	tay
	sec
	lda env_lo,x
	sbc env_decay,y
	sta env_lo,x
	lda _env_hi,x
	sbc env_decay+1,y
	sta _env_hi,x
	ply
	bcs :+
	stz env_lo,x
	stz _env_hi,x
:	rts

.import popa

.segment "RODATA"
patches_end:

; CRC-8, poly $07, MSB first (mbox.inc's mb_crc8 as a table)
crc8:
.repeat 256, I
	.scope
	c .set I
	.repeat 8
		.if c & $80
			c .set ((c << 1) ^ $07) & $FF
		.else
			c .set (c << 1) & $FF
		.endif
	.endrep
	.byte c
	.endscope
.endrep

; generated by CommanderSID's tools/sidplay/gen_tables.py (sidplay_tables.inc)
env_attack:
	.word $FF00, $FF00, $FF00, $B115, $6FD8, $4BE5, $3E80, $3520
	.word $2A80, $1100, $0880, $0550, $0440, $016B, $00DA, $0088
env_decay:
	.word $FF00, $B115, $588B, $3B07, $2548, $194C, $14D5, $11B5
	.word $0E2B, $05AB, $02D5, $01C5, $016B, $0079, $0049, $002D
