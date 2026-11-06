/* stats: the roster and line screens (src/routines/stats). */
#include "game.h"

/* (SetupScreen: a long pointer to WRAM offset off) */
#define WRAM_PTR(off) (0x7E0000u + (off))

/* team a2's data (a team offset: 0 home, 2 visitors) in the cartridge */
static uint32_t team_data(uint16_t a2) { return (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2); }

/* the byte of team a2's data that counts its forwards and defensemen (team data byte 3 past the
 * offset at +8) */
static uint16_t team_counts(uint16_t a2)
{
    uint32_t a3 = team_data(a2);
    uint16_t a = (uint16_t)((uint16_t)(a3 + m16((a3 + 8) & 0xFFFFFF)) + 3);
    return m16((a3 & 0xFF0000) | a);
}

/* the rating column d1 (AttribMenuSkater, or AttribMenuGoalie for a goalie: curplayer below the
 * goalie count) of player curplayer of team a2 through CalcAttrib */
static uint16_t player_rating(uint16_t d1, uint16_t a2, uint16_t hotcold)
{
    uint32_t a1 = SYM_AttribMenuSkater;
    uint16_t r2 = 0;
    if ((uint16_t)(g.curplayer[0] - GetGoalieCount(a2)) & 0x8000) {
        a1 = SYM_AttribMenuGoalie;
        r2 = 0x10;
    }
    for (uint16_t x = (uint16_t)(d1 - 1); !(x & 0x8000); x--) {
        uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
        a1 = (a1 & 0xFF0000) | (uint16_t)((s & 0xFFFF) + 4 + (s >> 16));
    }
    a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)a1 + m16(a1));
    uint16_t mask = m16(a1);
    uint16_t n;
    g.usehotcold = hotcold;
    return CalcAttrib(g.curplayer[0], a2, mask, r2, &n);
}

/* (SetLineSlot's .5: swap the line slots at a3 and a3 + 1) */
static void swap_next_slot(uint32_t a3)
{
    g.lineswap[0] = ReadLineSlot(a3);
    WriteLineSlot(a3, ReadLineSlot(a3 + 1));
    WriteLineSlot(a3 + 1, g.lineswap[0]);
}

/* (SelectLinePlayer's .10: the column title (DispAttribCtr of SkaterAttribColumns, AttribMenuGoalie
 * for the goalies, stepped back past the end), "PLAYER", the arrows when the list scrolls, and the
 * list of team a2) */
static void line_list_redraw(uint16_t a2)
{
    print(SYM_SelectLinePlayerPos2);
    eraser(0x20, 0xD, g.blankchar);
    g.printx = 0x11;
    g.printy = 2;
    uint32_t a1;
    for (;;) {
        a1 = g.SelectedPlayerIdx ? SYM_SkaterAttribColumns : SYM_AttribMenuGoalie;
        uint16_t d0 = g.DispAttribCtr;
        while (!(m16(a1) & 0x8000) && !(--d0 & 0x8000)) {
            uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
            a1 = (a1 & 0xFF0000) | (uint16_t)((s & 0xFFFF) + 4 + (s >> 16));
        }
        if (!(m16(a1) & 0x8000)) break;
        g.DispAttribCtr--;
    }
    g.printa = 0x3800;
    a1 = print(a1);
    uint16_t mask = m16(a1), d4 = m16(a1 + 2);
    print(SYM_PlayerTxt);
    if (g.picktop[0] != g.pickfirst[0]) print(SYM_LeftArrowTxt);
    if ((uint16_t)(g.picktop[0] - g.pickfirst[0] + g.pickrows[0] - g.listcount[0]) & 0x8000) print(SYM_RightArrowTxt);
    PrintLinePlayerList(d4, mask, a2);
}

/* One penalty summary line pair at printy: the period, the time, the team, the player; below, the minutes
 * and the penalty's name */
static void penalty_line(uint16_t a3)
{
    g.printx = 1;
    g.printa = 0x2000;
    uint16_t w = r16(a3);
    PrintToStatsPage(AdvanceStringPtr(SYM_PerLabels, (uint16_t)(w >> 14 & 3)));
    g.printx = 4;
    uint32_t a1 = PushTime(w & 0x0FFF);
    if (m16(a1) == 6) g.printx++;
    PrintToStatsPage(a1);
    g.printx = 0x0A;
    uint16_t b = r8(a3 + 2), d3 = b & 0x80;
    PrintTeamAbbrev(d3);
    g.printy -= 2;
    g.printx = 0x0E;
    PrintToStatsPage(PlayerInitialName(d3 ? 2 : 0, b & 0x7F));
    uint32_t pb = SYM_Penaltylist & 0xFF0000;
    uint16_t rec = (uint16_t)(SYM_Penaltylist + m16(SYM_Penaltylist + r8(a3 + 3)));
    g.printx = 0x1D;
    uint16_t q;
    PrintToStatsPage(PushNumberWrap(m8(pb | rec), 2, &q));
    g.printa = 0x2400;
    g.printy++;
    g.printx = 0x10;
    PrintToStatsPage(pb | (uint16_t)(rec + 2));
}

/* One scoring summary line set at printy from the goal entry at a3: period, time, team, strength (SH / PP
 * by the two kinds of each), the scorer and below him the assists */
static void scoring_line(uint32_t a3)
{
    g.printx = 1;
    g.printa = 0x2000;
    uint16_t w = m16(a3);
    PrintToStatsPage(AdvanceStringPtr(SYM_PerLabels, (uint16_t)(w >> 14 & 3)));
    g.printx = 4;
    uint32_t a1 = PushTime(w & 0x0FFF);
    if (m16(a1) == 6) g.printx++;
    PrintToStatsPage(a1);
    g.printx = 0x0A;
    uint16_t b = m8(a3 + 2), d3 = b & 0xF0, a2 = d3 ? 2 : 0;
    PrintTeamAbbrev(d3);
    g.printy -= 2;
    g.printx = 0x1C;
    switch (b & 0x0F) {
    case 0: PrintToStatsPage(SYM_ScoringSH2); break;
    case 1: PrintToStatsPage(SYM_ScoringSH); break;
    case 2: break;
    case 3: PrintToStatsPage(SYM_ScoringPP); break;
    default: PrintToStatsPage(SYM_ScoringPP2); break;
    }
    g.printx = 0x0E;
    PrintToStatsPage(PlayerInitialName(a2, m8(a3 + 3)));
    g.printa = 0x2400;
    g.printy++;
    uint16_t p = m8(a3 + 4);
    if (p != 0xFF) {
        g.printx = 0x10;
        PrintToStatsPage(PlayerInitialName(a2, p));
    }
    g.printy++;
    p = m8(a3 + 5);
    if (p == 0xFF) return;
    g.printx = 0x10;
    PrintToStatsPage(PlayerInitialName(a2, p));
}

/* One stat, a column's palette, at printx */
static void stat_cell(uint16_t col, uint16_t px, uint16_t v, uint16_t width)
{
    if (col) SetStatColumnPal(col);
    g.printx = px;
    uint16_t q;
    PrintToStatsPage(PushNumberWrap(v, width, &q));
}

/* The penalty / scoring summary list: Start leaves, up / down scroll it a row (six rows drawn while the IRQ
 * split slides, then five), the arrows redrawn (the penalties' pad read with key repeat) */
static void summary_loop(uint32_t none, int repeat)
{
    for (;;) {
        Pad p = getpzjoy();
        if (repeat) ProcessInputWithRepeat(p.press, p.change, p.held);
        if (p.held & 0x1000) return;
        if (p.held & 0x0800) {
            uint16_t v = (uint16_t)(g.listtop[0] - 1);
            if (!(v & 0x8000)) {
                g.listtop[0] = v;
                g.listrows[0] = 6;
                summary_page(none, 0, 0x0160);
                WaitFrame();
                g.irqvscroll1 = 0x15;
                g.irqvscroll2 = 0x18;
                do {
                    WaitFrame();
                    g.irqvscroll1 -= 2;
                } while (g.irqvscroll1 != 1);
                PrintScrollArrows();
                g.listrows[0] = 5;
                summary_page(none, 0, 0x0160);
                WaitFrame();
                g.irqvscroll1 = 0xFFFF;
                continue;
            }
        } else if (p.held & 0x0400) {
            g.listrows[0] = 6;
            summary_page(none, 0, 0x0160);
            g.listrows[0] = 5;
            uint16_t v = (uint16_t)(g.listtop[0] + 5);
            if ((uint16_t)(v - g.listend[0]) & 0x8000) {
                g.listtop[0] = (uint16_t)(v - 4);
                g.irqvscroll1 = 1;
                g.irqvscroll2 = 0x18;
                do {
                    WaitFrame();
                    g.irqvscroll1 += 2;
                } while (g.irqvscroll1 != 0x17);
                PrintScrollArrows();
                summary_page(none, 0, 0x0160);
                WaitFrame();
                g.irqvscroll1 = 0xFFFF;
                continue;
            }
        }
        WaitFrame();
    }
}

/* A summary screen: its title in letter sprites, the colours, the heading, the list (with the scroll hint
 * from 6 entries) */
