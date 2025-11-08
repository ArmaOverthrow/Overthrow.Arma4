# Coding Standards and Conventions

Complete guide for Overthrow coding standards, naming conventions, and documentation style.

---

## Class Naming

### Prefix Convention

**All Overthrow classes:** `OVT_` prefix

```cpp
class OVT_TownManagerComponent
class OVT_BaseController
class OVT_ShopContext
class OVT_TownData
class OVT_RecruitAction
```

**Why:** Distinguishes Overthrow code from base game and other mods

---

## Member Variables

### Prefix: `m_`

**All member variables:** Start with `m_`

```cpp
class OVT_SomeClass
{
    int m_iValue;
    string m_sName;
    bool m_bEnabled;
}
```

---

## Type-Specific Prefixes

### Integer: `m_i`

```cpp
int m_iCount;
int m_iTownId;
int m_iPopulation;
```

### Float: `m_f`

```cpp
float m_fValue;
float m_fProductionRate;
float m_fDistance;
```

### String: `m_s`

```cpp
string m_sName;
string m_sDescription;
string m_sPlayerId;
```

### Boolean: `m_b`

```cpp
bool m_bEnabled;
bool m_bIsActive;
bool m_bHasCompleted;
```

### Array: `m_a`

```cpp
ref array<int> m_aIds;
ref array<string> m_aPlayerNames;
ref array<ref OVT_TownData> m_aTowns;
```

### Map: `m_m`

```cpp
ref map<int, string> m_mNamesById;
ref map<string, int> m_mScoresByPlayer;
ref map<int, ref OVT_TownData> m_mTownsById;
```

### Vector: `m_v`

```cpp
vector m_vPosition;
vector m_vDirection;
vector m_vSpawnPoint;
```

---

## Static Variables

### Static Instance: `s_Instance`

```cpp
class OVT_SomeManagerComponent
{
    static OVT_SomeManagerComponent s_Instance;

    static OVT_SomeManagerComponent GetInstance()
    {
        return s_Instance;
    }
}
```

### Other Static Variables: `s_` prefix

```cpp
static const int s_MAX_TOWNS = 100;
static bool s_IsInitialized = false;
```

---

## Protection Levels

### Protected Members

**Default:** Use `protected` for member variables

```cpp
class OVT_SomeComponent
{
    protected int m_iValue;
    protected string m_sName;

    // Public getters/setters
    int GetValue() { return m_iValue; }
    void SetValue(int value) { m_iValue = value; }
}
```

**Why:** Encapsulation, controlled access via methods

### Public Members

**Rare:** Only when direct access needed

```cpp
class OVT_SomeData : Managed
{
    // Data classes may have public members
    int value;
    string name;
}
```

### Private Members

**Not recommended:** EnforceScript `private` keyword has limited use

---

## Method Naming

### Verbs for Actions

```cpp
void InitializeData()
void ProcessTransaction()
void UpdateEconomy()
void SpawnEntity()
void DestroyBase()
```

### Get/Set for Properties

```cpp
int GetTownId()
void SetTownId(int townId)

string GetName()
void SetName(string name)

bool IsEnabled()
void SetEnabled(bool enabled)
```

### On Prefix for Callbacks

```cpp
void OnFactionChanged()
void OnPlayerJoined()
void OnTownCaptured()
```

### Event Handler Pattern

```cpp
protected void OnFactionChanged()
{
    // Handle faction change
}
```

---

## Documentation Style

### Doxygen Comments

**Use `//!` for documentation:**

```cpp
//! Town manager component
//! Manages all towns in the game world
class OVT_TownManagerComponent: OVT_Component
{
    //! Array of all registered towns
    protected ref array<OVT_TownController> m_aTowns;

    //! Register a new town controller
    //! @param controller The town controller to register
    void RegisterTown(OVT_TownController controller)
    {
        if (!controller) return;
        m_aTowns.Insert(controller);
    }

    //! Get town by ID
    //! @param townId The unique town identifier
    //! @return The town controller, or null if not found
    OVT_TownController GetTownById(int townId)
    {
        // Implementation...
    }
}
```

### Method Documentation

```cpp
//! Process all pending transactions
//! @param deltaTime Time since last update in seconds
//! @return Number of transactions processed
int ProcessTransactions(float deltaTime)
{
    // Implementation...
}
```

### Parameter Documentation

```cpp
//! Transfer resources between towns
//! @param fromTownId Source town ID
//! @param toTownId Destination town ID
//! @param resourceType Type of resource to transfer
//! @param amount Amount to transfer
//! @return True if transfer successful
bool TransferResources(int fromTownId, int toTownId, string resourceType, int amount)
{
    // Implementation...
}
```

