/* hockey: the game loop, the pause menus, the stats screens and the title (src/routines/hockey). */
#include "game.h"

/* a byte or word written at a 24-bit address (only WRAM takes it) */
void put8(uint32_t a, uint8_t v) { if (is_wram(a)) w8(wram_off(a), v); }
void put16(uint32_t a, uint16_t v) { put8(a, (uint8_t)v); put8(a + 1, (uint8_t)(v >> 8)); }

/* Do nothing */
void CheckLateGameSong(void) {}
void UpdateCwdExcite(void) {}

/* Start a pause (sflags sfpz) */
void startpause(void) { g.sflags |= sfpz; }

/* The pause menu's resume: A = 0 */
uint16_t PauseResumeRtl(void) { return 0; }

/* The end of a pause submenu: the whole text map redrawn, A = 1 */
uint16_t PauseSubmenuDone(void)
{
    g.textdirtylo = 0;
    g.textdirtyhi = 0x03FF;
    return 1;
}

/* The period length in seconds for the period length option (d0) */
uint16_t GetPeriodTime(void) { return rom16(SYM_GetPeriodTime_timetab + (uint16_t)(g.OptPerlen << 1)); }

/* The title's busy-wait (no RAM changes) */

/* Put both teams' players on the bench (tmap 6) */
void restoreteams(void)
{
    for (uint16_t a2 = 0; a2 <= 2; a2 += 2) {
        w16(RAM_OFF_tmap + a2, 6);
        for (int d0 = 0x32; d0 >= 0; d0 -= 2) g.tmpdst[((a2 ? 0x34 : 0) + d0) >> 1] = TMPDBENCH;
    }
}

/* Every game loop: the crowd's hold counts down (at most 60), then its level (at most $480) falls by
 * impactstep, to 0 */
void updatecrowdf(void)
{
    if (g.crowdhold) {
        uint16_t h = g.crowdhold < 0x3C ? g.crowdhold : 0x3C;
        h = (uint16_t)(h - g.d7);
        g.crowdhold = h & 0x8000 ? 0 : h;
        return;
    }
    uint16_t c = g.Crowdlevel < 0x480 ? g.Crowdlevel : 0x480;
    c = (uint16_t)(c - g.impactstep);
    g.Crowdlevel = c & 0x8000 ? 0 : c;
}

/* The team of the controller that paused (a2: 0 home, 2 visitors); a negative pausepad is minus the side
 * whose first controller counts */
uint16_t seta2(void)
{
    uint16_t x = g.pausepad;
    if (x & 0x8000) {
        uint16_t side = (uint16_t)-x;
        for (x = 0; x < 0x0A; x += 2)
            if (r16(RAM_OFF_cont1team + x) == side) break;
        if (x >= 0x0A) return 0;
    }
    return r16(RAM_OFF_cont1team + x) == 1 ? 0 : 2;
}

/* The longer pause menu when this side still has a timeout (clock stopped, not the intermission), else the
 * standard one; the first item shown kept inside it */
void ChoosePauseMenu(void)
{
    uint16_t x = g.pausepad;
    if (!(x & 0x8000) && g.gmclock && !g.InIntermission && r16(RAM_OFF_tmtimeouts + (r16(RAM_OFF_cont1team + x) & 2))) {
        g.pmenucount = 0x11;
        g.pmenulist = 5;
    } else {
        g.pmenucount = 0x10;
        g.pmenulist = 0;
    }
    if ((uint16_t)(g.psfirst + 5) >= g.pmenucount) g.psfirst--;
}

/* With line changes on, the benched players of both teams get 10 energy back, up to $1000 */
void RestoreBenchEnergy(void)
{
    if (!g.OptLine) return;
    for (int a2 = 0; a2 <= 2; a2 += 2)
        for (int d0 = 0x32; d0 >= 0; d0 -= 2) {
            int i = ((a2 ? 0x34 : 0) + d0) >> 1;
            if (g.tmpdst[i] != 0xFFFE) continue;
            uint16_t e = (uint16_t)(g.tmpde[i] + 0x0A);
            g.tmpde[i] = (uint16_t)(e - 0x1000) & 0x8000 ? e : 0x1000;
        }
}

/* The window and COLDATA HDMA (channels 2 and 3): window 1 from left to right, the tables in bank $9D */
void start_hdma(uint8_t left, uint8_t right, uint16_t wtab, uint16_t ctab)
{
    io_w8(0x2125, 0xA0);   /* WOBJSEL */
    io_w8(0x2126, left);   /* WH0 */
    io_w8(0x2127, right);  /* WH1 */
    io_w8(0x2130, 0x10);   /* CGWSEL */
    io_w8(0x2131, 0x02);   /* CGADSUB */
    io_w8(0x2132, 0xE5);   /* COLDATA */
    io_w8(0x4320, 0x44);   /* DMAP2 */
    io_w8(0x4324, 0x9D);   /* A1B2 */
    io_w8(0x4327, 0x9D);   /* DASB2 */
    io_w16(0x4322, wtab);  /* A1T2 */
    io_w8(0x4321, 0x26);   /* BBAD2 */
    io_w8(0x4330, 0x40);   /* DMAP3 */
    io_w8(0x4334, 0x9D);   /* A1B3 */
    io_w8(0x4337, 0x9D);   /* DASB3 */
    io_w16(0x4332, ctab);  /* A1T3 */
    io_w8(0x4331, 0x32);   /* BBAD3 */
    io_w8(0x420C, 0x0C);   /* HDMAEN */
}

