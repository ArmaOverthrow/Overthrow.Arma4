# New Player Experience - Epic Requirements

**Created:** 2026-08-04
**Phase:** Unscheduled (candidate for a 1.4.x/1.5.0 slot — see docs/roadmap.md)

> Epic-level requirements — the higher-level scope for the whole epic. `/plan-epic new-player-experience` reads this file to drive epic scoping. Each **child feature** below has its own `requirements.md` that `/plan-feature new-player-experience/[feature-name]` consumes.

## Overview

The new-player experience in Overthrow is currently terrible: one repeating 20-second hint, five multiplayer-broken starter jobs, and no explanation of the home/car/cash a player is handed. This epic delivers an action-triggered tutorial system built on dismissable popups plus supporting onboarding improvements — a first-spawn welcome, an expanded field manual, and removal of the dead starter jobs. Everything in it serves one outcome: a fresh player understands each Overthrow system as they touch it, without the sandbox ever being replaced by goals or linearity.

## Requirements

- **Sandbox is inviolable.** Tutorial entries explain systems and hint at possibilities; they never assign goals, show objectives/markers, or impose an order. Every entry is independently triggerable by the player's own chosen actions. The player decides what to do and how to do it, always.
- **Dismissable popup UX like most games:** a custom Overthrow popup (title, body, optional image, Dismiss, "Don't show tips again", optional "Learn more" linking into the field manual) — not the base game's corner hint toast.
- **Action-triggered:** entries fire when the player performs an action for the first time (buys, gets wanted, captures, recruits, places, levels up, …), wired to existing server-side manager script invokers wherever possible; missing/dead invokers (wanted-level change; `m_OnPlayerTransaction`) are added/fixed rather than worked around.
- **Per-machine seen tracking (decided 2026-08-04):** seen-entry IDs and a global "disable Overthrow tips" toggle live in mod-owned local settings on the player's machine. Tips never repeat across campaigns/servers for a veteran. Accepted consequences: fresh campaigns don't re-show tips; the server has no visibility into seen state.
- **Multiplayer-correct:** server-authoritative triggers delivered per-player (only the acting player sees the popup), safe under JIP and on dedicated servers. Singleplayer is the one-player case, not a separate path.
- **Configuration over code:** tutorial entries and their trigger bindings are `Configs/` data so servers and modders can add, remove or retune them without EnforceScript changes.
- **Localized:** every player-visible string is a `#OVT-` stringtable key; no hardcoded English.
- **Content covers early + mid game:** the first hour (home, money, shops, map/fast travel, wanted system, skills) and the first escalation (recruiting, camps, base capture, FOB basics). Late-game systems are deferred.
- **First-spawn onboarding:** a popup-driven welcome sequence explains what the player has been given (home, car, starting cash) and what Overthrow is; the start menu explains faction and difficulty choices.
- **Field manual as reference depth:** the existing 1-entry Overthrow field manual grows to one entry per covered system, deep-linked from popups.
- **The five starter tutorial jobs are retired** once popup content covers what they taught — discharging BUG-037 (global once-only cap makes them first-player-only) and BUG-040 (server-side broadcast hint on completion) by removal.

## Planned Features

1. **tutorial-system** — Config-driven tutorial framework: triggers, server→client delivery, custom popup UI, per-machine seen store — Foundational; every other feature runs on or waits for it, and it de-risks the two novel pieces (popup UI, trigger bridge).
2. **field-manual** — Per-system field manual reference entries with imagery — No code dependency; buildable in parallel with #1; precedes content because "Learn more" links need real targets.
3. **tutorial-content** — Authored early+mid-game tutorial entries + localization — Depends on #1 (framework) and #2 (link targets); the epic's main player-facing value.
4. **first-spawn** — Welcome sequence + start-menu faction/difficulty descriptions — Depends on #1 (popup primitives); parallel with #3.
5. **starter-jobs-retirement** — Remove/rework the five MP-broken starter jobs — Last; requires #3's coverage to exist before removal.

## Dependencies

- No external epic must change first. The epic **consumes** existing manager script invokers (economy, resistance, occupying, towns, recruits, skills) and **edits** the jobs config list only in its final feature.
- Base-game systems relied on: the field manual framework (`SCR_FieldManualConfigCategory` etc.), local user settings storage (`ModuleGameSettings` pattern), and the stringtable pipeline.
- Scheduling: unscheduled on the roadmap as of 2026-08-04; content-heavy and UI-heavy, so it fits a release that leans on manual play-testing (UI is outside the automated test spine).

## Out of Scope

- **Per-campaign / per-player-record seen persistence** — explicitly decided against (per-machine chosen). Revisit only if player feedback demands per-campaign resets.
- **Late-game system coverage:** warehouse/port/import, real estate trading, loadouts, vehicle upgrades, resistance donations — deferred to a follow-up content pass.
- **Linear tutorial missions, scripted intros, objective/quest UI, cinematics** — contrary to the epic's design constraint, not merely deferred.
- **Jobs-system rework** beyond retiring the five starter jobs (the jobs epic owns its other debt, e.g. reactive refresh, stage titles).
- **Interactive UI walkthroughs** (highlighting widgets inside Overthrow menus step-by-step) — the base game supports widget highlights, but authoring per-menu walkthroughs is deferred until the popup system proves itself.

---

*Consumed by `/plan-epic new-player-experience`. After planning, run `/plan-feature new-player-experience/[feature-name]` per feature in the recommended order.*
