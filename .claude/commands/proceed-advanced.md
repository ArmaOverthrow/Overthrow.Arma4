---
description: "Proceed on the current feature using ADVANCED (opus, max-effort) agents, one phase only. Usage: /proceed-advanced"
---

You have been asked to continue work on the current feature using the **advanced agents** — for a **single phase only**. This is the heavyweight sibling of `/proceed`: use it when the next phase is a major refactor, touches many integration points, or is otherwise high-risk and benefits from opus running at maximum effort.

This command should be run after `/start-feature`, `/continue-feature`, or `/review-feature`. If the current feature is unclear, ask the user to run one of these first.

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and resolve the current feature's nested `<epic>/<feature>` path when it lives inside an epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When the current feature lives inside an epic, every `docs/features/<feature-name>/...` path below resolves to its nested `docs/features/<epic>/<feature>/...` location. This command's body is the advanced proceed workflow; it does **not** re-specify those rules.

## How this differs from `/proceed`

| | `/proceed` | `/proceed-advanced` |
|---|---|---|
| Agent | Picks the best fit; asks before going advanced | **Always** the `*-advanced` (opus, max-effort) agent |
| Scope | May continue phase after phase | **Exactly one phase, then stops** |
| Use when | Routine implementation | Major refactor / integration-heavy / high-risk phase |

The single-phase stop is deliberate: after each advanced phase, **you hand control back to the user** so they can decide whether the *next* phase also warrants an advanced agent (`/proceed-advanced`) or can drop back to a standard agent (`/proceed`). This keeps token spend under the user's control.

## Determine Work Source

By default, `/proceed-advanced` uses the **implementation workflow** from `tasks.md`.

The **code review workflow** is used instead only when `/review-feature` was run earlier in this conversation and `code-review.md` is loaded into context.

## Implementation Workflow

1. Read `docs/features/<feature-name>/tasks.md` and `docs/features/<feature-name>/context.md` to determine the **next single phase**.
2. Select the **advanced agent** for the work. These are the `model: opus`, max-effort agents created during setup — e.g. `frontend-dev-advanced`, `backend-dev-advanced`, `ml-dev-advanced`, `dev-advanced`. Choose the one matching the phase's domain.
   - If no matching `*-advanced` agent exists, tell the user and offer to either (a) run `/upgrade-beast-mode` to create the advanced agents, or (b) fall back to the standard agent for this phase.
3. Spawn **one** advanced agent to implement **only that phase**. Give it full context (the relevant tasks, decisions from `context.md`, and the implementation plan section for this phase). Do NOT implement it yourself. **Include this line verbatim in the prompt:** *"Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes."*
4. When the agent completes:
   - Update `tasks.md` and `context.md` if the agent did not.
   - **Run the post-phase test gate yourself, in the main thread** — once, per `.claude/test-policy.md` (the single source of truth). `tools/compile-check.sh` is free; `tools/run-tests.sh` launches a real Reforger client that **steals the user's desktop focus for ~15–20 s**, so: Fast group by default, All only for campaign/economy/persistence phases, **skipped** (and said out loud) for docs/layout/prefab/localization-only phases, **deferred** if the user may be play-testing. On red, read the case names — never re-run hoping it passes — fix it here or send the agent back, then re-run once. Exit 2 is *no verdict*, not a pass.
   - If the phase has a frontend/UI surface, prompt the user to test before anything else.
5. **STOP.** Report what was completed and explicitly hand back control:

```
✅ Advanced phase complete: <phase name>
Agent used: <name>-advanced (opus, max effort)
Progress: <X>/<Y> phases done

Next phase: <next phase name>
  • Run /proceed-advanced again if this next phase is also major/integration-heavy
  • Run /proceed for a standard (opus, medium-effort) agent if it's routine
```

Do not automatically continue to the next phase.

## Code Review Workflow

When `code-review.md` is loaded into context, apply the same single-phase, advanced-agent logic to the **next code-review finding (or group of related findings in the same file)**:

1. Read `docs/features/<feature-name>/code-review.md`, find the first unchecked `- [ ] CR-N`, and read the full finding (severity, file, problem, fix).
2. Spawn the matching **advanced** dev agent (opus, max effort) with the exact file(s)/line(s), the Problem and Fix sections, and the rule being addressed. Group related fixes in the same file into one task.
3. After the agent completes: verify, run type-check if the agent didn't, mark `- [ ] CR-N` → `- [x] CR-N`, and update the Status line.
4. **STOP** and report progress, then hand control back (offer `/proceed-advanced` for the next finding or `/proceed` for routine ones).

## Important Notes

**DO NOT** implement the work yourself — always use an advanced agent
**DO NOT** spawn parallel agents — one advanced agent at a time
**DO NOT** continue past a single phase/finding-group — always stop and hand back to the user
**DO** force the `*-advanced` agent even if the phase looks routine (that is the whole point of this command)
**DO** update `tasks.md` and `context.md` between phases if the agent has not
**DO** prompt for user testing when a phase has a frontend/UI surface
**DO NOT** let the agent run `tools/run-tests.sh` — the gate is yours, once, after the phase (`.claude/test-policy.md`)
**DO** remind the user that advanced agents use more tokens, so they can choose `/proceed` for the next phase if appropriate
