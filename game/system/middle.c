/* middle: frame drawing, digits, the pads and random numbers (src/routines/middle). */
#include "game.h"

/* A random number from 0 to limit-1 */
uint16_t randomd0A(uint16_t limit)
{
    if (limit < 0x100) return randoma8_go(limit);
    uint16_t r = RandomWord();
    return (uint16_t)(Mul16U(r, limit) >> 16);
}

/* d0 mod 100 */
uint16_t Mod100(uint16_t d0)
{
    while (d0 >= 100) d0 -= 100;
    return d0;
}

/* randomd0s with the range in d0: a random number from -d0 to d0-1 (clears the carry) */
uint16_t randomd0sd0(uint16_t d0)
{
    uint16_t v = (uint16_t)(randomd0A((uint16_t)(d0 << 1)) - d0);
    cf = 0;
    return v;
}

/* The two digit (BCD) number d0 as two characters at [a1], a blank for a leading zero; returns a1
 * past them */
uint32_t ConvertByteToDigits(uint16_t d0, uint32_t a1)
{
    uint16_t lo = 0x20;
    if ((d0 & 0xFF) >= 10) lo = (uint16_t)(((d0 & 0xFF) >> 4) + 0x30);
    uint16_t w = (uint16_t)(lo | ((d0 & 0xF) + 0x30) << 8);
    if (is_wram(a1)) { w8(wram_off(a1), (uint8_t)w); w8(wram_off((a1 + 1) & 0xFFFFFF), (uint8_t)(w >> 8)); }
    return (a1 & 0xFF0000) | (uint16_t)(a1 + 2);
}

/* d0 as d1 digits, right aligned with leading blanks, in PushWidthBuf (more digits run on to the
 * left); returns a1, the string, and in *a the last quotient (0) */
uint32_t PushNumberWidth(uint16_t d0, uint16_t d1, uint16_t *a)
{
    uint16_t y = (uint16_t)(d1 + 2);
    g.PushWidthBuf[0] = y;
    g.PushWidthBuf[1] = g.PushWidthBuf[2] = g.PushWidthBuf[3] = g.PushWidthBuf[4] = 0x2020;
    uint16_t n = d0;
    do {
        y--;
        uint16_t q = Div16x8U(n, 10);
        w8(RAM_OFF_PushWidthBuf + y, (uint8_t)(n - q * 10 + 0x30));
        n = q;
    } while (n);
    *a = n;
    return RAM_OFF_PushWidthBuf;
}

/* PushNumberWidth, with d0 taken mod 100 when the width d1 is 2 */
uint32_t PushNumberWrap(uint16_t d0, uint16_t d1, uint16_t *a)
{
    if (d1 == 2) while (d0 >= 100) d0 -= 100;   /* (its own copy of Mod100) */
    return PushNumberWidth(d0, d1, a);
}

/* One row of a frame into the text map at printx / printy: the corner [p0], d0 - 2 of the middle
 * tile [p0+2], the corner [p0+4], each plus tmp0; printy steps down. Returns p0 past the three;
 * *tmp2 the row's first map offset, *x the offset before its last corner. */
uint32_t FramerRow(uint32_t p0, uint16_t tmp0, uint16_t d0, uint16_t *tmp2, uint16_t *xo)
{
    uint32_t c = g.printy << 5;
    c = (c & 0xFFFF) + g.printx + (g.printy >> 11 & 1);
    uint32_t a = (c & 0xFFFF) << 1;
    uint16_t x = (uint16_t)((a & 0xFFFF) + g.printm + (a >> 16 & 1));
    *tmp2 = x;
    w16(RAM_OFF_textmap + x, (uint16_t)(m16(p0) + tmp0));
    p0 = (p0 & 0xFF0000) | (uint16_t)(p0 + 2);
    uint16_t t = (uint16_t)(m16(p0) + tmp0), n = (uint16_t)(d0 - 2);
    do { x += 2; w16(RAM_OFF_textmap + x, t); } while (--n);
    p0 = (p0 & 0xFF0000) | (uint16_t)(p0 + 2);
    w16(RAM_OFF_textmap + 2 + x, (uint16_t)(m16(p0) + tmp0));
    g.printy++;
    *xo = x;
    return p0;
}

/* a d0 by d1 frame of the map at map (+6 the top row, +12 the middle, +18 the bottom) with tile
 * base tmp0, the changed range of the text map kept (Framer and its kin) */
static void framer_box(uint32_t map, uint16_t tmp0, uint16_t d0, uint16_t d1)
{
    uint32_t bank = map & 0xFF0000;
    uint16_t t2, x;
    FramerRow(bank | (uint16_t)(map + 6), tmp0, d0, &t2, &x);
    if ((uint16_t)(t2 >> 1) < g.textdirtylo) g.textdirtylo = t2 >> 1;
    d1 -= 2;
    do FramerRow(bank | (uint16_t)(map + 12), tmp0, d0, &t2, &x); while (--d1);
    FramerRow(bank | (uint16_t)(map + 18), tmp0, d0, &t2, &x);
    x = (uint16_t)((uint16_t)(x + 2) >> 1);
    if (x >= g.textdirtyhi) g.textdirtyhi = x;
}

/* Framer with the nine-tile box (FramerNineTileMap) of tile set ninetilechars */
void FramerNineTile(uint16_t d0, uint16_t d1)
{
    framer_box(SYM_FramerNineTileMap, (uint16_t)(g.ninetilechars + g.printfontset), d0, d1);
}

/* Framer with the outline box (FramerOutlineMap) of tile set outlinechars */
void FramerOutline(uint16_t d0, uint16_t d1)
{
    framer_box(SYM_FramerOutlineMap, (uint16_t)(g.outlinechars + g.printfontset), d0, d1);
}

/* Once per frame: each pad's direction (PadDirection of its pad bits), changed buttons and new
 * presses */
void ReadPads(void)
{
    for (int i = 0; i < 5; i++) {
        g.paddir[i] = rom8(SYM_PadDirection + (g.JoyPad[i] >> 8 & 0xF));
    }
    GetNewPresses();
    g.padchangeany = g.padchange[0] | g.padchange[1] | g.padchange[2] | g.padchange[3] | g.padchange[4];
}
