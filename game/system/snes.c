/* snes: the engine's arithmetic and random numbers (src/routines/snes).
 *
 * The multiplies and divides use the console's multiplier and divider; here they are the same
 * arithmetic in C, each with the original's operand widths (an "8-bit" operand is the low byte
 * of the register), signs and odd results (Div32U's $7FFF when the quotient would not fit its
 * test, which is stricter than fitting). cf is the 65816's carry where a routine's caller goes on
 * using it (an adc with no clc after the call): the multiplies set it as the original leaves it. */
#include "game.h"

uint8_t cf;
uint8_t nmi_b;

/* 8 x 8 bits into 16 (A): unsigned; X signed; both signed */
uint16_t Mul8U(uint16_t a, uint16_t x) { return (uint16_t)((a & 0xFF) * (x & 0xFF)); }
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

/* X x A (low byte) / 256 into A (and $09): unsigned; X signed (the magnitude's, negated); both
 * signed (the carry as for Mul16x8U, never set by the negation) */
uint16_t Mul16x8HiU(uint16_t x, uint16_t a) { return (uint16_t)(Mul16x8U(x, a) >> 8); }
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

/* r2 (16 bits) / r1 into r0: unsigned ($7FFF for 0); signed */
uint16_t Div16U(uint16_t n, uint16_t d) { return d ? (uint16_t)(n / d) : 0x7FFF; }
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

/* r2 (16 bits) / r1 (low byte) into r0, $7FFF for 0 */
uint16_t Div16x8U(uint16_t n, uint16_t d) { return d & 0xFF ? (uint16_t)(n / (d & 0xFF)) : 0x7FFF; }

/* X / A (low byte) into A, the divider's own (x / 0 = $FFFF): unsigned; X signed */
uint16_t DivX8U(uint16_t x, uint16_t a) { return a & 0xFF ? (uint16_t)(x / (a & 0xFF)) : 0xFFFF; }
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

/* A random number from 0 to limit-1, for a limit below 256 (randoma8_go: randomd0A's way in) */
uint16_t randoma8_go(uint16_t limit) { return Mul8U(limit, RandomWord() & 0xFF) >> 8; }
uint16_t randoma8(uint16_t limit) { return randoma8_go(limit); }

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

/* ---- the console: hooks, pads, interrupts, the sound driver's commands, handles, memory ---- */

/* (the loops that wait for an interrupt to change WRAM: read it afresh, and between two reads let
 * the interrupt run) */
static uint16_t wait_r16(uint32_t off)
{
    return (uint16_t)(((volatile uint8_t *)&g)[off] | ((volatile uint8_t *)&g)[off + 1] << 8);
}
static void wait_tick(void) { hal_wait_interrupt(); }

/* a byte stored at a 24-bit address: WRAM, or the console's registers ($2100-$21FF, $4000-$43FF)
 * in the banks that have them; the cartridge ignores it */
void mset8(uint32_t a, uint8_t v)
{
    uint32_t b = a >> 16 & 0xFF, lo = a & 0xFFFF;
    if (is_wram(a)) { w8(wram_off(a), v); return; }
    if ((b < 0x40 || (b >= 0x80 && b < 0xC0)) && ((lo >= 0x2100 && lo < 0x2200) || (lo >= 0x4000 && lo < 0x4400))) io_w8((uint16_t)lo, v);
}
void mset16(uint32_t a, uint16_t v) { mset8(a, (uint8_t)v); mset8(a + 1, (uint8_t)(v >> 8)); }

/* Empty hooks and the reset stub */
void NullHook(void) {}
void ResetStub(void) {}
void IRQHook_Off(void) {}

/* SoundFadeSpeed = a, the amount SoundUpdate takes off SoundVolume each frame */
void SetSoundFade(uint16_t a) { g.SoundFadeSpeed = a; }

/* d1 = the buttons that changed on any pad */
uint16_t ReadAnyPad(void) { return g.padchangeany; }

/* Run the routine at hook (bank << 16 | address) as the frame hook of the next NMI */
void SetFrameHook(uint32_t hook)
{
    g.FrameHookNext[2] = (uint8_t)(hook >> 16);
    w16(RAM_OFF_FrameHookNext, (uint16_t)hook);
}

/* Set the NMI hook back to the empty routine */
void ClearNMIHook(void)
{
    g.NMIHookNext[2] = SYM_NullHook >> 16;
    w16(RAM_OFF_NMIHookNext, (uint16_t)SYM_NullHook);
}

/* Wait for the next frame (FrameCount changes: the NMI increments it) */
void WaitFrame(void)
{
    uint16_t f = g.FrameCount;
    while (wait_r16(RAM_OFF_FrameCount) == f) wait_tick();
}

/* Turn on an IRQ at line y with the routine at hook (bank << 16 | address): NMITIMEN $81 while the
 * hook is changed, then a frame later $B1 (NMI, pads and the H/V IRQ) */
void SetIRQ(uint32_t hook, uint16_t y)
{
    io_w8(0x4200, 0x81);
    g.IRQVTime = y;
    io_w16(0x4209, y);
    io_w16(0x4207, g.IRQHTime);
    g.IRQHook[2] = (uint8_t)(hook >> 16);
    w16(RAM_OFF_IRQHook, (uint16_t)hook);
    WaitFrame();
    io_w8(0x4200, 0xB1);
    IRQHook_Off();   /* (falls through into it) */
}

/* Turn the IRQ off (NMI and pad reading on) and the IRQ hook to IRQHook_Off */
void IRQOff(void)
{
    io_w8(0x4200, 0x81);
    g.IRQHook[2] = 0;
    w16(RAM_OFF_IRQHook, (uint16_t)SYM_IRQHook_Off);
}

/* NMI hook: the fade brightness to INIDISP unless the screen is blanked */
void BrightnessHook(void)
{
    if (g.IniDispShadow[0] & 0x80) return;
    io_w8(0x2100, (uint8_t)g.brightness);
}

/* 2 * JoyDirTable[the d-pad bits of a] (JoyDirIndex_Hi: of a's high byte) */
uint16_t JoyDirIndex(uint16_t a) { return (uint16_t)(rom8(SYM_JoyDirTable + (a & 0xF)) << 1); }
uint16_t JoyDirIndex_Hi(uint16_t a) { return JoyDirIndex((uint16_t)(a >> 8 | a << 8)); }

/* The frame time step for a 50 Hz console (STAT78 bit 4; the 16-bit read takes APUIO0 too) */
void CheckPAL(void)
{
    g.vcountstep = 0;
    if (io_r16(0x213F) & 0x0010) g.vcountstep = 0x3333;
}

/* a2: the team of the controller that owns the pause menu (2 the away team, else 0) */
uint16_t GetPauseTeam(void)
{
    return r16(RAM_OFF_cont1team + g.pausepad) == 2 ? 2 : 0;
}

