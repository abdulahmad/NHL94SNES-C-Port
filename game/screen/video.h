/* video.c: the score and clock displays (src/routines/video). */
#pragma once
#include <stdint.h>

void showref(void);
void updatesound(void);
void showclock(void);
void addframe(void);
void setffo(void);
void showzam(int c);
void checkfo(void);
void setsortcords(void);
void showcrowd(void);
void setvideo(void);
void setvideo_t1(uint16_t y, uint16_t r0);
void setvideo_t3(void);
