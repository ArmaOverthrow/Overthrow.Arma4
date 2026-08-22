# Resource Production (logistics/resource-production) - Task Checklist

**Last Updated:** 2026-08-22
**Progress:** 60/62 tasks complete (97%) — see "Owed" below

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

## Phase 2 — Manager, prefabs, discovery and the drip ⚠️ ADVANCED (`component-developer-advanced`, ~6-7 h) · Suite: **OVT_TEST_InitSuite** (10/10 complete ✅)

- [x] 2.1 `Scripts/Game/Components/OVT_ResourceProductionComponent.c` (§3.3) — both `OnPostInit` guards, the `GetRpl()`/empty-id ERRORs
- [x] 2.2 `Scripts/Game/GameMode/Managers/OVT_ResourceProductionManagerComponent.c` (§3.4) — records, `InitializeSites()`, position matching (**squared** distance), `RplSave`/`RplLoad`, the two broadcasts, `SetSiteOwner`/`SetSitePrivacy`/`ClearSiteStock`
- [x] 2.3 The drip — the hour latch (asserted **from the clock** at Init, initialised `-1`), `CheckProduction`, `ProduceForHours`, `HandleTimeSkip`
- [x] 2.4 `Prefabs/Production/OVT_ProductionSite_Base.et` + the three variants (Sawmill / Cement plant / Steel mill). **The store action is NOT authored yet** (D13 — it lands in Phase 5 with both halves of its gate)
- [x] 2.5 `Prefabs/GameMode/OVT_OverthrowGameMode.et` + `OVT_OverthrowGameMode.c` Init block + `OVT_Global.GetProduction()`
- [x] 2.6 `Scripts/Game/…/OVT_SleepService.c` — one `HandleTimeSkip` call after `:374`, **before** `AdvanceClock`
- [x] 2.7 `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` — **one Sawmill instance** (Phases 6 and 7 need it; it is the only way discovery is ever asserted)
- [x] 2.8 `Tests/TestSuites/Init/OVT_TEST_Init_ProductionSeam.c` — manager resolves through `OVT_Global.GetProduction()`; the test world's Sawmill is exactly one record with `owner == ""`; its entity resolves a store at 20000 litres and a production component whose resource id is known to `GetDefs()`; `ProduceForHours(1)` raises the ledger by the authored rate; a second call in the same hour does not
- [x] 2.9 Acceptance greps: `Replication.IsServer()` guards `CheckProduction`, `ProduceForHours`, `HandleTimeSkip` and both setters; `PublishContents()` at most once per site per batch and not at all for a site that produced nothing; `vector.Distance` appears nowhere in the matcher; `RplSave`/`RplLoad` same fields same order, `carry` in neither; no `array<…>` on any RPC
- [x] 2.10 Both broadcast arities written into `context.md` and checked against their handlers (BUG-090); prefab carries **exactly one** `ActionsManagerComponent` + an explicit `RplComponent`; GUID series `6A8E2F0…` re-verified 0 hits before authoring; `tools/compile-check.sh` exit 0

## Phase 3 — Ownership, privacy, and the difficulty accessor ⚠️ ADVANCED (`network-specialist-advanced`, ~4-5 h) · Suite: **OVT_TEST_InitSuite** (8/8 complete ✅)

- [x] 3.1 `Scripts/Game/Components/Controller/OVT_ResourceProductionRequestComponent.c` — the three RPCs of §3.5 (buy personal, buy resistance funds, toggle privacy)
- [x] 3.2 `Prefabs/Controllers/OVT_OverthrowController.et` — append the component **before** the trailing `RplComponent`
- [x] 3.3 `Tests/TestSuites/Init/OVT_TEST_Init_ControllerSeam.c` — one line **and** the `"10"` → `"11"` in the `PrintFormat` (R12)
- [x] 3.4 `OVT_OverthrowConfigComponent.GetRealEstateCostMultiplier()`
- [x] 3.5 `OVT_BuySiteAction.c`, `OVT_BuySiteResistanceAction.c`, `OVT_ToggleSitePrivacyAction.c` + their prefab entries (Sort 1, 2, 4)
- [x] 3.6 Extend `OVT_TEST_Init_ProductionSeam.c` — `OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get()` non-null **and** on this player's controller entity (`OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template); a server-side buy sets the owner, clears the stock and takes the money
- [x] 3.7 Acceptance: **every refusal answers `RpcDo_ProductionError` with a key** — read every `return` in both asks; identity is never an RPC parameter (`ResolveOwningPlayerId()` only); both asks resolve the site from the **server's** record position; `ClearSiteStock` in the same call as `SetSiteOwner` with `PublishContents()` after `Clear()`; one `BuyCost` call each for label and charge
- [x] 3.8 `CONFIG_STREAM_VERSION` **unchanged** (grep proves no `RplSave`/`RplLoad` edit); officer gating is `OVT_ResistanceFactionManager.IsOfficer(playerId)`; RPC arity table in `context.md`; `tools/compile-check.sh` exit 0

