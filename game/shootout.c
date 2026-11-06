/* shootout: the shootout and its shooter lists (src/routines/shootout). */
#include "game.h"

/* A shootout turn of team a2: a period started for it, the pad that plays its shooter, the penalty shot
 * with this round's shooter from its list (rounds past five from the top again), then the game loop until
 * the turn is over */
void PlayShootout_t1(uint16_t a2)
{
    g.shootoutstop = 0;
    g.shootoutturnover = 0;
    ClearPenaltyBuffer();
    g.gmode = a2 ? 2 : 0;
    StartPer();
    g.BA_Sktr_SCnum = a2 ? 0x000C : 0;
    g.pspad[0] = 0xFFFF;
    for (uint16_t y = 0; y < 0xA; y += 2)
        if (r16(RAM_OFF_padside + y) == a2) { g.pspad[0] = y; break; }
    StartPenaltyShot();
    uint16_t r = g.shootoutround;
    while (!((uint16_t)(r - 6) & 0x8000)) r = (uint16_t)(r - 5);
    if (a2) {
        g.BA_Skater_Offset = (uint16_t)(r8(RAM_OFF_awayshooters + r) - 1);
        g.BA_Sktr_SCnum = 0x000C;
        g.BA_Team = 2;
    } else {
        g.BA_Skater_Offset = (uint16_t)(r8(RAM_OFF_homeshooters + r) - 1);
        g.BA_Sktr_SCnum = 0;
        g.BA_Team = 0;
    }
    Gameloop();
}

/* The shootout (line changes off): both shooter lists, then a turn for each team (visitors first) every
 * round, five rounds and then on until the score differs */
void PlayShootout(void)
{
    uint16_t line = g.OptLine;
    g.OptLine = 0;
    g.shootoutmode = 1;
    InitShooters(0);
    InitShooters(2);
    StartGame_go();
    StartPer();
    Pausemode();
    g.shootoutround = 1;
    do {
        g.shootoutdecider = 0;
        PlayShootout_t1(2);
        if (!((uint16_t)(g.shootoutround - 5) & 0x8000)) g.shootoutdecider = 1;
        PlayShootout_t1(0);
        PlayShootout_ret();
        g.shootoutround++;
    } while (((uint16_t)(g.shootoutround - 6) & 0x8000) || g.tmscore[0] == g.tmscore[1]);
    g.shootoutmode = 0;
    g.OptLine = line;
}

/* "<team> Wins!" once the shootout is decided (a 6-row frame centred on the name); then the whistle
 * (PenNoText; $0A while undecided, outside a shootout the caller's d0) on player o */
void ShootoutWonBy(int o, uint16_t d0)
{
    if (g.shootoutmode) {
        d0 = 0x0A;
        if (g.shootoutdecider && g.tmscore[0] != g.tmscore[1]) {
            uint16_t y = (uint16_t)(g.tmscore[0] - g.tmscore[1]) & 0x8000 ? 2 : 0;
            uint16_t team = r16(RAM_OFF_HomeTeam + y);
            uint32_t a1 = GetTeamLongName(team);
            uint16_t w = (uint16_t)(((uint16_t)((m8(a1) - 2) << 1)) + 2);
            uint16_t r0 = (uint16_t)(0x20 - w) >> 1;
            g.printx = r0;
            g.printy = 0x0D;
            Framer(w, 6);
            g.printx = (uint16_t)(r0 + 1);
            g.printy = 0x0E;
            printbig(a1);
            g.printx = 0x0C;
            g.printy = 0x10;
            printbig(team == 0x0E || team == 0x0D ? SYM_WinMark : SYM_WinsMark);
            d0 = PenNoText;
        }
    }
    AddPenalty2(o, d0);
}

/* Does nothing */
void PlayShootout_ret(void) {}

/* The shootout lineup of the pausing team (when a pad plays it): up / down the row, A / B pick for it,
 * Start leaves (the goalie set from the list) */