static void summary_screen(uint32_t label, uint16_t lx, uint32_t head, uint32_t none, int penalties)
{
    g.setupvis = g.VisTeam[0];
    g.setuphome = g.HomeTeam;
    SetupScreen();
    g.curitem[0] = 0;
    g.listtop[0] = 0;
    g.listend[0] = 0;
    PrintLetterSprites(label, lx, 0x18);
    SpriteFrameEnd();
    w16(RAM_OFF_colourbuf + 0xC2, 0xFFFF);
    w16(RAM_OFF_colourbuf + 0x02, 0x2BFF);
    w16(RAM_OFF_colourbuf + 0x22, 0x6F7B);
    SendColours();
    print(head);
    if (penalties) g.listend[0] = g.PenSumLength;
    else {
        g.listend[0] = 0;
        for (uint16_t a = g.ScoreSumbytes[0]; !((uint16_t)(a - 6) & 0x8000); a -= 6) g.listend[0]++;
    }
    if (g.listend[0] >= 6) PrintScrollHint();
    if (penalties) FillPenaltyPage();
    else FillScoringPage();
    SendTextMap();
    SetupScrollIRQ(0x58, 0xD0, 0xE0);
    MenuScreenHDMA();
    ScreenOn();
    g.listrows[0] = 5;
    summary_page(none, 0, 0x0160);
    summary_loop(none, penalties);
}

/* The other scores list: its braces (none above the first game shown, or the user's game first), each
 * frame; Start leaves, up / down scroll a game (the user's game, skipped in the list, stepped over) */
static void otherscores_loop(void)
{
    for (;;) {
        uint16_t d3 = getpzjoy().held;
        if (d3 & 0x1000) return;
        if (d3 & 0x0800) {
            ClearOtherScoreBraces();
            uint16_t v = (uint16_t)(g.listtop[0] - 1);
            if (!(v & 0x8000) && !(v == g.gamenum && !v)) {
                if (v == g.gamenum) v--;
                g.listtop[0] = v;
                g.listrows[0] = 6;
                DrawOtherScoresPage();
                WaitFrame();
                g.irqvscroll1 = 0x15;
                g.irqvscroll2 = 0x18;
                do {
                    WaitFrame();
                    g.irqvscroll1 -= 2;
                } while (g.irqvscroll1 != 1);
                g.listrows[0] = 5;
                DrawOtherScoresPage();
                WaitFrame();
                g.irqvscroll1 = 0xFFFF;
                continue;
            }
        } else if (d3 & 0x0400) {
            ClearOtherScoreBraces();
            g.listrows[0] = 6;
            DrawOtherScoresPage();
            g.listrows[0] = 5;
            uint16_t a = (uint16_t)(g.listtop[0] + 5 + g.otherskip[0]);
            if ((uint16_t)(a - g.listend[0]) & 0x8000) {
                a = (uint16_t)(a - 4 - g.otherskip[0]);
                int go = 1;
                if (a == g.gamenum) {
                    a = (uint16_t)(a + 5 + g.otherskip[0]);
                    if ((uint16_t)(a - g.listend[0]) & 0x8000) a = (uint16_t)(a - 4 - g.otherskip[0]);
                    else go = 0;
                }
                if (go) {
                    g.listtop[0] = a;
                    g.irqvscroll1 = 1;
                    g.irqvscroll2 = 0x18;
                    do {
                        WaitFrame();
                        g.irqvscroll1 += 2;
                    } while (g.irqvscroll1 != 0x17);
                    DrawOtherScoresPage();
                    WaitFrame();
                    g.irqvscroll1 = 0xFFFF;
                    continue;
                }
            }
        }
        uint16_t lt = g.listtop[0];
        if (lt && !(lt == 1 && !g.gamenum)) {
            g.printx = 4;
            g.printy = (uint16_t)((uint16_t)(lt << 1) + lt + (lt >> 15));
            g.printa = 0x2000;
            PrintToStatsPage(SYM_LineBraces);
        }
        if ((uint16_t)(g.listtop[0] + g.listrows[0] + g.otherskip[0] - g.listend[0]) & 0x8000) {
            g.printx = 4;
            uint16_t r3 = (uint16_t)(g.listtop[0] + g.listrows[0] - 1);
            g.printy = (uint16_t)((uint16_t)(r3 << 1) + r3 + (r3 >> 15) + 1);
            g.printa = 0x2000;
            PrintToStatsPage((SYM_LineBraces & 0xFF0000) | (uint16_t)(SYM_LineBraces + 3));
        }
        DrawOtherScoresPage();
        WaitFrame();
    }
}

/* The crowd meter's lit part: the first crowdmeterlit words of each row of map [p0] (width and height, then
 * the words from +6) plus a, into the map at [p1] (r7 words a row) at column x, row y */
static void crowdmeter_lit(uint32_t p0, uint32_t p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7)
{
    uint32_t r2 = Mul16U((uint16_t)(y << 1), r7);
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)r2;
    uint16_t lo1 = (uint16_t)((uint16_t)p1 + (uint16_t)s + (s >> 16));
    uint16_t w = m16(p0), h = m16(p0 + 2);
    uint16_t lo0 = (uint16_t)((uint16_t)p0 + 6);
    uint32_t b0 = p0 & 0xFF0000, b1 = p1 & 0xFF0000;
    do {
        uint16_t k = 0;
        for (uint16_t n = g.crowdmeterlit[0]; n; n--, k += 2) mset16((b1 | lo1) + k, (uint16_t)(m16((b0 | lo0) + k) + a));
        lo1 = (uint16_t)((uint16_t)(r7 << 1) + lo1);
        lo0 = (uint16_t)((uint16_t)(w << 1) + lo0 + (w >> 15));
    } while (--h);
}

/* The crowd meter for level a dB: (a - 50) / 4 cells lit, past 18 the broken meter */
static void crowdmeter_bar(uint16_t a)
{
    uint16_t v = (uint16_t)(a - 0x32);
    if (v & 0x8000) v = 0;
    uint16_t r1 = v >> 2;
    uint32_t map = SYM_CrowdMeterMap;
    if (!((uint16_t)(r1 - 0x13) & 0x8000)) {
        r1 = 0;
        map = SYM_CrowdMeterBrokenMap;
    }
    uint16_t base = (uint16_t)(g.charofs + g.nextfreechar + 0x1C00);
    Decompress(map, unpackbuf);
    uint32_t p0 = unpackbuf, p1 = 0x7E0000 + RAM_OFF_textmap;
    PasteMap(&p0, &p1, base, 7, 0x0B, 0x20);
    WaitFrame();
    g.crowdmeterlit[0] = r1;
    if (!r1) return;
    Decompress(SYM_CrowdMeterLitMap, unpackbuf);
    crowdmeter_lit(unpackbuf, 0x7E0000 + RAM_OFF_textmap, base, 7, 0x0B, 0x20);
}

/* The crowd meter's three values (current, peak, average; curitem's highlighted) and its bar for it */
static void crowdmeter_values(void)
{
    uint16_t q, c = g.curitem[0];
    print(c ? SYM_CurrentTxt : SYM_CurrentTxt2);
    print(c ? SYM_DbTxt : SYM_DbTxt2);
    g.printx = 0x14;
    g.printy = 0x11;
    print(PushNumberWrap(g.CurCrowdMeter, 4, &q));
    print(c == 1 ? SYM_PeakTxt2 : SYM_PeakTxt);
    print(c == 1 ? SYM_DbTxt4 : SYM_DbTxt3);
    g.printx = 0x14;
    g.printy = 0x13;
    print(PushNumberWrap(g.CrowdPeak, 4, &q));
    print(c == 2 ? SYM_AverageTxt2 : SYM_AverageTxt);
    print(c == 2 ? SYM_DbTxt6 : SYM_DbTxt5);
    g.printx = 0x14;
    g.printy = 0x15;
    print(PushNumberWrap(g.CrowdAvg, 4, &q));
    crowdmeter_bar(c == 0 ? g.CurCrowdMeter : c == 1 ? g.CrowdPeak : g.CrowdAvg);
}

/* a long pointer's byte or word, indexed as [p],y does (the carry goes into the bank) */
static uint32_t lp(uint32_t bank, uint16_t ptr, uint16_t y) { return ((bank << 16 | ptr) + y) & 0xFFFFFF; }

/* Wait until the pause pad's buttons are up (the pads are read by the interrupt) */
void WaitButtonRelease(void)
{
    while (r16(RAM_OFF_JoyPad + g.pausepad)) hal_wait_interrupt();
}

/* The line editor of the pausing team: left / right the line (with line changes on; lines 6 and 7 have
 * four slots), up / down the slot, A / B pick a player for it, Start the menu */
void LineEditor(void)
{
    uint16_t a2 = GetPauseTeam();
    g.SelectedPlayerIdx = (uint16_t)(r16(RAM_OFF_tmline + a2) + 1);
    g.DispAttribCtr = 0;
    g.setupvis = r16(RAM_OFF_HomeTeam + a2);
    g.setuphome = 0xFFFF;
    SetupScreen();
    DrawEditLinesTop(a2);
    g.lineslots[0] = 5;
    g.linecursor[0] = 4;
    PrintLineNames(a2);
    SetupBandIRQ(5, 0x68);
    MenuScreenHDMA();
    ScreenOn();
    for (;;) {
        SendTextMap();
        WaitFrame();
        ReadPads();
        Pad p = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x1000) {
            if (LineEditorExit(a2)) return;
            PrintLineNames(a2);
            continue;
        }
        if (d1 & 0x8080) {
            uint16_t a = SelectLinePlayer(a2, 0x20);   /* (r7: $20, as DrawTeamScreen's paste left it) */
            if (!(a & 0x8000)) SetLineSlot(a, a2);
            g.drawlinesback = 1;
            PrintLineNames(a2);
            print(SYM_EditLinesScreenPos);
            print(AdvanceStringPtr(g.OptLine ? SYM_LineMenuNames : SYM_BestLineNames, g.SelectedPlayerIdx));
            SendTextMap();
            print(SYM_EditLinesScreenPos2);
            continue;
        }
        uint16_t d0;
        if (d1 & 0x0300) {
            d0 = d1 & 0x0100 ? 1 : 0xFFFF;
            if (!g.OptLine) continue;
            uint16_t v = (uint16_t)(d0 + g.SelectedPlayerIdx - 1);
            if ((v & 0x8000) || v >= 7) continue;
            v++;
            g.SelectedPlayerIdx = v;
            if (!((uint16_t)(v - 6) & 0x8000)) {
                if (g.lineslots[0] != 4) {
                    g.lineslots[0]--;
                    if (g.linecursor[0]) g.linecursor[0]--;
                }
            } else if (g.lineslots[0] != 5) {
                g.lineslots[0]++;
                g.linecursor[0]++;
            }
            PrintLineNames(a2);
            continue;
        }
        if (d1 & 0x0400) d0 = 0xFFFF;
        else if (d1 & 0x0800) d0 = 1;
        else continue;
        uint16_t v = (uint16_t)(d0 + g.linecursor[0]);
        if ((v & 0x8000) || !((uint16_t)(v - g.lineslots[0]) & 0x8000)) continue;
        g.linecursor[0] = v;
        PrintLineNames(a2);
    }
}

