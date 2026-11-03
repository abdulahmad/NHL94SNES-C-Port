/* setup: game setup, the scouting report and the sort list (src/routines/setup). */
#include "game.h"

/* Clear the team stats ($1AC bytes from tmPwrGoals, the goalie choices kept) and both penalty box
 * lists, then set up the teams */
void clearTeamStats(void)
{
    uint16_t g0 = g.tmgoalie[0], g1 = g.tmgoalie[1];
    for (int x = 0; x < 0x1AC; x += 2) w16(RAM_OFF_tmPwrGoals + x, 0);
    g.tmgoalie[1] = g1;
    g.tmgoalie[0] = g0;
    w16(RAM_OFF_tmboxlist, 0xFFFF);
    w16(RAM_OFF_tmboxlist + maxros, 0xFFFF);
    setteams();
}

/* Point both team structures at their team: sort start, team number and data pointer */
void setteams(void)
{
    g.tmsort[0] = 0;
    g.tmsort[1] = 0xC;
    uint16_t h = g.HomeTeam, v = r16(RAM_OFF_VisTeam);
    w16(RAM_OFF_tmteam, h);
    g.tmdata[0] = rom16(SYM_TeamList + (uint16_t)(h << 2));
    w16(RAM_OFF_tmteam + 2, v);
    g.tmdata[1] = rom16(SYM_TeamList + (uint16_t)(v << 2));
}

/* Sort the objects by screen position for drawing: Ylist the y (x on the horizontal rink) of the 16
 * objects, then a bubble sort of OOlist (object * 2 bytes), OOlistpos following */
void SprSort(void)
{
    for (int o = 0; o < 16; o++) g.Ylist[o] = r16((g.sfhormode ? RAM_OFF_Xpos : RAM_OFF_Ypos) + 2 * o);
    uint16_t swaps;
    do {
        swaps = 0;
        for (int x = 0; x < 15; x++) {
            uint8_t d0 = g.OOlist[x], d1 = g.OOlist[x + 1];
            uint16_t diff = (uint16_t)(r16(RAM_OFF_Ylist + d0) - r16(RAM_OFF_Ylist + d1));
            if ((diff & 0x8000) || !diff) continue;
            g.OOlist[x] = d1; g.OOlist[x + 1] = d0;
            w16(RAM_OFF_OOlistpos + d1, (uint16_t)x);
            w16(RAM_OFF_OOlistpos + d0, (uint16_t)(x + 1));
            swaps++;
        }
    } while (swaps);
}

/* Copy both teams' uniform palettes (rows 12 and 2 home, 13 and 3 visitors) and the player marker
 * palette (row 10) into palbuffer */
void SetTeamColors(void)
{
    const uint32_t pb = 0x7E0000u | RAM_OFF_palbuffer;
    uint32_t p0 = (SYM_HomeUniformPalettes & 0xFF0000) | (uint16_t)((uint16_t)(g.HomeTeam << 5) + (uint16_t)SYM_HomeUniformPalettes);
    MemCopy(p0, pb + 12 * 0x20, 0x20);
    MemCopy(p0, pb + 2 * 0x20, 0x20);
    p0 = (SYM_AwayUniformPalettes & 0xFF0000) | (uint16_t)((uint16_t)(g.VisTeam[0] << 5) + (uint16_t)SYM_AwayUniformPalettes);
    MemCopy(p0, pb + 13 * 0x20, 0x20);
    MemCopy(p0, pb + 3 * 0x20, 0x20);
    MemCopy(SYM_PlayerMarkerPalette, pb + 10 * 0x20, 0x20);
}

/* Set the team colours and send the whole palette to CGRAM */
void setplayercolors(void)
{
    SetTeamColors();
    DMAToCGRAM(0x7E0000u | RAM_OFF_palbuffer, 0x100, 0);
}

/* Reset the six players of each team: no animation in progress, the ready (goalie) or glide
 * animation, no impact, collision block, backwards or lock */
void resetplstuff(void)
{
    for (uint16_t a2 = 0; a2 < 4; a2 += 2) {
        g.tmflags[a2 >> 1] &= 0xFFEF;
        uint16_t x = g.tmsort[a2 >> 1];
        for (int k = 0; k < 6; k++, x = (uint16_t)(x + 2)) {
            int o = x >> 1;
            g.pf2aip[o] = 0;
            g.pflags[o] &= 0x2FFF;
            if (g.position[o] & 0x8000) continue;
            SetSPAFromA(o, (uint16_t)(g.position[o] ? SYM_SPAglide : SYM_SPAgready));
            g.impact[o] = 0;
            g.nopuck[o] = 0;
            g.pflags[o] &= 0xFFF6;
            g.pfnc[o] = 0;
            g.pfrev[o] = 0;
            g.pfalock[o] = 0;
        }
    }
}

