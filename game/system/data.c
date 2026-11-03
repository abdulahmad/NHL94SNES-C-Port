/* data: passwords, playoff games and team data (src/routines/data). */
#include "game.h"

/* Clear the 8 words of the password bits */
void ClrPassBits(void) { for (int i = 0; i < 8; i++) g.passbits[i] = 0; }

/* The games of playoff round playoffRoundIdx less one (*d1) and its first win bit (*d2) */
void GetShifter(uint16_t *d1, uint16_t *d2)
{
    uint16_t b = 0xFFF0, n = 0x10, y = g.playoffRoundIdx;
    do { b = (uint16_t)(b + n); n >>= 1; } while (!(--y & 0x8000));
    *d1 = (uint16_t)(n - 1);
    *d2 = b;
}

/* The password bits (128) += d0 (clears the carry) */
void SuperAdd(uint16_t d0)
{
    uint32_t s = (uint32_t)g.passbits[0] + d0;
    g.passbits[0] = (uint16_t)s;
    if (s >> 16)
        for (int i = 1; i < 8; i++) if (++g.passbits[i]) break;
    cf = 0;
}

/* The password bits *= d0 (shift and add through mulres, $DD-$EC; clears the carry) */
void SuperMult(uint16_t d0)
{
    for (int x = 14; x >= 0; x -= 2) { w16(RAM_OFF_mulres + x, g.passbits[x >> 1]); g.passbits[x >> 1] = 0; }
    uint16_t t = d0;
    do {
        unsigned bit = t & 1;
        t >>= 1;
        if (bit) {
            unsigned c = 0;
            for (int i = 0; i < 8; i++) {
                uint32_t s = (uint32_t)g.passbits[i] + r16(RAM_OFF_mulres + 2 * i) + c;
                g.passbits[i] = (uint16_t)s;
                c = s >> 16;
            }
        }
        unsigned c = 0;
        for (int i = 0; i < 8; i++) {
            uint16_t w = r16(RAM_OFF_mulres + 2 * i);
            w16(RAM_OFF_mulres + 2 * i, (uint16_t)(w << 1 | c));
            c = w >> 15;
        }
    } while (t);
    cf = 0;
}

/* The password bits /= d0, the remainder returned (in d0). A byte divisor goes through the divider
 * a byte at a time from byte 14 (byte 15 only seeds the remainder, and a quotient over a byte
 * spills into the byte below); $200 and the others (as $4000) are shifts, and the latter clears the
 * byte after passbits too. (Clears the carry.) */
uint16_t SuperDiv(uint16_t d0)
{
    uint8_t *pb = (uint8_t *)g.passbits;
    cf = 0;
    if (d0 < 0x100) {
        uint8_t dv = (uint8_t)d0, r3 = pb[15], b = 0;
        for (int x = 14; x >= 0; x--) {
            uint16_t n = (uint16_t)(r3 << 8 | pb[x]), q, rem;
            if (dv) { q = (uint16_t)(n / dv); rem = (uint16_t)(n % dv); }
            else { q = 0xFFFF; rem = n; }
            unsigned s = b + (q & 0xFF);
            pb[x] = (uint8_t)s;
            b = (uint8_t)((q >> 8) + (s >> 8));
            r3 = (uint8_t)rem;
        }
        return (uint16_t)((d0 & 0xFF00) | r3);
    }
    if (d0 == 0x200) {
        uint16_t rem = g.passbits[0] & 0x1FF;
        unsigned c = 0;
        uint8_t prev = pb[15];
        for (int x = 14; x >= 0; x--) {
            uint8_t y = prev;
            prev = pb[x];
            pb[x] = (uint8_t)(c << 7 | y >> 1);
            c = y & 1;
        }
        pb[15] = 0;
        return rem;
    }
    uint16_t rem = g.passbits[0] & 0x3FFF;
    for (int x = 0; x < 14; x++) {
        uint16_t w = (uint16_t)(pb[x + 1] | pb[x + 2] << 8);
        pb[x] = (uint8_t)((uint16_t)(w << 2) >> 8);
    }
    w16(RAM_OFF_passbits + 15, 0);
    w16(RAM_OFF_passbits + 14, 0);
    return rem;
}

/* The default menu choices at power on: the 8 option words from OptPlayMode on, not a demo */
void DefaultMenus(void)
{
    g.notdemo = 0xFFFF;
    for (int y = 0; y < 16; y += 2) w16(RAM_OFF_OptPlayMode + y, rom16(SYM_DefaultMenus_1 + y));
}

