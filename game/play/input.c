/* input: the pads and the players they control (src/routines/input). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */

/* Pad marker slot d4 follows player o (its number, stored as a word). */
void setpads(int o, uint16_t d4)
{
    w16(RAM_OFF_PadControlObj + d4, (uint16_t)o);
}

/* Hook or hold: player o is locked in the hook animation, or the hold when the best player a0 (if
 * any) faces within the right range of him. */
void Acheck(int o, uint16_t a0)
{
    uint16_t anim = (uint16_t)SYM_SPAhook;
    g.pfalock[o] = anim;
    if (!(a0 & 0x8000) && ((uint16_t)(g.facedir[o] - r16(RAM_OFF_facedir + a0) + 2) & 7) >= 5)
        anim = (uint16_t)SYM_SPAHold;
    SetSPA(o, anim);
}

/* passdir = player o's facing (and sfspdir), unless he is the goalie with the puck just behind the
 * net (x -16..15, y $108-$10F). */
void setpassmode(int o)
{
    if (g.position[o] == 0 && !NEG(g.Xpos[14], 0xFFF0) && NEG(g.Xpos[14], 0x10)) {
        uint16_t y = (uint16_t)g.pucky[0];
        if (!NEG(y, 0x108) || NEG(y, 0xFEF8)) {
            if (y < 0x110) return;
        }
    }
    g.passdir = g.facedir[o] & 7;
    g.sflags |= sfspdir;
}

/* Player o's swing for direction *d0 (made relative to his facing): 0 forehand, 1 backhand. */
uint16_t Findhittype(int o, uint16_t *d0)
{
    *d0 = (uint16_t)(g.facedir[o] - *d0) & 7;
    return m8(SYM_Findhittype_1 + (g.attribute[o] & 0x0800 ? 0 : 3) + *d0);
}

/* The cross product of the goalie vector d0 / d1 and (d3 / d4 - the puck), divided by d2 (*d3 is
 * left the point's x from the puck). */
uint16_t CalcAngleOffset(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t *d3, uint16_t d4)
{
    *d3 = (uint16_t)(*d3 - g.Xpos[14]);
    uint32_t p1 = Mul16S((uint16_t)(d4 - g.pucky[0]), d0);
    uint32_t p2 = Mul16S(d1, *d3);
    return Div32S(p1 - p2, d2);
}

/* The line for choice d0 (0-2) of team a2 by the man advantage, from the table: choice + 3 * the
 * current line, 21 on when the counts on the ice differ and 21 more when the other team has more
 * (or as many); -1 for none. */
uint16_t getlchoice(uint16_t a2, uint16_t d0)
{
    uint16_t diff = (uint16_t)(r16(RAM_OFF_tmap + (uint16_t)(a2 ^ 2)) - r16(RAM_OFF_tmap + a2));
    if (diff) {
        d0 = (uint16_t)(d0 + 0x15);
        if (!(diff & 0x8000)) d0 = (uint16_t)(d0 + 0x15);
    }
    uint16_t l = r16(RAM_OFF_tmline + a2);
    d0 = (uint16_t)(d0 + l + l + l);
    return (uint16_t)(int8_t)m8((SYM_getlchoice_tab + d0) & 0xFFFFFF);
}

/* Hold button: unless player o is the goalie or locked in his burst, the opponent with the lowest
 * distance + 16 * facing difference (folded to 0-4) is a0 (none: -1), and Acheck hooks or holds.
 * (A goalie or an empty spot scores the last distance, d4 as it came for the first; an empty
 * spot's position shifts in too.) */
void holdplayer(int o, uint16_t d4)
{
    if (g.position[o] == 0) return;
    if (g.pfalock[o] && g.SPA[o] == (uint16_t)SYM_SPAburst) return;
    uint16_t d3 = 0xFFFF, a0 = 0xFFFF;
    uint16_t y = 2 * o >= 0xC ? 0 : 0xC;
    for (int n = 0; n < 6; n++, y += 2) {
        uint16_t a = r16(RAM_OFF_position + y);
        unsigned c = 0;
        if (a && !(a & 0x8000)) {
            uint16_t dx = (uint16_t)(r16(RAM_OFF_Xpos + y) - g.Xpos[o]);
            uint16_t dy = (uint16_t)(r16(RAM_OFF_Ypos + y) - g.Ypos[o]);
            d4 = ApproxLength(dx, dy);
            a = (uint16_t)(vtoa(dx, dy) - g.facedir[o]) & 7;
            if (a >= 5) a = (uint16_t)(~a + 8 + 1);
        }
        c = a >> 12 & 1;                                  /* (the last asl's carry into the adc) */
        a = (uint16_t)((uint16_t)(a << 4) + d4 + c);
        if (a < d3) { d3 = a; a0 = y; }
    }
    Acheck(o, a0);
}

