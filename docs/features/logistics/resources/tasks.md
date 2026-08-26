# Resources (logistics/resources) - Task Checklist

**Last Updated:** 2026-08-21
**Progress:** 95/98 tasks complete (97%) — see "Closed out, not done" below

> Phases **3, 4, 5, 7, 8, 9** are flagged **⚠️ ADVANCED** per implementation.md §4 / Agent Routing Summary.
> `tools/run-tests.sh` is the **orchestrator's** gate only — never an agent's. Suites are run **by class name**, one at a time (`.claude/test-policy.md`).
> ⚠️ **User asked on 2026-08-21 to hold all `run-tests.sh` runs until the end of the autorun.** Per-phase gates from Phase 8 on are `compile-check.sh` only; the full suite sweep runs once at the end.
> Suite baselines to beat (2026-08-21): Logic 216/216 · Init 162/163 (one pre-existing red, `CompositionSlotGate_AcceptedTypesMatchTheCompositions`) · PersistenceRoundTrip 34/34 · Campaign 16/16 · Persistence 13/13.

---

## Phase 1 — The pure spine (`component-developer`, ~5-6 h) · Suite: **OVT_TEST_LogicSuite** (8/8 complete ✅)

- [x] 1.1 `Scripts/Game/Data/OVT_ResourceLedger.c` — `OVT_ResourceAmount` + ledger per §3.2; `TotalLitres()` O(1) by construction; integer litres only
- [x] 1.2 `Scripts/Game/Data/OVT_ResourceDefs.c` — the definition lookup surface of §3.2
- [x] 1.3 `Scripts/Game/Data/OVT_ResourcePack.c` — pure `Encode`/`Decode` pair for the packed `RplProp` string (D4)
- [x] 1.4 `Scripts/Game/Data/OVT_ResourceRules.c` — the pure statics of §3.2 (drift/level/sell/illegal predicates land as stubs where Phase 7 fills them)
- [x] 1.5 `Tests/TestSuites/Logic/OVT_TEST_Logic_ResourceLedger.c` — the five ledger cases of §7, registered on `OVT_TEST_LogicSuite`
- [x] 1.6 `Tests/TestSuites/Logic/OVT_TEST_Logic_ResourceRules.c` — the pack round-trip + rules cases of §7, incl. the D20 illegal-gate case
- [x] 1.7 Tier hygiene: no manager accessor / game-mode getter identifier anywhere under `TestSuites/Logic/` (comments included); `new` sets every field explicitly; no ternaries; no `maxAttempts`; polls are preconditions with a named failure; `grep float` on the ledger returns only `TotalWeightKg`
- [x] 1.8 Fail-prove every case once; record mutation + resulting message in `context.md`; `tools/compile-check.sh` exit 0

## Phase 2 — Config + manager + `OVT_Global` (`component-developer`, ~4-5 h) · Suite: **OVT_TEST_InitSuite** (7/7 complete ✅)

- [x] 2.1 `Scripts/Game/Configuration/OVT_ResourcesConfig.c` — the `OVT_BuildablesConfig`-shaped config class
- [x] 2.2 `Configs/Resistance/resources.conf` (+ `.meta`, GUID from `6A8E2E…`) with the four MVP resources of §3.9
- [x] 2.3 `Scripts/Game/GameMode/Managers/OVT_ResourceManagerComponent.c` — config load via `BaseContainerTools.LoadContainer` + `CreateInstanceFromContainer`, `m_Defs`, price table init-to-base through `m_aCurrentPrice`, accessors `GetPrice`/`GetSellPrice`/`GetBasePrice`/`GetDefs`/`GetSupplyRadius`/`GetMergeRadius`. **No drift** (Phase 7), **no pile spawn** (Phase 5)
- [x] 2.4 `Prefabs/GameMode/OVT_OverthrowGameMode.et` — the manager component + `m_rResourcesConfigFile`
- [x] 2.5 `OVT_Global.GetResources()`
- [x] 2.6 `Tests/TestSuites/Init/OVT_TEST_Init_ResourceSeam.c` — manager resolves; config populates 4 definitions with non-zero litres and base price; every definition id unique and non-empty
- [x] 2.7 Grep gate: no call site reads `m_aBasePrice` as a live price; `tools/compile-check.sh` exit 0

## Phase 3 — Store component + prefabs ⚠️ ADVANCED (`component-developer-advanced`, ~6-7 h) · Suite: **OVT_TEST_InitSuite** (10/10 complete)

