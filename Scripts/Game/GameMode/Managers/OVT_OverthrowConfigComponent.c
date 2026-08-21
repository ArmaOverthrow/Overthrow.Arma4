class OVT_OverthrowConfigComponentClass: OVT_ComponentClass
{
};



enum OVT_FactionType {
	OCCUPYING_FACTION,
	RESISTANCE_FACTION,
	SUPPORTING_FACTION
}

enum OVT_FactionTypeFlag {
	OCCUPYING_FACTION = 1,
	RESISTANCE_FACTION = 2,
	SUPPORTING_FACTION = 4
}

//! How a group patrols. ⚠ APPEND ONLY, NEVER RENUMBER: the members' integer values are what
//! Configs/Deployment/*.conf and every authored job stage carry, so inserting a member in the middle
//! silently re-points every existing authored value at the wrong behaviour.
enum OVT_PatrolType {
	//! Hold one post. Nothing movable, so a dormant group holding this plan is never walked.
	DEFEND,
	//! Circle the centre on a ROAD-SNAPPED ring - the town patrol's geometry. Each corner is pulled
	//! onto the nearest road, which is right for a town and wrong for a base (a base's roads run
	//! through it, not around it), which is what PERIMETER_BASE exists for.
	PERIMETER,
	//! Circle the nearest base's AUTHORED PERIMETER SQUARE (OVT_BaseControllerComponent's
	//! m_fPerimeterRadius / m_fPerimeterRotation, jittered a little per patrol), NOT road-snapped.
	//! Appended 2026-08-18 by amendment A1 of virtualization/base-defense-migration.
	PERIMETER_BASE
}

class OVT_OverthrowConfigStruct
{
	string occupyingFaction;
	string supportingFaction;
	string discordWebHookURL;
	ref array<string> officers;
	string difficulty;
	bool showPlayerPosition;
	bool mobileFOBOfficersOnly;
	
	//Item placement limits
	int houseItemLimit;
	int campItemLimit;
	int fobItemLimit;
	
	//Difficulty settings
	bool overrideDifficulty;
	int startingCash;
	float gunDealerSellPriceMultiplier;
	float procurementMultiplier;
	float vehiclePriceMultiplier;
	//! Price per litre charged at static fuel stations. 0 disables fuel charging entirely. Unlike the
	//! server-only fields below this one IS replicated (CONFIG_STREAM_VERSION 4) — the client needs it
	//! for the Refuel action's price suffix and its own affordability gate.
	float fuelPricePerLitre;
	//! Gear fee multiplier for equipped tent recruits. Server-side only - see the note on
	//! OVT_DifficultySettings.recruitLoadoutFeeMultiplier. Nothing replicates this and nothing may
	//! start to: it is deliberately absent from RplSave/RplLoad below, which is why
	//! CONFIG_STREAM_VERSION did not have to move for the equipped-recruit purchase (decision D18).
	float recruitLoadoutFeeMultiplier;

	//! Global spawn distance (metres) for virtualized AI groups - the distance at which a registered
	//! group's members materialise (implementation.md D5, issue #100). Each registration may override
	//! it; a very large value keeps every registered group spawned permanently.
	//! SERVER-ONLY, exactly like recruitLoadoutFeeMultiplier: it is deliberately absent from
	//! RplSave/RplLoad below, so CONFIG_STREAM_VERSION does not move for it. It is now the ONLY spawn
	//! distance the campaign has: the separate military and civilian distances it used to sit beside
	//! both went with the systems that read them (the town-civilian spawner, then base defense), and
	//! ambient civilians ride this value through their source's m_iSpawnDistanceOverride.
	int virtualizationSpawnDistance;

	//! Operator multiplier on town civilian ambience density (civilians implementation.md §3.6). The
	//! per-town crowd is population x the authored rate x THIS, so 0 is the documented "no civilians
	//! on this server" switch and 2.0 doubles every town. The per-town hard cap below still applies
	//! on top of it.
	//! SERVER-ONLY, exactly like virtualizationSpawnDistance: it is deliberately absent from
	//! RplSave/RplLoad below, so no client ever reads it and CONFIG_STREAM_VERSION does not move.
	float civilianDensityMultiplier;

