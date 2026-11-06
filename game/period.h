/* period.c: periods (src/routines/period). */
#pragma once
#include <stdint.h>

void StatsReport(void);
void ClearGameStatBraces(void);
void SendGameStatsPage(void);
void DisplayStats(void);
uint16_t GetTeamRating(uint16_t a2);
void updatePPTeamTime(void);
void PeriodStatsScreen(void);
void DrawPeriodStats(void);
void Reset_Fast_ret(void);
uint16_t ManualGoalieMenu(uint16_t d1);
