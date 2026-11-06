/* cards: player cards and ratings (src/routines/cards). */
#include "game.h"

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

/* The rating (0-100) of player d0 of team a2 (0 home, 2 visitors) from the attributes of mask d4+2
 * (bit 15: the weighted overall rating, r2 = 0 skater / 16 goalie weights; otherwise the one
 * attribute), with the hot / cold table when usehotcold is set. Attribute bits 6 and 13 counted
 * (*count, d0+2) keep the rating as it is; otherwise it is clamped to 100 and under 50 compressed
 * (d0 / 2 + 25). */
uint16_t CalcAttrib(uint16_t d0, uint16_t a2, uint16_t mask, uint16_t r2, uint16_t *count)
{
    uint16_t r0 = (uint16_t)(((uint16_t)(d0 << 1) + d0 + (d0 >> 15)) << 2);   /* (asl's carry into the adc) */
    uint16_t cnt = 0;
    if (a2) r0 = (uint16_t)(r0 + 0x138);
    uint32_t bank = SYM_TeamList & 0xFF0000;
    uint16_t a0 = r16(RAM_OFF_tmdata + a2);
    a0 = (uint16_t)(a0 + m16(bank | a0));
    for (uint16_t n = (uint16_t)(d0 - 1); !(n & 0x8000); n--) {
        uint32_t s = (uint32_t)a0 + m16(bank | a0);
        a0 = (uint16_t)((s & 0xFFFF) + 8 + (s >> 16));
    }
    a0 = (uint16_t)(m16(bank | a0) + a0);
    uint16_t sum = 0, hc = 0, rating;
    for (uint16_t k = 0; k < 15; k++) {
        if (!(rom16(SYM_BitMaskTable + 2 * k) & mask)) continue;
        if (k == 6 || k == 13) cnt++;
        uint16_t p1 = (uint16_t)(15 - k);
        uint16_t v = m8(bank | (uint16_t)((p1 >> 1) + a0));
        if (k & 1) v >>= 4;
        v &= 0xF;
        uint16_t col = (uint16_t)(p1 - 3);
        if (col >= 6) col--;
        uint16_t b = g.usehotcold ? r8((uint16_t)(RAM_OFF_HotColdTable + col + r0)) : 0;   /* (r3+2; the data bank's low RAM) */
        uint16_t m = b >= 0x80 ? (uint16_t)(0x100 - b) : b;
        uint16_t h = (uint16_t)((uint16_t)((uint16_t)(m << 1) + m) >> 2);
        if (h & 0x2000) h |= 0xC000;
        if (b & 0x80) h = (uint16_t)-h;
        if (!(mask & 0x8000)) {
            if (rom8(SYM_CalcAttrib_19 + p1)) {
                uint16_t t = (uint16_t)(Mul8U(0x12, v) + h);
                if (t & 0x8000) t = 0;
                if (t >= 100) t = 100;
                rating = t;
            } else rating = v;
            goto done;
        }
        sum = (uint16_t)(Mul8U(rom16(SYM_CalcAttrib_18 + (uint16_t)(p1 + r2)), v) + sum);
        hc = (uint16_t)((r2 ? (uint16_t)(h << 1) : h) + hc);
    }
    {
        uint16_t div = r2 ? 7 : 10;
        if (hc & 0x8000) sum = (uint16_t)(sum - DivX8U((uint16_t)-hc, div));
        else sum = (uint16_t)(DivX8U(hc, div) + sum);
        if (sum & 0x8000) sum = 0;
        rating = sum;
        if (r2) rating >>= 1;
    }
done:
    *count = cnt;
    if (cnt) return rating;
    if (rating >= 100) rating = 100;
    if (rating < 50) rating = (uint16_t)((rating >> 1) + 25);
    return rating;
}

/* Print string a1 centred on column 16; returns a1 past it */
uint32_t PrintCentered(uint32_t a1)
{
    g.printx = (uint16_t)(0x10 - ((uint16_t)(m16(a1) - 2) >> 1));
    return print(a1);
}


/* (DrawPlayerPicture's .8: 1 when curplayer of team a2 is a goalie (below the goalie count)) */
static uint16_t pic_goalie(uint16_t a2) { return (uint16_t)(g.curplayer[0] - GetGoalieCount(a2)) & 0x8000 ? 1 : 0; }

