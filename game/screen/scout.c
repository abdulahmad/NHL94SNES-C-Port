/* scout: the scouting report (src/routines/scout). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */

/* The roster slot (from 0) of team a2's player for matchup k (centre, wings, defence, goalie). */
uint16_t GetMatchupPlayer(uint16_t k, uint16_t a2)
{
    uint32_t bank = SYM_TeamList >> 16;
    uint16_t slot = m8((SYM_GetMatchupPlayer_1 + k) & 0xFFFFFF);
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    t = (uint16_t)(t + m16(((bank << 16 | t) + 0xC) & 0xFFFFFF));
    return (uint16_t)(m8(((bank << 16 | t) + slot) & 0xFFFFFF) - 1);
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
