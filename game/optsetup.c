/* optsetup: the game setup screen (src/routines/optsetup). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define WRAM24(off) (0x7E0000u + (off))
#define SPFpad 389         /* (frames94s.asm) */
#define SPFsmallfont 433

/* (a row's entry in .13, by play mode: 1 shown, $FFFF the playoff name, 0 not shown; asl's carry
 * added as adc does) */
static uint16_t opt_row(uint16_t row)
{
    uint16_t m = g.OptPlayMode;
    return rom16(SYM_setoptions_13 + (uint16_t)((uint16_t)(m << 4) + row + (m >> 12 & 1)));
}

/* Setup: change the option of row d7 by d2 (a team row, or the mode, also restarts the matchup
 * pictures), within .14's choices (team rows: $1A in the playoff modes), wrapping round; a2 left at
 * OptPlayMode */
void setoptions_7(uint16_t d2, uint16_t *a2)
{
    uint16_t d7 = g.d7;
    if ((d7 == 0 || d7 == 2 || d7 == 4) && d2) {
        g.setupmatchup = 5;
        g.teamschanged = 1;
        g.cardtimer = 1;
    }
    *a2 = RAM_OFF_OptPlayMode;
    uint16_t d5 = 0, d4 = rom16(SYM_setoptions_14 + d7), d3 = r16(RAM_OFF_OptPlayMode + d7);
    if (g.OptPlayMode >= 2 && g.OptPlayMode != 4 && d7 == 2) d4 = 0x1A;
    d3 = setoptions_9(d3, d2, d4, d5);
    w16(RAM_OFF_OptPlayMode + d7, d3);
}

/* Setup: d3 + d2, below d5 (signed) to d4 - 1, from d4 on to d5 */
uint16_t setoptions_9(uint16_t d3, uint16_t d2, uint16_t d4, uint16_t d5)
{
    d3 = (uint16_t)(d3 + d2);
    if ((int16_t)d3 < (int16_t)d5) d3 = (uint16_t)(d4 - 1);
    if ((int16_t)d3 >= (int16_t)d4) d3 = d5;
    return d3;
}

/* Setup: the option rows: the teams (setteams) when they changed, then each row the play mode shows,
 * its name and choice (the row at d7 highlighted, the team rows by name) */
void setoptions_10(uint16_t *a2)
{
    if (g.teamschanged) {
        g.HomeTeam = g.OptTeam1;
        g.VisTeam[0] = g.OptTeam2;
        setteams();
        g.teamschanged = 0;
    }
    g.printy = 0x0E;
    g.printx = 0;
    eraser(0x20, 0x0E, g.blankchar);
    uint16_t d6 = g.d7;
    g.printy = 0x0E;
    for (uint16_t x = 0; x < 0x0E; x += 2) {
        g.printa = 0x3800;
        uint32_t a1 = SYM_GameSetupNames;
        uint16_t t = opt_row(x);
        if (!t) continue;
        if (t & 0x8000) a1 = SYM_PlayoffSetupNames;
        g.printx = 3;
        Fprint(x >> 1, a1);
        g.printx = 0x11;
        g.d7 = x;
        setoptions_7(0, a2);
        uint16_t d3 = r16(RAM_OFF_OptPlayMode + x);
        uint32_t bank = SYM_setoptions_15 & 0xFF0000;
        uint16_t a0 = (uint16_t)(SYM_setoptions_15 + rom16(SYM_setoptions_15 + x));
        do {
            a1 = bank | a0;
            a0 = (uint16_t)(a0 + rom16(bank | a0));
        } while (!(--d3 & 0x8000));
        if (x == d6) g.printa = 0x2000;
        g.d7 = x;
        if (x == 2 || x == 4) print(GetTeamLongName(r16(RAM_OFF_OptPlayMode + x)));
        else print(a1);
        g.printy += 2;
    }
    g.d7 = d6;
}

/* Setup: team x's matchup player's short name; the home one's (x = 0) copied to $0A5C */
void setoptions_t1xxxxxxxxxxx(uint16_t x, uint16_t *a2)
{
    *a2 = x;
    uint32_t a1 = FormatPlayerNumInitial(x, GetMatchupPlayer(g.setupmatchup, x));
    if (!x) MemCopy(a1, 0x000A5C, 0x31);
}

/* Setup: the next matchup (0-5, then 6 the logos): the sides showing a team turn to the matchup
 * player's picture and name */
void setoptions_t2xxx(uint16_t *a2)
{
    g.cardtimer = 0x78;
    uint16_t a = (uint16_t)(g.setupmatchup + 1);
    if (a >= 7) a = 0;
    g.setupmatchup = a;
    if (a == 6) return;
    if (NEG(g.setuphome, 0x1C)) {
        g.setuphome = 0xFFFF;
        setoptions_t1xxxxxxxxxxx(0, a2);
    }
    if (NEG(g.setupvis, 0x1C)) {
        g.setupvis = 0xFFFF;
        setoptions_t1xxxxxxxxxxx(2, a2);
    }
    g.matchuphome = g.setupmatchup;
    *a2 = 2;
    g.matchupvis = g.setupmatchup;
}

