# Workbench CLI Empirical Findings (Phase 1)

**Date:** 2026-08-01
**Reforger engine version:** 190965 — game version 1.7.0.54 built 2026-06-14 (from fresh run console.log; June logs were 190084)
**Tools build:** Steam appid 1874910, stable branch (ArmaReforgerWorkbenchSteamDiag.exe — only binary present)
**Workbench branch:** Stable (`ArmaReforgerWorkbench` profile root; Experimental profile dir exists but only one Tools install)
**Steam:** RUNNING and logged in at test time (steam.exe PID 15248 + steamservice.exe present). No-Steam case untested.
**Profile root:** `C:\Users\Aaron Static\OneDrive\Documents\My Games\` (OneDrive-redirected)
**Pre-existing processes:** No ArmaReforger*/Workbench processes running before tests.

## Experiment table

| # | Flag/command | Exit code | Wall time | Window/focus | Log location | Notes |
|---|--------------|-----------|-----------|--------------|--------------|-------|
| 1.2 | `-gproj <Overthrow addon.gproj>` (nothing else) | 255 | 56.2s | No persistent GUI (init failed); exited on its own | `ArmaReforgerWorkbench/logs/logs_2026-08-01_21-00-25` | FAILS: `Game addon '58D0FB3206B6F859' not found` then `Cannot initialize game project settings!`. Base game addons dir is NOT auto-discovered from CLI; the GUI project-list dirs are not applied to `-gproj` CLI launches. 47s of the 56s was engine init retry/scan. |
| 1.3 | Detachment check | n/a | n/a | n/a | n/a | **Workbench does NOT detach.** The interop shell blocks for the full process lifetime; `$?` is the process exit code and is meaningful. Wall time == process lifetime in every run. |
| 1.4a | `-gproj ... -addonsDir <game addons> -validate` (no -wbsilent) | 0 (MEANINGLESS — user manually closed GUI) | 265.3s (until manual close) | **Full Workbench GUI opened** and stayed open; showed compile errors in GUI | `logs_2026-08-01_21-02-52` | Validation/compile DID run at startup (errors in console.log) but process does not exit; waits as a normal GUI session. `-validate` without `-wbsilent` is NOT usable for CI. |
| 1.4b | `-gproj ... -addonsDir ... -wbsilent -validate` (broken tree) | 255 | 3.3s | No window observed; exits on its own | `logs_2026-08-01_21-08-29` | Compile ran (Module: Workbench/GameLib OK, Game failed with 5 SCRIPT (E) lines + `Can't compile "Game" script module!`). **This is the CI shape.** |
| 1.4c | `-gproj <base ArmaReforger.gproj> -wbsilent -validate` (no addonsDir) | 255 | 0.39s | none | `logs_2026-08-01_21-09-29` | Fails early: `Addon 'ArmaReforger' dependency '5614BBCCBB55ED1C' can't be added` — core addon not found without -addonsDir. |
| 1.4d | `-gproj <base ArmaReforger.gproj> -addonsDir <game addons> -wbsilent -validate` | **0** | 4.45s | none | `logs_2026-08-01_21-09-45` | PASSING compile: `Module: Game; loaded 5633x files; 10984x classes ...` + `PROFILING : Compiling Game scripts took: 1502.17 ms`. Exit 0 despite 25 `RESOURCES (E)` lines — resource errors do NOT affect exit code. |
| 1.4e | `-wbsilent -validate PC` / `-validate workbench` / `-validate bogus` (Overthrow, broken tree) | 255 / 255 / 255 | 3.8s / 4.6s / 4.9s | none | `logs_..._21-10-12/16/21` | The config argument is EFFECTIVELY IGNORED: identical defines (`ENF_WB,WORKBENCH,PLATFORM_WINDOWS,ENABLE_DIAG,DEBUG_NAVMESH_REBUILD_AREAS,PERSISTENCE_DEBUG,...`), identical 5 errors, even with a bogus name. The addon.gproj `workbench` ScriptConfigurationClass defines are applied automatically in all cases. Plain `-validate` suffices. |
| 1.5a | Canary `Scripts/Game/OVT_CompileCheckCanary.c` (ternary) + `-wbsilent -validate` (default profile) | 255 | 3.8s | none | `logs_..._21-11-23` | Canary errors NOT listed — only pre-existing `OVT_Component.c` errors. New .c files ARE picked up (proved by 1.5b), but the compiler ABORTS AT THE FIRST FILE with parse errors (traversal order). |
| 1.5b | Canary in `Scripts/Game/AAACanary/` (sorts before `Components/`) | 255 | 3.2s | none | `logs_..._21-12-06` | Now ONLY the canary's errors are listed; `OVT_Component.c` never reached. Confirms first-failing-file abort for parse errors. New file compiled with no .meta and no GUI registration needed. |
| 1.7a | `-wbsilent` WITHOUT `-validate` | 255 | 3.4s | none | OverthrowCI profile | Startup compile still runs and exit code still reflects it. `-validate` adds nothing observable on top of `-wbsilent`, but keep it for intent. |
| 1.7b | `-validate -exitAfterInit` (NO -wbsilent) | **0 on a broken tree** | 6.4s | "Arma Reforger Workbench" main window present for whole run (PowerShell MainWindowHandle probe) | OverthrowCI profile | Exits on its own BUT exit code is 0 despite compile failure, and a GUI window shows. **-exitAfterInit is unusable for CI.** |
| 1.7c | Window probe during `-wbsilent -validate` | n/a | n/a | MainWindowHandle=0 for most of run; a handle titled "Arma Reforger Workbench" appears ~last probe before exit | n/a | wbsilent run creates a window HANDLE briefly near the end; no visible window reported by user during these runs. A **"missing addon" popup DID appear** (user-observed) during the run whose deps could not be resolved (semicolon/comma addonsDir failures) even with -wbsilent — the process still self-exited. Focus stealing: needs human confirmation, but no focus loss was reported across ~15 wbsilent runs. |
| 1.8a | `-profile OverthrowCI` + validate | (see 1.8d) | 4.1s | none | `My Games/OverthrowCI/logs/logs_<ts>/` created | Simple profile name works, root = `C:\Users\Aaron Static\OneDrive\Documents\My Games\<Name>`. |
| 1.8b | `-profile Overthrow/ci` (nested) | 0 | 4.4s | none | `My Games/Overthrow/ci/logs/...` created | Nested profile name ACCEPTED. |
| 1.8c | `-profile C:\Temp\OverthrowCIAbs` (absolute) | 0 | 4.3s | none | `C:\Temp\OverthrowCIAbs\logs\...` created | Absolute path ACCEPTED. |
| 1.8d | **FALSE-PASS TRAP:** `-profile OverthrowCI` with only `<game>/addons` as addonsDir | **0 on broken tree** | 4.1s | none | OverthrowCI logs | Fresh profile has no downloaded EPF/EDF → `Addon 'Overthrow' dependency '5D6EBC81EB1842EF' can't be added` → Workbench SILENTLY DROPS Overthrow and validates the BASE GAME (Module: Game 5633 files, no Overthrow defines) → exit 0. **A pinned profile MUST be paired with an addonsDir that resolves EPF+EDF, and the parser should verify Overthrow was actually loaded/compiled.** |
| 1.8e | `-addonsDir` forms | n/a | n/a | n/a | n/a | Repeated `-addonsDir` = LAST WINS. Semicolon-separated = treated as one bogus path. **Comma-separated works** (`dirA,dirB`). Scan is exactly ONE level deep: `<dir>/<sub>/*.gproj`. The `-gproj` project's own dir is always auto-added as an addon dir. |
| 1.8f | Source EPF/EDF as deps (`N:\Projects\Arma 4\EnfusionPersistenceFramework,...EnfusionDatabaseFramework,<game>/addons`) | 255 | 3.7s | none | OverthrowCI logs | All 5 addons load (EPF/EDF from src/), but **EPF source itself fails to compile** on 1.7.0.54: 21x `Expected attribute call` (`[EDF_DbName.Automatic()]` etc.), then `Too many errors`, then `Can't compile "Game" script module!`. Packed workshop EPF/EDF compile fine. Use packed deps for CI. |
| 1.8g | **Canonical deps:** `-addonsDir "C:\...\My Games\ArmaReforgerWorkbench\addons,N:\Program Files (x86)\...\Arma Reforger\addons"` + `-profile OverthrowCI` | 255 (broken tree) | 3.4s | none | OverthrowCI logs | Loaded addons: core, ArmaReforger, EDF (packed), EPF (packed), Overthrow. Overthrow scripts compiled; canary errors reported. **This is the canonical CI shape.** |
| 1.9 | Timings | n/a | cold-first-ever 56s (one-off project resource DB scan, rdb written to PROJECT root); fresh-profile cold 3.4s; warm 3.28s / 3.31s / 3.41s | none | n/a | Fresh profile adds ~nothing because the resource DB cache (`resourceDatabase.rdb`) lives in the project root, not the profile. Proposed default timeout: 120s. |
| 1.10a | bash `timeout 15` on a GUI-mode run | 124 (timeout) | 15.0s | GUI window stayed | n/a | **`timeout` kills ONLY the WSL interop stub — the Windows process SURVIVED** (tasklist still showed it). Exit 124 means "unknown outcome + orphan to clean". |
| 1.10b | `taskkill.exe /F /T /PID <pid>` | 0 | instant | window closed | n/a | Works from WSL. PID capture recipe: `tasklist.exe /FI "IMAGENAME eq ArmaReforgerWorkbenchSteamDiag.exe" /FO CSV /NH`. |
| 1.12 | Concurrency: own GUI instance open (default profile) + `-wbsilent -validate -profile OverthrowCI` | 255 | 3.5s | GUI unaffected | OverthrowCI logs | No refusal, no lock contention observed; validate produced correct errors while GUI ran. (Both processes write `<project>/resourceDatabase.rdb` — no failure observed, but a theoretical write race exists.) |
| 1.13 | Truncation | 255 | 3.9s | none | see 1.5b/1.8f | A file with 300 bad statements reports only the FIRST broken statement (~4 parse errors) — parse errors cannot trip the cap. The cap WAS observed with cross-file attribute errors (1.8f): **21 per-error lines, then `@"<file>,<line>": Too many errors`, then `Can't compile "Game" script module!`** So the sentinel exists, cap ≈ 21 errors, and non-parse errors ARE collected across multiple files. |
| 1.14a | Client, historical shape verbatim (`-addonsDir "N:\Projects\Arma 4"`, cwd = repo) | 0 (despite fatal error) | 4.5s | brief window | `My Games/OverthrowCI/logs/logs_..._21-21-33` | FAILED: `Addon 'ArmaReforger' dependency '5614BBCCBB55ED1C' can't be added` — the client finds `core` via `./addons` RELATIVE TO CWD, so it must be launched with cwd = game install dir (Steam normally does this). Exit 0 anyway. |
| 1.14b | Client, cwd=game dir, `-addonsDir "N:\Projects\Arma 4,C:\...\ArmaReforgerWorkbench\addons" -addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow -profile OverthrowCI -noFocus -noThrow -window -logLevel debug` | 0 (despite compile failure) | 4.1s | brief window | `logs_..._21-22-07` | Loaded addons: core, ArmaReforger, EDF+EPF (packed), Overthrow (source). Overthrow scripts compiled → same OVT_Component.c errors → client SHUT DOWN CLEANLY on its own. **Client `-profile OverthrowCI` shares the SAME `My Games/OverthrowCI` root as Workbench.** Comma-separated addonsDir works for the client too. **CLIENT EXIT CODES ARE MEANINGLESS (always 0 observed, even on fatal errors).** |
| 1.14c | Client vanilla `-autotest SCR_TEST_Example1TestSuite` (BI default flags, -profile OverthrowCI) | 0 | 7.1s | brief window | `logs_..._21-23-50` (has crash.log) | `-autotest` IS accepted and the framework engages, but value rejected: `Invalid -autotest parameter value` → crash.log written, clean self-exit. Cause: the example suites live in `scripts/Game/Tests/TestSuites/Example/` guarded by `#ifdef WORKBENCH` — they DO NOT EXIST in the retail client. Real class name is `SCR_TEST_Example1SubjectSuite` (file name `SCR_TEST_Example1TestSuite.c` is a decoy). |
| 1.14d | Client vanilla `-autotest "{6AB9C8EEE9A651B5}"` (shipped empty SCR_AutotestGroup `Configs/Tests/ExampleTestGroup.conf`) | 0 | 7.0s | brief window | `logs_..._21-24-56` | **SUCCESS — first autotest ever run on this machine.** Harness ran (0 suites, group is empty), wrote `junit.xml`, `autotest.log`, `autotest_failed.log` into the run's log dir, and the client REQUESTED CLOSE AND EXITED ON ITS OWN in 7.0s. junit.xml: `<testsuites time="1.844791" timestamp="2026-08-01T11:25:00.371Z">\n</testsuites>`. Accepted forms per `SCR_AutotestRunner.c`: `{GUID}` of an SCR_AutotestGroup config, a class inheriting `SCR_AutotestSuiteBase`, or one inheriting `SCR_AutotestCaseBase`. JUnit path constant: `$logs:/junit.xml`. |

