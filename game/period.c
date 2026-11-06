/* period: periods (src/routines/period). */
#include "game.h"

/* The game stats screen's braces round the rows shown (when rows are hidden above / below), the page sent;
 * then up / down scroll the rows (a row at a time, the IRQ split moving), Start leaves */
static void gamestats_loop(void)
{
    uint16_t d3;
mark:
    if (g.listtop[0]) {
        g.printx = 1;
        g.printy = (uint16_t)(g.listtop[0] << 1);
        g.printa = 0x2000;
        PrintToStatsPage(SYM_LineBraces);
    }
    if ((uint16_t)(g.listtop[0] + g.listrows[0]) != g.listcount[0]) {
        g.printx = 1;
        g.printy = (uint16_t)((uint16_t)(g.listtop[0] + g.listrows[0]) << 1);
        g.printa = 0x2000;
        PrintToStatsPage((SYM_LineBraces & 0xFF0000) | (uint16_t)(SYM_LineBraces + 3));
    }
    SendGameStatsPage();
    WaitFrame();
    for (;;) {
        WaitFrame();
        d3 = getpzjoy().held;
        if (d3 & 0x1000) return;
        if (d3 & 0x0800) goto up;
        if (d3 & 0x0400) goto down;
    }
up:
    ClearGameStatBraces();
    {
        uint16_t v = (uint16_t)(g.listtop[0] - 1);
        if (v & 0x8000) goto mark;
        g.listtop[0] = v;
    }
    g.listrows[0]++;
    SendGameStatsPage();
    g.listrows[0]--;
    WaitFrame();
    g.irqvscroll1 = 0x0D;
    do {
        WaitFrame();
        g.irqvscroll1 -= 2;
    } while (g.irqvscroll1 != 1);
    goto scrolled;
down:
    ClearGameStatBraces();
    if (!((uint16_t)(g.listtop[0] + g.listrows[0] - g.listcount[0]) & 0x8000)) goto mark;
    g.listrows[0]++;
    SendGameStatsPage();
    g.listrows[0]--;
    WaitFrame();
    g.listtop[0]++;
    g.irqvscroll1 = 1;
    do {
        WaitFrame();
        g.irqvscroll1 += 2;
    } while (g.irqvscroll1 != 0x0D);
scrolled:
    SendGameStatsPage();
    WaitFrame();
    g.irqvscroll1 = 0xFFFF;
    d3 = getpzjoy().held;
    if (d3 & 0x1000) return;
    if (d3 & 0x0800) goto up;
    if (!(d3 & 0x0400)) goto mark;
    goto down;
}

/* The game stats screen: both teams' blocks and the team stats (6 rows of 16, 11 without penalties, at a
 * time) */
void StatsReport(void)
{
    uint16_t a2 = r16(RAM_OFF_a2);   /* (as the caller left it) */
    g.setupvis = g.VisTeam[0];
    g.setuphome = g.HomeTeam;
    SetupScreen();
    PrintLetterSprites(SYM_GameStats, 0x50, 0x18);
    print(SYM_ShowGameStatsPos);
    DrawTeamBlocks(g.VisTeam[0], 0, a2);
    print(SYM_ShowGameStatsPos2);
    DrawTeamBlocks(g.HomeTeam, 0, a2);
    SpriteFrameEnd();
    w16(RAM_OFF_colourbuf + 0x42, 0xFFFF);
    w16(RAM_OFF_colourbuf + 0x22, 0x2BFF);
    SendColours();
    g.listtop[0] = 0;
    g.listrows[0] = 6;
    g.listcount[0] = 0x10;
    g.statpenrows = 0x10;
    if (!g.OptPen) {
        g.listcount[0] = 0x0B;
        g.statpenrows = 0;
    }
    g.listcount[0]--;
    SetupScrollIRQ(0x60, 0xC8, 0xD8);
    g.irqvscroll2 = 0x10;
    MenuScreenHDMA();
    ScreenOn();
    DisplayStats();
    SendTextMap();
    gamestats_loop();
}

/* Erase the braces around the visible game-stat rows / player-stat rows / other-scores rows */
void ClearGameStatBraces(void)
{
    clear_braces(1, (uint16_t)(g.listtop[0] << 1), (uint16_t)((uint16_t)(g.listtop[0] + g.listrows[0]) << 1));
}

