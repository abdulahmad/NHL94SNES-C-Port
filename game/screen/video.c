/* video: the DMA queue, VRAM and colours, text maps, the sprite tile cache and the OAM, the IRQ,
 * NMI and frame hooks, fades, and the score, clock, crowd and sort coordinates
 * (src/routines/video). */
#include "game.h"

/* Arithmetic shifts of a word right */
static uint16_t asr4(uint16_t v) { return (uint16_t)((int16_t)v >> 4); }

/* One frame of hat k's flight: it moves; in the air it falls (hatvz, its own: launching clears only the
 * first hat's); on the ice it slides, slowing by a sixteenth, until it stops */
static void hat_move(uint16_t x)
{
    if (!(r16(RAM_OFF_hatvx + x) | r16(RAM_OFF_hatvy + x))) return;
    w16(RAM_OFF_hatx + x, (uint16_t)(r16(RAM_OFF_hatx + x) + r16(RAM_OFF_hatvx + x)));
    w16(RAM_OFF_haty + x, (uint16_t)(r16(RAM_OFF_haty + x) + r16(RAM_OFF_hatvy + x)));
    uint16_t z = r16(RAM_OFF_hatz + x);
    if (z) {
        uint16_t a = (uint16_t)(z - (uint16_t)((int16_t)r16(RAM_OFF_hatvz + x) >> 2));
        if (a & 0x8000) { w16(RAM_OFF_hatz + x, 0); return; }
        w16(RAM_OFF_hatz + x, a);
        w16(RAM_OFF_hatvz + x, (uint16_t)(r16(RAM_OFF_hatvz + x) + 6));
        return;
    }
    w16(RAM_OFF_hatvz + x, 0);
    w16(RAM_OFF_hatz + x, 0);
    w16(RAM_OFF_hattimer + x, 0x03E8);
    uint16_t vx = r16(RAM_OFF_hatvx + x);
    if (vx < 0x10 || vx >= 0xFFF0) {
        w16(RAM_OFF_hatvx + x, 0);
        w16(RAM_OFF_hatvy + x, 0);
        return;
    }
    w16(RAM_OFF_hatvx + x, (uint16_t)(vx - asr4(vx)));
    uint16_t vy = r16(RAM_OFF_hatvy + x);
    w16(RAM_OFF_hatvy + x, (uint16_t)(vy - asr4(vy)));
}

/* Referee graphics: only clears the referee flag here */
void showref(void) { g.sflags &= (uint16_t)~sf2refref; }

/* The crowd noise voice's gain and pitch from Crowdlevel (at most $380) */
void updatesound(void)
{
    uint16_t c = g.Crowdlevel;
    if (c >= 0x380) c = 0x380;
    uint16_t v = (uint16_t)((uint16_t)(c + 0x80) << 4);
    g.VoiceGain[1] = v;
    uint16_t p = (uint16_t)(DivX8U(v, 0x15) + 0x253);
    g.VoicePitch[1] = p;
    g.VoiceGain[0] = v < 0x2733 ? 0x100 : (uint16_t)((uint16_t)(v - 0x2600) << 2);
}

/* Print the game clock (m:ss, at 3, 24; on the horizontal rink at 13, 11, but not in period 4)
 * unless a replay is playing. The carry it leaves (showzam's): kept in a replay, set in period 4
 * on the horizontal rink, else clear (print's end) */
void showclock(void)
{
    if (g.sflags & sf2replay) return;
    g.mesarea[0] = 7;
    g.mesarea[1] = 0x20;
    g.mesarea[2] = 0x3A;
    uint8_t *m = (uint8_t *)g.mesarea;
    uint16_t n = g.gameclock[0], q = Div16x8U(n, 10);
    m[6] = (uint8_t)(n - q * 10 + 0x30);
    n = q; q = Div16x8U(n, 6);
    m[5] = (uint8_t)(n - q * 6 + 0x30);
    n = q; q = Div16x8U(n, 10);
    m[3] = (uint8_t)(n - q * 10 + 0x30);
    if (q) m[2] = (uint8_t)(q + 0x30);
    g.printa = 0x2000;
    g.printx = 3;
    g.printy = 0x18;
    if (g.sfhormode) {
        g.printx = 0xD;
        g.printy = 0xB;
        if (g.gsp == 4) { cf = 1; return; }
    }
    print(RAM_OFF_mesarea);
    cf = 0;
}

#define SPFsmallfont 433   /* (frames94s.asm) */

/* Draw sprite frame sprframe (at most $3D0) at sprx / spry, flipped by sprhflip / sprvflip */
void addframe(void)
{
    if (g.sprframe >= 0x3D1) return;
    if (g.sprhflip) g.sprattr |= 0x4000;
    if (g.sprvflip) g.sprattr |= 0x8000;
    uint16_t f = (uint16_t)(g.sprframe - 1);
    if (f & 0x8000) f = 0;
    if (f >= 0x3D0) f = 0x3D0;
    DrawSprite(g.sprattr, g.sprx, g.spry, f, SYM_SpriteFrames);
}

/* Draw the markers of the pad-controlled players (PadControlObj's slots 6 to 0): the slot's star,
 * and below it the player's number in the small font and, but in the shootout, his pad's button
 * label */
void setffo(void)
{
    for (uint16_t a2 = 0xC; !(a2 & 0x8000); a2 = (uint16_t)(a2 - 2)) {
        uint16_t obj = r16(RAM_OFF_PadControlObj + a2);
        if (obj == 0xF) continue;
        uint16_t r0 = rom16(SYM_setffo_7 + a2);
        uint16_t x = (uint16_t)(obj << 1);
        uint16_t d0 = r16(RAM_OFF_Xpos + x), d1 = r16(RAM_OFF_Ypos + x);
        find3d(&d0, &d1, 0);
        if (d1 == 0x4E20) continue;
        if (r16(RAM_OFF_position + x) & 0x8000) continue;
        DrawSprite(0x2000, d0, d1, r0, SYM_SpriteFrames);
        d1 = (uint16_t)(d1 + 0xA);
        if (x == 0x20) continue;
        uint16_t y = 0, n = r16(RAM_OFF_rostnum + x);
        if (n >> 4 & 0xF) {
            w16(RAM_OFF_mesarea + 2 + y++, n >> 4 & 0xF);
            d0 = (uint16_t)(d0 - 4);
        }
        w16(RAM_OFF_mesarea + 2 + y++, n & 0xF);
        d0 = (uint16_t)(d0 - 4);
        uint16_t p = r16(RAM_OFF_position + x) & 7;
        if (!g.shootoutmode && p) {
            w16(RAM_OFF_mesarea + 2 + y++, 0xFFFF);
            w16(RAM_OFF_mesarea + 2 + y++, rom16(SYM_ButtonLabelCharTable - 1 + p));
            d0 = (uint16_t)(d0 - 6);
        }
        w16(RAM_OFF_mesarea, y);
        y = 0;
        do {
            uint16_t c = r8(RAM_OFF_mesarea + 2 + y);
            if (c == 0xFF) d0 = (uint16_t)(d0 + 4);
            else {
                DrawSprite(0x2000, d0, d1, (uint16_t)(c + SPFsmallfont - 1), SYM_SpriteFrames);
                d0 = (uint16_t)(d0 + 8);
            }
            y++;
        } while (y != g.mesarea[0]);
    }
}

#define SPFcup 217
#define SPFcupover 225
#define SPFPen 361
#define SPFfaceoff 423
#define SPFzamboni 448
#define SPFbenchcheck 896

/* Draw the zamboni moving across the horizontal rink during the intermission (x from the frames
 * since vcountmark, less the borrow c leaves: the caller's carry) */
void showzam(int c)
{
    uint32_t lo = (uint32_t)g.vcount[0] - g.vcountmark[0] - !c;
    uint16_t hi = (uint16_t)(g.vcount[1] - g.vcountmark[1] - (lo >> 16 & 1));
    if (hi || !g.sfhormode || !g.InIntermission) return;
    g.sprhflip = 0;
    g.sprvflip = 0;
    g.sprattr = 0x2000;
    g.sprx = (uint16_t)lo >> 2;
    g.spry = 0xC8;
    g.sprframe = SPFzamboni;
    addframe();
}

/* Draw the big faceoff sprites (both centres and the ref) while a faceoff is set up on the
 * vertical rink */
void checkfo(void)
{
    if (g.sfhormode || g.penshot || !(g.sflags & 0x0100) || (g.sflags & 0x0800)) return;
    g.sprvflip = 0;
    g.sprpriority = 0x3000;
    for (uint16_t x = 0; x != 0xC; x += 4) {
        g.sprx = g.fodropx;
        g.spry = g.fodropy;
        g.sprframe = (uint16_t)(r16(RAM_OFF_fofdata2 + x) + SPFfaceoff - 1);
        g.sprhflip = r16(RAM_OFF_fofdata2 + 2 + x) & 0x0800;
        g.sprattr = (x == 4 ? 0x0200 : 0) | g.sprpriority;
        addframe();
    }
    g.sprpriority = 0x2000;
}

/* Draw the bench cheer, then the sorted objects (OOlist) in display order, hiding ones off the
 * screen: penalized players over the boards, flips from the attributes (none for the bench and
 * boards frames), palette $2200 for attribute bit 0, the cup over the cup frames */
void setsortcords(void)
{
    if (!(g.benchframe & 0x8000) && !g.sfhormode) {
        uint16_t x = g.benchframe >> 3;
        uint16_t r0 = (uint16_t)((g.benchframe & 7) + rom16(SYM_setsortcords_1 - 2 + x));
        uint16_t y = (uint16_t)(g.Vpos - rom16(SYM_setsortcords_2 - 2 + x));
        DrawSprite(rom16(SYM_setsortcords_3 - 2 + x), (uint16_t)(0xFFE2 - g.Hpos), y, r0, SYM_SpriteFrames);
    }
    for (uint16_t a4 = RAM_OFF_OOlist; a4 != 0x15D3; a4++) {
        uint16_t x = r8(a4);
        int o = x >> 1;
        uint16_t d2 = 0;
        if (x == 0x1C) d2 = (uint16_t)g.puckz;
        if (x == 0x1E) d2 = (uint16_t)g.shadowz;
        if (x < 0x18) {
            if (g.OptPlayMode == 4 && (g.position[o] & 0x8000)) continue;
            if (g.frame[o] == SPFPen + 7 || g.frame[o] == SPFPen + 8) g.frame[o] = g.sfhormode ? SPFPen + 7 : SPFPen + 8;
        }
        uint16_t d0 = (uint16_t)g.Xpos[o], d1 = r16(RAM_OFF_Ypos + x);
        find3d(&d0, &d1, d2);
        if (d1 == 0x4E20) continue;
        g.spry = d1;
        g.sprx = d0;
        g.oldframe[o] = g.frame[o];
        if (!g.frame[o] && x >= 0x18) continue;
        g.sprframe = g.frame[o];
        if (g.frame[o] == SPFPen + 8 || g.frame[o] >= SPFbenchcheck) {
            g.sprhflip = 0;
            g.sprvflip = 0;
        } else {
            g.sprhflip = g.attribute[o] & 0x0800;
            g.sprvflip = g.attribute[o] & 0x1000;
        }
        g.sprattr = g.attribute[o] & 1 ? 0x2200 : 0x2000;
        addframe();
        if (g.sprframe < SPFcup || g.sprframe >= SPFcupover + 8) continue;
        DrawSprite(g.sprattr & 0xF000, g.sprx, g.spry, (uint16_t)(g.sprframe + SPFcupover - SPFcup - 1), SYM_SpriteFrames);
    }
}

#define SPFarrow 374


/* (showcrowd's .7: crowd slot x shows step crowdanimstep of its sequence p0 (of animation p1) for
 * its time (3 * word 6): the frame's tiles sent to VRAM (word 0 the tiles a row, word 4 the first,
 * word 2 the VRAM row); unless the sequence has ended. The Montreal fans (slot $28) start the
 * $24 / $26 pair) */
