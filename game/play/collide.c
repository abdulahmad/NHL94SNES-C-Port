/* collide: collisions, checks and the players' energy (src/routines/collide). */
#include "game.h"

/* The energy of player o (getpde leaves the team, nonzero for the visitors, in a2 and the
 * player number * 2 in d1: setpde's arguments) */
uint16_t getpde(int o)
{
    return g.tmpde[(g.pfteam[o] ? 26 : 0) + (g.pnum[o] & 0xFF)];
}

/* Set the energy of player num2 / 2 of team (nonzero: the visitors) to energy, 0 when negative */
void setpde(int team, uint16_t num2, uint16_t energy)
{
    w16(RAM_OFF_tmpde + (uint16_t)((team ? 0x34 : 0) + num2), energy & 0x8000 ? 0 : energy);   /* (num2 as given: by address) */
}

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */

static uint16_t abs16(uint16_t v) { return v & 0x8000 ? (uint16_t)-v : v; }

/* Player o's first assignment, from his position (none off the ice). */
void Setplass(int o)
{
    uint16_t p = (uint16_t)g.position[o];
    if (p & 0x8000) return;
    assreplace(o, m8((SYM_Setplass_1 + p) & 0xFFFFFF));
}

/* The period (gsp, in bits 14-15) and the time into it, for the goal and penalty logs. */
uint16_t GetPeriodTimeRemaining(void)
{
    return (uint16_t)((uint16_t)((g.gsp & 0xFF) << 14 | g.PerTimeTotal) - g.gameclock[0]);
}

/* Rating a scaled by the energy of player o (a0) (product / $1000); the carry is left clear. */
uint16_t makepde(uint16_t a, int o)
{
    uint32_t p = Mul16S(a, getpde(o));
    cf = 0;
    return (uint16_t)(p >> 12);
}

/* Player o's penalty roll: random below (20 - aggress) * 16, doubled on a pad, halved within 40 of
 * the puck. */
uint16_t checkagr(int o)
{
    uint16_t d0 = (uint16_t)((uint16_t)(0x14 - g.aggress[o]) << 4);
    if (!(g.playerpad[o] & 0x8000)) d0 = (uint16_t)(d0 << 1);
    uint16_t v = (uint16_t)(g.Xpos[o] - g.Xpos[14]);
    if (!NEG(v, 0xFFD7) && NEG(v, 0x29)) {
        v = (uint16_t)(g.Ypos[o] - g.pucky[0]);
        if (!NEG(v, 0xFFD7) && NEG(v, 0x29)) d0 >>= 1;
    }
    return randomd0(d0);
}

/* Roster player d0 of team a2 into slot d4 of the list at a4 if he is available (on the ice, on the
 * bench or not yet there, and not in the list): returns him (Z clear), or 0 (*d1 = 0). */
uint16_t TryAddPlayerToList(uint16_t d0, uint16_t d4, uint16_t a2, uint32_t a4, uint16_t *d1)
{
    *d1 = d0;
    uint16_t k = (uint16_t)(d0 - 1);
    uint16_t idx = (uint16_t)((a2 ? 0x34 : 0) + (uint16_t)(k << 1) + (k >> 15));   /* (asl's carry into the adc) */
    uint16_t st = r16(RAM_OFF_tmpdst + idx);
    if (st && !(st & 0x8000)) goto no;
    if (st == TMPDOUT || st == rosterNone) goto no;
    for (int e = 5; e >= 0; e--)
        if (m8((a4 + e) & 0xFFFFFF) == (d0 & 0xFF)) goto no;
    {
        uint32_t a = (a4 + d4) & 0xFFFFFF;
        if (is_wram(a)) w8(wram_off(a), (uint8_t)d0);
    }
    return d0 & 0xFF;
no:
    *d1 = 0;
    return 0;
}

/* (ResetBench for team a0 = 0 / 2 with box count unit d1: every player counted, and taken off
 * again when on the ice or the bench (who goes to the bench) or injured with no time left) */
static void rb(uint16_t a0, uint16_t d1)
{
    for (int d0 = 0x32; d0 >= 0; d0 -= 2) {
        uint32_t s = (uint32_t)g.PBnum + d1;
        g.PBnum = (uint16_t)s;
        uint16_t idx = (uint16_t)((a0 ? 0x34 : 0) + d0 + (s >> 16));   /* (the carry of the add) */
        uint16_t st = r16(RAM_OFF_tmpdst + idx);
        if ((st & 0x8000) || !st) {
            g.PBnum = (uint16_t)(g.PBnum - d1);
            if (st == TMPDOUT || st == rosterNone) continue;
            w16(RAM_OFF_tmpdst + idx, TMPDBENCH);
        } else if ((st & TMPDINJ) && !(st & 0x07FF)) {
            g.PBnum = (uint16_t)(g.PBnum - d1);
        }
    }
}

/* Empty the penalty boxes and put every player on his own bench. */
void ResetBench(void)
{
    g.PBnum = 0;
    rb(0, 0x10);
    rb(2, 1);
}

/* The puck is deflected: loose, a random velocity (x, y within +-$1000, height up to $FFF), spun. */
void deflect(uint16_t d0)
{
    g.puckc = -1;
    g.puckvy[0] = (int16_t)((RandomWord() & 0x1FFF) - 0x1000);
    g.puckvx[0] = (int16_t)((RandomWord() & 0x1FFF) - 0x1000);
    g.puckvz = (int16_t)(RandomWord() & 0xFFF);
    puckflip(d0);
}


/* (setplayer's rating: 3 bits scaled by 5/2) */
static uint16_t sp_scale(uint16_t n) { return (uint16_t)((n << 2) + n) >> 1; }

/* (setplayer's .8: rating a plus the player's hot / cold byte for stat k (/ 4), 0-15, in 8 bits) */
static uint16_t sp_adj(uint16_t a, uint16_t r3, uint16_t k)
{
    uint8_t v = (uint8_t)((g.HotColdTable[(uint16_t)(k + r3)] >> 2) + (uint8_t)a);
    if (v & 0x80) v = 0;
    if (v >= 0x0F) v = 0x0F;
    return v;
}

/* Bring roster player d3 onto the ice as object o: off the injured list, out of the box (assignment
 * $0B) or off the bench ($0A), on the ice; his 12 ratings from his roster record with the power
 * play, penalty kill, home / visitor and late third period bonuses (none for a goalie) and his hot /
 * cold bytes. */
void setplayer(int o, uint16_t d3)
{
    g.pflags[o] &= (uint16_t)~pf2inj;
    uint16_t a0 = g.pfteam[o] ? 2 : 0;
    g.pnum[o] = d3 & 0xFF;
    d3 = (uint16_t)((d3 & 0xFF) << 1);
    uint16_t y = (uint16_t)((a0 ? 0x34 : 0) + d3);
    uint16_t d1 = r16(RAM_OFF_tmpdst + y);
    uint16_t r3 = (uint16_t)(y * 6);
    if (!(d1 & 0x8000) || d1 == TMPDBENCH) {
        assinsert(o, d1 & 0x8000 ? 0xA : 0xB);
        g.pfalock[o] = 0;
        g.SPA[o] = 0;
    }
    w16(RAM_OFF_tmpdst + (uint16_t)((a0 ? 0x34 : 0) + d3), TMPDICE);
    d3 >>= 1;
    uint32_t bank = SYM_TeamList >> 16;
    uint16_t team = r16(RAM_OFF_tmdata + a0);
    uint16_t b = (uint16_t)(m16((bank << 16 | team) + 8) + team);   /* (the bonus bytes) */
    g.PPBonus = 0; g.PKBonus = 0; g.ThirdPBonus = 0; g.HmAwBonus = 0;
    if (g.position[o]) {
        if (g.sflags & 0x2000) {
            uint16_t w = m16((bank << 16 | b) + 1);
            if (!chkpk2(o)) g.PPBonus = w & 0xF;
            else g.PKBonus = (int16_t)-(w >> 4 & 0xF);
        }
        uint16_t w = m16((bank << 16 | b) + 2);
        g.HmAwBonus = g.pfteam[o] ? (uint16_t)-(w & 0xF) : (w >> 4 & 0xF);
        uint16_t p = (uint16_t)(g.gsp - 2);
        int bonus = 0;
        if (!(p & 0x8000)) {
            int score = 1;
            if (!p) {
                uint16_t v = (uint16_t)((GetPeriodTime() >> 1) - g.gameclock[0]);
                score = !v || (v & 0x8000);
            }
            if (score) {
                uint16_t t = (uint16_t)(g.tmscore[0] - g.tmscore[1]);
                if (t && g.pfteam[o]) t = (uint16_t)-t;
                bonus = !t || (t & 0x8000);
            }
        }
        if (bonus) g.ThirdPBonus = 2;
    }
    uint16_t a = (uint16_t)(m16(bank << 16 | team) + team);   /* (the roster records) */
    do {
        uint32_t s = (uint32_t)m16(bank << 16 | a) + a;
        a = (uint16_t)(s + 8 + (s >> 16 & 1));           /* (the first add's carry into the second) */
    } while (!((d3 = (uint16_t)(d3 - 1)) & 0x8000));
    a = (uint16_t)(a - 8);
#define B(k) m16((bank << 16 | a) + (k))
    w16(RAM_OFF_rostnum + 2 * o, m8(bank << 16 | a));
    uint16_t pp = g.PPBonus, pk = (uint16_t)g.PKBonus, ha = g.HmAwBonus, tp = g.ThirdPBonus;
    g.weight[o] = (B(1) & 0xF0) >> 1;
    g.legstr[o] = sp_adj(sp_scale(B(1) & 7), r3, 0);
    g.legspd[o] = sp_adj(sp_scale(B(2) >> 4 & 7), r3, 1);
    g.aioff[o] = (uint16_t)((sp_adj((uint16_t)(sp_scale(B(2) & 7) + pp + pk + ha + tp), r3, 2) ^ 0xF) + 0xF) >> 1;
    uint16_t t = sp_adj((uint16_t)(sp_scale(B(3) >> 4 & 7) + ha), r3, 3) ^ 0xF;
    g.aidef[o] = g.position[o] ? (uint16_t)(t + 7) : (uint16_t)(t + 0xF) >> 1;
    g.shotspd[o] = sp_adj(sp_scale(B(3) & 7), r3, 4);
    g.checking[o] = sp_adj((uint16_t)(sp_scale(B(4) >> 4 & 7) + tp), r3, 5);
    g.attribute[o] &= 0xF7FF;
    g.handed[o] = (B(4) & 1) ^ 1;
    if (!g.handed[o]) g.attribute[o] |= 0x0800;
    g.fighting[o] = B(4) & 0xE;
    g.stickhand[o] = sp_adj((uint16_t)(sp_scale(B(5) >> 4 & 7) + pp + pk + ha), r3, 6);
    g.shotacc[o] = sp_adj((uint16_t)(sp_scale(B(5) & 7) + pp + pk + ha), r3, 7);
    g.endurance[o] = sp_adj(sp_scale(B(6) >> 4 & 7), r3, 8);
    g.spodds[o] = sp_adj((uint16_t)(sp_scale(B(6) & 7) + tp + tp), r3, 9);
    g.passacc[o] = sp_adj((uint16_t)(sp_scale(B(7) >> 4 & 7) + pp + ha), r3, 10);
    g.aggress[o] = sp_adj(sp_scale(B(7) & 7), r3, 11);
#undef B
}