/* statsorder[i] = i for the rows of the list (one at least) */
void InitStatsOrder(void)
{
    uint16_t a = 0, y = 0xFFFF;
    do {
        y++;
        w8(RAM_OFF_statsorder + y, (uint8_t)a);
        a++;
    } while (y < g.listend[0]);
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

/* IRQ hooks: the BG1 scroll splits of the stats screens and the band, each installing the next */
static void set_irq_hook(uint32_t hook) { w16(RAM_OFF_IRQHook, (uint16_t)hook); }
/* Stats screen split 1 (at irqline1): BG1VOFS = irqvscroll1, then IRQ_StatsScroll2 at irqline2;
 * scrollirqdone set for the frame */
void IRQ_StatsScroll1(void)
{
    io_w8(0x210E, (uint8_t)g.irqvscroll1);
    io_w8(0x210E, (uint8_t)(g.irqvscroll1 >> 8));
    set_irq_hook(SYM_IRQ_StatsScroll2);
    io_w16(0x4209, g.irqline2);
    g.scrollirqdone[0] = 0xFFFF;
}
/* Stats screen split 2 (at irqline2): BG1VOFS = irqvscroll2, then IRQ_StatsScroll3 at irqline3 */
void IRQ_StatsScroll2(void)
{
    io_w8(0x210E, (uint8_t)g.irqvscroll2);
    io_w8(0x210E, (uint8_t)(g.irqvscroll2 >> 8));
    set_irq_hook(SYM_IRQ_StatsScroll3);
    io_w16(0x4209, g.irqline3);
}
/* Stats screen split 3 (at irqline3): BG1VOFS = irqvscroll3, then IRQ_StatsScroll1 again (the NMI
 * sets VTIME back to irqline1) */
void IRQ_StatsScroll3(void)
{
    io_w8(0x210E, g.irqvscroll3[0]);
    io_w8(0x210E, g.irqvscroll3[1]);
    set_irq_hook(SYM_IRQ_StatsScroll1);
}
/* Top of the band (at irqline1): BG1 scrolled to ($FFFC, $FFFB), then IRQ_BandEnd at irqline2 */
void IRQ_BandStart(void)
{
    io_w8(0x210D, 0xFC); io_w8(0x210D, 0xFF);
    io_w8(0x210E, 0xFB); io_w8(0x210E, 0xFF);
    set_irq_hook(SYM_IRQ_BandEnd);
    io_w16(0x4209, g.irqline2);
}
/* Bottom of the band (at irqline2): BG1 scrolled back to (0, $FFFF), then IRQ_BandStart at irqline1 */
void IRQ_BandEnd(void)
{
    io_w8(0x210D, 0); io_w8(0x210D, 0);
    io_w8(0x210E, 0xFF); io_w8(0x210E, 0xFF);
    set_irq_hook(SYM_IRQ_BandStart);
    io_w16(0x4209, g.irqline1);
}
/* A two-part BG1 split: BG1VOFS = BG3VScroll, then IRQ_BG1Scroll_t1 at line $30 */
void IRQ_BG1Scroll(void)
{
    io_w8(0x210E, (uint8_t)g.BG3VScroll);
    io_w8(0x210E, (uint8_t)(g.BG3VScroll >> 8));
    set_irq_hook(SYM_IRQ_BG1Scroll_t1);
    io_w16(0x4209, 0x0030);
}
/* IRQ_BG1Scroll's second part (at line $30): BG1VOFS back to $FFFF, then IRQ_BG1Scroll again */
void IRQ_BG1Scroll_t1(void)
{
    io_w8(0x210E, 0xFF); io_w8(0x210E, 0xFF);
    set_irq_hook(SYM_IRQ_BG1Scroll);
}

/* The sound driver's commands: wait for APUIO0 to be 0, send the arguments and the command, wait
 * for the driver to echo it, clear APUIO0 */
static void spc_wait(void) { while (io_r8(0x2140)) {} }
/* (the SPCCmd routines' send: cmd to APUIO0, then wait for the driver to echo it back) */
static void spc_send(uint8_t cmd)
{
    io_w8(0x2140, cmd);
    while (io_r8(0x2140) != cmd) {}
}
static void spc_cmd(uint8_t cmd) { spc_wait(); spc_send(cmd); io_w8(0x2140, 0); }
void SPCCmd01(void) { spc_cmd(0x01); }
void SPCCmd04(void) { spc_cmd(0x04); }
void SPCCmd0D(void) { spc_cmd(0x0D); }
void SPCCmd05(uint8_t x) { spc_wait(); io_w8(0x2141, x); spc_send(0x05); io_w8(0x2140, 0); }
void SPCCmd06(uint8_t x, uint16_t y) { io_w16(0x2142, y); spc_wait(); io_w8(0x2141, x); spc_send(0x06); io_w8(0x2140, 0); }
void SPCCmd0B(uint8_t x, uint16_t y) { io_w16(0x2142, y); spc_wait(); io_w8(0x2141, x); spc_send(0x0B); io_w8(0x2140, 0); }
void SPCCmd0C(uint8_t x, uint8_t y) { spc_wait(); io_w8(0x2142, y); io_w8(0x2141, x); spc_send(0x0C); io_w8(0x2140, 0); }
/* Command $0E: the volume of voice x, a the left (APUIO2) and y the right (APUIO3) */
void SPCSetVolume(uint8_t a, uint8_t x, uint8_t y)
{
    spc_wait();
    io_w8(0x2143, y); io_w8(0x2142, a); io_w8(0x2141, x);
    spc_send(0x0E);
    io_w8(0x2140, 0);
}

/* Command $02: the sample count (the result, a byte) and the free sound RAM pointer (*x, a
 * word: APUIO1 and APUIO2) */
uint16_t SPCStatus(uint16_t *x)
{
    spc_wait();
    spc_send(0x02);
    uint8_t y = io_r8(0x2141);
    spc_send(0x80);
    *x = io_r16(0x2141);
    io_w8(0x2140, 0);
    return y;
}

/* Sequencer: command $0C (the GAIN DSPGain of voice y); then $0B (the pitch SendPitch of voice y) */
static void seq_cmd0c(uint8_t y) { spc_wait(); io_w8(0x2142, g.DSPGain); io_w8(0x2141, y); spc_send(0x0C); io_w8(0x2140, 0); }
void SeqCmd0C(uint8_t y) { seq_cmd0c(y); }
/* Sequencer: command $0B, the pitch SendPitch of voice y (APUIO2 and APUIO3) */
void SeqCmd0B(uint8_t y)
{
    spc_wait();
    io_w8(0x2142, g.SendPitch[0]); io_w8(0x2143, g.SendPitch[1]);
    io_w8(0x2141, y);
    spc_send(0x0B);
    io_w8(0x2140, 0);
}
void SeqCmd0C0B(uint8_t y) { seq_cmd0c(y); SeqCmd0B(y); }   /* (its own copy of $0C, then into SeqCmd0B) */

/* Sequencer: clear the command $08 values (after keying every voice off), then send command $08
 * (the volume and pitch modulation) and $82 (noise and echo voices) */
void SeqCmd08Init(void)
{
    SPCCmd0D();
    g.PitchModVoices = g.NoiseVoices = g.EchoVoices = g.EchoDelay = g.EchoVolume = g.EchoFeedback = 0;
    SeqCmd08Init_send();
}
/* Send command $08 (SoundVolume's high byte as both sides' main volume, PitchModVoices) and $82
 * (NoiseVoices, EchoVoices) as they stand: SeqCmd08Init's second half, entered too by SeqCmd08,
 * the volume fade and the events that change one of them */
void SeqCmd08Init_send(void)
{
    spc_wait();
    io_w8(0x2141, (uint8_t)(g.SoundVolume >> 8));
    io_w8(0x2142, (uint8_t)(g.SoundVolume >> 8));
    io_w8(0x2143, g.PitchModVoices);
    spc_send(0x08);
    io_w8(0x2141, g.NoiseVoices);
    io_w8(0x2142, g.EchoVoices);
    spc_send(0x82);
    io_w8(0x2140, 0);
}

/* Clear the track pointers and waits of tracks y up to x (8-bit counters: y = x goes all round) */
void ClearTracks(uint8_t y, uint8_t x)
{
    g.VarLenAcc[0] = x;
    uint8_t i = y, k = (uint8_t)(y << 1);
    do {
        w16(RAM_OFF_TrackInst + k, 0);
        w16(RAM_OFF_TrackWait + k, 0);
        k = (uint8_t)(k + 2);
        i++;
    } while (i != x);
}

/* Copy x colours from [p0] into the colour buffer from colour y */
void SetColours(uint32_t p0, uint16_t x, uint16_t y)
{
    uint16_t k = (uint16_t)(y << 1), j = 0;
    do {
        w16(RAM_OFF_colourbuf + k, m16(p0 + j));
        j += 2; k += 2;
    } while (--x);
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

/* Handles: 64 entries of an address word and a bank word at $2000-$20FF (0, 0 = free) */

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

/* ReadjoyA for the controller that owns the pause menu */
Pad ReadPausePad(void) { return ReadjoyA(g.pausepad >> 1); }

/* Wait x frames, y counting the rounds of 65536 (WaitFrames_Inc adds 1 to y first) */
void WaitFrames(uint16_t x, uint16_t y)
{
    do {
        do { WaitFrame(); } while (--x);
    } while (--y);
}
void WaitFrames_Inc(uint16_t x, uint16_t y) { WaitFrames(x, (uint16_t)(y + 1)); }

/* Wait until the fade has reached black */
void WaitFadeOut(void)
{
    do WaitFrame(); while (g.brightness);
}

/* Wait until no button is held on pad 1 */
void WaitNoButtons(void)
{
    while (wait_r16(RAM_OFF_JoyPad) & 0xFFF0) wait_tick();
}

/* Fade the screen in after a 5-frame delay, or out: BrightnessHook as the frame hook */
void FadeIn(void) { g.fadestep = 5; SetFrameHook(SYM_BrightnessHook); }
void FadeOut(void) { g.fadestep = 0xFFFF; SetFrameHook(SYM_BrightnessHook); }

/* Set the frame hook back to the empty routine */
void ClearFrameHook(void) { SetFrameHook(SYM_NullHook); }

/* The band IRQ pair (lines a and x) that shifts the BG1 scroll */
void SetupBandIRQ(uint16_t a, uint16_t x)
{
    g.irqline1 = a;
    g.irqline2 = x;
    g.IRQHTime = 0x00C4;
    SetIRQ(SYM_IRQ_BandStart, g.irqline1);
}

/* The split scroll of a stats screen: the IRQ lines a, x, y, the three scrolls $FFFF */
void SetupScrollIRQ(uint16_t a, uint16_t x, uint16_t y)
{
    g.irqline1 = a;
    g.irqline2 = x;
    g.irqline3 = y;
    g.irqvscroll1 = g.irqvscroll2 = 0xFFFF;
    w16(RAM_OFF_irqvscroll3, 0xFFFF);
    g.IRQHTime = 0x00C4;
    SetIRQ(SYM_IRQ_StatsScroll1, g.irqline1);
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

/* Stop voice y (x = 2 * y): a streamed effect's stream state cleared, the voice free, owned by no
 * track, and a gain $9F sent */
void StopVoice(uint8_t y, uint8_t x)
{
    uint16_t inst = r16(RAM_OFF_VoiceInst + x);
    if (inst >= 0xE1C0 && inst < 0xE2B1) { g.streamstate = 0; g.streamsfx = 0; }
    w8(RAM_OFF_VoiceState + y, 0);
    w8(RAM_OFF_VoiceTrack + y, 0xFF);
    g.DSPGain = 0x9F;
    SeqCmd0C(y);
}

/* A variable length number (7 bits a byte, top bit = more) from the track at [EventPtr] (its low
 * word steps; the 16-bit reads take the byte after too, the 8-bit X only the byte) */
uint16_t ReadVarLen(void)
{
    uint32_t bank = (uint32_t)(g.EventPtr[1] & 0xFF) << 16;
    uint16_t v = m16(bank | g.EventPtr[0]);
    g.EventPtr[0]++;
    uint16_t a = v & 0x7F;
    while (v & 0x80) {
        w16(RAM_OFF_VarLenAcc, a);
        v = m16(bank | g.EventPtr[0]);
        g.EventPtr[0]++;
        g.VarLenByte = v & 0x7F;
        a = (uint16_t)(r16(RAM_OFF_VarLenAcc) << 7 | g.VarLenByte);
    }
    return a;
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
uint16_t Cos(uint16_t a) { return Sin((uint16_t)(a + 0x0100)); }

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

/* Sound driver commands $07 (voice x: volume, sample, ADSR from [p0]), $08 (main volume, pitch
 * modulation, noise, echo, FLG), $09 (echo), $0A (the echo filter: 8 bytes), each in two or three
 * sends */
static void spc_ack(uint8_t cmd)
{
    io_w8(0x2140, cmd);
    while (io_r8(0x2140) != cmd) {}
}
/* Command $07: voice x's volume, sample and ADSR, 5 bytes from [p0] in two sends ($07, $81) */
void SPCSetVoice(uint8_t x, uint32_t p0)
{
    spc_wait();
    io_w8(0x2141, x);
    io_w8(0x2142, m8(p0)); io_w8(0x2143, m8(p0 + 1));
    spc_ack(0x07);
    io_w8(0x2141, m8(p0 + 2)); io_w8(0x2142, m8(p0 + 3)); io_w8(0x2143, m8(p0 + 4));
    spc_ack(0x81);
    io_w8(0x2140, 0);
}
/* (SPCCmd08 and SPCCmd09: 5 bytes from [p0] in two sends, cmd with the first three, cmd2 with the
 * last two) */
static void spc_cmd5(uint8_t cmd, uint8_t cmd2, uint32_t p0)
{
    spc_wait();
    io_w8(0x2141, m8(p0)); io_w8(0x2142, m8(p0 + 1)); io_w8(0x2143, m8(p0 + 2));
    spc_ack(cmd);
    io_w8(0x2141, m8(p0 + 3)); io_w8(0x2142, m8(p0 + 4));
    spc_ack(cmd2);
    io_w8(0x2140, 0);
}
void SPCCmd08(uint32_t p0) { spc_cmd5(0x08, 0x82, p0); }
void SPCCmd09(uint32_t p0) { spc_cmd5(0x09, 0x83, p0); }
/* Command $0A: the 8 echo filter coefficients from [p0], in three sends ($0A, $84, $85) */
void SPCSetFIR(uint32_t p0)
{
    spc_wait();
    io_w8(0x2141, m8(p0)); io_w8(0x2142, m8(p0 + 1)); io_w8(0x2143, m8(p0 + 2));
    spc_ack(0x0A);
    io_w8(0x2141, m8(p0 + 3)); io_w8(0x2142, m8(p0 + 4)); io_w8(0x2143, m8(p0 + 5));
    spc_ack(0x84);
    io_w8(0x2141, m8(p0 + 6)); io_w8(0x2142, m8(p0 + 7));
    spc_ack(0x85);
    io_w8(0x2140, 0);
}

/* Sound driver command $11: x bytes of sound RAM from address y to [p0], a byte a handshake on
 * APUIO1 */
void SPCDownload(uint32_t p0, uint16_t x, uint16_t y)
{
    spc_wait();
    io_w16(0x2142, y);
    spc_ack(0x11);
    io_w16(0x2142, x);
    spc_ack(0x86);
    io_w8(0x2140, 0x87);
    io_w8(0x2141, 0);
    uint16_t k = 0;
    do {
        while (io_r8(0x2141) == (uint8_t)k) {}
        mset8(p0 + k, io_r8(0x2142));
        k++;
        io_w8(0x2141, (uint8_t)k);
    } while (--x);
    io_w8(0x2140, 0);
}

/* Reset the scroll, window and colour math registers and their copies */
void ResetPPURegs(void)
{
    g.BG1HScroll = g.BG2HScroll = 0;
    w16(RAM_OFF_BG3HScroll, 0);
    g.BG1VScroll = g.BG2VScroll = g.BG3VScroll = 0xFFFF;
    g.ClipTopUnused = 0xFFF0;
    g.r9[0] = 0xFFF0;
    g.ClipBottomUnused = 0x00E0;
    g.r9[1] = 0x0100;
    static const uint16_t regs[] = { 0x2131, 0x2130, 0x212E, 0x2106, 0x2133, 0x212D, 0x212F, 0x2123, 0x2124, 0x2125 };
    for (unsigned i = 0; i < sizeof regs / sizeof *regs; i++) io_w8(regs[i], 0);
    g.Mode7InUse = 0;
}

/* Move the unused OAM entries off screen (Y = $E1), swap the OAM buffers and set OAMReady */
void SpriteFrameEnd(void)
{
    uint16_t x = g.OAMNext;
    while (x < g.OAMHideEnd) {
        w16(x, 0xE100);
        x += 4;
        if (!x) break;
    }
    uint16_t last = g.OAMLastEnd;
    g.OAMLastEnd = g.OAMNext;
    g.OAMHideEnd = last;
    uint16_t d = g.OAMDrawBuf;
    g.OAMDrawBuf = g.OAMShowBuf;
    g.OAMShowBuf = d;
    g.OAMReady[0] = 1;
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

/* Add a DMA of x bytes from [p0] to video memory at y to the NMI's transfer list, in pieces of
 * $1000 bytes at most; the VRAM address after it is returned and *p0 moves past the source (its
 * low word: the second add takes the first's carry) */
uint16_t QueueVRAMDMA(uint32_t *p0, uint16_t x, uint16_t y)
{
    uint16_t a = x, t = g.DMAQueueTail, lo = (uint16_t)*p0;
    for (;;) {
        uint16_t n = a >= 0x1000 ? 0x1000 : a;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, n);
        uint16_t words = n >> 1;
        w16(e + 6, y);
        y = (uint16_t)(y + words);
        w16(e + 1, lo);
        uint32_t s = (uint32_t)lo + words;
        lo = (uint16_t)(s + words + (s >> 16));
        w8(e + 3, (uint8_t)(*p0 >> 16));
        w8(e, 1);
        t = (t + 8) & 0x03FF;
        if (a <= 0x1000) break;
        a = (uint16_t)(a - 0x1000);
    }
    g.DMAQueueTail = t;
    *p0 = (*p0 & 0xFF0000) | lo;
    return y;
}

/* Fill the text map with the blank tile (textmap2 with charofs2; from row 12 down) */
void ClearTextMap(void) { MemFill(0x7E0000 + RAM_OFF_textmap, 0x0800, (uint16_t)(g.blankchar + g.charofs)); }
void ClearTextMap2(void) { MemFill(0x7E0000 + RAM_OFF_textmap2, 0x0800, (uint16_t)(g.blankchar + g.charofs2)); }
void ClearTextMapBottom(void) { MemFill(0x7E0000 + RAM_OFF_textmap + 0x300, 0x0500, (uint16_t)(g.blankchar + g.charofs)); }

/* HeapInit at the same starts */
void HeapReset(void) { HeapInit(g.HeapStart7E, g.HeapStart7F); }

/* Full brightness (fading in unless already there), no forced blank */
void ScreenOn(void)
{
    if (g.brightness != 0x000F) FadeIn();
    g.IniDispShadow[0] = 0x0F;
}

/* Wait for a frame, set the default hooks, turn the IRQ off, force blank (a frame later HDMA off)
 * and drop the DMA queue */
void BlankScreen(void)
{
    WaitFrame();
    IRQOff();
    ClearFrameHook();
    ClearNMIHook();
    g.IniDispShadow[0] = 0x8F;
    WaitFrame();
    io_w8(0x420C, 0);
    g.DMAQueueTail = g.DMAQueueHead;
}

/* DMA x bytes from [p0] to video memory at y (negative: where it is, r3 as the caller left it):
 * at once with the screen blanked, else queued for the NMI (QueueVRAMDMA, which moves *p0). The
 * VRAM address after it. */
uint16_t DMAToVRAM(uint32_t *p0, uint16_t x, uint16_t y, uint16_t r3)
{
    if (!(r16(RAM_OFF_IniDispShadow - 1) & 0x8000)) return QueueVRAMDMA(p0, x, y);
    io_w16(0x4315, x);
    io_w16(0x4312, (uint16_t)*p0);
    io_w16(0x4310, 0x1801);
    if (!(y & 0x8000)) { io_w16(0x2116, y); r3 = y; }
    io_w8(0x4314, (uint8_t)(*p0 >> 16));
    io_w8(0x420B, 0x02);
    return (uint16_t)((x >> 1) + r3 + (x & 1));
}

/* DMA x bytes from [p0] to the high bytes of video memory at y. Queued: in $1000-byte pieces of
 * type $FA, the VRAM address stepping by the bytes and the source by $1000 a piece. */
uint16_t DMAToVRAMHigh(uint32_t *p0, uint16_t x, uint16_t y)
{
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w16(0x4315, x);
        io_w16(0x4312, (uint16_t)*p0);
        io_w16(0x4310, 0x1900);
        io_w16(0x2116, y);
        io_w8(0x4314, (uint8_t)(*p0 >> 16));
        io_w8(0x420B, 0x02);
        return (uint16_t)((x >> 1) + y + (x & 1));
    }
    uint16_t a = x, t = g.DMAQueueTail;
    for (;;) {
        uint16_t n = a >= 0x1001 ? 0x1000 : a;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, n);
        w16(e + 6, y);
        y = (uint16_t)(n + y);
        w16(e + 1, (uint16_t)*p0);
        *p0 = (*p0 & 0xFF0000) | (uint16_t)(*p0 + 0x1000);
        w8(e + 3, (uint8_t)(*p0 >> 16));
        w8(e, 0xFA);
        t = (t + 8) & 0x03FF;
        if (a <= 0x1000) break;
        a = (uint16_t)(a - 0x1000);
    }
    g.DMAQueueTail = t;
    return y;
}

/* Fill x bytes of video memory at y (negative: where it is, r3 as the caller left it) with the
 * word a, the DMA's fixed source at $0016 (r3+2, which holds a): at once with the screen
 * blanked, else queued in $1000-byte pieces (type 9). The VRAM address after it. */
uint16_t FillVRAM(uint16_t a, uint16_t x, uint16_t y, uint16_t r3)
{
    g.r3[1] = a;
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w16(0x4315, x);
        if (!(y & 0x8000)) { io_w16(0x2116, y); r3 = y; }
        io_w16(0x4312, 0x0016);
        io_w8(0x4310, 0x09);
        io_w8(0x4311, 0x18);
        io_w8(0x4314, 0);
        io_w8(0x420B, 0x02);
        return (uint16_t)((x >> 1) + r3 + (x & 1));
    }
    uint16_t n = x, t = g.DMAQueueTail;
    for (;;) {
        uint16_t k = n >= 0x1001 ? 0x1000 : n;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, k);
        w16(e + 6, y);
        y = (uint16_t)((k >> 1) + y);
        w16(e + 1, 0x0016);
        w8(e + 3, 0);
        w8(e, 0x09);
        t = (t + 8) & 0x03FF;
        if (n <= 0x1000) break;
        n = (uint16_t)(n - 0x1000);
    }
    g.DMAQueueTail = t;
    return y;
}

/* Wait until the DMA queue is empty (the NMI moves its head up to the tail) */
void WaitDMAQueue(void)
{
    while (wait_r16(RAM_OFF_DMAQueueHead) != g.DMAQueueTail) wait_tick();
}

/* Copy map [*p0] (width, height, then the entries from +6) into the map at [*p1] (r7 entries a
 * row), column x, row y, adding a to each entry; *p0 and *p1 end past the last row (their low
 * words, with the carries of the original's adds) */
