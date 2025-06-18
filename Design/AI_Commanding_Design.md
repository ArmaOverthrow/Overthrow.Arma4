# AI Commanding Feature Design Document

## Overview
The AI Commanding feature allows players to recruit and command AI civilians in Overthrow. This system enables players to build their own squad of AI-controlled resistance fighters who can assist in various operations.

## Current Implementation Status

### Completed Features (Phase 1 - In Progress)
1. **Basic Recruitment System** ✅
   - `OVT_RecruitCivilianAction` user action added to civilian characters
   - Cost-based recruitment ($1000 default, configurable via difficulty settings)
   - Recruits civilians directly from the street
   - Adds recruited AI to player's group
   - **Status**: Fully functional - recruits can be hired and follow commands

2. **Group Management** ✅
   - Automatic group creation for players on spawn
   - Players spawn with CIV faction affiliation
   - Group persists across respawns
   - AI agents can be added to player groups
   - **Status**: Complete - recruits join player groups and respond to AI commands

3. **Localization Support** ✅
   - Added localization strings for recruitment action
   - Support for multiple languages (EN, FR, RU, UK)

### Architecture Changes
- Modified `OVT_RespawnSystemComponent` to handle group creation and faction assignment
- Moved faction/group initialization from `OVT_PlayerWantedComponent` to respawn system
- Added `OVT_RecruitCivilianAction` for civilian interaction

### Current Phase 1 Priorities (aicommanding branch)
1. **EPF Recruit Persistence** ✅
   - Save/load recruit data across sessions
   - Leverage existing character persistence for loadout/inventory (same as player)
   - Work out respawn mechanics when player logs back in

2. **Equipment System** 🔄
   - Remote inventory access for recruits (since base game offers no equipping interface)
   - Allow recruits to pick up items from ground/containers
   - Manual equipment management

3. **Recruit Management UI** 📋
   - Basic management screen accessible from main menu
   - Rename recruits functionality
   - Remove/dismiss recruits option

4. **Recruitment Tent/Center** 📋
   - Buildable structure at FOBs
   - Recruit from nearby town supporters at reduced cost
   - Alternative to street recruitment

5. **Loadout System** 📋
   - Save player loadouts at equipment boxes
   - Apply saved loadouts to nearby recruits using items from equipment box
   - Persist each player's loadouts in EPF
   - Officer ability to set resistance-wide loadouts for everyone

## Planned Features (From Issue #22)

### 1. Recruitment System Enhancements
- **Recruitment Center Building**
  - Buildable at FOBs and bases (~$2000 cost)
  - Reduced recruitment cost (~$250) when using center
  - Recruits drawn from nearest town with supporters

### 2. Recruit Progression System
- **Experience and Leveling**
  - Recruits start at level 0 with poor combat skills
  - Gain XP from combat, completing tasks
  - Improved accuracy and abilities with each level
  - Skill tree system for specialized roles (future)

- **Training Camp**
  - Base-only building (~$3000-4000 cost)
  - Train recruits to increase level
  - Time/cost increases with level
  - Recruits temporarily unavailable during training

### 3. Recruit Management
- **Management UI**
  - Access from main menu
  - View recruit stats, XP, level
  - Rename recruits
  - Manage equipment/loadouts

- **Map Integration**
  - Unique map icons for AI recruits
  - Similar to player icons but distinguishable
  - Track recruit locations

### 4. Combat and Behavior
- **Wanted System**
  - Recruits use same wanted system as players
  - Not targeted when unarmed unless in restricted areas
  - Inherit player's wanted level when in group

- **Equipment System**
  - Start with civilian clothes, no weapons
  - Manual inventory management
  - Future: Loadout templates and quick-equip

- **Death Handling**
  - Permanent death - recruits do not respawn
  - Dead recruits are removed from player's roster
  - Equipment on dead recruits can be looted

### 5. Utility Functions
- **Looting Command**
  - Command recruits to loot area
  - Automatic collection within radius
  - Deposits to nearest vehicle/container
  - No special vehicle requirements

## Technical Implementation Plan

