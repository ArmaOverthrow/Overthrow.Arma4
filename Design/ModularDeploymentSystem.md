# Modular Deployment System Design

## Overview

The Modular Deployment System is a new faction-agnostic framework designed to replace the BaseUpgrade system. It provides a flexible, config-driven approach for all factions (Occupying, Resistance, Supporting) to deploy military assets anywhere on the map, not just at bases.

## Key Design Principles

1. **Modular Architecture**: Deployments are composed of small, reusable modules that handle specific functionality
2. **Faction Agnostic**: Any faction can use deployments, with faction-specific configurations determining available assets
3. **Location Flexible**: Deployments can be placed at bases, along roads, in towns, or strategic locations using the existing slot system
4. **Persistent**: All deployments are automatically saved/loaded through EPF using world-based components
5. **Performance Conscious**: Entity spawning goes through a single API for future virtualization integration

## Core Architecture

### OVT_DeploymentComponent
The central component that represents each deployment instance in the world:

```cpp
class OVT_DeploymentComponent : OVT_Component
{
    [Attribute()]
    ref OVT_DeploymentConfig m_Config;
    
    protected ref array<ref OVT_BaseDeploymentModule> m_aActiveModules;
    protected int m_iControllingFaction;
    protected float m_fThreatLevel;
    protected int m_iResourcesInvested;
    protected bool m_bActive;
    
    // Lifecycle methods
    void InitializeDeployment(OVT_DeploymentConfig config, int factionIndex);
    void ActivateDeployment();
    void DeactivateDeployment();
    void UpdateDeployment(int deltaTime);
    void ReinforceDeployment(int additionalResources);
    void DestroyDeployment();
    
    // Module management
    void AddModule(OVT_BaseDeploymentModule module);
    void RemoveModule(typename moduleType);
    OVT_BaseDeploymentModule GetModule(typename moduleType);
    
    // Module type queries
    array<OVT_BaseConditionDeploymentModule> GetConditionModules();
    array<OVT_BaseSpawningDeploymentModule> GetSpawningModules();
    array<OVT_BaseBehaviorDeploymentModule> GetBehaviorModules();
    bool AreAllConditionsMet();
}
```

**Key Features:**
- Spawned as a prefab entity in the world at deployment location
- Automatically persisted by EPF framework
- Manages all modules for that specific deployment
- Tracks faction ownership, resources, and threat level
- Handles proximity-based activation/deactivation for performance

### OVT_DeploymentConfig
Configuration class that defines what a deployment does:

```cpp
class OVT_DeploymentConfig : OVT_BaseConfig
{
    [Attribute()]
    string m_sDeploymentName;
    
    [Attribute()]
    ref array<ref OVT_BaseDeploymentModule> m_aModules;
    
    [Attribute()]
    ref array<string> m_aAllowedFactionTypes; // "occupying", "resistance", "supporting"
    
    [Attribute()]
    int m_iBaseCost;
    
    [Attribute()]
    int m_iMinimumThreatLevel;
    
    [Attribute()]
    int m_iPriority; // 1-20, lower = higher priority
    
    [Attribute()]
    float m_fActivationRange; // Range for proximity-based activation
}
```

### OVT_BaseDeploymentModule (Base Class)
Abstract base class for all deployment modules that combines configuration and logic:

```cpp
class OVT_BaseDeploymentModule
{
    protected OVT_DeploymentComponent m_ParentDeployment;
    protected bool m_bInitialized;
    protected bool m_bActive;
    
    // Lifecycle
    void Initialize(OVT_DeploymentComponent parent);
    void Activate();
    void Deactivate();
    void Update(int deltaTime);
    void Cleanup();
    
    // Resource management
    int GetResourceCost();
    bool CanAfford(int availableResources);
    
    // Virtual methods for module-specific behavior
    protected void OnInitialize();
    protected void OnActivate();
    protected void OnDeactivate();
    protected void OnUpdate(int deltaTime);
    protected void OnCleanup();
}
```

