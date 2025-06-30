class OVT_TownManagerComponentClass: OVT_ComponentClass
{
};

class OVT_TownModifierData : Managed
{
	int id;
	int timer;
}

class OVT_TownData : Managed
{
	vector location;
	int population;
	int stability;
	int support;
	int faction;
	
	[NonSerialized()]
	int size;
	
	ref array<ref OVT_TownModifierData> stabilityModifiers = {};
	ref array<ref OVT_TownModifierData> supportModifiers = {};
	
	vector gunDealerPosition;
	
	//! Area heat level for undercover system (persisted)
	float areaHeat = 0.0;
	
	int SupportPercentage()
	{
		if(population == 0) 
			return 0;
		return Math.Round((support / population ) * 100);
	}
	
	OVT_Faction ControllingFaction()
	{
		return OVT_Global.GetFactions().GetOverthrowFactionByIndex(faction);
	}
	
	Faction ControllingFactionData()
	{
		return GetGame().GetFactionManager().GetFactionByIndex(faction);
	}
	
	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}
	
	//! Gets the current area heat level
	float GetAreaHeat()
	{
		return areaHeat;
	}
	
	//! Sets the area heat level
	void SetAreaHeat(float heat)
	{
		areaHeat = Math.Max(0.0, heat); // Prevent negative heat
	}
	
	void CopyFrom(OVT_TownData town)
	{
		population = town.population;
		stability = town.stability;
		support = town.support;
		faction = town.faction;
		stabilityModifiers = town.stabilityModifiers;
		supportModifiers = town.supportModifiers;
		gunDealerPosition = town.gunDealerPosition;
		areaHeat = town.areaHeat;
	}
}

//------------------------------------------------------------------------------------------------
//! Manages towns, their populations, stability, support, and controlling factions within the Overthrow gamemode.
//! Handles town initialization, modifier systems, house queries, and network synchronization of town data.
class OVT_TownManagerComponent: OVT_Component
{
	[Attribute( defvalue: "1200", desc: "Range to search cities for houses")]
	int m_iCityRange;
	
	[Attribute( defvalue: "600", desc: "Range to search towns for houses")]
	int m_iTownRange;
	
	[Attribute( defvalue: "250", desc: "Range to search villages for houses")]
	int m_iVillageRange;
	
	[Attribute( defvalue: "2", desc: "Default occupants per house")]
	int m_iDefaultHouseOccupants;
	
	[Attribute( defvalue: "3", desc: "Occupants per villa house")]
	int m_iVillaOccupants;
	
	[Attribute( defvalue: "5", desc: "Occupants per town house")]
	int m_iTownOccupants;
	
	[Attribute("", UIWidgets.Object)]	
	ref array<ref OVT_TownModifierSystem> m_aTownModifiers;
	
	[Attribute("", UIWidgets.Object)]	
	ref array<ref string> m_aIgnoreTowns;
	
	protected int m_iTownCount=0;
	
	//! Array of all towns managed by this component
	ref array<ref OVT_TownData> m_Towns;
	//! Array of town names, corresponding to the m_Towns array by index
	ref array<ref string> m_TownNames;
	
	protected IEntity m_EntitySearched;
	
	protected OVT_TownData m_CheckTown;
	
	protected ref array<ref EntityID> m_Houses;
	
	OVT_RealEstateManagerComponent m_RealEstate;
	
	//! Invoked when a town's controlling faction changes
	ref ScriptInvoker<IEntity> m_OnTownControlChange = new ScriptInvoker<IEntity>;
	
	const int MODIFIER_FREQUENCY = 10000;
	protected int m_iSupportCounter = 0;
	protected const int SUPPORT_FREQUENCY = 6; // * MODIFIER_FREQUENCY
	 
	static OVT_TownManagerComponent s_Instance;	
	