/* Start a shot by player o: no pass direction, shot mode (sflags 8), aimed at the goal, speed 15,
 * the forehand or backhand shot. */
void SetShotMode(int o)
{
    g.passdir = 8;
    g.sflags |= 8;
    uint16_t d0 = (uint16_t)(0 - g.Xpos[o]);
    uint16_t d1 = (uint16_t)((g.pfgoal[o] ? 0x128 : 0xFED8) - g.Ypos[o]);
    d0 = vtoa(d0, d1);
    g.passspeed = 0xF;
    SetSPA(o, (uint16_t)(Findhittype(o, &d0) ? SYM_SPAshotb : SYM_SPAshotf));
}

/* Start a sweep check by player o (held in pf2aip). */
void Sweepcheck(int o)
{
    g.pf2aip[o] = (uint16_t)SYM_SPAsweepchk;
    SetSPAFromA(o, (uint16_t)SYM_SPAsweepchk);
}

/* Check button: a speed burst along the facing of player a3 (o, the same player, for the lock and
 * the energy): energy * $233 >> 16, after paying $CC of it with line changes on (automatic ones:
 * from a fixed $1000). */
void burst(int o, int a3)
{
    if (g.pfalock[o]) return;
    uint16_t d0 = getpde(o);
    if (g.OptLine) {
        d0 = (uint16_t)(d0 - 0xCC);
        setpde(g.pfteam[o] != 0, (uint16_t)((g.pnum[o] & 0xFF) << 1), d0);
        if (g.OptLine == 2) d0 = 0x1000;
    }
    uint16_t sp = (uint16_t)(Mul16U(d0, 0x233) >> 16);
    w16(RAM_OFF_burstspeed, sp);
    uint32_t t = SYM_dirtab + (uint16_t)(g.facedir[a3] << 2);
    g.Xvel[a3] = (int16_t)(Mul8XS(sp, rom16(t)) + g.Xvel[a3]);
    g.Yvel[a3] = (int16_t)(Mul8XS(sp, rom16(t + 2)) + g.Yvel[a3]);
    g.pfalock[a3] = (uint16_t)SYM_SPAburst;
    SetSPAFromA(a3, (uint16_t)SYM_SPAburst);
}

/* Pad input for player o taking a face off (assignment $17): the pad direction d0 goes in fodir1
 * (fodir2 shooting at the top goal); unless he is swinging already, B (d1 $8000) starts the swing,
 * otherwise the ready pose. */
void faceoffinput(int o, uint16_t d0, uint16_t d1)
{
    if (g.asscur[o] != 0x17) return;
    if (g.pfgoal[o]) g.fodir2 = d0; else g.fodir1 = d0;
    if (g.pf2aip[o]) return;
    if (d1 & 0x8000) {
        g.pf2aip[o] = 1;
        SetSPAFromA(o, (uint16_t)SYM_SPAfaceoff);
    } else SetSPAFromA(o, (uint16_t)SYM_SPAfaceoffr);
}

/* A computer shot by player o (a3 after the aim) goes to the open side when the goalie, where he
 * will be, covers the middle: passdir 1-4 or 5-8 at random, 0 when he does not, 8 with no goalie. */
