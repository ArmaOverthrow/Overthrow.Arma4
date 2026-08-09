---
name: overthrow-architecture
description: Overthrow mod architecture patterns, naming conventions, and project structure
version: 1.0.0
---

# Overthrow Architecture

Quick reference for Overthrow mod-specific patterns and conventions. For detailed patterns, see resource files below.

---

## When to Use This Skill

Use this skill when:
- Creating new Manager or Controller components
- Understanding Overthrow's architecture patterns
- Following project naming conventions
- Organizing code in the correct directories
- Accessing global systems via OVT_Global
- Setting up new features or systems

---

## Quick Reference

### Manager Components
Singleton components on OVT_OverthrowGameMode managing entire systems. Use GetInstance() pattern with static s_Instance. Init() and PostGameStart() called manually by game mode.

**See:** `managers.md` for complete manager patterns

### Controller Components
Non-singleton components managing individual entities (bases, towns, camps). Register with manager in constructor. Multiple instances can exist simultaneously.

**See:** `controllers.md` for complete controller patterns

### OVT_Global Access
Central static class providing easy access to all manager singletons. Use OVT_Global.GetSomething() instead of calling GetInstance() directly. Cleaner and more consistent.

**See:** `global-access.md` for access patterns

### OVT_OverthrowController
New modular architecture for client-server communication. Each player owns a controller entity with specialized components. Replaces legacy OVT_PlayerCommsComponent. Built-in progress tracking support.

**See:** `overthrow-controller.md` for complete pattern

### File Organization
Scripts in Scripts/Game/, configs in Configs/, prefabs in Prefabs/. Specific subdirectories for Components, GameMode, Entities, Controllers, UI, UserActions.

**See:** `file-structure.md` for directory structure

### Coding Standards
OVT_ class prefix, m_ member prefix, type prefixes (m_i, m_f, m_s, m_b, m_a, m_m). Doxygen-style comments. Getters/setters for protected members.

**See:** `coding-standards.md` for complete conventions

---

## Critical Conventions

- ✅ **OVT_ prefix** - All Overthrow classes start with OVT_
- ✅ **Managers are singletons** - One instance per game mode
- ✅ **Controllers are instances** - Multiple instances per entity type
- ✅ **Use OVT_Global** - For accessing managers (not direct GetInstance())
- ✅ **Use OverthrowController** - For new client→server operations (not PlayerCommsComponent)
- ✅ **Register in constructor** - Controllers register with managers
- ✅ **Protected members** - Use getters/setters for external access
- ⚠️ **Init() not automatic** - Called manually by game mode or manager
- ✅ **Type prefixes** - m_i for int, m_f for float, m_s for string, etc.

---

## Architecture Hierarchy

```
OVT_OverthrowGameMode (entity)
├── OVT_SomeManagerComponent (singleton)
│   ├── Manages multiple controllers
│   └── Global system state
└── OVT_AnotherManagerComponent (singleton)
    └── Manages different system

Entity in World
└── OVT_SomeControllerComponent (instance)
    ├── Manages this specific entity
    └── Registered with relevant manager
```

---

## Resource Files

Detailed documentation organized by concern:

1. **managers.md** - Singleton manager pattern, GetInstance, Init, PostGameStart
2. **controllers.md** - Instance controllers, registration, lifecycle
3. **overthrow-controller.md** - NEW: Modular controller pattern for client-server operations
4. **global-access.md** - OVT_Global patterns, accessing managers/systems
5. **file-structure.md** - Project directory organization and file placement
6. **coding-standards.md** - Naming conventions, documentation style, best practices

---

## Common Patterns

### Creating a Manager
1. Extend OVT_Component
2. Add corresponding OVT_ComponentClass
3. Implement static s_Instance and GetInstance()
4. Add Init() and PostGameStart() if needed
5. Place component on OVT_OverthrowGameMode prefab
6. Add accessor to OVT_Global

### Creating a Controller
1. Extend OVT_Component
2. Add corresponding OVT_ComponentClass
3. Register with manager in constructor
4. Use protected members with getters/setters
5. Attach to entity prefab or spawn at runtime

### Accessing Systems
1. Use OVT_Global.GetManager() for managers
2. Use OVT_Global.GetController() for local controller
3. Use OVT_Global.GetUI() for UI manager
4. Use OVT_Global.GetPlayers() for player management
5. Always check for null before using

---

**Pattern:** Start here for quick reference, dive into resource files for implementation details.
