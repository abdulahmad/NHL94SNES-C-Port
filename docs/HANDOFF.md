# Handoff

Where the C-port work stands in this repository. Plan: `alignment-plan/c_ports_plan.md` in the project files.
Project-wide handoff (always current, read it first): `alignment-plan/c_ports_HANDOFF.md` in the project files; how the project is run: `alignment-plan/HARNESS.md`.

Rules: [AGENTS.md](../AGENTS.md), [RULES.md](RULES.md), [VANILLA.md](VANILLA.md).

## Now

Asstab slot indexes named from cleaned 94S (`anull`, `adefo`, `abreakaway` $10, `adumpin` $14) on `cursor/asstab-index-names-7e4c` ([#8](https://github.com/abdulahmad/NHL94SNES-C-Port/pull/8)). Values stay this ROM's numbers. S0–S3 remain on `main`. Names 844/0/6/5. Parked 6 / missing 5 stay. Oracle (plan S1/S2) still parked. Do not merge to `main` without Abdul.

## Done

* Phase 0: credit in the README, AGENTS.md, CLAUDE.md, docs/RULES.md, docs/VANILLA.md, this file.
* Phase 1 (part): the names gate `tools/check_names.py` and its baseline `tools/names_baseline.json`; the vanilla gate `tools/check_vanilla.sh`.

## Results (2026-11-03)

* `test/golden.c` crashed every run on Linux (glibc declares `strtok_r` only for POSIX builds; MinGW always). Fixed with
  `_POSIX_C_SOURCE`. Then `bash test/check.sh ROM`: 7 of 7 identical (Linux, GCC 13).
* Renames: 92 routines renamed to the NHL94SNES labels (`tools/rename_routines.py --module M`, one asm module a
  commit), `test/check.sh` 7 of 7 after each and the clock's routine table unchanged (851). The names gate now treats
  an asm local `Parent.x` as the C `Parent_x`. Status: ok 397 (was 334), other module 299, renamed 6 (parked, see
  docs/RULES.md), missing 4.
* Re-cut: every original routine now sits in its asm module's file (one commit per unit: logic, setup, middle,
  video, replay's helpers, hockey, stats, decompress, main, sound, then snes.c's routines). New files
  `game/system/decompress.c`, `game/system/main.c`; `game/system/snes.c` keeps only the port's glue. The names gate
  now counts one-line functions too: 855 routines, ok 844, other module 0, renamed 6 (parked), missing 5.
  `test/check.sh` 7 of 7, the clock table 851 and 0 warnings after every unit; the vanilla gate passes at the end.

2026-11-04: CONTRIBUTING.md added. Work stays on `claude/project-thread-4gad59`; do not merge to main.

2026-11-06: aligned to current NHL94SNES (leftover recut #387 on asm `main`, plus later cleaned names).
`claude/project-thread-4gad59` was already merged; this work is on `cursor/94s-cport-align-9908`. Do not merge to main.

* Names: 30 routines renamed to current 94S labels (`Start`, `VBlank`, `ManualGoalieMenu`, `puckunflip`,
  `waitx`, `forcefade`, `TitleScreen_ret`, `PlayShootout_ret`, `Reset_Fast_ret`, pause/stats/scout labels).
  Six parked locals unchanged (C `SYM_*_t1`/`_t2` at other addresses than the asm's `Parent.t1`/`.t2`).
* Re-cut: `ManualGoalieMenu` into `period.c` (94S leftover); new `game/system/print.c` and `game/screen/title.c`;
  remaining other_module routines into their asm module files. Names gate **844 ok / 0 other_module / 6 renamed
  / 5 missing**. `test/check.sh` 7 of 7, clock table 851, vanilla PASS.

## Next, in order

1. **Ask INDIO for ninety-four-snes's oracle tools** (snes9x hook, recordings, the routine-by-routine checker),
   and whether he wants Smooth as the default (Abdul's decision for the ports).
2. Otherwise build them (plan S2): a libretro SNES core with a CPU hook, recorded sessions, a lockstep player in
   `test/oracle/`, resyncing at lag frames as 1nput's Genesis player does.
3. Coverage report (plan S3), and the cost model measured against the oracle (plan S4).
4. ~~Renames~~ and ~~re-cut~~ done, including leftover recut alignment to current 94S (6 renames parked: C
   `SYM_*_t1`/`_t2` at other cartridge addresses than the asm locals).
5. ~~**S0 ROM-order checker**~~ (2026-11-06): `tools/check_rom_order.py --module M` vs 94S `src/routines/<module>/`.
   Self-test: print membership-complete, not ROM order. Does not fail the tree until S2.
6. ~~**S1 flatten**~~ (2026-11-06): `game/play|menus|screen|sound|system` → `game/<module>.c`; Makefile `GAME`/`GAMEH` are `game/*.c` / `game/*.h`.
7. ~~**S2 ROM order**~~ (2026-11-06): 20 original-routine files in name_map address order. Parked 6 renamed / 5 missing stay (C `SYM_*_t1`/`_t2` at other addresses; title/intro framehooks have no asm label).
8. ~~**S3 RAM**~~ (2026-11-06): `game/ram.h` field names follow `ram94s.asm` equates; offsets stay. Oracle (plan S1/S2) still parked.
9. **Asstab index names** (2026-11-06, [#8](https://github.com/abdulahmad/NHL94SNES-C-Port/pull/8)): C immediates use `asstab.asm` slot names at this ROM's numbers. No 94G extras. Review PR; do not merge.

## Needs

* The ROM (never committed): the US cartridge, SHA-1 `10dcd21be9c5542e82b0b94473bf5ff569b36cf5` (no copier header)
* The asm repository beside this one: `../NHL94SNES`.
