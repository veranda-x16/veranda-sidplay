/*
 * ui.c -- the window and everything in it but the voices (viz.c): the
 * menus, the frame and its title (the tune's name), the playlist (the
 * folder's folders and .sid files, a scroll bar), now playing (title,
 * author, released, song x of y, time and length, a progress bar, the
 * transport icons and their lights), the status line; the pointer, the
 * keys, and the main loop's pass (auto-advance, repeat).
 *
 * The look is Veranda Look 2's (DECISIONS V74, V77, V79): the kernel's
 * menu bar (theme_menus: the Veranda mark, the program's name first, its
 * menus in the UI font, the clock and the speaker; open menus on the
 * highlight colour), the window's chrome from the SDK (vrwin.h: the title
 * band on the theme's accent with the tune's name and the close glyph at
 * the right, the status bar, a frame and the dither shadow) over the
 * desk's colour, Look 2's palette (theme_palette, light or dark) for the
 * panels' chrome heads, the selection (the theme's highlight), the bars
 * and the scopes. Drawing goes to the foreground only: the menus and the
 * dialog boxes keep what they cover on the save-under stack (V14); while
 * a menu is open nothing is drawn where a menu can be (above the voices).
 *
 * Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
 * GPL-3.0-or-later (see COPYING).
 */
#include "sidp.h"
#include <theme.h>
#include <vrchoose.h>
#include <vrpath.h>

#pragma code-name ("OVERLAY1")

unsigned char view_tracker;
static unsigned char hilite;            /* the selection's colour, 0: inverted */

/*---------------------------------------------------------------
  the menus
 ---------------------------------------------------------------*/
struct item { const char *text; unsigned char type; void *p; };
struct menu2 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct item i[2]; };
struct menu3 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct item i[3]; };
struct menu7 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct item i[7]; };
struct menu9 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct item i[9]; };
struct bar4 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct item i[4]; };

static void m_about(void), m_da0(void), m_da1(void), m_da2(void), m_da3(void), m_da4(void), m_da5(void);
static void m_open(void), m_quit(void), m_card(void);
static void m_play(void), m_stop(void), m_next(void), m_prev(void), m_snext(void), m_sprev(void);
static void m_auto(void), m_repeat(void);
static void m_list(void), m_tracker(void);

#define DA_MAX 6
static char da_names[DA_MAX][17];
static unsigned char n_da;

#define IH 16                            /* (Look 2: items 16 lines apart, V74) */
static const char t_auto0[] = "Auto-advance: off", t_auto1[] = "Auto-advance: on";
static const char t_rep0[] = "Repeat: off", t_rep1[] = "Repeat: folder", t_rep2[] = "Repeat: song";
static const char t_list0[] = "  Playlist", t_list1[] = "* Playlist";
static const char t_trk0[] = "  Tracker", t_trk1[] = "* Tracker";
static struct menu7 mgeos = { 16, 16 + IH, 0, 0, VERTICAL | 1, {
    { "About SID Player", MENU_ACTION, m_about },
    { da_names[0], MENU_ACTION, m_da0 }, { da_names[1], MENU_ACTION, m_da1 },
    { da_names[2], MENU_ACTION, m_da2 }, { da_names[3], MENU_ACTION, m_da3 },
    { da_names[4], MENU_ACTION, m_da4 }, { da_names[5], MENU_ACTION, m_da5 } } };
static struct menu3 mfile = { 16, 16 + 3 * IH, 0, 0, VERTICAL | 3, {
    { "Open...", MENU_ACTION, m_open }, { "Find the card", MENU_ACTION, m_card },
    { "Quit", MENU_ACTION, m_quit } } };
static struct menu9 mplay = { 16, 16 + 8 * IH, 0, 0, VERTICAL | 8, {
    { "Play/Pause", MENU_ACTION, m_play }, { "Stop", MENU_ACTION, m_stop },
    { "Next tune", MENU_ACTION, m_next }, { "Previous tune", MENU_ACTION, m_prev },
    { "Next song", MENU_ACTION, m_snext }, { "Previous song", MENU_ACTION, m_sprev },
    { t_auto1, MENU_ACTION, m_auto }, { t_rep0, MENU_ACTION, m_repeat }, { 0, 0, 0 } } };
static struct menu2 mview = { 16, 16 + 2 * IH, 0, 0, VERTICAL | 2, {
    { t_list1, MENU_ACTION, m_list }, { t_trk0, MENU_ACTION, m_tracker } } };
