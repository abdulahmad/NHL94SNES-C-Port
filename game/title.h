/* title.c (src/routines/title). */
#pragma once
#include <stdint.h>

void PrintStringSprites(uint16_t x, uint16_t y, uint32_t a1);
void TitleScreen(void);
void TitleScreen_5(void);
void TitleScreen_6(void);
void TitleScreen_7(void);
void TitleScreen_8(void);
void TitleScreen_9(uint16_t a);
void TitleScreen_framehook(void);
void TitleScreen_irq1(void);
void TitleScreen_irq2(void);
void TitleScreen_irq3(void);
void DrawTitleSprite(uint16_t a);
void TitleZoomIn(void);
void TitleZoomOutRight(void);
void TitleZoomOutLeft(void);
void TitleDelay(uint16_t x);
void TitleBlankSprites(void);
uint16_t TitlePathScale(uint16_t a);
uint16_t TitlePathX(uint16_t a);
uint16_t TitlePathY(uint16_t a);
void PrintCopyright(void);
void DrawLicenceNHL(void);
void DrawLicenceNHLPA(void);
void StartCredits(void);
void RollCredits(void);
void LoadTitleLogo(uint16_t a);
void BuildTitlePath(void);
void TitleScreen_ret(void);
void TitleWindowWipe(void);
void TitleWait(uint16_t x);
void ShuffleTitleTeams(void);
uint16_t NextTitleTeam(void);
