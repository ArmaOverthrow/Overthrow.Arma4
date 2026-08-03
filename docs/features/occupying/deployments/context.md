# Deployments - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code; framework complete, migration from BaseUpgrades stalled at one upgrade)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — the `m_mFactionDeployments` leak is the long-campaign kill switch; unset `resourcesInvested`/`threatLevel` and the world-time unit bugs follow

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` — server-only manager: pools, registry, evaluate→score→create loop (30 s)
- `.../OVT_DeploymentComponent.c` — per-instance component on `Prefabs/GameMode/OVT_Deployment.et`; proximity activation; `ApplyPersistedDeployment`
- `.../OVT_DeploymentConfig.c` / `OVT_DeploymentRegistry.c` — authored configs; `FindConfigByName` is the runtime + persistence key
- `.../Modules/` — condition/spawning/behavior module hierarchy (prototype-clone pattern)
- `Configs/Deployment/overthrowDeployments.conf` — the 3 shipped configs (Town Patrol, Light/Heavy Vehicle Patrol)
- `Scripts/Game/Persistence/Serializers/Components/OVT_DeploymentManagerSerializer.c` + `OVT_DeploymentComponentSerializer.c`; `Overthrow.conf` entity rule (`SelfSpawn 1`)
- `docs/archive/ModularDeploymentSystem.md` — the original design (including the stalled migration strategy)

---

## Important Decisions

- **Marker entity = durable record:** forces virtualize by proximity; the marker persists with self-spawn and spawns nothing on load — the modules re-spawn near players. Module-internal state (routes, cooldowns) is deliberately not persisted.
- **Prototype-clone modules:** config holds one instance per module; `CloneModule` + hand-written `CopyTo` per class (new attributes must be added to the clone list manually).
- **Config-name strings** are both the persistence key and the integration key (`OVT_PatrolHarassmentStabilityModifier` looks up "Town Patrol" by literal).
- **Eliminated flag applied before module init** in `ApplyPersistedDeployment` — deliberate fix for the EPF-era resurrect-on-load bug (documented in-code).
- **Designed successor to BaseUpgrades**, coexisting instead: one migration (town patrols) done; both systems share `m_iResources`.

---

## Gotchas & Learnings

- **`m_mFactionDeployments` never prunes dead IDs** — the per-faction list grows to the 100 cap and the faction silently stops deploying. The system's long-campaign kill switch.
- **`m_iResourcesInvested` and `m_fThreatLevel` are never set at creation** — refunds always 0, persisted threat always 0.
- **Two time conventions in one folder:** patrol-scan interval and town-cache timeout authored in seconds but compared to millisecond world time (scan every tick; cache never caches); the reinforcement module uses ms correctly.
- **Runtime conditions are only evaluated by the reinforcement module** — a base flipping to the resistance leaves its patrols running forever; the design's `AreAllConditionsMet` gate was never built.
- **The marker teleports to the last-spawned vehicle** (`SetOrigin` in the per-vehicle loop), mutating the persisted position; waypoints are double-inserted and double-deleted.
- **Zero replication:** clients see nothing; several manager getters null-deref if called client-side (collections unallocated).
- **Renaming a deployment config** silently orphans persisted instances *and* disables the town stability modifier.
- Four of seven location flags (PORT/AIRFIELD/CHECKPOINT/OPEN_TERRAIN) can never produce candidates — stub getters.
- `OVT_DeployFOBAction`/`OVT_UndeployFOBAction` are unrelated resistance FOB actions — naming overlap only (verified: no imports either way).

---

*This context file was created retrospectively by analyzing existing code.*