/* Send the stats page rows shown (from listtop, listrows + 1 of them) to VRAM $0180 */
void SendGameStatsPage(void)
{
    uint16_t t = (uint16_t)(g.listtop[0] << 8 | g.listtop[0] >> 8);
    uint32_t p0 = (statspage & 0xFF0000) | (uint16_t)((t >> 1) + (uint16_t)statspage + (t & 1));   /* (lsr's carry) */
    uint16_t n = (uint16_t)(g.listrows[0] + 1);
    DMAToVRAM(&p0, (uint16_t)((n << 8 | n >> 8) >> 1), 0x0180, 0);
}

/* Game stats: one team's (a2) number of stat row curitem at printx (centred): a count, a ratio as a
 * percentage, a pair "a/b", or a time, from the TeamStatSources entry (penalty rows only with penalties) */
static void teamstat_value(uint16_t a2)
{
    uint32_t bank = SYM_TeamStatSources & 0xFF0000;
    uint16_t a0 = (uint16_t)SYM_TeamStatSources, d0 = g.curitem[0];
    uint16_t e;
    for (;;) {
        e = m16(bank | a0);
        if (e & 0x8000) return;
        if (((e & 0xF0) | g.statpenrows) && ((d0 = (uint16_t)(d0 - 1)) & 0x8000)) break;
        a0 = (uint16_t)(a0 + 6);
    }
    uint16_t type = e & 0x0F;
    a0 += 2;
    uint32_t a1;
    if (type == 1) {
        uint16_t s = m16(bank | a0);
        a0 += 2;
        a1 = PushNumber(r16((uint16_t)(s + a2)));
        uint16_t d1 = m16(bank | a0);
        g.mesarea[0] = 2;
        appstring(a1, RAM_OFF_mesarea);
        appstring(SYM_StatSlash, RAM_OFF_mesarea);
        appstring(PushNumber(r16((uint16_t)(a2 + d1))), RAM_OFF_mesarea);
        a1 = RAM_OFF_mesarea;
    } else if (type == 2) {
        uint16_t v0 = r16((uint16_t)(m16(bank | a0) + a2));
        a0 += 2;
        uint16_t v1 = r16((uint16_t)(m16(bank | a0) + a2));
        uint16_t p;
        if ((uint16_t)(v0 - v1) & 0x8000) p = v1 ? DivX8U(Mul8U(v0, 0x64), v1) : 0;
        else p = v0 ? 0x64 : 0;
        a1 = PushNumber(p);
        g.mesarea[0] = 2;
        appstring(a1, RAM_OFF_mesarea);
        appstring(SYM_StatPercent, RAM_OFF_mesarea);
        a1 = RAM_OFF_mesarea;
    } else if (type == 3) a1 = PushTime(r16((uint16_t)(m16(bank | a0) + a2)));
    else a1 = PushNumber(r16((uint16_t)(m16(bank | a0) + a2)));
    g.printx = (uint16_t)(g.printx - ((uint16_t)(m16(a1) + 1) >> 1));
    PrintToStatsPage(a1);
}

/* The game stats page: each team stat's name (the list without penalties when they are off) and both
 * teams' numbers */
void DisplayStats(void)
{
    MemFill(statspage, 0x4D80, (uint16_t)(g.smallfontchars + g.printfontset));
    g.printy = 0;
    g.curitem[0] = 0;
    uint16_t d4 = g.listcount[0];
    do {
        g.printa = 0x2800;
        g.printx = 9;
        uint32_t a1 = g.OptPen ? SYM_TeamStatNames : (SYM_TeamStatNames & 0xFF0000) | (uint16_t)SYM_TeamStatNamesNoPen;
        PrintToStatsPage(AdvanceStringPtr(a1, g.curitem[0]));
        g.printa = 0x2400;
        g.printx = 0x1C;
        teamstat_value(0);
        g.printx = 7;
        teamstat_value(2);
        g.printy += 2;
        g.curitem[0]++;
    } while (!((d4 = (uint16_t)(d4 - 1)) & 0x8000));
}

