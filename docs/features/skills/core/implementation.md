# Skills Core (Framework) - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Early Overthrow Reforger port (pre-2025), evolved since
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 23:15

---

## Executive Summary

The skills framework gives every player persistent XP, a level curve, and spendable skill points that buy levels in config-defined skills. It owns the XP economy (what actions award XP), the level-up loop (notification + implicit skill points), the config-driven skill/level/effect data model, the character sheet UI where points are spent, and the replication + persistence of all of it. The individual skills built on top of it (Trade, Stealth, Diplomacy) are documented separately in `skills/stealth` and `skills/influence`.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Reward core gameplay loops (fighting, trading, building, jobs) with XP and levels
- Let players specialize via skills whose definitions live entirely in config
- Keep skill state server-authoritative, replicated to all clients, and persistent across sessions

### Success Criteria
- [x] XP awarded from six gameplay sources + jobs + QRF participation
- [x] Level curve with level-up notifications and implicit skill points
- [x] Config-driven skills (`overthrowSkills.conf`) with polymorphic effects
- [x] Character sheet UI to view progress and spend points
- [x] Skills/XP persist (vanilla persistence, format v2) and sync to JIP clients
- [ ] Server-side validation of skill purchases (currently client-trust — see Tech Debt)
- [ ] `OnPlayerSpawn` effects replayed on respawn/JIP/load (currently fire-once)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c` | Manager singleton (`OVT_Global.GetSkills()`): XP award/removal, skill level-up, RPC streaming, effect invocation |
| `Scripts/Game/Configuration/OVT_SkillsConfig.c` | Data model: `OVT_SkillsConfig` → `OVT_SkillConfig` → `OVT_SkillLevelConfig` → `OVT_SkillEffect` (extension point) |
| `Scripts/Game/Data/OVT_PlayerData.c` | `xp`, `kills`, `levelNotified`, `skills` map (STORED); derived effect outputs (`priceMultiplier`, `stealthMultiplier`, `diplomacy`, `permissions` — all `[NonSerialized()]`); level curve; `ResetSkillEffects()` |
| `Configs/Player/overthrowSkills.conf` | The config asset — 3 skills × 5 levels, bound on `Prefabs/GameMode/OVT_OverthrowGameMode.et:188-191` |
| `Scripts/Game/UI/Context/OVT_CharacterSheetContext.c` | Character sheet: XP bar, level, per-skill spend buttons (layouts under `UI/Layouts/Menu/CharacterSheet*`) |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:94-103` | `BuySkill()` / `RpcAsk_BuySkill()` client→server seam |
| `Scripts/Game/GameMode/Systems/SkillEffects/OVT_GivePermissionSkillEffect.c` | Generic effect: grants a string permission (idempotent via `GivePermission()`) |
| `Scripts/Game/GameMode/Systems/SkillEffects/OVT_StaminaSkillEffect.c` | Stub effect — engine exposes no stamina params; unreferenced by config; its `#OVT-SkillEffect_Stamina` key is unlocalized |
| `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` | `OVT_PersistedPlayer`: `kills`/`xp`/`levelNotified` + parallel `skillKeys`/`skillLevels` arrays; header (lines 1-24) is the canonical STORED/SESSION/DERIVED contract |

### Level Curve (`OVT_PlayerData.c:82-133`)
- `GetRawLevel() = 1 + 0.1 * sqrt(xp)`; `GetLevel()` floors it
- `GetLevelXP(level) = (level / 0.1)^2` → thresholds 0, 100, 400, 900, 1600…
- **Skill points are implicit:** available = `(GetLevel() - 1) - CountSkills()` where `CountSkills()` sums levels across all skills. Nothing to persist, nothing to desync. Computed in `OVT_CharacterSheetContext.c:53` and `:168`.

### XP Sources (all server-side)

| Source | Amount | Site |
|---|---|---|
| Player death (victim) | −1 | `OVT_SkillManagerComponent.c:168-171` |
| Buy from shop | +1 (hardcoded) | `:177-180` |
| Sell to shop | `1 + floor(amount * 0.01)` | `:186-190` |
| Kill occupying-faction AI | +5, also `kills++` | `:196-214` |
| Build / Place | `m_iRewardXP` from config | `:150-163`; `buildables.conf` 5-40, `placeables.conf` 0-5 |
| Job completion | `m_iRewardXP` from config | `OVT_JobManagerComponent.c:432-435`; jobs 10-30 |
| QRF participation | +2 per tick in range | `OVT_QRFControllerComponent.c:145` |
| Debug cheat | +100 | `OVT_OverthrowGameMode.c:552` (DiagMenu 255) |

