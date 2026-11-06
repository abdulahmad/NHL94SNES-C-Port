/* Generated (ninety-four-snes's tools/gen_ram.py) from the disassembly's RAM map (ram94s.asm) and
 * the widths the routines access each symbol at. The SNES work RAM as one struct, each symbol at its WRAM
 * offset ($0000-$1FFF is also every bank's low 8 KB, where the game reaches most of it). The
 * 65816 is little-endian like the host: g's bytes are the machine's bytes (mem.h). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define WRAM_SIZE 0x20000
#define RAM_STACK_LO 0x1EC3   /* the stack: from here to $1FFF (Reset sets S = $1FFF) */

#pragma pack(push, 1)
struct G {
    uint16_t r0[2];
    uint16_t r1[2];
    uint16_t r2[2];
    uint32_t p0; /* long pointer; p0+2 is the bank */
    uint32_t p1; /* long pointer; p1+2 is the bank */
    uint16_t r3[2];
    uint16_t r4[2];
    uint16_t r5[2];
    uint16_t r6[2];
    uint16_t r7[2];
    uint8_t r8[4];
    uint16_t r9[2];
    uint16_t ClipTopUnused; /* ResetPPURegs is the only access: sta.b $30 = $FFF0 (with $2C = $FFF0, $2E = $0100, $32 = $00E0: ... */
    uint16_t ClipBottomUnused; /* ResetPPURegs sta.b $32 = $00E0 (224, screen height); never read anywhere */
    uint32_t SoundTable; /* Long pointer to the sound/instrument offset table; $36 = SoundTable+2 (bank, stored as a word so ... */
    uint16_t NMISkip; /* VBlank: lda.b $38 : beq -> nonzero = just pla/plb/rti (no hook, DMA, OAM, sound) */
    uint16_t DMAQueueHead; /* Read index into the DMAQueue ring ($0100, 8-byte entries, and #$03FF): NMI_HookReturn ldx $3A ... */
    uint16_t DMAQueueTail; /* Write index of the DMAQueue ring: QueueVRAMDMA, DMAToVRAMHigh, FillVRAM, FillVRAMSplit, ... */
    uint16_t MulCand[2]; /* Multiplicand of the software multiply (taken when Mode7InUse bit 6 is set): MulFrac8 $3E = A low ... */
    uint8_t StreamLoop; /* UpdateStreamSfx: $42 = byte-swapped first word of the stream block header, then ldy $42 -> Y = ... */
    uint8_t MulFracResult[3]; /* MulFrac16 returns A*Y/128 in A and $43 (soft path: lda $43 = MulProd+1; hardware path: MPYM*2 -> ... */
    uint16_t MulPlier[2]; /* MulFrac16: sty $46 (multiplier Y), lda $46 : lsr : sta $46 each step, the carry adds MulCand ... */
    uint8_t CurTrack; /* Track number being worked: StepTrack txa : sta $4A (restores X from it at the end); ... */
    uint8_t CurVoice; /* Voice number being worked: FindFreeVoice returns it ($FF = none, stx $4B; the steal scan keeps ... */
    uint16_t CurTrackPtr; /* StepTrack: lda TrackPtr,x : sta $4C; after each ReadTrackEvent copies EventPtr ($56) back to ... */
    uint16_t InstPtr; /* Sound/instrument entry pointer (SoundTable + offset) of the sound being started/stopped/updated: ... */
    uint8_t SoundBank[2]; /* Bank of the sound entries: SoundInit ldx $36 : stx $50 (SoundTable bank, 16-bit store) */
    uint8_t VarLenAcc[2]; /* ReadVarLen: sta $52 = value so far, then ($52 << 7) or $54 for each continuation byte (top bit ... */
    uint16_t VarLenByte; /* ReadVarLen: the next byte and #$7F, ORed into the shifted VarLenAcc */
    uint16_t EventPtr[2]; /* Long read pointer into the song ($58 = EventPtr+2 = song bank: MusicStart lda $0E : sta $58, 8-bit) */
    uint8_t GainWord; /* UpdateSfxVoices: lda VoiceGain,x : sta $5A so that $5B (its high byte) is the DSP GAIN ... */
    uint8_t DSPGain; /* GAIN byte sent by SeqCmd0C / SeqCmd0C0B (APUIO2 with command $0C, APUIO1 = voice): ReleaseVoice ... */
    uint8_t BendAmount[2]; /* StepTrack event $E5 (pitch bend): signed bend byte, sta TrackBend,x and $5C; its sign selects ... */
    uint16_t EventData2; /* ReadTrackEvent: second data item: note length (ReadVarLen) for notes, controller value ($DF: ... */
    uint8_t SendPitch[2]; /* UpdateSfxVoices: VoicePitch,x (+ VoiceVibOfs while the vibrato runs) -> $60; SeqCmd0B sends $60 ... */
    uint16_t EventEAUnused; /* StepTrack event $EA: EventData1 and #$FF -> $62 (word store); never read anywhere */
    uint16_t SfxVelocity; /* Velocity of the next sound effect, in the high byte ($65 = SfxVelocity+1): PlaySound lda $65 : ... */
    uint8_t VoiceRotor[2]; /* FindFreeVoice: lda $66 : inc : and #$07 : sta $66, the search starts at that voice (round ... */
    uint8_t StealCount[2]; /* FindFreeVoice steal scan (no free voice): lda #$08 : sta $68 ... dec $68 : bne, the voices left ... */
    uint8_t HuffBitsExt; /* Low word of the 32-bit bit window $6A-$6D under HuffBits */
    uint8_t LZCopySrc; /* Long pointer of a back-reference copy: $6B/$6C = LZOutAddr - LZDistance - 1 (lda $6F / clc / sbc ... */
    uint8_t HuffBits[3]; /* 16-bit bit buffer, MSB first: header loads it with the 2 bytes at header+6 (xba / sta $6C, Y = 8 ... */
    uint16_t LZOutAddr; /* Output address: starts as the destination word $10 (lda $10 / sta $6F) and every literal and ... */
    uint8_t LZCmd[2]; /* Command byte of each LZ token (lda [$0C],y / sta $71): bit 7 / bit 6 / bit 5 choose the 2-, 3-, ... */
    uint8_t LZCmdByte2; /* Second byte of a 2- or 3-byte LZ token: offset low byte of the short form (offset = (cmd and ... */
    uint8_t HuffEscapeLen; /* Code length of the escape symbol: set while building the tables when the symbol equals ... */
    uint16_t LZCmdByte3; /* Third byte of the 3-byte LZ token (lda [$0C],y / sta $75): offset low byte, offset = (($73 and ... */
    uint16_t LZLenLo; /* Fourth byte of the 4-byte LZ token (lda [$0C],y / sta $77): low byte of the copy length, length ... */
    uint16_t LZSrcIdx; /* Source index Y saved over each back-reference copy (sty $79 / ldy #0 / copy [$6B],y / ldy $79) */
    uint8_t LZDistance[2]; /* Back-reference distance: (cmd and $60)*8 + byte 2, or ((byte 2 and $3F) << 8) + byte 3, or the ... */
    uint8_t HuffSpan[4]; /* Table entries per code of the current length, plus 1 (lda .4,x / sta $7D; ldy $7D / dey / bne ... */
    uint16_t crowdhold; /* frames the crowd level holds before it fades (Goal 120, puckglue 20; updatecrowdf counts it down) */
    uint16_t vcount[2]; /* 32-bit frame counter, +1 per NMI (NMI_Tick; 93S vcount) */
    uint16_t xsave; /* X saved around the push/pop loops */
    uint32_t a0; /* long pointer; a0+2 is the bank */
    uint32_t a1; /* long pointer; a1+2 is the bank */
    uint16_t a2[2]; /* team index, 0 = home */
    uint32_t a3; /* long pointer; a3+2 is the bank */
    uint32_t a4; /* long pointer; a4+2 is the bank */
    uint16_t a5[4]; /* long pointer (the next of the 4-byte a registers); a5+2 is the bank */
    uint16_t d0[2];
    uint16_t d1[2]; /* d1+2 is the high word */
    uint16_t d2[2];
    uint16_t d3[2];
    uint16_t d4[2];
    uint16_t d5[2];
    uint16_t d6[2];
    uint16_t d7; /* frames elapsed, added into speeds */
    uint16_t introHookFlags; /* Jobs pending for the intro frame hook (.framehook), set with tsb and cleared there with trb: ... */
    uint16_t tmp0; /* scratch word; tmp0+1 is the next byte */
    uint16_t tmp1; /* scratch word */
    uint16_t tmp2; /* scratch byte; tmp2+1 is the next byte */
    uint16_t tmp3; /* scratch word (after tmp0-tmp2) */
    uint16_t tmp4; /* scratch word */
    uint16_t tmp5; /* scratch word */
    uint32_t introPal2Src; /* Long pointer ($D1 address word, $D3 bank) of the second palette the intro frame hook sends when ... */
    uint8_t introBG1H[2]; /* BG1 horizontal scroll of the intro: .3 sets $0180 (also into BG1HScroll) and tsb $0010; the ... */
    uint16_t cornerdy; /* checkwallcoll corner circle: |d3 - d5|, the y distance from the corner center (93S mulb at .dy); ... */
    uint16_t introM7A; /* Mode 7 matrix A (x scale) of the intro: .23 sets $0100, .24 sets $0001, the zoom step .6 adds ... */
    uint16_t introM7D; /* Mode 7 matrix D (y scale) of the intro, always stored with the same value as introM7A (sta $D9 / ... */
    uint8_t mulres[7]; /* SuperMult (passbits *= d0): passbits is copied here and cleared (sta $DD,x for x = $0E..0), then ... */
    uint16_t curobj; /* object index of the object being updated (updateplayers, CalcPuckDistances store X; the assignments reload it) */
    uint16_t PlayersUnused[2]; /* never read: updateplayers sets $FFFF for every object at the top of the per-object loop (lda #$FFFF / sta ... */
    uint16_t curobjteam; /* never read. Team of the object being updated, 0 = home, 2 = away: updateplayers sets 0 before the loop and 2 ... */
    uint16_t curobjopp[10]; /* never read. The other team of the object being updated (2 for the home objects, 0 from X = $0C on), set ... */
    uint16_t DMAQueue[512]; /* Ring of 128 8-byte VRAM transfer entries: ... */
    uint8_t DecompTab1[256]; /* DecompressPairs: first byte of each pair code (sta DecompTab1,x, read in the expand loop); DecompressHuffman: ... */
    uint8_t DecompTab2[256]; /* DecompressPairs: second byte of each pair code (sta DecompTab2,x); DecompressHuffman: code-length jump index of ... */
    uint8_t HuffCount[32]; /* DecompressHuffman header loop: sta HuffCount,x = number of codes of each length (x = 2*length, 16 words), ... */
    uint16_t HuffOffset[16]; /* DecompressHuffman: sta HuffOffset,x = first code minus symbols so far, per length; long-code decode does sbc ... */
    uint16_t HuffLimit[16]; /* DecompressHuffman: sta HuffLimit,x = left-justified code limit per length; long-code decode compares against ... */
    uint16_t HuffLongVec; /* DecompressHuffman: sta HuffLongVec = compare-chain entry chosen by the longest code length; jmp (HuffLongVec) for ... */
    uint8_t IniDispShadow[2]; /* INIDISP value the NMI writes (forced blank, brightness) */
    uint16_t FrameCount; /* NMI_SendOAM inc FrameCount each NMI (unless FrameCountHold); WaitFrame waits for it to change; Reset_Fast clears it; ... */
    uint8_t OAMReady[2]; /* nonzero: the NMI copies the OAM buffer at OAMShowBuf to OAM */
    uint16_t CGDMALen; /* DMAToCGRAM (screen on) stores colours*2 at CGDMALen; NMI_HookReturn DMAs that many bytes to CGRAM when ... */
    uint8_t CGDMAColour[2]; /* DMAToCGRAM sty CGDMAColour (first colour); NMI_HookReturn ldx CGDMAColour -> CGADD */
    uint16_t CGDMASrc; /* DMAToCGRAM sta CGDMASrc ($0C); NMI_HookReturn -> A1T0L */
    uint8_t CGDMABank[2]; /* DMAToCGRAM sta CGDMABank ($0E); NMI_HookReturn ldx CGDMABank -> A1B0 */
    uint8_t FrameHookNext[3]; /* SetFrameHook sta FrameHookNext / stx $0772 (bank); NMI_SendOAM: when $0771 (address high byte) is nonzero copies ... */
    uint8_t NMIHookNext[3]; /* ClearNMIHook sta NMIHookNext / stx $0775 (bank); VBlank: when $0774 is nonzero copies it to NMIHook ... */
    uint16_t JoyPad[5]; uint8_t JoyPad_end_; /* buttons held, a word per pad (5 pads) */
    uint8_t Multi5Sig[6]; /* Multi5ConnectCheck rol.w JOYSER0/1 bit 1 into $0782/Multi5Sig (port 1) and $0784/$0783 (port 2); $FF then ... */
    uint16_t FrameCountHold; /* NMI_SendOAM: lda FrameCountHold bne skips inc FrameCount; no writer in routines apart from the Reset_Fast clear */
    uint8_t FrameHook[3]; /* NMI_SendOAM jml [FrameHook] (returns to NMI_FrameHookReturn); loaded from FrameHookNext; Reset_Fast copies ... */
    uint8_t NMIHook[3]; /* VBlank jml [NMIHook] (returns to NMI_HookReturn); loaded from NMIHookNext */
    uint8_t IRQHook[3]; /* IRQ_Fast jml [IRQHook]; SetIRQ sta IRQHook / stx $0791 (bank = IRQHook+2); IRQOff sets IRQHook_Off; the IRQ_* ... */
    uint16_t seed[2]; /* RandomWord's two 16-bit generators (seed, $0794); FallDown/chk4shot/asspuckc/chk4lc read seed where 93S ... */
    uint8_t RAMTag[6]; /* Reset_Fast copies InitTag 'EACDSI' here (and to RAMTag2 and ReplayBufferEnd); no reader found - a RAM marker */
    uint16_t MapVRAM; /* VRAM word address of the screen map: MapToVRAM/MapPartToVRAM/MapPartToVRAMOffset/Mode7MapToVRAM add row*32 ... */
    uint16_t MapScreenStep; /* MapToVRAM/MapPartToVRAM add MapScreenStep to the VRAM address after row 31 (to the next 32-row screen); no writer ... */
    uint16_t HeapStart7E; /* first block header of the bank $7E heap */
    uint16_t HeapStart7F; /* first block header of the bank $7F heap */
    uint16_t NewHandleSlot; /* NewHandle: the free handle being filled */
    uint16_t NewHandleSkipBank; /* NewHandle: the bank it must not use */
    uint16_t OAMLastEnd; /* OAMNext of the frame last finished */
    uint16_t OAMDrawBuf; /* OAM buffer (bank $7E) the sprites are drawn into */
    uint16_t OAMShowBuf; /* OAM buffer (bank $7E) the NMI copies */
    uint16_t SpritesLeft; /* OAM entries still free this frame */
    uint16_t OAMHiPtr; /* byte of the OAM high table that takes the next entry's size and X high bits */
    uint16_t OAMHiShift; /* position of the next entry in that byte, 3 down to 0 */
    uint16_t SpriteVRAM; /* VRAM word address of sprite tile 0 */
    uint16_t SpritesReserved; /* OAM entries at the start that the sprite drawing leaves alone */
    uint16_t SpriteBlockTiles; /* InitSprites: (Y+$1F)&~$1F (at least $1A0) = tiles of the 16x16 block area of sprite VRAM; LoadFrameTiles ... */
    uint16_t FrameCacheCount; /* Tile cache entries in use: InitSprites stz, LoadFrameTiles inc (max $80), FreeCacheEntry dec; ... */
    uint16_t OAMNext; /* next OAM entry to fill in OAMDrawBuf */
    uint16_t OAMHideEnd; /* SpriteFrameEnd moves the entries from OAMNext up to here off screen */
    uint16_t FrameTilesFail; /* GetFrameTiles stores $FFFF here when LoadFrameTiles finds no room; never read */
    uint16_t CacheUseMark; /* InitSprites sets 2; GetFrameTiles stores it in the entry's FrameCacheUse word, which SpriteFrameStart shifts ... */
    uint16_t DrawFrameNum; /* DrawSprite/FreeFrameTiles/GetFrameTiles store the frame number here; with FrameByNumber set the cache ... */
    uint16_t FrameByNumber; /* Nonzero ($FFFF from StartGame/ExitStatsScreen): GetFrameTiles/FreeFrameTiles/DrawSpritePieces find the entry by ... */
    uint16_t PieceX; /* DrawSprite/DrawSpritePieces: x offset of the current frame piece (with $1C); ... */
    uint16_t PieceY; /* DrawSprite/DrawSpritePieces: y offset of the piece (from $1E); LoadFrameTiles reuses it for the 8x8 tile ... */
    uint16_t PieceTile; /* DrawSpritePieces: piece tile/attribute word, + first tile $16, eor $14 -> OAM +2 */
    uint8_t PieceSize[2]; /* DrawSpritePieces: piece size byte (0 = 8x8, else 16x16: centre 15 not 7, size bit in the OAM high table) */
    uint16_t PieceNext; /* DrawSprite/DrawSpritePieces sty PieceNext: offset of the next piece record of the frame */
    uint16_t IRQVTime; /* SetIRQ sty IRQVTime (IRQ line); NMI_HookReturn writes it to VTIMEL every frame */
    uint16_t IRQHTime; /* NMI_HookReturn writes it to HTIMEL every frame; set by SetupBandIRQ/SetupScrollIRQ/TitleScreen */
    uint16_t BG1HScroll; /* NMI_HookReturn writes the 6 words BG1HScroll..$07E1 to BG1HOFS..BG3VOFS (x = 0..$0B, $07D7 = BG1HScroll+1); ... */
    uint16_t BG1VScroll; /* 2nd word of the NMI_HookReturn scroll list (BG1VOFS); ResetPPURegs sets $FFFF */
    uint16_t BG2HScroll; /* updatescroll_rows stores Hscroll+$C0 here, as 93S writes BG2HOFS = Hscroll+$C0 */
    uint16_t BG2VScroll; /* updatescroll_rows stores vramrow*8 + (Vscroll&7), the 93S BG2VOFS value */
    uint8_t BG3HScroll[2]; /* 5th word of the NMI_HookReturn scroll list (BG3HOFS); ResetPPURegs/PlayoffScreen/TitleScreen */
    uint16_t BG3VScroll; /* 6th word (BG3VOFS); IRQ_BG1Scroll also writes BG3VScroll/$07E1 to BG1VOFS for the lower band */
    uint8_t Mode7InUse; /* TitleScreen/IntroSequence set $FF, ResetPPURegs clears; MulFrac8 bit Mode7InUse / MulFrac16 bit $07E1 (16-bit: ... */
    uint8_t SoundMono[2]; /* StepTrack (event $DE, program change) and PlaySound: nonzero -> both side volumes from instrument byte $17 ... */
    uint8_t SoundLock[2]; /* nonzero: SoundUpdate does nothing (decremented around sound work) */
    uint8_t EchoVoices; /* Controller $32 (StepTrack); SeqCmd08 sends it with command $82, which the driver ($0631) writes to DSP EON ... */
    uint8_t NoiseVoices; /* Controller $39; SeqCmd08 command $82 APUIO1 -> driver writes DSP NON ($3D) */
    uint8_t PitchModVoices; /* SeqCmd08 command $08 APUIO3 -> driver writes DSP PMON ($2D) (and ORs it into FLG); only ever cleared ... */
    uint8_t EchoDelay; /* Controller $33; SeqCmd08 command $83 APUIO2 -> driver writes DSP EDL ($7D) (ESA = $E0) */
    uint8_t EchoVolume; /* Controller $34; SeqCmd08 command $09 APUIO1/2 -> DSP EVOL L/R ($2C/$3C) */
    uint8_t EchoFeedback; /* Controller $35; SeqCmd08 command $09 APUIO3 -> DSP EFB ($0D) */
    uint16_t SongPtr; /* MusicStart sta SongPtr (song address $0C); Opening/PlayoffScreen compare it with the menu/playoff song ... */
    uint16_t SongUnused; /* MusicStart stz SongUnused; first byte of the sound RAM SoundInit clears (SongUnused..$0A09); never read */
    uint8_t MusicPause[2]; /* SequencerTick: nonzero -> UpdateVoices but no StepTrack; SoundUpdate tests it around a no-op save/restore ... */
    uint16_t MusicOn; /* nonzero while a song plays */
    uint8_t TempoAcc; /* SequencerTick adds $40 each frame and runs a tick (subtracting TempoTick) while it reaches TempoTick */
    uint8_t TempoTick; /* SequencerTick tick threshold; MusicStart sets it from the song header (DivX8U), StepTrack event $DD sets ... */
    uint16_t SoundVolume; /* master volume; its high byte goes to the sound driver */
    uint16_t SoundFadeSpeed; /* subtracted from SoundVolume each frame; at 0 the sound is reset */
    uint8_t InstVolL; /* StepTrack event $DE: instrument byte $12 (left, or $17), scaled by the event value into TrackVolL; ... */
    uint8_t VoiceVolL; /* Left volume sent to the driver: SeqCmd07 APUIO2 (DSP VOL L), StepTrack command $0E; StartSoundVoice = ... */
    uint8_t InstVolR; /* StepTrack event $DE: instrument byte $13 (right, or $17) */
    uint8_t VoiceVolR; /* Right volume: SeqCmd07 APUIO3 (DSP VOL R), StepTrack command $0E; FindFreeVoice reuses it for the new ... */
    uint8_t CurTrackOfs; /* StepTrack stx CurTrackOfs (track*2) and reloads X from it; PlaySound/QueueSound set $20 (track 16, sound effects) */
    uint8_t VoiceOfs; /* Voice*2 of the voice being worked: StartSoundVoice asl/sta VoiceOfs, ldx $0800; UpdateVoices/UpdateSfxVoices ... */
    uint8_t TrackCount; /* MusicStart: song header byte 6; SequencerTick steps tracks TrackCount-1 down to 0; MusicStop clears it */
    uint8_t MusicDuck[2]; /* StartSoundVoice: nonzero halves (negative: quarters) the volumes of music voices (track != $10); no writer ... */
    uint16_t TrackPtr[17]; /* 17 words (tracks 0-15 music, 16 = sound effects): MusicStart stores each track start; StepTrack reads ... */
    uint16_t TrackLoop[17]; /* MusicStart = track start; StepTrack event $E2 copies TrackPtr here, event $DB copies it back */
    uint16_t TrackWait[17]; /* StepTrack: ticks to the track's next event (ReadVarLen delta, dec each tick); ClearTracks zeroes it */
    uint16_t TrackInst[17]; /* Instrument/sound entry pointer of the track: event $DC (program change) from the [$34] table, ... */
    uint8_t TrackBend[17]; /* StepTrack event $E5 (pitch bend) stores the bend; StartSoundVoice/StepTrack bend the voice pitch by it */
    uint8_t TrackBendRange[17]; /* Instrument byte $2F (StepTrack program change, PlaySound); notes the full bend spans (StartSoundVoice adds ... */
    uint8_t TrackVolL[17]; /* Track left volume: controller $0B, event $DE, instrument byte $12; StartSoundVoice scales it by velocity ... */
    uint8_t TrackVolR[17]; /* Track right volume: controller $0C, event $DE, instrument byte $13 ($08CF = TrackVolR+$10 in PlaySound) */
    uint8_t TrackSustain[17]; /* Controller $40 (sustain pedal) value; StepTrack releases the track's voices when it becomes 0, UpdateVoice ... */
    uint8_t TrackUnused[17]; /* MusicStart sets all 17 bytes to $1F; never read */
    uint8_t VoiceState[8]; /* 0 free (FindFreeVoice), 1 gain / 2 ADSR (StartSoundVoice by instrument ADSR1 bit 7), 3 set by ... */
    uint8_t VoiceTrack[8]; /* Track that owns the voice (StartSoundVoice sta $4A; $10+ = sound effect, $FF after StopVoice); ... */
    uint8_t VoiceVelocity[8]; /* StartSoundVoice: VolumeCurve[velocity]*2; StepTrack event $DE rescales TrackVolL/R by it for the playing ... */
    uint8_t VoiceNote[8]; /* StartSoundVoice: note + instrument transpose (byte $04); StepTrack pitch bend reads PitchTable at it */
    uint8_t VoicePriority[8]; /* Instrument byte $2A (StartSoundVoice); FindFreeVoice only steals a voice whose priority is not above the ... */
    uint16_t VoiceLength[8]; /* StartSoundVoice: note length - 1; UpdateVoice counts it down and releases the voice at 0 */
    uint16_t VoiceInst[8]; /* Sound entry pointer the voice plays (StartSoundVoice $4E); QueueSound/StopSound/PlaySound .stop find a ... */
    uint16_t VoiceGain[8]; /* Gain word whose high byte SeqCmd0C0B sends as DSP GAIN (UpdateSfxVoices $5A); instrument byte $0A at ... */
    uint16_t VoicePitch[8]; /* Pitch (instrument tuning + bent PitchTable value); UpdateSfxVoices adds VoiceVibOfs and sends it ... */
    uint16_t VoiceBasePitch[8]; /* StartSoundVoice: PitchTable[note] before the bend; StepTrack event $E5 rebends from it */
    uint16_t VoiceAge[8]; /* Zeroed at start, inc every frame (UpdateSfxVoices); vibrato runs while it is between instrument $18 and ... */
    uint16_t VoiceVibCount[8]; /* UpdateSfxVoices dec; when negative steps the vibrato and reloads it from VoiceVibRate */
    uint16_t VoiceVibOfs[8]; /* Vibrato pitch offset: +/- VoiceVibSpeed per step up to VoiceVibDepth, added to VoicePitch */
    uint16_t VoiceVibRate[8]; /* Frames between vibrato steps: instrument $1C, ramped by instrument $2A/$2C (fraction in VoiceVibRateFrac) */
    uint16_t VoiceVibRateFrac[8]; /* Fraction of VoiceVibRate (UpdateSfxVoices clc adc instrument $2A, carry into VoiceVibRate) */
    uint16_t VoiceVibDepth[8]; /* Vibrato limit: instrument $20, ramped by $26/$28; at |VoiceVibOfs| >= it the vibrato turns or resets */
    uint16_t VoiceVibDepthFrac[8]; /* Fraction of VoiceVibDepth */
    uint16_t VoiceVibSpeed[8]; /* Vibrato step: instrument $1E, ramped by $22/$24; added to / subtracted from VoiceVibOfs */
    uint16_t VoiceVibSpeedFrac[8]; /* Fraction of VoiceVibSpeed */
    uint16_t VoiceVibDir[8]; /* 0 = vibrato offset rising, $FFFF = falling (bit VoiceVibDir,x; dec/inc to flip at the limit by instrument $2D ... */
    uint8_t RemapOut[32]; /* sixteen words: the tile remap is building */
    uint16_t mesarea[16]; /* string being built: a length word, then the bytes */
    uint16_t PWCharStr[34]; /* PasswordScreen: a one-character print string (length word 3, then the PasstoText character) printed ... */
    uint16_t PushWidthBuf[5]; /* PushNumberWrap/PushNumberWidth: length word (digits + 2), then 8 characters ($0A90-$0A97) set to spaces ... */
    uint16_t gamefps; /* Gameloop: gameframes (game frames since the faceoff) * 60 / FrameCount (NMI frames since the faceoff) = game frames ... */
    uint16_t padlast[5]; /* 5 words: each pad's buttons at the last ReadPads */
    uint16_t paddir[5]; /* 5 words: each pad's direction (PadDirection of the dpad bits) */
    uint16_t padchange[5]; /* 5 words: each pad's buttons that changed at the last ReadPads */
    uint16_t padnew[5]; /* 5 words: each pad's buttons newly pressed at the last ReadPads */
    uint16_t padchangeany; /* ReadPads: OR of the five padchange words; ReadAnyPad returns it in d1; SaveRegsStartHL2 clears it. */
    uint16_t repeatdelay; /* ProcessInputWithRepeat: frames to the next key repeat (dec, reload 6 / 20 on a change), as the 93S ... */
    uint16_t pausepad; /* the controller that owns the pause menu */
    uint16_t pausepadidle; /* SelectPauseController: 60 when the pause pad holds a button, -3 per idle frame; at 0 pausepad goes back to ... */
    uint8_t mp5padsig[5]; /* Multi5Bios: 5 bytes, one per pad: the low nibble of JOYxL (controller signature) shifted left with one ... */
    uint16_t bosgames; /* ReadPassBits/WritePassBits read and write it at the 93S bosgames slot (range 8); 0-6 = game of the best of ... */
    uint16_t fadestep; /* fade: a delay counting down to a fade in, or $FFFF to fade out (NMI) */
    uint16_t brightness; /* screen brightness 0-15 the fade sets; BrightnessHook writes it to INIDISP */
    uint16_t WinBits; /* ReadPassBits/WritePassBits at the 93S WinBits slot (range $4000); ResolveGames sets the bits of the teams ... */
    uint16_t mp5readall; /* PlayerSelect sets $FFFF while it runs and clears it at the end; when nonzero Multi5Bios reads the multitap ... */
    int16_t puckc; /* object index of the puck carrier, negative = loose */
    uint16_t timestep; /* Gameloop/SaveRegsStartHL2/ReplayMode/PauseReplay store d7 (frames elapsed) here (Pausemode_menu sets 2); ... */
    uint16_t padinputdone[5]; /* updateplayers: 5 words (pad*2) cleared each frame, set to 1 when the pad's doinput has run; ... */
    uint8_t PadControlObj[14]; /* setpads stores SCnum of the player each marker slot follows (slot 0 puck carrier, 1-5 pads, 6 replay ... */
    uint16_t fopadfirst[2]; /* FaceoffPads: per team (home here, away +2) the first pad given a player (findpc) at the last faceoff; the ... */
    uint16_t padsitnext[2]; /* asspenalty/assepen/FaceoffPads, indexed by team (a2): round-robin pad index for sitting a pad out (padside ... */
    uint16_t puckpadnext[2]; /* PuckCarrierPad/puckfaceoff2, indexed by team: the pad that last got the puck carrier, the round-robin ... */
    uint16_t passpad[2]; /* dopass stores the passer's pad (playerpad,x) per team (a2); puckglue gives that pad to the pass receiver ... */
    uint16_t goalieflags[12]; /* Per player (12): assgoalie/assgoalietopuck set bits 1,2 and clear bit 2 inside the crease, bit 1 near the ... */
    uint16_t puckdist[16]; /* per object: distance to the puck (ApproxLength) */
    uint16_t puckdist2[16]; /* per object: about (dx^2 + dy^2) / 4 to the puck */
    uint16_t puckdir[16]; /* per object: direction to the puck (vtoa) */
    uint16_t asscur[16]; /* Per object: the assignment number being run (the asslist entry at assnum), set by ... */
    uint16_t topgoalie; /* updateplayers: each frame the goalie (position 0) with Ypos >= 0 (top goal end), $FFFF none; avdgoal uses ... */
    uint16_t botgoalie; /* updateplayers: the goalie with Ypos < 0 (bottom goal end); avdgoal for the net at y -$108; topgoalie+2 in ... */
    uint16_t goaliestopped; /* assgoalie: set after the CPU goalie is stopped dead once while an opposing carrier is in a shot animation ... */
    uint16_t dumpinplayer; /* the player dumping the puck in (assdumpin), -1 none */
    uint16_t dumpintarget; /* assdumpin: the target index (DumpInTarget), a word offset into its x / y tables */
    uint16_t passtargetx; /* dopass: puckx + the pass vector x (where the receiver meets the puck); asspassrec aims the one-timer from ... */
    uint16_t passtargety; /* dopass: pucky + the pass vector y; asspassrec: goal line y (+-$108) minus it for the one-timer direction. */
    uint16_t onetimerreq; /* Set when the pass receiver's pad presses shoot (asspassrec, doinput) or the CPU decides; asspassrec then ... */
    uint16_t passwait[12]; /* Per player: dopass sets the receiver's frames until the puck arrives (d6+6); asspassrec counts it down by ... */
    uint16_t lastshotspeed[2]; /* doshot stores passspeed (the shot speed after the backhand cut) here; nothing reads it. */
    uint16_t GoalieUnused; /* assgoalietopuck clears it when the assignment starts */
    union {
        struct {
            uint16_t TitleWH1; /* Title: the WH1 window edge the frame hook writes; TitleWindowWipe moves it, TitleScreen clears it. */
            uint16_t TitleWH3; /* Title: the WH3 window edge for the top band (frame hook); TitleScreen steps it $42, $7D, $FF as the title ... */
            uint16_t TitleWH3Low; /* Title: the WH3 edge .irq1 writes for the band below line $46; TitleWindowWipe moves it. */
            uint16_t TitleBGMode; /* Title: the BGMODE the frame hook writes for the top band (9, then 1). */
            uint16_t TitleM7TM; /* Title: the TM value .irq2 writes for the mode 7 band ($11 / $10). */
            uint16_t TitleTeamIdx[2]; /* ShuffleTitleTeams fills the 26 team list at titleteams (it is the byte count while filling); NextTitleTeam ... */
            uint16_t CreditStep; /* StartCredits/RollCredits: credits state (0 scroll title and name, 1 scroll name, 2 hold, 3 done). */
            uint16_t CreditTime; /* RollCredits: the FrameCount frame count at which the next credits step runs. */
            uint16_t CreditTitleIdx; /* RollCredits: index into CreditTitles. */
            uint16_t CreditTitleY; /* RollCredits: screen y of the credit title (PrintStringSprites), scrolls up from $E0. */
            uint16_t CreditNameIdx; /* RollCredits: index into CreditNames. */
            uint16_t CreditNameY; /* RollCredits: screen y of the credit name, scrolls up from $F0. */
            uint16_t LogoSrcOfs; /* LoadTitleLogo: source offset of the logo row being unpacked (+$20 a row). */
            uint16_t LogoDstOfs; /* LoadTitleLogo: offset into the titlelogobuf mode 7 buffer of that row (+$40 a row). */
            uint16_t titleskip; /* set when a pad press ends the title sequence */
        };
        uint16_t goalieturnwait[16]; /* Per player: AdjustFacingDirection/goalieacc set 6 after turning a goalie's facedir one step and count it ... */
    };
    union {
        struct {
            uint16_t TitleSkipMask; /* Title: the frame hook ends the title (titleskip) when JoyPad has a bit of it; 0 at first, $F0C0 once the ... */
            uint8_t LogoPalBits[2]; /* LoadTitleLogo: the palette bits ($60 or $70) ORed into every unpacked logo pixel. */
            uint16_t TitlePathPos; /* TitleScreen .6: integer part of the logo path position (TitlePathFrac carries into it); with TitlePathFrac+1 it ... */
            uint16_t TitlePathFrac; /* TitleScreen .6: fraction of the path position (+$0C00 a frame). */
            uint8_t TitleM7X[2]; /* Title .irq2 writes it to M7X (low, then $0BF4 high). */
            uint8_t TitleM7Y[2]; /* Title .irq2 writes it to M7Y. */
            uint16_t TitleM7HOfs; /* Title .irq2 writes it to BG1HOFS; TitleZoomOutLeft/Right and the path move it. */
            uint16_t TitleM7VOfs; /* Title .irq2 writes it to BG1VOFS; TitleZoomOutLeft/Right and the path move it. */
            uint16_t TitleM7Scale; /* Title .irq2 writes it to M7A and M7D (the zoom); TitleZoomIn/OutLeft/OutRight step it between $100 and $200. */
            uint16_t TitlePathFlip; /* TitleScreen .6: toggled at each path end; nonzero negates the path's horizontal offset. */
            uint16_t TitleLogoFlags[6]; /* TitleScreen: bit 1 = the logo path runs, bit 0 = path ended: load the next team logo (NextTitleTeam, ... */
        };
        uint16_t goaliedivewait[16]; /* Per player: assgoalie sets $168 after a dive and $F0 after a poke check; updateplayers counts it down by ... */
    };
    uint16_t FaceoffUnused[12]; /* per player (12 words): FaceoffPads clears it with goalieflags, goalieturnwait, goaliedivewait */
    uint16_t benchcheer; /* the bench that is cheering (StartBenchCheer), 0 none */
    uint16_t benchrounds; /* StartBenchCheer clears it, UpdateBenchCheer counts the cheer rounds (ends after 4), FallDown sets 2 ... */
    uint16_t benchtimer; /* StartBenchCheer sets 11; UpdateBenchCheer counts it down by timestep and steps benchframe when it runs out. */
    uint16_t benchframe; /* bench cheer frame: set * 8 + step, drawn by setsortcords; negative = none (kept in the replay) */
    uint16_t crowdanim[21]; /* showcrowd: per crowd sprite slot (21 words, x = 0-$28): the CrowdAnimTable sequence playing (0 idle, $A, ... */
    uint16_t crowdanimstep[21]; /* showcrowd: per slot the byte offset of the current frame in the sequence (+2 each step). */
    uint16_t crowdanimtime[21]; /* showcrowd: per slot frames left on the current frame (count down by timestep). */
    uint16_t replaydelay; /* Goal / Stop4Pen set 30: updatereplay keeps recording until it counts down to 1, then stops; puckfaceoff2 / ... */
    uint16_t replaylock; /* ReplayMode: object the replay camera / cursor locks onto; $FFFF = none (follows PadControlObj slot 0 or ... */
    uint16_t recsfx; /* QueueSound: the sound queued this frame ($FFFF none) for the replay record; updatereplay records it ... */
    uint8_t recsfxpitch[2]; /* StartSoundVoice / PuckSfx / updatepwrplay store the voice pitch (VoicePitch) of the recorded sound; ReplayMode ... */
    uint8_t recsfxvol[2]; /* StartSoundVoice stores the sound's volume (VoiceGain); ReplayMode puts it in $64 (the volume in the high byte) ... */
    uint16_t recsfx2; /* QueueSound moves recsfx here when a second sound comes in the same frame; recorded and replayed like recsfx. */
    uint8_t recsfxpitch2[2]; /* QueueSound copies recsfxpitch here with recsfx2; ReplayMode restores VoicePitch from it. */
    uint8_t recsfxvol2[2]; /* QueueSound copies recsfxvol here; ReplayMode loads $64 from it. */
    uint16_t replaynewframe; /* RestoreReplayFrame increments it, ReplayMode clears it after drawing; showcrowd animates during a replay ... */
    uint16_t replaymusic; /* PauseReplay saves MusicOn here; at the end of ReplayMode a nonzero value restarts the menu / playoff song. */
    uint16_t replayHpos; /* ReplayMode: the replay camera x, eased toward the locked object / cursor (4 or 2 a frame, clamped) and ... */
    uint16_t replayVpos; /* ReplayMode: the replay camera y, eased the same way and written to Vpos. */
    uint16_t percue; /* StartPer sets $FFFF (cleared in some modes); puckfaceoff2 then plays team song slot 0 (the period start ... */
    uint16_t cursong; /* PlayTeamSong stores the TeamSongs slot + 1 of the song started; the SPC uploads and UpdateStreamSfx (no ... */
    uint16_t sprhflip; /* addframe parameter: nonzero = x flip (attribute $4000); setsortcords/checkfo set attribute & $0800 as 93S ... */
    uint16_t reloadtime; /* StartPer clears it, ExitStatsScreen adds 60 each time it reloads the rink; never read. */
    uint16_t sprattr; /* addframe parameter: the OAM attribute word for DrawSprite (priority, palette $0200; addframe ORs in the ... */
    uint16_t sprpriority; /* checkfo sets $3000 for the faceoff sprites and $2000 after, ORed into sprattr; setoptions sets 0: as 93S ... */
    uint16_t sprframe; /* addframe parameter: sprite frame number (1-based, at most $3D0), as 93S sprframe; set by setsortcords, ... */
    uint16_t sprx; /* addframe parameter: screen x of the frame (93S sprx). */
    uint16_t spry; /* addframe parameter: screen y of the frame (93S spry). */
    uint16_t sprvflip; /* addframe parameter: nonzero = y flip (attribute $8000); setsortcords sets attribute & $1000 as 93S sprvflip. */
    uint16_t textdirtylo; /* lowest changed word of the text map */
    uint16_t textdirtyhi; /* highest changed word of the text map */
    uint16_t printm; /* offset of the print map in the text map */
    uint16_t collflag; /* a player collision happened: checkcoll restores the old position (set by Bcheck, checkgoal) */
    uint16_t impactstep; /* d7*2: impact decay this game frame (Gameloop) */
    uint16_t velstep; /* d7*16: velocity to position scale this game frame */
    uint16_t gravstep; /* d7*96: gravity this game frame */
    uint16_t pausetimer; /* pause menu: frames left before a demo pause ends by itself */
    uint16_t ThirdPBonus; /* setplayer: +2 when tied or trailing late in the 3rd period */
    uint16_t cheercnt; /* crowd cheers still to play after a goal (one every 150 frames, Gameloop) */
    uint16_t cheervc; /* vcount of the last crowd cheer */
    int16_t Hpos; /* ice rink horizontal position */
    int16_t Hscroll; /* horizontal scroll */
    uint16_t HmAwBonus; /* setplayer: home bonus / visitor penalty */
    uint16_t iflags; /* icing flags (if* masks) */
    uint16_t lastplayer[2]; /* the last player who touched the puck (doshot, puckstick) */
    uint16_t lldisp; /* count down for the once-a-second screen / game updates (periodicevents) */
    uint16_t nibblebuffer[4]; /* 4 words: the weights SetScore picks from */
    uint16_t oldvcount; /* vcount at the last game frame (d7 = vcount - oldvcount) */
    uint16_t passdir; /* direction for pass/shot 0-7 (8 = none) */
    uint16_t passspeed; /* speed of pass/shot */
    uint16_t passReceiverPlayerNum; /* the player the last pass was aimed at (negative = none) */
    int16_t PKBonus; /* setplayer: penalty kill bonus (negative) */
    uint16_t PPBonus; /* setplayer: power play bonus */
    uint16_t printa; /* attribute (palette / priority bits) for print characters */
    uint16_t printx; /* x cordinate for printing (text map column) */
    uint16_t printy; /* y cordinate for printing */
    uint16_t psfirst; /* pause menu: first item shown (SetupPauseScreen shows five from here) */
    uint16_t psitem; /* pause menu item */
    uint16_t puckcross[4]; /* 2 entries of (x, frames): where / when the puck crosses the top / bottom goal line */
    uint32_t ReplayBufferPtr; /* long pointer to the next replay frame record */
    uint16_t Oldrow; /* last rink map row */
    uint16_t vramrow[2]; /* BG2 tile map row at the top of the ring buffer */
    uint16_t shotplayer; /* the player who shot last (doshot) */
    uint16_t sfxmutetime; /* vcount of the last goal: Stop4Pen waits 60 frames after it */
    uint16_t threat; /* direction of threat on puck handler (nonzero = threat) */
    uint16_t demoflag; /* nonzero in demo mode */
    uint16_t TickerNum; /* playoff game the score ticker shows next */
    uint16_t tickervc; /* vcount when the ticker box was last shown */
    uint16_t lastframevc; /* vcount of the last game frame (Gameloop) */
    int16_t Vpos; /* ice rink vertical position */
    int16_t Vscroll; /* vertical scroll (low 3 bits = BG2 fine scroll) */
    uint16_t wcradiusx; /* wall collision radius x */
    uint16_t wcradiusy; /* wall collision radius y */
    uint16_t PlayerScrollCtr; /* scouting report player scroll */
    uint16_t screentimer; /* frames left on the scouting report screen */
    uint16_t SelectedPlayerIdx; /* scouting report selected player */
    uint16_t ScoutingReportTimer; /* scouting report timer */
    uint16_t DispAttribCtr; /* text column (scouting report, line and shooter lists) */
    uint16_t VertLineScrolling; /* scouting report vertical scroll */
    int16_t xc1; /* scroll lock x */
    int16_t yc1; /* scroll lock y */
    uint16_t yleader; /* lead distance on scrolling */
    uint16_t vcountmark[2]; /* 32-bit vcount snapshot: elapsed time base */
    int16_t Xpos[17]; /* x position (integer part) of each object */
    uint16_t Xfrac[16]; /* fraction of Xpos */
    uint16_t frame[16]; /* sprite frame number of each object (SPF groups); object 14 ($1C) is the puck, 15 ($1E) its shadow */
    int16_t Ypos[14]; /* y position (integer part) of each object */
    int16_t pucky[3]; /* puck y position */
    uint16_t Yfrac[16]; /* fraction of Ypos */
    int16_t puckz; /* puck height (93S Zpos+puckobj) */
    int16_t shadowz; /* height of object 15, the puck shadow (setsortcords) */
    uint16_t puckzfrac; /* fraction of puckz */
    int16_t puckoldz; /* puckz before this frame's move (93S OldZpos+puckobj) */
    int16_t puckvz; /* puck height speed (93S Zvel+puckobj) */
    uint16_t attribute[16]; /* frame attribute bits (attrxflip, attryflip) */
    uint16_t oldframe[16]; /* frame last drawn (setsortcords) */
    int16_t OldXpos[16]; /* Xpos before this frame's move */
    uint16_t OldXfrac[16];
    int16_t OldYpos[16];
    uint16_t OldYfrac[16];
    int16_t Xvel[14]; /* x speed (puckvx is Xvel+puckobj) */
    int16_t puckvx[2]; /* puck x speed */
    int16_t Yvel[14]; /* y speed */
    int16_t puckvy[2]; /* puck y speed */
    int16_t impactp[16]; /* the other object (index) of the last collision or hold */
    int16_t impact[16]; /* impact value, added to by collisions */
    int16_t position[16]; /* player's position (0=goalie,1=l.def,2=r.def,3=l.wing,4=center,5=r.wing; negative = none) */
    uint16_t assnum[16]; /* index of the current assignment (0-7) */
    uint8_t asslist[128]; /* assignments, 8 bytes per object */
    int16_t temp1[16]; /* assignment scratch, per object */
    int16_t temp2[16];
    int16_t temp3[16];
    int16_t temp4[16];
    int16_t temp5[16];
    int16_t temp2b[16];
    int16_t radiusx[16]; /* collision width (defaultsprites_0) */
    int16_t radiusy[16]; /* collision height */
    uint16_t Wallcos[16]; /* per object: cos of the angle of the last wall collision */
    uint16_t Wallsin[16]; /* per object: sin of that angle */
    uint16_t SCnum[16]; /* object number (index / 2) */
    uint16_t facedir[16]; /* direction of facing, 0-7 */
    uint16_t facefrac[16]; /* turn fraction of facedir (doplayeracc) */
    uint16_t SPA[16]; /* current animation (offset into the animation data) */
    uint16_t SPAnum[16]; /* index into animation (4 per frame) */
    uint16_t SPAcnt[16]; /* count down to the next animation frame (-1 = restart) */
    uint16_t nopuck[16]; /* no puck collision until zero */
    uint16_t nopuckb[16]; /* second count down */
    int16_t newpos[16]; /* requested next position (negative = none) */
    int16_t newpnum[16]; /* requested next roster player (negative = none) */
    uint16_t pflags[16]; /* pflags (bits 0-7) and pflags2 (bit n at bit n+10); some flags have their own arrays below */
    uint16_t pnum[16]; /* player's team location number (low byte) */
    uint16_t weight[16];
    uint16_t legstr[16];
    uint16_t legspd[16];
    uint16_t aioff[16];
    uint16_t aidef[16];
    uint16_t shotspd[16];
    uint16_t shotacc[16];
    uint16_t passacc[16]; /* passacc (goalies: glove save right) */
    uint8_t rostnum[32];
    uint16_t spodds[16]; /* shot / pass odds */
    uint16_t stickhand[16];
    uint16_t endurance[16]; /* endurance (goalies: stick save right) */
    uint16_t aggress[16]; /* aggression: penalty odds (goalies: glove save left) */
    uint16_t fighting[16]; /* fighting rating (stored; 94S has no fights; 93 $74) */
    uint16_t checking[16]; /* checking rating (check odds; 93 $75) */
    uint16_t handed[16]; /* nonzero: right handed */
    uint16_t pfgoal[16]; /* nonzero: shoots at the top goal. pflags bit pfgoal */
    uint16_t pfalock[16]; /* nonzero: locked until the animation is done */
    uint16_t pfteam[16]; /* 0 = home, nonzero = visitors. pflags bit pfteam */
    uint16_t pfrev[16]; /* nonzero: skating backwards. pflags bit pfrev */
    uint16_t pf2aip[16]; /* nonzero: animation in progress. pflags2 bit pf2aip */
    uint16_t pfnc[16]; /* nonzero: no collision mode. pflags bit pfnc */
    uint16_t Ylist[16]; /* y of each sort object */
    uint16_t OOlistpos[16]; /* position of each object in OOlist */
    uint8_t OOlist[17]; /* 16 bytes: object order list, bottom of the screen last */
    uint16_t gmode; /* game mode flags (gm* masks) */
    uint16_t gmclock; /* nonzero: game clock is stopped */
    uint16_t sflags; /* sflags (low byte) and sflags2 (high byte) as one word (sf* /sf2* masks) */
    uint16_t sfhormode; /* nonzero: screen in horizontal mode. sflags bit sfhor */
    uint16_t sflags3[34]; /* sflags3 (sf3* masks) */
    int16_t Pencntdwn; /* penalty/fight count down (negative = fight over) */
    uint16_t pentimer; /* frames to the next penalty clock second (updatepentime) */
    uint16_t PBnum; /* players in the penalty boxes: home*16 + visitors */
    uint16_t InjCntDown; /* frames until the injury box is shown */
    uint16_t penmsgtimer; /* count down to reprinting the horizontal penalty message line */
    int16_t RefCnt; /* ref graphics control (negative = idle) */
    int16_t RefStep; /* step in the ref animation (negative = done) */
    uint16_t RefPen; /* penalty number of the ref animation */
    uint16_t gsp; /* game period */
    uint16_t gameclock[2]; /* game clock seconds; gameclock+2 = jiffies */
    uint16_t TempPlOffset; /* pnum of the penalized player, high bit set for the visitors */
    uint16_t IsBreakaway; /* 1 while the puck carrier has a breakaway (CheckBreakaway) */
    uint16_t PenShotActive; /* 1 during a penalty shot (SetupPenaltyShot .. puckfaceoff2) */
    uint16_t PenSumLength; /* penalty log entries (94G PenSumLength; 94S counts entries, not bytes) */
    uint16_t PenSum[130]; /* penalty log: $41 entries of (time word, player | $80 away, penalty) (InProgress, FillPenaltyPage) */
    uint16_t tmPwrGoals[2]; /* power play goals (team word pair); the first word clearTeamStats clears (through $18ED) */
    uint16_t tmPwrPlays[2]; /* power plays */
    uint16_t tmPenalties[2]; /* penalties */
    uint16_t tmPenmin[2]; /* penalty minutes */
    uint16_t tmATOP[2]; /* attack zone time (chkatop) */
    uint16_t tmPwrPlayTime[2]; /* power play time (updatePPTeamTime) */
    uint16_t tmPPShots[2]; /* power play shots (ChkShotStat) */
    uint16_t tmSHShots[2]; /* shorthanded shots (ChkShotStat; not on the stats screen) */
    uint16_t tmSHGoals[2]; /* shorthanded goals */
    uint16_t tmsort[2]; /* first object index of the team */
    uint16_t tmPerShots[10]; /* shots by period: + period*4 + team*2 */
    uint16_t tmPerGoals[8]; /* goals by period: + period*4 + team*2 */
    uint16_t tmscore[2]; /* score */
    uint16_t tmfowon[2]; /* faceoffs won */
    uint16_t tmline[2]; /* current line */
    uint16_t tmtouch1[2]; /* pnum of the last touch */
    uint16_t tmtouch2[2]; /* second last touch */
    uint16_t tmtouch3[2]; /* third last touch */
    uint16_t tmap[2]; /* active players on ice */
    uint16_t tmgoalie[2]; /* home and away goalie (words): low byte = goalie 0/1, $FFxx = pulled */
    uint16_t tmnear[2]; /* nearest distance to the puck this frame (updateplayers; 93S tmnear, one word here) */
    uint16_t tmnearpl[2]; /* the team's player nearest the puck (updateplayers, assnearest) */
    uint16_t tmchecks[2]; /* body checks (FallDown) */
    uint16_t tmpasscomp[2]; /* completed passes (puckglue) */
    uint16_t tmpassatt[2]; /* pass attempts (dopass) */
    uint16_t tmBreakaways[2]; /* breakaways (StartBreakaway) */
    uint16_t tmBreakawayGoals[2]; /* breakaway goals */
    uint16_t tmPenShots[2]; /* penalty shots (StartPenaltyShot) */
    uint16_t tmPenShotGoals[2]; /* penalty shot goals */
    uint16_t tmOneTimerSwings[2]; /* one-timer swings started (asspassrec; not on the stats screen) */
    uint16_t tmOneTimers[2]; /* one-timers (asspassrec) */
    uint16_t tmOneTimerGoals[2]; /* one-timer goals */
    uint16_t tmflags[2]; /* team flags */
    uint16_t tmtimeouts[2]; /* timeouts left (StartGame sets 1; TimeoutMenu) */
    uint16_t tmlcchoice[2]; /* the chosen line change row (lcfound, SetLCmode2) */
    uint16_t tmpde[52]; /* energy of each roster player (word per player) */
    uint16_t tmpdst[52]; /* bench/ice/box state of each roster player */
    uint8_t tmboxlist[52]; /* penalty box list of roster players (bytes, negative ends) */
    uint8_t tmgoals[52]; /* goals (goalies: goals against) */
    uint8_t tmassists[52]; /* assists */
    uint8_t tmpshots[52]; /* shots (goalies: shots against) */
    uint8_t ppenmin[52]; /* penalty minutes */
    uint16_t CwdExciteLvl; /* crowd excitement: raised by hits and goals, set per playoff stage at a period start */
    uint16_t CrowdLoud; /* 1 at 110 dB or more; cleared under 105 dB (Crowd_Noise) */
    uint16_t CurCrowdMeter; /* the crowd meter in dB: Sroot(CwdExciteLvl * 4) + 65, at most 128 */
    uint16_t CrowdPeak; /* the highest CurCrowdMeter of the game */
    uint16_t CrowdAvg; /* the game's average meter (CrowdMeterSum / CrowdMeterCount) */
    uint16_t CrowdMeterCount; /* meter samples taken */
    uint16_t CrowdMeterSum[2]; /* 32-bit sum of the meter samples */
    uint16_t HotColdRaised; /* nonzero while the hot / cold table is raised by 2 (LowerHotColdTable undoes it) */
    uint16_t teampads[2]; /* pads on the home / away team */
    uint16_t playerpad[12]; /* per player object: the controlling pad * 2, negative = none */
    uint16_t c1playernum[5]; /* per pad (5 words): the player object it controls (93S c1playernum, c2playernum ...) */
    uint16_t padside[5]; /* per pad: 0 home / 2 away; high byte $80 = moved for a penalty shot or shootout */
    uint16_t mp5connect; /* Multi5ConnectCheck: bit 7 multitap on port 1, bit 6 on port 2 */
    uint16_t mp5readdelay; /* frames to the next multitap read while pads 4 and 5 are unused */
    uint8_t mp5checktimer[2]; /* frames left calling Multi5ConnectCheck after a port line change */
    uint8_t mp5port1last[2]; /* last JOYSER0 bit 1 */
    uint8_t mp5port2last[2]; /* last JOYSER1 bit 1 */
    uint8_t mp5portchg[2]; /* the port 1 change bit */
    uint16_t mp5present; /* bit 0: multitap on port 2 (pads 3-5 are read) */
    uint16_t mp5warning; /* bit 0 multitap in port 1, bit 1 Super Scope, bit 2 mouse: ShowMP5Warning shows its text */
    uint16_t InIntermission; /* 1 during the intermission */
    uint8_t HotColdTable[624]; /* 52 players x 12 attribute bytes added to the ratings (Create_HotCold_Table) */
    uint16_t PerTimeTotal; /* length of this period (seconds) */
    uint16_t cont1team; /* Team on pad 1 (0 none, 1 home, 2 away); StartGame turns cont1team,x into padside,x = (team-1)*2, ... */
    uint16_t cont2team; /* Pad 2 team word of the cont1team array (93S/94G cont2team) */
    uint16_t cont3team; /* Pad 3 team word of the cont1team array (94G cont3team) */
    uint16_t cont4team; /* Pad 4 team word of the cont1team array (94G cont4team) */
    uint16_t cont5team[2]; /* Pad 5 (multitap) team word, the last of the five the loops over cont1team,x stop at ($0A); name extends ... */
    uint16_t goaliemode1; /* Home manual goalie flag, word per team indexed goaliemode1,y: StartGame copies the goalie option (OptGoalie) ... */
    uint16_t goaliemode2; /* Visitors manual goalie flag (goaliemode1+2), set with goaliemode1 by StartGame (94G goaliemode2) */
    uint16_t defcontrol[2]; /* Defense control on/off per team (word, indexed by team 0/2): PauseDefControl toggles it, SetupPauseScreen prints ... */
    uint16_t HomeTeam; /* home team number (TeamList index) */
    uint16_t VisTeam[33]; /* visiting team number */
    uint16_t tmdata[2]; /* team data address */
    uint8_t tmteam[117]; /* team number of each team struct */
    uint16_t lcpad[2]; /* Per team (y = 0/2) the pad (d4) that opened the line change box, negative = none: doinput sets it and ... */
    uint16_t jmlvector[2]; uint8_t jmlvector_end_; /* RAM jml trampoline: $5C (jml opcode), target at +1..+3 */
    uint16_t asv; /* Scouting text without delays: ScoutingReport sets $FFFF while down is held and clears it at the end of the ... */
    uint16_t PauseUnused[4]; /* Pausemode clears it when the pause menu is left (with pregamepause) */
    uint16_t sndbank[2]; uint8_t sndbank_end_; /* Sample set loaded in the SPC: SPCGameUpload/SPCIntroUpload/SPCTitleUpload skip the upload when it is 1 and ... */
    uint16_t gameframes[2]; /* Gameloop frames since the last faceoff: Gameloop increments it and computes gameframes*60/FrameCount (NMI frames) ... */
    uint8_t homeshooters[8]; /* home shootout list: the goalie, then the shooters (bytes, player number + 1) */
    uint8_t awayshooters[8]; /* away shootout list, as homeshooters */
    uint16_t shootoutmode; /* Nonzero during the shootout: PlayShootout sets 1 and clears it at the end, Opening clears it; ... */
    uint16_t shootoutstop; /* Shootout turn whistle: PlayShootout .t1 clears it, SetPA sets 1 in shootoutmode when the ref animation is ... */
    uint16_t shootoutround; /* shootout round, from 1 */
    uint16_t shootoutturnover; /* Shootout turn finished: cleared at each PlayShootout turn, incremented by puckfaceoff after shootoutstop; ... */
    uint16_t shootoutdecider; /* PlayShootout sets 0 before each round and 1 before the home team's turn from round 5 on; ShootoutWonBy ... */
    uint16_t linecursor[2]; /* Line editor slot cursor row: LineEditor starts it at 4 and moves it within 0..lineslots-1, ... */
    uint16_t lineplayer[2]; /* SetLineSlot: the player being put in the cursor slot (A on entry), compared with the slots for the swap ... */
    uint16_t lineswap[2]; /* Temporary slot byte while SetLineSlot (.5) and SwapLineSlots swap two line slots */
    uint16_t linemenuitem[2]; /* LineEditorExit item: 0 Return to Game, 1 Restore default lines (down/up set it, the button acts on it, ... */
    uint16_t curplayer[2]; /* Roster index of the player being worked on: DrawPlayerCard/NextPlayerCard/PrevPlayerCard/DrawPlayerPicture ... */
    uint16_t lineslots[2]; /* Rows (slots) of the line being edited: LineEditor sets 5, drops it to 4 for lines 6-7 and back; ... */
    uint16_t listcount[2]; /* Size of the current list: the defense/forward (SelectLinePlayer) or skater (ShooterSelectList) count of ... */
    uint16_t picktop[2]; /* Pick list (line editor, shootout shooters): roster index of the first player shown; ... */
    uint16_t pickfirst[2]; /* Pick list: roster index of the first player of the listed category (goalie count, + forwards for defense), ... */
    uint16_t pickrows[5]; /* Line editor pick list rows shown = min(9, listcount) (SelectLinePlayer), bound of the cursor, used by ... */
    uint16_t curitem[2]; /* Current item of the screen up: exit prompt choice (AskExitGame, DrawExitPrompt), card (PlayerCards, ... */
    uint16_t listtop[2]; /* First row shown of a scrolled stats list (game stats, penalty/scoring summaries, other scores: ... */
    uint16_t listend[2]; /* End of the current list: penalty count (PenSumLength), goal count, other games + 1, last player card (players+6), ... */
    uint16_t listrows[2]; /* Rows of a scrolled stats list in the window / sent to VRAM (5 summaries, 6 game stats, 7 player stats; one ... */
    uint16_t statstemp[2]; /* Player stats scratch: ComparePlayerStats returns nonzero to swap and uses it as a temp, SortPlayerStats ... */
    uint16_t scrollirqdone[2]; /* IRQ_StatsScroll1 sets $FFFF each frame; the unused waits in ScrollGameStatsDown (.t1) and OtherScoresRtl ... */
    uint16_t crowdmeterlit[12]; /* CrowdMeterScreen: lit columns of the crowd meter (level/4), the width the CrowdMeterLitMap paste loop uses */
    uint16_t otherskip[2]; /* Other scores: 1 when FillOtherScores skipped the user's game (gamenum), added into the row math of ... */
    uint16_t irqline1; /* Line of the first split IRQ: SetupScrollIRQ/SetupBandIRQ store A here and SetIRQ at it, IRQ_BandEnd ... */
    uint16_t irqline2; /* Line of the second split IRQ: SetupScrollIRQ/SetupBandIRQ store X, IRQ_StatsScroll1/IRQ_BandStart load ... */
    uint16_t irqline3; /* Line of the third split IRQ: SetupScrollIRQ stores Y, IRQ_StatsScroll2 loads VTIME from it */
    uint16_t irqvscroll1; /* BG1VOFS written by IRQ_StatsScroll1 (low byte, then $1DF4): the scroll routines step it by 2 per frame for ... */
    uint16_t irqvscroll2; /* BG1VOFS of the second split, written by IRQ_StatsScroll2; StatsReport/PlayerStatsScreen and the summary ... */
    uint8_t irqvscroll3[2]; /* BG1VOFS of the third split, written by IRQ_StatsScroll3; SetupScrollIRQ sets $FFFF */
    uint16_t statcolumn; /* Player stats page column: 0 = goalies, 1-5 = skater sort column; left/right in PlayerStatsScreen, ... */
    uint8_t statsorder[26]; /* Player stats row order, a byte per roster player (InitStatsOrder fills 0,1,2..; ... */
    uint16_t statpenrows; /* Game stats: $10 when penalties are on, 0 when off (StatsReport); DisplayStats ORs it with each ... */
    uint16_t letterx[2]; /* X of the next sprite letter: PrintLetterSprites, PrintSmallLetterSprites, PrintStringSprites, ... */
    uint16_t lettery[2]; /* Y of the sprite letters (Y on entry), passed to DrawSprite by the same routines */
    uint16_t otgoalscorer; /* ShowThreeStars: player (+$1A for visitors, $FFFF none) taken from a goal entry whose period field is 3 ... */
    uint16_t starplayerend; /* ShowThreeStars: end of the team's player indexes in the scan (GetPlayerCount, +$1A for visitors) */
    uint16_t stargoalieend; /* ShowThreeStars: end of the team's goalie indexes (GetGoalieCount, +$1A for visitors); below it the goalie scoring ... */
    uint16_t starswapplayer; /* ShowThreeStars: player pushed down a place while inserting into the three stars */
    uint16_t starswapscore[2]; /* ShowThreeStars: 32-bit star score (low word, high word at +2) of the player pushed down while inserting */
    uint16_t star1; /* ShowThreeStars: first star (player index, +$1A for visitors until printing) */
    uint16_t star2; /* ShowThreeStars: second star */
    uint16_t star3; /* ShowThreeStars: third star */
    uint16_t star1score[2]; /* ShowThreeStars: 32-bit score of star1 (low word compared with d0, high at +2 with $A7) */
    uint16_t star2score[2]; /* ShowThreeStars: 32-bit score of star2 */
    uint16_t star3score[2]; /* ShowThreeStars: 32-bit score of star3 */
    uint16_t matchuptimer; /* ScoutingReport: frames on the current matchup, the next matchup comes at 300 and paging resets it (94G ... */
    uint16_t matchuphold; /* ScoutingReport: $FFFF once a pad paged the matchups (A/C/Y), stops the automatic paging and the screentimer */
    uint16_t hotterteam; /* ScoutingReport: team number with the higher hot/cold total, $FFFF when they differ by less than 4; ... */
    uint16_t hotextreme; /* ScoutingReport: 1 when the hot/cold totals differ by 8 or more; ScoutExtremely prints 'extremely ' */
    uint16_t hotcoldsum[2]; /* ScoutingReport: per team (y 0/2) sum of rating minus base rating over the roster */
    uint16_t hotplayer1[2]; /* ScoutingReport: hottest player per team (home, visitors at +2, $FFFF none); ScoutHomeHot1 prints it (94G ... */
    uint16_t hotplayer2[2]; /* Second hottest player per team (ScoutHomeHot2) */
    uint16_t hotplayer3[2]; /* Third hottest player per team (ScoutHomeHot3) */
    uint16_t coldplayer1[2]; /* Coldest player per team (ScoutHomeCold1) */
    uint16_t coldplayer2[2]; /* Second coldest per team (ScoutHomeCold2) */
    uint16_t coldplayer3[2]; /* Third coldest per team (ScoutHomeCold3) */
    uint16_t hotvalue1[2]; /* ScoutingReport: hot value (rating - base rating) of hotplayer1 per team, init $FF06, compared in .t1xx */
    uint16_t hotvalue2[2]; /* Hot value of hotplayer2 per team */
    uint16_t hotvalue3[2]; /* Hot value of hotplayer3 per team */
    uint16_t coldvalue1[2]; /* Cold value of coldplayer1 per team, init $00FA */
    uint16_t coldvalue2[2]; /* Cold value of coldplayer2 per team */
    uint16_t coldvalue3[2]; /* Cold value of coldplayer3 per team */
    uint16_t strengths[6]; /* Player card best three ratings as (column, rating) word pairs: FindSkaterStrengths/FindGoalieStrengths ... */
    uint16_t cardframechars; /* LoadPlayerCardScreen: first VRAM tile after the card back tiles (where CardFrameTiles go), added to every ... */
    uint16_t picattr; /* DrawPlayerPicture: map attribute (palette) of the picture, $3400 home / $2400 visitors, added to the ... */
    uint16_t onetimer; /* A one-timer shot is under way: asspassrec sets 1 when the receiver shoots the pass directly (counts ... */
    uint16_t breakaway; /* Breakaway in progress (CheckBreakaway result IsBreakaway): StartBreakaway, PenShotChk, doshot set it; Goal ... */
    uint16_t BA_Sktr_SCnum; /* Penalty shot / shootout shooter object (SCnum*2, $FFFF none): Bcheck sets it on a penalty-shot foul, ... */
    uint16_t BA_Skater_Offset; /* Roster index of the penalty shot shooter: StartPenaltyShot = pnum of BA_Sktr_SCnum, PlayShootout = shooter ... */
    uint16_t BA_Team; /* Shooting team of the penalty shot / shootout turn (0 home, 2 visitors): StartPenaltyShot, PlayShootout; ... */
    uint16_t pspad[2]; /* Pad index (into padside) controlling the penalty shooter, $FFFF none: Bcheck copies playerpad,y, PlayShootout ... */
    uint16_t psfoxsave; /* StartPenaltyShot saves the faceoff spot x (fox) here and zeroes it; EndPenaltyShotPlay puts it back into ... */
    uint16_t psfoysave; /* StartPenaltyShot saves the faceoff spot y (foy); EndPenaltyShotPlay puts it into lty (last touch y) */
    uint16_t penshot; /* Penalty shot pending / under way: StartPenaltyShot sets 1, EndPenaltyShotPlay and StartGame clear; Goal, ... */
    uint16_t penshotlive; /* Set 1 by SetupPenaltyShot when the shot starts, cleared by puckfaceoff (.9) and StartGame; ... */
    uint16_t pstimer; /* Penalty shot time limit: SetupPenaltyShot sets 1000, Gameloop subtracts d7 outside the shootout and calls ... */
    uint16_t sopathx; /* assbreakaway: x of the current point of the CPU breakaway skater's path to the net (from the .10 point ... */
    uint16_t sopathy; /* assbreakaway: y of the current path point, negated for the bottom goal, skateto target (94G sopathy) */
    uint16_t sopathnext; /* assbreakaway: distance toward the goal at which the next path point is taken (.9); negative = last point, ... */
    uint16_t sopathpoint; /* assbreakaway: index of the path point in the .10 table, cleared at the start, incremented by .9 (94G ... */
    uint16_t sopathside; /* assbreakaway: x side of the path (first point x, signed by pfgoal); sopathx is negated by it and a ... */
    uint16_t sopathface; /* assbreakaway: facedir when the last path point was taken; the shot comes when facedir moves away from it */
    uint16_t lastperiod; /* Period just finished: PeriodOver copies gsp, StartGame clears it, DrawPeriodStats shows the periods up to it */
    uint16_t teamtaken[2]; /* InitializeGameStructures: nonzero when the random team's bit was already set (pick again) */
    uint16_t teambit[2]; /* InitializeGameStructures: 1 << (team & $F), tested and ORed into the used-team bit sets $B3 / d3 */
    uint16_t pmenulist; /* Pause menu list shown (0-5): Pausemode_menu/ChoosePauseMenu pick it, SetupPauseScreen prints items and values by it, ... */
    uint16_t pmenucount; /* Items in that pause menu list (Pausemode_menu/ChoosePauseMenu set it with pmenulist); bounds psitem and psfirst */
    uint16_t pregamepause; /* StartGame sets 1 when any pad has a team; StartPer then opens the pause menu at the start (startpause), ... */
    uint8_t RAMTag2[323]; /* Reset_Fast copies the 6 byte InitTag 'EACDSI' here (and to RAMTag and ReplayBufferEnd) after clearing RAM; ... */
    uint8_t HandleTable[1344]; /* handles of 4 bytes: address word, bank word (0,0 = free) */
    uint16_t FrameCacheAddr[128]; /* address of the frame (LoadFrameTiles; FindCachedFrame compares it) */
    uint16_t FrameCacheBank[128]; /* bank of the frame */
    uint8_t FrameCacheOf[1024]; /* a byte per frame number ($2741 + DrawFrameNum): its cache entry, $FF none (InitSprites fills $FF) */
    uint16_t FrameCacheNum[128]; /* frame number of the entry */
    uint16_t FrameCacheAlloc[128]; /* the entry's allocation: block run index | $8000 (16x16 blocks), else the single tile run index */
    uint16_t FrameCacheUse[128]; /* use bits: GetFrameTiles sets CacheUseMark when drawn, SpriteFrameStart shifts them each frame and frees the entry when they run out */
    uint16_t FrameCacheTile[128]; /* first VRAM tile of the entry's tiles (the DMA destination) */
    uint8_t BlockRuns[121]; /* 16x16 block area of sprite VRAM (InitSprites: one free run of SpriteBlockTiles / 4) */
    uint8_t BlockRunPrev[121]; /* previous run of each block run ($FF = none) */
    uint8_t TileRuns[97]; /* single 8x8 tile area (from SpriteBlockTiles to $200) */
    uint8_t TileRunPrev[97]; /* previous run of each tile run ($FF = none) */
    uint16_t carriedy; /* updateplayers stores pucky here while puckc >= 0, as 93S updateplayers .cy stores carriedy. */
    uint16_t ltplayer; /* a2touchpuck stores the touching player (index / 2) here like 93S a2touchpuck sta ltplayer. */
    uint16_t ltx; /* a2touchpuck stores Xpos,y (93S ltx); Stop4Pen loads it as the OOP faceoff x like 93S; EndPenaltyShotPlay ... */
    uint8_t lty[38]; /* a2touchpuck stores Ypos,y (93S lty); Stop4Pen loads it into d1 like 93S. */
    uint16_t fobox; /* puckfaceoff2 sets it (FaceOffMap+6) when the faceoff box is drawn, Endfaceoff clears it, SetLCmode2 / ... */
    uint16_t fodir1; /* Endfaceoff loads it into d3 (home pull direction) as 93S lda fodir1; puckfaceoff2 sets $FFFF. */
    uint16_t fodir2; /* Endfaceoff loads it into d3 when the winner faces down, as 93S fodir2; puckfaceoff2 sets $FFFF. */
    uint16_t fodropx; /* puckfaceoff2 sets the faceoff box x; updatefaceoff ((x-$2E)/8 to printx) and checkfo read it as 93S fodropx. */
    uint16_t fodropy; /* puckfaceoff2 sets $5C; updatefaceoff ((y-$44)/8 to printy) and checkfo read it as 93S fodropy. */
    uint16_t fofdata2[4]; /* Faceoff pull frame / attribute pairs of the home (fofdata2 / +2) and visiting (+4 / +6) centre: ... */
    uint16_t fofdata[2]; /* Frame / attribute of the faceoff ref: updatefaceoff stores the drop frame here (93S sta fofdata); ... */
    uint16_t fox; /* Faceoff spot x: limitfo clamps it, puckfaceoff2 puts it in puckx, StartPer clears it, StartPenaltyShot ... */
    uint16_t foy; /* Faceoff spot y: limitfo compares / sets it against the blue line (93S foy), puckfaceoff2 puts it in pucky, ... */
    uint16_t ScoreSumbytes[301]; /* Goal adds 6 per goal (capped at $258) and writes the goal at ScoreSum+it; ScoringSummaryScreen loops on it; ... */
    uint16_t hatx[10]; /* UpdateHatTrick: x (12.4 fixed) of each of the ten hats, plus hatvx each step, >>4 for the DrawSprite x. */
    uint16_t haty[10]; /* UpdateHatTrick: y of each hat (12.4), plus hatvy each step, used for the hat and its shadow. */
    uint16_t hatz[10]; /* UpdateHatTrick: height of each hat, lowered by hatvz until 0 (landed); subtracted from y for the hat sprite. */
    uint16_t hatvx[10]; /* UpdateHatTrick: x speed of each hat (DivX8S at launch), damped on landing. */
    uint16_t hatvy[10]; /* UpdateHatTrick: y speed of each hat, damped on landing. */
    uint16_t hatvz[10]; /* UpdateHatTrick: fall speed of each hat: +6 each step, >>2 taken from hatz (the launch clears only slot 0: ... */
    uint16_t hatstep[10]; /* UpdateHatTrick: animation step 1-3 of each hat, 0 = not launched yet; StartHatTrick clears the ten slots. */
    uint16_t hattimer[10]; /* UpdateHatTrick: frames to the hat's next animation step (reloaded from its .9 table, $3E8 once landed). */
    uint16_t hatkind[10]; /* UpdateHatTrick: kind of hat * 3 (0, 3, 6), added to the step for the SPFhat frame. */
    uint16_t hattrick; /* StartHatTrick sets 1, UpdateHatTrick runs only while it is set, puckfaceoff2 and SaveRegsStartHL2 clear it. */
    uint16_t refwindow; /* PushRef sets 1 when it draws a ref frame other than the little offsides ref (6) and clears it when it ... */
    uint16_t penmsgbox; /* prefmes sets 1 after framing the vertical penalty message and erases that box and clears it the next time; ... */
    uint16_t newperiod; /* StartGame and SetupTeamForIntermission set 1 at a period start; puckfaceoff skips the teampads line change ... */
    uint16_t pspuckback; /* pucknorm: during a penalty shot (PenShotActive) counts the frames the loose puck moves back from the goal; ... */
    uint16_t setuphome; /* setoptions sets HomeTeam ($1D logo, $FFFF while a player picture shows) like 94G GameSetUp setuphome; ... */
    uint16_t setupvis; /* As setuphome for the visitors (left) box: VisTeam (+$21 asks SetupScreen for SetupBack), $1C logo, $FFFF ... */
    uint16_t teamschanged; /* setoptions: set at entry and when a mode / team row changes; .10 then copies OptTeam1 / OptTeam2 to ... */
    uint16_t cardtimer; /* setoptions .11: counts down by 2 each pass; below 0 it is reset to $78 and the next matchup picture is ... */
    uint16_t matchuphome; /* Player (or matchup slot in matchup photo mode) whose picture DrawMatchupPictures draws in the home box, ... */
    uint16_t matchupvis; /* As matchuphome for the visitors box (94G matchupvis); set by setoptions and ShowMatchups. */
    uint16_t setupmatchup; /* setoptions: matchup shown on the game setup screen, 0-5 (GetMatchupPlayer slot) cycling to 6 = team logos; ... */
    uint16_t redrawblocks; /* Nonzero: DrawTeamBlocks / DrawSelectTeam also write the block palette and map (setoptions, SetHor, ... */
    uint16_t horblocks; /* Set while DrawHorTeams draws the blocks: DrawTeamBlocks skips the map when redrawblocks is 0, ... */
    uint16_t cardblock; /* Set while DrawPlayerCard draws its team block: DrawTeamBlocks moves the name sprites (x -8, +4 / +4). */
    uint16_t bigtextchar; /* RenderBigText: next letter tile (+2 per letter), used by the BigLetter hooks and by SendBigText / ... */
    uint16_t drawlinesback; /* LineEditor sets 1; DrawTeamScreen_title then draws DrawTeamScreen first and clears it. */
    uint16_t pausesubmenu; /* PauseEditLines / PauseShootoutSetup set it around LineEditor / ShootoutShooters run from the pause ... */
    uint16_t phototeams[2]; /* LoadMatchupPhotos: team (home +0, visitors +2) whose matchup photos are unpacked in the $7F photo buffer, ... */
    uint16_t homepicaddr; /* DrawPlayerPicture: bank $7F address of the home picture tiles ($6F00 or $5100+MatchupPhotoOffsets); ... */
    uint16_t vispicaddr; /* As homepicaddr for the visitors picture ($6900 or $2D00+MatchupPhotoOffsets). */
    uint16_t matchupphotos; /* ScoutingReport / setoptions set 1: DrawPlayerPicture takes the preloaded matchup photos ... */
    uint16_t pausemenuon; /* Pausemode_menu sets $FFFF, Pausemode / Intermission clear it after; DrawHorTeams leaves the pulsing side ... */
    uint16_t playerstatson; /* PlayerStatsScreen sets 1 while it runs; DrawMatchupPictures then skips the picture DMA. */
    uint16_t gamesstarted[33]; /* StartGame increments it right after clearing carriedy to playerstatson (count of games started; no reader found). */
    uint16_t OptPlayMode; /* Setup menu row 0 'Play Mode' (base of the 7-word option block setoptions edits; choices Regular Season .. ... */
    uint16_t OptTeam1; /* Setup row 1 'Home': setoptions copies it to HomeTeam; NewPO takes it as the user's team (93S OptTeam1). */
    uint16_t OptTeam2; /* Setup row 2 'Visitor', copied to VisTeam; FigureJoy stores the opponent here like 93S FigureJoy sta OptTeam2. */
    uint16_t OptPerlen; /* the period length option (index into GetPeriodTime_timetab) */
    uint16_t OptPen; /* penalties (0 = off) */
    uint16_t OptLine; /* setup row 5 Line Changes: 0 off, 1 on, 2 automatic (93S / 94G use 0 = on); readers test 0 or 2 */
    uint16_t OptGoalie; /* Setup row 6 'Goalie' (Automatic / Manual); StartGame copies it to goaliemode1 / goaliemode2 as 94G ... */
    uint16_t OptPlayers; /* FigureJoy regular season: index * 4 into the controller team list, as 93S FigureJoy .fjnpo lda OptPlayers; ... */
    uint16_t notdemo; /* DefaultMenus sets $FFFF, setoptions clears it for the attract demo; FigureJoy / PlayerSelect / ... */
    uint16_t pojoy; /* ReadPassBits stores the range-4 field here like 93S sta pojoy; FigureJoy adds it to the pad list index; ... */
    uint16_t postarts; /* ReadPassBits stores the range-32 field ($FFFF on a bad checksum) like 93S postarts; MakeTree, ... */
    uint8_t potree[32]; /* FigureJoy reads byte [#$34D2 + potreeteam] as 93S potree,potreeteam; PlayoffScreen compares potree,x. */
    uint16_t potreeteam; /* ReadPassBits range-16 field (93S potreeteam); NewPO, FigureJoy, EncodePW, PlayoffScreen. */
    uint8_t PassWord[20]; /* password characters */
    uint16_t passbits[8]; /* password bits, low word first */
    uint16_t tpassbits[8]; /* ResolveGames copies passbits here and EncodePW copies it back, as 93S tpassbits. */
    uint16_t gst1[8]; /* Playoff game team 1 (8 words): FigureJoy / SetScore compare and load it like 93S gst1,y. */
    uint16_t gst2[8]; /* Playoff game team 2 (93S gst2): FigureJoy, SetScore, PrintOtherScore. */
    uint16_t gspotwins[8]; /* ReadPassBits second range-5 series field per game, as 93S gspotwins. */
    uint16_t gspobwins[8]; /* ReadPassBits first range-5 series field per game, as 93S gspobwins. */
    uint16_t gsper[8]; /* InitScores clears it per game (93S sta gsper,y); SetScore, SaveRegsStartHL2. */
    uint16_t gss1[8]; /* SetScore: team 1 score (93S gss1,y). */
    uint16_t gss2[8]; /* SetScore: team 2 score (93S gss2,y). */
    uint16_t gsflags[8]; /* ReadPassBits / InitScores test and set bit 2 (series over) as 93S gsflags,y. */
    uint16_t printfontset; /* tile base of the selected print font */
    uint16_t charofs; /* Tile offset added to text map words: setupice / ExitStatsScreen set $FF00, PlayoffScreen 0; PushRef, EASNLogo, ... */
    uint16_t charofs2; /* Second tile offset, cleared with charofs by the screen setups (SetupScreen, LoadScoutingScreen, setupice) ... */
    uint16_t BigFontChars; /* setupice sets it to Framercset+4 ahead of the BigFontTiles; printbig adds it, as 93S BigFontChars. */
    uint16_t EASNcset; /* setupice: first tile of RinkLogoTiles after the faceoff tiles; EASNLogo pastes EASNLogoMap with it (93S ... */
    uint16_t cardbackchars; /* LoadPlayerCardScreen: first tile of CardBackTiles (set 0) and the map base of the card back. */
    uint16_t backchars; /* SetupScreen / LoadScoutingScreen: first BG2 tile after MenuBackTiles, where the screen background ... */
    uint16_t smallfontchars; /* vram tiles for the small font (print) */
    uint16_t centericechars; /* setupice: tile count of IceRinkTiles = first tile of the centre ice logo; LoadCenterIce DMAs the logo ... */
    uint16_t faceoffvrcset; /* LoadCenterIce sets it after the logo tiles; setupice loads FaceOffTiles there and puckfaceoff2 adds it to ... */
    uint16_t Framercset; /* vram tiles for the Framer box */
    uint16_t ninetilechars; /* SetupScreen: first tile of FramerNineTileTiles (then +$1400 palette); FramerNineTile builds its box from it. */
    uint16_t outlinechars; /* SetupScreen / LoadScoutingScreen: first tile of FramerOutlineTiles (then +$1000); FramerOutline builds its ... */
    uint16_t ExtraChars; /* setupice: first tile after TeamBlocksTiles (the last set), as 93S setupice sta ExtraChars; no reader found. */
    uint16_t nextfreechar; /* SetupScreen: running next free tile while it loads each set; CrowdMeterScreen puts CrowdMeterTiles there and ... */
    uint16_t cupchars; /* PlayoffScreen: scorechars+$70, the StanleyCupTiles after the bracket tiles; StanleyCupMap is pasted with it. */
    uint16_t refcharshor; /* setupice: first tile of RefTilesHor; PushRef uses it on the horizontal rink, as 93S refcharshor. */
    uint16_t refchars[2]; /* setupice: first tile of RefTiles; PushRef uses it on the vertical rink, as 93S refchars. */
    uint16_t scorechars; /* vram tiles for the scoreboard / team block maps */
    uint16_t blankchar; /* vram tile of the blank character (eraser fills with it) */
    uint16_t picchars; /* SetupScreen / LoadPlayerCardScreen: tile base of the two picture boxes; DrawPlayerPicture adds it to the ... */
    uint16_t playoffRoundIdx; /* playoff round for other scores and password bits */
    uint16_t gamenum; /* FigureJoy stores -1 or the user's game index and InitScores skips that game, as 93S gamenum. */
    uint16_t vcountfrac; /* NMI_Tick: fraction accumulator; its carry adds an extra vcount tick. */
    uint16_t vcountstep; /* CheckPAL: 0 on NTSC, $3333 on PAL; NMI_Tick adds it to vcountfrac each frame (60 / 50 time step). */
    uint16_t tickerstart[2]; /* Intermission saves TickerNum here; Intermission_tick plays DoHiLights when the ticker comes round to it, as ... */
    uint16_t Crowdlevel; /* crowd noise level: raised by the play, fades every frame */
    uint16_t SPCStreamLeft[2]; /* bytes of the sound stream still to send */
    uint16_t streamsfx; /* the streamed sound effect asked for (1-6, StreamSfx), 0 none */
    uint16_t streamstate; /* UpdateStreamSfx: 0 start, 1 streaming, 2 playing, 3 done */
    uint16_t streamloaded; /* the streamed sound whose sample is in sound RAM */
    uint32_t SPCStreamPtr; /* long pointer to the next stream byte (address word, bank word) */
    uint16_t replaypanel; /* ReplayMode sets $FFFF when it draws the control panel (map in rinkmapbuf); the first B press blanks it and ... */
    uint16_t usehotcold; /* GetPlayerRating / GetNameandAttrib set 1, GetPlayerBaseRating 0; CalcAttrib adds the hot / cold table only ... */
    uint16_t palbuffer[256]; /* the palette buffer (256 colours) sent to CGRAM */
    uint16_t textmap[1024]; /* the text map */
    uint16_t textmap2[1024]; /* the second text map (ClearTextMap2, SendTextMap2) */
    uint16_t hlsavebuf[896]; /* SaveRegsStartHL2 copies gmode-1 ($15D3) .. +$780 and then ScoreSumbytes here and back, as 93S StartHL2 ... */
    uint16_t bigtextbuf[33]; /* RenderBigText ORs the letter pixels in here, PlayerSelect clears $290 bytes, SendBigText / PlayoffScreen ... */
    uint8_t _pad04F36[0x573A];
    uint16_t screenmap[67]; /* PlayoffScreen map buffer (+0 width $80, +2 height $20, words from +6: bracket, team blocks, Stanley Cup); ... */
    uint8_t _pad0A6F6[0x1F80];
    uint16_t screenmap2[4096]; /* Second PlayoffScreen map ($80 x $20: +0 / +2 header, words from +6) for the BigLetterPlayoff letters, sent ... */
    uint8_t ReplayBufferEnd[6]; /* replay buffer end */
    uint8_t OAMHiMasks[2436]; /* Reset_Fast copies OAMHighMasks (16 bytes) here; DrawSprite / DrawSpritePieces AND with it (x indexed). */
    uint16_t titleteams[2048]; /* ShuffleTitleTeams fills it with the 26 team numbers in random order; TitleScreen / NextTitleTeam read word ... */
    uint8_t _pad10000[0x4000];
    uint16_t introbuf[24]; /* IntroSequence work buffer (it clears $4000 bytes from here): EALogoMap unpacked here (map words at +6, ... */
    uint8_t _pad14030[0x26D0];
    uint8_t brackettiles[3584]; /* PlayoffScreen draws the $70 bracket tiles (bytes $FF / $80 / $7F / $00) here and DMAs $E00 bytes to VRAM. */
    uint16_t colourbuf[256]; /* The colour buffer SetColours fills and SendColours sends (256 colours) on the menu screens; PlayoffScreen ... */
    uint16_t logoboxmap[64]; /* SetupScreen pastes the 7 x 7 part of MenuBackMap here (header 7, 7 at +0 / +2, words from +6); ... */
    uint16_t picboxmap[1088]; /* LoadPictureMap / LoadPlayerCardScreen unpack the 7 x 7 PictureMap here (+0 width, +2 height, words from ... */
    uint16_t titlescale[8]; /* BuildTitlePath: mode 7 scale of each of the $500 title logo path steps; TitleScreen reads it. */
    uint8_t introchars[2544]; /* IntroSequence unpacks EALogoTiles here and sends them as the mode 7 character bytes (VMDATAH); the same ... */
    uint8_t titlexofs[2560]; /* BuildTitlePath: x offset of each title path step; TitleScreen adds $190. */
    uint8_t titleyscroll[3072]; /* BuildTitlePath: y scroll of each title path step, read by TitleScreen. */
    uint8_t titlelogobuf[44]; /* LoadTitleLogo: mode 7 buffer of the title team logo (header +0 / +2 / +4, then the map; pixel pairs at +0 ... */
    uint8_t _pad1A02C[0x5FD4];
};
#pragma pack(pop)