/* mark team d0 taken in the bit sets (teamtaken nonzero if it was) */
static void igs_mark(uint16_t d0, uint16_t *d3)
{
    g.teamtaken[0] = 0;
    uint16_t a = (uint16_t)(d0 - 0x10);
    int hi = !(a & 0x8000);
    if (!hi) a = d0 & 0xF;
    g.teambit[0] = a >= 16 ? 0 : (uint16_t)(1u << a);
    if (d3[hi] & g.teambit[0]) g.teamtaken[0]++;
    d3[hi] |= g.teambit[0];
}

/* Random team pairs for the 8 other games (gst1 / gst2), none of them HomeTeam or VisTeam and no
 * team twice */
void InitializeGameStructures(void)
{
    uint16_t d3[2] = { 0, 0 };
    g.gamenum = 0xFFFF;
    igs_mark(g.HomeTeam, d3);
    igs_mark(r16(RAM_OFF_VisTeam), d3);
    for (int x = 0; x < 8; x++) {
        uint16_t t;
        do { t = randomd0(0x1A); igs_mark(t, d3); } while (g.teamtaken[0]);
        g.gst1[x] = t;
        do { t = randomd0(0x1A); igs_mark(t, d3); } while (g.teamtaken[0]);
        g.gst2[x] = t;
    }
}

/* Erase the goal box: its position string (boxPos) printed, then 19 x 9 blank tiles */
void box(void)
{
    print(SYM_boxPos);
    eraser(0x13, 9, g.blankchar);
}

/* The password bits = the password bits * d1 + d0 */
void PushBits(uint16_t d0, uint16_t d1)
{
    SuperMult(d1);
    SuperAdd(d0);
}

/* the next value of range n out of the password bits, added to the checksum in d7 (.sd) */
static uint16_t rpb_value(uint16_t n)
{
    uint16_t v = SuperDiv(n);
    g.d7 = (uint16_t)(g.d7 + v);
    return v;
}

/* The playoff state back out of the password bits (last written first): the series wins, WinBits,
 * pojoy, potreeteam, playoffRoundIdx, bosgames, postarts ($FFFF when the checksum is wrong). The
 * running sum is kept in d7. */
void ReadPassBits(void)
{
    uint16_t sum = SuperDiv(0x200);
    g.d7 = 0;
    for (int i = 7; i >= 0; i--) {
        g.gsflags[i] &= 0xFFFB;
        g.gspobwins[i] = rpb_value(5);
        if (g.gspobwins[i] == 4) g.gsflags[i] |= 4;
        g.gspotwins[i] = rpb_value(5);
        if (g.gspotwins[i] == 4) g.gsflags[i] |= 4;
    }
    g.WinBits = rpb_value(0x4000);
    g.pojoy = rpb_value(4);
    g.potreeteam = rpb_value(0x10);
    g.playoffRoundIdx = rpb_value(4);
    g.bosgames = rpb_value(8);
    g.postarts = rpb_value(0x20);
    if (sum != (g.d7 & 0x1FF)) g.postarts = 0xFFFF;
}

/* value d0 of range d1 into the password bits, added to the checksum in d7 (.t1) */
static void wpb_value(uint16_t d0, uint16_t d1)
{
    g.d7 = (uint16_t)(g.d7 + d0);
    PushBits(d0, d1);
}

/* The playoff state into the password bits, with a checksum (the running sum kept in d7) */
void WritePassBits(void)
{
    ClrPassBits();
    g.d7 = 0;
    wpb_value(g.postarts, 0x20);
    wpb_value(g.bosgames, 8);
    wpb_value(g.playoffRoundIdx, 4);
    wpb_value(g.potreeteam, 0x10);
    wpb_value(g.pojoy, 4);
    wpb_value(g.WinBits, 0x4000);
    for (int i = 0; i < 8; i++) {
        wpb_value(g.gspotwins[i], 5);
        wpb_value(g.gspobwins[i], 5);
    }
    PushBits(g.d7 & 0x1FF, 0x200);
}

/* Which team each controller plays: in the playoffs the series of the user's team gives HomeTeam,
 * VisTeam, OptTeam2 and gamenum and the pads in use their team (the same table word for each); in
 * the demo, play modes 0 and 4 only the other games */
