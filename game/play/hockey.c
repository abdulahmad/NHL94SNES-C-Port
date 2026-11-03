/* hockey: the game loop, the pause menus, the stats screens and the title (src/routines/hockey). */
#include "game.h"

/* a byte or word written at a 24-bit address (only WRAM takes it) */
static void put8(uint32_t a, uint8_t v) { if (is_wram(a)) w8(wram_off(a), v); }
static void put16(uint32_t a, uint16_t v) { put8(a, (uint8_t)v); put8(a + 1, (uint8_t)(v >> 8)); }

/* Do nothing */
void CheckLateGameSong(void) {}
void UpdateCwdExcite(void) {}
void nullrtl(void) {}
void TitleStub(void) {}

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

/* The end of the player stats screen: player stats off */
void ShowPlayerStats_exit(void) { g.playerstatson = 0; }

/* Write the low byte of v to the line slot at a3 / read it */
void WriteLineSlot(uint32_t a3, uint16_t v) { put8(a3, (uint8_t)v); }
uint16_t ReadLineSlot(uint32_t a3) { return m8(a3); }

/* Text palette $2000 for the sorted stat column, else $2400 */
void SetStatColumnPal(uint16_t column) { g.printa = column == g.statcolumn ? 0x2000 : 0x2400; }

/* Wait until the pause pad's buttons are up (the pads are read by the interrupt) */
void WaitButtonRelease(void)
{
    while (r16(RAM_OFF_JoyPad + g.pausepad)) hal_wait_interrupt();
}

/* The period length in seconds for the period length option (d0) */
uint16_t GetPeriodTime(void) { return rom16(SYM_GetPeriodTime_timetab + (uint16_t)(g.OptPerlen << 1)); }

/* The RAM lines of team a2 (a3, bank 0): homelines for team 0, else awaylines */
uint32_t GetTeamLines(uint16_t a2) { return a2 ? awaylines : homelines; }

/* The title's busy-wait (no RAM changes) */
/* A busy loop of x passes, none once a pad press has ended the title (titleskip) */
void TitleDelay(uint16_t x)
{
    if (g.titleskip) return;
    do hal_cost_mark(SYM_TitleDelay_loop); while (--x);
}

/* Reset the scouting report text scroll; a0 = TestList */
uint32_t InitScoutingDisplay(void)
{
    g.VertLineScrolling = 0xFFFF;
    g.PlayerScrollCtr = 0xFFFF;
    g.SelectedPlayerIdx = 0;
    g.ScoutingReportTimer = 0;
    return TestList;
}

/* Reset the credits state */
void StartCredits(void)
{
    g.CreditTitleIdx = 0;
    g.CreditNameIdx = 0;
    g.CreditStep = 0;
    g.CreditTitleY = 0xE0;
    g.CreditNameY = 0xF0;
    g.CreditTime = 0;
}

/* Blank the password */
void ResetPassWord(void)
{
    g.PassWord[0] = 0;
    for (int i = 15; i > 0; i--) g.PassWord[i] = 0x1E;
}

/* Start the hat trick sprites: clear the ten slots and set hattrick */
void StartHatTrick(void)
{
    for (int i = 0; i < 10; i++) g.hatstep[i] = 0;
    g.hattrick = 1;
}

/* Start bench a cheering, unless one already is */
void StartBenchCheer(uint16_t a)
{
    if (g.benchcheer) return;
    g.benchcheer = a;
    g.benchframe = (uint16_t)(a << 3);
    g.benchtimer = 0x0B;
    g.benchrounds = 0;
}

/* Copy the card team's two colours (a2 nonzero: colours $67-$68, else $77-$78) to colours $4E-$4F */
void SetCardTeamColours(uint16_t a2)
{
    uint16_t x = a2 ? 0xCE : 0xEE;
    w16(RAM_OFF_colourbuf + 0x9C, r16(RAM_OFF_colourbuf + x));
    w16(RAM_OFF_colourbuf + 0x9E, r16(RAM_OFF_colourbuf + x + 2));
}

/* The y scroll of title path step a: $1B0 - (a * $50 / 256) / 5 */
uint16_t TitlePathY(uint16_t a)
{
    uint16_t r2 = (uint16_t)(Mul16x8XS(a, 0x50) >> 8);
    return (uint16_t)(0x01B0 - Div16S(r2, 5));
}

/* The mode 7 scale of title path step a */
uint16_t TitlePathScale(uint16_t a)
{
    if (a < 0x400) {
        uint16_t r2 = (uint16_t)(Mul16U(a, 0x0E00) >> 8);
        return (uint16_t)(0x0F00 - (uint16_t)(Div16S(r2, 8) << 1));
    }
    uint16_t r2 = (uint16_t)(Mul16U((uint16_t)(a - 0x400), 0x00C0) >> 8);
    return (uint16_t)(0x0100 - (uint16_t)(Div16S(r2, 2) << 1));
}

/* The roster record of player d0 of team a2 (a0): past the team header, then d0 records of (length word
 * + 8) bytes */
uint32_t GetPlayerRecord(uint16_t a2, uint16_t d0)
{
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a0 = r16(RAM_OFF_tmdata + a2);
    a0 = (uint16_t)(a0 + m16(bank | a0));
    while (!((d0 = (uint16_t)(d0 - 1)) & 0x8000)) {
        uint32_t s = (uint32_t)a0 + m16(bank | a0);
        a0 = (uint16_t)(s + 8 + (s >> 16));   /* (the first add's carry) */
    }
    return bank | a0;
}

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

