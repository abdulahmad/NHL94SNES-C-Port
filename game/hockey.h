/* hockey.c: the game loop and play (src/routines/hockey). */
#pragma once
#include <stdint.h>

void CheckLateGameSong(void);
void UpdateCwdExcite(void);
void startpause(void);
uint16_t PauseResumeRtl(void);
uint16_t PauseSubmenuDone(void);
uint16_t GetPeriodTime(void);
void restoreteams(void);
void updatecrowdf(void);
uint16_t seta2(void);
void ChoosePauseMenu(void);
void RestoreBenchEnergy(void);
void startpause_pad(uint16_t a);
void startpause1(void);
void ResetClock(void);
void PmenuAppend(uint32_t a4, uint16_t y);
void PmenuNone(void);
void PmenuGoalieValue(uint16_t d0);
void PmenuDefValue(uint16_t d0);
uint32_t PrintMP5Line1(uint32_t a1);
uint32_t PrintMP5Line2(uint32_t a1);
uint32_t PrintMP5Line3(uint32_t a1);
uint32_t PrintMP5Line4(uint32_t a1);
void DrawExitPrompt(void);
void SetupPauseScreen(void);
void clockcont_0(void);
void clockcont(void);
void CheckInjury(void);
void periodicevents(void);
void DoGameFrame(void);
void PulsePauseSide(void);
void ShowMP5Warning(void);
void PulseUntilRelease(void);
uint16_t AskExitGame(void);
void demoread(void);
void SelectPauseController(uint16_t *d1, uint16_t *d3);
uint16_t PauseResume(uint16_t d1);
uint16_t PauseDefStore(void);
uint16_t PauseDefControl(uint16_t d1);
uint16_t PauseExitGame(uint16_t d1);
uint16_t PauseReplay(uint16_t d1);
uint16_t PauseMenuPoll(void);
uint16_t Pausemode_menu(void);
void Pausemode(void);
void Gameloop(void);
uint16_t PauseGameStats(uint16_t d1);
uint16_t PausePenaltySummary(uint16_t d1);
uint16_t PauseScoringSummary(uint16_t d1);
uint16_t PauseOtherScores(uint16_t d1);
uint16_t PauseTeamRoster(uint16_t d1);
uint16_t PausePeriodStats(uint16_t d1);
uint16_t PausePlayerStats(uint16_t d1);
uint16_t PausePlayerCards(uint16_t d1);
uint16_t PauseCrowdLevel(uint16_t d1);
uint16_t PauseEditLines(uint16_t d1);
uint16_t PauseShootoutSetup(uint16_t d1);

void StartPer(void);
void StartGame_go(void);
void StartGame_scout(void);
void StartGame(void);
/* (the direct page the intro's state machine keeps: d4+2 the next state, d5 the state after a zoom or
 * fade, d6 the timeout, d6+2 the end of a wait, p0 the map or palette) */
typedef struct { uint16_t next, d5, d6, d6h; uint32_t p0; } IntroDP;
void clear_braces(uint16_t x, uint16_t top, uint16_t bottom);
void frame_steps(uint16_t d7);
void put16(uint32_t a, uint16_t v);
void put8(uint32_t a, uint8_t v);
void start_hdma(uint8_t left, uint8_t right, uint16_t wtab, uint16_t ctab);
void summary_page(uint32_t none, uint16_t empty, uint16_t y);
