# Authoring Tools — Requirements

**Epic:** missions
**Created:** 2026-08-13

## Overview

Stretch goal: Workbench tooling that makes missions easier to author than raw `.conf` editing — possibly borrowing from the behaviour-tree systems, which provide a node-based editor that could be leveraged for branching mission graphs. Zero runtime impact; purely an authoring-time aid.

## Requirements

- Investigate whether the Workbench behaviour-tree node editor (or another Workbench plugin surface) can be leveraged to edit branching mission graphs visually; document the findings even if the answer is no.
- Whatever ships must emit/consume the standard mission config format from `missions/framework` — no parallel format.
- Validation tooling at minimum: a check that a mission config is well-formed (ids unique, branches resolvable, module parameters sane) — this is valuable even without a visual editor.

## Dependencies

- `missions/framework` config schema stable (ideally after `missions/mvp-missions` has exercised it).
- Can be built in parallel with `missions/recruit-missions` and `missions/dialog`.

## Out of Scope

- An in-game (runtime) mission editor.
- Editing tooling for non-mission configs.
- Any change to runtime behaviour.