static void crowd_frame(uint16_t x, uint32_t p1, uint32_t p0)
{
    uint16_t v = m16(p1 + 6);
    g.crowdanimtime[x >> 1] = (uint16_t)((uint16_t)(v << 1) + v + (v >> 15));   /* (asl's carry) */
    uint16_t f = m16(p0 + g.crowdanimstep[x >> 1]);
    if (f & 0x8000) return;
    uint16_t t = (uint16_t)(Mul8U(m16(p1), f) + m16(p1 + 4));
    uint32_t q = (SYM_CrowdTiles & 0xFF0000) | (uint16_t)((uint16_t)(t << 5) + (uint16_t)SYM_CrowdTiles);
    DMAToVRAM(&q, (uint16_t)(m16(p1) << 5), (uint16_t)((uint16_t)(m16(p1 + 2) << 4) + 0x1000), 0);
    if (x == 0x28 && !g.crowdanim[0x26 >> 1]) {
        g.crowdanim[0x24 >> 1] = 0xA;
        g.crowdanim[0x26 >> 1] = 0xA;
        g.crowdanimtime[0x24 >> 1] = 0;
        g.crowdanimtime[0x26 >> 1] = 0;
    }
}

/* (showcrowd's test of a slot's puck window: the puck above height $1A, its screen height (3 *
 * puckz + pucky) within [lo, lo + 8) and puckx within [xlo, xhi)) */
static int puck_in(uint16_t lo, uint16_t xlo, uint16_t xhi)
{
    uint16_t z = (uint16_t)g.puckz;
    if ((uint16_t)(z - 0x1A) & 0x8000) return 0;
    uint16_t t = (uint16_t)((uint16_t)((uint16_t)(z << 1) + z + (z >> 15)) + g.pucky[0]);
    if (!((uint16_t)(t - (lo + 8)) & 0x8000) || ((uint16_t)(t - lo) & 0x8000)) return 0;
    uint16_t px = (uint16_t)g.Xpos[14];
    return ((uint16_t)(px - xhi) & 0x8000) && !((uint16_t)(px - xlo) & 0x8000);
}

/* Animate the crowd sprites (slots $22 down to 0, from $28 in Montreal when its fans or song are
 * on): a running sequence steps on when its time runs out (at its end: a cheer goes back to the
 * fans' animation, the others to $0C, then now and then $0E, then off); an idle slot starts by
 * chance when the crowd is loud enough for it, the goal-end slots $22 / $20 when the puck flies
 * into their window; the hat trick hats first. Not while a replay holds its frame */
void showcrowd(void)
{
    if ((g.sflags & 0x0800) && !g.replaynewframe) return;
    UpdateHatTrick();
    uint16_t x = 0x22;
    if (g.HomeTeam == 0x13 && (g.crowdanim[0x28 >> 1] || g.cursong == 2)) x = 0x28;
    for (; !(x & 0x8000); x = (uint16_t)(x - 2)) {
        int k = x >> 1;
        if ((x == 0 || x == 0xA) && g.crowdanim[0x28 >> 1]) continue;
        uint32_t bank = SYM_CrowdAnimTable & 0xFF0000;
        uint32_t p1 = bank | rom16(SYM_CrowdAnimTable + x), p0;
        if (g.crowdanim[k]) {
            g.crowdanimtime[k] = (uint16_t)(g.crowdanimtime[k] - g.timestep);
            if (!(g.crowdanimtime[k] & 0x8000)) continue;
            p0 = bank | m16(p1 + g.crowdanim[k]);
            uint16_t y = g.crowdanimstep[k];
            if (!(m16(p0 + y) & 0x8000)) {
                y = (uint16_t)(y + 2);
                if (!(m16(p0 + y) & 0x8000)) {
                    g.crowdanimstep[k] = y;
                    crowd_frame(x, p1, p0);
                    continue;
                }
            }
            if (g.crowdanim[k] == 0xE) {
                g.crowdanim[k] = 0;
                continue;
            }
            if (x == 0x24 || x == 0x26) g.crowdanim[k] = g.crowdanim[0x28 >> 1];
            else if (g.crowdanim[k] != 0xC) g.crowdanim[k] = 0xC;
            else if (!(RandomWord() & 0xC0)) g.crowdanim[k] = 0xE;
            p0 = bank | m16(p1 + g.crowdanim[k]);
            g.crowdanimstep[k] = 0;
            crowd_frame(x, p1, p0);
            continue;
        }
        if (x == 0x26 || x == 0x24) continue;
        if (x == 0x22) { if (!puck_in(0xFEB0, 0xFFEC, 0xFFF4)) continue; }
        else if (x == 0x20) { if (!puck_in(0xFE90, 0xFFF4, 0xFFFD)) continue; }
        else if (x != 0x28) {
            uint16_t a = (uint16_t)(m16(p1 + 8) << 6);
            if (a != g.Crowdlevel && a >= g.Crowdlevel) continue;
            if (RandomWord() & 0x03F0) continue;
        }
        g.crowdanim[k] = 0xA;
        g.crowdanimstep[k] = 0;
        p0 = bank | m16(p1 + 0xA);
        crowd_frame(x, p1, p0);
    }
}

/* setvideo's .t1: the arrow r0 (3 frames: up / down, the side, a corner) of the player pad y
 * controls when he is off the screen, at the screen's edge toward him */
void setvideo_t1(uint16_t y, uint16_t r0)
{
    uint16_t o = r16(RAM_OFF_c1playernum + y);
    if (o & 0x8000) return;
    uint16_t d0, d1;
    if (g.sfhormode) {
        d0 = (uint16_t)-r16(RAM_OFF_Ypos + o);
        d1 = (uint16_t)(r16(RAM_OFF_Xpos + o) - 0xC5);
    } else {
        d0 = (uint16_t)(r16(RAM_OFF_Xpos + o) - g.Hpos);
        d1 = (uint16_t)(r16(RAM_OFF_Ypos + o) - g.Vpos);
    }
    uint16_t d2 = 0;
    if (!((uint16_t)(d0 - 0x74) & 0x8000)) d2 = 8;
    if ((uint16_t)(d0 - 0xFF8D) & 0x8000) d2 |= 4;
    if (!((uint16_t)(d1 - 0x64) & 0x8000)) d2++;
    if ((uint16_t)(d1 - 0xFF9D) & 0x8000) d2 += 2;
    if (!d2) return;
    uint32_t t = SYM_setvideo_8 + (uint16_t)((rom8(SYM_jdtab8F + d2)) << 3);
    if (rom16(t)) d0 = rom16(t);
    if (rom16(t + 2)) d1 = rom16(t + 2);
    r0 = (uint16_t)(r0 + rom16(t + 4));
    DrawSprite((uint16_t)(0x2000 | rom16(t + 6)), (uint16_t)(d0 + 0x80), (uint16_t)((uint16_t)-d1 + 0x70), r0, SYM_SpriteFrames);
}

/* setvideo's .t3: the off-screen arrows of the five pads (not in the pregame pause or with sflags
 * bit 11) */
void setvideo_t3(void)
{
    if (g.pregamepause || (g.sflags & 0x0800)) return;
    for (uint16_t y = 0; y < 0xA; y += 2) setvideo_t1(y, (uint16_t)(SPFarrow - 1 + 3 * (y >> 1)));
}

/* Once per game frame: build the sprite table for the next vblank (the rink, faceoff sprites, the
 * off-screen arrows, the sorted objects, pad markers, clock, zamboni, crowd, referee, the rink
 * rows, on the horizontal rink the team blocks) and queue the changed text */
void setvideo(void)
{
    updatescroll();
    checkfo();
    setvideo_t3();
    setsortcords();
    setffo();
    showclock();
    showzam(cf);
    showcrowd();
    showref();
    updatescroll_rows();
    if (g.sfhormode) DrawHorTeams();
    QueueTextDMA();
}

/* Run the routine at hook (bank << 16 | address) as the frame hook of the next NMI */
void SetFrameHook(uint32_t hook)
{
    g.FrameHookNext[2] = (uint8_t)(hook >> 16);
    w16(RAM_OFF_FrameHookNext, (uint16_t)hook);
}

/* Set the NMI hook back to the empty routine */
void ClearNMIHook(void)
{
    g.NMIHookNext[2] = SYM_NullHook >> 16;
    w16(RAM_OFF_NMIHookNext, (uint16_t)SYM_NullHook);
}

/* Wait for the next frame (FrameCount changes: the NMI increments it) */
void WaitFrame(void)
{
    uint16_t f = g.FrameCount;
    while (wait_r16(RAM_OFF_FrameCount) == f) wait_tick();
}

/* Turn on an IRQ at line y with the routine at hook (bank << 16 | address): NMITIMEN $81 while the
 * hook is changed, then a frame later $B1 (NMI, pads and the H/V IRQ) */
void SetIRQ(uint32_t hook, uint16_t y)
{
    io_w8(0x4200, 0x81);
    g.IRQVTime = y;
    io_w16(0x4209, y);
    io_w16(0x4207, g.IRQHTime);
    g.IRQHook[2] = (uint8_t)(hook >> 16);
    w16(RAM_OFF_IRQHook, (uint16_t)hook);
    WaitFrame();
    io_w8(0x4200, 0xB1);
    IRQHook_Off();   /* (falls through into it) */
}

/* Turn the IRQ off (NMI and pad reading on) and the IRQ hook to IRQHook_Off */
void IRQOff(void)
{
    io_w8(0x4200, 0x81);
    g.IRQHook[2] = 0;
    w16(RAM_OFF_IRQHook, (uint16_t)SYM_IRQHook_Off);
}

/* NMI hook: the fade brightness to INIDISP unless the screen is blanked */
void BrightnessHook(void)
{
    if (g.IniDispShadow[0] & 0x80) return;
    io_w8(0x2100, (uint8_t)g.brightness);
}

/* IRQ hooks: the BG1 scroll splits of the stats screens and the band, each installing the next */
static void set_irq_hook(uint32_t hook) { w16(RAM_OFF_IRQHook, (uint16_t)hook); }

/* Stats screen split 1 (at irqline1): BG1VOFS = irqvscroll1, then IRQ_StatsScroll2 at irqline2;
 * scrollirqdone set for the frame */
void IRQ_StatsScroll1(void)
{
    io_w8(0x210E, (uint8_t)g.irqvscroll1);
    io_w8(0x210E, (uint8_t)(g.irqvscroll1 >> 8));
    set_irq_hook(SYM_IRQ_StatsScroll2);
    io_w16(0x4209, g.irqline2);
    g.scrollirqdone[0] = 0xFFFF;
}

/* Stats screen split 2 (at irqline2): BG1VOFS = irqvscroll2, then IRQ_StatsScroll3 at irqline3 */
void IRQ_StatsScroll2(void)
{
    io_w8(0x210E, (uint8_t)g.irqvscroll2);
    io_w8(0x210E, (uint8_t)(g.irqvscroll2 >> 8));
    set_irq_hook(SYM_IRQ_StatsScroll3);
    io_w16(0x4209, g.irqline3);
}

/* Stats screen split 3 (at irqline3): BG1VOFS = irqvscroll3, then IRQ_StatsScroll1 again (the NMI
 * sets VTIME back to irqline1) */
void IRQ_StatsScroll3(void)
{
    io_w8(0x210E, g.irqvscroll3[0]);
    io_w8(0x210E, g.irqvscroll3[1]);
    set_irq_hook(SYM_IRQ_StatsScroll1);
}

/* Top of the band (at irqline1): BG1 scrolled to ($FFFC, $FFFB), then IRQ_BandEnd at irqline2 */
void IRQ_BandStart(void)
{
    io_w8(0x210D, 0xFC); io_w8(0x210D, 0xFF);
    io_w8(0x210E, 0xFB); io_w8(0x210E, 0xFF);
    set_irq_hook(SYM_IRQ_BandEnd);
    io_w16(0x4209, g.irqline2);
}

/* Bottom of the band (at irqline2): BG1 scrolled back to (0, $FFFF), then IRQ_BandStart at irqline1 */
void IRQ_BandEnd(void)
{
    io_w8(0x210D, 0); io_w8(0x210D, 0);
    io_w8(0x210E, 0xFF); io_w8(0x210E, 0xFF);
    set_irq_hook(SYM_IRQ_BandStart);
    io_w16(0x4209, g.irqline1);
}

/* A two-part BG1 split: BG1VOFS = BG3VScroll, then IRQ_BG1Scroll_t1 at line $30 */
void IRQ_BG1Scroll(void)
{
    io_w8(0x210E, (uint8_t)g.BG3VScroll);
    io_w8(0x210E, (uint8_t)(g.BG3VScroll >> 8));
    set_irq_hook(SYM_IRQ_BG1Scroll_t1);
    io_w16(0x4209, 0x0030);
}

