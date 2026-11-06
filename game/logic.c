/* logic: the game's decisions (src/routines/logic). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define YPOS(o) r16(RAM_OFF_Ypos + 2 * (o))           /* (objects past the array: by address) */

/* Replace player o's assignment with assdumpin ($14). */
void StartDumpIn(int o) { assreplace(o, 0x14); }

/* Start animation anim on object o (the same one keeps going). */
void SetSPAFromA(int o, uint16_t anim)
{
    if (anim == g.SPA[o]) return;
    g.SPA[o] = anim;
    g.SPAnum[o] = 0;
    g.SPAcnt[o] = 0xFFFF;
}

/* The puck slides and its spin restarts: during the spin frames its face turns around. */
void puckunflip(void)
{
    if (g.SPAnum[14] >= 8 && g.SPAnum[14] < 0x18) g.facedir[14] ^= 2;
    g.facedir[14] |= 4;
    g.SPAnum[14] = 0;
    g.SPAcnt[14] = 0xFFFF;
}

/* Pad d4 takes control of player d0: its old player is released (pfna set, his pad bit cleared,
 * playerpad -1, a goalie's goalieflags cleared), the new one marked (pflags bit 3, playerpad). */
void AssignPadPlayer(uint16_t d0, uint16_t d4)
{
    g.dumpinplayer = 0xFFFF;
    uint16_t y = r16(RAM_OFF_c1playernum + d4);
    if (y == d0) return;
    if (y < 0x18) {
        w16(RAM_OFF_pflags + y, (uint16_t)((r16(RAM_OFF_pflags + y) & 0xFFF7) | 2));
        w16(RAM_OFF_playerpad + y, 0xFFFF);
        if (!r16(RAM_OFF_position + y)) w16(RAM_OFF_goalieflags + y, 0);
    }
    if (d0 >= 0x18) return;
    w16(RAM_OFF_pflags + d0, (uint16_t)(r16(RAM_OFF_pflags + d0) | 8));
    w16(RAM_OFF_playerpad + d0, d4);
    w16(RAM_OFF_c1playernum + d4, d0);
}

/* A penalty shot for shooter BA_Sktr_SCnum, unless one is pending: his team (BA_Team, its
 * tmPenShots), one man each on the ice, the shooter's roster number; the faceoff spot saved and
 * cleared. */
void StartPenaltyShot(void)
{
    if (g.penshot) return;
    if (!NEG(g.BA_Sktr_SCnum, 0xC)) { g.BA_Team = 2; g.tmPenShots[1]++; }
    else { g.BA_Team = 0; g.tmPenShots[0]++; }
    w16(RAM_OFF_tmap + (uint16_t)(g.BA_Team ^ 2), 1);
    w16(RAM_OFF_tmap + g.BA_Team, 1);
    g.BA_Skater_Offset = r16(RAM_OFF_pnum + g.BA_Sktr_SCnum);
    g.penshot = 1;
    g.psfoxsave = g.fox;
    g.psfoysave = g.foy;
    g.fox = 0;
    g.foy = 0;
}

/* d0 / d1 = the vector from goalie o to the puck; when one of them is behind the goal line ($108)
 * and the other in front, d1 = +-1 so he faces out. */
void GoaliePuckVector(int o, uint16_t *d0, uint16_t *d1)
{
    *d0 = (uint16_t)(g.Xpos[14] - g.Xpos[o]);
    *d1 = (uint16_t)(g.pucky[0] - g.Ypos[o]);
    uint16_t y = (uint16_t)g.Ypos[o], py = (uint16_t)g.pucky[0];
    if (y & 0x8000) {
        if (NEG(y, 0xFEF8)) return;
        if (NEG(py, 0xFEF8)) *d1 = 1;
    } else {
        if (!NEG(y, 0x108)) return;
        if (!NEG(py, 0x108)) *d1 = 0xFFFF;
    }
}

/* For each of the 16 objects (curobj each in turn): puckdist2 (about (dx^2 + dy^2) / 16 by the
 * quarters' low bytes), puckdist (ApproxLength) and puckdir (vtoa) to the puck. */
void CalcPuckDistances(void)
{
    for (int o = 0; o < 16; o++) {
        g.curobj = (uint16_t)(2 * o);
        uint16_t dx = (uint16_t)(g.Xpos[14] - g.Xpos[o]);
        uint16_t ax = dx & 0x8000 ? (uint16_t)-dx : dx;
        g.puckdist2[o] = Mul8U(ax >> 2, ax >> 2);
        uint16_t dy = (uint16_t)(g.pucky[0] - YPOS(o));
        uint16_t ay = dy & 0x8000 ? (uint16_t)-dy : dy;
        g.puckdist2[o] = (uint16_t)(Mul8U(ay >> 2, ay >> 2) + g.puckdist2[o]);
        g.puckdist[o] = ApproxLength_abs(ax, ay);
        g.puckdir[o] = vtoa(dx, dy);
    }
}

/* assdumpin's target: dumpinplayer = o, dumpintarget and passdir from where he is. */
void DumpInTarget(int o)
{
    g.dumpinplayer = (uint16_t)(2 * o);
    if (!g.position[o]) {
        g.dumpintarget = 0;
        g.passdir = 0;
        if (g.pfgoal[o]) return;
        g.dumpintarget = 0xC;
        g.passdir = 4;
        return;
    }
    if (!g.pfgoal[o]) {
        if (NEG(g.Ypos[o], 0xFFA8)) {
            g.passdir = (uint16_t)(g.puckdir[13] - 4) & 7;
            g.dumpintarget = 8;
            return;
        }
        g.passdir = 4;
        g.dumpintarget = g.Xpos[o] & 0x8000 ? 6 : 0xA;
        return;
    }
    if (!NEG(g.Ypos[o], 0x58)) {
        g.passdir = (uint16_t)(g.puckdir[12] - 4) & 7;
        g.dumpintarget = 0x14;
        return;
    }
    g.passdir = 0;
    g.dumpintarget = g.Xpos[o] & 0x8000 ? 0x12 : 0x16;
}

/* IsBreakaway = 1 when the puck carrier, in the half he attacks, has no opposing skater between him
 * and the goal. Returns d0 as left (the carrier's y 4 nearer his goal line, once he is in that half;
 * else d0 as given): ChkOffsides_other reads it. */
uint16_t CheckBreakaway(uint16_t d0)
{
    g.IsBreakaway = 0;
    uint16_t pc = (uint16_t)g.puckc;
    if (pc & 0x8000) return d0;
    int down = (g.gmode & 2) ? !NEG(pc, 0xC) : NEG(pc, 0xC);   /* (.t3: attacking the + end) */
    uint16_t v = YPOS(pc >> 1);
    if (down ? (v & 0x8000) : !(v & 0x8000)) return d0;
    d0 = down ? (uint16_t)(v + 4) : (uint16_t)(v - 4);
    uint16_t y = NEG(pc, 0xC) ? 0xC : 0;
    for (int k = 0; k < 7; k++, y += 2) {
        uint16_t p = r16(RAM_OFF_position + y);
        if ((p & 0x8000) || !p) continue;
        uint16_t t = (uint16_t)(d0 - r16(RAM_OFF_Ypos + y));
        if (down ? (t & 0x8000) != 0 : !(t & 0x8000)) return d0;
    }
    g.IsBreakaway = 1;
    return d0;
}

/* d0 / d1 = the stick hot spot of object o's frame (its record's bytes $14 - $12 and $13 - $15),
 * x negated when flipped, y unless flipped vertically, turned in horizontal mode. */
void GetFrameHot(int o, uint16_t *d0, uint16_t *d1)
{
    uint32_t t = SYM_SpriteFrames + (uint16_t)((g.frame[o] - 1) * 4);
    uint16_t lo = rom16(t), hi = rom16(t + 2);
    uint32_t sum = lo + (SYM_SpriteFrames & 0xFFFF);
    uint32_t rec = ((hi + (SYM_SpriteFrames >> 16) + (sum >> 16)) & 0xFF) << 16 | (sum & 0xFFFF);
    uint16_t b12 = (uint16_t)(int8_t)m8(rec + 0x12), b13 = (uint16_t)(int8_t)m8(rec + 0x13);
    uint16_t b14 = (uint16_t)(int8_t)m8(rec + 0x14), b15 = (uint16_t)(int8_t)m8(rec + 0x15);
    uint16_t x = (uint16_t)(b14 - b12), y = (uint16_t)(b13 - b15);
    if (g.attribute[o] & 0x0800) x = (uint16_t)-x;
    if (!(g.attribute[o] & 0x1000)) y = (uint16_t)-y;
    if (g.sfhormode) { uint16_t t2 = x; x = y; y = (uint16_t)-t2; }
    *d0 = x; *d1 = y;
}

/* AssignPadPlayer, when player d0 has no pad yet. */
void AssignPadIfFree(uint16_t d0, uint16_t d4)
{
    if (r16(RAM_OFF_playerpad + d0) & 0x8000) AssignPadPlayer(d0, d4);
}

/* Glide backwards, unless player o's animation is locked. */
void GlideBack(int o)
{
    if (g.pfalock[o]) return;
    SetSPAFromA(o, (uint16_t)SYM_SPAglideback);
}

/* When player d0 has the puck and no pad: the next of his team's pads in turn (puckpadnext, from
 * pad 4 down), if it has any, takes him. */
void PuckCarrierPad(uint16_t d0)
{
    if (!(r16(RAM_OFF_playerpad + d0) & 0x8000)) return;
    uint16_t x = d0 < 0xC ? 0 : 2;
    if (!r16(RAM_OFF_teampads + x)) return;
    uint16_t y = (uint16_t)(r16(RAM_OFF_puckpadnext + x) - 2);
    if (y & 0x8000) y = 8;
    for (int n = 5; n; n--) {
        if (x == r16(RAM_OFF_padside + y)) {
            w16(RAM_OFF_puckpadnext + x, y);
            AssignPadPlayer(d0, y);
            return;
        }
        y = (uint16_t)(y - 2);
        if (y & 0x8000) y = 8;
    }
}

/* Pad d4 takes the player of its team in position d1, if he has no pad and is available
 * (pflags $1000 clear); padinputdone counts it. */
void PadTakePosition(uint16_t d1, uint16_t d4)
{
    uint16_t x = r16(RAM_OFF_cont1team + d4) == 1 ? 0 : 0xC;
    for (int n = 0; n < 6; n++, x += 2) {
        if (r16(RAM_OFF_position + x) != d1) continue;
        if (!(r16(RAM_OFF_playerpad + x) & 0x8000) || (r16(RAM_OFF_pflags + x) & 0x1000)) return;
        w16(RAM_OFF_padinputdone + d4, (uint16_t)(r16(RAM_OFF_padinputdone + d4) + 1));
        AssignPadPlayer(x, d4);
        return;
    }
}

/* Pad d4 takes its team's goalie. */
void PadTakeGoalie(uint16_t d4) { PadTakePosition(0, d4); }

/* With defence control on for player o's team: pad d4 takes the defenceman on the left of the
 * screen (LD shooting at the top goal, else RD). */
