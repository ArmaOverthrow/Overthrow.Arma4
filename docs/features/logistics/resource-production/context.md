# Resource Production (logistics/resource-production) - Context & Decisions

**Last Updated:** 2026-08-22
**Current Phase:** ✅ All 8 phases + the cross-phase review complete
**Status:** ✅ Ready for Review — 🔴 **no test has been run** (see Blockers)

---

## Quick Status

**What's Done:**
- ✅ Requirements written (2026-08-22)
- ✅ Implementation plan written (2026-08-22) — 8 phases, approach A, 14 recorded decisions (D1–D14)
- ✅ Feature started; `tasks.md` scaffolded (62 tasks across 8 phases + a cross-phase review)

- ✅ **Phase 1 — the pure spine + the icon field** (7/7). `OVT_ResourceProductionRules.c` (179 L, ten statics + `SITE_SELL_RATIO 0.8` + `MAX_SKIP_HOURS 720`), `m_sMapIconName` appended to `OVT_Resource`, `resources.conf` icon name, `OVT_TEST_Logic_ProductionRules.c` (711 L, 17 cases). Every case fail-proven. Gate: `compile-check.sh` exit 0 (6312 files). Suites **deferred to the final sweep** at the user's request.

- ✅ **Phase 2 — manager, prefabs, discovery and the drip** (10/10). 3 new scripts (`OVT_ResourceProductionComponent.c` 117 L, `OVT_ResourceProductionManagerComponent.c` 633 L, `OVT_TEST_Init_ProductionSeam.c` 568 L), 4 new prefabs (+`.meta`), 5 additive edits (`OVT_Global.c`, `OVT_OverthrowGameMode.c`/`.et`, `OVT_SleepService.c`, the test-world layer). Gate: `compile-check.sh` exit 0 (6316 files). **RPC arity: both rows ✓** (each `Rpc()` sits directly under a compiler-checked direct call with the same args — the only mechanical guard against BUG-090). The site prefabs carry **no** user action (D13's window stays shut until Phase 5).

- ✅ **Phase 3 — ownership, privacy and the difficulty accessor** (8/8). `OVT_ResourceProductionRequestComponent.c` (339 L, 3 RPCs), three user actions (`OVT_BuySiteAction` 200 L + `…Resistance` 64 L + `OVT_ToggleSitePrivacyAction` 85 L), the controller-prefab entry, `GetRealEstateCostMultiplier()` (that difficulty field's **first ever reader**), the controller-seam roster + its `"10"`→`"11"`, and Init cases G/H (seam 568 → 932 L). Gate: `compile-check.sh` exit 0 (6320 files). **Return audit: 24 returns across both asks, 22 answering with a key, 2 exempt `!IsServer()` early-outs — zero silent refusals.** `CONFIG_STREAM_VERSION` still 6; 4 persistence rules (unchanged); all walled files `git diff --exit-code` clean.

- 🟡 **Phase 4 part A — `SITE_BUY` on the wire** (5/9 — tasks 4.1–4.5). Six additive edits inside
`OVT_ResourceRequestComponent.c` (+135 / -22 L over 1241): the appended enum value, `IsKnownOp`/`OpReadsDest`,
the `MayReachHolder` extraction (**md5-identical** to the shipped steps 1–5), `MayBuyFromSite`,
`MayUseHolderForOp` routing all three gate call sites, and the money branch. Plus 3 Logic cases + a shared
fixture (711 → 985 L). Gate: `compile-check.sh` exit 0 (6320 files). **Arity audit: all six shipped RPCs
unchanged**, no RPC added, `array<…>` on none. Two files touched; every walled file `git diff --exit-code` clean.

**What's Next:**
- Phase 4 part B — the buy screen (`ui-developer`: tasks 4.6–4.8, `OVT_ProductionSiteBuyContext.c`,
  the `Character_Player.et` context block, `OVT_BuySiteStockAction.c` + Sort 3 on the site prefab with
  GUIDs `6A8E2F0000000024`/`…0025` reserved for it)

**Blockers:**
- 🔴 **The five-suite sweep has never run.** `tools/run-tests.sh` was refused by Claude Code's auto-mode permission classifier. Every phase gated on `compile-check.sh` exit 0 (final: **6325 files**) only, so **32 new test cases are unproven at runtime**. The user must run it, or add a Bash permission rule for `tools/run-tests.sh`.
  Baselines: Logic **247** · Init **174/175** (1 pre-existing red, `CompositionSlotGate_AcceptedTypesMatchTheCompositions`) · PersistenceRoundTrip **40** · Campaign **18** · Persistence **13**. This feature adds **20 Logic, 9 Init, 2 PersistenceRoundTrip, 1 Campaign**.

**Test-gate policy for this run:** the user decided on 2026-08-22 to **hold every `run-tests.sh` run until the end of the autorun** — a Workbench process (PID 65408) is open and concurrent Workbench has produced false exit-2 INDETERMINATE runs on this tree. Per-phase gate is `compile-check.sh` exit 0 only; the five suites run once, by class name, in the cross-phase review.

---

## Key Files

### Plan & requirements
- `docs/features/logistics/resource-production/implementation.md` — the authority; §3 architecture, §4 phases, §5 D1–D14, §6 DoD, §7 testing, §9 risks
- `docs/features/logistics/resource-production/requirements.md` — scope authority
- `docs/features/logistics/epic-overview.md` — epic build order and the two-ledgers wall

### Epic siblings this feature sits on
- `docs/features/logistics/ui/implementation.md` §3.4 — the **closed** eight-hook `OVT_TransferContext` contract (I1: do not modify the base)
- `docs/features/logistics/ui/context.md` — gamepad traps (`WLib_NavigationButton` not focusable without an override; the picker eats d-pad left/right; `array.Remove` is swap-with-last)
- `docs/features/logistics/resources/implementation.md` — `OVT_ResourceStoreComponent`, `OVT_ResourceLedger`, the `SITE_BUY` host `OVT_ResourceRequestComponent` (I3/I4)
- `docs/features/logistics/storage/implementation.md` — the item ledger this feature must **never** touch

### Core implementation (created as phases land)
*(populated per phase)*

---

## Decisions Made

The fourteen plan decisions (D1–D14) live in `implementation.md` §5 and are the authority. Recorded here only when a
phase discovers something the plan did not know.

### Phase 1 fail-prove ledger (2026-08-22)

Every one of the 17 Logic cases was proven able to fail: the mutation was applied to `OVT_ResourceProductionRules.c`,
`compile-check.sh` confirmed it was a valid semantic-only edit, then it was restored (final `diff` byte-identical).

| Case | Mutation | Resulting failure |
|---|---|---|
| `SitePriceIsEightyPercentFloorOne` | drop `Math.Max(1, …)` | `SitePrice(0, 0.8) is 0, expected the floor 1` |
| `SitePriceIsNotTheSellRatio` | `SITE_SELL_RATIO` 0.8 → 0.5 | site ratio and the port's 0.5 produce the same answer at live price 10 |
| `BuyCostScalesAndFloors` | drop the floor | `BuyCost(1, 0.01) is 0, expected the floor 1` |
| `AccessUnownedIsRefused` | `owner==""` returns true | an unowned site admitted a viewer |
| `AccessOwnerAlwaysAllowed` | consult `isPrivate` first | the owner was refused on their own private site |
| `AccessPublicAllowsStrangers` | reverse `!isPrivate` | a stranger refused on a public site |
| `AccessPrivateRefusesStrangers` | body → `return viewerId == owner;` | an **empty** viewer id matched an **empty** owner |
| `AccessResistanceAllowsEveryone` | remove the `"resistance"` branch | a viewer refused on a resistance-owned private site |
| `PrivacyTogglingIsOwnerOrOfficer` | `isOfficer` → `!isOfficer` | an officer refused on a resistance-owned site |
| `ProduceAccumulatesFraction` | force `carryOut = 0` | `Produce(0.5, 1, 0, out)` left the carry at 0 |
| `ProduceIsZeroForNonPositive` | drop the `hours <= 0` guard | `Produce(2, -5, 0.3, out)` returned **-10** units |
| `ProduceClampsHugeSkips` | remove the `MAX_SKIP_HOURS` clamp | a 100,000-hour skip returned 100,000 units |
| `ProduceCarryStaysInUnitRange` | `carryOut = total` | carry reached 1.11 after hour 2 |
| `FitProductionPausesWhenFull` | `Math.Min` → `Math.Max` | a full store fitted 10 units |
| `FitProductionRejectsBadLitres` | remove the `litresPerUnit <= 0` guard | integer divide-by-zero — a runtime abort rather than a clean `SetFailure`, still red |
| `ShouldProduceOncePerHour` | `!=` → `>` | hour 0 after a latch of 23 stopped producing (the midnight rollover) |
| `ColourStateHasThreeAnswers` | remove the unowned branch | an unowned site coloured 2 instead of 0 |

### Phase 2 — two deliberate departures from the plan (both additive, both correct)

1. **`OVT_ProductionSiteData` gained a sixth field, `float rate`**, copied from the component at discovery. Without it
   the acceptance rule "a site whose entity is not resolvable is skipped **with its carry advanced**" is
   unimplementable — the rate lives only on the component, which is on the entity that just failed to resolve. Not
   replicated, not persisted; Phase 7's `OVT_PersistedProductionSite` is unaffected.
2. **`CheckProduction`'s latch line is the sibling's working shape**, `if (!m_bLatchAsserted && AssertHourLatchFromClock()) m_bLatchAsserted = true;`
   — **not** the plan's `… ) return;`. The plan's pseudocode never sets the flag, so `AssertHourLatchFromClock()` would
   re-run every tick, re-stamp `m_iHourProduced` to the current hour, and make `ShouldProduce` permanently false:
   **no site would ever produce anything**. `OVT_ResourceManagerComponent.CheckPrices` is the shipped template and does
   it correctly. 🔴 **`implementation.md` §3.4's pseudocode is wrong on this line** — it is documentation, not shipped
   code, but a future reader should not copy it.

Two robustness additions that change no server behaviour: `InitializeSites()` is idempotent (a record already at a
discovered position keeps owner/privacy/carry and only re-adopts the entity, so a stray second pass cannot orphan
ownership — Phase 7's retry inherits this), and `RplLoad` runs discovery first when the site list is still empty
(EOnInit-before-stream is expected but not contractual, and an empty list would silently drop a joining client's whole
ownership payload).

**Watch on the suite run:** Init cases A–F assert a **mesh-less STATIC discovery** for the first time in this project.
The `Flags 1027` (`0x403`) + `EQueryEntitiesFlags.STATIC` combination is what `OVT_FallbackHomePos` /
`OVT_StartCameraPos` already use and is vanilla's dominant static-prop flag set, but case B is the one to watch. It
fails with a named diagnosis listing all three possible causes.

### Cross-phase review 2026-08-22 — 17 findings

Run by an independent reviewer that implemented none of the phases. **2 🔴, 4 🟠, 11 🟡.** Eleven were fixed in a
single follow-up batch; the rest are recorded below with the reason they were left.

**🔴 1 — a site's non-produced stock was destroyed by every save/load. FIXED.** The serializer persisted **one**
ledger line per site because D9 assumed "a site holds exactly one resource by construction". That premise is **false**:
the feature deliberately allows `Put` into a site and `OVT_ResourceTransferContext` ships `MODE_PUT`, so an owner who
stored steel and cement in their sawmill lost both on the next load, silently, with no in-game message — B1's "never a
lost line" and F14 failing on a supported action. `OVT_PersistedProductionSite` was unshipped, so `stockId`/
`stockQuantity` became `array<ref OVT_PersistedResourceLine> stock` (the class `ApplyPersisted` already consumes) with
**no version bump and no migration**. Caught one save short of costing a migration.

**🔴 2 — the sign prefab's GUID shadow. LEFT ALONE at the user's direction (2026-08-22).**
`Prefabs/Structures/Signs/Large/SignLarge_01_base.et` is untracked, authored outside this feature, and is the direct
parent of `OVT_ProductionSite_Base.et`. Its `.meta` declares `{48C842DD171DE305}` while the vanilla file it shadows on
that path is `{48C842DD171DE304}`, and its own header inherits `{48C842DD171DE304}` **at its own path**. Every other
same-path shadow in this tree keeps the vanilla GUID (`Prefabs/Structures/Signs/Signs_Base.et` is the precedent). Nine
vanilla `SignLarge_01_*` prefabs inherit `{…304}`. **The user owns this file and will verify it in Workbench.**

Two consequences of the sign parent that this feature inherits either way:
- It supplies the `RplComponent` that `OVT_ProductionSite_Base.et` no longer declares itself (GUID `…0006` was minted
  for one and is now unused). Without it the store's `RplProp` is dead and `GetHolderId` returns `RplId.Invalid()`,
  which would make `SITE_BUY` unaddressable. **A future parent change removes it silently.**
- Sites are now **vanilla-destructible** (1500 HP `SCR_DestructionMultiPhaseComponent`). `OVT_StructureDamage.IsUsable`
  only resolves Overthrow's own `OVT_StructureDestructionComponent`, so a shot-up sign still opens its storage. F16's
  "not an enemy target" is now a claim about AI behaviour, not about the prefab.
- On the upside it gives sites a **mesh**, which is exactly the escape hatch D12 reserved for "play-test says a site is
  hard to find on foot".

**Fixed in the follow-up batch:** `ApplyStaged` wrote `owner`/`isPrivate` directly instead of through the two
broadcasting setters (an in-session `ReapplyLatestSaveData` would have left every client on stale ownership, privacy,
map colour and storage gate **forever** — invisible to the single-machine suite); `RplLoad`'s empty-list fallback could
drop a joining client's entire ownership payload; a double `PublishContents()` on the persistence apply (Q6); a
`vector.Distance` deciding the 30 m purchase gate (Q5/B8); the privacy toggle sending client state instead of intent;
a refusal with no reason on `OVT_BuySiteAction`; an unchecked `GetPlayers()` deref on a per-frame path; the carry
guard discarding a whole unit instead of correcting; two stale doc comments; comment-volume trimming on the three
worst files; **and the Campaign-tier case the plan specified but no phase ever wrote** — the resistance-funds purchase
had no automated coverage beyond the pure predicates.

**Left deliberately, with reasons:**
- **The `SITE_BUY` partial-fit charge.** If `destStore.Add` fits fewer units than `Take` removed, the excess returns to
  the source but `moneyTotal` is unchanged. Inherited **verbatim** from the shipped `PORT_IMPORT` branch and
  unreachable behind the whole-cart fit check. Fixing it edits shipped port behaviour, which is out of this feature's
  scope. **Worth a bug report against `logistics/resources`.**
- **`SITE_BUY` takes none of the port's illegal-resource gates.** Deliberate and inert today (all four resources are
  `m_iIllegal 0`), but it becomes a loophole the day a resource is marked illegal.
- **The shipped variants departed from §3.3's balance** — rates 6/3/4 and costs 32000/40000/28000 against the plan's
  3/1.5/2 and 8000/20000/14000. Presumed user tuning. Consequence: **no shipped site has a sub-1 rate**, so F2's
  headline scenario is exercised only in the Logic suite, never in content.
- **`ResolveStatusKey` compares `owner`/`isPrivate` inline** — display only, decides no access, already recorded above.
- **Q7 is vacuous as written.** It checks `git status --porcelain Configs/Language/`; **that directory does not exist**
  in this tree. The real exports are `Language/*.conf`. The intent (never write the exports) was honoured throughout.
- **The pre-flight imageset note above is backwards.** The `size 784 522` → `1 1` change is *correct*, not a
  regression — the sibling `overthrow_priceicons.imageset` uses the same `1 1`, and `RefSize 784 652` matches the new
  atlas exactly. The user has since drawn `cement` and `steel` too; only `hardware` still falls back to `crate`.

### User change 2026-08-22 — unowned and private icons are WHITE, not black/green

User asked mid-run for the map icons to be white when unowned or private. Applied as attribute defvalue changes on
`OVT_MapLocationProductionSite` only — the predicate, the three-state model and `ColourState` are untouched:

| State | Was | Now |
|---|---|---|
| 0 — unowned | `0 0 0 1` black | **`1 1 1 1` white** |
| 1 — yours / public / resistance | `0 1 0 1` green | `0 1 0 1` green *(unchanged)* |
| 2 — owned private, not yours | `0 1 0 0.5` half-alpha green | **`1 1 1 0.5` half-alpha white** |

Half alpha is kept on state 2 so the three states stay distinguishable at a glance — a full-white state 2 would be
identical to unowned. The `OverthrowMap.conf` block authors no colour fields, so these defvalues are what ship.
**This supersedes `implementation.md` §3.10 and DoD F12**, which both say black-when-unowned.

### Phase 8 — the localization audit

**33 entries added**, GUIDs `6A8E2F3000000001`–`…0021` (series proven 0-hit outside this feature's own docs). Braces
**2342 → 2410, balanced both sides**. Parameter counts, which are the thing that breaks silently:

| Key | Params |
|---|---|
| `#OVT-ProdSite_Buy_Price`, `_BuyResistance_Price`, `_BuyStock`, `_Row_RateValue` | **one `%1`** |
| `#OVT-ProdSite_Summary`, `#OVT-ProdSite_Row_VolumeValue` | **two** |
| the other 27 | none |

🔧 **One defect found and fixed during the audit.** `Prefabs/Production/OVT_ProductionSite_Base.et`'s Sort 3 `UIInfo`
`Name` pointed at `#OVT-ProdSite_BuyStock`, which carries a `%1` for the stock count. A prefab `UIInfo` has nothing to
substitute, so on the fallback path (script label empty) it would render a **literal `%1`** on screen. Added a
parameter-free `#OVT-ProdSite_BuyStockName` and retargeted the prefab. Its sibling `#OVT-ProdSite_BuyResistance` was
simply missing and was added the same way.

Note the plan predicted 22 owed keys; the real figure is 33 — Phase 6's map info panel accounts for most of the gap.

### Phase 7 — 🔴 the rename trap is INVISIBLE to the toolchain (proven)

The agent renamed `Deserialize`'s local from `sites` to `siteRecords` — all three touch points — leaving `Serialize`'s
`sites` alone, and ran the gate:

```
compile-check: OK (6324 files, Game module, 6s)   EXIT_WITH_RENAMED_LOCAL=0
```

**Identical exit code and identical file count to the correct version.** The property name is not a symbol the compiler
can see; it is a runtime string derived from the variable. Then it found *why* the runtime cannot catch it either, in
`ArmaReforger/scripts/Game/generated/Plugins/Serialization/LoadContext.c`:

> `//! Read a given value. Name of property is automatically derived from the input variable name.`
> `DoesKeyExist` / `DoesObjectExist`: `NOTE: ... Binary container will always return true!`

Under the binary container there is **no "key missing" answer to return** — a mismatched name cannot fail, it can only
produce a default-constructed value. **A checked `Read()` therefore cannot catch this class of fault; only a
round-trip case can.** That is why the ownership case's still-dirty branch names the mechanism in full.

### Phase 7 — the two arrival orders

`ScriptedComponentSerializer.GetDeserializeEvent()` defaults to `AFTER_ENTITY_FINALIZE`, and it could not be
established contractually whether that precedes the game mode's `EOnInit`. Both orders are handled:

- **Deserialize first** (R4's case): `m_bSitesDiscovered` false → stage only, **touch nothing**. `Init()` →
  `InitializeSites()` → `ApplyStaged()`.
- **Init first** (in-session `ReapplyLatestSaveData`, where `Init()` will not run again): flag true → apply where it
  lands. Discovery has already proven the entities exist, so no entity is touched speculatively.

**Retry:** empty-unmatched returns with the payload **retained** (which is what makes a second `ApplyStaged` a real
re-application rather than a no-op); otherwise exactly one `CallLater(ApplyStaged, 1000)`, then at attempt 2 one
`LogLevel.ERROR` **per unmatched record** naming its location, match range, attempt count, owner, quantity and
resource id, and `m_aStagedSites = matched` so the dropped ones are gone and the landed ones stay re-appliable.

**Idempotency by construction:** every field is `=`, never `+=`; stock goes through `store.ApplyPersisted(lines)`,
which `Clear()`s then rebuilds. An **empty** payload still clears the store, so a site empty at save time is empty
after apply.

⚠️ **Flake window on the stock case:** the drip fires on an in-game hour change and day acceleration is 6, so an
in-game hour is 600 real seconds. The exposed windows are a couple of frames. If that case ever goes red with
`held = 137 + 6n`, that is the cause, not a defect.

⚠️ **Argued, not measured:** that an existing pre-Phase-7 save still loads. The `version < 1` guard returns before the
single `Read()`, and the shape is byte-identical to `OVT_ResourceManagerSerializer.c:72-77` and
`OVT_RealEstateManagerSerializer.c:169-172` — both added to this same `{65ACD95F40F6C669}` block over existing CI
saves and green in the 40/40 baseline. A real cold load is owed to play-test.

### Phase 7 — third plan-path correction

§4 Phase 7 gives a flat `Scripts/Game/Persistence/` path; §3.1's tree gives
`Scripts/Game/Persistence/Serializers/Components/`, where all 23 existing serializers live. Built at the §3.1 path.
**That is now three phases where §4's paths disagree with §3.1's tree** (Phases 6, 7 — and §3.8's
`Character_Player.et` in Phase 4). §3.1 has been right every time.

### Phase 6 — two plan-path corrections

1. **`implementation.md` §4 Phase 6 gives a flat path** (`Scripts/Game/UI/Map/OVT_MapLocationProductionSite.c`) while
   **§3.1's architecture tree gives `Scripts/Game/UI/Map/LocationTypes/`**. The latter is right — every existing map
   location type, including the reference `OVT_MapLocationResourcePile`, lives there. Built at the `LocationTypes/`
   path; `tasks.md` corrected.
2. **The status/owner display rows are deliberately NOT routed through `ColourState`/`MayAccessStore`.** Those
   predicates answer "can this viewer open it"; the info panel needs "what is this site's status", which is a
   different question. The distinction is display-only and carries no permission weight — worth knowing before
   someone "fixes" it into the predicate.

### Phase 5 — R1's mitigation, verified

`grep -rn "MayAccessStore" Scripts/Game --include=*.c` (excluding tests) finds **exactly** the definition
(`OVT_ResourceProductionRules.c:44`), its own internal use inside `ColourState` (`:174`), the server step
(`OVT_ResourceRequestComponent.c:1158`) and the client step (`OVT_OpenResourceStoreAction.c:202`).
`grep "owner ==\|owner !=\|.isPrivate ==" ` over both call-site files → **0 hits**. `PlayerMayUseWarehouse` still has
only its one pre-existing call site. The two gates share the *predicate*, not merely an outcome.

**Ladder position:** the site step is **appended last** — `MayReachHolder` → `WarehouseIsAccessible` →
`SiteIsAccessible`. The order between the warehouse and site checks is functionally inert (a site is never a
registered warehouse), but appending rather than inserting keeps the warehouse path's *code path*, not just its
outcome, untouched. For any non-site holder `OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(holder)`
returns null and the step short-circuits to `true` immediately.

`MayBuyFromSite` deliberately does **not** route through this step — it calls `MayReachHolder` directly and asks
`MayBuyStock`, an unowned-only question. `SITE_BUY`'s own refusal ladder is unaffected.

**Init case `I` drives the real ladder**, not the predicate: it teleports to the Sawmill and sends
`RequestTransferBegin(siteId, siteId, HOLDER_TO_GROUND, 1)` five times (that op reads only its source, so it is
exactly one `MayUseHolder` call per state and never a destination), reading `GetOnResourceError()` synchronously on
the listen host. Nothing is committed, so no stock or money moves, and the site is restored to unowned before any
assertion is judged.

### Phase 4 — the three money bounds (R6)

| Bound | Where | Stops |
|---|---|---|
| **Quantity** | `MAX_LINE_QUANTITY = 10000` at `RpcAsk_TransferLine`, re-enforced per line in the commit loop | a 16-line cart at `int.MAX` units wrapping the litre and money sums negative |
| **Real stock** | the generic `sourceStore.GetLedger().Count(id) < line.m_iQuantity` refusal (applies because `OpReadsSource(SITE_BUY)` is true) | buying stock a site never produced — also why no "is this the site's resource" check is needed |
| **Negative total** | `moneyTotal < 0`, immediately after the site price is added | the wrap itself: `PlayerHasMoney` accepts a negative and `TakePlayerMoney` of a negative **pays** the player |

All three are server-side and all three run **before** `PlayerHasMoney` is consulted. The per-unit price is re-derived
server-side from the live table — the client never supplies a price. **Nothing clamps**: the whole cart is refused.

**An owned site refuses in exactly one place** — `MayBuyFromSite`'s `!MayBuyStock(rec.owner)` → `#OVT-ProdSite_Owned`
(`grep -c` on the file returns 1). Because Commit re-runs the whole source ladder, a site bought by somebody else
between Begin and Commit refuses with that key **before any money moves** (R10). `MayBuyFromSite` calls
`MayReachHolder`, never `MayUseHolder`, so `PlayerMayUseWarehouse`'s `isRented` hole can never decide for a site —
that is the entire reason the extraction exists.

### Phase 4 — orchestrator fix on top of the agent's work

The agent flagged, and left alone under §3.6's "nothing else", that the `source == dest` guard at Begin was still
`HOLDER_TO_HOLDER`-only: a forged `Begin(site, site, SITE_BUY, …)` would take stock out of a site, put it straight
back, and still charge for it. Its reading was that this is a self-inflicted money burn rather than an exploit — true,
but **B1 rules out "a charge for goods not delivered"** and the fix is one line. The guard now reads
`(HOLDER_TO_HOLDER || SITE_BUY) && source == dest`. Recompiled green. The buy screen's `BuildDestinations` already
excludes the site, so no legitimate path could reach it.

### Phase 4 — the prefab path in the plan is wrong

§3.8 and §4 Phase 4 item 5 name `Prefabs/Characters/Core/Character_Player.et`. **That file does not exist** — there is
exactly one `Character_Player.et` in the tree, at `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et`, and
that is where the context block landed (line 126, inside an `m_aContexts` array spanning lines 20–177). A context
absent from `m_aContexts` silently never opens, so the placement was verified programmatically rather than by eye.

### Phase 3 — `.st` keys owed to Phase 8 (14 new)

`#OVT-ProdSite_Buy_Price`, `#OVT-ProdSite_BuyResistance_Price`, `#OVT-ProdSite_Buy`, `#OVT-ProdSite_BuyResistance`,
`#OVT-ProdSite_MakePublic`, `#OVT-ProdSite_MakePrivate`, `#OVT-ProdSite_NoPlayer`, `#OVT-ProdSite_NoSite`,
`#OVT-ProdSite_AlreadyOwned`, `#OVT-ProdSite_NotOfficer`, `#OVT-ProdSite_NoFunds`, `#OVT-ProdSite_NoMoney`,
`#OVT-ProdSite_NotYours`, `#OVT-ProdSite_Failed`.

⚠️ The two `_Price` keys go through `WidgetManager.Translate(key, OVT_MoneyFormat.FormatMoney(cost))` and **must each
carry exactly one `%1`**. The other twelve are bare strings. `#OVT-Resource_TooFar` already exists.
Phase 2 additionally owes `#OVT-ProdSite_Storage`, `#OVT-ProdSite_Sawmill`, `#OVT-ProdSite_SteelMill`,
`#OVT-ProdSite_CementPlant` (referenced from the prefabs). Phase 4 adds `#OVT-ProdSite_Owned` (bare),
`#OVT-ProdSite_NoStock` (bare), `#OVT-ProdSite_BuyStock` (**one `%1`** — the stock count) and
`#OVT-ProdSite_Summary` (**two params** — `%1` money, `%2` m³). Running total owed: **22 keys.**

### Phase 4A — one `.st` key owed to Phase 8 (1 new)

`#OVT-ProdSite_Owned` — the server's answer when a SITE_BUY names a site somebody already owns. A bare
string, no `%1`. `#OVT-ProdSite_NoSite` is the only other key the new gate answers with and Phase 3 already
owes it. Every other refusal on the SITE_BUY path reuses a shipped `#OVT-Resource_*` key.

### Phase 4A — the SITE_BUY refusal ordering

Read top to bottom; the FIRST match answers and nothing below it runs. Every line answers with a key —
there is no silent refusal anywhere on this path.

**`RpcAsk_TransferBegin`**

| # | Refusal | Key |
|---|---|---|
| — | not the server | *silent by design — nothing on this machine to refuse or answer* |
| 1 | `seq == SEQ_NONE` | `#OVT-Resource_BadRequest` (answered under SEQ_NONE, so it lands as a hint) |
| 2 | `playerId <= 0` | `#OVT-Resource_NoPlayer` |
| 3 | `!IsKnownOp(opKind)` | `#OVT-Resource_BadRequest` — **now passes for 4** |
| 4 | `lineCount <= 0` or `> m_iMaxCartLines` | `#OVT-Resource_BadRequest` |
| 5 | source ladder — `MayUseHolderForOp(…, isSource: true)` → **`MayBuyFromSite`** | see below |
| 6 | dest ladder — `MayUseHolderForOp(…, isSource: false)` → `MayUseHolder` (the shipped six steps, unchanged) | `#OVT-Resource_NoPlayer` / `NoStore` / `Ruined` / `TooFar` / `Locked` / `NoAccess` |

**`MayBuyFromSite`** — the source ladder, and the whole of what is new:

| # | Refusal | Key |
|---|---|---|
| a–e | `MayReachHolder`: no player, no holder or no store, ruined, caller beyond `m_fUseRadius` (30 m), locked or player-owned | `#OVT-Resource_NoPlayer` / `NoStore` / `Ruined` / `TooFar` / `Locked` |
| f | the holder carries no `OVT_ResourceProductionComponent` | `#OVT-ProdSite_NoSite` |
| g | no production manager | `#OVT-ProdSite_NoSite` |
| h | no record within `SITE_MATCH_RANGE` of it | `#OVT-ProdSite_NoSite` |
| i | `!MayBuyStock(rec.owner)` | `#OVT-ProdSite_Owned` — **the one and only place an owned site refuses a SITE_BUY** |

`MayBuyFromSite` calls `MayReachHolder`, **not** `MayUseHolder`, so `WarehouseIsAccessible` — and therefore
`PlayerMayUseWarehouse`, whose `isRented` clause is a recorded hole — never decides for a site. That is the
whole reason the extraction exists.

**`RpcAsk_TransferLine`** answers nothing, ever. A malformed line is remembered and refused once at Commit.

**`RpcAsk_TransferCommit`**, in order:

| # | Refusal | Key |
|---|---|---|
| 1 | `playerId <= 0` | `#OVT-Resource_NoPlayer` |
| 2 | `m_bMalformed` | `#OVT-Resource_BadRequest` |
| 3 | line count mismatch, or zero lines | `#OVT-Resource_BadRequest` |
| 4 | no resource manager, or an empty catalogue | `#OVT-Resource_NoCatalogue` |
| 5 | **the source ladder again, in full** | as above — a site bought by somebody else between Begin and Commit refuses `#OVT-ProdSite_Owned` **before any money moves** |
| 6 | source has no store or no ledger | `#OVT-Resource_NoStore` |
| 7 | the dest ladder again, in full | as above |
| 8 | dest has no store or no ledger | `#OVT-Resource_NoStore` |
| 9 | economy or player manager missing | `#OVT-Resource_Failed` — **new for SITE_BUY**: the resolution was hoisted off the port branch |
| 10 | per line: definition index out of range | `#OVT-Resource_BadRequest` |
| 11 | per line: `qty <= 0` or `qty > MAX_LINE_QUANTITY` | `#OVT-Resource_BadRequest` — **money bound 1** |
| 12 | per line: empty id | `#OVT-Resource_BadRequest` |
| 13 | per line: `totalLitres < 0` | `#OVT-Resource_BadRequest` |
| 14 | per line: `sourceStore.GetLedger().Count(id) < qty` | `#OVT-Resource_NotEnough` — **money bound 2 (real stock)** |
| 15 | per line: `moneyTotal < 0` after the site price is added | `#OVT-Resource_BadRequest` — **money bound 3 (the tripwire)** |
| 16 | whole cart: `destStore.GetFreeLitres() < totalLitres` | `#OVT-Resource_NoSpace` |
| 17 | whole cart: `!economy.PlayerHasMoney(persId, moneyTotal)` | `#OVT-Resource_NoMoney` |

Nothing below 17 can refuse and nothing above it has mutated anything. **Nothing clamps** — a cart that does
not fit, is not stocked or is not paid for is refused whole.

The port's own gates — `AtAPort`, `MayImport`, `IllegalGateOpen`, `ResistanceControlsNearestPort` — stay
behind `isPort` and a SITE_BUY takes **none** of them. There is deliberately no "is this the resource the
site produces" check either: a site's store only ever holds its own resource, so refusal 14 already covers it
with one fewer thing to keep in sync.

### Phase 4A — three departures worth recording

1. **The affordability and payment branches read `chargesMoney`, not `PORT_IMPORT || SITE_BUY` spelled out
   twice.** `chargesMoney` is defined once as exactly that set, so "who is checked for money" and "who is
   charged" cannot drift apart, and a future third paying op is one line rather than three. For every
   pre-existing op the value is identical to the shipped condition.
2. **`persId` is now resolved before `AtAPort` rather than after it.** The plan's hoist requires it. It is a
   pure lookup with no side effect, and the refusal ORDER for a port op is unchanged (`#OVT-Resource_Failed`
   still precedes `#OVT-Resource_NotAtPort`), so no port behaviour is observably different.
3. **The `source == dest` guard at Begin is still `HOLDER_TO_HOLDER`-only.** A forged
   `Begin(site, site, SITE_BUY, …)` would therefore take stock out of a site and put it straight back while
   charging the forger — a self-inflicted money burn, not an exploit: no resource is created, none is lost,
   and no other player is affected. Left alone because §3.6 item 6 says "nothing else"; widening the guard to
   `SITE_BUY` is a one-line change if the orchestrator wants it.

### Phase 4A — the three money bounds (B3, R6)

| Bound | Where | What it stops |
|---|---|---|
| Quantity | `MAX_LINE_QUANTITY = 10000`, enforced at `RpcAsk_TransferLine` (defers to Commit) **and** re-enforced per line in the Commit loop | A 16-line cart at `int.MAX` units wrapping the litre and money sums negative |
| Real stock | `sourceStore.GetLedger().Count(id) < line.m_iQuantity` in the same loop | Buying stock a site never produced; it is also why no separate "is this the site's resource" check is needed |
| Negative total | `moneyTotal < 0` immediately after the site price is added | The wrap itself — `PlayerHasMoney` accepts a negative amount and `TakePlayerMoney` of a negative **pays** the player |

All three run on the server, all three run before `PlayerHasMoney` is consulted, and the per-unit price is
**re-derived server-side** from the live table (`SitePrice(resources.GetPrice(idx), SITE_SELL_RATIO)`) — the
client never supplies a price. `OVT_TEST_Logic_ProdRules_SiteCartHasHeadroomAtTheShippedBounds` asserts the
worst legal cart (16 × 10000 units at a per-unit price 25× the dearest a shipped resource can drift to) stays
a positive int with room to spare, so raising `MAX_LINE_QUANTITY` re-derives that case rather than quietly
reopening the hole.

### Phase 3 — one deliberate departure

`OVT_BuySiteResistanceAction` **subclasses** `OVT_BuySiteAction` (four virtual seams: funding flag, label key,
no-funds key, affordability) rather than duplicating the ladder. Keeps the price, the label and the ask in one body,
which is what the "one `BuyCost` call each" rule is really protecting.

**Watch on the suite run:** Init **case H moves the player.** The Sawmill is at `140 1 120` and the ask refuses beyond
30 m, so the case funds the buyer exactly `BuyCost`, seeds stock with `ProduceForHours(1)`, `TeleportPlayer`s to the
site, drives the real `BuySite()`, teleports back, captures owner/privacy/stock/packed/money and **restores the site to
unowned before judging**. The move is a bounded precondition with a named failure that distinguishes "teleport did not
stick" from "the ask refused". This is the one new case with an environmental dependency.

### Icon-name correction (2026-08-22, orchestrator)

Phase 1's agent reported naming `timber` only but in fact wrote `m_sMapIconName` on **all four** resources. The
orchestrator blanked `cement`, `steel` and `hardware` in `resources.conf` — the user's commit `cd9cb2ff` ("Resource and
barracks icons") added exactly two quads, `barracks` and `timber`, so the other three names pointed at quads that do
not exist and would have rendered **nothing** (R5's invisible-site failure). They now fall back to the shipped `crate`.
**Turning them on later is a one-line `.conf` edit per resource once the glyphs are drawn** — no script change (D11).

### Pre-flight observations (2026-08-22, orchestrator)

- **The map atlas is mid-edit in the working tree.** `UI/Imagesets/overthrow_mapicons.imageset` already gained two
  quads (`barracks`, `timber`) in an uncommitted change, and the atlas grew a row (`RefSize 784 522` → `784 652`,
  and the texture `size` was corrected from `1 1` to `784 522`). Of this feature's four resources, **only `timber`
  has art**. Consequence for Phase 1 task 1.3 and Phase 6 task 6.3: name `timber` in `resources.conf`, leave
  `cement` / `steel` / `hardware` **empty** so `GetIconName` falls back to the shipped `crate` quad (R5). Naming a
  quad that does not exist in the imageset renders **nothing**, which is exactly the invisible-site failure R5
  exists to prevent. The three remaining glyphs are owed art — a `.conf` edit turns them on with no script change
  (D11).
- **Branch is `v1.5`.** The working tree also carries an uncommitted `Prefabs/Props/Resources/OVT_ResourcePile.et`
  edit. Nothing is committed by this run — the user owns all git operations.

---

## RPC Arity Table

> BUG-090: `Rpc()` is an untyped variadic proto — a wrong argument count compiles clean and dies silently at the
> wire. Every RPC added by this feature is recorded here with its arity and checked against its handler.

| RPC | Args | Handler args | ✓ |
|---|---|---|---|
| `RpcDo_SetSiteOwner` (Phase 2, Broadcast) | 2 — `Rpc(RpcDo_SetSiteOwner, rec.location, owner)` | 2 — `(vector pos, string owner)` | ✓ |
| `RpcDo_SetSitePrivacy` (Phase 2, Broadcast) | 2 — `Rpc(RpcDo_SetSitePrivacy, rec.location, isPrivate)` | 2 — `(vector pos, bool isPrivate)` | ✓ |
| `RpcAsk_BuySite` (Phase 3, Server) | 2 — `Rpc(RpcAsk_BuySite, pos, useResistanceFunds)` | 2 — `(vector pos, bool useResistanceFunds)` | ✓ |
| `RpcAsk_SetSitePrivacy` (Phase 3, Server) | 2 — `Rpc(RpcAsk_SetSitePrivacy, pos, isPrivate)` | 2 — `(vector pos, bool isPrivate)` | ✓ |
| `RpcDo_ProductionError` (Phase 3, Owner) | 1 — `Rpc(RpcDo_ProductionError, messageKey)` | 1 — `(string messageKey)` | ✓ |
| **Phase 4A adds no RPC.** The six shipped RPCs on `OVT_ResourceRequestComponent` were re-audited mechanically and are unchanged: | | | |
| `RpcAsk_TransferBegin` (Server) | 5 — `Rpc(RpcAsk_TransferBegin, source, dest, opKind, m_iSeq, lineCount)` | 5 — `(RplId, RplId, int, int, int)` | ✓ |
| `RpcAsk_TransferLine` (Server) | 4 — `Rpc(RpcAsk_TransferLine, seq, index, resIndex, qty)` | 4 — `(int, int, int, int)` | ✓ |
| `RpcAsk_TransferCommit` (Server) | 2 — `Rpc(RpcAsk_TransferCommit, seq, lineCount)` | 2 — `(int, int)` | ✓ |
| `RpcAsk_BuildFromSite` (Server) | 1 — `Rpc(RpcAsk_BuildFromSite, site)` | 1 — `(RplId)` | ✓ |
| `RpcDo_TransferResult` (Owner) | 4 — `Rpc(RpcDo_TransferResult, seq, movedLitres, earned, spent)` | 4 — `(int, int, int, int)` | ✓ |
| `RpcDo_ResourceError` (Owner) | 2 — `Rpc(RpcDo_ResourceError, seq, messageKey)` | 2 — `(int, string)` | ✓ |

`SITE_BUY` rides `RpcAsk_TransferBegin`'s existing `int opKind` slot as the bare value **4**, so the wire
shape does not move at all — which is the whole reason the plan appended an op instead of adding a protocol.
All six `Rpc()` calls still sit on the line directly beneath a compiler-checked direct call to the same
handler with the same arguments; none was touched by this phase. `grep` for `array<` inside an `Rpc()` or an
`[RplRpc]` signature in the file returns **0**.


All three Phase 3 rows have the same guard: each `Rpc()` sits directly beneath a compiler-checked
direct call to the same handler with the same arguments (`BuySite` / `SetSitePrivacy` take the
`Replication.IsServer()` branch first; `SendProductionError` takes the `ShouldRespondLocally`
branch first). No `array<…>` appears on any of the three wires, and identity is not a parameter
on any of them — `ResolveOwningPlayerId()` is the only source of the caller.

Both Phase 2 broadcasts sit directly under a **compiler-checked direct call to the same handler with
the same arguments** (`OVT_ResourceProductionManagerComponent.c` `SetSiteOwner` / `SetSitePrivacy`),
which is the only mechanical guard there is against BUG-090. Neither half may be hoisted. No
`array<…>` appears on either wire.

---

## Session Notes

### 2026-08-22 — Phase 4 part A (`SITE_BUY` on the wire)

**Built:** the six additive edits of §3.6 inside `OVT_ResourceRequestComponent.c` (1241 → 1354 L,
+135 / −22), and three Logic cases plus a shared fixture appended to
`OVT_TEST_Logic_ProductionRules.c` (711 → 985 L). Two files touched, nothing else.
`compile-check.sh` exit 0, 6320 files.

**The gate diff, in one line each:**

- `EOVT_ResourceOp.SITE_BUY` **appended** as value 4; the four shipped members keep their numbers.
- `IsKnownOp` and `OpReadsDest` each gain one `if`. `OpReadsSource` is `opKind != PORT_IMPORT` and was
  already correct for 4 — verified, not changed.
- `MayReachHolder` is the shipped `MayUseHolder`'s `rejectKey = ""` plus steps 1–5, **moved with no edit**:
  the 33 extracted lines hash `71c718e767b88e7186e6b05432c11dd8` in both `HEAD` and the working tree.
- `MayUseHolder` is now `MayReachHolder` + the untouched `WarehouseIsAccessible` step. For every
  pre-existing op the answer and the key are identical, and `rejectKey` is still initialised to `""` on
  entry (by `MayReachHolder`, which runs first on every path).
- `MayBuyFromSite` and `MayUseHolderForOp` are new; `HoldersUsable` and Commit's two inline gate calls all
  route through the latter, so `MayUseHolder` now has exactly one caller in the file.
- The money work: `chargesMoney`, the hoisted `economy`/`players`/`persId` block, the `SITE_BUY` price
  branch with its `moneyTotal < 0` tripwire, and `chargesMoney` on the affordability and payment branches.

**Logic cases added (3):**

| Case | Claim | Proof it can fail |
|---|---|---|
| `SitePriceComposesOverACart` | a cart totals the sum of each line's own site price × its own quantity, and weighs the sum of each line's own litres, both keyed on the LINE's index | price the whole cart at one index's rate — the swapped-quantity cart would tie at 1580 instead of 2780 |
| `SiteCartHasHeadroomAtTheShippedBounds` | 16 lines × `MAX_LINE_QUANTITY` at a per-unit price 25× the dearest a shipped resource can drift to stays a positive int, below the `int.MAX / (lines × qty)` ceiling | raise `MAX_LINE_QUANTITY` far enough and the running total wraps negative mid-loop |
| `SitePriceUndercutsThePortPerResource` | on every shipped resource a site undercuts the port's import price and still charges MORE than the port pays on export — so site-buy → port-export always loses money | set `SITE_SELL_RATIO` at or below the port's 0.5 sell ratio |

The fixture (`OVT_TEST_ProductionCartFixture`) hand-builds the four shipped resources with their real
litres (m³ × 1000) and base prices, and hand-builds carts out of `OVT_ResourceCartLine` — the same class
the server's commit loop walks — so `CartTotal` is the commit loop's money maths written out. It is a
**local** fixture, deliberately not the one in `OVT_TEST_Logic_ResourceRules.c`: that file is being edited
by a concurrent session.

**Deliberately NOT done** (Phase 5 owns them): no site step in `MayUseHolder`, no
`OVT_OpenResourceStoreAction` branch, no Sort 3 entry on `OVT_ProductionSite_Base.et` — GUIDs
`6A8E2F0000000024`/`…0025` are still unclaimed (`grep` returns 0).

### 2026-08-22 — Phase 2 (manager, prefabs, discovery, drip)

**Built:** `OVT_ResourceProductionComponent`, `OVT_ResourceProductionManagerComponent`, the four
`Prefabs/Production/` files, the game-mode prefab + Init block + `OVT_Global.GetProduction()`, the
`OVT_SleepService` call, one Sawmill in the test world layer, and
`OVT_TEST_Init_ProductionSeam.c` (6 cases, A–F). `compile-check.sh` exit 0, 6316 files.

**GUIDs minted** (`6A8E2F0…`, re-verified 0 hits in both trees immediately before authoring):
`…0001` production component, `…0002` store, `…0003` actions manager, `…0004` action context,
`…0005` context PointInfo, `…0006` RplComponent, `…0010` the manager on the game-mode prefab,
`…0100`–`…0103` the four prefab file GUIDs, `…0200` the shared entity ID (the three variants restate
the base's `ID`, which is how a delta prefab binds — `Site_Warehouse.et` is the precedent).

**Two deliberate departures from §3.4, both additive:**

1. **`OVT_ProductionSiteData` carries a sixth field, `float rate`,** copied from the component at
   discovery. Without it "a site whose entity is not resolvable is skipped with its **carry
   advanced**" is unimplementable — the rate only exists on the component, which is on the entity
   that just failed to resolve. Not replicated, not persisted; Phase 7's record class is unaffected.
2. **`CheckProduction`'s latch line is the sibling's working shape**, `if (!m_bLatchAsserted &&
   AssertHourLatchFromClock()) m_bLatchAsserted = true;`, not the plan's `… ) return;`. The plan's
   pseudocode never sets the flag, so `AssertHourLatchFromClock()` would re-run every tick, re-stamp
   `m_iHourProduced` to the current hour and make `ShouldProduce` **permanently false** — no site
   would ever produce anything. `OVT_ResourceManagerComponent.CheckPrices` is the shipped template.

**Two robustness additions** (neither changes server behaviour):

- `InitializeSites()` is **idempotent**: a record already at a discovered position keeps its owner,
  privacy and carry and only re-adopts the entity. So a stray second discovery pass cannot orphan
  ownership, and Phase 7's retry gets the same guarantee for free.
- `RplLoad` runs discovery first when the site list is still empty. EOnInit-before-stream is the
  expected order, but it is not contractual, and an empty list would drop every joining client's
  ownership with only a warning.

**Left for later phases, deliberately:** no user action of any kind is authored on the site prefabs
(D13 — the store action lands in Phase 5 with both halves of its gate), and no persistence rule or
entity persistence config exists anywhere (D9 — Phase 7).

**Observation for Phase 6 (not fixed here):** Phase 1 filled `m_sMapIconName` on **all four**
`resources.conf` entries, but `UI/Imagesets/overthrow_mapicons.imageset` only carries a `timber`
quad. Naming a quad that does not exist renders **nothing** — which is exactly the invisible-site
failure R5 exists to prevent. Either the three quads are drawn before Phase 6, or `cement`, `steel`
and `hardware` are blanked so `GetIconName` falls back to `crate`.

**Owed:** the `.st` keys `#OVT-ProdSite_Storage`, `#OVT-ProdSite_Sawmill`, `#OVT-ProdSite_SteelMill`
and `#OVT-ProdSite_CementPlant` are referenced by the prefabs and do not exist yet (Phase 8, task
8.1). They render raw until then.

### 2026-08-22 — Feature started (autorun)
Plan was already complete (8 phases, 4 advanced, 90 KB). Scaffolded `tasks.md` + `context.md`, flipped
`implementation.md` to In Progress, and recorded the map-atlas pre-flight observation above. Suite baselines
carried over from `logistics/resources`' final gate: Logic 247/247 · Init 174/175 (one pre-existing red) ·
PersistenceRoundTrip 40/40 · Campaign 18/18 · Persistence 13/13.
