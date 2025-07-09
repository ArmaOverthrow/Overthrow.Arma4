# Overthrow Interactive Map System Documentation

## Overview

This document describes the current implementation of the Overthrow Interactive Map System, which provides a unified map interface integrating location information and fast travel functionality directly into the game's map UI.

## Current Implementation Status

The map system has been partially implemented with core infrastructure complete and one location type (towns) fully operational. The system leverages Arma Reforger's base game map widgets and UI components for better UX and console support.

## Architecture Overview

### Core Components

#### 1. OVT_OverthrowMapUI
**Base Class:** `SCR_MapUIElementContainer`  
**Location:** `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c`

The main map UI component that manages all interactive map elements for Overthrow.

**Key Features:**
- Integrates with base game map system
- Manages location type registration and initialization
- Handles map open/close events and element updates
- Coordinates element positioning and UI state management
- Processes user interactions and delegates to appropriate handlers

**Configuration:**
```cpp
OVT_OverthrowMapUI {
    m_aLocationTypes {
        OVT_MapLocationTown {}
    }
}
```

#### 2. OVT_MapLocationType
**Base Class:** `ScriptAndConfig`  
**Location:** `Scripts/Game/UI/Map/OVT_MapLocationType.c`

Abstract base class for defining location types with hybrid configuration and logic approach.

**Key Attributes:**
- `m_sDisplayName` - Display name for location type
- `m_fVisibilityZoom` - Visibility priority (0=always visible, higher=visible at closer zoom)
- `m_IconLayout` - Icon widget layout resource
- `m_InfoLayout` - Info panel layout resource
- `m_IconImageset` - Icon imageset resource
- `m_sIconName` - Icon name in imageset
- `m_bShowDistance` - Show distance to location
- `m_bCanFastTravel` - Can fast travel to this location type by default

**Virtual Methods:**
```cpp
void PopulateLocations(OVT_OverthrowMapUI mapUI, array<ref OVT_MapLocationData> locations);
bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason);
void OnLocationSelected(OVT_MapLocationData location, OVT_MapLocationElement element);
void OnLocationClicked(OVT_MapLocationData location, OVT_MapLocationElement element);
void UpdateInfoPanel(OVT_MapLocationData location, Widget infoPanel);
string GetLocationName(OVT_MapLocationData location);
string GetLocationDescription(OVT_MapLocationData location);
bool ShouldShowLocation(OVT_MapLocationData location, string playerID);
```

#### 3. OVT_MapLocationElement
**Base Class:** `SCR_MapUIElement`  
**Location:** `Scripts/Game/UI/Map/OVT_MapLocationElement.c`

Individual interactive map elements that represent specific locations.

**Responsibilities:**
- Handle click detection and user interaction
- Manage visual state (selected, hovered, etc.)
- Display location-specific information
- Provide Fast Travel functionality when available

#### 4. OVT_MapLocationData
**Base Class:** `Managed`  
**Location:** `Scripts/Game/UI/Map/OVT_MapLocationData.c`

Runtime data container for location instances containing position, type reference, and type-specific data.

### Fast Travel Integration

#### OVT_FastTravelService
**Location:** `Scripts/Game/UI/Map/OVT_FastTravelService.c`

Centralized fast travel service that handles:
- Global fast travel checks (wanted level, QRF proximity, distance restrictions)
- Cost calculation for fast travel
- Fast travel execution with proper player positioning

**Key Methods:**
```cpp
static bool CanGlobalFastTravel(vector targetPos, string playerID, out string reason);
static void ExecuteFastTravel(vector targetPos, int playerID);
static int CalculateFastTravelCost(vector targetPos, string playerID);
```

## Implemented Location Types

### OVT_MapLocationTown
**Location:** `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationTown.c`

Handles towns and cities with support/stability display and faction information.

**Features:**
- Displays town name, population, and faction control
- Shows support/stability levels with visual indicators
- Provides fast travel capability based on town control
- Integrates with `OVT_TownManagerComponent` for data population