void PasteMap(uint32_t *p0, uint32_t *p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    uint16_t lo1 = (uint16_t)((uint16_t)*p1 + (uint16_t)s + (s >> 16));
    uint16_t w = m16(*p0), h = m16(*p0 + 2);
    uint16_t lo0 = (uint16_t)((uint16_t)*p0 + 6);
    uint32_t b0 = *p0 & 0xFF0000, b1 = *p1 & 0xFF0000;
    do {
        uint16_t k = 0, n = w;
        do {
            mset16((b1 | lo1) + k, (uint16_t)(m16((b0 | lo0) + k) + a));
            k += 2;
        } while (--n);
        lo1 = (uint16_t)((uint16_t)(r7 << 1) + lo1 + (r7 >> 15));
        lo0 = (uint16_t)((uint16_t)(w << 1) + lo0 + (w >> 15));
    } while (--h);
    *p0 = b0 | lo0;
    *p1 = b1 | lo1;
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

/* Release every voice (the sound locked meanwhile) */
void SoundVoicesOff(void)
{
    g.SoundLock[0]--;
    for (int y = 7; y >= 0; y--) StopVoice((uint8_t)y, (uint8_t)(y << 1));
    g.SoundLock[0]++;
}

/* Stop the song: release the voices of its tracks (below $10) and clear its tracks */
void MusicStop(void)
{
    g.SoundLock[0] = 0xFF;
    w8(RAM_OFF_MusicOn, 0);
    g.TrackCount = 0;
    for (int y = 7; y >= 0; y--) {
        if (g.VoiceTrack[y] >= 0x10 || !g.VoiceState[y]) continue;
        StopVoice((uint8_t)y, (uint8_t)(y << 1));
    }
    ClearTracks(0, 0x10);
    g.SoundLock[0] = 0;
}

/* Stop every voice that plays sound a (its entry through SoundTable; after the first stop the
 * comparisons are a byte: StopVoice returns with 8-bit registers) */
void StopSound(uint8_t a)
{
    g.SoundLock[0]--;
    uint32_t t = g.SoundTable & 0xFFFFFF;
    g.InstPtr = (uint16_t)(m16(t + (uint8_t)(a << 1)) + (uint16_t)t);
    int wide = 1;
    for (int y = 0; y < 8; y++) {
        uint16_t v = g.VoiceInst[y];
        if (wide ? v != g.InstPtr : (uint8_t)v != (uint8_t)g.InstPtr) continue;
        StopVoice((uint8_t)y, (uint8_t)(y << 1));
        wide = 0;
    }
    g.SoundLock[0]++;
}

/* Put voice y (x = 2 * y) into its release: state 4, its gain word from the instrument's byte
 * $15, and a release gain by the instrument's byte $11 (a voice in state 1 just stops) */
void ReleaseVoice(uint8_t y, uint8_t x)
{
    uint8_t st = r8(RAM_OFF_VoiceState + y);
    if (!st) return;
    if (st == 1) { StopVoice(y, x); return; }
    w8(RAM_OFF_VoiceState + y, 4);
    g.InstPtr = r16(RAM_OFF_VoiceInst + x);
    uint32_t bank = (uint32_t)g.SoundBank[0] << 16;
    w16(RAM_OFF_VoiceGain + x, (uint16_t)(((lm16(bank + g.InstPtr + 0x15) >> 1) & 0x7000) + 0x0FFF));
    uint8_t r = (uint8_t)(lm16(bank + g.InstPtr + 0x10) >> 8);
    g.DSPGain = r >= 0x6D ? 0x9F : r >= 0x36 ? 0x99 : r >= 0x03 ? 0x8C : 0x81;
    SeqCmd0C(y);
}

/* Send the text map (all, or rows 1-12) or the second text map to video memory */
static void send_map(uint32_t off, uint16_t x, uint16_t y) { uint32_t p0 = 0x7E0000 + off; DMAToVRAM(&p0, x, y, 0); }
void SendTextMap(void) { send_map(RAM_OFF_textmap, 0x0800, 0x0000); }
void SendTextMap2(void) { send_map(RAM_OFF_textmap2, 0x0800, 0x0400); }
void SendTextMapRows(void) { send_map(RAM_OFF_textmap + 0x40, 0x0300, 0x0020); }
void SendTextMap2Rows(void) { send_map(RAM_OFF_textmap2 + 0x40, 0x0300, 0x0420); }

/* Voice y (x = 2 * y): count its length down; at 0 release it unless it is releasing already or
 * its track holds the sustain pedal (a negative length: no limit) */
void UpdateVoice(uint8_t y, uint8_t x)
{
    uint16_t len = r16(RAM_OFF_VoiceLength + x);
    if (len & 0x8000) return;
    if (len) { w16(RAM_OFF_VoiceLength + x, (uint16_t)(len - 1)); return; }
    if (r8(RAM_OFF_VoiceState + y) == 4) return;
    if (r8(RAM_OFF_TrackSustain + r8(RAM_OFF_VoiceTrack + y))) return;
    ReleaseVoice(y, x);
}

/* Silence everything and reset the player: every voice released, the sound RAM from SongUnused
 * on cleared, SoundVolume $5000, the tracks cleared, command $08 sent, TempoTick $12 */
void SoundInit(void)
{
    g.SoundLock[0] = 0xFF;
    SoundVoicesOff();
    w16(RAM_OFF_SoundBank, (uint16_t)(g.SoundTable >> 16));
    for (int x = 0x021A; x >= 0; x--) w8(RAM_OFF_SongUnused + x, 0);
    g.SoundVolume = 0x5000;
    ClearTracks(0, 0x11);
    SeqCmd08Init();
    g.TempoTick = 0x12;
    g.SoundLock[0] = 0;
    g.VoiceRotor[0] = 0;
}

/* Start the song at [p0]: the tempo from its second byte (DivX8U leaves the 16-bit accumulator, so
 * TempoTick's store takes the byte after it too, and EventPtr+2 the word at p0+2: p0 here is the
 * direct page's 4 bytes), the tracks from its offset list at +8, the track count from +6 */
void MusicStart(uint32_t p0)
{
    uint32_t s = p0 & 0xFFFFFF;
    MusicStop();
    g.SoundLock[0]--;
    for (int y = 0x10; y >= 0; y--) g.TrackUnused[y] = 0x1F;
    uint16_t x = 0x2760;
    if (io_r8(0x213F) & 0x10) x = 0x1FE0;
    w16(RAM_OFF_TempoTick, DivX8U(x, m8(s + 1)));
    g.EventPtr[1] = (uint16_t)(p0 >> 16);
    g.SongPtr = (uint16_t)p0;
    uint8_t k = 0, y = 8;
    for (;;) {
        uint16_t v = m16(s + y);
        if (!v) break;
        v = (uint16_t)(v + (uint16_t)p0);
        w16(RAM_OFF_TrackPtr + k, v);
        w16(RAM_OFF_TrackLoop + k, v);
        y = (uint8_t)(y + 2);
        k = (uint8_t)(k + 2);
    }
    g.SongUnused = 0;
    g.TrackCount = m8(s + 6);
    w8(RAM_OFF_MusicOn, 1);
    g.SoundLock[0]++;
}

/* Sequencer: command $07 (the voice's volume and the instrument's sample and ADSR, bytes $14-$16),
 * $06 (its pitch) and, unless ADSR is on (byte $15 bit 7), $0C (its start gain); Y = the voice */
uint8_t SeqCmd07(void)
{
    uint32_t inst = (uint32_t)g.SoundBank[0] << 16;
    spc_wait();
    io_w8(0x2141, g.CurVoice); io_w8(0x2142, g.VoiceVolL); io_w8(0x2143, g.VoiceVolR);
    spc_send(0x07);
    io_w8(0x2141, lm8(inst + g.InstPtr + 0x14));
    uint8_t adsr = lm8(inst + g.InstPtr + 0x15);
    io_w8(0x2142, adsr);
    io_w8(0x2143, lm8(inst + g.InstPtr + 0x16));
    spc_send(0x81);
    io_w8(0x2140, 0);
    spc_wait();
    io_w8(0x2141, g.CurVoice);
    io_w16(0x2142, r16(RAM_OFF_NotePitch));
    spc_send(0x06);
    io_w8(0x2140, 0);
    if (!(adsr & 0x80)) {
        spc_wait();
        io_w8(0x2142, r8(RAM_OFF_StartGain + 1));
        spc_send(0x0C);
        io_w8(0x2140, 0);
    }
    return g.CurVoice;
}

/* Sequencer: command $08 (volume, pitch modulation, noise and echo voices) and, with echo voices,
 * $09 (echo volume, feedback, buffer at $E0, delay) */
void SeqCmd08(void)
{
    SeqCmd08Init_send();
    if (!g.EchoVoices) return;
    spc_wait();
    io_w8(0x2141, g.EchoVolume); io_w8(0x2142, g.EchoVolume); io_w8(0x2143, g.EchoFeedback);
    spc_send(0x09);
    io_w8(0x2141, 0xE0); io_w8(0x2142, g.EchoDelay);
    spc_send(0x83);
    io_w8(0x2140, 0);
}

/* A voice for the sound at InstPtr (byte 0: the voices it may use, $2A: its priority): a free one
 * from after the last used (VoiceRotor), else the usable one of no higher priority with the
 * highest state, then age (the ages are read from voice 7 down whatever voice is tested; the 16-bit
 * best age takes InstVolL and VoiceVolL), which is cut (command $0C; $FF none) */
uint8_t FindFreeVoice(void)
{
    uint32_t inst = (uint32_t)g.SoundBank[0] << 16 | g.InstPtr;
    uint8_t x = (uint8_t)((g.VoiceRotor[0] + 1) & 7);
    g.VoiceRotor[0] = x;
    for (int n = 0; n < 8; n++) {
        if (!g.VoiceState[x] && (lm8(inst) & rom8(SYM_ByteBitMasks + x))) return x;
        x = (uint8_t)((x + 1) & 7);
    }
    g.CurVoice = 0xFF;
    g.InstVolL = g.VoiceVolL = g.InstVolR = 0;
    g.VoiceVolR = lm8(inst + 0x2A);
    g.StealCount[0] = 8;
    x = g.VoiceRotor[0];
    uint8_t y = 0x0E;
    do {
        if ((lm8(inst) & rom8(SYM_ByteBitMasks + x)) && g.VoiceVolR >= g.VoicePriority[x]) {
            uint8_t st = g.VoiceState[x];
            uint16_t age = r16(RAM_OFF_VoiceAge + y);
            if (st > g.InstVolR) {
                g.CurVoice = x;
                g.InstVolR = st;
                w16(RAM_OFF_InstVolL, age);
            } else if (st == g.InstVolR && age >= r16(RAM_OFF_InstVolL)) {
                g.CurVoice = x;
                w16(RAM_OFF_InstVolL, age);
            }
        }
        x = (uint8_t)((x + 1) & 7);
        y = (uint8_t)(y - 2);
    } while (--g.StealCount[0]);
    SeqCmd0C(g.CurVoice);
    return g.CurVoice;
}

/* Sound driver command $0F: start streaming a sample of x bytes from [StreamSrc] with loop point
 * y, sending its first block ($8A bytes, or $73 when STAT78's version bits are 0); a byte a
 * handshake on APUIO1. SPCStreamPtr is left past the block (its bank word from StreamSrc+2). */
void SPCStreamStart(uint16_t x, uint16_t y)
{
    uint16_t n = io_r16(0x213F) & 0x007E ? 0x008A : 0x0073;
    g.SPCStreamLeft[0] = (uint16_t)(x - n);
    spc_wait();
    io_w16(0x2142, y);
    io_w8(0x2140, 0x0F);
    io_w8(0x2141, 0);
    while (io_r8(0x2140) != 0x0F) {}
    uint16_t src = r16(RAM_OFF_StreamSrc), k = 0;
    uint32_t bank = (uint32_t)r8(RAM_OFF_StreamSrc + 2) << 16;
    do {
        io_w8(0x2142, m8((bank | src) + k));
        k++;
        io_w8(0x2141, (uint8_t)k);
        while (io_r8(0x2141) != (uint8_t)k) {}
    } while (--n);
    io_w8(0x2140, 0);
    w16(RAM_OFF_SPCStreamPtr, (uint16_t)(k + src));
    w16(RAM_OFF_SPCStreamPtr + 2, r16(RAM_OFF_StreamSrc + 2));
}

/* Sound driver command $10: the next block of the stream ($8A bytes, or $73 when STAT78 bit 4 is
 * clear; the rest when it is the last, 0 = 65536); the bytes still left */
uint16_t SPCStreamNext(void)
{
    w16(RAM_OFF_StreamSrc, r16(RAM_OFF_SPCStreamPtr));
    w16(RAM_OFF_StreamSrc + 2, r16(RAM_OFF_SPCStreamPtr + 2));
    uint16_t blk = io_r16(0x213F) & 0x0010 ? 0x008A : 0x0073, n;
    if (g.SPCStreamLeft[0] < blk + 1) { n = g.SPCStreamLeft[0]; g.SPCStreamLeft[0] = 0; }
    else { g.SPCStreamLeft[0] = (uint16_t)(g.SPCStreamLeft[0] - blk); n = blk; }
    spc_wait();
    io_w8(0x2140, 0x10);
    io_w8(0x2141, 0);
    while (io_r8(0x2140) != 0x10) {}
    uint16_t src = r16(RAM_OFF_StreamSrc), k = 0;
    uint32_t bank = (uint32_t)r8(RAM_OFF_StreamSrc + 2) << 16;
    do {
        io_w8(0x2142, m8((bank | src) + k));
        k++;
        while (io_r8(0x2141) != (uint8_t)k) {}
    } while (--n);
    io_w8(0x2140, 0);
    w16(RAM_OFF_SPCStreamPtr, (uint16_t)(k + src));
    return g.SPCStreamLeft[0];
}

/* Send the stream at [p0] through the sound CPU's boot loader: blocks of length, sound RAM address
 * and data, a length of 0 ending it with the start address (each block's kick value the last
 * index + 4, never 0) */
void SPCUpload(uint32_t p0)
{
    uint16_t y = 0;
    while (io_r16(0x2140) != 0xBBAA) {}
    uint8_t a = 0xCC;
    for (;;) {
        uint8_t kick = a;
        uint16_t len = m16(p0 + y); y += 2;
        uint16_t addr = m16(p0 + y); y += 2;
        io_w16(0x2142, addr);
        uint8_t more = len >= 1;
        io_w8(0x2141, more);
        io_w8(0x2140, kick);
        while (io_r8(0x2140) != kick) {}
        if (!more) break;
        uint8_t data = m8(p0 + y); y++;
        a = 0;
        uint16_t x = len;
        for (;;) {
            io_w8(0x2140, a);
            io_w8(0x2141, data);
            if (!--x) break;
            data = m8(p0 + y); y++;
            while (io_r8(0x2140) != a) {}
            a++;
        }
        while (io_r8(0x2140) != a) {}
        a = (uint8_t)(a + 4);
        if (!a) a = 4;
    }
    io_w8(0x2140, 0);
}

/* Copy a part of map [*p0] (its width at +0, entries from +6): r4 entries by r4+2 rows from column
 * r6, row r6+2, into the map at [*p1] (r7 entries a row) at column x, row y, adding a to each
 * entry; *p0 and *p1 end past the last row (their low words, with the original's carries) */
void PasteMapPart(uint32_t *p0, uint32_t *p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7, uint16_t r6, uint16_t r6h, uint16_t r4, uint16_t r4h)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    uint16_t lo1 = (uint16_t)((uint16_t)*p1 + (uint16_t)s + (s >> 16));
    uint16_t w = m16(*p0);
    r2 = Mul16U((uint16_t)(r6h << 1), w);
    s = (uint32_t)(uint16_t)(r6 << 1) + (uint16_t)r2;
    s = (uint32_t)(uint16_t)s + 6 + (s >> 16);
    uint16_t lo0 = (uint16_t)((uint16_t)*p0 + (uint16_t)s + (s >> 16));
    uint32_t b0 = *p0 & 0xFF0000, b1 = *p1 & 0xFF0000;
    do {
        uint16_t k = 0, n = r4;
        do {
            mset16((b1 | lo1) + k, (uint16_t)(m16((b0 | lo0) + k) + a));
            k += 2;
        } while (--n);
        lo1 = (uint16_t)((uint16_t)(r7 << 1) + lo1 + (r7 >> 15));
        lo0 = (uint16_t)((uint16_t)(w << 1) + lo0 + (w >> 15));
    } while (--r4h);
    *p0 = b0 | lo0;
    *p1 = b1 | lo1;
}

/* Fill y bytes at [p1] with the byte a, from the top down (y = 1 goes round the 64 KB) */
void PasteMapPart_t1(uint32_t p1, uint8_t a, uint16_t y)
{
    y--;
    do { mset8(p1 + y, a); y--; } while (y);
    mset8(p1, a);
}

/* Read the next event of the track at CurTrackPtr: its delta (ReadVarLen, not kept), its code
 * into EventCode and its data into EventData1 / EventData2 by the code (a note: the key and its
 * length; $E7 skips a block) */
