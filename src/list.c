/*
 * list.c -- the folder's entries: "..", the folders, then the .sid files,
 * each by name (any case), as sidplay.s's read_dir lists device 8. The
 * entries live in RAM banks (64 bytes: the type, the name), order[] is
 * the list's order (a binary insertion as each entry comes, as sidplay.s
 * sorts).
 *
 * Copyright (C) 2026 the CommanderSID project (Marcin Spoczynski).
 * GPL-3.0-or-later (see COPYING).
 */
#include "sidp.h"

#define ENT_BANKS  (ENT_MAX / 128)
unsigned char ent_bank;
unsigned int n_ent, n_sids, top, sel;
char cur[64];
static unsigned int order[ENT_MAX];
static unsigned char dent[3 + 64];      /* DirNext's entry: type, size, name */
static char key[64];

static char *slot(unsigned int i)       /* (maps its bank: the caller puts its own back) */
{
    MAPBANK((unsigned char)(ent_bank + (i >> 7)));
    return (char *)(0xA000 + ((i & 127) << 6));
}

void ent_get(unsigned int i)
{
    unsigned char save = BANK;
    memcpy(cur, slot(order[i]), 64);
    MAPBANK(save);
}

static unsigned char up(unsigned char c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

/* the entry in slot s against key (type, then the name, any case):
   <0, 0, >0 */
static signed char cmp(unsigned int s)
{
    const unsigned char *a = (const unsigned char *)slot(s), *b = (const unsigned char *)key;
    unsigned char x, y;
    for (;;) {
        x = up(*a++);
        y = up(*b++);
        if (x != y)
            return x < y ? -1 : 1;
        if (!x)
            return 0;
    }
}

/* the type's place in the list: "..", folders, tunes */
static unsigned char rank(unsigned char t)
{
    return t == T_UP ? 1 : t == T_DIR ? 2 : 3;
}

static void put(unsigned char type, const char *name)
{
    unsigned char save = BANK;
    unsigned int lo = 0, hi = n_ent, mid;
    char *p;
    key[0] = (char)rank(type);
    strncpy(key + 1, name, 62);
    key[63] = 0;
    while (lo < hi) {
        mid = (lo + hi) >> 1;
        if (cmp(order[mid]) <= 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    p = slot(n_ent);
    memcpy(p, key, 64);
    MAPBANK(save);
    memmove(order + lo + 1, order + lo, (n_ent - lo) * sizeof order[0]);
    order[lo] = n_ent;
    ++n_ent;
}

unsigned char at_root(void)
{
    return curDirName[0] == '/' && curDirName[1] == 0;
}

unsigned char is_sid(const char *n)
{
    unsigned char l = (unsigned char)strlen(n);
    return l > 4 && n[l - 4] == '.' && up(n[l - 3]) == 'S' && up(n[l - 2]) == 'I' && up(n[l - 1]) == 'D';
}

void list_read(void)
{
    const char *nm = (const char *)dent + 3;
    unsigned int i;
    unsigned char save = BANK;
    n_ent = n_sids = 0;
    if (!at_root())
        put(T_UP, "..");
    if (DirOpen("*") == 0)
        while (DirNext(dent) == 0 && n_ent < ENT_MAX) {
            if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0)))
                continue;
            if (dent[0] == 'D')
                put(T_DIR, nm);
            else if (dent[0] == 'F' && is_sid(nm)) {
                put(strlen(nm) >= 62 ? T_LONG : T_SID, nm);
                ++n_sids;
            }
        }
    /* the rank back to the type letter */
    for (i = 0; i < n_ent; ++i) {
        char *p = slot(i);
        p[0] = p[0] == 1 ? T_UP : p[0] == 2 ? T_DIR : (strlen(p + 1) >= 62 ? T_LONG : T_SID);
    }
#ifdef SIDP_LOG
    for (i = 0; i < n_ent; ++i) {
        ent_get(i);
        cur[1 + 40] = 0;
        LOG("ENTRY", cur);
    }
#endif
    MAPBANK(save);
    top = sel = 0;
}

/* the tune named name, or NONE */
unsigned int find_entry(const char *name)
{
    unsigned int i;
    for (i = 0; i < n_ent; ++i) {
        ent_get(i);
        if ((cur[0] == T_SID || cur[0] == T_LONG) && !strcmp(cur + 1, name))
            return i;
    }
    return NONE;
}
