/*
 * tune.c -- a tune on the card, as sidplay.s plays it: read from the card
 * (the SD card) in 2 KB pieces and stored on the X16-SID card (STORE, $06:
 * a PSID at offset 0; an RSID after C64ROMS.BIN, at $5000), its song
 * lengths looked up (the card's MD5 of the file, MB_MD5 $14, in
 * SONGLEN.IDX), then played (PLAY $0B, or C64_SID $16 for an RSID on the
 * card's C64), paused ($15), stopped ($0C, $0E).
 *
 * Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
 * GPL-3.0-or-later (see COPYING).
 */
#include "sidp.h"

#define CHUNK      2048
#define RSID_AT    0x5000UL     /* RSID tunes go after the ROMs (at 0) */
#define ROMS_LEN   20480UL      /* C64ROMS.BIN: BASIC, KERNAL, characters */
#define STORE_MAX  0x10000UL    /* the card's store */

unsigned char buf_bank;
unsigned char playing, paused, is_rsid, songs, song, t_ntsc, t_version, t_model;
unsigned char auto_adv = 1, repeat;
char t_name[33], t_author[33], t_rel[33];
unsigned long flen;
static unsigned long tune_off;
unsigned int cur_len;
unsigned char have_lens, db_ok;
static unsigned char lens_n;
static unsigned int lens[64];
static unsigned char roms_on_card;
char playing_name[64], playing_dir[64];
unsigned int playing_ent = NONE;
unsigned char card_hw, card_fw[3];
char card_text[48];
static char start_dir[64];

const char *const errs[] = {
    "",
    "No X16-SID card found",
    "Cannot open the file",
    "Not a PSID or RSID file",
    "Too big for the card (64 KB at most)",
    "Upload error",
    "The card cannot play this tune",
    "RSID tune: needs C64ROMS.BIN (the card's root, or the player's folder)",
};
enum { E_OK, E_NOCARD, E_OPEN, E_NOTSID, E_BIG, E_UPLOAD, E_PLAY, E_ROMS };

/*---------------------------------------------------------------
  the card: IO5, else IO6
 ---------------------------------------------------------------*/
unsigned char card_find(void)
{
    char *p;
    if (!start_dir[0])
        strcpy(start_dir, (const char *)curDirName);
    card_base = 0;
    if (!card_probe(0xA0) && !card_probe(0xC0)) {
        strcpy(card_text, errs[E_NOCARD]);
        return 0;
    }
    card_reset();
    strcpy(card_text, "X16-SID");
    p = card_text + 7;
    if (card_request(MB_INFO, 0) == 0 && !memcmp(card_rx + 3, "X16SID", 6)) {
        card_hw = card_rx[9];
        memcpy(card_fw, card_rx + 10, 3);
        strcpy(p, " rev ");
        p[5] = (char)(card_hw >= 'A' && card_hw <= 'Z' ? card_hw : '?');
        strcpy(p + 6, ", firmware ");
        p = dec(p + 17, card_fw[0]);
        *p++ = '.';
        p = dec(p, card_fw[1]);
        *p++ = '.';
        p = dec(p, card_fw[2]);
    }
    strcpy(p, card_base == 0xA0 ? ", IO5" : ", IO6");
    return 1;
}

/*---------------------------------------------------------------
  files: name in the current folder, else the card's root, else the
  folder the player started in (sidplay.s: the root, else its own)
 ---------------------------------------------------------------*/
static unsigned char try_open(const char *dir, const char *name)
{
    strcpy(sb, dir);
    strcat(sb, name);
    return FOpen(sb, 'r');
}

/* name in the card's root, else in /VERANDA (the player's usual folder),
   else in the folder the player started in, else in the current folder
   (sidplay.s: the root, else its own folder) */
static unsigned char open_near(const char *name)
{
    unsigned char h;
    char dir[68];
    h = try_open("//:", name);
    if (h < 2)
        h = try_open("//VERANDA/:", name);
    if (h < 2 && start_dir[1] && strlen(start_dir) < 60) {
        strcpy(dir, "/");
        strcat(dir, start_dir);
        strcat(dir, "/:");
        h = try_open(dir, name);
    }
    if (h < 2)
        h = FOpen((void *)name, 'r');
    return h;
}

