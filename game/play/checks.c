/* checks: the players' assignments, skating and the puck's states (src/routines/checks). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define SPFpuck 396

/* The direction (0-7, 8 = none) of the vector dx / dy: its signs and two slope tests (each side
 * against twice the other, unsigned as the 65816 compares) pick it from the cartridge's table. */
uint16_t vtoa(uint16_t dx, uint16_t dy)
{
    if ((dx | dy) == 0) return 8;
    unsigned k = 0;                                     /* (built in d7+2) */
    if (dx & 0x8000) { dx = (uint16_t)-dx; k += 1; }
    if (dy & 0x8000) { dy = (uint16_t)-dy; k += 2; }
    if ((uint16_t)(dy << 1) >= dx) k |= 4;
    if ((uint16_t)(dx << 1) >= dy) k |= 8;
    return rom8(SYM_vtoa_dt + k);
}

/* Start animation anim on object o (the same one keeps going). */
void SetSPA(int o, uint16_t anim)
{
    if (anim == g.SPA[o]) return;
    g.SPA[o] = anim;
    g.SPAnum[o] = 0;
    g.SPAcnt[o] = 0xFFFF;
}

/* Z: player o is killing a penalty (no power play: never). */
int chkpk(int o)
{
    if (!(g.sflags & sf2pwrplay)) return 0;
    return chkpk2(o);
}

/* Z: player o is on the team a man short (sf2pwrtm is 0 for team 1). */
int chkpk2(int o)
{
    if (g.pfteam[o]) return !(g.sflags & sf2pwrtm);
    return (g.sflags & sf2pwrtm) != 0;
}

/* End player o's assignment and go back to the one under it. */
void assexit(int o)
{
    g.assnum[o] = (g.assnum[o] + 1) & 7;
    g.asscur[o] = g.asslist[o * 8 + g.assnum[o]];
    g.pflags[o] |= pfna;
}

/* Give player o assignment ass on top of the current one. */
void assinsert(int o, uint8_t ass)
{
    g.assnum[o] = (g.assnum[o] - 1) & 7;
    assreplace(o, ass);
}

/* Overwrite player o's current assignment with ass. */
void assreplace(int o, uint8_t ass)
{
    g.pflags[o] |= pfna;
    g.asslist[o * 8 + g.assnum[o]] = ass;
    g.asscur[o] = ass;
}

/* The hot spot of object o's sprite frame: the frame record's signed bytes at +$12 / +$13, x
 * negated when the frame is flipped (attribute $0800), y unless it is flipped vertically ($1000);
 * on the horizontal screen turned a quarter, (x, y) -> (y, -x). */
void GetHot(int o, int16_t *hx, int16_t *hy)
{
    uint32_t t = SYM_SpriteFrames + (uint16_t)((g.frame[o] - 1) * 4);   /* (offsets from SpriteFrames) */
    uint16_t lo = rom16(t), hi = rom16(t + 2);
    uint32_t sum = lo + (SYM_SpriteFrames & 0xFFFF);
    uint32_t rec = ((hi + (SYM_SpriteFrames >> 16) + (sum >> 16)) & 0xFF) << 16 | (sum & 0xFFFF);
    int16_t x = (int8_t)m8(rec + 0x12), y = (int8_t)m8(rec + 0x13);
    if (g.attribute[o] & 0x0800) x = (int16_t)-x;
    if (!(g.attribute[o] & 0x1000)) y = (int16_t)-y;
    if (g.sfhormode) { int16_t t2 = x; x = y; y = (int16_t)-t2; }
    *hx = x; *hy = y;
}

/* Turn goalie o's facing a step the short way toward want, at most a step per 6 ticks. */
void AdjustFacingDirection(int o, uint16_t want)
{
    uint16_t cur = g.facedir[o];
    if (g.goalieturnwait[o] == 0) {
        uint16_t diff = (uint16_t)(want - cur);
        if (diff) {
            uint16_t step = (uint16_t)((((uint16_t)-diff & 4) >> 1) - 1);   /* +1 or -1 */
            g.facedir[o] = (cur + step) & 7;
            g.goalieturnwait[o] = 6;
            return;
        }
    }
    if (g.goalieturnwait[o] == 0) return;
    uint16_t w = (uint16_t)(g.goalieturnwait[o] - g.timestep);
    g.goalieturnwait[o] = w & 0x8000 ? 0 : w;
}

/* Player g.curobj accelerates toward direction dir, up to his top speed: a step of the direction
 * (none into the wall he last hit), scaled by weight and leg strength and the frames elapsed (d7),
 * added to his velocity unless his speed would pass the limit for his leg speed and energy; then,
 * with line changes on, one call in 64 wears his energy down. */
void playeracc(uint16_t dir)
{
    int o = g.curobj >> 1;
    uint32_t t = SYM_playeracc_8 + (uint16_t)(dir << 2);
    uint16_t sy = rom16(t + 2), sx = rom16(t);
    if (g.Wallsin[o] && ((g.Wallsin[o] ^ sx) & 0x8000)) sx = 0;   /* (into the wall: no step) */
    if (g.Wallcos[o] && !((g.Wallcos[o] ^ sy) & 0x8000)) sy = 0;
    uint16_t f = (uint16_t)(0x20 - (g.weight[o] >> 3) + g.legstr[o]);
    f = (uint16_t)(f + (g.legstr[o] >> 1) + (g.legstr[o] & 1));   /* (lsr's carry into the adc) */
    if (g.position[o] == 0) {
        if ((g.playerpad[o] & 0x8000) || g.curobj == (uint16_t)g.puckc) {
            if (!(g.passReceiverPlayerNum & 0x8000)) f = (uint16_t)(f - g.legstr[o]);
            f = (uint16_t)(f + 8);
        } else {
            uint16_t l2 = (uint16_t)(g.legstr[o] << 1);
            unsigned c = g.legstr[o] >> 15;                       /* (asl's carry) */
            unsigned s1 = l2 + f + c;
            f = (uint16_t)(s1 + 8 + (s1 >> 16));
        }
    }
    /* the new velocity: the old plus step * factor * frames elapsed (carry from the multiply) */
    uint16_t m = Mul8XS(f, sx);
    uint32_t p = Mul16x8XS(m, g.d7);
    uint16_t vx = (uint16_t)((uint16_t)p + (uint16_t)g.Xvel[o] + cf);
    uint16_t hx = (uint16_t)(int8_t)(vx >> 8);
    uint16_t sq = Mul8S(hx, hx);
    m = Mul8XS(f, sy);
    p = Mul16x8XS(m, g.d7);
    uint16_t vy = (uint16_t)((uint16_t)p + (uint16_t)g.Yvel[o] + cf);
    uint16_t hy = (uint16_t)(int8_t)(vy >> 8);
    sq = (uint16_t)(Mul8S(hy, hy) + sq);
    /* the limit: the cartridge's table by leg speed and energy (automatic line changes: $1000) */
    uint16_t e = getpde(o);
    if (g.OptLine == 2) e = 0x1000;
    p = Mul16x8U((uint16_t)((e >> 1) + 0x800), (uint16_t)(g.legspd[o] << 4));
    uint16_t lim = rom16(SYM_MaxSpeed + (uint16_t)((uint16_t)(p >> 16) << 2) + 2);
    if (g.position[o] == 0 && !(g.playerpad[o] & 0x8000) && !(g.goalieflags[o] & 4)) {
        lim = (uint16_t)(-(lim >> 2) + lim + (lim >> 1 & 1));   /* (a computer goalie: a quarter off; lsr's carry) */
    }
    if (lim >= sq) { g.Xvel[o] = (int16_t)vx; g.Yvel[o] = (int16_t)vy; }
    if (!g.OptLine || (g.seed[0] & 0x3F)) return;
    e = (uint16_t)(getpde(o) - 0x21);
    if (!((uint16_t)(e - 0xC00) & 0x8000)) e = (uint16_t)(g.endurance[o] + e);
    setpde(g.pfteam[o] != 0, (uint16_t)((g.pnum[o] & 0xFF) << 1), e);
}

/* The shared lone rtl of ChkOffsides */
void ChkOffsides_rtl(void) {}

/* A lone rtl (the end of ChkGoalies) */
void rtss(void) {}

/* Flip direction dir when player o skates backwards, then accelerate toward it. */
void noturn(int o, uint16_t dir)
{
    if (g.pfrev[o]) dir ^= 4;
    playeracc(dir);
}

/* Where the puck will be soon: its position plus half the high byte of its velocity. */
void skatetopuckinit(uint16_t *d0, uint16_t *d1)
{
    uint16_t a = (uint16_t)(g.puckvx[0] >> 9 & 0x7F);
    if (a & 0x40) a |= 0xFF80;
    *d0 = (uint16_t)(a + g.Xpos[14]);
    a = (uint16_t)((uint16_t)g.puckvy[0] >> 9 & 0x7F);
    if (a & 0x40) a |= 0xFF80;
    *d1 = (uint16_t)(a + g.pucky[0]);
}

/* Icing: once armed and not yet called, the loose puck past the far goal line calls it. */
void puckIChk(void)
{
    if (!(g.iflags & 4) || (g.iflags & 1) || !(g.puckc & 0x8000)) return;
    if (g.iflags & 2) {
        if (!NEG(0x108, g.pucky[0])) return;
    } else if (NEG(0xFEF8, g.pucky[0])) return;
    g.iflags |= 1;
}

/* Computer player o shoots: the swing time temp2b is the distance to the goal line / 8, at most
 * 20, and the assignment becomes assshoot ($13). (The original drops its caller's return address:
 * the caller returns right after.) */
void CompShoot(int o)
{
    uint16_t d0 = (uint16_t)g.pucky[0];
    if (!g.pfgoal[o]) d0 = (uint16_t)-d0;
    uint16_t t = (uint16_t)(0x108 - d0) >> 3;
    if (t >= 0x14) t = 0x14;
    g.temp2[o] = 0;
    g.temp2b[o] = (int16_t)t;
    assreplace(o, 0x13);
}

/* The puck shadow (object o) follows the puck, one unit off it, while the puck frame shows. */
void puckshadow(int o)
{
    uint16_t f = g.frame[o];
    if (f == SPFpuck) {
        g.Xpos[o] = g.Xpos[14];
        w16(RAM_OFF_Ypos + 2 * o, (uint16_t)g.pucky[0]);
        g.shadowz = 0;
        if (g.sfhormode) g.Xpos[o]++;
        else w16(RAM_OFF_Ypos + 2 * o, (uint16_t)(r16(RAM_OFF_Ypos + 2 * o) + 1));
        return;
    }
    if (!f) return;
    g.Xpos[o] = 0;
    w16(RAM_OFF_Ypos + 2 * o, 0x12C);
    g.shadowz = 0xE;
    if (!(g.pucky[0] & 0x8000)) return;
    g.attribute[o] = 0x8000;
    w16(RAM_OFF_Ypos + 2 * o, 0xFED4);
    g.shadowz--;
}

/* A skateto callback: when the puck carrier will be within 40 of player o next frame on both axes,
 * the direction away from him; otherwise direction d0 as it was. */
uint16_t EvadePC(int o, uint16_t d0)
{
    if (g.puckc & 0x8000) return d0;
    uint16_t v = (uint16_t)(int8_t)(uint8_t)(((uint16_t)g.Xvel[o] >> 8) - ((uint16_t)g.puckvx[0] >> 8));
    v = (uint16_t)(v + g.Xpos[o] - g.Xpos[14]);
    if (NEG(v, 0xFFD7) || !NEG(v, 0x29)) return d0;
    v = (uint16_t)(int8_t)(uint8_t)(((uint16_t)g.Yvel[o] >> 8) - ((uint16_t)g.puckvy[0] >> 8));
    v = (uint16_t)(v + g.Ypos[o] - g.pucky[0]);
    if (NEG(v, 0xFFD7) || !NEG(v, 0x29)) return d0;
    return vtoa((uint16_t)(g.Xpos[o] - g.Xpos[14]), (uint16_t)(g.Ypos[o] - g.pucky[0]));
}

/* Slow player o down without changing his animation: a human goalie (not carrying the puck) loses
 * half his speed, everybody else ($70 + legstr) * timestep per axis, down to zero. */
void stopna(int o)
{
    if (g.position[o] == 0 && !(g.playerpad[o] & 0x8000) && 2 * o != g.puckc) {
        g.Xvel[o] = (int16_t)(g.Xvel[o] >> 1);           /* (cmp #$8000, ror: the sign kept) */
        g.Yvel[o] = (int16_t)(g.Yvel[o] >> 1);
        return;
    }
    uint16_t f = Mul8U((uint16_t)(0x70 + g.legstr[o]), g.timestep);
    uint16_t v = (uint16_t)g.Xvel[o];
    if (!(v & 0x8000)) { v = (uint16_t)(v - f); if (v & 0x8000) v = 0; }
    else { v = (uint16_t)(v + f); if (!(v & 0x8000)) v = 0; }
    g.Xvel[o] = (int16_t)v;
    v = (uint16_t)g.Yvel[o];
    if (!(v & 0x8000)) { v = (uint16_t)(v - f); if (v & 0x8000) v = 0; }
    else { v = (uint16_t)(v + f); if (!(v & 0x8000)) v = 0; }
    g.Yvel[o] = (int16_t)v;
}

/* avdgoal's working set: p0 = where the player is, p1 = where he skates to (d0 / d1), r3 / r3+2
 * the goal's sides, r6 the goalie at that end and r6+2 its goal line, tmp0 / tmp1 the zones of
 * p0 / p1, tmp2 the number of goals on the path; y is the 65816's Y (a goal side value, not an
 * object, on one path: Xpos,y then reads past the array) */
typedef struct { uint16_t p0x, p0y, p1x, p1y, r3a, r3b, r6, r6b, tmp0, tmp1, tmp2, d0, d1, y; } Avd;

static uint16_t avd_xpos(uint16_t y) { return m16((SYM_avdgoal & 0xFF0000) + RAM_OFF_Xpos + y); }

/* (.27 for a skater, .48 for a goalie: lim is $2C or $24) */
static void avd_zones(Avd *s, uint16_t rx, uint16_t ry, uint16_t lim)
{
    s->r3a = rx; s->r3b = ry;
    s->tmp0 = s->tmp1 = 0;
    uint16_t x = s->p0y, y = s->p1y;
    if (!NEG(x, s->r3a)) goto l4;
    s->tmp0 = 8;
    if (!NEG(y, s->r3a)) goto l5;
    return;
l2: if (NEG(y, s->r3b)) goto l6;
    s->tmp1 = 2;
    goto l6;
l3: if (NEG(y, lim)) goto l7;
    s->tmp1 |= 4;
    goto l7;
l4: if (!NEG(y, s->r3a)) goto l5;
    s->tmp1 = 8;
l5: if (NEG(x, s->r3b)) goto l2;
    s->tmp0 = 2;
    if (!NEG(y, s->r3b)) return;
l6: x = s->p0x; y = s->p1x;
    if (NEG(x, lim)) goto l3;
    s->tmp0 |= 4;
    if (!NEG(y, lim)) return;
l7: if (!NEG(x, (uint16_t)-lim)) goto l2x;
    s->tmp0 |= 0x10;
    if (NEG(y, (uint16_t)-lim)) return;
    s->tmp2++;
    return;
l2x:if (!NEG(y, (uint16_t)-lim)) { s->tmp2++; return; }
    s->tmp1 |= 0x10;
    s->tmp2++;
}

