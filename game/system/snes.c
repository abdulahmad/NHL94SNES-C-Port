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

/* ---- the console: hooks, pads, interrupts, the sound driver's commands, handles, memory ---- */

/* (the loops that wait for an interrupt to change WRAM: read it afresh, and between two reads let
 * the interrupt run) */
uint16_t wait_r16(uint32_t off)
{
    return (uint16_t)(((volatile uint8_t *)&g)[off] | ((volatile uint8_t *)&g)[off + 1] << 8);
}
void wait_tick(void) { hal_wait_interrupt(); }

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

/* 2 * JoyDirTable[the d-pad bits of a] (JoyDirIndex_Hi: of a's high byte) */
uint16_t JoyDirIndex(uint16_t a) { return (uint16_t)(rom8(SYM_JoyDirTable + (a & 0xF)) << 1); }
uint16_t JoyDirIndex_Hi(uint16_t a) { return JoyDirIndex((uint16_t)(a >> 8 | a << 8)); }

/* The sound driver's commands: wait for APUIO0 to be 0, send the arguments and the command, wait
 * for the driver to echo it, clear APUIO0 */
void spc_wait(void) { while (io_r8(0x2140)) {} }
/* (the SPCCmd routines' send: cmd to APUIO0, then wait for the driver to echo it back) */
void spc_send(uint8_t cmd)
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

/* Sequencer: command $0C (the GAIN DSPGain of voice y); then $0B (the pitch SendPitch of voice y) */
static void seq_cmd0c(uint8_t y) { spc_wait(); io_w8(0x2142, g.DSPGain); io_w8(0x2141, y); spc_send(0x0C); io_w8(0x2140, 0); }
void SeqCmd0C(uint8_t y) { seq_cmd0c(y); }
void SeqCmd0C0B(uint8_t y) { seq_cmd0c(y); SeqCmd0B(y); }   /* (its own copy of $0C, then into SeqCmd0B) */

/* Handles: 64 entries of an address word and a bank word at $2000-$20FF (0, 0 = free) */

/* ReadjoyA for the controller that owns the pause menu */
Pad ReadPausePad(void) { return ReadjoyA(g.pausepad >> 1); }

void WaitFrames_Inc(uint16_t x, uint16_t y) { WaitFrames(x, (uint16_t)(y + 1)); }

/* Fade the screen in after a 5-frame delay, or out: BrightnessHook as the frame hook */
void FadeIn(void) { g.fadestep = 5; SetFrameHook(SYM_BrightnessHook); }
void FadeOut(void) { g.fadestep = 0xFFFF; SetFrameHook(SYM_BrightnessHook); }

/* Set the frame hook back to the empty routine */
void ClearFrameHook(void) { SetFrameHook(SYM_NullHook); }

uint16_t Cos(uint16_t a) { return Sin((uint16_t)(a + 0x0100)); }

/* Sound driver commands $07 (voice x: volume, sample, ADSR from [p0]), $08 (main volume, pitch
 * modulation, noise, echo, FLG), $09 (echo), $0A (the echo filter: 8 bytes), each in two or three
 * sends */
void spc_ack(uint8_t cmd)
{
    io_w8(0x2140, cmd);
    while (io_r8(0x2140) != cmd) {}
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

/* Fill the text map with the blank tile (textmap2 with charofs2; from row 12 down) */
void ClearTextMap(void) { MemFill(0x7E0000 + RAM_OFF_textmap, 0x0800, (uint16_t)(g.blankchar + g.charofs)); }
void ClearTextMap2(void) { MemFill(0x7E0000 + RAM_OFF_textmap2, 0x0800, (uint16_t)(g.blankchar + g.charofs2)); }
void ClearTextMapBottom(void) { MemFill(0x7E0000 + RAM_OFF_textmap + 0x300, 0x0500, (uint16_t)(g.blankchar + g.charofs)); }

/* HeapInit at the same starts */
void HeapReset(void) { HeapInit(g.HeapStart7E, g.HeapStart7F); }

/* Send the text map (all, or rows 1-12) or the second text map to video memory */
static void send_map(uint32_t off, uint16_t x, uint16_t y) { uint32_t p0 = 0x7E0000 + off; DMAToVRAM(&p0, x, y, 0); }
void SendTextMap(void) { send_map(RAM_OFF_textmap, 0x0800, 0x0000); }
void SendTextMap2(void) { send_map(RAM_OFF_textmap2, 0x0800, 0x0400); }
void SendTextMapRows(void) { send_map(RAM_OFF_textmap + 0x40, 0x0300, 0x0020); }
void SendTextMap2Rows(void) { send_map(RAM_OFF_textmap2 + 0x40, 0x0300, 0x0420); }

/* ---- sprites: the tile cache, its VRAM allocators and the OAM ---- */

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

void FreeCacheEntryA(uint16_t a, uint16_t y) { FreeCacheEntry((uint16_t)((a >> 8 & 0xFF) << 1), y); }

/* ---- colours, screen maps, letters, the IRQ ---- */

/* Send the 256 colours of the colour buffer */
void SendColours(void) { DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf, 0x0100, 0); }

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

/* ---- the NMI ---- */

/* The IRQ and NMI vectors */
void IRQ(void) { IRQ_Fast(); }

/* ---- the reset ---- */

/* The reset vector (native mode, the stack at $1FFF, the direct page 0: the machine's) */
void Reset(void) { Reset_Fast(); }
