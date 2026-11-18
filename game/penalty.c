/* penalty: penalties and the penalty boxes (src/routines/penalty). The scoreboard and team stats are scoreboard.c. */
#include "game.h"

/* Remove the box list entry before index a0 by moving the rest down to the end marker (bit 7); returns a0
 * stepped back */
uint16_t RemovePlayerFromList(uint16_t a0)
{
    uint8_t v;
    uint16_t d2 = 0;
    do {
        uint16_t y = (uint16_t)(a0 + d2++);
        v = r8(RAM_OFF_tmboxlist + y);
        w8(RAM_OFF_tmboxlist - 1 + y, v);
    } while (!(v & 0x80));
    return (uint16_t)(a0 - 1);
}

/* Every game loop: the penalty box times, new calls, the referee */
void PenaltyManager(void)
{
    updatepentime();
    checkfornewpen();
    chkprogress();
    UpdatePA();
}

/* Go through PenBuf's new calls (not on the horizontal rink): a penalty that stops play does so (Stop4Pen)
 * unless its team's opponent has the puck (a delayed call: the referee's arm, once); once seen (bit 7 of
 * its player byte) a call sets the fight count down from its record */
void checkfornewpen(void)
{
    if (g.sfhormode) return;
    uint32_t pb = SYM_Penaltylist & 0xFF0000;
    uint16_t a0 = PenBuf;
    for (;;) {
        uint16_t w = r16(a0) & 0xFF;
        a0 += 2;
        if (w == 0x34) continue;
        if (!w) return;
        if (!(g.gmode & gmpen)) {
            uint16_t a1 = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + w));
            if (m8(pb | a1)) {
                if (g.puckc < 0) goto delayed;
                uint16_t d0 = (uint16_t)(((uint16_t)g.puckc >> 1) - 6);
                uint16_t p = r8(a0 - 1);
                if (p & 0x80) p |= 0xFF00;
                if ((uint16_t)((uint16_t)(p - 6) ^ d0) & 0x8000) goto delayed;
            }
            Stop4Pen(a0);
        }
        {
            uint8_t b = r8(a0 - 1);
            w8(a0 - 1, b | 0x80);
            if (b & 0x80) continue;
            uint16_t d0 = r8(a0 - 2);
            uint16_t a1 = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + d0));
            uint16_t d1 = (uint16_t)(m8((pb | a1) + 1) << 5);
            uint16_t d = (uint16_t)(d1 - (uint16_t)g.PenCntDwn);
            if (d && !(d & 0x8000)) g.PenCntDwn = (int16_t)d1;
        }
        continue;
    delayed:
        if (g.gmode & 0x0008) continue;
        g.gmode |= 0x0008;
        SetPA(0x30);
    }
}

/* Stop play for the penalty before a0 in PenBuf (unless the clock was already stopped): the faceoff spot
 * (none for a goal; the last touch for offside passes; else the puck, at the end of the icing team's goal
 * for icing) kept inside the faceoff circles; then the replay, the whistle and the referee */
void Stop4Pen(uint16_t a0)
{
    uint16_t c = g.gmclock;
    g.gmclock = 0xFFFF;
    if (!c) {
        uint16_t d0 = 0, d1 = 0;
        uint16_t pen = r8(a0 - 2);
        if (pen != PenGoal) {
            d0 = g.ltx;
            d1 = r16(RAM_OFF_lty);
            if (pen != PenOOP) {
                d0 = (uint16_t)g.Xpos[14];
                d1 = (uint16_t)g.pucky[0];
                if (pen == PenIcing) {
                    d1 = 0x0258;
                    if (r16(RAM_OFF_pfgoal + (uint16_t)(r8(a0 - 1) << 1))) d1 = 0xFDA8;
                }
            }
        }
        uint16_t tmp0 = d0;
        if (!((uint16_t)(d0 - 0x46) & 0x8000)) d0 = 0x46;
        if ((uint16_t)(d0 - 0xFFBA) & 0x8000) d0 = 0xFFBA;
        if (!((uint16_t)(d1 - 0xA6) & 0x8000)) { d1 = 0xCE; d0 = tmp0 & 0x8000 ? 0xFFBA : 0x46; }
        if ((uint16_t)(d1 - 0xFF5A) & 0x8000) { d1 = 0xFF32; d0 = tmp0 & 0x8000 ? 0xFFBA : 0x46; }
        g.fox = d0;
        g.foy = d1;
        limitfo();
    }
    if (!g.replaydelay) g.replaydelay = 0x1E;
    g.SfxVelocity = 0x7000;
    QueueSound(0x15);
    g.PenCntDwn = 0;
    g.gmode |= gmpen;
    SetPA(0x0A);
}

