class OVT_OverthrowConfigComponentClass: OVT_ComponentClass
{	
};

enum OVT_QRFFastTravelMode
{
	FREE,
	NOQRF,
	DISABLED
}

class OVT_DifficultySettings : ScriptAndConfig
{	
	[Attribute()]
	string name;
	
	//Wanted system
	[Attribute(defvalue: "30000", desc: "Timeout in ms for wanted levels 2-5 (per level)")]
	int wantedTimeout;
	[Attribute(defvalue: "120000", desc: "Timeout in ms for wanted level 1")]
	int wantedOneTimeout;
	
	//OF
	[Attribute(defvalue: "2000", desc: "OF starting resources")]
	int startingResources;
	[Attribute(defvalue: "250", desc: "OF resources per 6 hrs")]
	int baseResourcesPerTick;
	[Attribute(defvalue: "500", desc: "Additional OF resources per 6 hrs (* threat)")]
	int resourcesPerTick;
	[Attribute(defvalue: "10", desc: "Base resource cost")]
	int baseResourceCost;	
	
	//RF
	[Attribute(defvalue: "100", desc: "Player starting cash")]
	int startingCash;
	[Attribute(defvalue: "0", desc: "Base RF threat")]
	int baseThreat;	
	[Attribute(defvalue: "5", desc: "Money taken from player per respawn")]
	int respawnCost;
	[Attribute(defvalue: "5", desc: "Cost to fast travel")]
	int fastTravelCost;
	[Attribute(defvalue: "1", desc: "Cost of placeables is multiplied by this value")]
	float placeableCostMultiplier;
	[Attribute(defvalue: "10", desc: "Donation income per civilian supporter")]
	int donationIncome;
	[Attribute(defvalue: "5", desc: "Bus ticket price per km")]
	int busTicketPrice;
	
	//QRF
	[Attribute("1", UIWidgets.ComboBox, "QRF Fast Travel Mode", "", ParamEnumArray.FromEnum(OVT_QRFFastTravelMode) )]
	OVT_QRFFastTravelMode QRFFastTravelMode;
	
	[Attribute(defvalue: "500", desc: "Max size of QRF in resources")]
	int maxQRF;
}

class OVT_Placeable : ScriptAndConfig
{
	[Attribute()]
	string name;
		
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Object Prefabs", params: "et")]
	ref array<ResourceName> m_aPrefabs;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "", "edds")]
	ResourceName m_tPreview;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;
	
	[Attribute(defvalue: "0", desc: "Place on walls")]
	bool m_bPlaceOnWall;
	
	[Attribute(defvalue: "0", desc: "Can place it anywhere")]
	bool m_bIgnoreLocation;
	
	[Attribute(defvalue: "0", desc: "Cannot place near towns or bases")]
	bool m_bAwayFromTownsBases;
	
	[Attribute(defvalue: "0", desc: "Must be placed near a town")]
	bool m_bNearTown;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_PlaceableHandler handler;
}


class OVT_OverthrowConfigComponent: OVT_Component
{
	[Attribute( defvalue: "FIA", uiwidget: UIWidgets.EditBox, desc: "Faction affiliation of the player's side", category: "Factions")]
	string m_sPlayerFaction;
	
	[Attribute( defvalue: "USSR", uiwidget: UIWidgets.EditBox, desc: "The faction occupying this map (the enemy)", category: "Factions")]
	string m_sOccupyingFaction;
	
	[Attribute( defvalue: "US", uiwidget: UIWidgets.EditBox, desc: "The faction supporting the player", category: "Factions")]
	string m_sSupportingFaction;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Town Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pTownControllerPrefab;
	
	[Attribute()]
	ref OVT_DifficultySettings m_Difficulty;
		
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_DifficultySettings> m_aDifficultyPresets;
	ref array<ref OVT_DifficultySettings> m_aDifficultyPresetsPacked = new array<ref OVT_DifficultySettings>();
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Placeable> m_aPlaceables;
	ref array<ref OVT_Placeable> m_aPlaceablesPacked = new array<ref OVT_Placeable>();
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Gun Dealer Prefab", params: "et")]
	ResourceName m_pGunDealerPrefab;
	
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
	
	[Attribute(desc: "Starting Houses (these should have parking spot entities added to their prefabs)", params: "et")]
	ref array<string> m_aStartingHouseFilters;
	
	[Attribute(defvalue: "6", UIWidgets.EditBox, desc: "Time multiplier")]
	int m_iTimeMultiplier;
	
	protected  int m_iOccupyingFactionIndex = -1;
	protected int m_iPlayerFactionIndex = -1;
	
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
}