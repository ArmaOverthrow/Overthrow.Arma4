# Towns Stability - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (pre-dates Beast Mode; handler modifiers introduced in `d62e441` "Better support and stability modifiers")
**Documented:** 2026-08-02
**Last Updated:** 2026-08-03 00:25

---

## Executive Summary

Town stability is a fully **derived** 0–100 value: `stability = round(clamp(100 + Σ baseEffect of every active modifier))`, recomputed the moment any modifier is added or removed. This feature owns the **shared modifier framework** (`OVT_TownModifierSystem` base + `OVT_ModifierConfig`/`OVT_Modifier` — the sibling support feature reuses it with a different `Recalculate`) plus the stability system and its 11 configured modifiers (6 with handlers: black-market and strong-economy transactions, civilian and OF deaths, patrol harassment, three random-event instances of one class; 5 externally driven: battles, medical supplies, and one dead entry). Stability throttles tax income, NPC buying, shop stock, population growth, and gates the peaceful village flip — it is the "order" axis to support's "loyalty" axis.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase (`/discover-feature`, 2026-08-02). The feature has already been implemented and shipped.

---

## Goals

### Primary Goals
- A composable cause→effect system where any subsystem can push a named, timed, stackable modifier onto a town without touching town internals.
- Content-in-config: effect sizes, timeouts, stack limits and handlers all live in `stabilityModifiers.conf`.
- Stability derived (never accumulated) so it can never drift and always survives save/load exactly.

### Success Criteria
- [x] Shared framework: config load (all machines), handler bootstrap (server-only), 10 s tick with timeout decay, sum-and-clamp recalculation
- [x] 11 configured modifiers; add/remove/timeout/reset replicated to clients incl. JIP (fixed in `96ee803`)
- [x] Persistence as parallel id/timer arrays with stale-id drop; stability recomputed on load
- [ ] Permanent (timeout 0) modifiers surviving the tick rebuild (BUG-057)
- [ ] Per-town hour-gating in tick-driven handlers (BUG-058)

---

## Current Architecture

### Key Components

| Area | Files | Role |
|---|---|---|
| Framework base | `Scripts/Game/GameMode/Systems/OVT_TownModifierSystem.c` (159 L) | Config load, `PostInit` handler bootstrap, per-town `OnTick` (timer decay + rebuild), additive `Recalculate`, `TryAddByName`/`RemoveByName`, 3 protected routing virtuals |
| Framework types | `.../Modifiers/OVT_ModifierConfig.c` (31 L), `OVT_Modifier.c` (42 L) | `OVT_ModifierFlags {ACTIVE, STACKABLE}`; config entry (name/title/baseEffect/timeout/stackLimit/flags/handler); handler base with `Init/OnPostInit/OnStart/OnTick/OnActiveTick/OnDestroy` hooks |
| Stability system | `.../Modifiers/OVT_TownStabilityModifierSystem.c` (16 L) | Routes the 3 virtuals to the manager's stability methods; keeps base `Recalculate` |
| Handler base | `.../Modifiers/OVT_StabilityModifier.c` (17 L) | `AddModifierToNearestTown[InRange]` helpers |
| Handlers (6 classes) | `.../Modifiers/Stability/*.c` | BlackMarket, CivilianDeath, OccupyingFactionDeath, PatrolHarassment, Random (×3 config instances), StrongEconomy |
| Config | `Configs/Modifiers/stabilityModifiers.conf` (106 L) | 11 entries — **index order is the wire/save ID** |
| Owner/transport | `OVT_TownManagerComponent.c` | `m_aTownModifiers` (prefab-wired), 10 s `CheckUpdateModifiers`, TryAdd/Remove/Timeout/Recalculate API, RPCs, JIP, persistence apply (see towns/core — core owns the transport, this feature owns the payload semantics) |

### The modifier config table (index = persisted/replicated id)

