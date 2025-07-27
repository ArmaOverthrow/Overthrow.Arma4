
//----------------------------------------------------------------------------------------------------
//  Extra cleanup logic for vehicles that are not owned by players and were saved in the database
//  by various scripted systems (civ drivers, patrols, events, etc.).
//  We execute this after the vanilla InitialVehicleCleanup finishes.
//----------------------------------------------------------------------------------------------------

modded class OVT_VehicleManagerComponent
{
    /* List of vehicles that matched cleanup criteria in current pass */
    protected ref array<EntityID> m_aCleanupCandidates;
    protected bool m_bCleanupScheduled = false;
    protected ref array<EntityID> m_aPatrolCandidates;

    // Периодическая очистка пустых патрулей
    static const int PATROL_CLEANUP_INTERVAL_MS = 360000; // 6 минут
    static const int PATROL_CLEANUP_REWARD      = 50;     // ресурсов за машину

    //------------------------------------------------------------------------------
    // Override of vanilla method that is called ~5s after gamemode init once
    // EPF finished loading entities from DB. We call the original implementation
    // first (player-vehicle handling) and then run our extra pass.
    //------------------------------------------------------------------------------
    override protected void InitialVehicleCleanup()
    {
        super.InitialVehicleCleanup(); // keep vanilla logic

        // Run only on server side
        if (!Replication.IsServer())
            return;

        CleanupAbandonedVehicles();

        // Стартуем периодический таймер очистки патрулей
        if (!m_bCleanupScheduled)
        {
            m_bCleanupScheduled = true; // переиспользуем флаг, чтобы не стартовать несколько раз
            GetGame().GetCallqueue().CallLater(CleanupPatrolVehiclesNearBases, PATROL_CLEANUP_INTERVAL_MS, true);
        }
    }

    //------------------------------------------------------------------------------
    // Main routine - gathers all unowned vehicles in the world that are not located
    // inside a base area and removes them together with their persistence records.
    //------------------------------------------------------------------------------
    protected void CleanupAbandonedVehicles()
    {
        m_aCleanupCandidates = new array<EntityID>();

        // Collect all vehicles in the world - radius large enough to cover map
        GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 999999, null, FilterAbandonedVehicleAdd, EQueryEntitiesFlags.ALL);

        if (m_aCleanupCandidates.IsEmpty())
            return;

        // Access persistence manager once for DB removals
        OVT_OverthrowGameMode gm = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        OVT_PersistenceManagerComponent persistenceMgr = null;
        if (gm)
        {
            persistenceMgr = gm.GetPersistence();
        }

        int deleted = 0;
        foreach (EntityID entId : m_aCleanupCandidates)
        {
            IEntity vehicle = GetGame().GetWorld().FindEntityByID(entId);
            if (!vehicle)
                continue;

            // Try to remove save-data so it will not reappear next restart
            if (persistenceMgr)
            {
                EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(vehicle.FindComponent(EPF_PersistenceComponent));
                if (persistence)
                {
                    string persistentId = persistence.GetPersistentId();
                    if (persistentId != "")
                    {
                        // Obtain global EPF persistence manager to access DB context
                        EPF_PersistenceManager epfMgr = EPF_PersistenceManager.GetInstance();
                        if (epfMgr)
                        {
                            array<ref EDF_DbEntity> rows = epfMgr
                                .GetDbContext()
                                .FindAll(EPF_VehicleSaveData, EDF_DbFind.Id().Equals(persistentId), limit: 1)
                                .GetEntities();

                            if (rows && !rows.IsEmpty())
                            {
                                epfMgr.GetDbContext().RemoveAsync(rows.Get(0));
                            }
                        }
                    }
                }
            }

            // Finally delete from world
            SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
            deleted++;
        }

        Print(string.Format("[TSE] CleanupAbandonedVehicles removed %1 obsolete vehicles", deleted));

        // Schedule one more cleanup pass after 3 minutes to catch delayed spawns
        if (!m_bCleanupScheduled)
        {
            m_bCleanupScheduled = true;
            GetGame().GetCallqueue().CallLater(CleanupAbandonedVehicles, 180000, false); // 3 минуты
        }
    }

    //------------------------------------------------------------------------------
    // Periodic cleanup of empty patrol vehicles inside bases
    //------------------------------------------------------------------------------
    protected void CleanupPatrolVehiclesNearBases()
    {
        m_aPatrolCandidates = new array<EntityID>();

        // Сбор кандидатов
        GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 999999, null, FilterPatrolVehicleAdd, EQueryEntitiesFlags.ALL);

        int removed = 0;
        // Используем карту "ключ позиции" -> уже сохранён первый юнит
        ref map<string, bool> positionRegistry = new map<string, bool>();

        foreach (EntityID vid : m_aPatrolCandidates)
        {
            IEntity v = GetGame().GetWorld().FindEntityByID(vid);
            if (!v) continue;

            vector pos = v.GetOrigin();
            // Квантуем позицию до 2 м по XZ для надёжного сравнения (убираем ошибки округления)
            string key = string.Format("%1_%2", Math.Floor(pos[0] / 2) * 2, Math.Floor(pos[2] / 2) * 2);

            if (!positionRegistry.Contains(key))
            {
                // Первый объект в этой точке оставляем
                positionRegistry.Insert(key, true);
                continue;
            }

            // Дубликат — удаляем
            SCR_EntityHelper.DeleteEntityAndChildren(v);
            removed++;
        }

        if (removed > 0)
        {
            // Reward Occupying Faction
            OVT_OccupyingFactionManager occ = OVT_Global.GetOccupyingFaction();
            if (occ)
            {
                occ.RecoverResources(removed * PATROL_CLEANUP_REWARD);
            }

            Print(string.Format("[TSE] Patrol cleanup removed %1 vehicles (rewarded %2 resources)", removed, removed * PATROL_CLEANUP_REWARD));
        }
    }

    //------------------------------------------------------------------------------
    // Callback used by QueryEntitiesBySphere to select vehicles eligible for removal
    //------------------------------------------------------------------------------
    protected bool FilterAbandonedVehicleAdd(IEntity entity)
    {
        Vehicle veh = Vehicle.Cast(entity);
        if (!veh)
            return false; // not a drivable vehicle, skip

        // Keep vehicles that already belong to a player
        OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(entity.FindComponent(OVT_PlayerOwnerComponent));
        if (ownerComp && ownerComp.GetPlayerOwnerUid() != "")
            return false;

        // Skip vehicles that are positioned inside (or very near) a faction base - they
        // might be part of base defences and should persist.
        if (IsInsideAnyBase(entity.GetOrigin()))
            return false;

        // Everything else is a valid cleanup candidate
        m_aCleanupCandidates.Insert(entity.GetID());
        return false; // continue search
    }

    //------------------------------------------------------------------------------
    // Helper that decides whether a world position belongs to any known base.
    // Uses occupying faction bases; player bases are covered by ownership checks.
    //------------------------------------------------------------------------------
    protected bool IsInsideAnyBase(vector pos)
    {
        const float BASE_RADIUS = 300.0; // расширенный радиус для блок-постов

        OVT_OccupyingFactionManager occ = OVT_Global.GetOccupyingFaction();
        if (occ)
        {
            OVT_BaseData nearestBase = occ.GetNearestBase(pos);
            if (nearestBase && vector.Distance(nearestBase.location, pos) < BASE_RADIUS)
                return true;
        }

        return false;
    }

    // Callback to collect empty patrol vehicles inside bases
    protected bool FilterPatrolVehicleAdd(IEntity entity)
    {
        Vehicle veh = Vehicle.Cast(entity);
        if (!veh) return false;

        // Skip player vehicles
        OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(entity.FindComponent(OVT_PlayerOwnerComponent));
        if (ownerComp && ownerComp.GetPlayerOwnerUid() != "")
            return false;

        // Skip occupied vehicles
        SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(entity.FindComponent(SCR_BaseCompartmentManagerComponent));
        if (compMgr)
        {
            array<IEntity> occ = {};
            compMgr.GetOccupants(occ);
            if (occ && occ.Count() > 0)
                return false;
        }

        // Only inside base radius
        if (!IsInsideAnyBase(entity.GetOrigin()))
            return false;

        m_aPatrolCandidates.Insert(entity.GetID());
        return false;
    }
} 