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
