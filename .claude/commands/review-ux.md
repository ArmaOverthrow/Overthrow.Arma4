You have been asked to run a comprehensive UX quality review on the application.

**Target scope:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and operate across all of the epic's features when the target is an epic (aggregate findings, keep per-feature attribution).
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the review-ux workflow; it does **not** re-specify those rules. When `$ARGUMENTS` resolves to an **epic** (bare name with `epic-overview.md`), follow the **Epic-scope mode** below; everything else (the dimensions, scoring, friction inventory, live audit) is unchanged.

### Epic-scope mode (when `$ARGUMENTS` is an epic)

An epic is a set of related features that ship as one experience, so treat **the epic's features as the journey set** and review the **combined** experience — not one screen.

1. **Read the epic.** Read `docs/features/<epic>/epic-overview.md` (Features table + build order = the surfaces in scope) before defining the persona, so the persona and journeys span the whole epic, not a single feature.
2. **One persona for the epic** (Step 1): a user moving *through* the epic's features end to end. Infer it from the epic's purpose + the constituent features.
3. **Journeys cross features** (Step 2): the top journeys are the **cross-feature flows** the epic enables (e.g. `base → ux → realtime`). Cross-feature transitions (Dimension 10) and consistency across features (Dimension 2/4) become the highest-value findings — they are exactly what a single-feature review can't see.
4. **Run the 10 dimensions over the combined surface** (Step 3), then keep **per-feature attribution** on every finding: prefix each UX-N / F-N with the feature it lands on — `base UX-1`, `ux UX-1`, `realtime F-001` (or group findings under a per-feature subsection). A finding that spans features (a janky `base → ux` handoff) names both. Never a flat UX-N list that loses which feature it belongs to.
5. **One combined scorecard** (Step 8) for the epic, and note per-feature scores where they diverge (feature X drags Task Flow down). Present and **STOP for approval** exactly as the single-scope flow.

The single-feature / single-screen / `journey:<name>` / `live` paths (a plain feature, a slash-named feature, or any non-epic scope) are unchanged — see Modes below.

## What This Reviews (separate from /review-ui)

`/review-ui` checks **implementation quality** (CSS, tokens, contrast, layout bugs, live browser audit).
`/review-ux` evaluates **experience quality** (flows, cognitive load, heuristics, journey optimisation).

## Modes

- If `$ARGUMENTS` is empty -> review ALL screens and key user journeys
- If `$ARGUMENTS` names a feature or screen -> focus review on that area
- If `$ARGUMENTS` is `journey:<name>` -> deep-dive a specific user journey (e.g. `journey:checkout`)
- If `$ARGUMENTS` is `live` -> add the **Live UX Audit** (Chrome MCP) pass on top of the static review (see section below). Requires a running app.

## 10 UX Review Dimensions

