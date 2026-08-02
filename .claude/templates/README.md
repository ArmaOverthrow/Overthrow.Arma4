# Dev Docs System

**Purpose:** Keep context lean, track progress, and never lose the plot on long features.

This directory contains active and completed feature development documentation that helps you and Claude maintain continuity across work sessions, conversation compacting, and agent handoffs.

---

## 📁 Directory Structure

```
dev/
├── README.md              # This file - your guide
├── active/                # Features currently being worked on
│   └── feature-name/
│       ├── plan.md        # Implementation plan (from solution-architect)
│       ├── context.md     # Key files, decisions, next steps
│       └── tasks.md       # Checklist with progress tracking
├── completed/             # Archived finished features
│   └── feature-name/
│       ├── plan.md
│       ├── context.md
│       └── tasks.md
└── templates/             # Templates for new features
    ├── plan.md
    ├── context.md
    └── tasks.md
```

---

## 🚀 Quick Start

### Starting a New Feature

**After PRD + Implementation Plan are approved:**

```bash
# Use the slash command (recommended)
/start-feature

# Or manually:
mkdir -p dev/active/my-feature
cp dev/templates/*.md dev/active/my-feature/
# Edit plan.md, context.md, tasks.md with your feature details
```

### Resuming Work on a Feature

```bash
# Use the slash command (recommended)
/continue-feature

# Claude will:
# 1. Read plan.md to understand the feature
# 2. Read context.md to see current progress
# 3. Read tasks.md to know what's next
# 4. Ready to continue exactly where you left off!
```

### Before Compacting Conversations

```bash
# ALWAYS run this before compacting!
/dev-docs-update

# Claude will:
# 1. Update tasks.md with completed items
# 2. Update context.md with progress and decisions
# 3. Update "Next Steps" so you can resume seamlessly
```

---

## 📋 The Three Files Explained

### 1. plan.md - The Blueprint

**Source:** Copied from solution-architect's implementation plan
**Purpose:** The approved technical plan that doesn't change much
**Update Frequency:** Rarely (only if plan fundamentally changes)

**What it contains:**
- Executive summary
- Goals and success criteria
- Implementation phases
- Key technical decisions
- Risks and dependencies
- Testing strategy

**Best practice:** Treat this as read-only. If the plan changes significantly, note it in context.md

---

### 2. context.md - The Living Document

**Purpose:** Current status, key files, decisions made during implementation
**Update Frequency:** After each work session (use `/dev-docs-update`)

**What it contains:**
- **Quick Status** - What's done, what's next, blockers
- **Key Files** - All important files with descriptions
- **Important Decisions** - Decisions made during implementation
- **Gotchas & Learnings** - Problems encountered and solutions
- **Next Steps** - Immediate, short-term, and future tasks
- **Session Notes** - Progress log for each work session

**Best practice:**
- Update "Next Steps" at end of each session
- Document all important decisions with context
- Note gotchas so you don't hit them again
- Keep "Quick Status" current

---

### 3. tasks.md - The Checklist

**Purpose:** Granular task tracking with progress visibility
**Update Frequency:** Real-time (mark tasks done immediately)

**What it contains:**
- Tasks organized by phase
- Progress percentage
- Bugs and issues
- Technical debt items
- Testing tasks
- Documentation tasks

**Best practice:**
- Mark tasks ✅ immediately when complete
- Add new tasks as you discover them
- Use status indicators (🔄 in progress, ⏸️ blocked)
- Track bugs separately from feature tasks

---

## 🎯 Recommended Workflow

### Your Standard Process

**1. Feature Planning:**
```
You → Discuss requirements
   → Create PRD (docs/features/[name]/prd.md)
   → Iterate until approved

solution-architect → Creates implementation plan
                  → (docs/features/[name]/implementation.md)
   → You iterate and approve
```

**2. Start Development:**
```
You → /start-feature [name]
    → Claude creates dev docs from approved plan

frontend-dev → Reads plan.md + context.md
            → Implements Phase 1
            → Updates tasks.md as work progresses
```

**3. During Development:**
```
You work on feature...
   → Tasks get marked done in tasks.md
   → Decisions documented in context.md
   → Problems/solutions noted in context.md

Need to compact conversation?
   → /dev-docs-update (saves everything to context.md)
   → Compact safely
   → /continue-feature [name] (resume with full context)
```

**4. Phase Complete:**
```
frontend-dev → Marks phase tasks complete
            → Updates context.md "Next Steps"
            → Ready for next phase

You → Review progress in tasks.md
   → Approve to continue next phase
```

**5. Feature Complete:**
```
frontend-dev → All tasks in tasks.md marked ✅
            → All tests passing
            → Documentation updated

You → Move feature to completed/
```

---

## 💡 Best Practices

### DO:

✅ **Update context.md frequently**
   - After each work session
   - Before compacting conversations
   - When making important decisions

✅ **Mark tasks done immediately**
   - Don't batch updates
   - Keep tasks.md current

