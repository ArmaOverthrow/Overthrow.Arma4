---
description: "Independent review of PR changes - either an open GitHub PR or local uncommitted work about to become one. Usage: /review-pr [PR-number]"
---

You have been asked to perform an **independent review** of a pull request, either one already on GitHub or a set of local changes about to be submitted.

**PR number:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. `$ARGUMENTS` here is a PR number, not a feature — but when this review maps changed files back to feature docs (deferring findings to `pr-notes.md`, deriving a PR branch name), read `.claude/epic-resolution.md` and apply its rules: a feature affected by the diff may live **inside an epic** at `docs/features/<epic>/<feature>/`, not at the top level.
When scanning `docs/features/` to match changed paths to a feature, **descend into epic folders** (each epic's child features are features); write `pr-notes.md` into the resolved `docs/features/<epic>/<feature>/` and use `<epic>/<feature>` when deriving a `feat/<feature-name>` branch.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the PR-review workflow; it does **not** re-specify those rules.

## Guiding Principles

- This is meant to be a **fresh-context second opinion**. Approach the code with no prior assumptions — do not recycle conclusions from earlier in the conversation.
- In **GitHub-PR mode** (a PR number is given), be thorough. A human reviewer on GitHub will also read your comments, so flagging smaller issues is acceptable if they materially affect quality.
- In **pre-submit mode** (no PR number), be **light**. Filter out small/easily-deferred issues — a GitHub review will pick them up later. Focus on blockers, security, architecture, and anything that would embarrass the author in a public PR.
- Never invent findings to pad the report. If the PR is clean, say so.

---

## Step 1: Context Gate (MANDATORY, FIRST STEP)

Before doing anything else, assess the current conversation context.

- If the current session already contains **substantial prior work** (many tool calls, files read, features discussed, unrelated task history), this review will be biased and polluted.
- You cannot read context usage programmatically. **Self-assess honestly**: is this conversation effectively fresh (only this `/review-pr` invocation and maybe a greeting), or has meaningful work happened?

**If context is NOT fresh:**

Stop and respond:
```
/review-pr needs a clean slate to give an unbiased independent review.

This session already has prior work in context, which would bias the review.

Please run /clear and then run /review-pr [PR-number] again.
```
Then exit the command — do not proceed.

**If context IS fresh:** Continue.

---

## Step 2: Mode Selection

- If `$ARGUMENTS` is non-empty and looks like a PR number (digits, optionally prefixed with `#`), proceed as **GitHub-PR mode** (Step 3).
- If `$ARGUMENTS` is empty, proceed as **pre-submit mode** (Step 10).
- If `$ARGUMENTS` is set but is not a number, ask the user whether they meant a PR number or want pre-submit mode, then proceed.

---

## GitHub-PR Mode (PR number provided)

### Step 3: Preflight — Repo Must Be Clean

Run `git status --porcelain`.

- If output is empty → repo is clean, continue.
- If output is non-empty → working tree is dirty.

When dirty, **do not silently stash or discard**. Report the changes and ask the user how to proceed:

```
Your working tree has uncommitted changes:
[paste `git status --short` output]

I need a clean repo before switching to the PR branch. How would you like to proceed?
  1. Commit these changes yourself, then re-run /review-pr
  2. Let me commit them for you (I'll draft a message and ask you to approve it)
  3. Stash them (less safe — you'll need to pop them manually later)
  4. Cancel the review
```

Use `beast_ask` (on Discord) or a direct question (local) to get their choice. Do not proceed to the PR branch until the tree is clean.

### Step 4: Fetch PR Metadata

```bash
gh pr view <N> --json title,body,headRefName,baseRefName,state,additions,deletions,changedFiles,commits,author,url
```

Record: `headRefName` (the PR branch), `baseRefName` (usually `main`), `title`, `body`, `author`, `url`.

- If the PR doesn't exist or gh returns an error, report it and stop.
- If the PR is already `MERGED` or `CLOSED`, inform the user and ask whether they still want to review it.

### Step 5: Check Out the PR Branch

```bash
git fetch origin
git checkout <headRefName>
git pull --ff-only origin <headRefName>
```

If checkout fails (e.g. branch has diverged), report the error and stop.

### Step 6: Shape the Review

Run, in order:

```bash
git log --oneline <baseRefName>..HEAD
git diff --stat <baseRefName>..HEAD
git diff --name-status <baseRefName>..HEAD
```

From the file list, identify categories to focus on:
- Migrations / schema changes (high blast radius)
- New dependencies (`package.json`, `requirements.txt`, `go.mod`, etc.)
- Auth / middleware / session handling
- API surface (controllers, route handlers, public endpoints)
- File upload / external input validation
- Environment variables (scan for new `process.env.X` vs `.env.example`)
- Tests (are any added? do they actually cover the change?)

### Step 7: Deep-Dive the Diff

- Run targeted `git diff <baseRefName>..HEAD -- <path>` on each critical file.
- For **newly-added** files, use `Read` to see them in full, not just the diff.
- Grep the changed files for common footguns:
  - `req.user` / auth usage on new routes
  - `rateLimit` / `throttle` on new endpoints
  - `mimetype` / file-size checks on upload handlers
  - Any `TODO` / `FIXME` / `XXX` introduced in the diff
  - New env vars present in source but missing from `.env.example`

### Step 8: Verify the Build

From the PR branch, run the project's install + verification:

- Detect the tool: `package.json` → `npm install` + `npm run type-check` (or `tsc --noEmit`), Python → `pip install -r requirements.txt` (if safe) + whatever the project uses, etc.
- If the project has a documented verification command in `CLAUDE.md` or `README.md`, use it.
- Record: PASS / FAIL / SKIPPED (with reason) for each check. This goes into the report.

Also check for test files touching the changed code: `git diff <baseRefName>..HEAD --name-only | grep -iE 'test|spec'`.

### Step 9: Compose and Present Findings

Present a structured report to the user. **Do NOT post to GitHub yet.**

Format (Discord-friendly markdown):

```
## PR #<N> Review — `<branch>` (<N files>, +<add>/-<del>)

**Scope:** <one-paragraph summary of what the PR does>

**Build:** ✅ npm install | ✅ type-check | <other checks>
**Tests:** <added / none / existing coverage only>

### 🔴 Blockers — must-fix before merge
1. **<headline>** — `path/to/file.ts:123`
   <what's wrong, impact>
   Fix: <concrete suggestion>

### 🟠 Security
<numbered, continuing from above>

### 🟡 Architecture
...

### 🔵 Test coverage
...

### ⚪ Smaller issues
...

---

Which of these would you like to post as GitHub PR comments?
Reply with the numbers (e.g. "1, 3, 7") or "all" or "none".
The rest will be saved to `docs/features/<feature>/pr-notes.md` for follow-up.
```

Each finding must have: **headline**, `file.ext:line` reference, impact, concrete suggested fix. Number findings continuously across all sections (1, 2, 3, …) so the user can reference them by number.

Use `beast_ask` on Discord (or a plain question locally) to collect the user's selection. Wait for their answer.

### Step 10: Post Selected Comments

For each finding the user chose to post, use:

```bash
gh pr comment <N> --body "$(cat <<'EOF'
**<headline>**

`path/to/file.ext:line`

<impact>

**Suggested fix:**
<concrete suggestion>
EOF
)"
```

Collect the returned comment URLs and include them in the final reply.

### Step 11: Defer the Rest to `pr-notes.md`

For findings the user did **not** select:

1. Detect the affected feature(s). Match changed file paths to `docs/features/<name>/context.md` (the "Key Files" list) when possible — **including features nested inside epics** at `docs/features/<epic>/<feature>/context.md` (descend into any directory containing an `epic-overview.md`). Write `pr-notes.md` into the matched feature's folder — the nested `docs/features/<epic>/<feature>/` when it lives in an epic. If multiple features are affected, split findings across multiple `pr-notes.md` files.
2. If no feature can be identified, write to `docs/pr-notes.md` as a catch-all.
3. Append (don't overwrite) to `docs/features/<feature>/pr-notes.md`:

```markdown
## Items from PR #<N> review — <YYYY-MM-DD>

These items were noted during an independent review of PR #<N> but were **not** raised as PR comments. They should be addressed in follow-up work, post-merge.

1. **<headline>** — `path/to/file.ext:line`
   <impact + suggested fix>

2. ...
```

4. Ask the user whether to commit the `pr-notes.md` changes onto the PR branch (so the notes travel with the PR) or leave them uncommitted. If yes, stage and commit with a message like `docs: add pr-notes from PR #<N> review`.

### Step 12: Final Reply

Summarize:
- Comments posted (with URLs)
- Notes deferred to which `pr-notes.md` file(s)
- Commit SHA if pr-notes were committed
- PR URL for quick access

End with `beast_reply`.

---

## Pre-Submit Mode (no PR number)

The user is about to open a PR and wants an independent review of their local changes first.

### Step 13: Inspect Local State

```bash
git status --short
git branch --show-current
git log --oneline origin/main..HEAD  # or the default base
git diff --stat origin/main..HEAD
git diff --name-status origin/main..HEAD
```

- Note the current branch name.
- If there are uncommitted changes, include them in the review (they'll become part of the PR).
- If the current branch has no commits ahead of `main`, the only changes are working-tree ones; review those directly via `git diff`.

### Step 14: Review (Lighter)

Do the same deep-dive as Steps 7–8 above, but:

- **Skip** nitpicks (style, naming, minor refactor opportunities) — GitHub's review bots or the post-submit `/review-pr <N>` will catch those.
- **Focus** on: correctness, security, architecture drift, broken builds, missing tests for new behavior, things the author would want to know before strangers see them.

### Step 15: Present Findings and Offer to Fix

Present findings in the same numbered format as Step 9, but frame the prompt differently:

```
...
---

Want me to fix any of these before we open the PR?
Reply with the numbers to fix (e.g. "1, 3"), "all", or "none" to skip straight to PR creation.
```

For each finding the user wants fixed:
- Handle simple edits directly with the `Edit` tool.
- For non-trivial work, spawn the appropriate `Agent` (e.g. `frontend-dev`, `backend-dev` etc). For fixes that are **major refactors or touch many integration points**, prefer the `*-advanced` (opus, max-effort) agent. Use `solution-architect` if major re-designing is needed and append new phases to the implementation.md.
- After each fix, run the verification command again to catch regressions.
- Ask the user to verify if UI changes are involved

When all selected fixes are done, re-run the verification and report.

### Step 16: Branch Hygiene Before PR

Check the current branch name. If it's `main`, `master`, or `develop`, we must move to a PR branch.

**Branch naming rules** (enforce these):
- New feature with a corresponding Beast Mode feature doc → `feat/<feature-name>`
- Extension/fix to an existing feature → `feat/<feature-name>/<short-description>`
- Use kebab-case, no spaces, no special chars

Derive `<feature-name>` from:
1. The most-recently-updated feature directory under `docs/features/` (descend into epics — a feature may live at `docs/features/<epic>/<feature>/`), if the diff touches files listed in its `context.md`. For an epic-nested feature, name the branch `feat/<epic>/<feature>` (kebab-case throughout).
2. Otherwise, ask the user what to call it.

Procedure:

```bash
# Ensure main is up to date with origin
git fetch origin
# Check for conflicts with current main HEAD
git merge-base --is-ancestor origin/main HEAD && echo "clean" || echo "needs-rebase"
```

- If changes already exist on a non-main branch with a conformant name, stay on it.
- If changes are on `main` or a non-conformant branch:
  - `git checkout -b feat/<feature-name>[/<suffix>]` (from current HEAD, which has the work)
  - If work was on `main`, reset `main` back to `origin/main` **only after** confirming with the user and verifying the new branch contains everything.
- If `origin/main` has moved ahead since the work began:
  - Run `git rebase origin/main` on the PR branch.
  - If there are conflicts, stop and ask the user — do not auto-resolve.
  - Re-run the verification after rebase.

### Step 17: Create and Push the PR

1. Ensure all commits have clean messages. If there are uncommitted changes, draft a commit message and ask the user to approve.
2. Push: `git push -u origin <branch>`.
3. Create the PR:

```bash
gh pr create --title "<concise title>" --body "$(cat <<'EOF'
## Summary
<1–3 bullets>

## Test plan
- [ ] <concrete test step>
- [ ] ...
EOF
)"
```

4. Return the PR URL to the user.

### Step 18: Final Reply

Summarize:
- Findings found → fixed / deferred / none
- Branch name used
- Rebase performed (yes/no)
- PR URL

End with `beast_reply`.

---

## Important Notes

- **DO NOT** post to GitHub without explicit user selection. Always ask which findings to comment.
- **DO NOT** force-push, reset, or do any destructive git operation without explicit user confirmation.
- **DO NOT** run the review if context is polluted — the bias defeats the point of the command.
- **DO** use `gh` CLI for all GitHub interactions.
- **DO** always verify the build on the branch being reviewed (type-check, tests, whatever the project uses). A review without a build check is half a review.
- **DO** write deferred findings to `docs/features/<feature>/pr-notes.md` so nothing is lost.
- **DO** enforce branch naming (`feat/<feature-name>` or `feat/<feature-name>/<suffix>`) when creating PR branches.
- If the `beast-mode-discord` MCP is connected, use `beast_progress` between phases and `beast_ask` for all questions; all final output goes through `beast_reply`.

## Error Handling

- `gh` not installed or not authenticated → tell the user to run `gh auth login` and stop.
- PR branch checkout fails → report the git error and stop, don't force it.
- Build verification fails → report it as a **Blocker** finding (the PR literally doesn't build) and continue with the review.
- No `docs/features/` directory in the project → write deferred notes to `docs/pr-notes-<PR-or-date>.md` at the repo root instead.
- Rebase conflict during pre-submit mode → stop and ask the user to resolve, do not auto-resolve.
- If the user's selection in Step 9 or Step 15 is ambiguous ("the first few"), ask them to use explicit numbers.
