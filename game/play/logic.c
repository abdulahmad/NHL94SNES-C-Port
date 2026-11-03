/* logic: the game's decisions (src/routines/logic). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define YPOS(o) r16(RAM_OFF_Ypos + 2 * (o))           /* (objects past the array: by address) */

/* a long pointer's byte or word, indexed as [p],y does (the carry goes into the bank) */
static uint32_t lp(uint32_t bank, uint16_t ptr, uint16_t y) { return ((bank << 16 | ptr) + y) & 0xFFFFFF; }

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
void pucknothing(void)
{
    if (g.SPAnum[14] >= 8 && g.SPAnum[14] < 0x18) g.facedir[14] ^= 2;
    g.facedir[14] |= 4;
    g.SPAnum[14] = 0;
    g.SPAcnt[14] = 0xFFFF;
}

/* The number of goalies of team a2: the nibbles of its goalie odds word (byte-swapped) until the
 * rest is zero. */
uint16_t GetGoalieCount(uint16_t a2)
{
    uint32_t bank = SYM_TeamList >> 16;
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    t = (uint16_t)(t + m16(lp(bank, t, 10)));          /* (GoalieOdds) */
    uint16_t a = m16(lp(bank, t, 0));
    a = (uint16_t)(a << 8 | a >> 8);
    uint16_t n = 0;
    do { n++; a = (uint16_t)(a << 4); } while (a);
    return n;
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
    uint16_t d0 = (uint16_t)(randomd0s((uint16_t)(rom16(SYM_assdumpin_6 + y) + d4)) + rom16(SYM_assdumpin_8 + y) - g.Xpos[14]);
    uint16_t d1 = (uint16_t)(randomd0s((uint16_t)(rom16(SYM_assdumpin_7 + y) + d4)) + rom16(SYM_assdumpin_ytab + y) - g.pucky[0]);
    uint16_t d2 = ApproxLength(d1, d0);
    uint16_t a = DivX8U((uint16_t)(d2 << 4), 0x78);
    if (a >= 0x60) a = 0x60;
    uint16_t lo = r16(RAM_OFF_position + g.curobj) ? 0x10 : 0x20;
    if (a < lo) a = lo;
    uint16_t d3 = a;
    g.puckvx[0] = (int16_t)(Div32S(Mul16x8XS(d0, d3), d2) << 8);
    g.puckvy[0] = (int16_t)(Div32S(Mul16x8XS(d1, d3), d2) << 8);
    g.puckvz = (int16_t)(randoma((uint16_t)(0x3C0 - d4)) + 0x10E0);
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
void EndPenaltyShot(int o)
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