void PadTakeLeftDefense(int o, uint16_t d4)
{
    if (!g.defcontrol[2 * o < 0xC ? 0 : 1]) return;
    PadTakePosition(g.pfgoal[o] ? 1 : 2, d4);
}

/* The same for the defenceman on the right. */
void PadTakeRightDefense(int o, uint16_t d4)
{
    if (!g.defcontrol[2 * o < 0xC ? 0 : 1]) return;
    PadTakePosition(g.pfgoal[o] ? 2 : 1, d4);
}

/* Pad d4 of penalized player o goes to the first teammate on the ice with no pad (the search does
 * not stop before one is found). */
void PassPadToTeammate(int o, uint16_t d4)
{
    uint16_t y = 2 * o < 0xC ? 0 : 0xC;
    while (!(r16(RAM_OFF_playerpad + y) & 0x8000) || (r16(RAM_OFF_position + y) & 0x8000)) y = (uint16_t)(y + 2);
    PadTakePosition(r16(RAM_OFF_position + y), d4);
}

/* (FaceoffPads: a pad of the team (padside low byte side) whose side was moved off ($80xx) comes
 * back when the team has fewer pads on its side than skaters on the ice, counted against the
 * home team's pads (for both teams), from padsitnext on) */
static void fp_side(int t, uint16_t side, uint16_t xlo, uint16_t xhi)
{
    if (g.teampads[t] < 4) return;
    uint16_t cnt = 0, n = 0;
    for (uint16_t x = xhi; ; x = (uint16_t)(x - 2)) {
        uint16_t p = r16(RAM_OFF_position + x);
        if (p && !(p & 0x8000)) cnt++;
        if (x == xlo) break;
    }
    for (int x = 8; x >= 0; x -= 2) if (r16(RAM_OFF_padside + x) == side) n++;
    if (n >= cnt || n == g.teampads[0]) return;
    uint16_t y = g.padsitnext[t];
    for (;;) {
        y = (uint16_t)(y + 2);
        if (y == 0xA) y = 0;
        uint16_t ps = r16(RAM_OFF_padside + y);
        if (!(ps & 0x8000) || (ps & 0xFF) == 0x80 || (ps & 0xFF) != side) continue;
        w16(RAM_OFF_padside + y, side);
        w16(RAM_OFF_cont1team + y, (uint16_t)((side + 2) >> 1));
        return;
    }
}

/* (FaceoffPads: each pad of the team in turn from the one after fopadfirst gets a player
 * (findpc, run with the pad's index as the player); fopadfirst = the first) */
static void fp_pads(int t, uint16_t side, int a3)
{
    if (!g.teampads[t]) return;
    uint16_t x = (uint16_t)(g.fopadfirst[t] - 2);
    g.fopadfirst[t] = 0xFFFF;
    if (x & 0x8000) x = 8;
    for (int p0 = 5; p0; p0--) {
        if (r16(RAM_OFF_padside + x) == side) {
            if (g.fopadfirst[t] & 0x8000) g.fopadfirst[t] = x;
            findpc(x >> 1, x, a3);
        }
        x = (uint16_t)(x - 2);
        if (x & 0x8000) x = 8;
    }
}

/* At a faceoff: the distances to the puck, the pads moved for a penalty shot back on their sides,
 * pads given back to a team short of them, the pad and goalie tables reset and every pad given a
 * player again (findpc, a3 as given). */
void FaceoffPads(int a3)
{
    CalcPuckDistances();
    if (!g.penshot) {
        for (int y = 8; y >= 0; y -= 2) {
            uint16_t ps = r16(RAM_OFF_padside + y);
            if ((ps & 0xFF00) != 0x8000) continue;
            ps &= 0xFF;
            w16(RAM_OFF_padside + y, ps);
            w16(RAM_OFF_cont1team + y, (uint16_t)((uint16_t)(ps + 2) >> 1));
        }
    }
    fp_side(0, 0, 0, 0xA);
    fp_side(1, 2, 0xC, 0x16);
    g.benchcheer = 0;
    for (int x = 0x16; x >= 0; x -= 2) {
        w16(RAM_OFF_playerpad + x, 0xFFFF);
        w16(RAM_OFF_goalieflags + x, 0);
        w16(RAM_OFF_goalieturnwait + x, 0);
        w16(RAM_OFF_goaliedivewait + x, 0);
        w16(RAM_OFF_FaceoffUnused + x, 0);
    }
    g.dumpinplayer = 0xFFFF;
    for (int x = 8; x >= 0; x -= 2) w16(RAM_OFF_c1playernum + x, 0xFFFF);
    fp_pads(0, 0, a3);
    fp_pads(1, 2, a3);
}

/* (assdumpin's lob: aimed at the target's spot spread at random (more for a worse passer), its
 * speed from the distance (at least $10, a goalie $20, at most $60), lifted and spun; the carrier
 * (g.curobj) lets go of it and swings) */
static void dumpin_lob(int o, int a3)
{
    uint16_t d4 = (uint16_t)((uint16_t)(0xF - g.passacc[o]) << 1);
    uint16_t y = g.dumpintarget;
    uint16_t d0 = (uint16_t)(randomas((uint16_t)(rom16(SYM_assdumpin_6 + y) + d4)) + rom16(SYM_assdumpin_8 + y) - g.Xpos[14]);
    uint16_t d1 = (uint16_t)(randomas((uint16_t)(rom16(SYM_assdumpin_7 + y) + d4)) + rom16(SYM_assdumpin_ytab + y) - g.pucky[0]);
    uint16_t d2 = ApproxLength(d1, d0);
    uint16_t a = DivX8U((uint16_t)(d2 << 4), 0x78);
    if (a >= 0x60) a = 0x60;
    uint16_t lo = r16(RAM_OFF_position + g.curobj) ? 0x10 : 0x20;
    if (a < lo) a = lo;
    uint16_t d3 = a;
    g.puckvx[0] = (int16_t)(Div32S(Mul16x8XS(d0, d3), d2) << 8);
    g.puckvy[0] = (int16_t)(Div32S(Mul16x8XS(d1, d3), d2) << 8);
    g.puckvz = (int16_t)(randomd0A((uint16_t)(0x3C0 - d4)) + 0x10E0);
    puckflip(d0);
    int x = g.curobj >> 1;
    g.dumpinplayer = 0xFFFF;
    g.puckc = -1;
    w16(RAM_OFF_nopuck + g.curobj, 0x1E);
    g.lastplayer[0] = g.curobj >> 1;
    dopass_swing(x, a3);
}

/* Assignment $14: the puck carrier o dumps the puck in: once the target is picked a computer player
 * shoots instead toward $14 / 8 (CompShoot: returns nonzero, his caller ended too); he turns toward
 * passdir (a goalie moves the target by the step he still has to turn) and lobs it. Without the
 * puck: the assignment is over. */
int assdumpin(int o, int a3)
{
    if (2 * o != g.puckc) { assexit(o); return 0; }
    if (g.pflags[o] & 2) {
        DumpInTarget(o);
        g.pflags[o] &= 0xFFFD;
        if (g.playerpad[o] & 0x8000) {
            if (g.dumpintarget == 0x14 || g.dumpintarget == 8) { CompShoot(o); return 1; }
            return 0;
        }
    }
    uint16_t y = 0, a = g.passdir;
    if (a != g.facedir[o]) {
        y = 1; a = (uint16_t)(a - 1) & 7;
        if (a != g.facedir[o]) {
            y = 2; a = (uint16_t)(a + 2) & 7;
            if (a != g.facedir[o]) { AdjustFacingDirection(o, g.passdir); return 0; }
        }
    }
    if (!g.position[o]) {
        g.dumpintarget = (uint16_t)((y << 1) + g.dumpintarget);
        g.passdir = g.facedir[o];
    }
    dumpin_lob(o, a3);
    assexit(g.curobj >> 1);                                  /* (X as the lob left it) */
    return 0;
}


/* At the blue line: CheckBreakaway; a new breakaway queues sound 5 (loudness $7000, not during a
 * penalty shot), raises the crowd's excitement by 20 (unless that turns it negative) and counts for
 * the carrier's team. Returns d0 as CheckBreakaway leaves it. */
uint16_t StartBreakaway(uint16_t d0)
{
    d0 = CheckBreakaway(d0);
    g.breakaway = g.IsBreakaway;
    if (!g.breakaway) return d0;
    if (!g.penshot) {
        g.SfxVelocity = 0x7000;
        QueueSound(5);
    }
    uint16_t v = (uint16_t)(g.CwdExciteLvl + 0x14);
    if (!(v & 0x8000)) g.CwdExciteLvl = v;
    if (!g.penshot) g.tmBreakaways[NEG(g.puckc, 0xC) ? 0 : 1]++;
    return d0;
}


/* End the penalty shot or shootout attempt under way: no shooter, ShootoutWonBy 10, the faceoff
 * spot saved at its start back as the last touch. */
void EndPenaltyShotPlay(int o)
{
    if (!g.PenShotActive || !g.penshot) return;
    g.penshot = 0;
    g.BA_Sktr_SCnum = 0xFFFF;
    ShootoutWonBy(o, 0xA);                               /* (o: the X it was called with) */
    g.ltx = g.psfoxsave;
    w16(RAM_OFF_lty, g.psfoysave);
}

/* (Div32S's carry, which assgoalie adds in after it: by the divide's path; the long loops leave
 * bit 0 of r0 as it was, the 8-bit one bit 8, the hardware divider the carry it came with) */
static int div32u_carry(uint32_t n, uint16_t d, uint16_t r0, int cin)
{
    unsigned b3 = n >> 24, b2 = n >> 16 & 0xFF, b1 = n >> 8 & 0xFF, dh = d >> 8, dl = d & 0xFF;
    if (b3) return b3 >= dh ? 1 : r0 & 1;
    if (!dh) return b2 ? b2 >= dl : cin;
    if (!b2) return b1 >= dh ? r0 >> 8 & 1 : 0;
    return b2 < dh ? r0 >> 8 & 1 : r0 & 1;
}
/* (assgoalie: the carry Div32S leaves for n / d; a negative divisor or dividend is turned positive
 * first, and a quotient of 0 from that gives 1) */
static int div32s_carry(uint32_t n, uint16_t d, uint16_t r0, int cin)
{
    if (d & 0x8000) {
        d = (uint16_t)-d;
        if (!(n & 0x80000000u)) return Div32U(n, d) == 0;
        return div32u_carry((uint32_t)-(int32_t)n, d, r0, 0);
    }
    if (n & 0x80000000u) return Div32U((uint32_t)-(int32_t)n, d) == 0;
    return div32u_carry(n, d, r0, cin);
}

static uint16_t abs16(uint16_t v) { return v & 0x8000 ? (uint16_t)-v : v; }

/* (assgoalie's .49: a goalie between the goal lines clamps the aim's y to the crease ($FEFD-$103;
 * x 0 with the puck); then y made relative to the goal line d3) */
