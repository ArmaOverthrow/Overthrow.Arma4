# Technical Design Template

Use this template when creating `docs/technical-design.md` for a project. This is the companion to `docs/mission-statement.md` -- the mission statement captures *why* and *what*, this document captures *how*. Adapt heavily to fit the project -- a CLI tool will look very different from a web app. Remove sections that don't apply, add sections that do.

---

```markdown
# [Project Name]

## Technical Design Document

**A companion to the Mission Statement -- this document guides all technical decisions for the project, from architecture to data formats.**

[1-2 sentences about the project context that affects technical decisions. Solo dev vs team? Research prototype vs production? Optimising for speed of iteration vs reliability?]

---

## Table of Contents

[Number the sections -- update this as the document evolves.]

1. Stack & Environment
2. [Project-Specific Section]
3. Project Structure
4. Architecture Overview
5. [Component/Module sections as needed]
6. Data Flow & Internal Formats
7. [Interface section -- CLI / API / UI as applicable]
8. Configuration
9. Testing Strategy
10. Development Principles

---

## 1. Stack & Environment

### [Primary Language]

[Why this language? Keep it to 1-2 sentences -- the answer should be obvious from the ecosystem.]

### Core Technologies

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| Language | [e.g., Python 3.11+] | [Why] |
| Package Manager | [e.g., uv, npm, pnpm] | [Why] |
| Framework | [e.g., Next.js, Django, Express] | [Why] |
| Database | [e.g., PostgreSQL, SQLite] | [Why] |
| ORM / Data | [e.g., Prisma, SQLAlchemy] | [Why] |
| Testing | [e.g., pytest, Vitest] | [Why] |
| [Other layers] | [Technology] | [Why] |

### Hardware / Infrastructure (if relevant)

| Resource | Spec | Usage |
|----------|------|-------|
| [e.g., GPU] | [e.g., RTX 4070] | [What it's used for] |
| [e.g., Hosting] | [e.g., Vercel] | [What runs there] |

### Why This Stack

[1 paragraph explaining the overarching reasoning. What is the single most important factor driving technology choices?]

---

## 2. [Lessons Learned / Prior Art / Constraints] (optional)

[If the project builds on prior work, existing tools, or has learned from predecessors, document that here. What do we adopt? What do we avoid? Why?]

### What We Adopt

[Patterns, approaches, or tools borrowed from existing solutions.]

### What We Avoid

[Anti-patterns, approaches that don't work for this use case, and why.]

---

## 3. Project Structure

```
project-name/
├── [top-level config files]
├── src/
│   └── [source structure]
├── tests/
│   └── [test structure]
├── docs/
│   └── [doc structure]
└── [other directories]
```

### Directory Responsibilities

**`src/[main-module]/`** -- [What lives here and why.]

**`src/[sub-module]/`** -- [What lives here and why.]

**`tests/`** -- [Testing approach and organisation.]

**`config/` or equivalent** -- [Configuration files and their purpose.]

---

## 4. Architecture Overview

### Overview

[High-level architecture description. How do the main components relate to each other? Include an ASCII diagram if it helps.]

```
[ASCII architecture diagram showing main components and data flow]
```

### [Core Pattern / Orchestration]

[How components are coordinated. Pipeline? Event-driven? Request/response? Describe the main execution flow.]

### [Key Architectural Decision]

[Document significant architectural patterns -- plugin systems, middleware chains, module boundaries, etc.]

---

## 5+ [Component / Module Sections]

For each major component or module, document:

### Purpose

[What this component does and why it exists.]

### [Implementation Details]

[Key classes, interfaces, data structures. Include code examples for patterns that other components should follow.]

### Design Decisions

[Why was it built this way? What alternatives were considered? What trade-offs were made?]

---

## Data Flow & Internal Formats

### [Internal Data Exchange Format]

[How data moves between components. Data classes, types, schemas -- document the contracts.]

```[language]
[Example data structure / type definition]
```

### [External Data Format] (if applicable)

[API responses, file formats, database schemas -- whatever the system produces or consumes.]

---

## [Interface Section -- CLI / API / UI]

### [Interface Type]

[How users or other systems interact with this project.]

```[language]
[Example usage / API call / CLI command]
```

---

## Configuration

### [Configuration Approach]

[How is the project configured? Config files, environment variables, command-line flags?]

```[format]
[Example configuration]
```

### [Configuration Profiles / Environments] (if applicable)

[Different configurations for different use cases -- dev, prod, testing, etc.]

---

## Testing Strategy

### [Test Types]

[What kinds of tests exist? Unit, integration, end-to-end? What's the philosophy?]

### [Test Data / Fixtures]

[How is test data managed? Fixtures, factories, mocks?]

### [What Gets Tested vs What Doesn't]

[Be explicit about testing boundaries -- what's worth testing and what isn't at this stage.]

---

## Development Principles

[3-5 numbered principles that guide day-to-day development decisions. These should be specific to this project, not generic software engineering advice.]

1. **[Principle 1]** -- [Description. E.g., "Each module is independently testable. If testing a component requires spinning up the whole system, the boundaries are wrong."]

2. **[Principle 2]** -- [Description.]

3. **[Principle 3]** -- [Description.]

---

## [Phase Boundaries / Roadmap] (optional)

[If the project has clear phases or tiers, document the boundaries and what changes between them.]

### [Phase 1 / MVP]

[What's in scope, what's explicitly out.]

### [Phase 2 / Next]

[What's planned next, what prerequisites must be met.]

---

*This is a living document. Update it as technical decisions are made, patterns emerge, and the architecture evolves.*
```

---

## Guidelines for Creating a Technical Design

1. **Read the codebase first.** For existing projects, the technical design should document what *is*, not what you wish it was. Read package files, config, source structure, and existing docs.

2. **Every technology choice needs a "why".** The stack table should have a rationale column. "We use X because it's popular" is not a rationale. "We use X because the ecosystem requires it" is.

3. **Include code examples for patterns.** If other components should follow a pattern (registry pattern, plugin interface, data class structure), show the actual code or a representative example.

4. **Document decisions, not just facts.** "We use PostgreSQL" is a fact. "We use PostgreSQL because we need JSONB queries and transactional consistency for X" is a decision. Decisions are what future contributors need.

5. **Scale to the project.** A small CLI tool might have 3 sections. A large distributed system might have 20. Don't pad with sections that have nothing useful to say.

6. **ASCII diagrams over nothing.** A simple box-and-arrow ASCII diagram beats a wall of text for architecture overview. Don't overcomplicate it.

7. **For existing projects:** Extract the design from the code. Check package.json/pyproject.toml for dependencies, scan the source tree for patterns, read any existing docs. Ask the user to fill gaps.

8. **For new projects:** Ask about tech stack preferences, deployment targets, scale expectations, and team expertise. Draft the design for their review.
