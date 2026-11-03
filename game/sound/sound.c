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
    SPCUnload();
    SPCUnload();
    SPCUnload();
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
        while (SPCStatus(&fr) >= 0x0015) SPCUnload();
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
    SPCReset();
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

/* Command $0E: the volume of voice x, a the left (APUIO2) and y the right (APUIO3) */
void SPCSetVolume(uint8_t a, uint8_t x, uint8_t y)
{
    spc_wait();
    io_w8(0x2143, y); io_w8(0x2142, a); io_w8(0x2141, x);
    spc_send(0x0E);
    io_w8(0x2140, 0);
}

/* Command $02: the sample count (the result, a byte) and the free sound RAM pointer (*x, a
 * word: APUIO1 and APUIO2) */
uint16_t SPCStatus(uint16_t *x)
{
    spc_wait();
    spc_send(0x02);
    uint8_t y = io_r8(0x2141);
    spc_send(0x80);
    *x = io_r16(0x2141);
    io_w8(0x2140, 0);
    return y;
}

/* Sequencer: command $0B, the pitch SendPitch of voice y (APUIO2 and APUIO3) */
void SeqCmd0B(uint8_t y)
{
    spc_wait();
    io_w8(0x2142, g.SendPitch[0]); io_w8(0x2143, g.SendPitch[1]);
    io_w8(0x2141, y);
    spc_send(0x0B);
    io_w8(0x2140, 0);
}

/* Sequencer: clear the command $08 values (after keying every voice off), then send command $08
 * (the volume and pitch modulation) and $82 (noise and echo voices) */
void SeqCmd08Init(void)
{
    SPCKeyOffAll();
    g.PitchModVoices = g.NoiseVoices = g.EchoVoices = g.EchoDelay = g.EchoVolume = g.EchoFeedback = 0;
    SeqCmd08Init_send();
}

/* Send command $08 (SoundVolume's high byte as both sides' main volume, PitchModVoices) and $82
 * (NoiseVoices, EchoVoices) as they stand: SeqCmd08Init's second half, entered too by SeqCmd08,
 * the volume fade and the events that change one of them */
void SeqCmd08Init_send(void)
{
    spc_wait();
    io_w8(0x2141, (uint8_t)(g.SoundVolume >> 8));
    io_w8(0x2142, (uint8_t)(g.SoundVolume >> 8));
    io_w8(0x2143, g.PitchModVoices);
    spc_send(0x08);
    io_w8(0x2141, g.NoiseVoices);
    io_w8(0x2142, g.EchoVoices);
    spc_send(0x82);
    io_w8(0x2140, 0);
}

/* Clear the track pointers and waits of tracks y up to x (8-bit counters: y = x goes all round) */
void ClearTracks(uint8_t y, uint8_t x)
{
    g.VarLenAcc[0] = x;
    uint8_t i = y, k = (uint8_t)(y << 1);
    do {
        w16(RAM_OFF_TrackInst + k, 0);
        w16(RAM_OFF_TrackWait + k, 0);
        k = (uint8_t)(k + 2);
        i++;
    } while (i != x);
}

/* Stop voice y (x = 2 * y): a streamed effect's stream state cleared, the voice free, owned by no
 * track, and a gain $9F sent */
void StopVoice(uint8_t y, uint8_t x)
{
    uint16_t inst = r16(RAM_OFF_VoiceInst + x);
    if (inst >= 0xE1C0 && inst < 0xE2B1) { g.streamstate = 0; g.streamsfx = 0; }
    w8(RAM_OFF_VoiceState + y, 0);
    w8(RAM_OFF_VoiceTrack + y, 0xFF);
    g.DSPGain = 0x9F;
    SeqCmd0C(y);
}

/* A variable length number (7 bits a byte, top bit = more) from the track at [EventPtr] (its low
 * word steps; the 16-bit reads take the byte after too, the 8-bit X only the byte) */
