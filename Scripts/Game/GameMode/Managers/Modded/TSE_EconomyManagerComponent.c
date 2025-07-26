// Fixed version of BuildResourceDatabase with null-safety for factions without Overthrow data

modded class OVT_EconomyManagerComponent
{
    //----------------------------------------------------------------------------------------------
    // Builds the complete database of resources (items, vehicles) from configurations and faction data.
    // Adds a null-check for OVT_Faction to avoid VM exceptions when a game faction does not have
    // an Overthrow counterpart.
    //----------------------------------------------------------------------------------------------
    override void BuildResourceDatabase()
    {
        //-------------------------------------------------------------------------
        // 1) Process vehicle prefabs defined directly in the vehicle price config
        //-------------------------------------------------------------------------
        foreach (OVT_VehiclePriceConfig cfg : m_VehiclePriceConfig.m_aPrices)
        {
            if (cfg.m_sFind == "" && cfg.prefab != "" && !cfg.hidden)
            {
                ResourceName res = cfg.prefab;
                if (!m_aResources.Contains(res))
                {
                    m_aResources.Insert(res);
                    int id = m_aResources.Count() - 1;
                    m_aResourceIndex[res] = id;
                    m_aAllVehicles.Insert(id);
                    SetPrice(id, cfg.cost);
                    m_mVehicleParking[id] = cfg.parking;
                    if (!cfg.illegal)
                        m_aLegalVehicles.Insert(id);
                }
            }
        }

        //-------------------------------------------------------------------------
        // 2) Iterate over all factions provided by the game and gather their items
        //-------------------------------------------------------------------------
        FactionManager factionMgr = GetGame().GetFactionManager();
        array<Faction> factions = new array<Faction>();
        factionMgr.GetFactionsList(factions);

        foreach (Faction faction : factions)
        {
            // Obtain corresponding Overthrow faction definition (may be nullptr)
            OVT_Faction fac = OVT_Global.GetFactions().GetOverthrowFactionByKey(faction.GetFactionKey());
            if (!fac)
            {
                // Faction is not used by Overthrow – safely skip to avoid null-pointer crash
                continue;
            }

            int factionId = factionMgr.GetFactionIndex(faction);
            m_mFactionResources[factionId] = new array<int>();

            //---------------------------------------------
            // 2a) Inventory items
            //---------------------------------------------
            array<SCR_EntityCatalogEntry> items = new array<SCR_EntityCatalogEntry>();
            fac.GetAllInventoryItems(items);

            foreach (SCR_EntityCatalogEntry item : items)
            {
                bool configMatched = false;
                ResourceName res = item.GetPrefab();
                if (res == "" || m_aResources.Contains(res) || !item.IsEnabled())
                    continue;

                // Skip non-arsenal items
                SCR_NonArsenalItemCostCatalogData nonArsenalData = SCR_NonArsenalItemCostCatalogData.Cast(item.GetEntityDataOfType(SCR_NonArsenalItemCostCatalogData));
                if (nonArsenalData)
                    continue;

                bool   hidden  = false;
                int    cost    = 50;
                int    demand  = 5;

                // Apply price configs (later configs override earlier ones)
                foreach (OVT_PriceConfig config : m_PriceConfig.m_aPrices)
                {
                    bool matches = false;

                    if (config.m_sFind == "")
                    {
                        SCR_ArsenalItem arsenalItem = SCR_ArsenalItem.Cast(item.GetEntityDataOfType(SCR_ArsenalItem));
                        if (arsenalItem && arsenalItem.GetItemType() == config.m_eItemType)
                        {
                            if (config.m_eItemMode == SCR_EArsenalItemMode.DEFAULT || arsenalItem.GetItemMode() == config.m_eItemMode)
                                matches = true;
                        }
                    }
                    else
                    {
                        matches = res.IndexOf(config.m_sFind) > -1;
                    }

                    if (!matches)
                        continue;

                    if (config.hidden)
                    {
                        hidden = true;
                        break;
                    }

                    cost         = config.cost;
                    demand       = config.demand;
                    configMatched = true; // continue to allow overrides
                }

                if (hidden)
                    continue;

                if (!configMatched)
                {
                    Print("[Overthrow] Matching item price config not found for " + res);
                    SCR_ArsenalItem arsenalItem = SCR_ArsenalItem.Cast(item.GetEntityDataOfType(SCR_ArsenalItem));
                    if (arsenalItem)
                        cost = arsenalItem.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT);
                }

                m_aResources.Insert(res);
                int id = m_aResources.Count() - 1;
                m_aResourceIndex[res] = id;
                m_aEntityCatalogEntries.Insert(item);
                m_mFactionResources[factionId].Insert(id);
                SetPrice(id, cost);
                SetDemand(id, demand);
            }

            //---------------------------------------------
            // 2b) Vehicles
            //---------------------------------------------
            array<SCR_EntityCatalogEntry> vehicles = new array<SCR_EntityCatalogEntry>();
            fac.GetAllVehicles(vehicles);

            foreach (SCR_EntityCatalogEntry item : vehicles)
            {
                ResourceName res = item.GetPrefab();
                if (res == "" || res.IndexOf("Campaign") > -1 || !item.IsEnabled())
                    continue;

                if (!m_aResources.Contains(res))
                {
                    bool           illegal      = false;
                    bool           hidden       = false;
                    int            cost         = 500000;
                    OVT_ParkingType parkingType = OVT_ParkingType.PARKING_CAR;

                    foreach (OVT_VehiclePriceConfig cfg : m_VehiclePriceConfig.m_aPrices)
                    {
                        if (cfg.prefab != "")
                            continue;
                        if (cfg.m_sFind == "" || res.IndexOf(cfg.m_sFind) > -1)
                        {
                            if (cfg.hidden)
                            {
                                hidden = true;
                                break;
                            }
                            cost       = cfg.cost;
                            illegal    = cfg.illegal;
                            parkingType = cfg.parking;
                        }
                    }

                    if (hidden)
                        continue;

                    if (fac.GetFactionKey() == "CIV")
                        illegal = false;

                    m_aResources.Insert(res);
                    int id = m_aResources.Count() - 1;
                    m_aResourceIndex[res] = id;
                    m_aEntityCatalogEntries.Insert(item);
                    m_mFactionResources[factionId].Insert(id);
                    m_aAllVehicles.Insert(id);

                    if (cost == 50000)
                        Print("[Overthrow] Default price being set for: " + res);

                    m_mVehicleParking[id] = parkingType;
                    SetPrice(id, cost);
                    if (!illegal)
                        m_aLegalVehicles.Insert(id);
                }
            }
        }

        //-------------------------------------------------------------------------
        // 3) Gun-dealer prefab overrides
        //-------------------------------------------------------------------------
        foreach (OVT_PrefabItemCostConfig item : m_GunDealerConfig.m_aGunDealerItemPrefabs)
        {
            ResourceName res = item.m_sEntityPrefab;
            if (res == "")
                continue;

            if (!m_aResources.Contains(res))
            {
                m_aResources.Insert(res);
                int id = m_aResources.Count() - 1;
                m_aResourceIndex[res] = id;
                SetPrice(id, item.cost);
            }
        }

        // Price configs for inventory items are handled inline above – nothing else to do here
    }
} 