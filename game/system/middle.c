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

/* a byte or word at a 24-bit address (only work RAM takes the store) */
static void mw8(uint32_t a, uint8_t v) { if (is_wram(a)) w8(wram_off(a), v); }
static void mw16(uint32_t a, uint16_t v) { mw8(a, (uint8_t)v); mw8((a + 1) & 0xFFFFFF, (uint8_t)(v >> 8)); }
/* [dp],y: the pointer plus Y, carried into the bank */
static uint32_t ix(uint32_t p, uint16_t y) { return (p + y) & 0xFFFFFF; }
/* adc with the carry in c (and out) */
static uint16_t adc(uint16_t a, uint16_t b, unsigned *c) { uint32_t s = (uint32_t)a + b + *c; *c = s >> 16 & 1; return (uint16_t)s; }
/* asl, the bit shifted out in c */
static uint16_t asl(uint16_t a, unsigned *c) { *c = a >> 15; return (uint16_t)(a << 1); }

/* A random number from 0 to n-1 (d0) */
uint16_t randomd0(uint16_t n) { return randomd0A(n); }

/* A random number from -n to n-1 */
uint16_t randomd0s(uint16_t n) { return (uint16_t)(randomd0A((uint16_t)(n << 1)) - n); }

/* Raise the high end of the changed text map area to a */
void marktexthi(uint16_t a) { if (a >= g.textdirtyhi) g.textdirtyhi = a; }

/* Lower the low end of the changed text map area to a */
void marktextlo(uint16_t a) { if (a < g.textdirtylo) g.textdirtylo = a; }

/* Set the rink scroll from the camera position (horizontal mode has its own) */
void updatescroll(void)
{
    if (g.sfhormode) return;
    g.Hscroll = (int16_t)(g.Hpos - 0x80);
    g.Vscroll = (int16_t)((uint16_t)-g.Vpos - 0x70);
}

/* Key repeat: held buttons d3 repeat (into d1) after a delay; a change d2 restarts it. Returns d1. */
uint16_t ProcessInputWithRepeat(uint16_t d1, uint16_t d2, uint16_t d3)
{
    if (!d3) return d1;
    if (d2) { g.repeatdelay = 0x14; return d1; }
    g.repeatdelay--;
    if (!(g.repeatdelay & 0x8000)) return d1;
    g.repeatdelay = 6;
    return d3;
}

/* Append string [a1] to string [a3] (each a length word, then the bytes; a word is stored per byte) */
void appstring(uint32_t a1, uint32_t a3)
{
    uint16_t s = 2, t = m16(a3);
    while (s != m16(a1)) {
        mw16(ix(a3, t), m16(ix(a1, s)));
        s++; t++;
    }
    mw16(a3, t);
}

/* Pad 1: d0 = direction, d1 = new presses, d2 = changed buttons, d3 = buttons held */
void Readjoy1(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    *d3 = g.JoyPad[0];
    *d2 = g.padchange[0];
    *d1 = g.padnew[0];
    *d0 = g.paddir[0];
}

/* d0 as a decimal string ending at mesareaEnd, no leading zeros; returns a1, the string */
uint32_t PushNumber(uint16_t d0)
{
    uint16_t a1 = mesareaEnd, n = d0;
    do {
        uint16_t q = Div16x8U(n, 10);
        a1--;
        w8(a1, (uint8_t)(n - q * 10 + 0x30));
        n = q;
    } while (n);
    a1 -= 2;
    w16(a1, (uint16_t)(mesareaEnd - a1));
    return a1;
}

/* A random index 0 ... d0-1 into the weight words at nibblebuffer, each as likely as its weight */
uint16_t WeightedRandomSelect(uint16_t d0)
{
    uint16_t a1 = RAM_OFF_nibblebuffer, sum = 0;
    while (!(--d0 & 0x8000)) { sum = (uint16_t)(sum + r16(a1)); a1 += 2; }
    d0 = randomd0(sum);
    do { a1 -= 2; d0 = (uint16_t)(d0 - r16(a1)); } while (!(d0 & 0x8000));
    return (uint16_t)((uint16_t)(a1 - RAM_OFF_nibblebuffer) >> 1);
}

