# NHL '94 in C (SNES)

> **Credits:** built on [NHL94SNES](https://github.com/abdulahmad/NHL94SNES) by abdulahmad, with chaos, StaticEck, McMarkis (Mhopkinsinc) and Drezz, and special thanks to smozoma and kingraph. See [Credits](#credits).

The whole of NHL '94 for the SNES as C code: about 21,000 lines, one
file per module of the original assembly and one function per routine, each named after its
original label. It plays as the cartridge does, at the cartridge's speed (Game Speed Original, the
default) or faster (Smooth), and nothing more: no rule changes, no other teams, no online play. You need
your own dump of the US cartridge to play it; the game reads its graphics, sound and data from it.

- **Read the code:** `docs/HOW-IT-WORKS.md` (how it fits together, from power on to a frame of
  play), `docs/ROUTINES.md` (every function, its original routine and what it does),
  `docs/RAM.md` (the game's variables).
- **The game:** `game/` (play, screen, menus, sound, system). The console under it: `platform/`
  (the hardware the game touches, the cartridge's clock, a window) and `runtime/` (the picture
  processor, DMA, the pads, the sound chip and sound CPU).

## Build and play

Needs gcc, make, Python 3 and SDL2 (Windows: MSYS2's UCRT64 shell; `pacman -S
mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 make`), and your own dump of the cartridge:
the ROM is not provided. Put it beside the Makefile (any name ending `.smc` or `.sfc`) or give it
as `make ROM=path`. The sound CPU's program, the game's sound driver, is made from it at build
time (`tools/gen_spc.py`), so the driver is never in this repository. The dump these runs are
checked with has the SHA-1 `10dcd21be9c5542e82b0b94473bf5ff569b36cf5` (without a copier header;
one is fine).

```
make
./build/snes94 nhl94.smc
```

(Windows: run it from the UCRT64 shell, or `make dist` for `build/dist/snes94.exe`, one file
with SDL2 in it. Drag a ROM onto it, or put it beside it: any `.smc` that is the game.)

## Controls

| | Pad 1 | Pad 2 | Controller | In play |
|---|---|---|---|---|
| Directions | arrows | I J K L | d-pad, left stick | skate |
| B | Z | F | south | pass, switch player |
| A | X | G | east | shoot, check, speed burst |
| Y | A | R | west | dump in, hook or hold |
| X | S | T | north | take the goalie |
| L / R | Q / W | E / Y | shoulders | take the left / right defenseman |
| Select | Right Shift | Left Shift | Back | |
| Start | Enter | Tab | Start | pause |

## The settings menu

F9 opens it, Escape (or F9) closes it, and the game waits while it is open. Up / Down choose,
Enter (or Left / Right) changes; on a controller, the d-pad or stick, the south button or Start,
and Back to close. Each change is saved to `settings.ini` at once.

- **Controllers...**: the controller setup screen, each button asked for in turn, then the stick's
  dead zone.
- **Show FPS** (off): the game's new pictures a second, in the picture's top right corner (about
  30 in play at Original speed, 48 at Smooth).
- **Picture** (4:3): the picture's shape. 4:3 is a TV's, as the console's picture filled one;
  Native shows its pixels square (256 by 224), at the largest whole scale that fits the window.
- **Game Speed** (Original): see below.

## Players

Up to five: every key, each player's controller and the controllers' buttons are in
`settings.ini` (written beside the program the first time; controllers take slots 1-8 as they are
plugged in). With keys or a controller for a third, fourth or fifth player at power on, the
MultiPlayer 5 multitap is plugged in, as the cartridge needs it for more than two.

## Game Speed

On the cartridge a pass of the game loop is more work than the 65816 can do in one vblank: most
passes take two and some three, and the game moves everything by the vblanks that passed. The C
has no instruction timing, so at Original speed (the default) that time comes from a cost model of the
cartridge's CPU (`platform/clock.c`, measured on the cartridge in an emulator): the game runs at
the cartridge's speed, slowdown and all, close to it rather than exact. With Smooth the game's
work in a pass costs an eighth as much, as if the CPU were 8 times as fast (ninety-four-snes's
fast setting): a pass fits in one vblank and the game runs smoother and faster than the cartridge.

## Check it

```
test/check.sh nhl94.smc
```

plays seven scripted runs (a full game, a 30-second one at Original and Smooth speed, five players on
the multitap, the pause menus, random play, a shootout; 116,000 frames) headless and checks every
frame's hashes (work RAM, video memory, the sound CPU's RAM, the picture, the sound) against
`test/expected.txt`. The cartridge has no 30-second game (its shortest period is 5 minutes); for
those two runs `golden --period30` makes its 5 minutes 30 seconds.

## Where it comes from

The C is ninety-four-snes's, iDNi Studios' earlier work: the game rewritten routine by routine from the community's
byte-matching disassembly, [NHL94SNES](https://github.com/abdulahmad/NHL94SNES), and checked
against the cartridge running in an emulator (snes9x), routine by routine and frame by frame. This
repository is its game alone: its additions (save states, online play, the checking tools) are
taken out and the result checked frame for frame against it; its fast setting is Smooth. The sound
chip is a port of [ares](https://github.com/ares-emulator/ares)'s (ISC licence,
`runtime/LICENSE-ares`); the controller setup screen, its font and the settings file are
[pulseman](https://github.com/1nputs/pulseman)'s (MIT, `platform/LICENSE`). The rest is iDNi
Studios' (MIT, `LICENSE`).

## Credits

This port is built on the community's byte-matching disassembly **NHL94SNES** by
**abdulahmad** (https://github.com/abdulahmad/NHL94SNES): every routine, RAM and file name here comes from it. Thanks to
**chaos**, **StaticEck**, **McMarkis (Mhopkinsinc)** and **Drezz** for their help with that
disassembly, with special thanks to **smozoma** and **kingraph**. The C is **iDNi Studios**' (INDIO), from its earlier ninety-four-snes; the controller setup, its font and the settings file are from **1nput**'s [pulseman](https://github.com/1nputs/pulseman).

Anyone building on this repository (a mod, hack, port, fork or release) must carry this credit,
near the top of their README and in the notes of every release and any text file shipped in a
release archive. See AGENTS.md.
