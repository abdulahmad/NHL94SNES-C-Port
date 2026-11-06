/* The game logic, rewritten in C from the community's byte-matching disassembly of the cartridge
 * (NHL94SNES, github.com/abdulahmad/NHL94SNES): one C file per module of it (each file's first line
 * names its folder, src/routines/MODULE, one .asm file a routine), every routine under its label.
 *
 * Conventions. An object (the 65816's X, an object index: object * 2) is passed as its number o,
 * 0-15: g.facedir[o] is facedir,x. The direct page's scratch registers (r0-r9, p0, p1, a0-a5,
 * d0-d6, tmp0-tmp5) are the routine's parameters, results and locals, as C arguments, return
 * values and locals; a result left in the Z flag is an int, nonzero for Z set. */
#pragma once
#include <stdint.h>
#include "consts.h"
#include "mem.h"
#include "symbols_gen.h"
#include "hal.h"
#include "snes.h"
#include "cards.h"
#include "collide.h"
#include "crowd.h"
#include "title.h"
#include "video.h"
#include "hockey.h"
#include "logic.h"
#include "optsetup.h"
#include "penalty.h"
#include "period.h"
#include "replay.h"
#include "scout.h"
#include "setup.h"
#include "shootout.h"
#include "sound.h"
#include "stats.h"
#include "middle.h"
#include "print.h"
#include "decompress.h"
#include "main.h"

/* src/data/tables/asstab.asm slot indexes (this ROM's numbers; 92G a* / p* names, 94S extras) */
enum {
    anull = 0x00, adefo = 0x01, adefd = 0x02, awingd = 0x03, awingo = 0x04,
    acenterd = 0x05, acentero = 0x06, agoalie = 0x07, ascore = 0x08, astanley = 0x09,
    aeben = 0x0A, aepen = 0x0B, abench = 0x0C, apenalty = 0x0D, adopen = 0x0E,
    agoalietopuck = 0x0F, abreakaway = 0x10, apuckc = 0x11, anearest = 0x12, ashoot = 0x13,
    adumpin = 0x14, apassrec = 0x15, afaceoff = 0x16, afaceoffpl = 0x17, pnorm = 0x18,
    pshad = 0x19, pnothing = 0x1A, pfaceoff = 0x1B, pfaceoff2 = 0x1C
};