static const char *const barnames[4] = { "SID Player", "File", "Play", "View" };
static struct bar4 mbar = { 0, 14, 0, 0, HORIZONTAL | UN_CONSTRAINED | 4, {
    { "SID Player", SUB_MENU, &mgeos }, { "File", SUB_MENU, &mfile },
    { "Play", SUB_MENU, &mplay }, { "View", SUB_MENU, &mview } } };

/* The menus' widths in the UI font (theme_menus: the menus' font, V79);
   the kernel puts each list under its bar item (the table's place is not
   used, V74) and takes the bar's clicks up to its items' end (vr_bar_end,
   the bar's right) */
static void menu_layout(void)
{
    static unsigned int *const lefts[4] = { &mgeos.left, &mfile.left, &mplay.left, &mview.left };
    static unsigned int *const rights[4] = { &mgeos.right, &mfile.right, &mplay.right, &mview.right };
    static const char *const longest[4] = { "About SID Player", "Find the card", "Auto-advance: off", "* Playlist" };
    unsigned int e[5], w;
    unsigned char i, k;
    mgeos.flags = (unsigned char)(VERTICAL | (1 + n_da));
    mgeos.bottom = (unsigned char)(16 + (1 + n_da) * IH);
    for (i = 0; i < 5; ++i)
        e[i] = vr_bar_end(barnames, i);         /* (it ends in the system font) */
    UseFont(FONT_UI);
    for (i = 0; i < 4; ++i) {
        *lefts[i] = e[i];
        w = strw(longest[i]);
        if (i == 0)
            for (k = 0; k < n_da; ++k)
                if (strw(da_names[k]) > w)
                    w = strw(da_names[k]);
        *rights[i] = e[i] + w + 16;
    }
    UseSystemFont();
    mbar.right = e[4];
}

static void menu_marks(void)
{
    mplay.i[6].text = auto_adv ? t_auto1 : t_auto0;
    mplay.i[7].text = repeat == 2 ? t_rep2 : repeat ? t_rep1 : t_rep0;
    mview.i[0].text = view_tracker ? t_list0 : t_list1;
    mview.i[1].text = view_tracker ? t_trk1 : t_trk0;
}

static void menus_off(void) { GotoFirstMenu(); }

/*---------------------------------------------------------------
  the frame
 ---------------------------------------------------------------*/
static void panel(unsigned int l, unsigned int t, unsigned int r, unsigned int b, const char *head)
{
    WFrameRectangle(0xFF, l, t, r, b);
    WHorizontalLine(0xFF, l, r, t + 16);
    if (head) {
        currentMode = SET_BOLD;
        strcpy(sb, head);
        WPutString(sb, l + 9, t + 12);
        currentMode = 0;
    }
}

/* the desk (its colour: the theme's; without one the dither, as GEOS's
   desktop), the window, the panels and their chrome heads. The menu bar's
   cards are the kernel's (DoMenu colours them; nothing here covers them) */
static void frame(void)
{
    dispBufferOn = ST_WR_FORE;
    ColorRect(0, 80, 2, 58, theme.desk);        /* (the colour map on) */
    SetPattern(theme.desk ? 0 : 2);
    WRectangle(0, 16, 639, 479);
    win_draw("SID Player");
    panel(LP_L, LP_T, LP_R, LP_B, 0);
    panel(RP_L, RP_T, RP_R, RP_B, "Now playing");
    panel(VP_L, VP_T, VP_R, VP_B, 0);
    ColorRect(2, 40, 6, 2, C_BAR);              /* the panels' heads */
    ColorRect(43, 35, 6, 2, C_BAR);
}

static void reicons(void);

/*---------------------------------------------------------------
  the title and the status line
 ---------------------------------------------------------------*/
static void title(void)
{
    const char *t = "SID Player";
    if (playing && t_name[0])
        t = t_name;
    else if (playing)
        t = playing_name;
    currentMode = SET_BOLD;
    fit(t, 520);                                /* (printable, cut) */
    currentMode = 0;
    win_title(sb);
    LOG("TITLE", sb);
}

/* the status bar: the state at its left, the modes at its right
   (vr_win_status draws both, the band cleared); a new state redraws its
   left part alone, in the same place and font (only its area cleared: the
   UI font's text costs about 4,000 cycles a character, V81) */
static char st_left[48], st_right[40];
#define ST_Y   (((WIN_Y + WIN_H - 2) << 3) + 1)     /* the status bar's line (vrwin.c) */
#define ST_SPLIT 420                                /* the left text's room ends */

void ui_status(const char *s)
{
    strncpy(st_left, s, sizeof st_left - 1);
    SetPattern(0);
    WRectangle(WIN_L + 1, ST_Y + 1, ST_SPLIT, ST_Y + 14);
    UseFont(FONT_UI);
    fit(st_left, ST_SPLIT - WIN_L - 12);
    WPutString(sb, WIN_L + 6, ST_Y + 2 + baselineOffset);
    UseSystemFont();
}