/* The scouting report's shared return (nothing to do) */
void UpdateScoutingDisplay_rtl(void) {}

/* The next line of the scouting report (column 0); once 8 lines are on the screen the 8 text rows
 * move up a row */
void ScrollDisplayUp(void)
{
    g.DispAttribCtr = 0;
    g.VertLineScrolling++;
    if ((int16_t)g.VertLineScrolling < 8) { UpdateScoutingDisplay_rtl(); return; }
    g.VertLineScrolling--;
    uint32_t p1 = 0x7E0000u | (uint16_t)(RAM_OFF_textmap + 0x384);
    for (int r3 = 7; r3 >= 0; r3--) {
        uint32_t p0 = (p1 & 0xFFFF0000u) | (uint16_t)(p1 + 0x40);
        MemCopy(p0, p1, 0x28);
        p1 = p0;
    }
}

/* Reset the rink state (black screen, scroll 0, no pause, the sort objects; the players' goals
 * flipped for the other direction of play), set the colours and fade in */
void setupice_reset(void)
{
    forceblack();
    g.Oldrow = 0x03E7;
    g.sflags &= (uint16_t)~sfpz;
    g.Hpos = 0;
    g.Vpos = 0;
    g.vcountmark[1] = (uint16_t)(g.vcount[1] - 2);
    defaultsprites_0();
    nullrtl();
    if (g.gmode & gmdir)
        for (int o = 11; o >= 0; o--) g.pfgoal[o] ^= 1;
    setplayercolors();
    g.fadestep = 0;
    FadeIn();
    ScreenOn();
}


/* (setupice: unpack tile set p0 to unpackbuf; its size in bytes, *tiles its tiles (32 bytes) and
 * *c the carry the last shift leaves, for the adc after it) */
static uint16_t unpack(uint32_t p0, uint16_t *tiles, uint16_t *c)
{
    uint16_t n = Decompress(p0, unpackbuf);
    *tiles = n >> 5;
    *c = n >> 4 & 1;
    return n;
}

/* (setupice: the unpacked tiles to VRAM, after the last set) */
static void send_tiles(uint16_t n)
{
    uint32_t p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, 0);
}

/* Set up the rink screen once the screen has faded out: the video mode, the rink palette (rows 0-7
 * and 8-15), the game sound driver, and every tile set unpacked into VRAM one after the other, each
 * set's first tile kept (the rink, the centre ice logo, faceoff, rink logo, refs, a blank tile, the
 * small font and Framer box remapped, the big font, the team blocks); then setupice_reset */
void setupice(void)
{
    while (g.brightness) hal_wait_interrupt();
    forceblack();
    g.charofs = 0xFF00;
    g.charofs2 = 0;
    io_w8(0x2101, 0x03);
    io_w8(0x2105, 0x01);
    io_w8(0x2107, 0x00);
    io_w8(0x2108, 0x05);
    io_w8(0x210B, 0x12);
    io_w8(0x212C, 0x13);
    for (int x = 0xFE; x >= 0; x -= 2) {
        uint16_t v = rom16(SYM_RinkPalette + x);
        w16(RAM_OFF_palbuffer + x, v);
        w16(RAM_OFF_palbuffer + 8 * 0x20 + x, v);
    }
    FadeOutSound();
    while (g.SoundFadeSpeed) hal_wait_interrupt();
    SPCGameUpload();
    uint16_t t, c, n = unpack(SYM_IceRinkTiles, &t, &c);
    g.centericechars = t;
    uint32_t p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x1000, 0);
    LoadCenterIce();
    n = unpack(SYM_FaceOffTiles, &t, &c);
    g.EASNcset = (uint16_t)(t + g.faceoffvrcset + c);
    send_tiles(n);
    n = unpack(SYM_RinkLogoTiles, &t, &c);
    g.refcharshor = (uint16_t)(t + g.EASNcset + c);
    send_tiles(n);
    n = unpack(SYM_RefTilesHor, &t, &c);
    g.refchars[0] = (uint16_t)(t + g.refcharshor + c);
    send_tiles(n);
    n = unpack(SYM_RefTiles, &t, &c);
    g.blankchar = (uint16_t)(t + g.refchars[0] + c);
    g.smallfontchars = (uint16_t)(g.blankchar + 1);
    send_tiles(n);
    FillVRAM(0, 0x20, 0xFFFF, 0);
    unpack(SYM_SmallFontTiles, &t, &c);
    uint32_t s = (uint32_t)t + g.smallfontchars + c;
    g.Framercset = (uint16_t)s;
    g.BigFontChars = (uint16_t)(g.Framercset + 4 + (s >> 16));
    remap(unpackbuf, SYM_PixelRemap83, t);
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
    n = unpack(SYM_BigFontTiles, &t, &c);
    g.scorechars = (uint16_t)(t + g.BigFontChars + c);
    send_tiles(n);
    n = unpack(SYM_TeamBlocksTiles, &t, &c);
    g.ExtraChars = (uint16_t)(t + g.scorechars + c);
    send_tiles(n);
    setupice_reset();
}


