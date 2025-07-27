[ComponentEditorProps(category: "GameScripted/Events", description: "Convoy Event Manager")]
class TSE_ConvoyEventManagerComponentClass : ScriptComponentClass {}

class TSE_ConvoyEventManagerComponent : ScriptComponent
{
    ref array<IEntity> m_ConvoySpawnMarkers = new array<IEntity>();
    ref array<IEntity> m_ConvoyDestinationMarkers = new array<IEntity>();

    [Attribute(defvalue: "6", uiwidget: UIWidgets.EditBox, desc: "Интервал между событиями (часы)")]
    int m_iIntervalHours;

    [Attribute("", UIWidgets.Object, desc: "Конфигурация груза конвоя")]
    ref TSE_ConvoyCargoConfig m_CargoConfig;

    float m_fEventStartHour;
    bool m_bEventActive;
    ref array<EntityID> m_SpawnedEntities;
    ref array<EntityID> m_ConvoyCrewIDs;
    ref map<EntityID, EntityID> m_mPendingCrewAssignments = new map<EntityID, EntityID>();
    
    // Convoy status tracking
    EntityID m_ConvoyVehicleID;
    vector m_vDestinationPos;
    vector m_vStartPos;
    
    // Marker update system
    float m_fLastMarkerUpdate;
    static const float MARKER_UPDATE_INTERVAL = 300; // 5 minutes in seconds
    
    // Success reward
    static const int CONVOY_SUCCESS_REWARD = 500;
    
    // Static variables for convoy marker tracking (accessible by map system)
    static vector s_vActiveConvoyPos = vector.Zero;
    static bool s_bConvoyMarkerVisible = false;

    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        m_SpawnedEntities = new array<EntityID>();
        m_ConvoyCrewIDs = new array<EntityID>();
        