/* (the skater's cases: .t2x when tmp1 = 0, else the table .26 by tmp0) */
static void avd_skater(Avd *s)
{
    uint16_t pushed;
    if (s->tmp1) {
        switch (s->tmp0) {
        case 0: goto l7;
        case 2: goto l11;
        case 4: goto l18;
        case 6: goto l15;
        case 8: goto l21;
        case 12: goto l19;
        case 16: goto l24;
        case 18: goto l25;
        case 24: goto l22;
        default: return;                                 /* (a zero table entry: never taken) */
        }
    }
    if (!NEG(s->p0y, 0xFEF8)) goto t1xx;
    if (NEG(s->p1y, 0xFEF8)) return;
    goto l8;
t1xx:
    if (NEG(s->p1y, 0xFEF8)) goto l5;
    s->y = s->r6;
    if (s->y & 0x8000) return;
    if (NEG(s->p0x, s->p1x)) goto l4;
    if (NEG(s->p0x, avd_xpos(s->y))) return;
    s->d1 = s->r3b;
    return;
l4: if (!NEG(s->p0x, avd_xpos(s->y))) return;
    s->d1 = s->r3b;
    return;
l5: s->d1 = s->r3b;
    goto l10;
l6: s->d0 = 0xFFCA;
    return;
l7: if (!NEG(s->p0y, 0xFEF8)) goto l9;
l8: if (NEG(s->p0x, 0xFFCB)) goto l6;
    if (!NEG(s->p0x, 0x36)) goto l17;
    s->d1 = s->r3a;
    goto l12;
l9: if (!NEG(s->p1y, 0xFEF8)) goto t1xxx;
    s->d1 = s->r6b;
    s->y = s->r6;
    if (s->y & 0x8000) goto l12;
    if (NEG(s->p0x, avd_xpos(s->y))) goto l6;
    goto l17;
t1xxx:
    s->y = s->r6;
    if (s->y & 0x8000) return;
l10:pushed = (uint16_t)(s->p1x - avd_xpos(s->y));
    if (!(((uint16_t)(s->p0x - s->p1x) ^ pushed) & 0x8000)) return;
    if (!NEG(s->p0x, avd_xpos(s->y))) goto l17;
    goto l23;
l11:s->d1 = s->r3b;
    if (s->tmp1 & 8) return;
l12:if (!(s->p1x & 0x8000)) {
        if (!NEG(s->p1x, 0x36)) return;
        s->d0 = 0x36;
        return;
    }
    if (NEG(s->p1x, 0xFFCB)) return;
    s->d0 = 0xFFCA;
    return;
l15:if (s->tmp1 != 0x10) goto l17;
l16:s->d1 = s->r3b;
    return;
l17:s->d0 = 0x36;
    return;
l18:s->d0 = 0x36;
    if (!(s->tmp1 & 0xA)) goto l16;
    return;
l19:if (s->tmp1 != 0x10) goto l17;
l20:s->d1 = s->r3a;
    return;
l21:s->d1 = s->r3a;
    if (s->tmp1 & 2) goto l12;
    return;
l22:if (s->tmp1 == 4) goto l20;
l23:s->d0 = 0xFFCA;
    return;
l24:s->d0 = 0xFFCA;
    if (!(s->tmp1 & 0xA)) goto l16;
    return;
l25:if (s->tmp1 == 4) goto l16;
    s->d0 = 0xFFCA;
}

/* (the goalie's cases: .t3 when tmp1 = 0, else the table .47 by tmp0) */
static void avd_goalie(Avd *s)
{
    if (s->tmp1) {
        switch (s->tmp0) {
        case 0: goto l30;
        case 2: goto l34;
        case 4: goto l40;
        case 6: goto l37;
        case 8: goto l43;
        case 12: goto l41;
        case 16: goto l45;
        case 18: goto l46;
        case 24: goto l44;
        default: return;                                 /* (a zero table entry: never taken) */
        }
    }
    if (!NEG(s->p0y, 0xFEF8)) {
        if (!NEG(s->p1y, 0xFEF8)) return;
        goto l33;
    }
    if (NEG(s->p1y, 0xFEF8)) return;
    goto l31;
l29:s->d0 = 0xFFDA;
    return;
l30:if (!NEG(s->p0y, 0xFEF8)) goto l32;
l31:if (NEG(s->p0x, 0xFFDB)) goto l29;
    if (!NEG(s->p0x, 0x26)) goto l39;
    s->d1 = s->r3a;
    goto l35;
l32:if (s->tmp1 & 2) return;
l33:s->d1 = s->r3b;
    goto l35;
l34:s->d1 = s->r3b;
    if (s->tmp1 & 8) return;
l35:if (!(s->p1x & 0x8000)) {
        if (!NEG(s->p1x, 0x26)) return;
        s->d0 = 0x26;
        return;
    }
    if (NEG(s->p1x, 0xFFDB)) return;
    s->d0 = 0xFFDA;
    return;
l37:if (s->tmp1 != 0x10) goto l39;
l38:s->d1 = s->r3b;
    return;
l39:s->d0 = 0x26;
    return;
l40:s->d0 = 0x26;
    if (!(s->tmp1 & 0xA)) goto l38;
    return;
l41:if (s->tmp1 != 0x10) goto l39;
l42:s->d1 = s->r3a;
    return;
l43:s->d1 = s->r3a;
    if (s->tmp1 & 2) goto l35;
    return;
l44:if (s->tmp1 == 4) goto l42;
    s->d0 = 0xFFDA;
    return;
l45:s->d0 = 0xFFDA;
    if (!(s->tmp1 & 0xA)) goto l38;
    return;
l46:if (s->tmp1 == 4) goto l38;
    s->d0 = 0xFFDA;
}

/* (a path crossing the top goal: mirrored onto the bottom one) */
static void avd_mirror(Avd *s)
{
    uint16_t t = s->r3a; s->r3a = s->r3b; s->r3b = t;
    s->y = s->r3a;
    s->p0y = (uint16_t)-s->p0y;
    s->p1y = (uint16_t)-s->p1y;
    if (s->tmp0 & 0xA) s->tmp0 ^= 0xA;
    if (s->tmp1 & 0xA) s->tmp1 ^= 0xA;
}

/* Keep player o from skating through a goal: when the path to d0 / d1 crosses a net, aim beside it
 * (the goalie keeps to his crease's sides). */
void avdgoal(int o, uint16_t *d0, uint16_t *d1)
{
    Avd s = { 0 };
    s.p0x = (uint16_t)g.Xpos[o]; s.p0y = (uint16_t)g.Ypos[o];
    s.p1x = *d0; s.p1y = *d1;
    s.d0 = *d0; s.d1 = *d1;
    if (g.position[o]) {
        avd_zones(&s, 0xE5, 0x11C, 0x2C);
        s.r3a = 0xE4; s.r3b = s.y = 0x11D;
        s.r6b = 0x108; s.r6 = g.topgoalie;
        if (s.tmp2) avd_mirror(&s);
        else {
            avd_zones(&s, 0xFEE4, 0xFF1B, 0x2C);
            s.r3a = 0xFEE3; s.r3b = s.y = 0xFF1C;
            s.r6b = 0xFEF8; s.r6 = g.botgoalie;
            if (!s.tmp2) return;
        }
        avd_skater(&s);
    } else {
        avd_zones(&s, 0x102, 0x11C, 0x24);
        s.r3a = 0x101; s.r3b = s.y = 0x11D;
        if (s.tmp2) avd_mirror(&s);
        else {
            avd_zones(&s, 0xFEE4, 0xFEFE, 0x24);
            s.r3a = 0xFEE3; s.r3b = s.y = 0xFEFF;
            if (!s.tmp2) return;
        }
        avd_goalie(&s);
    }
    *d0 = s.d0; *d1 = s.d1;
}

/* Clear team a2's offsides flag once none of its six skaters is past the blue line (y beyond $58
 * from his own goal). An empty spot, or a player shooting at the top goal (his y not mirrored),
 * is judged by the y in d0 as it was; returns d0 as left. */
uint16_t ClearOffsidesIfAllPlayers(uint16_t a2, uint16_t d0)
{
    if (!(r16(RAM_OFF_tmflags + a2) & tmfoside)) return d0;
    uint16_t a0 = r16(RAM_OFF_tmsort + a2);
    for (int n = 0; n < 6; n++, a0 += 2) {
        if (!(r16(RAM_OFF_position + a0) & 0x8000)) {
            d0 = r16(RAM_OFF_Ypos + a0);
            if (!r16(RAM_OFF_pfgoal + a0)) d0 = (uint16_t)-d0;
        }
        if (!NEG(d0, 0x58)) return d0;
    }
    w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) & 0xFFEF);
    return d0;
}

/* A computer player with the puck looks for a shot: in the last seconds with the clock running he
 * shoots; with nothing special on, a threat and 1 in 4 he dumps the puck in; otherwise a random
 * roll on his shot odds (better near the goal, worse with the goalie set, an opponent in the way
 * doubling them) decides, when he is in front of the goal line. Returns nonzero when he shoots
 * (CompShoot: his caller is ended too). */
int chk4shot(int o)
{
    if (!g.gmclock && g.gameclock[0] < 5) { CompShoot(o); return 1; }
    if (!g.PenShotActive && chkpk(o) && g.threat && !NEG(g.Ypos[o], 0xFF13) && NEG(g.Ypos[o], 0xED)
        && !(g.seed[0] & 3)) {
        StartDumpIn(o);
        return 0;
    }
    int x = o;
    uint16_t d4 = (uint16_t)((uint16_t)(0x20 - g.spodds[o]) << 4);
    uint16_t d1 = (uint16_t)((g.pfgoal[o] ? 0x108 : 0xFEF8) - g.pucky[0]);
    uint16_t px = (uint16_t)g.Xpos[14];
    if (!NEG(d1, 0x64) || NEG(d1, 0xFF9C) || !NEG(px, 0x64) || NEG(px, 0xFF9C)) goto roll;
    {
        uint16_t d0 = (uint16_t)-px;
        Mul8S(d0, d0);
        Mul8S(d1, d1);                                    /* (the squared distance, compared with $2711 and dropped) */
        x = g.curobj >> 1;
        d4 >>= 4;
        uint16_t d5 = vtoa(d0, d1);
        uint16_t y = g.curobj < 0xC ? 0xC : 0;
        for (int d3 = 5; d3 >= 0; d3--, y += 2) {
            if (!r16(RAM_OFF_position + y)) {
                if (r16(RAM_OFF_pf2aip + y)) { d3 = 0; d4 = 1; }
            } else if (((uint16_t)(r16(RAM_OFF_puckdir + y) - 4) & 7) == d5) d4 = (uint16_t)(d4 << 1);
        }
    }
roll:
    if (randoma(d4) >= 9) return 0;
    uint16_t d0 = (uint16_t)g.pucky[0];
    if (!g.pfgoal[x]) d0 = (uint16_t)-d0;
    if ((d0 & 0x8000) || ((uint16_t)(0x108 - d0) & 0x8000)) return 0;
    if (g.PenShotActive && !((uint16_t)((0xB0 >> 2) + 0x58 - d0) & 0x8000)) return 0;
    if (g.sflags & 0x8000) return 0;
    CompShoot(x);
    return 1;
}

/* Goalie o skates in direction d0 (8: none, 9: stop), turning a step at a time toward it near his
 * goal or with the puck, or slows down. */
void goalieacc(int o, uint16_t d0)
{
    d0 &= 0xF;
    if (d0 >= 8) {
        if ((g.playerpad[o] & 0x8000) && d0 != 9) goto ready;
        if (g.Xvel[o] | g.Yvel[o]) { stopna(o); return; }
ready:
        if (g.pf2aip[o]) return;
        SetSPA(o, (uint16_t)SYM_SPAgready);
        return;
    }
    if (2 * o != g.puckc && g.asscur[o] != 0xC) {
        uint16_t ay = (uint16_t)g.Ypos[o];
        if (ay & 0x8000) ay = (uint16_t)-ay;
        if (ay >= 0x98) {
            if (!(g.playerpad[o] & 0x8000) && g.SPA[o] >= (uint16_t)SYM_SPAgdive && g.SPA[o] < (uint16_t)SYM_SPAinjury1) {
                stopna(o);
                return;
            }
            if (d0 == g.facedir[o]) goto wait;
            SetSPAFromA(o, (uint16_t)SYM_SPAgready);
            playeracc(d0);
            return;
        }
    }
    if (!g.goalieturnwait[o]) {
        uint16_t diff = (uint16_t)(d0 - g.facedir[o]);
        if (diff) {
            g.facedir[o] = (uint16_t)((((uint16_t)-diff & 4) >> 1) - 1 + g.facedir[o]) & 7;
            g.goalieturnwait[o] = 6;
        }
    }
wait:
    if (g.goalieturnwait[o]) {
        uint16_t w = (uint16_t)(g.goalieturnwait[o] - g.timestep);
        g.goalieturnwait[o] = w & 0x8000 ? 0 : w;
    }
    SetSPAFromA(o, (uint16_t)(g.goalieflags[o] & 2 ? SYM_SPAgskate : SYM_SPAgready));
    playeracc(g.facedir[o]);
}

/* Spin the puck: its facing flipped by bit 0 of d0, its flip animation started. */
void puckflip(uint16_t d0)
{
    g.facedir[14] = (uint16_t)((d0 & 1) ^ g.facedir[14]) & 3;
    g.SPAcnt[14] = 0xFFFF;
    SetSPAFromA(14, (uint16_t)SYM_SPApflip);
}

/* A chance to check: on 2 random rolls against (20 - checking) * 2 (with penalties on), an opponent
 * within 34 straight ahead of player o, not already holding / locked, is hooked or held (Acheck)
 * or burst into (burst, by player a3), by where o is and 7 in 8 of the random bits. */