/* Draw team d1's block at printx / printy: its name as letter sprites (8 pixels left on a player
 * card, 4 right and down there and in the pause menu), then, unless the horizontal rink's blocks
 * are only being redrawn, its colours (palette 7 for the home team, 6 for the visitors; both when
 * the teams are the same) and its logo map (2 rows of 9 tiles) in the text map */
void DrawTeamBlocks(uint16_t d1, uint32_t a1, uint16_t a2)
{
    uint16_t x = (uint16_t)((uint16_t)(g.printx << 3) + 0x24);
    if (g.cardblock) x = (uint16_t)(x - 8);
    uint16_t y = (uint16_t)((uint16_t)(g.printy << 3) + 3);
    if (g.pausesubmenu || g.cardblock) {
        x = (uint16_t)(x + 4);
        y = (uint16_t)(y + 4);
    }
    PrintTeamSprites(x, y, d1, a1, a2);
    if (g.horblocks && !g.redrawblocks) return;
    uint16_t r3 = 0x1800;
    y = 0x60;
    if (d1 == g.HomeTeam) {
        r3 = 0x1C00;
        y = 0x70;
    }
    for (;;) {
        uint16_t k = (uint16_t)(d1 << 2);
        SetColours((uint32_t)rom16(SYM_TeamBlockPalettes + 2 + k) << 16 | rom16(SYM_TeamBlockPalettes + k), 0x10, y);
        if (g.HomeTeam != g.VisTeam[0] || r3 == 0x1800) break;
        r3 = 0x1800;
        y = 0x60;
    }
    uint32_t map = SYM_TeamBlocksMap + 6;
    uint16_t t = (uint16_t)(g.printy << 5);
    uint32_t s = (uint32_t)t + g.printx + (g.printy >> 11 & 1);   /* (the carries of the shifts) */
    t = (uint16_t)s;
    x = (uint16_t)((uint16_t)(t << 1) + g.printm + (t >> 15));
    y = 0;
    for (int k = 0; k < 9; k++, y += 2, x += 2)
        w16(RAM_OFF_textmap + x, (uint16_t)(rom16(map + y) + g.scorechars + g.printfontset + r3));
    for (int k = 0; k < 9; k++, y += 2, x += 2)
        w16(RAM_OFF_textmap + 23 * 2 + x, (uint16_t)(rom16(map + y) + g.scorechars + g.printfontset + r3));
}

#define WRAM24(off) (0x7E0000u + (off))

/* (PlayoffScreen's .t1xxxxx / .7x: one 8 x 8 tile of brackettiles at x (its two plane bytes a row),
 * row 0 from pattern 1 and the others from r0: bits 0-3 give the four bytes, set the values in v,
 * clear in c) */
static void bracket_tile(uint16_t x, uint8_t r0, const uint8_t v[4], const uint8_t c[4])
{
    uint8_t p = 1;
    for (int y = 7; y >= 0; y--, x = (uint16_t)(x + 2)) {
        uint32_t b = WRAM24(RAM_OFF_brackettiles) + x;
        mset8(b, p & 1 ? v[0] : c[0]);
        mset8(b + 1, p & 2 ? v[1] : c[1]);
        mset8(b + 0x10, p & 4 ? v[2] : c[2]);
        mset8(b + 0x11, p & 8 ? v[3] : c[3]);
        p = r0;
    }
}

/* (PlayoffScreen's .9: the team block colour entry x (bytes of colourbuf: its tile and attribute,
 * twice; the attribute's palette from y) and the bracket tiles x * 16 (from bits 1-4 of y) and the
 * next) */
static void bracket_entry(uint16_t x, uint16_t y)
{
    static const uint8_t v1[4] = { 0xFF, 0x7F, 0x7F, 0x7F }, c1[4] = { 0x80, 0, 0, 0 };
    static const uint8_t v2[4] = { 0xFF, 0xFF, 0xFF, 0xFF }, c2[4] = { 0, 0, 0, 0 };
    uint16_t t = (uint16_t)(x << 4);
    uint32_t cb = WRAM24(RAM_OFF_colourbuf) + x;
    uint8_t a = (uint8_t)((x & 0xFF) >> 1);
    mset8(cb, a);
    mset8(cb + 2, (uint8_t)(a + 1));
    a = (uint8_t)((((y & 0xFF) >> 3) & 0x1C) + 0x30);
    if (x >= 0x200) a |= 1;
    mset8(cb + 1, a);
    mset8(cb + 3, a);
    uint8_t r0 = (uint8_t)(((y & 0xFF) >> 1) & 0x0F);
    bracket_tile(t, r0, v1, c1);
    bracket_tile((uint16_t)(t + 0x20), r0, v2, c2);
}

