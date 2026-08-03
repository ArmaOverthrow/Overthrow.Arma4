# Field Manual — Requirements

**Epic:** new-player-experience
**Created:** 2026-08-04

## Overview

Expand the existing one-entry Overthrow field manual (`Configs/FieldManual/FieldManualConfigRoot.conf`) into a per-system reference covering every system the epic's tutorial content touches. Tutorial popups deep-link here via "Learn more" — the popup is the nudge, the manual is the depth.

## Requirements

- One field-manual entry per covered system: getting started (home, money, what Overthrow is), shops & economy, map & fast travel, the wanted system, skills & levelling, recruiting, camps, base capture, FOBs & building. (Mirror of tutorial-content's early+mid scope.)
- Each entry follows the existing pattern: `SCR_FieldManualConfig*` pieces with `#OVT-FieldManual_*` localization keys and imagery in the existing imageset/texture pipeline (`UI/Textures/FieldManual/`).
- Entries are **reference, not walkthrough**: they describe how a system works and what it enables, in the same sandbox-preserving voice as the popups (no goals, no prescribed order).
- Entry identifiers are stable and linkable so tutorial entries can reference them; document the id scheme for `tutorial-content` to consume.
- The existing Introduction entry is reviewed and folded into the new structure rather than duplicated.
- All new strings localized via the stringtable (`#OVT-FieldManual_*`); English source text at minimum, structured so translators can follow.

## Dependencies

- None within the epic — pure config/content work; can be built **in parallel with** `tutorial-system`.
- Must be complete (or at least entry ids frozen) before `tutorial-content` wires "Learn more" links.
- Relies on the base game field-manual framework (`SCR_FieldManualConfigCategory` etc.).

## Out of Scope

- Late-game system entries (warehouse/port, real estate, loadouts, vehicle upgrades) — deferred with the epic's content-depth decision.
- New imagery beyond what's needed to make entries legible (screenshot-grade images are fine; bespoke illustrated art is not required).
- Any code changes — if the base framework can't do something needed, that need goes to `tutorial-system` or is dropped.
