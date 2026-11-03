/* optsetup: the game setup screen (src/routines/optsetup). */
#include "game.h"

#define NEG(a, b) (((uint16_t)((a) - (b))) & 0x8000)   /* cmp, then bmi */
#define WRAM24(off) (0x7E0000u + (off))

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
        PrintStringFromList(x >> 1, a1);
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
    FadeOut();
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
    InitSortObjects();
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
            ReadJoy1(&d0, &d1, &d2, &d3);
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
    maketree();
    PlayerSelect(a2, d2);
}
