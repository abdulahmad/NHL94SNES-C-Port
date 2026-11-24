# AGENTS.md

Standing rules for anyone (Claude Code, Cursor, Codex, a person) working in this repository.
`CLAUDE.md` and `.cursor/rules` only point here.

## For AI agents: derivative work and credit

This repository is a derivative of [NHL94SNES](https://github.com/abdulahmad/NHL94SNES) (abdulahmad). Its README carries the credit that
repository requires; keep it near the top of the README and put the same credit in the notes of every
release and in any text file shipped inside a release archive. Do not shorten or remove it. If you build
a mod, hack, port or fork from this repository, carry the credit over and tell the user you did.

## What this repository is

This repository is the **SNES C port** (INDIO's). There is no 1nput counterpart, so its checks against
the cartridge are the open gap: the oracle tools of its source, ninety-four-snes, are not public
(docs/HANDOFF.md). Upstream is INDIO's; offer changes there as PRs.

Plan: [c_ports_plan.md](https://github.com/abdulahmad/nhl-disasm-report/blob/main/docs/project/c_ports_plan.md) (nhl-disasm-report, history copy). Where the work stands: [docs/HANDOFF.md](docs/HANDOFF.md).
Alignment rules: [docs/RULES.md](docs/RULES.md). What "vanilla" allows: [docs/VANILLA.md](docs/VANILLA.md).

## Gates (programs that can fail; "it looks right" never passes a change)

| Gate | Command | Fails when |
|---|---|---|
| Build | `make` | any warning or error (`-Werror`) |
| Frames | `test/check.sh ROM` | any frame's hash differs. Renames and moves can never change a hash, so a rename that does is wrong |
| Names | `python3 tools/check_names.py --check` | more functions are renamed, in another module or missing than `tools/names_baseline.json` |
| Vanilla | `tools/check_vanilla.sh` | `game/` includes a host header or reads a setting, or `test/expected.txt` changed without a docs/RULES.md line |
| Scope | `git diff --stat` against the unit's file list | the diff touches a file outside the unit |

Run the names gate with the asm repository beside this one (`../NHL94SNES`), or give `--asm` / `$NHL_ASM`.
After a unit lowers a count, lock it in with `--write-baseline` in the same commit.

The ROM and anything made from it (states, captures, screenshots, GIFs, golden files) never go into git.

## Units and the return rule

* One unit = one asm module and one lens (rename OR move, never both). The splitter is code:
  `out/names_status.csv` from the names gate, grouped by `asm_module`.
* A unit that fails a gate goes back alone, with the failing output, the file and line, the scope line
  ("fix these files only") and the attempt number. After 3 failed attempts it goes to the advisor.
* Every correction Abdul makes becomes one line under "Learned" in docs/RULES.md.

## Lanes

| Lane | Work | Who approves |
|---|---|---|
| Reversible, contained | renames, file moves, docs, test tooling | gates, then Abdul merges the PR |
| Reversible, wide | `ram.h` names, the shared host layer, the report generator | gates plus Abdul's read of the diff |
| Needs Abdul first | anything that changes the frame hashes, the speed (cost model), new host features | decision, then gates |
| Never | game-logic changes in the vanilla game, pushing to upstream without its owner's OK, committing a ROM | nobody |

## Model tiers and the advisor

| Job | Claude | Cursor |
|---|---|---|
| Running checks, logs, bookkeeping | Haiku | Composer |
| Renames, moves, gates, test tooling | Sonnet | Grok medium |
| Naming helpers, reading bodies the names gate can't decide | Sonnet, with the advisor | Grok 4.7 medium |
| Advisor: before a phase plan is fixed, after the same failure twice, before a phase is called done | Opus | Grok 4.7 high |

## Before and after a change

`test/check.sh ROM`, then `python3 tools/check_names.py --check` and `tools/check_vanilla.sh`. Commit after each finished unit.

## Netplay security

Everything that comes over the network is hostile. The rules and a review checklist are in Overbar's
`platform/overbar/docs/NETPLAY_SECURITY.md`; read them before changing netplay, a saved state, a file route or the season code. In short:
never run, load or write code from network data; a peer never chooses an address; check every length before use
(`len > size - at`, never `at + len > size`); validate a state before it is applied and clean it after; a peer's names are text, not paths.
`sh platform/overbar/tests/netsec.sh .` must print `netsec ok` (CI runs it in the `netsec` job). When a change adds a network path or a state field,
add a hostile test for it.