_Static_assert(sizeof(struct G) == WRAM_SIZE, "struct G is the whole of WRAM");
_Static_assert(offsetof(struct G, r0) == 0x00000, "r0");
_Static_assert(offsetof(struct G, r1) == 0x00004, "r1");
_Static_assert(offsetof(struct G, r2) == 0x00008, "r2");
_Static_assert(offsetof(struct G, p0) == 0x0000C, "p0");
_Static_assert(offsetof(struct G, p1) == 0x00010, "p1");
_Static_assert(offsetof(struct G, r3) == 0x00014, "r3");
_Static_assert(offsetof(struct G, r4) == 0x00018, "r4");
_Static_assert(offsetof(struct G, r5) == 0x0001C, "r5");
_Static_assert(offsetof(struct G, r6) == 0x00020, "r6");
_Static_assert(offsetof(struct G, r7) == 0x00024, "r7");
_Static_assert(offsetof(struct G, r8) == 0x00028, "r8");
_Static_assert(offsetof(struct G, r9) == 0x0002C, "r9");
_Static_assert(offsetof(struct G, ClipTopUnused) == 0x00030, "ClipTopUnused");
_Static_assert(offsetof(struct G, ClipBottomUnused) == 0x00032, "ClipBottomUnused");
_Static_assert(offsetof(struct G, SoundTable) == 0x00034, "SoundTable");
_Static_assert(offsetof(struct G, NMISkip) == 0x00038, "NMISkip");
_Static_assert(offsetof(struct G, DMAQueueHead) == 0x0003A, "DMAQueueHead");
_Static_assert(offsetof(struct G, DMAQueueTail) == 0x0003C, "DMAQueueTail");
_Static_assert(offsetof(struct G, MulCand) == 0x0003E, "MulCand");
_Static_assert(offsetof(struct G, StreamLoop) == 0x00042, "StreamLoop");
_Static_assert(offsetof(struct G, MulFracResult) == 0x00043, "MulFracResult");
_Static_assert(offsetof(struct G, MulPlier) == 0x00046, "MulPlier");
_Static_assert(offsetof(struct G, CurTrack) == 0x0004A, "CurTrack");
_Static_assert(offsetof(struct G, CurVoice) == 0x0004B, "CurVoice");
_Static_assert(offsetof(struct G, CurTrackPtr) == 0x0004C, "CurTrackPtr");
_Static_assert(offsetof(struct G, InstPtr) == 0x0004E, "InstPtr");
_Static_assert(offsetof(struct G, SoundBank) == 0x00050, "SoundBank");
_Static_assert(offsetof(struct G, VarLenAcc) == 0x00052, "VarLenAcc");
_Static_assert(offsetof(struct G, VarLenByte) == 0x00054, "VarLenByte");
_Static_assert(offsetof(struct G, EventPtr) == 0x00056, "EventPtr");
_Static_assert(offsetof(struct G, GainWord) == 0x0005A, "GainWord");
_Static_assert(offsetof(struct G, DSPGain) == 0x0005B, "DSPGain");
_Static_assert(offsetof(struct G, BendAmount) == 0x0005C, "BendAmount");
_Static_assert(offsetof(struct G, EventData2) == 0x0005E, "EventData2");
_Static_assert(offsetof(struct G, SendPitch) == 0x00060, "SendPitch");
_Static_assert(offsetof(struct G, EventEAUnused) == 0x00062, "EventEAUnused");
_Static_assert(offsetof(struct G, SfxVelocity) == 0x00064, "SfxVelocity");
_Static_assert(offsetof(struct G, VoiceRotor) == 0x00066, "VoiceRotor");
_Static_assert(offsetof(struct G, StealCount) == 0x00068, "StealCount");
_Static_assert(offsetof(struct G, HuffBitsExt) == 0x0006A, "HuffBitsExt");
_Static_assert(offsetof(struct G, LZCopySrc) == 0x0006B, "LZCopySrc");
_Static_assert(offsetof(struct G, HuffBits) == 0x0006C, "HuffBits");
_Static_assert(offsetof(struct G, LZOutAddr) == 0x0006F, "LZOutAddr");
_Static_assert(offsetof(struct G, LZCmd) == 0x00071, "LZCmd");
_Static_assert(offsetof(struct G, LZCmdByte2) == 0x00073, "LZCmdByte2");
_Static_assert(offsetof(struct G, HuffEscapeLen) == 0x00074, "HuffEscapeLen");
_Static_assert(offsetof(struct G, LZCmdByte3) == 0x00075, "LZCmdByte3");
_Static_assert(offsetof(struct G, LZLenLo) == 0x00077, "LZLenLo");
_Static_assert(offsetof(struct G, LZSrcIdx) == 0x00079, "LZSrcIdx");
_Static_assert(offsetof(struct G, LZDistance) == 0x0007B, "LZDistance");
_Static_assert(offsetof(struct G, HuffSpan) == 0x0007D, "HuffSpan");
_Static_assert(offsetof(struct G, crowdhold) == 0x00081, "crowdhold");
_Static_assert(offsetof(struct G, vcount) == 0x00083, "vcount");
_Static_assert(offsetof(struct G, xsave) == 0x00087, "xsave");
_Static_assert(offsetof(struct G, a0) == 0x00089, "a0");
_Static_assert(offsetof(struct G, a1) == 0x0008D, "a1");
_Static_assert(offsetof(struct G, a2) == 0x00091, "a2");
_Static_assert(offsetof(struct G, a3) == 0x00095, "a3");
_Static_assert(offsetof(struct G, a4) == 0x00099, "a4");
_Static_assert(offsetof(struct G, a5) == 0x0009D, "a5");
_Static_assert(offsetof(struct G, d0) == 0x000A5, "d0");
_Static_assert(offsetof(struct G, d1) == 0x000A9, "d1");
_Static_assert(offsetof(struct G, d2) == 0x000AD, "d2");
_Static_assert(offsetof(struct G, d3) == 0x000B1, "d3");
_Static_assert(offsetof(struct G, d4) == 0x000B5, "d4");
_Static_assert(offsetof(struct G, d5) == 0x000B9, "d5");
_Static_assert(offsetof(struct G, d6) == 0x000BD, "d6");
_Static_assert(offsetof(struct G, d7) == 0x000C1, "d7");
_Static_assert(offsetof(struct G, introHookFlags) == 0x000C3, "introHookFlags");
_Static_assert(offsetof(struct G, tmp0) == 0x000C5, "tmp0");
_Static_assert(offsetof(struct G, tmp1) == 0x000C7, "tmp1");
_Static_assert(offsetof(struct G, tmp2) == 0x000C9, "tmp2");
_Static_assert(offsetof(struct G, tmp3) == 0x000CB, "tmp3");
_Static_assert(offsetof(struct G, tmp4) == 0x000CD, "tmp4");
_Static_assert(offsetof(struct G, tmp5) == 0x000CF, "tmp5");
_Static_assert(offsetof(struct G, introPal2Src) == 0x000D1, "introPal2Src");
_Static_assert(offsetof(struct G, introBG1H) == 0x000D5, "introBG1H");
_Static_assert(offsetof(struct G, cornerdy) == 0x000D7, "cornerdy");
_Static_assert(offsetof(struct G, introM7A) == 0x000D9, "introM7A");
_Static_assert(offsetof(struct G, introM7D) == 0x000DB, "introM7D");
_Static_assert(offsetof(struct G, mulres) == 0x000DD, "mulres");
_Static_assert(offsetof(struct G, curobj) == 0x000E4, "curobj");
_Static_assert(offsetof(struct G, PlayersUnused) == 0x000E6, "PlayersUnused");
_Static_assert(offsetof(struct G, curobjteam) == 0x000EA, "curobjteam");
_Static_assert(offsetof(struct G, curobjopp) == 0x000EC, "curobjopp");
_Static_assert(offsetof(struct G, DMAQueue) == 0x00100, "DMAQueue");
_Static_assert(offsetof(struct G, DecompTab1) == 0x00500, "DecompTab1");
_Static_assert(offsetof(struct G, DecompTab2) == 0x00600, "DecompTab2");
_Static_assert(offsetof(struct G, HuffCount) == 0x00700, "HuffCount");
_Static_assert(offsetof(struct G, HuffOffset) == 0x00720, "HuffOffset");
_Static_assert(offsetof(struct G, HuffLimit) == 0x00740, "HuffLimit");
_Static_assert(offsetof(struct G, HuffLongVec) == 0x00760, "HuffLongVec");
_Static_assert(offsetof(struct G, IniDispShadow) == 0x00762, "IniDispShadow");
_Static_assert(offsetof(struct G, FrameCount) == 0x00764, "FrameCount");
_Static_assert(offsetof(struct G, OAMReady) == 0x00766, "OAMReady");
_Static_assert(offsetof(struct G, CGDMALen) == 0x00768, "CGDMALen");
_Static_assert(offsetof(struct G, CGDMAColour) == 0x0076A, "CGDMAColour");
_Static_assert(offsetof(struct G, CGDMASrc) == 0x0076C, "CGDMASrc");
_Static_assert(offsetof(struct G, CGDMABank) == 0x0076E, "CGDMABank");
_Static_assert(offsetof(struct G, FrameHookNext) == 0x00770, "FrameHookNext");
_Static_assert(offsetof(struct G, NMIHookNext) == 0x00773, "NMIHookNext");
_Static_assert(offsetof(struct G, JoyPad) == 0x00776, "JoyPad");
_Static_assert(offsetof(struct G, Multi5Sig) == 0x00781, "Multi5Sig");
_Static_assert(offsetof(struct G, FrameCountHold) == 0x00787, "FrameCountHold");
_Static_assert(offsetof(struct G, FrameHook) == 0x00789, "FrameHook");
_Static_assert(offsetof(struct G, NMIHook) == 0x0078C, "NMIHook");
_Static_assert(offsetof(struct G, IRQHook) == 0x0078F, "IRQHook");
_Static_assert(offsetof(struct G, seed) == 0x00792, "seed");
_Static_assert(offsetof(struct G, RAMTag) == 0x00796, "RAMTag");
_Static_assert(offsetof(struct G, MapVRAM) == 0x0079C, "MapVRAM");
_Static_assert(offsetof(struct G, MapScreenStep) == 0x0079E, "MapScreenStep");
_Static_assert(offsetof(struct G, HeapStart7E) == 0x007A0, "HeapStart7E");
_Static_assert(offsetof(struct G, HeapStart7F) == 0x007A2, "HeapStart7F");
_Static_assert(offsetof(struct G, NewHandleSlot) == 0x007A4, "NewHandleSlot");
_Static_assert(offsetof(struct G, NewHandleSkipBank) == 0x007A6, "NewHandleSkipBank");
_Static_assert(offsetof(struct G, OAMLastEnd) == 0x007A8, "OAMLastEnd");
_Static_assert(offsetof(struct G, OAMDrawBuf) == 0x007AA, "OAMDrawBuf");
_Static_assert(offsetof(struct G, OAMShowBuf) == 0x007AC, "OAMShowBuf");
_Static_assert(offsetof(struct G, SpritesLeft) == 0x007AE, "SpritesLeft");
_Static_assert(offsetof(struct G, OAMHiPtr) == 0x007B0, "OAMHiPtr");
_Static_assert(offsetof(struct G, OAMHiShift) == 0x007B2, "OAMHiShift");
_Static_assert(offsetof(struct G, SpriteVRAM) == 0x007B4, "SpriteVRAM");
_Static_assert(offsetof(struct G, SpritesReserved) == 0x007B6, "SpritesReserved");
_Static_assert(offsetof(struct G, SpriteBlockTiles) == 0x007B8, "SpriteBlockTiles");
_Static_assert(offsetof(struct G, FrameCacheCount) == 0x007BA, "FrameCacheCount");
_Static_assert(offsetof(struct G, OAMNext) == 0x007BC, "OAMNext");
_Static_assert(offsetof(struct G, OAMHideEnd) == 0x007BE, "OAMHideEnd");
_Static_assert(offsetof(struct G, FrameTilesFail) == 0x007C0, "FrameTilesFail");
_Static_assert(offsetof(struct G, CacheUseMark) == 0x007C2, "CacheUseMark");
_Static_assert(offsetof(struct G, DrawFrameNum) == 0x007C4, "DrawFrameNum");
_Static_assert(offsetof(struct G, FrameByNumber) == 0x007C6, "FrameByNumber");
_Static_assert(offsetof(struct G, PieceX) == 0x007C8, "PieceX");
_Static_assert(offsetof(struct G, PieceY) == 0x007CA, "PieceY");
_Static_assert(offsetof(struct G, PieceTile) == 0x007CC, "PieceTile");
_Static_assert(offsetof(struct G, PieceSize) == 0x007CE, "PieceSize");
_Static_assert(offsetof(struct G, PieceNext) == 0x007D0, "PieceNext");
_Static_assert(offsetof(struct G, IRQVTime) == 0x007D2, "IRQVTime");
_Static_assert(offsetof(struct G, IRQHTime) == 0x007D4, "IRQHTime");
_Static_assert(offsetof(struct G, BG1HScroll) == 0x007D6, "BG1HScroll");
_Static_assert(offsetof(struct G, BG1VScroll) == 0x007D8, "BG1VScroll");
_Static_assert(offsetof(struct G, BG2HScroll) == 0x007DA, "BG2HScroll");
_Static_assert(offsetof(struct G, BG2VScroll) == 0x007DC, "BG2VScroll");
_Static_assert(offsetof(struct G, BG3HScroll) == 0x007DE, "BG3HScroll");
_Static_assert(offsetof(struct G, BG3VScroll) == 0x007E0, "BG3VScroll");
_Static_assert(offsetof(struct G, Mode7InUse) == 0x007E2, "Mode7InUse");
_Static_assert(offsetof(struct G, SoundMono) == 0x007E3, "SoundMono");
_Static_assert(offsetof(struct G, SoundLock) == 0x007E5, "SoundLock");
_Static_assert(offsetof(struct G, EchoVoices) == 0x007E7, "EchoVoices");
_Static_assert(offsetof(struct G, NoiseVoices) == 0x007E8, "NoiseVoices");
_Static_assert(offsetof(struct G, PitchModVoices) == 0x007E9, "PitchModVoices");
_Static_assert(offsetof(struct G, EchoDelay) == 0x007EA, "EchoDelay");
_Static_assert(offsetof(struct G, EchoVolume) == 0x007EB, "EchoVolume");
_Static_assert(offsetof(struct G, EchoFeedback) == 0x007EC, "EchoFeedback");
_Static_assert(offsetof(struct G, SongPtr) == 0x007ED, "SongPtr");
_Static_assert(offsetof(struct G, SongUnused) == 0x007EF, "SongUnused");
_Static_assert(offsetof(struct G, MusicPause) == 0x007F1, "MusicPause");
_Static_assert(offsetof(struct G, MusicOn) == 0x007F3, "MusicOn");
_Static_assert(offsetof(struct G, TempoAcc) == 0x007F5, "TempoAcc");
_Static_assert(offsetof(struct G, TempoTick) == 0x007F6, "TempoTick");
_Static_assert(offsetof(struct G, SoundVolume) == 0x007F7, "SoundVolume");
_Static_assert(offsetof(struct G, SoundFadeSpeed) == 0x007F9, "SoundFadeSpeed");
_Static_assert(offsetof(struct G, InstVolL) == 0x007FB, "InstVolL");
_Static_assert(offsetof(struct G, VoiceVolL) == 0x007FC, "VoiceVolL");
_Static_assert(offsetof(struct G, InstVolR) == 0x007FD, "InstVolR");
_Static_assert(offsetof(struct G, VoiceVolR) == 0x007FE, "VoiceVolR");
_Static_assert(offsetof(struct G, CurTrackOfs) == 0x007FF, "CurTrackOfs");
_Static_assert(offsetof(struct G, VoiceOfs) == 0x00800, "VoiceOfs");
_Static_assert(offsetof(struct G, TrackCount) == 0x00801, "TrackCount");
_Static_assert(offsetof(struct G, MusicDuck) == 0x00802, "MusicDuck");
_Static_assert(offsetof(struct G, TrackPtr) == 0x00804, "TrackPtr");
_Static_assert(offsetof(struct G, TrackLoop) == 0x00826, "TrackLoop");
_Static_assert(offsetof(struct G, TrackWait) == 0x00848, "TrackWait");
_Static_assert(offsetof(struct G, TrackInst) == 0x0086A, "TrackInst");
_Static_assert(offsetof(struct G, TrackBend) == 0x0088C, "TrackBend");
_Static_assert(offsetof(struct G, TrackBendRange) == 0x0089D, "TrackBendRange");
_Static_assert(offsetof(struct G, TrackVolL) == 0x008AE, "TrackVolL");
_Static_assert(offsetof(struct G, TrackVolR) == 0x008BF, "TrackVolR");
_Static_assert(offsetof(struct G, TrackSustain) == 0x008D0, "TrackSustain");
_Static_assert(offsetof(struct G, TrackUnused) == 0x008E1, "TrackUnused");
_Static_assert(offsetof(struct G, VoiceState) == 0x008F2, "VoiceState");
_Static_assert(offsetof(struct G, VoiceTrack) == 0x008FA, "VoiceTrack");
_Static_assert(offsetof(struct G, VoiceVelocity) == 0x00902, "VoiceVelocity");
_Static_assert(offsetof(struct G, VoiceNote) == 0x0090A, "VoiceNote");
_Static_assert(offsetof(struct G, VoicePriority) == 0x00912, "VoicePriority");
_Static_assert(offsetof(struct G, VoiceLength) == 0x0091A, "VoiceLength");
_Static_assert(offsetof(struct G, VoiceInst) == 0x0092A, "VoiceInst");
_Static_assert(offsetof(struct G, VoiceGain) == 0x0093A, "VoiceGain");
_Static_assert(offsetof(struct G, VoicePitch) == 0x0094A, "VoicePitch");
_Static_assert(offsetof(struct G, VoiceBasePitch) == 0x0095A, "VoiceBasePitch");
_Static_assert(offsetof(struct G, VoiceAge) == 0x0096A, "VoiceAge");
_Static_assert(offsetof(struct G, VoiceVibCount) == 0x0097A, "VoiceVibCount");
_Static_assert(offsetof(struct G, VoiceVibOfs) == 0x0098A, "VoiceVibOfs");
_Static_assert(offsetof(struct G, VoiceVibRate) == 0x0099A, "VoiceVibRate");
_Static_assert(offsetof(struct G, VoiceVibRateFrac) == 0x009AA, "VoiceVibRateFrac");
_Static_assert(offsetof(struct G, VoiceVibDepth) == 0x009BA, "VoiceVibDepth");
_Static_assert(offsetof(struct G, VoiceVibDepthFrac) == 0x009CA, "VoiceVibDepthFrac");
_Static_assert(offsetof(struct G, VoiceVibSpeed) == 0x009DA, "VoiceVibSpeed");
_Static_assert(offsetof(struct G, VoiceVibSpeedFrac) == 0x009EA, "VoiceVibSpeedFrac");
_Static_assert(offsetof(struct G, VoiceVibDir) == 0x009FA, "VoiceVibDir");
_Static_assert(offsetof(struct G, RemapOut) == 0x00A0A, "RemapOut");
_Static_assert(offsetof(struct G, mesarea) == 0x00A2A, "mesarea");
_Static_assert(offsetof(struct G, PWCharStr) == 0x00A4A, "PWCharStr");
_Static_assert(offsetof(struct G, PushWidthBuf) == 0x00A8E, "PushWidthBuf");
_Static_assert(offsetof(struct G, gamefps) == 0x00A98, "gamefps");
_Static_assert(offsetof(struct G, padlast) == 0x00A9A, "padlast");
_Static_assert(offsetof(struct G, paddir) == 0x00AA4, "paddir");
_Static_assert(offsetof(struct G, padchange) == 0x00AAE, "padchange");
_Static_assert(offsetof(struct G, padnew) == 0x00AB8, "padnew");
_Static_assert(offsetof(struct G, padchangeany) == 0x00AC2, "padchangeany");
_Static_assert(offsetof(struct G, repeatdelay) == 0x00AC4, "repeatdelay");
_Static_assert(offsetof(struct G, pausepad) == 0x00AC6, "pausepad");
_Static_assert(offsetof(struct G, pausepadidle) == 0x00AC8, "pausepadidle");
_Static_assert(offsetof(struct G, mp5padsig) == 0x00ACA, "mp5padsig");
_Static_assert(offsetof(struct G, bosgames) == 0x00ACF, "bosgames");
_Static_assert(offsetof(struct G, fadestep) == 0x00AD1, "fadestep");
_Static_assert(offsetof(struct G, brightness) == 0x00AD3, "brightness");
_Static_assert(offsetof(struct G, WinBits) == 0x00AD5, "WinBits");
_Static_assert(offsetof(struct G, mp5readall) == 0x00AD7, "mp5readall");
_Static_assert(offsetof(struct G, puckc) == 0x00AD9, "puckc");
_Static_assert(offsetof(struct G, timestep) == 0x00ADB, "timestep");
_Static_assert(offsetof(struct G, padinputdone) == 0x00ADD, "padinputdone");
_Static_assert(offsetof(struct G, PadControlObj) == 0x00AE7, "PadControlObj");
_Static_assert(offsetof(struct G, fopadfirst) == 0x00AF5, "fopadfirst");
_Static_assert(offsetof(struct G, padsitnext) == 0x00AF9, "padsitnext");
_Static_assert(offsetof(struct G, puckpadnext) == 0x00AFD, "puckpadnext");
_Static_assert(offsetof(struct G, passpad) == 0x00B01, "passpad");
_Static_assert(offsetof(struct G, goalieflags) == 0x00B05, "goalieflags");
_Static_assert(offsetof(struct G, puckdist) == 0x00B1D, "puckdist");
_Static_assert(offsetof(struct G, puckdist2) == 0x00B3D, "puckdist2");
_Static_assert(offsetof(struct G, puckdir) == 0x00B5D, "puckdir");
_Static_assert(offsetof(struct G, asscur) == 0x00B7D, "asscur");
_Static_assert(offsetof(struct G, topgoalie) == 0x00B9D, "topgoalie");
_Static_assert(offsetof(struct G, botgoalie) == 0x00B9F, "botgoalie");
_Static_assert(offsetof(struct G, goaliestopped) == 0x00BA1, "goaliestopped");
_Static_assert(offsetof(struct G, dumpinplayer) == 0x00BA3, "dumpinplayer");
_Static_assert(offsetof(struct G, dumpintarget) == 0x00BA5, "dumpintarget");
_Static_assert(offsetof(struct G, passtargetx) == 0x00BA7, "passtargetx");
_Static_assert(offsetof(struct G, passtargety) == 0x00BA9, "passtargety");
_Static_assert(offsetof(struct G, onetimerreq) == 0x00BAB, "onetimerreq");
_Static_assert(offsetof(struct G, passwait) == 0x00BAD, "passwait");
_Static_assert(offsetof(struct G, lastshotspeed) == 0x00BC5, "lastshotspeed");
_Static_assert(offsetof(struct G, GoalieUnused) == 0x00BC9, "GoalieUnused");
_Static_assert(offsetof(struct G, TitleWH1) == 0x00BCB, "TitleWH1");
_Static_assert(offsetof(struct G, TitleWH3) == 0x00BCD, "TitleWH3");
_Static_assert(offsetof(struct G, TitleWH3Low) == 0x00BCF, "TitleWH3Low");
_Static_assert(offsetof(struct G, TitleBGMode) == 0x00BD1, "TitleBGMode");
_Static_assert(offsetof(struct G, TitleM7TM) == 0x00BD3, "TitleM7TM");
_Static_assert(offsetof(struct G, TitleTeamIdx) == 0x00BD5, "TitleTeamIdx");
_Static_assert(offsetof(struct G, CreditStep) == 0x00BD9, "CreditStep");
_Static_assert(offsetof(struct G, CreditTime) == 0x00BDB, "CreditTime");
_Static_assert(offsetof(struct G, CreditTitleIdx) == 0x00BDD, "CreditTitleIdx");
_Static_assert(offsetof(struct G, CreditTitleY) == 0x00BDF, "CreditTitleY");
_Static_assert(offsetof(struct G, CreditNameIdx) == 0x00BE1, "CreditNameIdx");
_Static_assert(offsetof(struct G, CreditNameY) == 0x00BE3, "CreditNameY");
_Static_assert(offsetof(struct G, LogoSrcOfs) == 0x00BE5, "LogoSrcOfs");
_Static_assert(offsetof(struct G, LogoDstOfs) == 0x00BE7, "LogoDstOfs");
_Static_assert(offsetof(struct G, titleskip) == 0x00BE9, "titleskip");
_Static_assert(offsetof(struct G, goalieturnwait) == 0x00BCB, "goalieturnwait");
_Static_assert(offsetof(struct G, TitleSkipMask) == 0x00BEB, "TitleSkipMask");
_Static_assert(offsetof(struct G, LogoPalBits) == 0x00BED, "LogoPalBits");
_Static_assert(offsetof(struct G, TitlePathPos) == 0x00BEF, "TitlePathPos");
_Static_assert(offsetof(struct G, TitlePathFrac) == 0x00BF1, "TitlePathFrac");
_Static_assert(offsetof(struct G, TitleM7X) == 0x00BF3, "TitleM7X");
_Static_assert(offsetof(struct G, TitleM7Y) == 0x00BF5, "TitleM7Y");
_Static_assert(offsetof(struct G, TitleM7HOfs) == 0x00BF7, "TitleM7HOfs");
_Static_assert(offsetof(struct G, TitleM7VOfs) == 0x00BF9, "TitleM7VOfs");
_Static_assert(offsetof(struct G, TitleM7Scale) == 0x00BFB, "TitleM7Scale");
_Static_assert(offsetof(struct G, TitlePathFlip) == 0x00BFD, "TitlePathFlip");
_Static_assert(offsetof(struct G, TitleLogoFlags) == 0x00BFF, "TitleLogoFlags");
_Static_assert(offsetof(struct G, goaliedivewait) == 0x00BEB, "goaliedivewait");
_Static_assert(offsetof(struct G, FaceoffUnused) == 0x00C0B, "FaceoffUnused");
_Static_assert(offsetof(struct G, benchcheer) == 0x00C23, "benchcheer");
_Static_assert(offsetof(struct G, benchrounds) == 0x00C25, "benchrounds");
_Static_assert(offsetof(struct G, benchtimer) == 0x00C27, "benchtimer");
_Static_assert(offsetof(struct G, benchframe) == 0x00C29, "benchframe");
_Static_assert(offsetof(struct G, crowdanim) == 0x00C2B, "crowdanim");
_Static_assert(offsetof(struct G, crowdanimstep) == 0x00C55, "crowdanimstep");
_Static_assert(offsetof(struct G, crowdanimtime) == 0x00C7F, "crowdanimtime");
_Static_assert(offsetof(struct G, replaydelay) == 0x00CA9, "replaydelay");
_Static_assert(offsetof(struct G, replaylock) == 0x00CAB, "replaylock");
_Static_assert(offsetof(struct G, recsfx) == 0x00CAD, "recsfx");
_Static_assert(offsetof(struct G, recsfxpitch) == 0x00CAF, "recsfxpitch");
_Static_assert(offsetof(struct G, recsfxvol) == 0x00CB1, "recsfxvol");
_Static_assert(offsetof(struct G, recsfx2) == 0x00CB3, "recsfx2");
_Static_assert(offsetof(struct G, recsfxpitch2) == 0x00CB5, "recsfxpitch2");
_Static_assert(offsetof(struct G, recsfxvol2) == 0x00CB7, "recsfxvol2");
_Static_assert(offsetof(struct G, replaynewframe) == 0x00CB9, "replaynewframe");
_Static_assert(offsetof(struct G, replaymusic) == 0x00CBB, "replaymusic");
_Static_assert(offsetof(struct G, replayHpos) == 0x00CBD, "replayHpos");
_Static_assert(offsetof(struct G, replayVpos) == 0x00CBF, "replayVpos");
_Static_assert(offsetof(struct G, percue) == 0x00CC1, "percue");
_Static_assert(offsetof(struct G, cursong) == 0x00CC3, "cursong");
_Static_assert(offsetof(struct G, sprhflip) == 0x00CC5, "sprhflip");
_Static_assert(offsetof(struct G, reloadtime) == 0x00CC7, "reloadtime");
_Static_assert(offsetof(struct G, sprattr) == 0x00CC9, "sprattr");
_Static_assert(offsetof(struct G, sprpriority) == 0x00CCB, "sprpriority");
_Static_assert(offsetof(struct G, sprframe) == 0x00CCD, "sprframe");
_Static_assert(offsetof(struct G, sprx) == 0x00CCF, "sprx");
_Static_assert(offsetof(struct G, spry) == 0x00CD1, "spry");
_Static_assert(offsetof(struct G, sprvflip) == 0x00CD3, "sprvflip");
_Static_assert(offsetof(struct G, textdirtylo) == 0x00CD5, "textdirtylo");
_Static_assert(offsetof(struct G, textdirtyhi) == 0x00CD7, "textdirtyhi");
_Static_assert(offsetof(struct G, printm) == 0x00CD9, "printm");
_Static_assert(offsetof(struct G, collflag) == 0x00CDB, "collflag");
_Static_assert(offsetof(struct G, impactstep) == 0x00CDD, "impactstep");
_Static_assert(offsetof(struct G, velstep) == 0x00CDF, "velstep");
_Static_assert(offsetof(struct G, gravstep) == 0x00CE1, "gravstep");
_Static_assert(offsetof(struct G, pausetimer) == 0x00CE3, "pausetimer");
_Static_assert(offsetof(struct G, ThirdPBonus) == 0x00CE5, "ThirdPBonus");
_Static_assert(offsetof(struct G, cheercnt) == 0x00CE7, "cheercnt");
_Static_assert(offsetof(struct G, cheervc) == 0x00CE9, "cheervc");
_Static_assert(offsetof(struct G, Hpos) == 0x00CEB, "Hpos");
_Static_assert(offsetof(struct G, Hscroll) == 0x00CED, "Hscroll");
_Static_assert(offsetof(struct G, HmAwBonus) == 0x00CEF, "HmAwBonus");
_Static_assert(offsetof(struct G, iflags) == 0x00CF1, "iflags");
_Static_assert(offsetof(struct G, lastplayer) == 0x00CF3, "lastplayer");
_Static_assert(offsetof(struct G, lldisp) == 0x00CF7, "lldisp");
_Static_assert(offsetof(struct G, nibblebuffer) == 0x00CF9, "nibblebuffer");
_Static_assert(offsetof(struct G, oldvcount) == 0x00D01, "oldvcount");
_Static_assert(offsetof(struct G, passdir) == 0x00D03, "passdir");
_Static_assert(offsetof(struct G, passspeed) == 0x00D05, "passspeed");
_Static_assert(offsetof(struct G, passReceiverPlayerNum) == 0x00D07, "passReceiverPlayerNum");
_Static_assert(offsetof(struct G, PKBonus) == 0x00D09, "PKBonus");
_Static_assert(offsetof(struct G, PPBonus) == 0x00D0B, "PPBonus");
_Static_assert(offsetof(struct G, printa) == 0x00D0D, "printa");
_Static_assert(offsetof(struct G, printx) == 0x00D0F, "printx");
_Static_assert(offsetof(struct G, printy) == 0x00D11, "printy");
_Static_assert(offsetof(struct G, psfirst) == 0x00D13, "psfirst");
_Static_assert(offsetof(struct G, psitem) == 0x00D15, "psitem");
_Static_assert(offsetof(struct G, puckcross) == 0x00D17, "puckcross");
_Static_assert(offsetof(struct G, ReplayBufferPtr) == 0x00D1F, "ReplayBufferPtr");
_Static_assert(offsetof(struct G, Oldrow) == 0x00D23, "Oldrow");
_Static_assert(offsetof(struct G, vramrow) == 0x00D25, "vramrow");
_Static_assert(offsetof(struct G, shotplayer) == 0x00D29, "shotplayer");
_Static_assert(offsetof(struct G, sfxmutetime) == 0x00D2B, "sfxmutetime");
_Static_assert(offsetof(struct G, threat) == 0x00D2D, "threat");
_Static_assert(offsetof(struct G, demoflag) == 0x00D2F, "demoflag");
_Static_assert(offsetof(struct G, TickerNum) == 0x00D31, "TickerNum");
_Static_assert(offsetof(struct G, tickervc) == 0x00D33, "tickervc");
_Static_assert(offsetof(struct G, lastframevc) == 0x00D35, "lastframevc");
_Static_assert(offsetof(struct G, Vpos) == 0x00D37, "Vpos");
_Static_assert(offsetof(struct G, Vscroll) == 0x00D39, "Vscroll");
_Static_assert(offsetof(struct G, wcradiusx) == 0x00D3B, "wcradiusx");
_Static_assert(offsetof(struct G, wcradiusy) == 0x00D3D, "wcradiusy");
_Static_assert(offsetof(struct G, PlayerScrollCtr) == 0x00D3F, "PlayerScrollCtr");
_Static_assert(offsetof(struct G, screentimer) == 0x00D41, "screentimer");
_Static_assert(offsetof(struct G, SelectedPlayerIdx) == 0x00D43, "SelectedPlayerIdx");
_Static_assert(offsetof(struct G, ScoutingReportTimer) == 0x00D45, "ScoutingReportTimer");
_Static_assert(offsetof(struct G, DispAttribCtr) == 0x00D47, "DispAttribCtr");
_Static_assert(offsetof(struct G, VertLineScrolling) == 0x00D49, "VertLineScrolling");
_Static_assert(offsetof(struct G, xc1) == 0x00D4B, "xc1");
_Static_assert(offsetof(struct G, yc1) == 0x00D4D, "yc1");
_Static_assert(offsetof(struct G, yleader) == 0x00D4F, "yleader");
_Static_assert(offsetof(struct G, vcountmark) == 0x00D51, "vcountmark");
_Static_assert(offsetof(struct G, Xpos) == 0x00D55, "Xpos");
_Static_assert(offsetof(struct G, Xfrac) == 0x00D77, "Xfrac");
_Static_assert(offsetof(struct G, frame) == 0x00D97, "frame");
_Static_assert(offsetof(struct G, Ypos) == 0x00DB7, "Ypos");
_Static_assert(offsetof(struct G, pucky) == 0x00DD3, "pucky");
_Static_assert(offsetof(struct G, Yfrac) == 0x00DD9, "Yfrac");
_Static_assert(offsetof(struct G, puckz) == 0x00DF9, "puckz");
_Static_assert(offsetof(struct G, shadowz) == 0x00DFB, "shadowz");
_Static_assert(offsetof(struct G, puckzfrac) == 0x00DFD, "puckzfrac");
_Static_assert(offsetof(struct G, puckoldz) == 0x00DFF, "puckoldz");
_Static_assert(offsetof(struct G, puckvz) == 0x00E01, "puckvz");
_Static_assert(offsetof(struct G, attribute) == 0x00E03, "attribute");
_Static_assert(offsetof(struct G, oldframe) == 0x00E23, "oldframe");
_Static_assert(offsetof(struct G, OldXpos) == 0x00E43, "OldXpos");
_Static_assert(offsetof(struct G, OldXfrac) == 0x00E63, "OldXfrac");
_Static_assert(offsetof(struct G, OldYpos) == 0x00E83, "OldYpos");
_Static_assert(offsetof(struct G, OldYfrac) == 0x00EA3, "OldYfrac");
_Static_assert(offsetof(struct G, Xvel) == 0x00EC3, "Xvel");
_Static_assert(offsetof(struct G, puckvx) == 0x00EDF, "puckvx");
_Static_assert(offsetof(struct G, Yvel) == 0x00EE3, "Yvel");
_Static_assert(offsetof(struct G, puckvy) == 0x00EFF, "puckvy");
_Static_assert(offsetof(struct G, impactp) == 0x00F03, "impactp");
_Static_assert(offsetof(struct G, impact) == 0x00F23, "impact");
_Static_assert(offsetof(struct G, position) == 0x00F43, "position");
_Static_assert(offsetof(struct G, assnum) == 0x00F63, "assnum");
_Static_assert(offsetof(struct G, asslist) == 0x00F83, "asslist");
_Static_assert(offsetof(struct G, temp1) == 0x01003, "temp1");
_Static_assert(offsetof(struct G, temp2) == 0x01023, "temp2");
_Static_assert(offsetof(struct G, temp3) == 0x01043, "temp3");
_Static_assert(offsetof(struct G, temp4) == 0x01063, "temp4");
_Static_assert(offsetof(struct G, temp5) == 0x01083, "temp5");
_Static_assert(offsetof(struct G, temp2b) == 0x010A3, "temp2b");
_Static_assert(offsetof(struct G, radiusx) == 0x010C3, "radiusx");
_Static_assert(offsetof(struct G, radiusy) == 0x010E3, "radiusy");
_Static_assert(offsetof(struct G, Wallcos) == 0x01103, "Wallcos");
_Static_assert(offsetof(struct G, Wallsin) == 0x01123, "Wallsin");
_Static_assert(offsetof(struct G, SCnum) == 0x01143, "SCnum");
_Static_assert(offsetof(struct G, facedir) == 0x01163, "facedir");
_Static_assert(offsetof(struct G, facefrac) == 0x01183, "facefrac");
_Static_assert(offsetof(struct G, SPA) == 0x011A3, "SPA");
_Static_assert(offsetof(struct G, SPAnum) == 0x011C3, "SPAnum");
_Static_assert(offsetof(struct G, SPAcnt) == 0x011E3, "SPAcnt");
_Static_assert(offsetof(struct G, nopuck) == 0x01203, "nopuck");
_Static_assert(offsetof(struct G, nopuckb) == 0x01223, "nopuckb");
_Static_assert(offsetof(struct G, newpos) == 0x01243, "newpos");
_Static_assert(offsetof(struct G, newpnum) == 0x01263, "newpnum");
_Static_assert(offsetof(struct G, pflags) == 0x01283, "pflags");
_Static_assert(offsetof(struct G, pnum) == 0x012A3, "pnum");
_Static_assert(offsetof(struct G, weight) == 0x012C3, "weight");
_Static_assert(offsetof(struct G, legstr) == 0x012E3, "legstr");
_Static_assert(offsetof(struct G, legspd) == 0x01303, "legspd");
_Static_assert(offsetof(struct G, aioff) == 0x01323, "aioff");
_Static_assert(offsetof(struct G, aidef) == 0x01343, "aidef");
_Static_assert(offsetof(struct G, shotspd) == 0x01363, "shotspd");
_Static_assert(offsetof(struct G, shotacc) == 0x01383, "shotacc");
_Static_assert(offsetof(struct G, passacc) == 0x013A3, "passacc");
_Static_assert(offsetof(struct G, rostnum) == 0x013C3, "rostnum");
_Static_assert(offsetof(struct G, spodds) == 0x013E3, "spodds");
_Static_assert(offsetof(struct G, stickhand) == 0x01403, "stickhand");
_Static_assert(offsetof(struct G, endurance) == 0x01423, "endurance");
_Static_assert(offsetof(struct G, aggress) == 0x01443, "aggress");
_Static_assert(offsetof(struct G, fighting) == 0x01463, "fighting");
_Static_assert(offsetof(struct G, checking) == 0x01483, "checking");
_Static_assert(offsetof(struct G, handed) == 0x014A3, "handed");
_Static_assert(offsetof(struct G, pfgoal) == 0x014C3, "pfgoal");
_Static_assert(offsetof(struct G, pfalock) == 0x014E3, "pfalock");
_Static_assert(offsetof(struct G, pfteam) == 0x01503, "pfteam");
_Static_assert(offsetof(struct G, pfrev) == 0x01523, "pfrev");
_Static_assert(offsetof(struct G, pf2aip) == 0x01543, "pf2aip");
_Static_assert(offsetof(struct G, pfnc) == 0x01563, "pfnc");
_Static_assert(offsetof(struct G, Ylist) == 0x01583, "Ylist");
_Static_assert(offsetof(struct G, OOlistpos) == 0x015A3, "OOlistpos");
_Static_assert(offsetof(struct G, OOlist) == 0x015C3, "OOlist");
_Static_assert(offsetof(struct G, gmode) == 0x015D4, "gmode");
_Static_assert(offsetof(struct G, gmclock) == 0x015D6, "gmclock");
_Static_assert(offsetof(struct G, sflags) == 0x015D8, "sflags");
_Static_assert(offsetof(struct G, sfhormode) == 0x015DA, "sfhormode");
_Static_assert(offsetof(struct G, sflags3) == 0x015DC, "sflags3");
_Static_assert(offsetof(struct G, Pencntdwn) == 0x01620, "Pencntdwn");
_Static_assert(offsetof(struct G, pentimer) == 0x01622, "pentimer");
_Static_assert(offsetof(struct G, PBnum) == 0x01624, "PBnum");
_Static_assert(offsetof(struct G, InjCntDown) == 0x01626, "InjCntDown");
_Static_assert(offsetof(struct G, penmsgtimer) == 0x01628, "penmsgtimer");
_Static_assert(offsetof(struct G, RefCnt) == 0x0162A, "RefCnt");
_Static_assert(offsetof(struct G, RefStep) == 0x0162C, "RefStep");
_Static_assert(offsetof(struct G, RefPen) == 0x0162E, "RefPen");
_Static_assert(offsetof(struct G, gsp) == 0x01630, "gsp");
_Static_assert(offsetof(struct G, gameclock) == 0x01632, "gameclock");
_Static_assert(offsetof(struct G, TempPlOffset) == 0x01636, "TempPlOffset");
_Static_assert(offsetof(struct G, IsBreakaway) == 0x01638, "IsBreakaway");
_Static_assert(offsetof(struct G, PenShotActive) == 0x0163A, "PenShotActive");
_Static_assert(offsetof(struct G, PenSumLength) == 0x0163C, "PenSumLength");
_Static_assert(offsetof(struct G, PenSum) == 0x0163E, "PenSum");
_Static_assert(offsetof(struct G, tmPwrGoals) == 0x01742, "tmPwrGoals");
_Static_assert(offsetof(struct G, tmPwrPlays) == 0x01746, "tmPwrPlays");
_Static_assert(offsetof(struct G, tmPenalties) == 0x0174A, "tmPenalties");
_Static_assert(offsetof(struct G, tmPenmin) == 0x0174E, "tmPenmin");
_Static_assert(offsetof(struct G, tmATOP) == 0x01752, "tmATOP");
_Static_assert(offsetof(struct G, tmPwrPlayTime) == 0x01756, "tmPwrPlayTime");
_Static_assert(offsetof(struct G, tmPPShots) == 0x0175A, "tmPPShots");
_Static_assert(offsetof(struct G, tmSHShots) == 0x0175E, "tmSHShots");
_Static_assert(offsetof(struct G, tmSHGoals) == 0x01762, "tmSHGoals");
_Static_assert(offsetof(struct G, tmsort) == 0x01766, "tmsort");
_Static_assert(offsetof(struct G, tmPerShots) == 0x0176A, "tmPerShots");
_Static_assert(offsetof(struct G, tmPerGoals) == 0x0177E, "tmPerGoals");
_Static_assert(offsetof(struct G, tmscore) == 0x0178E, "tmscore");
_Static_assert(offsetof(struct G, tmfowon) == 0x01792, "tmfowon");
_Static_assert(offsetof(struct G, tmline) == 0x01796, "tmline");
_Static_assert(offsetof(struct G, tmtouch1) == 0x0179A, "tmtouch1");
_Static_assert(offsetof(struct G, tmtouch2) == 0x0179E, "tmtouch2");
_Static_assert(offsetof(struct G, tmtouch3) == 0x017A2, "tmtouch3");
_Static_assert(offsetof(struct G, tmap) == 0x017A6, "tmap");
_Static_assert(offsetof(struct G, tmgoalie) == 0x017AA, "tmgoalie");
_Static_assert(offsetof(struct G, tmnear) == 0x017AE, "tmnear");
_Static_assert(offsetof(struct G, tmnearpl) == 0x017B2, "tmnearpl");
_Static_assert(offsetof(struct G, tmchecks) == 0x017B6, "tmchecks");
_Static_assert(offsetof(struct G, tmpasscomp) == 0x017BA, "tmpasscomp");
_Static_assert(offsetof(struct G, tmpassatt) == 0x017BE, "tmpassatt");
_Static_assert(offsetof(struct G, tmBreakaways) == 0x017C2, "tmBreakaways");
_Static_assert(offsetof(struct G, tmBreakawayGoals) == 0x017C6, "tmBreakawayGoals");
_Static_assert(offsetof(struct G, tmPenShots) == 0x017CA, "tmPenShots");
_Static_assert(offsetof(struct G, tmPenShotGoals) == 0x017CE, "tmPenShotGoals");
_Static_assert(offsetof(struct G, tmOneTimerSwings) == 0x017D2, "tmOneTimerSwings");
_Static_assert(offsetof(struct G, tmOneTimers) == 0x017D6, "tmOneTimers");
_Static_assert(offsetof(struct G, tmOneTimerGoals) == 0x017DA, "tmOneTimerGoals");
_Static_assert(offsetof(struct G, tmflags) == 0x017DE, "tmflags");
_Static_assert(offsetof(struct G, tmtimeouts) == 0x017E2, "tmtimeouts");
_Static_assert(offsetof(struct G, tmlcchoice) == 0x017E6, "tmlcchoice");
_Static_assert(offsetof(struct G, tmpde) == 0x017EA, "tmpde");
_Static_assert(offsetof(struct G, tmpdst) == 0x01852, "tmpdst");
_Static_assert(offsetof(struct G, tmboxlist) == 0x018BA, "tmboxlist");
_Static_assert(offsetof(struct G, tmgoals) == 0x018EE, "tmgoals");
_Static_assert(offsetof(struct G, tmassists) == 0x01922, "tmassists");
_Static_assert(offsetof(struct G, tmpshots) == 0x01956, "tmpshots");
_Static_assert(offsetof(struct G, ppenmin) == 0x0198A, "ppenmin");
_Static_assert(offsetof(struct G, CwdExciteLvl) == 0x019BE, "CwdExciteLvl");
_Static_assert(offsetof(struct G, CrowdLoud) == 0x019C0, "CrowdLoud");
_Static_assert(offsetof(struct G, CurCrowdMeter) == 0x019C2, "CurCrowdMeter");
_Static_assert(offsetof(struct G, CrowdPeak) == 0x019C4, "CrowdPeak");
_Static_assert(offsetof(struct G, CrowdAvg) == 0x019C6, "CrowdAvg");
_Static_assert(offsetof(struct G, CrowdMeterCount) == 0x019C8, "CrowdMeterCount");
_Static_assert(offsetof(struct G, CrowdMeterSum) == 0x019CA, "CrowdMeterSum");
_Static_assert(offsetof(struct G, HotColdRaised) == 0x019CE, "HotColdRaised");
_Static_assert(offsetof(struct G, teampads) == 0x019D0, "teampads");
_Static_assert(offsetof(struct G, playerpad) == 0x019D4, "playerpad");
_Static_assert(offsetof(struct G, c1playernum) == 0x019EC, "c1playernum");
_Static_assert(offsetof(struct G, padside) == 0x019F6, "padside");
_Static_assert(offsetof(struct G, mp5connect) == 0x01A00, "mp5connect");
_Static_assert(offsetof(struct G, mp5readdelay) == 0x01A02, "mp5readdelay");
_Static_assert(offsetof(struct G, mp5checktimer) == 0x01A04, "mp5checktimer");
_Static_assert(offsetof(struct G, mp5port1last) == 0x01A06, "mp5port1last");
_Static_assert(offsetof(struct G, mp5port2last) == 0x01A08, "mp5port2last");
_Static_assert(offsetof(struct G, mp5portchg) == 0x01A0A, "mp5portchg");
_Static_assert(offsetof(struct G, mp5present) == 0x01A0C, "mp5present");
_Static_assert(offsetof(struct G, mp5warning) == 0x01A0E, "mp5warning");
_Static_assert(offsetof(struct G, InIntermission) == 0x01A10, "InIntermission");
_Static_assert(offsetof(struct G, HotColdTable) == 0x01A12, "HotColdTable");
_Static_assert(offsetof(struct G, PerTimeTotal) == 0x01C82, "PerTimeTotal");
_Static_assert(offsetof(struct G, cont1team) == 0x01C84, "cont1team");
_Static_assert(offsetof(struct G, cont2team) == 0x01C86, "cont2team");
_Static_assert(offsetof(struct G, cont3team) == 0x01C88, "cont3team");
_Static_assert(offsetof(struct G, cont4team) == 0x01C8A, "cont4team");
_Static_assert(offsetof(struct G, cont5team) == 0x01C8C, "cont5team");
_Static_assert(offsetof(struct G, goaliemode1) == 0x01C90, "goaliemode1");
_Static_assert(offsetof(struct G, goaliemode2) == 0x01C92, "goaliemode2");
_Static_assert(offsetof(struct G, defcontrol) == 0x01C94, "defcontrol");
_Static_assert(offsetof(struct G, HomeTeam) == 0x01C98, "HomeTeam");
_Static_assert(offsetof(struct G, VisTeam) == 0x01C9A, "VisTeam");
_Static_assert(offsetof(struct G, tmdata) == 0x01CDC, "tmdata");
_Static_assert(offsetof(struct G, tmteam) == 0x01CE0, "tmteam");
_Static_assert(offsetof(struct G, lcpad) == 0x01D55, "lcpad");
_Static_assert(offsetof(struct G, jmlvector) == 0x01D59, "jmlvector");
_Static_assert(offsetof(struct G, asv) == 0x01D5E, "asv");
_Static_assert(offsetof(struct G, PauseUnused) == 0x01D60, "PauseUnused");
_Static_assert(offsetof(struct G, sndbank) == 0x01D68, "sndbank");
_Static_assert(offsetof(struct G, gameframes) == 0x01D6D, "gameframes");
_Static_assert(offsetof(struct G, homeshooters) == 0x01D71, "homeshooters");
_Static_assert(offsetof(struct G, awayshooters) == 0x01D79, "awayshooters");
_Static_assert(offsetof(struct G, shootoutmode) == 0x01D81, "shootoutmode");
_Static_assert(offsetof(struct G, shootoutstop) == 0x01D83, "shootoutstop");
_Static_assert(offsetof(struct G, shootoutround) == 0x01D85, "shootoutround");
_Static_assert(offsetof(struct G, shootoutturnover) == 0x01D87, "shootoutturnover");
_Static_assert(offsetof(struct G, shootoutdecider) == 0x01D89, "shootoutdecider");
_Static_assert(offsetof(struct G, linecursor) == 0x01D8B, "linecursor");
_Static_assert(offsetof(struct G, lineplayer) == 0x01D8F, "lineplayer");
_Static_assert(offsetof(struct G, lineswap) == 0x01D93, "lineswap");
_Static_assert(offsetof(struct G, linemenuitem) == 0x01D97, "linemenuitem");
_Static_assert(offsetof(struct G, curplayer) == 0x01D9B, "curplayer");
_Static_assert(offsetof(struct G, lineslots) == 0x01D9F, "lineslots");
_Static_assert(offsetof(struct G, listcount) == 0x01DA3, "listcount");
_Static_assert(offsetof(struct G, picktop) == 0x01DA7, "picktop");
_Static_assert(offsetof(struct G, pickfirst) == 0x01DAB, "pickfirst");
_Static_assert(offsetof(struct G, pickrows) == 0x01DAF, "pickrows");
_Static_assert(offsetof(struct G, curitem) == 0x01DB9, "curitem");
_Static_assert(offsetof(struct G, listtop) == 0x01DBD, "listtop");
_Static_assert(offsetof(struct G, listend) == 0x01DC1, "listend");
_Static_assert(offsetof(struct G, listrows) == 0x01DC5, "listrows");
_Static_assert(offsetof(struct G, statstemp) == 0x01DC9, "statstemp");
_Static_assert(offsetof(struct G, scrollirqdone) == 0x01DCD, "scrollirqdone");
_Static_assert(offsetof(struct G, crowdmeterlit) == 0x01DD1, "crowdmeterlit");
_Static_assert(offsetof(struct G, otherskip) == 0x01DE9, "otherskip");
_Static_assert(offsetof(struct G, irqline1) == 0x01DED, "irqline1");
_Static_assert(offsetof(struct G, irqline2) == 0x01DEF, "irqline2");
_Static_assert(offsetof(struct G, irqline3) == 0x01DF1, "irqline3");
_Static_assert(offsetof(struct G, irqvscroll1) == 0x01DF3, "irqvscroll1");
_Static_assert(offsetof(struct G, irqvscroll2) == 0x01DF5, "irqvscroll2");
_Static_assert(offsetof(struct G, irqvscroll3) == 0x01DF7, "irqvscroll3");
_Static_assert(offsetof(struct G, statcolumn) == 0x01DF9, "statcolumn");
_Static_assert(offsetof(struct G, statsorder) == 0x01DFB, "statsorder");
_Static_assert(offsetof(struct G, statpenrows) == 0x01E15, "statpenrows");
_Static_assert(offsetof(struct G, letterx) == 0x01E17, "letterx");
_Static_assert(offsetof(struct G, lettery) == 0x01E1B, "lettery");
_Static_assert(offsetof(struct G, otgoalscorer) == 0x01E1F, "otgoalscorer");
_Static_assert(offsetof(struct G, starplayerend) == 0x01E21, "starplayerend");
_Static_assert(offsetof(struct G, stargoalieend) == 0x01E23, "stargoalieend");
_Static_assert(offsetof(struct G, starswapplayer) == 0x01E25, "starswapplayer");
_Static_assert(offsetof(struct G, starswapscore) == 0x01E27, "starswapscore");
_Static_assert(offsetof(struct G, star1) == 0x01E2B, "star1");
_Static_assert(offsetof(struct G, star2) == 0x01E2D, "star2");
_Static_assert(offsetof(struct G, star3) == 0x01E2F, "star3");
_Static_assert(offsetof(struct G, star1score) == 0x01E31, "star1score");
_Static_assert(offsetof(struct G, star2score) == 0x01E35, "star2score");
_Static_assert(offsetof(struct G, star3score) == 0x01E39, "star3score");
_Static_assert(offsetof(struct G, matchuptimer) == 0x01E3D, "matchuptimer");
_Static_assert(offsetof(struct G, matchuphold) == 0x01E3F, "matchuphold");
_Static_assert(offsetof(struct G, hotterteam) == 0x01E41, "hotterteam");
_Static_assert(offsetof(struct G, hotextreme) == 0x01E43, "hotextreme");
_Static_assert(offsetof(struct G, hotcoldsum) == 0x01E45, "hotcoldsum");
_Static_assert(offsetof(struct G, hotplayer1) == 0x01E49, "hotplayer1");
_Static_assert(offsetof(struct G, hotplayer2) == 0x01E4D, "hotplayer2");
_Static_assert(offsetof(struct G, hotplayer3) == 0x01E51, "hotplayer3");
_Static_assert(offsetof(struct G, coldplayer1) == 0x01E55, "coldplayer1");
_Static_assert(offsetof(struct G, coldplayer2) == 0x01E59, "coldplayer2");
_Static_assert(offsetof(struct G, coldplayer3) == 0x01E5D, "coldplayer3");
_Static_assert(offsetof(struct G, hotvalue1) == 0x01E61, "hotvalue1");
_Static_assert(offsetof(struct G, hotvalue2) == 0x01E65, "hotvalue2");
_Static_assert(offsetof(struct G, hotvalue3) == 0x01E69, "hotvalue3");
_Static_assert(offsetof(struct G, coldvalue1) == 0x01E6D, "coldvalue1");
_Static_assert(offsetof(struct G, coldvalue2) == 0x01E71, "coldvalue2");
_Static_assert(offsetof(struct G, coldvalue3) == 0x01E75, "coldvalue3");
_Static_assert(offsetof(struct G, strengths) == 0x01E79, "strengths");
_Static_assert(offsetof(struct G, cardframechars) == 0x01E85, "cardframechars");
_Static_assert(offsetof(struct G, picattr) == 0x01E87, "picattr");
_Static_assert(offsetof(struct G, onetimer) == 0x01E89, "onetimer");
_Static_assert(offsetof(struct G, breakaway) == 0x01E8B, "breakaway");
_Static_assert(offsetof(struct G, BA_Sktr_SCnum) == 0x01E8D, "BA_Sktr_SCnum");
_Static_assert(offsetof(struct G, BA_Skater_Offset) == 0x01E8F, "BA_Skater_Offset");
_Static_assert(offsetof(struct G, BA_Team) == 0x01E91, "BA_Team");
_Static_assert(offsetof(struct G, pspad) == 0x01E93, "pspad");
_Static_assert(offsetof(struct G, psfoxsave) == 0x01E97, "psfoxsave");
_Static_assert(offsetof(struct G, psfoysave) == 0x01E99, "psfoysave");
_Static_assert(offsetof(struct G, penshot) == 0x01E9B, "penshot");
_Static_assert(offsetof(struct G, penshotlive) == 0x01E9D, "penshotlive");
_Static_assert(offsetof(struct G, pstimer) == 0x01E9F, "pstimer");
_Static_assert(offsetof(struct G, sopathx) == 0x01EA1, "sopathx");
_Static_assert(offsetof(struct G, sopathy) == 0x01EA3, "sopathy");
_Static_assert(offsetof(struct G, sopathnext) == 0x01EA5, "sopathnext");
_Static_assert(offsetof(struct G, sopathpoint) == 0x01EA7, "sopathpoint");
_Static_assert(offsetof(struct G, sopathside) == 0x01EA9, "sopathside");
_Static_assert(offsetof(struct G, sopathface) == 0x01EAB, "sopathface");
_Static_assert(offsetof(struct G, lastperiod) == 0x01EAD, "lastperiod");
_Static_assert(offsetof(struct G, teamtaken) == 0x01EAF, "teamtaken");
_Static_assert(offsetof(struct G, teambit) == 0x01EB3, "teambit");
_Static_assert(offsetof(struct G, pmenulist) == 0x01EB7, "pmenulist");
_Static_assert(offsetof(struct G, pmenucount) == 0x01EB9, "pmenucount");
_Static_assert(offsetof(struct G, pregamepause) == 0x01EBB, "pregamepause");
_Static_assert(offsetof(struct G, RAMTag2) == 0x01EBD, "RAMTag2");
_Static_assert(offsetof(struct G, HandleTable) == 0x02000, "HandleTable");
_Static_assert(offsetof(struct G, FrameCacheAddr) == 0x02540, "FrameCacheAddr");
_Static_assert(offsetof(struct G, FrameCacheBank) == 0x02640, "FrameCacheBank");
_Static_assert(offsetof(struct G, FrameCacheOf) == 0x02740, "FrameCacheOf");
_Static_assert(offsetof(struct G, FrameCacheNum) == 0x02B40, "FrameCacheNum");
_Static_assert(offsetof(struct G, FrameCacheAlloc) == 0x02C40, "FrameCacheAlloc");
_Static_assert(offsetof(struct G, FrameCacheUse) == 0x02D40, "FrameCacheUse");
_Static_assert(offsetof(struct G, FrameCacheTile) == 0x02E40, "FrameCacheTile");
_Static_assert(offsetof(struct G, BlockRuns) == 0x02F40, "BlockRuns");
_Static_assert(offsetof(struct G, BlockRunPrev) == 0x02FB9, "BlockRunPrev");
_Static_assert(offsetof(struct G, TileRuns) == 0x03032, "TileRuns");
_Static_assert(offsetof(struct G, TileRunPrev) == 0x03093, "TileRunPrev");
_Static_assert(offsetof(struct G, carriedy) == 0x030F4, "carriedy");
_Static_assert(offsetof(struct G, ltplayer) == 0x030F6, "ltplayer");
_Static_assert(offsetof(struct G, ltx) == 0x030F8, "ltx");
_Static_assert(offsetof(struct G, lty) == 0x030FA, "lty");
_Static_assert(offsetof(struct G, fobox) == 0x03120, "fobox");
_Static_assert(offsetof(struct G, fodir1) == 0x03122, "fodir1");
_Static_assert(offsetof(struct G, fodir2) == 0x03124, "fodir2");
_Static_assert(offsetof(struct G, fodropx) == 0x03126, "fodropx");
_Static_assert(offsetof(struct G, fodropy) == 0x03128, "fodropy");
_Static_assert(offsetof(struct G, fofdata2) == 0x0312A, "fofdata2");
_Static_assert(offsetof(struct G, fofdata) == 0x03132, "fofdata");
_Static_assert(offsetof(struct G, fox) == 0x03136, "fox");
_Static_assert(offsetof(struct G, foy) == 0x03138, "foy");
_Static_assert(offsetof(struct G, ScoreSumbytes) == 0x0313A, "ScoreSumbytes");
_Static_assert(offsetof(struct G, hatx) == 0x03394, "hatx");
_Static_assert(offsetof(struct G, haty) == 0x033A8, "haty");
_Static_assert(offsetof(struct G, hatz) == 0x033BC, "hatz");
_Static_assert(offsetof(struct G, hatvx) == 0x033D0, "hatvx");
_Static_assert(offsetof(struct G, hatvy) == 0x033E4, "hatvy");
_Static_assert(offsetof(struct G, hatvz) == 0x033F8, "hatvz");
_Static_assert(offsetof(struct G, hatstep) == 0x0340C, "hatstep");
_Static_assert(offsetof(struct G, hattimer) == 0x03420, "hattimer");
_Static_assert(offsetof(struct G, hatkind) == 0x03434, "hatkind");
_Static_assert(offsetof(struct G, hattrick) == 0x03448, "hattrick");
_Static_assert(offsetof(struct G, refwindow) == 0x0344A, "refwindow");
_Static_assert(offsetof(struct G, penmsgbox) == 0x0344C, "penmsgbox");
_Static_assert(offsetof(struct G, newperiod) == 0x0344E, "newperiod");
_Static_assert(offsetof(struct G, pspuckback) == 0x03450, "pspuckback");
_Static_assert(offsetof(struct G, setuphome) == 0x03452, "setuphome");
_Static_assert(offsetof(struct G, setupvis) == 0x03454, "setupvis");
_Static_assert(offsetof(struct G, teamschanged) == 0x03456, "teamschanged");
_Static_assert(offsetof(struct G, cardtimer) == 0x03458, "cardtimer");
_Static_assert(offsetof(struct G, matchuphome) == 0x0345A, "matchuphome");
_Static_assert(offsetof(struct G, matchupvis) == 0x0345C, "matchupvis");
_Static_assert(offsetof(struct G, setupmatchup) == 0x0345E, "setupmatchup");
_Static_assert(offsetof(struct G, redrawblocks) == 0x03460, "redrawblocks");
_Static_assert(offsetof(struct G, horblocks) == 0x03462, "horblocks");
_Static_assert(offsetof(struct G, cardblock) == 0x03464, "cardblock");
_Static_assert(offsetof(struct G, bigtextchar) == 0x03466, "bigtextchar");
_Static_assert(offsetof(struct G, drawlinesback) == 0x03468, "drawlinesback");
_Static_assert(offsetof(struct G, pausesubmenu) == 0x0346A, "pausesubmenu");
_Static_assert(offsetof(struct G, phototeams) == 0x0346C, "phototeams");
_Static_assert(offsetof(struct G, homepicaddr) == 0x03470, "homepicaddr");
_Static_assert(offsetof(struct G, vispicaddr) == 0x03472, "vispicaddr");
_Static_assert(offsetof(struct G, matchupphotos) == 0x03474, "matchupphotos");
_Static_assert(offsetof(struct G, pausemenuon) == 0x03476, "pausemenuon");
_Static_assert(offsetof(struct G, playerstatson) == 0x03478, "playerstatson");
_Static_assert(offsetof(struct G, gamesstarted) == 0x0347A, "gamesstarted");
_Static_assert(offsetof(struct G, OptPlayMode) == 0x034BC, "OptPlayMode");
_Static_assert(offsetof(struct G, OptTeam1) == 0x034BE, "OptTeam1");
_Static_assert(offsetof(struct G, OptTeam2) == 0x034C0, "OptTeam2");
_Static_assert(offsetof(struct G, OptPerlen) == 0x034C2, "OptPerlen");
_Static_assert(offsetof(struct G, OptPen) == 0x034C4, "OptPen");
_Static_assert(offsetof(struct G, OptLine) == 0x034C6, "OptLine");
_Static_assert(offsetof(struct G, OptGoalie) == 0x034C8, "OptGoalie");
_Static_assert(offsetof(struct G, OptPlayers) == 0x034CA, "OptPlayers");
_Static_assert(offsetof(struct G, notdemo) == 0x034CC, "notdemo");
_Static_assert(offsetof(struct G, pojoy) == 0x034CE, "pojoy");
_Static_assert(offsetof(struct G, postarts) == 0x034D0, "postarts");
_Static_assert(offsetof(struct G, potree) == 0x034D2, "potree");
_Static_assert(offsetof(struct G, potreeteam) == 0x034F2, "potreeteam");
_Static_assert(offsetof(struct G, PassWord) == 0x034F4, "PassWord");
_Static_assert(offsetof(struct G, passbits) == 0x03508, "passbits");
_Static_assert(offsetof(struct G, tpassbits) == 0x03518, "tpassbits");
_Static_assert(offsetof(struct G, gst1) == 0x03528, "gst1");
_Static_assert(offsetof(struct G, gst2) == 0x03538, "gst2");
_Static_assert(offsetof(struct G, gspotwins) == 0x03548, "gspotwins");
_Static_assert(offsetof(struct G, gspobwins) == 0x03558, "gspobwins");
_Static_assert(offsetof(struct G, gsper) == 0x03568, "gsper");
_Static_assert(offsetof(struct G, gss1) == 0x03578, "gss1");
_Static_assert(offsetof(struct G, gss2) == 0x03588, "gss2");
_Static_assert(offsetof(struct G, gsflags) == 0x03598, "gsflags");
_Static_assert(offsetof(struct G, printfontset) == 0x035A8, "printfontset");
_Static_assert(offsetof(struct G, charofs) == 0x035AA, "charofs");
_Static_assert(offsetof(struct G, charofs2) == 0x035AC, "charofs2");
_Static_assert(offsetof(struct G, BigFontChars) == 0x035AE, "BigFontChars");
_Static_assert(offsetof(struct G, EASNcset) == 0x035B0, "EASNcset");
_Static_assert(offsetof(struct G, cardbackchars) == 0x035B2, "cardbackchars");
_Static_assert(offsetof(struct G, backchars) == 0x035B4, "backchars");
_Static_assert(offsetof(struct G, smallfontchars) == 0x035B6, "smallfontchars");
_Static_assert(offsetof(struct G, centericechars) == 0x035B8, "centericechars");
_Static_assert(offsetof(struct G, faceoffvrcset) == 0x035BA, "faceoffvrcset");
_Static_assert(offsetof(struct G, Framercset) == 0x035BC, "Framercset");
_Static_assert(offsetof(struct G, ninetilechars) == 0x035BE, "ninetilechars");
_Static_assert(offsetof(struct G, outlinechars) == 0x035C0, "outlinechars");
_Static_assert(offsetof(struct G, ExtraChars) == 0x035C2, "ExtraChars");
_Static_assert(offsetof(struct G, nextfreechar) == 0x035C4, "nextfreechar");
_Static_assert(offsetof(struct G, cupchars) == 0x035C6, "cupchars");
_Static_assert(offsetof(struct G, refcharshor) == 0x035C8, "refcharshor");
_Static_assert(offsetof(struct G, refchars) == 0x035CA, "refchars");
_Static_assert(offsetof(struct G, scorechars) == 0x035CE, "scorechars");
_Static_assert(offsetof(struct G, blankchar) == 0x035D0, "blankchar");
_Static_assert(offsetof(struct G, picchars) == 0x035D2, "picchars");
_Static_assert(offsetof(struct G, playoffRoundIdx) == 0x035D4, "playoffRoundIdx");
_Static_assert(offsetof(struct G, gamenum) == 0x035D6, "gamenum");
_Static_assert(offsetof(struct G, vcountfrac) == 0x035D8, "vcountfrac");
_Static_assert(offsetof(struct G, vcountstep) == 0x035DA, "vcountstep");
_Static_assert(offsetof(struct G, tickerstart) == 0x035DC, "tickerstart");
_Static_assert(offsetof(struct G, Crowdlevel) == 0x035E0, "Crowdlevel");
_Static_assert(offsetof(struct G, SPCStreamLeft) == 0x035E2, "SPCStreamLeft");
_Static_assert(offsetof(struct G, streamsfx) == 0x035E6, "streamsfx");
_Static_assert(offsetof(struct G, streamstate) == 0x035E8, "streamstate");
_Static_assert(offsetof(struct G, streamloaded) == 0x035EA, "streamloaded");
_Static_assert(offsetof(struct G, SPCStreamPtr) == 0x035EC, "SPCStreamPtr");
_Static_assert(offsetof(struct G, replaypanel) == 0x035F0, "replaypanel");
_Static_assert(offsetof(struct G, usehotcold) == 0x035F2, "usehotcold");
_Static_assert(offsetof(struct G, palbuffer) == 0x035F4, "palbuffer");
_Static_assert(offsetof(struct G, textmap) == 0x037F4, "textmap");
_Static_assert(offsetof(struct G, textmap2) == 0x03FF4, "textmap2");
_Static_assert(offsetof(struct G, hlsavebuf) == 0x047F4, "hlsavebuf");
_Static_assert(offsetof(struct G, bigtextbuf) == 0x04EF4, "bigtextbuf");
_Static_assert(offsetof(struct G, screenmap) == 0x0A670, "screenmap");
_Static_assert(offsetof(struct G, screenmap2) == 0x0C676, "screenmap2");
_Static_assert(offsetof(struct G, ReplayBufferEnd) == 0x0E676, "ReplayBufferEnd");
_Static_assert(offsetof(struct G, OAMHiMasks) == 0x0E67C, "OAMHiMasks");
_Static_assert(offsetof(struct G, titleteams) == 0x0F000, "titleteams");
_Static_assert(offsetof(struct G, introbuf) == 0x14000, "introbuf");
_Static_assert(offsetof(struct G, brackettiles) == 0x16700, "brackettiles");
_Static_assert(offsetof(struct G, colourbuf) == 0x17500, "colourbuf");
_Static_assert(offsetof(struct G, logoboxmap) == 0x17700, "logoboxmap");
_Static_assert(offsetof(struct G, picboxmap) == 0x17780, "picboxmap");
_Static_assert(offsetof(struct G, titlescale) == 0x18000, "titlescale");
_Static_assert(offsetof(struct G, introchars) == 0x18010, "introchars");
_Static_assert(offsetof(struct G, titlexofs) == 0x18A00, "titlexofs");
_Static_assert(offsetof(struct G, titleyscroll) == 0x19400, "titleyscroll");
_Static_assert(offsetof(struct G, titlelogobuf) == 0x1A000, "titlelogobuf");

