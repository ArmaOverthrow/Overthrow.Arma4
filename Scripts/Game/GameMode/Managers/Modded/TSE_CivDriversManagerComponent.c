const string TSE_CIV_DRIVER_SPAWN_POINT_PREFAB = "{51545B43C45523B6}Prefabs/GameMode/TSE_CivDriversSpawn.et";
// Cache of prefabs loaded from config
ref array<ResourceName> g_aCivVehiclePrefabs = {};

// Delay before spawning civilian drivers after gamemode init (milliseconds)
const int TSE_CIV_DRIVER_SPAWN_DELAY_MS = 240000; // 4 minutes

[ComponentEditorProps(category: "GameScripted/CivDrivers", description: "Civilian Drivers Manager")]
class TSE_CivDriversManagerComponentClass : ScriptComponentClass {}

class TSE_CivDriversManagerComponent : ScriptComponent
{
    // Сохранённые точки спавна
    ref array<IEntity> m_SpawnMarkers = new array<IEntity>();
    // Сущности, созданные менеджером (машины + группы)
    ref array<EntityID> m_SpawnedEntities = new array<EntityID>();
    // Ожидаемая посадка экипажа: key = groupID, value = vehicleID
    ref map<EntityID, EntityID> m_mPendingCrewAssignments = new map<EntityID, EntityID>();
    // Зарегистрированные маркеры (через компонент на префабе)
    ref array<IEntity> m_RegisteredMarkers = new array<IEntity>();
    ref map<EntityID, IEntity> m_mVehicleDestinations = new map<EntityID, IEntity>();
	// Direct config loading
	[Attribute("", UIWidgets.Object, desc: "Civilian vehicle config")]
	ref TSE_CarArrayConfig m_VehicleConfig;
	const string TSE_CIV_GROUP_PREFAB = "{1AF5B9AE5CFD4434}Prefabs/Groups/INDFOR/Group_CIV.et";
	
