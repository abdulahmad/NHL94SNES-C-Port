/* cards.c: player cards (src/routines/cards). */
#pragma once
#include <stdint.h>

void PrintSmallLetterSprites(uint16_t x, uint16_t y, uint32_t a1);
void PlayerCards(void);
void DrawPlayerCard(uint16_t a2);
void PrintLineSlotName(uint16_t x, uint16_t y, uint32_t a3, uint16_t a2);
void SetCardTeamColours(uint16_t a2);
void FindSkaterStrengths(uint16_t a2);
void FindGoalieStrengths(uint16_t a2);
void AddStrength(uint16_t d1, uint16_t a2);
void LoadPlayerCardScreen(void);
void DrawPlayerPicture(uint16_t d0, uint16_t a2, uint16_t x, uint16_t y);
void DrawLogoSprite(uint16_t a, uint16_t x, uint16_t y);
uint16_t CalcAttrib(uint16_t d0, uint16_t a2, uint16_t mask, uint16_t r2, uint16_t *count);
uint32_t PrintCentered(uint32_t a1);
