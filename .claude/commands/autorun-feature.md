---
description: "Autonomously run a feature to completion: plan (if needed), start, then per phase implement, verify (browser MCP for UI), end with a cross-phase review, /update-feature, and /update-master. Epic-aware;Usage: /autorun-feature <feature-name | epic/feature>"
---

You have been asked to run a feature to completion autonomously. This command drives the full lifecycle — plan (if needed) → start → phase-by-phase build with review → docs — end to end, with as little human intervention as possible, stopping only when genuinely blocked.

**Feature name:** first argument from `$ARGUMENTS` (kebab-case). May be a plain feature, or a feature inside an epic referenced as `<epic>/<feature>`.

- `/autorun-feature <feature-name>` — run that feature.
- `/autorun-feature` (no arg) — use the current in-progress feature from this conversation. If unclear, list `docs/features/*/` directories whose `tasks.md` is not 100% complete and ask which one.

---

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution — this command does **not** re-specify those rules. Two consequences for everything below:

- **Path rewriting.** When the target feature lives inside an epic, **every `docs/features/<feature-name>/...` path in this command resolves to its nested `docs/features/<epic>/<feature>/...` location** — the precondition checks, `requirements.md`, `implementation.md`/`context.md`/`tasks.md`, `code-review.md`, and all verification reads/writes use the nested path. When you fuzzy-resolve a bare name into an epic, **announce** the resolved `<epic>/<feature>` before proceeding.
- **A bare epic name is not a single buildable feature.** If the argument is a folder that has `epic-overview.md`, list the epic's child features and ask — in a **single** batched `AskUserQuestion` — which child to run, offering the option "run the whole epic in build order" as well. This is the one allowed scope-clarification stop. After the answer, proceed autonomously: a single child → run `<epic>/<feature>`; whole-epic → run each child feature in the epic's documented build order, one feature at a time, executing the full per-feature loop for each. `/update-master` still rolls the epic up to a **single** row (never lists its child features individually).

---

## CRITICAL SAFETY RULES (read first, never violate)

These override any momentum to "just finish":

1. **Never rebuild, restart, or touch any shared/production environment.** No restarting services, no building or deploying to a server. Local type-check / lint / test / build for verification only.
2. **NEVER touch git state. No branching, no committing, ever.** Do not run `git checkout -b`, `git switch -c`, `git branch`, `git add`, `git commit`, `git stash`, `git merge`, `git rebase`, `git pull`, `git push`, `git reset`, or `git restore` — not at the start, not between phases, not at the end, not "just to keep things clean". **The user owns all git operations entirely.** They will branch, stage, and commit on their own, or explicitly ask you to do it in a separate instruction. An explicit request in a *previous* run does not carry over — this command never initiates git changes on its own.
   - Read-only git inspection (`git status`, `git diff`, `git log`) is fine when you need to see what changed.
   - Work happens on whatever branch is currently checked out. Do not check, question, or "fix" it. Do not require a clean tree.
   - If you believe a commit or branch would help, **say so in the final summary as a suggestion** — do not perform it.
3. **Stop and prompt the user** the moment you hit a true blocker (see "When to stop and prompt"). Do not guess past irreversible or ambiguous situations.

---

## Operating mode

This command runs in **autonomous mode**:

- **Do not stop between phases to wait for the user to test.** Verify each phase yourself, then move on.
- **Do not pause for confirmation between routine steps.** Make the reasonable call and continue.
- **Reasonable defaults beat questions.** If a small choice comes up mid-implementation, pick the option most consistent with the plan and existing patterns, and continue.
- **Advanced-agent selection is never a stop reason.** When the plan marks a phase ADVANCED (or a task is load-bearing), use the advanced tier and keep going — do **not** ask. This is the key difference from `/proceed`, which stops to confirm before going advanced; the user opted into the full autonomous run, advanced token spend included.
- **Only stop if something genuinely blocks you** — see "When to stop and prompt" below.

---

## Orchestration model (same discipline as /proceed)

- **DO NOT implement phases yourself.** Always delegate implementation to agents, to keep your own context clear for orchestration.
- **One agent at a time.** No parallel implementation agents within a phase (a phase is a unit). You may spawn short read-only agents for lookups.
- **Choose the best agent for the work** — match it to your project's available agents (e.g. a backend agent for server work, a frontend agent for UI, a testing/integration agent for E2E, or a general-purpose agent for mixed/architectural work). Beast Mode projects expose two tiers per domain: a **standard** agent (`opus`, medium effort) and an **advanced** `*-advanced` variant (`opus`, maximum effort).
- **Follow the plan's tier.** `implementation.md` marks each phase `Agent tier: ADVANCED (opus)` or `STANDARD (high)` and `tasks.md` flags advanced phases in the header. Load-bearing work (sync engines, auth/security, concurrency, schema/data-consistency) and any phase the plan marks ADVANCED MUST route to the matched **`*-advanced`** agent — automatically, no prompt. If the project has **no** `*-advanced` variant for that domain, fall back to the standard agent, note the fallback in `context.md`, continue, and mention `/upgrade-beast-mode` (which creates the advanced agents) in the final report. A missing advanced agent is **not** a blocker.
- **Update `tasks.md` and `context.md` between every phase** if the agent did not.

