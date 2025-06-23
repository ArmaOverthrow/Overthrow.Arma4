# Overthrow Game Components

This directory contains various component classes that are attached to entities within the Overthrow game mode. Components provide specific functionalities to the entities they are attached to.

## Base Components

### OVT_Component
Base class for Overthrow-specific components, providing common functionality shared across all Overthrow components.

### OVT_MainMenuContextOverrideComponent
Overrides the Overthrow main menu to provide context-specific menu options and interfaces (such as the import and warehouse menus)

### OVT_ParkingComponent
Manages vehicle parking functionality, including designation of parking areas and management of parked vehicles.

### OVT_PlayerOwnerComponent
Handles the ownership relationship between players and entities, tracking what entities belong to which players.

### OVT_SpawnPointComponent
Defines and controls spawn points for players and AI within the game world.

## Player Components

Located in the `Player` subdirectory:

### OVT_PlayerWantedComponent
Handles the wanted/reputation system for players, including wanted levels, criminal activities, and law enforcement responses.

### OVT_PlayerCommsComponent
Central point for all player-triggered functions to be run on the server. Each connected player will own one of these

### OVT_UIManagerComponent
Controls player-specific UI elements and interface functionality.

## Economy Components

Located in the `Economy` subdirectory:

### OVT_ShopComponent
Enables entities to function as shops, providing buying and selling functionality for various items and services.

## Damage Components

Located in the `Damage` subdirectory:

### Modded Components

Located in the `Damage/Modded` subdirectory:

#### SCR_CharacterDamageManagerComponent
A modified version of the base game's character damage manager, adapted for Overthrow's specific damage and medical systems.

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 