### Phase 1: Core Individual Recruit System (Current - aicommanding branch)
**Target**: Beta testing on dev branch before Overthrow Reforger 1.3
1. ✅ Basic recruitment action
2. ✅ Group management integration  
3. ✅ EPF recruit persistence system
4. 🔄 Remote inventory/equipment interface
5. 📋 Recruit management UI (rename, dismiss)
6. 📋 Recruitment tent/center building
7. 📋 Loadout save/apply system with EPF persistence

### Phase 2: Squad-Based Operations
**Focus**: Map-level command and control
1. 📋 Barracks building at bases
2. 📋 Hire fully equipped squads
3. 📋 Map-based waypoint and order system
4. 📋 Squad-level AI behaviors

### Phase 3: Specialized Operations
**Focus**: Automated systems and specialized roles
1. 📋 Loot teams with area collection commands
2. 📋 Automated patrol squads
3. 📋 Checkpoint management teams
4. 📋 Advanced AI behaviors and specializations

**Legend**: ✅ Complete | 🔄 In Progress | 📋 Planned

## Development Status & Release Plan

### Current Status (aicommanding branch)
- **Functional prototype complete**: Players can recruit civilians and command them using base game AI commanding
- **Next milestone**: Complete Phase 1 features for beta testing
- **Target release**: Overthrow Reforger 1.3

### Beta Testing Plan
Once Phase 1 is complete, the feature will be released on the dev branch for community testing before being merged into the main release.

### Key Technical Challenges
1. **Recruit Persistence**: Respawning recruited characters when players reconnect ✅ **COMPLETE**
2. **Equipment Interface**: Base game provides no recruit equipping UI - implementing remote inventory access
3. **Performance**: Managing multiple AI entities per player in multiplayer environment

### Recruit Persistence Implementation ✅ **COMPLETE**

**Investigation Summary**: 
The EPF (Enfusion Persistence Framework) that Overthrow already uses provides robust character persistence. Each entity with an `EPF_PersistenceComponent` is automatically saved/loaded with full inventory, position, and component state. The `OVT_RespawnSystemComponent` extends `EPF_BaseRespawnSystemComponent` which handles async database queries and character lifecycle.

**Implementation Status**: ✅ **COMPLETE** - Recruits are now fully persistent with proper name restoration and AI activation.

**Solution**:
1. **Auto-Persistence**: Add `EPF_PersistenceComponent` to recruited characters automatically
2. **Unique IDs**: Format: `recruit_<ownerPersistentId>_<sequence>` for database tracking
3. **Lifecycle Management**: 
   - **Player Connect**: Query database for recruits, respawn entities, rejoin to player group
   - **Player Disconnect**: Start despawn timer (5-10 minutes), save current state
   - **Timeout**: Remove from world but keep database records for next login

**Technical Details**:
- **Character Persistence**: Use existing `EPF_CharacterSaveData` (same as players)
- **Manager Integration**: `OVT_RecruitManagerComponent` tracks ownership and timers
- **Group Reconnection**: Leverage existing group management in `OVT_RespawnSystemComponent`
- **Database**: EPF handles all save/load operations automatically

## Data Structures

### Recruit Data (Simplified with EPF Integration)
```cpp
class OVT_RecruitData : Managed
{
    // Identity (character entity persistence handled by EPF)
    string m_sRecruitPersistentId;    // EPF persistence ID: "recruit_<ownerPersistentId>_<sequence>"
    string m_sName;                   // Custom recruit name
    string m_sOwnerPersistentId;      // Player who owns this recruit
    
    // Progression (stored separately from character entity)
    int m_iKills = 0;
    int m_iXP = 0;
    int m_iLevel = 1;
    ref map<string, int> m_mSkills = new map<string, int>;
    
    // Management state
    bool m_bIsTraining = false;
    float m_fTrainingCompleteTime = 0;
    float m_fOfflineTimer = 0;        // Countdown to despawn when owner offline
    bool m_bSpawnedInWorld = false;   // Track if character entity exists
    
    // EPF handles: position, inventory, equipment, health, etc.
    
    // Methods
    int GetLevel() { return Math.Floor(1 + (0.1 * Math.Sqrt(m_iXP))); }
    int GetNextLevelXP() { return Math.Pow(GetLevel() / 0.1, 2); }
    
    static string GeneratePersistentId(string ownerPersistentId, int sequence)
    {
        return string.Format("recruit_%1_%2", ownerPersistentId, sequence);
    }
}
```