✅ **Document gotchas**
   - When you encounter a problem and solve it
   - Save future-you (and Claude) time

✅ **Keep "Next Steps" current**
   - Update at end of each session
   - Helps resume work seamlessly

✅ **Use `/dev-docs-update` before compacting**
   - Don't lose context!
   - Takes 30 seconds, saves hours later

✅ **Add new tasks as discovered**
   - Implementation always reveals new tasks
   - Track them in tasks.md immediately

### DON'T:

❌ **Don't skip `/dev-docs-update` before compacting**
   - You'll lose all progress context
   - Claude won't know where you left off

❌ **Don't edit plan.md during implementation**
   - Context.md is for implementation changes
   - Plan.md is the original approved plan

❌ **Don't forget to update timestamps**
   - Helps track when decisions were made
   - Shows which info is current

❌ **Don't let context.md get stale**
   - Update after each session
   - Stale context = lost context

❌ **Don't commit without updating dev docs**
   - Dev docs should reflect code state
   - Update before git commits

---

## 🔄 Agent Handoffs

### Why Dev Docs Help with Agents

**Problem:** Agents start fresh, don't know what's done
**Solution:** Dev docs are the source of truth

### When Using Agents:

**solution-architect:**
- Reads PRD
- Creates implementation plan
- Plan becomes `plan.md` in dev docs

