# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Overthrow is a dynamic and persistent revolution mod for Arma Reforger/Arma 4. It's built using EnforceScript and the Enfusion engine's component-based architecture.

## Development Workflow

Development is done through Arma Reforger Tools:
- Open the project by loading `addon.gproj` in Arma Reforger Workbench
- Test world: `Worlds/MP/OVT_Campaign_Test.ent` (small, loads quickly)
- Full map: `Worlds/MP/OVT_Campaign_Eden.ent`
- Press F5 or click Play button in World Editor to test
- Build > Compile and Reload Scripts in Script Editor after changes

## Architecture Overview

### Component Pattern
All manager components follow this pattern:
```cpp
class OVT_SomeManagerComponentClass: OVT_ComponentClass {};
class OVT_SomeManagerComponent: OVT_Component {
    static OVT_SomeManagerComponent s_Instance;
    static OVT_SomeManagerComponent GetInstance() { return s_Instance; }
    void Init(IEntity owner) { ... }
}
```

### Key Systems
- **OVT_OverthrowGameMode**: Central game mode coordinating all systems
- **Manager Components**: Economy, Player, Town, Vehicle, RealEstate, Job, Skill managers
- **Persistence**: Each manager has corresponding SaveData class using EPF framework
- **Global Access**: `OVT_Global` provides static methods to access all managers

### Coding Conventions
- Class prefix: `OVT_`
- Member variables: `m_` prefix
- Type-specific prefixes: `m_a` (array), `m_m` (map), `m_i` (int), `m_f` (float), `m_s` (string), `m_b` (bool)
- Static instances: `s_Instance`
- Documentation: Doxygen style with `//!` comments
- Attributes: `[Attribute()]` for editor-exposed properties

### File Organization
- `Design/`: Design docs
- `Scripts/Game/Components/`: Entity components and UI components
- `Scripts/Game/GameMode/`: Core game logic and managers
- `Scripts/Game/Controllers/`: Base, town, and faction controllers
- `Scripts/Game/Configuration/`: Config classes for game systems
- `Scripts/Game/UI/`: UI contexts and widgets
- `Scripts/Game/UserActions/`: Player interaction actions
- `Configs/`: Game configuration files
- `Prefabs/`: Entity prefabs and compositions

### Persistence Pattern
SaveData classes follow this structure:
```cpp
[EPF_ComponentSaveDataType(OVT_SomeManagerComponent)]
class OVT_SomeSaveDataClass : EPF_ComponentSaveDataClass {};
class OVT_SomeSaveData : EPF_ComponentSaveData {
    void ReadFrom(OVT_SomeManagerComponent component) { ... }
    void ApplyTo(OVT_SomeManagerComponent component) { ... }
}
```

### Network Synchronization
- Components use RPC for multiplayer sync
- Rpl system handles replication
- Manager components typically exist only on server

### Important Notes
- Always use `OVT_Global.GetXXX()` to access manager instances
- Follow existing patterns when adding new components or systems
- Test in `OVT_Campaign_Test.ent` for faster iteration