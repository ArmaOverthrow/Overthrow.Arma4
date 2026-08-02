# Skills - Epic Overview

**Epic:** skills
**Status:** 📄 Documented (Retrospective)
**Last Updated:** 2026-08-02 23:30

> **This file is the epic marker.** Its presence in `docs/features/skills/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The skills epic owns player progression in Overthrow: the XP economy and level curve, the config-driven skill system (`OVT_SkillManagerComponent` + `overthrowSkills.conf`), the character sheet where points are spent, and the three implemented skills built on that framework — Stealth (detection radius), Trade and Diplomacy (prices, import permissions, supporter conversion). These features belong together because every skill is data in one config consumed by one manager, and every skill payoff flows through the same derived-field contract on `OVT_PlayerData` (assign-on-replay, never persisted). This epic was created by backfilling documentation for existing, shipped systems via `/discover-feature`.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/skills/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented (Retrospective) | — | XP economy, level curve, skill-point loop, config-driven effect framework, character sheet, persistence/JIP (`OVT_SkillManagerComponent`, `OVT_SkillsConfig`) |
| 2 | stealth | 📄 Documented (Retrospective) | — | Stealth skill: wanted-system detection radius 255m→130m via `stealthMultiplier` (`OVT_StealthSkillEffect` → `OVT_PlayerWantedComponent`); two of three effect params are dead |
| 3 | influence | 📄 Documented (Retrospective) | — | Trade + Diplomacy skills: shop-buy discounts, `Import`/`IllegalImports` port gating, supporter-conversion chance (`OVT_TradeDiscountSkillEffect`, `OVT_GivePermissionSkillEffect`, `OVT_SupportSkillEffect`) |

> Reference any feature with the slash form `skills/<feature>` (e.g. `/continue-feature skills/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

(Retrospective — this is the dependency order the existing code exhibits, and the order any rework should follow.)

1. **core** — Foundational. Owns the manager, the effect base class, the derived-field replay contract, and the XP/level loop; both skill features are pure consumers.
2. **stealth** — One effect class + one consumer (`OVT_PlayerWantedComponent`); independent of influence.
3. **influence** — Three effect classes + consumers in economy pricing, port UI and town support; independent of stealth.

**Dependencies between features:**
- core → stealth (effect base class, replay/reset lifecycle, `stealthMultiplier` field)
- core → influence (effect base class, replay/reset lifecycle, `priceMultiplier`/`diplomacy`/permission storage)
- stealth and influence are independent of each other (can be reworked in parallel).

---

## Integration & Architecture

- **Within the epic:** one config asset (`Configs/Player/overthrowSkills.conf`) bound to one manager on the game mode; effects are polymorphic `OVT_SkillEffect` subclasses that write **derived, never-persisted** fields on `OVT_PlayerData`, rebuilt on every load/JIP by replaying earned levels ascending after `ResetSkillEffects()`. Effects must assign (not accumulate) and stay idempotent — that contract is what makes saves rebalance-proof, and it is pinned by the Tier A logic tests.
- **With other epics / features:** XP sources hook economy (`m_OnPlayerBuy/Sell`), occupying faction (AI kills), resistance (build/place) and jobs; skill payoffs land in economy pricing (`core` epic's economy manager — Trade), the wanted system (Stealth), and town support (Diplomacy). Persistence rides `core/player-manager`'s serializer (parallel `skillKeys`/`skillLevels` arrays, format v2) — see `docs/features/core/player-manager/`.
- **Key architectural decisions for the epic as a whole:** config-driven definitions via `ScriptAndConfig`; RPC broadcast streaming rather than `RplProp` (records must exist for offline players); derived-fields-not-persisted; skill points implicit (`(level-1) - CountSkills()`).

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. Seeded from the 2026-08-02 discovery investigation (three parallel code-analysis agents); refresh with `/review-epic skills`.

- [ ] 💳 **Client-trust RPC seams** — core, influence — `RpcAsk_BuySkill` (no point/cap validation → out-of-bounds crash past L5, **BUG-032**), `RpcAsk_ImportToVehicle` (no permission/proximity/item check at all, **BUG-033**), `RpcAsk_AddSupporters` (client-side diplomacy roll, caller-supplied count, **BUG-034**). The epic's single biggest theme: every payoff crosses a seam that trusts the client. Also filed: listen-server host purchases never broadcast (**BUG-035**).
- [ ] 💳 **JIP replay path skips `ResetSkillEffects()` and dereferences unknown skill keys** — core (affects all) — `OVT_PlayerManagerComponent.c:673` vs the persistence path at `:169`; config-mismatch join crash + latent stacking hazard filed as **BUG-036**.
- [ ] 💳 **`OnPlayerSpawn` effects never replay** (respawn/JIP/load) — core — latent: only the no-op Stamina stub uses the hook today.
- [ ] 💳 **Dead scaffolding across the skill surface** — stealth, core — `m_fDetectionTimeMul`/`m_fDisguiseBonus` dead at config+code+UI; `OVT_StaminaSkillEffect` stub with unlocalized key; three unconsumed difficulty knobs; stealth description is a hardcoded-English localization regression (one-line fix).
- [ ] 💳 **Magic permission strings** — influence, core — `"Import"`/`"IllegalImports"` shared by config and two UI files with no constants and no revocation API.
- [ ] 💳 **Duplicated level curve** — core — `OVT_PlayerData` and `OVT_RecruitData` carry identical hardcoded curves; no shared base.

---

## Master Overview Rollup

- **Rollup status:** 📄 Documented (3/3 retrospective)
- **One-line summary for master:** Player XP, levelling and config-driven skills — framework (`core`), Stealth detection skill, Trade/Diplomacy influence skills — backfilled with retrospective docs. Discovery surfaced ~35 issues headlined by three client-trust RPC seams (BuySkill, ImportToVehicle, AddSupporters) and a level-cap crash.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic skills` after working on the epic's features, and run `/review-epic skills` to refresh the Tech Debt / Findings section.*