/* Project rink position d0 / d1 (height d2) to the screen: d0 = x, d1 = y ($4E20 off the screen) */
void find3d(uint16_t *d0, uint16_t *d1, uint16_t d2)
{
    uint16_t x = *d0, y = *d1;
    if (g.sfhormode) { uint16_t t = x; x = (uint16_t)-y; y = (uint16_t)(t - 0xC5); }
    else { x = (uint16_t)(x - g.Hpos); y = (uint16_t)(y - g.Vpos); }
    *d0 = x;
    if (((uint16_t)(x - 0xFF6F) & 0x8000) || !((uint16_t)(x - 0x91) & 0x8000)) { *d1 = 0x4E20; return; }
    *d0 = (uint16_t)(x + 0x80);
    uint16_t h = (uint16_t)(d2 * 3);
    h = (uint16_t)((h & 0x8000) | h >> 1);              /* (asl then ror: the sign kept) */
    y = (uint16_t)(h + y);
    *d1 = y;
    if (((uint16_t)(y - 0xFF6F) & 0x8000) || !((uint16_t)(y - 0x91) & 0x8000)) { *d1 = 0x4E20; return; }
    *d1 = (uint16_t)(0x70 - y);
}

/* Fill a d0 by d1 rectangle of the text map at printx / printy with tile d2 (plus the font set) */
void eraser(uint16_t d0, uint16_t d1, uint16_t d2)
{
    d2 = (uint16_t)(d2 + g.printfontset);
    uint16_t row = 0;
    do {
        unsigned c;
        uint16_t a = (uint16_t)(g.printy + row);
        for (int i = 0; i < 5; i++) a = asl(a, &c);
        a = adc(a, g.printx, &c);
        a = asl(a, &c);
        uint16_t x = adc(a, g.printm, &c), n = d0;
        do { w16(RAM_OFF_textmap + x, d2); x += 2; } while (--n);
        row++;
    } while (row != d1);
    unsigned c;
    uint16_t a = g.printy;
    for (int i = 0; i < 5; i++) a = asl(a, &c);
    a = adc(a, g.printx, &c);
    if (a < g.textdirtylo) g.textdirtylo = a;
    g.printy = (uint16_t)(g.printy + d1) & 0x1F;
    a = (uint16_t)((uint16_t)(g.printy - 1) & 0x1F) << 5;
    c = 0;
    a = adc(a, g.printx, &c);
    a = adc(a, d0, &c);
    a--;
    if (a >= g.textdirtyhi) g.textdirtyhi = a;
}

/* The time d0 (seconds) as a minutes:seconds string ending at mesareaEnd; returns a1, the string */
uint32_t PushTime(uint16_t d0)
{
    uint16_t a1 = mesareaEnd;
    uint16_t q = Div16x8U(d0, 10);
    w8(--a1, (uint8_t)(d0 - q * 10 + 0x30));
    uint16_t q2 = Div16x8U(q, 6);
    w8(--a1, (uint8_t)(q - q2 * 6 + 0x30));
    w8(--a1, 0x3A);
    uint16_t q3 = Div16x8U(q2, 10);
    w8(--a1, (uint8_t)(q2 - q3 * 10 + 0x30));
    if (q3) w8(--a1, (uint8_t)(q3 + 0x30));
    a1 -= 2;
    w16(a1, (uint16_t)(mesareaEnd - a1));
    return a1;
}

/* Send d0 tiles from [a0] to video memory, each pixel's colour remapped through the table [a1];
 * returns a0 advanced past them */
uint32_t remap(uint32_t a0, uint32_t a1, uint16_t d0)
{
    do {
        for (int row = 14; row >= 0; row -= 2) {
            uint16_t p01 = m16(ix(a0, (uint16_t)row)), p23 = m16(ix(a0, (uint16_t)(row + 16)));
            for (int px = 0; px < 8; px++) {
                unsigned k = 0;
                if (p23 & 0x8000) k += 8;
                if (p23 & 0x0080) k += 4;
                if (p01 & 0x8000) k += 2;
                if (p01 & 0x0080) k += 1;
                p23 = (uint16_t)((p23 & 0xFF00) << 1 | (uint8_t)(p23 << 1));
                p01 = (uint16_t)((p01 & 0xFF00) << 1 | (uint8_t)(p01 << 1));
                uint8_t v = m8(ix(a1, (uint16_t)k));
                g.RemapOut[row] = (uint8_t)(g.RemapOut[row] << 1 | (v & 1));
                g.RemapOut[row + 1] = (uint8_t)(g.RemapOut[row + 1] << 1 | (v >> 1 & 1));
                g.RemapOut[row + 16] = (uint8_t)(g.RemapOut[row + 16] << 1 | (v >> 2 & 1));
                g.RemapOut[row + 17] = (uint8_t)(g.RemapOut[row + 17] << 1 | (v >> 3 & 1));
            }
        }
        for (int x = 0; x < 0x20; x += 2) io_w16(0x2118, (uint16_t)(g.RemapOut[x] | g.RemapOut[x + 1] << 8));
        a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + 0x20);
    } while (--d0);
    return a0;
}