/* Pause for pad a / from controller 1 */
void startpause_pad(uint16_t a) { g.pausepad = a; startpause(); }
void startpause1(void) { g.pausepad = 0; startpause(); }

/* Set the period length (a shootout $19, overtime of a regular season game 300) and stop the clock */
void ResetClock(void)
{
    uint16_t d0 = GetPeriodTime();
    if (g.shootoutmode) d0 = 0x19;
    else if (!((uint16_t)(g.gsp - 3) & 0x8000) && !g.OptPlayMode) d0 = 0x012C;
    g.gameclock[0] = d0;
    g.PerTimeTotal = d0;
    g.gmclock = d0;
}

/* Erase the braces at stats page rows top and bottom (column x) */
void clear_braces(uint16_t x, uint16_t top, uint16_t bottom)
{
    g.printx = x;
    g.printy = top;
    g.printa = 0x2000;
    PrintToStatsPage(SYM_LineBraces + 6);
    g.printx = x;
    g.printy = bottom;
    g.printa = 0x2000;
    PrintToStatsPage(SYM_LineBraces + 6);
}

/* Append string a4's characters from y + 1 to the pause item text in mesarea (a word at a time, its length
 * byte counted one up each) */
void PmenuAppend(uint32_t a4, uint16_t y)
{
    while ((y = (uint16_t)(y + 1)) != m16(a4)) {
        uint16_t v = m16(a4 + y);
        uint16_t at = (uint16_t)(RAM_OFF_mesarea + g.mesarea[0]);
        if (at < 0x2000) w8(at, (uint8_t)v);            /* (the data bank's WRAM) */
        if ((uint16_t)(at + 1) < 0x2000) w8((uint16_t)(at + 1), (uint8_t)(v >> 8));
        g.mesarea[0]++;
    }
}

/* Append "None" to the pause item text */
void PmenuNone(void) { PmenuAppend(SYM_GoalieOptionText + 33, 1); }

/* Append a pause item's value (the goalie or defense control setting of the side that paused) when item
 * d0 is the one PmenuValueItems names for this menu */
static void pmenu_value(uint16_t d0, uint16_t list, uint32_t flag, uint16_t on, uint16_t off)
{
    uint32_t a3 = (SYM_PmenuValueItems & 0xFF0000) | (uint16_t)(SYM_PmenuValueItems + list + (uint16_t)(g.pmenulist << 1));
    if (m16(a3) != d0) return;
    uint16_t a2 = seta2();
    PmenuAppend(SYM_GoalieOptionText + (r16(flag + a2) ? on : off), 1);
}
void PmenuGoalieValue(uint16_t d0) { pmenu_value(d0, 12, RAM_OFF_goaliemode1, 18, 0); }
void PmenuDefValue(uint16_t d0) { pmenu_value(d0, 24, RAM_OFF_defcontrol, 39, 43); }

/* The MultiPlayer 5 warning lines: print the line's position, then string a1 centred */
uint32_t PrintMP5Line1(uint32_t a1) { print(SYM_PrintMP5Line1Pos); return PrintCentered(a1); }
uint32_t PrintMP5Line2(uint32_t a1) { print(SYM_PrintMP5Line2Pos); return PrintCentered(a1); }
uint32_t PrintMP5Line3(uint32_t a1) { print(SYM_PrintMP5Line3Pos); return PrintCentered(a1); }
uint32_t PrintMP5Line4(uint32_t a1) { print(SYM_PrintMP5Line4Pos); return PrintCentered(a1); }

/* Draw the exit question (game or shootout) and the No / Yes lines, the one at curitem highlighted */
void DrawExitPrompt(void)
{
    print(SYM_DrawExitPromptPos);
    Framer(0x16, 7);
    for (uint16_t y = 0x0D; y < 0x12; y++) {
        g.printa = 0x2400;
        g.printx = 6;
        g.printy = y;
        print(SYM_ExitPromptClear);
    }
    print(SYM_DrawExitPromptPos2);
    g.printa = 0x2400;
    print(g.shootoutmode ? SYM_ExitShootoutAsk : SYM_ExitGameAsk);
    print(SYM_DrawExitPromptPos3);
    g.printa = g.curitem[0] ? 0x2400 : 0x2000;
    print(SYM_ExitNo);
    print(SYM_DrawExitPromptPos4);
    g.printa = g.curitem[0] ? 0x2000 : 0x2400;
    print(SYM_ExitYes);
}

