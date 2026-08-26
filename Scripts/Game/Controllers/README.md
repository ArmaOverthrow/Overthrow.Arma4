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

Marks a military base: its slot registry (where structures, parking, defend positions and vehicle
patrol spawns are), which faction holds it, and where a QRF spawns from. It does **not** buy, spend or
spawn anything — base defense is a set of `Configs/Deployment/Deployment_Base*.conf` deployments.

### OVT_TowerControllerComponent

Controls radio/comms towers used by the occupying faction for surveillance and communications.

### Base defense

There is no per-base upgrade system any more. Every base-defense concern the occupying faction has —
garrison patrols, defense positions, tower guards, sniper positions, checkpoints, fortifications and
parked vehicles — is a `Configs/Deployment/Deployment_Base*.conf` deployment, bought out of the
deployment framework's single resource pool and with its groups owned by the virtualization core.

## Resistance Faction Controllers

Located in the `ResistanceFaction` subdirectory:

### OVT_ResistanceFOBControllerComponent

Manages Forward Operating Bases for the resistance faction, including construction, upgrades, and operational capabilities.

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/r3XN7uDdV2).
