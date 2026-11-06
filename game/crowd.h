/* crowd.c: the crowd (src/routines/crowd). */
#pragma once
#include <stdint.h>

void Create_HotCold_Table(void);
void StartBenchCheer(uint16_t a);
void InitCrowdExcite(void);
void LowerHotColdTable(void);
void UpdateBenchCheer(void);
uint16_t Crowd_Noise_meter(void);
void Crowd_Noise(void);
