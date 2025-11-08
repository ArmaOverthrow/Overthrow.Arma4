# Component Patterns

Detailed patterns for EnforceScript component architecture in Overthrow mod.

---

## Component Types Overview

Overthrow uses three primary component types:

1. **Manager Components** - Singletons on OVT_OverthrowGameMode managing entire systems
2. **Controller Components** - Instance managers for individual entities (bases, towns, etc.)
3. **Component Classes** - Sub-systems on entities managing specific functionality

---

## Manager Component Pattern

### When to Use

Use Manager components when you need:
- System-wide singleton management
- Global state coordination
- Registration of multiple controller instances
- Server-side only logic with optional client replication

### Pattern Structure

```cpp
class OVT_SomeManagerComponentClass: OVT_ComponentClass {};

class OVT_SomeManagerComponent: OVT_Component
{
    // Editor-configurable parameters
    [Attribute("1", desc: "A parameter description")]
    int m_iSomeParameter;

    // Singleton instance
    static OVT_SomeManagerComponent s_Instance;

    // Singleton accessor
    static OVT_SomeManagerComponent GetInstance()
    {
        if (!s_Instance)
        {
            BaseGameMode pGameMode = GetGame().GetGameMode();
            if (pGameMode)
                s_Instance = OVT_SomeManagerComponent.Cast(pGameMode.FindComponent(OVT_SomeManagerComponent));
        }
        return s_Instance;
    }

    // Called manually by OVT_OverthrowGameMode for initialization
    void Init(IEntity owner)
    {
        // Initialize manager state
    }

    // Called manually after new game starts
    void PostGameStart()
    {
        // Setup initial game state
    }
}
```

### Key Points

- **Singleton Pattern:** Use static s_Instance and GetInstance()
- **Initialization:** Init() and PostGameStart() called manually by game mode
- **Placement:** Component placed on OVT_OverthrowGameMode.et prefab
- **Attributes:** Use [Attribute()] for editor-exposed configuration
- **Naming:** OVT_ prefix, Component suffix

### Gotchas

- GetInstance() may return null if game mode not loaded yet
- Always check for null before using GetInstance() result
- Init() not called automatically - game mode must call it
- Manager only exists on server unless explicitly replicated

---

## Controller Component Pattern

### When to Use

Use Controller components when you need:
- Management of individual entity instances
- State replication for specific entities
- Persistence of entity-specific data
- Non-singleton, multiple instances

### Pattern Structure

```cpp
class OVT_SomeControllerComponentClass: OVT_ComponentClass {};

class OVT_SomeControllerComponent: OVT_Component
{
    // Protected parameters (use getters/setters)
    [Attribute("1", desc: "A parameter description")]
    protected int m_iSomeParameter;

    // Constructor - register with manager
    void OVT_SomeControllerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
    {
        // Register on both client and server
        OVT_Global.GetSomeManager().RegisterController(this);
    }

    // Init may not be called unless manager explicitly calls it
    void Init(IEntity owner)
    {
        // Controller initialization if needed
    }

    // Getters and setters for parameters
    int GetSomeParameter() { return m_iSomeParameter; }
    void SetSomeParameter(int someParameter)
    {
        m_iSomeParameter = someParameter;
    }
}
```

### Key Points

- **Registration:** Register with manager in constructor
- **Constructor pattern:** void ComponentName(IEntityComponentSource src, IEntity ent, IEntity parent)
- **Protected members:** Use protected for parameters, expose via getters/setters
- **Multiple instances:** Not singletons, many can exist
- **Legacy detection:** Old pattern used QueryEntitiesBySphere (deprecated)

### Gotchas

- Constructor called on both client and server
- Init() only called if manager explicitly calls it
- Must register with manager or entity won't be tracked
- Don't use QueryEntitiesBySphere for new controllers

---

## Component Class Pattern

### When to Use

Use Component classes when you need:
- Sub-system management on an entity
- Split complex controllers into focused components
- Reusable functionality across multiple entity types
- Non-controller, non-manager components

### Pattern Structure

```cpp
class OVT_SomeComponentClass: OVT_ComponentClass {};

class OVT_SomeComponent: OVT_Component
{
    // Protected parameters
    [Attribute("1", desc: "A parameter description")]
    protected int m_iSomeParameter;

    // Initialization
    void Init(IEntity owner)
    {
        // Component initialization
    }

    // Getters and setters
    int GetSomeParameter() { return m_iSomeParameter; }
    void SetSomeParameter(int someParameter)
    {
        m_iSomeParameter = someParameter;
    }
}
```

### Key Points

- **No Class suffix needed:** Only components placed on entities need OVT_ComponentClass
- **Focused functionality:** Single responsibility principle
- **Composable:** Multiple components work together on same entity
- **Init pattern:** Init(IEntity owner) for setup

### Gotchas

- Init() called manually by parent component/controller
- Not all components need a corresponding ComponentClass
- Use for composition, not inheritance

---

## Component Class Naming

Only components that are placed on entities require a corresponding `ComponentClass`:

```cpp
// Placed on entity prefab - needs ComponentClass
class OVT_ManagerComponentClass: OVT_ComponentClass {};
class OVT_ManagerComponent: OVT_Component {};

// Not placed on entity - no ComponentClass needed
class OVT_HelperClass: Managed {};
class OVT_DataClass: Managed {};
```

---

## Data Struct Pattern

### When to Use

Use data structs for:
- Simple data storage without behavior
- Persistence via EPF
- Data transfer between systems

### Pattern Structure

```cpp
class OVT_TownData : Managed
{
    // Mark non-serialized fields
    [NonSerialized()]
    int m_iNonPersistedValue;

    // Serialized fields (default)
    vector location;
    int m_iAnInteger;
    string m_sName;
}
```

### Key Points

- **Extend Managed:** For garbage collection and EPF persistence
- **No Attributes:** Don't use [Attribute()] in data classes
- **NonSerialized:** Use for values that shouldn't persist
- **Strong refs:** Store as `ref OVT_TownData` in collections

---

## Lifecycle Summary

### Manager Component
1. Placed on OVT_OverthrowGameMode prefab in editor
2. Game mode loads
3. GetInstance() finds component
4. Game mode calls Init() manually
5. Game mode calls PostGameStart() after game starts
6. Exists for entire game session (singleton)

### Controller Component
1. Entity spawned or loaded from editor
2. Constructor called (both client/server)
3. Constructor registers with manager
4. Manager may call Init() if needed
5. Controller manages entity lifecycle
6. Multiple instances can exist simultaneously

### Component Class
1. Attached to entity programmatically or in prefab
2. Init() called manually when needed
3. Provides focused functionality to entity
4. Multiple components compose entity behavior

---

## Testing Component Patterns

### Manager Testing
- Verify GetInstance() returns non-null after game mode loads
- Test Init() and PostGameStart() are called
- Confirm singleton behavior (only one instance)
- Test manager exists only on server (unless replicated)

### Controller Testing
- Spawn entity and verify controller registered with manager
- Test multiple instances work independently
- Verify constructor registration works on both client/server
- Test getters/setters work correctly

### Component Testing
- Attach to entity and verify Init() called
- Test component functionality isolated from others
- Verify multiple components compose correctly

---

## Related Resources

- See `networking.md` for replication patterns
- See `persistence.md` for EPF save/load patterns
- See `memory-management.md` for strong ref usage
- See main `SKILL.md` for overview