void FigureJoy(void)
{
    if (!g.notdemo || !g.OptPlayMode || g.OptPlayMode == 4) { InitializeGameStructures(); return; }
    uint16_t d1, d2;
    GetShifter(&d1, &d2);
    d2 = r8(RAM_OFF_potree + g.potreeteam);
    uint16_t x = (uint16_t)(d1 << 1), d0;
    g.gamenum = 0xFFFF;
    for (;;) {
        if (d2 == r16(RAM_OFF_gst1 + x)) { d0 = 0; g.OptTeam2 = r16(RAM_OFF_gst2 + x); break; }
        if (d2 == r16(RAM_OFF_gst2 + x)) { d0 = 4; g.OptTeam2 = r16(RAM_OFF_gst1 + x); break; }
        x -= 2;
        if (--d1 & 0x8000) return;
    }
    d0 = (uint16_t)(d0 + g.pojoy);
    g.gamenum = d1;
    g.HomeTeam = r16(RAM_OFF_gst1 + x);
    w16(RAM_OFF_VisTeam, r16(RAM_OFF_gst2 + x));
    d0 = (uint16_t)(d0 << 2);
    for (int k = 0; k < 10; k += 2)
        if (r16(RAM_OFF_cont1team + k)) w16(RAM_OFF_cont1team + k, rom16(SYM_FigureJoy_5 + d0));
}

/* Build the playoff tree: the 16 seats of tree postarts into potree, the winner of each pair (by
 * WinBits) after them, then this round's series (gst1 / gst2, home first in the 2-3-2 rounds of
 * bosgames 2, 3 and 5) with their scores cleared, and FigureJoy */
void maketree(void)
{
    uint32_t bank = 0x7E0000;
    uint16_t a1 = (uint16_t)(SYM_playoffseats + (uint16_t)(g.postarts << 4));
    uint32_t sb = SYM_playoffseats & 0xFF0000;
    for (int y = 14; y >= 0; y -= 2) w16(RAM_OFF_potree + y, m16(((sb | a1) + y) & 0xFFFFFF));
    uint16_t a0 = RAM_OFF_potree, d0 = g.WinBits;
    a1 = 0x34E2;
    for (int i = 0; i < 15; i++) {
        w8(a1, m8(bank | (uint16_t)(a0 + (d0 & 1))));
        a1++; a0 += 2;
        d0 >>= 1;
    }
    uint16_t d1, d2;
    GetShifter(&d1, &d2);
    if (d1 & 0x8000) return;
    a0 = (uint16_t)((uint16_t)(d2 << 1) + RAM_OFF_potree + (d2 >> 15));
    for (int x = 0; !(d1 & 0x8000); x++, d1--) {
        g.gss1[x] = g.gss2[x] = g.gsper[x] = 0;
        g.gsflags[x] &= 0xFFFD;
        uint16_t b = g.bosgames;
        uint8_t first = m8(bank | a0), second = m8(bank | (uint16_t)(a0 + 1));
        a0 += 2;
        if (b == 2 || b == 3 || b == 5) { g.gsflags[x] &= 0xFFFE; g.gst1[x] = first; g.gst2[x] = second; }
        else { g.gsflags[x] |= 1; g.gst2[x] = first; g.gst1[x] = second; }
    }
    FigureJoy();
}

/* New playoffs with team 1 (25 at most): a random tree (from the next of a random number on) that
 * holds the team, its seat potreeteam and the tree postarts, the first round; bosgames 7 for
 * single games (play mode 2), else 0 and the series wins cleared; then the tree */
void NewPO(void)
{
    uint16_t d0 = RandomWord() & 0x1F, d1;
    for (;;) {
        d0 = (d0 + 1) & 0x1F;
        uint16_t a0 = (uint16_t)(SYM_playoffseats + (uint16_t)(d0 << 4));
        uint16_t t = g.OptTeam1 >= 0x1A ? 0x19 : g.OptTeam1;
        int found = 0;
        for (d1 = 0xF; !(d1 & 0x8000); d1--) {
            uint8_t s = m8((SYM_playoffseats & 0xFF0000) | a0);
            a0++;
            if (s == t) { found = 1; break; }
        }
        if (found) break;
    }
    g.potreeteam = d1 ^ 0xF;
    g.postarts = d0;
    g.playoffRoundIdx = 0;
    g.bosgames = 7;
    if (g.OptPlayMode != 2) {
        g.bosgames = 0;
        for (int i = 0; i < 8; i++) g.gspotwins[i] = g.gspobwins[i] = 0;
    }
    maketree();
}

