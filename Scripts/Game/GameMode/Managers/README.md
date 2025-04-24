# Overthrow Game Mode Managers

This directory contains the manager components that form the core systems of Overthrow for Arma Reforger. Each manager is responsible for handling a specific aspect of the game's functionality.

## Core Managers

### OVT_OverthrowConfigComponent
Handles the configuration settings for the Overthrow game mode, providing centralized access to game parameters and settings.

### OVT_PersistenceManagerComponent
Manages the persistence of game state, allowing the game world to be saved and restored between sessions.

### OVT_PlayerManagerComponent
Tracks and manages player entities, including player state, data, and interactions with the game world.

## Economy and Property

### OVT_EconomyManagerComponent
Controls the in-game economy including prices, trade, resources, and financial transactions.

### OVT_RealEstateManagerComponent
Handles all property-related functionality including ownership, purchasing, and management of buildings and land.

### OVT_TownManagerComponent
Manages town-related features including control status, population sentiment, and town-specific events.

## Factions and Ownership

### OVT_OwnerManagerComponent
Manages ownership of entities within the game world.

### OVT_RplOwnerManagerComponent
Handles replication of ownership data across the network to ensure consistency.

### Faction Managers
Located in the `Factions` subdirectory:

- **OVT_ResistanceFactionManager**: Controls the resistance faction dynamics and operations.
- **OVT_OccupyingFactionManager**: Manages the opposing occupying forces and their responses to player actions.

## Player Systems

### OVT_JobManagerComponent
Handles the generation, assignment, and completion of jobs and missions for players.

### OVT_NotificationManagerComponent
Manages the notification system for alerting players about important events.

### OVT_RespawnSystemComponent
Controls player respawning, including respawn locations and penalties.

### OVT_SkillManagerComponent
Manages player skills, progression, and advancement systems.

## World Systems

### OVT_VehicleManagerComponent
Handles vehicle spawning, persistence, and management within the game world.

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 