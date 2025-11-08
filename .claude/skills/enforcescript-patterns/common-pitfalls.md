# Common Pitfalls and Anti-Patterns

Comprehensive guide to common mistakes and how to avoid them in EnforceScript development.

---

## Language-Specific Pitfalls

### No Ternary Operators

**The Problem:** EnforceScript does NOT support ternary operators.

❌ **Don't do this:**
```cpp
int value = condition ? 10 : 20; // COMPILE ERROR!
string text = isActive ? "Active" : "Inactive"; // WON'T WORK!
```

✅ **Do this instead:**
```cpp
int value;
if (condition)
{
    value = 10;
}
else
{
    value = 20;
}

string text;
if (isActive)
{
    text = "Active";
}
else
{
    text = "Inactive";
}
```

**Why:** EnforceScript parser doesn't recognize ternary operator syntax. Always use full if/else blocks.

---

## Memory Management Pitfalls

### Forgot Strong Reference

**The Problem:** Managed classes without `ref` keyword get garbage collected.

❌ **Don't do this:**
```cpp
class OVT_SomeComponent : OVT_Component
{
    OVT_SomeData m_Data; // No ref! Garbage collected!

    void Init()
    {
        m_Data = new OVT_SomeData();
        m_Data.value = 100; // Works now...
    }

    void LaterMethod()
    {
        Print(m_Data.value); // CRASH! m_Data is null
    }
}
```

✅ **Do this instead:**
```cpp
class OVT_SomeComponent : OVT_Component
{
    ref OVT_SomeData m_Data; // Strong ref!

    void Init()
    {
        m_Data = new OVT_SomeData();
        m_Data.value = 100;
    }

    void LaterMethod()
    {
        Print(m_Data.value); // Works! m_Data persists
    }
}
```

**Why:** Managed classes are garbage collected at end of frame unless strong-referenced.

### Missing ref in Collections

**The Problem:** Forgot `ref` on collection or elements.

❌ **Don't do this:**
```cpp
array<ref OVT_SomeData> m_aData; // Collection not strong ref!
ref array<OVT_SomeData> m_aData; // Elements not strong ref!
```

✅ **Do this instead:**
```cpp
ref array<ref OVT_SomeData> m_aData; // Both collection AND elements!
ref map<int, ref OVT_SomeData> m_mData; // Same for maps!
```

**Why:** Need strong ref for BOTH the collection AND the Managed class elements.

---

## Entity Reference Pitfalls

### Storing IEntity Long-Term

**The Problem:** Entities can be deleted, making stored references invalid.

❌ **Don't do this:**
```cpp
class OVT_SomeComponent : OVT_Component
{
    protected IEntity m_TargetEntity; // Can become invalid!

    void SetTarget(IEntity target)
    {
        m_TargetEntity = target;
    }

    void UseTarget()
    {
        m_TargetEntity.DoSomething(); // CRASH if entity deleted!
    }
}
```

✅ **Do this instead:**
```cpp
class OVT_SomeComponent : OVT_Component
{
    protected EntityID m_TargetEntityId;

    void SetTarget(IEntity target)
    {
        if (!target)
        {
            m_TargetEntityId = EntityID.INVALID;
            return;
        }
        m_TargetEntityId = target.GetID();
    }

    void UseTarget()
    {
        IEntity target = GetGame().GetWorld().FindEntityByID(m_TargetEntityId);
        if (!target) return; // Entity gone, safe to handle

        target.DoSomething(); // Safe!
    }
}
```

**Why:** Entities can be deleted at any time. Store EntityID and fetch when needed, always checking existence.

### Using EntityID Across Network

**The Problem:** EntityID differs between server and client.

❌ **Don't do this:**
```cpp
[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_SetTarget(EntityID targetId) // Different on client/server!
{
    IEntity target = GetGame().GetWorld().FindEntityByID(targetId);
    // Won't find entity on client!
}
```

