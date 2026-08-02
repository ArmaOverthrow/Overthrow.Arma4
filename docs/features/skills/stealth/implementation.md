# Skills: Stealth - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Early Overthrow Reforger port; reworked in commit `21e8539` ("clothing detection, ai detection, ui color changing")
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 23:15

---

## Executive Summary

The Stealth skill reduces how far away Overthrow's wanted system will "notice" the player. Five config levels scale `OVT_PlayerData.stealthMultiplier` from 1.0 down to 0.5, which shrinks the wanted-system AI scan radius from 255m to 130m. The skill framework itself (manager, XP, effect base class) is documented in `skills/core`; this feature covers the `OVT_StealthSkillEffect` class, its config levels, and its consumption inside `OVT_PlayerWantedComponent`.

**Key finding:** the skill does **not** make the player harder for AI to *see* — engine perception is untouched. It only shrinks the radius within which Overthrow's own wanted-level bookkeeping scans. An already-hostile AI at 200m still engages a level-5 stealth player normally; the player just doesn't accrue wanted level from it.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Let players invest in being detected less readily by the occupying faction

### Success Criteria
- [x] Detection radius shrinks per level (255m → 130m at level 5)
- [x] Recruits inherit their owner's stealth multiplier
- [ ] Detection *time* bonus (`m_fDetectionTimeMul`) — dead parameter, never consumed
- [ ] Disguise effectiveness bonus (`m_fDisguiseBonus`) — dead parameter, never consumed

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Systems/SkillEffects/OVT_StealthSkillEffect.c` | The effect: `OnPlayerData` assigns `player.stealthMultiplier = 1 - m_fDistanceMul` (line 15) |
| `Configs/Player/overthrowSkills.conf:57-101` | Skill key `"Stealth"`, 5 levels, `m_fDistanceMul` 0.1 → 0.5, one effect per level |
| `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c` | Consumer — the wanted/detection system (1 Hz `CheckUpdate` tick, owner-side) |
| `Scripts/Game/Data/OVT_PlayerData.c:48` | `stealthMultiplier` — DERIVED, `[NonSerialized()]`, default 1, reset by `ResetSkillEffects()` |

`stealthMultiplier` has exactly five references repo-wide: declaration, reset, the effect write, and two reads in the wanted component.

### Data Flow

1. Level bought/replayed → `OnPlayerData` assigns `stealthMultiplier` (assign-not-compose; ascending replay means highest level wins — depends on the config being monotonic)
2. `OVT_PlayerWantedComponent.CheckUpdate()` (1 Hz, on the machine that **owns** the character — controlling client for players, server for AI/recruits) caches it at `:397-411`:
   - Players: `m_PlayerData.stealthMultiplier`
   - Recruits: the **owner's** `OVT_PlayerData.stealthMultiplier` (recruits have their own skill system but no Stealth skill; inheritance is by design or accident — undocumented)
3. **Consumption A — scan radius** (`:413`): `distanceSeen = 5 + (m_fBaseDistanceSeenAt * m_fStealthMultiplier)` — base 250 (no prefab overrides), so 255m at no skill, 130m at level 5. Pure cull radius over all `AIAgent`s before faction/alive filtering and LOS checks.
4. **Consumption B — low-recognition early-out** (`:570`): `if(recognition < 0.2 && dist > (10 * m_fStealthMultiplier) && !inVehicle) return true;` — same variable, geometrically *inverted* meaning (scales a min-safe distance rather than a max-scan distance).

### Effective radii per level

| Level | `m_fDistanceMul` | multiplier | scan radius |
|---|---|---|---|
| — | — | 1.0 | 255 m |
| 1 | 0.1 | 0.9 | 230 m |
| 3 | 0.3 | 0.7 | 180 m |
| 5 | 0.5 | 0.5 | 130 m |

(The `+5` floor means "50% reduction" at L5 is actually 49.0% — UI text and math disagree slightly.)

### What stealth does NOT touch
- Engine perception (`CharacterPerceivableComponent`, `GetVisualRecognitionFactor()` at `:326`) — unmodified
- The disguise close-range check `CheckDisguisedAsOccupying()` (`:243-290`, difficulty-config 15m) — the exact site `m_fDisguiseBonus` was meant for
- Wanted-level decay timers (`:462-479`) — the natural home for `m_fDetectionTimeMul`

---

## Key Technical Decisions

### Decision 1: Scale Overthrow's bookkeeping radius, not engine perception
**Context:** Reforger's perception API offers limited hooks; the wanted system is Overthrow's own layer.
**Implementation:** Multiplier applied to the wanted component's scan radius only.
**Trade-offs:** Cheap and self-contained, but the player fantasy ("harder to be seen") is only half-delivered; hostile AI vision is unchanged.

### Decision 2: Single scalar on the player record
**Context:** Effects communicate with consumers via derived `OVT_PlayerData` fields (see `skills/core`).
**Implementation:** Only `m_fDistanceMul` lands on the record. The other two effect params were designed to be "pulled by detection code when needed" via `GetDetectionTimeBonus()`/`GetDisguiseBonus()` — but nothing retains the effect instance after `OnPlayerData`, so they are unreachable in practice.
**Trade-offs:** Consuming them requires either new derived fields or a lookup through `GetSkill("Stealth").m_aLevels[level-1].m_aEffects`.

---

## Current State

### What's Working
- Scan-radius reduction per level, including recruit inheritance
- Replay-safe on load/JIP (assignment semantics)

### Known Issues
- **Two of three effect parameters are dead at all three layers simultaneously**: not authored in config (default 0), no production code reads their accessors, and the character sheet's conditional branches never fire. A test comment (`OVT_TEST_Logic_Skills.c:95-96`) describes the intended pull-based design and is currently untrue.
- **Localization regression:** `SetDescriptionTo()` builds hardcoded English (`"Detection distance -%1%%"`) — commit `21e8539` replaced the original `#OVT-SkillEffect_Stealth` SetTextFormat call. The key still exists, translated, in all locale files.
- The wanted system itself contains several dead limbs adjacent to this skill: `skipNormalDetection` write-only local, `GetEffectiveWantedLevel()` stub with zero callers, `m_iLastSeen` computed but never read, and three difficulty knobs (`baseDisguiseEffectiveness`, `wantedReductionMultiplier`, `detectionRangeMultiplier`) authored in all four difficulty configs but read by nothing.
- `m_bWantedSystemEnabled` is declared `float` with an `m_b` prefix, and its `[RplProp()]` never gets `Replication.BumpMe()` — the flag never actually replicates.

