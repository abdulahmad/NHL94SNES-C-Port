/* print (src/routines/print). */
#include "game.h"

/* a byte or word at a 24-bit address (only work RAM takes the store) */
static void mw8(uint32_t a, uint8_t v) { if (is_wram(a)) w8(wram_off(a), v); }

static void mw16(uint32_t a, uint16_t v) { mw8(a, (uint8_t)v); mw8((a + 1) & 0xFFFFFF, (uint8_t)(v >> 8)); }

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

/* the text map entry of the print position, and the carry the 65816 leaves (print's .1) */
static uint16_t print_pos(unsigned *c)
{
    uint16_t a = g.printy;
    for (int i = 0; i < 5; i++) a = asl(a, c);
    a = adc(a, g.printx, c);
    a = asl(a, c);
    return a;
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
