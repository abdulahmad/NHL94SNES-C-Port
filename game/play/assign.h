/* assign.c: the players' assignments (src/routines/assign). */
#pragma once
#include <stdint.h>

void assfaceoff(int o);
void assdopen(int o);
void assfaceoffpl(int o);
void assstanley(int o);
int check4bench(int o);
void donothing(int o, int a3);
void assdefo(int o, uint16_t a2, int a3);
void asswingd(int o, uint16_t a2, int a3);
void assdefd(int o, uint16_t a2, int a3);
void assbench(int o, int a3);
void asseben(int o);
void assscore(int o, uint16_t a2, int a3);
void assepen(int o, uint16_t a2);
void asspenalty(int o, uint16_t a2, int a3);