/* SetupPauseScreen's goalie item: when item d0 is the menu's goalie item, the side's goalie's last name (or None) */
static void pmenu_goalie(uint16_t d0)
{
    uint32_t a3 = (SYM_PmenuValueItems & 0xFF0000) | (uint16_t)(SYM_PmenuValueItems + (uint16_t)(g.pmenulist << 1));
    if (m16(a3) != d0) return;
    uint16_t a2 = seta2();
    uint16_t tmp0 = r16(RAM_OFF_tmgoalie + a2);
    if (tmp0 & 0x8000) { PmenuNone(); return; }
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a4 = r16(RAM_OFF_tmdata + a2);
    a4 = (uint16_t)(a4 + m16(bank | a4));
    while (!((tmp0 = (uint16_t)(tmp0 - 1)) & 0x8000)) {
        uint32_t s = (uint32_t)m16(bank | a4) + a4;
        a4 = (uint16_t)(s + 8 + (s >> 16));   /* (the first add's carry) */
    }
    uint16_t y = 1;
    do y++; while (m8((bank | a4) + y) != 0x20);
    PmenuAppend(bank | a4, y);
}

/* Draw the pause menu: five items of the current list from psfirst, the selected one (psitem)
 * highlighted, each with its value, centred on column $11 */
void SetupPauseScreen(void)
{
    print(SYM_PmenuPos);
    uint16_t d0 = g.psfirst, off;
    switch (g.pmenulist) {
    case 0: off = 0; break;
    case 1: off = 429; break;
    case 2: off = 626; break;
    case 3: off = 732; break;
    case 4: off = 808; break;
    default: off = 210;
    }
    uint32_t bank = SYM_PauseMenuItems & 0xFF0000;
    uint16_t a0 = (uint16_t)(SYM_PauseMenuItems + off);
    for (uint16_t x = d0; !((x = (uint16_t)(x - 1)) & 0x8000);) a0 = (uint16_t)(a0 + m16(bank | a0));
    do {
        g.printa = d0 == g.psitem ? 0x2000 : 0x2400;
        g.printx = 6;
        print(SYM_PmenuPos2);
        for (int y = 0x1E; y >= 0; y -= 2) w16(RAM_OFF_mesarea + y, m16((bank | a0) + y));
        pmenu_goalie(d0);
        PmenuGoalieValue(d0);
        PmenuDefValue(d0);
        g.printx = (uint16_t)(0x11 - (g.mesarea[0] >> 1));
        print(RAM_OFF_mesarea);
        g.printy++;
        a0 = (uint16_t)(a0 + m16(bank | a0));
        d0++;
    } while ((uint16_t)(d0 - g.psfirst) != 5);
}


/* The end of a period: the puck's whistle assignment and the horn; from the third period on, unless tied,
 * the skaters of the team ahead celebrate (the first of them with assignment 9 and every pad flag cleared
 * after a won Stanley Cup final in a playoff mode), the game is over: the crowd, the end of game message
 * (and the cup's). Tied, or before the third period: the shootout's whistle (ShootoutWonBy), unless an
 * overtime of a regular game ends the game. (In a final not yet won the team ahead's skaters and the
 * pad flags are taken from gamenum * 2 on, and the messages' player is wherever that count ended.) */
void clockcont_0(void)
{
    uint16_t x = 0x1C, d0 = 0x18;
    int p0 = 0;
    assinsert(14, 0x18);
    StreamSfx(1);
    if (g.gsp < 2) goto tied;
    d0 = 8;
    x = 0;
    if (g.playoffRoundIdx == 3) {
        int won = g.bosgames == 7;
        if (!won) {
            uint16_t a0 = (uint16_t)(g.gamenum << 1);
            x = a0;
            uint16_t d3 = r16(RAM_OFF_gsflags + x) & 1 ? 0x10 : 0;
            if ((uint16_t)(g.tmscore[0] - g.tmscore[1]) & 0x8000) d3 ^= 0x10;
            won = r16(RAM_OFF_gspotwins + (uint16_t)(a0 + d3)) == 3;
        }
        if (won && g.OptPlayMode) {
            p0 = 1;
            d0 = 9;
            for (int k = 0; k < 12; k++, x += 2) w16(RAM_OFF_pflags + x, r16(RAM_OFF_pflags + x) & 0xFFF7);
            x = 0;
        }
    }
    {
        uint16_t d = (uint16_t)(g.tmscore[0] - g.tmscore[1]);
        if (!d) goto tied;
        if (d & 0x8000) x = 0x0C;
        for (int k = 0; k < 6; k++, x += 2) {
            if ((int16_t)r16(RAM_OFF_position + x) <= 0) continue;
            assinsert(x >> 1, (uint8_t)d0);
            d0 = 8;
        }
    }
over:
    ClearPenaltyBuffer();
    g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x03E8);
    g.crowdhold = 0x3C;
    g.gmclock = 0x40;
    g.gmode |= 0x40;
    if (!((uint16_t)(g.tmscore[0] - g.tmscore[1]) & 0x8000)) {
        g.Crowdlevel = 0;
        g.cheercnt = 3;
        g.cheervc = (uint16_t)(g.vcount[0] - 0x96);
    }
    AddPenalty2(x >> 1, PenEOG);
    if (p0) AddPenalty2(x >> 1, PenNoText3);
    return;
tied:
    if (g.gsp == 3 && !g.OptPlayMode) goto over;
    EndPenaltyShotPlay(x >> 1);
    ShootoutWonBy(x >> 1, 2);
}

/* Once a game loop: when the clock has run out, freeze the view and end the period */
void clockcont(void)
{
    if (g.gmclock | g.gameclock[0]) return;
    freezewindow();
    clockcont_0();
}