- [x] 3.1 `Scripts/Game/Components/OVT_ResourceStoreComponent.c` — §3.3: the `[RplProp]` packed contents string, `m_iCapacityLitres` from the m³ attribute in `OnPostInit`, `PublishContents()` as the sole `Replication.BumpMe()` call site
- [x] 3.2 `Scripts/Game/Components/OVT_ResourcePileComponent.c` — the pile marker component (D16)
- [x] 3.3 `Scripts/Game/Utilities/OVT_ResourceUtils.c` — per-call query objects (no shared accumulator)
- [x] 3.4 **New same-GUID delta** `Prefabs/Vehicles/Core/Wheeled_Truck_Base.et` `{E03D5609EEA6E03D}` — parent line byte-for-byte, `ID "BBCBA43A9778AE21"`, the store, and `SCR_BaseHUDComponent` restating all three InfoDisplays
- [x] 3.5 Cap overrides on `M923A1_transport.et`, `Ural4320_transport.et`, `OverthrowMobileFOB.et`, `OverthrowMobileFOBDeployed.et`
- [x] 3.6 `Warehouse_01_Base.et` — add the store (unlimited), leaving the shipped `OVT_StorageComponent` untouched
- [x] 3.7 `Prefabs/Props/Resources/OVT_ResourcePile.et` — `CrateStack_01_base.et` parent, store (−1), pile component, `RplComponent`, `ActionsManagerComponent`, `SCR_DestructionMultiPhaseComponent { Enabled 0 }`
- [x] 3.8 Extend `OVT_TEST_Init_ResourceSeam.c` — spawned M923A1 transport / test-world `Warehouse_01` / spawned pile resolve stores at 20000 / −1 / −1; a spawned **car** resolves none
- [x] 3.9 Lifecycle + GUID hygiene: `!owner.GetWorld()` guard and the `GetRpl()` ERROR present; fresh GUIDs from `6A8E2E0…` verified unused by `grep -rl`; **inherited component GUIDs copied, not minted**; `m_iCapacityLitres` never written outside `OnPostInit`
- [x] 3.10 `tools/compile-check.sh` exit 0

## Phase 4 — Persistence ⚠️ ADVANCED (`component-developer-advanced`, ~5-6 h) · Suite: **OVT_TEST_PersistenceRoundTripSuite** (9/9 complete ✅)

- [x] 4.1 Read an existing versioned serializer first; then `Scripts/Game/Persistence/Serializers/Components/OVT_ResourceStoreComponentSerializer.c` + frozen `OVT_PersistedResourceLine` (§3.11)
- [x] 4.2 `Scripts/Game/Persistence/Serializers/Components/OVT_ResourceManagerSerializer.c` — the price table by stable id (two index-aligned arrays)
- [x] 4.3 `Configs/Systems/Persistence/Overthrow.conf` — bindings 1, 2, 3 and 5; binding 4 gains its store **and** `OVT_StorageComponentSerializer` now (D15), its site serializer in Phase 8; GUIDs from `6B0E7A7…`
- [x] 4.4 `EnsureTracked()` on first non-empty publish — **landed in Phase 3** (P3-d), complete not stubbed
- [x] 4.5 `OVT_TEST_PersistenceRoundTripSuite.c` — truck load round-trip
- [x] 4.6 …pile round-trip (spawned, tracked, `SelfSpawn`)
- [x] 4.7 …warehouse resource stock round-trip
- [x] 4.8 …**price state** round-trip: drift a price by hand through the manager's setter, reload, assert the drifted value not base
- [x] 4.9 Serialize/Deserialize locals identically named (rename shown to fail, recorded in `context.md`); every `Read()` return checked; forced failure leaves live state untouched + logs ERROR; **no new `ComponentClassPersistenceConfigRule` on `OVT_ResourceStoreComponent`**; new saving cases sort after `…_Capability_…`; `tools/compile-check.sh` exit 0

## Phase 5 — Request component, wire, pile spawn/merge ⚠️ ADVANCED (`network-specialist-advanced`, ~6-8 h) · Suite: **OVT_TEST_InitSuite** (9/9 complete)

