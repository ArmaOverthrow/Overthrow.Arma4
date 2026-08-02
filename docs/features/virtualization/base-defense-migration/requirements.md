# Base Defense Migration — Requirements

**Epic:** virtualization
**Created:** 2026-08-03

## Overview

Complete the migration the deployments design doc planned but never executed (phases 3–4 of `docs/archive/ModularDeploymentSystem.md`): re-express static base defense — today's nine base-upgrade classes — as deployment configs/modules running on the virtualization layer, then retire the base-upgrades system. This ends the "two force-placement systems sharing one budget" tension the occupying epic's discovery flagged as its core architectural problem, and replaces the third ad-hoc virtualization (banked-value "proxying") by construction. **Deliberately last and deferrable:** it is scoped here so its true cost is visible for scheduling, but features 1–3 ship standalone value without it.

## Requirements

- Deployment modules exist for static base defense at **behavioral parity** with the nine upgrade classes: garrison patrols, defense emplacements, tower guards, checkpoints, slotted compositions, parked vehicles, and specops — driven by the same difficulty/resource tuning that funds bases today.
- Groups belonging to these deployments are **virtualization-owned** (per-member state, proximity lifecycle); banked-value "proxying" is gone — the record of what exists at a base *is* the virtualization/deployment state, eliminating the upgrade-bank drift bug class (BUG-029 family) by construction.
- **One resource accounting path:** base defense spending flows through the deployments pool only; the dual-funding competition between base-upgrades and deployments ends.
- The **base-upgrades system is removed or permanently disabled** once parity is verified — including its legacy prefab-slot resolution path — with a documented decision if any class is intentionally dropped rather than migrated.
- **Legacy saves convert:** a campaign saved on the base-upgrades system loads with equivalent base defense re-established from the new configs (value-parity, not entity-identity).
- Occupying-faction behavior stays recognizable to players: bases garrison, fortify, and escalate with threat/difficulty as before; any deliberate behavior change is documented, not accidental.
- Logic-tier coverage for the new cost/selection maths; Persistence-tier coverage for a base-defense deployment round trip.

## Dependencies

- `virtualization/integration` complete — the deployments↔virtualization seam must be proven on the small consumers first.
- occupying epic coordination: the resource-economy bug cluster (BUG-026/027/029) intersects this work — fixing the spend chain should be sequenced with, not duplicated by, this migration.
- `docs/archive/ModularDeploymentSystem.md` (the original migration strategy) and the base-upgrades retrospective docs.

## Out of Scope

- New base-defense content beyond parity (new upgrade types, resistance base defense).
- QRF behavior and its bug pile (epic-level exclusion).
- Rebalancing the occupying faction's resource economy beyond what single-path accounting requires.
- Client-visible surface for base defense state.
