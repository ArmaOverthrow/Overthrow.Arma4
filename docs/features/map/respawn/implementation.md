# Map Respawn — Implementation Plan

**Status:** ✅ **COMPLETE** — Phases 0–8 built 2026-08-10/11; **Phase 9 run by the user and reported all green 2026-08-11**, with no code change required to pass it
**Epic:** map (feature 5 of 8)
**Started:** 2026-08-10
**Target Completion:** TBD
**Last Updated:** 2026-08-11

> This plan supersedes three premises in `requirements.md`. That file is the **user's intent**, not a code
> reference; §5 K1–K3 record where it is wrong and what the code actually does. All `file:line` citations
> in this document are load-bearing — keep them when editing. Citations in **code comments** follow the
> epic's K-9 discipline instead: keep the rationale, name the symbol, drop the line number.

---

## 1. Executive Summary

Overthrow currently respawns a killed player **automatically, at home, one frame after death**. This feature
replaces that with a Conflict-style choice: on death the player is shown a dedicated respawn screen listing
every location they are entitled to spawn at, and picks one. **"Respawn at home" is always available**, even
when home would not otherwise qualify — that is the guaranteed floor and the reason the feature can never
strand a player.

Three corrections to `requirements.md` were verified against the tree during planning and change the shape of
the work materially:

1. **`OVT_PersistentRespawnLogic` is dead code.** The requirements name it, `SCR_FreeSpawnData`, `CanSpawn`
   and `GetPlayerRespawnComponent_S().RequestSpawn` as the path to change. No prefab or config references
   that class; its own header says so (`Scripts/Game/Respawn/Logic/OVT_PersistentRespawnLogic.c:5-6`), as do
   `docs/features/core/player-manager/implementation.md:45` and `docs/bugs/BUG-088.md:94`. **The live path is
   `OVT_SpawnLogic`** (`Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`), referenced by
   `OVT_RespawnSystemComponent` from `Prefabs/GameMode/OVT_OverthrowGameMode.et`. There is no
   `SCR_FreeSpawnData` and no `RequestSpawn` anywhere on the death path. The change is therefore **not**
   "swap the position source" — it is "**defer** an automatic character creation until the player has chosen".
2. **Respawning is not free today and stays that way.** `OnPlayerKilled_S:1312` calls
   `OVT_Global.GetEconomy().ChargeRespawn(playerId)`. That call is **untouched**. "Free" means _the location
   choice adds no cost_.
3. **The identity hazard is the most likely way this ships looking correct and showing an empty map.** All
   three copies of `GetCurrentPlayerID()` in the map UI resolve the persistent id **through the controlled
   entity** (`OVT_MapLocationType.c:544-555`, `OVT_MapLocationElement.c:316-328`,
   `OVT_OverthrowMapUI.c:1305-1317`) and return `""` on a miss. `OVT_MapLocationHouse.PopulateLocations`
   returns immediately on an empty id (fail-closed, by design — the N1 privacy fix), and
   `OVT_MapLocationCamp` uses it to filter private camps. `OVT_Global.GetController()` has the **same**
   dependency (`OVT_Global.c:88`), so the request component would be unreachable too. An empty id therefore
   produces a screen that draws nothing and whose buttons do nothing, with no error anywhere.

The work is five things: a **client-safe identity resolver**; a **shared rule set** (`OVT_RespawnService`);
a **server-authoritative request component** on `OVT_OverthrowController`; a **deferred death path** in
`OVT_SpawnLogic`; and a **dedicated SPAWNSCREEN map screen** that a dead player can operate on a gamepad.

---

## 2. Goals

### Primary

1. **The player chooses.** On death, a dedicated respawn screen appears listing every eligible location; the
   player picks one and their new character is created there.
2. **"Respawn at home" is always present and always works**, independent of any map selection, including when
   home sits inside an active QRF.
3. **A player is never left without a character.** Every path out of the awaiting-respawn state is enumerated
   and terminates either in a spawn or in a disconnect.
4. **A client cannot spawn itself at an arbitrary position.** The server re-derives the eligible set from its
   own managers and spawns at _its_ recorded position, never at the vector the client sent.
5. **Fully operable on gamepad/console.** This is a screen the player cannot skip; a mouse-only picker is not
   shippable.

### Secondary

6. **The living fullscreen map is unchanged.** No new mode, no new branch in `SetVisible`, no new runtime flag.
7. **One rule set, two machines** — the client decides what to draw and whether to offer the button; the
   server decides what actually happens; both run the same predicates.
8. **The pure half of the rules is asserted in the Logic tier**, which is the only tier that can see this
   feature at all.

### Explicit non-goals (honoured from `requirements.md`)

- Respawn costs beyond the existing `ChargeRespawn`, death penalties, wave timers, respawn tickets.
- Changing how home is set; multiple homes.
- Group/squad spawning; adopting vanilla's `SCR_SpawnPoint` system.
- Vehicle or loadout selection on the screen.
- Respawning at bus stops, ports, gun dealers, warehouses or towns.
- Reworking `OVT_StartGameContext` beyond copying its shape.
- Fixing epic tech debt T1 wholesale, the `file:line`-comment audit, or the `FindAnyWidget` sweep across
  `Scripts/Game/UI/Map/**` (this feature audits **its own** names — §6 Q-5).

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
OVT_OverthrowGameMode (server + client)
├── OVT_RespawnSystemComponent
│   └── OVT_SpawnLogic                        ~ awaiting-respawn state, deferred creation
└── m_RespawnUIContext : OVT_RespawnContext   NEW (configured attribute, mirrors m_StartGameUIContext)

OVT_OverthrowController (one per player, server-owned, client-owned proxy)
└── OVT_RespawnRequestComponent               NEW  ask/result RPC pair

OVT_PlayerController prefab
└── OVT_RespawnScreenHandlerComponent         NEW  EOnFrame driver (mirrors OVT_PlayerStartMenuHandlerComponent)

Configs/Map/MapRespawn.conf (SPAWNSCREEN)     NEW
└── OVT_RespawnMapUI : OVT_OverthrowMapUI     NEW  respawn button instead of travel controls
    └── Configs/Map/OverthrowMapRespawn.conf  NEW  four types only, m_fVisibilityZoom 0, m_bRespawnOnly 1

