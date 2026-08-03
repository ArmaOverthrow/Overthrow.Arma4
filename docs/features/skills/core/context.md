# Skills Core (Framework) - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (three-agent code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c` — the manager (318 lines)
- `Scripts/Game/Configuration/OVT_SkillsConfig.c` — data model + `OVT_SkillEffect` extension point
- `Scripts/Game/Data/OVT_PlayerData.c` — XP/level curve, skills map, derived fields, `ResetSkillEffects()`
- `Configs/Player/overthrowSkills.conf` — the skill definitions (bound on `OVT_OverthrowGameMode.et:188-191`)
- `Scripts/Game/UI/Context/OVT_CharacterSheetContext.c` + `UI/Layouts/Menu/CharacterSheet*` — the sheet
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:94-103` — BuySkill client→server seam
- `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` — persisted shape; header lines 1-24 are the STORED/SESSION/DERIVED contract

---

## Important Decisions

- **Derived fields are never persisted** — effect outputs (`priceMultiplier`, `stealthMultiplier`, `diplomacy`, `permissions`) are rebuilt by replaying `OnPlayerData` per earned level after `ResetSkillEffects()`. Keeps saves rebalance-proof. Do not persist these fields; do not write effects that aren't replay-safe.
- **Effects assign, they don't accumulate** — ascending level replay means the highest level wins. Level configs carry absolute values (L5 Trade discount is 0.15, not +0.05). A new effect that does `player.x *= …` breaks this contract.
- **Skill points are implicit** — `(GetLevel() - 1) - CountSkills()`. There is no skillPoints field anywhere; don't add one.
- **RPC streaming, not RplProp** — `OVT_PlayerData` is a plain Managed map (holds offline players), so every server mutation must explicitly broadcast (`StreamPlayerXP` etc.).

---

## Gotchas & Learnings

- `DoInvokeSkillData/Spawn` index `m_aLevels[newlevel-1]` **before** the null check — the `if(!levelCfg) return;` is dead code; out-of-range throws first. Bound `newlevel` before touching the array.
- `GetNextLevelXP()` is misnamed: it returns the current level's *ceiling* threshold (`GetLevelXP(GetLevel())`). `GetLevelProgress()` compensates with `GetLevelXP(GetLevel()-1)`.
- `OnPlayerSpawn` effects fire once, at buy time, on the buying machine only — never replayed on respawn/JIP/load. Anything real built on `OnPlayerSpawn` needs a replay hook first (`OVT_OverthrowGameMode.OnPlayerSpawned()` is the natural site).
- The JIP path (`OVT_PlayerManagerComponent.c:673`) calls `OnPlayerDataLoaded` **without** `ResetSkillEffects()`, unlike the persistence path (`:169`). Safe today only because effects assign/are idempotent.
- Listen-server host skill purchases skip the broadcast RPC entirely (`AddSkillLevel:102-107`) — remote clients never learn the host's skill levels.
- `ApplyPersistedSkills()` validates keys against live config and drops unknown ones — that guard exists because `OnPlayerDataLoaded` null-derefs `GetSkill()`. The JIP payload path has no such filter.
- `OVT_StaminaSkillEffect` is a stub ("can't do anything until BI exposes stamina params"), unreferenced by config, with an unlocalized description key.
- `TakeXP` never lowers `levelNotified`, so a level lost to the death penalty never re-notifies on regain.

---

*This context file was created retrospectively by analyzing existing code.*