/* (PlayoffScreen: send the columns r7 of the two maps (screenmap 28 rows, screenmap2 32) that the
 * scroll brought on) */
static void playoff_column(uint16_t r7)
{
    uint16_t x = (uint16_t)(r7 - 0x20) & 0x3F;
    g.MapVRAM = 0;
    MapPartToVRAM(WRAM24(RAM_OFF_screenmap), x, 0, r7, 0, 1, 0x1C);
    g.MapVRAM = 0x3800;
    MapPartToVRAM(WRAM24(RAM_OFF_screenmap2), x, 0, r7, 0, 1, 0x20);
}

/* The playoff tree screen (in the playoffs only; the playoff song from the second round): the menu
 * background, the bracket tiles and colours for every team, the teams of the round's tree in big
 * letters with their blocks, the bracket lines, the series scores, the Stanley Cup and the
 * password; then it scrolls with the pad (left / right, to the round's stops, within its limits)
 * until start, the team colour pulsing */
void PlayoffScreen(void)
{
    g.phototeams[0] = 0xFFFF;
    g.phototeams[1] = 0xFFFF;
    g.charofs = 0;
    g.charofs2 = 0;
    if (!g.OptPlayMode || g.OptPlayMode == 4) goto done;
    if (g.playoffRoundIdx >= 2 && g.SongPtr != 0xED31) {
        FadeOutSound();
        forceblack();
        while (g.MusicOn) hal_wait_interrupt();
        MusicStart(SYM_PlayoffSong);
    } else forceblack();
    io_w8(0x2105, 0x09);
    io_w8(0x2107, 0x01);
    io_w8(0x2108, 0x08);
    io_w8(0x2109, 0x39);
    io_w8(0x210B, 0x31);
    io_w8(0x210C, 0x04);
    io_w8(0x2101, 0x03);
    io_w8(0x212C, 0x07);
    uint16_t n = Decompress(SYM_MenuBackTiles, unpackbuf);
    uint32_t p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x3000, 0);
    n = Decompress(SYM_MenuBackMap, unpackbuf);
    p0 = unpackbuf + 0x246;
    DMAToVRAM(&p0, 0x240, 0x0800, 0);
    DMAToVRAM(&p0, (uint16_t)(n - 0x240), 0x0920, 0);
    Decompress(SYM_MenuBackPalette, unpackbuf);
    DMAToCGRAM(unpackbuf, 0x10, 0);
    n = Decompress(SYM_PlayoffTiles, unpackbuf);
    g.smallfontchars = n >> 5;
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x1000, 0);
    Decompress(SYM_PlayoffPalette, WRAM24(RAM_OFF_palbuffer));
    n = Decompress(SYM_SmallFontTiles, unpackbuf);
    uint16_t d0 = n >> 5;
    g.BigFontChars = (uint16_t)(d0 + g.smallfontchars + (n >> 4 & 1));   /* (the last lsr's carry) */
    remap(unpackbuf, SYM_PixelRemap8F, d0);
    n = Decompress(SYM_SmallFontTiles, unpackbuf);
    g.scorechars = (uint16_t)((n >> 5) + g.BigFontChars + (n >> 4 & 1));
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, 0);
    for (int k = 4; k < 8; k++) w16(RAM_OFF_palbuffer + k * 0x20 + 2, 0x031A);
    uint32_t p1 = WRAM24(RAM_OFF_palbuffer + 0x80);
    for (int x = 0x1B; x >= 0; x--) {
        uint16_t t = (uint16_t)(x << 2);
        uint32_t tp = (uint32_t)rom16(SYM_TeamBlockPalettes + 2 + t) << 16 | rom16(SYM_TeamBlockPalettes + t);
        uint16_t y = rom8(SYM_PlayoffScreen_8 + (uint16_t)(x << 1));
        mset16(p1 + y, m16(tp + 8));
        bracket_entry((uint16_t)(x << 3), y);
        y = rom8(SYM_PlayoffScreen_8 + (uint16_t)((x << 1) + 1));
        mset16(p1 + y, m16(tp + 0x12));
        bracket_entry((uint16_t)(((x << 1) + 1) << 2), y);
    }
    g.cupchars = (uint16_t)(g.scorechars + 0x70);
    p0 = WRAM24(RAM_OFF_brackettiles);
    DMAToVRAM(&p0, 0x0E00, 0xFFFF, 0);
    Decompress(SYM_StanleyCupPalette, unpackbuf);
    MemCopy(unpackbuf, WRAM24(RAM_OFF_palbuffer + 3 * 0x20), 0x20);
    n = Decompress(SYM_StanleyCupTiles, unpackbuf);
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0xFFFF, 0);
    n = Decompress(SYM_BigLetterTiles, WRAM24(RAM_OFF_bigtextbuf));
    LinearToPlanar(WRAM24(RAM_OFF_bigtextbuf), (uint16_t)RAM_OFF_hlsavebuf, n);
    MemFill(WRAM24(RAM_OFF_bigtextbuf), 0x8000, 0);
    MemFill(WRAM24(RAM_OFF_screenmap + 6), 0x4002, 0);
    uint32_t a1 = WRAM24(RAM_OFF_potree);
    uint16_t y = (uint16_t)(g.playoffRoundIdx << 1);
    uint32_t a0 = (SYM_PlayoffScreen_10 & 0xFF0000) | (uint16_t)(rom16(SYM_PlayoffScreen_10 + y) + (uint16_t)SYM_PlayoffScreen_10 + (g.playoffRoundIdx >> 15));   /* (asl's carry) */