/* At a period start (not overtime): crowd excitement $10 per playoff stage */
void InitCrowdExcite(void)
{
    if (g.gsp == 3) return;
    uint16_t r0 = 0;
    if (g.OptPlayMode != 0 && g.OptPlayMode != 4) {
        r0 = 1;
        if (!((uint16_t)(g.playoffRoundIdx - 2) & 0x8000)) {
            r0 = 2;
            if (!((uint16_t)(g.playoffRoundIdx - 3) & 0x8000)) r0 = 3;
        }
    }
    g.CwdExciteLvl = (uint16_t)(r0 << 4);
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

/* Fill titleteams with the 26 team numbers in random order */
void ShuffleTitleTeams(void)
{
    g.TitleTeamIdx[0] = 0;
    uint16_t x = 0;
    while (x < 0x34) {
        uint16_t t;
        do t = RandomWord() & 0x1F; while (t >= 0x1A);
        uint16_t k;
        for (k = 0; k < g.TitleTeamIdx[0]; k += 2)
            if (r16(RAM_OFF_titleteams + k) == t) break;
        if (k < g.TitleTeamIdx[0]) continue;
        w16(RAM_OFF_titleteams + x, t);
        x += 2;
        g.TitleTeamIdx[0] = x;
    }
    g.TitleTeamIdx[0] = 0;
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

/* While HotColdRaised is not 0: the first four bytes of each of the 52 hot / cold entries go down by 2 */
void LowerHotColdTable(void)
{
    if (!g.HotColdRaised) return;
    for (int k = 0; k < 52; k++)
        for (int b = 0; b < 4; b++) g.HotColdTable[k * 12 + b] -= 2;
    g.HotColdRaised--;
}

/* The window and COLDATA HDMA (channels 2 and 3): window 1 from left to right, the tables in bank $9D */
static void start_hdma(uint8_t left, uint8_t right, uint16_t wtab, uint16_t ctab)
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
void ScoutingHDMA(void) { start_hdma(0x0C, 0xB3, (uint16_t)SYM_ScoutingHDMA_2, (uint16_t)SYM_ScoutingHDMA_4); }
void MenuScreenHDMA(void) { start_hdma(0x40, 0xBF, (uint16_t)SYM_MenuScreenHDMA_2, (uint16_t)SYM_MenuScreenHDMA_4); }

/* Each frame: step the cheering bench through its frames, and end it ($FFFF) after its rounds */
void UpdateBenchCheer(void)
{
    if (!g.benchcheer) { g.benchframe = 0xFFFF; return; }
    g.benchframe &= 7;
    uint16_t t = (uint16_t)(g.benchtimer - g.timestep);
    if (!(t & 0x8000)) g.benchtimer = t;
    else {
        g.benchtimer = (uint16_t)(t + 0x0B);
        if (g.benchrounds == 4) {
            uint16_t f = (uint16_t)(g.benchframe - 1);
            if (f & 0x8000) { g.benchcheer = 0; g.benchframe = 0xFFFF; return; }
            g.benchframe = f;
        } else {
            uint16_t f = (uint16_t)(g.benchframe + 1);
            if (f != 6) g.benchframe = f;
            else if (++g.benchrounds != 4) g.benchframe = 2;
        }
    }
    g.benchframe = (uint16_t)((uint16_t)(g.benchcheer << 3) + g.benchframe + (g.benchcheer >> 13 & 1));   /* (asl's carry) */
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

/* End the string built in mesarea: its length word = a1 - mesarea; a1 = mesarea */
uint32_t FinalizeTextBuffer(uint32_t a1)
{
    g.mesarea[0] = (uint16_t)(a1 - RAM_OFF_mesarea);
    return (a1 & 0xFF0000) | RAM_OFF_mesarea;
}

/* RenderBigText's letter hooks: letter tile bigtextchar (two rows) at printx / printy of screenmap2
 * (PlayoffScreen) or screenmap (DrawSelectTeam) */
void BigLetterPlayoff(void)
{
    uint16_t x = (uint16_t)(((uint16_t)(g.printy << 7) + g.printx + (g.printy >> 9 & 1)) << 1);   /* (asl's carry) */
    uint16_t v = (uint16_t)(g.bigtextchar | 0x2000 | g.printa);
    w16(RAM_OFF_screenmap2 + 6 + x, v);
    w16(RAM_OFF_screenmap2 + 0x106 + x, (uint16_t)(v + 1));
}
/* RenderBigText's letter hook on the team select screen: tile bigtextchar (two rows) in screenmap */
void BigLetterSelect(void)
{
    uint16_t x = (uint16_t)(((uint16_t)(g.printy << 5) + g.printx + (g.printy >> 11 & 1)) << 1);   /* (asl's carry) */
    uint16_t v = (uint16_t)(g.bigtextchar | 0x2400);
    w16(RAM_OFF_screenmap + 0x0C + x, v);
    w16(RAM_OFF_screenmap + 0x4C + x, (uint16_t)(v + 1));
}

/* Pause for pad a / from controller 1 */
void PauseFromPad(uint16_t a) { g.pausepad = a; startpause(); }
void startpause1(void) { g.pausepad = 0; startpause(); }

/* The next team of the shuffled title list, shuffling again after the 26th */
uint16_t NextTitleTeam(void)
{
    uint16_t i = (uint16_t)(g.TitleTeamIdx[0] + 1);
    if (i >= 0x1A) { ShuffleTitleTeams(); i = 0; }
    g.TitleTeamIdx[0] = i;
    return r16(RAM_OFF_titleteams + (uint16_t)(i << 1));
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

/* Erase the braces at stats page rows top and bottom (column x) */
static void clear_braces(uint16_t x, uint16_t top, uint16_t bottom)
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

/* Erase the braces around the visible game-stat rows / player-stat rows / other-scores rows */
void ClearGameStatBraces(void)
{
    clear_braces(1, (uint16_t)(g.listtop[0] << 1), (uint16_t)((uint16_t)(g.listtop[0] + g.listrows[0]) << 1));
}
/* Erase the braces around the player-stat rows shown (from curitem, listrows of them), before a scroll */
void ClearPlayerStatBraces(void)
{
    clear_braces(1, (uint16_t)(g.curitem[0] << 1), (uint16_t)((uint16_t)(g.curitem[0] + g.listrows[0] - 1) << 1));
}
/* Erase the braces around the other scores shown (three rows a game from listtop), before a scroll */
void ClearOtherScoreBraces(void)
{
    uint16_t t = g.listtop[0];
    uint16_t top = (uint16_t)((uint16_t)(t << 1) + t + (t >> 15));   /* (asl's carry) */
    uint16_t r3 = (uint16_t)(t + g.listrows[0] - 1);
    clear_braces(4, top, (uint16_t)((uint16_t)(r3 << 1) + r3 + (r3 >> 15) + 1));
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

/* Title: the mode 7 scale down to $100 a step at a time */
void TitleZoomIn(void)
{
    uint16_t a = g.TitleM7Scale;
    while ((a = (uint16_t)(a - 1)) >= 0x100) {
        g.TitleM7Scale = a;
        TitleDelay(100);
    }
    g.TitleM7Scale = 0x100;
}

/* Title: count BG1HScroll down, moving the window edges each step, then close them */
void TitleWindowWipe(void)
{
    uint16_t a = g.BG1HScroll;
    while (!((a = (uint16_t)(a - 1)) & 0x8000)) {
        g.BG1HScroll = a;
        uint16_t v = (uint16_t)-a & 0xFF;
        if (v) {
            g.TitleWH1 = v;
            if (!((uint16_t)(v - 0x20) & 0x8000)) {
                g.TitleWH3Low = (v - 0x20) & 0xFF;
            }
        }
        TitleDelay(0x200);
    }
    g.BG1HScroll = 0;
    g.TitleWH1 = 0xFF;
    g.TitleWH3Low = 0xFF;
}

/* Title: the mode 7 scale up to $200 while the BG1 scroll moves to ($1E8 / $120, $140) */
static void title_zoom_out(int right, uint16_t endh)
{
    uint16_t s;
    while ((s = (uint16_t)(g.TitleM7Scale + 1)) < 0x200) {
        g.TitleM7Scale = s;
        uint16_t d = (uint16_t)(s - 0x100);
        uint16_t h = Mul8U(d, right ? 0x58 : 0x70) >> 8;
        g.TitleM7HOfs = right ? (uint16_t)(h + 0x190) : (uint16_t)(0x190 - h);
        uint16_t v = (uint16_t)(0x180 - (Mul8U(d, 0x40) >> 8));
        g.TitleM7VOfs = v;
        TitleDelay(1000);
    }
    g.TitleM7Scale = 0x200;
    g.TitleM7HOfs = endh;
    g.TitleM7VOfs = 0x140;
}
void TitleZoomOutRight(void) { title_zoom_out(1, 0x01E8); }
void TitleZoomOutLeft(void) { title_zoom_out(0, 0x0120); }

/* mesarea = the first initial, a period and the last name of roster player d0 of team a2 (a word written
 * at a time, so a byte past each); a1 = mesarea */
uint32_t PlayerInitialName(uint16_t a2, uint16_t d0)
{
    uint32_t a0 = GetPlayerRecord(a2, d0), bank = a0 & 0xFF0000;
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

/* Copy team a2's lines ($38 bytes at its team data's offset +6) into its RAM lines, then without line
 * changes the 8 bytes at offset +$0C over their start */
void LoadTeamLines(uint16_t a2)
{
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a3 = (uint16_t)GetTeamLines(a2), t = r16(RAM_OFF_tmdata + a2);
    uint16_t a1 = (uint16_t)(t + m16((bank | t) + 6));
    for (int k = 0; k < 0x38; k++) w8((uint16_t)(a3 + k), m8(bank | (uint16_t)(a1 + k)));
    if (g.OptLine) return;
    a3 = (uint16_t)GetTeamLines(a2);
    a1 = (uint16_t)(t + m16((bank | t) + 0x0C));
    for (int k = 0; k < 8; k++) w8((uint16_t)(a3 + k), m8(bank | (uint16_t)(a1 + k)));
}

/* mesarea = the first initial and last name of player TempPlOffset (bit 15: a visitor); a1 = mesarea */
uint32_t GetPlayerInitial(void)
{
    uint16_t d0 = g.TempPlOffset, a2 = 0;
    if (d0 & 0x8000) { d0 &= 0xFF; a2 = 2; }
    return PlayerInitialName(a2, d0);
}

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

/* RAM's jml trampoline: the routine at jmlvector+1 (RenderBigText's letter hooks) */
static void jml_vector(void)
{
    uint32_t t = r16(RAM_OFF_jmlvector + 1) | (uint32_t)r8(RAM_OFF_jmlvector + 3) << 16;
    if (t == SYM_BigLetterSelect) BigLetterSelect();
    else if (t == SYM_BigLetterPlayoff) BigLetterPlayoff();
}

/* RenderBigText's state: the string, the letter pixels (hlsavebuf), the pixel offset r3 in the tile, its
 * complement r3+2, the shift masks' indexes r0+2 / r2+2, the letter's pixel rows r1 / r1+2 */
static struct { uint32_t p0, p1; uint16_t r1, r1h, r3, r3h, r0h, r2h; } bt;

/* The width of the string in pixels (a space at the start counts nothing) */
static uint16_t bigtext_width(void)
{
    uint16_t r3 = 0;
    for (uint16_t y = 0;; y++) {
        uint16_t c = m8(bt.p0 + y), w;
        if (!c) return r3;
        if (c == 0x20) { if (!y) continue; w = 5; }
        else if (c == 0x2E) w = 3;
        else if (c == 0x40) w = m8(SYM_LetterWidths + 0x1B);
        else if (c >= 0x30 && c < 0x3A) w = m8(SYM_LetterWidths + (uint16_t)(c - 0x30 + 0x1C));
        else w = m8(SYM_LetterWidths + (uint16_t)((c & 0xDF) - 0x41));
        r3 = (uint16_t)(r3 + w);
    }
}

/* OR a letter's 8 pixel rows (from r1, shifted right r3) into tile t of bigtextbuf */
static void bigtext_or(uint16_t t)
{
    uint32_t x = RAM_OFF_bigtextbuf + (uint16_t)(t << 4);
    for (int k = 0; k < 8; k++, x += 2, bt.r1 += 2) {
        uint16_t a = m16(bt.p1 + bt.r1);
        if (bt.r3) a = (uint16_t)(a >> bt.r3);
        a &= m16((SYM_RenderBigText_8 & 0xFF0000) + (uint16_t)(SYM_RenderBigText_8 + 16) + bt.r0h);
        w16(x, (uint16_t)(a | r16(x)));
    }
}

/* Store a letter's 8 pixel rows (from r1, shifted left r3+2 + 1) into tile t of bigtextbuf: the last one */
static uint16_t bigtext_put(uint16_t t)
{
    uint32_t x = RAM_OFF_bigtextbuf + (uint16_t)(t << 4);
    uint16_t a = 0;
    for (int k = 0; k < 8; k++, x += 2, bt.r1 += 2) {
        a = m16(bt.p1 + bt.r1);
        uint16_t y = (uint16_t)(bt.r3h + 1);
        do a = (uint16_t)(a << 1); while (--y);
        a &= m16((SYM_RenderBigText_8 & 0xFF0000) + (uint16_t)SYM_RenderBigText_8 + bt.r2h);
        w16(x, a);
    }
    return a;
}

/* The pixel rows of letter r2's half (0 top: its BigLetterMap entry; 1: the one $36 bytes on) */
static uint16_t bigtext_rows(uint16_t r2, int half)
{
    return (uint16_t)(m16(SYM_BigLetterMap + 6 + (uint16_t)((uint16_t)(r2 << 1) + (half ? 0x36 : 0))) << 5);
}

/* Draw string p0 in the big proportional font: letter tiles into bigtextbuf from tile bigtextchar, centred
 * in $24 pixels from printx, the jmlvector hook placing each letter on the map. A letter outside A-Z (a
 * digit) ends the string after its first tile. */
void RenderBigText(uint32_t p0)
{
    bt.p0 = p0;
    bt.p1 = 0x7E0000 | RAM_OFF_hlsavebuf;
    uint16_t a = (uint16_t)(0x24 - (bigtext_width() >> 1));
    bt.r3 = a;
    bt.r3h = a >> 3;
    g.printx = (uint16_t)(g.printx + bt.r3h);
    bt.r3 = (uint16_t)(bt.r3 - (uint16_t)(bt.r3h << 3));
    for (uint16_t y = 0;; y++) {
        bt.r0h = (uint16_t)(bt.r3 << 1);
        uint16_t c = m8(bt.p0 + y), r2;
        if (!c) {
            jml_vector();
            g.printx++;
            bt.r1 = bt.r1h;
            g.bigtextchar += 2;
            return;
        }
        if (c == 0x20) a = 0x20;
        else if (c == 0x2E) a = 0x1A;
        else a = (uint16_t)((c & 0xDF) - 0x41);
        r2 = a;
        if (a == 0x20) a = 5;
        else {
            bt.r1 = bt.r1h = bigtext_rows(r2, 0);
            bigtext_or(g.bigtextchar);
            bt.r1 = bigtext_rows(r2, 1);
            bigtext_or((uint16_t)(g.bigtextchar + 1));
            a = m8(SYM_LetterWidths + r2);
        }
        a = (uint16_t)(a + bt.r3);
        if (a >= 8) {
            jml_vector();
            g.printx++;
            bt.r1 = bt.r1h;
            g.bigtextchar += 2;
            if (r2 & 0x8000) return;
            a = r2;
            if (r2 != 0x20) {
                bt.r3h = bt.r3 ^ 7;
                bt.r2h = (uint16_t)(bt.r3h << 1);
                bigtext_put(g.bigtextchar);
                bt.r1 = bigtext_rows(r2, 1);
                a = bigtext_put((uint16_t)(g.bigtextchar + 1));
            }
        }
        a = a == 0x20 ? 5 : m8(SYM_LetterWidths + r2);   /* (A, not the letter, is tested) */
        bt.r3 = (uint16_t)(a + bt.r3) & 7;
    }
}

/* Turn the password bits into the password characters (the remainders by PWrange, the last first) */
void BitstoPW(void)
{
    for (int x = 15; x >= 0; x--) g.PassWord[x] = (uint8_t)SuperDiv(PWrange);
}

/* Turn the password characters into the password bits */
void PWtoBits(void)
{
    ClrPassBits();
    for (int x = 0; x < pwlength; x++) {
        SuperMult(PWrange);
        SuperAdd(g.PassWord[x]);
    }
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

/* The MultiPlayer 5 warning lines: print the line's position, then string a1 centred */
uint32_t PrintMP5Line1(uint32_t a1) { print(SYM_PrintMP5Line1Pos); return PrintCentered(a1); }
uint32_t PrintMP5Line2(uint32_t a1) { print(SYM_PrintMP5Line2Pos); return PrintCentered(a1); }
uint32_t PrintMP5Line3(uint32_t a1) { print(SYM_PrintMP5Line3Pos); return PrintCentered(a1); }
uint32_t PrintMP5Line4(uint32_t a1) { print(SYM_PrintMP5Line4Pos); return PrintCentered(a1); }

/* Where a scouting report name is printed: the record's position, column $14 for the home side */
void ScoutNamePos(uint16_t a2)
{
    print(SYM_ScoutNamePosAt + 5);
    if (!a2) g.printx = 0x14;
}

/* Line editor: erase the menu area (32 x 17) */
void ClearEditLinesMenu(void)
{
    print(SYM_ClearEditLinesMenuPos);
    eraser(0x20, 0x11, g.blankchar);
}

/* Move the password cursor to character d5: rows of 6 from the record's position */
void PlacePasswordCursor(uint16_t d5)
{
    print(SYM_PlacePasswordCursorPos);
    uint16_t q = DivX8U(d5, 6), rem = (uint16_t)(d5 - q * 6);   /* (the divider's remainder, RDMPYL) */
    g.printy = (uint16_t)((uint16_t)(q << 1) + g.printy + (q >> 15));   /* (asl's carry) */
    g.printx = (uint16_t)((uint16_t)(rem << 1) + g.printx + (rem >> 15) - 2);
}

/* Print the Select Left hint on the password screen, d4 columns on */
void PrintSelectLeft(uint16_t d4)
{
    print(SYM_PrintSelectLeftPos);
    g.printx = (uint16_t)(g.printx + d4);
    print(SYM_SelectLeft);
}

/* The crowd meter (d0): sqrt(CwdExciteLvl * 4) + 65, at most 128; the peak and the running average */
uint16_t UpdateCrowdMeter(void)
{
    uint16_t d0 = (uint16_t)(sroot((uint16_t)(g.CwdExciteLvl << 2)) + 0x41);
    if (d0 >= 0x80) d0 = 0x80;
    if (!((uint16_t)(d0 - g.CrowdPeak) & 0x8000)) g.CrowdPeak = d0;
    uint32_t sum = (g.CrowdMeterSum[0] | (uint32_t)g.CrowdMeterSum[1] << 16) + d0;
    g.CrowdMeterSum[0] = (uint16_t)sum;
    g.CrowdMeterSum[1] = (uint16_t)(sum >> 16);
    g.CrowdMeterCount++;
    g.CrowdAvg = Div32U(sum, g.CrowdMeterCount);
    return d0;
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

/* Print the password screen keys */
void PrintPasswordHelp(void)
{
    print(SYM_SelectTxt);
    print(SYM_ANextTxt);
    print(SYM_YBackTxt);
    print(SYM_PressStartTxt);
}

/* mesarea = the uniform number and name of roster player d0 of team a2; a1 = mesarea */
uint32_t getname(uint16_t a2, uint16_t d0)
{
    uint32_t rec = GetPlayerRecord(a2, d0), bank = rec & 0xFF0000;
    w16(RAM_OFF_mesarea, 4);
    uint16_t a0 = (uint16_t)(rec + m16(rec));
    PutDigits2(m8(bank | a0), RAM_OFF_mesarea + 2);
    appstring(SYM_PlayerNumNamePos, RAM_OFF_mesarea);
    appstring(rec, RAM_OFF_mesarea);
    return RAM_OFF_mesarea;
}

/* mesarea = the number, first initial and last name of roster player d0 of team a2 (a word written at a
 * time); a1 = mesarea */
uint32_t FormatPlayerNumInitial(uint16_t a2, uint16_t d0)
{
    uint32_t a0 = GetPlayerRecord(a2, d0), bank = a0 & 0xFF0000;
    uint16_t p0 = (uint16_t)a0;
    w16(RAM_OFF_mesarea, 2);
    uint16_t a1 = (uint16_t)PutDigits2(m8(bank | (uint16_t)(p0 + m16(a0))), RAM_OFF_mesarea + 2);
    w16(a1++, 0x0020);
    uint16_t end = (uint16_t)(m16(a0) + p0);
    p0 += 2;
    w16(a1++, m16(bank | p0++));
    w16(a1++, 0x002E);
    w16(a1++, 0x0020);
    while ((m8(bank | p0++)) != 0x20) {}
    do w16(a1++, m16(bank | p0++)); while (p0 != end);
    return FinalizeTextBuffer(a1);
}

/* Print the password (PassWord's characters through PasstoText), unless it is hidden (bit 7) */
void PWprint(void)
{
    if (g.PassWord[0] & 0x80) return;
    uint16_t a1 = RAM_OFF_mesarea;
    w16(a1, 0x12);
    a1 += 2;
    for (int k = 0; k < 16; k++) w16(a1++, m16(SYM_PasstoText + g.PassWord[k]));
    print(RAM_OFF_mesarea);
}

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

/* Turn the password into the playoff state */
void DecodePW(void) { PWtoBits(); ReadPassBits(); }

/* Erase the scouting report name line of side a2 */
void ClearRosterName(uint16_t a2)
{
    ScoutNamePos(a2);
    eraser(0x0B, 2, g.blankchar);
}

/* The crowd meter into CurCrowdMeter: 110 dB or more sets CrowdLoud; under 105 dB a loud crowd's raised
 * hot / cold values come down */
void CheckCrowdMeter(void)
{
    uint16_t d0 = UpdateCrowdMeter();
    g.CurCrowdMeter = d0;
    if ((uint16_t)(d0 - 0x69) & 0x8000) {
        if (g.CrowdLoud) { LowerHotColdTable(); g.CrowdLoud = 0; }
    } else if (!((uint16_t)(d0 - 0x6E) & 0x8000)) g.CrowdLoud = 1;
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


/* Start sprite animation anim on object o (as SetSPAFromA) and update it at once */
void SetSPAUpdate(int o, uint16_t anim)
{
    if (anim == g.SPA[o]) return;
    g.SPA[o] = anim;
    g.SPAnum[o] = 0;
    g.SPAcnt[o] = 0xFFFF;
    updateanim(o);
}


static void inc16(uint32_t off) { w16(off, (uint16_t)(r16(off) + 1)); }

/* The puck is in the net (o, the puck): the score and team stats, the siren and the crowd, the score
 * summary and the players' goals and assists, the scoreboard, the scorers celebrate, the puck pops up at
 * the goal line and the goal whistle. A stopped clock only stops the puck; a penalty shot at the wrong net
 * (by the end and gmdir, against BA_Team) ends without a goal. */
void Goal(int o)
{
    if (g.gmclock) {
        g.puckvz = 0;
        g.puckvx[0] = 0;
        g.puckvy[0] = 0;
        return;
    }
    if (g.PenShotActive) {
        int right = !(g.pucky[0] & 0x8000) == !!(g.gmode & gmdir);
        if (right ? !g.BA_Team : g.BA_Team) { EndPenaltyShot(o); return; }
    }
    ChkShotStat();
    freezewindow();
    g.replaydelay = 0x1E;
    uint16_t a2 = 0, a1 = 2;
    if (g.pucky[0] & 0x8000) { a1 = 0; a2 = 2; }
    if (g.gmode & gmdir) { a2 = a1; a1 ^= 2; }
    inc16(RAM_OFF_tmscore + a2);
    uint16_t d = (uint16_t)(g.tmap[0] - g.tmap[1]);
    if (d && (d & 0x8000 ? !a2 : a2)) inc16(RAM_OFF_tmSHGoals + a2);
    if (g.penshot) inc16(RAM_OFF_tmPenShotGoals + a2);
    else if (g.breakaway) inc16(RAM_OFF_tmBreakawayGoals + a2);
    if (g.onetimer) inc16(RAM_OFF_tmOneTimerGoals + a2);
    MusicStop();
    uint16_t a = g.streamsfx == 1 ? 1 : 2;   /* (StreamSfx's A: the stream asked for, or 1 still playing) */
    StreamSfx(2);
    g.benchcheer = 0;
    if (a2) {
        StartBenchCheer(4);
        uint16_t c = (uint16_t)(g.Crowdlevel - 0x300);
        g.Crowdlevel = c & 0x8000 ? 0 : c;
    } else {
        StartBenchCheer((uint16_t)(a + 2));
        g.Crowdlevel = (uint16_t)(g.Crowdlevel + 0x1F4);
        g.crowdhold = 0x78;
        PlayTeamSong(3);
    }
    g.sfxmutetime = g.vcount[0];
    EndPenaltyShot(o);
    g.breakaway = 0;
    g.onetimer = 0;
    uint16_t x = (uint16_t)(g.gsp << 2);
    uint16_t c = (uint16_t)(g.CwdExciteLvl + 0x1E);
    if (!(c & 0x8000)) g.CwdExciteLvl = c;
    if (a2) {
        x += 2;
        c = (uint16_t)(g.CwdExciteLvl + 0xFFCE);
        if (!(c & 0x8000)) g.CwdExciteLvl = c;
    }
    inc16(RAM_OFF_tmPerGoals + x);
    if (g.ScoreSumbytes[0] == 0x258) g.ScoreSumbytes[0] -= 6;
    uint32_t s = (uint32_t)(uint16_t)ScoreSum + g.ScoreSumbytes[0];
    uint32_t a0 = (ScoreSum & 0xFF0000) | (uint16_t)s;
    g.ScoreSumbytes[0] = (uint16_t)(g.ScoreSumbytes[0] + 6 + (s >> 16));   /* (the first add's carry) */
    put16(a0, GetPeriodTimeRemaining());
    a0 += 2;
    uint16_t v = (uint16_t)(2 + r16(RAM_OFF_tmap + a2) - r16(RAM_OFF_tmap + a1));
    if (a2) v |= 0x80;
    put16(a0, v);
    a0++;
    uint16_t d0 = r16(RAM_OFF_tmtouch1 + a2);
    put16(a0, d0);
    a0++;
    put16(a0, 0xFFFF);
    uint16_t base = a2 ? 0x1A : 0;
    if (r16(RAM_OFF_tmgoalie + a2) != d0) {
        uint32_t t = RAM_OFF_tmgoals + (uint16_t)(base + d0);
        w8(t, (uint8_t)(r8(t) + 1));
    }
    d0 = r16(RAM_OFF_tmtouch2 + a2);
    if (!(d0 & 0x8000)) {
        put8(a0, (uint8_t)d0);
        uint32_t t = RAM_OFF_tmassists + (uint16_t)(base + d0);
        w8(t, (uint8_t)(r8(t) + 1));
        a0++;
        d0 = r16(RAM_OFF_tmtouch3 + a2);
        if (!(d0 & 0x8000)) {
            put8(a0, (uint8_t)d0);
            t = RAM_OFF_tmassists + (uint16_t)(base + d0);
            w8(t, (uint8_t)(r8(t) + 1));
        }
    }
    d0 = r16(RAM_OFF_tmgoalie + a1);
    if (!(d0 & 0x8000)) inc16(RAM_OFF_tmgoals + (uint16_t)((a1 ? 0x1A : 0) + d0));   /* (a word: the next player's byte takes the carry) */
    PenGoalStuff(a1, a2);
    printscores1();
    for (int k = 0, p = r16(RAM_OFF_tmsort + a2) >> 1; k < 6; k++, p++) {
        if (g.position[p] <= 0) continue;
        g.pfnc[p] = 0;
        assinsert(p, 8);
    }
    g.collflag = 0;
    g.puckvx[0] = 0;
    g.puckvy[0] = 0;
    g.Xpos[14] = g.Xpos[14] & 0x8000 ? (int16_t)-6 : 6;
    g.pucky[0] = g.pucky[0] & 0x8000 ? (int16_t)0xFEF0 : 0x110;
    g.puckvz = 0x600;
    g.puckz = 0;
    g.puckcross[1] = 0xFFFF;
    g.puckcross[3] = 0xFFFF;
    g.pfnc[14] = 0xFFFF;
    assreplace(o, 0x1A);
    SetSPAFromA(o + 1, (uint16_t)SYM_SPAsiren);
    AddPenalty2(r16(RAM_OFF_tmsort + a2) >> 1, PenGoal);
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
    EndPenaltyShot(x >> 1);
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

/* A 32-bit score (low word, high word) plus v */
static void star_add(uint16_t *lo, uint16_t *hi, uint32_t v)
{
    uint32_t s = (uint32_t)*lo + (uint16_t)v;
    *lo = (uint16_t)s;
    *hi = (uint16_t)(*hi + (v >> 16) + (s >> 16));
}

/* Star score of player x (+$1A for the visitors): his team's goal difference (a 32-bit value), then for a
 * goalie with enough shots against (40 / the period length's low byte: the divider's operands swapped)
 * $7D00 below 3 goals per 100 shots and $124F8 more for none; for a skater $B71B0 for the overtime
 * winner, $2AF8 a goal, $2774 an assist and $0A a shot. Scaled for the 15-bit low word. */
static void star_score(uint16_t x, uint16_t *lo, uint16_t *hi)
{
    uint16_t a2 = 0, a1 = 2;
    if (!((uint16_t)(x - 0x1A) & 0x8000)) { a2 = 2; a1 = 0; }
    uint16_t s2 = r16(RAM_OFF_tmscore + a2), s1 = r16(RAM_OFF_tmscore + a1);
    *lo = (uint16_t)(s2 - s1);
    *hi = s2 < s1 ? 0xFFFF : 0;
    if ((uint16_t)(x - g.stargoalieend) & 0x8000) {
        uint16_t d1 = DivX8U(0x28, GetPeriodTime());
        uint16_t shots = g.tmpshots[x];
        if (!((uint16_t)(shots - d1) & 0x8000)) {
            uint16_t r0 = Div16U(Mul8U(g.tmgoals[x], 0x64), shots);
            if ((uint16_t)(r0 - 3) & 0x8000) {
                star_add(lo, hi, 0x7D00);
                if (!r0) star_add(lo, hi, 0x124F8);
            }
        }
    } else {
        if (x == g.otgoalscorer) star_add(lo, hi, 0xB71B0);
        star_add(lo, hi, Mul16U(g.tmgoals[x], 0x2AF8));
        star_add(lo, hi, Mul16U(g.tmassists[x], 0x2774));
        star_add(lo, hi, Mul16U(g.tmpshots[x], 0x000A));
    }
    *hi = (uint16_t)((uint16_t)(*hi << 1) + (*lo >> 15));
    *lo &= 0x7FFF;
}

/* Score lo / hi above the star's (signed words, high first) */
static int star_above(uint16_t lo, uint16_t hi, const uint16_t *s)
{
    if (hi != s[1]) return (int16_t)hi > (int16_t)s[1];
    return (int16_t)lo > (int16_t)s[0];
}

/* Choose the three stars: every player's star score, the best three kept in order (the overtime winner
 * read from the word before the score summary, the faceoff spot: a quirk of the original) */
static void choose_stars(void)
{
    g.star1score[0] = g.star1score[1] = 0;
    g.star2score[0] = g.star2score[1] = 0;
    g.star3score[0] = g.star3score[1] = 0;
    g.otgoalscorer = 0xFFFF;
    uint32_t a1 = ScoreSum - 6;
    if ((m16(a1) >> 14 & 3) == 3) {
        g.otgoalscorer = m8(a1 + 3);
        if (m8(a1 + 2) & 0xF0) g.otgoalscorer = (uint16_t)(g.otgoalscorer + 0x1A);
    }
    g.starplayerend = GetPlayerCount(0);
    g.stargoalieend = GetGoalieCount(0);
    uint16_t x = 0;
    do {
        if ((uint16_t)(x - g.starplayerend) & 0x8000) {
            uint16_t lo, hi;
            star_score(x, &lo, &hi);
            uint16_t p = x;
            int slot = star_above(lo, hi, g.star1score) ? 1 : star_above(lo, hi, g.star2score) ? 2 : star_above(lo, hi, g.star3score) ? 3 : 0;
            if (slot == 1) {
                g.starswapplayer = g.star1;
                g.starswapscore[0] = g.star1score[0];
                g.starswapscore[1] = g.star1score[1];
                g.star1 = p;
                g.star1score[0] = lo;
                g.star1score[1] = hi;
                p = g.starswapplayer;
                lo = g.starswapscore[0];
                hi = g.starswapscore[1];
            }
            if (slot == 1 || slot == 2) {
                g.starswapplayer = g.star2;
                g.starswapscore[0] = g.star2score[0];
                g.starswapscore[1] = g.star2score[1];
                g.star2 = p;
                g.star2score[0] = lo;
                g.star2score[1] = hi;
                p = g.starswapplayer;
                lo = g.starswapscore[0];
                hi = g.starswapscore[1];
            }
            if (slot) {
                g.star3 = p;
                g.star3score[0] = lo;
                g.star3score[1] = hi;
            }
        }
        x++;
        if ((uint16_t)(g.starplayerend - 0x1A) & 0x8000 && !((uint16_t)(x - 0x1A) & 0x8000)) {
            g.starplayerend = (uint16_t)(GetPlayerCount(2) + 0x1A);
            g.stargoalieend = (uint16_t)(GetGoalieCount(2) + 0x1A);
        }
    } while ((uint16_t)(x - 0x34) & 0x8000);
}

/* Stars of the Game: a frame with the three stars' teams and names */
void ShowThreeStars(void)
{
    print(SYM_ShowStarsPos);
    eraser(0x13, 8, g.blankchar);
    print(SYM_ShowStarsPos2);
    Framer(0x15, 7);
    print(SYM_StarsOfTheGame);
    choose_stars();
    uint16_t *stars[3] = { &g.star1, &g.star2, &g.star3 };
    g.printy = 5;
    for (int k = 0; k < 3; k++) {
        if (k) g.printy++;
        g.printx = 0x0D;
        uint16_t d3 = 0, a2 = 0;
        if (!((uint16_t)(*stars[k] - 0x1A) & 0x8000)) {
            *stars[k] = (uint16_t)(*stars[k] - 0x1A);
            d3 = 1;
            a2 = 2;
        }
        printteamname(d3);
        g.printy = (uint16_t)(5 + k);
        g.printx = 0x11;
        print(PlayerInitialName(a2, *stars[k]));
    }
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

/* Clear the message rows */
void ClearMessageArea(void) { ClearTextMapBottom(); }

/* Wait x frames, unless a pad press has ended the title */
void TitleWait(uint16_t x)
{
    if (g.titleskip) return;
    WaitFrames_Inc(x, 0);
}

/* Draw sprite a of the title set at its position from the _1 / _2 tables */
void DrawTitleSprite(uint16_t a)
{
    uint16_t x = rom16(SYM_DrawTitleSprite_1 + (uint16_t)(a << 1));
    uint16_t y = rom16(SYM_DrawTitleSprite_2 + (uint16_t)(a << 1));
    DrawSprite(0x3000, x, y, a, titlesprbuf);
}

/* Clear the big text letter tiles (bigtextbuf+1, $300 bytes) and part of its map (screenmap+$28C) */
void ClearBigText(void)
{
    MemFill(0x7E0000 + RAM_OFF_bigtextbuf + 1, 0x0300, 0);
    MemFill(0x7E0000 + RAM_OFF_screenmap + 0x28C, 0x0080, 0);
}

/* Send the stats page rows shown (from listtop, listrows + 1 of them) to VRAM $0180 */
void SendGameStatsPage(void)
{
    uint16_t t = (uint16_t)(g.listtop[0] << 8 | g.listtop[0] >> 8);
    uint32_t p0 = (statspage & 0xFF0000) | (uint16_t)((t >> 1) + (uint16_t)statspage + (t & 1));   /* (lsr's carry) */
    uint16_t n = (uint16_t)(g.listrows[0] + 1);
    DMAToVRAM(&p0, (uint16_t)((n << 8 | n >> 8) >> 1), 0x0180, 0);
}

/* Draw the logo of team a as sprites at x, y with its LogoPaletteTable palette */
void DrawLogoSprite(uint16_t a, uint16_t x, uint16_t y)
{
    uint32_t e = SYM_LogoPaletteTable + (uint16_t)(a << 2);
    SetColours((uint32_t)rom16(e) | (uint32_t)rom16(e + 2) << 16, 0x10, 0xA0);
    DrawSprite(0x2400, x, y, a, logosprbuf);
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

/* Blank the 6 x 6 picture area at column x, row y of the text map */
void ClearPictureBox(uint16_t x, uint16_t y)
{
    uint32_t s = (uint32_t)(uint16_t)(y << 5) + x + (y >> 11 & 1);   /* (the shifts' carries) */
    uint16_t t = (uint16_t)s;
    uint16_t p1 = (uint16_t)((uint16_t)(t << 1) + (uint16_t)(0x7E0000 + RAM_OFF_textmap) + (t >> 15));
    for (int r = 0; r < 6; r++) {
        MemFill(0x7E0000 | p1, 0x0C, (uint16_t)(g.blankchar + g.charofs));
        p1 = (uint16_t)(p1 + 0x40);
    }
}

/* The setup screen's tiles: the blank tile $200, the small font after it (unpacked, its VRAM cleared
 * first), the frame tiles after the font and the score tiles 4 further */
void InitSetupChars(void)
{
    g.blankchar = 0x0200;
    g.smallfontchars = 0x0201;
    FillVRAM(0, 0x20, 0x3000, 0);
    uint16_t n = Decompress(SYM_SmallFontTiles, unpackbuf);
    uint32_t s = (uint32_t)(n >> 5) + g.smallfontchars + (n >> 4 & 1);   /* (lsr's carry) */
    g.Framercset = (uint16_t)s;
    g.scorechars = (uint16_t)(g.Framercset + 4 + (s >> 16 & 1));
}

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

/* Send the big text map (screenmap from +6: a 32 x 2 part at row printy) and the letter tiles to VRAM */
void SendBigText(void)
{
    w16(RAM_OFF_screenmap + 6, 0x20);
    w16(RAM_OFF_screenmap + 8, 0x20);
    g.MapVRAM = 0x0800;
    MapPartToVRAM(0x7E0000 + RAM_OFF_screenmap + 6, 0, g.printy, 0, g.printy, 0x20, 2);
    uint32_t p0 = 0x7E0000 + RAM_OFF_bigtextbuf;
    DMAToVRAM(&p0, (uint16_t)((uint16_t)(g.bigtextchar + 1) << 4), 0x4000, 0);
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

/* The x offset of title path step a: sin(a) times a growing amplitude (a * 10 / $50 + 1), then a straight
 * run */
uint16_t TitlePathX(uint16_t a)
{
    if (a >= 0x480) return (uint16_t)(Div16U((uint16_t)Mul16x8U((uint16_t)(a - 0x480), 0xC6), 0x80) + 0x66);
    uint16_t ang = (uint16_t)(Div16U((uint16_t)Mul16x8U(a, 0x0A), 0x50) + 1);
    return (uint16_t)(Mul16x8XS(Sin(a), ang) >> 8);
}

/* The MultiPlayer 5 warnings: the mouse, the Super Scope, the socket */
void Multi5MouseMsg(void)
{
    print(SYM_TheMultiPlayer5IsNotTxt2);
    print(SYM_DesignedForUseWithTheTxt2);
    print(SYM_SuperNesMouseTxt);
    SendTextMap();
}
/* The warning that the MultiPlayer 5 is not designed for use with the Super Scope */
void Multi5ScopeMsg(void)
{
    print(SYM_TheMultiPlayer5IsNotTxt);
    print(SYM_DesignedForUseWithTheTxt);
    print(SYM_SuperNesSuperScopeTxt);
    SendTextMap();
}
/* The warning that the MultiPlayer 5 must be connected to controller socket 2 */
void Multi5SocketMsg(void)
{
    print(SYM_TheMultiPlayer5MustBeTxt);
    print(SYM_ConnectedToControllerTxt);
    print(SYM_Socket2Txt);
    SendTextMap();
}

/* Unpack the six matchup players' photos of team x (0 home, 2 visitors) into the $7F photo buffer (visitors
 * at $2D00, home at $5100, each at its MatchupPhotoOffsets place), unless that team's are already there */
void LoadMatchupPhotos(uint16_t x)
{
    uint16_t a2 = x;
    if (r16(RAM_OFF_HomeTeam + a2) == r16(RAM_OFF_phototeams + a2)) return;
    for (int k = 0x0A; k >= 0; k -= 2) {
        uint16_t d0 = GetMatchupPlayer((uint16_t)(k >> 1), a2);
        uint16_t team = r16(RAM_OFF_HomeTeam + a2);
        w16(RAM_OFF_phototeams + a2, team);
        uint16_t e = (uint16_t)((uint16_t)(Mul8U(team, 0x1A) + d0) << 2);
        uint32_t p0 = (uint32_t)rom16(SYM_PlayerPhotoTable + e) | (uint32_t)rom16(SYM_PlayerPhotoTable + e + 2) << 16;
        uint16_t r0 = Decompress(p0, unpackbuf);
        uint16_t p1 = (uint16_t)((a2 ? 0x2D00 : 0x5100) + rom16(SYM_MatchupPhotoOffsets + k));
        LinearToPlanar(unpackbuf, p1, r0);
    }
}

/* Unpack the rink map to rinkmapbuf and paste the home team's centre ice logo map (CenterIceTable+4: its
 * width, height, then the tiles from +6) into its middle, a row higher when 6 or more rows tall */
void BuildRinkMap(void)
{
    Decompress(SYM_IceRinkMap, rinkmapbuf);
    uint16_t x = (uint16_t)(g.HomeTeam << 3);
    uint32_t p0 = (uint32_t)rom16(SYM_CenterIceTable + 4 + x) | (uint32_t)rom16(SYM_CenterIceTable + 6 + x) << 16;
    uint32_t b0 = p0 & 0xFF0000, b1 = rinkmapbuf & 0xFF0000;
    uint16_t p1 = (uint16_t)((uint16_t)(rinkmapbuf + 0x10B6) - m16(p0));
    uint16_t r3 = (uint16_t)(m16(p0) << 1);
    uint16_t lo = (uint16_t)(p0 + 2);
    uint16_t y = m16(b0 | lo);
    if (y >= 6) p1 = (uint16_t)(p1 - 0x60);
    lo = (uint16_t)(lo + 4);
    do {
        uint16_t k = 0;
        do {
            uint32_t s = (uint32_t)m16(b0 | lo) + g.centericechars;
            mset16(b1 | p1, (uint16_t)(s + 0x1400 + (s >> 16)));   /* (the first add's carry) */
            k += 2;
            lo += 2;
            p1 += 2;
        } while (k != r3);
        p1 = (uint16_t)(p1 + 0x60 - r3);
    } while (--y);
}

/* Draw string [a1] with the small letter sprites from x, y: letters, digits and '.' (2 wide), a space 3
 * wide, other characters skipped */
void PrintSmallLetterSprites(uint16_t x, uint16_t y, uint32_t a1)
{
    g.letterx[0] = x;
    g.lettery[0] = y;
    for (uint16_t k = 2; k != m16(a1); k++) {
        uint16_t c = m8(a1 + k), w;
        if (c == 0x20) w = 3;
        else {
            uint16_t f;
            if (c == 0x2E) f = 0x24;
            else if (c >= 0x30 && c < 0x3A) f = (uint16_t)(c - 0x30 + 0x1A);
            else if (c >= 0x61 && c < 0x7B) f = (uint16_t)(c - 0x61);
            else if (c >= 0x41 && c < 0x5B) f = (uint16_t)(c - 0x41);
            else continue;
            DrawSprite(0x2000, g.letterx[0], g.lettery[0], f, SYM_SmallLetterSprites);
            w = f == 0x24 ? 2 : 4;
        }
        g.letterx[0] = (uint16_t)(g.letterx[0] + w);
    }
}

/* Player stats: the header (skaters' or goalies') and its rows to VRAM, then the visible rows of the stats
 * page from row curitem */
void DrawPlayerStatsPage(void)
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

/* The line editor's background: the top cleared, the frame outline, the background map into text map 2 */
void DrawEditLinesBack(void)
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

/* Team d1 on the player select screen at printx: its palette (colours $70 for the home team, unless both
 * teams are the same, else $60), its block map (with redrawblocks) and its city name in the big font */
void DrawSelectTeam(uint16_t d1)
{
    g.bigtextchar = 1;
    uint16_t r3 = 0x1800, y = 0x60;
    if (d1 == g.HomeTeam) {
        r3 = 0x1C00;
        y = 0x70;
        g.bigtextchar = 0x18;
    }
    for (;;) {
        uint32_t e = SYM_TeamBlockPalettes + (uint16_t)(d1 << 2);
        SetColours((uint32_t)rom16(e) | (uint32_t)rom16(e + 2) << 16, 0x10, y);
        if (g.HomeTeam != g.VisTeam[0] || r3 == 0x1800) break;
        r3 = 0x1800;
        y = 0x60;
    }
    if (g.redrawblocks) {
        uint32_t a2 = SYM_TeamBlocksMap + 6;
        uint32_t s = (uint32_t)(uint16_t)(g.printy << 5) + g.printx + (g.printy >> 11 & 1);   /* (the shifts' carries) */
        uint16_t t = (uint16_t)s;
        uint16_t x = (uint16_t)((uint16_t)(t << 1) + g.printm + (t >> 15));
        uint16_t k = 0;
        for (int row = 0; row < 2; row++)
            for (int n = 0; n < 9; n++, k += 2, x += 2)
                mset16(0x7E0000 + RAM_OFF_textmap + (row ? 0x2E : 0) + x,
                       (uint16_t)(m16(a2 + k) + g.scorechars + g.printfontset + r3));
    }
    uint16_t px = g.printx;
    w16(RAM_OFF_jmlvector, 0x005C);
    w16(RAM_OFF_jmlvector + 1, (uint16_t)SYM_BigLetterSelect);
    w16(RAM_OFF_jmlvector + 3, SYM_BigLetterSelect >> 16);
    RenderBigText((SYM_TeamPlaceNames & 0xFF0000) | rom16(SYM_TeamPlaceNames + (uint16_t)(d1 << 1)));
    g.printx = px;
}

/* The width of letter c (and its frame, f; -1 for a space) in the big sprite font */
static uint16_t bigsprite_letter(uint16_t c, uint16_t *f)
{
    if (c == 0x20) { *f = 0xFFFF; return 5; }
    if (c == 0x2E) *f = 0x1A;
    else if (c == 0x40) *f = 0x1B;
    else if (c >= 0x30 && c < 0x3A) *f = (uint16_t)(c - 0x30 + 0x1C);
    else *f = (uint16_t)((c & 0xDF) - 0x41);
    return rom8(SYM_LetterWidths + *f);
}

/* The letter loop of PrintTeamSprites: the width of string s (to its 0; a space first has none; '.' is 3
 * wide), letterx moved to centre it (r3h 0 or a2 negative), or to end at $F8 (a2 0), then each letter
 * drawn (palette $3800, on the horizontal blocks $3E00 home / $3C00 visitors) */
void PrintTeamSprites_go(uint32_t s, uint16_t r3, uint16_t r3h, uint16_t a2)
{
    uint16_t f;
    for (uint16_t y = 0;; y++) {
        uint16_t c = m8(s + y);
        if (!c) break;
        if (c == 0x20 && !y) continue;
        r3 = (uint16_t)(r3 + (c == 0x2E ? 3 : bigsprite_letter(c, &f)));
    }
    if (!r3h || (a2 & 0x8000)) g.letterx[0] = (uint16_t)(g.letterx[0] - (r3 >> 1));
    else if (!a2) g.letterx[0] = (uint16_t)(0xF8 - r3);
    for (uint16_t y = 0;; y++) {
        uint16_t c = m8(s + y);
        if (!c) return;
        if (c == 0x20 && !y) continue;
        uint16_t w = bigsprite_letter(c, &f);
        if (f != 0xFFFF) {
            uint16_t attr = !g.horblocks ? 0x3800 : a2 ? 0x3C00 : 0x3E00;
            DrawSprite(attr, g.letterx[0], g.lettery[0], (uint16_t)(f + 929 - 1), SYM_SpriteFrames);
        }
        g.letterx[0] = (uint16_t)(g.letterx[0] + w);
    }
}

/* Draw the place name of team d1, or string [a1] (made to end at its length) when d1 is negative, as big
 * letter sprites at x / y */
void PrintTeamSprites(uint16_t x, uint16_t y, uint16_t d1, uint32_t a1, uint16_t a2)
{
    g.letterx[0] = x;
    g.lettery[0] = y;
    if (!(d1 & 0x8000)) {
        uint32_t bank = SYM_PrintTeamSprites & 0xFF0000;
        PrintTeamSprites_go(bank | db16(bank >> 16, (uint16_t)(SYM_TeamPlaceNames + (uint16_t)(d1 << 1))), 0, 0, a2);
        return;
    }
    uint32_t bank = a1 & 0xFF0000;
    uint16_t n = m16(a1);
    uint16_t d = (uint16_t)(a1 + 2);
    mset16(bank + d + (uint16_t)(n - 2), 0);
    PrintTeamSprites_go(bank | d, 0, 1, a2);
}

/* Draw string a1 (from its first byte) as big letter sprites centred on x, at y */
void PrintStringSprites(uint16_t x, uint16_t y, uint32_t a1)
{
    g.letterx[0] = x;
    g.lettery[0] = y;
    PrintTeamSprites_go(a1, 0, 1, 0xFFFF);
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

/* The title logo's map header at titlelogobuf from the map at p0 (width, height, $10, then each tile + 1,
 * palette bits $70 when a tile's attribute byte has bits $1C, else $60); *w / *h its size */
static void titlelogo_map(uint32_t p0, uint16_t *w, uint16_t *h)
{
    w16(RAM_OFF_LogoPalBits, 0x60);
    *w = m16(p0);
    *h = m16(p0 + 2);
    w16(RAM_OFF_titlelogobuf, *w);
    w16(RAM_OFF_titlelogobuf + 2, *h);
    w16(RAM_OFF_titlelogobuf + 4, 0x10);
    uint16_t d1 = (uint16_t)(Mul8U(*w, *h) + 6);
    uint16_t y = 6, x = 6;
    do {
        uint8_t t = (uint8_t)(m8(p0 + y) + 1);
        w8(RAM_OFF_titlelogobuf + x, t);
        y++;
        if (t && (m8(p0 + y) & 0x1C)) g.LogoPalBits[0] = 0x70;
        y++;
        x++;
    } while (x < d1);
}

/* The title logo's graphics: n bytes at unpackbuf to a byte per pixel (palette bits added) from
 * titlelogobuf+$40, 32 source bytes at a time; the bytes to send */
static uint16_t titlelogo_tiles(uint16_t n)
{
    uint16_t blocks = (uint16_t)((n >> 5) + 1);
    for (uint16_t x = 0; x < 0x40; x += 2) w16(RAM_OFF_titlelogobuf + x, 0);
    g.LogoSrcOfs = 0;
    g.LogoDstOfs = 0x40;
    do {
        uint16_t x = g.LogoDstOfs, y = g.LogoSrcOfs;
        for (int k = 0; k < 0x20; k++, x += 2, y++) {
            uint8_t b = m8(unpackbuf + y);
            w8(RAM_OFF_titlelogobuf + 1 + x, (uint8_t)((b & 0x0F) | g.LogoPalBits[0]));
            w8(RAM_OFF_titlelogobuf + x, (uint8_t)((b >> 4) | g.LogoPalBits[0]));
        }
        g.LogoSrcOfs = (uint16_t)(g.LogoSrcOfs + 0x20);
        g.LogoDstOfs = (uint16_t)(g.LogoDstOfs + 0x40);
    } while (--blocks);
    return (uint16_t)((uint16_t)(n << 1) + 0x40);
}

/* The centre ice logo of team a in the title's mode 7 map: the map buffer sent, then the logo's map centred
 * at ($42, $40), its graphics unpacked, converted and sent */
void LoadTitleLogo(uint16_t a)
{
    g.MapVRAM = 0;
    Mode7MapToVRAM(0x7E0000 + RAM_OFF_introbuf, 0x3C, 0x3C);
    uint16_t x = (uint16_t)(a << 3);
    uint32_t p0 = (uint32_t)rom16(SYM_CenterIceTable + 4 + x) | (uint32_t)rom16(SYM_CenterIceTable + 6 + x) << 16;
    uint16_t w, h;
    titlelogo_map(p0, &w, &h);
    g.MapVRAM = 0;
    Mode7MapToVRAM(0x7E0000 + RAM_OFF_titlelogobuf, (uint16_t)(0x42 - (w >> 1)), (uint16_t)(0x40 - (h >> 1)));
    p0 = (uint32_t)rom16(SYM_CenterIceTable + x) | (uint32_t)rom16(SYM_CenterIceTable + 2 + x) << 16;
    uint16_t n = titlelogo_tiles(Decompress(p0, unpackbuf));
    p0 = 0x7E0000 + RAM_OFF_titlelogobuf;
    DMAToVRAMHigh(&p0, n, 0);
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

/* One stat, a column's palette, at printx */
static void stat_cell(uint16_t col, uint16_t px, uint16_t v, uint16_t width)
{
    if (col) SetStatColumnPal(col);
    g.printx = px;
    uint16_t q;
    PrintToStatsPage(PushNumberWrap(v, width, &q));
}

/* The player stats page from row listtop to listend, in statsorder: skaters' goals, assists, points, shots
 * and penalty minutes (the sorted column highlighted), or goalies' goals against, shots and save
 * percentage */
void FillPlayerStatsPage(uint16_t a2)
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

/* Arithmetic shifts of a word right */
static uint16_t asr4(uint16_t v) { return (uint16_t)((int16_t)v >> 4); }

/* One frame of hat k's flight: it moves; in the air it falls (hatvz, its own: launching clears only the
 * first hat's); on the ice it slides, slowing by a sixteenth, until it stops */
static void hat_move(uint16_t x)
{
    if (!(r16(RAM_OFF_hatvx + x) | r16(RAM_OFF_hatvy + x))) return;
    w16(RAM_OFF_hatx + x, (uint16_t)(r16(RAM_OFF_hatx + x) + r16(RAM_OFF_hatvx + x)));
    w16(RAM_OFF_haty + x, (uint16_t)(r16(RAM_OFF_haty + x) + r16(RAM_OFF_hatvy + x)));
    uint16_t z = r16(RAM_OFF_hatz + x);
    if (z) {
        uint16_t a = (uint16_t)(z - (uint16_t)((int16_t)r16(RAM_OFF_hatvz + x) >> 2));
        if (a & 0x8000) { w16(RAM_OFF_hatz + x, 0); return; }
        w16(RAM_OFF_hatz + x, a);
        w16(RAM_OFF_hatvz + x, (uint16_t)(r16(RAM_OFF_hatvz + x) + 6));
        return;
    }
    w16(RAM_OFF_hatvz + x, 0);
    w16(RAM_OFF_hatz + x, 0);
    w16(RAM_OFF_hattimer + x, 0x03E8);
    uint16_t vx = r16(RAM_OFF_hatvx + x);
    if (vx < 0x10 || vx >= 0xFFF0) {
        w16(RAM_OFF_hatvx + x, 0);
        w16(RAM_OFF_hatvy + x, 0);
        return;
    }
    w16(RAM_OFF_hatvx + x, (uint16_t)(vx - asr4(vx)));
    uint16_t vy = r16(RAM_OFF_hatvy + x);
    w16(RAM_OFF_hatvy + x, (uint16_t)(vy - asr4(vy)));
}

/* While the hat trick is on (not on the horizontal rink or in a replay): the ten hats, each launched at
 * random (a chance in 32 a frame) on an arc onto the ice, animated, moved timestep frames and drawn with
 * its shadow */
void UpdateHatTrick(void)
{
    if (g.sfhormode || (g.sflags & 0x0800) || !g.hattrick) return;
    for (uint16_t x = 0x12; !(x & 0x8000); x -= 2) {
        if (!r16(RAM_OFF_hatstep + x)) {
            if (RandomWord() & 0x07C0) continue;
            uint16_t r = RandomWord() & 0x3F;
            uint16_t d0 = (uint16_t)((uint16_t)((r >> 1) + r + (r & 1) + 0x50) << 4);   /* (lsr's carry) */
            uint16_t d1 = (uint16_t)((uint16_t)((RandomWord() & 0x3F) + 0x60) << 4);
            w16(RAM_OFF_haty + x, (uint16_t)((RandomWord() & 0xFF) << 4));
            uint16_t d2 = (uint16_t)((RandomWord() & 0x3F) + 0x78);
            w16(RAM_OFF_hatvy + x, DivX8S((uint16_t)(d1 - r16(RAM_OFF_haty + x)), d2));
            uint16_t hx = d0 & 0x0100 ? 0x1100 : 0xFF00;
            w16(RAM_OFF_hatx + x, hx);
            w16(RAM_OFF_hatvx + x, DivX8S((uint16_t)(d0 - hx), d2));
            uint16_t p = Mul8U((uint16_t)(d2 >> 2), (uint16_t)(d2 >> 2));
            w16(RAM_OFF_hatz + x, (uint16_t)((uint16_t)(p << 1) + p + (p >> 15)));
            uint16_t kind = (uint16_t)(RandomWord() >> 2 & 3);
            if (kind) kind--;
            w16(RAM_OFF_hatkind + x, (uint16_t)(kind * 3));
            g.hatvz[0] = 0;
            w16(RAM_OFF_hatstep + x, 1);
            w16(RAM_OFF_hattimer + x, rom16(SYM_UpdateHatTrick_9 + x));
        } else {
            uint16_t t = (uint16_t)(r16(RAM_OFF_hattimer + x) - g.timestep);
            if (t & 0x8000) {
                w16(RAM_OFF_hattimer + x, (uint16_t)(t + rom16(SYM_UpdateHatTrick_9 + x)));
                uint16_t s = (uint16_t)(r16(RAM_OFF_hatstep + x) + 1);
                w16(RAM_OFF_hatstep + x, s == 4 ? 1 : s);
            } else w16(RAM_OFF_hattimer + x, t);
            for (uint16_t y = g.timestep; y; y--) hat_move(x);
        }
        uint16_t f = (uint16_t)(r16(RAM_OFF_hatkind + x) + r16(RAM_OFF_hatstep + x) + 967 - 2);
        uint16_t hy = r16(RAM_OFF_haty + x), hx = r16(RAM_OFF_hatx + x);
        DrawSprite(0x2000, asr4(hx), asr4((uint16_t)(hy - (r16(RAM_OFF_hatz + x) >> 4))), f, SYM_SpriteFrames);
        DrawSprite(0x2000, asr4(r16(RAM_OFF_hatx + x)), asr4(r16(RAM_OFF_haty + x)), 976 - 1, SYM_SpriteFrames);
    }
}

/* Title: an empty sprite frame, then a 35-frame wait */
void TitleBlankSprites(void)
{
    SpriteFrameStart();
    SpriteFrameEnd();
    TitleWait(0x23);
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

/* The title logo path, $500 steps: each step's mode 7 scale, x offset and y scroll */
void BuildTitlePath(void)
{
    for (uint16_t y = 0, x = 0; y < 0x500; y++, x += 2) {
        w16(RAM_OFF_titlescale + x, TitlePathScale(y));
        w16(RAM_OFF_titlexofs + x, TitlePathX(y));
        w16(RAM_OFF_titleyscroll + x, TitlePathY(y));
    }
}

/* The licence screen: the NHL shield, the NHLPA logo and the NHL line at y $B0 */
void DrawLicenceNHL(void)
{
    SpriteFrameStart();
    DrawTitleSprite(9);
    DrawTitleSprite(10);
    DrawSprite(0x2800, 0x3C, 0xB0, 0x0B, titlesprbuf);
    SpriteFrameEnd();
}

/* The licence screen with the NHL line moved up and "Players Association" under it */
void DrawLicenceNHLPA(void)
{
    SpriteFrameStart();
    DrawTitleSprite(9);
    DrawTitleSprite(10);
    DrawSprite(0x2800, 0x3C, 0xA0, 0x0B, titlesprbuf);
    DrawSprite(0x2800, 0x3C, 0xD0, 0x0C, titlesprbuf);
    SpriteFrameEnd();
}

/* The two copyright lines */
void PrintCopyright(void)
{
    PrintStringSprites(0x80, 0xB8, SYM_CopyrightLines);
    PrintStringSprites(0x80, 0xC8, (SYM_CopyrightLines & 0xFF0000) | (uint16_t)(SYM_CopyrightLines + 23));
}

/* The short name of team a2's player in line slot y (a3) in small letter sprites centred on x, at printy */
void PrintLineSlotName(uint16_t x, uint16_t y, uint32_t a3, uint16_t a2)
{
    uint32_t a1 = FormatPlayerNumInitial(a2, (uint16_t)(m8(a3 + y) - 1));
    uint16_t w = (uint16_t)((uint16_t)((uint16_t)(m16(a1) - 2) >> 1) << 2);
    PrintSmallLetterSprites((uint16_t)(x - w), g.printy, a1);
}

/* The home team's centre ice logo tiles: unpacked, made planar after the source, sent to VRAM after the
 * rink tiles (faceoffvrcset the tile after them), then the rink map */
void LoadCenterIce(void)
{
    uint16_t x = (uint16_t)(g.HomeTeam << 3);
    uint32_t p0 = (uint32_t)rom16(SYM_CenterIceTable + x) | (uint32_t)rom16(SYM_CenterIceTable + 2 + x) << 16;
    uint16_t n = Decompress(p0, unpackbuf);
    LinearToPlanar(unpackbuf, n, n);
    p0 = (unpackbuf & 0xFF0000) | n;
    uint32_t s = (uint32_t)(n >> 5) + g.centericechars + (n >> 4 & 1);   /* (lsr's carry) */
    g.faceoffvrcset = (uint16_t)s;
    uint16_t y = (uint16_t)((uint16_t)(g.centericechars << 4) + 0x1000 + (g.centericechars >> 12 & 1));   /* (asl's carry) */
    DMAToVRAM(&p0, n, y, 0);
    BuildRinkMap();
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
    FillPlayerStatsPage(a2);
}

/* The credits, a step when its wait is over: the title and name scroll up, hold, then the next name (or
 * title, or the end) */
void RollCredits(void)
{
    if (g.FrameCount < g.CreditTime || g.CreditStep == 3) return;
    uint16_t a;
    if (g.CreditStep == 0) {
        if ((uint16_t)(g.CreditTitleY - 2 - 0xB9) & 0x8000) goto hold;
        g.CreditTitleY -= 2;
    up:
        g.CreditNameY -= 2;
        a = 2;
    } else if (g.CreditStep == 1) {
        if (!((uint16_t)(g.CreditNameY - 2 - 0xC9) & 0x8000)) goto up;
    hold:
        g.CreditStep = 2;
        a = 0x46;
    } else {
        g.CreditNameIdx++;
        if (rom16(SYM_CreditNames + (uint16_t)(g.CreditNameIdx << 1))) {
            g.CreditStep = 1;
            g.CreditNameY = 0xE0;
            a = 0x0F;
        } else {
            g.CreditTitleIdx++;
            if (!rom16(SYM_CreditTitles + (uint16_t)(g.CreditTitleIdx << 1))) {
                g.CreditStep = 3;
                g.CreditTitleY = 0xE0;
                g.CreditNameY = 0xF0;
                a = 0x2D;
            } else {
                g.CreditNameIdx++;
                g.CreditTitleY = 0xE0;
                g.CreditNameY = 0xF0;
                g.CreditStep = 0;
                a = 0x0F;
            }
        }
    }
    g.CreditTime = (uint16_t)(a + g.FrameCount);
    SpriteFrameStart();
    DrawTitleSprite(9);
    DrawTitleSprite(10);
    PrintStringSprites(0x80, g.CreditTitleY, (SYM_CreditTitles & 0xFF0000) | rom16(SYM_CreditTitles + (uint16_t)(g.CreditTitleIdx << 1)));
    PrintStringSprites(0x80, g.CreditNameY, (SYM_CreditNames & 0xFF0000) | rom16(SYM_CreditNames + (uint16_t)(g.CreditNameIdx << 1)));
    SpriteFrameEnd();
}


/* The frame step sizes from the frames elapsed d7: timestep, impactstep (x2), velstep (x16), gravstep (x96) */
static void frame_steps(uint16_t d7)
{
    g.d7 = d7;
    g.timestep = d7;
    g.impactstep = (uint16_t)(d7 << 1);
    g.velstep = (uint16_t)(d7 << 4);
    g.gravstep = (uint16_t)(d7 << 5);
    g.gravstep = (uint16_t)((uint16_t)(d7 << 6) + g.gravstep + (g.gravstep >> 15));   /* (asl's carry) */
}

/* The highlight copy of the game state: OOlist+$10 .. +$780 and ScoreSumbytes into hlsavebuf, no goalies,
 * no pads, no penalties */
static void hl_save(void)
{
    uint16_t x;
    for (x = 0; x < 0x0781; x += 2) w16(RAM_OFF_hlsavebuf + x, r16(RAM_OFF_OOlist + 0x10 + x));
    w16(RAM_OFF_hlsavebuf + x, g.ScoreSumbytes[0]);
    g.tmgoalie[0] = g.tmgoalie[1] = 0;
    g.teampads[0] = g.teampads[1] = 0;
    for (x = 0; x < 0x18; x += 2) w16(RAM_OFF_playerpad + x, 0xFFFF);
    ClearPenaltyBuffer();
}
/* (SaveRegsStartHL2: the game state back from hlsavebuf after an other game's highlight, as hl_save
 * left it) */
static void hl_restore(void)
{
    uint16_t x;
    for (x = 0; x < 0x0781; x += 2) w16(RAM_OFF_OOlist + 0x10 + x, r16(RAM_OFF_hlsavebuf + x));
    g.ScoreSumbytes[0] = r16(RAM_OFF_hlsavebuf + x);
}

/* A tied highlight game decided at random (a goal to one side), then over (period 5) */
static void hl_decide(uint16_t a0)
{
    uint16_t s1 = r16(RAM_OFF_gss1 + a0);
    if (s1 == r16(RAM_OFF_gss2 + a0)) {
        uint16_t r = RandomWord() & 1;
        uint32_t s = (uint32_t)s1 + r;
        w16(RAM_OFF_gss1 + a0, (uint16_t)s);
        w16(RAM_OFF_gss2 + a0, (uint16_t)((r ^ 1) + r16(RAM_OFF_gss2 + a0) + (s >> 16)));   /* (the first add's carry) */
    }
    w16(RAM_OFF_gsper + a0, 5);
}

/* Team a2 for a highlight: full energy, its players on the ice, no touches */
static void hl_team(uint16_t a2)
{
    reenergizeteam(a2);
    setpersonel(a2);
    forcepldata(a2);
    w16(RAM_OFF_tmtouch1 + a2, 0xFFFF);
    w16(RAM_OFF_tmtouch2 + a2, 0xFFFF);
    w16(RAM_OFF_tmtouch3 + a2, 0xFFFF);
}

/* The highlight of playoff game a0 / 2 (d1): its ticker box and "Highlight from" for 3 seconds (Start skips
 * it, and the rest), then the game played from a faceoff with the cpu on both sides for a few seconds of
 * clock (a button ends it, a tied game given a goal), the score kept, the menu screen and music back. Over
 * at the end of the period (4) unless still tied (then again); the game state is put back. */
void StartHL2(uint16_t a0, uint16_t d1)
{
    for (;;) {
        if (g.sflags3[0] & 8) { hl_decide(a0); goto skipped; }
        hl_save();
        NewTicker3(d1);
        if (g.sflags3[0] & 8) goto skip_rest;
        g.printx = 4;
        g.printy = 0x14;
        Framer(0x18, 3);
        g.printx++;
        g.printy -= 2;
        print(SYM_HighlightFrom);
        if (waitxsr(0xB4).held & 0x1000) goto skip_rest;
        g.padchangeany = 0;
        FadeOut();
        WaitFadeOut();
        forceblack();
        FadeOutSound();
        if (g.sndbank[0] != SoundBankGame) SPCGameUpload();
        g.HomeTeam = r16(RAM_OFF_gst1 + a0);
        g.VisTeam[0] = r16(RAM_OFF_gst2 + a0);
        clearTeamStats();
        restoreteams();
        LoadTeamLines(0);
        LoadTeamLines(2);
        reenergizeteam(0);
        reenergizeteam(2);
        LoadCenterIce();
        for (int x = 8; x >= 0; x -= 2) {
            w16(RAM_OFF_c1playernum + x, 0xFFFF);
            w16(RAM_OFF_padside + x, 0xFFFF);
        }
        g.cont1team = g.cont2team = g.cont3team = g.cont4team = g.cont5team[0] = 0;
        g.gameclock[0] = (uint16_t)(randoma8(0x78) + 0x78);
        g.gsp = (uint16_t)(r16(RAM_OFF_gsper + a0) - 1);
        g.tmscore[0] = r16(RAM_OFF_gss1 + a0);
        g.tmscore[1] = r16(RAM_OFF_gss2 + a0);
        g.gmode = 0x0010;
        g.gmclock = 0;
        if (g.gsp & 1) g.gmode |= gmdir;
        g.sflags3[0] = 0;
        g.sflags = 0x0400;
        g.sfhormode = 0;
        g.iflags = 0;
        g.RefCnt = -1;
        g.puckcross[1] = 0xFFFF;
        g.puckcross[3] = 0xFFFF;
        setupice_reset();
        ClrHor();
        g.Vpos = 0;
        g.Hpos = 0;
        ResetBench();
        hl_team(0);
        hl_team(2);
        resetplstuff();
        g.hattrick = 0;
        for (uint16_t x = 0; x < 0x18; x += 2) {
            uint16_t p = r16(RAM_OFF_position + x);
            if (!r16(RAM_OFF_pfgoal + x)) p = (uint16_t)(p + 6);
            uint16_t y = (uint16_t)(p << 2);
            w16(RAM_OFF_Xpos + x, rom16(SYM_StartHL2_20 + y));
            w16(RAM_OFF_Ypos + x, rom16(SYM_StartHL2_20 + (uint16_t)(y + 2)));
            w16(RAM_OFF_Xvel + x, 0);
            w16(RAM_OFF_Yvel + x, 0);
        }
        if (g.puckc < 0) g.puckc = (int16_t)(randoma(0x0C) << 1);
        uint16_t x = (uint16_t)g.puckc;
        if (!r16(RAM_OFF_position + x)) g.puckc = (int16_t)(x - 2);
        SprSort();
        uint16_t left = 0xB4;
        g.oldvcount = g.vcount[0];
        for (;;) {
            while (g.vcount[0] == g.oldvcount) hal_wait_interrupt();
            uint16_t v = g.vcount[0];
            frame_steps((uint16_t)(v - g.oldvcount));
            g.lastframevc = v;
            g.oldvcount = v;
            ReadPads();
            if (!(g.sflags & sfpz) && !g.gmclock) {
                for (uint16_t k = g.d7; k; k--) {
                    if (!g.gameclock[0]) continue;
                    uint16_t f = g.gameclock[1];
                    g.gameclock[1] = (uint16_t)(f - 0x0AAA);
                    if (f < 0x0AAA) g.gameclock[0]--;
                }
            }
            uint16_t d7 = g.d7;
            DoGameFrame();
            g.d7 = d7;
            g.timestep = d7;
            if (g.gmclock) {
                left = (uint16_t)(left - d7);
                if (left & 0x8000) break;
            }
            if (!(ReadAnyPad() & 0x9080)) continue;
            if (g.tmscore[0] == g.tmscore[1]) {
                uint16_t t = RandomWord() & 2;
                w16(RAM_OFF_tmscore + t, (uint16_t)(r16(RAM_OFF_tmscore + t) + 1));
            }
            break;
        }
        FadeOutSound();
        w16(RAM_OFF_gss1 + a0, g.tmscore[0]);
        w16(RAM_OFF_gss2 + a0, g.tmscore[1]);
        SetupTeamForIntermission();
        for (uint16_t k = 0; k < 0x18; k += 2) {
            w16(RAM_OFF_Xpos + k, 0xFF10);
            w16(RAM_OFF_Ypos + k, 0);
            w16(RAM_OFF_frame + k, 1);
        }
        FadeOut();
        WaitFadeOut();
        forceblack();
        hl_restore();
        setplayercolors();
        SetHor();
        SpriteFrameStart();
        setvideo();
        SpriteFrameEnd();
        while (g.SoundFadeSpeed || g.MusicOn) hal_wait_interrupt();
        SPCMenuUpload();
        if (g.OptPlayMode && g.OptPlayMode != 4 && g.playoffRoundIdx >= 2) MusicStart(SYM_PlayoffSong);
        else MusicStart(SYM_MenuSong);
        print(SYM_SaveRegsStartHL2Pos);
        Framer(0x16, 7);
        printscores1();
        SetupPauseScreen();
        FadeIn();
        ScreenOn();
        w16(RAM_OFF_gsper + a0, 4);
        if (r16(RAM_OFF_gss1 + a0) == r16(RAM_OFF_gss2 + a0)) continue;
        w16(RAM_OFF_gsper + a0, 5);
        NewTicker3(d1);
        if (waitxsr(0xB4).press & 0x1000) goto skip_on;
        goto skipped;
    skip_rest:
        g.printx = 4;
        g.printy = 0x14;
        eraser(0x18, 3, g.blankchar);
        SendTextMap();
        WaitFrame();
        hl_restore();
        hl_decide(a0);
    skip_on:
        g.sflags3[0] |= 8;
        break;
    skipped:
        g.sflags3[0] &= (uint16_t)~8;
        break;
    }
    LoadCenterIce();
    uint16_t n = Decompress(SYM_FaceOffTiles, unpackbuf);
    uint32_t p0 = unpackbuf;
    uint16_t y = (uint16_t)((uint16_t)(g.faceoffvrcset << 4) + 0x1000 + (g.faceoffvrcset >> 12 & 1));   /* (asl's carry) */
    DMAToVRAM(&p0, n, y, 0);
}

/* The highlight of playoff game a0 / 2 (d1) (the registers kept around it) */
void SaveRegsStartHL2(uint16_t a0, uint16_t d1) { StartHL2(a0, d1); }

/* Ask "exit game?": up / down choose (curitem 0 or 1), A / B take it, Start cancels (0); returns it */
uint16_t AskExitGame(void)
{
    g.curitem[0] = 0;
    for (;;) {
        DrawExitPrompt();
        SendTextMap();
        WaitFrame();
        ReadPads();
        uint16_t d1 = ReadPausePad().press;
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

/* In the demo: Start pauses, any other button ends it (Restart); in mode 4 with pads on teams, Start on
 * any of them pauses from that pad */
static void demo_read(void)
{
    uint16_t any = g.cont1team | g.cont2team | g.cont3team | g.cont4team | g.cont5team[0];
    if (g.OptPlayMode == 4 && any) {
        for (uint16_t x = 0; x < 0x0A; x += 2) {
            if (!r16(RAM_OFF_cont1team + x)) continue;
            if (ReadjoyA(x >> 1).held & 0x1000) { PauseFromPad(x); return; }
        }
        return;
    }
    if (any) return;
    uint16_t d0, d1, d2, d3;
    ReadJoy1(&d0, &d1, &d2, &d3);
    if (d1 & 0x1000) { startpause1(); return; }
    if (!d1) return;
    g.fadestep = 0;
    g.FrameByNumber = 0;
    Restart();
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
        if (IntermissionTick()) goto timeout;
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
/* Goalie control (manual or automatic): A / B / left / right switch it for the pause controller's team */
uint16_t PauseGoalieMode(uint16_t d1)
{
    if (d1 & 0x8380) {
        uint16_t a2 = seta2();
        w16(RAM_OFF_goaliemode1 + a2, r16(RAM_OFF_goaliemode1 + a2) ^ 1);
    }
    return PauseMenuPoll();
}
/* (the goalie: right / A / B the next one, after the last none; left the one before, before the first
 * none, from none the last) */
uint16_t PauseGoalie(uint16_t d1)
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
uint16_t PauseTimeout(uint16_t d1)
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
/* Exit game: A / B ask to be sure (AskExitGame), then the game is abandoned for the option screens
 * (Restart) */
uint16_t PauseExitGame(uint16_t d1)
{
    if (!(d1 & 0x8080) || !AskExitGame()) return PauseMenuPoll();
    g.FrameByNumber = 0;
    Restart();
    return 0;
}
/* Instant replay: A / B fade out to the replay (ReplayMode, the music noted in replaymusic) and back,
 * the frame count kept */
uint16_t PauseReplay(uint16_t d1)
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    g.replaymusic = g.MusicOn;
    FadeOut();
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
    case SYM_PauseGoalieMode: return PauseGoalieMode(d1);
    case SYM_PauseGoalie: return PauseGoalie(d1);
    case SYM_PauseTimeout: return PauseTimeout(d1);
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
    FadeOut();
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
    FadeOut();
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
                if (!g.pstimer) EndPenaltyShot(g.d7 >> 1);   /* (X holds d7) */
            } else {
                for (uint16_t k = g.d7; k; k--) {
                    if (!g.gameclock[0]) continue;
                    uint16_t f = g.gameclock[1];
                    g.gameclock[1] = (uint16_t)(f - 0x0AAA);
                    if (f >= 0x0AAA) continue;
                    g.gameclock[0]--;
                    uint16_t c = (uint16_t)(g.CwdExciteLvl + 0xFFFD);
                    if (!(c & 0x8000)) g.CwdExciteLvl = c;
                    CheckCrowdMeter();
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

/* Back to the rink after a pause screen: the rink map, its tiles, the centre ice logo, the faceoff, logo and
 * referee tiles, the fonts (remapped), the score and team block tiles to VRAM, the screen registers and the
 * palette; then the horizontal rink, the sprites, and the screen faded in */
void ReloadRink(void)
{
    FadeOut();
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
    RemapTiles(unpackbuf, SYM_PixelRemap83, (uint16_t)(n >> 5));
    RemapTiles(SYM_FramerTiles, SYM_PixelRemap87, 4);
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
        d3 = ReadPausePad().held;
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
    d3 = ReadPausePad().held;
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

/* A summary list's rows shown (from listtop, listrows of them, 3 text rows each) to VRAM y, or the none
 * message when the list has `empty` entries */
static void summary_page(uint32_t none, uint16_t empty, uint16_t y)
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

/* The penalty / scoring summary list: Start leaves, up / down scroll it a row (six rows drawn while the IRQ
 * split slides, then five), the arrows redrawn (the penalties' pad read with key repeat) */
static void summary_loop(uint32_t none, int repeat)
{
    for (;;) {
        Pad p = ReadPausePad();
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

/* The penalty summary and the scoring summary screens */
void ShowPenaltySummary(void) { summary_screen(SYM_PenaltiesLabel, 0x58, SYM_PenaltySummaryHead, SYM_NoPenalties, 1); }
void ShowScoringSummary(void) { summary_screen(SYM_ScoringLabel, 0x5E, SYM_ScoringSummaryHead, SYM_NoScoring, 0); }

/* The other scores' rows shown to VRAM $0140, or "no other games" */
void DrawOtherScoresPage(void) { summary_page(SYM_NoOtherGames, 1, 0x0140); }

/* The other scores list: its braces (none above the first game shown, or the user's game first), each
 * frame; Start leaves, up / down scroll a game (the user's game, skipped in the list, stepped over) */
static void otherscores_loop(void)
{
    for (;;) {
        uint16_t d3 = ReadPausePad().held;
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

/* The pause menu's screens: A / B show one, then the rink comes back (A = 1); other buttons go on polling */
static uint16_t pause_screen(uint16_t d1, void (*show)(void))
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    show();
    ReloadRink();
    return PauseSubmenuDone();
}
uint16_t PauseGameStats(uint16_t d1) { return pause_screen(d1, StatsReport); }
uint16_t PausePenaltySummary(uint16_t d1) { return pause_screen(d1, ShowPenaltySummary); }
uint16_t PauseScoringSummary(uint16_t d1) { return pause_screen(d1, ShowScoringSummary); }
uint16_t PauseOtherScores(uint16_t d1) { return pause_screen(d1, ShowScores); }
uint16_t PauseTeamRoster(uint16_t d1) { return pause_screen(d1, TeamRoster); }


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
        Pad p = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x1000) return;
        if (d1 & 0x4000) {
            g.curitem[0] ^= 1;
            DrawPeriodStats();
        }
    }
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
    DrawPlayerStatsPage();
    MenuScreenHDMA();
    ScreenOn();
    for (;;) {
        ReadPads();
        Pad p = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held), d3 = p.held;
        if (d3 & 0x1000) {
            ShowPlayerStats_exit();
            return;
        }
        if (d3 & 0x0800) {
            ClearPlayerStatBraces();
            uint16_t v = (uint16_t)(g.curitem[0] - 1);
            if (!((uint16_t)(v - g.listtop[0]) & 0x8000)) {
                g.curitem[0] = v;
                g.listrows[0]++;
                DrawPlayerStatsPage();
                g.listrows[0]--;
                WaitFrame();
                g.irqvscroll1 = 0x0D;
                do {
                    WaitFrame();
                    g.irqvscroll1 -= 2;
                } while (g.irqvscroll1 != 1);
                DrawPlayerStatsPage();
                WaitFrame();
                g.irqvscroll1 = 0xFFFF;
                continue;
            }
        } else if (d3 & 0x0400) {
            ClearPlayerStatBraces();
            if ((uint16_t)(g.curitem[0] + g.listrows[0] - 1 - g.listend[0]) & 0x8000) {
                g.listrows[0]++;
                DrawPlayerStatsPage();
                g.listrows[0]--;
                WaitFrame();
                g.curitem[0]++;
                g.irqvscroll1 = 1;
                do {
                    WaitFrame();
                    g.irqvscroll1 += 2;
                } while (g.irqvscroll1 != 0x0D);
                DrawPlayerStatsPage();
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
                DrawPlayerStatsPage();
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
            DrawPlayerStatsPage();
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
            DrawPlayerStatsPage();
        }
        WaitFrame();
    }
}

uint16_t PausePeriodStats(uint16_t d1) { return pause_screen(d1, PeriodStatsScreen); }
uint16_t PausePlayerStats(uint16_t d1) { return pause_screen(d1, PlayerStatsScreen); }


/* Add base to every word of the map unpacked at picboxmap (its width and height first, the words from +6) */
static void picboxmap_add(uint16_t base)
{
    uint32_t p1 = 0x7E0000 + RAM_OFF_picboxmap;
    uint16_t r3 = (uint16_t)(g.picboxmap[0] << 1), x = g.picboxmap[1];
    uint16_t lo = (uint16_t)(p1 + 6);
    do {
        for (uint16_t y = (uint16_t)(r3 - 2); !(y & 0x8000); y -= 2) {
            uint32_t a = (p1 & 0xFF0000) + (uint16_t)lo + y;
            mset16(a, (uint16_t)(m16(a) + base));
        }
        lo = (uint16_t)(lo + r3);
    } while (--x);
}

/* The player card screen's graphics: the team logo sprites, the screen registers, the fonts (remapped), the
 * team block, card back and frame tiles to VRAM, the card picture map, the card back map into text map 2,
 * the palettes, the "switch cards" text, the sprites and the scroll */
void LoadPlayerCardScreen(void)
{
    FadeOut();
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
    g.printm = 0;
    g.charofs2 = 0;
    ClearTextMap();
    ClearTextMap2();
    io_w16(0x2116, (uint16_t)((uint16_t)(g.smallfontchars << 4) + 0x1000 + (g.smallfontchars >> 12 & 1)));   /* (asl's carry) */
    uint16_t n = Decompress(SYM_SmallFontTiles, unpackbuf);
    RemapTiles(unpackbuf, SYM_PixelRemap8F, (uint16_t)(n >> 5));
    RemapTiles(SYM_FramerTiles, SYM_PixelRemap87, 4);
    n = Decompress(SYM_TeamBlocksTiles, unpackbuf);
    g.picchars = (uint16_t)((n >> 5) + g.scorechars + (n >> 4 & 1) - 0x100);   /* (lsr's carry) */
    uint32_t p0 = unpackbuf;
    DMAToVRAM(&p0, n, (uint16_t)((uint16_t)(g.scorechars << 4) + 0x1000 + (g.scorechars >> 12 & 1)), 0);
    g.cardbackchars = 0;
    n = Decompress(SYM_CardBackTiles, unpackbuf);
    g.cardframechars = (uint16_t)((n >> 5) + g.cardbackchars + (n >> 4 & 1));
    LinearToPlanar(unpackbuf, n, n);
    p0 = (unpackbuf & 0xFF0000) | n;
    uint16_t r3 = DMAToVRAM(&p0, n, 0x1000, 0);
    n = Decompress(SYM_CardFrameTiles, unpackbuf);
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, r3);
    Decompress(SYM_CardPictureMap, 0x7E0000 + RAM_OFF_picboxmap);
    picboxmap_add((uint16_t)(g.cardframechars + g.charofs2 + 0x1000));
    Decompress(SYM_CardBackMap, unpackbuf);
    p0 = unpackbuf;
    uint32_t p1 = 0x7E0000 + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, (uint16_t)(g.cardbackchars + g.charofs2 + 0x1000), 0, 0, 0x20);
    Decompress(SYM_SetupScreenPalette, unpackbuf);
    SetColours(unpackbuf, 0x40, 0);
    io_w8(0x2121, 0);
    io_w8(0x2122, 0);
    io_w8(0x2122, 0);
    Decompress(SYM_CardBackPalette, unpackbuf);
    SetColours(unpackbuf, 0x10, 0x40);
    SetColours(SYM_CardSpritePalette, 0x10, 0x80);
    w16(RAM_OFF_colourbuf, 0);
    SendColours();
    print(SYM_SwitchCardsYSwitchTeamsTxt);
    SendTextMap2();
    InitSprites(0x6000, 0, 0x0180);
    SpriteFrameStart();
    WaitFrame();
    SpriteFrameEnd();
    g.BG1HScroll = 4;
    g.BG1VScroll = 0xFFFB;
}

/* The six starters' roster bytes of team a2 (its team data + the word at +$C) */
static uint32_t card_starters(uint16_t a2)
{
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    return bank | (uint16_t)(t + m16((bank | t) + 0x0C));
}
/* (PlayerCards: whether roster player p is one of the six starters at a3, whose cards come first) */
static int card_starter(uint32_t a3, uint16_t p)
{
    for (uint16_t y = 0; y < 6; y++) if (m8(a3 + y) == p) return 1;
    return 0;
}

/* The player cards of the pausing team: right / left the next / previous card (the team card, the six line
 * slots, then the roster without the starters), Y the other team, Start leaves */
void PlayerCards(void)
{
    uint16_t a2 = GetPauseTeam();
    g.FrameByNumber = 0;
    LoadPlayerCardScreen();
    g.curitem[0] = 0;
    g.listend[0] = (uint16_t)(GetPlayerCount(a2) + 6);
    ScreenOn();
    DrawPlayerCard(a2);
    SendTextMap();
    for (;;) {
        WaitFrame();
        ReadPads();
        Pad p = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x1000) return;
        if (d1 & 0x0100) {
            if (!((uint16_t)(g.curitem[0] - g.listend[0]) & 0x8000)) continue;
            g.curitem[0]++;
            uint16_t v = (uint16_t)(g.curitem[0] - 7);
            if (!(v & 0x8000)) {
                g.curplayer[0] = v;
                uint32_t a3 = card_starters(a2);
                do g.curplayer[0]++; while (card_starter(a3, g.curplayer[0]));
                g.curplayer[0]--;
                g.curitem[0] = (uint16_t)(g.curplayer[0] + 7);
            }
        } else if (d1 & 0x0200) {
            if (!g.curitem[0]) continue;
            g.curitem[0]--;
            uint16_t v = (uint16_t)(g.curitem[0] - 7);
            if (!(v & 0x8000)) {
                g.curplayer[0] = (uint16_t)(v + 2);
                uint32_t a3 = card_starters(a2);
                for (;;) {
                    g.curplayer[0]--;
                    if (!g.curplayer[0] || (g.curplayer[0] & 0x8000)) break;
                    if (!card_starter(a3, g.curplayer[0])) break;
                }
                g.curplayer[0]--;
                g.curitem[0] = (uint16_t)(g.curplayer[0] + 7);
            }
        } else if (d1 & 0x4000) {
            a2 ^= 2;
            g.curitem[0] = 0;
            g.listend[0] = (uint16_t)(GetPlayerCount(a2) + 6);
        } else continue;
        DrawPlayerCard(a2);
        SendTextMap();
    }
}

uint16_t PausePlayerCards(uint16_t d1) { return pause_screen(d1, PlayerCards); }

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
            Pad p = ReadPausePad();
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

uint16_t PauseCrowdLevel(uint16_t d1) { return pause_screen(d1, CrowdMeterScreen); }


/* The line editor's title: the background redrawn (drawlinesback, then the band IRQ line back), the label,
 * team a2's block and the pictures */
void DrawEditLinesTitle(uint16_t a2)
{
    if (g.drawlinesback) DrawEditLinesBack();
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

/* The line editor's top: cleared, the title, the frame outline */
void DrawEditLinesTop(uint16_t a2)
{
    print(SYM_DrawEditLinesTopPos);
    eraser(0x20, 0x0D, g.blankchar);
    DrawEditLinesTitle(a2);
    print(SYM_DrawEditLinesTopPos2);
    g.printa = 0x0800;
    FramerOutline(0x11, 3);
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
    DrawEditLinesTitle(a2);
}

/* The selected line's name (the line change names, or the best line names without line changes), then its
 * players */
void PrintLineNames(uint16_t a2)
{
    print(SYM_PrintLineNamesPos);
    print(AdvanceStringPtr(g.OptLine ? SYM_LineMenuNames : SYM_BestLineNames, g.SelectedPlayerIdx));
    PrintLinePlayers(a2);
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

/* The line editor's menu (Start): Return to game (A = $FFFF) or Restore default lines (team a2's lines
 * loaded again, A = 0) */
uint16_t LineEditorExit(uint16_t a2)
{
    ClearEditLinesMenu();
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
        Pad p = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x9080) {
            if (!g.linemenuitem[0]) return 0xFFFF;
            LoadTeamLines(a2);
            ClearEditLinesMenu();
            DrawEditLinesTop(a2);
            return 0;
        }
        if (d1 & 0x0400) g.linemenuitem[0] = 1;
        else if (d1 & 0x0800) g.linemenuitem[0] = 0;
    }
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
        Pad p = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(p.press, p.change, p.held);
        if (d1 & 0x1000) {
            if (LineEditorExit(a2)) return;
            PrintLineNames(a2);
            continue;
        }
        if (d1 & 0x8080) {
            uint16_t a = SelectLinePlayer(a2, 0x20);   /* (r7: $20, as DrawEditLinesBack's paste left it) */
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

/* Edit lines from the pause menu: A / B open the line editor (LineEditor), then the rink is reloaded */
uint16_t PauseEditLines(uint16_t d1)
{
    if (!(d1 & 0x8080)) return PauseMenuPoll();
    g.pausesubmenu = 1;
    LineEditor();
    g.pausesubmenu = 0;
    ReloadRink();
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
    ReloadRink();
    return PauseSubmenuDone();
}

/* The scouting report's text (UpdateScoutingDisplay prints a word or a % field at a time) */

#define SCOUT_NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define SCOUT_INC(p) (((p) & 0xFF0000) | (uint16_t)((p) + 1))

/* Type the word at a1 (d1 characters) at the report's line column: on a new line when the column
 * d0 is past 20, its leading spaces dropped at the start of a line */
void ScoutTypeWord(uint32_t a1, uint16_t d0, uint16_t d1)
{
    if ((int16_t)d0 > 0x14) ScrollDisplayUp();
    if (!g.DispAttribCtr)
        for (uint16_t y = 2; m8(a1 + y) == 0x20; y++) {   /* (y and a1 both step: every other character looked at) */
            uint16_t n = (uint16_t)(m16(a1) - 1);
            a1 = SCOUT_INC(a1);
            mset16(a1, n);
            d1--;
        }
    print(SYM_ScoutNamePosAt);
    g.printx = (uint16_t)(g.printx + g.DispAttribCtr);
    g.printy = (uint16_t)(g.printy + g.VertLineScrolling);
    print(a1);
    g.DispAttribCtr = (uint16_t)(g.DispAttribCtr + d1);
}

/* Copy the next plain word of the script a2 (with the spaces after it) into mesarea and type it at
 * column d0; a "," or "." adds 40 frames to the report timer. PlayerScrollCtr = the script position
 * from a1 after it, $FFFF at the end of the paragraph (a carriage return) */
void ScoutPlainWord(uint32_t a1, uint32_t a2, uint16_t d0)
{
    g.PlayerScrollCtr = 0xFFFF;
    uint16_t a0 = (uint16_t)(RAM_OFF_mesarea + 2), d1 = 0;
    uint8_t c;
loop:
    if (m8(a2) == 0x0D) goto type;
    d0++;
loop2:
    c = m8(a2);
    w8(a0++, c);
    d1++;
    if (c == 0x2C || c == 0x2E) g.ScoutingReportTimer = (uint16_t)(g.ScoutingReportTimer + 0x28);
    a2 = SCOUT_INC(a2);
    if (c != 0x20) goto loop;
    if (m8(a2) == 0x20) goto loop2;
    g.PlayerScrollCtr = (uint16_t)(a2 - a1);
type:
    g.mesarea[0] = (uint16_t)(d1 + 2);
    ScoutTypeWord(RAM_OFF_mesarea, d0, d1);
}

/* (ScoutTypeText's .3: type the string at a1 as one word, a word more counted in PlayerScrollCtr;
 * returns the column d0 moved on by its length) */
uint16_t ScoutHomeCity_3(uint32_t a1, uint16_t d0)
{
    g.PlayerScrollCtr++;
    uint16_t n = m16(a1);
    d0 = (uint16_t)(d0 + n - 1);
    uint16_t d1 = (uint16_t)(n - 2);
    if (!m8(a1 + (uint16_t)(n - 1))) d1--;
    ScoutTypeWord(a1, d0, d1);
    return d0;
}

/* Type the string at a1 word by word from column d0, the words wrapping at 20 columns */
void ScoutTypeText(uint32_t a1, uint16_t d0)
{
    uint16_t y = 3;
    do {
        if (m8(a1 + y) == 0x20) {
            uint16_t n = m16(a1), d4 = (uint16_t)(y - 2);
            mset16(a1, y);
            d0 = ScoutHomeCity_3(a1, d0);
            g.PlayerScrollCtr--;
            a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + d4);
            mset16(a1, (uint16_t)(n - d4));
            if (!SCOUT_NEG(d0, 0x15)) d0 = d4;
            y = 2;
        }
        y++;
    } while (SCOUT_NEG(y, m16(a1)));
    ScoutHomeCity_3(a1, d0);
}

/* Type string a1 (copied to a fresh mesarea) from column d0 */
void ScoutAppend(uint32_t a1, uint16_t d0)
{
    g.mesarea[0] = 2;
    appstring(a1, RAM_OFF_mesarea);
    ScoutTypeText(RAM_OFF_mesarea, d0);
}

/* Script code %e: "extremely " when the hot / cold totals differ by 8 or more (hotextreme), else the
 * script's next word (a2 past the code) */
void ScoutExtremely(uint32_t a1, uint32_t a2, uint16_t d0)
{
    a2 = SCOUT_INC(a2);
    if (g.hotextreme) ScoutAppend(SYM_ScoutFragments, d0);
    else ScoutPlainWord(a1, a2, d0);
}

/* Script code %t: the full name of the hotter team, then " has" (" have" for the two New York teams) */
void ScoutHotTeam(uint16_t d0)
{
    uint32_t a1 = GetTeamLongName(g.hotterteam);
    g.mesarea[0] = 2;
    appstring(a1, RAM_OFF_mesarea);
    uint16_t k = g.hotterteam == 0x0D || g.hotterteam == 0x0E ? 18 : 12;
    appstring((SYM_ScoutFragments & 0xFF0000) | (uint16_t)(SYM_ScoutFragments + k), RAM_OFF_mesarea);
    ScoutTypeText(RAM_OFF_mesarea, d0);
}

/* Type the name of player d0 of team a2 (0 home, 2 visitors) from column x */
void ScoutPlayerName(uint16_t d0, uint16_t a2, uint16_t x)
{
    uint32_t a0 = GetPlayerRecord(a2, d0);
    g.mesarea[0] = 2;
    appstring(a0, RAM_OFF_mesarea);
    ScoutTypeText(RAM_OFF_mesarea, x);
}

/* Name player a of the home team / the visitors from column d0 */
void ScoutHomePlayer(uint16_t a, uint16_t d0) { ScoutPlayerName(a, 0, d0); }
void ScoutVisPlayer(uint16_t a, uint16_t d0) { ScoutPlayerName(a, 2, d0); }

/* (team data a1's strings: city, name, nickname, arena) */
static uint32_t scout_team_strings(uint16_t a1)
{
    uint32_t bank = SYM_TeamList & 0xFF0000;
    return bank | (uint16_t)(a1 + m16((bank | a1) + 4));
}

/* The city / the nickname (the first / the third string) of team data a1, from column d0 */
void ScoutTeamCity(uint16_t a1, uint16_t d0) { ScoutAppend(scout_team_strings(a1), d0); }
void ScoutTeamNick(uint16_t a1, uint16_t d0) { ScoutAppend(AdvanceStringPtr(scout_team_strings(a1), 2), d0); }

/* Script codes %r (the home arena), %H / %V (the full names), %h / %v (the cities), %n / %m (the
 * nicknames) */
void ScoutHomeArena(uint16_t d0) { ScoutAppend(AdvanceStringPtr(scout_team_strings(g.tmdata[0]), 3), d0); }
void ScoutHomeName(uint16_t d0) { ScoutAppend(GetTeamLongName(g.HomeTeam), d0); }
void ScoutVisName(uint16_t d0) { ScoutAppend(GetTeamLongName(g.VisTeam[0]), d0); }
void ScoutHomeCity(uint16_t d0) { ScoutTeamCity(g.tmdata[0], d0); }
void ScoutVisCity(uint16_t d0) { ScoutTeamCity(g.tmdata[1], d0); }
void ScoutHomeNick(uint16_t d0) { ScoutTeamNick(g.tmdata[0], d0); }
void ScoutVisNick(uint16_t d0) { ScoutTeamNick(g.tmdata[1], d0); }

/* Script codes %0-%5 / %6-%b: the home / visitors' three hottest and three coldest players */
void ScoutHomeHot1(uint16_t d0) { ScoutHomePlayer(g.hotplayer1[0], d0); }
void ScoutHomeHot2(uint16_t d0) { ScoutHomePlayer(g.hotplayer2[0], d0); }
void ScoutHomeHot3(uint16_t d0) { ScoutHomePlayer(g.hotplayer3[0], d0); }
void ScoutHomeCold1(uint16_t d0) { ScoutHomePlayer(g.coldplayer1[0], d0); }
void ScoutHomeCold2(uint16_t d0) { ScoutHomePlayer(g.coldplayer2[0], d0); }
void ScoutHomeCold3(uint16_t d0) { ScoutHomePlayer(g.coldplayer3[0], d0); }
void ScoutVisHot1(uint16_t d0) { ScoutVisPlayer(g.hotplayer1[1], d0); }
void ScoutVisHot2(uint16_t d0) { ScoutVisPlayer(g.hotplayer2[1], d0); }
void ScoutVisHot3(uint16_t d0) { ScoutVisPlayer(g.hotplayer3[1], d0); }
void ScoutVisCold1(uint16_t d0) { ScoutVisPlayer(g.coldplayer1[1], d0); }
void ScoutVisCold2(uint16_t d0) { ScoutVisPlayer(g.coldplayer2[1], d0); }
void ScoutVisCold3(uint16_t d0) { ScoutVisPlayer(g.coldplayer3[1], d0); }

/* (ShowMatchups: team a2's matchup player for the line slot curitem into curplayer, his rating
 * printed; returns it) */
static uint16_t matchup_rating(uint16_t a2)
{
    uint16_t q;
    g.curplayer[0] = GetMatchupPlayer(g.curitem[0], a2);
    uint16_t d0 = GetPlayerRating(1, a2);
    print(PushNumberWrap(d0, 3, &q));
    return d0;
}

/* The matchups box of the scouting report: both team blocks; for the line slot curitem (0 the teams)
 * the two players' pictures, ratings and names (or the teams' logos and overall ratings), then the
 * slot's name and who has the advantage (arrows up / down for the visitors, or none) */
void ShowMatchups(void)
{
    uint16_t a2 = r16(RAM_OFF_a2);   /* (as the caller left it) */
    uint16_t vis, home, q;
    print(SYM_ShowMatchupsPos);
    eraser(0x20, 0x0B, g.blankchar);
    SpriteFrameStart();
    PrintLetterSprites(SYM_MatchupsLabel, 0x56, 0x10);
    print(SYM_ShowMatchupsPos2);
    DrawTeamBlocks(g.VisTeam[0], 0, a2);
    print(SYM_ShowMatchupsPos3);
    DrawTeamBlocks(g.HomeTeam, 0, a2);
    if (g.curitem[0]) {
        g.curitem[0]--;
        g.setupvis = 0xFFFF;
        g.setuphome = 0xFFFF;
        g.matchuphome = g.curitem[0];
        g.matchupvis = g.curitem[0];
        DrawMatchupPictures(2);
        g.colourbuf[0] = 0x3568;
        SendColours();
        print(SYM_ShowMatchupsPos4);
        vis = matchup_rating(2);
        print(SYM_ShowMatchupsPos5);
        print(PlayerInitialName(2, g.curplayer[0]));
        print(SYM_ShowMatchupsPos6);
        home = matchup_rating(0);
        print(SYM_ShowMatchupsPos7);
        uint32_t a1 = PlayerInitialName(0, g.curplayer[0]);
        g.printx = (uint16_t)(0x21 - m16(a1));
        print(a1);
        g.curitem[0]++;
    } else {
        g.setuphome = g.HomeTeam;
        g.setupvis = g.VisTeam[0];
        DrawMatchupPictures(a2);
        SendColours();
        print(SYM_OverallTxt);
        vis = GetTeamRating(2);
        print(PushNumberWrap(vis, 3, &q));
        print(SYM_OverallTxt2);
        home = GetTeamRating(0);
        print(PushNumberWrap(home, 3, &q));
    }
    print(SYM_ShowMatchupsPos8);
    print(AdvanceStringPtr(SYM_LineSlotNames, g.curitem[0]));
    print(SYM_AdvantageTxt);
    if (vis == home) print(SYM_NoneTxt);
    else if (SCOUT_NEG(vis, home)) print(SYM_DownArrowsTxt);
    else print(SYM_UpArrowsTxt);
    WaitFrame();
    SpriteFrameEnd();
}

/* ---- the front end: the intro, the title, the password and player select screens ---- */

#define SPFpad 389         /* (frames94s.asm) */
#define SPFsmallfont 433

/* Clear the message rows and redraw the PlayerSelectAlt title and the team pictures */
void RedrawPlayerSelect(uint16_t a2)
{
    ClearTextMapBottom();
    SpriteFrameStart();
    PrintLetterSprites(SYM_PlayerSelectAlt, 0x47, 0x18);
    DrawMatchupPictures(a2);
    WaitFrame();
    SpriteFrameEnd();
}

/* Set up the scouting report screen: fade out (the team logo sprites unpacked meanwhile), the PPU's
 * maps and sprites, the small font, framer, team block, outline, menu back and Ron Barr tiles, the
 * menu back and Ron Barr maps in text map 2, the palettes, a clear text map, the sprites and the
 * picture map */
void LoadScoutingScreen(void)
{
    FadeOut();
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
    io_w16(0x2116, (uint16_t)((uint16_t)(g.blankchar << 4) + 0x1000 + (g.blankchar >> 12 & 1)));   /* (asl's carry) */
    g.smallfontchars = (uint16_t)(g.blankchar + 1);
    g.outlinechars = g.smallfontchars;
    FillVRAM(0, 0x20, 0xFFFF, 0);
    uint16_t n = Decompress(SYM_SmallFontTiles, unpackbuf);
    uint16_t d0 = n >> 5;
    g.outlinechars = (uint16_t)(g.outlinechars + d0);
    g.Framercset = (uint16_t)(d0 + g.smallfontchars);
    RemapTiles(unpackbuf, SYM_PixelRemap8F, d0);
    g.outlinechars = (uint16_t)(g.outlinechars + 4);
    g.scorechars = (uint16_t)(g.Framercset + 4);
    RemapTiles(SYM_FramerTiles, SYM_PixelRemap87, 4);
    n = Decompress(SYM_TeamBlocksTiles, unpackbuf);
    g.outlinechars = (uint16_t)(g.outlinechars + (n >> 5));
    uint32_t p0 = unpackbuf;
    DMAToVRAM(&p0, n, (uint16_t)((uint16_t)(g.scorechars << 4) + 0x1000 + (g.scorechars >> 12 & 1)), 0);
    p0 = SYM_FramerOutlineTiles;
    DMAToVRAM(&p0, 0x120, 0xFFFF, 0);
    n = Decompress(SYM_MenuBackTiles, unpackbuf);
    g.backchars = n >> 5;
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x1000, 0);
    Decompress(SYM_MenuBackMap, unpackbuf);
    p0 = unpackbuf;
    uint32_t p1 = 0x7E0000u + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 0, 0, 0x20);
    Decompress(SYM_SetupScreenPalette, unpackbuf);
    SetColours(unpackbuf, 0x30, 0);
    n = Decompress(SYM_RonBarrTiles, unpackbuf);
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, 0);
    n = Decompress(SYM_RonBarrMap, unpackbuf);
    uint16_t y = (uint16_t)(n - 8);
    do {
        uint16_t v = m16(unpackbuf + 6 + y);
        if (v) w16(RAM_OFF_textmap2 + y, (uint16_t)(v + g.backchars + g.charofs2 + 0x0800));
        y = (uint16_t)(y - 2);
    } while (!(y & 0x8000));
    Decompress(SYM_SetupScreenPalette, unpackbuf);
    SetColours(unpackbuf, 0x10, 0);
    SetColours(unpackbuf + 0x20, 0x10, 0x10);
    Decompress(SYM_RonBarrPalette, unpackbuf);
    SetColours(unpackbuf, 0x10, 0x20);
    io_w8(0x2121, 0);
    io_w8(0x2122, 0);
    io_w8(0x2122, 0);
    Decompress(SYM_MenuBackPalette, unpackbuf);
    SetColours(unpackbuf, 0x20, 0x30);
    g.outlinechars = (uint16_t)(g.outlinechars + 0x1000);
    SetColours(SYM_SetupSpritePalette, 0x10, 0x80);
    SetColours(SYM_UnusedSetupSpritePalette, 0x10, 0xC0);
    SendColours();
    ClearTextMap();
    SendTextMap2();
    WaitFrame();
    InitSprites(0x6000, 0, 0x0180);
    SpriteFrameStart();
    LoadPictureMap();
}

/* Insert rating column d1 of player curplayer of team a2 (GetPlayerRating) into the best-three list
 * strengths (column, rating pairs) */
void AddStrength(uint16_t d1, uint16_t a2)
{
    uint16_t d0 = GetPlayerRating(d1, a2), r0, r1;
    uint16_t a = (uint16_t)(d0 - 1);
    if ((uint16_t)(a - g.strengths[1]) & 0x8000) {
        if ((uint16_t)(a - g.strengths[3]) & 0x8000) {
            if ((uint16_t)(a - g.strengths[5]) & 0x8000) return;
            goto two;
        }
        goto one;
    }
    r0 = g.strengths[1]; r1 = g.strengths[0];
    g.strengths[1] = d0; g.strengths[0] = d1;
    d0 = r0; d1 = r1;
one:
    r0 = g.strengths[3]; r1 = g.strengths[2];
    g.strengths[3] = d0; g.strengths[2] = d1;
    d0 = r0; d1 = r1;
two:
    g.strengths[5] = d0;
    g.strengths[4] = d1;
}

/* The player card strengths of skater curplayer of team a2: the best three of eleven rating columns */
void FindSkaterStrengths(uint16_t a2)
{
    static const uint8_t cols[] = { 4, 6, 8, 0xF, 7, 0xB, 9, 3, 0xA, 0xD, 0xE };
    for (int k = 0; k < 6; k++) g.strengths[k] = 0xFFFF;
    for (int k = 0; k < 11; k++) AddStrength(cols[k], a2);
}

/* The player card strengths of goalie curplayer of team a2: the best three goalie rating columns
 * (columns 8 and 9, or 7 and 10 when column 4 is under 50) */
void FindGoalieStrengths(uint16_t a2)
{
    for (int k = 0; k < 6; k++) g.strengths[k] = 0xFFFF;
    AddStrength(5, a2);
    AddStrength(2, a2);
    AddStrength(6, a2);
    if (GetPlayerRating(4, a2) >= 0x32) {
        AddStrength(8, a2);
        AddStrength(9, a2);
    } else {
        AddStrength(7, a2);
        AddStrength(0xA, a2);
    }
}

/* (|a| as the 65816 takes it: negated when bit 15 is set) */
static uint16_t abs16(uint16_t a) { return a & 0x8000 ? (uint16_t)-a : a; }

/* The puck x against player y (a2): unless he carries it, is past the skaters, off the ice or not
 * colliding. A goalie: within his stick's reach (his stick hot spot, the height by the
 * checkpuckcoll_ccx_8 table) the stick (puckstick); then his body within 11 (puckgoalie). A skater,
 * puck low and slow: within $C4 squared of his stick (a quarter while the puck is not to be
 * touched) the stick; otherwise within 9 and $41 squared his body (puckbody, the original's X
 * being a3). */
void checkpuckcoll_ccx(int x, int y, int a3, uint16_t a0)
{
    int a2 = y;
    uint16_t d0, d1, d2, d3, d4;
    int16_t hx, hy;
    if (2 * y == g.puckc || y >= 12 || (g.position[y] & 0x8000) || (g.pfnc[y] | g.nopuck[y])) return;
    if (g.pflags[a2] & 0x1000) goto l5;
    if (!g.position[a2]) goto l9;
    if (!((uint16_t)(g.puckz - 5) & 0x8000) || !((uint16_t)(g.puckvz - 0x100) & 0x8000)) goto l5;
    GetHot(a2, &hx, &hy);
    d0 = abs16((uint16_t)((uint16_t)hx + g.Xpos[a2] - g.Xpos[14]));
    if (d0 >= 0xF) goto l5;
    d1 = abs16((uint16_t)((uint16_t)hy + g.Ypos[a2] - g.pucky[0]));
    if (d1 >= 0xF) goto l5;
    d0 = (uint16_t)(Mul8U(d0, d0) + Mul8U(d1, d1));
    d1 = 0xC4;
    if (!(g.nopuck[14] & 0x8000) && g.nopuck[14]) d1 >>= 2;
    if (d1 < d0) goto l5;
    puckstick(g.curobj >> 1, a2, a3, d0, a0);
    return;
l5:
    if (!g.position[a2]) goto l7;
    d0 = abs16((uint16_t)(g.Xpos[a2] - g.Xpos[14]));
    if (d0 >= 9) return;
    d1 = abs16((uint16_t)(g.Ypos[a2] - g.pucky[0]));
    if (d1 >= 9) return;
    d0 = (uint16_t)(Mul8U(d0, d0) + Mul8U(d1, d1));
    if (d0 >= 0x41) return;
    puckbody(a3, a2, a3, d0);
    return;
l9:
    GetFrameHot(a2, &d0, &d1);
    d2 = abs16(d0);
    d4 = rom16(SYM_checkpuckcoll_ccx_8 + (uint16_t)(d2 << 1));
    d3 = rom16(SYM_checkpuckcoll_ccx_8 + (uint16_t)(abs16(d1) << 1));
    x = g.curobj >> 1;
    GetHot(a2, &hx, &hy);
    d0 = abs16((uint16_t)((uint16_t)hx + g.Xpos[a2] - g.Xpos[14]));
    if (d0 >= d2) goto l7;
    d1 = abs16((uint16_t)((uint16_t)hy + g.Ypos[a2] - g.pucky[0]));
    if (d1 >= d2) goto l7;
    {
        uint16_t t = (uint16_t)(d3 + d4);
        if ((uint16_t)(t - 5) & 0x8000) t = 5;
        if (t < (uint16_t)g.puckz) goto l7;
        t = (uint16_t)(d3 - d4);
        if (!(t & 0x8000) && t >= (uint16_t)g.puckz) goto l7;
    }
    d0 = (uint16_t)(Mul8U(d0, d0) + Mul8U(d1, d1));
    d1 = Mul8U(d2, d2);
    if (!(g.nopuck[14] & 0x8000) && g.nopuck[14]) d1 >>= 2;
    if (d1 >= d0) puckstick(x, a2, a3, d0, a0);
l7:
    d0 = (uint16_t)(g.Xpos[a2] - g.Xpos[14]);
    if ((uint16_t)(d0 - 0xFFF5) & 0x8000 || !((uint16_t)(d0 - 0xB) & 0x8000)) return;
    d1 = (uint16_t)(g.Ypos[a2] - g.pucky[0]);
    if ((uint16_t)(d1 - 0xFFF5) & 0x8000 || !((uint16_t)(d1 - 0xB) & 0x8000)) return;
    puckgoalie(x, a2, a3, d0, d1);
}

/* Horizontal rink: both team blocks (home at column $14, visitors at 3, row 2) and their palette rows */
void DrawHorTeams(void)
{
    g.horblocks = 1;
    g.printx = 0x14;
    g.printy = 2;
    DrawTeamBlocks(g.HomeTeam, 0, 0);
    g.printx = 3;
    DrawTeamBlocks(g.VisTeam[0], 0, 2);
    g.horblocks = 0;
    g.redrawblocks = 0;
    g.colourbuf[0xE1] = 0x1D47;
    g.colourbuf[0xF1] = 0x1D47;
    if (!g.pausemenuon) {
        g.colourbuf[0xEF] = 0xFFFF;
        g.colourbuf[0xFF] = 0xFFFF;
    }
    DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf + 0xC0, 0x20, 0x60);
    WaitFrame();
    DMAToCGRAM(0x7E0000 + RAM_OFF_colourbuf + 0x1C0, 0x20, 0xE0);
    SendTextMap();
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
    FadeOut();
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

/* Bad password: wait for a button, then offer retry or exit. 0: the password is good (the password
 * screen ends; Y, exit, has restarted at the option screens), 1: back to the entry (PasswordRedraw) */
int BadPasswordPrompt(void)
{
    DecodePW();
    if (!(g.postarts & 0x8000)) return 0;
    while (r16(RAM_OFF_JoyPad) & 0xC080) hal_wait_interrupt();
    print(SYM_BadPassword);
    print(SYM_ARetryTxt);
    print(SYM_YExitScreenTxt);
    print(SYM_BadPasswordPromptPos);
    QueueTextDMA();
    for (;;) {
        uint16_t d0, d1, d2, d3;
        ReadPads();
        ReadJoy1(&d0, &d1, &d2, &d3);
        if (!d3) continue;
        if (d3 & 0x4000) {
            g.OptPlayMode = 2;
            Restart();
            return 0;
        }
        if (d3 & 0x8080) break;
    }
    print(SYM_BadPasswordPromptPos2);
    eraser(0x0F, 3, g.blankchar);
    PrintPasswordHelp();
    QueueTextDMA();
    WaitNoButtons();
    return 1;
}

/* (the password character d5 at the cursor printed in the one-character string PWCharStr) */
static void print_pw_char(uint16_t d5)
{
    g.PWCharStr[1] = rom16(SYM_PasstoText + d5);
    g.printx++;
    g.printy -= 2;
    print(RAM_OFF_PWCharStr);
}

/* The password entry: the cursor on character d4 of the password (A / B next and back, the d-pad
 * picks its character d5), Start checks it (BadPasswordPrompt). Returns d2 as it is left: $FFFF
 * (ReadPassBits' loop count, through DecodePW). */
uint16_t PasswordRedraw(uint16_t d4, uint16_t d5)
{
    uint16_t d0, d1, d2, d3;
    PrintPasswordHelp();
    for (;;) {
        print(SYM_PasswordScreenPos2);
        PWprint();
        QueueTextDMA();
        do {
            WaitFrame();
            ReadPads();
            ReadJoy1(&d0, &d1, &d2, &d3);
            d1 = ProcessInputWithRepeat(d1, d2, d3);
        } while (!d1);
        if (d1 & 0x1000) {
            if (!BadPasswordPrompt()) return 0xFFFF;
            PrintPasswordHelp();
            continue;
        }
        if (d1 & 0xC080) {
            d0 = d1 & 0x4000 ? 0xFFFF : 1;
            uint16_t a = (uint16_t)(d4 + d0);
            if (a >= 0x10) continue;
            d4 = a;
            PrintSelectLeft(d4);
            d0 = 0;
            uint16_t c = g.PassWord[d4];
            if (c < 0x1E) d0 = (uint16_t)(c - d5);
        } else if (d1 & 0x0100) d0 = 1;
        else if (d1 & 0x0200) d0 = 0xFFFF;
        else if (d1 & 0x0400) d0 = 6;
        else if (d1 & 0x0800) d0 = 0xFFFA;
        else continue;
        uint16_t a = (uint16_t)(d5 + d0);
        if (a >= 0x1E) continue;
        PlacePasswordCursor(d5);
        eraser(3, 3, g.blankchar);
        g.PWCharStr[0] = 3;
        print_pw_char(d5);
        d5 = a;
        PlacePasswordCursor(d5);
        FramerOutline(3, 3);
        print_pw_char(d5);
        g.PassWord[d4] = (uint8_t)d5;
    }
}

/* The password entry screen: the characters in five rows of six, the cursor on the password's first
 * character; returns d2 as PasswordRedraw leaves it */
uint16_t PasswordScreen(void)
{
    FadeOut();
    WaitFadeOut();
    w16(RAM_OFF_PassWord, r16(RAM_OFF_PassWord) & 0xFF7F);
    ClearTextMap();
    g.textdirtylo = 0;
    g.textdirtyhi = 0x03FF;
    ReadPads();
    ReadAnyPad();
    SpriteFrameStart();
    PrintLetterSprites(SYM_PasswordLabel, 0x53, 0x18);
    g.setupvis = 0x1C;
    g.setuphome = 0x1D;
    DrawMatchupPictures(0);
    SpriteFrameEnd();
    SendColours();
    print(SYM_PasswordScreenPos);
    FramerOutline(0x11, 5);
    print(SYM_PasswordTxt);
    g.printx -= 2;
    uint32_t a0 = SYM_PasstoText;
    for (int row = 0; row < 5; row++) {
        uint16_t a1 = RAM_OFF_mesarea;
        w16(a1, 0x0D);
        a1 += 2;
        for (int k = 0; k < 6; k++) {
            w16(a1, m16(a0));
            a0 = (a0 & 0xFF0000) | (uint16_t)(a0 + 1);
            a1++;
            w16(a1, 0x20);
            a1++;
        }
        print(RAM_OFF_mesarea);
        g.printy += 2;
        g.printx -= 0x0B;
    }
    uint16_t d4 = 0, d5 = 0;
    PrintSelectLeft(d4);
    if (g.PassWord[0] < 0x1E) d5 = g.PassWord[0];
    PlacePasswordCursor(d5);
    FramerOutline(3, 3);
    g.PWCharStr[0] = 3;
    print_pw_char(d5);
    g.fadestep = 0x0A;
    return PasswordRedraw(d4, d5);
}

/* The player select screen after the game setup: each pad moves its icon under the visitors or home
 * team (cont1team-cont5team), Start goes on. a2 and d2 (the multitap state last seen) as setoptions
 * leaves them. */
void PlayerSelect(uint16_t a2, uint16_t d2)
{
    g.cont1team = g.cont2team = g.cont3team = g.cont4team = 0;
    g.cont5team[0] = 0;
    if (!g.notdemo) return;
    g.mp5readall = 0xFFFF;
    FadeOut();
    WaitFadeOut();
    forceblack();
    io_w8(0x2105, 0x09);
    io_w8(0x2109, 0x08);
    io_w8(0x210C, 0x04);
    io_w8(0x212C, 0x17);
    g.BG3VScroll = 0xFFFC;
    MemFill(0x7E0000 + RAM_OFF_screenmap + 6, 0x0800, 0);
    uint32_t p0 = 0x7E0000 + RAM_OFF_screenmap + 6;
    DMAToVRAM(&p0, 0x0800, 0x0800, 0);
    MenuScreenHDMA();
    ScreenOn();
    FadeIn();
    g.fadestep = 0x0A;
    uint16_t solo = 0;                                     /* (d1+2: one player against the computer) */
    if (g.OptPlayMode >= 1 && g.OptPlayMode < 4) {
        g.cont1team = 1;
        FigureJoy();
        d2 = r8(RAM_OFF_potree + g.potreeteam);   /* (FigureJoy leaves the user's playoff team in d2) */
        if (g.cont1team != 1) g.HomeTeam = g.VisTeam[0];
        solo = 1;
    }
    g.cont1team = 1;
redraw:
    ClearTextMapBottom();
    ClearBigText();
    for (uint16_t x = 0; x < 0x0290; x += 2) w16(RAM_OFF_bigtextbuf + x, 0);
    g.redrawblocks = 0xFFFF;
    g.printx = 1;
    g.printy = 0x0A;
    if (!solo) DrawSelectTeam(g.VisTeam[0]);
    g.printx = 0x16;
    DrawSelectTeam(g.HomeTeam);
    SendBigText();
    SendTextMap();
    SetColours(SYM_PlayerMarkerPalette, 0x10, 0x90);
    if (solo) {
        print(SYM_NotTxt);
        print(SYM_PlayingTxt);
    } else {
        print(SYM_NotTxt2);
        print(SYM_PlayingTxt2);
    }
    print(SYM_UseToSelectTeamTxt);
    print(SYM_PressStartWhenDoneTxt);
    QueueTextDMA();
    for (;;) {
        WaitFrame();
        uint16_t w = g.mp5warning;
        if (w != d2) {
            d2 = w;
            if (!w) {
                Multi5Init();
                goto redraw;
            }
            if (w & 1) { RedrawPlayerSelect(a2); Multi5SocketMsg(); }
            else if (w & 2) { RedrawPlayerSelect(a2); Multi5ScopeMsg(); }
            else if (w & 4) { RedrawPlayerSelect(a2); Multi5MouseMsg(); }
        }
        if (g.mp5warning) continue;
        ReadPads();
        for (uint16_t x = 0;;) {
            uint32_t c = RAM_OFF_cont1team + x;
            if (g.mp5warning || (r16(RAM_OFF_JoyPad + x) & 0x000F)) w16(c, 0);
            else {
                uint16_t pn = r16(RAM_OFF_padnew + x);
                if (pn & 0x1000) {
                    if (solo) {
                        uint16_t any = 0;
                        for (uint16_t k = 0; k < 0x0A; k += 2) any |= r16(RAM_OFF_cont1team + k);
                        if (!(any & 1)) {
                            print(SYM_AtLeastOnePersonMustTxt);
                            print(SYM_PlayInPlayoffModeTxt);
                            goto refused;
                        }
                        FigureJoy();
                    }
                    if (g.OptPlayMode == 4)
                        for (uint16_t k = 5; k; k--) {             /* (from the word after cont5team) */
                            uint16_t t = r16(RAM_OFF_cont1team + (uint16_t)(k << 1));
                            if (!t) continue;
                            for (uint16_t j = (uint16_t)(k - 1); !(j & 0x8000); j--)
                                if (r16(RAM_OFF_cont1team + (uint16_t)(j << 1)) == t) {
                                    print(SYM_OnlyOnePlayerPerTeamTxt);
                                    print(SYM_IsAllowedInShootoutTxt);
                                    goto refused;
                                }
                        }
                    g.mp5readall = 0;
                    return;
                }
                uint16_t t = r16(c);
                if (pn & 0x0200) {
                    if (!t) { if (!solo) w16(c, 2); }
                    else if (t == 1) w16(c, 0);
                } else if (pn & 0x0100) {
                    if (t == 2) w16(c, 0);
                    else if (t != 1) w16(c, 1);
                }
            }
            x += 2;
            if (g.OptPlayMode == 4 && x >= 4) break;
            if (x >= 0x0A) break;
        }
        SpriteFrameStart();
        g.setupvis = g.VisTeam[0];
        g.setuphome = g.HomeTeam;
        if (solo) g.setupvis = 0x1C;
        PrintLetterSprites(SYM_PlayerSelectTitle, 0x47, 0x18);
        DrawMatchupPictures(a2);
        SendColours();
        for (uint16_t y = 0;;) {
            if (!g.mp5warning && !(r16(RAM_OFF_JoyPad + y) & 0x000F)) {
                uint16_t k = (uint16_t)((uint16_t)((uint16_t)(solo << 2) + r16(RAM_OFF_cont1team + y)) << 1);
                uint16_t sx = rom16(SYM_PlayerSelect_11 + k), sy = rom16(SYM_PlayerSelect_12 + y);
                DrawSprite(0x2600, sx, sy, (uint16_t)((y >> 1) + SPFpad), SYM_SpriteFrames);
                DrawSprite(0x2000, (uint16_t)(sx - 3), (uint16_t)(sy - 2), (uint16_t)((y >> 1) + SPFsmallfont), SYM_SpriteFrames);
            }
            y += 2;
            if (g.OptPlayMode == 4 && y >= 4) break;
            if (y >= 0x0A) break;
        }
        WaitFrame();
        SpriteFrameEnd();
        continue;
    refused:
        QueueTextDMA();
        WaitPadsRelease();
        for (;;) {
            ReadPads();
            uint16_t x;
            for (x = 8; !(x & 0x8000); x -= 2) {
                if (g.mp5warning) continue;
                uint16_t j = r16(RAM_OFF_JoyPad + x);
                if (!(j & 0x000F) && (j & 0xFFF0)) break;
            }
            if (!(x & 0x8000)) break;
        }
        print(SYM_UseToSelectTeamTxt2);
        print(SYM_PressStartWhenDoneTxt2);
        QueueTextDMA();
    again:
        for (uint16_t x = 8; !(x & 0x8000); x -= 2) {
            uint16_t j = r16(RAM_OFF_JoyPad + x);
            if (!(j & 0x000F) && (j & 0x1000)) { hal_wait_interrupt(); goto again; }
        }
    }
}

/* Title: the next team's logo when the logo path has ended */
void TitleScreen_5(void)
{
    if (!(g.TitleLogoFlags[0] & 1)) return;
    g.TitlePathPos = 0;
    g.TitlePathFrac = 0;
    LoadTitleLogo(NextTitleTeam());
    g.TitleLogoFlags[0] &= 0xFFFE;
}

/* Title: the logo along its path (frame hook): $C00 a frame, $500 steps, then the other side; the
 * step's scale and offsets from BuildTitlePath's tables */
void TitleScreen_6(void)
{
    if (!(g.TitleLogoFlags[0] & 2)) return;
    if (!(g.TitleLogoFlags[0] & 1)) {
        uint32_t s = (uint32_t)g.TitlePathFrac + 0x0C00;
        g.TitlePathFrac = (uint16_t)s;
        if (s > 0xFFFF) g.TitlePathPos++;
    }
    uint16_t a = (uint16_t)((g.TitlePathPos & 0xFF) << 8 | g.TitlePathFrac >> 8);
    if (a >= 0x0500) {
        g.TitleLogoFlags[0] |= 1;
        g.TitlePathFlip ^= 1;
        a = 0;
    }
    uint16_t x = (uint16_t)(a << 1);
    g.TitleM7Scale = r16(RAM_OFF_titlescale + x);
    uint16_t v = r16(RAM_OFF_titlexofs + x);
    if (g.TitlePathFlip) v = (uint16_t)-v;
    g.TitleM7HOfs = (uint16_t)(v + 0x0190);
    g.TitleM7VOfs = r16(RAM_OFF_titleyscroll + x);
}

/* Title: the palettes, the licence and logo tiles, the maps and the sprites */
void TitleScreen_7(void)
{
    uint32_t p0;
    Decompress(SYM_TitlePalette, unpackbuf);
    Decompress(SYM_TitleSpritePalette, unpackbuf + 0x100);
    DMAToCGRAM(unpackbuf, 0x0100, 0);
    FillVRAMSplit(0, 0x4000, 0);
    uint16_t n = Decompress(SYM_LicenceTiles, unpackbuf);
    p0 = unpackbuf;
    DMAToVRAMHigh(&p0, n, 0);
    g.MapVRAM = 0;
    Mode7MapToVRAM(SYM_LicenceNHLMap, 0x3C, 0x3C);
    n = Decompress(SYM_TitleLogoTiles, unpackbuf);
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x4000, 0);
    Decompress(SYM_Title94Map, unpackbuf);
    g.MapVRAM = 0x5400;
    MapToVRAM(unpackbuf, 0, 0);
    Decompress(SYM_TitleNHLMap, unpackbuf);
    g.MapVRAM = 0x5800;
    MapToVRAM(unpackbuf, 0, 0);
    Decompress(SYM_LicenceNHLPAMap, unpackbuf);
    n = Decompress(SYM_TitlePuckMap, 0x7E0000 + RAM_OFF_introbuf);
    MemOr(0x7E0000 + RAM_OFF_introbuf + 6, n, 0x8080);
    Decompress(SYM_TitleSprites, titlesprbuf);
}

/* Title: its variables (TitleWH1 to $0C00) cleared */
void TitleScreen_8(void) { memset(WRAM_BYTES + RAM_OFF_TitleWH1, 0, 0x0C01 - RAM_OFF_TitleWH1); }

/* Title: sprite a alone for 4 frames */
void TitleScreen_9(uint16_t a)
{
    SpriteFrameStart();
    DrawTitleSprite(a);
    SpriteFrameEnd();
    TitleWait(4);
}

/* (the title's next IRQ hook, its bank stored as a word: the byte after IRQHook's, seed's low byte,
 * cleared) */
static void title_irq_next(uint32_t hook, uint16_t v, uint16_t h)
{
    w16(RAM_OFF_IRQHook + 2, (uint16_t)(hook >> 16));
    w16(RAM_OFF_IRQHook, (uint16_t)hook);
    io_w16(0x4209, v);
    io_w16(0x4207, h);
}

/* Title frame hook: a press of TitleSkipMask ends the title (titleskip), then the top band's display
 * registers, .irq1 at line $46 and the logo along its path */
void TitleScreen_framehook(void)
{
    if (r16(RAM_OFF_JoyPad) & g.TitleSkipMask) g.titleskip = 1;
    io_w8(0x2100, (uint8_t)g.brightness);
    io_w8(0x212C, 0x17);
    io_w8(0x2107, 0x54);
    io_w8(0x210B, 0x44);
    io_w8(0x2123, 0xC3);
    io_w8(0x2105, (uint8_t)g.TitleBGMode);
    io_w8(0x2127, (uint8_t)g.TitleWH1);
    io_w8(0x2129, (uint8_t)g.TitleWH3);
    title_irq_next(SYM_TitleScreen_irq1, 0x46, 0);
    TitleScreen_6();
    nmi_b = (uint8_t)((g.TitleLogoFlags[0] & 0x0002 ? g.TitleM7VOfs : g.TitleLogoFlags[0]) >> 8);   /* (.6's last load) */
}

/* Title IRQ hooks: the WH3 window edge, then .irq2; the mode 7 band's matrix and scroll, then .irq3;
 * BG1 on the main screen */
void TitleScreen_irq1(void)
{
    io_w8(0x2129, (uint8_t)g.TitleWH3Low);
    title_irq_next(SYM_TitleScreen_irq2, 0x50, 0xA0);
}
/* The title's second IRQ: mode 7 for the logo band (its matrix, centre and scroll), then .irq3 at line $B0 */
void TitleScreen_irq2(void)
{
    io_w8(0x2105, 0x07);
    io_w8(0x2107, 0x00);
    io_w8(0x210B, 0x00);
    io_w8(0x2123, 0x00);
    io_w8(0x212C, (uint8_t)g.TitleM7TM);
    io_w8(0x211F, g.TitleM7X[0]);
    io_w8(0x211F, g.TitleM7X[1]);
    io_w8(0x2120, g.TitleM7Y[0]);
    io_w8(0x2120, g.TitleM7Y[1]);
    io_w8(0x210D, (uint8_t)g.TitleM7HOfs);
    io_w8(0x210D, (uint8_t)(g.TitleM7HOfs >> 8));
    io_w8(0x210E, (uint8_t)g.TitleM7VOfs);
    io_w8(0x210E, (uint8_t)(g.TitleM7VOfs >> 8));
    io_w8(0x211B, (uint8_t)g.TitleM7Scale);
    io_w8(0x211B, (uint8_t)(g.TitleM7Scale >> 8));
    io_w8(0x211E, (uint8_t)g.TitleM7Scale);
    io_w8(0x211E, (uint8_t)(g.TitleM7Scale >> 8));
    title_irq_next(SYM_TitleScreen_irq3, 0xB0, 0);
}
void TitleScreen_irq3(void) { io_w8(0x212C, 0x11); }

/* The title screen with its music: the logo zooms in, the licence screens with the team logos along
 * their path, then the credits, until they end or a button */
void TitleScreen(void)
{
    forceblack();
    SPCTitleUpload();
    InitSprites(0x6000, 0, 0x01A0);
    io_w8(0x2108, 0x58);
    io_w8(0x2109, 0x5C);
    io_w8(0x210C, 0x05);
    g.Mode7InUse = 0xFF;
    io_w8(0x2124, 0);
    io_w8(0x2125, 0);
    io_w8(0x2126, 0);
    io_w8(0x2128, 0);
    io_w8(0x212E, 0x03);
    io_w8(0x211A, 0x80);
    io_w8(0x211C, 0);
    io_w8(0x211C, 0);
    io_w8(0x211D, 0);
    io_w8(0x211D, 0);
    g.BG1VScroll = 0xFFFF;
    g.BG2HScroll = 0;
    g.BG2VScroll = 0;
    w16(RAM_OFF_BG3HScroll, 0);
    g.BG3VScroll = 0x50;
    g.BG1HScroll = 0x0100;
    g.TitleWH1 = 0;
    g.TitleWH3 = 0;
    g.TitleWH3Low = 0;
    g.TitleBGMode = 9;
    w16(RAM_OFF_TitleM7X, 0x0200);
    w16(RAM_OFF_TitleM7Y, 0x0200);
    g.TitleM7HOfs = 0x0190;
    g.TitleM7VOfs = 0x0180;
    g.TitleM7Scale = 0x1800;
    g.TitlePathFlip = 0x1800;
    g.TitleM7TM = 0x11;
    g.TitleLogoFlags[0] = 0;
    TitleScreen_7();
    BuildTitlePath();
    g.TitleSkipMask = 0;
    g.titleskip = 0;
    FadeIn();
    while (g.brightness < 0x0F) hal_wait_interrupt();
    g.IRQHTime = 0;
    SetIRQ(SYM_TitleScreen_irq1, 0x46);
    SetFrameHook(SYM_TitleScreen_framehook);
    WaitFrame();
    WaitFrame();
    g.brightness = 0x0F;
    w16(RAM_OFF_IniDispShadow, 0x0F);
    TitleZoomIn();
    g.SfxVelocity = 0x7000;
    PlaySound(0x37);
    TitleWait(0x1E);
    static const uint16_t wh3[3] = { 0x42, 0x7D, 0xFF };
    for (uint16_t k = 0; k < 9; k += 3) {
        TitleScreen_9(k);
        TitleScreen_9((uint16_t)(k + 1));
        TitleScreen_9((uint16_t)(k + 2));
        PlaySound(0x34);
        g.TitleWH3 = wh3[k / 3];
        TitleBlankSprites();
    }
    TitleStub();
    g.TitleBGMode = 1;
    PlaySound(0x36);
    TitleWait(7);
    TitleWindowWipe();
    SPCTitleToMenu();
    MusicStart(SYM_TitleSong);
    SpriteFrameStart();
    PrintCopyright();
    SpriteFrameEnd();
    WaitFrame();
    WaitFrame();
    TitleZoomOutRight();
    SpriteFrameStart();
    PrintCopyright();
    DrawTitleSprite(9);
    g.TitleM7TM = 0x10;
    g.TitleM7Scale = 0x1800;
    g.TitleM7HOfs = 0x0190;
    g.TitleM7VOfs = 0x0180;
    SpriteFrameEnd();
    WaitFrame();
    g.MapVRAM = 0;
    Mode7MapToVRAM(unpackbuf, 0x3C, 0x3C);
    WaitFrame();
    g.TitleM7TM = 0x11;
    WaitFrame();
    TitleZoomIn();
    TitleWait(0x2D);
    TitleZoomOutLeft();
    SpriteFrameStart();
    PrintCopyright();
    DrawTitleSprite(9);
    DrawTitleSprite(0x0A);
    SpriteFrameEnd();
    WaitFrame();
    WaitFrame();
    g.TitleM7TM = 0x10;
    g.TitleM7Scale = 0x0F00;
    g.TitleM7HOfs = 0x0190;
    g.TitleM7VOfs = 0x01B0;
    w16(RAM_OFF_TitleM7X, 0x0210);
    g.TitleSkipMask = 0xF0C0;
    uint16_t n = Decompress(SYM_TitlePuckTiles, unpackbuf);
    uint32_t p0 = unpackbuf;
    DMAToVRAMHigh(&p0, n, 0x2000);
    ShuffleTitleTeams();
    g.TitlePathFlip = 0;
    LoadTitleLogo(r16(RAM_OFF_titleteams + (uint16_t)(g.TitleTeamIdx[0] << 1)));
    g.TitleM7TM = 0x11;
    WaitFrame();
    WaitFrame();
    g.FrameCount = 0;
    g.TitlePathPos = 0;
    g.TitlePathFrac = 0;
    g.TitleLogoFlags[0] = 2;
    while (g.FrameCount < 0xF0) {
        if (g.FrameCount < 0x78) DrawLicenceNHL();
        else DrawLicenceNHLPA();
        TitleScreen_5();
    }
    g.FrameCount = 0;
    StartCredits();
    WaitFrame();
    while (g.FrameCount < 0x1284 && !g.titleskip) {
        RollCredits();
        TitleScreen_5();
    }
    while (g.brightness) {
        g.brightness--;
        WaitFrame();
    }
    TitleScreen_8();
}

#define introbuf24 (0x7E0000u + RAM_OFF_introbuf)

/* Intro: the video registers from the table at p0 (BGMODE, M7SEL, TM, TS, BG1-4SC, BG12NBA,
 * BG34NBA) */
void IntroSequence_33(uint32_t p0)
{
    io_w8(0x2101, 0x03);
    io_w8(0x2115, 0x80);
    io_w8(0x2105, m8(p0));
    io_w8(0x211A, m8(p0 + 1));
    io_w8(0x212C, m8(p0 + 2));
    io_w8(0x212D, m8(p0 + 3));
    for (uint16_t k = 0; k < 4; k++) io_w8((uint16_t)(0x2107 + k), (uint8_t)(m8(p0 + 5 + 3 * k) | m8(p0 + 6 + 3 * k)));
    io_w8(0x210B, (uint8_t)(m8(p0 + 17) >> 4 | m8(p0 + 19)));
    io_w8(0x210C, (uint8_t)(m8(p0 + 21) >> 4 | m8(p0 + 23)));
}

/* Intro: x + 1 colours of CGRAM from colour y filled with colour a */
void IntroSequence_31(uint16_t a, uint16_t x, uint16_t y)
{
    io_w8(0x2121, (uint8_t)y);
    uint8_t n = (uint8_t)x;
    do {
        io_w8(0x2122, (uint8_t)a);
        io_w8(0x2122, (uint8_t)(a >> 8));
    } while (!(--n & 0x80));
}

/* Intro: the palettes, the work buffer cleared, the logo's tiles and map unpacked and sent to VRAM in
 * the mode 7 layout, the map block copy that .28 sends, the sprites unpacked */
void IntroSequence_2(void)
{
    io_w8(0x2121, 0);
    io_w8(0x2122, 0xFF);
    io_w8(0x2122, 0xFF);
    DMAToCGRAM(SYM_EALogoEPalette, 0x10, 0x30);
    DMAToCGRAM(SYM_EALogoEPalette, 0x10, 0xB0);
    DMAToCGRAM(SYM_EALogoAPalette, 0x10, 0x40);
    DMAToCGRAM(SYM_EALogoAPalette, 0x10, 0xC0);
    IntroSequence_31(0xFFFF, 0x0F, 0x20);
    IntroSequence_31(0xFFFF, 0x0F, 0x10);
    memset(WRAM_BYTES + RAM_OFF_introbuf, 0, 0x4000);
    Decompress(SYM_EALogoTiles, 0x7E0000u + RAM_OFF_introchars);
    Decompress(SYM_EALogoMap, introbuf24);
    io_w16(0x2116, 0);
    io_w8(0x2115, 0x80);
    for (uint32_t x = 0; x < 0x4000; x++) {
        io_w8(0x2118, r8(RAM_OFF_introbuf + 6 + x));
        io_w8(0x2119, r8(RAM_OFF_introchars + x));
    }
    for (uint32_t x = 0x1C00, y = 0; y < 0x0A00; x++, y += 2) {
        w8(RAM_OFF_introbuf + 0x40 + y, r8(RAM_OFF_introbuf + x));
        w8(RAM_OFF_introbuf + 0x41 + y, r8(RAM_OFF_introchars + x));
    }
    Decompress(SYM_EALogoSprites, introbuf24 + 0xA40);
}

/* Intro: the mode 7 matrix A and D = $100 (normal size) / 1 (smallest) */
void IntroSequence_23(void) { g.introM7A = g.introM7D = 0x0100; }
void IntroSequence_24(void) { g.introM7A = g.introM7D = 0x0001; }

/* Intro: the Mode 7 centre (X $200, Y $200) and the matrix $100 0 0 $100 */
void IntroSequence_22(void)
{
    io_w8(0x211F, 0x00);
    io_w8(0x211F, 0x02);
    io_w8(0x2120, 0x00);
    io_w8(0x2120, 0x02);
    io_w8(0x211B, 0x00);
    io_w8(0x211B, 0x01);
    io_w8(0x211C, 0x00);
    io_w8(0x211C, 0x00);
    io_w8(0x211D, 0x00);
    io_w8(0x211D, 0x00);
    io_w8(0x211E, 0x00);
    io_w8(0x211E, 0x01);
    IntroSequence_23();
}

/* Intro: the 18 by 10 block of map bytes in the buffer's copy (row stride 128 words from word $37),
 * copied from the map at p0 (from its byte 6) or, d1 = 0, cleared */
void IntroSequence_27(uint32_t p0, uint16_t d1)
{
    uint16_t y = 6;
    for (uint16_t row = 0; row < 10; row++) {
        uint16_t x = (uint16_t)((uint16_t)((row << 7) + 0x37) << 1);
        for (int k = 0; k < 0x12; k++, y++, x += 2)
            w8(RAM_OFF_introbuf + 0x40 + x, d1 ? m8(p0 + y) : (uint8_t)d1);
    }
}
void IntroSequence_25(void) { IntroSequence_27(0, 0); }
void IntroSequence_26(uint32_t p0) { IntroSequence_27(p0, 1); }

/* Intro: DMA channel 0, a bytes from p0 to VRAM address x */
void IntroSequence_t1xxxxxxxxxxxx(uint16_t a, uint16_t x, uint32_t p0)
{
    io_w16(0x4305, a);
    io_w16(0x4302, (uint16_t)p0);
    io_w16(0x2116, x);
    io_w8(0x4304, (uint8_t)(p0 >> 16));
    io_w8(0x4300, 0x01);
    io_w8(0x4301, 0x18);
    io_w8(0x420B, 0x01);
}

/* Intro: the buffer's map block copy to VRAM word $1C00 ($A00 bytes) */
void IntroSequence_28(void) { IntroSequence_t1xxxxxxxxxxxx(0x0A00, 0x1C00, introbuf24 + 0x40); }

/* Intro: $20 bytes from [p0] to the buffer (the palette a fade works on) */
void IntroSequence_32(uint32_t p0)
{
    for (uint16_t y = 0; y < 0x20; y += 2) w16(RAM_OFF_introbuf + y, m16(p0 + y));
}

/* Intro: the palette at p0 into the buffer, the palette job's source the buffer, 16 colours */
void IntroSequence_29(uint32_t p0)
{
    IntroSequence_32(p0);
    g.tmp1 = 0x10;
    g.tmp2 = (uint16_t)introbuf24;
    g.tmp3 = introbuf24 >> 16;
}

/* Intro: one fade step of the buffer's 16 colours, each component +1 up to 31; 1 when all are white */
uint16_t IntroSequence_30(void)
{
    uint16_t y = 1;
    for (uint16_t x = 0; x < 0x20; x += 2) {
        uint32_t o = RAM_OFF_introbuf + x;
        uint16_t v = r16(o), a;
        a = v & 0x001F;
        if (a != 0x001F) a = (a + 1) & 0x001F;
        w16(o, a);
        a = v & 0x03E0;
        if (a != 0x03E0) a = (a + 0x20) & 0x03E0;
        w16(o, a |= r16(o));
        uint16_t b = v & 0x7C00;
        if (b != 0x7C00) b = (b + 0x0400) & 0x7C00;
        w16(o, b |= r16(o));
        if ((b & 0x7FFF) != 0x7FFF) y = 0;
    }
    return y;
}

/* The intro's frame hook: the jobs of introHookFlags, $80 the map block to VRAM (.28), else $08 the
 * palette tmp2/3 (colour tmp0, tmp1 of them; with $200 the second one, introPal2Src); $10 the BG1
 * scroll; $40 the mode 7 matrix A and D */
void IntroSequence_framehook(void)
{
    if (g.introHookFlags & 0x0080) {
        IntroSequence_28();
        g.introHookFlags &= ~0x0080;
    } else if (g.introHookFlags & 0x0008) {
        DMAToCGRAM(((uint32_t)g.tmp3 << 16 | g.tmp2) & 0xFFFFFF, g.tmp1, g.tmp0);
        if (g.introHookFlags & 0x0200) DMAToCGRAM(g.introPal2Src & 0xFFFFFF, g.tmp5, g.tmp4);
        g.introHookFlags &= ~0x0208;
    }
    if (g.introHookFlags & 0x0010) {
        io_w8(0x210D, g.introBG1H[0]);
        io_w8(0x210D, g.introBG1H[1]);
        io_w8(0x210E, r8(RAM_OFF_introBG1H + 2));
        io_w8(0x210E, r8(RAM_OFF_introBG1H + 3));
        g.introHookFlags &= ~0x0010;
    }
    nmi_b = (uint8_t)(g.introHookFlags >> 8);   /* (the last load: the flags, or $0040) */
    if (g.introHookFlags & 0x0040) {
        io_w8(0x211B, (uint8_t)g.introM7A);
        io_w8(0x211B, (uint8_t)(g.introM7A >> 8));
        io_w8(0x211E, (uint8_t)g.introM7D);
        io_w8(0x211E, (uint8_t)(g.introM7D >> 8));
        g.introHookFlags &= ~0x0040;
        nmi_b = 0;
    }
}

/* Intro: the start state: BG1 scroll $180 / $190 (job $10 sends it), next state 3, d6 the timeout
 * (vcount + $2D0), d7 the earliest end (vcount + $F0), the Mode 7 registers. (The skip flag d5+2 is
 * cleared here and never set: its branches are dead.) */
void IntroSequence_3(IntroDP *s)
{
    w16(RAM_OFF_introBG1H, 0x0180);
    g.BG1HScroll = 0x0180;
    w16(RAM_OFF_introBG1H + 2, 0x0190);
    g.BG1VScroll = 0x0190;
    g.introHookFlags |= 0x0010;
    s->next = 3;
    s->d6 = (uint16_t)(g.vcount[0] + 0x02D0);
    g.d7 = (uint16_t)(g.vcount[0] + 0x00F0);
    IntroSequence_22();
}

/* Intro states (.4's jump table), each 0 to go on, 1 at the end. State 0: zoom in, matrix A and D
 * growing by $C a frame to $100, then state d5 */
uint16_t IntroSequence_6(IntroDP *s)
{
    g.introM7A = (uint16_t)(g.introM7A + 0x0C);
    g.introM7D = g.introM7A;
    if (!((uint16_t)(g.introM7A - 0x0100) & 0x8000)) {
        IntroSequence_23();
        g.introHookFlags |= 0x0040;
        s->next = s->d5;
    } else g.introHookFlags |= 0x0040;
    return 0;
}

/* State 1: the palette toward white, up to 3 steps a frame; once white, reloaded from p0, then state d5 */
uint16_t IntroSequence_7(IntroDP *s)
{
    if (IntroSequence_30() || IntroSequence_30() || IntroSequence_30()) {
        IntroSequence_32(s->p0);
        g.introHookFlags |= 0x0008;
        s->next = s->d5;
    } else g.introHookFlags |= 0x0008;
    return 0;
}

/* State 2: wait until vcount reaches d6+2, then state d5 */
uint16_t IntroSequence_8(IntroDP *s)
{
    if (!((uint16_t)(g.vcount[0] - s->d6h) & 0x8000)) s->next = s->d5;
    return 0;
}

/* (states 3 and 5: the logo's map block into the buffer from scale 1, zoomed in (state 0), then state
 * d5) */
static uint16_t intro_logo(IntroDP *s, uint32_t map, uint16_t d5)
{
    IntroSequence_24();
    s->p0 = map;
    IntroSequence_26(map);
    g.introHookFlags |= 0x00C0;
    s->d5 = d5;
    s->next = 0;
    return 0;
}

/* State 3: sound 0 and the E logo */
uint16_t IntroSequence_9(IntroDP *s)
{
    PlaySound(0);
    return intro_logo(s, SYM_EALogoEMap, 4);
}

/* (states 4 and 6: sprites 0 to n - 1 at ($38, $2F), then state next) */
static uint16_t intro_sprites(IntroDP *s, uint16_t n, uint16_t next)
{
    SpriteFrameStart();
    for (uint16_t k = 0; k < n; k++) DrawSprite(0, 0x38, 0x2F, k, introbuf24 + 0xA40);
    SpriteFrameEnd();
    s->next = next;
    return 0;
}

/* State 4: sprite 0, then state 9 */
uint16_t IntroSequence_10(IntroDP *s) { return intro_sprites(s, 1, 9); }

/* State 5: the A logo */
uint16_t IntroSequence_11(IntroDP *s) { return intro_logo(s, SYM_EALogoAMap, 6); }

/* State 6: sprites 0 and 1, then state 10 */
uint16_t IntroSequence_12(IntroDP *s) { return intro_sprites(s, 2, 10); }

/* State 7: the map block cleared, the sports palette job (16 colours from $20), zoomed (state 0), then
 * state $B */
uint16_t IntroSequence_13(IntroDP *s)
{
    IntroSequence_24();
    IntroSequence_25();
    g.tmp2 = (uint16_t)SYM_EALogoSportsPalette;
    g.tmp3 = 0x9A;
    g.tmp0 = 0x20;
    g.tmp1 = 0x10;
    g.introHookFlags |= 0x00C8;
    s->d5 = 0x0B;
    s->next = 0;
    return 0;
}

/* State 8: the text palette job (16 colours from $10), then state $D */
uint16_t IntroSequence_14(IntroDP *s)
{
    g.tmp2 = (uint16_t)SYM_EALogoTextPalette;
    g.tmp3 = 0x9A;
    g.tmp0 = 0x10;
    g.tmp1 = 0x10;
    g.introHookFlags |= 0x0008;
    s->next = 0x0D;
    return 0;
}

/* (states 9 and 10: the palette at p0 for colours from tmp0 into the buffer, faded in (state 1), then
 * state d5) */
static uint16_t intro_fade(IntroDP *s, uint32_t pal, uint16_t colour, uint16_t d5)
{
    s->p0 = pal;
    g.tmp0 = colour;
    IntroSequence_29(pal);
    s->d5 = d5;
    s->next = 1;
    return 0;
}

/* State 9: the E palette (colours from $30), then state 5; state 10: the A palette ($40), then 7 */
uint16_t IntroSequence_15(IntroDP *s) { return intro_fade(s, SYM_EALogoEPalette, 0x30, 5); }
uint16_t IntroSequence_16(IntroDP *s) { return intro_fade(s, SYM_EALogoAPalette, 0x40, 7); }

/* State 11: state 8 */
uint16_t IntroSequence_17(IntroDP *s) { s->next = 8; return 0; }

/* State 12: 30 frames in state 2, then state 8 */
uint16_t IntroSequence_18(IntroDP *s)
{
    s->d6h = (uint16_t)(g.vcount[0] + 0x1E);
    s->next = 2;
    s->d5 = 8;
    return 0;
}

/* State 13: the end once at full brightness and d7 is reached, with a button or at the d6 timeout */
uint16_t IntroSequence_19(IntroDP *s)
{
    if (g.brightness != 0x0F) return 0;
    if ((uint16_t)(g.vcount[0] - g.d7) & 0x8000) return 0;
    if (r16(RAM_OFF_JoyPad)) return 1;
    return !((uint16_t)(g.vcount[0] - s->d6) & 0x8000);
}

/* Intro: one step of the state machine (d4 = d4+2), unless the frame hook still has jobs; 1 at the end */
uint16_t IntroSequence_4(IntroDP *s)
{
    static uint16_t (*const state[14])(IntroDP *) = {
        IntroSequence_6, IntroSequence_7, IntroSequence_8, IntroSequence_9, IntroSequence_10,
        IntroSequence_11, IntroSequence_12, IntroSequence_13, IntroSequence_14, IntroSequence_15,
        IntroSequence_16, IntroSequence_17, IntroSequence_18, IntroSequence_19,
    };
    uint16_t d4 = s->next;
    if (g.introHookFlags) return 0;
    return state[d4](s);
}

/* The power-on sequence before the title (the publisher's logo in mode 7): its own sample set and song, until it
 * ends or a button. Its state machine (.4) runs round a busy loop; each state's work is done by the
 * frame hook's jobs before the next one runs. */
void IntroSequence(void)
{
    IntroDP s = { 0, 0, 0, 0, SYM_EALogoSprites };         /* (p0 as .2 leaves it) */
    g.brightness = 0;
    forceblack();
    CheckPAL();
    SPCIntroUpload();
    g.Mode7InUse = 0xFF;
    InitSprites(0x6000, 0, 0x01A0);
    g.DMAQueueHead = 0;
    g.DMAQueueTail = 0;
    IntroSequence_33(SYM_IntroSequence_1);
    IntroSequence_2();
    g.introHookFlags = 0;
    IntroSequence_3(&s);
    g.fadestep = 0;
    g.brightness = 0;
    SpriteFrameStart();
    SpriteFrameEnd();
    SetFrameHook(SYM_IntroSequence_framehook);
    ScreenOn();
    g.SfxVelocity = 0x7000;
    for (uint16_t x = 0x3C; !(x & 0x8000); x--) WaitFrame();
    SetFrameHook(SYM_IntroSequence_framehook);
    do RandomWord(); while (!IntroSequence_4(&s));
    forceblack();
}