- [x] 5.1 `Scripts/Game/Components/Controller/OVT_ResourceRequestComponent.c` — class, attributes, `MayUseHolder`, `CallerIsWithin`, the checkout state machine
- [x] 5.2 The six RPCs of §3.5 — `Begin`/`Line`/`Commit` fan + the error/ack replies; no `array<…>` on any RPC; both `RplId` slots always valid
- [x] 5.3 Whole-cart atomicity in `Commit` — **nothing clamps**; a cart that does not fit is refused whole
- [x] 5.4 Every `RpcDo_*` takes the `ShouldRespondLocally` branch first; every refusal answers `RpcDo_ResourceError`, none returns silently
- [x] 5.5 `OVT_ResourceManagerComponent.SpawnOrMergePile` + pile cleanup (drained pile deletes itself)
- [x] 5.6 `Prefabs/GameMode/OVT_OverthrowController.et` — append the component **before** the `RplComponent` (`:48`)
- [x] 5.7 Extend `OVT_TEST_Init_ResourceSeam.c` — the controller-component seam case (`OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get()` non-null and on this player's controller entity)
- [x] 5.8 **RPC arity audit table** written into `context.md`, all 7 rows checked against their handlers; no `Rpc()` call wrapped in a helper
- [x] 5.9 `RemoveOrdered` wherever a line array's order is observable; `tools/compile-check.sh` exit 0

## Phase 6 — The transfer screen + actions (`ui-developer`, ~5-7 h) · Suite: **OVT_TEST_InitSuite** (8/8 complete)

- [x] 6.1 `Scripts/Game/UI/Context/OVT_ResourceTransferContext.c` — the eight `OVT_TransferContext` hooks of §3.6 + the `GetSummaryText()` override (the only base-virtual override)
- [x] 6.2 `Character_Player.et` — the context block, layout `{6A8E2C1000000001}`, `m_sContextName "OverthrowTransferContext"`, GUID from `6A8E2E1…`
- [x] 6.3 `Scripts/Game/UserActions/OVT_OpenResourceStoreAction.c` + entries on the truck delta, the pile prefab and the warehouse delta (Sort Priority 3)
- [x] 6.4 Take/Put vocabulary per D8; the destination picker shows ≥ 2 entries in Take mode (holder + Ground) sorted nearest-first
- [x] 6.5 The latch is set **before** the ask, never after (listen-host synchronicity)
- [x] 6.6 `OnClose` removes exactly what `OnShow` inserted, incl. the contents invoker and every pending `CallLater`
- [x] 6.7 Action labels read the replicated ledger, update with no menu open, 1 s cached
- [x] 6.8 `.st` keys for this phase (**never** `Configs/Language/*.conf`); `git diff --exit-code` clean on `OVT_TransferContext.c` / `OVT_TransferListModel.c` / `OVT_TransferCartModel.c`; `tools/compile-check.sh` exit 0

## Phase 7 — Price drift, difficulty, port category ⚠️ ADVANCED (`component-developer-advanced` 7.1-7.5, then `ui-developer` 7.6-7.8, ~6-7 h) · Suites: **OVT_TEST_LogicSuite**, **OVT_TEST_InitSuite** (4/9 complete)

- [x] 7.1 `OVT_ResourceRules.DriftStep` / `WarPressure` / `ApplyLevelMultiplier` / `SellPrice`
- [x] 7.2 Wire them into the manager's `CheckPrices` tick + the 6-hour latch + `RplSave`/`RplLoad` + `RpcDo_SetPrice`
- [x] 7.3 `OVT_DifficultySettings` — the three new `Economy` fields; four difficulty `.conf` overrides (§3.14)
- [x] 7.4 `OVT_OverthrowConfigComponent` — three accessors, `CONFIG_STREAM_VERSION` **5 → 6**, appends in **both** `RplSave` and `RplLoad`
- [x] 7.5 Logic cases for the whole drift surface (§7): composition order incl. level multiplier applied **after** the band clamp; volatility scales the step and **not** the band, asserted at both clamp edges; exactly one drift per 6-hour window
- [x] 7.6 `OVT_PortContext` — `CATEGORY_RESOURCES = 9`, resource rows in both modes, `GetCategoryLabelKey` special case
- [x] 7.7 `ValidateCart`/`OnAccept` `"res:"` prefix routing **before** any `GetInventoryId` call; the drift text in `FillDetails`
- [x] 7.8 Import lists no non-importable resource, Export lists them; disabled-with-a-reason rows when not in a truck store (F4)
- [x] 7.9 `.st` keys for this phase; grep the stream region for a stale `5`; `tools/compile-check.sh` exit 0

## Phase 8 — Construction ⚠️ ADVANCED (`component-developer-advanced`, ~8-10 h) · Suites: **OVT_TEST_LogicSuite**, **OVT_TEST_PersistenceRoundTripSuite** (12/12 complete ✅ — suites deferred to the final sweep)

- [x] 8.1 `Scripts/Game/Configuration/OVT_BuildablesConfig.c` — the two new `OVT_Buildable` fields + `OVT_BuildableResourceRequirement`
- [x] 8.2 `Scripts/Game/Data/OVT_ResourceRequirements.c` — the three **position-based** entry points of §3.8 (`NearbyAvailability(vector pos, …)`, `Consume(vector pos, …)`, the reusable readout) — `building-repair` calls these
- [x] 8.3 `OVT_ResistanceFactionManager` — the `BuildItem` / `FinishBuild(…, bool charge)` / `CompleteSite` split (D13) + `PlaceConstructionSite`; external `BuildItem` signature unchanged
- [x] 8.4 `Scripts/Game/Components/OVT_ConstructionSiteComponent.c`
- [x] 8.5 `Scripts/Game/Persistence/Serializers/Components/OVT_ConstructionSiteComponentSerializer.c` + binding 4's third serializer in `Overthrow.conf`
- [x] 8.6 `Prefabs/Structures/OVT_ConstructionSite.et` + the two user actions + the requirements readout dialog
- [x] 8.7 `Configs/Resistance/buildables.conf` — requirements on Garage, Helipad, Guard Tower (D17)
- [x] 8.8 `OVT_BuildContext` — scaled requirement rows on the card details
- [x] 8.9 Logic cases: requirement maths (`ScaleRequirement` never zeroes a non-zero requirement) + consumption order (deterministic, **squared** distances)
- [x] 8.10 Persistence case: a construction site round-trip
- [x] 8.11 Read-gates: empty requirement list byte-identical in behaviour; `FinishBuild` lines `:872-922` unchanged except the `if (charge)` wrapper; handler-failure path still returns before any charge; `m_OnBuild` fires exactly once at completion; `playerId == -1` never places a site; `Consume` all-or-nothing
- [x] 8.12 `.st` keys for this phase; `tools/compile-check.sh` exit 0

## Phase 9 — Warehouse resources + the buildable warehouse ⚠️ ADVANCED (`component-developer-advanced`, ~6-7 h) · Suites: **OVT_TEST_CampaignSuite**, **OVT_TEST_PersistenceRoundTripSuite** (9/9 complete ✅ — suites deferred to the final sweep)

- [x] 9.1 `Prefabs/Structures/Industrial/Houses/Warehouse_01/OVT_Warehouse.et` — `OVT_BuildableComponent` + `OVT_StructureDestructionComponent` (the `core/damage` retrofit, §3.10); `m_PhaseModel` a **bare `.xob`**
- [x] 9.2 `buildables.conf` — the Warehouse entry (`m_bBuildAtBase 1`, `m_bBuildInTown 1`, money 12000, the §3.9 requirement)
- [x] 9.3 Registration through `OVT_RealEstateManagerComponent.SetOwnerPersistentId` after build — **zero lines changed** in that file
- [x] 9.4 Town-control gate: pure predicate in `OVT_ResourceRules`, client reason in `OVT_BuildContext.CanBuild`, server re-check in `BuildItem`
- [x] 9.5 Binding 4 verified end-to-end — item ledger **and** resource stock on a *built* warehouse
- [x] 9.6 Campaign case: a built warehouse is registered exactly like a purchased one (`OVT_WarehouseData` present, on the map, `PlayerMayUseWarehouse` true for the builder); prefab path contains `Warehouse_01` and `GetConfig` matches it — asserted, not assumed
- [x] 9.7 Campaign case: a port purchase moves money **and** resources
- [x] 9.8 Persistence case: a **built** warehouse's stock survives a reload (binding 4's proof)
- [x] 9.9 `.st` keys for this phase; `tools/compile-check.sh` exit 0

