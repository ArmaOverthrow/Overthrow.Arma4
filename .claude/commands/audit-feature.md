---
description: "Audit a feature's architecture, integration, and DX from a meta standpoint. Usage: /audit-feature [feature-name]"
---

You have been asked to audit an existing feature's architecture and integration quality.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic.
If `$ARGUMENTS` is a bare epic name, this command audits one feature at a time — list the epic's child features and ask which to audit.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When the target is a feature inside an epic, every `docs/features/$ARGUMENTS/...` path below is the resolved nested `docs/features/<epic>/<feature>/...` location. Additionally, the cross-feature scan (Step 4) must **descend into epic folders** — an epic directory has no `implementation.md`/`context.md` of its own; its child features do (read each epic's `epic-overview.md` plus its children's docs). This command's body is the audit workflow; it does **not** re-specify the resolution rules.

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty, check if a feature is already loaded in the current conversation (via `/continue-feature` or `/start-feature`). If so, use that feature name automatically.
   - If still no feature name, list all features in `docs/features/` and ask user which one to audit
   - Verify the feature exists in `docs/features/<feature-name>/`

2. **Check for Existing Audit:**
   - Check if `docs/features/$ARGUMENTS/audit.md` already exists
   - **If it EXISTS:** Read it, load it into context, then skip to step 7 (Display Summary)
   - **If it does NOT exist:** Continue to step 3

3. **Load Target Feature Context:**
   Read these files to understand the feature being audited:
   - `docs/features/$ARGUMENTS/implementation.md` - What was planned
   - `docs/features/$ARGUMENTS/context.md` - Key files, decisions, current state
   - `docs/features/$ARGUMENTS/tasks.md` - What's been implemented

   From `context.md`, extract the **Key Files** list. These are the primary files to audit.

4. **Load Cross-Feature Context:**
   Read ALL other features to understand the broader system:
   - List all directories in `docs/features/`. **For any directory that is an epic** (`epic-overview.md` present), do not treat it as a single feature — read its `epic-overview.md`, then descend into its child feature subfolders and treat each child as a feature below.
   - For each feature OTHER than the target:
     - Read `docs/features/<other-feature>/context.md` (key files, decisions)
     - Read `docs/features/<other-feature>/implementation.md` (architecture, approach)
   - This provides the context needed to evaluate integration, duplication, and cross-feature opportunities

5. **Load Skills:**
   - Scan `.claude/skills/` for all installed skills
   - Read every `SKILL.md` file found (these contain project-specific patterns and conventions)
   - Use these as the baseline for evaluating DX, naming, and consistency

6. **Read Source Code and Perform Audit:**

   Read ALL key source files listed in the target feature's `context.md`, plus any shared types, index/barrel files, and parent components referenced by those files.

   Analyze across **6 categories:**

   ### Category 1: Architecture
   Evaluate the feature's internal structure:
   - Component hierarchy and separation of concerns
   - State management approach and data flow
   - Abstraction levels — are they appropriate or over/under-engineered?
   - Single responsibility — do modules have clear, focused purposes?
   - Error handling boundaries

   ### Category 2: Integration
   Evaluate how the feature connects to the rest of the system:
   - Coupling with other features — is it tight or loose?
   - Cohesion — does the feature own its domain or leak into others?
   - Shared dependencies — are they used consistently?
   - Interface contracts — are boundaries well-defined?
   - Event/message patterns — how does it communicate with other features?

   ### Category 3: Duplication & Overlap
   Identify redundancy across the codebase:
   - Shared patterns that could be extracted into utilities or shared modules
   - Repeated logic across this feature and others
   - Similar components that could be unified
   - Duplicate type definitions or constants
   - Opportunities for shared abstractions (only where 3+ consumers exist)

   ### Category 4: Developer Experience
   Evaluate how easy this feature is to work with:
   - API intuitiveness — are interfaces self-documenting?
   - Type safety — are types comprehensive and helpful?
   - Naming conventions — consistent with project patterns?
   - Discoverability — can a new developer find and understand things?
   - Extensibility — how hard is it to add new functionality?
   - Documentation quality — are complex areas explained?

   ### Category 5: Optimization
   Evaluate architectural-level performance (NOT micro-optimizations):
   - Shared state efficiency — are subscriptions appropriately scoped?
   - Lazy loading opportunities — are heavy modules loaded eagerly?
   - API design — are data fetching patterns efficient?
   - Bundle impact — does the feature contribute unnecessary weight?
   - Caching opportunities — is repeated work avoided?

   ### Category 6: Refactoring
   Concrete refactoring suggestions:
   - Each suggestion must include effort estimate (Small/Medium/Large)
   - Each must describe the current state and target state
   - Each must explain the benefit
   - Prioritize by impact-to-effort ratio

   **For each finding, document:**
   - **ID** — `AF-1`, `AF-2`, etc.
   - **Type** — Architecture | Integration | Duplication | DX | Optimization | Refactoring
   - **Severity** — Critical | High | Medium | Low
   - **Title** — Short, descriptive name
   - **Current State** — What exists now, with file paths and code references
   - **Recommendation** — Concrete suggestion for improvement
   - **Impact** — What improves if this is addressed
   - **Effort** — Small | Medium | Large

   **Also document what's already strong** — patterns, decisions, and architecture that are well-done.