	//! Absolute per-town ceiling on ambient civilians, applied after the source's own min/max clamps.
	//! 0 or below means NO CAP, and that asymmetry is deliberate: LoadConfig() runs SetDefaults()
	//! before ReadValue, so a config file written before this key existed keeps the 30 below - but a
	//! file that explicitly carries 0 is an operator saying "uncapped", never "no civilians". Turning
	//! civilians off is civilianDensityMultiplier's job.
	//! SERVER-ONLY, same as the two fields above - not in RplSave/RplLoad, CONFIG_STREAM_VERSION
	//! unchanged.
	int maxCiviliansPerTown;

	//! Should a town's ambient crowd be DESPAWNED while a QRF is being fought in that town?
	//!
	//! DEFAULT FALSE, and that is a deliberate change from the pre-migration game (civilians decision
	//! D13, user amendment 2026-08-17). The old spawner despawned every town's civilians during any
	//! QRF anywhere, which was a performance shortcut from when Reforger AI was expensive - and it
	//! actively fights the way people play, because players recruit civilians specifically to help
	//! defend a town against the QRF and then watch them vanish as the battle starts. Heavy servers
	//! that want the AI budget back turn this on; when they do, only the town actually under attack
	//! loses its crowd (never the whole map), and that town re-rolls a fresh one after the battle.
	//! SERVER-ONLY, same as the fields above - not in RplSave/RplLoad, CONFIG_STREAM_VERSION unchanged.
	bool despawnCiviliansDuringQRF;

	void SetDefaults()
	{
		discordWebHookURL = "see wiki: https://github.com/ArmaOverthrow/Overthrow.Arma4/wiki/Discord-Web-Hook";
		occupyingFaction = "";
		supportingFaction = "";
		officers = new array<string>;
		difficulty = "";	
		showPlayerPosition = true;	
		mobileFOBOfficersOnly = true; // Default: restrict Mobile FOB deployment to officers only
		
		houseItemLimit = 20;
		campItemLimit = 40;
		fobItemLimit = 100;
		
		overrideDifficulty = false;
		startingCash = 100;
		gunDealerSellPriceMultiplier = 0.5;
		procurementMultiplier = 0.8;
		vehiclePriceMultiplier = 1.0;
		fuelPricePerLitre = 1.0;
		recruitLoadoutFeeMultiplier = OVT_RecruitPurchaseRules.DEFAULT_LOADOUT_FEE_MULTIPLIER;
		virtualizationSpawnDistance = 1750;
		civilianDensityMultiplier = 1.0;
		maxCiviliansPerTown = 30;
		despawnCiviliansDuringQRF = false;
	}
}

class OVT_OverthrowConfigComponent: OVT_Component
{
	[Attribute("$profile:Overthrow_Config.json")]
	string m_sConfigFilePath;

	ref OVT_OverthrowConfigStruct m_ConfigFile;

	[Attribute( defvalue: "FIA", uiwidget: UIWidgets.EditBox, desc: "Faction affiliation of the player's side", category: "Factions")]
	string m_sPlayerFaction;

	[Attribute( defvalue: "USSR", uiwidget: UIWidgets.EditBox, desc: "The faction occupying this map (the enemy)", category: "Factions")]
	string m_sDefaultOccupyingFaction;

	string m_sOccupyingFaction = "USSR";

	[Attribute( defvalue: "US", uiwidget: UIWidgets.EditBox, desc: "The faction supporting the player faction", category: "Factions")]
	string m_sDefaultSupportingFaction;

	string m_sSupportingFaction = "US";

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Town Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pTownControllerPrefab;

	[Attribute("", UIWidgets.Object)]
	ref OVT_DifficultySettings m_Difficulty;

	//! WARNING: world-layer overrides of this array APPEND to the prefab's presets rather than
	//! replacing them (engine semantics) — e.g. the test world runs 5 presets with 'Test World'
	//! at index 4. Always select a preset by its name, never by index (see OVT_TEST_SuiteBase).
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_DifficultySettings> m_aDifficultyPresets;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Gun Dealer Prefab", params: "et")]
	ResourceName m_pGunDealerPrefab;