## Phase 10 — Cargo HUD + map marker (`ui-developer`, ~4-5 h) · Suite: **OVT_TEST_InitSuite** (7/7 complete ✅ — suites deferred to the final sweep)

- [x] 10.1 `Scripts/Game/UI/HUD/OVT_CargoInfo.c` + `UI/Layouts/HUD/CargoInfo.layout`
- [x] 10.2 The truck delta's third InfoDisplay entry (completing the `SCR_BaseHUDComponent` array authored in 3.4)
- [x] 10.3 The HUD reads the **occupied truck's** ledger, never a player-scoped cache (BUG-097's shape avoided)
- [x] 10.4 `OVT_MapMarkerCategory.RESOURCE_PILE` **appended** to the enum + the marker on the pile prefab
- [x] 10.5 `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationResourcePile.c` + the `OverthrowMap.conf` block; reads pile contents from the pile's own component (no second source of truth); `m_fRefreshInterval` set deliberately
- [x] 10.6 One crate quad in `overthrow_mapicons.imageset`
- [x] 10.7 `.st` keys for this phase; `tools/compile-check.sh` exit 0

## Phase 11 — Localization, conflict check, help & wiki sync (main thread + `help-docs-sync`, ~2-3 h) · Suite: **none (announced skip — the suites assert nothing about `.st`, Field Manual or wiki content)** (6/6 complete ✅)