### Module Type Base Classes

#### OVT_BaseSpawningDeploymentModule
Base class for all modules that spawn entities:
```cpp
class OVT_BaseSpawningDeploymentModule : OVT_BaseDeploymentModule
{
    protected ref array<ref EntityID> m_aSpawnedEntities;
    
    // Common spawning functionality
    protected IEntity SpawnEntity(ResourceName prefab, vector position, vector rotation);
    protected void DespawnEntity(EntityID entityId);
    protected void DespawnAllEntities();
    protected bool AreAllEntitiesAlive();
    
    protected override void OnCleanup()
    {
        DespawnAllEntities();
    }
}
```

#### OVT_BaseBehaviorDeploymentModule  
Base class for all modules that control AI behavior:
```cpp
class OVT_BaseBehaviorDeploymentModule : OVT_BaseDeploymentModule
{
    // Common behavior functionality
    protected array<SCR_AIGroup> GetManagedGroups();
    protected void ApplyBehaviorToGroup(SCR_AIGroup group);
    protected void RemoveBehaviorFromGroup(SCR_AIGroup group);
}
```

#### OVT_BaseConditionDeploymentModule
Base class for all modules that determine activation conditions:
```cpp
class OVT_BaseConditionDeploymentModule : OVT_BaseDeploymentModule
{
    protected bool m_bConditionMet;
    
    // Condition evaluation
    bool IsConditionMet() { return m_bConditionMet; }
    protected abstract bool EvaluateCondition();
    
    protected override void OnUpdate(int deltaTime)
    {
        m_bConditionMet = EvaluateCondition();
    }
}
```

## Module Types

### Spawning Modules

#### OVT_VehicleSpawningDeploymentModule
Spawns and manages vehicles:
```cpp
class OVT_VehicleSpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
    [Attribute()]
    string m_sVehicleTypeId; // Links to faction vehicle configs
    
    [Attribute()]
    int m_iVehicleCount;
    
    [Attribute()]
    bool m_bRequiresParkingSlot;
    
    [Attribute()]
    float m_fSpawnRadius;
    
    protected override void OnInitialize();
    protected override void OnActivate();
    protected override void OnDeactivate();
    protected override void OnUpdate(int deltaTime);
    protected void SpawnVehicles();
}
```

#### OVT_InfantrySpawningDeploymentModule
Spawns and manages infantry groups:
```cpp
class OVT_InfantrySpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
    [Attribute()]
    string m_sGroupTypeId; // Links to faction group configs
    
    [Attribute()]
    int m_iGroupCount;
    
    [Attribute()]
    float m_fSpawnRadius;
    
    protected ref array<ref SCR_AIGroup> m_aSpawnedGroups;
    
    protected override void OnInitialize();
    protected override void OnActivate();
    protected override void OnDeactivate();
    protected override void OnUpdate(int deltaTime);
    protected void SpawnGroups();
}
```

#### OVT_CompositionSpawningDeploymentModule
Spawns static structures:
```cpp
class OVT_CompositionSpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
    [Attribute()]
    string m_sCompositionTypeId; // Links to faction composition configs
    
    [Attribute()]
    string m_sRequiredSlotType; // "small", "medium", "large", "road_small", etc.
    
    [Attribute()]
    bool m_bMustBeNearBase;
    
    [Attribute()]
    bool m_bMustBeOnRoad;
    
    [Attribute()]
    float m_fMaxDistanceFromBase;
    
    protected EntityID m_UsedSlot;
    
    protected override void OnInitialize();
    protected override void OnActivate();
    protected override void OnDeactivate();
    protected IEntity FindSuitableSlot();
}
```

### Behavior Modules