#define NEXT(p) m8((p)++)
    uint16_t d4 = NEXT(a0);
    print(SYM_PlayoffScreenPos);
    g.bigtextchar = 1;
    do {
        g.printx = NEXT(a0);
        g.printa = 0x3000;
        g.printy = NEXT(a0);
        uint16_t tmp4 = NEXT(a1);
        if (!((tmp4 ^ r16(RAM_OFF_potree + g.potreeteam)) & 0xFF)) g.printa = 0x3400;
        p0 = (SYM_TeamPlaceNames & 0xFF0000) | rom16(SYM_TeamPlaceNames + (uint16_t)(tmp4 << 1));
        uint16_t px = g.printx;
        w16(RAM_OFF_jmlvector, 0x005C);
        w16(RAM_OFF_jmlvector + 1, (uint16_t)SYM_BigLetterPlayoff);
        w16(RAM_OFF_jmlvector + 3, SYM_BigLetterPlayoff >> 16);
        RenderBigText(p0);
        g.printx = px;
        uint32_t s = (uint32_t)(uint16_t)(tmp4 << 3) + (uint16_t)RAM_OFF_colourbuf;
        uint32_t a2 = (uint32_t)(uint16_t)((RAM_OFF_colourbuf + 0x7E0000u) >> 16) + (s >> 16);
        a2 = a2 << 16 | (uint16_t)s;
        uint16_t x = (uint16_t)(((uint16_t)(g.printy << 7) + g.printx + (g.printy >> 9 & 1)) << 1);   /* (asl's carry) */
        uint16_t v = (uint16_t)(m16(a2) + g.scorechars);
        w16(RAM_OFF_screenmap + 6 + x, v);
        w16(RAM_OFF_screenmap + 0x16 + x, (uint16_t)(v | 0x4000));
        x = (uint16_t)(x + 2);
        a2 = (a2 & 0xFF0000) | (uint16_t)(a2 + 2);
        v = (uint16_t)(m16(a2) + g.scorechars);
        for (int k = 0; k < 7; k++, x = (uint16_t)(x + 2)) w16(RAM_OFF_screenmap + 6 + x, v);
        a2 = (a2 & 0xFF0000) | (uint16_t)(a2 + 2);
        v = (uint16_t)((uint16_t)(m16(a2) + g.scorechars) | 0x8000);
        w16(RAM_OFF_screenmap + 0xF6 + x, v);
        w16(RAM_OFF_screenmap + 0x106 + x, (uint16_t)(v | 0xC000));
        x = (uint16_t)(x + 2);
        a2 = (a2 & 0xFF0000) | (uint16_t)(a2 + 2);
        v = (uint16_t)((uint16_t)(m16(a2) + g.scorechars) | 0x8000);
        for (int k = 0; k < 7; k++, x = (uint16_t)(x + 2)) w16(RAM_OFF_screenmap + 0xF6 + x, v);
    } while (--d4);
    p0 = WRAM24(RAM_OFF_bigtextbuf);
    DMAToVRAM(&p0, (uint16_t)((uint16_t)(g.bigtextchar + 1) << 4), 0x4000, 0);
    for (;;) {
        uint16_t b = NEXT(a0);
        if (b == 0xFF) break;
        uint32_t tmp0 = 0x990000u | (uint16_t)((uint16_t)(b << 1) + 0xB72C);
        uint16_t x = (uint16_t)(NEXT(a0) << 1);
        uint16_t w = rom16(SYM_PlayoffBracketMap);
        for (uint16_t r = rom16(SYM_PlayoffBracketMap + 2); r; r--) {
            w16(RAM_OFF_screenmap + 0x206 + x, m16(tmp0));
            uint32_t s1 = (uint32_t)(uint16_t)tmp0 + w;
            uint32_t s2 = (uint32_t)(uint16_t)s1 + w + (s1 >> 16);
            tmp0 = (tmp0 & 0xFF0000) | (uint16_t)s2;
            x = (uint16_t)(x + 0x100 + (s2 >> 16));
        }
    }
    uint16_t d2h = (uint16_t)(NEXT(a0) << 3), d0h = (uint16_t)(NEXT(a0) << 3), d3h = 0;
    print(SYM_PlayoffScreenPos2);
    if (g.bosgames != 7) {
        d4 = NEXT(a0);
        for (uint16_t a2 = 0; d4; d4--, a2 = (uint16_t)(a2 + 2)) {
            g.printx = NEXT(a0);
            uint16_t b = NEXT(a0);
            g.printy = (uint16_t)(b << 2);
            g.mesarea[0] = 5;
            uint32_t s = (uint32_t)r16(RAM_OFF_gspotwins + a2) + 0x30 + (b >> 6 & 1);   /* (the second asl's carry) */
            w16(RAM_OFF_mesarea + 2, (uint16_t)s);
            w16(RAM_OFF_mesarea + 3, 0x002D);
            w16(RAM_OFF_mesarea + 4, (uint16_t)(r16(RAM_OFF_gspobwins + a2) + 0x30 + (s >> 16)));
            g.printm = 0x6E82;
            print(RAM_OFF_mesarea);
        }
    }
