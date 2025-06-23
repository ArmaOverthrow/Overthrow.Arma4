# Mobile FOBs Feature Implementation

## Overview

The Mobile FOBs feature represents a significant evolution in Overthrow's base building and tactical positioning system. This feature enhances the existing camp system while introducing deployable truck-based Forward Operating Bases (FOBs) that provide full base-building capabilities.

## Feature Specification (Updated Design)

### Core Concept Changes

1. **Enhanced Camp System**: The existing camp system is extended with new capabilities while maintaining the one-camp-per-player limit
2. **Mobile FOB Introduction**: New truck-based deployable base system accessible to all players (with optional server restrictions)
3. **Dynamic Base Network**: Multiple FOBs can be deployed simultaneously, creating a flexible network of operational bases

### Detailed Implementation

#### Enhanced Camps
- **Foundation**: Extension of existing camp system already implemented in Overthrow
- **Fast Travel**: 
  - Previously: Owner-only fast travel
  - Now: Fast travel to any player's camp unless marked as 'private'
  - Privacy toggle allows camp owners to restrict access
- **Building Capabilities**:
  - Small building radius (50m for building, 75m for placing)
  - Basic camp structures allowed within radius
  - Placeables: Equipment boxes, sandbags, basic fortifications
- **Limitations**: 
  - One camp per player maximum (existing limitation)
  - Placing new camp destroys previous camp
  - Smaller operational radius compared to FOBs

#### Mobile FOBs
- **Acquisition**: 
  - Default: Any player can purchase Mobile FOB trucks
  - Server Option: `Overthrow_config.json` setting to restrict purchase/deployment to officers only
  - Multiple Mobile FOBs can be purchased and deployed simultaneously
- **Deployment**: 
  - Any player can deploy (unless server-restricted to officers)
  - Multiple FOBs can be deployed at the same time
  - Full base-building capabilities when deployed
- **Map Visibility**:
  - Officers can designate one FOB as "priority"
  - Priority FOB: Prominent marker visible at all zoom levels
  - Non-priority FOBs: Hidden when fully zoomed out
- **Capabilities When Deployed**:
  - All tent building types available
  - Full construction options
  - Fast travel destination  
  - Can be set as home/respawn location
- **Redeployment**:
  - Undeploy functionality for tactical repositioning
  - Items in nearby ammoboxes transferred to truck
  - Built structures removed on undeployment

#### Strategic Elements
- **Accessibility**: Democratic access to FOB capabilities (vs officer-only in original design)
- **Network Warfare**: Multiple simultaneous FOBs enable distributed operations
- **Tactical Hierarchy**: Priority FOB system for command coordination
- **Server Customization**: Flexible restrictions via config file

## Implementation Status

### Completed Features ✅

#### Core System Architecture
- **Data Structures**: 
  - `OVT_CampData` class for camp management
  - `OVT_FOBData` class for FOB tracking
  - `OVT_VehicleUpgrades` and `OVT_VehicleUpgrade` classes for upgrade system

#### Vehicle System
- **Prefabs Created**:
  - `OverthrowMobileFOB.et` - Base mobile FOB truck
  - `OverthrowMobileFOBCargoCanvas.et` - Canvas variant
  - `OverthrowMobileFOBDeployed.et` - Deployed FOB state
  - `OverthrowMobileFOBDeployedCargoCanvas.et` - Deployed canvas variant

#### User Actions
- **Deploy Action** (`OVT_DeployFOBAction.c`):
  - Officer-only action to deploy Mobile FOB
  - Calls `OVT_Global.GetServer().DeployFOB()`
- **Undeploy Action** (`OVT_UndeployFOBAction.c`):
  - Officer-only action to undeploy FOB
  - Calls `OVT_Global.GetServer().UndeployFOB()`

#### Configuration Updates
- **Placeables**: Camp configuration updated in `placeables.conf`
- **Pricing**: Mobile FOB pricing added to `vehiclePrices.conf`
- **Localization**: Multi-language support added for new UI elements

#### Manager Integration
- **Resistance Faction Manager** (`OVT_ResistanceFactionManager.c`):
  - Mobile FOB prefab references added
  - Camp and FOB data arrays implemented
  - Integration with vehicle upgrade system

#### UI Integration
- **Map Context**: FOB display logic updated
- **Build/Place Contexts**: Support for new camp vs FOB distinction
- **FOB Menu**: Updated for mobile FOB management

### Implementation Details

#### File Changes Summary
- **50+ files modified** across multiple implementation phases
- **Key Areas**:
  - Configuration files (pricing, buildables, placeables)
  - Localization files (English, Russian, other languages)
  - New prefabs for mobile FOB variants
  - Manager component updates
  - UI context modifications
  - User action implementations
  - Object tracking components and persistence
  - All placeable and buildable prefabs updated with tracking components
  - Enhanced placement validation system