void ReadTrackEvent(void)
{
    uint32_t bank = (uint32_t)(g.EventPtr[1] & 0xFF) << 16;
    g.EventPtr[0] = g.CurTrackPtr;
    ReadVarLen();
    uint8_t c = m8(bank | g.EventPtr[0]);
    g.EventPtr[0]++;
    w8(RAM_OFF_EventCode, c);
    if (c < 0xD9) {
        w8(RAM_OFF_EventData1, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        w16(RAM_OFF_EventData2, ReadVarLen());
        return;
    }
    switch (c) {
    case 0xE7:
        g.EventPtr[0] = (uint16_t)(g.EventPtr[0] + m8(bank | g.EventPtr[0]) + 1);
        return;
    case 0xDC: case 0xEA: case 0xDD: case 0xDE: case 0xE2:
        w8(RAM_OFF_EventData1, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        return;
    case 0xDF:
        w8(RAM_OFF_EventData1, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        w8(RAM_OFF_EventData2, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        return;
    case 0xE5:
        w16(RAM_OFF_EventData2, m16(bank | g.EventPtr[0]));
        g.EventPtr[0] = (uint16_t)(g.EventPtr[0] + 2);
        return;
    }
}

/* Start the sprites: sprite tile 0 at VRAM a, x OAM entries reserved, y tiles (at least $1C0, else
 * $1A0, in 32s) for the 16x16 blocks; the block and tile allocators empty, the frame cache clear,
 * the OAM buffers reset and an empty frame ended */
void InitSprites(uint16_t a, uint16_t x, uint16_t y)
{
    g.SpriteVRAM = a;
    g.SpritesReserved = x;
    uint16_t t = y >= 0x01C0 ? y : 0x01A0;
    g.SpriteBlockTiles = (uint16_t)(t + 0x1F) & 0xFFE0;
    g.FrameCacheCount = 0;
    uint8_t k = (uint8_t)(g.SpriteBlockTiles >> 2), v = 0;
    do {
        w8(RAM_OFF_BlockRuns + k, v);
        k = (uint8_t)(k - 8);
        w8(RAM_OFF_BlockRunPrev + (uint8_t)(k + 8), k);
        v = 8;
    } while (!(k & 0x80));
    g.BlockRunPrev[0] = 0xFF;
    g.CacheUseMark = 2;
    uint8_t n = (uint8_t)(0x0200 - g.SpriteBlockTiles);
    g.TileRuns[0] = n;
    w8(RAM_OFF_TileRuns + n, 0);
    w8(RAM_OFF_TileRunPrev + n, 0);
    g.TileRunPrev[0] = 0xFF;
    g.SpritesLeft = 0x0080;
    g.OAMNext = 0x2100;
    g.OAMHideEnd = 0x2300;
    g.OAMLastEnd = 0x2520;
    g.OAMShowBuf = 0x2320;
    g.OAMDrawBuf = 0x2100;
    for (int i = 0x03D0; i >= 0; i -= 2) w16(RAM_OFF_FrameCacheOf + i, 0xFFFF);
    SpriteFrameEnd();
}

/* Update each voice in use that belongs to a music track (below $10), CurVoice and VoiceOfs the
 * voice */
void UpdateVoices(void)
{
    for (uint8_t y = 0; y < 8; y++) {
        uint8_t x = (uint8_t)(y << 1);
        g.CurVoice = y;
        w8(RAM_OFF_VoiceOfs, x);
        if (!g.VoiceState[y] || g.VoiceTrack[y] >= 0x10) continue;
        UpdateVoice(y, x);
    }
}

/* Play song slot a of the home team (TeamSongs), or a random DefaultSongs song when it has none,
 * unless a song plays; cursong = a + 1 */
void PlayTeamSong(uint16_t a)
{
    if (g.MusicOn) return;
    uint16_t h = g.HomeTeam;
    uint16_t x = (uint16_t)((uint16_t)(h << 1) + h + (h >> 15));
    x = (uint16_t)((uint16_t)((uint16_t)(x << 1) + a) << 2);
    uint16_t bank = rom16(SYM_TeamSongs + 2 + x), lo;
    if (!(bank & 0x8000)) lo = rom16(SYM_TeamSongs + x);
    else {
        uint16_t k = (uint16_t)(randoma8(8) << 2);
        lo = rom16(SYM_DefaultSongs + k);
        bank = rom16(SYM_DefaultSongs + 2 + k);
    }
    MusicStart(lo | (uint32_t)bank << 16);
    g.cursong = (uint16_t)(a + 1);
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

/* Fill x VRAM words at y with the word a, the low bytes (from $0016) then the high bytes ($0017):
 * at once with the screen blanked, else queued in pairs of $1000-byte pieces (types $FD, $FC). The
 * VRAM address after it (y + x). */
uint16_t FillVRAMSplit(uint16_t a, uint16_t x, uint16_t y)
{
    g.r3[1] = a;
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w16(0x4315, x);
        io_w16(0x2116, y);
        io_w16(0x4312, 0x0016);
        io_w16(0x4310, 0x1808);
        io_w8(0x2115, 0);
        io_w8(0x4314, 0);
        io_w8(0x420B, 0x02);
        io_w16(0x4315, x);
        io_w16(0x2116, y);
        io_w16(0x4312, 0x0017);
        io_w16(0x4310, 0x1908);
        io_w8(0x2115, 0x80);
        io_w8(0x4314, 0);
        io_w8(0x420B, 0x02);
        return (uint16_t)(x + y);
    }
    uint16_t n = x, t = g.DMAQueueTail;
    for (;;) {
        uint16_t k = n >= 0x1001 ? 0x1000 : n;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, k); w16(e + 6, y); w16(e + 1, 0x0016); w8(e + 3, 0); w8(e, 0xFD);
        t = (t + 8) & 0x03FF;
        e = RAM_OFF_DMAQueue + t;
        w16(e + 4, k); w16(e + 6, y);
        y = (uint16_t)(k + y);
        w16(e + 1, 0x0017); w8(e + 3, 0); w8(e, 0xFC);
        t = (t + 8) & 0x03FF;
        if (n <= 0x1000) break;
        n = (uint16_t)(n - 0x1000);
    }
    g.DMAQueueTail = t;
    return y;
}

/* (a byte to WRAM through the port, $2180: the console writes it at the address in $2181-$2183) */
static void wmdata_w(uint8_t v) { io_w8(0x2180, v); }

/* (a byte at an absolute address in data bank db, as lda.w / sta.w reach it) */
static uint8_t db_r8(uint8_t db, uint16_t a) { return m8(((uint32_t)db << 16) + a); }
static void db_w8(uint8_t db, uint16_t a, uint8_t v) { mset8(((uint32_t)db << 16) + a, v); }

/* Run-length data at y (data bank db; its header word at +3 is $0101 for bytes, else byte pairs)
 * to WRAM through the port: a negative count n copies ~n + 1 literals, a positive one repeats the
 * next byte (pair) n + 1 times, 0 ends */
void DecompressRLE(uint8_t db, uint16_t y)
{
    y = (uint16_t)(y + 3);
    int bytes = (db_r8(db, y) | db_r8(db, (uint16_t)(y + 1)) << 8) == 0x0101;
    y++;
    for (;;) {
        y++;
        uint8_t v = db_r8(db, y);
        if (v & 0x80) {
            int16_t x = (uint8_t)(v ^ 0xFF);
            do {
                y++; wmdata_w(db_r8(db, y));
                if (!bytes) { y++; wmdata_w(db_r8(db, y)); }
            } while (--x >= 0);
            continue;
        }
        if (!v) return;
        int16_t x = v;
        y++;
        uint8_t b0 = db_r8(db, y), b1 = 0;
        if (!bytes) { y++; b1 = db_r8(db, y); }
        do {
            wmdata_w(b0);
            if (!bytes) wmdata_w(b1);
        } while (--x >= 0);
    }
}

/* LZ data at [p0] (its commands from +5) to WRAM through the port (LZOutAddr follows it, from p1; copies come
 * from the WRAM already written, at LZCopySrc). The four command forms by the top bits: a short and
 * a long copy after up to 3 literals, a copy with a 16-bit distance, a literal run; a run of $71 or
 * more ends it after up to 3 literals. The command bytes' words keep their zeroed high bytes. */
static uint16_t lz_lit(uint32_t p0, uint16_t y, uint16_t n)
{
    do { wmdata_w(m8(p0 + y)); y++; } while (--n);
    return y;
}
/* (DecompressLZ: a copy of n bytes to the port from the output already written, at LZCopySrc) */
static void lz_copy(uint16_t n)
{
    uint32_t src = r16(RAM_OFF_LZCopySrc) | (uint32_t)r8(RAM_OFF_LZCopySrc + 2) << 16;
    uint16_t k = 0;
    do { wmdata_w(m8(src + k)); k++; } while (--n);
}
static void lz_out(uint16_t n) { w16(RAM_OFF_LZOutAddr, (uint16_t)(n + r16(RAM_OFF_LZOutAddr))); }
/* (DecompressLZ: a copy's distance into LZDistance, and its source LZCopySrc dist + 1 bytes back
 * from LZOutAddr) */
static void lz_from(uint16_t dist)
{
    w16(RAM_OFF_LZDistance, dist);
    w16(RAM_OFF_LZCopySrc, (uint16_t)(r16(RAM_OFF_LZOutAddr) - dist - 1));
}
/* Decompress's format $FB10: the LZ data at [p0] to WRAM at p1, command by command until the end
 * run */
void DecompressLZ(uint32_t p0, uint32_t p1)
{
    p0 &= 0xFFFFFF;
    w16(RAM_OFF_LZOutAddr, (uint16_t)p1);
    w16(RAM_OFF_LZCopySrc + 2, (uint16_t)(p1 >> 16));
    w16(RAM_OFF_LZCmd, 0); w16(RAM_OFF_LZCmdByte2, 0); w16(RAM_OFF_LZCmdByte3, 0); w16(RAM_OFF_LZLenLo, 0);
    uint16_t y = 5;
    for (;;) {
        hal_cost_mark(SYM_DecompressLZ_2);
        uint8_t cmd = m8(p0 + y);
        y++;
        w8(RAM_OFF_LZCmd, cmd);
        uint16_t n;
        if (!(cmd & 0x80)) {
            w8(RAM_OFF_LZCmdByte2, m8(p0 + y)); y++;
            n = cmd & 3;
            if (n) { lz_out(n); y = lz_lit(p0, y, n); }
            lz_from((uint16_t)(((r16(RAM_OFF_LZCmd) & 0x60) << 3) + r16(RAM_OFF_LZCmdByte2)));
            n = (uint16_t)(((r16(RAM_OFF_LZCmd) & 0x1C) >> 2) + 3);
        } else if (!(cmd & 0x40)) {
            w8(RAM_OFF_LZCmdByte2, m8(p0 + y)); y++;
            w8(RAM_OFF_LZCmdByte3, m8(p0 + y)); y++;
            n = (uint16_t)(r16(RAM_OFF_LZCmdByte2) << 2) >> 8 & 3;
            if (n) { lz_out(n); y = lz_lit(p0, y, n); }
            lz_from((uint16_t)(((r16(RAM_OFF_LZCmdByte2) & 0x3F) << 8) + r16(RAM_OFF_LZCmdByte3)));
            n = (uint16_t)((r16(RAM_OFF_LZCmd) & 0x3F) + 4);
        } else if (!(cmd & 0x20)) {
            n = cmd & 3;
            lz_out(n);
            uint16_t d = m16(p0 + y);
            w16(RAM_OFF_LZDistance, (uint16_t)(d << 8 | d >> 8));
            y = (uint16_t)(y + 2);
            w8(RAM_OFF_LZLenLo, m8(p0 + y)); y++;
            if (n) y = lz_lit(p0, y, n);
            lz_from(r16(RAM_OFF_LZDistance));
            uint32_t s = (uint32_t)(uint16_t)(((r16(RAM_OFF_LZCmd) & 0x0C) >> 2) << 8) + r16(RAM_OFF_LZLenLo);
            n = (uint16_t)(s + 5 + (s >> 16));
        } else {
            uint8_t len = (uint8_t)(((cmd & 0x1F) << 2) + 4);
            if (len >= 0x71) {
                n = cmd & 3;
                if (!n) return;
                lz_out(n);
                lz_lit(p0, y, n);
                return;
            }
            lz_out(len);
            y = lz_lit(p0, y, len);
            continue;
        }
        lz_out(n);
        w16(RAM_OFF_LZSrcIdx, y);
        lz_copy(n);
        y = r16(RAM_OFF_LZSrcIdx);
    }
}

/* Byte pair data at y (data bank db) to WRAM through the port: an escape byte (flag 1) takes the
 * next byte as a literal (0 ends), a pair code (flag $FF, from the table after the header)
 * expands to its two bytes, recursively; DecompBuf holds the flags, DecompTab1 / DecompTab2 the
 * pairs. The expansion is unrolled two levels as in the original, which takes an escape code met
 * as a pair's first byte at those levels for a literal. */
static uint8_t pr_db;
static uint8_t pr_buf(uint16_t x) { return db_r8(pr_db, (uint16_t)(0x0100 + x)); }
static uint16_t pr_t1(uint16_t x) { return db_r8(pr_db, (uint16_t)(0x0500 + x)); }
static uint16_t pr_t2(uint16_t x) { return db_r8(pr_db, (uint16_t)(0x0600 + x)); }
static uint16_t pairs12(uint16_t x)   /* (.12: expands pair x but its last byte, left in X) */
{
    for (;;) {
        uint16_t sv = x;
        x = pr_t1(x);
        if (pr_buf(x)) x = pairs12(x);
        wmdata_w((uint8_t)x);
        x = pr_t2(sv);
        if (!pr_buf(x)) return x;
    }
}
/* Decompress's format $FB46: y the data after the format word (from y+3 the escape code, the pair
 * count and the pairs: code, first, second; then the packed bytes), expanded to WRAM */
void DecompressPairs(uint8_t db, uint16_t y)
{
    uint16_t st[8];
    int sp = 0;
    pr_db = db;
    y = (uint16_t)(y + 2);
    for (int x = 0x40; x; ) {
        x -= 2;
        for (int q = 0; q < 0x100; q += 0x40) { db_w8(db, (uint16_t)(0x0100 + q + x), 0); db_w8(db, (uint16_t)(0x0101 + q + x), 0); }
    }
    y++;
    uint16_t x = db_r8(db, y);
    db_w8(db, (uint16_t)(0x0100 + x), (uint8_t)(pr_buf(x) + 1));
    y++;
    uint8_t cnt = db_r8(db, y);
    g.r0[0] = (uint16_t)((g.r0[0] & 0xFF00) | cnt);
    while (cnt) {
        y++; x = db_r8(db, y);
        y++; db_w8(db, (uint16_t)(0x0500 + x), db_r8(db, y));
        y++; db_w8(db, (uint16_t)(0x0600 + x), db_r8(db, y));
        db_w8(db, (uint16_t)(0x0100 + x), (uint8_t)(pr_buf(x) - 1));
        cnt--;
    }
    g.r0[0] &= 0xFF00;
L7:
    y++; x = db_r8(db, y);
    if (!pr_buf(x)) goto L6;
    if (!(pr_buf(x) & 0x80)) goto L5;
    goto L8;
L6:
    wmdata_w((uint8_t)x);
    goto L7;
L5:
    for (;;) {
        y++;
        uint8_t v = db_r8(db, y);
        if (!v) return;
        wmdata_w(v);
        y++; x = db_r8(db, y);
        if (!pr_buf(x)) goto L6;
        if (pr_buf(x) & 0x80) goto L8;
    }
L8:
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x) & 0x80) goto L11;
L9:
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (!pr_buf(x)) goto L6;
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x) & 0x80) goto L2;
L10:
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (!pr_buf(x)) goto L6;
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x)) x = pairs12(x);
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (!pr_buf(x)) goto L6;
    x = pairs12(x);
    goto L6;
L2:
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x)) x = pairs12(x);
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (pr_buf(x)) x = pairs12(x);
    goto L10;
L11:
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x) & 0x80) goto L3x;
L2x:
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (!pr_buf(x)) goto L9;
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x)) x = pairs12(x);
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (!pr_buf(x)) goto L9;
    x = pairs12(x);
    goto L9;
L3x:
    st[sp++] = x; x = pr_t1(x);
    if (pr_buf(x)) x = pairs12(x);
    wmdata_w((uint8_t)x); x = pr_t2(st[--sp]);
    if (pr_buf(x)) x = pairs12(x);
    goto L2x;
}

/* ---- sprites: the tile cache, its VRAM allocators and the OAM ---- */

/* The sprite VRAM allocators: BlockRuns (16x16 blocks, from tile 0) and TileRuns (8x8 tiles,
 * after SpriteBlockTiles) are byte lists of runs, each run's first byte its length (bit 7 used;
 * a block run of a chain of 8-unit runs has bit 6), with the run before it in BlockRunPrev /
 * TileRunPrev ($FF none); a length of 0 ends the list. All of it 8-bit, as the original. */
static uint8_t br(uint8_t x) { return r8(RAM_OFF_BlockRuns + x); }
static void br_w(uint8_t x, uint8_t v) { w8(RAM_OFF_BlockRuns + x, v); }
static void brp_w(uint8_t x, uint8_t v) { w8(RAM_OFF_BlockRunPrev + x, v); }
static uint8_t tr(uint8_t x) { return r8(RAM_OFF_TileRuns + x); }
static void tr_w(uint8_t x, uint8_t v) { w8(RAM_OFF_TileRuns + x, v); }
static void trp_w(uint8_t x, uint8_t v) { w8(RAM_OFF_TileRunPrev + x, v); }

/* (split run x of length len at need: the rest a free run after it, which points back at x, and
 * the run after the rest at the rest; the second add takes the first's carry) */
static void split_run(void (*runs_w)(uint8_t, uint8_t), void (*prev_w)(uint8_t, uint8_t), uint8_t x, uint8_t len, uint8_t need)
{
    uint8_t rest = (uint8_t)(len - need);
    if (!rest) return;
    unsigned s = (unsigned)x + need;
    uint8_t y = (uint8_t)s;
    runs_w(y, rest);
    prev_w(y, x);
    prev_w((uint8_t)(y + rest + (s >> 8)), y);
}

/* (record the frame in the tile cache: its entry number by frame number, address, bank, number,
 * allocation, first tile; the entry's VRAM address, tile * 16 + SpriteVRAM with the carry of the
 * last shift) */
static uint16_t cache_add(uint32_t p0, uint16_t alloc, uint16_t tile)
{
    w8(RAM_OFF_FrameCacheOf + 1 + g.DrawFrameNum, (uint8_t)g.FrameCacheCount);
    uint16_t y = (uint16_t)(g.FrameCacheCount << 1);
    w16(RAM_OFF_FrameCacheNum + y, g.DrawFrameNum);
    w16(RAM_OFF_FrameCacheAddr + y, (uint16_t)p0);
    w16(RAM_OFF_FrameCacheBank + y, (uint16_t)(p0 >> 16));
    g.FrameCacheCount++;
    w16(RAM_OFF_FrameCacheAlloc + y, alloc);
    w16(RAM_OFF_FrameCacheTile + y, tile);
    return (uint16_t)((uint16_t)(tile << 4) + g.SpriteVRAM + (tile >> 12 & 1));
}
/* (LoadFrameTiles: DMA queue entry x, type 1 (DMAP), len bytes from bank:src; the caller stores
 * the VRAM address at +6) */
static void dma_entry(uint16_t x, uint16_t src, uint8_t bank, uint16_t len)
{
    w8(RAM_OFF_DMAQueue + x, 1);
    w16(RAM_OFF_DMAQueue + x + 1, src);
    w8(RAM_OFF_DMAQueue + x + 3, bank);
    w16(RAM_OFF_DMAQueue + x + 4, len);
}
static uint16_t cache_result(void) { return (uint16_t)((g.FrameCacheCount - 1) << 1); }

/* Allocate sprite VRAM for the frame at [p0] (p0 the direct page's 4 bytes: the cache keeps the word
 * at p0+2), queue the DMA of its tiles (one or two rows: PieceX / PieceY bytes) and add a tile cache
 * entry: its entry * 2, $FFFF when there is no room or all 128 entries are in use, x when the
 * frame has no pieces. A negative piece count is a frame of 16x16 blocks (in units of 4 tiles,
 * more than 8 taking a chain of 8-unit runs), else of 8x8 tiles. */
uint16_t LoadFrameTiles(uint32_t p0, uint16_t x)
{
    uint32_t f = p0 & 0xFFFFFF;
    if (g.FrameCacheCount == 0x0080) return 0xFFFF;
    uint8_t bank = (uint8_t)(p0 >> 16);
    uint16_t r0 = m16(f);
    if (!r0) return x;
    g.PieceX = m16(f + 0x0C);
    g.PieceY = m16(f + 0x0E);
    uint16_t src = (uint16_t)(m16(f + 0x10) + (uint16_t)p0);
    if (!(r0 & 0x8000)) {
        uint16_t need = m16(f + 2);
        uint8_t k = 0;
        for (;;) {
            uint8_t v = tr(k);
            if (!v) return 0xFFFF;
            if (!(v & 0x80) && v >= (uint8_t)need) break;
            k = (uint8_t)((k + v) & 0x7F);
        }
        split_run(tr_w, trp_w, k, tr(k), (uint8_t)need);
        tr_w(k, (uint8_t)need | 0x80);
        uint16_t t = g.DMAQueueTail;
        dma_entry(t, src, bank, g.PieceX);
        uint16_t vram = cache_add(p0, k, (uint16_t)(k + g.SpriteBlockTiles));
        w16(RAM_OFF_DMAQueue + t + 6, vram);
        g.DMAQueueTail = (t + 8) & 0x03FF;
        return cache_result();
    }
    uint16_t need = (uint16_t)((uint16_t)(m16(f + 2) + 3) & 0xFFFC) >> 2;
    uint16_t run;
    if (need < 9) {
        uint8_t k = 0;
        for (;;) {
            uint8_t v = br(k);
            if (!v) return 0xFFFF;
            if (v < 0x10 && v >= (uint8_t)need) break;
            k = (uint8_t)(k + (v & 0x3F));
        }
        run = k;
        split_run(br_w, brp_w, k, br(k), (uint8_t)need);
        br_w(k, (uint8_t)need | 0x80);
    } else {
        uint8_t k = 0, sum = 0, first = 0;
        for (;;) {
            uint8_t v = br(k);
            if (!v) return 0xFFFF;
            if (v == 8) {
                first = k;
                sum = v;
                for (;;) {
                    k = (uint8_t)(k + br(k));
                    v = br(k);
                    if (!v || v >= 0x10) break;
                    sum = (uint8_t)(v + sum);
                }
                if (sum >= (uint8_t)need) break;
                v = br(k);
            }
            k = (uint8_t)(k + (v & 0x3F));
        }
        k = first;
        run = k;
        uint8_t left = (uint8_t)need;
        for (;;) {
            uint8_t d = (uint8_t)(left - br(k));
            if (!d) break;
            if (d & 0x80) {
                uint8_t rest = (uint8_t)-d;
                unsigned s = (unsigned)k + left;
                uint8_t y = (uint8_t)s;
                br_w(y, rest);
                brp_w(y, k);
                brp_w((uint8_t)(y + rest + (s >> 8)), y);
                break;
            }
            left = d;
            br_w(k, 0xC8);
            k = (uint8_t)(k + 8);
        }
        br_w(k, left | 0x80);
        uint16_t t = g.DMAQueueTail;
        dma_entry(t, src, bank, g.PieceX);
        src = (uint16_t)(g.PieceX + src);
        uint16_t vram = cache_add(p0, run | 0x8000, (uint16_t)(((run & 0xFFF8) << 1) + (run & 7)) << 1);
        w16(RAM_OFF_DMAQueue + t + 6, vram);
        t = (t + 8) & 0x03FF;
        if (!g.PieceY) {
            g.DMAQueueTail = t;
            return cache_result();
        }
        dma_entry(t, src, bank, g.PieceY);
        w16(RAM_OFF_DMAQueue + t + 6, (uint16_t)((((g.PieceX >> 1) & 0xFF00) | 0x0100) + vram));
        g.DMAQueueTail = (t + 8) & 0x03FF;
        return cache_result();
    }
    uint16_t t = g.DMAQueueTail;
    dma_entry(t, src, bank, g.PieceX);
    src = (uint16_t)(g.PieceX + src);
    uint16_t vram = cache_add(p0, run | 0x8000, (uint16_t)(((run & 0xFFF8) << 1) + (run & 7)) << 1);
    w16(RAM_OFF_DMAQueue + t + 6, vram);
    t = (t + 8) & 0x03FF;
    dma_entry(t, src, bank, g.PieceY);
    if (!g.PieceY) {
        g.DMAQueueTail = t;
        return cache_result();
    }
    w16(RAM_OFF_DMAQueue + t + 6, vram | 0x0100);
    g.DMAQueueTail = (t + 8) & 0x03FF;
    return cache_result();
}