void check4check(int o, int a3)
{
    uint16_t d0 = (uint16_t)(0x14 - g.checking[o]);
    if (g.OptPen) d0 = (uint16_t)(d0 << 1);
    if (randoma(d0) >= 4 && randoma(d0) >= 4) return;
    uint16_t hold2 = (uint16_t)SYM_SPAHold2;
    uint16_t y = 2 * o >= 0xC ? 0 : 0xC;
    for (int d2 = 5; d2 >= 0; d2--, y += 2) {
        if (!r16(RAM_OFF_position + y)) continue;
        uint16_t s = r16(RAM_OFF_SPA + y);
        if (!(s >= hold2 && s < 0xF46B) && r16(RAM_OFF_pfalock + y)) continue;
        uint16_t dx = (uint16_t)(r16(RAM_OFF_Xpos + y) - g.Xpos[o]);
        if (NEG(dx, 0xFFDE) || !NEG(dx, 0x22)) continue;
        uint16_t dy = (uint16_t)(r16(RAM_OFF_Ypos + y) - g.Ypos[o]);
        if (NEG(dy, 0xFFDE) || !NEG(dy, 0x22)) continue;
        if (vtoa(dx, dy) != g.facedir[o]) continue;
        uint16_t px = (uint16_t)g.Xpos[o], py = (uint16_t)g.Ypos[o];
        uint16_t rnd = r16(RAM_OFF_seed + 1) & 7;
        int zone;                                           /* (.2x: in his attacking zone) */
        if (!NEG(px, 0x4A) || NEG(px, 0xFFB6)) zone = 0;
        else if (!g.pfgoal[o]) zone = !NEG(py, 0x58) && NEG(py, 0x108);
        else zone = NEG(py, 0xFFA8) && !NEG(py, 0xFEF8);
        if (zone) {
            if ((s >= hold2 && s < 0xF46B) || !rnd) { burst(o, a3); return; }
        } else if (rnd) { burst(o, a3); return; }
        Acheck(o, y);
        return;
    }
}

/* Choose the player for pad d4 (X = o, a3 the player again after the search): when o is a free
 * skater close to the puck, or the pad's best skater (distance to the puck, plus a penalty for the
 * puck's direction against the one preferred from o's goal, the two XORed) has it already, o
 * starts a sweep check unless locked; else the best one takes the pad. */
void findpc(int o, uint16_t d4, int a3)
{
    uint16_t r0 = 8;
    uint16_t x = (uint16_t)(2 * o);
    uint16_t pc = (uint16_t)g.puckc;
    if (!(pc & 0x8000)) {
        if (g.position[o] && !(g.playerpad[o] & 0x8000) && !g.pfalock[o] && g.puckdist2[o] < 0x64) goto l10;
        uint16_t pp = r16(RAM_OFF_playerpad + pc);
        if (!(pp & 0x8000) && r16(RAM_OFF_padside + pp) == r16(RAM_OFF_padside + x)) goto l9;   /* (padside by the player's index) */
        r0 = g.pfgoal[o] ? 0 : 4;
    }
l9:;
    uint16_t d2 = 0xFFFF, d3 = 0;
    uint16_t cx = r16(RAM_OFF_cont1team + d4) == 1 ? 0 : 0xC;
    for (int d1 = 6; d1; d1--, cx += 2) {
        uint16_t p = r16(RAM_OFF_position + cx);
        if ((p & 0x8000) || !p) continue;
        if (r16(RAM_OFF_pflags + cx) & 0x1000) continue;
        uint16_t s = r16(RAM_OFF_SPA + cx);
        if (!((s >= (uint16_t)SYM_SPAturnl && s < 0xEBE3) || (s >= (uint16_t)SYM_SPAcrosscheck && s < 0xF343))
            && ((r16(RAM_OFF_pflags + cx) & 0x1000) | r16(RAM_OFF_pfalock + cx))) continue;
        uint16_t pp = r16(RAM_OFF_playerpad + cx);
        if (!(pp & 0x8000) && pp != d4) continue;
        uint16_t r3 = 0;
        if (!(g.puckc & 0x8000) && r0 != 8) {
            uint16_t a = r0 ^ r16(RAM_OFF_puckdir + cx);
            if (a >= 4) a = (uint16_t)(~a + 8 + 1);
            if (a) r3 = (uint16_t)((uint16_t)(a - 1) << 5);
        }
        uint32_t c = (uint32_t)r16(RAM_OFF_puckdist + cx) + r3;
        if (c > 0xFFFF || c >= d2) continue;
        d3 = cx;
        d2 = (uint16_t)c;
    }
    x = (uint16_t)(2 * a3);
    if (d2 == 0xFFFF) return;
    if (d3 != r16(RAM_OFF_c1playernum + d4)) { AssignPadPlayer(d3, d4); return; }
l10:
    if (r16(RAM_OFF_pfalock + x)) return;
    Sweepcheck(x >> 1);
}



/* Player o stops: a fast one (over $1000 on an axis) glides (backwards) or plays the stop (the
 * puck carrier with the skate stop sound $11), then slows down (stopna). */
void dostop(int o)
{
    uint16_t vx = (uint16_t)g.Xvel[o], vy = (uint16_t)g.Yvel[o];
    if (!NEG(vx, 0xEFFF) && NEG(vx, 0x1001) && !NEG(vy, 0xEFFF) && NEG(vy, 0x1001)) { stopna(o); return; }
    uint16_t anim = (uint16_t)SYM_SPAglide;
    if (!g.pfrev[o]) {
        if (2 * o == g.puckc) sfx(0x11, vx, vy);
        anim = (uint16_t)SYM_SPAstop;
        g.pf2aip[o] = anim;
    }
    SetSPAFromA(o, anim);
    stopna(o);
}

/* Player o's wanted direction needs no turn (d0 = wanted - facing, * 2; d2 the direction to skate):
 * facing it he skates (backwards, stumbling injured, with the puck), otherwise he stops when moving
 * within 90 degrees of it, else turns a step and glides. */
void noturn0(int o, uint16_t d0, uint16_t d2)
{
    uint16_t d4 = 2;
    if (g.pfrev[o]) { d4 = 6; d0 ^= 8; }
    if (d0) {
        uint16_t v = vtoa((uint16_t)g.Xvel[o], (uint16_t)g.Yvel[o]);
        if (!(v & 8) && ((uint16_t)(v - g.facedir[o] + d4) & 7) < 4) { dostop(o); return; }
        g.facedir[o]++;
        if (g.attribute[o] & 0x0800) g.facedir[o] -= 2;
        g.facedir[o] &= 7;
        if (!g.pfrev[o]) SetSPA(o, (uint16_t)SYM_SPAglide);
        else SetSPAFromA(o, (uint16_t)SYM_SPAglideback);
        return;
    }
    uint16_t y = 1, anim = (uint16_t)SYM_SPAskateback;
    if (!g.pfrev[o]) {
        y = 0; anim = (uint16_t)SYM_SPAskate;
        if (g.pflags[o] & pf2inj) { y = 1; anim = (uint16_t)SYM_SPAstumble; }
        if (2 * o == g.puckc) { y = 0; anim = (uint16_t)SYM_SPAskatewp; }
    }
    if (!g.pf2aip[o]) {
        g.pf2aip[o] = y;
        SetSPA(o, anim);
    }
    noturn(o, d2);
}

/* Player o skates toward direction d0 (low nibble: 0-7, 8 none, 9 stop): goalies by goalieacc;
 * skaters decide on skating backwards (in their own half, the puck ahead, or staying so), then turn
 * toward it at a rate falling with speed, or need no turn (noturn0), and accelerate. After the
 * choice the turn is made by player a3 (X reloaded from a3). */
void doplayeracc(int o, uint16_t d0, int a3)
{
    if (!g.position[o]) { goalieacc(o, d0); return; }
    if (g.SPA[o] == (uint16_t)SYM_SPAsweepchk) return;
    uint16_t d1 = g.pfrev[o] ? (uint16_t)SYM_SPAglideback : (uint16_t)SYM_SPAglide;
    d0 &= 0xF;
    if (d0 >= 8) {
        if (d0 == 9 && (g.Xvel[o] | g.Yvel[o])) { dostop(o); return; }
        if (g.pf2aip[o]) return;
        SetSPA(o, d1);
        return;
    }
    uint16_t want = d0, d2 = d0;
    if (2 * o == g.puckc) goto fwd;
    {
        uint16_t dx = (uint16_t)(g.Xpos[14] - g.Xpos[o]);
        uint16_t d3 = (uint16_t)g.Ypos[o];
        uint16_t dy = (uint16_t)((uint16_t)(int8_t)((uint16_t)g.puckvy[0] >> 8) + g.pucky[0] - d3);
        if (!g.pfgoal[o]) { dx = (uint16_t)-dx; dy = (uint16_t)-dy; d3 = (uint16_t)-d3; d2 ^= 4; }
        if (!g.pfrev[o]) {
            if (!(d3 & 0x8000) || d2 != 4) goto fwd;
            if (((uint16_t)(vtoa(dx, dy) + 1) & 7) >= 3) goto fwd;
        } else {
            if (((uint16_t)(d2 - 3) & 7) >= 3) goto fwd;
            if (((uint16_t)(vtoa(dx, dy) + 2) & 7) >= 5) goto fwd;
        }
        if (g.pfrev[o]) goto turn;
        uint16_t a = 0;
        if (g.Xvel[o] | g.Yvel[o]) {
            a = (uint16_t)(vtoa((uint16_t)g.Xvel[o], (uint16_t)g.Yvel[o]) - want + 1) & 7;
            if (a >= 3) goto turn;
        }
        g.pfrev[o] = (uint16_t)(a + 1);
        goto turn;
    }
fwd:
    g.pfrev[o] = 0;
turn:
    d2 = g.facedir[o];
    d0 = (uint16_t)(((uint16_t)(want - d2) & 7) << 1);
    uint16_t step = rom16(SYM_doplayeracc_10 + d0);
    if (!step) { noturn0(a3, d0, d2); return; }
    uint16_t t0 = Mul8XS(g.d7, step);
    uint16_t hx = (uint16_t)(int8_t)((uint16_t)g.Xvel[a3] >> 8);
    uint16_t d4 = Mul8S(hx, hx);
    uint16_t hy = (uint16_t)(int8_t)((uint16_t)g.Yvel[a3] >> 8);
    uint16_t d3 = (uint16_t)(Mul8S(hy, hy) + d4);
    d4 = (uint16_t)(0x300 - d3);
    if (NEG(d4, 0x180)) d4 = 0x180;
    uint32_t t = Mul16x8S(d4, t0);
    if (g.pfrev[a3]) t = (uint32_t)-(int32_t)t;
    uint32_t f = (uint32_t)g.facefrac[a3] + (uint16_t)t;
    g.facefrac[a3] = (uint16_t)f;
    g.facedir[a3] = (uint16_t)(g.facedir[a3] + (t >> 16) + (f >> 16)) & 7;
    d2 = g.facedir[a3];
    if (d3 >= 0x15) {
        d1 = t0 & 0x8000 ? 0xFFCE : 0;
        if (g.attribute[a3] & 0x0800) d1 ^= 0xFFCE;
        d1 = (uint16_t)(d1 + (uint16_t)SYM_SPAturnr);
    }
    if (!g.pf2aip[a3]) {
        SetSPA(a3, d1);
        g.pf2aip[a3] = 1;
    }
    if (d3 >= 3) noturn(a3, d2);
}

static uint16_t asspuckc_chkdir(int o, uint16_t d0);
static uint16_t abs16(uint16_t v) { return v & 0x8000 ? (uint16_t)-v : v; }

/* (skateto's avoidance routine, a0: run through the RAM trampoline jmlvector, which is set) */
static uint16_t skate_avoid(int o, uint32_t a0, uint16_t d0)
{
    w16(RAM_OFF_jmlvector + 1, (uint16_t)a0);
    w16(RAM_OFF_jmlvector + 2, (uint16_t)(a0 >> 8));
    switch (a0 & 0xFFFFFF) {
    case SYM_EvadePC: return EvadePC(o, d0);
    case SYM_rtss: return d0;
    case SYM_assbreakaway_nocallback: return d0;
    case SYM_asspuckc_chkdir: return asspuckc_chkdir(o, d0);
    }
    w16(RAM_OFF_d0, d0);                                  /* (a routine not ported yet, through the trampoline) */
    if (call_long_other) call_long_other(RAM_OFF_jmlvector);   /* ($00:1D59, in RAM) */
    return r16(RAM_OFF_d0);
}

/* Player o skates toward d0 / d1 (around the goals, led by his velocity), every $0C frames aiming
 * again; slowing down to a stop (direction 9) near it (within 5, or 12 when fast; goalies only with
 * goalieflags bit 1; the distance measured from the target plus the velocity's sides as
 * ApproxLength left them, not the lead); the avoidance routine a0 may change the direction;
 * standing with no direction he looks at the puck (that direction is not used). Then doplayeracc
 * (a3 as given). */
void skateto(int o, uint16_t d0, uint16_t d1, uint32_t a0, int a3)
{
    uint16_t t = (uint16_t)(g.temp2[o] - g.timestep);
    unsigned c = (uint16_t)g.temp2[o] >= g.timestep;
    g.temp2[o] = (int16_t)t;
    if (t & 0x8000) {
        g.temp2[o] = (int16_t)(t + 0xC + c);
        uint16_t tx = d0, ty = d1;
        avdgoal(o, &d0, &d1);
        uint16_t r3 = (uint16_t)-(uint16_t)(int8_t)((uint16_t)g.Xvel[o] >> 8);
        d0 = (uint16_t)(r3 + d0 - g.Xpos[o]);
        uint16_t r3b = (uint16_t)-(uint16_t)(int8_t)((uint16_t)g.Yvel[o] >> 8);
        d1 = (uint16_t)(r3b + d1 - g.Ypos[o]);
        int slow = 0, stop = 0;
        if (g.position[o] || (g.goalieflags[o] & 2)) slow = 1;
        if (slow) {
            uint16_t sp = ApproxLength((uint16_t)g.Yvel[o], (uint16_t)g.Xvel[o]);
            uint16_t ax = (uint16_t)g.Xvel[o], ay = (uint16_t)g.Yvel[o];   /* (ApproxLength leaves the shorter side in r3, the longer in r3+2) */
            if (ax & 0x8000) ax = (uint16_t)-ax;
            if (ay & 0x8000) ay = (uint16_t)-ay;
            r3 = ax < ay ? ax : ay;
            r3b = ax < ay ? ay : ax;
            uint16_t dist = ApproxLength((uint16_t)(r3b + ty - g.Ypos[o]), (uint16_t)(r3 + tx - g.Xpos[o]));
            stop = sp >= 0x1000 ? NEG(dist, 0xC) != 0 : NEG(dist, 5) != 0;
        }
        d0 = stop ? 9 : vtoa(d0, d1);
        d0 = skate_avoid(o, a0, d0);
        g.temp2b[o] = (int16_t)d0;
        if (d0 >= 8 && !(g.Xvel[o] | g.Yvel[o])) vtoa((uint16_t)(g.Xpos[14] - g.Xpos[o]), (uint16_t)(g.pucky[0] - g.Ypos[o]));
    }
    doplayeracc(o, (uint16_t)g.temp2b[o], a3);
}

