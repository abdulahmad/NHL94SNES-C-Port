/* snes.c: the engine: arithmetic, memory, video and the console (src/routines/snes). */
#pragma once
#include <stdint.h>

extern uint8_t cf;   /* the carry flag, where the original's callers use what a routine leaves in it */
extern uint8_t nmi_b;   /* the accumulator's high byte (B) the frame hook leaves for NMI_FrameHookReturn */
uint16_t Mul8U(uint16_t a, uint16_t x);
uint16_t Mul16x8HiU(uint16_t x, uint16_t a);
uint16_t Div16U(uint16_t n, uint16_t d);
uint16_t Div16x8U(uint16_t n, uint16_t d);
uint16_t DivX8U(uint16_t x, uint16_t a);
uint16_t randoma8(uint16_t limit);
uint16_t randoma8_go(uint16_t limit);

/* the console. A hook is bank << 16 | address. */
typedef struct { uint16_t dir, press, change, held; } Pad;   /* ReadjoyA: d0-d3 */
void mset8(uint32_t a, uint8_t v);
void mset16(uint32_t a, uint16_t v);
/* the waits for an interrupt to change WRAM (snes.c) */
uint16_t wait_r16(uint32_t off);
void wait_tick(void);
void NullHook(void);
void ResetStub(void);
void IRQHook_Off(void);
void SetSoundFade(uint16_t a);
uint16_t ReadAnyPad(void);
uint16_t JoyDirIndex(uint16_t a);
uint16_t JoyDirIndex_Hi(uint16_t a);
void SPCCmd01(void);
void SPCCmd04(void);
void SPCCmd0D(void);
void SPCCmd05(uint8_t x);
void SPCCmd06(uint8_t x, uint16_t y);
void SPCCmd0B(uint8_t x, uint16_t y);
void SPCCmd0C(uint8_t x, uint8_t y);
void SeqCmd0C(uint8_t y);
void SeqCmd0C0B(uint8_t y);
Pad ReadPausePad(void);
void WaitFrames_Inc(uint16_t x, uint16_t y);
void FadeIn(void);
void FadeOut(void);
void ClearFrameHook(void);
uint16_t Cos(uint16_t a);
void SPCCmd08(uint32_t p0);
void SPCCmd09(uint32_t p0);
void ClearTextMap(void);
void ClearTextMap2(void);
void ClearTextMapBottom(void);
void HeapReset(void);
void SendTextMap(void);
void SendTextMap2(void);
void SendTextMapRows(void);
void SendTextMap2Rows(void);
void GetFrameTiles_drawat(uint32_t p0, uint16_t a, uint16_t x, uint16_t y);
uint16_t DrawSpritePieces_cacheat(uint32_t p0, uint16_t x);
uint16_t DrawSpritePieces_cacheframe(uint16_t a, uint32_t p1, uint16_t x);
void FreeCacheEntryA(uint16_t a, uint16_t y);
void SendColours(void);
extern void (*call_long_other)(uint32_t addr);   /* a hook outside this module (NULL: none) */
void call_long(uint32_t addr);
void SPCUploadBlock(uint32_t p0);
void IRQ(void);
void Reset(void);
void spc_ack(uint8_t cmd);
void spc_send(uint8_t cmd);
void spc_wait(void);