/* Send the player of the new penalty entry before a0 to the box: the team's penalties and minutes, the
 * player's minutes, the penalty log, his box time ($2000 | minutes * 60, keeping bit $1000), his box list
 * slot and the box assignment; then the referee and the scoreboard */
static void penalty_to_box(uint16_t a0, uint16_t d0, uint16_t d2)
{
    uint16_t x = (uint16_t)((r16(a0) >> 8 & 0xFF) << 1);
    int o = x >> 1;
    uint16_t flag = 0, a2 = 0;
    if (g.pfteam[o]) {
        flag = 0x8000;
        a2 = 2;
        uint16_t c = (uint16_t)(g.CwdExciteLvl + 0x14);
        if (!(c & 0x8000)) g.CwdExciteLvl = c;
    }
    w16(RAM_OFF_tmPenalties + a2, (uint16_t)(r16(RAM_OFF_tmPenalties + a2) + 1));
    w16(RAM_OFF_tmPenmin + a2, (uint16_t)(r16(RAM_OFF_tmPenmin + a2) + d2));
    g.TempPlOffset = (g.pnum[o] & 0xFF) | flag;
    uint32_t pm = RAM_OFF_ppenmin + (uint16_t)((a2 ? 0x1A : 0) + g.pnum[o]);
    w8(pm, (uint8_t)(r8(pm) + d2));
    if (g.PenSumLength < 0x41) {
        uint16_t y = (uint16_t)(g.PenSumLength << 2);
        w16(RAM_OFF_PenSum + y, GetPeriodTimeRemaining());
        uint16_t p = g.TempPlOffset;
        if (!((uint16_t)(p - 0x80) & 0x8000)) p |= 0x80;
        w8(RAM_OFF_PenSum + y + 2, (uint8_t)p);
        w8(RAM_OFF_PenSum + y + 3, r8(a0));
        g.PenSumLength++;
    }
    uint16_t pl = g.TempPlOffset & 0xFF;
    if (pl & 0x80) pl |= 0xFF00;
    pl = (uint16_t)(pl << 1);
    uint16_t t = Mul8U(d2 & 0xFF, 0x3C) | 0x2000;
    uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + pl);
    uint16_t old = r16(RAM_OFF_tmpdst + y);
    if (!(old & 0x8000)) t |= old & 0x1000;
    w16(RAM_OFF_tmpdst + y, t);
    uint16_t b = a2 ? 0x1A : 0, d1 = 0x18;
    do y = b++; while (!(r8(RAM_OFF_tmboxlist + y) & 0x80) && !((d1 = (uint16_t)(d1 - 1)) & 0x8000));
    w16(RAM_OFF_tmboxlist + y, pl | 0xFF00);
    assreplace(o, apenalty);
    SetPA(d0);
    USBoard();
}

/* The referee's work on the last PenBuf entry (a0 just past the first one): a call already seen stays
 * unless its player has left the ice; a new one (bit 15) sends its player to the box, starts a penalty
 * shot or only shows its message (the stars first for the end of the game). Then the coincidental
 * penalties. 1: look again (an entry without minutes on the horizontal rink). */
