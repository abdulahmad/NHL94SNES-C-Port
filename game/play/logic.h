/* logic.c: the game's decisions (src/routines/logic). */
#pragma once
#include <stdint.h>

void StartDumpIn(int o);
void SetSPAFromA(int o, uint16_t anim);
void pucknothing(void);
uint16_t GetGoalieCount(uint16_t a2);
void AssignPadPlayer(uint16_t d0, uint16_t d4);
void StartPenaltyShot(void);
void GoaliePuckVector(int o, uint16_t *d0, uint16_t *d1);
void CalcPuckDistances(void);
void DumpInTarget(int o);
uint16_t CheckBreakaway(uint16_t d0);
void GetFrameHot(int o, uint16_t *d0, uint16_t *d1);
void AssignPadIfFree(uint16_t d0, uint16_t d4);
void GlideBack(int o);
void PuckCarrierPad(uint16_t d0);
void PadTakePosition(uint16_t d1, uint16_t d4);
void PadTakeGoalie(uint16_t d4);
void PadTakeLeftDefense(int o, uint16_t d4);
void PadTakeRightDefense(int o, uint16_t d4);
void PassPadToTeammate(int o, uint16_t d4);
void FaceoffPads(int a3);
int assdumpin(int o, int a3);
uint16_t StartBreakaway(uint16_t d0);
void EndPenaltyShot(int o);
void assgoalie(int o, uint16_t a2, int a3);
