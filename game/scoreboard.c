/* scoreboard: the power play graphic, the scoreboard, the rink views, the intermission, the score
 * ticker and the highlights (src/routines/scoreboard). Cut from penalty.c as the asm is. */
#include "game.h"

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

/* Power play over: the EASN logo back */
void updatepwrplay_clr(void)
{
    uint16_t f = g.sflags;
    g.sflags &= (uint16_t)~sf2pwrplay;
    if (f & sf2pwrplay) EASNLogo();
}

/* a1 = string n of the string list a1 (each string starts with its length word) */
uint32_t AdvanceStringPtr(uint32_t a1, uint16_t n)
{
    while (!((n = (uint16_t)(n - 1)) & 0x8000))
        a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + m16(a1));
    return a1;
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

/* Does nothing */
void nullrtl89(void) {}

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

/* The scoreboard with both teams' penalty boxes */
void USBoard(void)
{
    printscores1();
    print(SYM_USBoardPos);
    DispPen(0);
    print(SYM_IntermissionPos);
    DispPen(2);
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

/* Between periods: both teams set up, the ticker started, the pause controller the first pad with a team
 * (none: 0), the scoreboard and the pause menu until it is left */
void Intermission(void)
{
    SetupTeamForIntermission();
    for (int y = 0x1E; y >= 0; y -= 2) w16(RAM_OFF_Xpos + y, 0);
    g.vcountmark[0] = g.Vcount[0];
    g.vcountmark[1] = g.Vcount[1];
    g.sflags &= (uint16_t)~sfpz;
    g.pausetimer = 0x0708;
    g.demoflag = 1;
    g.tickervc = (uint16_t)(g.Vcount[0] - 0x150);
    Intermission_ticker();
    g.tickerstart[0] = g.TickerNum;
    g.psfirst = 0;
    g.psitem = 0;
    g.sfpj = 0xFFFF;
    for (uint16_t y = 0; y < 0x0A; y += 2) {
        uint16_t t = r16(RAM_OFF_cont1team + y);
        if (t) { g.sfpj = (uint16_t)-t; break; }
    }
    if (!(g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0])) g.sfpj = 0;
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

/* In the intermission (demoflag): the next ticker score every $150 frames, until the ticker is back at its
 * first game: then the highlights. A = 0. */
uint16_t Intermission_tick(void)
{
    while (g.demoflag && (uint16_t)(g.Vcount[0] - g.tickervc) >= 0x150) {
        NewTicker2_next();
        if (g.tickerstart[0] == g.TickerNum) {
            DoHiLights();
            g.sflags3[0] &= (uint16_t)~0x0008;
            break;
        }
    }
    return IntermissionTick_done();
}

/* The intermission ticker's nothing to report: A = 0 */
uint16_t IntermissionTick_done(void) { return 0; }

void NewTicker2_next(void) { ticker_start(); }

/* The next ticker score once the clock is at 8:00 or less */
void NewTicker(void)
{
    if (g.gameclock[0] < 0x01E1) NewTicker2();
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
    g.tickervc = g.Vcount[0];
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

/* Back from the horizontal rink to the vertical one: a fade when the screen is lit, the sprites sorted,
 * the text map cleared and the scoreboard redrawn (not in a pause), the palette sent */
void ClrHor(void)
{
    if (g.brightness) {
        forcefade();
        WaitFadeOut();
        FadeIn();
        g.fadein = 0x0A;
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

/* The team of player o (a2: 0 home, 2 visitors); a1, the other team, is a2 ^ 2 */
uint16_t loadTeamStruct(int o) { return g.pfteam[o] ? 2 : 0; }

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

/* Clear the text screen and draw the horizontal rink's scoreboard background: the screen erased
 * between its two print positions, the two halves of the rink map (rinkmapbuf, columns 0 and $10
 * of row $55, 32 by 28) to the screen maps at $0400 and $0800, then USBoard. */
void SetHor_rink(void)
{
    print(SYM_StatsReportPos);
    eraser(0x20, 0x1D, g.blankchar);
    print(SYM_StatsReportPos2);
    g.MapVRAM = 0x400;
    MapPartToVRAM(SYM_rinkmapbuf, 0, 0, 0, 0x55, 0x20, 0x1C);
    g.MapVRAM = 0x800;
    MapPartToVRAM(SYM_rinkmapbuf, 0, 0, 0x10, 0x55, 0x20, 0x1C);
    USBoard();
}

/* Add amount to the team stat whose home word is at stat (the visitors' is the next word), for player o's team */
void ChkShotStat_add(int o, uint16_t stat, uint16_t amount)
{
    uint16_t y = (uint16_t)(stat + (g.pfteam[o] ? 2 : 0));
    w16(y, (uint16_t)(r16(y) + amount));
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

/* Give every player of team a2 full energy (the out-of-the-game check that follows can never pass) */
void reenergizeteam(uint16_t a2)
{
    for (int d0 = 0x32; d0 >= 0; d0 -= 2)
        g.tmpde[((a2 ? 0x34 : 0) + d0) >> 1] = 0x1000;
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