static int InProgress(uint16_t a0)
{
    uint16_t w;
    do { w = r16(a0); a0 += 2; } while (w);
    a0 -= 4;
    uint16_t e = r16(a0);
    w16(a0, e & 0x7FFF);
    if (!(e & 0x8000)) {
        if (r16(RAM_OFF_position + (uint16_t)((e >> 8 & 0x7F) << 1)) & 0x8000) w16(a0, 0);
        InProgress_coin();
        return 0;
    }
    uint16_t d0 = e & 0xFF;
    uint16_t a1 = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + d0));
    if (a1 == 0xD4AD) StartPenaltyShot();
    else {
        if (a1 == 0xD434) ShowThreeStars();
        uint16_t d2 = m8((SYM_Penaltylist & 0xFF0000) | a1);
        if (d2) {
            penalty_to_box(a0, d0, d2);
            InProgress_coin();
            return 0;
        }
    }
    w16(a0, 0);
    if (g.sfhormode) return 1;
    SetPA(d0);
    InProgress_coin();
    return 0;
}

/* Any penalty left in PenBuf: the referee's work on it (InProgress), else the end of the penalty mode */
static void chkprogress_find(void)
{
    for (;;) {
        uint16_t a0 = PenBuf, w;
        do { w = r16(a0) & 0xFF; a0 += 2; } while (w == 0x34);
        if (!w) { InProgress_exit(); return; }
        if (!InProgress(a0)) return;
    }
}

/* During a penalty: the fight count down; when it runs out a call with a message (minutes, not 1) turns
 * to the horizontal rink with the referee; else, the players in the box seated, once the referee is done
 * the next penalty */
void chkprogress(void)
{
    if (!(g.gmode & gmpen)) return;
    if (g.PenCntDwn < 0) {
        SeatPenaltyBox();
        if (g.RefCnt >= 0 || g.RefStep >= 0) return;
        chkprogress_find();
        return;
    }
    g.PenCntDwn = (int16_t)(g.PenCntDwn - g.d7);
    if (g.PenCntDwn >= 0) return;
    g.gmode &= (uint16_t)~0x0008;
    uint32_t pb = SYM_Penaltylist & 0xFF0000;
    uint16_t a0 = PenBuf;
    for (;;) {
        uint16_t w = r16(a0) & 0xFF;
        a0 += 2;
        if (w == 0x34) continue;
        if (!w) return;
        uint8_t m = m8(pb | (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + w)));
        if (m && m != 1) break;
    }
    g.sflags |= 0x0400;
    if (g.brightness) {
        forcefade();
        WaitFadeOut();
    }
    SetHor();
    g.puckc = (int16_t)(g.puckc | 0xFF00);
    assinsert(14, pnothing);
    g.temp1[14] = 0x1C20;
    g.RefStep = (int16_t)(g.RefStep | 0xFF00);
    g.RefCnt = 0;
    UpdatePA();
    g.RefCnt = 0x32;
    PushRef(0);
    FadeIn();
    ScreenOn();
}

/* The penalties are over: end the penalty mode, the puck to its faceoff assignment, then InProgress_coin */
void InProgress_exit(void)
{
    g.gmode &= ~gmpen;
    assreplace(14, pfaceoff);
    InProgress_coin();
}

/* The end of the referee's work: unless a penalty shot is on, count both teams' players on the ice */
void InProgress_coin(void)
{
    if (g.penshot) return;
    InProgress_ap(0);
    InProgress_ap(2);
}

/* Count team a2's players kept off the ice by penalties (the penalty flag $2000 cleared on the way) to set
 * how many it has on the ice (at least 4) */
void InProgress_ap(uint16_t a2)
{
    uint16_t d1 = 6;
    for (int d0 = 0x32; d0 >= 0; d0 -= 2) {
        uint32_t y = RAM_OFF_tmpdst + (uint16_t)((a2 ? 0x34 : 0) + d0);
        uint16_t v = r16(y);
        if ((v & 0x8000) || !v) continue;
        v &= 0xDFFF;
        w16(y, v);
        if (v & (TMPDINJ | 0x4000)) continue;
        if (d1 != 4) d1--;
    }
    w16(RAM_OFF_tmap + a2, d1);
}