void shotdiradj(int o, int a3)
{
    if (!(g.playerpad[o] & 0x8000)) return;
    uint16_t d0 = 8;
    uint16_t y = g.pfteam[o] ? 0 : 0xC;
    int n;
    for (n = 0; n < 6 && r16(RAM_OFF_position + y); n++) y = (uint16_t)(y + 2);
    if (n < 6) {
        int16_t v = (int8_t)(r16(RAM_OFF_Xvel + y) >> 8);
        d0 = (uint16_t)((uint16_t)(v >> 1) + r16(RAM_OFF_Xpos + y) - g.Xpos[14]);
        v = (int8_t)(r16(RAM_OFF_Yvel + y) >> 8);
        uint16_t d1 = (uint16_t)((uint16_t)(v >> 1) + r16(RAM_OFF_Ypos + y) - g.pucky[0]);
        uint16_t d2 = ApproxLength(d1, d0);
        uint16_t gy = g.pfgoal[o] ? 0x108 : 0xFEF8;
        uint16_t d3 = 0x12;
        uint16_t d5 = CalcAngleOffset(d0, d1, d2, &d3, gy);
        d3 = (uint16_t)-0x12;
        uint16_t d4 = (uint16_t)(CalcAngleOffset(d0, d1, d2, &d3, gy) + d5);
        d0 = 0;
        if (!NEG(d4, 0xFFD3) && NEG(d4, 0x2D)) {
            if (g.pfgoal[a3]) d4 = (uint16_t)-d4;
            d0 = (uint16_t)((RandomWord() & 3) + 1);
            if (d4 & 0x8000) d0 = (uint16_t)(d0 + 4);
        }
    }
    g.passdir = d0;
}

/* The pass once the puck's velocity is set (dopass, assdumpin): a goalie never passes back toward
 * his net; the pass swing (SPAgswing, or forehand / backhand), locked; a human goalie stops and his
 * pad picks its player again (findpc, a3 as given). */
void dopass_swing(int o, int a3)
{
    if (g.position[o] == 0) {
        int neg = (g.puckvy[0] & 0x8000) != 0;
        if (g.pfgoal[o] ? neg : !neg) g.puckvy[0] = (int16_t)-g.puckvy[0];
    }
    uint16_t d0 = vtoa((uint16_t)g.puckvx[0], (uint16_t)g.puckvy[0]);
    uint16_t anim = (uint16_t)SYM_SPAgswing;
    if (g.position[o]) anim = (uint16_t)(Findhittype(o, &d0) ? SYM_SPApassb : SYM_SPApassf);
    SetSPA(o, anim);
    g.pfalock[o] = 1;
    if (g.position[o] || (g.playerpad[o] & 0x8000)) return;
    g.Xvel[o] = 0;
    g.Yvel[o] = 0;
    findpc(o, g.playerpad[o], a3);
}


/* (dopass_count / its tail: the pass's speed d5 (spread by the passer's accuracy) along d2 / d3 over
 * the distance d4, the target, the waits, the pad) */
static void pass_launch(uint16_t a0, uint16_t a2, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t d5, uint16_t d6, int a3)
{
    g.passspeed = d5;
    int x = g.curobj >> 1;
    d5 = (uint16_t)(randomd0s((uint16_t)(0x10 - g.passacc[x])) + d5);
    g.puckvx[0] = (int16_t)(Div32S(Mul16x8XS(d2, d5), d4) << 8);
    g.puckvy[0] = (int16_t)(Div32S(Mul16x8XS(d3, d5), d4) << 8);
    g.passtargetx = (uint16_t)(d2 + g.Xpos[14]);
    g.passtargety = (uint16_t)(d3 + g.pucky[0]);
    g.puckvz = (int16_t)(d4 << 3);
    g.nopuck[14] = 0xA;
    w16(RAM_OFF_passwait + a0, (uint16_t)(d6 + 6));
    g.nopuck[x] = d6;
    w16(RAM_OFF_passpad + a2, g.playerpad[x]);
    if (g.position[x] || (g.playerpad[x] & 0x8000)) return;
    g.Xvel[x] = 0;
    g.Yvel[x] = 0;
    uint16_t d4p = g.playerpad[x];
    AssignPadIfFree(g.passReceiverPlayerNum, d4p);
    if (g.playerpad[x] & 0x8000) return;
    findpc(x, d4p, a3);
}

/* The pass to receiver a0 (player o's team, a3 the passer): counted (clock running), the other
 * goalie's timer raised, the receiver gets asspassrec ($15) and is aimed at where his stick will be
 * (his one-timer frame's hot spot) after the puck's travel, at a speed by the distance. */
