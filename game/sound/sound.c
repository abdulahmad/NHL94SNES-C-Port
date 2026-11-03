/* sound: the sound CPU's uploads and the sound effects (src/routines/sound). */
#include "game.h"

#define APUIO0 0x2140
#define APUIO1 0x2141
#define APUIO2 0x2142

/* Sound driver command 3: send the n (X) bytes of the sample at [p0] to the sound CPU, loop point
 * (the next sample slot) y, each byte through APUIO2 with its count on APUIO1 echoed back */
void SPCSendSample(uint32_t p0, uint16_t n, uint16_t y)
{
    while (io_r8(APUIO0)) {}
    io_w16(APUIO2, y);
    io_w8(APUIO0, 3);
    io_w8(APUIO1, 0);
    while (io_r8(APUIO0) != 3) {}
    uint16_t i = 0;
    do {
        io_w8(APUIO2, m8((p0 + i) & 0xFFFFFF));
        i++;
        io_w8(APUIO1, (uint8_t)i);
        while (io_r8(APUIO1) != (uint8_t)i) {}
    } while (--n);
    io_w8(APUIO0, 0);
}

/* Start fading the sound out (SoundUpdate lowers SoundVolume by $200 a frame) */
void FadeOutSound(void) { SetSoundFade(0x0200); }

/* Ask for streamed sound effect a (1-6): a different stream playing (3 and up) has its sound
 * stopped, and the stream starts over; a different stream 1 or 2 keeps playing */
void StreamSfx(uint16_t a)
{
    g.SoundLock[0]--;
    uint16_t s = g.streamsfx;
    if (s) {
        if (s == a || s < 3) { g.SoundLock[0]++; return; }
        StopSound((uint8_t)rom16(SYM_StreamSfxSounds - 2 + (uint16_t)(s << 1)));
    }
    g.streamstate = 0;
    g.streamsfx = a;
    g.SoundLock[0]++;
}

/* Turn the title sample set into the menu set: the last three samples dropped (command 4), the
 * four of the menu set after its first 12 sent */
void SPCTitleToMenu(void)
{
    g.cursong = 0;
    SPCCmd04();
    SPCCmd04();
    SPCCmd04();
    SPCUploadBlock(SYM_SampleTimp);
    SPCUploadBlock(SYM_SampleDist);
    SPCUploadBlock(SYM_SamplePlayoffOrgan);
    SPCUploadBlock(SYM_SampleAPower);
    g.sndbank[0] = SoundBankMenu;
}

/* (the sound entry at InstPtr, in the sound entries' bank: the data bank of the sound code) */
static uint32_t inst_at(uint8_t y) { return ((uint32_t)g.SoundBank[0] << 16) + g.InstPtr + y; }
static uint8_t ip8(uint8_t y) { return lm8(inst_at(y) & 0xFFFFFF); }
static uint16_t ip16(uint8_t y) { return lm16(inst_at(y) & 0xFFFFFF); }
/* (an entry of SoundTable: its offset word plus the table's address) */
static uint16_t sound_entry(uint16_t y) { return (uint16_t)(m16((g.SoundTable + y) & 0xFFFFFF) + (uint16_t)g.SoundTable); }

/* Put the note of track x / 2 (CurTrack; TrackInst its entry, EventCode the note, EventData1 the
 * velocity, EventData2 the length) on a free voice with its pitch, volumes and envelope, sent by
 * SeqCmd07; nothing without an entry, a voice, or a length for two special entries. The 16-bit
 * accumulator as the 65816 leaves it: the voice in the low byte (the high byte the pitch's, from
 * SeqCmd07), or the entry / length / $FF left on the paths that start nothing. */