/* Call penalty d0 on player o, unless the clock is stopped, a penalty shot is on, or penalties are off
 * (icing and the penalty shot always; offsides only with offsides on) */
void AddPenalty(int o, uint16_t d0)
{
    if (g.gmclock || g.PenShotActive) return;
    if (d0 != PenIcing && d0 != PenShot) {
        if (!g.OptPen) return;
        if (!(g.gmode & gmoffs) && d0 == PenOffsides) return;
    }
    AddPenalty2(o, d0);
}

/* Call penalty d0 on player o without the checks: the crowd reacts (home penalties of PenGoal and up), and
 * the first free PenBuf word (low byte 0; $34 entries are passed over) gets (o, d0), taken back if the
 * penalty has minutes and he already has one (pflags $4000) */
void AddPenalty2(int o, uint16_t d0)
{
    if (g.sfhormode) return;
    if (!((uint16_t)(d0 - PenGoal) & 0x8000)) {
        g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0xC8);
        g.crowdhold = 0x2D;
        if (g.pfteam[o]) g.Crowdlevel = g.Crowdlevel >= 0xC8 ? (uint16_t)(g.Crowdlevel - 0xC8) : 0;
    }
    uint16_t a1 = PenBuf, d1 = 0x1F;
    for (;;) {
        uint16_t w = r16(a1) & 0xFF;
        a1 += 2;
        if (w == 0x34) continue;
        if (!w) break;
        if ((d1 = (uint16_t)(d1 - 1)) & 0x8000) return;
    }
    a1 -= 2;
    w16(a1, (uint16_t)(((o & 0xFF) << 8) | d0));
    uint16_t rec = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + d0));
    if (!m8((SYM_Penaltylist & 0xFF0000) | rec)) return;
    uint16_t p = g.pflags[o];
    g.pflags[o] |= 0x4000;
    if (p & 0x4000) w16(a1, 0);
}

/* A word through the data bank (the game's code bank, $9F: only its low 8 KB is WRAM) */
#define DATA_BANK 0x9F0000u
static uint16_t db_r16(uint16_t a) { return m16(DATA_BANK | a); }

/* (PenGoalStuff's store through the data bank: only the bytes in its WRAM, below $2000, are written) */
static void db_w16(uint16_t a, uint16_t v)
{
    if (a < 0x2000) w8(a, (uint8_t)v);
    if ((uint16_t)(a + 1) < 0x2000) w8((uint16_t)(a + 1), (uint8_t)(v >> 8));
}

/* After a goal by team a2 on team a1: PenBuf's entries without minutes or not for the scoring team become
 * $34 (period 3 clears it), then, when the scoring team had more players on the ice, the first scored-on
 * player in the box whose penalty is not coincidental comes out. (The PenBuf walk leaves a1 at the last
 * penalty record it looked up: the release then goes by that address.) */
void PenGoalStuff(uint16_t a1, uint16_t a2)
{
    if (g.gsp == 3) ClearPenaltyBuffer();
    else {
        uint16_t a0 = PenBuf;
        for (;;) {
            uint16_t w = r16(a0) & 0xFF;
            a0 += 2;
            if (w == 0x34) continue;
            if (!w) break;
            a1 = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + w));
            if (m8((SYM_Penaltylist & 0xFF0000) | a1)) {
                uint16_t d0 = (r16((uint16_t)(a0 - 2)) & 0xFF00) >= 0x0600 ? 2 : 0;
                if (a2 == d0) continue;
            }
            w16((uint16_t)(a0 - 2), 0x34);
        }
    }
    uint16_t d = (uint16_t)(db_r16((uint16_t)(RAM_OFF_tmap + a2)) - db_r16((uint16_t)(RAM_OFF_tmap + a1)));
    if (!d || (d & 0x8000)) return;
    uint16_t a0 = a1 ? 0x1A : 0;
    for (;;) {
        uint16_t e = r8(RAM_OFF_tmboxlist + a0++);
        if (e & 0x80) return;
        uint16_t y = (uint16_t)((a1 ? 0x34 : 0) + e);
        if (r16(RAM_OFF_tmpdst + y) & 0x4000) continue;
        w16(RAM_OFF_tmpdst + y, 0);
        RemovePlayerFromList(a0);
        db_w16((uint16_t)(RAM_OFF_tmap + a1), (uint16_t)(db_r16((uint16_t)(RAM_OFF_tmap + a1)) + 1));
        w16(RAM_OFF_tmPwrGoals + a2, (uint16_t)(r16(RAM_OFF_tmPwrGoals + a2) + 1));
        g.tmflags[0] |= 1;
        g.tmflags[1] |= 1;
        return;
    }
}