- [x] 11.1 `.st` audit — every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after**; fresh GUIDs; Id order; multi-line values use the trailing backslash
- [x] 11.2 Ask the user to re-export the localization `.conf` (**never** write it) — keys render raw until then · **ASKED; OWED BY THE USER**
- [x] 11.3 `check-input-conflicts.py` plain and `--warnings` — **only if** an input file was touched (the plan expects none)
- [x] 11.4 Fact-check every tutorial/help claim against a file:line before it ships
- [x] 11.5 `help-docs-sync` — Field Manual done (new **Resources** page + Ports/FOBs/Storage corrections, every claim `file:line`-cited). No new tutorial (justified). 🔴 **Wiki BLOCKED — no `wikijs` MCP server**; 7-item debt list in `context.md`
- [x] 11.6 Final DoD sweep: the 12 static verification steps of §6

---

## Cross-Phase Review (1/1 complete ✅)

- [x] R1 Cross-phase review against §6 Definition of Done (F1-F18, Q1-Q10, I1-I6) — **17 findings**: 3 fixed by the reviewer (5-way title-helper duplication, 4-way m³-formatter duplication, 3 stale-translation markers using a private convention), 2 fixed by the orchestrator (**F-5** the silent town-gate build failure, **F-12** the drift readout on non-Normal presets), 12 logged below

---

## Bugs & Issues

**Final gate — 2026-08-21, all five suites by class name, one at a time:**
| Suite | Result | Baseline |
|---|---|---|
| `OVT_TEST_LogicSuite` | **247/247** ✅ | 217 (+30) |
| `OVT_TEST_InitSuite` | **174/175** ⚠️ | 162/163 (+12) — the 1 red is pre-existing |
| `OVT_TEST_PersistenceRoundTripSuite` | **40/40** ✅ | 34 (+6) |
| `OVT_TEST_CampaignSuite` | **18/18** ✅ | 16 (+2) |
| `OVT_TEST_PersistenceSuite` | **13/13** ✅ | 13 (+0) |

**492 cases, 50 of them new, one pre-existing red.** `compile-check.sh` exit 0 (6252 files). `check-input-conflicts.py` exit 0 at the shipped baseline.

One red was **ours** and is fixed: `OVT_TEST_Init_ResourceSeam_LConstructionSiteSeam` asserted exactly 3 requirement-bearing buildables (D17), but Phase 9 then added the Warehouse, which §3.9 gives a requirement. A phase-ordering seam the cross-phase review did not catch. Assertion corrected to 4; the case is green.

**Active Bugs:**
- ⚠️ **`OVT_TEST_Init_Virtualization_AmbientRollCountOverrideIsCalled` times out under machine load** — Priority: Low, **not this feature's**. `TestResultTimeout` after 60 000 ms; green at Phase 5, red at Phase 6 and red again in isolation. Evidence it is environmental, not a resources regression: (a) the identical 173-case suite went 69 s → 125 s on the same machine with a Workbench instance up; (b) the case's budget is **1200 frames** while the harness gate is **60 wall-clock seconds**, so it needs ≥20 fps sustained and is marginal by construction; (c) the Init test world holds **2** truck/warehouse entities, so this feature's prefab edits cannot move its frame rate; (d) this feature has touched **no** virtualization file. Owner: `virtualization`/`civilians`. Re-check on a quiet machine before filing.