	// RETIRED 2026-08-17 (virtualization/civilians Phase 2): the civilian group prefab moved into
	// Configs/Civilians/CivilianAmbience.conf, where each authored civilian TYPE names its own
	// one-man group - one global prefab could never describe a pool.

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Move Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pMoveWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Defend Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pDefendWaypointPrefab;
	//Chris Added wps
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Defend Base Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pDefendBaseWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Loiter Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pLoiterWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Get In Basic Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pGetInWaypointBPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Get Out Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pGetOutWaypointBPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Patrol Basic Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pPatrolBasicWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Scout Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pScoutWaypointPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Patrol Waypoint Prefab", params: "et", category: "Waypoints")]
	ResourceName m_pPatrolWaypointPrefab;

	//--------------------
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

	// RETIRED 2026-08-17 (virtualization/civilians Phase 2): the civilian density rate and the civilian
	// spawn distance both moved off this component. Density is authored per source in
	// Configs/Civilians/CivilianAmbience.conf (m_fPopulationRate) and scaled at runtime by the
	// operator's civilianDensityMultiplier / maxCiviliansPerTown; the spawn distance is now the
	// ambient source's own m_iSpawnDistanceOverride, which rides virtualizationSpawnDistance by
	// default. Nothing read either of them once the town-controller spawner was retired.

	// RETIRED 2026-08-18 (virtualization/base-defense-migration T7.7): the military spawn distance
	// attribute is gone. Its last reader was the base-upgrade patrol proximity gate, which was deleted
	// with the whole base-upgrade system, and it was authored in no prefab, config or world. Every
	// systemic force is virtualized now and rides virtualizationSpawnDistance (or a per-registration
	// override) instead. It was never in RplSave/RplLoad, so CONFIG_STREAM_VERSION does not move.

	[Attribute(defvalue: "0.1", UIWidgets.EditBox, desc: "NPC Shop Buy Rate", category: "Economy")]
	float m_fNPCBuyRate;

	[Attribute(defvalue: "0.25", UIWidgets.EditBox, desc: "Shop Profit Margin", category: "Economy")]
	float m_fShopProfitMargin;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Player Group Prefab", params: "et")]
	ResourceName m_pPlayerGroupPrefab;

	[Attribute( uiwidget: UIWidgets.Object, desc: "Civilian Loadout" )]
	ref OVT_LoadoutConfig m_CivilianLoadout;

	int m_iOccupyingFactionIndex = -1;
	int m_iSupportingFactionIndex = -1;
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

	bool LoadConfig()
	{
		Print("[Overthrow] Trying to load configuration file "+m_sConfigFilePath, LogLevel.NORMAL);
		
		m_ConfigFile = new OVT_OverthrowConfigStruct();
		m_ConfigFile.SetDefaults();

#ifdef PLATFORM_CONSOLE
		return true;
#endif

		// Overthrow_Config.json is a dedicated-server config. Single player and listen hosts are
		// configured through the start menu, so the file is neither read nor created there - a
		// leftover json from running a dedicated server must not leak into a hosted game.
		if (RplSession.Mode() != RplMode.Dedicated)
			return true;

		JsonLoadContext configLoadContext = new JsonLoadContext();

		if (!FileIO.FileExists( m_sConfigFilePath ))
		{
			Print("[Overthrow] Configuration file does not exist. Default will be created.", LogLevel.WARNING);
			SaveConfig();			
			return true;
		};

		if (!configLoadContext.LoadFromFile( m_sConfigFilePath ))
		{
			Print("[Overthrow] Configuration load failed, unable to read from disk", LogLevel.ERROR);
			return false;
		};

		if (!configLoadContext.ReadValue("", m_ConfigFile))
		{
			Print("[Overthrow] Configuration load failed, incorrect format", LogLevel.ERROR);
			return false;
		};

		return true;
	};

	bool SaveConfig()
	{
#ifdef PLATFORM_CONSOLE
		return true;
#endif
		JsonSaveContext configSaveContext = new JsonSaveContext();
		configSaveContext.WriteValue("", m_ConfigFile);

		if (!configSaveContext.SaveToFile( m_sConfigFilePath ))
		{
			Print("Overthrow: Saving config file failed!", LogLevel.ERROR);
			return false;
		};

		return true;
	};

	int GetPlaceableCost(OVT_Placeable placeable)
	{
		return Math.Round(m_Difficulty.placeableCostMultiplier * placeable.m_iCost);
	}