Scripts/Game/Services/OVT_RespawnService.c    NEW  the shared rule set (pure core + server enumeration)
```

### 3.2 The death sequence, end to end

```
 SERVER                                        CLIENT (the dead player's machine)
 ──────                                        ────────────────────────────────
 OnPlayerKilled_S
   ChargeRespawn                (UNCHANGED)
   clear m_sBodyPersistenceId,
     m_vLastKnownPosition/Angles (UNCHANGED)
   ClearPlayerGearSnapshot      (UNCHANGED)
   BeginAwaitingRespawn(playerId)   ◄── replaces CallQueue.Call(CreateCharacter, ...)
     ├─ no request component? ──► CreateCharacter now (degrade to today's behaviour, log ERROR)
     └─ m_aAwaitingRespawn.Insert(playerId)
        RpcDo_ShowRespawnScreen ─────────────► OVT_RespawnRequestComponent
        CallLater(ReAskRespawnScreen, 5s)       m_OnShowRespawnScreen.Invoke()
              (repeats while awaiting)               │
                                                     ▼
                                              OVT_RespawnScreenHandlerComponent
                                                OVT_RespawnContext.ShowLayout()
                                                  SetupMapConfig(SPAWNSCREEN, MapRespawn.conf, m_wRoot)
                                                  OpenMap(config)
                                                     │
                                              OVT_RespawnMapUI draws ONLY records that
                                              pass CanRespawn (m_bRespawnOnly), at every zoom
                                                     │
                                              player picks a marker ──► "Respawn here"
                                                     or presses "Respawn at home"
                                                     │
 RpcAsk_Respawn(destination, targetPos) ◄────────────┘
   ResolveOwningPlayerId()          (from the controller entity, never the payload)
   consume m_aAwaitingRespawn entry (idempotent: absent ⇒ ignore)
   destination == HOME
     └─ CreateFreshCharacter(playerId, persId)          ← today's untouched path
   destination == LOCATION
     └─ ResolveRespawnPosition -> server's OWN recorded vector
          matched   ─► CreateFreshCharacterAt(..., true, resolvedPos)
          no match  ─► CreateFreshCharacter(playerId, persId)   result = OK_FELL_BACK_HOME
   SendRespawnResult ───────────────────────► RpcDo_RespawnResult
                                                hint + m_OnRespawnResult.Invoke()
                                                context closes the screen + CloseMap()
```

### 3.3 Where authority lives

The map UI is a **client-only** projection: `OVT_MapLocationType` instances exist only on a machine that has
a map open, so the server cannot and does not use them. Authority is therefore **not** "re-run the map's
check" — it is a second, independent enumeration:

- **Client** draws a marker when the type's `CanRespawn(location, playerID, out reason)` says so.
- **Server** builds `OVT_RespawnService.CollectEligiblePositions(persId)` straight from the managers
  (`OVT_OccupyingFactionManager.m_Bases`, `OVT_ResistanceFactionManager.m_FOBs` / `.m_Camps`,
  `OVT_RealEstateManagerComponent.GetOwned/GetRented`) and accepts a request only if the sent vector matches
  one of those within `MATCH_TOLERANCE`.
- Both call the **same pure predicates** (`IsBaseEligible`, `IsCampEligible`, `IsHouseEligible`,
  `IsInsideQrf`), which is what keeps "displayed availability" and "enforced availability" in agreement.

The spawn uses the **server's** vector, not the client's, even on a match. A client that lies gets home.

### 3.4 The one identity fix everything hangs off

Add a single machine-scoped resolver that does **not** touch the controlled entity, and route the three
duplicate copies through it:

```
// OVT_Global — CLIENT-ONLY, like SCR_PlayerController.GetLocalPlayerId() itself
static string GetLocalPersistentId()
{
    OVT_PlayerManagerComponent players = GetPlayers();
    if (!players) return "";
    return players.GetPersistentIDFromPlayerID(SCR_PlayerController.GetLocalPlayerId());
}
```

`GetLocalPlayerId()` returns `0` with no player controller and `GetPersistentIDFromPlayerID` returns `""` for
`playerId < 1` (`OVT_PlayerManagerComponent.c:530-537`), so the failure mode is unchanged. `m_mPersistentIDs`
is populated on clients by `RpcDo_RegisterPlayer` (broadcast from `SetupPlayer`), so this works on a
dedicated-server client. When a controlled entity _does_ exist, both routes resolve the same player id — the
change is strictly widening and cannot alter the living map.

`OVT_Global.GetController()` gets the same treatment: fall back to `SCR_PlayerController.GetLocalPlayerId()`
when there is no controlled entity. That one branch is what makes `GetRespawnRequests()` reachable while dead,
and it also un-breaks every other controller component for any future dead-player feature.

---

## 4. Implementation Phases

Effort is **S / M / L** relative to a single focused session. "Agent" is the routing hint for `/proceed`.
Phases 1, 6 and 7 need the **advanced** variants — 1 and 7 because the screen is unskippable and
console-critical, 6 because it edits the project's most fragile system and a mistake means players with no
character.

---

### Phase 0 — Baseline — **S — no agent (already captured)**

Recorded 2026-08-10 on `new-map` at `28c2f957` + working tree:

| Gate                                             | Baseline                            |
| ------------------------------------------------ | ----------------------------------- |
| `tools/compile-check.sh`                         | **exit 0, 5958 files, Game module** |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | **OK, 44 tests, 15s**               |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All)  | **OK, 79 tests, 19s**               |

⚠️ `CLAUDE.md` says Fast 38 / All 66 and is **stale** — do not quote it. A _changed_ count is a finding to
investigate, never a number to update. Expected end-state: Fast 44 + N new Logic cases, All 79 + N.

---

### Phase 1 — Spike: can a workspace-hosted SPAWNSCREEN map be driven? — **M — `ui-developer-advanced`**

> **Advanced, and first, because it is the only genuine unknown in the feature.** Every other phase is a copy
> of an existing Overthrow pattern. This one is not: vanilla's SPAWNSCREEN map lives inside a
> `ChimeraMenuBase` (`SCR_DeployMenuBase.InitMapDeploy`, `SCR_DeployMenuBase.c:670-681`), and Overthrow has
> **no `ChimeraMenuBase` menus at all** — its screens are workspace layouts driven by `OVT_UIContext`. If a
> workspace-hosted map cannot get input, everything downstream changes shape, so find out on day one.

**Tasks**

1. Create `UI/Layouts/Respawn/OVT_RespawnScreen.layout`. It **must** embed vanilla's map frame as an
   inherited widget — `FrameWidgetClass : "{0651202E9F2646DE}UI/layouts/Map/Map.layout"` — because
   `SCR_MapEntity.OpenMap` does `config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.MAP_WIDGET_NAME)`
   (`SCR_MapEntity.c:296`) and `MAP_WIDGET_NAME` is `"MapWidget"`, defined inside `Map.layout` under a frame
   named `MapFrame`. This is exactly how `RoleSelectionMenu.layout:12` and `WelcomeScreenMenu.layout:15` do it.
   Add a title bar and two `WLib_NavigationButton`s (`RespawnHomeButton`, and a status/hint text) — the
   per-marker "Respawn here" button belongs to the info panel, not here.
2. Create `Configs/Map/MapRespawn.conf` as a **new file with a new GUID**, `m_iMapMode SPAWNSCREEN`, modelled
   on `ArmaReforger/Configs/Map/MapSpawnMenu.conf`. Register `OVT_OverthrowMapUI` for now (the subclass
   arrives in Phase 4) with `m_bShowSpawnPoints 0` and `m_bShowTasks 0` — those are vanilla attributes and
   leaving them on would draw vanilla's own spawn icons over Overthrow's.
   - ⚠️ Unlike `Configs/Map/MapOverthrow.conf`, this is **not** a same-GUID delta over a vanilla file. It is
     a standalone config, so it inherits **nothing** — every module, layer and props config vanilla's
     `MapSpawnMenu.conf` carries must be listed explicitly or the map will render without them.
3. Create `Scripts/Game/UI/Context/OVT_RespawnContext.c` (`OVT_UIContext`): `OnShow()` calls
   `SCR_MapEntity.GetMapInstance().SetupMapConfig(EMapEntityMode.SPAWNSCREEN, MAP_RESPAWN_CONF, m_wRoot)` then
   `OpenMap(config)`; `OnClose()` calls `CloseMap()`. `m_sCloseAction` stays **empty** — this screen has no
   dismiss.
4. Temporary harness only: add a debug keybind (or reuse `OVT_AdminCommandsComponent`) that shows the context,
   so the spike needs no server change.
5. **Measure and write down**: does the map pan, zoom and decluster? Do markers hover and select? Is
   `MapContext` active (test `MapSelect` on mouse **and** `gamepad0:a`)? Does the cursor appear? Does
   `SCR_MapUIElementContainer.m_bIsDeployMap` (set from `SPAWNSCREEN` at `SCR_MapUIElementContainer.c:153`)
   change anything visible?
6. Confirm the **`SetupMapConfig` cache**: it early-returns the _currently active_ config when
   `mapMode == m_eLastMapMode` (`SCR_MapEntity.c:471-475`), only swapping the root widget. Two deaths in a row
   without opening the gadget map in between therefore reuse the cached SPAWNSCREEN config. That is correct
   **only while Overthrow has exactly one SPAWNSCREEN config** — record that as a constraint.

**Acceptance**

- The respawn screen opens over a live session, the map is visible, and pan/zoom/marker-hover work with a
  mouse.
- The same is true with a gamepad only, or the fallback below is chosen and recorded.
- `tools/compile-check.sh` exit 0, file count 5958 + new script files.
- **Fallback recorded if it fails:** host the screen in a `ChimeraMenuBase` menu registered in the UI menu
  presets, modelled directly on `SCR_DeployMenuBase`, and keep `OVT_RespawnContext` only as the state machine.
  Decide this in Phase 1, not later — Phase 7 depends on the answer.

---

### Phase 2 — Corpse-independent identity — **S — `component-developer`**

**Tasks**

1. Add `OVT_Global.GetLocalPersistentId()` exactly as in §3.4, in the client-only neighbourhood of
   `OVT_Global.c`, with a `//!` block saying it must never be called from anything the server can reach.
2. Add the no-controlled-entity fallback to `OVT_Global.GetController()` (`OVT_Global.c:86-93`): when
   `SCR_PlayerController.GetLocalControlledEntity()` is null, resolve the player id from
   `SCR_PlayerController.GetLocalPlayerId()` instead of returning null. Keep the existing branch first and
   unchanged, so the living path is byte-identical.
3. Add `OVT_Global.GetRespawnRequests()` in the shape of `GetTravelRequests()` (`OVT_Global.c:131-137`).
4. Route the three duplicate `GetCurrentPlayerID()` bodies through `GetLocalPersistentId()`:
   `OVT_MapLocationType.c:544-555`, `OVT_MapLocationElement.c:316-328`, `OVT_OverthrowMapUI.c:1305-1317`.
   Keep the method names — five location types and two UI classes call them.

**Acceptance**

- `grep -rn "GetLocalControlledEntity" Scripts/Game/UI/Map/` returns only `OVT_MapLocationData.GetDistanceFromPlayer`,
  `OVT_OverthrowMapUI.GetMap`/`StowMapGadget` (map-gadget helpers, correctly entity-scoped) and nothing in an
  identity path.
- Living fullscreen map: houses, private camps and player vehicles still appear for their owner and still do
  **not** appear for anybody else (this is the N1 privacy contract — it must not move).
- compile exit 0; Fast 44; All 79.

---

### Phase 3 — `OVT_RespawnService` + the `CanRespawn` contract + Logic tests — **M — `component-developer`**

**Tasks**

1. Create `Scripts/Game/Services/OVT_RespawnService.c`, modelled on `OVT_FastTravelService.c` including its
   fenced **CLIENT-ONLY** discipline. It holds:
   - `enum OVT_RespawnDestination { HOME, LOCATION }`
   - `enum OVT_RespawnResult { OK, OK_FELL_BACK_HOME, NO_PLAYER, NOT_ELIGIBLE, SPAWN_FAILED }`
   - **Pure predicates** (no world, no manager, no game mode — this is what the Logic tier can assert):
     `IsBaseEligible(bool isOccupying)`, `IsFobEligible()`,
     `IsCampEligible(bool isPrivate, string ownerId, string playerId)`,
     `IsHouseEligible(string ownerId, string renterId, string playerId)`,
     `IsInsideQrf(bool qrfActive, vector qrfLocation, vector pos)`,
     `PositionsMatch(vector a, vector b)`, `ReasonKeyFor(int result)`.
   - **Shared world lookup, safe on both machines:** `IsPositionInActiveQRF(vector pos)` reads
     `OVT_Global.GetOccupyingFaction().m_bQRFActive` / `.m_vQRFLocation` and delegates to `IsInsideQrf`,
     using `OVT_QRFControllerComponent.QRF_RANGE` (`= 750`, `OVT_QRFControllerComponent.c:20`).
   - **Server enumeration:** `CollectEligiblePositions(string persId, notnull array<vector> out)` and
     `ResolveRespawnPosition(string persId, int destination, vector requestedPos, out vector resolvedPos)`.
     Neither resolves the actor from the machine; the persistent id is always a parameter.
   - `static const float MATCH_TOLERANCE = 2.0;` with a comment saying why it is not zero (float transport)
     and why it is small (it is the anti-arbitrary-position rule).
   - ❌ **Nothing in this file calls `OVT_FastTravelService.CanGlobalFastTravel`.** Add a `//!` line saying so
     and why: its minimum-distance rule, its wanted rule and its whole fare model are measured from a living
     player's current position and are meaningless for a respawn.
2. Add `CanRespawn(OVT_MapLocationData location, string playerID, out string reason)` to
   `OVT_MapLocationType` (`OVT_MapLocationType.c`), **defaulting to refuse**, immediately after
   `CanFastTravel`. Doxygen it as a **hot path** (reached per element on every zoom change once
   `m_bRespawnOnly` is set) — map lookups and comparisons only.
3. Add `[Attribute(defvalue: "false", ...)] bool m_bRespawnOnly;` to `OVT_MapLocationType` and gate
   `ShouldShowLocation` on it:

   ```
   bool ShouldShowLocation(OVT_MapLocationData location, string playerID)
   {
       if (!location.m_bVisible) return false;
       if (!m_bRespawnOnly) return true;
       string reason;
       return CanRespawn(location, playerID, reason);
   }
   ```

   Verified safe: **no location type overrides `ShouldShowLocation`** today — the only implementation is the
   base one, and the only caller is `OVT_MapLocationElement.SetVisible:498`. With the attribute defaulting to
   `false`, every entry in `Configs/Map/OverthrowMap.conf` behaves exactly as it does now.

4. Override `CanRespawn` on the four types, reusing the same data keys their `CanFastTravel` overrides already
   read, **minus the `CanGlobalFastTravel` tail**, plus the QRF exclusion:
   - `OVT_MapLocationBase.c` — refuse when `GetDataBool("isOccupying", true)`; reason `#OVT-Respawn_EnemyBase`.
   - `OVT_MapLocationFOB.c` — always eligible.
   - `OVT_MapLocationCamp.c` — refuse when `isPrivate` and `owner != playerID`; reason
     `#OVT-Respawn_PrivateCamp`.
   - `OVT_MapLocationHouse.c` — refuse when `OWNER != playerID && RENTER != playerID`; reason
     `#OVT-Respawn_NotYourHouse`.
   - All four then `if (OVT_RespawnService.IsPositionInActiveQRF(location.m_vPosition)) { reason =
"#OVT-Respawn_QRF"; return false; }`.
   - **T1:** new code uses **idiom A** (the inherited manager cache from `Init()`). `OVT_MapLocationBase`'s
     own shadowing `m_OccupyingFactionManager` is left alone — it serves `PopulateLocations`, which this
     feature does not touch, and rewriting a working populate path for cosmetics adds risk to the one feature
     whose blast radius is the spawn system. Recorded as still-open T1, not widened.
5. Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RespawnRules.c` and register it in
   `OVT_TEST_LogicSuite`. **Tier rule is absolute**: no manager, no game mode, no world, and the manager
   accessor's identifier must not appear _anywhere_ in the file, comments included. Cases:
   1. `IsBaseEligible(true) == false`, `IsBaseEligible(false) == true`.
   2. `IsCampEligible(false, "someone-else", "me") == true` (public camp).
   3. `IsCampEligible(true, "someone-else", "me") == false` (private, not yours).
   4. `IsCampEligible(true, "me", "me") == true` (private, yours).
   5. `IsHouseEligible("me", "", "me") == true`; `IsHouseEligible("", "me", "me") == true`;
      `IsHouseEligible("them", "them", "me") == false`.
   6. `IsHouseEligible("", "", "") == false` — **the empty-id case**, pinning correction 3: an unresolved
      player id must not match an unowned record.
   7. `IsInsideQrf(false, anywhere, pos) == false` regardless of distance.
   8. `IsInsideQrf(true, origin, pos)` true just inside 750 m, false just outside.
   9. `PositionsMatch` true at 0 m and just inside `MATCH_TOLERANCE`, false just outside.
   10. `ReasonKeyFor(OVT_RespawnResult.OK) == ""` and every non-OK code returns a non-empty key.
       **Prove each case can fail before shipping it** and record the method (invert the assertion, observe
       exit 1, revert). ❌ **No `maxAttempts`.**

**Acceptance**

- compile exit 0.
- Fast = 44 + (number of new cases); All = 79 + the same number. Any _other_ delta is a finding.
- Every new case demonstrated able to fail, with the method written into `tasks.md`.
- Living fullscreen map visually unchanged (spot-check all ten types at three zoom levels).

---

### Phase 4 — The respawn map: config, eligible-only markers, respawn info panel — **M — `ui-developer`**

**Tasks**

1. Create `Configs/Map/OverthrowMapRespawn.conf` — an `OVT_OverthrowMapConfig` declaring **only** the four
   eligible types. For each: `m_fVisibilityZoom 0`, `m_bRespawnOnly 1`, `m_fRefreshInterval 0`,
   `m_bShowDistance 0`, `m_bShowName 1`, `m_fShowNameZoom 0`.
   - `m_fVisibilityZoom 0` is how "all eligible locations are visible at every zoom level" is satisfied —
     **in config, with no change to `OVT_MapLocationElement.SetVisible`** (§5 K6).
   - `m_fRefreshInterval 0` is deliberate: BUG-136's reconciliation destroys elements _while the map is open_,
     and the respawn screen is the one screen the player cannot leave. The server re-validates on arrival
     anyway, so staleness costs a fallback-to-home, not a wrong spawn.
   - `m_bShowDistance 0` because `OVT_MapLocationData.GetDistanceFromPlayer()` returns `-1` without a
     controlled entity (`OVT_MapLocationData.c:132-139`).
2. Create `Scripts/Game/UI/Map/OVT_RespawnMapUI.c` extending `OVT_OverthrowMapUI`. It overrides exactly one
   method: `SetupTravelButton(location)` — **without calling super** — to set `m_PanelLocation` and wire a
   `RespawnButton` instead. Explicit rather than relying on the base's `if (!travelButton) return;`, so a
   later copy-paste of a `FastTravelButton` into the respawn panel cannot silently re-enable travel.
3. Create `UI/Layouts/Map/Core/OVT_MapInfoPanelRespawn.layout` — the panel shell with `LocationName`,
   `LocationType`, `ContentSlot`, `CloseButton`, and a `RespawnButton` (`WLib_NavigationButton` +
   `SCR_InputButtonComponent`). It carries **no** `FastTravelButton`, `FastTravelReason`,
   `BringRecruitsButton` or `Distance` widget — the requirement "no fast-travel button, cost label or reason
   text on this screen" is met by the layout, not by a code branch.
4. Point `MapRespawn.conf` at `OVT_RespawnMapUI` with
   `m_LocationElementLayout` = the existing `OVT_MapLocationElement.layout` (shared, unchanged),
   `m_InfoPanelLayout` = the new respawn panel, and `m_Config` = `OverthrowMapRespawn.conf`.
5. `OVT_RespawnMapUI.OnRespawnClicked()` mirrors `OnTravelClicked` (`OVT_OverthrowMapUI.c:1209-1241`):
   read `m_SelectedElement`'s location, call
   `OVT_Global.GetRespawnRequests().RequestRespawn(OVT_RespawnDestination.LOCATION, location.m_vPosition)`,
   and `Print(LogLevel.ERROR)` when the component is null. It does **not** close the screen — the server's
   result does (§5 K9).
6. **Layout↔code name audit.** Every `FindAnyWidget` name this feature introduces is listed, and each is
   grepped against the layout that must define it. This is the D1/D2 failure mode
   (`FindAnyWidget` returning null is a silent no-op `compile-check.sh` cannot see) and it is the single most
   likely way a configured feature ships completely dead.

**Acceptance**

- Opening the spike screen shows Base / FOB / Camp / House markers **only**, at maximum zoom-out.
- An enemy-held base, another player's private camp and a house you neither own nor rent are **absent**, not
  greyed.
- Selecting a marker shows a panel with a working "Respawn here" button, no travel controls, no cost, no
  reason text, no distance row.
- compile exit 0; Fast/All unchanged from Phase 3.

---

### Phase 5 — `OVT_RespawnRequestComponent` — **M — `network-specialist-advanced`**

> Copy `Scripts/Game/Components/Controller/OVT_TravelRequestComponent.c` line for line, including its
> discipline. ⚠️ `Rpc()`'s prototype is untyped variadic: a wrong argument count **compiles clean and dies
> silently at the wire** (BUG-090). Keep the signature count to three and hand-check each call.

**Tasks**

1. Create `Scripts/Game/Components/Controller/OVT_RespawnRequestComponent.c` (`OVT_Component`) with exactly
   three RPCs:
   - `[RplRpc(RplChannel.Reliable, RplRcver.Server)] RpcAsk_Respawn(int destination, vector targetPos)`
   - `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] RpcDo_ShowRespawnScreen()`
   - `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] RpcDo_RespawnResult(int result)`
2. `ResolveOwningPlayerId()` copied verbatim from `OVT_TravelRequestComponent.c:391-412` — identity comes from
   the controller entity, **never** from the payload.
3. **Listen-server short-circuits, both directions.** `SendRespawnResult` and `AskShowRespawnScreen` each test
   `playerId == SCR_PlayerController.GetLocalPlayerId()` and call the handler directly, exactly as
   `SendTravelResult` does (`OVT_TravelRequestComponent.c:364-377`) — a listen-server host never receives its
   own owner-targeted RPCs, and this is a known bug class in this project.
   `RequestRespawn` mirrors `RequestTravel:45-53`: call directly when `Replication.IsServer()`, else `Rpc()`.
4. `RpcAsk_Respawn` order, and nothing else:
   1. `if (!Replication.IsServer()) return;`
   2. arrival `Print` — the BUG-090 diagnostic that distinguishes "the request never left the client" from
      "a rule refused".
   3. reject an out-of-range `destination` outright (an unknown value must not fall through to a branch that
      skips validation — the exact defect `RpcAsk_Travel:95-99` guards).
   4. `playerId = ResolveOwningPlayerId()`; refuse `<= 0`.
   5. hand off to `OVT_SpawnLogic.GetInstance().CompleteRespawn(playerId, destination, targetPos)`.
   6. `SendRespawnResult(playerId, result)` — **every** outcome reports, including OK. Silence is
      indistinguishable from a request that never arrived.
5. Client side: two `ScriptInvoker`s, `m_OnShowRespawnScreen` and `m_OnRespawnResult(int result)`.
   `RpcDo_RespawnResult` shows `OVT_RespawnService.ReasonKeyFor(result)` as a hint for every non-OK result and
   `#OVT-Respawn_FellBackHome` for `OK_FELL_BACK_HOME`. It **never** moves anything or mutates state.
