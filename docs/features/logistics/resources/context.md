# Resources (logistics/resources) - Context & Decisions

**Last Updated:** 2026-08-21
**Current Phase:** ✅ All 11 phases + cross-phase review complete — Ready for Review
**Status:** ✅ Ready for Review (built 2026-08-21; play-test + `.st` re-export + `.edds` re-import owed)

---

## Quick Status

**What's Done:**
- ✅ Requirements formalized (2026-08-20)
- ✅ Implementation plan written (2026-08-21) — 11 phases, approach A, 20 recorded decisions (D1–D20)
- ✅ Feature started; `tasks.md` scaffolded (98 tasks across 11 phases + 1 cross-phase review)
- ✅ **Phase 1 — the pure spine** (8/8). 6 files, 22 Logic cases, every case fail-proven. Gate: `compile-check.sh` exit 0 (6232 files) · `OVT_TEST_LogicSuite` **239/239** (baseline 217 + 22)

- ✅ **Phase 2 — config + manager + `OVT_Global`** (7/7). 5 files + 2 edits. Gate: `compile-check.sh` exit 0 (6235 files) · `OVT_TEST_InitSuite` **166/167** — the single red is the pre-existing `CompositionSlotGate_AcceptedTypesMatchTheCompositions` (base-defense/deployments), red at baseline. All 4 new cases pass.

- ✅ **Phase 3 — store component + prefabs** (10/10). 3 new scripts + 2 new prefabs (+ `.meta`) + 6 prefab edits + 5 new Init cases. Gate: `compile-check.sh` exit 0 (6238 files) · `OVT_TEST_InitSuite` **171/172** (same pre-existing red; all 5 new cases pass). Vanilla-vs-delta parent line proven byte-identical; `Replication.BumpMe` has exactly one call site; the seven walled files `git diff --exit-code` clean.

- ✅ **Phase 4 — persistence** (9/9; 4.4 had already landed in Phase 3). 2 new serializers + 1 manager method + 5 `Overthrow.conf` bindings + 4 round-trip cases. Gate: `compile-check.sh` exit 0 (6240 files) · `OVT_TEST_PersistenceRoundTripSuite` **38/38** (baseline 34 + 4 new). Exactly four `ComponentClassPersistenceConfigRule`s, none naming `OVT_ResourceStoreComponent`. An **existing save is safe** — no payload means `version` stays 0 and the guard returns before any `Read()`.

- ✅ **Phase 5 — request component, wire, pile spawn/merge** (9/9). `OVT_ResourceRequestComponent` (1215 L, 6 RPCs) + the manager's `RpcDo_SetPrice` broadcast + `SpawnOrMergePile` + pile cleanup + the controller-seam Init case. Gate: `compile-check.sh` exit 0 (6241 files) · `OVT_TEST_InitSuite` **172/173** (same pre-existing red). **RPC arity audit: all 7 rows ✓**, produced mechanically. 46 returns across the four asks, 37 answering; the 9 non-answering are all `!IsServer()`, an already-answered checkout, or a deferred malformed-line flag — none is a silent refusal. **20 `.st` keys owed to Phase 11.**

- ✅ **Phase 6 — the transfer screen + actions** (8/8). `OVT_ResourceTransferContext` (765 L, the eight hooks + the `GetSummaryText()` override) + `OVT_OpenResourceStoreAction` (one class, three hosts) + the `Character_Player.et` context block + **34 `.st` entries** (braces 1942→2010, balanced). Gate: `compile-check.sh` exit 0 (6243 files) · `OVT_TEST_InitSuite` **171/173** — the known pre-existing red plus one **environmental timeout** in virtualization (see Active Bugs in `tasks.md`; not a resources regression). All seven walled files `git diff --exit-code` clean. Input `.conf` untouched; the conflict checker still reports the shipped baseline.

- ✅ **Phase 7 — price drift, difficulty, port category** (9/9, two agents). Part A: the drift tick + 6-hour latch + manager `RplSave`/`RplLoad`, three `Economy` difficulty fields + four `.conf` overrides, `CONFIG_STREAM_VERSION` **5 → 6** with positionally symmetric appends in both directions, 3 new Logic cases + 1 Init case (all fail-proven). Part B: `CATEGORY_RESOURCES = 9` on `OVT_PortContext` in both modes, `"res:"` routing **before** `GetInventoryId`, the drift readout, 12 `.st` keys (braces 2010 → 2034, balanced). Gate: `compile-check.sh` exit 0 (6243 files) · `OVT_TEST_LogicSuite` **242/242**. ⚠️ **Init has no verdict for this phase** — the run returned exit 2 (INDETERMINATE, no `junit.xml`), which is not a pass; re-run in the final sweep.

- ✅ **Phase 8 — construction** (12/12). 7 new files + 14 edits: the `BuildItem`/`FinishBuild(…, bool charge)`/`CompleteSite` split, `OVT_ResourceRequirements` (the three **frozen** position-based helpers `building-repair` is planned against), the site component + serializer + prefab + two actions, requirements on Guard Tower / Helipad / Garage, scaled rows on the build card, 6 new suite cases, 9 `.st` keys (braces 2034 → 2052). Gate: `compile-check.sh` exit 0 (6248 files); **suites deferred to the final sweep** at the user's request. The moved `:872-922` region diffs to **exactly one delta** — the `if (charge)` wrapper — with the handler-failure path still returning before any charge.

- ✅ **Phase 9 — warehouse resources + the buildable warehouse** (9/9). `OVT_Warehouse.et` (+ `.meta`) on vanilla `Warehouse_01.et`, the `buildables.conf` entry **appended at index 8** (so no saved `buildableIndex` shifts), the town-control gate on **both** sides of the wire over a pure Logic-tested predicate, registration through `SetOwnerPersistentId` at the tail of `FinishBuild`, 2 new Campaign cases + 1 Logic case + the built-warehouse Persistence case, 3 `.st` keys (braces 2052 → 2058). Gate: `compile-check.sh` exit 0 (6250 files); **suites deferred to the final sweep**. **Zero lines changed in `OVT_RealEstateManagerComponent.c`**; `core/damage` untouched (I4).

- ✅ **Phase 10 — cargo HUD + map marker** (7/7). `OVT_CargoInfo` + `CargoInfo.layout` + the truck delta's **third** InfoDisplay (completing Phase 3's array), `RESOURCE_PILE` **appended** to `OVT_MapMarkerCategory`, the marker on the pile prefab, `OVT_MapLocationResourcePile` + the `OverthrowMap.conf` block at `m_fRefreshInterval 5`, a crate quad, 9 `.st` keys (braces 2058 → 2076). Gate: `compile-check.sh` exit 0 (6252 files); **suites deferred to the final sweep**. The HUD is **vehicle-scoped** (its `owner` is the truck) and holds no player-scoped state, so BUG-097's shape cannot arise. 🔴 **The crate glyph is drawn into the atlas `.png` but the `.edds` is Workbench build output — the marker draws empty until you reimport the texture.**