uint16_t StartSoundVoice(uint8_t x)
{
    uint16_t a = r16(RAM_OFF_TrackInst + x);
    if (!a) return 0;
    g.InstPtr = a;
    if (a == 0xDF80 || a == 0xDFB0) {
        a = g.EventData2;
        if (!a || (a & 0x8000)) return a;
    }
    uint8_t v = FindFreeVoice();
    g.CurVoice = v;
    if (v & 0x80) return (uint16_t)((a & 0xFF00) | v);
    uint8_t vo = (uint8_t)(v << 1);
    w8(RAM_OFF_VoiceOfs, vo);
    w8(RAM_OFF_VoiceTrack + v, g.CurTrack);
    w8(RAM_OFF_VoicePriority + v, ip8(0x2A));
    w8(RAM_OFF_VoiceState + v, ip8(0x15) & 0x80 ? 2 : 1);
    uint16_t len = g.InstPtr == 0xE130 ? 0x002D : g.EventData2;
    w16(RAM_OFF_VoiceLength + vo, (uint16_t)(len - 1));
    w16(RAM_OFF_VoiceAge + vo, 0);
    w16(RAM_OFF_VoiceVibCount + vo, 0);
    w16(RAM_OFF_VoiceVibOfs + vo, 0);
    w16(RAM_OFF_VoiceVibRate + vo, ip16(0x1C));
    w16(RAM_OFF_VoiceVibRateFrac + vo, 0);
    w16(RAM_OFF_VoiceVibDepth + vo, ip16(0x20));
    w16(RAM_OFF_VoiceVibDepthFrac + vo, 0);
    w16(RAM_OFF_VoiceVibSpeed + vo, ip16(0x1E));
    w16(RAM_OFF_VoiceVibSpeedFrac + vo, 0);
    w16(RAM_OFF_VoiceVibDir + vo, ip16(0x2D) & 0x8000 ? 0x0000 : 0xFFFF);
    uint16_t gain = ip16(0x0A);
    w16(RAM_OFF_VoiceGain + vo, gain);
    w16(RAM_OFF_StartGain, gain);
    uint8_t ct = g.CurTrack;
    if (ct == 0x10 && !(g.sflags & sf2replay)) w16(RAM_OFF_recsfxvol, gain);
    w16(RAM_OFF_VoiceInst + vo, g.InstPtr);
    uint8_t vel = rom8(SYM_VolumeCurve + r8(RAM_OFF_EventData1));
    if (vel == 0x40) {
        g.VoiceVolL = g.TrackVolL[ct];
        g.VoiceVolR = g.TrackVolR[ct];
    } else {
        vel = (uint8_t)(vel << 1);
        w8(RAM_OFF_EventData1, vel);
        w8(RAM_OFF_VoiceVelocity + v, vel);
        g.VoiceVolL = (uint8_t)MulFrac8((uint16_t)(vel << 8 | g.TrackVolL[ct]));
        g.VoiceVolR = (uint8_t)MulFrac8((uint16_t)(vel << 8 | g.TrackVolR[ct]));
    }
    uint8_t duck = g.MusicDuck[0];
    if (duck && ct != 0x10) {
        g.VoiceVolR >>= 1; g.VoiceVolL >>= 1;
        if (duck & 0x80) { g.VoiceVolR >>= 1; g.VoiceVolL >>= 1; }
    }
    uint8_t note = (uint8_t)(r8(RAM_OFF_EventCode) + ip8(0x04));
    w8(RAM_OFF_EventCode, note);
    w8(RAM_OFF_VoiceNote + v, note);
    uint8_t n = ct == 0x09 ? (uint8_t)(0x3F + ip8(0x04)) : note;
    uint16_t p = rom16(SYM_PitchTable + (uint8_t)(n << 1));
    w16(RAM_OFF_VoiceBasePitch + vo, p);
    w16(RAM_OFF_EventData1, p);
    uint8_t range = g.TrackBendRange[ct], bend = g.TrackBend[ct];
    if (range && bend) {
        if (!(bend & 0x80)) {
            uint16_t d = (uint16_t)(rom16(SYM_PitchTable + (uint8_t)((uint8_t)(note + range) << 1)) - p);
            w16(RAM_OFF_EventData2, d);
            p = (uint16_t)(MulFrac16(d, bend) + p);
        } else {
            uint16_t d = (uint16_t)(p - rom16(SYM_PitchTable + (uint8_t)((uint8_t)(note - range) << 1)));
            w16(RAM_OFF_EventData2, d);
            uint8_t nb = (uint8_t)(0 - bend);
            if (nb & 0x80) w16(RAM_OFF_MulFracResult, d);
            else MulFrac16(d, nb);
            p = (uint16_t)(p - r16(RAM_OFF_MulFracResult));   /* (the zero multiply leaves $43 as it was) */
        }
        w16(RAM_OFF_EventData1, p);
    }
    uint16_t pitch = (uint16_t)(ip16(0x02) + p);
    w16(RAM_OFF_VoicePitch + vo, pitch);
    if (ct == 0x10 && !(g.sflags & sf2replay)) w16(RAM_OFF_recsfxpitch, pitch);
    uint8_t y = SeqCmd07();
    return (uint16_t)((r16(RAM_OFF_NotePitch) & 0xFF00) | y);
}