/* At a face off: team a2's six players take their new positions (newpos, a byte) straight away,
 * with their assignments (and a centre the face off one, $12), and their new roster players
 * (newpnum) on the ice. */
void forcepldata(uint16_t a2)
{
    uint16_t a3 = r16(RAM_OFF_tmsort + a2);
    for (int d4 = 5; d4 >= 0; d4--, a3 += 2) {
        int o = a3 >> 1;
        uint16_t p = (uint16_t)(int8_t)(uint8_t)g.newpos[o];
        g.position[o] = (int16_t)p;
        if (!(p & 0x8000)) {
            Setplass(o);
            if (g.position[o] == 4) assinsert(o, 0x12);
            uint16_t d3 = (uint16_t)(((uint16_t)g.newpnum[o] & 0xFF) << 1);
            w16(RAM_OFF_tmpdst + (uint16_t)((a2 ? 0x34 : 0) + d3), TMPDICE);
            setplayer(o, d3 >> 1);
        }
        g.newpnum[o] = -1;
        g.newpos[o] = -1;
    }
}


/* Player a2 is injured: pf2inj, the crowd louder (held 60 frames) and groaning (stream 4), the screen
 * locked on him, his roster slot out of the game (1 in 8 rosterNone, TempPlOffset bit $4000), who it
 * is in TempPlOffset. */
void setInjuryType(uint16_t a2)
{
    w16(RAM_OFF_pflags + a2, r16(RAM_OFF_pflags + a2) | 0x1000);
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x12C);
    g.crowdhold = 0x3C;
    StreamSfx(4);
    g.xc1 = (int16_t)r16(RAM_OFF_Xpos + a2);
    g.yc1 = (int16_t)r16(RAM_OFF_Ypos + a2);
    g.sflags |= 0x40;
    int team = r16(RAM_OFF_pfteam + a2) != 0;
    g.TempPlOffset = (uint16_t)((r16(RAM_OFF_pnum + a2) & 0xFF) | (team ? 0x8000 : 0));
    uint16_t v = (uint16_t)(int8_t)(uint8_t)g.TempPlOffset;
    uint16_t idx = (uint16_t)((team ? 0x34 : 0) + (uint16_t)(v << 1) + (v >> 15));   /* (asl's carry into the adc) */
    w16(RAM_OFF_tmpdst + idx, TMPDOUT);
    if (randoma8(7)) return;
    w16(RAM_OFF_tmpdst + idx, rosterNone);
    g.TempPlOffset |= 0x4000;
}


#define ASR(v, n) ((uint16_t)((int16_t)(v) >> (n)))   /* (lsr n, then the sign put back) */
#define HI(p) ((uint16_t)((p) >> 8))                  /* (r2+1: the product / 256) */

/* Object o bounces off a wall of angle cos / sin (kept in Wallcos / Wallsin): its velocity split into
 * v1n (into the wall) and v1t (along it), each cut, rotated back into a3's velocity. A puck hitting
 * hard pops up and spins (sound $0A, 4 when low); a player hitting hard plays the boards ($0D). Not
 * moving into it: nothing. A loose puck ends a penalty shot. */
void wallcoll(int o, uint16_t cos, uint16_t sin, int a3)
{
    w16(RAM_OFF_Wallcos + 2 * o, cos);
    w16(RAM_OFF_Wallsin + 2 * o, sin);
    uint16_t vx = r16(RAM_OFF_Xvel + 2 * o), vy = r16(RAM_OFF_Yvel + 2 * o);
    uint16_t d0 = HI(Mul16x8S(vy, cos));
    uint16_t d2 = (uint16_t)-(uint16_t)(d0 - HI(Mul16x8S(vx, sin)));
    d0 = HI(Mul16x8S(vx, cos));
    uint16_t d3 = (uint16_t)(HI(Mul16x8S(vy, sin)) + d0);
    int x = o;
    if (o == 14) {
        g.sflags &= (uint16_t)~0x1000;
        if (!(d2 & 0x8000)) goto done;
        d2 = ASR(d2, 2);
        if (NEG(d2, 0xFC00)) {
            uint16_t z = (uint16_t)-(RandomWord() & 0x7FF);
            g.puckvz = (int16_t)z;
            puckflip(z);
            PuckSfx((uint16_t)g.puckz >= 0xB ? 0xA : 4);
        }
        uint16_t a = ASR(d3, 6);
        d3 = (uint16_t)(d3 - a - ASR(a, 1));
    } else {
        if (!NEG(d2, 0x3E8)) goto done;
        if (NEG(d2, 0xF000) && !NEG(g.impact[o], 0xA)) sfx(0xD, (uint16_t)g.Xvel[o], (uint16_t)g.Yvel[o]);
        d2 = ASR(d2, 2);
        if (!NEG(d2, 0xFC7C)) d2 = 0xFC18;
    }
    x = a3;
    w16(RAM_OFF_Xvel + 2 * x, (uint16_t)(HI(Mul16x8S(d3, cos)) - HI(Mul16x8S(d2, sin))));
    w16(RAM_OFF_Yvel + 2 * x, (uint16_t)(HI(Mul16x8S(d2, cos)) + HI(Mul16x8S(d3, sin))));
    if (!(g.puckvz & 0x8000)) g.puckvz = 0;
done:
    if (x != 14 || !(g.puckc & 0x8000)) return;
    EndPenaltyShotPlay(14);
    g.breakaway = 0;
    g.onetimer = 0;
}

/* The puck is out of play: the screen locked, the puck out of collisions (pfnc = $40), flipped at
 * the bottom end, its shadow gone; with the clock running the last toucher's shootout result
 * (ShootoutWonBy 6); a penalty shot over. */
void wallcollb_out(void)
{
    g.sflags |= 0x40;
    g.pfnc[14] = 0x40;
    if (g.pucky[0] < 0) g.attribute[14] |= 0x8000;
    g.frame[15] = 0;
    if (!g.gmclock) ShootoutWonBy((uint16_t)(g.ltplayer << 1) >> 1, 6);   /* (the last toucher) */
    EndPenaltyShotPlay(14);                                   /* (X the puck: wallcollb comes here only for it) */
    g.breakaway = 0;
    g.onetimer = 0;
}

/* Object o hits a wall (cos / sin, a3 for the bounce): a high puck, or a fast one rising into the
 * net's top behind the goal, goes over the glass (the shadow's glass animation, stream 6, the crowd)
 * and out of play; anything else bounces. */
void wallcollb(int o, uint16_t cos, uint16_t sin, int a3)
{
    uint16_t z = (uint16_t)g.puckz, px = (uint16_t)g.Xpos[14];
    if (o != 14) goto bounce;
    if (!NEG(z, 0x21)) goto out;
    if (z < 0x16) goto bounce;
    if (NEG(g.pucky[0], 0x112)) goto out;
    if (NEG(px, 0xD) || !NEG(px, 0x10) || NEG(g.puckvy[0], 0xFA0)) goto bounce;
    g.puckvy[0] = (int16_t)(g.puckvy[0] >> 1);
    SetSPA(15, (uint16_t)SYM_SPAbglass);
    StreamSfx(6);
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x258);
    g.crowdhold = 0x5A;
    uint16_t v = (uint16_t)(g.CwdExciteLvl + 0xF);
    if (!(v & 0x8000)) g.CwdExciteLvl = v;
out:
    wallcollb_out();
    return;
bounce:
    wallcoll(o, cos, sin, a3);
}


/* (checkgoalp: a cosine / sine of the bounce off the goal kept to -$FF .. $FF) */
static uint16_t clamp_ff(uint16_t v)
{
    if (!NEG(v, 0xFF)) v = 0xFF;
    if (NEG(v, 0xFF01)) v = 0xFF01;
    return v;
}

/* Player o (a3 from the first test on) runs into goal a2 at d2 / d3 (wcradius around it): close to
 * its back he bounces off it (wallcoll, the angle from the goal's centre). A bursting player hitting
 * it fast at the front sometimes moves the net (half his speed again, he stops, the screen locks)
 * and with the clock running gets a goalie-holding penalty (PenGhold), which ends this. */
void checkgoalp(int o, uint16_t a2, uint16_t d2, uint16_t d3, int a3)
{
    if ((g.pflags[o] & 0x8000) || 2 * o >= 0x17) return;
    uint16_t y = a2;
    if (y != 0x18) { if (!NEG((uint16_t)(d3 - g.wcradiusy), 0xFF16)) return; }
    else if (NEG((uint16_t)(d3 + g.wcradiusy), 0xEA)) return;
    uint16_t a = d2 & 0x8000 ? (uint16_t)-d2 : d2;
    if (!NEG((uint16_t)(a - g.wcradiusx), 0x40)) return;
    uint16_t dy = (uint16_t)(d3 - r16(RAM_OFF_Ypos + y));
    uint16_t d0 = (uint16_t)-dy;
    uint16_t d1 = (uint16_t)(d2 - r16(RAM_OFF_Xpos + y));
    uint16_t v = Mul8S(d1, d1) >> 2;
    if (v >= 0x101) return;
    if ((uint16_t)((uint16_t)(Mul8S(dy, dy) << 1) + v) >= 0x101) return;
    int x = a3;
    if (!(g.seed[0] & 0x1C0) && g.SPA[x] == (uint16_t)SYM_SPAburst) {
        uint16_t py = (uint16_t)(g.pucky[0] - r16(RAM_OFF_Ypos + y));
        if (!NEG(py, 0xFF80) && NEG(py, 0x80)) {
            uint16_t e0 = (uint16_t)g.Xvel[x], e1 = (uint16_t)g.Yvel[x];
            if (NEG(e1, 0xDFFF) || !NEG(e1, 0x2001) || NEG(e0, 0xDFFF) || !NEG(e0, 0x2001)) {
                e0 = ASR(e0, 3);
                e0 = (uint16_t)(ASR(e0, 1) + e0);
                w16(RAM_OFF_Xvel + y, e0);
                e1 = ASR(e1, 3);
                e1 = (uint16_t)(ASR(e1, 1) + e1);
                w16(RAM_OFF_Yvel + y, e1);
                g.Xvel[x] = 0;
                g.Yvel[x] = 0;
                g.sflags |= 0x40;
                if (!g.gmclock) { AddPenalty2(x, PenGhold); return; }
                d0 = e0; d1 = e1;
            }
        }
    }
    uint16_t len = (uint16_t)(ApproxLength(d1, dy) + 1);
    uint16_t c = clamp_ff(DivX8S((uint16_t)((d0 & 0xFF) << 8), len));
    uint16_t s = clamp_ff(DivX8S((uint16_t)((d1 & 0xFF) << 8), len));
    wallcoll(a3, c, s, a3);
}


