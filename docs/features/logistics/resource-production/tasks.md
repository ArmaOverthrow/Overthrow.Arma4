# Resource Production (logistics/resource-production) - Task Checklist

**Last Updated:** 2026-08-22
**Progress:** 7/62 tasks complete (11%)

> Phases **2, 3, 4, 7** are flagged **⚠️ ADVANCED** per implementation.md §4 / Agent Routing Summary.
> `tools/run-tests.sh` is the **orchestrator's** gate only — never an agent's. Suites are run **by class name**, one at a time (`.claude/test-policy.md`).
> Suite baselines to beat (from `logistics/resources` final gate, 2026-08-21): Logic **247/247** · Init **174/175** (one pre-existing red, `CompositionSlotGate_AcceptedTypesMatchTheCompositions`) · PersistenceRoundTrip **40/40** · Campaign **18/18** · Persistence **13/13**.
> ⚠️ **User decided 2026-08-22 to hold all `run-tests.sh` runs until the end of the autorun** (a Workbench process is open; concurrent Workbench has produced false exit-2 INDETERMINATE runs). Per-phase gate is `compile-check.sh` only; the full suite sweep runs once at the end.
> Re-baseline (`git status`) before every phase — `OVT_ResourceRequestComponent.c`, `OVT_OverthrowGameMode.c` and `Overthrow.conf` are the three shared files most likely to have moved (R7, R14).

---

## Phase 1 — The pure spine + the icon field (`component-developer`, ~3-4 h) · Suite: **OVT_TEST_LogicSuite** (7/7 complete ✅)

- [x] 1.1 `Scripts/Game/Data/OVT_ResourceProductionRules.c` — all ten statics of §3.2 (`SitePrice`, `BuyCost`, `MayAccessStore`, `MayTogglePrivacy`, `Produce`, `FitProduction`, `ShouldProduce`, `ColourState`, + the `MAX_SKIP_HOURS` clamp)
- [x] 1.2 `Scripts/Game/Configuration/OVT_ResourcesConfig.c` — `string m_sMapIconName` **appended** to `OVT_Resource` after `m_iIllegal`
- [x] 1.3 `Configs/Resistance/resources.conf` — the icon names (only quads that exist in `overthrow_mapicons.imageset`; leave the rest empty so `GetIconName` falls back to `crate`)
- [x] 1.4 `Tests/TestSuites/Logic/OVT_TEST_Logic_ProductionRules.c` — the 17 cases of §7's table, registered on `OVT_TEST_LogicSuite`
- [x] 1.5 Tier hygiene: **no manager accessor and no game-mode getter identifier anywhere under `TestSuites/Logic/`**, comments included; `new` sets every field explicitly; no ternaries; polls are preconditions; no `maxAttempts`
- [x] 1.6 Fail-prove every case once; record the mutation + resulting message in `context.md`
- [x] 1.7 Acceptance greps: `SitePrice` never returns 0 and never touches `OVT_ResourceManagerComponent`; `Produce` carry in `[0,1)` for every row incl. a 720-hour skip and a 0.05 rate; `grep -c "OVT_Resource \"" resources.conf` still 4; `tools/compile-check.sh` exit 0

## Phase 2 — Manager, prefabs, discovery and the drip ⚠️ ADVANCED (`component-developer-advanced`, ~6-7 h) · Suite: **OVT_TEST_InitSuite** (0/10)