#### OVT_PatrolBehaviorDeploymentModule
Defines patrol routes and behavior:
```cpp
class OVT_PatrolBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
    [Attribute()]
    string m_sPatrolType; // "perimeter", "point_to_point", "random", "town"
    
    [Attribute()]
    float m_fPatrolRadius;
    
    [Attribute()]
    ref array<ref vector> m_aWaypoints; // For point_to_point patrols
    
    [Attribute()]
    string m_sTargetTownId; // For town patrols
    
    [Attribute()]
    float m_fPatrolSpeed;
    
    [Attribute()]
    int m_iPatrolCycleTime; // Minutes
    
    protected int m_iLastPatrolTime;
    
    protected override void OnInitialize();
    protected override void OnUpdate(int deltaTime);
    protected override void ApplyBehaviorToGroup(SCR_AIGroup group);
    protected void AssignPatrolWaypoints(SCR_AIGroup group);
}
```

#### OVT_DefenseBehaviorDeploymentModule
Defines defensive positions and behavior:
```cpp
class OVT_DefenseBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
    [Attribute()]
    string m_sDefenseType; // "static", "roaming", "overwatch"
    
    [Attribute()]
    float m_fDefenseRadius;
    
    [Attribute()]
    ref array<ref vector> m_aDefensePositions;
    
    [Attribute()]
    bool m_bUseNearbyDefendSlots; // Use SCR_AISmartActionSentinelComponent slots
    
    protected override void OnInitialize();
    protected override void OnActivate();
    protected override void ApplyBehaviorToGroup(SCR_AIGroup group);
    protected void AssignDefensePositions(SCR_AIGroup group);
}
```

#### OVT_LogisticsBehaviorDeploymentModule
Defines logistics operations between deployments:
```cpp
class OVT_LogisticsBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
    [Attribute()]
    string m_sLogisticsType; // "supply_run", "reinforcement", "evacuation"
    
    [Attribute()]
    ref array<ref vector> m_aTargetLocations;
    
    [Attribute()]
    int m_iFrequencyMinutes;
    
    [Attribute()]
    string m_sCargoTypeId; // What they're carrying
    
    [Attribute()]
    bool m_bRequiresEscort;
    
    protected int m_iLastLogisticsTime;
    protected ref array<ref SCR_AIGroup> m_aActiveConvoys;
    
    protected override void OnInitialize();
    protected override void OnUpdate(int deltaTime);
    protected override void ApplyBehaviorToGroup(SCR_AIGroup group);
    protected void StartLogisticsMission();
    protected void OnConvoyComplete(SCR_AIGroup convoy);
}
```

### Condition Modules

#### OVT_ProximityConditionDeploymentModule
Activates deployment based on player proximity:
```cpp
class OVT_ProximityConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
    [Attribute()]
    float m_fActivationRange;
    
    [Attribute()]
    float m_fDeactivationRange;
    
    [Attribute()]
    bool m_bRequireLineOfSight;
    
    protected override bool EvaluateCondition();
    protected bool CheckPlayerProximity();
}
```

#### OVT_ThreatConditionDeploymentModule
Activates deployment based on threat level:
```cpp
class OVT_ThreatConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
    [Attribute()]
    float m_fMinimumThreat;
    
    [Attribute()]
    float m_fMaximumThreat;
    
    [Attribute()]
    bool m_bRespondToThreatChanges;
    
    protected float m_fLastThreatLevel;
    
    protected override bool EvaluateCondition();
}
```

#### OVT_TimeConditionDeploymentModule
Activates deployment based on time:
```cpp
class OVT_TimeConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
    [Attribute()]
    int m_iStartHour; // 0-23
    
    [Attribute()]
    int m_iEndHour; // 0-23
    
    [Attribute()]
    bool m_bWeekdaysOnly;
    
    protected override bool EvaluateCondition();
}
```

## Faction Integration System

### OVT_FactionDeploymentAssets
Each faction defines its available assets:
```cpp
class OVT_FactionDeploymentAssets : OVT_BaseConfig
{
    [Attribute()]
    ref array<ref OVT_FactionVehicleAsset> m_aVehicles;
    
    [Attribute()]
    ref array<ref OVT_FactionGroupAsset> m_aGroups;
    
    [Attribute()]
    ref array<ref OVT_FactionCompositionAsset> m_aCompositions;
}

class OVT_FactionVehicleAsset
{
    [Attribute()]
    string m_sTypeId; // Matches VehicleSpawningModule.m_sVehicleTypeId
    
    [Attribute()]
    ResourceName m_sPrefab;
    
    [Attribute()]
    int m_iResourceCost;
}
```

