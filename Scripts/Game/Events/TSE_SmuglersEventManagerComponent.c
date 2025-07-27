const string TSE_SMUGLERS_SPAWN_POINT_PREFAB = "{B07A6AEDC9EE4B85}Prefabs/GameMode/TSE_SmuglersEventSpawn.et";



[ComponentEditorProps(category: "GameScripted/Events", description: "Smuglers Event Manager")]
class TSE_SmuglersEventManagerComponentClass : ScriptComponentClass {}

class TSE_SmuglersEventManagerComponent : ScriptComponent
{
    ref array<IEntity> m_SpawnMarkers = new array<IEntity>(); // Зарегистрированные маркеры

    [Attribute(defvalue: "24", uiwidget: UIWidgets.EditBox, desc: "Interval between events (hours)")]
    int m_iIntervalHours;

    [Attribute(defvalue: "12", uiwidget: UIWidgets.EditBox, desc: "Event duration (hours)")]
    int m_iDurationHours;

    [Attribute("", UIWidgets.Object, desc: "Crate content config")]
    ref TSE_SmuglerCrateContents m_CrateConfig;

    static const string CRATE_PREFAB = "{326E60F7A7358EDA}Prefabs/Props/Crates/Smuglers_Crate.et";
    static const string GUARD_GROUP_PREFAB = "{65F295CBD0257A73}Prefabs/Groups/OPFOR/Smuglers/Group_Smuglers_LightFireTeam.et";
    static const string VEHICLE_PREFAB = "{442939C9617DF228}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2_FIA.et";
    static const string CREW_GROUP_PREFAB = "{65F295CB4DD807B9}Prefabs/Groups/OPFOR/Smuglers/Group_Smuglers_Crew.et";

    float m_fEventStartHour;
    bool m_bEventActive;
    ref array<EntityID> m_SpawnedEntities;
    ref array<EntityID> m_GuardUnitIDs;
    ref map<EntityID, EntityID> m_mPendingCrewAssignments = new map<EntityID, EntityID>();
    ref array<IEntity> m_FoundVehicles = new array<IEntity>();
    TSE_MapSmuglersEventArea m_SmuglersEventAreaLayer;
    bool m_CrateInteracted = false; // Set to true when crate is interacted
    
    // Configuration override for Event Manager
    ref TSE_SmuglersEventConfig m_ManagedConfig;
    bool m_bManagedByEventManager = false;

    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        m_SpawnedEntities = new array<EntityID>();
        m_GuardUnitIDs = new array<EntityID>();
        Print("[SmuglersEvent] OnPostInit: " + owner.GetName() + " | m_bEventActive=" + m_bEventActive);
        