	int GetBuildableCost(OVT_Buildable buildable)
	{
		return Math.Round(m_Difficulty.buildableCostMultiplier * buildable.m_iCost);
	}

	//------------------------------------------------------------------------------------------------
	//! Scales one authored resource requirement of a buildable to this difficulty. The single call
	//! behind both the displayed figure and the consumed one.
	//! \param[in] baseQty The quantity buildables.conf authors.
	//! \return The scaled quantity; never zero unless baseQty was.
	int GetBuildableResourceCost(int baseQty)
	{
		return OVT_ResourceRules.ScaleRequirement(baseQty, m_Difficulty.buildableResourceCostMultiplier);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The multiplier applied to a stored resource price at read time, after the band clamp.
	float GetResourcePriceMultiplier()
	{
		return m_Difficulty.resourcePriceMultiplier;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The multiplier on one price drift step. Never touches the drift band.
	float GetResourcePriceVolatility()
	{
		return m_Difficulty.resourcePriceVolatility;
	}

	int GetHouseItemLimit()
	{
		return m_ConfigFile.houseItemLimit;
	}
	
	int GetCampItemLimit()
	{
		return m_ConfigFile.campItemLimit;
	}
	
	int GetFOBItemLimit()
	{
		return m_ConfigFile.fobItemLimit;
	}

	void SetOccupyingFaction(string key)
	{
		OVT_Faction of = GetOccupyingFaction();
		if(of && key == of.GetFactionKey()) return;
		FactionManager factionMgr = GetGame().GetFactionManager();
		Faction faction = factionMgr.GetFactionByKey(key);
		if(!faction)
		{
			Print("[Overthrow] Unknown occupying faction key '" + key + "' (check Overthrow_Config.json), falling back to default '" + m_sDefaultOccupyingFaction + "'", LogLevel.ERROR);
			if(key == m_sDefaultOccupyingFaction) return;
			SetOccupyingFaction(m_sDefaultOccupyingFaction);
			return;
		}
		m_iOccupyingFactionIndex = factionMgr.GetFactionIndex(faction);

		m_sOccupyingFaction = faction.GetFactionKey();
	}

	void SetSupportingFaction(string key)
	{
		OVT_Faction sf = GetSupportingFaction();
		if(sf && key == sf.GetFactionKey()) return;
		FactionManager factionMgr = GetGame().GetFactionManager();
		Faction faction = factionMgr.GetFactionByKey(key);
		if(!faction)
		{
			Print("[Overthrow] Unknown supporting faction key '" + key + "' (check Overthrow_Config.json), falling back to default '" + m_sDefaultSupportingFaction + "'", LogLevel.ERROR);
			if(key == m_sDefaultSupportingFaction) return;
			SetSupportingFaction(m_sDefaultSupportingFaction);
			return;
		}
		m_iSupportingFactionIndex = factionMgr.GetFactionIndex(faction);

		m_sSupportingFaction = faction.GetFactionKey();
	}

	void SetBaseAndTownOwners()
	{
		foreach(OVT_BaseData base : OVT_Global.GetOccupyingFaction().m_Bases)
		{
			base.faction = m_iOccupyingFactionIndex;
		}

		foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
		{
			town.faction = m_iOccupyingFactionIndex;
		}

		foreach(OVT_RadioTowerData tower : OVT_Global.GetOccupyingFaction().m_RadioTowers)
		{
			tower.faction = m_iOccupyingFactionIndex;
		}
	}

	//------------------------------------------------------------------------------------------------
	// THE FACTION ACCESSORS
	//
	// ⚠ THE FACTION MANAGER MAY NOT EXIST YET, AND THAT IS NOT A FAULT. A component's OnPostInit() runs
	// in the WORLD EDITOR, before anybody presses play: the game mode entity's components ARE
	// constructed, but GetGame().GetFactionManager() is null because no game has started. Anything
	// asked of these accessors in that context has exactly one honest answer - "cannot tell yet" - and
	// it is null from the three data accessors and -1 from the three index accessors. Before
	// 2026-08-19 all six dereferenced the manager unguarded and the first caller to reach one from an
	// OnPostInit() took the whole editor down with a NULL-pointer VM exception.
	//
	// ⚠ AND -1 IS NEVER WRITTEN INTO THE CACHE. -1 is this component's NOT-YET-RESOLVED sentinel - it
	// is what the members are born holding and what OVT_OccupyingFactionManager deliberately resets
	// them to when a campaign restarts - so caching it as an ANSWER would make one unlucky early call
	// permanent: the index would still read -1 for the rest of the session, long after the faction
	// manager existed. Each guard returns WITHOUT touching the member, so the real index is still
	// resolved by the first call that can see a faction manager.
	//------------------------------------------------------------------------------------------------

	OVT_Faction GetOccupyingFaction()
	{
		return OVT_Global.GetFactions().GetOverthrowFactionByKey(m_sOccupyingFaction);
	}

	Faction GetOccupyingFactionData()
	{
		FactionManager fm = GetGame().GetFactionManager();
		if(!fm) return null;

		return fm.GetFactionByKey(m_sOccupyingFaction);
	}

	int GetOccupyingFactionIndex()
	{
		if(m_iOccupyingFactionIndex == -1)
		{
			FactionManager fm = GetGame().GetFactionManager();
			if(!fm) return -1;

			m_iOccupyingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_sOccupyingFaction));
		}
		return m_iOccupyingFactionIndex;
	}