/* The power play time left (d0), from the box times: team a2 on the power play, a3 the shorthanded team */
uint16_t GetLowestPen(uint16_t a2, uint16_t a3)
{
    uint16_t d0 = 0, d3 = 0, a0 = a2 ? 0x1A : 0;
    for (;;) {
        uint16_t e = r8(RAM_OFF_tmboxlist + a0++);
        if (e & 0x80) break;
        uint16_t v = r16(RAM_OFF_tmpdst + (uint16_t)((a2 ? 0x34 : 0) + e));
        if (v & 0x4000) continue;
        d0 = (uint16_t)(d0 + (uint16_t)(v - d3));
        d3 = v;
    }
    if (r16(RAM_OFF_tmap + a3) == 6) return d0;
    d0 = (uint16_t)(d0 - d3);
    a0 = a3 ? 0x1A : 0;
    for (;;) {
        uint16_t e = r8(RAM_OFF_tmboxlist + a0++);
        if (e & 0x80) return d0;
        uint16_t v = r16(RAM_OFF_tmpdst + (uint16_t)((a3 ? 0x34 : 0) + e));
        if (v & 0x4000) continue;
        if ((uint16_t)(d0 - v) & 0x8000) return d0;
        return (uint16_t)(d0 + d3);
    }
}

/* The penalty box y spot of player o (r2): +-11 by team times (his team's players in the box, at most 2,
 * plus 3) */
uint32_t PenBoxSpot(int o)
{
    uint16_t d0 = 0x000B, d1 = g.PBnum;
    if (!g.pfteam[o]) { d1 >>= 4; d0 = 0xFFF5; }
    d1 &= 0xF;
    if (d1 >= 3) d1 = 2;
    return Mul16S(d0, (uint16_t)(d1 + 3));
}

/* Put every player in the penalty box (negative position) at his box spot on the sideline, with his box
 * assignment (not in a penalty shot, its set-up or a shootout) */
void SeatPenaltyBox(void)
{
    if (g.penshot | g.PenShotActive | g.shootoutmode) return;
    g.PBnum = 0;
    for (int o = 0; o < 12; o++) {
        if (!(g.position[o] & 0x8000)) continue;
        g.Ypos[o] = (int16_t)PenBoxSpot(o);
        g.Xpos[o] = sideline;
        assdopen(o);
    }
}

/* Release player d0 of team a2 when his box time has run out (below 6 and zero) */
static void pentime_release(uint16_t a2, uint16_t d0)
{
    uint16_t v = r16(RAM_OFF_tmpdst + (uint16_t)((a2 ? 0x34 : 0) + d0));
    if (!((uint16_t)(v - 6) & 0x8000)) return;
    if (v) return;
    releasepl(a2, d0);
}

/* One second of team a2's box list: the first two players with a running clock count down (the others
 * wait), a clock at 0 leaves the list; an untimed one (bit $4000) counts down too and, with bit $0800, ends
 * the team's pass ($1000 left when it had no bit $1000, else 0). Then the players whose time is up go out. */