### Technical Debt
See `context.md` for the full prioritized list from discovery.

---

## Future Enhancements

### High Priority
- [ ] Restore localization in `SetDescriptionTo()` (1-line revert to `#OVT-SkillEffect_Stealth`; all 7 locale files already carry the key)
- [ ] Decide: implement or delete `m_fDetectionTimeMul` / `m_fDisguiseBonus`. Natural implementations: gate wanted decay on the already-computed `m_iLastSeen`, and scale `CheckDisguisedAsOccupying()`'s close-range distance. Both can ship inert (config values are already 0)

### Medium Priority
- [ ] Rename or split the multiplier's two inverted uses (`:413` max-scan vs `:570` min-safe) before any refactor breaks one silently
- [ ] Pull the magic numbers (`+5` floor, `10` early-out, `0.2` recognition threshold, `20`m radio-tower radius) into `OVT_DifficultySettings`

### Low Priority / Nice to Have
- [ ] True perception integration (make AI genuinely see the player later/worse)
- [ ] Recruit-owned Stealth skill, or document owner-inheritance as intentional
- [ ] Perf: `CheckUpdate` walks every `AIAgent` in the world per second per component; a `QueryEntitiesBySphere` version exists commented-out at `:388`

---

## Testing

### Current Coverage
- `OVT_TEST_Logic_Skills_EffectsWriteOnlyTheirOwnField`: fresh default = 1; `m_fDistanceMul 0.4` → `0.6`; no cross-field spill; accessors echo their fields (proves nothing about gameplay)

### Testing Gaps
- The actual radius formula (`5 + 250 * mult`) is untested — needs extracting to a pure helper first
- The inverted `:570` early-out, recruit inheritance, config monotonicity (which the assign-semantics silently depend on), replay ordering
- The entire wanted system has zero test references

---

## Dependencies

### Internal Dependencies
- `skills/core` — effect base class, replay/reset lifecycle, character sheet rendering
- `OVT_PlayerWantedComponent` and the wanted/disguise system (consumer)
- `OVT_RecruitManagerComponent` (recruit → owner data resolution)

---

## Notes

**Retrospective Assessment:**
- The one implemented parameter works and is replay-safe; the feature's real story is the intended-but-unfinished detection-time and disguise mechanics, whose scaffolding (accessors, test assertions, localization branches, difficulty knobs, `m_iLastSeen` plumbing) all exists with no connecting wire.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature` to begin making improvements.*