static void status_modes(void)
{
    char *p;
    strcpy(sb, auto_adv ? "Auto-advance on" : "Auto-advance off");
    p = sb + strlen(sb);
    strcpy(p, repeat == 2 ? ",  repeat song" : repeat ? ",  repeat folder" : ",  repeat off");
    strcpy(st_right, sb);
    win_status(st_left, st_right);
}

void ui_message(const char *a, const char *b)
{
    strcpy(sb, a);
    if (b) {
        strcat(sb, ": ");
        strcat(sb, b);
    }
    strcpy(sb + 48, sb);
    ui_status(sb + 48);
}

/*---------------------------------------------------------------
  now playing: the fields, each drawn again when its value changes
 ---------------------------------------------------------------*/
enum { F_SONG, F_TIME, F_N };
static unsigned int f_last[F_N];
static unsigned char prog_n;

static void label(const char *s, unsigned int y)
{
    strcpy(sb, s);
    WPutString(sb, RP_X, y);
}

static void now_labels(void)
{
    label("Title", 76);
    label("Author", 90);
    label("Released", 104);
    label("Song", 122);
    label("Time", 136);
    WFrameRectangle(0xFF, PROG_COL * 8 - 1, PROG_ROW * 8 - 1, (PROG_COL + PROG_W) * 8, PROG_ROW * 8 + 8);
    ColorRect(PROG_COL, PROG_W, PROG_ROW, 1, C_BAR);
    WFrameRectangle(0xFF, (ICON_COL0 + 6) * 8 - 1, LED_ROW * 8 - 1, (ICON_COL0 + 6) * 8 + 16, LED_ROW * 8 + 8);
    WFrameRectangle(0xFF, (ICON_COL0 + 11) * 8 - 1, LED_ROW * 8 - 1, (ICON_COL0 + 11) * 8 + 16, LED_ROW * 8 + 8);
    prog_n = 0;
}

static void progress(unsigned int t, unsigned int len)
{
    unsigned char n = 0;
    if (playing && len)
        n = t >= len ? PROG_W : (unsigned char)(((unsigned long)t * PROG_W) / len);
    if (n > prog_n)
        ColorRect(PROG_COL + prog_n, n - prog_n, PROG_ROW, 1, C_PROG);
    else if (n < prog_n)
        ColorRect(PROG_COL + n, prog_n - n, PROG_ROW, 1, C_BAR);
    prog_n = n;
}

static void leds(void)
{
    ColorRect(ICON_COL0 + 6, 2, LED_ROW, 1, playing ? (paused ? C_YELLOW : C_GREEN) : 0);
    ColorRect(ICON_COL0 + 11, 2, LED_ROW, 1, playing ? 0 : C_RED);
}

static unsigned int song_len(void)
{
    return cur_len ? cur_len : DEFAULT_LEN;
}

static void fields(void)
{
    char s[24], *p;
    unsigned int t = IRQ_WORD(el_sec);
    if (f_last[F_SONG] != (song | songs << 8)) {
        f_last[F_SONG] = song | songs << 8;
        p = s;
        *p = 0;
        if (playing) {
            p = dec(s, song);
            strcpy(p, " of ");
            dec(p + 4, songs);
        }
        field_text(s, VAL_X, 122, 190, 0);
    }
    if (f_last[F_TIME] != t) {
        f_last[F_TIME] = t;
        s[0] = 0;
        if (playing) {
            p = mmss(s, t);
            strcpy(p, "  /  ");
            if (cur_len)
                mmss(p + 5, cur_len);
            else
                strcpy(p + 5, "-:--");
            if (paused)
                strcat(s, "   paused");
        }
        field_text(s, VAL_X, 136, 190, 1);
        progress(t, cur_len ? cur_len : auto_adv ? DEFAULT_LEN : 0);
    }
}

static void now_static(void)
{
    char s[48], *p;
    title();
    field_text(playing ? t_name : "", VAL_X, 76, 196, 1);
    field_text(playing ? t_author : "", VAL_X, 90, 196, 0);
    field_text(playing ? t_rel : "", VAL_X, 104, 196, 0);
    s[0] = 0;
    if (playing) {
        strcpy(s, is_rsid ? "RSID (C64)" : "PSID v");
        p = s + strlen(s);
        if (!is_rsid)
            p = dec(p, t_version);
        strcpy(p, t_ntsc ? ",  NTSC" : ",  PAL");
        strcat(s, t_model == 2 ? ",  8580" : t_model == 1 ? ",  6581" : "");
        p = s + strlen(s);
        strcpy(p, ",  ");
        p = dec(p + 3, (unsigned int)((flen + 1023) >> 10));
        strcpy(p, " KB");
    }
    field_text(s, RP_X, 171, 264, 0);
    field_text(card_text, RP_X, 252, 264, 0);
    field_text(db_ok ? (have_lens || !playing ? "Song lengths: SONGLEN.IDX" : "SONGLEN.IDX does not know this tune")
                     : "No SONGLEN.IDX: songs play for 3:00", RP_X, 266, 264, 0);
}