static void ag_clamp(int o, uint16_t *d0, uint16_t *d1, uint16_t d3)
{
    if (NEG(g.Ypos[o], 0x108) && !NEG(g.Ypos[o], 0xFEF8)) {
        if (2 * o == g.puckc) *d0 = 0;
        if (!NEG(*d1, 0x103)) *d1 = 0x103;
        if (NEG(*d1, 0xFEFD)) *d1 = 0xFEFD;
    }
    *d1 = (uint16_t)(*d1 - d3);
}

/* (assgoalie's lead of the puck by half its velocity's high byte again: r3 + r3 / 2 + bit 0) */
static uint16_t ag_lead(uint16_t r3) { return (uint16_t)((uint16_t)((int16_t)r3 >> 1) + r3 + (r3 & 1)); }

/* Assignment 0: the goalie. In and near his crease (goalieflags bits 1-2 as in assgoalietopuck);
 * a computer goalie out of position skates back to it; a human one with the clock stopped or the
 * puck stays put. Holding the puck too long (temp5) is a penalty. On his timer the computer goalie
 * passes (threat) or covers the puck, pokes or dives at a carrier coming in (a human one on his
 * buttons); otherwise he faces the puck and moves across the crease toward where it is going (by
 * its crossing of the goal line, puckcross), making a save (GoalieSaveList) when it comes at him,
 * or stops dead once against a shooter. A loose puck dumped around the boards far from both teams
 * sends him after it (assignment $F). (r0, r0+2 and r1 are the direct page's as they were where
 * the original reads them before writing them: its divide's carry, the facing tests.) */
void assgoalie(int o, uint16_t a2, int a3)
{
    uint16_t r0 = g.r0[0], r0h = g.r0[1], r1 = g.r1[0];
    uint16_t d0 = 0, d1 = 0, d2 = 0, d3 = 0, d4, a, y, sd0, sd1, a0 = 0;
    uint16_t px = (uint16_t)g.Xpos[o];
    g.goalieflags[o] |= 6;
    if (!NEG(px, 0xFFD8) && NEG(px, 0x29)) {
        r0 = abs16((uint16_t)g.Ypos[o]);
        if (!NEG(r0, 0xE4) && NEG(r0, 0x104)) g.goalieflags[o] &= 0xFFFB;
    }
    if (!NEG(px, 0xFFD0) && NEG(px, 0x31)) {
        r0 = abs16((uint16_t)g.Ypos[o]);
        if (!NEG(r0, 0xE4)) g.goalieflags[o] &= 0xFFFD;
    }
    if (check4bench(o)) return;   /* (it abandons its caller when it acts) */
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp1[o] = 0;
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
        g.temp4[o] = -1;
    }
    px = (uint16_t)g.Xpos[o];
    if (!(g.playerpad[o] & 0x8000)) goto t2;
    if (!NEG(px, 0xFFCB) && NEG(px, 0x35)) {
        a = abs16((uint16_t)g.Ypos[o]);
        if (a < 0x10F && a >= 0xD3) goto l13;
    }
    skateto(o, px & 0x8000 ? 0xFFFC : 4, g.pfgoal[o] ? 0xFF0C : 0xF4, SYM_rtss, a3);
    return;
t2:
    if (!g.gmclock && 2 * o != g.puckc) goto l12;
    g.goalieflags[o] |= 1;
    goto l13;
l12:
    if (g.goalieflags[o] & 1) goto l13;
    if (g.goalieflags[o] & 2) {
        g.temp2[o] = 0;
        skateto(o, px & 0x8000 ? 0xFFFC : 4, g.pfgoal[o] ? 0xFF07 : 0xF9, SYM_rtss, a3);
        return;
    }
    if (r16(RAM_OFF_paddir + g.playerpad[o]) == 8) stopna(o);
l13:
    if (!g.pf2aip[o]) SetSPAFromA(o, (uint16_t)SYM_SPAgready);
    if (g.gmclock) return;
    if (!(g.temp5[o] & 0x8000)) {
        if (2 * o != g.puckc) g.temp5[o] = -1;
        else {
            if (NEG(g.Ypos[o], 0xBB) && !NEG(g.Ypos[o], 0xFF45)) g.temp5[o] = 0;
            g.temp5[o] = (int16_t)(g.temp5[o] - g.d7);
            if (g.temp5[o] < 0) AddPenalty2(o, PenGhold);
        }
    }
    if (g.pf2aip[o]) return;
    g.temp1[o] = (int16_t)(g.temp1[o] - g.d7);
    if (g.temp1[o] >= 0) goto l45;
    a = (uint16_t)((g.aidef[o] >> 1) + g.temp1[o]);
    g.temp1[o] = (int16_t)(a & 0x8000 ? 0 : a);
    if (!(g.playerpad[o] & 0x8000)) goto l23;
    d0 = (uint16_t)g.pucky[0];
    d1 = (uint16_t)(g.Ypos[o] ^ d0);
    if (d1 & 0x8000) {
        d0 = 0;
        d2 = g.pfgoal[o] ? 0xFF0C : 0xF4;
        goto l44;
    }
    g.temp4[o]--;
    if (g.temp4[o] < 0) g.temp4[o] = -1;
    if (2 * o != g.puckc) goto l23;
    if (g.temp5[o] < 0) g.temp5[o] = 0x5A;
    g.temp4[o] = -1;
    if (!((uint16_t)(g.temp5[o] - 0x5A) & 0x8000)) goto l24;
    if (RandomWord() < 0xC000) goto l24;
    y = g.pfteam[o] ? 0 : 0xC;
    for (int n = 5; n >= 0; n--, y += 2) {
        if (r16(RAM_OFF_pflags + y) & 0x1000) continue;
        a = r16(RAM_OFF_puckdist + y);
        if (a >= 0x23) continue;
        if (a >= 0x14) goto l24;
        g.pf2aip[o] = 0x5A;
        SetSPAFromA(o, (uint16_t)SYM_SPAgcover);
        return;
    }
    g.threat = 1;
    if (chk4pass(o, a3)) return;
    if (g.puckc < 0) return;
    goto l24;
l21:
    a = vtoa(d0, d1);
l22:
    g.facedir[o] = a;
    g.pfalock[o] = a;
    SetSPAFromA(o, (uint16_t)SYM_SPAgpoke);
    g.goaliedivewait[o] = 0xF0;
    return;
l23:
    y = g.playerpad[o];
    if (y & 0x8000 || r16(RAM_OFF_padinputdone + y) != 1) goto t3;
    if (2 * o == g.puckc) goto l24;
    d0 = (uint16_t)(g.Xpos[14] - g.Xpos[o]);
    d1 = (uint16_t)(g.pucky[0] - g.Ypos[o]);
    if (r16(RAM_OFF_JoyPad + y) & 0x4000) {
        if (r16(RAM_OFF_paddir + y) == 8) goto t4;
        a = r16(RAM_OFF_paddir + y) & 7;
        goto dive;
    }
    if (!(r16(RAM_OFF_padnew + y) & 0x40)) goto l24;
    if (r16(RAM_OFF_paddir + y) == 8) goto l21;
    a = r16(RAM_OFF_paddir + y) & 7;
    goto l22;
t3:
    d0 = (uint16_t)(g.Xpos[14] - g.Xpos[o]);
    d1 = (uint16_t)(g.pucky[0] - g.Ypos[o]);
    if (!(g.puckc & 0x8000)) {
        if (r16(RAM_OFF_pfteam + ((uint16_t)g.puckc >> 1)) == g.pfteam[o]) goto l24;   /* (pfteam by the carrier's number, a byte index) */
        if (g.goaliedivewait[o]) goto l24;
        if (RandomWord() >= 0xA00) goto l24;
        if (g.puckdist[o] >= 0x23) goto l24;
        if (NEG(g.pucky[0], 0xFEF7) || !NEG(g.pucky[0], 0x109)) goto l24;
        goto l21;
    }
    if (g.temp4[o]) goto l24;
    if (g.puckdist[o] >= 0x2D) goto l24;
    if (NEG(g.pucky[0], 0xFEF7) || !NEG(g.pucky[0], 0x109)) goto l24;
    if (r16(RAM_OFF_tmnear + (a2 ^ 2)) >= 0x3C) goto l24;
    if (RandomWord() >= 0x800) goto l24;
t4:
    a = vtoa(d0, d1);
dive:
    g.facedir[o] = a;
    SetSPAFromA(o, (uint16_t)SYM_SPAgdive);
    g.goaliedivewait[o] = 0x168;
    a = (uint16_t)(g.CwdExciteLvl + 0xA);
    if (!(a & 0x8000)) g.CwdExciteLvl = a;
    g.pfalock[o] = 0x96;
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x96);
    g.crowdhold = 0x1E;
    return;
l24:
    if (!g.pfgoal[o]) { a0 = RAM_OFF_puckcross; d3 = 0x104; d4 = 0x108; }
    else { a0 = RAM_OFF_puckcross + 4; d3 = 0xFEFC; d4 = 0xFEF8; }
    if (2 * o != g.puckc) {
        GoaliePuckVector(o, &d0, &d1);
        d0 = vtoa(d0, d1);
        AdjustFacingDirection(o, d0);
    } else if (g.playerpad[o] & 0x8000) {
        d0 = (uint16_t)((g.pfgoal[o] ^ 1) << 2);
        AdjustFacingDirection(o, d0);
    }
    if (NEG(g.Ypos[o], 0xFEFB) || !NEG(g.Ypos[o], 0x105)) {
        if (g.playerpad[o] & 0x8000) {
            d2 = 0;
            d0 = 0;
            px = (uint16_t)g.Xpos[o];
            if (!NEG(px, 0xFFD3) && NEG(px, 0x2D)) d0 = px & 0x8000 ? 0xFF78 : sideline;
            goto l44;
        }
    }
    d0 = (uint16_t)g.Xpos[14];
    d1 = (uint16_t)g.pucky[0];
    ag_clamp(o, &d0, &d1, d3);
    if (!NEG(d1, 0xFFD2) && NEG(d1, 0x2E)) { sd0 = d0; sd1 = d1; goto l31; }
    a = (uint16_t)g.puckvy[0] >> 9 & 0x7F;
    if (a & 0x40) a |= 0xFF80;
    d1 = (uint16_t)(ag_lead(a) + g.pucky[0]);
    if (!NEG(d1, 0xE0) || NEG(d1, 0xFF20)) {
        if (!(g.passReceiverPlayerNum & 0x8000) || (g.puckc & 0x8000)) {
            a = (uint16_t)g.puckvx[0] >> 9 & 0x7F;
            if (a & 0x40) a |= 0xFF80;
            d0 = (uint16_t)(a + g.Xpos[14]);
            goto l30;
        }
        if (!(g.seed[0] & 0x180)) goto l29;
    } else if (g.seed[0] & 0x380) goto l29;
    d0 = (uint16_t)(ag_lead((uint16_t)(int8_t)((uint16_t)g.puckvx[0] >> 8)) + g.Xpos[14]);
    goto l30;
l29:
    d0 = (uint16_t)((uint16_t)(int8_t)((uint16_t)g.puckvx[0] >> 8) + g.Xpos[14]);
