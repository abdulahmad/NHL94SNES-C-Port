/* main: the reset, power on and the PAL check (src/routines/main). */
#include "game.h"

/* The frame time step for a 50 Hz console (STAT78 bit 4; the 16-bit read takes APUIO0 too) */
void CheckPAL(void)
{
    g.vcountstep = 0;
    if (io_r16(0x213F) & 0x0010) g.vcountstep = 0x3333;
}

/* The reset: the PPU's registers cleared (the mode 7 matrix the identity, colour math $30 / $E0),
 * VRAM cleared, the sound driver uploaded, all of WRAM cleared, the RAM tags, sprite masks and
 * hooks set, the sprites and the sound started, the screen black, then the game (Opening) */
void Reset_Fast(void)
{
    io_w8(0x420D, 0x01);
    io_w8(0x4200, 0);
    io_w8(0x2100, 0x8F);
    io_w8(0x2101, 0);
    io_w8(0x2102, 0);
    io_w8(0x2103, 0);
    io_w8(0x2104, 0);
    io_w8(0x2104, 0);
    uint16_t x = 0x2105;
    for (; x != 0x210D; x++) io_w8(x, 0);
    for (; x != 0x2115; x++) { io_w8(x, 0); io_w8(x, 0); }
    io_w8(0x2115, 0x80);
    x++;
    for (; x != 0x211B; x++) io_w8(x, 0);
    for (uint16_t y = 0; x != 0x2121; x++, y++) { io_w8(x, 0); io_w8(x, rom8(SYM_Mode7Identity + y)); }
    for (; x != 0x2130; x++) io_w8(x, 0);
    io_w8(0x2130, 0x30);
    io_w8(0x2131, 0);
    io_w8(0x2132, 0xE0);
    io_w8(0x2133, 0);
    w16(RAM_OFF_IniDispShadow, 0x008F);
    FillVRAM(0, 0, 0, g.r3[0]);
    io_w8(0x4200, 0);
    io_w8(0x4201, 0xFF);
    for (x = 0x4207; x != 0x420C; x++) io_w8(x, 0);   /* ($4202-$4206 too: the multiplier, not hardware here) */
    SPCUpload(SYM_SoundDriver & 0xFFFF);
    memset(&g, 0, sizeof g);
    for (int k = 5; k >= 0; k--) {
        uint8_t c = rom8(SYM_InitTag + k);
        g.RAMTag[k] = c;
        g.RAMTag2[k] = c;
        g.ReplayBufferEnd[k] = c;
    }
    for (int k = 0x10; k; k--) g.OAMHiMasks[k - 1] = rom8(SYM_OAMHighMasks + k - 1);
    Reset_Fast_ret();
    g.IniDispShadow[0] = 0x8F;
    for (int k = 0x0C; k >= 0; k--) w8(RAM_OFF_FrameHook + k, rom8(SYM_InitHooks + k));
    for (int k = 0x24; k >= 0; k--) w8(RAM_OFF_FrameCount + k, 0);
    InitSprites(0, 0, 0);
    io_w8(0x4200, 0x81);
    SoundInit();
    g.brightness = 0;
    forceblack();
    Opening();
}

void Reset_Fast_ret(void) {}

/* The reset vector (native mode, the stack at $1FFF, the direct page 0: the machine's) */
void Start(void) { Reset_Fast(); }
