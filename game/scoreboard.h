/* scoreboard.c: the scoreboard, rink views, intermission, ticker and highlights (src/routines/scoreboard). */
#pragma once
#include <stdint.h>

void chkatop(void);
void updatepwrplay(void);
void updatepwrplay_clr(void);
uint32_t AdvanceStringPtr(uint32_t a1, uint16_t n);
void ChkShotStat(void);
void UpdateScores(void);
void printscores1(void);
uint32_t printscores1_per(void);
uint32_t printscores1_ps(uint16_t a2, uint16_t d3);
void nullrtl89(void);
void SetScore(uint16_t a0);
void USBoard(void);
void pplpen(uint16_t a2, uint16_t d0);
void Intermission(void);
void Intermission_ticker(void);
uint16_t Intermission_tick(void);
uint16_t IntermissionTick_done(void);
void NewTicker2_next(void);
void NewTicker(void);
void NewTicker2(void);
void NewTicker3(uint16_t d3);
void DoHiLights(void);
void ClrHor(void);
void SetHor(void);
void printscores1_ps_name(uint16_t d3);
void EASNLogo(void);
uint16_t loadTeamStruct(int o);
void InitScores(void);
void SetHor_rink(void);
void ChkShotStat_add(int o, uint16_t stat, uint16_t amount);
uint16_t AvgCline(uint16_t a2);
void reenergizeteam(uint16_t a2);
uint16_t getlinee(uint16_t a2, uint16_t d0);
uint16_t linebar(uint16_t a2, uint16_t d0);
void SetupTeamForIntermission(void);