l30:
    ag_clamp(o, &d0, &d1, d3);
    sd0 = d0; sd1 = d1;
l31:
    d0 = ApproxLength(d0, d1);
    if (d0 < 0x1E) { d0 = sd0; d1 = sd1; }
    else {
        d2 = (uint16_t)(d0 + 1);
        d4 = g.sflags & 8 ? 0x1C : 0x14;
        d0 = sd0; d1 = sd1;
        cf = 1;                                           /* (the cmp before: a zero product keeps it) */
        uint32_t p = Mul16x8XS(d1, d4);
        int c = div32s_carry(p, d2, r0, cf);
        d1 = Div32S(p, d2);
        d4 = (uint16_t)(d4 + 6 + c);                      /* (adc: the divide's carry) */
        d0 = Div32S(Mul16x8XS(d0, d4), d2);
        r0 = d0; r1 = d2;
        o = g.curobj >> 1;
    }
    d1 = (uint16_t)(d1 + d3);
    d2 = d1;
    y = g.playerpad[o];
    if (!(y & 0x8000)) {
        if (2 * o == g.puckc || !(r16(RAM_OFF_JoyPad + y) & 0x80)) goto l43;
        goto t3x;
    }
    if (r16(a0 + 2) >= 0x23) goto l44;
    a = r16(a0);
    if (NEG(a, 0xFFE7) || !NEG(a, 0x19)) goto l48;
    if (r16(a0 + 2) >= 0x13 || NEG(g.pucky[0], 0xFEF7) || !NEG(g.pucky[0], 0x109)) goto l43;
t3x:
    g.Xvel[o] = (int16_t)(g.Xvel[o] >> 1);                /* (cmp #$8000, ror: the sign kept) */
    g.Yvel[o] = (int16_t)(g.Yvel[o] >> 1);
    if (g.goalieflags[o] & 2) {
        d3 = (uint16_t)(g.puckdir[o] - g.facedir[o]) & 7;
        if (!d3) goto l33;
        a = d3;
        goto l41;
    }
    a = (uint16_t)g.puckvx[0] >> 10 & 0x3F;
    if (a & 0x20) a |= 0xFFC0;
    d0 = (uint16_t)(a + g.Xpos[14] - g.Xpos[o]);
    r0h = d0;
    r0 = abs16(d0);
    a = (uint16_t)g.puckvy[0] >> 10 & 0x3F;
    if (a & 0x20) a |= 0xFFC0;
    d1 = (uint16_t)(a + g.pucky[0] - g.Ypos[o]);
    r1 = abs16(d1);
    d0 = vtoa(d0, d1);
    if (d0 == 8) goto l33;
    d3 = (uint16_t)(d0 - g.facedir[o]) & 7;
    if (!d3) goto l33;
    a = d3;
    goto l41;
l32:
    if (!NEG(g.puckvy[0], 0x800) || NEG(g.puckvy[0], 0xF800)) goto l33;
    d0 = 8;
    goto l42;
l33:
    switch (g.facedir[o]) {
    case 0: a = r0h & 0x8000 ? 7 : 1; break;
    case 4: a = r0h & 0x8000 ? 1 : 7; break;
    case 1: case 5: a = NEG(r0, r1) ? 7 : 1; break;
    case 7: case 3: a = NEG(r1, r0) ? 7 : 1; break;
    default: a = g.seed[0] & 1; break;
    }
l41:
    if (a == 4) goto l32;
    d0 = a >> 2;
    if (g.attribute[o] & 0x0800) d0 ^= 1;
    a = d3;
    if (a >= 4) a = (uint16_t)(~a + 9);                   /* (eor, adc with the carry of the cmp) */
    if (a >= 2) { d0 = (uint16_t)(d0 + 6); goto l42; }
    if (!((uint16_t)(g.puckz - 8) & 0x8000)) goto l42;
    if (!((uint16_t)(g.puckvz - 0x800) & 0x8000)) goto l42;
    d0 = (uint16_t)(d0 + 4);
    if (r16(a0 + 2) < 9 || g.facedir[o] == 2 || g.facedir[o] == 6 || (g.puckc & 0x8000)) goto l42;
    d0 = (uint16_t)(d0 - 2);
l42:
    d0 = (uint16_t)(d0 << 1);
    a = m16((SYM_GoalieSaveList + d0) & 0xFFFFFF);
    g.pfalock[o] = a;
    SetSPAFromA(o, a);
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x96);
    g.crowdhold = 0x1E;
l43:
    d0 = r16(a0);
    if (!((uint16_t)(g.pucky[0] - 0xFC) & 0x8000) || ((uint16_t)(g.pucky[0] - 0xFF04) & 0x8000))
        d0 = g.Xpos[14] < 0 ? 0xFFE8 : 0x18;
l44:
    if (!(g.playerpad[o] & 0x8000)) return;
    y = (uint16_t)g.puckc;
    if (!(y & 0x8000) && g.pfteam[o] != r16(RAM_OFF_pfteam + y)) {
        uint16_t s = r16(RAM_OFF_SPA + y);
        if (s >= (uint16_t)SYM_SPAshotf && s < 0xF021
            && (abs16(r16(RAM_OFF_Xvel + y)) >> 1) < abs16(r16(RAM_OFF_Yvel + y))) {
            if (!g.goaliestopped) {
                g.Xvel[o] = 0;
                g.Yvel[o] = 0;
                g.goaliestopped++;
                return;
            }
        } else g.goaliestopped = 0;
    }
    sd0 = d0;
    d1 = (uint16_t)(d2 - (uint16_t)(int8_t)((uint16_t)g.Yvel[o] >> 8) - g.Ypos[o]);
    d0 = (uint16_t)(sd0 - (uint16_t)(int8_t)((uint16_t)g.Xvel[o] >> 8) - g.Xpos[o]);
    {
        uint16_t tmp0 = 2 * o == g.puckc ? 10 : 5;
        if (abs16(d0) < tmp0 && abs16(d1) < tmp0) { d0 = 0; d1 = 0; }
    }
    g.temp2b[o] = (int16_t)vtoa(d0, d1);
l45:
    if (!(g.playerpad[o] & 0x8000)) return;
    d2 = (uint16_t)g.temp2b[o];
    if ((uint16_t)(d2 - 7) == 0 || ((uint16_t)(d2 - 7) & 0x8000)) playeracc(d2);
    else stopna(o);
    return;
l48:
    if (!(g.puckc & 0x8000) || (g.iflags & 4)) goto l44;
    if (NEG(abs16((uint16_t)g.Xpos[14]), 0x68)) goto l44;
    if (NEG(abs16((uint16_t)g.puckvy[0]), 0x3800)) goto l44;
    if (NEG(g.pucky[0], 0xB8) && !NEG(g.pucky[0], 0xFF48)) goto l44;
    a = (uint16_t)g.puckvy[0];
    if (g.pfgoal[o]) a = (uint16_t)-a;
    if (NEG(a, 0xFC00)) goto l44;
    if (g.tmnear[0] < 0x132 || g.tmnear[1] < 0x132) goto l44;
    assinsert(o, 0xF);
}

#define SPFPen 361

/* Assignment $16: wait (unless locked in an animation) until the face off is over. */
void assfaceoff(int o)
{
    if (g.pfalock[o] || (g.sflags & 0x100)) return;
    assexit(o);
}

/* Assignment $0E: sit player o in the penalty box (PBnum: $10 a home player, 1 a visitor). */
void assdopen(int o)
{
    g.PBnum = (uint16_t)(g.PBnum + (g.pfteam[o] ? 1 : 0x10));
    g.position[o] = -1;
    g.frame[o] = g.sfhormode ? SPFPen + 7 : SPFPen + 8;
}

/* Assignment $17: a player taking the face off. His pull frame goes in fofdata2 (+4 for the
 * visitors); a computer player starts the face off swing, the reverse one 7 times in 8 once the
 * puck's temp1 passes $10. When the face off is over: no puck collision for 20 frames, and out. */
void assfaceoffpl(int o)
{
    if (g.pfalock[o]) return;
    if (!(g.sflags & 0x100)) {
        g.nopuck[o] = 0x14;
        assexit(o);
        return;
    }
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp1[o] = 0;
    }
    uint16_t d0 = (uint16_t)((g.SPAnum[o] >> 2) + 1);
    if (!g.handed[o]) d0 = (uint16_t)(d0 + 3);
    w16(RAM_OFF_fofdata2 + (g.pfteam[o] ? 4 : 0), d0);
    if (!(g.playerpad[o] & 0x8000)) return;
    uint16_t was = g.pf2aip[o];
    g.pf2aip[o] = 1;
    if (was) return;
    uint16_t anim = (uint16_t)SYM_SPAfaceoff;
    if ((uint16_t)g.temp1[14] >= 0x11 && (RandomWord() & 7)) anim = (uint16_t)SYM_SPAfaceoffr;
    g.pf2aip[o] = 1;
    SetSPA(o, anim);
}

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */

/* Assignment 9, the Stanley Cup: skate to the side of the screen (x +-90, the scroll y) holding
 * the cup, then face that way, locked 20 frames. */
void assstanley(int o)
{
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp3[o] = g.Hpos < 0 ? (int16_t)0xFFA6 : 0x5A;
        g.temp4[o] = g.Vpos;
        g.pfalock[o] = (uint16_t)SYM_SPAstanley;
        SetSPAFromA(o, (uint16_t)SYM_SPAstanley);
    }
    uint16_t d = vtoa((uint16_t)(g.temp3[o] - g.Xpos[o]), (uint16_t)(g.temp4[o] - g.Ypos[o]));
    if (!NEG(d, 8)) return;
    g.facedir[o] = d;
    g.pfalock[o] = 0x14;
    playeracc(d);
}

/* When a line change wants player o off (and he caused no penalty): to the bench (assignment $0C,
 * pf2unav) unless he is going there already or has the puck, or, for the same roster player, a new
 * assignment from his new position. Returns nonzero when it ends its caller's assignment. */
int check4bench(int o)
{
    if (g.pflags[o] & 0x4000) return 0;
    if (g.newpos[o] < 0 && g.newpnum[o] < 0) return 0;
    if (g.newpnum[o] == (int16_t)g.pnum[o]) {
        g.pflags[o] &= 0xEFFF;
        g.pfnc[o] = 0;
        g.newpnum[o] = -1;
        g.newpos[o] = -1;
        Setplass(o);
        return 1;
    }
    if (g.asscur[o] == 0xC || 2 * o == g.puckc) return 0;
    g.pflags[o] |= 0x1000;
    g.temp1[o] = 0;
    assreplace(o, 0xC);
    return 1;
}

/* A computer player stands still (doplayeracc with no direction); a human is left alone. */
void donothing(int o, int a3)
{
    if (!(g.playerpad[o] & 0x8000)) return;
    doplayeracc(o, 8, a3);
}

/* (assdefo / asswingd / assdefd's start, as asswingo's: on offense the carrier gets asspuckc ($11)
 * on top and the other team's puck makes it assignment def, on defense his own team's does; the
 * clock stopped: donothing; check4bench; a human is left alone; the first time, the timers reset.
 * Nonzero: go on.) */
