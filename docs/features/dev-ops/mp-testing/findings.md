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

## 5. Not verified — do not assume

- **No client has ever connected to this server.** Everything above is server-side log evidence. The join handshake, the addon/script checksum match between a source-built client and this server, JIP, and any actual gameplay replication are **unproven**.
- Whether a source-built client must be launched with matching `-addonsDir`/`-addons` (near-certain, given the checksum errors above) and whether `isDevBinary` forces both sides onto the same Diag/non-Diag pairing.
- Whether `-client <address>` auto-joins from the command line. The param exists (`Starting multiplayer client using command line args.`, `Unable to connect as client to '%s'`), and `userName` sits beside it in the same parameter table, but neither was executed.
- RCON: initialisation was observed; **no command has been sent**. Its protocol, auth handshake and command surface are unexamined.
- Everything about a *second machine* — LAN join, firewall, ports 2001/17777 inbound.
- Whether a server session's saves under `<profile>/addons/saves` interact with the `.scripts/` save tooling, which assumes EPF's `.db/Overthrow` layout.
