# Overthrow Placeables System

This directory contains components that handle placeable objects in the Overthrow game mode. Placeables are structures that players can build and deploy in the game world to expand their resistance operations.

## Core Components

### OVT_PlaceableHandler
Base class for all placeable handlers, providing common functionality and interfaces for different types of placeables. This abstract class defines the core behavior that specific placeable types inherit.

## Specific Placeable Handlers

### OVT_PlaceableFOBHandler
Handles Forward Operating Base (FOB) placeables, which serve as advanced resistance outposts with expanded functionality. FOBs allow players to establish strategic positions across the map.

### OVT_PlaceableCampHandler
Manages resistance camp placeables, which serve as fast travel destinations and basic operational positions for resistance forces.

### OVT_PlaceableSupportModHandler
Handles placeables that add a support modifier to towns (such as posters)

## Integration with Other Systems

Placeables work closely with several other systems in Overthrow:

1. **Persistence** - Placeables are saved between game sessions using the [Enfusion Persistence Framework](https://github.com/Arkensor/EnfusionPersistenceFramework/)
2. **Resistance Faction** - Placeables provide infrastructure for resistance operations
3. **Economy** - Building placeables requires resources from the game's economy system

When players place these structures, the appropriate handler is invoked to manage the placement, validation, construction, and functionality of the placeable object. Each handler implements specific logic for its placeable type while sharing common behavior through the base handler class.

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 