6. Register `OVT_RespawnRequestComponent` on `Prefabs/GameMode/OVT_OverthrowController.et` with a fresh GUID.
   ⚠️ ❌ **Do not add anything to `OVT_PlayerCommsComponent`** — it is deprecated and
   `map/legacy-retirement` P4 deleted four unvalidated RPCs from it for exactly this reason.

**Acceptance**

- compile exit 0.
- Manual, single player: the arrival `Print` fires on a request and the result `Print` reports a code.
- Manual, two clients on `tools/launch-server.sh`: player A's request never resolves to player B (assert via
  the printed player ids).
- Fast/All unchanged.

---

### Phase 6 — Defer the death path in `OVT_SpawnLogic` — **L — `component-developer-advanced`**

> ⚠️ **Advanced, and the highest-consequence phase in the feature.** A mistake here means players with no
> character. Read the file's header comments before touching it: `CreateFreshCharacter` and
> `RetryCreateCharacter` both _deliberately tolerate a dead controlled entity_
> (`OVT_SpawnLogic.c:257-260`, `:323-328`) because the death path re-creates a character while the controller
> still references the corpse. **Deferring lengthens that window from one frame to as long as the player
> takes to choose** — every guard that reads `GetControlledEntity()` must be re-read with that in mind.

**Tasks**

1. Add the awaiting state, mirroring the existing `m_aPendingBodySpawns` idiom in the same file
   (`OVT_SpawnLogic.c:56`):
   `protected ref array<int> m_aAwaitingRespawn = {};`
2. Replace the last line of `OnPlayerKilled_S` (`:1339`,
   `GetGame().GetCallqueue().Call(CreateCharacter, playerId, playerUid)`) with
   `BeginAwaitingRespawn(playerId, playerUid)`. **Everything above that line is untouched**, including
   `ChargeRespawn`, the body-id clear, the last-known-position clear and the gear-snapshot clear.