/* (checkgoal: the puck's speed on one axis made at least $800 either way) */
static int16_t at_least_800(int16_t v)
{
    if (!NEG((uint16_t)v, 0x800) || NEG((uint16_t)v, 0xF800)) return v;
    return v < 0 ? (int16_t)0xF800 : 0x800;
}

/* (a coordinate's move this frame from the old position, in 1/256: (pos:frac - old) >> 8) */
static uint16_t move_hi(uint32_t pos, uint32_t frac, uint32_t opos, uint32_t ofrac)
{
    uint16_t f = (uint16_t)(r16(frac) - r16(ofrac));
    unsigned b = r16(frac) < r16(ofrac);
    uint16_t i = (uint16_t)(r16(pos) - r16(opos) - b);
    return (uint16_t)((i & 0xFF) << 8 | f >> 8);
}

/* Object o meets goal a2 at d2 / d3 (a3 as for wallcoll): players by checkgoalp. A low puck (not
 * held by a goalie) inside the net's box: over the top when it was high before; into the net from
 * the front (Goal), unless it was going out or came in own-goal-wise or along the goal line, which
 * makes a save off the post (the shot counted, the crowd, a random rebound); off the side or back
 * (its speed along at least $800) a bounce. */
void checkgoal(int o, uint16_t a2, uint16_t d2, uint16_t d3, int a3)
{
    if (o != 14) { checkgoalp(o, a2, d2, d3, a3); return; }
    uint16_t y = (uint16_t)g.puckc;
    if (!(y & 0x8000) && !r16(RAM_OFF_position + y)) return;
    if (!NEG(g.puckz, 0xD)) return;
    y = a2;
    d2 = (uint16_t)(d2 - r16(RAM_OFF_Xpos + y));
    if (!NEG(d2, 0x17) || NEG(d2, 0xFFEA)) return;
    uint16_t d5 = 9;
    d3 = (uint16_t)(d3 - r16(RAM_OFF_Ypos + y));
    if (!NEG(d3, 0xA)) return;
    d5 = (uint16_t)-d5;
    if (NEG(d3, 0xFFF7)) return;
    g.collflag = 0xFFFF;
    if (!NEG(g.puckoldz, 0xD)) {
        g.puckz = g.puckoldz;
        if (g.puckvz & 0x8000) g.puckvz = (int16_t)-g.puckvz;
        return;
    }
    g.pfgoal[14] = 0;
    if (!(g.puckc & 0x8000)) {
        uint16_t c = (uint16_t)g.puckc;
        g.puckc = -1;
        w16(RAM_OFF_nopuck + c, 8);
        uint16_t d1 = (uint16_t)g.pucky[0];
        if (!r16(RAM_OFF_pfgoal + c)) d1 = (uint16_t)-d1;
        if (d1 & 0x8000) g.pfgoal[14] = 1;
    }
    int x = o;
    uint16_t d0 = 0xFF01, p0 = 0xFF01;
    uint16_t d1 = move_hi(RAM_OFF_Ypos + 2 * o, RAM_OFF_Yfrac + 2 * o, RAM_OFF_OldYpos + 2 * o, RAM_OFF_OldYfrac + 2 * o);
    if (!d1) goto side;
    if (!(d1 & 0x8000)) { d0 = 0xFF; p0 = 0xFF; d5 = (uint16_t)-d5; }
    d5 = (uint16_t)(d5 + d3);
    {
        uint16_t mx = move_hi(RAM_OFF_Xpos + 2 * o, RAM_OFF_Xfrac + 2 * o, RAM_OFF_OldXpos + 2 * o, RAM_OFF_OldXfrac + 2 * o);
        uint32_t p = Mul16S(mx, d5);
        x = a3;
        uint16_t r1 = d1 & 0x8000 ? (uint16_t)-d1 : d1;
        uint16_t h = (uint16_t)(p >> 16);
        if (h & 0x8000) h = (uint16_t)-h;
        if (h >= r1) goto side;
        d5 = Div32S(p, d1);
        d2 = (uint16_t)(d2 - d5);
        if (NEG(d2, 0xFFEA) || !NEG(d2, 0x17)) goto side;
    }
    d1 = 0;
    if (!(((uint16_t)g.pucky[0] ^ p0) & 0x8000)) {
        d0 = (uint16_t)-d0;
        if (!g.pfgoal[14]) {
            if (g.puckz != 0xD && NEG(d2, 0x12) && !NEG(d2, 0xFFEF)) { Goal(a3); return; }
            ChkShotStat();
            g.breakaway = 0;
            g.onetimer = 0;
            PuckSfx(9);
            g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x12C);
            g.crowdhold = 0x3C;
            uint16_t v = (uint16_t)(g.CwdExciteLvl + 0x28);
            if (!(v & 0x8000)) g.CwdExciteLvl = v;
            uint16_t r = RandomWord() & 0xFFF;
            if (!(g.pucky[0] & 0x8000)) r = (uint16_t)-r;
            g.puckvy[0] = (int16_t)r;
            g.puckvx[0] = (int16_t)((RandomWord() & 0x1FFF) - 0x1000);
            g.puckvz = (int16_t)((RandomWord() & 0x1FFF) - 0x1000);
            puckflip(r);
            return;
        }
    }
    g.puckvy[0] = at_least_800(g.puckvy[0]);
    wallcoll(x, d0, d1, a3);
    return;
side:
    g.puckvx[0] = at_least_800(g.puckvx[0]);
    d1 = (uint16_t)(g.Xpos[14] - g.OldXpos[14]) & 0x8000 ? 0xFF : 0xFF01;
    wallcoll(x, 0, d1, a3);
}

/* (checkwallcoll: DivX8S's quotient kept in -$FF..$FF, the lower limit first) */
static uint16_t clamp_ff2(uint16_t v)
{
    if (NEG(v, 0xFF01)) v = 0xFF01;
    if (!NEG(v, 0xFF)) v = 0xFF;
    return v;
}

/* Object o at d2 / d3 against the rink: in an end zone's corner circle (64 in from the boards, by
 * its radius) a bounce off the curve (wallcollb, the normal from the circle's centre); between the
 * corners at the ends the goals (checkgoal); then, when nothing hit, the straight boards. */
void checkwallcoll(int o, uint16_t d2, uint16_t d3, int a3)
{
    uint16_t sy = (uint16_t)(0x12A - g.wcradiusy), sx = (uint16_t)(sideline - g.wcradiusx);
    uint16_t d5 = (uint16_t)(-sy + 0x40), d4 = (uint16_t)(-sx + 0x40);
    int x = o;
    if (NEG((uint16_t)(d3 - 1), d5)) {
        if (NEG(d2, d4)) goto corner;
        d4 = (uint16_t)-d4;
        if (NEG(d4, d2)) goto corner;
        checkgoal(o, 0x1A, d2, d3, a3);
        goto boards;
    }
    d5 = (uint16_t)-d5;
    if (!NEG(d5, d3)) goto boards;
    if (NEG(d2, d4)) goto corner;
    d4 = (uint16_t)-d4;
    if (NEG(d4, d2)) goto corner;
    checkgoal(o, 0x18, d2, d3, a3);
    goto boards;
corner:
    {
        uint16_t e2 = (uint16_t)(d2 - d4);
        uint16_t e1 = (uint16_t)-e2;
        uint16_t t4 = e1 & 0x8000 ? (uint16_t)-e1 : e1;
        uint16_t e0 = (uint16_t)(d3 - d5);
        uint16_t cd = e0 & 0x8000 ? (uint16_t)-e0 : e0;
        w16(RAM_OFF_cornerdy, cd);
        uint16_t len = cd >= t4 ? (uint16_t)((t4 >> 2) + cd + (t4 >> 1 & 1)) : (uint16_t)((cd >> 2) + t4 + (cd >> 1 & 1));   /* (lsr's carry into the adc) */
        if (len < 0x41) goto boards;
        uint16_t c = clamp_ff2(DivX8S((uint16_t)((e0 & 0xFF) << 8), len));
        uint16_t s = clamp_ff2(DivX8S((uint16_t)((e1 & 0xFF) << 8), len));
        x = a3;
        wallcollb(a3, c, s, a3);
    }
boards:
    d4 = sx; d5 = sy;
    if (r16(RAM_OFF_Wallcos + 2 * x) | r16(RAM_OFF_Wallsin + 2 * x)) return;
    if (!NEG((uint16_t)(d3 - 1), d5)) { wallcollb(x, 0xFF, 0, a3); return; }
    if (!NEG((uint16_t)(-d5 - 1), d3)) { wallcollb(x, 0xFF01, 0, a3); return; }
    if (!NEG((uint16_t)(d2 - 1), d4)) { wallcollb(x, 0, 0xFF01, a3); return; }
    if (!NEG((uint16_t)(-d4 - 1), d2)) { wallcollb(x, 0, 0xFF, a3); return; }
}

/* Player a2 takes the puck (a new carrier: the puck sound 3, a penalty shot over): the other team's
 * touches cleared (flag 4), a face off won counted (the crowd), a completed pass counted; a goalie
 * catching it makes a save (ChkShotStat) and holds it (temp5 140, 5 after a dive); the pass
 * receiver's pad goes to him, or the next pad of his team (PuckCarrierPad). */
void puckglue(int o, uint16_t a2)
{
    if (a2 != (uint16_t)g.puckc) {
        PuckSfx(3);
        EndPenaltyShotPlay(o);                                /* (o: the X it was called with) */
        g.breakaway = 0;
        g.onetimer = 0;
    }
    g.puckc = (int16_t)a2;
    int t = r16(RAM_OFF_pfteam + a2) != 0;
    int other = !t;
    g.tmtouch2[other] = 0xFFFF;
    g.tmtouch3[other] = 0xFFFF;
    g.tmflags[other] |= 4;
    if (g.sflags3[0] & 0x10) {
        g.sflags3[0] &= (uint16_t)~0x10;
        g.tmfowon[t]++;
        g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0xC8);
        g.crowdhold = 0x14;
    }
    if (!g.gmclock && a2 == g.passReceiverPlayerNum) g.tmpasscomp[t]++;
    g.sflags &= (uint16_t)~0xC;
    if (!r16(RAM_OFF_position + a2)) {
        ChkShotStat();
        w16(RAM_OFF_temp5 + a2, r16(RAM_OFF_SPA + a2) == (uint16_t)SYM_SPAgdive ? 5 : 0x8C);
    }
    uint16_t d4;
    if (a2 == g.passReceiverPlayerNum && !((d4 = g.passpad[t]) & 0x8000)) AssignPadIfFree(a2, d4);
    else PuckCarrierPad(a2);
    g.passReceiverPlayerNum = 0xFFFF;
}