/* Each frame, each voice in use: a sound effect voice (track $10 and up) counted down
 * (UpdateVoice); its age counted up, and between the entry's vibrato start and end ages, when the
 * vibrato counter runs out, its rate, depth and speed stepped and the vibrato offset moved (only
 * then added to the pitch sent); then by its state at the start: 2 the pitch (SeqCmd0B), 4 the
 * gain lowered by entry byte $10 (stopped below 0), others gain and pitch */
void UpdateSfxVoices(void)
{
    uint8_t x = 0, y = 0;
    do {
        g.CurVoice = y;
        w8(RAM_OFF_VoiceOfs, x);
        uint8_t st = g.VoiceState[y];
        if (!st) goto next;
        if (g.VoiceTrack[y] >= 0x10) UpdateVoice(y, x);
        g.InstPtr = r16(RAM_OFF_VoiceInst + x);
        w16(RAM_OFF_SendPitch, r16(RAM_OFF_VoicePitch + x));
        w16(RAM_OFF_GainWord, r16(RAM_OFF_VoiceGain + x));
        uint16_t age = (uint16_t)(r16(RAM_OFF_VoiceAge + x) + 1);
        w16(RAM_OFF_VoiceAge + x, age);
        if (age < ip16(0x1A) && age >= ip16(0x18)) {
            uint16_t c = (uint16_t)(r16(RAM_OFF_VoiceVibCount + x) - 1);
            w16(RAM_OFF_VoiceVibCount + x, c);
            if (c & 0x8000) {
                uint32_t s = (uint32_t)ip16(0x2A) + r16(RAM_OFF_VoiceVibRateFrac + x);
                w16(RAM_OFF_VoiceVibRateFrac + x, (uint16_t)s);
                uint16_t rate = (uint16_t)(ip16(0x2C) + r16(RAM_OFF_VoiceVibRate + x) + (s >> 16));
                w16(RAM_OFF_VoiceVibRate + x, rate);
                w16(RAM_OFF_VoiceVibCount + x, rate);
                s = (uint32_t)ip16(0x26) + r16(RAM_OFF_VoiceVibDepthFrac + x);
                w16(RAM_OFF_VoiceVibDepthFrac + x, (uint16_t)s);
                uint16_t depth = (uint16_t)(ip16(0x28) + r16(RAM_OFF_VoiceVibDepth + x) + (s >> 16));
                w16(RAM_OFF_VoiceVibDepth + x, depth);
                s = (uint32_t)ip16(0x22) + r16(RAM_OFF_VoiceVibSpeedFrac + x);
                w16(RAM_OFF_VoiceVibSpeedFrac + x, (uint16_t)s);
                uint16_t speed = (uint16_t)(ip16(0x24) + r16(RAM_OFF_VoiceVibSpeed + x) + (s >> 16));
                w16(RAM_OFF_VoiceVibSpeed + x, speed);
                uint32_t dir = RAM_OFF_VoiceVibDir + x, ofs = RAM_OFF_VoiceVibOfs + x;
                if (!(r16(dir) & 0x8000)) {
                    uint16_t o = (uint16_t)(speed + r16(ofs));
                    w16(ofs, o);
                    if (!(o & 0x8000) && o >= depth) {
                        if (ip16(0x2D) & 0x4000) w16(dir, (uint16_t)(r16(dir) - 1));
                        else w16(ofs, 0);
                    }
                } else {
                    uint16_t o = (uint16_t)(r16(ofs) - speed);
                    w16(ofs, o);
                    if ((o & 0x8000) && (uint16_t)-o >= depth) {
                        if (ip16(0x2D) & 0x8000) w16(dir, (uint16_t)(r16(dir) + 1));
                        else w16(ofs, 0);
                    }
                }
                w16(RAM_OFF_SendPitch, (uint16_t)(r16(ofs) + r16(RAM_OFF_SendPitch)));
            }
        }
        if (st == 2) SeqCmd0B(y);
        else if (st == 4) {
            uint16_t gn = r16(RAM_OFF_VoiceGain + x), sub = ip16(0x10);
            w16(RAM_OFF_VoiceGain + x, (uint16_t)(gn - sub));
            if (gn < sub) StopVoice(y, x);
            else SeqCmd0B(y);
        } else SeqCmd0C0B(y);
    next:
        x = (uint8_t)(x + 2);
    } while (++y < 8);
}

