/* snes: the port's glue to the console, which the asm has no module for: the carry flag, the
 * waits for an interrupt, stores to a 24-bit address, and call_long, the jump through a hook.
 * Every original routine lives in its module's file (src/routines/<module> is <module>.c). */
#include "game.h"

uint8_t cf;
uint8_t nmi_b;

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