#define RAM_OFF_r0 0x00000
#define RAM_OFF_Ram7E 0x00000
#define RAM_OFF_r1 0x00004
#define RAM_OFF_r2 0x00008
#define RAM_OFF_p0 0x0000C
#define RAM_OFF_p1 0x00010
#define RAM_OFF_r3 0x00014
#define RAM_OFF_r4 0x00018
#define RAM_OFF_r5 0x0001C
#define RAM_OFF_r6 0x00020
#define RAM_OFF_r7 0x00024
#define RAM_OFF_r8 0x00028
#define RAM_OFF_r9 0x0002C
#define RAM_OFF_ClipTopUnused 0x00030
#define RAM_OFF_ClipBottomUnused 0x00032
#define RAM_OFF_SoundTable 0x00034
#define RAM_OFF_NMISkip 0x00038
#define RAM_OFF_DMAQueueHead 0x0003A
#define RAM_OFF_DMAQueueTail 0x0003C
#define RAM_OFF_DMABudget 0x0003E
#define RAM_OFF_StreamSrc 0x0003E
#define RAM_OFF_MulCand 0x0003E
#define RAM_OFF_MulProd 0x00042
#define RAM_OFF_StreamLoop 0x00042
#define RAM_OFF_MulFracResult 0x00043
#define RAM_OFF_MulPlier 0x00046
#define RAM_OFF_CurTrack 0x0004A
#define RAM_OFF_CurVoice 0x0004B
#define RAM_OFF_CurTrackPtr 0x0004C
#define RAM_OFF_InstPtr 0x0004E
#define RAM_OFF_SoundBank 0x00050
#define RAM_OFF_VarLenAcc 0x00052
#define RAM_OFF_VarLenByte 0x00054
#define RAM_OFF_EventPtr 0x00056
#define RAM_OFF_EventData1 0x0005A
#define RAM_OFF_NotePitch 0x0005A
#define RAM_OFF_GainWord 0x0005A
#define RAM_OFF_DSPGain 0x0005B
#define RAM_OFF_StartGain 0x0005C
#define RAM_OFF_BendAmount 0x0005C
#define RAM_OFF_EventData2 0x0005E
#define RAM_OFF_EventCode 0x00060
#define RAM_OFF_SendPitch 0x00060
#define RAM_OFF_EventEAUnused 0x00062
#define RAM_OFF_SfxVelocity 0x00064
#define RAM_OFF_VoiceRotor 0x00066
#define RAM_OFF_StealCount 0x00068
#define RAM_OFF_HuffBitsExt 0x0006A
#define RAM_OFF_LZCopySrc 0x0006B
#define RAM_OFF_HuffBits 0x0006C
#define RAM_OFF_HuffNumber 0x0006F
#define RAM_OFF_LZOutAddr 0x0006F
#define RAM_OFF_HuffStartBits 0x00071
#define RAM_OFF_LZCmd 0x00071
#define RAM_OFF_HuffEscape 0x00073
#define RAM_OFF_LZCmdByte2 0x00073
#define RAM_OFF_HuffEscapeLen 0x00074
#define RAM_OFF_HuffCode 0x00075
#define RAM_OFF_LZCmdByte3 0x00075
#define RAM_OFF_HuffSymCount 0x00077
#define RAM_OFF_LZLenLo 0x00077
#define RAM_OFF_HuffMaxLen 0x00079
#define RAM_OFF_LZSrcIdx 0x00079
#define RAM_OFF_HuffLenIdx 0x0007B
#define RAM_OFF_LZDistance 0x0007B
#define RAM_OFF_HuffSpan 0x0007D
#define RAM_OFF_crowdhold 0x00081
#define RAM_OFF_vcount 0x00083
#define RAM_OFF_xsave 0x00087
#define RAM_OFF_a0 0x00089
#define RAM_OFF_a1 0x0008D
#define RAM_OFF_a2 0x00091
#define RAM_OFF_a3 0x00095
#define RAM_OFF_a4 0x00099
#define RAM_OFF_a5 0x0009D
#define RAM_OFF_d0 0x000A5
#define RAM_OFF_d1 0x000A9
#define RAM_OFF_d2 0x000AD
#define RAM_OFF_d3 0x000B1
#define RAM_OFF_d4 0x000B5
#define RAM_OFF_d5 0x000B9
#define RAM_OFF_d6 0x000BD
#define RAM_OFF_d7 0x000C1
#define RAM_OFF_introHookFlags 0x000C3
#define RAM_OFF_tmp0 0x000C5
#define RAM_OFF_tmp1 0x000C7
#define RAM_OFF_tmp2 0x000C9
#define RAM_OFF_tmp3 0x000CB
#define RAM_OFF_tmp4 0x000CD
#define RAM_OFF_tmp5 0x000CF
#define RAM_OFF_introPal2Src 0x000D1
#define RAM_OFF_introBG1H 0x000D5
#define RAM_OFF_introBG1V 0x000D7
#define RAM_OFF_burstspeed 0x000D7
#define RAM_OFF_cornerdy 0x000D7
#define RAM_OFF_introM7A 0x000D9
#define RAM_OFF_introM7D 0x000DB
#define RAM_OFF_mulres 0x000DD
#define RAM_OFF_curobj 0x000E4
#define RAM_OFF_PlayersUnused 0x000E6
#define RAM_OFF_curobjteam 0x000EA
#define RAM_OFF_curobjopp 0x000EC
#define RAM_OFF_DecompBuf 0x00100
#define RAM_OFF_DMAQueue 0x00100
#define RAM_OFF_DecompTab1 0x00500
#define RAM_OFF_DecompTab2 0x00600
#define RAM_OFF_HuffCount 0x00700
#define RAM_OFF_HuffOffset 0x00720
#define RAM_OFF_HuffLimit 0x00740
#define RAM_OFF_HuffLongVec 0x00760
#define RAM_OFF_IniDispShadow 0x00762
#define RAM_OFF_FrameCount 0x00764
#define RAM_OFF_OAMReady 0x00766
#define RAM_OFF_CGDMALen 0x00768
#define RAM_OFF_CGDMAColour 0x0076A
#define RAM_OFF_CGDMASrc 0x0076C
#define RAM_OFF_CGDMABank 0x0076E
#define RAM_OFF_FrameHookNext 0x00770
#define RAM_OFF_NMIHookNext 0x00773
#define RAM_OFF_JoyPad 0x00776
#define RAM_OFF_Multi5Sig 0x00781
#define RAM_OFF_FrameCountHold 0x00787
#define RAM_OFF_FrameHook 0x00789
#define RAM_OFF_NMIHook 0x0078C
#define RAM_OFF_IRQHook 0x0078F
#define RAM_OFF_seed 0x00792
#define RAM_OFF_RAMTag 0x00796
#define RAM_OFF_MapVRAM 0x0079C
#define RAM_OFF_MapScreenStep 0x0079E
#define RAM_OFF_HeapStart7E 0x007A0
#define RAM_OFF_HeapStart7F 0x007A2
#define RAM_OFF_NewHandleSlot 0x007A4
#define RAM_OFF_NewHandleSkipBank 0x007A6
#define RAM_OFF_OAMLastEnd 0x007A8
#define RAM_OFF_OAMDrawBuf 0x007AA
#define RAM_OFF_OAMShowBuf 0x007AC
#define RAM_OFF_SpritesLeft 0x007AE
#define RAM_OFF_OAMHiPtr 0x007B0
#define RAM_OFF_OAMHiShift 0x007B2
#define RAM_OFF_SpriteVRAM 0x007B4
#define RAM_OFF_SpritesReserved 0x007B6
#define RAM_OFF_SpriteBlockTiles 0x007B8
#define RAM_OFF_FrameCacheCount 0x007BA
#define RAM_OFF_OAMNext 0x007BC
#define RAM_OFF_OAMHideEnd 0x007BE
#define RAM_OFF_FrameTilesFail 0x007C0
#define RAM_OFF_CacheUseMark 0x007C2
#define RAM_OFF_DrawFrameNum 0x007C4
#define RAM_OFF_FrameByNumber 0x007C6
#define RAM_OFF_PieceX 0x007C8
#define RAM_OFF_PieceY 0x007CA
#define RAM_OFF_PieceTile 0x007CC
#define RAM_OFF_PieceSize 0x007CE
#define RAM_OFF_PieceNext 0x007D0
#define RAM_OFF_IRQVTime 0x007D2
#define RAM_OFF_IRQHTime 0x007D4
#define RAM_OFF_BG1HScroll 0x007D6
#define RAM_OFF_BG1VScroll 0x007D8
#define RAM_OFF_BG2HScroll 0x007DA
#define RAM_OFF_BG2VScroll 0x007DC
#define RAM_OFF_BG3HScroll 0x007DE
#define RAM_OFF_BG3VScroll 0x007E0
#define RAM_OFF_Mode7InUse 0x007E2
#define RAM_OFF_SoundMono 0x007E3
#define RAM_OFF_SoundLock 0x007E5
#define RAM_OFF_EchoVoices 0x007E7
#define RAM_OFF_NoiseVoices 0x007E8
#define RAM_OFF_PitchModVoices 0x007E9
#define RAM_OFF_EchoDelay 0x007EA
#define RAM_OFF_EchoVolume 0x007EB
#define RAM_OFF_EchoFeedback 0x007EC
#define RAM_OFF_SongPtr 0x007ED
#define RAM_OFF_SongUnused 0x007EF
#define RAM_OFF_MusicPause 0x007F1
#define RAM_OFF_MusicOn 0x007F3
#define RAM_OFF_TempoAcc 0x007F5
#define RAM_OFF_TempoTick 0x007F6
#define RAM_OFF_SoundVolume 0x007F7
#define RAM_OFF_SoundFadeSpeed 0x007F9
#define RAM_OFF_InstVolL 0x007FB
#define RAM_OFF_VoiceVolL 0x007FC
#define RAM_OFF_InstVolR 0x007FD
#define RAM_OFF_VoiceVolR 0x007FE
#define RAM_OFF_CurTrackOfs 0x007FF
#define RAM_OFF_VoiceOfs 0x00800
#define RAM_OFF_TrackCount 0x00801
#define RAM_OFF_MusicDuck 0x00802
#define RAM_OFF_TrackPtr 0x00804
#define RAM_OFF_TrackLoop 0x00826
#define RAM_OFF_TrackWait 0x00848
#define RAM_OFF_TrackInst 0x0086A
#define RAM_OFF_TrackBend 0x0088C
#define RAM_OFF_TrackBendRange 0x0089D
#define RAM_OFF_TrackVolL 0x008AE
#define RAM_OFF_TrackVolR 0x008BF
#define RAM_OFF_TrackSustain 0x008D0
#define RAM_OFF_TrackUnused 0x008E1
#define RAM_OFF_VoiceState 0x008F2
#define RAM_OFF_VoiceTrack 0x008FA
#define RAM_OFF_VoiceVelocity 0x00902
#define RAM_OFF_VoiceNote 0x0090A
#define RAM_OFF_VoicePriority 0x00912
#define RAM_OFF_VoiceLength 0x0091A
#define RAM_OFF_VoiceInst 0x0092A
#define RAM_OFF_VoiceGain 0x0093A
#define RAM_OFF_VoicePitch 0x0094A
#define RAM_OFF_VoiceBasePitch 0x0095A
#define RAM_OFF_VoiceAge 0x0096A
#define RAM_OFF_VoiceVibCount 0x0097A
#define RAM_OFF_VoiceVibOfs 0x0098A
#define RAM_OFF_VoiceVibRate 0x0099A
#define RAM_OFF_VoiceVibRateFrac 0x009AA
#define RAM_OFF_VoiceVibDepth 0x009BA
#define RAM_OFF_VoiceVibDepthFrac 0x009CA
#define RAM_OFF_VoiceVibSpeed 0x009DA
#define RAM_OFF_VoiceVibSpeedFrac 0x009EA
#define RAM_OFF_VoiceVibDir 0x009FA
#define RAM_OFF_RemapOut 0x00A0A
#define RAM_OFF_mesarea 0x00A2A
#define RAM_OFF_PWCharStr 0x00A4A
#define RAM_OFF_PushWidthBuf 0x00A8E
#define RAM_OFF_gamefps 0x00A98
#define RAM_OFF_padlast 0x00A9A
#define RAM_OFF_paddir 0x00AA4
#define RAM_OFF_padchange 0x00AAE
#define RAM_OFF_padnew 0x00AB8
#define RAM_OFF_padchangeany 0x00AC2
#define RAM_OFF_repeatdelay 0x00AC4
#define RAM_OFF_pausepad 0x00AC6
#define RAM_OFF_pausepadidle 0x00AC8
#define RAM_OFF_mp5padsig 0x00ACA
#define RAM_OFF_bosgames 0x00ACF
#define RAM_OFF_fadestep 0x00AD1
#define RAM_OFF_brightness 0x00AD3
#define RAM_OFF_WinBits 0x00AD5
#define RAM_OFF_mp5readall 0x00AD7
#define RAM_OFF_puckc 0x00AD9
#define RAM_OFF_timestep 0x00ADB
#define RAM_OFF_padinputdone 0x00ADD
#define RAM_OFF_PadControlObj 0x00AE7
#define RAM_OFF_fopadfirst 0x00AF5
#define RAM_OFF_padsitnext 0x00AF9
#define RAM_OFF_puckpadnext 0x00AFD
#define RAM_OFF_passpad 0x00B01
#define RAM_OFF_goalieflags 0x00B05
#define RAM_OFF_puckdist 0x00B1D
#define RAM_OFF_puckdist2 0x00B3D
#define RAM_OFF_puckdir 0x00B5D
#define RAM_OFF_asscur 0x00B7D
#define RAM_OFF_topgoalie 0x00B9D
#define RAM_OFF_botgoalie 0x00B9F
#define RAM_OFF_goaliestopped 0x00BA1
#define RAM_OFF_dumpinplayer 0x00BA3
#define RAM_OFF_dumpintarget 0x00BA5
#define RAM_OFF_passtargetx 0x00BA7
#define RAM_OFF_passtargety 0x00BA9
#define RAM_OFF_onetimerreq 0x00BAB
#define RAM_OFF_passwait 0x00BAD
#define RAM_OFF_lastshotspeed 0x00BC5
#define RAM_OFF_GoalieUnused 0x00BC9
#define RAM_OFF_goalieturnwait 0x00BCB
#define RAM_OFF_TitleWH1 0x00BCB
#define RAM_OFF_TitleWH3 0x00BCD
#define RAM_OFF_TitleWH3Low 0x00BCF
#define RAM_OFF_TitleBGMode 0x00BD1
#define RAM_OFF_TitleM7TM 0x00BD3
#define RAM_OFF_TitleTeamIdx 0x00BD5
#define RAM_OFF_CreditStep 0x00BD9
#define RAM_OFF_CreditTime 0x00BDB
#define RAM_OFF_CreditTitleIdx 0x00BDD
#define RAM_OFF_CreditTitleY 0x00BDF
#define RAM_OFF_CreditNameIdx 0x00BE1
#define RAM_OFF_CreditNameY 0x00BE3
#define RAM_OFF_LogoSrcOfs 0x00BE5
#define RAM_OFF_LogoDstOfs 0x00BE7
#define RAM_OFF_titleskip 0x00BE9
#define RAM_OFF_goaliedivewait 0x00BEB
#define RAM_OFF_TitleSkipMask 0x00BEB
#define RAM_OFF_LogoPalBits 0x00BED
#define RAM_OFF_TitlePathPos 0x00BEF
#define RAM_OFF_TitlePathFrac 0x00BF1
#define RAM_OFF_TitleM7X 0x00BF3
#define RAM_OFF_TitleM7Y 0x00BF5
#define RAM_OFF_TitleM7HOfs 0x00BF7
#define RAM_OFF_TitleM7VOfs 0x00BF9
#define RAM_OFF_TitleM7Scale 0x00BFB
#define RAM_OFF_TitlePathFlip 0x00BFD
#define RAM_OFF_TitleLogoFlags 0x00BFF
#define RAM_OFF_FaceoffUnused 0x00C0B
#define RAM_OFF_benchcheer 0x00C23
#define RAM_OFF_benchrounds 0x00C25
#define RAM_OFF_benchtimer 0x00C27
#define RAM_OFF_benchframe 0x00C29
#define RAM_OFF_crowdanim 0x00C2B
#define RAM_OFF_crowdanimstep 0x00C55
#define RAM_OFF_crowdanimtime 0x00C7F
#define RAM_OFF_replaydelay 0x00CA9
#define RAM_OFF_replaylock 0x00CAB
#define RAM_OFF_recsfx 0x00CAD
#define RAM_OFF_recsfxpitch 0x00CAF
#define RAM_OFF_recsfxvol 0x00CB1
#define RAM_OFF_recsfx2 0x00CB3
#define RAM_OFF_recsfxpitch2 0x00CB5
#define RAM_OFF_recsfxvol2 0x00CB7
#define RAM_OFF_replaynewframe 0x00CB9
#define RAM_OFF_replaymusic 0x00CBB
#define RAM_OFF_replayHpos 0x00CBD
#define RAM_OFF_replayVpos 0x00CBF
#define RAM_OFF_percue 0x00CC1
#define RAM_OFF_cursong 0x00CC3
#define RAM_OFF_sprhflip 0x00CC5
#define RAM_OFF_reloadtime 0x00CC7
#define RAM_OFF_sprattr 0x00CC9
#define RAM_OFF_sprpriority 0x00CCB
#define RAM_OFF_sprframe 0x00CCD
#define RAM_OFF_sprx 0x00CCF
#define RAM_OFF_spry 0x00CD1
#define RAM_OFF_sprvflip 0x00CD3
#define RAM_OFF_textdirtylo 0x00CD5
#define RAM_OFF_textdirtyhi 0x00CD7
#define RAM_OFF_printm 0x00CD9
#define RAM_OFF_collflag 0x00CDB
#define RAM_OFF_impactstep 0x00CDD
#define RAM_OFF_velstep 0x00CDF
#define RAM_OFF_gravstep 0x00CE1
#define RAM_OFF_pausetimer 0x00CE3
#define RAM_OFF_ThirdPBonus 0x00CE5
#define RAM_OFF_cheercnt 0x00CE7
#define RAM_OFF_cheervc 0x00CE9
#define RAM_OFF_Hpos 0x00CEB
#define RAM_OFF_Hscroll 0x00CED
#define RAM_OFF_HmAwBonus 0x00CEF
#define RAM_OFF_iflags 0x00CF1
#define RAM_OFF_lastplayer 0x00CF3
#define RAM_OFF_lldisp 0x00CF7
#define RAM_OFF_nibblebuffer 0x00CF9
#define RAM_OFF_oldvcount 0x00D01
#define RAM_OFF_passdir 0x00D03
#define RAM_OFF_passspeed 0x00D05
#define RAM_OFF_passReceiverPlayerNum 0x00D07
#define RAM_OFF_PKBonus 0x00D09
#define RAM_OFF_PPBonus 0x00D0B
#define RAM_OFF_printa 0x00D0D
#define RAM_OFF_printx 0x00D0F
#define RAM_OFF_printy 0x00D11
#define RAM_OFF_psfirst 0x00D13
#define RAM_OFF_psitem 0x00D15
#define RAM_OFF_puckcross 0x00D17
#define RAM_OFF_ReplayBufferPtr 0x00D1F
#define RAM_OFF_Oldrow 0x00D23
#define RAM_OFF_vramrow 0x00D25
#define RAM_OFF_shotplayer 0x00D29
#define RAM_OFF_sfxmutetime 0x00D2B
#define RAM_OFF_threat 0x00D2D
#define RAM_OFF_demoflag 0x00D2F
#define RAM_OFF_TickerNum 0x00D31
#define RAM_OFF_tickervc 0x00D33
#define RAM_OFF_lastframevc 0x00D35
#define RAM_OFF_Vpos 0x00D37
#define RAM_OFF_Vscroll 0x00D39
#define RAM_OFF_wcradiusx 0x00D3B
#define RAM_OFF_wcradiusy 0x00D3D
#define RAM_OFF_PlayerScrollCtr 0x00D3F
#define RAM_OFF_screentimer 0x00D41
#define RAM_OFF_SelectedPlayerIdx 0x00D43
#define RAM_OFF_ScoutingReportTimer 0x00D45
#define RAM_OFF_DispAttribCtr 0x00D47
#define RAM_OFF_VertLineScrolling 0x00D49
#define RAM_OFF_xc1 0x00D4B
#define RAM_OFF_yc1 0x00D4D
#define RAM_OFF_yleader 0x00D4F
#define RAM_OFF_vcountmark 0x00D51
#define RAM_OFF_Xpos 0x00D55
#define RAM_OFF_Xfrac 0x00D77
#define RAM_OFF_frame 0x00D97
#define RAM_OFF_Ypos 0x00DB7
#define RAM_OFF_pucky 0x00DD3
#define RAM_OFF_Yfrac 0x00DD9
#define RAM_OFF_puckz 0x00DF9
#define RAM_OFF_shadowz 0x00DFB
#define RAM_OFF_puckzfrac 0x00DFD
#define RAM_OFF_puckoldz 0x00DFF
#define RAM_OFF_puckvz 0x00E01
#define RAM_OFF_attribute 0x00E03
#define RAM_OFF_oldframe 0x00E23
#define RAM_OFF_OldXpos 0x00E43
#define RAM_OFF_OldXfrac 0x00E63
#define RAM_OFF_OldYpos 0x00E83
#define RAM_OFF_OldYfrac 0x00EA3
#define RAM_OFF_Xvel 0x00EC3
#define RAM_OFF_puckvx 0x00EDF
#define RAM_OFF_Yvel 0x00EE3
#define RAM_OFF_puckvy 0x00EFF
#define RAM_OFF_impactp 0x00F03
#define RAM_OFF_impact 0x00F23
#define RAM_OFF_position 0x00F43
#define RAM_OFF_assnum 0x00F63
#define RAM_OFF_asslist 0x00F83
#define RAM_OFF_temp1 0x01003
#define RAM_OFF_temp2 0x01023
#define RAM_OFF_temp3 0x01043
#define RAM_OFF_temp4 0x01063
#define RAM_OFF_temp5 0x01083
#define RAM_OFF_temp2b 0x010A3
#define RAM_OFF_radiusx 0x010C3
#define RAM_OFF_radiusy 0x010E3
#define RAM_OFF_Wallcos 0x01103
#define RAM_OFF_Wallsin 0x01123
#define RAM_OFF_SCnum 0x01143
#define RAM_OFF_facedir 0x01163
#define RAM_OFF_facefrac 0x01183
#define RAM_OFF_SPA 0x011A3
#define RAM_OFF_SPAnum 0x011C3
#define RAM_OFF_SPAcnt 0x011E3
#define RAM_OFF_nopuck 0x01203
#define RAM_OFF_nopuckb 0x01223
#define RAM_OFF_newpos 0x01243
#define RAM_OFF_newpnum 0x01263
#define RAM_OFF_pflags 0x01283
#define RAM_OFF_pnum 0x012A3
#define RAM_OFF_weight 0x012C3
#define RAM_OFF_legstr 0x012E3
#define RAM_OFF_legspd 0x01303
#define RAM_OFF_aioff 0x01323
#define RAM_OFF_aidef 0x01343
#define RAM_OFF_shotspd 0x01363
#define RAM_OFF_shotacc 0x01383
#define RAM_OFF_passacc 0x013A3
#define RAM_OFF_rostnum 0x013C3
#define RAM_OFF_spodds 0x013E3
#define RAM_OFF_stickhand 0x01403
#define RAM_OFF_endurance 0x01423
#define RAM_OFF_aggress 0x01443
#define RAM_OFF_fighting 0x01463
#define RAM_OFF_checking 0x01483
#define RAM_OFF_handed 0x014A3
#define RAM_OFF_pfgoal 0x014C3
#define RAM_OFF_pfalock 0x014E3
#define RAM_OFF_pfteam 0x01503
#define RAM_OFF_pfrev 0x01523
#define RAM_OFF_pf2aip 0x01543
#define RAM_OFF_pfnc 0x01563
#define RAM_OFF_Ylist 0x01583
#define RAM_OFF_OOlistpos 0x015A3
#define RAM_OFF_OOlist 0x015C3
#define RAM_OFF_gmode 0x015D4
#define RAM_OFF_gmclock 0x015D6
#define RAM_OFF_sflags 0x015D8
#define RAM_OFF_sfhormode 0x015DA
#define RAM_OFF_sflags3 0x015DC
#define RAM_OFF_Pencntdwn 0x01620
#define RAM_OFF_pentimer 0x01622
#define RAM_OFF_PBnum 0x01624
#define RAM_OFF_InjCntDown 0x01626
#define RAM_OFF_penmsgtimer 0x01628
#define RAM_OFF_RefCnt 0x0162A
#define RAM_OFF_RefStep 0x0162C
#define RAM_OFF_RefPen 0x0162E
#define RAM_OFF_gsp 0x01630
#define RAM_OFF_gameclock 0x01632
#define RAM_OFF_TempPlOffset 0x01636
#define RAM_OFF_IsBreakaway 0x01638
#define RAM_OFF_PenShotActive 0x0163A
#define RAM_OFF_PenSumLength 0x0163C
#define RAM_OFF_PenSum 0x0163E
#define RAM_OFF_tmPwrGoals 0x01742
#define RAM_OFF_tmPwrPlays 0x01746
#define RAM_OFF_tmPenalties 0x0174A
#define RAM_OFF_tmPenmin 0x0174E
#define RAM_OFF_tmATOP 0x01752
#define RAM_OFF_tmPwrPlayTime 0x01756
#define RAM_OFF_tmPPShots 0x0175A
#define RAM_OFF_tmSHShots 0x0175E
#define RAM_OFF_tmSHGoals 0x01762
#define RAM_OFF_tmsort 0x01766
#define RAM_OFF_tmPerShots 0x0176A
#define RAM_OFF_tmPerGoals 0x0177E
#define RAM_OFF_tmscore 0x0178E
#define RAM_OFF_tmfowon 0x01792
#define RAM_OFF_tmline 0x01796
#define RAM_OFF_tmtouch1 0x0179A
#define RAM_OFF_tmtouch2 0x0179E
#define RAM_OFF_tmtouch3 0x017A2
#define RAM_OFF_tmap 0x017A6
#define RAM_OFF_tmgoalie 0x017AA
#define RAM_OFF_tmnear 0x017AE
#define RAM_OFF_tmnearpl 0x017B2
#define RAM_OFF_tmchecks 0x017B6
#define RAM_OFF_tmpasscomp 0x017BA
#define RAM_OFF_tmpassatt 0x017BE
#define RAM_OFF_tmBreakaways 0x017C2
#define RAM_OFF_tmBreakawayGoals 0x017C6
#define RAM_OFF_tmPenShots 0x017CA
#define RAM_OFF_tmPenShotGoals 0x017CE
#define RAM_OFF_tmOneTimerSwings 0x017D2
#define RAM_OFF_tmOneTimers 0x017D6
#define RAM_OFF_tmOneTimerGoals 0x017DA
#define RAM_OFF_tmflags 0x017DE
#define RAM_OFF_tmtimeouts 0x017E2
#define RAM_OFF_tmlcchoice 0x017E6
#define RAM_OFF_tmpde 0x017EA
#define RAM_OFF_tmpdst 0x01852
#define RAM_OFF_tmboxlist 0x018BA
#define RAM_OFF_tmgoals 0x018EE
#define RAM_OFF_tmassists 0x01922
#define RAM_OFF_tmpshots 0x01956
#define RAM_OFF_ppenmin 0x0198A
#define RAM_OFF_CwdExciteLvl 0x019BE
#define RAM_OFF_CrowdLoud 0x019C0
#define RAM_OFF_CurCrowdMeter 0x019C2
#define RAM_OFF_CrowdPeak 0x019C4
#define RAM_OFF_CrowdAvg 0x019C6
#define RAM_OFF_CrowdMeterCount 0x019C8
#define RAM_OFF_CrowdMeterSum 0x019CA
#define RAM_OFF_HotColdRaised 0x019CE
#define RAM_OFF_teampads 0x019D0
#define RAM_OFF_playerpad 0x019D4
#define RAM_OFF_c1playernum 0x019EC
#define RAM_OFF_padside 0x019F6
#define RAM_OFF_mp5connect 0x01A00
#define RAM_OFF_mp5readdelay 0x01A02
#define RAM_OFF_mp5checktimer 0x01A04
#define RAM_OFF_mp5port1last 0x01A06
#define RAM_OFF_mp5port2last 0x01A08
#define RAM_OFF_mp5portchg 0x01A0A
#define RAM_OFF_mp5present 0x01A0C
#define RAM_OFF_mp5warning 0x01A0E
#define RAM_OFF_InIntermission 0x01A10
#define RAM_OFF_HotColdTable 0x01A12
#define RAM_OFF_PerTimeTotal 0x01C82
#define RAM_OFF_cont1team 0x01C84
#define RAM_OFF_cont2team 0x01C86
#define RAM_OFF_cont3team 0x01C88
#define RAM_OFF_cont4team 0x01C8A
#define RAM_OFF_cont5team 0x01C8C
#define RAM_OFF_goaliemode1 0x01C90
#define RAM_OFF_goaliemode2 0x01C92
#define RAM_OFF_defcontrol 0x01C94
#define RAM_OFF_HomeTeam 0x01C98
#define RAM_OFF_VisTeam 0x01C9A
#define RAM_OFF_tmdata 0x01CDC
#define RAM_OFF_tmteam 0x01CE0
#define RAM_OFF_lcpad 0x01D55
#define RAM_OFF_jmlvector 0x01D59
#define RAM_OFF_asv 0x01D5E
#define RAM_OFF_PauseUnused 0x01D60
#define RAM_OFF_sndbank 0x01D68
#define RAM_OFF_gameframes 0x01D6D
#define RAM_OFF_homeshooters 0x01D71
#define RAM_OFF_awayshooters 0x01D79
#define RAM_OFF_shootoutmode 0x01D81
#define RAM_OFF_shootoutstop 0x01D83
#define RAM_OFF_shootoutround 0x01D85
#define RAM_OFF_shootoutturnover 0x01D87
#define RAM_OFF_shootoutdecider 0x01D89
#define RAM_OFF_linecursor 0x01D8B
#define RAM_OFF_lineplayer 0x01D8F
#define RAM_OFF_lineswap 0x01D93
#define RAM_OFF_linemenuitem 0x01D97
#define RAM_OFF_curplayer 0x01D9B
#define RAM_OFF_lineslots 0x01D9F
#define RAM_OFF_listcount 0x01DA3
#define RAM_OFF_picktop 0x01DA7
#define RAM_OFF_pickfirst 0x01DAB
#define RAM_OFF_pickrows 0x01DAF
#define RAM_OFF_curitem 0x01DB9
#define RAM_OFF_listtop 0x01DBD
#define RAM_OFF_listend 0x01DC1
#define RAM_OFF_listrows 0x01DC5
#define RAM_OFF_statstemp 0x01DC9
#define RAM_OFF_scrollirqdone 0x01DCD
#define RAM_OFF_crowdmeterlit 0x01DD1
#define RAM_OFF_otherskip 0x01DE9
#define RAM_OFF_irqline1 0x01DED
#define RAM_OFF_irqline2 0x01DEF
#define RAM_OFF_irqline3 0x01DF1
#define RAM_OFF_irqvscroll1 0x01DF3
#define RAM_OFF_irqvscroll2 0x01DF5
#define RAM_OFF_irqvscroll3 0x01DF7
#define RAM_OFF_statcolumn 0x01DF9
#define RAM_OFF_statsorder 0x01DFB
#define RAM_OFF_statpenrows 0x01E15
#define RAM_OFF_letterx 0x01E17
#define RAM_OFF_lettery 0x01E1B
#define RAM_OFF_otgoalscorer 0x01E1F
#define RAM_OFF_starplayerend 0x01E21
#define RAM_OFF_stargoalieend 0x01E23
#define RAM_OFF_starswapplayer 0x01E25
#define RAM_OFF_starswapscore 0x01E27
#define RAM_OFF_star1 0x01E2B
#define RAM_OFF_star2 0x01E2D
#define RAM_OFF_star3 0x01E2F
#define RAM_OFF_star1score 0x01E31
#define RAM_OFF_star2score 0x01E35
#define RAM_OFF_star3score 0x01E39
#define RAM_OFF_matchuptimer 0x01E3D
#define RAM_OFF_matchuphold 0x01E3F
#define RAM_OFF_hotterteam 0x01E41
#define RAM_OFF_hotextreme 0x01E43
#define RAM_OFF_hotcoldsum 0x01E45
#define RAM_OFF_hotplayer1 0x01E49
#define RAM_OFF_hotplayer2 0x01E4D
#define RAM_OFF_hotplayer3 0x01E51
#define RAM_OFF_coldplayer1 0x01E55
#define RAM_OFF_coldplayer2 0x01E59
#define RAM_OFF_coldplayer3 0x01E5D
#define RAM_OFF_hotvalue1 0x01E61
#define RAM_OFF_hotvalue2 0x01E65
#define RAM_OFF_hotvalue3 0x01E69
#define RAM_OFF_coldvalue1 0x01E6D
#define RAM_OFF_coldvalue2 0x01E71
#define RAM_OFF_coldvalue3 0x01E75
#define RAM_OFF_strengths 0x01E79
#define RAM_OFF_cardframechars 0x01E85
#define RAM_OFF_picattr 0x01E87
#define RAM_OFF_onetimer 0x01E89
#define RAM_OFF_breakaway 0x01E8B
#define RAM_OFF_BA_Sktr_SCnum 0x01E8D
#define RAM_OFF_BA_Skater_Offset 0x01E8F
#define RAM_OFF_BA_Team 0x01E91
#define RAM_OFF_pspad 0x01E93
#define RAM_OFF_psfoxsave 0x01E97
#define RAM_OFF_psfoysave 0x01E99
#define RAM_OFF_penshot 0x01E9B
#define RAM_OFF_penshotlive 0x01E9D
#define RAM_OFF_pstimer 0x01E9F
#define RAM_OFF_sopathx 0x01EA1
#define RAM_OFF_sopathy 0x01EA3
#define RAM_OFF_sopathnext 0x01EA5
#define RAM_OFF_sopathpoint 0x01EA7
#define RAM_OFF_sopathside 0x01EA9
#define RAM_OFF_sopathface 0x01EAB
#define RAM_OFF_lastperiod 0x01EAD
#define RAM_OFF_teamtaken 0x01EAF
#define RAM_OFF_teambit 0x01EB3
#define RAM_OFF_pmenulist 0x01EB7
#define RAM_OFF_pmenucount 0x01EB9
#define RAM_OFF_pregamepause 0x01EBB
#define RAM_OFF_RAMTag2 0x01EBD
#define RAM_OFF_HandleTable 0x02000
#define RAM_OFF_FrameCacheAddr 0x02540
#define RAM_OFF_FrameCacheBank 0x02640
#define RAM_OFF_FrameCacheOf 0x02740
#define RAM_OFF_FrameCacheNum 0x02B40
#define RAM_OFF_FrameCacheAlloc 0x02C40
#define RAM_OFF_FrameCacheUse 0x02D40
#define RAM_OFF_FrameCacheTile 0x02E40
#define RAM_OFF_BlockRuns 0x02F40
#define RAM_OFF_BlockRunPrev 0x02FB9
#define RAM_OFF_TileRuns 0x03032
#define RAM_OFF_TileRunPrev 0x03093
#define RAM_OFF_carriedy 0x030F4
#define RAM_OFF_ltplayer 0x030F6
#define RAM_OFF_ltx 0x030F8
#define RAM_OFF_lty 0x030FA
#define RAM_OFF_fobox 0x03120
#define RAM_OFF_fodir1 0x03122
#define RAM_OFF_fodir2 0x03124
#define RAM_OFF_fodropx 0x03126
#define RAM_OFF_fodropy 0x03128
#define RAM_OFF_fofdata2 0x0312A
#define RAM_OFF_fofdata 0x03132
#define RAM_OFF_fox 0x03136
#define RAM_OFF_foy 0x03138
#define RAM_OFF_ScoreSumbytes 0x0313A
#define RAM_OFF_hatx 0x03394
#define RAM_OFF_haty 0x033A8
#define RAM_OFF_hatz 0x033BC
#define RAM_OFF_hatvx 0x033D0
#define RAM_OFF_hatvy 0x033E4
#define RAM_OFF_hatvz 0x033F8
#define RAM_OFF_hatstep 0x0340C
#define RAM_OFF_hattimer 0x03420
#define RAM_OFF_hatkind 0x03434
#define RAM_OFF_hattrick 0x03448
#define RAM_OFF_refwindow 0x0344A
#define RAM_OFF_penmsgbox 0x0344C
#define RAM_OFF_newperiod 0x0344E
#define RAM_OFF_pspuckback 0x03450
#define RAM_OFF_setuphome 0x03452
#define RAM_OFF_setupvis 0x03454
#define RAM_OFF_teamschanged 0x03456
#define RAM_OFF_cardtimer 0x03458
#define RAM_OFF_matchuphome 0x0345A
#define RAM_OFF_matchupvis 0x0345C
#define RAM_OFF_setupmatchup 0x0345E
#define RAM_OFF_redrawblocks 0x03460
#define RAM_OFF_horblocks 0x03462
#define RAM_OFF_cardblock 0x03464
#define RAM_OFF_bigtextchar 0x03466
#define RAM_OFF_drawlinesback 0x03468
#define RAM_OFF_pausesubmenu 0x0346A
#define RAM_OFF_phototeams 0x0346C
#define RAM_OFF_homepicaddr 0x03470
#define RAM_OFF_vispicaddr 0x03472
#define RAM_OFF_matchupphotos 0x03474
#define RAM_OFF_pausemenuon 0x03476
#define RAM_OFF_playerstatson 0x03478
#define RAM_OFF_gamesstarted 0x0347A
#define RAM_OFF_OptPlayMode 0x034BC
#define RAM_OFF_OptTeam1 0x034BE
#define RAM_OFF_OptTeam2 0x034C0
#define RAM_OFF_OptPerlen 0x034C2
#define RAM_OFF_OptPen 0x034C4
#define RAM_OFF_OptLine 0x034C6
#define RAM_OFF_OptGoalie 0x034C8
#define RAM_OFF_OptPlayers 0x034CA
#define RAM_OFF_notdemo 0x034CC
#define RAM_OFF_pojoy 0x034CE
#define RAM_OFF_postarts 0x034D0
#define RAM_OFF_potree 0x034D2
#define RAM_OFF_potreeteam 0x034F2
#define RAM_OFF_PassWord 0x034F4
#define RAM_OFF_passbits 0x03508
#define RAM_OFF_tpassbits 0x03518
#define RAM_OFF_gst1 0x03528
#define RAM_OFF_gst2 0x03538
#define RAM_OFF_gspotwins 0x03548
#define RAM_OFF_gspobwins 0x03558
#define RAM_OFF_gsper 0x03568
#define RAM_OFF_gss1 0x03578
#define RAM_OFF_gss2 0x03588
#define RAM_OFF_gsflags 0x03598
#define RAM_OFF_printfontset 0x035A8
#define RAM_OFF_charofs 0x035AA
#define RAM_OFF_charofs2 0x035AC
#define RAM_OFF_BigFontChars 0x035AE
#define RAM_OFF_EASNcset 0x035B0
#define RAM_OFF_cardbackchars 0x035B2
#define RAM_OFF_backchars 0x035B4
#define RAM_OFF_smallfontchars 0x035B6
#define RAM_OFF_centericechars 0x035B8
#define RAM_OFF_faceoffvrcset 0x035BA
#define RAM_OFF_Framercset 0x035BC
#define RAM_OFF_ninetilechars 0x035BE
#define RAM_OFF_outlinechars 0x035C0
#define RAM_OFF_ExtraChars 0x035C2
#define RAM_OFF_nextfreechar 0x035C4
#define RAM_OFF_cupchars 0x035C6
#define RAM_OFF_refcharshor 0x035C8
#define RAM_OFF_refchars 0x035CA
#define RAM_OFF_scorechars 0x035CE
#define RAM_OFF_blankchar 0x035D0
#define RAM_OFF_picchars 0x035D2
#define RAM_OFF_playoffRoundIdx 0x035D4
#define RAM_OFF_gamenum 0x035D6
#define RAM_OFF_vcountfrac 0x035D8
#define RAM_OFF_vcountstep 0x035DA
#define RAM_OFF_tickerstart 0x035DC
#define RAM_OFF_Crowdlevel 0x035E0
#define RAM_OFF_SPCStreamLeft 0x035E2
#define RAM_OFF_streamsfx 0x035E6
#define RAM_OFF_streamstate 0x035E8
#define RAM_OFF_streamloaded 0x035EA
#define RAM_OFF_SPCStreamPtr 0x035EC
#define RAM_OFF_replaypanel 0x035F0
#define RAM_OFF_usehotcold 0x035F2
#define RAM_OFF_palbuffer 0x035F4
#define RAM_OFF_textmap 0x037F4
#define RAM_OFF_textmap2 0x03FF4
#define RAM_OFF_hlsavebuf 0x047F4
#define RAM_OFF_bigtextbuf 0x04EF4
#define RAM_OFF_screenmap 0x0A670
#define RAM_OFF_screenmap2 0x0C676
#define RAM_OFF_ReplayBufferEnd 0x0E676
#define RAM_OFF_OAMHiMasks 0x0E67C
#define RAM_OFF_titleteams 0x0F000
#define RAM_OFF_Ram7F 0x10000
#define RAM_OFF_introbuf 0x14000
#define RAM_OFF_brackettiles 0x16700
#define RAM_OFF_colourbuf 0x17500
#define RAM_OFF_logoboxmap 0x17700
#define RAM_OFF_picboxmap 0x17780
#define RAM_OFF_titlescale 0x18000
#define RAM_OFF_introchars 0x18010
#define RAM_OFF_titlexofs 0x18A00
#define RAM_OFF_titleyscroll 0x19400
#define RAM_OFF_titlelogobuf 0x1A000

extern struct G g;
