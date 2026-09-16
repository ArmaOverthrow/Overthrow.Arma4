---
description: "Prepare a PR branch for review: check it out, merge main into it (resolve conflicts), run the compile check and the Fast test group, and give a ready / not ready verdict so /review-pr can start. Usage: /prepare-pr <PR-number>"
---

You have been asked to **prepare a pull request branch for review**. The goal is a PR branch that is checked out, current with `main`, merge-clean, and proven to compile and pass the Fast test group. Then `/review-pr <N>` can run against a known-good branch.

**PR number:** `$ARGUMENTS`

This command is the **setup step before `/review-pr`**. It does not review the PR's logic and does not post to GitHub. It does make the branch review-ready: it **resolves merge conflicts with `main`** and fixes breakage that the merge causes. It ends with a clear "READY / NOT READY" verdict.

## What you must know about this project

- **The user's Workbench runs the live working tree.** A checkout or a merge changes the scripts that an open Workbench compiles and play-tests. Workbench does not always reload scripts after an external edit. After a checkout or a merge, tell the user to run `Build > Compile and Reload Scripts` before a play-test.
- **`tools/compile-check.sh` is the compile gate.** It runs the Workbench headlessly and exits 0 only on a proven clean compile. Exit 2 means "could not determine", never pass and never fail. A warm run takes about 5 s.
- **`tools/run-tests.sh` is the test gate.** It launches the real game client. A game window appears on the user's desktop for about 20 s. Use the Fast group by default. Exit 2 is indeterminate, not a failure. Suites are not deterministic under machine load. An open Workbench GUI can cause a false exit 2.
- **Test group targets are GUIDs.** Quote them, the braces are part of the argument.
  - Fast: `"{6A6E29FF47ECB840}"` (Init + Logic suites, about 30 s wall time)
  - All: `"{6A6E2A002F53A581}"` (all suites, 2 to 3 min)
- **Localization has one source file.** `Language/localization_Overthrow.st` is the master. The sibling `Language/localization_Overthrow.<lang>.conf` exports are Workbench-generated build output. Never write to a `.conf` export. Fix the `.st` and ask the user for a re-export.
- **This tree can share the automation profile with sibling checkouts.** A compile verdict can belong to another checkout. Always confirm the log with `grep -n "CLI Params" .tmp/compile-check/last.log` and check that the path names this tree.
- **Discord:** if the `beast-mode-discord` MCP is connected, report progress with `beast_progress`, ask with `beast_ask`, and send the final verdict with `beast_reply`. Otherwise ask and report in the chat.

## Operating rules

- Work the steps **in order**. The goal is to get the branch review-ready: resolve merge conflicts and fix merge-induced breakage. Do not stop at a diagnosis. Fall back to a **STOP gate** only when a problem is beyond safe automated resolution. A STOP gate reports what is wrong and what the user must do, then ends the command. Never continue past a STOP in silence.
- This command **does not push, tag, or post to GitHub** on its own. The local writes are: the checkout, the merge of `main` (with conflict resolution), and a fix commit for merge-induced breakage. One explicit question gates the push. Never push without a yes.
- Never force-push. Never run a git operation that discards committed work (`reset --hard` over existing commits, `push --force`, branch deletion). These are fine: completing a merge, `git add` on resolved files, and `git reset --hard ORIG_HEAD` to undo an in-progress merge that you made in this run.
- **Resolve a conflict by intent.** Read what each side changed and why. Keep the PR's feature intent and main's changes. Then prove the result with the compile check and the Fast group. Never pick a side blind. Escalate (STOP) only when the two sides are semantically incompatible or when validation cannot pass after a real attempt.
- **Never run the suites while another agent writes to `Configs/Language` or `Language/`.**

---

## Step 1 - Preflight: the working tree must be clean

```bash
git status --porcelain
```