/* the bit of win bit k (BitMaskTable) */
static uint16_t bitmask(uint16_t k) { return rom16(SYM_BitMaskTable + (uint16_t)(k << 1)); }

/* the round's winners d3 (a bit a series, set for the second team) into WinBits at the round's
 * first bit d2, and the next round */
static void rg_winbits(uint16_t d2, uint16_t d3)
{
    uint16_t mask = bitmask(d2);
    g.WinBits &= (uint16_t)(mask - 1);
    g.WinBits |= (uint16_t)Mul16U(d3, mask);
    g.playoffRoundIdx++;
}

/* Record the played playoff game (gamenum, the score tmscore) and the series wins: single games
 * (bosgames 7) decide the round at once; in best of seven the round ends after the seventh game
 * or, before round 3, once the user's series is decided (the other series played out at random
 * and the password bits of the moment kept in tpassbits). The round's winners go to WinBits. */
void ResolveGames(void)
{
    uint16_t x = (uint16_t)(g.gamenum << 1);
    w16(RAM_OFF_gss1 + x, g.tmscore[0]);
    w16(RAM_OFF_gss2 + x, g.tmscore[1]);
    uint16_t d1, d2, d3 = 0;
    GetShifter(&d1, &d2);
    if (g.bosgames == 7) {
        for (uint16_t n = d1; !(n & 0x8000); n--) {
            uint16_t a0 = (uint16_t)(n << 1);
            if (r16(RAM_OFF_gss2 + a0) >= r16(RAM_OFF_gss1 + a0)) d3 |= bitmask(n);
            if (r16(RAM_OFF_gsflags + a0) & 1) d3 ^= bitmask(n);
        }
        rg_winbits(d2, d3);
        return;
    }
    for (uint16_t n = d1; !(n & 0x8000); n--) {
        uint16_t a0 = (uint16_t)(n << 1);
        if (r16(RAM_OFF_gspotwins + a0) == 4 || r16(RAM_OFF_gspobwins + a0) == 4) continue;
        uint16_t side = r16(RAM_OFF_gsflags + a0) & 1 ? 0x10 : 0;
        if ((uint16_t)(r16(RAM_OFF_gss1 + a0) - r16(RAM_OFF_gss2 + a0)) & 0x8000) side ^= 0x10;
        uint16_t k = (uint16_t)(a0 + side);
        w16(RAM_OFF_gspotwins + k, (uint16_t)(r16(RAM_OFF_gspotwins + k) + 1));
    }
    g.bosgames++;
    if (g.bosgames != 7) {
        x = (uint16_t)(g.gamenum << 1);
        if (r16(RAM_OFF_gspotwins + x) != 4 && r16(RAM_OFF_gspobwins + x) != 4) return;
        if ((int16_t)g.playoffRoundIdx < 3) {
            GetShifter(&d1, &d2);
            for (uint16_t n = d1; !(n & 0x8000); n--) {
                uint16_t a0 = (uint16_t)(n << 1);
                if (n == g.gamenum) continue;
                for (;;) {
                    if (r16(RAM_OFF_gspotwins + a0) == 4 || r16(RAM_OFF_gspobwins + a0) == 4) break;
                    w16(RAM_OFF_gspotwins + a0, (uint16_t)(r16(RAM_OFF_gspotwins + a0) + 1));
                    if (!(RandomWord() & 1)) continue;
                    w16(RAM_OFF_gspotwins + a0, (uint16_t)(r16(RAM_OFF_gspotwins + a0) - 1));
                    w16(RAM_OFF_gspobwins + a0, (uint16_t)(r16(RAM_OFF_gspobwins + a0) + 1));
                }
            }
            WritePassBits();
            for (int i = 0; i < 8; i++) g.tpassbits[i] = g.passbits[i];
            g.sflags3[0] |= 4;
        }
    }
    g.bosgames = 0;
    GetShifter(&d1, &d2);
    d3 = 0;
    for (uint16_t n = d1; !(n & 0x8000); n--) {
        uint16_t a0 = (uint16_t)(n << 1);
        if (r16(RAM_OFF_gspotwins + a0) != 4) d3 |= bitmask(n);
        w16(RAM_OFF_gspotwins + a0, 0);
        w16(RAM_OFF_gspobwins + a0, 0);
    }
    rg_winbits(d2, d3);
}

