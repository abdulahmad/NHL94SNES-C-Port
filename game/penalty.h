/* penalty.c: penalties (src/routines/penalty). */
#pragma once
#include <stdint.h>

uint16_t RemovePlayerFromList(uint16_t a0);
void PenaltyManager(void);
void checkfornewpen(void);
void Stop4Pen(uint16_t a0);
void chkprogress(void);
void InProgress_exit(void);
void InProgress_coin(void);
void InProgress_ap(uint16_t a2);
void AddPenalty(int o, uint16_t d0);
void AddPenalty2(int o, uint16_t d0);
void PenGoalStuff(uint16_t a1, uint16_t a2);
uint16_t GetLowestPen(uint16_t a2, uint16_t a3);
uint32_t PenBoxSpot(int o);
void SeatPenaltyBox(void);
void updatepentime(void);
uint16_t PenShotChk(uint16_t a2);
int releasepl(uint16_t a2, uint16_t d0);
void ClearPenaltyBuffer(void);
void prefmes(uint16_t d0);
void ClearPenaltyMessage(void);
uint32_t Fprint(uint16_t d0, uint32_t a1);
void UpdatePA(void);
void SetPA(uint16_t d0);
void SetPA2(void);
void PushRef(uint16_t d0);
void AlignBigNumber(uint32_t a1);
void DispPen(uint16_t a2);
void limitfo(void);
