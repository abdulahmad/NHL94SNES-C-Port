/* Generated (ninety-four-snes's tools/gen_ram.py) from the disassembly's RAM map (ram94s.asm). Its constants:
 * masks, object indexes, counts, penalty numbers. */
#pragma once

#define awaylines 0x1D1C  /* the visitors lines, as homelines */
#define blueline 0x0058  /* puck y past which a team can be offside */
#define camobj 0x0020  /* ReplayMode: object slot used as the replay camera (Xpos / Ypos and the wall check, as 93G CameraPosStruct) */
#define cbut 0x0080  /* shoot / check button in d2 */
#define gmdir 0x0002  /* 0 = home team goes up */
#define gmhl 0x0010  /* highlight mode */
#define gmoffs 0x0020  /* offsides on */
#define gmpen 0x0004  /* penalty called, ref in progress */
#define homelines 0x1CE4  /* the home team lines (7 lines of 8 slot bytes), loaded by LoadTeamLines, changed by the line editor */
#define logosprbuf 0x7F7800  /* team logo sprites, unpacked by LoadScoutingScreen, LoadPlayerCardScreen, SetupScreen and DrawMatchupPictures; DrawLogoSprite draws from here */
#define maxros 0x001A  /* roster slots per team (the visiting list starts maxros bytes on) */
#define mesareaEnd 0x0A48  /* one past the last digit PushNumber and PushTime may write */
#define MulShift 0x0040  /* MulFrac8: sta $40 = A*2, then asl $40 seven times; each carry (a bit of the high byte B) adds ... */
#define netobj1 0x0018  /* object index*2 of goal net 1 (y +268) */
#define netobj2 0x001A  /* object index*2 of goal net 2 (y -268) */
#define PenBuf 0x15DE  /* up to 32 penalties: (penalty number, object) word pairs, 0 = free */
#define PenCharging 0x0018
#define PenCrossChk 0x001E
#define PenDelay 0x0030
#define PenEOG 0x0004
#define PenEOP 0x0002
#define PenFighting 0x002A
#define PenFighting2 0x002C
#define PenGhold 0x0008
#define PenGoal 0x000E
#define PenHolding 0x0028
#define PenHooking 0x0020
#define PenIcing 0x000C
#define PenInst 0x002E
#define PenInterference 0x0026
#define PenNoText 0x0010
#define PenNoText2 0x0016
#define PenNoText3 0x0032
#define PenOffsides 0x0012
#define PenOOP 0x0006
#define PenRoughing 0x001C
#define PenRoughing2 0x0014
#define PenShot 0x0024
#define PenSlashing 0x001A
#define PenTripping 0x0022
#define PenWhistle 0x000A
#define pf2inj 0x0004  /* injured */
#define pfna 0x0002  /* new assignment */
#define PlList 0x7E3114  /* players wanted on the ice */
#define puckobj 0x001C  /* object index of the puck */
#define pwlength 0x0010  /* password characters */
#define PWrange 0x001E  /* characters in PasstoText */
#define ReplayBufferStart 0x7E577E  /* first replay frame record */
#define replaysize 0x007A  /* bytes per replay frame record */
#define rinkmapbuf 0x7FC800  /* BuildRinkMap unpacks IceRinkMap here and pastes the centre ice logo at +$10B6; ReplayMode and SetHor_rink send parts of it to VRAM */
#define rosterNone 0xFFFC  /* no roster slot (beside TMPDOUT in bench checks) */
#define ScoreSum 0x7E313C  /* Extra, not in the list (only as an immediate): the goal log Goal writes 6 bytes per goal into (93S ... */
#define sf2drec 0x0400  /* disable replay record */
#define sf2pwrplay 0x2000  /* power play in progress */
#define sf2pwrtm 0x4000  /* 0 for team 1 in power play */
#define sf2refref 0x0200  /* ref needs refresh */
#define sf2replay 0x0800  /* replay in progress */
#define sf3llcs 0x0001  /* lower line change box on screen */
#define sfpz 0x0001  /* pause mode */
#define sfslock 0x0040  /* scroll lock to xc1/yc1 */
#define sfspdir 0x0004  /* a pass direction is set (passdir) */
#define sfwrap 0x0010  /* replay has wrapped around */
#define sideline 0x0088  /* distance from center to side boards */
#define SoundBankGame 0x0001  /* sndbank: the in-game set (SPCIntroUpload and SPCTitleUpload set it too) */
#define SoundBankMenu 0x0002  /* sndbank: the menu set (SPCMenuUpload, SPCTitleToMenu); 0 = none */
#define statspage 0x7F2780  /* stats page buffer (32 wide) that PrintToStatsPage and the Fill*Page routines print into */
#define TestList 0x7E347C  /* scouting report text list */
#define titlesprbuf 0x7F5000  /* TitleSprites unpacked here; DrawTitleSprite draws sprite A of the set */
#define tmfoside 0x0010  /* team flagged offsides */
#define TMPDBENCH 0xFFFE  /* on the bench */
#define TMPDICE 0xFFFF  /* on the ice */
#define TMPDINJ 0x1000  /* injured: box time with this bit has no penalty */
#define TMPDOUT 0xFFFD  /* out of the game */
#define tmshots 0x177A  /* shots */
#define unpackbuf 0x7F0000  /* Decompress output buffer (bank $7F, as 93S unpackbuf): tiles, maps and palettes are unpacked here and sent to VRAM / CGRAM from here */
