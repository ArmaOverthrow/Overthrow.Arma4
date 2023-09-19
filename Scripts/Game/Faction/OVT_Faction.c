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
		
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Light Infantry)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupInfantryPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Heavy Infantry)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aHeavyInfantryPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (AT)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupATPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Special Forces)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupSpecialPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Sniper)", params: "et", category: "Faction Groups")]
	ResourceName m_aGroupSniperPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Faction group (Light Town Patrol)", params: "et", category: "Faction Groups")]
	ResourceName m_aLightTownPatrolPrefab;
	
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
}