/* IRQ_BG1Scroll's second part (at line $30): BG1VOFS back to $FFFF, then IRQ_BG1Scroll again */
void IRQ_BG1Scroll_t1(void)
{
    io_w8(0x210E, 0xFF); io_w8(0x210E, 0xFF);
    set_irq_hook(SYM_IRQ_BG1Scroll);
}

/* Copy x colours from [p0] into the colour buffer from colour y */
void SetColours(uint32_t p0, uint16_t x, uint16_t y)
{
    uint16_t k = (uint16_t)(y << 1), j = 0;
    do {
        w16(RAM_OFF_colourbuf + k, m16(p0 + j));
        j += 2; k += 2;
    } while (--x);
}

/* Wait x frames, y counting the rounds of 65536 (WaitFrames_Inc adds 1 to y first) */
void WaitFrames(uint16_t x, uint16_t y)
{
    do {
        do { WaitFrame(); } while (--x);
    } while (--y);
}

/* Wait until the fade has reached black */
void WaitFadeOut(void)
{
    do WaitFrame(); while (g.brightness);
}

/* The band IRQ pair (lines a and x) that shifts the BG1 scroll */
void SetupBandIRQ(uint16_t a, uint16_t x)
{
    g.irqline1 = a;
    g.irqline2 = x;
    g.IRQHTime = 0x00C4;
    SetIRQ(SYM_IRQ_BandStart, g.irqline1);
}

/* The split scroll of a stats screen: the IRQ lines a, x, y, the three scrolls $FFFF */
void SetupScrollIRQ(uint16_t a, uint16_t x, uint16_t y)
{
    g.irqline1 = a;
    g.irqline2 = x;
    g.irqline3 = y;
    g.irqvscroll1 = g.irqvscroll2 = 0xFFFF;
    w16(RAM_OFF_irqvscroll3, 0xFFFF);
    g.IRQHTime = 0x00C4;
    SetIRQ(SYM_IRQ_StatsScroll1, g.irqline1);
}

/* Reset the scroll, window and colour math registers and their copies */
void ResetPPURegs(void)
{
    g.BG1HScroll = g.BG2HScroll = 0;
    w16(RAM_OFF_BG3HScroll, 0);
    g.BG1VScroll = g.BG2VScroll = g.BG3VScroll = 0xFFFF;
    g.ClipTopUnused = 0xFFF0;
    g.r9[0] = 0xFFF0;
    g.ClipBottomUnused = 0x00E0;
    g.r9[1] = 0x0100;
    static const uint16_t regs[] = { 0x2131, 0x2130, 0x212E, 0x2106, 0x2133, 0x212D, 0x212F, 0x2123, 0x2124, 0x2125 };
    for (unsigned i = 0; i < sizeof regs / sizeof *regs; i++) io_w8(regs[i], 0);
    g.Mode7InUse = 0;
}

/* Move the unused OAM entries off screen (Y = $E1), swap the OAM buffers and set OAMReady */
void SpriteFrameEnd(void)
{
    uint16_t x = g.OAMNext;
    while (x < g.OAMHideEnd) {
        w16(x, 0xE100);
        x += 4;
        if (!x) break;
    }
    uint16_t last = g.OAMLastEnd;
    g.OAMLastEnd = g.OAMNext;
    g.OAMHideEnd = last;
    uint16_t d = g.OAMDrawBuf;
    g.OAMDrawBuf = g.OAMShowBuf;
    g.OAMShowBuf = d;
    g.OAMReady[0] = 1;
}

/* Add a DMA of x bytes from [p0] to video memory at y to the NMI's transfer list, in pieces of
 * $1000 bytes at most; the VRAM address after it is returned and *p0 moves past the source (its
 * low word: the second add takes the first's carry) */
uint16_t QueueVRAMDMA(uint32_t *p0, uint16_t x, uint16_t y)
{
    uint16_t a = x, t = g.DMAQueueTail, lo = (uint16_t)*p0;
    for (;;) {
        uint16_t n = a >= 0x1000 ? 0x1000 : a;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, n);
        uint16_t words = n >> 1;
        w16(e + 6, y);
        y = (uint16_t)(y + words);
        w16(e + 1, lo);
        uint32_t s = (uint32_t)lo + words;
        lo = (uint16_t)(s + words + (s >> 16));
        w8(e + 3, (uint8_t)(*p0 >> 16));
        w8(e, 1);
        t = (t + 8) & 0x03FF;
        if (a <= 0x1000) break;
        a = (uint16_t)(a - 0x1000);
    }
    g.DMAQueueTail = t;
    *p0 = (*p0 & 0xFF0000) | lo;
    return y;
}

/* Full brightness (fading in unless already there), no forced blank */
void ScreenOn(void)
{
    if (g.brightness != 0x000F) FadeIn();
    g.IniDispShadow[0] = 0x0F;
}

/* Wait for a frame, set the default hooks, turn the IRQ off, force blank (a frame later HDMA off)
 * and drop the DMA queue */
void BlankScreen(void)
{
    WaitFrame();
    IRQOff();
    ClearFrameHook();
    ClearNMIHook();
    g.IniDispShadow[0] = 0x8F;
    WaitFrame();
    io_w8(0x420C, 0);
    g.DMAQueueTail = g.DMAQueueHead;
}

/* DMA x bytes from [p0] to video memory at y (negative: where it is, r3 as the caller left it):
 * at once with the screen blanked, else queued for the NMI (QueueVRAMDMA, which moves *p0). The
 * VRAM address after it. */
uint16_t DMAToVRAM(uint32_t *p0, uint16_t x, uint16_t y, uint16_t r3)
{
    if (!(r16(RAM_OFF_IniDispShadow - 1) & 0x8000)) return QueueVRAMDMA(p0, x, y);
    io_w16(0x4315, x);
    io_w16(0x4312, (uint16_t)*p0);
    io_w16(0x4310, 0x1801);
    if (!(y & 0x8000)) { io_w16(0x2116, y); r3 = y; }
    io_w8(0x4314, (uint8_t)(*p0 >> 16));
    io_w8(0x420B, 0x02);
    return (uint16_t)((x >> 1) + r3 + (x & 1));
}

/* DMA x bytes from [p0] to the high bytes of video memory at y. Queued: in $1000-byte pieces of
 * type $FA, the VRAM address stepping by the bytes and the source by $1000 a piece. */
uint16_t DMAToVRAMHigh(uint32_t *p0, uint16_t x, uint16_t y)
{
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w16(0x4315, x);
        io_w16(0x4312, (uint16_t)*p0);
        io_w16(0x4310, 0x1900);
        io_w16(0x2116, y);
        io_w8(0x4314, (uint8_t)(*p0 >> 16));
        io_w8(0x420B, 0x02);
        return (uint16_t)((x >> 1) + y + (x & 1));
    }
    uint16_t a = x, t = g.DMAQueueTail;
    for (;;) {
        uint16_t n = a >= 0x1001 ? 0x1000 : a;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, n);
        w16(e + 6, y);
        y = (uint16_t)(n + y);
        w16(e + 1, (uint16_t)*p0);
        *p0 = (*p0 & 0xFF0000) | (uint16_t)(*p0 + 0x1000);
        w8(e + 3, (uint8_t)(*p0 >> 16));
        w8(e, 0xFA);
        t = (t + 8) & 0x03FF;
        if (a <= 0x1000) break;
        a = (uint16_t)(a - 0x1000);
    }
    g.DMAQueueTail = t;
    return y;
}

/* Fill x bytes of video memory at y (negative: where it is, r3 as the caller left it) with the
 * word a, the DMA's fixed source at $0016 (r3+2, which holds a): at once with the screen
 * blanked, else queued in $1000-byte pieces (type 9). The VRAM address after it. */
uint16_t FillVRAM(uint16_t a, uint16_t x, uint16_t y, uint16_t r3)
{
    g.r3[1] = a;
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w16(0x4315, x);
        if (!(y & 0x8000)) { io_w16(0x2116, y); r3 = y; }
        io_w16(0x4312, 0x0016);
        io_w8(0x4310, 0x09);
        io_w8(0x4311, 0x18);
        io_w8(0x4314, 0);
        io_w8(0x420B, 0x02);
        return (uint16_t)((x >> 1) + r3 + (x & 1));
    }
    uint16_t n = x, t = g.DMAQueueTail;
    for (;;) {
        uint16_t k = n >= 0x1001 ? 0x1000 : n;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, k);
        w16(e + 6, y);
        y = (uint16_t)((k >> 1) + y);
        w16(e + 1, 0x0016);
        w8(e + 3, 0);
        w8(e, 0x09);
        t = (t + 8) & 0x03FF;
        if (n <= 0x1000) break;
        n = (uint16_t)(n - 0x1000);
    }
    g.DMAQueueTail = t;
    return y;
}

/* Wait until the DMA queue is empty (the NMI moves its head up to the tail) */
void WaitDMAQueue(void)
{
    while (wait_r16(RAM_OFF_DMAQueueHead) != g.DMAQueueTail) wait_tick();
}

/* Copy map [*p0] (width, height, then the entries from +6) into the map at [*p1] (r7 entries a
 * row), column x, row y, adding a to each entry; *p0 and *p1 end past the last row (their low
 * words, with the carries of the original's adds) */
void PasteMap(uint32_t *p0, uint32_t *p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    uint16_t lo1 = (uint16_t)((uint16_t)*p1 + (uint16_t)s + (s >> 16));
    uint16_t w = m16(*p0), h = m16(*p0 + 2);
    uint16_t lo0 = (uint16_t)((uint16_t)*p0 + 6);
    uint32_t b0 = *p0 & 0xFF0000, b1 = *p1 & 0xFF0000;
    do {
        uint16_t k = 0, n = w;
        do {
            mset16((b1 | lo1) + k, (uint16_t)(m16((b0 | lo0) + k) + a));
            k += 2;
        } while (--n);
        lo1 = (uint16_t)((uint16_t)(r7 << 1) + lo1 + (r7 >> 15));
        lo0 = (uint16_t)((uint16_t)(w << 1) + lo0 + (w >> 15));
    } while (--h);
    *p0 = b0 | lo0;
    *p1 = b1 | lo1;
}

/* Copy a part of map [*p0] (its width at +0, entries from +6): r4 entries by r4+2 rows from column
 * r6, row r6+2, into the map at [*p1] (r7 entries a row) at column x, row y, adding a to each
 * entry; *p0 and *p1 end past the last row (their low words, with the original's carries) */
void PasteMapPart(uint32_t *p0, uint32_t *p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7, uint16_t r6, uint16_t r6h, uint16_t r4, uint16_t r4h)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    uint16_t lo1 = (uint16_t)((uint16_t)*p1 + (uint16_t)s + (s >> 16));
    uint16_t w = m16(*p0);
    r2 = Mul16U((uint16_t)(r6h << 1), w);
    s = (uint32_t)(uint16_t)(r6 << 1) + (uint16_t)r2;
    s = (uint32_t)(uint16_t)s + 6 + (s >> 16);
    uint16_t lo0 = (uint16_t)((uint16_t)*p0 + (uint16_t)s + (s >> 16));
    uint32_t b0 = *p0 & 0xFF0000, b1 = *p1 & 0xFF0000;
    do {
        uint16_t k = 0, n = r4;
        do {
            mset16((b1 | lo1) + k, (uint16_t)(m16((b0 | lo0) + k) + a));
            k += 2;
        } while (--n);
        lo1 = (uint16_t)((uint16_t)(r7 << 1) + lo1 + (r7 >> 15));
        lo0 = (uint16_t)((uint16_t)(w << 1) + lo0 + (w >> 15));
    } while (--r4h);
    *p0 = b0 | lo0;
    *p1 = b1 | lo1;
}

/* Fill y bytes at [p1] with the byte a, from the top down (y = 1 goes round the 64 KB) */
void PasteMapPart_t1(uint32_t p1, uint8_t a, uint16_t y)
{
    y--;
    do { mset8(p1 + y, a); y--; } while (y);
    mset8(p1, a);
}

/* Start the sprites: sprite tile 0 at VRAM a, x OAM entries reserved, y tiles (at least $1C0, else
 * $1A0, in 32s) for the 16x16 blocks; the block and tile allocators empty, the frame cache clear,
 * the OAM buffers reset and an empty frame ended */