/* (.9: 0 when curplayer is one of the six players of team a2's first line (team data +12), else 1) */
static uint16_t pic_not_starter(uint16_t a2)
{
    uint32_t a3 = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
    a3 = (a3 & 0xFF0000) | (uint16_t)((uint16_t)a3 + m16(a3 + 0xC));
    for (int y = 5; y >= 0; y--)
        if ((uint16_t)(m8(a3 + y) - 1) == g.curplayer[0]) return 0;
    return 1;
}

/* (.6: whether the picture is mirrored: a first line player never; else bit 0 of his overall
 * rating (column 4 for a goalie, 5 for a skater; inverted for a goalie)) */
static uint16_t pic_mirrored(uint16_t a2)
{
    if (!pic_not_starter(a2)) return 0;
    uint16_t r3 = GetPlayerRating(pic_goalie(a2) ? 4 : 5, a2) & 1;
    return pic_goalie(a2) ? r3 ^ 1 : r3;
}

/* (.5: paste map [p0] into the map at [p1] (r7 entries a row) at column x, row y, mirrored (each row
 * from the right, the flip bit set), adding a to each entry) */
static void paste_mirrored(uint32_t p0, uint32_t p1, uint16_t a, uint16_t x, uint16_t y, uint16_t r7)
{
    uint32_t bank = p1 & 0xFF0000;
    uint32_t s = (uint32_t)(uint16_t)(x << 1) + (uint16_t)Mul16U((uint16_t)(y << 1), r7);
    p1 = bank | (uint16_t)((s & 0xFFFF) + (uint16_t)p1 + (s >> 16));
    uint16_t w = m16(p0), h = m16(p0 + 2);
    p0 = (p0 & 0xFF0000) | (uint16_t)((uint16_t)p0 + 6);
    do {
        uint16_t k = 0;
        for (uint16_t n = w; n; n--, k += 2) {
            uint16_t t = (uint16_t)((uint16_t)((uint16_t)-(k >> 1) + w) - 1);
            uint16_t r3 = (uint16_t)((uint16_t)(t << 1) + (uint16_t)p1 + (t >> 15));   /* (asl's carry) */
            mset16(bank | r3, (uint16_t)((uint16_t)(m16(p0 + k) + a) | 0x4000));
        }
        p1 = bank | (uint16_t)((uint16_t)(r7 << 1) + (uint16_t)p1 + (r7 >> 15));
        p0 = (p0 & 0xFF0000) | (uint16_t)((uint16_t)(w << 1) + (uint16_t)p0 + (w >> 15));
    } while (--h);
}

/* (DrawPlayerPicture: the photo map in the text map at x, y (tiles from picchars, the visitors' $30
 * on), then the team's photo palette (colours $50 home, $10 visitors)) */
static void pic_map_and_colours(uint16_t a2, uint16_t x, uint16_t y, int mirrored)
{
    uint32_t p0 = SYM_PhotoMap, p1 = 0x7E0000u + RAM_OFF_textmap;
    uint16_t a = (uint16_t)(g.picchars + g.picattr);
    if (a2) a = (uint16_t)(a + 0x30);
    if (mirrored) paste_mirrored(p0, p1, a, x, y, 0x20);
    else PasteMap(&p0, &p1, a, x, y, 0x20);
    uint32_t a1 = (SYM_PhotoPaletteTable & 0xFF0000) | (uint16_t)((uint16_t)(r16(RAM_OFF_HomeTeam + a2) << 2) + (uint16_t)SYM_PhotoPaletteTable);
    uint32_t pal = m16(a1) | (uint32_t)m16((a1 & 0xFF0000) | (uint16_t)(a1 + 2)) << 16;
    SetColours(pal, 0x10, a2 ? 0x10 : 0x50);
}

/* Draw the picture of player d0 of team a2 at column x, row y: his photo (PlayerPhotoTable, 26 a
 * team) unpacked and made planar in the team's picture buffer ($7F6F00 home, $7F6900 visitors),
 * mirrored for some players; with matchupphotos the matchup photos (LoadMatchupPhotos) instead */
