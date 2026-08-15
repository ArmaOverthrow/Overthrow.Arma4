# GM State - Context & Decisions

**Feature:** gm/gm-state (epic `gm`, feature 1 of 5 — the data spine for overthrow-panel, hud-icons, waypoint-viz, gm-map)
**Last Updated:** 2026-08-14 17:55
**Current Phase:** Built — Phase 5 MP play-test owed
**Status:** 🟢 Ready for Review

---

## Quick Status

**What's Done:**
- ✅ Planning complete (`implementation.md`, verified against `f47b66a1` + Reforger 1.8 reference tree)
- ✅ Dev docs scaffolded
- ✅ Phase 0 baseline (see below)
- ✅ Phase 1: `OVT_GMSchedule` pure statics, `GainResources()` pure extraction, `GetThreatFloat()`, 30 Logic assertions — Fast suite green (113 tests, 44s)

- ✅ Phase 2: `OVT_GMRequestComponent` seam (gate + wire v1 + campaign fan + poll lifecycle), `OVT_GMCampaignState` store, prefab block `{6B07B0D93E8C72F6}`, Init seam test — Fast green (114 tests; the Init case proven-fail red first with the block removed)

- ✅ Phase 3 (ADVANCED): `OVT_GMGroupRegistry` + 17 tagging insertions across 11 files, all pure-insertion hunks; RplComponent join key verified safe; All group green (157 tests, Campaign registry case passed with 3 live tagged groups: `TOWER_GUARD#0`, `BASE_DEFENCE#0`, `BASE_SNIPER#0`)

- ✅ Phase 4: per-entity fan — `OVT_GMRecords`, `OVT_GMSnapshotBuilder` (read-only, capped, non-empty filter), four record RPCs, store arrays + lookups; All group green (157 tests)
- ✅ Phase 6: sibling consumption contract (below), epic-overview findings + help-docs hand-off, grep gates recorded

**What's Next:**
- ⏸️ **Phase 5 MP play-test — deferred by the user (2026-08-14) until `overthrow-panel` exists**, so verification reads a real panel instead of debug prints. Run the combined checklist ("Needs Human Verification" below) after the panel is built; record measured per-class record counts + build ms, and which auth path (admin login vs `-ovtGmDev`) was exercised
- 📋 Next: `/autorun-feature gm/overthrow-panel` (consumes this contract; carries the help-docs phase)

**Blockers:**
- None

---

## Key Files

### To be created
- `Scripts/Game/GameMode/GM/OVT_GMSchedule.c` — pure statics: mark countdown, real-seconds conversion, gain prediction (Phase 1)
- `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` — client-side store, locally-ticking countdowns (Phase 2)
- `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` — the ONLY networked class; gate + fan + poll lifecycle (Phase 2)
- `Scripts/Game/GameMode/GM/OVT_GMGroupRegistry.c` — server-only singleton, tag-and-sweep (Phase 3)
- `Scripts/Game/GameMode/GM/OVT_GMRecords.c` + `OVT_GMSnapshotBuilder.c` — record shapes + read-only builder (Phase 4)
- Tests: `Logic/OVT_TEST_Logic_GMSchedule.c`, `Init/OVT_TEST_Init_GMRequestSeam.c`, `Campaign/OVT_TEST_Campaign_GMGroupRegistry.c`

### Modified
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — pure extraction of `GainResources()` arithmetic + float threat accessor (Phase 1); tagging insertions (Phase 3)
- `Prefabs/GameMode/OVT_OverthrowController.et` — new component block (Phase 2)
- ~10 spawn-site files — one-line `Tag()` insertions (Phase 3, table in implementation.md §4)

### Related
- `docs/features/gm/gm-state/implementation.md` — the plan; §5 decisions D1–D11 are settled, do not re-litigate
- `Scripts/Game/Components/Controller/OVT_ControllerRequestComponent.c` — base class (gate helpers, BUG-090 short-circuit)
- `Scripts/Game/Components/Controller/OVT_EconomyRequestComponent.c` — the pattern to copy

---

## Important Decisions

(Planning decisions D1–D11 live in `implementation.md` §5 — user-settled: poll-not-delta, GAME_MASTER-or-IsAdmin gate, tag-and-sweep registry, threat grid deferred to gm-map, `GetDayDuration()` not `GetDayTimeMultiplier()`. Implementation-time decisions get recorded here.)

