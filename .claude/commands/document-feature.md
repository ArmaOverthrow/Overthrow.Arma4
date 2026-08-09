---
description: "Document reusable skills and patterns from a completed feature. Usage: /document-feature [feature-name]"
---

# /document-feature - Extract Reusable Knowledge from Features

You have been asked to document reusable skills and patterns from a completed feature.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, and fuzzy-fall-back into
epic folders for bare names.
If `$ARGUMENTS` is a bare epic name, this command documents one feature at a time — list the epic's child features and ask which to document.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When the feature lives inside an epic, every `docs/features/$ARGUMENTS/...` path below is the resolved nested `docs/features/<epic>/<feature>/...` location. This command's body is the document-feature workflow; it does **not** re-specify those rules.

## Purpose

This command extracts reusable knowledge from a feature and adds it to the project's skill system in `.claude/skills/`. This builds a knowledge base that helps future agents work more effectively.

## Process

### 1. Determine Feature Context

**If feature name is provided (`$ARGUMENTS` is not empty):**
- Use `$ARGUMENTS` as the feature name
- Proceed to step 2 to load feature docs

**If NO feature name is provided:**
- Assume user has already run `/continue-feature` in this chat session
- You should already have context from the current conversation about what feature was worked on
- Identify the feature name from your conversation context
- If you cannot determine the feature, ask the user which feature to document

### 2. Load Feature Context (if not already loaded)

**If you already have context from `/continue-feature`:**
- Skip loading - you already have the feature docs in context
- Use your existing knowledge of the feature from this chat session