/* The start shared by QueueSound and PlaySound (InstPtr the entry, track $10, X = $20): the
 * note, its length and the track's bend range and volumes from the entry (the bend range and bend
 * stored at the track's offset $20 as an index: past their lists), the velocity SfxVelocity's
 * high byte; StartSoundVoice's accumulator */
uint16_t PlaySoundEntry(void)
{
    w16(RAM_OFF_EventData2, ip16(0x06));
    w8(RAM_OFF_EventCode, ip8(0x05));
    w8(RAM_OFF_TrackBendRange + 0x20, ip8(0x2F));
    w8(RAM_OFF_TrackBend + 0x20, 0);
    uint8_t r;
    if (!g.SoundMono[0]) { g.TrackVolL[0x10] = ip8(0x12); r = ip8(0x13); }
    else g.TrackVolL[0x10] = r = ip8(0x17);
    g.TrackVolR[0x10] = r;
    w8(RAM_OFF_EventData1, (uint8_t)(g.SfxVelocity >> 8));
    uint16_t a = StartSoundVoice(0x20);
    g.SoundLock[0]++;
    return a;
}

/* Play sound a (a byte) at once on the sound effect track: no replay record, no test of a voice
 * playing it already */
void PlaySound(uint16_t a)
{
    g.SoundLock[0]--;
    g.CurTrack = 0x10;
    w8(RAM_OFF_CurTrackOfs, 0x20);
    g.InstPtr = sound_entry((uint8_t)(a << 1));
    w16(RAM_OFF_TrackInst + 0x20, g.InstPtr);
    PlaySoundEntry();
}

/* Start sound a on a sound effect voice (none when a voice plays it already); outside a replay
 * it is kept in recsfx for the replay record, the one before moved to recsfx2. Returns the
 * accumulator it leaves: the voice already playing the sound, or PlaySoundEntry's. */
uint16_t QueueSound(uint16_t a)
{
    g.SoundLock[0]--;
    g.InstPtr = sound_entry((uint8_t)(a << 1));
    for (uint8_t y = 0; y < 8; y++) {
        if (r16(RAM_OFF_VoiceInst + (uint8_t)(y << 1)) != g.InstPtr || !g.VoiceState[y]) continue;
        g.SoundLock[0]++;
        return y;
    }
    g.CurTrack = 0x10;
    w8(RAM_OFF_CurTrackOfs, 0x20);
    w16(RAM_OFF_TrackInst + 0x20, g.InstPtr);
    if (!(g.sflags & sf2replay)) {
        if (!(g.recsfx & 0x8000)) {
            g.recsfx2 = g.recsfx;
            w16(RAM_OFF_recsfxpitch2, r16(RAM_OFF_recsfxpitch));
            w16(RAM_OFF_recsfxvol2, r16(RAM_OFF_recsfxvol));
        }
        g.recsfx = a;
    }
    return PlaySoundEntry();
}