void ShootoutShooters(void)
{
    uint16_t a2 = GetPauseTeam();
    uint16_t d0 = a2 ? 2 : 1;
    if (g.cont1team == d0 || g.cont2team == d0) {
        g.setupvis = r16(RAM_OFF_HomeTeam + a2);
        g.setuphome = 0xFFFF;
        SetupScreen();
        ShootersSetup(a2);
        SendColours();
        ShootersBackground(a2);
        g.curitem[0] = 0;
        g.SelectedPlayerIdx = 1;
        g.DispAttribCtr = 0;
        PrintShooterNames(a2);
        SetupBandIRQ(5, 0x50);
        MenuScreenHDMA();
        ScreenOn();
        for (;;) {
            SendTextMap();
            WaitFrame();
            ReadPads();
            Pad p = getpzjoy();
            uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
            if (d1 & 0x0800) {
                if (g.curitem[0]) {
                    g.curitem[0]--;
                    PrintShooterNames(a2);
                }
            } else if (d1 & 0x0400) {
                if ((uint16_t)(g.curitem[0] - 5) & 0x8000) {
                    g.curitem[0]++;
                    PrintShooterNames(a2);
                }
            } else if (d1 & 0x8080) {
                PickShooterRow(a2);
                PrintShooterNames(a2);
            } else if (d1 & 0x1000) break;
        }
    }
    ShootersExit(a2);
}

/* Leave the shooter list: team a2's goalie from its list's first byte */
void ShootersExit(uint16_t a2) { ShootersSetGoalie(a2 ? g.awayshooters[0] : g.homeshooters[0], a2); }

/* Set team (y) goalie from the shooter list's first byte: that byte - 1 */
void ShootersSetGoalie(uint16_t a, uint16_t y) { w16(RAM_OFF_tmgoalie + y, (uint16_t)((a & 0xFF) - 1)); }

/* The shooters screen's top: cleared, the frame outline with "Lineup", the title */
void ShootersSetup(uint16_t a2)
{
    print(SYM_ShootoutShootersPos);
    eraser(0x20, 0x0D, g.blankchar);
    print(SYM_ShootoutShootersPos2);
    g.printa = 0x0800;
    FramerOutline(0x0E, 3);
    print(SYM_LineupTxt);
    ShootersBackground(a2);
}

/* The shooters screen: the top cleared, the frame outline with "Lineup", the background map into text
 * map 2 */
void DrawShootersBack(void)
{
    print(SYM_DrawShootersBackPos);
    eraser(0x20, 0x0D, g.blankchar);
    print(SYM_DrawShootersBackPos2);
    g.printa = 0x0800;
    FramerOutline(0x0E, 3);
    print(SYM_LineupTxt2);
    Decompress(SYM_MenuBackMap, unpackbuf);
    uint32_t p0 = unpackbuf, p1 = 0x7E0000 + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 0, 0, 0x20);
}

/* The shooters screen's title: the background redrawn (drawlinesback, then the band IRQ line back), the
 * label, team a2's block and the pictures */
void ShootersBackground(uint16_t a2)
{
    if (g.drawlinesback) DrawShootersBack();
    SpriteFrameStart();
    PrintLetterSprites(SYM_ShootoutLabel, 0x58, 0x18);
    print(SYM_ShootersBackgroundPos);
    DrawTeamBlocks(a2 ? g.VisTeam[0] : g.HomeTeam, 0, a2);
    DrawMatchupPictures(a2);
    SpriteFrameEnd();
    if (g.drawlinesback) {
        WaitFrame();
        g.irqline2 = 0x50;
        g.drawlinesback = 0;
    }
}

/* Team a2's shootout list: the goalie then the five shooters, a row each with its mark, the cursor row
 * (curitem) highlighted and its player the home matchup picture (for teams from $18 the frame of the
 * direct page's p1 freed); then the title */
