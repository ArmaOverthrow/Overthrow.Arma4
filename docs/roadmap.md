# Overthrow Roadmap

**Status:** DRAFT — version numbers and ordering are provisional
**Created:** 2026-08-03 (post-discovery strategy discussion)
**Last Updated:** 2026-08-03

> The product roadmap agreed after the full feature/epic discovery of 2026-08-02/03. Version numbers beyond 1.4.x are **drafts for scheduling discussion**, not commitments. The single open ordering decision — new-map vs virtualization first — is deliberately deferred until 1.4.0 has reached the player-base (see Decision Points). GitHub issues remain the public source of truth for feature ideas; this file records how they sequence into releases.

---

## Where we are (2026-08-03)

- **Discovery complete:** all 30 features across 8 epics documented (`docs/overview.md`); 76 internal bugs filed (53 open, 19 high priority), heaviest in resistance and towns. Two dominant bug classes: client-trusted RPCs without server validation, and item/money duplication paths.
- **vanilla-persistence merged to main** (PR #151), gate-verified, awaiting the 20-item manual play-test. Breaking save change.
- Old local branches: `new-map` is fresh (~43 commits behind, ~5,200 lines of coherent map-UI work); `virtualization` is obsolete (264 commits behind, EPF-based persistence — design salvaged into the new virtualization epic, code discarded).

---

## Release plan

| Version             | Theme                                             | Contents                                                                                                                                                                   | Status     |
| ------------------- | ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| **1.4.0**           | Vanilla persistence + bugfixes + Economy QOL      | EPF → vanilla persistence migration + discovery-phase bugfixes. New shop and economy QOL (economy/shop-ux feature) Breaking save change.                                   | In testing |
| **1.4.x**           | Stabilization patches + Virtualization            | Player-reported bugs as 1.4.0 reaches the player-base (see below). The `virtualization` epic (docs/features/virtualization/) — GitHub #100 rebuilt on vanilla persistence. | Planned    |
| **1.5.0** _(draft)_ | New map + High Command #24                        | Revive and land the `new-map` branch — GitHub #70 and High Command (unlocked by Virtualization and integrated into the new map).                                           | Proposed   |
| Unscheduled         | Economy 2.0, Victory/defeat conditions, Intel #11 | See Backlog.                                                                                                                                                               | —          |

---

## 1.4.x — stabilization (immediately after 1.4.0 ships) + virtualization

Handle any bugs reported by the player-base from 1.4.0 persistence migration.

The planned epic at `docs/features/virtualization/` (4 features: core → movement → integration → base-defense-migration; the last is deferrable). Keeps issue **#100**'s aim, discards the old branch, builds on vanilla persistence.

- Hard prerequisite: 1.4.0 persistence proven stable in the wild.
- `base-defense-migration` (retiring base-upgrades) may slip to a later release without blocking the epic's value — its cost is scoped for exactly this decision.

---

## 1.5.0 (draft) — new map system + high command

Revive `new-map` (GitHub **#70**): location types (bases, shops, houses, camps, FOBs, warehouses, ports, gun dealers), new fast travel, respawn selection, town info; retires the "home" concept.

- Rationale for this slot: lowest risk (branch is nearly current), most player-visible, and it retires `towns/map-info` debt (4 open bugs incl. BUG-067 every-frame icon validation) rather than competing with it — the towns docs already call it the designed successor.
- UI-heavy → leans on manual play-testing; fits a release where the automated spine can't help much.
- First step when scheduled: merge main into the branch, re-review against the towns discovery docs, then `/discover-feature` or `/plan-feature` it into the towns epic structure.

## 1.7.0 (draft) — Economy 2.0

GitHub **#99** — player/resistance shop ownership, store money accounts, bulk trading, export, industry chains, economic data. THE differentiator per the issue.

- Sequenced after virtualization because its design depends on it (virtual citizen purchases, virtual deliveries); virtualization/core carries the documented extensibility seam.
- Needs its own `/plan-epic economy-2` (or extension of the economy epic) when its slot approaches.

---

## Third track — core/controller-migration

`docs/features/core/controller-migration/` (planned 2026-08-03): retire the 60-RPC `OVT_PlayerCommsComponent` monolith onto domain components on `OVT_OverthrowController`, server-side validation riding every migrated RPC, monolith deleted at the end.

- **Not version-pinned.** Starts any time after the 1.4.x patch cycle settles (shared file churn); architecturally independent of new-map, virtualization and Economy 2.0.
- Logistically wide (62 call sites across managers/UI) — best run _between_ big epics or by a second contributor, not concurrently with another wide-touching epic.

---

## Backlog (unscheduled)

- **Victory/defeat conditions** — flagged by both occupying and towns discovery as absent entirely; a real product gap for the "grind the occupiers out" fantasy. Could headline any release 1.5–1.7+.
- **Intel system** (#11), **Undercover system** (#8 — ties into the freshly documented wanted-system; fix its 5 open bugs first)
- Smaller enhancements as patch-release filler: Custom difficulty settings (#126), Vehicle storage (#83), Renaming bases (#51)
- **Building in towns** (#10) — re-triage after 1.4.x recruit fixes and Economy 2.0 planning respectively (may be absorbed by them).

---

_Review this file at each release boundary; it complements (not replaces) `docs/overview.md` (feature status) and the GitHub issue tracker (public feature ideas)._