/* a tune started or stopped (or another song): all that shows it */
void ui_track(void)
{
    f_last[F_SONG] = f_last[F_TIME] = 0xFFFF;
    now_static();
    fields();
    leds();
    reicons();
    ui_status(!card_base ? card_text : playing ? (paused ? "Paused" : "Playing") : "Stopped");
}

/*---------------------------------------------------------------
  the playlist: rows of 16 pixels (two cards: the selection is their
  colour), a mark and a picture each; the playing tune marked and bold;
  the scroll bar (the colour map's grey, its thumb darker)
 ---------------------------------------------------------------*/
static unsigned char thumb_a, thumb_n;

static void list_head(void)
{
    char *p;
    clear(LP_L + 1, LP_T + 1, LP_R - 1, LP_T + 15);
    currentMode = SET_BOLD;
    strcpy(sb, "Playlist");
    WPutString(sb, LIST_X + 4, LP_T + 12);
    currentMode = 0;
    fit((const char *)curDirName, 150);
    WPutString(sb, LIST_X + 64, LP_T + 12);
    p = dec(sb, n_sids);
    strcpy(p, n_sids == 1 ? " tune" : " tunes");
    WPutString(sb, LP_R - 8 - strw(sb), LP_T + 12);
}

static void list_mark(unsigned char r, unsigned char on)
{
    unsigned int y = LIST_Y + r * ROW_H;
    if (hilite)
        ColorRect(LIST_COL, SB_COL - LIST_COL, (unsigned char)(y >> 3), 2, on ? hilite : 0);
    else if (on)
        WInvertRectangle(LIST_X, y, LIST_XR, y + ROW_H - 1);
}

static void list_row(unsigned char r)
{
    unsigned int e = top + r;
    unsigned int y = LIST_Y + r * ROW_H;
    unsigned char playing_here = e == playing_ent && playing;
    const unsigned char *pic;
    clear(LIST_X, y, LIST_XR, y + ROW_H - 1);
    if (hilite)
        list_mark(r, 0);
    if (e >= n_ent)
        return;
    ent_get(e);
    pic = cur[0] == T_UP ? pic_up : cur[0] == T_DIR ? pic_folder : pic_note;
    if (playing_here)
        WBitmapUp((void *)pic_playing, LIST_COL, 1, y + 4, 8);
    WBitmapUp((void *)pic, LIST_COL + 1, 1, y + 4, 8);
    currentMode = (playing_here || cur[0] == T_DIR) ? SET_BOLD : 0;
    fit(cur[0] == T_UP ? ".." : cur + 1, LIST_XR - 36 - LIST_X);
    WPutString(sb, LIST_X + 20, y + 11);
    currentMode = 0;
    if (e == sel)
        list_mark(r, 1);
}

static void list_bar(void)
{
    unsigned char a = 0, n = SB_TRACK;
    if (n_ent > LIST_N) {
        a = (unsigned char)(((unsigned long)top * SB_TRACK) / n_ent);
        n = (unsigned char)((LIST_N * SB_TRACK) / n_ent);
        if (!n)
            n = 1;
        if (a + n > SB_TRACK)
            a = SB_TRACK - n;
    }
    if (a == thumb_a && n == thumb_n)
        return;
    ColorRect(SB_COL, 1, SB_ROW0 + 1, SB_TRACK, C_BAR);
    ColorRect(SB_COL, 1, SB_ROW0 + 1 + a, n, C_THUMB);
    thumb_a = a;
    thumb_n = n;
}

static void list_draw(void)
{
    unsigned char r;
    for (r = 0; r < LIST_N; ++r)
        list_row(r);
    list_bar();
}

/* the left panel: the playlist or the tracker */
static void left_panel(void)
{
    clear(LP_L + 1, LP_T + 1, LP_R - 1, LP_B - 1);
    ColorRect(LIST_COL, SB_COL + 1 - LIST_COL, SB_ROW0, SB_ROWN - SB_ROW0 + 1, 0);
    if (view_tracker) {
        trk_draw();
        return;
    }
    WVerticalLine(0xFF, SB_COL * 8 - 1, LP_T + 16, LP_B);
    WBitmapUp((void *)pic_arrow_up, SB_COL, 1, SB_ROW0 * 8, 8);
    WBitmapUp((void *)pic_arrow_down, SB_COL, 1, SB_ROWN * 8, 8);
    thumb_a = thumb_n = 0xFF;
    list_head();
    list_draw();
}

