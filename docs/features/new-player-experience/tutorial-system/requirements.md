# Tutorial System — Requirements

**Epic:** new-player-experience
**Created:** 2026-08-04

## Overview

The framework every other feature in this epic runs on: a config-driven tutorial-entry system that listens to player actions via server-side manager invokers, delivers trigger events to the acting player's client, and renders a custom dismissable popup — with seen-state tracked per-machine in mod-owned local settings. Ships with only 1–2 proof entries; the real content belongs to `tutorial-content`.

## Requirements

- A tutorial entry is **data** (`Configs/`): id, title/body stringtable keys, optional image, optional field-manual link, and a trigger binding. Servers/modders can add or remove entries without touching EnforceScript.
- **Triggers subscribe to existing server-side script invokers** (e.g. `m_OnPlayerBuy`, `m_OnPlace`, `m_OnBuild`, `m_OnBaseControlChanged`, `m_OnRecruitAdded`, `m_OnTownControlChange`, `m_OnPlayerSkill`) rather than instrumenting call sites. Client-local events (opening the map, entering menus/contexts) may additionally trigger client-side.
- **Fill the invoker gaps this system needs:** add a wanted-level-changed invoker to `OVT_PlayerWantedComponent` (none exists; state is RplProp-only) and fix or bypass `m_OnPlayerTransaction` (declared but never invoked). Verify `m_OnPlayerBuy`'s actual signature (fires with 2 args, not the documented 4) before relying on it.
- **Server fires, client decides:** a server-side trigger reaches only the acting player's client (per-player delivery, not visible to others); the client checks its local seen-store and the disable toggle before showing anything. JIP-safe and dedicated-server-safe by construction.
- **Custom popup UI** (new layout + UI handling in Overthrow's visual style): title, body, optional image, Dismiss button, "Don't show tips again" control, optional "Learn more" button that opens the linked field-manual entry. Must support gamepad/keyboard navigation per existing menu-input patterns, queue politely if multiple entries trigger, and never appear during another modal Overthrow context.
- **Sequence support:** an entry can be a short multi-page sequence (Next/Back) — needed by `first-spawn`'s welcome flow; single-page entries are the norm.
- **Per-machine seen tracking:** seen-entry IDs and a global "disable Overthrow tips" flag stored in mod-owned local user settings (base game `SCR_HintSettings`/`EHint` cannot be extended by mods — do not reuse it). An entry id shows at most once per machine, ever.
- Every player-visible string is a `#OVT-` stringtable key.
- **Testable seams:** trigger→dedup→queue decision logic must be assertable in the Logic/Init test tiers (world-free where possible); popup rendering itself remains manual play-testing.

## Dependencies

- None within the epic — this is feature #1.
- Can be built **in parallel with** `field-manual` (no shared code).
- Relies on existing manager invokers listed above; adds missing ones in the owning components.

## Out of Scope

- Tutorial content beyond 1–2 proof entries (→ `tutorial-content`).
- The first-spawn welcome sequence content (→ `first-spawn`); this feature only provides the sequence primitive.
- Field-manual entries themselves (→ `field-manual`); this feature only implements the link-opening behavior.
- Per-campaign/per-player-record seen persistence (epic-level decision: per-machine only).
- Widget-highlight walkthroughs inside Overthrow menus (deferred at epic level).
