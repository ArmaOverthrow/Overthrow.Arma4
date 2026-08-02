---
description: "Evaluate a feature against its acceptance criteria using a fresh evaluator agent. Usage: /evaluate-feature [feature-name]"
---

You have been asked to evaluate a completed feature against its acceptance criteria.

This command implements the **generator/evaluator separation** pattern — a fresh agent with no implementation context evaluates the feature purely against its defined acceptance criteria. This avoids the self-evaluation bias where generators "confidently praise" their own work.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic.
If `$ARGUMENTS` is a bare epic name, this command evaluates one feature at a time — list the epic's child features and ask which to evaluate.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When the feature lives inside an epic, every `docs/features/<feature-name>/...` path below is the resolved nested `docs/features/<epic>/<feature>/...` location; pass the loaded epic context to the evaluator agent so it judges the feature within the epic's architecture. This command's body is the evaluate-feature workflow; it does **not** re-specify those rules.

## Process

### 1. Determine Feature Name

- If `$ARGUMENTS` is provided, use it as the feature name
- If empty, check if a feature is already loaded in the current conversation. If so, use that feature name.
- If still no feature name, list all features in `docs/features/` and ask which one to evaluate
- Verify the feature exists in `docs/features/<feature-name>/`

### 2. Load Acceptance Criteria

Read `docs/features/<feature-name>/implementation.md` and extract:
- The **Definition of Done** section (acceptance criteria)
- The **Verification Method** (how to test)
- The **Executive Summary** (what the feature does)
- The **Goals** and **Success Criteria**

If there is no "Definition of Done" section in the implementation plan:
- Inform the user that no acceptance criteria were defined
- Offer to generate acceptance criteria from the implementation plan's phases and goals
- If user agrees, extract testable criteria from each phase's acceptance criteria and the success criteria, then write a "Definition of Done" section into the implementation plan
- Then proceed with evaluation

### 3. Load Feature State

Read these files for context on what was built:
- `docs/features/<feature-name>/context.md` — Key files, current state
- `docs/features/<feature-name>/tasks.md` — What's been completed

Extract the **Key Files** list from `context.md`. These are the files to inspect.

### 4. Spawn Evaluator Agent

Launch a **fresh agent** (using the Agent tool, NOT doing the evaluation yourself) to perform the evaluation. The agent gets a clean context with no implementation bias.

**Use a high-capability evaluator.** Evaluation is a review-type task, so run it on `model: opus` at **maximum effort** (pass these via the Agent tool's model/effort options if available, otherwise use the project's `*-advanced` agent or any opus-based reviewer agent). Like code review and PR review, this is one of the places that justifies the strongest model — a weak evaluator rubber-stamps weak work. (Only dev *implementation* agents run at lower effort, because the architecture is already settled in the plan.)

**Agent prompt:**

```
You are an independent feature evaluator. Your job is to rigorously verify whether a feature meets its acceptance criteria. You have NO context about how this was built — you are evaluating purely against the defined criteria.

**Your standard is production-grade quality.** Features should feel intentional and polished — not just technically functional. A button that works but has no loading state is not done. An API that returns data but swallows errors is not done. A UI that renders but jumps on load is not done. Hold the work to the standard of software you would trust with your own data.

Be demanding. Do not give credit for partial implementations. Do not assume things work — verify them.

## Feature: <feature-name>

## What It Should Do
<executive summary from implementation.md>

## Acceptance Criteria
<full Definition of Done section from implementation.md>

## Verification Method
<verification method from implementation.md>

## Key Files to Inspect
<key files list from context.md>

## Your Evaluation Process

1. **Read all key source files** — understand what was actually built
2. **Check each functional criterion** — read the code paths that implement each behavior, verify they handle the happy path AND edge cases
3. **Check each quality criterion** — verify error handling, loading states, regressions
4. **Check each integration criterion** — verify data flows and system connections
5. **Follow the verification method** — if it describes manual steps, trace them through the code; if it specifies test commands, run them
6. **If the project has a dev server or tests**, run them to verify functionality works at runtime (use `npm test`, `npm run dev`, etc. as appropriate)
7. **If browser automation tools are available**, use them to interact with the running application and verify UI behaviors directly

## Output Format

Write your evaluation to `docs/features/<feature-name>/evaluation.md` with this structure:

# [Feature Name] - Evaluation Report

**Evaluated:** [date]
**Evaluator:** Independent Agent (Acceptance Criteria Verification)
**Overall Verdict:** PASS | PARTIAL | FAIL

---

## Criteria Results

### Functional Criteria
| # | Criterion | Verdict | Evidence |
|---|-----------|---------|----------|
| F1 | [criterion text] | PASS/FAIL | [what you found — specific file:line, test output, or observed behavior] |
| F2 | ... | ... | ... |

### Quality Criteria
| # | Criterion | Verdict | Evidence |
|---|-----------|---------|----------|
| Q1 | [criterion text] | PASS/FAIL | [evidence] |

### Integration Criteria
| # | Criterion | Verdict | Evidence |
|---|-----------|---------|----------|
| I1 | [criterion text] | PASS/FAIL | [evidence] |

---

## Issues Found

### [ISSUE-1]: [Title]
**Severity:** Critical / Major / Minor
**Criterion:** [Which acceptance criterion this relates to]
**Description:** [What's wrong]
**Location:** [file:line]
**Suggested Fix:** [Concrete suggestion]

---

## Score Summary

- Functional: [X/Y passed]
- Quality: [X/Y passed]
- Integration: [X/Y passed]
- **Overall: [X/Y total] ([percentage]%)**

## Verdict

[1-3 sentences: does this feature meet its definition of done? What must be fixed before it can be considered complete?]

---

## Scoring Rules
- **PASS (100%):** All criteria met
- **PARTIAL (60-99%):** Most criteria met, remaining issues are minor or medium
- **FAIL (<60%):** Critical criteria not met or major gaps

Be honest and rigorous. A PARTIAL verdict with clear issues is more useful than a false PASS.
```