uint16_t ReadVarLen(void)
{
    uint32_t bank = (uint32_t)(g.EventPtr[1] & 0xFF) << 16;
    uint16_t v = m16(bank | g.EventPtr[0]);
    g.EventPtr[0]++;
    uint16_t a = v & 0x7F;
    while (v & 0x80) {
        w16(RAM_OFF_VarLenAcc, a);
        v = m16(bank | g.EventPtr[0]);
        g.EventPtr[0]++;
        g.VarLenByte = v & 0x7F;
        a = (uint16_t)(r16(RAM_OFF_VarLenAcc) << 7 | g.VarLenByte);
    }
    return a;
}

/* Command $07: voice x's volume, sample and ADSR, 5 bytes from [p0] in two sends ($07, $81) */
void SPCSetVoice(uint8_t x, uint32_t p0)
{
    spc_wait();
    io_w8(0x2141, x);
    io_w8(0x2142, m8(p0)); io_w8(0x2143, m8(p0 + 1));
    spc_ack(0x07);
    io_w8(0x2141, m8(p0 + 2)); io_w8(0x2142, m8(p0 + 3)); io_w8(0x2143, m8(p0 + 4));
    spc_ack(0x81);
    io_w8(0x2140, 0);
}

/* Command $0A: the 8 echo filter coefficients from [p0], in three sends ($0A, $84, $85) */
void SPCSetFIR(uint32_t p0)
{
    spc_wait();
    io_w8(0x2141, m8(p0)); io_w8(0x2142, m8(p0 + 1)); io_w8(0x2143, m8(p0 + 2));
    spc_ack(0x0A);
    io_w8(0x2141, m8(p0 + 3)); io_w8(0x2142, m8(p0 + 4)); io_w8(0x2143, m8(p0 + 5));
    spc_ack(0x84);
    io_w8(0x2141, m8(p0 + 6)); io_w8(0x2142, m8(p0 + 7));
    spc_ack(0x85);
    io_w8(0x2140, 0);
}

/* Sound driver command $11: x bytes of sound RAM from address y to [p0], a byte a handshake on
 * APUIO1 */
void SPCDownload(uint32_t p0, uint16_t x, uint16_t y)
{
    spc_wait();
    io_w16(0x2142, y);
    spc_ack(0x11);
    io_w16(0x2142, x);
    spc_ack(0x86);
    io_w8(0x2140, 0x87);
    io_w8(0x2141, 0);
    uint16_t k = 0;
    do {
        while (io_r8(0x2141) == (uint8_t)k) {}
        mset8(p0 + k, io_r8(0x2142));
        k++;
        io_w8(0x2141, (uint8_t)k);
    } while (--x);
    io_w8(0x2140, 0);
}

/* Release every voice (the sound locked meanwhile) */
void SoundVoicesOff(void)
{
    g.SoundLock[0]--;
    for (int y = 7; y >= 0; y--) StopVoice((uint8_t)y, (uint8_t)(y << 1));
    g.SoundLock[0]++;
}

/* Stop the song: release the voices of its tracks (below $10) and clear its tracks */
void MusicStop(void)
{
    g.SoundLock[0] = 0xFF;
    w8(RAM_OFF_MusicOn, 0);
    g.TrackCount = 0;
    for (int y = 7; y >= 0; y--) {
        if (g.VoiceTrack[y] >= 0x10 || !g.VoiceState[y]) continue;
        StopVoice((uint8_t)y, (uint8_t)(y << 1));
    }
    ClearTracks(0, 0x10);
    g.SoundLock[0] = 0;
}

/* Stop every voice that plays sound a (its entry through SoundTable; after the first stop the
 * comparisons are a byte: StopVoice returns with 8-bit registers) */
void StopSound(uint8_t a)
{
    g.SoundLock[0]--;
    uint32_t t = g.SoundTable & 0xFFFFFF;
    g.InstPtr = (uint16_t)(m16(t + (uint8_t)(a << 1)) + (uint16_t)t);
    int wide = 1;
    for (int y = 0; y < 8; y++) {
        uint16_t v = g.VoiceInst[y];
        if (wide ? v != g.InstPtr : (uint8_t)v != (uint8_t)g.InstPtr) continue;
        StopVoice((uint8_t)y, (uint8_t)(y << 1));
        wide = 0;
    }
    g.SoundLock[0]++;
}

/* Put voice y (x = 2 * y) into its release: state 4, its gain word from the instrument's byte
 * $15, and a release gain by the instrument's byte $11 (a voice in state 1 just stops) */
