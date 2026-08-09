# New Player Experience - Epic Overview

**Epic:** new-player-experience
**Status:** 🟡 In Progress (4 of 5 features built, 3 fully closed)
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
| 4 | first-spawn | ✅ **Complete** (play-test passed) | 49/49 (100%) | First-spawn welcome sequence and campaign-setup descriptions. Shipped: **two** filtered welcome entries (house / houseless) on a new server→client spawn-context RPC, 4 pages each; the legacy `#OVT-IntroHint` retired; 4 setup description widgets + a runtime-composed numbers line; 15 string ids; 3 automated cases proven red. **Ships with the first page art any tutorial entry has ever carried** |
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

- **What `first-spawn` shipped (2026-08-09), and what it hands to feature #5:**
  - **The framework took its first real code extension, and it held.** Every prior feature added config and strings; this one added a **server→client owner RPC** carrying a per-player spawn context (`"house"` / `"nohouse"`) so the client can select between two welcome entries filtered on the same `PLAYER_SPAWNED` event. The client provably cannot derive the fact itself — `home != vector.Zero` is true in *both* spawn branches — so the server authors it in `FinalizePlayerPreparation` and pushes it. Only two `.c` files of `tutorial-system` were touched (D-set I2 held).
  - **`#OVT-IntroHint` is retired**, with its session-only dedup. Its "dead since the 1.6 spawn rework" code comment was re-verified and found **only two-thirds true** — it genuinely still fired on SP/listen-host respawn-after-death and after a Continue. Both string items were kept in place (six languages of translation) with a retirement record naming the three false claims in the hint's own text.
  - **⚠️ An image carries a claim exactly as a sentence does, and Rule 0 does not exempt it.** This feature is the first to ship page art, and the first wiring pointed **both** entries' page 2 at the same car-in-a-garage header — so the houseless entry illustrated "you own nothing" with a picture of a car in a garage. Caught by reading the images against the shipped copy. **The generalisation for #5 and any future content pass: fact-check art, not just prose.** A shared string key can be deliberate; a shared image across two entries that differ on that page is a bug.
  - **`starter-jobs-retirement` (#5) inherits one concrete liability:** `welcome-intro-3-ui` is a screenshot of the Overthrow menu with **Jobs visible**. The page-3 *text* deliberately omits Jobs for exactly this reason, but the image cannot. **If #5 removes the Jobs menu entry, that screenshot must be re-shot** — nothing automated will flag it.
  - **The plan's own test-count arithmetic was wrong** and was corrected rather than chased: it projected +3 cases, but one of the three was always specified as a *branch* on an existing guard, and a branch adds assertions rather than executed cases. Honest close: **Fast 51, All 88.**
  - **Three findings recorded, none fixed** (all are balance or save-format changes wearing bug-fix clothes): `realEstateCostMultiplier` is declared, replicated and read by **nobody**; `ChargeRespawn` only charges when `money > 500`, a hardcoded gate unrelated to the `respawnCost` setting; and the **supporting faction is neither replicated nor persisted**, so an MP client always evaluates the script default `"US"` and `OVT_WantedInfo.c:204-208` compares the undercover HUD icon against the wrong key when a host picked USSR.

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

- **Rollup status:** In Progress, **4/5 features built** (190/190 tasks across the four), **3 fully closed** (field-manual 56/56 ✅, tutorial-content 24/24 ✅ and first-spawn 49/49 ✅ all play-tested and signed off; tutorial-system 61/61 built, and since both the content and first-spawn play-tests exercised the framework end to end — including its multi-page modal sequence and, for the first time, its image path — its own formal play-test is largely discharged in practice; only #5 remains, requirements only)
- **One-line summary for master:** The framework, the manual, the content and the first-spawn welcome are all built and green (compile 0, Fast 51, All 88) — **twelve tutorial entries now ship**, ten of them added with zero gameplay EnforceScript and the two welcomes on a new per-player spawn-context RPC, the legacy intro hint is retired and the campaign-setup screen finally explains its three choices; only starter-jobs retirement remains.
- **What field-manual unblocks:** `tutorial-content` has its twelve "Learn more" targets (the frozen key table, `field-manual/implementation.md` §3.3) and `first-spawn` has `#OVT-FieldManual_Welcome_Title`. `starter-jobs-retirement` has a written documentation handoff in its own `requirements.md`.
- **What tutorial-content unblocks:** `starter-jobs-retirement` (#5) — its precondition, the as-built coverage mapping, is written into both `tutorial-content/context.md` and `starter-jobs-retirement/requirements.md`. #5 can now start.
- **Open items carried out of field-manual:** 16 numbered open questions for a gameplay owner in `field-manual/context.md` (surfaced by the wiki staleness sweep, not created by it), notably the officer-loadout sharing gap and whether a `difficulty/insane` page should be created.
- **Open items carried out of tutorial-content — all discharged 2026-08-09** except the last: the string export is done, the play-test passed, and the `getting-started` wiki paragraph is applied and verified live. **Still open: two known-wrong Field Manual strings** (`WantedSystem_Text`'s "comes looking for you", `BaseCapture_Text`'s "in that area"), listed in the Integration section above — the public wiki is already right on both.
- **What first-spawn unblocks and hands over:** `starter-jobs-retirement` (#5) is the epic's last feature and its precondition was already satisfied by #3. New from #4: **if #5 removes the Jobs menu entry, `welcome-intro-3-ui.edds` must be re-shot** — it is a screenshot of the menu with Jobs visible, and nothing automated will flag it stale.
- **Open item carried out of first-spawn:** one small Workbench string re-export for `OVT-FieldManual_Welcome_Text2` (the Field Manual's Welcome page gained a houseless clause in Phase 6, so it no longer contradicts the `welcome-nohome` entry that deep-links to it). It fails silently rather than showing a raw key — the manual just shows the older paragraph until the export runs.
- **Unverified rather than passed, carried forward honestly:** the two-tip queue (F5), `bases-first-capture` (P10), and the Learn More link spot-checks. **And the epic's biggest outstanding question is now bigger, not smaller: two-client per-player isolation (F7) has still never been observed passing, and `first-spawn` has added a new owner RPC to that same delivery path.** Its own F5/F6 (two clients + JIP) and F3 (the houseless page, which needs the starting houses exhausted) were not observed either. None is known-broken; none was observed. **An MP pass is the single highest-value verification left in this epic.**

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic new-player-experience` after working on the epic's features, and run `/review-epic new-player-experience` to refresh the Tech Debt / Findings section.*