/* Player o skates to where the puck will be (around the goals, led by his hot spot and velocity),
 * aiming again every 5 frames; a skater facing the puck (head on to its travel) at the right range
 * from its carrier sweep checks instead (as player a3, who then also skates). */
void skatetopuck(int o, int a3)
{
    uint16_t d0, d1;
    int x = o;
    skatetopuckinit(&d0, &d1);
    uint16_t t = (uint16_t)(g.temp2[o] - g.d7);
    unsigned c = (uint16_t)g.temp2[o] >= g.d7;
    g.temp2[o] = (int16_t)t;
    if (!(t & 0x8000)) goto skate;
    g.temp2[o] = (int16_t)(t + 5 + c);
    avdgoal(o, &d0, &d1);
    {
        int16_t hx, hy;
        GetHot(o, &hx, &hy);
        uint16_t a = (uint16_t)(int8_t)(uint8_t)((uint16_t)-hx - ((uint16_t)g.Xvel[o] >> 8));
        uint16_t nx = (uint16_t)(a + d0 - g.Xpos[o]);
        a = (uint16_t)(int8_t)(uint8_t)((uint16_t)-hy - ((uint16_t)g.Yvel[o] >> 8));
        uint16_t ny = (uint16_t)(a + d1 - g.Ypos[o]);
        g.temp2b[o] = (int16_t)vtoa(nx, ny);
    }
    if (!g.position[o]) goto skate;
    {
        uint16_t h = vtoa((uint16_t)g.puckvx[0], (uint16_t)g.puckvy[0]);
        if (h != 8 && (h ^ 4) == g.facedir[o]) goto skate;
        uint16_t dy = (uint16_t)(g.pucky[0] - g.Ypos[o]);
        uint16_t dx = (uint16_t)(g.Xpos[14] - g.Xpos[o]);
        if (g.facedir[o] != vtoa(dx, dy)) goto skate;
        if (g.puckc < 0) goto skate;
        uint16_t a = dx >> 3;
        if (a & 0x1000) a |= 0xE000;
        uint16_t s = Mul8S(a, a);
        a = dy >> 3;
        if (a & 0x1000) a |= 0xE000;
        s = (uint16_t)(Mul8S(a, a) + s);
        x = a3;
        if (s >= 0xE && s < 0x28) { Sweepcheck(x); return; }
    }
skate:
    doplayeracc(x, (uint16_t)g.temp2b[x], a3);
}

/* (asswingo / asscentero: a new spot from the table row d2 (x, x spread, y, y spread) in temp3 /
 * temp4, unless the row is the same as last time and the random word is not small) */
static void off_spot(int o, uint32_t tab, uint16_t d2, uint16_t seed)
{
    if (d2 == (uint16_t)g.temp5[o] && seed >= 0x200) return;
    g.temp5[o] = (int16_t)d2;
    g.temp3[o] = (int16_t)(randomd0s(m16(tab + d2 + 2)) + m16(tab + d2));
    g.temp4[o] = (int16_t)(randomd0s(m16(tab + d2 + 6)) + m16(tab + d2 + 4));
}

/* (asswingo / asscentero: d2 = the zone of puck y d0 (lead d3) seen from the player's goal: 0 back,
 * 8 mid, 16 past the blue line (unless offside), 24 past the goal line) */
static uint16_t off_zone(uint16_t a2, uint16_t d0, uint16_t d3)
{
    if (NEG(d3, 0xFFA8)) return 0;
    if (NEG(d0, 0x58) || (r16(RAM_OFF_tmflags + a2) & tmfoside)) return 8;
    if (NEG(d3, 0x108)) return 16;
    return 0x18;
}

/* (asswingo / asscentero / asscenterd's start: on offense the carrier gets asspuckc ($11) on top
 * and the other team's puck makes it assignment def, on defense his own team's does; the clock
 * stopped: donothing; then check4bench; a human is left alone; the first time, the timers (and
 * with spots the last spot row) reset.
 * Nonzero: go on.) */
static int ass_start(int o, int a3, uint8_t def, int carrier, int spots)
{
    uint16_t pc = (uint16_t)g.puckc;
    if (carrier && 2 * o == g.puckc) { assinsert(o, 0x11); return 0; }
    if (!(pc & 0x8000) && (g.pfteam[o] == r16(RAM_OFF_pfteam + pc)) != carrier) { assreplace(o, def); return 0; }
    if (g.gmclock) { donothing(o, a3); return 0; }
    if (check4bench(o)) return 0;
    if (!(g.playerpad[o] & 0x8000)) return 0;
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        if (spots) g.temp5[o] = -1;
        g.temp1[o] = 0;
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
    }
    return 1;
}

/* (temp1 -= d7; nonzero when it ran out: then reloaded with add, at least 0) */
static int ass_timer(int o, uint16_t add)
{
    uint16_t t = (uint16_t)(g.temp1[o] - g.d7);
    g.temp1[o] = (int16_t)t;
    if (!(t & 0x8000)) return 0;
    t = (uint16_t)(t + add);
    g.temp1[o] = (int16_t)(t & 0x8000 ? 0 : t);
    return 1;
}

/* Assignment 4: a wing on offense skates (avoiding the carrier) to a spot of the puck's zone (puck
 * y led by its velocity), picked again as the zone changes or at random when his timer runs out. */
void asswingo(int o, uint16_t a2, int a3)
{
    if (!ass_start(o, a3, 3, 1, 1)) return;
    if (ass_timer(o, g.aidef[o])) {
        uint16_t d0 = (uint16_t)g.pucky[0];
        uint16_t l = (uint16_t)g.puckvy[0] >> 6;
        if (l & 0x200) l |= 0xFC00;
        uint16_t d3 = (uint16_t)(l + d0);
        if (!g.pfgoal[o]) { d0 = (uint16_t)-d0; d3 = (uint16_t)-d3; }
        off_spot(o, SYM_asswingo_9, off_zone(a2, d0, d3), g.seed[0]);
    }
    uint16_t d0 = (uint16_t)g.temp3[o];
    if (g.position[o] != 5) d0 = (uint16_t)-d0;
    uint16_t d1 = (uint16_t)g.temp4[o];
    if (!g.pfgoal[o]) { d0 = (uint16_t)-d0; d1 = (uint16_t)-d1; }
    skateto(o, d0, d1, SYM_EvadePC, a3);
}

/* Assignment 6: the center on offense, the same with his own spots (CenterOSpots) by the puck y. */
void asscentero(int o, uint16_t a2, int a3)
{
    if (!ass_start(o, a3, 5, 1, 1)) return;
    if (ass_timer(o, g.aidef[o])) {
        uint16_t d0 = (uint16_t)g.pucky[0];
        if (!g.pfgoal[o]) d0 = (uint16_t)-d0;
        off_spot(o, SYM_CenterOSpots, off_zone(a2, d0, d0), g.seed[1]);
    }
    uint16_t d1 = (uint16_t)g.temp4[o];
    if (!g.pfgoal[o]) d1 = (uint16_t)-d1;
    skateto(o, (uint16_t)g.temp3[o], d1, SYM_EvadePC, a3);
}

/* Assignment 5: the center on defense, halfway between the puck and his goal. */
void asscenterd(int o, int a3)
{
    if (!ass_start(o, a3, 6, 0, 0)) return;
    uint16_t t = (uint16_t)(g.temp1[o] - g.d7);
    g.temp1[o] = (int16_t)(t & 0x8000 ? g.aidef[o] : t);
    uint16_t d0 = (uint16_t)(g.Xpos[14] >> 1);
    uint16_t d1 = 0x80, py = (uint16_t)g.pucky[0];
    if (g.pfgoal[o]) {
        d1 = 0xFF80;
        if ((uint16_t)(0xFFA8 - py) & 0x8000) d1 = (uint16_t)((int16_t)(0xFF80 + py) >> 1);
    } else if (!((uint16_t)(0x58 - py) & 0x8000)) d1 = (uint16_t)((int16_t)(d1 + py) >> 1);
    skateto(o, d0, d1, SYM_rtss, a3);
}

/* Assignment $12: the goalie skates out to a loose puck (goalieflags bits 1-2 by where he is), while
 * his team is not near it and it does not come at his net fast; the leg strength doubled meanwhile.
 * Otherwise the assignment is over. */
void assgoalietopuck(int o, int a3)
{
    g.goalieflags[o] |= 6;
    uint16_t x = (uint16_t)g.Xpos[o], ay = (uint16_t)g.Ypos[o];
    if (ay & 0x8000) ay = (uint16_t)-ay;
    if (!NEG(x, 0xFFD8) && NEG(x, 0x29) && !NEG(ay, 0xE4) && NEG(ay, 0x104)) g.goalieflags[o] &= 0xFFFB;
    if (!NEG(x, 0xFFD0) && NEG(x, 0x31) && !NEG(ay, 0xE4)) g.goalieflags[o] &= 0xFFFD;
    if (!(g.playerpad[o] & 0x8000) || g.gmclock) { assexit(o); return; }
    if (check4bench(o)) return;
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp1[o] = 0;
        g.GoalieUnused = 0;
    }
    uint16_t t = (uint16_t)(g.temp1[o] - g.d7);
    g.temp1[o] = (int16_t)t;
    if (t & 0x8000) {
        t = (uint16_t)((g.aidef[o] >> 2) + t);
        g.temp1[o] = (int16_t)(t & 0x8000 ? 0 : t);
        if (!(g.puckc & 0x8000)) { assexit(o); return; }
        uint16_t v = (uint16_t)g.puckvy[0];
        if (g.pfgoal[o]) v = (uint16_t)-v;
        if (NEG(v, 0xF800)) { assexit(o); return; }
        if (g.tmnear[g.pfteam[o] ? 0 : 1] < 0x96) { assexit(o); return; }
    }
    uint16_t d0, d1;
    GoaliePuckVector(o, &d0, &d1);
    AdjustFacingDirection(o, vtoa(d0, d1));
    g.legstr[o] <<= 1;
    t = (uint16_t)(g.temp2[o] - g.d7);
    unsigned c = (uint16_t)g.temp2[o] >= g.d7;
    g.temp2[o] = (int16_t)t;
    if (t & 0x8000) {
        g.temp2[o] = (int16_t)(t + 5 + c);
        d0 = (uint16_t)((uint16_t)(int8_t)((uint16_t)g.puckvx[0] >> 8) + g.Xpos[14]);
        d1 = (uint16_t)((uint16_t)(int8_t)((uint16_t)g.puckvy[0] >> 8) + g.pucky[0]);
        avdgoal(o, &d0, &d1);
        int16_t hx, hy;
        GetHot(o, &hx, &hy);
        uint16_t nx = (uint16_t)((uint16_t)(int8_t)(uint8_t)((uint16_t)-hx - ((uint16_t)g.Xvel[o] >> 8)) + d0 - g.Xpos[o]);
        uint16_t ny = (uint16_t)((uint16_t)(int8_t)(uint8_t)((uint16_t)-hy - ((uint16_t)g.Yvel[o] >> 8)) + d1 - g.Ypos[o]);
        g.temp2b[o] = (int16_t)vtoa(nx, ny);
    }
    doplayeracc(o, (uint16_t)g.temp2b[o], a3);
    g.legstr[o] >>= 1;
}

/* Offsides, both teams: each team's flag cleared once it is back onside (d0 as given for the
 * first's empty spots); when the puck crosses a blue line ($54 from centre) StartBreakaway, and
 * with offsides on the team attacking that end (by the carrier, or the last player to touch it)
 * is flagged when one of its skaters is past the line ($5E). The bottom half: ChkOffsides_other. */
void ChkOffsides(uint16_t d0)
{
    d0 = ClearOffsidesIfAllPlayers(2, d0);
    ClearOffsidesIfAllPlayers(0, d0);
    uint16_t a1 = 2, a2 = 0;
    uint16_t py = (uint16_t)g.pucky[0], oy = (uint16_t)g.OldYpos[14];
    if (py & 0x8000) { ChkOffsides_other(0x54, a1, a2); return; }
    if (NEG(0x54, py) ? NEG(0x54, oy) != 0 : !NEG(0x54, oy)) return;
    StartBreakaway(0x54);
    if (!(g.gmode & gmoffs)) return;
    if (g.gmode & gmdir) { uint16_t t = a2; a2 = a1; a1 = t; }
    uint16_t h = (uint16_t)g.puckc;
    if (h & 0x8000) h = (uint16_t)(g.lastplayer[0] << 1);
    if (!NEG(h, 0xC) ? !a2 : a2 != 0) return;
    uint16_t a0 = r16(RAM_OFF_tmsort + a2);
    for (int d2 = 5; d2 >= 0; d2--, a0 += 2) {
        if (r16(RAM_OFF_position + a0) & 0x8000) continue;
        if (!((uint16_t)(0x5E - r16(RAM_OFF_Ypos + a0)) & 0x8000)) continue;
        w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) | tmfoside);
        return;
    }
}

/* ChkOffsides with the puck in the bottom half: the line d0 negated (the skaters' line $0A below d0 as
 * CheckBreakaway leaves it), the teams a1 / a2 swapped the other way. */
void ChkOffsides_other(uint16_t d0, uint16_t a1, uint16_t a2)
{
    d0 = (uint16_t)-d0;
    uint16_t py = (uint16_t)g.pucky[0], oy = (uint16_t)g.OldYpos[14];
    if (NEG(d0, py) ? NEG(d0, oy) != 0 : !NEG(d0, oy)) return;
    d0 = StartBreakaway(d0);                               /* (the line as CheckBreakaway leaves d0) */
    if (!(g.gmode & gmoffs)) return;
    d0 = (uint16_t)(d0 - 0xA);
    if (!(g.gmode & gmdir)) { uint16_t t = a2; a2 = a1; a1 = t; }
    uint16_t h = (uint16_t)g.puckc;
    if (h & 0x8000) h = (uint16_t)(g.lastplayer[0] << 1);
    if (!NEG(h, 0xC) ? !a2 : a2 != 0) return;
    uint16_t a0 = r16(RAM_OFF_tmsort + a2);
    for (int d2 = 5; d2 >= 0; d2--, a0 += 2) {
        if (r16(RAM_OFF_position + a0) & 0x8000) continue;
        if ((uint16_t)(d0 - r16(RAM_OFF_Ypos + a0)) & 0x8000) continue;
        w16(RAM_OFF_tmflags + a2, r16(RAM_OFF_tmflags + a2) | tmfoside);
        return;
    }
}

/* The little offsides referee: with offsides on, while a teammate of player o is past the blue
 * line ($5C) with the puck outside it, sflags bit 15 is set and the ref shown (PushRef 6, when no
 * ref is up); once none is, the flag cleared and the ref erased (unless a ref window is up). Returns
 * d2 as left: blankchar after the erase (eraser's argument), else d2 as given. */
