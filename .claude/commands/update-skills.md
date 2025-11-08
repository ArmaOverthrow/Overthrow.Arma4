# /update-skills - Update Skills System After Feature Completion

You have been asked to update the skills system after completing a feature.

**Feature name:** `{feature-name}`

## Purpose

After completing a feature, ensure it's properly documented in the skills system so future agents can leverage the new patterns, components, and approaches. This creates a self-improving knowledge base.

## Process

### 1. Load Feature Context

Read the feature's dev docs:
- `dev/active/{feature-name}/plan.md` - What was planned
- `dev/active/{feature-name}/context.md` - Key decisions and files
- `dev/active/{feature-name}/tasks.md` - What was completed

If feature is in `dev/completed/`, read from there instead.

### 2. Analyze Implementation

Review the key files listed in context.md to understand:
- **What patterns were used?** (React patterns, state management, API design, etc.)
- **What components were created?** (Reusable UI, services, utilities, etc.)
- **What architectural decisions were made?** (Module structure, data flow, etc.)
- **What gotchas or learnings emerged?** (From context.md "Gotchas & Learnings")

### 3. Determine Relevant Skills

Based on the analysis, identify which existing skills should be updated:

**Common mappings:**
- React components, hooks, TypeScript → `.claude/skills/frontend-dev-guidelines/`
- Editor modules, viewport transforms, EventBus → `.claude/skills/editor-v3-module-dev/`
- API patterns, backend integration → `.claude/skills/api-patterns/` (if exists)
- State management patterns → `.claude/skills/state-management/` (if exists)

**Check existing skills:**
```bash
ls -la .claude/skills/
```

For each relevant skill, determine what should be documented.

### 4. Update Each Relevant Skill

For each skill that should be updated:

#### 4a. Create Feature Documentation File

Create a new file in the skill's directory:
`.claude/skills/{skill-name}/features/{feature-name}.md`

**Template:**
```markdown
# {Feature Name}

**Related Feature:** `{feature-name}`
**Implemented:** YYYY-MM-DD
**Status:** ✅ Complete / 🚧 In Progress

## Overview

[1-2 sentence description of what the feature does]

## Key Patterns

### Pattern 1: [Pattern Name]

**When to use:** [Describe the use case]

**Example:**
\`\`\`typescript
// Code example showing the pattern
\`\`\`

**Why this approach:** [Explain the rationale]

### Pattern 2: [Pattern Name]

...

## Key Components/Services

### ComponentName / ServiceName

**Location:** `path/to/file.ts:line`

**Purpose:** [What it does]

**Usage:**
\`\`\`typescript
// Example usage
\`\`\`

## Gotchas & Learnings

- **Gotcha:** [Description]
  - **Solution:** [How to avoid/fix]

- **Learning:** [Key insight]
  - **Application:** [When to apply this learning]

## Related Features

- `{related-feature-1}` - [Brief description of relationship]
- `{related-feature-2}` - [Brief description of relationship]

## See Also

- [Link to main implementation file]
- [Link to related skill documentation]
```

#### 4b. Update Skill's Main SKILL.md

Read `.claude/skills/{skill-name}/SKILL.md` and add a reference to the new feature:

**Add to relevant section:**
```markdown
## Feature: {Feature Name}

[1-2 sentence overview]

**Detailed docs:** See `features/{feature-name}.md`

**Quick reference:**
- Key pattern: [Brief description]
- Main component: `path/to/file.ts`
- When to use: [Brief guidance]
```

Use the Edit tool to add this reference in the appropriate section.

### 5. Update skill-rules.json

Read `.claude/skill-rules.json` to see if new activation rules are needed.

**Consider adding rules for:**
- New file patterns (e.g., `src/apps/Editor/modules/MyModule/**`)
- New keywords in prompts (e.g., "inspector panel", "layer tree")
- New component types (e.g., "BoxDimensionField")

**Example addition:**
```json
{
  "skillName": "frontend-dev-guidelines",
  "triggers": {
    "keywords": ["existing keywords...", "new feature keyword"],
    "filePaths": ["existing patterns...", "src/path/to/new/feature/**"]
  }
}
```

Use the Edit tool to add new rules if needed.

### 6. Summary Report

Provide a comprehensive summary:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📚 SKILLS UPDATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Feature: {feature-name}