- Empty: clean, continue.
- Non-empty: the tree is dirty. **Do not stash or discard in silence.** The dirty files can be the user's in-flight work, and Workbench runs this tree. Show the changes and ask:

```
Your working tree has uncommitted changes:
[paste `git status --short`]

I need a clean tree before I switch to the PR branch (Workbench runs the live
working tree). How do you want to proceed?
  1. Commit them yourself, then re-run /prepare-pr <N>
  2. Stash them (you pop them manually later)
  3. Cancel
```

Do not continue until the tree is clean.

## Step 2 - Fetch the PR metadata

```bash
gh pr view <N> --json number,title,headRefName,baseRefName,state,isDraft,mergeable,author,url
```

Record `headRefName` (the PR branch), `baseRefName` (expected `main`), `state`, `mergeable`.

- `gh` errors or the PR does not exist: report it and STOP.
- `state` is `MERGED` or `CLOSED`: tell the user and ask if they still want to prepare it. Stop if they decline.
- `baseRefName` is not `main`: flag it. PRs target `main` by convention. A different base often comes from a branch made off another feature branch. Note it in the final report. Do not retarget the PR, that is the user's call.

## Step 3 - Map the PR to its feature docs

Read `.claude/epic-resolution.md` and apply its rules. Find the feature under `docs/features/` whose `context.md` lists the changed files. Descend into epic folders. Record the feature path (for example `docs/features/<epic>/<feature>/`). `/review-pr` writes its deferred findings there. If no feature matches, record "no feature docs" and continue.

```bash
gh pr view <N> --json files --jq '.files[].path'
```

## Step 4 - Check out the PR branch

```bash
git fetch origin
git checkout <headRefName>
git pull --ff-only origin <headRefName>
```

- Checkout fails (a diverged local branch, for example): report the git error and STOP. Do not force it.
- Tell the user: an open Workbench now compiles this branch. They must run `Build > Compile and Reload Scripts` before a play-test.

## Step 5 - Merge `main` into the branch and resolve conflicts

A merge of `main` into the PR branch updates the branch and proves that it merges back into `main`.

```bash
git fetch origin main
git merge-base --is-ancestor origin/main HEAD && echo "up-to-date" || echo "behind"
```

- **`up-to-date`**: the branch contains current `main`. Note it and continue to Step 6.
- **`behind`**: merge `main` in:

  ```bash
  git merge --no-edit origin/main
  ```

  - **Clean merge**: a local merge commit exists. Continue to **"Push the merge?"** below.
  - **Conflicts**: resolve them. Do not hand the PR back unresolved. Work every conflicted file:

    1. List them: `git diff --name-only --diff-filter=U`.
    2. For each file, read both sides before you edit. `ours` (HEAD) is the PR's work. `theirs` (`origin/main`) is the base it must integrate.

       ```bash
       git log --oneline HEAD..origin/main -- <file>   # what main changed
       git log --oneline origin/main..HEAD -- <file>   # what the PR changed
       git diff origin/main...HEAD -- <file>           # the PR's own diff
       ```

    3. Apply the file-type rules below. Integrate both sides and delete the conflict markers. Never use a blind `--ours` or `--theirs` unless the two sides are the same change or a rule below says so.
    4. Complete the merge: `git add <resolved files> && git commit --no-edit`. Stage the resolved files by path, never with `git add -A`.

    **File-type rules for this project:**

    | File | Rule |
    |---|---|
    | `Scripts/**/*.c` | Integrate by intent. For a large or coupled conflict (both sides rewrote the same class), delegate to `component-developer`, `network-specialist`, or `ui-developer` with both diffs. Watch for the recurring shape: `main` adds a helper that the branch deleted or moved. Port the logic to the branch's structure, do not restore the deleted helper. |
    | `Language/localization_Overthrow.st` | Structured data. Never resolve by text hunk. Merge entry by entry, keyed by the entry GUID: same on both sides = keep, one side equals base = take the other, both differ = ask the user. Then verify: entry count is the union, no duplicate GUIDs, brace balance is 0, no fabricated values. An unbalanced brace is data loss on the next Workbench save. |
    | `Language/localization_Overthrow.*.conf` and `.conf.meta` | Build output. `git checkout --ours -- <file>` and continue. Tell the user in the report that a re-export is needed. |
    | `CHANGES.md` | Keep both sides' entries. Version numbers collide on a long-lived branch. Put the PR's entries under the correct version heading and do not drop main's entries. |
    | `*.et`, `*.conf`, `*.layout` (Prefabs, Configs, UI) | Structured Enfusion files. Integrate both sides' properties and keep every component block from each side. A same-GUID Overthrow prefab is a delta over vanilla. Never re-mint a GUID to resolve a conflict. |
    | `*.meta` | Keep the existing GUID. If both sides added a `.meta` for the same path with different GUIDs, ask the user which one the branch shipped with. |
    | delete/modify conflicts | If `main` modified a file that the PR deleted or moved, port main's change to the new location, then confirm the deletion. |

    **Validate the resolution before you trust it.** Steps 6 and 7 are the proof. If the compile check or the Fast group fails in a merged region, fix the resolution and validate again. Loop until clean, then continue.

    **Escalate (STOP gate) only when resolution is unsafe:** the two sides made semantically incompatible changes that need an owner decision, or the compile check still fails in the conflict region after a real attempt. Then undo the half-done merge (`git merge --abort`, or `git reset --hard ORIG_HEAD` if you committed it), report which files and why, and hand it back. Do not escalate because the merge is tedious.

