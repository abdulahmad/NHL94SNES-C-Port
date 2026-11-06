# Alignment rules

What decides each kind of name and place in the C ports. Workers and the splitter read this first.

## Authority

| Category | Authority |
|---|---|
| Function name of an original routine | The current label in [NHL94SNES](https://github.com/abdulahmad/NHL94SNES). Old names reach it through that repository's `docs/name_map.csv` (`tools/check_names.py` follows the chain) |
| Asm local (`Parent.x`) | The C function `Parent_x`, `static` in the parent's file where nothing else calls it |
| Helper (a piece split off a routine, or the C's own) | `<Routine>_<what>`, `static` where possible; listed as a helper in docs/ROUTINES.md |
| C file | One file per asm module, named after it (`src/routines/collide/` is `game/collide.c`). `game/snes.c` holds only the port's glue (no asm module) |
| Order inside a file | ROM order, as the asm |
| RAM names (`ram.h`) | The asm RAM equates (`src/ram94s.asm`) |
| First comment line of a routine | its asm file and label (`collide94.asm:checkcoll.`) |
| Other games' names | never in comments; lineage lives in the asm repositories' `docs/name_map.csv` |

## Status (tools/check_names.py, 2026-11-06, after leftover recut)

| ok | other_module | renamed | missing |
|---|---|---|---|
| 844 | 0 | 6 | 5 |

855 routines. Missing: the title screen's and the intro's hooks (`TitleScreen_framehook`, `_irq1`-`_irq3` in
`title.c`, `IntroSequence_framehook` in `setup.c`), which have no label in the asm. Parked renamed: C `SYM_*_t1`/`_t2`
at other cartridge addresses than the asm's `Parent.t1`/`.t2` (`GetFrameTiles_drawat`, `DrawSpritePieces_cacheat` /
`_cacheframe`, `setoptions_t1xxxxxxxxxxx` / `t2xxx`, `IntroSequence_t1xxxxxxxxxxxx`).

## Learned

(One line per correction, newest last.)
* 2026-11-03: an asm local `Parent.x` is the C function `Parent_x`; the names gate counts them as the same name.
* 2026-11-03: renaming a routine renames its locals too (`old_x` to `new_x`, C functions and `SYM_`s): the clock's table pairs each C function with its `SYM_`, and a local left behind drops out of it (`tools/rename_routines.py` does both; check the table's size, `awk -f tools/gen_fnsyms.awk ...`, stays the same).
* 2026-11-03: parked (UNSURE): a rename whose new name is already another C name or `SYM_` (`randomd0sd0` to `randomd0s`, a C helper's name; the asm's anonymous locals `.t1`/`.t2` where the C has `SYM_..._t1` at another address: setoptions_t1xxxxxxxxxxx, setoptions_t2xxx, IntroSequence_t1xxxxxxxxxxxx, DrawSpritePieces_cacheat, DrawSpritePieces_cacheframe). They need the symbol file regenerated from the current asm, or names in the asm.
* 2026-11-03: the names gate must count one-line functions (body on the head's line): it skipped 149 routines, 33 of them in the wrong file and 17 under old names.
* 2026-11-03: `randomd0sd0` to `randomd0s` is no longer parked: renamed together with `randomd0s` to `randomas` (placeholders). `GetFrameTiles_drawat` is parked with the other `.t1`/`.t2` locals.
* 2026-11-03: re-cut tools: `tools/recut_module.py SRC DST MODULE` moves a module's routines and resolves the helpers they need (moved along, or shared through the source's header); `tools/move_functions.py` moves named functions; `tools/merge_files.py` merges whole files. Gate after each unit: 0 warnings, `test/check.sh` 7 of 7, clock table 851, names gate.
* 2026-11-04: CONTRIBUTING.md must not contradict History: lineage lives only in the asm repositories' `docs/name_map.csv`, never in source comments. (CONTRIBUTING rewrite vs RULES History row)
* 2026-11-06 leftover recut: C files follow current NHL94SNES folders (`print.c`, `title.c`; `ManualGoalieMenu` in `period.c`). Names follow cleaned 94S (`Reset` is `Start`, `NMI_Fast` is `VBlank`, stub returns are `TitleScreen_ret` / `PlayShootout_ret` / `Reset_Fast_ret`). Six parked locals stay: C `SYM_*_t1`/`_t2` sit at other cartridge addresses than the asm's `Parent.t1`/`.t2`. Missing hooks have no asm label; they sit with their parent. Recut of a helper used before its definition must place the static above its first call. Static state and macros (`bt`, `introbuf24`, `SCOUT_INC`, `SPFpad`) hitchhike with the routines; recut_module.py only moves functions.
* 2026-11-06 flatten: original-routine files live at `game/<module>.c` (one file per 94S `src/routines/<module>/`). Host glue `snes.c`, `ram.c`, `rom.c` flatten with them. Makefile `GAME` / `GAMEH` are `game/*.c` and `game/*.h`. Do not keep an extra `play/` `menus/` `screen/` `sound/` `system/` level.
* 2026-11-06 ROM order: `tools/check_rom_order.py --module M` compares asm globals that have a C function, in name_map address order. C extras (`Parent_x` locals, missing framehooks) hitchhike with the parent and do not fail the order check. Static helpers stay at file prefix (a helper that hitchhikes with the next original can move after its callers: `picboxmap_add` in cards, `penshot_foul`/`abs16`/`NEG` in collide). Unpaired `#define` constants lift to prefix, deduped (`SPFPen` in video; repeated `NEG` in logic). `#define`/`#undef` pairs stay inside the function (`NEXT` in setup).
* 2026-11-06 RAM: `game/ram.h` field names follow `ram94s.asm` equates; `_Static_assert` offsets stay. INDIO names (`DMAQueue`, `JoyPad`, `vcount`) become the 94S labels (`DMAList`, `PadRaw1`, `Vcount`). A `$7Exxxx` equate is WRAM offset `xxxx`; a flag that shares the number (`sf2pwrplay` = `$2000`) is not a RAM name.