/* The list of the players team a2 wants on the ice now (PlList: six roster numbers, then the six
 * positions they take): from its current line in priolist's order for the men it has (a penalty
 * shot: the shooter and the goalie; no goalie when he is pulled), each one not available replaced
 * by the first available of his position's substitutes (sublist; $80: anyone, from the last
 * roster player down). */
void SetPlList(uint16_t a2)
{
    uint32_t a4 = PlList, a0, a1;
    uint16_t d0, d1, d3, d4, d5;
    for (int k = 0; k < 6; k++) mset8(a4 + k, 0);
    a0 = SYM_priolist;
    if (g.penshot) {
        a0 = SYM_priolist + 7;
        if (a2 == g.BA_Team) a0++;
    } else if (r16(RAM_OFF_tmgoalie + a2) & 0x8000) a0++;
    a1 = GetTeamLines(a2);
    {
        uint16_t l = r16(RAM_OFF_tmline + a2);
        d0 = (uint16_t)(l << 3);
        a1 = (a1 & 0xFF0000) | (uint16_t)(d0 + (uint16_t)a1 + (l >> 13 & 1));   /* (asl's carry into the adc) */
    }
    d4 = r16(RAM_OFF_tmap + a2);
    while (!((d4 = (uint16_t)(d4 - 1)) & 0x8000)) {
        d5 = m8((a0 + d4) & 0xFFFFFF);
        mset8(a4 + d4, m8((a1 + d5) & 0xFFFFFF));
        if (d5 && g.penshot) mset8(a4 + d4, (uint8_t)(g.BA_Skater_Offset + 1));
        mset8(a4 + d4 + 6, (uint8_t)d5);
        if (d5) continue;
        d3 = (uint16_t)(r16(RAM_OFF_tmgoalie + a2) + 1);
        if (!d3 && g.penshot) d3++;
        mset8(a4 + d4, (uint8_t)d3);
    }
    for (d4 = 5; !(d4 & 0x8000); d4--) {
        d3 = m8(a4 + d4);
        if (!d3) continue;
        d3 = (uint16_t)(((uint16_t)(int8_t)d3 - 1) << 1);
        uint16_t st = r16(RAM_OFF_tmpdst + (uint16_t)((a2 ? 0x34 : 0) + d3));
        if (st != 0xFFFD && st != 0xFFFC && ((st & 0x8000) || !st)) continue;
        a1 = GetTeamLines(a2);
        d0 = (uint16_t)(m8(a4 + d4 + 6) << 1);
        a0 = (SYM_sublist & 0xFF0000) | (uint16_t)(rom16(SYM_sublist + d0) + (uint16_t)SYM_sublist);
        for (;;) {
            d0 = m8(a0);
            a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + 1);
            if (d0 & 0x80) break;
            d0 = m8((a1 + d0) & 0xFFFFFF);
            if (TryAddPlayerToList(d0, d4, a2, a4, &d1)) goto next;
        }
        d3 = GetPlayerCount(a2);
        for (;;) {
            d0 = d3;
            d3 = (uint16_t)(d3 - 1);
            if (d3 & 0x8000) break;
            if (TryAddPlayerToList(d0, d4, a2, a4, &d1)) break;
        }
next:;
    }
}

/* Put team a2's wanted players (SetPlList) on the ice: the newpos / newpnum requests of its six
 * objects cleared, then a wanted player already on the ice keeps his object, and each other one
 * takes a free object (the last free one found, one in play preferred). (The original leaves X =
 * its caller's a3.) */
void setpersonel(uint16_t a2)
{
    uint16_t x = r16(RAM_OFF_tmsort + a2), a0 = 0, d5;
    int d3;
    for (int k = 0; k < 6; k++, x += 2) {
        w16(RAM_OFF_newpos + x, 0xFFFF);
        w16(RAM_OFF_newpnum + x, 0xFFFF);
    }
    SetPlList(a2);
    for (int d4 = 5; d4 >= 0; d4--) {
        d5 = m8(PlList + d4);
        if (!d5) continue;
        d5--;
        x = (uint16_t)(r16(RAM_OFF_tmsort + a2) - 2);
        for (d3 = 5; d3 >= 0; d3--) {
            x += 2;
            if ((d5 & 0xFF) != (r16(RAM_OFF_pnum + x) & 0xFF)) continue;
            w16(RAM_OFF_newpos + x, (uint16_t)(int8_t)m8(PlList + d4 + 6));
            w16(RAM_OFF_newpnum + x, (uint16_t)(int8_t)d5);
            mset8(PlList + d4, 0);
            break;
        }
    }
    for (int d4 = 5; d4 >= 0; d4--) {
        d5 = m8(PlList + d4);
        if (!d5) continue;
        d5--;
        x = (uint16_t)(r16(RAM_OFF_tmsort + a2) - 2);
        d3 = 5;
        for (;;) {
            x += 2;
            if (r16(RAM_OFF_newpnum + x) & 0x80) {
                a0 = x;
                if (!(r16(RAM_OFF_position + x) & 0x8000) || --d3 < 0) break;
                continue;
            }
            if (--d3 < 0) break;
        }
        w16(RAM_OFF_newpos + a0, (uint16_t)(int8_t)m8(PlList + d4 + 6));
        w16(RAM_OFF_newpnum + a0, (uint16_t)(int8_t)d5);
        mset8(PlList + d4, 0);
    }
}


/* Raise the crowd's excitement CwdExciteLvl by n (a hit, a fall), unless it would go negative */
static void excite(uint16_t n)
{
    uint16_t v = (uint16_t)(g.CwdExciteLvl + n);
    if (!(v & 0x8000)) g.CwdExciteLvl = v;
}

/* Player y (a2) falls or stumbles from a hit by x (a3 the hitter after it; the team check counted
 * for x): near the boards he goes into them (by where he is and where the hit came from, put
 * against the boards or the corner's curve), by the bench over it; otherwise he falls back (hit
 * from the front), forward or flips (a strong checker); a skater with the puck stick-handling
 * well and no lockout only stumbles. Then the crowd and benches react, and a hard hit on a
 * carrier falling back may injure him (the puck loose, the hitter bounced off, an injuring or
 * roughing penalty, the latter stopping play); else he may lose the puck. Returns d4 as left
 * (the corner's distance when it was measured), which the callers go on using. (a0 is the direct
 * page's as it was where the original reads it unset: Stop4Pen's.) */
uint16_t FallDown(int x, int y, int a3, uint16_t d4)
{
    uint16_t d0, d1, d2, d3, d5, d0h, d1h, d2h, a, a0 = r16(RAM_OFF_a0);
    excite(0xF);
    if (y >= 12) return d4;
    a = g.SPA[y];
    if (a == (uint16_t)SYM_SPAinjury1 || a == (uint16_t)SYM_SPAstumble || a == (uint16_t)SYM_SPAfallfwd
        || a == (uint16_t)SYM_SPAfallback || a == (uint16_t)SYM_SPAflip) return d4;
    d1 = (uint16_t)SYM_SPAstumble;
    if (!g.gmclock && g.stickhand[y] >= 0xC && !g.nopuckb[y]) {
        d0 = (uint16_t)((g.seed[0] & 0xF) + 0xA + 1);    /* (adc: the carry of the cmp) */
        if (!((uint16_t)(d0 - g.impact[y]) & 0x8000)) {
            g.nopuckb[y] = 0x3C;
            goto l21;
        }
    }
    if (g.position[x]) {
        a0 = g.pfteam[x] ? 2 : 0;
        if (!g.gmclock) g.tmchecks[a0 >> 1]++;
    }
    g.nopuck[y] = 0x78;
    d0 = vtoa((uint16_t)(g.Xpos[x] - g.Xpos[y]), (uint16_t)(g.Ypos[x] - g.Ypos[y]));
    if (!NEG(g.Xpos[y], 0x68)) {
        if (!NEG(g.Ypos[y], 0xF0) || NEG(g.Ypos[y], 0xFF0A)) goto l27;
        if (d0 < 5) goto l20;
        g.Xpos[y] = 0x82;
        g.Xvel[y] = 0;
        goto l8;
    }
    if (NEG(g.Xpos[y], 0xFF98)) {
        if (!NEG(g.Ypos[y], 0xF0) || NEG(g.Ypos[y], 0xFF0A)) goto l27;
        if ((uint16_t)(d0 - 1) >= 3) goto l20;
        g.Xpos[y] = (int16_t)0xFF7E;
        g.Xvel[y] = 0;
        if (NEG(g.Ypos[y], 0x58) && !NEG(g.Ypos[y], 0xFFA8)) goto l26;
        goto l11;
    }
    if (!NEG(g.Ypos[y], 0x112)) goto l15;
    if (NEG(g.Ypos[y], 0xFEED)) goto l17;
    goto l20;
l8:
    if (g.attribute[y] & 0x0800) goto l12;
l9:
    a = (uint16_t)SYM_SPAboardright;
    goto l13;
l11:
    if (g.attribute[y] & 0x0800) goto l9;
l12:
    a = (uint16_t)SYM_SPAboardleft;
l13:
    g.pfalock[y] = a;
    SetSPAUpdate(y, a);
    excite(0xA);
    g.benchcheer = 0;
    StartBenchCheer((uint16_t)((g.pfteam[y] ? 2 : 0) + 6));
    g.benchrounds = 2;
    x = a3;
    goto l24;
l15:
    if (!NEG(g.Xpos[y], 0x50) || NEG(g.Xpos[y], 0xFFB0)) goto l27;
    if (((d0 + 2) & 7) < 5) goto l20;
    g.Ypos[y] = 0x128;
    g.Yvel[y] = 0;
l16:
    a = (uint16_t)SYM_SPAboardfront;
    goto l13;
l17:
    if (!NEG(g.Xpos[y], 0x50) || NEG(g.Xpos[y], 0xFFB0)) goto l27;
    if (((d0 + 1) & 7) >= 3) goto l20;
    g.Ypos[y] = (int16_t)0xFED8;
    g.Yvel[y] = 0;
l18:
    a = (uint16_t)SYM_SPAboardback;
    goto l13;
l20:
    StartBenchCheer((uint16_t)((g.pfteam[a3] ? 2 : 0) + 2));
    g.benchrounds = 2;
    d1 = (uint16_t)SYM_SPAfallback;
    if (((uint16_t)(d0 - g.facedir[y] + 1) & 7) >= 3) {
        d1 = (uint16_t)SYM_SPAfallfwd;
        if (!(g.facedir[y] & 3) && !((uint16_t)(g.checking[x] - 0xA) & 0x8000) && x < 12) d1 = (uint16_t)SYM_SPAflip;
    }
l21:
    g.pfalock[y] = 1;
    SetSPA(y, d1);
    x = a3;
l24:
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x12C);
    g.crowdhold = 0x2D;
    if ((g.puckc & 0x8000) || 2 * y != g.puckc) { CollisionSfx(x, y); return d4; }
    if ((g.gmode & gmhl) || g.OptPlayMode == 4 || !g.position[x] || g.SPA[y] != (uint16_t)SYM_SPAfallback
        || (g.facedir[y] & 3) || (g.pflags[y] & 0x4000)) goto l25;
    a = (uint16_t)(randoma8(0x96) << 1);
    if (!((uint16_t)(a - g.impact[y]) & 0x8000) || g.gmclock) goto l25;
    if ((g.position[y] & 0x8000) || !g.position[y]) goto l25;
    g.puckc = -1;
    g.benchcheer = 0;
    StartBenchCheer((uint16_t)((g.pfteam[y] ? 2 : 0) + 6));
    SetSPAFromA(y, (uint16_t)SYM_SPAinjury1);
    x = a3;
    g.Xvel[x] = (int16_t)((int16_t)-g.Xvel[x] >> 1);     /* (cmp #$8000, ror) */
    g.Yvel[x] = (int16_t)((int16_t)-g.Yvel[x] >> 1);
    setInjuryType((uint16_t)(2 * y));
    g.InjCntDown = 4;
    excite(0x1E);
    if (!g.position[x] || !g.OptPen) { AddPenalty2(x, 0x16); return d4; }
    AddPenalty2(x, PenRoughing2);
    Stop4Pen(a0);
    return d4;
