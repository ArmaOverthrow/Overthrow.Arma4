# Starter Jobs Retirement — Requirements

**Epic:** new-player-experience
**Created:** 2026-08-04

## Overview

Remove the five "tutorial" starter jobs (`findGunDealer`, `findShop`, `placeEquipmentBox`, `recruitACivilian`, `placeACamp`) once tutorial-content teaches the same things. They are broken in multiplayer today — the global `m_iMaxTimes 1` cap means only the first player on a server ever receives them (BUG-037) — and their completion path broadcasts a server-side hint (BUG-040). Retirement by removal discharges both bugs.

## Requirements

- The five starter job configs are removed from the game mode's job list **without breaking positional `jobIndex` integrity** — the jobs epic documents the index as positional and append-only, so removal must either preserve slot positions (e.g. tombstone/disable in place) or prove index safety for saves and in-flight jobs; the chosen approach must be validated against a campaign save that has these jobs active or completed.
- Before removal lands, verify coverage: each thing the jobs taught (gun dealers exist/where, shops exist/where, the Place menu, recruiting, camps) has a corresponding live tutorial-content entry. Record the mapping in this feature's docs.
- Their small rewards ($50/XP dribbles) disappear with them; confirm nothing else (skills, achievements, other jobs' conditions) referenced these job configs or their completion.
- BUG-037 and BUG-040 are updated/closed with a pointer to this feature.
- Campaign/persistence test tiers still pass; extend the jobs-related test coverage if a removal-safety seam is assertable in the test world.

## Dependencies

- `tutorial-content` must be complete and shipped (coverage must exist before removal).
- Touches the jobs epic's config surface — coordinate with any concurrent jobs-epic work on `m_aJobConfigs`.

## Out of Scope

- Reworking the jobs system itself (per-player caps done right, reactive UI refresh, stage titles — the jobs epic owns that debt).
- Adding replacement jobs of any kind.
- Fixing BUG-037/BUG-040 *in place* (making the jobs work per-player) — the epic's decision is retirement, not repair; repair only becomes relevant if coverage verification fails.
