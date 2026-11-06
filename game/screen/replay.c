/* replay: recording and playing back the replay buffer, the view (src/routines/replay). */
#include "game.h"

#define WMDATA 0x2180
#define ReplayBufferEnd_addr (0x7E0000u | RAM_OFF_ReplayBufferEnd)

/* The next replay frame record after a4 (the buffer wraps round to its start; suba4 has its own copy) */
static uint32_t next_record(uint32_t a4)
{
    uint32_t lo = (a4 & 0xFFFF) + replaysize, hi = a4 >> 16 & 0xFFFF;
    if (lo >> 16) hi = (hi + 1) & 0xFFFF;
    lo &= 0xFFFF;
    if (lo == (ReplayBufferEnd_addr & 0xFFFF) && hi == ReplayBufferEnd_addr >> 16) return ReplayBufferStart;
    return hi << 16 | lo;
}
uint32_t adda43(uint32_t a4) { return next_record(a4); }

/* a byte's sign as a high byte */
static uint8_t sign8(uint8_t b) { return b & 0x80 ? 0xFF : 0; }

/* Save this frame into the replay buffer (through the work RAM port): each object's position,
 * frame and flip bits, the roster numbers and positions, the puck's heights, the sounds, the pads'
 * objects, the crowd, the view. Not in highlights or while the replay delay counts down. */
void updatereplay(void)
{
    if (g.gmode & gmhl) return;
    if (g.replaydelay) {
        uint16_t a = (uint16_t)(g.replaydelay - 1);
        if (!a) return;
        g.replaydelay = a;
    }
    if (!(g.sflags & sf2drec)) {
        uint32_t lo = (g.ReplayBufferPtr & 0xFFFF) + replaysize;
        uint32_t hi = g.ReplayBufferPtr >> 16;
        if (lo >> 16) hi = (hi + 1) & 0xFFFF;
        lo &= 0xFFFF;
        g.ReplayBufferPtr = hi << 16 | lo;
        if (lo == (ReplayBufferEnd_addr & 0xFFFF) && hi == ReplayBufferEnd_addr >> 16) {
            g.sflags |= sfwrap;
            g.ReplayBufferPtr = ReplayBufferStart;
        }
    }
    io_w16(0x2181, (uint16_t)g.ReplayBufferPtr);
    io_w8(0x2183, (uint8_t)(g.ReplayBufferPtr >> 16));
    for (int x = 0; x < 0x20; x += 2) {
        io_w8(WMDATA, r8(RAM_OFF_Xpos + x));
        io_w8(WMDATA, (uint8_t)(r8(RAM_OFF_Ypos + 1 + x) << 4 | (r8(RAM_OFF_Xpos + 1 + x) & 0x0F)));
        io_w8(WMDATA, r8(RAM_OFF_Ypos + x));
        io_w8(WMDATA, r8(RAM_OFF_frame + x));
        io_w8(WMDATA, (uint8_t)((r8(RAM_OFF_attribute + 1 + x) & 0x18) << 3 | (r8(RAM_OFF_frame + 1 + x) & 0x3F)));
    }
    for (int x = 0; x < 24; x += 4) {
        io_w8(WMDATA, r8(RAM_OFF_rostnum + x));
        io_w8(WMDATA, r8(RAM_OFF_rostnum + 2 + x));
        uint8_t p = r8(RAM_OFF_position + x), q = r8(RAM_OFF_position + 2 + x);
        if (p & 0x80) p = 0xFF;
        if (q & 0x80) q = 0xFF;
        io_w8(WMDATA, (uint8_t)(q << 4 | (p & 0x0F)));
    }
    io_w8(WMDATA, r8(RAM_OFF_puckz));
    io_w8(WMDATA, r8(RAM_OFF_shadowz));
    io_w8(WMDATA, r8(RAM_OFF_recsfx));
    io_w8(WMDATA, r8(RAM_OFF_recsfxpitch));
    io_w8(WMDATA, r8(RAM_OFF_recsfxpitch + 1));
    io_w8(WMDATA, r8(RAM_OFF_recsfxvol + 1));
    io_w8(WMDATA, r8(RAM_OFF_recsfx2));
    io_w8(WMDATA, r8(RAM_OFF_recsfxpitch2));
    io_w8(WMDATA, r8(RAM_OFF_recsfxpitch2 + 1));
    io_w8(WMDATA, r8(RAM_OFF_recsfxvol2 + 1));
    io_w8(WMDATA, (uint8_t)g.d7);
    for (int k = 0; k < 12; k += 4)
        io_w8(WMDATA, (uint8_t)(g.PadControlObj[k + 2] << 4 | g.PadControlObj[k]));
    io_w8(WMDATA, r8(RAM_OFF_Crowdlevel));
    io_w8(WMDATA, r8(RAM_OFF_Crowdlevel + 1));
    io_w8(WMDATA, r8(RAM_OFF_PBnum));
    io_w8(WMDATA, r8(RAM_OFF_Hpos));
    io_w8(WMDATA, r8(RAM_OFF_Hpos + 1));
    io_w8(WMDATA, r8(RAM_OFF_Vpos));
    io_w8(WMDATA, r8(RAM_OFF_Vpos + 1));
    io_w8(WMDATA, r8(RAM_OFF_benchframe));
    io_w8(WMDATA, r8(RAM_OFF_benchframe + 1));
    g.recsfx = 0xFFFF;
    g.recsfx2 = 0xFFFF;
}

