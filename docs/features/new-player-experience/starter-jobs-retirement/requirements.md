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

## Documentation handoff (from `field-manual`, 2026-08-08)

The `field-manual` feature audited every player-facing wiki page and deliberately left one piece of starter-jobs documentation standing. It is handed to this feature.

### The page and the section

- **Page:** `getting-started`, pageId **2**, at https://wiki.armaoverthrow.com/getting-started
- **Section:** the heading `### 1. Jobs System`, under `## Early Money-Making Strategies`. Inside it, the paragraph that begins `**Tutorial Jobs**:` is the starter-jobs description proper.
- **One further mention** on the same page: item 6 of the numbered list under `## Systems Worth Knowing About`, which reads `**Tutorial Jobs**: A set of starter jobs that introduce these features`.

### Why `field-manual` left it intact

Decision **D12** of `docs/features/new-player-experience/field-manual/implementation.md`: the five starter jobs still exist in the shipped game. De-versioning the page (removing its "New in v1.3" framing, fixing dead links, correcting stale figures) was in scope; deleting documentation of live behaviour was not. Removing the section before the code removes the jobs would leave the wiki wrong in the other direction, describing a game that does not yet exist.

### What `starter-jobs-retirement` must do

- Its own help and wiki sync pass removes the `**Tutorial Jobs**` paragraph from `### 1. Jobs System`, and item 6 from `## Systems Worth Knowing About`, at the point the job configs leave the shipped build. If the section heading `### 1. Jobs System` is left with nothing but the generic jobs description, it still reads correctly and does not need removing.
- The same pass checks whether anything each removed job taught (gun dealers, shops, the Place menu, recruiting, camps) now has a live tutorial-content entry or Field Manual page to point at, per the coverage requirement above.
- The release note at `v1_3` says "**3 new tutorial jobs**". That is a release note and is historically correct (see the count finding below). It is not to be edited.

### Count finding (established 2026-08-08, so this feature need not rediscover it)

The shipped number of starter tutorial jobs is **five**. All five configs exist and all five are registered in the job list:

- `Configs/Jobs/findGunDealer.conf`, `findShop.conf`, `placeEquipmentBox.conf`, `recruitACivilian.conf`, `placeACamp.conf`
- registered at `Prefabs/GameMode/OVT_OverthrowGameMode.et:30`, `:34`, `:36`, `:38`, `:40` inside `OVT_JobManagerComponent.m_aJobConfigs`

The wiki's `getting-started` said "Three new tutorial jobs in v1.3", so **"three" was the wrong figure for the current game** and has been corrected to five on that page. "Three" was not wrong when it was written: the three jobs added *in v1.3* were `placeEquipmentBox`, `recruitACivilian` and `placeACamp` (GUID series `65CD…`), while `findGunDealer` and `findShop` predate it (`5D9C…`). The `v1_3` release note's "3 new tutorial jobs" is therefore accurate as history and was left untouched.

All five carry `m_iMaxTimes 1` and `m_iMaxTimesPlayer 1`, which is the BUG-037 shape described above.

## Coverage handoff (from `tutorial-content`, 2026-08-09): THIS FEATURE IS NOW UNBLOCKED

The requirement above ("Before removal lands, verify coverage") is **discharged**. `tutorial-content` shipped ten tutorial entries on 2026-08-09 and recorded the **as-built** starter-job coverage mapping.

- **Authoritative mapping (read this, not the plan's proposal):** `docs/features/new-player-experience/tutorial-content/context.md`, section **"Starter-job coverage mapping: AS BUILT"**. It cites the job stage classes, the covering entry ids and the map-icon code, and it corrects one row of `tutorial-content/implementation.md` §3.5 that was proven wrong.
- **Entries are live** under `Configs/Tutorials/`, registered on `Prefabs/GameMode/OVT_OverthrowGameMode.et` in `OVT_TutorialManagerComponent.m_aEntries`. Their text lives in `Language/localization_Overthrow.st`.

### The five jobs and what now covers each

| Starter job | Covering tutorial entry id | Entry's "Learn more" page |
|---|---|---|
| `findShop.conf` | `economy-first-buy` (+ `map-first-open`) | `#OVT-FieldManual_Shops_Title` |
| `findGunDealer.conf` | `shops-first-gun-dealer` (+ `map-first-open`) | `#OVT-FieldManual_GunDealers_Title` |
| `placeEquipmentBox.conf` | `place-first-placeable` | `#OVT-FieldManual_Camps_Title` |
| `placeACamp.conf` | `place-first-placeable` (same entry; its body names the camp) | `#OVT-FieldManual_Camps_Title` |
| `recruitACivilian.conf` | `recruits-first-recruit` | `#OVT-FieldManual_Recruits_Title` |

### The two residual gaps this feature inherits

1. **Directed discovery.** `findShop` and `findGunDealer` wrote a *specific instance's* position into `job.location`, producing a marker pointing at one shop or one dealer. No tutorial entry points at an instance and none ever will. This is largely moot: `OVT_MapIcons` draws **every** registered shop (`TryCreateShopIcon:137-183`, per-type sprite) and **every** gun dealer (`TryCreateGunDealerIcon:111-134`, dedicated `"gundealer"` sprite, enumerated from `economy.GetGunDealers()` at `:626-638`) with **no** discovery or knowledge gate, for any player carrying a map. ⚠️ Do **not** repeat the struck claim in `tutorial-content/implementation.md` §3.5 that gun dealers lack their own icon; they have one.
2. **Recruit availability.** `recruits-first-recruit` fires on the **first recruit gained**, not on recruiting becoming possible, so a player who never uses "Recruit Civilian" is never told it exists. The job had the same completion condition but *appeared in the Jobs list* first; the tip system has no equivalent surface. If that matters, it is a content or UI decision for this feature to raise, not a defect in the tutorial entry.

Secondary note: `shops-first-gun-dealer` is triggered by a **transaction** at a dealer, so finding a dealer and buying nothing shows nothing.

### What disappears with the jobs

$100 cash total, 10 XP, and two field dressings, once per campaign per player, across all five. Nothing else references these five job configs.

### Wiki still owed by this feature

The `**Tutorial Jobs**` paragraph under `### 1. Jobs System` and item 6 under `## Systems Worth Knowing About` on `getting-started` are still standing and still accurate. `tutorial-content` was explicitly forbidden from touching them (its D10/task 3.3) and did not. They remain this feature's to remove when the configs leave the build. Note that `tutorial-content` also left one small wiki edit unapplied (a two-sentence description of the tip system on the same page) because the wiki MCP page-read path was failing on 2026-08-09; see that feature's Phase 3 report, and whoever fixes it may find both edits land in the same pass.
