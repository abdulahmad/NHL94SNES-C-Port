/* period.c: periods (src/routines/period). */
#pragma once
#include <stdint.h>

uint16_t GetTeamRating(uint16_t a2);
void ClearGameStatBraces(void);
void SendGameStatsPage(void);
void DisplayStats(void);
uint16_t ManualGoalieMenu(uint16_t d1);
void StatsReport(void);
void DrawPeriodStats(void);
void PeriodStatsScreen(void);
void Reset_Fast_ret(void);
void updatePPTeamTime(void);
