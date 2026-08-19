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

---

## 2026-08-20 — 🔴 The evaluator was choosing positions at RANDOM, and the authored threat gates had been dead for the whole campaign

**The report** (author, Normal-difficulty play-test): *"there is a lot of spending going on where it doesnt
make sense. building up bases far from the frontline or where there is any kind of threat. we might need to
tweak that as id rather the manager saved resources than just spend where it isnt needed."*

### The arithmetic, which is the whole diagnosis

`CalculateThreatLevel(position)` was:

```
threat  = ofManager.GetThreatLevel()          // GLOBAL - the SAME number at every position on the map
threat += ofManager.GetThreatByLocation(pos)  // the only term that knows where you are
```

and the evaluator then jittered the total by ±20%.

| Term | Magnitude (from the campaign that exposed it) |
|---|---|
| Global threat, identical everywhere | **~420** |
| Spatial score, realistic spread across a whole map | **0–60** |
| ±20% jitter, applied to the **total** | **±84** |

🔴 **The jitter alone was larger than the entire difference between the safest and the hottest position on the
map.** Ordering was a constant, plus noise, plus a signal a quarter the size of the noise. Sorting that is
sorting the noise — which is exactly the symptom reported.

The spatial term is small *by construction*, and that is fine as long as nothing swamps it: a known enemy base
within 1 km is worth **10**, a tower or FOB **5**, a warehouse **1**; a town contributes up to `5 × size` for
each of its support and stability components.

### It had also silently killed every authored `m_iMinimumThreatLevel`

Those thresholds are compared against this number. With ~420 added to every position, **every gate passed
everywhere, permanently, from the moment global threat first exceeded the threshold.** Three shipped configs
authored one, and all three had been inert for most of a campaign's life.

⚠ **The epic's own tech-debt list named this and was half right**: *"two threat concepts — the global
escalation scalar and the spatial `GetThreatByLocation` score — share a name but never interact."* They did
interact, in exactly one place, by addition, and that was the defect.

### The fix, in two halves (author chose both)

**1. The scores are separated.** `CalculateThreatLevel()` returns the spatial score alone. The global scalar
is not lost — it already governs *how much* the faction has to spend, through `PredictResourceGain()`, which
is what an escalation scalar should do. Deciding *where* to spend is a different question it was never
qualified to answer.

**2. A spend floor.** `MIN_LOCAL_THREAT_TO_DEPLOY = 5`: below it the candidate is skipped and the money stays
in the pool. Deliberately low — it excludes only the reported case (ground with no known target within a
kilometre and no town within three times its range, scoring 0–4) while leaving anywhere with a town or a
target able to develop. A higher floor would refuse to garrison real places early in a campaign, when support
is high and stability intact and every score is naturally small, and an undefended map is not recoverable by
waiting.

⚠ **The floor reads `candidate.threatLevel`, the UNBIASED score.** The objective anchor writes only to
`sortBy` (D5: it biases ordering, never eligibility), so an objective cannot make a dead corner worth
garrisoning — only change which *worthwhile* place is bought first.

⚠ **Free-at-game-start seeding does not pass through the floor** (`SeedFreeDeployments` → `PassesSeedConditions`
is a separate path) and — checked, not assumed — **none of the four free-at-start configs authors a threat
gate**, so a fresh campaign still seeds its garrisons, tower guards and town patrols exactly as before.

### Re-tuned to the new scale

| Config | Was | Now | Note |
|---|---|---|---|
| `Deployment_BaseHeavyPatrol` | 25 | **10** | same intent, scale that is actually compared against it |
| `Deployment_BaseATSection` | 50 | **20** | a notably hot area rather than a number nothing could fail |
| `Deployment_VehiclePatrol_Heavy` | 1200 | **30** | ⚠ **its meaning changed** — see below |
| `OVT_TEST_InitSuite.AT_MINIMUM_THREAT` | 50 | **20** | the one test pinning an authored threshold |

⚠ **`VehiclePatrol_Heavy` is a genuine semantic change, not a rescale.** At 1200 it was the one gate global
threat could not trivially clear, so it had become a *late-campaign* gate — "things are serious everywhere".
On the spatial scale it is now a *locality* gate — "this area is very hot". That is more consistent with every
other gate and with what the field is called, but it is a different rule and should be play-tested as one.

### A side effect worth knowing: the objective anchor works now

`m_fObjectiveAnchorWeight` is 25. Against the old scores (~420 ± 84) it was almost invisible — the jitter was
three times the bias. Against a 0–60 spatial score it is a strong, deliberate pull. **The anchor was
effectively inert until this change**, which also means any earlier judgement about whether objective-adjacent
deployments were being favoured was made against a bias that was not really operating.

`tools/compile-check.sh` exit 0. ⚠ **Suites not run** — the author is play-testing. The rescale touches a test
constant, so the All group is genuinely owed before this is trusted.
