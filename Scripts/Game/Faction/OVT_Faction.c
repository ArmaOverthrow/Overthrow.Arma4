[BaseContainerProps(configRoot:true)]
class OVT_FactionCompositionConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_FactionComposition> m_aCompositions;
}

enum OVT_GroupType
{
	LIGHT_INFANTRY,
	HEAVY_INFANTRY,
	ANTI_TANK,
	SPECIAL_FORCES,
	SNIPER
}

//! New string-based group registry entry
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sGroupName")]
class OVT_FactionGroupEntry
{
	[Attribute(desc: "Unique name for this group type")]
	string m_sGroupName;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Group prefab", params: "et")]
	ResourceName m_sGroupPrefab;
	
	[Attribute(defvalue: "10", desc: "Resource cost for this group")]
	int m_iCost;
	
	[Attribute(defvalue: "1", desc: "Relative spawn weight (higher = more likely to be selected)")]
	int m_iWeight;
	
	[Attribute(desc: "Optional description")]
	string m_sDescription;
}

//! Vehicle registry entry
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sVehicleName")]
class OVT_FactionVehicleEntry
{
	[Attribute(desc: "Unique name for this vehicle type")]
	string m_sVehicleName;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Vehicle prefab", params: "et")]
	ResourceName m_sVehiclePrefab;
	
	[Attribute(defvalue: "50", desc: "Resource cost for this vehicle")]
	int m_iCost;
	
	[Attribute(defvalue: "1", desc: "Relative spawn weight (higher = more likely to be selected)")]
	int m_iWeight;
	
	[Attribute(defvalue: "1", desc: "Minimum crew size needed")]
	int m_iMinCrew;
	
	[Attribute(defvalue: "4", desc: "Maximum capacity including crew")]
	int m_iMaxCapacity;
	
	[Attribute(desc: "Optional description")]
	string m_sDescription;
}

//! Vehicle registry for flexible vehicle management
[BaseContainerProps()]
class OVT_FactionVehicleRegistry
{
	[Attribute(desc: "Available vehicle types for this faction")]
	ref array<ref OVT_FactionVehicleEntry> m_aVehicleEntries;
	
