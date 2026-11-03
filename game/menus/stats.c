/* stats: the roster and line screens (src/routines/stats). */
#include "game.h"

/* team a2's data (a team offset: 0 home, 2 visitors) in the cartridge */
static uint32_t team_data(uint16_t a2) { return (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2); }

/* the byte of team a2's data that counts its forwards and defensemen (team data byte 3 past the
 * offset at +8) */
static uint16_t team_counts(uint16_t a2)
{
    uint32_t a3 = team_data(a2);
    uint16_t a = (uint16_t)((uint16_t)(a3 + m16((a3 + 8) & 0xFFFFFF)) + 3);
    return m16((a3 & 0xFF0000) | a);
}

/* The defensemen on team a2 */
uint16_t GetDefenseCount(uint16_t a2) { return team_counts(a2) & 0xF; }

/* The forwards on team a2 */
uint16_t GetForwardCount(uint16_t a2) { return team_counts(a2) >> 4 & 0xF; }

/* The players on team a2's roster (records up to the terminator word 2) */
uint16_t GetPlayerCount(uint16_t a2)
{
    uint32_t a0 = team_data(a2), bank = a0 & 0xFF0000;
    a0 = bank | (uint16_t)(m16(a0) + a0);
    uint16_t d0 = 0;
    do {
        d0++;
        uint32_t s = (uint32_t)m16(a0) + (uint16_t)a0;
        a0 = bank | (uint16_t)((s & 0xFFFF) + 8 + (s >> 16));
    } while (m16(a0) != 2);
    return d0;
}

/* Roster column tail: d0 as a number of 4 digits, then the text AttribFightingPos; returns a1
 * past the number */
uint32_t AttribRating(uint16_t d0)
{
    uint16_t a;
    uint32_t a1 = print(PushNumberWrap(d0, 4, &a));
    print(SYM_AttribFightingPos);
    return a1;
}

/* Roster column: the fighting rating (the handedness bit dropped) */
uint32_t AttribFighting(uint16_t d0) { return AttribRating(d0 & 0xE); }

/* Roster column: the weight in pounds (8 * rating + $8C), then the text AttribWeightPos */
uint32_t AttribWeight(uint16_t d0)
{
    uint16_t w = (uint16_t)((uint16_t)(d0 << 3) + 0x8C + (d0 >> 13 & 1));   /* (the last asl's carry) */
    uint32_t a1 = print(PushNumber(w));
    print(SYM_AttribWeightPos);
    return a1;
}

/* Roster column: player d0's energy of team a2 as a percentage (energy / $28, $1000 with automatic
 * line changes, at most 100) */
uint32_t AttribEnergy(uint16_t d0, uint16_t a2)
{
    uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + (uint16_t)(d0 << 1) + (d0 >> 15));   /* (asl's carry) */
    uint16_t e = r16(RAM_OFF_tmpde + y);
    if (g.OptLine == 2) e = 0x1000;
    uint16_t q = Div16x8U(e, 0x28);
    if (q >= 0x65) q = 0x64;
    return AttribRating(q);
}

/* Roster column: left or right handed (string 1 - bit 0 of d0 of HandedTextTbl); returns a1 past it */
uint32_t AttribHanded(uint16_t d0) { return Fprint((d0 & 1) ^ 1, SYM_HandedTextTbl); }

/* Roster column: player d0 of team a2's status from tmpdst: a negative word a state (StatusTextTbl
 * 0-3, larger ones 1), bit 12 state 3, otherwise a penalty time and string 4 (5 with bit 14) */
uint32_t AttribStatus(uint16_t d0, uint16_t a2)
{
    uint16_t y = (uint16_t)((a2 ? 0x34 : 0) + (uint16_t)(d0 << 1) + (d0 >> 15));   /* (asl's carry) */
    uint16_t s = r16(RAM_OFF_tmpdst + y);
    if (s & 0x8000) {
        s = (uint16_t)~s;
        if (s >= 4) s = 1;
    } else if (s & 0x1000) s = 3;
    else {
        print(PushTime(s & 0x0FFF));
        s = s & 0x4000 ? 5 : 4;
    }
    return Fprint(s, SYM_StatusTextTbl);
}

/* Print player d0 of team a2's number and name, then at column $19 ($1A out of the pause menu) the
 * attribute d4 through attribjmp (from 3 on the rating of mask d4+2 through CalcAttrib, hot / cold
 * included, r2 its weights) */