/* Setup, each pass: the team pictures (a blank box when the mode has no such team row); every $78
 * passes the next matchup, its title, team blocks or player names and pictures */
void setoptions_11(uint16_t *a2)
{
    uint16_t x = (uint16_t)(4);
    if (!opt_row(x)) {
        g.setupvis = 0x1C;
        g.printx = 1;
        g.printy = 0x0A;
        eraser(0x0A, 2, g.blankchar);
        g.redrawblocks = 1;
    } else g.setupvis = g.VisTeam[0];
    x -= 2;
    if (!opt_row(x)) {
        g.setuphome = 0x1D;
        g.printx = 0x16;
        g.printy = 0x0A;
        eraser(0x0A, 2, g.blankchar);
        g.redrawblocks = 1;
    } else g.setuphome = g.HomeTeam;
    g.cardtimer -= 2;
    if (!(g.cardtimer & 0x8000)) return;
    SpriteFrameStart();
    setoptions_t2xxx(a2);
    PrintLetterSprites(SYM_GameSetupAlt, 0x4F, 0x18);
    if (NEG(g.setupvis, 0x1C)) {
        g.printx = 1;
        g.printy = 0x0A;
        DrawTeamBlocks(g.VisTeam[0], SYM_GameSetupAlt, *a2);
    }
    if (NEG(g.setuphome, 0x1C)) {
        g.printx = 0x16;
        g.printy = 0x0A;
        DrawTeamBlocks(g.HomeTeam, SYM_GameSetupAlt, *a2);
    }
    if (g.setuphome & 0x8000) {
        *a2 = 0;
        PrintTeamSprites(8, 0x43, 0xFFFF, 0x000A5C, 0);
    }
    if (g.setupvis & 0x8000) {
        *a2 = 2;
        PrintTeamSprites(8, 0x43, 0xFFFF, RAM_OFF_mesarea, 2);
    }
    DrawMatchupPictures(*a2);
    SpriteFrameEnd();
    SendColours();
}

/* Setup: the cursor row d7 moved by d0 to the next row the play mode shows (kept at the ends) */
void setoptions_12(uint16_t d0)
{
    uint16_t d3 = g.d7;
    for (;;) {
        g.d7 = (uint16_t)(g.d7 + d0);
        if (g.d7 & 0x8000) g.d7 = 0;
        if (g.d7 >= 0x0E) {
            g.d7 = d3;
            return;
        }
        if (opt_row(g.d7)) return;
    }
}

/* The game setup screen: up and down pick a row, left and right change it, Start goes on (the
 * password screen in play mode 1, new playoffs in 2 and 3), then the player select screen; left alone
 * long enough it starts the demo with random teams. (a2 and the multitap state d2+2 as the caller left
 * them in the direct page.) */