/* Once a second: the count down to the next injury, its box at 0 */
void CheckInjury(void)
{
    if (--g.InjCntDown) return;
    ShowInjuryBox();
}

/* Every game loop: the penalties, the crowd, the sound and the clock; once a second of play (not on the
 * horizontal rink: lldisp counts 24 frames) the goalies, the bench energy, the injuries, the power play */
void periodicevents(void)
{
    PenaltyManager();
    updatecrowdf();
    updatesound();
    clockcont();
    if (g.sfhormode) return;
    g.lldisp = (uint16_t)(g.lldisp - g.d7);
    if (!(g.lldisp & 0x8000)) return;
    g.lldisp = (uint16_t)(g.lldisp + 0x18);
    ChkGoalies();
    UpdateCwdExcite();
    CheckLateGameSong();
    RestoreBenchEnergy();
    CheckInjury();
    updatepwrplay();
}


/* One frame of game logic */
void DoGameFrame(void)
{
    SpriteFrameStart();
    UpdateBenchCheer();
    periodicevents();
    updateplayers();
    checkwindow();
    updatereplay();
    setvideo();
    SpriteFrameEnd();
}

/* Pulse the colour of the pausing pad's side (colourbuf+$1DE or +$1FE), the other side white; both white
 * in a demo or with no side */
void PulsePauseSide(void)
{
    uint16_t x = g.pausepad, side = 0;
    if (g.notdemo && !(x & 0x8000)) side = r16(RAM_OFF_cont1team + x);
    if (!side) {
        w16(RAM_OFF_colourbuf + 0x1DE, 0xFFFF);
        w16(RAM_OFF_colourbuf + 0x1FE, 0xFFFF);
        return;
    }
    if (side == 1) {
        w16(RAM_OFF_colourbuf + 0x1DE, 0xFFFF);
        PulseColour((uint16_t)(RAM_OFF_colourbuf + 0x1FE), 0x7F);
    } else {
        w16(RAM_OFF_colourbuf + 0x1FE, 0xFFFF);
        PulseColour((uint16_t)(RAM_OFF_colourbuf + 0x1DE), 0x7F);
    }
}

/* The MultiPlayer 5 warning box for warning bits a (1 the multitap, 2 the Super Scope, 4 the mouse) */
static void mp5_box(uint16_t a)
{
    print(SYM_ShowMP5WarningPos);
    Framer(0x16, 7);
    if (a & 1) {
        PrintMP5Line1(SYM_MultiPlayer5Text);
        PrintMP5Line2(SYM_MP5MustConnect);
        PrintMP5Line3(SYM_MP5ToController);
        PrintMP5Line4(SYM_MP5Socket2);
    } else if (a & 2) {
        PrintMP5Line1(SYM_MP5ScopeHead);
        PrintMP5Line2(SYM_MP5ScopeNot);
        PrintMP5Line3(SYM_MP5ScopeWith);
        PrintMP5Line4(SYM_MP5SuperScope);
    } else if (a & 4) {
        PrintMP5Line1(SYM_MP5MouseHead);
        PrintMP5Line2(SYM_MP5MouseNot);
        PrintMP5Line3(SYM_MP5MouseWith);
        PrintMP5Line4(SYM_MP5Mouse);
    }
    SendTextMap();
    WaitFrame();
}

/* Show the MultiPlayer 5 warning while there is one, then the pause menu again */
void ShowMP5Warning(void)
{
    if (!g.mp5warning) return;
    do mp5_box(g.mp5warning); while (g.mp5warning);
    SetupPauseScreen();
}

/* While the pause pad holds A or B: pulse its side's colour and send it to CGRAM each frame */
void PulseUntilRelease(void)
{
    do {
        PulsePauseSide();
        WaitFrame();
        DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf + 0x1C0, 0x20, 0xE0);
    } while (r16(RAM_OFF_JoyPad + g.pausepad) & 0x8080);
}

/* The frame step sizes from the frames elapsed d7: timestep, impactstep (x2), velstep (x16), gravstep (x96) */
void frame_steps(uint16_t d7)
{
    g.d7 = d7;
    g.timestep = d7;
    g.impactstep = (uint16_t)(d7 << 1);
    g.velstep = (uint16_t)(d7 << 4);
    g.gravstep = (uint16_t)(d7 << 5);
    g.gravstep = (uint16_t)((uint16_t)(d7 << 6) + g.gravstep + (g.gravstep >> 15));   /* (asl's carry) */
}

/* Ask "exit game?": up / down choose (curitem 0 or 1), A / B take it, Start cancels (0); returns it */
uint16_t AskExitGame(void)
{
    g.curitem[0] = 0;
    for (;;) {
        DrawExitPrompt();
        SendTextMap();
        WaitFrame();
        ReadPads();
        uint16_t d1 = getpzjoy().press;
        if (d1 & 0x1000) { g.curitem[0] = 0; break; }
        if (d1 & 0x8080) break;
        if (d1 & 0x0800) {
            uint16_t v = (uint16_t)(g.curitem[0] - 1);
            if (!(v & 0x8000)) g.curitem[0] = v;
        } else if (d1 & 0x0400) {
            uint16_t v = (uint16_t)(g.curitem[0] + 1);
            if ((uint16_t)(v - 2) & 0x8000) g.curitem[0] = v;
        }
    }
    WaitButtonRelease();
    return g.curitem[0];
}