/* The selected line's name (the line change names, or the best line names without line changes), then its
 * players */
void PrintLineNames(uint16_t a2)
{
    print(SYM_PrintLineNamesPos);
    print(AdvanceStringPtr(g.OptLine ? SYM_LineMenuNames : SYM_BestLineNames, g.SelectedPlayerIdx));
    PrintLinePlayers(a2);
}

/* The line editor's top: cleared, the title, the frame outline */
void DrawEditLinesTop(uint16_t a2)
{
    print(SYM_DrawEditLinesTopPos);
    eraser(0x20, 0x0D, g.blankchar);
    DrawTeamScreen_title(a2);
    print(SYM_DrawEditLinesTopPos2);
    g.printa = 0x0800;
    FramerOutline(0x11, 3);
}

/* The line editor's title: the background redrawn (drawlinesback, then the band IRQ line back), the label,
 * team a2's block and the pictures */
void DrawTeamScreen_title(uint16_t a2)
{
    if (g.drawlinesback) DrawTeamScreen();
    SpriteFrameStart();
    PrintLetterSprites(SYM_EditLinesLabel, 0x58, 0x18);
    print(SYM_DrawEditLinesTitlePos);
    DrawTeamBlocks(a2 ? g.VisTeam[0] : g.HomeTeam, 0, a2);
    DrawMatchupPictures(a2);
    SpriteFrameEnd();
    if (g.drawlinesback) {
        WaitFrame();
        g.irqline2 = 0x68;
        g.drawlinesback = 0;
    }
    SendColours();
}

/* Line editor: erase the menu area (32 x 17) */
void ClearLineArea_erase(void)
{
    print(SYM_ClearEditLinesMenuPos);
    eraser(0x20, 0x11, g.blankchar);
}

/* The line editor's menu (Start): Return to game (A = $FFFF) or Restore default lines (team a2's lines
 * loaded again, A = 0) */
uint16_t LineEditorExit(uint16_t a2)
{
    ClearLineArea_erase();
    print(SYM_ReturnToGameTxt);
    print(SYM_RestoreDefaultLinesTxt);
    SendTextMap();
    WaitButtonRelease();
    g.linemenuitem[0] = 0;
    for (;;) {
        print(g.linemenuitem[0] ? SYM_EditLinesMenuPos2 : SYM_EditLinesMenuPos);
        print(SYM_ReturnToGame);
        print(g.linemenuitem[0] == 1 ? SYM_EditLinesMenuPos3 : SYM_EditLinesMenuPos4);
        print(SYM_RestoreDefaultLines);
        SendTextMap();
        WaitFrame();
        ReadPads();
        Pad p = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x9080) {
            if (!g.linemenuitem[0]) return 0xFFFF;
            LoadTeamLines(a2);
            ClearLineArea_erase();
            DrawEditLinesTop(a2);
            return 0;
        }
        if (d1 & 0x0400) g.linemenuitem[0] = 1;
        else if (d1 & 0x0800) g.linemenuitem[0] = 0;
    }
}

/* The players of the selected line (SelectedPlayerIdx; 0: the goalies) of team a2, a row a slot with its
 * position name, the cursor slot highlighted (its player the home matchup picture); then the title */
void PrintLinePlayers(uint16_t a2)
{
    print(SYM_PrintLinePlayersPos);
    eraser(0x20, 0x13, g.blankchar);
    print(SYM_PrintLinePlayersPos2);
    uint16_t d1 = (uint16_t)(GetGoalieCount(a2) - 1);
    uint32_t a5 = SYM_LineGoalieMarks, a3 = SYM_PrintLinePlayers_4;
    uint16_t d0 = (uint16_t)(g.SelectedPlayerIdx - 1);
    if (!(d0 & 0x8000)) {
        uint32_t t = GetTeamLines(a2);
        a3 = (t & 0xFF0000) | (uint16_t)(t + (uint16_t)(d0 << 3) + 1);
        a5 = SYM_PositionNames;
        d1 = 4;
        if ((int16_t)g.SelectedPlayerIdx > 5) d1 = 3;
    }
    do {
        g.printa = 0x3800;
        if (d1 == g.linecursor[0]) {
            g.printa = 0x2000;
            g.matchuphome = (uint16_t)(m8(a3) - 1);
        }
        g.printx = 5;
        a5 = print(a5);
        g.printx = 8;
        print(getname(a2, (uint16_t)(m8(a3) - 1)));
        a3 = (a3 & 0xFF0000) | (uint16_t)(a3 + 1);
        g.printy += 2;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
    DrawTeamScreen_title(a2);
}

/* Put player a in the cursor slot of the selected line (SelectedPlayerIdx) of team a2; if he is
 * already in a nearby slot of the line (searched by how far the cursor is from the line's end), the
 * old occupant of the cursor slot moves there */
void SetLineSlot(uint16_t a, uint16_t a2)
{
    g.lineplayer[0] = a;
    uint16_t d0 = (uint16_t)((uint16_t)(g.SelectedPlayerIdx - 1) << 3);
    d0 = (uint16_t)((uint16_t)GetTeamLines(a2) + d0 + 1);
    uint16_t a3, b;
    switch ((uint16_t)(g.lineslots[0] - g.linecursor[0])) {
    case 1:
        b = (uint16_t)(d0 + 1);
        goto at3;
    case 2:
        b = d0;
        goto at1;
    case 3:
        if (ReadLineSlot((uint16_t)(d0 + 4)) == g.lineplayer[0]) { SwapLineSlots(d0, 0); break; }
        b = (uint16_t)(d0 + 3);
    at3:
        if (ReadLineSlot(b) == g.lineplayer[0]) swap_next_slot((uint16_t)(b - 1));
        break;
    case 4:
        if (ReadLineSlot((uint16_t)(d0 + 4)) == g.lineplayer[0]) { swap_next_slot((uint16_t)(d0 + 3)); break; }
        b = (uint16_t)(d0 + 2);
        goto at1;
    default:
        if (ReadLineSlot((uint16_t)(d0 + 2)) == g.lineplayer[0]) { SwapLineSlots(d0, 0); break; }
        b = (uint16_t)(d0 + 3);
    at1:
        if (ReadLineSlot(b) == g.lineplayer[0]) swap_next_slot(b);
        break;
    }
    uint32_t s = (uint32_t)d0 + g.lineslots[0];
    a3 = (uint16_t)(s - g.linecursor[0] - 1 + (s >> 16));
    WriteLineSlot(a3, g.lineplayer[0]);
}

/* Swap the line slot bytes at d0+2 and d0+4 (in a3's bank); a3 is left at the second */
uint32_t SwapLineSlots(uint16_t d0, uint32_t a3)
{
    a3 = (a3 & 0xFF0000) | (uint16_t)(d0 + 2);
    g.lineswap[0] = ReadLineSlot(a3);
    uint32_t b = (a3 & 0xFF0000) | (uint16_t)(a3 + 2);
    WriteLineSlot(a3, ReadLineSlot(b));
    WriteLineSlot(b, g.lineswap[0]);
    return b;
}

uint16_t ReadLineSlot(uint32_t a3) { return m8(a3); }

/* Write the low byte of v to the line slot at a3 / read it */
void WriteLineSlot(uint32_t a3, uint16_t v) { put8(a3, (uint8_t)v); }

/* Line editor: pick a player of team a2 for the cursor slot (lineslots - linecursor) of line
 * SelectedPlayerIdx from the list of defensemen (the last slots) or forwards, 9 rows at a time,
 * starting at the player in the slot; left / right change the attribute column. Returns the
 * player (roster index), $FFFF when start cancels. (r7: the map width the caller left for PasteMap.) */
uint16_t SelectLinePlayer(uint16_t a2, uint16_t r7)
{
    print(SYM_SelectLinePlayerPos);
    SpriteFrameStart();
    eraser(0x20, 0xD, g.blankchar);
    Decompress(SYM_PlayerListMap, unpackbuf);
    uint32_t p0 = unpackbuf, p1 = 0x7E0000u + RAM_OFF_textmap2;
    WaitFrame();
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 1, 1, r7);
    SendTextMapRows();
    SendTextMap2Rows();
    SpriteFrameEnd();
    WaitFrame();
    g.irqline2 = 7;
    if ((uint16_t)(g.lineslots[0] - g.linecursor[0] - 3) & 0x8000) {
        g.listcount[0] = GetDefenseCount(a2);
        g.pickfirst[0] = GetGoalieCount(a2);
        g.pickfirst[0] = (uint16_t)(g.pickfirst[0] + GetForwardCount(a2));
    } else {
        g.listcount[0] = GetForwardCount(a2);
        g.pickfirst[0] = GetGoalieCount(a2);
    }
    g.picktop[0] = g.pickfirst[0];
    g.pickrows[0] = 9;
    if ((uint16_t)(9 - g.listcount[0]) && !((uint16_t)(9 - g.listcount[0]) & 0x8000)) g.pickrows[0] = g.listcount[0];
    uint16_t d0 = (uint16_t)((uint16_t)(g.SelectedPlayerIdx - 1) << 3);
    uint16_t a3 = (uint16_t)((uint16_t)GetTeamLines(a2) + d0 + g.lineslots[0] - g.linecursor[0]);
    g.curplayer[0] = ReadLineSlot(a3);
    uint16_t t = (uint16_t)(g.pickrows[0] + g.picktop[0] - g.curplayer[0]);
    if (!(t & 0x8000)) g.curplayer[0] = t;
    else {
        t = (uint16_t)(g.curplayer[0] - g.pickrows[0] - g.picktop[0]);
        if (!(t & 0x8000)) g.picktop[0] = (uint16_t)(t + g.picktop[0]);
        g.curplayer[0] = 0;
    }
    line_list_redraw(a2);
    for (;;) {
        SendTextMap();
        SendTextMap2();
        WaitFrame();
        ReadPads();
        Pad pd = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(pd.press, pd.change, pd.held);
        if (d1 & 0x1000) return 0xFFFF;
        if (d1 & 0x8080) return (uint16_t)(g.picktop[0] + g.pickrows[0] - g.curplayer[0]);
        if (d1 & 0x0300) {
            d0 = (uint16_t)((d1 & 0x0100 ? 1 : 0xFFFF) + g.DispAttribCtr);
            if (d0 & 0x8000) continue;
            g.DispAttribCtr = d0;
            line_list_redraw(a2);
            continue;
        }
        if (!(d1 & 0x0C00)) continue;
        d0 = (uint16_t)((d1 & 0x0400 ? 0xFFFF : 1) + g.curplayer[0]);
        if (d0 & 0x8000) {
            if (!((uint16_t)(g.picktop[0] - g.pickfirst[0] + g.pickrows[0] - g.listcount[0]) & 0x8000)) continue;
            g.picktop[0]++;
            line_list_redraw(a2);
            w16(RAM_OFF_BG3VScroll, 0xFFFF);
        } else if (!((uint16_t)(d0 - g.pickrows[0]) & 0x8000)) {
            if (g.picktop[0] == g.pickfirst[0]) continue;
            g.picktop[0]--;
            line_list_redraw(a2);
            w16(RAM_OFF_BG3VScroll, 0xFFFF);
            g.BG1VScroll = 0xFFFF;
        } else {
            g.curplayer[0] = d0;
            line_list_redraw(a2);
        }
    }
}

