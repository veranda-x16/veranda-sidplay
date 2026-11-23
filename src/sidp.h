/*
 * sidp.h -- Veranda SID Player: the parts and the screen's layout.
 *
 * A port of the CommanderSID project's SID player (software/rev-b/
 * sidplay.s, SIDPLAY.PRG) to Veranda as a native GEOS-style application.
 *
 *   main.c   the start, the document, the banks, the main loop's pass
 *   card.s   the card: its window (IO5/IO6), the mailbox, the interrupt
 *   list.c   the folder's entries (folders first, then .sid files)
 *   tune.c   a tune: read, uploaded, its song lengths, played
 *   ui.c     the menus, the window, the playlist, now playing, the icons
 *   viz.c    the voices: scopes, notes, waveforms, envelopes, ADSR, the
 *            filter; the tracker view (all from the card's registers)
 *   draw.c   drawing helpers
 *   pics.c   the pictures (tools/mkpics.py)
 *
 * Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
 * GPL-3.0-or-later (see COPYING).
 */
#ifndef SIDP_H
#define SIDP_H
#include <veranda.h>
#include <vr.h>
#include <string.h>
#include <theme.h>
#include <vrwin.h>

#define MAPBANK(b) (*(volatile unsigned char *)0 = (b))
#define BANK       (*(volatile unsigned char *)0)

/*---------------------------------------------------------------
  the screen (640 x 480; cards of 8 x 8 where the colour map is used)
 ---------------------------------------------------------------*/
/* the window (Veranda Look 2's chrome, vrwin.h; DECISIONS V79): card
   columns 1-78, card rows 3-58 (pixels 8-631 by 24-471); its title band
   rows 3-4 (the tune's name, the close glyph at the right), its status
   bar rows 57-58; a frame and the dither shadow */
#define WIN_X      1
#define WIN_Y      3
#define WIN_W      78
#define WIN_H      56
#define WIN_L      8
#define WIN_R      631

#define LP_L       15           /* the left panel: the playlist or the tracker */
#define LP_T       47
#define LP_R       336
#define LP_B       272
#define LIST_COL   2            /* its rows: card columns 2-40 */
#define LIST_X     16
#define LIST_XR    326
#define LIST_Y     64           /* the first row (card row 8) */
#define ROW_H      16
#define LIST_N     13           /* rows shown: card rows 8-33 */
#define SB_COL     41           /* the scroll bar: card column 41 */
#define SB_ROW0    8            /* its up arrow; the track 9-32; down 33 */
#define SB_ROWN    33
#define SB_TRACK   24

#define RP_L       343          /* the right panel: now playing */
#define RP_T       47
#define RP_R       624
#define RP_B       272
#define RP_X       352
#define VAL_X      424          /* the fields' values */
#define PROG_COL   44           /* the progress bar: card row 19, columns 44-76 */
#define PROG_W     33
#define PROG_ROW   19
#define ICON_ROW   22           /* the transport icons: 4 cards x 24 rows */
#define ICON_COL0  46           /* columns 46, 51, 56, 61, 66, 71 */
#define LED_ROW    26

#define VP_L       15           /* the voices' panel */
#define VP_T       279
#define VP_R       624
#define VP_B       440
#define V_HEAD     35           /* card rows: the heads */
#define V_SCOPE    37           /* the scopes: rows 37-41 (40 pixels) */
#define V_SCOPE_H  40
#define V_WAVE     43           /* the waveform's bits */
#define V_ENV      45           /* the envelope's bar */
#define V_PW       46           /* the pulse width's bar */
#define V_TEXT     48           /* rows 48-50: ADSR, pulse width, frequency */
#define V_GRAPH    13           /* the ADSR graph: the column's cards 13-23, y 380-407 */
#define V_COLW     24           /* a voice's columns: 2, 28, 54 */
#define F_ROW1     52           /* the filter: rows 52, 54 */
#define F_ROW2     54

/* the about box and the no-card box: their own place (WIDE_DB), wider than
   GEOS's default box, for their lines in the UI font (theme_menus: the
   program's boxes in the UI font, Veranda V79); OK at card 17, line 96 */
