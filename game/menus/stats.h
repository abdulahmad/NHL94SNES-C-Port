/* stats.c: statistics screens (src/routines/stats). */
#pragma once
#include <stdint.h>

uint16_t GetDefenseCount(uint16_t a2);
uint16_t GetForwardCount(uint16_t a2);
uint16_t GetPlayerCount(uint16_t a2);
uint32_t AttribRating(uint16_t d0);
uint32_t AttribFighting(uint16_t d0);
uint32_t AttribWeight(uint16_t d0);
uint32_t AttribEnergy(uint16_t d0, uint16_t a2);
uint32_t AttribHanded(uint16_t d0);
uint32_t AttribStatus(uint16_t d0, uint16_t a2);
void GetNameandAttrib(uint16_t d0, uint16_t d4, uint16_t mask, uint16_t a2, uint16_t r2);
uint16_t GetPlayerBaseRating(uint16_t d1, uint16_t a2);
uint16_t GetPlayerRating(uint16_t d1, uint16_t a2);
void SetLineSlot(uint16_t a, uint16_t a2);
void RosterRtl(void);
void ShowRosterLine(uint16_t a2);
void SetupScreen(void);
uint16_t SelectLinePlayer(uint16_t a2, uint16_t r7);
void TeamRosterScreen(void);