/* n bytes of a file into the buffer bank's $A000 */
static unsigned int fread_buf(unsigned char h, unsigned int n)
{
    unsigned char save = BANK;
    unsigned int got;
    MAPBANK(buf_bank);
    got = FRead(h, (void *)0xA000, n);
    MAPBANK(save);
    return got;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* the open file h from its position into the store at `at`, CHUNK at a
   time; flen = its length. 0, E_BIG (more than room), E_UPLOAD */
static unsigned char upload(unsigned char h, unsigned long at, unsigned long room)
{
    unsigned int n;
    flen = 0;
    for (;;) {
        n = fread_buf(h, CHUNK);
        if (!n)
            return E_OK;
        if (flen + n > room)
            return E_BIG;
        put32(card_req, at + flen);
        if (card_store(buf_bank, n))
            return E_UPLOAD;
        flen += n;
        if (n < CHUNK)
            return E_OK;
    }
}

/* the PSID header in the buffer bank: magic, title, author, released,
   clock. 0, or E_NOTSID */
static unsigned char header(unsigned int n)
{
    const unsigned char *h = (const unsigned char *)0xA000;
    unsigned char save = BANK, r = E_NOTSID;
    MAPBANK(buf_bank);
    if (n >= 0x7C && h[1] == 0x53 && h[2] == 0x49 && h[3] == 0x44 && (h[0] == 0x50 || h[0] == 0x52)) {
        is_rsid = h[0] == 0x52;
        memcpy(t_name, h + 0x16, 32);
        memcpy(t_author, h + 0x36, 32);
        memcpy(t_rel, h + 0x56, 32);
        t_name[32] = t_author[32] = t_rel[32] = 0;
        t_version = h[5];
        t_ntsc = 0;
        t_model = 0;
        if (t_version >= 2) {
            t_ntsc = (h[0x77] & 0x0C) == 0x08;          /* NTSC only */
            t_model = (h[0x77] >> 4) & 3;               /* 1 6581, 2 8580 */
        }
        r = E_OK;
    }
    MAPBANK(save);
    return r;
}

/*---------------------------------------------------------------
  SONGLEN.IDX (CommanderSID's tools/songlengths/mksongidx.py): "SLIX",
  version, key length 6, count, data; count records of the key (the first
  6 bytes of the MD5) and a 24-bit offset, by key; at data + offset the
  song count and a word of seconds each
 ---------------------------------------------------------------*/
static unsigned char dbhdr[16], rec[9], md5[16];

static unsigned char db_seek(unsigned char h, unsigned long pos)
{
    return FSeek(h, (unsigned int)pos, (unsigned int)(pos >> 16));
}

/* SONGLEN.IDX there? (db_ok: shown before a tune plays) */
void db_probe(void)
{
    unsigned char h = open_near("SONGLEN.IDX");
    db_ok = 0;
    if (h < 2)
        return;
    if (FRead(h, dbhdr, 16) == 16 && !memcmp(dbhdr, "SLIX", 4) && dbhdr[5] == 6)
        db_ok = 1;
    FClose(h);
}

static void song_lengths(void)
{
    unsigned char h, i;
    unsigned long lo, hi, mid;
    int c;
    have_lens = 0;
    put32(card_req, tune_off);
    put32(card_req + 4, flen);
    if (card_request(MB_MD5, 8))
        return;
    memcpy(md5, card_rx + 3, 16);
    h = open_near("SONGLEN.IDX");
    db_ok = 0;
    if (h < 2)
        return;
    if (FRead(h, dbhdr, 16) != 16 || memcmp(dbhdr, "SLIX", 4) || dbhdr[5] != 6)
        goto out;
    db_ok = 1;
    lo = 0;
    hi = *(unsigned long *)(dbhdr + 8);
    while (lo < hi) {
        mid = (lo + hi) >> 1;
        if (db_seek(h, 16 + mid * 9) || FRead(h, rec, 9) != 9)
            goto out;
        c = memcmp(rec, md5, 6);
        if (!c) {
            mid = *(unsigned long *)(dbhdr + 12) + (rec[6] | (unsigned int)rec[7] << 8 | (unsigned long)rec[8] << 16);
            if (db_seek(h, mid) || FRead(h, &lens_n, 1) != 1)
                goto out;
            if (lens_n > 64)
                lens_n = 64;
            for (i = 0; i < lens_n; ++i)
                if (FRead(h, lens + i, 2) != 2)
                    goto out;
            have_lens = 1;
            goto out;
        }
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
out:
    FClose(h);
}

static void cur_length(void)
{
    cur_len = 0;
    if (have_lens && song && song <= lens_n)
        cur_len = lens[song - 1];
}

/*---------------------------------------------------------------
  playing
 ---------------------------------------------------------------*/
/* PLAY song n (0: the start song) of the stored tune; an RSID: C64_SID
   (the ROMs at 0, the tune at tune_off, its length, the song). The
   reply: status, songs, song. 0, or E_PLAY */
unsigned char play_song(unsigned char n)
{
    unsigned char r;
    memset(card_req, 0, 16);
    if (is_rsid) {
        put32(card_req + 4, tune_off);
        put32(card_req + 8, flen);
        card_req[12] = n;
        r = card_request(MB_C64_SID, 13);
    } else {
        put32(card_req + 4, flen);
        card_req[8] = n;
        r = card_request(MB_PLAY, 9);
    }
    if (r || card_rx[3]) {
        playing = 0;
        irq_play = 0;
        return E_PLAY;
    }
    songs = card_rx[4];
    song = card_rx[5];
    asm("sei");
    el_jif = 0;
    el_sec = 0;
    irq_play = 1;
    asm("cli");
    playing = 1;
    paused = 0;
    cur_length();
    LOGN("SONG", song);
    return E_OK;
}

void stop(void)
{
    if (!card_base)
        return;
    card_request(is_rsid ? MB_C64_STOP : MB_STOP, 0);
    playing = paused = 0;
    irq_play = 0;
    LOG("STOP", 0);
}

/* 1: the card paused (or resumed) the tune */
unsigned char pause_set(unsigned char on)
{
    if (!playing)
        return 0;
    card_req[0] = on;
    if (card_request(MB_PAUSE, 1) || !card_rx[3])
        return 0;
    paused = on;
    irq_play = on ? 2 : 1;
    LOG(on ? "PAUSE" : "RESUME", 0);
    return 1;
}

/* C64ROMS.BIN (20 KB exactly) to the store at 0 */
static unsigned char roms(void)
{
    unsigned char h, r;
    h = open_near("C64ROMS.BIN");
    if (h < 2)
        return E_ROMS;
    r = upload(h, 0, ROMS_LEN + 1);
    FClose(h);
    if (r == E_BIG || (r == E_OK && flen != ROMS_LEN))
        return E_ROMS;
    if (r)
        return r;
    roms_on_card = 1;
    LOG("ROMS", "C64ROMS.BIN");
    return E_OK;
}

/* the tune `name` in the current folder: read, stored, its lengths, its
   start song. 0, or the error's number (errs[]) */
unsigned char play_file(const char *name)
{
    unsigned char h, r;
    unsigned int n;
    unsigned long room;
    if (!card_base)
        return E_NOCARD;
    if (playing)
        stop();
    h = FOpen((void *)name, 'r');
    if (h < 2)
        return E_OPEN;
    n = fread_buf(h, CHUNK);
    r = header(n);
    if (r)
        goto out;
    if (is_rsid) {
        tune_off = RSID_AT;
        if (!roms_on_card) {
            FClose(h);          /* (the ROMs need a stream of their own) */
            r = roms();
            if (r)
                return r;
            h = FOpen((void *)name, 'r');
            if (h < 2)
                return E_OPEN;
            n = fread_buf(h, CHUNK);
        }
    } else {
        tune_off = 0;
        roms_on_card = 0;       /* (the tune goes over them) */
    }
    room = STORE_MAX - tune_off;
    /* the first piece is in the buffer: stored first */
    if (n > room)
        n = (unsigned int)room;
    put32(card_req, tune_off);
    if (card_store(buf_bank, n)) {
        r = E_UPLOAD;
        goto out;
    }
    if (n == CHUNK) {
        r = upload(h, tune_off + CHUNK, room - CHUNK);
        flen += CHUNK;
    } else
        flen = n;
    if (r)
        goto out;
    FClose(h);
    h = 0;
    strcpy(playing_name, name);
    strcpy(playing_dir, (const char *)curDirName);
    LOGN("UPLOADED", flen);
    song_lengths();
    r = play_song(0);
out:
    if (h >= 2)
        FClose(h);
    return r;
}
