# Towns Support - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (pre-dates Beast Mode; handler modifiers from `d62e441`, div-zero guard `a4fb997`, poster refusal `79f5290`)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-03 00:30

---

## Executive Summary

Support is the resistance's loyalty axis: an **absolute supporter headcount** per town (stability is a percentage; support is people). It moves two ways: **directly** (converting civilians via the Diplomacy-skill roll, QRF resets, garrison/tent recruitment drawing supporters *and population* down) and **via modifiers** — but unlike stability, the support system's overridden `Recalculate` treats the modifier sum as a *rate*: at most **±1 supporter per 70 s per town**, rolled against the sum, with the sum doubling as an equilibrium target (gain chance drops to 25% once support% overtakes it). 13 configured modifiers (posters, medical supplies, deaths, battles, radio-tower/base proximity, discontent, revolutionary momentum). Support% gates the peaceful village flip (75/85 in, 25/15 out), triggers QRFs (>75 occupied / <25 resistance), scales donation income, and prices recruitment. There is **no victory condition anywhere in Overthrow** — support drives control, not endgame.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase (`/discover-feature`, 2026-08-02). The feature has already been implemented and shipped.

---

## Goals

### Primary Goals
- Make "winning hearts and minds" a slow, town-by-town campaign: conversion, posters, medical aid, and momentum vs. deaths, battles and OF infrastructure.
- Reuse the stability feature's modifier framework while keeping support path-dependent (a random walk, not a recomputed function).
- Feed the control loop: village flips, QRF triggers, recruitment supply, donation income.

### Success Criteria
- [x] ±1-per-cycle stochastic recalculation with equilibrium damping; div-by-zero guard on empty towns
- [x] 13 configured modifiers; direct mutators (convert/reset/take) replicated; raw-restore persistence (documented invariant)
- [x] Diplomacy skill (0.1 default → 1.0 at level 5) driving conversion chance
- [ ] Server-validated conversion (BUG-063 — entirely client-trusted today)
- [ ] Clamped accounting (BUG-064 — support can exceed population)

---

## Current Architecture

### Key Components

| Area | Files | Role |
|---|---|---|
| Support system | `Scripts/Game/GameMode/Systems/Modifiers/OVT_TownSupportModifierSystem.c` (78 L) | Routes the 3 framework virtuals to the manager's support methods; **replaces `Recalculate()`** with the ±1 random-walk algorithm |
| Handler base | `.../Modifiers/OVT_SupportModifier.c` (17 L) | Nearest-town helpers (`AddModifierToNearestTown` is dead — zero callers) |
| Handlers (5) | `.../Modifiers/Support/*.c` | BlackMarket, CivilianDeath, GrowingDiscontent, OccupyingFactionDeath (→ ResistanceVictory), RevolutionaryMomentum |
| Conversion | `Scripts/Game/UserActions/OVT_ConvertSupporterAction.c` (23 L) + `OVT_PlayerCommsComponent.c:83-92` | Client-side diplomacy roll → `RpcAsk_AddSupporters(pos, 1)` (unvalidated) |
| Posters | `Scripts/Game/GameMode/Placeables/OVT_PlaceableSupportModHandler.c` (13 L) + `Configs/Resistance/placeables.conf` | `OnPlace` → `TryAddByName("RecruitmentPosters")`; refusal (stack-full) deletes the placeable before charging (no flag placeable exists — posters only) |
| Diplomacy | `.../SkillEffects/OVT_SupportSkillEffect.c` (14 L) + `Configs/Player/overthrowSkills.conf:102-146` | **Assigns** `player.diplomacy = m_fSupportChance` (idempotent replay); levels 0.2/0.4/0.6/0.8/1.0 — level 5 always converts |
| Jobs seam | `OVT_TownSupportJobCondition` + `OVT_WaitTillSupportJobStage` + `Configs/Jobs/raiseSupport.conf` | Offered at exactly 0% support, completes at ≥10% (jobs epic owns these) |
| Config | `Configs/Modifiers/supportModifiers.conf` (103 L) | 13 entries — index = persisted/replicated id |
| Owner/transport | `OVT_TownManagerComponent.c` | `support`/`supportModifiers` on `OVT_TownData`; 70 s cadence; direct mutators; RPCs/JIP; raw-restore persistence (towns/core owns transport) |

### The `Recalculate` algorithm (the heart of the feature)

Called every **70 s** per town as `Recalculate(supportModifiers, town.support, 0, town.population)`:
1. Sum every active modifier's `baseEffect` (stacks count individually), clamp to ±100. Guard: `max == 0` → ERROR print + return unchanged.
2. Sum > +75 → deterministic +1; sum < −75 → deterministic −1.
3. Otherwise **roll**: positive sum → chance = sum while support% < sum, else sum × 0.25 (the equilibrium damping — a posters-only town trends to ~5% then crawls); negative sum → chance = |sum| always (no damping on decay).
4. Clamp result to `[0, population]`.