---

## Baseline (Phase 0) — 2026-08-14 16:40

- `tools/compile-check.sh` → exit 0, **6059 files**, 19s
- Highest allocated bug id: **BUG-167** (matches planning snapshot; that pair was renumbered BUG-172/BUG-173 in the 2026-08-15 main merge because main had claimed 166–171)
- GUID series `{6B07…}`: **0 hits** across `Prefabs`, `Configs`, `Scripts` — candidate `{6B07B0D93E8C72F6}` free for the Phase 2 prefab block
- Git: HEAD `f47b66a1` (same commit the plan's line numbers were verified against — citations valid); tree clean except this feature's new docs

---

## Gotchas & Learnings

### Phase 1 findings (2026-08-14)
- **Logic cases self-register** via `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]` — no edit to `OVT_TEST_LogicSuite.c` needed (verified against `OVT_TEST_Logic_TravelFares`).
- **Economy payout getters are pure** — `GetDonationIncome()` (:481-494) and `GetTaxIncome()` (:499-510) only read town data into a local; the prediction path may call them directly. ⚠️ The neighbouring `PayIncome`-style code at :470-474 calls `AddPlayerMoney` — the read-only path must call the two getters, never the payer.
- **Extraction preserved int truncation** — `newResources` stays `int`, so the `int + (int * float)` truncation semantics of the original `GainResources()` are intact.
- **Invert-proof (Logic tier):** inverted the on-mark boundary case (`ExpectNextMark(6,0,0, …)` expected 21600 → 0); traced to a named `SetFailure` (not a silent pass); reverted. Fast suite then run for real by the orchestrator: **113 tests, exit 0, 44s**.

### Phase 2 findings (2026-08-14)
- **Prove-can-fail (Init tier), executed for real:** removed the `OVT_GMRequestComponent "{6B07B0D93E8C72F6}"` block from the controller prefab → bare-case run `OVT_TEST_Init_Controller_GMRequestResolves` → **FAILED (1 of 1)** as designed; restored the block → Fast group **114 tests, exit 0** (+1 = the new case). A missing prefab block is otherwise fully silent — this test is the only gate.
- **Arity diffs recorded** (5 `Rpc()` sites, each with an identical-arg local short-circuit): RpcAsk_Snapshot 2/2, RpcDo_SnapshotBegin 2/2, RpcDo_CampaignResources 5/5 (int,float,int,int,int), RpcDo_CampaignSchedule 5/5 (int,int,float,int,float), RpcDo_SnapshotEnd 2/2. Max arity 5, no arrays.
- **DoD greps clean:** zero `RplProp`, zero `RplRcver.Broadcast`, no mutating call from the prediction path (`GainResources` appears only in "do not call" comments).
- **Suppression flags:** `FLAG_DISTRIBUTION_SUPPRESSED_QRF=1` (from `occupying.m_CurrentQRF` non-null, matching the early return at `OVT_OccupyingFactionManager.c:1169`), `FLAG_PAYOUT_SUPPRESSED_NO_PLAYERS=2` (`GetPlayerCount()==0`, `OVT_EconomyManagerComponent.c:161-165`).
- **Both countdowns currently share one `SecondsToNextMark()` value** (both loops fire on the same 6-hour marks) but ship as two independent wire fields so future divergence costs no wire change.
- **Owner scoping:** `Event_OnEditorManagerInitOwner` only fires on the owner's machine, but a client holds a replicated component instance per connected player — so `OnEditorOpened()`/`RequestSnapshot()` re-assert `IsLocalControllerOwner()` (compares `OVT_Global.GetController()` vs `GetOwner()`; works on a listen-server host, which never receives the owner-assignment RPC).

### Phase 3 findings (2026-08-14)
- **Tagging as applied:** 17 sites across 11 files (12 files touched — `OVT_BaseUpgrade.c` gained a shared guarded `GetBaseOriginIndex()` helper so no unguarded deref chain sits inside a spawn path; a throw there would be a behaviour change). Full site table in the Phase 3 agent report; origins: BASE_PATROL/DEFENCE/SNIPER/TOWER_GUARD via `ClassName()` reasons, TOWN_PATROL by townID, QRF, RADIO_TOWER_GARRISON by tower.id, BASE/CAMP/FOB_GARRISON (live + "Restored" boot paths), DEPLOYMENT ×3 by deployment name, JOB via guarded `GetJobOriginReason()`.
- **Camp/FOB index uses `m_Camps.Find(fob)` / `m_FOBs.Find(fob)`**, not `fob.id` — `.id` is only re-derived on the load path and six insert sites never set it.
- **Registry singleton survives world reload** (plain Managed, engine doesn't null it like component `s_Instance` weak refs) — it can carry dangling EntityIDs into a new world. This is why the Campaign test sweeps *before* counting, and why consumers must always `Sweep()` before reading (the snapshot builder does).
- **Not tagged (deliberate):** town civilians (`OVT_TownController.c:166`, record-budget dominance), player groups, recruits (not group spawns), client-local preview groups (`OVT_BaseMenuContext.c:57`/`OVT_FOBMenuContext.c:42` — structurally excluded by the `Replication.IsServer()` guard in `Tag()`).
- **RplComponent verification PASS:** every Overthrow-spawned group prefab deltas onto vanilla `Group_Base.et:103`, which carries `RplComponent {524EC5D51F101B32}`. Corroborated by `OVT_SpawnGroupJobStage.c:44-45` shipping an unguarded `rpl.Id()` today. Epic RplId join key is safe. (`PlayableGroup.et` has none — but is deliberately untagged.)
- **Pure-insertion proof:** `git diff -U0` — every Phase 3 hunk has a `-N,0` left side; the only deletions in the tree are Phase 1's documented extraction.
- **All group run (gate):** **157 tests, exit 0, 54s**. `OVT_TEST_Campaign_GMGroupRegistry` SUCCESS in 4.8s of case time / 218 ticks: **3 live tagged groups — `TOWER_GUARD#0:OVT_BaseUpgradeTowerGuard`, `BASE_DEFENCE#0:OVT_BaseUpgradeDefensePosition`, `BASE_SNIPER#0:OVT_BaseUpgradeSniperPosition`** — the definitive record of which sites fire in the autotest world (not BASE_PATROL as statically predicted; three *distinct* insertion sites exercised, which is stronger). Prove-can-fail is anchored by the failure message printing the per-base upgrade ledger, distinguishing "groups exist untagged" from "no groups spawned".

---

## Sibling Consumption Contract

**This section is the contract for `overthrow-panel`, `hud-icons`, `waypoint-viz` and `gm-map`.** A sibling
consumes the client-side cache and the invokers — it never touches an RPC, never adds a request, and never
asks this seam for data that is already replicated to every client (see the last table).

### Accessor and lifecycle (the trio)

```c
OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
gm.GetOnSnapshotUpdated().Insert(OnSnapshot);   // fired after each committed snapshot (~every 8 s while GM open)
gm.GetOnStateCleared().Insert(OnCleared);       // fired when the editor closes and the store is wiped
OVT_GMCampaignState state = gm.GetState();      // the store; never null, check state.HasData()
```

- No `OVT_Global` accessor exists or may be added (project rule, `OVT_ControllerComponent.c:10-14`).
- The store is populated **only while the local player has the Game Master editor open** and only if the
  server authorized them (`GAME_MASTER` role OR admin). Renderers should show "no data" until `HasData()`.
- Polling is invisible to consumers: subscribe to `GetOnSnapshotUpdated()`, re-read the store, redraw.

### Campaign scalars (on `OVT_GMCampaignState`)

- `m_fThreat` (float — full precision, not the truncated `GetThreatLevel()` int)
- `m_iOFResources` (occupying-faction reserve), `m_iOFDeploymentResources` (deployment pool)
- `m_iDistributionAmount` + `GetDistributionSecondsRemaining()` — next OF resource distribution
- `m_iPayoutAmount` + `GetPayoutSecondsRemaining()` — next resistance payout
- `m_iFlags` — bitfield: `FLAG_DISTRIBUTION_SUPPRESSED_QRF = 1` (a QRF is running; distribution won't fire),
  `FLAG_PAYOUT_SUPPRESSED_NO_PLAYERS = 2`. **Render the suppression**, or a GM watches a countdown hit zero
  with nothing happening and files a bug.
- The two `Get*SecondsRemaining()` readers **tick locally** (they subtract elapsed world time since arrival
  and clamp at 0) — call them every frame if you like; re-sync happens automatically on the next poll.

### Record arrays (filled per snapshot; cleared on editor close)

- `m_aBases` (`array<ref OVT_GMBaseRecord>`) — `m_iBaseIndex`, `m_iResources` (summed), `m_iGroups`,
  `m_iUpgrades` (all, including empty ones — so "12 upgrades, 4 upgrade rows" means 8 empty, not truncation)
- `m_aBaseUpgrades` (`array<ref OVT_GMBaseUpgradeRecord>`) — `m_iBaseIndex`, `m_sType` (upgrade
  `ClassName()`), `m_iResources`, `m_iGroups` — **non-empty upgrades only**
- `m_aDeployments` (`array<ref OVT_GMDeploymentRecord>`) — `m_RplId`, `m_sName`, `m_iFaction`,
  `m_iResourcesInvested`, `m_bActive`
- `m_aGroups` (`array<ref OVT_GMGroupRecord>`) — `m_RplId`, `m_iOriginType` (`OVT_EGroupOrigin`),
  `m_iOriginIndex`, `m_sReason`
- Convenience: `IsDistributionSuppressedByQRF()`, `IsPayoutSuppressedByNoPlayers()`; arrays are never null
- Lookups: `FindGroup(RplId)`, `FindBase(int baseIndex)`. `m_iReportedRecordCount` is the server's own count
  for sanity checks.

### Join keys (how a record maps to a thing on screen)

- **Groups & deployments → `RplId`.** From a GM selection (`SCR_EditableEntityComponent`) take the owner
  entity's `RplComponent.Id()` and call `FindGroup(id)`. Deployment *position* is not sent — resolve the
  RplId to the entity and ask it.
- **Bases → positional index** into `OVT_Global.GetOccupyingFaction().m_Bases` (same index the JIP stream
  uses): `m_Bases[record.m_iBaseIndex].location` etc.
- **Group origin `m_iOriginIndex`:** base-origin types carry the base *index*; TOWN_PATROL carries townID;
  RADIO_TOWER_GARRISON carries tower id; CAMP/FOB_GARRISON carry the position in `m_Camps`/`m_FOBs`; QRF,
  DEPLOYMENT and JOB carry `-1` (reason string identifies them).

### Already replicated — read locally, NEVER ask this seam

| Datum | Read from |
|---|---|
| Resistance funds | `OVT_Global.GetEconomy()` (`m_iResistanceMoney` + `m_OnResistanceMoneyChanged`) |
| Town support / stability / population / faction / modifiers | `OVT_Global.GetTowns()` (`OVT_TownData`, fully replicated) |
| Player money / XP / level / officer | economy + `OVT_PlayerData.GetLevel()` (replicated) |
| Base location + faction | `OVT_Global.GetOccupyingFaction().m_Bases` (JIP-streamed) |
| Radio tower location / faction / downtime | `OVT_RadioTowerData` |
| QRF active / location / points / timer | occupying-faction manager (broadcast) |

### Triage: "a GM sees nothing"

1. **Prefab block** — `OVT_GMRequestComponent "{6B07B0D93E8C72F6}"` present in
   `Prefabs/GameMode/OVT_OverthrowController.et`? (Missing block is fully silent; `OVT_TEST_Init_GMRequestSeam` is the gate.)
2. **Gate** — is the player actually authorized? Server logs a throttled WARNING per refused player id. Local
   testing needs the server started with `-ovtGmDev` (server-side CLI flag; `tools/launch-server.sh -- -ovtGmDev`).
3. **Editor hook** — `Event_OnEditorManagerInitOwner` fired? A limited editor (`IsLimited()`) never polls, by design.
4. **Seq mismatch** — with `m_bDebugSnapshotTiming` on, client prints staged counts at commit; records with a
   stale seq are dropped, never merged.
5. **Wire version** — a `WIRE_VERSION` mismatch refuses to stage and logs once (mixed client/server builds).

### Extending the wire (gm-map's threat grid, Phase 2 write actions)

- New request type → add to `OVT_EGMRequestType`; new records → new `RpcDo_*` under the same seq/version
  framing. Additive only; never change an existing signature.
- **Invariant for Phase 2 (write actions):** every current and future `RpcAsk_*` handler on
  `OVT_GMRequestComponent` MUST call `IsAuthorizedGM(playerId)` after `ResolveOwningPlayerId()` before
  touching campaign state, and refuse silently. A write handler that forgets this is privilege escalation.

---

## Verification Record

**Automated gates (all green, run 2026-08-14):**
- Compile: exit 0 at every phase boundary (final: 6068 files).
- Suites: Fast 113 after Phase 1 → Fast 114 after Phase 2 (+1 = Init seam case) → All 157 after Phase 3
  (+ Campaign registry case) → All 157 after Phase 4 (no new cases, count correctly unchanged).
- Prove-can-fail: Logic (static inversion, traced to named failure), Init (real red run with prefab block
  removed: `FAILED (1 of 1)`, then restored → green), Campaign (failure message prints the upgrade ledger;
  success print names the 3 live tagged groups).
- **Grep gates (Q-5/Q-6/F-7/I-3), all pass:** zero `RplProp` and zero `RplRcver.Broadcast` across all six
  feature files; max RPC arity 6 (≤8), no `array<>` params; no resistance-money/support/stability/population/
  player-money/level field or param anywhere in the feature (F-7); no gm-state reference in `OVT_Global.c`
  (I-3); Q-6 mutation grep hits are only doc-comments and assignments to the feature's own record objects.

## Needs Human Verification (Phase 5 — the MP gate, §7 Verification Method)

Everything below is MP-only and covered by no suite. Steps for the user (⚠️ client launches open windows on
your desktop):

1. `tools/launch-server.sh -- -ovtGmDev`
2. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
3. On client 1 try the in-game admin login (`devadmin`) then open GM — if admin auth works locally the real
   gate is exercised; otherwise the `-ovtGmDev` flag from step 1 is the fallback. **Record which path was used.**
   Set `m_bDebugSnapshotTiming` (controller prefab attribute) to see server build counts + client staged counts.
   - F-1/F-2: state populated within one poll; countdowns tick down and re-sync without big jumps
   - F-3/F-4: per-base/upgrade/deployment/group records present; origins match reality
   - Measure: per-class record counts + build ms on a populated campaign (Phase 4 acceptance; expect ~100–200)
4. Negative path (restart server WITHOUT `-ovtGmDev`): second client, non-admin — no `RpcDo_*` ever arrives
   (F-5); a coerced request gets no reply + one throttled server WARNING (F-6)
5. Host path: listen-server host opens GM and sees the same state (Q-2 — the `ShouldRespondLocally` case)
6. JIP: with a GM polling, join a second client into the established campaign, make it GM, first snapshot
   complete (the most common regression class in this project)
7. Lifecycle: close editor → server-side request print goes quiet, `GetOnStateCleared()` fires once (Q-3)
8. Stale-discard (Q-1, optional): drop `m_fPollIntervalMs` to ~200 ms and confirm commits are single-seq

---

## Session Notes

### 2026-08-14 16:38
- Feature started via /autorun-feature (Discord). Docs scaffolded from the completed plan.
- Next: Phase 0 baseline, then Phase 1 via component-developer.

### 2026-08-14 17:55 — autorun complete (Phases 0–4 + 6)
- Built in one autonomous run: Phase 1 (component-developer), Phase 2 (network-specialist), Phase 3
  (component-developer-advanced), Phase 4 (network-specialist), Phase 6 docs (orchestrator).
- Every suite gate green: Fast 113 → Fast 114 → All 157 → All 157. All three new test cases proven able to
  fail (methods recorded above). Grep gates Q-5/Q-6/F-7/I-3 pass.
- New files: `GM/OVT_GMSchedule.c`, `GM/OVT_GMCampaignState.c`, `GM/OVT_GMGroupRegistry.c`,
  `GM/OVT_GMRecords.c`, `GM/OVT_GMSnapshotBuilder.c`, `Controller/OVT_GMRequestComponent.c`, three test
  cases. Modified: `OVT_OccupyingFactionManager.c` (extraction + accessor + 2 tags),
  `OVT_OverthrowController.et` (prefab block), 10 more spawn-site files (pure insertions), plus a guarded
  helper in `OVT_BaseUpgrade.c` / `OVT_SpawnGroupJobStage.c`.
- Everything left **uncommitted** on `v1.5` per the autorun no-git contract.
- Next session: user runs the Phase 5 MP checklist; then `overthrow-panel`.

---

*Update this file at the end of each work session.*
