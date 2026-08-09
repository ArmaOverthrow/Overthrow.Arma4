# Workbench Automation — Requirements

**Epic:** dev-ops
**Created:** 2026-08-01

## Overview

Establishes the process boundary the whole epic stands on: the ability to launch and drive the Arma Reforger Workbench and game client from a WSL shell, unattended, and interpret the result programmatically. Its headline deliverable is a **compile check** — a single command that compiles all of Overthrow's EnforceScript and returns a meaningful exit code plus parsed errors, with no human pressing Build and no GUI interaction.

This is the highest-leverage feature in the epic. Today every compile error costs a full human round-trip; this feature removes that cost entirely and unblocks features #2, #4 and #5.

## Requirements

- **Verify the Workbench CLI empirically before designing around it.** The flags `-gproj`, `-wbmodule`, `-exitAfterInit`, `-wbsilent`, `-plugin`, `-loadAddons`, `-scriptAuthorizeAll` were read from the binary's strings but **none has been executed**. First task is to establish what each actually does, what exit code it returns, and where compile errors surface. Design follows evidence, not the other way round.
- **A single command compiles the project and reports success/failure**, runnable from `/mnt/n/Projects/Arma 4/Overthrow.Arma4` in a WSL shell.
- **Non-zero exit on compile failure, zero on success.** If the Workbench does not provide a usable exit code, derive one from the log — but the wrapper's contract to its callers must be a real exit code.
- **Compile errors are parsed into a structured form** — file, line, message — not left as a wall of log text. They must be readable by both a human and an agent.
- **Path translation between WSL and Windows is solved once, here.** `/mnt/n/...` ↔ `N:\...` conversion is owned by this feature and reused by every other; it must not be reimplemented per-feature.
- **Launching must be non-disruptive.** It must not steal focus or leave GUI windows open on a machine someone is working on. `-wbsilent` / `-noFocus` behaviour needs confirming as part of verification.
- **The game client can also be launched** with an arbitrary argument set (needed by feature #2 for `-autotest`), and the run's artifacts and log directory located afterwards.
- **Runs must terminate.** A hung Workbench must time out and be reported as a failure, not block indefinitely.
- **Document the real behaviour**, including anything that turned out to differ from the strings-derived assumptions — this becomes the reference for the rest of the epic.

## Definition of Done — documentation

This feature makes "no automated builds" false. Its completion **must** update:
- `CLAUDE.md` — the "No automated builds" constraint and the Workbench section
- `docs/technical-design.md` §2 ("What We Don't Have") and §10 (Testing Strategy)
- `docs/mission-statement.md` — "Play-testing as the quality gate"
- The `workbench-workflow` skill

## Dependencies

- Arma Reforger Tools (Workbench) installed at `N:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe`
- Arma Reforger 1.7.0+ at `N:\Program Files (x86)\Steam\steamapps\common\Arma Reforger`
- WSL ↔ Windows interop (confirmed available: `binfmt_misc/WSLInterop`)
- Overthrow's `addon.gproj` and its declared dependencies (`58D0FB3206B6F859` base data, `5D6EBC81EB1842EF` EPF)
- **No dependency on any other feature in this epic** — this is the foundation and must be built first, in isolation.

## Out of Scope

- Running tests — that is feature #2. This feature only needs to *launch* the game client with arbitrary arguments.
- CI orchestration, GitHub Actions, runner setup — feature #4.
- Workshop packing/publishing — feature #5, though it will reuse this feature's launcher.
- Any change to gameplay code.
- Solving the lack of a debugger. `Print()` remains the debugging tool; this feature addresses compilation, not runtime introspection.
