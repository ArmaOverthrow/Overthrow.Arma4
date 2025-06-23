# Overthrow Persistence System

This directory contains the components that handle game state persistence for the Overthrow game mode. Overthrow uses the [Enfusion Persistence Framework (EPF)](https://github.com/Arkensor/EnfusionPersistenceFramework/blob/armareforger/docs/index.md) to save and load game data between play sessions, allowing for a persistent world experience.

## Main Save Data Classes

### OVT_OverthrowSaveData
The primary save data container that holds global mode state, utilizing EPF's scripted states functionality to persist game-wide information.

### OVT_BaseUpgradeSaveData
Stores data related to base upgrades for the occupying faction's military installations, implemented using EPF's custom component save-data approach.

### OVT_BuildingSaveData
Manages persistence data for buildings and structures in the game world, working with EPF's baked map entities system.

### OVT_PlaceableSaveData
Handles persistence for player-placed objects and structures, leveraging EPF's world entity save-data capabilities.

## Component Save Data

Located in the `Components` subdirectory:

### OVT_PlayerSaveData
Stores player-specific data including money, skills, and progression, built on EPF's custom component save-data system.

### OVT_ResistanceSaveData
Manages persistence for the resistance faction, including resources, operational data, and faction-wide progress.

### OVT_TownSaveData
Stores data related to towns, including control status, population sentiment, and town-specific features.

### OVT_OccupyingFactionSaveData
Handles persistence for the occupying faction, including resources, force composition, alert status, and strategic data.

### OVT_RealEstateSaveData
Manages data related to player property ownership.

### OVT_ConfigSaveData
Stores configuration settings and parameters that persist between game sessions.

### OVT_EconomySaveData
Maintains the economic state of the game world, including prices, resources, and trade data.

## Implementation Details

The persistence system in Overthrow uses EPF's serialization methods to convert game objects and states into data that can be saved to disk. When players reconnect to a server or load a saved game, this data is deserialized and used to restore the game world to its previous state.

Each save data class defines what specific data should be serialized and how it should be reconstructed during loading. This modular approach allows different game systems to manage their own persistence needs while still maintaining a unified save/load process through the central EPF Persistence Manager.

## Enfusion Persistence Framework

Overthrow implements the [Enfusion Persistence Framework](https://github.com/Arkensor/EnfusionPersistenceFramework/) which provides:

- **Game Mode Integration**: Framework hooks into the Overthrow game mode lifecycle
- **Persistence Manager**: Central coordination of saving and loading operations
- **Component Setup**: Structure for making entities and components persistent
- **World Entity Save-Data**: System for tracking and restoring entities in the world
- **Custom Component Save-Data**: Support for Overthrow's unique component data
- **Versioning**: Handling data structure changes between game versions
- **Utilities**: Tools for managing persistence data and selective loading

For detailed information on how EPF works and how to extend it, refer to the [EPF Documentation](https://github.com/Arkensor/EnfusionPersistenceFramework/blob/armareforger/docs/index.md).

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 