- [ ] 2.1 `Scripts/Game/Components/OVT_ResourceProductionComponent.c` (§3.3) — both `OnPostInit` guards, the `GetRpl()`/empty-id ERRORs
- [ ] 2.2 `Scripts/Game/GameMode/Managers/OVT_ResourceProductionManagerComponent.c` (§3.4) — records, `InitializeSites()`, position matching (**squared** distance), `RplSave`/`RplLoad`, the two broadcasts, `SetSiteOwner`/`SetSitePrivacy`/`ClearSiteStock`
- [ ] 2.3 The drip — the hour latch (asserted **from the clock** at Init, initialised `-1`), `CheckProduction`, `ProduceForHours`, `HandleTimeSkip`
- [ ] 2.4 `Prefabs/Production/OVT_ProductionSite_Base.et` + the three variants (Sawmill / Cement plant / Steel mill). **The store action is NOT authored yet** (D13 — it lands in Phase 5 with both halves of its gate)
- [ ] 2.5 `Prefabs/GameMode/OVT_OverthrowGameMode.et` + `OVT_OverthrowGameMode.c` Init block + `OVT_Global.GetProduction()`
- [ ] 2.6 `Scripts/Game/…/OVT_SleepService.c` — one `HandleTimeSkip` call after `:374`, **before** `AdvanceClock`
- [ ] 2.7 `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` — **one Sawmill instance** (Phases 6 and 7 need it; it is the only way discovery is ever asserted)
- [ ] 2.8 `Tests/TestSuites/Init/OVT_TEST_Init_ProductionSeam.c` — manager resolves through `OVT_Global.GetProduction()`; the test world's Sawmill is exactly one record with `owner == ""`; its entity resolves a store at 20000 litres and a production component whose resource id is known to `GetDefs()`; `ProduceForHours(1)` raises the ledger by the authored rate; a second call in the same hour does not
- [ ] 2.9 Acceptance greps: `Replication.IsServer()` guards `CheckProduction`, `ProduceForHours`, `HandleTimeSkip` and both setters; `PublishContents()` at most once per site per batch and not at all for a site that produced nothing; `vector.Distance` appears nowhere in the matcher; `RplSave`/`RplLoad` same fields same order, `carry` in neither; no `array<…>` on any RPC
- [ ] 2.10 Both broadcast arities written into `context.md` and checked against their handlers (BUG-090); prefab carries **exactly one** `ActionsManagerComponent` + an explicit `RplComponent`; GUID series `6A8E2F0…` re-verified 0 hits before authoring; `tools/compile-check.sh` exit 0

## Phase 3 — Ownership, privacy, and the difficulty accessor ⚠️ ADVANCED (`network-specialist-advanced`, ~4-5 h) · Suite: **OVT_TEST_InitSuite** (0/8)

- [ ] 3.1 `Scripts/Game/Components/Controller/OVT_ResourceProductionRequestComponent.c` — the three RPCs of §3.5 (buy personal, buy resistance funds, toggle privacy)
- [ ] 3.2 `Prefabs/Controllers/OVT_OverthrowController.et` — append the component **before** the trailing `RplComponent`
- [ ] 3.3 `Tests/TestSuites/Init/OVT_TEST_Init_ControllerSeam.c` — one line **and** the `"10"` → `"11"` in the `PrintFormat` (R12)
- [ ] 3.4 `OVT_OverthrowConfigComponent.GetRealEstateCostMultiplier()`
- [ ] 3.5 `OVT_BuySiteAction.c`, `OVT_BuySiteResistanceAction.c`, `OVT_ToggleSitePrivacyAction.c` + their prefab entries (Sort 1, 2, 4)
- [ ] 3.6 Extend `OVT_TEST_Init_ProductionSeam.c` — `OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get()` non-null **and** on this player's controller entity (`OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template); a server-side buy sets the owner, clears the stock and takes the money
- [ ] 3.7 Acceptance: **every refusal answers `RpcDo_ProductionError` with a key** — read every `return` in both asks; identity is never an RPC parameter (`ResolveOwningPlayerId()` only); both asks resolve the site from the **server's** record position; `ClearSiteStock` in the same call as `SetSiteOwner` with `PublishContents()` after `Clear()`; one `BuyCost` call each for label and charge
- [ ] 3.8 `CONFIG_STREAM_VERSION` **unchanged** (grep proves no `RplSave`/`RplLoad` edit); officer gating is `OVT_ResistanceFactionManager.IsOfficer(playerId)`; RPC arity table in `context.md`; `tools/compile-check.sh` exit 0