/* The line editor's background: the top cleared, the frame outline, the background map into text map 2 */
void DrawTeamScreen(void)
{
    print(SYM_DrawEditLinesBackPos);
    eraser(0x20, 0x0D, g.blankchar);
    print(SYM_DrawEditLinesBackPos2);
    g.printa = 0x0800;
    FramerOutline(0x11, 3);
    Decompress(SYM_MenuBackMap, unpackbuf);
    uint32_t p0 = unpackbuf, p1 = 0x7E0000 + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 0, 0, 0x20);
}

/* The line editor's pick list: pickrows players from picktop + 1 (the cursor's highlighted), each with
 * attribute d4 of mask (goalies' from the goalie list) */
void PrintLinePlayerList(uint16_t d4, uint16_t mask, uint16_t a2)
{
    print(SYM_PrintLinePlayerListPos);
    uint16_t a3 = (uint16_t)(g.picktop[0] + 1);
    uint16_t d1 = (uint16_t)(g.pickrows[0] - 1);
    do {
        g.printa = 0x3800;
        if ((uint8_t)d1 == (uint8_t)g.curplayer[0]) g.printa = 0x2000;
        g.printx = 2;
        uint16_t r1 = (uint16_t)((a3 & 0xFF) - 1), r2 = 0;
        if ((uint16_t)(r1 - GetGoalieCount(a2)) & 0x8000) r2 = 0x10;
        GetNameandAttrib(r1, d4, mask, a2, r2);
        a3++;
        g.printy++;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
    g.printa = 0x3800;
}

/* The forwards on team a2 */
uint16_t GetForwardCount(uint16_t a2) { return team_counts(a2) >> 4 & 0xF; }

/* The defensemen on team a2 */
uint16_t GetDefenseCount(uint16_t a2) { return team_counts(a2) & 0xF; }

/* The RAM lines of team a2 (a3, bank 0): homelines for team 0, else awaylines */
uint32_t GetTeamLines(uint16_t a2) { return a2 ? awaylines : homelines; }

void ScoringSummaryScreen(void) { summary_screen(SYM_ScoringLabel, 0x5E, SYM_ScoringSummaryHead, SYM_NoScoring, 0); }

/* The scoring summary page in the stats page buffer: a line set per goal of the score summary */
void FillScoringPage(void)
{
    MemFill(statspage, 0x4D80, (uint16_t)(g.smallfontchars + g.printfontset));
    uint16_t a3 = (uint16_t)ScoreSum;
    g.printy = 0;
    for (uint16_t k = 0; k != g.listend[0]; k++) {
        scoring_line((ScoreSum & 0xFF0000) | a3);
        a3 = (uint16_t)(a3 + 6);
        g.printy++;
    }
}

/* The penalty summary and the scoring summary screens */
void PenaltySummaryScreen(void) { summary_screen(SYM_PenaltiesLabel, 0x58, SYM_PenaltySummaryHead, SYM_NoPenalties, 1); }

/* The penalty summary page in the stats page buffer: a line pair for each of the listend logged penalties */
void FillPenaltyPage(void)
{
    MemFill(statspage, 0x4D80, (uint16_t)(g.smallfontchars + g.printfontset));
    uint16_t a3 = RAM_OFF_PenSum;
    g.printy = 0;
    for (uint16_t k = 0; k != g.listend[0]; k++) {
        penalty_line(a3);
        a3 += 4;
        g.printy += 2;
    }
}

/* Print the scroll hint under a summary list */
void PrintScrollHint(void)
{
    print(SYM_PrintScrollHintPos);
    FramerOutline(0x0E, 3);
    g.printa = 0x2400;
    g.printx = 0x0A;
    g.printy = 7;
    print(SYM_ScrollHint);
}

/* Print the abbreviation of the home team (d3 = 0) or the visitors on the stats page, then two rows down */
void PrintTeamAbbrev(uint16_t d3)
{
    uint16_t px = g.printx;
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a1 = d3 ? g.tmdata[1] : g.tmdata[0];
    uint32_t s = (uint32_t)m16((bank | a1) + 4) + a1;
    a1 = (uint16_t)s;
    a1 = (uint16_t)(m16(bank | a1) + a1 + (s >> 16));   /* (the first add's carry) */
    PrintToStatsPage(bank | a1);
    g.printx = px;
    g.printy += 2;
}

/* The crowd meter screen: the current, peak and average levels, up / down choose the one the meter shows,
 * Start leaves */
void CrowdMeterScreen(void)
{
    uint16_t a2 = r16(RAM_OFF_a2);   /* (as the caller left it) */
    g.setupvis = g.VisTeam[0];
    g.setuphome = g.HomeTeam;
    SetupScreen();
    Decompress(SYM_CrowdMeterFramePalette, unpackbuf);
    SetColours(unpackbuf, 0x10, 0x50);
    SendColours();
    print(SYM_ShowCrowdMeterPos);
    g.printa = 0x0800;
    FramerNineTile(0x14, 5);
    uint16_t n = Decompress(SYM_CrowdMeterTiles, unpackbuf);
    LinearToPlanar(unpackbuf, n, n);
    uint32_t p0 = (unpackbuf & 0xFF0000) | n;
    DMAToVRAM(&p0, n, (uint16_t)((uint16_t)(g.nextfreechar << 4) + 0x1000), 0);
    SetColours(SYM_CrowdMeterPalette, 0x10, 0x70);
    w16(RAM_OFF_colourbuf + 0xC2, 0x6F7B);
    SendColours();
    g.curitem[0] = 0;
    ScreenOn();
    for (;;) {
        SpriteFrameStart();
        PrintLetterSprites(SYM_CrowdMeter, 0x48, 0x18);
        DrawMatchupPictures(a2);
        crowdmeter_values();
        SpriteFrameEnd();
        for (;;) {
            SendTextMap();
            WaitFrame();
            ReadPads();
            Pad p = getpzjoy();
            uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
            if (d1 & 0x1000) return;
            if (d1 & 0x0800) {
                uint16_t v = (uint16_t)(g.curitem[0] - 1);
                if (v & 0x8000) continue;
                g.curitem[0] = v;
                break;
            }
            if (d1 & 0x0400) {
                uint16_t v = (uint16_t)(g.curitem[0] + 1);
                if (v >= 3) continue;
                g.curitem[0] = v;
                break;
            }
        }
    }
}

/* The other scores screen: the other games of the round (the user's skipped), five at a time */
void ShowScores(void)
{
    g.setupvis = 0x1C;
    g.setuphome = 0x1D;
    SetupScreen();
    PrintLetterSprites(SYM_OtherScores, 0x49, 0x18);
    SpriteFrameEnd();
    SendColours();
    g.curitem[0] = 0;
    g.listtop[0] = g.gamenum ? 0 : 1;
    uint16_t d1, d2;
    GetShifter(&d1, &d2);
    g.listend[0] = (uint16_t)(d1 + 1);
    FillOtherScores(0);   /* (r0: DrawSprite's piece count, 0 after the title's last letter) */
    SendTextMap();
    SetupScrollIRQ(0x50, 0xC0, 0xD8);
    MenuScreenHDMA();
    ScreenOn();
    g.listrows[0] = 5;
    DrawOtherScoresPage();
    otherscores_loop();
}

/* The other scores' rows shown to VRAM $0140, or "no other games" */
void DrawOtherScoresPage(void) { summary_page(SYM_NoOtherGames, 1, 0x0140); }

/* Other scores: from game r0 on, every game's line but the user's (counted in otherskip), up to listend */
void FillOtherScores(uint16_t r0)
{
    MemFill(statspage, 0x4D80, (uint16_t)(g.smallfontchars + g.printfontset));
    if (g.listend[0] == 1) return;
    g.printa = 0x2400;
    g.printy = 0;
    g.curitem[0] = r0;
    g.otherskip[0] = 0;
    for (;;) {
        if (g.curitem[0] == g.gamenum) g.otherskip[0]++;
        else {
            if (g.curitem[0] == g.listend[0]) return;
            PrintOtherScore();
            g.printy++;
        }
        g.curitem[0]++;
    }
}

/* Erase the braces around the other scores shown (three rows a game from listtop), before a scroll */
void ClearOtherScoreBraces(void)
{
    uint16_t t = g.listtop[0];
    uint16_t top = (uint16_t)((uint16_t)(t << 1) + t + (t >> 15));   /* (asl's carry) */
    uint16_t r3 = (uint16_t)(t + g.listrows[0] - 1);
    clear_braces(4, top, (uint16_t)((uint16_t)(r3 << 1) + r3 + (r3 >> 15) + 1));
}

/* Other scores: game curitem's two teams with their scores, and its period label (one on for a
 * decided period 4) */
void PrintOtherScore(void)
{
    uint16_t x = (uint16_t)(g.curitem[0] << 1);
    PrintOtherScoreTeam(r16(RAM_OFF_gst1 + x), r16(RAM_OFF_gss1 + x));
    g.printy++;
    PrintOtherScoreTeam(r16(RAM_OFF_gst2 + x), r16(RAM_OFF_gss2 + x));
    g.printx = 0x17;
    uint16_t d0 = r16(RAM_OFF_gsper + x);
    if (r16(RAM_OFF_gss1 + x) != r16(RAM_OFF_gss2 + x) && d0 == 4) d0++;
    uint32_t a0 = SYM_PerLabels, a1;
    do {
        a1 = a0;
        a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + m16(a0));
        d0--;
    } while (d0 && !(d0 & 0x8000));
    PrintToStatsPage(a1);
    g.printy++;
}

