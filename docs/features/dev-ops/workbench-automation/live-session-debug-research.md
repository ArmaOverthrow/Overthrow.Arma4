# Live-Session Agent Debugging — Research Findings

**Date:** 2026-08-04 (Reforger 1.7.0.54, Workbench stable branch, Win11 26200)
**Question:** Can an external agent (CLI on the same machine) execute EnforceScript in a running Workbench play session — replacing the "agent writes `.tmp/group-debug.c`, human pastes into the console" round-trip? Secondary: can the agent trigger a script recompile in an open Workbench?
**Verdict:** **Yes.** Two engine capabilities compose into a full solution: a runtime eval API and an official external-control TCP server. Neither requires touching the private debugger protocol.
**Status:** Research only — nothing built. Spike list at the bottom decides the design.

All citations are into the extracted base-game tree at `/mnt/n/Projects/Arma 4/ArmaReforger`.

---

## Building block 1: runtime eval exists — `ScriptModule.CompileScript`

`scripts/Core/generated/System/ScriptModule.c`:

- `:63` `static proto ref ScriptModule CompileScript(ScriptModule parentModule, string text, out string errorText, out int errorLine)` — compile arbitrary EnforceScript source at runtime. Doc: **"Available in developer builds (workbench or diag) and in headless server."**
- `:53` `static proto ref ScriptModule LoadScript(ScriptModule parentModule, string scriptFile)` — same, from a file path (e.g. `$profile:agent/cmd.c`).
- `:45` `proto bool Call(Class inst, string function, bool async, out void returnVal, void param1…param9)` — execute the compiled code.
- `:12-31` — BI's own doc comment is a worked watch-expression evaluator (string-format a function around an expression, compile, call, print).

Parent module: `GetGame().GetScriptModule()` (`scripts/GameLib/generated/Game.c:110`) — the live Game module, so compiled snippets see all Overthrow classes and the running world. Compile errors come back as `errorText`/`errorLine` — a usable feedback loop, no crash.

Our test client `ArmaReforgerSteamDiag.exe` **is** a diag build, so this also works in `tools/launch-game.sh` sessions, not just Workbench play mode.

Dynamic dispatch needs no eval at all for simple cases: `GetScriptModule().Call(inst, "MethodName", …)` + `string.ToType()` + `typename.Spawn()` reach any loaded class/method by name; `typename.GetVariableCount/Name/Type/Value` (`scripts/Core/generated/Types/typename.c:39-42`) is a generic state dumper (no per-type serialisers).

## Building block 2: NetAPI — official external-control TCP server, already enabled here

The Workbench binary ships **NetAPI**: *"Allows workbench to be controlled from external applications and starts TCP server for receiving RPCs."*