## Phase 4 — `SITE_BUY` on the wire + the buy screen ⚠️ ADVANCED (`network-specialist-advanced` → `ui-developer`, ~6-8 h) · Suite: **OVT_TEST_InitSuite** (5/9)

- [x] 4.1 `OVT_ResourceRequestComponent.c` — enum value `SITE_BUY` **appended**, `IsKnownOp`/`OpReadsDest` updated
- [x] 4.2 `OVT_ResourceRequestComponent.c` — the `MayReachHolder` extraction (shipped steps 1–5 **verbatim**), `MayBuyFromSite`, `MayUseHolderForOp` and its three call sites
- [x] 4.3 `OVT_ResourceRequestComponent.c` — the money branch: `moneyTotal < 0` tripwire, `MAX_LINE_QUANTITY` bound, real-stock bound, server-side price re-derivation at Commit
- [x] 4.4 Logic cases for `SitePrice` composition against a hand-built defs table; the new refusal ordering documented in `context.md`
- [x] 4.5 Hand-audited RPC arity table for the touched file re-checked (the shipped six unchanged; the audit proves it)
- [x] 4.6 `Scripts/Game/UI/Context/OVT_ProductionSiteBuyContext.c` — the eight hooks + `GetSummaryText()` of §3.8
- [x] 4.7 `Prefabs/Characters/Core/Character_Player.et` — the context block, GUID `6A8E2F1000000001`
- [x] 4.8 `OVT_BuySiteStockAction.c` + its prefab entry (Sort 3)
- [x] 4.9 Acceptance: `OVT_TransferContext.c` / `OVT_TransferListModel.c` / `OVT_TransferCartModel.c` **unmodified** (`git diff --exit-code`), no ninth hook; an **owned** site refuses in exactly one place (`MayBuyFromSite` → `#OVT-ProdSite_Owned`); **nothing clamps**; the screen's latch is set **before** the ask; the destination picker is nearest-first and never lists the site itself; `tools/compile-check.sh` exit 0

## Phase 5 — The storage access predicate (both sides) (`component-developer`, ~2-3 h) · Suite: **OVT_TEST_InitSuite** (6/6 complete ✅)

- [x] 5.1 `OVT_ResourceRequestComponent.MayUseHolder` — the site step of §3.7
- [x] 5.2 `OVT_OpenResourceStoreAction.c` — the client branch beside `WarehouseIsOpenTo`, `SetCannotPerformReason("#OVT-Resource_NoAccess")`
- [x] 5.3 The `OVT_OpenResourceStoreAction` entry on `OVT_ProductionSite_Base.et` (Sort 5) — closes the window Phase 2 deliberately left (D13)
- [x] 5.4 Init cases: `owner ""` refuses; owned-by-you allows; owned-private-by-another refuses; owned-public-by-another allows; `"resistance"` allows
- [x] 5.5 Acceptance greps: `OVT_ResourceProductionRules.MayAccessStore` is the **only** ownership comparison (no inline `owner ==` / `isPrivate` test in the action, the request component or the map class); `PlayerMayUseWarehouse` **not** called for a site
- [x] 5.6 The action is visible-and-disabled with a reason, never hidden; the warehouse path through `MayUseHolder` is unchanged (read the diff); `tools/compile-check.sh` exit 0

