/* title (src/routines/title). */
#include "game.h"

/* Draw string a1 (from its first byte) as big letter sprites centred on x, at y */
void PrintStringSprites(uint16_t x, uint16_t y, uint32_t a1)
{
    g.letterx[0] = x;
    g.lettery[0] = y;
    PrintTeamSprites_go(a1, 0, 1, 0xFFFF);
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
    TitleScreen_ret();
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

/* Draw sprite a of the title set at its position from the _1 / _2 tables */
void DrawTitleSprite(uint16_t a)
{
    uint16_t x = rom16(SYM_DrawTitleSprite_1 + (uint16_t)(a << 1));
    uint16_t y = rom16(SYM_DrawTitleSprite_2 + (uint16_t)(a << 1));
    DrawSprite(0x3000, x, y, a, titlesprbuf);
}

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

/* A busy loop of x passes, none once a pad press has ended the title (titleskip) */
void TitleDelay(uint16_t x)
{
    if (g.titleskip) return;
    do hal_cost_mark(SYM_TitleDelay_loop); while (--x);
}

/* Title: an empty sprite frame, then a 35-frame wait */
void TitleBlankSprites(void)
{
    SpriteFrameStart();
    SpriteFrameEnd();
    TitleWait(0x23);
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

/* The x offset of title path step a: sin(a) times a growing amplitude (a * 10 / $50 + 1), then a straight
 * run */
uint16_t TitlePathX(uint16_t a)
{
    if (a >= 0x480) return (uint16_t)(Div16U((uint16_t)Mul16x8U((uint16_t)(a - 0x480), 0xC6), 0x80) + 0x66);
    uint16_t ang = (uint16_t)(Div16U((uint16_t)Mul16x8U(a, 0x0A), 0x50) + 1);
    return (uint16_t)(Mul16x8XS(Sin(a), ang) >> 8);
}

/* The y scroll of title path step a: $1B0 - (a * $50 / 256) / 5 */
uint16_t TitlePathY(uint16_t a)
{
    uint16_t r2 = (uint16_t)(Mul16x8XS(a, 0x50) >> 8);
    return (uint16_t)(0x01B0 - Div16S(r2, 5));
}

/* The two copyright lines */
void PrintCopyright(void)
{
    PrintStringSprites(0x80, 0xB8, SYM_CopyrightLines);
    PrintStringSprites(0x80, 0xC8, (SYM_CopyrightLines & 0xFF0000) | (uint16_t)(SYM_CopyrightLines + 23));
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

/* The title logo path, $500 steps: each step's mode 7 scale, x offset and y scroll */
void BuildTitlePath(void)
{
    for (uint16_t y = 0, x = 0; y < 0x500; y++, x += 2) {
        w16(RAM_OFF_titlescale + x, TitlePathScale(y));
        w16(RAM_OFF_titlexofs + x, TitlePathX(y));
        w16(RAM_OFF_titleyscroll + x, TitlePathY(y));
    }
}

void TitleScreen_ret(void) {}

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

/* Wait x frames, unless a pad press has ended the title */
void TitleWait(uint16_t x)
{
    if (g.titleskip) return;
    WaitFrames_Inc(x, 0);
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

/* The next team of the shuffled title list, shuffling again after the 26th */
uint16_t NextTitleTeam(void)
{
    uint16_t i = (uint16_t)(g.TitleTeamIdx[0] + 1);
    if (i >= 0x1A) { ShuffleTitleTeams(); i = 0; }
    g.TitleTeamIdx[0] = i;
    return r16(RAM_OFF_titleteams + (uint16_t)(i << 1));
}
