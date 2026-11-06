/* scout: the scouting report (src/routines/scout). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define SCOUT_NEG(a, b) NEG(a, b)
#define SCOUT_INC(p) (((p) & 0xFF0000) | (uint16_t)((p) + 1))

/* (team data a1's strings: city, name, nickname, arena) */
static uint32_t scout_team_strings(uint16_t a1)
{
    uint32_t bank = SYM_TeamList & 0xFF0000;
    return bank | (uint16_t)(a1 + m16((bank | a1) + 4));
}

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

/* (ScoutingReport's .t1xx: player d3's hot value d0 into team y's three hottest and three coldest,
 * the ones below moved down) */
static void scout_rank(uint16_t y, uint16_t d0, uint16_t d3)
{
    static const uint32_t hv[3] = { RAM_OFF_hotvalue1, RAM_OFF_hotvalue2, RAM_OFF_hotvalue3 };
    static const uint32_t hp[3] = { RAM_OFF_hotplayer1, RAM_OFF_hotplayer2, RAM_OFF_hotplayer3 };
    static const uint32_t cv[3] = { RAM_OFF_coldvalue1, RAM_OFF_coldvalue2, RAM_OFF_coldvalue3 };
    static const uint32_t cp[3] = { RAM_OFF_coldplayer1, RAM_OFF_coldplayer2, RAM_OFF_coldplayer3 };
    uint16_t v = d0, p = d3;
    for (int k = 0; k < 3; k++) {
        uint16_t t = (uint16_t)(v - r16(hv[k] + y));
        if (!t || (t & 0x8000)) continue;
        for (int j = k; j < 3; j++) {                     /* (from here each takes the one before's) */
            uint16_t ov = r16(hv[j] + y), op = r16(hp[j] + y);
            w16(hv[j] + y, v);
            w16(hp[j] + y, p);
            v = ov; p = op;
        }
        break;
    }
    v = d0; p = d3;
    for (int k = 0; k < 3; k++) {
        if (!NEG(v, r16(cv[k] + y))) continue;
        for (int j = k; j < 3; j++) {
            uint16_t ov = r16(cv[j] + y), op = r16(cp[j] + y);
            w16(cv[j] + y, v);
            w16(cp[j] + y, p);
            v = ov; p = op;
        }
        break;
    }
}

/* (ScoutingReport's .11: team a2's players' hot values, rating minus base rating of the first
 * rating column: their sum, and the hottest and coldest) */
static void scout_team(uint16_t a2)
{
    uint16_t d4 = GetPlayerCount(a2), x = 0;
    do {
        g.curplayer[0] = x;
        uint16_t d2 = GetPlayerBaseRating(1, a2);
        uint16_t d0 = (uint16_t)(GetPlayerRating(1, a2) - d2);
        w16(RAM_OFF_hotcoldsum + a2, (uint16_t)(r16(RAM_OFF_hotcoldsum + a2) + d0));
        scout_rank(a2, d0, x);
        x++;
    } while (NEG(x, d4));
}

/* (ScoutingReport's .10: the hot and cold players of both teams; the hotter team (none when the
 * sums are within 4) and whether by much (8)) */
static void scout_hotcold(void)
{
    g.hotplayer1[0] = g.hotplayer1[1] = g.hotplayer2[0] = g.hotplayer2[1] = g.hotplayer3[0] = g.hotplayer3[1] = 0xFFFF;
    g.coldplayer1[0] = g.coldplayer1[1] = g.coldplayer2[0] = g.coldplayer2[1] = g.coldplayer3[0] = g.coldplayer3[1] = 0xFFFF;
    g.hotvalue1[0] = g.hotvalue2[0] = g.hotvalue3[0] = g.hotvalue1[1] = g.hotvalue2[1] = g.hotvalue3[1] = 0xFF06;
    g.coldvalue1[0] = g.coldvalue2[0] = g.coldvalue3[0] = g.coldvalue1[1] = g.coldvalue2[1] = g.coldvalue3[1] = 0x00FA;
    g.hotcoldsum[0] = g.hotcoldsum[1] = 0;
    scout_team(0);
    scout_team(2);
    g.hotterteam = g.HomeTeam;
    g.hotextreme = 1;
    uint16_t d = (uint16_t)(g.hotcoldsum[0] - g.hotcoldsum[1]);
    if (d & 0x8000) {
        g.hotterteam = g.VisTeam[0];
        d = (uint16_t)(g.hotcoldsum[1] - g.hotcoldsum[0]);
    }
    if (d >= 8) return;
    g.hotextreme = 0;
    if (d >= 4) return;
    g.hotterteam = 0xFFFF;
}