static int ass_start(int o, int a3, uint8_t def, int offense)
{
    uint16_t pc = (uint16_t)g.puckc;
    if (offense && 2 * o == g.puckc) { assinsert(o, 0x11); return 0; }
    if (!(pc & 0x8000) && (g.pfteam[o] == r16(RAM_OFF_pfteam + pc)) != offense) { assreplace(o, def); return 0; }
    if (g.gmclock) { donothing(o, a3); return 0; }
    if (check4bench(o)) return 0;
    if (!(g.playerpad[o] & 0x8000)) return 0;
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.temp1[o] = 0;
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
    }
    return 1;
}

/* (temp1 -= d7; when it runs out, plus add, at least 0; nonzero then) */
static int ass_timer(int o, uint16_t add)
{
    uint16_t t = (uint16_t)(g.temp1[o] - g.d7);
    g.temp1[o] = (int16_t)t;
    if (!(t & 0x8000)) return 0;
    t = (uint16_t)(t + add);
    g.temp1[o] = (int16_t)(t & 0x8000 ? 0 : t);
    return 1;
}

/* Assignment 1: a defenseman on offense holds the blue line on his side (x +-80, at the puck x when
 * it is on his side, halfway otherwise; y by the puck's zone, 98 when offside), avoiding the
 * carrier. */
void assdefo(int o, uint16_t a2, int a3)
{
    if (!ass_start(o, a3, 2, 1)) return;
    ass_timer(o, g.aioff[o]);
    uint16_t d0 = g.position[o] == 2 ? 0x50 : 0xFFB0;
    if (!g.pfgoal[o]) d0 = (uint16_t)-d0;
    uint16_t px = (uint16_t)g.Xpos[14];
    d0 = (px ^ d0) & 0x8000 ? (uint16_t)((uint16_t)(g.Xpos[14] >> 1) + d0) : px;
    uint16_t d1;
    if (r16(RAM_OFF_tmflags + a2) & tmfoside) d1 = g.pfgoal[o] ? 0xFF9E : 0x62;
    else {
        uint16_t d2 = (uint16_t)g.pucky[0];
        if (!g.pfgoal[o]) d2 = (uint16_t)-d2;
        d1 = NEG(d2, 0xFFA8) ? 0xFF28 : NEG(d2, 0x58) ? 0xFFB2 : 0x62;
        if (!g.pfgoal[o]) d1 = (uint16_t)-d1;
    }
    skateto(o, d0, d1, SYM_EvadePC, a3);
}

/* Assignment 3: a wing on defense covers his side (x +-100, at the puck x when it is on his side),
 * at a y following the puck between his blue line and goal line (the limit when the carrier is a
 * goalie or his team is offside). (A loose puck's carrier index reads the word before position[0]:
 * by address, in the program bank's mirror.) */
void asswingd(int o, uint16_t a2, int a3)
{
    if (!ass_start(o, a3, 4, 0)) return;
    ass_timer(o, g.aidef[o]);
    uint16_t d0 = g.position[o] == 5 ? 0xFF9C : 0x64, d1, d2;
    uint16_t pcpos = r16((RAM_OFF_position + (uint16_t)g.puckc) & 0xFFFF);
    uint16_t py = (uint16_t)g.pucky[0];
    int lim;
    if (g.pfgoal[o]) {
        d0 = (uint16_t)-d0;
        d1 = 0xFF62; d2 = 0xFFB2;
        if (!pcpos || (r16(RAM_OFF_tmflags + a2) & tmfoside)) lim = 1;
        else if (!((uint16_t)(d2 - py) & 0x8000)) { d2 = 0xFEF8; lim = !((uint16_t)(d2 - py) & 0x8000); }
        else { d1 = py; lim = 0; }
    } else {
        d1 = 0x9E; d2 = 0x4E;
        if (!pcpos || (r16(RAM_OFF_tmflags + a2) & tmfoside)) lim = 1;
        else if (!((uint16_t)(d2 - py) & 0x8000)) { d1 = py; lim = 0; }
        else { d2 = 0x108; lim = ((uint16_t)(d2 - py) & 0x8000) != 0; }
    }
    if (lim) d1 = d2;
    if (!(((uint16_t)g.Xpos[14] ^ d0) & 0x8000)) d0 = (uint16_t)g.Xpos[14];
    skateto(o, d0, d1, SYM_rtss, a3);
}

/* Assignment 2: a defenseman on defense: when his timer runs out, a spot (x +-65, 0 when the puck is
 * on the other side) 40 behind the furthest attacker (led by his velocity toward the goal; at most
 * 220 out); skates there, then looks for a check. (The puck lead and carrier tests it then makes
 * are left unused.) */
void assdefd(int o, uint16_t a2, int a3)
{
    if (!ass_start(o, a3, 1, 0)) return;
    if (ass_timer(o, g.aidef[o])) {
        /* (the original's test of the puck against the blue line, its results dropped: only its chkpk
         * call is kept) */
        uint16_t d1 = (uint16_t)g.pucky[0], d3 = (uint16_t)((uint16_t)g.puckvy[0] >> 6);
        if (d3 & 0x0200) d3 |= 0xFC00;
        if (!g.pfgoal[o]) { d1 = (uint16_t)-d1; d3 = (uint16_t)-d3; }
        if (d3 & 0x8000) d3 = 0;
        d3 = (uint16_t)(d3 + d1);
        if (!NEG(d3, 0x58) && !(r16(RAM_OFF_tmflags + a2) & tmfoside) && !(g.puckc & 0x8000)) chkpk(o);
        g.temp3[o] = g.position[o] == 2 ? 0x41 : (int16_t)0xFFBF;
        uint16_t y = 2 * o >= 0xC ? 0 : 0xC;
        int up = g.pfgoal[o] != 0;
        uint16_t d0 = up ? 0x3E8 : 0xFC18;
        for (int n = 0; n < 6; n++, y += 2) {
            if ((r16(RAM_OFF_pflags + y) & 0x1000) || (r16(RAM_OFF_position + y) & 0x8000)) continue;
            uint16_t v = r16(RAM_OFF_Yvel + y);
            if (up ? !(v & 0x8000) : (v & 0x8000) != 0) v = 0;
            v >>= 5;
            if (v & 0x400) v |= 0xF800;
            uint16_t ny = (uint16_t)(v + r16(RAM_OFF_Ypos + y));
            if (up ? NEG(ny, d0) != 0 : !NEG(ny, d0)) d0 = ny;
        }
        if (up) {
            d0 = (uint16_t)(d0 - 0x28);
            if (NEG(d0, 0xFF42)) d0 = 0xFF24;
        } else {
            d0 = (uint16_t)(d0 + 0x28);
            if (!NEG(d0, 0xBE)) d0 = 0xDC;
            g.temp3[o] = (int16_t)-g.temp3[o];
        }
        g.temp4[o] = (int16_t)d0;
        if (((uint16_t)g.temp3[o] ^ (uint16_t)g.Xpos[14]) & 0x8000) g.temp3[o] = 0;
    }
    skateto(o, (uint16_t)g.temp3[o], (uint16_t)g.temp4[o], SYM_rtss, a3);
    check4check(o, a3);
}

/* (assbench's .8: the new position (newpos) and its assignment; the requests cleared) */
static void bench_pos(int o)
{
    g.position[o] = g.newpos[o];
    Setplass(o);
    g.newpnum[o] = -1;
    g.newpos[o] = -1;
}

/* Assignment $0C: the player skates to the bench door (x -144, y -80 home / +80 visitors) unless
 * check4bench ends it (or he caused a penalty: out); there he glides into the wall and is locked
 * (temp1 = 100); then his slot goes to the bench and the new player comes on (setplayer). The same
 * roster player coming on only changes position. */
void assbench(int o, int a3)
{
    if (g.temp1[o] == 0x64) {
        w16(RAM_OFF_tmpdst + (uint16_t)((g.pfteam[o] ? 0x34 : 0) + (uint16_t)(g.pnum[o] << 1)), TMPDBENCH);
        uint16_t d3 = (uint16_t)g.newpnum[o];
        bench_pos(o);
        setplayer(o, d3);
        return;
    }
    if (check4bench(o)) return;
    if (g.pflags[o] & 0x4000) { assexit(o); return; }
    if (g.pflags[o] & 2) {
        g.pflags[o] &= 0xFFFD;
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
        g.temp4[o] = g.pfteam[o] ? 0x50 : (int16_t)0xFFB0;
        g.temp3[o] = (int16_t)0xFF70;
        g.temp1[o] = 0;
    }
    if (g.newpnum[o] == (int16_t)g.pnum[o]) {
        g.pflags[o] &= 0xEFFF;
        bench_pos(o);
        return;
    }
    if (!g.position[o]) g.goalieflags[o] |= 3;
    uint16_t t = (uint16_t)(g.temp1[o] - g.d7);
    unsigned c = (uint16_t)g.temp1[o] >= g.d7;
    g.temp1[o] = (int16_t)t;
    if (t & 0x8000) {
        g.temp1[o] = (int16_t)(t + 8 + c);
        uint16_t dy = (uint16_t)(g.Ypos[o] - g.temp4[o]);
        if (dy & 0x8000) dy = (uint16_t)-dy;
        uint16_t d0 = (uint16_t)(g.Xpos[o] - g.temp3[o]);
        uint16_t v = (uint16_t)(d0 - 0x20);
        if (dy < 0x29 && (!v || (v & 0x8000))) {
            SetSPAFromA(o, (uint16_t)(g.position[o] ? SYM_SPAglide : SYM_SPAgready));
            g.pfnc[o] = 1;
            if (g.facedir[o] != 4) g.facedir[o] = (uint16_t)(g.facedir[o] + 1) & 7;
            g.Yvel[o] = 0;
            g.Xvel[o] = (int16_t)0xF800;
            v = (uint16_t)(d0 - 0x10);
            if (v && !(v & 0x8000)) return;
            g.Xvel[o] = 0;
            if (g.facedir[o] != 4) return;
            g.Xvel[o] = (int16_t)0xF800;
            g.facedir[o] = 2;
            SetSPAFromA(o, (uint16_t)(g.position[o] ? SYM_SPAwallleft : SYM_SPAgwallleft));
            g.temp1[o] = 0x64;
            g.pfalock[o] = 0x64;
            return;
        }
    }
    if (g.pfnc[o] || (g.pflags[o] & 0x4008)) return;
    skateto(o, (uint16_t)g.temp3[o], (uint16_t)g.temp4[o], SYM_EvadePC, a3);
}


/* Assignment $0A: coming on from the bench: first at the door (x -136, y 14 per slot from his
 * object number), locked in the wall animation; when it is over he faces right and skates out
 * ($1000), the assignment over. */
