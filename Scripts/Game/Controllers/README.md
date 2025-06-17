# Overthrow Game Controllers

This directory contains controller components that manage specific gameplay systems within Overthrow. Controllers handle the operational aspects of various features, often implementing the policies determined by manager components.

## Main Controllers

### OVT_TownController
Handles town-specific functionality at the operational level, implementing town control and management processes.

### OVT_PortController
Manages port facilities and related operations within the game world.

## Occupying Faction Controllers

Located in the `OccupyingFaction` subdirectory:

### OVT_QRFControllerComponent
Controls Quick Reaction Force spawning, movement, and tactical responses of the occupying faction to player activities.

### OVT_BaseControllerComponent
Manages military bases for the occupying faction, including functionality, upgrades, and responses.

### OVT_TowerControllerComponent
Controls radio/comms towers used by the occupying faction for surveillance and communications.

### Base Upgrades

Located in the `OccupyingFaction/BaseUpgrades` subdirectory:

- **OVT_BaseUpgrade**: Base class for all base upgrade components.
- **OVT_SlottedBaseUpgrade**: Handles upgrades that occupy specific slots within a base.
- **OVT_BasePatrolUpgrade**: Manages patrol-based upgrades for bases.
- **OVT_BaseUpgradeComposition**: Controls composition-based upgrades to bases.
- **OVT_BaseUpgradeCheckpoints**: Handles checkpoint placements around bases.
- **OVT_BaseUpgradeDefensePosition**: Manages static defense positions at bases.
- **OVT_BaseUpgradeDefensePatrol**: Controls defensive patrol routes around bases.
- **OVT_BaseUpgradeTowerGuard**: Manages guards positioned at towers.
- **OVT_BaseUpgradeSpecops**: Controls special operations forces stationed at bases.
- **OVT_BaseUpgradeTownPatrol**: Manages patrols that extend from bases into nearby towns.
- **OVT_BaseUpgradeParkedVehicles**: Controls vehicles parked at bases.

## Resistance Faction Controllers

Located in the `ResistanceFaction` subdirectory:

### OVT_ResistanceFOBControllerComponent
Manages Forward Operating Bases for the resistance faction, including construction, upgrades, and operational capabilities.

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 