**Push the merge?** After a clean or resolved merge you hold a **local** merge commit. Ask the user if you should push it to the PR branch so that the GitHub PR shows the merged state. On yes: `git push origin <headRefName>`. On no: leave it local. Take extra care if the PR is not the user's own. Ask, never push unprompted. Continue in both cases. **Remember the answer.** A fix commit from Step 6 or 7 is pushed under this same consent, with no second question.

## Step 6 - Compile gate

```bash
tools/compile-check.sh --keep-log
grep -n "CLI Params" .tmp/compile-check/last.log
```

- The `CLI Params` line must name this checkout's `addon.gproj`. If it names a sibling checkout, run the check again. Never accept a verdict from another tree.
- **Exit 0**: record the file count and continue.
- **Exit 1**: errors are on stdout, one per line. An error in a merged region is your resolution to fix. Fix it, commit it as `(fix) merge: <one line>`, and run the check again. An error in the PR's own code, outside any merged region, is a readiness problem. Record it for the report. Do not fix the PR author's code.
- **Exit 2 or 124**: indeterminate. Read the stderr summary line. If a Workbench GUI is open, the check is still valid but the profile can be shared. Run it one more time. If it stays indeterminate, record the reason and continue to Step 7. Say so in the report.

A syntax error stops the engine at the first bad file. A clean run after one fix can reveal more errors. Loop until exit 0.

## Step 7 - Test gate: the Fast group

Tell the user that a game window opens for about 20 s. Then run:

```bash
tools/run-tests.sh "{6A6E29FF47ECB840}"
```

- **Exit 0**: read the `run-tests: OK (<N> tests, <D>s)` line from stderr. Record N.
- **Exit 1**: failing case names are on stdout. Open `.tmp/run-tests/autotest.log` for the messages. Sort each failure:
  - A failure in a merged region is yours. Fix the resolution, commit as `(fix) merge: <one line>`, and run again.
  - A `timeout` on an Init case with `Output: <none>` is machine load, not a defect. Run the group one more time.
  - A failure in the PR's own code is a readiness problem. Record it. Do not fix the author's code.
- **Exit 2**: indeterminate. If a Workbench GUI is open, tell the user and ask them to close it or accept an unproven test gate. Run one more time after they answer. Record the outcome.
- **Exit 124**: the client hung. Record it and continue. `Setup_AwaitWorld` has no timeout of its own.

