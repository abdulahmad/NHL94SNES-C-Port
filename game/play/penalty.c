/* penalty: penalties, the penalty boxes, the scoreboard and team stats (src/routines/penalty). */
#include "game.h"

/* The intermission ticker's nothing to report: A = 0 */
uint16_t IntermissionTick_done(void) { return 0; }

/* Does nothing */
void nullrtl89(void) {}

/* a1 = string n of the string list a1 (each string starts with its length word) */
uint32_t AdvanceStringPtr(uint32_t a1, uint16_t n)
{
    while (!((n = (uint16_t)(n - 1)) & 0x8000))
        a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1));
    return a1;
}

/* The team of player o (a2: 0 home, 2 visitors); a1, the other team, is a2 ^ 2 */
uint16_t loadTeamStruct(int o) { return g.pfteam[o] ? 2 : 0; }

/* Add amount to the team stat whose home word is at stat (the visitors' is the next word), for player o's team */
void ChkShotStat_add(int o, uint16_t stat, uint16_t amount)
{
    uint16_t y = (uint16_t)(stat + (g.pfteam[o] ? 2 : 0));
    w16(y, (uint16_t)(r16(y) + amount));
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

/* Clear the list of called penalties */
void ClearPenaltyBuffer(void)
{
    for (int i = 0; i < 32; i++) w16(PenBuf + 2 * i, 0);
}

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

/* Give every player of team a2 full energy (the out-of-the-game check that follows can never pass) */
void reenergizeteam(uint16_t a2)
{
    for (int d0 = 0x32; d0 >= 0; d0 -= 2)
        g.tmpde[((a2 ? 0x34 : 0) + d0) >> 1] = 0x1000;
}

/* Add to the attack zone time of the team with the puck past a blue line */
void chkatop(void)
{
    uint16_t d1 = 0;
    if ((uint16_t)(g.pucky[0] - 0x58) & 0x8000) {
        d1 = 2;
        if (!((uint16_t)(g.pucky[0] - 0xFFA8) & 0x8000)) return;
    }
    if (g.gmode & gmdir) d1 ^= 2;
    g.tmATOP[d1 >> 1]++;
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

/* The average energy of team a2's players on the ice (d0; 0 when none) */
uint16_t AvgCline(uint16_t a2)
{
    uint16_t d0 = 0, d1 = 0, a0 = r16(RAM_OFF_tmsort + a2);
    for (int k = 0; k < 6; k++, a0 += 2) {
        uint16_t p = r16(RAM_OFF_position + a0);
        if (!p || (p & 0x8000)) continue;
        uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + ((r16(RAM_OFF_pnum + a0) & 0xFF) << 1));
        d0 = (uint16_t)(r16(RAM_OFF_tmpde + y) + d0);
        d1++;
    }
    return d1 ? Div16U(d0, d1) : d0;
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

/* The average energy of line d0 of team a2 (d0): the players of the line in priolist's order, as many as
 * the team has on the ice */
uint16_t getlinee(uint16_t a2, uint16_t d0)
{
    uint32_t a1 = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
    a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1 + 6));
    a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)(d0 << 3) + (uint16_t)a1 + (d0 >> 13 & 1));   /* (asl's carry) */
    uint16_t r1 = 0, r2 = 0, d4 = r16(RAM_OFF_tmap + a2);
    while (!((d4 = (uint16_t)(d4 - 1)) & 0x8000)) {
        uint16_t s = m8(SYM_priolist + d4);
        if (!s) continue;
        uint16_t d3 = (uint16_t)(m8(a1 + s) << 1);
        r1++;
        r2 = (uint16_t)(r16(RAM_OFF_tmpde - 2 + (uint16_t)((a2 ? 0x34 : 0) + d3)) + r2);
    }
    return Div16U(r2, r1);
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

/* If a shot was taken (sflags $1000, with the clock running), count it: team shots (by the shooter's
 * team), power play / shorthanded shots and shots by period (by his side: objects 6 and up are the
 * visitors), the shooter's shots (but a goalie number) and the other goalie's shots against; then excite
 * the crowd */
