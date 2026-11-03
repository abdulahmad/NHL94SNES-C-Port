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
