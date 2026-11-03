/* video: the score, clock, crowd and sort coordinates (src/routines/video). */
#include "game.h"

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