/* In the demo: Start pauses, any other button ends it (Opening2); in mode 4 with pads on teams, Start on
 * any of them pauses from that pad */
static void demo_read(void)
{
    uint16_t any = g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0];
    if (g.OptPlayMode == 4 && any) {
        for (uint16_t x = 0; x < 0x0A; x += 2) {
            if (!r16(RAM_OFF_cont1team + x)) continue;
            if (ReadjoyA(x >> 1).held & 0x1000) { startpause_pad(x); return; }
        }
        return;
    }
    if (any) return;
    uint16_t d0, d1, d2, d3;
    Readjoy1(&d0, &d1, &d2, &d3);
    if (d1 & 0x1000) { startpause1(); return; }
    if (!d1) return;
    g.fadestep = 0;
    g.FrameByNumber = 0;
    Opening2();
}
/* The game loop's pad check after each frame: in the demo a button pauses or ends it (demo_read) */
void demoread(void)
{
    demo_read();
}

/* The pause controller: the pad that has it (a negative pausepad: the first pad of that side to press a
 * button takes it, the menu rebuilt), its buttons with repeat in *d1 / *d3; it is given up after a while
 * idle; in the intermission's demo the pause times out (Start). Returns once a button is down. */
void SelectPauseController(uint16_t *d1, uint16_t *d3)
{
    for (;;) {
        ShowMP5Warning();
        PulsePauseSide();
        if (Intermission_tick()) goto timeout;
        SpriteFrameStart();
        setvideo();
        WaitFrame();
        SpriteFrameEnd();
        ReadPads();
        uint16_t a = g.pausepad;
        if (a & 0x8000) {
            uint16_t k;
            for (k = 0; k < 5; k++) {
                uint16_t kx = (uint16_t)(k << 1);
                if (r16(RAM_OFF_JoyPad + kx) & 0x000F) continue;
                if (!r16(RAM_OFF_cont1team + kx)) continue;
                Pad p = ReadjoyA(k);
                *d1 = p.press;
                *d3 = p.held;
                if (p.held) break;
            }
            if (k == 5) goto idle;
            uint16_t x = (uint16_t)(k << 1);
            if ((uint16_t)-r16(RAM_OFF_cont1team + x) != g.pausepad) {
                g.pausepad = x;
                g.psitem = 0;
                g.psfirst = 0;
                SetupPauseScreen();
                SendTextMap();
                PulseUntilRelease();
                g.pausepadidle = 0x3C;
                g.repeatdelay = 0x14;
                if (!g.pmenulist || g.pmenulist == 5) ChoosePauseMenu();
                return;
            }
            g.pausepad = x;
            g.pausepadidle = 0x3C;
            a = x;
        }
        {
            Pad p = ReadjoyA(a >> 1);
            g.repeatdelay -= 2;
            *d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
            *d3 = p.held;
            if (p.held) g.pausepadidle = 0x3C;
            else if (g.pausepadidle) {
                uint16_t v = (uint16_t)(g.pausepadidle - 3);
                g.pausepadidle = v;
                if (!v || (v & 0x8000)) {
                    g.pausepadidle = 0;
                    g.pausepad = (uint16_t)-r16(RAM_OFF_cont1team + g.pausepad);
                    return;
                }
            }
        }
    idle:
        if (g.demoflag && !(g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0]) && !--g.pausetimer) goto timeout;
        goto pressed;
    timeout:
        *d3 |= 0x1000;
    pressed:
        if (!(*d1 | *d3)) continue;
        if (g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0]) g.pausetimer = 0xEA60;
        return;
    }
}

/* The pause menu handlers (the item chosen, with the pad's new presses d1); A: 0 leaves the menu */
uint16_t PauseResume(uint16_t d1)
{
    if (d1 & 0x9080) return PauseResumeRtl();
    return PauseMenuPoll();
}
uint16_t PauseDefStore(void) { return PauseMenuPoll(); }
/* Defense control: A / B / left / right turn it on or off for the pause controller's team */
uint16_t PauseDefControl(uint16_t d1)
{
    if (d1 & 0x8380) {
        uint16_t a2 = seta2();
        w16(RAM_OFF_defcontrol + a2, r16(RAM_OFF_defcontrol + a2) ^ 1);
    }
    return PauseDefStore();
}
/* Exit game: A / B ask to be sure (AskExitGame), then the game is abandoned for the option screens
 * (Opening2) */
uint16_t PauseExitGame(uint16_t d1)
{
    if (!(d1 & 0x8080) || !AskExitGame()) return PauseMenuPoll();
    g.FrameByNumber = 0;
    Opening2();
    return 0;
}
/* Instant replay: A / B fade out to the replay (ReplayMode, the music noted in replaymusic) and back,
 * the frame count kept */
uint16_t PauseReplay(uint16_t d1)
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    g.replaymusic = g.MusicOn;
    forcefade();
    FadeOutSound();
    WaitFadeOut();
    forceblack();
    uint16_t d7 = g.d7;
    g.vcountmark[1] = (uint16_t)(g.vcount[1] - 2);
    ReplayMode();
    FadeIn();
    ScreenOn();
    g.d7 = d7;
    g.timestep = d7;
    return PauseSubmenuDone();
}

