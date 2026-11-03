# Alignment rules

What decides each kind of name and place in the C ports. Workers and the splitter read this first.

## Authority

| Category | Authority |
|---|---|
| Function name of an original routine | The current label in [NHL94SNES](https://github.com/abdulahmad/NHL94SNES). Old names reach it through that repository's `docs/name_map.csv` (`tools/check_names.py` follows the chain) |
| Asm local (`Parent.x`) | The C function `Parent_x`, `static` in the parent's file where nothing else calls it |
| Helper (a piece split off a routine, or the C's own) | `<Routine>_<what>`, `static` where possible; listed as a helper in docs/ROUTINES.md |
| C file | One file per asm module, named after it (`src/routines/collide/` is `collide.c`), once re-cut (phase 5) |
| Order inside a file | ROM order, as the asm |
| RAM names (`ram.h`) | The asm RAM equates (`src/ram94s.asm`) |
| First comment line of a routine | its asm file and label (`collide94.asm:checkcoll.`) |
| Other games' names | never in comments; lineage lives in the asm repositories' `docs/name_map.csv` |

## Status (tools/check_names.py, 2026-11-03, after the renames)

| ok | other_module | renamed | missing |
|---|---|---|---|
| 397 | 299 | 6 | 4 |

## Learned

(One line per correction, newest last.)
* 2026-11-03: an asm local `Parent.x` is the C function `Parent_x`; the names gate counts them as the same name.
* 2026-11-03: renaming a routine renames its locals too (`old_x` to `new_x`, C functions and `SYM_`s): the clock's table pairs each C function with its `SYM_`, and a local left behind drops out of it (`tools/rename_routines.py` does both; check the table's size, `awk -f tools/gen_fnsyms.awk ...`, stays the same).
* 2026-11-03: parked (UNSURE): a rename whose new name is already another C name or `SYM_` (`randomd0sd0` to `randomd0s`, a C helper's name; the asm's anonymous locals `.t1`/`.t2` where the C has `SYM_..._t1` at another address: setoptions_t1xxxxxxxxxxx, setoptions_t2xxx, IntroSequence_t1xxxxxxxxxxxx, DrawSpritePieces_cacheat, DrawSpritePieces_cacheframe). They need the symbol file regenerated from the current asm, or names in the asm.