void ReleaseVoice(uint8_t y, uint8_t x)
{
    uint8_t st = r8(RAM_OFF_VoiceState + y);
    if (!st) return;
    if (st == 1) { StopVoice(y, x); return; }
    w8(RAM_OFF_VoiceState + y, 4);
    g.InstPtr = r16(RAM_OFF_VoiceInst + x);
    uint32_t bank = (uint32_t)g.SoundBank[0] << 16;
    w16(RAM_OFF_VoiceGain + x, (uint16_t)(((lm16(bank + g.InstPtr + 0x15) >> 1) & 0x7000) + 0x0FFF));
    uint8_t r = (uint8_t)(lm16(bank + g.InstPtr + 0x10) >> 8);
    g.DSPGain = r >= 0x6D ? 0x9F : r >= 0x36 ? 0x99 : r >= 0x03 ? 0x8C : 0x81;
    SeqCmd0C(y);
}

/* Voice y (x = 2 * y): count its length down; at 0 release it unless it is releasing already or
 * its track holds the sustain pedal (a negative length: no limit) */
void UpdateVoice(uint8_t y, uint8_t x)
{
    uint16_t len = r16(RAM_OFF_VoiceLength + x);
    if (len & 0x8000) return;
    if (len) { w16(RAM_OFF_VoiceLength + x, (uint16_t)(len - 1)); return; }
    if (r8(RAM_OFF_VoiceState + y) == 4) return;
    if (r8(RAM_OFF_TrackSustain + r8(RAM_OFF_VoiceTrack + y))) return;
    ReleaseVoice(y, x);
}

/* Silence everything and reset the player: every voice released, the sound RAM from SongUnused
 * on cleared, SoundVolume $5000, the tracks cleared, command $08 sent, TempoTick $12 */
void SoundInit(void)
{
    g.SoundLock[0] = 0xFF;
    SoundVoicesOff();
    w16(RAM_OFF_SoundBank, (uint16_t)(g.SoundTable >> 16));
    for (int x = 0x021A; x >= 0; x--) w8(RAM_OFF_SongUnused + x, 0);
    g.SoundVolume = 0x5000;
    ClearTracks(0, 0x11);
    SeqCmd08Init();
    g.TempoTick = 0x12;
    g.SoundLock[0] = 0;
    g.VoiceRotor[0] = 0;
}

/* Start the song at [p0]: the tempo from its second byte (DivX8U leaves the 16-bit accumulator, so
 * TempoTick's store takes the byte after it too, and EventPtr+2 the word at p0+2: p0 here is the
 * direct page's 4 bytes), the tracks from its offset list at +8, the track count from +6 */
void MusicStart(uint32_t p0)
{
    uint32_t s = p0 & 0xFFFFFF;
    MusicStop();
    g.SoundLock[0]--;
    for (int y = 0x10; y >= 0; y--) g.TrackUnused[y] = 0x1F;
    uint16_t x = 0x2760;
    if (io_r8(0x213F) & 0x10) x = 0x1FE0;
    w16(RAM_OFF_TempoTick, DivX8U(x, m8(s + 1)));
    g.EventPtr[1] = (uint16_t)(p0 >> 16);
    g.SongPtr = (uint16_t)p0;
    uint8_t k = 0, y = 8;
    for (;;) {
        uint16_t v = m16(s + y);
        if (!v) break;
        v = (uint16_t)(v + (uint16_t)p0);
        w16(RAM_OFF_TrackPtr + k, v);
        w16(RAM_OFF_TrackLoop + k, v);
        y = (uint8_t)(y + 2);
        k = (uint8_t)(k + 2);
    }
    g.SongUnused = 0;
    g.TrackCount = m8(s + 6);
    w8(RAM_OFF_MusicOn, 1);
    g.SoundLock[0]++;
}

/* Sequencer: command $07 (the voice's volume and the instrument's sample and ADSR, bytes $14-$16),
 * $06 (its pitch) and, unless ADSR is on (byte $15 bit 7), $0C (its start gain); Y = the voice */