| # | Dimension | What It Evaluates |
|---|-----------|-------------------|
| 1 | **Task Flow Analysis** | Steps to complete key workflows. Count taps/clicks from intent to completion. Identify dead ends, unnecessary steps, and friction points. Map the critical path and compare to the ideal minimum. |
| 2 | **Nielsen's 10 Heuristics** | Systematic evaluation against Jakob Nielsen's usability heuristics: visibility of system status, match between system and real world, user control and freedom, consistency and standards, error prevention, recognition over recall, flexibility and efficiency of use, aesthetic and minimalist design, help users recognise/diagnose/recover from errors, help and documentation. |
| 3 | **Cognitive Load Audit** | Decision count per screen (Hick's Law -- more options = slower decisions). Working memory demands (Miller's Law -- 7+/-2 chunks). Progressive disclosure (show only what's needed now). Jargon and mental model alignment. |
| 4 | **Information Architecture** | Navigation depth (can users reach any feature in <=3 taps?). Content grouping logic. Label clarity and consistency. Findability -- can a new user locate feature X without training? Breadcrumbs and wayfinding. |
| 5 | **Visual Hierarchy & Gestalt** | Proximity grouping, similarity patterns, focal points. F-pattern and Z-pattern reading flow. CTA prominence -- is the primary action the most visually dominant element? Whitespace balance. Typography hierarchy (title -> subtitle -> body -> caption). |
| 6 | **Microinteraction Quality** | Feedback loops -- does every action produce visible feedback? State transitions -- loading, success, error, empty. Skeleton screens vs spinners. Optimistic UI where appropriate. Animation purpose (functional vs decorative). Toast/notification clarity. |
| 7 | **Error Prevention & Recovery** | Destructive action guards (confirm dialogs, undo). Inline validation timing (real-time vs on-submit). Error message quality (what went wrong + how to fix it). Constraint-based prevention (disabled buttons, input masks, min/max). Recovery paths -- can users get back to a good state? |
| 8 | **Contextual Relevance** | Right information at the right time. Adaptive UI based on state/role/context. Smart defaults that reduce input. Contextual actions -- showing relevant actions based on current data state. Reducing cognitive switching between screens. |
| 9 | **Efficiency & Power Users** | Keyboard shortcuts, bulk actions, quick filters. Frequently-used actions within 1 tap of the main view. Search and command patterns. Customisation and personalisation. Remembered preferences and last-used states. Keyboard-only navigation (Tab order, focus management, skip links). |
| 10 | **Journey Optimisation** | End-to-end workflow analysis for the top 5 user journeys. Identify the "happy path" and measure deviation. Shortcut opportunities (can we skip steps for common cases?). Cross-feature transitions -- how smooth is moving between related features? Onboarding and first-use experience. |

## Scoring Framework

Each dimension is scored 1-5. **Use the per-dimension rubrics below** for concrete anchors -- "3 = Adequate" is not enough on its own to keep scores consistent across sessions.

| Score | Meaning |
|-------|---------|
| 5 | **Excellent** -- Best practice, delightful, nothing to improve |
| 4 | **Good** -- Solid UX, minor polish opportunities |
| 3 | **Adequate** -- Functional but noticeable friction or inconsistency |
| 2 | **Poor** -- Significant usability issues, users will struggle |
| 1 | **Failing** -- Broken flow, users cannot complete tasks |

**Overall UX Score** = average of all 10 dimensions (out of 5.0).

### Per-Dimension Rubric (concrete anchors)

| Dim | 5 (Excellent) | 3 (Adequate) | 1 (Failing) |
|-----|---------------|--------------|-------------|
| 1 Task Flow | Common task <=3 taps from home, zero dead ends | Common task 4-7 taps, 1-2 redundant screens | Common task >10 taps OR has an unrecoverable dead end |
| 2 Nielsen | <=1 heuristic violated, none CRITICAL | 3-5 heuristics violated, none CRITICAL | >=2 CRITICAL violations (e.g. no error recovery, no system status) |
| 3 Cognitive Load | <=5 decisions per screen, progressive disclosure used everywhere | 6-9 decisions, some progressive disclosure | >12 decisions per screen, dense form walls, no progressive disclosure |
| 4 Info Arch | Every feature in <=3 taps, labels match user mental model | Most features in 3-4 taps, 1-2 ambiguous labels | Multiple features require >4 taps OR labels actively mislead |
| 5 Visual Hierarchy | Primary CTA unmistakable on every screen, clear reading flow | Primary CTA identifiable, occasional visual ambiguity | Primary CTA unclear OR equal weight given to destructive + safe actions |
| 6 Microinteractions | Every action acknowledged <100ms, every state designed | Most actions acknowledged, some loading/empty states missing | Silent mutations, blank loading, blank empty states |
| 7 Error Prevention | Constraints prevent invalid input, undo for destructive, error messages name the fix | Validation on submit, generic error messages, "Are you sure?" guards on destructive | Free-text where constraints would help, "Something went wrong" errors, no undo |
| 8 Contextual Relevance | Smart defaults, context bar persists across navigation, role-appropriate UI | Some smart defaults, occasional re-asking of known data | Re-asks the same data repeatedly, no role-awareness, no smart defaults |
| 9 Efficiency | Power-user shortcuts (keyboard, bulk), search where lists >20, full keyboard nav | Some shortcuts, basic search, partial keyboard nav | No keyboard shortcuts, no search on long lists, keyboard nav broken |
| 10 Journey Opt | Happy path 1-tap from home, smooth cross-feature transitions, onboarding present | Happy path 2-3 taps, cross-feature transitions OK | Happy path requires navigating multiple unrelated screens, no onboarding |

## CRITICAL RULE: Report First, Fix Later

**NEVER auto-fix without explicit user approval.**

Process:
1. Run all checks
2. Collect and analyse findings
3. Present findings summary with scores
4. **STOP and ask the user** which findings to address
5. Only implement changes the user explicitly approves

## Process

### 1. Define the Persona + Jobs-to-Be-Done (mandatory, FIRST STEP)

Before doing anything else, write down **who uses this and why**. UX scores drift wildly without a specific persona -- a "5/5 Task Flow" for a power user is a "2/5 Task Flow" for a first-time user, and vice versa.

For each scope, produce a 4-line persona block:

```
Persona: <role>, <experience level>, <usage frequency>
Context: <where + when they use this> (e.g. "at a desk, low interruption", "on mobile mid-task, frequent interruptions")
Jobs-to-Be-Done: <the 2-3 outcomes they hire this feature to deliver>
Constraints: <what limits them> (e.g. gloves, low bandwidth, screen reader, untrained)
```

If the user supplied the scope (`/review-ux <feature>`), infer the persona from the codebase (e.g. an admin-only route implies an internal operator) and call it out so the user can correct it. If you can't infer one with confidence, ask.

Persona drives every subsequent step. A finding that wouldn't matter to that persona doesn't go in the report.

### 2. Identify Key User Journeys

Read the codebase structure -- routes, navigation tree, menu definitions, marketing pages. Produce the top 5 journeys for the chosen persona, prioritised by:

1. **Frequency** -- how often does the persona do this? (Daily = top of list)
2. **Stakes** -- what does the persona lose if it goes wrong? (Lost data > minor confusion)
3. **Recency of pain** -- has the user complained about this surface recently? (Check git log + open issues if available)

If you can't determine frequency/stakes from the codebase alone, name your assumption explicitly so the user can correct it.

### 3. Run Each Dimension

For each of the 10 dimensions:

**Read the actual screens and components** -- don't guess from file names. Open each screen file, trace the user flow, count interactions, check feedback patterns.

**Trigger non-happy-path states too.** Empty states, error states, loading states, and "user got interrupted mid-flow" are where most UX dies. For each key screen:
- Find what triggers the empty state -- read the screen file or hit the API with empty data
- Find what triggers each error path -- read the catch blocks / error UI
- Check what happens if the user backgrounds the app / navigates away / refreshes mid-flow (form draft persistence? URL state? local storage?)

**Apply established frameworks:**

**Task Flow (Dimension 1):**
- For each key journey: list every screen/modal/sheet the user must interact with
- Count: taps to complete, fields to fill, decisions to make
- Ideal: most common tasks in <=3 taps from home
- Flag: any journey requiring >5 screens or >10 taps

**Nielsen's Heuristics (Dimension 2):**
- H1 Visibility: Does every action show immediate feedback? Loading states? Progress indicators?
- H2 Real World Match: Does terminology match users' mental model? Natural language, not system jargon?
- H3 User Control: Can users undo? Go back? Cancel mid-flow? Exit without losing work?
- H4 Consistency: Same action = same result everywhere? Consistent button placement/styling?
- H5 Error Prevention: Are dangerous actions guarded? Constraints prevent invalid input?
- H6 Recognition: Are options visible rather than requiring memory? Labels on icons?
- H7 Flexibility: Shortcuts for experts? Works for both novice and power users?
- H8 Aesthetic: Is every element necessary? No visual clutter? Clean hierarchy?
- H9 Error Recovery: Clear error messages? Suggestions for resolution? Easy to retry?
- H10 Help: Is the UI self-explanatory? Tooltips where needed? Onboarding for complex features?

**Cognitive Load (Dimension 3):**
- Count visible options per screen (>7 = high load)
- Check if complex forms use progressive disclosure (multi-step vs one long form)
- Identify screens where users need to remember info from a previous screen
- Flag jargon or technical terms without explanation

**Information Architecture (Dimension 4):**
- Draw the navigation tree (screen hierarchy)
- Measure depth: home -> deepest screen (should be <=4 levels)
- Check if related features are grouped logically
- Test: "Where would a new user look for [feature X]?"

**Visual Hierarchy (Dimension 5):**
- For each screen: what's the first thing the eye is drawn to? Is it the right thing?
- Are primary CTAs visually dominant (colour, size, position)?
- Is there clear visual separation between sections?
- Does the layout guide the eye in a logical reading order?

**Microinteractions (Dimension 6):**
- Every button press: does something visible happen immediately?
- Loading: skeleton, spinner, or nothing?
- Success: toast, animation, navigation, or nothing?
- **Empty states**: helpful message with action, or blank? (Trigger them -- don't assume)
- **Error states**: specific message + recovery action, or generic? (Trigger them -- don't assume)
- Transitions: smooth or jarring?

**Error Prevention (Dimension 7):**
- Destructive actions (delete, cancel): confirmation dialog?
- Form inputs: validated in real-time or only on submit?
- Error messages: generic ("Error occurred") or specific ("Volume must be between 0.5 and 200L")?
- Can users recover from mistakes without starting over?

**Contextual Relevance (Dimension 8):**
- Do screens show different content/actions based on state?
- Are defaults smart (pre-filled from context, last-used values)?
- Are irrelevant actions hidden or disabled with explanation?
- Does the UI adapt to what the user is currently doing?

**Efficiency (Dimension 9):**
- Most common action: how many taps from the main screen?
- Are there quick-action shortcuts (FABs, swipe actions, long-press)?
- Does search exist where lists are long?
- Are filters/sort remembered between sessions?
- **Keyboard-only walkthrough** (desktop apps): Tab through the primary flow without touching the mouse. Does focus land on the right element next? Is the focus ring visible? Can you submit forms with Enter? Cancel with Esc? Is there a "skip to content" link?

**Journey Optimisation (Dimension 10):**
- Trace each key journey end-to-end
- Identify: where do users get stuck? Where do they abandon?
- Can any steps be eliminated or combined?
- Are cross-feature transitions smooth (e.g. shopping cart -> checkout -> confirmation)?

### 4. Build the Friction Inventory (separate output)

In parallel with the dimension scoring, keep a **flat numbered list** of every 1-second papercut you find. No grouping by dimension, no scoring, no severity. Just:

```
F-001: <screen> -- <what makes the operator sigh> (<rough cost: extra taps x frequency>)
F-002: <screen> -- ...
```

Examples of papercuts to capture: extra confirmation dialog where none was needed, scroll-to-find on a small list, tap-then-scroll-then-tap, missing default, modal animation that runs every time even when the operator already knows it, repeated authentication, re-entering data the system already has.

This is operationally useful for stand-up triage -- engineers can pick a papercut off this list during slack time even if the dimension scores haven't moved.

### 5. Classify Findings

For each dimension issue found, number it **UX-N** (matches the CR-N convention in /review-feature so findings can be referenced across reviews and PRs):

- **CRITICAL UX**: Users cannot complete a core task, or will abandon the flow
- **HIGH UX**: Significant friction -- users will struggle or need help
- **MEDIUM UX**: Noticeable inconvenience -- works but feels clunky
- **LOW UX**: Polish -- works fine but could be smoother
- **OPPORTUNITY**: Not a problem, but a chance to delight

Friction inventory items stay as F-N (separate from UX-N) so they can be triaged independently.

### 6. Find at Least 3 Positives (mandatory)

UX reports that contain only criticism erode the dev/designer relationship and miss the chance to reinforce patterns worth spreading. Before writing the summary, identify at least 3 things the team is doing well, with reasons. Examples:

- "The empty state on /orders gives users their next action ('Place your first order' with a CTA), not just a sad face emoji."
- "Inline validation on email fields fires on blur, not on every keystroke -- correct timing, low annoyance."
- "The destructive 'Delete account' flow requires typing the username -- friction matched to stakes."

If you genuinely can't find 3 positives, say so honestly and explain why -- but the search is mandatory.

### 7. When Proposing Improvements, Cite Domain Analogues

Abstract suggestions ("improve discoverability of the export option") rarely land. Concrete references to products solving the same problem ("Linear's command palette pattern -- Cmd+K from anywhere -> typeahead to any action") give the implementer a starting point. When you propose a non-trivial change:

- Name 1-2 products that already solve the same problem
- Describe the relevant pattern in one sentence (not the whole product)
- Note any constraints that mean the project shouldn't copy 1:1

Don't reach for analogues for tiny tweaks ("the label should say Email Address not Email"). Reserve them for changes that are structural enough to need a model.

### 8. Present Findings Summary

```
## /review-ux Findings Summary

### Persona
<the 4-line persona block from Step 1>

### UX Scorecard

| # | Dimension | Score | Key Finding |
|---|-----------|-------|-------------|
| 1 | Task Flow | X/5 | [one-line summary] |
| 2 | Nielsen Heuristics | X/5 | [one-line summary] |
| 3 | Cognitive Load | X/5 | [one-line summary] |
| 4 | Information Architecture | X/5 | [one-line summary] |
| 5 | Visual Hierarchy | X/5 | [one-line summary] |
| 6 | Microinteractions | X/5 | [one-line summary] |
| 7 | Error Prevention | X/5 | [one-line summary] |
| 8 | Contextual Relevance | X/5 | [one-line summary] |
| 9 | Efficiency | X/5 | [one-line summary] |
| 10 | Journey Optimisation | X/5 | [one-line summary] |

**Overall UX Score: X.X / 5.0**

### Critical UX Issues (must address)
- **UX-1**: <description> -- file:line, impact on persona, suggested improvement, [analogue if structural]
- **UX-2**: ...

### High UX Issues (should address)
- **UX-N**: ...

### Quick Wins (low effort, high impact)
- **UX-N**: <what to do> -- <expected improvement>

### Friction Inventory (separate flat list)
- **F-001**: <screen> -- <papercut>
- **F-002**: ...

### What's Already Good (mandatory, >=3)
- <pattern> -- why it works for this persona

### UX Debt Backlog
| Priority | ID | Issue | Dimension | Effort | Impact |
|----------|----|----- -|-----------|--------|--------|
| P1 | UX-N | ... | ... | Low/Med/High | Low/Med/High |

---
**What would you like to address?** Options:
- "fix critical" -- address critical UX issues
- "fix quick wins" -- implement low-effort improvements
- "fix friction F-N, F-N" -- knock out specific papercuts
- "fix UX-N" -- address a specific finding
- "fix [specific dimension]" -- focus on one area
- "plan redesign" -- create a UX improvement plan for the backlog
- "none" -- review only
```

### 9. Wait for User Response

Do NOT proceed with any changes until the user explicitly tells you what to address.

---

## Live UX Audit (Chrome MCP, optional)

`/review-ux live` runs the dimension review AND an interactive verification pass against a running app via the Chrome MCP. Use it when the question isn't "how is this designed?" but "how does this actually feel when I do it?".

### How this differs from /review-feature and /review-ui

The three `/review-*` commands target deliberately non-overlapping concerns. Send a finding to the command whose audience would actually read the report it belongs in.

|  | `/review-feature` | `/review-ui` | `/review-ux` |
|---|---|---|---|
| **Asks** | Is the **code** well-written? | Does the **implementation** work correctly? | Does the **experience** feel right to a real user? |
| **Domain** | React / JS patterns, bundle, re-renders, composition, hook discipline | CSS, layout, design tokens, contrast, component-for-state routing, server-vs-visual fidelity | User journeys, cognitive load, focus order, microinteractions, empty/error/interrupted states |
| **Static or Live** | Static only (code-as-text) | Static + optional Live MCP | Static + optional Live MCP |
| **Chrome MCP** | Never | `/review-ui live` -- verify component routing, computed styles, server response vs visual | `/review-ux live` -- verify focus order, interaction timing, rendered text in bad-path states |
| **Example bug** | Barrel import dragging in 200KB of unused module; useState derived in an effect; missing React.memo on a hot list row | Wrong properties panel rendered for a widget state; CSS variable cascade leak hiding text; preview endpoint silently skipping a transform the sender runs | 6-tap flow that should be 2 with smart defaults; "Something went wrong" error that says nothing actionable; broken keyboard Tab order across a modal |
| **Findings ID** | CR-N | (project convention) | UX-N (dimension) / F-N (friction) |
| **Audience** | Engineer reviewing PRs | Engineer + designer triaging visual quality | Designer + PM + end-user advocate |

Rule of thumb:
- Pattern that violates a Vercel React best practice -> `/review-feature`
- Pattern that produces a CSS / wiring / fidelity bug -> `/review-ui`
- Pattern that produces a worse experience for the persona -> `/review-ux`

If the same finding plausibly fits two commands, ask: **which report would I send it to?** Engineer PR review = CR. Visual-quality triage = UI. Designer / PM strategy review = UX.

### What live verification catches that code-reading misses

- **Real focus order** vs the order you predicted from the JSX. React portals, modals, and `tabIndex={-1}` shuffles all bend the actual Tab order in ways the source doesn't make obvious.
- **Real interaction latency** vs the assumption that "it'll be fast enough". Doherty's threshold (<400ms) is hard to estimate from code; trivial to measure with `performance.now()` deltas inside a `javascript_tool` call.
- **Real empty / error / loading states** rendered with real data. Mocked data in storybook hides the "what does this look like when the user has 0 items / 1 item / 1,000 items" question.
- **Actual interruption recovery.** Navigate away mid-form, come back. Did the draft survive? Is the user on the same field? You can't read this from the code with confidence; you can prove it in 3 MCP calls.

### Process

#### 1. Acquire a tab + sign in (if needed)

```
tabs_context_mcp({ createIfEmpty: true })
```

If the app needs auth and the user isn't logged in, ask the user to sign in manually in the browser. Do NOT type passwords.

#### 2. Walk each key journey end-to-end

For each of the top journeys identified in Step 2, drive the actual flow:

- Use `browser_batch` to compress click -> wait -> read sequences (one round trip per logical step is ~3-5x faster than sequential)
- Capture **state at each step**: what's selected, what's focused, what's visible, what's announced (ARIA live regions)
- **Time each step**: wrap actions in `performance.now()` to measure actual latency

Example -- measure the time from click-to-feedback for a primary mutation:

```
javascript_tool({ action: 'javascript_exec', tabId, text: `
  (async () => {
    const btn = document.querySelector('[data-testid="primary-cta"]');
    const t0 = performance.now();
    btn.click();
    // Wait for the visible state change you expect (toast, navigation, spinner, etc.)
    await new Promise(r => {
      const start = performance.now();
      const tick = () => {
        const done = document.querySelector('[role="status"], [data-loading="done"]');
        if (done || performance.now() - start > 3000) r();
        else requestAnimationFrame(tick);
      };
      tick();
    });
    return { ms: Math.round(performance.now() - t0) };
  })()
`})
```

Findings: any mutation taking >400ms without an optimistic UI update is a Doherty violation; >1s without a spinner is a Visibility violation.

#### 3. Keyboard-only pass

Drive a complete journey with no mouse:

```
computer({ action: 'key', text: 'Tab', tabId })
// repeat, capturing document.activeElement at each step
```

Verify:
- Focus lands on the first interactive element on page load
- Tab order matches the visual reading order
- Focus ring is visible on every focusable element (`getComputedStyle(document.activeElement).outline` not 'none')
- Enter submits the primary form action; Esc cancels modals; arrows navigate menu items
- No focus traps (you can always Tab/Shift+Tab back out of any region)

#### 4. Trigger error + empty + interrupted states

- Empty: navigate to a list before any data exists (filter to zero results, or POST a request that clears all)
- Error: cause a deliberate error (disable network in DevTools, submit with invalid data) and read the rendered message
- Interrupted: start a flow, navigate away, navigate back. Confirm the user is where they expect to be and no input is lost

Capture the actual rendered text in each state and judge it against the persona ("would this message tell my persona what to do next?").

#### 5. Close the browser

```
tabs_close_mcp({ tabId })
# Then:
pkill -f chrome-devtools-mcp
pkill -f 'chrome.*--remote-debugging'
```

Chrome MCP holds ~1.3 GB of RAM. Open when needed, close immediately after.

### Add a Live Audit section to the report

```
### Live Audit (MCP)
- [PASS/FAIL] Real focus order matches visual order
- [PASS/FAIL] Primary mutations acknowledge <400ms (Doherty)
- [PASS/FAIL] Empty / error / interrupted states verified
- [PASS/FAIL] Keyboard-only completion of the top journey
- **UX-N**: <one-line summary> -- file:line, evidence (timing / focus path / actual rendered message)
```

---

## Reference Frameworks

This review draws from:
- **Jakob Nielsen's 10 Usability Heuristics** (Nielsen Norman Group)
- **Google HEART Framework** (Happiness, Engagement, Adoption, Retention, Task success)
- **Fitts's Law** -- target size and distance affect speed and accuracy
- **Hick's Law** -- decision time increases with number and complexity of choices
- **Miller's Law** -- working memory holds 7+/-2 chunks of information
- **Jakob's Law** -- users expect your app to work like apps they already know
- **Tesler's Law** -- every application has inherent complexity that cannot be removed, only moved
- **Doherty Threshold** -- productivity increases when interactions are <400ms
- **Peak-End Rule** -- users judge experiences by the peak (best/worst) and the end
- **Jobs to Be Done** -- users "hire" features to accomplish specific goals

## Important Notes

- This is a **UX strategy review**, not a code review -- focus on the experience, not the implementation
- Read actual screen files to understand what users see and do
- Trace real user journeys through the navigation tree
- Consider both novice and expert users (but anchor on the chosen persona)
- Consider error cases and edge cases, not just happy paths
- The goal is actionable improvement suggestions, not theoretical critique
- Prioritise findings by impact on user task completion
- Use Live UX Audit (`/review-ux live`) when you need real focus order, real timing, real rendered states -- code-reading can't answer those reliably