✅ **Do this instead:**
```cpp
[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_SetTarget(RplId targetRplId) // Same on client/server!
{
    RplComponent rpl = RplComponent.Cast(Replication.FindItem(targetRplId));
    if (!rpl) return;

    IEntity target = rpl.GetEntity();
    // Works on both client and server!
}
```

**Why:** EntityID is locally assigned and differs between server/client. RplId is network-synchronized.

---

## Replication Pitfalls

### Replicating Arrays/Maps with RplProp

**The Problem:** RplProp doesn't support complex types.

❌ **Don't do this:**
```cpp
[RplProp()]
ref array<int> m_aValues; // Won't replicate!

[RplProp()]
ref OVT_SomeData m_Data; // Won't replicate!
```

✅ **Do this instead:**
```cpp
// Use RPC for collections
void UpdateValues(array<int> values)
{
    m_aValues = values;
    Rpc(RpcDo_UpdateValues, values);
}

[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_UpdateValues(array<int> values)
{
    m_aValues = values;
}

// Or use JIP for collections
override bool RplSave(ScriptBitWriter writer)
{
    writer.WriteInt(m_aValues.Count());
    foreach (int value : m_aValues)
    {
        writer.WriteInt(value);
    }
    return true;
}

override bool RplLoad(ScriptBitReader reader)
{
    int count;
    reader.ReadInt(count);

    m_aValues = new array<int>();
    for (int i = 0; i < count; i++)
    {
        int value;
        reader.ReadInt(value);
        m_aValues.Insert(value);
    }
    return true;
}
```

**Why:** RplProp only supports simple types (int, float, bool, short strings). Use RPC or JIP for complex data.

### Forgetting Replication.BumpMe()

**The Problem:** RplProp values don't update without BumpMe().

❌ **Don't do this:**
```cpp
[RplProp()]
protected int m_iValue;

void SetValue(int value)
{
    m_iValue = value; // Forgot to broadcast!
}
```

✅ **Do this instead:**
```cpp
[RplProp()]
protected int m_iValue;

void SetValue(int value)
{
    m_iValue = value;
    Replication.BumpMe(); // Broadcast change!
}
```

**Why:** Changing RplProp value doesn't automatically replicate. Must call Replication.BumpMe() to broadcast.

### Replication Flooding

**The Problem:** Too frequent replication updates flood network.

❌ **Don't do this:**
```cpp
void UpdateEveryFrame(float value)
{
    m_fValue = value;
    Replication.BumpMe(); // EVERY FRAME! Network flood!
}
```

✅ **Do this instead:**
```cpp
protected float m_fLastReplicatedValue;

void UpdateThrottled(float value)
{
    m_fValue = value;

    // Only replicate significant changes
    if (Math.AbsFloat(m_fValue - m_fLastReplicatedValue) > 0.1)
    {
        m_fLastReplicatedValue = m_fValue;
        Replication.BumpMe();
    }
}
```

**Why:** Frequent replication floods network bandwidth. Throttle updates to significant changes only.

---

## RPC Pitfalls

### Wrong RPC Direction

**The Problem:** Using wrong receiver for RPC direction.

❌ **Don't do this:**
```cpp
// Trying to send client→server with Broadcast
[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] // Wrong!
void RpcAsk_ServerAction()
{
    // This sends to all clients, not server!
}
```

✅ **Do this instead:**
```cpp
// Client→Server: Use Server receiver
[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_ServerAction()
{
    // Correctly sends to server
}

// Server→Client: Use Broadcast or Owner
[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_ClientAction()
{
    // Correctly sends to all clients
}
```

**Why:** Receiver type determines RPC direction. Server for client→server, Broadcast/Owner for server→client.

### Forgetting Host Check

**The Problem:** Unnecessary RPC when server is also client (host).

❌ **Don't do this:**
```cpp
void RequestAction()
{
    Rpc(RpcAsk_Action); // Always RPC, even on host!
}

[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_Action()
{
    // Process action
}
```