#define BOX_POS    SET_DB_POS | WIDE_DB | 1
#define BOX_TOP    168
#define BOX_BOT    295
#define BOX_LEFT   160
#define BOX_RIGHT  479
#define BOX_OKX    17
#define BOX_OKY    96

/* colours: Veranda Look 2's palette entries (theme.h; theme_palette, the
   light or dark palette: DECISIONS V74) */
#define C_BAR      CHROME       /* the panels' heads, the progress bar's track */
#define C_THUMB    SELECT       /* the scroll bar's thumb (its track chrome) */
#define C_GREEN    GREEN
#define C_YELLOW   YELLOW
#define C_RED      RED
#define C_PROG     SELECT       /* the progress bar, the filter's cutoff */
#define C_V1       GREEN        /* the scopes' paper: green, title blue, yellow */
#define C_V2       TITLE
#define C_V3       YELLOW
#define C_ENV_OFF  CHROME       /* an envelope bar's unlit cards */

/*---------------------------------------------------------------
  card.s
 ---------------------------------------------------------------*/
unsigned char __fastcall__ card_probe(unsigned char lo);
void card_reset(void);
void card_release(void);
unsigned char __fastcall__ card_request(unsigned char cmd, unsigned char len);
unsigned char __fastcall__ card_store(unsigned char bank, unsigned int n);
void card_snap(void);
void card_irq(void);
extern unsigned char card_base;         /* $A0 IO5, $C0 IO6, 0 none */
extern unsigned char card_req[16];
extern unsigned char card_rx[128];
extern unsigned char sid[29];
extern unsigned char card_status;
extern unsigned char card_keep_on;
extern volatile unsigned int irq_frames;
extern volatile unsigned char irq_play;
extern volatile unsigned char el_jif;
extern volatile unsigned int el_sec;
extern volatile unsigned char env_hi[3];
/* a word the interrupt writes, read whole (its two bytes from one moment) */
#define IRQ_WORD(w) irq_word(&(w))
unsigned int __fastcall__ irq_word(volatile unsigned int *w);

/* the mailbox's commands (CommanderSID firmware/core/mailbox.h) */
#define MB_INFO     0x02
#define MB_STORE    0x06
#define MB_PLAY     0x0B
#define MB_STOP     0x0C
#define MB_C64_STOP 0x0E
#define MB_MD5      0x14
#define MB_PAUSE    0x15
#define MB_C64_SID  0x16

/*---------------------------------------------------------------
  list.c: the folder's entries in RAM banks, 64 bytes each: the type
  and the name; order[] the list's order
 ---------------------------------------------------------------*/
#define T_UP       'U'
#define T_DIR      'D'
#define T_SID      'S'
#define T_LONG     'L'          /* a tune whose name is too long to open */
#define ENT_MAX    512
extern unsigned int n_ent, n_sids, top, sel;
extern char cur[64];                    /* an entry read back: type, name */
extern unsigned char ent_bank;
void ent_get(unsigned int i);
unsigned char at_root(void);
void list_read(void);
unsigned int find_entry(const char *name);
#define NONE 0xFFFF

/*---------------------------------------------------------------
  main.c: what must run in main RAM for the UI (overlay 1, appBank)
 ---------------------------------------------------------------*/
extern unsigned char ui_bank, dlg_bank;  /* overlays 1 (the UI) and 2 (the Open dialog) */
extern unsigned char win_bank;          /* overlay 3: the window's chrome (the SDK's vrwin.c) */
/* the window's chrome (vrwin.h) through main RAM: overlay 3 mapped, the
   UI's bank back after */
void __fastcall__ win_draw(const char *title);
void __fastcall__ win_title(const char *title);
void win_status(const char *left, const char *right);
unsigned char win_hit(unsigned int x, unsigned int y);
unsigned char choose(char *name);       /* the Open dialog (vrchoose.h, overlay 2) */
unsigned char da_run(const char *name); /* a desk accessory (GetFile) */
unsigned char is_sid(const char *n);