uint16_t checkob(int o, uint16_t d2)
{
    if (!(g.gmode & gmoffs)) return d2;
    uint16_t a0 = 2 * o < 0xC ? 0 : 0xC;
    if (!g.pfgoal[o]) {
        uint16_t t = (uint16_t)(0xFFA4 - g.pucky[0]);
        if (t && !(t & 0x8000)) goto l3;
        for (int n = 5; n >= 0; n--, a0 += 2) {
            if (r16(RAM_OFF_position + a0) & 0x8000) continue;
            t = (uint16_t)(0xFFA4 - r16(RAM_OFF_Ypos + a0));
            if (t && !(t & 0x8000)) goto l6;
        }
    } else {
        if (NEG(0x5C, g.pucky[0])) goto l3;
        for (int n = 5; n >= 0; n--, a0 += 2) {
            if (r16(RAM_OFF_position + a0) & 0x8000) continue;
            if (NEG(0x5C, r16(RAM_OFF_Ypos + a0))) goto l6;
        }
    }
l3:
    if (!(g.sflags & 0x8000)) return d2;
    g.sflags &= 0x7FFF;
    if (g.refwindow) return d2;
    g.printx = 2;
    g.printy = 2;
    eraser(5, 5, g.blankchar);
    return g.blankchar;                                    /* (d0 and d1 kept, d2 left as eraser's) */
l6:
    if (g.sflags & 0x8000) return d2;
    g.sflags |= 0x8000;
    if (g.RefCnt < 0) PushRef(6);
    return d2;
}

/* A computer player o with the puck looks for a pass (1 in 2 or so without a threat): a random
 * teammate, available, ahead of him or not too far back (and on his side of the blue line with
 * offsides on), with no opponent closer to the puck in that direction. After the pass a skater's
 * assignment is over: returns nonzero, his caller ended too. */
int chk4pass(int o, int a3)
{
    if (!g.threat && randoma8((uint16_t)(0x10 + g.spodds[o])) >= 9) return 0;
    uint16_t d0 = randoma8(6);
    if (2 * o >= 0xC) d0 = (uint16_t)(d0 + 6);
    uint16_t px = (uint16_t)g.Xpos[14], py = (uint16_t)g.pucky[0];
    if (g.position[o] && !NEG(px, 0xFFD7) && NEG(px, 0x29) && (NEG(py, 0xFF33) || !NEG(py, 0xCD))) return 0;
    d0 = (uint16_t)(d0 << 1);
    if (2 * o == d0) return 0;
    uint16_t y = d0;
    uint16_t p = r16(RAM_OFF_position + y);
    if (!p || (p & 0x8000)) return 0;
    if ((r16(RAM_OFF_pflags + y) & 0x1000) | r16(RAM_OFF_pfalock + y)) return 0;
    uint16_t e0 = r16(RAM_OFF_Ypos + y), e1 = (uint16_t)g.Ypos[o];
    if (!g.pfgoal[o]) { e0 = (uint16_t)-e0; e1 = (uint16_t)-e1; }
    if ((g.gmode & gmoffs) && (((uint16_t)(e1 - 0x58) ^ (uint16_t)(e0 - 0x58)) & 0x8000)) return 0;
    if (NEG(e0, 0x58) && NEG((uint16_t)(e0 - e1), 0xFFF1)) return 0;
    g.passdir = (uint16_t)(r16(RAM_OFF_puckdir + y) - 4) & 7;
    uint16_t d2 = r16(RAM_OFF_puckdist2 + y);
    uint16_t z = 2 * o >= 0xC ? 0 : 0xC;
    for (int d3 = 5; d3 >= 0; d3--, z += 2)
        if (d2 >= r16(RAM_OFF_puckdist2 + z) && ((uint16_t)(r16(RAM_OFF_puckdir + z) - 4) & 7) == g.passdir) return 0;
    dopass(o, a3);
    if (!r16(RAM_OFF_position + g.curobj)) return 0;
    assexit(g.curobj >> 1);
    return 1;
}

/* Assignment $13: a computer shot: the first time it starts (SetShotMode); with shot mode over, the
 * assignment is over; meanwhile the swing timer temp2b counts down, releasing the shot button (d2
 * cbut) when it runs out (ShotMode, the pad direction from temp1). */
void assshoot(int o, int a3)
{
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        SetShotMode(o);
        return;
    }
    if (!(g.sflags & 8)) { assexit(o); return; }
    uint16_t d2 = 0;
    g.temp2b[o] = (int16_t)(g.temp2b[o] - g.d7);
    if (g.temp2b[o] < 0) d2 |= 0x80;
    ShotMode(o, (uint16_t)g.temp1[o], d2, a3);
}

/* Offsides on player a2 (a3 his partner in a2 / a3, swapped around the call as the original does):
 * with offsides on, the puck past the blue line in his attacking zone and his team flagged.
 * Returns d0 as left (the puck's y from his side, or the penalty). */
uint16_t a2offsides(uint16_t a2, uint16_t d0)
{
    if (!(g.gmode & gmoffs)) return d0;
    d0 = (uint16_t)g.pucky[0];
    if (!r16(RAM_OFF_pfgoal + a2)) d0 = (uint16_t)-d0;
    if (NEG(d0, blueline)) return d0;
    if (!(r16(RAM_OFF_tmflags + (r16(RAM_OFF_pfteam + a2) ? 2 : 0)) & tmfoside)) return d0;
    if (g.gmode & gmhl) return d0;
    AddPenalty(a2 >> 1, PenOffsides);
    return PenOffsides;
}

/* Player a2 touches the puck: the last toucher (ltx / lty / ltplayer) and his team's last three
 * touches (a new touch after the team's flag 4 raises the home crowd), then offsides; a called icing
 * (by the player iflags names, X = x around it) unless the touching team may touch it; otherwise
 * icing re-armed for him when the puck is in his half and his team is not short-handed. Returns a3
 * as left (x after an icing); *d0 (when given) as it was in, as left out (callers go on using it). */
int a2touchpuck(uint16_t a2, int a3, int x, uint16_t *d0p)
{
    uint16_t y = a2, dummy;
    if (!d0p) d0p = &dummy;
    if (y >= 0x18) return a3;
    g.ltx = r16(RAM_OFF_Xpos + y);
    w16(RAM_OFF_lty, r16(RAM_OFF_Ypos + y));
    g.ltplayer = y >> 1;
    uint16_t a0 = r16(RAM_OFF_pfteam + y) ? 2 : 0;
    uint16_t d0 = r16(RAM_OFF_pnum + y);
    int t = a0 >> 1;
    if (d0 != g.tmtouch1[t]) {
        if (g.tmflags[t] & 4) {
            g.tmflags[t] &= 0xFFFB;
            if (!a0) {
                uint16_t v = (uint16_t)(g.CwdExciteLvl + 0xA);
                if (!(v & 0x8000)) g.CwdExciteLvl = v;
            }
        } else {
            g.tmtouch3[t] = g.tmtouch2[t];
            g.tmtouch2[t] = g.tmtouch1[t];
        }
        g.tmtouch1[t] = d0;
        if (d0 == g.tmtouch3[t]) g.tmtouch3[t] = 0xFFFF;
    }
    g.sflags &= (uint16_t)~0x1000;
    *d0p = d0 = a2offsides(a2, d0);
    if ((g.iflags & 4) && (g.iflags & 1) && r16(RAM_OFF_position + y)) {
        uint16_t pg = r16(RAM_OFF_pfgoal + y);
        if ((g.iflags & 2) ? !pg : pg != 0) {
            AddPenalty((uint16_t)((g.iflags >> 8 & 0xFF) << 1) >> 1, PenIcing);
            *d0p = PenIcing;
            return x;
        }
    }
    g.iflags = (uint16_t)((y >> 1) << 8);
    d0 = (uint16_t)g.pucky[0];
    if (r16(RAM_OFF_pfgoal + y)) { g.iflags |= 2; d0 = (uint16_t)-d0; }
    *d0p = d0;
    if (d0 & 0x8000) return a3;
    *d0p = d0 = (uint16_t)(g.tmap[0] - g.tmap[1]);
    if (r16(RAM_OFF_pfteam + y)) *d0p = d0 = (uint16_t)-d0;
    if (d0 & 0x8000) return a3;
    g.iflags |= 4;
    return a3;
}

/* Assignment $15: waiting for a pass. Until the puck is there (passwait) and while it is loose:
 * a one-timer is asked for (the pad's shoot button, or at random for a computer player in the
 * attacking zone with no pads on his team) and the one-timer swing starts toward the goal; when
 * the stick meets the puck he takes it and shoots at once (a2touchpuck, doshot). Otherwise (the
 * wait over, the puck taken, the swing done) the assignment is over. */
void asspassrec(int o, uint16_t a2, int a3)
{
    if (g.pflags[o] & pfna) {
        g.pflags[o] = (uint16_t)((g.pflags[o] & 0xFFFD) | 1);
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
    }
    uint16_t pw = (uint16_t)(g.passwait[o] - g.timestep);
    g.passwait[o] = pw;
    if ((pw & 0x8000) || !(g.puckc & 0x8000)) goto over;
    if (!g.onetimerreq) {
        uint16_t y = g.playerpad[o];
        if (!(y & 0x8000)) {
            uint16_t pn = r16(RAM_OFF_padnew + y);
            if (pn & 0x8000) goto over;
            if (pn & 0x80) { g.onetimerreq++; goto swing; }
        }
        if (!r16(RAM_OFF_teampads + a2)
            && (g.pfgoal[o] ? !NEG(g.Ypos[o], 0x58) : NEG(g.Ypos[o], 0xFFA8)) && !(RandomWord() & 0x80)) {
            g.onetimerreq++;
            goto swing;
        }
        if (g.puckc & 0x8000) return;
        goto over;
    }
swing:
    if (g.SPA[o] != (uint16_t)SYM_SPAonetimerf && g.SPA[o] != (uint16_t)SYM_SPAonetimerb) {
        g.tmOneTimerSwings[NEG(2 * o, 0xC) ? 0 : 1]++;
        uint16_t d0 = (uint16_t)(0 - g.passtargetx);
        uint16_t d1 = (uint16_t)((g.pfgoal[o] ? 0x108 : 0xFEF8) - g.passtargety);
        d0 = vtoa(d0, d1);
        SetSPAFromA(o, (uint16_t)(Findhittype(o, &d0) ? SYM_SPAonetimerb : SYM_SPAonetimerf));
        return;
    }
    g.nopuck[o] = 0xA;
    if (g.SPAnum[o] >= 0x10) goto over;
    {
        int16_t hx, hy;
        GetHot(o, &hx, &hy);
        uint16_t d0 = (uint16_t)(g.Xpos[o] + hx - g.Xpos[14]);
        uint16_t d1 = (uint16_t)(g.Ypos[o] + hy - g.pucky[0]);
        uint16_t len = ApproxLength(d1, d0);
        if (len >= 8) {
            if ((g.puckc & 0x8000) || 2 * o == g.puckc) return;
            goto over;
        }
        g.SPAcnt[o] = 0;
        g.puckc = (int16_t)(2 * o);
        uint16_t y = g.playerpad[o];
        g.passdir = y & 0x8000 ? 8 : r16(RAM_OFF_paddir + y);
        g.passspeed = 0x24;
        g.pfalock[o] = 0x24;
        if (!g.gmclock) g.tmOneTimers[NEG(2 * o, 0xC) ? 0 : 1]++;
        g.onetimer = 1;
        uint16_t v = (uint16_t)(g.CwdExciteLvl + 0xA);
        if (!(v & 0x8000)) g.CwdExciteLvl = v;
        a3 = a2touchpuck((uint16_t)(2 * o), a3, o, 0);
        doshot(o, a3);
    }
over:
    g.passReceiverPlayerNum = 0xFFFF;
    g.pfalock[o] = 0;
    g.onetimerreq = 0;
    g.pflags[o] &= 0xFFFE;
    assexit(o);
}


/* (assbreakaway's .9: the next point of the path (x signed by its side), the distance for the one
 * after) */
static void sopath_next(void)
{
    uint32_t t = SYM_assbreakaway_10 + (uint16_t)(g.sopathpoint * 6);
    uint16_t d0 = m16(t);
    if (g.sopathside & 0x8000) d0 = (uint16_t)-d0;
    g.sopathx = d0;
    g.sopathy = m16(t + 2);
    g.sopathnext = m16(t + 4);
    g.sopathpoint++;
}

/* (assbreakaway's .11 / .4x: the side to go in on, at random or by his stick hand, signed by his
 * goal; close in (d0, his distance from his own end, past $2C) the path starts at its third point,
 * straight in when he is near the middle. The second roll is left in d0, which the caller compares
 * next.) */
static void sopath_start(int o, uint16_t *d0)
{
    uint16_t a;
    if (NEG(*d0, 0x2C)) {
        if (RandomWord() & 0xC0) a = g.attribute[o] & 0x0800 ? 0x2D : 0xFFD3;
        else {
            *d0 = RandomWord() & 0xF0;
            a = ((uint16_t)(0x7F - *d0) & 0x8000) ? 0xFFD3 : 0x2D;
        }
        if (!g.pfgoal[o]) a = (uint16_t)-a;
        g.sopathside = a;
        g.sopathx = a;
        g.sopathy = 0x28;
        g.sopathnext = 0x1E;
        return;
    }
    uint16_t px = (uint16_t)g.Xpos[o];
    if (!NEG(px, 0x20) || NEG(px, 0xFFE0)) {
        g.sopathside = px;
        g.sopathx = 0;
    } else {
        if (RandomWord() & 0xC0) a = g.attribute[o] & 0x0800 ? 0x2C : 0xFFD4;
        else {
            *d0 = RandomWord() & 0xF0;
            a = ((uint16_t)(0x7F - *d0) & 0x8000) ? 0xFFD4 : 0x2C;
        }
        if (!g.pfgoal[o]) a = (uint16_t)-a;
        g.sopathside = a;
        g.sopathx = a;
    }
    g.sopathpoint = 2;
    g.sopathy = 0x97;
    g.sopathnext = 0x86;
}

/* Assignment $10: a computer puck carrier on a breakaway skates the path to the net (no avoidance)
 * and, once his facing turns from the path's (at random), shoots a one-timer at the goal's side;
 * with the clock stopped he glides back. Anyone else's assignment is over. */
