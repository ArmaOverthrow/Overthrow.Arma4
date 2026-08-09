# File Organization

Complete guide for Overthrow project directory structure and file placement.

---

## Directory Structure

```
Overthrow.Arma4/
├── Configs/                    # Game configuration files
│   ├── Factions/              # Faction configs
│   ├── Economy/               # Economy configs
│   └── ...
│
├── Prefabs/                    # Entity prefabs and compositions
│   ├── Bases/                 # Base prefabs
│   ├── Towns/                 # Town markers/triggers
│   ├── Vehicles/              # Vehicle prefabs
│   └── Compositions/          # Grouped entity compositions
│
├── Scripts/                    # All EnforceScript code
│   └── Game/
│       ├── Components/        # Entity components and UI components
│       ├── GameMode/          # Core game logic and managers
│       ├── Entities/          # Entity classes
│       ├── Controllers/       # Controller components (deprecated location)
│       ├── Configuration/     # Config classes for game systems
│       ├── UI/                # UI contexts and widgets
│       └── UserActions/       # Player interaction actions
│
├── UI/                         # UI layouts (.layout files)
│   ├── Contexts/              # Main UI screens
│   ├── Widgets/               # Reusable widget layouts
│   └── HUD/                   # HUD elements
│
├── Design/                     # Design documents (being migrated)
│
├── docs/                       # Documentation (new location)
│   └── features/              # Feature design docs
│
└── dev/                        # Development tracking (Beast Mode)
    ├── active/                # Active feature dev docs
    ├── completed/             # Completed feature dev docs
    └── templates/             # Dev doc templates
```

---

## Scripts Organization

### Scripts/Game/Components/

**Purpose:** Entity components and UI components

**Contents:**
- Manager components (singletons on game mode)
- Controller components (instance managers)
- Helper components (sub-systems)
- UI component helpers

**Example:**
```
Scripts/Game/Components/
├── OVT_TownController.c           # Town controller
├── OVT_BaseController.c           # Base controller
├── OVT_DefenseComponent.c         # Defense sub-component
└── UI/
    └── OVT_MapMarkerComponent.c   # UI helper component
```

### Scripts/Game/GameMode/

**Purpose:** Core game logic, managers, and game mode

**Contents:**
- OVT_OverthrowGameMode
- Manager components
- Game mode helpers
- Global utility classes

**Example:**
```
Scripts/Game/GameMode/
├── OVT_OverthrowGameMode.c
├── OVT_TownManagerComponent.c
├── OVT_EconomyManagerComponent.c
├── OVT_FactionManagerComponent.c
└── OVT_Global.c
```

### Scripts/Game/Entities/

**Purpose:** Entity classes (non-component)

**Contents:**
- Custom entity classes
- Entity helpers
- Spawning utilities

**Example:**
```
Scripts/Game/Entities/
├── OVT_VehicleSpawner.c
├── OVT_AICommander.c
└── OVT_TriggerEntity.c
```

### Scripts/Game/Controllers/ (Deprecated)

**Purpose:** Legacy location for controllers

**Note:** New controllers should go in Scripts/Game/Components/

### Scripts/Game/Configuration/

**Purpose:** Configuration classes and data structures

**Contents:**
- Config classes for systems
- Data structures
- Constants and enums

**Example:**
```
Scripts/Game/Configuration/
├── OVT_FactionConfig.c
├── OVT_EconomyConfig.c
└── OVT_GameConstants.c
```

### Scripts/Game/UI/

**Purpose:** UI contexts and widgets

**Contents:**
- OVT_UIContext derived classes
- UI widget helpers
- Menu logic

**Example:**
```
Scripts/Game/UI/
├── OVT_ShopContext.c
├── OVT_MapContext.c
├── OVT_DialogContext.c
└── Widgets/
    └── OVT_TownListWidget.c
```

### Scripts/Game/UserActions/

**Purpose:** Player interaction actions

**Contents:**
- SCR_BaseGameModeAction derived classes
- Context actions (use, interact, etc.)
- Custom action classes

**Example:**
```
Scripts/Game/UserActions/
├── OVT_RecruitAction.c
├── OVT_BuildAction.c
└── OVT_TradeAction.c
```

---

## File Naming Conventions

### Class Files

