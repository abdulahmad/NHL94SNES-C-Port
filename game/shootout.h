/* shootout.c: shootouts (src/routines/shootout). */
#pragma once
#include <stdint.h>

void PlayShootout_ret(void);
void InitShooters(uint16_t a2);
void SetShooter(uint16_t a2);
void ShootersSetGoalie(uint16_t a, uint16_t y);
void ShootersExit(uint16_t a2);
void ShootoutWonBy(int o, uint16_t d0);
void DrawShootersBack(void);
void ShootersBackground(uint16_t a2);
void ShootersSetup(uint16_t a2);
void PrintShooterNames(uint16_t a2);
void PrintShooterRows(uint16_t d4, uint16_t mask, uint16_t a2);
void PrintShooterList(uint16_t a2);
void ShooterSelectList(uint16_t a2);
void PickShooterRow(uint16_t a2);
void ShootoutShooters(void);

void PlayShootout_t1(uint16_t a2);
void PlayShootout(void);