static void row_of(unsigned int e)
{
    if (!view_tracker && e != NONE && e >= top && e < top + LIST_N)
        list_row((unsigned char)(e - top));
}

static void scroll_to(int t)
{
    int most = n_ent > LIST_N ? (int)(n_ent - LIST_N) : 0;
    if (t > most)
        t = most;
    if (t < 0)
        t = 0;
    if ((unsigned int)t == top)
        return;
    top = (unsigned int)t;
    if (!view_tracker)
        list_draw();
}

static void select_entry(int e)
{
    unsigned int old = sel, t = top;
    if (!n_ent)
        return;
    if (e < 0)
        e = 0;
    if (e >= (int)n_ent)
        e = (int)n_ent - 1;
    sel = (unsigned int)e;
    if (sel < top)
        top = sel;
    else if (sel >= top + LIST_N)
        top = sel - LIST_N + 1;
    if (view_tracker)
        return;
    if (top != t)
        list_draw();
    else {
        row_of(old);
        row_of(sel);
    }
}

/*---------------------------------------------------------------
  playing
 ---------------------------------------------------------------*/
static void show_error(unsigned char r)
{
    ui_track();
    if (r) {
        LOG("ERROR", errs[r]);
        ui_status(errs[r]);
        Beep();
    }
}

static unsigned int q_pos = NONE;

void ui_play_entry(unsigned int e)
{
    unsigned int was = playing_ent;
    unsigned char r;
    ent_get(e);
    if (cur[0] == T_LONG) {
        ui_status("The name is too long to open");
        Beep();
        return;
    }
    if (cur[0] != T_SID)
        return;
    q_pos = e;
    strcpy(playing_name, cur + 1);
    strcpy(playing_dir, (const char *)curDirName);
    playing_ent = e;
    SetPointer(PTR_BUSY);
    ui_status("Loading...");
    strcpy(sb + 48, cur + 1);
    r = play_file(sb + 48);
    SetPointer(PTR_ARROW);
    if (r)
        playing_ent = NONE;
    row_of(was);
    row_of(e);
    if (!r) {
        select_entry((int)e);
        viz_reset();
    }
    show_error(r);
}

static void stop_cmd(void);

/* the playlist's folder made the music's again: 1 if it changed */
static unsigned char music_dir(void)
{
    if (!playing_dir[0] || !strcmp(playing_dir, (const char *)curDirName))
        return 0;
    if (ChDir((void *)playing_dir)) {
        ChDir((void *)curDirName);
        return 0;
    }
    strcpy((char *)curDirName, playing_dir);
    return 1;
}

/* the next or previous tune from the one last played or tried (q_pos),
   in its folder; past the end: the other end with repeat (folder), else
   stop. A tune that does not load is passed over. */
static void step_tune(signed char d)
{
    unsigned int e, k;
    if (music_dir())
        ui_list();
    if (!n_sids)
        return;
    e = q_pos;
    for (k = 0; k < n_ent; ++k) {
        if (e == NONE)
            e = d > 0 ? 0 : n_ent - 1;
        else {
            e += d;
            if (e >= n_ent) {
                if (repeat != 1) {
                    if (d > 0)
                        stop_cmd();
                    return;
                }
                e = d > 0 ? 0 : n_ent - 1;
            }
        }
        ent_get(e);
        if (cur[0] != T_SID)
            continue;
        ui_play_entry(e);
        if (playing)
            return;
    }
}

static void song_step(signed char d)
{
    unsigned char n;
    if (!playing)
        return;
    n = song + d;
    if (!n || n > songs)
        return;
    if (play_song(n))
        playing = 0;
    viz_reset();
    ui_track();
}

static void pause_toggle(void)
{
    if (!playing) {
        if (n_ent && sel < n_ent) {
            ent_get(sel);
            if (cur[0] == T_SID) {
                ui_play_entry(sel);
                return;
            }
        }
        step_tune(1);
        return;
    }
    pause_set(!paused);
    f_last[F_TIME] = 0xFFFF;
    fields();
    leds();
    ui_status(paused ? "Paused" : "Playing");
}

static void stop_cmd(void)
{
    stop();
    row_of(playing_ent);
    ui_track();
}

/*---------------------------------------------------------------
  folders
 ---------------------------------------------------------------*/
