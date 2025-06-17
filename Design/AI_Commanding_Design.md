# AI Commanding Feature Design Document

## Overview
The AI Commanding feature allows players to recruit and command AI civilians in Overthrow. This system enables players to build their own squad of AI-controlled resistance fighters who can assist in various operations.

## Current Implementation Status

### Completed Features
1. **Basic Recruitment System**
   - `OVT_RecruitCivilianAction` user action added to civilian characters
   - Cost-based recruitment ($1000 default, configurable via difficulty settings)
   - Recruits civilians directly from the street
   - Adds recruited AI to player's group

2. **Group Management**
   - Automatic group creation for players on spawn
   - Players spawn with CIV faction affiliation
   - Group persists across respawns
   - AI agents can be added to player groups

3. **Localization Support**
   - Added localization strings for recruitment action
   - Support for multiple languages (EN, FR, RU, UK)

### Architecture Changes
- Modified `OVT_RespawnSystemComponent` to handle group creation and faction assignment
- Moved faction/group initialization from `OVT_PlayerWantedComponent` to respawn system
- Added `OVT_RecruitCivilianAction` for civilian interaction

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

### Phase 1: Core Systems (Current Sprint)
1. ✅ Basic recruitment action
2. ✅ Group management integration
3. ⬜ Persistent recruit data storage
4. ⬜ Basic AI command interface

### Phase 2: Progression and Buildings
1. ⬜ XP/Level system for AI
2. ⬜ Recruitment Center prefab and logic
3. ⬜ Training Camp prefab and logic
4. ⬜ Recruit management UI

### Phase 3: Advanced Features
1. ⬜ Loadout system
2. ⬜ Skill trees
3. ⬜ Advanced AI behaviors
4. ⬜ Looting command system

## Data Structures

### Recruit Data (Similar to OVT_PlayerData)
```cpp
class OVT_RecruitData : Managed
{
    // Identity
    string m_sRecruitId;
    string m_sName;
    string m_sOwnerPersistentId; // Player who owns this recruit
    
    // Progression
    int m_iKills = 0;
    int m_iXP = 0;
    int m_iLevel = 1;
    ref map<string, int> m_mSkills = new map<string, int>;
    
    // State
    bool m_bIsTraining = false;
    float m_fTrainingCompleteTime = 0;
    vector m_vLastKnownPosition = "0 0 0";
    
    // Methods similar to OVT_PlayerData
    int GetLevel() { return Math.Floor(1 + (0.1 * Math.Sqrt(m_iXP))); }
    int GetNextLevelXP() { return Math.Pow(GetLevel() / 0.1, 2); }
}
```

### Manager Component
```cpp
class OVT_RecruitManagerComponent : OVT_Component
{
    // Constants
    static const int MAX_RECRUITS_PER_PLAYER = 16;
    
    // Track all recruits by ID
    ref map<string, ref OVT_RecruitData> m_mRecruits;
    
    // Track recruits by owner
    ref map<string, ref array<string>> m_mRecruitsByOwner;
    
    // Handle recruitment, training, commands
    static OVT_RecruitManagerComponent s_Instance;
    static OVT_RecruitManagerComponent GetInstance() { return s_Instance; }
    
    // Helper methods
    int GetRecruitCount(string playerPersistentId)
    {
        if (!m_mRecruitsByOwner.Contains(playerPersistentId))
            return 0;
        return m_mRecruitsByOwner[playerPersistentId].Count();
    }
    
    bool CanRecruit(string playerPersistentId)
    {
        return GetRecruitCount(playerPersistentId) < MAX_RECRUITS_PER_PLAYER;
    }
}
```

### Persistence (Using EPF Pattern)
```cpp
[EPF_ComponentSaveDataType(OVT_RecruitManagerComponent)]
class OVT_RecruitSaveDataClass : EPF_ComponentSaveDataClass {};

[EDF_DbName.Automatic()]
class OVT_RecruitSaveData : EPF_ComponentSaveData
{
    ref map<string, ref OVT_RecruitData> m_mRecruits;
    ref map<string, ref array<string>> m_mRecruitsByOwner;
    
    override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
    {
        OVT_RecruitManagerComponent manager = OVT_RecruitManagerComponent.Cast(component);
        if (!manager) return EPF_EReadResult.ERROR;
        
        m_mRecruits = new map<string, ref OVT_RecruitData>;
        m_mRecruitsByOwner = new map<string, ref array<string>>;
        
        // Copy recruit data
        for (int i = 0; i < manager.m_mRecruits.Count(); i++)
        {
            string id = manager.m_mRecruits.GetKey(i);
            OVT_RecruitData recruit = manager.m_mRecruits.GetElement(i);
            m_mRecruits[id] = recruit;
        }
        
        // Copy ownership data
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

### Character Loadout Persistence
For recruit character entities and their loadouts, we'll use EPF's existing character persistence:
- Each recruit's character entity will have an `EPF_PersistenceComponent`
- The character's inventory, equipment, and stance will be saved using `EPF_CharacterSaveData`
- This is the same system used for player character persistence
- The recruit's entity ID will be tracked in `OVT_RecruitData` for restoration

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

## Next Steps
1. Create `OVT_RecruitManagerComponent` for centralized recruit tracking
2. Implement recruit persistence using EPF
3. Design and implement Recruitment Center building
4. Create basic recruit management UI
5. Implement XP/leveling system

## Design Decisions
1. **Maximum recruits per player**: 16 recruits
2. **Recruit death handling**: Permanent death (no respawn)
3. **Cross-player recruit sharing/trading**: Planned for future (low priority)
4. **AI behavior customization**: Use default game AI behavior
5. **Job system integration**: Not required