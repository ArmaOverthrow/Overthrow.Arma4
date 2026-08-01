# Mission Statement Template

Use this template when creating `docs/mission-statement.md` for a project. Adapt sections to fit the project's scope -- remove sections that don't apply, add sections that do. The goal is a living document that captures *why* this project exists and *what it stands for*.

---

```markdown
# [Project Name]

## Mission Statement

**[One-sentence mission statement -- what does this project do and why does it matter?]**

[2-3 sentences expanding on the mission. What problem does it solve? What gap does it fill? What makes this approach different from alternatives?]

---

## Core Philosophy

### [Philosophy Pillar 1]

[Why does this project exist? What's wrong with the status quo? What does this project do differently?]

### [Philosophy Pillar 2]

[Another core belief that drives design decisions. These should be opinionated -- they help future contributors understand what trade-offs the project favours.]

### [Philosophy Pillar 3 (optional)]

[Additional guiding principle if needed.]

---

## What We're Building

### [High-Level Description]

[What is the system at a high level? Describe the main components or pipeline without getting into technical details. This section answers "what does it actually do?"]

### [Key Differentiator / Target Outcome]

[What makes this different from existing solutions? What outcome are we optimising for?]

---

## Who This Is For

### [User Persona 1]

[Who is the primary user? What do they use this for? How does it help them?]

### [User Persona 2]

[Secondary audience. What's their use case?]

### [User Persona 3 (optional)]

[Additional audience if applicable.]

---

## What [Project Name] Is Not

- **Not [common misconception].** [Clarify what the project does NOT do.]
- **Not [scope boundary].** [Set expectations about what's out of scope.]
- **Not [anti-pattern].** [Distinguish from approaches the project explicitly avoids.]

---

## Design Pillars

1. **[Pillar 1]** -- [One-sentence description of a core design trade-off or principle. E.g., "Accuracy over speed -- we take the slower, more correct approach every time."]

2. **[Pillar 2]** -- [Another design principle that guides implementation decisions.]

3. **[Pillar 3]** -- [Additional principle.]

4. **[Pillar 4 (optional)]** -- [Additional principle.]

5. **[Pillar 5 (optional)]** -- [Additional principle.]

---

## Technical Direction

### [Technology Choice / Approach 1]

[Brief description of the core technical approach and why it was chosen. Keep this high-level -- detailed technical decisions belong in technical-design.md.]

### [Technology Choice / Approach 2]

[Another significant technical direction.]

### [Approach to Quality / Iteration (optional)]

[How does the project approach quality, versioning, or iterative improvement?]

---

*This document is a living guide. It will evolve as the project develops, but the core mission does not change.*
```

---

## Guidelines for Creating a Mission Statement

1. **Start with the one-liner.** If you can't explain the project's purpose in one bold sentence, the scope isn't clear enough.

2. **Be opinionated.** The philosophy section should express strong opinions about trade-offs. "We prioritise X over Y" is better than "we try to balance X and Y."

3. **Name the audience.** If you can't name who this is for, you don't know what you're building.

4. **Set boundaries.** The "What This Is Not" section prevents scope creep and misaligned expectations.

5. **Keep it high-level.** Technical details belong in `technical-design.md`. The mission statement is about *why* and *what*, not *how*.

6. **For existing projects:** Read the codebase, README, and any existing docs to extract the mission. Ask the user to confirm or adjust.

7. **For new projects:** Ask the user about their goals, audience, and what makes this different. Then draft the mission statement for their review.
