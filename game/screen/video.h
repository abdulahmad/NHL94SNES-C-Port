/* video.c: printing, the rink display and sort objects (src/routines/video). */
#pragma once
#include <stdint.h>

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