3. `BeginAwaitingRespawn(playerId, persId)`:
   - idempotent — return if `playerId` is already in `m_aAwaitingRespawn`.
   - **capability check at t=0, not a timeout:** resolve
     `OVT_Global.GetPlayers().GetController(playerId)` and its `OVT_RespawnRequestComponent`. If either is
     missing, `Print(LogLevel.ERROR)` naming the prefab and call `CreateCharacter(playerId, persId)`
     immediately — a misconfigured controller prefab degrades to today's behaviour instead of bricking the
     server.
   - otherwise insert, ask the client to show the screen, and start the re-ask tick.
4. `ReAskRespawnScreen(playerId, persId)` on a 5 s `CallLater` chain, re-scheduling itself while the entry
   survives. Each tick, in order:
   - entry gone ⇒ stop (this is how the chain self-cancels).
   - no player controller ⇒ drop the entry and stop (they left between disconnect events).
   - a **living** controlled entity exists (`!OVT_PlayerManagerComponent.IsCharacterDead`) ⇒ something else
     gave them a character; drop the entry and stop.
   - otherwise re-send `RpcDo_ShowRespawnScreen` and reschedule.
     This is the reconciliation of D-3's "no timeout" with the never-stranded invariant: the screen is retried
     forever so a lost RPC or a not-yet-ready JIP client always eventually gets it, and **nothing ever spawns
     the player without a pick**.
5. `CompleteRespawn(int playerId, int destination, vector requestedPos)` — the only entry point the request
   component calls, and it returns an `OVT_RespawnResult`:
   - resolve `persId` from `OVT_Global.GetPlayerUID(playerId)`; refuse `NO_PLAYER` on empty.
   - consume the `m_aAwaitingRespawn` entry; **absent ⇒ return without spawning** (a duplicate, a late
     packet, or a request from a player who is not dead — this single check is what makes the RPC unable to
     conjure a second character).
   - `destination == HOME` ⇒ `CreateFreshCharacter(playerId, persId)`. **Today's untouched path**, which
     already does home → `IsSpawnLocationSafe` → `FindSafeSpawnLocation` (`GetCreationPosition:768-813`), so
     the requirement "home must always resolve to a usable position and must not be regressed" is met by not
     touching it.
   - `destination == LOCATION` ⇒ `OVT_RespawnService.ResolveRespawnPosition(...)`. On a match,
     `CreateFreshCharacterAt(playerId, persId, true, resolvedPos)` with the **server's** vector. On no match,
     `CreateFreshCharacter(playerId, persId)` and return `OK_FELL_BACK_HOME`.
6. **The position override carries no stored state.** Split `CreateFreshCharacter` as:

   ```
   protected void CreateFreshCharacter(int playerId, string persId)
   {
       CreateFreshCharacterAt(playerId, persId, false, vector.Zero);
   }

   protected void CreateFreshCharacterAt(int playerId, string persId, bool useChosenPosition, vector chosenPosition)
   { ... }
   ```

   `CreateFreshCharacterAt` is the old body with one branch: use `chosenPosition` when
   `useChosenPosition`, else call `GetCreationPosition` exactly as before. **A parameter, not a member and not
   a map** — a stale override is the obvious defect in this feature and this shape makes it impossible rather
   than unlikely. The three existing callers (`CreateCharacter:293`, `OnPlayerBodySpawned:562` and `:579`,
   `OnPlayerBodySpawnTimeout:673`) keep calling the two-argument form and are not edited.
   Note `CompleteRespawn` deliberately calls `CreateFreshCharacter*` and **not** `CreateCharacter`: death has
   already cleared `m_sBodyPersistenceId`, so the persisted-body route is unreachable anyway, and bypassing
   it removes a whole class of interaction between respawn choice and body restoration.

7. Override `OnPlayerDisconnected_S(int playerId, KickCauseCode cause, int timeout)` (the virtual exists on
   `SCR_SpawnLogic` at `SCR_SpawnLogic.c:86` and `OVT_SpawnLogic` does not currently override it): call
   `super`, then drop any `m_aAwaitingRespawn` entry. No character is created — reconnect goes through the
   normal `OnPlayerAuditSuccess_S → ExcuteInitialLoadOrSpawn_S → DoSpawn_S → SpawnDeferredPlayer` path, which
   with a cleared body id and a zeroed last-known position lands them at home. That is precisely D-3's
   "let normal reconnect/spawn handling take over".
8. **Audit the widened corpse window.** Walk every guard in the file that tests `GetControlledEntity()` and
   confirm each still reads correctly when the corpse persists for minutes rather than one frame:
   `SpawnDeferredPlayer:178`, `RetryCreateCharacter:258-260`, `CreateFreshCharacter:323-328`,
   `OnPlayerBodySpawned:586`, `OnPlayerBodySpawnTimeout:664`. Note that `OnPlayerBodySpawned:586` and
   `OnPlayerBodySpawnTimeout:664` test a _bare_ `GetControlledEntity()` without the dead check — reachable
   only via the stored-body route, which death cannot take, but write down the reasoning rather than assuming
   it.

**Acceptance**

- Single player: die → **no character is created**; the server log shows the awaiting entry and the re-ask
  ticks; a `CompleteRespawn(HOME)` (driven from the spike harness or an admin command) creates a character at
  home with the civilian loadout, exactly as before this feature.
- `CompleteRespawn` called twice for the same death creates **one** character.
- Disconnect while awaiting, then reconnect: the player gets a character at home, and the log shows the
  awaiting entry dropped by `OnPlayerDisconnected_S`.
- Deleting `OVT_RespawnRequestComponent` from the controller prefab reproduces today's behaviour with a loud
  ERROR line (revert immediately — this is a one-off proof of the degrade path).
- compile exit 0; Fast 44 + new; All 79 + new. ⚠️ **A change in the Persistence or Campaign tier counts here
  is a finding**, not a number to update — those tiers exercise spawn state.

---

### Phase 7 — Wire the screen end to end, gamepad, localization — **M — `ui-developer-advanced`**

**Tasks**

1. Create `Scripts/Game/Components/Player/OVT_RespawnScreenHandlerComponent.c` (`ScriptComponent` on
   `Prefabs/Characters/Core/OVT_PlayerController.et`), modelled on `OVT_PlayerStartMenuHandlerComponent.c`:
   - `EOnFrame` drives `m_RespawnContext.EOnFrame(owner, timeSlice)` so the input context is activated every
     frame while the screen is up — the same mechanism the start menu uses.
   - Polls for `OVT_Global.GetRespawnRequests()` and subscribes to `m_OnShowRespawnScreen` /
     `m_OnRespawnResult` once, then stops polling.
   - Resolves the context from the game mode: add `[Attribute()] ref OVT_RespawnContext m_RespawnUIContext;`
     and `GetRespawnContext()` to `OVT_OverthrowGameMode`, mirroring `m_StartGameUIContext` /
     `GetStartGameContext()` (`OVT_OverthrowGameMode.c:11-12`, `:101-104`), and configure it in
     `Prefabs/GameMode/OVT_OverthrowGameMode.et`.
   - **Re-open guard:** while the context is active, poll `SCR_MapEntity.GetMapInstance().IsOpen()` and re-open
     if it closed. A poll, deliberately — ❌ **no Overthrow context may subscribe to a static
     `SCR_MapEntity.GetOn*` invoker** (BUG-069 part 4 is structurally closed and `grep -rn
"SCR_MapEntity.GetOn"` must stay empty).
2. `OVT_RespawnContext`: wire `RespawnHomeButton` to
   `OVT_Global.GetRespawnRequests().RequestRespawn(OVT_RespawnDestination.HOME, vector.Zero)`. Close the
   screen on **either** an OK/OK_FELL_BACK_HOME result **or** the local player acquiring a living controlled
   entity (the belt, in case the result RPC is lost). Non-OK results leave the screen open and show the
   reason.
3. Input: add `OverthrowRespawnHere` and `OverthrowRespawnAtHome` actions plus an
   `ActionContext OverthrowRespawnContext` to `Configs/System/chimeraInputCommon.conf`, including
   `MenuUp/Down/Left/Right/MenuSelect` for list-free gamepad navigation the way `OverthrowMainMenuContext`
   does.
   - Proposed bindings: **Respawn here** `keyboard:KC_RETURN` + `gamepad0:x`; **Respawn at home**
     `keyboard:KC_H` + `gamepad0:y`.
   - ⚠️ `MapContext` is **also active** on this screen (the map needs `MapSelect`), and Overthrow already adds
     `OverthrowFastTravel` (`KC_SPACE` + `gamepad0:pad_right`), `OverthrowToggleRecruits` (`KC_R`) and
     `OverthrowCloseInfoPanel` (`KC_C` + `gamepad0:b`) to it. Avoid all six inputs and `gamepad0:a`/
     `mouse:button0`.
   - ⚠️ The repo's input-conflict checker has a **known blind spot**: ~197 inline `ActionContext` actions are
     invisible to it. Cross-check by hand against vanilla's `MapContext` block
     (`ArmaReforger/Configs/System/chimeraInputCommon.conf:8128+`) and Overthrow's `MapContext` `ActionRefs`.
4. Add every new string id to **`Language/localization_Overthrow.st` only**:
   `#OVT-Respawn_Title`, `#OVT-Respawn_Here`, `#OVT-Respawn_AtHome`, `#OVT-Respawn_Hint`,
   `#OVT-Respawn_FellBackHome`, `#OVT-Respawn_EnemyBase`, `#OVT-Respawn_PrivateCamp`,
   `#OVT-Respawn_NotYourHouse`, `#OVT-Respawn_QRF`, `#OVT-Respawn_NotEligible`, `#OVT-Respawn_Failed`.
   ❌ **Never edit `Language/localization_Overthrow.<lang>.conf`** — those are Workbench-generated exports the
   user regenerates; hand-editing corrupts them silently. Until the user regenerates, any layout referencing
   an unexported key must carry **literal text**.
5. Gamepad pass with the controller unplugged from the mouse: every affordance on the screen must be reachable
   and every button must show a glyph. `SCR_InputButtonComponent` refuses **both** its input paths on an
   invisible or disabled widget, so hidden ≠ merely invisible.

**Acceptance**

- Die → screen appears within ~1 s → pick a marker → spawn there → screen closes and the HUD returns.
- "Respawn at home" works from the moment the screen appears, with nothing selected.
- The screen cannot be dismissed: `Esc`, `M`, the map-gadget key, `MenuBack` and every bound close action
  leave it up.
