/* replay.c: replays (src/routines/replay). */
#pragma once
#include <stdint.h>

Pad getpzjoy(void);
void updatereplay(void);
void ReplayMode(void);
uint32_t suba4(uint32_t a4);
uint32_t adda4(uint32_t a4);
uint32_t adda43(uint32_t a4);
void RestoreReplayFrame(uint32_t a4);
