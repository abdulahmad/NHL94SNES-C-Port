# Handoff

Where the C-port work stands in this repository. Plan: `alignment-plan/c_ports_plan.md` in the project files.
Project-wide handoff (always current, read it first): `alignment-plan/c_ports_HANDOFF.md` in the project files; how the project is run: `alignment-plan/HARNESS.md`.

Rules: [AGENTS.md](../AGENTS.md), [RULES.md](RULES.md), [VANILLA.md](VANILLA.md).

## Done

* Phase 0: credit in the README, AGENTS.md, CLAUDE.md, docs/RULES.md, docs/VANILLA.md, this file.
* Phase 1 (part): the names gate `tools/check_names.py` and its baseline `tools/names_baseline.json`; the vanilla gate `tools/check_vanilla.sh`.

## Results (2026-11-03)

* `test/golden.c` crashed every run on Linux (glibc declares `strtok_r` only for POSIX builds; MinGW always). Fixed with
  `_POSIX_C_SOURCE`. Then `bash test/check.sh ROM`: 7 of 7 identical (Linux, GCC 13).

## Next, in order

1. **Ask INDIO for ninety-four-snes's oracle tools** (snes9x hook, recordings, the routine-by-routine checker),
   and whether he wants Smooth as the default (Abdul's decision for the ports).
2. Otherwise build them (plan S2): a libretro SNES core with a CPU hook, recorded sessions, a lockstep player in
   `test/oracle/`, resyncing at lag frames as 1nput's Genesis player does.
3. Coverage report (plan S3), and the cost model measured against the oracle (plan S4).
4. Renames (99 functions, `python3 tools/check_names.py --list renamed`), then the re-cut to the NHL94SNES folders
   (`--list other_module`), one asm module per unit. The names gate and `test/check.sh` must stay green.

## Needs

* The ROM (never committed): the US cartridge, SHA-1 `10dcd21be9c5542e82b0b94473bf5ff569b36cf5` (no copier header)
* The asm repository beside this one: `../NHL94SNES`.
