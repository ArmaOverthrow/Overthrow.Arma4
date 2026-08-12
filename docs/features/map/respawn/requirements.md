# Map Respawn — Requirements

**Epic:** map
**Created:** 2026-08-10
**Position:** Feature 5 — first feature after `legacy-retirement`, ahead of the three stretch goals

## Overview

This feature replaces Overthrow's fixed "always respawn at home" with a **player choice made on a map**, in the manner of the base game's Conflict mode: on death the player is shown a dedicated respawn screen listing every location they are currently entitled to spawn at, and picks one. **Home is retained as a guaranteed option** — a "Respawn at home" action is always available, even when home would not otherwise qualify (for example when it sits inside an active QRF).

Today the choice does not exist. `OVT_PersistentRespawnLogic.OnPlayerSpawnRequest_S` (`Scripts/Game/Respawn/Logic/OVT_PersistentRespawnLogic.c:132-138`) resolves the player's home and spawns them there unconditionally:

```
vector home = OVT_Global.GetRealEstate().GetHome(persId);
SCR_FreeSpawnData data = new SCR_FreeSpawnData(m_PlayerPrefab, home, "0 0 0");
if (GetPlayerRespawnComponent_S(playerId).CanSpawn(data)) DoSpawn(playerId, data);
else OnPlayerSpawnFailed_S(playerId);
```

Two properties of that code make this feature cheaper than it looks. It is already **server-side** (`_S`, going through `GetPlayerRespawnComponent_S().RequestSpawn`), so the authority model is right and only the *source of the position* changes. And it is a **free-position spawn** (`SCR_FreeSpawnData`) rather than a `SCR_SpawnPoint` one, so this is "let the player choose a vector the server then validates" — Overthrow does not need to adopt vanilla's spawn-point system to get a Conflict-style picker.

## Requirements

### The respawn screen

- Present a **dedicated respawn screen** that owns the map while the player is awaiting respawn — not a mode toggled on the normal fullscreen map. Overthrow already has a precedent for a pre-spawn screen driving a context: `OVT_PlayerStartMenuHandlerComponent` + `OVT_StartGameContext`.
- **"Respawn at home" must be present at all times**, independent of any map selection, and must work even when home is not otherwise eligible (see QRF below).
- Selecting an eligible location offers **"Respawn here"**. Selecting an ineligible one must say why rather than silently doing nothing.
- The player must not be able to dismiss the screen into a live world without having spawned; and the screen must not be able to leave the player stranded with no option — "Respawn at home" is the guaranteed floor.
- **Fast-travel actions must not appear** on this screen. The info panel's travel button, cost label and reason text belong to the living map, not the respawn map.

### Eligibility — a distinct rule set, deliberately not a call into fast travel

- A location is respawn-eligible when it passes **the same per-type ownership and control gate** that fast travel uses, and is **not inside an active QRF**:

  | Type | Eligible when |
  |---|---|
  | Base | held by the resistance (not `isOccupying`) |
  | FOB | always |
  | Camp | yours, or public (not `isPrivate` unless `owner == playerID`) |
  | House | you own or rent it |
  | Town, and everything else | never |

  This set is exactly the four types currently configured with `m_bCanFastTravel 1`, which keeps one mental model for players.
- **Respawning is free.** No cost, no payment path, no affordability check.
- **Do not call `OVT_FastTravelService.CanGlobalFastTravel`.** It opens with

  ```
  ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
  if (!playerEntity) return false;                              // OVT_FastTravelService.c:14-16
  ```

  A player awaiting respawn has **no controlled entity**, so every location would be refused. Its minimum-distance rule (`:19-24`) and its cost model are likewise measured *from the player's current position*, which does not exist. The wanted-level check is meaningless once dead. Respawn therefore needs its **own** eligibility function, sharing the per-type ownership gates but not the global fast-travel rules.
