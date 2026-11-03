/* input.c: the pads and the players they control (src/routines/input). */
#pragma once
#include <stdint.h>

void setpads(int o, uint16_t d4);
void Acheck(int o, uint16_t a0);
void setpassmode(int o);
uint16_t Findhittype(int o, uint16_t *d0);
uint16_t CalcAngleOffset(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t *d3, uint16_t d4);
uint16_t getlchoice(uint16_t a2, uint16_t d0);
void holdplayer(int o, uint16_t d4);
void SetShotMode(int o);
void Sweepcheck(int o);
void burst(int o, int a3);
void faceoffinput(int o, uint16_t d0, uint16_t d1);
void shotdiradj(int o, int a3);
void dopass_swing(int o, int a3);
void dopass_count(int o, uint16_t a0, int a3, uint16_t y);
void dopass(int o, int a3);
void passmode(int o, uint16_t d0, uint16_t d2, int a3);
void doshot(int o, int a3);
void ShotMode(int o, uint16_t d0, uint16_t d2, int a3);
uint16_t setlccords(uint16_t a2, uint16_t *d1);
uint16_t SetLCmode2(uint16_t a2);
void SetLCmode(int o);
void lcfound_close(uint16_t a2);
void lcfound(uint16_t a2, int a3, uint16_t d2);
void lineinput(int o, uint16_t d1, uint16_t d0, int a3);
void doinput(int o, uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, int a3);
