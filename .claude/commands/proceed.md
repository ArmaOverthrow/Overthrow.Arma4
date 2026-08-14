---
description: "Proceed work on the current feature. Usage: /proceed"
---

You have been asked to continue work on the current feature. You should know which feature this is and this command should be run after /start-feature, /continue-feature, or /review-feature.

If the current feature is unclear, ask the user to run one of these commands first.

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and resolve the current feature's nested `<epic>/<feature>` path when it lives inside an epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When the current feature lives inside an epic, every `docs/features/<feature-name>/...` path below resolves to its nested `docs/features/<epic>/<feature>/...` location. This command's body is the proceed workflow; it does **not** re-specify those rules.

## Determine Work Source

By default, `/proceed` uses the **implementation workflow** from `tasks.md`.

The **code review workflow** is only used when `/review-feature` was run earlier in this conversation and the `code-review.md` content is already loaded into context. In that case, proceed with code review fixes.

If unsure which workflow is active, check the conversation history:
- Was `/review-feature` the most recent feature command? → **Code review workflow**
- Was `/continue-feature` or `/start-feature` the most recent? → **Implementation workflow**

---

## Implementation Workflow

Use `docs/features/<feature-name>/tasks.md` and `docs/features/<feature-name>/context.md` to determine what the next steps are and begin implementation.

You will only use agents to implement the next steps, keeping your own context clear for orchestration. You will not use them in parallel, but instead get them to implement one phase at a time, after each phase you will determine if the user needs to be prompted to test the completed work before moving on if it has a frontend UI, otherwise you may spawn an agent and continue.

### Agent Selection: Standard vs. Advanced

Beast Mode creates two tiers of dev agent for each domain:

- **Standard** (e.g. `frontend-dev`, `backend-dev`) — `model: opus`, `medium` effort. The default for routine implementation work, because the architecture and decisions already live in the plan (written by the high-effort solution-architect).
- **Advanced** (e.g. `frontend-dev-advanced`, `backend-dev-advanced`) — `model: opus`, maximum effort. For phases that are genuinely hard to get right.

Before spawning an agent for a phase, **assess whether it warrants the advanced agent.** Signals that it does:

- It's a **major refactor** or rewrite, not additive work
- It **touches many integration points** / crosses module or service boundaries
- It involves **tricky concurrency, data migration, or state management**
- The plan itself flags it as high-risk, or the solution-architect marked it as needing an advanced agent
- Getting it wrong would be **expensive to unwind** (schema changes, public APIs, auth)

If the phase looks routine, just use the standard agent and proceed.

**If you judge that the advanced agent is warranted, do NOT silently use it — ASK the user first.** Use `AskUserQuestion` (or the equivalent), naming the specific reasons:

```
This next phase (<phase name>) looks like a good candidate for an advanced agent
(opus, max effort) rather than the standard one, because:
  • <reason 1, e.g. it refactors the auth layer touched by 6 modules>
  • <reason 2, e.g. a mistake here breaks existing sessions>

Advanced agents use more tokens but I recommend it here. Options:
  • Use the advanced agent for this phase  (recommended)
  • Use the standard agent
```

Respect the user's choice. If they decline, use the standard agent. If they'd rather drive it phase-by-phase at the advanced tier, point them at `/proceed-advanced` (forces advanced agents, one phase at a time).

### The post-phase test gate (orchestrator only)

After the agent reports a phase complete, **you** run the regression gate in the main thread — never the
subagent, and never mid-phase. Read `.claude/test-policy.md`; it is the single source of truth. The short
version:

