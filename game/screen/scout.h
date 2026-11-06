/* scout.c: the scouting report (src/routines/scout). */
#pragma once
#include <stdint.h>

uint16_t GetMatchupPlayer(uint16_t k, uint16_t a2);
void DrawMatchupPictures(uint16_t a2);
void ScoutingReport(void);
void ScoutingHDMA(void);
void ScoutNamePos(uint16_t a2);
void ClearPictureBox(uint16_t x, uint16_t y);
void ScoutTypeWord(uint32_t a1, uint16_t d0, uint16_t d1);
void ScoutPlainWord(uint32_t a1, uint32_t a2, uint16_t d0);
uint16_t GetTeamCity_home_3(uint32_t a1, uint16_t d0);
void ScoutTypeText(uint32_t a1, uint16_t d0);
void ScoutAppend(uint32_t a1, uint16_t d0);
void ScoutExtremely(uint32_t a1, uint32_t a2, uint16_t d0);
void ScoutHotTeam(uint16_t d0);
void ScoutPlayerName(uint16_t d0, uint16_t a2, uint16_t x);
void ScoutHomePlayer(uint16_t a, uint16_t d0);
void ScoutVisPlayer(uint16_t a, uint16_t d0);
void GetTeamCity(uint16_t a1, uint16_t d0);
void GetTeamNickname(uint16_t a1, uint16_t d0);
void GetTeamArena_home(uint16_t d0);
void ScoutHomeName(uint16_t d0);
void ScoutVisName(uint16_t d0);
void GetTeamCity_home(uint16_t d0);
void GetTeamCity_vis(uint16_t d0);
void GetTeamNickname_home(uint16_t d0);
void GetTeamNickname_vis(uint16_t d0);
void ScoutHomeHot1(uint16_t d0);
void ScoutHomeHot2(uint16_t d0);
void ScoutHomeHot3(uint16_t d0);
void ScoutHomeCold1(uint16_t d0);
void ScoutHomeCold2(uint16_t d0);
void ScoutHomeCold3(uint16_t d0);
void ScoutVisHot1(uint16_t d0);
void ScoutVisHot2(uint16_t d0);
void ScoutVisHot3(uint16_t d0);
void ScoutVisCold1(uint16_t d0);
void ScoutVisCold2(uint16_t d0);
void ScoutVisCold3(uint16_t d0);
void ShowMatchups(void);
void LoadScoutingScreen(void);