**Configuration:**
```cpp
OVT_MapLocationTown {
    m_sDisplayName "Town"
    m_fVisibilityZoom 0
    m_IconLayout "{A22A047954A80F2E}UI/layouts/Map/OVT_MapLocationElement.layout"
    m_InfoLayout "{5C40C1114BC0BE2E}UI/layouts/Map/OVT_MapInfoTown.layout"
    m_IconImageset "{2EFEA2AF1F38E7EC}UI/Textures/OverthrowIcons.imageset"
    m_sIconName "town"
    m_bShowDistance 1
    m_bCanFastTravel 1
}
```

## UI Layout System

### Base Element Layout
**File:** `UI/layouts/Map/OVT_MapLocationElement.layout`

Contains common elements:
- Icon container with imageset support
- Selection highlight overlay
- Distance text display
- Fast travel availability indicator

### Info Panel Layout
**File:** `UI/layouts/Map/OVT_MapInfoTown.layout`

Town-specific info panel containing:
- Town name and type header
- Population and faction information
- Support/stability meters
- Fast travel button with cost display
- Distance information

## Integration Points

### Map Configuration
**File:** `Configs/UI/Map/MapFullscreen.conf`

The new map system is integrated into the base game's map configuration:
```cpp
SCR_MapEntity {
    m_MapUIRoot {
        m_aElements {
            OVT_OverthrowMapUI {
                m_aLocationTypes {
                    OVT_MapLocationTown {}
                }
            }
        }
    }
}
```

### Legacy System Status
The old `OVT_MapIcons` component is disabled but still present in the codebase. The old system handled:
- Military bases and FOBs
- Houses and real estate
- Shops and services
- Vehicles and personal camps
- Radio towers and ports
- Gun dealers

These location types need to be migrated to the new system by implementing corresponding `OVT_MapLocationType` subclasses.

## Current Limitations

1. **Limited Location Types**: Only towns are implemented in the new system
2. **Legacy Dependency**: Most location types still rely on the old `OVT_MapIcons` system
3. **Migration Incomplete**: Main menu still has separate "Map Info" and "Fast Travel" options
4. **Configuration Gaps**: Many location types lack proper configuration and layouts

## Testing and Validation

### Functional Testing
- ✅ Map integration with base game map system
- ✅ Town location display and interaction
- ✅ Fast travel functionality from map
- ✅ Info panel display and updates
- ✅ Distance calculation and display

### Performance Testing
- ✅ Map loading with multiple town locations
- ✅ Real-time location updates
- ✅ Memory usage optimization

## Benefits Achieved

### For Players
- Unified map interface with integrated fast travel
- Rich location information accessible directly from map
- Better visual feedback and interaction
- Improved console controller navigation

### For Developers
- Modular, maintainable code architecture
- Clear separation of concerns
- Leverages proven base game patterns
- Extensible location type system

### For Modders
- Configuration-driven approach for new location types
- Well-defined extension points
- Example implementation available (towns)
- No need to modify core map code

## Next Steps for Complete Implementation

To fully replace the legacy system, the following location types need to be implemented:

1. **OVT_MapLocationBase** - Military bases with faction control
2. **OVT_MapLocationFOB** - Resistance FOBs with ownership status
3. **OVT_MapLocationHouse** - Owned/rented properties
4. **OVT_MapLocationShop** - Shops with type-specific information
5. **OVT_MapLocationCamp** - Personal camps with fast travel
6. **OVT_MapLocationVehicle** - Owned vehicles with positioning
7. **OVT_MapLocationRadioTower** - Radio towers with control status
8. **OVT_MapLocationPort** - Ports with import/export functionality
9. **OVT_MapLocationGunDealer** - Gun dealers with trade information

Each would follow the same pattern as `OVT_MapLocationTown`, implementing the virtual methods from `OVT_MapLocationType` and creating location-specific info panel layouts.

## Conclusion

The new map system provides a solid foundation with proven functionality for towns. The architecture is sound and extensible, requiring only the implementation of additional location types to achieve full feature parity with the legacy system. The modular design ensures easy maintenance and future expansion while providing a significantly improved user experience.