void asseben(int o)
{
    if (g.pfalock[o]) return;
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.Xvel[o] = 0;
        g.Yvel[o] = 0;
        g.goalieflags[o] = 0;
        uint16_t s = (uint16_t)(o - 6);
        if (!(s & 0x8000)) s++;
        g.Ypos[o] = (int16_t)Mul16x8XS(s, 0xE);
        g.Xpos[o] = (int16_t)0xFF78;
        g.facedir[o] = 2;
        g.pfalock[o] = 2;
        SetSPAUpdate(o, (uint16_t)(g.position[o] ? SYM_SPAwallright : SYM_SPAgwallright));
        return;
    }
    g.facedir[o] = 4;
    g.pflags[o] &= 0x6FFF;
    g.pfnc[o] = 0;
    g.SPA[o] = 0;
    g.Xvel[o] = 0x1000;
    assexit(o);
}

/* Assignment 8: celebrating a goal (bench a2 + 2 cheers when gmode $40): unless a human holds a
 * button, player a3 (from the slow-down on: o and a3 are the same player in the game) slows to 3/4,
 * drops the puck (a carrier test that compares his number with puckc's index), skates to the side
 * of the screen and celebrates there now and then (the scorer pumps his arm). */
void assscore(int o, uint16_t a2, int a3)
{
    if (g.gmode & 0x40) StartBenchCheer((uint16_t)(a2 + 2));
    uint16_t y = g.playerpad[o];
    if (!(y & 0x8000) && (r16(RAM_OFF_JoyPad + y) & 0xFFF0)) return;
    int x = a3;
    g.Xvel[x] = (int16_t)(Mul16x8XS((uint16_t)g.Xvel[o], 0xC0) >> 8);
    g.Yvel[x] = (int16_t)(Mul16x8XS((uint16_t)g.Yvel[x], 0xC0) >> 8);
    if ((uint16_t)x == (uint16_t)g.puckc) {
        g.puckc = -1;
        g.nopuck[x] = 0x708;
    }
    if (g.pflags[x] & pfna) {
        g.pflags[x] &= 0xFFFD;
        g.temp1[x] = (int16_t)randoma8(0x78);
        g.temp2[x] = 0;
        g.temp2b[x] = 8;
        g.temp3[x] = g.Hpos < 0 ? (int16_t)0xFFA6 : 0x5A;
        g.temp4[x] = g.Vpos;
    }
    uint16_t t = (uint16_t)(g.temp1[x] - g.d7);
    g.temp1[x] = (int16_t)t;
    if (!(t & 0x8000)) { skateto(x, (uint16_t)g.temp3[x], (uint16_t)g.temp4[x], SYM_rtss, a3); return; }
    uint16_t anim = (uint16_t)SYM_SPAcelebrate;
    g.pfalock[x] = anim;
    if (2 * x == g.shotplayer) {
        anim = (uint16_t)SYM_SPApump2;
        g.shotplayer = 0xFFFF;
    }
    SetSPA(x, anim);
    g.temp1[x] = (int16_t)randoma8(0x78);
}

/* (asspenalty / assepen: from team a2's padsitnext on, round robin, the next pad moved off it for a
 * penalty (high byte set) whose team is a2; it goes back to the team (padside, cont1team); the
 * original loops for ever when there is none) */
static uint16_t pad_back(uint16_t a2)
{
    uint16_t y = r16(RAM_OFF_padsitnext + a2), s;
    for (;;) {
        y += 2;
        if (y == 0xA) y = 0;
        s = r16(RAM_OFF_padside + y);
        if (!(s & 0x8000)) continue;
        s &= 0xFF;
        if (s == a2) break;
    }
    w16(RAM_OFF_padside + y, s);
    w16(RAM_OFF_cont1team + y, (uint16_t)((s + 2) >> 1));
    return y;
}

/* (asspenalty's .9: with 4 pads or more on team a2 and as many skaters left or fewer, a pad sits
 * out (from padsitnext on; with 3 skaters first one comes back): its player loses it; then
 * player o, still with a pad, passes it to a teammate) */
static void pen_pads(int o, uint16_t a2)
{
    uint16_t d1 = r16(RAM_OFF_teampads + a2);
    if (d1 >= 4) {
        uint16_t x = r16(RAM_OFF_tmsort + a2), n = 0;
        for (int k = 0; k < 6; k++, x += 2) {
            uint16_t p = r16(RAM_OFF_position + x);
            if (!(p & 0x8000) && p) n++;
        }
        if (n <= d1) {
            if (n == 3) pad_back(a2);
            uint16_t y = r16(RAM_OFF_padsitnext + a2);
            while (r16(RAM_OFF_padside + y) != a2) {
                y += 2;
                if (y == 0xA) y = 0;
            }
            w16(RAM_OFF_padside + y, a2 | 0xFF00);
            w16(RAM_OFF_cont1team + y, 0);
            uint16_t ny = (uint16_t)(y + 2);
            if (ny == 0xA) ny = 0;
            w16(RAM_OFF_padsitnext + a2, ny);
            uint16_t px = r16(RAM_OFF_c1playernum + y);
            w16(RAM_OFF_c1playernum + y, 0xFFFF);
            w16(RAM_OFF_playerpad + px, 0xFFFF);
        }
    }
    o = g.curobj >> 1;
    if (!(g.playerpad[o] & 0x8000)) PassPadToTeammate(o, g.playerpad[o]);
}

/* Assignment $0B: player o (of team a2) leaves the penalty box. The first time: out of the box
 * count, still at its door (by his team), facing left in the wall animation, then the sprites
 * sorted. After: facing right, no requests, flags cleared, collisions on, skating out at -$1000;
 * with 4 pads or more on the team one comes back to it and he takes it; the assignment is over. */
void assepen(int o, uint16_t a2)
{
    if (g.pflags[o] & pfna) {
        g.pflags[o] &= 0xFFFD;
        g.PBnum = (uint16_t)(g.PBnum - (g.pfteam[o] ? 1 : 0x10));
        g.Xvel[o] = 0;
        g.Yvel[o] = 0;
        g.Ypos[o] = (int16_t)(g.pfteam[o] ? 0x3C : 0xFFC4);
        g.Xpos[o] = sideline;
        g.facedir[o] = 2;
        g.pfalock[o] = 2;
        SetSPAFromA(o, (uint16_t)SYM_SPAwallleft);
        SprSort();
        return;
    }
    g.facedir[o] = 4;
    g.newpnum[o] = -1;
    g.newpos[o] = -1;
    g.pflags[o] &= 0x6FF7;
    g.pfnc[o] = 0;
    g.Xvel[o] = (int16_t)0xF000;
    if (r16(RAM_OFF_teampads + a2) >= 4) {
        uint16_t y = pad_back(a2), x = g.curobj;
        w16(RAM_OFF_playerpad + x, y);
        w16(RAM_OFF_c1playernum + y, x);
    }
    assexit(g.curobj >> 1);
}

/* Assignment $0D: player o (team a2) skates to the penalty box. The first time: the pads (pen_pads,
 * setpads 0), unavailable and out of collisions, his box spot (PenBoxSpot) at the side boards the
 * target, a timeout of 900. Then he skates there (skateto, a3); close to the door (13 in y, 24 in
 * x) every 8 frames: the puck dropped, he glides in turning toward his stick side and slides at
 * $1000; at the door (8) facing it, or at the timeout, he is put in the box (wall animation, pf2pen
 * cleared) and the assignment becomes assdopen ($0E). */
void asspenalty(int o, uint16_t a2, int a3)
{
    if (g.pflags[o] & pfna) {
        pen_pads(o, a2);
        o = g.curobj >> 1;
        g.pflags[o] = (uint16_t)((g.pflags[o] & 0xFFF9) | 0x1000);
        setpads(o, 0);
        g.temp2[o] = 0;
        g.temp2b[o] = 8;
        g.temp4[o] = (int16_t)PenBoxSpot(o);
        g.temp3[o] = sideline;
        g.temp1[o] = 0;
        g.pflags[o] |= 0x8000;
        g.Wallcos[o] = 0;
        g.Wallsin[o] = 0;
        g.temp5[o] = 0x384;
    }
    g.temp5[o] = (int16_t)(g.temp5[o] - g.timestep);
    if (g.temp5[o] < 0) goto in_box;
    {
        uint16_t dy = (uint16_t)(g.Ypos[o] - g.temp4[o]);
        if (NEG(dy, 0xFFF3) || !NEG(dy, 0xD)) goto skate;
        uint16_t d0 = (uint16_t)(g.Xpos[o] - g.temp3[o]);
        if (NEG(d0, 0xFFE8)) goto skate;
        uint16_t t = (uint16_t)g.temp1[o];
        unsigned c = t >= g.d7;
        t = (uint16_t)(t - g.d7);
        g.temp1[o] = (int16_t)t;
        if (!(t & 0x8000)) return;
        g.temp1[o] = (int16_t)(t + 8 + c);                /* (adc: the carry of the sbc) */
        if (2 * o == g.puckc) {
            g.puckc = -1;
            g.nopuck[o] = 0x3C;
        }
        g.pfnc[o] = (uint16_t)SYM_SPAglide;
        SetSPAFromA(o, (uint16_t)SYM_SPAglide);
        uint16_t d2 = g.handed[o] ? 2 : 6;
        if (g.facedir[o] != d2) g.facedir[o] = (uint16_t)(g.facedir[o] + 1) & 7;
        g.Yvel[o] = 0;
        g.Xvel[o] = 0x1000;
        if (NEG(d0, 0xFFF8)) return;
        g.Xvel[o] = 0;
        if (g.facedir[o] != d2) return;
    }
in_box:
    g.Ypos[o] = g.temp4[o];
    g.Xpos[o] = sideline;
    g.facedir[o] = 2;
    g.pfalock[o] = 2;
    SetSPAFromA(o, (uint16_t)SYM_SPAwallright);
    g.pflags[o] &= 0xBFFF;
    assreplace(o, 0xE);
    return;
skate:
    skateto(o, (uint16_t)g.temp3[o], (uint16_t)g.temp4[o], SYM_rtss, a3);
}

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
    if (randomd0A(d4) >= 9) return 0;
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
    if (randomd0A(d0) >= 4 && randomd0A(d0) >= 4) return;
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
    g.temp3[o] = (int16_t)(randomas(m16(tab + d2 + 2)) + m16(tab + d2));
    g.temp4[o] = (int16_t)(randomas(m16(tab + d2 + 6)) + m16(tab + d2 + 4));
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
static int asswingo_start(int o, int a3, uint8_t def, int carrier, int spots)
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

/* Assignment 4: a wing on offense skates (avoiding the carrier) to a spot of the puck's zone (puck
 * y led by its velocity), picked again as the zone changes or at random when his timer runs out. */