/* The full name of team a (a1): its TeamList city, or the long New York names for teams $0D / $0E */
uint32_t GetTeamLongName(uint16_t a)
{
    if (a == 0x0D) return SYM_LongNYIslanders;
    if (a == 0x0E) return SYM_LongNYRangers;
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint32_t a1 = bank | m16(SYM_TeamList + (uint16_t)(a << 2));
    return bank | (uint16_t)(a1 + m16(a1 + 4));
}

/* Other scores: team a's full name at column 7 and its score d0 at column $14 of the stats page */
void PrintOtherScoreTeam(uint16_t a, uint16_t d0)
{
    uint16_t q;
    uint32_t a1 = GetTeamLongName(a);
    g.printx = 7;
    PrintToStatsPage(a1);
    g.printx = 0x14;
    PrintToStatsPage(PushNumberWrap(d0, 2, &q));
}

/* The player stats screen of the pausing team (Y the other team): up / down scroll the rows, left / right
 * the sorted column (column 0: the goalies), Start leaves */
void PlayerStatsScreen(void)
{
    uint16_t a2 = GetPauseTeam();
    g.setupvis = r16(RAM_OFF_HomeTeam + a2);
    g.setuphome = 0x1D;
    SetupScreen();
    PrintLetterSprites(SYM_PlayerStatsTitle, 0x49, 0x18);
    SpriteFrameEnd();
    SendColours();
    print(SYM_ShowPlayerStatsPos);
    g.printa = 0x0800;
    FramerOutline(0x0E, 3);
    print(SYM_YSwitchTeamsTxt);
    g.statcolumn = 1;
    SetPlayerStatsRange(a2);
    g.listrows[0] = 7;
    InitStatsOrder();
    SortPlayerStats(a2);
    SetupScrollIRQ(0x58, 0xC0, 0xD0);
    g.irqvscroll2 = 0x18;
    g.playerstatson = 1;
    SendTextMap();
    DrawStatsList();
    MenuScreenHDMA();
    ScreenOn();
    for (;;) {
        ReadPads();
        Pad p = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held), d3 = p.held;
        if (d3 & 0x1000) {
            PlayerStatsScreen_exit();
            return;
        }
        if (d3 & 0x0800) {
            ClearPlayerStatBraces();
            uint16_t v = (uint16_t)(g.curitem[0] - 1);
            if (!((uint16_t)(v - g.listtop[0]) & 0x8000)) {
                g.curitem[0] = v;
                g.listrows[0]++;
                DrawStatsList();
                g.listrows[0]--;
                WaitFrame();
                g.irqvscroll1 = 0x0D;
                do {
                    WaitFrame();
                    g.irqvscroll1 -= 2;
                } while (g.irqvscroll1 != 1);
                DrawStatsList();
                WaitFrame();
                g.irqvscroll1 = 0xFFFF;
                continue;
            }
        } else if (d3 & 0x0400) {
            ClearPlayerStatBraces();
            if ((uint16_t)(g.curitem[0] + g.listrows[0] - 1 - g.listend[0]) & 0x8000) {
                g.listrows[0]++;
                DrawStatsList();
                g.listrows[0]--;
                WaitFrame();
                g.curitem[0]++;
                g.irqvscroll1 = 1;
                do {
                    WaitFrame();
                    g.irqvscroll1 += 2;
                } while (g.irqvscroll1 != 0x0D);
                DrawStatsList();
                WaitFrame();
                g.irqvscroll1 = 0xFFFF;
                continue;
            }
        } else if (d1 & 0x0300) {
            uint16_t v = g.statcolumn;
            int move = 0;
            if (d1 & 0x0200) { v--; move = !(v & 0x8000); }
            else { move = (uint16_t)(v - 5) & 0x8000; v++; }
            if (move) {
                g.statcolumn = v;
                if (v == (d1 & 0x0200 ? 0 : 1)) {
                    SetPlayerStatsRange(a2);
                    InitStatsOrder();
                }
                SortPlayerStats(a2);
                DrawStatsList();
            }
        } else if (d1 & 0x4000) {
            a2 ^= 2;
            SetPlayerStatsRange(a2);
            InitStatsOrder();
            SortPlayerStats(a2);
            SpriteFrameStart();
            g.setupvis = r16(RAM_OFF_HomeTeam + a2);
            PrintLetterSprites(SYM_PlayerStatsHeading, 0x49, 0x18);
            DrawMatchupPictures(a2);
            WaitFrame();
            DrawStatsList();
            SpriteFrameEnd();
            SendColours();
        }
        if (g.statcolumn) {
            if (g.curitem[0] != g.listtop[0]) {
                g.printx = 1;
                g.printy = (uint16_t)(g.curitem[0] << 1);
                g.printa = 0x2000;
                PrintToStatsPage(SYM_LineBraces);
            }
            uint16_t last = (uint16_t)(g.curitem[0] + g.listrows[0] - 1);
            if (last != g.listend[0]) {
                g.printx = 1;
                g.printy = (uint16_t)(last << 1);
                g.printa = 0x2000;
                PrintToStatsPage((SYM_LineBraces & 0xFF0000) | (uint16_t)(SYM_LineBraces + 3));
            }
            DrawStatsList();
        }
        WaitFrame();
    }
}

/* The end of the player stats screen: player stats off */
void PlayerStatsScreen_exit(void) { g.playerstatson = 0; }

/* The first and last row of the player stats list of team a2: its goalies (statcolumn 0), else
 * its skaters (after the goalies); curitem the first */
void SetPlayerStatsRange(uint16_t a2)
{
    if (!g.statcolumn) {
        g.listtop[0] = 0;
        g.listend[0] = GetGoalieCount(a2);
    } else {
        g.listtop[0] = GetGoalieCount(a2);
        g.listend[0] = GetPlayerCount(a2);
    }
    g.listend[0]--;
    g.curitem[0] = g.listtop[0];
}

/* Erase the braces around the player-stat rows shown (from curitem, listrows of them), before a scroll */
void ClearPlayerStatBraces(void)
{
    clear_braces(1, (uint16_t)(g.curitem[0] << 1), (uint16_t)((uint16_t)(g.curitem[0] + g.listrows[0] - 1) << 1));
}

/* Player stats: the header (skaters' or goalies') and its rows to VRAM, then the visible rows of the stats
 * page from row curitem */
void DrawStatsList(void)
{
    uint16_t d1;
    uint32_t p0 = 0x7E0000 + RAM_OFF_textmap + 0x1C0;
    if (g.statcolumn) {
        print(SYM_PlayerGAPtShPmTxt);
        d1 = (uint16_t)(g.listrows[0] - 1);
        DMAToVRAM(&p0, 0x00C0, 0x00E0, 0);
    } else {
        print(SYM_GoalieGaShPtgTxt);
        d1 = g.listend[0];
        DMAToVRAM(&p0, 0x04C0, 0x00E0, 0);
    }
    uint16_t t = (uint16_t)(g.curitem[0] << 8 | g.curitem[0] >> 8);
    p0 = (statspage & 0xFF0000) | (uint16_t)((t >> 1) + (uint16_t)statspage + (t & 1));   /* (lsr's carry) */
    t = (uint16_t)(d1 << 8 | d1 >> 8);
    DMAToVRAM(&p0, (uint16_t)((t >> 1) + 0x40 + (t & 1)), 0x0160, 0);
}

/* The player stats page from row listtop to listend, in statsorder: skaters' goals, assists, points, shots
 * and penalty minutes (the sorted column highlighted), or goalies' goals against, shots and save
 * percentage */