/* mesarea = the number and name of player TempPlOffset (bit 15 set for the visitors); a1 = mesarea */
uint32_t GetPlayerName(void)
{
    uint16_t d0 = g.TempPlOffset, a2 = 0;
    if (d0 & 0x8000) { d0 &= 0xFF; a2 = 2; }
    return getname(a2, d0);
}

/* mesarea = the number, first initial and last name of player TempPlOffset; a1 past it */
uint32_t GetPlayerNumInitial(void)
{
    uint16_t d0 = g.TempPlOffset, a2 = 0;
    if (d0 & 0x8000) { d0 &= 0xFF; a2 = 2; }
    return FormatPlayerNumInitial(a2, d0);
}

/* After a playoff game: work out the winners and the new password and tree; when the user won it
 * all or is out (a one-player first round swaps to the other team instead) the password is hidden
 * and the play mode is 2 (3 unless the rounds are single games) */
void EncodePW(void)
{
    if (!g.OptPlayMode) return;
    g.OptPlayMode = 1;
    ResolveGames();
    maketree();
    if (g.playoffRoundIdx == 4) {
        ResetPassWord();
        goto out;
    }
    if (g.gamenum & 0x8000) {
        if (((uint16_t)(g.pojoy - 2) & 0x8000) || g.playoffRoundIdx != 1) goto out;
        g.potreeteam ^= 1;
        g.pojoy ^= 1;
    }
    WritePassBits();
    BitstoPW();
    if (g.sflags3[0] & 0x0004) {
        for (int x = 7; x >= 0; x--) g.passbits[x] = g.tpassbits[x];
        ReadPassBits();
    }
    maketree();
    return;
out:
    w16(RAM_OFF_PassWord, (uint16_t)(r16(RAM_OFF_PassWord) | 0x80));
    g.OptPlayMode = 2;
    if (g.bosgames != 7) g.OptPlayMode = 3;
}

/* The injury box: its frame, the injured player (TempPlOffset) and whether he is out for the period
 * or the game (bit 14, cleared here) */
void ShowInjuryBox(void)
{
    print(SYM_ShowInjuryBoxPos);
    Framer(0x12, 5);
    print(SYM_InjuryToOutForPeriodTxt);
    if (g.TempPlOffset & 0x4000) {
        g.TempPlOffset &= 0xBFFF;
        print(SYM_TheGameTxt);
    }
    print(GetPlayerNumInitial());
}


/* The goal box: both line change boxes closed, then the last goal of the goal log (ScoreSum): the
 * frame (taller with each assist), "GOAL" (on a skater's third goal "HAT TRICK", with the hats for
 * the home team and a longer stoppage), the scorer and the assists */
void ShowGoalBox(void)
{
    g.lcpad[0] = 0xFFFF;
    g.lcpad[1] = 0xFFFF;
    lcfound2(0);
    lcfound2(2);
    uint32_t a4 = (ScoreSum & 0xFF0000) | (uint16_t)((uint16_t)ScoreSum - 6 + g.ScoreSumbytes[0]);
    print(SYM_ShowGoalBoxPos);
    uint16_t d1 = 5, v = m16(a4 + 4);
    if (!(v & 0x0080)) {
        d1 += 2;
        if (!(v & 0x8000)) d1++;
    }
    Framer(0x13, d1);
    uint16_t a2 = m16(a4 + 2) & 0x0080 ? 2 : 0;
    uint32_t a1 = SYM_GoalBoxText;
    uint16_t y = m8(a4 + 3);
    if (a2) y = (uint16_t)(y + 0x1A);
    if (r8(RAM_OFF_tmgoals + y) == 3 && !((uint16_t)(m8(a4 + 3) - GoalieCount(a2)) & 0x8000)) {
        a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)a1 + m16(a1));
        if (!a2) {
            StartHatTrick();
            g.Pencntdwn = (int16_t)(g.Pencntdwn + 0xA);
        }
    }
    printbig(a1);
    print(FormatPlayerNumInitial(a2, m8(a4 + 3)));
    uint16_t d0 = m8(a4 + 4);
    if (d0 & 0x80) return;
    print(SYM_AssistByTxt);
    print(FormatPlayerNumInitial(a2, d0));
    d0 = m8(a4 + 5);
    if (d0 & 0x80) return;
    print(SYM_ShowGoalBoxPos2);
    print(FormatPlayerNumInitial(a2, d0));
}