void dopass_count(int o, uint16_t a0, int a3, uint16_t y)
{
    uint16_t a2 = loadTeamStruct(o);
    if (!g.gmclock) {
        y = a2;                                           /* (with the clock stopped Y stays as the caller left it) */
        w16(RAM_OFF_tmpassatt + a2, (uint16_t)(r16(RAM_OFF_tmpassatt + a2) + 1));
    }
    uint16_t gx = r16(RAM_OFF_topgoalie + y);
    w16(RAM_OFF_temp1 + gx, (uint16_t)((r16(RAM_OFF_aidef + gx) >> 1) + 1 + r16(RAM_OFF_temp1 + gx)));
    g.passReceiverPlayerNum = a0;
    assinsert(a0 >> 1, 0x15);
    uint16_t save = r16(RAM_OFF_frame + a0);
    uint16_t dir = r16(RAM_OFF_attribute + a0) & 0x0800 ? (uint16_t)(0 - r16(RAM_OFF_facedir + a0)) : r16(RAM_OFF_facedir + a0);
    uint16_t t = rom16(SYM_SPAonetimerf + (uint16_t)((dir & 7) << 1));
    w16(RAM_OFF_frame + a0, rom16((SYM_SPAonetimerf & 0xFF0000) | t));
    int16_t hx, hy;
    GetHot(a0 >> 1, &hx, &hy);
    w16(RAM_OFF_frame + a0, save);
    uint16_t d1 = (uint16_t)(hy + r16(RAM_OFF_Ypos + a0) - g.pucky[0]);
    uint16_t d0 = (uint16_t)(hx + r16(RAM_OFF_Xpos + a0) - g.Xpos[14]);
    uint16_t vx = r16(RAM_OFF_Xvel + a0), vy = r16(RAM_OFF_Yvel + a0);
    uint16_t d6 = 0x18;
    uint16_t d2 = (uint16_t)((Mul16XS((uint16_t)(d6 << 4), vx) >> 16) + d0);
    uint16_t d3 = (uint16_t)((Mul16XS((uint16_t)(d6 << 4), vy) >> 16) + d1);
    uint16_t d4 = ApproxLength(d3, d2);
    uint16_t a = DivX8U((uint16_t)(d4 << 1), 3);
    if (a < 0x40 || a >= 0x50) {
        if (a < 0x40) { d6 = d4 >> 2; a = 0x40; }
        else { d6 = DivX8U(d4, 5); a = 0x50; }
        d2 = (uint16_t)((Mul16XS((uint16_t)(d6 << 4), vx) >> 16) + d0);
        d3 = (uint16_t)((Mul16XS((uint16_t)(d6 << 4), vy) >> 16) + d1);
        d4 = ApproxLength(d3, d2);
    }
    pass_launch(a0, a2, d2, d3, d4, a, d6, a3);
}

/* Player o passes in passdir: the puck goes loose, the pass speed by his accuracy (a goalie 7); the
 * teammate (by a3's team... by o's) most nearly in that direction (puckdist2, $1000 more one step
 * off) gets it (dopass_count), else it goes along passdir plus half the passer's velocity. Then the
 * pass swing (as g.curobj). */
void dopass(int o, int a3)
{
    g.onetimerreq = 0;
    g.sflags &= (uint16_t)~4;
    g.puckc = -1;
    g.nopuck[o] = 0x10;
    g.lastplayer[0] = (uint16_t)o;
    uint16_t d0 = g.position[o] ? g.passacc[o] : 7;
    g.passspeed = (uint16_t)((d0 << 2) + 0x90);
    uint16_t d4 = 0xFFFF, a0 = 0;
    uint16_t a1 = 2 * o >= 0xC ? 0xC : 0;
    uint16_t y = 0;                                       /* (the Y the loop leaves: the last teammate looked at) */
    for (int d3 = 5; d3 >= 0; d3--, a1 += 2) {
        if (2 * o == a1) continue;
        y = a1;
        uint16_t p = r16(RAM_OFF_position + a1);
        if ((p & 0x8000) || !p || (r16(RAM_OFF_pflags + a1) & 0x1000)) continue;
        uint16_t a = (uint16_t)(r16(RAM_OFF_puckdir + a1) - g.passdir - 4) & 7;
        uint16_t r0 = 0;
        if (a) {
            if (a != 1 && a != 7) continue;
            r0 = 0x1000;
        }
        uint32_t s = (uint32_t)r0 + r16(RAM_OFF_puckdist2 + a1);
        if (s > 0xFFFF || s >= d4) continue;
        d4 = (uint16_t)s;
        a0 = a1;
    }
    if (!(d4 & 0x8000)) {
        dopass_count(a3, a0, a3, y);
        PuckSfx(0);
    } else {
        uint32_t t = SYM_dirtab + (uint16_t)(g.passdir << 2);
        int x = g.curobj >> 1;
        uint16_t r3 = (uint16_t)(Mul8XS(0x50, (uint16_t)((int16_t)rom16(t + 2) >> 1)) << 1);
        g.puckvy[0] = (int16_t)((g.Yvel[x] >> 1) + r3);
        r3 = (uint16_t)(Mul8XS(0x50, (uint16_t)((int16_t)rom16(t) >> 1)) << 1);
        g.puckvx[0] = (int16_t)((g.Xvel[x] >> 1) + r3);
        g.puckvz = (int16_t)(RandomWord() & 0xFFF);
    }
    dopass_swing(g.curobj >> 1, a3);
}