/* The handler at addr (jmlvector) */
static uint16_t pause_handler(uint32_t addr, uint16_t d1)
{
    switch (addr) {
    case SYM_PauseResume: return PauseResume(d1);
    case SYM_PauseReplay: return PauseReplay(d1);
    case SYM_PauseDefControl: return PauseDefControl(d1);
    case SYM_ManualGoalieMenu: return ManualGoalieMenu(d1);
    case SYM_Pausemode_go: return Pausemode_go(d1);
    case SYM_TimeoutMenu: return TimeoutMenu(d1);
    case SYM_PauseExitGame: return PauseExitGame(d1);
    case SYM_PauseGameStats: return PauseGameStats(d1);
    case SYM_PausePenaltySummary: return PausePenaltySummary(d1);
    case SYM_PauseScoringSummary: return PauseScoringSummary(d1);
    case SYM_PauseOtherScores: return PauseOtherScores(d1);
    case SYM_PauseTeamRoster: return PauseTeamRoster(d1);
    case SYM_PausePeriodStats: return PausePeriodStats(d1);
    case SYM_PausePlayerStats: return PausePlayerStats(d1);
    case SYM_PausePlayerCards: return PausePlayerCards(d1);
    case SYM_PauseCrowdLevel: return PauseCrowdLevel(d1);
    case SYM_PauseEditLines: return PauseEditLines(d1);
    case SYM_PauseShootoutSetup: return PauseShootoutSetup(d1);
    }
    if (call_long_other) call_long_other(addr);
    return 0;
}

/* The pause menu: the items shown, the pause controller's buttons (Start resumes), up / down move the
 * item (five shown from psfirst), another button runs it (its handler from PauseMenuHandlers through the
 * RAM trampoline, by menu list) */
uint16_t PauseMenuPoll(void)
{
    uint16_t d1 = 0, d3 = 0;
    for (;;) {
        SetupPauseScreen();
        SelectPauseController(&d1, &d3);
        if (d3 & 0x1000) return PauseResumeRtl();
        if (d1 & 0x8080) break;
        if (d1 & 0x0400) {
            g.psitem++;
            if (g.pmenucount == g.psitem) g.psitem--;
            else if (!((uint16_t)((uint16_t)(g.psitem - 5) - g.psfirst) & 0x8000)) g.psfirst++;
            continue;
        }
        if (!(d1 & 0x0800)) break;
        g.psitem--;
        if (!((uint16_t)(g.psitem - g.psfirst) & 0x8000)) continue;
        g.psfirst--;
        if (!(g.psfirst & 0x8000)) continue;
        g.psfirst = 0;
        g.psitem = 0;
    }
    static const uint16_t base[6] = { 0, 132, 192, 228, 252, 64 };
    uint16_t l = g.pmenulist;
    uint32_t e = SYM_PauseMenuHandlers + base[l < 5 ? l : 5] + (uint16_t)(g.psitem << 2);
    w16(RAM_OFF_jmlvector + 1, m16(e));
    w16(RAM_OFF_jmlvector + 2, m16(e + 1));
    return pause_handler(m16(e) | (uint32_t)m8(e + 2) << 16, d1);
}

/* Pick the pause menu (a shootout: 4, or 3 before the game; the overtime period: 1; before the game: 2;
 * else ChoosePauseMenu's), a frame of sprites, both sides' colours white, then the menu */
uint16_t Pausemode_menu(void)
{
    g.pausemenuon = 0xFFFF;
    if (g.shootoutmode) {
        if (g.pregamepause) { g.pmenucount = 6; g.pmenulist = 3; }
        else { g.pmenucount = 6; g.pmenulist = 4; }
    } else if (g.gsp == 4) { g.pmenucount = 0x0F; g.pmenulist = 1; }
    else if (g.pregamepause) { g.pmenucount = 9; g.pmenulist = 2; }
    else ChoosePauseMenu();
    SpriteFrameStart();
    setvideo();
    WaitFrame();
    SpriteFrameEnd();
    g.timestep = 2;
    w16(RAM_OFF_colourbuf + 0x1DE, 0xFFFF);
    w16(RAM_OFF_colourbuf + 0x1FE, 0xFFFF);
    return PauseMenuPoll();
}

/* Pause mode: the text map saved, the sound stopped, the scoreboard screen with the pause menu until it is
 * left, then the text map and the screen back, the pause sounds, the frame clock restarted */
