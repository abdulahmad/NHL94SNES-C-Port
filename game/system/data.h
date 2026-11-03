/* data.c: team and roster data (src/routines/data). */
#pragma once
#include <stdint.h>

void ClrPassBits(void);
void GetShifter(uint16_t *d1, uint16_t *d2);
void SuperAdd(uint16_t d0);
void SuperMult(uint16_t d0);
uint16_t SuperDiv(uint16_t d0);
void DefaultMenus(void);
void InitializeGameStructures(void);
void box(void);
void PushBits(uint16_t d0, uint16_t d1);
void ReadPassBits(void);
void WritePassBits(void);
void FigureJoy(void);
void MakeTree(void);
void NewPO(void);
void ResolveGames(void);
uint32_t GetPlayerName(void);
uint32_t GetPlayerNumInitial(void);
void EncodePW(void);
void ShowInjuryBox(void);
void ShowGoalBox(void);