**Bug-report candidates (do not file from the plan — verify first):**
- `OVT_VehicleMenuContext.c:90` shows the port button at `dist < 20` while every server port gate is 30 m
- The Overthrow **Buildable** persistence config `{6B0E7A27C0D539F2}` carries no `OVT_StorageComponentSerializer` (fixed incidentally by task 4.3)
- `Warehouse_01_Workshop.et` inherits `Building_Base.et` directly — a real-estate warehouse with neither an item ledger nor a resource store
- `OVT_StorageComponent.EnsureTracked` is `Building.Cast`-gated (`:341`), so a *prop* holder is never lazily tracked

---

## Needs Human Verification

- (populated as phases land)

---

## Closed out, not done (3 of 98)

Honest accounting — these are ticked as *addressed*, not as *complete*:

- **11.2 — the `.st` re-export is OWED BY THE USER.** 81 new keys are in `Language/localization_Overthrow.st` (braces balanced 2104/2104). They render as raw `#OVT-…` text until the localization `.conf` files are re-exported from Workbench. **Never hand-write those exports.**
- **11.5 — the wiki half is BLOCKED**, not done. No `wikijs` MCP server exists in this session (the same block `logistics/ui`, `logistics/storage` and `economy/fuel` recorded). The Field Manual half **is** done, with every claim `file:line`-cited. A 7-item wiki debt list is in `context.md`.
- **10.6 — the crate map glyph is drawn into the atlas `.png`, but the `.edds` is Workbench build output.** The imageset loads the `.edds`, so the pile marker samples empty pixels until the texture is re-imported. Atlas dimensions are unchanged, so nothing existing breaks. **This is the one hard blocker on DoD F7.**

## Cross-phase review findings still open (12)

**Should-fix, handed over:**
- **F-4 — no resource icons exist.** `m_tIcon` is authored on none of the four resources and no `UI/Textures/Icons/resource_*.edds` is on disk. The member is read at `OVT_PortContext.c:1181` and `OVT_ResourceTransferContext.c:717`, both documenting `""` as valid, so rows render iconless rather than breaking. Plan §3.1 lists these as a deliverable ("one placeholder reused is acceptable in MVP") — unbuilt, and unreported by any phase. Product call + Workbench work.
- **F-6 — the map atlas `.edds`** (see 10.6 above).
- **F-7 — a construction site removed through the shipped removal flow is never untracked.** `CompleteSite` untracks deliberately; `RemovePlacedItem` → `DestroyPlacedItem` does not. **Pre-existing for every placeable and buildable in the mod** — neither method was touched here — but DoD F12 inherits the gap. A one-line fix in `DestroyPlacedItem` would change behaviour for every buildable, so it was left alone.

**Nice-to-have, handed over:**
- **F-8** — 9 dead accessors, kept as public surface `building-repair` may want.
- **F-9** — `OVT_ResourceLedger.m_OnChanged` is never allocated or subscribed. Plan-mandated dead code (§3.2 says "lazily allocated"; there is no allocator). Left exactly as the plan wrote it.
- **F-10** — two remaining 2-copy helpers (`FindResource`, `ResolveIcon`) not collapsed; doing so means moving three things, not one.
- **F-11 — Q6 edge, the only DoD quality item that fails.** On the P5-g path (dropping to ground while standing on the source pile) the same holder is bumped twice in one request. Harmless — `BumpMe` only marks dirty and both happen in one frame, so no transient goes on the wire — but literally what Q6 forbids. Unfixed because the ordering is P5-f's deliberate loss-path guard: the pile is filled *before* the source is drained so a failed spawn costs the player nothing. Reordering to satisfy Q6 reopens the loss path.
- **F-13** — a site outliving a `buildables.conf` requirement edit reads as unfinishable client-side while the server would complete it. Config-edit-only edge.
- **F-14 — three files carry edits unrelated to this feature** (print removals in two deployment modules and `OVT_DeploymentComponent.c`, the last leaving an unread local). **A concurrent session's, not this feature's** — confirm before committing.
- **F-15** — the `context.md` decision log stops at P6-g; phases 7–11 made calls the plan did not settle. Documentation gap, no code defect.
- **F-16 / F-17** — missing trailing newline on three files; the `SITE_REQUIREMENTS` dialog preset inserted mid-file rather than appended. Both harmless.
