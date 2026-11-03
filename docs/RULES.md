# Alignment rules

What decides each kind of name and place in the C ports. Workers and the splitter read this first.

## Authority

| Category | Authority |
|---|---|
| Function name of an original routine | The current label in [NHL94SNES](https://github.com/abdulahmad/NHL94SNES). Old names reach it through that repository's `docs/name_map.csv` (`tools/check_names.py` follows the chain) |
| Asm local (`Parent.x`) | The C function `Parent_x`, `static` in the parent's file where nothing else calls it |
| Helper (a piece split off a routine, or the C's own) | `<Routine>_<what>`, `static` where possible; listed as a helper in docs/ROUTINES.md |
| C file | One file per asm module, named after it (`src/routines/collide/` is `collide.c`). `game/system/snes.c` holds only the port's glue (no asm module) |
| Order inside a file | ROM order, as the asm |
| RAM names (`ram.h`) | The asm RAM equates (`src/ram94s.asm`) |
| First comment line of a routine | its asm file and label (`collide94.asm:checkcoll.`) |
| Other games' names | never in comments; lineage lives in the asm repositories' `docs/name_map.csv` |

## Status (tools/check_names.py, 2026-11-03, after the re-cut)

| ok | other_module | renamed | missing |
|---|---|---|---|
| 844 | 0 | 6 | 5 |

855 routines: the gate counts one-line functions since the re-cut (it skipped 149 before). Missing: the title screen's
and the intro's hooks (`TitleScreen_framehook`, `_irq1`-`_irq3`, `IntroSequence_framehook`), which have no label in the asm.

## Learned

(One line per correction, newest last.)
* 2026-11-03: an asm local `Parent.x` is the C function `Parent_x`; the names gate counts them as the same name.
* 2026-11-03: renaming a routine renames its locals too (`old_x` to `new_x`, C functions and `SYM_`s): the clock's table pairs each C function with its `SYM_`, and a local left behind drops out of it (`tools/rename_routines.py` does both; check the table's size, `awk -f tools/gen_fnsyms.awk ...`, stays the same).
* 2026-11-03: parked (UNSURE): a rename whose new name is already another C name or `SYM_` (`randomd0sd0` to `randomd0s`, a C helper's name; the asm's anonymous locals `.t1`/`.t2` where the C has `SYM_..._t1` at another address: setoptions_t1xxxxxxxxxxx, setoptions_t2xxx, IntroSequence_t1xxxxxxxxxxxx, DrawSpritePieces_cacheat, DrawSpritePieces_cacheframe). They need the symbol file regenerated from the current asm, or names in the asm.
* 2026-11-03: the names gate must count one-line functions (body on the head's line): it skipped 149 routines, 33 of them in the wrong file and 17 under old names.
* 2026-11-03: `randomd0sd0` to `randomd0s` is no longer parked: renamed together with `randomd0s` to `randomas` (placeholders). `GetFrameTiles_drawat` is parked with the other `.t1`/`.t2` locals.
* 2026-11-03: re-cut tools: `tools/recut_module.py SRC DST MODULE` moves a module's routines and resolves the helpers they need (moved along, or shared through the source's header); `tools/move_functions.py` moves named functions; `tools/merge_files.py` merges whole files. Gate after each unit: 0 warnings, `test/check.sh` 7 of 7, clock table 851, names gate.