l25:
    if (g.SPA[y] != (uint16_t)SYM_SPAstumble) g.puckc = -1;
    if (!g.pfteam[y]) { StreamSfx(3); return d4; }
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x258);
    g.crowdhold = 0x2D;
    return d4;
l26:
    excite(0xA);
    g.Xpos[y] = (int16_t)0xFF7E;
    g.Xvel[y] = 0;
    a = g.attribute[y] & 0x0800 ? (uint16_t)SYM_SPAbenchcheckr : (uint16_t)SYM_SPAbenchcheckl;
    g.pfalock[y] = a;
    SetSPAFromA(y, a);
    x = a3;
    goto l24;
l27:
    d5 = d0;
    d2 = (uint16_t)g.Xpos[y];
    d3 = (uint16_t)g.Ypos[y];
    d0h = d2 & 0x8000 ? 0xFFB8 : 0x48;
    d1h = d3 & 0x8000 ? 0xFF16 : 0xEA;
    d0 = (uint16_t)(d2 - d0h);
    d1 = (uint16_t)(d3 - d1h);
    d4 = ApproxLength(d1, d0);
    if (d4 < 0x14) { x = a3; goto l20; }
    a = (uint16_t)(DivX8S((uint16_t)(d0 << 8 | d0 >> 8), d4) >> 2);
    if (a & 0x2000) a |= 0xC000;
    d0 = a;
    d2h = abs16(d0);
    a = (uint16_t)(DivX8S((uint16_t)(d1 << 8 | d1 >> 8), d4) >> 2);
    x = a3;
    if (a & 0x2000) a |= 0xC000;
    d1 = a;
    if (abs16(d1) < d2h) {
        if (d0h & 0x8000) {
            if ((uint16_t)(d5 - 1) >= 3) goto l20;
            g.Xpos[y] = (int16_t)(d0 + 6 + d0h);
            g.Ypos[y] = (int16_t)(d1 + d1h);
            g.Xvel[y] = g.Yvel[y] = 0;
            goto l11;
        }
        if (d5 < 5) goto l20;
        g.Xpos[y] = (int16_t)(d0 - 6 + d0h);
        g.Ypos[y] = (int16_t)(d1 + d1h);
        g.Xvel[y] = g.Yvel[y] = 0;
        goto l8;
    }
    if (d1h & 0x8000) {
        if (((d5 + 1) & 7) >= 3) goto l20;
        g.Xpos[y] = (int16_t)(d0 + d0h);
        g.Ypos[y] = (int16_t)(d1 + d1h);
        g.Xvel[y] = g.Yvel[y] = 0;
        goto l18;
    }
    if (((d5 + 2) & 7) < 5) goto l20;
    g.Xpos[y] = (int16_t)(d0 + d0h);
    g.Ypos[y] = (int16_t)(d1 + d1h);
    g.Xvel[y] = g.Yvel[y] = 0;
    goto l16;
}

/* (checkint's .ci: goalie a2 against player x (a3): with the clock running and x's impact past 26
 * (past 3 when a3 has the puck), x falls (FallDown by the goalie); a hard hit on the goalie (past
 * 31) may be interference on x (by his aggression), unless he is out of his crease) */
static uint16_t check_int(int x, int a2, int a3, uint16_t d4)
{
    if (g.position[a2] || g.gmclock) return d4;
    if (g.puckc != 2 * a3 && ((uint16_t)(g.impact[x] - 0x1A) & 0x8000)) return d4;
    if ((uint16_t)(g.impact[x] - 3) & 0x8000) return d4;
    d4 = FallDown(a2, a3, a2, d4);
    if ((uint16_t)(g.impact[a2] - 0x1F) & 0x8000) return d4;
    if (g.pflags[a3] & 0x4000) return d4;
    if (randoma8((uint16_t)(0x14 - g.aggress[a3])) >= 2) return d4;
    if ((g.gmode & gmhl) || (g.goalieflags[a2] & 4)) return d4;
    AddPenalty(a3, PenInterference);
    return d4;
}

/* A goalie and another player collide (x and a2 / a3 each way: check_int): the skater falls, and
 * interference may be called. Returns d4 as FallDown left it. */
uint16_t checkint(int x, int a2, int a3, uint16_t d4)
{
    d4 = check_int(x, a2, a3, d4);
    return check_int(a2, a3, a2, d4);
}


/* (a foul on player a2 when he is the carrier on a breakaway and no shooter is set: a penalty shot
 * (on x) for him; nonzero then) */
static int penshot_foul(int x, int a2)
{
    if (!PenShotChk((uint16_t)(2 * a2)) || !(g.BA_Sktr_SCnum & 0x8000)) return 0;
    g.BA_Sktr_SCnum = (uint16_t)(2 * a2);
    g.pspad[0] = g.playerpad[a2];
    AddPenalty(x, PenShot);
    return 1;
}

/* Player a2, sweep checking, body checks x (a3): unless x is locked or a goalie (or, with
 * penalties off, a human checker loses a roll on checking against x's legs), when x is in front
 * of him a3 falls (FallDown by a2), then a penalty shot or (by aggression) tripping; collflag set.
 * Returns d4 as FallDown left it. */
uint16_t Bcheck(int x, int a2, int a3, uint16_t d4)
{
    if (g.pfalock[x] || !g.position[x]) return d4;
    if (!g.OptPen && !(g.playerpad[a2] & 0x8000)
        && randoma8((uint16_t)(0x10 + g.checking[a2] - g.legstr[x])) < 0xC) return d4;
    uint16_t d0 = vtoa((uint16_t)(g.Xpos[x] - g.Xpos[a2]), (uint16_t)(g.Ypos[x] - g.Ypos[a2]));
    if (((uint16_t)(d0 - g.facedir[a2] + 1) & 7) >= 3) return d4;
    d4 = FallDown(a2, a3, a2, d4);
    if (!penshot_foul(a2, a3) && checkagr(a2) < 5) AddPenalty(a2, PenTripping);
    g.collflag = 0xFFFF;
    return d4;
}

/* Player a2 holds or hooks x (a3), who is in front of him: a2 turns to him, both take half their
 * summed velocity, x flails (pfalock) and a2 holds (SPAHold2, or hooks), each the other's impactp;
 * the carrier's shot is off; then a penalty shot or (by aggression) holding / hooking; collflag
 * set. */
void holdcheck(int x, int a2, int a3)
{
    if (!g.position[x] || g.pfalock[x]) return;
    uint16_t d0 = vtoa((uint16_t)(g.Xpos[x] - g.Xpos[a2]), (uint16_t)(g.Ypos[x] - g.Ypos[a2]));
    if (((uint16_t)(d0 - g.facedir[a2] + 1) & 7) >= 3) return;
    if (g.puckc == 2 * a3) g.sflags &= 0xFFF7;
    g.facedir[a2] = d0;
    g.Xvel[x] = g.Xvel[a2] = (int16_t)((int16_t)(g.Xvel[x] + g.Xvel[a2]) >> 1);   /* (asl, ror: the sign kept) */
    g.Yvel[x] = g.Yvel[a2] = (int16_t)((int16_t)(g.Yvel[x] + g.Yvel[a2]) >> 1);
    g.impactp[x] = (int16_t)(2 * a2);
    g.pfalock[x] = (uint16_t)SYM_SPAflail;
    SetSPAFromA(x, (uint16_t)SYM_SPAflail);
    g.impactp[a2] = (int16_t)(2 * a3);
    g.pfalock[a2] = (uint16_t)SYM_SPAHold2;
    SetSPAFromA(a2, g.SPA[a2] == (uint16_t)SYM_SPAHold ? (uint16_t)SYM_SPAHold2 : (uint16_t)SYM_SPAhook2);
    if (!penshot_foul(a2, a3) && checkagr(a2) < 1)
        AddPenalty(a2, g.SPA[a2] == (uint16_t)SYM_SPAHold2 ? 0x28 : PenHooking);
    g.collflag = 0xFFFF;
}

/* (checkcheck's .cc: x checking a2: a2 holding or hooking is holdcheck, sweep checking Bcheck;
 * otherwise x bursting into a2 plays a check by the direction (from the table .4, a shoulder or
 * hip check, or at random the next one), and a skater a2 hit hard enough (d4, the impact, past
 * $20) goes down on a roll against x's checking, with charging, roughing or a penalty shot by x's
 * aggression; returns d4 as left) */
