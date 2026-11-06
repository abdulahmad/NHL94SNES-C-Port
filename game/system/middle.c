/* middle: arithmetic, random numbers, the heap and its handles, the pads and the multitap, frame
 * drawing and digits (src/routines/middle).
 *
 * The multiplies and divides use the console's multiplier and divider; here they are the same
 * arithmetic in C, each with the original's operand widths (an "8-bit" operand is the low byte
 * of the register), signs and odd results (Div32U's $7FFF when the quotient would not fit its
 * test, which is stricter than fitting). cf is the 65816's carry where a routine's caller goes on
 * using it (an adc with no clc after the call): the multiplies set it as the original leaves it. */
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

/* randomas with the range in d0: a random number from -d0 to d0-1 (clears the carry) */
uint16_t randomd0s(uint16_t d0)
{
    uint16_t v = (uint16_t)(randomd0A((uint16_t)(d0 << 1)) - d0);
    cf = 0;
    return v;
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

/* [dp],y: the pointer plus Y, carried into the bank */
uint32_t ix(uint32_t p, uint16_t y) { return (p + y) & 0xFFFFFF; }
/* adc with the carry in c (and out) */
uint16_t adc(uint16_t a, uint16_t b, unsigned *c) { uint32_t s = (uint32_t)a + b + *c; *c = s >> 16 & 1; return (uint16_t)s; }
/* asl, the bit shifted out in c */
uint16_t asl(uint16_t a, unsigned *c) { *c = a >> 15; return (uint16_t)(a << 1); }

/* A random number from 0 to n-1 (d0) */
uint16_t randomd0(uint16_t n) { return randomd0A(n); }

/* A random number from -n to n-1 */
uint16_t randomas(uint16_t n) { return (uint16_t)(randomd0A((uint16_t)(n << 1)) - n); }

/* Raise the high end of the changed text map area to a */
void marktexthi(uint16_t a) { if (a >= g.textdirtyhi) g.textdirtyhi = a; }

/* Lower the low end of the changed text map area to a */
void marktextlo(uint16_t a) { if (a < g.textdirtylo) g.textdirtylo = a; }

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

/* Pad 1: d0 = direction, d1 = new presses, d2 = changed buttons, d3 = buttons held */
void Readjoy1(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    *d3 = g.JoyPad[0];
    *d2 = g.padchange[0];
    *d1 = g.padnew[0];
    *d0 = g.paddir[0];
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

/* Turn the screen black: fade out (when lit), forced blank, PPU registers reset */
void forceblack(void)
{
    if (g.brightness) {
        forcefade();
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

/* Wait d0 + 1 frames (drawing each one) or until a pad with a team holds a button (ReadjoyA's d3);
 * returns the pad read last (zero when no pad has a team: the direct page's d0-d3 as they were) */
Pad waitx(uint16_t d0)
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

/* 8 x 8 bits into 16 (A), X signed: A times X's magnitude, negated for a negative X (the carry
 * set, or for a negative X set only when the product is 0) */
uint16_t Mul8XS(uint16_t a, uint16_t x)
{
    cf = 1;                                             /* (cpx #0) */
    if (!(x & 0x8000)) return Mul8U(a, x);
    uint16_t p = (uint16_t)((a & 0xFF) * ((uint16_t)-x & 0xFF));
    cf = p == 0;                                        /* (0 - p) */
    return (uint16_t)-p;
}

/* 8 x 8 bits into 16 (A), both signed: the magnitudes multiplied, negated when the signs differ
 * (two negatives leave the carry as it came) */
uint16_t Mul8S(uint16_t a, uint16_t x)
{
    if (!(a & 0x8000)) return Mul8XS(a, x);
    if (!(x & 0x8000)) {
        uint16_t p = (uint16_t)((x & 0xFF) * ((uint16_t)-a & 0xFF));
        cf = p == 0;
        return (uint16_t)-p;
    }
    return (uint16_t)(((uint16_t)-x & 0xFF) * ((uint16_t)-a & 0xFF));   /* (the carry as it came) */
}

/* X (16 bits) x A (its low byte) into r2 ($08-$0B): unsigned; X signed; both signed. A zero
 * operand (A or X as a whole) leaves the carry as it came; a product clears it, but a negated
 * product sets it when the product is 0. */
uint32_t Mul16x8U(uint16_t x, uint16_t a)
{
    if (!a || !x) return 0;
    cf = 0;
    return (uint32_t)x * (a & 0xFF);
}

static uint32_t neg_product(uint32_t p) { cf = p == 0; return (uint32_t)-(int32_t)p; }

/* X x A (low byte) into r2, X signed: X's magnitude times A, negated for a negative X */
uint32_t Mul16x8XS(uint16_t x, uint16_t a)
{
    if (!a || !x) return 0;
    if (!(x & 0x8000)) return Mul16x8U(x, a);
    return neg_product(Mul16x8U((uint16_t)-x, a));
}

/* X x A (low byte) into r2, both signed: the magnitudes multiplied, negated when the signs differ */
uint32_t Mul16x8S(uint16_t x, uint16_t a)
{
    if (!(a & 0x8000)) return Mul16x8XS(x, a);
    if (!x) return 0;
    uint32_t p = Mul16x8U(x & 0x8000 ? (uint16_t)-x : x, (uint16_t)-a);
    return x & 0x8000 ? p : neg_product(p);
}

/* A x X into r2 ($08-$0B): unsigned; X signed; both signed (the carry as for Mul16x8U) */
uint32_t Mul16U(uint16_t a, uint16_t x)
{
    if (!a || !x) return 0;
    cf = 0;
    return (uint32_t)a * x;
}

/* A x X into r2, X signed: A times X's magnitude, negated for a negative X */
uint32_t Mul16XS(uint16_t a, uint16_t x)
{
    if (!a || !x) return 0;
    if (!(x & 0x8000)) return Mul16U(a, x);
    return neg_product(Mul16U(a, (uint16_t)-x));
}

/* A x X into r2, both signed: the magnitudes multiplied, negated when the signs differ */
uint32_t Mul16S(uint16_t a, uint16_t x)
{
    if (!(a & 0x8000)) return Mul16XS(a, x);
    if (!x) return 0;
    uint32_t p = Mul16U((uint16_t)-a, x & 0x8000 ? (uint16_t)-x : x);
    return x & 0x8000 ? p : neg_product(p);
}

/* X x A (low byte) / 256, X signed: the product of X's magnitude / 256, negated for a negative X
 * (so it rounds toward 0) */
uint16_t Mul16x8HiXS(uint16_t x, uint16_t a)
{
    if (!a || !x) return 0;
    if (!(x & 0x8000)) return Mul16x8HiU(x, a);
    return (uint16_t)-Mul16x8HiU((uint16_t)-x, a);
}

/* X x A (low byte) / 256, both signed: the magnitudes' product / 256, negated when signs differ */
uint16_t Mul16x8HiS(uint16_t x, uint16_t a)
{
    if (!(a & 0x8000)) return Mul16x8HiXS(x, a);
    if (!x) return 0;
    uint16_t p = Mul16x8HiU(x & 0x8000 ? (uint16_t)-x : x, (uint16_t)-a);
    return x & 0x8000 ? p : (uint16_t)-p;
}

/* r2 ($08-$0B) / r1 ($04) into r0, unsigned. $7FFF when the dividend's top byte is not below the
 * divisor's high byte, or, for a byte divisor (0 too), when the dividend's third byte is not below
 * it; otherwise the exact quotient. */
uint16_t Div32U(uint32_t n, uint16_t d)
{
    unsigned b3 = n >> 24, b2 = n >> 16 & 0xFF, dh = d >> 8, dl = d & 0xFF;
    if (b3) {
        if (b3 >= dh) return 0x7FFF;
    } else if (!dh) {
        if (!dl || (b2 && b2 >= dl)) return 0x7FFF;
    }
    return (uint16_t)(n / d);
}

/* r2 / r1 into r0, signed: the magnitudes divided, negated when the signs differ ($7FFF -> $8001) */
uint16_t Div32S(uint32_t n, uint16_t d)
{
    int neg = 0;
    if (d & 0x8000) { d = (uint16_t)-d; neg ^= 1; }
    if (n & 0x80000000u) { n = (uint32_t)-(int32_t)n; neg ^= 1; }
    uint16_t q = Div32U(n, d);
    return neg ? (uint16_t)-q : q;
}

/* r2 (16 bits) / r1 into r0, signed: the magnitudes divided, negated when the signs differ ($7FFF
 * for 0, $8001 when negated) */
uint16_t Div16S(uint16_t n, uint16_t d)
{
    int neg = 0;
    if (d & 0x8000) { d = (uint16_t)-d; neg ^= 1; }
    if (n & 0x8000) { n = (uint16_t)-n; neg ^= 1; }
    uint16_t q = Div16U(n, d);
    return neg ? (uint16_t)-q : q;
}

/* X / A (low byte) into A, X signed: X's magnitude divided by the unsigned byte, negated for a
 * negative X (the collision and hat trick speeds: a distance over a length or a frame count) */
uint16_t DivX8S(uint16_t x, uint16_t a)
{
    if (!(x & 0x8000)) return DivX8U(x, a);
    return (uint16_t)-DivX8U((uint16_t)-x, a);
}

/* A random word: the sum of two 16-bit shift-register generators (seeded $A5A5 / $B4B4 at 0) */
uint16_t RandomWord(void)
{
    uint16_t s = g.seed[0] ? g.seed[0] : 0xA5A5;
    g.seed[0] = (uint16_t)(s << 1 ^ (s & 0x8000 ? 0x1D87 : 0));
    uint16_t t = g.seed[1] ? g.seed[1] : 0xB4B4;
    g.seed[1] = (uint16_t)(t << 1 ^ (t & 0x8000 ? 0x1D87 : 0));
    return (uint16_t)(g.seed[1] + g.seed[0]);
}

/* The approximate length of (x, y): the longer side plus a quarter of the shorter, or, when the
 * shorter is more than half the longer, plus 3/8 of it (in 16-bit words, unsigned) */
uint16_t ApproxLength(uint16_t x, uint16_t y)
{
    if (x & 0x8000) x = (uint16_t)-x;
    if (y & 0x8000) y = (uint16_t)-y;
    return ApproxLength_abs(x, y);
}

/* ApproxLength's second half, for x and y already positive (entered directly by the puck distance
 * and the replay, which have the magnitudes) */
uint16_t ApproxLength_abs(uint16_t x, uint16_t y)
{
    uint16_t lo = x, hi = y;                            /* (r3 the shorter, r3+2 the longer) */
    if (y < x) { lo = y; hi = x; }
    if ((hi >> 1) < lo) return (uint16_t)(((uint16_t)((lo >> 1) + lo) >> 2) + hi);
    return (uint16_t)((lo >> 2) + hi);
}

/* Pad n: direction, new presses, changed buttons, buttons held */
Pad ReadjoyA(uint16_t n)
{
    uint16_t x = (uint16_t)(n << 1);
    Pad p = { r16(RAM_OFF_paddir + x), r16(RAM_OFF_padnew + x), r16(RAM_OFF_padchange + x), r16(RAM_OFF_JoyPad + x) };
    return p;
}

/* Wait until every pad that has a side (cont1team) has no button down */
void WaitSidePadsRelease(void)
{
again:
    for (int x = 8; x >= 0; x -= 2) {
        if (!r16(RAM_OFF_cont1team + x)) continue;
        if (wait_r16(RAM_OFF_JoyPad + x)) { wait_tick(); goto again; }
    }
}

/* Wait until every standard pad (low nibble 0) has no button down */
void WaitPadsRelease(void)
{
again:
    for (int x = 8; x >= 0; x -= 2) {
        uint16_t j = wait_r16(RAM_OFF_JoyPad + x);
        if (j & 0x000F) continue;
        if (j & 0xFFF0) { wait_tick(); goto again; }
    }
}

/* Copy x bytes from [p0] to [p1], from the top down (an odd byte first); x = 2 or 3 goes round the
 * whole 64 KB */
void MemCopy(uint32_t p0, uint32_t p1, uint16_t x)
{
    uint16_t y = x;
    if (x & 1) { y--; mset8(p1 + y, m8(p0 + y)); }
    y = (uint16_t)(y - 2);
    do {
        mset16(p1 + y, m16(p0 + y));
        y = (uint16_t)(y - 2);
    } while (y);
    mset16(p1, m16(p0));
}

/* Free handle a unless it is a heap block (bank $7E or $7F) */
void DisposePtrHandle(uint16_t a)
{
    if (a < 0x2000 || a >= 0x2100) return;
    uint16_t b = r16(a + 2);
    if (b == 0x7E || b == 0x7F) return;
    w16(a, 0);
    w16(a + 2, 0);
}

/* Handle a's address (low word) and bank (high word); 0 for a bad handle */
uint32_t DerefHandle(uint16_t a)
{
    if (a < 0x2000 || a >= 0x2100 || !(r16(a) | r16(a + 2))) return 0;
    return r16(a) | (uint32_t)r16(a + 2) << 16;
}

/* The handle of address a in bank x, 0 if none */
uint16_t FindHandle(uint16_t a, uint16_t x)
{
    for (uint16_t h = 0x2000; h != 0x2100; h += 4)
        if (r16(h) == a && r16(h + 2) == x) return h;
    return 0;
}

/* A handle for fixed memory at a in bank x; 0 if none is free */
uint16_t NewPtrHandle(uint16_t a, uint16_t x)
{
    for (uint16_t h = 0x2000; h != 0x2100; h += 4)
        if (!(r16(h) | r16(h + 2))) { w16(h, a); w16(h + 2, x); return h; }
    return 0;
}

/* sum plus the sizes of the free blocks (flags 0) of the heap chain in p0's bank from block y
 * (a header: size, flags, next) */
uint32_t HeapFreeBank(uint32_t p0, uint16_t y, uint32_t sum)
{
    for (;;) {
        if (!m16(p0 + (uint16_t)(y + 2))) sum += m16(p0 + y);
        y = m16(p0 + (uint16_t)(y + 4));
        if (!y) return sum;
    }
}

/* Wait until no button is held on pad 1 */
void WaitNoButtons(void)
{
    while (wait_r16(RAM_OFF_JoyPad) & 0xFFF0) wait_tick();
}

/* For each of the 5 pads: the buttons that changed, those newly pressed, then the buttons held */
void GetNewPresses(void)
{
    for (int i = 0; i < 5; i++) {
        g.padchange[i] = g.JoyPad[i] ^ g.padlast[i];
        g.padnew[i] = g.padchange[i] & g.JoyPad[i];
    }
    for (int i = 0; i < 5; i++) {
        g.padlast[i] = g.JoyPad[i];
    }
}

/* Fill x bytes at [p1] with the word a (from the top down; x = 2 goes round the 64 KB); _t1 adds
 * a to each word */
void MemFill(uint32_t p1, uint16_t x, uint16_t a)
{
    uint16_t y = (uint16_t)(x - 2);
    do { mset16(p1 + y, a); y = (uint16_t)(y - 2); } while (y);
    mset16(p1, a);
}

/* MemFill's second entry: add the word a to each word of x bytes at [p1] (from the top down) */
void MemFill_t1(uint32_t p1, uint16_t x, uint16_t a)
{
    uint16_t y = (uint16_t)(x - 2);
    do { mset16(p1 + y, (uint16_t)(m16(p1 + y) + a)); y = (uint16_t)(y - 2); } while (y);
    mset16(p1, (uint16_t)(m16(p1) + a));
}

/* OR the word a into x bytes at [p1]; _t1 ANDs it */
void MemOr(uint32_t p1, uint16_t x, uint16_t a)
{
    uint16_t y = (uint16_t)(x - 2);
    do { mset16(p1 + y, m16(p1 + y) | a); y = (uint16_t)(y - 2); } while (y);
    mset16(p1, m16(p1) | a);
}

/* MemOr's second entry: AND the word a into x bytes at [p1] */
void MemOr_t1(uint32_t p1, uint16_t x, uint16_t a)
{
    uint16_t y = (uint16_t)(x - 2);
    do { mset16(p1 + y, m16(p1 + y) & a); y = (uint16_t)(y - 2); } while (y);
    mset16(p1, m16(p1) & a);
}

/* sin / cos of a (1024 steps a circle, $0100 = 1), from the quarter-wave SineTable */
uint16_t Sin(uint16_t a)
{
    a &= 0x03FF;
    if (a >= 0x0200) {
        a = (uint16_t)(a - 0x0200);
        if (a >= 0x0100) a = (uint16_t)(0x0200 - a);
        return (uint16_t)(0 - rom16(SYM_SineTable + (uint16_t)(a << 1)));
    }
    if (a >= 0x0100) a = (uint16_t)(0x0200 - a);
    return rom16(SYM_SineTable + (uint16_t)(a << 1));
}

/* n / d into the result (32 bits), the remainder in *rem, unsigned, by shift and subtract (the
 * divisor is first shifted until its top bit is set: d = 0 never ends) */
uint32_t Div32x32U(uint32_t n, uint32_t d, uint32_t *rem)
{
    uint32_t q = 0;
    int16_t y = 0;
    while (!(d & 0x80000000u)) { y++; d <<= 1; }
    do {
        unsigned c = n >= d;
        if (c) n -= d;
        q = q << 1 | c;
        d >>= 1;
    } while (--y >= 0);
    *rem = n;
    return q;
}

/* The heaps: start them at x in bank $7E (at least $E68C) and y in bank $7F, one free block each
 * (a header: size, flags, next, previous), all handles free */
void HeapInit(uint16_t x, uint16_t y)
{
    if (x < 0xE68C) x = 0xE68C;
    g.HeapStart7E = x;
    g.HeapStart7F = y;
    w16(x + 6, 0xFFFF); w16(x + 4, 0); w16(x + 2, 0);
    w16(x, (uint16_t)(0xFFF8 - g.HeapStart7E));
    uint32_t b = 0x10000 + y;
    w16(b + 6, 0xFFFF); w16(b + 4, 0); w16(b + 2, 0);
    w16(b, (uint16_t)(0xFFF8 - g.HeapStart7F));
    for (int k = 0xFE; k >= 0; k -= 2) w16(RAM_OFF_HandleTable + k, 0);
}

/* The free bytes of both heaps (X high word, A low) */
uint32_t HeapFree(void)
{
    uint32_t s = HeapFreeBank(0x7E0000, g.HeapStart7E, 0);
    return HeapFreeBank(0x7F0000, g.HeapStart7F, s);
}

/* Handle a's block header (8 bytes before its address) in its heap's chain: *p0 the bank and the
 * header's offset, 0 when not a heap handle or not in the chain */
static uint16_t handle_block(uint16_t a, uint32_t *p0)
{
    if (a < 0x2000 || a >= 0x2100) return 0;
    uint16_t bank = r16(a + 2), addr = r16(a), y;
    if (bank == 0x7E) y = g.HeapStart7E;
    else if (bank == 0x7F) { y = g.HeapStart7F; }
    else return 0;
    uint16_t want = (uint16_t)(addr - 8);
    *p0 = (uint32_t)(bank & 0xFF) << 16;
    while (y != want) {
        y = m16(*p0 + (uint16_t)(y + 4));
        if (!y) return 0;
    }
    return (uint16_t)(y + 2);
}

/* Set / clear the locked bit ($4000) of handle a's block (a free block is left alone; clearing
 * keeps only the used bit) */
void LockHandle(uint16_t a)
{
    uint32_t p0;
    uint16_t f = handle_block(a, &p0);
    if (!f || !m16(p0 + f)) return;
    mset16(p0 + f, m16(p0 + f) | 0x4000);
}

/* Clear the locked bit of handle a's block, keeping only the used bit ($8000) */
void UnlockHandle(uint16_t a)
{
    uint32_t p0;
    uint16_t f = handle_block(a, &p0);
    if (!f || !m16(p0 + f)) return;
    mset16(p0 + f, m16(p0 + f) & 0x8000);
}

/* a * y (y a byte) / 128 into the result and MulProd+1: the mode 7 multiplier, or with bit 6 of
 * Mode7InUse a shift-and-add of 7 steps (the 16-bit MulPlier: its high byte shifts down too) */
uint16_t MulFrac16(uint16_t a, uint8_t y)
{
    if (!(g.Mode7InUse & 0x40)) {
        io_w8(0x211B, (uint8_t)a);
        io_w8(0x211B, (uint8_t)(a >> 8));
        io_w8(0x211C, y);
        uint16_t v = (uint16_t)(io_r16(0x2135) << 1);
        w16(RAM_OFF_StreamLoop + 1, v);
        return v;
    }
    if (!y || !a) return 0;
    g.MulPlier[0] = (uint16_t)((g.MulPlier[0] & 0xFF00) | y);
    uint32_t cand = a, prod = 0;
    for (uint8_t k = 7; k; k--) {
        cand <<= 1;
        uint16_t m = g.MulPlier[0];
        if (!m) break;
        g.MulPlier[0] = m >> 1;
        if (m & 1) prod += cand;
    }
    g.MulCand[0] = (uint16_t)cand; g.MulCand[1] = (uint16_t)(cand >> 16);
    w16(RAM_OFF_StreamLoop, (uint16_t)prod); w16(RAM_OFF_StreamLoop + 2, (uint16_t)(prod >> 16));
    return r16(RAM_OFF_StreamLoop + 1);
}

/* c's low byte times its high byte (7 bits) / 128: the mode 7 multiplier, or with bit 6 of
 * Mode7InUse a shift-and-add (each set bit adds the multiplicand and the carry, 1). The 16-bit
 * accumulator as the 65816 leaves it: the result in the low byte. */
uint16_t MulFrac8(uint16_t c)
{
    if (!(g.Mode7InUse & 0x40)) {
        uint8_t a = (uint8_t)(c << 1);
        io_w8(0x211B, a);
        io_w8(0x211B, 0);
        io_w8(0x211C, (uint8_t)(c >> 8));
        return (uint16_t)(a << 8 | io_r8(0x2135));
    }
    uint16_t m2 = (uint16_t)(c << 1);
    g.MulCand[1] = m2;
    g.MulCand[0] = (uint16_t)(m2 >> 1) & 0xFF;
    uint16_t acc = 0;
    for (int k = 0; k < 7; k++) {
        if (k) acc = (uint16_t)(acc << 1);
        unsigned bit = g.MulCand[1] >> 15;
        g.MulCand[1] = (uint16_t)(g.MulCand[1] << 1);
        if (bit) acc = (uint16_t)(acc + g.MulCand[0] + 1);
    }
    acc = (uint16_t)(acc << 1);
    return (uint16_t)(acc >> 8 | acc << 8);
}

/* The first free block of at least want bytes in the heap chain of bank (bank << 16) from header y,
 * taken with flags | $8000 (split when 16 bytes or more are left: the new free block's header after
 * the data, its next the old next; the old next's previous is not updated); its data address,
 * $FFFF for none */
uint16_t HeapAllocBlock(uint32_t bank, uint16_t y, uint16_t want, uint16_t flags)
{
    for (;;) {
        uint16_t size = m16(bank + y);
        y = (uint16_t)(y + 2);
        if (size >= want && !m16(bank + y)) break;
        uint16_t next = m16(bank + (uint16_t)(y + 2));
        if (!next) return 0xFFFF;
        y = next;
    }
    uint16_t hdr = (uint16_t)(y - 2), size = m16(bank + hdr);
    uint16_t rem = (uint16_t)(size - want);
    if (!rem || rem < 0x0010) {
        mset16(bank + (uint16_t)(hdr + 2), flags | 0x8000);
        return (uint16_t)(hdr + 8);
    }
    mset16(bank + hdr, want);
    mset16(bank + (uint16_t)(hdr + 2), flags | 0x8000);
    uint16_t old = m16(bank + (uint16_t)(hdr + 4));
    uint32_t s = (uint32_t)hdr + want;
    uint16_t nb = (uint16_t)(s + 8 + (s >> 16));
    mset16(bank + (uint16_t)(hdr + 4), nb);
    mset16(bank + nb, (uint16_t)(rem - 8));
    mset16(bank + (uint16_t)(nb + 2), 0);
    mset16(bank + (uint16_t)(nb + 4), old);
    mset16(bank + (uint16_t)(nb + 6), hdr);
    return (uint16_t)(hdr + 8);
}

/* Draw string [a1] (its length + 2 at +0, the characters from +2) with the letter sprites at x, y:
 * letters and digits, a space 6 pixels, other characters nothing */
void PrintLetterSprites(uint32_t a1, uint16_t x, uint16_t y)
{
    uint32_t s = a1 & 0xFFFFFF;
    g.letterx[0] = x;
    w16(RAM_OFF_lettery, y);
    for (uint16_t k = 2; k != m16(s); k++) {
        uint16_t ch = m8(s + k), idx, adv;
        if (ch == 0x20) adv = 6;
        else {
            if (ch >= 0x30 && ch < 0x3A) idx = (uint16_t)(ch - 0x30 + 0x1A);
            else if (ch >= 0x61 && ch < 0x7B) idx = (uint16_t)(ch - 0x61);
            else if (ch >= 0x41 && ch < 0x5B) idx = (uint16_t)(ch - 0x41);
            else continue;
            DrawSprite(0x2000, g.letterx[0], r16(RAM_OFF_lettery), idx, SYM_LetterSprites);
            adv = rom8(SYM_LetterSpriteWidths + idx);
        }
        g.letterx[0] = (uint16_t)(adv + g.letterx[0]);
    }
}

/* ---- the heap: handles, blocks (a header of words: size, flags, next, previous) ---- */

/* Handles: 64 entries of an address word and a bank word at $2000-$20FF (0, 0 = free) */

static uint16_t hr(uint32_t bank, uint16_t y) { return m16(bank + y); }
static void hw(uint32_t bank, uint16_t y, uint16_t v) { mset16(bank + y, v); }
/* (block y's link words: next at +4, previous at +6) */
#define NEXT(y) ((uint16_t)((y) + 4))
#define PREV(y) ((uint16_t)((y) + 6))

/* Free handle a and its block (8 bytes before its address in the heap of its bank), joined with a
 * free block after it, then with a free block before it */
void DisposeHandle(uint16_t a)
{
    if (a < 0x2000 || a >= 0x2100) return;
    uint16_t bank = r16(a + 2), addr = r16(a), y;
    w16(a, 0);
    w16(a + 2, 0);
    if (bank == 0x7E) y = g.HeapStart7E;
    else if (bank == 0x7F) y = g.HeapStart7F;
    else return;
    uint32_t b = (uint32_t)(bank & 0xFF) << 16;
    uint16_t hdr = (uint16_t)(addr - 8);
    while (y != hdr) {
        y = hr(b, NEXT(y));
        if (!y) return;
    }
    uint16_t nx = hr(b, NEXT(hdr));
    if (nx && !hr(b, (uint16_t)(nx + 2))) {
        uint16_t size = hr(b, nx), after = hr(b, NEXT(nx));
        if (after) hw(b, PREV(after), hdr);
        uint32_t s = (uint32_t)hr(b, hdr) + size;
        hw(b, hdr, (uint16_t)(s + 8 + (s >> 16 & 1)));
        hw(b, NEXT(hdr), after);
    }
    hw(b, (uint16_t)(hdr + 2), 0);
    uint16_t pv = hr(b, PREV(hdr));
    if (pv == 0xFFFF || hr(b, (uint16_t)(pv + 2))) return;
    uint16_t size = hr(b, hdr), after = hr(b, NEXT(hdr));
    if (after) hw(b, PREV(after), pv);
    uint32_t s = (uint32_t)hr(b, pv) + size;
    hw(b, pv, (uint16_t)(s + 8 + (s >> 16 & 1)));
    hw(b, NEXT(pv), after);
}

/* Join the free blocks of the chain in bank (bank << 16) from header y and move each used unlocked
 * block after a free one down into it (its data by an ascending byte move), the free space moving
 * up; the handle whose address word is the old data address points at the new one (whatever its
 * bank) */
void HeapCompactBank(uint32_t bank, uint16_t y)
{
    for (;;) {
        if (hr(bank, (uint16_t)(y + 2))) {
        next:
            y = hr(bank, NEXT(y));
            if (!y) return;
            continue;
        }
    free:;
        uint16_t r0 = y, fsize = hr(bank, y);
        uint16_t nx = hr(bank, NEXT(y));
        if (!nx) return;
        uint16_t nf = hr(bank, (uint16_t)(nx + 2));
        if (!nf) {
            uint16_t size = hr(bank, nx), after = hr(bank, NEXT(nx));
            if (after) hw(bank, PREV(after), r0);
            uint32_t s = (uint32_t)size + 8;
            hw(bank, r0, (uint16_t)(s + fsize + (s >> 16)));
            hw(bank, NEXT(r0), after);
            y = r0;
            goto free;
        }
        if (nf & 0x4000) { y = nx; goto next; }
        uint16_t size = hr(bank, nx), after = hr(bank, NEXT(nx));
        uint32_t s = (uint32_t)r0 + 8;
        uint16_t nfree = (uint16_t)(s + size + (s >> 16));
        if (after) hw(bank, PREV(after), nfree);
        hw(bank, r0, size);
        hw(bank, (uint16_t)(r0 + 2), 0x8000);
        hw(bank, NEXT(r0), nfree);
        uint16_t src = (uint16_t)(nx + 8), dst = (uint16_t)(r0 + 8);
        for (uint32_t k = 0; k <= (uint16_t)(size - 1); k++) mset8(bank | (uint16_t)(dst + k), m8(bank | (uint16_t)(src + k)));
        hw(bank, nfree, fsize);
        hw(bank, (uint16_t)(nfree + 2), 0);
        hw(bank, NEXT(nfree), after);
        hw(bank, PREV(nfree), r0);
        for (int x = 0xFC; x >= 0; x -= 4)
            if (r16(RAM_OFF_HandleTable + x) == src) { w16(RAM_OFF_HandleTable + x, dst); break; }
        y = nfree;
        goto free;
    }
}

/* HeapCompactBank for both banks */
void HeapCompact(void)
{
    HeapCompactBank(0x7E0000, g.HeapStart7E);
    HeapCompactBank(0x7F0000, g.HeapStart7F);
}

/* A handle for a block of x bytes (a's bit 14: locked) in bank y ($7E or $7F; any other: either),
 * compacting the heaps when it does not fit at first; 0 when no handle or no room */
uint16_t NewHandle(uint16_t a, uint16_t x, uint16_t y)
{
    uint16_t flags = a & 0x4000, h;
    for (h = 0x2000; h != 0x2100; h += 4)
        if (!(r16(h) | r16(h + 2))) break;
    if (h == 0x2100) return 0;
    g.NewHandleSlot = h;
    g.NewHandleSkipBank = y ^ 1;
    for (int pass = 0; pass < 2; pass++) {
        if (pass) HeapCompact();
        for (uint16_t bank = 0x7E; bank <= 0x7F; bank++) {
            if (bank == g.NewHandleSkipBank) continue;
            uint16_t d = HeapAllocBlock((uint32_t)bank << 16, bank == 0x7E ? g.HeapStart7E : g.HeapStart7F, x, flags);
            if (d == 0xFFFF) continue;
            w16(g.NewHandleSlot, d);
            w16(g.NewHandleSlot + 2, bank);
            return g.NewHandleSlot;
        }
    }
    return 0;
}

/* Resize handle a's block to x bytes: in place when it can (shrinking by 9 or more: the rest a free
 * block, joined with a free block after it; growing into a free block after it), else in a new
 * block (NewHandle, either bank, not locked) that the old data (its size + 1 bytes) is moved to,
 * the old one freed and the handle kept */
void SetHandleSize(uint16_t a, uint16_t x)
{
    if (a < 0x2000 || a >= 0x2100 || !(r16(a) | r16(a + 2))) return;
    uint16_t bk = r16(a + 2);
    if (bk != 0x7E && bk != 0x7F) return;
    uint32_t b = (uint32_t)(bk & 0xFF) << 16;
    uint16_t want = x, hdr = (uint16_t)(r16(a) - 8);
    if (!hr(b, (uint16_t)(hdr + 2))) return;
    uint16_t size = hr(b, hdr), nx = hr(b, NEXT(hdr)), fr, after, fsize;
    if (size >= want) {
        if ((uint16_t)(size - want) < 9) return;
        if (nx && !hr(b, (uint16_t)(nx + 2))) {
            uint16_t nsize = hr(b, nx);
            after = hr(b, NEXT(nx));
            fsize = (uint16_t)(size - want);
            hw(b, hdr, want);
            uint32_t s = (uint32_t)hdr + want;
            fr = (uint16_t)(s + 8 + (s >> 16));
            hw(b, NEXT(hdr), fr);
            hw(b, fr, (uint16_t)(fsize + nsize));
        } else {
            after = nx;
            uint32_t d = (uint32_t)size - 8;
            fsize = (uint16_t)(d - want - (d >> 31));
            hw(b, hdr, want);
            uint32_t s = (uint32_t)hdr + want;
            fr = (uint16_t)(s + 8 + (s >> 16));
            hw(b, NEXT(hdr), fr);
            hw(b, fr, fsize);
        }
    } else {
        if (!nx || hr(b, (uint16_t)(nx + 2))) goto move;
        uint32_t s = (uint32_t)hr(b, nx) + 8;
        uint16_t room = (uint16_t)(s + size + (s >> 16));
        if (room == want) {
            after = hr(b, NEXT(nx));
            hw(b, hdr, want);
            hw(b, NEXT(hdr), after);
            if (after) hw(b, PREV(after), hdr);
            return;
        }
        if (room < want) goto move;
        uint16_t grow = (uint16_t)(want - size);
        fsize = (uint16_t)(hr(b, nx) - grow);
        after = hr(b, NEXT(nx));
        hw(b, hdr, want);
        uint32_t t = (uint32_t)(uint16_t)(hdr + 4) + want;
        fr = (uint16_t)(t + 4 + (t >> 16));
        hw(b, NEXT(hdr), fr);
        hw(b, fr, fsize);
    }
    hw(b, (uint16_t)(fr + 2), 0);
    hw(b, NEXT(fr), after);
    hw(b, PREV(fr), hdr);
    if (after) hw(b, PREV(after), fr);
    return;
move:;
    uint16_t nh = NewHandle(0, want, 0);
    uint16_t dst = r16(nh), src = r16(a);
    uint32_t db = (uint32_t)r8(nh + 2) << 16, sb = (uint32_t)r8(a + 2) << 16;
    for (uint32_t k = 0; k <= size; k++) mset8(db | (uint16_t)(dst + k), m8(sb | (uint16_t)(src + k)));
    DisposeHandle(a);
    uint16_t lo = r16(nh), hi = r16(nh + 2);
    w16(a + 2, hi);
    w16(a, lo);
    w16(nh, 0);
    w16(nh + 2, 0);
}

/* Read 16 signature bits from each controller port (bit 1 of JOYSER0 / JOYSER1, after the auto
 * read): mp5connect bit 7 (port 1) or 6 (port 2) for a multitap */
static void rol8(uint32_t off, unsigned c) { w8(off, (uint8_t)(r8(off) << 1 | c)); }

/* Look for a multitap on each port: one when the first 8 signature bits are all 1 and the next 8
 * are not */
void Multi5ConnectCheck(void)
{
    w8(RAM_OFF_mp5connect, 0);
    while (io_r8(0x4212) & 0x01) {}
    io_w8(0x4016, 0);
    io_w8(0x4016, 1);
    for (int k = 0; k < 8; k++) {
        rol8(RAM_OFF_Multi5Sig + 1, io_r8(0x4016) >> 1 & 1);
        rol8(RAM_OFF_Multi5Sig + 3, io_r8(0x4017) >> 1 & 1);
    }
    io_w8(0x4016, 0);
    for (int k = 0; k < 8; k++) {
        rol8(RAM_OFF_Multi5Sig, io_r8(0x4016) >> 1 & 1);
        rol8(RAM_OFF_Multi5Sig + 2, io_r8(0x4017) >> 1 & 1);
    }
    if (g.Multi5Sig[1] == 0xFF && g.Multi5Sig[0] != 0xFF) w8(RAM_OFF_mp5connect, 0x80);
    if (g.Multi5Sig[3] == 0xFF && g.Multi5Sig[2] != 0xFF) w8(RAM_OFF_mp5connect, r8(RAM_OFF_mp5connect) | 0x40);
}

/* Check for the multitap, then clear the multitap pads and state */
void Multi5Init(void)
{
    Multi5ConnectCheck();
    g.JoyPad[2] = g.JoyPad[3] = g.JoyPad[4] = 0x000F;
    g.mp5present = g.mp5warning = 0;
    w16(RAM_OFF_mp5checktimer, 0);
    g.mp5readdelay = 0;
    if (g.mp5connect & 0x0080) g.mp5warning |= 1; else g.mp5warning &= ~1;
    if (g.mp5connect & 0x0040) g.mp5present |= 1; else g.mp5present &= ~1;
}

/* Read the five pads: pads 1 and 2 from the auto read, pads 3 to 5 through the multitap on port 2
 * when mp5present is set (pads 4 and 5 only every 30 frames while nobody uses them); a port line
 * change runs Multi5ConnectCheck for 16 frames; pads without the standard signature read $000F;
 * with a warning every pad flashes $1000 / 0. b is the high byte of the accumulator the caller
 * leaves (the 16-bit stores of the port bits take it). */
static void mp5_sig(uint8_t a)
{
    a &= 0x1E;
    if (a == 0x02) g.mp5warning |= 4;
    else if (a == 0x1E) g.mp5warning |= 2;
}

/* The pads' read of each frame, run by the NMI after the frame hook: JoyPad[0-4], with the
 * multitap's check and warnings */
void Multi5Bios(uint8_t b)
{
    g.mp5padsig[3] = g.mp5padsig[4] = 0;
    while (io_r8(0x4212) & 0x01) {}
    g.JoyPad[0] = io_r16(0x4218);
    g.mp5padsig[0] = (uint8_t)(g.JoyPad[0] & 0x0F);
    rol8(RAM_OFF_mp5padsig, io_r8(0x4016) & 1);
    g.JoyPad[1] = io_r16(0x421A);
    g.mp5padsig[1] = (uint8_t)(g.JoyPad[1] & 0x0F);
    if (!(uint8_t)g.mp5present) {
        rol8(RAM_OFF_mp5padsig + 1, io_r8(0x4017) & 1);
    } else {
        g.JoyPad[2] = io_r16(0x421E);
        g.mp5padsig[2] = (uint8_t)(g.JoyPad[2] & 0x0F);
        uint8_t j = io_r8(0x4017);
        rol8(RAM_OFF_mp5padsig + 1, j & 1);
        rol8(RAM_OFF_mp5padsig + 2, j >> 1 & 1);
        io_w8(0x4201, 0);
        int read = 1;
        if (!((uint8_t)g.mp5readall | (uint8_t)g.mp5warning) && !((uint8_t)g.cont4team | (uint8_t)g.cont5team[0])) {
            uint8_t d = (uint8_t)g.mp5readdelay;
            if (d) { w8(RAM_OFF_mp5readdelay, (uint8_t)(d - 1)); read = 0; }
            else w8(RAM_OFF_mp5readdelay, 0x1E);
        }
        if (read) {
            for (int k = 0; k < 8; k++) {
                j = io_r8(0x4017);
                rol8(RAM_OFF_JoyPad + 7, j & 1);
                rol8(RAM_OFF_JoyPad + 9, j >> 1 & 1);
            }
            for (int k = 0; k < 8; k++) {
                j = io_r8(0x4017);
                rol8(RAM_OFF_JoyPad + 6, j & 1);
                rol8(RAM_OFF_JoyPad + 8, j >> 1 & 1);
            }
            g.mp5padsig[3] = r8(RAM_OFF_JoyPad + 6) & 0x0F;
            g.mp5padsig[4] = r8(RAM_OFF_JoyPad + 8) & 0x0F;
            j = io_r8(0x4017);
            rol8(RAM_OFF_mp5padsig + 3, j & 1);
            rol8(RAM_OFF_mp5padsig + 4, j >> 1 & 1);
        }
    }
    io_w8(0x4201, 0x80);
    g.mp5warning = (uint16_t)((g.mp5warning & 0xFF00) | (g.mp5warning & 0x01));
    if (g.mp5checktimer[0]) g.mp5checktimer[0]--;
    io_w8(0x4016, 0);
    io_w8(0x4016, 1);
    uint8_t p1 = io_r8(0x4016) & 0x02;
    uint8_t chg = (uint8_t)(p1 ^ g.mp5port1last[0]);
    g.mp5port1last[0] = p1; g.mp5port1last[1] = b;
    g.mp5portchg[0] = chg;
    uint8_t p2 = io_r8(0x4017) & 0x02;
    uint8_t c2 = (uint8_t)(p2 ^ g.mp5port2last[0]);
    g.mp5port2last[0] = p2; g.mp5port2last[1] = b;
    if (c2 | g.mp5portchg[0]) g.mp5checktimer[0] = 0x10;
    if (g.mp5checktimer[0]) {
        Multi5ConnectCheck();
        g.JoyPad[2] = g.JoyPad[3] = g.JoyPad[4] = 0x000F;
        if (g.mp5connect & 0x80) g.mp5warning |= 1; else g.mp5warning &= (uint16_t)~1;
        if (g.mp5connect & 0x40) g.mp5present |= 1; else g.mp5present &= (uint16_t)~1;
    } else {
        io_w8(0x4016, 0);
    }
    if (g.mp5padsig[0] != 0x01) {
        g.JoyPad[0] = 0x000F;
        if (g.mp5present) mp5_sig(g.mp5padsig[0]);
    }
    if (g.mp5padsig[1] != 0x01) {
        g.JoyPad[1] = 0x000F;
        if (!g.mp5present) goto warn;
        mp5_sig(g.mp5padsig[1]);
    }
    if ((uint8_t)g.mp5present) {
        for (int i = 2; i < 5; i++)
            if (g.mp5padsig[i] != 0x01) { g.JoyPad[i] = 0x000F; mp5_sig(g.mp5padsig[i]); }
    }
warn:
    if ((uint8_t)g.mp5warning) {
        uint16_t v = (uint8_t)g.vcount[0] & 0x10 ? 0x0000 : 0x1000;
        for (int i = 0; i < 5; i++) g.JoyPad[i] = v;
    }
}

/* The scroll arrows beside a summary list (right while rows are below the five shown, left while
 * rows are above), or blanks in their place; then the text map queued */
void PrintScrollArrows(void)
{
    print((uint16_t)(g.listtop[0] + 5 - g.listend[0]) & 0x8000 ? SYM_RightArrowTxt2 : SYM_PrintScrollArrowsPos);
    print((uint16_t)(g.listtop[0] - 1) & 0x8000 ? SYM_PrintScrollArrowsPos2 : SYM_LeftArrowTxt2);
    QueueTextDMA();
}

/* 8 x 8 bits into 16 (A): unsigned; X signed; both signed */
uint16_t Mul8U(uint16_t a, uint16_t x) { return (uint16_t)((a & 0xFF) * (x & 0xFF)); }

/* X x A (low byte) / 256 into A (and $09): unsigned; X signed (the magnitude's, negated); both
 * signed (the carry as for Mul16x8U, never set by the negation) */
uint16_t Mul16x8HiU(uint16_t x, uint16_t a) { return (uint16_t)(Mul16x8U(x, a) >> 8); }

/* r2 (16 bits) / r1 into r0: unsigned ($7FFF for 0); signed */
uint16_t Div16U(uint16_t n, uint16_t d) { return d ? (uint16_t)(n / d) : 0x7FFF; }

/* r2 (16 bits) / r1 (low byte) into r0, $7FFF for 0 */
uint16_t Div16x8U(uint16_t n, uint16_t d) { return d & 0xFF ? (uint16_t)(n / (d & 0xFF)) : 0x7FFF; }

/* X / A (low byte) into A, the divider's own (x / 0 = $FFFF): unsigned; X signed */
uint16_t DivX8U(uint16_t x, uint16_t a) { return a & 0xFF ? (uint16_t)(x / (a & 0xFF)) : 0xFFFF; }

/* A random number from 0 to limit-1, for a limit below 256 (randoma8_go: randomd0A's way in) */
uint16_t randoma8_go(uint16_t limit) { return Mul8U(limit, RandomWord() & 0xFF) >> 8; }

uint16_t randoma8(uint16_t limit) { return randoma8_go(limit); }

/* d1 = the buttons that changed on any pad */
uint16_t ReadAnyPad(void) { return g.padchangeany; }

/* 2 * JoyDirTable[the d-pad bits of a] (JoyDirIndex_Hi: of a's high byte) */
uint16_t JoyDirIndex(uint16_t a) { return (uint16_t)(rom8(SYM_JoyDirTable + (a & 0xF)) << 1); }

uint16_t JoyDirIndex_Hi(uint16_t a) { return JoyDirIndex((uint16_t)(a >> 8 | a << 8)); }

uint16_t Cos(uint16_t a) { return Sin((uint16_t)(a + 0x0100)); }

/* HeapInit at the same starts */
void HeapReset(void) { HeapInit(g.HeapStart7E, g.HeapStart7F); }

void forcefade(void) { g.fadestep = 0xFFFF; SetFrameHook(SYM_BrightnessHook); }