## Phase 4 — `SITE_BUY` on the wire + the buy screen ⚠️ ADVANCED (`network-specialist-advanced` → `ui-developer`, ~6-8 h) · Suite: **OVT_TEST_InitSuite** (0/9)

- [ ] 4.1 `OVT_ResourceRequestComponent.c` — enum value `SITE_BUY` **appended**, `IsKnownOp`/`OpReadsDest` updated
- [ ] 4.2 `OVT_ResourceRequestComponent.c` — the `MayReachHolder` extraction (shipped steps 1–5 **verbatim**), `MayBuyFromSite`, `MayUseHolderForOp` and its three call sites
- [ ] 4.3 `OVT_ResourceRequestComponent.c` — the money branch: `moneyTotal < 0` tripwire, `MAX_LINE_QUANTITY` bound, real-stock bound, server-side price re-derivation at Commit
- [ ] 4.4 Logic cases for `SitePrice` composition against a hand-built defs table; the new refusal ordering documented in `context.md`
- [ ] 4.5 Hand-audited RPC arity table for the touched file re-checked (the shipped six unchanged; the audit proves it)
- [ ] 4.6 `Scripts/Game/UI/Context/OVT_ProductionSiteBuyContext.c` — the eight hooks + `GetSummaryText()` of §3.8
- [ ] 4.7 `Prefabs/Characters/Core/Character_Player.et` — the context block, GUID `6A8E2F1000000001`
- [ ] 4.8 `OVT_BuySiteStockAction.c` + its prefab entry (Sort 3)
- [ ] 4.9 Acceptance: `OVT_TransferContext.c` / `OVT_TransferListModel.c` / `OVT_TransferCartModel.c` **unmodified** (`git diff --exit-code`), no ninth hook; an **owned** site refuses in exactly one place (`MayBuyFromSite` → `#OVT-ProdSite_Owned`); **nothing clamps**; the screen's latch is set **before** the ask; the destination picker is nearest-first and never lists the site itself; `tools/compile-check.sh` exit 0

## Phase 5 — The storage access predicate (both sides) (`component-developer`, ~2-3 h) · Suite: **OVT_TEST_InitSuite** (0/6)

- [ ] 5.1 `OVT_ResourceRequestComponent.MayUseHolder` — the site step of §3.7
- [ ] 5.2 `OVT_OpenResourceStoreAction.c` — the client branch beside `WarehouseIsOpenTo`, `SetCannotPerformReason("#OVT-Resource_NoAccess")`
- [ ] 5.3 The `OVT_OpenResourceStoreAction` entry on `OVT_ProductionSite_Base.et` (Sort 5) — closes the window Phase 2 deliberately left (D13)
- [ ] 5.4 Init cases: `owner ""` refuses; owned-by-you allows; owned-private-by-another refuses; owned-public-by-another allows; `"resistance"` allows
- [ ] 5.5 Acceptance greps: `OVT_ResourceProductionRules.MayAccessStore` is the **only** ownership comparison (no inline `owner ==` / `isPrivate` test in the action, the request component or the map class); `PlayerMayUseWarehouse` **not** called for a site
- [ ] 5.6 The action is visible-and-disabled with a reason, never hidden; the warehouse path through `MayUseHolder` is unchanged (read the diff); `tools/compile-check.sh` exit 0

## Phase 6 — Map location type + icons (`ui-developer`, ~3-4 h) · Suite: **OVT_TEST_InitSuite** (0/5)