void Pausemode(void)
{
    MemCopy(0x7E0000 + RAM_OFF_textmap, 0x7E4F7E, 0x0800);
    FadeOutSound();
    uint16_t hor = g.sfhormode, fl = g.sflags;
    forcefade();
    WaitFadeOut();
    forceblack();
    SetHor();
    InitSprites(0x6000, 0, 0x01C0);
    FadeIn();
    ScreenOn();
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    g.psfirst = 0;
    g.psitem = 0;
    do {
        print(SYM_PausemodePos);
        Framer(0x16, 7);
        printscores1();
    } while (Pausemode_menu());
    g.pausemenuon = 0;
    g.pregamepause = 0;
    g.PauseUnused[0] = 0;
    forcefade();
    WaitFadeOut();
    forceblack();
    SoundInit();
    MemCopy(0x7E4F7E, 0x7E0000 + RAM_OFF_textmap, 0x0800);
    g.textdirtylo = 0;
    g.textdirtyhi = 0x03FF;
    g.sflags = fl;
    g.sfhormode = hor;
    if (!hor) ClrHor();
    g.sflags &= (uint16_t)~sfpz;
    printscores1();
    InitSprites(0x6000, 0, 0x01C0);
    FadeIn();
    ScreenOn();
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    g.SfxVelocity = 0x7000;
    QueueSound(0x12);
    QueueSound(0x13);
    g.recsfx = 0xFFFF;
    g.recsfx2 = 0xFFFF;
    g.lastframevc = g.vcount[0];
    g.oldvcount = g.vcount[0];
}

/* The game loop, a pass per frame elapsed: the frames per second, a team song now and then in the third
 * period without music, the step sizes, the pads, the clock (or the penalty shot timer: its end), the
 * crowd's cheers, a frame of the game, the demo's pads, the pause (forced by a MultiPlayer 5 warning).
 * Returns after a shootout turnover. */
void Gameloop(void)
{
    for (;;) {
        while (!(uint16_t)(g.vcount[0] - g.oldvcount)) hal_wait_interrupt();
        g.gameframes[0]++;
        uint32_t m = Mul16x8U(g.gameframes[0], 0x3C);
        g.gamefps = Div32U(m, g.FrameCount);
        if (g.gsp == 2 && !g.MusicOn && !(RandomWord() & 0x0FFF)) PlayTeamSong(5);
        RandomWord();
        uint16_t v = g.vcount[0];
        frame_steps((uint16_t)(v - g.oldvcount));
        g.lastframevc = v;
        g.oldvcount = v;
        ReadPads();
        if (!(g.sflags & sfpz) && !g.gmclock) {
            if (!g.shootoutmode && g.PenShotActive) {
                g.pstimer = (uint16_t)(g.pstimer - g.d7);
                if (!g.pstimer) EndPenaltyShotPlay(g.d7 >> 1);   /* (X holds d7) */
            } else {
                for (uint16_t k = g.d7; k; k--) {
                    if (!g.gameclock[0]) continue;
                    uint16_t f = g.gameclock[1];
                    g.gameclock[1] = (uint16_t)(f - 0x0AAA);
                    if (f >= 0x0AAA) continue;
                    g.gameclock[0]--;
                    uint16_t c = (uint16_t)(g.CwdExciteLvl + 0xFFFD);
                    if (!(c & 0x8000)) g.CwdExciteLvl = c;
                    Crowd_Noise();
                }
            }
        }
        if (g.cheercnt && (uint16_t)(g.vcount[0] - g.cheervc) >= 0x96) {
            g.cheercnt--;
            g.cheervc = (uint16_t)(g.cheervc + 0x96);
        }
        DoGameFrame();
        demoread();
        if (g.sflags & sfpz) Pausemode();
        else if (g.mp5warning && !(g.gmode & 0x0010)) {
            g.sflags |= sfpz;
            Pausemode();
        }
        if (g.shootoutmode && g.shootoutturnover) return;
    }
}

/* A summary list's rows shown (from listtop, listrows of them, 3 text rows each) to VRAM y, or the none
 * message when the list has `empty` entries */
void summary_page(uint32_t none, uint16_t empty, uint16_t y)
{
    if (g.listend[0] == empty) {
        print(none);
        QueueTextDMA();
        return;
    }
    uint16_t lt = g.listtop[0], lr = g.listrows[0];
    uint16_t t = (uint16_t)((uint16_t)(lt << 1) + lt + (lt >> 15));   /* (asl's carry) */
    t = (uint16_t)(t << 8 | t >> 8);
    uint32_t p0 = (statspage & 0xFF0000) | (uint16_t)((t >> 2) + (uint16_t)statspage + (t >> 1 & 1));   /* (lsr's carry) */
    uint16_t u = (uint16_t)((uint16_t)(lr << 1) + lr + (lr >> 15));
    u = (uint16_t)(u << 8 | u >> 8);
    DMAToVRAM(&p0, (uint16_t)(u >> 2), y, 0);
}

/* The pause menu's screens: A / B show one, then the rink comes back (A = 1); other buttons go on polling */
static uint16_t pause_screen(uint16_t d1, void (*show)(void))
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    show();
    ExitStatsScreen();
    return PauseSubmenuDone();
}
uint16_t PauseGameStats(uint16_t d1) { return pause_screen(d1, StatsReport); }
uint16_t PausePenaltySummary(uint16_t d1) { return pause_screen(d1, PenaltySummaryScreen); }
uint16_t PauseScoringSummary(uint16_t d1) { return pause_screen(d1, ScoringSummaryScreen); }
uint16_t PauseOtherScores(uint16_t d1) { return pause_screen(d1, ShowScores); }
uint16_t PauseTeamRoster(uint16_t d1) { return pause_screen(d1, TeamRosterScreen); }