/* The tile cache entry (* 2) of the frame at [p0], searching y entries from x; else LoadFrameTiles */
uint16_t FindCachedFrame(uint32_t p0, uint16_t x, uint16_t y)
{
    do {
        if (r16(RAM_OFF_FrameCacheAddr + x) == (uint16_t)p0 && r16(RAM_OFF_FrameCacheBank + x) == (uint16_t)(p0 >> 16)) return x;
        x += 2;
    } while (--y);
    return LoadFrameTiles(p0, x);
}

/* The first VRAM tile of the frame at [p0] (its entry by frame number with FrameByNumber, else by
 * address), loaded now if needed (x as the original's X there), its use mark refreshed;
 * FrameTilesFail and $FFFF when there is no room */
uint16_t GetFrameTiles(uint32_t p0, uint16_t x)
{
    uint16_t y = g.FrameCacheCount;
    if (y) {
        if (!g.FrameByNumber) {
            x = 0;
            do {
                hal_cost_mark(SYM_GetFrameTiles_5);
                if (r16(RAM_OFF_FrameCacheAddr + x) == (uint16_t)p0 && r16(RAM_OFF_FrameCacheBank + x) == (uint16_t)(p0 >> 16)) goto found;
                x += 2;
            } while (--y);
        } else {
            x = g.DrawFrameNum;
            uint16_t a = r16(RAM_OFF_FrameCacheOf + x);
            if (!(a & 0x8000)) { x = (uint16_t)((a >> 8) << 1); goto found; }
        }
    }
    x = LoadFrameTiles(p0, x);
    if (x & 0x8000) { g.FrameTilesFail = 0xFFFF; return 0xFFFF; }
found:
    w16(RAM_OFF_FrameCacheUse + x, g.CacheUseMark);
    return r16(RAM_OFF_FrameCacheTile + x);
}

/* (one OAM entry for the current piece at (px, py), if on screen: the tile word (piece tile + first
 * tile, xor r3) and the high table's size and X bits; mode: 0 plain, 1 mirrored, 2 flipped (x
 * mirrored too when r3 has $4000)) */
static int piece_entry(int mode, uint16_t r3, uint16_t tile0, uint16_t r5, uint16_t r5h, uint16_t *ox)
{
    uint16_t ctr = r16(RAM_OFF_PieceSize) ? 0x000F : 0x0007, py, px;
    py = mode == 2 ? (uint16_t)(r5h - g.PieceY - ctr) : (uint16_t)(g.PieceY + r5h);
    if ((uint16_t)(py - 0xFFF0) & 0x8000) return 0;
    if (!((uint16_t)(py - 0x00E0) & 0x8000)) return 0;
    if (mode == 1 || (mode == 2 && (r3 & 0x4000))) px = (uint16_t)(r5 - g.PieceX - ctr);
    else px = (uint16_t)(g.PieceX + r5);
    if (!((uint16_t)(px - 0x0100) & 0x8000)) return 0;
    if ((uint16_t)(px - 0xFFF0) & 0x8000) return 0;
    uint16_t x = *ox;
    w16(x, px);
    w16(x + 1, py);
    w16(x + 2, (uint16_t)((uint16_t)(g.PieceTile + tile0) ^ r3));
    uint16_t hp = g.OAMHiPtr, sh = g.OAMHiShift;
    uint8_t v = r8(hp), sz = r8(RAM_OFF_PieceSize);
    uint32_t m = RAM_OFF_OAMHiMasks + sh;
    if (px & 0x8000) v = sz ? (uint8_t)(v | r8(m + 12)) : (uint8_t)((v & r8(m)) | r8(m + 4));
    else v = sz ? (uint8_t)((v & r8(m)) | r8(m + 8)) : (uint8_t)(v & r8(m));
    w8(hp, v);
    g.SpritesLeft--;
    sh--;
    if (sh & 0x8000) { g.OAMHiShift = 3; g.OAMHiPtr++; }
    else g.OAMHiShift = sh;
    if (mode < 2) hal_cost_mark(mode ? SYM_DrawSprite_draw_4x : SYM_DrawSprite_draw_4);   /* (a piece drawn) */
    g.OAMNext = (uint16_t)(g.OAMNext + 4);
    *ox = g.OAMNext;
    return 1;
}
/* (DrawSprite_draw's piece loop: an OAM entry for each of the r0 pieces of the frame at [p0], 7
 * bytes each from +$16 (X, Y, tile, size), while sprites are left) */
static void draw_pieces(int mode, uint32_t p0, uint16_t r3, uint16_t tile0, uint16_t r5, uint16_t r5h, uint16_t r0, uint16_t x)
{
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t y = 0x0016;
    do {
        if (mode < 2) hal_cost_mark(mode ? SYM_DrawSprite_draw_8 : SYM_DrawSprite_draw_2);   /* (the piece loop's head) */
        if (!g.SpritesLeft) return;
        g.PieceX = m16(f + y);
        g.PieceY = m16(f + (uint16_t)(y + 2));
        g.PieceTile = m16(f + (uint16_t)(y + 4));
        w16(RAM_OFF_PieceSize, m8(f + (uint16_t)(y + 6)));
        g.PieceNext = (uint16_t)(y + 7);
        piece_entry(mode, r3, tile0, r5, r5h, &x);
        y = g.PieceNext;
    } while (--r0);
}

/* Draw the frame at [p0] at (r5, r5h), r3 xored into each tile word (bit 15: flipped, the pieces'
 * positions mirrored, DrawSpritePieces; bit 14: mirrored); x is the original's X at the tile cache
 * lookup */
void DrawSprite_draw(uint32_t p0, uint16_t r3, uint16_t r5, uint16_t r5h, uint16_t x)
{
    uint16_t tile0 = GetFrameTiles(p0, x);
    if (tile0 & 0x8000) return;
    uint16_t r0 = m8(p0 & 0xFFFFFF);
    draw_pieces(r3 & 0x8000 ? 2 : r3 & 0x4000 ? 1 : 0, p0, r3, tile0, r5, r5h, r0, g.OAMNext);
}

/* Draw frame `frame` of the sprite set at [p1] (p1 the direct page's 4 bytes) at (x, y), a xored into
 * the tile and attribute word */
void DrawSprite(uint16_t a, uint16_t x, uint16_t y, uint16_t frame, uint32_t p1)
{
    g.DrawFrameNum = frame;
    uint32_t s = p1 & 0xFFFFFF;
    uint16_t k = (uint16_t)(frame << 2);
    uint32_t lo = (uint32_t)m16(s + k) + (uint16_t)p1;
    uint16_t hi = (uint16_t)(m16(s + (uint16_t)(k + 2)) + (uint16_t)(p1 >> 16) + (lo >> 16));
    DrawSprite_draw((uint16_t)lo | (uint32_t)hi << 16, a, x, y, x);
}

/* (unused entries) DrawSprite with the frame at [p0]; the tile cache entry of the frame at [p0] or
 * of frame a of the sprite set at [p1], loading it when needed */
void GetFrameTiles_drawat(uint32_t p0, uint16_t a, uint16_t x, uint16_t y) { DrawSprite_draw(p0, a, x, y, x); }
/* (unused entry) The tile cache entry * 2 of the frame at [p0], found by its number with
 * FrameByNumber, else by its address; LoadFrameTiles when it is not cached */
uint16_t DrawSpritePieces_cacheat(uint32_t p0, uint16_t x)
{
    uint16_t y = g.FrameCacheCount;
    if (!y) return LoadFrameTiles(p0, x);
    if (!g.FrameByNumber) return FindCachedFrame(p0, 0, y);
    x = g.DrawFrameNum;
    uint16_t a = r16(RAM_OFF_FrameCacheOf + x);
    if (a & 0x8000) return LoadFrameTiles(p0, x);
    return (uint16_t)((a >> 8) << 1);
}
/* (unused entry) DrawSpritePieces_cacheat for frame a of the sprite set at [p1] (DrawFrameNum = a) */
uint16_t DrawSpritePieces_cacheframe(uint16_t a, uint32_t p1, uint16_t x)
{
    g.DrawFrameNum = a;
    uint32_t s = p1 & 0xFFFFFF;
    uint16_t k = (uint16_t)(a << 2);
    uint32_t lo = (uint32_t)m16(s + k) + (uint16_t)p1;
    uint16_t hi = (uint16_t)(m16(s + (uint16_t)(k + 2)) + (uint16_t)(p1 >> 16) + (lo >> 16));
    return DrawSpritePieces_cacheat((uint16_t)lo | (uint32_t)hi << 16, x);
}

/* Free tile cache entry x: the last entry moves into its slot (its frame pointed at it), and its
 * VRAM run is given back, joined with a free run after it (blocks: in the same row of 8) and with
 * a free run before it */
void FreeCacheEntry_go(uint16_t x)
{
    uint16_t r0 = r16(RAM_OFF_FrameCacheAlloc + x);
    if (--g.FrameCacheCount) {
        uint16_t y = (uint16_t)(g.FrameCacheCount << 1);
        if (y != x) {
            static const uint32_t f[] = { RAM_OFF_FrameCacheAddr, RAM_OFF_FrameCacheBank, RAM_OFF_FrameCacheNum, RAM_OFF_FrameCacheAlloc, RAM_OFF_FrameCacheTile, RAM_OFF_FrameCacheUse };
            for (int i = 0; i < 6; i++) w16(f[i] + x, r16(f[i] + y));
            w8(RAM_OFF_FrameCacheOf + 1 + r16(RAM_OFF_FrameCacheNum + x), (uint8_t)(x >> 1));
        }
    }
    uint8_t k = (uint8_t)r0;
    if (!(r0 & 0x8000)) {
        uint8_t len = tr(k) & 0x7F;
        uint8_t y = (uint8_t)(len + k);
        if (!(tr(y) & 0x80)) len = (uint8_t)(tr(y) + len);
        uint8_t p = r8(RAM_OFF_TileRunPrev + k);
        if (!(p & 0x80) && !(tr(p) & 0x80)) {
            unsigned s = (unsigned)tr(p) + len;
            tr_w(p, (uint8_t)s);
            trp_w((uint8_t)(p + (uint8_t)s + (s >> 8)), p);
            return;
        }
        tr_w(k, len);
        trp_w((uint8_t)(k + len), k);
        return;
    }
    if (br(k) & 0x40) {
        while (br(k) & 0x40) { br_w(k, 8); k = (uint8_t)(k + 8); }
        uint8_t len = br(k) & 0x3F;
        uint8_t y = (uint8_t)(k + len);
        if ((y & 7) && !(br(y) & 0x80)) len = (uint8_t)(br(y) + len);
        br_w(k, len);
        brp_w((uint8_t)(k + len), k);
        return;
    }
    uint8_t len = br(k) & 0x3F;
    uint8_t y = (uint8_t)(len + k);
    if ((y & 7) && !(br(y) & 0x80)) len = (uint8_t)(br(y) + len);
    if (k & 7) {
        uint8_t p = r8(RAM_OFF_BlockRunPrev + k);
        if (!(p & 0x80) && !(br(p) & 0x80)) {
            unsigned s = (unsigned)br(p) + len;
            br_w(p, (uint8_t)s);
            brp_w((uint8_t)(p + (uint8_t)s + (s >> 8)), p);
            return;
        }
    }
    br_w(k, len);
    brp_w((uint8_t)(k + len), k);
}

/* Free entry x of frame number y (its FrameCacheOf slot cleared); _A: the entry in a's high byte */
void FreeCacheEntry(uint16_t x, uint16_t y)
{
    w8(RAM_OFF_FrameCacheOf + 1 + y, 0xFF);
    FreeCacheEntry_go(x);
}
void FreeCacheEntryA(uint16_t a, uint16_t y) { FreeCacheEntry((uint16_t)((a >> 8 & 0xFF) << 1), y); }

/* Free the VRAM tiles of the frame at [p0] (by its number with FrameByNumber); not cached: nothing.
 * Found by its address the frame keeps its FrameCacheOf slot. */
void FreeFrameTiles(uint32_t p0)
{
    uint16_t y = g.FrameCacheCount;
    if (!y) return;
    if (g.FrameByNumber) {
        uint16_t a = r16(RAM_OFF_FrameCacheOf + g.DrawFrameNum);
        if (!(a & 0x8000)) FreeCacheEntryA(a, g.DrawFrameNum);
        return;
    }
    uint16_t x = 0;
    do {
        if (r16(RAM_OFF_FrameCacheAddr + x) == (uint16_t)p0 && r16(RAM_OFF_FrameCacheBank + x) == (uint16_t)(p0 >> 16)) { FreeCacheEntry_go(x); return; }
        x += 2;
    } while (--y);
}

/* (unused entries) FreeFrameTiles for frame a of the sprite set at [p1]; the sum of the free
 * block run lengths, walking from index SpriteBlockTiles */
void FreeFrameTiles_t1(uint16_t a, uint32_t p1)
{
    g.DrawFrameNum = a;
    uint32_t s = p1 & 0xFFFFFF;
    uint16_t k = (uint16_t)(a << 2);
    uint32_t lo = (uint32_t)m16(s + k) + (uint16_t)p1;
    uint16_t hi = (uint16_t)(m16(s + (uint16_t)(k + 2)) + (uint16_t)(p1 >> 16) + (lo >> 16));
    FreeFrameTiles((uint16_t)lo | (uint32_t)hi << 16);
}
/* (unused entry) The sum of the free block run lengths, walking BlockRuns from SpriteBlockTiles
 * (after a free run the step is the sum so far, as the original) */
uint16_t FreeFrameTiles_t2(void)
{
    uint16_t sum = 0, x = g.SpriteBlockTiles;
    for (;;) {
        uint16_t a = r8(RAM_OFF_BlockRuns + x);
        if (!a) return sum;
        if (a < 0x10) a = sum = (uint16_t)(a + sum);   /* (the step is then the sum's) */
        x = (uint16_t)(x + (a & 0x3F));
    }
}

/* Start filling OAMDrawBuf after the reserved entries (the high table's shift from the byte offset,
 * as the original); age the tile cache's use marks and free the entries not drawn since */
void SpriteFrameStart(void)
{
    uint16_t sr = g.SpritesReserved;
    g.SpritesLeft = (uint16_t)(0x0080 - sr);
    uint16_t x = sr >> 2;
    uint32_t s = (uint32_t)x + g.OAMDrawBuf;
    g.OAMHiPtr = (uint16_t)(s + 0x0200 + (s >> 16));
    g.OAMHiShift = (x & 3) ^ 3;
    g.OAMNext = (uint16_t)((uint16_t)(sr << 2) + g.OAMDrawBuf + (sr >> 14 & 1));
    if (!g.FrameCacheCount) return;
    for (int16_t k = (int16_t)((g.FrameCacheCount - 1) << 1); k >= 0; k -= 2) {
        uint16_t u = r16(RAM_OFF_FrameCacheUse + k);
        if (u & 0x8000) continue;
        w16(RAM_OFF_FrameCacheUse + k, u >> 1);
        if (!(u & 1)) continue;
        FreeCacheEntry((uint16_t)k, r16(RAM_OFF_FrameCacheNum + k));
    }
}

/* ---- colours, screen maps, letters, the IRQ ---- */

/* DMA x colours from [p0] to CGRAM at colour y: at once with the screen blanked (none for x = 0;
 * y is a byte there, CGADD), else left for the NMI (CGDMALen etc.; the colour word as y is) */
void DMAToCGRAM(uint32_t p0, uint16_t x, uint16_t y)
{
    if (g.IniDispShadow[0] & 0x80) {
        io_w8(0x2121, (uint8_t)y);
        uint16_t n = (uint16_t)(x << 1);
        if (!n) return;
        io_w16(0x4315, n);
        io_w16(0x4310, 0x2200);
        io_w16(0x4312, (uint16_t)p0);
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x420B, 0x02);
        return;
    }
    w16(RAM_OFF_CGDMAColour, y);
    w16(RAM_OFF_CGDMABank, (uint16_t)(p0 >> 16));
    g.CGDMASrc = (uint16_t)p0;
    g.CGDMALen = (uint16_t)(x << 1);
}

