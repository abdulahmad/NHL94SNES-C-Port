# The game's RAM

The SNES has 128 KB of work RAM ($7E:0000-$7F:FFFF; its first 8 KB is also the low 8 KB
of banks $00-$3F and $80-$BF, where the game reaches most of its variables). Here it is `struct G g`
in `game/ram.h`, every field at its work RAM offset (packed), with the disassembly's comment beside
it; `_Static_assert`s keep each at its original offset. `ram.h` is generated from the disassembly's
RAM map (`ram94s.asm`) and is the complete list; `consts.h` has the masks and counts that go with
it. This page is a map into it.

## The layout

| Offset | What |
|---|---|
| $0000-$00D0 | The direct page: the routines' scratch registers (`r0`-`r9`, `p0`, `p1`, `a0`-`a5`, `d0`-`d6`, `tmp0`-`tmp5`), which the C keeps as arguments and locals; `Vcount` and `d7` |
| $0100-$04FF | The DMA queue: tiles and maps waiting for the NMI to send them to VRAM |
| $0500-$1EC2 | The game's variables: the interrupts and their hooks, the pads, the sprites, the sound sequencer, every object, the teams, the clock |
| $1EC3-$1FFF | The stack |
| $2000-$30FF | The heap's handles and the sprite tile cache (which frames' tiles are in VRAM) |
| $3100-$35F3 | More variables: scores and stats, the options, the passwords, the sound stream |
| $35F4-$FFFF | Bank $7E's buffers: the palette, the text maps, the screen maps, the replay buffer |
| $10000-$1FFFF | Bank $7F (`Ram7F`): the intro's and title's buffers, the menus' colours and maps |

## The objects

The players, the puck and the nets are objects 0-15, each variable an array of 16 indexed by the
object (the 65816's X register, object * 2: in C `g.facedir[o]` is `facedir,x`). For example
`position` ($0F43: goalie, defense, wings, centre, or none), `facedir` ($1163: 0-7), `pfteam`
($1503: home or visitors), the coordinates, speeds, animation, ratings and assignment. The puck is
object 14 (`puckobj` in `consts.h`, X = $1C).

## Variables worth knowing

| Variable | Offset | What |
|---|---|---|
| `g.Vcount` | $0083 | frames so far (a 32-bit count; the NMI adds 1, PAL's extra steps through `vcountfrac`) |
| `g.OldVcount` | $0D01 | `Vcount` at the last game loop pass |
| `g.d7` | $00C1 | frames since the last pass: every speed and timer is scaled by it |
| `g.PadRaw1` | $0776 | the five pads' buttons held (the NMI reads them); `g.padnew1` ($0AB8) the newly pressed |
| `g.mp5present` | $1A0C | the MultiPlayer 5 multitap is in port 2 (pads 3-5 are read) |
| `g.NMIHook`, `g.FrameHook`, `g.IRQHook` | $078C, $0789, $078F | the routines the interrupts jump to (long addresses; `call_long` runs them) |
| `g.DMAList` | $0100 | the DMA queue's 8-byte entries (`DMAlisthead`, `DMAlistend` at $3A, $3C) |
| `g.IniDispShadow` | $0762 | the screen's brightness and forced blank, written by the NMI |
| `g.seed` | $0792 | `RandomWord`'s two generators |
| `g.gmode`, `g.sflags` | $15D4, $15D8 | the game's mode and state flags (clock stopped, pause, shootout ...; `gm*`, `sf*` in `consts.h`) |
| `g.gameclock` | $1632 | the period's clock: seconds, then the fraction |
| `g.CwdExciteLvl` | $19BE | the crowd's excitement |
| `g.HomeTeam`, `g.VisTeam` | $1C98, $1C9A | the two teams |
| `g.tmdata` | $1CDC | the teams' data addresses in the ROM |
| `g.shootoutmode` | $1D81 | a shootout is on |
| `g.OptPlayMode` ... | $34BC | the setup screen's choices: play mode, teams, period length, penalties, line changes ... |

Search `ram.h` for a name or an offset to find any other.