uint8_t SeqCmd07(void)
{
    uint32_t inst = (uint32_t)g.SoundBank[0] << 16;
    spc_wait();
    io_w8(0x2141, g.CurVoice); io_w8(0x2142, g.VoiceVolL); io_w8(0x2143, g.VoiceVolR);
    spc_send(0x07);
    io_w8(0x2141, lm8(inst + g.InstPtr + 0x14));
    uint8_t adsr = lm8(inst + g.InstPtr + 0x15);
    io_w8(0x2142, adsr);
    io_w8(0x2143, lm8(inst + g.InstPtr + 0x16));
    spc_send(0x81);
    io_w8(0x2140, 0);
    spc_wait();
    io_w8(0x2141, g.CurVoice);
    io_w16(0x2142, r16(RAM_OFF_NotePitch));
    spc_send(0x06);
    io_w8(0x2140, 0);
    if (!(adsr & 0x80)) {
        spc_wait();
        io_w8(0x2142, r8(RAM_OFF_StartGain + 1));
        spc_send(0x0C);
        io_w8(0x2140, 0);
    }
    return g.CurVoice;
}

/* Sequencer: command $08 (volume, pitch modulation, noise and echo voices) and, with echo voices,
 * $09 (echo volume, feedback, buffer at $E0, delay) */
void SeqCmd08(void)
{
    SeqCmd08Init_send();
    if (!g.EchoVoices) return;
    spc_wait();
    io_w8(0x2141, g.EchoVolume); io_w8(0x2142, g.EchoVolume); io_w8(0x2143, g.EchoFeedback);
    spc_send(0x09);
    io_w8(0x2141, 0xE0); io_w8(0x2142, g.EchoDelay);
    spc_send(0x83);
    io_w8(0x2140, 0);
}

/* A voice for the sound at InstPtr (byte 0: the voices it may use, $2A: its priority): a free one
 * from after the last used (VoiceRotor), else the usable one of no higher priority with the
 * highest state, then age (the ages are read from voice 7 down whatever voice is tested; the 16-bit
 * best age takes InstVolL and VoiceVolL), which is cut (command $0C; $FF none) */
uint8_t FindFreeVoice(void)
{
    uint32_t inst = (uint32_t)g.SoundBank[0] << 16 | g.InstPtr;
    uint8_t x = (uint8_t)((g.VoiceRotor[0] + 1) & 7);
    g.VoiceRotor[0] = x;
    for (int n = 0; n < 8; n++) {
        if (!g.VoiceState[x] && (lm8(inst) & rom8(SYM_ByteBitMasks + x))) return x;
        x = (uint8_t)((x + 1) & 7);
    }
    g.CurVoice = 0xFF;
    g.InstVolL = g.VoiceVolL = g.InstVolR = 0;
    g.VoiceVolR = lm8(inst + 0x2A);
    g.StealCount[0] = 8;
    x = g.VoiceRotor[0];
    uint8_t y = 0x0E;
    do {
        if ((lm8(inst) & rom8(SYM_ByteBitMasks + x)) && g.VoiceVolR >= g.VoicePriority[x]) {
            uint8_t st = g.VoiceState[x];
            uint16_t age = r16(RAM_OFF_VoiceAge + y);
            if (st > g.InstVolR) {
                g.CurVoice = x;
                g.InstVolR = st;
                w16(RAM_OFF_InstVolL, age);
            } else if (st == g.InstVolR && age >= r16(RAM_OFF_InstVolL)) {
                g.CurVoice = x;
                w16(RAM_OFF_InstVolL, age);
            }
        }
        x = (uint8_t)((x + 1) & 7);
        y = (uint8_t)(y - 2);
    } while (--g.StealCount[0]);
    SeqCmd0C(g.CurVoice);
    return g.CurVoice;
}

/* Sound driver command $0F: start streaming a sample of x bytes from [StreamSrc] with loop point
 * y, sending its first block ($8A bytes, or $73 when STAT78's version bits are 0); a byte a
 * handshake on APUIO1. SPCStreamPtr is left past the block (its bank word from StreamSrc+2). */