✅ **Do this instead:**
```cpp
void RequestAction()
{
    if (Replication.IsServer())
    {
        RpcAsk_Action(); // Direct call on host
    }
    else
    {
        Rpc(RpcAsk_Action); // RPC from client
    }
}

[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_Action()
{
    // Process action
}
```

**Why:** When server is host (also a client), avoid unnecessary RPC by calling directly.

### Client Not Owner

**The Problem:** Client trying to RPC on entity they don't own.

❌ **Don't do this:**
```cpp
void ClientRequestAction()
{
    // Try to RPC on some other player's controller
    OVT_OverthrowController otherController = GetSomeOtherController();
    otherController.DoSomething(); // Won't work! Not owner!
}
```

✅ **Do this instead:**
```cpp
void ClientRequestAction()
{
    // Only RPC on OWN controller
    OVT_OverthrowController myController = OVT_Global.GetController();
    if (!myController) return;

    myController.DoSomething(); // Works! We own this!
}
```

**Why:** Client can only send RPC on entities they own (their controller). Use server for inter-player communication.

---

## Persistence Pitfalls

### Persisting IEntity

**The Problem:** Trying to save IEntity reference directly.

❌ **Don't do this:**
```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    IEntity m_SavedEntity; // Will crash or corrupt save!

    override void ReadFrom(OVT_SomeComponent component)
    {
        m_SavedEntity = component.GetEntity();
    }
}
```

✅ **Do this instead:**
```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    EntityID m_SavedEntityId; // Save ID, not entity!

    override void ReadFrom(OVT_SomeComponent component)
    {
        IEntity entity = component.GetEntity();
        if (entity)
        {
            m_SavedEntityId = entity.GetID();
        }
        else
        {
            m_SavedEntityId = EntityID.INVALID;
        }
    }

    override void ApplyTo(OVT_SomeComponent component)
    {
        if (m_SavedEntityId != EntityID.INVALID)
        {
            IEntity entity = GetGame().GetWorld().FindEntityByID(m_SavedEntityId);
            component.SetEntity(entity); // May be null if entity doesn't exist
        }
    }
}
```

**Why:** IEntity cannot be serialized. Save EntityID instead and fetch entity on load.

### Forgetting Platform Guards

**The Problem:** EPF code runs on console platforms that don't support it.

❌ **Don't do this:**
```cpp
[EPF_ComponentSaveDataType(OVT_SomeComponent)]
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    // Will crash on Xbox/PlayStation!
}
```

✅ **Do this instead:**
```cpp
#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_SomeComponent)]
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    // Only compiled for PC
}

#endif
```

**Why:** Console platforms (Xbox/PlayStation) don't support FileIO. EPF must be disabled with platform guards.

---

## Component Pitfalls

### Expecting Init() to be Called Automatically

**The Problem:** Assuming Init() is called by engine.

❌ **Don't do this:**
```cpp
class OVT_SomeManagerComponent : OVT_Component
{
    ref array<int> m_aValues;

    void Init(IEntity owner)
    {
        m_aValues = new array<int>(); // Won't be called!
    }

    void DoSomething()
    {
        m_aValues.Insert(5); // CRASH! Array is null!
    }
}
```

✅ **Do this instead:**
```cpp
class OVT_SomeManagerComponent : OVT_Component
{
    ref array<int> m_aValues = new array<int>(); // Initialize in declaration

    void Init(IEntity owner)
    {
        // Init() only called if game mode explicitly calls it
        // Don't rely on it for critical initialization
    }

    static OVT_SomeManagerComponent GetInstance()
    {
        if (!s_Instance)
        {
            BaseGameMode pGameMode = GetGame().GetGameMode();
            if (pGameMode)
            {
                s_Instance = OVT_SomeManagerComponent.Cast(
                    pGameMode.FindComponent(OVT_SomeManagerComponent)
                );

                // Call Init() manually if needed
                if (s_Instance)
                {
                    s_Instance.Init(pGameMode);
                }
            }
        }
        return s_Instance;
    }
}
```