void ChkShotStat(void)
{
    uint16_t was = g.sflags & 0x1000;
    g.sflags &= ~0x1000;
    if (!was || g.gmclock) return;
    uint16_t x = g.shotplayer, a3 = x, a2 = 0, a1 = 2;
    if (!((uint16_t)(x - 0xC) & 0x8000)) { a2 = 2; a1 = 0; }
    uint16_t y = (uint16_t)(tmshots + (r16(RAM_OFF_pfteam + x) ? 2 : 0));
    w16(y, (uint16_t)(r16(y) + 1));
    int t = a2 >> 1;
    uint16_t d = (uint16_t)(g.tmap[t] - g.tmap[t ^ 1]);
    if (d) {
        if (d & 0x8000) g.tmSHShots[t]++;
        else g.tmPPShots[t]++;
    }
    x = (uint16_t)((g.gsp << 2) + a2);
    w16(RAM_OFF_tmPerShots + x, (uint16_t)(r16(RAM_OFF_tmPerShots + x) + 1));
    uint16_t idx = (uint16_t)((a2 ? 0x1A : 0) + r16(RAM_OFF_pnum + a3));
    if (idx != r16(RAM_OFF_tmgoalie + a2)) w8(RAM_OFF_tmpshots + idx, (uint8_t)(r8(RAM_OFF_tmpshots + idx) + 1));
    uint16_t gl = r16(RAM_OFF_tmgoalie + a1);
    if (!(gl & 0x8000)) {
        idx = (uint16_t)((a1 ? 0x1A : 0) + gl);
        w8(RAM_OFF_tmpshots + idx, (uint8_t)(r8(RAM_OFF_tmpshots + idx) + 1));
    }
    uint16_t c = (uint16_t)(g.CwdExciteLvl + 0xA);
    if (!(c & 0x8000)) g.CwdExciteLvl = c;
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

/* The end of the referee's work: unless a penalty shot is on, count both teams' players on the ice */
void InProgress_coin(void)
{
    if (g.penshot) return;
    InProgress_ap(0);
    InProgress_ap(2);
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

/* The penalties are over: end the penalty mode, the puck to its faceoff assignment, then InProgress_coin */
void InProgress_exit(void)
{
    g.gmode &= ~gmpen;
    assreplace(14, 0x1B);
    InProgress_coin();
}

/* Print string d0 of the string list a1 (each string starts with its length word); returns a1 past it */
uint32_t Fprint(uint16_t d0, uint32_t a1)
{
    for (; d0; d0--) a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1));
    return print(a1);
}

/* One ticker line at column px (NewTicker3's, on the stack): team d0's name, its score d1 at px + $17 */
uint32_t PrintGameInfo(uint16_t d0, uint16_t d1, uint16_t px)
{
    g.printx = px;
    g.printy++;
    print(GetTeamLongName(d0));
    g.printx = (uint16_t)(px + 0x17);
    return print(PushNumber(d1));
}

/* Print the label of the current period (overtime's one column to the left) */
uint32_t printscores1_per(void)
{
    uint16_t d0 = g.gsp;
    if (d0 == 4) { g.printx--; d0++; }
    uint32_t a0 = SYM_PerLabels, a1;
    do {
        a1 = a0;
        a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + m16(a0));
    } while (!((d0 = (uint16_t)(d0 - 1)) & 0x8000));
    return print(a1);
}

/* Print team d3's name (0 home), then move down two rows */
void printscores1_ps_name(uint16_t d3)
{
    uint16_t px = g.printx;
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a1 = d3 ? g.tmdata[1] : g.tmdata[0];
    uint32_t s = (uint32_t)m16((bank | a1) + 4) + a1;
    a1 = (uint16_t)s;
    a1 = (uint16_t)(m16(bank | a1) + a1 + (s >> 16));   /* (the first add's carry) */
    print(bank | a1);
    g.printx = px;
    g.printy += 2;
}

/* Clear the penalty message area on the horizontal rink (a 24 x 3 frame) */
void ClearPenaltyMessage(void)
{
    print(SYM_ClearPenaltyMessagePos);
    Framer(0x18, 3);
    print(SYM_ClearPenaltyMessagePos2);
}

/* Draw the energy bar of team a2's line d0: five bar characters by its energy (at most $1000); returns that
 * energy (left in d0) */
uint16_t linebar(uint16_t a2, uint16_t d0)
{
    d0 = getlinee(a2, d0);
    uint16_t pa = g.printa;
    g.printa = 0x3000;
    if (d0 >= 0x1000) d0 = 0x1000;
    uint16_t v = (uint16_t)((Mul16x8U(d0, 0x2F) >> 16) + 0x1D);
    for (int k = 2; k <= 6; k++) w16(RAM_OFF_mesarea + k, v);
    w16(RAM_OFF_mesarea, (uint16_t)((Mul16x8U(d0, 0x54) >> 16) + 2));
    print(RAM_OFF_mesarea);
    g.printa = pa;
    return d0;
}