void ui_list(void)
{
    SetPointer(PTR_BUSY);
    list_read();
    SetPointer(PTR_ARROW);
    q_pos = playing_dir[0] && !strcmp(playing_dir, (const char *)curDirName) ? find_entry(playing_name) : NONE;
    playing_ent = playing ? q_pos : NONE;
    if (!view_tracker)
        left_panel();
    LOG("FOLDER", (const char *)curDirName);
}

static void enter(const char *name)
{
    unsigned char l;
    if (name[0] == '.' && name[1] == '.') {
        if (at_root() || ChDir(".."))
            return;
        l = (unsigned char)strlen((char *)curDirName);
        while (l > 1 && curDirName[--l] != '/')
            ;
        curDirName[l ? l : 1] = 0;
    } else {
        l = (unsigned char)strlen((char *)curDirName);
        if (l + strlen(name) + 2 > 61 || ChDir((void *)name)) {
            ui_status("Cannot open the folder");
            Beep();
            return;
        }
        if (l > 1)
            curDirName[l++] = '/';
        strcpy((char *)curDirName + l, name);
    }
    ui_list();
}

static void open_entry(unsigned int e)
{
    char name[64];
    ent_get(e);
    if (cur[0] == T_DIR || cur[0] == T_UP) {
        strcpy(name, cur + 1);
        enter(name);
    } else
        ui_play_entry(e);
}

/*---------------------------------------------------------------
  the menus' routines
 ---------------------------------------------------------------*/
struct dlg_about {
    unsigned char pos;
    unsigned int top, bot, left, right;
    unsigned char t1, x1, y1; const char *s1;
    unsigned char t2, x2, y2; const char *s2;
    unsigned char t3, x3, y3; const char *s3;
    unsigned char t4, x4, y4; const char *s4;
    unsigned char ok, okx, oky;
    unsigned char end;
};
static struct dlg_about about = {
    BOX_POS, BOX_TOP, BOX_BOT, BOX_LEFT, BOX_RIGHT,
    DBTXTSTR, TXT_LN_X, TXT_LN_1_Y, "\x18Veranda SID Player 1.1\x1B",
    DBTXTSTR, TXT_LN_X, TXT_LN_2_Y, "A port of CommanderSID's SID player.",
    DBTXTSTR, TXT_LN_X, TXT_LN_3_Y, "GPL-3.0-or-later.",
    DBTXTSTR, TXT_LN_X, TXT_LN_4_Y, card_text,
    OK, BOX_OKX, BOX_OKY,
    0
};

static void m_about(void)
{
    menus_off();
    LOG("ABOUT", 0);
    DoDlgBox(&about);
    LOG("ABOUT CLOSED", 0);
}


static void da(unsigned char i)
{
    unsigned char r;
    menus_off();
    vr_cd(VR_APPS);                     /* (with the applications: Veranda V71) */
    r = da_run(da_names[i]);
    ChDir((void *)curDirName);
    if (r) {
        ui_status("Cannot start it");
        Beep();
    }
}
static void m_da0(void) { da(0); }
static void m_da1(void) { da(1); }
static void m_da2(void) { da(2); }
static void m_da3(void) { da(3); }
static void m_da4(void) { da(4); }
static void m_da5(void) { da(5); }

static void m_open(void)
{
    char name[64];
    unsigned int e;
    menus_off();
    LOG("OPEN BOX", 0);
    name[0] = 0;
    if (!choose(name))
        return;
    ui_list();
    e = find_entry(name);
    if (e != NONE)
        ui_play_entry(e);
}

static void m_card(void)
{
    menus_off();
    if (playing)
        stop();
    card_find();
    LOG("CARD", card_text);
    ui_track();
}

static void m_quit(void)
{
    menus_off();
    if (playing)
        stop();
#ifdef SIDP_LOG
    {
        extern void pass_stats(void);
        pass_stats();
    }
#endif
    LOG("QUIT", 0);
    intFirstVector = 0;
    card_release();
    EnterDeskTop();
}

static void m_play(void) { menus_off(); pause_toggle(); }
static void m_stop(void) { menus_off(); stop_cmd(); }
static void m_next(void) { menus_off(); step_tune(1); }
static void m_prev(void) { menus_off(); step_tune(-1); }
static void m_snext(void) { menus_off(); song_step(1); }
static void m_sprev(void) { menus_off(); song_step(-1); }

static void m_auto(void)
{
    menus_off();
    auto_adv = !auto_adv;
    menu_marks();
    status_modes();
    LOG(auto_adv ? "AUTO ON" : "AUTO OFF", 0);
}