## Manager System

### OVT_DeploymentManager
Central manager for all deployments across all factions:

```cpp
class OVT_DeploymentManager : OVT_Component
{
    protected ref array<ref OVT_DeploymentComponent> m_aActiveDeployments;
    protected ref array<ref OVT_DeploymentConfig> m_aAvailableConfigs;
    
    // Deployment creation
    OVT_DeploymentComponent CreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex);
    bool CanCreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex);
    
    // Resource management
    bool SpendFactionResources(int factionIndex, OVT_DeploymentConfig config, vector position, float threat);
    void ProcessDeploymentQueue();
    
    // Location finding
    vector FindSuitableLocation(OVT_DeploymentConfig config, vector nearPosition, float searchRadius);
    IEntity FindAppropriateSlot(string slotType, vector nearPosition, float searchRadius);
    
    // Update system
    void UpdateAllDeployments(int deltaTime);
    void UpdateDeploymentsByProximity();
}
```

## Slot System Integration

The system leverages the existing slot infrastructure:

### Available Slot Types
- **Small Slots**: `SLOT_FLAT_SMALL` - Bunkers, small structures
- **Medium Slots**: `SLOT_FLAT_MEDIUM` - Towers, medium buildings  
- **Large Slots**: `SLOT_FLAT_LARGE` - Large compositions, bases
- **Small Road Slots**: `SLOT_ROAD_SMALL` - Roadblocks, small checkpoints
- **Medium Road Slots**: `SLOT_ROAD_MEDIUM` - Large checkpoints
- **Large Road Slots**: `SLOT_ROAD_LARGE` - Major roadblocks
- **Parking Slots**: `OVT_ParkingComponent` - Vehicle spawning

### Slot Requirements
Modules can specify requirements:
```cpp
// Must be near a base
m_bMustBeNearBase = true;
m_fMaxDistanceFromBase = 500;

// Must be on a road
m_bMustBeOnRoad = true;
m_sRequiredSlotType = "road_medium";

// Can be anywhere
m_sRequiredSlotType = ""; // Uses any available slot or spawns freely
```

## Configuration Examples

### Vehicle Patrol Deployment
```cpp
OVT_DeploymentConfig {
    m_sDeploymentName "Highway Patrol"
    m_aAllowedFactionTypes {"occupying"}
    m_iBaseCost 400
    m_iPriority 5
    m_aModules {
        // Spawn vehicles
        OVT_VehicleSpawningDeploymentModule {
            m_sVehicleTypeId "patrol_car"
            m_iVehicleCount 2
            m_bRequiresParkingSlot true
        }
        
        // Spawn crew
        OVT_InfantrySpawningDeploymentModule {
            m_sGroupTypeId "patrol_crew"
            m_iGroupCount 2
        }
        
        // Define patrol behavior
        OVT_PatrolBehaviorDeploymentModule {
            m_sPatrolType "point_to_point"
            m_aWaypoints { /* waypoint vectors */ }
            m_fPatrolSpeed 50
        }
        
        // Proximity activation
        OVT_ProximityConditionDeploymentModule {
            m_fActivationRange 1000
            m_fDeactivationRange 1500
        }
    }
}
```

