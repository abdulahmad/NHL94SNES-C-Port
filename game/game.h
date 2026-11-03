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
#include "screen/cards.h"
#include "play/collide.h"
#include "screen/crowd.h"
#include "screen/video.h"
#include "play/hockey.h"
#include "play/logic.h"
#include "menus/optsetup.h"
#include "play/penalty.h"
#include "play/period.h"
#include "screen/replay.h"
#include "screen/scout.h"
#include "menus/setup.h"
#include "play/shootout.h"
#include "system/snes.h"
#include "sound/sound.h"
#include "menus/stats.h"
#include "system/middle.h"