void PrintShooterNames(uint16_t a2)
{
    print(SYM_PrintShooterNamesPos);
    eraser(0x20, 0x13, g.blankchar);
    print(SYM_PrintShooterNamesPos2);
    uint16_t d1 = (uint16_t)(GetGoalieCount(a2) - 1);
    uint32_t a5 = SYM_PrintShooterNames_7, a3 = SYM_PrintShooterNames_6;
    if (!((uint16_t)(g.SelectedPlayerIdx - 1) & 0x8000)) {
        a3 = a2 ? RAM_OFF_awayshooters : RAM_OFF_homeshooters;
        a5 = SYM_PrintShooterNames_8;
        d1 = 5;
    }
    do {
        g.printa = 0x3800;
        if ((uint16_t)(5 - d1) == g.curitem[0]) {
            g.printa = 0x2000;
            g.matchuphome = (uint16_t)(m8(a3) - 1);
            if (r16(RAM_OFF_HomeTeam + a2) >= 0x18)
                FreeFrameTiles((uint32_t)r16(RAM_OFF_p1) | (uint32_t)r16(RAM_OFF_p1 + 2) << 16);
        }
        g.printx = 5;
        a5 = print(a5);
        g.printx = 8;
        print(getname(a2, (uint16_t)(m8(a3) - 1)));
        a3 = (a3 & 0xFF0000) | (uint16_t)(a3 + 1);
        g.printy += 2;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
    ShootersBackground(a2);
}

/* Shootout: the 8 bytes of team a2's starters list (team data + its word at +$0C) as its shooter list */
void InitShooters(uint16_t a2)
{
    uint16_t a3 = a2 ? RAM_OFF_awayshooters : RAM_OFF_homeshooters;
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    uint16_t a1 = (uint16_t)(t + m16((bank | t) + 0x0C));
    for (int k = 0; k < 8; k++) w8((uint16_t)(a3 + k), m8(bank | (uint16_t)(a1 + k)));
}

/* Shooter row curitem of team a2 picked: the goalie row steps to the next goalie (after the last the
 * first), a shooter row opens the pick list */
void PickShooterRow(uint16_t a2)
{
    if (g.curitem[0]) {
        ShooterSelectList(a2);
        return;
    }
    uint16_t d0 = GetGoalieCount(a2);
    uint16_t a1 = a2 ? RAM_OFF_awayshooters : RAM_OFF_homeshooters;
    uint16_t v = (uint16_t)(r8(a1) + 1);
    if (v != d0 && !((uint16_t)(v - d0) & 0x8000)) v = 1;
    w8(a1, (uint8_t)v);
}

/* Pick a player for shooter row curitem of team a2 from the skaters, nine at a time (up / down move the
 * cursor and scroll, left / right the attribute shown); A / B take him (SetShooter), Start cancels */
void ShooterSelectList(uint16_t a2)
{
    SpriteFrameStart();
    print(SYM_ShooterSelectListPos);
    eraser(0x20, 0x0D, g.blankchar);
    Decompress(SYM_PlayerListMap, unpackbuf);
    uint32_t p0 = unpackbuf, p1 = 0x7E0000 + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 1, 1, 0x20);   /* (r7: $20, as the screen's last paste left it) */
    SendTextMapRows();
    SendTextMap2Rows();
    SpriteFrameEnd();
    WaitFrame();
    g.irqline2 = 7;
    g.listcount[0] = GetPlayerCount(a2);
    uint16_t d0 = GetGoalieCount(a2);
    g.listcount[0] = (uint16_t)(g.listcount[0] - d0);
    g.pickfirst[0] = d0;
    g.picktop[0] = d0;
    uint16_t a3 = (uint16_t)((a2 ? RAM_OFF_awayshooters : RAM_OFF_homeshooters) + g.curitem[0]);
    g.curplayer[0] = (uint16_t)(r8(a3) - 1);
    uint16_t v = (uint16_t)(8 + g.picktop[0] - g.curplayer[0]);
    if (v & 0x8000) {
        uint16_t w = (uint16_t)(g.curplayer[0] - 8 - g.picktop[0]);
        if (!(w & 0x8000)) g.picktop[0] = (uint16_t)(w + g.picktop[0]);
        v = 0;
    }
    g.curplayer[0] = v;
    PrintShooterList(a2);
    for (;;) {
        SendTextMap();
        SendTextMap2();
        WaitFrame();
        ReadPads();
        Pad p = getpzjoy();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x1000) break;
        if (d1 & 0x0800) {
            v = (uint16_t)(g.curplayer[0] + 1);
            if ((uint16_t)(v - 9) & 0x8000) g.curplayer[0] = v;
            else if (!((uint16_t)(g.picktop[0] - 1 - g.pickfirst[0]) & 0x8000)) g.picktop[0]--;
            else continue;
        } else if (d1 & 0x0400) {
            v = (uint16_t)(g.curplayer[0] - 1);
            if (!(v & 0x8000)) g.curplayer[0] = v;
            else if ((uint16_t)(g.picktop[0] - g.pickfirst[0] + 9 - g.listcount[0]) & 0x8000) g.picktop[0]++;
            else continue;
        } else if (d1 & 0x0100) {
            v = (uint16_t)(g.DispAttribCtr + 1);
            if (!((uint16_t)(v - 0x10) & 0x8000)) continue;
            g.DispAttribCtr = v;
        } else if (d1 & 0x0200) {
            v = (uint16_t)(g.DispAttribCtr - 1);
            if (v & 0x8000) continue;
            g.DispAttribCtr = v;
        } else if (d1 & 0x8080) {
            SetShooter(a2);
            break;
        } else continue;
        PrintShooterList(a2);
    }
    g.drawlinesback = 1;
}