7. **Write or Display Results:**

   **If creating new audit:** Write findings to `docs/features/$ARGUMENTS/audit.md` using this structure:

   ```markdown
   # [Feature Name] - Architecture Audit

   **Audited:** [date]
   **Auditor:** Claude (Architecture & Integration Review)
   **Status:** Open — [N] findings

   ---

   ## Executive Summary

   [2-3 sentence overview of the feature's architectural health. Lead with overall assessment, note the most significant findings.]

   ### Finding Counts

   | Type | Critical | High | Medium | Low | Total |
   |------|----------|------|--------|-----|-------|
   | Architecture | | | | | |
   | Integration | | | | | |
   | Duplication | | | | | |
   | DX | | | | | |
   | Optimization | | | | | |
   | Refactoring | | | | | |
   | **Total** | | | | | |

   ---

   ## What's Already Strong

   - [Pattern or decision that is well-done, with file reference]
   - [...]

   ---

   ## Findings

   ### AF-1: [Title]
   **Type:** [Architecture | Integration | Duplication | DX | Optimization | Refactoring]
   **Severity:** [Critical | High | Medium | Low]

   **Current State:**
   [Description of what exists now, with `file:line` references]

   **Recommendation:**
   [Concrete suggestion for improvement]

   **Impact:** [What improves]
   **Effort:** [Small | Medium | Large]

   ---

   ### AF-2: [Title]
   [... more findings ...]

   ---

   ## Cross-Feature Opportunities

   [Improvements that span multiple features. Reference specific features by name and describe the shared opportunity.]

   - **[Opportunity Title]:** [Description, which features are involved, what could be shared or unified]
   - [...]

   ---

   ## Recommended Refactor Order

   | Priority | ID | Title | Severity | Effort | Impact |
   |----------|----|-------|----------|--------|--------|
   | 1 | AF-X | ... | ... | ... | ... |
   | 2 | AF-Y | ... | ... | ... | ... |
   | ... | | | | | |

   ---

   ## Next Steps

   To act on any finding, create a new feature for it:
   - Run `/plan-feature <finding-name>` with the recommendation as requirements
   - Reference this audit finding ID in the new feature's implementation plan

   ---

   *Audit generated by Beast Mode `/audit-feature` command.*
   ```

   **If audit already existed:** Display summary from existing file.

8. **Display Summary:**
   ```
   Architecture Audit: $ARGUMENTS
   Findings: [X] total

   Critical: [count]
   High: [count]
   Medium: [count]
   Low: [count]

   By type:
   Architecture: [count] | Integration: [count] | Duplication: [count]
   DX: [count] | Optimization: [count] | Refactoring: [count]

   Top recommendation:
   [AF-N]: [title] ([severity], [effort] effort)
   [Brief description]

   To act on a finding, run /plan-feature <name> with the recommendation as requirements.
   Full audit: docs/features/$ARGUMENTS/audit.md
   ```

## Important Notes

- **DO NOT** implement any changes during the audit — only document findings
- **DO** read the actual source code, not just the plan documents
- **DO** read ALL other features' context and implementation docs for cross-feature analysis
- **DO** load all installed skills from `.claude/skills/` for convention baseline
- **DO** provide concrete file paths and code references for every finding
- **DO** note what's already well-done, not just problems
- **DO** number findings as AF-1, AF-2, etc. for easy reference
- **DO** include a recommended refactor order sorted by impact-to-effort ratio
- **DO NOT** suggest running `/proceed` — findings become new features via `/plan-feature`
- **DO** keep recommendations concrete and actionable, not vague

## Error Handling

- If `$ARGUMENTS` is empty, list all features in `docs/features/` and ask user to select
- If feature doesn't exist, list available features
- If context.md is missing (no key files list), read the implementation.md and scan the codebase for relevant files
- If no other features exist for cross-feature analysis, note this and focus on internal architecture
- If no skills are installed, note this and skip convention baseline comparison