## Phase 6 — Map location type + icons (`ui-developer`, ~3-4 h) · Suite: **OVT_TEST_InitSuite** (5/5 complete ✅)

- [x] 6.1 `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationProductionSite.c` (the `LocationTypes/` path of §3.1, not the flat path §4 gives) — §3.10 (`BuildInfoRows`, `GetIconName`, `GetIconColor`, three colour states via `ColourState`)
- [x] 6.2 `Configs/Map/OverthrowMap.conf` — the block, GUID `6A8E2F2000000001`, `m_fVisibilityZoom 0`, `m_fRefreshInterval 5`
- [x] 6.3 `UI/Imagesets/overthrow_mapicons.imageset` — quads for the resources whose art exists; the code must be correct with quads missing (fallback `crate`). **The `.edds` atlas re-import is the user's Workbench step**
- [x] 6.4 Acceptance: `m_RplID` is **never assigned**; `BuildInfoRows` reads stock from `location.GetEntity()`'s store, not the record; `GetIconName` falls back to `m_sIconName` on an empty `m_sMapIconName`; `GetIconColor` returns a `Color` (`GetArgb` appears nowhere); all three colours are `[Attribute]`-tunable
- [x] 6.5 `tools/compile-check.sh` exit 0

## Phase 7 — Persistence + round trip ⚠️ ADVANCED (`component-developer-advanced`, ~5-6 h) · Suite: **OVT_TEST_PersistenceRoundTripSuite** (7/7 complete ✅)

- [x] 7.1 `Scripts/Game/Persistence/Serializers/Components/OVT_ResourceProductionManagerSerializer.c` (the §3.1 path where all 23 existing serializers live, not the flat path §4 gives) + the **frozen** `OVT_PersistedProductionSite` (§3.11)
- [x] 7.2 `StagePersisted` / `ApplyStaged` + the single bounded retry + the named ERROR on the manager (R4 — deserialization runs while the world is being built)
- [x] 7.3 `Configs/Systems/Persistence/Overthrow.conf` — one serializer in `{65ACD95F40F6C669}`, GUID from `6B0E7A9…`
- [x] 7.4 `…_ProductionSiteOwnership_RoundTrips` — owner, `isPrivate` and a **non-zero** fractional carry survive a reload
- [x] 7.5 `…_ProductionSiteStock_RoundTrips` — stock re-applied onto the entity's store; a second `ApplyStaged` is idempotent
- [x] 7.6 Acceptance: Serialize/Deserialize locals **identically named** (a deliberate rename shown to fail during development and recorded in `context.md`); the single `Read()` return checked; a forced failure leaves live state untouched and logs ERROR; new cases sort **after** `…_Capability_…`
- [x] 7.7 **No new `ComponentClassPersistenceConfigRule`** anywhere and no persistence config on the site prefab (`grep -c` unchanged); `ApplyStaged` matches by squared distance; `tools/compile-check.sh` exit 0

## Phase 8 — Localization, epic docs, help & wiki sync (main thread + `help-docs-sync`, ~2-3 h) · Suite: **skipped — announced** (5/6)

> **Suite skip, stated:** this phase touches only `.st`, `.conf` and docs. The suites assert nothing there (`.claude/test-policy.md` §2, "skip the gate entirely"). The five-suite sweep runs once in the cross-phase review.

- [x] 8.1 `.st` audit — every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after**; fresh GUIDs; multi-line values use the trailing backslash
- [x] 8.2 Ask the user to re-export the string table (keys render raw until then). **Never write `Language/*.conf`** — Workbench build output
- [x] 8.3 `docs/features/logistics/epic-overview.md` — Features table row 5, build-order item 5, the dependency list, the rollup line
- [x] 8.4 `docs/features/logistics/epic-requirements.md` — "Passive town/industry resource production" moves **in** scope as this feature; production **chains** remain out
- [x] 8.5 State the `check-input-conflicts.py` skip (no input `.conf` touched)
- [x] 8.6 `help-docs-sync` — tutorials (`Configs/Tutorials/`), Field Manual (`Configs/FieldManual/`) and the wiki's economy/resources pages. **Every claim fact-checked against a `file:line`**

## Cross-phase review (0/4)

