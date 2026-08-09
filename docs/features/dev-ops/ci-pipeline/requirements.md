# CI Pipeline — Requirements

**Epic:** dev-ops
**Created:** 2026-08-01

## Overview

Turns the compile check (feature #1) and the test loop (feature #2) into automation that runs on its own, on every push and pull request, and reports results where contributors will see them. Orchestration only — it must not reimplement the launching or testing logic those features own.

The binding constraint is that **Reforger has no headless rendering**, so this runs on a self-hosted Windows runner with a GPU, not on GitHub-hosted infrastructure.

## Requirements

- **Runs automatically on push and pull request** against this repository.
- **Two-stage gate:** compile check first (fast, catches most breakage), tests only if compilation succeeds. Never run tests against a build that did not compile.
- **JUnit results are surfaced on the PR** — per-test pass/fail visible without digging through logs. The framework's native `junit.xml` needs no adapter.
- **Compile errors are reported as file/line/message**, not as an undifferentiated log dump.
- **Self-hosted Windows runner**, documented well enough that a second one could be stood up from the notes. Reforger's `Headless` symbols refer to headless *MP clients*; there is no offscreen rendering mode, so GPU-less Linux runners are not an option.
- **The runner must be resilient.** A hung Workbench or game client times out and fails the job rather than blocking the queue indefinitely. Stale processes from a previous run are cleaned up before starting.
- **Artifacts are collected** — `junit.xml`, `autotest_failed.log`, the autotest log and the compile log — and attached to the run for post-mortem.
- **Fast subset on push, full suite where appropriate.** Uses the fast/slow suite split established in feature #3; the exact policy is a planning decision, but the pipeline must support both.
- **Contributors are not blocked by runner unavailability** — if the self-hosted runner is offline, the failure mode must be obvious and distinguishable from a genuine test failure.
- **Secrets and machine-specific paths are not hardcoded** into committed workflow files.
- **Respects the branch policy in force:** `main` is under a bugfix-only code freeze until `vanilla-persistence` lands; active work is on the `vanilla-persistence` branch. CI must be useful on feature branches, not just `main`.

## Definition of Done — documentation

- `CLAUDE.md` — add the CI workflow to the development workflow section
- `docs/technical-design.md` §10 — document the pipeline and what it gates
- `README.md` — contributor-facing note on what CI checks on a PR
- The `workbench-workflow` skill — when to rely on CI vs. test locally

## Dependencies

- **`dev-ops/workbench-automation`** — the compile check and launcher it orchestrates
- **`dev-ops/autotest-foundation`** — a working `-autotest` loop and stable artifact paths
- **Can be built in parallel with `dev-ops/test-coverage`** — needs the loop to exist, not the coverage to be complete
- A Windows machine with a GPU available to act as a runner, and permission to register it against the GitHub repository
- Repository admin access to configure Actions and the runner

## Out of Scope

- **Workshop publishing** — feature #5, deliberately a separate manually-triggered path so CI can never publish by accident.
- **Cloud/hosted runners.** Ruled out by the GPU requirement.
- **Writing tests** — features #2 and #3.
- **Multi-runner / matrix builds.** One working runner first.
- **Auto-merge, release tagging, or any automated push** to a protected branch. The pipeline reports; humans decide.
- **Rewriting the compile or test invocation.** If a command is wrong, it gets fixed in the feature that owns it.
