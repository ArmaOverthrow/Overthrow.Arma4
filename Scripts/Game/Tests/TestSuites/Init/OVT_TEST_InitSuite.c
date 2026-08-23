//------------------------------------------------------------------------------------------------
//! TIER B - initialisation / integration.
//!
//! World loaded, Overthrow game mode and its managers alive, campaign deliberately NOT started
//! (RequiresStartedCampaign() stays false). Guards the "everything is null on startup" class of
//! breakage: a manager dropped from the game-mode prefab, a renamed OVT_Global getter, a town
//! controller that no longer registers.
//!
//! Belongs here: anything needing live managers but no running campaign. Not here: pure maths
//! (Tier A, OVT_TEST_LogicSuite) and anything needing a started campaign (Tier C/D).
//!
//! The test world defines exactly ONE town and ONE base, so count assertions are ">= 1".
//!
//! ⚠ [BaseContainerProps()] is MANDATORY on a concrete suite class: without it a SCR_AutotestGroup
//! config silently instantiates nothing (`Unknown class`, empty <testsuites>, exit 2).
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TEST_InitSuite : OVT_TEST_SuiteBase
{
}

//------------------------------------------------------------------------------------------------
//! Every OVT_Global manager getter that does not need a local player returns a live object.
//!
//! Deliberately EXCLUDES GetUI(), GetController() and every controller component (reached with
//! OVT_ControllerComponent<T>.Get()): they all depend on a LOCAL PLAYER, which is a different
//! question from "is the game mode carrying its managers".
//! The controller seam has its own case - OVT_TEST_Init_Controller_ComponentsResolve.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Globals_ManagersResolve : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string firstNull = FindFirstNullGetter();

		if (firstNull != "")
		{
			SetFailure("OVT_Global getter returned null: %1 - the game mode is missing that manager component", firstNull);
			return true;
		}

		Print("Every non-player OVT_Global getter resolved");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks each getter in turn and names the FIRST one that came back null.
	//! Ordered so that a getter is only called once everything it dereferences has been proven
	//! non-null (GetDifficulty() reads GetConfig().m_Difficulty, so GetConfig() is checked first).
	//! \return Name of the first null getter, or an empty string when all of them resolved.
	protected string FindFirstNullGetter()
	{
		if (!OVT_Global.GetOverthrow()) return "GetOverthrow()";
		if (!OVT_Global.GetConfig()) return "GetConfig()";
		if (!OVT_Global.GetDifficulty()) return "GetDifficulty()";
		if (!OVT_Global.GetEconomy()) return "GetEconomy()";
		if (!OVT_Global.GetPlayers()) return "GetPlayers()";
		if (!OVT_Global.GetRealEstate()) return "GetRealEstate()";
		if (!OVT_Global.GetVehicles()) return "GetVehicles()";
		if (!OVT_Global.GetTowns()) return "GetTowns()";
		if (!OVT_Global.GetOccupyingFaction()) return "GetOccupyingFaction()";
		if (!OVT_Global.GetResistanceFaction()) return "GetResistanceFaction()";
		if (!OVT_Global.GetJobs()) return "GetJobs()";
		if (!OVT_Global.GetNotify()) return "GetNotify()";
		if (!OVT_Global.GetFactions()) return "GetFactions()";
		if (!OVT_Global.GetSkills()) return "GetSkills()";
		if (!OVT_Global.GetInventory()) return "GetInventory()";
		if (!OVT_Global.GetDeploymentManager()) return "GetDeploymentManager()";
		if (!OVT_Global.GetRecruits()) return "GetRecruits()";
		if (!OVT_Global.GetLoadouts()) return "GetLoadouts()";
		if (!OVT_Global.GetMapMarkers()) return "GetMapMarkers()";
		if (!OVT_Global.GetTutorialManager()) return "GetTutorialManager()";
		if (!OVT_Global.GetVirtualization()) return "GetVirtualization()";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The town manager found the world's towns during Init and populated them.
//!
//! Covers the seam that breaks silently: town discovery runs off a world query in
//! OVT_TownManagerComponent.Init(), so a prefab or filter change can leave m_Towns empty, or leave
//! towns with no population, long before anything visibly misbehaves.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Towns_ArePopulated : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList)
		{
			SetFailure("OVT_TownManagerComponent.GetTowns() returned a null array");
			return true;
		}

		// >= 1, never a magic count - the test world has exactly one town.
		if (townList.Count() < 1)
		{
			SetFailure("No towns registered: OVT_TownManagerComponent.GetTowns().Count() = %1", townList.Count().ToString());
			return true;
		}

		OVT_TownData town = townList[0];
		if (!town)
		{
			SetFailure("Town 0 is null");
			return true;
		}

		if (town.population <= 0)
		{
			SetFailure("Town 0 has no population: %1", town.population.ToString());
			return true;
		}

		if (town.location == vector.Zero)
		{
			SetFailure("Town 0 has no location (vector.Zero)");
			return true;
		}

		PrintFormat("Towns registered: %1, town 0 population %2 at %3",
			townList.Count().ToString(), town.population.ToString(), town.location.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town and base controllers placed in the world are registered with their managers, and a town
//! controller resolves back to an entry in m_Towns.
//!
//! Both registrations happen during Init (world load), NOT at campaign start: towns via
//! OVT_TownManagerComponent.FilterTownControllerEntities, bases via
//! OVT_OccupyingFactionManager.InitializeBases. That is exactly why this case belongs in Tier B.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Controllers_AreRegistered : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		if (towns.m_TownControllers.Count() < 1)
		{
			SetFailure("No town controllers registered: m_TownControllers.Count() = %1", towns.m_TownControllers.Count().ToString());
			return true;
		}

		IEntity townEntity = GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]);
		if (!townEntity)
		{
			SetFailure("Town controller 0 is registered but its entity ID no longer resolves in the world");
			return true;
		}

		OVT_TownControllerComponent townController = OVT_TownControllerComponent.Cast(townEntity.FindComponent(OVT_TownControllerComponent));
		if (!townController)
		{
			SetFailure("Registered town controller entity has no OVT_TownControllerComponent");
			return true;
		}

		// The controller must resolve back into the manager's own town list.
		OVT_TownData town = towns.GetNearestTown(townEntity.GetOrigin());
		if (!town)
		{
			SetFailure("GetNearestTown() found no town at the town controller's position %1", townEntity.GetOrigin().ToString());
			return true;
		}

		int townId = towns.GetTownID(town);
		if (townId < 0)
		{
			SetFailure("The town at the town controller's position is not in m_Towns (GetTownID returned %1)", townId.ToString());
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		if (occupying.m_Bases.Count() < 1)
		{
			SetFailure("No bases registered: m_Bases.Count() = %1", occupying.m_Bases.Count().ToString());
			return true;
		}

		OVT_BaseControllerComponent baseController = occupying.GetBaseByIndex(0);
		if (!baseController)
		{
			SetFailure("Base 0 is registered but GetBaseByIndex(0) resolved no OVT_BaseControllerComponent");
			return true;
		}

		PrintFormat("Town controllers: %1, controller '%2' resolves to town id %3",
			towns.m_TownControllers.Count().ToString(), townController.m_sName, townId.ToString());
		PrintFormat("Bases registered: %1", occupying.m_Bases.Count().ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownManagerComponent.GetNearestTownInRange() returns the NEAREST in-range town, not the
//! first in array order (BUG-062 regression).
//!
//! Two synthetic villages with overlapping radii, deliberately ordered so the FARTHER one comes
//! first. Both sit tens of kilometres from the test world's real town and are removed from m_Towns
//! again before any assertion returns, keeping the index-aligned town list intact for later cases.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Towns_GetNearestTownInRange_ReturnsNearest : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		float range = towns.m_iVillageRange;
		vector probe = "40000 0 40000";

		// Inserted FIRST, sits FARTHER from the probe (80% of the village range).
		OVT_TownData farTown = new OVT_TownData();
		farTown.location = probe + Vector(range * 0.8, 0, 0);
		farTown.size = OVT_TownSize.VILLAGE;

		// Inserted SECOND, sits NEARER to the probe (40% of the village range).
		OVT_TownData nearTown = new OVT_TownData();
		nearTown.location = probe + Vector(0, 0, range * 0.4);
		nearTown.size = OVT_TownSize.VILLAGE;

		towns.m_Towns.Insert(farTown);
		towns.m_Towns.Insert(nearTown);

		OVT_TownData overlapResult = towns.GetNearestTownInRange(probe);
		OVT_TownData outOfRangeResult = towns.GetNearestTownInRange(probe + Vector(0, 0, range * 10));

		towns.m_Towns.RemoveItem(nearTown);
		towns.m_Towns.RemoveItem(farTown);

		if (overlapResult == farTown)
		{
			SetFailure("GetNearestTownInRange() returned the farther town because it precedes the nearer one in m_Towns (BUG-062 first-match regression)");
			return true;
		}

		if (overlapResult != nearTown)
		{
			SetFailure("GetNearestTownInRange() did not return the nearer of two overlapping in-range towns");
			return true;
		}

		if (outOfRangeResult)
		{
			SetFailure("GetNearestTownInRange() returned a town for a probe outside every town's range");
			return true;
		}

		PrintFormat("GetNearestTownInRange: nearest of 2 overlapping villages returned, out-of-range probe returned null (village range %1)", range.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Reforger's vanilla persistence system is registered for the Overthrow world and did not fail
//! its own setup.
//!
//! Two failure modes, one case, because they are the same wiring: GetScriptedInstance() null means
//! ChimeraSystemsConfig.conf lost its SCR_PersistenceSystem entry; state FAILURE means
//! Persistence/Overthrow.conf could not be loaded. Hand-authored conf GUIDs fail SILENTLY in the
//! Workbench, so this is the only automated tripwire for that class of typo.
//!
//! ACTIVE rather than "not FAILURE" because INIT/SETUP are world-load states and Main steps only
//! run after the world load completed. If setup ever turns asynchronous, loosen this deliberately
//! and record why. Never add retries.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_SystemIsOnline : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - the persistence system is not registered for this world. Check the SCR_PersistenceSystem entry in Configs/Systems/ChimeraSystemsConfig.conf.");
			return true;
		}

		EPersistenceSystemState state = persistence.GetState();

		if (state == EPersistenceSystemState.FAILURE)
		{
			SetFailure("Persistence system state is FAILURE - its config could not be loaded. Check Configs/Systems/Persistence/Overthrow.conf and the GUID it inherits.");
			return true;
		}

		if (state != EPersistenceSystemState.ACTIVE)
		{
			SetFailure("Persistence system state is %1, expected ACTIVE. INIT and SETUP mean the world load has not finished setting persistence up.",
				typename.EnumToString(EPersistenceSystemState, state));
			return true;
		}

		PrintFormat("Persistence system online, state %1", typename.EnumToString(EPersistenceSystemState, state));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The persistence system that is online is running OVERTHROW's persistence config, not somebody
//! else's.
//!
//! Separate from the case above deliberately. Several vanilla SystemSettings configs register an
//! SCR_PersistenceSystem of their own pointing at EditableMission.conf; if a world's chain resolves
//! one of those, the case above still passes while every Overthrow collection and serializer
//! binding silently does not exist. The probe is the "Overthrow" collection Overthrow.conf adds on
//! top of vanilla Common.conf.
//!
//! Red here with the case above green means the SystemSettings chain is what has to change.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_OverthrowConfigLoaded : SCR_AutotestCaseBase
{
	//! Display name of the collection declared in Configs/Systems/Persistence/Overthrow.conf.
	static const string OVERTHROW_COLLECTION = "Overthrow";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - see OVT_TEST_Init_Persistence_SystemIsOnline for the wiring this depends on.");
			return true;
		}

		// Sanity anchor: an inherited vanilla collection must resolve, otherwise a null result below
		// would only prove that collection lookup itself is broken.
		PersistenceCollection vanillaCollection = persistence.FindCollection("Character");
		if (!vanillaCollection)
		{
			SetFailure("FindCollection('Character') is null - the loaded persistence config does not even contain vanilla Common.conf's collections, so no config comparison is meaningful.");
			return true;
		}

		PersistenceCollection overthrowCollection = persistence.FindCollection(OVERTHROW_COLLECTION);
		if (!overthrowCollection)
		{
			SetFailure("FindCollection('%1') is null - the live persistence system is NOT running Configs/Systems/Persistence/Overthrow.conf. Check that the SCR_PersistenceSystem entry in Configs/Systems/ChimeraSystemsConfig.conf is the one this world's SystemSettings chain resolves.", OVERTHROW_COLLECTION);
			return true;
		}

		PrintFormat("Overthrow persistence config in force (collection '%1' resolved)", OVERTHROW_COLLECTION);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The economy's price and demand seams answer their own public setters, and GetBuyPrice() applies
//! the configured shop profit margin on top of the base price.
//!
//! Uses synthetic resource IDs far outside m_aResources' range, so no real item's price is
//! disturbed. The expected buy price is derived from m_fShopProfitMargin rather than hardcoded; the
//! independent claim is the second assertion, that buy price is always strictly above base price.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Economy_PriceAndDemandSeams : SCR_AutotestCaseBase
{
	//! Synthetic resource ID used for the price/demand round-trip.
	static const int PROBE_ITEM_ID = 900001;

	//! Second synthetic ID, never written to, used to pin the documented defaults.
	static const int PROBE_UNKNOWN_ITEM_ID = 900002;

	//! Price seeded through SetPrice(). Chosen so price * (1 + 0.25 default margin) is exact.
	static const int PROBE_PRICE = 1000;

	//! Demand seeded through SetDemand().
	static const int PROBE_DEMAND = 7;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		// Documented fallbacks for an ID the maps have never seen.
		int unknownPrice = economy.GetPrice(PROBE_UNKNOWN_ITEM_ID);
		if (unknownPrice != 500)
		{
			SetFailure("GetPrice() on an unknown id returned %1, expected the documented default 500", unknownPrice.ToString());
			return true;
		}

		int unknownDemand = economy.GetDemand(PROBE_UNKNOWN_ITEM_ID);
		if (unknownDemand != 5)
		{
			SetFailure("GetDemand() on an unknown id returned %1, expected the documented default 5", unknownDemand.ToString());
			return true;
		}

		// SetPrice -> GetPrice round-trip.
		economy.SetPrice(PROBE_ITEM_ID, PROBE_PRICE);
		int readPrice = economy.GetPrice(PROBE_ITEM_ID);
		if (readPrice != PROBE_PRICE)
		{
			SetFailure("SetPrice(%1) then GetPrice() returned %2", PROBE_PRICE.ToString(), readPrice.ToString());
			return true;
		}

		// SetDemand -> GetDemand round-trip.
		economy.SetDemand(PROBE_ITEM_ID, PROBE_DEMAND);
		int readDemand = economy.GetDemand(PROBE_ITEM_ID);
		if (readDemand != PROBE_DEMAND)
		{
			SetFailure("SetDemand(%1) then GetDemand() returned %2", PROBE_DEMAND.ToString(), readDemand.ToString());
			return true;
		}

		// GetBuyPrice at "0 0 0" with no player skips the town stock / port distance terms in
		// GetSellPrice entirely, so it is exactly the base price plus the shop profit margin.
		int buyPrice = economy.GetBuyPrice(PROBE_ITEM_ID, "0 0 0", -1);
		int expectedBuyPrice = Math.Round(PROBE_PRICE + (PROBE_PRICE * config.m_fShopProfitMargin));

		if (buyPrice != expectedBuyPrice)
		{
			SetFailure("GetBuyPrice() returned %1, expected %2 (base %3 plus m_fShopProfitMargin)",
				buyPrice.ToString(), expectedBuyPrice.ToString(), PROBE_PRICE.ToString());
			return true;
		}

		if (buyPrice <= PROBE_PRICE)
		{
			SetFailure("GetBuyPrice() %1 is not above the base price %2 - the shop margin is not being applied",
				buyPrice.ToString(), PROBE_PRICE.ToString());
			return true;
		}

		PrintFormat("Economy seams: price %1, demand %2, buy price %3",
			readPrice.ToString(), readDemand.ToString(), buyPrice.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! No character's persistence configuration may self-spawn - alive OR dead. Both halves are
//! load-bearing and they fail in opposite directions.
//!
//! ALIVE: Overthrow's managers rebuild every garrison, patrol and deployment from manager state on
//! load, so a live character that ALSO self-spawns is doubled AI at every base.
//!
//! DEAD: guards against reinstating MarkForSelfSpawn, which flipped a corpse's config via
//! SetConfig(). That marks the configuration SCRIPTED, a scripted configuration is written with an
//! EMPTY store name, and the loader resolves BY store name - so every marked corpse became a record
//! the engine rejects on load. ⚠ The flag was set in memory, which is why the previous version of
//! this case went GREEN while the feature stayed broken: it asserted the flag, not the outcome. So
//! the dead half now asserts the ABSENCE of the flag. BUG-018 remains open for AI corpses.
//!
//! The poll is diagnostic, NOT a retry - it waits for the controller to report death so a failure
//! cannot be "asked too early", and expiry is itself a named failure.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_CharacterConfigNeverSelfSpawns : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the native lazy tracking registration of a freshly spawned character.
	static const int MAX_TRACKING_POLLS = 120;

	//! Frame polls allowed after ForceDeath() for the corpse config to be the matched one. The
	//! game-mode hook re-matches on the death event and once more a frame later; this budget is far
	//! above that so expiry means "never", not "not yet".
	static const int MAX_REMATCH_POLLS = 300;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iRematchPolls;

	//! The character this case spawns, kills and deletes. Never outlives the case.
	protected IEntity m_Character;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnSubjectCharacter();

		if (m_iPhase == 1)
			return AwaitTrackingThenKill();

		return AwaitCorpseConfig();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a lone recruit character - the cheapest single-character prefab in the game mode's own
	//! configuration (the "civilian prefab" is a whole AI group, not a character).
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubjectCharacter()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - see OVT_TEST_Init_Persistence_SystemIsOnline.");
			return true;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn a subject from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject character");
			return true;
		}

		m_Character = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, towns.m_Towns[0].location);
		if (!m_Character)
		{
			SetFailure("SpawnEntityPrefab() produced no character from the civilian prefab");
			return true;
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the native lazy tracking, pins the alive-config invariant, then kills the subject.
	//! \return True when the case is finished.
	protected bool AwaitTrackingThenKill()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence || !m_Character)
		{
			SetFailure("The persistence system or the subject character disappeared while waiting for tracking");
			return FinishAndCleanUp();
		}

		if (!persistence.IsTracked(m_Character))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > MAX_TRACKING_POLLS)
			{
				SetFailure("The spawned character was never tracked (%1 polls) - Character_Base.et no longer carries the native Persistence component, so no character (dead or alive) is ever saved", m_iTrackingPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		EntityPersistenceConfig aliveConfig = EntityPersistenceConfig.Cast(persistence.GetConfig(m_Character));
		if (!aliveConfig)
		{
			SetFailure("GetConfig() handed back no entity config for a tracked live character");
			return FinishAndCleanUp();
		}

		// Phase 3's invariant, the reason the corpse rule must not overmatch: a LIVE character that
		// self-spawns on load is doubled AI for every garrison Overthrow rebuilds itself.
		if (aliveConfig.m_bSelfSpawn)
		{
			SetFailure("A LIVE character's matched persistence config already self-spawns - the corpse rule (or a config edit) is matching the living, which doubles every AI on load");
			return FinishAndCleanUp();
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		if (!character || !character.GetCharacterController())
		{
			SetFailure("The spawned recruit is not a character with a controller - it cannot be killed, so the corpse re-match cannot be exercised");
			return FinishAndCleanUp();
		}

		character.GetCharacterController().ForceDeath();

		m_iPhase = 2;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the controller to actually report death, then asserts the corpse's config STILL does
	//! not self-spawn. Reading before death has settled would pass for the wrong reason, so the wait is
	//! part of the assertion rather than a convenience.
	//! \return True when the case is finished.
	protected bool AwaitCorpseConfig()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence || !m_Character)
		{
			SetFailure("The persistence system or the corpse disappeared before the corpse config could be read");
			return FinishAndCleanUp();
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		bool isDead = character && character.GetCharacterController() && character.GetCharacterController().IsDead();

		if (!isDead)
		{
			m_iRematchPolls += 1;
			if (m_iRematchPolls > MAX_REMATCH_POLLS)
			{
				SetFailure("ForceDeath() was called but the character controller never reported the character dead (%1 polls) - the corpse half of this case could not be exercised at all",
					m_iRematchPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		// The kill hook has had the death event plus every frame since. If anything flipped this bit,
		// the resulting record is written with an empty configuration store name and the loader drops it.
		EntityPersistenceConfig corpseConfig = EntityPersistenceConfig.Cast(persistence.GetConfig(m_Character));
		if (corpseConfig && corpseConfig.m_bSelfSpawn)
		{
			SetFailure("A dead character's persistence config self-spawns. Something re-introduced PersistenceSystem.SetConfig() on the kill path (OVT_PersistenceTracking.MarkForSelfSpawn): a scripted config is serialized with an EMPTY store name, so the loader rejects the record with \"Unable to locate configuruation ''\" and the corpse never comes back - it only poisons the save. See BUG-018.");
			return FinishAndCleanUp();
		}

		PrintFormat("Character config never self-spawns: verified alive, and verified dead after %1 poll(s)", m_iRematchPolls.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the subject from the world after the verdict is in, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);

		m_Character = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The LOCAL PLAYER's character must be matched to a configuration that self-spawns on load.
//!
//! A load only instantiates records whose configuration says SelfSpawn 1; everything else is
//! dropped from storage and gone from every later save point. Vanilla ships the player-character
//! config as SelfSpawn 0 and re-enables it in Mission.conf; Overthrow inherits Common.conf, not
//! Mission.conf, so it must do that itself - the {64ECE6462993EA13} override in the Player group of
//! Configs/Systems/Persistence/Overthrow.conf. Without it a player who logs out comes back as a
//! fresh civilian with their gear gone.
//!
//! ⚠ The config is read off a LIVE player-controlled character rather than looked up by GUID: what
//! matters is which configuration the engine actually MATCHED. A rule that stopped matching, or an
//! override in the wrong group, would both leave the GUID intact and the behaviour broken.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_PlayerCharacterConfigSelfSpawns : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player to have a character with a matched config.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - see OVT_TEST_Init_Persistence_SystemIsOnline.");
			return true;
		}

		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);

		IEntity body;
		if (!players.IsEmpty())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(players[0]);

		if (!body || !persistence.IsTracked(body))
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("No tracked player-controlled character appeared in %1 polls, so the configuration the engine matches to a player body could not be read", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		EntityPersistenceConfig config = EntityPersistenceConfig.Cast(persistence.GetConfig(body));
		if (!config)
		{
			SetFailure("GetConfig() handed back no entity config for a tracked player character");
			return true;
		}

		if (!config.m_bSelfSpawn)
		{
			SetFailure("The player character's matched persistence config does NOT self-spawn. The {64ECE6462993EA13} override in the Player group of Overthrow.conf is not reaching the config the engine matched. A player who logs out will come back as a fresh civilian with their gear gone, because their stored body record is dropped at load.");
			return true;
		}

		PrintFormat("Player character config self-spawns (matched after %1 poll(s)) - a stored body survives a restart", m_iPolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Overthrow's reconnect component must be ON THE GAME MODE, or a disconnecting player's body is
//! deleted and BUG-086 is back.
//!
//! SCR_BaseGameMode.OnPlayerDisconnected deletes the leaving player's character unless
//! SCR_ReconnectComponent.GetInstance().HandlePlayerDisconnect() claims it. That instance exists
//! only because OVT_OverthrowGameMode.et carries an OVT_ReconnectComponent entry, and a dropped or
//! renamed prefab entry fails SILENTLY.
//!
//! ⚠ It asserts the SUBCLASS. Vanilla's base class would resolve here and answer with vanilla's
//! rules - replication kicks only, 120 s expiry, no hiding - a different feature wearing the name.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_ReconnectComponentClaimsLeavingBodies : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		SCR_ReconnectComponent reconnect = SCR_ReconnectComponent.GetInstance();
		if (!reconnect)
		{
			SetFailure("There is no reconnect component on the game mode. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_ReconnectComponent entry, so SCR_BaseGameMode.OnPlayerDisconnected will DELETE every disconnecting player's body - see BUG-086.");
			return true;
		}

		if (!OVT_ReconnectComponent.Cast(reconnect))
		{
			SetFailure("The game mode's reconnect component is vanilla's %1, not OVT_ReconnectComponent. Vanilla only reserves a body for connection drops and only for its timeout, so a clean quit still destroys it.",
				reconnect.Type().ToString());
			return true;
		}

		Print("Overthrow's reconnect component is live - a disconnecting player's body is claimed, not deleted");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Reserving a CHARACTER must hide it, stop it simulating, leave it tracked - and be reversible.
//!
//! Exercised on a real character rather than a bare entity because the reservation model assumes
//! ClearFlags/SetFlags behave on a ChimeraCharacter - controller, animation system, inventory - the
//! way they do on a plain entity, and reversibly. The flags are read back, not assumed.
//!
//! Tracking is asserted throughout: a reserved body that stopped being tracked would be absent from
//! the next save point and gone after a restart, which is what BUG-086 is about.
//!
//! Cannot see whether a CLIENT still renders its own copy of a hidden body - play-test territory.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_ReservationHidesACharacterReversibly : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the native lazy tracking to pick up the spawned character.
	static const int MAX_TRACKING_POLLS = 300;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected IEntity m_Character;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnSubject();

		return AwaitTrackingThenReserve();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a character to reserve. The civilian recruit prefab is used for the same reason the
	//! corpse case uses it: it is a real ChimeraCharacter that the campaign already spawns.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubject()
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn a subject from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject character");
			return true;
		}

		m_Character = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, towns.m_Towns[0].location);
		if (!m_Character)
		{
			SetFailure("SpawnEntityPrefab() produced no character from the civilian prefab");
			return true;
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for tracking, then drives reserve -> assert -> release -> assert in one pass.
	//! \return True when the case is finished.
	protected bool AwaitTrackingThenReserve()
	{
		if (!m_Character)
		{
			SetFailure("The subject character disappeared before it could be reserved");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceTracking.IsTracked(m_Character))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > MAX_TRACKING_POLLS)
			{
				SetFailure("The spawned character was never tracked (%1 polls) - a body that is not tracked cannot be reserved in the first place", m_iTrackingPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (OVT_PersistenceReservation.IsReserved(m_Character))
		{
			SetFailure("A freshly spawned character already reports reserved - IsReserved() is not reading the flag it claims to, so every assertion below would pass vacuously");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.Reserve(m_Character))
		{
			SetFailure("Reserve() refused a tracked, live character");
			return FinishAndCleanUp();
		}

		EntityFlags reservedFlags = m_Character.GetFlags();

		if (reservedFlags & EntityFlags.VISIBLE)
		{
			SetFailure("Reserve() left the character VISIBLE. ClearFlags does not take on a ChimeraCharacter, so an offline player's body stays in play - see OVT_PersistenceReservation.");
			return FinishAndCleanUp();
		}

		if (reservedFlags & EntityFlags.TRACEABLE)
		{
			SetFailure("Reserve() left the character TRACEABLE - an offline player's body can still be shot and seen by AI, which is the whole thing hiding it is meant to prevent");
			return FinishAndCleanUp();
		}

		if (reservedFlags & EntityFlags.ACTIVE)
		{
			SetFailure("Reserve() left the character ACTIVE - a reserved body keeps simulating, and every offline player costs a full character tick forever");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceTracking.IsTracked(m_Character))
		{
			SetFailure("Reserving the character released its persistence tracking. A reserved body MUST stay tracked - an untracked one is absent from the next save point and gone after a restart, which is BUG-086 all over again.");
			return FinishAndCleanUp();
		}

		if (!GetGame().GetWorld().FindEntityByID(m_Character.GetID()))
		{
			SetFailure("Reserving the character removed it from the world - it must be hidden in place, not despawned");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.Release(m_Character))
		{
			SetFailure("Release() refused a reserved character");
			return FinishAndCleanUp();
		}

		EntityFlags releasedFlags = m_Character.GetFlags();

		if (!(releasedFlags & EntityFlags.VISIBLE) || !(releasedFlags & EntityFlags.TRACEABLE) || !(releasedFlags & EntityFlags.ACTIVE))
		{
			SetFailure("Release() did not put the character back in play: flags %1 (expected VISIBLE, TRACEABLE and ACTIVE all set). A returning player would be handed an invisible, untraceable, frozen body.",
				releasedFlags.ToString());
			return FinishAndCleanUp();
		}

		if (OVT_PersistenceReservation.IsReserved(m_Character))
		{
			SetFailure("The character still reports reserved after Release()");
			return FinishAndCleanUp();
		}

		PrintFormat("Reservation round trip on a live character: hidden, untraceable, inactive, still tracked, and fully restored (tracked after %1 poll(s))", m_iTrackingPolls.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the subject from the world after the verdict is in, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);

		m_Character = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Reserving must be TOLD TO THE CLIENTS, or a disconnected player stands visible on every one.
//!
//! OVT_PersistenceReservation hides with ClearFlags(), a LOCAL engine call, so clients - including
//! ones that stream the body in AFTER the reservation - see a frozen unkillable body.
//! OVT_ReservationSyncComponent mirrors the state into an RplProp and each proxy applies the visual
//! half locally.
//!
//! What this case pins, there being no client in the autotest world:
//!  1. the WIRING - the ownable-vehicle, player-character and recruit prefab chains still carry the
//!     component (BUG-191 was the recruit prefab, the one the original fix missed);
//!  2. the MIRROR - Reserve()/Release() drive the replicated state, through the production seams;
//!  3. the COLLISION half (BUG-189) - Reserve() takes the authority body out of the physics world
//!     and Release() restores the exact simulation state it saved.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_ReservationReplicatesToClients : SCR_AutotestCaseBase
{
	protected IEntity m_Vehicle;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string diagnostic;

		ResourceName prefab;
		if (!OVT_TEST_PersistenceSubject.ResolveOwnableVehiclePrefab(prefab, diagnostic))
		{
			SetFailure("Cannot resolve a vehicle to spawn: %1", diagnostic);
			return true;
		}

		vector position;
		if (!OVT_TEST_PersistenceSubject.ResolveVehicleSpawnPosition(position, diagnostic))
		{
			SetFailure("Cannot resolve somewhere to put a vehicle: %1", diagnostic);
			return true;
		}

		m_Vehicle = OVT_Global.SpawnEntityPrefab(prefab, position);
		if (!m_Vehicle)
		{
			SetFailure("SpawnEntityPrefab() produced no vehicle from %1", prefab);
			return true;
		}

		// Wiring, vehicle half: the component rides the Wheeled_Base override every ownable car
		// inherits. Losing the prefab entry is silent - this sentence is the tripwire.
		OVT_ReservationSyncComponent sync = OVT_ComponentFinder<OVT_ReservationSyncComponent>.Find(m_Vehicle);
		if (!sync)
		{
			SetFailure("The spawned vehicle has no OVT_ReservationSyncComponent - Prefabs/Vehicles/Core/Wheeled_Base.et has lost its entry, so a reserved vehicle is a visible ghost on every client again");
			return FinishAndCleanUp();
		}

		if (sync.IsReserved())
		{
			SetFailure("A freshly spawned vehicle already reports reserved to clients - the mirror assertions below would pass vacuously");
			return FinishAndCleanUp();
		}

		// The collision half (BUG-189) is pinned against the authority's own body, which is the one
		// this world has: reserve must take the body out of the physics world, release must put it
		// back in the state it was in. A vehicle prefab without physics would be a test-world defect
		// worth hearing about, so that is a failure too.
		Physics phys = m_Vehicle.GetPhysics();
		if (!phys)
		{
			SetFailure("The spawned vehicle has no Physics body - the BUG-189 collision half cannot be pinned");
			return FinishAndCleanUp();
		}
		SimulationState stateBefore = phys.GetSimulationState();
		if (stateBefore == SimulationState.NONE)
		{
			SetFailure("A freshly spawned vehicle is already out of the physics world - the restore assertion below would pass vacuously");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.Reserve(m_Vehicle))
		{
			SetFailure("Reserve() refused a live vehicle");
			return FinishAndCleanUp();
		}

		if (!sync.IsReserved())
		{
			SetFailure("Reserve() hid the vehicle but did NOT mirror the state into OVT_ReservationSyncComponent - nothing reaches the clients, and every one of them keeps rendering the reserved entity");
			return FinishAndCleanUp();
		}

		if (phys.GetSimulationState() != SimulationState.NONE)
		{
			SetFailure("Reserve() hid the vehicle but its physics body is still in the physics world (%1) - the invisible car still collides (BUG-189)", typename.EnumToString(SimulationState, phys.GetSimulationState()));
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.Release(m_Vehicle))
		{
			SetFailure("Release() refused a reserved vehicle");
			return FinishAndCleanUp();
		}

		if (sync.IsReserved())
		{
			SetFailure("Release() put the vehicle back in play but left the replicated state reserved - clients would hide a vehicle its returning owner is standing next to");
			return FinishAndCleanUp();
		}

		// The restore has to be the state the body actually had, not a plausible default: an aggregate
		// restore is what made a released car undrivable while it still looked and traced fine.
		if (phys.GetSimulationState() != stateBefore)
		{
			SetFailure("Release() did not restore the physics simulation state (%1, expected %2) - a returned vehicle would collide wrongly, or sit in its parking spot refusing to drive", typename.EnumToString(SimulationState, phys.GetSimulationState()), typename.EnumToString(SimulationState, stateBefore));
			return FinishAndCleanUp();
		}

		// Wiring, character half: the player prefab is reached through the spawn logic's own
		// attribute rather than a hard-coded path, so a re-pointed prefab is checked wherever it
		// points. The entry itself must be on the prefab: a reserved BODY is what the servers
		// actually reported.
		OVT_SpawnLogic spawnLogic = OVT_SpawnLogic.GetInstance();
		if (!spawnLogic || spawnLogic.m_rDefaultPrefab.IsEmpty())
		{
			SetFailure("No spawn logic instance (or no default character prefab) - the player-character half of the wiring cannot be checked");
			return FinishAndCleanUp();
		}

		if (!SCR_BaseContainerTools.FindComponentSource(Resource.Load(spawnLogic.m_rDefaultPrefab), OVT_ReservationSyncComponent))
		{
			SetFailure("The player character prefab (%1) has no OVT_ReservationSyncComponent - a disconnected player's body is a visible ghost on every client again", spawnLogic.m_rDefaultPrefab);
			return FinishAndCleanUp();
		}

		// Wiring, recruit half (BUG-191): reserved recruit bodies go through the same Reserve()
		// path, and the recruit prefab was the one reservable prefab BUG-185's fix missed. Reached
		// through the manager's own attribute for the same reason as the player prefab above.
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("No recruit manager instance (or no recruit prefab) - the recruit half of the wiring cannot be checked");
			return FinishAndCleanUp();
		}

		if (!SCR_BaseContainerTools.FindComponentSource(Resource.Load(recruits.m_sRecruitPrefab), OVT_ReservationSyncComponent))
		{
			SetFailure("The recruit prefab (%1) has no OVT_ReservationSyncComponent - a parked recruit's body is a visible ghost on every client (BUG-191)", recruits.m_sRecruitPrefab);
			return FinishAndCleanUp();
		}

		Print("Reservation state reaches the clients: prefab wiring intact on the vehicle, character and recruit chains, Reserve()/Release() drive the replicated flag both ways, and the physics body leaves and re-enters collision with them (BUG-189)");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the subject from the world after the verdict is in, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (m_Vehicle)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);

		m_Vehicle = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-085: a loadout must carry the CONTENTS of clothing and backpacks, not just the containers.
//!
//! The whole save -> apply round trip through the manager's public API. The assertion is made
//! against the TARGET CONTAINER's own storage - "somewhere on the character" is exactly what a
//! flattened apply would also satisfy.
//!
//! Nothing is hard-coded to a prefab: the container is whatever the character is already wearing
//! and the nested item comes from the difficulty config's starting items, both read from the LIVE
//! config, so a content change fails with a named diagnostic rather than turning this vacuous.
//!
//! ⚠ The storage PURPOSE mask is deliberately never used to classify anything here: EStoragePurpose
//! ordinals and flag bits diverge, and the character loadout storage answers PURPOSE_DEPOSIT while
//! holding worn clothing. Containers are identified by component class, as the code under test does.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Loadout_NestedItemsSurviveApply : SCR_AutotestCaseBase
{
	//! Owner id for the throwaway loadout. Not a real player - SaveLoadout keys on the string alone,
	//! which keeps this case independent of whether a player is registered at this tier.
	static const string TEST_PLAYER_ID = "OVT_TEST_BUG085";
	static const string TEST_LOADOUT_NAME = "bug085_nested";

	//! Frame polls allowed for a spawned character's inventory storages to come up.
	static const int MAX_INVENTORY_POLLS = 300;

	protected int m_iPhase;
	protected int m_iPolls;
	protected IEntity m_SourceCharacter;
	protected IEntity m_TargetCharacter;
	protected string m_sContainerPrefab;
	protected string m_sNestedPrefab;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnCharacters();

		if (m_iPhase == 1)
			return AwaitInventoriesThenStock();

		return SaveApplyAndAssert();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the source and the target. Two separate characters is the point: applying a loadout to
	//! the character it was taken from would pass even if apply did nothing at all.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnCharacters()
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn loadout subjects from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject characters");
			return true;
		}

		vector origin = towns.m_Towns[0].location;

		m_SourceCharacter = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, origin);
		m_TargetCharacter = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, origin + "6 0 6");

		if (!m_SourceCharacter || !m_TargetCharacter)
		{
			SetFailure("SpawnEntityPrefab() produced no character from the civilian prefab");
			return FinishAndCleanUp();
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for both characters' storages, then stocks the source: a worn container with one
	//! distinctive item inside it.
	//! \return True when the case is finished.
	protected bool AwaitInventoriesThenStock()
	{
		InventoryStorageManagerComponent sourceManager = OVT_ComponentFinder<InventoryStorageManagerComponent>.Find(m_SourceCharacter);
		InventoryStorageManagerComponent targetManager = OVT_ComponentFinder<InventoryStorageManagerComponent>.Find(m_TargetCharacter);

		if (!sourceManager || !targetManager || !HasStorages(sourceManager) || !HasStorages(targetManager))
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_INVENTORY_POLLS)
			{
				SetFailure("The spawned characters never came up with inventory storages (%1 polls) - nothing about a loadout can be measured without them", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		string diagnostic;

		IEntity container = ResolveContainer(sourceManager, diagnostic);
		if (!container)
		{
			SetFailure("%1", diagnostic);
			return FinishAndCleanUp();
		}

		UniversalInventoryStorageComponent containerStorage = UniversalInventoryStorageComponent.Cast(container.FindComponent(UniversalInventoryStorageComponent));

		IEntity nested = StockContainer(sourceManager, containerStorage, diagnostic);
		if (!nested)
		{
			SetFailure("%1", diagnostic);
			return FinishAndCleanUp();
		}

		m_sContainerPrefab = OVT_Global.GetPrefabName(container);
		m_sNestedPrefab = OVT_Global.GetPrefabName(nested);

		if (m_sContainerPrefab.IsEmpty() || m_sNestedPrefab.IsEmpty())
		{
			SetFailure("Could not read back a prefab name for the container or its contents - the assertion below would have nothing to match on");
			return FinishAndCleanUp();
		}

		m_iPhase = 2;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Saves the source's loadout, applies it to the target, and asserts the nested item arrived
	//! INSIDE the applied container.
	//! \return True when the case is finished.
	protected bool SaveApplyAndAssert()
	{
		OVT_LoadoutManagerComponent loadouts = OVT_Global.GetLoadouts();
		if (!loadouts)
		{
			SetFailure("OVT_Global.GetLoadouts() is null - no loadout manager on the game mode");
			return FinishAndCleanUp();
		}

		loadouts.SaveLoadout(TEST_PLAYER_ID, TEST_LOADOUT_NAME, m_SourceCharacter);

		OVT_PlayerLoadout saved = loadouts.GetLoadout(TEST_PLAYER_ID, TEST_LOADOUT_NAME);
		if (!saved)
		{
			SetFailure("SaveLoadout() stored nothing for the source character - the apply half cannot be measured");
			return FinishAndCleanUp();
		}

		// The EXTRACTION half is asserted separately and first, so that a regression there reports as
		// itself instead of being blamed on apply. It is also what stops this case passing vacuously:
		// if the container's contents were never recorded, there is nothing for apply to restore.
		if (!ExtractedNesting(saved))
		{
			SetFailure("The saved loadout does not record %1 inside %2 - extraction dropped the nesting, so the apply assertion below would be vacuous",
				m_sNestedPrefab, m_sContainerPrefab);
			return FinishAndCleanUp();
		}

		if (!loadouts.ApplyLoadoutToEntity(saved, m_TargetCharacter))
		{
			SetFailure("ApplyLoadoutToEntity() reported failure applying the saved loadout to a fresh character");
			return FinishAndCleanUp();
		}

		bool containerArrived = false;
		if (FindNestedOnTarget(containerArrived))
		{
			PrintFormat("Loadout round trip kept container contents: %1 arrived inside %2 on a fresh character", m_sNestedPrefab, m_sContainerPrefab);
			return FinishAndCleanUp();
		}

		if (!containerArrived)
		{
			SetFailure("The container %1 itself never arrived on the target character - the loadout apply failed further up than the nested-item path this case is about",
				m_sContainerPrefab);
			return FinishAndCleanUp();
		}

		SetFailure("%1 is not inside the applied container %2 - the container came back EMPTY. Nested items are inserted through the OWNER's storage manager against the container's own storage; a manager looked up on the container itself is always null for clothing and backpacks, and every item is then deleted (BUG-085).",
			m_sNestedPrefab, m_sContainerPrefab);
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] storageManager Manager to query.
	//! \return True when the manager reports at least one storage.
	protected bool HasStorages(InventoryStorageManagerComponent storageManager)
	{
		array<BaseInventoryStorageComponent> storages = {};
		storageManager.GetStorages(storages);
		return !storages.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds a container on the character - a worn item carrying a UniversalInventoryStorageComponent,
	//! which is precisely the class the code under test branches on. Falls back to dressing the
	//! character from the civilian loadout config, taking the FIRST qualifying choice rather than a
	//! random one, so this never depends on a skip roll.
	//! \param[in] storageManager The character's storage manager.
	//! \param[out] diagnostic Reason no container could be resolved; untouched on success.
	//! \return The container entity, or null.
	protected IEntity ResolveContainer(InventoryStorageManagerComponent storageManager, out string diagnostic)
	{
		IEntity worn = FindWornContainer(storageManager);
		if (worn)
			return worn;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_CivilianLoadout || !config.m_CivilianLoadout.m_aSlots)
		{
			diagnostic = "The source character wears no container and the civilian loadout config is empty - nothing to dress it with";
			return null;
		}

		foreach (OVT_LoadoutSlot slot : config.m_CivilianLoadout.m_aSlots)
		{
			if (!slot.m_aChoices)
				continue;

			foreach (ResourceName choice : slot.m_aChoices)
			{
				if (choice.IsEmpty())
					continue;

				EntitySpawnParams spawnParams();
				spawnParams.Transform[3] = m_SourceCharacter.GetOrigin();

				IEntity candidate = GetGame().SpawnEntityPrefab(Resource.Load(choice), GetGame().GetWorld(), spawnParams);
				if (!candidate)
					continue;

				if (!candidate.FindComponent(UniversalInventoryStorageComponent) || !storageManager.TryInsertItem(candidate))
				{
					SCR_EntityHelper.DeleteEntityAndChildren(candidate);
					continue;
				}

				return candidate;
			}
		}

		diagnostic = "No civilian-loadout choice with a universal storage could be put on the source character - there is no container to nest anything inside";
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] storageManager The character's storage manager.
	//! \return The first item the character holds that is itself a universal-storage container.
	protected IEntity FindWornContainer(InventoryStorageManagerComponent storageManager)
	{
		array<IEntity> items = {};
		storageManager.GetItems(items);

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			if (item.FindComponent(UniversalInventoryStorageComponent))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one distinctive item inside the container, through the character's manager - which is both
	//! the correct API and the one Overthrow's own spawn logic uses for starting items.
	//! \param[in] storageManager The character's storage manager.
	//! \param[in] containerStorage The container's own storage.
	//! \param[out] diagnostic Reason nothing could be stocked; untouched on success.
	//! \return The item now inside the container, or null.
	protected IEntity StockContainer(InventoryStorageManagerComponent storageManager, BaseInventoryStorageComponent containerStorage, out string diagnostic)
	{
		if (!containerStorage)
		{
			diagnostic = "The resolved container has no UniversalInventoryStorageComponent after all - nothing can be nested in it";
			return null;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty || !config.m_Difficulty.startingItems || config.m_Difficulty.startingItems.IsEmpty())
		{
			diagnostic = "The difficulty config lists no starting items - this case has no distinctive item to nest";
			return null;
		}

		foreach (ResourceName candidatePrefab : config.m_Difficulty.startingItems)
		{
			if (candidatePrefab.IsEmpty())
				continue;

			EntitySpawnParams spawnParams();
			spawnParams.Transform[3] = m_SourceCharacter.GetOrigin();

			IEntity candidate = GetGame().SpawnEntityPrefab(Resource.Load(candidatePrefab), GetGame().GetWorld(), spawnParams);
			if (!candidate)
				continue;

			if (!storageManager.TryInsertItemInStorage(candidate, containerStorage))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(candidate);
				continue;
			}

			// Insertion with no slot id may legitimately land in a child storage of the container, so
			// the recursive read is the one that decides whether it is genuinely nested.
			array<IEntity> contents = {};
			containerStorage.GetAll(contents, true);
			if (contents.Contains(candidate))
				return candidate;

			SCR_EntityHelper.DeleteEntityAndChildren(candidate);
		}

		diagnostic = "No starting item would fit inside the resolved container - the source loadout would have no nested contents to lose";
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] loadout The saved loadout.
	//! \return True when the loadout records the nested item as a CHILD of the container.
	protected bool ExtractedNesting(OVT_PlayerLoadout loadout)
	{
		array<ref OVT_LoadoutItem> items = loadout.GetItems();
		if (!items)
			return false;

		foreach (OVT_LoadoutItem item : items)
		{
			if (item.m_sResourceName != m_sContainerPrefab)
				continue;

			if (!item.HasChildItems())
				continue;

			foreach (OVT_LoadoutItem child : item.GetChildItems())
			{
				if (child.m_sResourceName == m_sNestedPrefab)
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the target character back: does it wear the container, and is the nested item inside it?
	//! \param[out] containerArrived True when a container of the saved prefab is on the target at all,
	//!             which separates "apply failed entirely" from "contents were lost".
	//! \return True when the nested item is inside one of those containers.
	protected bool FindNestedOnTarget(out bool containerArrived)
	{
		containerArrived = false;

		InventoryStorageManagerComponent targetManager = OVT_ComponentFinder<InventoryStorageManagerComponent>.Find(m_TargetCharacter);
		if (!targetManager)
			return false;

		array<IEntity> items = {};
		targetManager.GetItems(items);

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			if (OVT_Global.GetPrefabName(item) != m_sContainerPrefab)
				continue;

			containerArrived = true;

			UniversalInventoryStorageComponent containerStorage = UniversalInventoryStorageComponent.Cast(item.FindComponent(UniversalInventoryStorageComponent));
			if (!containerStorage)
				continue;

			array<IEntity> contents = {};
			containerStorage.GetAll(contents, true);

			foreach (IEntity content : contents)
			{
				if (content && OVT_Global.GetPrefabName(content) == m_sNestedPrefab)
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes both subjects and the throwaway loadout after the verdict is in, whichever verdict it
	//! was. The loadout store is manager state that outlives the case, so leaving an entry behind
	//! would leak into every later case in the run.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		OVT_LoadoutManagerComponent loadouts = OVT_Global.GetLoadouts();
		if (loadouts)
			loadouts.DeleteLoadout(TEST_PLAYER_ID, TEST_LOADOUT_NAME);

		if (m_SourceCharacter)
			SCR_EntityHelper.DeleteEntityAndChildren(m_SourceCharacter);

		if (m_TargetCharacter)
			SCR_EntityHelper.DeleteEntityAndChildren(m_TargetCharacter);

		m_SourceCharacter = null;
		m_TargetCharacter = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The player-group manager is ON THE GAME MODE and answers GetInstance().
//!
//! OVT_SpawnLogic.CreateAndJoinGroup delegates to its EnsureOwnGroup(), and its
//! GetOnPlayerAdded()/GetOnPlayerRemoved() subscriptions put a player back in a group after they
//! leave one. The instance exists only because OVT_OverthrowGameMode.et carries the component, and
//! a dropped prefab entry fails SILENTLY: every player spawns with NO GROUP (BUG-088's symptoms),
//! invisible in solo play until someone tries to command an AI.
//!
//! ⚠ It asserts the LIVE COMPONENT, not just a non-null static - s_Instance is assigned in
//! OnPostInit and never cleared, so a stale pointer from an earlier world satisfies a null check.
//!
//! The third assertion pins a precondition: EnsureOwnGroup() must refuse a player id with no
//! player controller and return -1, or a faction's group list fills with empty leaderless groups.
//!
//! Nothing here about joining, leaving or reconnecting - those need two clients and are on the
//! manual play-test checklist.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_PlayerGroups_ManagerResolves : SCR_AutotestCaseBase
{
	//! A player id no session ever issues, used to prove EnsureOwnGroup refuses an unknown player.
	static const int NOT_A_PLAYER_ID = -1;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_PlayerGroupManagerComponent manager = OVT_PlayerGroupManagerComponent.GetInstance();
		if (!manager)
		{
			SetFailure("OVT_PlayerGroupManagerComponent.GetInstance() is null - Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_PlayerGroupManagerComponent entry. Nothing gives a spawning player a group, and nothing puts a player back in one when they leave a group: no AI commanding, no group indicator, no recruits (BUG-088's symptom set).");
			return true;
		}

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
		{
			SetFailure("GetGame().GetGameMode() is null - there is no game mode to carry the manager");
			return true;
		}

		OVT_PlayerGroupManagerComponent onGameMode = OVT_PlayerGroupManagerComponent.Cast(gameMode.FindComponent(OVT_PlayerGroupManagerComponent));
		if (onGameMode != manager)
		{
			SetFailure("OVT_PlayerGroupManagerComponent.GetInstance() is not the component on this world's game mode - s_Instance is stale, so every caller is talking to a manager that is not wired to anything");
			return true;
		}

		// A player id that is not a player must never produce a group.
		int refused = manager.EnsureOwnGroup(NOT_A_PLAYER_ID);
		if (refused != -1)
		{
			SetFailure("EnsureOwnGroup(%1) returned %2 instead of -1 - the manager creates groups for ids that have no player controller, which fills the faction's group list with stray leaderless groups and exhausts its radio frequencies",
				NOT_A_PLAYER_ID.ToString(), refused.ToString());
			return true;
		}

		Print("Player-group manager is live on the game mode and refuses a player id with no controller");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-118: AI that Overthrow rebuilds every session must never be persistence-tracked, so it can
//! never write the orphaned records that made the save grow without bound.
//!
//! An occupying-faction group is spawned the way every un-virtualized spawner still does it, and a
//! waypoint the way every patrol gets one; the group entity, every member and the waypoint must all
//! end up UNTRACKED.
//!
//! ⚠ The control is what keeps it honest: native registration is LAZY and lands frames after spawn,
//! so "not tracked" right after spawn is what every fresh entity looks like. A control character is
//! spawned in the same frame and the case only passes once that control IS tracked.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_TransientAINotTracked : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the control's lazy registration AND the untrack retry queue (which
	//! ticks at 1 s) to both settle. The reservation case budgets 300 frames for registration
	//! alone; the queue adds up to two of its ticks on top.
	static const int MAX_POLLS = 900;

	protected int m_iPhase;
	protected int m_iPolls;
	protected IEntity m_Group;
	protected IEntity m_Waypoint;
	protected IEntity m_Control;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnSubjects();

		return AwaitVerdict();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the AI group (with members), a patrol waypoint, and the tracked control character.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubjects()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subjects");
			return true;
		}
		vector location = towns.m_Towns[0].location;

		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (!faction)
		{
			SetFailure("No occupying faction is configured - no group prefab to spawn");
			return true;
		}

		// The vanilla GROUP catalog first (it is what OVT_Faction.Init() builds and is never empty for
		// a real faction), then the group REGISTRY by name. The legacy prefab-slot arrays that used to
		// be the fallbacks here were retired with the base-defense migration.
		ResourceName groupPrefab;
		if (faction.m_aGroupPrefabSlots && !faction.m_aGroupPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aGroupPrefabSlots[0];
		else
			groupPrefab = faction.GetGroupPrefabByName("light_patrol");

		if (groupPrefab.IsEmpty())
			groupPrefab = faction.GetGroupPrefabByName("heavy_infantry");

		if (groupPrefab.IsEmpty())
		{
			SetFailure("The occupying faction has no group prefabs in any slot list - nothing to spawn a garrison from");
			return true;
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(groupPrefab, location));
		if (!group)
		{
			SetFailure("The occupying faction's group prefab did not produce an SCR_AIGroup");
			return true;
		}
		m_Group = group;

		// Members arrive over the following frames (SCR_AIGroup spawns one per EOnFrame tick, and
		// retries while the navmesh tile is still streaming in - SpawnAllImmediately() would DROP
		// members whose tile is not loaded yet). AwaitVerdict() gates on their arrival.

		m_Waypoint = OVT_Global.GetConfig().SpawnPatrolWaypoint(location);
		if (!m_Waypoint)
		{
			SetFailure("SpawnPatrolWaypoint() produced no waypoint");
			return FinishAndCleanUp();
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn the tracked control from");
			return FinishAndCleanUp();
		}

		m_Control = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, location);
		if (!m_Control)
		{
			SetFailure("SpawnEntityPrefab() produced no control character from the civilian prefab");
			return FinishAndCleanUp();
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the control is tracked and every AI entity is not, or the budget runs out.
	//! \return True when the case is finished.
	protected bool AwaitVerdict()
	{
		m_iPolls += 1;

		// Members spawn one per frame and retry while their navmesh tile streams in; without at
		// least one, every member assertion below would pass vacuously.
		SCR_AIGroup spawnedGroup = SCR_AIGroup.Cast(m_Group);
		if (spawnedGroup && spawnedGroup.GetAgentsCount() < 1)
		{
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The spawned group never produced a member (%1 polls) - the member half of this case cannot be asserted", m_iPolls.ToString());
				return FinishAndCleanUp();
			}
			return false;
		}

		// The control proves lazy registration has landed for this spawn batch. Until it has,
		// "not tracked" means nothing.
		if (!OVT_PersistenceTracking.IsTracked(m_Control))
		{
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The control character was never tracked (%1 polls) - registration is not running, so the untracked AI below proves nothing", m_iPolls.ToString());
				return FinishAndCleanUp();
			}
			return false;
		}

		string stillTracked = NameAnyTrackedSubject();
		if (stillTracked.IsEmpty())
		{
			PrintFormat("Rebuild-on-boot AI is untracked while a directly spawned character is tracked (settled after %1 poll(s))", m_iPolls.ToString());
			return FinishAndCleanUp();
		}

		if (m_iPolls > MAX_POLLS)
		{
			SetFailure("%1 is still persistence-tracked after %2 polls - it will write a record on the next save that no later session can ever claim or delete, which is BUG-118's unbounded save growth", stillTracked, m_iPolls.ToString());
			return FinishAndCleanUp();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Names the first AI subject that is still tracked, for the failure diagnostic.
	//! \return A description of the offending entity, or an empty string when all are untracked.
	protected string NameAnyTrackedSubject()
	{
		if (OVT_PersistenceTracking.IsTracked(m_Group))
			return "the group entity";

		SCR_AIGroup group = SCR_AIGroup.Cast(m_Group);
		if (group)
		{
			array<AIAgent> agents = new array<AIAgent>;
			group.GetAgents(agents);
			foreach (int i, AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity member = agent.GetControlledEntity();
				if (member && OVT_PersistenceTracking.IsTracked(member))
					return string.Format("member %1 of the spawned group", i);
			}
		}

		if (OVT_PersistenceTracking.IsTracked(m_Waypoint))
			return "the patrol waypoint";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every spawned subject after the verdict is in, whichever verdict it was. The control
	//! is tracked, so deleting it also removes its record (SelfDelete defaults on).
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(m_Group);
		if (group)
		{
			array<AIAgent> agents = new array<AIAgent>;
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity member = agent.GetControlledEntity();
				if (member)
					SCR_EntityHelper.DeleteEntityAndChildren(member);
			}
		}

		if (m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		if (m_Control)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Control);

		m_Group = null;
		m_Waypoint = null;
		m_Control = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-131: recruiting a group-spawned character must put its body BACK under persistence tracking.
//!
//! BUG-118's spawn-side untracking releases every group-spawned character, and town civilians are
//! group-spawned - so a recruited one had no persistent id, m_sBodyPersistenceId stayed empty, and
//! its gear could not survive a save.
//!
//! The case waits until a member is meaningfully untracked (same control-character honesty device
//! as the previous case), recruits it through AddRecruit(), and asserts the body is tracked again.
//! The final assertion is positive, so it cannot pass vacuously - it dies on the poll budget.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_RecruitedTransientCharacterIsRetracked : SCR_AutotestCaseBase
{
	//! Same budget as the previous case: lazy registration plus up to two 1 s untrack-queue ticks.
	static const int MAX_POLLS = 900;

	//! Collides with no real player; the record is removed again on cleanup.
	static const string TEST_OWNER_UID = "OVT_TEST_BUG131_OWNER";

	protected int m_iPhase;
	protected int m_iPolls;
	protected IEntity m_Group;
	protected IEntity m_Member;
	protected IEntity m_Control;
	protected string m_sRecruitId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnSubjects();

		if (m_iPhase == 1)
			return AwaitUntrackedMemberThenRecruit();

		return AwaitRetracked();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the AI group whose member will be recruited, and the tracked control character.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubjects()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subjects");
			return true;
		}
		vector location = towns.m_Towns[0].location;

		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (!faction)
		{
			SetFailure("No occupying faction is configured - no group prefab to spawn");
			return true;
		}

		// The vanilla GROUP catalog first (it is what OVT_Faction.Init() builds and is never empty for
		// a real faction), then the group REGISTRY by name. The legacy prefab-slot arrays that used to
		// be the fallbacks here were retired with the base-defense migration.
		ResourceName groupPrefab;
		if (faction.m_aGroupPrefabSlots && !faction.m_aGroupPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aGroupPrefabSlots[0];
		else
			groupPrefab = faction.GetGroupPrefabByName("light_patrol");

		if (groupPrefab.IsEmpty())
			groupPrefab = faction.GetGroupPrefabByName("heavy_infantry");

		if (groupPrefab.IsEmpty())
		{
			SetFailure("The occupying faction has no group prefabs in any slot list - nothing to spawn a group from");
			return true;
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(groupPrefab, location));
		if (!group)
		{
			SetFailure("The occupying faction's group prefab did not produce an SCR_AIGroup");
			return true;
		}
		m_Group = group;

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn the tracked control from");
			return FinishAndCleanUp();
		}

		m_Control = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, location);
		if (!m_Control)
		{
			SetFailure("SpawnEntityPrefab() produced no control character from the civilian prefab");
			return FinishAndCleanUp();
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for a group member whose untracked state is meaningful, then recruits it.
	//! \return True when the case is finished.
	protected bool AwaitUntrackedMemberThenRecruit()
	{
		m_iPolls += 1;

		// Members spawn one per frame and retry while their navmesh tile streams in.
		SCR_AIGroup spawnedGroup = SCR_AIGroup.Cast(m_Group);
		if (!m_Member && spawnedGroup && spawnedGroup.GetAgentsCount() >= 1)
		{
			array<AIAgent> agents = new array<AIAgent>;
			spawnedGroup.GetAgents(agents);
			if (agents.Count() >= 1 && agents[0])
				m_Member = agents[0].GetControlledEntity();
		}

		// The control proves lazy registration has landed for this spawn batch; the member being
		// untracked at that point is BUG-118's untracking having settled, not registration lag.
		bool ready = m_Member
			&& OVT_PersistenceTracking.IsTracked(m_Control)
			&& !OVT_PersistenceTracking.IsTracked(m_Member);

		if (!ready)
		{
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("No untracked group member alongside a tracked control after %1 polls - the precondition (BUG-118 untracking settled, registration running) never held", m_iPolls.ToString());
				return FinishAndCleanUp();
			}
			return false;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		m_sRecruitId = recruits.AddRecruit(TEST_OWNER_UID, m_Member);
		if (m_sRecruitId.IsEmpty())
		{
			SetFailure("AddRecruit() returned no recruit ID for the untracked group member");
			return FinishAndCleanUp();
		}

		m_iPhase = 2;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the recruited body is tracked again, or the budget runs out.
	//! \return True when the case is finished.
	protected bool AwaitRetracked()
	{
		m_iPolls += 1;

		if (OVT_PersistenceTracking.IsTracked(m_Member))
		{
			PrintFormat("Recruiting an untracked group-spawned character put its body back under persistence tracking (settled after %1 poll(s))", m_iPolls.ToString());
			return FinishAndCleanUp();
		}

		if (m_iPolls > MAX_POLLS)
		{
			SetFailure("The recruited body is still untracked after %1 polls - it will never reach a save, its record keeps an empty body id, and the recruit's gear cannot survive a despawn or restart (BUG-131)", m_iPolls.ToString());
			return FinishAndCleanUp();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the test recruit record and every spawned subject, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (!m_sRecruitId.IsEmpty())
		{
			OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
			if (recruits)
				recruits.RemoveRecruit(m_sRecruitId);
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(m_Group);
		if (group)
		{
			array<AIAgent> agents = new array<AIAgent>;
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity member = agent.GetControlledEntity();
				if (member)
					SCR_EntityHelper.DeleteEntityAndChildren(member);
			}
		}

		if (m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
		if (m_Control)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Control);

		m_Group = null;
		m_Member = null;
		m_Control = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A world bus-stop sign carries an OVT_MapMarkerComponent, and that marker reaches the registry by
//! itself and leaves it again when the entity is destroyed.
//!
//! Bus stops stopped being vanilla map descriptors: the only thing making one findable is the
//! OVT_MapMarkerComponent block in the same-GUID delta SignBusStop_01.et. That failing silently
//! means the map draws no bus stops and IsAtBusStop refuses every bus trip. compile-check.sh cannot
//! see a prefab, so this is the only automated guard.
//!
//! Covered in one pass: self-registration from OnPostInit; filed under BUS_STOP; GetNearestMarker()
//! finds it within 15 m and refuses outside; OnDelete unregisters.
//!
//! The subject is spawned by this case rather than found in the world, so it asserts nothing about
//! the test world's content. The poll is diagnostic, not a retry - registration is deferred one
//! frame and the budget is far above one, so expiry means "never", not "not yet".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_MapMarkers_BusStopRegisters : SCR_AutotestCaseBase
{
	//! The one vanilla bus-stop prefab, overridden by Overthrow's same-GUID delta.
	static const ResourceName BUS_STOP_PREFAB = "{7FCD4E7C25D886A8}Prefabs/Structures/Signs/Traffic/SignBusStop_01.et";

	//! The radius bus travel uses (OVT_FastTravelService.BUS_STOP_RADIUS). Named here so a change to it
	//! shows up as a test change rather than as silently different behaviour.
	static const float BUS_TRAVEL_RADIUS = 15;

	//! Frame polls allowed for the deferred self-registration. One frame is expected.
	static const int MAX_REGISTER_POLLS = 120;

	protected int m_iPhase;
	protected int m_iPolls;

	//! The sign this case spawns and destroys. Never outlives the case.
	protected IEntity m_Sign;

	//! Where the sign was put, kept so the proximity assertions survive the entity being deleted.
	protected vector m_vSignPos;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnSubjectSign();

		return AwaitRegistrationThenAssert();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one bus-stop sign well away from anything else and checks the prefab delta still carries
	//! the marker component at all.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubjectSign()
	{
		if (!OVT_Global.GetMapMarkers())
		{
			SetFailure("OVT_Global.GetMapMarkers() is null - Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_MapMarkerManagerComponent entry, so no map marker of any kind can be registered");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject sign");
			return true;
		}

		// Offset far enough that no world-placed bus stop can be inside the proximity assertions below.
		m_vSignPos = towns.m_Towns[0].location + Vector(500, 0, 500);

		m_Sign = OVT_Global.SpawnEntityPrefab(BUS_STOP_PREFAB, m_vSignPos);
		if (!m_Sign)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", BUS_STOP_PREFAB);
			return true;
		}

		// Read the sign's real origin: the spawn may be adjusted, and every distance below is measured
		// from where the entity actually is.
		m_vSignPos = m_Sign.GetOrigin();

		OVT_MapMarkerComponent marker = OVT_MapMarkerComponent.Cast(m_Sign.FindComponent(OVT_MapMarkerComponent));
		if (!marker)
		{
			SetFailure("A spawned bus stop has no OVT_MapMarkerComponent. Prefabs/Structures/Signs/Traffic/SignBusStop_01.et has lost its marker block, so NO bus stop in any world is discoverable: the map draws none and bus travel refuses everywhere with '#OVT-NotAtBusStop'.");
			return FinishAndCleanUp();
		}

		if (marker.GetCategory() != OVT_MapMarkerCategory.BUS_STOP)
		{
			SetFailure("The bus stop's marker is filed under category %1, not BUS_STOP - the bus-stop location type queries BUS_STOP and would find nothing",
				typename.EnumToString(OVT_MapMarkerCategory, marker.GetCategory()));
			return FinishAndCleanUp();
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the deferred registration, then drives registry -> proximity -> delete -> registry.
	//! \return True when the case is finished.
	protected bool AwaitRegistrationThenAssert()
	{
		OVT_MapMarkerManagerComponent markers = OVT_Global.GetMapMarkers();
		if (!markers || !m_Sign)
		{
			SetFailure("The marker registry or the subject sign disappeared while waiting for registration");
			return FinishAndCleanUp();
		}

		OVT_MapMarkerComponent marker = OVT_MapMarkerComponent.Cast(m_Sign.FindComponent(OVT_MapMarkerComponent));
		if (!marker)
		{
			SetFailure("The subject sign lost its marker component between phases");
			return FinishAndCleanUp();
		}

		array<OVT_MapMarkerComponent> busStops = markers.GetMarkers(OVT_MapMarkerCategory.BUS_STOP);
		if (busStops.Find(marker) == -1)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_REGISTER_POLLS)
			{
				SetFailure("A spawned bus stop never registered itself (%1 polls). OVT_MapMarkerComponent.OnPostInit no longer reaches OVT_MapMarkerManagerComponent.RegisterMarker, so every marker that appears after the world scan - every player-built one - is invisible on the map.",
					m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		// Registering twice must not duplicate: the world scan and self-registration both run, and the
		// whole design depends on that being harmless.
		int beforeCount = markers.GetMarkerCount();
		markers.RegisterMarker(marker);
		if (markers.GetMarkerCount() != beforeCount)
		{
			SetFailure("RegisterMarker() is not idempotent: count went %1 -> %2 on a re-register. The world scan and component self-registration both run over the same markers, so every world-placed marker would be listed twice.",
				beforeCount.ToString(), markers.GetMarkerCount().ToString());
			return FinishAndCleanUp();
		}

		// The bus-travel lookup itself, at the radius OVT_FastTravelService.IsAtBusStop uses.
		if (markers.GetNearestMarker(m_vSignPos, OVT_MapMarkerCategory.BUS_STOP, BUS_TRAVEL_RADIUS) != marker)
		{
			SetFailure("GetNearestMarker() did not return the registered bus stop standing at the probe position - the lookup OVT_FastTravelService.IsAtBusStop makes for bus travel is broken");
			return FinishAndCleanUp();
		}

		// Out of range must refuse, otherwise the radius means nothing and OVT_TravelResult.NOT_AT_BUS_STOP
		// ("#OVT-NotAtBusStop") never fires.
		if (markers.GetNearestMarker(m_vSignPos + Vector(0, 0, BUS_TRAVEL_RADIUS * 20), OVT_MapMarkerCategory.BUS_STOP, BUS_TRAVEL_RADIUS))
		{
			SetFailure("GetNearestMarker() returned a bus stop for a probe far outside the radius - bus travel would accept a click anywhere on the map");
			return FinishAndCleanUp();
		}

		// Wrong category must not match, or POI and bus-stop markers would draw as each other.
		if (markers.GetNearestMarker(m_vSignPos, OVT_MapMarkerCategory.POI, BUS_TRAVEL_RADIUS))
		{
			SetFailure("GetNearestMarker() returned a BUS_STOP marker when asked for a POI - the category filter is not applied");
			return FinishAndCleanUp();
		}

		SCR_EntityHelper.DeleteEntityAndChildren(m_Sign);
		m_Sign = null;

		if (markers.GetNearestMarker(m_vSignPos, OVT_MapMarkerCategory.BUS_STOP, BUS_TRAVEL_RADIUS))
		{
			SetFailure("A destroyed bus stop is still in the registry - OVT_MapMarkerComponent.OnDelete no longer unregisters, so the map keeps drawing markers for entities that are gone");
			return FinishAndCleanUp();
		}

		PrintFormat("Bus-stop marker round trip: registered after %1 poll(s), idempotent, found at %2 m and refused beyond it, and unregistered on delete",
			m_iPolls.ToString(), BUS_TRAVEL_RADIUS.ToString());

		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the subject from the world after the verdict is in, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (m_Sign)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Sign);

		m_Sign = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TutorialManagerComponent manager = OVT_Global.GetTutorialManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetTutorialManager() is null - Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_TutorialManagerComponent entry. Nothing subscribes to any trigger invoker and no tutorial can ever fire, silently.");
			return true;
		}

		array<ref OVT_TutorialEntryConfig> entries = manager.GetEntries();
		if (!entries)
		{
			SetFailure("OVT_TutorialManagerComponent.GetEntries() returned a null array - m_aEntries was never authored on the game mode prefab");
			return true;
		}

		// >= 1, never a magic count: the proof entry is one today and tutorial-content adds more.
		if (entries.Count() < 1)
		{
			SetFailure("No tutorial entries registered: m_aEntries on the game mode prefab is empty, so the tutorial framework has nothing to deliver");
			return true;
		}

		string shapeError = FindFirstShapeError(entries);
		if (shapeError != "")
		{
			SetFailure("%1", shapeError);
			return true;
		}

		Print("Tutorial manager is live with " + entries.Count().ToString() + " structurally valid entries");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks the authored entries and describes the FIRST structural problem found.
	//! \param[in] entries The authored entry list. Assumed non-null and non-empty.
	//! \return A ready-to-report failure message, or an empty string when every entry is sound.
	protected string FindFirstShapeError(array<ref OVT_TutorialEntryConfig> entries)
	{
		ref set<string> seenIds = new set<string>();

		// Every OVT_ShopType member name, read off the enum itself rather than hand-listed, so a new
		// shop type is accepted automatically and a renamed one is caught. See CheckTransactionFilters.
		ref array<string> shopTypeNames = new array<string>();
		SCR_Enum.GetEnumNames(OVT_ShopType, shopTypeNames);

		for (int i = 0; i < entries.Count(); i++)
		{
			OVT_TutorialEntryConfig entry = entries.Get(i);
			string position = "Tutorial entry at index " + i.ToString();

			if (!entry)
				return position + " is null - an empty row was left in m_aTutorialEntries on the game mode prefab";

			if (entry.m_sId == "")
				return position + " has an empty m_sId. The id is the entry's identity on the wire and its permanent key in every player's seen store; it cannot be blank.";

			if (seenIds.Contains(entry.m_sId))
				return position + " repeats the id '" + entry.m_sId + "'. Ids must be unique and are never reused - a duplicate makes one entry's seen state suppress the other's, permanently.";

			seenIds.Insert(entry.m_sId);

			if (!entry.m_aPages || entry.m_aPages.Count() < 1)
				return "Tutorial entry '" + entry.m_sId + "' has no pages - there is nothing for the popup to show";

			if (!entry.m_aTriggers || entry.m_aTriggers.Count() < 1)
				return "Tutorial entry '" + entry.m_sId + "' has no triggers - nothing can ever make it fire";

			string filterError = CheckTransactionFilters(entry, shopTypeNames);
			if (filterError != "")
				return filterError;

			string spawnError = CheckSpawnFilters(entry);
			if (spawnError != "")
				return spawnError;
		}

		return CheckWelcomeCoverage(entries);
	}

	//------------------------------------------------------------------------------------------------
	//! Validates every PLAYER_SPAWNED filter on one entry against the spawn-context vocabulary.
	//!
	//! There are exactly three legal values - "house", "nohouse" and "" (either). ⚠ A fourth value is
	//! NOT findable by grep against another config: it simply never equals the dispatched context, the
	//! welcome never fires, and nothing reports it.
	//! \param[in] entry The entry to check. Assumed non-null with a non-empty m_aTriggers.
	//! \return A ready-to-report failure message, or an empty string when every filter is valid.
	protected string CheckSpawnFilters(OVT_TutorialEntryConfig entry)
	{
		for (int i = 0; i < entry.m_aTriggers.Count(); i++)
		{
			OVT_TutorialTrigger trigger = entry.m_aTriggers.Get(i);
			if (!trigger)
				continue;

			if (trigger.m_eEvent != OVT_TutorialEvent.PLAYER_SPAWNED)
				continue;

			// "" means "either spawn", which is always valid.
			if (trigger.m_sFilter == "")
				continue;

			if (trigger.m_sFilter == OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE)
				continue;

			if (trigger.m_sFilter == OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE)
				continue;

			return "Tutorial entry '" + entry.m_sId + "' filters PLAYER_SPAWNED on '" + trigger.m_sFilter + "', which is not a spawn context. The value it is compared against is authored by the server in OVT_OverthrowGameMode.FinalizePlayerPreparation and carried to the client by OVT_TutorialComponent, and it is only ever '" + OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE + "' or '" + OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE + "'. This filter can therefore never match ANY spawn: the entry will SILENTLY NEVER FIRE - no compile error, no runtime warning, no log line. Fix the m_sFilter in the entry's .conf under Configs/Tutorials/ to '" + OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE + "', '" + OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE + "', or \"\" for either.";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Both spawn contexts still have a welcome to show.
	//!
	//! Delete one, or set its m_bEnabled to 0, and half the player base gets NO welcome - and nothing
	//! else notices, because every remaining entry is still structurally perfect. Deliberately "at
	//! least one", not "exactly one", so a third-party mod adding its own entry is not a defect. The
	//! selection runs through the real matcher, so a disabled entry does not count towards coverage.
	//! \param[in] entries The authored entry list. Assumed non-null and non-empty.
	//! \return A ready-to-report failure message, or an empty string when both contexts are covered.
	protected string CheckWelcomeCoverage(array<ref OVT_TutorialEntryConfig> entries)
	{
		array<string> houseIds = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeSpawnContext(OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE), houseIds);

		if (houseIds.Count() < 1)
			return "No enabled tutorial entry matches the '" + OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE + "' spawn context. A player who is given a house, a car and starting cash would see no welcome at all on their first spawn. Check that Configs/Tutorials/proofWelcome.conf still carries a PLAYER_SPAWNED trigger filtered '" + OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE + "', is still m_bEnabled 1, and is still listed in m_aTutorialEntries on the game mode prefab.";

		array<string> nohouseIds = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeSpawnContext(OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE), nohouseIds);

		if (nohouseIds.Count() < 1)
			return "No enabled tutorial entry matches the '" + OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE + "' spawn context. A player for whom no starting house was free spawns at a bus stop with no house and no car, and would see no welcome at all - the exact player this feature exists for, and the one nobody play-tests. Check that Configs/Tutorials/welcomeNohome.conf still carries a PLAYER_SPAWNED trigger filtered '" + OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE + "', is still m_bEnabled 1, and is still listed in m_aTutorialEntries on the game mode prefab.";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one PLAYER_SPAWNED occurrence carrying a spawn context, for the coverage check.
	//! \param[in] filter The spawn context to dispatch.
	//! \return The occurrence.
	protected OVT_TutorialEventContext MakeSpawnContext(string filter)
	{
		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = OVT_TutorialEvent.PLAYER_SPAWNED;
		ctx.m_iPlayerId = 1;
		ctx.m_iValue = 0;
		ctx.m_sFilter = filter;
		return ctx;
	}

	//------------------------------------------------------------------------------------------------
	//! Validates every PLAYER_TRANSACTION filter on one entry against the OVT_ShopType enum.
	//!
	//! ⚠ PLAYER_TRANSACTION is the only event whose filter value is MANUFACTURED BY THE ENGINE -
	//! SCR_Enum.GetEnumName(OVT_ShopType, ...) - rather than written by a human on both sides. Every
	//! other filter is compared against a string some other config spells out, so a typo is findable
	//! by grep. Here it is not: the entry never fires and nothing reports it.
	//! \param[in] entry The entry to check. Assumed non-null with a non-empty m_aTriggers.
	//! \param[in] shopTypeNames Every OVT_ShopType member name, from SCR_Enum.GetEnumNames.
	//! \return A ready-to-report failure message, or an empty string when every filter is valid.
	protected string CheckTransactionFilters(OVT_TutorialEntryConfig entry, array<string> shopTypeNames)
	{
		for (int i = 0; i < entry.m_aTriggers.Count(); i++)
		{
			OVT_TutorialTrigger trigger = entry.m_aTriggers.Get(i);
			if (!trigger)
				continue;

			if (trigger.m_eEvent != OVT_TutorialEvent.PLAYER_TRANSACTION)
				continue;

			// An empty filter means "any shop", which is always valid.
			if (trigger.m_sFilter == "")
				continue;

			if (shopTypeNames.Contains(trigger.m_sFilter))
				continue;

			string known = "";
			for (int n = 0; n < shopTypeNames.Count(); n++)
			{
				if (n > 0)
					known += ", ";

				known += shopTypeNames.Get(n);
			}

			return "Tutorial entry '" + entry.m_sId + "' filters PLAYER_TRANSACTION on '" + trigger.m_sFilter + "', which is not the name of any OVT_ShopType value. The manager builds the value it compares against with SCR_Enum.GetEnumName(OVT_ShopType, shop.m_ShopType) (OVT_TutorialManagerComponent.c:255), so this filter can never match ANY transaction: the tip will SILENTLY NEVER FIRE - no compile error, no runtime warning, no log line, just a tutorial nobody ever sees. Fix the m_sFilter in the entry's .conf under Configs/Tutorials/ to one of: " + known;
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Tutorial_InvokerSeamsExist : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string missing = FindFirstMissingInvoker();

		if (missing != "")
		{
			SetFailure("Tutorial trigger seam missing: %1. OVT_TutorialManagerComponent subscribes to it in SubscribeToInvokers(); with it gone that trigger silently never fires again.", missing);
			return true;
		}

		Print("Every catalogued tutorial trigger invoker is present and allocated");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks every catalogued invoker in turn and names the FIRST one that is missing.
	//! Each owning manager is checked before its invokers are dereferenced.
	//! \return Description of the first missing seam, or an empty string when all of them are present.
	protected string FindFirstMissingInvoker()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy) return "OVT_Global.GetEconomy() is null, so PLAYER_BUY, PLAYER_SELL and PLAYER_TRANSACTION have no source";
		if (!economy.m_OnPlayerBuy) return "OVT_EconomyManagerComponent.m_OnPlayerBuy (PLAYER_BUY)";
		if (!economy.m_OnPlayerSell) return "OVT_EconomyManagerComponent.m_OnPlayerSell (PLAYER_SELL)";
		if (!economy.m_OnPlayerTransaction) return "OVT_EconomyManagerComponent.m_OnPlayerTransaction (PLAYER_TRANSACTION)";

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance) return "OVT_Global.GetResistanceFaction() is null, so PLAYER_PLACE and PLAYER_BUILD have no source";
		if (!resistance.m_OnPlace) return "OVT_ResistanceFactionManager.m_OnPlace (PLAYER_PLACE)";
		if (!resistance.m_OnBuild) return "OVT_ResistanceFactionManager.m_OnBuild (PLAYER_BUILD)";

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits) return "OVT_Global.GetRecruits() is null, so PLAYER_RECRUIT_ADDED has no source";
		if (!recruits.m_OnRecruitAdded) return "OVT_RecruitManagerComponent.m_OnRecruitAdded (PLAYER_RECRUIT_ADDED)";

		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
		if (!skills) return "OVT_Global.GetSkills() is null, so PLAYER_SKILL has no source";
		if (!skills.m_OnPlayerSkill) return "OVT_SkillManagerComponent.m_OnPlayerSkill (PLAYER_SKILL)";

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns) return "OVT_Global.GetTowns() is null, so TOWN_CONTROL_CHANGE has no source";
		if (!towns.m_OnTownControlChange) return "OVT_TownManagerComponent.m_OnTownControlChange (TOWN_CONTROL_CHANGE)";

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying) return "OVT_Global.GetOccupyingFaction() is null, so BASE_CONTROL_CHANGE has no source";
		if (!occupying.m_OnBaseControlChanged) return "OVT_OccupyingFactionManager.m_OnBaseControlChanged (BASE_CONTROL_CHANGE)";

		// Static and lazily allocated: this can only be null if the getter itself stopped allocating.
		if (!OVT_PlayerWantedComponent.GetOnWantedLevelChanged()) return "OVT_PlayerWantedComponent.GetOnWantedLevelChanged() (PLAYER_WANTED)";
		if (!OVT_PlayerWantedComponent.GetOnEnteredBaseRange()) return "OVT_PlayerWantedComponent.GetOnEnteredBaseRange() (PLAYER_ENTER_BASE)";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve : SCR_AutotestCaseBase
{
	//! The ResourceName FieldManual.layout:16 hands to SCR_ConfigUIComponent.m_ConfigPath.
	static const ResourceName FIELD_MANUAL_ROOT = "{17295EF80DC38D53}Configs/FieldManual/FieldManualConfigRoot.conf";

	//! Title key of the category Overthrow appends. Everything else in the merged root is the base
	//! game's, which is how the vanilla categories are counted without naming any of them.
	static const string OVERTHROW_CATEGORY_TITLE = "#OVT-FieldManual_Category_Overthrow_Title";

	//! The base game's Introduction title key. Overthrow's sub-category reused it until Phase 7.3,
	//! which put a second button named "Introduction" in the category list.
	static const string VANILLA_INTRODUCTION_TITLE = "#AR-FieldManual_Category_Introduction_Title";

	//! Introduction, Editor, MP Modes, Gameplay, Equipment. A floor, never an equality.
	static const int VANILLA_CATEGORY_FLOOR = 5;

	//! The four sub-category buttons Overthrow's category draws (field-manual plan section 3.1).
	//! Checked for MEMBERSHIP, never for equality: a later feature adding a fifth sub-category is a
	//! content decision, not a regression, and must not turn this case red.
	static const ref array<string> OVERTHROW_SUB_CATEGORY_TITLES = {
		"#OVT-FieldManual_Category_GettingStarted_Title",
		"#OVT-FieldManual_Category_MoneyAndTrade_Title",
		"#OVT-FieldManual_Category_StayingHidden_Title",
		"#OVT-FieldManual_Category_TheResistance_Title"
	};

	//! Prefix every Overthrow-authored manual key carries. Branch C only judges these; vanilla's own
	//! 140 pages are the base game's business.
	static const string OVERTHROW_ENTRY_TITLE_PREFIX = "#OVT-FieldManual_";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		SCR_FieldManualConfigRoot root = SCR_FieldManualConfigLoader.LoadConfigRoot(FIELD_MANUAL_ROOT);
		if (!root)
		{
			SetFailure("SCR_FieldManualConfigLoader.LoadConfigRoot() returned null for the field-manual root. SCR_FieldManualUI.OnMenuOpen closes itself when this happens, so the Field Manual would not open at all.");
			return true;
		}

		// Printed BEFORE any assertion so one run yields the measurement even on a red verdict.
		Print("[Overthrow.FieldManual] merged root inventory: " + DescribeRoot(root));

		string failure = FindFirstMergeFault(root);
		if (failure != "")
		{
			SetFailure("%1", failure);
			return true;
		}

		failure = FindFirstContentlessOverthrowEntry(root);
		if (failure != "")
		{
			SetFailure("%1", failure);
			return true;
		}

		failure = FindFirstMissingOverthrowSubCategory(root);
		if (failure != "")
		{
			SetFailure("%1", failure);
			return true;
		}

		failure = FindFirstDuplicateOverthrowEntryTitle(root);
		if (failure != "")
		{
			SetFailure("%1", failure);
			return true;
		}

		failure = FindFirstBrokenTutorialLink(root);
		if (failure != "")
		{
			SetFailure("%1", failure);
			return true;
		}

		Print("Field-manual same-GUID override merged as a delta, and every tutorial deep link resolves");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the merged root for the faults a broken override produces, and describes the first one.
	//! \param[in] root The merged field-manual root. Assumed non-null.
	//! \return A ready-to-report failure message, or an empty string when the merge held.
	protected string FindFirstMergeFault(notnull SCR_FieldManualConfigRoot root)
	{
		if (!root.m_aCategories)
			return "The merged field-manual root has a null m_aCategories array, so the menu has nothing to list";

		int total = root.m_aCategories.Count();
		int overthrowCount = CountCategoriesTitled(root, OVERTHROW_CATEGORY_TITLE);

		if (overthrowCount < 1)
			return "The merged field-manual root has " + total.ToString() + " categories and NONE of them is Overthrow's ('" + OVERTHROW_CATEGORY_TITLE + "'). Overthrow's Configs/FieldManual/FieldManualConfigRoot.conf is not reaching the menu - check its .meta still declares GUID {17295EF80DC38D53} and that element {59908331EDFD9788} still exists in m_aCategories. Categories found: " + JoinCategoryTitles(root);

		int vanillaCount = total - overthrowCount;
		if (vanillaCount < VANILLA_CATEGORY_FLOOR)
			return "STOP - SAME-GUID MERGE SEMANTICS FALSIFIED. The merged field-manual root has " + total.ToString() + " categories, only " + vanillaCount.ToString() + " of which are the base game's; all five (Introduction, Editor, MP Modes, Gameplay, Equipment) should be there beside Overthrow's. Overthrow's same-GUID .conf has REPLACED the base root instead of appending to it. That takes the vanilla Field Manual with it, AND it falsifies the delta-merge behaviour that chimeraInputCommon.conf, ChimeraSystemsConfig.conf, CommandingMenu.conf and the game-mode prefab's append form all depend on - a far bigger finding than the Field Manual. Categories found: " + JoinCategoryTitles(root);

		if (!root.m_aTileBackgrounds || root.m_aTileBackgrounds.IsEmpty())
			return "The merged field-manual root has no tile backgrounds. Overthrow's override never declares m_aTileBackgrounds, so this array can only be populated by the base root's value surviving the merge - and SCR_FieldManualUI.c:253 calls m_aTileBackgrounds.GetRandomElement() UNGUARDED once per tile. Opening the Field Manual would error on its first tile.";

		return FindOverthrowCategoryFault(root);
	}

	//------------------------------------------------------------------------------------------------
	//! Overthrow's own category is shaped the way the UI can actually draw it.
	//!
	//! Two faults, both silent. A category with no sub-categories is pruned outright by
	//! SetAllEntriesAndParents (:655) and simply never appears. And a sub-category that reuses the base
	//! game's Introduction title key renders as a SECOND left-hand button literally named
	//! "Introduction" - which is what shipped until Phase 7.3 and what this guards against returning.
	//! \param[in] root The merged field-manual root. Assumed non-null, with m_aCategories non-null.
	//! \return A ready-to-report failure message, or an empty string when the category is sound.
	protected string FindOverthrowCategoryFault(notnull SCR_FieldManualConfigRoot root)
	{
		foreach (SCR_FieldManualConfigCategory category : root.m_aCategories)
		{
			if (!category || category.m_sTitle != OVERTHROW_CATEGORY_TITLE)
				continue;

			if (!category.m_aCategories || category.m_aCategories.IsEmpty())
				return "Overthrow's field-manual category has no sub-categories. Configs/FieldManual/Categories/FM_Overthrow.conf is not being inherited by element {59908331EDFD9788} of the root delta, and SCR_FieldManualUI.SetAllEntriesAndParents prunes an empty category outright - the Overthrow section would vanish from the manual with no error.";

			foreach (SCR_FieldManualConfigCategory subCategory : category.m_aCategories)
			{
				if (subCategory && subCategory.m_sTitle == VANILLA_INTRODUCTION_TITLE)
					return "Overthrow's field-manual sub-category is titled '" + VANILLA_INTRODUCTION_TITLE + "' - the BASE GAME's Introduction key. It renders as a second left-hand button literally named 'Introduction' beside the real one. Use an #OVT- key (Phase 7.3 introduced #OVT-FieldManual_Category_GettingStarted_Title for exactly this).";
			}
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! BRANCH A. Every entry under Overthrow's category carries at least one content piece.
	//!
	//! SCR_FieldManualUI.SetAllEntriesAndParents drops any entry whose m_aContent is empty, then any
	//! sub-category left with no entries - silently. A content-free entry is not a stub, it is an
	//! ABSENCE: its tile never draws and OVT_OpenEntryByTitle falls back to the manual's front page.
	//! \param[in] root The merged field-manual root. Assumed non-null.
	//! \return A ready-to-report failure message, or an empty string when every entry has content.
	protected string FindFirstContentlessOverthrowEntry(notnull SCR_FieldManualConfigRoot root)
	{
		if (!root.m_aCategories)
			return "";

		foreach (SCR_FieldManualConfigCategory category : root.m_aCategories)
		{
			if (!category || category.m_sTitle != OVERTHROW_CATEGORY_TITLE || !category.m_aCategories)
				continue;

			foreach (SCR_FieldManualConfigCategory subCategory : category.m_aCategories)
			{
				if (!subCategory || !subCategory.m_aEntries)
					continue;

				foreach (SCR_FieldManualConfigEntry entry : subCategory.m_aEntries)
				{
					if (!entry)
						continue;

					if (entry.m_aContent && !entry.m_aContent.IsEmpty())
						continue;

					return "Overthrow's field-manual entry '" + entry.m_sTitle + "' (under sub-category '" + subCategory.m_sTitle + "') has NO content pieces. SCR_FieldManualUI.SetAllEntriesAndParents prunes an entry with empty m_aContent SILENTLY, then prunes a sub-category left with no entries, then a category left with neither - so this page is not in the manual at all: no tile draws for it, and any tutorial popup deep-linking its title key resolves to the manual's FRONT PAGE instead. Give it at least one SCR_FieldManualPiece_Text in Configs/FieldManual/Categories/FM_Overthrow.conf, or retire it properly with m_bEnabled 0.";
				}
			}
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! BRANCH B. Each of the four named sub-categories is present under Overthrow's category.
	//!
	//! MEMBERSHIP, NOT EQUALITY. A fifth sub-category added by a later feature is a content decision
	//! and must not turn this red; only the disappearance or renaming of one of the four is a fault.
	//! A sub-category is a left-hand button and the shelf its entries live on, so losing one takes its
	//! whole tile grid with it without an error.
	//! \param[in] root The merged field-manual root. Assumed non-null.
	//! \return A ready-to-report failure message, or an empty string when all four are present.
	protected string FindFirstMissingOverthrowSubCategory(notnull SCR_FieldManualConfigRoot root)
	{
		array<string> present = {};
		CollectSubCategoryTitles(root, OVERTHROW_CATEGORY_TITLE, present);

		foreach (string expected : OVERTHROW_SUB_CATEGORY_TITLES)
		{
			if (present.Find(expected) != -1)
				continue;

			return "Overthrow's field-manual category is missing the sub-category '" + expected + "'. It is one of the four buttons the manual's left-hand list draws under the Overthrow heading, and every entry it holds goes with it - SCR_FieldManualUI never reports a missing sub-category, the button and its tiles simply are not there. Check Configs/FieldManual/Categories/FM_Overthrow.conf for a renamed or deleted m_sTitle. Sub-categories found: " + JoinStrings(present);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! BRANCH C. No Overthrow entry title key appears twice anywhere in the MERGED manual.
	//!
	//! OVT_OpenEntryByTitle takes the FIRST entry whose m_sTitle matches, so a duplicated key sends
	//! every deep link to whichever page was collected first. The MERGED root is searched, not just
	//! Overthrow's category, because a vanilla page adopting an #OVT- key breaks the links too.
	//! \param[in] root The merged field-manual root. Assumed non-null.
	//! \return A ready-to-report failure message, or an empty string when every key is unique.
	protected string FindFirstDuplicateOverthrowEntryTitle(notnull SCR_FieldManualConfigRoot root)
	{
		array<string> titles = {};
		CollectEntryTitles(root, titles);

		foreach (string title : titles)
		{
			if (!title.StartsWith(OVERTHROW_ENTRY_TITLE_PREFIX))
				continue;

			int occurrences;

			foreach (string other : titles)
			{
				if (other == title)
					occurrences++;
			}

			if (occurrences < 2)
				continue;

			return "The field-manual title key '" + title + "' appears " + occurrences.ToString() + " times in the merged manual. Title keys ARE the deep-link ids: OVT_OpenEntryByTitle matches m_sTitle exactly and case-sensitively and opens the FIRST match, so every popup pointing at this key lands on whichever page happens to come first - silently, and possibly on the wrong page forever. Give each entry in Configs/FieldManual/Categories/FM_Overthrow.conf its own title key (a copy-pasted entry that kept its source's m_sTitle is the usual cause). Manual pages available: " + JoinStrings(titles);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every authored tutorial entry that declares a field-manual deep link points at a page that
	//! actually exists in the merged root.
	//!
	//! Links match on the target's m_sTitle localization key, exactly and case-sensitively, so
	//! renaming a manual page silently breaks every popup pointing at it - the helper degrades to the
	//! front page by design, which looks like a content bug rather than a broken link.
	//! \param[in] root The merged field-manual root. Assumed non-null.
	//! \return A ready-to-report failure message, or an empty string when every link resolves.
	protected string FindFirstBrokenTutorialLink(notnull SCR_FieldManualConfigRoot root)
	{
		OVT_TutorialManagerComponent manager = OVT_Global.GetTutorialManager();
		if (!manager)
			return ""; // Its own case owns that failure; this one must not double-report it.

		array<ref OVT_TutorialEntryConfig> entries = manager.GetEntries();
		if (!entries)
			return "";

		array<string> manualTitles = {};
		CollectEntryTitles(root, manualTitles);

		foreach (OVT_TutorialEntryConfig entry : entries)
		{
			if (!entry)
				continue;

			if (entry.m_sFieldManualTitleKey == "")
				continue;

			if (manualTitles.Find(entry.m_sFieldManualTitleKey) != -1)
				continue;

			return "Tutorial entry '" + entry.m_sId + "' links to field-manual page '" + entry.m_sFieldManualTitleKey + "', and no entry in the merged manual has that m_sTitle. Its 'Learn more' button will silently open the manual's front page instead. Title keys ARE the link ids (plan D12): they are matched exactly and case-sensitively, and renaming one breaks every popup pointing at it. Manual pages available: " + JoinStrings(manualTitles);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] root The merged root. Assumed non-null.
	//! \param[in] title The category title key to count.
	//! \return How many top-level categories carry that title.
	protected int CountCategoriesTitled(notnull SCR_FieldManualConfigRoot root, string title)
	{
		int found;

		foreach (SCR_FieldManualConfigCategory category : root.m_aCategories)
		{
			if (category && category.m_sTitle == title)
				found++;
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks root category -> sub-category -> entries, which is every level the UI supports
	//! (SCR_FieldManualUI.SetAllEntriesAndParents:600-663 goes no deeper and silently drops a third).
	//! \param[in] root The merged root. Assumed non-null.
	//! \param[out] titles Receives every entry title key found, in declaration order.
	protected void CollectEntryTitles(notnull SCR_FieldManualConfigRoot root, notnull array<string> titles)
	{
		if (!root.m_aCategories)
			return;

		foreach (SCR_FieldManualConfigCategory category : root.m_aCategories)
		{
			if (!category)
				continue;

			CollectCategoryEntryTitles(category, titles);

			if (!category.m_aCategories)
				continue;

			foreach (SCR_FieldManualConfigCategory subCategory : category.m_aCategories)
			{
				if (subCategory)
					CollectCategoryEntryTitles(subCategory, titles);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] category The category whose direct entries are wanted.
	//! \param[out] titles Receives each entry's title key.
	protected void CollectCategoryEntryTitles(notnull SCR_FieldManualConfigCategory category, notnull array<string> titles)
	{
		if (!category.m_aEntries)
			return;

		foreach (SCR_FieldManualConfigEntry entry : category.m_aEntries)
		{
			if (entry)
				titles.Insert(entry.m_sTitle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] root The merged root. Assumed non-null.
	//! \return A one-line human-readable inventory for the run log.
	protected string DescribeRoot(notnull SCR_FieldManualConfigRoot root)
	{
		int categoryCount;
		if (root.m_aCategories)
			categoryCount = root.m_aCategories.Count();

		int backgroundCount;
		if (root.m_aTileBackgrounds)
			backgroundCount = root.m_aTileBackgrounds.Count();

		array<string> titles = {};
		CollectEntryTitles(root, titles);

		array<string> overthrowSubCategories = {};
		CollectSubCategoryTitles(root, OVERTHROW_CATEGORY_TITLE, overthrowSubCategories);

		return categoryCount.ToString() + " categories [" + JoinCategoryTitles(root) + "], "
			+ backgroundCount.ToString() + " tile backgrounds, "
			+ titles.Count().ToString() + " entries, Overthrow sub-categories ["
			+ JoinStrings(overthrowSubCategories) + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] root The merged root. Assumed non-null.
	//! \param[in] categoryTitle Title key of the top-level category to look under.
	//! \param[out] titles Receives that category's sub-category title keys.
	protected void CollectSubCategoryTitles(notnull SCR_FieldManualConfigRoot root, string categoryTitle, notnull array<string> titles)
	{
		if (!root.m_aCategories)
			return;

		foreach (SCR_FieldManualConfigCategory category : root.m_aCategories)
		{
			if (!category || category.m_sTitle != categoryTitle || !category.m_aCategories)
				continue;

			foreach (SCR_FieldManualConfigCategory subCategory : category.m_aCategories)
			{
				if (subCategory)
					titles.Insert(subCategory.m_sTitle);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] root The merged root. Assumed non-null.
	//! \return The top-level category titles, comma separated.
	protected string JoinCategoryTitles(notnull SCR_FieldManualConfigRoot root)
	{
		array<string> titles = {};

		if (root.m_aCategories)
		{
			foreach (SCR_FieldManualConfigCategory category : root.m_aCategories)
			{
				if (category)
					titles.Insert(category.m_sTitle);
			}
		}

		return JoinStrings(titles);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] values Strings to join. May be empty.
	//! \return The values comma separated, or "(none)" when there are none.
	protected string JoinStrings(notnull array<string> values)
	{
		if (values.IsEmpty())
			return "(none)";

		string joined = values.Get(0);

		for (int i = 1; i < values.Count(); i++)
		{
			joined = joined + ", " + values.Get(i);
		}

		return joined;
	}
}

//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve : SCR_AutotestCaseBase
{
	//! Flipped true on 2026-08-09 when the five configs were deleted. This is now a PERMANENT
	//! regression guard, not a spent one: it asserts none of the five retired ids ever comes back.
	//! Re-adding a config carrying one would make a version-1 save's dropped records start resolving
	//! again, onto a job that is not the job they were saved on. Leave it true.
	static const bool RETIRED_IDS_ARE_DELETED = true;

	//! The seven job ids that survive starter-jobs-retirement. Literals on purpose - see the header.
	static const ref array<string> SURVIVING_LEGACY_IDS = {
		"assassinate-traitor",
		"base-recon",
		"raise-support",
		"propaganda-run",
		"pirate-radio",
		"sabotage-radio-tower",
		"assassinate-officer"
	};

	//! The five job ids starter-jobs-retirement removed on 2026-08-09. These must resolve to NOTHING.
	//!
	//! ⚠ These lists stay LITERAL and stay here - deliberately NOT pointed at
	//! OVT_JobManagerSerializer.LEGACY_V1_JOB_IDS. That table is POSITIONAL HISTORY for the version 1
	//! save format, frozen for good; these are expectations about the config list shipping NOW. Add a
	//! thirteenth job and the frozen table must not grow while SURVIVING_LEGACY_IDS should. Two
	//! independent copies are also what catches a .conf id rename that was "kept in sync". The frozen
	//! table is guarded separately by a Logic-tier case against its own literals.
	static const ref array<string> RETIRED_LEGACY_IDS = {
		"find-gun-dealer",
		"find-shop",
		"place-equipment-box",
		"recruit-a-civilian",
		"place-a-camp"
	};

	//! Every character a stable job id may contain.
	static const string LEGAL_ID_CHARACTERS = "abcdefghijklmnopqrstuvwxyz-";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if (!jobs)
		{
			SetFailure("OVT_Global.GetJobs() is null - see OVT_TEST_Init_Globals_ManagersResolve for the wiring this depends on.");
			return true;
		}

		int configCount = jobs.GetJobConfigCount();
		if (configCount < 1)
		{
			SetFailure("The job manager has no job configs at all (GetJobConfigCount() = %1) - m_aJobConfigs on Prefabs/GameMode/OVT_OverthrowGameMode.et is empty, so every assertion below would pass vacuously", configCount.ToString());
			return true;
		}

		string failure = FindFirstIdError(jobs, configCount);
		if (failure == "")
			failure = FindFirstLegacyResolveError(jobs);

		if (failure != "")
		{
			SetFailure("%1", failure);
			return true;
		}

		PrintFormat("Job stable ids: %1 configs, all non-empty, lowercase-kebab, unique and index<->id round-tripping; %2 surviving legacy ids resolve; retired-ids-deleted switch is %3",
			configCount.ToString(), SURVIVING_LEGACY_IDS.Count().ToString(), RETIRED_IDS_ARE_DELETED.ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks every configured job and describes the FIRST id problem found.
	//! \param[in] jobs The live job manager. Assumed non-null.
	//! \param[in] configCount The manager's config count. Assumed >= 1.
	//! \return A ready-to-report failure message, or an empty string when every id is sound.
	protected string FindFirstIdError(OVT_JobManagerComponent jobs, int configCount)
	{
		ref set<string> seenIds = new set<string>();

		for (int i = 0; i < configCount; i++)
		{
			string position = "Job config at index " + i.ToString();

			OVT_JobConfig config = jobs.GetConfig(i);
			if (!config)
				return position + " is null - an empty row was left in m_aJobConfigs on Prefabs/GameMode/OVT_OverthrowGameMode.et";

			if (config.m_sId == "")
				return position + " (title '" + config.m_sTitle + "') has an empty m_sId. The id is this job's identity in the save format - without it the job's board entries and both of its lifetime counter maps cannot be written, so the job silently disappears from every continued campaign. Author it in the job's .conf under Configs/Jobs/.";

			string shapeError = FindIdShapeError(config.m_sId);
			if (shapeError != "")
				return position + " has the id '" + config.m_sId + "', which " + shapeError + ". Job ids are short lowercase-kebab (e.g. 'raise-support'), the same convention as OVT_TutorialEntryConfig.m_sId, and they appear verbatim in the WARNING lines a bug reporter pastes.";

			if (seenIds.Contains(config.m_sId))
				return position + " repeats the id '" + config.m_sId + "'. Job ids must be unique: a duplicate makes one job's saved board entries and lifetime counters resolve to the OTHER job on load, which is exactly the silent mis-attachment the stable id exists to prevent.";

			seenIds.Insert(config.m_sId);

			// index -> id -> index. The two helpers are the save format's only translation points, so a
			// mapping that does not round-trip is a corrupted save waiting for the next load.
			string idAtIndex = jobs.GetJobIdByIndex(i);
			if (idAtIndex != config.m_sId)
				return position + ": GetJobIdByIndex(" + i.ToString() + ") returned '" + idAtIndex + "' but the config's m_sId is '" + config.m_sId + "'";

			int indexForId = jobs.FindJobIndexById(idAtIndex);
			if (indexForId != i)
				return position + ": the id '" + idAtIndex + "' resolved back to index " + indexForId.ToString() + " instead of " + i.ToString() + " - the index<->id mapping does not round-trip";
		}

		// Out-of-range contract, asserted rather than assumed: the deserializer relies on these exact
		// misses to decide that a saved record names nothing.
		if (jobs.GetJobIdByIndex(-1) != "")
			return "GetJobIdByIndex(-1) returned an id instead of an empty string";

		if (jobs.GetJobIdByIndex(configCount) != "")
			return "GetJobIdByIndex(" + configCount.ToString() + ") returned an id for an out-of-range index instead of an empty string";

		if (jobs.FindJobIndexById("") != -1)
			return "FindJobIndexById(\"\") matched a config. An empty id must never resolve, or a config that lost its id would start absorbing unrelated saved records.";

		if (jobs.FindJobIndexById("__ovt-no-such-job") != -1)
			return "FindJobIndexById() matched an id no config carries, so a saved record naming a deleted job would be restored onto some other job instead of being dropped";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Describes what is wrong with an id's SHAPE, as a sentence fragment following "which ...".
	//! \param[in] id The id to inspect. Assumed non-empty.
	//! \return A fragment describing the first shape problem, or an empty string when the id is legal.
	protected string FindIdShapeError(string id)
	{
		if (id.Get(0) == "-")
			return "starts with a hyphen";

		if (id.Get(id.Length() - 1) == "-")
			return "ends with a hyphen";

		if (id.Contains("--"))
			return "contains a double hyphen";

		for (int i = 0; i < id.Length(); i++)
		{
			string character = id.Get(i);
			if (!LEGAL_ID_CHARACTERS.Contains(character))
				return "contains the illegal character '" + character + "' at position " + i.ToString();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The rename guard, plus the Phase 4 switch.
	//!
	//! Surviving ids: every one must resolve to a live config, because a version-1 save on a player's
	//! disk already names them. Retired ids: gated by RETIRED_IDS_ARE_DELETED, and asserted in BOTH
	//! directions so that neither state can pass silently.
	//! \param[in] jobs The live job manager. Assumed non-null.
	//! \return A ready-to-report failure message, or an empty string when the legacy ids are as expected.
	protected string FindFirstLegacyResolveError(OVT_JobManagerComponent jobs)
	{
		for (int i = 0; i < SURVIVING_LEGACY_IDS.Count(); i++)
		{
			string survivingId = SURVIVING_LEGACY_IDS.Get(i);
			if (jobs.FindJobIndexById(survivingId) < 0)
				return "The surviving job id '" + survivingId + "' resolves to no config. Either its .conf under Configs/Jobs/ was renamed, its m_sId was edited, or the job was removed. That id is already written into saved campaigns, so every board entry and lifetime counter naming it would be DROPPED on the next load. Job ids are immutable once shipped - put it back.";
		}

		for (int i = 0; i < RETIRED_LEGACY_IDS.Count(); i++)
		{
			string retiredId = RETIRED_LEGACY_IDS.Get(i);
			int retiredIndex = jobs.FindJobIndexById(retiredId);

			if (RETIRED_IDS_ARE_DELETED)
			{
				if (retiredIndex >= 0)
					return "The retired job id '" + retiredId + "' still resolves, to index " + retiredIndex.ToString() + ". RETIRED_IDS_ARE_DELETED is set, so its .conf should be gone from Configs/Jobs/ and its entry gone from m_aJobConfigs on the game-mode prefab.";

				continue;
			}

			if (retiredIndex < 0)
				return "The retired job id '" + retiredId + "' no longer resolves, but RETIRED_IDS_ARE_DELETED in this case is still false. If the five starter jobs have just been deleted (starter-jobs-retirement Phase 4), flip that constant to true - that is task 4.5 and this message is the reminder. If they have NOT been deleted, a job config has lost its id and the version-1 save conversion can no longer be exercised against it.";
		}

		return "";
	}
}


//------------------------------------------------------------------------------------------------
//! The virtualization manager is on the game-mode prefab, initialised, and empty before anything
//! registers.
//!
//! Resolution pins the prefab wiring: without the component, every OVT_Global.GetVirtualization()
//! returns null. The empty-registry claim pins the SERVER GUARD's ordering - OnPostInit allocates
//! the record map only after Replication.IsServer(), so a count of 0 (rather than a null-map crash
//! or a stale count) is the evidence the collection exists and starts clean.
//!
//! Init tier, not campaign tier: both facts are true at world load.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_ManagerResolvesEmpty : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - OVT_VirtualizationManagerComponent is missing from the game-mode prefab, so the whole virtualization epic has no entry point");
			return true;
		}

		int count = virtualization.GetGroupCount();
		if (count != 0)
		{
			SetFailure("The virtualization registry holds %1 group(s) before anything registered - core ships with no consumers, so this must be 0", count.ToString());
			return true;
		}

		// An unknown handle answers "unknown" rather than dereferencing anything.
		if (virtualization.IsRegistered(1))
		{
			SetFailure("IsRegistered(1) is true on an empty registry");
			return true;
		}

		if (virtualization.GetRecord(1))
		{
			SetFailure("GetRecord(1) returned a record on an empty registry");
			return true;
		}

		array<int> ownedHandles = virtualization.FindGroupsByOwner("test", "nothing");
		if (!ownedHandles || !ownedHandles.IsEmpty())
		{
			SetFailure("FindGroupsByOwner on an empty registry did not return an empty array");
			return true;
		}

		Print("Virtualization manager resolved and its registry is empty");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! virtualizationSpawnDistance exists in the config struct and defaults to 1750 m.
//!
//! Issue #100's operator-facing knob. ⚠ The default is asserted because a field silently missing
//! from SetDefaults() would read back 0 - the legitimate "never materialise" value - so nothing
//! would look broken until an entire campaign's AI failed to appear.
//!
//! GetGlobalSpawnDistance() must agree with the config, so a mis-wired accessor cannot pass by
//! falling back to its attribute default.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_SpawnDistanceDefault : SCR_AutotestCaseBase
{
	//! The default written by OVT_OverthrowConfigStruct.SetDefaults().
	protected const int EXPECTED_DEFAULT = 1750;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		if (!config.m_ConfigFile)
		{
			SetFailure("The config component has no loaded config struct");
			return true;
		}

		if (config.m_ConfigFile.virtualizationSpawnDistance != EXPECTED_DEFAULT)
		{
			SetFailure("virtualizationSpawnDistance read back %1, expected the %2 m default - a 0 here would mean 'never materialise' for every registered group",
				config.m_ConfigFile.virtualizationSpawnDistance.ToString(), EXPECTED_DEFAULT.ToString());
			return true;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		if (virtualization.GetGlobalSpawnDistance() != EXPECTED_DEFAULT)
		{
			SetFailure("The manager resolved a global spawn distance of %1 while the config says %2",
				virtualization.GetGlobalSpawnDistance().ToString(), EXPECTED_DEFAULT.ToString());
			return true;
		}

		Print("virtualizationSpawnDistance defaults to 1750 m and the manager reads it back");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! RegisterGroup refuses a composition it cannot resolve, and books nothing when it does.
//!
//! Composition is (factionKey, groupName) - never a faction index, which is positional across saves.
//! Both halves fail independently: a removed faction mod gives an unknown KEY, a renamed registry
//! entry an unknown GROUP NAME. Both must return -1 with a warning, because a booked-but-unbuildable
//! record would be persisted and dropped again on every subsequent load.
//!
//! The known-faction half discovers a REAL faction key from the faction manager rather than
//! hard-coding "USSR", so a faction rename fails where it belongs.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_RegisterRefusesUnknownComposition : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		int before = virtualization.GetGroupCount();

		// A faction key nothing defines - the "a faction mod was removed" shape (R3).
		int handle = virtualization.RegisterGroup("test", "unknown_faction", "OVT_NO_SUCH_FACTION", "light_patrol", vector.Zero);
		if (handle != -1)
		{
			SetFailure("RegisterGroup with an unknown faction key returned handle %1, expected -1", handle.ToString());
			return true;
		}

		if (virtualization.GetGroupCount() != before)
		{
			SetFailure("A refused registration still booked a record: the registry went from %1 to %2",
				before.ToString(), virtualization.GetGroupCount().ToString());
			return true;
		}

		// A real faction, discovered rather than assumed, with a group name it does not define.
		string knownKey = FindKnownFactionKey();
		if (knownKey == "")
		{
			SetFailure("No faction in the test world has Overthrow faction data, so the unknown-group-name half cannot be exercised");
			return true;
		}

		handle = virtualization.RegisterGroup("test", "unknown_group", knownKey, "OVT_NO_SUCH_GROUP_NAME", vector.Zero);
		if (handle != -1)
		{
			SetFailure("RegisterGroup on faction '%1' with an unknown group name returned handle %2, expected -1", knownKey, handle.ToString());
			return true;
		}

		if (virtualization.GetGroupCount() != before)
		{
			SetFailure("An unresolvable group name still booked a record: the registry went from %1 to %2",
				before.ToString(), virtualization.GetGroupCount().ToString());
			return true;
		}

		PrintFormat("RegisterGroup refused an unknown faction key and an unknown group name on real faction '%1'", knownKey);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds a faction key that resolves to Overthrow faction data in this world.
	//! \return The first such key, or an empty string when there is none.
	protected string FindKnownFactionKey()
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "";

		int count = factions.GetFactionsCount();
		for (int i = 0; i < count; i++)
		{
			Faction faction = factions.GetFactionByIndex(i);
			if (!faction)
				continue;

			string key = faction.GetFactionKey();
			if (factions.GetOverthrowFactionByKey(key))
				return key;
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Shared discovery helpers for the Phase 3 virtualization cases.
//!
//! Every case below needs a composition the CURRENT world can resolve; hard-coding
//! "USSR"/"light_patrol" would turn a faction-config rename into a false red here instead of in the
//! faction tests. These helpers ask the live registries instead.
//------------------------------------------------------------------------------------------------
class OVT_TEST_VirtualizationFixture
{
	//! Owner system tag used by every case here, so a leaked record is obvious in a log.
	static const string OWNER_SYSTEM = "test_virtualization";

	//------------------------------------------------------------------------------------------------
	//! Finds a (factionKey, groupName) pair that resolves to a real group prefab in this world.
	//! \param[out] factionKey The faction key found, or unchanged when there is none.
	//! \param[out] groupName The group registry name found, or unchanged when there is none.
	//! \return True when a resolvable composition was found.
	static bool FindComposition(out string factionKey, out string groupName)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return false;

		int count = factions.GetFactionsCount();
		for (int i = 0; i < count; i++)
		{
			Faction faction = factions.GetFactionByIndex(i);
			if (!faction)
				continue;

			string key = faction.GetFactionKey();
			OVT_Faction overthrowFaction = factions.GetOverthrowFactionByKey(key);
			if (!overthrowFaction)
				continue;

			array<string> names = overthrowFaction.GetAvailableGroupNames();
			if (!names)
				continue;

			foreach (string name : names)
			{
				if (overthrowFaction.GetGroupPrefabByName(name) == ResourceName.Empty)
					continue;

				factionKey = key;
				groupName = name;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every group registry name a faction defines, so a case that needs a roster of a given size can
	//! look for one instead of assuming.
	//! \param[in] factionKey Faction to ask.
	//! \return The names, never null.
	static array<string> ListGroupNames(string factionKey)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return new array<string>();

		OVT_Faction overthrowFaction = factions.GetOverthrowFactionByKey(factionKey);
		if (!overthrowFaction)
			return new array<string>();

		array<string> names = overthrowFaction.GetAvailableGroupNames();
		if (!names)
			return new array<string>();

		return names;
	}

	//------------------------------------------------------------------------------------------------
	//! A world position with terrain under it. The first registered town, because a group registered
	//! at the world origin may sit on nothing at all - and one case tries to materialise a member.
	//! \return A usable registration position.
	static vector PickPosition()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns && !towns.m_Towns.IsEmpty())
			return towns.m_Towns[0].location;

		return vector.Zero;
	}
}

//------------------------------------------------------------------------------------------------
//! RegisterGroup builds a real, DORMANT group entity carrying the engine lifecycle stamps.
//!
//! A registration is not a booking - it is a group entity in the world with ZERO member characters
//! that hands its spawn/despawn decisions to the engine. Six facts, six ways to fail silently:
//!   - the entity exists at all (IgnoreSpawning + prefab spawn),
//!   - it has NO members (a failed IgnoreSpawning materialises a squad at registration time, far
//!     from any player, and nothing else would notice),
//!   - the policy is ProximityDriven (Manual would mean the group never appears),
//!   - the distances read back the resolved ring, and despawn > spawn (equal values flap),
//!   - the importance is the tier asked for, never vanilla's silent LOW default,
//!   - the roster was captured into an all-alive mask (an empty mask makes the group immortal).
//!
//! Safe at Init tier: an unobserved ProximityDriven group stays memberless, so nothing materialises
//! while the case runs.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_RegisterBuildsDormantGroup : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so registration cannot be exercised");
			return true;
		}

		int before = virtualization.GetGroupCount();
		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		int handle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "dormant_case",
			factionKey, groupName, position, null, -1, SCR_EAISpawnImportance.HIGH);

		string failure = Verify(virtualization, handle, before, position);

		// Cleanup BEFORE reporting, so a red assertion cannot leak a record into the cases after it.
		if (handle != -1)
			virtualization.UnregisterGroup(handle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		if (virtualization.GetGroupCount() != before)
		{
			SetFailure("UnregisterGroup left the registry at %1 records, expected %2",
				virtualization.GetGroupCount().ToString(), before.ToString());
			return true;
		}

		if (virtualization.IsRegistered(handle) || virtualization.GetGroup(handle))
		{
			SetFailure("Handle %1 still resolves after UnregisterGroup", handle.ToString());
			return true;
		}

		PrintFormat("RegisterGroup built a dormant %1 '%2' group with the engine lifecycle stamped, and UnregisterGroup dropped it", factionKey, groupName);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, int handle, int before, vector position)
	{
		if (handle == -1)
			return "RegisterGroup returned -1 for a composition the faction registry resolves";

		if (virtualization.GetGroupCount() != before + 1)
			return string.Format("The registry holds %1 records after one registration, expected %2",
				virtualization.GetGroupCount().ToString(), (before + 1).ToString());

		if (!virtualization.IsRegistered(handle))
			return "IsRegistered() is false for the handle RegisterGroup just returned";

		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return "GetGroup() is null - RegisterGroup booked a record without building the group entity";

		if (group.GetAgentsCount() != 0)
			return string.Format("The freshly registered group already has %1 member(s) - IgnoreSpawning did not take",
				group.GetAgentsCount().ToString());

		if (virtualization.IsSpawned(handle))
			return "IsSpawned() is true for a group with no members";

		if (group.GetLifecyclePolicy() != SCR_EAIGroupLifecyclePolicy.ProximityDriven)
			return string.Format("The group's lifecycle policy is %1, expected ProximityDriven (%2) - a Manual group never materialises on approach",
				group.GetLifecyclePolicy().ToString(), SCR_EAIGroupLifecyclePolicy.ProximityDriven.ToString());

		int expectedSpawn = virtualization.GetSpawnDistance(handle);
		if (Math.AbsFloat(group.GetSpawnDistance() - expectedSpawn) > 1)
			return string.Format("The group's spawn distance is %1 m, expected the resolved %2 m",
				group.GetSpawnDistance().ToString(), expectedSpawn.ToString());

		if (group.GetDespawnDistance() <= group.GetSpawnDistance())
			return string.Format("Despawn distance %1 m is not beyond spawn distance %2 m - without that band the group flaps at the boundary",
				group.GetDespawnDistance().ToString(), group.GetSpawnDistance().ToString());

		if (group.GetImportance() != SCR_EAISpawnImportance.HIGH)
			return string.Format("The group reports importance %1, expected the HIGH (%2) it was registered with - an unstamped group sits at vanilla's LOW tier and is evicted first",
				group.GetImportance().ToString(), SCR_EAISpawnImportance.HIGH.ToString());

		if (virtualization.GetImportance(handle) != SCR_EAISpawnImportance.HIGH)
			return string.Format("The record reports importance %1, expected HIGH", virtualization.GetImportance(handle).ToString());

		int roster = virtualization.GetMemberCount(handle);
		if (roster < 1)
			return "GetMemberCount() is 0 - the roster was never captured, so the survivor mask is inert and every death would be dropped";

		for (int slot = 0; slot < roster; slot++)
		{
			if (!virtualization.GetMemberAlive(handle, slot))
				return string.Format("Slot %1 of a freshly registered group is already dead", slot.ToString());
		}

		if (virtualization.GetAliveMemberCount(handle) != roster)
			return string.Format("GetAliveMemberCount() is %1 for a %2-slot group nobody has shot at",
				virtualization.GetAliveMemberCount(handle).ToString(), roster.ToString());

		if (vector.Distance(virtualization.GetPosition(handle), position) > 1)
			return string.Format("The group is at %1, registered at %2",
				virtualization.GetPosition(handle).ToString(), position.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! GetAllHandles() enumerates the WHOLE registry, and stops returning a handle the moment it is
//! unregistered.
//!
//! A tick advancing every dormant registered group cannot be built on FindGroupsByOwner, because
//! ownerSystem is a deliberately free-form mod-extensible string. Three silent failures, three
//! assertions: the enumeration misses a handle (a frozen patrol looks like no patrol); the count
//! disagrees with GetGroupCount(); an unregistered handle keeps coming back.
//!
//! TWO groups are registered because a single-element answer cannot distinguish "returns the whole
//! registry" from "returns the newest record". Nothing asserts array length or ORDER - the order
//! genuinely is a map's and is not stable, so asserting one would assert a lie.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_GetAllHandlesEnumeratesRegistry : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so the registry cannot be enumerated");
			return true;
		}

		int before = virtualization.GetGroupCount();
		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		int first = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "handles_case_a",
			factionKey, groupName, position);
		int second = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "handles_case_b",
			factionKey, groupName, position);

		string failure = Verify(virtualization, first, second, before);

		// Cleanup BEFORE reporting, so a red assertion cannot leak two records into the cases after it.
		if (first != -1)
			virtualization.UnregisterGroup(first);

		if (second != -1)
			virtualization.UnregisterGroup(second);

		// Only worth asking once the registered half held - otherwise the first broken claim is the
		// one worth reporting.
		if (failure == "")
			failure = VerifyGone(virtualization, first, second, before);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("GetAllHandles() enumerated the whole registry including handles %1 and %2, and dropped both on unregister",
			first.ToString(), second.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the enumeration holds, or the first broken claim.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, int first, int second, int before)
	{
		if (first == -1 || second == -1)
			return string.Format("RegisterGroup returned %1 / %2 for a composition the faction registry resolves - there is nothing to enumerate",
				first.ToString(), second.ToString());

		if (first == second)
			return string.Format("Both registrations returned handle %1 - handles come from a monotonic counter and are never reused",
				first.ToString());

		array<int> handles = virtualization.GetAllHandles();
		if (!handles)
			return "GetAllHandles() returned null - every finder on this manager answers with an empty array instead, so a caller may not have to null-check";

		if (handles.Find(first) == -1)
			return string.Format("GetAllHandles() returned %1 handle(s) and handle %2 is not among them, although IsRegistered() answers %3 for it",
				handles.Count().ToString(), first.ToString(), virtualization.IsRegistered(first).ToString());

		if (handles.Find(second) == -1)
			return string.Format("GetAllHandles() returned %1 handle(s) and handle %2 is not among them, although IsRegistered() answers %3 for it",
				handles.Count().ToString(), second.ToString(), virtualization.IsRegistered(second).ToString());

		if (handles.Count() != virtualization.GetGroupCount())
			return string.Format("GetAllHandles() returned %1 handle(s) but GetGroupCount() says %2 - the two read the same registry, so one of them is wrong",
				handles.Count().ToString(), virtualization.GetGroupCount().ToString());

		if (handles.Count() != before + 2)
			return string.Format("GetAllHandles() returned %1 handle(s) after two registrations, expected the %2 this case started with plus 2",
				handles.Count().ToString(), before.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when both handles have really left the enumeration.
	protected string VerifyGone(notnull OVT_VirtualizationManagerComponent virtualization, int first, int second, int before)
	{
		array<int> handles = virtualization.GetAllHandles();
		if (!handles)
			return "GetAllHandles() returned null after both records were unregistered";

		if (handles.Find(first) != -1)
			return string.Format("GetAllHandles() still returns handle %1 after UnregisterGroup - a consumer's tick would keep working a record that no longer exists",
				first.ToString());

		if (handles.Find(second) != -1)
			return string.Format("GetAllHandles() still returns handle %1 after UnregisterGroup - a consumer's tick would keep working a record that no longer exists",
				second.ToString());

		if (handles.Count() != before)
			return string.Format("GetAllHandles() returned %1 handle(s) after unregistering both, expected the %2 this case started with",
				handles.Count().ToString(), before.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Shared fixture for the three virtual-MOVEMENT cases below.
//!
//! Separate from OVT_TEST_VirtualizationFixture: these cases consume core rather than the movement
//! manager's internals, and need a leg entirely on LAND plus a plan whose types decide whether the
//! group may walk at all.
//!
//! ⚠ The leg has to be PICKED, not assumed. Movement's water rule advances the accumulator through
//! water but WRITES NOTHING while it is over water, so a leg pointed into a bay leaves the group's
//! origin where it started - a false red. IsOceanAtPosition is the manager's own predicate.
//------------------------------------------------------------------------------------------------
class OVT_TEST_VirtualMovementFixture
{
	//! Owner system tag, so a leaked record is obvious in a log and cannot be confused with the
	//! registration cases' own.
	static const string OWNER_SYSTEM = "test_virtual_movement";

	//! Leg length in metres. Far enough that ~10 s of walking at the default 1.5 m/s cannot arrive
	//! (ARRIVAL_RADIUS_M is 10 m), so "moved less than the whole leg" stays a real claim.
	static const float LEG_LENGTH_M = 200;

	//! Wall-clock observation window in ms. At the default 2000 ms cadence that is ~5 passes, and the
	//! FIRST pass over a handle only derives its state (its dt is 0 by construction), so at the default
	//! 1.5 m/s the group has ~4 advances - around 12 m - to be seen moving. Bounded, and NOT a retry
	//! budget: each case asserts exactly once, when it has its answer or the window is spent.
	static const float OBSERVE_WINDOW_MS = 10000;

	//------------------------------------------------------------------------------------------------
	//! A point LEG_LENGTH_M away from origin whose whole leg is on land.
	//!
	//! Eight compass directions are tried in turn and each candidate leg is sampled at five points; the
	//! first leg with no ocean sample wins.
	//! \param[in] origin Where the group is registered.
	//! \return The target point, or vector.Zero when this world offers no all-land leg from here.
	static vector PickLandTarget(vector origin)
	{
		array<vector> directions = new array<vector>();
		directions.Insert(Vector(1, 0, 0));
		directions.Insert(Vector(0, 0, 1));
		directions.Insert(Vector(-1, 0, 0));
		directions.Insert(Vector(0, 0, -1));
		directions.Insert(Vector(0.7071, 0, 0.7071));
		directions.Insert(Vector(-0.7071, 0, 0.7071));
		directions.Insert(Vector(0.7071, 0, -0.7071));
		directions.Insert(Vector(-0.7071, 0, -0.7071));

		foreach (vector direction : directions)
		{
			vector candidate = origin;
			candidate[0] = origin[0] + direction[0] * LEG_LENGTH_M;
			candidate[2] = origin[2] + direction[2] * LEG_LENGTH_M;

			if (LegIsOnLand(origin, candidate))
				return candidate;
		}

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] from Leg start.
	//! \param[in] to Leg end.
	//! \return True when five evenly spaced samples along the leg are all on land.
	static bool LegIsOnLand(vector from, vector to)
	{
		for (int sample = 0; sample <= 4; sample++)
		{
			float fraction = sample / 4.0;

			vector point = from;
			point[0] = from[0] + (to[0] - from[0]) * fraction;
			point[2] = from[2] + (to[2] - from[2]) * fraction;

			if (OVT_WorldUtils.IsOceanAtPosition(point))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A two-point plan of one waypoint type.
	//!
	//! The TYPE is the whole experiment: PATROL/MOVE make the plan movable and DEFEND makes it
	//! stationary, which is the D10 opt-in contract the two cases below assert from either side. The
	//! plan is deliberately NOT cycling - two points and a ping-pong are all the route these cases
	//! need, and a cycling plan would add a third (closing) leg to reason about.
	//! \param[in] first Where the group is registered.
	//! \param[in] second The far point.
	//! \param[in] type OVT_EVirtualWaypointType applied to both points.
	//! \return A fresh plan.
	static OVT_VirtualWaypointPlan BuildPlan(vector first, vector second, int type)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		plan.m_aPositions.Insert(first);
		plan.m_aTypes.Insert(type);
		plan.m_aParams.Insert(25);

		plan.m_aPositions.Insert(second);
		plan.m_aTypes.Insert(type);
		plan.m_aParams.Insert(25);

		plan.m_bCycle = false;

		return plan;
	}
}

//------------------------------------------------------------------------------------------------
//! THE MOVEMENT TICK ACTUALLY ADVANCES A DORMANT GROUP, TOWARD ITS PLAN.
//!
//! The one end-to-end claim of `virtualization/movement`: enumeration, round-robin slice, lazy state
//! derivation, per-group dt, arrival maths and ground-snapped write all wired together and installed
//! on the game mode. Every other case asserts one piece in isolation.
//!
//! A group is registered with a two-point PATROL/MOVE plan 200 m out, then polled for up to 10 s:
//! XZ displacement exceeds 1 m (it moved), is less than the whole leg (no teleport), and ends closer
//! to the target (it moved TOWARD the plan). No exact distance boundary is asserted - vector.Distance
//! is +1 ULP off at 1000 m and 2000 m, and the step depends on how many passes the window contained.
//!
//! ⚠ The FIRST pass over a handle cannot move it: state is derived on first touch and stamped with
//! the current world time, so its dt is 0 by construction. The window is sized for several after it.
//!
//! Cleanup before reporting: this is the one case that deliberately registers a group that MOVES.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_VirtualMovement_TickAdvancesDormantGroup : SCR_AutotestCaseBase
{
	//! XZ metres the group must cover to count as advanced.
	static const float MIN_DISPLACEMENT_M = 1;

	protected int m_iPhase;
	protected int m_iHandle = -1;
	protected float m_fDeadlineMs;

	//! Where the group actually was once registered (NOT the requested position - core ground-snaps).
	protected vector m_vStart;

	//! The far point of the plan; the direction "toward" is measured against.
	protected vector m_vTarget;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitAdvance();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the walking group and opens the observation window.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Arrange()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		if (!OVT_Global.GetVirtualMovement())
		{
			SetFailure("OVT_Global.GetVirtualMovement() is null - the movement manager is missing from the game-mode prefab, so no registered group can ever advance");
			return true;
		}

		// Init-tier worlds never press Start (RequiresStartedCampaign() is false for this suite), so
		// DoStartGame()/PostGameStart() never ran here and the movement CallLater was never installed.
		// PostGameStart() is public and idempotent (m_bTickRunning latch), so the case installs the
		// exact tick it is about to assert against - without it the group sits still and this case
		// reds with "the tick is not advancing registered groups at all".
		OVT_Global.GetVirtualMovement().PostGameStart();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("There is no world to walk a group across");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a walking group cannot be registered");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		m_vTarget = OVT_TEST_VirtualMovementFixture.PickLandTarget(position);

		if (m_vTarget == vector.Zero)
		{
			SetFailure(string.Format("No %1 m leg out of %2 is entirely on land in this world, so movement's water rule would veto every write and 'the group moved' could not be asserted",
				OVT_TEST_VirtualMovementFixture.LEG_LENGTH_M.ToString(), position.ToString()));
			return true;
		}

		// spawnDistanceOverride 0 = the MANUAL lifecycle policy, so the engine never materialises the
		// group by proximity. ⚠ Dormant by construction is required: the autotest camera IS an observer
		// and at the global ring can spawn a test group's members, at which point the IsSpawned gate
		// refuses to advance and this case reds with "not advancing at all".
		m_iHandle = virtualization.RegisterGroup(OVT_TEST_VirtualMovementFixture.OWNER_SYSTEM, "movement_walks",
			factionKey, groupName, position,
			OVT_TEST_VirtualMovementFixture.BuildPlan(position, m_vTarget, OVT_EVirtualWaypointType.PATROL), 0);

		if (m_iHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a composition the faction registry resolves, so there is nothing to advance");
			return true;
		}

		m_vStart = virtualization.GetPosition(m_iHandle);
		m_fDeadlineMs = world.GetWorldTime() + OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS;

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the group has visibly moved, or the window is spent.
	//! \return True when the case is finished.
	protected bool AwaitAdvance()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		BaseWorld world = GetGame().GetWorld();

		if (!virtualization || !world)
		{
			CleanUp();
			SetFailure("The virtualization manager or the world went away while the case was watching a group walk");
			return true;
		}

		vector current = virtualization.GetPosition(m_iHandle);
		float moved = OVT_VirtualMovementMath.DistanceXZ(current, m_vStart);
		bool expired = world.GetWorldTime() >= m_fDeadlineMs;

		if (moved <= MIN_DISPLACEMENT_M && !expired)
			return false;

		string failure = Verify(current, moved);

		// Cleanup BEFORE reporting: this case registers the one group in the tree that deliberately
		// MOVES, and a red assertion must not leak it into the cases after it.
		CleanUp();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("The movement tick walked a dormant group %1 m of its %2 m leg within the observation window",
			moved.ToString(), OVT_VirtualMovementMath.DistanceXZ(m_vStart, m_vTarget).ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] current Where the group is now.
	//! \param[in] moved XZ metres covered since registration.
	//! \return An empty string when the group walked toward its plan, or the first broken claim.
	protected string Verify(vector current, float moved)
	{
		float leg = OVT_VirtualMovementMath.DistanceXZ(m_vStart, m_vTarget);

		if (moved <= MIN_DISPLACEMENT_M)
		{
			int tracked = 0;
			OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
			if (movement)
				tracked = movement.GetTrackedCount();

			return string.Format("A dormant group with a movable %1 m plan moved %2 m in %3 ms (%4 handle(s) tracked) - the tick is not advancing registered groups at all",
				leg.ToString(), moved.ToString(), OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS.ToString(), tracked.ToString());
		}

		if (moved >= leg)
			return string.Format("The group covered %1 m of a %2 m leg - a single pass may never cover the whole route, which is what an unclamped dt or a broken step clamp would do",
				moved.ToString(), leg.ToString());

		if (OVT_VirtualMovementMath.DistanceXZ(current, m_vTarget) >= leg)
			return string.Format("The group moved %1 m but is %2 m from its target, no closer than the %3 m it started at - it is walking somewhere, just not toward its plan",
				moved.ToString(), OVT_VirtualMovementMath.DistanceXZ(current, m_vTarget).ToString(), leg.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the walking group on every exit path.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterGroup(m_iHandle);

		m_iHandle = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! A DEFEND-ONLY PLAN IS NEVER ADVANCED - the D10 opt-in contract, asserted from the other side.
//!
//! "The plan IS the opt-in": there is no flag to set and no core field to check, so this
//! classification is the only thing between a tower garrison and a stroll across the map.
//!
//! Deliberately the same shape as the case above with ONE variable changed - the waypoint type - so
//! the pair is a controlled experiment: both red means the tick is dead, this one alone means the
//! classification is.
//!
//! Tolerance 0.5 m of XZ: below the smallest step a pass could take (1.5 m/s x 2 s) and above
//! floating-point noise. No exact boundary is asserted.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced : SCR_AutotestCaseBase
{
	//! XZ metres a DEFEND group is allowed to drift over the whole window.
	static const float MAX_DRIFT_M = 0.5;

	protected int m_iPhase;
	protected int m_iHandle = -1;
	protected float m_fDeadlineMs;
	protected vector m_vStart;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitWindow();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the garrison and opens the observation window.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Arrange()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		if (!OVT_Global.GetVirtualMovement())
		{
			SetFailure("OVT_Global.GetVirtualMovement() is null - with no movement manager 'a DEFEND plan is never advanced' would be asserted against nothing that could advance it");
			return true;
		}

		// Same tick-install as the walking case (Init-tier worlds never run PostGameStart themselves):
		// without a LIVE tick this case would pass vacuously - "never advanced" by a tick that never ran.
		// PostGameStart() is idempotent, so installing it here is safe whatever ran before.
		OVT_Global.GetVirtualMovement().PostGameStart();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("There is no world to hold a garrison still in");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a garrison cannot be registered");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		// The SAME all-land leg the walking case uses: the only variable between the two cases must be
		// the waypoint type, so a red here can never be blamed on a leg pointed into a bay.
		vector target = OVT_TEST_VirtualMovementFixture.PickLandTarget(position);
		if (target == vector.Zero)
		{
			SetFailure(string.Format("No %1 m leg out of %2 is entirely on land in this world, so this case would not be the same experiment as the walking one",
				OVT_TEST_VirtualMovementFixture.LEG_LENGTH_M.ToString(), position.ToString()));
			return true;
		}

		// spawnDistanceOverride 0 = Manual policy, dormant by construction - same reasoning and same
		// controlled-pair discipline as the walking case: the only variable between the two is the type.
		m_iHandle = virtualization.RegisterGroup(OVT_TEST_VirtualMovementFixture.OWNER_SYSTEM, "movement_garrison",
			factionKey, groupName, position,
			OVT_TEST_VirtualMovementFixture.BuildPlan(position, target, OVT_EVirtualWaypointType.DEFEND), 0);

		if (m_iHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a composition the faction registry resolves, so there is nothing to hold still");
			return true;
		}

		m_vStart = virtualization.GetPosition(m_iHandle);
		m_fDeadlineMs = world.GetWorldTime() + OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS;

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits out the WHOLE window - a garrison that leaves late is still a garrison that leaves.
	//! \return True when the case is finished.
	protected bool AwaitWindow()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		BaseWorld world = GetGame().GetWorld();

		if (!virtualization || !world)
		{
			CleanUp();
			SetFailure("The virtualization manager or the world went away while the case was watching a garrison stand still");
			return true;
		}

		if (world.GetWorldTime() < m_fDeadlineMs)
			return false;

		float drift = OVT_VirtualMovementMath.DistanceXZ(virtualization.GetPosition(m_iHandle), m_vStart);

		// Cleanup BEFORE reporting, the suite's rule.
		CleanUp();

		if (drift > MAX_DRIFT_M)
		{
			SetFailure(string.Format("A DEFEND-only group drifted %1 m in %2 ms - the plan is the ONLY opt-in movement has, so a garrison with a classification bug walks off its post and nothing else in the tree notices",
				drift.ToString(), OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS.ToString()));
			return true;
		}

		PrintFormat("A DEFEND-only group held its position (%1 m of drift) across the whole %2 ms window",
			drift.ToString(), OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the garrison on every exit path.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterGroup(m_iHandle);

		m_iHandle = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! THE MOVEMENT MANAGER RESOLVES, AND ITS TRANSIENT STATE DOES NOT LEAK.
//!
//! OVT_Global.GetVirtualMovement() answering is the only proof in the suites that the manager is on
//! the game-mode prefab (it was text-wired, not added in Workbench); every other movement claim is
//! silently vacuous without it. The tracked count must return to 0 once nothing is registered.
//!
//! ⚠ A group whose plan is empty, null or DEFEND-only keeps NO entry in the transient map - it is
//! re-classified cheaply each pass instead, which is what keeps the map empty in a campaign full of
//! garrisons. A group that latches stationary at RUNTIME, having reached a DEFEND point, KEEPS its
//! entry on purpose: dropping it would let the next pass re-derive a movable plan and walk the group
//! off the post it just took up. This case does not exercise that path and must not be "fixed" to
//! expect 0 for it.
//!
//! The count is settled with a bounded poll, and that wait IS the no-leak assertion.
//! GetTrackedCount() is a read-only diagnostic, not part of any API.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_VirtualMovement_ManagerResolvesAndDoesNotLeak : SCR_AutotestCaseBase
{
	//! Wall-clock ms allowed for the tick to drop the previous cases' progress. Several passes at the
	//! 2000 ms default; bounded, and not a retry budget.
	static const float SETTLE_WINDOW_MS = 10000;

	protected int m_iPhase;
	protected int m_iHandle = -1;
	protected float m_fDeadlineMs;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Begin();

		if (m_iPhase == 1)
			return AwaitEmptyState();

		return AwaitGarrisonWindow();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the manager and opens the settle window.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Begin()
	{
		OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
		if (!movement)
		{
			SetFailure("OVT_Global.GetVirtualMovement() is null - the movement manager is not on the game-mode prefab, so every other movement claim in these suites is vacuous");
			return true;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("There is no world, so the movement tick has nothing to run against");
			return true;
		}

		// Init-tier worlds never run PostGameStart; install the tick (idempotent) so the no-leak claim
		// is made against a manager whose tick is actually running and purging.
		movement.PostGameStart();

		m_fDeadlineMs = world.GetWorldTime() + SETTLE_WINDOW_MS;
		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the tracked count to return to 0 now that the cases above have unregistered their
	//! groups - the no-leak claim (Q7).
	//! \return True when the case is finished.
	protected bool AwaitEmptyState()
	{
		OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
		BaseWorld world = GetGame().GetWorld();

		if (!movement || !world)
		{
			SetFailure("The movement manager or the world went away while the case was waiting for its state map to empty");
			return true;
		}

		int tracked = movement.GetTrackedCount();
		if (tracked > 0 && world.GetWorldTime() < m_fDeadlineMs)
			return false;

		if (tracked > 0)
		{
			SetFailure(string.Format("The movement manager still tracks %1 handle(s) %2 ms after every group was unregistered - transient progress is leaking, and in a long campaign it would grow for the rest of the session",
				tracked.ToString(), SETTLE_WINDOW_MS.ToString()));
			return true;
		}

		return RegisterGarrison();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a DEFEND-only group and opens the observation window for it.
	//! \return True when the case is finished (a named failure); false to keep going.
	protected bool RegisterGarrison()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a garrison cannot be registered");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		vector target = position;
		target[2] = position[2] + OVT_TEST_VirtualMovementFixture.LEG_LENGTH_M;

		// A DEFEND plan is never walked, so this leg does not have to be on land - only distinct.
		// spawnDistanceOverride 0 = Manual policy, dormant by construction (see the walking case).
		m_iHandle = virtualization.RegisterGroup(OVT_TEST_VirtualMovementFixture.OWNER_SYSTEM, "movement_no_leak",
			factionKey, groupName, position,
			OVT_TEST_VirtualMovementFixture.BuildPlan(position, target, OVT_EVirtualWaypointType.DEFEND), 0);

		if (m_iHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a composition the faction registry resolves, so 'a garrison is not tracked' would be asserted against nothing");
			return true;
		}

		BaseWorld world = GetGame().GetWorld();
		m_fDeadlineMs = world.GetWorldTime() + OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS;

		m_iPhase = 2;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the tick several passes over the garrison, then asserts it is still tracking nothing.
	//! \return True when the case is finished.
	protected bool AwaitGarrisonWindow()
	{
		OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
		BaseWorld world = GetGame().GetWorld();

		if (!movement || !world)
		{
			CleanUp();
			SetFailure("The movement manager or the world went away while the case was watching a garrison");
			return true;
		}

		if (world.GetWorldTime() < m_fDeadlineMs)
			return false;

		int tracked = movement.GetTrackedCount();

		// Cleanup BEFORE reporting, the suite's rule.
		CleanUp();

		if (tracked != 0)
		{
			SetFailure(string.Format("The movement manager tracks %1 handle(s) while the only registered group has a DEFEND-only plan - a plan that cannot move must hold no transient state at all, or a campaign of garrisons pays for progress none of them can make",
				tracked.ToString()));
			return true;
		}

		PrintFormat("OVT_Global.GetVirtualMovement() resolves, its tracked count returned to 0 after the walking cases, and a registered DEFEND-only group contributed nothing to it");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the garrison on every exit path.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterGroup(m_iHandle);

		m_iHandle = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! A waypoint plan becomes real AIWaypoint entities the record OWNS - and unregistering deletes
//! every one of them along with the group entity.
//!
//! D6: the deployments framework's shared group cleanup detached waypoints without deleting them, so
//! a spawn/despawn cycle leaked one entity per waypoint forever. In 1.8 waypoints are also
//! persistence-tracked, so a leak is save bloat too.
//!
//! The deletion assertions poll rather than reading the same frame - entity deletion is committed at
//! end of frame. The bound is a diagnostic ceiling, not a retry budget.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_WaypointsAreOwnedAndDeleted : SCR_AutotestCaseBase
{
	//! Diagnostic ceiling for the end-of-frame deletion, in real milliseconds.
	static const float MAX_WAIT_MS = 5000;

	protected bool m_bRegistered;
	protected float m_fFirstPollMs;
	protected ref array<EntityID> m_aExpectGone = new array<EntityID>();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("GetGame().GetWorld() is null");
			return true;
		}

		if (!m_bRegistered)
			return RegisterAndTearDown(world);

		return PollDeleted(world);
	}

	//------------------------------------------------------------------------------------------------
	//! First tick: register with a plan, assert ownership, then unregister and remember what must go.
	//! \param[in] world The world, for entity-id lookups.
	//! \return True when the case is finished (always a failure here); false to start polling.
	protected bool RegisterAndTearDown(notnull BaseWorld world)
	{
		m_bRegistered = true;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		// Two legs plus the cycle that replays them: three waypoint entities, all core's to delete.
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();
		plan.m_aPositions.Insert(position);
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.PATROL);
		plan.m_aParams.Insert(25);
		plan.m_aPositions.Insert(position + Vector(120, 0, 0));
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.MOVE);
		plan.m_aParams.Insert(0);
		plan.m_bCycle = true;

		int handle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "waypoint_case",
			factionKey, groupName, position, plan);

		if (handle == -1)
		{
			SetFailure("RegisterGroup refused a valid two-leg cycling plan");
			return true;
		}

		OVT_VirtualGroupRecord record = virtualization.GetRecord(handle);
		SCR_AIGroup group = virtualization.GetGroup(handle);

		string failure = VerifyOwnership(record, group);

		if (record && record.m_aOwnedWaypoints)
		{
			foreach (AIWaypoint waypoint : record.m_aOwnedWaypoints)
			{
				if (waypoint)
					m_aExpectGone.Insert(waypoint.GetID());
			}
		}

		if (group)
			m_aExpectGone.Insert(group.GetID());

		virtualization.UnregisterGroup(handle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		m_fFirstPollMs = world.GetWorldTime();
		return false; // start polling for the deletions
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the plan produced owned, attached waypoints.
	protected string VerifyOwnership(OVT_VirtualGroupRecord record, SCR_AIGroup group)
	{
		if (!record)
			return "GetRecord() is null for the handle RegisterGroup just returned";

		if (!group)
			return "GetGroup() is null - no group entity to attach waypoints to";

		if (!record.m_aOwnedWaypoints || record.m_aOwnedWaypoints.Count() != 3)
		{
			// NB: `owned` is a RESERVED EnforceScript keyword - never name a local that.
			int ownedCount = 0;
			if (record.m_aOwnedWaypoints)
				ownedCount = record.m_aOwnedWaypoints.Count();

			return string.Format("The record owns %1 waypoint(s), expected 3 (two legs plus the cycle) - an unowned waypoint is a leaked entity and a leaked save record",
				ownedCount.ToString());
		}

		foreach (int i, AIWaypoint waypoint : record.m_aOwnedWaypoints)
		{
			if (!waypoint)
				return string.Format("Owned waypoint %1 is null", i.ToString());
		}

		array<AIWaypoint> attached = new array<AIWaypoint>();
		group.GetWaypoints(attached);
		if (attached.Count() < 3)
			return string.Format("The group carries %1 waypoint(s), expected at least the 3 core built - a waypoint that is not attached does nothing",
				attached.Count().ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Later ticks: every remembered entity must be gone.
	//! \param[in] world The world, for entity-id lookups.
	//! \return True when finished; false to keep polling.
	protected bool PollDeleted(notnull BaseWorld world)
	{
		int surviving = 0;
		foreach (EntityID entityId : m_aExpectGone)
		{
			if (world.FindEntityByID(entityId))
				surviving++;
		}

		if (surviving == 0)
		{
			PrintFormat("The plan built 3 owned waypoints and UnregisterGroup deleted them with the group entity (%1 entities)", m_aExpectGone.Count().ToString());
			return true;
		}

		float waited = world.GetWorldTime() - m_fFirstPollMs;
		if (waited < MAX_WAIT_MS)
			return false; // keep polling

		SetFailure("%1 of %2 entities (waypoints + group) survived UnregisterGroup after %3 ms - core leaked them",
			surviving.ToString(), m_aExpectGone.Count().ToString(), waited.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Deaths flip the per-slot mask, and killing the last living slot wipes the record.
//!
//! ReportMemberKilled is the public death seam, so the survivor-truth contract is assertable with no
//! world combat: a death reduces the alive count and marks that specific slot, re-reporting the same
//! slot changes nothing (a double-report would wipe a live group early), and the last death fires
//! OnGroupWiped BEFORE the record disappears - subscribers are documented as able to read it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_DeathsFlipMaskAndWipeRecord : SCR_AutotestCaseBase
{
	protected int m_iWipedHandle = -1;
	protected bool m_bRecordReadableAtWipe;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry");
			return true;
		}

		int before = virtualization.GetGroupCount();

		int handle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "death_case",
			factionKey, groupName, OVT_TEST_VirtualizationFixture.PickPosition());

		if (handle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a resolvable composition");
			return true;
		}

		virtualization.GetOnGroupWiped().Insert(OnGroupWiped);

		string failure = Verify(virtualization, handle, before);

		virtualization.GetOnGroupWiped().Remove(OnGroupWiped);

		// A failure before the wipe leaves the record behind - drop it so later cases see a clean
		// registry (a wipe removes it already, and UnregisterGroup is idempotent).
		virtualization.UnregisterGroup(handle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Slot deaths flipped the mask one at a time and the last one wiped handle %1", handle.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the whole D2 death contract holds.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, int handle, int before)
	{
		int roster = virtualization.GetMemberCount(handle);
		if (roster < 1)
			return "GetMemberCount() is 0 - the mask is empty, so nothing below can mean anything";

		virtualization.ReportMemberKilled(handle, 0);

		if (virtualization.GetMemberAlive(handle, 0))
			return "Slot 0 is still alive after ReportMemberKilled(handle, 0)";

		if (virtualization.GetAliveMemberCount(handle) != roster - 1)
			return string.Format("GetAliveMemberCount() is %1 after one death in a %2-slot group, expected %3",
				virtualization.GetAliveMemberCount(handle).ToString(), roster.ToString(), (roster - 1).ToString());

		// Idempotence: the kill hook can legitimately see the same slot twice (a corpse taking more
		// damage), and a second decrement would wipe a group that still has members.
		virtualization.ReportMemberKilled(handle, 0);
		if (virtualization.GetAliveMemberCount(handle) != roster - 1)
			return string.Format("Re-reporting slot 0 changed the alive count to %1 - deaths are not idempotent per slot",
				virtualization.GetAliveMemberCount(handle).ToString());

		// An out-of-range slot must be refused, not clamped onto a live one.
		virtualization.ReportMemberKilled(handle, roster + 5);
		if (virtualization.GetAliveMemberCount(handle) != roster - 1)
			return "An out-of-range slot report changed the alive count";

		if (roster > 1 && !virtualization.IsRegistered(handle))
			return "The record was wiped after a single death in a multi-slot group";

		for (int slot = 1; slot < roster; slot++)
		{
			virtualization.ReportMemberKilled(handle, slot);
		}

		if (m_iWipedHandle != handle)
			return string.Format("OnGroupWiped fired with handle %1, expected %2 (0 deaths reported means it never fired)",
				m_iWipedHandle.ToString(), handle.ToString());

		if (!m_bRecordReadableAtWipe)
			return "OnGroupWiped fired AFTER the record was removed - a subscriber cannot read what it is being told about";

		if (virtualization.IsRegistered(handle))
			return "The record survived a full wipe";

		if (virtualization.GetRecord(handle))
			return "GetRecord() still answers for a wiped handle";

		array<int> byOwner = virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "death_case");
		if (!byOwner.IsEmpty())
			return string.Format("FindGroupsByOwner still returns %1 handle(s) for a wiped group", byOwner.Count().ToString());

		if (virtualization.GetGroupCount() != before)
			return string.Format("The registry holds %1 records after the wipe, expected %2",
				virtualization.GetGroupCount().ToString(), before.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] handle The wiped handle, as reported by the manager.
	protected void OnGroupWiped(int handle)
	{
		m_iWipedHandle = handle;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
			m_bRecordReadableAtWipe = virtualization.GetRecord(handle) != null;
	}
}

//------------------------------------------------------------------------------------------------
//! The survivor mask - not the agent count - decides which roster slot the engine refills.
//!
//! Vanilla's ExpandOneMember always spawns `slotIndex == current agent count`, a first-N refill that
//! structurally destroys identity: a group that lost only its slot-0 machinegunner comes back with
//! the MG alive and a tail rifleman missing. Core overrides that seam. Two claims:
//!
//!   1. UNCONDITIONAL - the group carries the record's LIVE mask (one shared array, so a death needs
//!      no push), the refill selector picks the surviving slot rather than slot 0, and the group does
//!      not consider itself fully expanded while a survivor is unmaterialised.
//!   2. CONDITIONAL - where this world will actually materialise a member, it must occupy the
//!      SURVIVING slot. That is the runtime proof SpawnGroupMember accepts an arbitrary slot index.
//!      Conditional because a world that cannot spawn anyone proves nothing - but it can only ever
//!      fail loudly, never pass falsely.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_MaskDrivesSlotSelection : SCR_AutotestCaseBase
{
	//! Upper bound on ExpandOneMember calls. Vanilla's SpawnGroupMember answers false while a navmesh
	//! tile is loading and gives up waiting after NAVMESH_STALL_LIMIT (30) attempts, so this is that
	//! limit plus headroom - one bounded loop in one frame, not a retry budget across frames.
	static const int MAX_EXPAND_CALLS = 40;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry");
			return true;
		}

		// The claim needs a roster with a slot 1 to survive into, so find a composition that has one.
		int handle = RegisterMultiSlotGroup(virtualization, factionKey);
		if (handle == -1)
		{
			SetFailure("Faction '%1' defines no group with 2 or more roster slots, so slot-accurate refill cannot be exercised in this world", factionKey);
			return true;
		}

		string failure = Verify(virtualization, handle);

		// Claim 2 may have materialised a real character. Hand it back to the engine's own teardown
		// before unregistering, so the case cannot leave a soldier standing in the test world for
		// every case after it.
		virtualization.ForceDespawn(handle);
		virtualization.UnregisterGroup(handle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the first group registry entry of a faction whose roster has at least two slots,
	//! dropping the ones that do not qualify.
	//! \param[in] virtualization The manager.
	//! \param[in] factionKey The faction to walk.
	//! \return The handle of a registered multi-slot group, or -1.
	protected int RegisterMultiSlotGroup(notnull OVT_VirtualizationManagerComponent virtualization, string factionKey)
	{
		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		foreach (string name : OVT_TEST_VirtualizationFixture.ListGroupNames(factionKey))
		{
			int handle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, "slot_case",
				factionKey, name, position);

			if (handle == -1)
				continue;

			if (virtualization.GetMemberCount(handle) >= 2)
				return handle;

			virtualization.UnregisterGroup(handle);
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the refill seam is mask-driven.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return "GetGroup() is null";

		if (!group.HasOVTSlotMask())
			return "The group carries no survivor mask - the ExpandOneMember override would fall through to vanilla's first-N refill";

		// Kill slot 0 through the manager. The group must see it, because they share one array.
		virtualization.ReportMemberKilled(handle, 0);

		array<int> mask = group.GetOVTSlotMask();
		if (!mask || mask.IsEmpty())
			return "The group's mask is empty after a death was recorded on the record";

		if (mask[0] != 0)
			return "The group's mask still shows slot 0 alive after ReportMemberKilled(handle, 0) - the record and the group are not sharing one array";

		int next = OVT_VirtualizationMath.NextSlotToSpawn(mask, group.GetOVTSpawnedSlots());
		if (next != 1)
			return string.Format("The refill selector picked slot %1 with slot 0 dead and nothing materialised, expected slot 1", next.ToString());

		if (group.IsExpandComplete())
			return "IsExpandComplete() is true while a surviving slot has not been materialised - the queue would drop the group's spawn requests";

		// Claim 2: the runtime proof, when this world can give it.
		int expandCalls = 0;
		while (expandCalls < MAX_EXPAND_CALLS)
		{
			expandCalls++;
			if (group.ExpandOneMember())
				break;
		}

		array<int> spawnedSlots = group.GetOVTSpawnedSlots();
		if (!spawnedSlots || spawnedSlots.IsEmpty())
		{
			PrintFormat("Mask-driven refill: this world materialised no member in %1 attempts (navmesh unavailable at the test position) - the selector assertions above still hold", expandCalls.ToString());
			return "";
		}

		if (spawnedSlots.Count() != 1)
			return string.Format("%1 slots materialised from a single ExpandOneMember call", spawnedSlots.Count().ToString());

		if (spawnedSlots[0] != 1)
			return string.Format("ExpandOneMember materialised slot %1 with slot 0 dead - vanilla's first-N refill is still in charge and identity is lost",
				spawnedSlots[0].ToString());

		if (group.GetAgentsCount() != 1)
			return string.Format("The group reports %1 agents after materialising exactly one slot", group.GetAgentsCount().ToString());

		PrintFormat("Mask-driven refill materialised roster slot 1 (slot 0 dead) - SpawnGroupMember accepts an arbitrary slot index");
		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A SAVED RECORD WHOSE FACTION KEY NO LONGER RESOLVES IS DROPPED - AND ONE THAT STILL CARRIES A
//! VALID PREFAB SURVIVES ON IT (R3 / D3).
//!
//! A faction mod is removed, or a registry entry renamed, and a save still names it. Core's
//! three-step ResolvePersistedComposition: faction KEY -> registry NAME -> prefab; else the prefab
//! stored at first registration; else drop with a warning. ⚠ Steps 2 and 3 are the ones a live
//! campaign never exercises, so they are the ones that rot silently.
//!
//! Both branches in one pass, because they are one decision: the same dead-key payload is applied
//! with NO stored prefab (must drop) and WITH one this world can load (must survive). Asserting only
//! the drop would pass just as happily if resolution were reduced to "faction key or nothing".
//!
//! ApplyPersistedRegistry() is the manager's public write seam, so a hand-built payload exercises
//! exactly what a load runs. ⚠ The live registry is SNAPSHOTTED into the payload first, because a
//! restore drops everything the payload does not claim - without it this case would unregister a
//! neighbouring case's groups.
//!
//! The warning TEXT is not asserted (no tier can read the log). The dropped record must leave
//! NOTHING behind - no record, no group entity, no FindGroupsByOwner hit.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_MissingFactionRecordIsDropped : SCR_AutotestCaseBase
{
	//! A faction key and group name no config in this tree defines - the "the mod is gone" payload.
	static const string DEAD_FACTION_KEY = "OVT_TEST_NO_SUCH_FACTION";
	static const string DEAD_GROUP_NAME = "no_such_group";

	//! Owner keys, distinct so each half can be looked up on its own.
	static const string LOST_KEY = "missing_faction_lost";
	static const string FALLBACK_KEY = "missing_faction_fallback";

	//! Tiny spawn ring, for the reason every virtualization case uses one: the engine's 1 Hz lifecycle
	//! tick must not materialise members of the re-created group while the case runs.
	static const int SPAWN_DISTANCE_OVERRIDE = 23;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so no stored prefab can be exercised");
			return true;
		}

		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
		{
			SetFailure("OVT_Global.GetFactions() is null");
			return true;
		}

		OVT_Faction faction = factions.GetOverthrowFactionByKey(factionKey);
		if (!faction)
		{
			SetFailure("The faction key %1 the fixture just found no longer resolves", factionKey);
			return true;
		}

		ResourceName prefab = faction.GetGroupPrefabByName(groupName);
		if (prefab == ResourceName.Empty)
		{
			SetFailure("The composition %1 '%2' the fixture just found resolves to no prefab", factionKey, groupName);
			return true;
		}

		int baseHandle = virtualization.GetNextHandle();
		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		// The live registry goes into the payload FIRST: a restore removes every record the payload does
		// not claim, and this case is about two synthetic records, not about evicting anyone else's.
		array<ref OVT_PersistedVirtualGroup> payload = new array<ref OVT_PersistedVirtualGroup>();
		virtualization.SnapshotRegistry(payload);
		int carried = payload.Count();

		payload.Insert(BuildEntry(baseHandle, LOST_KEY, ResourceName.Empty, position));
		payload.Insert(BuildEntry(baseHandle + 1, FALLBACK_KEY, prefab, position));

		virtualization.ApplyPersistedRegistry(payload, baseHandle + 2);

		string failure = Verify(virtualization, baseHandle, carried);

		// Cleanup BEFORE reporting, so a red assertion cannot leak a group into the cases after it.
		if (virtualization.IsRegistered(baseHandle))
			virtualization.UnregisterGroup(baseHandle);

		if (virtualization.IsRegistered(baseHandle + 1))
			virtualization.UnregisterGroup(baseHandle + 1);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A saved record naming the dead faction key '%1' was dropped, and the sibling record carrying a stored prefab survived on it", DEAD_FACTION_KEY);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One synthetic payload entry naming a composition this world cannot resolve.
	//! \param[in] handle Handle to claim - always above the manager's next handle, so it can collide
	//!            with nothing live.
	//! \param[in] ownerKey Owner key to register under.
	//! \param[in] storedPrefab The registration-time prefab, or Empty to exercise the drop path.
	//! \param[in] position Where the group would be re-created.
	//! \return The entry.
	protected OVT_PersistedVirtualGroup BuildEntry(int handle, string ownerKey, ResourceName storedPrefab, vector position)
	{
		OVT_PersistedVirtualGroup entry = new OVT_PersistedVirtualGroup();
		entry.handle = handle;
		entry.ownerSystem = OVT_TEST_VirtualizationFixture.OWNER_SYSTEM;
		entry.ownerKey = ownerKey;
		entry.factionKey = DEAD_FACTION_KEY;
		entry.groupRegistryName = DEAD_GROUP_NAME;
		entry.resolvedPrefab = storedPrefab;
		entry.spawnDistanceOverride = SPAWN_DISTANCE_OVERRIDE;
		entry.importance = SCR_EAISpawnImportance.NORMAL;
		entry.position = position;

		// Two living slots. The roster is reconciled against whatever the prefab declares, so this only
		// has to be "not wiped" - a mask with no living slot is dropped for an unrelated reason.
		entry.slotAlive.Insert(1);
		entry.slotAlive.Insert(1);

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, int baseHandle, int carried)
	{
		if (virtualization.IsRegistered(baseHandle))
			return string.Format("Handle %1 named the faction key '%2', which nothing in this world defines, and carried no stored prefab - it was restored anyway, so a removed faction mod resurrects its groups as something else",
				baseHandle.ToString(), DEAD_FACTION_KEY);

		if (virtualization.GetGroup(baseHandle))
			return string.Format("Handle %1 was dropped but still hands out a group entity - a dropped record left a group in the world that nothing owns", baseHandle.ToString());

		if (!virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, LOST_KEY).IsEmpty())
			return "FindGroupsByOwner still resolves the dropped record's owner key, so a consumer reclaiming after a load would adopt a group that was never re-created";

		if (!virtualization.IsRegistered(baseHandle + 1))
			return string.Format("Handle %1 named the same dead faction key but carried the prefab it was registered with - it was dropped instead of falling back to that prefab, so every record of a RENAMED registry entry is silently deleted on load",
				(baseHandle + 1).ToString());

		if (!virtualization.GetGroup(baseHandle + 1))
			return string.Format("Handle %1 survived the restore as a record but has no group entity - the fallback resolved a prefab and then never built it", (baseHandle + 1).ToString());

		int roster = virtualization.GetMemberCount(baseHandle + 1);
		if (roster < 1)
			return "The record restored on its stored prefab has no roster slots, so its survivor mask is inert and every later death would be dropped";

		array<int> byOwner = virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, FALLBACK_KEY);
		if (byOwner.Count() != 1 || byOwner[0] != baseHandle + 1)
			return string.Format("FindGroupsByOwner returned %1 handle(s) for the record that survived, expected exactly handle %2 - the reclaim seam disagrees with the registry",
				byOwner.Count().ToString(), (baseHandle + 1).ToString());

		if (virtualization.GetGroupCount() != carried + 1)
			return string.Format("The registry holds %1 records after the restore, expected %2 (the %3 live record(s) the payload carried, plus the one synthetic record that resolved) - a drop took more than the record it was about",
				virtualization.GetGroupCount().ToString(), (carried + 1).ToString(), carried.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Test-only ambient source config.
//!
//! Answers one question the manager cannot be asked directly: when core needs a roll, does it call
//! the CONFIG's overridable method or reach past it to the arithmetic helper? This subclass
//! deliberately DISAGREES with its own authored min/max - attributes say 1, the override says
//! ROLLED_COUNT (2) - so the two answers are distinguishable.
//!
//! RollPrefab() returns ResourceName.Empty on purpose: nothing is spawned, nothing needs cleaning
//! out of the world, and the case cannot be reddened by a prefab that will not fit on the terrain.
//------------------------------------------------------------------------------------------------
class OVT_TEST_AmbientCountingConfig : OVT_AmbientSpawnSourceConfig
{
	//! What the override returns - deliberately NOT what m_iMinCount / m_iMaxCount say.
	static const int ROLLED_COUNT = 2;

	int m_iRollCountCalls;
	int m_iRollPrefabCalls;

	//------------------------------------------------------------------------------------------------
	override int RollCount()
	{
		m_iRollCountCalls++;
		return ROLLED_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	override ResourceName RollPrefab()
	{
		m_iRollPrefabCalls++;
		return ResourceName.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! A registered ambient source resolves, is counted, and takes nothing with it when it goes.
//!
//! Three things must be true before any consumer can rely on the seam:
//!   - registration SPAWNS NOTHING (dormant until an observer arrives, which is what makes it safe
//!     to register a whole town's worth of sources at campaign start),
//!   - the source is counted, so a consumer can tell "registered" from "silently refused",
//!   - unregistering removes exactly one source and is idempotent - a second call must answer false
//!     rather than corrupting the round-robin order behind it.
//!
//! The config is built in code with NO prefabs: core ships zero authored ambient content, so a test
//! needing authored content would be testing something core does not have.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_AmbientSourceRegisters : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		int before = virtualization.GetAmbientSourceCount();

		OVT_AmbientSpawnSourceConfig config = new OVT_AmbientSpawnSourceConfig();
		config.m_sSourceName = "test_ambient_source";
		config.m_fRadius = 25;

		int handle = virtualization.RegisterAmbientSource(config, OVT_TEST_VirtualizationFixture.PickPosition(), "ambient_case");

		string failure = Verify(virtualization, handle, before);

		// Cleanup BEFORE reporting, so a red assertion cannot leak a source into the cases after it.
		bool removed = false;
		if (handle != -1)
			removed = virtualization.UnregisterAmbientSource(handle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		if (!removed)
		{
			SetFailure("UnregisterAmbientSource(%1) returned false for a source that had just registered", handle.ToString());
			return true;
		}

		if (virtualization.GetAmbientSourceCount() != before)
		{
			SetFailure("UnregisterAmbientSource left %1 sources registered, expected %2",
				virtualization.GetAmbientSourceCount().ToString(), before.ToString());
			return true;
		}

		// Idempotence: the second removal is a no-op that answers honestly.
		if (virtualization.UnregisterAmbientSource(handle))
		{
			SetFailure("UnregisterAmbientSource(%1) returned true a second time - an unknown handle must answer false", handle.ToString());
			return true;
		}

		Print("An ambient source registers, is counted, spawns nothing at registration, and unregisters idempotently");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the registration behaved, or the failure to report.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, int handle, int before)
	{
		if (handle == -1)
			return "RegisterAmbientSource refused a valid config - ambient registration is the seam `civilians` is written against";

		if (virtualization.GetAmbientSourceCount() != before + 1)
			return string.Format("The ambient registry holds %1 source(s) after one registration, expected %2",
				virtualization.GetAmbientSourceCount().ToString(), (before + 1).ToString());

		array<IEntity> entities = virtualization.GetAmbientEntities(handle);
		if (!entities)
			return "GetAmbientEntities() returned null rather than an empty array";

		if (!entities.IsEmpty())
			return string.Format("A freshly registered source already owns %1 entities - registration must spawn nothing until an observer arrives",
				entities.Count().ToString());

		// An unknown ambient handle answers empty, not null and not somebody else's list.
		array<IEntity> unknown = virtualization.GetAmbientEntities(handle + 10000);
		if (!unknown || !unknown.IsEmpty())
			return "GetAmbientEntities() on an unknown handle did not return an empty array";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The CONFIG's overridden RollCount() is the one core calls - not the arithmetic behind it.
//!
//! The modder seam the whole ambient design rests on: a subclass overrides a roll, a .conf names the
//! subclass, no core code changes. ⚠ If core ever "optimised" that into a direct
//! RollCountSafe(m_iMinCount, m_iMaxCount) call, every subclass in every consumer mod would silently
//! stop being consulted - the source would still spawn, just with the authored numbers.
//!
//! OVT_TEST_AmbientCountingConfig authors min == max == 1 but overrides RollCount() to return 2, so
//! counting prefab requests in one activation distinguishes the two: 2 means consulted, 1 bypassed.
//! To make an activation happen the case parks a LOCAL OBSERVER on the source position through the
//! engine's own ObserversSystem and waits out the 2 s ambient tick.
//!
//! Where this world cannot activate, it falls back to asserting virtual dispatch through a
//! BASE-TYPED reference and says so in the log. It never passes silently on the weaker claim.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Virtualization_AmbientRollCountOverrideIsCalled : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the 2 s ambient tick to come round (generous: the tick is wall-clock,
	//! the test world's frame rate is not this case's subject). Bounded, and NOT a retry budget -
	//! the case asserts once, when the activation has happened or the budget is spent.
	static const int MAX_POLLS = 1200;

	protected int m_iPhase;
	protected int m_iPolls;
	protected int m_iHandle = -1;
	protected ref OVT_TEST_AmbientCountingConfig m_Config;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitActivation();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the counting source and parks an observer on it.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Arrange()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		m_Config = new OVT_TEST_AmbientCountingConfig();
		m_Config.m_sSourceName = "test_ambient_rollcount";
		m_Config.m_iMinCount = 1;      // deliberately disagrees with the override
		m_Config.m_iMaxCount = 1;
		m_Config.m_fRadius = 10;

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		m_iHandle = virtualization.RegisterAmbientSource(m_Config, position, "rollcount_case");
		if (m_iHandle == -1)
		{
			SetFailure("RegisterAmbientSource refused the test config");
			return true;
		}

		// ⚠ NO parked observer. InsertObserverSP with a null entity has ZERO vanilla callers, and the one
		// All-group run that parked one here froze the main thread the moment this case began. In a
		// world with real observers the source activates off them; in a world with none, the documented
		// fallback assertion runs.
		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the source has activated, or the budget is spent.
	//! \return True when the case is finished.
	protected bool AwaitActivation()
	{
		m_iPolls++;

		if (m_Config.m_iRollCountCalls == 0 && m_iPolls < MAX_POLLS)
			return false;

		string failure = Verify();
		CleanUp();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the override is the implementation core consulted.
	protected string Verify()
	{
		if (m_Config.m_iRollCountCalls == 0)
		{
			// This world never activated the source. Fall back to the weaker, world-free claim and
			// SAY SO - a silent pass here would hide a real regression.
			OVT_AmbientSpawnSourceConfig asBase = m_Config;
			int dispatched = asBase.RollCount();

			if (dispatched != OVT_TEST_AmbientCountingConfig.ROLLED_COUNT)
				return string.Format("RollCount() through a base-typed reference returned %1, expected the override's %2 - the modder seam is not virtual at all",
					dispatched.ToString(), OVT_TEST_AmbientCountingConfig.ROLLED_COUNT.ToString());

			PrintFormat("Ambient activation did not happen in this world within %1 polls (no honoured observer) - asserted virtual dispatch of the RollCount() override instead",
				m_iPolls.ToString());
			return "";
		}

		if (m_Config.m_iRollCountCalls != 1)
			return string.Format("RollCount() was called %1 times for one activation - the count must be rolled ONCE and then spent across ticks, never re-rolled per tick",
				m_Config.m_iRollCountCalls.ToString());

		if (m_Config.m_iRollPrefabCalls != OVT_TEST_AmbientCountingConfig.ROLLED_COUNT)
			return string.Format("The activation asked for %1 prefab(s); the override said %2 and the authored min/max said 1, so core consulted the wrong one",
				m_Config.m_iRollPrefabCalls.ToString(), OVT_TEST_AmbientCountingConfig.ROLLED_COUNT.ToString());

		PrintFormat("Ambient activation consulted the config subclass: RollCount() once, %1 prefab rolls (authored min/max would have given 1)",
			m_Config.m_iRollPrefabCalls.ToString());
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the observer and the source on EVERY exit path - a leaked source would keep ticking
	//! for the rest of the suite, and a leaked observer would keep whatever is near it awake.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterAmbientSource(m_iHandle);

		m_iHandle = -1;
	}

}

//------------------------------------------------------------------------------------------------
//! ReleaseAmbientEntity() answers false for an entity that was never ambient.
//!
//! Release is an OWNERSHIP TRANSFER and the honest "no" matters as much as the yes: `civilians`
//! calls it on whatever the player just recruited, very often a character some other Overthrow
//! system spawned. An unknown entity answering true would have the caller believe it had taken
//! ownership of something no source is tracking.
//!
//! The subject is the game-mode entity: unambiguously not ambient, guaranteed to exist, and
//! guaranteed not to be deleted by a wrong answer.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_ReleaseUnknownAmbientEntity : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
		{
			SetFailure("There is no game-mode entity to use as a definitely-not-ambient subject");
			return true;
		}

		if (virtualization.ReleaseAmbientEntity(gameMode))
		{
			SetFailure("ReleaseAmbientEntity() claimed the game-mode entity was ambient - a caller would believe it had taken ownership of something no source tracks");
			return true;
		}

		// The same must hold while a source exists: "unknown" is about the reverse map, not about
		// whether anything is registered at all.
		OVT_AmbientSpawnSourceConfig config = new OVT_AmbientSpawnSourceConfig();
		config.m_sSourceName = "test_release_unknown";

		int handle = virtualization.RegisterAmbientSource(config, OVT_TEST_VirtualizationFixture.PickPosition(), "release_case");

		bool claimed = virtualization.ReleaseAmbientEntity(gameMode);

		if (handle != -1)
			virtualization.UnregisterAmbientSource(handle);

		if (claimed)
		{
			SetFailure("ReleaseAmbientEntity() claimed a non-ambient entity while one source was registered");
			return true;
		}

		Print("ReleaseAmbientEntity() answers false for an entity no source owns, with and without a registered source");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Test-only ambient source config for the two hooks `civilians` added to the seam.
//!
//! Both overrides deliberately DISAGREE with the base class and count their calls, so a call made
//! through a base-typed reference is distinguishable from one that reached the base implementation:
//! IsEntityDead() answers m_bAnswer where the base answers false for any entity with no damage
//! manager; OnEntityPruned() bumps a counter where the base does nothing observable.
//!
//! This is the shape a group-tracking source is forced into: a group entity has no
//! SCR_DamageManagerComponent, so the base predicate answers false forever.
//------------------------------------------------------------------------------------------------
class OVT_TEST_AmbientDeadCheckConfig : OVT_AmbientSpawnSourceConfig
{
	//! What the override answers - set by the case, never derived from the entity.
	bool m_bAnswer;

	int m_iIsEntityDeadCalls;
	int m_iOnEntityPrunedCalls;

	//------------------------------------------------------------------------------------------------
	override bool IsEntityDead(IEntity entity)
	{
		m_iIsEntityDeadCalls++;
		return m_bAnswer;
	}

	//------------------------------------------------------------------------------------------------
	override void OnEntityPruned(IEntity entity)
	{
		m_iOnEntityPrunedCalls++;
	}
}

//------------------------------------------------------------------------------------------------
//! The config's IsEntityDead() / OnEntityPruned() are overridable, and the default of the first one
//! is the manager's old inline damage-state check.
//!
//!  1. VIRTUAL DISPATCH THROUGH A BASE-TYPED REFERENCE. The manager holds its config as an
//!     OVT_AmbientSpawnSourceConfig, so a subclass's override is only reachable if the seam is
//!     virtual through exactly that type. `civilians` cannot work without it: its tracked entity is
//!     a GROUP, which carries no damage manager, so the base predicate answers false forever and its
//!     dead civilians would never be pruned.
//!  2. THE DEFAULT DID NOT CHANGE - "destroyed means dead, and an entity with no damage manager is
//!     not dead", asserted directly on a stock config, because the whole justification for moving
//!     the check onto the config class was that nothing else would notice.
//!
//! The subject is the game-mode entity: guaranteed to exist, no answer can delete it, and the
//! default's verdict on it is false either way.
//!
//! It deliberately does NOT wait for a real prune - that needs an activated source, an observer this
//! world may not have, and then a death. The base-typed call here is the manager's own call.
//!
//! The call counters are what a non-virtual seam would show: they stay 0 while the assertions on the
//! ANSWERS still pass, which is why both are asserted.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_AmbientDeadCheckOverrideIsCalled : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
		{
			SetFailure("There is no game-mode entity to use as a guaranteed-live, damage-manager-free subject");
			return true;
		}

		// -- claim 2 first: the stock default is the manager's old inline check --------------------

		OVT_AmbientSpawnSourceConfig stock = new OVT_AmbientSpawnSourceConfig();
		stock.m_sSourceName = "test_ambient_deadcheck_stock";

		if (stock.IsEntityDead(gameMode))
		{
			SetFailure("The stock config called a live entity with no damage manager DEAD - every ambient source that does not override this would prune its whole crowd on the first tick");
			return true;
		}

		if (stock.IsEntityDead(null))
		{
			SetFailure("The stock config called a null entity dead");
			return true;
		}

		// The stock OnEntityPruned is a no-op that must survive being called with anything.
		stock.OnEntityPruned(gameMode);
		stock.OnEntityPruned(null);

		// -- claim 1: the override is reachable through the reference type the manager holds -------

		OVT_TEST_AmbientDeadCheckConfig subclass = new OVT_TEST_AmbientDeadCheckConfig();
		subclass.m_sSourceName = "test_ambient_deadcheck";
		subclass.m_bAnswer = true;

		// EXACTLY the manager's shape: OVT_AmbientSpawnSourceInstance.m_Config is a base-typed ref.
		OVT_AmbientSpawnSourceConfig asBase = subclass;

		if (!asBase.IsEntityDead(gameMode))
		{
			SetFailure("IsEntityDead() through a base-typed reference answered false where the override answers true - the base implementation ran, so the hook is not overridable and a group-shaped source could never prune a dead civilian");
			return true;
		}

		if (subclass.m_iIsEntityDeadCalls != 1)
		{
			SetFailure("The override's call counter reads %1 after one base-typed call, expected 1", subclass.m_iIsEntityDeadCalls.ToString());
			return true;
		}

		// The answer follows the override, not the entity: flipping it flips the verdict.
		subclass.m_bAnswer = false;
		if (asBase.IsEntityDead(gameMode))
		{
			SetFailure("IsEntityDead() through a base-typed reference answered true after the override was told to answer false");
			return true;
		}

		asBase.OnEntityPruned(gameMode);
		if (subclass.m_iOnEntityPrunedCalls != 1)
		{
			SetFailure("OnEntityPruned() through a base-typed reference bumped the override's counter %1 times, expected 1 - the prune hook is not overridable, so a consumer could never delete a pruned civilian's waypoints or its emptied group husk",
				subclass.m_iOnEntityPrunedCalls.ToString());
			return true;
		}

		// -- and the manager accepts a source carrying the overrides ------------------------------

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		int handle = virtualization.RegisterAmbientSource(subclass, OVT_TEST_VirtualizationFixture.PickPosition(), "deadcheck_case");
		if (handle == -1)
		{
			SetFailure("RegisterAmbientSource refused a config subclass that overrides the two prune hooks");
			return true;
		}

		// A source with no entities prunes to nothing and must not consult either hook for entities it
		// does not own - GetAmbientEntities() prunes first, which is the cheapest way to run that path.
		int deadCallsBefore = subclass.m_iIsEntityDeadCalls;
		array<IEntity> entities = virtualization.GetAmbientEntities(handle);
		int deadCallsAfter = subclass.m_iIsEntityDeadCalls;

		// Cleanup BEFORE reporting, so a red assertion cannot leak a source into the cases after it.
		virtualization.UnregisterAmbientSource(handle);

		if (!entities || !entities.IsEmpty())
		{
			SetFailure("A freshly registered source reported live entities before any activation");
			return true;
		}

		if (deadCallsAfter != deadCallsBefore)
		{
			SetFailure("Pruning an empty source called IsEntityDead() %1 extra time(s) - the predicate must only ever be asked about entities the source owns",
				(deadCallsAfter - deadCallsBefore).ToString());
			return true;
		}

		Print("IsEntityDead() and OnEntityPruned() dispatch to a config subclass through a base-typed reference, and the stock default still calls a damage-manager-free entity alive");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! civilianDensityMultiplier, maxCiviliansPerTown and despawnCiviliansDuringQRF exist in the config
//! struct and read back their defaults.
//!
//! ⚠ A field silently missing from SetDefaults() reads back 0, and 0 is MEANINGFUL for both
//! magnitudes: a multiplier of 0 is the documented "no civilians on this server" switch, and a cap
//! of 0 means UNCAPPED. Either would look like a deliberate setting rather than a bug.
//!
//! despawnCiviliansDuringQRF must default to FALSE - a town keeps its crowd while its QRF is fought,
//! a deliberate change from the pre-migration game because players recruit civilians specifically to
//! help fight it. A bool missing from SetDefaults() also reads back false, so this assertion is a
//! statement of intent that survives somebody "tidying" the default away.
//!
//! No field here is in the JIP config bitstream, which is why CONFIG_STREAM_VERSION did not move.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Civilians_AmbienceConfigDefaults : SCR_AutotestCaseBase
{
	//! The defaults written by OVT_OverthrowConfigStruct.SetDefaults().
	protected const float EXPECTED_MULTIPLIER = 1.0;
	protected const int EXPECTED_CAP = 30;

	//! Floats are never compared with == - 1.0 read back through a JSON round trip is not bit-equal.
	protected const float EPSILON = 0.0001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		if (!config.m_ConfigFile)
		{
			SetFailure("The config component has no loaded config struct");
			return true;
		}

		if (Math.AbsFloat(config.m_ConfigFile.civilianDensityMultiplier - EXPECTED_MULTIPLIER) > EPSILON)
		{
			SetFailure("civilianDensityMultiplier read back %1, expected the %2 default - a 0 here is the documented 'turn civilians off' value, so a missing default would empty every town and look deliberate",
				config.m_ConfigFile.civilianDensityMultiplier.ToString(), EXPECTED_MULTIPLIER.ToString());
			return true;
		}

		if (config.m_ConfigFile.maxCiviliansPerTown != EXPECTED_CAP)
		{
			SetFailure("maxCiviliansPerTown read back %1, expected the %2 default - a 0 here means UNCAPPED, so a missing default would quietly remove the per-town ceiling",
				config.m_ConfigFile.maxCiviliansPerTown.ToString(), EXPECTED_CAP.ToString());
			return true;
		}

		if (config.m_ConfigFile.despawnCiviliansDuringQRF)
		{
			SetFailure("despawnCiviliansDuringQRF read back true, expected the false default - a town under QRF keeps its civilians unless an operator opts out, because players recruit them to fight that very battle");
			return true;
		}

		// The defaults are the numbers the density formula is authored around: a city of 283 at the
		// parity rate resolves to 28, comfortably under the cap, and the multiplier leaves it alone.
		int resolved = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, 0.1,
			config.m_ConfigFile.civilianDensityMultiplier, 2, 40, config.m_ConfigFile.maxCiviliansPerTown);

		if (resolved != 28)
		{
			SetFailure("A 283-population town resolved to %1 civilians under the shipped defaults, expected 28 - the defaults and the formula disagree",
				resolved.ToString());
			return true;
		}

		Print("civilianDensityMultiplier defaults to 1.0, maxCiviliansPerTown to 30 and despawnCiviliansDuringQRF to false, and the shipped pair reproduces the parity crowd size");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared lookups for the civilian-ambience cases, so each of them fails on its OWN claim rather
//! than on somebody else's plumbing.
//------------------------------------------------------------------------------------------------
class OVT_TEST_CivilianAmbienceFixture
{
	//------------------------------------------------------------------------------------------------
	//! The authored town-crowd template, resolved the way the manager resolves it.
	//! \return The template, or null when the registry is not wired or carries no such source.
	static OVT_CivilianAmbienceConfig FindTemplate()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return null;

		return OVT_CivilianAmbienceConfig.Cast(
			virtualization.FindAmbientSourceConfig(OVT_CivilianAmbienceManagerComponent.TOWN_SOURCE_NAME));
	}

	//------------------------------------------------------------------------------------------------
	//! The controller of town 0, which the town manager keeps index-aligned with its town list.
	//! \return The controller component, or null when this world places no town controllers.
	static OVT_TownControllerComponent FindFirstTownController()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_TownControllers || towns.m_TownControllers.IsEmpty())
			return null;

		IEntity controllerEntity = GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]);
		if (!controllerEntity)
			return null;

		return OVT_TownControllerComponent.Cast(controllerEntity.FindComponent(OVT_TownControllerComponent));
	}
}

//------------------------------------------------------------------------------------------------
//! The authored ambient registry is wired, and it carries a town crowd of the right CLASS.
//!
//! ⚠ The CLASS matters, not just the name. The manager casts what the registry hands it to
//! OVT_CivilianAmbienceConfig and refuses anything else, because the civilian type pool, the
//! archetypes and the population rate all live on the subclass. A .conf that named the source
//! correctly but authored it as the base class would resolve, cast to null, log a warning nobody
//! reads, and leave every town empty - which looks like an unlucky roll, not a bug.
//!
//! It also pins the PARITY NUMBERS - the authored rate, the two density clamps and "ride the global
//! spawn distance" - which are what make the new crowd the same size as the retired spawner's.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Civilians_AmbienceRegistryResolves : SCR_AutotestCaseBase
{
	//! The authored parity settings (implementation.md §3.6).
	protected const float EXPECTED_RATE = 0.1;
	protected const int EXPECTED_MIN = 2;
	protected const int EXPECTED_MAX = 40;

	//! The one group prefab EVERY civilian type spawns. The per-type prefab pairs were dropped: they
	//! were byte-identical deltas whose inherited look the type's own loadout overwrote anyway.
	protected const ResourceName SHARED_GROUP_PREFAB = "{1AF5B9AE5CFD4434}Prefabs/Groups/INDFOR/Group_CIV.et";

	//! Floats are never compared with ==.
	protected const float EPSILON = 0.0001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		if (!virtualization.GetAmbientRegistry())
		{
			SetFailure("The virtualization manager has no ambient registry - the m_AmbientRegistry binding to Configs/Civilians/CivilianAmbience.conf is missing from the game-mode prefab, so no town can ever have civilians");
			return true;
		}

		OVT_AmbientSpawnSourceConfig named = virtualization.FindAmbientSourceConfig(OVT_CivilianAmbienceManagerComponent.TOWN_SOURCE_NAME);
		if (!named)
		{
			SetFailure("The ambient registry carries no source called '%1' - the lookup is exact and case-sensitive",
				OVT_CivilianAmbienceManagerComponent.TOWN_SOURCE_NAME);
			return true;
		}

		OVT_CivilianAmbienceConfig template = OVT_CivilianAmbienceConfig.Cast(named);
		if (!template)
		{
			SetFailure("The '%1' source is a %2, not an OVT_CivilianAmbienceConfig - the manager refuses it and every town stays empty",
				OVT_CivilianAmbienceManagerComponent.TOWN_SOURCE_NAME, named.Type().ToString());
			return true;
		}

		if (Math.AbsFloat(template.m_fPopulationRate - EXPECTED_RATE) > EPSILON)
		{
			SetFailure("The authored population rate is %1, expected the %2 parity value - a city's crowd is no longer the size the retired spawner built",
				template.m_fPopulationRate.ToString(), EXPECTED_RATE.ToString());
			return true;
		}

		if (template.m_iMinCount != EXPECTED_MIN || template.m_iMaxCount != EXPECTED_MAX)
		{
			SetFailure(string.Format("The authored density clamps are [%1, %2], expected [%3, %4]",
				template.m_iMinCount.ToString(), template.m_iMaxCount.ToString(),
				EXPECTED_MIN.ToString(), EXPECTED_MAX.ToString()));
			return true;
		}

		if (template.m_iSpawnDistanceOverride != -1)
		{
			SetFailure("The authored spawn-distance override is %1, expected -1 (ride virtualizationSpawnDistance) - a town crowd with its own ring stops following the operator's setting",
				template.m_iSpawnDistanceOverride.ToString());
			return true;
		}

		string poolFailure = VerifyPool(template);
		if (poolFailure != "")
		{
			SetFailure(poolFailure);
			return true;
		}

		PrintFormat("The authored town crowd resolves as an OVT_CivilianAmbienceConfig with %1 civilian type(s), %2 archetype(s) and the parity density settings",
			template.m_aTypes.Count().ToString(), template.m_aArchetypes.Count().ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] template The resolved template.
	//! \return An empty string when the authored content can actually produce a civilian.
	protected string VerifyPool(notnull OVT_CivilianAmbienceConfig template)
	{
		if (!template.m_aTypes || template.m_aTypes.IsEmpty())
			return "The town crowd authors no civilian types at all, so every roll answers Empty and the crowd is never built";

		bool rollable = false;
		foreach (OVT_CivilianTypeConfig type : template.m_aTypes)
		{
			if (!type)
				continue;

			if (type.m_rGroupPrefab == ResourceName.Empty)
				return string.Format("Civilian type '%1' names no group prefab", type.m_sTypeName);

			// EVERY TYPE SHARES ONE PREFAB, ON PURPOSE. The per-type civilian prefabs were deleted
			// because ApplyCivilianLoadout overwrites every slot they could have authored - a type's
			// look is now entirely its loadout. This pins that: a type left pointing at a prefab that
			// no longer exists deserialises to a resource nothing can spawn, and core would stop that
			// town's activation on every roll that picked it.
			if (type.m_rGroupPrefab != SHARED_GROUP_PREFAB)
				return string.Format("Civilian type '%1' names group prefab '%2', expected the shared '%3' - every type spawns the one civilian group and gets its look from its loadout, so a different (or deleted) prefab here is either a stale reference or a variant the clothing pass would overwrite anyway",
					type.m_sTypeName, type.m_rGroupPrefab, SHARED_GROUP_PREFAB);

			if (type.m_iWeight > 0)
				rollable = true;
		}

		if (!rollable)
			return "Every authored civilian type has a weight of 0 or below, so nothing is ever eligible and no town gets a crowd";

		if (!template.m_aArchetypes || template.m_aArchetypes.IsEmpty())
			return "The town crowd authors no behaviour archetypes, so every civilian would fall back to the hard-coded wait bounds";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A per-town instance built from the authored template is bound to ONE town, reads the template BY
//! REFERENCE, and reads that town's population LIVE.
//!
//!  1. THE TEMPLATE IS SHARED, NOT COPIED (D5). A builder that copied fields would compile, work
//!     today, and silently drop every attribute added to the template afterwards.
//!  2. THE RADIUS IS THE TOWN'S, NOT THE TEMPLATE'S. One authored radius shared by a city and a
//!     hamlet would scatter a village's civilians into the next valley.
//!  3. THE POPULATION IS NEVER BAKED. RollCount() must follow the town's CURRENT population with no
//!     re-registration. Asserted by moving the town's population to 0 and re-rolling: 0 beats the
//!     authored minimum clamp, so an instance that cached at build time answers the old figure.
//!
//! Nothing is registered - the instance is built and asked directly, so no ambient source is left
//! behind and no observer is needed. The town's population is restored before the verdict is
//! reported, on every path.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Civilians_PerTownSourceBinds : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_CivilianAmbienceManagerComponent civilians = OVT_Global.GetCivilianAmbience();
		if (!civilians)
		{
			SetFailure("OVT_Global.GetCivilianAmbience() is null - see OVT_TEST_Init_Civilians_ManagerResolves");
			return true;
		}

		OVT_CivilianAmbienceConfig template = OVT_TEST_CivilianAmbienceFixture.FindTemplate();
		if (!template)
		{
			SetFailure("The authored town crowd did not resolve - see OVT_TEST_Init_Civilians_AmbienceRegistryResolves");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("No towns are registered, so there is nothing to bind a crowd to");
			return true;
		}

		OVT_TownControllerComponent controller = OVT_TEST_CivilianAmbienceFixture.FindFirstTownController();
		if (!controller)
		{
			SetFailure("Town 0 has no controller component, so the town's authored range cannot be read");
			return true;
		}

		OVT_TownData town = towns.m_Towns[0];
		OVT_TownCivilianSourceConfig source = civilians.BuildTownSource(template, controller, town, 0);
		if (!source)
		{
			SetFailure("BuildTownSource() produced no per-town instance");
			return true;
		}

		// -- claims 1 and 2, which need no campaign state at all ------------------------------------

		string failure = VerifyBinding(source, template, controller);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// -- claim 3: the population is read live, every roll ---------------------------------------

		int originalPopulation = town.population;

		int liveRoll = source.RollCount();

		town.population = 0;
		int emptyRoll = source.RollCount();

		// Restored BEFORE anything is reported, so a red assertion below cannot leave the campaign's
		// only town depopulated for the cases after this one.
		town.population = originalPopulation;

		if (liveRoll <= 0)
		{
			SetFailure("A town of %1 people rolled %2 civilians - a populated town must produce a crowd at the authored rate and floor",
				originalPopulation.ToString(), liveRoll.ToString());
			return true;
		}

		if (emptyRoll != 0)
		{
			SetFailure("A town with 0 population rolled %1 civilians - either the population was baked at build time (so a depopulated town keeps its old crowd) or the minimum clamp is resurrecting one",
				emptyRoll.ToString());
			return true;
		}

		PrintFormat("A per-town instance shares the authored template, takes town 0's %1 m range as its radius, and rolls %2 civilians from its LIVE population of %3 (0 when the town is emptied)",
			controller.m_iTownRange.ToString(), liveRoll.ToString(), originalPopulation.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] source The instance under test.
	//! \param[in] template The template it was built from.
	//! \param[in] controller The town's controller.
	//! \return An empty string when the binding holds, or the first broken claim.
	protected string VerifyBinding(notnull OVT_TownCivilianSourceConfig source, notnull OVT_CivilianAmbienceConfig template, notnull OVT_TownControllerComponent controller)
	{
		if (source.GetTemplate() != template)
			return "The per-town instance does not hand back the registry's own template object - it was copied rather than bound, so every attribute the template gains later will be missing from every town";

		if (source.GetTownId() != 0)
			return string.Format("The instance reports town id %1, expected 0", source.GetTownId().ToString());

		if (Math.AbsFloat(source.m_fRadius - controller.m_iTownRange) > 0.5)
			return string.Format("The instance scatters over %1 m, expected town 0's authored range of %2 m - a shared radius puts a village's civilians in the next valley",
				source.m_fRadius.ToString(), controller.m_iTownRange.ToString());

		if (source.m_iMinCount != template.m_iMinCount || source.m_iMaxCount != template.m_iMaxCount)
			return "The instance's density clamps do not match the template's, so core would spend a different number of spawns than the crowd was authored for";

		if (source.m_iSpawnDistanceOverride != template.m_iSpawnDistanceOverride)
			return "The instance's spawn-distance override does not match the template's";

		if (source.m_sSourceName != template.m_sSourceName)
			return "The instance lost the template's source name, so it cannot be recognised in a log";

		array<ref OVT_CivilianTypeConfig> allowed = source.GetAllowedTypes();
		if (!allowed || allowed.IsEmpty())
			return "The instance resolved NO allowed civilian types for a town the authored 'generic' type is supposed to be valid in - RollPrefab() would answer Empty and the town would stay empty";

		if (allowed.Count() > template.m_aTypes.Count())
			return "The instance allows more civilian types than the template authors";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The civilian ambience manager is on the game mode and resolves through OVT_Global.
//!
//! Everything this feature does hangs off one component entry on the game-mode prefab, and a dropped
//! or re-saved entry fails SILENTLY: ActivateTown() finds a null manager and returns, and every town
//! has no civilians - which looks like content, not a bug.
//!
//! It pins the accessor to the component ON THE GAME MODE ENTITY, because the static is deliberately
//! re-resolved across a world: a manager left over from a previous campaign in the same session must
//! be dropped rather than handed out.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Civilians_ManagerResolves : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_CivilianAmbienceManagerComponent civilians = OVT_Global.GetCivilianAmbience();
		if (!civilians)
		{
			SetFailure("OVT_Global.GetCivilianAmbience() is null - Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_CivilianAmbienceManagerComponent entry, so every town in the campaign silently has no civilians");
			return true;
		}

		if (OVT_CivilianAmbienceManagerComponent.GetInstance() != civilians)
		{
			SetFailure("OVT_Global.GetCivilianAmbience() and GetInstance() answered with different components");
			return true;
		}

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
		{
			SetFailure("There is no game mode to check the manager's owner against");
			return true;
		}

		IEntity gameModeEntity = gameMode;
		if (civilians.GetOwner() != gameModeEntity)
		{
			SetFailure("The civilian ambience manager is not on the game-mode entity - a static left over from a previous campaign is being handed out");
			return true;
		}

		if (civilians.GetTownHandle(-1) != -1)
		{
			SetFailure("GetTownHandle() answered a handle for a town id that does not exist");
			return true;
		}

		if (civilians.GetTownSource(-1))
		{
			SetFailure("GetTownSource() answered a source for a town id that does not exist");
			return true;
		}

		PrintFormat("The civilian ambience manager resolves through OVT_Global, belongs to the current game mode, and holds %1 town source(s)",
			civilians.GetActiveTownCount().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Releasing a civilian that was never ambient does nothing at all - and above all does not destroy
//! anything.
//!
//! ⚠ The release runs inside OVT_RecruitManagerComponent.RecruitCivilian(), the single chokepoint
//! BOTH recruit paths funnel through. The tent recruit was never ambient, so it arrives here every
//! single time somebody recruits at a tent; a release path that reached for a group, a husk or a
//! waypoint set that was not its own would delete part of a player's recruit as they paid for it.
//!
//! The assertions are therefore about ABSENCE: the call answers false, the character is still in the
//! world, and no town's source count moved. Null is asserted first as the cheapest way to fault.
//!
//! The subject is a REAL character from the recruit prefab, because the resolution being exercised
//! (character -> AI control -> agent -> parent group) has nothing to walk on a bare entity. Whether
//! an agent materialises inside the poll budget is reported, not asserted - the answer must be
//! `false` either way.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Civilians_ReleaseOfNonAmbientIsSafe : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the spawned subject's agent to materialise. Not a correctness bound -
	//! the verdict is the same with or without an agent.
	static const int MAX_AGENT_POLLS = 60;

	protected int m_iPhase;
	protected int m_iAgentPolls;
	protected IEntity m_Character;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnSubject();

		return AwaitAgentThenRelease();
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the null path, then spawns the live subject.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubject()
	{
		OVT_CivilianAmbienceManagerComponent civilians = OVT_Global.GetCivilianAmbience();
		if (!civilians)
		{
			SetFailure("OVT_Global.GetCivilianAmbience() is null - see OVT_TEST_Init_Civilians_ManagerResolves");
			return true;
		}

		// The cheapest way for the whole resolution to fault, asserted before anything is built.
		if (civilians.ReleaseRecruitedCivilian(null))
		{
			SetFailure("ReleaseRecruitedCivilian(null) answered true");
			return true;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn a subject from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject character");
			return true;
		}

		m_Character = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, towns.m_Towns[0].location);
		if (!m_Character)
		{
			SetFailure("SpawnEntityPrefab() produced no character from the recruit prefab");
			return true;
		}

		AIControlComponent aiControl = AIControlComponent.Cast(m_Character.FindComponent(AIControlComponent));
		if (aiControl)
			aiControl.ActivateAI();

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits (briefly) for an agent, then asserts the release is a no-op whichever way that went.
	//! \return True when the case is finished.
	protected bool AwaitAgentThenRelease()
	{
		if (!m_Character)
		{
			SetFailure("The subject character disappeared before it could be released");
			return FinishAndCleanUp();
		}

		bool hasAgent = false;
		AIControlComponent aiControl = AIControlComponent.Cast(m_Character.FindComponent(AIControlComponent));
		if (aiControl && aiControl.GetAIAgent())
			hasAgent = true;

		if (!hasAgent && m_iAgentPolls < MAX_AGENT_POLLS)
		{
			m_iAgentPolls += 1;
			return false;
		}

		OVT_CivilianAmbienceManagerComponent civilians = OVT_Global.GetCivilianAmbience();
		if (!civilians)
		{
			SetFailure("The civilian ambience manager went away mid-case");
			return FinishAndCleanUp();
		}

		int townsBefore = civilians.GetActiveTownCount();

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(m_Character);
		if (!character)
		{
			SetFailure("The recruit prefab did not spawn an SCR_ChimeraCharacter, so the release path cannot be exercised on it");
			return FinishAndCleanUp();
		}

		bool claimed = civilians.ReleaseRecruitedCivilian(character);

		if (claimed)
		{
			SetFailure("ReleaseRecruitedCivilian() claimed a character no ambient source owns - a tent recruit would be treated as somebody's town crowd");
			return FinishAndCleanUp();
		}

		if (!m_Character)
		{
			SetFailure("The subject character was DESTROYED by a release that should have been a no-op - this is the shape of deleting part of a player's recruit at the moment they pay for it");
			return FinishAndCleanUp();
		}

		if (civilians.GetActiveTownCount() != townsBefore)
		{
			SetFailure("A no-op release changed the number of registered town sources from %1 to %2",
				townsBefore.ToString(), civilians.GetActiveTownCount().ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Releasing a non-ambient character is a safe no-op (agent present: %1, after %2 poll(s)) and null is refused before anything is touched",
			hasAgent.ToString(), m_iAgentPolls.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the subject from the world after the verdict is in, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);

		m_Character = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! PER-TOWN CURATION IS REAL: a city and a village resolve DIFFERENT civilian type sets out of the
//! same authored template, and a town's own allow-list narrows the set further.
//!
//! The Logic tier asserts the filter RULE and the shipped table as literal data. What it cannot
//! reach is the wiring from the authored .conf through the manager's resolver: a type entry that
//! failed to deserialise, an m_eMinTownSize left at its default, or a resolver that stopped passing
//! the allow-list all compile clean, keep Logic green, and show up in play only as "the towns all
//! look the same" - which nobody reports as a bug.
//!
//!  1. A CITY ALLOWS STRICTLY MORE THAN A VILLAGE. Equal counts mean the size filter is not applied
//!     at resolve time at all.
//!  2. THE CITY-ONLY TYPE IS EXACTLY WHERE IT BELONGS - in the city set, absent from the village set.
//!  3. AN AUTHORED ALLOW-LIST NARROWS THE SET, and an EMPTY list does not. Both readings of an empty
//!     list compile; the wrong one empties every un-authored town in the world.
//!
//! It also checks at least one shipped type carries a per-type LOADOUT with slots - the one thing
//! about the .conf no compiler can see, and without it every prefab variant is re-dressed out of the
//! global pool and the whole variety phase is invisible in play.
//!
//! Nothing is registered and no town is modified: the resolver is a pure function of the template.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Civilians_TypeCurationBySize : SCR_AutotestCaseBase
{
	//! The shipped type that is authored for cities only.
	protected const string CITY_ONLY_TYPE = "businessman";

	//! The shipped type that is authored for every settlement size.
	protected const string UNRESTRICTED_TYPE = "generic";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_CivilianAmbienceManagerComponent civilians = OVT_Global.GetCivilianAmbience();
		if (!civilians)
		{
			SetFailure("OVT_Global.GetCivilianAmbience() is null - see OVT_TEST_Init_Civilians_ManagerResolves");
			return true;
		}

		OVT_CivilianAmbienceConfig template = OVT_TEST_CivilianAmbienceFixture.FindTemplate();
		if (!template)
		{
			SetFailure("The authored town crowd did not resolve - see OVT_TEST_Init_Civilians_AmbienceRegistryResolves");
			return true;
		}

		if (template.m_aTypes.Count() < 2)
		{
			SetFailure("The authored template carries %1 civilian type(s) - per-town curation cannot mean anything until there is more than one kind of civilian to curate",
				template.m_aTypes.Count().ToString());
			return true;
		}

		// -- claim 1: the two sizes disagree ---------------------------------------------------------

		array<ref OVT_CivilianTypeConfig> cityTypes = civilians.ResolveAllowedTypes(template, OVT_TownSize.CITY, null);
		array<ref OVT_CivilianTypeConfig> villageTypes = civilians.ResolveAllowedTypes(template, OVT_TownSize.VILLAGE, null);

		if (villageTypes.IsEmpty())
		{
			SetFailure("A VILLAGE resolved no civilian types at all - the unrestricted type must survive every size, or every hamlet on the map is silently empty");
			return true;
		}

		if (cityTypes.Count() <= villageTypes.Count())
		{
			SetFailure("A CITY resolved %1 civilian types and a VILLAGE resolved %2 - a city must allow strictly more, or the minimum-size filter is not being applied when a town's set is resolved",
				cityTypes.Count().ToString(), villageTypes.Count().ToString());
			return true;
		}

		// -- claim 2: the city-only type is where it belongs -----------------------------------------

		if (!ContainsType(cityTypes, CITY_ONLY_TYPE))
		{
			SetFailure("The '%1' type is missing from a CITY's set - it is authored for cities, so a city is the one place it must appear",
				CITY_ONLY_TYPE);
			return true;
		}

		if (ContainsType(villageTypes, CITY_ONLY_TYPE))
		{
			SetFailure("The '%1' type was allowed in a VILLAGE - the whole point of its authored minimum size is to keep it out of hamlets",
				CITY_ONLY_TYPE);
			return true;
		}

		// -- claim 3: an authored allow-list narrows, an empty one does not --------------------------

		array<string> curated = {UNRESTRICTED_TYPE};
		array<ref OVT_CivilianTypeConfig> curatedTypes = civilians.ResolveAllowedTypes(template, OVT_TownSize.CITY, curated);

		if (curatedTypes.Count() != 1 || !ContainsType(curatedTypes, UNRESTRICTED_TYPE))
		{
			SetFailure("A CITY curated down to one type resolved %1 type(s) - a town's authored list is an allow-list and must be honoured on top of the size filter",
				curatedTypes.Count().ToString());
			return true;
		}

		array<string> notAuthored = {};
		array<ref OVT_CivilianTypeConfig> emptyListTypes = civilians.ResolveAllowedTypes(template, OVT_TownSize.CITY, notAuthored);

		if (emptyListTypes.Count() != cityTypes.Count())
		{
			SetFailure("A CITY with an EMPTY allow-list resolved %1 types instead of the %2 its size permits - every un-authored town carries an empty list, so reading it as 'nothing allowed' empties the map",
				emptyListTypes.Count().ToString(), cityTypes.Count().ToString());
			return true;
		}

		// -- the authored per-type clothing actually deserialised ------------------------------------

		int withLoadout = 0;
		foreach (OVT_CivilianTypeConfig type : cityTypes)
		{
			if (type && type.m_Loadout && type.m_Loadout.m_aSlots && !type.m_Loadout.m_aSlots.IsEmpty())
				withLoadout++;
		}

		if (withLoadout == 0)
		{
			SetFailure("Not one of the %1 authored civilian types carries a per-type loadout with slots in it - the visible clothing is overwritten on every civilian regardless of its prefab, so without per-type clothing every variant in the crowd looks identical",
				cityTypes.Count().ToString());
			return true;
		}

		PrintFormat("Per-town curation resolves as authored: a CITY allows %1 civilian types and a VILLAGE %2, the city-only type is refused in the village, an authored list narrows a city to exactly what it names, and %3 type(s) carry their own clothing",
			cityTypes.Count().ToString(), villageTypes.Count().ToString(), withLoadout.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] types A resolved type set.
	//! \param[in] typeName The name to look for.
	//! \return True when the set carries a type of that exact name.
	protected bool ContainsType(array<ref OVT_CivilianTypeConfig> types, string typeName)
	{
		if (!types)
			return false;

		foreach (OVT_CivilianTypeConfig type : types)
		{
			if (type && type.m_sTypeName == typeName)
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The authored PARKED VEHICLE source resolves, is of the right class, and can roll a car.
//!
//!  1. THE REGISTRY CARRIES `town_vehicles`. The lookup is exact and case-sensitive, and the manager
//!     is deliberately SILENT when it misses, so a name typo produces no cars and no log line at all.
//!  2. IT IS AN OVT_TownVehicleAmbienceConfig - the manager casts and refuses anything else, because
//!     the three size-scaled count pairs and the search ranges live on the subclass.
//!  3. THE POOL IS AUTHORED AND ROLLS. m_aPrefabs is the base-class array by design and a bound
//!     instance ALIASES the template's array rather than copying it, so this asserts the alias too: a
//!     Bind() that forgot it leaves an instance rolling out of its own empty array.
//!  4. THE SIZE-SCALED COUNTS ARE ORDERED AND NON-NEGATIVE. RollCountSafe swaps an inverted pair
//!     silently, so a mis-authored max-below-min never fails loudly in play.
//!
//! ⚠ It deliberately does NOT roll a position. RollPosition() runs kerb and road-network queries
//! against the live world, and the autotest world's one settlement is not a placement fixture -
//! asserting on it would be asserting on the terrain. Placement quality is the play-test.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Civilians_VehicleSourceResolvesAndRolls : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		// -- claim 1 --------------------------------------------------------------------------------

		OVT_AmbientSpawnSourceConfig named = virtualization.FindAmbientSourceConfig(OVT_CivilianAmbienceManagerComponent.TOWN_VEHICLE_SOURCE_NAME);
		if (!named)
		{
			SetFailure("The ambient registry carries no source called '%1' - the lookup is exact and case-sensitive, and the manager is silent when it misses, so a town would simply never have a parked car",
				OVT_CivilianAmbienceManagerComponent.TOWN_VEHICLE_SOURCE_NAME);
			return true;
		}

		// -- claim 2 --------------------------------------------------------------------------------

		OVT_TownVehicleAmbienceConfig template = OVT_TownVehicleAmbienceConfig.Cast(named);
		if (!template)
		{
			SetFailure("The '%1' source is a %2, not an OVT_TownVehicleAmbienceConfig - the manager refuses it and no town gets parked cars",
				OVT_CivilianAmbienceManagerComponent.TOWN_VEHICLE_SOURCE_NAME, named.Type().ToString());
			return true;
		}

		if (!template.m_aPrefabs || template.m_aPrefabs.IsEmpty())
		{
			SetFailure("The '%1' source authors no vehicle prefabs, so every roll answers Empty and core stops the activation",
				OVT_CivilianAmbienceManagerComponent.TOWN_VEHICLE_SOURCE_NAME);
			return true;
		}

		// -- claims 3 and 4, through a bound instance ------------------------------------------------

		OVT_TownVehicleSourceConfig source = new OVT_TownVehicleSourceConfig();
		source.Bind(template, 0, OVT_TownSize.CITY);

		ResourceName rolled = source.RollPrefab();
		if (rolled == ResourceName.Empty)
		{
			SetFailure("A bound vehicle source rolled no prefab even though the template authors %1 - the instance is not reading the template's pool (Bind() aliases m_aPrefabs)",
				template.m_aPrefabs.Count().ToString());
			return true;
		}

		if (template.m_aPrefabs.Find(rolled) == -1)
		{
			SetFailure("A bound vehicle source rolled '%1', which is not in the authored pool", rolled);
			return true;
		}

		string boundsFailure = CheckBounds(template);
		if (boundsFailure != "")
		{
			SetFailure(boundsFailure);
			return true;
		}

		// No SetFailure() is the pass, exactly as every other case in this suite reports one.
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The three size-scaled count pairs, in order and non-negative.
	//! \param[in] template The authored declaration.
	//! \return An empty string when the counts are sane, else the reason they are not.
	protected string CheckBounds(notnull OVT_TownVehicleAmbienceConfig template)
	{
		if (template.m_iVillageMin < 0 || template.m_iTownMin < 0 || template.m_iCityMin < 0)
			return "A size-scaled parked-car minimum is negative";

		if (template.m_iVillageMax < template.m_iVillageMin)
			return string.Format("The VILLAGE parked-car pair is inverted: [%1, %2]", template.m_iVillageMin.ToString(), template.m_iVillageMax.ToString());

		if (template.m_iTownMax < template.m_iTownMin)
			return string.Format("The TOWN parked-car pair is inverted: [%1, %2]", template.m_iTownMin.ToString(), template.m_iTownMax.ToString());

		if (template.m_iCityMax < template.m_iCityMin)
			return string.Format("The CITY parked-car pair is inverted: [%1, %2]", template.m_iCityMin.ToString(), template.m_iCityMax.ToString());

		if (template.m_iPlacementAttempts < 1)
			return "m_iPlacementAttempts is below 1, so no car would ever be given a spot";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE GATE: is a server-side InsertObserverSP honoured?
//!
//! Phase 6 of `virtualization/integration` wants an engine observer that FOLLOWS an entity, so a
//! parked recruit squad pulls dormant groups awake with no player nearby. InsertObserverSP is
//! documented as a LOCAL ("SP") observer, has ZERO vanilla script callers in the 1.8 tree, and
//! nothing in script says whether an insert made on the AUTHORITY is seen by the proximity query the
//! group lifecycle uses.
//!
//! ⚠⚠ APPLICATION IS DEFERRED BY A FRAME, in both directions. Sampling immediately after the call
//! reads "not honoured"; the same probe answers true ~30 frames later. So every sample this case
//! asserts on is taken after settling, and any consumer built on this API must never expect its own
//! effect to be visible on the frame it asks for it.
//!
//! ⚠ NEVER A NULL ENTITY. InsertObserverSP(key, x, z, null) hard-froze the game client the one time a
//! test parked one. This case always passes a real marker entity with zero offsets.
//!
//! Measured, and printed as one greppable "T1.9 VERDICT" line whatever happens: honoured (probe true
//! after settling, false before); the deferral itself (same-frame samples kept as data, never
//! asserted on); key semantics (the same key inserted TWICE must grow the settled count by exactly
//! ONE); removal (probe false again and the count back to its pre-insert value).
//!
//! A "not honoured" result is NOT a red - this is a spike over an engine surface, and a negative
//! verdict re-plans Phase 6 onto InsertObserverMP. It fails only on what is genuinely broken or
//! dangerous: no ObserversSystem, no marker entity, a settled count that grew by two for two
//! same-key inserts, or an observer that SURVIVES its removal.
//!
//! The settling budgets are NOT retry budgets: each phase polls a bounded number of frames and then
//! asserts exactly ONCE, on the sample it ends with.
//!
//! ⚠ The probe position is far from the autotest camera, which is itself an engine observer. The
//! case searches a ring for a spot with no observer within 1 km; if this world has none it says so
//! and asserts nothing. The marker outlives the removal on purpose, so no observer is ever following
//! a deleted entity.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Virtualization_ServerObserverSPSpike : SCR_AutotestCaseBase
{
	//! Namespaced SP key. The SP key space has no vanilla script user at all, so this only has to not
	//! collide with Overthrow's own future keys.
	static const int OBSERVER_KEY = 770019;

	//! Probe ring: the radius the "is anyone near the marker" question is asked at, squared.
	static const float PROBE_RANGE_SQ = 2500;    // 50 m

	//! How far the probe position must be from every existing observer, squared. Generous, because the
	//! autotest camera is an observer and this case's whole precondition is "nobody is near here".
	static const float CLEAR_RANGE_SQ = 1000000; // 1000 m

	//! Frames allowed for a deferred insert or removal to be applied. Measured at ~<= 30 frames on
	//! 2026-08-17; the budget is 4x that so a slow frame does not turn the spike red. Bounded, and NOT
	//! a retry budget - each phase asserts once, on the sample it ends with.
	static const int SETTLE_FRAMES = 120;

	protected int m_iPhase;
	protected int m_iFrames;
	protected IEntity m_Marker;
	protected vector m_vProbe;

	protected int m_iCountBefore;
	protected int m_iCountSameFrame;      // informational: the deferral, not an assertion
	protected int m_iCountSettled;
	protected int m_iInsertFrames;
	protected bool m_bHasBefore;
	protected bool m_bHasSameFrame;       // informational: the deferral, not an assertion
	protected bool m_bHasSettled;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		if (m_iPhase == 1)
			return AwaitInsert();

		return AwaitRemoval();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds an observer-free probe position, spawns the marker there and inserts the observer twice on
	//! one key. Samples same-frame purely as evidence of the deferral.
	//! \return True when the case is already finished.
	protected bool Arrange()
	{
		ObserversSystem observers = FindObserversSystem();
		if (!observers)
		{
			SetFailure("This world has no ObserversSystem - the spike cannot be run here, and neither can any of core's proximity (nothing would ever materialise)");
			return true;
		}

		m_vProbe = FindClearPosition(observers);
		if (m_vProbe == vector.Zero)
		{
			Print("T1.9 VERDICT: NOT MEASURED - no candidate position in this world is 1 km clear of every existing observer, so the 'false before the insert' precondition could not be established. No claim asserted.");
			return true;
		}

		m_bHasBefore = observers.HasObserverWithinRangeSq(m_vProbe[0], m_vProbe[2], PROBE_RANGE_SQ);
		if (m_bHasBefore)
		{
			Print("T1.9 VERDICT: NOT MEASURED - the chosen probe position already reports an observer within 50 m despite being 1 km clear at the wider radius. No claim asserted.");
			return true;
		}

		m_Marker = SpawnMarker(m_vProbe);
		if (!m_Marker)
		{
			SetFailure("Could not spawn the throwaway marker entity - the spike refuses to insert a null-entity observer (core context.md gotcha 0: it hard-freezes the client)");
			return true;
		}

		m_iCountBefore = CountObserversSP(observers);

		// The entity-following form: zero offsets, so the observer IS the marker's position.
		// Inserted TWICE on the SAME key - "insert or update" per the engine header, so the settled
		// count must grow by exactly one.
		observers.InsertObserverSP(OBSERVER_KEY, 0, 0, m_Marker);
		observers.InsertObserverSP(OBSERVER_KEY, 0, 0, m_Marker);

		// Evidence of the deferral, never asserted on: on 2026-08-17 both of these read "nothing
		// happened" while the settled samples 30 frames later read "honoured".
		m_bHasSameFrame = observers.HasObserverWithinRangeSq(m_vProbe[0], m_vProbe[2], PROBE_RANGE_SQ);
		m_iCountSameFrame = CountObserversSP(observers);

		m_iPhase = 1;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the deferred insert has been applied, or the budget is spent, then takes the sample
	//! the honoured/not-honoured verdict is read from and asks for the removal.
	//! \return True when the case is finished (only on an infrastructure failure).
	protected bool AwaitInsert()
	{
		ObserversSystem observers = FindObserversSystem();
		if (!observers)
		{
			CleanUp();
			SetFailure("The ObserversSystem disappeared while waiting for the deferred insert");
			return true;
		}

		m_iFrames++;

		bool has = observers.HasObserverWithinRangeSq(m_vProbe[0], m_vProbe[2], PROBE_RANGE_SQ);
		if (!has && m_iFrames < SETTLE_FRAMES)
			return false;

		m_bHasSettled = has;
		m_iCountSettled = CountObserversSP(observers);
		m_iInsertFrames = m_iFrames;

		observers.RemoveObserverSP(OBSERVER_KEY);

		m_iPhase = 2;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the deferred removal has been applied, or the budget is spent, then cleans up,
	//! prints the verdict and asserts.
	//! \return True - the case is finished on this frame either way.
	protected bool AwaitRemoval()
	{
		ObserversSystem observers = FindObserversSystem();
		if (!observers)
		{
			CleanUp();
			SetFailure("The ObserversSystem disappeared while waiting for the deferred removal");
			return true;
		}

		m_iFrames++;

		bool has = observers.HasObserverWithinRangeSq(m_vProbe[0], m_vProbe[2], PROBE_RANGE_SQ);
		int count = CountObserversSP(observers);

		bool settled = !has && count <= m_iCountBefore;
		if (!settled && m_iFrames < SETTLE_FRAMES)
			return false;

		// The marker is deleted only now: until the removal has been applied, an observer is still
		// following it, and nothing here is worth finding out what a deleted followed entity does.
		CleanUp();

		Report(has, count, m_iFrames);

		// -- the assertions, all post-settle, all about broken/dangerous behaviour, never the verdict --

		if (m_bHasSettled && m_iCountSettled > m_iCountBefore + 1)
		{
			SetFailure("Two InsertObserverSP calls on the SAME key settled at " + m_iCountSettled.ToString()
				+ " SP observers, up from " + m_iCountBefore.ToString()
				+ " - the key is not an identity, so an idempotent AddEntityObserver() would leak one observer per call");
			return true;
		}

		if (count > m_iCountBefore)
		{
			SetFailure("RemoveObserverSP left the SP observer count at " + count.ToString()
				+ " after " + m_iFrames.ToString() + " settling frames, above the pre-insert "
				+ m_iCountBefore.ToString()
				+ " - a leaked observer keeps everything near it materialised for the rest of the session");
			return true;
		}

		if (has)
		{
			SetFailure("The probe position still reports an observer within 50 m " + m_iFrames.ToString()
				+ " frames after RemoveObserverSP, having reported none before the insert - the removal did not take");
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Prints the one greppable verdict line the Phase 6 gate is read from.
	//! \param[in] hasAfterRemove The settled has(probe) after the removal.
	//! \param[in] countAfterRemove The settled SP observer count after the removal.
	//! \param[in] removeFrames Frames waited for the removal to be applied.
	protected void Report(bool hasAfterRemove, int countAfterRemove, int removeFrames)
	{
		// Built in short pieces: one long concatenation is "Formula too complex" to the compiler.
		string honoured = "NOT HONOURED even after settling (Phase 6 must re-plan onto InsertObserverMP)";
		if (m_bHasSettled)
			honoured = "HONOURED, application DEFERRED (invisible same-frame, visible within the settling budget)";

		string verdict = "T1.9 VERDICT: server-side InsertObserverSP(key, 0, 0, entity) is ";
		verdict = verdict + honoured;
		verdict = verdict + string.Format(" | settled after %1 frame(s), removal settled after %2", m_iInsertFrames.ToString(), removeFrames.ToString());
		verdict = verdict + string.Format(" | has(probe): before=%1 sameFrame=%2", m_bHasBefore.ToString(), m_bHasSameFrame.ToString());
		verdict = verdict + string.Format(" settled=%1 afterRemove=%2", m_bHasSettled.ToString(), hasAfterRemove.ToString());
		verdict = verdict + string.Format(" | GetObserversSP(): before=%1 sameFrame=%2", m_iCountBefore.ToString(), m_iCountSameFrame.ToString());
		verdict = verdict + string.Format(" settled=%1 (two inserts, one key) afterRemove=%2", m_iCountSettled.ToString(), countAfterRemove.ToString());

		Print(verdict);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes the marker entity. Called on every exit path, before the case reports.
	protected void CleanUp()
	{
		if (m_Marker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Marker);
			m_Marker = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The world's observer system, or null when it has none.
	protected ObserversSystem FindObserversSystem()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;

		return ObserversSystem.Cast(world.FindSystem(ObserversSystem));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] observers The system to ask.
	//! \return How many local (SP) observers currently exist.
	protected int CountObserversSP(notnull ObserversSystem observers)
	{
		array<vector> positions = new array<vector>();
		observers.GetObserversSP(positions);
		return positions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Searches a ring of candidates for a spot with no observer inside CLEAR_RANGE_SQ. The autotest
	//! camera is an observer, so "far from the camera" is measured, never assumed.
	//! \param[in] observers The system to ask.
	//! \return A clear position, or vector.Zero when this world has none.
	protected vector FindClearPosition(notnull ObserversSystem observers)
	{
		vector anchor = OVT_TEST_VirtualizationFixture.PickPosition();

		array<vector> offsets = new array<vector>();
		offsets.Insert(Vector(2000, 0, 0));
		offsets.Insert(Vector(-2000, 0, 0));
		offsets.Insert(Vector(0, 0, 2000));
		offsets.Insert(Vector(0, 0, -2000));
		offsets.Insert(Vector(2000, 0, 2000));
		offsets.Insert(Vector(-2000, 0, -2000));
		offsets.Insert(Vector(2000, 0, -2000));
		offsets.Insert(Vector(-2000, 0, 2000));
		offsets.Insert(Vector(4000, 0, 0));
		offsets.Insert(Vector(-4000, 0, 0));
		offsets.Insert(Vector(0, 0, 4000));
		offsets.Insert(Vector(0, 0, -4000));

		foreach (vector offset : offsets)
		{
			vector candidate = anchor + offset;
			if (!observers.HasObserverWithinRangeSq(candidate[0], candidate[2], CLEAR_RANGE_SQ))
				return candidate;
		}

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! A bare positional entity for the observer to follow. Never null at the call site: a null-entity
	//! InsertObserverSP hard-freezes the client.
	//! \param[in] pos Where to stand it.
	//! \return The entity, or null when the spawn failed.
	protected IEntity SpawnMarker(vector pos)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		return GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
	}
}

//------------------------------------------------------------------------------------------------
//! THE FREEZE GUARD: AddEntityObserver(null) is a safe false, and this run gets to finish.
//!
//! ⚠ A null-entity InsertObserverSP has ZERO vanilla script callers and hard-FROZE the game client
//! the one time a test case parked one - total log silence, the per-case timeout never fired, the
//! run had to be killed at 300 s. That failure mode produces a hung process, not a red case. So the
//! refusal is asserted here, and the case PASSING AT ALL is half of what it proves.
//!
//! Three claims, one per null-taking entry point, because each has its own early-out:
//!   - Add refuses and books NOTHING - a booked key with no observer behind it would make
//!     HasEntityObserver lie for the rest of the session;
//!   - Has answers false rather than dereferencing;
//!   - Remove answers false rather than dereferencing.
//!
//! ⚠ The count is compared against its own before-value, never against 0: the manager's observer map
//! is shared with any consumer live in this world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_EntityObserverRefusesNull : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		int before = virtualization.GetEntityObserverCount();

		if (virtualization.AddEntityObserver(null))
		{
			SetFailure("AddEntityObserver(null) returned TRUE - a null entity must be refused before it ever reaches InsertObserverSP, which hard-freezes the client (core context.md gotcha 0)");
			return true;
		}

		int after = virtualization.GetEntityObserverCount();
		if (after != before)
		{
			SetFailure("AddEntityObserver(null) was refused but still moved the observer count from "
				+ before.ToString() + " to " + after.ToString()
				+ " - it booked a key with no observer behind it, and HasEntityObserver would lie about it for the rest of the session");
			return true;
		}

		if (virtualization.HasEntityObserver(null))
		{
			SetFailure("HasEntityObserver(null) returned TRUE");
			return true;
		}

		if (virtualization.RemoveEntityObserver(null))
		{
			SetFailure("RemoveEntityObserver(null) returned TRUE - it claimed to have removed an observer for an entity that does not exist");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The entity-observer round trip: add, ask, count, add again, remove - all answered from core's own
//! map, on the frame each call returns.
//!
//! ⚠ Engine application of an SP observer is DEFERRED BY ONE FRAME in both directions, which is why
//! HasEntityObserver and GetEntityObserverCount answer from the manager's map instead of asking the
//! ObserversSystem. This case is the assertion that they do: a version that queried the engine would
//! read false right after the add and true right after the remove. Nothing here queries the engine,
//! so nothing needs a settling budget.
//!
//! Five claims, five things that rot independently:
//!   - a fresh add returns true, is visible to Has, and moves the count by exactly one;
//!   - the entity core keyed on is RESOLVABLE by that key (FindEntityByID) - the load-bearing
//!     assumption of the 2 s stale-entity sweep, which would otherwise delete every observer within
//!     two seconds of parking it;
//!   - a SECOND add on the same entity returns true and moves the count by NOTHING (the engine key is
//!     an identity, so a non-idempotent Add parks a second observer per call, unnoticed);
//!   - removing an entity nobody added an observer for is a quiet false;
//!   - the remove takes: Has goes false, the count returns, and a second remove is a quiet false.
//!
//! ⚠⚠ The markers are PREFAB-spawned and their ids are asserted before anything else. Two bare
//! GetGame().SpawnEntity(GenericEntity, ...) markers both answered GetID() with EntityID.INVALID - an
//! entity that is not world-registered has no id, and every such entity shares one. Core now refuses
//! an invalid id outright. The case waits for both ids to be valid AND distinct before asserting.
//!
//! ⚠ The observer is removed FIRST and the followed entity deleted several frames LATER, so no
//! engine observer is ever left following a deleted entity. Both delays are fixed, not retry budgets.
//!
//! ⚠ The markers are spawned NEAR the fixture position: the autotest world is small, and an entity
//! spawned outside the world's bounds may never be registered in it. The offset only keeps them clear
//! of where other cases register groups.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Virtualization_EntityObserverRoundTrip : SCR_AutotestCaseBase
{
	//! Frames allowed for both markers to become world-registered with distinct ids. Generous, because
	//! the alternative to waiting is asserting on an engine timing property. Bounded, and NOT a retry
	//! budget - the precondition is sampled once, when it is met or when the budget runs out.
	static const int ID_SETTLE_FRAMES = 30;

	//! Frames between removing the observer and deleting the entity it was following. The engine
	//! applies a removal on the next frame; this is 5x that. A FIXED DELAY - the case asserts nothing
	//! while it counts.
	static const int DELETE_DELAY_FRAMES = 5;

	//! How far from the fixture position to stand the markers. Small ON PURPOSE - see the header: an
	//! entity spawned outside the world's bounds may never be world-registered, and an unregistered
	//! entity has no EntityID, which is the failure this case was rewritten around.
	static const float MARKER_OFFSET = 150;

	protected int m_iPhase;
	protected int m_iFrames;
	protected IEntity m_Marker;
	protected IEntity m_Stranger;
	protected string m_sFailure;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		if (m_iPhase == 1)
			return AwaitEntityIds();

		return AwaitDeletion();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the two markers and hands over to the id wait. Deliberately asserts NOTHING about them
	//! yet: a freshly spawned entity may not be world-registered on this frame.
	//! \return True when the case is already finished (nothing usable was spawned).
	protected bool Arrange()
	{
		if (!OVT_Global.GetVirtualization())
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		vector anchor = OVT_TEST_VirtualizationFixture.PickPosition() + Vector(MARKER_OFFSET, 0, MARKER_OFFSET);

		m_Marker = SpawnMarker(anchor);
		m_Stranger = SpawnMarker(anchor + Vector(25, 0, 0));

		if (!m_Marker || !m_Stranger)
		{
			CleanUp();
			SetFailure("Could not spawn the throwaway marker entities - the case refuses to exercise the observer API with a null entity, which hard-freezes the client (core context.md gotcha 0)");
			return true;
		}

		m_iPhase = 1;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits until both markers are world-registered with distinct ids, then runs every assertion and
	//! removes the observer.
	//!
	//! The precondition is the case's own subject as much as a setup step: core keys its observer map
	//! on EntityID, and two entities sharing one id is precisely the bug this case caught.
	//! \return True when the case is finished on this frame.
	protected bool AwaitEntityIds()
	{
		m_iFrames++;

		if (!m_Marker || !m_Stranger)
		{
			CleanUp();
			SetFailure("A marker entity disappeared while waiting for its EntityID to be assigned");
			return true;
		}

		EntityID markerId = m_Marker.GetID();
		EntityID strangerId = m_Stranger.GetID();

		bool usable = markerId != EntityID.INVALID && strangerId != EntityID.INVALID && markerId != strangerId;
		if (!usable && m_iFrames < ID_SETTLE_FRAMES)
			return false;

		if (!usable)
		{
			CleanUp();
			SetFailure("After " + m_iFrames.ToString()
				+ " frames the two freshly spawned marker entities still do not have valid, distinct EntityIDs (invalid="
				+ (markerId == EntityID.INVALID).ToString() + "/" + (strangerId == EntityID.INVALID).ToString()
				+ ", equal=" + (markerId == strangerId).ToString()
				+ ") - so nothing in this world can be keyed by EntityID, and the observer API cannot be exercised here at all. That is a finding about entity registration, not a flaky assertion: record it before changing this case");
			return true;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			CleanUp();
			SetFailure("OVT_Global.GetVirtualization() went null between phases");
			return true;
		}

		m_sFailure = Verify(virtualization);

		// The observer comes off BEFORE the entity it follows is deleted, always - including on the
		// failing paths, which is why the failure text is carried instead of reported here.
		virtualization.RemoveEntityObserver(m_Marker);

		m_iPhase = 2;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every claim, in order, first failure wins.
	//! \param[in] virtualization The manager.
	//! \return The failure text, or "" when everything held.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization)
	{
		int before = virtualization.GetEntityObserverCount();

		if (virtualization.HasEntityObserver(m_Marker))
			return "The freshly spawned marker already reported an observer before one was added";

		if (!virtualization.AddEntityObserver(m_Marker))
			return "AddEntityObserver returned false for a real, non-null, world-registered entity on the server";

		if (!virtualization.HasEntityObserver(m_Marker))
			return "HasEntityObserver was false on the frame AddEntityObserver returned true - it is answering from the engine, whose application is deferred by a frame, instead of from core's own map";

		int afterAdd = virtualization.GetEntityObserverCount();
		if (afterAdd != before + 1)
			return "One AddEntityObserver moved the observer count from " + before.ToString() + " to " + afterAdd.ToString() + ", expected " + (before + 1).ToString();

		// -- the sweep's load-bearing assumption: the key core stored names the entity it came from --

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "GetGame().GetWorld() is null";

		if (world.FindEntityByID(m_Marker.GetID()) != m_Marker)
			return "The followed entity is not resolvable through the EntityID core keyed it on - the manager's 2 s stale-entity sweep would drop this observer within two seconds of parking it, and every other one too";

		// -- idempotence: the engine key is an identity, so a second add must reuse it --

		if (!virtualization.AddEntityObserver(m_Marker))
			return "A second AddEntityObserver on the SAME entity returned false - re-adding must reuse the existing key and report success";

		int afterSecondAdd = virtualization.GetEntityObserverCount();
		if (afterSecondAdd != afterAdd)
			return "A second AddEntityObserver on the same entity moved the count from " + afterAdd.ToString() + " to " + afterSecondAdd.ToString() + " - it minted a second key, so every re-add leaks an observer that nothing will ever remove";

		// -- an entity nobody ever added --

		if (virtualization.HasEntityObserver(m_Stranger))
			return "HasEntityObserver was true for a DIFFERENT entity no observer was ever added for - two entities are sharing one map entry, so removing either would silently remove the other's observer";

		if (virtualization.RemoveEntityObserver(m_Stranger))
			return "RemoveEntityObserver claimed to remove an observer for an entity that never had one";

		int afterStranger = virtualization.GetEntityObserverCount();
		if (afterStranger != afterSecondAdd)
			return "Removing an unknown entity moved the observer count from " + afterSecondAdd.ToString() + " to " + afterStranger.ToString();

		// -- the removal --

		if (!virtualization.RemoveEntityObserver(m_Marker))
			return "RemoveEntityObserver returned false for the entity it had just been added for";

		if (virtualization.HasEntityObserver(m_Marker))
			return "HasEntityObserver was still true after RemoveEntityObserver returned true";

		int afterRemove = virtualization.GetEntityObserverCount();
		if (afterRemove != before)
			return "After the removal the observer count is " + afterRemove.ToString() + ", not back at the pre-add " + before.ToString() + " - an observer was leaked, and a leaked one keeps everything near it materialised for the rest of the session";

		if (virtualization.RemoveEntityObserver(m_Marker))
			return "A second RemoveEntityObserver on the same entity returned true - removal is not idempotent";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Counts out the fixed delay, then deletes the markers and reports.
	//! \return True once the case is finished.
	protected bool AwaitDeletion()
	{
		m_iFrames++;
		if (m_iFrames < DELETE_DELAY_FRAMES)
			return false;

		CleanUp();

		if (m_sFailure != "")
		{
			SetFailure(m_sFailure);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes both markers. Called on every exit path, before the case reports.
	protected void CleanUp()
	{
		if (m_Marker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Marker);
			m_Marker = null;
		}

		if (m_Stranger)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Stranger);
			m_Stranger = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A throwaway positional entity for an observer to follow.
	//!
	//! ⚠ A PREFAB spawn, not GetGame().SpawnEntity(GenericEntity, ...) - two bare class spawns came
	//! back sharing one EntityID, which core now refuses outright. This uses the config's wait-waypoint
	//! prefab, untracked for persistence and belonging to no group. The bare spawn survives only as the
	//! fallback for a world with no config component.
	//! \param[in] pos Where to stand it.
	//! \return The entity, or null when the spawn failed.
	protected IEntity SpawnMarker(vector pos)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config)
		{
			IEntity waypoint = config.SpawnWaitWaypoint(pos, 1);
			if (waypoint)
				return waypoint;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		return GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
	}
}

//------------------------------------------------------------------------------------------------
//! The SHIPPED "Town Patrol" config resolves, and its patrol module answers with a real, CYCLING plan.
//!
//! Since 2026-08-21 the type is TOWN_SWEEP and the plan is ROLLED PER GROUP: a house-to-house SEARCH
//! route or a loose un-snapped ring. Both cycle and both are movable, so the shape assertions hold
//! whichever half the roll lands on; the printout names which one this run got.
//!
//! The Logic tier pins the plan factory's geometry, but the factory is world-free statics that know
//! nothing about which config is shipped. Between the two sits the authored config, and four things
//! can silently go wrong there:
//!   - the registry stops carrying "Town Patrol" - the deployment would never be created again, and
//!     NOTHING would log it;
//!   - the config loses its patrol behaviour module - every group registers with a null plan and
//!     every town patrol stands still forever, which reads exactly like broken virtualization;
//!   - the plan comes back EMPTY or ragged - RegisterGroup refuses it outright, so the patrol is
//!     silently never registered;
//!   - the plan stops CYCLING or loses its movable point. A plan is the ONLY opt-in for being walked
//!     while dormant: a patrol with nothing movable is a garrison, and one that lost its cycle
//!     guards a quarter of the town for the rest of the campaign.
//!
//! Asked off the config TEMPLATE with no deployment behind it: BuildVirtualPlan falls back to the
//! group's own position when there is no centre to circle, so the shipped answer's shape is
//! assertable without creating a marker entity and leaking a repeating 10 s UpdateDeployment into
//! the shared test world. Nothing is registered.
//!
//! ⚠ The positions are NOT asserted - they are road-snapped against the live world, so their values
//! are a property of the terrain. The Logic tier owns the geometry.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_TownPatrolPlanCycles : SCR_AutotestCaseBase
{
	//! The shipped config this case is about.
	static const string CONFIG_NAME = "Town Patrol";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so no shipped deployment config can be resolved at all");
			return true;
		}

		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(CONFIG_NAME);
		if (!config)
		{
			SetFailure("The deployment registry does not resolve '%1' - the shipped town patrol can never be created", CONFIG_NAME);
			return true;
		}

		if (!config.IsValidConfig())
		{
			SetFailure("Config '%1' resolves but is not valid (no name, no modules, or no spawning module)", CONFIG_NAME);
			return true;
		}

		OVT_PatrolBehaviorDeploymentModule patrol = FindPatrolModule(config);
		if (!patrol)
		{
			SetFailure("Config '%1' carries no OVT_PatrolBehaviorDeploymentModule - every group it registers would get a null plan and stand still forever", CONFIG_NAME);
			return true;
		}

		// ⚠ TOWN_SWEEP - NOT PERIMETER and NOT PERIMETER_BASE. PERIMETER is the old ROAD-SNAPPED ring,
		// which parked every town patrol in the middle of a road at each corner; PERIMETER_BASE looks for
		// a base controller within 250 m, which a town centre does not have, and would warn per plan and
		// walk a square. A config silently reverting to PERIMETER would put the patrols back on the
		// tarmac and nothing else would say so.
		if (patrol.m_ePatrolType != OVT_PatrolType.TOWN_SWEEP)
		{
			SetFailure("Config '%1' authors patrol type %2, not TOWN_SWEEP - the town patrol would go back to standing on road corners instead of sweeping the town's houses", CONFIG_NAME,
				typename.EnumToString(OVT_PatrolType, patrol.m_ePatrolType));
			return true;
		}

		// The sweep's two authored knobs. A house count of 0 rolls the ring every time (the house branch
		// picks nothing and falls through), and a chance of 0 never enters it; either silently turns the
		// sweep back into a plain ring, so both are pinned against the shipped config.
		if (patrol.m_iSweepHouseCount <= 0)
		{
			SetFailure("Config '%1' authors m_iSweepHouseCount %2 - no house would ever be searched and every group would walk the ring", CONFIG_NAME,
				patrol.m_iSweepHouseCount.ToString());
			return true;
		}

		if (patrol.m_fSweepHouseChance <= 0)
		{
			SetFailure("Config '%1' authors m_fSweepHouseChance %2 - the house route could never be rolled", CONFIG_NAME,
				patrol.m_fSweepHouseChance.ToString());
			return true;
		}

		vector groupPosition = OVT_TEST_VirtualizationFixture.PickPosition();
		OVT_VirtualWaypointPlan plan = patrol.BuildVirtualPlan(groupPosition);

		string failure = VerifyPlan(plan);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print(string.Format("'%1' builds a %2-point cycling plan with %3 movable point(s) (%4)", CONFIG_NAME,
			plan.m_aPositions.Count().ToString(), CountMovable(plan).ToString(), DescribeShape(plan)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to search.
	//! \return Its first patrol behaviour module, or null.
	protected OVT_PatrolBehaviorDeploymentModule FindPatrolModule(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = config.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			OVT_PatrolBehaviorDeploymentModule patrol = OVT_PatrolBehaviorDeploymentModule.Cast(behaviorModule);
			if (patrol)
				return patrol;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] plan The plan the patrol module answered with.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyPlan(OVT_VirtualWaypointPlan plan)
	{
		if (!plan)
			return string.Format("The patrol module of '%1' answered with no plan at all - its groups would register with no waypoints, which is a garrison", CONFIG_NAME);

		int count = plan.m_aPositions.Count();
		if (count == 0)
			return "The plan is empty - a town patrol would register with no waypoints and never move";

		if (plan.m_aTypes.Count() != count || plan.m_aParams.Count() != count)
			return string.Format("The plan's parallel arrays are ragged (%1 positions, %2 types, %3 params) - RegisterGroup refuses a ragged plan outright, so the patrol would silently never be registered",
				count.ToString(), plan.m_aTypes.Count().ToString(), plan.m_aParams.Count().ToString());

		if (!plan.m_bCycle)
			return "The plan does not cycle - the patrol would walk to its last corner and guard that quarter of the town for the rest of the campaign";

		if (CountMovable(plan) == 0)
			return "The plan contains no movable point - a plan is the ONLY opt-in for being walked while dormant, so this patrol would stand still exactly like the old proximity-toggled one";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] plan The plan to count.
	//! \return How many of its points the movement tick would advance along. SEARCH counts: it is walked
	//! to and then held, exactly as a PATROL corner with its WAIT is.
	protected int CountMovable(notnull OVT_VirtualWaypointPlan plan)
	{
		int movable = 0;
		foreach (int type : plan.m_aTypes)
		{
			if (type == OVT_EVirtualWaypointType.PATROL || type == OVT_EVirtualWaypointType.MOVE || type == OVT_EVirtualWaypointType.SEARCH)
				movable++;
		}

		return movable;
	}

	//------------------------------------------------------------------------------------------------
	//! Which half of the sweep this plan is - the roll is per group, so either is a correct answer, and
	//! the printout says which one this run got. The shape itself is what VerifyPlan checks.
	protected string DescribeShape(notnull OVT_VirtualWaypointPlan plan)
	{
		foreach (int type : plan.m_aTypes)
		{
			if (type == OVT_EVirtualWaypointType.SEARCH)
				return "house route";
		}

		return "loose ring";
	}
}

//------------------------------------------------------------------------------------------------
//! The EnsureGroups contract, driven against the real core through the real key statics: register
//! under a deployment owner key, reclaim it, prove a second pass adds NOTHING, then unregister.
//!
//! Idempotence is the point. Every registration path converges to a wanted count rather than
//! spawning one, because the same method is reached from a deployment's activation, from the
//! manager's records-restored fan-out and from the paid-for rebuy, in any order and any number of
//! times. If reclaim-before-register broke, a continued campaign would come back with two of every
//! patrol and nothing would notice - both copies are legal registrations.
//!
//! The cycle is SIMULATED rather than driven through a live module: driving the module needs a
//! deployment marker entity, which leaks a repeating 10 s UpdateDeployment into the shared test
//! world for every case after it. What is exercised is exactly what the module composes.
//!
//! ⚠ spawnDistanceOverride = 0, NOT -1. Zero is the engine's Manual lifecycle policy; the autotest
//! camera IS an observer, so a registration at the global 1750 m ring would put real soldiers on the
//! ground in the middle of the suite.
//!
//! ⚠ Null plans, and everything registered here is unregistered inside the SAME frame. Either
//! property alone makes the fixture safe from the movement tick; this case has both.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_EnsureGroupsIsIdempotent : SCR_AutotestCaseBase
{
	//! The owner system every deployment-registered group carries.
	static const string OWNER_SYSTEM = "deployment";

	//! Stands in for a shipped config name in the key derivation. Deliberately carries a space and a '#'
	//! so the sanitisation the composed key depends on is exercised on the way through.
	static const string CONFIG_NAME = "Test #Patrol";

	//! Stands in for the authored module name.
	static const string MODULE_NAME = "Spawn Infantry";

	//! How many groups the simulated module wants to hold.
	static const int WANTED = 2;

	//! Manual lifecycle policy: never materialise by proximity. See the header.
	static const int SPAWN_DISTANCE_NEVER = 0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so registration cannot be exercised");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		string ownerKey = BuildOwnerKey(position);

		int before = virtualization.GetGroupCount();
		array<int> handles = new array<int>();

		string failure = RunCycle(virtualization, ownerKey, factionKey, groupName, position, handles);

		// Cleanup BEFORE reporting, on every path.
		foreach (int handle : handles)
		{
			virtualization.UnregisterGroup(handle);
		}

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		if (!virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey).IsEmpty())
		{
			SetFailure("FindGroupsByOwner still answers for '%1' after every handle was unregistered", ownerKey);
			return true;
		}

		if (virtualization.GetGroupCount() != before)
		{
			SetFailure("The registry holds %1 records after cleanup, expected the %2 it started with",
				virtualization.GetGroupCount().ToString(), before.ToString());
			return true;
		}

		PrintFormat("EnsureGroups converged to %1 group(s) under '%2', a second pass added nothing, and unregistering emptied the owner", WANTED.ToString(), ownerKey);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Two convergence passes over one owner key. The second must add nothing.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey The composed owner key.
	//! \param[in] factionKey A faction this world resolves.
	//! \param[in] groupName A composition that faction resolves.
	//! \param[in] position Where to register.
	//! \param[out] handles Every handle registered, for cleanup - filled even on a failure path.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string RunCycle(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey,
		string factionKey, string groupName, vector position, notnull array<int> handles)
	{
		if (!virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey).IsEmpty())
			return string.Format("Owner key '%1' already has registered groups before this case ran - a previous case leaked, or the key arithmetic collides", ownerKey);

		// PASS 1: nothing held, so the whole wanted count is missing.
		int missing = OVT_DeploymentVirtualKey.MissingCount(WANTED, 0);
		if (missing != WANTED)
			return string.Format("MissingCount(%1, 0) answered %2 - a fresh module would register the wrong number of groups", WANTED.ToString(), missing.ToString());

		for (int i = 0; i < missing; i++)
		{
			int handle = virtualization.RegisterGroup(OWNER_SYSTEM, ownerKey, factionKey, groupName,
				position, null, SPAWN_DISTANCE_NEVER, SCR_EAISpawnImportance.NORMAL);

			if (handle == -1)
				return string.Format("RegisterGroup refused group %1 of %2 for a composition the faction registry resolves (%3 '%4')",
					(i + 1).ToString(), missing.ToString(), factionKey, groupName);

			handles.Insert(handle);
		}

		// The registration really is inert - nothing can materialise it by walking near it.
		foreach (int registeredHandle : handles)
		{
			if (virtualization.GetSpawnDistance(registeredHandle) != 0)
				return string.Format("Handle %1 resolved a spawn distance of %2 m from an override of 0 - this fixture would materialise real soldiers next to the autotest camera",
					registeredHandle.ToString(), virtualization.GetSpawnDistance(registeredHandle).ToString());
		}

		// PASS 2: reclaim. This is the step every real caller starts with.
		array<int> reclaimed = virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey);
		if (reclaimed.Count() != WANTED)
			return string.Format("FindGroupsByOwner reclaimed %1 handle(s) for '%2', expected %3 - a module that cannot find its own groups registers a second set on top of them",
				reclaimed.Count().ToString(), ownerKey, WANTED.ToString());

		foreach (int handle : handles)
		{
			if (!reclaimed.Contains(handle))
				return string.Format("Handle %1 was registered under '%2' but the reclaim did not return it", handle.ToString(), ownerKey);
		}

		// ...and the shortfall after reclaiming is nothing, so the second pass registers nothing.
		int missingAfter = OVT_DeploymentVirtualKey.MissingCount(WANTED, reclaimed.Count());
		if (missingAfter != 0)
			return string.Format("MissingCount(%1, %2) answered %3 - a second convergence would register %3 duplicate group(s), which is how a continued campaign doubles every patrol",
				WANTED.ToString(), reclaimed.Count().ToString(), missingAfter.ToString());

		if (virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey).Count() != WANTED)
			return "The owner's group count moved between the two reclaim calls without anything being registered";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Composes the owner key exactly as a spawning module does: the deployment's derived key, then the
	//! module's tag.
	//! \param[in] position The marker position the key is derived from.
	//! \return The owner key.
	protected string BuildOwnerKey(vector position)
	{
		string deploymentKey = OVT_DeploymentVirtualKey.DeriveKey(CONFIG_NAME, position[0], position[2]);
		deploymentKey = OVT_DeploymentVirtualKey.Disambiguate(deploymentKey, 0);

		return OVT_DeploymentVirtualKey.OwnerKey(deploymentKey, OVT_DeploymentVirtualKey.ModuleTag(MODULE_NAME, 0));
	}
}

//------------------------------------------------------------------------------------------------
//! The SHIPPED "Tower Garrison" config resolves, and everything about it that decides whether a
//! garrison behaves like a garrison is what it is supposed to be.
//!
//! Radio tower garrisons used to be bespoke code inside the occupying faction manager's 9 s loop;
//! they are now this one authored file, and every claim below is silent when it breaks:
//!   - the registry stops carrying "Tower Garrison" - no tower would ever get a garrison again, and
//!     the only symptom would be quiet towers;
//!   - the config is not valid (no spawning module) - FindBestDeploymentConfig skips it silently;
//!   - the plan stops being a ONE-POINT, NON-CYCLING DEFEND. A plan is the only opt-in for being
//!     walked while dormant, so a garrison that acquired a movable point would WANDER OFF ITS TOWER
//!     while nobody was watching. Nothing else asserts "garrisons never wander";
//!   - the importance stops being HIGH - the nastiest, because a garrison at the vanilla LOW/NORMAL
//!     tier is capped lower in the AI budget and evicted first, so on a busy server it never appears.
//!
//! ⚠ The CLONE is checked as well as the template: modules are cloned by hand per deployment and a
//! CloneModule that forgets an attribute ships the class default instead of the authored value,
//! which is exactly how m_fMaxCruiseSpeed was lost on the vehicle module for a release.
//!
//! Asked off the config template with no deployment behind it, as the Town Patrol case is, so
//! nothing is registered and no repeating UpdateDeployment leaks into the shared test world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_TowerGarrisonHoldsItsPost : SCR_AutotestCaseBase
{
	//! The shipped config this case is about.
	static const string CONFIG_NAME = "Tower Garrison";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so no shipped deployment config can be resolved at all");
			return true;
		}

		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(CONFIG_NAME);
		if (!config)
		{
			SetFailure("The deployment registry does not resolve '%1' - no radio tower would ever be garrisoned again", CONFIG_NAME);
			return true;
		}

		if (!config.IsValidConfig())
		{
			SetFailure("Config '%1' resolves but is not valid (no name, no modules, or no spawning module) - the evaluator skips it in silence", CONFIG_NAME);
			return true;
		}

		string failure = VerifyPlan(config);
		if (failure == "")
			failure = VerifyImportance(config);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("'%1' builds a one-point non-cycling DEFEND plan and registers at HIGH importance, template and clone", CONFIG_NAME);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The plan half: one DEFEND point, no cycle, nothing movable.
	//! \param[in] config The shipped config.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyPlan(notnull OVT_DeploymentConfig config)
	{
		OVT_PatrolBehaviorDeploymentModule patrol;
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = config.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			patrol = OVT_PatrolBehaviorDeploymentModule.Cast(behaviorModule);
			if (patrol)
				break;
		}

		if (!patrol)
			return string.Format("Config '%1' carries no OVT_PatrolBehaviorDeploymentModule - its groups would register with no plan at all", CONFIG_NAME);

		vector groupPosition = OVT_TEST_VirtualizationFixture.PickPosition();
		OVT_VirtualWaypointPlan plan = patrol.BuildVirtualPlan(groupPosition);

		if (!plan)
			return string.Format("The patrol module of '%1' answered with no plan - a garrison with no waypoints is legal, but then this case cannot tell a DEFEND post from a lost setting", CONFIG_NAME);

		int count = plan.m_aPositions.Count();
		if (count != 1)
			return string.Format("The plan has %1 point(s), expected exactly 1 - a garrison holds ONE post; more than one means the patrol type is no longer DEFEND", count.ToString());

		if (plan.m_aTypes.Count() != count || plan.m_aParams.Count() != count)
			return string.Format("The plan's parallel arrays are ragged (%1 positions, %2 types, %3 params) - RegisterGroup refuses a ragged plan outright, so the garrison would silently never be registered",
				count.ToString(), plan.m_aTypes.Count().ToString(), plan.m_aParams.Count().ToString());

		if (plan.m_aTypes[0] != OVT_EVirtualWaypointType.DEFEND)
			return string.Format("The plan's only point is type %1, expected DEFEND", plan.m_aTypes[0].ToString());

		if (plan.m_bCycle)
			return "The plan cycles - a cycling plan is walked by the movement tick, so the garrison would leave its tower while nobody was watching";

		foreach (int type : plan.m_aTypes)
		{
			if (type == OVT_EVirtualWaypointType.PATROL || type == OVT_EVirtualWaypointType.MOVE)
				return "The plan contains a movable point - the movement tick would walk this garrison off its tower";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The budget half: HIGH on the authored template AND on the clone a real deployment gets.
	//! \param[in] config The shipped config.
	//! \return An empty string when both hold, or the broken one.
	protected string VerifyImportance(notnull OVT_DeploymentConfig config)
	{
		OVT_InfantrySpawningDeploymentModule infantry;
		array<OVT_BaseSpawningDeploymentModule> spawningModules = config.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			infantry = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
			if (infantry)
				break;
		}

		if (!infantry)
			return string.Format("Config '%1' carries no OVT_InfantrySpawningDeploymentModule - there is nothing to garrison the tower with", CONFIG_NAME);

		if (infantry.m_eImportance != SCR_EAISpawnImportance.HIGH)
			return string.Format("The authored importance is %1, expected HIGH - a garrison below HIGH loses the AI spawn-budget race on a busy server and silently never appears",
				typename.EnumToString(SCR_EAISpawnImportance, infantry.m_eImportance));

		OVT_InfantrySpawningDeploymentModule clone = OVT_InfantrySpawningDeploymentModule.Cast(infantry.CloneModule());
		if (!clone)
			return "CloneModule() on the infantry module did not return an infantry module";

		if (clone.m_eImportance != SCR_EAISpawnImportance.HIGH)
			return string.Format("The CLONE's importance is %1 while the template's is HIGH - CloneModule drops the attribute, so every real deployment ships at the wrong tier",
				typename.EnumToString(SCR_EAISpawnImportance, clone.m_eImportance));

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A radio tower's position classifies as RADIO_TOWER - IN ADDITION to whatever else it was.
//!
//! GetLocationTypeAtPosition is a first-match precedence chain returning a SINGLE flag. Every radio
//! tower worth garrisoning is near something, so before the RADIO_TOWER bit was OR-ed in on top, a
//! tower inside a town's bounds classified as TOWN, the tower-only config never matched, and the
//! only symptom was ungarrisoned towers with no error anywhere.
//!
//! Both halves of D19, because the fix is only correct if it is additive:
//!   - at a tower, the result CONTAINS the RADIO_TOWER bit;
//!   - at a tower, it still contains every bit the precedence chain produced on its own;
//!   - away from every tower, it is EXACTLY the precedence value (conditional, not welded on).
//!
//! Reads the LIVE tower list rather than a fixed position: the test world defines one radio tower
//! and Eden two, so a magic coordinate would be wrong in one of them. Nothing is mutated.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_RadioTowerLocationTypeIsOredIn : SCR_AutotestCaseBase
{
	//! How far from every known tower the "no tower here" probe is taken. Well outside the 300 m
	//! classification ring on any map, and no terrain is touched - the classification does no ground
	//! trace, so an off-map probe is legal.
	static const float FAR_FROM_TOWERS = 5000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_RadioTowers || occupying.m_RadioTowers.IsEmpty())
		{
			SetFailure("This world has no radio towers, so the classification cannot be exercised - InitializeBases() found none");
			return true;
		}

		vector towerPos = occupying.m_RadioTowers[0].location;

		OVT_LocationTypeFlag primary = manager.GetPrimaryLocationTypeAtPosition(towerPos);
		OVT_LocationTypeFlag full = manager.GetLocationTypeAtPosition(towerPos);

		if ((full & OVT_LocationTypeFlag.RADIO_TOWER) == 0)
		{
			SetFailure("A position at a radio tower classifies as %1 with no RADIO_TOWER bit - the Tower Garrison config can never match a candidate and no tower is ever garrisoned",
				full.ToString());
			return true;
		}

		if ((full & primary) != primary)
		{
			SetFailure("Classifying a tower position returned %1, which no longer contains the %2 the precedence chain produced - the RADIO_TOWER bit REPLACED a location kind instead of joining it, and whichever config wanted that kind has silently lost this position",
				full.ToString(), primary.ToString());
			return true;
		}

		vector awayPos = FindPositionAwayFromTowers(occupying);
		OVT_LocationTypeFlag awayPrimary = manager.GetPrimaryLocationTypeAtPosition(awayPos);
		OVT_LocationTypeFlag awayFull = manager.GetLocationTypeAtPosition(awayPos);

		if (awayFull != awayPrimary)
		{
			SetFailure("A position %1 m from every tower classifies as %2 but the precedence chain says %3 - the RADIO_TOWER bit is being added unconditionally, so every candidate on the map would look like a tower",
				FAR_FROM_TOWERS.ToString(), awayFull.ToString(), awayPrimary.ToString());
			return true;
		}

		PrintFormat("A tower position classifies as %1 (precedence alone said %2); a position far from every tower is unchanged at %3",
			full.ToString(), primary.ToString(), awayFull.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A position at least FAR_FROM_TOWERS from every tower in this world.
	//! \param[in] occupying The occupying faction manager, for its tower list.
	//! \return A position no tower is near.
	protected vector FindPositionAwayFromTowers(notnull OVT_OccupyingFactionManager occupying)
	{
		// Walk outwards from the first tower until nothing is within range. Two towers on Eden and
		// one in the test world, so this settles on the first or second step.
		vector candidate = occupying.m_RadioTowers[0].location;

		for (int attempt = 1; attempt <= 10; attempt++)
		{
			candidate = occupying.m_RadioTowers[0].location + Vector(FAR_FROM_TOWERS * attempt, 0, FAR_FROM_TOWERS * attempt);

			bool clear = true;
			foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
			{
				if (tower && vector.Distance(candidate, tower.location) < FAR_FROM_TOWERS)
				{
					clear = false;
					break;
				}
			}

			if (clear)
				return candidate;
		}

		return candidate;
	}
}

//------------------------------------------------------------------------------------------------
//! THE PHASE'S HEADLINE CLAIM: a tower changes hands when its garrison is KILLED, and at no other
//! time - and then exactly once.
//!
//! Drive away from a garrisoned tower until the garrison despawns, drive back: the tower must still
//! be enemy-held. The old code flipped the tower whenever its list of garrison entity ids was empty
//! on a tick, and that list was emptied by DRIVING AWAY (and by any QRF anywhere on the map). The
//! new rule is that capture is driven by the deployment's eliminated flag, which the virtualization
//! core sets only when a group's survivor mask reports every slot dead.
//!
//! The deaths are real, through ReportMemberKilled, and the wipe that follows is core's own - the
//! case never sets an eliminated flag by hand. Three states, the middle one being the point:
//!   1. two groups registered, nothing killed -> no capture;
//!   2. ONE group wiped, the other untouched  -> still no capture (this is what a despawn would look
//!      like if the accounting ever regressed);
//!   3. the second group wiped as well        -> capture, ONCE. A fourth call does nothing.
//!
//! Driven through EvaluateCapture() rather than OnUpdate: the only difference is where the three
//! inputs come from, and OnUpdate would need a live deployment marker entity.
//!
//! ⚠ spawnDistanceOverride = 0 on every registration (Manual policy) because the autotest camera IS
//! an observer. Null plans, so the movement tick has nothing to walk.
//!
//! ⚠ This case really FLIPS a tower, sending the live capture notification and broadcast. The
//! tower's faction is captured before and restored after, on every path.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Deployments_TowerCaptureOnlyOnRealWipe : SCR_AutotestCaseBase
{
	//! The owner system every deployment-registered group carries.
	static const string OWNER_SYSTEM = "deployment";

	//! Manual lifecycle policy: never materialise by proximity.
	static const int SPAWN_DISTANCE_NEVER = 0;

	//! How many groups stand in for the garrison. Two, because the claim that ONE dead group is not
	//! a wipe needs a second one to still be standing.
	static const int GARRISON_GROUPS = 2;

	//! Handles the core has not yet told us are wiped. This is the whole of the bridge between core
	//! deaths and a deployment's eliminated flag, and it is the same rule the infantry spawning
	//! module applies: the flag goes up when the last handle is gone and the module ever held one.
	protected ref array<int> m_aLiveHandles;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		m_aLiveHandles = new array<int>();

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_RadioTowers || occupying.m_RadioTowers.IsEmpty())
		{
			SetFailure("This world has no radio towers, so there is nothing to capture");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();
		int playerFaction = config.GetPlayerFactionIndex();
		if (occupyingFaction == playerFaction || occupyingFaction < 0 || playerFaction < 0)
		{
			SetFailure("The occupying faction (%1) and the player faction (%2) do not resolve to two different factions, so a capture would be unobservable",
				occupyingFaction.ToString(), playerFaction.ToString());
			return true;
		}

		OVT_RadioTowerData tower = occupying.m_RadioTowers[0];
		int originalFaction = tower.faction;

		// The deployment under test garrisons a tower its own faction holds, which is the only state
		// the evaluator would ever have created one in.
		tower.faction = occupyingFaction;

		string failure = RunCapture(virtualization, occupying, tower, occupyingFaction, playerFaction);

		// Cleanup BEFORE reporting, on every path: release anything still registered and hand the
		// tower back exactly as it was found.
		foreach (int handle : m_aLiveHandles)
		{
			virtualization.UnregisterGroup(handle);
		}
		m_aLiveHandles.Clear();
		tower.faction = originalFaction;

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A partially wiped garrison did not capture the tower; wiping the last group captured it once, and a repeat call did nothing");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a stand-in garrison, kills it a group at a time, and asserts what the capture module
	//! does at each step.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] occupying The occupying faction manager, which owns the tower record.
	//! \param[in] tower The tower under test.
	//! \param[in] occupyingFaction The faction the stand-in deployment belongs to.
	//! \param[in] playerFaction The faction a capture hands the tower to.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string RunCapture(notnull OVT_VirtualizationManagerComponent virtualization,
		notnull OVT_OccupyingFactionManager occupying, notnull OVT_RadioTowerData tower,
		int occupyingFaction, int playerFaction)
	{
		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
			return "No faction in this world defines a resolvable group registry entry, so no garrison can be registered";

		string ownerKey = OVT_DeploymentVirtualKey.OwnerKey(
			OVT_DeploymentVirtualKey.DeriveKey("Tower Garrison", tower.location[0], tower.location[2]),
			OVT_DeploymentVirtualKey.ModuleTag("Tower Garrison", 0));

		for (int i = 0; i < GARRISON_GROUPS; i++)
		{
			int handle = virtualization.RegisterGroup(OWNER_SYSTEM, ownerKey, factionKey, groupName,
				tower.location, null, SPAWN_DISTANCE_NEVER, SCR_EAISpawnImportance.HIGH);

			if (handle == -1)
				return string.Format("RegisterGroup refused garrison group %1 of %2 for a composition the faction registry resolves (%3 '%4')",
					(i + 1).ToString(), GARRISON_GROUPS.ToString(), factionKey, groupName);

			m_aLiveHandles.Insert(handle);
		}

		virtualization.GetOnGroupWiped().Insert(OnGroupWiped);
		string failure = WalkTheThreeStates(virtualization, tower, occupyingFaction, playerFaction);
		virtualization.GetOnGroupWiped().Remove(OnGroupWiped);

		return failure;
	}

	//------------------------------------------------------------------------------------------------
	//! Alive -> partially wiped -> wiped, asserting the capture module at each step.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string WalkTheThreeStates(notnull OVT_VirtualizationManagerComponent virtualization,
		notnull OVT_RadioTowerData tower, int occupyingFaction, int playerFaction)
	{
		OVT_RadioTowerCaptureBehaviorDeploymentModule capture = new OVT_RadioTowerCaptureBehaviorDeploymentModule();
		capture.m_fMaxDistance = 300;

		// STATE 1: the garrison is registered and untouched.
		if (capture.EvaluateCapture(IsEliminated(), tower.location, occupyingFaction))
			return "The capture module fired with a fully alive garrison - a tower would flip the moment its deployment was created";

		if (tower.faction != occupyingFaction)
			return "The tower changed hands with a fully alive garrison";

		// STATE 2: one group wiped, one still standing. This is the state a proximity despawn would
		// be mistaken for if the wipe accounting ever regressed to counting what is materialised.
		string wipeFailure = WipeOneGroup(virtualization);
		if (wipeFailure != "")
			return wipeFailure;

		if (m_aLiveHandles.Count() != GARRISON_GROUPS - 1)
			return string.Format("After wiping one group the case still holds %1 live handle(s), expected %2 - the core did not raise its group-wiped event",
				m_aLiveHandles.Count().ToString(), (GARRISON_GROUPS - 1).ToString());

		if (capture.EvaluateCapture(IsEliminated(), tower.location, occupyingFaction))
			return "The capture module fired with one garrison group still alive - a partial loss is not a wipe, and this is exactly how walking away used to take a tower";

		if (tower.faction != occupyingFaction)
			return "The tower changed hands while one garrison group was still alive";

		// STATE 3: the last group dies. Now, and only now, the tower changes hands.
		wipeFailure = WipeOneGroup(virtualization);
		if (wipeFailure != "")
			return wipeFailure;

		if (!m_aLiveHandles.IsEmpty())
			return string.Format("After wiping every group the case still holds %1 live handle(s) - the core did not raise its group-wiped event for the last one",
				m_aLiveHandles.Count().ToString());

		if (!capture.EvaluateCapture(IsEliminated(), tower.location, occupyingFaction))
			return "The capture module did NOT fire after the whole garrison was killed - clearing a tower would no longer take it";

		if (tower.faction != playerFaction)
			return string.Format("The capture reported success but the tower's faction is %1, expected the player faction %2",
				tower.faction.ToString(), playerFaction.ToString());

		// ...and only once. The eliminated flag stays up for as long as the deployment lives, and the
		// module is asked again every ~10 s, so without a latch this would re-notify forever.
		if (capture.EvaluateCapture(IsEliminated(), tower.location, occupyingFaction))
			return "The capture module fired a SECOND time - the edge latch is gone and every tick would re-capture and re-notify";

		if (tower.faction != playerFaction)
			return "The tower's faction moved on the second capture call";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Kills every slot of the first live group through the public death seam.
	//! \param[in] virtualization The virtualization manager.
	//! \return An empty string on success, or what went wrong.
	protected string WipeOneGroup(notnull OVT_VirtualizationManagerComponent virtualization)
	{
		if (m_aLiveHandles.IsEmpty())
			return "Nothing left to wipe";

		int handle = m_aLiveHandles[0];

		int roster = virtualization.GetMemberCount(handle);
		if (roster < 1)
			return string.Format("Handle %1 has an empty roster, so no death can be reported against it and nothing below can mean anything", handle.ToString());

		for (int slot = 0; slot < roster; slot++)
		{
			virtualization.ReportMemberKilled(handle, slot);
		}

		if (virtualization.IsRegistered(handle))
			return string.Format("Handle %1 is still registered after all %2 of its slots were reported killed - the core did not wipe it",
				handle.ToString(), roster.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The eliminated flag as a spawning module computes it: every handle gone, having held some.
	//! \return Whether the stand-in garrison counts as wiped out.
	protected bool IsEliminated()
	{
		return m_aLiveHandles.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The core says one of our groups lost its last member.
	//! \param[in] handle The wiped group's registry handle.
	protected void OnGroupWiped(int handle)
	{
		m_aLiveHandles.RemoveItem(handle);
	}
}

//------------------------------------------------------------------------------------------------
//! Both SHIPPED vehicle patrol configs resolve, their crew compositions exist for every faction that
//! could field them, and their crews are registered ALWAYS-MATERIALISED.
//!
//! Every way this breaks is silent:
//!   - the registry stops carrying "Light Vehicle Patrol" or "Heavy Vehicle Patrol" - the evaluator
//!     would never create one again, and nothing would log it;
//!   - the config loses its vehicle spawning module or that module's crew group name - the trucks
//!     spawn and sit at the base forever with nobody in them;
//!   - the crew group name stops resolving for a faction. Core resolves by (factionKey, groupName)
//!     and REFUSES the registration when it cannot, so a faction whose registry lost "light_patrol"
//!     fields driverless vehicles with one warning per attempt. BOTH shipped occupying factions are
//!     checked, because a registry entry can be dropped from one .conf and not the other;
//!   - the crew spawn distance stops being always-materialised. A crew at the global ring (-1) goes
//!     DORMANT with no observer near, and a dormant group holding a route plan is walked along it in
//!     a straight line while its truck stays parked - so the crew materialises kilometres from its
//!     vehicle. A crew at 0 gets Manual policy and never materialises at all. Neither errors.
//!
//! ⚠ The CLONE is checked as well as the template - a CloneModule that forgets an attribute ships
//! the class default, which is how m_fMaxCruiseSpeed was lost on this very module for a release.
//!
//! Nothing is registered or mutated. The route plan's shape is pinned in the Logic tier; asking the
//! multi-town module for a plan here would depend on how many towns this world has inside the
//! config's 2500 m search radius.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_VehiclePatrolCrewsResolve : SCR_AutotestCaseBase
{
	//! The two shipped vehicle patrol configs.
	static const string LIGHT_CONFIG = "Light Vehicle Patrol";
	static const string HEAVY_CONFIG = "Heavy Vehicle Patrol";

	//! How many factions must carry a group registry for the composition half to mean anything. Both
	//! shipped occupying factions do; a world where fewer do would make this case pass vacuously, so it
	//! fails loudly instead.
	static const int MIN_FACTIONS_WITH_REGISTRY = 2;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so no shipped deployment config can be resolved at all");
			return true;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - vehicle crews are registered groups, so without it there is nothing to assert");
			return true;
		}

		string failure = VerifyConfig(manager, virtualization, LIGHT_CONFIG);
		if (failure == "")
			failure = VerifyConfig(manager, virtualization, HEAVY_CONFIG);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("'%1' and '%2' both resolve, their crews resolve for every faction with a group registry, and both register always-materialised",
			LIGHT_CONFIG, HEAVY_CONFIG);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every claim about one shipped vehicle patrol config.
	//! \param[in] manager The deployment manager.
	//! \param[in] virtualization The virtualization manager, for the global spawn ring.
	//! \param[in] configName The config to check.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyConfig(notnull OVT_DeploymentManagerComponent manager, notnull OVT_VirtualizationManagerComponent virtualization, string configName)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(configName);
		if (!config)
			return string.Format("The deployment registry does not resolve '%1' - that vehicle patrol can never be created again", configName);

		if (!config.IsValidConfig())
			return string.Format("Config '%1' resolves but is not valid (no name, no modules, or no spawning module) - the evaluator skips it in silence", configName);

		OVT_VehicleSpawningDeploymentModule vehicleModule = FindVehicleModule(config);
		if (!vehicleModule)
			return string.Format("Config '%1' carries no OVT_VehicleSpawningDeploymentModule - there is nothing to put on the road", configName);

		if (vehicleModule.m_sCrewGroupType.IsEmpty())
			return string.Format("The vehicle module of '%1' authors no crew group type - its trucks would spawn with nobody in them and never move", configName);

		string failure = VerifyCrewSpawnDistance(virtualization, vehicleModule, configName);
		if (failure != "")
			return failure;

		return VerifyComposition(configName, vehicleModule.m_sCrewGroupType);
	}

	//------------------------------------------------------------------------------------------------
	//! The always-materialised half, on the template AND on the clone a real deployment gets.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] vehicleModule The config's vehicle module.
	//! \param[in] configName The config being checked, for the message.
	//! \return An empty string when both hold, or the broken one.
	protected string VerifyCrewSpawnDistance(notnull OVT_VirtualizationManagerComponent virtualization, notnull OVT_VehicleSpawningDeploymentModule vehicleModule, string configName)
	{
		int authored = vehicleModule.m_iSpawnDistanceOverride;

		if (authored <= 0)
			return string.Format("'%1' registers its crews with spawn distance %2 - at 0 the engine's Manual policy means they never materialise at all, and anything below 0 is the global ring, which lets them go dormant and be walked away from their own vehicle",
				configName, authored.ToString());

		int global = virtualization.GetGlobalSpawnDistance();
		if (authored < global)
			return string.Format("'%1' registers its crews at %2 m, inside the global virtualization ring of %3 m - a vehicle crew must never be allowed to go dormant, because the movement tick walks a dormant crew along its route while its truck stays parked",
				configName, authored.ToString(), global.ToString());

		OVT_VehicleSpawningDeploymentModule clone = OVT_VehicleSpawningDeploymentModule.Cast(vehicleModule.CloneModule());
		if (!clone)
			return "CloneModule() on the vehicle module did not return a vehicle module";

		if (clone.m_iSpawnDistanceOverride != authored)
			return string.Format("The CLONE of '%1' registers crews at %2 while the template says %3 - CloneModule drops the attribute, so every real deployment ships at the class default instead of the authored value",
				configName, clone.m_iSpawnDistanceOverride.ToString(), authored.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The composition half: every faction that has a group registry at all can field this crew.
	//!
	//! Asked through the same door the core uses - GetOverthrowFactionByKey then GetGroupPrefabByName -
	//! rather than by reading the .conf, so a registry that parses but resolves to nothing is caught.
	//! \param[in] configName The config being checked, for the message.
	//! \param[in] crewName The crew group registry name it authors.
	//! \return An empty string when every faction resolves it, or the first that does not.
	protected string VerifyComposition(string configName, string crewName)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "OVT_Global.GetFactions() is null - no composition can be resolved at all";

		int checkedFactions = 0;

		int count = factions.GetFactionsCount();
		for (int i = 0; i < count; i++)
		{
			Faction faction = factions.GetFactionByIndex(i);
			if (!faction)
				continue;

			string key = faction.GetFactionKey();
			OVT_Faction overthrowFaction = factions.GetOverthrowFactionByKey(key);
			if (!overthrowFaction)
				continue;

			// Civilians and any other faction with no group registry cannot field a patrol at all and
			// are not what this case is about.
			array<string> names = overthrowFaction.GetAvailableGroupNames();
			if (!names || names.IsEmpty())
				continue;

			checkedFactions++;

			if (overthrowFaction.GetGroupPrefabByName(crewName) == ResourceName.Empty)
				return string.Format("Faction '%1' has no group registry entry named '%2', which '%3' authors as its crew - the core refuses that registration, so this faction's vehicle patrols would drive nowhere",
					key, crewName, configName);
		}

		if (checkedFactions < MIN_FACTIONS_WITH_REGISTRY)
			return string.Format("Only %1 faction(s) carry a group registry, expected at least %2 - the composition check for '%3' would pass vacuously",
				checkedFactions.ToString(), MIN_FACTIONS_WITH_REGISTRY.ToString(), configName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to search.
	//! \return Its first vehicle spawning module, or null.
	protected OVT_VehicleSpawningDeploymentModule FindVehicleModule(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = config.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_VehicleSpawningDeploymentModule vehicleModule = OVT_VehicleSpawningDeploymentModule.Cast(spawningModule);
			if (vehicleModule)
				return vehicleModule;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The two configs marked FREE AT GAME START really carry the flag - and the rest of the registry
//! does not.
//!
//! `m_bFreeAtGameStart` is an authored .conf attribute defaulting to FALSE, and everything about
//! losing it is silent: an unauthored line, a renamed attribute, a registry entry that stops
//! inheriting from its base .conf, a Workbench re-save that drops a default-looking value. The only
//! symptom would be most radio towers ungarrisoned because the seeding pass had nothing to seed.
//!
//! ⚠ The FALSE half matters as much as the true half. A defvalue typo ("1" instead of "0") would
//! make EVERY config free, seeding both vehicle patrols at every base for nothing on the first load
//! of every campaign. Requiring at least one shipped config to answer false proves the flag is
//! opt-in rather than universal.
//!
//! Nothing is registered, created or mutated - a pure read of the shipped registry.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_FreeAtGameStartIsAuthored : SCR_AutotestCaseBase
{
	//! The two shipped configs the 2026-08-17 amendment marks free.
	static const string TOWN_CONFIG = "Town Patrol";
	static const string TOWER_CONFIG = "Tower Garrison";

	//! The two the 2026-08-18 amendment (A1) adds: a base's opening garrison and its tower guards are
	//! the world's baseline state, not an opportunistic purchase - a player who drives past a base in
	//! the first minutes of a campaign must not find it empty because the pool had not filled yet.
	static const string BASE_GARRISON_CONFIG = "Base Garrison Patrol";
	static const string BASE_TOWER_GUARDS_CONFIG = "Base Tower Guards";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry || !manager.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			SetFailure("The deployment manager has no registry, so no shipped deployment config can be resolved at all");
			return true;
		}

		string failure = VerifyMarkedFree(manager, TOWN_CONFIG);
		if (failure == "")
			failure = VerifyMarkedFree(manager, TOWER_CONFIG);

		if (failure == "")
			failure = VerifyMarkedFree(manager, BASE_GARRISON_CONFIG);

		if (failure == "")
			failure = VerifyMarkedFree(manager, BASE_TOWER_GUARDS_CONFIG);

		if (failure == "")
			failure = VerifyFlagIsOptIn(manager);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("'%1' and '%2' are marked free at game start", TOWN_CONFIG, TOWER_CONFIG);
		PrintFormat("So are '%1' and '%2'; %3 other shipped config(s) are not",
			BASE_GARRISON_CONFIG, BASE_TOWER_GUARDS_CONFIG, CountNotFree(manager).ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One named shipped config resolves and carries the flag.
	//! \param[in] manager The deployment manager.
	//! \param[in] configName The config to check.
	//! \return An empty string when the claim holds, or the broken one.
	protected string VerifyMarkedFree(notnull OVT_DeploymentManagerComponent manager, string configName)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(configName);
		if (!config)
			return string.Format("The deployment registry does not resolve '%1', so it can neither be seeded nor bought", configName);

		if (!config.IsValidConfig())
			return string.Format("Config '%1' resolves but is not valid (no name, no modules, or no spawning module) - the seeding pass skips it in silence", configName);

		if (!config.m_bFreeAtGameStart)
			return string.Format("Config '%1' is NOT marked m_bFreeAtGameStart - it is back to being bought out of the faction resource pool, which on Easy (150 per tick) is how most radio towers ended up ungarrisoned", configName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! At least one shipped config answers false, which is what proves the default is false and the
	//! flag is opt-in.
	//! \param[in] manager The deployment manager.
	//! \return An empty string when the claim holds, or the broken one.
	protected string VerifyFlagIsOptIn(notnull OVT_DeploymentManagerComponent manager)
	{
		if (CountNotFree(manager) > 0)
			return "";

		return string.Format("EVERY one of the registry's %1 config(s) is marked free at game start - the attribute's default has flipped to true, so both vehicle patrols would be seeded at every base on the map for nothing on the first load of every campaign",
			manager.m_DeploymentRegistry.m_aDeploymentConfigs.Count().ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] manager The deployment manager.
	//! \return How many registry configs are NOT marked free at game start.
	protected int CountNotFree(notnull OVT_DeploymentManagerComponent manager)
	{
		int notFree = 0;
		foreach (OVT_DeploymentConfig config : manager.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (config && !config.m_bFreeAtGameStart)
				notFree++;
		}

		return notFree;
	}
}

//------------------------------------------------------------------------------------------------
//! A seeding pass puts a marked deployment on the ground, charges NOTHING for it, and a second pass
//! adds nothing.
//!
//!   - THE PASS IS FREE. SeedFreeDeployments() calls CreateDeployment with resourcesInvested 0 and
//!     never touches m_mFactionResources. A copy-pasted `availableResources -= deploymentCost` or a
//!     non-zero invested argument would quietly drain the occupying faction's opening pool on load,
//!     or refund money nobody spent when a player collected the garrison. Neither logs anything.
//!   - THE PASS IS IDEMPOTENT. It fires from PostGameStart(), which runs on a CONTINUED campaign too.
//!     If the 250 m same-name dedup stopped holding, every load would stack another garrison on the
//!     last one forever - a tower that got harder to clear every time you reloaded.
//!
//! Driven directly, because this tier never runs PostGameStart() and the CallLater never fires here.
//! The seeding method is public precisely so its contract is assertable without a campaign.
//!
//! ⚠ FIXTURE SAFETY - this case creates REAL deployments, which is unusual for this suite. A live
//! deployment starts a repeating 8-12 s update whose first tick registers real groups at the GLOBAL
//! ring, inside which the autotest camera is an observer. Safe on BOTH accepted grounds deliberately:
//!   (a) every deployment is SetSpawnedUnitsEliminated(true) on the deployment AND on every spawning
//!       module before anything else happens, so ConvergeGroups() refuses at both gates;
//!   (b) SeedFreeDeployments() is synchronous, so creation, assertions and teardown all happen in ONE
//!       Execute() frame and no UpdateDeployment tick can run in between.
//! Teardown runs on every path including the red ones.
//!
//! ⚠ Two pieces of shared world state are borrowed and handed back exactly as found: the first radio
//! tower's controlling faction (the garrison's control condition rightly refuses a resistance-held
//! tower) and the occupying faction's resource pool (planted so "nothing was charged" is a claim
//! about a real budget rather than a pool that was 0 either way).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Deployments_FreeSeedingIsFreeAndIdempotent : SCR_AutotestCaseBase
{
	//! The marked config this case looks for by name. Chosen over "Town Patrol" because it is the one
	//! with a REAL cost (20 base + 10 per group): a pass that quietly charged for it would be visible
	//! in the pool, where a 0-cost config would hide the bug.
	static const string TOWER_CONFIG = "Tower Garrison";

	//! Planted in the occupying faction's pool before the pass and taken back out afterwards, so the
	//! "nothing was charged" claim is made against a budget that could have been spent.
	static const int PLANTED_POOL = 5000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so the seeding pass has nothing to read");
			return true;
		}

		OVT_DeploymentConfig towerConfig = manager.m_DeploymentRegistry.FindConfigByName(TOWER_CONFIG);
		if (!towerConfig || !towerConfig.m_bFreeAtGameStart)
		{
			SetFailure("'%1' either does not resolve or is not marked m_bFreeAtGameStart, so a seeding pass could not produce one - see OVT_TEST_Init_Deployments_FreeAtGameStartIsAuthored", TOWER_CONFIG);
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();
		if (occupyingFaction < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so nothing can be seeded for it");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_RadioTowers || occupying.m_RadioTowers.IsEmpty())
		{
			SetFailure("This world has no radio towers, so there is nowhere for a tower garrison to be seeded - InitializeBases() found none");
			return true;
		}

		// BORROWED STATE 1: the tower's controlling faction. A garrison's control condition refuses a
		// tower the occupying faction does not hold, and this tier never ran NewGameStart() to hand the
		// towers over.
		OVT_RadioTowerData tower = occupying.m_RadioTowers[0];
		int originalTowerFaction = tower.faction;
		tower.faction = occupyingFaction;

		// BORROWED STATE 2: the resource pool.
		int originalPool = manager.GetFactionResources(occupyingFaction);
		manager.AddFactionResources(occupyingFaction, PLANTED_POOL);
		int expectedPool = originalPool + PLANTED_POOL;

		array<OVT_DeploymentComponent> created = new array<OVT_DeploymentComponent>();
		string failure = RunSeeding(manager, occupyingFaction, expectedPool, created);

		int seededCount = created.Count();

		// TEARDOWN BEFORE REPORTING, ON EVERY PATH. See the header: inert first, then deleted, then the
		// two pieces of borrowed world state handed back.
		Teardown(manager, created);
		tower.faction = originalTowerFaction;
		RestorePool(manager, occupyingFaction, originalPool);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A seeding pass created %1 deployment(s) including a '%2', every one of them with 0 invested resources, without moving a pool of %3; a second pass created nothing",
			seededCount.ToString(), TOWER_CONFIG, expectedPool.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Two seeding passes over the live manager, asserting what each one is allowed to do.
	//! \param[in] manager The deployment manager.
	//! \param[in] factionIndex The occupying faction.
	//! \param[in] expectedPool What the faction's pool must read both before and after.
	//! \param[in] created Every deployment either pass created, for teardown - filled even on a failure
	//!            path, and filled BEFORE the assertion that could return.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string RunSeeding(notnull OVT_DeploymentManagerComponent manager, int factionIndex, int expectedPool,
		notnull array<OVT_DeploymentComponent> created)
	{
		array<OVT_DeploymentComponent> beforeFirst = manager.GetAllDeployments();

		manager.SeedFreeDeployments();
		CollectNew(manager, beforeFirst, created);

		if (created.IsEmpty())
			return "SeedFreeDeployments() created nothing at all - with a marked config, an occupying-held radio tower and a pool of resources it did not need, the pass is inert";

		// EVERY seeded deployment is free, not just the one this case went looking for.
		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (!deployment)
				continue;

			if (deployment.GetResourcesInvested() != 0)
				return string.Format("Seeded deployment '%1' reports %2 invested resources, expected 0 - a deployment nobody paid for would refund that on collection and would read as money spent in the GM panel",
					deployment.GetDeploymentName(), deployment.GetResourcesInvested().ToString());
		}

		if (!ContainsConfig(created, TOWER_CONFIG))
			return string.Format("The pass created %1 deployment(s) but not one of them is a '%2' - the config the amendment exists for was not seeded at an occupying-held tower",
				created.Count().ToString(), TOWER_CONFIG);

		int poolAfterFirst = manager.GetFactionResources(factionIndex);
		if (poolAfterFirst != expectedPool)
			return string.Format("The occupying faction's pool moved from %1 to %2 across the seeding pass - seeding is charging for what it creates, which is the whole thing it exists not to do",
				expectedPool.ToString(), poolAfterFirst.ToString());

		// PASS 2: the dedup. This is the state every load of a saved campaign arrives in.
		int firstPassCount = created.Count();
		array<OVT_DeploymentComponent> beforeSecond = manager.GetAllDeployments();

		manager.SeedFreeDeployments();

		array<OVT_DeploymentComponent> second = new array<OVT_DeploymentComponent>();
		CollectNew(manager, beforeSecond, second);

		// Anything the second pass DID create still has to be torn down, so it joins the list before
		// the assertion that returns on it.
		foreach (OVT_DeploymentComponent duplicate : second)
		{
			created.Insert(duplicate);
		}

		if (!second.IsEmpty())
			return string.Format("A second seeding pass created %1 more deployment(s) on top of the first pass's %2 - the same-name dedup is not holding, so every load of a saved campaign would stack another garrison on the last one",
				second.Count().ToString(), firstPassCount.ToString());

		int poolAfterSecond = manager.GetFactionResources(factionIndex);
		if (poolAfterSecond != expectedPool)
			return string.Format("The occupying faction's pool moved to %1 across the SECOND seeding pass, expected %2", poolAfterSecond.ToString(), expectedPool.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every live deployment that was not in the baseline.
	//! \param[in] manager The deployment manager.
	//! \param[in] baseline The deployments that existed before the pass.
	//! \param[in] into Where the new ones are appended.
	protected void CollectNew(notnull OVT_DeploymentManagerComponent manager, notnull array<OVT_DeploymentComponent> baseline,
		notnull array<OVT_DeploymentComponent> into)
	{
		array<OVT_DeploymentComponent> current = manager.GetAllDeployments();
		foreach (OVT_DeploymentComponent deployment : current)
		{
			if (deployment && !baseline.Contains(deployment))
				into.Insert(deployment);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployments to search.
	//! \param[in] configName The config name to look for.
	//! \return True when at least one of them runs that config.
	protected bool ContainsConfig(notnull array<OVT_DeploymentComponent> deployments, string configName)
	{
		foreach (OVT_DeploymentComponent deployment : deployments)
		{
			if (deployment && deployment.GetDeploymentName() == configName)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Makes every fixture deployment unable to register anything, then deletes it.
	//!
	//! INERT FIRST, DELETED SECOND, and both in the frame that created them. See the case header for
	//! why either alone would already be enough and why this does both anyway.
	//! \param[in] manager The deployment manager.
	//! \param[in] created Every deployment the passes created.
	protected void Teardown(notnull OVT_DeploymentManagerComponent manager, notnull array<OVT_DeploymentComponent> created)
	{
		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (!deployment)
				continue;

			deployment.SetSpawnedUnitsEliminated(true);

			array<OVT_BaseSpawningDeploymentModule> modules = deployment.GetSpawningModules();
			foreach (OVT_BaseSpawningDeploymentModule module : modules)
			{
				if (module)
					module.SetSpawnedUnitsEliminated(true);
			}
		}

		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (deployment)
				manager.DeleteDeployment(deployment);
		}

		created.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the faction's resource pool back exactly where it was found, whichever way it moved.
	//! \param[in] manager The deployment manager.
	//! \param[in] factionIndex The faction whose pool was borrowed.
	//! \param[in] originalPool The value to restore.
	protected void RestorePool(notnull OVT_DeploymentManagerComponent manager, int factionIndex, int originalPool)
	{
		int current = manager.GetFactionResources(factionIndex);

		if (current > originalPool)
			manager.SubtractFactionResources(factionIndex, current - originalPool);
		else if (current < originalPool)
			manager.AddFactionResources(factionIndex, originalPool - current);
	}
}

//------------------------------------------------------------------------------------------------
//! A position at a base classifies with the BASE bit, so a BASE-only config can be bought there at
//! all.
//!
//! Every base-defense config the migration ships authors m_iAllowedLocationTypes BASE. If the BASE
//! branch stopped being reachable - a reordered precedence, a changed radius, a town test that
//! swallowed it - nine configs would silently become unbuyable everywhere, the way the Tower
//! Garrison config was before the RADIO_TOWER bit was OR-ed in.
//!
//! The claim is made against a SHIPPED CONSUMER: the "Light Vehicle Patrol" config is asked whether
//! it would accept the classification, because CanUseLocationType() is the only question the
//! evaluator asks and a bit nobody consumes proves nothing.
//!
//! 🔴 TOWNS SHADOW BASES, and this case MEASURES it rather than asserting it away. The chain tests
//! towns first with a hardcoded 500 m radius, so a base within 500 m of a town centre classifies as
//! TOWN and can never be offered a BASE-only config. True of this test world's only base (114 m from
//! its town) and of 4 of Eden's 10 bases - an open design question in the feature's context.md.
//! Until it is decided the honest claim is that the branch is alive somewhere in a base's ring; the
//! shadow count is printed on every run so the number cannot be quietly forgotten.
//!
//! Nothing is registered, created or mutated. The classification does no ground trace, so probes off
//! the terrain are legal.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_BaseLocationTypeIsReachable : SCR_AutotestCaseBase
{
	//! A shipped config that authors BASE and nothing else, used as the consumer of the classification.
	static const string BASE_ONLY_CONFIG = "Light Vehicle Patrol";

	//! Probe distances from a base centre, all inside the 500 m ring the classification uses.
	static const float PROBE_NEAR = 150;
	static const float PROBE_MID = 300;
	static const float PROBE_FAR = 450;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases || occupying.m_Bases.IsEmpty())
		{
			SetFailure("This world has no bases, so the BASE classification cannot be exercised - InitializeBases() found none");
			return true;
		}

		vector probe;
		if (!FindBaseClassifiedPosition(manager, occupying, probe))
		{
			SetFailure("No position within 500 m of any of this world's %1 base(s) classifies as BASE - every BASE-only deployment config is unbuyable everywhere on this map, silently",
				occupying.m_Bases.Count().ToString());
			return true;
		}

		OVT_LocationTypeFlag classification = manager.GetLocationTypeAtPosition(probe);

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so the classification has no shipped consumer to be checked against");
			return true;
		}

		OVT_DeploymentConfig baseConfig = manager.m_DeploymentRegistry.FindConfigByName(BASE_ONLY_CONFIG);
		if (!baseConfig)
		{
			SetFailure("The deployment registry does not resolve '%1', so the classification has no shipped consumer to be checked against", BASE_ONLY_CONFIG);
			return true;
		}

		if (!baseConfig.CanUseLocationType(classification))
		{
			SetFailure("'%1' refuses a position classified %2 - the config's authored location types and the classification no longer agree, so it can never be created at a base",
				BASE_ONLY_CONFIG, classification.ToString());
			return true;
		}

		int shadowed = CountTownShadowedBases(manager, occupying);
		PrintFormat("A position at a base classifies as %1 and '%2' accepts it; %3 of this world's base(s) are shadowed by a town at their own centre and can never be offered a BASE-only config",
			classification.ToString(), BASE_ONLY_CONFIG, shadowed.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The first position inside some base's 500 m ring that classifies with the BASE bit.
	//!
	//! Tries each base's own centre first - which is the only position GetBasePositions() ever offers
	//! the evaluator - and falls back to eight compass probes inside the ring, because a base centre
	//! that sits inside a town's bounds classifies as TOWN.
	//! \param[in] manager The deployment manager, for the classification.
	//! \param[in] occupying The occupying faction manager, for the base list.
	//! \param[out] found The position that classified as BASE.
	//! \return True when one was found.
	protected bool FindBaseClassifiedPosition(notnull OVT_DeploymentManagerComponent manager,
		notnull OVT_OccupyingFactionManager occupying, out vector found)
	{
		foreach (OVT_BaseData baseData : occupying.m_Bases)
		{
			if (!baseData)
				continue;

			if (ClassifiesAsBase(manager, baseData.location))
			{
				found = baseData.location;
				return true;
			}
		}

		array<float> distances = {PROBE_FAR, PROBE_MID, PROBE_NEAR};

		foreach (OVT_BaseData baseData : occupying.m_Bases)
		{
			if (!baseData)
				continue;

			foreach (float distance : distances)
			{
				for (int step = 0; step < 8; step++)
				{
					float angle = (step / 8.0) * Math.PI2;
					vector probe = baseData.location + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);

					if (ClassifiesAsBase(manager, probe))
					{
						found = probe;
						return true;
					}
				}
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] manager The deployment manager.
	//! \param[in] position The position to classify.
	//! \return True when the classification carries the BASE bit.
	protected bool ClassifiesAsBase(notnull OVT_DeploymentManagerComponent manager, vector position)
	{
		return (manager.GetLocationTypeAtPosition(position) & OVT_LocationTypeFlag.BASE) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! How many bases do NOT classify as BASE at their own centre - i.e. how many of this world's
	//! bases the evaluator can never offer a BASE-only config to.
	//! \param[in] manager The deployment manager.
	//! \param[in] occupying The occupying faction manager.
	//! \return The count, 0 when every base classifies as its own kind.
	protected int CountTownShadowedBases(notnull OVT_DeploymentManagerComponent manager,
		notnull OVT_OccupyingFactionManager occupying)
	{
		int shadowed = 0;

		foreach (OVT_BaseData baseData : occupying.m_Bases)
		{
			if (baseData && !ClassifiesAsBase(manager, baseData.location))
				shadowed++;
		}

		return shadowed;
	}
}

//------------------------------------------------------------------------------------------------
//! The no-players-nearby condition refuses to CREATE a deployment on top of a player, and never
//! fails at runtime for the same reason.
//!
//! ⚠ The asymmetry is the entire point of the module and this is the only mechanical guard on it.
//! EvaluateStaticCondition() gates creation, so a base does not fortify around a player standing in
//! it. EvaluateCondition() gates the runtime, and every base-defense config authors
//! m_bDeleteOnConditionFail - so if that method ever started answering the distance question too,
//! walking into a base would DELETE its whole defense and refund the occupying faction for it. The
//! fight would evaporate on approach and read as a spawning bug, not a condition bug.
//!
//! Driven against a REAL player body, because "a player is standing here" is the only input the
//! module has. The local character can take a few frames to exist, so the case polls for it.
//!
//! ⚠ The distance is set BY HAND: [Attribute] defvalues are applied by the config loader, not by
//! `new`, so a hand-constructed module carries 0 and would accept every position. The shipped
//! default (320) is asserted by the configs that author it.
//!
//! Nothing is registered or created - the module is a bare object, asked two questions.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Deployments_NoPlayersNearbyGatesCreationOnly : SCR_AutotestCaseBase
{
	//! The shipped default, set by hand because attribute defvalues do not apply to `new`.
	static const float MIN_PLAYER_DISTANCE = 320;

	//! Well beyond MIN_PLAYER_DISTANCE from any player in either world this suite runs in.
	static const float FAR_AWAY = 5000;

	//! A value no default could be mistaken for, for the clone check.
	static const float CLONE_PROBE_DISTANCE = 777;

	//! Frame polls allowed for the local player to have a character.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);

		IEntity body;
		if (!players.IsEmpty())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(players[0]);

		if (!body)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("No player-controlled character appeared in %1 polls, so 'a player is standing here' could not be put to the condition at all", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();

		OVT_NoPlayersNearbyConditionDeploymentModule module = new OVT_NoPlayersNearbyConditionDeploymentModule();
		module.m_fMinPlayerDistance = MIN_PLAYER_DISTANCE;

		vector playerPos = body.GetOrigin();

		// CREATION, ON TOP OF A PLAYER: refused.
		if (module.EvaluateStaticCondition(playerPos, occupyingFaction, 0))
		{
			SetFailure("The condition ACCEPTED a candidate position with a player standing on it (minimum distance %1 m) - a base would fortify itself in front of a player, and its groups would materialise in his view because he is an observer",
				MIN_PLAYER_DISTANCE.ToString());
			return true;
		}

		// CREATION, FAR AWAY: accepted.
		vector farPos = playerPos + Vector(FAR_AWAY, 0, FAR_AWAY);
		if (!module.EvaluateStaticCondition(farPos, occupyingFaction, 0))
		{
			SetFailure("The condition REFUSED a candidate position %1 m from the nearest player - no deployment carrying this module could ever be created anywhere", FAR_AWAY.ToString());
			return true;
		}

		// RUNTIME, WITH A PLAYER RIGHT THERE: still true. This is the asymmetry.
		if (!module.EvaluateCondition())
		{
			SetFailure("EvaluateCondition() answered FALSE while a player was nearby - with m_bDeleteOnConditionFail authored, walking into a base would delete its entire defense and refund the occupying faction for it");
			return true;
		}

		// CLONE FIDELITY: a dropped distance clones as 0, which accepts every position and turns the
		// module into a no-op nobody would notice.
		module.m_fMinPlayerDistance = CLONE_PROBE_DISTANCE;
		OVT_NoPlayersNearbyConditionDeploymentModule clone = OVT_NoPlayersNearbyConditionDeploymentModule.Cast(module.CloneModule());
		if (!clone)
		{
			SetFailure("CloneModule() did not answer an OVT_NoPlayersNearbyConditionDeploymentModule at all");
			return true;
		}

		if (clone.m_fMinPlayerDistance != CLONE_PROBE_DISTANCE)
		{
			SetFailure("A clone carries a minimum player distance of %1, expected the authored %2 - every deployment gets a CLONE of its config's modules, so the authored value would never reach the game",
				clone.m_fMinPlayerDistance.ToString(), CLONE_PROBE_DISTANCE.ToString());
			return true;
		}

		PrintFormat("The no-players condition refuses creation on top of a player and accepts it %1 m away, answers true at runtime regardless, and clones its distance", FAR_AWAY.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE PHASE'S HEADLINE CLAIM: a position that already holds config A is answered with config B, not
//! with nothing.
//!
//! FindBestDeploymentConfig() used to answer the single lowest-priority config a place qualified
//! for, and its caller skipped the position when that one was already there - so a base could hold
//! exactly ONE deployment forever, which is why nine per-concern base-defense configs could not
//! exist before this phase. Two edits changed it: the blanket 100 m proximity veto came out of
//! IsPositionSuitableForDeployment(), and the name-scoped 250 m dedup moved INTO the per-config
//! filter, ahead of the priority comparison.
//!
//! The ladder is walked with REAL deployments, because the claim is about
//! HasExistingDeploymentOfType() reading the live deployment list - the half the Logic tier's pure
//! selection arithmetic cannot see.
//!
//! Two FIXTURE configs are appended to the live registry and removed again, deliberately instead of
//! the shipped ones: those carry chance rolls, instance caps, condition modules and location
//! restrictions, so a case asserting the ladder through them would be asserting their authoring. The
//! fixtures author NO location types, which reads as "no restrictions".
//!
//! ⚠ FIXTURE SAFETY - this case creates REAL deployments. Safe on both accepted grounds: every
//! deployment is SetSpawnedUnitsEliminated(true) on the deployment AND every spawning module before
//! anything else, and everything is synchronous inside one Execute() frame. Teardown runs on every
//! path and deletes the deployments BEFORE taking the fixture configs out of the registry.
//!
//! ⚠ The probe position is chosen away from every town, base and tower so no shipped config can be
//! suitable there and be picked instead of a fixture one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Deployments_EscalationBuysTheNextConfig : SCR_AutotestCaseBase
{
	//! Fixture config names. Prefixed so they can never collide with a shipped config's name, which
	//! is what the 250 m dedup keys on.
	static const string CONFIG_A = "OVT_TEST Escalation A";
	static const string CONFIG_B = "OVT_TEST Escalation B";

	//! How far the probe must be from every town, base and tower, so no shipped config classifies as
	//! usable there.
	static const float CLEARANCE = 2000;

	//! A budget every fixture config (cost 0) fits inside.
	static const int BUDGET = 1000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager || !manager.m_DeploymentRegistry || !manager.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			SetFailure("The deployment manager or its registry is null, so no config can be offered at all");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();
		if (occupyingFaction < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so nothing can be offered to it");
			return true;
		}

		vector probe = FindClearPosition();

		array<ref OVT_DeploymentConfig> registryConfigs = manager.m_DeploymentRegistry.m_aDeploymentConfigs;
		int originalConfigCount = registryConfigs.Count();

		registryConfigs.Insert(BuildFixtureConfig(CONFIG_A, 1));
		registryConfigs.Insert(BuildFixtureConfig(CONFIG_B, 2));

		array<OVT_DeploymentComponent> created = new array<OVT_DeploymentComponent>();
		string failure = WalkTheLadder(manager, occupyingFaction, probe, created);

		// TEARDOWN BEFORE REPORTING, ON EVERY PATH: deployments first, then the fixture configs.
		Teardown(manager, created);

		while (registryConfigs.Count() > originalConfigCount)
		{
			registryConfigs.Remove(registryConfigs.Count() - 1);
		}

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A position holding '%1' was answered '%2' rather than nothing, and answered nothing once it held both", CONFIG_A, CONFIG_B);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks for the next config three times, creating what it is given, and checks each answer.
	//! \param[in] manager The deployment manager.
	//! \param[in] factionIndex The occupying faction.
	//! \param[in] probe The position being fortified.
	//! \param[in] created Every deployment created, for teardown - filled BEFORE any assertion that
	//!            could return.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string WalkTheLadder(notnull OVT_DeploymentManagerComponent manager, int factionIndex, vector probe,
		notnull array<OVT_DeploymentComponent> created)
	{
		// RUNG 1: nothing here yet, so the cheapest priority wins.
		OVT_DeploymentConfig first = manager.FindBestDeploymentConfig(probe, factionIndex, 0, BUDGET);
		if (!first)
			return string.Format("An empty position was offered no config at all, expected '%1' - the fixture configs are not reaching the filter (faction type, cost or conditions)", CONFIG_A);

		if (first.m_sDeploymentName != CONFIG_A)
			return string.Format("An empty position was offered '%1', expected '%2' - either priority is not deciding, or a shipped config is suitable at a probe chosen to be clear of every town, base and tower",
				first.m_sDeploymentName, CONFIG_A);

		OVT_DeploymentComponent firstDeployment = manager.CreateDeployment(first, probe, factionIndex, 0, 0);
		if (!firstDeployment)
			return string.Format("Creating a '%1' at the probe answered nothing, so the ladder cannot be walked", CONFIG_A);

		created.Insert(firstDeployment);
		MakeInert(firstDeployment);

		// RUNG 2: THE HEADLINE. A already stands here, so the answer is B and not nothing.
		OVT_DeploymentConfig second = manager.FindBestDeploymentConfig(probe, factionIndex, 0, BUDGET);
		if (!second)
			return string.Format("A position already holding '%1' was offered NOTHING - this is the pre-migration behaviour: a place can hold exactly one config and is then written off forever, so a base could never fortify concern by concern",
				CONFIG_A);

		if (second.m_sDeploymentName == CONFIG_A)
			return string.Format("A position already holding '%1' was offered it AGAIN - the name-scoped dedup is not being applied before the priority comparison, and the pool would be spent on creations the caller's guard then refuses",
				CONFIG_A);

		if (second.m_sDeploymentName != CONFIG_B)
			return string.Format("A position already holding '%1' was offered '%2', expected '%3'", CONFIG_A, second.m_sDeploymentName, CONFIG_B);

		OVT_DeploymentComponent secondDeployment = manager.CreateDeployment(second, probe, factionIndex, 0, 0);
		if (!secondDeployment)
			return string.Format("Creating a '%1' at the probe answered nothing", CONFIG_B);

		created.Insert(secondDeployment);
		MakeInert(secondDeployment);

		// RUNG 3: both stand here, so there is nothing left to buy AT THIS POSITION.
		OVT_DeploymentConfig third = manager.FindBestDeploymentConfig(probe, factionIndex, 0, BUDGET);
		if (third)
			return string.Format("A position holding both fixture configs was offered '%1' - a fully fortified place must be answered nothing, or it is re-evaluated into creation attempts forever",
				third.m_sDeploymentName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A config that qualifies everywhere, costs nothing, rolls no chance and carries no conditions -
	//! so that what this case asserts is the evaluator's ordering and nothing else.
	//! \param[in] name The config's name; the dedup keys on it.
	//! \param[in] priority Lower wins.
	//! \return The config.
	protected OVT_DeploymentConfig BuildFixtureConfig(string name, int priority)
	{
		// ⚠ [Attribute] defvalues are applied by the config loader, never by `new`, so EVERY field a
		// filter reads has to be set here. m_fChance in particular would be 0 and the config would
		// essentially never be offered.
		OVT_DeploymentConfig fixture = new OVT_DeploymentConfig();
		fixture.m_sDeploymentName = name;
		fixture.m_iPriority = priority;
		fixture.m_iBaseCost = 0;
		fixture.m_iMinimumThreatLevel = 0;
		fixture.m_fChance = 100;
		fixture.m_iMaxInstances = -1;
		fixture.m_bFreeAtGameStart = false;
		fixture.m_iAllowedFactionTypes = OVT_FactionTypeFlag.OCCUPYING_FACTION;
		fixture.m_iAllowedLocationTypes = 0; // "no restrictions" - the probe's classification is irrelevant

		// IsValidConfig() demands at least one spawning module. This one is left entirely unauthored,
		// so it wants 0 groups and costs 0, and the deployment is made inert before it could tick.
		OVT_InfantrySpawningDeploymentModule spawning = new OVT_InfantrySpawningDeploymentModule();
		spawning.m_sModuleName = "OVT_TEST inert";
		spawning.m_iMinGroupCount = 0;
		spawning.m_iMaxGroupCount = 0;
		spawning.m_iCostPerGroup = 0;
		fixture.m_aModules.Insert(spawning);

		return fixture;
	}

	//------------------------------------------------------------------------------------------------
	//! A position at least CLEARANCE from every town, base and radio tower, so no shipped config's
	//! location rule can match there.
	//! \return The probe position.
	protected vector FindClearPosition()
	{
		vector origin = "0 0 0";

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_Bases && !occupying.m_Bases.IsEmpty())
			origin = occupying.m_Bases[0].location;

		// Both worlds this suite runs in fit inside a few kilometres, so stepping out along one
		// diagonal clears everything within a couple of steps.
		vector candidate = origin;
		for (int step = 1; step <= 12; step++)
		{
			candidate = origin + Vector(CLEARANCE * step, 0, CLEARANCE * step);

			if (IsClearOfEverything(candidate))
				return candidate;
		}

		return candidate;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] position The position to test.
	//! \return True when no town, base or radio tower is within CLEARANCE.
	protected bool IsClearOfEverything(vector position)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns)
		{
			foreach (OVT_TownData town : towns.m_Towns)
			{
				if (town && vector.Distance(position, town.location) < CLEARANCE)
					return false;
			}
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return true;

		if (occupying.m_Bases)
		{
			foreach (OVT_BaseData baseData : occupying.m_Bases)
			{
				if (baseData && vector.Distance(position, baseData.location) < CLEARANCE)
					return false;
			}
		}

		if (occupying.m_RadioTowers)
		{
			foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
			{
				if (tower && vector.Distance(position, tower.location) < CLEARANCE)
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Makes one fixture deployment unable to register anything, immediately after it is created.
	//! \param[in] deployment The deployment to disarm.
	protected void MakeInert(notnull OVT_DeploymentComponent deployment)
	{
		deployment.SetSpawnedUnitsEliminated(true);

		array<OVT_BaseSpawningDeploymentModule> modules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : modules)
		{
			if (module)
				module.SetSpawnedUnitsEliminated(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every fixture deployment. Called on every path, including the red ones.
	//! \param[in] manager The deployment manager.
	//! \param[in] created Every deployment this case created.
	protected void Teardown(notnull OVT_DeploymentManagerComponent manager, notnull array<OVT_DeploymentComponent> created)
	{
		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (deployment)
				MakeInert(deployment);
		}

		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (deployment)
				manager.DeleteDeployment(deployment);
		}

		created.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! Every faction registry name the base-defense migration introduces resolves to a REAL prefab, on
//! BOTH shipped occupying factions.
//!
//! Nine deployment configs are authored against these names and every failure mode of a wrong one is
//! silent at authoring time: RegisterGroup logs a warning and returns -1 for an unknown group name,
//! the composition module logs a warning and builds nothing, the parked-vehicle module parks
//! nothing. A campaign with a typo'd registry name is one whose bases simply never fortify.
//!
//! Asked through the same door the game uses - GetOverthrowFactionByKey then GetGroupPrefabByName /
//! GetVehiclePrefabByName / GetCompositionConfig - rather than by reading the .conf, so a registry
//! that parses but resolves to nothing is caught too.
//!
//! BOTH factions, or it means nothing: Overthrow swaps which faction occupies, so a name authored on
//! USSR alone fortifies under one occupier and not the other. The case counts the factions it
//! actually checked and fails loudly rather than passing vacuously.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_BaseDefenseRegistryEntriesResolve : SCR_AutotestCaseBase
{
	//! The group registry names Phase 2 appended.
	static const ref array<string> GROUP_NAMES = {"heavy_infantry", "at_team", "sniper", "sniper_team", "bunker_team"};

	//! The vehicle registry names Phase 2 appended.
	static const ref array<string> VEHICLE_NAMES = {"car", "truck"};

	//! The composition tags Phase 2 appended.
	static const ref array<string> COMPOSITION_TAGS = {"MediumCheckpoint", "LargeCheckpoint"};

	//! Both shipped occupying factions carry a group registry. Fewer would make every check below
	//! pass vacuously.
	static const int MIN_FACTIONS_WITH_REGISTRY = 2;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
		{
			SetFailure("OVT_Global.GetFactions() is null - no registry name can be resolved at all");
			return true;
		}

		int checkedFactions = 0;
		string failure = "";

		int count = factions.GetFactionsCount();
		for (int i = 0; i < count; i++)
		{
			Faction faction = factions.GetFactionByIndex(i);
			if (!faction)
				continue;

			string key = faction.GetFactionKey();
			OVT_Faction overthrowFaction = factions.GetOverthrowFactionByKey(key);
			if (!overthrowFaction)
				continue;

			// Civilians and anything else with no group registry cannot field base defense at all and
			// are not what this case is about.
			array<string> names = overthrowFaction.GetAvailableGroupNames();
			if (!names || names.IsEmpty())
				continue;

			checkedFactions++;

			if (failure == "")
				failure = VerifyFaction(overthrowFaction, key);
		}

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		if (checkedFactions < MIN_FACTIONS_WITH_REGISTRY)
		{
			SetFailure("Only %1 faction(s) carry a group registry, expected at least %2 - every base-defense registry check would pass vacuously",
				checkedFactions.ToString(), MIN_FACTIONS_WITH_REGISTRY.ToString());
			return true;
		}

		PrintFormat("All %1 base-defense group names, %2 vehicle names and %3 composition tags resolve on every faction with a registry",
			GROUP_NAMES.Count().ToString(), VEHICLE_NAMES.Count().ToString(), COMPOSITION_TAGS.Count().ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every new name, on one faction.
	//! \param[in] faction The Overthrow faction data.
	//! \param[in] key Its faction key, for the message.
	//! \return An empty string when every name resolves, or the first that does not.
	protected string VerifyFaction(notnull OVT_Faction faction, string key)
	{
		foreach (string groupName : GROUP_NAMES)
		{
			if (faction.GetGroupPrefabByName(groupName) == ResourceName.Empty)
				return string.Format("Faction '%1' has no GROUP registry entry named '%2' - core refuses that registration outright, so every base-defense config authoring it would register nothing and log only a WARNING",
					key, groupName);
		}

		foreach (string vehicleName : VEHICLE_NAMES)
		{
			if (faction.GetVehiclePrefabByName(vehicleName) == ResourceName.Empty)
				return string.Format("Faction '%1' has no VEHICLE registry entry named '%2' - the parked-vehicle module would park nothing and log only a WARNING",
					key, vehicleName);
		}

		foreach (string tag : COMPOSITION_TAGS)
		{
			OVT_FactionComposition composition = faction.GetCompositionConfig(tag);
			if (!composition)
				return string.Format("Faction '%1' has no composition tagged '%2' - the composition module would build nothing and log only a WARNING",
					key, tag);

			if (!composition.m_aPrefabs || composition.m_aPrefabs.IsEmpty())
				return string.Format("Faction '%1' composition '%2' resolves but authors no prefabs - it would be picked, refused and never retried",
					key, tag);

			if (composition.m_iCost <= 0)
				return string.Format("Faction '%1' composition '%2' costs %3 - a free structure makes the deployment's resource cost wrong and the pool would never be the constraint it is meant to be",
					key, tag, composition.m_iCost.ToString());
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Test-only window onto OVT_InfantrySpawningDeploymentModule's protected placement seam.
//!
//! ResolveSpawnPosition() is protected deliberately - it is a subclass seam, not an API - so a
//! subclass is the only honest way to assert it.
//!
//! ⚠ NOT [BaseContainerProps]. It must never appear in a Workbench config picker or be authorable
//! into a deployment config.
//------------------------------------------------------------------------------------------------
class OVT_TEST_SnapProbeInfantryModule : OVT_InfantrySpawningDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] anchor The batch anchor to roll around.
	//! \return Exactly what a registration would have used.
	vector ProbeSpawnPosition(vector anchor)
	{
		return ResolveSpawnPosition(anchor, 0);
	}
}

//------------------------------------------------------------------------------------------------
//! m_bSnapToRoad 0 keeps a registration inside its own spawn radius - the road-snap opt-out, which
//! is the whole reason the attribute exists.
//!
//! The shared roller picks a point 10..m_fSpawnRadius from the anchor and then calls
//! FindNearestRoad, which searches up to 500 m. So with the snap ON, m_fSpawnRadius bounds the ROLL
//! and not the RESULT - integration measured a tower garrison registering on the access road instead
//! of at its tower.
//!
//! The assertion is an INVARIANT over many samples, not a retry: every roll must satisfy both halves.
//!   - HORIZONTAL: the result is 10..m_fSpawnRadius from the anchor.
//!   - ALTITUDE: the result's Y is EXACTLY the anchor's, because the ring offset has a zero Y
//!     component. This is the half that catches a broken opt-out - FindNearestRoad answers a road
//!     waypoint at terrain height, which is not the anchor's altitude except by coincidence.
//!
//! ⚠ The snap-ON half is PRINTED, not asserted: what snapping does depends on where the roads are in
//! whichever world this runs in, so asserting a road network the test world does not guarantee would
//! be a flake. The line tells a reader whether this world can even distinguish the two paths.
//!
//! ⚠ [Attribute] defvalues do NOT apply to `new`. A hand-constructed module has m_fSpawnRadius 0 and
//! m_bSnapToRoad FALSE, so both are set by hand below.
//!
//! Nothing is registered and no deployment exists - the probe module is a bare `new` with no parent.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_SnapToRoadOptOutStaysInRadius : SCR_AutotestCaseBase
{
	//! The radius the probe module is given. Comfortably above the roller's hardcoded 10 m minimum.
	static const float PROBE_RADIUS = 60;

	//! The roller's own minimum distance from the anchor (Math.RandomFloat(10, m_fSpawnRadius)).
	static const float ROLL_MINIMUM = 10;

	//! How many rolls the invariant is checked over. Every one must hold.
	static const int SAMPLES = 12;

	//! Float slack. vector.Distance is only +1 ULP off at 1000/2000 m and these are tens of metres,
	//! so this is generous by a wide margin.
	static const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector anchor = OVT_TEST_VirtualizationFixture.PickPosition();

		OVT_TEST_SnapProbeInfantryModule probe = new OVT_TEST_SnapProbeInfantryModule();
		probe.m_fSpawnRadius = PROBE_RADIUS;
		probe.m_bSnapToRoad = false;

		for (int i = 0; i < SAMPLES; i++)
		{
			vector sample = probe.ProbeSpawnPosition(anchor);

			float altitudeDelta = Math.AbsFloat(sample[1] - anchor[1]);
			if (altitudeDelta > EPSILON)
			{
				SetFailure("With m_bSnapToRoad OFF a registration came back %1 m off the anchor's altitude - the ring offset has a zero Y component, so the only thing that can move it is the road snap this flag is supposed to have turned off",
					altitudeDelta.ToString());
				return true;
			}

			float horizontal = HorizontalDistance(sample, anchor);
			if (horizontal < ROLL_MINIMUM - EPSILON || horizontal > PROBE_RADIUS + EPSILON)
			{
				SetFailure("With m_bSnapToRoad OFF a registration came back %1 m from the anchor, outside the roll's own %2..%3 m range - a garrison authored to hold a place would be registered somewhere else and, with a DEFEND or null plan, would hold THERE",
					horizontal.ToString(), ROLL_MINIMUM.ToString(), PROBE_RADIUS.ToString());
				return true;
			}
		}

		ReportSnapOnBehaviour(anchor);

		PrintFormat("m_bSnapToRoad OFF kept all %1 registrations within %2 m of the anchor and at its exact altitude",
			SAMPLES.ToString(), PROBE_RADIUS.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs what the SHIPPED path (snap ON) does at this anchor, so a reader can tell whether this
	//! world distinguishes the two paths at all. Asserts nothing - see the class header.
	//! \param[in] anchor The anchor the OFF half was measured around.
	protected void ReportSnapOnBehaviour(vector anchor)
	{
		vector snappedAnchor = OVT_WorldUtils.FindNearestRoad(anchor);
		if (snappedAnchor == anchor)
		{
			Print("[OVT_TEST] Snap diagnostic: this world's road query does not move the probe anchor at all, so snap ON and snap OFF are indistinguishable here - the altitude fail proof would NOT redden in this world", LogLevel.NORMAL);
			return;
		}

		OVT_TEST_SnapProbeInfantryModule snapped = new OVT_TEST_SnapProbeInfantryModule();
		snapped.m_fSpawnRadius = PROBE_RADIUS;
		snapped.m_bSnapToRoad = true;

		int leftTheRing = 0;
		int changedAltitude = 0;
		float furthest = 0;

		for (int i = 0; i < SAMPLES; i++)
		{
			vector sample = snapped.ProbeSpawnPosition(anchor);

			if (Math.AbsFloat(sample[1] - anchor[1]) > EPSILON)
				changedAltitude++;

			float horizontal = HorizontalDistance(sample, anchor);
			if (horizontal > PROBE_RADIUS + EPSILON)
				leftTheRing++;

			if (horizontal > furthest)
				furthest = horizontal;
		}

		PrintFormat("[OVT_TEST] Snap diagnostic: with snap ON, %1 of the samples left the %2 m ring and the furthest landed %3 m from the anchor",
			leftTheRing.ToString(), PROBE_RADIUS.ToString(), furthest.ToString());
		PrintFormat("[OVT_TEST] Snap diagnostic: %1 samples changed altitude, so the altitude fail proof IS live in this world",
			changedAltitude.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] a First position.
	//! \param[in] b Second position.
	//! \return Their distance ignoring altitude.
	protected float HorizontalDistance(vector a, vector b)
	{
		return vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2]));
	}
}

//------------------------------------------------------------------------------------------------
//! Every shipped placement provider answers an EMPTY LIST, never null, where nothing qualifies.
//!
//! Point 1 of the provider contract. A provider is asked on every convergence pass of every placed
//! deployment, and "nothing here" is the ORDINARY answer. A provider answering null would have
//! OVT_PlacedInfantrySpawningDeploymentModule.CalculateGroupCount dereference it on the first tick of
//! the first tower-guard deployment. The module carries a defensive re-allocation for exactly that,
//! and this case is what stops the defence from being the only thing holding the contract up.
//!
//! ⚠ The probe position is chosen clear of every town, base and radio tower: the claim is about the
//! EMPTY answer, and a probe beside a real tower would assert nothing. The defend-position provider
//! is the one that would notice - it range-checks the nearest base against its own radius.
//!
//! Nothing is registered, created or mutated - providers are read-only sphere queries.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_PlacementProvidersAnswerEmptyNotNull : SCR_AutotestCaseBase
{
	//! How far the probe must be from every town, base and tower.
	static const float CLEARANCE = 2000;

	//! The radius the providers are asked over. m_fSearchRadius' shipped default (baseRange).
	static const float SEARCH_RADIUS = 280;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector probe = FindClearPosition();

		string failure = VerifyProvider(new OVT_DeploymentPlacementProvider(), "the base provider", probe);

		if (failure == "")
			failure = VerifyProvider(new OVT_TowerCoverPostPlacementProvider(), "the tower cover post provider", probe);

		if (failure == "")
			failure = VerifyProvider(new OVT_SniperMarkerPlacementProvider(), "the sniper marker provider", probe);

		if (failure == "")
			failure = VerifyProvider(new OVT_BaseDefendPositionPlacementProvider(), "the base defend position provider", probe);

		if (failure == "")
			failure = VerifyProvider(new OVT_RoadSlotOverwatchPlacementProvider(), "the road slot overwatch provider", probe);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("All four shipped placement providers, and the base contract, answered an empty non-null list at a position %1 m clear of every town, base and tower",
			CLEARANCE.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The whole contract, on one provider.
	//! \param[in] provider The provider to ask.
	//! \param[in] label Its name, for the message.
	//! \param[in] probe A position with nothing near it.
	//! \return An empty string when the contract holds, or the broken half.
	protected string VerifyProvider(notnull OVT_DeploymentPlacementProvider provider, string label, vector probe)
	{
		array<ref OVT_DeploymentPlacement> placements = provider.ResolvePlacements(probe, SEARCH_RADIUS, 0);

		if (!placements)
			return string.Format("%1 answered NULL where nothing qualifies - 'nothing here' is the ordinary answer for a placement provider and every caller would have to guard against it",
				label);

		if (!placements.IsEmpty())
			return string.Format("%1 answered %2 post(s) at a position deliberately chosen clear of every town, base and tower - either the probe is not clear after all, or the provider is not bounded by the radius it is given",
				label, placements.Count().ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A position at least CLEARANCE from every town, base and radio tower.
	//! \return The probe position.
	protected vector FindClearPosition()
	{
		vector origin = "0 0 0";

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_Bases && !occupying.m_Bases.IsEmpty())
			origin = occupying.m_Bases[0].location;

		vector candidate = origin;
		for (int step = 1; step <= 12; step++)
		{
			candidate = origin + Vector(CLEARANCE * step, 0, CLEARANCE * step);

			if (IsClearOfEverything(candidate))
				return candidate;
		}

		return candidate;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] position The position to test.
	//! \return True when no town, base or radio tower is within CLEARANCE.
	protected bool IsClearOfEverything(vector position)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns)
		{
			foreach (OVT_TownData town : towns.m_Towns)
			{
				if (town && vector.Distance(position, town.location) < CLEARANCE)
					return false;
			}
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return true;

		if (occupying.m_Bases)
		{
			foreach (OVT_BaseData baseData : occupying.m_Bases)
			{
				if (baseData && vector.Distance(position, baseData.location) < CLEARANCE)
					return false;
			}
		}

		if (occupying.m_RadioTowers)
		{
			foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
			{
				if (tower && vector.Distance(position, tower.location) < CLEARANCE)
					return false;
			}
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE STANDING CloneModule TRAP, asserted directly on every module this phase ships.
//!
//! ⚠ Every deployment gets a CLONE of its config's modules, and CloneModule copies attribute by
//! attribute BY HAND and is NOT CHAINED - a subclass builds a fresh instance and repeats its parent's
//! whole copy list. A forgotten line does not warn, log or fail to parse: it ships the CLASS DEFAULT
//! instead of the authored value, on every deployment, forever. That is how m_fMaxCruiseSpeed was
//! lost on the vehicle module for a whole release.
//!
//! What a dropped line costs here, module by module:
//!   - m_eImportance     -> tower guards and sniper teams register at the class default tier and lose
//!                          the AI spawn-budget race on a busy server;
//!   - m_Placement       -> the placed module has nowhere to stand anybody, wants 0 groups, registers
//!                          NOTHING, logs nothing;
//!   - m_sCompositionTag -> the composition module resolves nothing and builds no structure;
//!   - m_eSlotType       -> a road checkpoint hunts for a flat SMALL slot and never finds one;
//!   - m_eParkingType    -> every truck asks for a car-sized spot and the base parks nothing.
//!
//! ⚠ Every probe value is non-zero, non-empty and non-false, which is the point: a `new` instance
//! starts at 0 / "" / false / enum 0, so a probe value of false or 0 would be indistinguishable from
//! a dropped copy. [Attribute] defvalues do NOT apply to `new`.
//!
//! Nothing is registered or created - four bare `new` modules; CloneModule is pure field copying.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_NewModuleClonesCarryEveryAttribute : SCR_AutotestCaseBase
{
	//! Distinctive probe values. None may be 0, "" or false - see the class header.
	static const string PROBE_NAME = "OVT_TEST clone probe";
	static const string PROBE_GROUP = "OVT_TEST group type";
	static const string PROBE_TAG = "OVT_TEST composition tag";
	static const string PROBE_VEHICLE = "OVT_TEST vehicle type";
	static const int PROBE_MIN_GROUPS = 3;
	static const int PROBE_MAX_GROUPS = 7;
	static const float PROBE_SPAWN_RADIUS = 123.5;
	static const int PROBE_COST = 41;
	static const int PROBE_REINFORCE_COST = 17;
	static const float PROBE_SEARCH_RADIUS = 321.5;
	static const int PROBE_VEHICLE_COUNT = 5;
	static const int PROBE_VEHICLE_COST = 77;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = VerifyInfantryModule();

		if (failure == "")
			failure = VerifyPlacedModule();

		if (failure == "")
			failure = VerifyCompositionModule();

		if (failure == "")
			failure = VerifyParkedVehicleModule();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Every attribute of the infantry, placed-infantry, composition and parked-vehicle modules survives CloneModule", LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ONE attribute this phase added to the shipped infantry module.
	//! \return An empty string when the clone carries it, or the failure.
	protected string VerifyInfantryModule()
	{
		OVT_InfantrySpawningDeploymentModule module = new OVT_InfantrySpawningDeploymentModule();
		AuthorInfantryAttributes(module);

		OVT_InfantrySpawningDeploymentModule clone = OVT_InfantrySpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
			return "OVT_InfantrySpawningDeploymentModule.CloneModule() did not answer an infantry module at all";

		return CompareInfantryAttributes(module, clone, "OVT_InfantrySpawningDeploymentModule");
	}

	//------------------------------------------------------------------------------------------------
	//! All thirteen inherited attributes plus the placed module's own two.
	//! \return An empty string when the clone carries them all, or the first missing one.
	protected string VerifyPlacedModule()
	{
		OVT_PlacedInfantrySpawningDeploymentModule module = new OVT_PlacedInfantrySpawningDeploymentModule();
		AuthorInfantryAttributes(module);
		module.m_Placement = new OVT_SniperMarkerPlacementProvider();
		module.m_fSearchRadius = PROBE_SEARCH_RADIUS;

		OVT_PlacedInfantrySpawningDeploymentModule clone = OVT_PlacedInfantrySpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
			return "OVT_PlacedInfantrySpawningDeploymentModule.CloneModule() did not answer a placed-infantry module at all";

		string failure = CompareInfantryAttributes(module, clone, "OVT_PlacedInfantrySpawningDeploymentModule");
		if (failure != "")
			return failure;

		if (!clone.m_Placement)
			return "The clone of OVT_PlacedInfantrySpawningDeploymentModule carries NO placement provider - it would have nowhere to stand anybody, want 0 groups and register nothing at all, silently";

		if (clone.m_fSearchRadius != PROBE_SEARCH_RADIUS)
			return string.Format("The clone of OVT_PlacedInfantrySpawningDeploymentModule carries m_fSearchRadius %1, expected the authored %2",
				clone.m_fSearchRadius.ToString(), PROBE_SEARCH_RADIUS.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! All thirteen inherited attributes plus the composition module's own three.
	//! \return An empty string when the clone carries them all, or the first missing one.
	protected string VerifyCompositionModule()
	{
		OVT_CompositionSpawningDeploymentModule module = new OVT_CompositionSpawningDeploymentModule();
		AuthorInfantryAttributes(module);
		module.m_sCompositionTag = PROBE_TAG;
		module.m_eSlotType = OVT_EDeploymentSlotType.ROAD_LARGE;
		module.m_bFillAmmoBoxes = true;

		OVT_CompositionSpawningDeploymentModule clone = OVT_CompositionSpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
			return "OVT_CompositionSpawningDeploymentModule.CloneModule() did not answer a composition module at all";

		string failure = CompareInfantryAttributes(module, clone, "OVT_CompositionSpawningDeploymentModule");
		if (failure != "")
			return failure;

		if (clone.m_sCompositionTag != PROBE_TAG)
			return string.Format("The clone of OVT_CompositionSpawningDeploymentModule carries m_sCompositionTag '%1', expected '%2' - it would resolve no composition and build no structure",
				clone.m_sCompositionTag, PROBE_TAG);

		if (clone.m_eSlotType != OVT_EDeploymentSlotType.ROAD_LARGE)
			return string.Format("The clone of OVT_CompositionSpawningDeploymentModule carries slot type %1, expected ROAD_LARGE - a road checkpoint would hunt for a flat slot it will never find",
				typename.EnumToString(OVT_EDeploymentSlotType, clone.m_eSlotType));

		if (!clone.m_bFillAmmoBoxes)
			return "The clone of OVT_CompositionSpawningDeploymentModule carries m_bFillAmmoBoxes FALSE, expected the authored TRUE";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! All five of the parked-vehicle module's attributes.
	//! \return An empty string when the clone carries them all, or the first missing one.
	protected string VerifyParkedVehicleModule()
	{
		OVT_ParkedVehicleSpawningDeploymentModule module = new OVT_ParkedVehicleSpawningDeploymentModule();
		module.m_sModuleName = PROBE_NAME;
		module.m_sVehicleType = PROBE_VEHICLE;
		module.m_iVehicleCount = PROBE_VEHICLE_COUNT;
		module.m_iCostPerVehicle = PROBE_VEHICLE_COST;
		module.m_eParkingType = OVT_ParkingType.PARKING_HEAVY;

		OVT_ParkedVehicleSpawningDeploymentModule clone = OVT_ParkedVehicleSpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
			return "OVT_ParkedVehicleSpawningDeploymentModule.CloneModule() did not answer a parked-vehicle module at all";

		if (clone.m_sModuleName != PROBE_NAME)
			return "The clone of OVT_ParkedVehicleSpawningDeploymentModule lost m_sModuleName";

		if (clone.m_sVehicleType != PROBE_VEHICLE)
			return string.Format("The clone of OVT_ParkedVehicleSpawningDeploymentModule carries m_sVehicleType '%1', expected '%2' - it would resolve no prefab and park nothing",
				clone.m_sVehicleType, PROBE_VEHICLE);

		if (clone.m_iVehicleCount != PROBE_VEHICLE_COUNT)
			return string.Format("The clone of OVT_ParkedVehicleSpawningDeploymentModule carries m_iVehicleCount %1, expected %2",
				clone.m_iVehicleCount.ToString(), PROBE_VEHICLE_COUNT.ToString());

		if (clone.m_iCostPerVehicle != PROBE_VEHICLE_COST)
			return string.Format("The clone of OVT_ParkedVehicleSpawningDeploymentModule carries m_iCostPerVehicle %1, expected %2 - the deployment's total cost would be wrong",
				clone.m_iCostPerVehicle.ToString(), PROBE_VEHICLE_COST.ToString());

		if (clone.m_eParkingType != OVT_ParkingType.PARKING_HEAVY)
			return string.Format("The clone of OVT_ParkedVehicleSpawningDeploymentModule carries parking type %1, expected PARKING_HEAVY - every vehicle would ask for the wrong spot size and the base would park nothing",
				typename.EnumToString(OVT_ParkingType, clone.m_eParkingType));

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Sets every attribute OVT_InfantrySpawningDeploymentModule declares to a distinctive value.
	//! \param[in] module The module to author.
	protected void AuthorInfantryAttributes(notnull OVT_InfantrySpawningDeploymentModule module)
	{
		module.m_sModuleName = PROBE_NAME;
		module.m_sGroupType = PROBE_GROUP;
		module.m_iMinGroupCount = PROBE_MIN_GROUPS;
		module.m_iMaxGroupCount = PROBE_MAX_GROUPS;
		module.m_bScaleByTownSize = true;
		module.m_fSpawnRadius = PROBE_SPAWN_RADIUS;
		module.m_iCostPerGroup = PROBE_COST;
		module.m_bAllowReinforcement = true;
		module.m_iReinforcementCost = PROBE_REINFORCE_COST;
		module.m_bSpawnAtNearestBase = true;
		module.m_bReinforceFromNearestBase = true;
		module.m_eImportance = SCR_EAISpawnImportance.CRITICAL;
		module.m_bSnapToRoad = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Compares every attribute OVT_InfantrySpawningDeploymentModule declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \param[in] label The class being checked, for the message.
	//! \return An empty string when all thirteen survived, or the first that did not.
	protected string CompareInfantryAttributes(notnull OVT_InfantrySpawningDeploymentModule module,
		notnull OVT_InfantrySpawningDeploymentModule clone, string label)
	{
		if (clone.m_sModuleName != module.m_sModuleName)
			return string.Format("%1's clone lost m_sModuleName - the module's OWNER KEY is derived from it, so its groups would be registered under a different key and never reclaimed", label);

		if (clone.m_sGroupType != module.m_sGroupType)
			return string.Format("%1's clone lost m_sGroupType - core resolves (factionKey, groupName) and would refuse every registration", label);

		if (clone.m_iMinGroupCount != module.m_iMinGroupCount)
			return string.Format("%1's clone lost m_iMinGroupCount", label);

		if (clone.m_iMaxGroupCount != module.m_iMaxGroupCount)
			return string.Format("%1's clone lost m_iMaxGroupCount - the force size would be the class default on every deployment", label);

		if (clone.m_bScaleByTownSize != module.m_bScaleByTownSize)
			return string.Format("%1's clone lost m_bScaleByTownSize", label);

		if (clone.m_fSpawnRadius != module.m_fSpawnRadius)
			return string.Format("%1's clone lost m_fSpawnRadius", label);

		if (clone.m_iCostPerGroup != module.m_iCostPerGroup)
			return string.Format("%1's clone lost m_iCostPerGroup - the deployment's resource cost would be wrong", label);

		if (clone.m_bAllowReinforcement != module.m_bAllowReinforcement)
			return string.Format("%1's clone lost m_bAllowReinforcement - a wiped force would never be rebought", label);

		if (clone.m_iReinforcementCost != module.m_iReinforcementCost)
			return string.Format("%1's clone lost m_iReinforcementCost", label);

		if (clone.m_bSpawnAtNearestBase != module.m_bSpawnAtNearestBase)
			return string.Format("%1's clone lost m_bSpawnAtNearestBase", label);

		if (clone.m_bReinforceFromNearestBase != module.m_bReinforceFromNearestBase)
			return string.Format("%1's clone lost m_bReinforceFromNearestBase", label);

		if (clone.m_eImportance != module.m_eImportance)
			return string.Format("%1's clone lost m_eImportance - every group would register at the class default tier and lose the AI spawn-budget race", label);

		if (clone.m_bSnapToRoad != module.m_bSnapToRoad)
			return string.Format("%1's clone lost m_bSnapToRoad - a garrison authored to hold a PLACE would be road-snapped up to 500 m away and, with a DEFEND or null plan, would hold THERE", label);

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE THREE BASE-DEFENSE CONFIGS THAT USED TO BE PATROLS: two still are, on the base's OWN AUTHORED
//! SQUARE, and the third is now a placed road overwatch that never moves.
//!
//! "Garrisons never wander, patrols always do" at its root. A plan is the only opt-in for being
//! walked while dormant, so getting it backwards is invisible in play until a base's entire garrison
//! has walked off into the countryside - or a "patrol" stands in one spot for a whole campaign.
//!
//!   - Base Garrison Patrol and Base Heavy Patrol author PERIMETER_BASE, which walks the nearest base
//!     controller's AUTHORED square and ROAD-SNAPS NOTHING. Plain PERIMETER is still the town
//!     patrol's road-snapped ring.
//!   - Base AT Section is no longer a patrol: a placed-infantry module with the road-slot overwatch
//!     provider and a one-point DEFEND plan.
//!
//! The geometry is asserted against the LIVE base, which is what this tier can prove and the Logic
//! tier cannot: that the numbers reaching BuildSquarePerimeterPlan are the ones a designer AUTHORED
//! ON THE BASE, and that nothing pulled a corner onto a road. Every corner sits at the authored
//! radius and within the jitter band of an authored corner bearing.
//!
//! ⚠ PERIMETER_BASE is asserted BY NAME, not inferred from the plan shape: the DEFEND branch also
//! answers with a plan, and a config that lost its m_ePatrolType line would fall back to the enum's
//! zero value and read as an ordinary authoring value rather than a mistake.
//!
//! Asked off the config template with no deployment behind it. With no parent deployment the patrol
//! module falls back to "circle where you are", so handing it the BASE's own position is what puts
//! the base controller inside its 250 m lookup.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_BasePatrolConfigsCyclePerimeter : SCR_AutotestCaseBase
{
	//! The two shipped base-defense configs that are allowed to move.
	static const string GARRISON_CONFIG = "Base Garrison Patrol";
	static const string HEAVY_CONFIG = "Base Heavy Patrol";

	//! The one that used to be a third patrol and is now a placed overwatch.
	static const string AT_CONFIG = "Base AT Section";

	//! How far a corner may sit off the authored radius, in metres. The square's XZ is exact - only Y
	//! moves under the ground snap - and every distance below is taken horizontally, so this is slop
	//! and not a real allowance. A road-snapped corner would miss by tens or hundreds of metres.
	static const float RADIUS_TOLERANCE_M = 1;

	//! How far a corner's bearing may sit off the nearest authored one: the jitter band plus a degree
	//! for float slop.
	static const float ANGLE_TOLERANCE_DEG = 11;

	//! The threat floor and acquisition priority the AT section is authored with. Both are behaviour a
	//! player feels and neither logs anything if it drifts.
	//!
	//! ⚠ Re-scaled 50 -> 20 on 2026-08-20 with the threat scale itself, not because the intent changed.
	//! Candidate scores used to carry the GLOBAL campaign threat added to every position (~420 in the
	//! campaign that exposed it), so a floor of 50 gated nothing. CalculateThreatLevel() now returns
	//! the SPATIAL score alone, roughly 0-60.
	static const int AT_MINIMUM_THREAT = 20;
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so no shipped deployment config can be resolved at all");
			return true;
		}

		vector basePosition;
		OVT_BaseControllerComponent controller = FindTestWorldBase(basePosition);
		if (!controller)
		{
			SetFailure("No OVT_BaseControllerComponent resolved within %1 m of this world's first base centre - PERIMETER_BASE reads its square off that component, so without one every base garrison would fall back to an un-authored ring and log a warning per plan",
				OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS.ToString());
			return true;
		}

		array<string> configNames = {GARRISON_CONFIG, HEAVY_CONFIG};

		foreach (string configName : configNames)
		{
			string failure = VerifyPatrolConfig(manager, configName, controller, basePosition);
			if (failure != "")
			{
				SetFailure(failure);
				return true;
			}
		}

		string atFailure = VerifyATSection(manager);
		if (atFailure != "")
		{
			SetFailure(atFailure);
			return true;
		}

		PrintFormat("'%1' and '%2' build cycling PERIMETER_BASE plans on the base's own authored square", GARRISON_CONFIG, HEAVY_CONFIG);
		PrintFormat("The square is %1 m at %2 degrees; every corner landed on it, un-snapped, within the +/-%3 degree jitter band",
			controller.m_fPerimeterRadius.ToString(), controller.m_fPerimeterRotation.ToString(),
			OVT_PatrolBehaviorDeploymentModule.PERIMETER_ROTATION_JITTER_DEG.ToString());
		PrintFormat("'%1' is a placed road overwatch: one-point non-cycling DEFEND, provider '%2'", AT_CONFIG, "road slot overwatch");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! This world's first base, and the controller component that carries its authored square.
	//! \param[out] position The base centre.
	//! \return The controller, or null when there is no base or it carries no controller.
	protected OVT_BaseControllerComponent FindTestWorldBase(out vector position)
	{
		position = vector.Zero;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases || occupying.m_Bases.IsEmpty())
			return null;

		position = occupying.m_Bases[0].location;

		return OVT_BaseControllerComponent.FindNearestBaseControllerWithin(position, OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS);
	}

	//------------------------------------------------------------------------------------------------
	//! One of the two configs that walk the authored square.
	//! \param[in] manager The deployment manager, for its registry.
	//! \param[in] configName The shipped config name to check.
	//! \param[in] controller The base controller carrying the authored square.
	//! \param[in] basePosition The base centre, which is also the position the plan is asked for.
	//! \return An empty string when every claim holds for it, or the first broken one.
	protected string VerifyPatrolConfig(notnull OVT_DeploymentManagerComponent manager, string configName, notnull OVT_BaseControllerComponent controller, vector basePosition)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(configName);
		if (!config)
			return string.Format("The deployment registry does not resolve '%1' - the config file is missing, misnamed, or has no entry in overthrowDeployments.conf, and the base defense it carries can never be created", configName);

		if (!config.IsValidConfig())
			return string.Format("Config '%1' resolves but is not valid (no name, no modules, or no spawning module) - the evaluator refuses it in CreateDeployment and logs nothing a player would see", configName);

		OVT_PatrolBehaviorDeploymentModule patrol = FindPatrolModule(config);
		if (!patrol)
			return string.Format("Config '%1' carries no OVT_PatrolBehaviorDeploymentModule - every group it registers would get a null plan and hold the spawn point, which is a garrison and not a patrol", configName);

		if (patrol.m_ePatrolType != OVT_PatrolType.PERIMETER_BASE)
			return string.Format("Config '%1' authors patrol type %2, not PERIMETER_BASE - it would either hold one post or walk a ROAD-SNAPPED ring, and a base's roads run through it rather than around it",
				configName, typename.EnumToString(OVT_PatrolType, patrol.m_ePatrolType));

		OVT_VirtualWaypointPlan plan = patrol.BuildVirtualPlan(basePosition);

		string shape = VerifyPlanShape(plan, configName);
		if (shape != "")
			return shape;

		return VerifySquareGeometry(plan, configName, controller, basePosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Everything a movable plan has to be, whatever built it.
	//! \param[in] plan The plan the patrol module answered with.
	//! \param[in] configName Its config's name, for the messages.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyPlanShape(OVT_VirtualWaypointPlan plan, string configName)
	{
		if (!plan)
			return string.Format("The patrol module of '%1' answered with no plan at all - its groups would register with no waypoints", configName);

		int count = plan.m_aPositions.Count();
		if (count == 0)
			return string.Format("'%1' builds an EMPTY plan - the patrol would register with no waypoints and never move", configName);

		if (plan.m_aTypes.Count() != count || plan.m_aParams.Count() != count)
			return string.Format("'%1' builds a ragged plan (%2 positions, %3 types) - RegisterGroup refuses a ragged plan outright, so the patrol would silently never be registered",
				configName, count.ToString(), plan.m_aTypes.Count().ToString());

		if (!plan.m_bCycle)
			return string.Format("'%1' builds a NON-CYCLING plan - the patrol would walk to its last corner and guard that quarter of the base for the rest of the campaign", configName);

		if (CountMovable(plan) == 0)
			return string.Format("'%1' builds a plan with no movable point - a plan is the ONLY opt-in for being walked while dormant, so this patrol would stand still", configName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! THE AMENDMENT'S HEADLINE CLAIM: the corners are on the square the DESIGNER authored, and none of
	//! them was pulled onto a road.
	//! \param[in] plan The plan to measure.
	//! \param[in] configName Its config's name, for the messages.
	//! \param[in] controller The base controller carrying the authored square.
	//! \param[in] basePosition The base centre.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifySquareGeometry(notnull OVT_VirtualWaypointPlan plan, string configName, notnull OVT_BaseControllerComponent controller, vector basePosition)
	{
		float radius = controller.m_fPerimeterRadius;
		if (radius <= 0)
			return string.Format("The base controller authors a perimeter radius of %1 - a square with no size cannot be asserted, and at runtime the patrol would silently fall back to the module's own radius", radius.ToString());

		int corners = 0;

		for (int i = 0; i < plan.m_aTypes.Count(); i++)
		{
			if (plan.m_aTypes[i] != OVT_EVirtualWaypointType.PATROL)
				continue;

			corners++;

			vector corner = plan.m_aPositions[i];

			float distance = HorizontalDistance(corner, basePosition);
			if (Math.AbsFloat(distance - radius) > RADIUS_TOLERANCE_M)
				return string.Format("'%1': a corner is %2 m from the base centre, expected the authored %3 m - either the authored square is being ignored, or the corner was ROAD-SNAPPED off it, which is the one thing a base perimeter must never be",
					configName, distance.ToString(), radius.ToString());

			float bearingOff = BearingOffAuthored(basePosition, corner, controller.m_fPerimeterRotation);
			if (bearingOff > ANGLE_TOLERANCE_DEG)
				return string.Format("'%1': a corner sits %2 degrees off the nearest authored corner of the square, and the whole jitter band is only +/-%3 degrees",
					configName, bearingOff.ToString(), OVT_PatrolBehaviorDeploymentModule.PERIMETER_ROTATION_JITTER_DEG.ToString());
		}

		if (corners != OVT_VirtualPlanFactory.PERIMETER_POINTS)
			return string.Format("'%1' builds %2 movable corner(s), expected %3 - a base perimeter is a four-corner square",
				configName, corners.ToString(), OVT_VirtualPlanFactory.PERIMETER_POINTS.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The AT section: placed, not patrolling, and standing beside the road slots.
	//! \param[in] manager The deployment manager, for its registry.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyATSection(notnull OVT_DeploymentManagerComponent manager)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(AT_CONFIG);
		if (!config)
			return string.Format("The deployment registry does not resolve '%1' - the config file is missing, misnamed, or has no entry in overthrowDeployments.conf", AT_CONFIG);

		if (!config.IsValidConfig())
			return string.Format("Config '%1' resolves but is not valid (no name, no modules, or no spawning module)", AT_CONFIG);

		if (config.m_iMinimumThreatLevel != AT_MINIMUM_THREAT)
			return string.Format("Config '%1' authors a minimum threat of %2, expected %3 - an AT section is a late-campaign answer to armour and this is when a base starts buying one",
				AT_CONFIG, config.m_iMinimumThreatLevel.ToString(), AT_MINIMUM_THREAT.ToString());

		// ⚠ A priority assertion was deleted here (2026-08-22), not weakened. It pinned m_iPriority to an
		// exact number and broke the moment the author retuned it - which is the whole purpose of the
		// field, so "the author changed his mind" and "somebody broke escalation order" became
		// indistinguishable. It was not converted to a relative-order check either: the framework
		// requires no RELATIONSHIP between two configs' priorities (a pure sort key and a threshold), so
		// there is no inversion that breaks anything, only one a designer might not want.
		//
		// What IS still asserted is structural: that it resolves, that it is valid, that it carries a
		// placed module with a placement provider, and that it is offered at bases at all.

		OVT_PlacedInfantrySpawningDeploymentModule placed = FindPlacedModule(config);
		if (!placed)
			return string.Format("Config '%1' carries no OVT_PlacedInfantrySpawningDeploymentModule - its AT teams would be rolled onto a ring around the marker instead of standing beside the road slots", AT_CONFIG);

		if (!placed.m_Placement)
			return string.Format("Config '%1' authors no m_Placement provider - the module has nowhere to stand anybody, wants 0 groups and registers NOTHING, logging nothing", AT_CONFIG);

		OVT_RoadSlotOverwatchPlacementProvider overwatch = OVT_RoadSlotOverwatchPlacementProvider.Cast(placed.m_Placement);
		if (!overwatch)
			return string.Format("Config '%1' authors the '%2' placement provider - it must be the road-slot overwatch one, which is what puts the team where a checkpoint would be",
				AT_CONFIG, placed.m_Placement.GetProviderName());

		if (overwatch.m_fSideOffset <= 0)
			return string.Format("Config '%1' authors a side offset of %2 - at zero the AT team stands in the middle of the road slot, inside whatever checkpoint the base builds there later",
				AT_CONFIG, overwatch.m_fSideOffset.ToString());

		OVT_VirtualWaypointPlan plan = ResolvePlanLikeProduction(config);
		if (!plan)
			return string.Format("'%1' builds NO plan - a stationed AT team holds its post through a DEFEND waypoint, exactly as the defense positions do", AT_CONFIG);

		int count = plan.m_aPositions.Count();
		if (count != 1)
			return string.Format("'%1' builds a %2-point plan, expected exactly one - an overwatch post is one place held by one team", AT_CONFIG, count.ToString());

		if (plan.m_aTypes.Count() != count || plan.m_aParams.Count() != count)
			return string.Format("'%1' builds a ragged plan (%2 positions, %3 types) - RegisterGroup refuses a ragged plan outright", AT_CONFIG, count.ToString(), plan.m_aTypes.Count().ToString());

		if (plan.m_aTypes[0] != OVT_EVirtualWaypointType.DEFEND)
			return string.Format("'%1' builds a plan whose only point is type %2, expected DEFEND - any movable type here hands the movement tick an AT team to walk off its overwatch",
				AT_CONFIG, plan.m_aTypes[0].ToString());

		if (plan.m_bCycle)
			return string.Format("'%1' builds a CYCLING plan - a one-point cycle is still a cycle, and the point of an overwatch is that it is never left", AT_CONFIG);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! How far a corner's bearing is from the NEAREST corner of the authored square.
	//!
	//! Folded to a quarter turn rather than matched corner-by-corner, because which authored corner a
	//! walk starts on follows the walker's own approach - the square does not move, but the walk order
	//! rotates, and pinning the order here would pin a tie-break instead of the geometry.
	//! \param[in] centre The base centre.
	//! \param[in] corner The plan point.
	//! \param[in] rotationDeg The authored rotation of the square.
	//! \return Degrees off the nearest authored corner, 0..45.
	protected float BearingOffAuthored(vector centre, vector corner, float rotationDeg)
	{
		vector direction = corner - centre;
		direction[1] = 0;

		if (direction.Length() < 0.01)
			return 180;

		float delta = direction.ToYaw() - rotationDeg;

		// Math.Floor folds negatives correctly, which a modulo would not: EnforceScript's % is integer
		// only and keeps the sign of its left operand.
		delta = delta - (Math.Floor(delta / 90) * 90);

		if (delta > 45)
			delta = 90 - delta;

		return Math.AbsFloat(delta);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] a First position.
	//! \param[in] b Second position.
	//! \return The distance between them in the XZ plane, ignoring the ground snap's Y.
	protected float HorizontalDistance(vector a, vector b)
	{
		return vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! The plan a group of this config would be registered with, resolved the way
	//! OVT_BaseSpawningDeploymentModule.ResolveVirtualPlan() does it: ask every BEHAVIOUR module in
	//! authored order, take the first non-null answer.
	//! \param[in] config The config to ask.
	//! \return The plan, or null when no behaviour module has an opinion.
	protected OVT_VirtualWaypointPlan ResolvePlanLikeProduction(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = config.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			if (!behaviorModule)
				continue;

			OVT_VirtualWaypointPlan plan = behaviorModule.BuildVirtualPlan(OVT_TEST_VirtualizationFixture.PickPosition());
			if (plan)
				return plan;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to search.
	//! \return Its first placed-infantry spawning module, or null.
	protected OVT_PlacedInfantrySpawningDeploymentModule FindPlacedModule(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = config.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_PlacedInfantrySpawningDeploymentModule placed = OVT_PlacedInfantrySpawningDeploymentModule.Cast(spawningModule);
			if (placed)
				return placed;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to search.
	//! \return Its first patrol behaviour module, or null.
	protected OVT_PatrolBehaviorDeploymentModule FindPatrolModule(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = config.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			OVT_PatrolBehaviorDeploymentModule patrol = OVT_PatrolBehaviorDeploymentModule.Cast(behaviorModule);
			if (patrol)
				return patrol;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] plan The plan to count.
	//! \return How many of its points the movement tick would advance along.
	protected int CountMovable(notnull OVT_VirtualWaypointPlan plan)
	{
		int movable = 0;
		foreach (int type : plan.m_aTypes)
		{
			if (type == OVT_EVirtualWaypointType.PATROL || type == OVT_EVirtualWaypointType.MOVE)
				movable++;
		}

		return movable;
	}
}

//------------------------------------------------------------------------------------------------
//! A base whose centre sits inside a town's 500 m bounds STILL carries the BASE bit, and a BASE-only
//! base-defense config accepts it. Decision S1, and this case is its only mechanical guard.
//!
//! GetPrimaryLocationTypeAtPosition() tests towns before bases against a hardcoded 500 m radius, so a
//! base centre inside those bounds classified as TOWN and could be offered NO base-only config.
//! Measured 2026-08-17: 4 of Eden's 10 bases and this test world's only base (114 m). The legacy
//! system never asked what kind of place a base was, so leaving it would have been a straight
//! regression on ~40 % of the map's bases.
//!
//! ⚠ The radius is the safety argument and is asserted here as a NUMBER. BASE_CLASSIFICATION_RADIUS
//! is deliberately EQUAL to HasExistingDeploymentOfType()'s 250 m name-scoped dedup radius, so every
//! position that gains the bit is within the dedup's reach of the base's own copy of each config and
//! a base and a shadowing town cannot each buy a full set of base defense. Raise this constant to
//! "fix" a base at 300 m and force doubling becomes possible at every base whose town centre is
//! between the two radii.
//!
//! The negative control: a probe just OUTSIDE the radius, taken toward the town so the precedence
//! chain still answers TOWN, must NOT carry the BASE bit. Whether this world's geometry can produce
//! such a probe depends on the town's bounds, so that half is skipped-with-a-print.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_TownShadowedBaseAcceptsBaseConfig : SCR_AutotestCaseBase
{
	//! The first shipped BASE-only base-defense config, used as the consumer of the classification.
	static const string BASE_ONLY_CONFIG = "Base Garrison Patrol";

	//! The radius the OR-in is required to use, restated here so a change to the production constant
	//! has to be a deliberate two-file change rather than a silent one.
	static const float EXPECTED_RADIUS = 250;

	//! How far past the radius the negative control probes.
	static const float OUTSIDE_MARGIN = 10;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS != EXPECTED_RADIUS)
		{
			SetFailure("BASE_CLASSIFICATION_RADIUS is %1, not %2 - it MUST equal HasExistingDeploymentOfType()'s dedup radius, or a base and a position just outside the dedup's reach can each buy a full set of base defense",
				OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS.ToString(), EXPECTED_RADIUS.ToString());
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases || occupying.m_Bases.IsEmpty())
		{
			SetFailure("This world has no bases, so the BASE classification cannot be exercised - InitializeBases() found none");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null, so 'is this base shadowed by a town' cannot be asked at all");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so the classification has no shipped consumer to be checked against");
			return true;
		}

		OVT_DeploymentConfig baseConfig = manager.m_DeploymentRegistry.FindConfigByName(BASE_ONLY_CONFIG);
		if (!baseConfig)
		{
			SetFailure("The deployment registry does not resolve '%1' - the first BASE-only base-defense config is missing, so nothing consumes the classification", BASE_ONLY_CONFIG);
			return true;
		}

		OVT_BaseData shadowed = FindTownShadowedBase(manager, occupying);

		if (!shadowed)
		{
			// Not a failure: whether a world HAS a town-shadowed base is a property of its layers.
			// The positive claim is still made against an ordinary base centre below.
			shadowed = occupying.m_Bases[0];
			Print("[Overthrow][TEST] No town-shadowed base in this world - the S1 claim is exercised against an ordinary base centre instead", LogLevel.WARNING);
		}
		else
		{
			PrintFormat("Base at %1 is town-shadowed: its primary classification is %2", shadowed.location.ToString(),
				manager.GetPrimaryLocationTypeAtPosition(shadowed.location).ToString());
		}

		OVT_LocationTypeFlag classification = manager.GetLocationTypeAtPosition(shadowed.location);

		if ((classification & OVT_LocationTypeFlag.BASE) == 0)
		{
			SetFailure("The base centre at %1 classifies %2 with NO base bit - every BASE-only base-defense config is unbuyable there, silently, exactly as it was before the bit was OR-ed in",
				shadowed.location.ToString(), classification.ToString());
			return true;
		}

		if (!baseConfig.CanUseLocationType(classification))
		{
			SetFailure("'%1' refuses a base centre classified %2 - the config's authored location types and the classification no longer agree, so the base can never be fortified",
				BASE_ONLY_CONFIG, classification.ToString());
			return true;
		}

		string negative = VerifyOutsideRadius(manager, towns, shadowed);
		if (negative != "")
		{
			SetFailure(negative);
			return true;
		}

		PrintFormat("A shadowed base centre classifies %1 and '%2' accepts it", classification.ToString(), BASE_ONLY_CONFIG);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The first base whose own centre the PRECEDENCE CHAIN calls a TOWN - i.e. one that could be
	//! offered no BASE-only config at all before the OR-in existed.
	//! \param[in] manager The deployment manager, for the classification.
	//! \param[in] occupying The occupying faction manager, for the base list.
	//! \return The shadowed base, or null when this world has none.
	protected OVT_BaseData FindTownShadowedBase(notnull OVT_DeploymentManagerComponent manager,
		notnull OVT_OccupyingFactionManager occupying)
	{
		foreach (OVT_BaseData baseData : occupying.m_Bases)
		{
			if (!baseData)
				continue;

			if (manager.GetPrimaryLocationTypeAtPosition(baseData.location) == OVT_LocationTypeFlag.TOWN)
				return baseData;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The negative control: a position just outside BASE_CLASSIFICATION_RADIUS, taken toward the
	//! nearest town so the precedence chain still answers TOWN there, must NOT gain the BASE bit.
	//!
	//! Skipped with a printed line when this world's geometry cannot produce such a probe, because a
	//! probe that classifies BASE through the chain's own 500 m branch would be asserting the wrong
	//! thing.
	//! \param[in] manager The deployment manager.
	//! \param[in] towns The town manager, for the direction to probe in.
	//! \param[in] baseData The base to probe away from.
	//! \return An empty string when the control holds or could not be run, or the broken claim.
	protected string VerifyOutsideRadius(notnull OVT_DeploymentManagerComponent manager,
		notnull OVT_TownManagerComponent towns, notnull OVT_BaseData baseData)
	{
		OVT_TownData town = towns.GetNearestTown(baseData.location);
		if (!town)
		{
			Print("[Overthrow][TEST] No town near this base - the outside-the-radius control was not run", LogLevel.NORMAL);
			return "";
		}

		vector direction = town.location - baseData.location;
		direction[1] = 0;

		if (direction.Length() < 1)
		{
			Print("[Overthrow][TEST] The base and its town share a centre - the outside-the-radius control was not run", LogLevel.NORMAL);
			return "";
		}

		direction.Normalize();
		vector probe = baseData.location + direction * (EXPECTED_RADIUS + OUTSIDE_MARGIN);

		if (manager.GetPrimaryLocationTypeAtPosition(probe) != OVT_LocationTypeFlag.TOWN)
		{
			Print("[Overthrow][TEST] The outside-the-radius probe does not classify TOWN through the chain, so the control cannot distinguish the OR-in from the chain's own base branch - not run", LogLevel.NORMAL);
			return "";
		}

		OVT_LocationTypeFlag outside = manager.GetLocationTypeAtPosition(probe);
		if ((outside & OVT_LocationTypeFlag.BASE) != 0)
			return string.Format("A position %1 m from the base centre classifies %2 and carries the BASE bit - the OR-in reaches past the 250 m dedup radius, so that position and the base can each buy their own full set of base defense",
				(EXPECTED_RADIUS + OUTSIDE_MARGIN).ToString(), outside.ToString());

		PrintFormat("Negative control held: a probe %1 m out classifies %2 with no base bit",
			(EXPECTED_RADIUS + OUTSIDE_MARGIN).ToString(), outside.ToString());
		return "";
	}
}


//------------------------------------------------------------------------------------------------
//! THE THREE PLACED BASE-DEFENSE CONFIGS RESOLVE, VALIDATE, AND NONE OF THEM CAN EVER WANDER.
//!
//! The other half of "garrisons never wander, patrols always do", asserted at the only place the
//! difference exists: the PLAN.
//!   - Base Tower Guards     -> NO plan at all (null). Legacy parity and deliberate: a post waypoint
//!     parks a guard at a smart action that is a pose loop with no fire node, while an idle group
//!     keeps full threat and attack reactions.
//!   - Base Sniper Positions -> NO plan at all (null), same reason.
//!   - Base Defense Positions -> a ONE-POINT, NON-CYCLING DEFEND plan, because the legacy
//!     defense-position guard did carry a defend waypoint on its post.
//!
//! ⚠ "Null" is asserted through the same walk the production path uses, not by counting modules:
//! ResolveVirtualPlan() asks every behaviour module in order and takes the FIRST non-null answer, and
//! the reinforcement module IS a behaviour module. A case that asserted "no patrol module" would pass
//! if some future behaviour module started answering a plan of its own.
//!
//! The placement provider is asserted too - a placed module with no m_Placement wants zero groups,
//! registers nothing and logs nothing, the most silent way one of these ships broken. So is
//! m_eImportance HIGH: a tower guard at the class default loses the AI spawn-budget race on a busy
//! server, which is indistinguishable from "the placement failed".
//!
//! Asked off the config template with no deployment behind it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_PlacedBaseConfigsHoldTheirPosts : SCR_AutotestCaseBase
{
	//! The three shipped configs that stand men on exact posts.
	static const string DEFENSE_CONFIG = "Base Defense Positions";
	static const string TOWER_CONFIG = "Base Tower Guards";
	static const string SNIPER_CONFIG = "Base Sniper Positions";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null");
			return true;
		}

		if (!manager.m_DeploymentRegistry)
		{
			SetFailure("The deployment manager has no registry, so no shipped deployment config can be resolved at all");
			return true;
		}

		string failure = VerifyStatic(manager, TOWER_CONFIG, SCR_EAISpawnImportance.HIGH);

		if (failure == "")
			failure = VerifyStatic(manager, SNIPER_CONFIG, SCR_EAISpawnImportance.HIGH);

		if (failure == "")
			failure = VerifyDefensePositions(manager);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("'%1' and '%2' build NO plan at all; '%3' builds a one-point non-cycling DEFEND plan - none of the three can be walked by the movement tick",
			TOWER_CONFIG, SNIPER_CONFIG, DEFENSE_CONFIG);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A config whose groups get no waypoint at all.
	//! \param[in] manager The deployment manager, for its registry.
	//! \param[in] configName The shipped config name.
	//! \param[in] expectedImportance The AI spawn-budget tier the config is required to author.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyStatic(notnull OVT_DeploymentManagerComponent manager, string configName, SCR_EAISpawnImportance expectedImportance)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(configName);

		string shared = VerifyShared(config, configName, expectedImportance);
		if (shared != "")
			return shared;

		OVT_VirtualWaypointPlan plan = ResolvePlanLikeProduction(config);
		if (plan)
			return string.Format("'%1' builds a plan of %2 point(s) - it must build NONE. Its guards would be given waypoints, and any movable point in them hands the movement tick a garrison to walk off its post",
				configName, plan.m_aPositions.Count().ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The one placed config that DOES get a plan - a one-point, non-cycling DEFEND.
	//! \param[in] manager The deployment manager, for its registry.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyDefensePositions(notnull OVT_DeploymentManagerComponent manager)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(DEFENSE_CONFIG);

		string shared = VerifyShared(config, DEFENSE_CONFIG, SCR_EAISpawnImportance.NORMAL);
		if (shared != "")
			return shared;

		OVT_VirtualWaypointPlan plan = ResolvePlanLikeProduction(config);
		if (!plan)
			return string.Format("'%1' builds NO plan - the legacy defense-position guard carried a DEFEND waypoint on its post and this config is its replacement", DEFENSE_CONFIG);

		int count = plan.m_aPositions.Count();
		if (count != 1)
			return string.Format("'%1' builds a %2-point plan, expected exactly one - a defense position is one post held by one group",
				DEFENSE_CONFIG, count.ToString());

		if (plan.m_aTypes.Count() != count || plan.m_aParams.Count() != count)
			return string.Format("'%1' builds a ragged plan (%2 positions, %3 types, %4 params) - RegisterGroup refuses a ragged plan outright, so the guards would silently never be registered",
				DEFENSE_CONFIG, count.ToString(), plan.m_aTypes.Count().ToString(), plan.m_aParams.Count().ToString());

		if (plan.m_aTypes[0] != OVT_EVirtualWaypointType.DEFEND)
			return string.Format("'%1' builds a plan whose only point is type %2, expected DEFEND - any movable type here is an invitation for the movement tick to walk the base's defense away",
				DEFENSE_CONFIG, plan.m_aTypes[0].ToString());

		if (plan.m_bCycle)
			return string.Format("'%1' builds a CYCLING plan - a one-point cycle is still a cycle, and the point of a defense position is that it is never left", DEFENSE_CONFIG);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Everything all three configs must satisfy.
	//! \param[in] config The resolved config, or null.
	//! \param[in] configName Its name, for the messages.
	//! \param[in] expectedImportance The AI spawn-budget tier it is required to author.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyShared(OVT_DeploymentConfig config, string configName, SCR_EAISpawnImportance expectedImportance)
	{
		if (!config)
			return string.Format("The deployment registry does not resolve '%1' - the config file is missing, misnamed, or has no entry in overthrowDeployments.conf, and the base defense it carries can never be created", configName);

		if (!config.IsValidConfig())
			return string.Format("Config '%1' resolves but is not valid (no name, no modules, or no spawning module) - the evaluator refuses it in CreateDeployment and logs nothing a player would see", configName);

		// ⚠ A priority assertion was deleted here (2026-08-22), not weakened. It pinned m_iPriority to an
		// exact number and broke the moment the author retuned it - which is the whole purpose of the
		// field, so "the author changed his mind" and "somebody broke escalation order" became
		// indistinguishable. It was not converted to a relative-order check either: the framework
		// requires no RELATIONSHIP between two configs' priorities (a pure sort key and a threshold), so
		// there is no inversion that breaks anything, only one a designer might not want.
		//
		// What IS still asserted is structural: that it resolves, that it is valid, that it carries a
		// placed module with a placement provider, and that it is offered at bases at all.

		if ((config.m_iAllowedLocationTypes & OVT_LocationTypeFlag.BASE) == 0)
			return string.Format("Config '%1' does not allow the BASE location type (%2) - it would never be offered at a base at all",
				configName, config.m_iAllowedLocationTypes.ToString());

		OVT_PlacedInfantrySpawningDeploymentModule placed = FindPlacedModule(config);
		if (!placed)
			return string.Format("Config '%1' carries no OVT_PlacedInfantrySpawningDeploymentModule - its groups would be rolled onto a ring around the marker instead of standing on their posts", configName);

		if (!placed.m_Placement)
			return string.Format("Config '%1' authors no m_Placement provider - the module has nowhere to stand anybody, wants 0 groups and registers NOTHING, logging nothing", configName);

		if (placed.m_eImportance != expectedImportance)
			return string.Format("Config '%1' authors AI spawn importance %2, expected %3 - the wrong tier loses the spawn-budget race on a busy server and the post is simply empty when the player arrives",
				configName, typename.EnumToString(SCR_EAISpawnImportance, placed.m_eImportance), typename.EnumToString(SCR_EAISpawnImportance, expectedImportance));

		if (placed.m_bSnapToRoad)
			return string.Format("Config '%1' leaves m_bSnapToRoad ON - the placed module does not read it today, but a future class swap would silently move this garrison up to 500 m onto a road", configName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The plan a group of this config would be registered with, resolved the way
	//! OVT_BaseSpawningDeploymentModule.ResolveVirtualPlan() does it: ask every BEHAVIOUR module in
	//! authored order, take the first non-null answer.
	//! \param[in] config The config to ask.
	//! \return The plan, or null when no behaviour module has an opinion.
	protected OVT_VirtualWaypointPlan ResolvePlanLikeProduction(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = config.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			if (!behaviorModule)
				continue;

			OVT_VirtualWaypointPlan plan = behaviorModule.BuildVirtualPlan(OVT_TEST_VirtualizationFixture.PickPosition());
			if (plan)
				return plan;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to search.
	//! \return Its first placed-infantry spawning module, or null.
	protected OVT_PlacedInfantrySpawningDeploymentModule FindPlacedModule(notnull OVT_DeploymentConfig config)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = config.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_PlacedInfantrySpawningDeploymentModule placed = OVT_PlacedInfantrySpawningDeploymentModule.Cast(spawningModule);
			if (placed)
				return placed;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! THE RE-MATERIALISATION CLAIM: the second time a placed group comes back, its men stand where they
//! stood the first time.
//!
//! Live, this needs a real despawn/respawn cycle driven by real distance, which needs a deployment
//! marker - and a marker leaks a repeating 8-12 s UpdateDeployment into a shared test world. So the
//! placement DECISION was made a pure function of its arguments, the production teleport routes
//! through it, and the claim reduces to "the same inputs answer the same output twice".
//!
//! Six claims, each a different way a guard ends up in the wrong place:
//!   1. POST SELECTION IS BY INDEX AND WRAPS. A fourth group where the world offers three posts
//!      doubles up on post 0 rather than being dropped somewhere the provider never chose.
//!   2. TWO MATERIALISATIONS AGREE, with the arrival counter reset between them exactly as
//!      OnPlacedGroupDespawning() resets it.
//!   3. A MISSED RESET CANNOT MARCH ANYONE OFF THE POST - arrival + spread lands on the same step as
//!      arrival. Without the modulo wrap a tower guard walks another 1.2 m sideways every
//!      materialisation, forever, and eventually off his walkway.
//!   4. THE SIDEWAYS STEP IS EXACTLY MEMBER_SPACING, asserted against the production constant.
//!   5. THE POST'S OWN HEADING IS RESPECTED - a spotter stepped one metre "east" of a marker facing
//!      east would be standing in front of the sniper.
//!   6. DEFENSIVE INPUTS DO NOT INDEX OUT OF BOUNDS. ⚠ EnforceScript's % keeps the sign of its left
//!      operand, so a negative index would answer a negative slot.
//!
//! Nothing is registered, created or mutated - bare placements and static calls.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_PlacedArrivalPlacementIsStable : SCR_AutotestCaseBase
{
	//! Three posts, far enough apart that a mixed-up assignment cannot look like a rounding error.
	static const vector POST_A = "1000 10 1000";
	static const vector POST_B = "1050 20 1000";
	static const vector POST_C = "1100 30 1000";

	//! A roster size to wrap the arrival index against.
	static const int SPREAD = 4;

	//! Tolerance for a metre-scale comparison. vector.Distance is +1 ULP off at 1000 m and 2000 m, and
	//! these posts are deliberately at 1000 m to sit on that case.
	static const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_DeploymentPlacement> posts = BuildPosts();

		string failure = VerifyPostSelection(posts);

		if (failure == "")
			failure = VerifyTwoMaterialisationsAgree(posts);

		if (failure == "")
			failure = VerifyMissedResetWraps(posts);

		if (failure == "")
			failure = VerifySpacing(posts);

		if (failure == "")
			failure = VerifyHeadingIsRespected();

		if (failure == "")
			failure = VerifyDefensiveInputs(posts);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Placement is stable across two materialisations, wraps past %1 posts and %2 arrival steps, and steps %3 m along each post's own right vector",
			posts.Count().ToString(), SPREAD.ToString(), OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Three headingless posts.
	protected array<ref OVT_DeploymentPlacement> BuildPosts()
	{
		array<ref OVT_DeploymentPlacement> posts = new array<ref OVT_DeploymentPlacement>();

		posts.Insert(new OVT_DeploymentPlacement(POST_A, vector.Zero));
		posts.Insert(new OVT_DeploymentPlacement(POST_B, vector.Zero));
		posts.Insert(new OVT_DeploymentPlacement(POST_C, vector.Zero));

		return posts;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 1: one post per group, in handle order, wrapping past the end.
	//! \param[in] posts The three posts.
	//! \return An empty string when it holds, or the broken claim.
	protected string VerifyPostSelection(notnull array<ref OVT_DeploymentPlacement> posts)
	{
		for (int i = 0; i < posts.Count(); i++)
		{
			OVT_DeploymentPlacement post = OVT_PlacedInfantrySpawningDeploymentModule.PostForGroup(posts, i);
			if (!post)
				return string.Format("Group %1 was given no post at all, with %2 offered", i.ToString(), posts.Count().ToString());

			if (post.m_vPosition != posts[i].m_vPosition)
				return string.Format("Group %1 was given the post at %2, expected the one at %3 - posts are assigned by handle order and nothing else",
					i.ToString(), post.m_vPosition.ToString(), posts[i].m_vPosition.ToString());
		}

		OVT_DeploymentPlacement wrapped = OVT_PlacedInfantrySpawningDeploymentModule.PostForGroup(posts, posts.Count());
		if (!wrapped || wrapped.m_vPosition != POST_A)
			return string.Format("A %1th group where %2 posts are offered did not wrap back onto the first post - it must double up rather than be placed somewhere the provider never chose",
				(posts.Count() + 1).ToString(), posts.Count().ToString());

		array<ref OVT_DeploymentPlacement> empty = new array<ref OVT_DeploymentPlacement>();
		if (OVT_PlacedInfantrySpawningDeploymentModule.PostForGroup(empty, 0))
			return "PostForGroup answered a post from an EMPTY post list - the caller uses null to mean 'the world offers nowhere to stand', and a non-null answer here would be a read off the end of an empty array";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 2: the headline promise.
	//! \param[in] posts The three posts.
	//! \return An empty string when it holds, or the broken claim.
	protected string VerifyTwoMaterialisationsAgree(notnull array<ref OVT_DeploymentPlacement> posts)
	{
		array<vector> first = Materialise(posts);
		array<vector> second = Materialise(posts);

		if (first.Count() != second.Count())
			return string.Format("The two materialisations produced %1 and %2 placements", first.Count().ToString(), second.Count().ToString());

		for (int i = 0; i < first.Count(); i++)
		{
			if (vector.Distance(first[i], second[i]) > EPSILON)
				return string.Format("Placement %1 moved between two materialisations: %2 then %3. A base's guards would drift a little further from their posts every time the player drove away and came back",
					i.ToString(), first[i].ToString(), second[i].ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! One materialisation: every group's whole roster arrives, counting from zero.
	//! \param[in] posts The posts.
	//! \return The world position of every arrival, in order.
	protected array<vector> Materialise(notnull array<ref OVT_DeploymentPlacement> posts)
	{
		array<vector> placements = new array<vector>();

		for (int group = 0; group < posts.Count(); group++)
		{
			for (int arrival = 0; arrival < SPREAD; arrival++)
			{
				placements.Insert(OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(posts, group, arrival, SPREAD));
			}
		}

		return placements;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 3: a missed despawn notification wraps instead of marching.
	//! \param[in] posts The three posts.
	//! \return An empty string when it holds, or the broken claim.
	protected string VerifyMissedResetWraps(notnull array<ref OVT_DeploymentPlacement> posts)
	{
		for (int arrival = 0; arrival < SPREAD; arrival++)
		{
			vector reset = OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(posts, 0, arrival, SPREAD);
			vector missed = OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(posts, 0, arrival + SPREAD, SPREAD);

			if (vector.Distance(reset, missed) > EPSILON)
				return string.Format("Arrival %1 of a SECOND materialisation whose counter was never reset landed at %2 instead of %3 - without the wrap every missed despawn notification steps the whole group another %4 m sideways, forever",
					arrival.ToString(), missed.ToString(), reset.ToString(), OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 4: the step is exactly the production constant, on a headingless post along world +X.
	//! \param[in] posts The three posts.
	//! \return An empty string when it holds, or the broken claim.
	protected string VerifySpacing(notnull array<ref OVT_DeploymentPlacement> posts)
	{
		vector zeroth = OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(posts, 0, 0, SPREAD);

		if (vector.Distance(zeroth, POST_A) > EPSILON)
			return string.Format("The FIRST man of a group landed at %1, not on the post itself (%2) - arrival 0 takes step 0 and no offset at all", zeroth.ToString(), POST_A.ToString());

		vector first = OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(posts, 0, 1, SPREAD);
		float step = vector.Distance(first, zeroth);

		if (Math.AbsFloat(step - OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING) > EPSILON)
			return string.Format("The second man of a group stands %1 m from the first, expected MEMBER_SPACING (%2 m)",
				step.ToString(), OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING.ToString());

		vector offset = first - zeroth;
		if (Math.AbsFloat(offset[0] - OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING) > EPSILON)
			return string.Format("On a HEADINGLESS post the step went %1 rather than straight along world +X - an identity rotation must produce an identity offset", offset.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 5: the post's authored heading turns the step with it.
	//! \return An empty string when it holds, or the broken claim.
	protected string VerifyHeadingIsRespected()
	{
		array<ref OVT_DeploymentPlacement> turned = new array<ref OVT_DeploymentPlacement>();
		turned.Insert(new OVT_DeploymentPlacement(POST_A, "90 0 0"));

		vector zeroth = OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(turned, 0, 0, SPREAD);
		vector first = OVT_PlacedInfantrySpawningDeploymentModule.PlacementForArrival(turned, 0, 1, SPREAD);

		float step = vector.Distance(first, zeroth);
		if (Math.AbsFloat(step - OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING) > EPSILON)
			return string.Format("On a post with an authored heading the step is %1 m, expected MEMBER_SPACING (%2 m) - the rotation must turn the step, not stretch it",
				step.ToString(), OVT_PlacedInfantrySpawningDeploymentModule.MEMBER_SPACING.ToString());

		if (Math.AbsFloat((first - zeroth)[0]) > EPSILON)
			return string.Format("A post yawed 90 degrees still stepped along world +X (%1) - the offset is not being taken through the post's own transform, so a sniper's spotter would be placed in front of him instead of beside him",
				(first - zeroth).ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 6: inputs no caller passes today still cannot index out of bounds.
	//! \param[in] posts The three posts.
	//! \return An empty string when it holds, or the broken claim.
	protected string VerifyDefensiveInputs(notnull array<ref OVT_DeploymentPlacement> posts)
	{
		OVT_DeploymentPlacement negativeGroup = OVT_PlacedInfantrySpawningDeploymentModule.PostForGroup(posts, -3);
		if (!negativeGroup || negativeGroup.m_vPosition != POST_A)
			return "A negative group index did not resolve to the first post - EnforceScript's % keeps the sign of its left operand, so an unguarded negative reads off the front of the array";

		if (OVT_PlacedInfantrySpawningDeploymentModule.SlotForArrival(-3, SPREAD) != 0)
			return "A negative arrival index did not resolve to step 0";

		int fallback = OVT_PlacedInfantrySpawningDeploymentModule.SlotForArrival(2, 0);
		if (fallback != 2 % OVT_PlacedInfantrySpawningDeploymentModule.FALLBACK_SPREAD)
			return string.Format("An unreadable roster size gave step %1 - it must fall back to FALLBACK_SPREAD (%2), which is what stops a group with no readable roster marching sideways forever",
				fallback.ToString(), OVT_PlacedInfantrySpawningDeploymentModule.FALLBACK_SPREAD.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Test-only: pins the campaign threat the sniper provider filters markers against.
//!
//! The provider deliberately reads the LIVE occupying-faction threat rather than taking one as an
//! argument, because the threat a deployment carries is a snapshot taken when it was created -
//! filtering against that would freeze a base's sniper coverage at whatever the campaign looked like
//! the day the deployment appeared, and the point of the per-marker gate is that a base grows sniper
//! teams as threat RISES. Overriding the one protected accessor keeps that true.
//!
//! ⚠ NOT [BaseContainerProps] - it must never be authorable in a config.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ThreatPinnedSniperProvider : OVT_SniperMarkerPlacementProvider
{
	//! The threat every call reports, whatever the campaign is doing.
	float m_fPinnedThreat;

	//------------------------------------------------------------------------------------------------
	//! \return The pinned threat.
	override protected float GetOccupyingThreat()
	{
		return m_fPinnedThreat;
	}
}

//------------------------------------------------------------------------------------------------
//! THE AT SECTION'S POSTS: exactly one side-offset ACROSS the road slot, looking back at it, and the
//! same answer however the slots arrive.
//!
//!   1. THE OFFSET IS ACROSS THE ROAD, NOT ALONG IT - the step is taken along the slot's OWN right
//!      vector, because a slot carries the road's rotation. Stepping along world X would put the team
//!      in the middle of any north-south road, which looks like a placement bug rather than an axis
//!      bug, and only at some bases.
//!   2. THE TEAM LOOKS AT THE ROAD. An AT team facing away from its approach engages seconds late,
//!      which reads as "the AT team is useless" and never as a heading bug.
//!   3. THE SIDE IS A FUNCTION OF THE SLOT, NOT OF LIST ORDER. The provider is re-asked on every
//!      convergence pass, after every load and after every re-discovery of a base's slots, so a
//!      left/right pick that came from a list index would teleport a team across the road.
//!
//! Asserted through the production statics (PostBesideSlot / SideForSlot), not a copy. The transforms
//! are hand-built so the claim does not depend on this world having a base with road slots - the Init
//! tier never runs InitBaseControllers(), so a live slot list does not exist here at all.
//!
//! The live half is still exercised as a comparison between two consecutive resolves plus the
//! never-null contract; the count is PRINTED rather than asserted.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_RoadSlotOverwatchIsOffsetAndStable : SCR_AutotestCaseBase
{
	//! The offset Deployment_BaseATSection.conf authors, asserted against the shipped config below.
	static const float AUTHORED_SIDE_OFFSET = 15;

	//! The shipped config that carries the provider.
	static const string AT_CONFIG = "Base AT Section";

	//! Metres of slop allowed on a position claim, and the same for a dot product treated as a ratio.
	static const float EPSILON = 0.05;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = VerifyOffsetIsAcrossTheSlot();

		if (failure == "")
			failure = VerifyPostLooksBackAtTheSlot();

		if (failure == "")
			failure = VerifySideFollowsTheSlotAndNotTheOrder();

		if (failure == "")
			failure = VerifyShippedConfigAuthorsTheOffset();

		if (failure == "")
			failure = VerifyLiveResolveIsRepeatable();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Every AT post is exactly %1 m ACROSS its road slot, looking back at it, and the side follows the slot rather than the order the slots arrived in",
			AUTHORED_SIDE_OFFSET.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 1 - the step is the authored distance, and it is perpendicular to the slot's facing.
	//! \return An empty string when the claim holds, or the broken half.
	protected string VerifyOffsetIsAcrossTheSlot()
	{
		// Two slots on deliberately awkward headings: a road running north-south, and one on no round
		// number at all. A world-X step would pass the first and fail nothing, which is why the second
		// is here.
		array<float> headings = {0, 37};

		foreach (float heading : headings)
		{
			vector slotMat[4];
			BuildSlotTransform(Vector(1200, 30, 1200), heading, slotMat);

			OVT_DeploymentPlacement post = OVT_RoadSlotOverwatchPlacementProvider.PostBesideSlot(slotMat, AUTHORED_SIDE_OFFSET);
			if (!post)
				return string.Format("PostBesideSlot answered nothing for a slot on heading %1", heading.ToString());

			float distance = vector.Distance(post.m_vPosition, slotMat[3]);
			if (Math.AbsFloat(distance - AUTHORED_SIDE_OFFSET) > EPSILON)
				return string.Format("A post beside a slot on heading %1 is %2 m from it, expected the authored %3 m",
					heading.ToString(), distance.ToString(), AUTHORED_SIDE_OFFSET.ToString());

			vector step = post.m_vPosition - slotMat[3];
			step.Normalize();

			// ALONG the road is the slot's forward vector: the step must have no component on it.
			float alongRoad = Math.AbsFloat(vector.Dot(step, slotMat[2]));
			if (alongRoad > EPSILON)
				return string.Format("A post beside a slot on heading %1 is %2 of the way ALONG the road rather than across it - the offset is not being taken on the slot's own right vector",
					heading.ToString(), alongRoad.ToString());

			// ACROSS the road is the slot's right vector: the step must be entirely on it.
			float acrossRoad = Math.AbsFloat(vector.Dot(step, slotMat[0]));
			if (Math.AbsFloat(acrossRoad - 1) > EPSILON)
				return string.Format("A post beside a slot on heading %1 lies %2 of the way across the slot's right vector, expected 1",
					heading.ToString(), acrossRoad.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 2 - the occupant faces the slot it is covering.
	//! \return An empty string when the claim holds, or the broken half.
	protected string VerifyPostLooksBackAtTheSlot()
	{
		vector slotMat[4];
		BuildSlotTransform(Vector(1200, 30, 1200), 37, slotMat);

		OVT_DeploymentPlacement post = OVT_RoadSlotOverwatchPlacementProvider.PostBesideSlot(slotMat, AUTHORED_SIDE_OFFSET);

		vector postMat[4];
		post.GetTransform(postMat);

		vector towardsSlot = slotMat[3] - post.m_vPosition;
		towardsSlot[1] = 0;
		towardsSlot.Normalize();

		float facing = vector.Dot(postMat[2], towardsSlot);
		if (Math.AbsFloat(facing - 1) > EPSILON)
			return string.Format("An AT post's forward vector is %1 of the way towards the slot it covers, expected 1 - the team would start the fight looking the wrong way", facing.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 3 - the side is a property of the slot, so the same slots answer the same posts however
	//! they are ordered, and two different slots really can take different sides.
	//! \return An empty string when the claim holds, or the broken half.
	protected string VerifySideFollowsTheSlotAndNotTheOrder()
	{
		// One metre apart flips the parity, which is what makes neighbouring slots alternate.
		array<vector> slots = {Vector(1200, 30, 1200), Vector(1201, 30, 1200), Vector(1200, 30, 1207)};

		int first = OVT_RoadSlotOverwatchPlacementProvider.SideForSlot(slots[0]);
		int second = OVT_RoadSlotOverwatchPlacementProvider.SideForSlot(slots[1]);

		if (first == 0 || second == 0)
			return "SideForSlot answered 0 - a post with no side would land on the slot itself";

		if (first == second)
			return string.Format("Two slots one metre apart both answered side %1 - neighbouring road slots would put every AT team on the same side of the road", first.ToString());

		// ORDER INDEPENDENCE, demonstrated rather than argued from the signature. The same three slots
		// are resolved FORWARDS and then BACKWARDS; each slot must answer the same post both times,
		// which is only true if the side never looks at a position in a list.
		array<ref OVT_DeploymentPlacement> forwards = new array<ref OVT_DeploymentPlacement>();

		for (int i = 0; i < slots.Count(); i++)
		{
			vector forwardMat[4];
			BuildSlotTransform(slots[i], 37, forwardMat);

			forwards.Insert(OVT_RoadSlotOverwatchPlacementProvider.PostBesideSlot(forwardMat, AUTHORED_SIDE_OFFSET));
		}

		for (int j = slots.Count() - 1; j >= 0; j--)
		{
			vector backwardMat[4];
			BuildSlotTransform(slots[j], 37, backwardMat);

			OVT_DeploymentPlacement backward = OVT_RoadSlotOverwatchPlacementProvider.PostBesideSlot(backwardMat, AUTHORED_SIDE_OFFSET);

			float moved = vector.Distance(forwards[j].m_vPosition, backward.m_vPosition);
			if (moved > EPSILON)
				return string.Format("Slot %1 answered posts %2 m apart when the same three slots were resolved in the opposite order - an AT team would cross the road whenever a slot list was rebuilt",
					j.ToString(), moved.ToString());

			float turned = vector.Distance(forwards[j].m_vAngles, backward.m_vAngles);
			if (turned > EPSILON)
				return string.Format("Slot %1 answered a different heading (%2 apart) when the same three slots were resolved in the opposite order", j.ToString(), turned.ToString());
		}

		// And a negative coordinate is an ordinary position, not a third answer: EnforceScript's % keeps
		// the sign of its left operand, so an unguarded parity would read as neither side.
		int negative = OVT_RoadSlotOverwatchPlacementProvider.SideForSlot(Vector(-1201, 30, -1200));
		if (negative != 1 && negative != -1)
			return string.Format("A slot at a negative coordinate answered side %1, expected +1 or -1", negative.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped config really authors the offset this case asserts against.
	//! \return An empty string when the claim holds, or the broken half.
	protected string VerifyShippedConfigAuthorsTheOffset()
	{
		OVT_RoadSlotOverwatchPlacementProvider provider = FindShippedProvider();
		if (!provider)
			return string.Format("Could not reach the road-slot overwatch provider through '%1' - either the config does not resolve, it has no placed module, or its m_Placement is a different provider", AT_CONFIG);

		if (Math.AbsFloat(provider.m_fSideOffset - AUTHORED_SIDE_OFFSET) > EPSILON)
			return string.Format("'%1' authors a side offset of %2, and this case asserts %3 - one of the two moved",
				AT_CONFIG, provider.m_fSideOffset.ToString(), AUTHORED_SIDE_OFFSET.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The live contract: never null, and the same answer twice in a row.
	//! \return An empty string when the claim holds, or the broken half.
	protected string VerifyLiveResolveIsRepeatable()
	{
		OVT_RoadSlotOverwatchPlacementProvider provider = FindShippedProvider();
		if (!provider)
			return "";

		vector probe = FindBasePosition();

		array<ref OVT_DeploymentPlacement> first = provider.ResolvePlacements(probe, 280, 0);
		array<ref OVT_DeploymentPlacement> second = provider.ResolvePlacements(probe, 280, 0);

		if (!first || !second)
			return "The road-slot overwatch provider answered NULL - 'nothing here' is the ordinary answer for a placement provider and every caller would have to guard against it";

		if (first.Count() != second.Count())
			return string.Format("Two consecutive resolves answered %1 and %2 posts - the AT section would gain or lose a team on a convergence pass that changed nothing",
				first.Count().ToString(), second.Count().ToString());

		for (int i = 0; i < first.Count(); i++)
		{
			float moved = vector.Distance(first[i].m_vPosition, second[i].m_vPosition);
			if (moved > EPSILON)
				return string.Format("Post %1 moved %2 m between two consecutive resolves of the same base", i.ToString(), moved.ToString());
		}

		PrintFormat("The live road-slot overwatch resolve answered %1 post(s) at this world's base, twice, identically", first.Count().ToString());
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The provider instance the shipped AT config carries.
	//! \return It, or null when anything in the chain does not resolve.
	protected OVT_RoadSlotOverwatchPlacementProvider FindShippedProvider()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager || !manager.m_DeploymentRegistry)
			return null;

		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(AT_CONFIG);
		if (!config)
			return null;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = config.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_PlacedInfantrySpawningDeploymentModule placed = OVT_PlacedInfantrySpawningDeploymentModule.Cast(spawningModule);
			if (placed)
				return OVT_RoadSlotOverwatchPlacementProvider.Cast(placed.m_Placement);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This world's first base centre, or the origin when there is none.
	protected vector FindBasePosition()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases || occupying.m_Bases.IsEmpty())
			return "0 0 0";

		return occupying.m_Bases[0].location;
	}

	//------------------------------------------------------------------------------------------------
	//! A slot transform on a given heading - what a road slot's GetWorldTransform() answers.
	//! \param[in] position Where the slot is.
	//! \param[in] headingDeg Which way the road runs, in degrees.
	//! \param[out] outMat The transform.
	protected void BuildSlotTransform(vector position, float headingDeg, out vector outMat[4])
	{
		Math3D.AnglesToMatrix(Vector(headingDeg, 0, 0), outMat);
		outMat[3] = position;
	}
}

//------------------------------------------------------------------------------------------------
//! A CURATED SNIPER MARKER IS SKIPPED WHILE ITS OWN m_iMinimumThreat IS ABOVE THE CAMPAIGN'S THREAT,
//! AND MANNED FROM THE MOMENT IT IS NOT.
//!
//! A config's m_iMinimumThreatLevel gates the WHOLE deployment, all or nothing. The legacy
//! sniper-position upgrade gated PER MARKER, so a designer could author exposed forward positions
//! that only get manned once the campaign is hot. The framework offers no per-position equivalent, so
//! the filter lives in OVT_SniperMarkerPlacementProvider - the one piece of authored designer
//! progression in this feature with no config to protect it.
//!
//! ⚠ The BOUNDARY is asserted, not just the middle. The production test is `threat < minimum ->
//! skip`, so a marker whose threshold EQUALS the current threat is manned. An off-by-one flip to `<=`
//! would leave every always-on marker (m_iMinimumThreat 0, the authored default) unmanned at threat 0
//! on a brand-new campaign - invisible to a middle-of-the-range test.
//!
//! It runs against a WORLD-AUTHORED marker: spawning one would assert the arithmetic but not that the
//! sphere query finds the entities a level designer places. The claim is scoped to THIS marker rather
//! than to a count, so other markers in a larger world cannot make it flaky.
//!
//! ⚠ The one mutation - the marker's authored m_iMinimumThreat - is written and put back inside a
//! single Execute(), on every path including the red ones.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_SniperMarkerThreatGateFilters : SCR_AutotestCaseBase
{
	//! The campaign threat every resolve in this case is pinned to.
	static const float PINNED_THREAT = 50;

	//! Thresholds written onto the marker, either side of PINNED_THREAT and exactly on it.
	static const int THRESHOLD_BELOW = 25;
	static const int THRESHOLD_EQUAL = 50;
	static const int THRESHOLD_ABOVE = 75;

	//! How far around the marker the provider is asked. Small on purpose: the claim is about THIS
	//! marker, and a tight radius keeps the answer short.
	static const float SEARCH_RADIUS = 50;

	//! How close a returned post has to be to count as this marker's.
	static const float MATCH_EPSILON = 0.5;

	//! How far around each base and town the marker hunt looks.
	static const float HUNT_RADIUS = 500;

	//! Scratch for the marker hunt's own query.
	protected ref array<IEntity> m_aFoundMarkers;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		IEntity marker = FindAuthoredMarker();
		if (!marker)
		{
			Print("[Overthrow][TEST] This world authors no OVT_SniperPositionComponent marker within 500 m of any base or town - the per-marker threat gate was not exercised", LogLevel.NORMAL);
			return true;
		}

		OVT_SniperPositionComponent component = OVT_SniperPositionComponent.Cast(marker.FindComponent(OVT_SniperPositionComponent));
		if (!component)
		{
			SetFailure("The marker found by the hunt carries no OVT_SniperPositionComponent, which is the only thing the hunt filtered on");
			return true;
		}

		int authored = component.m_iMinimumThreat;

		string failure = Verify(marker, component);

		// Restore BEFORE reporting, on every path.
		component.m_iMinimumThreat = authored;

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// PrintFormat takes at most three string parameters, hence two lines.
		PrintFormat("The marker at %1 is offered at thresholds %2 and %3 against a campaign threat of 50",
			marker.GetOrigin().ToString(), THRESHOLD_BELOW.ToString(), THRESHOLD_EQUAL.ToString());
		PrintFormat("...and withheld at threshold %1; its authored threshold (%2) was restored",
			THRESHOLD_ABOVE.ToString(), authored.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The three threshold claims and the heading claim.
	//! \param[in] marker The world-authored marker.
	//! \param[in] component Its sniper-position component.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string Verify(notnull IEntity marker, notnull OVT_SniperPositionComponent component)
	{
		OVT_TEST_ThreatPinnedSniperProvider provider = new OVT_TEST_ThreatPinnedSniperProvider();
		provider.m_fPinnedThreat = PINNED_THREAT;

		vector origin = marker.GetOrigin();

		// BELOW: manned, and this is also what proves the query finds a world-authored marker at all -
		// without it, "withheld" below would be satisfied by a query that found nothing.
		component.m_iMinimumThreat = THRESHOLD_BELOW;
		array<ref OVT_DeploymentPlacement> below = provider.ResolvePlacements(origin, SEARCH_RADIUS, 0);

		if (!Offers(below, origin))
			return string.Format("A marker authored at minimum threat %1 was NOT offered at a campaign threat of %2 - either the sphere query does not see level-authored markers or the gate is inverted",
				THRESHOLD_BELOW.ToString(), PINNED_THREAT.ToString());

		// The heading claim rides on the below-threshold answer, which is the one that has a post in it.
		string heading = VerifyHeading(below, marker);
		if (heading != "")
			return heading;

		// EQUAL: the production test is `threat < minimum -> skip`, so equal is manned.
		component.m_iMinimumThreat = THRESHOLD_EQUAL;
		array<ref OVT_DeploymentPlacement> equal = provider.ResolvePlacements(origin, SEARCH_RADIUS, 0);

		if (!Offers(equal, origin))
			return string.Format("A marker whose minimum threat EQUALS the campaign threat (%1) was withheld - the gate has become `<=`, which also leaves every always-on marker (threshold 0) unmanned at threat 0 on a brand-new campaign",
				THRESHOLD_EQUAL.ToString());

		// ABOVE: withheld.
		component.m_iMinimumThreat = THRESHOLD_ABOVE;
		array<ref OVT_DeploymentPlacement> above = provider.ResolvePlacements(origin, SEARCH_RADIUS, 0);

		if (Offers(above, origin))
			return string.Format("A marker authored at minimum threat %1 was offered at a campaign threat of only %2 - the per-marker escalation a designer authored is being ignored and every exposed forward position is manned from day one",
				THRESHOLD_ABOVE.ToString(), PINNED_THREAT.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The placement carries the marker's own rotation, which is the entire reason a designer places a
	//! curated marker instead of letting the ring roller pick a spot.
	//! \param[in] placements The answer that contains this marker.
	//! \param[in] marker The marker.
	//! \return An empty string when it holds (or cannot be told apart), or the broken claim.
	protected string VerifyHeading(notnull array<ref OVT_DeploymentPlacement> placements, notnull IEntity marker)
	{
		vector authored = marker.GetAngles();

		if (authored == vector.Zero)
		{
			Print("[Overthrow][TEST] This world's sniper marker has no authored rotation, so 'the heading is preserved' cannot be told apart from 'the heading was thrown away' - not asserted", LogLevel.NORMAL);
			return "";
		}

		vector origin = marker.GetOrigin();

		foreach (OVT_DeploymentPlacement placement : placements)
		{
			if (!placement || vector.Distance(placement.m_vPosition, origin) > MATCH_EPSILON)
				continue;

			if (placement.m_vAngles != authored)
				return string.Format("The post offered for the marker at %1 faces %2, but the marker faces %3 - the team would spawn looking somewhere other than down the line the designer aimed them",
					origin.ToString(), placement.m_vAngles.ToString(), authored.ToString());

			return "";
		}

		return string.Format("The marker at %1 vanished from the answer between two calls with the same threshold", origin.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] placements An answer from the provider.
	//! \param[in] origin The marker's position.
	//! \return True when this marker's own post is in the answer.
	protected bool Offers(array<ref OVT_DeploymentPlacement> placements, vector origin)
	{
		if (!placements)
			return false;

		foreach (OVT_DeploymentPlacement placement : placements)
		{
			if (placement && vector.Distance(placement.m_vPosition, origin) <= MATCH_EPSILON)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The first level-authored sniper marker near any base or town.
	//! \return The marker entity, or null when this world has none.
	protected IEntity FindAuthoredMarker()
	{
		m_aFoundMarkers = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_Bases)
		{
			foreach (OVT_BaseData baseData : occupying.m_Bases)
			{
				if (!baseData)
					continue;

				world.QueryEntitiesBySphere(baseData.location, HUNT_RADIUS, AddMarker, FilterMarker, EQueryEntitiesFlags.ALL);
				if (!m_aFoundMarkers.IsEmpty())
					return m_aFoundMarkers[0];
			}
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns)
		{
			foreach (OVT_TownData town : towns.m_Towns)
			{
				if (!town)
					continue;

				world.QueryEntitiesBySphere(town.location, HUNT_RADIUS, AddMarker, FilterMarker, EQueryEntitiesFlags.ALL);
				if (!m_aFoundMarkers.IsEmpty())
					return m_aFoundMarkers[0];
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from the hunt's query.
	//! \return True when it carries a sniper-position component.
	protected bool FilterMarker(IEntity entity)
	{
		if (!entity)
			return false;

		return entity.FindComponent(OVT_SniperPositionComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A marker that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddMarker(IEntity entity)
	{
		m_aFoundMarkers.Insert(entity);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Test-only window onto OVT_CompositionSpawningDeploymentModule's protected latch.
//!
//! ApplyBuildDecision() is protected because it is an internal step of TryBuildComposition(), not an
//! API, so a subclass is the only honest way to assert it.
//!
//! ⚠ NOT [BaseContainerProps]. It must never appear in a Workbench config picker or be authorable
//! into a deployment config.
//------------------------------------------------------------------------------------------------
class OVT_TEST_CompositionProbeModule : OVT_CompositionSpawningDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] decision What DecideBuild() answered.
	//! \return Exactly what TryBuildComposition() would have gone on to do.
	bool ProbeApplyBuildDecision(OVT_ECompositionBuildDecision decision)
	{
		return ApplyBuildDecision(decision);
	}
}

//------------------------------------------------------------------------------------------------
//! A BASE NEVER GROWS A SECOND BUNKER. The no-double-build claim, asserted at the seam.
//!
//! A composition is a world ENTITY that vanilla persistence saves and restores on its own, and whose
//! slot claim comes back separately in the base controller's m_aSlotsFilled. So a deployment restored
//! from a save must build NOTHING: if it built again, every Continue would add one more structure to
//! every fortified base, in a different slot each time, forever (D7) - completely silent for the
//! first few loads.
//!
//! The Init tier never runs InitBaseControllers(), so m_aSlotsFilled is null and every slot list is
//! null. The DECISION was therefore made a pure function of its inputs (DecideBuild), the production
//! path routes through it, and the claim reduces to a truth table.
//!
//! Three answers, not two, and the asymmetry is the point:
//!   BUILD - a fresh deployment, first pass;
//!   SKIP  - do nothing NOW, retry next pass, latch NOTHING (a config template, or a force currently
//!           flagged eliminated - the rebuy path clears that flag and the structure is then owed);
//!   NEVER - do nothing EVER, and latch it (already attempted, or restored from a save).
//! ⚠ Latching a SKIP would make a base that converged one tick before its controller finished
//! initialising permanently unfortifiable. Not latching a NEVER is the double build. Both are silent.
//!
//! The latch is asserted separately from the decision, on a real module instance, because a decision
//! computed correctly and never applied fails in exactly the same way as a wrong decision.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_CompositionNeverBuildsTwice : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = VerifyDecisionTable();

		if (failure == "")
			failure = VerifyLatch();

		if (failure == "")
			failure = VerifyFreshModuleState();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("A restored deployment's composition module answers NEVER and latches it; a fresh one answers BUILD once and NEVER thereafter");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every row of DecideBuild(), each naming the campaign situation it stands for.
	//! \return An empty string when every row holds, or the first broken one.
	protected string VerifyDecisionTable()
	{
		// A config TEMPLATE - GetResourceCost() asks the registry's templates for their cost and they
		// have no deployment behind them. SKIP, never NEVER: that same template is cloned onto every
		// real deployment of this config and the clone must still be able to build.
		string row = ExpectDecision(false, false, false, false, OVT_ECompositionBuildDecision.SKIP,
			"a module with no deployment behind it (a config template)");
		if (row != "")
			return row;

		// The ordinary case: a brand-new deployment, first convergence pass.
		row = ExpectDecision(true, false, false, false, OVT_ECompositionBuildDecision.BUILD,
			"a fresh deployment on its first pass");
		if (row != "")
			return row;

		// IDEMPOTENCE. EnsureGroups() runs on activation, on the records-restored fan-out and on every
		// rebuy; the second call must not put a second structure in a second slot.
		row = ExpectDecision(true, true, false, false, OVT_ECompositionBuildDecision.NEVER,
			"a module that has already had its one attempt");
		if (row != "")
			return row;

		// D7, THE HEADLINE. The structure and its slot claim are already back from the save.
		row = ExpectDecision(true, false, true, false, OVT_ECompositionBuildDecision.NEVER,
			"a deployment restored from a save point");
		if (row != "")
			return row;

		// A wiped force does not quietly grow a new bunker - but the rebuy clears this flag, so the
		// structure is owed rather than forfeited.
		row = ExpectDecision(true, false, false, true, OVT_ECompositionBuildDecision.SKIP,
			"a deployment whose force is currently flagged eliminated");
		if (row != "")
			return row;

		// PRECEDENCE. A restored deployment that is ALSO flagged eliminated must still answer NEVER: if
		// the eliminated SKIP won here, the reinforcement that clears the flag would then build a second
		// structure alongside the one the save restored. This is the row that catches a re-ordering.
		row = ExpectDecision(true, false, true, true, OVT_ECompositionBuildDecision.NEVER,
			"a restored deployment that is also flagged eliminated - restored must win");
		if (row != "")
			return row;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The latch, on a real module instance: NEVER records itself, SKIP does not, and only BUILD gives
	//! the caller permission to go on.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyLatch()
	{
		OVT_TEST_CompositionProbeModule skipProbe = new OVT_TEST_CompositionProbeModule();
		if (skipProbe.ProbeApplyBuildDecision(OVT_ECompositionBuildDecision.SKIP))
			return "ApplyBuildDecision(SKIP) gave the caller permission to build - SKIP means 'not this pass', and building on it would put a structure in the world for a deployment that has no live deployment or whose force is wiped";

		if (skipProbe.HasAttemptedComposition())
			return "ApplyBuildDecision(SKIP) LATCHED the attempt - a base that converged one tick before its controller finished discovering slots would then be permanently unable to fortify, silently and for the rest of the campaign";

		OVT_TEST_CompositionProbeModule neverProbe = new OVT_TEST_CompositionProbeModule();
		if (neverProbe.ProbeApplyBuildDecision(OVT_ECompositionBuildDecision.NEVER))
			return "ApplyBuildDecision(NEVER) gave the caller permission to build - this is the D7 gate, and building through it is one extra bunker per load, forever";

		if (!neverProbe.HasAttemptedComposition())
			return "ApplyBuildDecision(NEVER) did NOT latch the attempt - the next convergence would ask again, and a reinforcement that clears the eliminated flags would then build a second structure beside the one the save restored";

		OVT_TEST_CompositionProbeModule buildProbe = new OVT_TEST_CompositionProbeModule();
		if (!buildProbe.ProbeApplyBuildDecision(OVT_ECompositionBuildDecision.BUILD))
			return "ApplyBuildDecision(BUILD) refused the caller permission to build - no base would ever get a bunker, a cache, an MG nest or a checkpoint at all";

		if (buildProbe.HasAttemptedComposition())
			return "ApplyBuildDecision(BUILD) latched the attempt before the build was even tried - the production path latches only once it has a slot, so that a module that could not find a base controller yet gets its free retry";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A freshly constructed module has neither a structure nor a spent attempt. Cheap, but it is what
	//! catches a class-default flip that would make every module a no-op or a rebuilder.
	//! \return An empty string when both claims hold, or the broken one.
	protected string VerifyFreshModuleState()
	{
		OVT_CompositionSpawningDeploymentModule fresh = new OVT_CompositionSpawningDeploymentModule();

		if (fresh.HasAttemptedComposition())
			return "A freshly constructed composition module already reports its attempt spent - every deployment of every fortification config would build nothing, with no warning anywhere";

		if (fresh.GetComposition())
			return "A freshly constructed composition module already answers a composition entity - its guards would be anchored on whatever that resolves to instead of on the structure they are meant to hold";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! One row of the truth table.
	//! \param[in] hasDeployment Whether a live deployment is behind the module.
	//! \param[in] alreadyAttempted Whether the one attempt is spent.
	//! \param[in] restoredFromSave Whether the deployment came back from a save.
	//! \param[in] eliminated Whether the force is flagged wiped out.
	//! \param[in] expected The decision the situation requires.
	//! \param[in] situation What this row stands for, in campaign terms.
	//! \return An empty string when the row holds, or the failure text.
	protected string ExpectDecision(bool hasDeployment, bool alreadyAttempted, bool restoredFromSave, bool eliminated, OVT_ECompositionBuildDecision expected, string situation)
	{
		OVT_ECompositionBuildDecision actual = OVT_CompositionSpawningDeploymentModule.DecideBuild(hasDeployment, alreadyAttempted, restoredFromSave, eliminated);

		if (actual == expected)
			return "";

		return string.Format("DecideBuild answered %1 for %2, expected %3",
			typename.EnumToString(OVT_ECompositionBuildDecision, actual), situation,
			typename.EnumToString(OVT_ECompositionBuildDecision, expected));
	}
}

//------------------------------------------------------------------------------------------------
//! TWO COMPOSITIONS NEVER SHARE A SLOT - which is what lets a legacy save's structures and a new
//! deployment's structures stand at one base without overlapping.
//!
//! A base controller keeps one m_aSlotsFilled list; the serializer writes it, InitBaseControllers()
//! restores it verbatim, and a composition module rolls for a slot NOT in it and then claims the one
//! it built in. Nothing ever removes an entry, so the list is a permanent, campaign-wide,
//! save-crossing record of "taken". Both halves have to hold:
//!   1. A CLAIMED SLOT IS NEVER SELECTED - else a new bunker spawns inside a legacy bunker on the
//!      first load of a converted campaign;
//!   2. A BUILD'S CLAIM ACTUALLY LANDS - else the same base builds into the same slot again next
//!      pass, stacking structures at one point.
//!
//! The Init tier never runs InitBaseControllers(), so a live-slot assertion is impossible here by
//! construction: the roll and the claim are static functions of the two arrays.
//!
//! ⚠ Real EntityIDs, not fabricated ones. array.Contains() compares by value and EntityID is an
//! opaque handle; invented ids could not tell "never selects a claimed slot" from "every id compares
//! equal". They are harvested read-only from entities this world already has.
//!
//! Invariants over samples, never a retry - each roll is random by design. Plus one deterministic
//! liveness row with an empty claim list, so a function that simply always refused could not pass.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_CompositionSlotClaimsAreRespected : SCR_AutotestCaseBase
{
	//! How many slots the case needs to distinguish "refused the claimed ones" from "refused
	//! everything". Three is the minimum: two claimed, one free.
	static const int SLOTS_NEEDED = 3;

	//! How many rolls each invariant is checked over. Every single one must hold.
	static const int SAMPLES = 60;

	//! How far around a base to look for entities to borrow ids from.
	static const float HARVEST_RADIUS = 150;

	//! Ids harvested from the world, in discovery order.
	protected ref array<ref EntityID> m_aHarvested;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref EntityID> slots = HarvestSlotIds();
		if (!slots)
		{
			Print("This world offers fewer than 3 distinct entity ids near its bases and towns, so the slot lottery cannot be exercised here at all - the claim is unasserted in this world rather than asserted weakly");
			return true;
		}

		string failure = VerifyDefensiveInputs(slots);

		if (failure == "")
			failure = VerifyEmptyClaimListAlwaysAnswers(slots);

		if (failure == "")
			failure = VerifyClaimedSlotsAreNeverSelected(slots);

		if (failure == "")
			failure = VerifyFullBaseIsRefused(slots);

		if (failure == "")
			failure = VerifyClaimRoundTrip(slots);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("A slot in m_aSlotsFilled is never rolled, a full base answers -1 rather than doubling up, and a claim takes its slot out of the lottery permanently");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Null and empty inputs answer -1/false rather than reaching the roll. RandInt(0, 0) is an engine
	//! error, so the emptiness guard is load-bearing rather than tidy.
	//! \param[in] slots The harvested ids.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyDefensiveInputs(notnull array<ref EntityID> slots)
	{
		array<ref EntityID> empty = new array<ref EntityID>();

		if (OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(null, empty) != -1)
			return "RollFreeSlotIndex answered an index for a NULL slot list - a base whose controller has not discovered slots of this size yet would be indexed into nothing";

		if (OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(empty, empty) != -1)
			return "RollFreeSlotIndex answered an index for an EMPTY slot list - RandInt(0, 0) is an engine error, so this guard is what stops a base with no road slots from erroring on every convergence";

		if (OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(slots, null) != -1)
			return "RollFreeSlotIndex answered an index against a NULL claim list - m_aSlotsFilled is null until InitializeBase() runs, and building then would claim a slot into a list that is about to be replaced";

		if (OVT_CompositionSpawningDeploymentModule.ClaimSlot(null, slots[0]))
			return "ClaimSlot reported success against a NULL claim list - the slot would be built in and never recorded, so the next pass would build in it again";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! THE LIVENESS ROW, and it is deterministic: with nothing claimed, every roll must answer a real
	//! index. Without it, a RollFreeSlotIndex that always refused would satisfy every invariant below.
	//! \param[in] slots The harvested ids.
	//! \return An empty string when every roll answers, or the first failure.
	protected string VerifyEmptyClaimListAlwaysAnswers(notnull array<ref EntityID> slots)
	{
		array<ref EntityID> filled = new array<ref EntityID>();

		for (int i = 0; i < SAMPLES; i++)
		{
			int index = OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(slots, filled);

			if (index < 0)
				return "With NOTHING claimed, a roll still refused every slot - no base would ever build a composition, and the only symptom would be one 'the base is full' warning per module";

			if (index >= slots.Count())
				return string.Format("A roll answered index %1 against %2 slots - the caller indexes straight into the array with it",
					index.ToString(), slots.Count().ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 1: a slot already in the claim list is never offered. Two of three claimed, so the only
	//! acceptable answers are "the free one" and "refused this roll".
	//! \param[in] slots The harvested ids.
	//! \return An empty string when every roll holds, or the first failure.
	protected string VerifyClaimedSlotsAreNeverSelected(notnull array<ref EntityID> slots)
	{
		array<ref EntityID> filled = new array<ref EntityID>();
		filled.Insert(slots[0]);
		filled.Insert(slots[1]);

		for (int i = 0; i < SAMPLES; i++)
		{
			int index = OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(slots, filled);

			if (index == 0 || index == 1)
				return string.Format("A roll offered slot %1, which is already in the claim list - a legacy campaign's bunker and a new deployment's bunker would be built at the same point, and m_aSlotsFilled would then hold the same slot twice",
					index.ToString());

			if (index > 2)
				return string.Format("A roll answered index %1 against 3 slots", index.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 1, at its limit: a base with every slot of the wanted size taken answers -1 on EVERY roll,
	//! deterministically. The production caller reads that as "the base is full", says so once and
	//! latches - it must never be reached by doubling up instead.
	//! \param[in] slots The harvested ids.
	//! \return An empty string when every roll refuses, or the first failure.
	protected string VerifyFullBaseIsRefused(notnull array<ref EntityID> slots)
	{
		array<ref EntityID> filled = new array<ref EntityID>();
		foreach (EntityID id : slots)
		{
			filled.Insert(id);
		}

		for (int i = 0; i < SAMPLES; i++)
		{
			int index = OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(slots, filled);

			if (index >= 0)
				return string.Format("With every slot claimed, a roll still offered slot %1 - the base would build a second structure on top of an existing one instead of reporting itself full",
					index.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 2: the claim lands, it is not duplicated, and it is what removes the slot from the lottery.
	//! This is the pair the whole coexistence argument rests on.
	//! \param[in] slots The harvested ids.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyClaimRoundTrip(notnull array<ref EntityID> slots)
	{
		array<ref EntityID> filled = new array<ref EntityID>();
		filled.Insert(slots[0]);
		filled.Insert(slots[1]);

		// The free slot before the claim.
		int before = OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(slots, filled);

		if (!OVT_CompositionSpawningDeploymentModule.ClaimSlot(filled, slots[2]))
			return "ClaimSlot refused to record a slot nothing had claimed - the structure would stand in a slot the base still believes is free, and the next fortification pass would build into it";

		if (!filled.Contains(slots[2]))
			return "ClaimSlot reported success but the slot is not in the claim list - m_aSlotsFilled is what the serializer writes, so the claim would also be missing from every future save";

		if (OVT_CompositionSpawningDeploymentModule.ClaimSlot(filled, slots[2]))
			return "ClaimSlot recorded the SAME slot twice - m_aSlotsFilled is written to the save point verbatim and never pruned, so duplicates accumulate across every load";

		if (filled.Count() != 3)
			return string.Format("The claim list holds %1 entries after two claims of one slot, expected 3", filled.Count().ToString());

		for (int i = 0; i < SAMPLES; i++)
		{
			if (OVT_CompositionSpawningDeploymentModule.RollFreeSlotIndex(slots, filled) >= 0)
				return string.Format("After claiming the last free slot (roll before the claim answered %1), a roll still offered one - the claim is recorded but not consulted, which is the same as not claiming at all",
					before.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Borrows SLOTS_NEEDED distinct, valid EntityIDs from entities this world already has. Read-only:
	//! nothing is spawned, moved or modified, and the ids stand in for slot entities purely as values.
	//! \return The ids, or null when this world cannot offer enough.
	protected array<ref EntityID> HarvestSlotIds()
	{
		m_aHarvested = new array<ref EntityID>();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		// The base markers themselves first - they are guaranteed distinct entities and cost no query.
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_Bases)
		{
			foreach (OVT_BaseData baseData : occupying.m_Bases)
			{
				if (baseData)
					AddHarvested(baseData.entId);
			}
		}

		if (m_aHarvested.Count() >= SLOTS_NEEDED)
			return m_aHarvested;

		// Then whatever stands around them. STATIC only: buildings and props, never a character.
		if (occupying && occupying.m_Bases)
		{
			foreach (OVT_BaseData baseData : occupying.m_Bases)
			{
				if (!baseData)
					continue;

				world.QueryEntitiesBySphere(baseData.location, HARVEST_RADIUS, AddHarvestedEntity, null, EQueryEntitiesFlags.STATIC);
				if (m_aHarvested.Count() >= SLOTS_NEEDED)
					return m_aHarvested;
			}
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns)
		{
			foreach (OVT_TownData town : towns.m_Towns)
			{
				if (!town)
					continue;

				world.QueryEntitiesBySphere(town.location, HARVEST_RADIUS, AddHarvestedEntity, null, EQueryEntitiesFlags.STATIC);
				if (m_aHarvested.Count() >= SLOTS_NEEDED)
					return m_aHarvested;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from a harvest query.
	//! \return True, to keep the query running until the list is long enough.
	protected bool AddHarvestedEntity(IEntity entity)
	{
		if (m_aHarvested.Count() >= SLOTS_NEEDED)
			return false;

		if (entity)
			AddHarvested(entity.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds an id when it is valid and not already held. Distinctness is what the whole case rests on.
	//! \param[in] id The candidate id.
	protected void AddHarvested(EntityID id)
	{
		if (id == EntityID.INVALID)
			return;

		if (m_aHarvested.Contains(id))
			return;

		if (m_aHarvested.Count() >= SLOTS_NEEDED)
			return;

		m_aHarvested.Insert(id);
	}
}

//------------------------------------------------------------------------------------------------
//! Test-only window onto OVT_PatrolBehaviorDeploymentModule's two protected anchor inputs.
//!
//! Both answer differently for a LIVE deployment than for a config template: GetPatrolCenter()
//! returns the deployment marker when there is a deployment and vector.Zero when there is not, and
//! GroupsAreStationedDeliberately() walks the deployment's spawning modules. So a case built on a
//! config template - how every other plan-shape case in this suite works - sees the template answers
//! and cannot distinguish the marker anchor from the group anchor at all. Creating a real deployment
//! would leak a repeating 8-12 s UpdateDeployment into this shared world.
//!
//! ⚠ NOT [BaseContainerProps]. It must never appear in a Workbench config picker.
//------------------------------------------------------------------------------------------------
class OVT_TEST_DefendAnchorProbeModule : OVT_PatrolBehaviorDeploymentModule
{
	//! Stands in for the live deployment marker GetPatrolCenter() would answer.
	vector m_vProbeCentre;

	//! Stands in for "this deployment's spawning module chose where its groups stand".
	bool m_bProbeStationed;

	//------------------------------------------------------------------------------------------------
	override protected vector GetPatrolCenter()
	{
		return m_vProbeCentre;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool GroupsAreStationedDeliberately()
	{
		return m_bProbeStationed;
	}
}

//------------------------------------------------------------------------------------------------
//! A DEFEND PLAN HOLDS WHERE THE GROUP WAS STATIONED - AND ONLY WHEN THAT POSITION WAS CHOSEN.
//!
//! Every base-defense config that stands men somewhere specific is teleported into place by its
//! spawning module and then told by this plan where to hold. Anchor the plan on the deployment MARKER
//! and the whole garrison walks to the base flag the moment it materialises, undoing the placement
//! that is the entire point of those configs.
//!
//! ⚠ But the opposite anchor is also wrong, for a different config. The plain infantry module rolls a
//! ring point and road-snaps it through a 500 m search that ignores m_fSpawnRadius - integration
//! measured a tower garrison landing on its access road - so anchoring ITS defend point on the group
//! would park that garrison on the road for good. The anchor therefore follows the spawning module's
//! own StationsGroupsDeliberately(), and both directions are asserted:
//!   1. STATIONED DELIBERATELY (placed / composition modules) -> hold the GROUP position;
//!   2. ROLLED AND SNAPPED (the plain infantry module) -> hold the MARKER;
//!   3. NO DEPLOYMENT AT ALL (a config template) -> hold the group position. ⚠ This one is a
//!      regression guard with NO independent fail proof: with no deployment the centre fallback also
//!      resolves to the group position, so both branches agree and no single edit can break it. It is
//!      here to catch a future change that removes the fallback or starts answering a marker for a
//!      template;
//!   4. PERIMETER IS UNTOUCHED - still a multi-point cycling plan. Its centre-dependence is
//!      deliberately NOT asserted: SnapPatrolPointsToRoads moves every corner onto a road up to 500 m
//!      away, so any exact geometric claim about a perimeter ring is a flake waiting to happen.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_DefendPlansHoldTheStation : SCR_AutotestCaseBase
{
	//! Stands in for a deployment marker - a base flag, a radio tower.
	static const vector MARKER = "1000 20 1000";

	//! Stands in for where a group actually is: a defend position, a bunker, or a road the snap chose.
	//! Far enough from MARKER that no rounding could confuse the two.
	static const vector STATION = "1300 35 1150";

	//! Float slack. Both positions are hundreds of metres apart, so this is generous by a wide margin.
	static const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = VerifyStationedHoldsTheGroup();

		if (failure == "")
			failure = VerifyRolledHoldsTheMarker();

		if (failure == "")
			failure = VerifyTemplateHoldsTheGroup();

		if (failure == "")
			failure = VerifyPerimeterStillBuildsARing();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("A deliberately stationed group holds ITS OWN position; a rolled-and-snapped one still holds the deployment marker; PERIMETER is unchanged");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 1 - the headline. A placed or composition module chose this position, so it is a post.
	//! \return An empty string when the claim holds, or the failure.
	protected string VerifyStationedHoldsTheGroup()
	{
		OVT_VirtualWaypointPlan plan = BuildDefend(MARKER, true);

		if (!plan)
			return "A DEFEND module built no plan at all for a deliberately stationed group - its guards would be registered with no waypoint and the config's whole behaviour would be silently missing";

		string shape = VerifyDefendShape(plan);
		if (shape != "")
			return shape;

		float offset = vector.Distance(plan.m_aPositions[0], STATION);
		if (offset > EPSILON)
			return string.Format("A deliberately stationed group's DEFEND point is %1 m from where it stands (%2 instead of %3) - every defense-position, tower-guard and checkpoint garrison would walk that far off its post towards the deployment marker the moment it materialised",
				offset.ToString(), plan.m_aPositions[0].ToString(), STATION.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 2 - the other direction, and the one that protects a SHIPPED, FROZEN config. The plain
	//! infantry module's position is a ring roll that has been through a 500 m road snap; the marker is
	//! the only trustworthy point.
	//! \return An empty string when the claim holds, or the failure.
	protected string VerifyRolledHoldsTheMarker()
	{
		OVT_VirtualWaypointPlan plan = BuildDefend(MARKER, false);

		if (!plan)
			return "A DEFEND module built no plan at all for a rolled registration";

		string shape = VerifyDefendShape(plan);
		if (shape != "")
			return shape;

		float offset = vector.Distance(plan.m_aPositions[0], MARKER);
		if (offset > EPSILON)
			return string.Format("A rolled-and-road-snapped group's DEFEND point is %1 m from the deployment marker (%2 instead of %3) - Deployment_TowerGarrison.conf registers exactly this way, and its garrison would hold whatever road the 500 m snap dropped it on instead of its tower",
				offset.ToString(), plan.m_aPositions[0].ToString(), MARKER.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 3 - the config-template path, which every other plan-shape case in this suite resolves
	//! through. An unset centre has always meant "hold where you are", and that must not change.
	//!
	//! ⚠ A REGRESSION GUARD, NOT AN INDEPENDENT CLAIM: with no deployment the centre fallback resolves
	//! to the group position too, so both anchors agree here by construction. See the case header.
	//! \return An empty string when the claim holds, or the failure.
	protected string VerifyTemplateHoldsTheGroup()
	{
		// vector.Zero is what GetPatrolCenter() answers with no deployment behind it.
		OVT_VirtualWaypointPlan plan = BuildDefend(vector.Zero, true);

		if (!plan)
			return "A DEFEND module built no plan at all off a config template - every template-based plan-shape case in this suite depends on it answering";

		string shape = VerifyDefendShape(plan);
		if (shape != "")
			return shape;

		float offset = vector.Distance(plan.m_aPositions[0], STATION);
		if (offset > EPSILON)
			return string.Format("Off a config template the DEFEND point is %1 m from the group position - the template fallback has always meant 'hold where you are', and the shipped plan-shape cases assert against it",
				offset.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! CLAIM 4 - PERIMETER did not come along for the ride. Structure only, deliberately: see the case
	//! header for why a geometric claim about a road-snapped ring would be a flake.
	//! \return An empty string when the claim holds, or the failure.
	protected string VerifyPerimeterStillBuildsARing()
	{
		OVT_TEST_DefendAnchorProbeModule probe = new OVT_TEST_DefendAnchorProbeModule();
		probe.m_ePatrolType = OVT_PatrolType.PERIMETER;
		probe.m_fPatrolRadius = 200;
		probe.m_bUseNearestTownCenter = false;
		probe.m_vProbeCentre = MARKER;
		probe.m_bProbeStationed = true;

		OVT_VirtualWaypointPlan plan = probe.BuildVirtualPlan(STATION);

		if (!plan)
			return "PERIMETER built no plan - a town patrol would be registered with no waypoints and would stand still forever";

		if (plan.m_aPositions.Count() <= 1)
			return string.Format("PERIMETER built a %1-point plan - it has been routed through the DEFEND branch, and every patrol in the campaign would hold one spot instead of circling",
				plan.m_aPositions.Count().ToString());

		if (!plan.m_bCycle)
			return "PERIMETER built a NON-CYCLING plan - a patrol that stops at its last corner guards one quarter of its area for the rest of the campaign";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Everything a DEFEND plan must be, whatever it is anchored on. A ragged plan is refused outright
	//! by RegisterGroup, so the guards would silently never be registered at all.
	//! \param[in] plan The plan to check.
	//! \return An empty string when the shape is right, or the failure.
	protected string VerifyDefendShape(notnull OVT_VirtualWaypointPlan plan)
	{
		int count = plan.m_aPositions.Count();

		if (count != 1)
			return string.Format("A DEFEND plan carries %1 points, expected exactly one", count.ToString());

		if (plan.m_aTypes.Count() != count || plan.m_aParams.Count() != count)
			return string.Format("A DEFEND plan is ragged (%1 positions, %2 types, %3 params) - RegisterGroup refuses a ragged plan outright",
				count.ToString(), plan.m_aTypes.Count().ToString(), plan.m_aParams.Count().ToString());

		if (plan.m_aTypes[0] != OVT_EVirtualWaypointType.DEFEND)
			return string.Format("A DEFEND plan's only point is type %1 - any movable type here hands the movement tick a garrison to walk away while nobody is watching",
				plan.m_aTypes[0].ToString());

		if (plan.m_bCycle)
			return "A DEFEND plan CYCLES - a one-point cycle is still a cycle";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a DEFEND plan for a group standing at STATION, with the two live inputs forced.
	//! \param[in] centre What GetPatrolCenter() should answer (vector.Zero = no deployment).
	//! \param[in] stationed Whether the spawning module chose the group's position.
	//! \return The plan, or null.
	protected OVT_VirtualWaypointPlan BuildDefend(vector centre, bool stationed)
	{
		OVT_TEST_DefendAnchorProbeModule probe = new OVT_TEST_DefendAnchorProbeModule();
		probe.m_ePatrolType = OVT_PatrolType.DEFEND;
		probe.m_fPatrolRadius = 0;
		probe.m_bUseNearestTownCenter = false;
		probe.m_vProbeCentre = centre;
		probe.m_bProbeStationed = stationed;

		return probe.BuildVirtualPlan(STATION);
	}
}

//------------------------------------------------------------------------------------------------
//! EVERY RESOURCE THE OCCUPYING FACTION SPENDS ON DEFENSE ARRIVES IN THE DEPLOYMENT POOL, AND THE
//! TRANSFER THAT PUTS IT THERE CONSERVES THE TOTAL.
//!
//! Base defense used to be funded by a SECOND spender: the campaign tick split 80 % of every gain
//! across the bases by threat and each base controller converted its share into men directly. Both
//! are deleted and the economy now runs through one credit point. Two silent failures:
//!   1. THE MONEY NEVER ARRIVES. Credit the reserve instead of the pool - the obvious mistake, since
//!      NewGameStart() sets the reserve two lines earlier - and the evaluator starts every campaign
//!      broke while both numbers look healthy in the Game Master panel.
//!   2. THE MONEY IS INVENTED OR LOST. A transfer that credits without debiting doubles the faction's
//!      income forever; one that debits without crediting starves defense. Neither is visible from
//!      any player or GM surface, which is why this asserts an EQUALITY on the sum.
//!
//! Three claims on the live managers: (a) the opening seed lands in the POOL and leaves the reserve
//! untouched; (b) a tick's transfer moves exactly the funding split's share and reserve+pool is
//! unchanged; (c) a reserve smaller than the share is clamped to what exists and the identity still
//! holds - the degenerate state a campaign reaches after an expensive QRF.
//!
//! ⚠ The reserve and the pool are borrowed and handed back exactly as found, inside ONE Execute()
//! frame so no evaluation pass, campaign tick or QRF can observe the planted values.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_DefenseFundingLandsInThePool : SCR_AutotestCaseBase
{
	//! Planted into the reserve. Deliberately not a value any campaign start produces, and comfortably
	//! larger than the share of the tick below, so claim (b) is not accidentally testing the clamp.
	static const int PLANTED_RESERVE = 4137;

	//! Planted into the deployment pool, for the same reason.
	static const int PLANTED_POOL = 913;

	//! A resource tick the size the shipped Normal difficulty produces on a quiet day.
	static const int TICK = 250;

	//! The reserve claim (c) is run against - smaller than the tick's share, so the transfer has to
	//! clamp to it.
	static const int STARVED_RESERVE = 7;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null, so there is no pool for defense funding to land in");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int factionIndex = config.GetOccupyingFactionIndex();
		if (factionIndex < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so its pool cannot be addressed");
			return true;
		}

		// BORROWED STATE, both halves.
		int originalReserve = occupying.m_iResources;
		int originalPool = manager.GetFactionResources(factionIndex);

		string failure = RunClaims(occupying, manager, factionIndex);

		// TEARDOWN BEFORE REPORTING, ON EVERY PATH.
		occupying.m_iResources = originalReserve;
		RestorePool(manager, factionIndex, originalPool);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Defense funding: the opening seed landed in the deployment pool with the reserve untouched, a tick of %1 moved exactly %2 across, and reserve+pool was conserved through both - including out of a starved reserve",
			TICK.ToString(), OVT_BaseDefenseConversion.DefenseShare(TICK).ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The three claims, run against the live managers with the borrowed state planted.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] manager The deployment manager holding the pool.
	//! \param[in] factionIndex The occupying faction's index.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string RunClaims(notnull OVT_OccupyingFactionManager occupying, notnull OVT_DeploymentManagerComponent manager, int factionIndex)
	{
		// ---- (a) THE OPENING SEED LANDS IN THE POOL ----------------------------------------------
		occupying.m_iResources = PLANTED_RESERVE;
		RestorePool(manager, factionIndex, PLANTED_POOL);

		int seed = occupying.CalculateOpeningDeploymentSeed();
		if (seed <= 0)
			return string.Format("The opening defense budget computes to %1 - this world has no difficulty settings or no base controllers, so 'the seed lands in the pool' would assert nothing",
				seed.ToString());

		occupying.SeedOpeningDeploymentResources();

		int poolAfterSeed = manager.GetFactionResources(factionIndex);
		if (poolAfterSeed != PLANTED_POOL + seed)
			return string.Format("The opening budget of %1 left the deployment pool at %2, expected %3 - the money a campaign starts its defense with is not reaching the only budget defense is bought from",
				seed.ToString(), poolAfterSeed.ToString(), (PLANTED_POOL + seed).ToString());

		if (occupying.m_iResources != PLANTED_RESERVE)
			return string.Format("The opening budget moved the occupying faction's reserve to %1, expected it untouched at %2 - the seed is being credited to the QRF reserve instead of, or as well as, the pool",
				occupying.m_iResources.ToString(), PLANTED_RESERVE.ToString());

		// ---- (b) A TICK'S TRANSFER MOVES THE SHARE, AND ONLY THE SHARE ---------------------------
		int reserveBefore = occupying.m_iResources;
		int poolBefore = manager.GetFactionResources(factionIndex);
		int expectedShare = OVT_BaseDefenseConversion.DefenseShare(TICK);

		if (expectedShare <= 0 || expectedShare >= reserveBefore)
			return string.Format("A tick of %1 splits to %2, which is not a share this case can tell apart from the clamp - pick a different planted reserve",
				TICK.ToString(), expectedShare.ToString());

		occupying.TransferDefenseShareToPool(TICK);

		int reserveAfter = occupying.m_iResources;
		int poolAfter = manager.GetFactionResources(factionIndex);

		if (poolAfter - poolBefore != expectedShare)
			return string.Format("The transfer moved %1 into the deployment pool, expected %2 - the funding split and the transfer disagree about what a tick is worth",
				(poolAfter - poolBefore).ToString(), expectedShare.ToString());

		if (reserveBefore - reserveAfter != expectedShare)
			return string.Format("The transfer took %1 out of the reserve while putting %2 into the pool - resources are being created or destroyed on every campaign tick",
				(reserveBefore - reserveAfter).ToString(), expectedShare.ToString());

		if (reserveAfter + poolAfter != reserveBefore + poolBefore)
			return string.Format("Reserve+pool moved from %1 to %2 across one transfer - the conserved-total identity the single funding path exists to guarantee does not hold",
				(reserveBefore + poolBefore).ToString(), (reserveAfter + poolAfter).ToString());

		// ---- (c) A STARVED RESERVE IS CLAMPED, AND THE IDENTITY STILL HOLDS -----------------------
		occupying.m_iResources = STARVED_RESERVE;
		poolBefore = manager.GetFactionResources(factionIndex);

		occupying.TransferDefenseShareToPool(TICK);

		reserveAfter = occupying.m_iResources;
		poolAfter = manager.GetFactionResources(factionIndex);

		if (reserveAfter != 0)
			return string.Format("Transferring out of a reserve of %1 left it at %2, expected 0 - a reserve that can go negative is a campaign that can never afford a QRF again",
				STARVED_RESERVE.ToString(), reserveAfter.ToString());

		if (poolAfter - poolBefore != STARVED_RESERVE)
			return string.Format("A starved reserve handed the pool %1, expected exactly the %2 it had - the transfer is crediting money that did not exist",
				(poolAfter - poolBefore).ToString(), STARVED_RESERVE.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a faction's pool to an exact value, whichever way it has to go.
	//! \param[in] manager The deployment manager.
	//! \param[in] factionIndex The faction whose pool is being set.
	//! \param[in] target The value to leave it on.
	protected void RestorePool(notnull OVT_DeploymentManagerComponent manager, int factionIndex, int target)
	{
		int current = manager.GetFactionResources(factionIndex);

		if (current > target)
			manager.SubtractFactionResources(factionIndex, current - target);
		else if (current < target)
			manager.AddFactionResources(factionIndex, target - current);
	}
}

//------------------------------------------------------------------------------------------------
//! The relaxed house-search waypoint is WIRED END TO END: the game-mode prefab authors it, it spawns
//! as a timed Search & Destroy waypoint, and all THREE hand-authored behaviour trees (waypoint,
//! soldier, move-to) are registered resources.
//!
//! ⚠ The trees are the claim. OVT_AIWaypoint_HouseSearch.et, WP_HouseSearch.bt and HouseSearch.bt
//! were written as TEXT with hand-minted GUIDs and a .meta whose resource class was read out of the
//! engine binary, not a Workbench save. If the database refuses either tree the waypoint still spawns
//! and the group still walks to the house - and then stands there until the hold expires, which in
//! play looks like "the search is broken" with nothing in the log naming the tree. Resource.Load on
//! the GUID'd name is the cheapest honest check that the registration took.
//!
//! The spawned waypoint is untracked and deleted on every path, so no persistence record outlives it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_HouseSearchWaypointResolves : SCR_AutotestCaseBase
{
	static const ResourceName WAYPOINT_TREE = "{A086847134FE94FF}AI/BehaviorTrees/Overthrow/Waypoints/WP_HouseSearch.bt";
	static const ResourceName SOLDIER_TREE = "{7ABD3B8D152B6DBA}AI/BehaviorTrees/Overthrow/Soldier/HouseSearch.bt";
	static const ResourceName MOVE_TO_TREE = "{ACFFFA96E11FDA0F}AI/BehaviorTrees/Overthrow/Waypoints/WP_HouseSearchMoveTo.bt";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		if (!config.m_pHouseSearchWaypointPrefab)
		{
			SetFailure("The game-mode prefab does not author m_pHouseSearchWaypointPrefab - every town sweep would fall back to the tactical vanilla Search & Destroy");
			return true;
		}

		if (OVT_AIHouseSearchBehavior.HOUSE_SEARCH_TREE != SOLDIER_TREE)
		{
			SetFailure("OVT_AIHouseSearchBehavior.HOUSE_SEARCH_TREE is '%1', not the registered soldier tree this case checks", OVT_AIHouseSearchBehavior.HOUSE_SEARCH_TREE);
			return true;
		}

		Resource waypointTree = Resource.Load(WAYPOINT_TREE);
		if (!waypointTree || !waypointTree.IsValid())
		{
			SetFailure("The waypoint tree '%1' does not load - the hand-authored .bt or its .meta is not a registered resource, and a house-search waypoint would never start its activity", WAYPOINT_TREE);
			return true;
		}

		Resource soldierTree = Resource.Load(SOLDIER_TREE);
		if (!soldierTree || !soldierTree.IsValid())
		{
			SetFailure("The soldier tree '%1' does not load - the hand-authored .bt or its .meta is not a registered resource, and every searching soldier would stand still", SOLDIER_TREE);
			return true;
		}

		Resource moveToTree = Resource.Load(MOVE_TO_TREE);
		if (!moveToTree || !moveToTree.IsValid())
		{
			SetFailure("The move-to tree '%1' does not load - the group would fall back to nothing for the leg between houses", MOVE_TO_TREE);
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		AIWaypoint waypoint = config.SpawnHouseSearchWaypoint(position);
		if (!waypoint)
		{
			SetFailure("SpawnHouseSearchWaypoint() spawned nothing from '%1'", config.m_pHouseSearchWaypointPrefab);
			return true;
		}

		string failure = "";
		if (!SCR_SearchAndDestroyWaypoint.Cast(waypoint))
			failure = "The house-search waypoint is not an SCR_SearchAndDestroyWaypoint - the activity's grid, holding time and completion all come from that class";

		SCR_TimedWaypoint timed = SCR_TimedWaypoint.Cast(waypoint);
		if (failure == "" && timed)
		{
			timed.SetHoldingTime(77);
			if (Math.AbsFloat(timed.GetHoldingTime() - 77) > 0.01)
				failure = "SetHoldingTime() did not take on the house-search waypoint - the prefab lost m_TimedWaypointParameters, so every search would run the prefab's default hold";
		}

		OVT_PersistenceManagerComponent.CancelUntrackTransient(waypoint);
		OVT_PersistenceTracking.Untrack(waypoint, false);
		SCR_EntityHelper.DeleteEntityAndChildren(waypoint);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("The house-search waypoint is authored on the game mode, spawns as a timed Search & Destroy waypoint, and all three Overthrow behaviour trees are registered resources");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE DRIP DELIVERS THE WHOLE DEFENSE SHARE ACROSS A WINDOW, AND NOTHING IS EVER STRANDED.
//!
//! The single transfer this replaced was safe because it was instantaneous: reserve down by X, pool
//! up by X, in one statement, with no state in between. The drip deliberately introduces exactly the
//! state that statement had none of - a debt the reserve owes the pool, outstanding for up to six
//! in-game hours - and every way that debt can be lost is silent in play. A window that pays five of
//! its six slices does not log anything; it just leaves the occupying faction unable to afford its
//! next base defense, hours later, for no visible reason.
//!
//! WHAT IS ASSERTED HERE AND NOT AT THE LOGIC TIER. OVT_TEST_Logic_BaseDefenseConversion_DripPaysOut-
//! ExactlyTheShare pins the ARITHMETIC - that six DripAmount calls sum to their share. This case pins
//! the WIRING: that arming actually moves nothing, that each drip debits the live reserve and credits
//! the live pool by the same number, that the six of them add up to what DefenseShare said, and that
//! the flush at the next payday settles a window abandoned half-paid. Those are four different things
//! and only the first is arithmetic.
//!
//! THE FLUSH CLAIM IS THE LOAD-BEARING ONE. CheckUpdate returns early for the whole of an engaged
//! QRF, a campaign can be loaded mid-window, and a sleep replay only drips on the hour boundaries it
//! walks - so windows WILL be abandoned part-paid in normal play. ArmDefenseShareDrip flushing first
//! is what turns every one of those from a money loss into a timing wobble.
//!
//! ⚠ The reserve and the pool are borrowed and handed back exactly as found, on every exit path,
//! inside ONE Execute() - the same contract as the funding case above.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Deployments_DefenseShareDripsIntoThePool : SCR_AutotestCaseBase
{
	//! Deep enough that no drip is ever clamped by it.
	static const int PLANTED_RESERVE = 9000;

	//! Distinguishable from zero, so a credit is never confused with a reset.
	static const int PLANTED_POOL = 777;

	//! A tick whose 80 % share (1069) divides by six with a remainder, so the carry is exercised.
	static const int TICK = 1337;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null, so there is no pool for the drip to land in");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int factionIndex = config.GetOccupyingFactionIndex();
		if (factionIndex < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so its pool cannot be addressed");
			return true;
		}

		// BORROWED STATE - the reserve, the pool AND the live drip window, which this case overwrites.
		int originalReserve = occupying.m_iResources;
		int originalPool = manager.GetFactionResources(factionIndex);
		int originalPending = occupying.GetPendingDefenseTransfer();
		int originalDrips = occupying.GetDefenseDripsRemaining();
		int originalMinute = occupying.GetDripMinute();

		string failure = RunClaims(occupying, manager, factionIndex);

		// TEARDOWN BEFORE REPORTING, ON EVERY PATH.
		occupying.m_iResources = originalReserve;
		RestorePool(manager, factionIndex, originalPool);
		occupying.ApplyPersistedDefenseDrip(originalPending, originalDrips, originalMinute);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Defense drip: a tick of %1 armed a debt of %2 without moving anything, six drips delivered exactly that to the pool with reserve+pool conserved at every step, and a window abandoned half-paid was settled in full by the next payday's flush",
			TICK.ToString(), OVT_BaseDefenseConversion.DefenseShare(TICK).ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The three claims, run against the live managers with the borrowed state planted.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] manager The deployment manager holding the pool.
	//! \param[in] factionIndex The occupying faction's index.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string RunClaims(notnull OVT_OccupyingFactionManager occupying, notnull OVT_DeploymentManagerComponent manager, int factionIndex)
	{
		int share = OVT_BaseDefenseConversion.DefenseShare(TICK);
		if (share <= OVT_BaseDefenseConversion.DRIP_STEPS)
			return string.Format("A tick of %1 splits to %2, which is not enough to tell a carried remainder from a truncated one - pick a bigger planted tick",
				TICK.ToString(), share.ToString());

		// ---- (a) ARMING MOVES NOTHING ------------------------------------------------------------
		// The whole point of the change: at the payday the money STAYS in the reserve. If arming
		// credited the pool, the drip would be decoration over the old burst behaviour.
		occupying.m_iResources = PLANTED_RESERVE;
		RestorePool(manager, factionIndex, PLANTED_POOL);
		occupying.ApplyPersistedDefenseDrip(0, 0, 0);

		occupying.ArmDefenseShareDrip(TICK);

		if (manager.GetFactionResources(factionIndex) != PLANTED_POOL)
			return string.Format("Arming the drip moved the pool to %1, expected it untouched at %2 - the share is still crossing at the payday and nothing has actually been smoothed",
				manager.GetFactionResources(factionIndex).ToString(), PLANTED_POOL.ToString());

		if (occupying.m_iResources != PLANTED_RESERVE)
			return string.Format("Arming the drip moved the reserve to %1, expected it untouched at %2 - the money is meant to stay in the reserve until each slice is actually paid",
				occupying.m_iResources.ToString(), PLANTED_RESERVE.ToString());

		if (occupying.GetPendingDefenseTransfer() != share)
			return string.Format("Arming a tick of %1 left a debt of %2, expected %3 - the drip window and the funding split disagree about what a tick is worth",
				TICK.ToString(), occupying.GetPendingDefenseTransfer().ToString(), share.ToString());

		if (occupying.GetDefenseDripsRemaining() != OVT_BaseDefenseConversion.DRIP_STEPS)
			return string.Format("Arming left %1 drips to pay the debt with, expected %2",
				occupying.GetDefenseDripsRemaining().ToString(), OVT_BaseDefenseConversion.DRIP_STEPS.ToString());

		// ---- (b) SIX DRIPS DELIVER EXACTLY THE SHARE, CONSERVING THE TOTAL AT EVERY STEP ---------
		int delivered = 0;

		for (int i = 0; i < OVT_BaseDefenseConversion.DRIP_STEPS; i++)
		{
			int reserveBefore = occupying.m_iResources;
			int poolBefore = manager.GetFactionResources(factionIndex);

			int moved = occupying.DripDefenseShare();

			int reserveAfter = occupying.m_iResources;
			int poolAfter = manager.GetFactionResources(factionIndex);

			if (poolAfter - poolBefore != moved)
				return string.Format("Drip %1 reported moving %2 but the pool rose by %3 - the drip and the credit disagree",
					(i + 1).ToString(), moved.ToString(), (poolAfter - poolBefore).ToString());

			if (reserveBefore - reserveAfter != moved)
				return string.Format("Drip %1 reported moving %2 but the reserve fell by %3 - resources are being created or destroyed one slice at a time",
					(i + 1).ToString(), moved.ToString(), (reserveBefore - reserveAfter).ToString());

			if (reserveAfter + poolAfter != reserveBefore + poolBefore)
				return string.Format("Reserve+pool moved from %1 to %2 across drip %3 - the conserved-total identity does not survive being paid in slices",
					(reserveBefore + poolBefore).ToString(), (reserveAfter + poolAfter).ToString(), (i + 1).ToString());

			delivered += moved;
		}

		if (delivered != share)
			return string.Format("Six drips delivered %1 of a %2 share - the remainder carry is losing or conjuring money, and a pool that is quietly short funds no defenses with nothing logged",
				delivered.ToString(), share.ToString());

		if (occupying.GetPendingDefenseTransfer() != 0)
			return string.Format("A fully dripped window still owes %1 - the debt outlives the window that created it and the next payday's flush will pay it twice",
				occupying.GetPendingDefenseTransfer().ToString());

		// ---- (c) AN ABANDONED WINDOW IS SETTLED BY THE NEXT PAYDAY'S FLUSH ------------------------
		// A QRF freeze, a mid-window load or a sleep replay all abandon windows part-paid. Arming the
		// next one must pay off what the last one still owed FIRST, or the loss is permanent, silent,
		// and only visible as an occupying faction that stopped defending itself.
		occupying.m_iResources = PLANTED_RESERVE;
		RestorePool(manager, factionIndex, PLANTED_POOL);
		occupying.ApplyPersistedDefenseDrip(0, 0, 0);

		occupying.ArmDefenseShareDrip(TICK);
		occupying.DripDefenseShare();
		occupying.DripDefenseShare();

		int owed = occupying.GetPendingDefenseTransfer();
		if (owed <= 0)
			return "Two of six drips left nothing owed, so there is no abandoned window to settle - the drip is paying the whole share on its first slice";

		int poolBeforeFlush = manager.GetFactionResources(factionIndex);

		// The next payday. Its own share arms a fresh window; the abandoned one must be settled here.
		occupying.ArmDefenseShareDrip(TICK);

		int credited = manager.GetFactionResources(factionIndex) - poolBeforeFlush;
		if (credited != owed)
			return string.Format("The next payday credited %1 while the abandoned window owed %2 - a window that loses drips to a QRF freeze, a load or a sleep replay loses the money for good",
				credited.ToString(), owed.ToString());

		if (occupying.GetPendingDefenseTransfer() != share)
			return string.Format("After the flush the new window owes %1, expected %2 - the flush and the arm are treading on each other",
				occupying.GetPendingDefenseTransfer().ToString(), share.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a faction's pool to an exact value, whichever way it has to go.
	//! \param[in] manager The deployment manager.
	//! \param[in] factionIndex The faction whose pool is being set.
	//! \param[in] target The value to leave it on.
	protected void RestorePool(notnull OVT_DeploymentManagerComponent manager, int factionIndex, int target)
	{
		int current = manager.GetFactionResources(factionIndex);

		if (current > target)
			manager.SubtractFactionResources(factionIndex, current - target);
		else if (current < target)
			manager.AddFactionResources(factionIndex, target - current);
	}
}

//------------------------------------------------------------------------------------------------
//! A LAND-ISOLATED TARGET IS NEVER COLLECTED AS AN OBJECTIVE CANDIDATE.
//!
//! Erquy Harbour sits on an island. Nothing in the objective machinery knew that until this case's
//! feature: OVT_ObjectiveSelection.ProximityScore is straight-line distance, and Erquy's nearest
//! occupying holding is about 4.7 km away - INSIDE the director's 5 km m_fMaxUsefulDistance - so it
//! scored as an ordinary, slightly distant prize and was selectable like any other base.
//!
//! WHAT PICKING IT ACTUALLY DID, which is why a gate and not a score penalty: a penalty does not
//! exclude anything, because the director picks the BEST candidate it has and on a quiet map a
//! heavily penalised island is still the best thing going. And once picked, the harassment insertion
//! spawns a truck at a mainland source and drives it at the objective; it strands at the coast, the
//! stranded-transport path opens the doors, and the passengers march on a target across open water.
//! The forward base is then sited between the nearest holding and the target, which across a strait
//! is the sea. Neither path has any water rule - every IsOceanAtPosition call in the tree asks
//! whether a POINT is wet, and none asks whether a point can be WALKED TO.
//!
//! ⚠ THE FLAG IS PLANTED, NOT READ OFF THE MAP. This suite runs in OVT_Campaign_Test.ent, which has
//! one nameless base and no island; asserting that some base carries the authored attribute would be
//! asserting a fact about a world this tier never loads. So the case plants the flag on the live
//! record, drives the real collection, and hands the record back - which tests the GATE, in any
//! world, and is the half that a code change can break.
//!
//! ⚠ THE OTHER HALF - that bases.layer still CARRIES m_bLandIsolated - is checked by
//! tools/check-land-isolated.py, because nothing in the autotests can see the Eden layer and a
//! Workbench re-save drops an authored attribute silently. A dropped attribute leaves a campaign
//! that compiles, loads, plays, and marches a fireteam into the sea an hour later.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Objectives_LandIsolatedTargetsAreNeverCandidates : SCR_AutotestCaseBase
{
	//! Comfortably past any world, so no candidate is dropped for mere distance.
	static const float MAX_USEFUL_DISTANCE = 50000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		if (!occupying.m_Bases || occupying.m_Bases.IsEmpty())
		{
			SetFailure("The world carries no base controllers, so there is nothing to make a candidate of and this case would assert nothing");
			return true;
		}

		OVT_BaseData subject = occupying.m_Bases[0];

		// BORROWED STATE - the record is the live one, handed back on every path below.
		int originalFaction = subject.faction;
		bool originalIsolated = subject.landIsolated;

		int resistanceIndex = config.GetPlayerFactionIndex();
		int sources = OVT_EObjectiveCandidateSource.RESISTANCE_TOWNS | OVT_EObjectiveCandidateSource.RESISTANCE_BASES;

		// A base is only ever a candidate while the RESISTANCE holds it.
		subject.faction = resistanceIndex;

		string failure = RunClaims(occupying, subject, sources, resistanceIndex);

		subject.faction = originalFaction;
		subject.landIsolated = originalIsolated;

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Land-isolated gate: a reachable base was collected as an objective candidate and the same base, flagged land-isolated, was not");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Collects twice - once reachable, once isolated - so the absence proves the FLAG and not some
	//! unrelated reason the base was never a candidate in this world at all.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] subject The base record being flipped.
	//! \param[in] sources The candidate sources to collect.
	//! \param[in] resistanceIndex The resistance faction index.
	//! eturn An empty string when both claims hold, or the broken one.
	protected string RunClaims(notnull OVT_OccupyingFactionManager occupying, notnull OVT_BaseData subject, int sources, int resistanceIndex)
	{
		// ---- (a) THE CONTROL: reachable, so it IS collected --------------------------------------
		subject.landIsolated = false;

		OVT_ObjectiveCandidateSet reachable = new OVT_ObjectiveCandidateSet();
		reachable.Collect(occupying, OVT_Global.GetTowns(), sources, resistanceIndex, MAX_USEFUL_DISTANCE);

		if (!IsCollected(reachable, subject.location))
			return "A resistance-held base that is NOT land-isolated was not collected as a candidate either - so this world cannot tell the gate apart from a base that was never eligible, and the exclusion below would prove nothing";

		// ---- (b) THE CLAIM: isolated, so it is NOT collected --------------------------------------
		subject.landIsolated = true;

		OVT_ObjectiveCandidateSet isolated = new OVT_ObjectiveCandidateSet();
		isolated.Collect(occupying, OVT_Global.GetTowns(), sources, resistanceIndex, MAX_USEFUL_DISTANCE);

		if (IsCollected(isolated, subject.location))
			return "A base flagged land-isolated was still collected as an objective candidate. The occupying faction can pick it, and everything downstream assumes a land route: the harassment truck strands at the coast and marches its passengers at open water, and the forward base is sited in the sea";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position turned up among the collected candidates.
	//!
	//! MATCHED BY POSITION, because the set stores a display name and a position but not the record.
	//! A candidate is collected AT base.location, so this is an identity check, not a proximity search.
	//! \param[in] candidates The collected set.
	//! \param[in] position The base's location.
	//! eturn True when the set carries a candidate there.
	protected bool IsCollected(notnull OVT_ObjectiveCandidateSet candidates, vector position)
	{
		int count = candidates.Count();
		for (int i = 0; i < count; i++)
		{
			if (vector.Distance(candidates.GetPosition(i), position) < 1)
				return true;
		}

		return false;
	}
}
