# Skill Structure - Progressive Disclosure Pattern

**CRITICAL:** Skills must use the progressive disclosure pattern to keep context usage low. Claude only loads the files it needs for the current task.

---

## Structure

```
.claude/skills/
└── your-skill-name/
    ├── SKILL.md                    # ← MUST BE UPPERCASE! Bare bones quick reference (~100-150 lines)
    ├── resource-1.md               # ← Detailed content (specific patterns)
    ├── resource-2.md               # ← Detailed content (specific patterns)
    └── resource-3.md               # ← Detailed content (specific patterns)
```

**⚠️ CRITICAL:** The main skill file MUST be named `SKILL.md` (ALL CAPS). Claude Code looks specifically for this filename.

---

## SKILL.md Pattern (Bare Bones)

The main `SKILL.md` file should be **SHORT** (~100-150 lines) and act as a **quick reference** that points to detailed resource files.

### Template Structure:

```markdown
---
name: your-skill-name
description: Brief description of what this skill provides
version: 1.0.0
---

# Your Skill Name

Quick reference for [topic]. For detailed patterns, see resource files below.

---

## When to Use This Skill

Use this skill when working on:
- [Use case 1]
- [Use case 2]
- [Use case 3]

---

## Quick Reference

### Topic 1 - [Brief Summary]
[2-3 sentence overview]

**See:** `resource-1.md` for detailed patterns and examples

### Topic 2 - [Brief Summary]
[2-3 sentence overview]

**See:** `resource-2.md` for detailed patterns and examples

### Topic 3 - [Brief Summary]
[2-3 sentence overview]

**See:** `resource-3.md` for detailed patterns and examples

---

## Critical Constraints

List 3-5 absolute must-follow rules:
- ❌ Don't [anti-pattern]
- ✅ Do [best practice]
- ⚠️ Watch out for [gotcha]

---

## Resource Files

Detailed documentation organized by concern:

1. **resource-1.md** - [What this covers]
2. **resource-2.md** - [What this covers]
3. **resource-3.md** - [What this covers]

---

**Pattern:** Start here for quick reference, dive into resource files for implementation details.
```

---

## Resource File Pattern (Detailed)

Resource files contain the **deep content** - patterns, examples, gotchas:

```markdown
# [Topic Name]

Detailed patterns and examples for [specific concern].

---

## Common Patterns

### Pattern 1: [Name]

**When to use:**
[Description of when this pattern applies]

**Example:**
```[language]
[Complete working example]
```

**Gotchas:**
- [Common mistake 1]
- [Common mistake 2]

---

### Pattern 2: [Name]

[Same structure as above]

---

## Examples

### Example 1: [Real-world scenario]

[Complete example with explanation]

---

## Anti-Patterns

❌ **Don't do this:**
```[language]
[Bad example]
```

✅ **Do this instead:**
```[language]
[Good example]
```

**Why:** [Explanation]

---

## Testing

[How to test this pattern]

---

## Related Resources

- See `other-resource.md` for [related topic]
- See main `SKILL.md` for overview
```

---

## Real Example: frontend-dev-guidelines

### SKILL.md (~110 lines)

```markdown
---
name: frontend-dev-guidelines
description: React 19 + TypeScript + Vite + RSuite development patterns
version: 1.0.0
---

# Frontend Development Guidelines

Quick reference for xyz-frontend-v2 development. For detailed patterns, see resource files.

---

## When to Use This Skill

Use when:
- Creating React components
- Working with TypeScript types
- Using RSuite UI components
- Managing state with Zustand

---

## Quick Reference

### React Patterns
Component patterns, hooks, and performance optimization.

**See:** `react-patterns.md` for detailed examples

### TypeScript Standards
Type definitions, strict mode compliance, utility types.

**See:** `typescript-standards.md` for comprehensive guide

### RSuite Components
RSuite usage, CSS variables, theming.

**See:** `rsuite-components.md` for component examples

### State Management
Zustand patterns, selector optimization, store structure.

**See:** `state-management.md` for detailed patterns

---

## Critical Constraints

- ❌ Never use `any` type (use `unknown` instead)
- ✅ Always use CSS variables for styling
- ⚠️ RSuite icons deprecated, use react-icons
- ✅ Minimize re-renders with selectors

---

## Resource Files

1. **react-patterns.md** - Components, hooks, performance
2. **typescript-standards.md** - Types, interfaces, generics
3. **rsuite-components.md** - UI components, styling
4. **state-management.md** - Zustand patterns
5. **performance.md** - Optimization techniques
6. **testing.md** - Vitest patterns
```

### Resource: react-patterns.md (~400 lines)

Contains all the deep React patterns, examples, gotchas, etc.

---

## Why This Pattern?

### ✅ Benefits:

1. **Low context usage** - Claude only loads what it needs
2. **Fast scanning** - Quick reference is easy to scan
3. **Progressive depth** - Can go deeper when needed
4. **Organized** - Separation of concerns
5. **Maintainable** - Easy to update specific topics

### ❌ Anti-Pattern (Don't Do This):

```
.claude/skills/
└── everything-skill/
    └── SKILL.md              # ← 1000+ lines, everything in one file
```

**Problems:**
- High context usage (loads everything)
- Hard to scan (too much content)
- Difficult to maintain (one giant file)
- No separation of concerns

---

## Skill Creation Checklist

When creating a new skill:

- [ ] Create skill directory in `.claude/skills/[name]/`
- [ ] Write bare-bones `SKILL.md` (~100-150 lines)
  - [ ] ⚠️ **MUST be named `SKILL.md` (ALL CAPS)**
  - [ ] YAML frontmatter with name/description/version
  - [ ] "When to Use" section
  - [ ] Quick reference sections (2-3 sentences each)
  - [ ] Links to resource files
  - [ ] Critical constraints (3-5 rules)
- [ ] Create resource files (one per concern, lowercase .md)
  - [ ] Detailed patterns and examples
  - [ ] Gotchas and anti-patterns
  - [ ] Testing guidance
- [ ] Add skill to `skill-rules.json` for auto-activation
- [ ] Test skill activation with sample prompts

---

## Examples by Tech Stack

### React/TypeScript Project:
```
frontend-dev-guidelines/
├── SKILL.md                    # Quick reference
├── react-patterns.md           # Components, hooks
├── typescript-standards.md     # Types, interfaces
├── rsuite-components.md        # UI components
├── state-management.md         # Zustand
├── performance.md              # Optimization
└── testing.md                  # Vitest
```

### Django/Python Project:
```
django-patterns/
├── SKILL.md                    # Quick reference
├── models.md                   # ORM patterns
├── views.md                    # View patterns
├── templates.md                # Template patterns
├── forms.md                    # Form handling
├── testing.md                  # pytest patterns
└── security.md                 # Security best practices
```

### Node/Express API:
```
backend-api-patterns/
├── SKILL.md                    # Quick reference
├── routing.md                  # Route patterns
├── middleware.md               # Middleware patterns
├── database.md                 # Database patterns
├── authentication.md           # Auth patterns
├── error-handling.md           # Error patterns
└── testing.md                  # Test patterns
```

---

**Remember:** The goal is to give Claude just enough context to know what's available, then load details only when needed!