void GetNameandAttrib(uint16_t d0, uint16_t d4, uint16_t mask, uint16_t a2, uint16_t r2)
{
    print(getname(a2, d0));
    g.printx = 0x19;
    if (!g.pausesubmenu) g.printx++;
    if (d4 >= 3) {
        uint16_t n;
        g.usehotcold = 1;
        d0 = CalcAttrib(d0, a2, mask, r2, &n);
    }
    uint32_t a0 = (SYM_attribjmp & 0xFF0000) | (uint16_t)(m16(SYM_attribjmp + d4) + (uint16_t)SYM_attribjmp);
    w16(RAM_OFF_jmlvector + 1, (uint16_t)a0);
    w16(RAM_OFF_jmlvector + 2, (uint16_t)(a0 >> 8));
    switch (a0) {
    case SYM_AttribStatus: AttribStatus(d0, a2); break;
    case SYM_AttribEnergy: AttribEnergy(d0, a2); break;
    case SYM_AttribHanded: AttribHanded(d0); break;
    case SYM_AttribWeight: AttribWeight(d0); break;
    case SYM_AttribFighting: AttribFighting(d0); break;
    case SYM_AttribRating: AttribRating(d0); break;
    }
}

/* the rating column d1 (AttribMenuSkater, or AttribMenuGoalie for a goalie: curplayer below the
 * goalie count) of player curplayer of team a2 through CalcAttrib */
static uint16_t player_rating(uint16_t d1, uint16_t a2, uint16_t hotcold)
{
    uint32_t a1 = SYM_AttribMenuSkater;
    uint16_t r2 = 0;
    if ((uint16_t)(g.curplayer[0] - GetGoalieCount(a2)) & 0x8000) {
        a1 = SYM_AttribMenuGoalie;
        r2 = 0x10;
    }
    for (uint16_t x = (uint16_t)(d1 - 1); !(x & 0x8000); x--) {
        uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
        a1 = (a1 & 0xFF0000) | (uint16_t)((s & 0xFFFF) + 4 + (s >> 16));
    }
    a1 = (a1 & 0xFF0000) | (uint16_t)((uint16_t)a1 + m16(a1));
    uint16_t mask = m16(a1);
    uint16_t n;
    g.usehotcold = hotcold;
    return CalcAttrib(g.curplayer[0], a2, mask, r2, &n);
}

/* d0 = rating column d1 of player curplayer of team a2, without the hot / cold adjustment */
uint16_t GetPlayerBaseRating(uint16_t d1, uint16_t a2) { return player_rating(d1, a2, 0); }

/* d0 = rating column d1 of player curplayer of team a2, with the hot / cold adjustment */
uint16_t GetPlayerRating(uint16_t d1, uint16_t a2) { return player_rating(d1, a2, 1); }

/* (SetLineSlot's .5: swap the line slots at a3 and a3 + 1) */
static void swap_next_slot(uint32_t a3)
{
    g.lineswap[0] = ReadLineSlot(a3);
    WriteLineSlot(a3, ReadLineSlot(a3 + 1));
    WriteLineSlot(a3 + 1, g.lineswap[0]);
}

/* Put player a in the cursor slot of the selected line (SelectedPlayerIdx) of team a2; if he is
 * already in a nearby slot of the line (searched by how far the cursor is from the line's end), the
 * old occupant of the cursor slot moves there */
void SetLineSlot(uint16_t a, uint16_t a2)
{
    g.lineplayer[0] = a;
    uint16_t d0 = (uint16_t)((uint16_t)(g.SelectedPlayerIdx - 1) << 3);
    d0 = (uint16_t)((uint16_t)GetTeamLines(a2) + d0 + 1);
    uint16_t a3, b;
    switch ((uint16_t)(g.lineslots[0] - g.linecursor[0])) {
    case 1:
        b = (uint16_t)(d0 + 1);
        goto at3;
    case 2:
        b = d0;
        goto at1;
    case 3:
        if (ReadLineSlot((uint16_t)(d0 + 4)) == g.lineplayer[0]) { SwapLineSlots(d0, 0); break; }
        b = (uint16_t)(d0 + 3);
    at3:
        if (ReadLineSlot(b) == g.lineplayer[0]) swap_next_slot((uint16_t)(b - 1));
        break;
    case 4:
        if (ReadLineSlot((uint16_t)(d0 + 4)) == g.lineplayer[0]) { swap_next_slot((uint16_t)(d0 + 3)); break; }
        b = (uint16_t)(d0 + 2);
        goto at1;
    default:
        if (ReadLineSlot((uint16_t)(d0 + 2)) == g.lineplayer[0]) { SwapLineSlots(d0, 0); break; }
        b = (uint16_t)(d0 + 3);
    at1:
        if (ReadLineSlot(b) == g.lineplayer[0]) swap_next_slot(b);
        break;
    }
    uint32_t s = (uint32_t)d0 + g.lineslots[0];
    a3 = (uint16_t)(s - g.linecursor[0] - 1 + (s >> 16));
    WriteLineSlot(a3, g.lineplayer[0]);
}

