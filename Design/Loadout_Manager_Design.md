# Loadout Manager Design

## Overview

The Loadout Manager system allows players to save, load, and manage equipment loadouts for themselves and AI units. Each loadout is saved as an individual file using EPF's PersistentScriptedState pattern, providing better performance and granular control.

## Architecture

### Core Components

#### 1. OVT_LoadoutManagerComponent
- **Type**: Manager Component on Game Mode entity
- **Purpose**: Central interface for loadout operations
- **Persistence**: Standard component save data for manager state only
- **Responsibilities**:
  - Provide API for saving/loading loadouts
  - Manage loadout validation
  - Handle loadout application to entities
  - Coordinate with other systems (inventory, equipment)

#### 2. OVT_PlayerLoadout (Individual Loadout State)
- **Type**: EPF_PersistentScriptedState
- **Purpose**: Individual loadout data saved as separate file
- **File Location**: `.db/Mission/PlayerLoadouts/[unique-id].json`
- **Data Structure**:
  ```cpp
  class OVT_PlayerLoadout : EPF_PersistentScriptedState
  {
      string m_sLoadoutName;          // User-defined name
      string m_sPlayerId;             // Owner's player ID
      string m_sDescription;          // Optional description
      int m_iCreatedTimestamp;        // Creation time
      int m_iLastUsedTimestamp;       // Last applied time
      bool m_bIsTemplate;             // Can be used by others
      ref array<ref OVT_LoadoutItem> m_aItems;  // Equipment items
      ref OVT_LoadoutMetadata m_Metadata;       // Additional data
  }
  ```

#### 3. OVT_LoadoutItem (Equipment Data)
- **Purpose**: Represents individual equipment pieces
- **Data Structure**:
  ```cpp
  class OVT_LoadoutItem
  {
      string m_sResourceName;         // Prefab resource name
      int m_iSlotType;               // Equipment slot (primary, secondary, etc.)
      ref array<string> m_aAttachments; // Weapon attachments
      ref map<string, string> m_mProperties; // Custom properties
  }
  ```

#### 4. OVT_LoadoutRepository
- **Purpose**: Data access layer for loadout operations
- **Pattern**: Repository pattern using EPF's database framework
- **Operations**:
  - Save/Update loadouts
  - Query loadouts by player/name
  - Delete loadouts
  - List available templates

## Persistence Strategy

### Individual File Persistence
Each loadout uses EPF's `EPF_PersistentScriptedState` for automatic individual file management:

```cpp
[
    EPF_PersistentScriptedStateSettings(OVT_PlayerLoadout),
    EDF_DbName.Automatic()
]
class OVT_PlayerLoadoutSaveData : EPF_ScriptedStateSaveData
{
    string m_sLoadoutName;
    string m_sPlayerId;
    string m_sDescription;
    int m_iCreatedTimestamp;
    int m_iLastUsedTimestamp;
    bool m_bIsTemplate;
    ref array<ref OVT_LoadoutItem> m_aItems;
    ref OVT_LoadoutMetadata m_Metadata;
}
```

### Benefits
- **Performance**: Load only specific loadouts when needed
- **Scalability**: Handles large numbers of loadouts efficiently
- **Granular Control**: Individual save/load/delete operations
- **File Organization**: EPF automatically organizes files by type
- **Cleanup**: Automatic file cleanup when loadouts are deleted

## API Design

### Core Operations

#### Save Loadout
```cpp
void SaveLoadout(string playerId, string loadoutName, IEntity sourceEntity, string description = "")
```

#### Load Loadout
```cpp
void LoadLoadout(string playerId, string loadoutName, IEntity targetEntity, bool async = true)
```

#### Apply Loadout
```cpp
bool ApplyLoadout(ref OVT_PlayerLoadout loadout, IEntity targetEntity)
```

#### Query Operations
```cpp
void GetPlayerLoadouts(string playerId, EDF_DbFindCallbackMultiple<OVT_PlayerLoadoutSaveData> callback)
void GetTemplateLoadouts(EDF_DbFindCallbackMultiple<OVT_PlayerLoadoutSaveData> callback)
bool DeleteLoadout(string playerId, string loadoutName)
```

### Integration Points

#### With Inventory System
- Extract equipment from entity inventory
- Apply equipment to entity inventory
- Validate equipment availability
- Handle equipment conflicts

#### With AI Commanding
- Apply loadouts to recruited AI units
- Template loadouts for specific roles
- Quick loadout switching for AI squads

#### With UI System
- Loadout selection dialogs
- Loadout creation/editing interfaces
- Preview loadout contents
- Manage saved loadouts

## Implementation Phases

### Phase 1: Core Infrastructure
1. Create base loadout classes and save data structures
2. Implement OVT_LoadoutManagerComponent with basic API
3. Set up EPF persistence for individual loadouts
4. Create OVT_LoadoutRepository for data access

### Phase 2: Equipment Integration
1. Implement equipment extraction from entities
2. Create equipment application logic
3. Add validation and conflict resolution
4. Handle weapon attachments and modifications

### Phase 3: UI Integration
1. Create loadout management UI contexts
2. Implement loadout creation/editing dialogs
3. Add loadout selection interfaces for AI commanding
4. Create template loadout management

### Phase 4: Advanced Features
1. Add loadout sharing between players
2. Implement preset role-based loadouts
3. Add loadout validation against available equipment
4. Create loadout comparison tools

## Console Platform Considerations

All persistence operations must be wrapped with console platform checks:

```cpp
#ifndef PLATFORM_CONSOLE
    // EPF persistence operations
    loadout.Save();
#endif
```

Alternative approaches for console platforms:
- In-memory loadout storage for session only
- Simplified loadout system without persistence
- Server-side loadout storage for multiplayer

## File Structure

```
Scripts/Game/GameMode/Managers/
└── OVT_LoadoutManagerComponent.c        // Contains both ComponentClass and Component

Scripts/Game/GameMode/SaveData/
├── OVT_LoadoutManagerSaveData.c
└── OVT_PlayerLoadoutSaveData.c

Scripts/Game/Data/
├── OVT_PlayerLoadout.c
├── OVT_LoadoutItem.c
├── OVT_LoadoutMetadata.c
└── OVT_LoadoutRepository.c

Scripts/Game/UI/Loadouts/
├── OVT_LoadoutManagerDialog.c
├── OVT_LoadoutCreationDialog.c
└── OVT_LoadoutSelectionWidget.c
```