**If feature name was provided (and you don't have context):**
- Verify `docs/features/$ARGUMENTS/` exists with at least `implementation.md`
- Read these files to understand what was built:
  - `docs/features/$ARGUMENTS/implementation.md`
  - `docs/features/$ARGUMENTS/context.md` (if exists)
  - `docs/features/$ARGUMENTS/tasks.md` (if exists)

If feature not found:
- List available features in `docs/features/`
- Ask user to specify which feature to document

### 3. Analyze for Reusable Patterns

Review the feature for knowledge worth sharing. Look for:

**DOCUMENT these:**
- Systems or frameworks that future features may need to use
- Gotchas discovered during development that would help future agents
- Concise example code for using systems developed by this feature
- Updates to existing patterns/systems that are already documented
- New component patterns, API patterns, state management approaches
- Integration patterns with existing systems

**DO NOT document:**
- Elements specific and unique to this feature only
- Patterns that won't be needed by other features
- Implementation details that don't generalize
- Obvious patterns already well-documented

### 4. Check Existing Skills

List directories in `.claude/skills/` to see what's already documented.

Read relevant `SKILL.md` files to understand existing content.

Determine if you need to:
- **Create** new skill directory with `SKILL.md` + resource files
- **Update** existing skill's `SKILL.md` or resource files
- **Remove** outdated patterns from existing files

### 5. Create/Update Skill Files

Create directory structure if needed:
```bash
mkdir -p .claude/skills/<skill-name>
```

#### For NEW Skills:

Create `.claude/skills/<skill-name>/SKILL.md` (MUST be uppercase).

The frontmatter `description` is **load-bearing**: Claude Code reads it to decide when to
surface the skill, so write it as a clear, specific trigger — what the skill covers and when
to reach for it (e.g. "React 19 + TypeScript component, hook, and state-management patterns
for this app"). There is no separate keyword/rules file; the `description` is what drives
native skill loading.

```markdown
---
name: <skill-name>
description: Specific, trigger-style summary of what this skill covers and when to use it
version: 1.0.0
---

# <Skill Name>

Quick reference for [topic]. For detailed patterns, see resource files below.

---

## When to Use This Skill

Use this skill when:
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

---

## Critical Constraints

- ❌ Don't [anti-pattern]
- ✅ Do [best practice]
- ⚠️ Watch out for [gotcha]

---

## Resource Files

Detailed documentation organized by concern:

1. **resource-1.md** - [What this covers]
2. **resource-2.md** - [What this covers]

---

*Last updated by: <feature-name> feature*
*Date: YYYY-MM-DD*
```

Create resource files `.claude/skills/<skill-name>/<resource>.md` for detailed content:

```markdown
# <Resource Topic>

Detailed patterns and examples for [specific concern].

---

## Common Patterns

### Pattern 1: [Name]

**When to use:** [Description]

**Example:**
```[language]
[Complete working example]
```

**Gotchas:**
- [Common mistake]

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
```

#### For EXISTING Skills:

Update the relevant `SKILL.md` quick reference section and/or resource files.

**Keep skill files:**
- Focused on ONE system/pattern
- Concise but complete (~100-150 lines for SKILL.md)
- Full of practical examples
- Free of deprecated patterns

### 6. Verify Changes

- Check that all new skill files exist and have proper content
- Verify `SKILL.md` files are UPPERCASE
- Confirm each `SKILL.md` has a clear, specific `description` in its frontmatter (this is what Claude Code uses to surface the skill — there is no keyword/rules file)
- Ensure no stale patterns remain

### 7. Show Summary

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ SKILLS DOCUMENTED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📋 Feature: <feature-name>

📝 Skills Created/Updated:
- [skill-name/] - Created - Brief description
- [other-skill/] - Updated - What changed

📄 Files Modified:
- .claude/skills/<skill-name>/SKILL.md
- .claude/skills/<skill-name>/<resource>.md

🗑️ Removed (if any):
- [deprecated-pattern] - Removed from [file] (no longer used)

🎯 Future agents will now know about:
- [Key capability 1]
- [Key capability 2]
- [Key capability 3]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

💡 Claude Code surfaces these skills automatically based on each SKILL.md `description`.
```

## Important Rules

**DO NOT** Leave stale, outdated patterns in skill files - they must reflect CURRENT working state

**DO NOT** Deprecate patterns - if a pattern is deprecated, remove it entirely. New work should never use legacy systems.

**DO NOT** Document feature-specific patterns that won't help other features

**DO NOT** Create massive skill files - use progressive disclosure with resource files

**DO** Keep `SKILL.md` as a quick reference (~100-150 lines) that points to resource files

**DO** Keep resource files focused and practical with real code examples

**DO** Remove patterns that are no longer valid when updating existing skill files

**DO** Use UPPERCASE `SKILL.md` filename (required by Claude Code)

## Error Handling

- If no feature name provided AND you can't determine feature from conversation context, ask user which feature to document
- If feature doesn't exist, list available features and ask user to specify
- If no reusable patterns found, explain why and confirm with user before skipping
- If skill file exists, show diff of proposed changes before applying
- If removing patterns, confirm with user first
- If `.claude/skills/` doesn't exist, create it

## Example Workflows

**Example 1: After /continue-feature (no argument needed)**
```
User: /continue-feature user-auth
Claude: [Loads context, works on feature...]

[... work session ...]

User: /document-feature

Claude: I'll document reusable skills from the user-auth feature
(using context from our current session).

Analyzing for reusable patterns...
[Already has context from /continue-feature]

Found these patterns worth documenting:
1. Authentication middleware pattern
2. JWT token handling utilities

[Creates/updates skill files]

✅ Skills documented!
```

**Example 2: With feature name argument**
```
User: /document-feature user-auth

Claude: I'll document reusable skills from the user-auth feature.

Loading feature context...
[Reads implementation.md, context.md, tasks.md]

Analyzing for reusable patterns...

Found these patterns worth documenting:
1. Authentication middleware pattern
2. JWT token handling utilities
3. GOTCHA: Session invalidation timing issue

Checking existing skills...
[Lists .claude/skills/ and reads relevant SKILL.md files]

I'll create:
- .claude/skills/authentication/SKILL.md (new)
- .claude/skills/authentication/jwt-patterns.md (new)
- Update .claude/skills/api-patterns/SKILL.md (add auth middleware)

[Creates/updates skill files]

✅ Skills documented!
```

**Example 3: Updating existing skill**
```
User: /document-feature data-visualization

Claude: I'll document reusable skills from the data-visualization feature.

[Reads feature docs]

Found patterns to document:
1. Chart component abstraction
2. Data transformation utilities

Checking existing skills...
Found: .claude/skills/frontend-dev-guidelines/ (relevant)

I'll update:
- Add new resource: .claude/skills/frontend-dev-guidelines/charts.md
- Update SKILL.md quick reference with charts section

[Shows proposed changes before applying]
[Updates files]

✅ Skills documented!
```

## Integration with Workflow

**Recommended workflow:**
1. Complete feature implementation
2. Run `/update-feature <feature-name>` to update feature docs
3. Run `/document-feature <feature-name>` to extract reusable skills
4. Future features automatically benefit from documented patterns!

## Notes

- This command should be run **after** feature is complete or nearly complete
- Can be run multiple times if feature evolves
- Existing documentation will be updated, not duplicated
- This creates a self-improving system - each feature makes the next one easier!

---

**Ready to document reusable skills!** 📚
