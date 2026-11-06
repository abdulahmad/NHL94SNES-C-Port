/* optsetup.c: the options screen (src/routines/optsetup). */
#pragma once
#include <stdint.h>
void setoptions_7(uint16_t d2, uint16_t *a2);
uint16_t setoptions_9(uint16_t d3, uint16_t d2, uint16_t d4, uint16_t d5);
void setoptions_10(uint16_t *a2);
void setoptions_t1xxxxxxxxxxx(uint16_t x, uint16_t *a2);
void setoptions_t2xxx(uint16_t *a2);
void setoptions_11(uint16_t *a2);
void setoptions_12(uint16_t d0);
void setoptions(void);
void LoadTeamLines(uint16_t a2);
void PrintSelectLeft(uint16_t d4);
void ClearMessageArea(void);
void ClearBigText(void);
void InitSetupChars(void);
void SendBigText(void);
void Multi5MouseMsg(void);
void Multi5ScopeMsg(void);
void Multi5SocketMsg(void);
void LoadMatchupPhotos(uint16_t x);
void DrawSelectTeam(uint16_t d1);
void PrintTeamSprites_go(uint32_t s, uint16_t r3, uint16_t r3h, uint16_t a2);
void PrintTeamSprites(uint16_t x, uint16_t y, uint16_t d1, uint32_t a1, uint16_t a2);
void RedrawPlayerSelect(uint16_t a2);
void PlayerSelect(uint16_t a2, uint16_t d2);