#undef NEXT
    Decompress(SYM_StanleyCupMap, unpackbuf);
    p0 = unpackbuf;
    p1 = WRAM24(RAM_OFF_screenmap + 6);
    PasteMap(&p0, &p1, (uint16_t)(g.cupchars | 0x0C00), 0x3D, 8, 0x80);
    print(SYM_N8hTxt);
    g.printm = 0x6E82;
    PWprint();
    g.screenmap[0] = 0x80;
    g.screenmap2[0] = 0x80;
    g.screenmap[1] = 0x20;
    g.screenmap2[1] = 0x20;
    g.MapVRAM = 0x3800;
    MapPartToVRAM(WRAM24(RAM_OFF_screenmap2), 0, 0, 0x20, 0, 0x40, 0x20);
    g.MapVRAM = 0;
    MapPartToVRAM(WRAM24(RAM_OFF_screenmap), 0, 0, 0x20, 0, 0x40, 0x1C);
    g.Hscroll = 0x180;
    g.BG1HScroll = 0x80;
    w16(RAM_OFF_BG3HScroll, 0x80);
    g.BG2HScroll = 0;
    w16(RAM_OFF_BG3VScroll, 0xFFFC);
    g.palbuffer[0x10 + 1] = 0x1D47;
    g.palbuffer[0x10 + 5] = 0x1D47;
    g.palbuffer[0x10 + 3] = 0xFFFF;
    DMAToCGRAM(WRAM24(RAM_OFF_palbuffer + 0x20), 0x70, 0x10);
    ScreenOn();
    for (;;) {
        WaitFrame();
        ReadPads();
        if ((g.padnew[0] | g.padnew[1]) == 0x1000) break;
        if (g.vcount[0] == g.d7) continue;
        g.d7 = g.vcount[0];
        PulseColour((uint16_t)(RAM_OFF_palbuffer + 0x2E), 0x7E);
        DMAToCGRAM(WRAM24(RAM_OFF_palbuffer + 0x20), 0x10, 0x10);
        uint16_t j = g.JoyPad[0] | g.JoyPad[1];
        if (j & 0x0200) d3h = 0xFFFE;
        else if (j & 0x0100) d3h = 2;
        else if (!d3h) continue;
        uint16_t a = (uint16_t)(g.Hscroll + d3h);
        if (a < d2h || a >= d0h) d3h = 0;
        else {
            g.Hscroll = (int16_t)a;
            if (!(a & 7)) {
                if (!(d3h & 0x8000)) {
                    uint16_t r7 = (uint16_t)((a >> 3) + 0x2F);
                    if (r7 < 0x80) playoff_column(r7);
                } else if ((a >> 3) >= 0x10) playoff_column((uint16_t)((a >> 3) - 0x10));
            }
            if (a == 0x58 || a == 0xB0 || a == 0x108 || a == 0x180 || a == 0x1F8 || a == 0x250 || a == 0x2A8) d3h = 0;
        }
        uint16_t h = (uint16_t)(g.Hscroll - 0x100);
        g.BG1HScroll = h;
        w16(RAM_OFF_BG3HScroll, h);
        g.BG2HScroll = (uint16_t)(h - 0x80);
    }