**frontend-dev:**
- Reads `plan.md` (what to build)
- Reads `context.md` (current status)
- Reads `tasks.md` (what's next)
- Implements phase by phase
- Updates tasks.md as work progresses

**Your benefit:**
- No need to re-explain progress to agents
- Agents have same context you do
- Seamless handoffs between agents
- Continuity across sessions

---

## 📊 Progress Tracking

### Reading Progress at a Glance

**Quick check:**
1. Open `tasks.md` → See percentage complete
2. Look for 🔄 (in progress) and ⏸️ (blocked) indicators
3. Count ✅ tasks vs total tasks

**Detailed check:**
1. Read `context.md` → "Quick Status" section
2. See "What's Done" and "What's Next"
3. Check for blockers

**Full context:**
1. Read `plan.md` → Remember the overall plan
2. Read `context.md` → Understand decisions and progress
3. Read `tasks.md` → See granular progress

---

## 🛠️ Slash Commands

### `/start-feature [feature-name]`
**When:** After PRD and implementation plan approved
**Does:**
- Creates dev/active/[feature-name]/ directory
- Copies plan from docs/features/[name]/implementation.md
- Creates context.md with initial structure
- Breaks down plan into tasks.md checklist

**Usage:**
```
# With argument (faster - direct execution)
You: /start-feature layer-inspector
Claude: [Reads command with $ARGUMENTS="layer-inspector"]
        [Creates all dev docs, shows structure]

# Without argument (interactive)
You: /start-feature
Claude: [Reads command with $ARGUMENTS=""]
        "I see $ARGUMENTS is empty. Available features in docs/features/..."
        "Which feature would you like to set up?"
You: "layer-inspector"
Claude: [Creates all dev docs, shows structure]
```

---

### `/continue-feature [feature-name]`
**When:** Resuming work on existing feature
**Does:**
- Reads plan.md, context.md, tasks.md for specified feature
- Summarizes current status
- Shows next steps
- Ready to continue

**Usage:**
```
# With argument (fastest - instant context)
You: /continue-feature layer-inspector
Claude: [Reads command with $ARGUMENTS="layer-inspector"]
        [Loads dev/active/layer-inspector/*.md files]
        "📋 Feature: layer-inspector
         🎯 Current Phase: Phase 2 - UI Components
         📊 Progress: 5/12 tasks complete (42%)
         Ready to continue!"

# Without argument (lists features)
You: /continue-feature
Claude: [Reads command with $ARGUMENTS=""]
        "Available features in dev/active/:
         1. layer-inspector (Phase 2 - UI Components)
         2. undo-redo (Phase 1 - Foundation)
         Which feature would you like to continue?"
You: "1"
Claude: [Loads layer-inspector docs, shows status]
```

---

### `/dev-docs-update [feature-name]`
**When:** Before compacting conversation (CRITICAL!)
**Does:**
- Updates tasks.md (marks completed, adds new)
- Updates context.md (progress, decisions, next steps)
- Updates timestamps
- Shows summary

**Usage:**
```
# With argument (explicit)
You: /dev-docs-update layer-inspector
Claude: [Reads command with $ARGUMENTS="layer-inspector"]
        [Reviews conversation, updates context.md and tasks.md]
        "✅ Updated dev docs for: layer-inspector
         📝 tasks.md: Marked 5 tasks complete
         📋 context.md: Added 2 decisions, updated Next Steps
         ✅ Safe to compact conversation!"

# Without argument (auto-detects from conversation)
You: /dev-docs-update
Claude: [Reads command with $ARGUMENTS=""]
        [Scans conversation for feature mentions in dev/active/]
        "I see you've been working on layer-inspector. Update those docs?"
You: "yes"
Claude: [Updates all files, shows summary]
```

---

## 🎓 Tips & Tricks

### Tip 1: Use Dev Docs as Your Memory

Before each session:
```
1. Open dev/active/[feature]/context.md
2. Read "Next Steps"
3. You immediately know what to do!
```

### Tip 2: Document Decisions Immediately

When you make a decision during implementation:
```
1. Add to context.md "Important Decisions"
2. Include: Context, Decision, Rationale
3. Future-you will thank you
```

### Tip 3: Track Discovered Tasks

Implementation always reveals new tasks:
```
1. Add to tasks.md under "Discovered New Tasks"
2. Move to appropriate phase
3. Never forget to do something!
```

### Tip 4: Session Notes

End each session with notes in context.md:
```
### 2025-10-30 15:00
- Completed module structure and service
- Decision: Use pub/sub pattern for events
- Blocker: Need API endpoint for data fetching
- Next: Create UI overlay component
```

### Tip 5: Archive Completed Features

When feature is done:
```bash
mv dev/active/my-feature dev/completed/
# Keep as reference for future features!
```

---

## 🚨 Common Pitfalls

### Pitfall 1: Forgetting to Update

**Problem:** Work for hours, forget to update context.md
**Result:** Can't remember what you did or why
**Solution:** Update context.md at END of each session (set a timer!)

### Pitfall 2: Skipping `/dev-docs-update`

**Problem:** Compact conversation without updating dev docs
**Result:** Lose all progress context
**Solution:** ALWAYS `/dev-docs-update` before compacting

### Pitfall 3: Not Documenting Decisions

**Problem:** Make decision, don't write it down
**Result:** Forget why you chose that approach
**Solution:** Add to context.md immediately

### Pitfall 4: Letting Tasks.md Get Stale

**Problem:** Complete tasks but don't mark them
**Result:** Can't see real progress
**Solution:** Mark tasks ✅ immediately when done

### Pitfall 5: No "Next Steps"

**Problem:** End session without noting what's next
**Result:** Waste time next session figuring out what to do
**Solution:** Always update "Next Steps" before stopping

---

## 📚 Example: Complete Workflow

### Feature: Layer Inspector

**Day 1: Planning**
```
You: "I want a layer inspector for the editor"
[Create PRD, iterate]
solution-architect: [Creates implementation plan]
You: /start-feature layer-inspector
[Dev docs created]
```

**Day 2: Phase 1**
```
You: /continue-feature
Claude: "Layer Inspector - Starting Phase 1"
frontend-dev: [Implements module structure]
[Updates tasks.md as work progresses]
You: /dev-docs-update
[Compact conversation]
```

**Day 3: Phase 2**
```
You: /continue-feature layer-inspector
Claude: [Reads context.md] "Phase 1 complete. Next: UI overlay"
frontend-dev: [Implements UI components]
[Discovers 3 new tasks, adds to tasks.md]
[Documents decision about state management in context.md]
You: /dev-docs-update
```

**Day 4: Complete**
```
You: /continue-feature layer-inspector
frontend-dev: [Finishes Phase 3, all tasks ✅]
You: "Great! Let's archive it"
mv dev/active/layer-inspector dev/completed/
```

---

## 🎉 Benefits Summary

### For You:
✅ Never forget where you left off
✅ Clear progress tracking
✅ Safe conversation compacting
✅ Quick context recovery
✅ Historical record of decisions

### For Claude:
✅ Instant context on resume
✅ Clear understanding of progress
✅ Know exactly what's next
✅ No need for re-explanation
✅ Better agent handoffs

### For Your Workflow:
✅ Lean main conversation context
✅ Better agent coordination
✅ Faster feature completion
✅ Reduced context switching cost
✅ Confidence in progress

---

## 📖 Further Reading

- **Optimization Plan:** `docs/claude-code-optimization-plan.md`
- **Feature Templates:** `dev/templates/`
- **Active Features:** `dev/active/`
- **Completed Features:** `dev/completed/` (for reference)

---

## 🙋 Questions?

**Q: Do I need to use all three files?**
A: Yes! Each serves a purpose. plan.md = blueprint, context.md = current state, tasks.md = checklist.

**Q: Can I skip `/dev-docs-update`?**
A: NO! This is the most important command. Always use before compacting.

**Q: How often should I update context.md?**
A: At minimum: End of each work session, before compacting. Ideally: After major decisions or discoveries.

**Q: What if I forget to update dev docs?**
A: You'll lose context when you compact. Best practice: Set a timer to remind yourself.

**Q: Can I modify plan.md during implementation?**
A: Rarely. Only if the plan fundamentally changes. Otherwise use context.md for notes.

---

*This system is based on the Reddit post "Claude Code is a beast: Tips from 6 months of hardcore use" and adapted for the xyz-frontend-v2 workflow.*

**Remember: The dev docs system is only as good as you keep it updated. Make it a habit!** 🚀