void DrawStatsList_rows(uint16_t a2)
{
    MemFill(statspage, 0x4D80, (uint16_t)(g.smallfontchars + g.printfontset));
    uint16_t y = g.listtop[0];
    g.printy = (uint16_t)((uint16_t)(y - 1) << 1);
    do {
        g.printy += 2;
        uint16_t x = g.statsorder[y];
        SetStatColumnPal(0xFFFF);
        g.printx = 3;
        PrintToStatsPage(PlayerInitialName(a2, x));
        g.statstemp[0] = x;
        x = (uint16_t)((a2 ? 0x1A : 0) + g.statstemp[0]);
        if (g.statcolumn) {
            stat_cell(1, 0x11, g.tmgoals[x], 2);
            stat_cell(2, 0x14, g.tmassists[x], 2);
            stat_cell(3, 0x17, (uint16_t)(g.tmgoals[x] + g.tmassists[x]), 2);
            stat_cell(4, 0x1A, g.tmpshots[x], 2);
            stat_cell(5, 0x1D, g.ppenmin[x], 2);
            SetStatColumnPal(0xFFFF);
        } else {
            stat_cell(0, 0x12, g.tmgoals[x], 3);
            stat_cell(0, 0x16, g.tmpshots[x], 3);
            uint16_t r2 = Mul8XS(g.tmgoals[x], 0x64), r1 = g.tmpshots[x];
            uint16_t d0 = r1 ? Div16U(r2, r1) : 0x64;
            stat_cell(0, 0x1B, (uint16_t)(0x64 - d0), 3);
            g.printx = 0x1E;
            PrintToStatsPage(SYM_PercentSign);
        }
        y++;
    } while (y <= g.listend[0]);
}

/* statsorder[i] = i for the rows of the list (one at least) */
void InitStatsOrder(void)
{
    uint16_t a = 0, y = 0xFFFF;
    do {
        y++;
        w8(RAM_OFF_statsorder + y, (uint8_t)a);
        a++;
    } while (y < g.listend[0]);
}

/* Player stats of team a2: the column's name, the rows from listtop sorted by column statcolumn
 * (ComparePlayerStats), the page filled */
void SortPlayerStats(uint16_t a2)
{
    print(SYM_SortPlayerStatsPos);
    print(AdvanceStringPtr(SYM_StatColumnNames, g.statcolumn));
    uint16_t x = g.listtop[0];
    do {
        uint16_t y = x;
        do {
            y++;
            ComparePlayerStats(x, y, a2);
            if (g.statstemp[0]) {
                uint8_t t = g.statsorder[y];
                w8(RAM_OFF_statstemp, t);
                g.statsorder[y] = g.statsorder[x];
                g.statsorder[x] = t;
            }
        } while (y < g.listend[0]);
        x++;
    } while (x < g.listend[0]);
    DrawStatsList_rows(a2);
}

/* Compare stat column statcolumn of the players at order slots x and y of team a2: statstemp = 1 when y's
 * is greater (signed bytes; goals + assists as words), else 0 */
void ComparePlayerStats(uint16_t x, uint16_t y, uint16_t a2)
{
    uint16_t px = (uint16_t)((a2 ? 0x1A : 0) + r8(RAM_OFF_statsorder + x));
    uint16_t py = (uint16_t)((a2 ? 0x1A : 0) + r8(RAM_OFF_statsorder + y));
    uint32_t col;
    int gt = 0;
    switch (g.statcolumn) {
    case 1: col = RAM_OFF_tmgoals; break;
    case 2: col = RAM_OFF_tmassists; break;
    case 4: col = RAM_OFF_tmpshots; break;
    case 5: col = RAM_OFF_ppenmin; break;
    case 3: {
        uint16_t a = (uint16_t)(r8(RAM_OFF_tmgoals + py) + r8(RAM_OFF_tmassists + py));
        uint16_t b = (uint16_t)(r8(RAM_OFF_tmgoals + px) + r8(RAM_OFF_tmassists + px));
        gt = a != b && !((uint16_t)(a - b) & 0x8000);
        col = 0;
        break;
    }
    default: col = 0;
    }
    if (col) {
        uint8_t d = (uint8_t)(r8(col + py) - r8(col + px));
        gt = d && !(d & 0x80);
    }
    g.statstemp[0] = gt ? 1 : 0;
}

/* Text palette $2000 for the sorted stat column, else $2400 */
void SetStatColumnPal(uint16_t column) { g.printa = column == g.statcolumn ? 0x2000 : 0x2400; }

/* The team roster screen (pause menu) of the pause pad's team: the title, frames, team block and
 * the current line with an attribute column; then the loop (TeamRosterLoop): start leaves
 * (RosterRtl), Y switches team (SwapRosterTeam, then TeamRosterRedraw), left / right step the
 * column (MoveRosterColumn), up / down the line (0-7) */
void TeamRosterScreen(void)
{
    uint16_t a2 = GetPauseTeam();
    g.SelectedPlayerIdx = (uint16_t)(r16(RAM_OFF_tmline + a2) + 1);
    g.DispAttribCtr = 0;
    g.setupvis = r16(RAM_OFF_HomeTeam + a2);
    g.setuphome = 0x1D;
    SetupScreen();
    PrintLetterSprites(SYM_TeamRosterTitle, 0x4C, 0x18);
    FadeIn();
    MenuScreenHDMA();
    ScreenOn();
    MemFill(0x7E0000u + RAM_OFF_textmap, 0x800, (uint16_t)(g.blankchar + g.charofs));
    print(SYM_PlayerRatingTxt);
    g.printa = 0x0800;
    FramerOutline(0xF, 3);
    print(SYM_TeamRosterPos);
    g.printa = 0x0800;
    FramerOutline(0xF, 3);
redraw:
    print(SYM_TeamRosterPos2);
    DrawTeamBlocks(a2 ? g.VisTeam[0] : g.HomeTeam, SYM_TeamRosterTitle, a2);
    SpriteFrameEnd();
    g.colourbuf[1] = 0x7F97;
    g.colourbuf[0x21] = 0xFFFF;
    SendColours();
    ShowRosterLine(a2);
    for (;;) {
        SendTextMap();
        WaitFrame();
        ReadPads();
        Pad pd = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(pd.press, pd.change, pd.held);
        if (d1 & 0x1000) return;
        if (d1 & 0x4000) {
            a2 ^= 2;
            SpriteFrameStart();
            g.setupvis = r16(RAM_OFF_HomeTeam + a2);
            PrintLetterSprites(SYM_TeamRosterSwap, 0x4C, 0x18);
            DrawMatchupPictures(a2);
            goto redraw;
        }
        uint16_t d0;
        if (d1 & 0x0300) {
            d0 = (uint16_t)((d1 & 0x0100 ? 1 : 0xFFFF) + g.DispAttribCtr);
            if (d0 & 0x8000) continue;
            g.DispAttribCtr = d0;
            ShowRosterLine(a2);
            continue;
        }
        if (!(d1 & 0x0C00)) continue;
        d0 = (uint16_t)((d1 & 0x0800 ? 0xFFFF : 1) + g.SelectedPlayerIdx);
        if ((d0 & 0x8000) || d0 >= 8) continue;
        g.SelectedPlayerIdx = d0;
        ShowRosterLine(a2);
    }
}

/* Print the roster line SelectedPlayerIdx of team a2 (0 the goalies) with the attribute column
 * DispAttribCtr (stepped back past the menu's end): the line name and column title, the area below
 * cleared, then each player of the line with GetNameandAttrib under its position name (or goalie
 * mark) */
void ShowRosterLine(uint16_t a2)
{
    print(SYM_ShowRosterLinePos);
    print(AdvanceStringPtr(SYM_RosterLineNames, g.SelectedPlayerIdx));
    g.printx = 0x11;
    g.printy = 9;
    uint32_t a1;
    for (;;) {
        a1 = g.SelectedPlayerIdx ? SYM_AttribMenuSkater : SYM_AttribMenuGoalie;
        uint16_t d0 = g.DispAttribCtr;
        while (!(m16(a1) & 0x8000) && !(--d0 & 0x8000)) {
            uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
            a1 = (a1 & 0xFF0000) | (uint16_t)((s & 0xFFFF) + 4 + (s >> 16));
        }
        if (!(m16(a1) & 0x8000)) break;
        g.DispAttribCtr--;
    }
    a1 = print(a1);
    uint16_t mask = m16(a1), d4 = m16(a1 + 2);
    print(SYM_ShowRosterLinePos2);
    eraser(0x20, 0x13, g.blankchar);
    print(SYM_YSwitchTeamsTxt2);
    uint16_t d1 = (uint16_t)(GetGoalieCount(a2) - 1);
    uint32_t a5 = SYM_RosterGoalieMarks, a3 = SYM_RosterGoalieList;
    uint16_t d0 = (uint16_t)(g.SelectedPlayerIdx - 1);
    if (!(d0 & 0x8000)) {
        d0 = (uint16_t)(d0 << 3);
        uint32_t t = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
        a3 = (t & 0xFF0000) | (uint16_t)((uint16_t)t + m16(t + 6) + d0 + 1);
        a5 = SYM_PositionNames;
        d1 = (int16_t)g.SelectedPlayerIdx > 5 ? 3 : 4;
    }
    do {
        g.printx = 1;
        a5 = print(a5);
        g.printx = 4;
        uint16_t r1 = (uint16_t)(m8(a3) - 1);
        a3 = (a3 & 0xFF0000) | (uint16_t)(a3 + 1);
        uint16_t r2 = (uint16_t)(r1 - GetGoalieCount(a2)) & 0x8000 ? 0x10 : 0;
        GetNameandAttrib(r1, d4, mask, a2, r2);
        g.printy += 2;
    } while (!(--d1 & 0x8000));
}