## Verbatim log samples

All samples from `console.log` (also mirrored in `script.log`; `error.log` carries the (E)/(W) subset). Timestamp prefix `HH:MM:SS.mmm`, then padded channel, then `(E)`/`(W)`/`(D)` severity.

Per-error line (confirmed under `-wbsilent -validate`, comma INSIDE quotes, addon-relative path, NO addon identifier):

    21:12:09.053    SCRIPT    (E): @"Scripts/Game/AAACanary/OVT_CompileCheckCanary2.c,5": Broken expression (missing ';'?)

An identical (W) duplicate of an (E) line can appear (same file/line/message, warning severity) — parser must dedupe.

Failure marker + blank line + unprefixed restatement of the FIRST error only (confirmed):

    21:12:09.053    SCRIPT    (E): Can't compile "Game" script module!

    Scripts/Game/AAACanary/OVT_CompileCheckCanary2.c(5): Broken expression (missing ';'?)
    21:12:09.110    SCRIPT    (W): Failed to load

Truncation sentinel (confirmed, after 21 listed errors; sentinel is itself formatted as an error at a file/line):

    21:15:39.672    SCRIPT    (E): @"Scripts/Game/Entities/EPF_TimeAndWeatherSaveData.c,7": Too many errors
    21:15:39.672    SCRIPT    (E): Can't compile "Game" script module!