/* The shooter pick list: the attribute column's title (DispAttribCtr, kept inside the columns), the scroll
 * arrows, the rows */
void PrintShooterList(uint16_t a2)
{
    print(SYM_PrintShooterListPos);
    eraser(0x20, 0x0D, g.blankchar);
    g.printx = 0x11;
    g.printy = 2;
    uint32_t a1;
    for (;;) {
        a1 = SYM_ShooterAttribColumns;
        uint16_t d0 = g.DispAttribCtr;
        while (!(m16(a1) & 0x8000) && !((d0 = (uint16_t)(d0 - 1)) & 0x8000)) {
            uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
            a1 = (a1 & 0xFF0000) | (uint16_t)(s + 4 + (s >> 16));   /* (the first add's carry) */
        }
        if (!(m16(a1) & 0x8000)) break;
        g.DispAttribCtr--;
    }
    g.printa = 0x3800;
    a1 = print(a1);
    uint16_t mask = m16(a1), d4 = m16(a1 + 2);
    print(SYM_PlayerTxt2);
    if (g.picktop[0] != g.pickfirst[0]) print(SYM_LeftArrowTxt3);
    if ((uint16_t)(g.picktop[0] - g.pickfirst[0] + 9 - g.listcount[0]) & 0x8000) print(SYM_RightArrowTxt3);
    PrintShooterRows(d4, mask, a2);
}

/* The pick list's rows: nine players from picktop + 1 (eight from line 6), the cursor's highlighted, each
 * with attribute d4 of mask (goalies' from the goalie list) */
void PrintShooterRows(uint16_t d4, uint16_t mask, uint16_t a2)
{
    print(SYM_PrintShooterRowsPos);
    uint16_t a3 = (uint16_t)(g.picktop[0] + 1);
    uint16_t d1 = 8;
    if ((int16_t)g.SelectedPlayerIdx > 5) d1 = 7;
    do {
        g.printa = 0x3800;
        if (d1 == g.curplayer[0]) g.printa = 0x2000;
        g.printx = 2;
        uint16_t r1 = (uint16_t)((a3 & 0xFF) - 1), r2 = 0;
        if ((uint16_t)(r1 - GetGoalieCount(a2)) & 0x8000) r2 = 0x10;
        GetNameandAttrib(r1, d4, mask, a2, r2);
        a3++;
        g.printy++;
    } while (!((d1 = (uint16_t)(d1 - 1)) & 0x8000));
    g.printa = 0x2000;
}

/* Put the player at the cursor in shooter row curitem of team a2's list; if he is already in rows 1-6, the
 * player the row had takes his place */
void SetShooter(uint16_t a2)
{
    uint8_t d0 = (uint8_t)(9 - g.curplayer[0] + g.picktop[0]);
    uint32_t a1 = a2 ? RAM_OFF_awayshooters : RAM_OFF_homeshooters;
    uint8_t d1 = r8(a1 + g.curitem[0]);
    for (uint16_t y = 1; (uint16_t)(y - 7) & 0x8000; y++)
        if (r8(a1 + y) == d0) { w8(a1 + y, d1); break; }
    w8(a1 + g.curitem[0], d0);
}