/* Pass mode input for player o: the pass button changing (d2 $8000) passes; a pad direction (d0,
 * bit 3 clear) becomes passdir and passes. */
void passmode(int o, uint16_t d0, uint16_t d2, int a3)
{
    if (!(d2 & 0x8000)) {
        if (d0 & 8) return;
        g.passdir = d0 & 7;
    }
    dopass(o, a3);
}


/* The stick is at the puck: player o shoots toward the goal (shotdiradj may aim a computer shot at
 * the open side; a3 after). The shot speed from the windup (a backhand loses a quarter) and his shot
 * power and energy; the aim (the shotsets spot for passdir) spread by his accuracy unless the roll
 * says it is on target (or in a highlight); lifted by the height from shotsets. The shot sound (1,
 * 2 for a hard one; 7 when he does not have the puck), the puck kept moving toward the goal line.
 * (The original keeps the sound, the height, and the aim's y and x on the stack, adding the spread
 * into them in place.) */
void doshot(int o, int a3)
{
    shotdiradj(o, a3);
    uint16_t sound = 7;
    g.sflags &= (uint16_t)~8;
    g.pfalock[o] = 8;
    g.shotplayer = (uint16_t)(2 * o);
    if (2 * o == g.puckc) {
        sound = 1;
        g.sflags |= 0x1000;
        if (g.SPA[o] == (uint16_t)SYM_SPAshotb) {
            sound = 1;
            g.passspeed = (uint16_t)(-(g.passspeed >> 2) + g.passspeed);
        }
        g.lastshotspeed[0] = g.passspeed;
        if (g.passspeed >= 0x1E) sound = 2;
        uint16_t m = (uint16_t)(makepde(g.shotspd[o], o) + 0x14);
        uint32_t p = Mul16x8U(m, g.passspeed);
        g.passspeed = (uint16_t)(Mul16U(0x2570, (uint16_t)p) >> 16);
        if (g.breakaway) { CheckBreakaway(0); g.breakaway = g.IsBreakaway; }
        int x = g.curobj >> 1;
        g.puckc = -1;
        g.nopuck[x] = 0x10;
        g.lastplayer[0] = (uint16_t)x;
        uint16_t gy = g.pfgoal[x] ? 0x108 : 0xFEF8;
        uint32_t t = SYM_shotsets + (uint16_t)(g.passdir << 2);
        uint16_t ax = (uint16_t)(m16(t) - g.Xpos[14]);       /* (the stack: height, aim y, aim x) */
        uint16_t height = m16(t + 2);
        uint16_t ay = (uint16_t)(gy - g.pucky[0]);
        uint16_t d3 = ApproxLength(ax, ay);
        if (!d3) d3 = 1;
        if (!(g.gmode & gmhl) && (d3 >= 0xC9 || randoma8((uint16_t)(0x10 + g.shotacc[x])) < 0xF)) {
            uint16_t a = (uint16_t)((g.passspeed >> 3) - g.shotacc[x] + 0x10);
            uint16_t d0 = (uint16_t)((uint16_t)Mul16U(a, d3) >> 6);
            if (d3 < 0xFB) d0 >>= 1;
            if (d0 >= sideline) d0 = sideline;
            uint16_t spread = d0;
            ax = (uint16_t)(randomd0s(d0) + ax);
            d0 = spread;
            if (d0 >= 0x3D) { d0 = 0x3C; spread = 0x3C; }
            if (ay & 0x8000) d0 >>= 1;
            ay = (uint16_t)(ay + randomd0sd0(d0));
            height = (uint16_t)(randomd0A(spread >> 1) + height);
        }
        g.puckvx[0] = (int16_t)(Div32S(Mul16S(Mul8XS(0x44, g.passspeed), ax), d3) << 1);
        g.puckvy[0] = (int16_t)(Div32S(Mul16S(Mul8XS(0x44, g.passspeed), ay), d3) << 1);
        if (height) {
            uint32_t q = Mul16x8U(height, g.passspeed);
            q = Mul16x8U((uint16_t)q, 0x44);
            uint16_t d1 = (uint16_t)(Div32U(q, d3) << 1);
            uint16_t r0 = Div32U(Mul16U(0xB33, d3), g.passspeed);
            uint16_t vz = (uint16_t)((r0 >> 1) + d1 + (r0 & 1));   /* (lsr's carry into the adc) */
            g.puckvz = (int16_t)(vz >= 0x1801 ? 0x1800 : vz);
        }
    }
    PuckSfx(sound);
    uint16_t gy = g.pfgoal[a3] ? 0x108 : 0xFEF8;
    if (((uint16_t)(gy - g.pucky[0]) ^ (uint16_t)g.puckvy[0]) & 0x8000) g.puckvy[0] = (int16_t)-g.puckvy[0];
    if (g.PenShotActive) assreplace(a3, 0x12);
}

