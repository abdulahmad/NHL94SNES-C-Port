/* title.c (src/routines/title). */
#pragma once
#include <stdint.h>
void TitleScreen_ret(void);
void TitleDelay(uint16_t x);
void StartCredits(void);
uint16_t TitlePathY(uint16_t a);
uint16_t TitlePathScale(uint16_t a);
void ShuffleTitleTeams(void);
uint16_t NextTitleTeam(void);
void TitleZoomIn(void);
void TitleWindowWipe(void);
void TitleZoomOutRight(void);
void TitleZoomOutLeft(void);
void TitleWait(uint16_t x);
void DrawTitleSprite(uint16_t a);
uint16_t TitlePathX(uint16_t a);
void PrintStringSprites(uint16_t x, uint16_t y, uint32_t a1);
void LoadTitleLogo(uint16_t a);
void TitleBlankSprites(void);
void BuildTitlePath(void);
void DrawLicenceNHL(void);
void DrawLicenceNHLPA(void);
void PrintCopyright(void);
void RollCredits(void);
void TitleScreen_5(void);
void TitleScreen_6(void);
void TitleScreen_7(void);
void TitleScreen_8(void);
void TitleScreen_9(uint16_t a);
void TitleScreen(void);
void TitleScreen_framehook(void);
void TitleScreen_irq1(void);
void TitleScreen_irq2(void);
void TitleScreen_irq3(void);