- ✅ **Phase 11 — localization, conflict check, help & wiki sync** (6/6). `.st` audit: 165 keys across 61 changed files, **all resolve**, braces balanced 2104/2104. New Field Manual **Resources** page + Ports/FOBs/Storage corrections, **every claim `file:line`-cited**; no new tutorial (justified — the existing `buildFirstStructure` entry fires at the right moment and was factually wrong, so it was corrected instead). Input-conflict checker at the shipped baseline. 🔴 Wiki BLOCKED (no `wikijs` MCP server) — 7-item debt list below.
- ✅ **Cross-phase review** — 17 findings, 5 fixed, 12 logged in `tasks.md`.
- ✅ **Final gate, all five suites:** Logic **247/247** · Init **174/175** (1 pre-existing red) · PersistenceRoundTrip **40/40** · Campaign **18/18** · Persistence **13/13**. **492 cases, 50 new.**

**What's Next:**
- 👤 **User:** `.st` re-export (Workbench), atlas `.edds` re-import (Workbench), the Workbench prefab pass, and the three play-tests (A single-player, B gamepad, C dedicated + JIP)

**Blockers:**
- None

---

## Key Files

### Plan & requirements
- `docs/features/logistics/resources/implementation.md` — the authority; §3 architecture, §4 phases, §5 D1–D20, §6 DoD
- `docs/features/logistics/resources/requirements.md` — scope authority (sections A–F + Out of Scope, 7 user decisions)
- `docs/features/logistics/epic-overview.md` — epic build order and the two-ledgers wall

### Epic siblings this feature sits on
- `docs/features/logistics/ui/implementation.md` §3.4 — the **closed** eight-hook `OVT_TransferContext` contract
- `docs/features/logistics/ui/context.md` — gamepad traps (`WLib_NavigationButton` not focusable without an override; the picker eats d-pad left/right; `array.Remove` is swap-with-last)
- `docs/features/logistics/storage/implementation.md` — the item ledger this feature must not touch (I1)

### Core implementation (created as phases land)
- `Scripts/Game/Data/OVT_ResourceLedger.c` · `OVT_ResourceDefs.c` · `OVT_ResourcePack.c` · `OVT_ResourceRules.c` · `OVT_ResourceRequirements.c`
- `Scripts/Game/Components/OVT_ResourceManagerComponent.c` · `OVT_ResourceStoreComponent.c` · `OVT_ResourcePileComponent.c` · `OVT_ConstructionSiteComponent.c`
- `Scripts/Game/Components/Controller/OVT_ResourceRequestComponent.c`
- `Scripts/Game/UI/Context/OVT_ResourceTransferContext.c`
- `Configs/Resistance/resources.conf` · `Configs/Systems/Persistence/Overthrow.conf`

---

## Standing Constraints (read before every phase)

1. **The `storage` wall (I1).** `OVT_StorageLedger.c`, `OVT_StorageComponent.c`, `OVT_StorageRequestComponent.c`, `OVT_StorageContext.c`, `OVT_TransferContext.c`, `OVT_TransferListModel.c`, `OVT_TransferCartModel.c` stay **unmodified** — `git diff --exit-code` proves it. If a base change seems necessary, stop and raise it as a plan defect.
2. **`OVT_EconomyManagerComponent.c` and `OVT_RealEstateManagerComponent.c` are read-only (I2).**
3. **Never write `Configs/Language/*.conf`** — they are Workbench build output. Edit only the `.st` master and ask the user to re-export.
4. **`tools/run-tests.sh` is the orchestrator's**, by class name, one suite at a time. Agents gate on `tools/compile-check.sh` exit 0 only. ⚠️ **From Phase 8 the user asked to hold every suite run until the end of the autorun** — per-phase gating is `compile-check.sh` only, with one full sweep at the finish.
5. **No git writes** anywhere in this run. Everything is left uncommitted in the working tree.
6. Fresh GUIDs come from `6A8E2E…` (prefabs/configs) / `6B0E7A7…` (persistence bindings); inherited component GUIDs are **copied, never minted**.

---

## Important Decisions

The 20 load-bearing decisions (D1–D20) live in `implementation.md` §5 and are **not** restated here. This section records only decisions made *during implementation* that the plan did not settle.

- **P2-a — the config class lives in `Scripts/Game/Configuration/`, not `Scripts/Game/Config/`.** `tasks.md` 2.1 names a directory that does not exist; §3.1 and every shipped config class use `Configuration/`.
- **P2-b — `GetPrice()` composes through a `GetPriceMultiplier()` hook returning 1.0.** The difficulty accessor lands in Phase 7; the composition order (multiplier after the band clamp) is already in place, so Phase 7 replaces one constant.
- **P2-c — the manager's config member is `m_ResourcesConfig`.** `OVT_Component` already owns a protected `m_Config` (the Overthrow config component); reusing the name would shadow it.
- **P2-d — the four `#OVT-Resource_*` title/description keys are NOT in the `.st` yet.** Phase 11 owns the `.st` pass; nothing displays them before Phase 6.

- **P3-a — `OVT_PersistedResourceLine` is declared in `OVT_ResourceStoreComponent.c`, and `ApplyPersisted` takes it verbatim.** §3.3's signature is honoured exactly. The record is spelled as §3.11 froze it (`string resourceId; int quantity;`, no `m_` prefix — the field names *are* the save format) and is declared as a bare class, matching the shipped `OVT_PersistedStorageLine`. **Phase 4 must reference it, not re-declare it** — a duplicate is a compile error, so the failure is loud, but the intent is that the serializer maps its payload onto this record.

- **P3-b — the truck delta restates TWO InfoDisplays now; Phase 10 adds the third (task 10.2 already says so).** The plan allowed either option provided the array is complete at every commit. Authoring an `OVT_CargoInfo` placeholder would name a script class that does not exist yet, which makes the entry *invalid* rather than merely incomplete, so the two inherited entries (`OVT_WantedInfo {59B70A6E01375E1B}`, `OVT_EconomyInfo {59B70A616D989C46}`, GUIDs copied from `Wheeled_Base.et:12-23`) are restated alone. Nothing is lost under either array semantics.

- **P3-c — the store's client-side mirror is rebuilt lazily, not only from the `RplProp` callback.** `m_sMirrorSource` records the packed string the mirror reflects; every client-safe read calls `EnsureMirror()`, which rebuilds when it has drifted. `EnsureMirror()` early-returns on `Replication.IsServer()`, so the authority's ledger — the truth the packed string is derived *from* — can never be clobbered by a decode. This makes JIP correct without depending on `onRplName` firing for streamed-in state.