✅ Created Documentation:
   • .claude/skills/{skill-1}/features/{feature-name}.md
   • .claude/skills/{skill-2}/features/{feature-name}.md

✅ Updated Skills:
   • {skill-1} - Added {feature-name} patterns
   • {skill-2} - Added {feature-name} components

✅ Updated skill-rules.json:
   • Added keywords: {keyword1}, {keyword2}
   • Added file patterns: {pattern1}

📝 Key Patterns Documented:
   1. {Pattern name} - {Brief description}
   2. {Pattern name} - {Brief description}
   3. {Pattern name} - {Brief description}

🎯 Future agents will now know about:
   • {Key capability 1}
   • {Key capability 2}
   • {Key capability 3}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

💡 These patterns will auto-activate when relevant files/keywords are detected!
```

## Guidelines

### What to Document

**DO document:**
- ✅ Reusable patterns that will appear in other features
- ✅ Architectural decisions that should be consistent
- ✅ Component APIs and usage patterns
- ✅ Gotchas and non-obvious solutions
- ✅ Performance considerations
- ✅ Testing approaches

**DON'T document:**
- ❌ Implementation details that won't be reused
- ❌ Obvious patterns already well-documented
- ❌ Experimental code that might be refactored
- ❌ Project-specific business logic

### Writing Style

- **Be concise** - Future agents need quick reference, not novels
- **Show examples** - Code examples are worth 1000 words
- **Explain "why"** - Rationale is more important than "what"
- **Link to code** - Always include file paths with line numbers
- **Highlight gotchas** - Save future agents from the same mistakes

### Skill Organization

**Create new skill if:**
- Feature introduces entirely new domain (e.g., "data-visualization")
- Patterns don't fit existing skills
- Would make existing skill too large/unfocused

**Update existing skill if:**
- Feature extends existing patterns
- Fits naturally into existing skill's scope
- Related to skill's core domain

## Error Handling

- If `{feature-name}` doesn't exist in `dev/active/` or `dev/completed/`:
  - List available features
  - Ask user to specify correct feature name

- If no existing skills found in `.claude/skills/`:
  - Offer to create initial skill structure
  - Ask user what skills they want to create

- If feature is too small/simple to warrant documentation:
  - Inform user
  - Ask if they still want to proceed

## Examples

### Example 1: Frontend Feature

```
User: /update-skills layer-inspector

Claude: I'll update the skills system with the layer-inspector feature.

[Reads dev docs, analyzes implementation]

Creating documentation:
• .claude/skills/frontend-dev-guidelines/features/layer-inspector.md
  - Documented recursive tree rendering pattern
  - Documented virtualization with react-window
  - Documented drag-and-drop layer reordering

• .claude/skills/editor-v3-module-dev/features/layer-inspector.md
  - Documented Inspector module integration
  - Documented ContentStore usage patterns
  - Documented ViewportTransformService interaction

Updating SKILL.md files:
• frontend-dev-guidelines/SKILL.md - Added Layer Inspector section
• editor-v3-module-dev/SKILL.md - Added Inspector Module section

Updating skill-rules.json:
• Added keywords: "layer inspector", "layer tree", "inspector panel"
• Added patterns: "src/apps/Editor/modules/Inspector/**"

[Shows summary report]
```

### Example 2: Backend Feature

```
User: /update-skills api-authentication

Claude: I'll update the skills system with the api-authentication feature.

[Analyzes feature]

Creating documentation:
• .claude/skills/api-patterns/features/authentication.md
  - Documented JWT token management
  - Documented refresh token rotation
  - Documented middleware patterns

Updating skill-rules.json:
• Added keywords: "authentication", "JWT", "auth middleware"
• Added patterns: "src/api/auth/**"

[Shows summary report]
```

## Integration with Workflow

**Recommended workflow:**
1. Complete feature implementation
2. Run `/dev-docs-update {feature-name}` to update dev docs
3. Run `/update-skills {feature-name}` to update skills system
4. Move feature to `dev/completed/` (manual or via script)
5. Future features automatically benefit from documented patterns!

## Notes

- This command should be run **after** feature is complete or nearly complete
- Can be run multiple times if feature evolves
- Existing documentation will be updated, not duplicated
- This creates a self-improving system - each feature makes the next one easier!

---

**Ready to update the skills system!** 📚
