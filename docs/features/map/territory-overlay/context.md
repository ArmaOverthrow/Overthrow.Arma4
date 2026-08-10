# Map Territory Overlay - Context & Decisions

**Last Updated:** 2026-08-11
**Current Phase:** Phase 0 — Baseline
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- Planning complete — `implementation.md` §4 records five decisions settled with the user on 2026-08-11
  (D1 weighted sites incl. FOBs/towers, D2 coastline clipping in, D3 threat grid **deferred**,
  D4 textured fills for both rings and bands, D5 no in-feature toggle).
- Dev docs scaffolded by `/start-feature map/territory-overlay`.

**What's Next:**
- **Phase 0** — re-measure the baselines, then **Phase 1**, the shared-canvas compositor
  (`component-developer-advanced`). Nothing in this feature can be evaluated visually until two layers
  can draw at the same time.

**Blockers:** none.

---

## Baselines (Phase 0)

| Gate | Plan's recorded value | Re-measured |
|---|---|---|
| `tools/compile-check.sh` | exit 0, **5964 files**, Game module | _pending_ |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | OK, **54 tests** | _pending_ |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) | OK, **89 tests** | _pending_ |
| Highest allocated bug id | **BUG-144** | _pending_ |
| Free GUID series | **`{6A84…}`** | _pending_ |

⚠️ `CLAUDE.md` says Fast 38 / All 66 and is **stale** — never quote it. A *changed* count at a phase
boundary is a finding to investigate, never a number to update.

---

## Key Decisions Made

_(Planning decisions live in `implementation.md` §4 and §6 and are not duplicated here. This section
records decisions made **during implementation** — the ones a future reader cannot recover from the plan.)_

---

## Probe results (Phase 2)

_Recorded verbatim once the user runs them. Until then, nothing here is known._

| Probe | Result | Observed signature |
|---|---|---|
| **P1** textured polygon | _pending_ | |
| **P2** non-convex fill | _pending_ | |
| **P3** `TriMeshDrawCommand` | _pending_ | |
| **P4** affine projection | _pending_ | |

**Chosen rung:** _pending_

---

## Measured numbers (Phase 6)

| Metric | Budget | Measured |
|---|---|---|
| Site count on the populated save | — | _pending_ |
| Solve time at map open | ≤ 250 ms | _pending_ |
| Rolling 60-frame `Draw()` average | ≤ 1.5 ms | _pending_ |
| Total composited commands/frame | ≤ 250 | _pending_ |

**Shipped tunables:** _pending_

---

## Still unverified

_The running list of everything that needs a human. Nothing is ticked here on reasoning alone._

---

## Where to look when it doesn't work

| Symptom | Most likely cause | First check |
|---|---|---|
| **Territory renders, rings vanish** (or the reverse) | The compositor is not composing — a layer is calling `SetDrawCommands` with its own list, or a bucket's frame stamp is never current | `Print` the composited command count per frame; if it equals one layer's count, the flush path is wrong |
| **Overlay is empty, no error** | `CollectSites` found nothing — a manager was null at map open (JIP window), or a site-type config entry is `m_bEnabled 0` | `Print` the site count per source. Zero on a client but non-zero on the host is a **replication** finding against the owning feature, not this one |
| **Cells look mirrored / inside-out / hinged on one corner** | Either the world-Z → screen-Y sign (P4) or the fan-from-vertex-0 artefact (P2) | Re-run P4 and P2. **They look similar and have completely different fixes** — P4's failure mirrors the whole cell, P2's fills the concave notches while leaving the convex hull correct |

---

## Session Notes

### 2026-08-11 — `/start-feature`
Docs scaffolded from `implementation.md` §5: 96 tasks across phases 0–9 plus the external/art rows.
Phases 1 and 6 flagged ADVANCED. Tree state at scaffold time: branch `new-map`, five modified files
from the preceding `map/location-types` work, highest bug id **BUG-144**, `{6A84…}` GUID series
confirmed unused.