/* (unused entry) x colours from colour y of [p0] into [p1]: a = 0 fills them with y * 2 (the
 * index, as the original leaves it in A), $0100 copies them, else scales each component by the
 * byte a / 256 (the multiplier) */
void DMAToCGRAM_t3(uint32_t p0, uint32_t p1, uint16_t a, uint16_t x, uint16_t y)
{
    uint16_t k = (uint16_t)(y << 1);
    if (!a) { do { mset16(p1 + k, (uint16_t)(y << 1)); k += 2; } while (--x); return; }
    if (a == 0x0100) { do { mset16(p1 + k, m16(p0 + k)); k += 2; } while (--x); return; }
    uint16_t f = a & 0xFF;
    do {
        uint16_t c = m16(p0 + k);
        uint16_t v = (uint16_t)((c & 0x1F) * f) >> 8 & 0x1F;
        v |= (uint16_t)(((c >> 5) & 0x1F) * f) >> 3 & 0x03E0;
        v |= (uint16_t)((uint16_t)(((c >> 10) & 0x1F) * f) << 2) & 0x7C00;
        mset16(p1 + k, v);
        k += 2;
    } while (--x);
}

/* Send the 256 colours of the colour buffer */
void SendColours(void) { DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf, 0x0100, 0); }

/* Store at y:x the colour of this moment (vcount) of a 60-frame pulse: white to $7E10 and back,
 * each component interpolated over 30 frames (the divides' remainders as the divider leaves them) */
void PulseColour(uint16_t x, uint16_t y)
{
    uint16_t t = (uint16_t)(g.vcount[0] % 30), half = (uint16_t)(g.vcount[0] / 30) % 2;
    uint32_t tab = SYM_PulseColour_1 + (uint16_t)(half << 1);
    uint16_t c0 = rom16(tab), c1 = rom16(tab + 2);
    static const int sh[3] = { 10, 5, 0 };
    uint16_t col = 0;
    for (int i = 0; i < 3; i++) {
        uint16_t from = sh[i] == 10 ? (c0 >> 8 & 0xFF) >> 2 : c0 >> sh[i] & 0x1F;
        uint16_t to = sh[i] == 10 ? (c1 >> 8 & 0xFF) >> 2 : c1 >> sh[i] & 0x1F;
        uint16_t q = Div32S(Mul16S((uint16_t)(to - from), t), 0x001E);
        col |= (uint16_t)(((uint16_t)(q + from) & 0x1F) << sh[i]);
    }
    mset16((uint32_t)y << 16 | x, col);
}

/* (the screen map in VRAM: the word address of column c, row r) */
static uint16_t map_addr(uint16_t c, uint16_t r)
{
    uint16_t s = (uint16_t)(r << 4);
    return (uint16_t)(((uint16_t)(s << 1) + g.MapVRAM + (s >> 15)) | c);
}
/* (MapToVRAM with the screen blanked: channel 1's size n, source src and VMADD vram for a row; the
 * caller starts the DMA) */
static void map_dma_now(uint16_t n, uint16_t src, uint16_t vram)
{
    io_w16(0x4315, n);
    io_w16(0x4312, src);
    io_w16(0x2116, vram);
}

/* Copy map [p0] (width, height, then the entries from +6) into the screen map in VRAM at column x,
 * row y, a row at a time (a row crossing column 32 in two, the rest on the next screen); at once
 * with the screen blanked, else queued */
void MapToVRAM(uint32_t p0, uint16_t x, uint16_t y)
{
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t c = x, row = y, w = m16(f), h = m16(f + 2);
    uint16_t src = (uint16_t)((uint16_t)p0 + 6), v = map_addr(c, row);
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x4310, 0x01);
        io_w8(0x4311, 0x18);
        do {
            if ((uint16_t)(w + c) < 0x0021) map_dma_now((uint16_t)(w << 1), src, v);
            else {
                uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
                map_dma_now(r6, src, v);
                io_w8(0x420B, 0x02);
                io_w16(0x4315, (uint16_t)((uint16_t)(w << 1) - r6));
                uint32_t s = (uint32_t)src + r6;
                io_w16(0x4312, (uint16_t)s);
                io_w16(0x2116, (uint16_t)(v + 0x0400 + (s >> 16)) & 0xFFE0);
            }
            io_w8(0x420B, 0x02);
            uint32_t s = (uint32_t)src + w;
            src = (uint16_t)(s + w + (s >> 16));
            v = (uint16_t)(v + 0x20);
            if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
        } while (--h);
        return;
    }
    uint32_t q = (p0 & 0xFF0000) | src;
    do {
        if ((uint16_t)(w + c) < 0x0021) QueueVRAMDMA(&q, (uint16_t)(w << 1), v);
        else {
            uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
            QueueVRAMDMA(&q, r6, v);
            uint16_t n2 = (uint16_t)((uint16_t)(w << 1) - r6);
            unsigned nb = (uint16_t)(w << 1) >= r6;
            QueueVRAMDMA(&q, n2, (uint16_t)(v + 0x0400 + nb) & 0xFFE0);
        }
        v = (uint16_t)(v + 0x20);
        if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
    } while (--h);
}

/* Copy a part of map [p0] (its width at +0): r4 entries by r4+2 rows from column r7, row r7+2, into
 * the screen map at column x (32 or more: the next screen), row y; at once or queued as MapToVRAM */
void MapPartToVRAM(uint32_t p0, uint16_t x, uint16_t y, uint16_t r7, uint16_t r7h, uint16_t r4, uint16_t r4h)
{
    if (x >= 0x0020) { x &= 0x1F; g.MapVRAM = (uint16_t)(g.MapVRAM + 0x0400); }
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t stride = (uint16_t)(m16(f) << 1);
    uint16_t r2 = (uint16_t)Mul16U(stride, r7h);
    r7 = (uint16_t)(r7 << 1);
    uint32_t s = (uint32_t)(uint16_t)p0 + 6;
    s = (uint32_t)(uint16_t)s + r7 + (s >> 16);
    uint16_t src = (uint16_t)((uint16_t)s + r2 + (s >> 16));
    uint16_t c = x, row = y, v = map_addr(c, row);
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x4310, 0x01);
        io_w8(0x4311, 0x18);
        do {
            if ((uint16_t)(r4 + c) < 0x0021) map_dma_now((uint16_t)(r4 << 1), src, v);
            else {
                uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
                map_dma_now(r6, src, v);
                io_w8(0x420B, 0x02);
                io_w16(0x4315, (uint16_t)((uint16_t)(r4 << 1) - r6));
                uint32_t t = (uint32_t)src + r6;
                io_w16(0x4312, (uint16_t)t);
                io_w16(0x2116, (uint16_t)(v + 0x0400 + (t >> 16)) & 0xFFE0);
            }
            io_w8(0x420B, 0x02);
            src = (uint16_t)(src + stride);
            v = (uint16_t)(v + 0x20);
            if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
        } while (--r4h);
        return;
    }
    uint32_t bank = p0 & 0xFF0000;
    do {
        uint32_t q = bank | src;
        if ((uint16_t)(r4 + c) < 0x0021) QueueVRAMDMA(&q, (uint16_t)(r4 << 1), v);
        else {
            uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
            QueueVRAMDMA(&q, r6, v);
            uint16_t n2 = (uint16_t)((uint16_t)(r4 << 1) - r6);
            unsigned nb = (uint16_t)(r4 << 1) >= r6;
            QueueVRAMDMA(&q, n2, (uint16_t)(v + 0x0400 + nb) & 0xFFE0);
        }
        src = (uint16_t)(src + stride);
        v = (uint16_t)(v + 0x20);
        if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
    } while (--r4h);
}

/* The same into VRAM through VMDATA with the screen blanked (it waits for that), adding a and
 * ORing r3 into each entry */
void MapPartToVRAMOffset(uint32_t p0, uint16_t a, uint16_t x, uint16_t y, uint16_t r3, uint16_t r7, uint16_t r7h, uint16_t r4, uint16_t r4h)
{
    g.r3[1] = a;
    while (!(wait_r16(RAM_OFF_IniDispShadow - 1) & 0x8000)) wait_tick();
    if (x >= 0x0020) { x &= 0x1F; g.MapVRAM = (uint16_t)(g.MapVRAM + 0x0400); }
    uint32_t f = p0 & 0xFFFFFF, bank = p0 & 0xFF0000;
    uint16_t stride = (uint16_t)(m16(f) << 1);
    uint16_t r2 = (uint16_t)Mul16U(stride, r7h);
    r7 = (uint16_t)(r7 << 1);
    uint32_t s = (uint32_t)(uint16_t)p0 + 6;
    s = (uint32_t)(uint16_t)s + r7 + (s >> 16);
    uint16_t src = (uint16_t)((uint16_t)s + r2 + (s >> 16));
    uint16_t c = x, row = y, v = map_addr(c, row);
    do {
        io_w16(0x2116, v);
        uint16_t k = 0, n;
        if ((uint16_t)(r4 + c) < 0x0021) n = r4;
        else {
            uint16_t r6 = (uint16_t)(0x20 - c);
            n = r6;
            do { io_w16(0x2118, (uint16_t)(m16((bank | src) + k) + a) | r3); k += 2; } while (--n);
            io_w16(0x2116, (uint16_t)(v + 0x0400) & 0xFFE0);
            n = (uint16_t)(r4 - r6);
        }
        do { io_w16(0x2118, (uint16_t)(m16((bank | src) + k) + a) | r3); k += 2; } while (--n);
        src = (uint16_t)(src + stride);
        v = (uint16_t)(v + 0x20);
        if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
    } while (--r4h);
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

/* A routine reached by a long jump through a hook (IRQHook, NMIHook, FrameHook): the snes module's by
 * address, any other through call_long_other (an address in banks $00-$3F, as Reset_Fast's InitHooks
 * leaves them, is its mirror in $80-$BF) */
void (*call_long_other)(uint32_t addr);
/* Call the hook routine at addr (bank << 16 | address), as the original's jml through the hook */
void call_long(uint32_t addr)
{
    if ((addr >> 16 & 0xFF) < 0x40) addr |= 0x800000;
    switch (addr) {
    case SYM_NullHook: NullHook(); return;
    case SYM_IRQHook_Off: IRQHook_Off(); return;
    case SYM_BrightnessHook: BrightnessHook(); return;
    case SYM_IRQ_StatsScroll1: IRQ_StatsScroll1(); return;
    case SYM_IRQ_StatsScroll2: IRQ_StatsScroll2(); return;
    case SYM_IRQ_StatsScroll3: IRQ_StatsScroll3(); return;
    case SYM_IRQ_BandStart: IRQ_BandStart(); return;
    case SYM_IRQ_BandEnd: IRQ_BandEnd(); return;
    case SYM_IRQ_BG1Scroll: IRQ_BG1Scroll(); return;
    case SYM_IRQ_BG1Scroll_t1: IRQ_BG1Scroll_t1(); return;
    }
    if (call_long_other) call_long_other(addr);
}

/* The IRQ: acknowledge TIMEUP and run the hook at IRQHook */
void IRQ_Fast(void)
{
    io_r8(0x4211);
    call_long((uint32_t)g.IRQHook[2] << 16 | r16(RAM_OFF_IRQHook));
}

/* ---- the heap: handles, blocks (a header of words: size, flags, next, previous) ---- */

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

/* Convert linear 4bpp tiles at p0 (r0 bytes: r0 / 4 + 1 rows of 8 pixels, one row more than the
 * bytes) to SNES planar into p1 (a 16-bit address in p0's bank): each row's two words, a nibble
 * per pixel (a zero word skips its 4 pixels), into the row's plane words at +0 / +$10 of its tile */
void LinearToPlanar(uint32_t p0, uint16_t p1, uint16_t r0)
{
    uint32_t bank = p0 & 0xFF0000;
    uint16_t src = (uint16_t)p0, row = 0, out = 0xFFFE;
    int16_t n = (int16_t)(r0 >> 2);
    do {
        uint8_t t[4] = { 0, 0, 0, 0 };   /* (r3, r3+1, r3+2, r3+3) */
        uint16_t x = 0x0080, y = (uint16_t)(row << 2);
        for (;;) {
            uint16_t w = m16(bank + src + y);
            if (!w) {
                x >>= 4;
                if (!x) break;
                y = (uint16_t)(y + 2);
                continue;
            }
            uint16_t v = (uint16_t)(w << 8 | w >> 8);
            for (;;) {
                if (v & 0x8000) t[1] |= (uint8_t)x;
                if (v & 0x4000) t[0] |= (uint8_t)x;
                if (v & 0x2000) t[3] |= (uint8_t)x;
                if (v & 0x1000) t[2] |= (uint8_t)x;
                v = (uint16_t)(v << 4);
                x >>= 1;
                if (x >= 0x10 || (x && x < 8)) continue;
                break;
            }
            if (!x) break;
            y = (uint16_t)((row << 2) + 2);
        }
        out = (uint16_t)(out + 2);
        if (out & 0x10) out = (uint16_t)(out + 0x10);
        mset16(bank + p1 + out, (uint16_t)(t[2] | t[3] << 8));
        mset16(bank + p1 + (uint16_t)(out + 0x10), (uint16_t)(t[0] | t[1] << 8));
        row++;
    } while (--n >= 0);
}

/* Copy byte map [p0] (width, height, then the bytes from +6) into the mode 7 map at column x, row
 * y (128 bytes a row), a row at a time: at once with the screen blanked (VMAIN 0, the low bytes),
 * else queued (type $FB) */
void Mode7MapToVRAM(uint32_t p0, uint16_t x, uint16_t y)
{
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t w = m16(f), h = m16(f + 2), src = (uint16_t)((uint16_t)p0 + 6);
    uint16_t s = (uint16_t)(y << 6);
    uint16_t v = (uint16_t)(((uint16_t)(s << 1) + g.MapVRAM + (s >> 15)) | x);
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x4310, 0);
        io_w8(0x2115, 0);
        io_w8(0x4311, 0x18);
        do {
            io_w16(0x4315, w);
            io_w16(0x4312, src);
            io_w16(0x2116, v);
            io_w8(0x420B, 0x02);
            src = (uint16_t)(src + w);
            v = (uint16_t)(v + 0x80);
        } while (--h);
        io_w8(0x2115, 0x80);
        return;
    }
    uint16_t t = g.DMAQueueTail;
    do {
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 1, src);
        src = (uint16_t)(src + w);
        w16(e + 4, w);
        w16(e + 6, v);
        v = (uint16_t)(v + 0x80);
        w8(e + 3, (uint8_t)(p0 >> 16));
        w8(e, 0xFB);
        t = (t + 8) & 0x03FF;
    } while (--h);
    g.DMAQueueTail = t;
}

/* (unused entry) paste r6+2 bytes of [p0] into the map at [p1] (r7 entries a row) at column x,
 * row y as words + a; a 0 byte ends the string, the rest of the count filled with $20 + a */
void Mode7MapToVRAM_t1(uint32_t p0, uint32_t p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7, uint16_t r6h)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    p1 = (p1 & 0xFF0000) | (uint16_t)((uint16_t)p1 + (uint16_t)s + (s >> 16));
    uint16_t n = r6h, k = 0;
    uint32_t b0 = p0 & 0xFF0000;
    uint16_t lo0 = (uint16_t)p0;
    do {
        uint16_t c = m8(b0 | lo0);
        if (!c) {
            for (;;) {
                if ((int16_t)--n < 0) return;
                mset16(p1 + k, (uint16_t)(0x20 + a));
                k += 2; lo0++;
            }
        }
        mset16(p1 + k, (uint16_t)(c + a));
        k += 2; lo0++;
    } while (--n);
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

/* ---- the Huffman unpacker and Decompress ---- */

/* (the WRAM port's address registers) */
static void wmadd_hm(uint16_t v) { io_w16(0x2182, v); }
static void wmadd_lm(uint16_t v) { io_w16(0x2181, v); }
static void wmadd_h(uint8_t v) { io_w8(0x2183, v); }

/* The bit stream: a 16-bit window A (its high byte the next 8 bits, then the bits left of the current
 * byte, counted by Y in the header and by X = 2 * bits in the data) refilled a byte at a time from
 * (p0) in the data bank when its low byte runs out */
static struct { uint8_t db; uint16_t p0, a; uint8_t x, y; } hf;
static uint8_t hf_r8(uint16_t a) { return db_r8(hf.db, a); }
static uint16_t hf_r16(uint16_t a) { return (uint16_t)(hf_r8(a) | hf_r8((uint16_t)(a + 1)) << 8); }
static void hf_w16(uint16_t a, uint16_t v) { db_w8(hf.db, a, (uint8_t)v); db_w8(hf.db, (uint16_t)(a + 1), (uint8_t)(v >> 8)); }
static void hf_refill(void) { hf.a = (uint16_t)((hf.a & 0xFF00) | hf_r8(hf.p0)); hf.p0++; }
static unsigned hf_shl(void) { unsigned c = hf.a >> 15; hf.a = (uint16_t)(hf.a << 1); return c; }
static void hf_rol_number(unsigned c) { w16(RAM_OFF_HuffNumber, (uint16_t)(r16(RAM_OFF_HuffNumber) << 1 | c)); }
/* (a bit counted by Y / by X, the refill when it runs out) */
static unsigned hf_bit_y(void) { unsigned c = hf_shl(); if (!--hf.y) { hf_refill(); hf.y = 8; } return c; }
static unsigned hf_bit_x(void) { unsigned c = hf_shl(); hf.x = (uint8_t)(hf.x - 2); if (!hf.x) { hf_refill(); hf.x = 0x10; } return c; }