---

## File Structure

### Class File Template

```cpp
//! Brief description of what this class does
class OVT_SomeComponentClass: OVT_ComponentClass {};

//! Detailed description of component purpose and usage
class OVT_SomeComponent: OVT_Component
{
    //-----------------------------------------------------------------------
    // ATTRIBUTES
    //-----------------------------------------------------------------------

    [Attribute("0", desc: "Parameter description")]
    protected int m_iSomeParameter;

    //-----------------------------------------------------------------------
    // MEMBER VARIABLES
    //-----------------------------------------------------------------------

    protected ref array<int> m_aValues;
    protected ref map<string, int> m_mData;

    //-----------------------------------------------------------------------
    // STATIC
    //-----------------------------------------------------------------------

    static OVT_SomeComponent s_Instance;

    static OVT_SomeComponent GetInstance()
    {
        // Implementation...
    }

    //-----------------------------------------------------------------------
    // LIFECYCLE
    //-----------------------------------------------------------------------

    void OVT_SomeComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
    {
        // Constructor
    }

    void ~OVT_SomeComponent()
    {
        // Destructor
    }

    void Init(IEntity owner)
    {
        // Initialization
    }

    //-----------------------------------------------------------------------
    // PUBLIC METHODS
    //-----------------------------------------------------------------------

    void DoSomething()
    {
        // Public method
    }

    //-----------------------------------------------------------------------
    // GETTERS/SETTERS
    //-----------------------------------------------------------------------

    int GetValue() { return m_iValue; }
    void SetValue(int value) { m_iValue = value; }

    //-----------------------------------------------------------------------
    // PROTECTED METHODS
    //-----------------------------------------------------------------------

    protected void InternalMethod()
    {
        // Protected helper
    }

    //-----------------------------------------------------------------------
    // EVENT HANDLERS
    //-----------------------------------------------------------------------

    protected void OnSomeEvent()
    {
        // Event handler
    }
}
```

---

## Code Organization

### Group Related Code

```cpp
class OVT_SomeComponent
{
    // Group 1: Member variables
    protected int m_iValue;
    protected string m_sName;

    // Group 2: Lifecycle methods
    void Init(IEntity owner) {}
    void PostGameStart() {}

    // Group 3: Public API
    void DoAction() {}
    void ProcessData() {}

    // Group 4: Getters/Setters
    int GetValue() { return m_iValue; }
    void SetValue(int value) { m_iValue = value; }

    // Group 5: Protected helpers
    protected void InternalHelper() {}
}
```

### Separate Sections with Comments

```cpp
//-----------------------------------------------------------------------
// SECTION NAME
//-----------------------------------------------------------------------
```

---

## Attribute Usage

### Attribute Pattern

```cpp
[Attribute("defaultValue", desc: "Clear description")]
protected type m_varName;
```

### Common Attributes

```cpp
// Integer with range
[Attribute("10", UIWidgets.Slider, "Population", "0 1000 1")]
protected int m_iPopulation;

// String
[Attribute("DefaultName", desc: "Town name")]
protected string m_sTownName;

// Boolean
[Attribute("1", desc: "Is enabled")]
protected bool m_bEnabled;

// Resource path
[Attribute("{PathToResource}", UIWidgets.ResourceNamePicker, "Prefab")]
protected ResourceName m_PrefabResource;

// Enum
[Attribute("0", UIWidgets.ComboBox, "Faction", "", ParamEnumArray.FromEnum(EFaction))]
protected EFaction m_eFaction;
```

---

## Constants and Enums

### Constants

```cpp
class OVT_Constants
{
    static const int MAX_TOWNS = 100;
    static const float UPDATE_INTERVAL = 10.0;
    static const string DEFAULT_FACTION = "FIA";
}
```

### Enums

```cpp
enum EFactionType
{
    FRIENDLY,
    NEUTRAL,
    HOSTILE
}

enum ETownSize
{
    SMALL,
    MEDIUM,
    LARGE
}
```

---

## Best Practices

### ✅ DO:

- **Use OVT_ prefix:** For all Overthrow classes
- **Use type prefixes:** m_i, m_f, m_s, m_b, m_a, m_m
- **Use protected members:** With public getters/setters
- **Document public APIs:** Using Doxygen style
- **Group related code:** Use section comments
- **Use descriptive names:** Clear intent
- **Follow conventions:** Consistency across codebase

### ❌ DON'T:

- **Skip prefixes:** Always use OVT_ and m_
- **Use public members:** Except for data classes
- **Abbreviate unnecessarily:** `m_iTownId` not `m_iTId`
- **Mix naming styles:** Be consistent
- **Skip documentation:** Especially for complex methods
- **Use magic numbers:** Define constants

