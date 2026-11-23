/*
 * main.c -- Veranda SID Player: the start.
 *
 * The shell starts it from its icon, or with a .sid file (app.hdr's
 * documents = SID, Veranda DECISIONS V48): r0L & ST_LD_DATA, the name in
 * dataLongName (its whole FAT name, V82), its folder in curDirName. It
 * finds the card (IO5, else
 * IO6), takes its RAM banks (the folder's entries, a buffer), puts the
 * card's part of the interrupt in intFirstVector (the ARM lease, the
 * song's time, the envelopes: card.s) and draws its window (ui.c); the
 * main loop's pass is ui_pass.
 *
 * Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
 * GPL-3.0-or-later (see COPYING).
 */
#include "sidp.h"
#include <vrchoose.h>

unsigned char ui_bank, dlg_bank, win_bank;

/* the window (Veranda Look 2's chrome, vrwin.h; DECISIONS V79): the
   SDK's vrwin.c compiled into overlay 3 (main RAM has no room for it),
   called from the UI (overlay 1) through these; its strings and this
   table in main RAM */
static const struct vrwin win = { WIN_X, WIN_Y, WIN_W, WIN_H, 0, 2, VW_CLOSE | VW_FRAME | VW_SHADOW };

void __fastcall__ win_draw(const char *title)
{
    MAPBANK(win_bank);
    vr_win(&win, title);
    MAPBANK(ui_bank);
}

void __fastcall__ win_title(const char *title)
{
    MAPBANK(win_bank);
    vr_win_title(&win, title);
    MAPBANK(ui_bank);
}

void win_status(const char *left, const char *right)
{
    MAPBANK(win_bank);
    vr_win_status(&win, left, right);
    MAPBANK(ui_bank);
}

unsigned char win_hit(unsigned int x, unsigned int y)
{
    unsigned char r;
    MAPBANK(win_bank);
    r = vr_win_hit(&win, x, y);
    MAPBANK(ui_bank);
    return r;
}

/* the file chooser: the SDK's Open dialog (vrchoose.h, Veranda DECISIONS
   V79), compiled into overlay 2: called from main RAM with its overlay
   mapped (it maps a bank of its own for the entries), its filter here
   too; the UI's bank back after it */
static unsigned char sid_filter(const char *name)
{
    return is_sid(name);
}

unsigned char choose(char *name)
{
    unsigned char r;
    vr_file_filter = sid_filter;
    MAPBANK(dlg_bank);
    r = vr_file(VRF_OPEN, "Play a SID tune", name);
    MAPBANK(ui_bank);
    return r;
}

/* a desk accessory: it comes back here (RstrAppl); the UI's bank and
   the interrupt's vector again */
unsigned char da_run(const char *name)
{
    unsigned char r;
    strcpy(sb, name);
    r = GetFile(sb, 0, 0, 0, 0);
    appBank = ui_bank;
    MAPBANK(ui_bank);
    intFirstVector = (unsigned int)card_irq;
    return r;
}

#ifdef SIDP_LOG
static char lb[112];
void logs(const char *a, const char *b)
{
    strcpy(lb, "SIDP ");
    strcat(lb, a);
    if (b) {
        strcat(lb, " ");
        strncat(lb, b, 80);
    }
    LogText(lb);
}

void lognum(const char *a, unsigned long n)
{
    char t[12];
    unsigned char i = 11;
    t[11] = 0;
    do {
        t[--i] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    logs(a, t + i);
}
#endif

struct nocard {
    unsigned char pos;
    unsigned int top, bot, left, right;
    unsigned char t1, x1, y1; const char *s1;
    unsigned char t2, x2, y2; const char *s2;
    unsigned char t3, x3, y3; const char *s3;
    unsigned char ok, okx, oky;
    unsigned char end;
};
static const struct nocard nocard = {
    BOX_POS, BOX_TOP, BOX_BOT, BOX_LEFT, BOX_RIGHT,
    DBTXTSTR, TXT_LN_X, TXT_LN_1_Y, "\x18No X16-SID card found.\x1B",
    DBTXTSTR, TXT_LN_X, TXT_LN_2_Y, "Looked at IO5 and IO6.",
    DBTXTSTR, TXT_LN_X, TXT_LN_3_Y, "Nothing can play.",
    OK, BOX_OKX, BOX_OKY,
    0
};

int main(void)
{
    unsigned char doc = (unsigned char)(r0L & ST_LD_DATA);     /* (before anything uses r0) */
    unsigned int e;
    char name[LONG_MAX + 1];

    if (doc) {                          /* (the whole name, long or not: API 5, V82) */
        memcpy(name, (void *)dataLongName, LONG_MAX);
        name[LONG_MAX] = 0;
    }
    load_font();
    ent_bank = BankAlloc(ENT_MAX / 128 + 1);
    if (!ent_bank) {
        LogText("SIDP NO BANKS");
        EnterDeskTop();
    }
    buf_bank = ent_bank + ENT_MAX / 128;
    LoadOverlay(1, 0);
    ui_bank = r1L;
    if (!ui_bank) {
        LogText("SIDP NO OVERLAY");
        EnterDeskTop();
    }
    LoadOverlay(2, 0);
    dlg_bank = r1L;
    if (!dlg_bank) {
        LogText("SIDP NO OVERLAY");
        EnterDeskTop();
    }
    LoadOverlay(3, 0);
    win_bank = r1L;
    if (!win_bank) {
        LogText("SIDP NO OVERLAY");
        EnterDeskTop();
    }
    appBank = ui_bank;
    MAPBANK(ui_bank);
    if (doc && curDirName[0])
        ChDir((void *)curDirName);      /* (started from the top folder) */
    if (!curDirName[0]) {
        curDirName[0] = '/';
        curDirName[1] = 0;
    }
    card_find();
    LOG("CARD", card_text);
    intFirstVector = (unsigned int)card_irq;
    db_probe();
    list_read();
    ui_start();
    LOG("RUNNING", 0);
    if (!card_base)
        DoDlgBox((void *)&nocard);
    if (doc) {
        LOG("DOCUMENT", name);
        e = find_entry(name);
        if (e != NONE)
            ui_play_entry(e);
    }
    return 0;
}
