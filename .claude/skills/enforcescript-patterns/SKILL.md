---
name: enforcescript-patterns
description: EnforceScript component patterns, networking, persistence, and memory management for Enfusion engine
version: 1.0.0
---

# EnforceScript Patterns

Quick reference for EnforceScript development in Arma Reforger. For detailed patterns, see resource files below.

---

## When to Use This Skill

Use this skill when:
- Creating entity components (Managers, Controllers, Components)
- Implementing network replication (RplProp, RPC, JIP)
- Setting up persistence with EPF (save/load patterns)
- Managing memory (strong refs, garbage collection)
- Building UI contexts and layouts
- Troubleshooting common EnforceScript pitfalls

---

## Quick Reference

### Component Patterns
Three main component types in Overthrow: Managers (singletons on game mode), Controllers (instance managers), and Components (sub-systems). Each has specific lifecycle and registration patterns.

**See:** `component-patterns.md` for detailed patterns and examples

### Network Replication
Use RplProp for simple value synchronization, RPC for server/client communication, and JIP for late-join state sync. Never replicate EntityID - use RplId instead.

**See:** `networking.md` for comprehensive replication patterns

### Persistence
EPF requires SaveData classes that extend EPF_ComponentSaveDataClass. ReadFrom extracts data, ApplyTo restores it. Console platforms require PLATFORM_CONSOLE guards.

**See:** `persistence.md` for EPF save/load patterns

### Memory Management
All Managed class references must use `ref` keyword to prevent garbage collection. Store EntityID instead of IEntity for long-term references. Check entity existence before use.

**See:** `memory-management.md` for garbage collection patterns

### UI Patterns
UI contexts extend OVT_UIContext with m_Layout property. Activate contexts via OVT_Global.GetUI().ShowContext(). Each context manages its own .layout file lifecycle.

**See:** `ui-patterns.md` for UI context patterns

### Common Pitfalls
EnforceScript has unique constraints: no ternary operators, specific replication patterns, strict typing. Knowing these pitfalls saves debugging time.

**See:** `common-pitfalls.md` for anti-patterns and solutions

---

## Critical Constraints

- ❌ **No ternary operators** - Use full if/else statements always
- ✅ **Strong refs for Managed** - Always use `ref` keyword for arrays/maps of Managed classes
- ⚠️ **EntityID vs RplId** - Use RplId for network entity references, EntityID locally only
- ✅ **Check entity existence** - Always verify entity still exists before using it
- ❌ **Don't replicate entities** - Never use RplProp on IEntity or EntityID
- ✅ **Server authority** - Server drives game state, clients receive updates
- ⚠️ **RPC direction** - RpcAsk = client→server, RpcDo = server→client(s)

---

## Resource Files

Detailed documentation organized by concern:

1. **component-patterns.md** - Manager, Controller, and Component class patterns
2. **networking.md** - RplProp, RPC, JIP replication, and optimization
3. **persistence.md** - EPF save/load patterns and console platform handling
4. **memory-management.md** - Strong refs, garbage collection, entity lifecycle
5. **ui-patterns.md** - OVT_UIContext, layout activation, UI manager integration
6. **common-pitfalls.md** - Ternary operators, weak refs, replication gotchas

---

## Language-Specific Notes

EnforceScript is a C++ variant with unique characteristics:
- Strongly typed with type inference limited
- No ternary operator support (use if/else)
- Garbage collection for Managed classes
- Native entity-component architecture
- Built-in replication system via Rpl attributes
- No null coalescing operator

---

**Pattern:** Start here for quick reference, dive into resource files for implementation details.
