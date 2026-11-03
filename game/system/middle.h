/* middle.c: frames, digits, the pads and random numbers (src/routines/middle). */
#pragma once
#include <stdint.h>

uint16_t randomd0A(uint16_t limit);
uint16_t Mod100(uint16_t d0);
uint16_t randomd0sd0(uint16_t d0);
uint32_t ConvertByteToDigits(uint16_t d0, uint32_t a1);
uint32_t PushNumberWidth(uint16_t d0, uint16_t d1, uint16_t *a);
uint32_t PushNumberWrap(uint16_t d0, uint16_t d1, uint16_t *a);
uint32_t FramerRow(uint32_t p0, uint16_t tmp0, uint16_t d0, uint16_t *tmp2, uint16_t *xo);
void FramerNineTile(uint16_t d0, uint16_t d1);
void FramerOutline(uint16_t d0, uint16_t d1);
void ReadPads(void);

uint16_t randomd0(uint16_t n);
uint16_t randomd0s(uint16_t n);
void marktexthi(uint16_t a);
void marktextlo(uint16_t a);
void updatescroll(void);
uint16_t ProcessInputWithRepeat(uint16_t d1, uint16_t d2, uint16_t d3);
void appstring(uint32_t a1, uint32_t a3);
void Readjoy1(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);
uint32_t PushNumber(uint16_t d0);
uint16_t WeightedRandomSelect(uint16_t d0);
void find3d(uint16_t *d0, uint16_t *d1, uint16_t d2);
void eraser(uint16_t d0, uint16_t d1, uint16_t d2);
uint32_t PushTime(uint16_t d0);
uint32_t remap(uint32_t a0, uint32_t a1, uint16_t d0);
uint16_t Sroot(uint32_t d0);
uint32_t print(uint32_t a1);
void printbig(uint32_t a1);
void Framer(uint16_t d0, uint16_t d1);
void defaultsprites_0(void);
void forceblack(void);
void QueueTextDMA(void);
void updatescroll_rows(void);
Pad waitxsr(uint16_t d0);
