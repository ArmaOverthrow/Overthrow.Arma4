# Memory Management and Garbage Collection

Complete guide for memory management, strong references, and entity lifecycle in EnforceScript.

---

## Garbage Collection Basics

EnforceScript uses automatic garbage collection for `Managed` classes. Objects are collected when no strong references remain.

**Critical:** Without strong references (`ref` keyword), Managed objects are collected at end of frame.

---

## Strong References

### When to Use

Use strong references (`ref` keyword) for:
- All Managed class instances stored in member variables
- Arrays of Managed classes
- Maps containing Managed classes (keys or values)
- Any Managed object that must persist beyond current frame

### Pattern Structure

```cpp
class OVT_SomeComponent : OVT_Component
{
    // Strong ref for single instance
    ref OVT_TownData m_SelectedTown;

    // Strong refs for arrays
    ref array<ref OVT_TownData> m_aTowns;

    // Strong refs for maps
    ref map<int, ref OVT_TownData> m_mTownsById;
    ref map<string, ref OVT_PlayerData> m_mPlayersByName;
}
```

### Key Points

- **ref keyword:** Creates strong reference preventing garbage collection
- **Nested refs:** Arrays/maps AND their contents both need ref
- **Member variables:** Always use ref for Managed class members
- **Automatic cleanup:** Still garbage collected when no strong refs remain

---

## What Needs Strong Refs

### ✅ Requires `ref`

```cpp
// Managed classes
ref OVT_TownData m_TownData;

// Arrays of Managed
ref array<ref OVT_TownData> m_aTowns;

// Maps with Managed keys or values
ref map<int, ref OVT_TownData> m_mTowns;
ref map<ref OVT_PlayerId, int> m_mScores;

// Any Managed-derived class
ref OVT_CustomDataClass m_Data;
```

### ❌ Does NOT need `ref`

```cpp
// Simple types (int, float, bool, string)
int m_iCount;
float m_fValue;
bool m_bEnabled;
string m_sName;

// Vectors
vector m_vPosition;

// Entities (special handling - see Entity Lifecycle section)
IEntity m_Entity; // Usually should store EntityID instead

// EntityID
EntityID m_EntityId;

// Arrays/maps of simple types (but still use ref for the collection)
ref array<int> m_aIds; // ref for array, not int
ref map<string, int> m_mCounts; // ref for map, not int/string
```

---

## Entity Lifecycle

### The Problem

Entities (IEntity) can be deleted at any time. Storing direct entity references can lead to null pointer crashes.

### The Solution

Store EntityID and fetch entity when needed, always checking for existence.

### Pattern Structure

```cpp
class OVT_SomeComponent : OVT_Component
{
    // Store EntityID, not IEntity
    protected EntityID m_TargetEntityId;

    void SetTargetEntity(IEntity entity)
    {
        if (!entity)
        {
            m_TargetEntityId = EntityID.INVALID;
            return;
        }
        m_TargetEntityId = entity.GetID();
    }

    IEntity GetTargetEntity()
    {
        // Fetch entity when needed
        IEntity entity = GetGame().GetWorld().FindEntityByID(m_TargetEntityId);
        if (!entity)
        {
            // Entity no longer exists - clean up
            m_TargetEntityId = EntityID.INVALID;
            return null;
        }
        return entity;
    }

    void UseTargetEntity()
    {
        IEntity entity = GetTargetEntity();
        if (!entity) return; // Entity gone, abort

        // Safe to use entity now
        entity.DoSomething();
    }
}
```

### Key Points

- **Store EntityID:** Not IEntity reference
- **Fetch when needed:** Call FindEntityByID() each time
- **Always check null:** Entity may have been deleted
- **Clean up:** Clear EntityID when entity gone
- **EntityID.INVALID:** Use for "no entity" state

---

## Collections of Entities

### Wrong Approach

❌ **Storing IEntity directly:**
```cpp
ref array<IEntity> m_aEntities; // Can become null pointers!
```

### Correct Approach

✅ **Store EntityID, fetch when needed:**
```cpp
ref array<EntityID> m_aEntityIds; // Store IDs

void ProcessEntities()
{
    // Clean up deleted entities while iterating
    for (int i = m_aEntityIds.Count() - 1; i >= 0; i--)
    {
        IEntity entity = GetGame().GetWorld().FindEntityByID(m_aEntityIds[i]);
        if (!entity)
        {
            // Entity deleted, remove from list
            m_aEntityIds.Remove(i);
            continue;
        }

        // Safe to use entity
        entity.DoSomething();
    }
}
```

### Key Points

- **Iterate backwards:** When removing items during iteration
- **Check existence:** Every time before using entity
- **Clean up list:** Remove invalid EntityIDs
- **Performance:** Acceptable overhead for safety

---

## Network Entity References

### Local vs Network

- **EntityID:** Only valid locally, differs between server/client
- **RplId:** Same across network, use for networked entity references

### Pattern for Networked Entities

```cpp
class OVT_NetworkComponent : OVT_Component
{
    // Network-safe entity reference
    protected RplId m_TargetRplId;

    void SetTargetEntity(IEntity entity)
    {
        if (!entity)
        {
            m_TargetRplId = RplId.Invalid();
            return;
        }

        RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
        if (!rpl)
        {
            m_TargetRplId = RplId.Invalid();
            return;
        }

        m_TargetRplId = rpl.Id();
    }

    IEntity GetTargetEntity()
    {
        RplComponent rpl = RplComponent.Cast(Replication.FindItem(m_TargetRplId));
        if (!rpl) return null;

        return rpl.GetEntity();
    }
}
```