- Full gamepad-only pass, no mouse.
- compile exit 0; Fast/All unchanged from Phase 6.

---

### Phase 8 — Docs and contract records — **S — `component-developer`**

**Tasks**

1. Add two rows to the contract table in `docs/features/map/core/context.md`, in that file's exact format
   (bold the new member; open the Purpose cell with a bolded attribution):
   ```
   | **`CanRespawn(location, playerID, out reason)`** | virtual | **Added by `map/respawn` (2026-08-10).** Per-record respawn eligibility, defaulting to refuse. Overridden on Base/FOB/Camp/House. **Hot path** — reached from `ShouldShowLocation` on every zoom change whenever `m_bRespawnOnly` is set. |
   | **`m_bRespawnOnly`** | attribute | **Added by `map/respawn` (2026-08-10).** `1` = this instance draws only records that pass `CanRespawn`. Set only in `Configs/Map/OverthrowMapRespawn.conf`; `0` (default) leaves the living map byte-for-byte unchanged. |
   ```
2. Add rows to that file's **Layout ↔ code names** table for `RespawnButton`, `RespawnHomeButton` and the
   inherited `MapFrame`/`MapWidget` pair, with the code that reads each.
3. Update `docs/features/map/epic-overview.md` row 5 and the rollup.
4. Correct `OVT_MapContext.HideMap()`'s `//!` block (`OVT_MapContext.c:44-46`), which currently names
   `map/respawn` as its consumer — see §5 K10.
5. Write `docs/features/map/respawn/context.md` with the findings, the gotchas discovered and the
   still-unverified branches.

**Acceptance** — the two tables in `map/core/context.md` describe the shipped contract, and no doc claims a
consumer that does not exist.

---

### Phase 9 — Verification gate — **M — user-driven, no agent**

Run §6 _Verification Method_ in order and record every result. This gate is the **only** evidence that exists
for the two new `.conf` files and the two new `.layout` files — those four file classes are invisible to
`tools/compile-check.sh` **and** to both test groups, so a dangling GUID or a mistyped widget name passes every
automated gate and fails in the world.

---

## 5. Key Technical Decisions

**K1 — `OVT_PersistentRespawnLogic` is dead; the live path is `OVT_SpawnLogic`, and the change is a deferral,
not a position swap.** `requirements.md:11-20` builds its whole cost estimate on
`OVT_PersistentRespawnLogic.OnPlayerSpawnRequest_S:132-138`, `SCR_FreeSpawnData`, `CanSpawn` and
`GetPlayerRespawnComponent_S().RequestSpawn`. That class is referenced by no prefab and no config; its own
header says so at `:5-6`, corroborated by `docs/features/core/player-manager/implementation.md:45` and
`docs/bugs/BUG-088.md:94`. The method it cites is actually named `Spawn()` and has no callers. The live chain
is `OVT_SpawnLogic.OnPlayerKilled_S:1306` → `CreateCharacter:288` → `CreateFreshCharacter:309` →
`GetCreationPosition:768`, and it contains **no** `SCR_FreeSpawnData` and **no** `RequestSpawn`. The
consequence for planning is real: the requirements' "only the _source of the position_ changes" is wrong, and
the actual work — defer an automatic creation, hold per-player state on the server, guarantee every exit from
that state — is where the risk lives.

**K2 — `ChargeRespawn` is untouched, and "free" means the location choice adds nothing.**
`OnPlayerKilled_S:1312` calls `OVT_Global.GetEconomy().ChargeRespawn(playerId)`
(`OVT_EconomyManagerComponent.c:1869`), which takes `m_Difficulty.respawnCost` when the player holds more than
$500. `requirements.md:45` reads "Respawning is free. No cost, no payment path, no affordability check" — that
is true of _this feature_, not of respawning. Settled: the charge stays where it is, charged at death, before
any choice is made, so the fee cannot vary by destination and there is nothing for the player to game. No
per-destination fare, no affordability check, no cheaper or pricier spawn point, and **nothing in this feature
touches the economy**.

**K3 — Resolving the local persistent id without a controlled entity is the feature's single most dangerous
detail, and it gets its own fix and its own acceptance criterion.** `requirements.md:46-53` avoids
`CanGlobalFastTravel` for the reason "a player awaiting respawn has no controlled entity". That reasoning is
probably wrong — a dead player still controls their corpse, which is exactly why
`OVT_SpawnLogic.CreateFreshCharacter:323-328` and `RetryCreateCharacter:257-260` both go out of their way to
tolerate a _dead_ controlled entity — but the **conclusion is right for better reasons**: the minimum-distance
rule (`OVT_FastTravelService.c:94-96`), the wanted rule (`:99-104`) and the entire fare model are measured from
a living player's current position and are meaningless for a respawn. So: **do not call
`CanGlobalFastTravel`** — and equally, **do not rely on the corpse**. Whether the engine keeps a dead player
possessing their corpse for the whole time the screen is up is not something this feature should bet on, and
the failure is silent: `GetCurrentPlayerID()` returns `""`, `OVT_MapLocationHouse.PopulateLocations` returns
early by design, `OVT_MapLocationCamp` filters every private camp out, and `OVT_Global.GetController()` returns
null so the buttons do nothing. The result is a correct-looking screen with an empty map and dead buttons and
no error anywhere. Fix: one `OVT_Global.GetLocalPersistentId()` built on
`SCR_PlayerController.GetLocalPlayerId()` → `GetPersistentIDFromPlayerID`, plus the same fallback inside
`GetController()`, plus routing the three duplicated `GetCurrentPlayerID()` bodies through it. See DoD **I-5**.

**K4 (D-1) — The screen is a vanilla SPAWNSCREEN map, not a mode on the fullscreen map.** Modelled on
`SCR_DeployMenuBase.InitMapDeploy` (`ArmaReforger/.../SCR_DeployMenuBase.c:670-681`):
`SetupMapConfig(EMapEntityMode.SPAWNSCREEN, configPath, rootWidget)` then `OpenMap(config)`. This needs **no
map gadget and no live character**, which matters because a dead player cannot raise a gadget. It also buys a
property the fullscreen map cannot give: `SCR_MapEntity.OpenMap` installs the life-state-changed and
player-deleted hooks **only** for `EMapEntityMode.FULLSCREEN` (`SCR_MapEntity.c:298-307`), so a SPAWNSCREEN map
is not torn down by the engine when the player dies or their entity is deleted — which is precisely the state
this screen exists to sit in. `SCR_MapUIElementContainer` already reads `m_bIsDeployMap` from the same enum
(`:153`). Two consequences to hold on to: `Configs/Map/MapRespawn.conf` is a **new file with a new GUID and is
not a same-GUID delta** (unlike `MapOverthrow.conf`), so it inherits nothing and must list every module and
layer config it wants; and `SetupMapConfig` early-returns the cached active config when the mode already
matches (`:471-475`), which is fine while Overthrow has exactly one SPAWNSCREEN config and must be recorded as
a constraint on adding a second.

**K5 — The four eligible types are reused as configured; no respawn-specific subclasses.**
`OVT_MapLocationType` is a `ScriptAndConfig` instantiated **per config entry**, so the same class appears in
both `OverthrowMap.conf` and `OverthrowMapRespawn.conf` with different attribute values — which is how
`m_fVisibilityZoom 0` and `m_bRespawnOnly 1` apply to the respawn map alone. What the types need in _code_ is
one new virtual (`CanRespawn`) and one gate inside the base `ShouldShowLocation`; neither can be expressed as
an attribute, and both are additive. Four subclasses carrying a one-line override each would be four more
files, four more config GUIDs and four more places for the two maps to drift. The only class that _is_
subclassed is the container — `OVT_RespawnMapUI` — because "Respawn here" versus "Fast Travel" is genuinely
different behaviour, and a subclass keeps that difference out of the living map's hot path entirely.

**K6 — The zoom-gate bypass is achieved in config, not by branching `SetVisible`.** `requirements.md:59`
asks for the `currentZoom >= m_fVisibilityZoom` test in `OVT_MapLocationElement.SetVisible:484-491` to be
"bypassed in this mode". It does not need to be: `m_fVisibilityZoom` is a **per-instance attribute**, and the
respawn config sets it to `0` on all four types, which makes `zoomVisible` unconditionally true through the
unmodified code. That is strictly better than a branch — `SetVisible` runs per element on every zoom change,
it is one of the two documented hot paths on this contract, and a respawn-mode branch there would mean the
living map paying for a screen it never shows. It also means there is no mode flag to leave armed (see K7).
The eligibility filter that _does_ need code lives in `ShouldShowLocation`, which the same call already
consults — so the whole feature adds exactly one boolean test to that path, taken only when the config asks
for it.

**K7 — Ineligible locations are not drawn at all (D-2), and the only remaining "why can't I?" case is a
race.** The respawn config declares only the four eligible types, and within them `m_bRespawnOnly` filters to
records that pass `CanRespawn`. Nothing unusable reaches the screen, so `requirements.md:28`'s "selecting an
ineligible one must say why" has no steady-state case left. What remains is the race: a base falls, a camp is
made private, or a QRF starts between the panel opening and the request landing. The server re-derives the
eligible set on arrival, finds no match, and **falls back to home rather than failing the spawn**, reporting
`OK_FELL_BACK_HOME` so the client can say so. That is the requirements' own stated fallback, applied to the
only case that can still occur.

**K8 — There is no mode flag, and BUG-069's defect class cannot recur here.** BUG-069 was a runtime boolean
(bus mode) that survived an engine-side map close and charged a fare on the next click. This feature has no
equivalent: `m_bRespawnOnly` is a **config attribute on a different config file's instances**, not runtime
state — a second map opened from the same session instantiates the fullscreen config's own type objects, with
their own attribute values. There is exactly one piece of per-session state anywhere in the feature, the
server's `m_aAwaitingRespawn`, and it is _server-side_, keyed by player, consumed on use, and cleared on
disconnect; a client cannot leave it set and cannot read it. The client's screen state is the map's own
open/closed flag, whose teardown is the engine's. And no Overthrow context subscribes to a static
`SCR_MapEntity.GetOn*` invoker — `map/legacy-retirement` closed that structurally and this feature keeps it
closed by polling instead.

**K9 (D-4) — The server structure mirrors `map/fast-travel` exactly, with one deliberate improvement.**
`OVT_RespawnService` copies `OVT_FastTravelService`'s shape and its stated invariant: nothing reachable from
the server resolves the acting player from the machine, because `GetLocalControlledEntity()` is null on a
dedicated server and is the **host's** character on a listen server. `OVT_RespawnRequestComponent` copies
`OVT_TravelRequestComponent` line for line, including `ResolveOwningPlayerId()`, the `RplRcver.Server` ask /
`RplRcver.Owner` result split, the listen-server short-circuit and the arrival/result `Print` pair that lets a
play-test tell "the request never left the client" from "a rule refused". The improvement: fast travel
validates a client-sent vector against rules and then teleports **to that vector**; respawn validates a
client-sent vector against the server's own enumeration and then spawns **at the server's vector**. The client
names a place, it does not supply a coordinate. `MATCH_TOLERANCE` exists only to absorb float transport.

