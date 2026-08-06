# MP Testing — Empirical Findings

**Recorded:** 2026-08-06
**Build:** Arma Reforger 1.7.0.54 / engine 190965; **Arma Reforger Server** Steam app 1874900 (installed 2026-08-06)
**Method:** six launches driven from WSL through `tools/lib/common.sh`; every claim below is from a `console.log` line, not from documentation.

> Shape follows `workbench-automation/findings.md` and `test-coverage/findings.md`: what was executed, what was observed, what differs from assumption, and what is still unproven.

---

## 1. Experiment table

| # | Binary | Invocation | Result |
|---|---|---|---|
| 1 | `ArmaReforgerSteamDiag.exe` (client) | `-config <json>` with `mods:[base-game GUID, Overthrow]` | **Failed.** `BACKEND (E): Addon 58D0FB3206B6F859 - Addon was not found on workshop.` Config itself validated (`JSON is Valid`). |
| 2 | client | `-server <scenario>` `-addonsDir` `-addons Overthrow` | **Worked**, but is a *listen* server: created a local player (`Creating player: PlayerId=1, Name=Aaron Static`) and rendered a window. |
| 3 | `ArmaReforgerServerDiag.exe` | `-config <json>` with `mods:[Overthrow]` `-addonsDir <repo parent>` | **Worked.** Headless, registered, RCON up. Working tree mounted; EPF+EDF also mounted (see §3). |
| 4 | server | `-config <json>` with `mods:[]` **+** `-addons Overthrow` | **Fatal.** `DEFAULT (F): -config cannot be used together with addons!` |
| 5 | server | `-server <scenario>` `-addonsDir` `-addons Overthrow` | **Worked, and is the best local-dev shape.** Headless, no local player, working tree only, offline, listening in ~5 s. |
| 6 | server | experiment 5 **+** `-bindPort 2101` | Flag accepted and **ignored**: `RPL listen address not specified. Using default fallback.`, bound 2001. |

---

## 2. Ground truth established

- **The dedicated server is a separate Steam app** (1874900). The game client install contains the DS *code path* — experiment 1 reached `Starting dedicated server using command line args.` — but no `ArmaReforgerServer.exe`. The engine exposes three distinct startup verbs, all present in both binaries: `Starting multiplayer client…` (`-client`), `Starting local multiplayer server…` (`-server`), `Starting dedicated server…` (`-config`).
- **`58D0FB3206B6F859` is the BASE GAME `ArmaReforger.gproj`, not EPF.** `addon.gproj`'s `Dependencies { "58D0FB3206B6F859" }` is a dependency on the base game. This is worth stating explicitly because `OVT_AutotestFramework.c`'s `ADDONS_OVT = "58D0FB3206B6F859,59B657D731E2A11D"` reads like "EPF + Overthrow" and is not.
- **The working tree wins over the published build in both modes.** `Loaded addons:` names `N:/Projects/Arma 4/Overthrow.Arma4/addon.gproj` even in experiment 3, where the published Overthrow 1.3.34 had just been downloaded to `<profile>/addons/Overthrow_59B657D731E2A11D`. The downloaded copy is present on disk and **not** mounted.
- **`-addonsDir` takes the repo's PARENT**, not the repo. The engine scans it for `addon.gproj` files; on this machine that exposes seven sibling projects (`Overthrow.Dev`, `Overthrow.Arma4.RHS`, `Overthrow.GulfcoastIslands`, …) as candidates. Only selected addons load.
- **A mission header (`.conf`) is required, not a world.** `-server '{GUID}Worlds/...ent'` is not the contract; the two Overthrow headers are `{6B0E7A50D1E2F3A4}Missions/25_OVT_TestWorld.conf` and `{3DAD390C31623F04}Missions/24_OVT_Eden.conf`.
- **The DS config schema ships on disk**: `<server install>/addons/core/data/backend/DSConfigSchema.json`. Top-level `required` is `["game"]` only. It validates at load (`JSON Schema Validation: JSON is Valid`) — a malformed config fails fast and loudly, which makes it safe to generate.
- **`supportedPlatforms` is declared in the schema but skipped by the script layer**: `BACKEND (W): !!! JsonApi Array name="supportedPlatforms" found in JSON - which missing adequate script declaration, items will be skipped!`. Harmless; kept in the generated config for schema conformity.
- **Timings** (test world, warm): local mode ~5 s launch→listening; dedicated mode ~38 s launch→listening (workshop resolve + dependency verify), plus ~2 s more to backend registration.
- **Ports observed:** RPL 2001 (config `bindPort`), A2S 17777, RCON 19999 (`[RCON] Init, Ip address=127.0.0.1 and Port=19999`).