void SPCStreamStart(uint16_t x, uint16_t y)
{
    uint16_t n = io_r16(0x213F) & 0x007E ? 0x008A : 0x0073;
    g.SPCStreamLeft[0] = (uint16_t)(x - n);
    spc_wait();
    io_w16(0x2142, y);
    io_w8(0x2140, 0x0F);
    io_w8(0x2141, 0);
    while (io_r8(0x2140) != 0x0F) {}
    uint16_t src = r16(RAM_OFF_StreamSrc), k = 0;
    uint32_t bank = (uint32_t)r8(RAM_OFF_StreamSrc + 2) << 16;
    do {
        io_w8(0x2142, m8((bank | src) + k));
        k++;
        io_w8(0x2141, (uint8_t)k);
        while (io_r8(0x2141) != (uint8_t)k) {}
    } while (--n);
    io_w8(0x2140, 0);
    w16(RAM_OFF_SPCStreamPtr, (uint16_t)(k + src));
    w16(RAM_OFF_SPCStreamPtr + 2, r16(RAM_OFF_StreamSrc + 2));
}

/* Sound driver command $10: the next block of the stream ($8A bytes, or $73 when STAT78 bit 4 is
 * clear; the rest when it is the last, 0 = 65536); the bytes still left */
uint16_t SPCStreamNext(void)
{
    w16(RAM_OFF_StreamSrc, r16(RAM_OFF_SPCStreamPtr));
    w16(RAM_OFF_StreamSrc + 2, r16(RAM_OFF_SPCStreamPtr + 2));
    uint16_t blk = io_r16(0x213F) & 0x0010 ? 0x008A : 0x0073, n;
    if (g.SPCStreamLeft[0] < blk + 1) { n = g.SPCStreamLeft[0]; g.SPCStreamLeft[0] = 0; }
    else { g.SPCStreamLeft[0] = (uint16_t)(g.SPCStreamLeft[0] - blk); n = blk; }
    spc_wait();
    io_w8(0x2140, 0x10);
    io_w8(0x2141, 0);
    while (io_r8(0x2140) != 0x10) {}
    uint16_t src = r16(RAM_OFF_StreamSrc), k = 0;
    uint32_t bank = (uint32_t)r8(RAM_OFF_StreamSrc + 2) << 16;
    do {
        io_w8(0x2142, m8((bank | src) + k));
        k++;
        while (io_r8(0x2141) != (uint8_t)k) {}
    } while (--n);
    io_w8(0x2140, 0);
    w16(RAM_OFF_SPCStreamPtr, (uint16_t)(k + src));
    return g.SPCStreamLeft[0];
}

/* Send the stream at [p0] through the sound CPU's boot loader: blocks of length, sound RAM address
 * and data, a length of 0 ending it with the start address (each block's kick value the last
 * index + 4, never 0) */
void SPCUpload(uint32_t p0)
{
    uint16_t y = 0;
    while (io_r16(0x2140) != 0xBBAA) {}
    uint8_t a = 0xCC;
    for (;;) {
        uint8_t kick = a;
        uint16_t len = m16(p0 + y); y += 2;
        uint16_t addr = m16(p0 + y); y += 2;
        io_w16(0x2142, addr);
        uint8_t more = len >= 1;
        io_w8(0x2141, more);
        io_w8(0x2140, kick);
        while (io_r8(0x2140) != kick) {}
        if (!more) break;
        uint8_t data = m8(p0 + y); y++;
        a = 0;
        uint16_t x = len;
        for (;;) {
            io_w8(0x2140, a);
            io_w8(0x2141, data);
            if (!--x) break;
            data = m8(p0 + y); y++;
            while (io_r8(0x2140) != a) {}
            a++;
        }
        while (io_r8(0x2140) != a) {}
        a = (uint8_t)(a + 4);
        if (!a) a = 4;
    }
    io_w8(0x2140, 0);
}

/* Read the next event of the track at CurTrackPtr: its delta (ReadVarLen, not kept), its code
 * into EventCode and its data into EventData1 / EventData2 by the code (a note: the key and its
 * length; $E7 skips a block) */