**K10 (D-3) — No timeout, an unbounded re-ask, and a capability check at t=0.** The screen waits indefinitely
for a choice; nobody is ever spawned somewhere they did not pick. The exits from awaiting, exhaustively:
(1) the player picks a location; (2) the player picks home; (3) a pick arrives for a location that has become
ineligible and falls back to home; (4) the player disconnects, and `OnPlayerDisconnected_S` drops the entry so
reconnect takes the normal spawn path; (5) something else gives the player a **living** character, which the
re-ask tick detects and clears; (6) the world ends (restart or save load), taking the in-memory state with it
and returning the player to the normal join path. That is the complete list. The tension with "a player is
never left without a character" is resolved two ways rather than by a timer: the show request is **re-sent
every 5 s while awaiting**, so a dropped RPC or a JIP client that was not ready still gets the screen; and if
the player's controller has no `OVT_RespawnRequestComponent` **at the moment of death**, the server logs an
ERROR and spawns them immediately — a prefab misconfiguration degrades to today's behaviour instead of
bricking the session. That check is a capability test at t=0, not a timeout, and it never fires in a correctly
built session.

**K11 — The chosen position is passed as a parameter, never stored.** The obvious defect in a deferred spawn
is a stale override leaking into a _non_-death path — initial spawn, `RetryCreateCharacter`, the body-spawn
timeout, the two `OnPlayerBodySpawned` failure fallbacks. A member field or a `map<int, vector>` makes that a
lifetime question. Splitting `CreateFreshCharacter` into a two-argument wrapper and a four-argument
`CreateFreshCharacterAt(playerId, persId, useChosenPosition, chosenPosition)` makes it a **type** question:
the four existing call sites keep calling the two-argument form, which passes `false`, and there is no state
for anything to leak from. `CompleteRespawn` also calls the fresh-character path directly rather than
`CreateCharacter`, because death has already cleared `m_sBodyPersistenceId` and bypassing the persisted-body
route removes any interaction between the respawn choice and body restoration.

**K12 — "Respawn at home" is implemented by _not_ changing anything.** `requirements.md:65` requires the
existing home chain — `GetHome` → assign a starting house → `SpawnPlayerAtFallbackPosition` at a bus stop —
not to be regressed. The cheapest guarantee is to route the HOME destination through the untouched
`CreateFreshCharacter(playerId, persId)`, whose `GetCreationPosition:768-813` already implements that chain
including the `IsSpawnLocationSafe` check and `FindSafeSpawnLocation`. Home therefore also needs **no QRF
exemption code**: it never goes through `CanRespawn` at all, which is what makes "always available, even
inside an active QRF" true by construction rather than by a special case.

**K13 — `OVT_MapContext.HideMap()` is not needed by this feature, and the honest thing is to say so.**
`map/legacy-retirement` retained that method with zero callers, explicitly "as public API for map/respawn"
(`OVT_MapContext.c:44-46`), because its `ToggleFocused(false)`-then-stow ordering came from play-test rather
than any gate. The SPAWNSCREEN screen never involves the map **gadget**: it is opened with
`SCR_MapEntity.OpenMap` and closed with `CloseMap`, and if the player had the gadget map open when they died,
the FULLSCREEN life-state hook already closed it. So the retained method has turned out to have no consumer.
Recommendation, in scope: **keep the method, correct its comment** — one zero-risk edit that stops the doc
claiming a consumer that does not exist, while preserving the play-test-derived rationale. Deleting it is
_not_ in scope: the identical ordering also lives in the live `OVT_OverthrowMapUI.HideMap`/`StowMapGadget`
pair, so the knowledge is not at risk, and a deletion belongs in a cleanup pass with its own grep proof. File
it as a candidate, do not action it here.

**K14 — Manager access (T1): new code uses idiom A, and `OVT_MapLocationBase` is left alone.** The epic's
standing rule is that T1 is fixed _opportunistically_ — "any type touched switches to the inherited manager
cache", with `Base`/`RadioTower` off-limits unless touched. This feature touches `OVT_MapLocationBase.c` (it
adds `CanRespawn`), but the file's shadowing `m_OccupyingFactionManager` serves only `PopulateLocations`, which
is not edited. New code uses the inherited cache from `Init()`; the existing shadow stays. Rewriting a working
populate path for consistency is not worth adding surface to the one feature whose blast radius is the spawn
system. T1 remains open and un-widened.

---

## 6. Definition of Done

Written so an evaluator with no implementation context can verify each item.

### Functional

**F-1 — Death produces a screen, not a spawn.** Kill your own character in single player. Within ~1 second a
full-screen respawn map appears. **No character is created** until you choose — verify by leaving the screen
untouched for 60 seconds and confirming you are still on it.

**F-2 — Eligible markers are drawn, at every zoom level.** With at least one resistance-held base, one FOB,
one public camp and one house you own in the world, all four appear on the respawn map **fully zoomed out**,
and are still there at maximum zoom in. No zooming is required to find a spawn point.

**F-3 — Ineligible locations are absent, not greyed.** Each of the following is verified **not drawn at all**:

- [ ] an enemy-held base (`isOccupying`)
- [ ] another player's private camp
- [ ] a house you neither own nor rent
- [ ] any town, shop, gun dealer, warehouse, port, radio tower, bus stop, vehicle, waypoint or POI —
      the respawn map declares only four types
- [ ] any otherwise-eligible location within 750 m of an active QRF

**F-4 — Picking a marker spawns you there.** Select an eligible marker, press "Respawn here". You spawn at
that location with the civilian loadout, the screen closes, and the HUD returns.

**F-5 — "Respawn at home" is always present and always works.** The button is visible and operable from the
moment the screen appears, with nothing selected. It works when every other option is unavailable, and it
works **while a QRF is active on top of your home** — start a QRF at your home, die, press it, and confirm you
spawn at home.

**F-6 — No travel affordances on this screen.** The info panel for a selected marker shows **no** fast-travel
button, **no** cost label, **no** refusal-reason text and **no** distance row. The `OverthrowFastTravel` and
`OverthrowToggleRecruits` keybinds do nothing on this screen.

**F-7 — The screen cannot be dismissed into a live world.** With the screen up, each of these leaves it up and
does not spawn you:

- [ ] `Esc`
- [ ] the map-gadget key
- [ ] `MenuBack` / `gamepad0:b` (this dismisses the _info panel_, never the screen)
- [ ] the main-menu key
- [ ] clicking empty map (unpins the panel only)

**F-8 — The living fullscreen map is unchanged.** Open the normal map from the gadget: all ten location types
draw with their configured zoom thresholds, fast travel and bus travel work, the recruit toggle works, and no
"Respawn here" control appears anywhere.

### Quality

**Q-1 — No player is ever left without a character.** Every exit from the awaiting-respawn state terminates in
a spawn or a disconnect. Verified by exercising all six (K10) and confirming that in no case does a connected
player end up alive-less with no screen.

**Q-2 — The awaiting state is torn down on every exit path, including disconnect.** After each of: a normal
pick, a fallback-to-home pick, a disconnect, and an externally-granted character, the server log shows the
`m_aAwaitingRespawn` entry consumed and the 5 s re-ask chain stopped. **Why BUG-069 cannot recur:** the state
is server-side and per-player, not a client mode flag; the only client-visible "mode" is the map's own
open/closed flag whose teardown is the engine's; and `m_bRespawnOnly` is a config attribute on a separate
config file's type instances, so nothing about the respawn map can be left armed on the living map. Confirm
`grep -rn "SCR_MapEntity.GetOn" Scripts/` is still empty.

**Q-3 — Server refusal always produces an on-screen reason, never silence.** Every result code, including a
successful one, sends `RpcDo_RespawnResult`. Force a fallback (see I-2) and confirm the client shows
`#OVT-Respawn_FellBackHome` — not nothing.

**Q-4 — Diagnostics distinguish "the request never left the client" from "a rule refused".** With the map open,
press "Respawn here" and confirm both the arrival `Print` and the result `Print` appear in the server log with
matching player ids. This is the BUG-090 mitigation: `Rpc()`'s untyped variadic prototype means a wrong
argument count compiles clean and dies at the wire.

**Q-5 — Every new layout↔code name is audited.** For each of `RespawnButton`, `RespawnHomeButton`, `MapFrame`
and `MapWidget`, the name is grepped in both the layout that defines it and the code that reads it, and both
are shown. `FindAnyWidget` returning null is a silent no-op the compiler cannot catch (BUG-133/BUG-134).

**Q-6 — No `file:line` pointers in new code comments.** New comments name symbols, not line numbers (epic
K-9 discipline). `docs/` citations are exempt and expected.

### Integration

**I-1 — A client cannot spawn itself at an arbitrary position.** The server ignores the client's vector except
as a _lookup key_: it re-derives the eligible set from its own managers and spawns at **its** recorded
position. Verified by inspection of `ResolveRespawnPosition` (it must return a vector taken from
`CollectEligiblePositions`, never `requestedPos`) and, at runtime, by confirming a request with a nonsense
vector spawns at home rather than at the nonsense vector.

**I-2 — Eligibility is re-evaluated on the server and falls back to home rather than failing.** With the
respawn screen open, have a second player (or an admin command) flip the chosen location ineligible — capture
the base, make the camp private, or start a QRF on it — then press "Respawn here". You spawn **at home**, alive,
and see `#OVT-Respawn_FellBackHome`. You are never left dead.

**I-3 — The request lands on `OVT_OverthrowController`, not `OVT_PlayerCommsComponent`.**
`grep -n "Respawn" Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` returns nothing, and
`Prefabs/GameMode/OVT_OverthrowController.et` lists `OVT_RespawnRequestComponent`.

**I-4 — `map/location-types`' house-privacy isolation still holds.** On two clients: player A's owned houses,
rented houses, private camps and vehicles are **not** drawn on player B's living map or B's respawn map, and
vice versa. This is the N1 fix and this feature changes the identity resolver it depends on, so it must be
re-proven, not assumed.

**I-5 — The persistent id resolves with no controlled entity.** With the respawn screen open on a
**dedicated-server client**, the map draws the local player's own houses and their own private camps. An empty
map here means `GetLocalPersistentId()` failed and every ownership check silently refused — this is the
single most likely way the feature ships looking correct and doing nothing, and it is why this is its own
criterion rather than a line in F-2.