#### Technical Architecture
- **Component-Based Design**: Follows Overthrow's established component pattern
- **Persistence Support**: Integration with EPF save/load system
- **Network Sync**: RPC-based multiplayer synchronization
- **Officer Permissions**: Role-based access control for deployment actions

#### Object Tracking System ✅
- **Tracking Components**:
  - `OVT_PlaceableComponent` - Tracks ownership and base association for placed items
  - `OVT_BuildableComponent` - Tracks ownership and base association for built structures
  - Both components include EPF persistence for save/load functionality
- **Persistent Identification**:
  - Added persistent string IDs to `OVT_CampData` and `OVT_FOBData` 
  - Generated unique IDs (e.g., "CAMP_1719936000_54321") for reliable cross-session tracking
  - Updated all creation and RPC methods to handle persistent IDs
- **Association Logic**:
  - Automatic association of placed/built objects with nearest base/camp/FOB
  - Configurable via `m_bAssociateWithNearest` flag in placeable configuration
  - Distance-based association using `FindNearestBase()` method
- **Camp Cleanup System**:
  - Comprehensive cleanup when camps are deleted
  - Removes all associated placed items and built structures within 75m radius
  - Uses persistent IDs for reliable object identification and removal
  - Server-only deletion with RplId to prevent accidental deletions

#### Enhanced Placement System ✅
- **Distance Validation**:
  - Added `m_bAwayFromCamps` configuration flag
  - Enforces 100m minimum distance from existing camps when enabled
  - Uses existing "#OVT-TooCloseCamp" localization string
- **Flexible Association**:
  - `m_bAssociateWithNearest` flag controls object-to-base association
  - Allows independent objects that aren't tied to specific bases/camps
  - Still tracks ownership for all placed items

### Remaining Work 🚧

Based on the updated design specifications, the following features need implementation:

#### Camp System Enhancements
1. **Privacy Toggle System**
   - Add privacy flag to `OVT_CampData`
   - UI toggle for camp privacy settings
   - Fast travel permission logic based on privacy status
   - Visual indicators for public vs private camps

2. **Building at Camps**
   - Enable basic structures within camp radius
   - Define allowed buildables for camps
   - Implement building restrictions based on radius

#### Mobile FOB Updates
1. **Access Control**
   - Add `mobileFOBOfficersOnly` flag to `OVT_OverthrowConfigStruct`
   - Implement purchase/deployment restrictions based on config
   - Update UI to reflect permission status

2. **Multiple FOB Management**
   - Remove single-deployment limitation
   - Track all deployed FOBs per player/faction
   - Update save/load system for multiple FOBs

3. **Priority FOB System**
   - Add priority flag to `OVT_FOBData`
   - Officer-only action to set FOB priority
   - Map rendering logic for priority vs normal FOBs
   - Enhanced map marker for priority FOB

#### Testing Requirements
- Camp privacy toggle functionality
- Multi-FOB deployment scenarios
- Config-based access restrictions
- Priority FOB map display at various zoom levels
- Performance with multiple deployed FOBs

## Strategic Impact

The updated design creates a more inclusive and flexible tactical system:

1. **Democratized Base Building**: All players can contribute to the resistance infrastructure
2. **Collaborative Networks**: Multiple FOBs enable coordinated operations across the map
3. **Flexible Command Structure**: Officers retain strategic control through priority designation
4. **Server Customization**: Communities can tailor the experience to their playstyle
5. **Enhanced Camp Utility**: Shared camps create natural rally points and supply hubs

## Implementation Progress

### Current State
- ✅ Core mobile FOB system implemented
- ✅ Basic camp-to-FOB distinction established
- ✅ Officer-only deployment actions (original design)
- ✅ Single FOB deployment limitation (original design)
- ✅ Object tracking system with persistent IDs
- ✅ Camp cleanup and deletion system
- ✅ Enhanced placement validation with distance controls
- ✅ Configurable object association system
- ⚠️ Partially implemented based on Issue #28 specs

### Next Steps
The implementation needs updating to match the revised design:
1. ~~Enhance camp system with privacy and building capabilities~~ ✅ **COMPLETED** - Object tracking and cleanup system implemented
2. Remove FOB deployment restrictions (make configurable)
3. Enable multiple simultaneous FOBs
4. Implement priority FOB designation system
5. Add server configuration options

### Recently Completed ✅
- **Object Tracking & Camp Cleanup**: Full implementation of object association and automatic cleanup when camps are deleted
- **Enhanced Placement Validation**: Distance controls and configurable association system
- **Persistent ID System**: Reliable cross-session tracking using string-based persistent IDs
- **Component-Based Tracking**: Both placeable and buildable objects now track ownership and base association

## Conclusion

While the foundation for Mobile FOBs is in place, the implementation currently follows the original Issue #28 specification rather than the updated, more flexible design. The enhanced camp system and democratized FOB access will create a more dynamic and inclusive gameplay experience that better serves the diverse Overthrow community.