void assbreakaway(int o, int a3)
{
    if (2 * o != g.puckc || !(g.playerpad[o] & 0x8000)) { assexit(o); return; }
    if (g.gmclock) {
        GlideBack(o);
        g.nopuck[o] = 0x3C;
        donothing(o, a3);
        return;
    }
    uint16_t d0 = (uint16_t)g.Ypos[o];
    if (!g.pfgoal[o]) d0 = (uint16_t)-d0;
    if (g.pflags[o] & 2) {
        g.pflags[o] &= 0xFFFD;
        g.temp1[o] = 0;
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
        g.sopathpoint = 0;
        g.temp3[o] = (int16_t)(g.seed[0] & 3);
        sopath_start(o, &d0);
    }
    if (!(g.sopathnext & 0x8000)) {
        if (!NEG(d0, g.sopathnext)) {
            sopath_next();
            g.sopathface = g.facedir[o];
        }
    } else {
        uint16_t diff = (uint16_t)(g.facedir[o] - g.sopathface);
        if (diff && !(RandomWord() & (((uint16_t)(diff + 1) & 7) >= 3 ? 0x600 : 0xF80))) {
            uint16_t dir = (uint16_t)((RandomWord() & 1) + 2);
            if (g.sopathside & 0x8000) dir = (uint16_t)(dir + 3);
            g.passdir = dir;
            g.passspeed = 0x14;
            uint16_t e0 = (uint16_t)(0 - g.Xpos[o]);
            uint16_t e1 = (uint16_t)((g.pfgoal[o] ? 0x108 : 0xFEF8) - g.Ypos[o]);
            e0 = vtoa(e0, e1);
            SetSPAFromA(o, (uint16_t)(Findhittype(o, &e0) ? SYM_SPAonetimerb : SYM_SPAonetimerf));
            updateanim(o);
            g.SPAcnt[o] = 2;
            doshot(o, a3);
            return;
        }
    }
    uint16_t d1 = g.sopathy;
    if (!g.pfgoal[o]) d1 = (uint16_t)-d1;
    g.temp2[o] = (int16_t)(g.temp2[o] - g.impactstep);
    skateto(o, g.sopathx, d1, SYM_assbreakaway_nocallback, a3);
}

/* (Endfaceoff's .13: the player of team y0's six in position p, -1 for none) */
static uint16_t fo_find(uint16_t p, uint16_t y)
{
    for (int n = 6; n; n--, y = (uint16_t)(y + 2))
        if (p == r16(RAM_OFF_position + y)) return y;
    return 0xFFFF;
}

/* Drop the puck (a3, the puck object): the face off sound, the face off over and the clock going;
 * the winner picked at random by the two centres' pulls (fofdata2), the puck sent in his pad
 * direction (fodir1 / fodir2) or at random toward his side (and then 1 in 4 at random anyway); the
 * centre of the team it goes to touches it (a2touchpuck); the puck takes assignment $18. */
void Endfaceoff(int a3)
{
    sfx(7, 0x2400, 0);
    g.sflags &= (uint16_t)~0x100;
    g.fobox = 0;
    g.sflags &= (uint16_t)~0x400;
    g.replaydelay = 0;
    g.gmclock = 0;
    g.sflags3[0] |= 0x10;
    uint16_t d3 = g.fodir1, d4 = 0x800, a1 = 0, d2 = 0x10;
    d2 = (uint16_t)(-(uint16_t)(int8_t)m8((SYM_Endfaceoff_14 - 1 + g.fofdata2[0]) & 0xFFFFFF) + d2);
    d2 = (uint16_t)((uint16_t)(int8_t)m8((SYM_Endfaceoff_14 - 1 + g.fofdata2[2]) & 0xFFFFFF) + d2);
    if ((uint16_t)((RandomWord() & 0x1F) + 1) < d2) a1 = 4;
    if (r16(RAM_OFF_fofdata2 + 2 + a1) & 0x0800) { d3 = g.fodir2; d4 = 0xF800; }
    uint16_t d0;
    if (d3 & 8 || (d0 = d3 & 7, !(g.seed[0] & 3))) {
        d0 = (uint16_t)(randoma8(5) - 2) & 7;
        if (!(d4 & 0x8000)) d0 ^= 4;
    }
    uint32_t t = SYM_dirtab + (uint16_t)(d0 << 2);
    w16(RAM_OFF_Xvel + 2 * a3, (uint16_t)(rom16(t) << 5));
    w16(RAM_OFF_Yvel + 2 * a3, (uint16_t)((uint16_t)(rom16(t + 2) << 5) + d4));
    uint16_t y = fo_find(4, 0);
    if (!(y & 0x8000)) {
        int up = r16(RAM_OFF_pfgoal + y) != 0;
        if (g.puckvy[0] < 0 ? !up : up) y = fo_find(4, 0xC);
        if (!(y & 0x8000)) a2touchpuck(y, a3, a3, 0);
    }
    g.tmflags[0] &= 0xFFFB;
    g.tmflags[1] &= 0xFFFB;
    g.puckvz = (int16_t)(RandomWord() & 0x7FF);
    g.puckz = 0;
    w16(RAM_OFF_pfnc + 2 * a3, 0);
    assreplace(a3, 0x18);
}

/* Assignment 1: the player nearest the puck who does not have it (a goalie: assgoalie; the
 * computer carrier: asspuckc on top). On his timer (aioff), the team's nearest player (or a
 * teammate with the puck) not him, free and in play, takes assignment $12 on top and he leaves
 * his. A computer player goes after the puck (skatetopuck), or to the blue line when the goalie
 * has it, or, on a random roll on his checking (with the team flag 2 or the temp3 timer: harder
 * far from the puck, the power play's sides adjusting it) halfway back toward his goal, then
 * looks for a check. (d2 is kept in the direct page: the original reads it at .3 unset on one path,
 * as other code left it.) */
void assnearest(int o, uint16_t a2, int a3)
{
    uint16_t d0, d1, d2, a1, y;
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp3[o] = 0;
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
        g.temp1[o] = 0;
    }
    if (!g.position[o]) { assgoalie(o, a2, a3); return; }
    if (2 * o == g.puckc) {
        if (g.playerpad[o] & 0x8000) assinsert(o, 0x11);
        return;
    }
    {
        uint16_t t = (uint16_t)g.temp1[o];
        unsigned c = t >= g.d7;
        t = (uint16_t)(t - g.d7);
        g.temp1[o] = (int16_t)t;
        if (!(t & 0x8000)) goto l9;
        t = (uint16_t)(t + g.aioff[o] + c);
        g.temp1[o] = (int16_t)(t & 0x8000 ? 0 : t);
    }
    y = (uint16_t)g.puckc;
    if (!(y & 0x8000) && r16(RAM_OFF_position + y) && r16(RAM_OFF_pfteam + y) == g.pfteam[o]) a1 = y;
    else {
        w16(RAM_OFF_d2, r16(RAM_OFF_tmnear + a2));
        a1 = r16(RAM_OFF_tmnearpl + a2);
        if (g.d2[0] & 0x8000) goto l3;
    }
    if (2 * o != a1 && !g.gmclock && !(r16(RAM_OFF_pflags + a1) & 0x1000) && r16(RAM_OFF_position + a1)
        && r16(RAM_OFF_asscur + a1) != 0x15) {
        w16(RAM_OFF_pflags + a1, r16(RAM_OFF_pflags + a1) & 0xFFFE);
        assinsert(a1 >> 1, 0x12);
        assexit(g.curobj >> 1);
        return;
    }
l3:
    d1 = 2;
    d2 = g.d2[0];
    if (d2 < 0xE2) {
        d1 = 0;
        if (!g.position[o]) goto l9;
    }
    if (g.sflags & 0x2000) {
        d1 = (uint16_t)(d1 - 2);
        if (chkpk2(o)) d1 = (uint16_t)(d1 + 4);
    }
    d0 = (uint16_t)(0x14 - g.checking[o]);
    if (!(d1 & 0x8000)) d0 = (uint16_t)(d0 << d1);
    if (randomd0(d0) < 2) g.temp3[o] = 0xF0;
l9:
    if (!(g.playerpad[o] & 0x8000)) return;
    if (g.gmclock) { GlideBack(o); donothing(o, a3); return; }
    if (!(r16(RAM_OFF_tmflags + a2) & 2)) {
        if (g.puckc & 0x8000) goto l5;
        g.temp3[o] = (int16_t)(g.temp3[o] - g.d7);
        if (g.temp3[o] >= 0) goto l5;
        g.temp3[o] = 0;
    }
    {
        uint16_t s0, s1;
        skatetopuckinit(&s0, &s1);
        d0 = (uint16_t)-s0;
        d1 = (uint16_t)(-s1 + 0xF4);
        if (g.pfgoal[o]) d1 = (uint16_t)(d1 - 0x1E8);
        d0 = (uint16_t)(((int16_t)d0 >> 1) + s0);
        d1 = (uint16_t)(((int16_t)d1 >> 1) + s1);
        if (r16(RAM_OFF_tmflags + a2) & 2) {
            if (g.pfgoal[o]) { if (!((uint16_t)(d1 - 0x53) & 0x8000)) d1 = 0x44; }
            else if ((uint16_t)(d1 - 0xFFAD) & 0x8000) d1 = 0xFFBC;
        }
        skateto(o, d0, d1, SYM_rtss, a3);
        check4check(o, a3);
        return;
    }
l5:
    y = (uint16_t)g.puckc;
    if (!(y & 0x8000) && !r16(RAM_OFF_position + y)) {
        skateto(o, (uint16_t)g.Xpos[14], g.pfgoal[o] ? 0x58 : 0xFFA8, SYM_rtss, a3);
        check4check(o, a3);
        return;
    }
    skatetopuck(o, a3);
    check4check(o, a3);
}

/* The computer picks team a2's line (a1 the other team): with a man more or less, of lines 3 / 4
 * (short) or 5 / 6 (the power play), the one with more energy; at even strength (the home team
 * with a human on pad 1 and automatic changes, or the visitors: the second table) the first line
 * of a list (by the score in the third period) with energy left, the home team's matching the
 * other team's line when it has enough. */
void CompLine(uint16_t a2, uint16_t a1)
{
    uint16_t d0 = 3, d1;
    uint16_t m2 = r16(RAM_OFF_tmap + a2), m1 = r16(RAM_OFF_tmap + a1);
    uint32_t a0;
    if (m2 != m1) {
        if (NEG(m2, m1)) d0 = (uint16_t)(d0 + 2);
        uint16_t e = getlinee(a2, d0);
        if (NEG(e, getlinee(a2, (uint16_t)(d0 + 1)))) d0++;
        w16(RAM_OFF_tmline + a2, d0);
        return;
    }
    uint16_t s2 = r16(RAM_OFF_tmscore + a2), s1 = r16(RAM_OFF_tmscore + a1);
    if (!a2 && !(g.OptLine == 2 && !(g.c1playernum[0] & 0x8000))) {
        a0 = SYM_CompLine_5;
        if (g.gsp == 2 && s2 != s1) a0 = NEG(s2, s1) ? SYM_CompLine_5 + 28 : SYM_CompLine_5 + 14;
        d1 = (uint16_t)(r16(RAM_OFF_tmline + a1) << 1);
        if (getlinee(a2, rom16(a0 + d1)) >= 0xC01) {
            w16(RAM_OFF_tmline + a2, rom16(a0 + d1));
            return;
        }
    }
    a0 = SYM_CompLine_7;
    if (g.gsp == 2 && s2 != s1) a0 = NEG(s2, s1) ? SYM_CompLine_7 + 12 : SYM_CompLine_7 + 6;
    for (int n = 2; n > 0; n--, a0 += 2)                 /* (the third is taken anyway) */
        if (getlinee(a2, rom16(a0)) >= 0xF34) break;
    w16(RAM_OFF_tmline + a2, rom16(a0));
}

/* The computer pulls team a2's goalie (a1 the other team) in the third period, behind by one or
 * two with under a minute ($3D) left and the puck (y d1) in its attacking half: tmgoalie's high
 * byte $FF, then setpersonel. */
void CPgoalie(uint16_t a2, uint16_t a1, uint16_t d1)
{
    if (g.gsp != 2) return;
    uint16_t b = (uint16_t)(r16(RAM_OFF_tmscore + a1) - r16(RAM_OFF_tmscore + a2));
    if ((b & 0x8000) || !b || b >= 3) return;
    if (g.gameclock[0] >= 0x3D) return;
    if (!r16(RAM_OFF_pfgoal + r16(RAM_OFF_tmsort + a2))) d1 = (uint16_t)-d1;
    if (d1 & 0x8000) return;
    w16(RAM_OFF_tmgoalie + a2, r16(RAM_OFF_tmgoalie + a2) | 0xFF00);
    setpersonel(a2);
}

/* (ReturnGoalies' .1: team a2's pulled goalie back, unless a pad is on the team: then CPgoalie
 * with the faceoff spot's y) */
static void return_goalie(uint16_t a2, uint16_t a1)
{
    uint16_t t = r16(RAM_OFF_tmgoalie + a2);
    if (!(t & 0x8000)) return;
    t &= 0xFF;
    if (t >= 0xA) return;
    w16(RAM_OFF_tmgoalie + a2, t);
    if (r16(RAM_OFF_teampads + a2)) return;
    CPgoalie(a2, a1, g.foy);
}

/* At a faceoff each team's pulled goalie comes back (for a computer team, unless CPgoalie pulls
 * him again). */
void ReturnGoalies(void)
{
    return_goalie(0, 2);
    return_goalie(2, 0);
}

/* (ChkGoalies' .1: team a2 with the puck pulls its goalie on a delayed penalty, or a computer
 * team by CPgoalie) */
static void chk_goalie(uint16_t a2, uint16_t a1)
{
    if (r16(RAM_OFF_tmgoalie + a2) & 0x8000) return;
    uint16_t x = r16(RAM_OFF_tmsort + a2), y = (uint16_t)g.puckc;
    if (y & 0x8000) return;
    if (r16(RAM_OFF_pfteam + y) != r16(RAM_OFF_pfteam + x)) return;
    if (g.gmode & 8) {
        w16(RAM_OFF_tmgoalie + a2, r16(RAM_OFF_tmgoalie + a2) | 0xFF00);
        setpersonel(a2);
        return;
    }
    if (r16(RAM_OFF_teampads + a2)) return;
    CPgoalie(a2, a1, (uint16_t)g.pucky[0]);
}

/* With the clock running, each team may pull its goalie (chk_goalie). */
void ChkGoalies(void)
{
    if (g.gmclock) return;
    chk_goalie(0, 2);
    chk_goalie(2, 0);
}

/* (pucknorm_move's .16: where and in how many frames the puck crosses the goal line d4 (the entry
 * at y of puckcross: x clamped to the sidelines, frames; $FFFF frames when it moves away or
 * not at all); returns d0 as left) */
static uint16_t puck_cross(uint16_t d4, uint16_t y)
{
    uint16_t d0 = (uint16_t)(d4 - g.pucky[0]), vy = (uint16_t)g.puckvy[0];
    if (!vy || ((vy ^ d0) & 0x8000)) {
        w16(RAM_OFF_puckcross + 2 + y, 0xFFFF);
        return d0;
    }
    uint16_t q = Div32S((uint32_t)((int32_t)(int16_t)d0 << 12), vy);
    w16(RAM_OFF_puckcross + 2 + y, q);
    cf = q >> 12 & 1;                                     /* (the last asl's carry, kept by a zero product) */
    uint16_t hi = (uint16_t)(Mul16XS((uint16_t)(q << 4), (uint16_t)g.puckvx[0]) >> 16);
    d0 = (uint16_t)(hi + g.Xpos[14] + cf);                /* (adc: the multiply's carry) */
    if (!NEG(d0, sideline)) d0 = sideline;
    if (NEG(d0, 0xFF78)) d0 = 0xFF78;
    w16(RAM_OFF_puckcross + y, d0);
    return d0;
}

