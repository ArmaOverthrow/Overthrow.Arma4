# OVT_Global Access Patterns

Complete guide for accessing managers and systems via OVT_Global static class.

---

## Overview

OVT_Global provides centralized static methods to access all manager singletons and key systems. Use OVT_Global instead of calling GetInstance() directly for cleaner, more consistent code.

---

## Available Accessors

### Manager Access

```cpp
class OVT_Global
{
    // Manager components
    static OVT_TownManagerComponent GetTowns();
    static OVT_BaseManagerComponent GetBases();
    static OVT_FactionManagerComponent GetFactions();
    static OVT_EconomyManagerComponent GetEconomy();
    static OVT_CampManagerComponent GetCamps();
    // ... other managers ...

    // Player/Client systems
    static OVT_PlayerManagerComponent GetPlayers();
    static OVT_OverthrowController GetController(); // Local client controller
    static OVT_UIManagerComponent GetUI(); // Local UI manager

    // Server system (deprecated - use components on controller instead)
    static OVT_OverthrowServerComponent GetServer(); // Legacy

    // Game mode
    static OVT_OverthrowGameMode GetGameMode();
}
```

---

## Usage Patterns

### Accessing Managers

```cpp
void SomeMethod()
{
    // Get town manager
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (!towns) return; // Manager not loaded

    // Use manager
    OVT_TownController town = towns.GetTownById(15);
}
```

### Chaining Calls

```cpp
void GetTownName(int townId)
{
    OVT_TownController town = OVT_Global.GetTowns().GetTownById(townId);
    if (!town) return;

    return town.GetName();
}
```

### Always Check for Null

```cpp
void UpdateEconomy()
{
    OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
    if (!economy) return; // Not loaded or doesn't exist

    economy.UpdatePrices();
}
```

---

## Client-Specific Access

### Getting Local Controller

```cpp
void ClientMethod()
{
    // Get locally controlled entity
    OVT_OverthrowController controller = OVT_Global.GetController();
    if (!controller) return; // We're on dedicated server

    // Access components on controller
    OVT_SomeServerComponent component = OVT_SomeServerComponent.Cast(
        controller.FindComponent(OVT_SomeServerComponent)
    );

    if (component)
    {
        component.DoClientTask();
    }
}
```

### Getting UI Manager

```cpp
void ShowUI()
{
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return; // No UI (dedicated server)

    OVT_SomeContext context = OVT_SomeContext.Cast(ui.GetContext(OVT_SomeContext));
    if (!context) return;

    ui.ShowContext(OVT_SomeContext);
}
```

---

## Server-Specific Access

### Server-Only Managers

Most managers exist only on server:

```cpp
void ServerMethod()
{
    // These return null on client
    OVT_AIManagerComponent ai = OVT_Global.GetAI();
    if (!ai) return; // Client or not loaded

    ai.SpawnAIGroup();
}
```

### Checking Server vs Client

```cpp
void SomeMethod()
{
    if (Replication.IsServer())
    {
        // Server-side logic
        OVT_Global.GetEconomy().ProcessTransactions();
    }
    else
    {
        // Client-side logic
        OVT_Global.GetUI().UpdateDisplay();
    }
}
```

---

## vs Direct GetInstance()

### ❌ Don't Use GetInstance() Directly

```cpp
void OldWay()
{
    OVT_TownManagerComponent towns = OVT_TownManagerComponent.GetInstance();
    if (!towns) return;

    // Verbose, inconsistent
}
```

### ✅ Use OVT_Global

```cpp
void NewWay()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (!towns) return;

    // Clean, consistent
}
```

**Benefits:**
- Shorter, cleaner code
- Consistent access pattern
- Easier to refactor
- Centralized access point

---

## Adding New Manager Accessors

### Step 1: Create Manager

```cpp
class OVT_NewManagerComponent: OVT_Component
{
    static OVT_NewManagerComponent s_Instance;

    static OVT_NewManagerComponent GetInstance()
    {
        if (!s_Instance)
        {
            BaseGameMode pGameMode = GetGame().GetGameMode();
            if (pGameMode)
            {
                s_Instance = OVT_NewManagerComponent.Cast(
                    pGameMode.FindComponent(OVT_NewManagerComponent)
                );
            }
        }
        return s_Instance;
    }
}
```

