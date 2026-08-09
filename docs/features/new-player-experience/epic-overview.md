# New Player Experience - Epic Overview

**Epic:** new-player-experience
**Status:** 🟡 In Progress (3 of 5 features built)
**Last Updated:** 2026-08-09

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
| 1 | tutorial-system | 🟢 Built · ⏳ play-test owed | 61/61 (100%) · 2 cancelled by R3 | Framework: config-driven tutorial entries, action-trigger wiring to existing manager invokers, server→client delivery, custom dismissable popup UI, per-machine seen tracking |
| 2 | field-manual | ✅ **Complete** (play-test passed) | 56/56 (100%) | Expand the 1-entry Overthrow field manual into a per-system reference that tutorial popups deep-link to via "Learn more". Shipped: 12 entries under 4 sub-categories, 102 new string ids (exported), the twelve frozen link ids frozen and documented, and a full staleness sweep of the public wiki |
| 3 | tutorial-content | ✅ **Complete** (play-test passed) | 24/24 (100%) | The authored early+mid-game tutorial entries (home/money/shops/map/wanted/skills → recruiting/camps/base capture/FOB basics) with localization. **Ten entries live**, 18 new string ids + 1 rewritten body, zero gameplay EnforceScript |
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
- **What tutorial-system actually shipped (2026-08-07), for the three features that build on it:**
  - **The contract siblings consume is `tutorial-system/implementation.md` §5**, corrected against shipped code (its DoD item I6). It carries the entry-id scheme, the stringtable key scheme, the field-manual link rule, the full trigger catalog and the add-an-entry procedure. Read that, not this section.
  - **Adding an entry needs no EnforceScript** — demonstrated, not asserted: `welcome-intro` was added with 1 `.conf` + 5 string ids + 1 prefab line and zero script lines. Two proof entries ship (`economy-first-buy` NONMODAL/1-page/linked; `welcome-intro` MODAL/2-page/unlinked) and are the templates to copy.
  - **The trigger catalog is ten invokers, not the nine the plan said.** Three corrections matter to `tutorial-content`: `PLAYER_SELL` also fires from `AddPlayerMoney` and so does **not** mean "sold at a shop" (prefer `PLAYER_TRANSACTION`, which carries the shop); `PLAYER_SKILL` now carries the skill key in `m_sFilter`; and `m_OnTownControlChange` is declared `ScriptInvoker<IEntity>` but invoked with `OVT_TownData`.
  - **`field-manual` has a dedicated contract section** in `tutorial-system/context.md` — title keys *are* the link ids (exact, case-sensitive, renaming one breaks every popup pointing at it); new categories go in `Configs/FieldManual/Categories/FM_Overthrow.conf`, never the same-GUID root delta; and `SCR_FieldManualUI.SetAllEntriesAndParents` supports **only two category levels** and silently prunes empty nodes.
  - **`first-spawn` gets its sequence primitive** (multi-page modal with Next/Back/page indicator) plus a bounded `PLAYER_SPAWNED` retry that survives the async controller-assignment race. `#OVT-IntroHint` and `m_aHintedPlayers` are deliberately **untouched** — removing them is still `first-spawn`'s task.
  - ⚠️ **One acceptance criterion was retired, not met:** F5, the keybinding that escalated a non-modal popup to the modal. Risk R3 fired twice — there is no free gamepad input during gameplay (all 16 are bound in some context live under a popup; `shoulder_left` and `KC_T` are VON at Priority 110). The documented fallback was taken: the escalation route is **HUD prompt → Overthrow main menu → Tips**. No `chimeraInputCommon.conf` gameplay binding was added.

