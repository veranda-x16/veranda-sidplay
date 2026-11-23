/*
 * draw.c -- drawing helpers: text in the system font, fitted to a width;
 * numbers; text in 8 x 8 cards (PutCards) in the ROM's font.
 *
 * Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
 * GPL-3.0-or-later (see COPYING).
 */
#include "sidp.h"

char sb[96];

void clear(unsigned int l, unsigned int t, unsigned int r, unsigned int b)
{
    SetPattern(0);
    WRectangle(l, t, r, b);
}

unsigned int strw(const char *s)
{
    unsigned int w = 0;
    unsigned char b = currentMode & SET_BOLD ? 1 : 0;
    while (*s)
        w += GetCharWidth(*s++) + b;
    return w;
}

/* s into sb, cut to w pixels ("..." at the end if it was cut) */
void fit(const char *s, unsigned int w)
{
    unsigned char i = 0, j;
    unsigned char b = currentMode & SET_BOLD ? 1 : 0;
    unsigned int x = 0, dots = 3 * (GetCharWidth('.') + b);
    while (s[i] && i < 80) {
        sb[i] = s[i] >= 32 && s[i] < 127 ? s[i] : '?';
        x += GetCharWidth(sb[i]) + b;
        ++i;
    }
    sb[i] = 0;
    if (x <= w)
        return;
    while (i && x + dots > w) {
        --i;
        x -= GetCharWidth(sb[i]) + b;
    }
    for (j = 0; j < 3; ++j)
        sb[i++] = '.';
    sb[i] = 0;
}

/* s at (x, baseline y), cut to w pixels, its area cleared first */
void field_text(const char *s, unsigned int x, unsigned int y, unsigned int w, unsigned char bold)
{
    currentMode = bold ? SET_BOLD : 0;
    clear(x, y - 7, x + w - 1, y + 2);
    fit(s, w);
    WPutString(sb, x, y);
    currentMode = 0;
}

char *dec(char *p, unsigned int v)
{
    char t[6];
    unsigned char n = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *p++ = t[--n];
    *p = 0;
    return p;
}

char *mmss(char *p, unsigned int s)
{
    p = dec(p, s / 60);
    *p++ = ':';
    *p++ = (char)('0' + (s % 60) / 10);
    *p++ = (char)('0' + s % 10);
    *p = 0;
    return p;
}

char *hex(char *p, unsigned int v, unsigned char digits)
{
    static const char hd[] = "0123456789ABCDEF";
    p += digits;
    *p = 0;
    while (digits--) {
        *--p = hd[v & 15];
        v >>= 4;
    }
    return p + 0;
}

/*---------------------------------------------------------------
  text in cards: the ROM's 8 x 8 font (glyphs 32-127), PutCards (the
  MOD Player's way: the font found by its smiley in ROM bank 6)
 ---------------------------------------------------------------*/
/* glyph c at GLYPHS + 8 c: 32-127 the ROM's, 160-255 the same reversed
   (a set flag: cards() takes c | 128) */
static unsigned char glyphs[96 * 8 + 32 * 8 + 96 * 8];
#define GLYPHS ((void *)(glyphs - 256))
static const unsigned char smiley[8] = { 0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E };

void load_font(void)
{
    unsigned char keep;
    unsigned int a, f = 0xC800;
    asm("sei");                 /* ROM bank 6 has no interrupt vectors */
    keep = *(unsigned char *)1;
    *(unsigned char *)1 = 6;
    for (a = 0xC000; a; a += 0x800)
        if (!memcmp((void *)(a + 8), smiley, 8)) {
            f = a;
            break;
        }
    memcpy(glyphs, (void *)(f + 256), 96 * 8);
    *(unsigned char *)1 = keep;
    asm("cli");
    for (a = 0; a < 96 * 8; ++a)
        glyphs[a + 1024] = (unsigned char)~glyphs[a];
}

static char tb[81];
/* s at card column x, row y (pixels), padded with spaces to w cards;
   flags: 1 reversed, 2 bold; a character + 128 is drawn reversed */
void cards(unsigned char x, unsigned int y, const char *s, unsigned char w, unsigned char flags)
{
    unsigned char i = 0, c;
    while (s[i] && i < w) {
        c = s[i] & 0x7F;
        tb[i] = (c >= 32 && c < 127) ? s[i] : '?';
        ++i;
    }
    while (i < w)
        tb[i++] = ' ';
    PutCards(tb, GLYPHS, x, w, y, flags);
}