### Step 2: Add to OVT_Global

```cpp
// In OVT_Global.c
class OVT_Global
{
    // ... existing accessors ...

    static OVT_NewManagerComponent GetNewManager()
    {
        return OVT_NewManagerComponent.GetInstance();
    }
}
```

### Step 3: Use Everywhere

```cpp
void UseNewManager()
{
    OVT_NewManagerComponent manager = OVT_Global.GetNewManager();
    if (!manager) return;

    manager.DoSomething();
}
```

---

## Common Access Patterns

### Pattern: Get and Use Manager

```cpp
void ProcessTowns()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (!towns) return;

    array<OVT_TownController> allTowns = towns.GetAllTowns();
    foreach (OVT_TownController town : allTowns)
    {
        ProcessTown(town);
    }
}
```

### Pattern: Cross-Manager Communication

```cpp
void TransferResources(int fromTownId, int toTownId, int amount)
{
    // Get both managers
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();

    if (!towns || !economy) return;

    // Get towns
    OVT_TownController fromTown = towns.GetTownById(fromTownId);
    OVT_TownController toTown = towns.GetTownById(toTownId);

    if (!fromTown || !toTown) return;

    // Process transfer via economy
    economy.TransferResources(fromTown, toTown, amount);
}
```

### Pattern: Client Request to Server

```cpp
void ClientRequestAction()
{
    // Get local controller
    OVT_OverthrowController controller = OVT_Global.GetController();
    if (!controller) return; // We're on server

    // Get component on controller
    OVT_ServerRequestComponent requests = OVT_ServerRequestComponent.Cast(
        controller.FindComponent(OVT_ServerRequestComponent)
    );

    if (!requests) return;

    // Send request
    requests.RequestAction();
}
```

---

## Best Practices

### ✅ DO:

- **Always use OVT_Global:** For accessing managers
- **Check for null:** Every time, managers may not exist
- **Short variable names:** `towns`, `economy`, etc.
- **Early return:** If manager null, return early
- **Add new accessors:** When creating new managers
- **Document managers:** What they manage, when available

### ❌ DON'T:

- **Call GetInstance() directly:** Use OVT_Global instead
- **Assume manager exists:** Always check for null
- **Cache manager references:** Get when needed, managers are singletons
- **Skip null checks:** Will crash if manager doesn't exist
- **Use on wrong side:** Some managers server-only, UI client-only

---

## Null Check Strategies

### Strategy 1: Early Return

```cpp
void Method()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (!towns) return;

    // Use towns
}
```

### Strategy 2: Guard Pattern

```cpp
void Method()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (towns)
    {
        // Use towns
    }
}
```

### Strategy 3: Multiple Managers

```cpp
void Method()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();

    if (!towns || !economy) return;

    // Use both
}
```

---

## Testing Global Access

### Verify Manager Available

```cpp
void TestManagerAccess()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (!towns)
    {
        Print("ERROR: Town manager not available!", LogLevel.ERROR);
        return;
    }

    Print("Town manager available");
}
```

### Verify Client-Side Access

```cpp
void TestClientAccess()
{
    // Should return null on dedicated server
    OVT_UIManagerComponent ui = OVT_Global.GetUI();

    if (Replication.IsServer() && !Replication.IsRunning())
    {
        // Dedicated server - UI should be null
        if (ui)
        {
            Print("ERROR: UI exists on dedicated server!", LogLevel.ERROR);
        }
    }
    else
    {
        // Client or host - UI should exist
        if (!ui)
        {
            Print("ERROR: UI missing on client!", LogLevel.ERROR);
        }
    }
}
```

---

## Related Resources

- See `managers.md` for manager pattern
- See `controllers.md` for controller pattern
- See `enforcescript-patterns/component-patterns.md` for base patterns
- See main `SKILL.md` for overview
