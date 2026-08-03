# Skills: Stealth - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Systems/SkillEffects/OVT_StealthSkillEffect.c` — the effect (42 lines; only line 15 touches game state)
- `Configs/Player/overthrowSkills.conf:57-101` — 5 levels, `m_fDistanceMul` 0.1→0.5
- `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c` — consumer; multiplier cached `:397-411`, used `:413` (scan radius) and `:570` (early-out)
- `Scripts/Game/Data/OVT_PlayerData.c:48` — `stealthMultiplier` (derived, never persisted)

---

## Important Decisions

- **Stealth scales Overthrow's wanted-scan radius only** — engine AI perception is deliberately untouched. Any "AI sees you less" work is new design, not a bug fix.
- **Assign-not-compose:** `stealthMultiplier = 1 - m_fDistanceMul`; correctness depends on the config levels being monotonically increasing (they are; nothing enforces it).
- **Recruits inherit the owner's multiplier** (`:403-411`) even though `OVT_RecruitData` has its own skill system — undocumented whether intentional.

---

## Gotchas & Learnings

- `m_fDetectionTimeMul` and `m_fDisguiseBonus` are **dead at three layers at once**: absent from config (default 0), accessors called only by a test, character-sheet branches never fire. The test comment at `OVT_TEST_Logic_Skills.c:95-96` describing the pull-based design is factually wrong — nothing pulls them. They're also architecturally unreachable: nothing retains the effect instance after `OnPlayerData`, so consuming them needs new derived fields or a config lookup.
- `SetDescriptionTo()` is a **localization regression** (hardcoded English, introduced in commit `21e8539`); `#OVT-SkillEffect_Stealth` is still translated in all 7 locale files — restoring it is one line.
- The same multiplier has **two geometrically opposite meanings**: `:413` scales a max-scan radius, `:570` scales a min-safe distance. Renaming/splitting before refactor is cheap insurance.
- The `+5` floor at `:413` makes the advertised percentage slightly wrong (49.0% at L5, not 50%).
- Adjacent dead code to be aware of when working here: `skipNormalDetection` (write-only), `GetEffectiveWantedLevel()` (no callers), `m_iLastSeen` (computed, never read — ready-made hook for detection-time), `IsDisguisedAsFIA()`/`IsDisguisedAsSupporting()` (no callers), and three unconsumed difficulty knobs (`baseDisguiseEffectiveness`, `wantedReductionMultiplier`, `detectionRangeMultiplier`).
- `m_bWantedSystemEnabled` is a `float` named like a bool, and its `[RplProp()]` never bumps — it doesn't replicate.
- The wanted tick runs on the machine that **owns** the character (controlling client for players) — so the derived multiplier must be correct client-side, which is why the JIP replay path matters (see `skills/core` gotchas).

---

*This context file was created retrospectively by analyzing existing code.*