void InitSprites(uint16_t a, uint16_t x, uint16_t y)
{
    g.SpriteVRAM = a;
    g.SpritesReserved = x;
    uint16_t t = y >= 0x01C0 ? y : 0x01A0;
    g.SpriteBlockTiles = (uint16_t)(t + 0x1F) & 0xFFE0;
    g.FrameCacheCount = 0;
    uint8_t k = (uint8_t)(g.SpriteBlockTiles >> 2), v = 0;
    do {
        w8(RAM_OFF_BlockRuns + k, v);
        k = (uint8_t)(k - 8);
        w8(RAM_OFF_BlockRunPrev + (uint8_t)(k + 8), k);
        v = 8;
    } while (!(k & 0x80));
    g.BlockRunPrev[0] = 0xFF;
    g.CacheUseMark = 2;
    uint8_t n = (uint8_t)(0x0200 - g.SpriteBlockTiles);
    g.TileRuns[0] = n;
    w8(RAM_OFF_TileRuns + n, 0);
    w8(RAM_OFF_TileRunPrev + n, 0);
    g.TileRunPrev[0] = 0xFF;
    g.SpritesLeft = 0x0080;
    g.OAMNext = 0x2100;
    g.OAMHideEnd = 0x2300;
    g.OAMLastEnd = 0x2520;
    g.OAMShowBuf = 0x2320;
    g.OAMDrawBuf = 0x2100;
    for (int i = 0x03D0; i >= 0; i -= 2) w16(RAM_OFF_FrameCacheOf + i, 0xFFFF);
    SpriteFrameEnd();
}

/* Fill x VRAM words at y with the word a, the low bytes (from $0016) then the high bytes ($0017):
 * at once with the screen blanked, else queued in pairs of $1000-byte pieces (types $FD, $FC). The
 * VRAM address after it (y + x). */
uint16_t FillVRAMSplit(uint16_t a, uint16_t x, uint16_t y)
{
    g.r3[1] = a;
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w16(0x4315, x);
        io_w16(0x2116, y);
        io_w16(0x4312, 0x0016);
        io_w16(0x4310, 0x1808);
        io_w8(0x2115, 0);
        io_w8(0x4314, 0);
        io_w8(0x420B, 0x02);
        io_w16(0x4315, x);
        io_w16(0x2116, y);
        io_w16(0x4312, 0x0017);
        io_w16(0x4310, 0x1908);
        io_w8(0x2115, 0x80);
        io_w8(0x4314, 0);
        io_w8(0x420B, 0x02);
        return (uint16_t)(x + y);
    }
    uint16_t n = x, t = g.DMAQueueTail;
    for (;;) {
        uint16_t k = n >= 0x1001 ? 0x1000 : n;
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 4, k); w16(e + 6, y); w16(e + 1, 0x0016); w8(e + 3, 0); w8(e, 0xFD);
        t = (t + 8) & 0x03FF;
        e = RAM_OFF_DMAQueue + t;
        w16(e + 4, k); w16(e + 6, y);
        y = (uint16_t)(k + y);
        w16(e + 1, 0x0017); w8(e + 3, 0); w8(e, 0xFC);
        t = (t + 8) & 0x03FF;
        if (n <= 0x1000) break;
        n = (uint16_t)(n - 0x1000);
    }
    g.DMAQueueTail = t;
    return y;
}

/* The sprite VRAM allocators: BlockRuns (16x16 blocks, from tile 0) and TileRuns (8x8 tiles,
 * after SpriteBlockTiles) are byte lists of runs, each run's first byte its length (bit 7 used;
 * a block run of a chain of 8-unit runs has bit 6), with the run before it in BlockRunPrev /
 * TileRunPrev ($FF none); a length of 0 ends the list. All of it 8-bit, as the original. */
static uint8_t br(uint8_t x) { return r8(RAM_OFF_BlockRuns + x); }

static void br_w(uint8_t x, uint8_t v) { w8(RAM_OFF_BlockRuns + x, v); }

static void brp_w(uint8_t x, uint8_t v) { w8(RAM_OFF_BlockRunPrev + x, v); }

/* (record the frame in the tile cache: its entry number by frame number, address, bank, number,
 * allocation, first tile; the entry's VRAM address, tile * 16 + SpriteVRAM with the carry of the
 * last shift) */
static uint16_t cache_add(uint32_t p0, uint16_t alloc, uint16_t tile)
{
    w8(RAM_OFF_FrameCacheOf + 1 + g.DrawFrameNum, (uint8_t)g.FrameCacheCount);
    uint16_t y = (uint16_t)(g.FrameCacheCount << 1);
    w16(RAM_OFF_FrameCacheNum + y, g.DrawFrameNum);
    w16(RAM_OFF_FrameCacheAddr + y, (uint16_t)p0);
    w16(RAM_OFF_FrameCacheBank + y, (uint16_t)(p0 >> 16));
    g.FrameCacheCount++;
    w16(RAM_OFF_FrameCacheAlloc + y, alloc);
    w16(RAM_OFF_FrameCacheTile + y, tile);
    return (uint16_t)((uint16_t)(tile << 4) + g.SpriteVRAM + (tile >> 12 & 1));
}

static uint16_t cache_result(void) { return (uint16_t)((g.FrameCacheCount - 1) << 1); }

/* (LoadFrameTiles: DMA queue entry x, type 1 (DMAP), len bytes from bank:src; the caller stores
 * the VRAM address at +6) */
static void dma_entry(uint16_t x, uint16_t src, uint8_t bank, uint16_t len)
{
    w8(RAM_OFF_DMAQueue + x, 1);
    w16(RAM_OFF_DMAQueue + x + 1, src);
    w8(RAM_OFF_DMAQueue + x + 3, bank);
    w16(RAM_OFF_DMAQueue + x + 4, len);
}

/* (split run x of length len at need: the rest a free run after it, which points back at x, and
 * the run after the rest at the rest; the second add takes the first's carry) */
static void split_run(void (*runs_w)(uint8_t, uint8_t), void (*prev_w)(uint8_t, uint8_t), uint8_t x, uint8_t len, uint8_t need)
{
    uint8_t rest = (uint8_t)(len - need);
    if (!rest) return;
    unsigned s = (unsigned)x + need;
    uint8_t y = (uint8_t)s;
    runs_w(y, rest);
    prev_w(y, x);
    prev_w((uint8_t)(y + rest + (s >> 8)), y);
}

static uint8_t tr(uint8_t x) { return r8(RAM_OFF_TileRuns + x); }
static void tr_w(uint8_t x, uint8_t v) { w8(RAM_OFF_TileRuns + x, v); }
static void trp_w(uint8_t x, uint8_t v) { w8(RAM_OFF_TileRunPrev + x, v); }

/* Allocate sprite VRAM for the frame at [p0] (p0 the direct page's 4 bytes: the cache keeps the word
 * at p0+2), queue the DMA of its tiles (one or two rows: PieceX / PieceY bytes) and add a tile cache
 * entry: its entry * 2, $FFFF when there is no room or all 128 entries are in use, x when the
 * frame has no pieces. A negative piece count is a frame of 16x16 blocks (in units of 4 tiles,
 * more than 8 taking a chain of 8-unit runs), else of 8x8 tiles. */
uint16_t LoadFrameTiles(uint32_t p0, uint16_t x)
{
    uint32_t f = p0 & 0xFFFFFF;
    if (g.FrameCacheCount == 0x0080) return 0xFFFF;
    uint8_t bank = (uint8_t)(p0 >> 16);
    uint16_t r0 = m16(f);
    if (!r0) return x;
    g.PieceX = m16(f + 0x0C);
    g.PieceY = m16(f + 0x0E);
    uint16_t src = (uint16_t)(m16(f + 0x10) + (uint16_t)p0);
    if (!(r0 & 0x8000)) {
        uint16_t need = m16(f + 2);
        uint8_t k = 0;
        for (;;) {
            uint8_t v = tr(k);
            if (!v) return 0xFFFF;
            if (!(v & 0x80) && v >= (uint8_t)need) break;
            k = (uint8_t)((k + v) & 0x7F);
        }
        split_run(tr_w, trp_w, k, tr(k), (uint8_t)need);
        tr_w(k, (uint8_t)need | 0x80);
        uint16_t t = g.DMAQueueTail;
        dma_entry(t, src, bank, g.PieceX);
        uint16_t vram = cache_add(p0, k, (uint16_t)(k + g.SpriteBlockTiles));
        w16(RAM_OFF_DMAQueue + t + 6, vram);
        g.DMAQueueTail = (t + 8) & 0x03FF;
        return cache_result();
    }
    uint16_t need = (uint16_t)((uint16_t)(m16(f + 2) + 3) & 0xFFFC) >> 2;
    uint16_t run;
    if (need < 9) {
        uint8_t k = 0;
        for (;;) {
            uint8_t v = br(k);
            if (!v) return 0xFFFF;
            if (v < 0x10 && v >= (uint8_t)need) break;
            k = (uint8_t)(k + (v & 0x3F));
        }
        run = k;
        split_run(br_w, brp_w, k, br(k), (uint8_t)need);
        br_w(k, (uint8_t)need | 0x80);
    } else {
        uint8_t k = 0, sum = 0, first = 0;
        for (;;) {
            uint8_t v = br(k);
            if (!v) return 0xFFFF;
            if (v == 8) {
                first = k;
                sum = v;
                for (;;) {
                    k = (uint8_t)(k + br(k));
                    v = br(k);
                    if (!v || v >= 0x10) break;
                    sum = (uint8_t)(v + sum);
                }
                if (sum >= (uint8_t)need) break;
                v = br(k);
            }
            k = (uint8_t)(k + (v & 0x3F));
        }
        k = first;
        run = k;
        uint8_t left = (uint8_t)need;
        for (;;) {
            uint8_t d = (uint8_t)(left - br(k));
            if (!d) break;
            if (d & 0x80) {
                uint8_t rest = (uint8_t)-d;
                unsigned s = (unsigned)k + left;
                uint8_t y = (uint8_t)s;
                br_w(y, rest);
                brp_w(y, k);
                brp_w((uint8_t)(y + rest + (s >> 8)), y);
                break;
            }
            left = d;
            br_w(k, 0xC8);
            k = (uint8_t)(k + 8);
        }
        br_w(k, left | 0x80);
        uint16_t t = g.DMAQueueTail;
        dma_entry(t, src, bank, g.PieceX);
        src = (uint16_t)(g.PieceX + src);
        uint16_t vram = cache_add(p0, run | 0x8000, (uint16_t)(((run & 0xFFF8) << 1) + (run & 7)) << 1);
        w16(RAM_OFF_DMAQueue + t + 6, vram);
        t = (t + 8) & 0x03FF;
        if (!g.PieceY) {
            g.DMAQueueTail = t;
            return cache_result();
        }
        dma_entry(t, src, bank, g.PieceY);
        w16(RAM_OFF_DMAQueue + t + 6, (uint16_t)((((g.PieceX >> 1) & 0xFF00) | 0x0100) + vram));
        g.DMAQueueTail = (t + 8) & 0x03FF;
        return cache_result();
    }
    uint16_t t = g.DMAQueueTail;
    dma_entry(t, src, bank, g.PieceX);
    src = (uint16_t)(g.PieceX + src);
    uint16_t vram = cache_add(p0, run | 0x8000, (uint16_t)(((run & 0xFFF8) << 1) + (run & 7)) << 1);
    w16(RAM_OFF_DMAQueue + t + 6, vram);
    t = (t + 8) & 0x03FF;
    dma_entry(t, src, bank, g.PieceY);
    if (!g.PieceY) {
        g.DMAQueueTail = t;
        return cache_result();
    }
    w16(RAM_OFF_DMAQueue + t + 6, vram | 0x0100);
    g.DMAQueueTail = (t + 8) & 0x03FF;
    return cache_result();
}

/* The tile cache entry (* 2) of the frame at [p0], searching y entries from x; else LoadFrameTiles */
uint16_t FindCachedFrame(uint32_t p0, uint16_t x, uint16_t y)
{
    do {
        if (r16(RAM_OFF_FrameCacheAddr + x) == (uint16_t)p0 && r16(RAM_OFF_FrameCacheBank + x) == (uint16_t)(p0 >> 16)) return x;
        x += 2;
    } while (--y);
    return LoadFrameTiles(p0, x);
}