/* The puck (x) moves: the goal-line crossing times run down, and every 5 frames they are worked
 * out again (puck_cross); a carried puck is touched and follows his stick (a quarter of the way
 * to his hot spot, his velocity); icing and offsides; with the clock running, during a penalty
 * shot a puck going back toward the shooter's end for $30 frames ends it, and a puck that has not
 * moved for $78 frames is out of play (PenOOP); a puck at rest on the ice stops spinning; then its
 * collisions (checkpuckcoll). d0 and a0 are the direct page's as the caller left them (d0 for
 * ChkOffsides' empty spots, a0 for puckstick: a2touchpuck leaves the carrier's team there). */
void pucknorm_move(int x, int a3, uint16_t d0, uint16_t a0)
{
    g.puckcross[1] = (uint16_t)(g.puckcross[1] - g.d7);
    g.puckcross[3] = (uint16_t)(g.puckcross[3] - g.d7);
    {
        uint16_t t = (uint16_t)g.temp1[14];
        unsigned c = t >= g.d7;
        t = (uint16_t)(t - g.d7);
        g.temp1[14] = (int16_t)t;
        if (t & 0x8000) {
            g.temp1[14] = (int16_t)(t + 5 + c);
            puck_cross(0x108, 0);
            d0 = puck_cross(0xFEF8, 4);
        }
    }
    if (!(g.puckc & 0x8000)) {
        uint16_t a2 = (uint16_t)g.puckc;
        if (a2 < 0x18) a0 = r16(RAM_OFF_pfteam + a2) ? 2 : 0;
        a3 = a2touchpuck(a2, a3, x, &d0);
        int16_t hx, hy;
        GetHot(a2 >> 1, &hx, &hy);
        d0 = (uint16_t)hx;
        uint16_t t = (uint16_t)((uint16_t)(hx + r16(RAM_OFF_Xpos + a2) - g.Xpos[14]) >> 2);
        if (t & 0x2000) t |= 0xC000;
        g.Xpos[14] = (int16_t)(t + g.Xpos[14]);
        t = (uint16_t)((uint16_t)(hy + r16(RAM_OFF_Ypos + a2) - g.pucky[0]) >> 2);
        if (t & 0x2000) t |= 0xC000;
        g.pucky[0] = (int16_t)(t + g.pucky[0]);
        g.puckvx[0] = (int16_t)r16(RAM_OFF_Xvel + a2);
        g.puckvy[0] = (int16_t)r16(RAM_OFF_Yvel + a2);
    }
    puckIChk();
    ChkOffsides(d0);
    if (g.gmclock) goto l13;
    if (!(g.puckc & 0x8000)) { g.pspuckback = 0; goto l12; }
    if (g.PenShotActive) {
        int back;
        if ((g.gmode & 2) ? g.BA_Team != 0 : g.BA_Team == 0) back = !((uint16_t)(g.puckvy[0] - 1) & 0x8000) ? 0 : 1;
        else back = (g.puckvy[0] & 0x8000) ? 0 : 1;
        if (back) {
            g.pspuckback++;
            if (g.pspuckback < 0x30) goto l10;
            EndPenaltyShot(x);
        }
    }
    g.pspuckback = 0;
l10:
    if (g.Xpos[14] == g.OldXpos[14] && g.pucky[0] == g.OldYpos[14]) {
        g.temp2b[14]--;
        if (g.temp2b[14] < 0) AddPenalty2(x, PenOOP);
        goto l13;
    }
l12:
    g.temp2b[14] = 0x78;
l13:
    if (!(((uint16_t)g.puckvz & 0xFF00) | (uint16_t)g.puckz)) pucknothing();
    checkpuckcoll(x, a3, a0);
}

/* Assignment $18 (the puck most of the time): the first time, its timers reset (still puck $78,
 * the penalty shot's count back); then pucknorm_move. */
void pucknorm(int x, int a3, uint16_t d0, uint16_t a0)
{
    if (g.pflags[14] & 2) {
        g.pflags[14] &= 0xFFFD;
        g.temp1[14] = 0;
        g.temp2[14] = 0;
        g.temp2b[14] = 0x78;
        g.pspuckback = 0;
    }
    pucknorm_move(x, a3, d0, a0);
}


/* A computer carrier o (a3 after setpersonel) in his own end ($58 from his goal) changes lines on
 * 1 in 4 (line changes on, not automatic; no change under way, his line tired: under $C01, no
 * penalty shot): CompLine, setpersonel, the scoreboard, then he shoots the puck in (CompShoot:
 * returns nonzero, his caller is ended too). */
int chk4lc(int o, int a3)
{
    if (g.OptLine == 2) return 0;
    uint16_t d0 = (uint16_t)g.pucky[0];
    if (!g.pfgoal[o]) d0 = (uint16_t)-d0;
    if ((d0 & 0x8000) || !((uint16_t)(d0 - 0x58) & 0x8000)) return 0;
    if (g.seed[0] & 3) return 0;
    if (g.tmflags[g.pfteam[o] ? 1 : 0] & 8) return 0;
    uint16_t a2 = g.pfteam[o] ? 2 : 0, a1 = a2 ^ 2;
    if (AvgCline(a2) >= 0xC01 || g.PenShotActive || !g.OptLine) return 0;
    CompLine(a2, a1);
    setpersonel(a2);
    printscores1();
    CompShoot(a3);
    return 1;
}

/* (asspuckc's .chkdir, a skateto callback: when an opponent (led by his velocity) will be within
 * $15 of where the puck goes (threat counted), the direction away from him, turned a step at
 * random when that is right behind player o; otherwise d0's low byte as it was) */
static uint16_t asspuckc_chkdir(int o, uint16_t d0)
{
    d0 = (uint16_t)(int8_t)d0;
    uint16_t d2 = (uint16_t)((uint16_t)(int8_t)((uint16_t)g.Xvel[o] >> 8) + g.Xpos[14]);
    uint16_t d3 = (uint16_t)((uint16_t)(int8_t)((uint16_t)g.Yvel[o] >> 8) + g.pucky[0]);
    g.threat = 0;
    uint16_t y = 2 * o >= 0xC ? 0 : 0xC;
    for (int d4 = 5; d4 >= 0; d4--, y += 2) {
        uint16_t t = (uint16_t)((uint16_t)(int8_t)(r16(RAM_OFF_Xvel + y) >> 8) + r16(RAM_OFF_Xpos + y) - d2);
        if (NEG(t, 0xFFEB) || !NEG(t, 0x15)) continue;
        t = (uint16_t)((uint16_t)(int8_t)(r16(RAM_OFF_Yvel + y) >> 8) + r16(RAM_OFF_Ypos + y) - d3);
        if (NEG(t, 0xFFEB) || !NEG(t, 0x15)) continue;
        g.threat++;
        d0 = vtoa((uint16_t)(g.Xpos[o] - r16(RAM_OFF_Xpos + y)), (uint16_t)(g.Ypos[o] - r16(RAM_OFF_Ypos + y)));
        if ((g.facedir[o] ^ 4) == d0) d0 = (uint16_t)((g.seed[1] & 1) + d0) & 7;
    }
    return d0;
}

/* Assignment $11: the puck carrier (anyone else, or a human, leaves it). With the clock stopped
 * he glides back. A computer carrier on his timer (aioff): the offsides ref, on a breakaway close
 * in ($90) assbreakaway ($10); else a line change, a shot or a pass may end it; then he skates to
 * his spot of PuckcSpots (by the random temp3, or his position with a teammate offside), avoiding
 * opponents (.chkdir). (temp2b is set for object 0, not him, as the original does.) */
void asspuckc(int o, int a3)
{
    if (2 * o != g.puckc || !(g.playerpad[o] & 0x8000)) { assexit(o); return; }
    if (g.gmclock) { GlideBack(o); donothing(o, a3); return; }
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp1[o] = 0;
        g.temp2[o] = 0;
        g.temp2b[0] = 8;
        g.temp3[o] = (int16_t)(g.seed[0] & 3);
    }
    {
        uint16_t t = (uint16_t)g.temp1[o];
        unsigned c = t >= g.d7;
        t = (uint16_t)(t - g.d7);
        g.temp1[o] = (int16_t)t;
        if (t & 0x8000) {
            t = (uint16_t)(t + g.aioff[o] + c);           /* (adc: the carry of the sbc) */
            g.temp1[o] = (int16_t)(t & 0x8000 ? 0 : t);
            checkob(o, 0);
            if (g.breakaway && NEG(abs16((uint16_t)g.Ypos[o]), 0x90)) { assreplace(o, 0x10); return; }
            if (chk4lc(o, a3) || chk4shot(o)) return;
            if (g.asscur[o] == 0x11 && chk4pass(o, a3)) return;
        }
    }
    uint16_t k = (uint16_t)((g.sflags & 0x8000 ? g.position[o] : (uint16_t)(6 + g.temp3[o])) << 2);
    uint16_t d0 = m16(SYM_PuckcSpots + k), d1 = m16(SYM_PuckcSpots + k + 2);
    if (!g.pfgoal[o]) { d0 = (uint16_t)-d0; d1 = (uint16_t)-d1; }
    skateto(o, d0, d1, SYM_asspuckc_chkdir, a3);
}

/* The puck drop of player o's faceoff (temp1 the timer): while it runs, the drop frame 10 - (timer
 * + 6) / 8 into fofdata in its last steps; when it is over the faceoff box is erased (its print
 * position by fodropx / fodropy; the penalty shot's own box) and the frame counts restart. Returns
 * d0 as left. */
uint16_t updatefaceoff(int o)
{
    uint16_t d0, d1;
    if (g.temp1[o]) {
        d0 = (uint16_t)((uint16_t)(g.temp1[o] + 6) >> 3);
        if (!NEG(d0, 3)) return d0;
        d0 = (uint16_t)(10 - d0);
        g.fofdata[0] = d0;
        return d0;
    }
    if (!g.penshot) {
        print(SYM_updatefaceoffPos);
        uint16_t t = (uint16_t)((uint16_t)(g.fodropx - 0x2E) >> 3);
        if (t & 0x1000) t |= 0xE000;
        g.printx = t;
        t = (uint16_t)((uint16_t)(g.fodropy - 0x44) >> 3);
        if (t & 0x1000) t |= 0xE000;
        g.printy = t;
        d0 = 0xC;
        d1 = 0xD;
    } else {
        g.printy = 1;
        g.printx = 6;
        d0 = 0x14;
        d1 = 8;
    }
    eraser(d0, d1, g.blankchar);
    g.FrameCount = 0;
    g.gameframes[0] = 0;
    return d0;
}


/* (puckfaceoff2: a player's faceoff spot, d1 his position: from the table .19 (by how many his team
 * has on the ice; the penalty shot's .20) into the offsets .21, his end's way round; the skaters'
 * near the faceoff spot pulled toward the middle) */
static void fo_spot(int o, uint16_t d1, uint16_t *px, uint16_t *py)
{
    uint16_t d4 = r16(RAM_OFF_tmap + (g.pfteam[o] ? 2 : 0));
    d4 = (uint16_t)((uint16_t)(6 - d4) << 3);
    uint32_t a1 = (SYM_puckfaceoff2_19 & 0xFF0000) | (uint16_t)(SYM_puckfaceoff2_19 + d4);
    if (g.penshot) a1 = SYM_puckfaceoff2_20;
    d4 = (uint16_t)((m8((a1 + d1) & 0xFFFFFF)) << 2);
    uint16_t d0 = m16(SYM_puckfaceoff2_21 + d4);
    d1 = m16(SYM_puckfaceoff2_21 + d4 + 2);
    if (!g.pfgoal[o]) { d0 = (uint16_t)-d0; d1 = (uint16_t)-d1; }
    if (g.position[o]) {
        if (NEG(d4, 9)) {
            if ((g.fox ^ d0) & 0x8000) {
                uint16_t t = g.foy >> 3;
                if (t & 0x1000) t |= 0xE000;
                d1 = (uint16_t)(-t + d1);
            }
            uint16_t d3 = g.fox >> 2;
            if (d3 & 0x2000) d3 |= 0xC000;
            d0 = (uint16_t)(d0 - d3);
        }
        d0 = (uint16_t)(d0 + g.fox);
        d1 = (uint16_t)(d1 + g.foy);
    }
    *px = d0; *py = d1;
}

/* (puckfaceoff2: every pad of team t but the one kept (keep) moved off it) */
static void pads_off(uint16_t t, uint16_t keep)
{
    for (int x = 8; x >= 0; x -= 2) {
        uint16_t s = r16(RAM_OFF_padside + x);
        if (s != t || x == keep) continue;
        w16(RAM_OFF_padside + x, s | 0x8000);
        w16(RAM_OFF_cont1team + x, 0);
    }
}

/* Assignment $17 (the puck at a faceoff), object o, a3 the puck object. The first time: the faceoff
 * sounds and music, the flags of play reset, the puck at the faceoff spot (fox / foy) and the nets
 * in place, the rink scrolled there, both teams' players on the ice (the centres take assignment
 * $17, the others $16) at their spots facing the puck, the penalty shot's pads; the faceoff box
 * drawn (the penalty shot's: shooter versus goalie; with line changes the two lines' names), the
 * drop time and the faceoff frames set. Then the drop time counts down (updatefaceoff) and the
 * puck drops (Endfaceoff; a penalty shot set up after it). */