**Why:** Init() is not automatically called by engine. Either initialize in declaration or call manually.

### Not Checking GetInstance() for Null

**The Problem:** Assuming singleton always exists.

❌ **Don't do this:**
```cpp
void DoSomething()
{
    OVT_SomeManager.GetInstance().DoAction(); // May crash if null!
}
```

✅ **Do this instead:**
```cpp
void DoSomething()
{
    OVT_SomeManager manager = OVT_SomeManager.GetInstance();
    if (!manager) return; // Safe handling

    manager.DoAction();
}
```

**Why:** GetInstance() may return null if game mode not loaded or component not attached.

---

## UI Pitfalls

### Not Checking GetUI()

**The Problem:** Trying to access UI on server.

❌ **Don't do this:**
```cpp
void ShowUI()
{
    OVT_Global.GetUI().ShowContext(OVT_SomeContext); // Crash on dedicated server!
}
```

✅ **Do this instead:**
```cpp
void ShowUI()
{
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return; // No UI on server

    ui.ShowContext(OVT_SomeContext);
}
```

**Why:** Dedicated servers don't have UI manager. Always check GetUI() returns non-null.

### Widget Memory Leaks

**The Problem:** Not removing dynamically created widgets.

❌ **Don't do this:**
```cpp
override void OnShow()
{
    // Create widgets
    for (int i = 0; i < 10; i++)
    {
        Widget item = GetGame().GetWorkspace().CreateWidgets(...);
    }
    // Never removed! Memory leak!
}
```

✅ **Do this instead:**
```cpp
ref array<Widget> m_aCreatedWidgets;

override void OnShow()
{
    m_aCreatedWidgets = new array<Widget>();

    for (int i = 0; i < 10; i++)
    {
        Widget item = GetGame().GetWorkspace().CreateWidgets(...);
        m_aCreatedWidgets.Insert(item);
    }
}

override void OnHide()
{
    foreach (Widget widget : m_aCreatedWidgets)
    {
        widget.RemoveFromHierarchy();
    }
    m_aCreatedWidgets.Clear();
}
```

**Why:** Dynamically created widgets must be explicitly removed to prevent memory leaks.

---

## Testing Pitfalls

### Not Testing on Dedicated Server

**The Problem:** Code works in editor/host but fails on dedicated server.

**Common causes:**
- UI code without GetUI() check
- Client-side assumptions in server code
- Missing Replication.IsServer() checks

**Fix:** Always test on dedicated server build, not just editor/host.

### Not Testing Late-Join

**The Problem:** JIP replication not implemented or broken.

**Symptoms:**
- First players work fine
- Late-joining players have missing/broken state

**Fix:** Test with one client joining late after game state established.

---

## Checklist for Common Pitfalls

Before committing code, verify:

- [ ] No ternary operators used
- [ ] All Managed classes have `ref` keyword
- [ ] Arrays/maps have `ref` for both collection and elements
- [ ] EntityID stored instead of IEntity long-term
- [ ] RplId used for network entity references
- [ ] RplProp only used for simple types
- [ ] Replication.BumpMe() called after RplProp changes
- [ ] RPC has correct receiver type (Server vs Broadcast/Owner)
- [ ] Host check before client→server RPC
- [ ] EPF code wrapped in #ifndef PLATFORM_CONSOLE
- [ ] IEntity not persisted in SaveData
- [ ] GetInstance() results checked for null
- [ ] GetUI() checked before UI operations
- [ ] Dynamically created widgets removed
- [ ] Tested on dedicated server
- [ ] Tested with late-joining clients

---

## Related Resources

- See `component-patterns.md` for component lifecycle
- See `networking.md` for replication patterns
- See `persistence.md` for save/load patterns
- See `memory-management.md` for strong refs
- See `ui-patterns.md` for UI context patterns
- See main `SKILL.md` for overview