---

## Example: Complete Class

```cpp
//! Manages economic transactions and resource pricing
class OVT_EconomyManagerComponentClass: OVT_ComponentClass {};

//! Economy manager handles global economic state, pricing, and transactions.
//! Singleton component placed on OVT_OverthrowGameMode.
class OVT_EconomyManagerComponent: OVT_Component
{
    //-----------------------------------------------------------------------
    // ATTRIBUTES
    //-----------------------------------------------------------------------

    [Attribute("1000", UIWidgets.Slider, "Starting wealth", "0 10000 100")]
    protected int m_iStartingWealth;

    [Attribute("1.0", UIWidgets.Slider, "Inflation rate", "0 5 0.1")]
    protected float m_fInflationRate;

    //-----------------------------------------------------------------------
    // MEMBER VARIABLES
    //-----------------------------------------------------------------------

    //! Global wealth pool
    protected int m_iGlobalWealth;

    //! Resource prices by type
    protected ref map<string, int> m_mResourcePrices;

    //! Pending transactions
    protected ref array<ref OVT_Transaction> m_aTransactions;

    //-----------------------------------------------------------------------
    // STATIC
    //-----------------------------------------------------------------------

    static OVT_EconomyManagerComponent s_Instance;

    //! Get singleton instance
    //! @return The economy manager instance, or null if not loaded
    static OVT_EconomyManagerComponent GetInstance()
    {
        if (!s_Instance)
        {
            BaseGameMode pGameMode = GetGame().GetGameMode();
            if (pGameMode)
            {
                s_Instance = OVT_EconomyManagerComponent.Cast(
                    pGameMode.FindComponent(OVT_EconomyManagerComponent)
                );
            }
        }
        return s_Instance;
    }

    //-----------------------------------------------------------------------
    // LIFECYCLE
    //-----------------------------------------------------------------------

    //! Initialize economy manager
    //! @param owner The owning entity (game mode)
    void Init(IEntity owner)
    {
        m_iGlobalWealth = 0;
        m_mResourcePrices = new map<string, int>();
        m_aTransactions = new array<ref OVT_Transaction>();

        InitializeDefaultPrices();
    }

    //! Setup initial economy state for new game
    void PostGameStart()
    {
        m_iGlobalWealth = m_iStartingWealth;
        DistributeStartingWealth();
    }

    //-----------------------------------------------------------------------
    // PUBLIC METHODS
    //-----------------------------------------------------------------------

    //! Process a transaction
    //! @param transaction The transaction to process
    //! @return True if transaction successful
    bool ProcessTransaction(OVT_Transaction transaction)
    {
        if (!ValidateTransaction(transaction)) return false;

        m_aTransactions.Insert(transaction);
        return true;
    }

    //! Update resource price
    //! @param resourceType Type of resource
    //! @param newPrice New price value
    void UpdatePrice(string resourceType, int newPrice)
    {
        m_mResourcePrices.Set(resourceType, newPrice);
        OnPricesUpdated();
    }

    //-----------------------------------------------------------------------
    // GETTERS/SETTERS
    //-----------------------------------------------------------------------

    int GetGlobalWealth() { return m_iGlobalWealth; }
    void SetGlobalWealth(int wealth) { m_iGlobalWealth = wealth; }

    int GetPrice(string resourceType)
    {
        return m_mResourcePrices.Get(resourceType);
    }

    //-----------------------------------------------------------------------
    // PROTECTED METHODS
    //-----------------------------------------------------------------------

    //! Initialize default resource prices
    protected void InitializeDefaultPrices()
    {
        m_mResourcePrices.Insert("food", 10);
        m_mResourcePrices.Insert("supplies", 20);
        m_mResourcePrices.Insert("weapons", 50);
    }

    //! Validate transaction is legal
    //! @param transaction The transaction to validate
    //! @return True if valid
    protected bool ValidateTransaction(OVT_Transaction transaction)
    {
        if (!transaction) return false;
        if (transaction.m_iAmount <= 0) return false;
        return true;
    }

    //-----------------------------------------------------------------------
    // EVENT HANDLERS
    //-----------------------------------------------------------------------

    //! Called when prices are updated
    protected void OnPricesUpdated()
    {
        // Notify other systems
        BroadcastPriceUpdate();
    }
}
```

---

## Related Resources

- See `managers.md` for manager patterns
- See `controllers.md` for controller patterns
- See `file-structure.md` for file organization
- See `enforcescript-patterns/component-patterns.md` for component patterns
- See main `SKILL.md` for overview