/* The first VRAM tile of the frame at [p0] (its entry by frame number with FrameByNumber, else by
 * address), loaded now if needed (x as the original's X there), its use mark refreshed;
 * FrameTilesFail and $FFFF when there is no room */
uint16_t GetFrameTiles(uint32_t p0, uint16_t x)
{
    uint16_t y = g.FrameCacheCount;
    if (y) {
        if (!g.FrameByNumber) {
            x = 0;
            do {
                hal_cost_mark(SYM_GetFrameTiles_5);
                if (r16(RAM_OFF_FrameCacheAddr + x) == (uint16_t)p0 && r16(RAM_OFF_FrameCacheBank + x) == (uint16_t)(p0 >> 16)) goto found;
                x += 2;
            } while (--y);
        } else {
            x = g.DrawFrameNum;
            uint16_t a = r16(RAM_OFF_FrameCacheOf + x);
            if (!(a & 0x8000)) { x = (uint16_t)((a >> 8) << 1); goto found; }
        }
    }
    x = LoadFrameTiles(p0, x);
    if (x & 0x8000) { g.FrameTilesFail = 0xFFFF; return 0xFFFF; }
found:
    w16(RAM_OFF_FrameCacheUse + x, g.CacheUseMark);
    return r16(RAM_OFF_FrameCacheTile + x);
}

/* (one OAM entry for the current piece at (px, py), if on screen: the tile word (piece tile + first
 * tile, xor r3) and the high table's size and X bits; mode: 0 plain, 1 mirrored, 2 flipped (x
 * mirrored too when r3 has $4000)) */
static int piece_entry(int mode, uint16_t r3, uint16_t tile0, uint16_t r5, uint16_t r5h, uint16_t *ox)
{
    uint16_t ctr = r16(RAM_OFF_PieceSize) ? 0x000F : 0x0007, py, px;
    py = mode == 2 ? (uint16_t)(r5h - g.PieceY - ctr) : (uint16_t)(g.PieceY + r5h);
    if ((uint16_t)(py - 0xFFF0) & 0x8000) return 0;
    if (!((uint16_t)(py - 0x00E0) & 0x8000)) return 0;
    if (mode == 1 || (mode == 2 && (r3 & 0x4000))) px = (uint16_t)(r5 - g.PieceX - ctr);
    else px = (uint16_t)(g.PieceX + r5);
    if (!((uint16_t)(px - 0x0100) & 0x8000)) return 0;
    if ((uint16_t)(px - 0xFFF0) & 0x8000) return 0;
    uint16_t x = *ox;
    w16(x, px);
    w16(x + 1, py);
    w16(x + 2, (uint16_t)((uint16_t)(g.PieceTile + tile0) ^ r3));
    uint16_t hp = g.OAMHiPtr, sh = g.OAMHiShift;
    uint8_t v = r8(hp), sz = r8(RAM_OFF_PieceSize);
    uint32_t m = RAM_OFF_OAMHiMasks + sh;
    if (px & 0x8000) v = sz ? (uint8_t)(v | r8(m + 12)) : (uint8_t)((v & r8(m)) | r8(m + 4));
    else v = sz ? (uint8_t)((v & r8(m)) | r8(m + 8)) : (uint8_t)(v & r8(m));
    w8(hp, v);
    g.SpritesLeft--;
    sh--;
    if (sh & 0x8000) { g.OAMHiShift = 3; g.OAMHiPtr++; }
    else g.OAMHiShift = sh;
    if (mode < 2) hal_cost_mark(mode ? SYM_DrawSprite_draw_4x : SYM_DrawSprite_draw_4);   /* (a piece drawn) */
    g.OAMNext = (uint16_t)(g.OAMNext + 4);
    *ox = g.OAMNext;
    return 1;
}

/* (DrawSprite_draw's piece loop: an OAM entry for each of the r0 pieces of the frame at [p0], 7
 * bytes each from +$16 (X, Y, tile, size), while sprites are left) */
static void draw_pieces(int mode, uint32_t p0, uint16_t r3, uint16_t tile0, uint16_t r5, uint16_t r5h, uint16_t r0, uint16_t x)
{
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t y = 0x0016;
    do {
        if (mode < 2) hal_cost_mark(mode ? SYM_DrawSprite_draw_8 : SYM_DrawSprite_draw_2);   /* (the piece loop's head) */
        if (!g.SpritesLeft) return;
        g.PieceX = m16(f + y);
        g.PieceY = m16(f + (uint16_t)(y + 2));
        g.PieceTile = m16(f + (uint16_t)(y + 4));
        w16(RAM_OFF_PieceSize, m8(f + (uint16_t)(y + 6)));
        g.PieceNext = (uint16_t)(y + 7);
        piece_entry(mode, r3, tile0, r5, r5h, &x);
        y = g.PieceNext;
    } while (--r0);
}

/* Draw the frame at [p0] at (r5, r5h), r3 xored into each tile word (bit 15: flipped, the pieces'
 * positions mirrored, DrawSpritePieces; bit 14: mirrored); x is the original's X at the tile cache
 * lookup */
void DrawSprite_draw(uint32_t p0, uint16_t r3, uint16_t r5, uint16_t r5h, uint16_t x)
{
    uint16_t tile0 = GetFrameTiles(p0, x);
    if (tile0 & 0x8000) return;
    uint16_t r0 = m8(p0 & 0xFFFFFF);
    draw_pieces(r3 & 0x8000 ? 2 : r3 & 0x4000 ? 1 : 0, p0, r3, tile0, r5, r5h, r0, g.OAMNext);
}

/* Draw frame `frame` of the sprite set at [p1] (p1 the direct page's 4 bytes) at (x, y), a xored into
 * the tile and attribute word */
void DrawSprite(uint16_t a, uint16_t x, uint16_t y, uint16_t frame, uint32_t p1)
{
    g.DrawFrameNum = frame;
    uint32_t s = p1 & 0xFFFFFF;
    uint16_t k = (uint16_t)(frame << 2);
    uint32_t lo = (uint32_t)m16(s + k) + (uint16_t)p1;
    uint16_t hi = (uint16_t)(m16(s + (uint16_t)(k + 2)) + (uint16_t)(p1 >> 16) + (lo >> 16));
    DrawSprite_draw((uint16_t)lo | (uint32_t)hi << 16, a, x, y, x);
}

/* Free tile cache entry x: the last entry moves into its slot (its frame pointed at it), and its
 * VRAM run is given back, joined with a free run after it (blocks: in the same row of 8) and with
 * a free run before it */
void FreeCacheEntry_go(uint16_t x)
{
    uint16_t r0 = r16(RAM_OFF_FrameCacheAlloc + x);
    if (--g.FrameCacheCount) {
        uint16_t y = (uint16_t)(g.FrameCacheCount << 1);
        if (y != x) {
            static const uint32_t f[] = { RAM_OFF_FrameCacheAddr, RAM_OFF_FrameCacheBank, RAM_OFF_FrameCacheNum, RAM_OFF_FrameCacheAlloc, RAM_OFF_FrameCacheTile, RAM_OFF_FrameCacheUse };
            for (int i = 0; i < 6; i++) w16(f[i] + x, r16(f[i] + y));
            w8(RAM_OFF_FrameCacheOf + 1 + r16(RAM_OFF_FrameCacheNum + x), (uint8_t)(x >> 1));
        }
    }
    uint8_t k = (uint8_t)r0;
    if (!(r0 & 0x8000)) {
        uint8_t len = tr(k) & 0x7F;
        uint8_t y = (uint8_t)(len + k);
        if (!(tr(y) & 0x80)) len = (uint8_t)(tr(y) + len);
        uint8_t p = r8(RAM_OFF_TileRunPrev + k);
        if (!(p & 0x80) && !(tr(p) & 0x80)) {
            unsigned s = (unsigned)tr(p) + len;
            tr_w(p, (uint8_t)s);
            trp_w((uint8_t)(p + (uint8_t)s + (s >> 8)), p);
            return;
        }
        tr_w(k, len);
        trp_w((uint8_t)(k + len), k);
        return;
    }
    if (br(k) & 0x40) {
        while (br(k) & 0x40) { br_w(k, 8); k = (uint8_t)(k + 8); }
        uint8_t len = br(k) & 0x3F;
        uint8_t y = (uint8_t)(k + len);
        if ((y & 7) && !(br(y) & 0x80)) len = (uint8_t)(br(y) + len);
        br_w(k, len);
        brp_w((uint8_t)(k + len), k);
        return;
    }
    uint8_t len = br(k) & 0x3F;
    uint8_t y = (uint8_t)(len + k);
    if ((y & 7) && !(br(y) & 0x80)) len = (uint8_t)(br(y) + len);
    if (k & 7) {
        uint8_t p = r8(RAM_OFF_BlockRunPrev + k);
        if (!(p & 0x80) && !(br(p) & 0x80)) {
            unsigned s = (unsigned)br(p) + len;
            br_w(p, (uint8_t)s);
            brp_w((uint8_t)(p + (uint8_t)s + (s >> 8)), p);
            return;
        }
    }
    br_w(k, len);
    brp_w((uint8_t)(k + len), k);
}

/* Free entry x of frame number y (its FrameCacheOf slot cleared); _A: the entry in a's high byte */
void FreeCacheEntry(uint16_t x, uint16_t y)
{
    w8(RAM_OFF_FrameCacheOf + 1 + y, 0xFF);
    FreeCacheEntry_go(x);
}

/* Free the VRAM tiles of the frame at [p0] (by its number with FrameByNumber); not cached: nothing.
 * Found by its address the frame keeps its FrameCacheOf slot. */
void FreeFrameTiles(uint32_t p0)
{
    uint16_t y = g.FrameCacheCount;
    if (!y) return;
    if (g.FrameByNumber) {
        uint16_t a = r16(RAM_OFF_FrameCacheOf + g.DrawFrameNum);
        if (!(a & 0x8000)) FreeCacheEntryA(a, g.DrawFrameNum);
        return;
    }
    uint16_t x = 0;
    do {
        if (r16(RAM_OFF_FrameCacheAddr + x) == (uint16_t)p0 && r16(RAM_OFF_FrameCacheBank + x) == (uint16_t)(p0 >> 16)) { FreeCacheEntry_go(x); return; }
        x += 2;
    } while (--y);
}

/* (unused entries) FreeFrameTiles for frame a of the sprite set at [p1]; the sum of the free
 * block run lengths, walking from index SpriteBlockTiles */
void FreeFrameTiles_t1(uint16_t a, uint32_t p1)
{
    g.DrawFrameNum = a;
    uint32_t s = p1 & 0xFFFFFF;
    uint16_t k = (uint16_t)(a << 2);
    uint32_t lo = (uint32_t)m16(s + k) + (uint16_t)p1;
    uint16_t hi = (uint16_t)(m16(s + (uint16_t)(k + 2)) + (uint16_t)(p1 >> 16) + (lo >> 16));
    FreeFrameTiles((uint16_t)lo | (uint32_t)hi << 16);
}

/* (unused entry) The sum of the free block run lengths, walking BlockRuns from SpriteBlockTiles
 * (after a free run the step is the sum so far, as the original) */
uint16_t FreeFrameTiles_t2(void)
{
    uint16_t sum = 0, x = g.SpriteBlockTiles;
    for (;;) {
        uint16_t a = r8(RAM_OFF_BlockRuns + x);
        if (!a) return sum;
        if (a < 0x10) a = sum = (uint16_t)(a + sum);   /* (the step is then the sum's) */
        x = (uint16_t)(x + (a & 0x3F));
    }
}

/* Start filling OAMDrawBuf after the reserved entries (the high table's shift from the byte offset,
 * as the original); age the tile cache's use marks and free the entries not drawn since */