/* One penalty box line (above row $0B): player d0 / 2 of team a2's number and his time left */
void pplpen(uint16_t a2, uint16_t d0)
{
    if (g.printy >= 0x0B) return;
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint32_t s = (uint32_t)(a2 ? 0x34 : 0) + d0;   /* (cmp's carry is clear) */
    uint16_t d2 = r16(RAM_OFF_tmpdst + (uint16_t)s) & 0x07FF;
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    uint16_t a1 = (uint16_t)(t + m16(bank | t) + (s >> 16 & 1));
    d0 >>= 1;
    do {
        uint32_t u = (uint32_t)m16(bank | a1) + a1;
        a1 = (uint16_t)(u + 8 + (u >> 16));
    } while (!((d0 = (uint16_t)(d0 - 1)) & 0x8000));
    a1 = (uint16_t)(a1 - 8);
    uint16_t px = g.printx;
    ConvertByteToDigits(m8(bank | a1), RAM_OFF_mesarea + 2);
    w16(RAM_OFF_mesarea, 4);
    print(RAM_OFF_mesarea);
    uint32_t tm = PushTime(d2);
    g.printx++;
    print(tm);
    g.printx = px;
    g.printy++;
}

/* Print team a2's (d3's for the name) name, then its score mod 100 as two characters */
uint32_t printscores1_ps(uint16_t a2, uint16_t d3)
{
    printscores1_ps_name(d3);
    uint16_t d0 = Mod100(r16(RAM_OFF_tmscore + a2));
    w16(RAM_OFF_mesarea, 4);
    w16(RAM_OFF_mesarea + 2, 0x0020);
    uint16_t q = Div16x8U(d0, 10);
    if (q) w16(RAM_OFF_mesarea + 2, (uint16_t)(q + 0x30));
    w16(RAM_OFF_mesarea + 3, (uint16_t)(d0 - q * 10 + 0x30));   /* (the divider's remainder) */
    return print(RAM_OFF_mesarea);
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

/* SetScore's goals for team d0 against team d1: the weights of d0's row (bits 4-6 of its team record's
 * +8 word) plus those of d1's row (bits 0-2) of the SetScore_5 table; one of the 4 picked */
static uint16_t setscore_goals(uint16_t d0, uint16_t d1)
{
    uint32_t tb = SYM_TeamList & 0xFF0000, sb = SYM_SetScore_5 & 0xFF0000;
    unsigned c = d0 >> 14 & 1;                                   /* (asl's carry) */
    uint16_t a1 = m16(SYM_TeamList + (uint16_t)(d0 << 2));
    uint32_t s = (uint32_t)a1 + m16((tb | a1) + 8) + c;
    a1 = (uint16_t)s;
    a1 = (uint16_t)(SYM_SetScore_5 + ((m16(tb | a1) & 0x70) >> 1));
    for (int k = 0; k < 4; k++) g.nibblebuffer[k] = m16((sb | a1) + 2 * k);
    c = d1 >> 14 & 1;
    a1 = m16(SYM_TeamList + (uint16_t)(d1 << 2));
    s = (uint32_t)a1 + m16((tb | a1) + 8) + c;
    a1 = (uint16_t)s;
    s = (uint32_t)(uint16_t)SYM_SetScore_5 + (uint16_t)((m16(tb | a1) & 7) << 3);
    a1 = (uint16_t)s;
    c = s >> 16 & 1;
    for (int k = 0; k < 4; k++) {
        s = (uint32_t)m16((sb | a1) + 2 * k) + g.nibblebuffer[k] + c;
        g.nibblebuffer[k] = (uint16_t)s;
        c = s >> 16 & 1;
    }
    return WeightedRandomSelect(4);
}

/* Play one period of playoff game a0 / 2 with random goals; after period 3 a game within a goal goes to
 * overtime (period 3 again, flag 2), or in period 4 a tie goes to team 1 */
void SetScore(uint16_t a0)
{
    uint16_t p = r16(RAM_OFF_gsper + a0);
    if (p >= 4) return;
    if (p == 3) {
        w16(RAM_OFF_gsper + a0, 5);
        uint16_t d = (uint16_t)(r16(RAM_OFF_gss1 + a0) - r16(RAM_OFF_gss2 + a0));
        if (d & 0x8000) d = (uint16_t)-d;
        if (d >= 2) return;
        if (g.gsp == 4) {
            uint16_t s1 = r16(RAM_OFF_gss1 + a0);
            if (s1 == r16(RAM_OFF_gss2 + a0)) w16(RAM_OFF_gss1 + a0, (uint16_t)(s1 + 1));
        } else {
            w16(RAM_OFF_gsper + a0, 3);
            w16(RAM_OFF_gsflags + a0, r16(RAM_OFF_gsflags + a0) | 2);
        }
        return;
    }
    w16(RAM_OFF_gsper + a0, (uint16_t)(p + 1));
    uint16_t n = setscore_goals(r16(RAM_OFF_gst1 + a0), r16(RAM_OFF_gst2 + a0));
    w16(RAM_OFF_gss1 + a0, (uint16_t)(r16(RAM_OFF_gss1 + a0) + n));
    n = setscore_goals(r16(RAM_OFF_gst2 + a0), r16(RAM_OFF_gst1 + a0));
    w16(RAM_OFF_gss2 + a0, (uint16_t)(r16(RAM_OFF_gss2 + a0) + n));
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

/* Update the other playoff games of the round: the ticker starts at the last; every game but the user's
 * loses its highlight flag (2) and, unless over (4), plays a period (SetScore) */
void UpdateScores(void)
{
    uint16_t d1, d2;
    GetShifter(&d1, &d2);
    g.TickerNum = d1;
    uint16_t a0 = (uint16_t)(d1 << 1);
    do {
        if (d1 != g.gamenum) {
            uint16_t f = r16(RAM_OFF_gsflags + a0) & 0xFFFD;
            w16(RAM_OFF_gsflags + a0, f);
            if (!(f & 4)) SetScore(a0);
        }
        a0 -= 2;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
}

/* Playoff rounds 0 and 1: the other games of the round not over get period 0 and 1 to 3 SetScore periods */
void InitScores(void)
{
    if (g.playoffRoundIdx >= 2) return;
    uint16_t d1, d2;
    GetShifter(&d1, &d2);
    uint16_t a0 = (uint16_t)(d1 << 1);
    do {
        if (d1 != g.gamenum && !(r16(RAM_OFF_gsflags + a0) & 4)) {
            w16(RAM_OFF_gsper + a0, 0);
            uint16_t d0 = (uint16_t)(randoma8(3) + 1);
            while (!((d0 = (uint16_t)(d0 - 1)) & 0x8000)) SetScore(a0);
        }
        a0 -= 2;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
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

/* Team a2 (a3 the other) for a period: full energy, line 0; with line changes on, a3 on line 3 when it has
 * more players on the ice than a2, line 5 when fewer */
static void setup_team(uint16_t a2, uint16_t a3)
{
    reenergizeteam(a2);
    w16(RAM_OFF_tmline + a3, 0);
    if (!g.OptLine) return;
    uint16_t d0 = (uint16_t)(r16(RAM_OFF_tmap + a3) - r16(RAM_OFF_tmap + a2));
    if (!d0) return;
    w16(RAM_OFF_tmline + a3, d0 & 0x8000 ? 5 : 3);
}

/* Before a period: reset the bench, newperiod, refill both teams' energy and choose their lines */
void SetupTeamForIntermission(void)
{
    ResetBench();
    g.newperiod = 1;
    setup_team(0, 2);
    setup_team(2, 0);
}

/* Paste the EASN logo into the rink's text map and widen the dirty range, unless a power play is on */
void EASNLogo(void)
{
    if (g.sflags & sf2pwrplay) return;
    print(SYM_EASNLogoPos);
    uint32_t p0 = SYM_EASNLogoMap, p1 = 0x7E0000 + RAM_OFF_textmap;
    uint32_t s = (uint32_t)0x2000 + g.EASNcset;
    uint16_t a = (uint16_t)(s + g.charofs + (s >> 16 & 1));   /* (the first add's carry) */
    PasteMap(&p0, &p1, a, 1, 0x19, 0x20);
    if (0x0321 < g.textdirtylo) g.textdirtylo = 0x0321;
    uint16_t hi = (uint16_t)((uint16_t)((uint16_t)(m16(SYM_EASNLogoMap + 2) + 0x19 - 1) << 5) + m16(SYM_EASNLogoMap));
    if (hi >= g.textdirtyhi) g.textdirtyhi = hi;
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

/* Power play over: the EASN logo back */
void updatepwrplay_clr(void)
{
    uint16_t f = g.sflags;
    g.sflags &= (uint16_t)~sf2pwrplay;
    if (f & sf2pwrplay) EASNLogo();
}


/* The power play box: the time left (the warning sound in its last seconds) and the team on the power play
 * (not in a shootout); the logo back when the teams are even */
void updatepwrplay(void)
{
    if (g.OptPlayMode == 4) return;
    uint16_t a2 = 0, a3 = 2;
    uint16_t d0 = (uint16_t)(g.tmap[0] - g.tmap[1]);
    if (!d0) { updatepwrplay_clr(); return; }
    if (d0 & 0x8000) {
        if (!(g.sflags & sf2pwrtm)) { updatepwrplay_clr(); g.sflags |= sf2pwrtm; }
    } else {
        a2 = 2; a3 = 0;
        if (g.sflags & sf2pwrtm) { updatepwrplay_clr(); g.sflags &= (uint16_t)~sf2pwrtm; }
    }
    uint16_t f = g.sflags;
    g.sflags |= sf2pwrplay;
    if (!(f & sf2pwrplay)) w16(RAM_OFF_tmPwrPlays + a3, (uint16_t)(r16(RAM_OFF_tmPwrPlays + a3) + 1));
    print(SYM_updatepwrplayPos);
    d0 = GetLowestPen(a2, a3);
    if (!g.gmclock && d0 < 5) {
        g.SfxVelocity = 0x7000;
        uint16_t y = QueueSound(5);
        if (d0 == 1 && r8(RAM_OFF_VoiceState + y) == 2) {
            w8(RAM_OFF_VoiceState + y, 3);
            y = (uint16_t)(y << 1);
            w16(RAM_OFF_VoicePitch + y, 0x0659);
            w16(RAM_OFF_recsfxpitch, 0x0659);
        }
    }
    print(PushTime(d0));
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a0 = r16(RAM_OFF_tmdata + a3);
    g.mesarea[0] = 2;
    appstring(SYM_updatepwrplayPos2, RAM_OFF_mesarea);
    uint32_t s = (uint32_t)a0 + m16((bank | a0) + 4);
    a0 = (uint16_t)s;
    a0 = (uint16_t)(a0 + m16(bank | a0) + (s >> 16 & 1));   /* (the first add's carry) */
    appstring(bank | a0, RAM_OFF_mesarea);
    print(RAM_OFF_mesarea);
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
    uint16_t t = g.pentimer;
    uint16_t v = (uint16_t)(t - g.d7);
    g.pentimer = v;
    if (!(v & 0x8000)) return;
    g.pentimer = (uint16_t)(v + 0x18 + (t >= g.d7));   /* (sbc's carry) */
    g.sflags3[0] |= 0x0020;
    chkatop();
    updatePPTeamTime();
    pentime_team(0);
    pentime_team(2);
}

/* The scoreboard: on the vertical rink the period box (the shootout round in a shootout; else the period,
 * the logo and the power play box) and, without the lower line change box, the score boxes; on the
 * horizontal rink the period and both scores in the big font */
void printscores1(void)
{
    if (g.sfhormode) {
        print(SYM_printscores1Pos8);
        printscores1_per();
        print(SYM_printscores1Pos9);
        uint32_t a1 = PushNumber(Mod100(g.tmscore[0]));
        AlignBigNumber(a1);
        printbig(a1);
        print(SYM_printscores1Pos10);
        a1 = PushNumber(Mod100(g.tmscore[1]));
        AlignBigNumber(a1);
        printbig(a1);
        return;
    }
    print(SYM_printscores1Pos);
    Framer(9, 5);
    if (g.OptPlayMode != 4) {
        print(SYM_printscores1Pos2);
        uint16_t d0 = g.gsp;
        if (d0 == 4) { g.printx--; d0++; }
        Fprint(d0, SYM_PerLabels);
        EASNLogo();
        updatepwrplay();
    } else {
        print(SYM_RoundTxt);
        g.printx = 6;
        uint16_t a;
        print(PushNumberWrap(g.shootoutround, 2, &a));
    }
    if (g.sflags3[0] & sf3llcs) return;
    print(SYM_printscores1Pos3);
    Framer(9, 5);
    print(SYM_printscores1Pos4);
    printscores1_ps(0, 0);
    print(SYM_printscores1Pos5);
    printscores1_ps(2, 1);
    print(SYM_printscores1Pos6);
    nullrtl89();
    print(SYM_printscores1Pos7);
    nullrtl89();
}


/* Back from the horizontal rink to the vertical one: a fade when the screen is lit, the sprites sorted,
 * the text map cleared and the scoreboard redrawn (not in a pause), the palette sent */
void ClrHor(void)
{
    if (g.brightness) {
        forcefade();
        WaitFadeOut();
        FadeIn();
        g.fadestep = 0x0A;
    }
    g.sfhormode = 0;
    g.Oldrow = 0x03E7;
    SprSort();
    if (!(g.sflags & sfpz)) {
        ClearTextMap();
        g.textdirtylo = 0;
        g.textdirtyhi = 0x03FF;
        printscores1();
    }
    DMAToCGRAM(0x7E0000 + RAM_OFF_palbuffer, 0x80, 0);
}

/* The scoreboard with both teams' penalty boxes */
void USBoard(void)
{
    printscores1();
    print(SYM_USBoardPos);
    DispPen(0);
    print(SYM_IntermissionPos);
    DispPen(2);
}

/* The horizontal rink (the scoreboard screen): the puck at half speed, the screen blanked, the scroll and
 * rows reset, the sprites sorted, the stats report drawn */
void SetHor(void)
{
    g.puckvx[0] = (int16_t)(g.puckvx[0] >> 1);
    g.puckvy[0] = (int16_t)(g.puckvy[0] >> 1);
    g.redrawblocks = 1;
    forceblack();
    g.sfhormode = 1;
    g.Hscroll = 0;
    g.Vscroll = -1;
    g.Oldrow = 0x2E;
    g.vramrow[0] = 0;
    SprSort();
    SetHor_rink();
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
    g.Pencntdwn = 0;
    g.gmode |= gmpen;
    SetPA(0x0A);
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
            uint16_t d = (uint16_t)(d1 - (uint16_t)g.Pencntdwn);
            if (d && !(d & 0x8000)) g.Pencntdwn = (int16_t)d1;
        }
        continue;
    delayed:
        if (g.gmode & 0x0008) continue;
        g.gmode |= 0x0008;
        SetPA(0x30);
    }
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
    assreplace(o, 0x0D);
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
    if (g.Pencntdwn < 0) {
        SeatPenaltyBox();
        if (g.RefCnt >= 0 || g.RefStep >= 0) return;
        chkprogress_find();
        return;
    }
    g.Pencntdwn = (int16_t)(g.Pencntdwn - g.d7);
    if (g.Pencntdwn >= 0) return;
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
    assinsert(14, 0x1A);
    g.temp1[14] = 0x1C20;
    g.RefStep = (int16_t)(g.RefStep | 0xFF00);
    g.RefCnt = 0;
    UpdatePA();
    g.RefCnt = 0x32;
    PushRef(0);
    FadeIn();
    ScreenOn();
}

/* Every game loop: the penalty box times, new calls, the referee */
void PenaltyManager(void)
{
    updatepentime();
    checkfornewpen();
    chkprogress();
    UpdatePA();
}

/* The ticker box for playoff game d3 (not one over or with a highlight pending: 0): a frame with the round,
 * the period and both teams with their scores */
static int ticker_box(uint16_t d3)
{
    uint16_t a0 = (uint16_t)(d3 << 1);
    if (r16(RAM_OFF_gsflags + a0) & 6) return 0;
    print(SYM_NewTicker3Pos);
    if (!(g.sflags & sfpz)) print(SYM_NewTicker3Pos2);
    if (!g.sfhormode) g.printy -= 5;
    Framer(0x1C, 5);
    g.tickervc = g.vcount[0];
    g.printx++;
    g.printy -= 4;
    uint16_t px = g.printx, d1, d2;
    GetShifter(&d1, &d2);
    g.printa = 0x2400;
    print(SYM_NewTicker3Pos3);
    g.printx = px;
    Fprint(g.OptPlayMode ? (uint16_t)(g.playoffRoundIdx + 1) : 0, SYM_GameLabels);
    g.printx = (uint16_t)(px + 0x15);
    Fprint((uint16_t)(r16(RAM_OFF_gsper + a0) - 1), SYM_PerLabels);
    g.printa = 0x2000;
    PrintGameInfo(r16(RAM_OFF_gst1 + a0), r16(RAM_OFF_gss1 + a0), px);
    PrintGameInfo(r16(RAM_OFF_gst2 + a0), r16(RAM_OFF_gss2 + a0), px);
    return 1;
}

/* The next ticker game (TickerNum, counting down; the user's game skipped) shown, while there is one */
void NewTicker2(void)
{
    for (;;) {
        uint16_t d3 = g.TickerNum;
        if (d3 & 0x8000) return;
        g.TickerNum--;
        if (d3 == g.gamenum) continue;
        NewTicker3(d3);                                    /* (bra NewTicker3) */
        return;
    }
}

/* The ticker box for game d3, or the next game when it is over or has a highlight pending */
void NewTicker3(uint16_t d3)
{
    if (!ticker_box(d3)) NewTicker2();
}

/* The next ticker score once the clock is at 8:00 or less */
void NewTicker(void)
{
    if (g.gameclock[0] < 0x01E1) NewTicker2();
}

/* The score ticker from the last game of the round when it has none (or it is at game 0 and so is the
 * user's game); then the next game */
static void ticker_start(void)
{
    if ((g.TickerNum & 0x8000) || !(g.TickerNum | g.gamenum)) {
        uint16_t d1, d2;
        GetShifter(&d1, &d2);
        g.TickerNum = d1;
        if (d1 & 0x8000) return;
    }
    NewTicker2();
}
void Intermission_ticker(void) { ticker_start(); }
void NewTicker2_next(void) { ticker_start(); }

/* Playoffs: the highlight of each game of the round still pending (and not played out) */
void DoHiLights(void)
{
    if (!g.OptPlayMode) return;
    uint16_t d1, d2;
    GetShifter(&d1, &d2);
    uint16_t a0 = (uint16_t)(d1 << 1);
    do {
        uint16_t f = r16(RAM_OFF_gsflags + a0);
        if (!(f & 4) && (f & 2)) {
            w16(RAM_OFF_gsflags + a0, f & 0xFFFD);
            SaveRegsStartHL2(a0, d1);
        }
        a0 -= 2;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
}

/* In the intermission (demoflag): the next ticker score every $150 frames, until the ticker is back at its
 * first game: then the highlights. A = 0. */
uint16_t Intermission_tick(void)
{
    while (g.demoflag && (uint16_t)(g.vcount[0] - g.tickervc) >= 0x150) {
        NewTicker2_next();
        if (g.tickerstart[0] == g.TickerNum) {
            DoHiLights();
            g.sflags3[0] &= (uint16_t)~0x0008;
            break;
        }
    }
    return IntermissionTick_done();
}

/* Between periods: both teams set up, the ticker started, the pause controller the first pad with a team
 * (none: 0), the scoreboard and the pause menu until it is left */
void Intermission(void)
{
    SetupTeamForIntermission();
    for (int y = 0x1E; y >= 0; y -= 2) w16(RAM_OFF_Xpos + y, 0);
    g.vcountmark[0] = g.vcount[0];
    g.vcountmark[1] = g.vcount[1];
    g.sflags &= (uint16_t)~sfpz;
    g.pausetimer = 0x0708;
    g.demoflag = 1;
    g.tickervc = (uint16_t)(g.vcount[0] - 0x150);
    Intermission_ticker();
    g.tickerstart[0] = g.TickerNum;
    g.psfirst = 0;
    g.psitem = 0;
    g.pausepad = 0xFFFF;
    for (uint16_t y = 0; y < 0x0A; y += 2) {
        uint16_t t = r16(RAM_OFF_cont1team + y);
        if (t) { g.pausepad = (uint16_t)-t; break; }
    }
    if (!(g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0])) g.pausepad = 0;
    g.InIntermission = 1;
    do {
        for (int y = 0x1E; y >= 0; y -= 2) w16(RAM_OFF_Xpos + y, 0);
        print((SYM_IntermissionPos & 0xFF0000) | (uint16_t)(SYM_IntermissionPos + 5));
        Framer(0x16, 7);
        printscores1();
    } while (Pausemode_menu());
    g.pausemenuon = 0;
    g.InIntermission = 0;
    g.demoflag = 0;
}