- **QRF exclusion:** a location within `OVT_QRFControllerComponent.QRF_RANGE` of `OVT_OccupyingFactionManager.m_vQRFLocation` while `m_bQRFActive` is not eligible — **except home**, which is always offered. This is the one deliberate asymmetry and it is the point of keeping home: the player can always get back into the world even when the enemy is sitting on everything they own.
- Eligibility must be **re-evaluated on the server** when the request arrives. The screen showing an option is not authority; a client must not be able to spawn at an arbitrary position by driving the UI. If the chosen location has become ineligible between selection and request, fall back to home rather than failing the spawn.

### Map behaviour in respawn mode

- **All eligible locations are visible at every zoom level.** The normal zoom gate — `currentZoom >= m_fVisibilityZoom` in `OVT_MapLocationElement.SetVisible` (`:485-511`) — must be bypassed in this mode so a player never has to hunt for a spawn point by zooming. Ineligible locations should either be hidden or clearly shown as unavailable; decide which during planning, but do not show an eligible-looking icon that cannot be used.
- **Otherwise markers behave exactly as on the normal map** — same icons, colours, names, selection and info panels. This mode changes *which* locations are shown and *what action* the panel offers, not how the map works.
- Whatever mode state drives this must be **torn down on every exit path**, including engine-side closes. Mode flags surviving a close is precisely the defect class of BUG-069 (closed) — where the legacy map left bus mode armed so the next map click charged a fare. The new map system avoids that by having a single teardown; this feature must not reintroduce it.

### Home

- Home must always resolve to a usable position. The existing chain — `GetHome` (`OVT_RealEstateManagerComponent.c:750`), falling back to assigning a starting house, falling back to `SpawnPlayerAtFallbackPosition` at a bus stop (`OVT_OverthrowGameMode.c:917-935`) — already provides this and must not be regressed.
- Setting home stays where it is (the real-estate flow). This feature consumes home; it does not change how it is chosen.

### Verification

- Must work in **multiplayer**, including two players respawning simultaneously and a JIP client dying shortly after joining.
- Must be fully operable **on gamepad/console** — this is a screen a player cannot skip, so a mouse-only picker is not shippable.

## Dependencies

- **`map/core`** — the map UI, element visibility (the zoom gate this mode bypasses) and the info-panel lifecycle.
- **`map/location-types`** — the markers being picked from, and the per-type ownership/control gates the eligibility rule reuses.
- **`map/fast-travel`** — the per-type gates are shared conceptually; this feature must not depend on `CanGlobalFastTravel` itself. Landing after fast-travel's server-authority fixes (F1–F3) means the correct pattern for a server-validated, client-requested position change already exists to copy.
- **`map/legacy-retirement`** — sequenced after it so the respawn mode is added to a map with no legacy modes left.
- **`OVT_PersistentRespawnLogic` / `OVT_SpawnLogic`** (`Scripts/Game/Respawn/Logic/`) — the server-side spawn path whose position source changes. This is the feature's one write outside the map UI.
- **`core/game-mode`** — the connect/respawn flow in `OVT_OverthrowGameMode` and the home-assignment fallback chain.
- **`economy/real-estate`** — `GetHome`, and house ownership/rent state for eligibility.
- **`occupying/core`** — QRF state (`m_bQRFActive`, `m_vQRFLocation`, `QRF_RANGE`) and base control.
- **`resistance/fob`** — FOB and camp records.
- **`core/controller-migration`** — the client→server request must land on `OVT_OverthrowController`, **not** the deprecated `OVT_PlayerCommsComponent`.

## Out of Scope

- **Respawn costs, death penalties, wave timers or respawn tickets.** Respawning is free and immediate; adding an economy to it is a separate design decision.
- **Changing how home is set**, or adding multiple homes.
- **Group/squad spawning** — spawning on a squadmate or a group leader is not included; it belongs with `core/player-groups` if wanted.
- **Adopting vanilla's `SCR_SpawnPoint` system.** Overthrow spawns to free positions; this feature keeps that.
- **Vehicle or loadout selection on the respawn screen** — the existing loadout flow is unchanged.
- **Respawning at bus stops, ports, gun dealers, warehouses or towns** — the eligible set is deliberately the four ownership/control types.
- **Reworking `OVT_StartGameContext`** beyond what is needed to host or model the respawn screen.
