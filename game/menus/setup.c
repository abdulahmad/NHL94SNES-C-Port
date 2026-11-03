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
 * (from its scouting report unless sflags3 bit 2 was set), else back to the start (Opening2). Never returns
 * on the cartridge. */
void GameOver(void)
{
    g.FrameByNumber = 0;
    EncodePW();
    PlayoffScreen();
    if (g.OptPlayMode != 1 || (g.gamenum & 0x8000)) { Opening2(); return; }
    g.PassWord[0] &= 0x7F;
    g.PassWord[1] &= 0x7F;
    DecodePW();
    MakeTree();
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
    g.playoffRoundIdx = 0;   /* (back to Opening2's start: jmp, not an entry) */
    hal_stack_reset(restart_go);
}
/* Back to the option screens from anywhere (the end of a game or the demo, exit game): out of the playoffs,
 * then restart_go on a fresh stack */
void Opening2(void)
{
    g.playoffRoundIdx = 0;
    hal_stack_reset(restart_go);
}


/* The cold start after Reset_Fast: the intro, bank $7F cleared, the password and default options, the
 * title screen, then the option screens (Opening2, which the original falls into) */
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
    Opening2();
}

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
void MakeTree(void)
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
    MakeTree();
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
    MakeTree();
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
    MakeTree();
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
    lcfound_close(0);
    lcfound_close(2);
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
    if (r8(RAM_OFF_tmgoals + y) == 3 && !((uint16_t)(m8(a4 + 3) - GetGoalieCount(a2)) & 0x8000)) {
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