done:
    g.textdirtyhi = 0;
    g.textdirtylo = 0xFFFF;
}

/* The scouting report's text, a word at a time (UpdateScoutingDisplay) */

/* (UpdateScoutingDisplay_skip: script a2 past its next carriage return) */
static uint32_t scout_skip_line(uint32_t a2)
{
    uint8_t c;
    do {
        c = m8(a2);
        a2 = (a2 & 0xFF0000) | (uint16_t)(a2 + 1);
    } while (c != 0x0D);
    return a2;
}

/* Print the report's next word or % field from PregameText a1 + d0 at the line column (DispAttribCtr),
 * the next one $0A frames later */
void UpdateScoutingDisplay_para(uint32_t a1, uint16_t d0)
{
    uint32_t a2 = (a1 & 0xFF0000) | (uint16_t)(a1 + d0);
    g.ScoutingReportTimer = 0x0A;
    d0 = g.DispAttribCtr;
    if (m8(a2) != 0x25) { ScoutPlainWord(a1, a2, d0); return; }
    a2 = (a2 & 0xFF0000) | (uint16_t)(a2 + 1);
    uint8_t c = m8(a2);
    g.PlayerScrollCtr++;
    switch (c) {
    case 'r': ScoutHomeArena(d0); return;
    case 'v': ScoutVisCity(d0); return;
    case 'h': ScoutHomeCity(d0); return;
    case 'V': ScoutVisName(d0); return;
    case 'H': ScoutHomeName(d0); return;
    case 'm': ScoutVisNick(d0); return;
    case 'n': ScoutHomeNick(d0); return;
    case 't': ScoutHotTeam(d0); return;
    case 'e': ScoutExtremely(a1, a2, d0); return;
    case '0': ScoutHomeHot1(d0); return;
    case '1': ScoutHomeHot2(d0); return;
    case '2': ScoutHomeHot3(d0); return;
    case '3': ScoutHomeCold1(d0); return;
    case '4': ScoutHomeCold2(d0); return;
    case '5': ScoutHomeCold3(d0); return;
    case '6': ScoutVisHot1(d0); return;
    case '7': ScoutVisHot2(d0); return;
    case '8': ScoutVisHot3(d0); return;
    case '9': ScoutVisCold1(d0); return;
    case 'a': ScoutVisCold2(d0); return;
    case 'b': ScoutVisCold3(d0); return;
    default: ScoutPlainWord(a1, a2, d0); return;
    }
}

/* Skip the rest of script a2's line and d0 more, then print from there */
void UpdateScoutingDisplay_skip(uint32_t a1, uint32_t a2, uint16_t d0)
{
    UpdateScoutingDisplay_next(a1, scout_skip_line(a2), d0);
}

/* Skip d0 lines of the script a2 (from PregameText a1), then print from there */
void UpdateScoutingDisplay_next(uint32_t a1, uint32_t a2, uint16_t d0)
{
    d0 = (uint16_t)(d0 - 1);
    if (!(d0 & 0x8000)) { UpdateScoutingDisplay_skip(a1, a2, d0); return; }
    UpdateScoutingDisplay_para(a1, (uint16_t)(a2 - a1));
}

/* Start paragraph d0 of PregameText a1 on a new line (a blank line before it once text is up) */
void UpdateScoutingDisplay_scroll(uint32_t a1, uint16_t d0)
{
    if (!((uint16_t)(g.VertLineScrolling - 1) & 0x8000)) ScrollDisplayUp();
    ScrollDisplayUp();
    UpdateScoutingDisplay_next(a1, a1, d0);
}

/* Every frame of the scouting report: when its timer runs out (at once with asv negative), the next
 * word of the paragraph, or the next paragraph of the list TestList; at the list's end the timer
 * stops and the screen stays 480 frames */
void UpdateScoutingDisplay(void)
{
    if ((int16_t)g.ScoutingReportTimer > 0x1000) { UpdateScoutingDisplay_rtl(); return; }
    if (!(g.asv & 0x8000)) {
        g.ScoutingReportTimer--;
        if (!(g.ScoutingReportTimer & 0x8000)) { UpdateScoutingDisplay_rtl(); return; }
    }
    uint32_t a1 = SYM_PregameText;
    uint16_t d0 = g.PlayerScrollCtr;
    if (!(d0 & 0x8000)) { UpdateScoutingDisplay_para(a1, d0); return; }
    g.DispAttribCtr = 0;
    uint16_t y = g.SelectedPlayerIdx;
    g.SelectedPlayerIdx = (uint16_t)(y + 2);
    d0 = m16(TestList + y);
    if (!(d0 & 0x8000)) { UpdateScoutingDisplay_scroll(a1, d0); return; }
    g.ScoutingReportTimer = 0x7FFF;
    g.screentimer = 0x01E0;
    UpdateScoutingDisplay_rtl();
}