void ReadTrackEvent(void)
{
    uint32_t bank = (uint32_t)(g.EventPtr[1] & 0xFF) << 16;
    g.EventPtr[0] = g.CurTrackPtr;
    ReadVarLen();
    uint8_t c = m8(bank | g.EventPtr[0]);
    g.EventPtr[0]++;
    w8(RAM_OFF_EventCode, c);
    if (c < 0xD9) {
        w8(RAM_OFF_EventData1, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        w16(RAM_OFF_EventData2, ReadVarLen());
        return;
    }
    switch (c) {
    case 0xE7:
        g.EventPtr[0] = (uint16_t)(g.EventPtr[0] + m8(bank | g.EventPtr[0]) + 1);
        return;
    case 0xDC: case 0xEA: case 0xDD: case 0xDE: case 0xE2:
        w8(RAM_OFF_EventData1, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        return;
    case 0xDF:
        w8(RAM_OFF_EventData1, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        w8(RAM_OFF_EventData2, m8(bank | g.EventPtr[0]));
        g.EventPtr[0]++;
        return;
    case 0xE5:
        w16(RAM_OFF_EventData2, m16(bank | g.EventPtr[0]));
        g.EventPtr[0] = (uint16_t)(g.EventPtr[0] + 2);
        return;
    }
}

/* Update each voice in use that belongs to a music track (below $10), CurVoice and VoiceOfs the
 * voice */
void UpdateVoices(void)
{
    for (uint8_t y = 0; y < 8; y++) {
        uint8_t x = (uint8_t)(y << 1);
        g.CurVoice = y;
        w8(RAM_OFF_VoiceOfs, x);
        if (!g.VoiceState[y] || g.VoiceTrack[y] >= 0x10) continue;
        UpdateVoice(y, x);
    }
}

/* Play song slot a of the home team (TeamSongs), or a random DefaultSongs song when it has none,
 * unless a song plays; cursong = a + 1 */
void PlayTeamSong(uint16_t a)
{
    if (g.MusicOn) return;
    uint16_t h = g.HomeTeam;
    uint16_t x = (uint16_t)((uint16_t)(h << 1) + h + (h >> 15));
    x = (uint16_t)((uint16_t)((uint16_t)(x << 1) + a) << 2);
    uint16_t bank = rom16(SYM_TeamSongs + 2 + x), lo;
    if (!(bank & 0x8000)) lo = rom16(SYM_TeamSongs + x);
    else {
        uint16_t k = (uint16_t)(randoma8(8) << 2);
        lo = rom16(SYM_DefaultSongs + k);
        bank = rom16(SYM_DefaultSongs + 2 + k);
    }
    MusicStart(lo | (uint32_t)bank << 16);
    g.cursong = (uint16_t)(a + 1);
}

/* Track t: count its wait down, or play its events until one gives a wait (notes start voices,
 * $D9 and up are commands: $D9/$DA stop the song, $E2/$DB loop (from the pointer the track's
 * events started at), $DD tempo, $DE volume, $DF controllers, $E5 pitch bend (the pitches it sets
 * take the tuning of the entry at InstPtr, whichever it is), $DC program change); returns CurTrack */
uint8_t StepTrack(uint8_t t)
{
    g.CurTrack = t;
    uint8_t x = (uint8_t)(t << 1);
    w8(RAM_OFF_CurTrackOfs, x);
top:
    if (r16(RAM_OFF_TrackWait + x)) goto wait;
    g.CurTrackPtr = r16(RAM_OFF_TrackPtr + x);
event:
    ReadTrackEvent();
    g.CurTrackPtr = g.EventPtr[0];
    x = g.CurTrackOfs;
    uint8_t c = r8(RAM_OFF_EventCode), ct = g.CurTrack;
    if (c < 0xD9) {
        if (ct == 0x09) {
            g.InstPtr = sound_entry((uint8_t)((uint8_t)(c - 0x2F) << 1));
            w16(RAM_OFF_TrackInst + 0x12, g.InstPtr);
            g.TrackVolL[9] = ip8(0x12);
            g.TrackVolR[9] = ip8(0x13);
            g.TrackBendRange[9] = ip8(0x2F);
            g.TrackBend[9] = 0;
        }
        StartSoundVoice((uint8_t)(g.CurTrack << 1));
    } else if (c == 0xD9 || c == 0xDA) {
        MusicStop();
        return g.CurTrack;
    } else if (c == 0xE2) {
        w16(RAM_OFF_TrackLoop + x, r16(RAM_OFF_TrackPtr + x));
    } else if (c == 0xDB) {
        w16(RAM_OFF_TrackPtr + x, r16(RAM_OFF_TrackLoop + x));
        goto top;
    } else if (c == 0xDD) {
        g.TempoTick = r8(RAM_OFF_EventData1);
    } else if (c == 0xDE || (c == 0xDF && r8(RAM_OFF_EventData1) == 0x07)) {
        if (c == 0xDF) w16(RAM_OFF_EventData1, g.EventData2);
        g.InstPtr = r16(RAM_OFF_TrackInst + x);
        if (!g.SoundMono[0]) { g.InstVolL = ip8(0x12); g.InstVolR = ip8(0x13); }
        else g.InstVolL = g.InstVolR = ip8(0x17);
        uint8_t e = r8(RAM_OFF_EventData1);
        g.TrackVolL[ct] = (uint8_t)MulFrac8((uint16_t)(g.InstVolL << 8 | e));
        g.TrackVolR[ct] = (uint8_t)MulFrac8((uint16_t)(g.InstVolR << 8 | e));
        for (int y = 7; y >= 0; y--) {
            if (g.VoiceTrack[y] != ct) continue;
            uint8_t vel = g.VoiceVelocity[y];
            g.VoiceVolL = (uint8_t)MulFrac8((uint16_t)(g.TrackVolL[ct] << 8 | vel));
            g.VoiceVolR = (uint8_t)MulFrac8((uint16_t)(g.TrackVolR[ct] << 8 | vel));
            spc_wait();
            io_w8(0x2143, g.VoiceVolR);
            io_w8(0x2142, g.VoiceVolL);
            io_w8(0x2141, (uint8_t)y);
            spc_send(0x0E);
            io_w8(0x2140, 0);
        }
    } else if (c == 0xDF) {
        uint8_t k = r8(RAM_OFF_EventData1), v = (uint8_t)g.EventData2;
        switch (k) {
        case 0x40:
            g.TrackSustain[ct] = v;
            if (v) break;
            for (int y = 7; y >= 0; y--)
                if (g.VoiceTrack[y] == ct && g.VoiceState[y]) ReleaseVoice((uint8_t)y, (uint8_t)(y << 1));
            break;
        case 0x0B: g.TrackVolL[ct] = v; break;
        case 0x0C: g.TrackVolR[ct] = v; break;
        case 0x32: g.EchoVoices = v; SeqCmd08(); break;
        case 0x33: g.EchoDelay = v; SeqCmd08(); break;
        case 0x34: g.EchoVolume = v; SeqCmd08(); break;
        case 0x35: g.EchoFeedback = v; SeqCmd08(); break;
        case 0x37: w8(RAM_OFF_SoundVolume + 1, v); SeqCmd08Init_send(); break;
        case 0x39: g.NoiseVoices = v; SeqCmd08Init_send(); break;
        }
    } else if (c == 0xE5) {
        uint16_t d = g.EventData2;
        d = (uint16_t)((d & 0xFF00) | (uint8_t)(d << 1));
        uint8_t bend = (uint8_t)((uint16_t)((uint16_t)(d - 0x4000) << 1) >> 8);
        g.TrackBend[ct] = bend;
        g.BendAmount[0] = bend;
        uint8_t range = g.TrackBendRange[ct];
        if (range) {
            w8(RAM_OFF_EventCode, range);
            for (int y = 7; y >= 0; y--) {
                if (g.VoiceTrack[y] != ct) continue;
                uint8_t vx = (uint8_t)(y << 1);
                uint16_t base = r16(RAM_OFF_VoiceBasePitch + vx), p;
                if (!bend) p = base;
                else if (!(bend & 0x80)) {
                    uint16_t e = (uint16_t)(rom16(SYM_PitchTable + (uint8_t)((uint8_t)(g.VoiceNote[y] + range) << 1)) - base);
                    w16(RAM_OFF_EventData2, e);
                    p = (uint16_t)(MulFrac16(e, bend) + base);
                } else {
                    uint16_t lo = rom16(SYM_PitchTable + (uint8_t)((uint8_t)(g.VoiceNote[y] - range) << 1));
                    w16(RAM_OFF_EventData1, lo);
                    uint16_t e = (uint16_t)(base - lo);
                    w16(RAM_OFF_EventData2, e);
                    uint8_t nb = (uint8_t)(0 - bend);
                    if (nb & 0x80) w16(RAM_OFF_MulFracResult, e);
                    else MulFrac16(e, nb);
                    p = (uint16_t)(base - r16(RAM_OFF_MulFracResult));   /* (the zero multiply leaves $43 as it was) */
                }
                w16(RAM_OFF_EventData1, p);
                w16(RAM_OFF_VoicePitch + vx, (uint16_t)(ip16(0x02) + p));
            }
        }
    } else if (c == 0xDC) {
        uint16_t n = r8(RAM_OFF_EventData1);
        if (n == 0x6A) n = 0x14;
        g.InstPtr = sound_entry((uint8_t)(n << 1));
        w16(RAM_OFF_TrackInst + x, g.InstPtr);
        g.TrackBendRange[ct] = ip8(0x2F);
        g.TrackBend[ct] = 0;
        uint8_t r;
        if (!g.SoundMono[0]) { g.TrackVolL[ct] = ip8(0x12); r = ip8(0x13); }
        else g.TrackVolL[ct] = r = ip8(0x17);
        g.TrackVolR[ct] = r;
    } else if (c == 0xEA) {
        g.EventEAUnused = r8(RAM_OFF_EventData1);
    }
    uint16_t delta = ReadVarLen();
    x = g.CurTrackOfs;
    if (!delta) goto event;
    w16(RAM_OFF_TrackWait + x, delta);
    w16(RAM_OFF_TrackPtr + x, g.CurTrackPtr);
wait:
    w16(RAM_OFF_TrackWait + x, (uint16_t)(r16(RAM_OFF_TrackWait + x) - 1));
    return g.CurTrack;
}

/* The tempo: TempoAcc + $40, and while it reaches TempoTick (less it each time) a tick: the
 * music voices updated and, unless the music is paused, every track stepped (from the last; none
 * when the song stops, and no more ticks without tracks) */
void SequencerTick(void)
{
    uint8_t a = (uint8_t)(g.TempoAcc + 0x40);
    for (;;) {
        g.TempoAcc = a;
        if (a < g.TempoTick) return;
        UpdateVoices();
        if (!g.MusicPause[0]) {
            uint8_t x = g.TrackCount;
            if (!x) return;
            x--;
            do {
                x = StepTrack(x);
                if (!(uint8_t)g.MusicOn) return;
                x--;
            } while (!(x & 0x80));
        }
        a = (uint8_t)(g.TempoAcc - g.TempoTick);
    }
}

/* Each frame, unless the sound is locked: the fade (SoundVolume less SoundFadeSpeed, the sound
 * reset below 0, else the volume sent), the sound effect voices and the song's ticks (the pause's
 * mute of the volume is undone at once) */
void SoundUpdate(void)
{
    if (g.SoundLock[0]) return;
    if (g.SoundFadeSpeed) {
        uint16_t v = g.SoundVolume;
        g.SoundVolume = (uint16_t)(v - g.SoundFadeSpeed);
        if (v < g.SoundFadeSpeed) SoundInit();
        else SeqCmd08Init_send();
    }
    if (g.MusicPause[0]) {
        uint8_t h = r8(RAM_OFF_SoundVolume + 1);
        w8(RAM_OFF_SoundVolume + 1, 0);
        w8(RAM_OFF_SoundVolume + 1, h);
    }
    UpdateSfxVoices();
    if ((uint8_t)g.MusicOn) SequencerTick();
}

/* SPCSendSample of the sample at [p0]: its loop point and length (big-endian words), then the BRR
 * blocks from +4 (the sound locked meanwhile) */
void SPCUploadBlock(uint32_t p0)
{
    g.SoundLock[0]--;
    uint16_t lp = m16(p0), len = m16(p0 + 2);
    SPCSendSample((p0 & 0xFF0000) | (uint16_t)((uint16_t)p0 + 4), (uint16_t)(len << 8 | len >> 8), (uint16_t)(lp << 8 | lp >> 8));
    g.SoundLock[0]++;
}