static void m_repeat(void)
{
    menus_off();
    repeat = (unsigned char)((repeat + 1) % 3);
    menu_marks();
    status_modes();
    LOG("REPEAT", repeat == 2 ? "SONG" : repeat ? "FOLDER" : "OFF");
}

static void set_view(unsigned char t)
{
    if (view_tracker == t)
        return;
    view_tracker = t;
    menu_marks();
    left_panel();
    LOG(t ? "VIEW TRACKER" : "VIEW PLAYLIST", 0);
}
static void m_list(void) { menus_off(); set_view(0); }
static void m_tracker(void) { menus_off(); set_view(1); }

/*---------------------------------------------------------------
  the transport icons (GEOS icons, flashed when clicked)
 ---------------------------------------------------------------*/
static void i_prev(void) { LOG("ICON PREV", 0); step_tune(-1); }
static void i_play(void) { LOG("ICON PLAY", 0); pause_toggle(); reicons(); }
static void i_stop(void) { LOG("ICON STOP", 0); stop_cmd(); reicons(); }
static void i_next(void) { LOG("ICON NEXT", 0); step_tune(1); }
static void i_sminus(void) { LOG("ICON SONG-", 0); song_step(-1); }
static void i_splus(void) { LOG("ICON SONG+", 0); song_step(1); }

struct icon { const void *pic; unsigned char x, y, w, h; void *f; };
struct icons6 { unsigned char n; unsigned int mx; unsigned char my; struct icon i[6]; };
static struct icons6 icons = { 6, 0, 0, {
    { pic_prev,  ICON_COL0,      ICON_ROW * 8, 4, 24, i_prev },
    { pic_play,  ICON_COL0 + 5,  ICON_ROW * 8, 4, 24, i_play },
    { pic_stop,  ICON_COL0 + 10, ICON_ROW * 8, 4, 24, i_stop },
    { pic_next,  ICON_COL0 + 15, ICON_ROW * 8, 4, 24, i_next },
    { pic_minus, ICON_COL0 + 20, ICON_ROW * 8, 4, 24, i_sminus },
    { pic_plus,  ICON_COL0 + 25, ICON_ROW * 8, 4, 24, i_splus } } };

/* play or pause on the second icon, as the tune goes */
static void reicons(void)
{
    const void *p = playing && !paused ? pic_pause : pic_play;
    if (icons.i[1].pic == p)
        return;
    icons.i[1].pic = p;
    if (!menuNumber)
        WBitmapUp((void *)p, ICON_COL0 + 5, 4, ICON_ROW * 8, 24);
}

/*---------------------------------------------------------------
  the pointer and the keys
 ---------------------------------------------------------------*/
static void pressed(void)
{
    unsigned int x, y, e;
    unsigned char r;
    if (mouseData & 0x80)
        return;                         /* a release */
    do {
        x = mouseXPos;
        y = mouseYPosW;
    } while (x != mouseXPos || y != mouseYPosW);
    if (win_hit(x, y) == VW_XCLOSE) {
        m_quit();
        return;
    }
    if (view_tracker || y < LIST_Y || y >= LIST_Y + LIST_N * ROW_H)
        return;
    if (x >= SB_COL * 8 && x < SB_COL * 8 + 8) {      /* the scroll bar */
        r = (unsigned char)(y >> 3);
        if (r == SB_ROW0)
            scroll_to((int)top - 1);
        else if (r == SB_ROWN)
            scroll_to((int)top + 1);
        else if (r < SB_ROW0 + 1 + thumb_a)
            scroll_to((int)top - (LIST_N - 1));
        else if (r >= SB_ROW0 + 1 + thumb_a + thumb_n)
            scroll_to((int)top + (LIST_N - 1));
        return;
    }
    if (x < LIST_X || x > LIST_XR)
        return;
    e = top + (y - LIST_Y) / ROW_H;
    if (e >= n_ent)
        return;
    if (e == sel && dblClickCount) {
        dblClickCount = 0;
        LOG("DOUBLE CLICK", 0);
        open_entry(e);
        reicons();
        return;
    }
    select_entry((int)e);
    dblClickCount = CLICK_COUNT;
}

