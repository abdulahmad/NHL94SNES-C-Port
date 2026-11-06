/* collide.c: collisions, checks and energy (src/routines/collide). */
#pragma once
#include <stdint.h>

uint16_t getpde(int o);
void setpde(int team, uint16_t num2, uint16_t energy);
void Setplass(int o);
uint16_t GetPeriodTimeRemaining(void);
uint16_t makepde(uint16_t a, int o);
uint16_t checkagr(int o);
uint16_t TryAddPlayerToList(uint16_t d0, uint16_t d4, uint16_t a2, uint32_t a4, uint16_t *d1);
void ResetBench(void);
void deflect(uint16_t d0);
void setplayer(int o, uint16_t d3);
void forcepldata(uint16_t a2);
void setInjuryType(uint16_t a2);
void wallcoll(int o, uint16_t cos, uint16_t sin, int a3);
void wallcollb_out(void);
void wallcollb(int o, uint16_t cos, uint16_t sin, int a3);
void checkgoalp(int o, uint16_t a2, uint16_t d2, uint16_t d3, int a3);
void checkgoal(int o, uint16_t a2, uint16_t d2, uint16_t d3, int a3);
void checkwallcoll(int o, uint16_t d2, uint16_t d3, int a3);
void puckglue(int o, uint16_t a2);
void SetPlList(uint16_t a2);
void setpersonel(uint16_t a2);
uint16_t FallDown(int x, int y, int a3, uint16_t d4);
uint16_t checkint(int x, int a2, int a3, uint16_t d4);
uint16_t Bcheck(int x, int a2, int a3, uint16_t d4);
void holdcheck(int x, int a2, int a3);
void checkcheck(int x, int a2, int a3, uint16_t d4);
void checkplcoll(int x, uint16_t d2, uint16_t d3, int a3);
void checkcoll(int x, uint16_t d2, uint16_t d3, int a3);
void checkpuckcoll(int x, int a3, uint16_t a0);
int puckbody(int x, int a2, int a3, uint16_t d0);
void puckstick(int x, int a2, int a3, uint16_t d0, uint16_t a0);
void puckgoalie(int x, int a2, int a3, uint16_t d0, uint16_t d1);
void SetupPenaltyShot(void);
void Goal(int o);
void checkpuckcoll_ccx(int x, int y, int a3, uint16_t a0);