/* The square root of the 32-bit d0 (a binary search, the range narrowed first) */
uint16_t Sroot(uint32_t d0)
{
    uint16_t hi = (uint16_t)(d0 >> 16), p0, p1;
    if (!hi) {
        p0 = 0; p1 = 0xFF;
        for (;;) {
            uint32_t s = (uint32_t)p0 + p1;
            uint16_t mid = (uint16_t)(s >> 1 | (s & 0x10000) >> 1);
            if (mid == p0) return mid;
            if (Mul8U(mid, mid) < (uint16_t)d0) p0 = mid; else p1 = mid;
        }
    }
    if (hi >= 0x48) { p0 = 0x87C; p1 = 0x1000; if (hi >= 0x96) p0 = 0xC3F; else p1 = 0xC40; }
    else { p0 = 0x100; p1 = 0x87D; if (hi >= 0x16) p0 = 0x4B0; else p1 = 0x4B1; }
    for (;;) {
        uint32_t s = (uint32_t)p0 + p1;
        uint16_t mid = (uint16_t)(s >> 1 | (s & 0x10000) >> 1);
        if (mid == p0) return mid;
        if (d0 >= Mul16U(mid, mid)) p0 = mid; else p1 = mid;
    }
}

/* the text map entry of the print position, and the carry the 65816 leaves (print's .1) */
static uint16_t print_pos(unsigned *c)
{
    uint16_t a = g.printy;
    for (int i = 0; i < 5; i++) a = asl(a, c);
    a = adc(a, g.printx, c);
    a = asl(a, c);
    return a;
}

/* a control record (the negated byte n): palette and priority into printa, the font set, the map */
static void print_control(uint16_t n, int big)
{
    uint16_t pal = n & 0x30;
    if (!big && pal == 0x10) pal += 0x50;
    g.printa = (uint16_t)(pal << 6);
    g.printa |= (uint16_t)((n & 0x40) << 7);
    g.printfontset = r16(RAM_OFF_printfontset + (n & 3) * 2);
    g.printm = (uint16_t)(((uint16_t)((n & 3) - 1) & 0xFF) << 11);
}

/* the changed range of the text map: entry e (the map word index) */
static void print_dirty(uint16_t e)
{
    if (e < g.textdirtylo) g.textdirtylo = e;
    if (e >= g.textdirtyhi) g.textdirtyhi = e;
}

/* Print string [a1] (a length word, then bytes: bit 7 a control record followed by printx and
 * printy, $40 a blank, others SmallFontMap tiles) into the text map at printx / printy; returns a1
 * advanced past it */
uint32_t print(uint32_t a1)
{
    uint16_t y = 2;
    for (;;) {
        unsigned c = 0;
        uint16_t x = print_pos(&c);
        x = adc(x, g.printm, &c);
        uint16_t r3 = (uint16_t)(adc(g.smallfontchars, g.printfontset, &c) | g.printa);
        for (;;) {
            if (y == m16(a1)) return (a1 & 0xFF0000) | (uint16_t)(a1 + y);
            uint16_t b = m16(ix(a1, y));
            if (b & 0x80) {
                print_control((uint16_t)-b, 0);
                y++;
                g.printx = m16(ix(a1, y)) & 0x7F;
                y++;
                g.printy = m16(ix(a1, y)) & 0x7F;
                y++;
                break;
            }
            b &= 0xFF;
            uint16_t t;
            if (b == 0x40) t = (uint16_t)(g.blankchar + g.printfontset);
            else t = (uint16_t)(rom16(SYM_SmallFontMap + 6 + b * 2) + r3);
            if (t != r16(RAM_OFF_textmap + x)) {
                w16(RAM_OFF_textmap + x, t);
                print_dirty(x >> 1);
            }
            g.printx++;
            x += 2;
            y++;
        }
    }
}

/* a big font tile: the map word at BigFontMap + off, its tile bases and printa */
static uint16_t big_tile(uint32_t off)
{
    unsigned c = 0;
    uint16_t t = adc(rom16(SYM_BigFontMap + off), g.BigFontChars, &c);
    return (uint16_t)(adc(t, g.printfontset, &c) | g.printa);
}

/* Print string [a1] with the big font (2 by 2 tiles a character, narrow ones 1 wide; lower case
 * as upper case, a space a blank block) at printx / printy */