void SpriteFrameStart(void)
{
    uint16_t sr = g.SpritesReserved;
    g.SpritesLeft = (uint16_t)(0x0080 - sr);
    uint16_t x = sr >> 2;
    uint32_t s = (uint32_t)x + g.OAMDrawBuf;
    g.OAMHiPtr = (uint16_t)(s + 0x0200 + (s >> 16));
    g.OAMHiShift = (x & 3) ^ 3;
    g.OAMNext = (uint16_t)((uint16_t)(sr << 2) + g.OAMDrawBuf + (sr >> 14 & 1));
    if (!g.FrameCacheCount) return;
    for (int16_t k = (int16_t)((g.FrameCacheCount - 1) << 1); k >= 0; k -= 2) {
        uint16_t u = r16(RAM_OFF_FrameCacheUse + k);
        if (u & 0x8000) continue;
        w16(RAM_OFF_FrameCacheUse + k, u >> 1);
        if (!(u & 1)) continue;
        FreeCacheEntry((uint16_t)k, r16(RAM_OFF_FrameCacheNum + k));
    }
}

/* DMA x colours from [p0] to CGRAM at colour y: at once with the screen blanked (none for x = 0;
 * y is a byte there, CGADD), else left for the NMI (CGDMALen etc.; the colour word as y is) */
void DMAToCGRAM(uint32_t p0, uint16_t x, uint16_t y)
{
    if (g.IniDispShadow[0] & 0x80) {
        io_w8(0x2121, (uint8_t)y);
        uint16_t n = (uint16_t)(x << 1);
        if (!n) return;
        io_w16(0x4315, n);
        io_w16(0x4310, 0x2200);
        io_w16(0x4312, (uint16_t)p0);
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x420B, 0x02);
        return;
    }
    w16(RAM_OFF_CGDMAColour, y);
    w16(RAM_OFF_CGDMABank, (uint16_t)(p0 >> 16));
    g.CGDMASrc = (uint16_t)p0;
    g.CGDMALen = (uint16_t)(x << 1);
}

/* (unused entry) x colours from colour y of [p0] into [p1]: a = 0 fills them with y * 2 (the
 * index, as the original leaves it in A), $0100 copies them, else scales each component by the
 * byte a / 256 (the multiplier) */
void DMAToCGRAM_t3(uint32_t p0, uint32_t p1, uint16_t a, uint16_t x, uint16_t y)
{
    uint16_t k = (uint16_t)(y << 1);
    if (!a) { do { mset16(p1 + k, (uint16_t)(y << 1)); k += 2; } while (--x); return; }
    if (a == 0x0100) { do { mset16(p1 + k, m16(p0 + k)); k += 2; } while (--x); return; }
    uint16_t f = a & 0xFF;
    do {
        uint16_t c = m16(p0 + k);
        uint16_t v = (uint16_t)((c & 0x1F) * f) >> 8 & 0x1F;
        v |= (uint16_t)(((c >> 5) & 0x1F) * f) >> 3 & 0x03E0;
        v |= (uint16_t)((uint16_t)(((c >> 10) & 0x1F) * f) << 2) & 0x7C00;
        mset16(p1 + k, v);
        k += 2;
    } while (--x);
}

/* Store at y:x the colour of this moment (vcount) of a 60-frame pulse: white to $7E10 and back,
 * each component interpolated over 30 frames (the divides' remainders as the divider leaves them) */
void PulseColour(uint16_t x, uint16_t y)
{
    uint16_t t = (uint16_t)(g.vcount[0] % 30), half = (uint16_t)(g.vcount[0] / 30) % 2;
    uint32_t tab = SYM_PulseColour_1 + (uint16_t)(half << 1);
    uint16_t c0 = rom16(tab), c1 = rom16(tab + 2);
    static const int sh[3] = { 10, 5, 0 };
    uint16_t col = 0;
    for (int i = 0; i < 3; i++) {
        uint16_t from = sh[i] == 10 ? (c0 >> 8 & 0xFF) >> 2 : c0 >> sh[i] & 0x1F;
        uint16_t to = sh[i] == 10 ? (c1 >> 8 & 0xFF) >> 2 : c1 >> sh[i] & 0x1F;
        uint16_t q = Div32S(Mul16S((uint16_t)(to - from), t), 0x001E);
        col |= (uint16_t)(((uint16_t)(q + from) & 0x1F) << sh[i]);
    }
    mset16((uint32_t)y << 16 | x, col);
}

/* (the screen map in VRAM: the word address of column c, row r) */
static uint16_t map_addr(uint16_t c, uint16_t r)
{
    uint16_t s = (uint16_t)(r << 4);
    return (uint16_t)(((uint16_t)(s << 1) + g.MapVRAM + (s >> 15)) | c);
}

/* (MapToVRAM with the screen blanked: channel 1's size n, source src and VMADD vram for a row; the
 * caller starts the DMA) */
static void map_dma_now(uint16_t n, uint16_t src, uint16_t vram)
{
    io_w16(0x4315, n);
    io_w16(0x4312, src);
    io_w16(0x2116, vram);
}

/* Copy map [p0] (width, height, then the entries from +6) into the screen map in VRAM at column x,
 * row y, a row at a time (a row crossing column 32 in two, the rest on the next screen); at once
 * with the screen blanked, else queued */
void MapToVRAM(uint32_t p0, uint16_t x, uint16_t y)
{
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t c = x, row = y, w = m16(f), h = m16(f + 2);
    uint16_t src = (uint16_t)((uint16_t)p0 + 6), v = map_addr(c, row);
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x4310, 0x01);
        io_w8(0x4311, 0x18);
        do {
            if ((uint16_t)(w + c) < 0x0021) map_dma_now((uint16_t)(w << 1), src, v);
            else {
                uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
                map_dma_now(r6, src, v);
                io_w8(0x420B, 0x02);
                io_w16(0x4315, (uint16_t)((uint16_t)(w << 1) - r6));
                uint32_t s = (uint32_t)src + r6;
                io_w16(0x4312, (uint16_t)s);
                io_w16(0x2116, (uint16_t)(v + 0x0400 + (s >> 16)) & 0xFFE0);
            }
            io_w8(0x420B, 0x02);
            uint32_t s = (uint32_t)src + w;
            src = (uint16_t)(s + w + (s >> 16));
            v = (uint16_t)(v + 0x20);
            if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
        } while (--h);
        return;
    }
    uint32_t q = (p0 & 0xFF0000) | src;
    do {
        if ((uint16_t)(w + c) < 0x0021) QueueVRAMDMA(&q, (uint16_t)(w << 1), v);
        else {
            uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
            QueueVRAMDMA(&q, r6, v);
            uint16_t n2 = (uint16_t)((uint16_t)(w << 1) - r6);
            unsigned nb = (uint16_t)(w << 1) >= r6;
            QueueVRAMDMA(&q, n2, (uint16_t)(v + 0x0400 + nb) & 0xFFE0);
        }
        v = (uint16_t)(v + 0x20);
        if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
    } while (--h);
}

/* Copy a part of map [p0] (its width at +0): r4 entries by r4+2 rows from column r7, row r7+2, into
 * the screen map at column x (32 or more: the next screen), row y; at once or queued as MapToVRAM */
void MapPartToVRAM(uint32_t p0, uint16_t x, uint16_t y, uint16_t r7, uint16_t r7h, uint16_t r4, uint16_t r4h)
{
    if (x >= 0x0020) { x &= 0x1F; g.MapVRAM = (uint16_t)(g.MapVRAM + 0x0400); }
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t stride = (uint16_t)(m16(f) << 1);
    uint16_t r2 = (uint16_t)Mul16U(stride, r7h);
    r7 = (uint16_t)(r7 << 1);
    uint32_t s = (uint32_t)(uint16_t)p0 + 6;
    s = (uint32_t)(uint16_t)s + r7 + (s >> 16);
    uint16_t src = (uint16_t)((uint16_t)s + r2 + (s >> 16));
    uint16_t c = x, row = y, v = map_addr(c, row);
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x4310, 0x01);
        io_w8(0x4311, 0x18);
        do {
            if ((uint16_t)(r4 + c) < 0x0021) map_dma_now((uint16_t)(r4 << 1), src, v);
            else {
                uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
                map_dma_now(r6, src, v);
                io_w8(0x420B, 0x02);
                io_w16(0x4315, (uint16_t)((uint16_t)(r4 << 1) - r6));
                uint32_t t = (uint32_t)src + r6;
                io_w16(0x4312, (uint16_t)t);
                io_w16(0x2116, (uint16_t)(v + 0x0400 + (t >> 16)) & 0xFFE0);
            }
            io_w8(0x420B, 0x02);
            src = (uint16_t)(src + stride);
            v = (uint16_t)(v + 0x20);
            if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
        } while (--r4h);
        return;
    }
    uint32_t bank = p0 & 0xFF0000;
    do {
        uint32_t q = bank | src;
        if ((uint16_t)(r4 + c) < 0x0021) QueueVRAMDMA(&q, (uint16_t)(r4 << 1), v);
        else {
            uint16_t r6 = (uint16_t)((uint16_t)(0x20 - c) << 1);
            QueueVRAMDMA(&q, r6, v);
            uint16_t n2 = (uint16_t)((uint16_t)(r4 << 1) - r6);
            unsigned nb = (uint16_t)(r4 << 1) >= r6;
            QueueVRAMDMA(&q, n2, (uint16_t)(v + 0x0400 + nb) & 0xFFE0);
        }
        src = (uint16_t)(src + stride);
        v = (uint16_t)(v + 0x20);
        if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
    } while (--r4h);
}

/* The same into VRAM through VMDATA with the screen blanked (it waits for that), adding a and
 * ORing r3 into each entry */
void MapPartToVRAMOffset(uint32_t p0, uint16_t a, uint16_t x, uint16_t y, uint16_t r3, uint16_t r7, uint16_t r7h, uint16_t r4, uint16_t r4h)
{
    g.r3[1] = a;
    while (!(wait_r16(RAM_OFF_IniDispShadow - 1) & 0x8000)) wait_tick();
    if (x >= 0x0020) { x &= 0x1F; g.MapVRAM = (uint16_t)(g.MapVRAM + 0x0400); }
    uint32_t f = p0 & 0xFFFFFF, bank = p0 & 0xFF0000;
    uint16_t stride = (uint16_t)(m16(f) << 1);
    uint16_t r2 = (uint16_t)Mul16U(stride, r7h);
    r7 = (uint16_t)(r7 << 1);
    uint32_t s = (uint32_t)(uint16_t)p0 + 6;
    s = (uint32_t)(uint16_t)s + r7 + (s >> 16);
    uint16_t src = (uint16_t)((uint16_t)s + r2 + (s >> 16));
    uint16_t c = x, row = y, v = map_addr(c, row);
    do {
        io_w16(0x2116, v);
        uint16_t k = 0, n;
        if ((uint16_t)(r4 + c) < 0x0021) n = r4;
        else {
            uint16_t r6 = (uint16_t)(0x20 - c);
            n = r6;
            do { io_w16(0x2118, (uint16_t)(m16((bank | src) + k) + a) | r3); k += 2; } while (--n);
            io_w16(0x2116, (uint16_t)(v + 0x0400) & 0xFFE0);
            n = (uint16_t)(r4 - r6);
        }
        do { io_w16(0x2118, (uint16_t)(m16((bank | src) + k) + a) | r3); k += 2; } while (--n);
        src = (uint16_t)(src + stride);
        v = (uint16_t)(v + 0x20);
        if (++row == 0x20) v = (uint16_t)(v + g.MapScreenStep);
    } while (--r4h);
}

/* The IRQ: acknowledge TIMEUP and run the hook at IRQHook */
void IRQ_Fast(void)
{
    io_r8(0x4211);
    call_long((uint32_t)g.IRQHook[2] << 16 | r16(RAM_OFF_IRQHook));
}

/* Convert linear 4bpp tiles at p0 (r0 bytes: r0 / 4 + 1 rows of 8 pixels, one row more than the
 * bytes) to SNES planar into p1 (a 16-bit address in p0's bank): each row's two words, a nibble
 * per pixel (a zero word skips its 4 pixels), into the row's plane words at +0 / +$10 of its tile */