static uint16_t check_check(int x, int a2, int a3, uint16_t d4)
{
    uint16_t s = g.SPA[a2], y, d0;
    if (s == (uint16_t)SYM_SPAHold || s == (uint16_t)SYM_SPAhook) { holdcheck(x, a2, a3); return d4; }
    if (s == (uint16_t)SYM_SPAsweepchk) return Bcheck(x, a2, a3, d4);
    if (g.SPA[x] != (uint16_t)SYM_SPAburst) return d4;
    if (g.gmclock) { g.impact[x] = 0x100; g.impact[a2] = 0x100; }
    d0 = vtoa((uint16_t)(g.Xpos[a2] - g.Xpos[x]), (uint16_t)(g.Ypos[a2] - g.Ypos[x]));
    y = (uint16_t)(((uint16_t)(d0 - g.facedir[x]) & 7) << 1);
    g.pf2aip[x] = (uint16_t)SYM_checkcheck_4;
    if (!y) {
        uint16_t px = (uint16_t)g.Xpos[x], py = (uint16_t)g.Ypos[x];
        if (!NEG(px, 0x50) || NEG(px, 0xA057) || !NEG(py, 0xF8) || !NEG(py, 0xFF08) ? (g.seed[0] & 0x30) : (g.seed[0] & 0xF0))
            y += 2;
    }
    SetSPAFromA(x, m16(SYM_checkcheck_4 + y));
    if (!g.position[a2] || ((uint16_t)(d4 - 0x20) & 0x8000)) return d4;
    d0 = (uint16_t)(((uint16_t)(0x78 - g.weight[x] + g.weight[a2]) >> 1) - g.impact[a2]);
    if (d0 && !(d0 & 0x8000)) {
        d0 = randomd0(d0);
        if (!((uint16_t)(d0 - g.checking[x]) & 0x8000)) {
            if (checkagr(x) < 1 && !(g.gmode & gmhl)) AddPenalty(x, (uint16_t)((r16(RAM_OFF_seed + 1) & 2) + PenCharging));
            return d4;
        }
    }
    if (checkagr(x) < 1 && !(g.gmode & gmhl)) AddPenalty(x, (uint16_t)((r16(RAM_OFF_seed + 1) & 2) + PenRoughing));
    penshot_foul(x, a2);
    return FallDown(x, a2, a3, d4);
}

/* Two players in contact: what happens each way (check_check of a3 on a2, then of a2 on a3). */
void checkcheck(int x, int a2, int a3, uint16_t d4)
{
    d4 = check_check(x, a2, a3, d4);
    check_check(a2, a3, a2, d4);
}

static int in_hold(uint16_t s) { return s >= (uint16_t)SYM_SPAHold2 && s < 0xF46B; }
static int in_sweep(uint16_t s) { return s >= (uint16_t)SYM_SPAsweepchk && s < 0xF343; }

/* (checkplcoll's .t3: players x (a3) and y touch (within $10) while closing in: any hold between
 * them or their partners is broken; opponents take the impact (the closing speed / 16, at least 5,
 * half again in a sweep check; the collision sound when hard with the carrier), then checkint and
 * checkcheck; unless that set collflag, they bounce apart by their weights (+$8C), collflag set) */
static void pl_pair(int x, int y, int a3)
{
    uint16_t d2 = (uint16_t)(g.Xpos[y] - g.Xpos[x]), d3 = (uint16_t)(g.Ypos[y] - g.Ypos[x]);
    if (!NEG(ApproxLength(d3, d2), 0x10)) return;
    uint16_t d0 = (uint16_t)(g.Xvel[x] - g.Xvel[y]), d1 = (uint16_t)(g.Yvel[x] - g.Yvel[y]);
    if (!(d0 | d1)) return;
    uint16_t v0 = d0, v1 = d1, d4, d5;
    uint32_t p = Mul16x8S(d1, d3);
    p = (uint32_t)(Mul16x8S(d0, d2) + p);
    x = a3;
    if (p & 0x80000000u) return;
    if (in_hold(g.SPA[x])) {
        g.pfalock[x] = 0; g.pf2aip[x] = 0;
        uint16_t z = (uint16_t)g.impactp[x];
        if (in_hold(r16(RAM_OFF_SPA + z))) { w16(RAM_OFF_pfalock + z, 0); w16(RAM_OFF_pf2aip + z, 0); }
    }
    if (in_hold(g.SPA[y])) {
        g.pfalock[y] = 0; g.pf2aip[y] = 0;
        uint16_t z = (uint16_t)g.impactp[y];
        if (in_hold(r16(RAM_OFF_SPA + z))) { w16(RAM_OFF_pfalock + z, 0); w16(RAM_OFF_pf2aip + z, 0); }
    }
    d1 = (uint16_t)(p >> 8);
    if (g.pfteam[x] != g.pfteam[y]) {
        d4 = d1 >> 4;
        if (d4 < 5) d4 = 5;
        if (in_sweep(g.SPA[x]) || in_sweep(g.SPA[y])) d4 = (uint16_t)((d4 >> 1) + d4 + (d4 & 1));   /* (lsr's carry into the adc) */
        g.impact[x] = (int16_t)(d4 + g.impact[x]);
        g.impact[y] = (int16_t)(d4 + g.impact[y]);
        g.impactp[y] = (int16_t)(2 * x);
        g.impactp[x] = (int16_t)(2 * y);
        if (d4 >= 0x14 && (g.puckc == 2 * x || g.puckc == 2 * y)) CollisionSfx(x, y);
        d4 = checkint(x, y, a3, d4);
        checkcheck(a3, y, a3, d4);
    }
    d4 = d1;
    d0 = v0; d1 = v1;
    if (g.collflag & 0x8000) return;
    p = Mul16x8S(d1, d2);
    p = (uint32_t)(Mul16x8S(d0, d3) - p);
    d5 = (uint16_t)(p >> 8);
    x = a3;
    d0 = (uint16_t)(g.weight[x] + 0x8C);
    d1 = (uint16_t)(g.weight[y] + 0x8C + d0);
    d0 = d1 = Div32S(Mul16x8XS(d4, d0), d1);
    uint16_t t = (uint16_t)Mul16x8S(d5, d3);
    g.Xvel[x] = (int16_t)((uint16_t)Mul16x8S(d0, d2) + t + g.Xvel[y]);
    t = (uint16_t)Mul16x8S(d5, d2);
    g.Yvel[x] = (int16_t)((uint16_t)Mul16x8S(d0, d3) - t + g.Yvel[y]);
    g.Xvel[y] = (int16_t)((uint16_t)Mul16x8S(d1, d2) + g.Xvel[y]);
    g.Yvel[y] = (int16_t)((uint16_t)Mul16x8S(d1, d3) + g.Yvel[y]);
    g.collflag = 0xFFFF;
}

/* (checkplcoll: object z of OOlist can touch x, at new x d2: in play, within 15 in x, no flag 15 /
 * pfnc, a player) */
static int pl_near(uint16_t z, uint16_t d2)
{
    if (r16(RAM_OFF_position + z) & 0x8000) return 0;
    uint16_t dx = (uint16_t)(r16(RAM_OFF_Xpos + z) - d2);
    if (NEG(dx, 0xFFF1) || !NEG(dx, 0xF)) return 0;
    if ((r16(RAM_OFF_pflags + z) & 0x8000) | r16(RAM_OFF_pfnc + z)) return 0;
    return z < 0x18;
}

/* Player x (a3) at his new place d2 / d3 against the players near him in y (OOlist, up then down,
 * within $0E): pl_pair. */
void checkplcoll(int x, uint16_t d2, uint16_t d3, int a3)
{
    if ((g.pflags[x] & 0x8000) || x >= 12) return;
    uint16_t d1 = g.OOlistpos[x], z;
    for (; d1 != 0xF; d1++) {
        z = g.OOlist[d1 + 1];
        if (!((uint16_t)(r16(RAM_OFF_Ylist + z) - d3 - 0xE) & 0x8000)) break;
        if (pl_near(z, d2)) pl_pair(x, z >> 1, a3);
    }
    if (!g.OOlistpos[x]) return;
    for (d1 = g.OOlistpos[x]; d1; d1--) {
        z = g.OOlist[d1 - 1];
        if (!((uint16_t)(d3 - r16(RAM_OFF_Ylist + z) - 0xE) & 0x8000)) return;
        if (pl_near(z, d2)) pl_pair(x, z >> 1, a3);
    }
}

/* Object x moves to d2 / d3 (swapped on the horizontal screen): the walls (at its hot spot, and a
 * player's stick end too when the body hits none) and the players (checkplcoll), unless it skips
 * collisions; after a player collision it goes back to its old place (but into the boards);
 * otherwise it moves along OOlist (kept in y order) and its Ylist y is the new one. */
void checkcoll(int x, uint16_t d2, uint16_t d3, int a3)
{
    g.collflag = 0;
    if (g.sfhormode) { uint16_t t = d2; d2 = d3; d3 = t; }
    if (!g.pfnc[x]) {
        g.wcradiusx = (uint16_t)g.radiusx[x];
        g.wcradiusy = (uint16_t)g.radiusy[x];
        checkwallcoll(x, (uint16_t)g.Xpos[x], (uint16_t)g.Ypos[x], a3);
        if (!(g.Wallcos[x] | g.Wallsin[x]) && x < 12) {
            int16_t hx, hy;
            GetHot(x, &hx, &hy);
            g.wcradiusx = g.wcradiusy = 1;
            checkwallcoll(x, (uint16_t)(g.Xpos[x] + hx), (uint16_t)(g.Ypos[x] + hy), a3);
        }
        if (!(g.position[x] & 0x8000)) checkplcoll(x, d2, d3, a3);
    }
    if (g.collflag) {
        if (g.SPA[x] >= (uint16_t)SYM_SPAboardfront) return;
        g.Xpos[x] = g.OldXpos[x];
        g.Ypos[x] = g.OldYpos[x];
        return;
    }
    uint16_t d1 = g.OOlistpos[x], d4;
    for (; d1 != 0xF; d1++) {
        d4 = g.OOlist[d1 + 1];
        uint16_t t = (uint16_t)(d3 - r16(RAM_OFF_Ylist + d4));
        if (!t || (t & 0x8000)) break;
        g.OOlistpos[x]++;
        w16(RAM_OFF_OOlistpos + d4, (uint16_t)(r16(RAM_OFF_OOlistpos + d4) - 1));
        g.OOlist[d1] = (uint8_t)d4;
        g.OOlist[d1 + 1] = (uint8_t)(2 * x);
    }
    if (g.OOlistpos[x]) {
        for (d1 = g.OOlistpos[x]; d1; d1--) {
            d4 = g.OOlist[d1 - 1];
            if (!((uint16_t)(d3 - r16(RAM_OFF_Ylist + d4)) & 0x8000)) break;
            g.OOlistpos[x]--;
            w16(RAM_OFF_OOlistpos + d4, (uint16_t)(r16(RAM_OFF_OOlistpos + d4) + 1));
            g.OOlist[d1] = (uint8_t)d4;
            g.OOlist[d1 - 1] = (uint8_t)(2 * x);
        }
    }
    g.Ylist[x] = d3;
}


/* The puck (x, low: under $10) against the objects near it in y (OOlist, up then down, within
 * $28): checkpuckcoll_ccx for each (a0 as the caller left it, for puckstick). */
