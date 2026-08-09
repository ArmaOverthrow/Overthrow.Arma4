# Network Replication Patterns

Comprehensive guide for network replication, RPC patterns, and JIP handling in EnforceScript.

---

## Core Principles

### Entity Identification
- **DO NOT** use EntityID across network - it differs between server and client
- **DO** use RplId for network entity references
- **Use Replication singleton** to manage entities across network

### Manager vs Controller
- **Managers:** Typically server-only, notify clients via RPC when needed
- **Controllers:** Replicate state to clients via RplProp and RPC
- **Server authority:** Server drives game state, clients are observers

---

## RplProp - Value Replication

### When to Use

Use RplProp for:
- Simple value synchronization (int, float, bool, short strings)
- Values that change occasionally
- One-way server→client state sync

### Pattern Structure

```cpp
class ExampleComponent : OVT_Component
{
    // Replicated property with change callback
    [RplProp(onRplName: "OnExampleParamChanged")]
    protected int m_iExampleParam;

    // Called on client when value changes
    protected void OnExampleParamChanged()
    {
        // Update client-side visuals/state
        // This runs ONLY on clients, not server
    }

    // Server method to update value
    void SetExampleParam(int exampleParam)
    {
        m_iExampleParam = exampleParam;
        Replication.BumpMe(); // Broadcast change to all clients
    }
}
```

### Key Points

- **Simple types only:** int, bool, float, short strings
- **onRplName:** Optional callback when value changes on client
- **Replication.BumpMe():** Call after changing value to broadcast
- **Callback runs on client only:** Not on server
- **One-way:** Always server→client

### What NOT to Replicate

❌ **Don't use RplProp for:**
- Arrays or maps
- Complex objects or classes
- Entities (IEntity or EntityID)
- Long strings (use registry with int IDs)
- Rapidly changing values (floods replication)

---

## RPC - Remote Procedure Calls

### Server to Client Patterns

#### Broadcast RPC (All Clients)

```cpp
void PerformTask(float value)
{
    // Call directly in case we are a host (server is also client)
    RpcDo_PerformTask(value);

    // Broadcast to all remote clients
    Rpc(RpcDo_PerformTask, value);
}

[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_PerformTask(float value)
{
    // All clients execute this
    // Also executes on host (called directly above)
}
```

#### Targeted RPC (Specific Client)

```cpp
void SendToSpecificClient(string playerId, float value)
{
    // Get controller for specific player
    OVT_OverthrowController clientController = OVT_Global.GetPlayers().GetController(playerId);
    if (!clientController) return; // Player offline, fail silently

    // Get component on client's controller
    OVT_MyServerTaskComponent component = OVT_MyServerTaskComponent.Cast(
        clientController.FindComponent(OVT_MyServerTaskComponent)
    );
    if (!component) return; // Component missing

    // Send to specific client
    component.PerformClientTask(value);
}

// In OVT_MyServerTaskComponent on OVT_OverthrowController
void PerformClientTask(float value)
{
    Rpc(RpcDo_PerformClientTask, value);
}

[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
void RpcDo_PerformClientTask(float value)
{
    // Only this specific client executes
}
```

### Client to Server Pattern

**CRITICAL:** Client must be "owner" of the entity to send RPC to server.

```cpp
// CLIENT SIDE: Get locally owned controller
void RequestServerAction(int param)
{
    OVT_OverthrowController controller = OVT_Global.GetController();
    if (!controller) return; // We're on dedicated server

    OVT_MyServerTaskComponent component = OVT_MyServerTaskComponent.Cast(
        controller.FindComponent(OVT_MyServerTaskComponent)
    );
    if (!component) return; // Component missing

    component.AskServerToDoTask(param);
}

// In OVT_MyServerTaskComponent on OVT_OverthrowController
void AskServerToDoTask(int param)
{
    // Check if we're already server to avoid unnecessary RPC
    if (Replication.IsServer())
    {
        RpcAsk_DoTask(param); // Direct call
    }
    else
    {
        Rpc(RpcAsk_DoTask, param); // RPC call
    }
}

[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_DoTask(int param)
{
    // Only server executes
    // Validate request here
    // Process task
    // Optionally send result back via RpcDo
}
```

### RPC Best Practices

✅ **DO:**
- Check Replication.IsServer() before RPC to avoid unnecessary call when server is host
- Use Reliable channel for important data, Unreliable for frequent updates
- Validate all client requests on server (never trust client)
- Fail silently if entity/player offline
- Use descriptive RPC names: RpcAsk_ for client→server, RpcDo_ for server→client

❌ **DON'T:**
- Send RPCs too frequently (floods network)
- Trust client-provided data without validation
- Forget to call RPC directly when server is host
- Use RPC for continuous value sync (use RplProp)

---

## RPC Channels and Receivers

### Channels
- **RplChannel.Reliable:** Guaranteed delivery, ordered (use for important data)
- **RplChannel.Unreliable:** Fast, unordered, may drop (use for frequent updates)

### Receivers
- **RplRcver.Broadcast:** All clients (server→client only)
- **RplRcver.Owner:** Specific owner client (server→client) or server (client→server)
- **RplRcver.Server:** Only server (client→server only)

