# Overthrow Game Mode

This directory contains the core components that define the Overthrow game mode for Arma Reforger. Overthrow is a dynamic and persistent revolution platform where players work to liberate their homeland from occupying forces.

## Main Components

### OVT_OverthrowGameMode
The primary game mode class that orchestrates the overall gameplay experience, initializing and connecting various systems.

### OVT_OverthrowFactionManager
Manages the relationships and interactions between the different factions in the game world.

### OVT_TimeAndWeatherHandlerComponent
Extends the base game mode SCR_TimeAndWeatherHandlerComponent to provide Overthrow-specific functionality

## Subdirectories

### Managers/
Contains manager components that handle specific game systems such as economy, towns, player data, and more. These managers provide high-level oversight of their respective systems and manage controllers as needed.

### Placeables/
Contains components and systems for objects that can be placed in the world by players, such as resistance facilities, defenses, and other buildable structures.

### Systems/
Includes various gameplay systems that implement specific features and mechanics within the Overthrow game mode.

## Persistence

Saving and loading run on Reforger's own persistence system. `Managers/OVT_PersistenceManagerComponent.c` is the façade over it (save points, load, wipe), the serializers live in `Scripts/Game/Persistence/`, and the bindings are declared in `Configs/Systems/Persistence/Overthrow.conf`.

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 