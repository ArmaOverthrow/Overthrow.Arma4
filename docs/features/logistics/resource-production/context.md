# Resource Production (logistics/resource-production) - Context & Decisions

**Last Updated:** 2026-08-22
**Current Phase:** Phase 2 — Manager, prefabs, discovery and the drip
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- ✅ Requirements written (2026-08-22)
- ✅ Implementation plan written (2026-08-22) — 8 phases, approach A, 14 recorded decisions (D1–D14)
- ✅ Feature started; `tasks.md` scaffolded (62 tasks across 8 phases + a cross-phase review)

- ✅ **Phase 1 — the pure spine + the icon field** (7/7). `OVT_ResourceProductionRules.c` (179 L, ten statics + `SITE_SELL_RATIO 0.8` + `MAX_SKIP_HOURS 720`), `m_sMapIconName` appended to `OVT_Resource`, `resources.conf` icon name, `OVT_TEST_Logic_ProductionRules.c` (711 L, 17 cases). Every case fail-proven. Gate: `compile-check.sh` exit 0 (6312 files). Suites **deferred to the final sweep** at the user's request.

**What's Next:**
- Phase 2 — manager, prefabs, discovery and the drip (⚠️ advanced)

**Blockers:**
- None

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
| *(populated in Phases 2, 3, 4)* | | | |

---

## Session Notes

### 2026-08-22 — Feature started (autorun)
Plan was already complete (8 phases, 4 advanced, 90 KB). Scaffolded `tasks.md` + `context.md`, flipped
`implementation.md` to In Progress, and recorded the map-atlas pre-flight observation above. Suite baselines
carried over from `logistics/resources`' final gate: Logic 247/247 · Init 174/175 (one pre-existing red) ·
PersistenceRoundTrip 40/40 · Campaign 18/18 · Persistence 13/13.