**Pattern:** `OVT_ClassName.c`

**Examples:**
- `OVT_TownManagerComponent.c`
- `OVT_BaseController.c`
- `OVT_ShopContext.c`

### Multiple Classes Per File

**When to use:** Tightly coupled classes (Component + ComponentClass, SaveData classes)

**Example:**
```cpp
// OVT_TownManagerComponent.c

class OVT_TownManagerComponentClass: OVT_ComponentClass {};

class OVT_TownManagerComponent: OVT_Component
{
    // Manager implementation
}

#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_TownManagerComponent)]
class OVT_TownManagerSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_TownManagerSaveData : EPF_ComponentSaveData
{
    // Save data implementation
}

#endif
```

---

## Prefabs Organization

### Prefabs/

**Structure by category:**

```
Prefabs/
├── Bases/
│   ├── OVT_Base_Small.et
│   ├── OVT_Base_Medium.et
│   └── OVT_Base_Large.et
│
├── Towns/
│   ├── OVT_TownMarker.et
│   └── OVT_TownTrigger.et
│
├── Vehicles/
│   ├── OVT_Vehicle_Truck.et
│   └── OVT_Vehicle_APC.et
│
├── Compositions/
│   ├── OVT_Checkpoint.et
│   └── OVT_FOB.et
│
└── GameMode/
    └── OVT_OverthrowGameMode.et
```

### Prefab Naming

**Pattern:** `OVT_Category_Specific.et`

**Examples:**
- `OVT_Base_Small.et`
- `OVT_Vehicle_Truck.et`
- `OVT_Checkpoint.et`

---

## UI Organization

### UI Layouts

```
UI/
├── Contexts/                  # Main screen layouts
│   ├── Shop.layout           # Shop UI
│   ├── Map.layout            # Map UI
│   └── Dialog.layout         # Dialog UI
│
├── Widgets/                   # Reusable widgets
│   ├── TownList.layout       # Town list widget
│   └── InventoryGrid.layout  # Inventory grid
│
└── HUD/                       # HUD elements
    ├── ResourceDisplay.layout
    └── Minimap.layout
```

---

## Configuration Files

### Configs/

**Structure by system:**

```
Configs/
├── Factions/
│   ├── FIA.conf
│   ├── RACS.conf
│   └── Everon.conf
│
├── Economy/
│   ├── Resources.conf
│   └── Prices.conf
│
└── GameMode/
    └── Overthrow.conf
```

---

## Where to Put New Files

### New Manager Component?
**Location:** `Scripts/Game/GameMode/OVT_NewManagerComponent.c`

### New Controller Component?
**Location:** `Scripts/Game/Components/OVT_NewController.c`

### New UI Context?
**Location:** `Scripts/Game/UI/OVT_NewContext.c`

**Layout:** `UI/Contexts/NewScreen.layout`

### New User Action?
**Location:** `Scripts/Game/UserActions/OVT_NewAction.c`

### New Data Class?
**Location:** `Scripts/Game/Configuration/OVT_NewData.c`

### New Prefab?
**Location:** `Prefabs/Category/OVT_New.et`

### New Config?
**Location:** `Configs/System/config.conf`

---

## Best Practices

### ✅ DO:

- **Follow structure:** Use established directories
- **Use OVT_ prefix:** For all Overthrow files
- **Group related files:** Keep related classes together
- **Match class to filename:** OVT_ClassName in OVT_ClassName.c
- **Organize by category:** Not by file type
- **Keep paths short:** Avoid deep nesting

### ❌ DON'T:

- **Create new top-level dirs:** Use existing structure
- **Mix concerns:** Keep managers in GameMode/, controllers in Components/
- **Use deep nesting:** Keep directory depth reasonable
- **Forget OVT_ prefix:** Distinguish from base game files
- **Put scripts in root:** All scripts in Scripts/Game/

---

## Migration Notes

### Design Docs

**Old:** `Design/` (being deprecated)

**New:** `docs/features/`

### Controller Location

**Old:** `Scripts/Game/Controllers/` (deprecated)

**New:** `Scripts/Game/Components/`

---

## Related Resources

- See `managers.md` for manager component patterns
- See `controllers.md` for controller component patterns
- See `coding-standards.md` for naming conventions
- See main `SKILL.md` for overview
