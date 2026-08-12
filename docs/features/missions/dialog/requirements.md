# Dialog — Requirements

**Epic:** missions
**Created:** 2026-08-13

## Overview

Stretch goal: a very simple text-only dialog system so NPCs can relay information and add flavour to missions, in a question/answer style. Not RPG-level — a mission module plus a minimal UI.

## Requirements

- A mission module attaches a dialog to an NPC; interacting opens a text-only Q&A UI (NPC line + selectable player responses).
- Dialog choices can drive mission branching (a response routes the mission instance down a path) and trigger modules (grant item, reveal location, advance objective).
- Dialog definitions are config-driven like all other mission content; strings via `Language/localization_Overthrow.st`.
- Gamepad/console usable.

## Dependencies

- `missions/framework` (module + branching seams); UI patterns from `missions/mission-ui`.
- Can be built in parallel with `missions/recruit-missions` and `missions/authoring-tools`.

## Out of Scope

- Voice acting, audio, camera work, or any cinematic presentation.
- Persistent NPC conversation state outside a mission instance.
- Dialog for non-mission NPCs (shopkeepers etc.).