| idx | name | effect | timeout | stacks | added by |
|---|---|---|---|---|---|
| 0 | RecentGunfire | −10 | 1200 | — | **nothing — dead entry** (but it's what the persistence tests pick as "first negative modifier") |
| 1 | CivilianDeath | −5 | 1800 | ×5 | civilian death event (no instigator check) |
| 2 | OccupyingFactionDeath | −5 | 1800 | ×5 | OF AI death event |
| 3 | MedicalSupplies | +1 | 1200 | ×10 | delivery (`cost/10` add-calls in a loop) |
| 4/5/6 | CorruptMayor / OrganizedCrime / DrugProblems | −3/−8/−5* | 3600 | non-stackable | `OVT_RandomStabilityModifier` rolls (~0.7%/town/hour base) |
| 7 | StrongEconomy | +5 | 3600 | ×5 | non-dealer purchase ≥ $50 |
| 8 | RecentBattle | −50 | 3600 | non-stackable | every town QRF resolution |
| 9 | BlackMarketActivity | −5 | 3600 | ×3 | gun-dealer purchase ≥ $1000 |
| 10 | PatrolHarassment | −2 | **0 (permanent)** | ×5 | hourly patrol-presence check |

*DrugProblems' −5 is the attribute default by omission. Omitted `flags` also makes 0/4/5/6/8 non-stackable by default — `ACTIVE` (1) is never read anywhere (both checks commented out), only `STACKABLE` (2) matters.

### Lifecycle & tick

- `LoadConfig` runs on **all machines** (clients need titles/effects for the map UI); `PostInit` handler bootstrap is server-only — all modifier mutation originates server-side.
- Every 10 s per town: decrement each non-permanent modifier's `timer` by 10 (timeout unit = real seconds), rebuild the list dropping expired ones (`OnTimeout` → broadcast remove), then call every config handler's `OnTick(town)`. `OnActiveTick` has zero overrides (dead hook, unreachable for permanent modifiers anyway).
- Stacking on add: absent → insert; present + STACKABLE → insert up to `stackLimit` else refuse; present + non-stackable → reset timer (stability's branch lacks the `timeout > 0` guard its support twin has — pointless resets for permanent modifiers).
- `Recalculate` = `round(clamp(100 + Σ effects, 0, 100))`; because base is 100 and the clamp caps at 100, **positive modifiers are repair, never bonus** — StrongEconomy/MedicalSupplies only matter while negatives are active.

### Networking & persistence

Add: local insert via the synchronously-self-executing `RpcAsk_AddStabilityModifier` → `RecalculateStability` → broadcast add + set-stability. Remove/timeout/reset each broadcast. Clients never tick timers — their lists exist purely for the map UI chips; the displayed percentage arrives via `RpcDo_SetStability`. JIP carries full `{id, timer}` lists per town.
Persistence: parallel id/timer arrays per town; on load ids `< 0` or `≥ config count` are dropped with a warning (ids are config **indices** — reordering `stabilityModifiers.conf` silently remaps every save), then stability is **recomputed from the restored modifiers** (the saved int is only a fallback). Handler-internal hour-gates aren't persisted, so PatrolHarassment/discontent re-fire immediately after load.

---

## Key Technical Decisions

### Decision 1: Stability is derived, never accumulated
**Implementation:** Every path is modifier-change → `Recalculate` → store; the serializer re-derives on load.
**Trade-offs:** Zero drift, exact save round-trips, removal restores the prior value perfectly; the cost is that everything meaningful must be expressed as a modifier.

### Decision 2: One base class, two subsystems, three routing virtuals
**Implementation:** The base knows timers and sums, not what a town is; stability keeps base `Recalculate`, support replaces it (see towns/support for the seam table).
**Trade-offs:** Genuine reuse (the support feature is ~78 lines); but the base's rebuild loop has the permanent-modifier defect (BUG-057) that hits both systems.

### Decision 3: Content in `.conf`, behaviour in optional handlers
**Implementation:** `handler` is an inline `UIWidgets.Object`; a handler-less entry is a valid externally-driven modifier (RecentBattle, MedicalSupplies).
**Trade-offs:** Effect tuning without code; but omitted attributes silently take defaults (DrugProblems' effect, five entries' stackability) and `flags` half-lies (ACTIVE is inert).

### Decision 4: Modifier ids are config indices
**Implementation:** Two ints per entry on the wire and in saves; defended on load by the drop-with-warning pass.
**Trade-offs:** Compact; but config order is load-bearing for saves and for client UI indexing (`OVT_MapContext` indexes `m_aModifiers[data.id]` unguarded — a server/client config mismatch is an out-of-bounds read in the map).

### Decision 5: Two time bases
**Implementation:** Timeouts count real seconds (tick-decremented); PatrolHarassment gates on the in-game hour.
**Trade-offs:** Config timeouts read naturally; but one system on two clocks confuses tuning, and the hour-gate is stored per-handler (BUG-058).

---

## Current State

### What's Working
- The framework: add/remove/timeout/reset with stacking rules, replication incl. JIP chips, persistence with recompute-on-load — the best-tested seam in the towns epic (Logic maths + Persistence/RoundTrip suites + a Campaign tax case all drive it)
- Transaction-driven modifiers (StrongEconomy/BlackMarket, mutually exclusive by shop type), death modifiers, QRF RecentBattle, medical supplies

### Known Issues (filed)
- **BUG-057**: permanent (timeout 0) modifiers are silently wiped whenever any other modifier in the same town expires — the tick rebuild skips `timeout <= 0` entries before re-inserting; no remove RPC is sent, so clients keep stale chips forever and accumulate duplicates on re-add
- **BUG-058**: `PatrolHarassment` (and support's `GrowingDiscontent`) store their hour-gate on the single shared handler instance — only `m_Towns[0]` is ever processed; every other town never gains or sheds these modifiers
- **BUG-059**: handler `OnStart` never runs — `system.Init()` is called before `InitializeTowns()`, so `PostInit` iterates an empty town list; the configured game-start random-event burst (20–30× CorruptMayor/OrganizedCrime/DrugProblems) is dead and campaigns always open at a flat 100 everywhere
- **BUG-060** (towns/core): `RpcAsk_AddStabilityModifier` trusts `townId`/`index` — crafted clients can crash the server or spam RecentBattle (−50) onto every town

### Technical Debt (unfiled)
- `m_Config.m_aModifiers[id]` indexed unguarded in `OnTick`, `Recalculate`, both RPC handlers and the client map UI — only the persistence path validates ids
- Null-entry deref in the stability stack counter (the support twin and the tick loop both guard; git history `2587e9c` proves null entries occur)
- Both economy handlers accept-and-ignore `isBuying`; since the BUG-020 fix (`e82b892`) sells also invoke `m_OnPlayerTransaction` (`isBuying=false`), so BlackMarket now fires on dealer sells (arguably correct) and StrongEconomy on any shop sell ≥ $50 (review whether intended)
- Dead: RecentGunfire (idx 0), `OnActiveTick`, `OnDestroy` (never called — subscriptions never removed), the ACTIVE flag, `OVT_BaseUpgradeTownPatrol`'s three nonexistent modifier names (BUG-066, filed under towns/support)
- Perf: `GetModifierSystem` string-compare + `TryAddByName` linear scans in the hot tick; near-duplicate CivilianDeath/BlackMarket/OFDeath handler pairs across the two systems (~70 lines each, same filter chains)
- `s_AIRandomGenerator` used directly (no injection seam — the exact reason random branches are untestable)
- Removal drains one stack per call — a town with 5 PatrolHarassment stacks needs 5 clean hours to shed them
- Name collisions across configs (`MedicalSupplies`, `BlackMarketActivity`, `CivilianDeath`, `RecentBattle` exist in both at different indices)

---

## Future Enhancements

### High Priority
- [ ] Fix the tick rebuild to retain permanent modifiers + broadcast drops (BUG-057)
- [ ] Per-town hour-gates (`map<int,int>` keyed by town id) in PatrolHarassment/GrowingDiscontent (BUG-058)
- [ ] Reorder `Init` (or defer handler `OnStart`) so the game-start burst runs (BUG-059)

### Medium Priority
- [ ] Bounds-validate ids at every index site (with BUG-060); guard the stack counter's null entries
- [ ] Make the transaction handlers honour `isBuying` now that sells invoke the event (post-`e82b892`) — decide per modifier whether sells should count
- [ ] Wire RecentGunfire or delete it; decide the ACTIVE flag (implement or remove)

### Low Priority / Nice to Have
- [ ] Single event-fan-out handler per cause with two effects (halve the duplicated handler pairs)
- [ ] RNG injection seam so the random branches become testable
- [ ] Stack-aware removal; name-collision lint across the two configs

---

## Testing

### Current Coverage
- `OVT_TEST_Logic_Town.c` `Modifiers_RecalculateSumsAndClamps` (8 assertions: identity, sum, null-skip, caller base/floor, both clamps, round-not-truncate) + the support system's deterministic branches
- `OVT_TEST_PersistenceSuite` + `RoundTripSuite`: stability round trips through the real `TryAdd/Remove` seam, asserting `stability == Recalculate(modifiers)` (derived, never hardcoded)
- `OVT_TEST_Campaign_Economy`: a real modifier drives tax income down fractionally (proves the stability factor isn't truncated)
- Recorded can-fail proofs in dev-ops `findings.md` (effect mutation, absent-modifier removal, positive-pick mutation)

### Testing Gaps
- `OnTick` itself: decay, timeout removal, the rebuild (BUG-057 would be caught by one permanent+temporary case), `recalc` return
- Stacking/stackLimit, `TryAddByName`/`RemoveByName`, all six handlers, `PostInit`/`OnStart` wiring (BUG-059), RPC fan-out to clients
- Random handlers: untestable by design (no RNG seam) — recorded in the suite

---

## Dependencies

### Internal Dependencies
- **towns/core**: owns the data (`OVT_TownData.stabilityModifiers`), the tick, the API and all transport; stability gates population growth and the village flip there
- **towns/support**: shares the entire framework; cross-feeds both ways (discontent reads stability < 30, random stability reads support% < 50)
- **towns/gun-dealers** + **economy**: transaction events (BlackMarket/StrongEconomy), tax/donation/stock/NPC-buying consumers
- **occupying**: AI-death event, QRF RecentBattle + battle resolution, patrol deployments feeding PatrolHarassment, threat scoring prefers low stability, deployment min/max stability gates (upstream hazard: BUG-028's deployment leak eventually starves PatrolHarassment)
- **towns/map-info**: renders the replicated chips (unguarded config indexing client-side)
- **core/persistence**: serializer + recompute-on-load invariant

### External Dependencies
- Engine `CallLater`, `s_AIRandomGenerator`, time/weather manager (in-game hour)

---

## Notes

**Discovered Information:**
- The `RpcAsk`-executes-locally convention (towns/core context) is what makes the add path work at all — it looks like dead code otherwise
- `96ee803` fixed JIP modifier replication; `2587e9c` fixed null modifiers on array rebuild (evidence the rebuild path has bitten before — BUG-057 is its remaining defect)
- Because base stability is 100 with a 100 cap, the design has no headroom for "thriving" towns — positives only repair

**Retrospective Assessment:**
- The framework earns its keep: derived values + config content + optional handlers is exactly why the persistence migration and the test suites could use it as their seam
- The defect cluster is in the tick rebuild and handler instancing — small, localized, high-impact fixes (BUG-057/058/059 are each < 20 lines)
- The support feature's different `Recalculate` proves the abstraction boundary was drawn in the right place

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature towns/stability` to begin making improvements.*