- **P3-d — `EnsureTracked()` landed with `PublishContents()` in Phase 3, not Phase 4.** §3.3 specifies it as part of `PublishContents`; task 4.4 assigns it to Phase 4. It is implemented (latched, server-only, behind `OVT_PersistenceTracking.IsTracked`) and is inert until Phase 5 first puts stock in a holder. **Task 4.4 is already done.** It deliberately has no `Building.Cast()` class test (the shipped storage component's optimisation) because a pile is a prop and a truck is a vehicle: both latch out on the `IsTracked` check instead.

- **P3-e — the manager's `m_rPilePrefab` was wired on the game-mode prefab in Phase 3.** Not on the task list; done because the pile prefab now exists and an unwired `ResourceName` is exactly the silent gap the Init seam suite exists to catch. Phase 5's `SpawnOrMergePile` needs no prefab edit.

- **P3-f — `#OVT-Resource_Pile` is authored on the pile prefab but is NOT in the `.st` yet**, continuing P2-d. Phase 11 owns the `.st` pass; the key first renders in Phase 6.

- **P4-a — the price payload is applied through a new `OVT_ResourceManagerComponent.ApplyPersistedPrices(array<string>, array<int>)`.** The shipped convention is "the serializer reads, the manager applies" (`ApplyPersistedEconomy`, `ApplyPersistedFactionResources`). It **resets every index to base and then refills**, which is what makes §3.11's "an id the save does not know starts at base" literally true and makes the apply idempotent under `ReapplyLatestSaveData` on a live session. `SetStoredPrice` (P2-e) remains the hand-drivable setter the round-trip case uses.

- **P4-b — both serializers refuse a non-empty payload when the resource catalogue is empty.** Not in §3.11, added after tracing the failure. `OVT_ResourceLedger.Add` silently ignores an id the definition table does not know, and `OVT_ResourceManagerComponent` always hands out a non-null `OVT_ResourceDefs`, so a broken `resources.conf` would have taken `ApplyPersisted` down the normal path: clear the ledger, drop every saved line, `PublishContents()` — a **silent wipe of every truck, pile and warehouse from one config typo**. The guard aborts with an ERROR and leaves live state untouched, the same contract as a failed `Read()`. **Residual, stated deliberately:** the *next* save then writes the (still empty) live ledger, because nothing latches "this holder never loaded". A load-failure latch is out of Phase 4's scope; the ERROR is the only warning.

- **P4-c — binding 4 landed with two of its three serializers.** `OVT_ResourceStoreComponentSerializer` and `OVT_StorageComponentSerializer` are in the Buildable block now (D15); `OVT_ConstructionSiteComponentSerializer` is Phase 8's line and the block is otherwise complete.

- **P4-d — the four new cases cover bindings 1, 2, 3 and 5. Binding 4 has NO case in this phase**, and cannot: a built holder needs `OVT_BuildableComponent`, and the buildable warehouse arrives in Phase 9, which owns §7's `…_BuiltWarehouseResources_RoundTrip`. Until then the Buildable block's two additions are proven by reading only.

- **P4-e — the pile case's "not tracked at spawn" check is a `Print` note, not a failure.** The pile's whole prefab chain (`OVT_ResourcePile.et` → `CrateStack_01_base.et` → `DestructibleMultiPhase_Props_Base.et`) provably carries no native `Persistence` component, so `EnsureTracked()` really is the only route — but the shipped `…_StorageWarehouse_…` case uses a note for exactly this check, so a later prefab edit that adds a `Persistence` component cannot turn the case red for a non-defect. The load-bearing assertion (tracked **before** the save) is unchanged.

- **P4-f — the two mutation artifacts below are COMPILE proofs, not run proofs.** Agents may not run `tools/run-tests.sh` (`.claude/test-policy.md`), and there is no other runtime harness, so each mutation was applied, compiled, and reverted; the runtime signature and the exact assertion that catches it are recorded from the case source. The orchestrator's suite run is what converts them into observed failures.

---

- **P5-a — the `Rpc()` call lives in a per-handler `Send…` method, and that IS the audited shape.**
  §3.5 forbids wrapping `Rpc()` in a helper. What it forbids is a *generic forwarder* that hides the
  argument list; a per-handler sender whose body is the two shipped lines
  (`if (ShouldRespondLocally(playerId)) { RpcDo_Thing(a, b); return; } Rpc(RpcDo_Thing, a, b);`) is the
  precedent §3.5 names — `OVT_StorageRequestComponent.SendStorageError`,
  `OVT_GMRequestComponent.SendCampaignObjective` — and it is strictly *better* for the audit: the
  direct call above is COMPILER-CHECKED and the `Rpc()` beside it is a one-line visual diff of the
  same list. Every handler in this feature has **exactly one** `Rpc()` call site and **exactly one**
  direct call, mechanically verified (table above).

- **P5-b — `MAX_LINE_QUANTITY = 10000` was added; it is NOT in the plan, and it closes an exploit.**
  §3.5 bounds the LINE COUNT (`m_iMaxCartLines`) but nothing bounds a line's *quantity*, and a port
  import has no source holder to bound it against. A cart of 16 `PORT_IMPORT` lines at `int.MAX` units
  overflows both `totalLitres` and `moneyTotal` into NEGATIVE numbers; a negative litre total passes
  `GetFreeLitres() < totalLitres`, and a negative money total passes `PlayerHasMoney()` and then makes
  `TakePlayerMoney()` **pay the player**. The bound copies the shipped
  `OVT_VehicleRequestComponent.IMPORT_MAX_QUANTITY`, is applied at BOTH the streamed line (malformed →
  refused once at Commit) and at Commit's re-derivation, and all three running totals carry a
  `< 0` tripwire so raising the constant cannot quietly reopen it.

- **P5-c — a repeated `resIndex` inside one cart is MALFORMED, not coalesced.** Coalescing would break
  the `index == LineCount()` stream check, and counting the same resource twice would double it in the
  litre sum and take it twice from the source. It is refused once, at Commit, with
  `#OVT-Resource_BadRequest`.

- **P5-d — the ladder's step 6 `m_IsWarehouse` test lives inside `PlayerMayUseWarehouse`.**
  `OVT_RealEstateManagerComponent.PlayerMayUseWarehouse` (`:624-656`) already answers **true** for a
  building whose real-estate config is absent or is not flagged `m_IsWarehouse`, so calling
  `GetConfig()` first — as §3.5 spells the step — would run the same lookup twice and give the flag two
  homes that can drift. One call, same semantics, and it is the identical body `storage` uses
  (`WarehouseIsAccessible`).

- **P5-e — `RpcAsk_BuildFromSite` ships GATED AND INERT; Phase 8 replaces one branch.** The wire, the
  identity resolution, the ruin gate and the distance gate are complete and audited now, because that
  is the half §3.5 owns. The construction-site component is Phase 8's, so nothing an `RplId` can name
  is a site and the terminal answer is `#OVT-Resource_NoSite` — the same refusal a demolished site will
  produce once the component exists. No protocol changes in Phase 8.

- **P5-f — the ground drop fills the pile BEFORE it takes from the source.** Not stated in §3.4, but the
  other order has a loss path: take the load off the truck, then fail to spawn a pile, and the goods are
  gone. `SpawnOrMergePile` answers null only on paths taken **before** it adds anything, so a failure is
  a clean whole-cart refusal (`#OVT-Resource_DropFailed`) with nothing mutated. The `Take` that follows
  cannot fail — availability was proven in the derivation phase and nothing between them yields.
  §3.4's `SpawnOrMergePile(vector, notnull array<ref OVT_ResourceAmount>)` signature is unchanged.

- **P5-g — dropping to the ground while standing on the source pile merges back into that same pile.**
  A no-op, not a defect: the frozen `SpawnOrMergePile` signature carries no exclusion, the ledger ends
  where it started and nothing is lost. Recorded so nobody reads it as one.

- **P5-h — no destination-radius gate beyond the six-step ladder.** §3.5's ladder puts the caller within
  `m_fUseRadius` of *every* holder the op reads, which already bounds two holders to `2 × m_fUseRadius`
  of each other. `m_fHolderRadius` is the PICKER's radius and is exposed through `GetHolderRadius()` for
  Phase 6; adding it as a seventh server step would be a rejection key the plan does not define.

- **P5-i — the `.st` is DEFERRED to Phase 11, continuing P2-d and P3-f.** Nothing renders a rejection
  key before Phase 6. **Twenty keys are owed**, all from `OVT_ResourceRequestComponent.c`:
  `#OVT-Resource_BadRequest`, `_DropFailed`, `_Failed`, `_Illegal`, `_Locked`, `_NoAccess`,
  `_NoCatalogue`, `_NoMoney`, `_NoPilePrefab`, `_NoPlayer`, `_NoPosition`, `_NoSite`, `_NoSpace`,
  `_NoStore`, `_NotAtPort`, `_NotEnough`, `_NotImportable`, `_NotSellable`, `_Ruined`, `_TooFar`.
  `Language/localization_Overthrow.st` is untouched (`git diff --exit-code` clean; 1 942 `{` / 1 942 `}`
  before and after).

---

- **P6-a — `ValidateCart`'s over-capacity refusal uses a NEW key, not `#OVT-Transfer_NoSpace`.** §3.6
  asks for `#OVT-Transfer_NoSpace` "with the remaining m³ in the message", but that key is
  `logistics/ui`'s and carries no format parameter, and re-wording it would change the port screen's
  refusal too. `#OVT-Resource_NoSpaceVolume` ("Not enough room: %1 m³ ordered, %2 m³ free") is
  returned already-resolved through `WidgetManager.Translate`, which is what a hook returning a string
  allows. No base file was touched.

- **P6-b — the screen subscribes to the destination picker itself.** In Put mode the picked holder IS
  the source, so the row set changes with the picker — but the base only calls `RefreshCheckout()` on a
  picker change, which is all its own consumers need. The consumer adds its own `m_OnChanged` listener
  (inserted after `super.OnShow()` wired the widget, removed before `super.OnClose()` nulls it) and
  guards on the base's `m_bRebuildingDestinations`, without which `RefreshDestinations`'
  `ClearAll`/`AddItem` would re-enter the refresh that is filling the picker. **This is a friction
  point with `ui`'s closed hook list, not a widening of it**: nothing was added to the base.

- **P6-c — `OnShow` runs one extra `Refresh()`.** `Refresh()` builds the entries BEFORE the
  destinations, so the first pass has no receiver to measure rows against and Put mode has no source.
  A second pass with the picker populated applies the per-row fit flags. Cheap (one sphere query) and
  it leaves focus where the base's own `RestoreFocus()` put it.

- **P6-d — the ground destination goes LAST in Take mode.** §3.6 says "plus" and does not fix the
  position; appending keeps every real holder's index stable when the mode changes to Put, which drops
  ground from the list. `PickedHolder()` falls back to the LAST store-bearing entry, reproducing
  exactly the index `RefreshDestinations` is about to clamp to.

- **P6-e — with no other holder in radius, Take offers exactly ONE destination (the ground).** §4's
  acceptance line reads "≥ 2 entries in Take mode (the holder plus Ground at minimum)", but §3.6's
  table is explicit that the opened holder is excluded from its own picker, and moving resources from
  a holder to itself has no meaning. §3.6 was followed. Park a truck beside a pile to see the two-plus
  case; alone in a field the picker hides itself and Accept drops on the ground.

- **P6-f — the action label branches on capacity, not on the host.** Capped → `#OVT-Resource_Cargo
  (used / cap m³)`; unlimited → `GetDisplayName() (used m³)`. That yields §3.6's three labels
  ("Cargo (12.5 / 20 m³)", "Crate pile (4.4 m³)", "Warehouse (… m³)") from one branch, and the truck
  is the only capped holder in the feature.

- **P6-g — 34 `.st` keys PAID, 6 left for Phase 11.** Paid: the 12 screen/action keys, the 8 catalogue
  title/description keys (clearing P2-d and P3-f) and the 14 checkout-path refusals from P5-i's list.
  Still owed, all unreachable until their own phase ships: `#OVT-Resource_Illegal`, `_NoMoney`,
  `_NotAtPort`, `_NotImportable`, `_NotSellable` (Phase 7's port category) and `_NoSite` (Phase 8).
  `Language/localization_Overthrow.st`: 1 942 `{` / 1 942 `}` before, 2 010 / 2 010 after.
  `Configs/Language/*.conf` untouched — a Workbench re-export is owed.

---

## Gotchas & Learnings

Recorded per phase as they are hit. Project-wide traps already known and relevant here:

- `array.Remove()` is swap-with-last — use `RemoveOrdered` wherever order is observable.
- Enfusion save contexts key properties by the **local variable's name** — a renamed local in `Deserialize` silently reads zeros and reports success.
- EnforceScript `float` is IEEE binary32 — hence D3's integer litres.
- `vector.Distance` is not correctly rounded — compare **squared** distances at boundaries.
- `ScriptBitWriter` cannot be round-tripped from script — hence D4's packed-string pack/unpack over a bitstream.
- `RandInt` is max-**exclusive**.
- `owned` is a reserved EnforceScript keyword.
- An `ItemPreview` spawns a **worldless** component instance — hence the `!owner.GetWorld()` guard in `OnPostInit` (3.9).

---

## Required Artifacts (produced during the run, recorded here)

- [x] **Phase 1** — the fail-proof mutation + resulting message for every Logic case (table below)
- [x] **Phase 4** — the deliberate Serialize/Deserialize local rename (table below)
- [x] **Phase 5** — the **RPC arity audit table**, all 7 rows against their handlers (Q4) (table below)

---

## Session Notes

### 2026-08-21 — Phase 4 complete

2 new serializers (142 + 138 lines), 1 new manager method (`ApplyPersistedPrices`), 5 `Overthrow.conf`
bindings (+25 lines), 4 round-trip cases + a shared fixture (+1 438 lines).
`compile-check.sh` exit 0 (6240 files) across six runs, three of them deliberate mutations.
Six implementation decisions recorded above (P4-a…P4-f).

**GUID ledger for this phase** (all `6B0E7A7…`; the whole prefix is 0 hits in the Reforger tree and
appeared only in this feature's own docs before today, so all nine are provably fresh and each occurs
exactly once): `…70A1B2C3D4` manager serializer on the game-mode config, `…71B2C3D4E5` store
serializer on vanilla CAR `{64C6B4937723DA61}`, `…72C3D4E5F6` on vanilla BUILDING `{65B682661F79DDBE}`,
`…73D4E5F607` + `…74E5F60718` (store **and** storage — D15) on Overthrow BUILDABLE `{6B0E7A27C0D539F2}`,
and the pile block `…75F6071829` config / `…7607182A3B` rule / `…7718293B4C` `GenericEntitySerializer`
/ `…78293A4C5D` store serializer.

**Existing saves.** Safe, and by the shipped mechanism: a save taken before this phase carries no
payload for either new serializer, so `ReadValue("version", version)` leaves `version` at 0, the
`version < 1` guard returns before any read, and nothing is applied. A truck/pile/warehouse comes up
with the empty ledger it spawned with (clean "no stock", not an error and not a wipe) and the price
table stays at whatever `InitPricesToBase()` built. The first save afterwards writes version 1.

**Not covered by any suite, and therefore owed to a play-test:** binding 4 (the Buildable block) has no
case until Phase 9 (P4-d); `SelfSpawn 1` on the pile block is only exercised by a **real** restart, not
by an in-session re-application; the order "game-mode `OnPostInit` builds the catalogue **before** any
holder record is deserialized" is assumed, and P4-b's guard is what makes a violation loud instead of
silent; and every multiplayer/JIP path, as ever.

### 2026-08-21 — Phase 3 complete

3 new scripts (650 lines), 2 new prefabs + `.meta`, 6 prefab edits, 5 new Init cases (~515 lines).
`compile-check.sh` exit 0 (6238 files). Six implementation decisions recorded above (P3-a…P3-f); P3-a
and P3-d are live constraints on Phase 4, P3-b on Phase 10, P3-e on Phase 5.

**GUID ledger for this phase** (all `6A8E2E0…`, all proven unused by `grep -rl 6A8E2E` in both trees):
`…0010` truck-base store (copied, not re-minted, by all four cap overrides), `…0011` warehouse store,
`…0012` pile store, `…0013` pile marker, `…0014` pile actions manager, `…0015` its user-action context,
`…0016` its point info, `…0100` the pile prefab's own resource GUID. Copied inherited GUIDs:
`SCR_BaseHUDComponent {53151CEE6C0A409F}` + its two InfoDisplays, `SCR_DestructionMultiPhaseComponent
{5624A88D86EFE8BA}` and `RplComponent {5624A88DC2D9928D}` (both from
`DestructibleMultiPhase_Props_Base.et:14-17`).

**Not covered by any suite, and therefore owed to a play-test:** opening a child of the same-GUID truck
delta in the Workbench (the only real proof a same-GUID delta of a vanilla prefab resolves), the
warehouse's `ItemPreview` path through the real-estate screen (the worldless-owner guard), and every
multiplayer/JIP path — the mirror rebuild has no single-machine exercise at all.

### 2026-08-21 — Phase 1 complete
6 files (1 558 lines of implementation + 1 538 of tests), 22 Logic cases. `OVT_TEST_LogicSuite` **239/239**. Four implementation decisions recorded above (P1-a…P1-d); two of them — P1-b and P1-d — are live constraints on Phases 5 and 8.

### 2026-08-21 — feature started
`/autorun-feature logistics/resources`. Plan already existed (117 KB, 11 phases) so planning was skipped. Scaffolded `tasks.md` + this file; `implementation.md` flipped to In Progress. Baselines to beat recorded in `tasks.md`.


---

## Phase 4 — Serialize/Deserialize local-rename artifact (required)

⚠️ **`SaveContext.Write(x)` and `LoadContext.Read(y)` key each property by the LOCAL VARIABLE'S NAME**
(`LoadContext.c`: *"Name of property is automatically derived from the input variable name"*). A local
spelled differently in `Deserialize` reads **zeros/empty and returns `true`** — total, silent data loss.

Each mutation below was applied to the source, compiled with `tools/compile-check.sh`, and reverted.
**Every one compiled clean (exit 0, 6240 files) — that IS the finding: no compiler, and no Workbench
validate, can see any of them.** See P4-f on why the runtime column is read off the case source.

| # | Mutation | Compiler | Runtime behaviour | Case + assertion that catches it |
|---|---|---|---|---|
| 1 | `OVT_ResourceStoreComponentSerializer.Deserialize`: local `lines` → `savedLines` (write side still `lines`) | **exit 0** | `Read()` finds no property called `savedLines`, fills an **empty array** and returns **`true`**. `ApplyPersisted([])` then clears the ledger and republishes — **every truck, pile and warehouse empties itself on the next continue**, with nothing in the log. | all three stock cases, first assertion in `AssertStockRestored`: *"…came back EMPTY. SaveContext.Write() and LoadContext.Read() key each property by the LOCAL VARIABLE'S NAME…"* |
| 2 | `OVT_ResourceManagerSerializer.Deserialize`: local `priceValues` → `storedPrices` | **exit 0** | Same empty-and-succeed. The count check then sees `4 != 0`… **unless** the ids local is renamed too, and in the single-rename case the mismatch ERROR fires and the table is left dirty. With **both** renamed, `ApplyPersistedPrices([], [])` resets the whole market **to base**. | `…_ResourcePrices_RoundTrip`, `AssertDrifted`: *"…came back at its config base of %3 - the price table was reset and never refilled, which is what a renamed Deserialize local does"* |
| 3 | `OVT_ResourceStoreComponentSerializer.Deserialize`: `if (!context.Read(lines))` → `if (!context.Read(lines) || true)` (forced read failure) | **exit 0** | The abort path is taken: an ERROR naming the holder is printed and **`ApplyPersisted` is never reached**, so the live ledger keeps exactly what it held. | all three stock cases, second assertion: *"…still holds %2 of '%3', the line written after the save…"* — i.e. the **dirty** line survives, which is the proof live state was untouched |

**Why "live state untouched" holds by construction, not by luck.** In both serializers every `Read()`
return is tested, **all** reads complete before **any** apply call, and the abort returns `true`
(the payload is consumed) without calling `ApplyPersisted` / `ApplyPersistedPrices`. There is exactly
one mutation call in each `Deserialize`, and it is the last statement before `return true`.

## Phase 5 — RPC arity audit table (required, Q4)

⚠ `Rpc()` is an **untyped variadic prototype**. A wrong argument count compiles clean and dies
silently at the wire (BUG-090). Reading each `Rpc()` line against its handler is the only gate there
is, so every handler in this feature has **exactly one** `Rpc()` call site and **exactly one**
compiler-checked direct call, and the two sit on adjacent lines. There is no generic send helper.

| # | Signature | Declared arity | `Rpc()` call-site args | Handler params | ✓ |
|---|---|---|---|---|---|
| 1 | `RpcAsk_TransferBegin(RplId source, RplId dest, int opKind, int seq, int lineCount)` — Server | 5 | 5 (`OVT_ResourceRequestComponent.c:254`) | 5 (`:333`) | ✓ |
| 2 | `RpcAsk_TransferLine(int seq, int index, int resIndex, int qty)` — Server | 4 | 4 (`:276`) | 4 (`:405`) | ✓ |
| 3 | `RpcAsk_TransferCommit(int seq, int lineCount)` — Server | 2 | 2 (`:294`) | 2 (`:448`) | ✓ |
| 4 | `RpcAsk_BuildFromSite(RplId site)` — Server | 1 | 1 (`:314`) | 1 (`:790`) | ✓ |
| 5 | `RpcDo_TransferResult(int seq, int movedLitres, int earned, int spent)` — Owner | 4 | 4 (`:868`) | 4 (`:878`) | ✓ |
| 6 | `RpcDo_ResourceError(int seq, string messageKey)` — Owner | 2 | 2 (`:850`) | 2 (`:897`) | ✓ |
| 7 | `RpcDo_SetPrice(int resIndex, int price)` — Broadcast, **on the manager** | 2 | 2 (`OVT_ResourceManagerComponent.c:288`) | 2 (`:297`) | ✓ |

Every row matches §3.5's table exactly. **No `array<…>` appears on any RPC parameter** (0 grep hits),
and both `RplId` slots always carry a valid holder — `HOLDER_TO_GROUND` passes the source in both and
the server reads only the slot the op names (`OpReadsSource` / `OpReadsDest`).

**Return-vs-answer audit** — every `return` in every ask, classified:

| Ask | Returns | Answering | Non-answering | Answer calls in body |
|---|---|---|---|---|
| `RpcAsk_TransferBegin` | 7 | 6 | 1 | 6 |
| `RpcAsk_TransferLine` | 5 | 0 | 5 | 0 |
| `RpcAsk_TransferCommit` | 29 | 27 | 2 | 28 |
| `RpcAsk_BuildFromSite` | 5 | 4 | 1 | 5 |
| **Total** | **46** | **37** | **9** | **39** |

37 answering returns + 2 terminal answers that need no `return` (Commit's `SendTransferResult`,
BuildFromSite's final `SendResourceError`) = the 39 answer calls. The nine non-answering returns are
all one of three kinds and none is a silent refusal:

1. **`!Replication.IsServer()`** — one per ask (4). The ask was delivered on a machine that owns
   nothing; there is no request to refuse and no client here to answer.
2. **"no open checkout / seq mismatch"** in Line and Commit (2). That checkout was already answered
   at Begin, or this is a duplicate commit. Answering again would break ONE ANSWER PER CHECKOUT.
3. **The three `m_bMalformed = true` deferrals** in Line (3). Not silent — deferred. The client
   streams Begin, up to 16 lines and Commit back to back, so answering per line would send 16
   refusals for one order; Commit answers once with `#OVT-Resource_BadRequest`.

## Phase 1 — Fail-proof table (Q2)

Every mutation was applied to the source, compiled (`compile-check.sh` exit 0 in all 23 runs — each is a live semantic variant no compiler catches), then reverted.

| Case | Mutation | Resulting assertion message |
|---|---|---|
| `LedgerAddClampsToCapacity` | `Add`: drop `if (fitted > room) fitted = room;` | `Add(cement, 10) with 400 litres of room fitted 10, expected 8` |
| `LedgerAddUnlimited` | `Add`: `capacityLitres >= 0 && unitLitres > 0` → `unitLitres > 0` (−1 as a literal cap) | `Add(timber, 10000) at unlimited capacity fitted 0, expected 10000` |
| `LedgerTakeClampsAndDropsLine` | `Take`: remove-on-zero → unconditional `m_mLines.Set(id, remaining)` | `An emptied line left 2 lines, expected 1 - a line that reaches zero is removed` |
| `LedgerTotalIsMaintained` | `Take`: drop the `m_iTotalLitres` decrement (stale field) | `TotalLitres() is 440 after taking 2 hardware, expected 400` |
| `LedgerIgnoresGarbage` | `Add`: drop the `index == -1` guard | `Add() of an id the definition table does not know fitted 5, expected 0` |
| `LedgerWouldFitIsExact` | `WouldFit`: `<=` → `<` | `WouldFit() refused a load that fills the cap to the litre` |
| `LedgerWeightAggregates` | `WouldFit`: insert a kg-vs-capacity gate | `WouldFit() refused 900 kg that occupies exactly the 400 litre cap - weight must not gate a fit` |
| `PackRoundTrips` | `Encode`: drop the `LINE_SEPARATOR` append | `Decode() rejected the string Encode() produced: '0:121:73:3'` |
| `PackRejectsMalformed` | `Decode`: field-count `return false` → `continue` | `Decode() accepted the malformed payload 'garbage'` |
| `DriftClampsAtBothEdges` | `DriftStep`: drop `if (next > hi) next = hi;` | `A huge positive roll walked to 500, expected the upper edge 200` |
| `DriftVolatilityScalesStepNotBand` | `DriftStep`: `hi = Round(base * bandMax * volatility)` | `The upper edge moved with volatility: 200 at 1, 400 at 2` |
| `DriftMultiplierAppliesAfterClamp` | `ApplyLevelMultiplier`: drop the multiplier | `The live price is 200, expected round(200 x 1.5) = 300` |
| `DriftOneStepPerWindow` | `ShouldDrift`: → `return true;` | `A second tick inside the same 06:00 window drifted again` |
| `WarPressureDirection` | `WarPressure`: `− (0.5 * controlledPortFraction)` → `+` | `Controlling every port did not push pressure down: 1 with no ports, 1 with all of them` |
| `SellFollowsLive` | `SellPrice`: price off the stored value, not the live one | `The sell price off a live 150 is 50, expected 75` |
| `MergeSelectsNearestThenLargest` | `SelectMergeTarget`: tie rule `>` → `<` | `The merge target is index 1, expected 2 - nearest, and the larger of the two tied piles` |
| `RequirementScaleNeverZero` | `ScaleRequirement`: drop the `< 1` floor | `ScaleRequirement(1, 0.1) is 0, expected the floor 1 - a requirement never scales to free` |
| `RequirementSatisfied` | `AmountOf`: sum → first entry only | `An exactly-met requirement was reported short on 'timber' - availability must sum across every pile` |
| `ConsumptionOrderIsNearestFirst` | `SortPilesForConsumption`: `<` → `<=` (unstable on a tie) | `The first pile drained is index 2, expected 1 - nearest first, and stable on the tie with index 2` |
| `ConsumptionOrderIsNearestFirst` (2nd) | test: `order.RemoveOrdered(1)` → `order.Remove(1)` | `After removing the second entry the order is 1, 0, 3 - expected 1, 3, 0` |
| `ImportablePredicate` | `MayExport`: same flag both ways | `A non-importable resource was refused on export - the flag gates buying only` |
| `IllegalPredicate` (D20) | `IllegalGateOpen`: drop the `IsIllegal` test | `An illegal resource was admitted with neither the permission nor a resistance-held port` |
| `ReadoutNamesTheShortfall` | `FormatReadout`: `continue` on a satisfied requirement | `The readout dropped the satisfied requirement: 'timber: 12 / 40 (short 28)'` |

## Phase 1 — Implementation decisions the plan did not settle

**P1-a — `OVT_ResourceRules` carries five statics beyond §3.2's nine.** `IsDriftWindowHour(int)` + `ShouldDrift(int hour, int lastDriftedHour)` (so §3.4's latch is a *call*, not a re-implementation, and `DriftOneStepPerWindow` is a Logic case), `MayImport`/`MayExport`, `IllegalGateOpen` (D20's case), and one shared helper `AmountOf(array<ref OVT_ResourceAmount>, string)` behind `IsSatisfied` and `FormatReadout`. §7 mandates cases the nine cannot express; these are the smallest additions that let the pure tier own them.

**P1-b — `Add` returns UNITS, not litres.** §3.2's table says "returns how many fitted" (units, matching `OVT_StorageLedger.Add`); §7's row phrases it "what fitted in litres". Units chosen — the wire carries `movedLitres` separately. **Phase 5 must not assume litres from `Add`.**

**P1-c — the ledger holds a second map `m_mUnitLitres`.** Not in §3.2's member list, but `Take(string, int)` takes no `defs` and `TotalLitres()` must be O(1), so the per-unit volume is captured at `Add` time. No signature changed. Every path that fills a ledger (`Add`, `Decode`, `ApplyPersisted`) passes `defs`, so the captured value cannot go stale across a config edit.

**P2-e — the manager was moved to `Scripts/Game/GameMode/Managers/`** by the orchestrator after Phase 2. §3.1 puts it there beside `OVT_EconomyManagerComponent`; `tasks.md` had said `Scripts/Game/Components/`, which was a scaffolding error. Three other scaffolding paths were corrected in `tasks.md` at the same time: the config class is `Scripts/Game/Configuration/`, the serializers are `Scripts/Game/Persistence/Serializers/Components/`, and the map location type is `Scripts/Game/UI/Map/LocationTypes/`. **§3.1 is the authority on file location, not `tasks.md`.**

**P1-d — `FormatReadout` returns raw text, not localized keys.** `"timber: 12 / 40 (short 28)"`, newline-separated. Logic cases cannot resolve a `#OVT-…` key, and §11 owns the `.st` work. **Phase 8 will want keyed formatting** — the case asserts the numbers and the shortfall marking, so that swap needs the case updated with it.

---

## Needs Human Verification

The suites cover no UI, no layout, no `.et` and no `.conf`, and no MP path is reachable single-machine. Everything below is owed to a Workbench session or a play-test.

**Workbench (prefab integrity):**
- Open `M923A1_transport.et` (a child of the new same-GUID truck delta) — exactly **one** `OVT_ResourceStoreComponent` at 20 m³, and the vanilla truck's own components (`SCR_WheeledDamageManagerComponent`, `VehicleWheeledSimulation`, `m_eVehicleType TRUCK`) still resolved.
- Open `resources.conf` and save without changing anything — the four entries must survive with their m³ values intact.
- Open `OVT_ResourcePile.et`, `OVT_OverthrowController.et`, `Character_Player.et`, `OVT_OverthrowGameMode.et` and `Warehouse_01_Base.et` with no dropped-attribute warnings; the warehouse must show the item storage component **and** the new resource store.
- The warehouse's `ItemPreview` path — open the real-estate purchase screen and confirm the warehouse icon renders without a null crash. That is the worldless-owner guard's only real exercise.

**`.st` re-export (user, Workbench):** 34 entries were added to `Language/localization_Overthrow.st`. Keys render raw until you re-export `Configs/Language/*.conf`. **Never** hand-write those exports.

**Play-test — the transfer screen (Phase 6):**
- Truck action reads `Cargo (0.0 / 15.0 m³)`; opening gives Take/Put with no tab row.
- Ground drop puts the pile **at your feet**, not the truck's; the truck's label falls within a second with no menu open.
- With two holders in 25 m the picker lists **nearest-first**; Put mode drops Ground from the list.
- **P6-b:** in Put mode with two piles near, changing the picker must repopulate the *row list*, not just the summary.
- Over-capacity refuses the **whole** cart naming the remaining m³ — never a partial move.
- The warehouse action sits **third**; on a warehouse you do not own it is visible and refused with a reason; on a ruin it vanishes entirely.
- Open/close three times then change contents once → exactly **one** redraw (a second means a subscription survived `OnClose`).
- **Gamepad only:** focus on arrival, d-pad walks rows, left/right on the picker changes destination **without** moving the focus column.

**Known acceptance-criteria conflict (P6-e):** §4 Phase 6's gate says the picker shows ≥ 2 entries in Take mode, but §3.6's table excludes the opened holder from its own picker. §3.6 was followed, so a truck **alone in a field offers exactly one destination (Ground) and the picker hides itself**. Two-plus needs a second holder within 25 m. Confirm which behaviour you want.

---

## Wiki debt (BLOCKED — no `wikijs` MCP server in this session)

Same block recorded by `logistics/ui`, `logistics/storage` and `economy/fuel`. Nothing was attempted. A later session with the server owes, in the Documentation space:

1. **New page `resources`** — the full loop (buy → haul → drop → build), and the actual per-unit volume/weight/base-price table (the Field Manual deliberately omits the numbers because difficulty scales them). Cross-link `ports`, `building`, `storage`, `difficulty`. **Search first** — a `logistics` or `supplies` page may already exist.
2. **`ports`** — the Resources tab in both Import and Export; prices are live and separate from the shop catalogue; trade goes through the truck's **cargo** store, not its item storage.
3. **The building page** — "buildings cost money only" is now wrong. Guard Tower, Helipad, Garage and Warehouse raise a construction site; money is charged at placement; Build consumes piles within 30 m nearest-first; removal refunds nothing.
4. **`storage` / warehouse** — warehouses hold resources on a second, separate action and ledger; a Warehouse is now buildable (base, or a resistance-controlled town).
5. **`difficulty`** — three new `Economy` settings with their shipped per-level values: `buildableResourceCostMultiplier`, `resourcePriceMultiplier`, `resourcePriceVolatility` (Easy 0.8/0.8/0.5 · Hard 1.5/1.25/1.5 · Extreme 3/1.5/2 · Insane 4/2/2).
6. **Modding docs** — `Configs/Resistance/resources.conf` is the resource catalogue and adding or re-pricing a resource is a config edit; `m_aResourceRequirements` and `m_SitePrefab` are new optional `OVT_Buildable` fields, and an empty requirement list is byte-identical to the old behaviour.
7. **Screenshots owed** — the port Resources tab, the cargo HUD, a crate pile on the map, a construction site with its Requirements readout.

---

## Post-review fixes by the orchestrator (2026-08-21)

**R-1 — the town gate disagreed across the wire, failing silently.** `OVT_BuildContext.CanBuild` returns `true` out of its `m_bBuildAtBase` branch **before** the town branch runs, but `OVT_ResistanceFactionManager.BuildItem` applied `TownControlAllowsBuild` to every buildable carrying `m_bBuildInTown` — and the new Warehouse sets **both** flags. A Warehouse ordered at a resistance-held base inside an enemy town/city radius was therefore offered by the client and refused **silently** by the server: no notification, no money taken, nothing placed. DoD **F14** ("at a base **or** in a resistance-controlled town") and **F15** ("refused client-side with a reason and server-side on re-validation") collide exactly there and the plan never settles precedence.

**Resolution:** F14's "or" is the clearest statement of player-facing intent and it sits in the DoD, which is the acceptance authority — so a qualifying base wins outright on **both** sides. Added `OVT_ResistanceFactionManager.BaseAllowsBuild(vector)`, mirroring the client's base branch exactly (nearest base, not occupying-faction, within `m_Difficulty.baseRange`), and short-circuited `TownControlAllowsBuild` on it. **If you would rather a base inside an enemy town be refused, the change to make is the opposite one — run the town gate before the base branch on the client — and this note is where to start.**

**R-2 — the drift readout described no drift on any preset but Normal.** `OVT_PortContext.DriftText` divided `GetPrice()` (the live price, with `resourcePriceMultiplier` already applied) by the raw base, so Hard (1.25×) and Insane (2×) pegged every resource at "above base" or "far above base" permanently, regardless of where the random walk actually sat — defeating DoD **F16**'s "the movement is visible in the details panel". Now reads `GetStoredPrice()`, the walked, band-clamped value. The level multiplier is flat scaling, not drift.

**R-3 — a stale test assertion at the Phase 8 → 9 seam.** `OVT_TEST_Init_ResourceSeam_LConstructionSiteSeam` asserted exactly **3** requirement-bearing buildables (D17: Guard Tower, Helipad, Garage). Phase 9 then added the Warehouse, which §3.9's table explicitly gives a requirement — making the correct answer **4**. The code was right and the assertion was stale. Corrected; the cross-phase review did **not** catch this, the final suite run did.

---

## Play-test tweaks — round 1 (2026-08-22, user)

**T-1 — storage and cargo actions were unreachable from the truck bed.** Both this feature's `OVT_OpenResourceStoreAction` and `storage`'s four vehicle actions (`OVT_OpenStorageMenuAction`, `OVT_SellVehicleCargoAction`, `OVT_TransferAllToStorageAction`, `OVT_RenameStorageAction`) already list `"door_rear"` in their `ParentContextList`. **Neither vanilla truck defines a context by that name** — `M923A1.et` and `Ural4320.et` author `door_l01`, `door_r01`, `window_rear` and the passenger contexts, and nothing at the bed — so every one of those five actions bound only at the two cab doors and the `door_rear` entry was inert.

Fixed by authoring the missing context once, on the truck delta: `ActionContexts { UserActionContext "{6A8E2E1000000030}" { ContextName "door_rear" … } }`. All five actions pick it up with **no action edit**. Offsets are vehicle-local with **−Z aft**; the M923A1's hull ends at −4.3 and the Ural's at −3.7 (measured from the most-negative component offsets in each vanilla prefab), so `Offset 0 1 -4` with `Radius 3` / `Height 2.5` reaches the tailgate of both and covers a player standing behind it.

⚠️ **Unproven until Workbench/play-test: that a delta's `ActionContexts` array MERGES with the concrete truck's own rather than replacing it.** The precedent says it does — `Character_CIV.et` (a delta of `Character_CIV_base.et`) declares `ActionContexts` containing a single entry that sets only `Radius 2`, i.e. an inherited context overridden **by GUID**, and its siblings survive. A fresh GUID should therefore add an entry. If the rear action does **not** appear in play-test, the fallback is to author the same context directly on `M923A1_transport.et` / `Ural4320_transport.et` (and the two Mobile FOB prefabs), which already carry cap overrides.

⚠️ **`.et` files carry no comments.** The reasoning above was briefly written into the prefab and removed: the edited file was the **only** `.et` with a `//` line in either the Overthrow or the vanilla tree, and Workbench rewrites these files on save. Do not put comments in a `.et`.

**T-2 — a ground drop from a truck spawned inside the cab.** `ResolveDropPosition` placed the pile `m_fUnloadOffset` metres along the **caller's character** forward axis. A player opening the screen at the driver's door faces the truck, so "4 m forward" put the pile through the right-hand side of the cabin.

Now: when the source holder is a `Vehicle`, the drop is measured from the **hull's own aft extent** — `pos = origin + forward * (mins[2] − offset)`, where `mins` comes from `GetBounds()` (local space; the `OVT_StructureDestructionComponent.c:412` precedent). Measuring from the bounding box rather than a fixed offset means a long truck and a short one both clear their own tailgate. Any non-vehicle source (pile, warehouse) keeps the character-relative behaviour. `m_fUnloadOffset` default **4 → 3** per the user, and its description now covers both cases.