---

## 3. Differs from assumption

- **Assumed the client exe could serve as the dedicated server.** It can host, but `-server` on the *client* binary produces a listen server with a local player and a window (experiment 2), while the same flag on the *server* binary is headless with no player (experiment 5). The binary matters, not just the flag.
- **Assumed `-addonsDir` would let a DS config load a purely local addon.** It does not: `-config` resolves `mods[]` through the workshop API regardless of what is available locally, and `-config` + `-addons` is a fatal error. Local-only loading is possible **only** via the `-server` route.
- **Assumed the port would be controllable in every mode.** It is a config-only setting; the `-server` route has no bind-port flag (experiment 6). `tools/launch-server.sh` rejects `--port` in local mode rather than report a port it is not listening on.
- **Dedicated mode is not a superset of local mode.** Resolving Overthrow through the workshop pulls the *published* build's dependency manifest, which still lists **EPF (`5D6EBC81EB1842EF`) and EDF (`5D6EA74A94173EDF`)**, and both get mounted:
  ```
  Loaded addons:
    './addons/core/core.gproj'
    './addons/data/ArmaReforger.gproj'
    'N:/Projects/Arma 4/Overthrow.Arma4/addon.gproj'
    '<profile>/addons/EnfusionDatabaseFramework_5D6EA74A94173EDF/…'
    '<profile>/addons/EnfusionPersistenceFramework_5D6EBC81EB1842EF/…'
  ```
  This branch no longer loads EPF at runtime at all (local mode's addon list is three entries). EPF defines `modded class` overrides, so dedicated mode is testing a **different composition** than local mode. Which one is "right" depends on the question: dedicated mode is closer to what the *published* server does today; local mode is exactly what is in the checkout.
- **`fastValidation` differs by mode**: `true` in dedicated (set in the generated config), `false` in local. Relevant if a checksum-mismatch behaviour is ever investigated.

---

## 4. Confirmed by observation, useful later

- The Overthrow boot sequence is fully visible in the server log and is a ready-made readiness/health oracle:
  ```
  [Overthrow] Save scan complete, save present: false
  [Overthrow] Persistence manager initialized with vanilla system
  [Overthrow] Initializing Overthrow / Players / Towns / Economy / Occupying Faction /
              Resistance Faction / Vehicles / Jobs / Skills / Deployment
  [Overthrow] Found 1 towns
  ```
- `NETWORK : Starting RPL server, listening on address 0.0.0.0:<port>` is the unambiguous "accepting connections" marker; `tools/launch-server.sh` greps the live log for exactly this.
- Dedicated mode emits `BACKEND : Direct Join Code: <10 digits>` ~2 s after the listener is up — later than the readiness marker, which the launcher's watcher accounts for.
- The engine's own validation errors name the failure modes a source-built client will hit:
  `RplConnection::ValidationError remote script source code checksum does not match!`, `…remote checksum of loaded addons or their order does not match!`, `…isDevBinary value does not match!`.
- Pre-existing noise on the test world, not caused by any of this: `DestructibleEntity … was spawned at runtime. It won't take damage and wont replicate.` (repeated, tree prefabs).

---

## 5. The join path — VERIFIED (2026-08-06, same session)

Recorded after §1–§4 were written; it supersedes their "no client has ever connected" caveat.

| # | Attempt | Result |
|---|---|---|
| 7 | Workbench `-gproj <addon.gproj>` `-client 127.0.0.1:2001` + all five `-rpl-validation-*-disable` | **Ignored the client flag.** Never logged `Starting multiplayer client…`; server recorded no connection; a local session started instead. First attempt failed earlier for an unrelated reason (`-addonsDir` omitted → `Game addon '58D0FB3206B6F859' not found`). |
| 8 | User, manually: World Editor → test world → **Play** | Local session with the start-game menu. Confirms Workbench Play is local-only. |
| 9 | `tools/launch-game.sh --profile OverthrowClient1 -- -client 127.0.0.1:2001` | **CONNECTED.** No validation switches used. |

Experiment 9, both sides of the wire:

```
CLIENT  NETWORK : Starting multiplayer client using command line args.
        NETWORK : Starting RPL client, number of addresses to try connecting to: 1
        RPL  (D): Endpoint 127.0.0.1:2001 state 0: send ConnectionRequest, size 512.
        RPL     : ClientImpl event: connected (identity=0x00000000)
        NETWORK : ### Creating player: PlayerId=1, Name=<steam name>
SERVER  NETWORK : Player connected: connectionID=0
        NETWORK : ### Creating player: PlayerId=1, Name=<steam name>
        NETWORK : Players connected: 1 / 1
        RPL     : ServerImpl event: disconnected … reason=9      (operator closed the window)
```

What this establishes:

- **`-client <ip:port>` auto-joins the game client from the CLI** — no Direct Join menu navigation, which is what makes a scripted two-process loop conceivable.
- **A source-built client and a source-built server validate natively.** Script, addon and rdb checksums matched with **zero** `rpl-validation-*` switches. This is the important part: the two ends were running identical code, which the Workbench route could never have given (see §6).
- The client sits at the spawn-select camera ("the test world from a great height") — connected, not yet spawned.

## 6. The Workbench is not a viable client

Ruled out deliberately, because it is the obvious thing to try:

- **No GUI path.** The Workbench's play modes are `Play inside Viewport`, `Play in Fullscreen`, `Play from Camera Position`, `PlayWithoutDisabledLayers` — there is no join-a-server mode, confirmed by the user clicking Play and getting a local session.
- **No CLI path.** The binary contains the MP-client strings, but `-client` produces no `Starting multiplayer client…` and no connection.
- **Even if it worked, it should not be used.** `addon.gproj`'s `workbench` script configuration defines `WORKBENCH`, `PERSISTENCE_DEBUG`, `ENF_WB` and `DEBUG_NAVMESH_REBUILD_AREAS`; the server compiles the shipping config. The script checksums genuinely differ, and `-rpl-validation-scr-disable` silences the check without reconciling the code — client and server would run different builds, with `#ifdef WORKBENCH` branches live on one side only.

**The dev-only switch family** (in the Workbench and server binaries, *not* the retail client) is real and accepted in plain `-flag` form, each logging `RplSession::CheckWarning: <x> validation disabled. This option is DEVELOPER ONLY! The application might be unstable!` at `RplSession::Constructed`:

```
rpl-validation-devbin-disable   rpl-validation-addons-disable   rpl-validation-scr-disable
rpl-validation-rdb-disable      rpl-validation-version-disable
rpl-timeout-disable   rpl-timeout-ms   rpl-reconnect   rpl-vcons
```

Validation is **server-side** (`Rpc_Validation_S received` → `ValidationError` → `Rpc_ValidationPassed_O sent`), so a switch only matters on the server. None is needed for the verified path, and needing one is a signal that the two ends have diverged.

## 7. Local mode has no identities — the spawn stall, and the fix (2026-08-06)

Connecting was not enough to *play*. The first real play-test stalled at the spawn camera: connected, campaign started, never spawned.

```
SERVER  RPL     : ServerImpl event: authenticating (identity=0x00000000, address=127.0.0.1:54032)
        NETWORK : ### Updating player: PlayerId=1, Name=Aaron Static, rplIdentity=0x00000000, IdentityId=
        SCRIPT  : [Overthrow] OVT_SpawnLogic.DoSpawn_S called for playerId: 1
        SCRIPT  : [Overthrow] WARNING: Persistent UID not available yet for playerId: 1, retrying...
        ... once a second, forever
CLIENT  SCRIPT  : [Overthrow] Game started: false, Has save: false, Mode: 1, IsServer: false
        SCRIPT  : [Overthrow] Not showing start menu (multiplayer client, dedicated server, or game already started/loaded)
```

**Root cause.** Local mode (`-server`) never contacts the backend, so nobody is authenticated and `SCR_PlayerIdentityUtils.GetPlayerIdentityId()` returns empty forever. Overthrow keys every player record on that string, so `DoSpawn_S` can never register the player.

Two things this ruled out along the way, both worth recording because both looked like the cause:
- **The campaign had started.** `[Overthrow] Dedicated server: no existing campaign - starting a new game` fired at boot; the dedicated auto-start path works. The client's `Game started: false` is a client-side read before replication, not the problem.
- **The suppressed start menu is correct behaviour**, not a bug — a dedi is meant to start from config/defaults with no menu.

**Fix** (`OVT_Global.GetPlayerUID`, 2026-08-06): when the identity is empty *and* the session was launched with `-ovtDevUid`, synthesise `DEV_<playerId>`.

Design decisions, both deliberate:
- **Gated on a CLI parameter, not merely on "the identity is blank."** An empty identity is also the normal transient state while a real player authenticates — which is exactly why the retry loop exists. Synthesising on sight would hand a legitimate player a fresh blank record instead of waiting for their real one. A production server never passes the parameter.
- **Not a compile-time gate**, though that was the first instinct. `addon.gproj`'s only `ScriptConfigurationClass` with defines is `workbench` (`WORKBENCH`, `PERSISTENCE_DEBUG`, `ENF_WB`, `DEBUG_NAVMESH_REBUILD_AREAS`). The dedicated server compiles the shipping config, so a `#ifdef WORKBENCH` fallback would be dead code precisely where it must run. A new define would have to live in the `PC` config, which ships.
- **Derived from the runtime player id**, not from the account or name. Two clients launched from the same Steam account both report `Name=Aaron Static`; anything name- or account-shaped would collide into one player record.

`tools/launch-server.sh` passes the flag automatically in local mode only (`--no-dev-uid` suppresses), never in dedicated mode.

## 8. Two clients + JIP — VERIFIED (2026-08-06)

Server in local mode, two clients on separate profiles, one machine:

```
NETWORK : ### Creating player: PlayerId=1, Name=Aaron Static
SCRIPT  : [Overthrow] Setting up player data for: DEV_1
SCRIPT  : [Overthrow] Player assigned home at <226.19, 1.655, 193.901>
NETWORK : Players connected: 1 / 1
NETWORK : ### Creating player: PlayerId=2, Name=Aaron Static     <- joined a RUNNING campaign
SCRIPT  : [Overthrow] Setting up player data for: DEV_2
SCRIPT  : [Overthrow] Player assigned home at <232.689, 1, 27.62>
NETWORK : Players connected: 2 / 2
```

Distinct UIDs, distinct homes, both spawned; the operator confirmed spawning into the world. The second connection is JIP at the connection level — a client joining a campaign that was already running with another player in it.

**Operational trap found the hard way:** `launch-game.sh --timeout` defaults to **600 s** and the launcher kills the client when it expires, mid-play-test. A 70 s timeout used for a scripted check terminated a live session. Pass `--timeout 3600` for real play-testing.

## 9. Difficulty selection via `Overthrow_Config.json` (2026-08-06)

A dedi has no start menu, so the difficulty comes from `$profile:Overthrow_Config.json`. `OVT_OverthrowGameMode.DoStartGame` matches its `difficulty` string **by name** against `OVT_OverthrowConfigComponent.m_aDifficultyPresets` and, separately, `DoStartNewGame` reads `occupyingFaction`/`supportingFaction` from the same file (that pair is gated on `isDedicated`; difficulty is not).

- **The game writes this file itself.** `OVT_OverthrowConfigComponent.LoadConfig` creates it with defaults (`difficulty: ""`) when absent. So a launcher that only creates the file when missing does nothing from the second launch onward — it must patch the existing one. `tools/launch-server.sh` rewrites just the `difficulty` value with `sed`, leaving `officers`, item limits and the webhook intact, and prints the before/after.
- **`Test World` is not in the base game mode's preset list.** `Prefabs/GameMode/OVT_OverthrowGameMode.et` carries Easy / Normal / Hard / Extreme / Insane; `{0BA489F3657E13D8}Configs/Difficulty/Difficulty_TestWorld.conf` is added to the **test world's** config only. Hence the launcher applies it for the testworld scenario and leaves other scenarios' values alone — naming a preset that is not in the list would silently fall through to whatever `m_Difficulty` already held.
- Verified: `[Overthrow] Overthrow_Config.json - setting difficulty to Test World`, with the on-disk file showing only the difficulty field changed.

Path note: `$profile:` resolves to `<My Games>/<profile>/profile/` — the extra `profile/` level, same as the save-path finding in `test-coverage/findings.md` 1.8.

## 10. Not verified — do not assume

- **A second machine.** Everything so far is loopback on one box: LAN join, firewall and inbound 2001/17777 are untested.
- **JIP into a campaign with accumulated state.** §8's second client joined ~18 s into a fresh campaign. Joining hours in — towns taken, vehicles owned, recruits alive — is a materially different test.
- **That any Overthrow system replicates correctly.** §5 and §8 prove transport, registration and home assignment. They say nothing about groups, recruits, economy or persistence over the wire.
- **Dedicated mode with a client attached.** The two-client run was local mode with synthesised UIDs; dedicated mode's real-identity path has never had a client connect to it.
- RCON: initialisation was observed; **no command has been sent**. Its protocol, auth handshake and command surface are unexamined.
- Everything about a *second machine* — LAN join, firewall, ports 2001/17777 inbound.
- Whether a server session's saves under `<profile>/addons/saves` interact with the `.scripts/` save tooling, which assumes EPF's `.db/Overthrow` layout.
