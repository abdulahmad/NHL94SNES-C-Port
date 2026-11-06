/* crowd.c: the crowd (src/routines/crowd). */
#pragma once
#include <stdint.h>

void InitCrowdExcite(void);
uint16_t Crowd_Noise_meter(void);
void Crowd_Noise(void);
void LowerHotColdTable(void);
void StartBenchCheer(uint16_t a);
void UpdateBenchCheer(void);
void Create_HotCold_Table(void);