	OVT_Faction GetSupportingFaction()
	{
		return OVT_Global.GetFactions().GetOverthrowFactionByKey(m_sSupportingFaction);
	}

	Faction GetSupportingFactionData()
	{
		FactionManager fm = GetGame().GetFactionManager();
		if(!fm) return null;

		return fm.GetFactionByKey(m_sSupportingFaction);
	}

	int GetSupportingFactionIndex()
	{
		if(m_iSupportingFactionIndex == -1)
		{
			FactionManager fm = GetGame().GetFactionManager();
			if(!fm) return -1;

			m_iSupportingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_sSupportingFaction));
		}
		return m_iSupportingFactionIndex;
	}

	OVT_Faction GetPlayerFaction()
	{
		return OVT_Global.GetFactions().GetOverthrowFactionByKey(m_sPlayerFaction);
	}

	Faction GetPlayerFactionData()
	{
		FactionManager fm = GetGame().GetFactionManager();
		if(!fm) return null;

		return fm.GetFactionByKey(m_sPlayerFaction);
	}

	int GetPlayerFactionIndex()
	{
		if(m_iPlayerFactionIndex == -1)
		{
			FactionManager fm = GetGame().GetFactionManager();
			if(!fm) return -1;

			m_iPlayerFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_sPlayerFaction));
		}
		return m_iPlayerFactionIndex;
	}

	OVT_Faction GetFactionByType(OVT_FactionType type)
	{
		switch(type)
		{
			case OVT_FactionType.OCCUPYING_FACTION:
				return GetOccupyingFaction();
			case OVT_FactionType.SUPPORTING_FACTION:
				return GetSupportingFaction();
		}
		return GetPlayerFaction();
	}

	AIWaypoint SpawnWaypoint(ResourceName res, vector pos)
	{
		AIWaypoint wp = AIWaypoint.Cast(OVT_Global.SpawnEntityPrefab(res, pos));
		return wp;
	}

	AIWaypoint SpawnPatrolWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pPatrolWaypointPrefab, pos);
		return wp;
	}
	
	AIWaypoint SpawnMoveWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pMoveWaypointPrefab, pos);
		return wp;
	}

	AIWaypoint SpawnSearchAndDestroyWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pSearchAndDestroyWaypointPrefab, pos);
		return wp;
	}
	//Chris Added Wps
	AIWaypoint SpawnDefendBaseWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pDefendBaseWaypointPrefab, pos);
		return wp;
	}
	
	AIWaypoint SpawnGetInWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pGetOutWaypointBPrefab, pos);
		return wp;
	}
	
	AIWaypoint SpawnGetOutWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pGetOutWaypointBPrefab, pos);
		return wp;
	}
	
	AIWaypoint SpawnLoiterWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pLoiterWaypointPrefab, pos);
		return wp;
	}

	AIWaypoint SpawnBasicPatrolWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pPatrolBasicWaypointPrefab, pos);
		return wp;
	}
	
		AIWaypoint SpawnScoutWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pScoutWaypointPrefab, pos);
		return wp;
	}
	
	AIWaypoint SpawnBasicCycleWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_pCycleWaypointPrefab, pos);
		return wp;
	}
	
	//------------------------------------------------
	AIWaypoint SpawnDefendWaypoint(vector pos, int preset = 0)
	{
		AIWaypoint wp = SpawnWaypoint(m_pDefendWaypointPrefab, pos);
		SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(wp);
		defend.SetCurrentDefendPreset(preset);
		return wp;
	}

	SCR_EntityWaypoint SpawnGetInWaypoint(IEntity target)
	{
		SCR_EntityWaypoint wp = SCR_EntityWaypoint.Cast(OVT_Global.SpawnEntityPrefab(m_pGetInWaypointPrefab, target.GetOrigin()));

		wp.SetEntity(target);

		return wp;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a wait waypoint that actually waits for the duration asked for.
	//!
	//! ⚠ THE DURATION USED TO BE DROPPED ON THE FLOOR (defect F-C). Every caller since this method was
	//! written has passed a time that was never applied, so every "wait" waypoint in Overthrow held for
	//! whatever AIWaypoint_Wait.et authors (60 s) instead. SetHoldingTime() is the missing call.
	//!
	//! SetHoldingTime() itself silently does nothing when the prefab does not author
	//! m_TimedWaypointParameters (SCR_TimedWaypoint.SetHoldingTime guards on it). The vanilla
	//! Prefabs/AI/Waypoints/AIWaypoint_Wait.et this component is bound to DOES author that object
	//! (m_holdingTime 60), verified 2026-08-17 against the 1.8.0.10 reference tree - so if a future
	//! edit points m_pWaitWaypointPrefab at a different prefab, check that prefab before trusting the
	//! duration.
	//! \param[in] pos Where the waypoint goes.
	//! \param[in] time Seconds to hold there.
	//! \return The waypoint, or null when the prefab failed to spawn or is not a timed waypoint.
	SCR_TimedWaypoint SpawnWaitWaypoint(vector pos, float time)
	{
		SCR_TimedWaypoint wp = SCR_TimedWaypoint.Cast(OVT_Global.SpawnEntityPrefab(m_pWaitWaypointPrefab, pos));

		// Overrides the prefab's own m_holdingTime (60 s in vanilla's AIWaypoint_Wait.et). This
		// call was missing for years, so every caller's time was silently ignored in favour of
		// that 60 s default.
		if (wp)
			wp.SetHoldingTime(time);

		return wp;
	}

	SCR_SmartActionWaypoint SpawnActionWaypoint(vector pos, IEntity target, string action)
	{
		SCR_SmartActionWaypoint wp = SCR_SmartActionWaypoint.Cast(OVT_Global.SpawnEntityPrefab(m_pSmartActionWaypointPrefab, pos));

		wp.SetSmartActionEntity(target, action);

		return wp;
	}

	void GivePatrolWaypoints(SCR_AIGroup aigroup, OVT_PatrolType type, vector center = "0 0 0", float radius = 0)
	{
		if(center[0] == 0) center = aigroup.GetOrigin();

		if(type == OVT_PatrolType.DEFEND)
		{
			aigroup.AddWaypoint(SpawnDefendWaypoint(center));
			return;
		}

		// PERIMETER_BASE is handled HERE AS AN ORDINARY PERIMETER, deliberately. This is the LEGACY
		// hand-authored waypoint path and its only caller is OVT_SpawnGroupJobStage, which spawns a
		// group at a JOB location - there is no base controller to read an authored perimeter square
		// off, so the road-snapped ring is the only thing this path can build. Falling through instead
		// would give a job group NO waypoints at all and it would stand still, which is worse than the
		// wrong shape. No shipped job authors PERIMETER_BASE (the member did not exist until 2026-08-18),
		// so nothing's behaviour changes today.
		if(type == OVT_PatrolType.PERIMETER || type == OVT_PatrolType.PERIMETER_BASE)
		{
			float dist = radius;
			if(radius == 0)
			{
				dist = vector.Distance(aigroup.GetOrigin(), center);
			}
			vector dir = vector.Direction(aigroup.GetOrigin(), center);
			float angle = dir.VectorToAngles()[1];

			array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
			AIWaypoint firstWP;
			for(int i=0; i< 4; i++)
			{
				vector pos = center + (Vector(0,angle,0).AnglesToVector() * dist);
				vector roadPos = OVT_WorldUtils.FindNearestRoad(pos);

				AIWaypoint wp = SpawnPatrolWaypoint(roadPos);
				queueOfWaypoints.Insert(wp);

				AIWaypoint wait = SpawnWaitWaypoint(roadPos, s_AIRandomGenerator.RandFloatXY(45, 75));
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
	
	//RPC Methods

	//! Version stamp for the hand-rolled JIP config bitstream below. RplSave and RplLoad are
	//! positional: every field must be written and read in exactly the same order. Keep the two
	//! functions adjacent and edit them together — and bump this constant whenever a field is
	//! added, removed or reordered, so a mismatched client fails loudly at connect instead of
	//! silently reading shifted garbage (BUG-078).
	//!
	//! Version 2 appended radioTowerRange and baseSupportRange to the difficulty block. Both are
	//! read client-side by the map influence overlay, which draws a source's reach and decides
	//! which locations are in range of it. Without them a client falls back to whatever difficulty
	//! preset its game-mode prefab instantiated m_Difficulty from, which agrees with the server at
	//! exactly one difficulty and silently invents or omits influence edges at every other.
	//!
	//! Version 3 appended allowFOBDuringQRF to the difficulty block. It is read client-side by
	//! ValidateTravel (the travel button's enable state) and OVT_MapLocationFOB.CanRespawn (the
	//! respawn marker) — without it a client whose server disabled the FOB exemption would show an
	//! enabled button the server then refuses.
	//!
	//! Version 4 appended fuelPricePerLitre to the difficulty block. It is read client-side by the
	//! Refuel action's price suffix and by the local affordability gate that greys that action out —
	//! without it a client would draw its own preset's price and refuse (or offer) a refuel the
	//! server prices differently, which is the gate disagreeing with the authority, not a cosmetic
	//! difference.
	//!
	//! Version 5 appended repairCostMultiplier to the difficulty block, for the same reason version 4
	//! appended the fuel price: the repair action draws the price in its own label and greys itself out
	//! locally when the player cannot pay it. Without it a client would price a repair from whatever
	//! preset its game-mode prefab happened to instantiate, and offer (or refuse) a repair the server
	//! charges differently for.
	//!
	//! Version 6 appended the three resource multipliers to the difficulty block. The port's Resources
	//! tab prices every row client-side from the replicated stored price times resourcePriceMultiplier,
	//! and a construction site draws its requirement rows through buildableResourceCostMultiplier —
	//! both are quoted locally and re-derived by the server, so a client running its own preset's
	//! values would show a price it is not charged and a requirement it does not owe.
	//! resourcePriceVolatility rides with them: it is server-only today (only the drift tick reads it),
	//! but splitting one difficulty concept across two transports invites the next reader to assume it
	//! is there.
	protected const int CONFIG_STREAM_VERSION = 6;

	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(CONFIG_STREAM_VERSION);

		//Send needed difficulty items
		writer.WriteBool(m_Difficulty.showPlayerOnMap);
		writer.WriteInt(m_Difficulty.wantedTimeout);
		writer.WriteInt(m_Difficulty.wantedOneTimeout);
		writer.WriteFloat(m_Difficulty.placeableCostMultiplier);
		writer.WriteFloat(m_Difficulty.buildableCostMultiplier);
		writer.WriteFloat(m_Difficulty.realEstateCostMultiplier);
		writer.WriteInt(m_Difficulty.busTicketPrice);
		writer.WriteInt(m_Difficulty.baseRecruitCost);
		writer.WriteFloat(m_Difficulty.gunDealerSellPriceMultiplier);
		writer.WriteFloat(m_Difficulty.procurementMultiplier);
		writer.WriteFloat(m_Difficulty.vehiclePriceMultiplier);
		writer.WriteString(m_Difficulty.name);
		writer.WriteFloat(m_Difficulty.minFastTravelDistance);
		writer.WriteInt(m_Difficulty.QRFFastTravelMode);
		writer.WriteFloat(m_Difficulty.baseRange);
		writer.WriteFloat(m_Difficulty.baseCloseRange);
		writer.WriteInt(m_Difficulty.fastTravelCost);
		writer.WriteInt(m_Difficulty.QRFPointsToWin);
		writer.WriteFloat(m_Difficulty.disguiseDetectionDistance);
		writer.WriteFloat(m_Difficulty.radioTowerRange);
		writer.WriteFloat(m_Difficulty.baseSupportRange);
		writer.WriteBool(m_Difficulty.allowFOBDuringQRF);
		writer.WriteFloat(m_Difficulty.fuelPricePerLitre);
		writer.WriteFloat(m_Difficulty.repairCostMultiplier);
		writer.WriteFloat(m_Difficulty.buildableResourceCostMultiplier);
		writer.WriteFloat(m_Difficulty.resourcePriceMultiplier);
		writer.WriteFloat(m_Difficulty.resourcePriceVolatility);

		//Send server config options
		writer.WriteBool(m_ConfigFile.mobileFOBOfficersOnly);	
		writer.WriteInt(m_ConfigFile.houseItemLimit);
		writer.WriteInt(m_ConfigFile.campItemLimit);
		writer.WriteInt(m_ConfigFile.fobItemLimit);
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{		
				
		//Recieve difficulty items
		int i;
		float f;
		bool b;

		if (!reader.ReadInt(i)) return false;
		if (i != CONFIG_STREAM_VERSION)
		{
			Print(string.Format("[Overthrow] Config JIP stream version mismatch (got %1, expected %2) — client and server are running different Overthrow versions", i, CONFIG_STREAM_VERSION), LogLevel.ERROR);
			return false;
		}

		if (!reader.ReadBool(b)) return false;
		m_Difficulty.showPlayerOnMap = b;
		
		if (!reader.ReadInt(i)) return false;
		m_Difficulty.wantedTimeout = i;
		
		if (!reader.ReadInt(i)) return false;
		m_Difficulty.wantedOneTimeout = i;
		
		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.placeableCostMultiplier = f;
		
		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.buildableCostMultiplier = f;
		
		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.realEstateCostMultiplier = f;
		
		if (!reader.ReadInt(i)) return false;
		m_Difficulty.busTicketPrice = i;
		
		if (!reader.ReadInt(i)) return false;
		m_Difficulty.baseRecruitCost = i;
		
		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.gunDealerSellPriceMultiplier = f;
		
		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.procurementMultiplier = f;
		
		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.vehiclePriceMultiplier = f;

		string s;
		if (!reader.ReadString(s)) return false;
		m_Difficulty.name = s;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.minFastTravelDistance = f;

		if (!reader.ReadInt(i)) return false;
		m_Difficulty.QRFFastTravelMode = i;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.baseRange = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.baseCloseRange = f;

		if (!reader.ReadInt(i)) return false;
		m_Difficulty.fastTravelCost = i;

		if (!reader.ReadInt(i)) return false;
		m_Difficulty.QRFPointsToWin = i;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.disguiseDetectionDistance = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.radioTowerRange = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.baseSupportRange = f;

		if (!reader.ReadBool(b)) return false;
		m_Difficulty.allowFOBDuringQRF = b;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.fuelPricePerLitre = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.repairCostMultiplier = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.buildableResourceCostMultiplier = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.resourcePriceMultiplier = f;

		if (!reader.ReadFloat(f)) return false;
		m_Difficulty.resourcePriceVolatility = f;

		//Receive server config options
		if (!reader.ReadBool(b)) return false;
		
		// Create config file structure if it doesn't exist (for clients)
		if (!m_ConfigFile)
		{
			m_ConfigFile = new OVT_OverthrowConfigStruct();
			m_ConfigFile.SetDefaults();
		}
		
		m_ConfigFile.mobileFOBOfficersOnly = b;
		
		if (!reader.ReadInt(i)) return false;
		m_ConfigFile.houseItemLimit = i;
		
		if (!reader.ReadInt(i)) return false;
		m_ConfigFile.campItemLimit = i;
		
		if (!reader.ReadInt(i)) return false;
		m_ConfigFile.fobItemLimit = i;
		
		return true;
	}
}