/* a nibble of the record as a signed byte (8-15 negative) */
static uint8_t sx4(uint8_t n) { return n & 8 ? (uint8_t)(n | 0xF0) : n; }

/* Put the replay frame record at a4 back into the objects, the puck, the sounds and the view
 * (through the work RAM port; the view stays when sflags bit 5 is set) */
void RestoreReplayFrame(uint32_t a4)
{
    g.replaynewframe++;
    io_w16(0x2181, (uint16_t)a4);
    io_w8(0x2183, (uint8_t)(a4 >> 16));
    for (int x = 0; x < 0x20; x += 2) {
        w8(RAM_OFF_Xpos + x, io_r8(WMDATA));
        uint8_t r3 = io_r8(WMDATA);
        w8(RAM_OFF_Xpos + 1 + x, sx4(r3 & 0x0F));
        w8(RAM_OFF_Ypos + x, io_r8(WMDATA));
        w8(RAM_OFF_Ypos + 1 + x, sx4(r3 >> 4));
        w8(RAM_OFF_frame + x, io_r8(WMDATA));
        r3 = io_r8(WMDATA);
        w8(RAM_OFF_frame + 1 + x, r3 & 0x3F);
        uint8_t at = r8(RAM_OFF_attribute + 1 + x) & 0xE7;
        w8(RAM_OFF_attribute + 1 + x, (uint8_t)((r3 >> 3 & 0x18) | at));
    }
    for (int x = 0; x < 24; x += 4) {
        w8(RAM_OFF_rostnum + x, io_r8(WMDATA));
        w8(RAM_OFF_rostnum + 2 + x, io_r8(WMDATA));
        w8(RAM_OFF_position + 1 + x, 0);
        uint8_t r3 = io_r8(WMDATA), a = r3 & 0x0F;
        if (a == 0x0F) { a = 0xFF; w8(RAM_OFF_position + 1 + x, 0xFF); }
        w8(RAM_OFF_position + x, a);
        w8(RAM_OFF_position + 3 + x, 0);
        a = r3 >> 4;
        if (a == 0x0F) { a = 0xFF; w8(RAM_OFF_position + 3 + x, 0xFF); }
        w8(RAM_OFF_position + 2 + x, a);
    }
    uint8_t b = io_r8(WMDATA);
    w8(RAM_OFF_puckz, b);
    w8(RAM_OFF_puckz + 1, sign8(b));
    b = io_r8(WMDATA);
    w8(RAM_OFF_shadowz, b);
    w8(RAM_OFF_shadowz + 1, sign8(b));
    w8(RAM_OFF_recsfx, io_r8(WMDATA));
    w8(RAM_OFF_recsfxpitch, io_r8(WMDATA));
    w8(RAM_OFF_recsfxpitch + 1, io_r8(WMDATA));
    w8(RAM_OFF_recsfxvol + 1, io_r8(WMDATA));
    w8(RAM_OFF_recsfx2, io_r8(WMDATA));
    w8(RAM_OFF_recsfxpitch2, io_r8(WMDATA));
    w8(RAM_OFF_recsfxpitch2 + 1, io_r8(WMDATA));
    w8(RAM_OFF_recsfxvol2 + 1, io_r8(WMDATA));
    w8(RAM_OFF_d7, io_r8(WMDATA));
    for (int k = 0; k < 12; k += 4) {
        uint8_t r3 = io_r8(WMDATA);
        g.PadControlObj[k] = r3 & 0x0F;
        g.PadControlObj[k + 2] = r3 >> 4;
    }
    w8(RAM_OFF_Crowdlevel, io_r8(WMDATA));
    w8(RAM_OFF_Crowdlevel + 1, io_r8(WMDATA));
    b = io_r8(WMDATA);
    w8(RAM_OFF_PBnum, b);
    w8(RAM_OFF_PBnum + 1, sign8(b));
    if (!(g.sflags & 0x20)) {
        w8(RAM_OFF_Hpos, io_r8(WMDATA));
        w8(RAM_OFF_Hpos + 1, io_r8(WMDATA));
        w8(RAM_OFF_Vpos, io_r8(WMDATA));
        w8(RAM_OFF_Vpos + 1, io_r8(WMDATA));
    } else {
        for (int i = 0; i < 4; i++) io_r8(WMDATA);
    }
    w8(RAM_OFF_benchframe, io_r8(WMDATA));
    w8(RAM_OFF_benchframe + 1, io_r8(WMDATA));
    g.recsfx = (uint16_t)(int8_t)(g.recsfx & 0xFF);
    g.recsfx2 = (uint16_t)(int8_t)(g.recsfx2 & 0xFF);
}