/* The roster routines' shared return (nothing to do) */
void RosterRtl(void) {}

/* d0 = rating column d1 of player curplayer of team a2, with the hot / cold adjustment */
uint16_t GetPlayerRating(uint16_t d1, uint16_t a2) { return player_rating(d1, a2, 1); }

/* d0 = rating column d1 of player curplayer of team a2, without the hot / cold adjustment */
uint16_t GetPlayerBaseRating(uint16_t d1, uint16_t a2) { return player_rating(d1, a2, 0); }

/* Common start of the stats screens: the old screen faded out, the team logo sprites unpacked, the
 * video registers, the small font, Framer, team block, nine tile and outline tiles one after the
 * other (each set's first tile kept), the menu background (its 7 x 7 logo box part and the whole
 * map in textmap2) and its palettes; for setupvis $21 and up (less $21) the setup background over
 * it; colour 0 black, the sprite palettes, the text map cleared, the sprites started and the
 * matchup pictures drawn */
void SetupScreen(void)
{
    g.FrameByNumber = 0;
    forcefade();
    Decompress(SYM_TeamLogoSprites, logosprbuf);
    WaitFadeOut();
    forceblack();
    io_w8(0x2107, 0x00);
    io_w8(0x2108, 0x04);
    io_w8(0x210B, 0x12);
    io_w8(0x2101, 0x03);
    io_w8(0x212C, 0x13);
    g.charofs = 0xFF00;
    g.printfontset = 0xFF00;
    g.charofs2 = 0;
    g.nextfreechar = g.smallfontchars;
    io_w16(0x2116, (uint16_t)((uint16_t)(g.smallfontchars << 4) + 0x1000 + (g.smallfontchars >> 12 & 1)));   /* (asl's carry) */
    uint16_t d0 = Decompress(SYM_SmallFontTiles, unpackbuf) >> 5;
    g.nextfreechar = (uint16_t)(g.nextfreechar + d0);
    g.Framercset = g.nextfreechar;
    remap(unpackbuf, SYM_PixelRemap8F, d0);
    g.nextfreechar = (uint16_t)(g.nextfreechar + 4);
    g.scorechars = g.nextfreechar;
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
    uint16_t n = Decompress(SYM_TeamBlocksTiles, unpackbuf);
    g.nextfreechar = (uint16_t)(g.nextfreechar + (n >> 5));
    g.ninetilechars = g.nextfreechar;
    uint32_t p0 = unpackbuf, p1;
    DMAToVRAM(&p0, n, 0xFFFF, 0);
    g.ninetilechars = g.nextfreechar;
    g.nextfreechar = (uint16_t)(g.nextfreechar + (0x140 >> 5));
    g.outlinechars = g.nextfreechar;
    p0 = SYM_FramerNineTileTiles;
    DMAToVRAM(&p0, 0x140, 0xFFFF, 0);
    g.outlinechars = g.nextfreechar;
    g.nextfreechar = (uint16_t)(g.nextfreechar + (0x120 >> 5));
    p0 = SYM_FramerOutlineTiles;
    DMAToVRAM(&p0, 0x120, 0xFFFF, 0);
    g.picchars = (uint16_t)(g.nextfreechar - 0x100);
    n = Decompress(SYM_MenuBackTiles, unpackbuf);
    g.backchars = n >> 5;
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x1000, 0);
    Decompress(SYM_MenuBackMap, unpackbuf);
    p0 = unpackbuf;
    p1 = WRAM_PTR(RAM_OFF_logoboxmap + 6);
    PasteMapPart(&p0, &p1, 0x0C00, 0, 0, 7, 1, 1, 7, 7);
    g.logoboxmap[0] = 7;
    g.logoboxmap[1] = 7;
    p0 = unpackbuf;
    p1 = WRAM_PTR(RAM_OFF_textmap2);
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 0, 0, 0x20);
    Decompress(SYM_SetupScreenPalette, unpackbuf);
    SetColours(unpackbuf, 0x30, 0);
    if (g.setupvis >= 0x21) {
        g.setupvis = (uint16_t)(g.setupvis - 0x21);
        n = Decompress(SYM_SetupBackTiles, unpackbuf);
        p0 = unpackbuf;
        DMAToVRAM(&p0, n, 0xFFFF, 0);
        n = Decompress(SYM_SetupBackMap, unpackbuf);
        uint32_t map = unpackbuf + 6;
        for (uint16_t y = (uint16_t)(n - 8); !(y & 0x8000); y = (uint16_t)(y - 2)) {
            uint16_t v = m16(map + y);
            if (v) w16(RAM_OFF_textmap2 + y, (uint16_t)(v + g.backchars + g.charofs2 + 0x0800));
        }
        Decompress(SYM_SetupScreenPalette, unpackbuf);
        SetColours(unpackbuf, 0x10, 0);
        SetColours(unpackbuf + 0x20, 0x10, 0x10);
        Decompress(SYM_SetupBackPalette, unpackbuf);
        SetColours(unpackbuf, 0x10, 0x20);
    }
    io_w8(0x2121, 0);
    io_w8(0x2122, 0);
    io_w8(0x2122, 0);
    Decompress(SYM_MenuBackPalette, unpackbuf);
    SetColours(unpackbuf, 0x20, 0x30);
    g.ninetilechars = (uint16_t)(g.ninetilechars + 0x1400);
    g.outlinechars = (uint16_t)(g.outlinechars + 0x1000);
    SetColours(SYM_SetupSpritePalette, 0x10, 0x80);
    SetColours(SYM_UnusedSetupSpritePalette, 0x10, 0xC0);
    ClearTextMap();
    SendTextMap2();
    WaitFrame();
    InitSprites(0x6000, 0, 0x180);
    SpriteFrameStart();
    LoadPictureMap();
    g.colourbuf[0] = 0x3568;
    uint16_t a2 = r16(RAM_OFF_a2);                        /* (the team offset the caller left in a2, or the real teams') */
    if (g.HomeTeam < 0x18) a2 = 0;
    if (g.VisTeam[0] < 0x18) a2 = 2;
    DrawMatchupPictures(a2);
}

/* Unpack the picture map to picboxmap and add the second font's tile base and palette 3 to each word */
void LoadPictureMap(void)
{
    uint32_t p1 = 0x7E0000 + RAM_OFF_picboxmap;
    Decompress(SYM_PictureMap, p1);
    uint16_t r3 = (uint16_t)(g.picboxmap[0] << 1), x = g.picboxmap[1];
    uint16_t lo = (uint16_t)(p1 + 6);
    do {
        for (uint16_t y = (uint16_t)(r3 - 2); !(y & 0x8000); y -= 2) {
            uint32_t a = (p1 & 0xFF0000) + (uint16_t)lo + y;
            mset16(a, (uint16_t)(m16(a) + g.charofs2 + 0x0C00));
        }
        lo = (uint16_t)(lo + r3);
    } while (--x);
}

void MenuScreenHDMA(void) { start_hdma(0x40, 0xBF, (uint16_t)SYM_MenuScreenHDMA_2, (uint16_t)SYM_MenuScreenHDMA_4); }

/* a2: the team of the controller that owns the pause menu (2 the away team, else 0) */
uint16_t GetPauseTeam(void)
{
    return r16(RAM_OFF_cont1team + g.pausepad) == 2 ? 2 : 0;
}

/* Print string a1 at printx / printy into the stats page buffer (32 wide) through SmallFontMap. A zero
 * character blanks as many cells as X held then: the previous character * 2, or for the first, what the
 * multiply left (0, or 32 on row 0). */
void PrintToStatsPage(uint32_t a1)
{
    uint16_t n = (uint16_t)(m16(a1) - 2);
    uint32_t p0 = (a1 & 0xFF0000) | (uint16_t)(a1 + 2);
    uint16_t base = (uint16_t)((uint16_t)(g.smallfontchars + g.printfontset) | g.printa);
    uint16_t row2 = (uint16_t)(g.printy << 1);
    uint16_t x = row2 ? 0 : 0x20;
    uint32_t s = (uint32_t)(uint16_t)(g.printx << 1) + (uint16_t)Mul16U(row2, 0x20);
    uint32_t p1 = (statspage & 0xFF0000) | (uint16_t)(s + (statspage & 0xFFFF) + (s >> 16));
    uint16_t y = 0;
    do {
        uint16_t c = m8(p0);
        if (!c) {
            while (!((x = (uint16_t)(x - 1)) & 0x8000)) {
                put16(p1 + y, (uint16_t)(0x20 + base));
                y += 2;
            }
            return;
        }
        x = (uint16_t)(c << 1);
        put16(p1 + y, (uint16_t)(rom16(SYM_SmallFontMap + 6 + x) + base));
        y += 2;
        p0 = (p0 & 0xFF0000) | (uint16_t)(p0 + 1);
    } while (--n);
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

/* Print player d0 of team a2's number and name, then at column $19 ($1A out of the pause menu) the
 * attribute d4 through attribjmp (from 3 on the rating of mask d4+2 through CalcAttrib, hot / cold
 * included, r2 its weights) */
void GetNameandAttrib(uint16_t d0, uint16_t d4, uint16_t mask, uint16_t a2, uint16_t r2)
{
    print(getname(a2, d0));
    g.printx = 0x19;
    if (!g.pausesubmenu) g.printx++;
    if (d4 >= 3) {
        uint16_t n;
        g.usehotcold = 1;
        d0 = CalcAttrib(d0, a2, mask, r2, &n);
    }
    uint32_t a0 = (SYM_attribjmp & 0xFF0000) | (uint16_t)(m16(SYM_attribjmp + d4) + (uint16_t)SYM_attribjmp);
    w16(RAM_OFF_jmlvector + 1, (uint16_t)a0);
    w16(RAM_OFF_jmlvector + 2, (uint16_t)(a0 >> 8));
    switch (a0) {
    case SYM_AttribStatus: AttribStatus(d0, a2); break;
    case SYM_AttribEnergy: AttribEnergy(d0, a2); break;
    case SYM_AttribHanded: AttribHanded(d0); break;
    case SYM_AttribWeight: AttribWeight(d0); break;
    case SYM_AttribFighting: AttribFighting(d0); break;
    case SYM_AttribRating: AttribRating(d0); break;
    }
}

/* Roster column: player d0 of team a2's status from tmpdst: a negative word a state (StatusTextTbl
 * 0-3, larger ones 1), bit 12 state 3, otherwise a penalty time and string 4 (5 with bit 14) */
uint32_t AttribStatus(uint16_t d0, uint16_t a2)
{
    uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + (uint16_t)(d0 << 1) + (d0 >> 15));   /* (asl's carry) */
    uint16_t s = r16(RAM_OFF_tmpdst + y);
    if (s & 0x8000) {
        s = (uint16_t)~s;
        if (s >= 4) s = 1;
    } else if (s & 0x1000) s = 3;
    else {
        print(PushTime(s & 0x0FFF));
        s = s & 0x4000 ? 5 : 4;
    }
    return Fprint(s, SYM_StatusTextTbl);
}