/* The roster routines' shared return (nothing to do) */
void RosterRtl(void) {}

/* Print the roster line SelectedPlayerIdx of team a2 (0 the goalies) with the attribute column
 * DispAttribCtr (stepped back past the menu's end): the line name and column title, the area below
 * cleared, then each player of the line with GetNameandAttrib under its position name (or goalie
 * mark) */
void ShowRosterLine(uint16_t a2)
{
    print(SYM_ShowRosterLinePos);
    print(AdvanceStringPtr(SYM_RosterLineNames, g.SelectedPlayerIdx));
    g.printx = 0x11;
    g.printy = 9;
    uint32_t a1;
    for (;;) {
        a1 = g.SelectedPlayerIdx ? SYM_AttribMenuSkater : SYM_AttribMenuGoalie;
        uint16_t d0 = g.DispAttribCtr;
        while (!(m16(a1) & 0x8000) && !(--d0 & 0x8000)) {
            uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
            a1 = (a1 & 0xFF0000) | (uint16_t)((s & 0xFFFF) + 4 + (s >> 16));
        }
        if (!(m16(a1) & 0x8000)) break;
        g.DispAttribCtr--;
    }
    a1 = print(a1);
    uint16_t mask = m16(a1), d4 = m16(a1 + 2);
    print(SYM_ShowRosterLinePos2);
    eraser(0x20, 0x13, g.blankchar);
    print(SYM_YSwitchTeamsTxt2);
    uint16_t d1 = (uint16_t)(GetGoalieCount(a2) - 1);
    uint32_t a5 = SYM_RosterGoalieMarks, a3 = SYM_RosterGoalieList;
    uint16_t d0 = (uint16_t)(g.SelectedPlayerIdx - 1);
    if (!(d0 & 0x8000)) {
        d0 = (uint16_t)(d0 << 3);
        uint32_t t = (SYM_TeamList & 0xFF0000) | r16(RAM_OFF_tmdata + a2);
        a3 = (t & 0xFF0000) | (uint16_t)((uint16_t)t + m16(t + 6) + d0 + 1);
        a5 = SYM_PositionNames;
        d1 = (int16_t)g.SelectedPlayerIdx > 5 ? 3 : 4;
    }
    do {
        g.printx = 1;
        a5 = print(a5);
        g.printx = 4;
        uint16_t r1 = (uint16_t)(m8(a3) - 1);
        a3 = (a3 & 0xFF0000) | (uint16_t)(a3 + 1);
        uint16_t r2 = (uint16_t)(r1 - GetGoalieCount(a2)) & 0x8000 ? 0x10 : 0;
        GetNameandAttrib(r1, d4, mask, a2, r2);
        g.printy += 2;
    } while (!(--d1 & 0x8000));
}


/* (SetupScreen: a long pointer to WRAM offset off) */
#define WRAM_PTR(off) (0x7E0000u + (off))

/* Common start of the stats screens: the old screen faded out, the team logo sprites unpacked, the
 * video registers, the small font, Framer, team block, nine tile and outline tiles one after the
 * other (each set's first tile kept), the menu background (its 7 x 7 logo box part and the whole
 * map in textmap2) and its palettes; for setupvis $21 and up (less $21) the setup background over
 * it; colour 0 black, the sprite palettes, the text map cleared, the sprites started and the
 * matchup pictures drawn */
