# Overthrow Interactive Map System Design

## Overview

This document outlines the design for a new interactive map system for Overthrow that will replace the current map implementation with a modern, extensible system that leverages base game map features and provides modular location type support.

## Goals

- Replace the current map system (`OVT_MapRestrictedAreas`, `OVT_MapIcons`, `OVT_MapContext`)
- Integrate Fast Travel and Map Info functionality directly into the map
- Create a modular, extensible system for location types
- Leverage base game map widgets and UI components for better UX and console support
- Provide easy configuration for modders to add new location types

## Architecture Overview

### Core Components

#### 1. OVT_OverthrowMapUI
**Base Class:** `SCR_MapUIElementContainer`

The main map UI component that manages all interactive map elements for Overthrow.

**Responsibilities:**
- Initialize and manage all location types
- Handle map open/close events
- Coordinate element positioning and updates
- Manage UI state (selection, hover, etc.)
- Process click events and delegate to appropriate handlers

#### 2. OVT_MapLocationType
**Base Class:** `ScriptAndConfig` (following `OVT_BaseUpgrade` pattern)

Abstract base class for defining location types with both configuration and logic.

**Key Features:**
- Hybrid config/code approach for maximum flexibility
- Configurable attributes for modders
- Virtual methods for custom behavior
- Built-in support for common functionality

#### 3. OVT_MapLocationElement
**Base Class:** `SCR_MapUIElement`

Individual interactive map elements that represent specific locations.

**Responsibilities:**
- Handle click detection and user interaction
- Manage visual state (selected, hovered, etc.)
- Display location-specific information
- Provide Fast Travel functionality when available

#### 4. OVT_MapLocationData
**Base Class:** `Managed`

Runtime data container for location instances.

## Location Type System

### Base Configuration Attributes

```cpp
class OVT_MapLocationType : ScriptAndConfig
{
    [Attribute(defvalue: "Generic Location", desc: "Display name for location type")]
    protected string m_sDisplayName;
    
    [Attribute(defvalue: "1", desc: "Visibility priority (0=always visible, higher=visible at closer zoom)")]
    protected float m_fVisibilityZoom;
    
    [Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Icon widget layout", params: "layout")]
    protected ResourceName m_IconLayout;
    
    [Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Info panel layout", params: "layout")]
    protected ResourceName m_InfoLayout;
    
    [Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Icon imageset", params: "imageset")]
    protected ResourceName m_IconImageset;
    
    [Attribute(defvalue: "", desc: "Icon name in imageset")]
    protected string m_sIconName;
    
    [Attribute(defvalue: "true", desc: "Show distance to location")]
    protected bool m_bShowDistance;
    
    [Attribute(defvalue: "false", desc: "Can fast travel to this location type by default")]
    protected bool m_bCanFastTravel;
}
```

### Virtual Methods

```cpp
// Populate locations of this type
void PopulateLocations(OVT_OverthrowMapUI mapUI, array<ref OVT_MapLocationData> locations);

// Check if specific location allows fast travel
bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason);

// Handle location selection
void OnLocationSelected(OVT_MapLocationData location, OVT_MapLocationElement element);

// Handle location click
void OnLocationClicked(OVT_MapLocationData location, OVT_MapLocationElement element);

// Update location-specific UI info panel
void UpdateInfoPanel(OVT_MapLocationData location, Widget infoPanel);

// Get location name for display
string GetLocationName(OVT_MapLocationData location);

// Get location description
string GetLocationDescription(OVT_MapLocationData location);

// Check if location should be visible
bool ShouldShowLocation(OVT_MapLocationData location, string playerID);
```

## Built-in Location Types

### 1. OVT_MapLocationTown
Handles towns and cities with support/stability display.

### 2. OVT_MapLocationBase
Handles military bases with faction control and capture status.

### 3. OVT_MapLocationFOB
Handles resistance FOBs with ownership and management options.

### 4. OVT_MapLocationHouse
Handles owned/rented properties with home setting functionality.

### 5. OVT_MapLocationShop
Handles shops with type-specific icons and opening functionality.

### 6. OVT_MapLocationCamp
Handles personal camps with fast travel capability.

### 7. OVT_MapLocationVehicle
Handles owned vehicles with positioning and status.

### 8. OVT_MapLocationRadioTower
Handles radio towers with faction control status.

### 9. OVT_MapLocationPort
Handles ports with import/export functionality.

### 10. OVT_MapLocationGunDealer
Handles gun dealers with illegal trade functionality.

## UI Layout System

### Base Element Layout
**File:** `UI/layouts/Map/OVT_MapLocationElement.layout`

Contains common elements:
- Icon container
- Selection highlight
- Distance text (optional)
- Fast travel indicator (when available)

### Info Panel Base Layout
**File:** `UI/layouts/Map/OVT_MapInfoPanel.layout`

Contains common info structure:
- Title bar with location name and type
- Distance display
- Fast travel button (when available)
- Content slot for location-specific information
- Close button