        Print("[ConvoyEvent] OnPostInit: " + owner.GetName() + " | m_bEventActive=" + m_bEventActive);
        GetGame().GetCallqueue().CallLater(WaitForGameModeInitialized, 1000, false);
    }

    void WaitForGameModeInitialized()
    {
        OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if (!mode || !mode.IsInitialized())
        {
            GetGame().GetCallqueue().CallLater(WaitForGameModeInitialized, 1000, false);
            return;
        }
        
        Print("[ConvoyEvent] OverthrowGameMode initialized, starting convoy event logic");
        
        if(m_ConvoySpawnMarkers.IsEmpty())
        {
            Print("[ConvoyEvent] No convoy spawn markers registered!");
        }
        
        if(m_ConvoyDestinationMarkers.IsEmpty())
        {
            Print("[ConvoyEvent] No convoy destination markers registered!");
        }
        
        // Start convoy event after 8 minutes to avoid conflicts with vehicle cleanup systems
        GetGame().GetCallqueue().CallLater(TryStartEvent, 480000, false); // 8 minutes = 480,000 ms
    }

    void TryStartEvent()
    {
        Print("[ConvoyEvent] TryStartEvent | m_bEventActive=" + m_bEventActive);
        if (m_bEventActive)
            return;
        StartEvent();
    }

    // Registration methods for spawn and destination markers
    void RegisterConvoySpawnMarker(IEntity marker)
    {
        if(!marker) return;
        if(m_ConvoySpawnMarkers.Contains(marker)) return;
        m_ConvoySpawnMarkers.Insert(marker);
        Print("[ConvoyEvent] Registered convoy spawn marker: " + marker.GetOrigin());
    }
    
    void RegisterConvoyDestinationMarker(IEntity marker)
    {
        if(!marker) return;
        if(m_ConvoyDestinationMarkers.Contains(marker)) return;
        m_ConvoyDestinationMarkers.Insert(marker);
        Print("[ConvoyEvent] Registered convoy destination marker: " + marker.GetOrigin());
    }

    void StartEvent()
    {
        if (m_bEventActive) {
            Print("[ConvoyEvent] StartEvent: already active, skipping");
            return;
        }
        
        // Check if we have spawn and destination points
        if (m_ConvoySpawnMarkers.IsEmpty() || m_ConvoyDestinationMarkers.IsEmpty()) {
            Print("[ConvoyEvent] Not enough spawn/destination markers to start event");
            GetGame().GetCallqueue().CallLater(TryStartEvent, m_iIntervalHours * 3600 * 1000, false);
            return;
        }
        
        m_bEventActive = true;
        Print("[ConvoyEvent] Starting convoy event");
        
        // Get random spawn and destination positions
        vector spawnAngles;
        m_vStartPos = GetRandomSpawnPosition(spawnAngles);
        m_vDestinationPos = GetRandomDestinationPosition();
        
                 // Check if positions are the same (this shouldn't happen)
         if (vector.Distance(m_vStartPos, m_vDestinationPos) < 10.0) {
             Print("[ConvoyEvent] ERROR: Start and destination positions are too close! Distance: " + vector.Distance(m_vStartPos, m_vDestinationPos));
         }
        
        // Get occupying faction data
        OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
        if (!config) {
            Print("[ConvoyEvent] ERROR - Could not get config component!");
            EndEvent();
            return;
        }
        
        string occupyingFactionKey = config.m_sOccupyingFaction;
        OVT_Faction factionData = config.GetOccupyingFaction();
        if (!factionData) {
            Print("[ConvoyEvent] ERROR - Could not get faction data for: " + occupyingFactionKey);
            EndEvent();
            return;
        }
        
        SpawnConvoy(factionData, spawnAngles);
        
        // Send notifications and setup monitoring
        NotifyConvoyEventStart();
        
        ChimeraWorld world = GetGame().GetWorld();
        TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
        TimeContainer time = t.GetTime();
        m_fEventStartHour = time.m_iHours;
        m_fLastMarkerUpdate = 0;
        
        Print("[ConvoyEvent] Event started at hour: " + m_fEventStartHour);
        GetGame().GetCallqueue().CallLater(MonitorEvent, 60000, true); // Check every 60 seconds (give more time for crew to initialize)
    }

    vector GetRandomSpawnPosition(out vector spawnAngles)
    {
        spawnAngles = vector.Zero;
        
        if (m_ConvoySpawnMarkers.IsEmpty()) {
            Print("[ConvoyEvent] ERROR: No spawn markers available!");
            return Vector(0,0,0);
        }
        
        int idx = Math.RandomInt(0, m_ConvoySpawnMarkers.Count());
        IEntity spawnMarker = m_ConvoySpawnMarkers[idx];
        
        // Get both position and orientation from spawn marker
        spawnAngles = spawnMarker.GetAngles();
        vector spawnPos = spawnMarker.GetOrigin();

        return spawnPos;
    }
    
    vector GetRandomDestinationPosition()
    {
        if (m_ConvoyDestinationMarkers.IsEmpty()) {
            Print("[ConvoyEvent] ERROR: No destination markers available!");
            return Vector(0,0,0);
        }
        
        int idx = Math.RandomInt(0, m_ConvoyDestinationMarkers.Count());
        vector destPos = m_ConvoyDestinationMarkers[idx].GetOrigin();

        return destPos;
    }

    void SpawnConvoy(OVT_Faction factionData, vector spawnAngles = "0 0 0")
    {
        // Get truck prefab from faction data
        array<ResourceName> truckPrefabs = factionData.m_aVehicleTruckPrefabSlots;
        if (truckPrefabs.IsEmpty()) {
            Print("[ConvoyEvent] ERROR - No truck prefabs available for faction!");
            return;
        }
        
        ResourceName truckPrefab = truckPrefabs[Math.RandomInt(0, truckPrefabs.Count())];
        
        // Get crew group from faction data 
        array<ref OVT_FactionGroupEntry> groups = factionData.m_GroupRegistry.m_aGroupEntries;
        ResourceName crewPrefab;
        foreach (OVT_FactionGroupEntry entry : groups) {
            if (entry.m_sGroupName == "Squad") {
                crewPrefab = entry.m_sGroupPrefab;
                break;
            }
        }
        
        if (crewPrefab.IsEmpty()) {
            Print("[ConvoyEvent] ERROR - No Squad group found for faction!");
            return;
        }
        
        // Spawn truck
        vector truckSpawnPos = m_vStartPos + "0 0 0";
        IEntity truck = OVT_Global.SpawnEntityPrefab(truckPrefab, truckSpawnPos);
        if (truck) {
            m_SpawnedEntities.Insert(truck.GetID());
            m_ConvoyVehicleID = truck.GetID();
            
            // Protect convoy truck from auto-cleanup systems
            ProtectVehicleFromCleanup(truck);
            
                         // Orient truck - use spawn marker angles if available, otherwise calculate direction to destination
             if (spawnAngles != vector.Zero)
             {
                 truck.SetAngles(spawnAngles);
             }
             else
             {
                 SetVehicleOrientation(truck, m_vStartPos, m_vDestinationPos);
             }
            
            Print("[ConvoyEvent] Convoy truck spawned: " + truck.ToString());
            
            // Fill truck with cargo
            FillTruckWithCargo(truck);
        }
        
        // Spawn crew
        vector crewSpawnPos = m_vStartPos + "5 0 5";
        IEntity crewGroup = OVT_Global.SpawnEntityPrefab(crewPrefab, crewSpawnPos);
        if (crewGroup && truck) {
            m_SpawnedEntities.Insert(crewGroup.GetID());
            
                         // Orient crew group - use spawn marker angles if available, otherwise calculate direction to destination
             if (spawnAngles != vector.Zero)
             {
                 crewGroup.SetAngles(spawnAngles);
             }
             else
             {
                 SetVehicleOrientation(crewGroup, m_vStartPos, m_vDestinationPos);
             }
            
            // Collect crew member IDs
            m_ConvoyCrewIDs.Clear();
            IEntity child = crewGroup.GetChildren();
            while (child) {
                if (CharacterEntity.Cast(child)) {
                    m_ConvoyCrewIDs.Insert(child.GetID());
                }
                child = child.GetSibling();
            }
            Print("[ConvoyEvent] Total crew members registered: " + m_ConvoyCrewIDs.Count());
            
            SCR_AIGroup aiGroup = SCR_AIGroup.Cast(crewGroup);
            if (aiGroup) {
                m_mPendingCrewAssignments.Insert(aiGroup.GetID(), truck.GetID());
                aiGroup.GetOnInit().Insert(OnCrewGroupInitialized);
                Print("[ConvoyEvent] Subscribed to OnInit for crew group " + aiGroup.GetID());
            }
            
            Print("[ConvoyEvent] Convoy crew spawned: " + crewGroup.ToString() + ", crew members: " + m_ConvoyCrewIDs.Count());
        }
    }

    void OnCrewGroupInitialized(SCR_AIGroup group)
    {
        // Similar to smugglers event - board crew into vehicle
        if (!group) return;
        EntityID groupID = group.GetID();
        if (!m_mPendingCrewAssignments.Contains(groupID)) {
            Print("[ConvoyEvent] OnCrewGroupInitialized: no pending assignment for group " + groupID);
            return;
        }
        
        EntityID vehicleID = m_mPendingCrewAssignments.Get(groupID);
        IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehicleID);
        if (!vehicle) {
            Print("[ConvoyEvent] OnCrewGroupInitialized: vehicle not found for group " + groupID);
            m_mPendingCrewAssignments.Remove(groupID);
            return;
        }
        
        SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
        if (!compMgr) {
            Print("[ConvoyEvent] OnCrewGroupInitialized: vehicle has no compartment manager");
            m_mPendingCrewAssignments.Remove(groupID);
            return;
        }
        
                 array<AIAgent> agents = {};
         group.GetAgents(agents);
         
         int agentIndex = 0;
         // Driver
         if (agentIndex < agents.Count()) {
             MoveAgentToCompartment(compMgr, agents[agentIndex], ECompartmentType.PILOT);
             agentIndex++;
         }
         
         // Passengers
         while (agentIndex < agents.Count()) {
             if (!MoveAgentToCompartment(compMgr, agents[agentIndex], ECompartmentType.CARGO)) break;
             agentIndex++;
         }
         
         m_mPendingCrewAssignments.Remove(groupID);
        
        // Give the group a move task to destination
        GiveConvoyMoveOrder(group);
    }

    bool MoveAgentToCompartment(SCR_BaseCompartmentManagerComponent compMgr, AIAgent agent, ECompartmentType type)
    {
        if (!agent || !agent.GetControlledEntity()) return false;
        IEntity character = agent.GetControlledEntity();
        SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
        if (!access) return false;
        IEntity vehicle = compMgr.GetOwner();
        if (!vehicle) return false;
        return access.MoveInVehicle(vehicle, type);
    }

    void GiveConvoyMoveOrder(SCR_AIGroup group)
    {
        // Give the group a waypoint to move to destination
        Print("[ConvoyEvent] Giving move order to convoy group to destination: " + m_vDestinationPos);
        
        OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
        if (!config) {
            Print("[ConvoyEvent] Could not get config component for waypoint creation");
            return;
        }
        
        // Clear any existing waypoints
        array<AIWaypoint> waypoints = {};
        group.GetWaypoints(waypoints);
        foreach(AIWaypoint wp : waypoints) {
            group.RemoveWaypoint(wp);
            SCR_EntityHelper.DeleteEntityAndChildren(wp);
        }
        
        // Create search and destroy waypoint at destination
        AIWaypoint moveWaypoint = config.SpawnSearchAndDestroyWaypoint(m_vDestinationPos);
        if (moveWaypoint) {
            group.AddWaypoint(moveWaypoint);
            Print("[ConvoyEvent] Added move waypoint to convoy at: " + m_vDestinationPos);
        } else {
            Print("[ConvoyEvent] Failed to create move waypoint");
        }
    }

    void FillTruckWithCargo(IEntity truck)
    {
        if (!truck || !m_CargoConfig) return;
        
        InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(truck.FindComponent(InventoryStorageManagerComponent));
        if (!invMgr) {
            Print("[ConvoyEvent] Truck has no InventoryStorageManagerComponent");
            return;
        }
        
        OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
        if (!economy) {
            Print("[ConvoyEvent] Could not get economy manager");
            return;
        }
        
        // Fill with weapons
        FillCargoCategory(invMgr, economy, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 
                         m_CargoConfig.m_iWeaponsChance, m_CargoConfig.m_iWeaponsMinCount, m_CargoConfig.m_iWeaponsMaxCount, "Weapons");
        
        // Fill with ammunition
        FillCargoCategory(invMgr, economy, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.AMMUNITION, 
                         m_CargoConfig.m_iAmmunitionChance, m_CargoConfig.m_iAmmunitionMinCount, m_CargoConfig.m_iAmmunitionMaxCount, "Ammunition");
        
        // Fill with attachments
        FillCargoCategory(invMgr, economy, SCR_EArsenalItemType.WEAPON_ATTACHMENT, SCR_EArsenalItemMode.DEFAULT, 
                         m_CargoConfig.m_iAttachmentsChance, m_CargoConfig.m_iAttachmentsMinCount, m_CargoConfig.m_iAttachmentsMaxCount, "Attachments");
        
        // Fill with throwables (grenades)
        FillCargoCategory(invMgr, economy, SCR_EArsenalItemType.LETHAL_THROWABLE, SCR_EArsenalItemMode.DEFAULT, 
                         m_CargoConfig.m_iThrowableChance, m_CargoConfig.m_iThrowableMinCount, m_CargoConfig.m_iThrowableMaxCount, "Throwables");
        
                 // Fill with explosives
         FillCargoCategory(invMgr, economy, SCR_EArsenalItemType.EXPLOSIVES, SCR_EArsenalItemMode.DEFAULT, 
                          m_CargoConfig.m_iExplosivesChance, m_CargoConfig.m_iExplosivesMinCount, m_CargoConfig.m_iExplosivesMaxCount, "Explosives");
    }
    
    void FillCargoCategory(InventoryStorageManagerComponent invMgr, OVT_EconomyManagerComponent economy, 
                          SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode, 
                          int chance, int minCount, int maxCount, string categoryName)
    {
        // Check spawn chance
        if (Math.RandomInt(0, 100) >= chance) {
            Print("[ConvoyEvent] Skipping " + categoryName + " category due to chance roll");
            return;
        }
        
        // Get faction-specific items
        array<SCR_EntityCatalogEntry> availableItems = new array<SCR_EntityCatalogEntry>();
        if (!GetOccupyingFactionItems(economy, itemType, itemMode, availableItems)) {
            Print("[ConvoyEvent] No items found for category: " + categoryName);
            return;
        }
        
        if (availableItems.IsEmpty()) {
            Print("[ConvoyEvent] No available items for category: " + categoryName);
            return;
        }
        
        // Calculate random count
        int count = Math.RandomInt(minCount, maxCount + 1);
        
                 // Spawn items
         for (int i = 0; i < count; i++) {
             // Pick random item from available
             SCR_EntityCatalogEntry randomItem = availableItems[Math.RandomInt(0, availableItems.Count())];
             ResourceName prefab = randomItem.GetPrefab();
             
             invMgr.TrySpawnPrefabToStorage(prefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, 1);
         }
    }
    
    bool GetOccupyingFactionItems(OVT_EconomyManagerComponent economy, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode, out array<SCR_EntityCatalogEntry> items)
    {
        // Get all items of the specified type
        array<SCR_EntityCatalogEntry> allItems = new array<SCR_EntityCatalogEntry>();
        if (!economy.FindInventoryItems(itemType, itemMode, "", allItems)) {
            return false;
        }
        
        // Filter to only occupying faction items
        int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
        
        foreach (SCR_EntityCatalogEntry entry : allItems) {
            ResourceName prefab = entry.GetPrefab();
            int itemId = economy.GetInventoryId(prefab);
            
            // Check if item belongs to occupying faction
            if (economy.ItemIsFromFaction(itemId, occupyingFactionIndex)) {
                items.Insert(entry);
            }
        }
        
        return true;
    }

    void NotifyConvoyEventStart()
    {
        // TODO: Get nearest town name to start position and nearest base name to destination
        string startLocationName = GetNearestLocationName(m_vStartPos, true); // true for town
        string destLocationName = GetNearestLocationName(m_vDestinationPos, false); // false for base
        
        string message = string.Format("Convoy departing from %1 to %2", startLocationName, destLocationName);
        SCR_HintManagerComponent.GetInstance().ShowCustom("Convoy Movement", message);
        
        // Place initial marker if nearest radio tower belongs to resistance
        CheckAndPlaceConvoyMarker();
    }

    string GetNearestLocationName(vector pos, bool findTown)
    {
        if (findTown) {
            // Get nearest town name
            OVT_TownManagerComponent townMgr = OVT_Global.GetTowns();
            if (townMgr) {
                return townMgr.GetNearestTownName(pos);
            }
            return "Unknown Town";
        } else {
            // Get nearest base name
            OVT_OccupyingFactionManager occupyingMgr = OVT_Global.GetOccupyingFaction();
            if (occupyingMgr) {
                OVT_BaseData nearestBase = occupyingMgr.GetNearestBase(pos);
                if (nearestBase) {
                    // Get base controller to get name
                    OVT_BaseControllerComponent baseController = occupyingMgr.GetBase(nearestBase.entId);
                    if (baseController) {
                        return baseController.m_sName;
                    }
                }
            }
            return "Unknown Base";
        }
    }

    void CheckAndPlaceConvoyMarker()
    {
        if (!m_bEventActive) return;
        
        IEntity truck = GetGame().GetWorld().FindEntityByID(m_ConvoyVehicleID);
        if (!truck) return;
        
        vector convoyPos = truck.GetOrigin();
        
        // Check if nearest radio tower belongs to resistance
        OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
        OVT_RadioTowerData tower = of.GetNearestRadioTower(convoyPos);
        
                 if (tower) {
             bool isResistanceTower = !tower.IsOccupyingFaction();
             
             if (isResistanceTower) {
                 PlaceConvoyMarker(convoyPos);
             } else {
                 RemoveConvoyMarker();
             }
         } else {
             RemoveConvoyMarker();
         }
    }
    
    // Convoy marker management
    
    void PlaceConvoyMarker(vector pos)
    {
        // Update static convoy position and visibility
        s_vActiveConvoyPos = pos;
        s_bConvoyMarkerVisible = true;
    }
    
    void RemoveConvoyMarker()
    {
        if (!s_bConvoyMarkerVisible) return;
        
        // Hide convoy marker
        s_bConvoyMarkerVisible = false;
        s_vActiveConvoyPos = vector.Zero;
    }
    
    // Static methods for external access (e.g., by map systems)
    static bool IsConvoyMarkerVisible()
    {
        return s_bConvoyMarkerVisible;
    }
    
    static vector GetActiveConvoyPosition()
    {
        return s_vActiveConvoyPos;
    }
    
    // Static array to track convoy vehicle IDs for protection
    static ref array<EntityID> s_ConvoyVehicleIDs = new array<EntityID>();
    
    // Protect convoy vehicles from cleanup systems
    void ProtectVehicleFromCleanup(IEntity vehicle)
    {
        if (!vehicle) return;
        
        // Add to our convoy tracking list
        EntityID vehicleID = vehicle.GetID();
        if (!s_ConvoyVehicleIDs.Contains(vehicleID))
        {
            s_ConvoyVehicleIDs.Insert(vehicleID);
        }
        
        // Try to use player owner component if available
        OVT_PlayerOwnerComponent playerOwnerComp = OVT_PlayerOwnerComponent.Cast(vehicle.FindComponent(OVT_PlayerOwnerComponent));
        if (playerOwnerComp)
        {
            // Set a special UID to mark this as a convoy vehicle
            playerOwnerComp.SetPlayerOwner("CONVOY_SYSTEM");
        }
        
        Print("[ConvoyEvent] Protected convoy vehicle from cleanup: " + vehicle.ToString());
    }
    
    // Remove convoy protection from vehicle
    void RemoveConvoyProtection(IEntity vehicle)
    {
        if (!vehicle) return;
        
        // Remove from convoy tracking list
        EntityID vehicleID = vehicle.GetID();
        if (s_ConvoyVehicleIDs.Contains(vehicleID))
        {
            s_ConvoyVehicleIDs.RemoveItem(vehicleID);
        }
        
        // Clear convoy owner UID
        OVT_PlayerOwnerComponent playerOwnerComp = OVT_PlayerOwnerComponent.Cast(vehicle.FindComponent(OVT_PlayerOwnerComponent));
        if (playerOwnerComp)
        {
            string ownerUID = playerOwnerComp.GetPlayerOwnerUid();
            if (ownerUID == "CONVOY_SYSTEM")
            {
                playerOwnerComp.ClearPlayerOwner(); // Clear convoy protection
            }
        }
        
        Print("[ConvoyEvent] Removed convoy protection from vehicle: " + vehicle.ToString());
    }
    
    // Check if a vehicle is a convoy vehicle (static method for external use)
    static bool IsConvoyVehicle(IEntity vehicle)
    {
        if (!vehicle) return false;
        
        // Check if it's in our convoy tracking list
        EntityID vehicleID = vehicle.GetID();
        if (s_ConvoyVehicleIDs.Contains(vehicleID))
            return true;
        
        // Check if it has convoy owner UID
        OVT_PlayerOwnerComponent playerOwnerComp = OVT_PlayerOwnerComponent.Cast(vehicle.FindComponent(OVT_PlayerOwnerComponent));
        if (playerOwnerComp)
        {
            string ownerUID = playerOwnerComp.GetPlayerOwnerUid();
            if (ownerUID == "CONVOY_SYSTEM")
                return true;
        }
        
        return false;
    }
     
           void GrantOccupationResources(int amount)
      {
          // This would implement granting resources to the occupying faction
          // For now, this is a placeholder as the exact mechanism depends on 
          // how faction resources are tracked in the game
          OVT_OccupyingFactionManager occupyingMgr = OVT_Global.GetOccupyingFaction();
          if (occupyingMgr) {
              // TODO: Implement actual resource granting to occupying faction
              // This might involve increasing base resources, faction strength, etc.
              Print("[ConvoyEvent] Granted " + amount + " resources to occupying faction");
          }
      }
      
      void SetVehicleOrientation(IEntity vehicle, vector fromPos, vector toPos)
      {
          if (!vehicle) return;
          
          // Calculate direction vector from start to destination
          vector direction = toPos - fromPos;
          
          // Calculate yaw angle in degrees (Arma coordinate system: X = East, Z = South)
          // North = -Z, so we use atan2(x, -z) to get bearing angle
          float yawAngle = Math.Atan2(direction[0], -direction[2]) * Math.RAD2DEG;
          
          // Create angles vector (yaw, pitch, roll)
          vector angles = Vector(yawAngle, 0, 0);
          
                   // Apply orientation to vehicle
         vehicle.SetAngles(angles);
      }

    void MonitorEvent()
    {
        if (!m_bEventActive)
            return;
        
        // Wait for crew to be properly initialized (at least 1 crew member registered)
        if (m_ConvoyCrewIDs.IsEmpty()) {
            Print("[ConvoyEvent] MonitorEvent: Waiting for crew initialization...");
            return;
        }
        
        // Check if convoy reached destination
        if (IsConvoyAtDestination()) {
            // Success - grant resources to occupation
            GrantOccupationResources(CONVOY_SUCCESS_REWARD);
            NotifyConvoyEventSuccess();
            EndEvent();
            return;
        }
        
        // Check if convoy crew is dead
        if (IsConvoyCrewDead()) {
            Print("[ConvoyEvent] MonitorEvent: Crew is dead, ending event");
            NotifyConvoyEventFailure();
            EndEvent(true); // Keep truck when crew is dead
            return;
        }
        
        // Update marker every 5 minutes
        float currentTime = GetGame().GetWorld().GetWorldTime();
        if (currentTime - m_fLastMarkerUpdate >= MARKER_UPDATE_INTERVAL) {
            CheckAndPlaceConvoyMarker();
            m_fLastMarkerUpdate = currentTime;
        } else {
            // Update marker position more frequently (every monitoring cycle) if it's visible
            if (s_bConvoyMarkerVisible) {
                IEntity truck = GetGame().GetWorld().FindEntityByID(m_ConvoyVehicleID);
                if (truck) {
                    s_vActiveConvoyPos = truck.GetOrigin();
                }
            }
        }
    }

    bool IsConvoyAtDestination()
    {
        IEntity truck = GetGame().GetWorld().FindEntityByID(m_ConvoyVehicleID);
        if (!truck) return false;
        
        float distance = vector.Distance(truck.GetOrigin(), m_vDestinationPos);
        return distance < 50.0; // 50 meter tolerance
    }

    bool IsConvoyCrewDead()
    {
        if (m_ConvoyCrewIDs.IsEmpty()) {
            Print("[ConvoyEvent] IsConvoyCrewDead: No crew IDs registered yet, crew not dead");
            return false; // No crew registered yet - don't consider them dead
        }
        
        int aliveCount = 0;
        int deadCount = 0;
        int missingCount = 0;
        
        foreach (EntityID id : m_ConvoyCrewIDs) {
            IEntity crew = GetGame().GetWorld().FindEntityByID(id);
            if (crew) {
                DamageManagerComponent dmg = DamageManagerComponent.Cast(crew.FindComponent(DamageManagerComponent));
                if (dmg && !dmg.IsDestroyed()) {
                    aliveCount++;
                } else {
                    deadCount++;
                }
            } else {
                missingCount++;
            }
        }
        
        Print("[ConvoyEvent] Crew status: " + aliveCount + " alive, " + deadCount + " dead, " + missingCount + " missing");
        
        // Only consider crew dead if all are confirmed dead (not missing/despawned)
        return (aliveCount == 0) && (deadCount > 0 || missingCount == m_ConvoyCrewIDs.Count());
    }

    void NotifyConvoyEventSuccess()
    {
        string message = "Convoy successfully reached destination";
        SCR_HintManagerComponent.GetInstance().ShowCustom("Convoy Success", message);
    }

    void NotifyConvoyEventFailure()
    {
        string message = "Convoy crew eliminated - mission failed";
        SCR_HintManagerComponent.GetInstance().ShowCustom("Convoy Intercepted", message);
    }

    void EndEvent(bool keepTruck = false)
    {
        Print("[ConvoyEvent] Ending convoy event | keepTruck=" + keepTruck);
        
        // Remove convoy marker
        RemoveConvoyMarker();
        
        // Clean up spawned entities
        foreach (EntityID id : m_SpawnedEntities) {
            // Skip truck deletion if keepTruck is true
            if (keepTruck && id == m_ConvoyVehicleID) {
                Print("[ConvoyEvent] Keeping truck as trophy");
                // Remove convoy protection from trophy truck so it can be managed normally
                IEntity truck = GetGame().GetWorld().FindEntityByID(id);
                if (truck) {
                    RemoveConvoyProtection(truck);
                }
                continue;
            }
            
            IEntity ent = GetGame().GetWorld().FindEntityByID(id);
            if (ent)
                SCR_EntityHelper.DeleteEntityAndChildren(ent);
        }
        
        m_SpawnedEntities.Clear();
        m_ConvoyCrewIDs.Clear();
        m_bEventActive = false;
        m_ConvoyVehicleID = EntityID.INVALID;
        
        // Clear static convoy marker variables
        s_bConvoyMarkerVisible = false;
        s_vActiveConvoyPos = vector.Zero;
        
        // Clear convoy vehicle tracking for cleanup (except kept trucks)
        if (!keepTruck)
        {
            s_ConvoyVehicleIDs.Clear();
        }
        else if (m_ConvoyVehicleID != EntityID.INVALID)
        {
            // Remove all convoy vehicles except the kept truck
            for (int i = s_ConvoyVehicleIDs.Count() - 1; i >= 0; i--)
            {
                if (s_ConvoyVehicleIDs[i] != m_ConvoyVehicleID)
                {
                    s_ConvoyVehicleIDs.Remove(i);
                }
            }
        }
        
        GetGame().GetCallqueue().Remove(MonitorEvent);
        Print("[ConvoyEvent] EndEvent: scheduling next event in " + m_iIntervalHours + " hours");
        GetGame().GetCallqueue().CallLater(TryStartEvent, m_iIntervalHours * 3600 * 1000, false);
    }
} 