void checkpuckcoll(int x, int a3, uint16_t a0)
{
    if (!((uint16_t)(g.puckz - 0x10) & 0x8000)) return;
    uint16_t d1, z;
    for (d1 = g.OOlistpos[14]; d1 != 0xF; d1++) {
        z = g.OOlist[d1 + 1];
        if (!((uint16_t)(r16(RAM_OFF_Ylist + z) - g.pucky[0] - 0x28) & 0x8000)) break;
        checkpuckcoll_ccx(x, z >> 1, a3, a0);
    }
    for (d1 = g.OOlistpos[14]; d1; d1--) {
        z = g.OOlist[d1 - 1];
        if (!((uint16_t)(g.pucky[0] - r16(RAM_OFF_Ylist + z) - 0x28) & 0x8000)) return;
        checkpuckcoll_ccx(x, z >> 1, a3, a0);
    }
}


/* The puck x hits player a2's body (d0 the squared distance from ccx): unless it is low and off
 * by $10's remainder, he touches it with the puck sound 8 (the original compares a2touchpuck's A,
 * never $E or more, where it meant the puck's height), the puck takes his offset (or his hot spot)
 * as its velocity's high bytes and flips; a fast high puck (speed squared $0900 0000 or more, $C
 * up) knocks him down (FallDown by a3), a slower one is caught. Returns a3 as a2touchpuck left
 * it. */
int puckbody(int x, int a2, int a3, uint16_t d0)
{
    if (NEG(g.puckz, 8) && (d0 & 0xF)) return a3;
    a3 = a2touchpuck((uint16_t)(2 * a2), a3, x, &d0);
    PuckSfx(8);
    g.puckvz = 0;
    g.nopuck[a2] = 8;
    d0 = (uint16_t)(g.Xpos[x] - g.Xpos[a2]);
    uint16_t d1 = (uint16_t)(g.Ypos[x] - g.Ypos[a2]);
    if (!d1) {
        int16_t hx, hy;
        GetHot(a2, &hx, &hy);
        d0 = (uint16_t)hx; d1 = (uint16_t)hy;
    }
    uint16_t d2 = (uint16_t)g.Xvel[x], d3 = (uint16_t)g.Yvel[x];
    w8(RAM_OFF_Xvel + 2 * x + 1, (uint8_t)d0);
    w8(RAM_OFF_Yvel + 2 * x + 1, (uint8_t)d1);
    puckflip(d0);
    if (NEG(g.puckz, 8)) return a3;
    uint32_t p = Mul16S(d2, d2);
    p = (uint32_t)(Mul16S(d3, d3) + p);
    if ((p >> 16) >= 0x900) {
        if (NEG(g.puckz, 0xC)) return a3;
        FallDown(a3, a2, a3, 0);
        return a3;
    }
    if (g.pfalock[a2]) return a3;
    g.pf2aip[a2] = 1;
    SetSPAFromA(a2, (uint16_t)SYM_SPAcatch);
    return a3;
}

/* The puck x hits player a2's stick (d0 the squared distance, a0 the carrier as the original has
 * it: what a2touchpuck left there, the carrier's team offset; read as the original leaves it, as an earlier
 * puckstick of the same checkpuckcoll may have changed it). With the puck carried by the other
 * team (close, under $25): a skater may poke it loose on a roll by both players' stick handling
 * (energy-scaled), both lose the puck for a while; with the puck loose he takes it (puckglue)
 * unless it is too fast for his stick handling (with a pass to him or for a goalie, slower), when
 * it deflects off his stick. */
void puckstick(int x, int a2, int a3, uint16_t d0, uint16_t a0)
{
    if (!g.position[a2] && (g.gmode & gmhl)) return;
    g.pflags[a2] &= 0xFFFE;
    uint16_t y = (uint16_t)g.puckc, c;
    if (!(y & 0x8000)) {
        if (r16(RAM_OFF_pfteam + y) == g.pfteam[a2] || d0 >= 0x25) return;
        if (g.position[a2]) {
            if (!r16(RAM_OFF_position + a0)) return;
            c = makepde(r16(RAM_OFF_stickhand + a0), a0 >> 1);
            if (randomd0A((uint16_t)(c - makepde(g.stickhand[a2], a2) + 0x24)) >= 3) return;
        }
        c = makepde(r16(RAM_OFF_stickhand + a0), a0 >> 1);
        w16(RAM_OFF_nopuck + a0, (uint16_t)(c + 0x14 + cf));   /* (adc: makepde's carry) */
        d0 = makepde(g.stickhand[a2], a2);
        g.nopuck[a2] = (uint16_t)(d0 + 0x14 + cf);
        g.lastplayer[0] = a0 >> 1;
        g.sflags &= 0xFFF3;
        goto l3;
    }
    {
        uint16_t d2 = d0;
        uint32_t s = Mul16S((uint16_t)g.puckvx[0], (uint16_t)g.puckvx[0]);
        s = (uint32_t)(Mul16S((uint16_t)g.puckvy[0], (uint16_t)g.puckvy[0]) + s);
        d0 = (uint16_t)s;
        uint16_t d1 = 0;
        if (!(g.sflags & 0x1000)) d1 = (uint16_t)Mul16x8U(0x2BC, g.stickhand[a2]);
        uint32_t a = (uint32_t)d1 + 0x2710;
        if (!g.position[x] || 2 * x == g.passReceiverPlayerNum) a = (a & 0xFFFF) + 0xBB8 + (a >> 16);   /* (adc: the carry of the add) */
        if (Mul16U((uint16_t)a, (uint16_t)a) >= s) { puckglue(x, (uint16_t)(2 * a2)); return; }
        if (g.position[x] && d2 >= 0x25) return;
        g.nopuck[a2] = 8;
    }
l3:
    PuckSfx(3);
    a2touchpuck((uint16_t)(2 * a2), a3, x, &d0);
    deflect(d0);
}

/* The puck x hits goalie a2 (d0 / d1 its offset from ccx): the save rating of the side it comes
 * at (low: glove or stick, by his facing; high: the other two), unless he is diving and it is high;
 * then the shot counts, he touches it (the sound, the crowd for a hard one); a carrier loses it.
 * A loose slow puck (by his shot speed rating) he covers (puckglue); otherwise it bounces off him
 * (its velocity's high bytes his offset, y away from his movement) and flips. (The original
 * indexes the shot speed rating by the Y the sound left when the puck was slow; here a2.) */
void puckgoalie(int x, int a2, int a3, uint16_t d0, uint16_t d1)
{
    if (g.gmode & gmhl) return;
    uint16_t d2 = NEG(g.puckz, 8) ? 2 : 0;
    d0 = vtoa((uint16_t)-d0, (uint16_t)-d1);
    d0 = (uint16_t)(d0 - g.facedir[a2]) & 7;
    d1 = d0 & 3;
    if (!d1) d0 = g.seed[0];
    d2 = (uint16_t)(((((d0 & 4) >> 2) ^ 1) + d2) << 1);
    if (g.attribute[a2] & 0x0800) d2 ^= 2;
    if ((uint16_t)(g.frame[a2] - 662) < 0x10 && !NEG(g.puckz, 5)) return;   /* (SPFgdive: diving) */
    ChkShotStat();
    a2touchpuck((uint16_t)(2 * a2), a3, x, &d0);
    uint16_t y = PuckSfx(6);   /* (Y: the sound's voice for a slow puck) */
    if (!NEG(g.puckvy[0], 0x3000) || NEG(g.puckvy[0], 0xD000)) {
        y = (uint16_t)(2 * a2);
        excite(0xA);
        if (g.pfteam[a2]) StreamSfx(4);
        else {
            g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x258);
            g.crowdhold = 0x3C;
        }
    }
    {
        uint16_t pc = (uint16_t)g.puckc;
        if (!(pc & 0x8000)) pc >>= 1;
        if ((pc & 0x8000) || 2 * x == g.puckc) {
            d0 = (uint16_t)(randomd0A((uint16_t)((uint16_t)((2 + r16(RAM_OFF_shotspd + y)) << 8) << 1)) + 0x2000);
            if (!NEG(d0, g.puckvx[0]) && !NEG(d0, g.puckvy[0])) {
                d0 = (uint16_t)-d0;
                if (NEG(d0, g.puckvx[0]) && NEG(d0, g.puckvy[0]) && !(g.pflags[a2] & 0x1000)) {
                    g.puckvz = 0;
                    puckglue(x, (uint16_t)(2 * a2));
                    return;
                }
            }
        } else {
            g.puckc = (int16_t)(g.puckc | 0xFF00);
            pc <<= 1;
            w16(RAM_OFF_nopuck + pc, 0x14);
            g.lastplayer[0] = pc >> 1;
            g.sflags &= 0xFFF3;
        }
    }
    g.puckvz = 0;
    g.nopuck[a2] = 0xA;
    g.temp4[a2] = 4;
    d0 = (uint16_t)(g.Xpos[x] - g.Xpos[a2]);
    d1 = (uint16_t)(g.Ypos[x] - g.Ypos[a2]);
    if (!d1) {
        int16_t hx, hy;
        GetHot(a2, &hx, &hy);
        d0 = (uint16_t)hx; d1 = (uint16_t)hy;
    }
    d1 = (uint16_t)(int8_t)d1;
    if (d1) d1++;
    if (!((g.Yvel[a2] ^ d1) & 0x8000)) d1 = (uint16_t)-d1;
    w8(RAM_OFF_Xvel + 2 * x + 1, (uint8_t)d0);
    w8(RAM_OFF_Yvel + 2 * x + 1, (uint8_t)d1);
    puckflip(d0);
}

/* Set up a penalty shot: the puck at centre ice and still; each skater of the shooting team
 * (BA_Team) takes assignment $10 eight times over (puckc him meanwhile); with manual goalies the
 * other team's pad takes its goalie; the shooting team's touches cleared, the crowd excited; the
 * shooter (BA_Sktr_SCnum) has the puck (and the pad pspad); PenShotActive, penshotlive, the time
 * limit pstimer 1000; the penalty buffer cleared. */
void SetupPenaltyShot(void)
{
    g.Xpos[14] = 0;
    g.pucky[0] = 0;
    g.puckz = 0;
    g.puckvx[0] = 0;
    g.puckvy[0] = 0;
    g.puckvz = 0;
    uint16_t x = g.BA_Team ? 0xC : 0, d4 = g.BA_Team ? 0 : 2;
    for (int k = 0; k < 6; k++, x += 2) {
        g.puckc = (int16_t)x;
        for (int n = 0; n < 8; n++) assinsert(x >> 1, 0x10);
    }
    if (r16(RAM_OFF_teampads + d4) && r16(RAM_OFF_goaliemode1 + d4)) {
        for (uint16_t y = 0; y < 0xA; y += 2) {
            uint16_t s = r16(RAM_OFF_padside + y);
            if (!(s & 0x8000) && s == d4) { PadTakeGoalie(y); break; }
        }
    }
    uint16_t y = g.BA_Team;
    w16(RAM_OFF_tmtouch1 + y, 0xFFFF);
    w16(RAM_OFF_tmtouch2 + y, 0xFFFF);
    w16(RAM_OFF_tmtouch3 + y, 0xFFFF);
    excite(0x14);
    g.puckc = (int16_t)g.BA_Sktr_SCnum;
    if (!(g.pspad[0] & 0x8000)) w16(RAM_OFF_playerpad + g.BA_Sktr_SCnum, g.pspad[0]);
    g.PenShotActive = 1;
    g.penshotlive = 1;
    g.pstimer = 0x3E8;
    ClearPenaltyBuffer();
}