/* After a game: the password and the playoff screen; in a playoff series with a user game the next game
 * (from its scouting report unless sflags3 bit 2 was set), else back to the start (Restart). Never returns
 * on the cartridge. */
void GameOver(void)
{
    g.FrameByNumber = 0;
    EncodePW();
    PlayoffScreen();
    if (g.OptPlayMode != 1 || (g.gamenum & 0x8000)) { Restart(); return; }
    g.PassWord[0] &= 0x7F;
    g.PassWord[1] &= 0x7F;
    DecodePW();
    maketree();
    uint16_t t = g.sflags3[0] & 4;
    g.sflags3[0] &= (uint16_t)~4;
    if (t) StartGame();
    else StartGame_scout();
}

/* End of a period: fade out, the menu or playoff music, the teams turned around (players out of the game
 * back on the bench), the next period number (tied after the third: overtime, again in a playoff game),
 * the intermission; then game over (GameOver) or the next period (StartPer). Never returns on the
 * cartridge. */
void PeriodOver(void)
{
    FadeOut();
    FadeOutSound();
    WaitFrame();
    WaitFrame();
    while (g.brightness || g.SoundFadeSpeed) hal_wait_interrupt();
    forceblack();
    if (!g.shootoutmode) {
        SPCMenuUpload();
        if (g.OptPlayMode && g.OptPlayMode != 4 && g.playoffRoundIdx >= 2) MusicStart(SYM_PlayoffSong);
        else MusicStart(SYM_MenuSong);
    }
    g.gmode ^= gmdir;
    for (int k = 0; k < 52; k++)
        if (g.tmpdst[k] == 0xFFFD) g.tmpdst[k] = TMPDBENCH;
    SetHor();
    InitSprites(0x6000, 0, 0x01C0);
    FadeIn();
    ScreenOn();
    SpriteFrameStart();
    setvideo();
    SpriteFrameEnd();
    g.gsp++;
    if (g.gsp != 3) {
        if (g.gsp < 3) goto final;
        g.gsp = 3;
        if (!g.OptPlayMode) g.gsp = 4;
    }
    if (g.tmscore[0] != g.tmscore[1]) g.gsp = 4;
final:
    ResetClock();
    UpdateScores();
    Intermission();
    if (g.gsp == 4) { GameOver(); return; }
    g.lastperiod = g.gsp;
    FadeOutSound();
    FadeOut();
    while (g.SoundFadeSpeed) hal_wait_interrupt();
    SPCGameUpload();
    g.BA_Sktr_SCnum = 0xFFFF;
    StartPer();
}

/* Start over: the menu music (unless it is playing), vertical mode, the option screens, then the game
 * (StartGame) or the shootout, after which it starts over again. Never returns on the cartridge (it resets
 * the stack). */
static void restart_go(void)
{
    SPCMenuUpload();
    if (!g.MusicOn || g.SongPtr != 0xC20C) {
        if (g.MusicOn) {
            FadeOutSound();
            while (g.MusicOn) hal_wait_interrupt();
        }
        MusicStart(SYM_MenuSong);
    }
    g.sfhormode = 0;
    w16(RAM_OFF_jmlvector, 0x005C);
    CheckPAL();
    setoptions();
    g.shootoutmode = 0;
    if (g.OptPlayMode != 4) { StartGame(); return; }
    PlayShootout();
    g.FrameByNumber = 0;
    g.playoffRoundIdx = 0;   /* (back to Restart's start: jmp, not an entry) */
    hal_stack_reset(restart_go);
}
/* Back to the option screens from anywhere (the end of a game or the demo, exit game): out of the playoffs,
 * then restart_go on a fresh stack */
void Restart(void)
{
    g.playoffRoundIdx = 0;
    hal_stack_reset(restart_go);
}


/* The cold start after Reset_Fast: the intro, bank $7F cleared, the password and default options, the
 * title screen, then the option screens (Restart, which the original falls into) */
void Opening(void)
{
    Multi5Init();
    IntroSequence();
    forceblack();
    memset(WRAM_BYTES + RAM_OFF_Ram7F, 0, 0x10000);
    for (int k = 0; k < 0x8000; k++) hal_cost_mark(SYM_Opening + 0x12);   /* (the clear's 32768 passes, sta.l Ram7F,x) */
    w16(RAM_OFF_jmlvector, 0x005C);
    CheckPAL();
    ResetPassWord();
    DefaultMenus();
    ReadAnyPad();
    g.brightness = 0;
    ReadAnyPad();
    g.sndbank[0] = 0;
    TitleScreen();
    FadeOutSound();
    while (g.brightness || g.MusicOn) hal_wait_interrupt();
    Restart();
}
