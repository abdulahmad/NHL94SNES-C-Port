/* setup.c: game setup (src/routines/setup). */
#pragma once
#include <stdint.h>

void PeriodOver(void);

void clearTeamStats(void);
void setteams(void);
void SprSort(void);
void SetTeamColors(void);
void setplayercolors(void);
void resetplstuff(void);
void UpdateScoutingDisplay_rtl(void);
void ScrollDisplayUp(void);
void setupice_reset(void);
void setupice(void);
void DrawTeamBlocks(uint16_t d1, uint32_t a1, uint16_t a2);
void PlayoffScreen(void);
void UpdateScoutingDisplay_para(uint32_t a1, uint16_t d0);
void UpdateScoutingDisplay_next(uint32_t a1, uint32_t a2, uint16_t d0);
void UpdateScoutingDisplay_skip(uint32_t a1, uint32_t a2, uint16_t d0);
void UpdateScoutingDisplay_scroll(uint32_t a1, uint16_t d0);
void UpdateScoutingDisplay(void);

void GameOver(void);
void Opening2(void);
void Opening(void);