/* (.149 / .150: a number of the stream: 1 and 2 bits (0-3), or k zeros, a 1 and k + 3 bits over the
 * base of .151; HuffBits the window after it) */
static uint16_t hf_number(int by_x)
{
    w16(RAM_OFF_HuffNumber, 0);
    hf.a = r16(RAM_OFF_HuffBits);
    unsigned (*bit)(void) = by_x ? hf_bit_x : hf_bit_y;
    if (bit()) {
        hf_rol_number(bit());
        hf_rol_number(bit());
        w16(RAM_OFF_HuffBits, hf.a);
        return r16(RAM_OFF_HuffNumber);
    }
    uint8_t n = 2;
    do n++; while (!bit());
    uint8_t k = n;
    do hf_rol_number(bit()); while (--k);
    w16(RAM_OFF_HuffBits, hf.a);
    uint16_t v = (uint16_t)(rom16(SYM_DecompressHuffman_151 - 4 + (uint8_t)(n << 1)) + r16(RAM_OFF_HuffNumber));
    w16(RAM_OFF_HuffNumber, v);
    return v;
}

/* (.152: skip y bits of the window) */
static void hf_skip(uint8_t y) { do hf_bit_x(); while (--y); }

/* Huffman data at (p0) + 5 in data bank db to WRAM at p1 through the port. The header: the escape
 * symbol, then per code length the number of codes (HuffCount / HuffOffset / HuffLimit), the
 * symbols in order of their codes as skip counts over the ones not taken (into DecompBuf through
 * the port), from which the 256-entry lookup of the codes up to 8 bits (DecompTab1 the symbol,
 * DecompTab2 its length * 2; $10 longer, $12 the escape) is built. The data: symbols; the escape
 * followed by a count repeats the last symbol, by 0 and a 0 bit a literal byte, by 0 and a 1 bit
 * ends it. Longer codes are told by HuffLimit (the chain HuffLongVec picks by the longest length). */
void DecompressHuffman(uint8_t db, uint32_t p0, uint32_t p1)
{
    hf.db = db;
    hf.p0 = (uint16_t)((uint16_t)p0 + 5);
    w16(RAM_OFF_HuffEscape, hf_r16(hf.p0));
    hf.p0++;
    uint16_t w = hf_r16(hf.p0);
    hf.p0 = (uint16_t)(hf.p0 + 2);
    w16(RAM_OFF_HuffBits, (uint16_t)(w << 8 | w >> 8));
    hf.y = 8;
    w16(RAM_OFF_HuffSymCount, 0);
    w16(RAM_OFF_HuffCode, 0);
    uint16_t r3 = 0x10;
    uint8_t x = 0xFE;
    for (;;) {
        x = (uint8_t)(x + 2);
        r3--;
        w16(RAM_OFF_HuffCode, (uint16_t)(r16(RAM_OFF_HuffCode) << 1));
        hf_w16((uint16_t)(0x0720 + x), (uint16_t)(r16(RAM_OFF_HuffCode) - r16(RAM_OFF_HuffSymCount)));
        uint16_t n = hf_number(0);
        hf_w16((uint16_t)(0x0700 + x), n);
        w16(RAM_OFF_HuffSymCount, (uint16_t)(n + r16(RAM_OFF_HuffSymCount)));
        w16(RAM_OFF_HuffCode, (uint16_t)(r16(RAM_OFF_HuffNumber) + r16(RAM_OFF_HuffCode)));
        if (!r16(RAM_OFF_HuffNumber)) { hf_w16((uint16_t)(0x0740 + x), 0); continue; }
        uint16_t r0 = r16(RAM_OFF_HuffCode);
        unsigned c = 0;
        for (uint16_t k = r3; k; k--) { c = r0 >> 15; r0 = (uint16_t)(r0 << 1); }
        hf_w16((uint16_t)(0x0740 + x), r0);
        if (c) break;
    }
    w16(RAM_OFF_HuffMaxLen, x >> 1);
    for (int k = 0x3E; k >= 0; k -= 2)
        for (int q = 0; q < 0x100; q += 0x40) hf_w16((uint16_t)(0x0500 + q + k), 0);
    wmadd_h(0x7E);
    wmadd_lm(0x0100);
    x = 0xFF;
    do {
        uint8_t a = (uint8_t)(hf_number(0) + 1);
        do {
            do { hal_cost_mark(SYM_DecompressHuffman_loop4); x++; } while (hf_r8((uint16_t)(0x0500 + x)) & 0x80);
        } while (--a);
        db_w8(hf.db, (uint16_t)(0x0500 + x), (uint8_t)(hf_r8((uint16_t)(0x0500 + x)) - 1));
        wmdata_w(x);
        w16(RAM_OFF_HuffSymCount, (uint16_t)(r16(RAM_OFF_HuffSymCount) - 1));
    } while (r16(RAM_OFF_HuffSymCount));
    w16(RAM_OFF_HuffStartBits, (uint16_t)(hf.y << 1));
    x = 0;
    uint8_t y = 0, r0b = 0;
    w8(RAM_OFF_HuffLenIdx, 0);
    for (;;) {
        r0b = x;
        uint8_t li = r8(RAM_OFF_HuffLenIdx);
        if ((uint8_t)(li << 1) == 0x10) break;
        w8(RAM_OFF_HuffSymCount, hf_r8((uint16_t)(0x0700 + (uint8_t)(li << 1))));
        w8(RAM_OFF_HuffSpan, rom8(SYM_DecompressHuffman_4 + li));
        x = r0b;
        for (;;) {
            w8(RAM_OFF_HuffSymCount, (uint8_t)(r8(RAM_OFF_HuffSymCount) - 1));
            if (r8(RAM_OFF_HuffSymCount) & 0x80) break;
            uint8_t len = (uint8_t)(r8(RAM_OFF_HuffLenIdx) << 1), sym = db_r8(hf.db, (uint16_t)(0x0100 + y));
            y++;
            if (sym == r8(RAM_OFF_HuffEscape)) { w8(RAM_OFF_HuffEscapeLen, (uint8_t)(r8(RAM_OFF_HuffLenIdx) + 1)); len = 0x12; }
            for (uint8_t s = r8(RAM_OFF_HuffSpan); --s; ) {
                db_w8(hf.db, (uint16_t)(0x0500 + x), sym);
                db_w8(hf.db, (uint16_t)(0x0600 + x), len);
                x++;
            }
        }
        w8(RAM_OFF_HuffLenIdx, (uint8_t)(r8(RAM_OFF_HuffLenIdx) + 1));
        if (r8(RAM_OFF_HuffMaxLen) < r8(RAM_OFF_HuffLenIdx)) { r0b = x; break; }
    }
    if (r0b) { x = r0b; do { db_w8(hf.db, (uint16_t)(0x0600 + x), 0x10); x++; } while (x); }
    hf_w16(0x0760, rom16(SYM_DecompressHuffman_10 - 0x10 + (uint8_t)(r8(RAM_OFF_HuffMaxLen) << 1)));
    wmadd_h((uint8_t)(p1 >> 16));
    wmadd_lm((uint16_t)p1);
    hf.a = r16(RAM_OFF_HuffBits);
    hf.x = r8(RAM_OFF_HuffStartBits);
    /* (each state's dispatch on the code length, jmp (.21,x) ... jmp (.105,x): X = $10 ... 2) */
    static const uint16_t dispatch[8] = { 0xBD2A, 0xBD77, 0xBDC5, 0xBE14, 0xBE64, 0xBEB5, 0xBF07, 0xBF5A };
    for (;;) {
        /* the state: HuffBits the window, X = 2 * the bits left in its low byte */
        hal_cost_mark((SYM_DecompressHuffman & 0xFF0000) | dispatch[(0x10 - hf.x) >> 1 & 7]);
        w16(RAM_OFF_HuffBits, hf.a);
        uint8_t look = r8(RAM_OFF_HuffBits + 1), k = hf_r8((uint16_t)(0x0600 + look));
        if (k < 0x10) {
            for (int b = (k >> 1) + 1; b; b--) hf_bit_x();
            uint8_t sym = hf_r8((uint16_t)(0x0500 + look));
            wmdata_w(sym);
            w8(RAM_OFF_r2, sym);
            continue;
        }
        uint8_t extra = 0;
        int escape = k != 0x10;
        if (k == 0x10) {
            w16(RAM_OFF_HuffBitsExt, 0);
            uint8_t nb = hf.x >> 1;
            w16(RAM_OFF_HuffBitsExt + 1, (uint16_t)((uint16_t)((hf_r16(hf.p0) & 0xFF) << (8 - nb)) | r16(RAM_OFF_HuffBitsExt + 1)));
            uint16_t win = r16(RAM_OFF_HuffBits), vec = hf_r16(0x0760);
            static const uint16_t chain[8] = { SYM_DecompressHuffman_111 & 0xFFFF, SYM_DecompressHuffman_112 & 0xFFFF, SYM_DecompressHuffman_114 & 0xFFFF, SYM_DecompressHuffman_115 & 0xFFFF,
                                               SYM_DecompressHuffman_116 & 0xFFFF, SYM_DecompressHuffman_117 & 0xFFFF, SYM_DecompressHuffman_118 & 0xFFFF, SYM_DecompressHuffman_119 & 0xFFFF };
            int maxlen = 0;
            for (int i = 0; i < 8; i++) if (chain[i] == vec) maxlen = 9 + i;
            if (!maxlen) return;   /* (no chain there: the original jumps into the void) */
            int len = 9;
            while (len < maxlen && win >= hf_r16((uint16_t)(0x0740 + 2 * (len - 1)))) len++;
            uint8_t idx = (uint8_t)((win >> (16 - len)) - hf_r16((uint16_t)(0x0720 + 2 * (len - 1))));
            uint8_t sym = hf_r8((uint16_t)(0x0100 + idx));
            extra = (uint8_t)(len - 8);
            if (sym != r8(RAM_OFF_HuffEscape)) {
                wmdata_w(sym);
                w8(RAM_OFF_r2, sym);
                hf.p0++;
                hf.a = r16(RAM_OFF_HuffBitsExt + 1);
                for (int b = extra; b; b--) hf_bit_x();
                continue;
            }
            hf.p0++;
            hf.a = r16(RAM_OFF_HuffBitsExt + 1);
        }
        hf_skip(escape && k != 0x10 ? r8(RAM_OFF_HuffEscapeLen) : extra);
        w16(RAM_OFF_HuffBits, hf.a);
        uint16_t n = hf_number(1);
        if (n) {
            uint8_t last = r8(RAM_OFF_r2);
            do wmdata_w(last); while (--n);
            hf.a = r16(RAM_OFF_HuffBits);
            continue;
        }
        if (r16(RAM_OFF_HuffBits) & 0x8000) return;
        hf.a = r16(RAM_OFF_HuffBits);
        hf_skip(1);
        w16(RAM_OFF_HuffBits, hf.a);
        uint8_t lit = r8(RAM_OFF_HuffBits + 1);
        wmdata_w(lit);
        w8(RAM_OFF_r2, lit);
        hf_skip(8);
    }
}

/* Unpack the data at [p0] to WRAM at [p1] (through the WRAM port, once the DMA queue is empty) by
 * its format word: $FB10 LZ, $FB46 pairs, $FB30 Huffman, $FB7A run-length; the unpacked size from
 * the header (its big-endian word at +3), or 1 for an unknown format */
uint16_t Decompress(uint32_t p0, uint32_t p1)
{
    WaitDMAQueue();
    wmadd_hm((uint16_t)(p1 >> 8));
    wmadd_lm((uint16_t)p1);
    uint8_t db = (uint8_t)(p0 >> 16);
    uint16_t y = (uint16_t)p0;
    uint16_t sz = (uint16_t)(db_r8(db, (uint16_t)(y + 3)) << 8 | db_r8(db, (uint16_t)(y + 4)));
    uint16_t fmt = (uint16_t)(db_r8(db, y) | db_r8(db, (uint16_t)(y + 1)) << 8);
    y = (uint16_t)(y + 2);
    switch (fmt) {
    case 0xFB10: DecompressLZ(p0, p1); break;
    case 0xFB46: DecompressPairs(db, y); break;
    case 0xFB30: DecompressHuffman(db, p0, p1); break;
    case 0xFB7A: DecompressRLE(db, y); break;
    default: return 1;
    }
    return sz;
}

/* SPCSendSample of the sample at [p0]: its loop point and length (big-endian words), then the BRR
 * blocks from +4 (the sound locked meanwhile) */
void SPCUploadBlock(uint32_t p0)
{
    g.SoundLock[0]--;
    uint16_t lp = m16(p0), len = m16(p0 + 2);
    SPCSendSample((p0 & 0xFF0000) | (uint16_t)((uint16_t)p0 + 4), (uint16_t)(len << 8 | len >> 8), (uint16_t)(lp << 8 | lp >> 8));
    g.SoundLock[0]++;
}

/* ---- the NMI ---- */

/* The IRQ and NMI vectors */
void IRQ(void) { IRQ_Fast(); }
/* The NMI vector: NMI_Fast, then NMI_FrameHookReturn unless NMISkip was set when it came */
void NMI(void)
{
    uint16_t skip = g.NMISkip;
    NMI_Fast();
    if (!skip) NMI_FrameHookReturn(nmi_b);
}

/* The NMI: nothing while NMISkip is set (the 16-bit RDNMI read takes TIMEUP too), else the DMA budget
 * for the frame, the NMI hook (NMIHookNext taken when set), then NMI_HookReturn. The NMI_Tick at its
 * end runs the frame hook; the frame hook's own return, NMI_FrameHookReturn (the pads, SoundUpdate,
 * the sound stream), is the caller's (NMI). */
void NMI_Fast(void)
{
    io_r16(0x4210);
    if (g.NMISkip) return;
    w16(RAM_OFF_DMABudget, 0x16D3);
    if (g.NMIHookNext[1]) {
        w16(RAM_OFF_NMIHook, r16(RAM_OFF_NMIHookNext));
        w16(RAM_OFF_NMIHookNext, 0);
        g.NMIHook[2] = g.NMIHookNext[2];
    }
    call_long((uint32_t)g.NMIHook[2] << 16 | r16(RAM_OFF_NMIHook));
    NMI_HookReturn();
}

/* (a DMA of channel 0 for a queue entry: its source and size from the entry, to VRAM at +6, if the
 * budget left covers cost + its size; 0 when not) */
static int nmi_dma(uint16_t x, uint16_t dmap, uint16_t cost, int vmain)
{
    uint32_t e = RAM_OFF_DMAQueue + x;
    io_w8(0x4302, r8(e + 1));
    io_w16(0x4300, dmap);
    uint16_t b = r16(RAM_OFF_DMABudget);
    if (b < cost) return 0;
    b = (uint16_t)(b - cost);
    if (b < r16(e + 4)) return 0;
    w16(RAM_OFF_DMABudget, (uint16_t)(b - r16(e + 4)));
    io_w16(0x4303, r16(e + 2));
    io_w16(0x4305, r16(e + 4));
    io_w16(0x2116, r16(e + 6));
    if (vmain) io_w8(0x2115, 0);
    io_w8(0x420B, 0x01);
    if (vmain) io_w8(0x2115, 0x80);
    return 1;
}

/* The NMI after its hook: the IRQ timers, INIDISP, the six BG scroll registers, the colour DMA left
 * by DMAToCGRAM, then the DMA queue while the frame's budget lasts (entry types: a DMAP value; $FE /
 * $FF one or two words written through VMDATA; $FD / $FC low / high byte fills; $FB low bytes; the
 * rest high bytes), then NMI_SendOAM (NMI_Tick when the budget ran out) */
void NMI_HookReturn(void)
{
    io_w16(0x4209, g.IRQVTime);
    io_w16(0x4207, g.IRQHTime);
    io_w8(0x2100, g.IniDispShadow[0]);
    for (int x = 0, y = 0; x != 0x0C; x += 2, y++) {
        io_w8((uint16_t)(0x210D + y), r8(RAM_OFF_BG1HScroll + x));
        io_w8((uint16_t)(0x210D + y), r8(RAM_OFF_BG1HScroll + 1 + x));
    }
    if (g.CGDMALen) {
        io_w16(0x4300, 0x2200);
        io_w16(0x4302, g.CGDMASrc);
        io_w8(0x4304, g.CGDMABank[0]);
        io_w16(0x4305, g.CGDMALen);
        io_w8(0x2121, g.CGDMAColour[0]);
        io_w8(0x420B, 0x01);
        uint32_t d = (uint32_t)r16(RAM_OFF_DMABudget) - g.CGDMALen;
        w16(RAM_OFF_DMABudget, (uint16_t)(d - 0x48 - (d >> 31)));
        g.CGDMALen = 0;
    }
    uint16_t x = g.DMAQueueHead;
    while (x != g.DMAQueueTail) {
        uint32_t e = RAM_OFF_DMAQueue + x;
        uint8_t t = r8(e);
        int ok;
        if (!(t & 0x80)) {
            io_w8(0x4300, t);
            io_w8(0x4301, 0x18);
            io_w8(0x4302, r8(e + 1));
            uint16_t b = r16(RAM_OFF_DMABudget);
            ok = b >= 0x0066 && (uint16_t)(b - 0x0066) >= r16(e + 4);
            if (ok) {
                w16(RAM_OFF_DMABudget, (uint16_t)(b - 0x0066 - r16(e + 4)));
                io_w16(0x4303, r16(e + 2));
                io_w16(0x4305, r16(e + 4));
                io_w16(0x2116, r16(e + 6));
                io_w8(0x420B, 0x01);
            }
        } else if (t == 0xFE || t == 0xFF) {
            uint16_t b = r16(RAM_OFF_DMABudget);
            ok = b >= 0x005C;
            if (ok) {
                w16(RAM_OFF_DMABudget, (uint16_t)(b - 0x005C));
                io_w16(0x2116, r16(e + 2));
                io_w16(0x2118, r16(e + 4));
                if ((uint16_t)(t + 1) != 0) io_w16(0x2118, r16(e + 6));
            }
        } else if (t == 0xFD) ok = nmi_dma(x, 0x1808, 0x0078, 1);
        else if (t == 0xFC) ok = nmi_dma(x, 0x1908, 0x0079, 0);
        else if (t == 0xFB) ok = nmi_dma(x, 0x1800, 0x007D, 1);
        else ok = nmi_dma(x, 0x1900, 0x007D, 0);
        if (!ok) { g.DMAQueueHead = x; NMI_Tick(); return; }
        x = (x + 8) & 0x03FF;
    }
    NMI_SendOAM(x);
}