Consequences: pacing is population-independent (a 200-pop town needs ≥ 2.9 real hours of guaranteed rolls to reach 75%); the value is **path-dependent** — which is exactly why persistence restores support **raw** while stability recomputes (the repo's best-documented invariant, `OVT_TownManagerSerializer.c:50-71`).

### Framework seam vs towns/stability (which owns the base docs)

Support overrides four things, stability three: the routing virtuals plus `Recalculate`. Other deliberate asymmetries: `RpcAsk_AddSupportModifier` does **not** recalculate (stability's twin does — support waits for the 70 s cadence); the tick's support `OnTick` return is **discarded** (timeouts don't force a recalc); support's timer-reset branch guards `timeout > 0` (stability's doesn't).

### The 13 modifiers (idx = id)

CivilianDeath −20×5 (no instigator check — OF crossfire hurts the resistance), RecruitmentPosters +5×5, MedicalSupplies +1×25, NearbyRadioTowerNegative/Positive ∓50 permanent, RecentBattlePositive +25 / RecentBattleNegative −50 (7200 s), NearbyBaseNegative/Positive ∓25 permanent, ResistanceVictory +5×5 (OF death, only when stability < 50), BlackMarketActivity +5×3, GrowingDiscontent +1×10 permanent (stability < 30, hourly), RevolutionaryMomentum +10 (resistance town within 2 km; **stackable-by-omission** ×5 — BUG-065). The four proximity modifiers are re-added/removed every 10 s tick by the town manager itself; a QRF loss additionally calls `ResetSupport` outright ("avoids the battle looping").

### Direct mutators & consumers

`AddSupport` (unclamped — BUG-064), `TakeSupportersFromNearestTown` (refuses silently when short — free garrisons; removes support **and population** 1:1), `NearestTownHasSupporters` (the only guarded one), `ResetSupport`. Consumers: village flip + QRF trigger + OF threat scoring (all via `SupportPercentage()`), donation income (raw headcount × 5, doubled above 75 stability), garrison/tent recruitment, patrol deployment gate (≤50%), jobs, map/HUD/menus.

---

## Key Technical Decisions

### Decision 1: Headcount, not percentage — and a random walk, not a function
**Implementation:** `town.support` is people; the modifier sum is a probability/equilibrium-target, capped at ±1 per cycle.
**Trade-offs:** Slow, tuggable, narratively right; but path-dependence forces raw persistence, makes the value untestable deterministically (no RNG seam), and means modifier tuning changes *rates*, not levels.

### Decision 2: 70 s cadence, no recalc-on-add
**Implementation:** `SUPPORT_FREQUENCY` sub-tick; `RpcAsk_AddSupportModifier` deliberately skips recalculation.
**Trade-offs:** Bounds the walk speed; but a modifier added and expiring within one window does nothing, and the discarded `OnTick` return means timeouts never trigger an immediate update.

### Decision 3: Client-side conversion roll, server-side blind apply
**Implementation:** The Diplomacy roll, hint, and one-shot flag are all client-local; `RpcAsk_AddSupporters` applies whatever arrives.
**Trade-offs:** Zero latency UX; but it's the epic's biggest client-trust hole (BUG-063) — same class as the skills/resistance epic RPC findings.

### Decision 4: Poster refusal via the `OnPlace` bool contract
**Implementation:** `TryAddByName` returns false at stack-full → resistance manager deletes the entity *before* charging.
**Trade-offs:** Clean no-charge refusal; but silent — the player gets no hint, the poster just vanishes (and the handler has three unguarded derefs on townless maps).

### Decision 5: Diplomacy assigns rather than accumulates
**Implementation:** `player.diplomacy = m_fSupportChance` — replay-idempotent (test-pinned), safe under JIP skill replay without a reset.
**Trade-offs:** Correct-by-construction for this effect; the pattern only works for assignment-style effects.

---

## Current State

### What's Working
- The full loop: convert/posters/medical/momentum up; deaths/battles/towers/bases down; flips and QRFs fire from the thresholds; donation income pays out; recruitment draws down
- Raw-restore persistence round trips (same-session + save→dirty→reload suites green); deterministic `Recalculate` branches and `SupportPercentage` boundaries pinned in Logic tests

### Known Issues (filed)
- **BUG-058** (towns/stability): `GrowingDiscontent`'s shared hour-gate — only `m_Towns[0]` ever accrues/sheds discontent; stacks acquired elsewhere are permanent
- **BUG-063**: supporter conversion is entirely client-authoritative — the roll uses client data, `RpcAsk_AddSupporters` validates nothing (any client can flip every village); `MarkAsPerformed` is client-local so co-op players can all convert the same civilian
- **BUG-064**: unclamped accounting — `AddSupport` lets support exceed population (SupportPercentage() 300% is test-pinned), breaking every percentage gate; `TakeSupportersFromNearestTown` silent-fails when short (free garrison units, TOCTOU in MP); population-0 towns ERROR-spam every 70 s
- **BUG-065**: `RevolutionaryMomentum` is stackable by omission (default ×5 = +50 instead of +10) and drains one stack per flip — momentum is sticky in the wrong direction
- **BUG-066**: town-patrol modifier feedback is entirely dead — all three `OVT_BaseUpgradeTownPatrol` names (`RecentPatrol*`) exist in neither config, and its support gate compares the raw headcount to 75 as if it were a percentage

### Technical Debt (unfiled)
- `Math.AbsInt(float)` truncation in the decay roll (a fractional negative sum like −0.9 never decays — masked by all-integer shipped effects)
- `CivilianDeath` ignores the instigator (OF massacres cost the resistance support); `OnPlayerKilled` subscription is an empty stub
- `m_OnTownControlChange`/`m_OnAIKilled` invoker generics are wrong (`ScriptInvoker<IEntity>` invoked with `OVT_TownData`/two args) — subscribers disagree on signatures
- `RevolutionaryMomentum` never runs at game start (event-only) and is O(n²) per flip; `ResetSupport` writes the field directly while `AddSupport` calls the RpcDo locally (inconsistent local-apply idiom)
- Placeable proximity is client-enforced only, against the deprecated town ranges; poster refusal gives no feedback
- Support idx 3/4 reuse a stability localisation title; four modifier names exist in both configs at different indices
- Map panel truncates float effects into an `int` and labels support effects with `%`

---

## Future Enhancements

### High Priority
- [ ] Server-validate conversion (BUG-063): re-roll server-side from server-held diplomacy, distance-check, rate-limit
- [ ] Clamp support ≤ population at every mutation point; make `TakeSupporters` failure explicit to callers (BUG-064)
- [ ] `flags 1` on RevolutionaryMomentum (BUG-065) and per-town hour-gate for GrowingDiscontent (BUG-058)

### Medium Priority
- [ ] Fix or delete the dead patrol feedback (BUG-066) — wire real names + use `SupportPercentage()`
- [ ] Instigator-aware CivilianDeath (only player/resistance kills penalise support)
- [ ] Recalc-on-timeout (consume the `OnTick` return) so expiries take effect within the cycle

### Low Priority / Nice to Have
- [ ] Poster-refusal feedback hint; guard the placeable handler's derefs
- [ ] Float-safe decay roll; run momentum once at campaign start; typed invokers
- [ ] An RNG seam so the equilibrium-damping branch (the feature's most important line) becomes testable

---

## Testing

### Current Coverage
- `OVT_TEST_Logic_Town.c`: `SupportPercentage` boundaries (incl. the unclamped 300% pin), `Recalculate`'s seven deterministic branches (±75 thresholds, sum clamp, both value clamps, `max == 0` guard)
- `OVT_TEST_Logic_Jobs.c`: support condition min/max/unset (inclusive bounds); low-support random factor
- `OVT_TEST_Logic_Skills.c`: diplomacy default 0.1, assign-not-accumulate, no cross-writes
- Persistence + RoundTrip suites: `AddSupport`/`ResetSupport`/`TakeSupporters` seams round-trip raw through save/reload; Campaign economy: donation linear in support, doubled above 75 stability

### Testing Gaps
- **The equilibrium-damping branch** (support% vs sum → 25% chance) — the feature's core line, RNG-gated, untestable without a seam (honestly recorded in the suite)
- All five handlers (a two-town fixture would catch BUG-058 and BUG-065), stacking semantics, village-flip thresholds, `ApplyPersistedModifiers`' id-drop, `WaitTillSupportJobStage`, the placeable contract, conversion action, JIP payloads

---

## Dependencies

### Internal Dependencies
- **towns/stability**: the shared framework (base system/config/handler classes — documented there); cross-feeds (discontent reads stability, random stability reads support%, donation doubling, flip needs stability ≥ 50)
- **towns/core**: data + transport + the direct mutators + the village-flip rule (which lives inside `RecalculateSupport`)
- **resistance**: garrison/tent supporter draw-down; poster placeables (the `OnPlace` contract)
- **skills**: Diplomacy → `player.diplomacy` (its only reader is the conversion action)
- **occupying**: QRF triggers/outcomes (incl. `ResetSupport`), AI-death → ResistanceVictory, tower/base proximity, patrol deployment gate
- **economy**: donation income; black-market transactions; medical-supply deliveries
- **jobs**: raiseSupport condition/stage

### External Dependencies
- Engine time manager (hour gates), `s_AIRandomGenerator`, faction keys (`"CIV"`)

---

## Notes

**Discovered Information:**
- There is no victory condition anywhere in the game mode — support drives control flips and economy, not an endgame (matches the occupying epic's "no victory/defeat condition" finding)
- The raw-restore-vs-recompute persistence split is explicitly documented at both the serializer and the manager — the epic's best-documented invariant
- "Flags" as support placeables don't exist; posters are the only `OVT_PlaceableSupportModHandler` user

**Retrospective Assessment:**
- The ±1-with-equilibrium design is genuinely clever pacing — but it lives in one undocumented function whose most important branch is untestable; a seam + a doc comment would pay for themselves
- The client-trust conversion path is the standout risk; everything else is small, localized fixes
- The asymmetries vs stability (no recalc-on-add, discarded tick return, raw restore) are *mostly* deliberate — future maintainers need the table in this doc to not "fix" them wrongly

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature towns/support` to begin making improvements.*