### Location-Specific Info Layouts
Each location type defines its own info content layout that gets inserted into the content slot:
- `OVT_MapInfoTown.layout` - Support/stability, population, etc.
- `OVT_MapInfoBase.layout` - Faction control, garrison, defenses
- `OVT_MapInfoShop.layout` - Shop type, inventory status, prices
- etc.

## Fast Travel Integration

### Core Fast Travel Logic
The existing fast travel logic from `OVT_MapContext.CanFastTravel()` will be moved to a centralized service:

```cpp
class OVT_FastTravelService
{
    // Global fast travel checks (wanted level, QRF, distance, etc.)
    static bool CanGlobalFastTravel(vector targetPos, string playerID, out string reason);
    
    // Execute fast travel with cost calculation
    static void ExecuteFastTravel(vector targetPos, int playerID);
}
```

### Location Type Fast Travel
Each location type implements its own fast travel logic:

```cpp
bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
{
    // Check global restrictions first
    if (!OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason))
        return false;
    
    // Location-specific checks
    // For houses: check ownership, not rented, not warehouse
    // For FOBs: check resistance control
    // For bases: check resistance control
    // etc.
    
    return true;
}
```

## Configuration System

### Map Configuration
**File:** `Configs/Map/OVT_MapConfiguration.conf`

```cpp
OVT_MapConfiguration
{
    LocationTypes
    {
        OVT_MapLocationTown {}
        OVT_MapLocationBase {}
        OVT_MapLocationFOB {}
        OVT_MapLocationHouse {}
        OVT_MapLocationShop {}
        OVT_MapLocationCamp {}
        OVT_MapLocationVehicle {}
        OVT_MapLocationRadioTower {}
        OVT_MapLocationPort {}
        OVT_MapLocationGunDealer {}
    }
}
```

### Modder Extension
Modders can add new location types by:

1. Creating a new class extending `OVT_MapLocationType`
2. Implementing the required virtual methods
3. Creating layout files for icon and info panel
4. Adding the location type to their mod's configuration

Example custom location type:
```cpp
[BaseContainerProps(configRoot: true)]
class OVT_MapLocationCustom : OVT_MapLocationType
{
    [Attribute(defvalue: "Custom POI", desc: "Custom location display name")]
    protected string m_sCustomName;
    
    override void PopulateLocations(OVT_OverthrowMapUI mapUI, array<ref OVT_MapLocationData> locations)
    {
        // Custom population logic
    }
    
    override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
    {
        // Custom fast travel logic
        return m_bCanFastTravel;
    }
}
```

## Implementation Plan

### Phase 1: Core Infrastructure
1. Create `OVT_OverthrowMapUI` extending `SCR_MapUIElementContainer`
2. Create `OVT_MapLocationType` base class with `ScriptAndConfig` pattern
3. Create `OVT_MapLocationElement` extending `SCR_MapUIElement`
4. Create `OVT_MapLocationData` for runtime data
5. Create base UI layouts for elements and info panel

### Phase 2: Basic Location Types
1. Implement `OVT_MapLocationTown` for towns/cities
2. Implement `OVT_MapLocationBase` for military bases
3. Implement `OVT_MapLocationFOB` for resistance FOBs
4. Test basic functionality and interactions

### Phase 3: Extended Location Types
1. Implement remaining location types (houses, shops, vehicles, etc.)
2. Create location-specific info panel layouts
3. Integrate with existing managers for data population

### Phase 4: Fast Travel Integration
1. Extract and refactor fast travel logic into `OVT_FastTravelService`
2. Implement location-specific fast travel rules
3. Remove old fast travel from main menu
4. Test fast travel functionality across all location types

### Phase 5: Polish and Optimization
1. Add visual polish (animations, effects, sounds)
2. Optimize performance for large numbers of locations
3. Add accessibility features for console support
4. Documentation for modders

## Migration from Current System

### Deprecation Plan
1. Keep old system functional during development
2. Add config option to switch between old/new map systems
3. Once new system is stable, remove old system
4. Update main menu to remove "Map Info" and "Fast Travel" options

### Data Migration
- Town data: Already available through `OVT_TownManagerComponent`
- Base data: Available through `OVT_OccupyingFactionManager`
- Real estate data: Available through `OVT_RealEstateManagerComponent`
- Vehicle data: Available through `OVT_VehicleManagerComponent`
- No data migration needed, just new presentation layer

## Testing Strategy
- Test UX improvements over current system
- Test console controller navigation
- Test performance with large numbers of locations
- Test modder extension capabilities

## Benefits of New System

### For Players
- Unified map interface with all functionality
- Better visual feedback and interaction
- Improved console support
- More intuitive fast travel selection
- Rich location information at a glance

### For Developers
- Modular, maintainable code architecture
- Easier to add new location types
- Better separation of concerns
- Leverages proven base game patterns

### For Modders
- Easy to extend with new location types
- Configuration-driven approach
- Well-documented extension points
- No need to modify core map code

## Conclusion

This new map system will provide a significant improvement to the Overthrow user experience while creating a solid foundation for future expansion. The modular architecture ensures that new features and location types can be added easily, and the use of base game patterns ensures compatibility and performance.