**I-6 — `ChargeRespawn` is unchanged.** `git diff` shows no edit to `OVT_EconomyManagerComponent.ChargeRespawn`
and no edit to the `ChargeRespawn` call site in `OnPlayerKilled_S`. At runtime, dying with more than $500 debits
`m_Difficulty.respawnCost` **once**, at death, regardless of which location is later chosen — and the same
amount whichever is chosen.

### Verification Method

Run in order. Stop and fix at the first failure.

**V-1 — Compile.** `tools/compile-check.sh` → exit **0**. File count is the baseline **5958** plus the number
of new `.c` files. Any other delta is a finding.

**V-2 — Automated tests, against the captured baselines.**

- `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0**, **44 + N** tests (baseline 44).
- `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0**, **79 + N** tests (baseline 79).
- `N` is the number of Logic cases added in Phase 3, and it must be the **same** `N` in both. A change
  anywhere else — especially Campaign or Persistence — is a finding to investigate, never a number to update.
- Each new case has been demonstrated able to fail (invert, observe exit 1, revert), with the method recorded.

**V-3 — Workbench load.** Open the project in Workbench and confirm a clean load with **no missing-resource,
unknown-class or dangling-GUID errors**. This is the only gate that can see
`Configs/Map/MapRespawn.conf`, `Configs/Map/OverthrowMapRespawn.conf`,
`UI/Layouts/Respawn/OVT_RespawnScreen.layout`, `UI/Layouts/Map/Core/OVT_MapInfoPanelRespawn.layout` and the
three edited `.et` prefabs — all six file classes are invisible to V-1 and V-2. Then open each new `.conf` and
`.layout` and confirm every referenced GUID resolves.

> Note the known pre-existing orphaned `.meta` for the retained `OVT_MapThreatGrid` (`{B8F4C6A8C9D3E4F1}`) —
> if a GUID error names that, it is not this feature.

**V-4 — Single-player pass.** Run F-1 … F-8, I-1, I-2, I-6 and Q-1 … Q-4 in a single-player campaign with at
least one resistance base, one FOB, one public camp, one private camp not yours, one owned house and one rented
house in the world.

**V-5 — Two-client multiplayer.**

> ⚠️ Warn the user before launching: each client opens a window on their desktop. **Always pass a long
> `--timeout`** — it defaults to 600 s and will kill the client mid-play-test.

1. `tools/launch-server.sh`
2. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
3. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001`

- [ ] **Simultaneous respawn.** Both players die within a second of each other. Both get their own screen.
      Both pick **different** locations at the same time. Each spawns at their own choice; neither spawns at
      the other's; neither gets two characters.
- [ ] **JIP death.** Client 2 joins a campaign with accumulated state and dies within ~10 s of spawning. The
      screen appears (possibly on a re-ask tick — the log shows how many), the map is populated, and the pick
      works.
- [ ] **I-4 privacy.** Client 1's houses, rented houses, private camps and vehicles are absent from client 2's
      living map **and** respawn map, both ways.
- [ ] **I-5 identity.** On a client (not the host), the respawn map draws that client's own houses and private
      camps.
- [ ] **I-2 race.** Client 1 opens the respawn screen on a camp; client 2 makes that camp private (or the
      base is captured); client 1 presses "Respawn here" and lands at home with the fallback message.
- [ ] **Listen-server host path.** Repeat F-1/F-4/F-5 as the **host** of a listen server — this is the
      short-circuit branch that fast travel left untested, and it is the one where an owner-targeted RPC to
      yourself silently does nothing.

**V-6 — Disconnect while awaiting.** Die on a client, and with the respawn screen up, quit to the main menu.
The server log shows `OnPlayerDisconnected_S` dropping the awaiting entry and the re-ask chain stopping.
Reconnect: you spawn at home through the normal join path, alive, with no respawn screen.

**V-7 — Gamepad-only pass, no mouse.** Unplug or ignore the mouse entirely. Die, then:

- [ ] pan and zoom the respawn map
- [ ] move the cursor to a marker and select it
- [ ] press "Respawn here" and spawn there
- [ ] die again, and press "Respawn at home" without selecting anything
- [ ] dismiss the info panel and re-select a different marker
- [ ] confirm every on-screen button shows a controller glyph, and that no action on this screen is bound to
      an input with no gamepad source

**V-8 — Localization.** Ask the user to regenerate the six `localization_Overthrow.<lang>.conf` exports in
Workbench, then confirm no raw `#OVT-Respawn_*` key renders on screen. ❌ Those six files are never hand-edited.

---

## 7. Testing Strategy

**Automated — a thin spine, and honest about it.** Baselines captured 2026-08-10: compile **exit 0 / 5958
files**, Fast **44**, All **79**. Both groups must stay green at every phase boundary. Neither group can see
**any** of this feature's runtime behaviour: the autotest world has no players, so `OnPlayerKilled_S` never
fires; there is no UI tier; and `.conf`, `.layout`, `.et` and `.st` edits are invisible to both groups **and**
to `compile-check.sh`. Treat the automated gates as a regression guard on everything else, not as evidence
this feature works.

**The pure rule predicates are genuinely worth asserting, and are the only part that can be.** Ten cases in
`OVT_TEST_Logic_RespawnRules.c` (Phase 3 task 5) over `IsBaseEligible`, `IsCampEligible`, `IsHouseEligible`,
`IsInsideQrf`, `PositionsMatch` and `ReasonKeyFor`. The tier rule is absolute — **no manager, no game mode, no
world**, and the manager accessor's identifier must not appear anywhere in the file including comments,
because a reviewer grep over `TestSuites/Logic/` does not distinguish code from prose. This is precisely why
`OVT_RespawnService` splits its pure predicates from its manager-reading enumeration: the split is not
aesthetic, it is what makes any of this testable. Case 6 (`IsHouseEligible("", "", "")` must be **false**) is
the one that pins K3's failure mode — an unresolved player id must never match an unowned record.
**Prove every case can fail before shipping it** and record the method. ❌ **No `maxAttempts`** — a test that
needs retries is a bug in the test.

**Manual — the real gate**, and for this feature it is most of the gate.

| #   | Scenario                                          | Expected                                                         |
| --- | ------------------------------------------------- | ---------------------------------------------------------------- |
| 1   | Die in SP                                         | Screen within ~1 s; no character created                         |
| 2   | Wait 60 s on the screen                           | Still on the screen, still dead, no timeout spawn                |
| 3   | Fully zoomed out                                  | All four eligible types visible                                  |
| 4   | Enemy base / foreign private camp / foreign house | Not drawn at all                                                 |
| 5   | QRF active over an eligible camp                  | That camp not drawn                                              |
| 6   | QRF active over **home**                          | "Respawn at home" still works                                    |
| 7   | Pick a marker                                     | Spawn there, civilian loadout, screen closes                     |
| 8   | Pick home with nothing selected                   | Spawn at home                                                    |
| 9   | Esc / M / MenuBack / main-menu key                | Screen stays up                                                  |
| 10  | Info panel                                        | No travel button, cost, reason or distance                       |
| 11  | Two clients, simultaneous picks                   | Each spawns at their own choice, one character each              |
| 12  | JIP client dies ~10 s after joining               | Screen appears; log shows re-ask count                           |
| 13  | Location goes ineligible mid-screen               | Spawn at home + `#OVT-Respawn_FellBackHome`                      |
| 14  | Disconnect while awaiting, reconnect              | Spawn at home via the normal path; entry dropped                 |
| 15  | Listen-server **host** dies                       | Screen appears and the pick works (short-circuit branch)         |
| 16  | Gamepad only, no mouse                            | Full pass, every button glyphed                                  |
| 17  | Die with > $500                                   | `respawnCost` debited once, at death, same for every destination |
| 18  | Living fullscreen map after all of the above      | Unchanged: ten types, travel, bus, recruit toggle                |

**Debugging.** Three causes account for nearly every silent failure here, and each has a distinguishing
signature: the request never left the client (**neither** `Print` fires — suspect a missing component on
`OVT_OverthrowController.et`, or `Rpc()` arity per BUG-090); a rule refused (**both** `Print`s fire with a
non-OK result); or the map is empty because the persistent id is `""` (markers absent for locations you
_know_ you own — add a temporary `Print` of `OVT_Global.GetLocalPersistentId()` in `OVT_RespawnContext.OnShow`).

---

## 8. Dependencies

### Internal (code)

- `map/core` — `OVT_MapLocationType` contract, `OVT_MapLocationElement.SetVisible`, `OVT_OverthrowMapUI`
  panel lifecycle. This feature adds two rows to that contract and must record them.
- `map/location-types` — the four types being picked from, and the **N1 house-privacy** populate-time filter,
  which depends on the identity resolver this feature changes.
- `map/fast-travel` — `OVT_TravelRequestComponent` and `OVT_FastTravelService` are the templates. ❌ Nothing
  here calls `CanGlobalFastTravel`.
- `map/legacy-retirement` — the legacy-free map this is built against; `OVT_MapContext.HideMap()`'s retention
  rationale (K13).
- `core/game-mode` — `OVT_OverthrowGameMode` gains a configured context attribute + accessor; the connect and
  home-assignment fallback chains are read, not changed.
- `core/controller-migration` — the request lands on `OVT_OverthrowController`. ❌ Never
  `OVT_PlayerCommsComponent`.
- `core/player-manager` — `GetPersistentIDFromPlayerID`, `GetController`, `IsCharacterDead`.
- `economy/real-estate` — `GetHome`, `GetOwned`, `GetRented`, `IsHome` (read only).
- `occupying/core` — `m_bQRFActive`, `m_vQRFLocation`, `OVT_QRFControllerComponent.QRF_RANGE` (read only).
- `resistance/fob` — `m_FOBs`, `m_Camps` (read only).

### External — user / Workbench work

| Item                                                            | Blocking?       | Notes                                                                                          |
| --------------------------------------------------------------- | --------------- | ---------------------------------------------------------------------------------------------- |
| Regenerate the six `localization_Overthrow.<lang>.conf` exports | **YES** for V-8 | Only the user does this; `.st` is the editable master                                          |
| Workbench clean-load check (V-3)                                | **YES**         | The only gate that can see the new `.conf`/`.layout`/`.et` files                               |
| Two-client MP session (V-5)                                     | **YES**         | `tools/launch-server.sh` + two `tools/launch-game.sh --profile` clients; warn before launching |
| Gamepad hardware (V-7)                                          | **YES**         | Hard requirement, not polish                                                                   |
| Authoring the two new `.conf` files and two `.layout` files     | No              | Can be hand-written; must be Workbench-verified                                                |

### New and changed files