	//------------------------------------------------------------------------------------------------
	void OVT_FactionVehicleRegistry()
	{
		if (!m_aVehicleEntries)
			m_aVehicleEntries = new array<ref OVT_FactionVehicleEntry>;
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_FactionVehicleEntry FindVehicleByName(string vehicleName)
	{
		foreach (OVT_FactionVehicleEntry entry : m_aVehicleEntries)
		{
			if (entry.m_sVehicleName == vehicleName)
				return entry;
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetVehiclePrefab(string vehicleName)
	{
		OVT_FactionVehicleEntry entry = FindVehicleByName(vehicleName);
		if (entry)
			return entry.m_sVehiclePrefab;
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	int GetVehicleCost(string vehicleName)
	{
		OVT_FactionVehicleEntry entry = FindVehicleByName(vehicleName);
		if (entry)
			return entry.m_iCost;
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetRandomVehiclePrefab()
	{
		if (m_aVehicleEntries.IsEmpty())
			return "";
		
		// Build weighted selection array
		array<OVT_FactionVehicleEntry> weightedEntries = new array<OVT_FactionVehicleEntry>;
		foreach (OVT_FactionVehicleEntry entry : m_aVehicleEntries)
		{
			for (int i = 0; i < entry.m_iWeight; i++)
			{
				weightedEntries.Insert(entry);
			}
		}
		
		if (weightedEntries.IsEmpty())
			return "";
		
		OVT_FactionVehicleEntry selected = weightedEntries.GetRandomElement();
		return selected.m_sVehiclePrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	array<string> GetAllVehicleNames()
	{
		array<string> names = new array<string>;
		foreach (OVT_FactionVehicleEntry entry : m_aVehicleEntries)
		{
			names.Insert(entry.m_sVehicleName);
		}
		return names;
	}
}

//! Group registry for flexible group management
[BaseContainerProps()]
class OVT_FactionGroupRegistry
{
	[Attribute(desc: "Available group types for this faction")]
	ref array<ref OVT_FactionGroupEntry> m_aGroupEntries;
	
	//------------------------------------------------------------------------------------------------
	void OVT_FactionGroupRegistry()
	{
		if (!m_aGroupEntries)
			m_aGroupEntries = new array<ref OVT_FactionGroupEntry>;
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_FactionGroupEntry FindGroupByName(string groupName)
	{
		foreach (OVT_FactionGroupEntry entry : m_aGroupEntries)
		{
			if (entry.m_sGroupName == groupName)
				return entry;
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetGroupPrefab(string groupName)
	{
		OVT_FactionGroupEntry entry = FindGroupByName(groupName);
		if (entry)
			return entry.m_sGroupPrefab;
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	int GetGroupCost(string groupName)
	{
		OVT_FactionGroupEntry entry = FindGroupByName(groupName);
		if (entry)
			return entry.m_iCost;
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetRandomGroupPrefab()
	{
		if (m_aGroupEntries.IsEmpty())
			return "";
		
		// Build weighted selection array
		array<OVT_FactionGroupEntry> weightedEntries = new array<OVT_FactionGroupEntry>;
		foreach (OVT_FactionGroupEntry entry : m_aGroupEntries)
		{
			for (int i = 0; i < entry.m_iWeight; i++)
			{
				weightedEntries.Insert(entry);
			}
		}
		
		if (weightedEntries.IsEmpty())
			return "";
		
		OVT_FactionGroupEntry selected = weightedEntries.GetRandomElement();
		return selected.m_sGroupPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	array<string> GetAllGroupNames()
	{
		array<string> names = new array<string>;
		foreach (OVT_FactionGroupEntry entry : m_aGroupEntries)
		{
			names.Insert(entry.m_sGroupName);
		}
		return names;
	}
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sTag")]
class OVT_FactionComposition
{
	[Attribute()]
	string m_sTag;
	
	[Attribute(defvalue:"5", desc:"Resource cost (multiplied by difficulty base resources)")]
	int m_iCost;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Prefabs", params: "et")]
	ref array<ResourceName> m_aPrefabs;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Groups", params: "et")]
	ref array<ResourceName> m_aGroupPrefabs;
}

[BaseContainerProps(configRoot:true), SCR_BaseContainerCustomTitleField("m_sFactionKey")]
class OVT_Faction
{
	[Attribute()]
	string m_sFactionKey;
	
	[Attribute()]
	bool m_bIsPlayable;
	
	//! New group registry system - use this for new deployments
	[Attribute(desc: "Group registry for deployment system", category: "Group Registry")]
	ref OVT_FactionGroupRegistry m_GroupRegistry;
	
	//! Vehicle registry system for deployments
	[Attribute(desc: "Vehicle registry for deployment system", category: "Vehicle Registry")]
	ref OVT_FactionVehicleRegistry m_VehicleRegistry;
		
	//! Legacy group prefabs - deprecated, use m_GroupRegistry instead
	//! Kept for compatibility with BaseUpgrade systems
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "LEGACY: Faction groups (Light Infantry)", params: "et", category: "Legacy Faction Groups")]
	ref array<ResourceName> m_aGroupInfantryPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Heavy Infantry)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aHeavyInfantryPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (AT)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupATPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Special Forces)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupSpecialPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Sniper)", params: "et", category: "Faction Groups")]
	ResourceName m_aGroupSniperPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Sniper)", params: "et", category: "Faction Groups")]
	ResourceName m_aGroupSniper2Prefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (MG)", params: "et", category: "Faction Groups")]
	ResourceName m_aGroupMGPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (AT)", params: "et", category: "Faction Groups")]
	ResourceName m_aGroupATPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (FRAG)", params: "et", category: "Faction Groups")]
	ResourceName m_aGroupFRAGPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Light Town Patrol)", params: "et", category: "Faction Groups")]
	ResourceName m_aLightTownPatrolPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Heavy Town Patrol)", params: "et", category: "Faction Groups")]
	ResourceName m_aHeavyTownPatrolPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (SpecOps Patrol)", params: "et", category: "Faction Groups")]
	ResourceName m_aSpecOpsPatrolPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Tower Defense Patrol)", params: "et", category: "Faction Groups")]
	ResourceName m_aTowerDefensePatrolPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction vehicles (all)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aVehiclePrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction vehicles (Cars)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aVehicleCarPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction vehicles (Trucks)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aVehicleTruckPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction vehicles (Lightly Armed)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aVehicleLightPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction vehicles (Heavy Armed)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aVehicleHeavyPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction gun tripods (Light)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aTripodLightPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction gun tripods (Heavy)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aTripodHeavyPrefabSlots;
		
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Flag Pole Prefab", params: "et", category: "Faction Objects")]
	ResourceName m_sFlagPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Medium Checkpoint Prefab", params: "et", category: "Faction Objects")]
	ResourceName m_aMediumCheckpointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Large Checkpoint Prefab", params: "et", category: "Faction Objects")]
	ResourceName m_aLargeCheckpointPrefab;
		
	[Attribute()]
	ref OVT_FactionCompositionConfig m_aCompositionConfig;
	
	ref array<ResourceName> m_aGroupPrefabSlots = {};
	
	void Init()
	{
		if (SCR_Global.IsEditMode()) return;
		
		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey));
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.GROUP);		
		array<SCR_EntityCatalogEntry> entries();
		catalog.GetEntityList(entries);
		
		foreach(SCR_EntityCatalogEntry entry : entries)
		{
			m_aGroupPrefabSlots.Insert(entry.GetPrefab());
		}
	}
		
	string GetFactionKey()
	{
		return m_sFactionKey;
	}
	
	bool IsPlayable()
	{
		return m_bIsPlayable;
	}
	
	bool GetAllInventoryItems(out array<SCR_EntityCatalogEntry> inventoryItems)
	{		
		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey));
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM);		
		catalog.GetEntityList(inventoryItems);		
		return true;
	}
	
	bool GetAllVehicles(out array<SCR_EntityCatalogEntry> vehicles)
	{		
		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey));
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);		
		catalog.GetEntityList(vehicles);		
		return true;
	}
	
	OVT_FactionComposition GetCompositionConfig(string tag)
	{
		if(!m_aCompositionConfig) return null;
		foreach(OVT_FactionComposition config :  m_aCompositionConfig.m_aCompositions)
		{
			if(config.m_sTag == tag) return config;
		}
		return null;
	}
	
	ResourceName GetRandomGroupByType(OVT_GroupType type)
	{
		switch(type)
		{				
			case OVT_GroupType.HEAVY_INFANTRY:
				return m_aHeavyInfantryPrefabSlots.GetRandomElement();
			case OVT_GroupType.ANTI_TANK:
				return m_aGroupATPrefabSlots.GetRandomElement();
			case OVT_GroupType.SPECIAL_FORCES:
				return m_aGroupSpecialPrefabSlots.GetRandomElement();
			case OVT_GroupType.SNIPER:
				return m_aGroupSniperPrefab;
		}
		
		return m_aGroupPrefabSlots.GetRandomElement();
	}
	
	//------------------------------------------------------------------------------------------------
	// New group registry methods
	//------------------------------------------------------------------------------------------------
	void InitializeGroupRegistry()
	{
		if (!m_GroupRegistry)
			m_GroupRegistry = new OVT_FactionGroupRegistry();
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetGroupPrefabByName(string groupName)
	{
		if (!m_GroupRegistry)
		{
			Print(string.Format("Group registry not initialized for faction %1", m_sFactionKey), LogLevel.WARNING);
			return "";
		}
		
		return m_GroupRegistry.GetGroupPrefab(groupName);
	}
	
	//------------------------------------------------------------------------------------------------
	int GetGroupCostByName(string groupName)
	{
		if (!m_GroupRegistry)
			return 0;
		
		return m_GroupRegistry.GetGroupCost(groupName);
	}
	
	//------------------------------------------------------------------------------------------------
	array<string> GetAvailableGroupNames()
	{
		if (!m_GroupRegistry)
		{
			array<string> empty = new array<string>;
			return empty;
		}
		
		return m_GroupRegistry.GetAllGroupNames();
	}
	
	//------------------------------------------------------------------------------------------------
	bool HasGroupType(string groupName)
	{
		if (!m_GroupRegistry)
			return false;
		
		return m_GroupRegistry.FindGroupByName(groupName) != null;
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetRandomGroupPrefab()
	{
		if (!m_GroupRegistry)
			return "";
		
		return m_GroupRegistry.GetRandomGroupPrefab();
	}
	
	//------------------------------------------------------------------------------------------------
	// Vehicle registry methods
	//------------------------------------------------------------------------------------------------
	void InitializeVehicleRegistry()
	{
		if (!m_VehicleRegistry)
			m_VehicleRegistry = new OVT_FactionVehicleRegistry();
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetVehiclePrefabByName(string vehicleName)
	{
		if (!m_VehicleRegistry)
		{
			Print(string.Format("Vehicle registry not initialized for faction %1", m_sFactionKey), LogLevel.WARNING);
			return "";
		}
		
		return m_VehicleRegistry.GetVehiclePrefab(vehicleName);
	}
	
	//------------------------------------------------------------------------------------------------
	int GetVehicleCostByName(string vehicleName)
	{
		if (!m_VehicleRegistry)
			return 0;
		
		return m_VehicleRegistry.GetVehicleCost(vehicleName);
	}
	
	//------------------------------------------------------------------------------------------------
	array<string> GetAvailableVehicleNames()
	{
		if (!m_VehicleRegistry)
		{
			array<string> empty = new array<string>;
			return empty;
		}
		
		return m_VehicleRegistry.GetAllVehicleNames();
	}
	
	//------------------------------------------------------------------------------------------------
	bool HasVehicleType(string vehicleName)
	{
		if (!m_VehicleRegistry)
			return false;
		
		return m_VehicleRegistry.FindVehicleByName(vehicleName) != null;
	}
	
	//------------------------------------------------------------------------------------------------
	ResourceName GetRandomVehiclePrefab()
	{
		if (!m_VehicleRegistry)
			return "";
		
		return m_VehicleRegistry.GetRandomVehiclePrefab();
	}
}