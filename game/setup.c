/* setup: game setup, the scouting report and the sort list (src/routines/setup). */
#include "game.h"

#define WRAM24(off) (0x7E0000u + (off))
/* RenderBigText's state: the string, the letter pixels (hlsavebuf), the pixel offset r3 in the tile, its
 * complement r3+2, the shift masks' indexes r0+2 / r2+2, the letter's pixel rows r1 / r1+2 */
static struct { uint32_t p0, p1; uint16_t r1, r1h, r3, r3h, r0h, r2h; } bt;
/* Intro: the video registers from the table at p0 (BGMODE, M7SEL, TM, TS, BG1-4SC, BG12NBA,
 * BG34NBA) */
#define introbuf24 (0x7E0000u + RAM_OFF_introbuf)

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

/* the next value of range n out of the password bits, added to the checksum in d7 (.sd) */
static uint16_t rpb_value(uint16_t n)
{
    uint16_t v = SuperDiv(n);
    g.d7 = (uint16_t)(g.d7 + v);
    return v;
}

/* value d0 of range d1 into the password bits, added to the checksum in d7 (.t1) */
static void wpb_value(uint16_t d0, uint16_t d1)
{
    g.d7 = (uint16_t)(g.d7 + d0);
    PushBits(d0, d1);
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

/* RAM's jml trampoline: the routine at jmlvector+1 (RenderBigText's letter hooks) */
static void jml_vector(void)
{
    uint32_t t = r16(RAM_OFF_jmlvector + 1) | (uint32_t)r8(RAM_OFF_jmlvector + 3) << 16;
    if (t == SYM_BigLetterSelect) BigLetterSelect();
    else if (t == SYM_BigLetterPlayoff) BigLetterPlayoff();
}

/* Score lo / hi above the star's (signed words, high first) */
static int star_above(uint16_t lo, uint16_t hi, const uint16_t *s)
{
    if (hi != s[1]) return (int16_t)hi > (int16_t)s[1];
    return (int16_t)lo > (int16_t)s[0];
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

/* (SaveRegsStartHL2: the game state back from hlsavebuf after an other game's highlight, as hl_save
 * left it) */
static void hl_restore(void)
{
    uint16_t x;
    for (x = 0; x < 0x0781; x += 2) w16(RAM_OFF_OOlist + 0x10 + x, r16(RAM_OFF_hlsavebuf + x));
    g.ScoreSumbytes[0] = r16(RAM_OFF_hlsavebuf + x);
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

/* (the password character d5 at the cursor printed in the one-character string PWCharStr) */
static void print_pw_char(uint16_t d5)
{
    g.PWCharStr[1] = rom16(SYM_PasstoText + d5);
    g.printx++;
    g.printy -= 2;
    print(RAM_OFF_PWCharStr);
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

/* (states 4 and 6: sprites 0 to n - 1 at ($38, $2F), then state next) */
static uint16_t intro_sprites(IntroDP *s, uint16_t n, uint16_t next)
{
    SpriteFrameStart();
    for (uint16_t k = 0; k < n; k++) DrawSprite(0, 0x38, 0x2F, k, introbuf24 + 0xA40);
    SpriteFrameEnd();
    s->next = next;
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

/* Start the hat trick sprites: clear the ten slots and set hattrick */
void StartHatTrick(void)
{
    for (int i = 0; i < 10; i++) g.hatstep[i] = 0;
    g.hattrick = 1;
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

/* The password entry screen: the characters in five rows of six, the cursor on the password's first
 * character; returns d2 as PasswordRedraw leaves it */
uint16_t PasswordScreen(void)
{
    forcefade();
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
    g.fadein = 0x0A;
    return PasswordRedraw(d4, d5);
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
            Readjoy1(&d0, &d1, &d2, &d3);
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

/* Move the password cursor to character d5: rows of 6 from the record's position */
void PlacePasswordCursor(uint16_t d5)
{
    print(SYM_PlacePasswordCursorPos);
    uint16_t q = DivX8U(d5, 6), rem = (uint16_t)(d5 - q * 6);   /* (the divider's remainder, RDMPYL) */
    g.printy = (uint16_t)((uint16_t)(q << 1) + g.printy + (q >> 15));   /* (asl's carry) */
    g.printx = (uint16_t)((uint16_t)(rem << 1) + g.printx + (rem >> 15) - 2);
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
        Readjoy1(&d0, &d1, &d2, &d3);
        if (!d3) continue;
        if (d3 & 0x4000) {
            g.OptPlayMode = 2;
            Opening2();
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

/* Print the password screen keys */
void PrintPasswordHelp(void)
{
    print(SYM_SelectTxt);
    print(SYM_ANextTxt);
    print(SYM_YBackTxt);
    print(SYM_PressStartTxt);
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

/* Turn the password into the playoff state */
void DecodePW(void) { PWtoBits(); ReadPassBits(); }

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

/* Clear the 8 words of the password bits */
void ClrPassBits(void) { for (int i = 0; i < 8; i++) g.passbits[i] = 0; }

/* The password bits = the password bits * d1 + d0 */
void PushBits(uint16_t d0, uint16_t d1)
{
    SuperMult(d1);
    SuperAdd(d0);
}

/* Blank the password */
void ResetPassWord(void)
{
    g.PassWord[0] = 0;
    for (int i = 15; i > 0; i--) g.PassWord[i] = 0x1E;
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
        printscores1_ps_name(d3);
        g.printy = (uint16_t)(5 + k);
        g.printx = 0x11;
        print(PlayerInitialName(a2, *stars[k]));
    }
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

/* Clear the objects ($76E bytes from Xpos) and set the 16 sort objects up from their start table
 * (9 words each: x, y, height, frame, attribute, collision width and height, assignment, flags),
 * the sort lists in order; then sort them */
void defaultsprites_0(void)
{
    for (int x = 0; x < 0x76E; x += 2) w16(RAM_OFF_Xpos + x, 0);
    g.puckz = 0;
    for (int o = 0; o < 16; o++) {
        uint32_t t = SYM_defaultsprites_0_3 + (uint32_t)o * 18;
        uint16_t y = (uint16_t)(o * 2);
        w16(RAM_OFF_SCnum + y, (uint16_t)o);
        w16(RAM_OFF_oldframe + y, 0xFFFF);
        w16(RAM_OFF_pnum + y, 0xFFFF);
        w16(RAM_OFF_Xpos + y, rom16(t));
        w16(RAM_OFF_Ypos + y, rom16(t + 2));
        w16(RAM_OFF_frame + y, rom16(t + 6));
        w16(RAM_OFF_attribute + y, rom16(t + 8));
        w16(RAM_OFF_radiusx + y, rom16(t + 10));
        w16(RAM_OFF_radiusy + y, rom16(t + 12));
        w16(RAM_OFF_asslist + o * 8, rom16(t + 14));
        w16(RAM_OFF_asscur + y, rom16(t + 14));
        w16(RAM_OFF_pflags + y, rom16(t + 16));
        w16(RAM_OFF_pfnc + y, y == 0x1E);
        w16(RAM_OFF_pfgoal + y, o * 18 < 0x6C);
        if (o * 18 >= 0x6C && o * 18 < 0xD8) w16(RAM_OFF_pfteam + y, 0x34);
        g.OOlist[o] = (uint8_t)(o * 2);
        w16(RAM_OFF_OOlistpos + y, (uint16_t)o);
    }
    w16(RAM_OFF_position + netobj1, 0xFFFF);
    w16(RAM_OFF_position + netobj2, 0xFFFF);
    SprSort();
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
    g.BigFontchars = (uint16_t)(d0 + g.smallfontchars + (n >> 4 & 1));   /* (the last lsr's carry) */
    remap(unpackbuf, SYM_PixelRemap8F, d0);
    n = Decompress(SYM_SmallFontTiles, unpackbuf);
    g.scorechars = (uint16_t)((n >> 5) + g.BigFontchars + (n >> 4 & 1));
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
        if ((g.padnew1[0] | g.padnew1[1]) == 0x1000) break;
        if (g.Vcount[0] == g.d7) continue;
        g.d7 = g.Vcount[0];
        PulseColour((uint16_t)(RAM_OFF_palbuffer + 0x2E), 0x7E);
        DMAToCGRAM(WRAM24(RAM_OFF_palbuffer + 0x20), 0x10, 0x10);
        uint16_t j = g.PadRaw1[0] | g.PadRaw1[1];
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

/* The games of playoff round playoffRoundIdx less one (*d1) and its first win bit (*d2) */
void GetShifter(uint16_t *d1, uint16_t *d2)
{
    uint16_t b = 0xFFF0, n = 0x10, y = g.playoffRoundIdx;
    do { b = (uint16_t)(b + n); n >>= 1; } while (!(--y & 0x8000));
    *d1 = (uint16_t)(n - 1);
    *d2 = b;
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
        uint16_t t = g.Opt1team >= 0x1A ? 0x19 : g.Opt1team;
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

/* Which team each controller plays: in the playoffs the series of the user's team gives HomeTeam,
 * VisTeam, Opt2team and gamenum and the pads in use their team (the same table word for each); in
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
        if (d2 == r16(RAM_OFF_gst1 + x)) { d0 = 0; g.Opt2team = r16(RAM_OFF_gst2 + x); break; }
        if (d2 == r16(RAM_OFF_gst2 + x)) { d0 = 4; g.Opt2team = r16(RAM_OFF_gst1 + x); break; }
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

/* The default menu choices at power on: the 8 option words from OptPlayMode on, not a demo */
void DefaultMenus(void)
{
    g.notdemo = 0xFFFF;
    for (int y = 0; y < 16; y += 2) w16(RAM_OFF_OptPlayMode + y, rom16(SYM_DefaultMenus_1 + y));
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

/* The scouting report's shared return (nothing to do) */
void UpdateScoutingDisplay_rtl(void) {}

/* Start paragraph d0 of PregameText a1 on a new line (a blank line before it once text is up) */
void UpdateScoutingDisplay_scroll(uint32_t a1, uint16_t d0)
{
    if (!((uint16_t)(g.VertLineScrolling - 1) & 0x8000)) ScrollDisplayUp();
    ScrollDisplayUp();
    UpdateScoutingDisplay_next(a1, a1, d0);
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
    case 'r': GetTeamArena_home(d0); return;
    case 'v': GetTeamCity_vis(d0); return;
    case 'h': GetTeamCity_home(d0); return;
    case 'V': ScoutVisName(d0); return;
    case 'H': ScoutHomeName(d0); return;
    case 'm': GetTeamNickname_vis(d0); return;
    case 'n': GetTeamNickname_home(d0); return;
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

/* End the string built in mesarea: its length word = a1 - mesarea; a1 = mesarea */
uint32_t FinalizeTextBuffer(uint32_t a1)
{
    g.mesarea[0] = (uint16_t)(a1 - RAM_OFF_mesarea);
    return (a1 & 0xFF0000) | RAM_OFF_mesarea;
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

/* Back to the option screens from anywhere (the end of a game or the demo, exit game): out of the playoffs,
 * then restart_go on a fresh stack */
void Opening2(void)
{
    g.playoffRoundIdx = 0;
    hal_stack_reset(restart_go);
}

/* End of a period: fade out, the menu or playoff music, the teams turned around (players out of the game
 * back on the bench), the next period number (tied after the third: overtime, again in a playoff game),
 * the intermission; then game over (GameOver) or the next period (StartPer). Never returns on the
 * cartridge. */
void PeriodOver(void)
{
    forcefade();
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
    forcefade();
    while (g.SoundFadeSpeed) hal_wait_interrupt();
    SPCGameUpload();
    g.BA_Sktr_SCnum = 0xFFFF;
    StartPer();
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

/* The highlight of playoff game a0 / 2 (d1) (the registers kept around it) */
void SaveRegsStartHL2(uint16_t a0, uint16_t d1) { StartHL2(a0, d1); }

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
        if (waitx(0xB4).held & 0x1000) goto skip_rest;
        g.padchangeany = 0;
        forcefade();
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
        if (g.puckc < 0) g.puckc = (int16_t)(randomd0A(0x0C) << 1);
        uint16_t x = (uint16_t)g.puckc;
        if (!r16(RAM_OFF_position + x)) g.puckc = (int16_t)(x - 2);
        SprSort();
        uint16_t left = 0xB4;
        g.OldVcount = g.Vcount[0];
        for (;;) {
            while (g.Vcount[0] == g.OldVcount) hal_wait_interrupt();
            uint16_t v = g.Vcount[0];
            frame_steps((uint16_t)(v - g.OldVcount));
            g.lastframevc = v;
            g.OldVcount = v;
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
        forcefade();
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
        if (waitx(0xB4).press & 0x1000) goto skip_on;
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

/* Erase the goal box: its position string (boxPos) printed, then 19 x 9 blank tiles */
void box(void)
{
    print(SYM_boxPos);
    eraser(0x13, 9, g.blankchar);
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
            g.PenCntDwn = (int16_t)(g.PenCntDwn + 0xA);
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
    g.framercset = (uint16_t)s;
    g.BigFontchars = (uint16_t)(g.framercset + 4 + (s >> 16));
    remap(unpackbuf, SYM_PixelRemap83, t);
    remap(SYM_FramerTiles, SYM_PixelRemap87, 4);
    n = unpack(SYM_BigFontTiles, &t, &c);
    g.scorechars = (uint16_t)(t + g.BigFontchars + c);
    send_tiles(n);
    n = unpack(SYM_TeamBlocksTiles, &t, &c);
    g.ExtraChars = (uint16_t)(t + g.scorechars + c);
    send_tiles(n);
    setupice_reset();
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
    g.vcountmark[1] = (uint16_t)(g.Vcount[1] - 2);
    defaultsprites_0();
    nullrtl();
    if (g.gmode & gmdir)
        for (int o = 11; o >= 0; o--) g.pfgoal[o] ^= 1;
    setplayercolors();
    g.fadein = 0;
    FadeIn();
    ScreenOn();
}

/* Set the team colours and send the whole palette to CGRAM */
void setplayercolors(void)
{
    SetTeamColors();
    DMAToCGRAM(0x7E0000u | RAM_OFF_palbuffer, 0x100, 0);
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

void nullrtl(void) {}

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

/* mesarea = the uniform number and name of roster player d0 of team a2; a1 = mesarea */
uint32_t getname(uint16_t a2, uint16_t d0)
{
    uint32_t rec = GetPlayerNamePointer(a2, d0), bank = rec & 0xFF0000;
    w16(RAM_OFF_mesarea, 4);
    uint16_t a0 = (uint16_t)(rec + m16(rec));
    ConvertByteToDigits(m8(bank | a0), RAM_OFF_mesarea + 2);
    appstring(SYM_PlayerNumNamePos, RAM_OFF_mesarea);
    appstring(rec, RAM_OFF_mesarea);
    return RAM_OFF_mesarea;
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

/* mesarea = the number, first initial and last name of roster player d0 of team a2 (a word written at a
 * time); a1 = mesarea */
uint32_t FormatPlayerNumInitial(uint16_t a2, uint16_t d0)
{
    uint32_t a0 = GetPlayerNamePointer(a2, d0), bank = a0 & 0xFF0000;
    uint16_t p0 = (uint16_t)a0;
    w16(RAM_OFF_mesarea, 2);
    uint16_t a1 = (uint16_t)ConvertByteToDigits(m8(bank | (uint16_t)(p0 + m16(a0))), RAM_OFF_mesarea + 2);
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

/* mesarea = the first initial and last name of player TempPlOffset (bit 15: a visitor); a1 = mesarea */
uint32_t GetPlayerInitial(void)
{
    uint16_t d0 = g.TempPlOffset, a2 = 0;
    if (d0 & 0x8000) { d0 &= 0xFF; a2 = 2; }
    return PlayerInitialName(a2, d0);
}

/* Erase the scouting report name line of side a2 */
void ClearRosterName(uint16_t a2)
{
    ScoutNamePos(a2);
    eraser(0x0B, 2, g.blankchar);
}

/* The two digit (BCD) number d0 as two characters at [a1], a blank for a leading zero; returns a1
 * past them */
uint32_t ConvertByteToDigits(uint16_t d0, uint32_t a1)
{
    uint16_t lo = 0x20;
    if ((d0 & 0xFF) >= 10) lo = (uint16_t)(((d0 & 0xFF) >> 4) + 0x30);
    uint16_t w = (uint16_t)(lo | ((d0 & 0xF) + 0x30) << 8);
    if (is_wram(a1)) { w8(wram_off(a1), (uint8_t)w); w8(wram_off((a1 + 1) & 0xFFFFFF), (uint8_t)(w >> 8)); }
    return (a1 & 0xFF0000) | (uint16_t)(a1 + 2);
}

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

/* The roster record of player d0 of team a2 (a0): past the team header, then d0 records of (length word
 * + 8) bytes */
uint32_t GetPlayerNamePointer(uint16_t a2, uint16_t d0)
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

/* Reset the scouting report text scroll; a0 = TestList */
uint32_t InitScoutingDisplay(void)
{
    g.VertLineScrolling = 0xFFFF;
    g.PlayerScrollCtr = 0xFFFF;
    g.SelectedPlayerIdx = 0;
    g.ScoutingReportTimer = 0;
    return TestList;
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

/* Intro: the start state: BG1 scroll $180 / $190 (job $10 sends it), next state 3, d6 the timeout
 * (Vcount + $2D0), d7 the earliest end (Vcount + $F0), the Mode 7 registers. (The skip flag d5+2 is
 * cleared here and never set: its branches are dead.) */
void IntroSequence_3(IntroDP *s)
{
    w16(RAM_OFF_introBG1H, 0x0180);
    g.BG1HScroll = 0x0180;
    w16(RAM_OFF_introBG1H + 2, 0x0190);
    g.BG1VScroll = 0x0190;
    g.introHookFlags |= 0x0010;
    s->next = 3;
    s->d6 = (uint16_t)(g.Vcount[0] + 0x02D0);
    g.d7 = (uint16_t)(g.Vcount[0] + 0x00F0);
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

/* State 2: wait until Vcount reaches d6+2, then state d5 */
uint16_t IntroSequence_8(IntroDP *s)
{
    if (!((uint16_t)(g.Vcount[0] - s->d6h) & 0x8000)) s->next = s->d5;
    return 0;
}

/* State 3: sound 0 and the E logo */
uint16_t IntroSequence_9(IntroDP *s)
{
    PlaySound(0);
    return intro_logo(s, SYM_EALogoEMap, 4);
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

/* State 9: the E palette (colours from $30), then state 5; state 10: the A palette ($40), then 7 */
uint16_t IntroSequence_15(IntroDP *s) { return intro_fade(s, SYM_EALogoEPalette, 0x30, 5); }

uint16_t IntroSequence_16(IntroDP *s) { return intro_fade(s, SYM_EALogoAPalette, 0x40, 7); }

/* State 11: state 8 */
uint16_t IntroSequence_17(IntroDP *s) { s->next = 8; return 0; }

/* State 12: 30 frames in state 2, then state 8 */
uint16_t IntroSequence_18(IntroDP *s)
{
    s->d6h = (uint16_t)(g.Vcount[0] + 0x1E);
    s->next = 2;
    s->d5 = 8;
    return 0;
}

/* State 13: the end once at full brightness and d7 is reached, with a button or at the d6 timeout */
uint16_t IntroSequence_19(IntroDP *s)
{
    if (g.brightness != 0x0F) return 0;
    if ((uint16_t)(g.Vcount[0] - g.d7) & 0x8000) return 0;
    if (r16(RAM_OFF_JoyPad)) return 1;
    return !((uint16_t)(g.Vcount[0] - s->d6) & 0x8000);
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
    g.DMAlisthead = 0;
    g.DMAlistend = 0;
    IntroSequence_33(SYM_IntroSequence_1);
    IntroSequence_2();
    g.introHookFlags = 0;
    IntroSequence_3(&s);
    g.fadein = 0;
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