/* Move the replay one frame forward from a4 (frames elapsed d7 = 0): not onto the record being
 * written; returns the new a4 */
uint32_t adda4(uint32_t a4)
{
    w16(RAM_OFF_d7, 0);
    if ((g.sflags & sf2drec) && adda43(a4) == g.ReplayBufferPtr) return a4;
    if (a4 == g.ReplayBufferPtr) return a4;
    RestoreReplayFrame(a4);
    return adda43(a4);
}

/* Move the replay one frame back from a4: not past the buffer's start unless it has wrapped, nor
 * onto the record being written; returns the new a4 */
uint32_t suba4(uint32_t a4)
{
    if (a4 == ReplayBufferStart) {
        if (!(g.sflags & sfwrap)) { RestoreReplayFrame(a4); g.d7 = 0; return a4; }
        a4 = ReplayBufferEnd_addr;
    }
    uint32_t lo = (a4 & 0xFFFF) - replaysize, hi = (a4 >> 16) - ((lo >> 16) & 1);
    a4 = (hi & 0xFFFF) << 16 | (lo & 0xFFFF);
    if (a4 != g.ReplayBufferPtr) { RestoreReplayFrame(a4); return a4; }
    a4 = next_record(a4);
    g.d7 = 0;
    return a4;
}


#define SPFpuck 396

#define CAM (camobj >> 1)

/* (ReplayMode: the camera's step from cur toward target, at most step) */
static uint16_t ease(uint16_t target, uint16_t cur, uint16_t step)
{
    uint16_t a;
    if ((uint16_t)(target - cur) & 0x8000) {
        a = (uint16_t)(cur - step);
        return (uint16_t)(a - target) & 0x8000 ? target : a;
    }
    a = (uint16_t)(cur + step);
    return (uint16_t)(a - target) & 0x8000 ? a : target;
}

/* (ReplayMode: the view's limits, x -64 to 64 and y -199 to 256) */
static uint16_t clamp_h(uint16_t a)
{
    if ((uint16_t)(a - 0xFFC0) & 0x8000) a = 0xFFC0;
    if (!((uint16_t)(a - 0x40) & 0x8000)) a = 0x40;
    return a;
}
/* (ReplayMode: the camera's y, replayVpos, kept to -199 .. 256) */
static uint16_t clamp_v(uint16_t a)
{
    if ((uint16_t)(a - 0xFF39) & 0x8000) a = 0xFF39;
    if (!((uint16_t)(a - 0x100) & 0x8000)) a = 0x100;
    return a;
}