void DrawPlayerPicture(uint16_t d0, uint16_t a2, uint16_t x, uint16_t y)
{
    if (g.matchupphotos) {
        LoadMatchupPhotos(a2);
        uint16_t k = (uint16_t)(d0 << 1);
        if (a2) {
            g.picattr = 0x2400;
            g.vispicaddr = (uint16_t)(0x2D00 + rom16(SYM_MatchupPhotoOffsets + k));
        } else {
            g.picattr = 0x3400;
            g.homepicaddr = (uint16_t)(0x5100 + rom16(SYM_MatchupPhotoOffsets + k));
        }
        pic_map_and_colours(a2, x, y, 0);
        return;
    }
    g.picattr = a2 ? 0x2400 : 0x3400;
    uint16_t k = (uint16_t)((uint16_t)(Mul8U(r16(RAM_OFF_HomeTeam + a2), 0x1A) + d0) << 2);
    uint32_t p0 = rom16(SYM_PlayerPhotoTable + k) | (uint32_t)rom16(SYM_PlayerPhotoTable + 2 + k) << 16;
    uint16_t r0 = Decompress(p0, unpackbuf);
    uint16_t p1 = a2 ? 0x6900 : 0x6F00;
    if (a2) g.vispicaddr = p1;
    else g.homepicaddr = p1;
    LinearToPlanar(unpackbuf, p1, r0);
    pic_map_and_colours(a2, x, y, pic_mirrored(a2) != 0);
}


/* (print string a1 centred on column 16 at row y; a1 past it) */
static uint32_t card_print(uint32_t a1, uint16_t y)
{
    g.printx = (uint16_t)(0x10 - ((uint16_t)(m16(a1) - 2) >> 1));
    g.printy = y;
    return print(a1);
}

/* (DrawPlayerCard: the strengths label, then the names (from list a1) of up to three strengths, the
 * second and third only when rated 80 or more) */
static void card_strengths(uint32_t label, uint32_t names)
{
    PrintSmallLetterSprites(0x38, 0x7A, label);
    if (g.strengths[0] & 0x8000) return;
    PrintSmallLetterSprites(0x40, 0x84, AdvanceStringPtr(names, g.strengths[0]));
    if ((g.strengths[2] & 0x8000) || ((uint16_t)(g.strengths[3] - 0x50) & 0x8000)) return;
    PrintSmallLetterSprites(0x40, 0x8E, AdvanceStringPtr(names, g.strengths[2]));
    if ((g.strengths[4] & 0x8000) || ((uint16_t)(g.strengths[5] - 0x50) & 0x8000)) return;
    PrintSmallLetterSprites(0x40, 0x98, AdvanceStringPtr(names, g.strengths[4]));
}

/* Draw the current card of team a2: curitem 0 the team card (the team name, the six line slots and the
 * team rating), 1-6 a line slot's player, 7 and up roster player curitem - 7 (the number and name, the
 * position group, the strengths and the overall rating); then the picture and the card colours */