---

## Process

### 1. Verify preconditions (HARD STOP if any fail)

- **Feature folder exists.** `docs/features/<feature-name>/` must exist. If not, stop and tell the user exactly what's missing and where it should live.
- **A starting point exists.** The feature needs **either** `requirements.md` (this command will plan from it) **or** an existing `implementation.md` (already planned). If neither exists, stop and tell the user to write `requirements.md` (or run `/plan-feature <feature-name>`) first — do **not** invent requirements yourself.
- **No git precondition.** Build on whatever branch is currently checked out, with whatever is in the working tree. Do **not** create a branch, switch branches, require a clean tree, or sync with remote — the user manages all of that themselves.

Browser MCP is **not** a hard precondition — it's checked lazily, only when a phase actually has UI to verify (step 5c). Backend-only / library / CLI features never need it.

If preconditions pass, briefly confirm what you found and continue. Do not ask permission to proceed — the user invoked the command, that *is* the permission.

### 2. Read the plan (or requirements)

Read the feature's docs end-to-end to load the phase structure and decisions:

- If `implementation.md`, `tasks.md`, and `context.md` exist, read all three.
- Otherwise read `requirements.md`.
- Read `CLAUDE.md` for project conventions (dev URL, test credentials, tech stack, browser/MCP rules).
- Glance at `docs/features/` siblings to learn the project's feature-doc conventions.
- **If the feature lives inside an epic** (resolved above), load the epic context per `.claude/epic-resolution.md` §4 first — the epic's `epic-overview.md` + `epic-requirements.md` and **every sibling feature's** `implementation.md`/`context.md` — so planning and verification respect the epic's build order and don't contradict siblings.

### 3. Plan (only if no `implementation.md`)

**If `implementation.md` already exists:** skip to step 4. Trust the existing plan; do not re-plan.

**If it does NOT exist:** run the project's `/plan-feature <feature-name>` skill, using the requirements you just read as the initial prompt. First, identify **gaps or ambiguities** in `requirements.md` that materially affect scope (missing roles, undefined success metrics, unclear acceptance criteria, ambiguous integrations). If there are real gaps, batch them into a **single** `AskUserQuestion` with concrete options derived from the codebase — **do not ask trivia**; if the requirements are clear enough, skip the question and plan directly. When `/plan-feature` produces a plan, **do not** stop at its "wait for approval" step — treat the plan as approved and continue (this command's contract is to build the whole feature in one go).

### 4. Start the feature (only if no `tasks.md`)

If `context.md` and `tasks.md` don't yet exist, run the project's `/start-feature <feature-name>` skill to scaffold them from `implementation.md` and flip the status to In Progress. If they already exist, treat the feature as resumed — do **not** overwrite; continue from the first incomplete phase.

### 5. The per-phase loop

Repeat for each incomplete phase in `tasks.md`, **in order**:

**5a. Implement.** Spawn the best agent at the plan's tier with a tight prompt: the phase's tasks, the exact files, the acceptance criteria from `implementation.md`, and the relevant decisions/gotchas from `context.md`. Tell the agent to run the project's type-check/build and not to exceed the phase scope. **Every implementation agent prompt MUST include the no-git rule verbatim: "Do not run any git command that writes state — no branch, checkout, add, commit, stash, merge, rebase, pull, or push. Leave all changes uncommitted in the working tree."** If an agent commits anyway, do not try to undo it — report it in the final summary and let the user decide.

**5b. Automated gate (must pass before continuing).**
- Run the project's type-check / lint / build in every package or area touched.
- Run the relevant tests if any exist for the touched code.
- For UI phases, a production build or dev-server boot with no errors.
- Check the relevant dev log for compilation errors / runtime exceptions introduced by the phase.
- If the gate fails, send the agent back to fix it (~2–3 focused iterations). If it still fails and you cannot determine the fix, STOP and prompt with the exact error.

