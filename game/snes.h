/* snes.c: the port's glue to the console (no asm module). */
#pragma once
#include <stdint.h>

extern uint8_t cf;   /* the carry flag, where the original's callers use what a routine leaves in it */
extern uint8_t nmi_b;   /* the accumulator's high byte (B) the frame hook leaves for NMI_FrameHookReturn */

/* the console. A hook is bank << 16 | address. */
typedef struct { uint16_t dir, press, change, held; } Pad;   /* ReadjoyA: d0-d3 */
void mset8(uint32_t a, uint8_t v);
void mset16(uint32_t a, uint16_t v);
/* the waits for an interrupt to change WRAM (snes.c) */
uint16_t wait_r16(uint32_t off);
void wait_tick(void);
extern void (*call_long_other)(uint32_t addr);   /* a hook outside this module (NULL: none) */
void call_long(uint32_t addr);