### Key Points

- **RplId for network:** Use when entity referenced across network
- **EntityID for local:** Use for local-only references
- **Replication.FindItem():** Convert RplId to entity
- **Still check null:** Entity may not exist or not replicated yet

---

## Cleanup Patterns

### Component Cleanup

```cpp
class OVT_SomeComponent : OVT_Component
{
    ref array<ref OVT_SomeData> m_aData;
    ref map<int, ref OVT_SomeData> m_mData;

    override void OnDelete(IEntity owner)
    {
        // Clear collections to release strong refs
        m_aData = null;
        m_mData = null;

        // Call parent
        super.OnDelete(owner);
    }
}
```

### Entity References Cleanup

```cpp
void CleanupDeletedEntities()
{
    // Iterate backwards when removing
    for (int i = m_aEntityIds.Count() - 1; i >= 0; i--)
    {
        IEntity entity = GetGame().GetWorld().FindEntityByID(m_aEntityIds[i]);
        if (!entity)
        {
            m_aEntityIds.Remove(i);
        }
    }
}
```

### Periodic Cleanup

```cpp
void SetupPeriodicCleanup()
{
    GetGame().GetCallqueue().CallLater(
        CleanupDeletedEntities,
        30000, // 30 seconds
        true   // Repeat
    );
}
```

---

## Common Memory Issues

### Issue: Data disappears after one frame

**Cause:** Missing `ref` keyword for Managed class
**Symptoms:** Object exists during creation, null next frame
**Fix:** Add `ref` to member variable declaration

```cpp
// Wrong
OVT_SomeData m_Data; // Garbage collected!

// Right
ref OVT_SomeData m_Data; // Persists
```

### Issue: Null pointer crash on entity access

**Cause:** Storing IEntity directly, entity was deleted
**Symptoms:** Crash or null reference error
**Fix:** Store EntityID, fetch and check each time

```cpp
// Wrong
IEntity m_Entity; // May become invalid
m_Entity.DoSomething(); // Crash!

// Right
EntityID m_EntityId;
IEntity entity = GetGame().GetWorld().FindEntityByID(m_EntityId);
if (entity) entity.DoSomething(); // Safe
```

### Issue: Array/Map elements disappear

**Cause:** Missing `ref` on array/map itself OR on elements
**Symptoms:** Collection exists but elements are null
**Fix:** Use `ref` for both collection AND contents

```cpp
// Wrong
array<ref OVT_SomeData> m_aData; // Collection not strong ref!
ref array<OVT_SomeData> m_aData; // Elements not strong ref!

// Right
ref array<ref OVT_SomeData> m_aData; // Both strong refs
```

---

## Best Practices

### ✅ DO:

- **Always use ref:** For Managed class members, arrays, maps
- **Store EntityID:** Not IEntity for long-term references
- **Check entity existence:** Before every use
- **Clean up collections:** Remove deleted entities periodically
- **Use RplId for network:** When entity referenced across network
- **Null collections on cleanup:** In OnDelete() to release refs
- **Document ownership:** Who owns the strong ref

### ❌ DON'T:

- **Store IEntity long-term:** Will become invalid when entity deleted
- **Forget ref keyword:** For Managed classes in collections
- **Skip existence checks:** Before using fetched entities
- **Use EntityID across network:** Use RplId instead
- **Leak strong refs:** Clear references when object destroyed
- **Assume entity exists:** Always check for null

---

## Memory Usage Optimization

### Reuse Objects

```cpp
// Reuse existing object instead of creating new
if (!m_Data)
{
    m_Data = new OVT_SomeData();
}
m_Data.Reset(); // Clear state instead of new instance
```

### Pool Objects

```cpp
class OVT_ObjectPool : Managed
{
    ref array<ref OVT_PooledObject> m_aAvailable;
    ref array<ref OVT_PooledObject> m_aInUse;

    OVT_PooledObject Acquire()
    {
        if (m_aAvailable.Count() > 0)
        {
            OVT_PooledObject obj = m_aAvailable[0];
            m_aAvailable.Remove(0);
            m_aInUse.Insert(obj);
            return obj;
        }

        // Create new if pool empty
        OVT_PooledObject obj = new OVT_PooledObject();
        m_aInUse.Insert(obj);
        return obj;
    }

    void Release(OVT_PooledObject obj)
    {
        int idx = m_aInUse.Find(obj);
        if (idx >= 0)
        {
            m_aInUse.Remove(idx);
            obj.Reset();
            m_aAvailable.Insert(obj);
        }
    }
}
```

### Clear Large Collections

```cpp
void ClearLargeData()
{
    // Clear instead of null if reusing
    m_aLargeArray.Clear();

    // Or null if done with it
    m_aLargeArray = null;
}
```

---

## Testing Memory Management

### Test Strong Refs
1. Create Managed object without ref
2. Wait one frame
3. Verify object is null (should be collected)
4. Add ref keyword
5. Verify object persists across frames

### Test Entity Lifecycle
1. Store EntityID of spawned entity
2. Delete the entity
3. Attempt to fetch via FindEntityByID
4. Verify returns null
5. Confirm no crashes

### Test Collection Cleanup
1. Add entities to collection
2. Delete some entities externally
3. Run cleanup method
4. Verify deleted entities removed from collection

---

## Related Resources

- See `component-patterns.md` for component lifecycle
- See `networking.md` for RplId usage
- See `persistence.md` for strong refs in save data
- See `common-pitfalls.md` for memory gotchas
- See main `SKILL.md` for overview
