# New Player Experience - Epic Overview

**Epic:** new-player-experience
**Status:** 🔵 Planned
**Last Updated:** 2026-08-04

> **This file is the epic marker.** Its presence in `docs/features/new-player-experience/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The experience of a fresh Overthrow player is currently hostile: they are dropped into an occupied country with a random house, a car, $100 and a single 20-second hint that repeats every session because nothing tracks what they've been told. The only in-game teaching — five starter jobs — is broken in multiplayer (BUG-037: only the first player on a server ever receives them). This epic replaces that with an action-triggered tutorial system built on dismissable popups (the UX pattern most games use), plus a proper first-spawn welcome, an expanded field manual for reference depth, and retirement of the dead starter jobs.

The design constraint that binds every feature here: **Overthrow is a sandbox, and the tutorials must never break that.** Popups react to what the player already chose to do — they explain the system the player just touched and hint at what it enables, without ever assigning goals, objectives or a prescribed order. The player always decides what to do and how to do it; the tutorial's job is to make sure they understand the tools they're holding.

---

## Features

The constituent features of this epic, in build order. Each feature is a subfolder under `docs/features/new-player-experience/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | tutorial-system | Planned | — | Framework: config-driven tutorial entries, action-trigger wiring to existing manager invokers, server→client delivery, custom dismissable popup UI, per-machine seen tracking |
| 2 | field-manual | Planned | — | Expand the 1-entry Overthrow field manual into a per-system reference that tutorial popups deep-link to via "Learn more" |
| 3 | tutorial-content | Planned | — | The authored early+mid-game tutorial entries (home/money/shops/map/wanted/skills → recruiting/camps/base capture/FOB basics) with localization |
| 4 | first-spawn | Planned | — | First-spawn welcome sequence (your home, your car, your cash, what Overthrow is) and start-menu faction/difficulty descriptions |
| 5 | starter-jobs-retirement | Planned | — | Retire the five MP-broken tutorial starter jobs once popups teach the same things (closes BUG-037 by removal) |

> Reference any feature with the slash form `new-player-experience/[feature-name]` (e.g. `/continue-feature new-player-experience/tutorial-system`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **tutorial-system** — Foundational: every other feature either runs on this framework (content, first-spawn) or is only safe once it exists (starter-jobs-retirement). Building it first also de-risks the epic's two genuinely novel pieces: the custom popup UI (nothing like it exists in Overthrow today) and the server-trigger→client-popup bridge.
2. **field-manual** — Content/config work with no code dependency on tutorial-system, so it can be built **in parallel with #1**. It comes before tutorial-content because popup "Learn more" links need real field-manual entries to target.
3. **tutorial-content** — Depends on tutorial-system (the framework it's authored against) and field-manual (link targets). This is where the epic's player-facing value lands.
4. **first-spawn** — Depends on tutorial-system (the welcome sequence is popup-driven). Can be built **in parallel with #3**; ordered after it only because content covers where players actually bounce off, while first-spawn polishes the opening minutes.
5. **starter-jobs-retirement** — Last, deliberately: the starter jobs (broken as they are) must not be removed until tutorial-content demonstrably teaches everything they taught (gun dealers, shops, placing, recruiting, camps).

**Dependencies between features:**
- tutorial-system → tutorial-content (framework, trigger registry, popup UI)
- tutorial-system → first-spawn (welcome sequence uses the popup/sequence primitives)
- field-manual → tutorial-content (deep-link targets for "Learn more")
- tutorial-content → starter-jobs-retirement (coverage must exist before removal)
- Parallel pairs: tutorial-system ∥ field-manual; tutorial-content ∥ first-spawn
- External: none outside this epic. Touches (but does not depend on changes to) the jobs epic; BUG-037/BUG-040 are discharged by starter-jobs-retirement.

---

## Integration & Architecture

- **Within the epic:** tutorial-system owns the moving parts — a tutorial manager, a `Configs/` tutorial-entry config (configuration-over-code pillar), a custom popup UI context/layout, and a client-local seen-store. tutorial-content and first-spawn are then almost entirely config + stringtable authoring against that framework. field-manual is pure config (`Configs/FieldManual/`) joined to the rest by entry IDs referenced from tutorial entries.
- **With other epics / features:** Triggers subscribe to existing server-side manager invokers (`m_OnPlayerBuy`, `m_OnPlace`, `m_OnBuild`, `m_OnBaseControlChanged`, `m_OnRecruitAdded`, `m_OnTownControlChange`, `m_OnPlayerSkill`, `m_OnRecruitXPGained`, loadout invokers) rather than instrumenting call sites. Where an invoker is missing or dead (wanted-level changes have none; `m_OnPlayerTransaction` is never invoked), tutorial-system adds/fixes it in the owning manager. starter-jobs-retirement touches the jobs epic's config list (positional `jobIndex` is append-only — retirement must respect that constraint).
- **Key architectural decisions for the epic as a whole (decided at epic planning, 2026-08-04):**
  - **Sandbox-preserving tone:** entries inform ("Shops buy and sell — prices differ by town"), never direct ("Go buy a rifle"). No objectives, no markers, no completion tracking beyond "seen". No linear chains — every entry is independently triggerable.
  - **Custom Overthrow popup UI**, not the base game's `SCR_HintUIComponent` corner toast: title, body, optional image, Dismiss, "Don't show tips again", optional "Learn more" → field manual. The base game's `EHint` dedup enum is mod-hostile (can't be extended) and its presentation is too small for this UX.
  - **Per-machine seen tracking** (user decision, chosen over per-campaign persistence): seen-entry IDs + a global "disable Overthrow tips" flag stored in mod-owned local settings. A veteran never sees tips again on any campaign or server; consequence accepted: a fresh campaign does not re-show tips, and the server cannot see who has seen what.
  - **Server fires, client decides:** trigger events originate server-side in managers and are delivered to the acting player's client (per-player, not broadcast-visible); the client dedups against its local seen-store and renders. This keeps MP/dedicated/JIP correct without any persistence-format change.
  - **All strings via `#OVT-` stringtable keys** — no hardcoded English (an existing debt class this epic must not add to).

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`.**

- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

- **Rollup status:** Planned (0/5 features)
- **One-line summary for master:** Action-triggered, sandbox-preserving tutorial popups plus first-spawn welcome, expanded field manual, and retirement of the MP-broken starter jobs.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic new-player-experience` after working on the epic's features, and run `/review-epic new-player-experience` to refresh the Tech Debt / Findings section.*