        // Note: Event Manager will call StartEventFromManager instead of self-starting
        // Legacy behavior kept for backward compatibility
        if (!m_bManagedByEventManager)
        {
            // Запускаем проверку готовности гейммода
            GetGame().GetCallqueue().CallLater(WaitForGameModeInitialized, 1000, false);
        }
    }

    void WaitForGameModeInitialized()
    {
        OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if (!mode || !mode.IsInitialized())
        {
            // Ещё не готово — повторить через 1 сек
            GetGame().GetCallqueue().CallLater(WaitForGameModeInitialized, 1000, false);
            return;
        }
        Print("[SmuglersEvent] OverthrowGameMode initialized, starting event logic");
        // точки спавна должны быть зарегистрированы компонентами на префабах
        if(m_SpawnMarkers.IsEmpty())
        {
            Print("[SmuglersEvent] No spawn markers registered!");
        }
        CleanAllSpawnMarkersFromVehicles();
        GetGame().GetCallqueue().CallLater(TryStartEvent, 60000, false);
    }

    void CleanAllSpawnMarkersFromVehicles()
    {
        if (!m_SpawnMarkers) m_SpawnMarkers = new array<IEntity>();
        BaseWorld world = GetGame().GetWorld();
        if (!world) return;
        // m_SpawnMarkers уже заполнен через регистрацию компонентов
        if(m_SpawnMarkers.IsEmpty()) return;
        foreach (IEntity marker : m_SpawnMarkers)
        {
            CleanVehiclesNearMarker(marker.GetOrigin());
        }
    }

    bool CollectVehicleCallback(IEntity ent)
    {
        m_FoundVehicles.Insert(ent);
        return true;
    }

    void CleanVehiclesNearMarker(vector pos)
    {
        m_FoundVehicles.Clear();
        GetGame().GetWorld().QueryEntitiesBySphere(pos, 100, CollectVehicleCallback, VehicleFilterWheeled, EQueryEntitiesFlags.ALL);
        foreach (IEntity veh : m_FoundVehicles)
        {
            Print("[SmuglersEvent] Deleting vehicle at spawn: " + veh.ToString());
            SCR_EntityHelper.DeleteEntityAndChildren(veh);
        }
    }

    void TryStartEvent()
    {
        Print("[SmuglersEvent] TryStartEvent | m_bEventActive=" + m_bEventActive);
        if (m_bEventActive)
            return;
        StartEvent();
    }
    
    // New method called by Event Manager
    void StartEventFromManager(TSE_SmuglersEventConfig config)
    {
        if (m_bEventActive) {
            Print("[SmuglersEvent] StartEventFromManager: already active, skipping");
            return;
        }
        
        Print("[SmuglersEvent] Starting event from Event Manager");
        m_bManagedByEventManager = true;
        m_ManagedConfig = config;
        
        // Override interval and duration from config
        if (config) {
            m_iIntervalHours = config.m_iMaxIntervalHours;
            m_iDurationHours = config.m_iDurationHours;
        }
            
        StartEvent();
    }
    
    // Check if event is active (called by Event Manager)
    bool IsEventActive()
    {
        return m_bEventActive;
    }

    // Регистрация маркера из компонента
    void RegisterSpawnMarker(IEntity marker)
    {
        if(!marker) return;
        if(m_SpawnMarkers.Contains(marker)) return;
        m_SpawnMarkers.Insert(marker);
    }

    // Добавь в localization_Overthrow.en-us.conf:
    // STR_TSE_SMUGLERS_EVENT_START "Intelligence reports a smuggler shipment is being prepared for export - intercept it."
    // Добавь в конфиг NotificationManager:
    // [NotificationPreset STR_TSE_SMUGLERS_EVENT_START]
    // UIInfo = "{...}"

    void NotifySmuglersEventStart()
    {
        OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
        if (notify)
        {
            notify.SendTextNotification("STR_TSE_SMUGLERS_EVENT_START", -1);
        }
    }

    void NotifySmuglersEventSuccess()
    {
        OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
        if (notify)
        {
            notify.SendTextNotification("STR_TSE_SMUGLERS_EVENT_SUCCESS", -1);
        }
    }

    void NotifySmuglersEventTimeout()
    {
        OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
        if (notify)
        {
            notify.SendTextNotification("STR_TSE_SMUGLERS_EVENT_TIMEOUT", -1);
        }
    }

    void PlaceSmuglersEventAreaMarker(vector pos)
    {
        SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapMarkerManagerComponent));
        if (!markerMgr) return;

        SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
        marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM); // Custom marker
        marker.SetWorldPos(pos[0], pos[2]);
        marker.SetCustomText("#STR_TSE_SMUGLERS_EVENT_START");
        marker.SetColorEntry(5); // Индекс цвета из палитры (коричневый или кастомный)
        marker.SetIconEntry(3); // Индекс иконки
        markerMgr.InsertStaticMarker(marker, false);
    }

    void StartEvent()
    {
        if (m_bEventActive) {
            Print("[SmuglersEvent] StartEvent: already active, skipping");
            return;
        }
        m_bEventActive = true;
        Print("[SmuglersEvent] Starting event");
        vector markerPos = GetRandomSpawnMarkerPosition();
        // Чистим технику вокруг выбранного маркера
        CleanVehiclesNearMarker(markerPos);
        // Смещения относительно маркера
        vector crateOffset = "0 0 0";
        vector guardOffset = "10 0 10";
        vector vehicleOffset = "-10 0 -10";
        vector crewOffset = "-12 0 -10";
        // Спавн ящика
        IEntity crate = OVT_Global.SpawnEntityPrefab(CRATE_PREFAB, markerPos + crateOffset);
        Print("[SmuglersEvent] Crate entity: " + crate.ToString());
        if (crate)
        {
            m_SpawnedEntities.Insert(crate.GetID());
            Print("[SmuglersEvent] Crate spawned: " + crate.ToString());
            FillCrateWithRandomContent(crate);
            Print("[SmuglersEvent] FillCrateWithRandomContent finished");
        }
        // Спавн охраны
        IEntity guardGroup = OVT_Global.SpawnEntityPrefab(GUARD_GROUP_PREFAB, markerPos + guardOffset);
        Print("[SmuglersEvent] Guard group entity: " + guardGroup.ToString());
        if (guardGroup)
        {
            m_SpawnedEntities.Insert(guardGroup.GetID());
            m_GuardUnitIDs.Clear();
            IEntity child = guardGroup.GetChildren();
            while (child)
            {
                if (CharacterEntity.Cast(child))
                    m_GuardUnitIDs.Insert(child.GetID());
                child = child.GetSibling();
            }
            Print("[SmuglersEvent] Guard group spawned: " + guardGroup.ToString() + ", units: " + m_GuardUnitIDs.Count());
        }
        // Спавн машины
        IEntity vehicle = OVT_Global.SpawnEntityPrefab(VEHICLE_PREFAB, markerPos + vehicleOffset);
        Print("[SmuglersEvent] Vehicle entity: " + vehicle.ToString());
        if (vehicle) {
            m_SpawnedEntities.Insert(vehicle.GetID());
            Print("[SmuglersEvent] Vehicle spawned: " + vehicle.ToString());
        }
        // Спавн экипажа
        IEntity crewGroup = OVT_Global.SpawnEntityPrefab(CREW_GROUP_PREFAB, markerPos + crewOffset);
        Print("[SmuglersEvent] Crew group entity: " + crewGroup.ToString());
        if (crewGroup && vehicle) {
            m_SpawnedEntities.Insert(crewGroup.GetID());
            Print("[SmuglersEvent] Crew group spawned: " + crewGroup.ToString());
            SCR_AIGroup aiGroup = SCR_AIGroup.Cast(crewGroup);
            if (aiGroup) {
                m_mPendingCrewAssignments.Insert(aiGroup.GetID(), vehicle.GetID());
                aiGroup.GetOnInit().Insert(OnCrewGroupInitialized);
                Print("[SmuglersEvent] Subscribed to OnInit for crew group " + aiGroup.GetID());
            }
        }
        // --- Уведомление и маркер ---
        //GetGame().GetCallqueue().CallLater(NotifySmuglersEventStart, 3000, false);
        // Проверка принадлежности ближайшей радиовышки
        string msg = string.Format("Intelligence report!");
		SCR_HintManagerComponent.GetInstance().ShowCustom("#STR_TSE_SMUGLERS_EVENT_START", msg);
        OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
        OVT_RadioTowerData tower = of.GetNearestRadioTower(markerPos);
        if (tower && !tower.IsOccupyingFaction())
        {
            PlaceSmuglersEventAreaMarker(markerPos);
        }
        // Save start hour (only hours)
        ChimeraWorld world = GetGame().GetWorld();
        TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
        TimeContainer time = t.GetTime();
        m_fEventStartHour = time.m_iHours;
        Print("[SmuglersEvent] Event started at hour: " + m_fEventStartHour);
        GetGame().GetCallqueue().CallLater(MonitorEvent, 900000, true); // 15 минут
    }

    void OnCrewGroupInitialized(SCR_AIGroup group)
    {
        if (!group) return;
        EntityID groupID = group.GetID();
        if (!m_mPendingCrewAssignments.Contains(groupID)) {
            Print("[SmuglersEvent] OnCrewGroupInitialized: no pending assignment for group " + groupID);
            return;
        }
        EntityID vehicleID = m_mPendingCrewAssignments.Get(groupID);
        IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehicleID);
        if (!vehicle) {
            Print("[SmuglersEvent] OnCrewGroupInitialized: vehicle not found for group " + groupID);
            m_mPendingCrewAssignments.Remove(groupID);
            return;
        }
        SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
        if (!compMgr) {
            Print("[SmuglersEvent] OnCrewGroupInitialized: vehicle has no compartment manager");
            m_mPendingCrewAssignments.Remove(groupID);
            return;
        }
        array<AIAgent> agents = {};
        group.GetAgents(agents);
        Print("[SmuglersEvent] OnCrewGroupInitialized: agents count: " + agents.Count());
        int agentIndex = 0;
        // Driver
        if (agentIndex < agents.Count())
        {
            Print("[SmuglersEvent] Boarding driver...");
            MoveAgentToCompartment(compMgr, agents[agentIndex], ECompartmentType.PILOT);
            agentIndex++;
        }
        // Gunner(s)
        while (agentIndex < agents.Count())
        {
            Print("[SmuglersEvent] Boarding gunner...");
            if (!MoveAgentToCompartment(compMgr, agents[agentIndex], ECompartmentType.TURRET)) break;
            agentIndex++;
        }
        // Остальные - пассажиры
        while (agentIndex < agents.Count())
        {
            Print("[SmuglersEvent] Boarding cargo...");
            if (!MoveAgentToCompartment(compMgr, agents[agentIndex], ECompartmentType.CARGO)) break;
            agentIndex++;
        }
        m_mPendingCrewAssignments.Remove(groupID);
        Print("[SmuglersEvent] Crew assignment complete for group " + groupID);
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

    bool VehicleFilterWheeled(IEntity ent)
    {
        if (!ent) return false;
        EntityPrefabData prefabData = ent.GetPrefabData();
        if (!prefabData) return false;
        string prefabName = prefabData.GetPrefabName();
        // Проверяем, что это нужный каталог
        return prefabName.StartsWith("{22B327C6752EC4D4}Prefabs/Vehicles/Wheeled/");
    }


    void MonitorEvent()
    {
        // Лог убран
        if (!m_bEventActive)
            return;
        ChimeraWorld world = GetGame().GetWorld();
        TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
        TimeContainer time = t.GetTime();
        float currentHour = time.m_iHours;
        float hoursPassed = currentHour - m_fEventStartHour;
        if (hoursPassed < 0) hoursPassed += 24; // handle midnight wrap
        if (hoursPassed >= m_iDurationHours)
        {
            if (IsGuardsAlive() && IsCrateUntouched())
            {
                Print("[SmuglersEvent] Time expired, cleaning up event");
                EndEvent();
                return;
            }
        }
        // Проверяем условия успеха:
        if (IsGuardsDead() && IsCrateInteracted())
        {
            string msg = string.Format("Intelligence report!");
            SCR_HintManagerComponent.GetInstance().ShowCustom("#STR_TSE_SMUGLERS_EVENT_SUCCESS", msg);
            EndEvent();
            return;
        }
        // Проверяем тайм-аут:
        if (IsEventTimeout())
        {
            string msg = string.Format("Intelligence report!");
            SCR_HintManagerComponent.GetInstance().ShowCustom("#STR_TSE_SMUGLERS_EVENT_TIMEOUT", msg);
            EndEvent();
            return;
        }
        GetGame().GetCallqueue().CallLater(MonitorEvent, 900000, true); // 15 минут
    }

    void EndEvent()
    {
        Print("[SmuglersEvent] Ending event");
        foreach (EntityID id : m_SpawnedEntities)
        {
            IEntity ent = GetGame().GetWorld().FindEntityByID(id);
            if (ent)
                SCR_EntityHelper.DeleteEntityAndChildren(ent);
        }
        m_SpawnedEntities.Clear();
        m_GuardUnitIDs.Clear();
        m_bEventActive = false;
        GetGame().GetCallqueue().Remove(MonitorEvent);
        
        // Only schedule next event if not managed by Event Manager
        if (!m_bManagedByEventManager)
        {
            Print("[SmuglersEvent] EndEvent: scheduling next event in " + m_iIntervalHours + " hours");
            GetGame().GetCallqueue().CallLater(TryStartEvent, m_iIntervalHours * 3600 * 1000, false);
        }
        else
        {
            Print("[SmuglersEvent] EndEvent: managed by Event Manager, not scheduling next event");
            m_bManagedByEventManager = false; // Reset for potential future legacy use
            m_ManagedConfig = null;
        }
    }

    bool IsGuardsAlive()
    {
        if (!m_GuardUnitIDs)
            return false;
        foreach (EntityID id : m_GuardUnitIDs)
        {
            IEntity unit = GetGame().GetWorld().FindEntityByID(id);
            if (!unit)
                continue;
            SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(unit.FindComponent(SCR_CharacterControllerComponent));
            if (charCtrl && !charCtrl.IsDead())
                return true; // At least one alive
        }
        return false; // All dead
    }

    // Returns true if all guard units are dead
    bool IsGuardsDead()
    {
        foreach (EntityID id : m_GuardUnitIDs)
        {
            IEntity ent = GetGame().GetWorld().FindEntityByID(id);
            if (ent)
            {
                DamageManagerComponent dmg = DamageManagerComponent.Cast(ent.FindComponent(DamageManagerComponent));
                if (dmg && !dmg.IsDestroyed())
                    return false;
            }
        }
        return true;
    }

    // Returns true if the crate was interacted with (implement your logic to set m_CrateInteracted)
    bool IsCrateInteracted()
    {
        return m_CrateInteracted;
    }

    // Returns true if event duration exceeded
    bool IsEventTimeout()
    {
        ChimeraWorld world = GetGame().GetWorld();
        TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
        TimeContainer time = t.GetTime();
        int currentHour = time.m_iHours;
        int elapsed = currentHour - m_fEventStartHour;
        if (elapsed < 0) elapsed += 24; // handle day wrap
        return (elapsed >= m_iDurationHours);
    }

    bool IsCrateUntouched()
    {
        // TODO: реализуй свою проверку, если нужно
        return true;
    }

    void FillCrateWithRandomContent(IEntity crate)
    {
        if (!crate || !m_CrateConfig) return;
        InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(crate.FindComponent(InventoryStorageManagerComponent));
        if (!invMgr) {
            Print("SmuglersEvent: Crate has no InventoryStorageManagerComponent");
            return;
        }
        array<ref TSE_PrefabItemChanceConfig> itemConfigs = m_CrateConfig.m_aSmuglersCrateItemPrefabs;
        if (!itemConfigs || itemConfigs.Count() == 0) {
            Print("SmuglersEvent: m_CrateConfig.m_aSmuglersCrateItemPrefabs is null or empty");
            return;
        }
        foreach (TSE_PrefabItemChanceConfig item : itemConfigs)
        {
            if (!item) continue;
            if (Math.RandomInt(0, 100) >= item.chance) continue;
            int count = Math.RandomInt(item.minStock, item.maxStock + 1);
            bool ok = invMgr.TrySpawnPrefabToStorage(item.m_sEntityPrefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, count);
            Print("[SmuglersEvent] TrySpawnPrefabToStorage: " + item.m_sEntityPrefab + " x" + count + " result=" + ok);
        }
    }

    vector GetRandomSpawnMarkerPosition()
    {
        // список уже собран через регистрацию, ничего не делаем
        if (m_SpawnMarkers.Count() == 0)
        {
            Print("SmuglersEvent: No spawn markers found, using origin");
            return Vector(0,0,0);
        }
        int idx = Math.RandomInt(0, m_SpawnMarkers.Count());
        return m_SpawnMarkers[idx].GetOrigin();
    }
} 