/* Shot input for player o (d0 the pad direction, d2 the changed buttons): only the carrier in a shot
 * animation; at its last frame ($18) he shoots; the pad aims, the windup builds the speed by the
 * frames elapsed, and letting go of the button jumps the animation on ($1C - the frame, shooting
 * at $18 or past). Otherwise shot mode ends (sflags 8). */
void ShotMode(int o, uint16_t d0, uint16_t d2, int a3)
{
    if (2 * o != g.puckc || g.SPA[o] < (uint16_t)SYM_SPAshotf || g.SPA[o] >= 0xF021) { g.sflags &= (uint16_t)~8; return; }
    if (g.SPAnum[o] >= 0x18) { doshot(o, a3); return; }
    if (!(d0 & 8)) g.passdir = d0 & 7;
    if (g.SPAnum[o] >= 0x10) return;
    g.passspeed = (uint16_t)(g.passspeed + g.d7);
    if (!(d2 & cbut)) return;
    g.SPAnum[o] = (uint16_t)(0x1C - g.SPAnum[o]);
    if (g.SPAnum[o] < 0x18) return;
    g.SPAnum[o] = 0x18;
    doshot(o, a3);
}

/* The print position (printx / printy) and size of team a2's line change box: its side's spot
 * (setlccordsPos, the other side with the teams switched, 21 rows apart; the print's carry goes
 * into the add), 12 wide (returned) and 6 high (*d1; 5, a row lower, when choice 2 has no line). */
uint16_t setlccords(uint16_t a2, uint16_t *d1)
{
    uint16_t d0 = a2 ? 0 : 1;
    uint32_t e = print(SYM_setlccordsPos);
    unsigned c = (uint16_t)e < (uint16_t)SYM_setlccordsPos;   /* (print's last adc) */
    if (g.gmode & gmdir) d0 ^= 1;
    g.printy = (uint16_t)(Mul8U(0x15, d0) + g.printy + c);
    *d1 = 6;
    if (getlchoice(a2, 2) & 0x8000) {
        (*d1)--;
        g.printy++;
    }
    return 0xC;
}


/* Draw team a2's line change box: its area erased (but over the faceoff box) and framed (in the
 * upper half a plain box first; in the lower half sf3llcs set), then a row per line choice (all
 * of them while it is open, else the chosen one with the current line): the button letter, the
 * line's name and its energy bar; then the team's name. Returns d0 as left (the last choice's
 * line, its energy after the bar). */
