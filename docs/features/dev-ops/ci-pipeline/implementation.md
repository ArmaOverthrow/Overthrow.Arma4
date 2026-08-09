# CI Pipeline — Implementation Plan

**Epic:** dev-ops (feature #4 of 5)
**Status:** Planning
**Started:** 2026-08-02
**Target Completion:** TBD
**Last Updated:** 2026-08-02 04:15

---

## Quality Bar

**Target quality:** Infrastructure-grade, same bar as features #1 and #2 — but with a new failure surface. #1 and #2 could only lie to a developer standing in front of the terminal. This feature can lie to a **contributor who is not in the room**, on a machine they cannot see, and a green tick on a PR is the most trusted signal this project emits.

- **No silent false greens.** A green check must mean "the code in this PR compiled and its tests passed". Three specific ways this feature could produce a green check that means something else, each of which gets an explicit control:
  1. **The wrong tree was verified** — the toolchain reads the developer's working repo instead of the runner's checkout. Proven against, not assumed (Phase 1, task 1.6).
  2. **The exit code was swallowed** — `tools/compile-check.sh | annotate` reports the annotator's status, not the tool's. Every tool invocation captures its own `rc` explicitly; `set -e` is disabled around them deliberately.
  3. **Nothing ran** — the test target does not exist in the checkout, so zero tests "passed". Degraded runs are reported distinctly and loudly, never silently.
- **Three-valued, end to end.** The tools return 0 / 1 / 2 / 124 and mean it. Exit 2 is *not* a test failure and *not* a pass — a red check caused by exit 2 must say, in the PR UI, that the runner could not determine a verdict and this is not evidence about the code.
- **Orchestration only.** `tools/*.sh` and `.scripts/*.sh` are consumed byte-identical. If a command is wrong, it is fixed in the feature that owns it (#1/#2/#3). This feature adds one YAML file, one docs page, and nothing else executable.
- **Reproducible from the docs alone.** A second runner must be standable-up by someone who has never seen this machine, working only from `runner-setup.md`. Anything discovered by fiddling gets written down the same hour.
- **Nothing machine-specific is committed.** No absolute paths, no drive letters, no profile names that only exist here, no secrets, in any file under `.github/`.
- **Bounded and self-cleaning.** Every job is bounded three ways (tool timeout → job timeout → one-job-at-a-time runner) and starts by reaping orphans from whatever went wrong last time. A CI runner dies of accumulated zombies; this one sweeps before it works.
- **The pipeline reports; humans decide.** No auto-merge, no tagging, no pushing, no publishing. `contents: read` and nothing more.

---

## 1. Executive Summary

This feature makes the compile check and the test suite run **on their own, on a pull request, on a machine nobody is watching**, and puts the result where a contributor sees it: a check on the PR, per-test results, file/line annotations on the offending lines, and downloadable artifacts on every run including the red ones.

It is deliberately, aggressively thin. The hard problems were already solved: feature #1 owns the WSL→Windows process boundary, timeouts and verified kills; feature #2 owns the test contract and the honest `junit.xml` verdict; feature #3 owns the suites. All three published a stable contract in `tools/README.md`. **This feature writes one GitHub Actions workflow file that calls those commands and translates their exit codes into GitHub's vocabulary.** It reimplements nothing, names no `.exe`, and parses no game log.

The binding constraint is unchanged and unfixable: **Reforger has no headless rendering**, so the tests need a real GPU and a real logged-in Windows session. That rules out every hosted runner and forces a self-hosted one. The architecture the user selected is a GitHub Actions runner **installed inside WSL, registered as a Linux runner, run as a command-line process in the logged-in session** — which puts the runner on the same side of the interop boundary as the tooling it drives, so a workflow step is a plain bash line calling a script that already works.

Two things in this feature are genuinely unknown and get an empirical spike before anything is built: whether the toolchain works against a **checkout under the runner's work directory** rather than the developer's repo (path translation, addon resolution, and the resource-database cache all touch this), and how GitHub behaves at the edges we depend on (approval gating for fork PRs, token permissions on fork PRs, what a cancelled job leaves behind). Phase 1 answers both by execution and gates every later phase — the pattern #1 and #2 both used and both got value from.

---

## 2. Goals

### Primary Goals

1. **A pull request gets checked automatically.** Open or update a PR → compile check runs → if it passes, the full test suite runs → the result is a check on the PR within minutes.
2. **Compile errors land on the line that caused them.** `tools/compile-check.sh`'s gcc-style stdout becomes GitHub `::error file=...,line=...::` annotations, visible in the Files-changed tab.
3. **Per-test results on the PR.** `junit.xml` surfaced so a contributor sees *which* test failed without downloading anything.
4. **Honest verdicts survive the translation.** Compile failure, test failure, indeterminate and timeout are four visibly different outcomes in the PR UI — not four ways of saying "red".
5. **A runner that can be rebuilt.** `runner-setup.md` complete enough to stand up a second machine, including the parts that are specific to this one and why.
6. **Artifacts on every run** — `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log`, the compile log — attached even when the run is red or indeterminate.

### Secondary Goals

1. **Manual dispatch with target selection**, so the **Fast** group is a supported target (per `requirements.md`) without being wired to a trigger, and so the runner can be exercised without opening a PR.
2. **Graceful behaviour on checkouts that predate the test tree** — CI must be useful on `vanilla-persistence` and on feature branches, not only on a future `main`.
3. **Fork-PR safety documented and configured**, not left to luck.
4. **Machine hygiene** — the pre-job stale-process sweep `tools/README.md` explicitly asks this feature to run.

### Explicitly Out of Scope

Restating `requirements.md` so nothing creeps in:

- **Workshop publishing / packing** — feature #5, and deliberately never reachable from this workflow. No `-packAddon`, no `-publishAddon*`, no release job, not even a disabled one.
- **Auto-merge, tagging, or any push to any branch.** `permissions: contents: read`.
- **Cloud/hosted runners.** Ruled out by the GPU requirement.
- **Matrix builds, multiple runners, multiple workflow files.** One runner, one workflow, one job.
- **Writing or fixing tests** — feature #3. If CI goes red because a suite is flaky, the fix lands in #3.
- **Changing anything in `tools/` or `.scripts/`.** Byte-identical is a DoD criterion.
- **Branch protection / required-check configuration.** A repo-settings decision for the maintainer once the pipeline has a track record; noted in the docs, not configured here.
- **Nightly/scheduled runs.** No `schedule` trigger.

---

## 3. Deviations from `requirements.md` (user decisions, 2026-08-02)

These override the requirements document where they conflict. Recorded here so a future reader does not "fix" the plan back to the requirement.

| # | `requirements.md` says | Decision taken | Why |
|---|---|---|---|
| **D-1** | "Runs automatically **on push** and pull request"; "**Fast subset on push**, full suite where appropriate" | **PR-only. No `push` trigger at all.** Pull requests run compile check + the **All** group. Fast remains a *supported target* via `workflow_dispatch`, wired to no automatic trigger. | The runner is the user's own working machine. A push trigger means every `git push` on a work branch seizes the GPU for ~30 s and steals a Workbench slot. PRs are the decision points; that is where the gate belongs. **Note:** `on: pull_request` still fires on `synchronize` — pushing new commits to an *open PR* does re-run CI. "No push trigger" means no runs for branch pushes outside a PR. |
| **D-2** | "Self-hosted **Windows** runner" | **Runner installed inside WSL, registered as a Linux runner**, started as a command-line process in the logged-in session (not a service). | The entire toolchain is bash and already lives in WSL; a Windows runner would add a second shell and a second quoting model at exactly the boundary `tools/lib/common.sh` exists to own. The Windows binaries are still driven through WSL interop, exactly as they are interactively. A service cannot do this — the game client needs the interactive session and a GPU. Established pattern: [zenn.dev/tenpa — WSL self-hosted runner](https://zenn.dev/tenpa/articles/wsl-self-hosted-runner-cicd?locale=en). |
| **D-3** | (silent on fork policy) | **Approval-gated:** repo setting *"Require approval for all outside collaborators"*. A maintainer clicks **Approve and run** on each outside PR. | The repo is public with outside contributors and the runner is a personal machine. |
| **D-4** | (silent on runner host) | **This dev machine** is the runner. Documentation must still allow a second one to be built from the notes. | One working runner first (`requirements.md` out-of-scope list agrees). |

Everything else in `requirements.md` stands unchanged.

---

## 4. Architecture Overview

### Ground truth verified at planning time (2026-08-02)

Read from the repo and the working tree, not assumed. Two items correct the brief this plan was written against.

| Fact | Evidence | Consequence |
|---|---|---|
| **The Fast/All group configs now exist**, with the GUIDs `tools/README.md` declares the stable contract: Fast `{6A6E29FF47ECB840}` (18 cases), All `{6A6E2A002F53A581}` (30 cases). | `Configs/Tests/OVT_TestGroup_{Fast,All}.conf` + `.meta` on disk; `tools/README.md` group-target table; `docs/technical-design.md` §10 | **Corrects the planning brief**, which assumed they did not exist. The GUIDs may be hardcoded in the workflow. |
| **…but they are untracked in git** at planning time (`?? Configs/Tests/`), as is the whole tier-suite tree. | `git status` snapshot, 2026-08-02 | **CI checks out git, not the working tree.** A PR against today's `vanilla-persistence` HEAD would see `tools/` + Smoke/Meta but **no group configs** → `run-tests.sh "{GUID}"` → invalid target → exit 2. The workflow must resolve its target by *file presence*, not faith. |
| `main` is frozen and has **neither** `tools/` nor any test tree. | branch policy; commits `5c28a2b`, `f5bb869` are on `vanilla-persistence` | A PR whose head branch lacks the workflow file simply does not trigger it — sane. A head branch that has the workflow but not the tools must fail preflight with a clear message, not a confusing one. |
| `tools/config.local.sh` is **gitignored**. | `.gitignore:15` | It will **never** exist in a runner workspace. All machine-specific config must reach the tools through the **runner process's environment**. |
| `.tmp/` is gitignored; `*.rdb` is gitignored. | `.gitignore:1,12` | `actions/checkout` with default `clean: true` runs `git clean -ffdx`, which deletes **ignored** files — including `resourceDatabase.rdb`. That would make every CI run pay the one-off ~60 s project scan `tools/README.md` documents. Phase 1 measures it; Decision 6 resolves it. |
| Client exit codes are meaningless; junit failures must be counted as **elements**; an invalid `-autotest` target manifests as a missing `junit.xml` → exit 2. | `tools/README.md`; `autotest-foundation/findings.md` | The workflow never inspects a game log or a junit file itself. It reads exit codes and the tools' stderr summary lines. Nothing to re-derive. |
| Timings: compile ~4–7 s warm (~60 s first-ever), green All run ~16–19 s, `run-tests` default timeout 300 s, `compile-check` default 120 s. | `tools/README.md` | Job wall time is dominated by checkout and runner overhead, not by the game. `timeout-minutes: 20` is generous. |
| `tools/README.md` asks feature #4 to run `bash tools/lib/common.sh --sweep-stale --kill` before each job, and warns the `.scripts/` save tools default to the **user's real Workbench save** unless `--profile` is given. | `tools/README.md` — Process hygiene; Persistence acceptance gate | Both become hard workflow rules (Decision 8) and DoD criteria. |

### The shape of the thing

```
GitHub                                   This machine (Windows + GPU, user logged in)
──────                                   ────────────────────────────────────────────
PR opened / synchronized
  │
  │ (outside collaborator? → maintainer clicks "Approve and run")
  ▼
workflow `CI` queued  ──────────────────► WSL2 distro, user's login session
  concurrency: per-PR, cancel-in-progress    │
                                             ▼
                                   actions-runner (Linux, ./run.sh in a terminal)
                                     labels: self-hosted, linux, x64, overthrow-wsl
                                     --work /mnt/n/<ci-work>        ← Windows-visible!
                                             │
                                             ▼
                                   job: compile-and-test  (timeout-minutes: 20)
                                     1. checkout        (clean:false, depth 1)
                                     2. sweep orphans   tools/lib/common.sh --sweep-stale --kill
                                     3. preflight       resolve TEST_TARGET by file presence
                                     4. compile         tools/compile-check.sh   ─┐
                                     5. reset save      .scripts/reset_save.sh --profile OverthrowCI
                                     6. tests           tools/run-tests.sh $TARGET │  WSL interop
                                     7. junit report    mikepenz/action-junit-report │  ↓
                                     8. artifacts       actions/upload-artifact      │ Workbench.exe
                                                                                     │ ArmaReforger.exe
                                                                                     └─ (GPU, session)
```

The runner sits **inside** WSL, so every step is the same command the developer types. There is no new process boundary, no PowerShell layer, and no path translation the workflow has to think about — `tools/lib/common.sh` already does it, which is exactly why the work directory must be Windows-visible (`ovt_win_path` refuses to translate a Linux-filesystem path rather than emit a wrong `\\wsl.localhost\...` UNC).

### Ownership boundary (epic rule)

| Layer | Owner | This feature |
|---|---|---|
| Windows process boundary, timeouts, PID kill, log resolution, path translation | #1 `workbench-automation` | **consumed via `tools/*.sh`, never touched** |
| Test contract, `junit.xml` verdict, artifact names | #2 `autotest-foundation` | **consumed via `tools/run-tests.sh`, never touched** |
| Which suites exist, group configs, save-state preconditions | #3 `test-coverage` | **consumed by target name/GUID and by `.scripts/reset_save.sh`** |
| Trigger policy, job sequencing, exit-code → PR-UI translation, artifacts, runner operations | **#4 (this)** | built here |
| Packing / publishing | #5 `release-automation` | **must never be reachable from this workflow** |

### Verdict translation table (the core of the feature)

| Stage | Tool exit | Job outcome | What the PR shows |
|---|---|---|---|
| compile-check | **0** | continue | summary: `Compile check: OK` |
| compile-check | **1** | **fail**, tests skipped | one `::error file=<path>,line=<n>::<msg>` per error + full list in job summary + `last.log` artifact |
| compile-check | **2** | **fail**, tests skipped | `::error title=Compile check INDETERMINATE::` + summary stating this is a tooling/environment result and **not evidence about the code** |
| compile-check | **124** | **fail**, tests skipped | `::error title=Compile check TIMED OUT::` + summary |
| target resolution | group config absent | continue, tests **degraded or skipped** | `::warning title=Reduced test target::` naming what actually ran; job summary states it plainly |
| run-tests | **0** | pass | junit check with N passing cases + summary `N tests, 0 failures` |
| run-tests | **1** | **fail** | junit check listing each failing case + failing names in the summary + `autotest_failed.log` artifact |
| run-tests | **2** | **fail** | `::error title=Tests INDETERMINATE::` + the tool's own reason line + artifacts (`crash.log` if the target was invalid) |
| run-tests | **124** | **fail** | `::error title=Tests TIMED OUT::` + summary noting that **no test artifacts exist by contract** on a timeout |
| runner offline | — | check stays **Queued** | documented in README + `runner-setup.md`; no machinery (Decision 10) |

### File structure

```
.github/
└── workflows/
    └── ci.yml                          NEW — the one workflow. ~120 lines.

docs/features/dev-ops/ci-pipeline/
├── requirements.md                     exists
├── implementation.md                   this file
├── findings.md                         NEW — Phase 1 empirical record
├── runner-setup.md                     NEW — reproducible runner build (2nd-machine recipe)
├── context.md / tasks.md               Beast Mode scaffolding

Unchanged, deliberately:
  tools/**                              byte-identical (DoD I1)
  .scripts/**                           byte-identical (DoD I1)
  Scripts/**, Configs/**, Prefabs/**    this feature touches NO game code
  .gitignore                            `.tmp/` already covered; nothing to add
```

Machine-specific configuration lives **outside the repo entirely**, in a launch script on the runner host that exports `OVERTHROW_*` and then execs `./run.sh`. Documented in `runner-setup.md`, never committed.

---

## 5. Implementation Phases

> **Agent note for every phase in this feature:** the work is bash, YAML and GitHub configuration. **Every phase requires a Bash-capable, full-toolset agent.** The project's `component-developer-advanced` / `network-specialist-advanced` agents have no Bash tool and cannot execute *any* phase here — they are EnforceScript agents and this feature contains no EnforceScript.
>
> **Human-in-the-loop:** Phases 1 and 3 contain steps only the repository owner can do (generate a runner registration token, change repo Actions settings, click *Approve and run*, open PRs from a fork). An agent must **stop and ask** rather than request a PAT. No agent in this feature should ever hold a personal access token.

---

### Phase 1: Empirical spike — runner, workspace, isolation — **REQUIRES ADVANCED (MAX-EFFORT) AGENT + BASH + USER**

**Goal:** Replace every GitHub-side and workspace-side assumption with an observed fact in `findings.md`. Nothing in `.github/` is written until this passes its gate.

> **This phase is the crux.** The tools are proven; what is *not* proven is that they behave identically when the tree lives at `/mnt/n/<ci-work>/Overthrow.Arma4/Overthrow.Arma4` instead of `/mnt/n/Projects/Arma 4/Overthrow.Arma4`, and that GitHub's fork/permission/queue behaviour matches the documentation. Task 1.6 in particular guards the single worst outcome this feature can produce: **CI reporting on the developer's working tree instead of the PR.**

**Tasks:**

- [ ] **1.1** Create `docs/features/dev-ops/ci-pipeline/findings.md` with the epic's table shape: *action → observed result → wall time → notes*. Record at the top: runner version, WSL distro + kernel, Reforger/Tools build (must match `tools/README.md`'s 1.7.0.54 / 190965 — findings are build-specific), and the runner's `--work` path.
- [ ] **1.2** **Register the runner (user-driven).** User generates a registration token from *Settings → Actions → Runners → New self-hosted runner* (expires in ~1 h) and runs `./config.sh` in WSL with: `--url https://github.com/ArmaOverthrow/Overthrow.Arma4`, `--labels overthrow-wsl`, and **`--work <a path under /mnt/n>`**. Record: does `config.sh` accept a DrvFs work dir? Any permission/exec-bit complaints? Install the runner binaries themselves in the **Linux** filesystem (`~/actions-runner`) for speed and correct file modes — only `--work` is on `/mnt/n`. Start it with `./run.sh` in a terminal in the logged-in session (**not** `svc.sh install` — the game needs the session and a GPU).
- [ ] **1.3** **Trivial job.** A throwaway workflow on a scratch branch running `echo`, `pwd`, `uname -a`, `env | grep -c GITHUB`. Confirms the runner picks up work at all and shows the workspace path. Record it.
- [ ] **1.4** **Checkout behaviour on DrvFs.** `actions/checkout` into the `/mnt/n` work dir. Measure: first checkout wall time, subsequent checkout wall time. Then the decisive pair — run twice with **`clean: true`** and twice with **`clean: false`**, and after each record whether `resourceDatabase.rdb` and `.tmp/` survived. Confirms or refutes the Decision-6 prediction that default cleaning wipes the resource DB and forces a ~60 s cold scan every run.
- [ ] **1.5** **The tools, from the workspace.** In the runner's workspace checkout, by hand: `bash tools/lib/common.sh --self-test` (29 assertions — path round-trips are the thing at risk), then `tools/compile-check.sh`, then `tools/run-tests.sh "{6A6E2A002F53A581}"`. Record exit codes, wall times, and the `Loaded addons:` evidence. Note whether **any** `OVERTHROW_*` override was needed (prediction: none — the built-in defaults already match this machine).
- [ ] **1.6** **🔴 ISOLATION PROOF — the most important task in the feature.** Prove the toolchain verifies the *workspace* tree, not the developer's repo. Both directions:
  - **(a)** Introduce a deliberate compile error **in the workspace checkout only** (e.g. a stray `?:` or a dropped `;`). `tools/compile-check.sh` from the workspace → **must exit 1** and name the workspace's file. The developer's repo is untouched.
  - **(b)** Revert (a). Introduce a deliberate compile error **in the developer's repo only** (`/mnt/n/Projects/Arma 4/Overthrow.Arma4`, uncommitted). `tools/compile-check.sh` from the workspace → **must still exit 0**. Revert.
  - **(c)** Repeat the same two directions for `tools/run-tests.sh` using a one-line perturbation of an existing green case, and confirm the `Loaded addons:` block in `.tmp/run-tests/console.log` names the **workspace** `addon.gproj`.
  - If (b) or (c) fails — i.e. the client or Workbench resolves the wrong Overthrow through `<My Games>/ArmaReforgerWorkbench/addons` or an addon-dir precedence quirk — **stop**. The fix is an `OVERTHROW_ADDONS_DIRS` / `OVERTHROW_GAME_ADDONS_DIRS` override in the runner environment, and it must be found and recorded here, not discovered later by a wrong verdict.
- [ ] **1.7** **Concurrency + cancellation.** Start a long job, cancel it from the GitHub UI mid-`run-tests`. Record: does the runner's SIGINT/SIGTERM reach the tool's trap (documented 130/143 with verified kill)? Afterwards `tasklist.exe | grep -i armareforger` → expect nothing. Then run `bash tools/lib/common.sh --sweep-stale` and record whether it lists anything. This settles whether `cancel-in-progress: true` is safe (Decision 5).
- [ ] **1.8** **`OVT_PID_REGISTRY` placement.** Confirm the registry's default (`<workspace>/.tmp/ovt-pids`) is wiped by `clean: true`, and that pinning it to a stable path outside the workspace via the runner env makes the pre-job sweep effective across runs and across checkout policies.
- [ ] **1.9** **GitHub-side settings (user-driven).** Enable *Settings → Actions → General → Fork pull request workflows from outside collaborators →* **"Require approval for all outside collaborators"**. Confirm *"Send write tokens to workflows from pull requests"* and *"Send secrets to workflows from pull requests"* are **off**. Record the exact setting names and screenshots/notes for `runner-setup.md`.
- [ ] **1.10** **Fork-PR token behaviour.** Determine (by test if a second account/fork is available, otherwise by documented behaviour recorded as *unverified*) whether `GITHUB_TOKEN` on a fork PR is read-only, and therefore whether `mikepenz/action-junit-report` can create its check run. This decides whether the junit step needs `continue-on-error: true` and whether the job summary must be the primary result surface (prediction: yes to both — Decision 4).
- [ ] **1.11** **Runner-offline behaviour.** Stop `./run.sh`. Trigger the throwaway workflow. Record exactly what the PR/checks UI shows and for how long (expect an indefinitely "Queued" check; GitHub cancels unpicked jobs after ~24 h). Restart the runner and confirm the job is picked up. This is the evidence behind the README contributor note.
- [ ] **1.12** **Wall-time budget.** From 1.4/1.5, record end-to-end expected job time for: compile-error PR (compile only), clean PR (compile + All group), and a cold first run. These numbers become the DoD's "within N minutes" thresholds.
- [ ] **1.13** **Write up.** `findings.md` gets a **"Differs from assumptions"** section naming anything contradicting this plan, and a **"GitHub-side gaps"** section for anything the platform does that the docs do not describe. Delete the throwaway workflow and the scratch branch.

**Estimated Time:** 3–5 hours (dominated by GitHub round-trips, runner registration and careful recording; the user is needed for 1.2 and 1.9)

**Acceptance Criteria:**
- [ ] `findings.md` exists; every task 1.2–1.12 has an observed result, including the ones that were uneventful.
- [ ] The runner appears **Idle** in repo settings and has executed at least one job.
- [ ] `tools/compile-check.sh` and `tools/run-tests.sh` have both run **from the runner workspace** with recorded exit codes and wall times.
- [ ] **Isolation is proven in both directions** (1.6a and 1.6b/c), verbatim, with the `Loaded addons:` line quoted.
- [ ] The `clean: true` vs `clean: false` resource-DB question is settled by measurement, with both cold and warm timings recorded.
- [ ] Cancellation leaves no surviving Windows process, or the mitigation is recorded.
- [ ] Approval gating is enabled and its exact setting name recorded.
- [ ] "Differs from assumptions" and "GitHub-side gaps" sections exist (even if one says "nothing").

> **Gate:** before Phase 2 begins, re-read Phases 2–5 against `findings.md` and amend them **in place** (the pattern #1 and #2 both used). Specifically: if 1.6 showed wrong-tree resolution, the `OVERTHROW_*` override lands in Phase 2's runner-env contract *at the gate*, not improvised inside a step. If 1.4 showed `clean: false` is unsafe, Decision 6 flips and the cold-scan cost is accepted and documented.

---

### Phase 2: The workflow file

**Agent tier: STANDARD (high). Requires Bash** (the workflow is exercised by `workflow_dispatch`, and the annotation/summary shell is written and dry-run locally).

**Goal:** `.github/workflows/ci.yml` exists and produces correct, honest outcomes for every row of the verdict table — proven by `workflow_dispatch` on a scratch branch, before any PR is involved.

**Tasks:**

- [ ] **2.1** Create `.github/workflows/ci.yml`. Triggers: `pull_request` (default types; `paths-ignore: ['**/*.md', '.claude/**']`) and `workflow_dispatch` with a `test_target` choice input (`all` | `fast` | `smoke` | `none`, default `all`). **No `push`. No `schedule`. No `pull_request_target` — ever** (Decision 2).
- [ ] **2.2** Workflow-level `permissions: {contents: read, checks: write}` and nothing else. `concurrency: {group: ci-${{ github.event.pull_request.number || github.ref }}, cancel-in-progress: true}` (Decision 5). One job, `runs-on: [self-hosted, overthrow-wsl]`, `timeout-minutes: 20`, `defaults.run.shell: bash`.
- [ ] **2.3** **Step 1 — checkout.** `actions/checkout` pinned to a full commit SHA with a `# vN.N.N` comment (Decision 9), `fetch-depth: 1`, and the `clean:` value decided at the Phase 1 gate.
- [ ] **2.4** **Step 2 — pre-job hygiene.** `bash tools/lib/common.sh --sweep-stale --kill`, guarded on the file existing. `tools/README.md` asks this feature by name to do it. Log its output into the job summary; a non-zero return (kill unverifiable) fails the job early with a clear message — a runner that cannot clean up must not go on to produce verdicts.
- [ ] **2.5** **Step 3 — preflight and target resolution.** In one step:
  - Fail loudly if `tools/compile-check.sh` / `tools/run-tests.sh` are missing or not executable ("this checkout predates the dev-ops tooling").
  - Resolve `TEST_TARGET` through the ladder (Decision 3): `Configs/Tests/OVT_TestGroup_All.conf` present → the pinned All GUID; else `…/TestSuites/Smoke/OVT_TEST_SmokeSuite.c` present → `OVT_TEST_SmokeSuite` + `DEGRADED=1`; else empty + `DEGRADED=2`. `workflow_dispatch` input overrides rung 1 (`fast` → the Fast GUID, `none` → skip tests).
  - The **two group GUIDs are the only hardcoded contract values in the file**, quoted verbatim from `tools/README.md`, with a comment pointing there. Never read the `.conf`.
  - Emit `::warning title=Reduced test target::` when degraded, and write the resolved target + reason to `$GITHUB_STEP_SUMMARY`.
- [ ] **2.6** **Step 4 — compile check.** Run `tools/compile-check.sh`, capturing stdout to a file and its **own** exit code (`set +e`; never `| tee` without `PIPESTATUS`; note that Actions `run:` blocks are `bash -e` by default, so `set +e` is mandatory — this is the #2 false-green vector from the Quality Bar). Then:
  - Transform each `path:line: error: message` line into `::error file=<path>,line=<line>::<message>` (escape `%`, `\r`, `\n` per GitHub's annotation rules).
  - Write the **full** error list to `$GITHUB_STEP_SUMMARY` (annotations are display-capped at ~10 per level per step; the summary and the artifact are the complete record).
  - Map rc → outcome per the verdict table, with distinct `title=` values for 1 / 2 / 124. Export rc as a step output so later steps can branch.
- [ ] **2.7** **Step 5 — save-state precondition.** `if: compile succeeded && tests will run && .scripts/reset_save.sh exists` → `.scripts/reset_save.sh --profile OverthrowCI`. **`--profile` is mandatory and unconditional**; the runner environment must never set `OVERTHROW_SAVE_DIR` (Decision 8). Echo the script's `Resolved save directory:` line into the summary — that line is the proof the destructive guard resolved where it should.
- [ ] **2.8** **Step 6 — tests.** `if: compile rc == 0 && TEST_TARGET != ''` → `tools/run-tests.sh "$TEST_TARGET"`, same rc-capture discipline. stdout (failing test names) and the stderr summary line both go to `$GITHUB_STEP_SUMMARY`; rc → outcome per the verdict table with distinct titles for 1 / 2 / 124.
- [ ] **2.9** **Step 8 — artifacts.** `actions/upload-artifact` (SHA-pinned), `if: always()`, `if-no-files-found: warn` (a 124 run legitimately produces no test artifacts), `retention-days: 14`, name including `github.run_id` + `run_attempt`. Paths: `.tmp/compile-check/last.log`, `.tmp/compile-check/last-launch.txt`, `.tmp/run-tests/**`.
- [ ] **2.10** **Grep the file clean**: no `/mnt/`, no drive letters, no `OneDrive`, no `Steam`, no `.exe`, no secret references, no `pull_request_target`. This is a DoD criterion; check it now.
- [ ] **2.11** **Exercise every verdict row via `workflow_dispatch` on a scratch branch** — clean (0/0), compile error (1), compile indeterminate (Steam stopped → 2), test failure (perturb a green case → 1), test indeterminate (target `none`/bogus → 2), reduced target (temporarily remove the group config → warning path). Record each observed PR-UI/summary result in `findings.md`. Revert every perturbation.

**Estimated Time:** 2.5–3.5 hours

**Acceptance Criteria:**
- [ ] A `workflow_dispatch` run on a clean tree is **green**, with a summary naming the target and the case count.
- [ ] Every row of the verdict table has been produced at least once and recorded.
- [ ] A compile error yields an annotation carrying the correct **file and line**.
- [ ] Exit 2 produces a red check whose title and summary say *indeterminate*, distinctly from a genuine failure.
- [ ] Removing the group config yields a green-with-warning run whose summary states which reduced target ran.
- [ ] `grep -nE '/mnt/|[A-Za-z]:\\|OneDrive|Steam|\.exe|pull_request_target' .github/workflows/ci.yml` → no matches.
- [ ] `git diff --stat -- tools/ .scripts/` → empty.

---

### Phase 3: PR integration and surfacing — **ADVANCED AGENT RECOMMENDED + BASH + USER**

**Agent tier: ADVANCED.** GitHub-side semantics (fork tokens, approval gating, check-run creation) are the remaining unknowns and each verification cycle costs a real PR. **The user must open/approve at least one fork PR** if that path is verified live.

**Goal:** The PR experience is the deliverable, verified with real pull requests rather than inferred.

**Tasks:**

- [ ] **3.1** Add **step 7 — junit report**: `mikepenz/action-junit-report` (SHA-pinned, `# v5.x`), `if: always()`, `continue-on-error: true`, `report_paths: .tmp/run-tests/junit.xml`, `check_name: Autotests`, `fail_on_failure: false`, `require_tests: false`, `detailed_summary: true`. **`fail_on_failure: false` is deliberate**: the job's verdict comes from `tools/run-tests.sh`'s exit code, never from a third-party XML parser (Decision 4).
- [ ] **3.2** Confirm the junit check renders per-test results on a PR from a **branch in this repo**: pass case and fail case.
- [ ] **3.3** Confirm the **fork-PR** behaviour recorded in finding 1.10: if the read-only token blocks check creation, prove `continue-on-error: true` keeps the job's own verdict intact and that the **job summary is a complete standalone result surface** (target, compile verdict, test counts, failing names). If a live fork test is not possible, state that plainly in `findings.md` as *unverified — behaviour taken from GitHub documentation*.
- [ ] **3.4** **Real PR — deliberate compile error.** Branch, break one file, open a PR against `vanilla-persistence`. Verify: check goes red inside the Phase-1 budget; the Files-changed tab shows the annotation on the right line; **the test step did not run**; the artifact contains `last.log`. Close the PR.
- [ ] **3.5** **Real PR — deliberate test failure.** Perturb one green case (never add `OVT_TEST_MetaSuite` or `OVT_TEST_PersistenceRoundTripSuite` to a group — both are red by design and must never enter a CI run). Verify: compile green, tests red, the failing case **named** in the junit check *and* the summary, `junit.xml` + `autotest_failed.log` in the artifact. Close the PR.
- [ ] **3.6** **Real PR — clean change.** A trivial code-only change. Verify green inside the budget, junit check shows the full case count (30 for the All group), artifacts present on the green run too.
- [ ] **3.7** **Concurrency in anger.** Push twice in quick succession to an open PR; confirm the first run is superseded, the second completes, and no orphan process survives (`tasklist.exe`, then `--sweep-stale`).
- [ ] **3.8** **Approval gating observed.** With 1.9's setting on, confirm an outside-collaborator PR shows *"1 workflow awaiting approval"* and that **nothing executes on the runner** before a maintainer clicks *Approve and run*. Record the exact UI wording for the README note.
- [ ] **3.9** Record every one of the above in `findings.md` with PR numbers, wall times and the observed UI text.

**Estimated Time:** 2.5–3.5 hours (dominated by PR round-trips)

**Acceptance Criteria:**
- [ ] Three real PRs (compile error / test failure / clean) each produced the expected check, within the recorded time budget.
- [ ] Per-test results are visible on a same-repo PR without downloading anything.
- [ ] Fork-PR degradation (if any) is proven harmless, or explicitly recorded as unverified.
- [ ] Artifacts are present on **all three** runs, including the red ones.
- [ ] Approval gating observed to hold work before execution.
- [ ] No orphaned Windows process after the whole sequence.

---

### Phase 4: Runner operations and the second-machine recipe

**Agent tier: STANDARD (high). Requires Bash** (the launch script is written and tested on the machine).

**Goal:** The runner survives a reboot, starts without ceremony, carries its environment correctly, and can be rebuilt elsewhere by someone who has never seen this machine.

**Tasks:**

- [ ] **4.1** Write the **runner launch script** (lives on the host, **outside the repo**, never committed): exports the `OVERTHROW_*` variables the Phase-1 gate proved necessary — expected to be few or none, plus `OVT_PID_REGISTRY` pinned outside the workspace (finding 1.8) — then `exec ./run.sh`. Document why this exists instead of `tools/config.local.sh`: that file is gitignored and can never appear in a workspace checkout.
- [ ] **4.2** **Start-at-logon**, still as a session process: a Windows Startup shortcut or Task Scheduler "at logon" entry invoking `wsl.exe -d <distro> -u <user> -- bash -lc '<launcher>'`. **Explicitly not** `svc.sh install` — a Windows service has no interactive session and no GPU, which is the whole reason for D-2. Verify by rebooting and confirming the runner returns to **Idle** and picks up a `workflow_dispatch` run.
- [ ] **4.3** Write `docs/features/dev-ops/ci-pipeline/runner-setup.md`, complete enough to build a second runner from nothing:
  - **Prerequisites:** Windows + GPU, Steam installed/running/logged in, Reforger + Reforger Tools (stable branch), **packed workshop EPF/EDF** under `My Games/ArmaReforgerWorkbench/addons` (source repos do not compile — `tools/README.md`), WSL2 with interop enabled, git.
  - **Install:** runner binaries in the Linux FS; `./config.sh` with `--labels overthrow-wsl` and **`--work` under a `/mnt/<drive>` path** — with the reason stated (`ovt_win_path` refuses Linux-filesystem paths, and the Windows exes must be able to read the checkout).
  - **Environment:** the launch script, what may go in it, and the hard rule that `OVERTHROW_SAVE_DIR` must **never** be set on a runner.
  - **First run:** expect the one-off ~60 s resource-DB scan; `bash tools/lib/common.sh --self-test` as the smoke check; a `workflow_dispatch` run as the acceptance test.
  - **Operations:** how to tell if the runner is offline (checks stuck Queued), how to restart it, how to run the stale sweep by hand, where artifacts and logs live, disk-usage expectations (`_diag/`, `.tmp/`, per-run game logs under the `OverthrowCI` profile) and how to prune them.
  - **Security posture:** approval gating is the only control between an outside PR and arbitrary code execution as this user on this machine; keep `permissions:` minimal; never enable write tokens or secrets for PR workflows; never add a secret this workflow does not need (it needs none). Ephemeral-runner hardening is the documented first follow-up (Decision 7).
  - **Teardown:** `./config.sh remove` with a fresh token; what to delete.
- [ ] **4.4** Verify the recipe by **following it literally** and correcting anything that required knowledge not on the page. (A second physical machine is not required — reading it back adversarially against the observed history in `findings.md` is.)

**Estimated Time:** 1.5–2 hours

**Acceptance Criteria:**
- [ ] The runner returns to Idle automatically after a host reboot + logon.
- [ ] `runner-setup.md` contains no step that says "as before" or assumes this machine's layout.
- [ ] The launch script is outside the repo and `git status` shows nothing new because of it.
- [ ] The security posture section states the trust model in plain language, including what an approved malicious PR could do.

---

### Phase 5: Documentation (Definition of Done)

**Agent tier: STANDARD (high).**

**Goal:** Per the epic's documentation policy, update every document this feature invalidates — and say only what exists. `requirements.md` names four; the epic adds a fifth.

**Tasks:**

- [ ] **5.1** **`CLAUDE.md`** — Development Workflow section. Add the CI gate: PRs get an automatic compile check + the All test group on a self-hosted runner; `workflow_dispatch` exists for Fast/manual runs; there is **no push trigger**; a check stuck on *Queued* means the runner is offline, not a failure. Keep the existing local-first guidance (`tools/compile-check.sh` before pushing) — CI is a backstop, not a substitute for the 5-second local loop.
- [ ] **5.2** **`docs/technical-design.md` §10** — the section already documents the compile check and the suites (features #1–#3). Add a short **CI** subsection: what runs, on what trigger, what gates what (compile blocks tests; nothing blocks merge — the pipeline reports, humans decide), where artifacts go, and the fact that the runner is a single self-hosted machine so availability is not guaranteed.
- [ ] **5.3** **`README.md`** — a contributor-facing note in/near **Contributing** (line ~56): what CI checks on a PR, that outside-contributor PRs need a maintainer to approve the run, that a *Queued* check means the runner is offline, and where to find the artifacts. Short — three or four sentences.
- [ ] **5.4** **`.claude/skills/workbench-workflow/SKILL.md`** — a **"CI vs. local"** section: run `tools/compile-check.sh` locally (5 s) and the relevant group locally before pushing; CI is the pre-merge backstop and the thing that catches "works on my machine"; CI never publishes; the runner is one machine, so do not treat a Queued check as a failure. Link `tools/README.md` and `runner-setup.md` rather than restating them.
- [ ] **5.5** **`docs/features/dev-ops/epic-overview.md`** — flip #4 to complete, refresh the rollup line and task count, and add the cross-feature note that **#5 release-automation must never be triggerable by this workflow**.
- [ ] **5.6** Cross-check: `grep -ri "no ci\|no automated\|manual play-testing only"` across the repo; fix or explicitly flag what turns up. Do **not** weaken the still-true claims — no debugger, and play-testing remains the only gate for MP/JIP/UI/performance and the gated save-reload round-trip.

**Estimated Time:** 1–1.5 hours

**Acceptance Criteria:**
- [ ] All four documents named in `requirements.md` are updated, plus the epic overview.
- [ ] No document claims CI does anything it does not do (it does not publish, does not merge, does not run on push, and does not guarantee availability).
- [ ] No document overstates coverage — CI runs whatever the checkout contains, which is 30 cases across four tiers today.
- [ ] Claims about the debugger and about manual MP/JIP/UI testing are untouched.

---

**Total estimated effort:** 10.5–15.5 hours, front-loaded into Phases 1 and 3 (the two that talk to GitHub).

---

## 6. Key Technical Decisions

### Decision 1: The runner lives inside WSL, as a session process, not a service

**Context:** The tests need a GPU and a logged-in Windows session; the toolchain is bash and lives in WSL.
**Decision:** Install the GitHub Actions runner **in WSL**, register it as a Linux runner with a custom `overthrow-wsl` label, and run it with `./run.sh` from a terminal in the user's logged-in session — started manually or at logon, never via `svc.sh install`.
**Rationale:** It puts the runner on the same side of the interop boundary as `tools/lib/common.sh`, so a workflow step is literally the command the developer types; a Windows runner would introduce PowerShell and a second quoting model precisely where quoting bugs live, and would have to re-cross into WSL to call the tools anyway. A **service** is excluded on hard grounds, not preference: a Windows service has no interactive desktop and no GPU access, and the game client cannot render without one. The pattern is established in the wild — [zenn.dev/tenpa](https://zenn.dev/tenpa/articles/wsl-self-hosted-runner-cicd?locale=en). Cost: the runner is only online while the user is logged in, which is accepted and documented (Decision 10).
**Alternatives considered:** Native Windows runner (extra language boundary, no benefit — the exes are launched by the tools either way); runner as a Windows service (impossible: no session, no GPU); a dedicated always-on machine (out of scope — `requirements.md` says one working runner first).

### Decision 2: `pull_request` only. `pull_request_target` is banned, permanently

**Context:** A public repo with outside contributors and a self-hosted runner on a personal machine is the exact configuration GitHub's own docs single out as dangerous.
**Decision:** Trigger on `pull_request` and `workflow_dispatch` only. **`pull_request_target` must never appear in this workflow**, and the DoD greps for it.
**Rationale:** `pull_request_target` runs in the *base* branch context with a **read-write** token and **bypasses the approval gate entirely** — on a self-hosted runner that is a straight path from "stranger opens a PR" to "arbitrary code runs as me on my machine with a write token". `pull_request` is the safe form: fork code runs with a read-only token and, with approval gating on, only after a maintainer looks at it. Sources: [GitHub — managing Actions settings for a repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository), [StepSecurity — defending public-repo CI/CD](https://www.stepsecurity.io/blog/defend-your-github-actions-ci-cd-environment-in-public-repositories), [community discussion #26722](https://github.com/orgs/community/discussions/26722). The ban is written into the plan rather than left to judgement because the temptation to reach for it appears exactly when something (like the fork-PR junit check) does not work.
**Alternatives considered:** `pull_request_target` to get a write token for the junit check on fork PRs — rejected outright; Decision 4's job-summary fallback covers the need at zero risk.

### Decision 3: The test target is resolved by file presence, through a three-rung ladder

**Context:** CI checks out git, not the working tree. At planning time the Fast/All group configs exist on disk but are **untracked**, and `main` has no test tree at all. `run-tests.sh` correctly reports an invalid target as exit 2 — which would make CI permanently red-indeterminate on any checkout without the configs.
**Decision:** One preflight step resolves a single `TEST_TARGET` variable:
1. `Configs/Tests/OVT_TestGroup_All.conf` present → the pinned **All** GUID `{6A6E2A002F53A581}`. Normal path.
2. else `Scripts/Game/Tests/TestSuites/Smoke/OVT_TEST_SmokeSuite.c` present → `OVT_TEST_SmokeSuite`, **degraded**, with a `::warning::` and a job-summary line naming what actually ran.
3. else no target → tests skipped, **degraded**, warned; the job's verdict is the compile check alone.

`workflow_dispatch` overrides rung 1. The two group GUIDs are the **only** hardcoded contract values in the workflow, quoted verbatim from `tools/README.md` (which declares them the stable contract and says never to read the `.conf` files) — presence of the file is the guard, the GUID is the value.
**Rationale:** It makes CI useful on `vanilla-persistence` and on feature branches *today* and correct on `main` after the merge, with no re-wiring when #3's configs get committed. It also keeps the honest-verdict property: a degraded run is green, but never *silently* green — the warning annotation, the summary, and the junit case count all state what ran. Rung 2 becomes dead code once the group configs are committed on every live branch; **delete it then** rather than carrying it forever.
**Alternatives considered:** Hardcode the GUID unconditionally (permanently red-indeterminate on any checkout without it — worse: it reads as "the tests are broken"); fail the job when the target is missing (blocks contributors on branches that never had tests); read the GUID out of the `.conf.meta` at run time (violates the stated contract and couples CI to a file format for no benefit).

### Decision 4: The job summary is the primary result surface; the junit check is best-effort

**Context:** Results must be visible on the PR. `mikepenz/action-junit-report` posts per-test results as a check run — but on a **fork** PR the `GITHUB_TOKEN` is read-only, so creating a check run can fail.
**Decision:** Every step writes its outcome to `$GITHUB_STEP_SUMMARY` — resolved target, compile verdict (with the full error list), test verdict (counts + failing names + the tool's own stderr summary line). The junit action runs `if: always()` with `continue-on-error: true` and `fail_on_failure: false`; it is a nicety layered on top, never the source of truth.
**Rationale:** The job's verdict must come from `tools/run-tests.sh`'s exit code, which is the thing #2 built to be honest — letting a third-party XML parser decide green/red would put the epic's most carefully-defended property in someone else's repository. `fail_on_failure: false` also avoids double-gating, where a parser disagreement produces an unexplainable red. And the summary works identically for fork PRs, same-repo PRs and `workflow_dispatch`, so there is exactly one surface that is always right. `mikepenz/action-junit-report@v5` is chosen over `dorny/test-reporter` as the currently better-maintained option ([mikepenz/action-junit-report](https://github.com/mikepenz/action-junit-report)).
**Alternatives considered:** Junit action as the gate (`fail_on_failure: true`) — moves the verdict off the honest exit code; PR comments instead of a check (noisy, needs `pull-requests: write`, and still fails on forks); writing our own junit parser (reimplementation of #2's verdict, forbidden by the ownership boundary).

### Decision 5: One job, sequential steps; concurrency keyed per-PR with cancellation

**Context:** Two-stage gating (compile, then tests) could be two jobs. Only one Workbench/game may run on the machine at a time.
**Decision:** **One job**, `compile-and-test`, with the test step conditioned on the compile step's exit code being exactly 0. Machine-level serialization comes from the runner itself (a non-ephemeral self-hosted runner executes one job at a time). The Actions `concurrency` group is keyed **per PR** with `cancel-in-progress: true`.
**Rationale:** Two jobs would each pay runner acquisition and a fresh checkout while adding nothing — the single runner already serializes them, so they could never overlap anyway. Keying concurrency per PR gets the thing a global group cannot: pushing three times to a PR in five minutes cancels the stale runs instead of queueing three full game launches. Cancellation is safe because the tools install SIGINT/SIGTERM traps that kill the Windows process and verify death (documented 130/143), and because step 2 of the *next* job sweeps the pidfile registry regardless — Phase 1 task 1.7 proves this rather than assuming it. Requiring exit **0** (not merely "not 1") before running tests satisfies "never run tests against a build that did not compile": an indeterminate compile means we do not know, and the tests are not the way to find out.
**Alternatives considered:** A single global concurrency group with `cancel-in-progress: false` (queues redundant runs on a machine someone is using); separate compile and test jobs (double the overhead for the illusion of parallelism); `needs:` with artifact passing (all cost, no benefit at one runner).

### Decision 6: `clean: false` on checkout, to keep the resource database warm

**Context:** `actions/checkout` defaults to `clean: true` → `git clean -ffdx`, which removes **gitignored** files. `resourceDatabase.rdb` is gitignored, and `tools/README.md` documents a one-off ~60 s project resource-DB scan when it is absent.
**Decision:** Check out with `clean: false` (subject to the Phase-1 gate: **measure it, do not assume it**), and rely on the tools' own stale-artifact discipline for correctness.
**Rationale:** Without this, every CI run pays ~60 s of scan on a machine the developer is using — a 5x increase in job time for no correctness gain. It is safe because the correctness burden does not rest on cleaning: `compile-check.sh` deletes its `last.log` before launch and `run-tests.sh` deletes `.tmp/run-tests/*` before launch (both documented, both specifically so a previous run's artifact can never satisfy this run's verdict), and `git checkout --force` still removes files that were tracked in the previous ref. The residual exposure is untracked files left by an earlier run, which in this pipeline means build caches only. If Phase 1 shows otherwise, the decision flips and the cold-scan cost is simply accepted and documented — it is a performance decision, never a correctness one.
**Alternatives considered:** `clean: true` and eat the scan (honest, slow); pre-seeding the rdb from outside the workspace (extra machinery, same result); relocating the rdb (not configurable).

### Decision 7: A persistent runner, not an ephemeral one — with the hardening written down as the next step

**Context:** `--ephemeral` (one job per runner registration, plus a relaunch loop) is the standard hardening so one job cannot poison the next.
**Decision:** **Persistent runner.** Ephemeral mode is documented in `runner-setup.md` as the first hardening follow-up, with the exact trigger conditions for adopting it: the repo drops approval gating, contributor volume grows past what one maintainer can review per PR, or a second (non-personal) runner appears.
**Rationale:** YAGNI against a real cost. Ephemeral means a fresh workspace per job, which fights Decision 6 (the warm resource DB) and adds a supervisor loop, re-registration and a token source to a setup that is otherwise "run one command in a terminal". The threat it defends against — job-to-job poisoning — is already gated: with approval required for outside collaborators, nothing runs on the machine until a maintainer has read the diff, and the mitigating control against a *malicious approved* PR is not ephemerality but the fact that it would already be running as this user on this machine. Saying that plainly in the docs is worth more than a container-shaped illusion of isolation. Sources: [StepSecurity](https://www.stepsecurity.io/blog/defend-your-github-actions-ci-cd-environment-in-public-repositories), [GitHub Actions settings](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository).
**Alternatives considered:** Ephemeral + relaunch loop now (real cost, marginal gain given approval gating); a dedicated VM/second machine (out of scope).

### Decision 8: Machine configuration lives in the runner's environment, never in the repo

**Context:** `tools/config.local.sh` is the documented per-machine override file — and it is **gitignored**, so it can never exist in a workspace checkout. The workflow must contain no machine paths.
**Decision:** Any `OVERTHROW_*` override reaches the tools through the **runner process's environment**, exported by a launch script that lives on the host outside the repo. The workflow file has no `env:` block of machine values, no secrets and no repository variables carrying paths. Two hard rules on top:
- **`.scripts/reset_save.sh` is always called with `--profile OverthrowCI`**, never bare.
- **`OVERTHROW_SAVE_DIR` is never set in the runner environment.** Unset, the save tools default to the *user's real Workbench campaign save*; #3's Phase 2 found that a set-but-empty value used to fall through to exactly that. This pipeline must not be one typo away from deleting the developer's save.
**Rationale:** It satisfies the requirement ("secrets and machine-specific paths are not hardcoded into committed workflow files") through structure rather than discipline: there is nowhere in the repo for a machine path to live, so a review that greps the workflow is sufficient. It also makes the second-runner story a config-file edit. Pinning `OVT_PID_REGISTRY` outside the workspace is included here so the pre-job sweep works across runs regardless of checkout policy.
**Alternatives considered:** Repository variables/secrets for paths (unnecessary indirection, and secrets are deliberately absent so a malicious PR has nothing to steal); committing a CI-specific config file (violates the requirement and encodes one machine into the repo).

### Decision 9: Third-party and first-party actions are pinned to commit SHAs

**Context:** Three marketplace actions run on a machine with a GPU, a Steam session and the developer's files.
**Decision:** `actions/checkout`, `actions/upload-artifact` and `mikepenz/action-junit-report` are pinned to **full commit SHAs**, each with a trailing `# vX.Y.Z` comment. No floating tags.
**Rationale:** A mutable tag on a self-hosted runner is a supply-chain hole: the tag owner can change what executes on this machine without a PR. SHA pinning costs one comment per line and is the standard recommendation for public repos with self-hosted runners ([StepSecurity](https://www.stepsecurity.io/blog/defend-your-github-actions-ci-cd-environment-in-public-repositories)). The version comment keeps upgrades a deliberate, reviewable act.
**Alternatives considered:** Major-version tags (`@v4`) — convenient, mutable, and the convenience is worth nothing here; vendoring the actions (absurd for three well-known ones).

### Decision 10: Runner unavailability is documented, not engineered around

**Context:** `requirements.md` requires that runner unavailability be obvious and distinguishable from a genuine test failure.
**Decision:** No machinery. The check sits **Queued** (never red), and that state is explained in three places: the README contributor note, `runner-setup.md`, and the `workbench-workflow` skill. Phase 1 task 1.11 records the exact UI text so the docs quote reality.
**Rationale:** "Queued" is already unambiguous and is already *not* a failure — the requirement is satisfied by the platform, and everything a bot could add (a scheduled ping, a status badge job, a fallback runner) needs infrastructure that is itself subject to the same single-machine availability. The honest statement — *"CI runs on one maintainer's machine; if it is off, your check waits"* — is more useful to a contributor than a green badge that lies. Straight YAGNI, and the brief calls for exactly this trade.
**Alternatives considered:** A heartbeat workflow on a hosted runner writing a status badge (new moving part, informs nobody at the moment they care); auto-cancelling queued jobs after N minutes (converts a clear "waiting" into a confusing "cancelled").

---

## 7. Definition of Done

All criteria must pass. Written to be verifiable by an evaluator with no implementation context.

### Functional Criteria

- [ ] **F1.** Exactly one workflow file exists: `.github/workflows/ci.yml`. It triggers on `pull_request` and `workflow_dispatch` and on nothing else — **no `push`, no `schedule`, no `pull_request_target`**.
- [ ] **F2.** Opening a PR containing a **deliberate compile error** produces a **failed check within 5 minutes**, carrying at least one `::error file=<path>,line=<n>::` annotation that names the correct file and line, and the test step **did not run**.
- [ ] **F3.** Opening a PR containing a **deliberate test failure** (compile clean) produces a failed check that **names the failing test case** in the PR UI without downloading anything.
- [ ] **F4.** Opening a PR with a **clean code change** produces a green check within **10 minutes**, showing the executed case count (30 for the All group as of today's tree).
- [ ] **F5.** The compile check runs first and the tests run **only** when it exits exactly 0.
- [ ] **F6.** `workflow_dispatch` offers a target choice including **Fast** and **All**; the Fast group is a supported target wired to no automatic trigger.
- [ ] **F7.** `runner-setup.md` exists and covers prerequisites, registration (including the `--work` on a `/mnt` path and why), the environment launch script, first-run expectations, operations, security posture and teardown.
- [ ] **F8.** `findings.md` exists and records, per action, the observed result and wall time — including the **isolation proof** (Phase 1 task 1.6) quoted verbatim, and "Differs from assumptions" / "GitHub-side gaps" sections.

### Quality Criteria

- [ ] **Q1. No wrong-tree verdicts.** Evidence in `findings.md` that a compile error in the runner's checkout goes red **and** that a compile error in the developer's working repo does **not** affect a CI run. The `Loaded addons:` line naming the workspace `addon.gproj` is quoted.
- [ ] **Q2. Exit 2 ≠ failure ≠ pass.** An indeterminate compile or test result produces a red check whose title contains *INDETERMINATE* and whose summary states this is a tooling/environment outcome and not evidence about the code — visibly different from a genuine failure.
- [ ] **Q3. No swallowed exit codes.** Every tool invocation in the workflow captures its own return code (no bare pipe into an annotator, no reliance on `bash -e`). Verified by reading the file **and** by the fact that Q2's cases produce red checks at all.
- [ ] **Q4. Artifacts on every completed run**, red and indeterminate included: `.tmp/compile-check/last.log` plus `.tmp/run-tests/{junit.xml,autotest.log,autotest_failed.log,console.log}` where the run produced them. A 124 timeout legitimately produces no test artifacts and does not fail the upload step.
- [ ] **Q5. Bounded three ways.** Per-tool timeouts (`compile-check` 120 s, `run-tests` 300 s defaults), `timeout-minutes: 20` on the job, and one job at a time on the runner. No run can hang the queue indefinitely.
- [ ] **Q6. Stale-process hygiene.** `bash tools/lib/common.sh --sweep-stale --kill` runs before any launch in every job, and after a full verification sequence `tasklist.exe` shows no surviving `ArmaReforger*` process and the pidfile registry is empty.
- [ ] **Q7. Degraded runs are loud.** A checkout without the group config produces a green run that carries a `::warning::` **and** a job-summary line naming the reduced target. It never silently reports success for tests that did not exist.
- [ ] **Q8. The save guard holds.** Every `.scripts/` invocation in the workflow passes `--profile OverthrowCI`; `OVERTHROW_SAVE_DIR` is set nowhere in the workflow or the runner environment; the resolved save path is echoed into the job summary.
- [ ] **Q9. Reproducibility.** An evaluator following `runner-setup.md` alone can explain every configuration choice on the machine, and the runner returns to Idle by itself after a host reboot + logon.
- [ ] **Q10. Red-by-design suites never run in CI.** `OVT_TEST_MetaSuite` and `OVT_TEST_PersistenceRoundTripSuite` appear in no workflow target and in no group the workflow selects.

### Integration Criteria

- [ ] **I1. Orchestration only.** `git diff --stat -- tools/ .scripts/` is **empty** for this feature's whole branch. No script under `tools/` or `.scripts/` was modified, and the workflow contains no `.exe` path, no `taskkill`, no log-directory globbing and no `junit.xml` parsing of its own.
- [ ] **I2. Nothing machine-specific committed.** `grep -nE '/mnt/|[A-Za-z]:\\|OneDrive|Steam|secrets\.' .github/workflows/ci.yml` returns nothing, and the file references no repository secret.
- [ ] **I3. Least privilege.** The workflow declares `permissions: contents: read` plus `checks: write` and nothing else. It performs no push, no tag, no merge and no publish.
- [ ] **I4. Fork safety.** The repo setting *"Require approval for all outside collaborators"* is enabled; write tokens and secrets for PR workflows are **off**; `pull_request_target` appears nowhere under `.github/`.
- [ ] **I5. Actions pinned.** Every `uses:` line pins a full commit SHA with a version comment.
- [ ] **I6. #5 stays unreachable.** The workflow contains no pack/publish verb, and `epic-overview.md` records that release automation must never be triggerable from CI.
- [ ] **I7. Contract respected.** The only values the workflow hardcodes from `tools/README.md` are the two group GUIDs, quoted verbatim, with a comment pointing at the contract.

### Documentation Criteria

- [ ] **D1. `CLAUDE.md`** — the CI gate is described in the Development Workflow section, including PR-only triggering and the Queued-means-offline note.
- [ ] **D2. `docs/technical-design.md` §10** — a CI subsection documents the pipeline and exactly what it gates (compile blocks tests; nothing blocks merge).
- [ ] **D3. `README.md`** — a contributor-facing note near Contributing: what CI checks, that outside PRs need maintainer approval, and what Queued means.
- [ ] **D4. `workbench-workflow` skill** — a "CI vs. local" section: what to run locally before pushing and what CI adds.
- [ ] **D5. `epic-overview.md`** — feature #4 flipped to complete with a refreshed rollup.
- [ ] **D6.** No document claims CI publishes, merges, runs on push, or is guaranteed available; and no still-true claim (no debugger; manual MP/JIP/UI/perf testing; the gated save-reload round-trip) is weakened.

### Verification Method

An independent evaluator with repository admin access, on the runner machine, Steam running and logged in:

1. **Runner online** — *Settings → Actions → Runners* shows a runner labelled `overthrow-wsl`, status **Idle**. → F7, Q9
2. **Manual dispatch, clean** — *Actions → CI → Run workflow* with `test_target: all` on the working branch. Expect: **green** in ≤10 min; the job summary names the resolved target and the case count; the *Autotests* check lists the cases; the artifact bundle downloads and contains `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log` and `last.log`. → F4, F6, Q4
3. **PR-A — deliberate compile error.** Branch from the working branch, break one `.c` file (e.g. remove a `;`), push, open a PR. Expect: check **red in ≤5 min**; the *Files changed* tab shows the annotation on the broken line; the job log shows the test step **skipped**; the artifact contains `last.log` with the error. Close the PR and delete the branch. → F2, F5
4. **PR-B — deliberate test failure.** Branch, perturb one assertion in an existing green case (do **not** touch `OVT_TEST_MetaSuite` or `OVT_TEST_PersistenceRoundTripSuite`), open a PR. Expect: compile green, tests **red**; the *Autotests* check and the job summary both **name the failing case**; the artifact contains a `junit.xml` with a `<failure>` element and a non-empty `autotest_failed.log`. Close the PR. → F3, Q4, Q10
5. **PR-C — clean change.** Branch, make a trivial code-only change (not markdown — markdown is `paths-ignore`d), open a PR. Expect: **green** in ≤10 min with the full case count. Close the PR. → F4
6. **Indeterminate ≠ failure.** Quit Steam. Dispatch the workflow manually. Expect: **red**, with an error title containing *INDETERMINATE* and a summary stating this is a tooling/environment result rather than evidence about the code. Restart Steam and re-dispatch → green. → Q2, Q3
7. **Degraded target.** On a scratch branch, delete `Configs/Tests/OVT_TestGroup_All.conf*`, dispatch. Expect: **green with a warning annotation**, and a summary line naming the reduced target that actually ran. Delete the scratch branch. → Q7
8. **Isolation.** With CI green, introduce an uncommitted compile error in the developer's own repo at `/mnt/n/Projects/Arma 4/Overthrow.Arma4`. Dispatch the workflow. Expect: **still green** (CI reads its own checkout). Revert. → Q1
9. **Hygiene.** After steps 2–8: `tasklist.exe | grep -i armareforger` → nothing; `bash tools/lib/common.sh --sweep-stale` → lists nothing. → Q6
10. **Concurrency.** Reopen PR-C and push two commits within a minute. Expect: the first run superseded/cancelled, the second completes green, and step 9's checks still clean. → Decision 5
11. **Runner offline.** Stop `./run.sh`. Push to an open PR. Expect: the check sits **Queued**, not red, and matches the wording in the README note. Restart the runner; the job is picked up. → D3, Decision 10
12. **Static review of the workflow** — `grep -nE '/mnt/|[A-Za-z]:\\|OneDrive|Steam|secrets\.|pull_request_target|\.exe|taskkill' .github/workflows/ci.yml` → **no matches**; every `uses:` pins a SHA; `permissions:` is `contents: read` + `checks: write`; `on:` contains no `push` and no `schedule`. → F1, I2, I3, I4, I5
13. **Orchestration proof** — `git diff --stat -- tools/ .scripts/` over the feature branch → **empty**. → I1
14. **Fork policy** — *Settings → Actions → General*: "Require approval for all outside collaborators" selected; write tokens and secrets for PR workflows off. → I4
15. **Docs** — open `CLAUDE.md`, `docs/technical-design.md` §10, `README.md`, the `workbench-workflow` skill and `epic-overview.md`; each carries the claim listed in D1–D5 and none overstates. Then read `runner-setup.md` and confirm no step depends on knowledge that is not on the page. → D1–D6, F7, Q9
16. **Findings** — open `findings.md`; expect a per-action table, the verbatim isolation proof, the `clean:` measurement, cancellation behaviour, and the two required write-up sections. → F8, Q1

---

## 8. Testing Strategy

There is no test framework for a CI pipeline and building one is out of scope. Validation is a written checklist executed by hand, exactly as features #1, #2 and #3 did — with one difference: **several scenarios can only be exercised by opening a real pull request**, so the cost per iteration is minutes, and each perturbation is reverted immediately.

### The verdict matrix (every row must be produced at least once and recorded)

| ID | Scenario | Expected |
|---|---|---|
| T1 | Clean tree, All target | green; case count in summary; artifacts present |
| T2 | Compile error | red; correct file/line annotation; **tests skipped**; `last.log` attached |
| T3 | Test failure | red; failing case named in check **and** summary; `junit.xml` + `autotest_failed.log` attached |
| T4 | Compile indeterminate (Steam down) | red, titled INDETERMINATE, summary disclaims code evidence |
| T5 | Test indeterminate (bogus target) | red, titled INDETERMINATE; `crash.log` attached if produced |
| T6 | Timeout (forced short tool timeout) | red, titled TIMED OUT; no orphan process; upload step does not fail on missing artifacts |
| T7 | Group config absent | green **with warning**; summary names the reduced target |
| T8 | No test tree at all | compile-only run; warning; summary says tests were skipped |
| T9 | Markdown-only PR | no run at all (`paths-ignore`) |
| T10 | Two rapid pushes to one PR | first run superseded; no orphan |
| T11 | Runner stopped | check Queued indefinitely, never red |
| T12 | Three consecutive dispatches, unchanged tree | identical outcome and identical case count each time |

### Isolation (the highest-value tests in the feature)

| ID | Scenario | Expected |
|---|---|---|
| T13 | Error in workspace checkout only | CI red |
| T14 | Error in developer's repo only | CI **green** |
| T15 | `Loaded addons:` inspection | names the **workspace** `addon.gproj` |

### Security posture (verified by inspection, recorded in findings)

| ID | Scenario | Expected |
|---|---|---|
| T16 | Outside-collaborator PR | "waiting for approval"; nothing executes before a maintainer approves |
| T17 | Fork PR token | read-only; junit check may fail; job verdict and summary unaffected |
| T18 | Workflow grep | no machine paths, no secrets, no `pull_request_target`, no `.exe` |

**Not tested here:** anything about Overthrow's behaviour (feature #3), and anything about whether the tools' verdicts are correct (features #1 and #2 — already proven, and re-litigating them here would be reimplementation by another name).

---

## 9. Dependencies

### Internal

- **`dev-ops/workbench-automation` (complete)** — `tools/compile-check.sh`, `tools/launch-game.sh`, `tools/lib/common.sh`, `--sweep-stale --kill`, the `OVERTHROW_*` environment contract, exit codes 0/1/2/124 and the gcc-style stdout that becomes annotations. Consumed, never modified.
- **`dev-ops/autotest-foundation` (complete)** — `tools/run-tests.sh`, the honest `junit.xml` verdict, `.tmp/run-tests/*` artifact names, the accepted `-autotest` target forms.
- **`dev-ops/test-coverage` (in progress, parallel)** — the Fast/All group configs and the suites they name; `.scripts/reset_save.sh --profile` for the save-state precondition. **Soft dependency by design**: Decision 3's ladder means CI works before, during and after #3's configs are committed. The only hard requirement is that #3 does not change the two group GUIDs (`tools/README.md` calls them a stable contract).
- **`tools/README.md`** — the contract this feature builds against, read instead of the script sources.
- **`workbench-automation/findings.md`, `autotest-foundation/findings.md`, `test-coverage/findings.md`** — machine ground truth (client exit codes meaningless, junit failures counted as elements, invalid target → exit 2, timings, OneDrive-redirected profile root).

### External

- **A GitHub repository with admin access** — `github.com/ArmaOverthrow/Overthrow.Arma4` (public). Needed for: registering the runner (short-lived registration token, generated by the user), setting the fork-PR approval policy, and enabling Actions.
- **GitHub Actions runner** (self-hosted, Linux x64 build, installed inside WSL2).
- **Actions used, SHA-pinned:** `actions/checkout`, `actions/upload-artifact`, `mikepenz/action-junit-report`.
- **This machine:** Windows + GPU + logged-in session; WSL2 with interop; Steam installed, **running and logged in**; Arma Reforger 1.7.0.54 + Reforger Tools (stable); packed workshop EPF/EDF under `My Games/ArmaReforgerWorkbench/addons`; a `/mnt/<drive>` path with room for the workspace, per-run game logs and artifacts.
- **The user**, for: the runner registration token, the repo Actions settings, and clicking *Approve and run* on outside PRs. An agent must never be given a PAT for any of it.

### Dependents

- **#5 `release-automation`** — must remain **unreachable from this workflow**. Publishing stays a human-triggered path; the DoD and the epic overview both record it.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **CI verifies the wrong tree.** The Workbench or client resolves Overthrow from `<My Games>/ArmaReforgerWorkbench/addons` or the developer's repo instead of the runner's checkout — producing green checks that say nothing about the PR. | Low-Med | **Critical** — the worst outcome in this feature | Phase 1 task 1.6 proves isolation **in both directions** before anything is built, and quotes the `Loaded addons:` line naming the workspace `addon.gproj`. If it fails, the fix is an `OVERTHROW_*ADDONS_DIRS` override in the runner environment, found at the gate. Q1 is a standing DoD criterion, re-verifiable at any time by step 8 of the Verification Method. |
| **R2** | **Workspace path translation fails.** `ovt_win_path` refuses Linux-filesystem paths by design; if the runner's `--work` lands anywhere but a `/mnt/<drive>` mount, every tool exits 2. | Med (if misconfigured) | High | Architectural: `--work` on `/mnt/n` is a registration-time requirement, stated with its reason in `runner-setup.md`, and proven in Phase 1 task 1.5 by `common.sh --self-test` (29 assertions, path round-trips included). The failure mode is loud (exit 2 / INDETERMINATE), never a silent wrong answer. |
| **R3** | **`git clean -ffdx` wipes `resourceDatabase.rdb`**, so every run pays the ~60 s cold scan and job time grows 5x. | High with default settings | Med | Decision 6 (`clean: false`), **measured** in Phase 1 task 1.4 rather than assumed. Correctness does not depend on it — both tools delete their own stale artifacts before launch. If `clean: false` proves unsafe, the cost is accepted and documented. |
| **R4** | **Steam not running / logged out** on the runner. Only a Steam Workbench binary exists; the dependency cannot be engineered away (`tools/README.md`). | Med | Med | Manifests as exit 2 → a red check titled INDETERMINATE that explicitly says it is not evidence about the code (Q2), so a contributor is not blamed for the maintainer's Steam client. Verification step 6 exercises this deliberately. `runner-setup.md` lists it as a prerequisite and an operations check. |
| **R5** | **Runner offline** — the machine is off, logged out, or `./run.sh` was closed. Contributors' checks hang. | High (it is a personal machine) | Med | Accepted and documented, not engineered around (Decision 10). "Queued" is unambiguous and is not a failure; the README, the skill and `runner-setup.md` all say so, quoting the real UI text recorded in Phase 1 task 1.11. |
| **R6** | **WSL restart / host reboot** drops the runner, or `wsl --shutdown` kills it mid-job. | Med | Low-Med | Start-at-logon (Phase 4 task 4.2) restores it automatically; a job killed mid-flight leaves a Windows process that the **next** job's `--sweep-stale --kill` reaps (the exact scenario `tools/README.md` asks this feature to handle). GitHub marks the interrupted run failed, which is honest. |
| **R7** | **Fork-PR code executes on a personal machine.** An approved malicious PR runs arbitrary code as this user, with access to their files, Steam session and network. | Low (gated) | **Critical** | Approval gating for all outside collaborators (Decision 2 / task 1.9); `pull_request_target` banned and grepped for; `permissions` minimal; **no secrets in the workflow**, so there is nothing to exfiltrate beyond the machine itself; actions SHA-pinned (Decision 9). The residual risk is stated in plain language in `runner-setup.md` — approval is a *human reading the diff*, and it is the only real control. Ephemeral runners are the documented next hardening step (Decision 7). |
| **R8** | **#3's group configs are not in the checked-out tree**, so the pinned GUID is an invalid target → permanent exit 2. | High today (untracked at planning time) | Med | Decision 3's presence-guarded ladder: All group → Smoke suite → compile-only, each degradation warned and summarised. CI is useful on `vanilla-persistence` today and correct on `main` after the merge, with no re-wiring. |
| **R9** | **OneDrive-redirected `My Games`** delays or locks a log file, so log-dir resolution or artifact collection intermittently fails. | Low (never observed) | Med | Inherited mitigation: `tools/README.md` documents `OVERTHROW_MYGAMES_DIR` as the escape hatch, settable in the runner environment (Decision 8) with no repo change. Manifests as exit 2 (INDETERMINATE), never as a false green. Pausing sync is the operational fallback, noted in `runner-setup.md`. |
| **R10** | **Cancelled job orphans a Windows process**, and the machine accumulates zombies until it is unusable. | Low-Med | Med | The tools trap SIGINT/SIGTERM and kill by PID with verified death; every job begins with `--sweep-stale --kill`; Phase 1 task 1.7 verifies this against a real mid-run cancellation before `cancel-in-progress: true` is adopted. Q6 checks it at DoD time. |
| **R11** | **A swallowed exit code produces a false green** — e.g. `tools/compile-check.sh \| annotate` reporting the annotator's status. | Med (easy mistake) | **Critical** | Called out explicitly in the Quality Bar and in task 2.6: `set +e` and explicit `rc` capture, never a bare pipe. Verification steps 3 and 6 would both go green if this regressed, so the DoD catches it empirically rather than only by review. |
| **R12** | **Annotation display cap** (~10 per level per step) hides most errors on a badly broken PR. | Med | Low | The full error list also goes to the job summary and the uploaded `last.log`; the summary is the complete record and the annotations are the convenience. Noted in task 2.6 and in the docs. |
| **R13** | **Third-party action supply chain** — `mikepenz/action-junit-report` executes on this machine. | Low | High | SHA-pinned (Decision 9); `continue-on-error: true` so a failure cannot flip the verdict; it is not the source of truth (Decision 4); it needs only `checks: write` and sees no secret. |
| **R14** | **Disk fill** — per-run game logs under the `OverthrowCI` profile, runner `_diag/`, and workspace `.tmp/` grow without bound. | Med over months | Low-Med | `retention-days: 14` on artifacts; a pruning note and expected growth rates in `runner-setup.md`'s operations section. Deliberately not automated — deleting files on a developer's machine from CI is a worse risk than the disk filling slowly. |
| **R15** | **Scope creep into release automation** — "while we're here, tag the release / publish the addon". | Med | High | Structurally blocked: `permissions: contents: read`, no pack/publish verb anywhere, an explicit out-of-scope entry, a DoD criterion (I6) and a note in `epic-overview.md`. Publishing stays human-only, which was the point of separating #5. |
| **R16** | **A future branch-protection rule requires the CI check, which `paths-ignore` prevents from ever running on a docs-only PR** — blocking the PR forever. | Low (no protection today) | Med | Documented in `technical-design.md` §10 and in the workflow file as a comment: if the check ever becomes required, either drop `paths-ignore` or add a skip-job that reports success. Not configured now — branch protection is out of scope. |

---

## 11. Notes

- **"No push trigger" does not mean "no re-runs".** `on: pull_request` fires on `synchronize`, so pushing new commits to an open PR does re-run CI. Only pushes to branches with no open PR are silent. Worth stating in the docs, because the phrase invites the opposite reading.
- **The workflow file on a `pull_request` run comes from the merge ref** (base + head), so a PR that *adds* the workflow does get checked by it. A PR whose head branch predates the workflow triggers nothing at all — which is the correct, quiet behaviour for `main`-based branches until `vanilla-persistence` lands.
- **`tools/config.local.sh` will never exist on a runner.** It is gitignored, so it cannot be in a checkout. Every "just put it in config.local" instinct must be redirected to the runner's launch script (Decision 8).
- **`.tmp/run-tests/junit.xml` is copied by `run-tests.sh`** from the run's log directory — the workflow must point the junit action at the `.tmp/` copy, not at anything under `My Games`. Same for the compile log.
- **A green CI run does not exercise the shipping define set.** `-validate` always compiles with `WORKBENCH` and `PERSISTENCE_DEBUG` defined (`tools/README.md`, Known limitations). CI inherits that blind spot; it is #1's documented gap, not something to work around here.
- **CI covers what the suites cover, and nothing more.** MP/JIP, UI, performance, AI movement and the save/reload round-trip remain manual (`technical-design.md` §10). A green PR check is a floor, not a release decision — the docs must not let anyone read it as one.
- **`workflow_dispatch` is not a push trigger** and does not conflict with decision D-1. It exists so Fast is a real supported target and so the runner can be exercised without opening a PR.

---

## Related Documentation

- `docs/features/dev-ops/epic-overview.md` — epic scope, build order, documentation policy
- `docs/features/dev-ops/ci-pipeline/requirements.md` — this feature's requirements (see §3 for the four user overrides)
- `tools/README.md` — **the contract this feature orchestrates**; read it instead of the scripts
- `docs/features/dev-ops/workbench-automation/findings.md` — Workbench/client CLI ground truth
- `docs/features/dev-ops/autotest-foundation/findings.md` — junit shape, invalid-target behaviour, timings
- `docs/features/dev-ops/test-coverage/context.md` — suite/tier layout, group configs, save-state preconditions
- `docs/features/dev-ops/workbench-automation/implementation.md` / `autotest-foundation/implementation.md` — the sibling plans this one mirrors in shape and ethos
- [GitHub — managing Actions settings for a repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository)
- [StepSecurity — defending GitHub Actions in public repositories](https://www.stepsecurity.io/blog/defend-your-github-actions-ci-cd-environment-in-public-repositories)
- [GitHub community discussion #26722 — self-hosted runners on public repos](https://github.com/orgs/community/discussions/26722)
- [mikepenz/action-junit-report](https://github.com/mikepenz/action-junit-report)
- [Running a self-hosted runner inside WSL](https://zenn.dev/tenpa/articles/wsl-self-hosted-runner-cicd?locale=en)