### Static Checkpoint Deployment
```cpp
OVT_DeploymentConfig {
    m_sDeploymentName "Road Checkpoint"
    m_aAllowedFactionTypes {"occupying"}
    m_iBaseCost 200
    m_iPriority 3
    m_aModules {
        // Spawn checkpoint structure
        OVT_CompositionSpawningDeploymentModule {
            m_sCompositionTypeId "checkpoint_medium"
            m_sRequiredSlotType "road_medium"
            m_bMustBeOnRoad true
        }
        
        // Spawn guards
        OVT_InfantrySpawningDeploymentModule {
            m_sGroupTypeId "checkpoint_guards"
            m_iGroupCount 1
        }
        
        // Static defense
        OVT_DefenseBehaviorDeploymentModule {
            m_sDefenseType "static"
            m_bUseNearbyDefendSlots true
        }
        
        // Always active near roads
        OVT_ProximityConditionDeploymentModule {
            m_fActivationRange 800
        }
    }
}
```

### Supply Convoy Deployment
```cpp
OVT_DeploymentConfig {
    m_sDeploymentName "Supply Convoy"
    m_aAllowedFactionTypes {"occupying"}
    m_iBaseCost 600
    m_iPriority 8
    m_aModules {
        // Supply truck
        OVT_VehicleSpawningDeploymentModule {
            m_sVehicleTypeId "supply_truck"
            m_iVehicleCount 1
        }
        
        // Escort vehicle
        OVT_VehicleSpawningDeploymentModule {
            m_sVehicleTypeId "escort_vehicle"
            m_iVehicleCount 1
        }
        
        // Crews
        OVT_InfantrySpawningDeploymentModule {
            m_sGroupTypeId "convoy_crew"
            m_iGroupCount 2
        }
        
        // Logistics mission
        OVT_LogisticsBehaviorDeploymentModule {
            m_sLogisticsType "supply_run"
            m_aTargetLocations { /* base positions */ }
            m_iFrequencyMinutes 120
            m_bRequiresEscort true
        }
        
        // Threat-based activation
        OVT_ThreatConditionDeploymentModule {
            m_fMinimumThreat 25
        }
    }
}
```

## Implementation Phases

### Phase 1: Core Framework
1. Implement `OVT_DeploymentComponent` with EPF persistence
2. Create base module classes:
   - `OVT_BaseDeploymentModule`
   - `OVT_BaseSpawningDeploymentModule`
   - `OVT_BaseBehaviorDeploymentModule`
   - `OVT_BaseConditionDeploymentModule`
3. Implement `OVT_DeploymentManager` with basic deployment creation
4. Create unified entity spawning API for virtualization integration

### Phase 2: Basic Modules
1. Implement `OVT_VehicleSpawningDeploymentModule`
2. Implement `OVT_InfantrySpawningDeploymentModule`
3. Implement `OVT_PatrolBehaviorDeploymentModule`
4. Implement `OVT_ProximityConditionDeploymentModule`

### Phase 3: Advanced Modules
1. Implement `OVT_CompositionSpawningDeploymentModule`
2. Implement `OVT_DefenseBehaviorDeploymentModule`
3. Implement `OVT_LogisticsBehaviorDeploymentModule`
4. Implement remaining condition modules

### Phase 4: Faction Integration
1. Convert existing faction configs to support deployment assets
2. Implement faction-specific deployment strategies
3. Add threat-responsive reinforcement system
4. Performance optimization and virtualization preparation

## Migration Strategy

### Coexistence Period
- New deployment system runs alongside existing BaseUpgrade system
- BaseUpgrade system marked as deprecated but continues functioning
- New features (vehicle patrols, logistics) built only with deployment system

### Gradual Replacement
- Convert existing base upgrades one-by-one to deployment configs
- Maintain save compatibility during transition
- Remove BaseUpgrade system once all functionality is migrated

## Benefits Over BaseUpgrade System

1. **Flexibility**: Deployments not tied to specific bases
2. **Modularity**: Mix and match behaviors easily
3. **Faction Agnostic**: All factions can use the same system
4. **Performance**: Built-in virtualization preparation
5. **Persistence**: Automatic save/load through EPF
6. **Scalability**: Easy to add new module types
7. **Configuration**: Declarative, data-driven approach
8. **Logistics**: Support for dynamic operations between locations

This system provides the foundation for rich, dynamic military operations across all factions while maintaining performance and providing clear upgrade paths for future features.