/* The team rating of team a2: byte 4 of its scouting report. */
uint16_t GetTeamRating(uint16_t a2)
{
    uint32_t bank = SYM_TeamList >> 16;
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    t = (uint16_t)(m16(((bank << 16 | t) + 8) & 0xFFFFFF) + t + 4);
    return m8(bank << 16 | t);
}

/* The team with more players on the ice gets one more tick of power play time */
void updatePPTeamTime(void)
{
    uint16_t d = (uint16_t)(g.tmap[0] - g.tmap[1]);
    if (!d) return;
    g.tmPwrPlayTime[d & 0x8000 ? 1 : 0]++;
}

/* The period stats screen: Y switches goals / shots, Start leaves */
void PeriodStatsScreen(void)
{
    g.setupvis = g.VisTeam[0];
    g.setuphome = g.HomeTeam;
    SetupScreen();
    SpriteFrameEnd();
    SendColours();
    g.listtop[0] = 0;
    g.listrows[0] = 6;
    g.curitem[0] = 0;
    MenuScreenHDMA();
    ScreenOn();
    DrawPeriodStats();
    for (;;) {
        SendTextMap();
        WaitFrame();
        ReadPads();
        Pad p = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x1000) return;
        if (d1 & 0x4000) {
            g.curitem[0] ^= 1;
            DrawPeriodStats();
        }
    }
}

/* One column of the period stats at printx: the period's label (none past lastperiod), the total's, then
 * both teams' goals (curitem 0) or shots in that period, and the title of what is shown */
static void period_column(uint16_t x, uint16_t px)
{
    g.printx = px;
    g.printa = 0x2400;
    g.printy = 0x0C;
    g.printx++;
    if (x == 4) {
        g.printx--;
        print(SYM_TotalShotsLine);
        g.printy++;
    } else {
        if (!((uint16_t)(x - g.lastperiod) & 0x8000) && x != g.lastperiod) return;
        print(AdvanceStringPtr(SYM_PerLabels, x));
    }
    uint16_t xo = (uint16_t)(x << 2), q;
    g.printa = 0x2000;
    uint32_t stat = g.curitem[0] ? RAM_OFF_tmPerShots : RAM_OFF_tmPerGoals;
    g.printx = px;
    g.printy = 0x0F;
    print(PushNumberWrap(r16(stat + 2 + xo), 3, &q));
    g.printx = px;
    g.printy = 0x14;
    print(PushNumberWrap(r16(stat + xo), 3, &q));
    if (g.curitem[0]) {
        g.printy++;
        g.printa = 0x2400;
        PrintLetterSprites((SYM_TotalShotsLine & 0xFF0000) | (uint16_t)(SYM_TotalShotsLine + 7), 0x68, 0x40);
        print((SYM_TotalShotsLine & 0xFF0000) | (uint16_t)(SYM_TotalShotsLine + 14));
    } else {
        PrintLetterSprites(SYM_GoalsLabel, 0x68, 0x40);
        print(SYM_YShotsTxt);
    }
}

/* The period stats page: the pictures, the title, both teams' blocks and a column per period and the total */
void DrawPeriodStats(void)
{
    uint16_t a2 = r16(RAM_OFF_a2);   /* (as the caller left it) */
    SpriteFrameStart();
    DrawMatchupPictures(a2);
    WaitDMAQueue();
    PrintLetterSprites(SYM_PeriodStats, 0x4C, 0x18);
    print(SYM_DrawPeriodStatsPos);
    eraser(0x20, 0x13, g.smallfontchars);
    print(SYM_DrawPeriodStatsPos2);
    DrawTeamBlocks(g.VisTeam[0], 0, a2);
    print(SYM_DrawPeriodStatsPos3);
    DrawTeamBlocks(g.HomeTeam, 0, a2);
    for (uint16_t x = 0; x < 5; x++) period_column(x, (uint16_t)(0x0A + (x << 2)));
    SpriteFrameEnd();
}

void Reset_Fast_ret(void) {}

/* Goalie control (manual or automatic): A / B / left / right switch it for the pause controller's team */
uint16_t ManualGoalieMenu(uint16_t d1)
{
    if (d1 & 0x8380) {
        uint16_t a2 = seta2();
        w16(RAM_OFF_goaliemode1 + a2, r16(RAM_OFF_goaliemode1 + a2) ^ 1);
    }
    return PauseMenuPoll();
}