static void pentime_team(uint16_t a2)
{
    uint16_t a0 = a2 ? 0x1A : 0;
    uint16_t a1 = RAM_OFF_mesarea;
    uint16_t d1 = 2;
    for (;;) {
        uint16_t d0 = r8(RAM_OFF_tmboxlist + a0++);
        if (d0 & 0x80) break;
        uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + d0);
        uint16_t v = r16(RAM_OFF_tmpdst + y);
        if (v & 0x4000) {
            v--;
            w16(RAM_OFF_tmpdst + y, v);
            if (!(v & 0x0800)) continue;
            w16(RAM_OFF_tmpdst + y, v & 0x1000 ? 0 : 0x1000);
            RemovePlayerFromList(a0);
            return;
        }
        d1--;
        if (d1 & 0x8000) continue;
        w16(a1, d0);
        a1 += 2;
        w16(RAM_OFF_tmpdst + y, (uint16_t)(v - 1));
        if (v == 1) a0 = RemovePlayerFromList(a0);
    }
    if (d1 == 1) { pentime_release(a2, g.mesarea[0]); return; }
    if (d1 == 0) pentime_release(a2, g.mesarea[0]);
    else if (d1 != 0xFFFF) return;
    pentime_release(a2, g.mesarea[1]);
}

/* Once a second of running clock (not in a penalty shot): the attack zone and power play times, then both
 * teams' penalty box clocks; sflags3 bit $20 marks the second */
void updatepentime(void)
{
    g.sflags3[0] &= (uint16_t)~0x0020;
    if (g.gmclock || g.PenShotActive) return;
    uint16_t t = g.Penaltytimer;
    uint16_t v = (uint16_t)(t - g.d7);
    g.Penaltytimer = v;
    if (!(v & 0x8000)) return;
    g.Penaltytimer = (uint16_t)(v + 0x18 + (t >= g.d7));   /* (sbc's carry) */
    g.sflags3[0] |= 0x0020;
    chkatop();
    updatePPTeamTime();
    pentime_team(0);
    pentime_team(2);
}

/* A = -1 when the player hit (a2) is the puck carrier on a breakaway (outside highlight mode; the breakaway
 * flag checked again): a penalty shot instead of the penalty; else 0 */
uint16_t PenShotChk(uint16_t a2)
{
    if (g.gmode & 0x0010) return 0;
    if (a2 != (uint16_t)g.puckc) return 0;
    if (!g.breakaway) return 0;
    CheckBreakaway(0);
    g.breakaway = g.IsBreakaway;
    return g.breakaway ? 0xFFFF : 0;
}

/* Player d0 / 2 of team a2 is out of the box: the team's first object with no position takes his place on
 * the ice (the next position of priolist, the goalie-pulled list when the goalie is out); returns him */
int releasepl(uint16_t a2, uint16_t d0)
{
    uint16_t x = (uint16_t)(r16(RAM_OFF_tmsort + a2) - 2);
    do x += 2; while (!(r16(RAM_OFF_position + x) & 0x8000));
    int o = x >> 1;
    uint16_t d3 = d0 >> 1;
    uint16_t d1 = r16(RAM_OFF_tmap + a2);
    w16(RAM_OFF_tmap + a2, (uint16_t)(d1 + 1));
    g.tmflags[0] |= 1;
    g.tmflags[1] |= 1;
    uint32_t a0 = SYM_priolist;
    if (r16(RAM_OFF_tmgoalie + a2) & 0x8000) a0++;
    g.position[o] = m8(a0 + d1);
    Setplass(o);
    setplayer(o, d3);
    g.pflags[o] |= 0x1000;
    return o;
}

/* Clear the list of called penalties */
void ClearPenaltyBuffer(void)
{
    for (int i = 0; i < 32; i++) w16(PenBuf + 2 * i, 0);
}

/* Print the message for penalty d0 (negative: only clear it): on the vertical rink in a frame sized to it,
 * on the horizontal one in the message area. On the vertical rink a box still up from the last message is
 * erased first, which leaves d0 at $0E: then penalty $0E's message is printed whatever d0 was. */
