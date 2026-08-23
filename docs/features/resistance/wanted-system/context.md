# Wanted System - Context & Decisions

**Last Updated:** 2026-08-23
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code, shipped; undercover icon since v1.3.0)
- ✅ Retrospective documentation created (2026-08-03)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md — Known Issues filed as **BUG-072…078** on 2026-08-03)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c` — the entire system (810 L): state, 1 Hz tick, detection, disguise, decay, perceived-faction override
- `Scripts/Game/UI/HUD/OVT_WantedInfo.c` + `UI/Layouts/HUD/WantedInfo.layout` — stars / seen-eye / undercover icon (`OVT_WantedInfoWidgets.c` is dead)
- `Scripts/Game/AI/Modded/SCR_ChimeraAIAgent.c`, `SCR_AIRetreatFromTargetBehavior.c`, `Scripts/Game/AI/Components/Modded/SCR_AIConfigComponent.c` — AI consumption + shooting escalation
- `Scripts/Game/Components/Damage/Modded/SCR_CharacterDamageManagerComponent.c` — damage/kill escalation (server-side writers)
- `Scripts/Game/Configuration/OVT_DifficultySettings.c` + `Configs/Difficulty/*.conf` — timeouts/ranges (3 undercover fields are dead)
- Attached on: `Character_Player.et`, recruit prefabs, `Character_CIV.et` (dormant until recruited); HUD also on vehicle/turret base prefabs

---

## Important Decisions

- **Per-character component, no manager** — component *presence* is itself the AI's "player-aligned" marker; attaching it to a prefab changes AI friend/foe behaviour.
- **AI interface = `SetPerceivedFactionOverride`** (disguised→CIV since BUG-170 — presenting OF made recruits target the owner; wanted→player faction, clean→"CIV"); real faction affiliation untouched so vehicles/compartments keep working. Side effect: wanted players leave the FRIENDLY perception bucket — the only bucket the detection scan reads (mitigated by the BUG-072 dual-bucket scan).
- **Owner-machine tick + RplProp broadcast** — player wanted state is effectively client-authoritative while combat escalation writes from the server; the tick likely double-registers (server+client) because ownership transfers after `OnPostInit`.
- **Hybrid detection** — perception target lists gate, manual head→head `TraceMove` confirms, `GetVisualRecognitionFactor()` early-outs (and drives the HUD eye's opacity).
- **Not persisted** — wanted level resets on spawn/load by design; only the never-read `areaHeat` town sink persists.

---

## Gotchas & Learnings

- `m_bWantedSystemEnabled` is declared `float` despite the `m_b` prefix, and `EnableWantedSystem()` never calls `Replication.BumpMe()`.
- `m_bIsDisguised` is **not replicated** but read on both client (HUD) and server (damage manager).
- Detection distance formula: `5 + 250 × stealthMultiplier` (250 attribute never overridden by any prefab); disguise blow range is `disguiseDetectionDistance` (15 m); radio-tower radius is a hardcoded 20, duplicated in `OVT_MapRestrictedAreas.c`.
- Level 1 is decay-only (never set directly); being seen at level 1 jumps to 2 silently, and the decay timer is not reset on escalation.
- `OnPlayerLoot(IEntity)` ignores its parameter — listen-host looting can flag every seen recruit; the invoker subscription leaks (never removed in `OnDelete`).
- Two full `AIWorld.GetAIAgents()` walks per component per second (scan + disguise check); `QueryEntitiesBySphere` alternative sits commented out at the scan site.
- Mobile FOB detection hardcodes the prefab GUID; `"CIV"`/`"FIA"` faction keys are hardcoded fallbacks.
- Test coverage is zero; the pure-logic extraction candidates are the radius formula, the decay state machine, and `CheckEntity`'s level-selection rules.
- The game mode's `SCR_PerceivedFactionManagerComponent` is set to `FULL_OUTFIT` (PR #132, 2025-07-18): perceived faction is all-or-nothing — one worn CIV-scored item (vanilla civilian shirt/trousers score CIV 40) or a missing jacket/pants slot makes it unknown, so no disguise and no inventory faction icon. Under the pre-#132 `HIGHEST_VALUE` the dominant faction won.
- Fixed 2026-08-14: the tick's `InitPlayerOutfitFaction_S` fallback re-broadcast an RPC every second while the outfit had no faction-scored items (BUG-168), and `OVT_WantedInfo` never rebound after death, reading the corpse's wanted/disguise state until the body despawned (BUG-169).
- Fixed 2026-08-15 (BUG-170): a working disguise presented the OCCUPYING faction via `SetPerceivedFactionOverride`, landing the player in their own recruits' native ENEMY perception bucket — armed recruits (fighting again since BUG-146) opened fire on their disguised owner. Disguise now presents CIV; native `AIWeaponTargetSelector` has no script seam, so the override is the only place this can be fixed. The modded `SCR_ChimeraAIAgent.IsPerceivedEnemy`/`IsEnemy` only gate the four scripted danger reactions, not target selection.

---

*This context file was created retrospectively by analyzing existing code.*

---

## Fix 2026-08-23 — mounted players were never "seen", so restricted areas did nothing in a vehicle

User report: *"if im in a vehicle and in a restricted area it doesnt make me wanted"*. Two defects in
`OVT_PlayerWantedComponent.c`, both of which made `m_bTempSeen` unreachable while mounted — and every
restricted-area rule (base proximity, radio tower, the illegal-action window, the level-1 re-sighting
escalation) is gated on it.

1. **`CheckEntity` matched the perception target against the CHARACTER only.** Vanilla perception
   deliberately looks past vehicle occupants — `SCR_AIGroupPerception` skips any target whose
   `PerceivableComponent.IsInCompartment()` with the comment *"we don't care about vehicle
   occupants"* — so what an AI actually holds as you drive past a base is the **vehicle**.
   `realEnt == GetOwner()` therefore never matched, and the `inVehicle` branch already written inside
   that block (the `TraceLOSVehicle` call, the armed-vehicle level 4, the Mobile FOB special case) was
   only ever reachable when an AI happened to still be tracking the character itself. Now the target
   is matched against the occupied vehicle as well.

2. **Every distance was measured from the occupant's own origin.** A mounted character is parented to
   the vehicle, and it is the vehicle the world sees and measures anyway. `GetDetectionOrigin()` is
   now the single source for all of them — the AI sweep, the disguise close-range sweep, the base and
   tower proximity checks and the tutorial's base-range edge — so the mounted and dismounted answers
   cannot diverge.

**Deliberately unchanged:** base proximity still requires being **seen**. Driving through an empty
restricted area is still not a crime, exactly as on foot.

**Gate:** `compile-check.sh` exit 0 (6341 files). No suite covers this path.

**Owed:** a play-test — drive a civilian car into a base ring with AI present and confirm wanted 2
("WantedBaseProximity"); confirm an unarmed car passing an AI on open ground still does **not**.