- [ ] 6.1 `Scripts/Game/UI/Map/OVT_MapLocationProductionSite.c` — §3.10 (`BuildInfoRows`, `GetIconName`, `GetIconColor`, three colour states via `ColourState`)
- [ ] 6.2 `Configs/Map/OverthrowMap.conf` — the block, GUID `6A8E2F2000000001`, `m_fVisibilityZoom 0`, `m_fRefreshInterval 5`
- [ ] 6.3 `UI/Imagesets/overthrow_mapicons.imageset` — quads for the resources whose art exists; the code must be correct with quads missing (fallback `crate`). **The `.edds` atlas re-import is the user's Workbench step**
- [ ] 6.4 Acceptance: `m_RplID` is **never assigned**; `BuildInfoRows` reads stock from `location.GetEntity()`'s store, not the record; `GetIconName` falls back to `m_sIconName` on an empty `m_sMapIconName`; `GetIconColor` returns a `Color` (`GetArgb` appears nowhere); all three colours are `[Attribute]`-tunable
- [ ] 6.5 `tools/compile-check.sh` exit 0

## Phase 7 — Persistence + round trip ⚠️ ADVANCED (`component-developer-advanced`, ~5-6 h) · Suite: **OVT_TEST_PersistenceRoundTripSuite** (0/7)

- [ ] 7.1 `Scripts/Game/Persistence/OVT_ResourceProductionManagerSerializer.c` + the **frozen** `OVT_PersistedProductionSite` (§3.11)
- [ ] 7.2 `StagePersisted` / `ApplyStaged` + the single bounded retry + the named ERROR on the manager (R4 — deserialization runs while the world is being built)
- [ ] 7.3 `Configs/Systems/Persistence/Overthrow.conf` — one serializer in `{65ACD95F40F6C669}`, GUID from `6B0E7A9…`
- [ ] 7.4 `…_ProductionSiteOwnership_RoundTrips` — owner, `isPrivate` and a **non-zero** fractional carry survive a reload
- [ ] 7.5 `…_ProductionSiteStock_RoundTrips` — stock re-applied onto the entity's store; a second `ApplyStaged` is idempotent
- [ ] 7.6 Acceptance: Serialize/Deserialize locals **identically named** (a deliberate rename shown to fail during development and recorded in `context.md`); the single `Read()` return checked; a forced failure leaves live state untouched and logs ERROR; new cases sort **after** `…_Capability_…`
- [ ] 7.7 **No new `ComponentClassPersistenceConfigRule`** anywhere and no persistence config on the site prefab (`grep -c` unchanged); `ApplyStaged` matches by squared distance; `tools/compile-check.sh` exit 0

## Phase 8 — Localization, epic docs, help & wiki sync (main thread + `help-docs-sync`, ~2-3 h) · Suite: **none (announce the skip)** (0/6)

- [ ] 8.1 `.st` audit — every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after**; fresh GUIDs; multi-line values use the trailing backslash
- [ ] 8.2 Ask the user to re-export the string table (keys render raw until then). **Never write `Language/*.conf`** — Workbench build output
- [ ] 8.3 `docs/features/logistics/epic-overview.md` — Features table row 5, build-order item 5, the dependency list, the rollup line
- [ ] 8.4 `docs/features/logistics/epic-requirements.md` — "Passive town/industry resource production" moves **in** scope as this feature; production **chains** remain out
- [ ] 8.5 State the `check-input-conflicts.py` skip (no input `.conf` touched)
- [ ] 8.6 `help-docs-sync` — tutorials (`Configs/Tutorials/`), Field Manual (`Configs/FieldManual/`) and the wiki's economy/resources pages. **Every claim fact-checked against a `file:line`**

## Cross-phase review (0/4)

- [ ] R.1 Full DoD static sweep — §6's 13 numbered grep/diff steps
- [ ] R.2 Final suite sweep — Logic, Init, PersistenceRoundTrip, Campaign, Persistence, by class name, one at a time
- [ ] R.3 Independent review pass over the whole feature diff; findings fixed or logged
- [ ] R.4 `/update-feature`, `/update-epic`, `/update-master`

---

## Needs human verification

*(populated as phases land — Workbench prefab pass, `.st` re-export, `.edds` re-import, and play-tests A/B/C from §6 steps 15–35)*

## Active Bugs

*(none yet)*
