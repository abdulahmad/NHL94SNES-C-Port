/* sound.c: sound and music (src/routines/sound). */
#pragma once
#include <stdint.h>

void SPCSendSample(uint32_t p0, uint16_t n, uint16_t y);
void FadeOutSound(void);
void StreamSfx(uint16_t a);
void SPCTitleToMenu(void);
uint16_t StartSoundVoice(uint8_t x);
void UpdateSfxVoices(void);
uint16_t PlaySoundEntry(void);
void PlaySound(uint16_t a);
uint16_t QueueSound(uint16_t a);   /* the voice it took (A) */
uint16_t sfx(uint16_t a, uint16_t vx, uint16_t vy);
uint16_t PuckSfx(uint16_t a);
void CollisionSfx(int x, int y);
void UpdateStreamSfx(void);
void SPCGameUpload(void);
void SPCIntroUpload(void);
void SPCTitleUpload(void);
void SPCMenuUpload(void);