**Run the All group as well** when the PR touches any of these: `Scripts/Game/Persistence/`, `Scripts/Game/GameMode/`, a save serializer, a `Deserialize` method, or more than 20 script files.

```bash
tools/run-tests.sh "{6A6E2A002F53A581}"
```

Apply the same triage. The All group takes 2 to 3 min.

## Step 8 - Static checks for the touched areas

Run only the checks that match the PR's changed files. Skip the rest and say so.

| Touched path | Check |
|---|---|
| `Configs/System/ShopConfig.conf`, `Configs/System/GunDealerConfig.conf`, `Configs/Pricing/` | `python3 tools/check-shop-coverage.py --mode all` (the summary mode misses a single-shop regression) |
| `Configs/Resistance/placeables.conf`, `Configs/Resistance/buildables.conf`, a placeable or buildable prefab | `python3 tools/check-placeables.py` |
| `Language/localization_Overthrow.st` | `grep -oE 'CustomStringTableItem "\{[0-9A-F]+\}"' Language/localization_Overthrow.st \| sort \| uniq -d` must print nothing |
| Any new `*.meta` | `grep -rhoE 'Name "\{[0-9A-F]{16}\}' --include='*.meta' Prefabs Configs UI Language Worlds \| sort \| uniq -d` must print nothing |

A finding here is a readiness problem for the report, not a fix for you to make.

## Step 9 - Readiness report

Produce a short report. End with `beast_reply` when on Discord.

```
## PR #<N> prepared - `<headRefName>`

<title> - <author> - <url>
Feature docs: docs/features/<...>/ (or: none matched)

| Check | Result |
| --- | --- |
| Working tree clean / branch checked out | OK |
| Up to date with main | OK already / OK merged main in / OK merged + resolved conflicts / FAIL unresolvable |
| Merge pushed to PR branch | yes / no (local only) / n/a |
| Compile check (tools/compile-check.sh) | OK <N> files / FAIL <N> errors / INDETERMINATE <reason> |
| Fast group (Init + Logic) | OK <N> tests / FAIL <names> / INDETERMINATE <reason> |
| All group | OK <N> tests / FAIL / not run (reason) |
| Static checks | OK / <finding> / not applicable |
| Localization re-export needed | yes / no |

<Base branch note if baseRefName is not main>
<If you resolved conflicts: each file, and one line on how you integrated the two
 sides. Full resolution: `git diff ORIG_HEAD HEAD`>
<Each compile error or test failure in the PR's own code>

Verdict: READY for `/review-pr <N>`   (or: NOT READY - <what blocks>)
```

- Every gate passed: **READY**. The branch is current, merge-clean, compiles, and passes the Fast group. If you resolved conflicts, point the user at the resolution. Then tell them they can run `/review-pr <N>`.
- A STOP gate fired, or the compile check fails in the PR's own code: **NOT READY**. State what the user must resolve, then tell them to re-run `/prepare-pr <N>`.
- A test failure in the PR's own code with a clean compile: **READY WITH FINDINGS**. `/review-pr` can start, and the failing cases go in its report.

## Error handling

- `gh` is not installed or not authenticated: tell the user to run `gh auth login` and STOP.
- PR branch checkout fails: report the git error and STOP. Never force it.
- Merge conflict: resolve by intent and validate with Steps 6 and 7. `git merge --abort` and STOP only when resolution is unsafe.
- A `.conf` localization export conflicts: `git checkout --ours`, and flag the re-export. Never write the file.
- Compile check exit 2 twice: continue, but the report says the compile gate is unproven.
- Test group exit 2 with an open Workbench: ask the user to close it, then run again.
- An Init case times out with no output: machine load. Run one more time before you record it.
- The compile verdict names a sibling checkout in `CLI Params`: run the check again. Never accept it.