### Manager Component (Integrated with EPF)
```cpp
class OVT_RecruitManagerComponent : OVT_Component
{
    // Constants
    static const int MAX_RECRUITS_PER_PLAYER = 16;
    static const float OFFLINE_DESPAWN_TIME = 600.0; // 10 minutes
    
    // Track recruit metadata (EPF handles character entities)
    ref map<string, ref OVT_RecruitData> m_mRecruits;          // By recruit persistent ID
    ref map<string, ref array<string>> m_mRecruitsByOwner;     // By owner persistent ID
    
    // Offline player timers
    ref map<string, float> m_mOfflinePlayerTimers;
    
    static OVT_RecruitManagerComponent s_Instance;
    static OVT_RecruitManagerComponent GetInstance() { return s_Instance; }
    
    // Recruitment
    bool CanRecruit(string playerPersistentId)
    {
        return GetRecruitCount(playerPersistentId) < MAX_RECRUITS_PER_PLAYER;
    }
    
    OVT_RecruitData CreateRecruit(string ownerPersistentId, IEntity characterEntity)
    {
        int sequence = GetNextRecruitSequence(ownerPersistentId);
        string recruitId = OVT_RecruitData.GeneratePersistentId(ownerPersistentId, sequence);
        
        // Add EPF persistence to character
        EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(characterEntity);
        if (!persistence)
        {
            // Add persistence component dynamically
            persistence = EPF_PersistenceComponent.Cast(characterEntity.CreateComponent("EPF_PersistenceComponent"));
        }
        persistence.SetPersistentId(recruitId);
        
        // Create recruit data
        OVT_RecruitData recruit = new OVT_RecruitData();
        recruit.m_sRecruitPersistentId = recruitId;
        recruit.m_sOwnerPersistentId = ownerPersistentId;
        recruit.m_sName = "Recruit " + sequence;
        recruit.m_bSpawnedInWorld = true;
        
        // Register recruit
        m_mRecruits[recruitId] = recruit;
        if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
            m_mRecruitsByOwner[ownerPersistentId] = {};
        m_mRecruitsByOwner[ownerPersistentId].Insert(recruitId);
        
        return recruit;
    }
    
    // Player connection events
    void OnPlayerConnected(string playerPersistentId)
    {
        m_mOfflinePlayerTimers.Remove(playerPersistentId);
        RespawnPlayerRecruits(playerPersistentId);
    }
    
    void OnPlayerDisconnected(string playerPersistentId)
    {
        m_mOfflinePlayerTimers[playerPersistentId] = OFFLINE_DESPAWN_TIME;
        SavePlayerRecruits(playerPersistentId);
    }
    
    // EPF Integration
    void RespawnPlayerRecruits(string playerPersistentId)
    {
        if (!m_mRecruitsByOwner.Contains(playerPersistentId))
            return;
            
        foreach (string recruitId : m_mRecruitsByOwner[playerPersistentId])
        {
            OVT_RecruitData recruit = m_mRecruits[recruitId];
            if (!recruit || recruit.m_bSpawnedInWorld)
                continue;
                
            // Query EPF for recruit character entity
            EPF_PersistenceManager.GetInstance().LoadAsync(EPF_EntitySaveData, recruitId, this, "OnRecruitLoaded");
        }
    }
    
    void OnRecruitLoaded(EPF_ELoadResult result, EPF_EntitySaveData saveData)
    {
        if (result != EPF_ELoadResult.SUCCESS || !saveData)
            return;
            
        // Spawn recruit character entity
        IEntity recruitEntity = saveData.Spawn();
        if (!recruitEntity)
            return;
            
        string recruitId = saveData.GetId();
        OVT_RecruitData recruit = m_mRecruits[recruitId];
        if (recruit)
        {
            recruit.m_bSpawnedInWorld = true;
            
            // Add to player's group
            AddRecruitToPlayerGroup(recruit.m_sOwnerPersistentId, recruitEntity);
        }
    }
    
    void SavePlayerRecruits(string playerPersistentId)
    {
        if (!m_mRecruitsByOwner.Contains(playerPersistentId))
            return;
            
        foreach (string recruitId : m_mRecruitsByOwner[playerPersistentId])
        {
            OVT_RecruitData recruit = m_mRecruits[recruitId];
            if (!recruit || !recruit.m_bSpawnedInWorld)
                continue;
                
            // Find character entity and save via EPF
            IEntity recruitEntity = FindRecruitEntity(recruitId);
            if (recruitEntity)
            {
                EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(recruitEntity);
                if (persistence)
                    persistence.Save();
            }
        }
    }
    
    // Offline timer processing
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        GetGame().GetCallqueue().CallLater(ProcessOfflineTimers, 1000, true); // Every second
    }
    
    void ProcessOfflineTimers()
    {
        array<string> toRemove = {};
        foreach (string playerPersistentId, float timer : m_mOfflinePlayerTimers)
        {
            timer -= 1.0;
            if (timer <= 0)
            {
                DespawnPlayerRecruits(playerPersistentId);
                toRemove.Insert(playerPersistentId);
            }
            else
            {
                m_mOfflinePlayerTimers[playerPersistentId] = timer;
            }
        }
        
        foreach (string playerPersistentId : toRemove)
        {
            m_mOfflinePlayerTimers.Remove(playerPersistentId);
        }
    }
    
    void DespawnPlayerRecruits(string playerPersistentId)
    {
        if (!m_mRecruitsByOwner.Contains(playerPersistentId))
            return;
            
        foreach (string recruitId : m_mRecruitsByOwner[playerPersistentId])
        {
            OVT_RecruitData recruit = m_mRecruits[recruitId];
            if (!recruit || !recruit.m_bSpawnedInWorld)
                continue;
                
            IEntity recruitEntity = FindRecruitEntity(recruitId);
            if (recruitEntity)
            {
                // Save before despawn
                EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(recruitEntity);
                if (persistence)
                    persistence.Save();
                    
                // Remove from world
                SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
                recruit.m_bSpawnedInWorld = false;
            }
        }
    }
}
```