---

## Join-In-Progress (JIP)

### When to Use

Use JIP replication when:
- Clients need state beyond RplProp values
- Collections (arrays/maps) need to sync on join
- Complex state must be initialized on late-join

### Pattern Structure

```cpp
class OVT_SomeComponent : OVT_Component
{
    protected ref array<ref OVT_SomeData> m_aItems;

    // RplProp values replicate automatically
    [RplProp()]
    protected int m_iSimpleValue;

    // Called on client when joining
    override bool RplLoad(ScriptBitReader reader)
    {
        // Read array size
        int count;
        reader.ReadInt(count);

        // Read each item
        m_aItems = new array<ref OVT_SomeData>();
        for (int i = 0; i < count; i++)
        {
            OVT_SomeData item = new OVT_SomeData();
            reader.ReadInt(item.m_iValue);
            reader.ReadString(item.m_sName);
            m_aItems.Insert(item);
        }

        return true;
    }

    // Called on server when client joins
    override bool RplSave(ScriptBitWriter writer)
    {
        // Write array size
        writer.WriteInt(m_aItems.Count());

        // Write each item
        foreach (OVT_SomeData item : m_aItems)
        {
            writer.WriteInt(item.m_iValue);
            writer.WriteString(item.m_sName);
        }

        return true;
    }
}
```

### Key Points

- **RplSave:** Called on server when client joins
- **RplLoad:** Called on client after connecting
- **RplProp auto-sync:** RplProp values replicate automatically, don't include in JIP
- **Use for collections:** Arrays, maps, complex state
- **Keep minimal:** Only send essential state, not derivable data

### Gotchas

- RplProp values already replicate - don't duplicate in JIP
- JIP only called on join, not during session
- Use RPC for state changes after join
- Keep JIP payload small (network bandwidth)

---

## Optimization Techniques

### Minimize Replication Frequency

❌ **Don't:**
```cpp
void UpdateEveryFrame(float value)
{
    m_fValue = value;
    Replication.BumpMe(); // Called every frame = flood!
}
```

✅ **Do:**
```cpp
void UpdateWhenSignificant(float value)
{
    // Only replicate when change is significant
    if (Math.AbsFloat(m_fValue - value) > 0.1)
    {
        m_fValue = value;
        Replication.BumpMe();
    }
}
```

### Use Appropriate Channels

```cpp
// Critical data - must arrive
[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_CriticalUpdate(int data) {}

// Frequent position updates - can drop
[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
void RpcDo_PositionUpdate(vector pos) {}
```

### Batch Updates

❌ **Don't:**
```cpp
// Multiple RPCs for related data
Rpc(RpcDo_UpdateHealth, health);
Rpc(RpcDo_UpdateArmor, armor);
Rpc(RpcDo_UpdateStamina, stamina);
```

✅ **Do:**
```cpp
// Single RPC with all data
Rpc(RpcDo_UpdateVitals, health, armor, stamina);
```

---

## Entity References Across Network

### Wrong Approach

❌ **Don't use EntityID:**
```cpp
protected EntityID m_TargetEntityID; // Different on client/server!

[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_SetTarget(EntityID targetId) // Won't work!
{
    IEntity target = GetGame().GetWorld().FindEntityByID(targetId);
}
```

### Correct Approach

✅ **Use RplId:**
```cpp
protected RplId m_TargetRplId; // Same on client/server

[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
void RpcDo_SetTarget(RplId targetRplId)
{
    m_TargetRplId = targetRplId;
    UpdateTarget();
}

void UpdateTarget()
{
    RplComponent rpl = RplComponent.Cast(
        Replication.FindItem(m_TargetRplId)
    );
    if (!rpl) return;
    IEntity target = rpl.GetEntity();
    // Use target
}
```

---

## Testing Replication

### Server Testing
- Verify RplProp values update on value change
- Test Replication.BumpMe() broadcasts changes
- Confirm RPC calls reach correct receivers
- Check server authority maintained

### Client Testing
- Join game and verify JIP loads state correctly
- Test RplProp callbacks trigger on value change
- Verify targeted RPCs reach correct client only
- Confirm broadcast RPCs reach all clients

### Host Testing
- Test server is also client (host scenario)
- Verify direct RPC calls work when IsServer() true
- Confirm RplProp updates don't duplicate on host

---

## Common Issues

### Issue: Client state out of sync
**Cause:** Forgot Replication.BumpMe() after value change
**Fix:** Call Replication.BumpMe() after setting RplProp values

### Issue: RPC not received
**Cause:** Wrong receiver type or component not on owned entity
**Fix:** Verify receiver type matches intent, check entity ownership

### Issue: Replication flooding
**Cause:** Too frequent Replication.BumpMe() or RPC calls
**Fix:** Throttle updates, batch data, use significance thresholds

### Issue: EntityID mismatch across network
**Cause:** Using EntityID instead of RplId
**Fix:** Switch to RplId for all network entity references

---

## Related Resources

- See `component-patterns.md` for component architecture
- See `persistence.md` for save/load with replication
- See `memory-management.md` for entity lifecycle
- See `common-pitfalls.md` for replication gotchas
- See main `SKILL.md` for overview