	//------------------------------------------------------------------------------------------------
	//! Returns the singleton instance of the Town Manager Component
	//! \return OVT_TownManagerComponent instance or null if not found
	static OVT_TownManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_TownManagerComponent.Cast(pGameMode.FindComponent(OVT_TownManagerComponent));
		}

		return s_Instance;
	}
	
	void OVT_TownManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Towns = new array<ref OVT_TownData>;
		m_TownNames = new array<ref string>;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initializes the Town Manager Component, finding towns and setting up modifiers.
	//! Called once during game initialization.
	//! \param owner The owning entity of this component
	void Init(IEntity owner)
	{		
		m_RealEstate = OVT_Global.GetRealEstate();		
		foreach(OVT_TownModifierSystem system : m_aTownModifiers)
		{
			system.Init();
		}		
		
		InitializeTowns();
		
		if(!Replication.IsServer()) return;
		SetupTowns();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called after the game has started, schedules recurring updates and spawns controllers.
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdateModifiers, MODIFIER_FREQUENCY, true, GetOwner());		
		GetGame().GetCallqueue().CallLater(SpawnTownControllers, 0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Streams the current town modifiers (stability and support) to a specific player.
	//! \param playerId The ID of the player to stream modifiers to
	void StreamTownModifiers(int playerId)
	{
		foreach(int townID, OVT_TownData town : m_Towns)
		{
			array<int> stability = new array<int>;
			foreach(OVT_TownModifierData data : town.stabilityModifiers)
			{
				if(data) stability.Insert(data.id);
			}
			array<int> support = new array<int>;
			foreach(OVT_TownModifierData data : town.supportModifiers)
			{
				if(data) support.Insert(data.id);
			}
			
			if(support.Count() > 0 || stability.Count() > 0)			
				Rpc(RpcDo_StreamModifiers, playerId, townID, stability, support);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gets a random town from the list of managed towns.
	//! \return A random OVT_TownData instance or null if no towns exist
	OVT_TownData GetRandomTown()
	{
		if(m_Towns.Count() == 0) return null;
		return m_Towns.GetRandomElement();		
	}
	
	/*
	Town Modifier Systems
	*/
	//------------------------------------------------------------------------------------------------
	//! Retrieves a specific town modifier system by its class type.
	//! \param typeName The typename of the modifier system to retrieve
	//! \return The OVT_TownModifierSystem instance or null if not found
	OVT_TownModifierSystem GetModifierSystem(typename typeName)
	{
		foreach(OVT_TownModifierSystem system : m_aTownModifiers)
		{
			if(system.ClassName() == typeName.ToString())
			{
				return system;
			}
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Periodically checks and updates town modifiers (stability, support) and related effects (towers, bases).
	protected void CheckUpdateModifiers()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		
		m_iSupportCounter++;
		bool dosupport = false;
		if(m_iSupportCounter > SUPPORT_FREQUENCY)
		{
			m_iSupportCounter = 0;
			dosupport = true;
		}
		
		foreach(OVT_TownData town : m_Towns)
		{
			int townID = GetTownID(town);
			bool recalc = false;
			OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);
			if(system && town.stabilityModifiers)
				if(system.OnTick(town.stabilityModifiers, town)) recalc = true;
			
			system = GetModifierSystem(OVT_TownSupportModifierSystem);
			if(system && town.supportModifiers)
				system.OnTick(town.supportModifiers, town);
			
			bool hasEnemyTower = false;
			bool hasFriendlyTower = false;
			foreach(OVT_RadioTowerData tower : of.m_RadioTowers)
			{				
				float dist = vector.Distance(town.location, tower.location);
				if(dist < OVT_Global.GetConfig().m_Difficulty.radioTowerRange)
				{
					if(tower.IsOccupyingFaction())
					{
						hasEnemyTower = true;
					}else{
						hasFriendlyTower = true;
					}
				}
			}			
			
			if(hasEnemyTower)
			{
				RemoveSupportModifierByName(townID, "NearbyRadioTowerPositive");
				TryAddSupportModifierByName(townID, "NearbyRadioTowerNegative");
			}else if(hasFriendlyTower)
			{
				RemoveSupportModifierByName(townID, "NearbyRadioTowerNegative");
				TryAddSupportModifierByName(townID, "NearbyRadioTowerPositive");
			}	
			
			bool hasEnemyBase = false;
			bool hasFriendlyBase = false;	
			
			foreach(OVT_BaseData base : of.m_Bases)
			{				
				float dist = vector.Distance(town.location, base.location);
				if(dist < OVT_Global.GetConfig().m_Difficulty.baseSupportRange)
				{
					if(base.IsOccupyingFaction())
					{
						hasEnemyBase = true;
					}else{
						hasFriendlyBase = true;
					}
				}
			}
			
			if(hasEnemyBase)
			{
				RemoveSupportModifierByName(townID, "NearbyBasePositive");
				TryAddSupportModifierByName(townID, "NearbyBaseNegative");
			}else if(hasFriendlyBase)
			{
				RemoveSupportModifierByName(townID, "NearbyBaseNegative");
				TryAddSupportModifierByName(townID, "NearbyBasePositive");
			}	
			
			if(recalc) RecalculateStability(townID);
						
			if(dosupport)
			{
				//We always recalculate support modifiers and add/remove supporters, but less often
				RecalculateSupport(townID);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the total population and total supporters across all towns.
	//! \param[out] population Total population count
	//! \param[out] supporters Total supporter count
	void GetTotalPopulationStats(out int population, out int supporters)
	{
		population = 0;
		supporters = 0;
		foreach(OVT_TownData town : m_Towns)
		{
			population += town.population;
			supporters += town.support;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a stability modifier from a specific town by its modifier index.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier to remove
	void RemoveStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.stabilityModifiers, index);
		if(i > -1)
		{
			town.stabilityModifiers.Remove(i);
			Rpc(RpcDo_RemoveStabilityModifier, townId, index);
			RecalculateStability(townId);
		}		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the ID (index in the m_Towns array) of a given town.
	//! \param town The OVT_TownData instance
	//! \return The integer ID of the town
	int GetTownID(OVT_TownData town)
	{
		return m_Towns.Find(town);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the index of a modifier within a modifier array based on its ID.
	//! \param modifiers The array of OVT_TownModifierData to search within
	//! \param id The ID of the modifier to find
	//! \return The index of the modifier in the array, or -1 if not found
	protected int GetModifierIndex(array<ref OVT_TownModifierData> modifiers, int id)
	{
		int i = -1;
		if(!modifiers) return -1;
		foreach(int index, OVT_TownModifierData data : modifiers)
		{
			if(!data) continue;
			if(data.id == id)
			{
				i = index;
				break;
			}
		}
		return i;
	}
		
	//------------------------------------------------------------------------------------------------
	//! Attempts to add a stability modifier to a town by its configuration name.
	//! \param townId The ID of the town
	//! \param name The name of the stability modifier as defined in the config
	void TryAddStabilityModifierByName(int townId, string name)
	{
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);
		system.TryAddByName(townId, name);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Attempts to add a support modifier to a town by its configuration name.
	//! \param townId The ID of the town
	//! \param name The name of the support modifier as defined in the config
	void TryAddSupportModifierByName(int townId, string name)
	{
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);
		system.TryAddByName(townId, name);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Removes a support modifier from a town by its configuration name.
	//! \param townId The ID of the town
	//! \param name The name of the support modifier as defined in the config
	void RemoveSupportModifierByName(int townId, string name)
	{
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);
		system.RemoveByName(townId, name);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Attempts to add a stability modifier to a town by its index ID. Handles stacking and timer resets.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier
	//! \return true if the modifier was added or refreshed, false if stacking limit reached
	bool TryAddStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_ModifierConfig mod = GetModifierSystem(OVT_TownStabilityModifierSystem).m_Config.m_aModifiers[index];
		//if(!(mod.flags & OVT_StabilityModifierFlags.ACTIVE)) return;
		if(GetModifierIndex(town.stabilityModifiers, index) == -1)
		{
			AddStabilityModifier(townId, index);
		}else if(mod.flags & OVT_ModifierFlags.STACKABLE)
		{
			//Is stackable, so stack it
			int num = 0;
			foreach(OVT_TownModifierData data : town.stabilityModifiers)
			{
				if(data.id == index) num++;
			}
			if(num < mod.stackLimit){
				AddStabilityModifier(townId, index);
			}else{
				return false;
			}
		}else{
			//Is not stackable, reset timer
			int i = GetModifierIndex(town.stabilityModifiers, index);			
			town.stabilityModifiers[i].timer = mod.timeout;
			Rpc(RpcDo_ResetStabilityModifier, townId, index);
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initiates the process to add a stability modifier via RPC.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier
	void AddStabilityModifier(int townId, int index)
	{
		Rpc(RpcAsk_AddStabilityModifier, townId, index);
	}
	
		
	//------------------------------------------------------------------------------------------------
	//! Removes a support modifier from a specific town by its modifier index.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier to remove
	void RemoveSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.supportModifiers, index);
		if(i > -1)
		{
			town.supportModifiers.Remove(i);
			Rpc(RpcDo_RemoveSupportModifier, townId, index);
		}		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Requests removal of a timed-out support modifier via RPC.
	//! \param townId The ID of the town
	//! \param index The index ID of the timed-out support modifier
	void TimeoutSupportModifier(int townId, int index)
	{		
		Rpc(RpcDo_RemoveSupportModifier, townId, index);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! Requests removal of a timed-out stability modifier via RPC.
	//! \param townId The ID of the town
	//! \param index The index ID of the timed-out stability modifier
	void TimeoutStabilityModifier(int townId, int index)
	{		
		Rpc(RpcDo_RemoveStabilityModifier, townId, index);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! Attempts to add a support modifier to a town by its index ID. Handles stacking and timer resets.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier
	//! \return true if the modifier was added or refreshed, false if stacking limit reached
	bool TryAddSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_ModifierConfig mod = GetModifierSystem(OVT_TownSupportModifierSystem).m_Config.m_aModifiers[index];
		//if(!(mod.flags & OVT_StabilityModifierFlags.ACTIVE)) return;
		if(GetModifierIndex(town.supportModifiers, index) == -1)
		{
			AddSupportModifier(townId, index);
		}else if(mod.flags & OVT_ModifierFlags.STACKABLE)
		{
			//Is stackable, so stack it
			int num = 0;
			foreach(OVT_TownModifierData data : town.supportModifiers)
			{
				if(data && data.id == index) num++;
			}
			if(num < mod.stackLimit)
			{
				AddSupportModifier(townId, index);
			}else{
				return false;
			}
		}else if(mod.timeout > 0){
			//Is not stackable, reset timer
			int i = GetModifierIndex(town.supportModifiers, index);			
			town.supportModifiers[i].timer = mod.timeout;
			Rpc(RpcDo_ResetSupportModifier, townId, index);
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initiates the process to add a support modifier via RPC.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier
	void AddSupportModifier(int townId, int index)
	{
		Rpc(RpcAsk_AddSupportModifier, townId, index);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Recalculates the stability value for a town based on its active stability modifiers.
	//! Updates the town's stability if changed and synchronizes via RPC.
	//! \param townId The ID of the town to recalculate
	protected void RecalculateStability(int townId)
	{
		OVT_TownData town = m_Towns[townId];		
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);
		
		int stab = system.Recalculate(town.stabilityModifiers);
		
		if(stab != town.stability)
		{
			town.stability = stab;
			Rpc(RpcDo_SetStability, townId, stab);
		}		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Recalculates the support value for a town based on its active support modifiers.
	//! Updates the town's support if changed and synchronizes via RPC.
	//! Also handles potential peaceful faction flips for villages based on support and stability thresholds.
	//! \param townId The ID of the town to recalculate
	protected void RecalculateSupport(int townId)
	{
		Faction playerFaction = GetGame().GetFactionManager().GetFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction);
		int playerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(playerFaction);
		
		Faction occupyingFaction = GetGame().GetFactionManager().GetFactionByKey(OVT_Global.GetConfig().m_sOccupyingFaction);
		int occupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(occupyingFaction);
				
		OVT_TownData town = m_Towns[townId];		
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);
		
		int newsupport = system.Recalculate(town.supportModifiers, town.support, 0, town.population);
		
		if(newsupport != town.support)
		{
			town.support = newsupport;
			Rpc(RpcDo_SetSupport, townId, newsupport);
		}
		
		//Villages only, see if we can peacefully flip it to the resistance (or back)
		if(town.size > 1) return;
		
		int support = town.SupportPercentage();		
		if(town.faction != playerFactionIndex && support >= 75 && town.stability >= 50)
		{
			//Chance this village will flip to the resistance
			if(support >= 85 || s_AIRandomGenerator.RandFloat01() < 0.5)
			{
				ChangeTownControl(town, playerFactionIndex);
			}			
		}else if(town.faction == playerFactionIndex && support < 25)
		{
			//Chance this village will flip back to the OF
			if(support < 15 || s_AIRandomGenerator.RandFloat01() < 0.5)
			{
				ChangeTownControl(town, occupyingFactionIndex);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Resets the support value of a town to 0 and synchronizes via RPC.
	//! \param town The town data to reset support for
	void ResetSupport(OVT_TownData town)
	{
		town.support = 0;
		int townID = GetTownID(town);
		Rpc(RpcDo_SetSupport, townID, 0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Changes the controlling faction of a town and notifies relevant systems.
	//! \param town The OVT_TownData instance of the town
	//! \param faction The new controlling faction index
	void ChangeTownControl(OVT_TownData town, int faction)
	{
		int townID = GetTownID(town);
		town.faction = faction;
		if(m_OnTownControlChange)
			m_OnTownControlChange.Invoke(town);
		
		Rpc(RpcDo_SetTownFaction, townID, faction);
		string type = "Village";
		if(town.size == 2) type = "Town";
		if(town.size == 3) type = "City";
		
		string factionType = "Resistance";
		if(faction != OVT_Global.GetConfig().GetPlayerFactionIndex()) factionType = "Occupying";
		
		OVT_Global.GetNotify().SendTextNotification(type + "Controlled" + factionType, -1, GetTownName(townID));
		OVT_Global.GetNotify().SendExternalNotifications(type + "Controlled" + factionType, GetTownName(townID));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a random, unowned house entity from any town.
	//! Tries multiple times if the first attempt fails or finds an owned house.
	//! \return A random house IEntity or null if none found after attempts
	IEntity GetRandomHouse()
	{
		m_Houses = new array<ref EntityID>;
		OVT_TownData town;
		IEntity house;
		int i = 0;
		
		while(!house && i < 20)
		{
			i++;
			town = m_Towns.GetRandomElement();
			GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iCityRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
			if(m_Houses.Count() == 0) continue;
			house = GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
			if(m_RealEstate.IsOwned(house.GetID())) house = null;			
		}
				
		return house;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest house entity within a small radius (25m) of a given position.
	//! \param pos The position vector to search around
	//! \return The nearest house IEntity or null if none found within range
	IEntity GetNearestHouse(vector pos)
	{
		m_Houses = new array<ref EntityID>;		
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 25, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		float nearest = 26;
		IEntity nearestEnt;		
		
		foreach(EntityID id : m_Houses)
		{			
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a random house entity within the bounds of a specific town.
	//! \param town The OVT_TownData instance of the town
	//! \return A random house IEntity within the town, or null if no houses found
	IEntity GetRandomHouseInTown(OVT_TownData town)
	{
		m_Houses = new array<ref EntityID>;		
		
		GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iTownRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		return GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the town whose center is geographically nearest to the given position.
	//! \param pos The position vector to check from
	//! \return The OVT_TownData of the nearest town
	OVT_TownData GetNearestTown(vector pos)
	{
		OVT_TownData nearestTown;
		float nearest = -1;
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestTown = town;
			}
		}
		return nearestTown;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the effective radius of a town based on its size (village, town, city).
	//! \param town The OVT_TownData instance
	//! \return The radius in meters
	float GetTownRange(OVT_TownData town)
	{
		float range = m_iCityRange;
		if(town.size == 1) range = m_iVillageRange;
		if(town.size == 2) range = m_iTownRange;
		return range;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest town that contains the given position within its effective radius.
	//! \param pos The position vector to check
	//! \return The OVT_TownData of the containing town, or null if the position is not within any town's range
	OVT_TownData GetNearestTownInRange(vector pos)
	{
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			int range = m_iCityRange;
			if(town.size == 2) range = m_iTownRange;
			if(town.size == 1) range = m_iVillageRange;
			if(distance <= range){
				return town;
			}
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest town map marker entity (City, Town, Village) within a small radius (5m) of a position.
	//! \param pos The position vector to search around
	//! \return The SCR_MapDescriptorComponent of the nearest town marker, or null if none found
	SCR_MapDescriptorComponent GetNearestTownMarker(vector pos)
	{	
		m_EntitySearched = null;	
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 5, null, FindTownMarker, EQueryEntitiesFlags.STATIC);
		if(!m_EntitySearched) return null;
		
		return SCR_MapDescriptorComponent.Cast(m_EntitySearched.FindComponent(SCR_MapDescriptorComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the OVT_TownData instance for a given town ID.
	//! \param townId The ID of the town (index in m_Towns)
	//! \return The OVT_TownData instance
	OVT_TownData GetTown(int townId)
	{
		return m_Towns[townId];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the display name of a town from its map marker, caching the result.
	//! \param townId The ID of the town
	//! \return The string name of the town
	string GetTownName(int townId)
	{
		if(m_TownNames[townId] == "")
		{
			SCR_MapDescriptorComponent desc = GetNearestTownMarker(m_Towns[townId].location);
			m_TownNames[townId] = desc.Item().GetDisplayName();
		}
		return m_TownNames[townId];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the display name of the town nearest to a given location.
	//! \param location The position vector
	//! \return The string name of the nearest town
	string GetTownName(vector location)
	{
		OVT_TownData town = GetNearestTown(location);
		int townId = GetTownID(town);
		
		return GetTownName(townId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest bus stop map marker entity within a radius (15m) of a position.
	//! \param pos The position vector to search around
	//! \return The SCR_MapDescriptorComponent of the nearest bus stop marker, or null if none found
	SCR_MapDescriptorComponent GetNearestBusStop(vector pos)
	{	
		m_EntitySearched = null;	
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FindBusStop, EQueryEntitiesFlags.STATIC);
		if(!m_EntitySearched) return null;
		
		return SCR_MapDescriptorComponent.Cast(m_EntitySearched.FindComponent(SCR_MapDescriptorComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Fills an array with references to all towns located within a specified distance from a position.
	//! \param pos The center position vector for the search
	//! \param maxDistance The maximum distance (radius) to search within
	//! \param[out] towns The array to be filled with OVT_TownData references
	void GetTownsWithinDistance(vector pos, float maxDistance, out array<ref OVT_TownData> towns)
	{
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			if(distance < maxDistance){
				towns.Insert(town);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Returns the array containing all managed towns.
	//! \return Reference to the array<ref OVT_TownData> m_Towns
	array<ref OVT_TownData> GetTowns()
	{
		return m_Towns;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initializes town data by querying map markers across the entire world.
	//! Called once during Init.
	protected void InitializeTowns()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding cities, towns and villages");
		#endif	
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckCityTownAddPopulation, FilterCityTownEntities, EQueryEntitiesFlags.STATIC);
		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawns a town controller prefab at the location of each managed town.
	//! Called once after game start.
	protected void SpawnTownControllers()
	{
		foreach(OVT_TownData town : m_Towns)
		{
			IEntity controller = OVT_Global.SpawnEntityPrefab(OVT_Global.GetConfig().m_pTownControllerPrefab, town.location);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query callback function used during town initialization (InitializeTowns).
	//! Processes found town map markers.
	//! \param entity The entity found by the query (potential town marker)
	//! \return Always true to continue the query
	protected bool CheckCityTownAddPopulation(IEntity entity)
	{	
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			ProcessTown(entity, mapdesc);
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Creates and initializes an OVT_TownData object based on a map marker entity.
	//! Adds the new town data to the m_Towns and m_TownNames arrays.
	//! \param entity The town map marker entity
	//! \param mapdesc The MapDescriptorComponent of the entity
	protected void ProcessTown(IEntity entity, MapDescriptorComponent mapdesc)
	{
		OVT_TownData town = new OVT_TownData();
		int townID = GetTownID(town);
		
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(OVT_Global.GetConfig().m_sOccupyingFaction);
			
		townID = m_iTownCount;
		town.location = entity.GetOrigin();
		town.population = 0;
		town.support = 0;
		town.faction = GetGame().GetFactionManager().GetFactionIndex(faction);
		town.stability = 100;
		
		m_iTownCount++;
		
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_VILLAGE) town.size = 1;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_TOWN) town.size = 2;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_CITY) town.size = 3;
		
		m_Towns.Insert(town);
		m_TownNames.Insert(mapdesc.Item().GetDisplayName());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the initial population for each town by querying nearby house entities.
	//! Called once during server Init.
	protected void SetupTowns()
	{
		foreach(OVT_TownData town : m_Towns)
		{
			m_CheckTown = town;
		
			int range = m_iTownRange;
			if(town.size == 1) range = m_iVillageRange;
			if(town.size == 3) range = m_iCityRange;
			
			GetGame().GetWorld().QueryEntitiesBySphere(town.location, range, CheckHouseAddPopulation, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query filter function used during town initialization (InitializeTowns).
	//! Checks if an entity is a city, town, or village map marker.
	//! \param entity The entity to filter
	//! \return true if the entity is a valid town marker type, false otherwise
	protected bool FilterCityTownEntities(IEntity entity) 
	{		
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){			
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_CITY) return true;
			if(type == EMapDescriptorType.MDT_NAME_VILLAGE) return true;
			if(type == EMapDescriptorType.MDT_NAME_TOWN) return true;
		}
				
		return false;		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query filter function used by GetNearestTownMarker.
	//! Checks if an entity is a city, town, or village map marker and stores it if found.
	//! \param entity The entity to check
	//! \return false once a marker is found to stop the query, true otherwise
	protected bool FindTownMarker(IEntity entity) 
	{		
		bool got = false;
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_CITY) got = true;
			if(type == EMapDescriptorType.MDT_NAME_VILLAGE) got = true;
			if(type == EMapDescriptorType.MDT_NAME_TOWN) got = true;
		}
		
		if(got)
		{
			m_EntitySearched = entity;
		}
				
		return false;		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query filter function used by GetNearestBusStop.
	//! Checks if an entity is a bus stop map marker and stores it if found.
	//! \param entity The entity to check
	//! \return false once a marker is found to stop the query, true otherwise
	protected bool FindBusStop(IEntity entity) 
	{		
		bool got = false;
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_BUSSTOP) got = true;
		}
		
		if(got)
		{
			m_EntitySearched = entity;
		}
				
		return false;		
	}
	

	//------------------------------------------------------------------------------------------------
	//! Removes a specified number of supporters and population from the town nearest to a position.
	//! Synchronizes changes via RPC.
	//! \param pos The position vector to find the nearest town from
	//! \param num The number of supporters/population to remove (default: 1)
	void TakeSupportersFromNearestTown(vector pos, int num = 1)
	{
		OVT_TownData town = GetNearestTown(pos);
		int townID = GetTownID(town);
		if(town.support < num || town.population < num) return;
		RpcDo_SetSupport(townID, town.support - num);
		Rpc(RpcDo_SetSupport, townID, town.support - num);
		
		RpcDo_SetPopulation(townID, town.population - num);
		Rpc(RpcDo_SetPopulation, townID, town.population - num);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if the nearest town to a position has available supporters for recruitment.
	//! \param pos The position vector to find the nearest town from
	//! \param num The number of supporters needed (default: 1)
	//! \return true if the nearest town has enough supporters and population, false otherwise
	bool NearestTownHasSupporters(vector pos, int num = 1)
	{
		OVT_TownData town = GetNearestTown(pos);
		if (!town)
			return false;
			
		return (town.support >= num && town.population >= num);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Adds a specified number of supporters to the town nearest to a position.
	//! Synchronizes changes via RPC.
	//! \param pos The position vector to find the nearest town from
	//! \param num The number of supporters to add (default: 1)
	void AddSupport(vector pos, int num = 1)
	{
		OVT_TownData town = GetNearestTown(pos);
		int townID = GetTownID(town);
				
		RpcDo_SetSupport(townID, town.support + num);
		Rpc(RpcDo_SetSupport, townID, town.support + num);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query callback function used during town setup (SetupTowns).
	//! Adds population to the currently checked town (m_CheckTown) based on the house type found.
	//! \param entity The house entity found by the query
	//! \return Always true to continue the query
	protected bool CheckHouseAddPopulation(IEntity entity)
	{
		VObject mesh = entity.GetVObject();
		if(mesh){
			string res = mesh.GetResourceName();
			int pop = m_iDefaultHouseOccupants;
			if(res.IndexOf("/Villa/") > -1)
				pop = m_iVillaOccupants;
			if(res.IndexOf("/Town/") > -1)
				pop = m_iTownOccupants;
			
			m_CheckTown.population += pop;
		}
				
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query callback function used by house-finding methods (GetRandomHouse, GetNearestHouse, etc.).
	//! Adds the ID of an unowned house entity to the m_Houses array.
	//! \param entity The house entity found by the query
	//! \return Always true to continue the query
	protected bool CheckHouseAddToArray(IEntity entity)
	{
		EntityID id = entity.GetID();
		if(!m_RealEstate.IsOwned(id))
			m_Houses.Insert(id);
				
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Query filter function used by house-finding and population calculation methods.
	//! Checks if an entity is a valid, non-military/industrial/ruined house building.
	//! \param entity The entity to filter
	//! \return true if the entity is a valid house for population/ownership, false otherwise
	protected bool FilterHouseEntities(IEntity entity) 
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity"){
			ResourceName prefab = entity.GetPrefabData().GetPrefabName();
			foreach(string s : OVT_Global.GetConfig().m_aStartingHouseFilters)
			{
				if(prefab.IndexOf(s) > -1) return false;
			}	
			
			VObject mesh = entity.GetVObject();
			
			if(mesh){
				string res = mesh.GetResourceName();
				if(res.IndexOf("/Military/") > -1) return false;
				if(res.IndexOf("/Industrial/") > -1) return false;
				if(res.IndexOf("/Recreation/") > -1) return false;
				
				if(res.IndexOf("/Houses/") > -1){
					if(res.IndexOf("_ruin") > -1) return false;
					if(res.IndexOf("/Shed/") > -1) return false;
					if(res.IndexOf("/Garage/") > -1) return false;
					if(res.IndexOf("/HouseAddon/") > -1) return false;
					return true;
				}
					
			}
		}
		return false;
	}
	
	//RPC Methods
	
	//------------------------------------------------------------------------------------------------
	//! Saves town data (population, stability, support, faction) for network replication (Join-In-Progress).
	//! \param[in] writer The ScriptBitWriter to write data to
	//! \return true if serialization is successful
	override bool RplSave(ScriptBitWriter writer)
	{	
			
		//Send JIP towns
		int length = m_Towns.Count();
		//Print("Writing " + length + " towns");
		writer.WriteInt(length); 
		for(int i=0; i<length; i++)
		{			
			OVT_TownData town = m_Towns[i];
			//int townID = GetTownID(town);
			//Print("Writing town ID " + townID);
			
			writer.WriteInt(town.population);
			writer.WriteInt(town.stability);
			writer.WriteInt(town.support);
			writer.WriteInt(town.faction);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loads town data (population, stability, support, faction) received from the server during Join-In-Progress.
	//! \param[in] reader The ScriptBitReader to read data from
	//! \return true if deserialization is successful
	override bool RplLoad(ScriptBitReader reader)
	{		
				
		//Recieve JIP towns
		int length, modlength;
		vector pos;
		
		if (!reader.ReadInt(length)) return false;
		//Print("Replicating " + length + " towns");
		for(int i=0; i<length; i++)
		{			
			OVT_TownData town = GetTown(i);
			
			//Print("Replicating town ID " + i);
				
			if (!reader.ReadInt(town.population)) return false;		
			if (!reader.ReadInt(town.stability)) return false;		
			if (!reader.ReadInt(town.support)) return false;		
			if (!reader.ReadInt(town.faction)) return false;
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side RPC handler for adding a stability modifier.
	//! Creates the modifier data, applies it, recalculates stability, and broadcasts the addition.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = mod.timeout;
		
		town.stabilityModifiers.Insert(data);
		
		RecalculateStability(townId);		
		Rpc(RpcDo_AddStabilityModifier, townId, index, mod.timeout);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side RPC handler for adding a support modifier.
	//! Creates the modifier data, applies it, and broadcasts the addition.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = mod.timeout;
		
		town.supportModifiers.Insert(data);
				
		Rpc(RpcDo_AddSupportModifier, townId, index, mod.timeout);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to update the stability value of a town on all clients.
	//! \param townId The ID of the town
	//! \param value The new stability value
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetStability(int townId, int value)
	{
		OVT_TownData town = m_Towns[townId];
		town.stability = value;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to update the support value of a town on all clients.
	//! \param townId The ID of the town
	//! \param value The new support value
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetSupport(int townId, int value)
	{
		OVT_TownData town = m_Towns[townId];
		town.support = value;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to update the population value of a town on all clients.
	//! \param townId The ID of the town
	//! \param value The new population value
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPopulation(int townId, int value)
	{
		OVT_TownData town = m_Towns[townId];
		town.population = value;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to add a stability modifier to a town on all clients.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier
	//! \param timer The initial timer value for the modifier
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddStabilityModifier(int townId, int index, int timer)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = timer;
		
		town.stabilityModifiers.Insert(data);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to add a support modifier to a town on all clients.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier
	//! \param timer The initial timer value for the modifier
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddSupportModifier(int townId, int index, int timer)
	{
		OVT_TownData town = m_Towns[townId];
		
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = timer;
		
		town.supportModifiers.Insert(data);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to remove a stability modifier from a town on all clients.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier to remove
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.stabilityModifiers, index);
		if(i > -1)
		{
			town.stabilityModifiers.Remove(i);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to remove a support modifier from a town on all clients.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier to remove
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.supportModifiers, index);
		if(i > -1)
		{
			town.supportModifiers.Remove(i);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to reset the timer of an existing stability modifier on all clients.
	//! \param townId The ID of the town
	//! \param index The index ID of the stability modifier to reset
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ResetStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		int i = GetModifierIndex(town.stabilityModifiers, index);
		if(i > -1)
		{
			town.stabilityModifiers[i].timer = mod.timeout;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to reset the timer of an existing support modifier on all clients.
	//! \param townId The ID of the town
	//! \param index The index ID of the support modifier to reset
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ResetSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		int i = GetModifierIndex(town.supportModifiers, index);
		if(i > -1)
		{
			town.supportModifiers[i].timer = mod.timeout;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Targeted RPC (effectively broadcast, but filtered locally) to stream initial modifiers to a joining player.
	//! \param playerId The ID of the player the modifiers are intended for
	//! \param townId The ID of the town whose modifiers are being streamed
	//! \param stability Array of stability modifier IDs active in the town
	//! \param support Array of support modifier IDs active in the town
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_StreamModifiers(int playerId, int townId, array<int> stability, array<int> support)
	{
		int localId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		if(playerId != localId) return;
				
		OVT_TownData town = m_Towns[townId];
		foreach(int id : stability)
		{
			OVT_TownModifierData data = new OVT_TownModifierData;
			OVT_ModifierConfig mod = GetModifierSystem(OVT_TownStabilityModifierSystem).m_Config.m_aModifiers[id];
			data.id = id;
			data.timer = mod.timeout;
			town.stabilityModifiers.Insert(data);
		}
		foreach(int id : support)
		{
			OVT_TownModifierData data = new OVT_TownModifierData;
			OVT_ModifierConfig mod = GetModifierSystem(OVT_TownSupportModifierSystem).m_Config.m_aModifiers[id];
			data.id = id;
			data.timer = mod.timeout;
			town.supportModifiers.Insert(data);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast RPC to set the controlling faction of a town on all clients.
	//! \param townId The ID of the town
	//! \param index The new faction index
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetTownFaction(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		town.faction = index;
	}
	
	
}