/*---------------------------------------------------------------
  tune.c
 ---------------------------------------------------------------*/
extern unsigned char buf_bank;          /* the reading buffer's RAM bank */
extern unsigned char playing, paused, is_rsid, songs, song, t_ntsc;
extern unsigned char auto_adv, repeat;  /* repeat 0 off, 1 the folder, 2 the song */
extern char t_name[33], t_author[33], t_rel[33];
extern unsigned char t_version, t_model;
extern unsigned long flen;
extern unsigned int cur_len;            /* the song's length in seconds; 0 unknown */
extern unsigned char have_lens, db_ok;
extern char playing_name[64], playing_dir[64];
extern unsigned int playing_ent;
extern unsigned char card_hw, card_fw[3];
extern char card_text[48];
unsigned char card_find(void);          /* 1: found; card_text says what */
void db_probe(void);                    /* db_ok: SONGLEN.IDX found */
unsigned char play_file(const char *name);      /* 0, or an error message's number */
unsigned char play_song(unsigned char n);       /* 0 the start song */
void stop(void);
unsigned char pause_set(unsigned char on);
extern const char *const errs[];
#define DEFAULT_LEN 180

/*---------------------------------------------------------------
  draw.c
 ---------------------------------------------------------------*/
extern char sb[96];                     /* a string for the kernel */
void clear(unsigned int l, unsigned int t, unsigned int r, unsigned int b);
unsigned int strw(const char *s);
void fit(const char *s, unsigned int w);
void field_text(const char *s, unsigned int x, unsigned int y, unsigned int w, unsigned char bold);
char *dec(char *p, unsigned int v);
char *mmss(char *p, unsigned int s);
char *hex(char *p, unsigned int v, unsigned char digits);
void load_font(void);
void cards(unsigned char x, unsigned int y, const char *s, unsigned char w, unsigned char flags);

/*---------------------------------------------------------------
  ui.c
 ---------------------------------------------------------------*/
void ui_start(void);
void ui_status(const char *s);
void ui_track(void);                    /* a tune started or stopped */
void ui_list(void);
void ui_pass(void);                     /* the main loop's pass */
void ui_play_entry(unsigned int e);
void ui_message(const char *a, const char *b);
extern unsigned char view_tracker;

/*---------------------------------------------------------------
  viz.c, viz.s
 ---------------------------------------------------------------*/
void viz_frame(void);                   /* the voices' panel drawn whole */
void viz_update(void);                  /* from sid[] */
void viz_reset(void);
void trk_update(void);
void trk_draw(void);
void __fastcall__ scope_render(unsigned char v);        /* viz.s: scope_buf */
void __fastcall__ scope_blit(unsigned int vram);         /* scope_buf to the foreground */
void curve_render(void);                                /* sc_tab's curve into scope_buf */
extern unsigned char sc_tab[95], blit_rows;
extern unsigned char scope_buf[V_SCOPE_H * (V_COLW + 1)];
extern unsigned char sc_level, sc_ctrl, sc_pw;
extern unsigned int sc_step;
extern unsigned char note_index(unsigned int f);
extern const char note_names[];

/*---------------------------------------------------------------
  pics.c
 ---------------------------------------------------------------*/
extern const unsigned char pic_prev[], pic_play[], pic_pause[], pic_stop[], pic_next[];
extern const unsigned char pic_minus[], pic_plus[];
extern const unsigned char pic_up[], pic_folder[], pic_note[], pic_playing[];
extern const unsigned char pic_arrow_up[], pic_arrow_down[];

/*---------------------------------------------------------------
  logging (the test builds: SIDP_LOG)
 ---------------------------------------------------------------*/
#ifdef SIDP_LOG
void logs(const char *a, const char *b);
void lognum(const char *a, unsigned long n);
#define LOG(a, b) logs(a, b)
#define LOGN(a, n) lognum(a, n)
#else
#define LOG(a, b)
#define LOGN(a, n)
#endif

#endif