void SetupScreen(void)
{
    g.FrameByNumber = 0;
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
    g.nextfreechar = g.smallfontchars;
    io_w16(0x2116, (uint16_t)((uint16_t)(g.smallfontchars << 4) + 0x1000 + (g.smallfontchars >> 12 & 1)));   /* (asl's carry) */
    uint16_t d0 = Decompress(SYM_SmallFontTiles, unpackbuf) >> 5;
    g.nextfreechar = (uint16_t)(g.nextfreechar + d0);
    g.Framercset = g.nextfreechar;
    remap(unpackbuf, SYM_PixelRemap8F, d0);
    g.nextfreechar = (uint16_t)(g.nextfreechar + 4);
    g.scorechars = g.nextfreechar;
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
    uint16_t n = Decompress(SYM_TeamBlocksTiles, unpackbuf);
    g.nextfreechar = (uint16_t)(g.nextfreechar + (n >> 5));
    g.ninetilechars = g.nextfreechar;
    uint32_t p0 = unpackbuf, p1;
    DMAToVRAM(&p0, n, 0xFFFF, 0);
    g.ninetilechars = g.nextfreechar;
    g.nextfreechar = (uint16_t)(g.nextfreechar + (0x140 >> 5));
    g.outlinechars = g.nextfreechar;
    p0 = SYM_FramerNineTileTiles;
    DMAToVRAM(&p0, 0x140, 0xFFFF, 0);
    g.outlinechars = g.nextfreechar;
    g.nextfreechar = (uint16_t)(g.nextfreechar + (0x120 >> 5));
    p0 = SYM_FramerOutlineTiles;
    DMAToVRAM(&p0, 0x120, 0xFFFF, 0);
    g.picchars = (uint16_t)(g.nextfreechar - 0x100);
    n = Decompress(SYM_MenuBackTiles, unpackbuf);
    g.backchars = n >> 5;
    p0 = unpackbuf;
    DMAToVRAM(&p0, n, 0x1000, 0);
    Decompress(SYM_MenuBackMap, unpackbuf);
    p0 = unpackbuf;
    p1 = WRAM_PTR(RAM_OFF_logoboxmap + 6);
    PasteMapPart(&p0, &p1, 0x0C00, 0, 0, 7, 1, 1, 7, 7);
    g.logoboxmap[0] = 7;
    g.logoboxmap[1] = 7;
    p0 = unpackbuf;
    p1 = WRAM_PTR(RAM_OFF_textmap2);
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 0, 0, 0x20);
    Decompress(SYM_SetupScreenPalette, unpackbuf);
    SetColours(unpackbuf, 0x30, 0);
    if (g.setupvis >= 0x21) {
        g.setupvis = (uint16_t)(g.setupvis - 0x21);
        n = Decompress(SYM_SetupBackTiles, unpackbuf);
        p0 = unpackbuf;
        DMAToVRAM(&p0, n, 0xFFFF, 0);
        n = Decompress(SYM_SetupBackMap, unpackbuf);
        uint32_t map = unpackbuf + 6;
        for (uint16_t y = (uint16_t)(n - 8); !(y & 0x8000); y = (uint16_t)(y - 2)) {
            uint16_t v = m16(map + y);
            if (v) w16(RAM_OFF_textmap2 + y, (uint16_t)(v + g.backchars + g.charofs2 + 0x0800));
        }
        Decompress(SYM_SetupScreenPalette, unpackbuf);
        SetColours(unpackbuf, 0x10, 0);
        SetColours(unpackbuf + 0x20, 0x10, 0x10);
        Decompress(SYM_SetupBackPalette, unpackbuf);
        SetColours(unpackbuf, 0x10, 0x20);
    }
    io_w8(0x2121, 0);
    io_w8(0x2122, 0);
    io_w8(0x2122, 0);
    Decompress(SYM_MenuBackPalette, unpackbuf);
    SetColours(unpackbuf, 0x20, 0x30);
    g.ninetilechars = (uint16_t)(g.ninetilechars + 0x1400);
    g.outlinechars = (uint16_t)(g.outlinechars + 0x1000);
    SetColours(SYM_SetupSpritePalette, 0x10, 0x80);
    SetColours(SYM_UnusedSetupSpritePalette, 0x10, 0xC0);
    ClearTextMap();
    SendTextMap2();
    WaitFrame();
    InitSprites(0x6000, 0, 0x180);
    SpriteFrameStart();
    LoadPictureMap();
    g.colourbuf[0] = 0x3568;
    uint16_t a2 = r16(RAM_OFF_a2);                        /* (the team offset the caller left in a2, or the real teams') */
    if (g.HomeTeam < 0x18) a2 = 0;
    if (g.VisTeam[0] < 0x18) a2 = 2;
    DrawMatchupPictures(a2);
}


