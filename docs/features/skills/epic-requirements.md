# Skills - Epic Requirements

**Created:** 2026-08-02
**Phase:** Retrospective (existing shipped systems)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic skills` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature skills/<feature>` consumes.

## Overview

The skills epic delivers player progression: XP earned from Overthrow's core loops (fighting, trading, building, jobs), a level curve granting spendable skill points, and config-defined skills whose effects shape gameplay (shop prices, import access, detection, supporter conversion). It owns the framework (`skills/core`) and the implemented skills (`skills/stealth`, `skills/influence`). All three were backfilled retrospectively via `/discover-feature` on 2026-08-02.

## Requirements

- Players earn XP from gameplay and spend implicit skill points on config-defined skills
- Skill definitions, levels and effects are pure config (`overthrowSkills.conf`) — rebalancing must never invalidate saves (derived fields recomputed, never persisted)
- Skill state is server-authoritative, replicated to all clients, JIP-safe and persistent
- Skill effects integrate with their consumer systems (economy pricing, wanted/detection, town support, port permissions) without those systems knowing skills exist

## Planned Features

1. **core** — XP/levelling/effect framework + character sheet — foundational; everything else consumes it.
2. **stealth** — Stealth skill and wanted-system integration — independent of influence.
3. **influence** — Trade + Diplomacy skills (prices, permissions, supporters) — independent of stealth.

## Dependencies

- `core` epic (game-mode manager registration, player-manager records/persistence/JIP)
- `economy` epic (buy/sell invokers as XP sources; `GetBuyPrice` as the Trade consumer)
- Occupying/resistance faction managers (AI-kill and build/place XP sources)

## Out of Scope

- Recruit skills/XP (`OVT_RecruitData` has its own parallel system managed by `OVT_RecruitManagerComponent`) — only its *inheritance* of the owner's stealth multiplier is documented here
- The wanted/disguise system itself (consumer of stealth, not part of this epic)
- New skill design (respec, new skill trees) — future planning via `/plan-feature skills/<name>`

---

*Consumed by `/plan-epic skills`. After planning, run `/plan-feature skills/<feature>` per feature in the recommended order.*