/* (ReplayMode: replay recorded sound s (its low byte, $FF none): a streamed one from $16 on, else
 * queued with its volume, and a voice it starts in ADSR gets the recorded pitch) */
static void replay_sound(uint16_t s, uint16_t vol, uint16_t pitch)
{
    s &= 0xFF;
    if (s == 0xFF) return;
    if (s >= 0x16) {
        StreamSfx((uint16_t)(s - 0x15));
        return;
    }
    g.SfxVelocity = vol;
    uint16_t y = QueueSound(s);
    if (r8(RAM_OFF_VoiceState + y) != 2) return;
    w8(RAM_OFF_VoiceState + y, 3);
    w16(RAM_OFF_VoicePitch + (uint16_t)(y << 1), pitch);
}

/* The instant replay: from the start of the recorded play (half way back to where the replay
 * buffer's frames restart), frame by frame with the control panel. The camera follows the player
 * the pad points at (or a free cursor the pad moves, kept off the boards), else the puck carrier
 * or the puck; buttons step back / forward, play on (with the recorded sounds) or stop; on exit
 * the game's frame, view and rink come back, then the sound and screen fade and the menu music
 * plays again if it was on. (phk / plb: the data bank is the program's from the free cursor on.) */
void ReplayMode(void)
{
    while (g.SoundFadeSpeed) hal_wait_interrupt();
    SPCGameUpload();
    g.Crowdlevel = 0;
    g.sflags |= 0x0800;
    ClrHor();
    g.textdirtylo = 0xFFFF;
    g.textdirtyhi = 0;
    FillVRAMSplit((uint16_t)(g.blankchar + g.charofs), 0x400, 0);
    g.replaylock = 0xFFFF;
    g.replaypanel = 0xFFFF;
    g.MapVRAM = 0;
    MapPartToVRAMOffset(rinkmapbuf, g.charofs, 2, 2, 0x2000, 0x20, 0x55, 8, 8);
    uint32_t saved_ptr = g.ReplayBufferPtr;
    g.sflags &= (uint16_t)~0x20;
    int16_t saved_v = g.Vpos, saved_h = g.Hpos;
    uint32_t p = g.ReplayBufferPtr;
    if (p < ReplayBufferStart || ReplayBufferEnd_addr < p || (p - ReplayBufferStart) >> 16) g.ReplayBufferPtr = ReplayBufferStart;   /* (Div32U's quotient unused) */
    uint32_t a4 = g.ReplayBufferPtr;
    uint16_t n = 0;
    do {
        a4 = suba4(a4);
        n++;
    } while (g.d7);
    for (n >>= 1; n; n--) a4 = adda4(a4);
    g.replayHpos = (uint16_t)g.Hpos;
    g.replayVpos = (uint16_t)g.Vpos;
    InitSprites(0x6000, 0, 0x1C0);
    g.brightness = 0;
    g.fadestep = 0xA;
    SetFrameHook(SYM_BrightnessHook);
    ScreenOn();
    SprSort();
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    g.d7 = 1;
    g.sflags3[0] &= (uint16_t)~2;
    Pad pd;
    uint16_t wait;
    goto frame;
view:
    if (!(g.replaylock & 0x8000)) {
        uint16_t x = g.replaylock;
        g.Xpos[CAM] = (int16_t)r16(RAM_OFF_Xpos + x);
        g.replayHpos = clamp_h(ease((uint16_t)g.Xpos[CAM], g.replayHpos, 4));
        g.Hpos = (int16_t)g.replayHpos;
        w16(RAM_OFF_Ypos + camobj, r16(RAM_OFF_Ypos + x));
        g.replayVpos = clamp_v(ease(r16(RAM_OFF_Ypos + camobj), g.replayVpos, 4));
        g.Vpos = (int16_t)g.replayVpos;
        w16(RAM_OFF_PadControlObj + 0xC, x >> 1);
    } else if (g.replaylock == 0xFFFF) {
        g.replayHpos = (uint16_t)g.Hpos;
        g.replayVpos = (uint16_t)g.Vpos;
    } else {
        g.replayHpos = clamp_h(ease((uint16_t)g.Xpos[CAM], g.replayHpos, 2));
        g.Hpos = (int16_t)g.replayHpos;
        g.replayVpos = clamp_v(ease(r16(RAM_OFF_Ypos + camobj), g.replayVpos, 2));
        g.Vpos = (int16_t)g.replayVpos;
    }
    g.timestep = g.d7;
    SprSort();
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    g.replaynewframe = 0;
frame:
    WaitFrames_Inc(g.d7 ? g.d7 : 1, 0);
    ReadPads();
    pd = getpzjoy();
    g.d7 = 1;
    if (!(pd.dir & 8)) {
        uint16_t d5 = pd.dir, d4 = 0xFFFF, best = 0xFFFF;
        g.replaylock = 0xFFFF;
        for (uint16_t a0 = 0x16; !(a0 & 0x8000); a0 = (uint16_t)(a0 - 2)) {
            if (r16(RAM_OFF_position + a0) & 0x8000) continue;
            uint16_t d0 = (uint16_t)(r16(RAM_OFF_Xpos + a0) - g.Xpos[CAM]);
            uint16_t d1 = (uint16_t)(r16(RAM_OFF_Ypos + a0) - r16(RAM_OFF_Ypos + camobj));
            uint16_t a = vtoa(d0, d1);
            if (a == 8) continue;
            a = (uint16_t)(a - d5);
            if (a && a != 1 && a != 7) continue;
            uint16_t h = (uint16_t)(d0 >> 3 | (d0 & 0x8000 ? 0xE000 : 0));
            uint16_t s = Mul8S(h, h);
            h = (uint16_t)(d1 >> 3 | (d1 & 0x8000 ? 0xE000 : 0));
            s = (uint16_t)(Mul8S(h, h) + s);
            if (s >= best) continue;
            best = s;
            d4 = a0;
        }
        if (!(d4 & 0x8000) && best < 2) {
            g.replaylock = d4;
            g.d7 = 0x1E;
            g.sflags &= (uint16_t)~0x20;
            goto view;
        }
        pd = getpzjoy();
        g.replaylock = 0xFFFE;
        g.sflags |= 0x20;
        uint16_t c = pd.dir >> 14 & 1, y = (uint16_t)(pd.dir << 2);   /* (the second asl's carry, into the adc) */
        uint16_t sx = db16(SYM_ReplayMode_14 >> 16, (uint16_t)(SYM_ReplayMode_14 + y));
        uint16_t sy = db16(SYM_ReplayMode_14 >> 16, (uint16_t)(SYM_ReplayMode_14 + 2 + y));
        uint16_t d2 = (uint16_t)(sx + g.Xpos[CAM] + c);
        w16(RAM_OFF_Xvel + camobj, (uint16_t)(sx << 8));
        uint16_t d3 = (uint16_t)(sy + r16(RAM_OFF_Ypos + camobj));
        w16(RAM_OFF_Yvel + camobj, (uint16_t)(sy << 8));
        w16(RAM_OFF_Wallcos + camobj, 0);
        w16(RAM_OFF_Wallsin + camobj, 0);
        g.wcradiusx = 1;
        g.wcradiusy = 1;
        checkwallcoll(CAM, d2, d3, CAM);
        uint16_t ws = r16(RAM_OFF_Wallsin + camobj), wc = r16(RAM_OFF_Wallcos + camobj);
        if (ws | wc) {
            uint16_t v = (uint16_t)(ws << 1) >> 8;
            if (v >= 0x80) v |= 0xFF00;
            d2 = (uint16_t)(v + g.Xpos[CAM]);
            v = (uint16_t)(wc << 1) >> 8;
            if (v >= 0x80) v |= 0xFF00;
            d3 = (uint16_t)(-v + r16(RAM_OFF_Ypos + camobj));
        }
        g.Xpos[CAM] = (int16_t)d2;
        g.Hpos = (int16_t)d2;
        w16(RAM_OFF_Ypos + camobj, d3);
        g.Vpos = (int16_t)d3;
        w16(RAM_OFF_PadControlObj + 0xC, 0x10);
        goto view;
    }
    if (g.replaylock == 0xFFFF) {
        uint16_t pc = r16(RAM_OFF_PadControlObj);
        w16(RAM_OFF_PadControlObj + 0xC, 0xF);
        if (pc != 0xF) {
            g.Xpos[CAM] = (int16_t)r16(RAM_OFF_Xpos + (uint16_t)(pc << 1));
            w16(RAM_OFF_Ypos + camobj, r16(RAM_OFF_Ypos + (uint16_t)(pc << 1)));
        } else {
            uint16_t a = (uint16_t)g.Xpos[14];
            if ((uint16_t)(a - 0xFF78) & 0x8000) a = 0xFF78;
            if (!((uint16_t)(a - sideline) & 0x8000)) a = sideline;
            g.Xpos[CAM] = (int16_t)a;
            a = (uint16_t)g.pucky[0];
            if ((uint16_t)(a - 0xFED4) & 0x8000) a = 0xFED4;
            if (!((uint16_t)(a - 0x12C) & 0x8000)) a = 0x12C;
            w16(RAM_OFF_Ypos + camobj, a);
        }
    }
    if (pd.press & 0x8000) {
        if (g.replaypanel) {
            FillVRAMSplit((uint16_t)(g.blankchar + g.charofs), 0x180, 0);
            WaitFrame();
            g.replaypanel = 0;
        }
        g.sflags3[0] ^= 2;
    }
    if (pd.held & 0x20) {
        a4 = suba4(a4);
        wait = 6;
        goto waits;
    }
    if (pd.held & 0x50) {
        a4 = adda4(a4);
        wait = pd.held & 0x10 ? 6 : 0xC;
        goto waits;
    }
    if (pd.press & 0x2000) {
        g.replaylock = 0xFFFF;
        g.sflags &= (uint16_t)~0x20;
        goto frame;
    }
    if (pd.press & 0x1000) goto out;
    if (pd.held & 0x80) {
        a4 = adda4(a4);
        a4 = adda4(a4);
        goto still;
    }
    if (pd.held & 0x4000) {
        a4 = suba4(a4);
        a4 = suba4(a4);
        goto still;
    }
    if (!(g.sflags3[0] & 2)) goto frame;
    a4 = adda4(a4);
    if (!g.d7) goto still;
    replay_sound(g.recsfx2, r16(RAM_OFF_recsfxvol2), r16(RAM_OFF_recsfxpitch2));
    replay_sound(g.recsfx, r16(RAM_OFF_recsfxvol), r16(RAM_OFF_recsfxpitch));
    goto view;
waits:
    WaitFrames_Inc(wait, 0);
still:
    SoundVoicesOff();
    g.d7 = 1;
    g.sflags3[0] &= (uint16_t)~2;
    goto view;
out:
    w16(RAM_OFF_PadControlObj + 0xC, 0xF);
    SoundVoicesOff();
    g.Hpos = saved_h;
    g.Vpos = saved_v;
    g.ReplayBufferPtr = saved_ptr;
    g.sflags &= (uint16_t)~0x20;
    RestoreReplayFrame(g.ReplayBufferPtr);
    g.sflags &= (uint16_t)~0x0800;
    SetHor();
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    g.textdirtylo = 0;
    g.textdirtyhi = 0x3FF;
    forcefade();
    FadeOutSound();
    while (g.brightness || g.SoundFadeSpeed) hal_wait_interrupt();
    forceblack();
    if (g.shootoutmode || !g.replaymusic) return;
    SPCMenuUpload();
    if (g.OptPlayMode && g.OptPlayMode != 4 && g.playoffRoundIdx >= 2) MusicStart(SYM_PlayoffSong);
    else MusicStart(SYM_MenuSong);
}

/* ReadjoyA for the controller that owns the pause menu */
Pad getpzjoy(void) { return ReadjoyA(g.pausepad >> 1); }
