---
description: "Review a feature's code against Vercel best practices. Usage: /review-feature [feature-name]"
---

You have been asked to review an existing feature's code against best practices.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and operate across all of the epic's features when the target is an epic (aggregate findings, keep per-feature attribution).
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the review-feature workflow; it does **not** re-specify those rules. When `$ARGUMENTS` resolves to an **epic** (bare name with `epic-overview.md`), follow the **Epic-scope mode** below instead of Step 1's single-feature path; everything else is unchanged. (For an architecture/integration pass across the epic rather than a code pass, use `/review-epic <epic>`.)

### Epic-scope mode (when `$ARGUMENTS` is an epic)

When the resolved target is an epic, review the **code of every child feature** and aggregate the findings into one report — keeping which feature each finding belongs to.

1. **Read the epic.** Read `docs/features/<epic>/epic-overview.md` (Features table = the feature set, in build order) and enumerate the child feature subfolders:
   ```bash
   for f in docs/features/<epic>/*/; do [ -f "$f/context.md" ] && basename "$f"; done
   ```
2. **Run the full Process (Steps 3–5) per feature.** For **each** child feature, load its context (`implementation.md` / `context.md` / `tasks.md`), read its Key Files, and perform the Vercel-rules review exactly as the single-feature flow does. Apply the skills once (Step 4) and reuse them across all features.
3. **Keep per-feature attribution.** Namespace every finding ID with the feature so nothing loses its owner: `base CR-1`, `base CR-2`, `ux CR-1`, … (i.e. `<feature> CR-N`, N restarting per feature). Either group findings under a per-feature `### <feature>` subsection (each with its own CR-N list, What's Already Good, and fix order) or carry the `<feature>` prefix on every CR-N — never a flat CR-N list that drops the feature.
4. **Write one aggregated report** at `docs/features/<epic>/code-review.md` (the epic level), with a per-feature section for each child plus a combined **Recommended Fix Order** sorted by impact/effort *across* the epic (each row labelled with its feature). This complements `/review-epic` (architecture/integration) — it is the deep **code** layer per feature. Do **not** write into child `code-review.md` files in epic mode; the aggregated epic report is the single source so cross-feature fix prioritisation stays in one place.
5. **Status summary** lists totals per feature and the single next recommended fix across the epic (`<feature> CR-N`).

The single-feature path (a plain feature, or `<epic>/<feature>` naming one feature) is unchanged — it follows Steps 1–7 below and writes to `docs/features/$ARGUMENTS/code-review.md` exactly as before.

## Boundary with /review-ui and /review-ux

The three `/review-*` commands target deliberately non-overlapping concerns. Send a finding to the command whose audience would actually read the report it belongs in.

|  | `/review-feature` | `/review-ui` | `/review-ux` |
|---|---|---|---|
| **Asks** | Is the **code** well-written? | Does the **implementation** work correctly? | Does the **experience** feel right to a real user? |
| **Domain** | React / JS patterns, bundle, re-renders, composition, hook discipline | CSS, layout, design tokens, contrast, component-for-state routing, server-vs-visual fidelity | User journeys, cognitive load, focus order, microinteractions, empty/error/interrupted states |
| **Static or Live** | Static only (code-as-text) | Static + optional Live MCP | Static + optional Live MCP |
| **Chrome MCP** | Never | `/review-ui live` -- verify component routing, computed styles, server response vs visual | `/review-ux live` -- verify focus order, interaction timing, rendered text in bad-path states |
| **Example bug** | Barrel import dragging 200KB of unused module; useState derived in an effect; missing React.memo on a hot list row | Wrong properties panel rendered for a widget state; CSS variable cascade leak hiding text; preview endpoint silently skipping a transform the sender runs | 6-tap flow that should be 2 with smart defaults; "Something went wrong" error that says nothing actionable; broken keyboard Tab order across a modal |
| **Findings ID** | CR-N | (project convention) | UX-N (dimension) / F-N (friction) |
| **Audience** | Engineer reviewing PRs | Engineer + designer triaging visual quality | Designer + PM + end-user advocate |

Rule of thumb:
- Pattern that violates a Vercel React best practice -> `/review-feature`
- Pattern that produces a CSS / wiring / fidelity bug -> `/review-ui`
- Pattern that produces a worse experience for the persona -> `/review-ux`

If the same finding plausibly fits two commands, ask: **which report would I send it to?** Engineer PR review = CR. Visual-quality triage = UI. Designer / PM strategy review = UX.

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty, check if a feature is already loaded in the current conversation (via `/continue-feature` or `/start-feature`). If so, use that feature name automatically.
   - If still no feature name, list all features in `docs/features/` and ask user which one to review
   - Verify the feature exists in `docs/features/<feature-name>/`

2. **Check for Existing Review:**
   - Check if `docs/features/$ARGUMENTS/code-review.md` already exists
   - **If it EXISTS:** Read it, load it into context, then skip to step 6 (Status Summary)
   - **If it does NOT exist:** Continue to step 3 (Perform Review)

3. **Load Feature Context:**
   Read these files to understand the feature:
   - `docs/features/$ARGUMENTS/implementation.md` - What was planned
   - `docs/features/$ARGUMENTS/context.md` - Key files, decisions, current state
   - `docs/features/$ARGUMENTS/tasks.md` - What's been implemented

   From `context.md`, extract the **Key Files** list. These are the files to review.

4. **Load Skills and Source Code:**
   - Load the `vercel-react-best-practices` skill (use the Skill tool)
   - Load the `vercel-composition-patterns` skill (use the Skill tool)
   - Read the full compiled guides: `~/.claude/skills/vercel-react-best-practices/AGENTS.md` and `~/.claude/skills/vercel-composition-patterns/AGENTS.md`
   - Read ALL key source files listed in `context.md` "Key Files" section
   - Also read any shared types, index/barrel files, and parent components referenced by those files

5. **Perform Review:**
   Analyze every source file against the Vercel rules, organized by priority:

   **CRITICAL (check first):**
   - `bundle-barrel-imports` - Are there barrel file imports that load unnecessary modules?
   - `bundle-conditional` - Are heavy components loaded unconditionally?
   - `bundle-dynamic-imports` - Should any components be dynamically imported?
   - `async-parallel` - Are there sequential awaits that could be parallel?

   **HIGH (composition):**
   - `architecture-avoid-boolean-props` - Are there components with boolean prop proliferation?
   - `architecture-compound-components` - Should complex components use compound pattern?
   - `state-decouple-implementation` - Is UI tightly coupled to state implementation?
   - `state-context-interface` - Would a context interface improve reusability?
   - `patterns-explicit-variants` - Should boolean modes become explicit variant components?

   **MEDIUM (re-renders):**
   - `rerender-derived-state` - Are store subscriptions too broad?
   - `rerender-defer-reads` - Are state reads happening in callbacks that don't need them?
   - `rerender-memo-with-default-value` - Do inline arrows/objects defeat React.memo?
   - `rerender-derived-state-no-effect` - Is state being derived in effects instead of render?
   - `rerender-simple-expression-in-memo` - Are trivial expressions wrapped in useMemo?
   - `rerender-functional-setstate` - Should callbacks use functional setState?

   **MEDIUM (rendering):**
   - `rendering-hoist-jsx` - Can static JSX be hoisted outside components?
   - `rendering-conditional-render` - Are `&&` conditionals risking rendering `0` or `""`?
   - `rendering-animate-svg-wrapper` - Are SVGs animated directly instead of via wrappers?

   **MEDIUM (React 19):**
   - `react19-no-forwardref` - Is forwardRef used when ref could be a regular prop?
   - `react19-no-forwardref` - Is useContext used where use() would be better?

   **LOW-MEDIUM (JS performance):**
   - `js-hoist-regexp` - Are RegExp objects created inside functions/loops?
   - `js-combine-iterations` - Can multiple array passes be combined?
   - `js-set-map-lookups` - Should arrays be replaced with Set/Map for lookups?

   For each finding, document:
   - **Severity** (CRITICAL/HIGH/MEDIUM/LOW-MEDIUM/LOW)
   - **Rule** (the Vercel rule ID)
   - **File** (exact file path and line numbers)
   - **Problem** (what's wrong, with code snippet)
   - **Fix** (concrete code showing the solution)
   - **Effort** (Low/Medium/High)

   Also document what's already good - patterns that align with best practices.

   Create a **Recommended Fix Order** table sorted by impact-to-effort ratio.

   Create a **Progress** checklist with all findings.

6. **Write or Display Results:**

   **If creating new review:** Write findings to `docs/features/$ARGUMENTS/code-review.md` using this structure:
   ```markdown
   # [Feature Name] - Code Review

   **Reviewed:** [date]
   **Reviewer:** Claude (Vercel React Best Practices + Composition Patterns)
   **Skills Applied:** `vercel-react-best-practices`, `vercel-composition-patterns`
   **Status:** Open - [N] findings, [M] resolved

   ---

   ## Summary
   [Brief overview, finding counts by severity]

   ## Findings
   ### CR-1: [Title]
   **Severity:** [CRITICAL/HIGH/MEDIUM/LOW]
   **Rule:** `[rule-id]`
   **File:** `[path:lines]`
   **Problem:** [description + code]
   **Fix:** [description + code]
   **Effort:** [Low/Medium/High]

   [... more findings ...]

   ## What's Already Good
   [List patterns that align with best practices]

   ## Recommended Fix Order
   [Table sorted by impact/effort ratio]

   ## Progress
   - [ ] CR-1: [title]
   - [ ] CR-2: [title]
   [...]
   ```

   **If review already existed:** Display status summary.

7. **Display Status Summary:**
   ```
   Code Review: $ARGUMENTS
   Findings: [X] total ([Y] resolved, [Z] remaining)

   CRITICAL: [count]
   HIGH: [count]
   MEDIUM: [count]
   LOW: [count]

   Next recommended fix:
   [CR-N]: [title] ([severity], [effort] effort)
   [Brief description of what to do]

   Run /proceed to implement the next fix, or tell me which finding to tackle.
   ```

## Important Notes

- **DO NOT** implement any fixes during the review - only document findings
- **DO** read the actual source code, not just the plan documents
- **DO** provide concrete code examples for both the problem and fix
- **DO** check ALL key files from context.md, not just the main component
- **DO** also note what's already done well
- **DO** number findings as CR-1, CR-2, etc. for easy reference
- **DO** include a progress checklist at the bottom for tracking

## Error Handling

- If `$ARGUMENTS` is empty, list all features in `docs/features/` and ask user to select
- If feature doesn't exist, list available features
- If context.md is missing (no key files list), read the implementation.md and scan the codebase for relevant files
- If skills are not installed, inform user they need the `vercel-react-best-practices` and `vercel-composition-patterns` skills