Hooks are wired in `PostGameStart()` (`:46-62`) against invokers on the game mode, economy, occupying faction and resistance managers.

### Data Flow

**XP:** `GiveXP(playerId, num)` (`:233-245`) mutates the server record, broadcasts `RpcDo_SetPlayerXP` (Reliable/Broadcast), and fires a `"LevelUp"` notification when `GetLevel()` exceeds `levelNotified`. `TakeXP` clamps at 0 and never lowers `levelNotified` (a level lost to death never re-notifies — apparently intended, undocumented).

**Skill purchase (client → server → broadcast):**
1. Client `OVT_CharacterSheetContext.BuySkill()` (`:162-172`) — the *only* affordability check — sends `OVT_Global.GetServer().BuySkill(playerId, key)`
2. `RpcAsk_BuySkill` forwards **unconditionally** to `OVT_SkillManagerComponent.AddSkillLevel()` (`:92-108`)
3. Server increments `player.skills[key]`, invokes `OnPlayerData` effects, then either applies `OnPlayerSpawn` locally (if buyer is the host) **or** broadcasts `RpcDo_SetPlayerSkill` — never both (see Tech Debt #5)
4. Receiving clients update their copy; the owning client also replays `OnPlayerData` + `OnPlayerSpawn`

**Effect invocation timing:** `OnPlayerData` runs at buy time and is replayed on every data load; `OnPlayerSpawn` runs **once, at buy time, only on the buying machine** — never on respawn, JIP, or load. Latent gap: only the no-op Stamina effect uses `OnPlayerSpawn` today.

**Replay/rebuild:** `OnPlayerDataLoaded` (`:68-85`) replays levels 1..N ascending per skill. Effects **assign** rather than accumulate, so the highest level wins; permissions are the one accumulating effect, made safe by `GivePermission()`'s idempotency guard. The persistence path calls `ResetSkillEffects()` first (`OVT_PlayerManagerComponent.c:169`); the JIP path (`:673`) does **not** — safe only because JIP creates fresh records.

### Integration Points
- **Persistence:** skills travel as parallel `skillKeys`/`skillLevels` arrays; `ApplyPersistedSkills()` (`OVT_PlayerManagerComponent.c:183-214`) validates keys against the live config and drops unknown ones with a WARNING — the guard that lets a rebalanced config load an old save
- **JIP:** all skill/XP state rides `OVT_PlayerManagerComponent.RplSave/RplLoad`'s manual bitstream (`:600-676`); the skill manager itself has no RplSave/RplLoad
- **Consumers of derived fields:** economy buy pricing, wanted-system detection radius, supporter conversion, port import gating — see `skills/influence` and `skills/stealth`
- **Parallel system:** `OVT_RecruitData` duplicates the exact level curve and skill map for recruits (no shared base class)

---

## Key Technical Decisions

### Decision 1: Config-driven skills via `ScriptAndConfig`
**Context:** Skills need rebalancing without code changes; modders extend via Workbench.
**Implementation:** All four data classes derive `ScriptAndConfig`; effects are polymorphic (`OnPlayerData`/`OnPlayerSpawn`/`SetDescriptionTo`) and the manager never switches on type. A new effect kind is a ~15-line class.
**Trade-offs:** Very extensible; but effects that assign the same derived field would silently overwrite each other (nothing enforces one-field-per-skill).

### Decision 2: RPC broadcast streaming, not RplProp
**Context:** `OVT_PlayerData` must hold *offline* players' records, so it's a plain `Managed` map keyed by persistent ID, not a replicated component.
**Implementation:** Deltas via explicit Reliable/Broadcast RPCs (`RpcDo_SetPlayerXP/Kills/Skill`); full state via the player manager's hand-written JIP bitstream.
**Trade-offs:** Works for offline records; but every mutation site must remember to stream, and record-not-found races null-deref (Tech Debt #4).

### Decision 3: Derived fields are never persisted
**Context:** (Verbatim rationale, serializer header) "Persisting them would let a save and the skill config disagree, and a mod update that rebalanced a skill could never reach an existing campaign."
**Implementation:** The four effect outputs are `[NonSerialized()]` and rebuilt by replaying `OnPlayerData` per earned level after `ResetSkillEffects()`.
**Trade-offs:** Saves stay rebalance-proof; correctness now depends on effect idempotency and ascending-replay order — pinned by the Tier A logic tests.

---

## Current State

### What's Working
- Full XP → level → skill-point → buy-skill loop, in SP and dedicated MP
- Config-driven definitions; character sheet renders skills/levels/effects generically
- Persistence round trip (Tier D + quarantined Tier D′ both green) and JIP sync
- 13 localization keys across 6 languages

### Known Issues
- `RpcAsk_BuySkill` trusts the client completely (no point/level-cap validation server-side)
- Buying past a skill's max level indexes `m_aLevels[]` out of bounds — engine exception; the `if(!levelCfg)` guard on the next line is dead code
- Listen-server host purchases never broadcast to other clients
- `GetSkill()` null (unknown key from a JIP payload) is dereferenced — mismatched client/server configs crash on load

### Technical Debt
See the prioritized list in `context.md`. Headlines: client-trust `AddSkillLevel` (+ out-of-bounds crash), `OnPlayerSpawn` never replayed, JIP path skips `ResetSkillEffects()`, hardcoded XP amounts (5 sites), level curve duplicated in `OVT_RecruitData`, stale `generated-docs/`.

---

## Future Enhancements

### High Priority
- [ ] Server-side validation in `AddSkillLevel`: point affordability + `m_aLevels.Count()` bounds + null-`GetSkill()` gate (fixes the crash and the exploit in one place)
- [ ] Broadcast `RpcDo_SetPlayerSkill` unconditionally (fix listen-server desync)
- [ ] Replay `OnPlayerSpawn` effects from `OVT_OverthrowGameMode.OnPlayerSpawned()`

### Medium Priority
- [ ] `ResetSkillEffects()` on the JIP path for symmetry with the persistence path
- [ ] Build a `map<string, OVT_SkillConfig>` in `Init()`; make `GetSkill()` the single null-safe gate
- [ ] Move hardcoded XP amounts (death/buy/sell/AI-kill/QRF) into config

### Low Priority / Nice to Have
- [ ] Share the level curve between `OVT_PlayerData` and `OVT_RecruitData`
- [ ] Max-level indicator in the character sheet (currently indistinguishable from "no points")
- [ ] Implement or delete `OVT_StaminaSkillEffect` (+ its missing localization key)
- [ ] Early-out `OnPlace` for `m_iRewardXP 0` (skips a pointless broadcast RPC)

---

## Testing

### Current Coverage
- **Tier A (Logic):** `OVT_TEST_Logic_Skills.c` — effects write only their own field (`FindSpill()`), permission idempotency, level accessors (thresholds 0/100/400), fractional level progress; all mutation-proven
- **Tier B (Init):** `OVT_Global.GetSkills()` resolves non-null
- **Tier D (Persistence):** `OVT_TEST_Persistence_PlayerSkills_RoundTrip` — XP give/take + skill level through the public manager API
- **Tier D′ (Round trip):** `OVT_TEST_PersistenceRoundTrip_PlayerSkills_SurvivesSaveAndReload` — save → dirty → re-apply

### Testing Gaps
- `AddSkillLevel` past max level (the crash), null `GetSkill()`, the full BuySkill RPC path, `OnPlayerSpawn` timing, `levelNotified` behavior, all six XP source callbacks, `ApplyPersistedSkills` dropping an unknown key, character sheet UI
- `OVT_StaminaSkillEffect` deliberately unasserted (documented in test-coverage findings)

---

## Dependencies

### Internal Dependencies
- `OVT_PlayerManagerComponent` (records, persistence, JIP), `OVT_EconomyManagerComponent` (buy/sell invokers), `OVT_OccupyingFactionManager` (AI kills), `OVT_ResistanceFactionManager` (build/place), `OVT_NotificationManager`, `OVT_Global`
- Consumed by: `skills/stealth`, `skills/influence` (all concrete skill effects extend `OVT_SkillEffect`)

---

## Notes

**Discovered Information:**
- Skill state exists in three representations, each chosen for its medium: map in memory, parallel arrays in the save, (count, key, level) tuples on the JIP wire
- Several `ScriptInvoker` doc comments lie about arity (`m_OnPlayerBuy`/`m_OnPlayerSell` documented 4 args, invoked with 2; `m_OnAIKilled` typed 1, invoked with 2) — the skill manager's handlers match the actual calls

**Retrospective Assessment:**
- The config-driven effect model and the derived-fields-not-persisted contract are genuinely good architecture — rebalance-proof saves fall out for free
- The framework's weakest edge is uniformly the server boundary: every client→server seam trusts its caller

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature` to begin making improvements.*