/* (SelectLinePlayer's .10: the column title (DispAttribCtr of SkaterAttribColumns, AttribMenuGoalie
 * for the goalies, stepped back past the end), "PLAYER", the arrows when the list scrolls, and the
 * list of team a2) */
static void line_list_redraw(uint16_t a2)
{
    print(SYM_SelectLinePlayerPos2);
    eraser(0x20, 0xD, g.blankchar);
    g.printx = 0x11;
    g.printy = 2;
    uint32_t a1;
    for (;;) {
        a1 = g.SelectedPlayerIdx ? SYM_SkaterAttribColumns : SYM_AttribMenuGoalie;
        uint16_t d0 = g.DispAttribCtr;
        while (!(m16(a1) & 0x8000) && !(--d0 & 0x8000)) {
            uint32_t s = (uint32_t)(uint16_t)a1 + m16(a1);
            a1 = (a1 & 0xFF0000) | (uint16_t)((s & 0xFFFF) + 4 + (s >> 16));
        }
        if (!(m16(a1) & 0x8000)) break;
        g.DispAttribCtr--;
    }
    g.printa = 0x3800;
    a1 = print(a1);
    uint16_t mask = m16(a1), d4 = m16(a1 + 2);
    print(SYM_PlayerTxt);
    if (g.picktop[0] != g.pickfirst[0]) print(SYM_LeftArrowTxt);
    if ((uint16_t)(g.picktop[0] - g.pickfirst[0] + g.pickrows[0] - g.listcount[0]) & 0x8000) print(SYM_RightArrowTxt);
    PrintLinePlayerList(d4, mask, a2);
}

/* Line editor: pick a player of team a2 for the cursor slot (lineslots - linecursor) of line
 * SelectedPlayerIdx from the list of defensemen (the last slots) or forwards, 9 rows at a time,
 * starting at the player in the slot; left / right change the attribute column. Returns the
 * player (roster index), $FFFF when start cancels. (r7: the map width the caller left for PasteMap.) */
uint16_t SelectLinePlayer(uint16_t a2, uint16_t r7)
{
    print(SYM_SelectLinePlayerPos);
    SpriteFrameStart();
    eraser(0x20, 0xD, g.blankchar);
    Decompress(SYM_PlayerListMap, unpackbuf);
    uint32_t p0 = unpackbuf, p1 = 0x7E0000u + RAM_OFF_textmap2;
    WaitFrame();
    PasteMap(&p0, &p1, (uint16_t)(g.charofs2 + 0x0C00), 1, 1, r7);
    SendTextMapRows();
    SendTextMap2Rows();
    SpriteFrameEnd();
    WaitFrame();
    g.irqline2 = 7;
    if ((uint16_t)(g.lineslots[0] - g.linecursor[0] - 3) & 0x8000) {
        g.listcount[0] = GetDefenseCount(a2);
        g.pickfirst[0] = GetGoalieCount(a2);
        g.pickfirst[0] = (uint16_t)(g.pickfirst[0] + GetForwardCount(a2));
    } else {
        g.listcount[0] = GetForwardCount(a2);
        g.pickfirst[0] = GetGoalieCount(a2);
    }
    g.picktop[0] = g.pickfirst[0];
    g.pickrows[0] = 9;
    if ((uint16_t)(9 - g.listcount[0]) && !((uint16_t)(9 - g.listcount[0]) & 0x8000)) g.pickrows[0] = g.listcount[0];
    uint16_t d0 = (uint16_t)((uint16_t)(g.SelectedPlayerIdx - 1) << 3);
    uint16_t a3 = (uint16_t)((uint16_t)GetTeamLines(a2) + d0 + g.lineslots[0] - g.linecursor[0]);
    g.curplayer[0] = ReadLineSlot(a3);
    uint16_t t = (uint16_t)(g.pickrows[0] + g.picktop[0] - g.curplayer[0]);
    if (!(t & 0x8000)) g.curplayer[0] = t;
    else {
        t = (uint16_t)(g.curplayer[0] - g.pickrows[0] - g.picktop[0]);
        if (!(t & 0x8000)) g.picktop[0] = (uint16_t)(t + g.picktop[0]);
        g.curplayer[0] = 0;
    }
    line_list_redraw(a2);
    for (;;) {
        SendTextMap();
        SendTextMap2();
        WaitFrame();
        ReadPads();
        Pad pd = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(pd.press, pd.change, pd.held);
        if (d1 & 0x1000) return 0xFFFF;
        if (d1 & 0x8080) return (uint16_t)(g.picktop[0] + g.pickrows[0] - g.curplayer[0]);
        if (d1 & 0x0300) {
            d0 = (uint16_t)((d1 & 0x0100 ? 1 : 0xFFFF) + g.DispAttribCtr);
            if (d0 & 0x8000) continue;
            g.DispAttribCtr = d0;
            line_list_redraw(a2);
            continue;
        }
        if (!(d1 & 0x0C00)) continue;
        d0 = (uint16_t)((d1 & 0x0400 ? 0xFFFF : 1) + g.curplayer[0]);
        if (d0 & 0x8000) {
            if (!((uint16_t)(g.picktop[0] - g.pickfirst[0] + g.pickrows[0] - g.listcount[0]) & 0x8000)) continue;
            g.picktop[0]++;
            line_list_redraw(a2);
            w16(RAM_OFF_BG3VScroll, 0xFFFF);
        } else if (!((uint16_t)(d0 - g.pickrows[0]) & 0x8000)) {
            if (g.picktop[0] == g.pickfirst[0]) continue;
            g.picktop[0]--;
            line_list_redraw(a2);
            w16(RAM_OFF_BG3VScroll, 0xFFFF);
            g.BG1VScroll = 0xFFFF;
        } else {
            g.curplayer[0] = d0;
            line_list_redraw(a2);
        }
    }
}