void LinearToPlanar(uint32_t p0, uint16_t p1, uint16_t r0)
{
    uint32_t bank = p0 & 0xFF0000;
    uint16_t src = (uint16_t)p0, row = 0, out = 0xFFFE;
    int16_t n = (int16_t)(r0 >> 2);
    do {
        uint8_t t[4] = { 0, 0, 0, 0 };   /* (r3, r3+1, r3+2, r3+3) */
        uint16_t x = 0x0080, y = (uint16_t)(row << 2);
        for (;;) {
            uint16_t w = m16(bank + src + y);
            if (!w) {
                x >>= 4;
                if (!x) break;
                y = (uint16_t)(y + 2);
                continue;
            }
            uint16_t v = (uint16_t)(w << 8 | w >> 8);
            for (;;) {
                if (v & 0x8000) t[1] |= (uint8_t)x;
                if (v & 0x4000) t[0] |= (uint8_t)x;
                if (v & 0x2000) t[3] |= (uint8_t)x;
                if (v & 0x1000) t[2] |= (uint8_t)x;
                v = (uint16_t)(v << 4);
                x >>= 1;
                if (x >= 0x10 || (x && x < 8)) continue;
                break;
            }
            if (!x) break;
            y = (uint16_t)((row << 2) + 2);
        }
        out = (uint16_t)(out + 2);
        if (out & 0x10) out = (uint16_t)(out + 0x10);
        mset16(bank + p1 + out, (uint16_t)(t[2] | t[3] << 8));
        mset16(bank + p1 + (uint16_t)(out + 0x10), (uint16_t)(t[0] | t[1] << 8));
        row++;
    } while (--n >= 0);
}

/* Copy byte map [p0] (width, height, then the bytes from +6) into the mode 7 map at column x, row
 * y (128 bytes a row), a row at a time: at once with the screen blanked (VMAIN 0, the low bytes),
 * else queued (type $FB) */
void Mode7MapToVRAM(uint32_t p0, uint16_t x, uint16_t y)
{
    uint32_t f = p0 & 0xFFFFFF;
    uint16_t w = m16(f), h = m16(f + 2), src = (uint16_t)((uint16_t)p0 + 6);
    uint16_t s = (uint16_t)(y << 6);
    uint16_t v = (uint16_t)(((uint16_t)(s << 1) + g.MapVRAM + (s >> 15)) | x);
    if (r16(RAM_OFF_IniDispShadow - 1) & 0x8000) {
        io_w8(0x4314, (uint8_t)(p0 >> 16));
        io_w8(0x4310, 0);
        io_w8(0x2115, 0);
        io_w8(0x4311, 0x18);
        do {
            io_w16(0x4315, w);
            io_w16(0x4312, src);
            io_w16(0x2116, v);
            io_w8(0x420B, 0x02);
            src = (uint16_t)(src + w);
            v = (uint16_t)(v + 0x80);
        } while (--h);
        io_w8(0x2115, 0x80);
        return;
    }
    uint16_t t = g.DMAQueueTail;
    do {
        uint32_t e = RAM_OFF_DMAQueue + t;
        w16(e + 1, src);
        src = (uint16_t)(src + w);
        w16(e + 4, w);
        w16(e + 6, v);
        v = (uint16_t)(v + 0x80);
        w8(e + 3, (uint8_t)(p0 >> 16));
        w8(e, 0xFB);
        t = (t + 8) & 0x03FF;
    } while (--h);
    g.DMAQueueTail = t;
}

/* (unused entry) paste r6+2 bytes of [p0] into the map at [p1] (r7 entries a row) at column x,
 * row y as words + a; a 0 byte ends the string, the rest of the count filled with $20 + a */
void Mode7MapToVRAM_t1(uint32_t p0, uint32_t p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7, uint16_t r6h)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    p1 = (p1 & 0xFF0000) | (uint16_t)((uint16_t)p1 + (uint16_t)s + (s >> 16));
    uint16_t n = r6h, k = 0;
    uint32_t b0 = p0 & 0xFF0000;
    uint16_t lo0 = (uint16_t)p0;
    do {
        uint16_t c = m8(b0 | lo0);
        if (!c) {
            for (;;) {
                if ((int16_t)--n < 0) return;
                mset16(p1 + k, (uint16_t)(0x20 + a));
                k += 2; lo0++;
            }
        }
        mset16(p1 + k, (uint16_t)(c + a));
        k += 2; lo0++;
    } while (--n);
}

/* The NMI vector: VBlank, then NMI_FrameHookReturn unless NMISkip was set when it came */
void NMI(void)
{
    uint16_t skip = g.NMISkip;
    VBlank();
    if (!skip) NMI_FrameHookReturn(nmi_b);
}

/* The NMI: nothing while NMISkip is set (the 16-bit RDNMI read takes TIMEUP too), else the DMA budget
 * for the frame, the NMI hook (NMIHookNext taken when set), then NMI_HookReturn. The NMI_Tick at its
 * end runs the frame hook; the frame hook's own return, NMI_FrameHookReturn (the pads, SoundUpdate,
 * the sound stream), is the caller's (NMI). */
void VBlank(void)
{
    io_r16(0x4210);
    if (g.NMISkip) return;
    w16(RAM_OFF_DMABudget, 0x16D3);
    if (g.NMIHookNext[1]) {
        w16(RAM_OFF_NMIHook, r16(RAM_OFF_NMIHookNext));
        w16(RAM_OFF_NMIHookNext, 0);
        g.NMIHook[2] = g.NMIHookNext[2];
    }
    call_long((uint32_t)g.NMIHook[2] << 16 | r16(RAM_OFF_NMIHook));
    NMI_HookReturn();
}

/* (a DMA of channel 0 for a queue entry: its source and size from the entry, to VRAM at +6, if the
 * budget left covers cost + its size; 0 when not) */
static int nmi_dma(uint16_t x, uint16_t dmap, uint16_t cost, int vmain)
{
    uint32_t e = RAM_OFF_DMAQueue + x;
    io_w8(0x4302, r8(e + 1));
    io_w16(0x4300, dmap);
    uint16_t b = r16(RAM_OFF_DMABudget);
    if (b < cost) return 0;
    b = (uint16_t)(b - cost);
    if (b < r16(e + 4)) return 0;
    w16(RAM_OFF_DMABudget, (uint16_t)(b - r16(e + 4)));
    io_w16(0x4303, r16(e + 2));
    io_w16(0x4305, r16(e + 4));
    io_w16(0x2116, r16(e + 6));
    if (vmain) io_w8(0x2115, 0);
    io_w8(0x420B, 0x01);
    if (vmain) io_w8(0x2115, 0x80);
    return 1;
}

/* The NMI after its hook: the IRQ timers, INIDISP, the six BG scroll registers, the colour DMA left
 * by DMAToCGRAM, then the DMA queue while the frame's budget lasts (entry types: a DMAP value; $FE /
 * $FF one or two words written through VMDATA; $FD / $FC low / high byte fills; $FB low bytes; the
 * rest high bytes), then NMI_SendOAM (NMI_Tick when the budget ran out) */
void NMI_HookReturn(void)
{
    io_w16(0x4209, g.IRQVTime);
    io_w16(0x4207, g.IRQHTime);
    io_w8(0x2100, g.IniDispShadow[0]);
    for (int x = 0, y = 0; x != 0x0C; x += 2, y++) {
        io_w8((uint16_t)(0x210D + y), r8(RAM_OFF_BG1HScroll + x));
        io_w8((uint16_t)(0x210D + y), r8(RAM_OFF_BG1HScroll + 1 + x));
    }
    if (g.CGDMALen) {
        io_w16(0x4300, 0x2200);
        io_w16(0x4302, g.CGDMASrc);
        io_w8(0x4304, g.CGDMABank[0]);
        io_w16(0x4305, g.CGDMALen);
        io_w8(0x2121, g.CGDMAColour[0]);
        io_w8(0x420B, 0x01);
        uint32_t d = (uint32_t)r16(RAM_OFF_DMABudget) - g.CGDMALen;
        w16(RAM_OFF_DMABudget, (uint16_t)(d - 0x48 - (d >> 31)));
        g.CGDMALen = 0;
    }
    uint16_t x = g.DMAQueueHead;
    while (x != g.DMAQueueTail) {
        uint32_t e = RAM_OFF_DMAQueue + x;
        uint8_t t = r8(e);
        int ok;
        if (!(t & 0x80)) {
            io_w8(0x4300, t);
            io_w8(0x4301, 0x18);
            io_w8(0x4302, r8(e + 1));
            uint16_t b = r16(RAM_OFF_DMABudget);
            ok = b >= 0x0066 && (uint16_t)(b - 0x0066) >= r16(e + 4);
            if (ok) {
                w16(RAM_OFF_DMABudget, (uint16_t)(b - 0x0066 - r16(e + 4)));
                io_w16(0x4303, r16(e + 2));
                io_w16(0x4305, r16(e + 4));
                io_w16(0x2116, r16(e + 6));
                io_w8(0x420B, 0x01);
            }
        } else if (t == 0xFE || t == 0xFF) {
            uint16_t b = r16(RAM_OFF_DMABudget);
            ok = b >= 0x005C;
            if (ok) {
                w16(RAM_OFF_DMABudget, (uint16_t)(b - 0x005C));
                io_w16(0x2116, r16(e + 2));
                io_w16(0x2118, r16(e + 4));
                if ((uint16_t)(t + 1) != 0) io_w16(0x2118, r16(e + 6));
            }
        } else if (t == 0xFD) ok = nmi_dma(x, 0x1808, 0x0078, 1);
        else if (t == 0xFC) ok = nmi_dma(x, 0x1908, 0x0079, 0);
        else if (t == 0xFB) ok = nmi_dma(x, 0x1800, 0x007D, 1);
        else ok = nmi_dma(x, 0x1900, 0x007D, 0);
        if (!ok) { g.DMAQueueHead = x; NMI_Tick(); return; }
        x = (x + 8) & 0x03FF;
    }
    NMI_SendOAM(x);
}

/* The NMI: the queue head at x; the sprite table to OAM when it is ready and the budget has $26E
 * left; then NMI_Tick */
void NMI_SendOAM(uint16_t x)
{
    g.DMAQueueHead = x;
    if (r16(RAM_OFF_DMABudget) >= 0x026E && g.OAMReady[0]) {
        g.OAMReady[0] = 0;
        io_w8(0x4304, 0x7E);
        io_w16(0x4300, 0x0400);
        io_w16(0x4302, g.OAMShowBuf);
        io_w16(0x4305, 0x0220);
        io_w16(0x2102, 0);
        io_w8(0x420B, 0x01);
    }
    NMI_Tick();
}

/* The NMI's frame work: vcount (with the 50 Hz fraction's carry), a step of the fade (in after the
 * fadestep delay, out with $FFFF; not on the frames STAT77 bit 7 is set), FrameCount (unless held),
 * then the frame hook (FrameHookNext taken when set) */
void NMI_Tick(void)
{
    uint32_t f = (uint32_t)g.vcountfrac + g.vcountstep;
    g.vcountfrac = (uint16_t)f;
    uint32_t v = (uint32_t)g.vcount[0] + 1 + (f >> 16);
    g.vcount[0] = (uint16_t)v;
    if (v >> 16) g.vcount[1]++;
    uint16_t fs = g.fadestep;
    if (fs) {
        if (!(fs & 0x8000)) {
            if (fs != 1) g.fadestep--;
            else if (g.brightness >= 0x000F) { g.brightness = 0x000F; g.fadestep = 0; }
            else if (!(io_r16(0x213E) & 0x8000)) g.brightness++;
        } else if (fs != 0xFFFF) g.fadestep++;
        else if (!(io_r16(0x213E) & 0x8000)) {
            g.brightness--;
            if (!g.brightness || (g.brightness & 0x8000)) { g.brightness = 0; g.fadestep = 0; }
        }
    }
    if (!g.FrameCountHold) g.FrameCount++;
    nmi_b = (uint8_t)(g.FrameCountHold >> 8);
    if (g.FrameHookNext[1]) {
        nmi_b = g.FrameHookNext[1];
        w16(RAM_OFF_FrameHook, r16(RAM_OFF_FrameHookNext));
        w16(RAM_OFF_FrameHookNext, 0);
        g.FrameHook[2] = g.FrameHookNext[2];
    }
    call_long((uint32_t)g.FrameHook[2] << 16 | r16(RAM_OFF_FrameHook));
}

/* The NMI after the frame hook: the pads (Multi5Bios; b the accumulator's high byte the hook
 * leaves), SoundUpdate, the sound stream, NMISkip cleared */
void NMI_FrameHookReturn(uint8_t b)
{
    Multi5Bios(b);
    SoundUpdate();
    UpdateStreamSfx();
    g.NMISkip = 0;
}

/* Empty hooks and the reset stub */
void NullHook(void) {}