static void inc16(uint32_t off) { w16(off, (uint16_t)(r16(off) + 1)); }

/* The puck is in the net (o, the puck): the score and team stats, the siren and the crowd, the score
 * summary and the players' goals and assists, the scoreboard, the scorers celebrate, the puck pops up at
 * the goal line and the goal whistle. A stopped clock only stops the puck; a penalty shot at the wrong net
 * (by the end and gmdir, against BA_Team) ends without a goal. */
void Goal(int o)
{
    if (g.gmclock) {
        g.puckvz = 0;
        g.puckvx[0] = 0;
        g.puckvy[0] = 0;
        return;
    }
    if (g.PenShotActive) {
        int right = !(g.pucky[0] & 0x8000) == !!(g.gmode & gmdir);
        if (right ? !g.BA_Team : g.BA_Team) { EndPenaltyShotPlay(o); return; }
    }
    ChkShotStat();
    freezewindow();
    g.replaydelay = 0x1E;
    uint16_t a2 = 0, a1 = 2;
    if (g.pucky[0] & 0x8000) { a1 = 0; a2 = 2; }
    if (g.gmode & gmdir) { a2 = a1; a1 ^= 2; }
    inc16(RAM_OFF_tmscore + a2);
    uint16_t d = (uint16_t)(g.tmap[0] - g.tmap[1]);
    if (d && (d & 0x8000 ? !a2 : a2)) inc16(RAM_OFF_tmSHGoals + a2);
    if (g.penshot) inc16(RAM_OFF_tmPenShotGoals + a2);
    else if (g.breakaway) inc16(RAM_OFF_tmBreakawayGoals + a2);
    if (g.onetimer) inc16(RAM_OFF_tmOneTimerGoals + a2);
    MusicStop();
    uint16_t a = g.streamsfx == 1 ? 1 : 2;   /* (StreamSfx's A: the stream asked for, or 1 still playing) */
    StreamSfx(2);
    g.benchcheer = 0;
    if (a2) {
        StartBenchCheer(4);
        uint16_t c = (uint16_t)(g.Crowdlevel - 0x300);
        g.Crowdlevel = c & 0x8000 ? 0 : c;
    } else {
        StartBenchCheer((uint16_t)(a + 2));
        g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x1F4);
        g.crowdhold = 0x78;
        PlayTeamSong(3);
    }
    g.sfxmutetime = g.vcount[0];
    EndPenaltyShotPlay(o);
    g.breakaway = 0;
    g.onetimer = 0;
    uint16_t x = (uint16_t)(g.gsp << 2);
    uint16_t c = (uint16_t)(g.CwdExciteLvl + 0x1E);
    if (!(c & 0x8000)) g.CwdExciteLvl = c;
    if (a2) {
        x += 2;
        c = (uint16_t)(g.CwdExciteLvl + 0xFFCE);
        if (!(c & 0x8000)) g.CwdExciteLvl = c;
    }
    inc16(RAM_OFF_tmPerGoals + x);
    if (g.ScoreSumbytes[0] == 0x258) g.ScoreSumbytes[0] -= 6;
    uint32_t s = (uint32_t)(uint16_t)ScoreSum + g.ScoreSumbytes[0];
    uint32_t a0 = (ScoreSum & 0xFF0000) | (uint16_t)s;
    g.ScoreSumbytes[0] = (uint16_t)(g.ScoreSumbytes[0] + 6 + (s >> 16));   /* (the first add's carry) */
    put16(a0, GetPeriodTimeRemaining());
    a0 += 2;
    uint16_t v = (uint16_t)(2 + r16(RAM_OFF_tmap + a2) - r16(RAM_OFF_tmap + a1));
    if (a2) v |= 0x80;
    put16(a0, v);
    a0++;
    uint16_t d0 = r16(RAM_OFF_tmtouch1 + a2);
    put16(a0, d0);
    a0++;
    put16(a0, 0xFFFF);
    uint16_t base = a2 ? 0x1A : 0;
    if (r16(RAM_OFF_tmgoalie + a2) != d0) {
        uint32_t t = RAM_OFF_tmgoals + (uint16_t)(base + d0);
        w8(t, (uint8_t)(r8(t) + 1));
    }
    d0 = r16(RAM_OFF_tmtouch2 + a2);
    if (!(d0 & 0x8000)) {
        put8(a0, (uint8_t)d0);
        uint32_t t = RAM_OFF_tmassists + (uint16_t)(base + d0);
        w8(t, (uint8_t)(r8(t) + 1));
        a0++;
        d0 = r16(RAM_OFF_tmtouch3 + a2);
        if (!(d0 & 0x8000)) {
            put8(a0, (uint8_t)d0);
            t = RAM_OFF_tmassists + (uint16_t)(base + d0);
            w8(t, (uint8_t)(r8(t) + 1));
        }
    }
    d0 = r16(RAM_OFF_tmgoalie + a1);
    if (!(d0 & 0x8000)) inc16(RAM_OFF_tmgoals + (uint16_t)((a1 ? 0x1A : 0) + d0));   /* (a word: the next player's byte takes the carry) */
    PenGoalStuff(a1, a2);
    printscores1();
    for (int k = 0, p = r16(RAM_OFF_tmsort + a2) >> 1; k < 6; k++, p++) {
        if (g.position[p] <= 0) continue;
        g.pfnc[p] = 0;
        assinsert(p, 8);
    }
    g.collflag = 0;
    g.puckvx[0] = 0;
    g.puckvy[0] = 0;
    g.Xpos[14] = g.Xpos[14] & 0x8000 ? (int16_t)-6 : 6;
    g.pucky[0] = g.pucky[0] & 0x8000 ? (int16_t)0xFEF0 : 0x110;
    g.puckvz = 0x600;
    g.puckz = 0;
    g.puckcross[1] = 0xFFFF;
    g.puckcross[3] = 0xFFFF;
    g.pfnc[14] = 0xFFFF;
    assreplace(o, 0x1A);
    SetSPAFromA(o + 1, (uint16_t)SYM_SPAsiren);
    AddPenalty2(r16(RAM_OFF_tmsort + a2) >> 1, PenGoal);
}

/* The puck x against player y (a2): unless he carries it, is past the skaters, off the ice or not
 * colliding. A goalie: within his stick's reach (his stick hot spot, the height by the
 * checkpuckcoll_ccx_8 table) the stick (puckstick); then his body within 11 (puckgoalie). A skater,
 * puck low and slow: within $C4 squared of his stick (a quarter while the puck is not to be
 * touched) the stick; otherwise within 9 and $41 squared his body (puckbody, the original's X
 * being a3). */
void checkpuckcoll_ccx(int x, int y, int a3, uint16_t a0)
{
    int a2 = y;
    uint16_t d0, d1, d2, d3, d4;
    int16_t hx, hy;
    if (2 * y == g.puckc || y >= 12 || (g.position[y] & 0x8000) || (g.pfnc[y] | g.nopuck[y])) return;
    if (g.pflags[a2] & 0x1000) goto l5;
    if (!g.position[a2]) goto l9;
    if (!((uint16_t)(g.puckz - 5) & 0x8000) || !((uint16_t)(g.puckvz - 0x100) & 0x8000)) goto l5;
    GetHot(a2, &hx, &hy);
    d0 = abs16((uint16_t)((uint16_t)hx + g.Xpos[a2] - g.Xpos[14]));
    if (d0 >= 0xF) goto l5;
    d1 = abs16((uint16_t)((uint16_t)hy + g.Ypos[a2] - g.pucky[0]));
    if (d1 >= 0xF) goto l5;
    d0 = (uint16_t)(Mul8U(d0, d0) + Mul8U(d1, d1));
    d1 = 0xC4;
    if (!(g.nopuck[14] & 0x8000) && g.nopuck[14]) d1 >>= 2;
    if (d1 < d0) goto l5;
    puckstick(g.curobj >> 1, a2, a3, d0, a0);
    return;
l5:
    if (!g.position[a2]) goto l7;
    d0 = abs16((uint16_t)(g.Xpos[a2] - g.Xpos[14]));
    if (d0 >= 9) return;
    d1 = abs16((uint16_t)(g.Ypos[a2] - g.pucky[0]));
    if (d1 >= 9) return;
    d0 = (uint16_t)(Mul8U(d0, d0) + Mul8U(d1, d1));
    if (d0 >= 0x41) return;
    puckbody(a3, a2, a3, d0);
    return;
l9:
    GetFrameHot(a2, &d0, &d1);
    d2 = abs16(d0);
    d4 = rom16(SYM_checkpuckcoll_ccx_8 + (uint16_t)(d2 << 1));
    d3 = rom16(SYM_checkpuckcoll_ccx_8 + (uint16_t)(abs16(d1) << 1));
    x = g.curobj >> 1;
    GetHot(a2, &hx, &hy);
    d0 = abs16((uint16_t)((uint16_t)hx + g.Xpos[a2] - g.Xpos[14]));
    if (d0 >= d2) goto l7;
    d1 = abs16((uint16_t)((uint16_t)hy + g.Ypos[a2] - g.pucky[0]));
    if (d1 >= d2) goto l7;
    {
        uint16_t t = (uint16_t)(d3 + d4);
        if ((uint16_t)(t - 5) & 0x8000) t = 5;
        if (t < (uint16_t)g.puckz) goto l7;
        t = (uint16_t)(d3 - d4);
        if (!(t & 0x8000) && t >= (uint16_t)g.puckz) goto l7;
    }
    d0 = (uint16_t)(Mul8U(d0, d0) + Mul8U(d1, d1));
    d1 = Mul8U(d2, d2);
    if (!(g.nopuck[14] & 0x8000) && g.nopuck[14]) d1 >>= 2;
    if (d1 >= d0) puckstick(x, a2, a3, d0, a0);
l7:
    d0 = (uint16_t)(g.Xpos[a2] - g.Xpos[14]);
    if ((uint16_t)(d0 - 0xFFF5) & 0x8000 || !((uint16_t)(d0 - 0xB) & 0x8000)) return;
    d1 = (uint16_t)(g.Ypos[a2] - g.pucky[0]);
    if ((uint16_t)(d1 - 0xFFF5) & 0x8000 || !((uint16_t)(d1 - 0xB) & 0x8000)) return;
    puckgoalie(x, a2, a3, d0, d1);
}