uint16_t SetLCmode2(uint16_t a2)
{
    uint16_t d1, d0 = setlccords(a2, &d1);
    if (!g.fobox) eraser((uint16_t)(d0 + 1), (uint16_t)(d1 + 1), g.blankchar);
    d0 = setlccords(a2, &d1);
    if (!NEG(g.printy, 0xF)) g.sflags3[0] |= 1;
    else {
        box();
        d0 = setlccords(a2, &d1);
    }
    Framer(d0, d1);
    g.printy -= 2;
    g.printx++;
    uint16_t px = g.printx;
    for (int d4 = 2; d4 >= 0; d4--) {
        g.printx = px;
        d0 = getlchoice(a2, (uint16_t)d4);
        if (d0 & 0x8000) continue;
        if (!(r16(RAM_OFF_tmflags + a2) & 8)) {
            if (d4 != r16(RAM_OFF_tmlcchoice + a2)) { g.printy--; continue; }
            d0 = r16(RAM_OFF_tmline + a2);
        }
        w16(RAM_OFF_mesarea, 4);
        w16(RAM_OFF_mesarea + 2, (uint16_t)((m16(SYM_LineChangeButtons + d4) & 0xFF) | 0x2000));
        print(RAM_OFF_mesarea);
        PrintStringFromList(d0, SYM_linelist);
        d0 = linebar(a2, d0);
        g.printy--;
    }
    g.printx = (uint16_t)(px + 5);
    uint32_t a1 = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
    a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1 + 4));
    a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1));
    print(a1);
    return d0;
}

/* Open the line change box of player o's team (line changes on, not automatic; not open yet):
 * tmflags bit 3, the pass and shot directions off, SetLCmode2. */
void SetLCmode(int o)
{
    if (g.OptLine != 1) return;
    uint16_t a2 = loadTeamStruct(o);
    if (r16(RAM_OFF_tmflags + a2) & 8) return;
    w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) | 8);
    g.sflags &= 0xFFF3;
    SetLCmode2(a2);
}

/* Close team a2's line change box: its area erased (but over the faceoff box; sf3llcs cleared in
 * the lower half), then the scoreboard. */
void lcfound_close(uint16_t a2)
{
    uint16_t d1, d0 = setlccords(a2, &d1);
    if (!NEG(g.printy, 0xF)) g.sflags3[0] &= 0xFFFE;
    if (!g.fobox) eraser(d0, (uint16_t)(d1 + 1), g.blankchar);
    printscores1();
}

/* Team a2 changes to the line of row d2 (remembered; none there: nothing): player a3 is done with
 * the box (pflags bit 3), his team's box closes (tmflags bit 3, no pad), its line set and put on
 * the ice (setpersonel), lcfound_close. */
void lcfound(uint16_t a2, int a3, uint16_t d2)
{
    w16(RAM_OFF_tmlcchoice + a2, d2);
    uint16_t d0 = getlchoice(a2, d2);
    if (d0 & 0x8000) return;
    g.pflags[a3] |= 8;
    a2 = loadTeamStruct(a3);
    w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) & 0xFFF7);
    w16(RAM_OFF_tmline + a2, d0);
    w16(RAM_OFF_lcpad + a2, 0xFFFF);
    setpersonel(a2);
    lcfound_close(a2);
}

/* Pad input of player o while his team's line change box is open (d1 the new presses, d0 the
 * direction): a redraw request (tmflags bit 0) closes and reopens it; Y, B, A pick row 0, 1, 2
 * (lcfound, by a3); Select closes it keeping the line; otherwise he skates when done with the box
 * and free (with the d0 a redraw left). */
void lineinput(int o, uint16_t d1, uint16_t d0, int a3)
{
    uint16_t a2 = g.pfteam[o] ? 2 : 0;
    if (r16(RAM_OFF_tmflags + a2) & 1) {
        w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) & 0xFFFE);
        w16(RAM_OFF_lcpad + a2, 0xFFFF);
        lcfound_close(a2);
        d0 = SetLCmode2(a2);
    }
    if (d1 & 0x4000) { lcfound(a2, a3, 0); return; }
    if (d1 & 0x8000) { lcfound(a2, a3, 1); return; }
    if (d1 & 0x0080) { lcfound(a2, a3, 2); return; }
    if (d1 & 0x2000) {
        g.pflags[o] |= 8;
        a2 = loadTeamStruct(o);
        w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) & 0xFFF7);
        w16(RAM_OFF_lcpad + a2, 0xFFFF);
        lcfound_close(a2);
        return;
    }
    if ((g.pflags[o] & 8) && !g.pfalock[o]) doplayeracc(o, d0, a3);
}

/* (doinput's .13: a goalie skates unless his flags say he is out of his crease, not covering) */
static void goalie_input(int o, uint16_t d0, int a3)
{
    if (!(g.goalieflags[o] & 1) && (g.goalieflags[o] & 2)) return;
    doplayeracc(o, d0, a3);
}