static void key(void)
{
    unsigned char k = keyData;
    if (menuNumber)
        return;
    if (k >= 'A' && k <= 'Z')
        k += 32;
    if (k == ' ')
        pause_toggle();
    else if ((k == KEY_RIGHT) || k == 'n')
        step_tune(1);
    else if ((k == KEY_LEFT && (keyDataMods & KEYM_ARROW)) || k == 'p')
        step_tune(-1);
    else if (k == KEY_ESC || k == 's')
        stop_cmd();
    else if (k == '+' || k == '=')
        song_step(1);
    else if (k == '-')
        song_step(-1);
    else if (k >= '1' && k <= '9') {
        if (playing && k - '0' <= songs) {
            song_step((signed char)(k - '0' - song));
        }
    } else if (k == 'a')
        m_auto();
    else if (k == 'r')
        m_repeat();
    else if (k == 't')
        set_view(!view_tracker);
    else if (k == (SHORTCUT | 'o'))
        m_open();
    else if (k == (SHORTCUT | 'q') || k == 'q')
        m_quit();
    else if (view_tracker)
        ;
    else if (k == KEY_UP)
        select_entry((int)sel - 1);
    else if (k == KEY_DOWN)
        select_entry((int)sel + 1);
    else if (k == KEY_PGUP)
        select_entry((int)sel - (LIST_N - 1));
    else if (k == KEY_PGDN)
        select_entry((int)sel + (LIST_N - 1));
    else if (k == KEY_HOME)
        select_entry(0);
    else if (k == KEY_END)
        select_entry(0x7FFF);
    else if (k == KEY_LEFT)             /* Backspace: up a folder */
        enter("..");
    else if (k == CR && n_ent)
        open_entry(sel);
    reicons();
}

/*---------------------------------------------------------------
  the main loop's pass: at most 30 a second
 ---------------------------------------------------------------*/
static unsigned int last_frame;
#ifdef SIDP_LOG
/* the passes' cycles (the emulator's counter at $9FB8: the log build runs
   with the card at IO6) */
static unsigned long pass_sum, pass_max, pass_n;
static unsigned long cyc(void)
{
    unsigned long v = *(volatile unsigned char *)0x9FB8;
    v |= (unsigned long)*(volatile unsigned char *)0x9FB9 << 8;
    v |= (unsigned long)*(volatile unsigned char *)0x9FBA << 16;
    v |= (unsigned long)*(volatile unsigned char *)0x9FBB << 24;
    return v;
}
static void pass_body(void);
void ui_pass(void)
{
    unsigned long t0;
    unsigned int f = IRQ_WORD(irq_frames);
    if ((unsigned int)(f - last_frame) < 2)
        return;
    t0 = cyc();
    pass_body();
    t0 = cyc() - t0;
    pass_sum += t0;
    if (t0 > pass_max)
        pass_max = t0;
    ++pass_n;
}
void pass_stats(void)
{
    LOGN("PASSES", pass_n);
    LOGN("PASS CYCLES AVERAGE", pass_n ? pass_sum / pass_n : 0);
    LOGN("PASS CYCLES MAX", pass_max);
    LOGN("FRAMES", IRQ_WORD(irq_frames));
    {
        extern void viz_stats(void);
        viz_stats();
    }
}
static void pass_body(void)
#else
void ui_pass(void)
#endif
{
    signed char w;
    unsigned int f = IRQ_WORD(irq_frames);
    if ((unsigned int)(f - last_frame) < 2)
        return;
    last_frame = f;
    asm("sei");
    w = (signed char)mouseWheel;
    mouseWheel = 0;
    asm("cli");
    if (card_base)
        card_snap();
    /* the song's time is up: the next song, then the next tune (once no
       menu is open: those draw where a menu can be) */
    if (playing && !paused && auto_adv && IRQ_WORD(el_sec) >= song_len() && !menuNumber) {
        LOG("SONG END", 0);
        if (repeat == 2)
            song_step(0);
        else if (song < songs)
            song_step(1);
        else
            step_tune(1);
        reicons();
        return;
    }
    viz_update();
    if (!menuNumber) {
        fields();
        if (w && !view_tracker)
            scroll_to((int)top + 3 * w);
    }
}

/*---------------------------------------------------------------
  the start
 ---------------------------------------------------------------*/
void ui_start(void)
{
    theme_load();
    theme_palette();                    /* Look 2's palette, light or dark (V74) */
    hilite = theme.hilite ? theme.hilite : SELECT;
    vr_cd(VR_APPS);                     /* the desk accessories, with the applications (V71) */
    n_da = (unsigned char)(DA_MAX - FindFTypes(da_names, DESK_ACC, DA_MAX, 0));
    if (n_da > DA_MAX)
        n_da = 0;
    ChDir((void *)curDirName);
    frame();
    now_labels();
    status_modes();
    viz_frame();
    iconSelFlg = ST_FLASH;
    DoIcons((void *)&icons);
    left_panel();
    menu_layout();
    menu_marks();
    theme_menus();                      /* the menu bar (V74, V79) */
    DoMenu((void *)&mbar, 0);
    ui_track();
    otherPressVec = VR_FN(pressed);
    keyVector = VR_FN(key);
    appMain = VR_FN(ui_pass);
}