**IMPORTANT:** The evaluator agent must:
- Use the Agent tool with a fresh context (do NOT evaluate in your own context)
- If the feature lives inside an epic, receive a brief **Epic Context** section in its prompt (epic purpose, build order + this feature's position, one-line sibling summaries) so it judges the feature within the epic's architecture
- Read actual source files, not just documentation
- Run tests if available
- Provide specific file:line evidence for every verdict
- Not implement any fixes — only evaluate and document

### 5. Verify Evaluation Was Created

After the agent completes:
- Check that `docs/features/<feature-name>/evaluation.md` exists
- Read the full evaluation report

If evaluation failed:
- Show error to user
- Offer to retry

### 6. Present Results

Display the evaluation summary:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
FEATURE EVALUATION COMPLETE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Feature: <feature-name>
Verdict: [PASS/PARTIAL/FAIL]

Score: [X/Y criteria passed] ([percentage]%)
  Functional:  [X/Y]
  Quality:     [X/Y]
  Integration: [X/Y]

Issues Found: [N]
  Critical: [count]
  Major:    [count]
  Minor:    [count]

Full report: docs/features/<feature-name>/evaluation.md
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 7. Recommend Next Steps

Based on the verdict:

**If PASS:**
- "All acceptance criteria met. Feature is ready for final review or merge."
- Suggest running `/review-feature` for code quality review if not already done

**If PARTIAL:**
- List the failing criteria
- "Run `/proceed` to fix the issues, then re-evaluate with `/evaluate-feature`"
- Update tasks.md with new tasks for each failing criterion

**If FAIL:**
- List critical gaps
- "Significant work remains. Review the evaluation report and update the implementation plan if needed."
- Update tasks.md with new tasks for each failing criterion

## How This Differs From /review-feature

| | /review-feature | /evaluate-feature |
|---|---|---|
| **Focus** | Code quality & best practices | Feature completeness & correctness |
| **Criteria** | Vercel/React patterns, performance | Acceptance criteria from the plan |
| **Perspective** | "Is the code well-written?" | "Does the feature actually work?" |
| **Agent** | Same context | Fresh agent (no implementation bias) |
| **Output** | code-review.md | evaluation.md |

Both are valuable at different stages:
1. `/evaluate-feature` during/after implementation — "does it work?"
2. `/review-feature` before merge — "is the code good?"

## Important Notes

- **DO NOT** implement any fixes during evaluation — only document findings
- **DO** spawn a fresh agent for unbiased evaluation
- **DO** require specific evidence (file:line) for every verdict
- **DO** run tests and interact with the application when possible
- **DO** be rigorous — a false PASS is worse than a harsh PARTIAL
- **DO** update tasks.md with new tasks for failing criteria

## Error Handling

- If no acceptance criteria exist, offer to generate them from the implementation plan
- If key files from context.md don't exist, note which files are missing in the evaluation
- If tests fail to run, document the failure and evaluate based on code inspection
- If the evaluator agent fails, offer to retry or evaluate manually
