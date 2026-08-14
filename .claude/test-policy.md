# Test-Run Policy — Shared Reference

**This file is the single source of truth for *when* `tools/run-tests.sh` may be run.** It is installed at
**`.claude/test-policy.md`** and consumed by the workflow commands and agent definitions via a short pointer
block (see §7). The rules live here and **only** here — commands and agents never restate them, they point
at this file.

---

## 1. The two gates are not equivalent

| Gate | Cost | Who runs it | How often |
|---|---|---|---|
| `tools/compile-check.sh` | Headless, ~5 s warm, no window, no focus steal | **Everyone** — every agent, every session | Freely. After every edit, until exit 0 |
| `tools/run-tests.sh` | **Launches a real Arma Reforger client that takes over the user's desktop for ~15–20 s** | **The orchestrator only** (see §3) | Rarely, and deliberately (see §2) |

The asymmetry is the whole point. `compile-check.sh` is free, so treat a non-zero exit from it as a hard
stop and re-run it as often as you like. `run-tests.sh` is **intrusive**: it steals focus from whatever the
user is doing, which can wreck a play-test session, a Workbench edit, or unrelated work on the same machine.
It is a good gate, so it stays — but it is a scarce resource and must be spent, not sprayed.

---

## 2. When the suites may be run

**Run them only as a regression gate, after work is finished.** Specifically:

✅ **Do run** — once, in the main thread:
- After an implementation **phase is complete** during `/autorun-feature`, `/proceed`, `/proceed-advanced`
- After a bug fix is complete during `/fix-bug` / `/fix-feature`
- When the user asks for a test run
- When you are specifically working on the test suites themselves (then a bare class name for one
  suite/case, not a group)

❌ **Never run** — no exceptions:
- **During planning.** `/plan-feature`, `solution-architect`, `/suggest-feature`, `/audit-feature`,
  `/discover-feature`, and any read-only review command must not run the suites at all. **There is no
  such thing as a useful baseline run.** A plan is not code; the tree will be changed by concurrent
  bugfix sessions, by other features, and by whatever lands between planning and implementation, so a
  green run taken at planning time proves nothing about the run that matters later.
- **Inside a subagent.** See §3.
- **Mid-phase**, to "check progress" on half-finished work.
- **Repeatedly**, to watch a fix converge. See §5.

⏸️ **Skip the gate entirely** when the completed phase could not have moved the needle:
- Docs, comments, `dev/` or `docs/` only
- `.layout` / prefab / `.conf` authoring with no EnforceScript change
- Localization `.st` edits
- Pure UI work (the suites do not cover UI at all)

Say that you skipped it and why — one line. A skipped gate that is announced is honest; a skipped gate
that is silent reads as a pass.

⚠️ **Defer the gate** if the user may be in-game — e.g. a `tools/launch-server.sh` or `tools/launch-game.sh`
session that this conversation started is still running, or the user said they were play-testing. Launching a
test client on top of that costs them their session. Finish the phase, say the gate is pending, and run it
when the coast is clear or when the user says go.

---

## 3. The orchestrator runs the tests, never the subagent

**Implementation agents (`component-developer`, `network-specialist`, `ui-developer`, their `-advanced`
variants, and any general-purpose agent spawned to write code) must not run `tools/run-tests.sh`.**

Their gate is **`tools/compile-check.sh` exit 0**, plus whatever static checks their own definition names.
They finish there and report what they changed.

The **orchestrator** — the main thread driving `/autorun-feature`, `/proceed`, `/proceed-advanced`,
`/fix-bug` — runs the suites itself, once, after the phase is complete and its docs are updated. Rationale:

- A subagent has no idea what the *other* phases did, so its run is not a regression gate — it is noise.
- N agents in a feature means N client launches, each stealing focus, for one meaningful verdict.
- The orchestrator is the only party that can decide the run is not worth doing at all (§2).

**One exception:** a subagent whose *entire purpose* is verification, spawned by a command the user invoked
for that purpose — i.e. `/evaluate-feature`'s evaluator. It may run the suites **once**, against the target
its acceptance criteria name. Implementation agents, review agents and research agents are not covered by
this and never run them.

**Every implementation-agent prompt must carry this line verbatim:**

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself
> after the phase completes.

---

## 4. Which target to run

- **Fast group** — `tools/run-tests.sh "{6A6E29FF47ECB840}"` — the default for a completed phase.
- **All group** — `tools/run-tests.sh "{6A6E2A002F53A581}"` — only when the phase touched **campaign,
  economy or persistence state**, or serializers/`Configs/Systems/Persistence/`.
- **A bare class name** — one suite or one case — for debugging a specific red, and when iterating on the
  test tree itself. Cheaper than a group and just as intrusive, so it is still an orchestrator call.

Do not run both groups. All ⊃ Fast.

Announce before launching, so the focus steal is not a surprise:

> Running the Fast group — a Reforger client will take focus for ~20 s.

---

## 5. When the gate is red

99.9% of the time the suites pass, so a red is information and deserves a look, not a reflex re-run.

1. **Read the failing case names and `.tmp/run-tests/` artifacts first.** Never re-run to "see if it
   sticks" — these suites are deterministic and `maxAttempts` is banned project-wide precisely so that a
   flake is a bug, not a retry.
2. **Fix it in the main thread** if it is small and you can see it, or **re-engage the implementing agent**
   with the failing case names and the assertion text. The orchestrator may edit code directly here — the
   "always use agents" rule in `/proceed` is about implementing phases, not about repairing a gate.
3. **Re-run once** after the fix. If it is still red, stop and report to the user with the case names and
   what you tried — do not iterate the suite a third time.
4. **Exit 2 is not a failure and not a pass** — it is "no verdict" (tool failure, mistyped target, no
   `junit.xml`). Say so explicitly; never report exit 2 as green.
5. A red that is **pre-existing** — the same case fails on a stash of your changes — is not this phase's
   fault. Say so, and do not sink the phase into it.

---

## 6. Reporting

State the verdict plainly and only claim what was actually verified:

- `Fast group: 0 (101/101)` — verified green
- `Fast group: skipped — layout-only phase, suites cover no UI`
- `Fast group: deferred — your play-test session is still running`
- `Fast group: 2 (indeterminate) — no junit.xml produced; this is not evidence about the code`

Never write "tests pass" without having run them in that session, and never carry forward a verdict from an
earlier phase as if it covered the current one.

---

## 7. The canonical pointer block

Commands and agents paste **only** this, near where they describe verification. Update the wording here;
everything else inherits it.

```markdown
**Test-run policy:** `tools/compile-check.sh` runs freely; `tools/run-tests.sh` launches a real Reforger
client that steals desktop focus, so it is run **by the orchestrator only, once, after a phase completes** —
never during planning, never inside a subagent. See `.claude/test-policy.md` for the full rules.
```