- Settings `NetAPI_Enabled` / `NetAPI_Port` (default **5775**, range 3500–9000); companion LSP server on 5776 (editor intelligence only, no code exec).
- **Enabled and listening on this machine now**: `wbSettingsDump.ini` has `enableNetApi=true` / `netApiPort=5775`, and netstat shows the running Workbench listening on 0.0.0.0:5775.
- It is a JSON-RPC transport that dispatches to **script-side "Net Handlers"** registered by a loaded mod (binary string: `"NetApi server doesn't have any Net Handler"`). With no handler it does nothing; with a handler it can do anything script can — including `CompileScript`.
- Built for the Enfusion Blender Tools. Working community proof: **[steffenbk/enfusion-mcp-BK](https://github.com/steffenbk/enfusion-mcp-BK)** connects to 127.0.0.1:5775 and ships handler scripts (`mod/Scripts/WorkbenchGame/EnfusionMCP/`) exposing script-exec/state-query RPCs to external tools. Study this before building our own handler.
- Workbench-only: the game client binary has no NetAPI strings. For diag-client sessions, use the file/HTTP transports below.

Other inbound channels checked and ruled out: no named pipes, no HTTP/WebSocket server, no second-instance command forwarding. RCON is dedicated-server admin-command dispatch (mod-registered `ScrServerCommand` only, not eval). The Script Editor's Remote Console / script debugger (DBG_PORT, 1000 here) genuinely executes script in a live session but over a private checksum-gated binary protocol — not worth speaking directly.

## Transports for a game-side bridge (when not using NetAPI)

A dev-only component polling on `GetGame().GetCallqueue().CallLater(..., ~250ms, repeat: true)` can use:

1. **File drop-box in `$profile:`** — `FileIO.FileExists` / `OpenFile` / `DeleteFile` (delete/copy restricted to `$profile:`/`$logs:`/`$saves:`, `FileIO.c:35-37`). No mtime API exists (`FileAttribute` has no timestamps), so protocol must be existence-based (consume + delete) or seq-number-based. Overthrow precedent: `OVT_OverthrowConfigComponent.c:211-241` already reads/writes `$profile:` JSON at runtime. Agent resolves the profile dir the same way `tools/lib/common.sh` resolves log dirs. Absolute-path FileIO outside `$profile:` is unproven in the non-Workbench client — just use `$profile:`.
2. **Synchronous HTTP to the agent** — `RestContext.GET_now/POST_now` (`scripts/GameLib/generated/online/RestContext.c:18-30`) return the body inline; BI's own Blender bridge shape (`EBT_HTTPRequest.c:28-38`, `http://localhost:8080/`). Overthrow already uses runtime REST (`OVT_NotificationManagerComponent.c:143`). Caveat: synchronous on main thread = frame hitch if the agent is slow; long-poll belongs in an async/`Sleep` loop, not per-frame.
3. **Clipboard** — `System.ImportFromClipboard()` / `ExportToClipboard()` (`System.c:47-49`); agent sets it externally (`clip.exe`), bridge polls for a sentinel prefix.

Output channels: write JSON via `JsonSaveContext.SaveToFile("$profile:agent/out.json")`; or just `Print()` — the agent already parses `console.log` via existing tooling. `Debug.DumpStack(out string)` returns traces into a string. `System.MakeScreenshot()` for a visual channel.

Per-frame hosts if needed: `SCR_GameCoreBase.OnUpdate` (game-side, config-registered, `scripts/Game/GameCore/SCR_GameCoreBase.c:65`); in the World Editor (edit mode), a component's `_WB_AfterWorldUpdate` with `EEntityFrameUpdateSpecs.CALL_ALWAYS`. Workbench plugins have **no** tick — but a hotkey plugin's `Run()` may loop forever on `Sleep()` (BI's `SCR_FPSDiagnosticPlugin` pattern) with full game + Workbench API access; `WorldEditor.SwitchToGameMode()/SwitchToEditMode()` starts/stops play from script.

## The recompile question

- **Hotkey (current build, decoded from the binary's Qt action table; no user remaps):** Script Editor `Build → Validate and Reload Scripts` = **`Shift+F7`** (the action was renamed — no "Compile and Reload Scripts" string remains in the binary; README.md:54 label is stale). Bare `F7` = Validate only; `Ctrl+Shift+F7` = Reload Scripts; World Editor `Shift+R` = Reload Game Scripts, `Ctrl+Shift+R` = Reload Workbench Scripts, `Esc` = Exit Play Mode.
- **External trigger is feasible**: the Workbench exposes three top-level windows with stable UIA ClassNames (`WorldEditorQt`, `ScriptEditor`, `ResourceManager`); PowerShell 5.1 UIAutomation works from WSL and can read the live menu tree. Most robust: UIA `ExpandCollapse` the `Build` menu then `Invoke` the item by name (remap-immune, no keystroke translation); Qt populates submenus lazily so the parent must be expanded first, and whether `Expand()` works unfocused is the main unknown. Fallbacks: focus + SendInput `Shift+F7` (works but steals focus mid-session); `PostMessage` WM_KEYDOWN (focus-free but modifiers likely dropped by Qt's key mapper — 5-min test). Script-visible alternative: `WBModuleDef.ExecuteAction(menuPath)` (`WBModuleDef.c:22`) invokes any menu action by path — zero shipped call sites, path format unknown, Workbench-only.
- **But hot reload probably doesn't live-patch a running session.** Three independent signals say reload = full script-VM teardown: `GameSessionStorage` exists specifically for "data which must survive scripts/addons reloading" (`GameSessionStorage.c:12`) — i.e. statics/ScriptInvokers do NOT; our own run log shows the Game module compiled twice in one process with a fresh `Creating game instance` after (`.tmp/run-tests/console.log:600-643`); and `workbench-workflow` SKILL.md:277 records the empirical "No hot reload — restart play mode". One manual test would settle what `Shift+F7` does mid-play-session (disabled? drops to edit mode? orphans entities?).
- **Conclusion:** the eval bridge makes mid-session recompile mostly moot — new debug code is compiled into the live session via `CompileScript`, no reload needed. External reload-triggering (UIA route) is still worth having for the *edit → replay* loop (kill the "Workbench play-tests stale scripts after WSL edits" failure mode), where teardown is expected anyway.

## Recommended architecture (pending spikes)

**`OVT_AgentBridge`** — dev-only game-side component/core (`#ifdef WORKBENCH` or ENABLE_DIAG-gated; note `CompileScript` self-limits to dev builds anyway, but the bridge must not ship active to retail):

1. Repeating `CallLater` (~250 ms) polls `$profile:agent/cmd.c` (or, Workbench-only variant, registers a NetAPI handler à la enfusion-mcp).
2. On command: read text → `CompileScript(GetGame().GetScriptModule(), src, err, line)` → `Call(null, "Main", false, ret)` → write `{result, printOutput, errorText, errorLine}` to `$profile:agent/out.json` → delete input.
3. Agent side: a small `tools/` wrapper writes the snippet, waits for the output file, prints it. Guards copied from `SCR_AutotestRunner.c:90-125`: `IsPreloadFinished()`, transition-in-progress check, re-check each tick.

Human action per debug iteration: **zero** (after pressing Play once).

## Spike list (each ~30 min, in priority order)

1. **`CompileScript` in a live session** — does it work in Workbench play mode AND diag client; can compiled code reference Overthrow types (`OVT_Global.GetTowns()`); does repeated compiling leak. *This single result decides the design.*
2. `$profile:` file round-trip from the bridge poll loop (existence-poll → read → write → delete).
3. `RestContext.GET_now` latency from main thread (if HTTP transport wanted).
4. Manual: press `Shift+F7` during play mode — enabled? drops to edit? live-patches? (settles the hot-reload question for the record).
5. UIA `Expand+Invoke` on an unfocused Workbench; `ExecuteAction` menu-path discovery. (Lowest priority — only needed for the reload-trigger convenience, not the bridge.)

## Interim workflow win (zero code)

Until a bridge exists: pipe snippets to the Windows clipboard from WSL (`clip.exe < .tmp/group-debug.c`) so the human's step shrinks to focus-console + Ctrl+V + Enter. Mind the Script Console dialect gotchas (see `workbench-stale-scripts` memory).

## Sources

- Community proof of NetAPI driving: https://github.com/steffenbk/enfusion-mcp-BK
- Workbench code-exec vulnerability postmortem (context on NetAPI/remote console): https://gist.github.com/Arkensor/2389176894d847cba3cd9bf0e76bdb3c
- BI feedback T174597 "Workbench remote Console not executing code": https://feedback.bistudio.com/T174597
- BI Wiki: Arma Reforger Script Editor / Workbench Plugin Tutorial
