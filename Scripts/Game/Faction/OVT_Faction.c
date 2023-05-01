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

class OVT_Faction : SCR_Faction
{
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (all)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupPrefabSlots;
	
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
	ResourceName m_aFlagPolePrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Medium Checkpoint Prefab", params: "et", category: "Faction Objects")]
	ResourceName m_aMediumCheckpointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Large Checkpoint Prefab", params: "et", category: "Faction Objects")]
	ResourceName m_aLargeCheckpointPrefab;
	
	[Attribute()]
	ref array<ResourceName> m_aArsenalConfigFiles;
	
	ref array<ref SCR_ArsenalItemListConfig> m_aArsenalConfigs;
	
	[Attribute()]
	ref OVT_FactionCompositionConfig m_aCompositionConfig;
	
	void OVT_Faction()
	{
		m_aArsenalConfigs = new array<ref SCR_ArsenalItemListConfig>;		
	}
	
	void LoadArsenalConfigs()
	{
		if(!m_aArsenalConfigFiles) return;
		foreach(ResourceName res : m_aArsenalConfigFiles)
		{
			Resource holder = BaseContainerTools.LoadContainer(res);
			if (holder)		
			{
				SCR_ArsenalItemListConfig obj = SCR_ArsenalItemListConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
				if(obj)
				{
					m_aArsenalConfigs.Insert(obj);
				}
			}
		}
	}
	
	override bool GetArsenalItems(out array<ref SCR_ArsenalItem> arsenalItems)
	{
		if(m_aArsenalConfigs.Count() == 0) LoadArsenalConfigs();
		foreach(SCR_ArsenalItemListConfig config : m_aArsenalConfigs)
		{
			array<ref SCR_ArsenalItem> items = new array<ref SCR_ArsenalItem>;
			config.GetArsenalItems(items);
			foreach(SCR_ArsenalItem item : items)
			{
				if(item.GetItemResourceName() == "") continue;
				arsenalItems.Insert(item);
			}
		}
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