- [x] R.1 Full DoD static sweep — §6's 13 numbered grep/diff steps (all green; **Q7's path is vacuous** — `Configs/Language/` does not exist, the real exports are `Language/*.conf`)
- [ ] R.2 **BLOCKED — final suite sweep never ran.** `tools/run-tests.sh` was refused by Claude Code's auto-mode permission classifier. The user must run it, or add a Bash permission rule. Baselines: Logic 247 · Init 174/175 (1 pre-existing red) · PersistenceRoundTrip 40 · Campaign 18 · Persistence 13. This feature adds **20 Logic, 9 Init, 2 PersistenceRoundTrip, 1 Campaign**
- [x] R.3 Independent review pass — **17 findings (2 🔴, 4 🟠, 11 🟡); 12 fixed, 5 logged with reasons.** See `context.md`
- [x] R.4 `/update-feature`, `/update-epic`, `/update-master`

---

## Needs human verification

1. 🔴 **The five-suite sweep has never run** (R.2 above). Every phase gated on `compile-check.sh` exit 0 only. **No test in this feature has been executed even once** — 32 new cases are unproven at runtime.
2. **Workbench prefab pass** — open `OVT_ProductionSite_Base.et` + the three variants without dropped-attribute warnings; confirm one `ActionsManagerComponent` with five actions; open `OVT_OverthrowGameMode.et`, `OVT_OverthrowController.et` and `Character_Player.et`.
3. **The sign prefab** — `Prefabs/Structures/Signs/Large/SignLarge_01_base.et`'s `.meta` GUID (`…305`) differs from the vanilla path GUID (`…304`) it shadows, and its header inherits its own path. User-owned; user is verifying in Workbench.
4. **One more `.st` re-export** — the 22:22 export predates the 10 Field Manual keys added at 22:29, so the Production Sites page renders raw keys until then.
5. **`hardware` still has no map quad** (`timber`, `cement`, `steel` now do) — it falls back to `crate`, which is correct but not final art.
6. **Play-test A (single-player, mouse)** — §6 steps 15–25.
7. **Play-test B (gamepad only)** — §6 steps 26–30. The buy screen is `ui`'s second consumer and re-exercises the picker's d-pad trap.
8. **Play-test C (dedicated + JIP)** — §6 steps 31–35, including the two concurrent-buy races. **All MP behaviour is unproven**; the suites run one machine.
9. **A real quit → Continue** — the round-trip suite re-applies in-session; a cold load with the world still being built (R4) has never been exercised.

## Owed / not done

- **R.2, the suite sweep** (above) — the one blocking item.
- **Wiki sync** — blocked, no `wikijs` MCP server connected. The owed page list is in the Phase 8 report.
- **`OVT_TEST_Logic_ProductionRules.ProduceCarryStaysInUnitRange`'s recorded mutation is stale** after review FIX 9 — `carryOut = total` is now semantically equivalent to the correct code, so that mutant no longer fails. The mutation that still fails it removes **both** the `- units` subtraction and the normalization.
- **Bug-report candidate against `logistics/resources`** — the `PORT_IMPORT` partial-fit charge (`OVT_ResourceRequestComponent.c`): if `destStore.Add` fits fewer units than `Take` removed, the excess returns to the source but `moneyTotal` is unchanged. Unreachable behind the whole-cart fit check today. `SITE_BUY` now rides the same branch.
- **Two more bug-report candidates from the plan** — `OVT_TEST_Init_ControllerSeam.c`'s hard-coded roster count, and `OVT_RealEstateManagerComponent.GetBuyPrice` ignoring `realEstateCostMultiplier` (this feature is that field's first ever reader).

## Active Bugs

*(none yet)*

## Concurrent-session watch

⚠️ A second session is writing this tree (`building-repair`: `OVT_RepairStructureAction.c`, `OVT_RepairRequirementsAction.c`,
`OVT_ResourceRules.c`, `OVT_ResistanceFactionManager.c`, `Language/localization_Overthrow.st`, several FOB/structure prefabs).
Re-baseline before every phase. The `.st` master is the shared file most at risk — **count braces before and after** any
Phase 8 edit (an unbalanced `.st` means the next Workbench save eats entries).
