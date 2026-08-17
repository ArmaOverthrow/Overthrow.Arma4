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

//! Single-character registry entry.
//!
//! The group and vehicle registries below answer "give me a squad / a truck for this faction". This
//! one answers "give me ONE named character" - an officer, a courier, a defector - which is what a
//! job that spawns a target needs. Without it a job config has to name a prefab literally, and a
//! literal USSR officer is simply wrong on a campaign whose occupier is the US.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sCharacterName")]
class OVT_FactionCharacterEntry
{
	[Attribute(desc: "Unique name for this character type, e.g. Officer. Referenced by name from job configs")]
	string m_sCharacterName;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Character prefab", params: "et")]
	ResourceName m_sCharacterPrefab;

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

//! Character registry for spawning one named character of a faction
[BaseContainerProps()]
class OVT_FactionCharacterRegistry
{
	[Attribute(desc: "Named single characters for this faction")]
	ref array<ref OVT_FactionCharacterEntry> m_aCharacterEntries;

	//------------------------------------------------------------------------------------------------
	void OVT_FactionCharacterRegistry()
	{
		if (!m_aCharacterEntries)
			m_aCharacterEntries = new array<ref OVT_FactionCharacterEntry>;
	}

	//------------------------------------------------------------------------------------------------
	OVT_FactionCharacterEntry FindCharacterByName(string characterName)
	{
		foreach (OVT_FactionCharacterEntry entry : m_aCharacterEntries)
		{
			if (entry.m_sCharacterName == characterName)
				return entry;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The prefab registered under that name, or an empty ResourceName when this faction has
	//! no such character. Callers must handle empty - a faction is allowed not to define one.
	ResourceName GetCharacterPrefab(string characterName)
	{
		OVT_FactionCharacterEntry entry = FindCharacterByName(characterName);
		if (entry)
			return entry.m_sCharacterPrefab;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetAllCharacterNames()
	{
		array<string> names = new array<string>;
		foreach (OVT_FactionCharacterEntry entry : m_aCharacterEntries)
		{
			names.Insert(entry.m_sCharacterName);
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

	//! Named single characters, referenced by name from job configs
	[Attribute(desc: "Named single characters for this faction", category: "Character Registry")]
	ref OVT_FactionCharacterRegistry m_CharacterRegistry;
		
	// RETIRED 2026-08-18 (virtualization/base-defense-migration, decision D9). The whole "Legacy
	// Faction Groups" / "Faction Groups" prefab-slot block is gone, along with the by-enum resolver
	// that picked a random element out of it (GetGroupPrefabByType below is its replacement, and it
	// answers from the registry). It was the last path in the codebase that resolved a force to a RAW
	// PREFAB rather than to a (factionKey, groupName) pair, which is the only identity the
	// virtualization core can register a composition under - so anything reached through it could
	// never have dead members that stay dead.
	// Every meaning it carried survives as a NAMED ENTRY in m_GroupRegistry above:
	//   Light Infantry / Light Town Patrol / Tower Defense Patrol -> "light_patrol"
	//   Heavy Infantry (and Special Forces) -> "heavy_infantry"      AT -> "at_team"
	//   Sniper -> "sniper"                   Sniper Team -> "sniper_team"
	// The MG, AT-single, FRAG, Heavy Town Patrol and SpecOps Patrol entries were swept in the same pass:
	// they had neither a reader nor an authored value in any faction config, in either shipped faction
	// prefab, or anywhere else in the tree.
	// The checkpoint prefabs that used to live under "Faction Objects" went with them - they are now the
	// "MediumCheckpoint" / "LargeCheckpoint" entries in m_aCompositionConfig - and the Cars / Trucks
	// vehicle arrays are the "car" / "truck" entries in m_VehicleRegistry.
	// ⚠ The attributes and their authored values were deleted in ONE change-set. An authored value with
	// no attribute behind it is a parse warning on every load.

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction vehicles (all)", params: "et", category: "Faction Vehicles")]
	ref array<ResourceName> m_aVehiclePrefabSlots;
	
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
		
	[Attribute()]
	ref OVT_FactionCompositionConfig m_aCompositionConfig;
	
	ref array<ResourceName> m_aGroupPrefabSlots = {};
	
	void Init()
	{
		if (SCR_Global.IsEditMode()) return;
		
		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey));
		if (!faction)
		{
			Print("[Overthrow] OVT_Faction.Init: no faction registered for key '" + m_sFactionKey + "' - check the m_sFactionKey in this faction's OverthrowData config", LogLevel.ERROR);
			return;
		}
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.GROUP);
		if (!catalog)
		{
			Print("[Overthrow] OVT_Faction.Init: faction '" + m_sFactionKey + "' has no GROUP entity catalog", LogLevel.ERROR);
			return;
		}
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

	//------------------------------------------------------------------------------------------------
	//! Prefab for one of this faction's named characters.
	//! \param[in] characterName Name as authored in the faction's character registry, e.g. "Officer".
	//! \return The prefab, or an empty ResourceName when this faction defines no such character.
	ResourceName GetCharacterPrefab(string characterName)
	{
		if (!m_CharacterRegistry) return "";

		return m_CharacterRegistry.GetCharacterPrefab(characterName);
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
	
	//------------------------------------------------------------------------------------------------
	//! A group prefab for a coarse OVT_GroupType, resolved through the GROUP REGISTRY.
	//!
	//! WHY THE ENUM STILL EXISTS AT ALL. It is a job-stage authoring convenience
	//! (OVT_SpawnGroupJobStage.m_GroupType), not a resolution mechanism: a job config picks a rough
	//! shape of force and does not want to know a faction's registry names. Everything systemic - every
	//! deployment, every garrison, every patrol - asks for a registry NAME instead, because that is the
	//! only identity the virtualization core resolves a composition from.
	//!
	//! THE MAP, AND THE ONE JUDGEMENT CALL IN IT. LIGHT_INFANTRY -> "light_patrol",
	//! HEAVY_INFANTRY -> "heavy_infantry", ANTI_TANK -> "at_team", SNIPER -> "sniper" are each the
	//! registry entry that replaced the legacy array of the same meaning. SPECIAL_FORCES has no
	//! registry entry and maps to "heavy_infantry", which is the only general-purpose combat group both
	//! shipped factions define; the legacy USSR value for it was a manoeuvre group (itself a member of
	//! the heavy-infantry list) and the legacy US value was a sniper team, which reads as an authoring
	//! accident rather than a design worth inheriting. Nothing authors SPECIAL_FORCES today.
	//!
	//! THE FALLBACK IS THE VANILLA GROUP CATALOG and it is load-bearing, not defensive: a modded
	//! faction that ships no registry entry under one of these names still gets a group of some sort
	//! rather than an empty ResourceName and a silent no-spawn. m_aGroupPrefabSlots is built from the
	//! faction's own vanilla GROUP entity catalog in Init(), so it is never empty for a real faction.
	//! \param[in] type The coarse group type authored on the job stage.
	//! \return A group prefab, or an empty ResourceName only when the faction has no catalog at all.
	ResourceName GetGroupPrefabByType(OVT_GroupType type)
	{
		string groupName;

		switch(type)
		{
			case OVT_GroupType.LIGHT_INFANTRY:
				groupName = "light_patrol";
				break;
			case OVT_GroupType.HEAVY_INFANTRY:
				groupName = "heavy_infantry";
				break;
			case OVT_GroupType.ANTI_TANK:
				groupName = "at_team";
				break;
			case OVT_GroupType.SPECIAL_FORCES:
				groupName = "heavy_infantry";
				break;
			case OVT_GroupType.SNIPER:
				groupName = "sniper";
				break;
		}

		if(groupName != "")
		{
			ResourceName prefab = GetGroupPrefabByName(groupName);
			if(prefab != "") return prefab;
		}

		if(!m_aGroupPrefabSlots || m_aGroupPrefabSlots.IsEmpty()) return "";

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