```
Scripts/Game/
├── Services/
│   └── OVT_RespawnService.c                              NEW  enum + pure rules + server enumeration
├── Components/Controller/
│   └── OVT_RespawnRequestComponent.c                     NEW  3 RPCs on OVT_OverthrowController
├── Components/Player/
│   └── OVT_RespawnScreenHandlerComponent.c               NEW  EOnFrame driver on the player controller
├── UI/Context/
│   └── OVT_RespawnContext.c                              NEW  OVT_UIContext hosting the SPAWNSCREEN map
├── UI/Map/
│   ├── OVT_RespawnMapUI.c                                NEW  OVT_OverthrowMapUI subclass
│   └── OVT_OverthrowMapUI.c                              ~    GetCurrentPlayerID delegates
├── UI/Map/Core/
│   ├── OVT_MapLocationType.c                             ~    CanRespawn, m_bRespawnOnly, ShouldShowLocation gate
│   └── OVT_MapLocationElement.c                          ~    GetCurrentPlayerID delegates
├── UI/Map/LocationTypes/
│   ├── OVT_MapLocationBase.c                             ~    CanRespawn
│   ├── OVT_MapLocationFOB.c                              ~    CanRespawn
│   ├── OVT_MapLocationCamp.c                             ~    CanRespawn
│   └── OVT_MapLocationHouse.c                            ~    CanRespawn
├── Respawn/Logic/
│   └── OVT_SpawnLogic.c                                  ~    awaiting state, deferral, CreateFreshCharacterAt,
│                                                              OnPlayerDisconnected_S
├── Global/
│   └── OVT_Global.c                                      ~    GetLocalPersistentId, GetRespawnRequests,
│                                                              GetController fallback
├── GameMode/
│   └── OVT_OverthrowGameMode.c                           ~    m_RespawnUIContext + GetRespawnContext
└── Tests/TestSuites/Logic/
    ├── OVT_TEST_Logic_RespawnRules.c                     NEW  ~10 world-free cases
    └── OVT_TEST_LogicSuite.c                             ~    register the new case class

Configs/
├── Map/MapRespawn.conf                                   NEW  SCR_MapConfig, m_iMapMode SPAWNSCREEN
├── Map/OverthrowMapRespawn.conf                          NEW  four types, zoom 0, m_bRespawnOnly 1
└── System/chimeraInputCommon.conf                        ~    2 actions + OverthrowRespawnContext

UI/Layouts/
├── Respawn/OVT_RespawnScreen.layout                      NEW  embeds UI/layouts/Map/Map.layout
└── Map/Core/OVT_MapInfoPanelRespawn.layout               NEW  RespawnButton, no travel widgets

Prefabs/
├── GameMode/OVT_OverthrowController.et                   ~    + OVT_RespawnRequestComponent
├── GameMode/OVT_OverthrowGameMode.et                     ~    + m_RespawnUIContext config
└── Characters/Core/OVT_PlayerController.et               ~    + OVT_RespawnScreenHandlerComponent

Language/localization_Overthrow.st                        ~    11 new ids (master only)

docs/features/map/
├── core/context.md                                       ~    2 contract rows + 3 layout-name rows
├── epic-overview.md                                      ~    feature 5 status + rollup
└── respawn/context.md                                    NEW
```

> Allocate a **fresh contiguous GUID block** for the new `.conf`, `.layout` and prefab-component entries (the
> map epic's recent series is `{6A7E1C4D…}`; travel used `{6A7F3E5C…}`). Before committing, `grep -rn` each
> new GUID across the repo and confirm it is unique — a duplicate GUID is a Workbench-only failure.

---

## 9. Risks & Mitigation

| #       | Risk                                                                                                                                                                                                                                                                             | Likelihood      | Impact | Mitigation                                                                                                                                                                                                                                                                                                                                                                                              |
| ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **R1**  | **A workspace-hosted SPAWNSCREEN map gets no input.** Vanilla only ever opens a SPAWNSCREEN map from inside a `ChimeraMenuBase`; Overthrow has no `ChimeraMenuBase` menus at all. If `MapContext` is not activated for a workspace layout, the map renders and cannot be driven. | Medium          | High   | **Phase 1 is a spike and comes first**, before any server change. Success criterion is explicit (pan, zoom, marker hover, `MapSelect` on mouse _and_ gamepad). Recorded fallback: host the screen in a `ChimeraMenuBase` modelled on `SCR_DeployMenuBase` and keep `OVT_RespawnContext` as the state machine only. Deciding this on day one is worth more than any other sequencing choice in the plan. |
| **R2**  | **The map draws nothing because the persistent id is `""`.** `OVT_MapLocationHouse` returns early on an empty id _by design_ (the N1 fail-closed contract) and `OVT_MapLocationCamp` filters every private camp out. The screen looks correct and is empty, with no error.       | High if unfixed | High   | K3's identity fix, its own DoD criterion **I-5**, a dedicated V-5 step on a real client, and Logic case 6 pinning `IsHouseEligible("", "", "")` false. Add a temporary `Print` of the resolved id in `OVT_RespawnContext.OnShow` during bring-up.                                                                                                                                                       |
| **R3**  | **A player is left dead with no screen** — the show RPC is lost, or a JIP client's controller has not registered yet.                                                                                                                                                            | Medium          | High   | Unbounded 5 s re-ask while awaiting (K10), self-cancelling on entry removal, player-gone or living-character. Plus the t=0 capability check that degrades to today's immediate spawn when the component is missing. V-5's JIP step and V-6 exercise both.                                                                                                                                               |
| **R4**  | **A stale chosen-position override leaks into a non-death spawn** — initial spawn, `RetryCreateCharacter`, the body-spawn timeout, or either `OnPlayerBodySpawned` fallback lands the player at somebody's old respawn pick.                                                     | Medium          | High   | K11: the position is a **parameter**, never a member and never a map. The four existing callers keep the two-argument form and are not edited, so there is no state for anything to leak from. Reviewed as a diff, not as behaviour.                                                                                                                                                                    |
| **R5**  | **The widened corpse window breaks a guard.** Deferral stretches "the controller still references the corpse" from one frame to minutes; several guards in `OVT_SpawnLogic` test `GetControlledEntity()` and two do so _without_ the dead check.                                 | Medium          | High   | Phase 6 task 8 is an explicit audit of all five sites, with the reasoning written down rather than assumed. Phase 6 is routed to `component-developer-advanced`.                                                                                                                                                                                                                                        |
| **R6**  | **Two characters for one death.** A duplicate or late `RpcAsk_Respawn`, or a re-ask racing a pick.                                                                                                                                                                               | Medium          | High   | `CompleteRespawn` consumes the `m_aAwaitingRespawn` entry first and returns without spawning when it is absent — the same one-shot-claim idiom `m_aPendingBodySpawns` already uses in the same file. Verified explicitly in Phase 6 acceptance and V-5's simultaneous-respawn step.                                                                                                                     |
| **R7**  | **A client spawns itself at an arbitrary position** by sending a crafted vector.                                                                                                                                                                                                 | Low             | High   | The server never uses `requestedPos` as a position — only as a lookup key against its own `CollectEligiblePositions`, with a 2 m tolerance, spawning at _its_ recorded vector. No match ⇒ home. DoD **I-1** verifies both the code shape and the runtime behaviour.                                                                                                                                     |
| **R8**  | **Gamepad has no free binding.** `MapContext` is active on this screen and Overthrow has already spent `KC_SPACE`/`pad_right`, `KC_R`, `KC_C`/`gamepad0:b` in it; `mouse:button0`/`gamepad0:a` belong to `MapSelect`.                                                            | Medium          | High   | Phase 7 proposes `gamepad0:x` and `gamepad0:y`, which no `MapContext` action claims. The buttons are also reachable via cursor + `MapSelect`, which is the path that always works. Cross-check by hand — the repo's input-conflict checker cannot see inline `ActionContext` actions. V-7 is a hard gate.                                                                                               |
| **R9**  | **A `.conf`/`.layout`/`.et` fault passes every automated gate.** This feature adds two configs, two layouts and three prefab edits — six file classes invisible to `compile-check.sh` and to both test groups. A dangling GUID or a mistyped widget name ships dead.             | High            | Medium | V-3's Workbench load is mandatory and is the _only_ evidence those files are sound. Q-5's name-by-name audit covers the widget half. Allocate and grep-verify fresh GUIDs.                                                                                                                                                                                                                              |
| **R10** | **`MapRespawn.conf` is not a delta and silently loses vanilla map machinery.** `MapOverthrow.conf` is a same-GUID delta over vanilla's and inherits everything; a brand-new config inherits nothing.                                                                             | Medium          | Medium | Phase 1 task 2 requires listing every module, layer and props config `MapSpawnMenu.conf` carries. The spike's pan/zoom/cursor criteria are what catch an omission.                                                                                                                                                                                                                                      |
| **R11** | **`SetupMapConfig`'s cache returns the wrong config** if a second SPAWNSCREEN config is ever added — it early-returns the currently active config when the mode already matches.                                                                                                 | Low             | Medium | Recorded as a constraint in Phase 1 task 6 and in `respawn/context.md`: Overthrow has exactly one SPAWNSCREEN config, and adding a second requires revisiting this.                                                                                                                                                                                                                                     |
| **R12** | **The refresh timer destroys markers under a screen the player cannot leave** (BUG-136's reconciliation path).                                                                                                                                                                   | Low             | Medium | `m_fRefreshInterval 0` on all four respawn types, with the reason recorded in the config comment. The server's arrival re-validation makes staleness cost a fallback-to-home, not a wrong spawn.                                                                                                                                                                                                        |
| **R13** | **`OVT_MapContext.HideMap()` gets deleted as an orphan** once this feature proves it does not want it, taking play-test-derived ordering with it.                                                                                                                                | Low             | Low    | K13: keep the method, correct its comment in Phase 8. Note the identical ordering also lives in the live `OVT_OverthrowMapUI.HideMap`/`StowMapGadget` pair, so the knowledge is not single-sourced. Deletion is a separate cleanup with its own grep proof.                                                                                                                                             |
| **R14** | **Parallel development** — other sessions commit to this tree mid-feature (the epic has a history of it).                                                                                                                                                                        | Medium          | Low    | Re-check `git status` and the highest BUG id at each phase boundary; commit per phase so there is a revert path.                                                                                                                                                                                                                                                                                        |

---

_Plan created 2026-08-10 by `/plan-feature map/respawn`. Baselines in §4 Phase 0 were measured, not quoted:
compile exit 0 / 5958 files, Fast 44, All 79. Three premises in `requirements.md` are corrected in §5 K1–K3;
that file remains the record of user intent and is deliberately not edited._