void puckfaceoff2(int o, int a3)
{
    if (!(g.pflags[o] & pfna)) {
        uint16_t tmp0 = NEG(g.d7, g.temp1[o]) ? g.d7 : 1;
        g.temp1[o] = (int16_t)(g.temp1[o] - tmp0);
        if (g.temp1[o] >= 0) { updatefaceoff(o); return; }
        if (g.penshot) {
            Endfaceoff(a3);
            SetupPenaltyShot();
            return;
        }
        g.PenShotActive = 0;
        g.BA_Sktr_SCnum = 0xFFFF;
        Endfaceoff(a3);
        return;
    }
    g.pflags[o] &= 0xFFFD;
    g.SfxVelocity = 0x7000;
    QueueSound(0x12);
    QueueSound(0x13);
    g.recsfx = 0xFFFF;
    g.recsfx2 = 0xFFFF;
    g.hattrick = 0;
    if (g.Crowdlevel >= 0x258) g.Crowdlevel = 0x258;
    if (g.brightness) {
        FadeOut();
        WaitFadeOut();
        g.brightness = 0;
    }
    FadeIn();
    g.fadestep = 0xA;
    if (!g.MusicOn) {
        uint16_t r3 = g.PBnum >> 4, lo = g.PBnum & 0xF;
        if (g.PBnum && lo != r3) PlayTeamSong(lo > r3 ? 1 : 4);
        else if (g.percue || (g.OptPlayMode != 4 && !(RandomWord() & 3))) {
            PlayTeamSong(g.percue ? 0 : 2);
            g.percue = 0;
        }
    }
    g.sflags &= 0xFFF2;
    g.sflags3[0] &= 0xFFFE;
    g.iflags = 0;
    g.replaydelay = 0;
    g.RefCnt = -1;
    g.puckcross[1] = 0xFFFF;
    g.puckcross[3] = 0xFFFF;
    g.puckz = -1;
    g.passReceiverPlayerNum = 0xFFFF;
    g.puckc = (int16_t)0xFFFE;
    g.sflags &= 0x7DFF;
    g.refwindow = 0;
    g.penmsgbox = 0;
    g.sflags |= 0x500;
    ClrHor();
    g.Vpos = 0;
    g.Hpos = 0;
    g.Xpos[14] = (int16_t)g.fox;
    g.pucky[0] = (int16_t)g.foy;
    g.puckvx[0] = 0;
    g.puckvy[0] = 0;
    g.puckvz = 0;
    g.Xvel[12] = 0; g.Yvel[12] = 0; g.Xpos[12] = 0; g.Ypos[12] = 0x10C;
    g.Xvel[13] = 0; g.Yvel[13] = 0; g.Xpos[13] = 0; g.Ypos[13] = (int16_t)0xFEF4;
    g.frame[15] = SPFpuck;
    g.SPA[15] = 0;
    g.attribute[15] = 0;
    g.attribute[14] = 0;
    g.sflags &= 0xFFBF;
    for (int d4 = 0x64; d4 >= 0; d4--) checkwindow();
    g.yleader = 0x3C;
    ResetBench();
    setpersonel(0);
    forcepldata(0);
    setpersonel(2);
    forcepldata(2);
    resetplstuff();
    g.tmtouch2[0] = g.tmtouch3[0] = g.tmtouch2[1] = g.tmtouch3[1] = 0xFFFF;
    for (int x = 0; x < 0x18; x += 2) {
        int p = x >> 1;
        g.pfgoal[p] = (uint16_t)(((((g.pfteam[p] >> 1) ^ g.gmode) & 2) >> 1) ^ 1);
        uint16_t idx = (uint16_t)(g.pnum[p] << 1);
        if (x >= 0xC) idx = (uint16_t)(idx + 0x34);
        uint16_t st = r16(RAM_OFF_tmpdst + idx);
        if (!(st & 0x8000) && !(st & 0x1000)) continue;
        g.Xpos[p] = (int16_t)0xFF10;
        g.Ypos[p] = 0;
        g.frame[p] = 1;
        uint16_t d1 = g.position[p];
        if (d1 & 0x8000) continue;
        if (d1) {
            if (g.penshot) g.pnum[g.BA_Team ? 10 : 4] = g.BA_Skater_Offset;
            uint8_t d0 = 0x16;
            if (d1 == 4) {
                uint16_t a2 = g.pfteam[p] ? 2 : 0;
                w16(RAM_OFF_tmtouch1 + a2, g.pnum[p] & 0xFF);
                w16(RAM_OFF_tmflags + a2, 4);
                d0 = 0x17;
            }
            assinsert(p, d0);
        }
        uint16_t sx, sy;
        fo_spot(p, d1, &sx, &sy);
        g.Xpos[p] = (int16_t)sx;
        g.Ypos[p] = (int16_t)sy;
        g.Xvel[p] = 0;
        g.Yvel[p] = 0;
        g.facedir[p] = vtoa((uint16_t)-(uint16_t)(sx - g.Xpos[14]), (uint16_t)-(uint16_t)(sy - g.pucky[0]));
        g.pflags[p] &= 0xEFFF;
        g.pfalock[p] = 0;
        g.SPA[p] = 0;
        SetSPAFromA(p, g.position[p] ? (uint16_t)SYM_SPAglide : (uint16_t)SYM_SPAgready);
        updateanim(p);
    }
    SprSort();
    if (g.penshot) {
        pads_off(g.BA_Team, g.pspad[0]);
        uint16_t d0 = g.BA_Team ^ 2;
        if (r16(RAM_OFF_teampads + d0)) {
            uint16_t y = r16(RAM_OFF_puckpadnext + d0);
            do {
                y += 2;
                if (y == 0xA) y = 0;
            } while (r16(RAM_OFF_padside + y) != d0);
            w16(RAM_OFF_puckpadnext + d0, y);
            pads_off(d0, y);
        }
    }
    FaceoffPads(a3);
    print(SYM_puckfaceoff2Pos);
    uint16_t d0 = (g.fox & 0x8000) ? 0xBE : 0x36;
    g.fodropx = d0;
    g.printx = (uint16_t)(d0 - 0x2E) >> 3;
    g.fodropy = 0x5C;
    g.printy = 3;
    {
        uint16_t py = g.printy, cols = m16(SYM_FaceOffMap), rows = m16(SYM_FaceOffMap + 2);
        unsigned c = py >> 11 & 1;                        /* (the last asl's carry) */
        marktextlo((uint16_t)((uint16_t)(py << 5) + g.printx + c));
        uint16_t t = (uint16_t)(py + rows - 1);
        c = t >> 11 & 1;
        uint32_t s = (uint32_t)(uint16_t)(t << 5) + g.printx + c;
        s = (s & 0xFFFF) + cols + (s >> 16);
        marktexthi((uint16_t)(s - 1));
        SeatPenaltyBox();
        g.fobox = (uint16_t)(SYM_FaceOffMap + 6);
        if (!g.penshot) {
            uint16_t x = (uint16_t)(((uint16_t)(g.printy << 5) + g.printx + (g.printy >> 11 & 1)) << 1), y = 0;
            for (uint16_t r = rows; r; r--) {
                for (uint16_t k = cols; k; k--, x += 2, y += 2) {
                    uint32_t v = (uint32_t)m16((0x9A0000u | (uint16_t)(SYM_FaceOffMap + 6)) + y) + g.faceoffvrcset;
                    w16(RAM_OFF_textmap + x, (uint16_t)(v + g.charofs + (v >> 16)));   /* (adc: the first add's carry) */
                }
                uint32_t a = (uint32_t)x + 0x40;
                uint16_t b = (uint16_t)a;
                unsigned borrow = b < cols;
                b = (uint16_t)(b - cols);
                x = (uint16_t)(b - cols - borrow);
            }
        } else {
            g.printy = 1;
            g.printx = 6;
            Framer(0x14, 8);
            g.printy = 5;
            uint32_t a1 = PlayerInitialName(g.BA_Team, g.BA_Skater_Offset);
            g.printx = (uint16_t)(((uint16_t)(0x14 - m16(a1)) >> 1) + 7);
            print(a1);
            g.printy++;
            g.printx = 0xF;
            print(SYM_VersusText);
            g.printy++;
            uint16_t a2 = g.BA_Team ^ 2;
            a1 = PlayerInitialName(a2, r16(RAM_OFF_tmgoalie + a2));
            g.printx = (uint16_t)(((uint16_t)(0x14 - m16(a1)) >> 1) + 7);
            print(a1);
            g.printx = 7;
            g.printy = 2;
            printbig(g.shootoutmode ? SYM_ShootoutPenShot : SYM_ShootoutPenShot + 10);
        }
    }
    if (g.OptLine && !(g.penshot | g.penshotlive)) {
        g.printy += 0xA;
        Framer(0xC, 3);
        uint16_t l0 = g.tmline[0], l1 = g.tmline[1];
        if (!(g.gmode & gmdir)) { l0 = g.tmline[1]; l1 = g.tmline[0]; }
        g.printx++;
        g.printy -= 2;
        PrintStringFromList(l0, SYM_linelist);
        g.printx += 4;
        PrintStringFromList(l1, SYM_linelist);
    }
    {
        uint16_t r = RandomWord();
        unsigned c = ((uint32_t)g.seed[0] + g.seed[1]) >> 16;   /* (RandomWord's last adc) */
        g.temp1[a3] = (int16_t)((r & 0x7F) + 0xB4 + c);
    }
    if (g.penshot) g.temp1[a3] = 0xB4;
    g.fofdata2[0] = 1;
    g.fofdata2[1] = 0x8000;
    g.fofdata[1] = 0x8000;
    g.fofdata2[2] = 4;
    g.fofdata2[3] = 0xA800;
    g.fofdata[0] = 7;
    if (!(g.gmode & gmdir)) {
        g.fofdata2[1] = 0x8800;
        g.fofdata2[3] = 0xA000;
    }
    g.fodir1 = 0xFFFF;
    g.fodir2 = 0xFFFF;
}

/* (puckfaceoff's .4: the computer picks team a2's line (a1 the other), puts it on, the scoreboard) */
static void fo_compline(uint16_t a2, uint16_t a1)
{
    CompLine(a2, a1);
    setpersonel(a2);
    printscores1();
}

/* (puckfaceoff's .1: with line changes on and no pad on team d0 (1 home, 2 visitors), fo_compline) */
static void fo_line(uint16_t a2, uint16_t a1, uint16_t d0)
{
    if (!g.OptLine) return;
    for (int k = 0; k < 5; k++) if (d0 == r16(RAM_OFF_cont1team + 2 * k)) return;
    fo_compline(a2, a1);
}

/* (puckfaceoff's .5 / .6: automatic changes: the computer's line for team a2; else player p (an
 * object * 2) opens his team's box, and when it is open its timer (home temp1 / temp3 of the puck
 * a3 $258, visitors temp2 / temp4 $168) starts with him as its owner) */
static void fo_pad_line(uint16_t a2, uint16_t a1, uint16_t p, int a3)
{
    if (g.OptLine == 2) { fo_compline(a2, a1); return; }
    w16(RAM_OFF_lcpad + (r16(RAM_OFF_pfteam + p) ? 2 : 0), 0xFFFF);
    SetLCmode(p >> 1);
    if (!(r16(RAM_OFF_tmflags + (r16(RAM_OFF_pfteam + p) ? 2 : 0)) & 8)) return;
    if (r16(RAM_OFF_pfteam + p)) {
        g.temp2[a3] = 0x168;
        g.temp4[a3] = (int16_t)(p >> 1);
    } else {
        g.temp1[a3] = 0x258;
        g.temp3[a3] = (int16_t)(p >> 1);
    }
}

/* (puckfaceoff's .10-.12: the player of the first pad of team a2 (c1playernum by padside)) */
static uint16_t fo_pad_player(uint16_t a2)
{
    uint16_t y = 0xFFFF;
    for (uint16_t k = 0; k < 0xA; k += 2)
        if (r16(RAM_OFF_padside + k) == a2) { y = k; break; }
    return r16(RAM_OFF_c1playernum + y);
}

/* (puckfaceoff's .t1xxxxxxx: the line change box timer of the puck a3 at d0 (0: temp1 / temp3 for
 * the home team, $20: temp2 / temp4 for the visitors): its owner's box closed meanwhile: the timer
 * stopped and the box drawn again; run out: the box still open makes his choice row 0 (lcfound) and
 * draws it again) */
static void fo_box_timer(int a3, uint16_t d0)
{
    uint16_t y = (uint16_t)(2 * a3 + d0);
    uint16_t t = r16(RAM_OFF_temp1 + y);
    if (t & 0x8000) return;
    uint16_t d1 = (uint16_t)(r16(RAM_OFF_temp3 + y) << 1);
    uint16_t a2 = r16(RAM_OFF_pfteam + d1) ? 2 : 0;
    if (!(r16(RAM_OFF_tmflags + a2) & 8)) {
        w16(RAM_OFF_temp1 + y, t | 0xFF00);
        SetLCmode2(a2);
        return;
    }
    t = (uint16_t)(t - g.d7);
    w16(RAM_OFF_temp1 + y, t);
    if (!(t & 0x8000)) return;
    if (!(r16(RAM_OFF_tmflags + a2) & 8)) return;
    lcfound(a2, d1 >> 1, 0);
    SetLCmode2(loadTeamStruct(d1 >> 1));
}

/* Assignment $1C (the puck before a faceoff), object o, a3 the puck object (d0 and a0 as the caller
 * left them, for pucknorm_move). In the shootout's whistle the turn is over. The puck moves (loose).
 * The first time: the period or game over (PeriodOver, overtime decided: clockcont_0), a delayed
 * penalty (Stop4Pen); else the goalies back, and with line changes on the boxes reset and the line
 * changes of the faceoff: the pad players open their boxes (automatic: the computer's lines), the
 * computer's line for the visitors with no pad. Then while a box's timer runs it waits; when none
 * runs, the home computer line too, and the assignment becomes $1C. (a0 for Stop4Pen as it is
 * here, not as pucknorm_move's collisions may leave it.) */
void puckfaceoff(int o, int a3, uint16_t d0, uint16_t a0)
{
    g.CrowdLoud = 0;
    if (g.shootoutmode && g.shootoutstop) { g.shootoutturnover++; return; }
    g.puckc = -1;
    pucknorm_move(o, a3, d0, a0);
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        if (!g.gameclock[0] || (g.gmode & 0x40)) { PeriodOver(); return; }
        if (g.gsp == 3 && g.tmscore[0] != g.tmscore[1]) { clockcont_0(); return; }
        if (g.gmode & 8) { Stop4Pen(a0); return; }
        ReturnGoalies();
        g.temp1[o] = -1;
        g.temp2[o] = -1;
        if (g.OptLine && !(g.penshot | g.penshotlive)) {
            g.lcpad[0] = g.lcpad[1] = 0xFFFF;
            g.tmflags[0] &= 0xFFF7;
            g.tmflags[1] &= 0xFFF7;
            if ((g.cont1team | r16(RAM_OFF_cont1team + 2) | r16(RAM_OFF_cont1team + 4) | r16(RAM_OFF_cont1team + 6)
                 | r16(RAM_OFF_cont1team + 8)) && g.sfhormode) {
                g.sflags |= 0x40;
                ClrHor();
            }
            if (!g.newperiod && g.teampads[0]) fo_pad_line(0, 2, (uint16_t)((fo_pad_player(0) >> 1) << 1), a3);
            if (!g.newperiod && g.teampads[1]) fo_pad_line(2, 0, (uint16_t)((fo_pad_player(2) >> 1) << 1), a3);
            g.newperiod = 0;
            fo_line(2, 0, 2);
        }
    }
    g.penshotlive = 0;
    fo_box_timer(a3, 0);
    fo_box_timer(a3, 0x20);
    if (g.temp1[a3] >= 0 || g.temp2[a3] >= 0) return;
    fo_line(0, 2, 1);
    assreplace(a3, 0x1C);
}