    //---------------------------------------------------------
    // Lifecycle
    //---------------------------------------------------------
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);

        // Load vehicle prefabs once
        if (g_aCivVehiclePrefabs.IsEmpty())
        {
            g_aCivVehiclePrefabs = new array<ResourceName>();
            
            // Load from direct config
            if (m_VehicleConfig && m_VehicleConfig.m_aEntityPrefab)
            {
                foreach (TSE_CarArrayAndChance car : m_VehicleConfig.m_aEntityPrefab)
                {
                    if (car && car.m_sEntityPrefab != "")
                    {
                        g_aCivVehiclePrefabs.Insert(car.m_sEntityPrefab);
                    }
                }
            }
            
            Print(string.Format("[CivDrivers] Loaded %1 civilian car prefabs", g_aCivVehiclePrefabs.Count()));
        }

        // Ждём инициализации гейм-мода, после чего стартуем
        GetGame().GetCallqueue().CallLater(WaitForGameModeInitialized, 1000, false);
    }



    void WaitForGameModeInitialized()
    {
        OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if (!mode || !mode.IsInitialized())
        {
            // Ещё не готов — повторяем позже
            GetGame().GetCallqueue().CallLater(WaitForGameModeInitialized, 1000, false);
            return;
        }

        // Делаем отложенный старт, чтобы сервер успел полностью загрузиться
        GetGame().GetCallqueue().CallLater(SpawnDriversDelayed, TSE_CIV_DRIVER_SPAWN_DELAY_MS, false);
    }

    // Отложенный запуск спавна и монитора прибытия
    void SpawnDriversDelayed()
    {
        SpawnAllDrivers();
        StartArrivalMonitor();
        StartPeriodicSpawn();
    }
    
    // Запуск периодического спавна для поддержания дорожного движения
    void StartPeriodicSpawn()
    {
        if (!m_VehicleConfig) return;
        
        // Получаем интервал из конфига (в часах) и конвертируем в миллисекунды
        float intervalHours = m_VehicleConfig.m_fPeriodicSpawnIntervalHours;
        if (intervalHours <= 0) intervalHours = 1.0; // Default fallback
        
        int intervalMs = Math.Round(intervalHours * 3600000); // часы -> миллисекунды
        Print(string.Format("[CivDrivers] Setting periodic spawn interval: %1 hours (%2 ms)", intervalHours, intervalMs));
        
        GetGame().GetCallqueue().CallLater(PeriodicSpawn, intervalMs, true);
    }
    
    // Периодический спавн одного водителя на случайной точке
    void PeriodicSpawn()
    {
        if (m_SpawnMarkers.IsEmpty()) return;
        
        // Выбираем случайную точку спавна
        int randomIndex = Math.RandomInt(0, m_SpawnMarkers.Count());
        IEntity randomMarker = m_SpawnMarkers[randomIndex];
        if (randomMarker)
        {
            SpawnDriverAt(randomMarker.GetOrigin());
            Print("[CivDrivers] Periodic spawn: spawned driver for traffic simulation");
        }
    }

    //---------------------------------------------------------
    // Spawning logic
    //---------------------------------------------------------
    void SpawnAllDrivers()
    {
        // Собираем все маркеры точек спавна
        CollectSpawnMarkers();

        Print(string.Format("[CivDrivers] Found %1 spawn markers", m_SpawnMarkers.Count()));

        foreach (IEntity marker : m_SpawnMarkers)
        {
            if (!marker) continue;
            SpawnDriverAt(marker.GetOrigin());
        }
    }

    void CollectSpawnMarkers()
    {
        m_SpawnMarkers.Clear();
        // Переносим только те маркеры, которые были зарегистрированы компонентами
        foreach (IEntity ent : m_RegisteredMarkers)
        {
            if (ent) m_SpawnMarkers.Insert(ent);
        }
    }

    bool CivDrivers_FindSpawnMarkerFilter(IEntity ent)
    {
        if (!ent) return false;
        EntityPrefabData prefabData = ent.GetPrefabData();
        if (!prefabData) return false;
        if (prefabData.GetPrefabName() == TSE_CIV_DRIVER_SPAWN_POINT_PREFAB)
        {
            m_SpawnMarkers.Insert(ent);
            return true; // entity подходит, продолжаем поиск остальных
        }
        return false;
    }

    void SpawnDriverAt(vector pos)
    {
        //--------------------------------------------------
        // Выбираем случайный префаб машины из списка
        //--------------------------------------------------
        ResourceName selectedPrefab = "";
        if (g_aCivVehiclePrefabs && g_aCivVehiclePrefabs.Count() > 0)
        {
            int idx = s_AIRandomGenerator.RandInt(0, g_aCivVehiclePrefabs.Count());
            selectedPrefab = g_aCivVehiclePrefabs[idx];
        }
        else
        {
            Print("[CivDrivers] Vehicle list config empty – fallback to default prefab");
            selectedPrefab = "{128253A267BE9424}Prefabs/Vehicles/Wheeled/S105/S105_randomized.et";
        }

        IEntity vehicle = OVT_Global.SpawnEntityPrefab(selectedPrefab, pos);
        if (!vehicle)
        {
            Print("[CivDrivers] Failed to spawn vehicle at " + pos);
            return;
        }
        m_SpawnedEntities.Insert(vehicle.GetID());

        //--------------------------------------------------
        // Спавним гражданского (группу)
        //--------------------------------------------------
        vector crewOffset = "2 0 0"; // немножко в сторону, чтобы не коллизить
        IEntity crewGroup = OVT_Global.SpawnEntityPrefab(TSE_CIV_GROUP_PREFAB, pos + crewOffset);
        if (!crewGroup)
        {
            Print("[CivDrivers] Failed to spawn civ group at " + pos);
            return;
        }
        m_SpawnedEntities.Insert(crewGroup.GetID());

        SCR_AIGroup aiGroup = SCR_AIGroup.Cast(crewGroup);
        if (!aiGroup)
        {
            Print("[CivDrivers] Spawned civ group is not AIGroup");
            return;
        }

        // Откладываем посадку до инициализации группы
        m_mPendingCrewAssignments.Insert(aiGroup.GetID(), vehicle.GetID());
        aiGroup.GetOnInit().Insert(OnCrewGroupInitialized);

        // Note: Specific AIGroup policies not available in this env; skipping.
    }

    //---------------------------------------------------------
    // Crew boarding
    //---------------------------------------------------------
    void OnCrewGroupInitialized(SCR_AIGroup group)
    {
        if (!group) return;
        EntityID groupID = group.GetID();
        if (!m_mPendingCrewAssignments.Contains(groupID))
        {
            return;
        }

        EntityID vehicleID = m_mPendingCrewAssignments.Get(groupID);
        IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehicleID);
        if (!vehicle)
        {
            m_mPendingCrewAssignments.Remove(groupID);
            return;
        }

        SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
        if (!compMgr)
        {
            m_mPendingCrewAssignments.Remove(groupID);
            return;
        }

        array<AIAgent> agents = {};
        group.GetAgents(agents);
        int agentIndex = 0;

        // Сажаем первого агента водителем
        if (agents.Count() > 0)
        {
            MoveAgentToCompartment(compMgr, agents[0], ECompartmentType.PILOT);

            // Рандомизируем одежду водителя
            OVT_Global.RandomizeCivilianClothes(agents[0]);
        }

        // Удаляем остальных агентов, если они есть, чтобы остался ровно один гражданский
        for (int i = 1; i < agents.Count(); i++)
        {
            IEntity extraChar = agents[i].GetControlledEntity();
            if (extraChar)
            {
                SCR_EntityHelper.DeleteEntityAndChildren(extraChar);
            }
        }

        m_mPendingCrewAssignments.Remove(groupID);

        // --- Назначаем вейпоинт перемещения ---
        vector dest = GetRandomDestination(vehicle.GetOrigin());
        OVT_OverthrowConfigComponent cfg = OVT_Global.GetConfig();
        if(cfg)
        {
            AIWaypoint moveWp = cfg.SpawnMoveWaypoint(dest);
            if(moveWp)
            {
                group.AddWaypoint(moveWp);
                Print(string.Format("[CivDrivers] Added move waypoint to group %1 at %2", groupID, dest.ToString()));
                // Сохраняем назначение для отслеживания прибытия
                m_mVehicleDestinations.Insert(vehicle.GetID(), GetMarkerByPosition(dest));
                array<AIWaypoint> wps = {};
                group.GetWaypoints(wps);
                Print(string.Format("[CivDrivers] Group %1 now has %2 waypoints", groupID, wps.Count()));
            }
        }
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

    // Возвращает случайную точку из спавнов, отличную от current
    vector GetRandomDestination(vector current)
    {
        if(m_SpawnMarkers.IsEmpty())
        {
            // fallback — вернём ту же точку
            return current;
        }

        array<vector> positions = {};
        foreach(IEntity m : m_SpawnMarkers)
        {
            if(!m) continue;
            vector p = m.GetOrigin();
            if(vector.DistanceSq(p, current) > 1) // не совпадает с текущей точкой
            {
                positions.Insert(p);
            }
        }

        if(positions.IsEmpty())
        {
            return current;
        }

        int idx = Math.RandomInt(0, positions.Count());
        return positions[idx];
    }

    // Вызывается компонентом на префабе спавна
    void RegisterSpawnMarker(IEntity marker)
    {
        if (!marker) return;
        if (m_RegisteredMarkers.Contains(marker)) return;
        m_RegisteredMarkers.Insert(marker);
    }

    //---------------------------------------------------------
    // Cleanup on deletion (optional)
    //---------------------------------------------------------
    void OnDestroy()
    {
        // Останавливаем периодические таймеры
        GetGame().GetCallqueue().Remove(PeriodicSpawn);
        GetGame().GetCallqueue().Remove(CheckArrivals);
        
        // Удаляем созданные сущности
        foreach (EntityID id : m_SpawnedEntities)
        {
            IEntity ent = GetGame().GetWorld().FindEntityByID(id);
            if (ent)
                SCR_EntityHelper.DeleteEntityAndChildren(ent);
        }
        m_SpawnedEntities.Clear();
    }

    // Найти маркер по позиции (точное совпадение координат)
    IEntity GetMarkerByPosition(vector pos)
    {
        foreach(IEntity m : m_SpawnMarkers)
        {
            if(!m) continue;
            if(vector.DistanceSq(m.GetOrigin(), pos) < 0.1) return m;
        }
        return null;
    }

    //---------------------------------------------------------
    // Arrival check
    //---------------------------------------------------------
    void StartArrivalMonitor()
    {
        GetGame().GetCallqueue().CallLater(CheckArrivals, 300000, true); // 5 минут
    }

    void CheckArrivals()
    {
        array<EntityID> toRemove = {};
        for(int i = 0; i < m_mVehicleDestinations.Count(); i++)
        {
            EntityID vehID = m_mVehicleDestinations.GetKey(i);
            IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehID);
            if(!vehicle)
            {
                toRemove.Insert(vehID);
                continue;
            }
            IEntity marker = m_mVehicleDestinations.Get(vehID);
            if(!marker)
            {
                toRemove.Insert(vehID);
                continue;
            }
            if(vector.Distance(vehicle.GetOrigin(), marker.GetOrigin()) < 10)
            {
                Print("[CivDrivers] Vehicle reached destination -> despawn and respawn new driver");
                // Delete vehicle and occupants
                SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
                // Spawn new driver at this marker
                SpawnDriverAt(marker.GetOrigin());
                toRemove.Insert(vehID);
            }
        }
        foreach(EntityID id : toRemove)
        {
            m_mVehicleDestinations.Remove(id);
        }
    }
} 