void asswingo(int o, uint16_t a2, int a3)
{
    if (!asswingo_start(o, a3, 3, 1, 1)) return;
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
    if (!asswingo_start(o, a3, 5, 1, 1)) return;
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
    if (!asswingo_start(o, a3, 6, 0, 0)) return;
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
            EndPenaltyShotPlay(x);
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
    if (!(((uint16_t)g.puckvz & 0xFF00) | (uint16_t)g.puckz)) puckunflip();
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
        forcefade();
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
        Fprint(l0, SYM_linelist);
        g.printx += 4;
        Fprint(l1, SYM_linelist);
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
    d5 = (uint16_t)(randomas((uint16_t)(0x10 - g.passacc[x])) + d5);
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
            ax = (uint16_t)(randomas(d0) + ax);
            d0 = spread;
            if (d0 >= 0x3D) { d0 = 0x3C; spread = 0x3C; }
            if (ay & 0x8000) d0 >>= 1;
            ay = (uint16_t)(ay + randomd0s(d0));
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
        Fprint(d0, SYM_linelist);
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
    if (d3 & 0x1000) { startpause_pad(d4); return; }
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

/* arithmetic shift right by 4, at least 1 away from 0 (checkwindow's step toward the target) */
static uint16_t view_step(uint16_t a)
{
    a = (uint16_t)(a >> 4 | (a & 0x8000 ? 0xF000 : 0));
    return a ? a : 1;
}

/* a - step, at least 0 (the count downs by timestep and impactstep) */
static uint16_t count_down(uint16_t a, uint16_t step)
{
    a = (uint16_t)(a - step);
    return a & 0x8000 ? 0 : a;
}

/* (the jsl jmlvector of updateplayers: assignment addr, an asstab entry, for object o of team a2;
 * a3 = o) */
static void run_assignment(uint32_t addr, int o, uint16_t a2)
{
    switch (addr) {
    case SYM_rtss: rtss(); break;
    case SYM_assdefo: assdefo(o, a2, o); break;
    case SYM_assdefd: assdefd(o, a2, o); break;
    case SYM_asswingd: asswingd(o, a2, o); break;
    case SYM_asswingo: asswingo(o, a2, o); break;
    case SYM_asscenterd: asscenterd(o, o); break;
    case SYM_asscentero: asscentero(o, a2, o); break;
    case SYM_assgoalie: assgoalie(o, a2, o); break;
    case SYM_assscore: assscore(o, a2, o); break;
    case SYM_assstanley: assstanley(o); break;
    case SYM_asseben: asseben(o); break;
    case SYM_assepen: assepen(o, a2); break;
    case SYM_assbench: assbench(o, o); break;
    case SYM_asspenalty: asspenalty(o, a2, o); break;
    case SYM_assdopen: assdopen(o); break;
    case SYM_assgoalietopuck: assgoalietopuck(o, o); break;
    case SYM_assbreakaway: assbreakaway(o, o); break;
    case SYM_asspuckc: asspuckc(o, o); break;
    case SYM_assnearest: assnearest(o, a2, o); break;
    case SYM_assshoot: assshoot(o, o); break;
    case SYM_assdumpin: assdumpin(o, o); break;
    case SYM_asspassrec: asspassrec(o, a2, o); break;
    case SYM_assfaceoff: assfaceoff(o); break;
    case SYM_assfaceoffpl: assfaceoffpl(o); break;
    case SYM_pucknorm: pucknorm(o, o, r16(RAM_OFF_d0), r16(RAM_OFF_a0)); break;   /* (d0, a0: the direct page's) */
    case SYM_puckshadow: puckshadow(o); break;
    case SYM_puckunflip: puckunflip(); break;
    case SYM_puckfaceoff: puckfaceoff(o, o, r16(RAM_OFF_d0), r16(RAM_OFF_a0)); break;   /* (d0, a0: the direct page's) */
    case SYM_puckfaceoff2: puckfaceoff2(o, o); break;
    }
}

/* Lock the view at its current position */
void freezewindow(void)
{
    g.yc1 = g.Vpos;
    g.xc1 = g.Hpos;
    g.sflags |= sfslock;
}

/* Move the view so the screen follows the puck (or the lock position yc1 / xc1): the puck's y
 * plus its lead (yleader, toward the goal it is heading for), a sixteenth of the way a frame once
 * past a dead zone, within the rink's limits */
void checkwindow(void)
{
    uint16_t d2 = (uint16_t)g.yc1, d3 = (uint16_t)g.xc1;
    if (!(g.sflags & sfslock)) {
        uint16_t x = 0x1C;
        if (!(g.puckc & 0x8000)) {
            x = (uint16_t)g.puckc;
            if (r16(RAM_OFF_pfgoal + x)) {
                g.yleader = (uint16_t)(g.yleader + g.impactstep + cf);
                if (!((uint16_t)(g.yleader - 0x32) & 0x8000)) g.yleader = 0x32;
            } else {
                g.yleader = (uint16_t)(g.yleader - g.impactstep);
                if ((uint16_t)(g.yleader - 0xFFCF) & 0x8000) g.yleader = 0xFFCE;
            }
        }
        uint16_t v = r16(RAM_OFF_Yvel + x);
        v = (uint16_t)(v >> 7 | (v & 0x8000 ? 0xFE00 : 0));
        d2 = (uint16_t)(v + r16(RAM_OFF_Ypos + x) + g.yleader);
        g.yc1 = (int16_t)d2;
        d3 = r16(RAM_OFF_Xpos + x);
        g.xc1 = (int16_t)d3;
    }
    uint16_t d0 = (uint16_t)(d2 - g.Vpos), d1;
    int move = 1;
    if ((uint16_t)(d0 - 0xFFF7) & 0x8000) {
        d1 = (uint16_t)(d2 - 0xFFF6);
        if ((uint16_t)(d1 - 0xFF39) & 0x8000) d1 = 0xFF39;
    } else if ((uint16_t)(d0 - 0xB) & 0x8000) move = 0;
    else {
        d1 = (uint16_t)(d2 - 0xA);
        if (!((uint16_t)(d1 - 0x100) & 0x8000)) d1 = 0x100;
    }
    if (move) {
        uint16_t a = (uint16_t)(d1 - g.Vpos);
        if (a) g.Vpos = (int16_t)(view_step(a) + g.Vpos);
    }
    d0 = (uint16_t)(d3 - g.Hpos);
    move = 1;
    if ((uint16_t)(d0 - 0xFFD8) & 0x8000) {
        d1 = (uint16_t)(d3 - 0xFFD8);
        if ((uint16_t)(d1 - 0xFFC4) & 0x8000) d1 = 0xFFC4;
    } else if ((uint16_t)(d0 - 0x29) & 0x8000) move = 0;
    else {
        d1 = (uint16_t)(d3 - 0x28);
        if (!((uint16_t)(d1 - 0x3D) & 0x8000)) d1 = 0x3C;
    }
    if (move) {
        uint16_t a = (uint16_t)(d1 - g.Hpos);
        if (a) g.Hpos = (int16_t)(view_step(a) + g.Hpos);
    }
}

/* Step object o's sprite animation, looping or ending it: the frame list for its facing direction
 * (mirrored for flipped players, turned for the horizontal rink), the next frame when its count
 * runs out; at the end the animation is cleared (unless it loops, word $10) and the bench
 * animations take the player to the door */
void updateanim(int o)
{
    uint16_t spa = g.SPA[o];
    if (!spa) {
        g.pf2aip[o] = 0;
        g.pfalock[o] = 0;
        return;
    }
    uint32_t a0 = (SYM_SPAlist & 0xFF0000) | spa;
    uint16_t d1 = m16(a0 + 0x10), dir;
    if (g.sfhormode && o < 12)
        dir = g.attribute[o] & 0x0800 ? (uint16_t)(2 - g.facedir[o]) : (uint16_t)(g.facedir[o] - 2);
    else if (spa < (uint16_t)SYM_SPAbenchcheckr && (g.attribute[o] & 0x0800))
        dir = (uint16_t)-g.facedir[o];
    else
        dir = g.facedir[o];
    a0 = (a0 & 0xFF0000) | m16(a0 + (uint16_t)((dir & 7) << 1));
    uint16_t d0 = g.SPAnum[o];
    uint16_t d2 = m16(a0 + d0);
    if (!(g.SPAcnt[o] & 0x8000)) {
        g.SPAcnt[o] = (uint16_t)(g.SPAcnt[o] - g.timestep);
        if (!(g.SPAcnt[o] & 0x8000)) { g.frame[o] = d2; return; }
        uint16_t y = (uint16_t)(d0 + 2);
        g.SPAnum[o] = (uint16_t)(g.SPAnum[o] + 4);
        d0 = g.SPAnum[o];
        if (m16(a0 + y) & 0x8000) {
            d0 = 0;
            g.SPAnum[o] = 0;
            g.pf2aip[o] = 0;
            g.pfalock[o] = 0;
            if (spa < (uint16_t)SYM_SPAunused1 && spa >= (uint16_t)SYM_SPAbenchcheckr) {
                g.Xpos[o] = (int16_t)0xFF78;
                g.facedir[o] = 2;
                g.pfalock[o] = 2;
                SetSPAUpdate(o, (uint16_t)SYM_SPAwallright);
                return;
            }
            if (!d1) g.SPA[o] = 0;
        }
    }
    uint16_t c = m16(a0 + (uint16_t)(d0 + 2));
    if (c & 0x8000) c = (uint16_t)-c;
    g.SPAcnt[o] = c;
    g.frame[o] = d2;
}

/* Every object's frame: first (from the last object) the animation, the counts, the velocity decay
 * and the move by it (the puck's height with gravity and its bounce sound), the distance and
 * direction to the puck, the nearest player of each team, the icing waved off; the nets kept in
 * their ends; then (from the first) each player's pad input and assignment, goalies kept out of the
 * goal posts, collisions (players crossing the goal line beside the net put back) and the impact
 * decay (an absent object's impact = its position); the puck kept within the boards */
void updateplayers(void)
{
    g.topgoalie = 0xFFFF;
    g.botgoalie = 0xFFFF;
    g.tmnear[0] = g.tmnear[1] = 0xFFFF;
    g.tmnearpl[0] = g.tmnearpl[1] = 0xFFFF;
    g.puckoldz = g.puckz;
    for (uint16_t x = 0x1E; !(x & 0x8000); x = (uint16_t)(x - 2)) {
        int o = x >> 1;
        g.curobj = x;
        g.OldXpos[o] = g.Xpos[o];
        g.OldXfrac[o] = g.Xfrac[o];
        g.OldYpos[o] = (int16_t)r16(RAM_OFF_Ypos + x);
        g.OldYfrac[o] = g.Yfrac[o];
        if (x == netobj1 || x == netobj2) goto decay;
        if (g.position[o] & 0x8000) goto dist;
        if (!g.position[o]) {
            if (r16(RAM_OFF_Ypos + x) & 0x8000) g.botgoalie = x;
            else g.topgoalie = x;
        }
        updateanim(o);
        g.nopuck[o] = count_down(g.nopuck[o], g.timestep);
        g.nopuckb[o] = count_down(g.nopuckb[o], g.timestep);
        if (g.sflags3[0] & 0x0020) {
            uint16_t d0 = g.pnum[o] & 0xFF;
            if (d0 >= 0x80) d0 |= 0xFF00;
            if (!(d0 & 0x8000)) loadTeamStruct(o);
        }
        if (g.goaliedivewait[o]) g.goaliedivewait[o] = count_down(g.goaliedivewait[o], g.timestep);
        if (x == g.passReceiverPlayerNum && g.onetimer) goto move;
        if (g.SPA[o] == (uint16_t)SYM_SPAinjury1 || g.SPA[o] >= (uint16_t)SYM_SPAboardfront) goto dist;
        if (x == puckobj && g.puckz) goto move;
    decay:
        hal_cost_mark(SYM_updateplayers_9);
        uint16_t d2 = rom8(SYM_updateplayers_20 + g.timestep);
        if (g.pflags[o] & 1) d2 = rom8(SYM_updateplayers_22 - 1 + g.timestep);
        w16(RAM_OFF_Xvel + x, Mul16x8HiXS(r16(RAM_OFF_Xvel + x), d2));
        w16(RAM_OFF_Yvel + x, Mul16x8HiXS(r16(RAM_OFF_Yvel + x), d2));
    move:
        hal_cost_mark(SYM_updateplayers_10);
        uint16_t v = r16(RAM_OFF_Xvel + x);
        if (v) {
            uint32_t r2 = Mul16x8XS(v, g.velstep), s = (uint32_t)(uint16_t)r2 + g.Xfrac[o];
            g.Xfrac[o] = (uint16_t)s;
            g.Xpos[o] = (int16_t)(uint16_t)((r2 >> 16) + (uint16_t)g.Xpos[o] + (s >> 16));
        }
        hal_cost_mark(SYM_updateplayers_1xxx);
        v = r16(RAM_OFF_Yvel + x);
        if (v) {
            uint32_t r2 = Mul16x8XS(v, g.velstep), s = (uint32_t)(uint16_t)r2 + g.Yfrac[o];
            g.Yfrac[o] = (uint16_t)s;
            w16(RAM_OFF_Ypos + x, (uint16_t)((r2 >> 16) + r16(RAM_OFF_Ypos + x) + (s >> 16)));
        }
        hal_cost_mark(SYM_updateplayers_2xx);
        if (x == puckobj && !(g.puckz & 0x8000) && (g.puckz || g.puckvz)) {
            hal_cost_mark(SYM_updateplayers_3x);
            g.puckvz = (int16_t)(g.puckvz - g.gravstep);
            uint32_t r2 = Mul16x8XS((uint16_t)g.puckvz, g.velstep), s = (uint32_t)(uint16_t)r2 + g.puckzfrac;
            g.puckzfrac = (uint16_t)s;
            g.puckz = (int16_t)(uint16_t)((r2 >> 16) + (uint16_t)g.puckz + (s >> 16));
            if (g.puckz & 0x8000) {
                g.puckz = 0;
                g.puckzfrac = 0;
                uint16_t vz = (uint16_t)((uint16_t)-g.puckvz >> 1);
                g.puckvz = (int16_t)vz;
                if (!((uint16_t)(vz - 0x200) & 0x8000)) sfx(7, (uint16_t)(vz << 2), 0);
            }
        }
    dist:
        hal_cost_mark(SYM_updateplayers_11);
        uint16_t d0 = (uint16_t)(g.Xpos[14] - g.Xpos[o]), ax = abs16(d0);
        g.puckdist2[o] = Mul8U(ax >> 2, ax >> 2);
        uint16_t d1 = (uint16_t)(g.pucky[0] - r16(RAM_OFF_Ypos + x)), ay = abs16(d1);
        uint16_t sq = (uint16_t)(Mul8U(ay >> 2, ay >> 2) + g.puckdist2[o]);
        g.puckdist2[o] = sq;
        if (x < 0x18 && !(g.position[o] & 0x8000) && g.position[o] && !((g.pflags[o] & 0x1000) | g.pfalock[o])) {
            int t = x >= 0xC;
            if (sq < g.tmnear[t]) {
                g.tmnear[t] = sq;
                g.tmnearpl[t] = x;
            }
        }
        uint16_t len = ApproxLength_abs(ay, ax);
        g.puckdist[o] = len;
        if (x < 0x18 && len < 0x1E && (g.iflags & 4) && !(g.iflags & 1)
            && ((g.iflags & 2) ? !g.pfgoal[o] : g.pfgoal[o])
            && (uint16_t)g.puckz < 0xD && (uint16_t)g.puckvy[0] < 0x2400 && !((uint16_t)(g.puckvy[0] - 0xDC00) & 0x8000))
            g.iflags &= (uint16_t)~5;
        g.puckdir[o] = vtoa(d0, d1);
    }
    uint16_t y = r16(RAM_OFF_Ypos + netobj1);
    if (!((uint16_t)(y - 0x11F) & 0x8000)) y = 0x11F;
    w16(RAM_OFF_Ypos + netobj1, y);
    y = r16(RAM_OFF_Ypos + netobj2);
    if ((uint16_t)(y - 0xFED2) & 0x8000) y = 0xFED2;
    w16(RAM_OFF_Ypos + netobj2, y);
    for (int k = 0xC; k >= 0; k -= 2) w16(RAM_OFF_PadControlObj + k, 0xF);
    for (int k = 0; k < 5; k++) g.padinputdone[k] = 0;
    g.curobjteam = 0;
    g.curobjopp[0] = 2;
    for (uint16_t x = 0; x != 0x20; x += 2) {
        int o = x >> 1;
        g.curobj = x;
        if (x == 0xC) {
            g.curobjteam = 2;
            g.curobjopp[0] = 0;
        }
        g.PlayersUnused[0] = 0xFFFF;
        if (g.position[o] & 0x8000) {
            g.impact[o] = g.position[o];
            continue;
        }
        if (x == (uint16_t)g.puckc && !g.sfhormode) setpads(o, 0);
        if (x < 0x18 && !(g.playerpad[o] & 0x8000)) {
            uint16_t p = g.playerpad[o];
            if (!r16(RAM_OFF_padinputdone + p)) {
                w16(RAM_OFF_padinputdone + p, 1);
                doinput(o, r16(RAM_OFF_paddir + p), r16(RAM_OFF_padnew + p), r16(RAM_OFF_padchange + p), r16(RAM_OFF_JoyPad + p), (uint16_t)(p + 2), o);
            }
        }
        if (!g.pfalock[o]) {
            uint32_t t = SYM_asstab + (uint16_t)(g.asscur[o] << 2);
            w16(RAM_OFF_jmlvector + 1, rom16(t));
            w16(RAM_OFF_jmlvector + 2, rom16(t + 1));
            run_assignment(rom24(t), o, loadTeamStruct(o));
        }
        if (!((uint16_t)g.position[o] | g.gmclock) && x < 0x18 && x != (uint16_t)g.puckc && !(g.goalieflags[o] & 1)) {
            uint16_t a = (uint16_t)g.Xpos[o];
            if (!((uint16_t)(a - 0xFFD5) & 0x8000)) {
                if (!((uint16_t)(a - 0x2C) & 0x8000) && ((uint16_t)(a - 0x31) & 0x8000)) { a = 0x2B; w16(RAM_OFF_Xvel + x, 0); }
            } else if (!((uint16_t)(a - 0xFFCF) & 0x8000)) { a = 0xFFD5; w16(RAM_OFF_Xvel + x, 0); }
            g.Xpos[o] = (int16_t)a;
            a = r16(RAM_OFF_Ypos + x);
            if (!(a & 0x8000)) {
                if (((uint16_t)(a - 0xE5) & 0x8000) && !((uint16_t)(a - 0xE0) & 0x8000)) { w16(RAM_OFF_Ypos + x, 0xE5); w16(RAM_OFF_Yvel + x, 0); }
            } else if (!((uint16_t)(a - 0xFF1C) & 0x8000) && ((uint16_t)(a - 0xFF21) & 0x8000)) { w16(RAM_OFF_Ypos + x, 0xFF1B); w16(RAM_OFF_Yvel + x, 0); }
        }
        g.Wallcos[o] = 0;
        g.Wallsin[o] = 0;
        uint16_t d3 = r16(RAM_OFF_Ypos + x), d2 = (uint16_t)g.Xpos[o];
        if (d2 != (uint16_t)g.OldXpos[o] || d3 != (uint16_t)g.OldYpos[o]) {
            checkcoll(o, d2, d3, o);
            uint16_t xp = (uint16_t)g.Xpos[o];
            if (x < 0x18 && g.position[o] && !((g.pflags[o] & 0x8000) | g.sfhormode)
                && !((uint16_t)(xp - 0xFFE7) & 0x8000) && ((uint16_t)(xp - 0x19) & 0x8000)) {
                uint16_t ay = abs16(r16(RAM_OFF_Ypos + x)), r0 = (uint16_t)(ay - 0x10D);
                if ((((uint16_t)(abs16((uint16_t)g.OldYpos[o]) - 0x10D) ^ r0) & 0x8000) || (ay >= 0x103 && ay < 0x117)) {
                    g.Xpos[o] = g.OldXpos[o];
                    w16(RAM_OFF_Ypos + x, (uint16_t)g.OldYpos[o]);
                }
            }
        }
        g.impact[o] = (int16_t)count_down((uint16_t)g.impact[o], g.impactstep);
    }
    cf = 1;   /* (the carry of the last compare, which checkwindow adds) */
    if ((uint16_t)g.puckz < 0x18) {
        cf = (uint16_t)g.Xpos[14] >= 0xFF78;
        if ((uint16_t)(g.Xpos[14] - 0xFF78) & 0x8000) {
            g.Xpos[14] = (int16_t)0xFF78;
            cf = (uint16_t)g.frame[14] >= SPFpuck;
            if (g.frame[14] == SPFpuck) g.Xpos[15] = (int16_t)0xFF78;
        }
        cf = (uint16_t)g.Xpos[14] >= sideline;
        if (!((uint16_t)(g.Xpos[14] - sideline) & 0x8000)) {
            g.Xpos[14] = sideline;
            cf = (uint16_t)g.frame[14] >= SPFpuck;
            if (g.frame[14] == SPFpuck) g.Xpos[15] = sideline;
        }
    }
    if (!g.puckz) {
        uint16_t py = (uint16_t)g.pucky[0];
        cf = py >= 0xFEC2;
        if (!((uint16_t)(py - 0xFEC2) & 0x8000)) {
            cf = py >= 0x13E;
            if ((uint16_t)(py - 0x13E) & 0x8000) {
                cf = py >= 0xFEDB;
                if ((uint16_t)(py - 0xFEDB) & 0x8000) {
                    g.pucky[0] = (int16_t)0xFEDB;
                    g.pucky[1] = (int16_t)0xFEDB;
                }
                py = (uint16_t)g.pucky[0];
                cf = py >= 0x12A;
                if (!((uint16_t)(py - 0x12A) & 0x8000)) {
                    g.pucky[0] = 0x12A;
                    g.pucky[1] = 0x12A;
                }
            }
        }
    }
    if (!(g.puckc & 0x8000)) g.carriedy = (uint16_t)g.pucky[0];
}

/* Start sprite animation anim on object o (as SetSPAFromA) and update it at once */
void SetSPAUpdate(int o, uint16_t anim)
{
    if (anim == g.SPA[o]) return;
    g.SPA[o] = anim;
    g.SPAnum[o] = 0;
    g.SPAcnt[o] = 0xFFFF;
    updateanim(o);
}