- **What `tutorial-content` shipped (2026-08-09), and what it hands to features #4 and #5:**
  - **The framework's central claim held under load.** Ten entries were added with **zero gameplay EnforceScript** — nine new `.conf` files, 18 string items, nine prefab lines, and one adopted proof entry. The only `.c` touched was a test file. `tutorial-system`'s "adding an entry needs no script" is now demonstrated at scale, not just by its own two proof entries.
  - **`starter-jobs-retirement` (#5) is UNBLOCKED.** The as-built starter-job coverage mapping is recorded in `tutorial-content/context.md` and appended to `starter-jobs-retirement/requirements.md` — five jobs, their covering entry ids, and two residual gaps (discovery is *directed* rather than absent, and the recruit tip fires on the first recruit rather than on the option becoming available). A finding worth carrying: the map already marks every shop and gun dealer **ungated by any discovery flag**, so the jobs' only unique contribution was a marker on one *named instance* plus $100 and 10 XP.
  - **`first-spawn` (#4) coordination point:** `home-first-open` covers what ownership *means* mechanically and deliberately goes deeper than the welcome's "here is your house". #4 must not add a second home entry. Both may link `#OVT-FieldManual_YourHome_Title` — link keys are not exclusive. `proofWelcome.conf`, `welcome-intro`, `#OVT-IntroHint` and `m_aHintedPlayers` were verified byte-untouched.
  - **One trigger gap recorded rather than worked around:** FOB *deployment* has no invoker anywhere (`DeployFOB` touches neither `m_OnPlace` nor `m_OnBuild`, and per-player signals exist only on rejection). Filed as a **non-blocking** `tutorial-system` note naming the `RegisterFOB` seam. The topic still ships — `build-first-structure` covers it from the build side.
  - **⚠️ A trap list is not evidence.** The plan's own pre-loaded trap table contained a **false row** (it claimed gun dealers have no dedicated map icon; they do, via a separate enumeration path and a `"gundealer"` sprite). Caught by a phase fact-check, verified independently, struck in three places before the wiki pass could inherit it. The lesson generalises to #4 and #5: re-verify a documented trap against source before relying on it.
  - **Two inherited Field Manual claims are now known-wrong and unfixed** (`Configs/FieldManual/` was out of scope every phase): `WantedSystem_Text`'s "the occupying faction comes looking for you" (no search or dispatch behaviour keyed to wanted level exists — only a perception override) and `BaseCapture_Text`'s "in that area" (`m_iThreat` is a single **global** counter). The tips shipped narrowed and correct; **the public wiki is already right on both**, which inverts the usual staleness direction.

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

- **Rollup status:** In Progress, **3/5 features built** (141/141 tasks across the three), **2 fully closed** (field-manual 56/56 ✅ and tutorial-content 24/24 ✅ both play-tested and signed off; tutorial-system 61/61 built, and since the content play-test exercised the framework end to end its own formal play-test is now largely discharged in practice; the remaining two planned, requirements only)
- **One-line summary for master:** The framework, the reference manual and the content are all built and green (compile 0, Fast 47, All 78) — **ten action-triggered tutorial entries now ship**, added with zero gameplay EnforceScript, which is the framework's central claim demonstrated at scale rather than asserted; play-tests owed on #1 and #3, then first-spawn and starter-jobs retirement close the epic.
- **What field-manual unblocks:** `tutorial-content` has its twelve "Learn more" targets (the frozen key table, `field-manual/implementation.md` §3.3) and `first-spawn` has `#OVT-FieldManual_Welcome_Title`. `starter-jobs-retirement` has a written documentation handoff in its own `requirements.md`.
- **What tutorial-content unblocks:** `starter-jobs-retirement` (#5) — its precondition, the as-built coverage mapping, is written into both `tutorial-content/context.md` and `starter-jobs-retirement/requirements.md`. #5 can now start.
- **Open items carried out of field-manual:** 16 numbered open questions for a gameplay owner in `field-manual/context.md` (surfaced by the wiki staleness sweep, not created by it), notably the officer-loadout sharing gap and whether a `difficulty/insane` page should be created.
- **Open items carried out of tutorial-content — all discharged 2026-08-09** except the last: the string export is done, the play-test passed, and the `getting-started` wiki paragraph is applied and verified live. **Still open: two known-wrong Field Manual strings** (`WantedSystem_Text`'s "comes looking for you", `BaseCapture_Text`'s "in that area"), listed in the Integration section above — the public wiki is already right on both.
- **Unverified rather than passed, carried forward honestly:** the two-tip queue (F5), two-client per-player isolation (F7 — no MP pass was run), `bases-first-capture` (P10), and the Learn More link spot-checks. None is known-broken; none was observed. **`first-spawn` inherits the MP question**, since it shares the same delivery path.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic new-player-experience` after working on the epic's features, and run `/review-epic new-player-experience` to refresh the Tech Debt / Findings section.*