void IRQHook_Off(void) {}

void WaitFrames_Inc(uint16_t x, uint16_t y) { WaitFrames(x, (uint16_t)(y + 1)); }

/* Fade the screen in after a 5-frame delay, or out: BrightnessHook as the frame hook */
void FadeIn(void) { g.fadestep = 5; SetFrameHook(SYM_BrightnessHook); }

/* Set the frame hook back to the empty routine */
void ClearFrameHook(void) { SetFrameHook(SYM_NullHook); }

/* Fill the text map with the blank tile (textmap2 with charofs2; from row 12 down) */
void ClearTextMap(void) { MemFill(0x7E0000 + RAM_OFF_textmap, 0x0800, (uint16_t)(g.blankchar + g.charofs)); }

void ClearTextMap2(void) { MemFill(0x7E0000 + RAM_OFF_textmap2, 0x0800, (uint16_t)(g.blankchar + g.charofs2)); }

void ClearTextMapBottom(void) { MemFill(0x7E0000 + RAM_OFF_textmap + 0x300, 0x0500, (uint16_t)(g.blankchar + g.charofs)); }

/* Send the text map (all, or rows 1-12) or the second text map to video memory */
static void send_map(uint32_t off, uint16_t x, uint16_t y) { uint32_t p0 = 0x7E0000 + off; DMAToVRAM(&p0, x, y, 0); }

void SendTextMap(void) { send_map(RAM_OFF_textmap, 0x0800, 0x0000); }

void SendTextMap2(void) { send_map(RAM_OFF_textmap2, 0x0800, 0x0400); }

void SendTextMapRows(void) { send_map(RAM_OFF_textmap + 0x40, 0x0300, 0x0020); }

void SendTextMap2Rows(void) { send_map(RAM_OFF_textmap2 + 0x40, 0x0300, 0x0420); }

void FreeCacheEntryA(uint16_t a, uint16_t y) { FreeCacheEntry((uint16_t)((a >> 8 & 0xFF) << 1), y); }

/* Send the 256 colours of the colour buffer */
void SendColours(void) { DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf, 0x0100, 0); }

/* The IRQ and NMI vectors */
void IRQ(void) { IRQ_Fast(); }

/* (unused entries) DrawSprite with the frame at [p0]; the tile cache entry of the frame at [p0] or
 * of frame a of the sprite set at [p1], loading it when needed */
void GetFrameTiles_drawat(uint32_t p0, uint16_t a, uint16_t x, uint16_t y) { DrawSprite_draw(p0, a, x, y, x); }

/* (unused entry) The tile cache entry * 2 of the frame at [p0], found by its number with
 * FrameByNumber, else by its address; LoadFrameTiles when it is not cached */
uint16_t DrawSpritePieces_cacheat(uint32_t p0, uint16_t x)
{
    uint16_t y = g.FrameCacheCount;
    if (!y) return LoadFrameTiles(p0, x);
    if (!g.FrameByNumber) return FindCachedFrame(p0, 0, y);
    x = g.DrawFrameNum;
    uint16_t a = r16(RAM_OFF_FrameCacheOf + x);
    if (a & 0x8000) return LoadFrameTiles(p0, x);
    return (uint16_t)((a >> 8) << 1);
}

/* (unused entry) DrawSpritePieces_cacheat for frame a of the sprite set at [p1] (DrawFrameNum = a) */
uint16_t DrawSpritePieces_cacheframe(uint16_t a, uint32_t p1, uint16_t x)
{
    g.DrawFrameNum = a;
    uint32_t s = p1 & 0xFFFFFF;
    uint16_t k = (uint16_t)(a << 2);
    uint32_t lo = (uint32_t)m16(s + k) + (uint16_t)p1;
    uint16_t hi = (uint16_t)(m16(s + (uint16_t)(k + 2)) + (uint16_t)(p1 >> 16) + (lo >> 16));
    return DrawSpritePieces_cacheat((uint16_t)lo | (uint32_t)hi << 16, x);
}

/* Set the rink scroll from the camera position (horizontal mode has its own) */
void updatescroll(void)
{
    if (g.sfhormode) return;
    g.Hscroll = (int16_t)(g.Hpos - 0x80);
    g.Vscroll = (int16_t)((uint16_t)-g.Vpos - 0x70);
}

/* Project rink position d0 / d1 (height d2) to the screen: d0 = x, d1 = y ($4E20 off the screen) */
void find3d(uint16_t *d0, uint16_t *d1, uint16_t d2)
{
    uint16_t x = *d0, y = *d1;
    if (g.sfhormode) { uint16_t t = x; x = (uint16_t)-y; y = (uint16_t)(t - 0xC5); }
    else { x = (uint16_t)(x - g.Hpos); y = (uint16_t)(y - g.Vpos); }
    *d0 = x;
    if (((uint16_t)(x - 0xFF6F) & 0x8000) || !((uint16_t)(x - 0x91) & 0x8000)) { *d1 = 0x4E20; return; }
    *d0 = (uint16_t)(x + 0x80);
    uint16_t h = (uint16_t)(d2 * 3);
    h = (uint16_t)((h & 0x8000) | h >> 1);              /* (asl then ror: the sign kept) */
    y = (uint16_t)(h + y);
    *d1 = y;
    if (((uint16_t)(y - 0xFF6F) & 0x8000) || !((uint16_t)(y - 0x91) & 0x8000)) { *d1 = 0x4E20; return; }
    *d1 = (uint16_t)(0x70 - y);
}

/* Queue the rink map rows that the vertical scroll brought on screen (at most $1D: the whole
 * screen) into the BG2 ring of rows, and set BG2's scroll */
void updatescroll_rows(void)
{
    if (g.sfhormode) return;
    uint16_t x = (uint16_t)((uint16_t)(g.Vscroll + 0x170) >> 3);
    uint16_t a = (uint16_t)(x - g.Oldrow);
    if (a) {
        uint16_t tmp1, tmp2;
        g.Oldrow = x;
        if (a & 0x8000) {
            a = (uint16_t)-a;
            if (a >= 0x19) goto all;
            tmp1 = a;
            g.vramrow[0] = (uint16_t)(g.vramrow[0] - tmp1) & 0x1F;
            tmp2 = g.vramrow[0];
        } else {
            if (a >= 0x19) goto all;
            tmp1 = a;
            uint32_t s = (uint32_t)a + g.vramrow[0];
            g.vramrow[0] = (uint16_t)s & 0x1F;
            tmp2 = (uint16_t)(g.vramrow[0] + 0x1D + (s >> 16) - tmp1) & 0x1F;
            x = (uint16_t)(x + 0x1D - tmp1);
        }
        if (0) {
        all:
            tmp1 = 0x1D;
            g.vramrow[0] = 0;
            tmp2 = 0;
        }
        uint16_t t5 = (uint16_t)(x << 5);
        uint32_t s = (uint32_t)(uint16_t)(t5 << 1) + t5 + (t5 >> 15);   /* (the carries of asl, adc, adc) */
        s = (s & 0xFFFF) + (uint16_t)rinkmapbuf + (s >> 16);
        uint16_t tmp0 = (uint16_t)((s & 0xFFFF) + 6 + (s >> 16));
        tmp2 = (uint16_t)((uint16_t)(tmp2 << 5) + 0x400 + (tmp2 >> 11 & 1));
        uint16_t r1 = tmp1;
        do {
            uint32_t p0 = (rinkmapbuf & 0xFF0000) | tmp0;
            DMAToVRAM(&p0, 0x40, tmp2, 0);
            p0 = (p0 & 0xFFFF0000u) | (uint16_t)(tmp0 + 0x40);
            DMAToVRAM(&p0, 0x20, (uint16_t)(tmp2 + 0x400), 0);
            tmp0 = (uint16_t)(tmp0 + 0x60);
            tmp2 = (uint16_t)(((tmp2 + 0x20) & 0x3FF) | 0x400);
        } while (--r1);
    }
    g.BG2HScroll = (uint16_t)(g.Hscroll + 0xC0);
    g.BG2VScroll = (uint16_t)((uint16_t)(g.vramrow[0] << 3) | (g.Vscroll & 7));
}

/* While the hat trick is on (not on the horizontal rink or in a replay): the ten hats, each launched at
 * random (a chance in 32 a frame) on an arc onto the ice, animated, moved timestep frames and drawn with
 * its shadow */
void UpdateHatTrick(void)
{
    if (g.sfhormode || (g.sflags & 0x0800) || !g.hattrick) return;
    for (uint16_t x = 0x12; !(x & 0x8000); x -= 2) {
        if (!r16(RAM_OFF_hatstep + x)) {
            if (RandomWord() & 0x07C0) continue;
            uint16_t r = RandomWord() & 0x3F;
            uint16_t d0 = (uint16_t)((uint16_t)((r >> 1) + r + (r & 1) + 0x50) << 4);   /* (lsr's carry) */
            uint16_t d1 = (uint16_t)((uint16_t)((RandomWord() & 0x3F) + 0x60) << 4);
            w16(RAM_OFF_haty + x, (uint16_t)((RandomWord() & 0xFF) << 4));
            uint16_t d2 = (uint16_t)((RandomWord() & 0x3F) + 0x78);
            w16(RAM_OFF_hatvy + x, DivX8S((uint16_t)(d1 - r16(RAM_OFF_haty + x)), d2));
            uint16_t hx = d0 & 0x0100 ? 0x1100 : 0xFF00;
            w16(RAM_OFF_hatx + x, hx);
            w16(RAM_OFF_hatvx + x, DivX8S((uint16_t)(d0 - hx), d2));
            uint16_t p = Mul8U((uint16_t)(d2 >> 2), (uint16_t)(d2 >> 2));
            w16(RAM_OFF_hatz + x, (uint16_t)((uint16_t)(p << 1) + p + (p >> 15)));
            uint16_t kind = (uint16_t)(RandomWord() >> 2 & 3);
            if (kind) kind--;
            w16(RAM_OFF_hatkind + x, (uint16_t)(kind * 3));
            g.hatvz[0] = 0;
            w16(RAM_OFF_hatstep + x, 1);
            w16(RAM_OFF_hattimer + x, rom16(SYM_UpdateHatTrick_9 + x));
        } else {
            uint16_t t = (uint16_t)(r16(RAM_OFF_hattimer + x) - g.timestep);
            if (t & 0x8000) {
                w16(RAM_OFF_hattimer + x, (uint16_t)(t + rom16(SYM_UpdateHatTrick_9 + x)));
                uint16_t s = (uint16_t)(r16(RAM_OFF_hatstep + x) + 1);
                w16(RAM_OFF_hatstep + x, s == 4 ? 1 : s);
            } else w16(RAM_OFF_hattimer + x, t);
            for (uint16_t y = g.timestep; y; y--) hat_move(x);
        }
        uint16_t f = (uint16_t)(r16(RAM_OFF_hatkind + x) + r16(RAM_OFF_hatstep + x) + 967 - 2);
        uint16_t hy = r16(RAM_OFF_haty + x), hx = r16(RAM_OFF_hatx + x);
        DrawSprite(0x2000, asr4(hx), asr4((uint16_t)(hy - (r16(RAM_OFF_hatz + x) >> 4))), f, SYM_SpriteFrames);
        DrawSprite(0x2000, asr4(r16(RAM_OFF_hatx + x)), asr4(r16(RAM_OFF_haty + x)), 976 - 1, SYM_SpriteFrames);
    }
}

/* Horizontal rink: both team blocks (home at column $14, visitors at 3, row 2) and their palette rows */
void DrawHorTeams(void)
{
    g.horblocks = 1;
    g.printx = 0x14;
    g.printy = 2;
    DrawTeamBlocks(g.HomeTeam, 0, 0);
    g.printx = 3;
    DrawTeamBlocks(g.VisTeam[0], 0, 2);
    g.horblocks = 0;
    g.redrawblocks = 0;
    g.colourbuf[0xE1] = 0x1D47;
    g.colourbuf[0xF1] = 0x1D47;
    if (!g.pausemenuon) {
        g.colourbuf[0xEF] = 0xFFFF;
        g.colourbuf[0xFF] = 0xFFFF;
    }
    DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf + 0xC0, 0x20, 0x60);
    WaitFrame();
    DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf + 0x1C0, 0x20, 0xE0);
    SendTextMap();
}
