class OVT_OverthrowConfigComponentClass: OVT_ComponentClass
{	
};

enum OVT_QRFFastTravelMode
{
	FREE,
	NOQRF,
	DISABLED
}

enum OVT_FactionType {
	OCCUPYING_FACTION,
	RESISTANCE_FACTION,
	SUPPORTING_FACTION
}

enum OVT_PatrolType {
	DEFEND,
	PERIMETER	
}

class OVT_DifficultySettings : ScriptAndConfig
{	
	[Attribute()]
	string name;
	
	//Wanted system
	[Attribute(defvalue: "30000", desc: "Timeout in ms for wanted levels 2-5 (per level)", category: "Wanted System")]
	int wantedTimeout;
	[Attribute(defvalue: "120000", desc: "Timeout in ms for wanted level 1", category: "Wanted System")]
	int wantedOneTimeout;
	
	//OF
	[Attribute(defvalue: "2000", desc: "OF starting resources", category: "Occupying Faction")]
	int startingResources;
	[Attribute(defvalue: "250", desc: "OF resources per 6 hrs", category: "Occupying Faction")]
	int baseResourcesPerTick;
	[Attribute(defvalue: "500", desc: "Additional OF resources per 6 hrs (* threat)", category: "Occupying Faction")]
	int resourcesPerTick;
	[Attribute(defvalue: "10", desc: "Base resource cost", category: "Occupying Faction")]
	int baseResourceCost;	
	
	//Economy
	[Attribute(defvalue: "100", desc: "Player starting cash", category: "Economy")]
	int startingCash;
	[Attribute(defvalue: "5", desc: "Money taken from player per respawn", category: "Economy")]
	int respawnCost;
	[Attribute(defvalue: "5", desc: "Cost to fast travel", category: "Economy")]
	int fastTravelCost;
	[Attribute(defvalue: "1", desc: "Cost of placeables is multiplied by this value", category: "Economy")]
	float placeableCostMultiplier;
	[Attribute(defvalue: "1", desc: "Cost of buildables is multiplied by this value", category: "Economy")]
	float buildableCostMultiplier;
	[Attribute(defvalue: "0.5", desc: "Cost of Real Estate is multiplied by this value", category: "Economy")]
	float realEstateCostMultiplier;
	[Attribute(defvalue: "10", desc: "Donation income per civilian supporter", category: "Economy")]
	int donationIncome;
	[Attribute(defvalue: "5", desc: "Bus ticket price per km", category: "Economy")]
	int busTicketPrice;
	[Attribute(defvalue: "250", desc: "Base price for AI recruit", category: "Economy")]
	int baseRecruitCost;
	
	//RF
	[Attribute(defvalue: "0", desc: "Base RF threat", category: "Resistance Faction")]
	int baseThreat;	
	
	//QRF
	[Attribute("1", UIWidgets.ComboBox, "QRF Fast Travel Mode", "", ParamEnumArray.FromEnum(OVT_QRFFastTravelMode), category: "QRF" )]
	OVT_QRFFastTravelMode QRFFastTravelMode;
	
	[Attribute(defvalue: "500", desc: "Max size of QRF in resources", category: "QRF")]
	int maxQRF;
}

class OVT_OverthrowConfigComponent: OVT_Component
{
	[Attribute( defvalue: "FIA", uiwidget: UIWidgets.EditBox, desc: "Faction affiliation of the player's side", category: "Factions")]
	string m_sPlayerFaction;
	
	[Attribute( defvalue: "USSR", uiwidget: UIWidgets.EditBox, desc: "The faction occupying this map (the enemy)", category: "Factions")]
	string m_sDefaultOccupyingFaction;
	
	string m_sOccupyingFaction = "USSR";
	
	[Attribute( defvalue: "US", uiwidget: UIWidgets.EditBox, desc: "The faction supporting the player", category: "Factions")]
	string m_sSupportingFaction;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Town Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pTownControllerPrefab;
	
	[Attribute()]
	ref OVT_DifficultySettings m_Difficulty;
		
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_DifficultySettings> m_aDifficultyPresets;
		
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Gun Dealer Prefab", params: "et")]
	ResourceName m_pGunDealerPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Civilian Prefab", params: "et")]
	ResourceName m_pCivilianPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Move Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pMoveWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Defend Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pDefendWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Patrol Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pPatrolWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Wait Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pWaitWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Cycle Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pCycleWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Search and Destroy Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pSearchAndDestroyWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Get In Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pGetInWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Smart Action Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pSmartActionWaypointPrefab;
	
	[Attribute(desc: "Starting Houses (these should have parking spot entities added to their prefabs)", params: "et", category: "Real Estate")]
	ref array<string> m_aStartingHouseFilters;
	
	[Attribute(desc: "Real estate configs to set prices and rents for building types", category: "Real Estate", UIWidgets.Object)]
	ref array<ref OVT_RealEstateConfig> m_aBuildingTypes;
	
	[Attribute(defvalue: "6", UIWidgets.EditBox, desc: "Time multiplier")]
	int m_iTimeMultiplier;
	
	[Attribute(defvalue: "0.1", UIWidgets.EditBox, desc: "Civilians to spawn per population")]
	float m_fCivilianSpawnRate;
	
	[Attribute(defvalue: "2500", UIWidgets.EditBox, desc: "Civilian spawn distance")]
	int m_iCivilianSpawnDistance;
	
	[Attribute(defvalue: "0.1", UIWidgets.EditBox, desc: "NPC Shop Buy Rate", category: "Economy")]
	float m_fNPCBuyRate;
	