/* The team roster screen (pause menu) of the pause pad's team: the title, frames, team block and
 * the current line with an attribute column; then the loop (TeamRosterLoop): start leaves
 * (RosterRtl), Y switches team (SwapRosterTeam, then TeamRosterRedraw), left / right step the
 * column (MoveRosterColumn), up / down the line (0-7) */
void TeamRosterScreen(void)
{
    uint16_t a2 = GetPauseTeam();
    g.SelectedPlayerIdx = (uint16_t)(r16(RAM_OFF_tmline + a2) + 1);
    g.DispAttribCtr = 0;
    g.setupvis = r16(RAM_OFF_HomeTeam + a2);
    g.setuphome = 0x1D;
    SetupScreen();
    PrintLetterSprites(SYM_TeamRosterTitle, 0x4C, 0x18);
    FadeIn();
    MenuScreenHDMA();
    ScreenOn();
    MemFill(0x7E0000u + RAM_OFF_textmap, 0x800, (uint16_t)(g.blankchar + g.charofs));
    print(SYM_PlayerRatingTxt);
    g.printa = 0x0800;
    FramerOutline(0xF, 3);
    print(SYM_TeamRosterPos);
    g.printa = 0x0800;
    FramerOutline(0xF, 3);
redraw:
    print(SYM_TeamRosterPos2);
    DrawTeamBlocks(a2 ? g.VisTeam[0] : g.HomeTeam, SYM_TeamRosterTitle, a2);
    SpriteFrameEnd();
    g.colourbuf[1] = 0x7F97;
    g.colourbuf[0x21] = 0xFFFF;
    SendColours();
    ShowRosterLine(a2);
    for (;;) {
        SendTextMap();
        WaitFrame();
        ReadPads();
        Pad pd = ReadPausePad();
        uint16_t d1 = ProcessInputWithRepeat(pd.press, pd.change, pd.held);
        if (d1 & 0x1000) return;
        if (d1 & 0x4000) {
            a2 ^= 2;
            SpriteFrameStart();
            g.setupvis = r16(RAM_OFF_HomeTeam + a2);
            PrintLetterSprites(SYM_TeamRosterSwap, 0x4C, 0x18);
            DrawMatchupPictures(a2);
            goto redraw;
        }
        uint16_t d0;
        if (d1 & 0x0300) {
            d0 = (uint16_t)((d1 & 0x0100 ? 1 : 0xFFFF) + g.DispAttribCtr);
            if (d0 & 0x8000) continue;
            g.DispAttribCtr = d0;
            ShowRosterLine(a2);
            continue;
        }
        if (!(d1 & 0x0C00)) continue;
        d0 = (uint16_t)((d1 & 0x0800 ? 0xFFFF : 1) + g.SelectedPlayerIdx);
        if ((d0 & 0x8000) || d0 >= 8) continue;
        g.SelectedPlayerIdx = d0;
        ShowRosterLine(a2);
    }
}