void prefmes(uint16_t d0)
{
    uint32_t pb = SYM_Penaltylist & 0xFF0000;
    if (g.sfhormode) {
        ClearPenaltyMessage();
        if (d0 & 0x8000) return;
        print(SYM_prefmesPos3);
        uint16_t a1 = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + d0) + 2);
        g.printx = (uint16_t)(g.printx - (m16(pb | a1) >> 1));
        print(pb | a1);
        return;
    }
    if (g.penmsgbox) {
        print(SYM_prefmesPos);
        eraser(0x0E, 3, g.blankchar);
        g.penmsgbox = 0;
        d0 = 0x0E;                                      /* (the erase's width is left in d0) */
    }
    if (d0 & 0x8000) return;
    print(SYM_prefmesPos2);
    /* (penmsgbox is 0 here: the skips it would allow for penalties 6, 8, $0C and $12 never happen) */
    uint16_t a1 = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + d0) + 2);
    uint16_t n = (uint16_t)(m16(pb | a1) - 2);
    if (!n) return;
    n = (uint16_t)(n + 1) >> 1;
    g.printx = (uint16_t)(g.printx - n);
    if (g.printx & 0x8000) g.printx = 0;
    uint16_t w = m16(pb | a1);
    if (!m8((pb | (uint16_t)(a1 - 1)) + w)) w--;
    g.penmsgbox = 1;
    Framer(w, 3);
    g.printy -= 2;
    g.printx++;
    print(pb | a1);
}

/* Clear the penalty message area on the horizontal rink (a 24 x 3 frame) */
void ClearPenaltyMessage(void)
{
    print(SYM_ClearPenaltyMessagePos);
    Framer(0x18, 3);
    print(SYM_ClearPenaltyMessagePos2);
}

/* Print string d0 of the string list a1 (each string starts with its length word); returns a1 past it */
uint32_t Fprint(uint16_t d0, uint32_t a1)
{
    for (; d0; d0--) a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1));
    return print(a1);
}

/* The referee animation's next step when its count runs out; when the message time runs out, the message
 * line cleared and the penalized player's name printed there, centred */
void UpdatePA(void)
{
    if (g.RefCnt < 0) return;
    g.RefCnt = (int16_t)(g.RefCnt - g.d7);
    if (g.RefCnt < 0) SetPA2();
    g.penmsgtimer = (uint16_t)(g.penmsgtimer - g.d7);
    if (!(g.penmsgtimer & 0x8000)) return;
    g.penmsgtimer = 0x7FFF;
    ClearPenaltyMessage();
    print(SYM_UpdatePAPos);
    uint32_t a1 = GetPlayerName();
    g.printx = (uint16_t)(g.printx - (m16(a1) >> 1));
    print(a1);
}

/* Start the referee animation of penalty d0 and its message (the goal box for a goal), the message time
 * long ($3C frames on the horizontal rink); its first step */
void SetPA(uint16_t d0)
{
    g.RefPen = d0;
    g.RefStep = 0;
    prefmes(d0);
    if (d0 == PenGoal) ShowGoalBox();
    g.penmsgtimer = g.sfhormode ? 0x3C : 0x7FFF;
    SetPA2();
}

/* Step the referee animation of penalty RefPen: its next (delay, frame) pair from the animation the
 * penalty's record points to (a negative pair is the last: RefStep -1); RefCnt = delay * 8, the frame
 * drawn. Once it is done, a shootout's turn stops. */
void SetPA2(void)
{
    if (g.RefStep & 0x8000) {
        if (g.shootoutmode) g.shootoutstop = 1;
        return;
    }
    uint16_t d0 = (uint16_t)g.RefStep;
    g.RefStep = (int16_t)(g.RefStep + 2);
    uint16_t x = (uint16_t)(m16(SYM_Penaltylist + g.RefPen) + 2);
    uint32_t s = (uint32_t)x + m16(SYM_Penaltylist + x);
    x = (uint16_t)(s + d0 + (s >> 16));   /* (the first add's carry) */
    d0 = m16(SYM_Penaltylist + x);
    if (d0 & 0x8000) { d0 = (uint16_t)-d0; g.RefStep = -1; }
    g.RefCnt = (int16_t)((d0 & 0xFF) << 3);
    PushRef(d0 >> 8);
}

