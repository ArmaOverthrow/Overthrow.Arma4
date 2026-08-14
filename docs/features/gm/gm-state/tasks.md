# GM State - Task Checklist

**Feature:** gm/gm-state (epic `gm`, feature 1 of 5)
**Last Updated:** 2026-08-14 17:55
**Progress:** 27/28 tasks complete (96%) — only the Phase 5 MP play-test remains (user's desktop)

> ⚠️ **Advanced phases:** Phase 3 routes to `component-developer-advanced` (spawn-path insertions across
> four subsystems; behaviour must not change). All other implementation phases use standard agents.
> Agents stop at `tools/compile-check.sh` exit 0 — the orchestrator runs the suites post-phase per
> `.claude/test-policy.md` (Fast after Phases 1–2, All after Phases 3–4).

---

## Phase 0: Baseline (2/2 complete) ✅ — no agent

- [x] ✅ **Record baseline in context.md**
  - Description: `tools/compile-check.sh` exit code + file count; highest allocated bug id (`ls docs/bugs/` — BUG-167 at planning); `git status` snapshot
  - File(s): `docs/features/gm/gm-state/context.md`
  - Estimate: 🟢 Small
- [x] ✅ **Grep-prove the GUID series `{6B07…}` is still unused**
  - Description: 0 hits across `Prefabs`, `Configs`, `Scripts`; candidate `{6B07B0D93E8C72F6}` for the Phase 2 prefab block
  - File(s): (read-only greps)
  - Estimate: 🟢 Small

---

## Phase 1: Pure foundations — schedule maths + non-mutating gain predictor (5/5 complete) ✅ — `component-developer`

- [x] ✅ **Create `OVT_GMSchedule` pure statics**
  - Description: `InGameSecondsToNextMark` (marks 0/6/12/18, exactly-on-mark → 21600, documented boundary), `RealSecondsFor` (guard non-positive day duration), `PredictResourceGain` (arithmetic lifted verbatim from `GainResources()` incl. threatFactor clamp at 4 and five player bands). No `GetGame()`/`OVT_Global`/world refs — not even in comments (Logic-tier grep reads prose)
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMSchedule.c` (new)
  - Estimate: 🟡 Medium
- [x] ✅ **Refactor `GainResources()` to call the extracted arithmetic**
  - Description: Pure extraction — keeps its Prints, `m_iResources += newResources`, `AllocateDeploymentResourcesIfNeeded`. Behaviour byte-identical; read the diff
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (~:1433-1469)
  - Estimate: 🟢 Small
- [x] ✅ **Verify payout predictors are pure**
  - Description: Read `GetDonationIncome()` (:481) and `GetTaxIncome()` (:499); if either mutates, extract it too and record the finding
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟢 Small
- [x] ✅ **Float-precision threat accessor**
  - Description: `GetThreatLevel()` truncates float `m_iThreat` to int; add a float accessor (or read the field with a comment why)
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 🟢 Small
- [x] ✅ **Logic-tier test cases + registration, proven able to fail**
  - Description: §8 case list (marks incl. boundary/wrap/seconds precision, real-seconds conversion + degenerate day, gain clamp/bands/purity); register in `OVT_TEST_LogicSuite.c`; record the inversion proof in context.md
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMSchedule.c` (new), `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_LogicSuite.c`
  - Estimate: 🟡 Medium

---

## Phase 2: The seam — component, gate, framing, campaign records, poll lifecycle (7/7 complete) ✅ — `network-specialist`

- [x] ✅ **Create `OVT_GMRequestComponent` extending `OVT_ControllerRequestComponent`**
  - Description: Component + `OVT_GMRequestComponentClass` pattern per `OVT_EconomyRequestComponent.c:1-2`; `[Attribute]` tunables `m_fPollIntervalMs` (8000), `m_iMaxRecordsPerSnapshot` (400), `m_bDebugSnapshotTiming` (0)
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` (new)
  - Estimate: 🟡 Medium
- [x] ✅ **Add prefab block with grep-proven-unique GUID**
  - Description: Plain-text edit; missing block fails silently — the Init test is the only gate
  - File(s): `Prefabs/GameMode/OVT_OverthrowController.et`
  - Estimate: 🟢 Small
- [x] ✅ **The gate: `static bool IsAuthorizedGM(int playerId)` + refusal behaviour + dev override**
  - Description: `HasPlayerRole(GAME_MASTER) OR SCR_Global.IsAdmin`, called inside every RpcAsk after `ResolveOwningPlayerId()`; refusal = one throttled WARNING (per-player `m_fLastRefusalLog`), NO reply; `System.IsCLIParam("ovtGmDev")` server-side override
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟡 Medium
- [x] ✅ **Wire format v1: request type enum + framed campaign fan**
  - Description: `WIRE_VERSION=1`, `OVT_EGMRequestType { CAMPAIGN_SNAPSHOT }`; `RpcAsk_Snapshot(type, seq)`, `RpcDo_SnapshotBegin(seq, ver)`, `RpcDo_CampaignResources(seq, threat, ofRes, ofDepRes, flags)`, `RpcDo_CampaignSchedule(seq, distAmt, distSecs, payAmt, paySecs)`, `RpcDo_SnapshotEnd(seq, count)`; suppression flags bitfield (QRF-running, zero-players); every RpcDo via `ShouldRespondLocally` short-circuit; seq client-generated and echoed; ≤8 params per RPC, no arrays
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🔴 Large
- [x] ✅ **Create `OVT_GMCampaignState` client store with locally-ticking countdowns**
  - Description: Campaign scalars, arrival stamp `m_fReceivedWorldTime`, `GetDistributionSecondsRemaining()`/`GetPayoutSecondsRemaining()` per the `OVT_RadioTowerData.GetDisabledRemaining()` pattern, `Clear()`; staging-vs-committed handled in component (stale-seq drop, version refusal)
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` (new)
  - Estimate: 🟡 Medium
- [x] ✅ **Client lifecycle: editor hook, politeness gate, poll timer**
  - Description: `SCR_EditorManagerCore.Event_OnEditorManagerInitOwner` → `GetOnOpened()`/`GetOnClosed()`; on open: skip if `IsLimited()`, else request + `CallLater(poll, true)`; on close: remove CallLater, `m_State.Clear()`, fire `GetOnStateCleared()`; invoker trio `GetState()`/`GetOnSnapshotUpdated()`/`GetOnStateCleared()`
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟡 Medium
- [x] ✅ **Init-tier seam test, proven able to fail**
  - Description: Modelled on `OVT_TEST_Init_EconomyRequestSeam.c` — resolves via `OVT_ControllerComponent<T>.Get()` AND is on this player's own controller entity; prove by removing/restoring the prefab block
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMRequestSeam.c` (new), Init suite registration
  - Estimate: 🟡 Medium

---

## Phase 3: 🔴 ADVANCED — Group origin registry + spawn-site tagging (5/5 complete) ✅ — `component-developer-advanced`

- [x] ✅ **Create `OVT_GMGroupRegistry` scripted singleton**
  - Description: `OVT_EGroupOrigin` enum, `OVT_GMGroupOrigin` Managed record, static `Tag()` (no-op unless `Replication.IsServer()`, no-op on null), `Sweep()` (entity-resolution ONLY — never agent-count), `Find`/`Count`/`GetAll`. Never persisted, never RplProp, NO untag sites
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMGroupRegistry.c` (new)
  - Estimate: 🟡 Medium
- [x] ✅ **Insert ~13 one-line `Tag()` calls at spawn sites (§4 Phase 3 table)**
  - Description: Base upgrades (5 classes via `BuyPatrol` + DefensePosition/Sniper/TowerGuard/TownPatrol), QRF `SpawnFromQueue`, radio-tower garrison, resistance garrisons (base/camp/FOB + boot-restore ×2 + saved base garrison), deployment modules (infantry initial/reinforce, vehicle crew), job stage. Pure insertions beside existing tracking calls — nothing deleted/reordered/conditioned. Re-confirm every cited line first (tree moves)
  - File(s): 10 files across `Controllers/OccupyingFaction/`, `GameMode/Managers/Factions/`, `GameMode/Deployments/Modules/`, `GameMode/Systems/Jobs/Stages/`
  - Estimate: 🔴 Large
- [x] ✅ **Record deliberate non-tag sites in context.md**
  - Description: Town civilians, player groups, recruits, client-local preview groups (structurally excluded by IsServer guard)
  - File(s): `docs/features/gm/gm-state/context.md`
  - Estimate: 🟢 Small
- [x] ✅ **Verify spawned `SCR_AIGroup`s carry `RplComponent`**
  - Description: STOP CONDITION if not — position-based fallback would change the epic's join-key contract; report, don't improvise
  - File(s): (verification; `Prefabs/` group prefabs)
  - Estimate: 🟢 Small
- [x] ✅ **Campaign-tier registry test, proven able to fail**
  - Description: Count ≥1 in started campaign, no UNKNOWN origins, `Sweep()` idempotent; failure message prints per-base upgrade group counts (anti-vacuity); prove by removing one `Tag()` call, record which
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_GMGroupRegistry.c` (new), Campaign suite registration
  - Estimate: 🟡 Medium

---

## Phase 4: The per-entity fan — bases, upgrades, deployments, groups (5/5 complete) ✅ — `network-specialist`

- [x] ✅ **Create `OVT_GMRecords.c` — four plain Managed record classes**
  - Description: `OVT_GMBaseRecord`, `OVT_GMBaseUpgradeRecord`, `OVT_GMDeploymentRecord`, `OVT_GMGroupRecord` — one set of classes used on both sides
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMRecords.c` (new)
  - Estimate: 🟢 Small
- [x] ✅ **Create `OVT_GMSnapshotBuilder` (server-side, read-only)**
  - Description: Bases by index (sum upgrade resources/groups), non-empty upgrades only, deployments from `m_aActiveDeployments` (verify threat-level field exists before sending one), groups after `registry.Sweep()` with resolvable RplComponent
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMSnapshotBuilder.c` (new)
  - Estimate: 🔴 Large
- [x] ✅ **Add four record RPCs to the fan**
  - Description: `RpcDo_Base(seq, baseIndex, res, groups, upgrades)`, `RpcDo_BaseUpgrade(seq, baseIndex, type, res, groups)`, `RpcDo_Deployment(seq, rplId, name, faction, invested, active)`, `RpcDo_Group(seq, rplId, originType, originIndex, reason)` — each with `ShouldRespondLocally`; hand arity-diff every call site
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟡 Medium
- [x] ✅ **Cap + budget instrumentation**
  - Description: Stop at `m_iMaxRecordsPerSnapshot` with one WARNING naming the truncated class; per-class counts + build time under `m_bDebugSnapshotTiming`
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMSnapshotBuilder.c`, component
  - Estimate: 🟢 Small
- [x] ✅ **Extend `OVT_GMCampaignState` with arrays + lookups**
  - Description: Four arrays, `FindGroup(RplId)`, `FindBase(int)`, cleared in `Clear()`
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c`
  - Estimate: 🟢 Small

---

## Phase 5: Verification gate (0/1 complete) ⏸️ — user-driven (MP play-test owed; automated gates + greps all green)

- [ ] ⏸️ **Run §7 Verification Method end-to-end** (compile/suites/greps done by orchestrator — MP play-test **deferred by user until `overthrow-panel` ships UI**; combined checklist in context.md "Needs Human Verification")
  - Description: MP play-test (server `-ovtGmDev`, admin positive path, negative path WITHOUT the flag, host path, JIP, lifecycle); grep gates Q-5/Q-6/F-7/I-3 pasted into context.md; measured record counts + build time on a populated campaign
  - File(s): `docs/features/gm/gm-state/context.md`
  - Estimate: 🔴 Large (user desktop required for client launches)

---

## Phase 6: Consumption contract + docs (3/3 complete) ✅ — `component-developer`

- [x] ✅ **Write the sibling consumption contract into context.md**
  - Description: Wire table (exact arities), accessor/store/invoker trio, record shapes, join keys (RplId groups/deployments, positional index bases), locally-ticking countdown readers, "already replicated — read locally" table, GM-sees-nothing triage section
  - File(s): `docs/features/gm/gm-state/context.md`
  - Estimate: 🟡 Medium
- [x] ✅ **Update epic-overview.md**
  - Description: Feature 1 status; Findings: pre-existing group-cleanup leaks (§5 D7) attributed to `occupying` epic; record the help-docs hand-off to `overthrow-panel`
  - File(s): `docs/features/gm/epic-overview.md`
  - Estimate: 🟢 Small
- [x] ✅ **Final doc refresh**
  - Description: `/update-feature` + `/update-epic` + `/update-master` (epic rolls to one row)
  - File(s): feature docs, `docs/overview.md`
  - Estimate: 🟢 Small

---

## Bugs & Issues

**Active Bugs:**
- (none yet)

**Fixed Bugs:**
- (none yet)

---

## Technical Debt

- (none yet — pre-existing group-cleanup leaks are the `occupying` epic's debt, recorded in epic-overview.md at Phase 6)

---

## Progress Tracking

### Discovered New Tasks
- (add as discovered)

### Blocked Items
- (none)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