void printbig(uint32_t a1)
{
    uint16_t y = 2;
    while (y != m16(a1)) {
        unsigned c = 0;
        uint16_t d1 = print_pos(&c);
        uint16_t b = m16(ix(a1, y));
        if (b & 0x80) {
            print_control((uint16_t)-b, 1);
            y++;
            g.printx = m16(ix(a1, y)) & 0x1F;
            y++;
            g.printy = m16(ix(a1, y)) & 0x1F;
            y++;
            continue;
        }
        b &= 0xFF;
        if (b) {
            if (b >= 0x61 && b < 0x7B) b -= 0x20;
            b -= 0x20;
            if (!b) {
                uint16_t t = big_tile(0x32);
                w16(RAM_OFF_textmap + d1, t);
                w16(RAM_OFF_textmap + 0x40 + d1, t);
                uint16_t e = d1 >> 1;
                if (e < g.textdirtylo) g.textdirtylo = e;
                e += 0x20;
                if (e >= g.textdirtyhi) g.textdirtyhi = e;
            } else {
                int wide = 1;
                uint16_t k = (uint16_t)(int8_t)rom8((SYM_BigFontIndex + b) & 0xFFFFFF);
                if (k & 0x8000) { k = (uint16_t)-k; wide = 0; }
                uint16_t d0 = (uint16_t)(k << 1);
                w16(RAM_OFF_textmap + d1, big_tile(6u + d0));
                uint16_t e = d1 >> 1;
                if (e < g.textdirtylo) g.textdirtylo = e;
                e += 0x21;
                if (e >= g.textdirtyhi) g.textdirtyhi = e;
                w16(RAM_OFF_textmap + 0x40 + d1, big_tile(0x9Eu + d0));
                if (wide) {
                    w16(RAM_OFF_textmap + 2 + d1, big_tile(8u + d0));
                    w16(RAM_OFF_textmap + 0x42 + d1, big_tile(0xA0u + d0));
                    g.printx++;
                }
            }
            g.printx++;
        }
        y++;
    }
}

/* Draw a d0 by d1 frame (FramerMap: corners, edges, the fill) at printx / printy, the changed
 * range of the text map kept */
void Framer(uint16_t d0, uint16_t d1)
{
    uint16_t tmp0 = (uint16_t)((g.Framercset + g.printfontset) | g.printa), t2, x;
    uint32_t bank = SYM_FramerMap & 0xFF0000;
    FramerRow(bank | (uint16_t)(SYM_FramerMap + 6), tmp0, d0, &t2, &x);
    if ((uint16_t)(t2 >> 1) < g.textdirtylo) g.textdirtylo = t2 >> 1;
    d1 -= 2;
    do FramerRow(bank | (uint16_t)(SYM_FramerMap + 12), tmp0, d0, &t2, &x); while (--d1);
    FramerRow(bank | (uint16_t)(SYM_FramerMap + 18), tmp0, d0, &t2, &x);
    x = (uint16_t)((uint16_t)(x + 2) >> 1);
    if (x >= g.textdirtyhi) g.textdirtyhi = x;
}

/* Clear the objects ($76E bytes from Xpos) and set the 16 sort objects up from their start table
 * (9 words each: x, y, height, frame, attribute, collision width and height, assignment, flags),
 * the sort lists in order; then sort them */
void defaultsprites_0(void)
{
    for (int x = 0; x < 0x76E; x += 2) w16(RAM_OFF_Xpos + x, 0);
    g.puckz = 0;
    for (int o = 0; o < 16; o++) {
        uint32_t t = SYM_defaultsprites_0_3 + (uint32_t)o * 18;
        uint16_t y = (uint16_t)(o * 2);
        w16(RAM_OFF_SCnum + y, (uint16_t)o);
        w16(RAM_OFF_oldframe + y, 0xFFFF);
        w16(RAM_OFF_pnum + y, 0xFFFF);
        w16(RAM_OFF_Xpos + y, rom16(t));
        w16(RAM_OFF_Ypos + y, rom16(t + 2));
        w16(RAM_OFF_frame + y, rom16(t + 6));
        w16(RAM_OFF_attribute + y, rom16(t + 8));
        w16(RAM_OFF_radiusx + y, rom16(t + 10));
        w16(RAM_OFF_radiusy + y, rom16(t + 12));
        w16(RAM_OFF_asslist + o * 8, rom16(t + 14));
        w16(RAM_OFF_asscur + y, rom16(t + 14));
        w16(RAM_OFF_pflags + y, rom16(t + 16));
        w16(RAM_OFF_pfnc + y, y == 0x1E);
        w16(RAM_OFF_pfgoal + y, o * 18 < 0x6C);
        if (o * 18 >= 0x6C && o * 18 < 0xD8) w16(RAM_OFF_pfteam + y, 0x34);
        g.OOlist[o] = (uint8_t)(o * 2);
        w16(RAM_OFF_OOlistpos + y, (uint16_t)o);
    }
    w16(RAM_OFF_position + netobj1, 0xFFFF);
    w16(RAM_OFF_position + netobj2, 0xFFFF);
    SprSort();
}