uint16_t PausePeriodStats(uint16_t d1) { return pause_screen(d1, PeriodStatsScreen); }
uint16_t PausePlayerStats(uint16_t d1) { return pause_screen(d1, PlayerStatsScreen); }


uint16_t PausePlayerCards(uint16_t d1) { return pause_screen(d1, PlayerCards); }

uint16_t PauseCrowdLevel(uint16_t d1) { return pause_screen(d1, CrowdMeterScreen); }


/* Edit lines from the pause menu: A / B open the line editor (LineEditor), then the rink is reloaded */
uint16_t PauseEditLines(uint16_t d1)
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    g.pausesubmenu = 1;
    LineEditor();
    g.pausesubmenu = 0;
    ExitStatsScreen();
    return PauseSubmenuDone();
}

/* Shootout setup from the pause menu: A / B open the shooter order (ShootoutShooters), then the rink is
 * reloaded */
uint16_t PauseShootoutSetup(uint16_t d1)
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    g.pausesubmenu = 1;
    ShootoutShooters();
    g.pausesubmenu = 0;
    ExitStatsScreen();
    return PauseSubmenuDone();
}

/* Start a period: the teams' energy, the rink and the clock, the pregame pause (owned by the first pad on a
 * team), the faceoff, the replay buffer, the frame counters and the sprites; then the game loop (in the
 * shootout the caller runs it) */
static void start_period(void)
{
    reenergizeteam(0);
    reenergizeteam(2);
    g.reloadtime = 0;
    forceblack();
    setupice_reset();
    ResetClock();
    InitCrowdExcite();
    if (g.pregamepause) {
        g.pausepad = 0xFFFF;
        for (uint16_t y = 0; y < 0xA; y += 2) {
            uint16_t c = r16(RAM_OFF_cont1team + y);
            if (c) { g.pausepad = (uint16_t)-c; break; }
        }
        if (!(g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0])) g.pausepad = 0;
        startpause();
    }
    g.fox = 0;
    g.foy = 0;
    assreplace(14, 0x1B);
    ClearTextMap();
    g.sflags |= 0x0400;
    g.recsfx = 0xFFFF;
    g.recsfx2 = 0xFFFF;
    g.percue = 0xFFFF;
    if (g.OptPlayMode != 4) {
        g.sflags &= (uint16_t)~0x0010;
        g.ReplayBufferPtr = ReplayBufferStart;
    } else if (!g.pregamepause) g.percue = 0;
    g.textdirtylo = 0xFFFF;
    g.textdirtyhi = 0;
    g.vcount[0] = 0;
    g.vcount[1] = 0;
    g.lastframevc = 0;
    g.oldvcount = 0;
    InitSprites(0x6000, 0, 0x01C0);
    if (!g.shootoutmode) Gameloop();
}
/* Start the next period (start_period); outside the shootout the stack is reset first, since the game
 * loop never returns */
void StartPer(void)
{
    if (!g.shootoutmode) hal_stack_reset(start_period);
    else start_period();
}

/* A new game from its start: game RAM cleared, the options in force, the teams, scores and lines, the pads'
 * teams and sides, then the first period (StartPer; in the shootout the caller starts it) */
void StartGame_go(void)
{
    g.FrameByNumber = 0xFFFF;
    forcefade();
    for (uint32_t x = 0x0AD7; x != 0x1A11; x++) w16(x, 0);   /* (a word at each byte, as the original clears it) */
    memset(WRAM_BYTES + 0x30F4, 0, 0x347A - 0x30F4);
    g.gamesstarted[0]++;
    g.gmode = 0;
    if (g.OptPen == 1) g.gmode |= 0x0020;
    clearTeamStats();
    restoreteams();
    InitScores();
    g.ReplayBufferPtr = ReplayBufferStart;
    g.sflags &= (uint16_t)~0x0010;
    LoadTeamLines(0);
    LoadTeamLines(2);
    g.goaliemode1 = g.OptGoalie;
    g.goaliemode2 = g.OptGoalie;
    g.tmtimeouts[0] = 1;
    g.tmtimeouts[1] = 1;
    g.penshot = 0;
    g.penshotlive = 0;
    g.lastperiod = 0;
    g.newperiod = 1;
    g.teampads[0] = 0;
    g.teampads[1] = 0;
    for (int x = 8; x >= 0; x -= 2) {
        uint16_t c = r16(RAM_OFF_cont1team + x);
        if (c == 1) g.teampads[0]++;
        else if (c == 2) g.teampads[1]++;
    }
    setupice();
    g.pregamepause = 0;
    for (int k = 0; k < 5; k++) g.padside[k] = (uint16_t)((uint16_t)(r16(RAM_OFF_cont1team + 2 * k) - 1) << 1);
    for (int x = 0; x < 0xA; x += 2)
        if (r16(RAM_OFF_cont1team + x)) { g.pregamepause = 1; break; }
    g.BA_Sktr_SCnum = 0xFFFF;
    if (!g.shootoutmode) StartPer();
}

/* A new game from the hot / cold table and the scouting report on */
void StartGame_scout(void)
{
    Create_HotCold_Table();
    ScoutingReport();
    StartGame_go();
}

/* A new game: the playoff screen, the scouting report, then the first period */
void StartGame(void)
{
    PlayoffScreen();
    StartGame_scout();
}