/* Play sound a with the velocity vx / vy: the loudness from their high bytes, (x^2 + y^2) / 8
 * (at most $70) into SfxVelocity's high byte, the pitch the entry's times $C0-$FF (seed) / 256;
 * a voice QueueSound just started in state 2 takes the pitch, state 3. (QueueSound's whole
 * accumulator is the voice index: the pitch's high byte comes with a voice started.) Returns the
 * voice (Y), doubled when it was just started. */
uint16_t sfx(uint16_t a, uint16_t vx, uint16_t vy)
{
    g.InstPtr = sound_entry((uint16_t)(a << 1));
    uint16_t sx = (uint16_t)(int16_t)(int8_t)(vx >> 8), sy = (uint16_t)(int16_t)(int8_t)(vy >> 8);
    g.SfxVelocity = Mul8S(sx, sx);
    uint16_t v = (uint16_t)((uint16_t)(Mul8S(sy, sy) + g.SfxVelocity) >> 3);
    g.SfxVelocity = v >= 0x0070 ? 0x7000 : (uint16_t)(v << 8 | v >> 8);
    uint16_t pitch = (uint16_t)(Mul16x8U(lm16(inst_at(0x02) & 0xFFFFFF), (uint16_t)((g.seed[0] & 0x3F) + 0xC0)) >> 8);
    uint16_t y = QueueSound(a);
    g.SoundLock[0]--;
    if (r8(RAM_OFF_VoiceState + y) == 2) {
        w8(RAM_OFF_VoiceState + y, 3);
        y = (uint16_t)(y << 1);
        w16(RAM_OFF_VoicePitch + y, pitch);
        w16(RAM_OFF_recsfxpitch, pitch);
    }
    g.SoundLock[0]++;
    return y;
}

/* Play sound a with half the puck's velocity (Y as sfx leaves it) */
uint16_t PuckSfx(uint16_t a)
{
    return sfx(a, (uint16_t)(g.puckvx[0] >> 1), (uint16_t)(g.puckvy[0] >> 1));
}

/* Play the collision sound ($0F) with the doubled velocity of the faster of objects x and y, so
 * meant: the second's length is compared with its own shorter side (ApproxLength leaves it in r3
 * over the first length), so it is y's but for a length that wraps */
void CollisionSfx(int x, int y)
{
    uint32_t xv = RAM_OFF_Xvel, yv = RAM_OFF_Yvel;
    ApproxLength(r16(yv + 2 * x), r16(xv + 2 * x));
    uint16_t vx = r16(xv + 2 * y), vy = r16(yv + 2 * y);
    uint16_t l2 = ApproxLength(vx, vy);
    if (vx & 0x8000) vx = (uint16_t)-vx;
    if (vy & 0x8000) vy = (uint16_t)-vy;
    int o = l2 >= (vx < vy ? vx : vy) ? y : x;   /* (r3, where the first length was kept, is the second call's shorter side) */
    sfx(0x000F, (uint16_t)(r16(xv + 2 * o) << 1), (uint16_t)(r16(yv + 2 * o) << 1));
}

/* Each frame: the streamed sound effect streamsfx (but while the sound is locked or the menu set is
 * loaded; with no game clock and no song, cursong cleared). By streamstate: 0 its sample sent
 * (SPCStreamStart, the sound CPU's samples cut down to $14 first), or played at once when it is
 * loaded already (then 3); 1 streaming, its sound started (2); 2 streaming; 3 done */