- Every implementation-agent prompt must carry, verbatim: *"Do not run `tools/run-tests.sh`. Your gate is
  `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes."*
- `tools/compile-check.sh` is headless and free — run it as often as you like.
- `tools/run-tests.sh` **launches a real Reforger client that steals the user's desktop focus for ~15–20 s**.
  One run per completed phase, announced before it launches. **Fast group** (`{6A6E29FF47ECB840}`) by default;
  **All** (`{6A6E2A002F53A581}`) only if the phase touched campaign, economy or persistence state.
- **Skip the run entirely** — and say you skipped it, in one line — when the phase was docs, `.layout`,
  prefab, `.conf` or localization only. The suites assert nothing about those, and UI is uncovered outright.
- **Defer it** if the user may be in-game: a `launch-server.sh`/`launch-game.sh` session from this
  conversation still running, or the user said they were play-testing. Report the gate as pending.
- **On red:** the suites are deterministic (`maxAttempts` is banned), so never re-run hoping it passes. Read
  the case names and `.tmp/run-tests/` artifacts, then fix it yourself here or re-engage the agent with the
  failing case names — **the "always use agents" rule below is about implementing phases, not about repairing
  a gate.** Re-run once. Still red → stop and report. Exit 2 is *no verdict*, never a pass.

Important Notes:
**DO NOT** Implement the tasks yourself, always use agents
**DO NOT** Spawn parallel agents, use one agent at a time
**DO NOT** Ask agents to implement more than one phase at a time
**DO** Choose the best agent for the current task/phase
**DO NOT** Let a subagent run `tools/run-tests.sh` — the gate is yours, once, after the phase
**DO** Run the post-phase test gate yourself per `.claude/test-policy.md` (or state that you skipped/deferred it, and why)
**DO** Determine if frontend testing and verification is required by the user before moving on to the next phase, if not, you may continue by spawning another agent
**DO** Update `tasks.md` and `context.md` between each phase if the agent has not done it already

---

## Code Review Workflow

This workflow is only active when `/review-feature` was run earlier in this conversation and the `code-review.md` is loaded into context.

When proceeding with code review fixes from `code-review.md`:

### 1. Determine Next Fix

- Read `docs/features/<feature-name>/code-review.md`
- Find the first unchecked item in the **Progress** section (`- [ ] CR-N`)
- Cross-reference with the **Recommended Fix Order** table to confirm priority
- Read the full finding details (severity, file, problem, fix)

### 2. Prepare Agent Context

Build a detailed prompt for the agent including:
- The specific CR finding number and title
- The exact file(s) and line numbers to modify
- The **Problem** section (what's wrong)
- The **Fix** section (what to do)
- The Vercel rule being addressed
- Any related findings that should be considered together (e.g., CR-5 and CR-9 both affect BoxDimensionField)

**Group related fixes:** If the next 2-3 fixes all affect the same file, combine them into one agent task. For example:
- CR-10 + CR-11 (both quick cleanups in the same files)
- CR-5 + CR-9 (both improve BoxDimensionField)

### 3. Spawn Agent

Use a `frontend-dev` agent (or `general-purpose` if the fix is more architectural). For **CRITICAL/HIGH findings that require redesign or cross many integration points**, apply the same Standard-vs-Advanced judgment as the implementation workflow — ask the user before using the `*-advanced` (opus, max-effort) agent. Provide:
- Full context of what to change and why
- The exact code snippets from the review showing current vs. desired
- Instructions to run `npm run type-check` after changes
- Instructions NOT to change anything beyond the scope of the finding

### 4. Verify and Update

After the agent completes:
- Verify the changes make sense
- Run type-check if the agent didn't
- Mark the finding as resolved in `code-review.md`: change `- [ ] CR-N` to `- [x] CR-N`
- Update the **Status** line at the top: increment resolved count
- Determine if the user should test before continuing:
  - **CRITICAL/HIGH** fixes: Suggest testing
  - **MEDIUM/LOW** fixes: May continue to next fix

### 5. Report and Continue

After each fix:
```
Fixed CR-[N]: [title]
Progress: [X]/[Y] findings resolved

Next: CR-[M]: [title] ([severity], [effort])
[Brief description]

Continue with /proceed or tell me which finding to tackle next.
```

If the user hasn't intervened, proceed to the next fix automatically for LOW/MEDIUM severity items.

---

## Important Notes (Both Workflows)

**DO NOT** Implement the tasks yourself, always use agents
**DO NOT** Spawn parallel agents, use one agent at a time
**DO NOT** Ask agents to implement more than one phase/finding at a time (unless grouping related fixes in the same file)
**DO** Choose the best agent for the current task — standard (opus, medium effort) by default, advanced (opus, max effort) for major/integration-heavy/high-risk phases
**DO** Ask the user before switching to an advanced agent, naming the specific reasons and noting the higher token cost
**DO** Determine if frontend testing and verification is required
**DO** Keep `tools/run-tests.sh` to the orchestrator, once per completed phase — never in planning, never in a subagent (`.claude/test-policy.md`)
**DO** Update documentation between each step
**DO** Group related code review fixes that touch the same file
**DO** Point the user at `/proceed-advanced` if they want to force advanced agents one phase at a time