/* (doinput's .17: the pad d4 takes its team's goalie, with manual goalies) */
static void take_goalie(int o, uint16_t d4)
{
    if (!r16(RAM_OFF_goaliemode1 + (NEG(2 * o, 0xC) ? 0 : 2))) return;
    PadTakeGoalie(d4);
}

/* (doinput's .22: a locked player (or the pass receiver) can only change player, take the goalie,
 * or with A give the pad to a free computer teammate the pass goes to, asking for a one-timer) */
static void locked_input(int o, uint16_t d1, uint16_t d4, int a3)
{
    if (2 * o == g.puckc) return;
    if (d1 & 0x8000) { findpc(o, d4, a3); return; }
    if (d1 & 0x0040) { take_goalie(o, d4); return; }
    if (!(d1 & 0x0080)) return;
    uint16_t y = g.passReceiverPlayerNum;
    if (y & 0x8000) return;
    if ((r16(RAM_OFF_pflags + y) & 0x1000) | r16(RAM_OFF_pfalock + y)) return;
    if (!(r16(RAM_OFF_playerpad + y) & 0x8000)) return;
    if (r16(RAM_OFF_pfteam + y) != g.pfteam[o]) return;
    g.onetimerreq++;
    AssignPadPlayer(y, d4);
}

/* Pad d4's input for player o (d0 the direction, bit 3 none, turned on the horizontal screen; d1
 * the new presses, d2 for the pass and shot modes, d3 held): Start pauses; the faceoff; the line
 * change box (when open and his pad's or nobody's); Select opens it; the carrier passes, shoots,
 * dumps in (Y) or skates; others hold (Y), take the goalie (X), change player (B), take a
 * defenseman (L / R), burst (A) or skate. */
void doinput(int o, uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, int a3)
{
    setpads(o, d4);
    d4 = (uint16_t)(d4 - 2);
    if (d3 & 0x1000) { PauseFromPad(d4); return; }
    if (g.sfhormode && !(d0 & 8)) d0 = (uint16_t)(d0 + 2) & 7;
    if (g.sflags & 0x100) { faceoffinput(o, d0, d1); return; }
    if (r16(RAM_OFF_asscur + 2 * o) == 0xBCC7) return;
    uint16_t t = g.pfteam[o] ? 2 : 0, lp = r16(RAM_OFF_lcpad + t);
    if (((lp & 0x8000) || lp == d4) && (r16(RAM_OFF_tmflags + t) & 8)) { lineinput(o, d1, d0, a3); return; }
    if (d1 & 0x2000) {
        if (g.gmclock || !(r16(RAM_OFF_lcpad + t) & 0x8000)) return;
        w16(RAM_OFF_lcpad + t, d4);
        SetLCmode(o);
        return;
    }
    if (2 * o == g.puckc) {
        if (g.pfalock[o]) { locked_input(o, d1, d4, a3); return; }
        d2 = checkob(o, d2);
        if (!g.position[o] && g.pf2aip[o]) return;
        if (g.sflags & sfspdir) { passmode(o, d0, d2, a3); return; }
        if (g.sflags & 8) { ShotMode(o, d0, d2, a3); return; }
        if (g.dumpinplayer == g.curobj) return;
        if (d1 & 0x8000) { setpassmode(o); return; }
        if (d1 & 0x4000) { assinsert(o, 0x14); return; }
        if (!g.position[o]) { goalie_input(o, d0, a3); return; }
        if (d1 & 0x0080) { SetShotMode(o); return; }
        doplayeracc(o, d0, a3);
        return;
    }
    if (d1 & 0x4000) { holdplayer(o, d4); return; }
    if (g.pfalock[o]) { locked_input(o, d1, d4, a3); return; }
    if (d1 & 0x0040) { take_goalie(o, d4); return; }
    if (d1 & 0x8000) { findpc(o, d4, a3); return; }
    if (!g.position[o]) { goalie_input(o, d0, a3); return; }
    if (d1 & 0x0020) { PadTakeLeftDefense(o, d4); return; }
    if (d1 & 0x0010) { PadTakeRightDefense(o, d4); return; }
    if (g.passReceiverPlayerNum == g.curobj) { locked_input(o, d1, d4, a3); return; }
    if (d1 & 0x0080) { burst(o, a3); return; }
    doplayeracc(o, d0, a3);
}
