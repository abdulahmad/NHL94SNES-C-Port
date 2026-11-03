/* assign: the players' assignments (src/routines/assign). */
#include "game.h"

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