/* Roster column: player d0's energy of team a2 as a percentage (energy / $28, $1000 with automatic
 * line changes, at most 100) */
uint32_t AttribEnergy(uint16_t d0, uint16_t a2)
{
    uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + (uint16_t)(d0 << 1) + (d0 >> 15));   /* (asl's carry) */
    uint16_t e = r16(RAM_OFF_tmpde + y);
    if (g.OptLine == 2) e = 0x1000;
    uint16_t q = Div16x8U(e, 0x28);
    if (q >= 0x65) q = 0x64;
    return AttribRating(q);
}

/* Roster column: the fighting rating (the handedness bit dropped) */
uint32_t AttribFighting(uint16_t d0) { return AttribRating(d0 & 0xE); }

/* Roster column tail: d0 as a number of 4 digits, then the text AttribFightingPos; returns a1
 * past the number */
uint32_t AttribRating(uint16_t d0)
{
    uint16_t a;
    uint32_t a1 = print(PushNumberWrap(d0, 4, &a));
    print(SYM_AttribFightingPos);
    return a1;
}

/* Roster column: left or right handed (string 1 - bit 0 of d0 of HandedTextTbl); returns a1 past it */
uint32_t AttribHanded(uint16_t d0) { return Fprint((d0 & 1) ^ 1, SYM_HandedTextTbl); }

/* Roster column: the weight in pounds (8 * rating + $8C), then the text AttribWeightPos */
uint32_t AttribWeight(uint16_t d0)
{
    uint16_t w = (uint16_t)((uint16_t)(d0 << 3) + 0x8C + (d0 >> 13 & 1));   /* (the last asl's carry) */
    uint32_t a1 = print(PushNumber(w));
    print(SYM_AttribWeightPos);
    return a1;
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

/* mesarea = the first initial, a period and the last name of roster player d0 of team a2 (a word written
 * at a time, so a byte past each); a1 = mesarea */
uint32_t PlayerInitialName(uint16_t a2, uint16_t d0)
{
    uint32_t a0 = GetPlayerNamePointer(a2, d0), bank = a0 & 0xFF0000;
    uint16_t p0 = (uint16_t)a0, a1 = RAM_OFF_mesarea;
    w16(a1, 2);
    a1 += 2;
    uint16_t end = (uint16_t)(m16(bank | p0) + p0);
    p0 += 2;
    w16(a1++, m16(bank | p0++));
    w16(a1++, 0x002E);
    while ((m8(bank | p0++)) != 0x20) {}
    do w16(a1++, m16(bank | p0++)); while (p0 != end);
    return FinalizeTextBuffer(a1);
}

/* The players on team a2's roster (records up to the terminator word 2) */
uint16_t GetPlayerCount(uint16_t a2)
{
    uint32_t a0 = team_data(a2), bank = a0 & 0xFF0000;
    a0 = bank | (uint16_t)(m16(a0) + a0);
    uint16_t d0 = 0;
    do {
        d0++;
        uint32_t s = (uint32_t)m16(a0) + (uint16_t)a0;
        a0 = bank | (uint16_t)((s & 0xFFFF) + 8 + (s >> 16));
    } while (m16(a0) != 2);
    return d0;
}

/* (the goalie: right / A / B the next one, after the last none; left the one before, before the first
 * none, from none the last) */
uint16_t Pausemode_go(uint16_t d1)
{
    uint16_t d0 = GetGoalieCount(seta2());
    uint16_t a2 = seta2(), v;
    uint16_t gl = r16(RAM_OFF_tmgoalie + a2);
    if (d1 & 0x8180) {
        if (gl & 0x8000) v = 0;
        else {
            v = (uint16_t)(gl + 1);
            if (v >= d0) v = 0xFFFF;
        }
    } else if (d1 & 0x0200) v = (uint16_t)((gl & 0x8000 ? d0 : gl) - 1);
    else return PauseMenuPoll();
    w16(RAM_OFF_tmgoalie + a2, v);
    setpersonel(a2);
    return PauseMenuPoll();
}

/* Timeout: A / B use one of the team's timeouts (if it has one left), its players rested, "timeout" and
 * the team's name shown a moment */
uint16_t TimeoutMenu(uint16_t d1)
{
    if (!(d1 & 0x8080)) return PauseDefStore();
    uint16_t a2 = seta2();
    uint16_t v = (uint16_t)(r16(RAM_OFF_tmtimeouts + a2) - 1);
    if (!(v & 0x8000)) {
        w16(RAM_OFF_tmtimeouts + a2, v);
        reenergizeteam(a2);
        print(SYM_PauseTimeoutPos);
        Framer(0x16, 7);
        print(SYM_TimeoutTxt);
        uint32_t a1 = GetTeamLongName(r16(RAM_OFF_HomeTeam + a2));
        g.printx = (uint16_t)(0x10 - ((uint16_t)(m16(a1) - 2) >> 1));
        print(a1);
        SendTextMap();
        WaitFrames_Inc(0x96, 0);
        ChoosePauseMenu();
    }
    return PauseMenuPoll();
}

/* Back to the rink after a pause screen: the rink map, its tiles, the centre ice logo, the faceoff, logo and
 * referee tiles, the fonts (remapped), the score and team block tiles to VRAM, the screen registers and the
 * palette; then the horizontal rink, the sprites, and the screen faded in */
void ExitStatsScreen(void)
{
    forcefade();
    g.FrameByNumber = 0xFFFF;
    BuildRinkMap();
    uint16_t n = Decompress(SYM_IceRinkTiles, unpackbuf);
    WaitFadeOut();
    forceblack();
    SpriteFrameStart();
    SpriteFrameEnd();
    setplayercolors();
    io_w8(0x2107, 0x00);
    io_w8(0x2108, 0x05);
    io_w8(0x210B, 0x12);
    io_w8(0x212C, 0x13);
    io_w8(0x2101, 0x03);
    DMAToCGRAM(0x7E0000 + RAM_OFF_palbuffer, 0x40, 0);
    g.charofs = 0xFF00;
    g.charofs2 = 0;
    uint32_t p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x1000, 0);
    uint16_t x = (uint16_t)(g.HomeTeam << 3);
    n = Decompress((uint32_t)rom16(SYM_CenterIceTable + x) | (uint32_t)rom16(SYM_CenterIceTable + 2 + x) << 16, unpackbuf);
    LinearToPlanar(unpackbuf, n, n);
    p0 = (unpackbuf & 0xFF0000) | n;
    uint16_t y = (uint16_t)((uint16_t)(g.centericechars << 4) + 0x1000 + (g.centericechars >> 12 & 1));   /* (asl's carry) */
    DMAToVRAM(&p0, n, y, 0);
    n = Decompress(SYM_FaceOffTiles, unpackbuf);
    p0 = unpackbuf;
    uint16_t r3 = (uint16_t)((uint16_t)(g.faceoffvrcset << 4) + 0x1000 + (g.faceoffvrcset >> 12 & 1));
    DMAToVRAM(&p0, n, r3, 0);
    static const uint32_t tiles[3] = { SYM_RinkLogoTiles, SYM_RefTilesHor, SYM_RefTiles };
    for (int k = 0; k < 3; k++) {
        n = Decompress(tiles[k], unpackbuf);
        p0 = unpackbuf;
        DMAToVRAM(&p0, n, 0xFFFF, r3);
    }
    n = Decompress(SYM_SmallFontTiles, unpackbuf);
    io_w16(0x2116, (uint16_t)((uint16_t)(g.smallfontchars << 4) + 0x1000 + (g.smallfontchars >> 12 & 1)));
    remap(unpackbuf, SYM_PixelRemap83, (uint16_t)(n >> 5));
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
    n = Decompress(SYM_BigFontTiles, unpackbuf);
    g.scorechars = (uint16_t)((n >> 5) + g.BigFontChars + (n >> 4 & 1));   /* (lsr's carry) */
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, r3);
    n = Decompress(SYM_TeamBlocksTiles, unpackbuf);
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, r3);
    uint32_t c = ((uint32_t)(n >> 1) + r3 + (n & 1)) >> 16;   /* (the carry DMAToVRAM's last add leaves) */
    g.reloadtime = (uint16_t)(g.reloadtime + 0x3C + c);
    g.textdirtylo = 0xFFFF;
    g.textdirtyhi = 0;
    SetHor();
    InitSprites(0x6000, 0, 0x01C0);
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    FadeIn();
    ScreenOn();
}