Success evidence (confirmed on base-game validate; counts differ from the assumed 5630/10894):

    21:09:48.939    SCRIPT       : Module: Game; loaded 5633x files; 10984x classes; used 31792K of static memory; defines: "..."; CRC32: a42150c3
    21:09:48.970   PROFILING    : Compiling Game scripts took: 1502.166500 ms

Confirmed: NO success string, NO error-count summary. Exit code + presence/absence of `Can't compile "<module>" script module!` are the machine-readable signals. Modules compile in order Workbench, GameLib, WorkbenchGame, Game; only "Game" matters for the mod.

## Differs from assumptions

1. **Bare `-gproj` launch does not work**: base game addons are NOT auto-discovered; you MUST pass `-addonsDir`. The GUI's addon dirs come from the Workbench project list (`profile/.projectList_*.conf`), which CLI launches do not apply.
2. **`-validate` without `-wbsilent` opens the full GUI and never exits** — the wiki's "returns -1/0" only materializes with `-wbsilent`. Exit -1 does surface as 255 through WSL interop (confirmed literal).
3. **The `-validate [scriptConfigName]` argument is effectively ignored** — `PC`, `workbench`, and `bogus` all behave identically; the addon.gproj `workbench` script config defines (WORKBENCH, PERSISTENCE_DEBUG, DEBUG_NAVMESH_REBUILD_AREAS) are applied automatically.
4. **Workbench does NOT detach** — exit codes are trustworthy (0 pass / 255 fail) with `-wbsilent`.
5. **`-exitAfterInit` returns 0 on a broken tree** and shows a GUI window — unusable.
6. **FALSE-PASS TRAP:** if any dependency (EPF/EDF) can't be resolved, Workbench silently drops the Overthrow addon and validates the BASE GAME, returning 0 on a broken tree (observed with a fresh `-profile` whose addons dir is empty). A missing-addon popup may appear even with `-wbsilent` (user-observed; process still self-exits). CI must verify Overthrow was loaded (grep `Loaded addons` for `Overthrow.Arma4/addon.gproj`) and/or that `Module: Game` file count is ~5633+Overthrow's ~700, not exactly base-game's.
7. **Compile-error listing aborts at the FIRST file containing parse errors** (only that file's ~4 cascade errors are listed). Non-parse errors (e.g. attribute errors) ARE collected across files, capped at ~21 lines before `Too many errors`.
8. **The tree at HEAD of `vanilla-persistence` does not compile**: `Scripts/Game/Components/OVT_Component.c:15` `static T Find<T>(IEntity entity)` — generic-method syntax rejected by 1.7.0.54 (engine 190965). All passing-compile measurements had to use the base game project. (June GUI logs were engine 190084 — the syntax may have compiled on an older/Experimental build.)
9. **EPF/EDF SOURCE repos (N:\Projects\Arma 4\...) do not compile against 1.7.0.54** (21x `Expected attribute call` inside EPF SaveData files) — the packed workshop copies under `My Games/ArmaReforgerWorkbench/addons` compile fine and must be used as deps. Their gprojs also sit at `<repo>/src/*.gproj` — addonsDir scanning is exactly one level deep, so the repo ROOT works as an addonsDir entry but `N:\Projects\Arma 4` alone does not reach them.
10. **`-addonsDir` accepts ONE value; repeated flags = last wins; separator is COMMA** (semicolon is treated as part of the path).
11. **bash `timeout` does NOT kill the Windows process** — only the interop stub. Exit 124 = outcome unknown + orphan needing `taskkill.exe /F /T /PID`.
12. **Game client exit code is always 0** (observed even on fatal addon-resolution and compile failures) — client outcomes must be read from logs/artifacts, never `$?`.
13. **Client requires cwd = game install dir** (finds `core` via `./addons`), unlike historical assumption that the flags alone suffice.
14. Base-game Module: Game counts are `5633x files; 10984x classes` on this build, not the assumed `5630x/10894x` — parsers must not hardcode counts.
15. `-autotest` value `SCR_TEST_Example1TestSuite` is invalid in the retail client (example suites are `#ifdef WORKBENCH`); the `{GUID}` group form works and produced junit.xml.
16. First-ever Workbench CLI run on a machine pays a one-off ~50s project resource-DB scan (writes `<project>/resourceDatabase.rdb`, gitignored); after that even brand-new profiles validate in ~3.4s.

## Proposed defaults

**Canonical silent compile check (from WSL bash):**

    WB="/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe"
    timeout 120 "$WB" \
      -gproj 'N:\Projects\Arma 4\Overthrow.Arma4\addon.gproj' \
      -addonsDir 'C:\Users\Aaron Static\OneDrive\Documents\My Games\ArmaReforgerWorkbench\addons,N:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons' \
      -wbsilent -validate -profile OverthrowCI
    # $? == 0 pass, 255 compile failure, 124 timeout (orphan cleanup needed)

- Parse `My Games/OverthrowCI/logs/<newest logs_*>/console.log`.
- Guard against the false-pass trap: require `Overthrow.Arma4/addon.gproj` in the `Loaded addons:` block, else treat as infrastructure failure regardless of exit 0.
  **⚠️ Correction (Phase 3, verified live):** a whole-log grep for the gproj path is NOT sufficient — the path appears ~3x even in dropped-addon runs (CLI-params echo + registration lines). The proof must be scoped to the `Loaded addons:` block itself (the consecutive `gproj:` lines following that header); in a good run it appears exactly once there. `tools/compile-check.sh` implements the block-scoped check; reproducing trap 1.8d against the naive whole-log grep produced a real false pass before the fix.
- Error regex: `^\S+\s+SCRIPT\s+\(E\): @"(?P<file>[^,]+),(?P<line>\d+)": (?P<msg>.*)$`; failure marker `Can't compile "Game" script module!`; sentinel message `Too many errors`; dedupe (W) duplicates.
- Script config name: none needed (argument ignored; plain `-validate`).
- Default timeout: 120 s (warm ~3.4 s, first-ever-scan ~60 s).
- Process control: on timeout, `tasklist.exe /FI "IMAGENAME eq ArmaReforgerWorkbenchSteamDiag.exe" /FO CSV /NH` → `taskkill.exe /F /T /PID <pid>`; verify with a follow-up tasklist. Never kill by image name blindly (may hit the user's GUI session).
- Concurrency with an open GUI Workbench: safe in observation (validate on `OverthrowCI` while GUI ran on default profile); only shared artifact is `<project>/resourceDatabase.rdb` (theoretical write race).
- Game client CI shape (cwd MUST be the game dir): `./ArmaReforgerSteamDiag.exe -gproj <base ArmaReforger.gproj> -addonsDir 'N:\Projects\Arma 4,C:\...\My Games\ArmaReforgerWorkbench\addons' -addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow -profile OverthrowCI -noFocus -noThrow -window -logLevel debug [-autotest <{GUID}|SuiteClass>]`; read results from junit.xml/logs, ignore exit code.

## Open items / untested

- **Passing-compile exit 0 on Overthrow itself**: blocked by pre-existing `OVT_Component.c:15` generic-method syntax error at HEAD (hard rule: no source edits during this phase). Semantics proven via base game (exit 0). Re-run the canonical command after that file is fixed to close this out. This also blocks measuring: multi-file error collection on an otherwise-clean tree and the semantic-error truncation cap.
- **No-Steam dependency**: Steam was running (logged in) throughout; untested with Steam closed (will not close the user's Steam). Hint: both Diag exes logged `[API loaded no]` and ran fine, suggesting weak coupling.
- **Focus stealing under `-wbsilent`**: a window HANDLE titled "Arma Reforger Workbench" appears briefly near the end of a wbsilent run (PowerShell probe); no visible window/focus loss reported by the user across ~15 runs. Needs one deliberate human confirmation.
- Experimental Workbench branch: not installed as a separate binary; untested.