### Persistence (Simplified with EPF Integration)
```cpp
[EPF_ComponentSaveDataType(OVT_RecruitManagerComponent)]
class OVT_RecruitSaveDataClass : EPF_ComponentSaveDataClass {};

[EDF_DbName.Automatic()]
class OVT_RecruitSaveData : EPF_ComponentSaveData
{
    // Only store recruit metadata - EPF handles character entities separately
    ref map<string, ref OVT_RecruitData> m_mRecruits;
    ref map<string, ref array<string>> m_mRecruitsByOwner;
    
    override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
    {
        OVT_RecruitManagerComponent manager = OVT_RecruitManagerComponent.Cast(component);
        if (!manager) return EPF_EReadResult.ERROR;
        
        // Copy only recruit metadata (not character entities)
        m_mRecruits = new map<string, ref OVT_RecruitData>;
        m_mRecruitsByOwner = new map<string, ref array<string>>;
        
        for (int i = 0; i < manager.m_mRecruits.Count(); i++)
        {
            string id = manager.m_mRecruits.GetKey(i);
            OVT_RecruitData recruit = manager.m_mRecruits.GetElement(i);
            m_mRecruits[id] = recruit;
        }
        
        for (int i = 0; i < manager.m_mRecruitsByOwner.Count(); i++)
        {
            string owner = manager.m_mRecruitsByOwner.GetKey(i);
            array<string> recruits = manager.m_mRecruitsByOwner.GetElement(i);
            m_mRecruitsByOwner[owner] = recruits;
        }
        
        return EPF_EReadResult.OK;
    }
    
    override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
    {
        OVT_RecruitManagerComponent manager = OVT_RecruitManagerComponent.Cast(component);
        if (!manager) return EPF_EApplyResult.ERROR;
        
        if (m_mRecruits)
            manager.m_mRecruits = m_mRecruits;
            
        if (m_mRecruitsByOwner)
            manager.m_mRecruitsByOwner = m_mRecruitsByOwner;
            
        return EPF_EApplyResult.OK;
    }
}
```

### Character Entity Persistence (Automatic via EPF)
**No Additional Code Required** - EPF handles everything automatically:

- **Character Entities**: Each recruit gets `EPF_PersistenceComponent` with unique ID
- **Full State Saving**: Position, rotation, inventory, equipment, health, stance
- **Database Operations**: Async save/load operations via `EPF_PersistenceManager`  
- **Loadout Persistence**: Uses existing `EPF_CharacterSaveData` (same as players)
- **Deferred Loading**: Complex component dependencies handled automatically

