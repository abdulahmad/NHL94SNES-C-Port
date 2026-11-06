/* The hardware the game logic touches directly (platform/hal.c puts it on the runtime).
 *
 * The console's registers ($2100-$21FF, $4000-$43FF), a byte at a time in the 65816's order (a
 * 16-bit store is its low byte, then its high byte). The multiplier and divider ($4202-$4206,
 * $4214-$4217) are not here: the C does the arithmetic. */
#pragma once
#include <stdint.h>

void io_w8(uint16_t addr, uint8_t v);
uint8_t io_r8(uint16_t addr);
static inline void io_w16(uint16_t addr, uint16_t v) { io_w8(addr, (uint8_t)v); io_w8((uint16_t)(addr + 1), (uint8_t)(v >> 8)); }
static inline uint16_t io_r16(uint16_t addr) { uint8_t lo = io_r8(addr); return (uint16_t)(lo | io_r8((uint16_t)(addr + 1)) << 8); }

/* Wait for the next interrupt (a loop polling RAM the NMI or IRQ handler changes: PadRaw1, Vcount):
 * the console runs to it and the handler runs. */
void hal_wait_interrupt(void);

/* The original resets the stack here (to $1FFF: the callers' frames are abandoned and the routine
 * never returns to them), then goes on with resume (the rest of the routine): the C frames are
 * dropped and resume is called from the top level. */
void hal_stack_reset(void (*resume)(void));

/* A place the cost model times (platform/clock.c): the original passes the instruction at addr here,
 * as often as the C calls this (inside a loop whose passes vary in number, so that the loop takes the
 * cartridge's time). */
void hal_cost_mark(uint32_t addr);