void DrawPlayerCard(uint16_t a2)
{
    uint16_t q;
    SpriteFrameStart();
    print(SYM_DrawPlayerCardPos);
    eraser(0x20, 0x14, g.blankchar);
    DrawLogoSprite(r16(RAM_OFF_HomeTeam + a2), 0xC8, 0x10);
    uint32_t a3 = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
    a3 = (a3 & 0xFF0000) | (uint16_t)((uint16_t)a3 + m16(a3 + 0xC));
    if (g.curitem[0]) {
        uint16_t t = (uint16_t)(g.curitem[0] - 7);
        if (t & 0x8000) {
            uint16_t slot = rom8(SYM_DrawPlayerCard_8 + (uint16_t)(t + 6));
            t = (uint16_t)(m8(a3 + slot) - 1);
        }
        g.curplayer[0] = t;
        card_print(getname(a2, g.curplayer[0]), 0xC);
        if ((uint16_t)(g.curplayer[0] - GetGoalieCount(a2)) & 0x8000) {
            card_print(SYM_RosterGroups + 18, 0xD);
            FindGoalieStrengths(a2);
            card_strengths(SYM_StrengthsLabel, SYM_RatingNames + 178);
        } else {
            uint16_t d1 = GetGoalieCount(a2);
            uint16_t n = (uint16_t)(GetForwardCount(a2) + d1);
            uint16_t c = (uint16_t)(n - g.curplayer[0]);
            card_print(!c || (c & 0x8000) ? SYM_RosterGroups + 9 : SYM_RosterGroups, 0xD);
            FindSkaterStrengths(a2);
            card_strengths(SYM_StrengthsLabelB, SYM_RatingNames);
        }
        PrintSmallLetterSprites(0x98, 0x7A, SYM_OverallRating);
        PrintSmallLetterSprites(0xA0, 0x84, PushNumberWrap(GetPlayerRating(1, a2), 4, &q));
    } else {
        ClearPictureBox(0xD, 4);
        g.cardblock = 1;
        print(SYM_DrawPlayerCardPos2);
        DrawTeamBlocks(r16(RAM_OFF_HomeTeam + a2), 0, a2);
        g.cardblock = 0;
        uint32_t a1 = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
        a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)a1 + m16(a1 + 4));
        a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)a1 + m16(a1));
        a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)a1 + m16(a1));
        a1 = card_print(a1, 7);
        card_print(a1, 9);
        g.printy = 0x60;
        PrintLineSlotName(0x80, 4, a3, a2);
        g.printy = (uint16_t)(g.printy + 8);
        PrintLineSlotName(0x40, 3, a3, a2);
        PrintLineSlotName(0xC0, 5, a3, a2);
        g.printy = (uint16_t)(g.printy + 0xE);
        PrintLineSlotName(0x58, 1, a3, a2);
        PrintLineSlotName(0xA8, 2, a3, a2);
        g.printy = (uint16_t)(g.printy + 0xE);
        PrintLineSlotName(0x80, 0, a3, a2);
        PrintSmallLetterSprites(0x66, 0x98, SYM_TeamRating);
        PrintSmallLetterSprites(0x70, 0xA2, PushNumberWrap(GetTeamRating(a2), 4, &q));
    }
    if (g.curitem[0]) {
        DrawPlayerPicture(g.curplayer[0], a2, 0xD, 4);
        uint32_t p0 = 0x7F0000u | g.homepicaddr;
        uint16_t y = (uint16_t)((uint16_t)(g.picchars << 4) + 0x2000);
        WaitFrame();
        DMAToVRAM(&p0, 0x0600, y, 0);
        p0 = 0x7F0000u | g.vispicaddr;
        DMAToVRAM(&p0, 0x0600, (uint16_t)((uint16_t)(g.picchars << 4) + 0x2300), 0);
        SendTextMap();
        g.MapVRAM = 0x0400;
        MapToVRAM(0x7E0000 + RAM_OFF_picboxmap, 0xC, 4);
    } else SendTextMap2();
    SetCardTeamColours(a2);
    WaitFrame();
    SpriteFrameEnd();
    g.colourbuf[0] = 0x3568;
    SendColours();
}

/* Copy the card team's two colours (a2 nonzero: colours $67-$68, else $77-$78) to colours $4E-$4F */
void SetCardTeamColours(uint16_t a2)
{
    uint16_t x = a2 ? 0xCE : 0xEE;
    w16(RAM_OFF_colourbuf + 0x9C, r16(RAM_OFF_colourbuf + x));
    w16(RAM_OFF_colourbuf + 0x9E, r16(RAM_OFF_colourbuf + x + 2));
}

/* Draw the logo of team a as sprites at x, y with its LogoPaletteTable palette */
void DrawLogoSprite(uint16_t a, uint16_t x, uint16_t y)
{
    uint32_t e = SYM_LogoPaletteTable + (uint16_t)(a << 2);
    SetColours((uint32_t)rom16(e) | (uint32_t)rom16(e + 2) << 16, 0x10, 0xA0);
    DrawSprite(0x2400, x, y, a, logosprbuf);
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

/* The short name of team a2's player in line slot y (a3) in small letter sprites centred on x, at printy */
void PrintLineSlotName(uint16_t x, uint16_t y, uint32_t a3, uint16_t a2)
{
    uint32_t a1 = FormatPlayerNumInitial(a2, (uint16_t)(m8(a3 + y) - 1));
    uint16_t w = (uint16_t)((uint16_t)((uint16_t)(m16(a1) - 2) >> 1) << 2);
    PrintSmallLetterSprites((uint16_t)(x - w), g.printy, a1);
}

/* The player card screen's graphics: the team logo sprites, the screen registers, the fonts (remapped), the
 * team block, card back and frame tiles to VRAM, the card picture map, the card back map into text map 2,
 * the palettes, the "switch cards" text, the sprites and the scroll */
void LoadPlayerCardScreen(void)
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
    g.printm = 0;
    g.charofs2 = 0;
    ClearTextMap();
    ClearTextMap2();
    io_w16(0x2116, (uint16_t)((uint16_t)(g.smallfontchars << 4) + 0x1000 + (g.smallfontchars >> 12 & 1)));   /* (asl's carry) */
    uint16_t n = Decompress(SYM_SmallFontTiles, unpackbuf);
    remap(unpackbuf, SYM_PixelRemap8F, (uint16_t)(n >> 5));
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
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
        Pad p = getpzjoy();
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