**Benefits**:
- **Zero Custom Code**: Leverage existing robust character persistence
- **Identical to Players**: Same reliability and feature set
- **Automatic Management**: No manual save/load timing or state tracking
- **Performance Optimized**: EPF change tracking reduces unnecessary database writes

## Recruit Lifecycle Management

### Recruitment Process
1. **Player Action**: Uses `OVT_RecruitCivilianAction` on civilian character
2. **Character Conversion**: Civilian entity becomes recruit with added `EPF_PersistenceComponent`
3. **ID Assignment**: Unique persistent ID: `recruit_<ownerPersistentId>_<sequence>`
4. **Registration**: `OVT_RecruitManagerComponent` tracks ownership and metadata
5. **Group Assignment**: Recruit joins player's group using existing group system

### Player Connection Events

#### Player Connects/Logs In
```
1. OVT_RespawnSystemComponent.OnPlayerConnected()
2. → OVT_RecruitManagerComponent.OnPlayerConnected()
3. → Remove offline timer for player
4. → RespawnPlayerRecruits()
   - Query EPF database for each recruit persistent ID
   - Async load recruit character entities
   - OnRecruitLoaded() callback spawns entities and rejoins to player group
```

#### Player Disconnects/Logs Out
```
1. OVT_RespawnSystemComponent.OnPlayerDisconnected()
2. → OVT_RecruitManagerComponent.OnPlayerDisconnected()
3. → Start offline timer (10 minutes)
4. → SavePlayerRecruits()
   - Force save all recruit character entities via EPF
   - Maintains current state in database
```

### Offline Timer System

#### Timer Processing (Every Second)
```
1. ProcessOfflineTimers()
2. → Decrement timers for offline players
3. → If timer reaches 0:
   - DespawnPlayerRecruits()
   - Save recruit states
   - Remove entities from world
   - Keep database records intact
```

#### Quick Reconnection (Within Timer Window)
```
1. Player logs back in before timeout
2. → Cancel despawn timer
3. → Recruits remain in world (no respawn needed)
4. → Rejoin to player's group
```

### Death and Persistence

#### Recruit Death
```
1. Character dies in combat
2. → EPF automatically saves death state
3. → OVT_RecruitManagerComponent removes from tracking
4. → Dead body persists for looting (EPF handles)
5. → No respawn - permanent death
```

#### Database Management
```
- Recruit Metadata: Stored in OVT_RecruitManagerComponent save data
- Character Data: Stored separately via EPF system
- Cleanup: Dead recruits removed from manager, EPF garbage collection handles entities
```

### Integration Points

#### With OVT_RespawnSystemComponent
- Hook into existing player connect/disconnect events
- Leverage group management for recruit assignment
- Use same faction management (CIV faction)

#### With EPF_PersistenceManager
- Automatic character entity persistence
- Async database operations
- Change tracking optimization
- Garbage collection of old entities

#### Performance Considerations
- **Staggered Loading**: Use async EPF loading to prevent server hitches
- **Timer Optimization**: 1-second timer resolution for offline processing
- **Memory Management**: Only load recruits for online players
- **Database Efficiency**: EPF change tracking minimizes unnecessary saves

## Dependencies
- Arma Reforger AI Commanding feature (must be released)
- EPF for persistence
- Group system modifications
- UI framework for management screens

## Risk Factors
1. AI commanding feature availability in base game
2. Performance impact with many AI recruits
3. Multiplayer synchronization complexity
4. Save data size with many recruits

## Next Steps (Phase 1 Completion)
1. **Remote Equipment Interface** - Create inventory access system for recruit equipment management  
2. **Recruit Management UI** - Basic screen for renaming and dismissing recruits
3. **Recruitment Tent** - Buildable structure for cheaper recruitment from town supporters
4. **Loadout System** - Save/apply loadout functionality with EPF persistence
5. **Beta Testing** - Release on dev branch for community feedback

## Design Decisions
1. **Maximum recruits per player**: 16 recruits
2. **Recruit death handling**: Permanent death (no respawn)
3. **Cross-player recruit sharing/trading**: Planned for future (low priority)
4. **AI behavior customization**: Use default game AI behavior
5. **Job system integration**: Not required