static void stream_sfx(void)
{
    if (g.SoundLock[0]) return;
    if (g.sndbank[0] == SoundBankMenu) return;
    if (!g.gmclock && !g.MusicOn) g.cursong = 0;
    uint16_t s = g.streamsfx;
    if (!s) return;
    uint16_t st = g.streamstate;
    if (!st) {
        if (s == g.streamloaded) {
            g.SfxVelocity = 0x7000;
            QueueSound(rom16(SYM_StreamSfxSounds - 2 + (uint16_t)(s << 1)));
            g.streamstate = 3;
            return;
        }
        uint16_t fr;
        while (SPCStatus(&fr) >= 0x0015) SPCCmd04();
        g.streamloaded = s;
        uint32_t t = SYM_UpdateStreamSfx_7 - 4 + (uint32_t)(uint16_t)(s << 2);
        w16(RAM_OFF_StreamSrc, rom16(t));
        w16(RAM_OFF_StreamSrc + 2, rom16(t + 2));
        uint32_t src = (r16(RAM_OFF_StreamSrc) | (uint32_t)r16(RAM_OFF_StreamSrc + 2) << 16) & 0xFFFFFF;
        uint16_t loop = m16(src);
        w16(RAM_OFF_StreamLoop, (uint16_t)(loop << 8 | loop >> 8));
        uint16_t n = m16((src + 2) & 0xFFFFFF);
        n = (uint16_t)(n << 8 | n >> 8);
        w16(RAM_OFF_StreamSrc, (uint16_t)(r16(RAM_OFF_StreamSrc) + 4));
        SPCStreamStart(n, r16(RAM_OFF_StreamLoop));
        g.streamstate = 1;
        return;
    }
    if (st == 3) return;
    if (!SPCStreamNext()) { g.streamstate = 3; return; }
    if (g.streamstate != 1) return;
    g.streamstate = 2;
    g.SfxVelocity = 0x7000;
    QueueSound(rom16(SYM_StreamSfxSounds - 2 + (uint16_t)(s << 1)));
}
/* Each frame: the streamed sound effect's sample sent to the sound CPU a block at a time, then played
 * (stream_sfx) */
void UpdateStreamSfx(void)
{
    stream_sfx();
}


/* (a sample set upload: the screen black, sndbank set, the sample area emptied (command 1), the
 * player reset on the sound table, then the n bytes of 4-byte long pointers of the list sent) */
static void sample_upload(uint16_t bank, uint32_t table, uint32_t list, uint16_t n)
{
    forceblack();
    g.sndbank[0] = bank;
    g.SoundLock[0]--;
    SPCCmd01();
    g.SoundLock[0]++;
    g.SoundTable = table;
    SoundInit();
    for (uint16_t x = 0; x != n; x = (uint16_t)(x + 4))
        SPCUploadBlock((rom16(list + x) | (uint32_t)rom16(list + x + 2) << 16) & 0xFFFFFF);
}

/* Switch the sound driver to the in-game sample set, unless it is loaded */
void SPCGameUpload(void)
{
    if (g.sndbank[0] == SoundBankGame) return;
    sample_upload(SoundBankGame, SYM_GameSfxTable, SYM_GameSampleList, 0x0050);
}

/* The intro sample set (marked as the game set), unless the game set's mark is there; no team song */
void SPCIntroUpload(void)
{
    g.cursong = 0;
    if (g.sndbank[0] == SoundBankGame) return;
    sample_upload(SoundBankGame, SYM_IntroSfxTable, SYM_IntroSampleList, 0x0004);
}

/* The title sample set (the menu table, marked as the game set), unless the game set's mark is
 * there; no team song */
void SPCTitleUpload(void)
{
    g.cursong = 0;
    if (g.sndbank[0] == SoundBankGame) return;
    sample_upload(SoundBankGame, SYM_MenuSfxTable, SYM_TitleSampleList, 0x003C);
}

/* The menu sample set, unless it is loaded; no team song, no streamed sample loaded either way */
void SPCMenuUpload(void)
{
    g.cursong = 0;
    if (g.sndbank[0] != SoundBankMenu) sample_upload(SoundBankMenu, SYM_MenuSfxTable, SYM_MenuSampleList, 0x0040);
    g.streamloaded = 0;
}
