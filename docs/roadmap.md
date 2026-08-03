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

| Version | Theme | Contents | Status |
|---------|-------|----------|--------|
| **1.4.0** | Vanilla persistence | EPF → vanilla persistence migration + discovery-phase bugfixes. **No new features.** Breaking save change. | In testing |
| **1.4.x** | Stabilization patches | Player-reported and discovery-ledger bug burndown as 1.4.0 reaches the player-base (see below). | Planned |
| **1.5.0** *(draft)* | New map system | Revive and land the `new-map` branch — GitHub #70. | Proposed — pending the ordering decision |
| **1.6.0** *(draft)* | Virtualization | The `virtualization` epic (docs/features/virtualization/) — GitHub #100 rebuilt on vanilla persistence. | Epic planned 2026-08-03 |
| **1.7.0** *(draft)* | Economy 2.0 | The flagship differentiator — GitHub #99 — built on virtualization's foundations. | Idea — needs `/plan-epic` |
| Unscheduled | Victory/defeat conditions, High Command #24, Intel #11, Undercover #8 | See Backlog. | — |

---

## 1.4.x — stabilization (immediately after 1.4.0 ships)

Priority order:

1. **Dedicated-server recruit/group bugs** — GitHub **#147** and **#138** (likely `DelayedRpcAddRecruitToGroup` in `OVT_RecruitManagerComponent`). The worst player-facing problem given the multiplayer-first mission.
2. **Exploit-class internal bugs** — the unvalidated-RPC family (**BUG-025** `RpcAsk_InstantCaptureBase` first) and duplication paths (BUG-042/046/048). Hot-fixed on `OVT_PlayerCommsComponent`; the fixes carry forward into `core/controller-migration` later.
3. **High-priority discovery bugs** from the resistance/towns ledgers (19 high open at time of writing) via `/fix-bug` / `/fix-feature`.
4. **Verify-and-close candidates:** GitHub **#143** (disappearing vehicles/weapons) and **#142** (duplicating turrets) are persistence-shaped and may already be fixed by 1.4.0 — verify during testing, close with the release.
5. **Deployments lifecycle bugs** — BUG-028 (faction-list leak, the long-campaign kill switch) + world-time unit bugs. Wanted fixed *before* `virtualization/integration` starts (recorded in that feature's requirements).
6. Housekeeping: back-link BUG-001/003/004/005/008/010 to features; `/update-master` (resistance 4/4 → 5/5 drift).

**In parallel (not release-gated): finish the dev-ops epic** — `ci-pipeline` then `release-automation`. Cheap, already planned in `docs/features/dev-ops/epic-requirements.md`, and it makes the 1.4.x patch cadence and everything after it faster.

---

## 1.5.0 (draft) — new map system

Revive `new-map` (GitHub **#70**): location types (bases, shops, houses, camps, FOBs, warehouses, ports, gun dealers), new fast travel, respawn selection, town info; retires the "home" concept.

- Rationale for this slot: lowest risk (branch is nearly current), most player-visible, and it retires `towns/map-info` debt (4 open bugs incl. BUG-067 every-frame icon validation) rather than competing with it — the towns docs already call it the designed successor.
- UI-heavy → leans on manual play-testing; fits a release where the automated spine can't help much.
- First step when scheduled: merge main into the branch, re-review against the towns discovery docs, then `/discover-feature` or `/plan-feature` it into the towns epic structure.

## 1.6.0 (draft) — virtualization

The planned epic at `docs/features/virtualization/` (4 features: core → movement → integration → base-defense-migration; the last is deferrable). Keeps issue **#100**'s aim, discards the old branch, builds on vanilla persistence.

- Hard prerequisite: 1.4.0 persistence proven stable in the wild.
- Wanted first: BUG-028 + deployments time-unit fixes in 1.4.x.
- `base-defense-migration` (retiring base-upgrades) may slip to a later release without blocking the epic's value — its cost is scoped for exactly this decision.

## 1.7.0 (draft) — Economy 2.0

GitHub **#99** — player/resistance shop ownership, store money accounts, bulk trading, export, industry chains, economic data. THE differentiator per the issue.

- Sequenced after virtualization because its design depends on it (virtual citizen purchases, virtual deliveries); virtualization/core carries the documented extensibility seam.
- Needs its own `/plan-epic economy-2` (or extension of the economy epic) when its slot approaches.

---

## Third track — core/controller-migration

`docs/features/core/controller-migration/` (planned 2026-08-03): retire the 60-RPC `OVT_PlayerCommsComponent` monolith onto domain components on `OVT_OverthrowController`, server-side validation riding every migrated RPC, monolith deleted at the end.

- **Not version-pinned.** Starts any time after the 1.4.x patch cycle settles (shared file churn); architecturally independent of new-map, virtualization and Economy 2.0.
- Logistically wide (62 call sites across managers/UI) — best run *between* big epics or by a second contributor, not concurrently with another wide-touching epic.

---

## Backlog (unscheduled)

- **Victory/defeat conditions** — flagged by both occupying and towns discovery as absent entirely; a real product gap for the "grind the occupiers out" fantasy. Could headline any release 1.5–1.7+.
- **High Command** (#24), **Intel system** (#11), **Undercover system** (#8 — ties into the freshly documented wanted-system; fix its 5 open bugs first), **AI Driving** (#71 — engine-risk, keep watching Reforger releases).
- Smaller enhancements as patch-release filler: Custom difficulty settings (#126), Vehicle storage (#83), Renaming bases (#51), Mobile FOBs (#28), looting/sell loop (#145), WCS attachment compat (#136).
- **AI Recruits** (#22) / **Building in towns** (#10) — re-triage after 1.4.x recruit fixes and Economy 2.0 planning respectively (may be absorbed by them).

---

## Decision points

1. **New-map vs virtualization ordering** (decides whether 1.5.0/1.6.0 swap) — decide once 1.4.0 is stable with the player-base. Case for new-map first: nearly free, player-visible goodwill while virtualization gets properly planned. Case for virtualization first: starts the hard foundational work sooner; new-map merges are cheap to keep fresh.
2. **base-defense-migration timing** — in 1.6.0, or deferred to a later release (epic ships value without it).
3. **controller-migration slot** — between which releases, and who drives it.
4. **Economy 2.0 shape** — new epic vs extension of the existing economy epic; plan when 1.6.0 is underway.

---

*Review this file at each release boundary; it complements (not replaces) `docs/overview.md` (feature status) and the GitHub issue tracker (public feature ideas).*