/* Turn the screen black: fade out (when lit), forced blank, PPU registers reset */
void forceblack(void)
{
    if (g.brightness) {
        FadeOut();
        WaitFadeOut();
    }
    BlankScreen();
    ResetPPURegs();
}

/* Queue the changed part of the text map (textdirtylo to textdirtyhi) for transfer to video
 * memory */
void QueueTextDMA(void)
{
    uint16_t lo = g.textdirtylo;
    if (lo & 0x8000) return;
    uint32_t p0 = 0x7E0000u | (uint16_t)((uint16_t)(lo << 1) + (uint16_t)RAM_OFF_textmap);
    if (!g.textdirtyhi) return;
    DMAToVRAM(&p0, (uint16_t)((uint16_t)(g.textdirtyhi - lo + 1) << 1), lo, 0);
    g.textdirtylo = 0xFFFF;
    g.textdirtyhi = 0;
}

/* Queue the rink map rows that the vertical scroll brought on screen (at most $1D: the whole
 * screen) into the BG2 ring of rows, and set BG2's scroll */
void updatescroll_rows(void)
{
    if (g.sfhormode) return;
    uint16_t x = (uint16_t)((uint16_t)(g.Vscroll + 0x170) >> 3);
    uint16_t a = (uint16_t)(x - g.Oldrow);
    if (a) {
        uint16_t tmp1, tmp2;
        g.Oldrow = x;
        if (a & 0x8000) {
            a = (uint16_t)-a;
            if (a >= 0x19) goto all;
            tmp1 = a;
            g.vramrow[0] = (uint16_t)(g.vramrow[0] - tmp1) & 0x1F;
            tmp2 = g.vramrow[0];
        } else {
            if (a >= 0x19) goto all;
            tmp1 = a;
            uint32_t s = (uint32_t)a + g.vramrow[0];
            g.vramrow[0] = (uint16_t)s & 0x1F;
            tmp2 = (uint16_t)(g.vramrow[0] + 0x1D + (s >> 16) - tmp1) & 0x1F;
            x = (uint16_t)(x + 0x1D - tmp1);
        }
        if (0) {
        all:
            tmp1 = 0x1D;
            g.vramrow[0] = 0;
            tmp2 = 0;
        }
        uint16_t t5 = (uint16_t)(x << 5);
        uint32_t s = (uint32_t)(uint16_t)(t5 << 1) + t5 + (t5 >> 15);   /* (the carries of asl, adc, adc) */
        s = (s & 0xFFFF) + (uint16_t)rinkmapbuf + (s >> 16);
        uint16_t tmp0 = (uint16_t)((s & 0xFFFF) + 6 + (s >> 16));
        tmp2 = (uint16_t)((uint16_t)(tmp2 << 5) + 0x400 + (tmp2 >> 11 & 1));
        uint16_t r1 = tmp1;
        do {
            uint32_t p0 = (rinkmapbuf & 0xFF0000) | tmp0;
            DMAToVRAM(&p0, 0x40, tmp2, 0);
            p0 = (p0 & 0xFFFF0000u) | (uint16_t)(tmp0 + 0x40);
            DMAToVRAM(&p0, 0x20, (uint16_t)(tmp2 + 0x400), 0);
            tmp0 = (uint16_t)(tmp0 + 0x60);
            tmp2 = (uint16_t)(((tmp2 + 0x20) & 0x3FF) | 0x400);
        } while (--r1);
    }
    g.BG2HScroll = (uint16_t)(g.Hscroll + 0xC0);
    g.BG2VScroll = (uint16_t)((uint16_t)(g.vramrow[0] << 3) | (g.Vscroll & 7));
}

/* Wait d0 + 1 frames (drawing each one) or until a pad with a team holds a button (ReadjoyA's d3);
 * returns the pad read last (zero when no pad has a team: the direct page's d0-d3 as they were) */
Pad waitxsr(uint16_t d0)
{
    Pad p = { 0, 0, 0, 0 };
    uint16_t n = d0;
    do {
        for (uint16_t x = 0; x < 5; x++)
            if (r16(RAM_OFF_cont1team + 2 * x) && (p = ReadjoyA(x)).held) return p;
        SpriteFrameStart();
        setvideo();
        SpriteFrameEnd();
        uint16_t v = g.vcount[0];
        while (g.vcount[0] == v) hal_wait_interrupt();
        ReadPads();
    } while (!(--n & 0x8000));
    return p;
}