**5c. UI verification via browser MCP.** If the phase added or changed user-facing UI:
- Use whatever browser MCP the project has (e.g. Playwright MCP, Chrome DevTools MCP, or claude-in-chrome). Bring the dev environment up per the project's documented method if it isn't already, open the relevant screen in whatever environment this branch runs in, confirm it renders, read the console + network for errors, and exercise the key interaction the phase added. Derive the check from the phase's acceptance criteria in `implementation.md`.
- **Honour project-specific browser/MCP rules in `CLAUDE.md`** — e.g. reuse an existing tab/session rather than spawning new ones if the project's browser is prone to OOM, memory limits, when to close the browser, the dev URL, and the test credentials to sign in with. Open the browser only for this check and close it when done; do not leave it running between phases.
- A preview showing "no data" / 401 is usually an auth gap, not a bug — be logged in to the app in the same browser (same-origin session).
- **If the MCP verification is BLOCKED** — no browser MCP available in this session, the feature is not reachable in any running environment, login is required and you cannot complete it, or the result is ambiguous and needs a human eye — STOP and prompt: say exactly what you were verifying, what blocked you, and the smallest thing you need (e.g. "log in to the app in your browser", "deploy this branch to a preview", "confirm the layout looks right"). Resume once unblocked.
- Backend-only phases skip this step — verify via targeted API/`curl` calls or a focused integration test instead.

**5d. Update docs.** Mark the phase's tasks `[x]` in `tasks.md`, update the `Progress:` count/percent, and update `context.md` (What's Done / What's Next / a session note + any non-obvious decisions). Record any UI items that still need a human eye in a running "Needs human verification" list.

**5g. Next phase.** Repeat until all phases in `tasks.md` are complete.

### 6. After the final phase

1. **`/update-feature <feature-name>`** — refresh the feature docs with the final state (set `implementation.md` `Status:` to the project's done state, e.g. `Ready for Review`).
2. **`/update-epic`** — update the epic overview if this feature is in an epic
3. **`/update-master`** — update the project master overview / index / changelog (an epic rolls up to a single row).
4. **Final summary** to the user:
   - Phases completed
   - Total review findings fixed across phases.
   - What was UI-verified via MCP vs the "Needs human verification" list.
   - Any phase completed with caveats, and any remaining open items / tech debt logged. Surface anything you couldn't auto-fix as a clearly labelled "⚠️ Known issues" list rather than silently shipping.
   - **Next steps for the user:** eyeball the "Needs human verification" items, then handle git however you like — the changes are left **uncommitted in the working tree** on the current branch. This command never branches, stages, or commits; that is entirely yours.

---

## When to stop and prompt the user

Stop the autonomous loop and ask, rather than guessing, when:

- **A precondition fails** (no feature folder; neither `requirements.md` nor `implementation.md`).
- **Planning needs user input** — material gaps in `requirements.md` that change scope (step 3). Ask once, in a single batched `AskUserQuestion`, then proceed.
- **A bare epic name** needs a child/whole-epic choice (the one allowed scope stop — see Epic awareness).
- **Browser MCP verification is blocked** (unavailable, unreachable, login needed, or an ambiguous result needing a human eye) — per step 5c.
- **The automated gate keeps failing** after ~2–3 focused fix attempts on the same root cause and you cannot determine the cause.
- A phase needs a **credential, external account, or manual prerequisite** that is not in place (mock or defer it, and flag it).
- An action would be **irreversible or outward-facing** (any git write, tag, deploy, external API write to a real account, or deleting/overwriting something you did not create).
- **A step seems to require a git write** (branch, stage, commit, merge, pull, push). Never do it — finish everything else, leave the changes in the working tree, and note it in the final summary. This is a "report it", not a "stop and wait", situation.
- **The plan is fundamentally wrong** for the requirements — revisit planning rather than power through.
- A depended-on skill is **missing** (`/plan-feature`, `/start-feature`, `/update-feature`, `/update-master`) — tell the user this command depends on it.

When you stop, state precisely: what you were doing, what blocked you, and the smallest thing you need from the user to continue. Then wait.

---

## Notes

- Keep the orchestrator context lean: delegate, summarise agent results, do not paste whole files back.
- **Orchestrator, not coder** — you run the loop and keep context clean; agents write the feature, one at a time.
- Honour all standing project rules in `CLAUDE.md` (no hardcoded config values, migration checks, code style — mirror existing patterns). Use the project's own skills (`/plan-feature`, `/start-feature`, …) — don't reimplement them inline. **If a sub-skill's instructions tell you to branch or commit, skip that step** — the no-git rule in "CRITICAL SAFETY RULES" overrides any invoked skill.
- **Honor `requirements.md`/`implementation.md` as the source of truth.** If you find yourself drifting outside scope, stop and check with the user before expanding.
- This command is resumable: if interrupted, re-invoking `/autorun-feature <feature-name>` picks up from the first incomplete phase in `tasks.md`.
