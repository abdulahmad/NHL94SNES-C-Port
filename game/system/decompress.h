/* decompress.c: the unpackers (src/routines/decompress). */
#pragma once
#include <stdint.h>

void DecompressRLE(uint8_t db, uint16_t y);
void DecompressLZ(uint32_t p0, uint32_t p1);
void DecompressPairs(uint8_t db, uint16_t y);
void DecompressHuffman(uint8_t db, uint32_t p0, uint32_t p1);
uint16_t Decompress(uint32_t p0, uint32_t p1);