	[Attribute(defvalue: "0.25", UIWidgets.EditBox, desc: "Shop Profit Margin", category: "Economy")]
	float m_fShopProfitMargin;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Player Group Prefab", params: "et")]
	ResourceName m_pPlayerGroupPrefab;
	
	int m_iOccupyingFactionIndex = -1;
	int m_iPlayerFactionIndex = -1;
	
	[Attribute(defvalue: "false", UIWidgets.EditBox, desc: "Debug Mode")]
	bool m_bDebugMode;
	
	// Instance of this component
	private static OVT_OverthrowConfigComponent s_Instance = null;
	
	static OVT_OverthrowConfigComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_OverthrowConfigComponent.Cast(pGameMode.FindComponent(OVT_OverthrowConfigComponent));
		}

		return s_Instance;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(owner);
		if (!gameMode)
			// If parent is not gamemode, print an error
			Print("OVT_OverthrowConfigComponent has to be attached to a OVT_OverthrowGameMode (or inherited) entity!", LogLevel.ERROR);
			
		
	}
	
	int GetPlaceableCost(OVT_Placeable placeable)
	{
		return Math.Round(m_Difficulty.placeableCostMultiplier * placeable.m_iCost);
	}
	
	int GetBuildableCost(OVT_Buildable buildable)
	{
		return Math.Round(m_Difficulty.buildableCostMultiplier * buildable.m_iCost);
	}
	
	void SetOccupyingFaction(string key)
	{
		FactionManager factionMgr = GetGame().GetFactionManager();
		Faction faction = factionMgr.GetFactionByKey(key);
		Rpc(RpcAsk_SetOccupyingFaction, factionMgr.GetFactionIndex(faction));
	}
	
	OVT_Faction GetOccupyingFaction()
	{
		return OVT_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sOccupyingFaction));
	}
	
	int GetOccupyingFactionIndex()
	{
		if(m_iOccupyingFactionIndex == -1)
		{
			m_iOccupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(GetOccupyingFaction());
		}
		return m_iOccupyingFactionIndex;
	}
	
	OVT_Faction GetPlayerFaction()
	{
		return OVT_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sPlayerFaction));
	}
	
	int GetPlayerFactionIndex()
	{
		if(m_iPlayerFactionIndex == -1)
		{
			m_iPlayerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(GetPlayerFaction());
		}
		return m_iPlayerFactionIndex;
	}
	
	OVT_Faction GetFactionByType(OVT_FactionType type)
	{
		switch(type)
		{
			case OVT_FactionType.OCCUPYING_FACTION:
				return GetOccupyingFaction();		
		}
		return GetPlayerFaction();
	}
	
	AIWaypoint SpawnWaypoint(ResourceName res, vector pos)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		AIWaypoint wp = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(res), null, params));
		return wp;
	}
	
	AIWaypoint SpawnPatrolWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pPatrolWaypointPrefab, pos);
		return wp;
	}
	
	AIWaypoint SpawnDefendWaypoint(vector pos, int preset = 0)
	{
		AIWaypoint wp = SpawnWaypoint(m_pDefendWaypointPrefab, pos);
		SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(wp);
		defend.SetCurrentDefendPreset(preset);
		return wp;
	}
	
	SCR_EntityWaypoint SpawnGetInWaypoint(IEntity target)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = target.GetOrigin();
		SCR_EntityWaypoint wp = SCR_EntityWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_pGetInWaypointPrefab), null, params));
		
		wp.SetEntity(target);
		
		return wp;
	}
	
	SCR_TimedWaypoint SpawnWaitWaypoint(vector pos, float time)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		SCR_TimedWaypoint wp = SCR_TimedWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_pWaitWaypointPrefab), null, params));
		
		return wp;
	}
	
	SCR_SmartActionWaypoint SpawnActionWaypoint(vector pos, IEntity target, string action)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		SCR_SmartActionWaypoint wp = SCR_SmartActionWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_pSmartActionWaypointPrefab), null, params));
		
		wp.SetSmartActionEntity(target, action);
		
		return wp;
	}
	
	void GivePatrolWaypoints(SCR_AIGroup aigroup, OVT_PatrolType type, vector center = "0 0 0")
	{
		if(center[0] == 0) center = aigroup.GetOrigin();
		
		if(type == OVT_PatrolType.DEFEND)
		{
			aigroup.AddWaypoint(SpawnDefendWaypoint(center));
			return;
		}
		
		if(type == OVT_PatrolType.PERIMETER)
		{
			float dist = vector.Distance(aigroup.GetOrigin(), center);
			vector dir = vector.Direction(aigroup.GetOrigin(), center);
			float angle = dir.VectorToAngles()[1];
			
			array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
			AIWaypoint firstWP;
			for(int i; i< 4; i++)
			{
				vector pos = center + (Vector(0,angle,0).AnglesToVector() * dist);
				
				AIWaypoint wp = SpawnPatrolWaypoint(pos);
				queueOfWaypoints.Insert(wp);
				
				AIWaypoint wait = SpawnWaitWaypoint(pos, s_AIRandomGenerator.RandFloatXY(45, 75));								
				queueOfWaypoints.Insert(wait);
				
				angle += 90;
				if(angle > 359) angle -= 360;
			}
			AIWaypointCycle cycle = AIWaypointCycle.Cast(SpawnWaypoint(m_pCycleWaypointPrefab, aigroup.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
			return;
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetOccupyingFaction(int index)
	{
		FactionManager factionMgr = GetGame().GetFactionManager();
		m_iOccupyingFactionIndex = index;
		Faction faction = factionMgr.GetFactionByIndex(index);
		if(!faction) return;
		m_sOccupyingFaction = faction.GetFactionKey();
	}
}