void setoptions(void)
{
    uint16_t a2 = g.a2[0];
    uint16_t d0, d1, d2 = 0, d3, d6;
    forcefade();
    g.matchupphotos = 1;
    g.playerstatson = 0;
    g.phototeams[0] = 0xFFFF;
    g.phototeams[1] = 0xFFFF;
    g.redrawblocks = 1;
    g.cardtimer = 1;
    g.teamschanged = 1;
    g.setupmatchup = 5;
    forceblack();
    ClearTextMap();
    InitSetupChars();
    g.HomeTeam = g.OptTeam1;
    g.VisTeam[0] = g.OptTeam2;
    setteams();
    g.setupvis = (uint16_t)(g.VisTeam[0] + 0x21);
    g.setuphome = g.HomeTeam;
    SetupScreen();
    io_w8(0x2105, 0x01);
    PrintLetterSprites(SYM_GameSetupTitle, 0x4F, 0x18);
    w16(RAM_OFF_colourbuf + 0x0A, 0x1D47);
    w16(RAM_OFF_colourbuf + 0x0E, 0xFFFF);
    w16(RAM_OFF_colourbuf + 0x182, 0x14A5);
    w16(RAM_OFF_colourbuf + 0xC2, 0x6F7B);
    SendColours();
    SpriteFrameEnd();
    SendTextMap();
    ReadPads();
    ReadAnyPad();
    g.sprpriority = 0;
    print(SYM_setoptionsPos);
    defaultsprites_0();
    g.OldXpos[0] = 0x28;                                   /* (rosxoff) */
    g.OldXpos[6] = 0x28;
    g.notdemo = 0xFFFF;
    LoadTeamLines(0);
    LoadTeamLines(2);
    g.d7 = 0;
    setoptions_12(0);
    setoptions_10(&a2);
    setoptions_11(&a2);
    ReadPads();
    MenuScreenHDMA();
    ScreenOn();
    SetupBandIRQ(8, 0x40);
    SendTextMap();
    LoadMatchupPhotos(0);
    LoadMatchupPhotos(2);
    for (;;) {
        d6 = 0x12C0;
        for (;;) {
            do WaitFrame(); while (r16(RAM_OFF_OAMReady));
            if (!++g.seed[0]) g.seed[1]++;
            WaitFrame();
            uint16_t w = g.mp5warning;
            if (w != g.d2[1]) {
                g.d2[1] = w;                               /* (kept in the direct page: read there each pass) */
                if (!w) {
                    Multi5Init();
                    ClearTextMapBottom();
                    setoptions_10(&a2);
                    d1 = 0;
                    goto pressed;
                }
                if (w & 1) { ClearMessageArea(); Multi5SocketMsg(); }
                else if (w & 2) { ClearMessageArea(); Multi5ScopeMsg(); }
                else if (w & 4) { ClearMessageArea(); Multi5MouseMsg(); }
            }
            if (g.mp5warning) continue;
            ReadPads();
            g.lastframevc = g.vcount[0];
            g.oldvcount = g.vcount[0];
            Readjoy1(&d0, &d1, &d2, &d3);
            g.repeatdelay--;
            d1 = ProcessInputWithRepeat(d1, d2, d3) & 0xFFF0;
            if (d1) goto pressed;
            setoptions_11(&a2);
            QueueTextDMA();
            d6 -= 2;
            if (d6 & 0x8000) goto timeout;
        }
    pressed:
        if (d1 & 0x1000) goto start;
        if (d1 & 0x0400) {
            setoptions_12(2);
            setoptions_10(&a2);
            continue;
        }
        if (d1 & 0x0800) {
            setoptions_12(0xFFFE);
            setoptions_10(&a2);
            continue;
        }
        d2 = 1;
        if (!(d1 & 0x0100)) {
            if (!(d1 & 0x0200)) continue;
            d2 = 0xFFFF;
        }
        setoptions_7(d2, &a2);
        setoptions_10(&a2);
    }
timeout:
    g.OptTeam1 = randoma8(0x1C);
    g.HomeTeam = g.OptTeam1;
    uint16_t t;
    do t = randoma8(0x1C); while (t == g.OptTeam1);
    g.OptTeam2 = t;
    g.VisTeam[0] = t;
    g.notdemo = 0;
    g.OptPlayMode = 0;
    g.OptPlayers = 0;
start:;
    uint16_t n = Decompress(SYM_BigLetterTiles, WRAM24(RAM_OFF_bigtextbuf));
    LinearToPlanar(WRAM24(RAM_OFF_bigtextbuf), (uint16_t)RAM_OFF_hlsavebuf, n);
    g.matchupphotos = 0;
    if (g.OptPlayMode == 2 || g.OptPlayMode == 3) {
        NewPO();
        d2 = g.OptTeam1 < 0x1A ? g.OptTeam1 : 0x19;       /* (NewPO leaves team 1 in d2) */
    }
    g.OptPlayers = 1;
    if (g.OptPlayMode >= 1 && g.OptPlayMode < 4) g.OptPlayers = 5;
    g.pojoy = (uint16_t)(g.OptPlayers - 5);
    if (g.OptPlayMode == 1) d2 = PasswordScreen();
    w16(RAM_OFF_PassWord, r16(RAM_OFF_PassWord) | 0x0080);
    MakeTree();
    PlayerSelect(a2, d2);
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

/* Print the Select Left hint on the password screen, d4 columns on */
void PrintSelectLeft(uint16_t d4)
{
    print(SYM_PrintSelectLeftPos);
    g.printx = (uint16_t)(g.printx + d4);
    print(SYM_SelectLeft);
}

/* Clear the message rows */
void ClearMessageArea(void) { ClearTextMapBottom(); }

/* Clear the big text letter tiles (bigtextbuf+1, $300 bytes) and part of its map (screenmap+$28C) */
void ClearBigText(void)
{
    MemFill(0x7E0000 + RAM_OFF_bigtextbuf + 1, 0x0300, 0);
    MemFill(0x7E0000 + RAM_OFF_screenmap + 0x28C, 0x0080, 0);
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

/* The player select screen after the game setup: each pad moves its icon under the visitors or home
 * team (cont1team-cont5team), Start goes on. a2 and d2 (the multitap state last seen) as setoptions
 * leaves them. */
void PlayerSelect(uint16_t a2, uint16_t d2)
{
    g.cont1team = g.cont2team = g.cont3team = g.cont4team = 0;
    g.cont5team[0] = 0;
    if (!g.notdemo) return;
    g.mp5readall = 0xFFFF;
    forcefade();
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