/* Draw referee frame d0 (8 rows of 7 tiles from RefsMap, RefsMapHor on the horizontal rink) in the referee
 * window; frame $40 erases the window and clears the message */
void PushRef(uint16_t d0)
{
    if (d0 == 0x40) {
        g.printx = 2;
        g.printy = 2;
        if (g.sfhormode) { g.printx = 0x0D; g.printy = 0x0F; }
        eraser(7, 8, g.blankchar);
        g.sflags |= 0x0200;
        prefmes(0xFFFF);
        g.refwindow = 0;
        return;
    }
    if (d0 != 6) g.refwindow = 1;
    uint16_t off = Mul8U(d0, 0x70);
    uint32_t a0 = SYM_RefsMap;
    uint16_t tmp1 = 0x84, tmp0 = g.refchars[0] | 0x2000;
    if (g.sfhormode) {
        tmp0 = g.refcharshor;
        tmp1 = 0x03DA;
        a0 = SYM_RefsMapHor + 6;
    }
    a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + off);
    for (int row = 0; row < 8; row++) {
        for (int k = 0; k < 7; k++) {
            uint32_t s = (uint32_t)m16(a0) + tmp0;
            uint16_t v = (uint16_t)(s + g.charofs + (s >> 16));   /* (the first add's carry) */
            w16(RAM_OFF_textmap + tmp1, v);
            uint16_t w = tmp1 >> 1;
            if (w < g.textdirtylo) g.textdirtylo = w;
            if (w >= g.textdirtyhi) g.textdirtyhi = w;
            a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + 2);
            tmp1 += 2;
        }
        tmp1 += 0x32;
    }
    g.sflags |= 0x0200;
}

/* Move printx left by the width of string a1 in the big font after its first character (one column for a 1,
 * else two) */
void AlignBigNumber(uint32_t a1)
{
    uint16_t y = 2;
    do {
        g.printx--;
        if (m8(a1 + y) != 0x31) g.printx--;
        y++;
    } while (y < m16(a1));
}

/* Team a2's penalty box lines: the players with a running penalty clock first, then the others */
void DispPen(uint16_t a2)
{
    for (int pass = 0; pass < 2; pass++) {
        uint16_t a0 = a2 ? 0x1A : 0;
        for (;;) {
            uint16_t d0 = r8(RAM_OFF_tmboxlist + a0++);
            if (d0 & 0x80) break;
            uint16_t untimed = r16(RAM_OFF_tmpdst + (uint16_t)((a2 ? 0x34 : 0) + d0)) & 0x4000;
            if (!untimed == !pass) pplpen(a2, d0);
        }
    }
}

/* Keep the faceoff spot away from the blue line for every called penalty (from PenBuf's first word, zero
 * or not, to the next zero word) */
static void limitfo_t2(uint16_t w)
{
    if ((w & 0xFF) == 0x34) return;
    uint16_t y = (uint16_t)(((w >> 8) & 0x7F) << 1);
    if (!r16(RAM_OFF_pfgoal + y)) {
        if ((uint16_t)(0xFFA8 - g.foy) & 0x8000) return;
        g.foy = 0xFFBF;
    } else {
        if (!((uint16_t)(0x58 - g.foy) & 0x8000)) return;
        g.foy = 0x41;
    }
    g.fox = g.fox & 0x8000 ? 0xFFBA : 0x46;
}

/* The faceoff spot (fox, foy) after a penalty call, checked against each penalty in PenBuf (limitfo_t2):
 * one may pull it out of an end zone to the neutral-zone dot on that side */
void limitfo(void)
{
    uint16_t a0 = PenBuf;
    do {
        limitfo_t2(r16(a0));
        a0 += 2;
    } while (r16(a0));
}
