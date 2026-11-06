# How the game works

NHL '94 for the SNES, as C. Each file in `game/` is one module of the
community's byte-matching disassembly of the cartridge
([NHL94SNES](https://github.com/abdulahmad/NHL94SNES): `hockey.c` is its `src/routines/hockey`),
and each function is one of its routines, named after its label (`docs/ROUTINES.md` lists them all
with their cartridge addresses). The C does what the 65816 did, step by step: same order, same
arithmetic, same RAM. That is why it plays exactly as the cartridge (`test/check.sh`).

## The pieces

| Where | What |
|---|---|
| `game/play/` | The game on the ice: the game loop, the title, the pause menus and the stats screens (`hockey.c`); the players' assignments, skating and the puck (`checks.c`, `assign.c`); the computer's decisions (`logic.c`); collisions, checks and energy (`collide.c`); the controls (`input.c`); penalties, the scoreboard and team stats (`penalty.c`); periods; the shootout |
| `game/screen/` | The picture: printing, the rink and sort objects (`video.c`), the score, the clock, the crowd's sound and the sort coordinates (`display.c`), the replay buffer and the objects' motion (`replay.c`), player cards (`cards.c`), the scouting report (`scout.c`), the hot / cold table (`crowd.c`) |
| `game/menus/` | The game setup screen (`optsetup.c`), setting up a game and the rink (`setup.c`), the roster and line screens (`stats.c`) |
| `game/sound/` | The sound CPU's uploads and the sound effects (`sound.c`) |
| `game/system/` | The engine (`snes.c`: the arithmetic the 65816 did with the console's multiplier and divider, the interrupts, the DMA queue, sprites and their tile cache, decompression, the music sequencer, the MultiPlayer 5), frame drawing, digits, the pads and random numbers (`middle.c`), passwords, playoffs and team data (`data.c`), RAM and the ROM |
| `game/*.h` | `game.h` (the conventions; includes every module's header), `ram.h` (work RAM: `docs/RAM.md`), `consts.h` (its masks and counts), `hal.h` (the hardware the game touches), `mem.h` (RAM and the ROM by address), `rom.h`, `symbols_gen.h` (every label's cartridge address) |
| `platform/` | The console under the game: `hal.c` (the registers to the runtime, the hooks), `clock.c` and `costs.inc` (the cartridge's time), `fnsyms.h` and `uncounted.txt` (which C routines the clock counts as the original's), `main.c` (the window), `menu.c` (the settings menu), `padsetup.c` (the controller setup screen), `settings.c` (`settings.ini`) |
| `runtime/` | The SNES hardware: the picture processor (`ppu.c`), DMA and HDMA, the CPU's I/O and the pads with the multitap (`io.c`), the sound chip (`dsp.c`) and the sound CPU running the game's sound driver (`spc.c`, and `build/spc_program.inc`, made from your ROM) |
| `tools/` | `gen_spc.py` (the sound CPU's programs as C, from your ROM), `gen_fnsyms.awk` (the table of C routines and their cartridge addresses: game/ is built with `-finstrument-functions`, and `hal.c` tells the clock each routine the 65816 would have entered), `routines.py` (`docs/ROUTINES.md`) |
| `test/` | `golden.c` (the game headless, a line of hashes a frame), `check.sh`, the input scripts |

## From power on

1. `Start` (`system/main.c`, `Reset_Fast`): the picture processor's registers cleared, VRAM
   cleared, the sound driver uploaded to the sound CPU (`SPCIPLUpload`), all of work RAM cleared,
   the interrupt hooks set, the NMI on, the sound started, then `Opening`.
2. `Opening` (`menus/setup.c`): the MultiPlayer 5 check, the publisher's logo and intro
   (`IntroSequence`, mode 7), the password and default options, the title (`TitleScreen`), then
   `Restart`: the setup screen (`menus/optsetup.c`), the matchup, a game.
3. A game starts with `StartGame` (`play/hockey.c`): the playoff screen, the hot / cold table, the
   scouting report, then each period through `StartPer`: the teams' energy, the rink and its
   graphics (`setupice`), the clock, the faceoff, the replay buffer.
4. `Gameloop` (`play/hockey.c`) makes passes until the period ends:
   - wait until at least one frame has passed (`hal_wait_interrupt`); the frames since the last
     pass are `d7`, which every speed and timer is scaled by;
   - read the pads, run the game clock (or the penalty shot's), the crowd's cheers;
   - `DoGameFrame`: `periodicevents` (penalties, the crowd, the sound, the clock; once a second
     the goalies, excitement, bench energy, injuries, the power play), `updateplayers` (every
     player's assignment, the AI, motion, the puck, collisions), the window, the replay buffer
     (`updatereplay`), `setvideo` (the sprites and scrolling for the next picture);
   - the pause menu (`Pausemode`) when Start was pressed.
5. Intermissions, stats, the three stars, the playoffs: each is a screen with its own small loop,
   in the module the original put it in.

## The interrupts

The 65816 does most of the console work in two interrupts, here ordinary C functions that the
clock calls at the moment the hardware would (`platform/clock.c`):

- **NMI** (every frame, at the vertical blank; `NMI` in `system/snes.c`): the NMI hook of the
  current screen, the IRQ timers, the scroll registers, the colour DMA, the DMA queue (tiles and
  maps the game queued during the frame, as far as the frame's DMA budget goes), the sprite table
  (`NMI_SendOAM`), the frame counter `vcount` (`NMI_Tick`), then the frame hook, the pads
  (`JoyPad`), the music sequencer (`SoundUpdate`) and the sound stream.
- **IRQ** (at a set line, when a screen splits its picture: the title, the stats screens, the
  rink's scoreboard band): `IRQ_Fast` jumps through `IRQHook`, and each `IRQ_*` routine changes
  scroll or layer registers mid-picture and sets the next.

A wait in the game (`hal_wait_interrupt`) is the original's loop polling RAM an interrupt changes
(`vcount`, `JoyPad`): the console runs to the next interrupt, which runs, and the loop looks again.

## How the C stands in for the 65816

- **RAM** is one struct, `g` (`ram.h`), laid out at the 65816's work RAM offsets: every variable
  sits at its original address, with the disassembly's comment beside it. `mem.h` reads and
  writes it by address (`r16(RAM_OFF_x)`), as the 65816 did through its index registers.
- **The direct page**: the original keeps parameters and scratch values in the direct page's
  registers (`r0`-`r9`, `a0`-`a5`, `d0`-`d6`, `p0`, `p1` ...). The C passes them as arguments,
  results and locals, named as the original names them (`game.h`). An object (a player, the puck)
  is the 65816's X register, object * 2; in C its number `o`, so `g.facedir[o]` is `facedir,x`.
- **The ROM** is the player's own cartridge: graphics, text, tables, rosters and the sound driver
  are read from it at run time by address (`rom8(SYM_x)`), so none of the game's art or data is in this
  repository.
- **Hardware**: every register access the 65816 made is a call in `hal.h` (`io_w8`, `io_r8`: one
  call, one byte, in the original's order), sent to the runtime by `platform/hal.c`. The multiplier
  and divider are not: the C does that arithmetic (`system/snes.c`, with the original's operand
  widths, signs and carries).
- **Jumps through RAM**: the original jumps to addresses kept in RAM (the NMI, IRQ and frame hooks
  each screen sets); `call_long` (`system/snes.c`) turns an address back into its C function.
  Where the original reset the stack and jumped (a new period, a restart), `hal_stack_reset` does
  the same to the C stack.
- **Sound**: the music sequencer runs on the 65816 (`system/snes.c`: tracks, voices, instruments)
  and sends commands through the four ports to the sound CPU, an SPC700 with its own 64 KB of RAM.
  Its program, the game's sound driver, is uploaded at power on; here it runs as C made
  instruction by instruction from its listing (`build/spc_program.inc`, which make writes from the
  cartridge with `tools/gen_spc.py`: the driver is the game's and stays on the cartridge), driving the sound
  chip (`runtime/src/dsp.c`, after ares).

## Time

The C has no instruction timing of its own, but the SNES game's speed depends on it: a pass of the
game loop is more work than one frame of the 65816's time, so on the cartridge most passes take
two frames and some three, and the game moves everything by the frames that passed (`d7`). So the
console's time comes from a cost model of the cartridge's CPU (`platform/clock.c`): each step of
the game's work (a routine's start, a register access) moves the beam on by what the cartridge
takes between those two steps (`platform/costs.inc`, measured on the cartridge in an emulator),
interrupts come when the beam reaches them, and a wait runs to the next one. `game/` is compiled
with `-finstrument-functions` so that each routine's start is such a step (`platform/hal.c`), and
where a loop's passes vary, `hal_cost_mark` marks each pass. The game therefore runs at the
cartridge's speed, slowdown and all. Being a model, it is close to the cartridge rather than exact:
the menus reach the game within a frame or two of the cartridge, and in live play a pass takes
three frames a little less often than on the cartridge (about 4% of passes against 6-7%). This is
Game Speed Original, the default; with Smooth (the window's settings menu, `--smooth` for `test/golden`)
the steps of a pass cost an eighth of the cartridge's, as if the 65816 were 8 times as fast
(ninety-four-snes's fast setting): a pass fits in one vblank, while the interrupts and the waits keep
the cartridge's time.

## Reading a routine

Open its file in `game/`, find it by name (or look it up in `docs/ROUTINES.md`). The comment
before it says what it does, with the registers it takes and leaves; comments inside in brackets
quote the original where the C needs a reason, such as `(adc: makepde's carry)`. The disassembly's
`.asm` file of the same name, in NHL94SNES, is the original it was written from.
