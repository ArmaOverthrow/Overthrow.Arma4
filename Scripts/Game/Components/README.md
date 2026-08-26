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

### OVT_UIManagerComponent

Controls player-specific UI elements and interface functionality.

## Controller Components

Located in the `Controller` subdirectory:

These are the client->server request seam. Each connected player owns one `OVT_OverthrowController` entity, and every domain that needs a client->server request carries a component on it: vehicles, real estate and warehouses, economy, shop transactions, resistance operations, FOBs and camps, recruits, loadouts, possession, jobs, campaign actions, fast travel, respawn, tower sabotage, container transfer, tutorials and admin commands. All of them are reached with `OVT_ControllerComponent<T>.Get()`, never through a getter on `OVT_Global`.

The single 2,001-line comms monolith that used to serve all of these - one component listed on both the game-mode prefab and the player-character prefab - was replaced domain by domain and deleted in Phase 10 of `docs/features/core/controller-migration/`. New client->server operations belong on a controller component; see the `overthrow-architecture` skill's `overthrow-controller.md`.

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

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/r3XN7uDdV2).