/* (ScoutingReport's .12: the commentary's paragraph list (from InitScoutingDisplay's a0): $1D, $1C,
 * the hot team's $1E, $1F, $20 when the teams differ, $21, the end; no delays, the screen's time) */
static void scout_list(void)
{
    uint32_t a0 = InitScoutingDisplay();
    uint16_t list[7], n = 0;
    list[n++] = 0x1D;
    list[n++] = 0x1C;
    if (!(g.hotterteam & 0x8000)) list[n++] = 0x1E;
    list[n++] = 0x1F;
    if (g.VisTeam[0] != g.HomeTeam) list[n++] = 0x20;
    list[n++] = 0x21;
    list[n++] = 0xFFFF;
    for (uint16_t k = 0; k < n; k++) mset16((a0 & 0xFF0000) | (uint16_t)(a0 + 2 * k), list[k]);
    g.asv = 0;
    g.screentimer = 0x7FFF;
}

/* The scouting report screen: both teams' hot and cold players, the matchups (paged by the pads'
 * Y / B, A and on a timer) and the scrolling commentary (X starts it again; down held: no delays),
 * until Start or, in the demo, its time runs out. */
void ScoutingReport(void)
{
    g.matchupphotos = 1;
    g.matchuphold = 0;
    clearTeamStats();
    LoadScoutingScreen();
    LoadMatchupPhotos(0);
    LoadMatchupPhotos(2);
    SetupBandIRQ(8, 0x3E);
    g.curitem[0] = 0;
    g.matchuptimer = 0;
    scout_hotcold();
    scout_list();
    print(SYM_ScoutingReportPos);
    FramerOutline(0x16, 0xB);
    ShowMatchups();
    ScoutingHDMA();
    ScreenOn();
    for (;;) {
        SendTextMap();
        WaitFrame();
        ReadPads();
        uint16_t r3 = g.padnew[0];
        for (uint16_t x = 2; x < 0xA; x += 2)
            if (r16(RAM_OFF_cont1team + x)) r3 |= r16(RAM_OFF_padnew + x);
        if (r3 & 0x1000) break;
        if (r3 & 0x0800) WaitSidePadsRelease();
        if (r3 & 0x4000) {
            g.matchuphold = 0xFFFF;
            uint16_t t = (uint16_t)(g.curitem[0] - 1);
            g.curitem[0] = t & 0x8000 ? 6 : t;
            ShowMatchups();
            g.matchuptimer = 0;
            continue;
        }
        if (r3 & 0x8080) goto next;
        if (r3 & 0x0040) {
            print(SYM_ScoutingReportPos2);
            eraser(0x14, 9, g.blankchar);
            scout_list();
            continue;
        }
        if (!g.matchuphold) {
            uint16_t t = (uint16_t)(g.matchuptimer + 1);
            if (t >= 0x12C) goto later;
            g.matchuptimer = t;
        }
        g.asv = 0;
        if ((g.JoyPad[0] | g.JoyPad[1]) & 0x0400) g.asv = 0xFFFF;
        UpdateScoutingDisplay();
        if (g.matchuphold || g.notdemo) continue;
        g.screentimer--;
        if (g.screentimer & 0x8000) break;
        continue;
next:
        g.matchuphold = 0xFFFF;
later:
        {
            uint16_t t = (uint16_t)(g.curitem[0] + 1);
            g.curitem[0] = t >= 7 ? 0 : t;
        }
        ShowMatchups();
        g.matchuptimer = 0;
    }
    g.matchupphotos = 0;
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

/* Set up the scouting report screen: fade out (the team logo sprites unpacked meanwhile), the PPU's
 * maps and sprites, the small font, framer, team block, outline, menu back and Ron Barr tiles, the
 * menu back and Ron Barr maps in text map 2, the palettes, a clear text map, the sprites and the
 * picture map */
void LoadScoutingScreen(void)
{
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
    io_w16(0x2116, (uint16_t)((uint16_t)(g.blankchar << 4) + 0x1000 + (g.blankchar >> 12 & 1)));   /* (asl's carry) */
    g.smallfontchars = (uint16_t)(g.blankchar + 1);
    g.outlinechars = g.smallfontchars;
    FillVRAM(0, 0x20, 0xFFFF, 0);
    uint16_t n = Decompress(SYM_SmallFontTiles, unpackbuf);
    uint16_t d0 = n >> 5;
    g.outlinechars = (uint16_t)(g.outlinechars + d0);
    g.Framercset = (uint16_t)(d0 + g.smallfontchars);
    remap(unpackbuf, SYM_PixelRemap8F, d0);
    g.outlinechars = (uint16_t)(g.outlinechars + 4);
    g.scorechars = (uint16_t)(g.Framercset + 4);
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
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

void ScoutingHDMA(void) { start_hdma(0x0C, 0xB3, (uint16_t)SYM_ScoutingHDMA_2, (uint16_t)SYM_ScoutingHDMA_4); }

/* The roster slot (from 0) of team a2's player for matchup k (centre, wings, defence, goalie). */
uint16_t GetMatchupPlayer(uint16_t k, uint16_t a2)
{
    uint32_t bank = SYM_TeamList >> 16;
    uint16_t slot = m8((SYM_GetMatchupPlayer_1 + k) & 0xFFFFFF);
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    t = (uint16_t)(t + m16(((bank << 16 | t) + 0xC) & 0xFFFFFF));
    return (uint16_t)(m8(((bank << 16 | t) + slot) & 0xFFFFFF) - 1);
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

/* (DrawMatchupPictures: the box at column x of the logo f (palette by LogoPaletteTable into colour
 * y, the sprite drawn with attribute a at sx), its picture box cleared and the logo box map
 * pasted into textmap2) */
static void matchup_logo(uint16_t f, uint16_t col, uint16_t a, uint16_t sx, uint16_t x)
{
    uint16_t k = (uint16_t)(f << 2);
    SetColours((uint32_t)m16(SYM_LogoPaletteTable + 2 + k) << 16 | m16(SYM_LogoPaletteTable + k), 0x10, col);
    DrawSprite(a, sx, 0xC, f, logosprbuf);
    ClearPictureBox(x, 1);
    uint32_t p0 = 0x7E0000u + RAM_OFF_logoboxmap, p1 = 0x7E0000u + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, 0, x, 1, 0x20);
}

/* (DrawMatchupPictures: player d0's picture for team a2 at column x, its box map pasted) */
static void matchup_player(uint16_t d0, uint16_t a2, uint16_t x)
{
    DrawPlayerPicture(d0, a2, x, 1);
    uint32_t p0 = 0x7E0000u + RAM_OFF_picboxmap, p1 = 0x7E0000u + RAM_OFF_textmap2;
    PasteMap(&p0, &p1, 0, x, 1, 0x20);
}

/* Draw the two 6 x 6 pictures, visitors at column 1 and home at $18: a team logo (setupvis /
 * setuphome) or, when that is negative, the matchup player's picture (matchupvis / matchuphome;
 * the team a2 as the caller left it, or the other box's when both are pictures); unless the player
 * stats are up, the pictures' tiles to VRAM (when one is a picture) and both text maps sent. */
void DrawMatchupPictures(uint16_t a2)
{
    if (g.setupvis & 0x8000) {
        if (g.setuphome & 0x8000) a2 = 2;
        matchup_player(g.matchupvis, a2, 1);
    } else matchup_logo(g.setupvis, 0xA0, 0x2400, 0xC, 1);
    if (g.setuphome & 0x8000) {
        if (g.setupvis & 0x8000) a2 = 0;
        matchup_player(g.matchuphome, a2, 0x18);
    } else matchup_logo(g.setuphome, 0xB0, 0x2600, 0xC4, 0x18);
    if (g.playerstatson) return;
    if ((g.setupvis & 0x8000) || (g.setuphome & 0x8000)) {
        uint32_t p0 = 0x7F0000u | g.homepicaddr;
        WaitFrame();
        DMAToVRAM(&p0, 0x600, (uint16_t)((uint16_t)(g.picchars << 4) + 0x2000), 0);
        p0 = 0x7F0000u | g.vispicaddr;
        DMAToVRAM(&p0, 0x600, (uint16_t)((uint16_t)(g.picchars << 4) + 0x2300), 0);
    } else WaitFrame();
    SendTextMapRows();
    SendTextMap2Rows();
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

/* Name player a of the home team / the visitors from column d0 */
void ScoutHomePlayer(uint16_t a, uint16_t d0) { ScoutPlayerName(a, 0, d0); }

void ScoutVisPlayer(uint16_t a, uint16_t d0) { ScoutPlayerName(a, 2, d0); }

/* Type the name of player d0 of team a2 (0 home, 2 visitors) from column x */
void ScoutPlayerName(uint16_t d0, uint16_t a2, uint16_t x)
{
    uint32_t a0 = GetPlayerNamePointer(a2, d0);
    g.mesarea[0] = 2;
    appstring(a0, RAM_OFF_mesarea);
    ScoutTypeText(RAM_OFF_mesarea, x);
}

void GetTeamNickname_vis(uint16_t d0) { GetTeamNickname(g.tmdata[1], d0); }

void GetTeamNickname_home(uint16_t d0) { GetTeamNickname(g.tmdata[0], d0); }

void GetTeamNickname(uint16_t a1, uint16_t d0) { ScoutAppend(AdvanceStringPtr(scout_team_strings(a1), 2), d0); }

/* Script codes %r (the home arena), %H / %V (the full names), %h / %v (the cities), %n / %m (the
 * nicknames) */
void GetTeamArena_home(uint16_t d0) { ScoutAppend(AdvanceStringPtr(scout_team_strings(g.tmdata[0]), 3), d0); }

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

void ScoutVisName(uint16_t d0) { ScoutAppend(GetTeamLongName(g.VisTeam[0]), d0); }

void ScoutHomeName(uint16_t d0) { ScoutAppend(GetTeamLongName(g.HomeTeam), d0); }

void GetTeamCity_vis(uint16_t d0) { GetTeamCity(g.tmdata[1], d0); }

void GetTeamCity_home(uint16_t d0) { GetTeamCity(g.tmdata[0], d0); }

/* The city / the nickname (the first / the third string) of team data a1, from column d0 */
void GetTeamCity(uint16_t a1, uint16_t d0) { ScoutAppend(scout_team_strings(a1), d0); }

/* Type string a1 (copied to a fresh mesarea) from column d0 */
void ScoutAppend(uint32_t a1, uint16_t d0)
{
    g.mesarea[0] = 2;
    appstring(a1, RAM_OFF_mesarea);
    ScoutTypeText(RAM_OFF_mesarea, d0);
}

/* (ScoutTypeText's .3: type the string at a1 as one word, a word more counted in PlayerScrollCtr;
 * returns the column d0 moved on by its length) */
uint16_t GetTeamCity_home_3(uint32_t a1, uint16_t d0)
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
            d0 = GetTeamCity_home_3(a1, d0);
            g.PlayerScrollCtr--;
            a1 = (a1 & 0xFF0000) | (uint16_t)(a1 + d4);
            mset16(a1, (uint16_t)(n - d4));
            if (!SCOUT_NEG(d0, 0x15)) d0 = d4;
            y = 2;
        }
        y++;
    } while (SCOUT_NEG(y, m16(a1)));
    GetTeamCity_home_3(a1, d0);
}

/* Where a scouting report name is printed: the record's position, column $14 for the home side */
void ScoutNamePos(uint16_t a2)
{
    print(SYM_ScoutNamePosAt + 5);
    if (!a2) g.printx = 0x14;
}