/* The NMI: the queue head at x; the sprite table to OAM when it is ready and the budget has $26E
 * left; then NMI_Tick */
void NMI_SendOAM(uint16_t x)
{
    g.DMAQueueHead = x;
    if (r16(RAM_OFF_DMABudget) >= 0x026E && g.OAMReady[0]) {
        g.OAMReady[0] = 0;
        io_w8(0x4304, 0x7E);
        io_w16(0x4300, 0x0400);
        io_w16(0x4302, g.OAMShowBuf);
        io_w16(0x4305, 0x0220);
        io_w16(0x2102, 0);
        io_w8(0x420B, 0x01);
    }
    NMI_Tick();
}

/* The NMI's frame work: vcount (with the 50 Hz fraction's carry), a step of the fade (in after the
 * fadestep delay, out with $FFFF; not on the frames STAT77 bit 7 is set), FrameCount (unless held),
 * then the frame hook (FrameHookNext taken when set) */
void NMI_Tick(void)
{
    uint32_t f = (uint32_t)g.vcountfrac + g.vcountstep;
    g.vcountfrac = (uint16_t)f;
    uint32_t v = (uint32_t)g.vcount[0] + 1 + (f >> 16);
    g.vcount[0] = (uint16_t)v;
    if (v >> 16) g.vcount[1]++;
    uint16_t fs = g.fadestep;
    if (fs) {
        if (!(fs & 0x8000)) {
            if (fs != 1) g.fadestep--;
            else if (g.brightness >= 0x000F) { g.brightness = 0x000F; g.fadestep = 0; }
            else if (!(io_r16(0x213E) & 0x8000)) g.brightness++;
        } else if (fs != 0xFFFF) g.fadestep++;
        else if (!(io_r16(0x213E) & 0x8000)) {
            g.brightness--;
            if (!g.brightness || (g.brightness & 0x8000)) { g.brightness = 0; g.fadestep = 0; }
        }
    }
    if (!g.FrameCountHold) g.FrameCount++;
    nmi_b = (uint8_t)(g.FrameCountHold >> 8);
    if (g.FrameHookNext[1]) {
        nmi_b = g.FrameHookNext[1];
        w16(RAM_OFF_FrameHook, r16(RAM_OFF_FrameHookNext));
        w16(RAM_OFF_FrameHookNext, 0);
        g.FrameHook[2] = g.FrameHookNext[2];
    }
    call_long((uint32_t)g.FrameHook[2] << 16 | r16(RAM_OFF_FrameHook));
}

/* ---- the sequencer ---- */

/* The first and last row of the player stats list of team a2: its goalies (statcolumn 0), else
 * its skaters (after the goalies); curitem the first */
void SetPlayerStatsRange(uint16_t a2)
{
    if (!g.statcolumn) {
        g.listtop[0] = 0;
        g.listend[0] = GetGoalieCount(a2);
    } else {
        g.listtop[0] = GetGoalieCount(a2);
        g.listend[0] = GetPlayerCount(a2);
    }
    g.listend[0]--;
    g.curitem[0] = g.listtop[0];
}

/* (the instrument entry at InstPtr, in the sound entries' bank) */
static uint8_t ip8(uint8_t y) { return lm8((((uint32_t)g.SoundBank[0] << 16) + g.InstPtr + y) & 0xFFFFFF); }
static uint16_t ip16(uint8_t y) { return lm16((((uint32_t)g.SoundBank[0] << 16) + g.InstPtr + y) & 0xFFFFFF); }
static uint16_t sound_entry(uint8_t y) { return (uint16_t)(m16((g.SoundTable + y) & 0xFFFFFF) + (uint16_t)g.SoundTable); }

/* Track t: count its wait down, or play its events until one gives a wait (notes start voices,
 * $D9 and up are commands: $D9/$DA stop the song, $E2/$DB loop (from the pointer the track's
 * events started at), $DD tempo, $DE volume, $DF controllers, $E5 pitch bend (the pitches it sets
 * take the tuning of the entry at InstPtr, whichever it is), $DC program change); returns CurTrack */
uint8_t StepTrack(uint8_t t)
{
    g.CurTrack = t;
    uint8_t x = (uint8_t)(t << 1);
    w8(RAM_OFF_CurTrackOfs, x);
top:
    if (r16(RAM_OFF_TrackWait + x)) goto wait;
    g.CurTrackPtr = r16(RAM_OFF_TrackPtr + x);
event:
    ReadTrackEvent();
    g.CurTrackPtr = g.EventPtr[0];
    x = g.CurTrackOfs;
    uint8_t c = r8(RAM_OFF_EventCode), ct = g.CurTrack;
    if (c < 0xD9) {
        if (ct == 0x09) {
            g.InstPtr = sound_entry((uint8_t)((uint8_t)(c - 0x2F) << 1));
            w16(RAM_OFF_TrackInst + 0x12, g.InstPtr);
            g.TrackVolL[9] = ip8(0x12);
            g.TrackVolR[9] = ip8(0x13);
            g.TrackBendRange[9] = ip8(0x2F);
            g.TrackBend[9] = 0;
        }
        StartSoundVoice((uint8_t)(g.CurTrack << 1));
    } else if (c == 0xD9 || c == 0xDA) {
        MusicStop();
        return g.CurTrack;
    } else if (c == 0xE2) {
        w16(RAM_OFF_TrackLoop + x, r16(RAM_OFF_TrackPtr + x));
    } else if (c == 0xDB) {
        w16(RAM_OFF_TrackPtr + x, r16(RAM_OFF_TrackLoop + x));
        goto top;
    } else if (c == 0xDD) {
        g.TempoTick = r8(RAM_OFF_EventData1);
    } else if (c == 0xDE || (c == 0xDF && r8(RAM_OFF_EventData1) == 0x07)) {
        if (c == 0xDF) w16(RAM_OFF_EventData1, g.EventData2);
        g.InstPtr = r16(RAM_OFF_TrackInst + x);
        if (!g.SoundMono[0]) { g.InstVolL = ip8(0x12); g.InstVolR = ip8(0x13); }
        else g.InstVolL = g.InstVolR = ip8(0x17);
        uint8_t e = r8(RAM_OFF_EventData1);
        g.TrackVolL[ct] = (uint8_t)MulFrac8((uint16_t)(g.InstVolL << 8 | e));
        g.TrackVolR[ct] = (uint8_t)MulFrac8((uint16_t)(g.InstVolR << 8 | e));
        for (int y = 7; y >= 0; y--) {
            if (g.VoiceTrack[y] != ct) continue;
            uint8_t vel = g.VoiceVelocity[y];
            g.VoiceVolL = (uint8_t)MulFrac8((uint16_t)(g.TrackVolL[ct] << 8 | vel));
            g.VoiceVolR = (uint8_t)MulFrac8((uint16_t)(g.TrackVolR[ct] << 8 | vel));
            spc_wait();
            io_w8(0x2143, g.VoiceVolR);
            io_w8(0x2142, g.VoiceVolL);
            io_w8(0x2141, (uint8_t)y);
            spc_send(0x0E);
            io_w8(0x2140, 0);
        }
    } else if (c == 0xDF) {
        uint8_t k = r8(RAM_OFF_EventData1), v = (uint8_t)g.EventData2;
        switch (k) {
        case 0x40:
            g.TrackSustain[ct] = v;
            if (v) break;
            for (int y = 7; y >= 0; y--)
                if (g.VoiceTrack[y] == ct && g.VoiceState[y]) ReleaseVoice((uint8_t)y, (uint8_t)(y << 1));
            break;
        case 0x0B: g.TrackVolL[ct] = v; break;
        case 0x0C: g.TrackVolR[ct] = v; break;
        case 0x32: g.EchoVoices = v; SeqCmd08(); break;
        case 0x33: g.EchoDelay = v; SeqCmd08(); break;
        case 0x34: g.EchoVolume = v; SeqCmd08(); break;
        case 0x35: g.EchoFeedback = v; SeqCmd08(); break;
        case 0x37: w8(RAM_OFF_SoundVolume + 1, v); SeqCmd08Init_send(); break;
        case 0x39: g.NoiseVoices = v; SeqCmd08Init_send(); break;
        }
    } else if (c == 0xE5) {
        uint16_t d = g.EventData2;
        d = (uint16_t)((d & 0xFF00) | (uint8_t)(d << 1));
        uint8_t bend = (uint8_t)((uint16_t)((uint16_t)(d - 0x4000) << 1) >> 8);
        g.TrackBend[ct] = bend;
        g.BendAmount[0] = bend;
        uint8_t range = g.TrackBendRange[ct];
        if (range) {
            w8(RAM_OFF_EventCode, range);
            for (int y = 7; y >= 0; y--) {
                if (g.VoiceTrack[y] != ct) continue;
                uint8_t vx = (uint8_t)(y << 1);
                uint16_t base = r16(RAM_OFF_VoiceBasePitch + vx), p;
                if (!bend) p = base;
                else if (!(bend & 0x80)) {
                    uint16_t e = (uint16_t)(rom16(SYM_PitchTable + (uint8_t)((uint8_t)(g.VoiceNote[y] + range) << 1)) - base);
                    w16(RAM_OFF_EventData2, e);
                    p = (uint16_t)(MulFrac16(e, bend) + base);
                } else {
                    uint16_t lo = rom16(SYM_PitchTable + (uint8_t)((uint8_t)(g.VoiceNote[y] - range) << 1));
                    w16(RAM_OFF_EventData1, lo);
                    uint16_t e = (uint16_t)(base - lo);
                    w16(RAM_OFF_EventData2, e);
                    uint8_t nb = (uint8_t)(0 - bend);
                    if (nb & 0x80) w16(RAM_OFF_MulFracResult, e);
                    else MulFrac16(e, nb);
                    p = (uint16_t)(base - r16(RAM_OFF_MulFracResult));   /* (the zero multiply leaves $43 as it was) */
                }
                w16(RAM_OFF_EventData1, p);
                w16(RAM_OFF_VoicePitch + vx, (uint16_t)(ip16(0x02) + p));
            }
        }
    } else if (c == 0xDC) {
        uint16_t n = r8(RAM_OFF_EventData1);
        if (n == 0x6A) n = 0x14;
        g.InstPtr = sound_entry((uint8_t)(n << 1));
        w16(RAM_OFF_TrackInst + x, g.InstPtr);
        g.TrackBendRange[ct] = ip8(0x2F);
        g.TrackBend[ct] = 0;
        uint8_t r;
        if (!g.SoundMono[0]) { g.TrackVolL[ct] = ip8(0x12); r = ip8(0x13); }
        else g.TrackVolL[ct] = r = ip8(0x17);
        g.TrackVolR[ct] = r;
    } else if (c == 0xEA) {
        g.EventEAUnused = r8(RAM_OFF_EventData1);
    }
    uint16_t delta = ReadVarLen();
    x = g.CurTrackOfs;
    if (!delta) goto event;
    w16(RAM_OFF_TrackWait + x, delta);
    w16(RAM_OFF_TrackPtr + x, g.CurTrackPtr);
wait:
    w16(RAM_OFF_TrackWait + x, (uint16_t)(r16(RAM_OFF_TrackWait + x) - 1));
    return g.CurTrack;
}

/* The tempo: TempoAcc + $40, and while it reaches TempoTick (less it each time) a tick: the
 * music voices updated and, unless the music is paused, every track stepped (from the last; none
 * when the song stops, and no more ticks without tracks) */
void SequencerTick(void)
{
    uint8_t a = (uint8_t)(g.TempoAcc + 0x40);
    for (;;) {
        g.TempoAcc = a;
        if (a < g.TempoTick) return;
        UpdateVoices();
        if (!g.MusicPause[0]) {
            uint8_t x = g.TrackCount;
            if (!x) return;
            x--;
            do {
                x = StepTrack(x);
                if (!(uint8_t)g.MusicOn) return;
                x--;
            } while (!(x & 0x80));
        }
        a = (uint8_t)(g.TempoAcc - g.TempoTick);
    }
}

/* Each frame, unless the sound is locked: the fade (SoundVolume less SoundFadeSpeed, the sound
 * reset below 0, else the volume sent), the sound effect voices and the song's ticks (the pause's
 * mute of the volume is undone at once) */
void SoundUpdate(void)
{
    if (g.SoundLock[0]) return;
    if (g.SoundFadeSpeed) {
        uint16_t v = g.SoundVolume;
        g.SoundVolume = (uint16_t)(v - g.SoundFadeSpeed);
        if (v < g.SoundFadeSpeed) SoundInit();
        else SeqCmd08Init_send();
    }
    if (g.MusicPause[0]) {
        uint8_t h = r8(RAM_OFF_SoundVolume + 1);
        w8(RAM_OFF_SoundVolume + 1, 0);
        w8(RAM_OFF_SoundVolume + 1, h);
    }
    UpdateSfxVoices();
    if ((uint8_t)g.MusicOn) SequencerTick();
}

/* The NMI after the frame hook: the pads (Multi5Bios; b the accumulator's high byte the hook
 * leaves), SoundUpdate, the sound stream, NMISkip cleared */
void NMI_FrameHookReturn(uint8_t b)
{
    Multi5Bios(b);
    SoundUpdate();
    UpdateStreamSfx();
    g.NMISkip = 0;
}

/* The scroll arrows beside a summary list (right while rows are below the five shown, left while
 * rows are above), or blanks in their place; then the text map queued */
void PrintScrollArrows(void)
{
    print((uint16_t)(g.listtop[0] + 5 - g.listend[0]) & 0x8000 ? SYM_RightArrowTxt2 : SYM_PrintScrollArrowsPos);
    print((uint16_t)(g.listtop[0] - 1) & 0x8000 ? SYM_PrintScrollArrowsPos2 : SYM_LeftArrowTxt2);
    QueueTextDMA();
}

/* ---- the reset ---- */

/* The reset: the PPU's registers cleared (the mode 7 matrix the identity, colour math $30 / $E0),
 * VRAM cleared, the sound driver uploaded, all of WRAM cleared, the RAM tags, sprite masks and
 * hooks set, the sprites and the sound started, the screen black, then the game (Opening) */
void Reset_Fast(void)
{
    io_w8(0x420D, 0x01);
    io_w8(0x4200, 0);
    io_w8(0x2100, 0x8F);
    io_w8(0x2101, 0);
    io_w8(0x2102, 0);
    io_w8(0x2103, 0);
    io_w8(0x2104, 0);
    io_w8(0x2104, 0);
    uint16_t x = 0x2105;
    for (; x != 0x210D; x++) io_w8(x, 0);
    for (; x != 0x2115; x++) { io_w8(x, 0); io_w8(x, 0); }
    io_w8(0x2115, 0x80);
    x++;
    for (; x != 0x211B; x++) io_w8(x, 0);
    for (uint16_t y = 0; x != 0x2121; x++, y++) { io_w8(x, 0); io_w8(x, rom8(SYM_Mode7Identity + y)); }
    for (; x != 0x2130; x++) io_w8(x, 0);
    io_w8(0x2130, 0x30);
    io_w8(0x2131, 0);
    io_w8(0x2132, 0xE0);
    io_w8(0x2133, 0);
    w16(RAM_OFF_IniDispShadow, 0x008F);
    FillVRAM(0, 0, 0, g.r3[0]);
    io_w8(0x4200, 0);
    io_w8(0x4201, 0xFF);
    for (x = 0x4207; x != 0x420C; x++) io_w8(x, 0);   /* ($4202-$4206 too: the multiplier, not hardware here) */
    SPCUpload(SYM_SoundDriver & 0xFFFF);
    memset(&g, 0, sizeof g);
    for (int k = 5; k >= 0; k--) {
        uint8_t c = rom8(SYM_InitTag + k);
        g.RAMTag[k] = c;
        g.RAMTag2[k] = c;
        g.ReplayBufferEnd[k] = c;
    }
    for (int k = 0x10; k; k--) g.OAMHiMasks[k - 1] = rom8(SYM_OAMHighMasks + k - 1);
    ResetStub();
    g.IniDispShadow[0] = 0x8F;
    for (int k = 0x0C; k >= 0; k--) w8(RAM_OFF_FrameHook + k, rom8(SYM_InitHooks + k));
    for (int k = 0x24; k >= 0; k--) w8(RAM_OFF_FrameCount + k, 0);
    InitSprites(0, 0, 0);
    io_w8(0x4200, 0x81);
    SoundInit();
    g.brightness = 0;
    forceblack();
    Opening();
}

/* The reset vector (native mode, the stack at $1FFF, the direct page 0: the machine's) */
void Reset(void) { Reset_Fast(); }
