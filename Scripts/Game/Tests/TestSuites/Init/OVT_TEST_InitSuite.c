//------------------------------------------------------------------------------------------------
//! TIER B - initialisation / integration.
//!
//! World loaded, Overthrow game mode and its managers alive, campaign deliberately NOT started
//! (RequiresStartedCampaign() stays false). This is the cheapest possible guard against the
//! "everything is null on startup" class of breakage: a manager dropped from the game mode prefab,
//! a renamed OVT_Global getter, a town controller that no longer registers, an economy map that
//! stops answering its own setters.
//!
//! What belongs here: anything that needs live managers but no running campaign.
//! What does NOT belong here: pure maths with no manager (Tier A, OVT_TEST_LogicSuite) and anything
//! that needs the campaign started (Tier C/D, which override RequiresStartedCampaign()).
//!
//! No magic counts: the test world (Worlds/MP/OVT_Campaign_Test.ent) defines exactly ONE town and
//! ONE base, so every count assertion here is ">= 1". An Eden-sized expectation would be red on
//! arrival and would say nothing about Overthrow.
//!
//! [BaseContainerProps()] is MANDATORY on a concrete suite class: without it a SCR_AutotestGroup
//! config silently instantiates nothing (`Unknown class`, empty <testsuites>, exit 2) - findings.md
//! 1.10. Feature #3 Phase 6 puts this suite in the Fast and All groups.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TEST_InitSuite : OVT_TEST_SuiteBase
{
}

//------------------------------------------------------------------------------------------------
//! Every OVT_Global manager getter that does not need a local player returns a live object.
//!
//! Deliberately EXCLUDES GetServer(), GetUI(), GetController() and GetContainerTransfer(): all four
//! dereference SCR_PlayerController.GetLocalControlledEntity(), which is not what this tier is
//! about, and GetServer()/GetUI() do it without a null check.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Globals_ManagersResolve : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		string firstNull = FindFirstNullGetter();

		if (firstNull != "")
		{
			SetResultFailure("OVT_Global getter returned null: %1 - the game mode is missing that manager component", firstNull);
			return true;
		}

		Print("Every non-player OVT_Global getter resolved");
		SetResultSuccess();
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
		if (!OVT_Global.GetTutorialManager()) return "GetTutorialManager()";

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
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList)
		{
			SetResultFailure("OVT_TownManagerComponent.GetTowns() returned a null array");
			return true;
		}

		// >= 1, never a magic count - the test world has exactly one town.
		if (townList.Count() < 1)
		{
			SetResultFailure("No towns registered: OVT_TownManagerComponent.GetTowns().Count() = %1", townList.Count().ToString());
			return true;
		}

		OVT_TownData town = townList[0];
		if (!town)
		{
			SetResultFailure("Town 0 is null");
			return true;
		}

		if (town.population <= 0)
		{
			SetResultFailure("Town 0 has no population: %1", town.population.ToString());
			return true;
		}

		if (town.location == vector.Zero)
		{
			SetResultFailure("Town 0 has no location (vector.Zero)");
			return true;
		}

		PrintFormat("Towns registered: %1, town 0 population %2 at %3",
			townList.Count().ToString(), town.population.ToString(), town.location.ToString());

		SetResultSuccess();
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
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		if (towns.m_TownControllers.Count() < 1)
		{
			SetResultFailure("No town controllers registered: m_TownControllers.Count() = %1", towns.m_TownControllers.Count().ToString());
			return true;
		}

		IEntity townEntity = GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]);
		if (!townEntity)
		{
			SetResultFailure("Town controller 0 is registered but its entity ID no longer resolves in the world");
			return true;
		}

		OVT_TownControllerComponent townController = OVT_TownControllerComponent.Cast(townEntity.FindComponent(OVT_TownControllerComponent));
		if (!townController)
		{
			SetResultFailure("Registered town controller entity has no OVT_TownControllerComponent");
			return true;
		}

		// The controller must resolve back into the manager's own town list.
		OVT_TownData town = towns.GetNearestTown(townEntity.GetOrigin());
		if (!town)
		{
			SetResultFailure("GetNearestTown() found no town at the town controller's position %1", townEntity.GetOrigin().ToString());
			return true;
		}

		int townId = towns.GetTownID(town);
		if (townId < 0)
		{
			SetResultFailure("The town at the town controller's position is not in m_Towns (GetTownID returned %1)", townId.ToString());
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetResultFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		if (occupying.m_Bases.Count() < 1)
		{
			SetResultFailure("No bases registered: m_Bases.Count() = %1", occupying.m_Bases.Count().ToString());
			return true;
		}

		OVT_BaseControllerComponent baseController = occupying.GetBaseByIndex(0);
		if (!baseController)
		{
			SetResultFailure("Base 0 is registered but GetBaseByIndex(0) resolved no OVT_BaseControllerComponent");
			return true;
		}

		PrintFormat("Town controllers: %1, controller '%2' resolves to town id %3",
			towns.m_TownControllers.Count().ToString(), townController.m_sName, townId.ToString());
		PrintFormat("Bases registered: %1", occupying.m_Bases.Count().ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownManagerComponent.GetNearestTownInRange() returns the NEAREST in-range town, not the
//! first in array order (BUG-062 regression).
//!
//! Two synthetic villages with overlapping radii are appended to m_Towns, deliberately ordered so
//! that the FARTHER one comes first in the array - the exact configuration the old first-match loop
//! returned wrong on, which made every death-driven modifier credit the wrong town wherever radii
//! overlap. Both sit tens of kilometres from the test world's real town, so no real record can be
//! in range of either probe, and both are removed from m_Towns again before any assertion returns,
//! keeping the index-aligned town list intact for every later case.
//!
//! PROVEN ABLE TO FAIL: with GetNearestTownInRange() reverted to its pre-fix first-match body
//! (`if(distance <= range) return town;`), this case goes red on the "returned the farther town"
//! assertion.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Towns_GetNearestTownInRange_ReturnsNearest : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
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
			SetResultFailure("GetNearestTownInRange() returned the farther town because it precedes the nearer one in m_Towns (BUG-062 first-match regression)");
			return true;
		}

		if (overlapResult != nearTown)
		{
			SetResultFailure("GetNearestTownInRange() did not return the nearer of two overlapping in-range towns");
			return true;
		}

		if (outOfRangeResult)
		{
			SetResultFailure("GetNearestTownInRange() returned a town for a probe outside every town's range");
			return true;
		}

		PrintFormat("GetNearestTownInRange: nearest of 2 overlapping villages returned, out-of-range probe returned null (village range %1)", range.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Reforger's vanilla persistence system is registered for the Overthrow world and did not fail
//! its own setup.
//!
//! Two independent failure modes, one case, because they are the same wiring:
//!  1. GetScriptedInstance() null - Configs/Systems/ChimeraSystemsConfig.conf lost its
//!     SCR_PersistenceSystem entry, or the world's SystemSettings chain stopped passing through it.
//!     Nothing persistence-related can work; every save is a silent no-op.
//!  2. State FAILURE - the system exists but Configs/Systems/Persistence/Overthrow.conf could not be
//!     loaded (bad GUID reference, malformed conf). Hand-authored conf GUIDs fail SILENTLY in the
//!     Workbench, so this is the only automated tripwire the migration has for that class of typo.
//!
//! ACTIVE is asserted rather than merely "not FAILURE" because INIT/SETUP are world-load states:
//! vanilla treats "below ACTIVE" as "load still in progress" (SCR_PersistenceSystem.IsLoadInProgress,
//! and SCR_SpawnLogic gives up on persistent player data when the state is not ACTIVE), and Main
//! steps only run after Setup_AwaitWorld has completed the world load. The failure message names
//! the observed state, so a build that turns setup asynchronous reports exactly what it reached
//! instead of hiding behind a weaker assertion - if that ever happens, loosen this deliberately
//! (to "not FAILURE") and record why. Never add retries.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_SystemIsOnline : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetResultFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - the persistence system is not registered for this world. Check the SCR_PersistenceSystem entry in Configs/Systems/ChimeraSystemsConfig.conf.");
			return true;
		}

		EPersistenceSystemState state = persistence.GetState();

		if (state == EPersistenceSystemState.FAILURE)
		{
			SetResultFailure("Persistence system state is FAILURE - its config could not be loaded. Check Configs/Systems/Persistence/Overthrow.conf and the GUID it inherits.");
			return true;
		}

		if (state != EPersistenceSystemState.ACTIVE)
		{
			SetResultFailure("Persistence system state is %1, expected ACTIVE. INIT and SETUP mean the world load has not finished setting persistence up.",
				typename.EnumToString(EPersistenceSystemState, state));
			return true;
		}

		PrintFormat("Persistence system online, state %1", typename.EnumToString(EPersistenceSystemState, state));
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The persistence system that is online is running OVERTHROW's persistence config, not somebody
//! else's.
//!
//! Separate from the case above on purpose. "A persistence system exists" and "our config is the one
//! in force" are different claims with different causes, and only the second one tells us that
//! Configs/Systems/Persistence/Overthrow.conf was actually resolved through the
//! Configs/Systems/ChimeraSystemsConfig.conf entry. Several vanilla SystemSettings configs
//! (MissionSystems.conf) register an SCR_PersistenceSystem of their own pointing at
//! EditableMission.conf; if a world's chain resolves one of those instead, the case above still
//! passes while every Overthrow collection and serializer binding silently does not exist.
//!
//! The probe is the "Overthrow" collection that Overthrow.conf adds on top of vanilla Common.conf -
//! looked up by display name, the same way SCR_SpawnLogic looks up "Player"/"Character".
//! If this goes red while the case above is green, the config is not in force: nothing about the
//! script layer will fix it, the SystemSettings chain is what has to change.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_OverthrowConfigLoaded : SCR_AutotestCaseBase
{
	//! Display name of the collection declared in Configs/Systems/Persistence/Overthrow.conf.
	static const string OVERTHROW_COLLECTION = "Overthrow";

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetResultFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - see OVT_TEST_Init_Persistence_SystemIsOnline for the wiring this depends on.");
			return true;
		}

		// Sanity anchor: an inherited vanilla collection must resolve, otherwise a null result below
		// would only prove that collection lookup itself is broken.
		PersistenceCollection vanillaCollection = persistence.FindCollection("Character");
		if (!vanillaCollection)
		{
			SetResultFailure("FindCollection('Character') is null - the loaded persistence config does not even contain vanilla Common.conf's collections, so no config comparison is meaningful.");
			return true;
		}

		PersistenceCollection overthrowCollection = persistence.FindCollection(OVERTHROW_COLLECTION);
		if (!overthrowCollection)
		{
			SetResultFailure("FindCollection('%1') is null - the live persistence system is NOT running Configs/Systems/Persistence/Overthrow.conf. Check that the SCR_PersistenceSystem entry in Configs/Systems/ChimeraSystemsConfig.conf is the one this world's SystemSettings chain resolves.", OVERTHROW_COLLECTION);
			return true;
		}

		PrintFormat("Overthrow persistence config in force (collection '%1' resolved)", OVERTHROW_COLLECTION);
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The economy's price and demand seams answer their own public setters, and GetBuyPrice() applies
//! the configured shop profit margin on top of the base price.
//!
//! Uses synthetic resource IDs. Real IDs are indices into OVT_EconomyManagerComponent.m_aResources
//! (see GetInventoryId), so an ID far outside that range exercises the int-keyed maps without
//! disturbing any real item's price for the rest of the session.
//!
//! The expected buy price is derived from the config's m_fShopProfitMargin rather than hardcoded,
//! so a deliberate config change does not silently invalidate this case - it only changes the
//! number both sides compute. The independent claim is the second assertion: a shop's buy price is
//! always strictly above the base price.
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
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetResultFailure("OVT_Global.GetEconomy() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetResultFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		// Documented fallbacks for an ID the maps have never seen.
		int unknownPrice = economy.GetPrice(PROBE_UNKNOWN_ITEM_ID);
		if (unknownPrice != 500)
		{
			SetResultFailure("GetPrice() on an unknown id returned %1, expected the documented default 500", unknownPrice.ToString());
			return true;
		}

		int unknownDemand = economy.GetDemand(PROBE_UNKNOWN_ITEM_ID);
		if (unknownDemand != 5)
		{
			SetResultFailure("GetDemand() on an unknown id returned %1, expected the documented default 5", unknownDemand.ToString());
			return true;
		}

		// SetPrice -> GetPrice round-trip.
		economy.SetPrice(PROBE_ITEM_ID, PROBE_PRICE);
		int readPrice = economy.GetPrice(PROBE_ITEM_ID);
		if (readPrice != PROBE_PRICE)
		{
			SetResultFailure("SetPrice(%1) then GetPrice() returned %2", PROBE_PRICE.ToString(), readPrice.ToString());
			return true;
		}

		// SetDemand -> GetDemand round-trip.
		economy.SetDemand(PROBE_ITEM_ID, PROBE_DEMAND);
		int readDemand = economy.GetDemand(PROBE_ITEM_ID);
		if (readDemand != PROBE_DEMAND)
		{
			SetResultFailure("SetDemand(%1) then GetDemand() returned %2", PROBE_DEMAND.ToString(), readDemand.ToString());
			return true;
		}

		// GetBuyPrice at "0 0 0" with no player skips the town stock / port distance terms in
		// GetSellPrice entirely, so it is exactly the base price plus the shop profit margin.
		int buyPrice = economy.GetBuyPrice(PROBE_ITEM_ID, "0 0 0", -1);
		int expectedBuyPrice = Math.Round(PROBE_PRICE + (PROBE_PRICE * config.m_fShopProfitMargin));

		if (buyPrice != expectedBuyPrice)
		{
			SetResultFailure("GetBuyPrice() returned %1, expected %2 (base %3 plus m_fShopProfitMargin)",
				buyPrice.ToString(), expectedBuyPrice.ToString(), PROBE_PRICE.ToString());
			return true;
		}

		if (buyPrice <= PROBE_PRICE)
		{
			SetResultFailure("GetBuyPrice() %1 is not above the base price %2 - the shop margin is not being applied",
				buyPrice.ToString(), PROBE_PRICE.ToString());
			return true;
		}

		PrintFormat("Economy seams: price %1, demand %2, buy price %3",
			readPrice.ToString(), readDemand.ToString(), buyPrice.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! No character's persistence configuration may self-spawn - alive OR dead. Both halves are
//! load-bearing and they fail in opposite directions.
//!
//! ALIVE: Overthrow's managers rebuild every garrison, patrol and deployment from manager state on
//! load (decision v2-5). A live character that ALSO self-spawns is doubled AI at every base, which
//! is the catastrophe the AI SelfSpawn 0 overrides in Overthrow.conf exist to prevent.
//!
//! DEAD: this is the guard against reinstating a mechanism that has now failed BUG-018 twice. The
//! kill hook used to flip the corpse's config to self-spawn via PersistenceSystem.SetConfig()
//! (OVT_PersistenceTracking.MarkForSelfSpawn). Measured 2026-08-04 by decoding save blobs: SetConfig
//! marks the configuration SCRIPTED, a scripted configuration is written with an EMPTY store name,
//! and the loader resolves configurations BY store name - so every marked corpse became a record the
//! engine rejects on load with "Unable to locate configuruation ''". It never brought a corpse back
//! and it poisoned the save. The flag was set in memory, which is exactly why the previous version of
//! this case went GREEN while the feature stayed broken - it asserted the flag, not the outcome.
//!
//! So the dead half now asserts the ABSENCE of the flag. If someone reinstates the flip, this case
//! goes red and names the reason. BUG-018 remains open for AI corpses; the only mechanism that
//! survives a load is SelfSpawn declared in a .conf, and no native rule can pick out a dead character.
//! Player corpses are covered, because the player-character config carries SelfSpawn 1 in
//! Overthrow.conf - declared, not scripted.
//!
//! THE POLL IS DIAGNOSTIC, NOT A RETRY. The case waits for the controller to actually report death
//! before reading the config, so a failure cannot be "asked too early"; expiry of that wait is itself
//! a named failure.
//!
//! PROVEN ABLE TO FAIL 2026-08-04: restoring the MarkForSelfSpawn() call in
//! OVT_OverthrowGameMode.OnCharacterKilledPersist() turns the dead half red with its own sentence;
//! setting SelfSpawn 1 on the AI character config {64EACAC5BFDB31EC} turns the live half red.
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
	[Step(EStage.Main)]
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
			SetResultFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - see OVT_TEST_Init_Persistence_SystemIsOnline.");
			return true;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetResultFailure("The recruit manager has no character prefab to spawn a subject from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetResultFailure("No towns are registered - nowhere sensible to spawn the subject character");
			return true;
		}

		m_Character = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, towns.m_Towns[0].location);
		if (!m_Character)
		{
			SetResultFailure("SpawnEntityPrefab() produced no character from the civilian prefab");
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
			SetResultFailure("The persistence system or the subject character disappeared while waiting for tracking");
			return FinishAndCleanUp();
		}

		if (!persistence.IsTracked(m_Character))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > MAX_TRACKING_POLLS)
			{
				SetResultFailure("The spawned character was never tracked (%1 polls) - Character_Base.et no longer carries the native Persistence component, so no character (dead or alive) is ever saved", m_iTrackingPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		EntityPersistenceConfig aliveConfig = EntityPersistenceConfig.Cast(persistence.GetConfig(m_Character));
		if (!aliveConfig)
		{
			SetResultFailure("GetConfig() handed back no entity config for a tracked live character");
			return FinishAndCleanUp();
		}

		// Phase 3's invariant, the reason the corpse rule must not overmatch: a LIVE character that
		// self-spawns on load is doubled AI for every garrison Overthrow rebuilds itself.
		if (aliveConfig.m_bSelfSpawn)
		{
			SetResultFailure("A LIVE character's matched persistence config already self-spawns - the corpse rule (or a config edit) is matching the living, which doubles every AI on load");
			return FinishAndCleanUp();
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		if (!character || !character.GetCharacterController())
		{
			SetResultFailure("The spawned recruit is not a character with a controller - it cannot be killed, so the corpse re-match cannot be exercised");
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
			SetResultFailure("The persistence system or the corpse disappeared before the corpse config could be read");
			return FinishAndCleanUp();
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		bool isDead = character && character.GetCharacterController() && character.GetCharacterController().IsDead();

		if (!isDead)
		{
			m_iRematchPolls += 1;
			if (m_iRematchPolls > MAX_REMATCH_POLLS)
			{
				SetResultFailure("ForceDeath() was called but the character controller never reported the character dead (%1 polls) - the corpse half of this case could not be exercised at all",
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
			SetResultFailure("A dead character's persistence config self-spawns. Something re-introduced PersistenceSystem.SetConfig() on the kill path (OVT_PersistenceTracking.MarkForSelfSpawn): a scripted config is serialized with an EMPTY store name, so the loader rejects the record with \"Unable to locate configuruation ''\" and the corpse never comes back - it only poisons the save. See BUG-018.");
			return FinishAndCleanUp();
		}

		PrintFormat("Character config never self-spawns: verified alive, and verified dead after %1 poll(s)", m_iRematchPolls.ToString());
		SetResultSuccess();
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
//! WHY THIS IS THE INVARIANT. A load only instantiates records whose configuration says
//! SelfSpawn 1; everything else is dropped from storage and is gone from every later save point
//! (measured 2026-08-05 across a real restart: 215 records in one savepoint, 12 in the next).
//! Vanilla ships the player-character config as SelfSpawn 0 (Configuration/Character/Player.conf:5)
//! and re-enables it for missions in Mission.conf:18-24; Overthrow inherits Common.conf, not
//! Mission.conf, so it must do that itself - the {64ECE6462993EA13} override in the Player group of
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! Without it a player who logs out has a stored body id pointing at a record that will not exist
//! next session, and comes back as a fresh civilian with their gear gone - which is exactly what a
//! dedicated server reported on 2026-08-04.
//!
//! THE CONFIG IS READ OFF A LIVE PLAYER-CONTROLLED CHARACTER, not looked up by GUID, because what
//! matters is which configuration the engine actually MATCHED - a rule that stopped matching, or an
//! override in the wrong group, would both leave the GUID intact and the behaviour broken.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Persistence_PlayerCharacterConfigSelfSpawns : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player to have a character with a matched config.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			SetResultFailure("SCR_PersistenceSystem.GetScriptedInstance() is null - see OVT_TEST_Init_Persistence_SystemIsOnline.");
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
				SetResultFailure("No tracked player-controlled character appeared in %1 polls, so the configuration the engine matches to a player body could not be read", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		EntityPersistenceConfig config = EntityPersistenceConfig.Cast(persistence.GetConfig(body));
		if (!config)
		{
			SetResultFailure("GetConfig() handed back no entity config for a tracked player character");
			return true;
		}

		if (!config.m_bSelfSpawn)
		{
			SetResultFailure("The player character's matched persistence config does NOT self-spawn. The {64ECE6462993EA13} override in the Player group of Overthrow.conf is not reaching the config the engine matched. A player who logs out will come back as a fresh civilian with their gear gone, because their stored body record is dropped at load.");
			return true;
		}

		PrintFormat("Player character config self-spawns (matched after %1 poll(s)) - a stored body survives a restart", m_iPolls.ToString());
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Overthrow's reconnect component must be ON THE GAME MODE, or a disconnecting player's body is
//! deleted and BUG-086 is back.
//!
//! WHY THIS CASE IS THE TRIPWIRE FOR A PREFAB EDIT. SCR_BaseGameMode.OnPlayerDisconnected deletes the
//! leaving player's character unless SCR_ReconnectComponent.GetInstance().HandlePlayerDisconnect()
//! claims it (SCR_BaseGameMode.c:963-975). That instance exists only because
//! Prefabs/GameMode/OVT_OverthrowGameMode.et carries an OVT_ReconnectComponent entry - and a prefab
//! entry that is dropped, renamed or re-saved without it fails SILENTLY: the scripts still compile,
//! the component simply never initialises, and the next disconnect quietly destroys a player's body
//! and everything they were carrying.
//!
//! IT ASSERTS THE SUBCLASS, not merely "some reconnect component". Vanilla's own base class would
//! resolve here and would answer HandlePlayerDisconnect() with vanilla's rules - REPLICATION kicks
//! only, 120 s expiry, and no hiding - which is a different feature wearing the same name.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_ReconnectComponentClaimsLeavingBodies : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		SCR_ReconnectComponent reconnect = SCR_ReconnectComponent.GetInstance();
		if (!reconnect)
		{
			SetResultFailure("There is no reconnect component on the game mode. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_ReconnectComponent entry, so SCR_BaseGameMode.OnPlayerDisconnected will DELETE every disconnecting player's body - see BUG-086.");
			return true;
		}

		if (!OVT_ReconnectComponent.Cast(reconnect))
		{
			SetResultFailure("The game mode's reconnect component is vanilla's %1, not OVT_ReconnectComponent. Vanilla only reserves a body for connection drops and only for its timeout, so a clean quit still destroys it.",
				reconnect.Type().ToString());
			return true;
		}

		Print("Overthrow's reconnect component is live - a disconnecting player's body is claimed, not deleted");
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Reserving a CHARACTER must hide it, stop it simulating, leave it tracked - and be reversible.
//!
//! WHY IT IS EXERCISED ON A REAL CHARACTER RATHER THAN ON A BARE ENTITY. The reservation model rests
//! on an assumption that had never been measured when it was designed (BUG-086 handoff, 2026-08-05):
//! that ClearFlags/SetFlags behave on a ChimeraCharacter - which carries a controller, an animation
//! system and an inventory - the way the engine's own documentation says they do on a plain entity,
//! and that the change is cleanly reversible. This case is that measurement, as far as script can
//! observe it: the flags are read back, not assumed.
//!
//! TRACKING IS ASSERTED THROUGHOUT, and it is the property that distinguishes this design from the
//! one it replaces. A reserved body that stopped being tracked would be absent from the next save
//! point and gone after a restart, which is precisely the failure BUG-086 is about.
//!
//! WHAT IT CANNOT SEE: whether a CLIENT still renders its own copy of a hidden body. Flags are local
//! engine state, there is no client in the autotest world, and that residual is play-test territory -
//! it is documented on OVT_PersistenceReservation itself.
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
	[Step(EStage.Main)]
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
			SetResultFailure("The recruit manager has no character prefab to spawn a subject from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetResultFailure("No towns are registered - nowhere sensible to spawn the subject character");
			return true;
		}

		m_Character = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, towns.m_Towns[0].location);
		if (!m_Character)
		{
			SetResultFailure("SpawnEntityPrefab() produced no character from the civilian prefab");
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
			SetResultFailure("The subject character disappeared before it could be reserved");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceTracking.IsTracked(m_Character))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > MAX_TRACKING_POLLS)
			{
				SetResultFailure("The spawned character was never tracked (%1 polls) - a body that is not tracked cannot be reserved in the first place", m_iTrackingPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (OVT_PersistenceReservation.IsReserved(m_Character))
		{
			SetResultFailure("A freshly spawned character already reports reserved - IsReserved() is not reading the flag it claims to, so every assertion below would pass vacuously");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.Reserve(m_Character))
		{
			SetResultFailure("Reserve() refused a tracked, live character");
			return FinishAndCleanUp();
		}

		EntityFlags reservedFlags = m_Character.GetFlags();

		if (reservedFlags & EntityFlags.VISIBLE)
		{
			SetResultFailure("Reserve() left the character VISIBLE. ClearFlags does not take on a ChimeraCharacter, so an offline player's body stays in play - see OVT_PersistenceReservation.");
			return FinishAndCleanUp();
		}

		if (reservedFlags & EntityFlags.TRACEABLE)
		{
			SetResultFailure("Reserve() left the character TRACEABLE - an offline player's body can still be shot and seen by AI, which is the whole thing hiding it is meant to prevent");
			return FinishAndCleanUp();
		}

		if (reservedFlags & EntityFlags.ACTIVE)
		{
			SetResultFailure("Reserve() left the character ACTIVE - a reserved body keeps simulating, and every offline player costs a full character tick forever");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceTracking.IsTracked(m_Character))
		{
			SetResultFailure("Reserving the character released its persistence tracking. A reserved body MUST stay tracked - an untracked one is absent from the next save point and gone after a restart, which is BUG-086 all over again.");
			return FinishAndCleanUp();
		}

		if (!GetGame().GetWorld().FindEntityByID(m_Character.GetID()))
		{
			SetResultFailure("Reserving the character removed it from the world - it must be hidden in place, not despawned");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.Release(m_Character))
		{
			SetResultFailure("Release() refused a reserved character");
			return FinishAndCleanUp();
		}

		EntityFlags releasedFlags = m_Character.GetFlags();

		if (!(releasedFlags & EntityFlags.VISIBLE) || !(releasedFlags & EntityFlags.TRACEABLE) || !(releasedFlags & EntityFlags.ACTIVE))
		{
			SetResultFailure("Release() did not put the character back in play: flags %1 (expected VISIBLE, TRACEABLE and ACTIVE all set). A returning player would be handed an invisible, untraceable, frozen body.",
				releasedFlags.ToString());
			return FinishAndCleanUp();
		}

		if (OVT_PersistenceReservation.IsReserved(m_Character))
		{
			SetResultFailure("The character still reports reserved after Release()");
			return FinishAndCleanUp();
		}

		PrintFormat("Reservation round trip on a live character: hidden, untraceable, inactive, still tracked, and fully restored (tracked after %1 poll(s))", m_iTrackingPolls.ToString());
		SetResultSuccess();
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
//! BUG-085: a loadout must carry the CONTENTS of clothing and backpacks, not just the containers.
//!
//! WHAT IT MEASURES: the whole save -> apply round trip through the manager's PUBLIC API. A source
//! character is dressed, a distinctive item is put INSIDE a worn container, the loadout is saved,
//! and it is applied to a SECOND, freshly spawned character. The assertion is made against the
//! target container's own storage - the item must be in THERE, not merely somewhere on the target -
//! because "somewhere on the character" is exactly what a flattened apply would also satisfy.
//!
//! WHY IT FAILED BEFORE: ApplyNestedItemsSpawnToUniversalStorage() looked for an
//! InventoryStorageManagerComponent ON THE CONTAINER to insert through. Uniforms, vests and
//! backpacks never carry one - that component belongs to the CHARACTER - so the lookup failed for
//! exactly the containers that matter, and every nested item was spawned and then immediately
//! deleted. A player restoring a loadout got empty clothing and an empty backpack. This is the
//! mechanism the logout gear snapshot depends on (see the persistence feature's context.md), so it
//! is also the difference between "kit restored" and "kit lost" for a returning player.
//!
//! PROVEN ABLE TO FAIL: reverting the fix (inserting through
//! containerEntity.FindComponent(InventoryStorageManagerComponent) again) makes this case report
//! "... is not inside the applied container ... - the container came back EMPTY", which is the
//! defect verbatim.
//!
//! NOTHING IS HARD-CODED TO A PREFAB. The container is whatever the character is already wearing,
//! falling back to the first civilian-loadout choice that has a universal storage; the nested item
//! comes from the difficulty config's starting items. Both are read from the LIVE config, so a
//! content change cannot quietly turn this case vacuous - it fails with a named diagnostic instead.
//!
//! The storage PURPOSE mask is deliberately never used to classify anything here: EStoragePurpose
//! ordinals and flag bits diverge, and the character loadout storage answers PURPOSE_DEPOSIT while
//! holding worn clothing. Containers are identified by component class, which is the same test the
//! code under test makes.
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
	[Step(EStage.Main)]
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
			SetResultFailure("The recruit manager has no character prefab to spawn loadout subjects from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetResultFailure("No towns are registered - nowhere sensible to spawn the subject characters");
			return true;
		}

		vector origin = towns.m_Towns[0].location;

		m_SourceCharacter = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, origin);
		m_TargetCharacter = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, origin + "6 0 6");

		if (!m_SourceCharacter || !m_TargetCharacter)
		{
			SetResultFailure("SpawnEntityPrefab() produced no character from the civilian prefab");
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
				SetResultFailure("The spawned characters never came up with inventory storages (%1 polls) - nothing about a loadout can be measured without them", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		string diagnostic;

		IEntity container = ResolveContainer(sourceManager, diagnostic);
		if (!container)
		{
			SetResultFailure("%1", diagnostic);
			return FinishAndCleanUp();
		}

		UniversalInventoryStorageComponent containerStorage = UniversalInventoryStorageComponent.Cast(container.FindComponent(UniversalInventoryStorageComponent));

		IEntity nested = StockContainer(sourceManager, containerStorage, diagnostic);
		if (!nested)
		{
			SetResultFailure("%1", diagnostic);
			return FinishAndCleanUp();
		}

		m_sContainerPrefab = OVT_Global.GetPrefabName(container);
		m_sNestedPrefab = OVT_Global.GetPrefabName(nested);

		if (m_sContainerPrefab.IsEmpty() || m_sNestedPrefab.IsEmpty())
		{
			SetResultFailure("Could not read back a prefab name for the container or its contents - the assertion below would have nothing to match on");
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
			SetResultFailure("OVT_Global.GetLoadouts() is null - no loadout manager on the game mode");
			return FinishAndCleanUp();
		}

		loadouts.SaveLoadout(TEST_PLAYER_ID, TEST_LOADOUT_NAME, m_SourceCharacter);

		OVT_PlayerLoadout saved = loadouts.GetLoadout(TEST_PLAYER_ID, TEST_LOADOUT_NAME);
		if (!saved)
		{
			SetResultFailure("SaveLoadout() stored nothing for the source character - the apply half cannot be measured");
			return FinishAndCleanUp();
		}

		// The EXTRACTION half is asserted separately and first, so that a regression there reports as
		// itself instead of being blamed on apply. It is also what stops this case passing vacuously:
		// if the container's contents were never recorded, there is nothing for apply to restore.
		if (!ExtractedNesting(saved))
		{
			SetResultFailure("The saved loadout does not record %1 inside %2 - extraction dropped the nesting, so the apply assertion below would be vacuous",
				m_sNestedPrefab, m_sContainerPrefab);
			return FinishAndCleanUp();
		}

		if (!loadouts.ApplyLoadoutToEntity(saved, m_TargetCharacter))
		{
			SetResultFailure("ApplyLoadoutToEntity() reported failure applying the saved loadout to a fresh character");
			return FinishAndCleanUp();
		}

		bool containerArrived = false;
		if (FindNestedOnTarget(containerArrived))
		{
			PrintFormat("Loadout round trip kept container contents: %1 arrived inside %2 on a fresh character", m_sNestedPrefab, m_sContainerPrefab);
			SetResultSuccess();
			return FinishAndCleanUp();
		}

		if (!containerArrived)
		{
			SetResultFailure("The container %1 itself never arrived on the target character - the loadout apply failed further up than the nested-item path this case is about",
				m_sContainerPrefab);
			return FinishAndCleanUp();
		}

		SetResultFailure("%1 is not inside the applied container %2 - the container came back EMPTY. Nested items are inserted through the OWNER's storage manager against the container's own storage; a manager looked up on the container itself is always null for clothing and backpacks, and every item is then deleted (BUG-085).",
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
//! WHY THIS CASE IS THE TRIPWIRE FOR A PREFAB EDIT. OVT_PlayerGroupManagerComponent is what guarantees
//! every connected player has a group of their own: OVT_SpawnLogic.CreateAndJoinGroup delegates the
//! whole "create this player's private group and put them in it" body to its EnsureOwnGroup(), and its
//! SCR_AIGroup.GetOnPlayerAdded()/GetOnPlayerRemoved() subscriptions are what put a player back in a
//! group after they leave one. That instance exists only because
//! Prefabs/GameMode/OVT_OverthrowGameMode.et carries an OVT_PlayerGroupManagerComponent entry - and a
//! prefab entry that is dropped, renamed or re-saved without it fails SILENTLY: the scripts still
//! compile, the component never initialises, and every player spawns with NO GROUP - no AI commanding,
//! no group indicator, no recruits. That is BUG-088's symptom set, and it is invisible in solo play
//! until someone tries to command an AI.
//!
//! It asserts the LIVE COMPONENT, not just a non-null static: s_Instance is assigned in OnPostInit and
//! never cleared, so a stale pointer from an earlier world would satisfy a bare null check. Comparing
//! it against the component the game mode actually carries is what makes the assertion mean "this
//! world's game mode has the manager".
//!
//! The third assertion pins the manager's own precondition rather than its behaviour: EnsureOwnGroup()
//! must refuse a player id that has no player controller and return -1. If it ever answered anything
//! else it would be creating stray leaderless groups for ids that are not players, which is how a
//! faction's group list fills with empty groups and radio frequencies run out.
//!
//! There is nothing here about JOINING, LEAVING or RECONNECTING - all three need two client processes
//! and are on the manual play-test checklist (implementation.md section 6, steps 1, 9, 10, 14).
//!
//! PROVEN ABLE TO FAIL 2026-08-06: the OVT_PlayerGroupManagerComponent entry was temporarily deleted
//! from Prefabs/GameMode/OVT_OverthrowGameMode.et and `tools/run-tests.sh
//! OVT_TEST_Init_PlayerGroups_ManagerResolves` exited 1 on the first assertion
//! ("OVT_PlayerGroupManagerComponent.GetInstance() is null ..."); restoring the entry returned it to
//! exit 0. No retries, no maxAttempts - the manager either initialised during world load or it did not.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_PlayerGroups_ManagerResolves : SCR_AutotestCaseBase
{
	//! A player id no session ever issues, used to prove EnsureOwnGroup refuses an unknown player.
	static const int NOT_A_PLAYER_ID = -1;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_PlayerGroupManagerComponent manager = OVT_PlayerGroupManagerComponent.GetInstance();
		if (!manager)
		{
			SetResultFailure("OVT_PlayerGroupManagerComponent.GetInstance() is null - Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_PlayerGroupManagerComponent entry. Nothing gives a spawning player a group, and nothing puts a player back in one when they leave a group: no AI commanding, no group indicator, no recruits (BUG-088's symptom set).");
			return true;
		}

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
		{
			SetResultFailure("GetGame().GetGameMode() is null - there is no game mode to carry the manager");
			return true;
		}

		OVT_PlayerGroupManagerComponent onGameMode = OVT_PlayerGroupManagerComponent.Cast(gameMode.FindComponent(OVT_PlayerGroupManagerComponent));
		if (onGameMode != manager)
		{
			SetResultFailure("OVT_PlayerGroupManagerComponent.GetInstance() is not the component on this world's game mode - s_Instance is stale, so every caller is talking to a manager that is not wired to anything");
			return true;
		}

		// A player id that is not a player must never produce a group.
		int refused = manager.EnsureOwnGroup(NOT_A_PLAYER_ID);
		if (refused != -1)
		{
			SetResultFailure("EnsureOwnGroup(%1) returned %2 instead of -1 - the manager creates groups for ids that have no player controller, which fills the faction's group list with stray leaderless groups and exhausts its radio frequencies",
				NOT_A_PLAYER_ID.ToString(), refused.ToString());
			return true;
		}

		Print("Player-group manager is live on the game mode and refuses a player id with no controller");
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The tutorial manager is live on the game mode and its authored entries are structurally sound.
//!
//! Two failures hide here and neither announces itself at runtime. The first is the manager being
//! dropped from Prefabs/GameMode/OVT_OverthrowGameMode.et, which turns the whole tutorial system off
//! silently - no error, no popup, just a mod that teaches nothing. The second is a malformed entry:
//! an empty or duplicated id, a body with no pages, or a binding with no triggers. An id is the
//! entry's permanent key in every player's per-machine seen store, so a duplicate does not merely
//! misfire - it can suppress the wrong tip forever on every machine that saw it.
//!
//! This asserts the SHAPE of the authored data, never a count of entries. The proof entry is one
//! today and tutorial-content will add a dozen; ">= 1" is the only honest expectation.
//!
//! A third failure was added by tutorial-content Phase 2b and hides even better than the other two: a
//! PLAYER_TRANSACTION filter that is not an OVT_ShopType member name. See CheckTransactionFilters.
//!
//! A fourth and a fifth were added by first-spawn Phase 5 and are the same shape one step further on.
//! A PLAYER_SPAWNED filter that is not a spawn context can never match (CheckSpawnFilters), and a
//! spawn context with no enabled entry left to match leaves half the player base with no welcome at
//! all (CheckWelcomeCoverage). Both are silent at runtime and invisible to grep.
//!
//! PROVEN ABLE TO FAIL 2026-08-07: see the record in
//! docs/features/new-player-experience/tutorial-system/context.md.
//! The PLAYER_TRANSACTION filter branch PROVEN ABLE TO FAIL 2026-08-09: see the Proven-Red Record in
//! docs/features/new-player-experience/tutorial-content/context.md.
//! The PLAYER_SPAWNED filter and welcome-coverage branches PROVEN ABLE TO FAIL 2026-08-09: see the
//! Proven-Red Table in docs/features/new-player-experience/first-spawn/context.md.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TutorialManagerComponent manager = OVT_Global.GetTutorialManager();
		if (!manager)
		{
			SetResultFailure("OVT_Global.GetTutorialManager() is null - Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_TutorialManagerComponent entry. Nothing subscribes to any trigger invoker and no tutorial can ever fire, silently.");
			return true;
		}

		array<ref OVT_TutorialEntryConfig> entries = manager.GetEntries();
		if (!entries)
		{
			SetResultFailure("OVT_TutorialManagerComponent.GetEntries() returned a null array - m_aEntries was never authored on the game mode prefab");
			return true;
		}

		// >= 1, never a magic count: the proof entry is one today and tutorial-content adds more.
		if (entries.Count() < 1)
		{
			SetResultFailure("No tutorial entries registered: m_aEntries on the game mode prefab is empty, so the tutorial framework has nothing to deliver");
			return true;
		}

		string shapeError = FindFirstShapeError(entries);
		if (shapeError != "")
		{
			SetResultFailure("%1", shapeError);
			return true;
		}

		Print("Tutorial manager is live with " + entries.Count().ToString() + " structurally valid entries");
		SetResultSuccess();
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
	//! PLAYER_SPAWNED's filter is the SPAWN CONTEXT: what the server's player preparation actually gave
	//! this player. There are exactly three legal values - "house" (a home, a car and starting cash),
	//! "nohouse" (a fallback spawn with neither) and "" (fire for either). The value compared against is
	//! carried to the client and pushed into the event by the tutorial component, so like the
	//! PLAYER_TRANSACTION case above, a fourth value is NOT findable by grep against another config: it
	//! simply never equals the dispatched context, the welcome never fires, and nothing reports it.
	//!
	//! This is the failure a one-character typo in Configs/Tutorials/welcomeNohome.conf produces, and
	//! the entry it names is the one to open.
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
	//! A player who spawns with a house and a player who spawns at a bus stop are told different things,
	//! by two different entries filtered on the same event. Delete one, or set its m_bEnabled to 0, and
	//! half the player base gets NO welcome at all - and nothing else in the tree notices, because every
	//! remaining entry is still structurally perfect.
	//!
	//! Deliberately "AT LEAST ONE", not "exactly one": a third-party mod adding its own PLAYER_SPAWNED
	//! entry is not a defect and must not fail the build. The selection runs through the real matcher,
	//! so a disabled entry does not count towards coverage - which is what makes the disable case fail.
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
	//! PLAYER_TRANSACTION is the only event in the authored set whose filter value is MANUFACTURED BY
	//! THE ENGINE rather than written by a human on both sides: the manager builds it with
	//! SCR_Enum.GetEnumName(OVT_ShopType, shop.m_ShopType) (OVT_TutorialManagerComponent.c:255), which
	//! is typename.EnumToString. Every other filter in the set is compared against a string some other
	//! config already spells out, so a typo there is findable by grep. Here it is not: a filter that is
	//! not literally an OVT_ShopType member name simply never equals the dispatched value, the entry
	//! never fires, and NOTHING reports it - no compile error, no runtime warning, no log line.
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
//! Every manager ScriptInvoker the trigger catalog names still exists and is allocated.
//!
//! The tutorial system instruments NOTHING at the call sites - every server-side trigger hangs off an
//! invoker that already belonged to some other manager. That is what keeps the feature cheap, and it
//! is also exactly why it can rot without a sound: rename, delete or stop allocating one of these
//! invokers and OVT_TutorialManagerComponent.SubscribeToInvokers() quietly subscribes to one fewer
//! event. Nothing errors. A whole class of tutorial simply never fires again.
//!
//! This is the case that goes red on that day. It asserts existence only - never that anything fires,
//! which needs a running campaign and belongs in a higher tier.
//!
//! The wanted invoker is the odd one out: it is STATIC and lazily allocated (plan decision D13 plus
//! the Phase 0 refinement), so its getter can never return null. Asserted anyway - the assertion that
//! matters there is that the getter still exists at all.
//!
//! PROVEN ABLE TO FAIL 2026-08-07: see the record in
//! docs/features/new-player-experience/tutorial-system/context.md.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Tutorial_InvokerSeamsExist : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		string missing = FindFirstMissingInvoker();

		if (missing != "")
		{
			SetResultFailure("Tutorial trigger seam missing: %1. OVT_TutorialManagerComponent subscribes to it in SubscribeToInvokers(); with it gone that trigger silently never fires again.", missing);
			return true;
		}

		Print("Every catalogued tutorial trigger invoker is present and allocated");
		SetResultSuccess();
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

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE SERIALIZATION GATE (plan risk R1). The per-machine seen store really round-trips through the
//! engine's user-settings container.
//!
//! Everything about the seen store's RULES is Logic-tier and world-free. This case exists for the
//! one thing that tier structurally cannot answer: whether a
//! `ref array<ref OVT_SeenTutorialEntry>` inside a ModuleGameSettings actually survives
//! BaseContainerTools.ReadFromInstance -> WriteToInstance at all. That shape was chosen because it
//! has direct base-game precedent (SCR_FilterSetStorage, round-tripped on every server-browser
//! filter save) rather than because it was tested - and "chosen because precedent" is exactly the
//! kind of belief that should have an assertion behind it. If this goes red, the ranked fallbacks
//! are array<ResourceName>, then a delimited string, then an enum array.
//!
//! WHAT IT DOES NOT PROVE: an application restart. The harness cannot relaunch the process, so the
//! disk half of the round trip is verified by inspecting
//! $profile:.save/settings/ReforgerGameSettings.conf after a run (recorded in context.md) and by
//! the manual smoke test in the plan's verification method.
//!
//! PROFILE HYGIENE: the ids are a deliberately test-only namespace that no authored entry can
//! collide with, and the case RESTORES the profile block to empty before it returns - including on
//! every failure path. A test that leaves state behind would silently change what the next run of
//! any other suite sees.
//!
//! Which is why this case is not instantaneous. GetGame().SaveUserSettings() is THROTTLED, and a
//! call inside the throttle window is DROPPED rather than deferred - measured on 2026-08-07: two
//! saves microseconds apart leave only the first on disk, two saves six seconds apart both land.
//! The cleanup write therefore has to wait out the window it just opened, or the test ids would sit
//! in the CI profile forever with m_bTipsDisabled set. That wait is the whole reason for the phase
//! machine below.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips : SCR_AutotestCaseBase
{
	//! Test-only ids. Leading underscores are illegal in the authored entry-id scheme (lowercase
	//! ASCII letters, digits and dashes), so these cannot collide with real content, ever.
	static const string TEST_ID_A = "__ovt-selftest-alpha";
	static const string TEST_ID_B = "__ovt-selftest-beta";

	//! How long to wait after the round trip's write before the cleanup write, in milliseconds.
	//! Comfortably past the measured throttle window (6000 ms was already enough).
	static const int FLUSH_SETTLE_MS = 10000;

	//! 0 = not started, 1 = written and waiting out the flush throttle, 2 = cleaned up.
	protected int m_iPhase;

	//! Tick at which the round trip's write happened.
	protected int m_iWriteTick;

	//! The round trip's verdict, held across frames while the throttle window drains.
	protected string m_sFailure;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Phase 1: write and read the record back. Returning false asks the harness for another frame.
		if (m_iPhase == 0)
		{
			m_sFailure = RunRoundTrip();
			m_iWriteTick = System.GetTickCount();
			m_iPhase = 1;
			return false;
		}

		// Phase 2: wait out the disk-write throttle the round trip just opened.
		if (m_iPhase == 1)
		{
			if (System.GetTickCount() - m_iWriteTick < FLUSH_SETTLE_MS)
				return false;

			m_iPhase = 2;
		}

		// Restore the profile unconditionally: a failed assertion must not ALSO leave the next run's
		// settings block polluted.
		string cleanupFailure = RestoreProfile();

		if (m_sFailure == "")
			m_sFailure = cleanupFailure;

		if (m_sFailure != "")
		{
			SetResultFailure("%1", m_sFailure);
			return true;
		}

		Print("Tutorial settings store round-tripped two ids and the tips flag through the engine user-settings container, and the profile was restored");
		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the profile block back the way this case found it, and PROVES it went back.
	//!
	//! Asserted rather than assumed for two reasons. It is this case's own hygiene contract - the ids
	//! it writes must not be visible to any later run. And "can a value be written BACK to its
	//! default?" is a real property of this store that the shipping code depends on: re-enabling tips
	//! after disabling them is exactly that operation, and a settings serializer that skipped
	//! default-valued members would make the toggle one-way.
	//! \return A ready-to-report failure message, or an empty string when the profile came back clean.
	protected string RestoreProfile()
	{
		if (!OVT_TutorialSettingsAccessor.Reset())
			return "OVT_TutorialSettingsAccessor.Reset() reported the settings store unavailable, so this case's test ids are still in the profile";

		OVT_TutorialSeenStore afterReset = new OVT_TutorialSeenStore();
		bool tipsDisabled;
		OVT_TutorialSettingsAccessor.Load(afterReset, tipsDisabled);

		if (afterReset.Count() != 0)
			return "Reset() left " + afterReset.Count().ToString() + " ids in the profile. Either this case is polluting every later run, or a stored value cannot be written back to its default - which would also make 'Don't show tips again' impossible to turn off.";

		if (tipsDisabled)
			return "Reset() left m_bTipsDisabled set. A value cannot be written back to its default, so re-enabling tips would never persist.";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a known record through the accessor and reads it back through a fresh instance.
	//! \return A ready-to-report failure message, or an empty string when the round trip held.
	protected string RunRoundTrip()
	{
		OVT_TutorialSeenStore written = new OVT_TutorialSeenStore();
		written.MarkSeen(TEST_ID_A);
		written.MarkSeen(TEST_ID_B);

		if (!OVT_TutorialSettingsAccessor.Save(written, true))
			return "OVT_TutorialSettingsAccessor.Save() reported the settings store unavailable. Either the engine has no OVT_TutorialSettings module (declaring the class is supposed to be the whole registration contract) or this run is a console app, in which case the case is being run in the wrong place.";

		// A FRESH store and a fresh OVT_TutorialSettings instance inside Load(): nothing that was
		// just written can be read back out of memory.
		OVT_TutorialSeenStore reloaded = new OVT_TutorialSeenStore();
		bool tipsDisabled;

		if (!OVT_TutorialSettingsAccessor.Load(reloaded, tipsDisabled))
			return "OVT_TutorialSettingsAccessor.Load() reported the settings store unavailable immediately after a successful Save()";

		if (reloaded.Count() != 2)
			return "The seen store came back with " + reloaded.Count().ToString() + " ids, expected 2. The nested ref array<ref OVT_SeenTutorialEntry> did NOT survive the settings container - risk R1 has fired and the feature needs one of its ranked fallbacks.";

		if (!reloaded.HasSeen(TEST_ID_A))
			return "The seen store lost the id '" + TEST_ID_A + "' across a settings round trip";

		if (!reloaded.HasSeen(TEST_ID_B))
			return "The seen store lost the id '" + TEST_ID_B + "' across a settings round trip";

		if (!tipsDisabled)
			return "The 'Don't show tips again' flag was written as true and came back false, so the player's suppression choice would be silently forgotten every launch";

		if (reloaded.GetVersion() != OVT_TutorialSeenStore.CURRENT_VERSION)
			return "The reloaded store is at schema version " + reloaded.GetVersion().ToString() + ", expected " + OVT_TutorialSeenStore.CURRENT_VERSION.ToString();

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE FIELD-MANUAL MERGE SPIKE (plan risk R2, Phase 7.1). Overthrow's same-GUID override of the
//! base game's field-manual root is a DELTA that appends, not a replacement that erases.
//!
//! Configs/FieldManual/FieldManualConfigRoot.conf declares GUID {17295EF80DC38D53} in its .meta -
//! the exact GUID the base game's UI/layouts/Menus/FieldManual/FieldManual.layout:16 hands to
//! SCR_ConfigUIComponent.m_ConfigPath, and therefore the exact resource SCR_FieldManualUI.OnMenuOpen
//! loads. It is not an orphan file: it IS the Field Manual's root. Overthrow's copy declares one
//! category and nothing else, so everything the vanilla manual shows depends on that file merging
//! onto the base rather than shadowing it.
//!
//! WHY THIS IS WORTH A TEST RATHER THAN A GLANCE. The failure is not subtle but it is invisible from
//! the repo: five vanilla categories and every tile background disappear, and
//! SCR_FieldManualUI.c:253 calls m_ConfigRoot.m_aTileBackgrounds.GetRandomElement() UNGUARDED for
//! every tile it draws - so a replacement does not degrade the manual, it errors the moment it opens.
//! Worse, the same merge behaviour is what at least four other Overthrow configs rely on
//! (chimeraInputCommon.conf over 9,583 vanilla lines, ChimeraSystemsConfig.conf, CommandingMenu.conf,
//! the game-mode prefab's append form). If this case ever goes red, the finding is much larger than
//! the Field Manual.
//!
//! It loads through SCR_FieldManualConfigLoader.LoadConfigRoot - the menu's own entry point - so it
//! exercises the real resolution path and not a private reimplementation of it. That call is
//! side-effect free here: SCR_ConfigHelperT.GetConfigObject builds a FRESH instance from the
//! container every time, which matters because SCR_FieldManualUI.SetAllEntriesAndParents prunes the
//! object it is given.
//!
//! THE COUNTS ARE FLOORS, NOT MAGIC NUMBERS. Five base-game categories (Introduction, Editor, MP
//! Modes, Gameplay, Equipment) plus Overthrow's is six today. A vanilla update that adds a seventh
//! must not turn this suite red, so the assertion is ">= 5 categories that are not Overthrow's".
//!
//! THE ENTRY COUNT IS A LOG LINE, NOT AN ASSERTION. DescribeRoot() prints how many entries the merged
//! root holds (141 when this was written) BEFORE any assertion runs, so one run yields the measurement
//! even on a red verdict. Nothing compares that number to anything, which is deliberate: content
//! features add pages, and adding a page must never turn this case red.
//!
//! THREE BRANCHES GUARD OVERTHROW'S OWN PAGES (field-manual Phase 1b, plan decision D8). They exist
//! because between the twelve frozen title keys landing and tutorial-content linking them, nothing
//! points at eleven of them - so the tutorial-link branch below is blind to them, and a page that was
//! silently pruned or misspelled would go unnoticed for months:
//!  A - every Overthrow entry has at least one content piece. SetAllEntriesAndParents removes an entry
//!      with empty m_aContent, then a sub-category left with nothing, then a category left with
//!      nothing, all without a word. The page does not exist and its deep link opens the front page.
//!  B - the four sub-categories (Getting Started, Money and Trade, Staying Hidden, The Resistance) are
//!      all present. MEMBERSHIP, NOT EQUALITY - a fifth sub-category added later must not turn it red.
//!  C - every Overthrow entry title is unique across the MERGED manual. OVT_OpenEntryByTitle takes the
//!      first m_sTitle that matches, so a collision - with a vanilla page or with another Overthrow
//!      page - silently sends a deep link to the wrong page.
//!
//! Deliberately NOT here: a hard-coded list of the twelve entry title keys. That duplicates the .conf
//! into this file and makes every content edit a two-file change; branch A plus the tutorial-link
//! branch already make a pruned or broken link a red build.
//!
//! WHAT IT DOES NOT PROVE: that anything renders. Widgets, tile art and the two open routes (main
//! menu and pause menu) are outside the automated spine and stay on the human checklist.
//!
//! PROVEN ABLE TO FAIL 2026-08-07: see the record in
//! docs/features/new-player-experience/tutorial-system/context.md.
//! BRANCHES A/B/C PROVEN ABLE TO FAIL 2026-08-08: see the proven-red table in
//! docs/features/new-player-experience/field-manual/context.md.
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
	[Step(EStage.Main)]
	bool Execute()
	{
		SCR_FieldManualConfigRoot root = SCR_FieldManualConfigLoader.LoadConfigRoot(FIELD_MANUAL_ROOT);
		if (!root)
		{
			SetResultFailure("SCR_FieldManualConfigLoader.LoadConfigRoot() returned null for the field-manual root. SCR_FieldManualUI.OnMenuOpen closes itself when this happens, so the Field Manual would not open at all.");
			return true;
		}

		// Printed BEFORE any assertion so one run yields the measurement even on a red verdict.
		Print("[Overthrow.FieldManual] merged root inventory: " + DescribeRoot(root));

		string failure = FindFirstMergeFault(root);
		if (failure != "")
		{
			SetResultFailure("%1", failure);
			return true;
		}

		failure = FindFirstContentlessOverthrowEntry(root);
		if (failure != "")
		{
			SetResultFailure("%1", failure);
			return true;
		}

		failure = FindFirstMissingOverthrowSubCategory(root);
		if (failure != "")
		{
			SetResultFailure("%1", failure);
			return true;
		}

		failure = FindFirstDuplicateOverthrowEntryTitle(root);
		if (failure != "")
		{
			SetResultFailure("%1", failure);
			return true;
		}

		failure = FindFirstBrokenTutorialLink(root);
		if (failure != "")
		{
			SetResultFailure("%1", failure);
			return true;
		}

		Print("Field-manual same-GUID override merged as a delta, and every tutorial deep link resolves");
		SetResultSuccess();
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
	//! SCR_FieldManualUI.SetAllEntriesAndParents (:600-663) drops any entry whose m_aContent is empty,
	//! then any sub-category left with no entries, then any category left with neither - silently, with
	//! no error and no log line. A content-free entry is therefore not a stub, it is an ABSENCE: its
	//! tile never draws, it is missing from m_aAllEntries, and OVT_OpenEntryByTitle falls back to the
	//! manual's front page for it. This is the exact shape of "I'll fill that page in later", which is
	//! why it is guarded from the moment the keys are frozen (plan decision D6).
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
	//! OVT_OpenEntryByTitle walks m_aAllEntries and takes the FIRST entry whose m_sTitle matches, so a
	//! duplicated key means every deep link to it lands on whichever page was collected first. That is
	//! silent and it is wrong, and it is equally wrong whether the collision is with one of the base
	//! game's 140 pages or with another Overthrow page (a copy-pasted entry that never had its title
	//! changed is the likely way it happens). The merged root is searched, not just Overthrow's
	//! category, because a vanilla page adopting an #OVT- key would break the links just as thoroughly.
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
	//! This is plan decision D12's whole accepted cost, made cheap: links are matched on the target
	//! entry's m_sTitle localization key, exactly and case-sensitively, so renaming a manual page
	//! silently breaks every popup pointing at it. Nothing else in the build would notice - the helper
	//! degrades to the manual's front page by design (I2), which looks like a content bug, not a
	//! broken link. No hard-coded key here: whatever tutorial-content authors is what gets checked.
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
//! BUG-118: AI that Overthrow rebuilds every session must never be persistence-tracked, so it can
//! never write the orphaned records that made the save grow without bound (~490 permanent records
//! per idle restart, +4x blob size in four days on the reporting server).
//!
//! WHAT IT MEASURES: an occupying-faction group is spawned exactly the way base upgrades spawn
//! garrisons (OVT_Global.SpawnEntityPrefab + SpawnAllImmediately), and a patrol waypoint the way
//! every patrol gets one (config.SpawnPatrolWaypoint). The case then asserts the group entity,
//! every spawned member character, and the waypoint all end up UNTRACKED.
//!
//! THE CONTROL THAT KEEPS IT HONEST: native persistence registration is LAZY and lands frames
//! after spawn, so "not tracked" right after spawn is what a freshly spawned entity ALWAYS looks
//! like. A control character (the civilian recruit prefab, spawned directly - a path the fix
//! deliberately leaves alone) is spawned in the same frame, and the case only passes once that
//! control IS tracked while the AI entities are NOT. Without the control, this case would pass
//! vacuously in a world where registration never runs at all.
//!
//! PROVEN ABLE TO FAIL (2026-08-09): with the UntrackTransient() call in the modded
//! SCR_AIGroup.AddAIEntityToGroup commented out, the case reports "member 0 ... is still
//! persistence-tracked"; with the whole modded class inert it also names the group entity.
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
	[Step(EStage.Main)]
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
			SetResultFailure("No towns are registered - nowhere sensible to spawn the subjects");
			return true;
		}
		vector location = towns.m_Towns[0].location;

		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (!faction)
		{
			SetResultFailure("No occupying faction is configured - no group prefab to spawn");
			return true;
		}

		ResourceName groupPrefab;
		if (faction.m_aGroupPrefabSlots && !faction.m_aGroupPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aGroupPrefabSlots[0];
		else if (faction.m_aGroupInfantryPrefabSlots && !faction.m_aGroupInfantryPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aGroupInfantryPrefabSlots[0];
		else if (faction.m_aHeavyInfantryPrefabSlots && !faction.m_aHeavyInfantryPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aHeavyInfantryPrefabSlots[0];

		if (groupPrefab.IsEmpty())
		{
			SetResultFailure("The occupying faction has no group prefabs in any slot list - nothing to spawn a garrison from");
			return true;
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(groupPrefab, location));
		if (!group)
		{
			SetResultFailure("The occupying faction's group prefab did not produce an SCR_AIGroup");
			return true;
		}
		m_Group = group;

		// Members arrive over the following frames (SCR_AIGroup spawns one per EOnFrame tick, and
		// retries while the navmesh tile is still streaming in - SpawnAllImmediately() would DROP
		// members whose tile is not loaded yet). AwaitVerdict() gates on their arrival.

		m_Waypoint = OVT_Global.GetConfig().SpawnPatrolWaypoint(location);
		if (!m_Waypoint)
		{
			SetResultFailure("SpawnPatrolWaypoint() produced no waypoint");
			return FinishAndCleanUp();
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetResultFailure("The recruit manager has no character prefab to spawn the tracked control from");
			return FinishAndCleanUp();
		}

		m_Control = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, location);
		if (!m_Control)
		{
			SetResultFailure("SpawnEntityPrefab() produced no control character from the civilian prefab");
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
				SetResultFailure("The spawned group never produced a member (%1 polls) - the member half of this case cannot be asserted", m_iPolls.ToString());
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
				SetResultFailure("The control character was never tracked (%1 polls) - registration is not running, so the untracked AI below proves nothing", m_iPolls.ToString());
				return FinishAndCleanUp();
			}
			return false;
		}

		string stillTracked = NameAnyTrackedSubject();
		if (stillTracked.IsEmpty())
		{
			PrintFormat("Rebuild-on-boot AI is untracked while a directly spawned character is tracked (settled after %1 poll(s))", m_iPolls.ToString());
			SetResultSuccess();
			return FinishAndCleanUp();
		}

		if (m_iPolls > MAX_POLLS)
		{
			SetResultFailure("%1 is still persistence-tracked after %2 polls - it will write a record on the next save that no later session can ever claim or delete, which is BUG-118's unbounded save growth", stillTracked, m_iPolls.ToString());
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
//! BUG-131: recruiting a group-spawned character must put its body BACK under persistence
//! tracking.
//!
//! BUG-118's spawn-side untracking releases every group-spawned character (rebuild-on-boot AI),
//! and town civilians are group-spawned - so by the time a player recruits one, its body is
//! untracked and nothing on the recruitment path re-registered it. An untracked body has no
//! persistent id, the record's m_sBodyPersistenceId stays empty, and the recruit's gear cannot
//! survive any save: every despawn or restart rebuilds it from the fresh prefab in civilian
//! clothes.
//!
//! WHAT IT MEASURES: a group is spawned through the same chokepoints garrisons and town civilian
//! groups use, the case waits until a member is meaningfully untracked (the control character
//! proves registration has landed for the spawn batch - same honesty device as the previous
//! case), recruits that member through the public AddRecruit() API, and asserts the body ends up
//! tracked again.
//!
//! The final assertion is positive (IsTracked flips true), so the case cannot pass vacuously in
//! a world where registration never runs - it dies on the poll budget instead.
//!
//! PROVEN ABLE TO FAIL (2026-08-09): with the CancelUntrackTransient()/Track() pair in
//! AddRecruit() disabled, the case reports "the recruited body is still untracked".
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
	[Step(EStage.Main)]
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
			SetResultFailure("No towns are registered - nowhere sensible to spawn the subjects");
			return true;
		}
		vector location = towns.m_Towns[0].location;

		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (!faction)
		{
			SetResultFailure("No occupying faction is configured - no group prefab to spawn");
			return true;
		}

		ResourceName groupPrefab;
		if (faction.m_aGroupPrefabSlots && !faction.m_aGroupPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aGroupPrefabSlots[0];
		else if (faction.m_aGroupInfantryPrefabSlots && !faction.m_aGroupInfantryPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aGroupInfantryPrefabSlots[0];
		else if (faction.m_aHeavyInfantryPrefabSlots && !faction.m_aHeavyInfantryPrefabSlots.IsEmpty())
			groupPrefab = faction.m_aHeavyInfantryPrefabSlots[0];

		if (groupPrefab.IsEmpty())
		{
			SetResultFailure("The occupying faction has no group prefabs in any slot list - nothing to spawn a group from");
			return true;
		}

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(groupPrefab, location));
		if (!group)
		{
			SetResultFailure("The occupying faction's group prefab did not produce an SCR_AIGroup");
			return true;
		}
		m_Group = group;

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetResultFailure("The recruit manager has no character prefab to spawn the tracked control from");
			return FinishAndCleanUp();
		}

		m_Control = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, location);
		if (!m_Control)
		{
			SetResultFailure("SpawnEntityPrefab() produced no control character from the civilian prefab");
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
				SetResultFailure("No untracked group member alongside a tracked control after %1 polls - the precondition (BUG-118 untracking settled, registration running) never held", m_iPolls.ToString());
				return FinishAndCleanUp();
			}
			return false;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		m_sRecruitId = recruits.AddRecruit(TEST_OWNER_UID, m_Member);
		if (m_sRecruitId.IsEmpty())
		{
			SetResultFailure("AddRecruit() returned no recruit ID for the untracked group member");
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
			SetResultSuccess();
			return FinishAndCleanUp();
		}

		if (m_iPolls > MAX_POLLS)
		{
			SetResultFailure("The recruited body is still untracked after %1 polls - it will never reach a save, its record keeps an empty body id, and the recruit's gear cannot survive a despawn or restart (BUG-131)", m_iPolls.ToString());
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
//! THE WAY BACK FROM "DON'T SHOW TIPS AGAIN" (BUG-133). A reset really does leave the profile with
//! zero seen ids AND tips re-enabled, read back through a fresh store.
//!
//! WHAT IT PINS, AND WHY IT IS WORTH A CASE. OVT_TutorialComponent.ResetSeen() is three statements,
//! and every one of them is a statement about STORAGE rather than about logic:
//!   - the store is LOADED before it is cleared (GetSeenStore() is what performs the load, so a
//!     clear-then-load ordering would silently repopulate the set from disk and undo the reset);
//!   - the clear is written back, not just held in memory;
//!   - m_bTipsDisabled goes back to its DEFAULT, and a settings serializer that skipped
//!     default-valued members would make the toggle one-way - which is BUG-133 all over again.
//! Only the round trip can answer any of those, so this drives the same accessor pair ResetSeen's
//! PersistSettings() drives, in the same order, against the same engine container.
//!
//! WHAT IT DOES NOT PIN: ResetSeen() itself. OVT_TutorialComponent lives on a per-player
//! OVT_OverthrowController that this world has none of - the component cannot be constructed without
//! an IEntityComponentSource - so the case mirrors its sequence rather than calling it. A change to
//! ResetSeen's ORDERING would not turn this red. That is stated here rather than papered over.
//!
//! PROFILE HYGIENE, and the phase machine that pays for it: identical to
//! OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips above. The ids are in the same illegal-by-scheme
//! namespace, the profile is restored on EVERY path including failures, and the restore waits out
//! the SaveUserSettings() throttle first - a second flush inside that window is DROPPED, not
//! deferred (measured 2026-08-07), so an immediate cleanup write would never reach disk and this
//! case's ids would sit in the CI profile forever.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Tutorial_ResetRestoresTips : SCR_AutotestCaseBase
{
	//! Test-only id. Leading underscores are illegal in the authored entry-id scheme, so this cannot
	//! collide with real content, ever.
	static const string TEST_ID = "__ovt-selftest-reset";

	//! How long to wait after this case's writes before the cleanup write, in milliseconds.
	static const int FLUSH_SETTLE_MS = 10000;

	//! 0 = not started, 1 = written and waiting out the flush throttle, 2 = cleaned up.
	protected int m_iPhase;

	//! Tick at which the last write happened.
	protected int m_iWriteTick;

	//! The verdict, held across frames while the throttle window drains.
	protected string m_sFailure;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
		{
			m_sFailure = RunReset();
			m_iWriteTick = System.GetTickCount();
			m_iPhase = 1;
			return false;
		}

		if (m_iPhase == 1)
		{
			if (System.GetTickCount() - m_iWriteTick < FLUSH_SETTLE_MS)
				return false;

			m_iPhase = 2;
		}

		string cleanupFailure = RestoreProfile();

		if (m_sFailure == "")
			m_sFailure = cleanupFailure;

		if (m_sFailure != "")
		{
			SetResultFailure("%1", m_sFailure);
			return true;
		}

		Print("Tutorial reset cleared a seeded seen id and re-enabled tips through the engine user-settings container, and the profile was restored");
		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Seeds the exact state BUG-133 leaves behind, then performs the reset the way ResetSeen() does.
	//! \return A ready-to-report failure message, or an empty string when the reset held.
	protected string RunReset()
	{
		// 1. The state a player is stuck in: one tip read, and tips switched off for good.
		OVT_TutorialSeenStore seeded = new OVT_TutorialSeenStore();
		seeded.MarkSeen(TEST_ID);

		if (!OVT_TutorialSettingsAccessor.Save(seeded, true))
			return "OVT_TutorialSettingsAccessor.Save() reported the settings store unavailable while seeding. Either the engine has no OVT_TutorialSettings module or this run is a console app, in which case the case is being run in the wrong place.";

		// 2. THE RESET, in ResetSeen()'s order. The load comes FIRST and through a fresh store,
		//    exactly as GetSeenStore() does on a component that has not touched the profile yet; only
		//    then is the set cleared and the flag put back to its default.
		OVT_TutorialSeenStore store = new OVT_TutorialSeenStore();
		bool tipsDisabled;

		if (!OVT_TutorialSettingsAccessor.Load(store, tipsDisabled))
			return "OVT_TutorialSettingsAccessor.Load() reported the settings store unavailable immediately after a successful Save()";

		// The seed has to be visible at this point, or the case would pass without ever having had
		// anything to clear.
		if (!store.HasSeen(TEST_ID))
			return "The seeded id '" + TEST_ID + "' was not readable back before the reset, so this case never had any progress to clear and proves nothing";

		if (!tipsDisabled)
			return "The seeded 'Don't show tips again' flag was not readable back before the reset, so this case never reproduced the state BUG-133 is about";

		store.Clear();

		if (!OVT_TutorialSettingsAccessor.Save(store, false))
			return "OVT_TutorialSettingsAccessor.Save() reported the settings store unavailable while writing the reset";

		// 3. Read the whole record back through a FRESH store: nothing just cleared can be answered
		//    out of the instance above.
		OVT_TutorialSeenStore reloaded = new OVT_TutorialSeenStore();
		bool tipsDisabledAfter;

		if (!OVT_TutorialSettingsAccessor.Load(reloaded, tipsDisabledAfter))
			return "OVT_TutorialSettingsAccessor.Load() reported the settings store unavailable immediately after the reset was written";

		if (reloaded.Count() != 0)
			return "The reset left " + reloaded.Count().ToString() + " seen ids in the profile, expected 0. Clearing the store does not survive the settings round trip, so 'Turn Tips Back On' would clear the record in memory and then have every id written straight back - the tips would still never reappear (BUG-133).";

		if (reloaded.HasSeen(TEST_ID))
			return "The reset left the id '" + TEST_ID + "' in the profile";

		if (tipsDisabledAfter)
			return "The reset wrote m_bTipsDisabled false and it came back true. The flag cannot be written back to its default, so 'Don't show tips again' is a one-way door and BUG-133 is not fixed.";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the profile block back the way this case found it, and PROVES it went back.
	//! \return A ready-to-report failure message, or an empty string when the profile came back clean.
	protected string RestoreProfile()
	{
		if (!OVT_TutorialSettingsAccessor.Reset())
			return "OVT_TutorialSettingsAccessor.Reset() reported the settings store unavailable, so this case's test id may still be in the profile";

		OVT_TutorialSeenStore afterReset = new OVT_TutorialSeenStore();
		bool tipsDisabled;
		OVT_TutorialSettingsAccessor.Load(afterReset, tipsDisabled);

		if (afterReset.Count() != 0)
			return "Reset() left " + afterReset.Count().ToString() + " ids in the profile, so this case is polluting every later run";

		if (tipsDisabled)
			return "Reset() left m_bTipsDisabled set, so this case is polluting every later run";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Every shipped job config carries a unique, well-formed STABLE ID, and the id <-> index mapping the
//! save format is about to be built on actually round-trips.
//!
//! WHY THIS IS THE ONE GUARD THE MIGRATION CANNOT DO WITHOUT. OVT_Job.jobIndex is a POSITION in
//! OVT_JobManagerComponent.m_aJobConfigs and it is persisted. Reordering or trimming that list
//! silently reattaches saved records to DIFFERENT jobs - the record stays in range, the stage index
//! stays valid, FindRestorableJobConfig() accepts it, and the wrong reward is paid. No error, no log
//! line, no crash. OVT_JobConfig.m_sId (decision D1) is what removes that trap, and an id is only
//! worth anything if it is present, unique and stable. This case is where "present and unique" is
//! enforced; "stable" is enforced by the legacy branch below.
//!
//! THE LEGACY BRANCH IS A RENAME GUARD, not a duplicate of the branch above it. The seven surviving
//! ids are written here as literals because they are HISTORY: they are the names a version-1 save
//! already wrote to disk. Rename raise-support in Configs/Jobs/raiseSupport.conf and this case goes
//! red naming it - which is the entire point, because the alternative is a shipped campaign losing
//! that job on its next load with nothing said.
//!
//! THE RETIRED HALF IS LIVE since 2026-08-09: the five starter jobs are deleted, so their ids must now
//! resolve to nothing, permanently. See RETIRED_IDS_ARE_DELETED for how that transition was forced.
//!
//! Ids are checked for SHAPE (lowercase kebab) as well as presence: the convention is shared with
//! OVT_TutorialEntryConfig.m_sId, ids appear verbatim in the drop WARNINGs a bug reporter will paste,
//! and an id with a capital or a space is the kind of thing that only bites once it is unchangeable.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve : SCR_AutotestCaseBase
{
	//! THE TRANSITION HAS BEEN MADE - flipped to true on 2026-08-09 by starter-jobs-retirement task 4.5,
	//! in the same change that deleted the five configs. Nothing further is pending here.
	//!
	//! It reads as a switch because that is how the deletion was made impossible to do quietly. Phases
	//! 1-3 deliberately kept all twelve configs alive - the version-1 payload conversion had to be
	//! exercisable against live configs before those configs went - and while this was false the case
	//! asserted the OPPOSITE of what it asserts now: every retired id had to STILL resolve. So deleting
	//! the configs without flipping it went red naming this constant (and did, in a run that overlapped
	//! the deletion), and flipping it before the deletion went red on the first retired id that still
	//! resolved. Neither state could ever pass silently, which a commented-out block would have allowed.
	//!
	//! Now that it is true this is a PERMANENT REGRESSION GUARD, not a spent one: it asserts that none of
	//! the five retired ids ever comes back. Re-adding a config carrying one of them would make a
	//! version-1 save's dropped records start resolving again, onto a job that is not the job they were
	//! saved on. Leave it true.
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
	//! PHASE 2 DECIDED: THESE LISTS STAY LITERAL AND STAY HERE. They are NOT pointed at
	//! OVT_JobManagerSerializer.LEGACY_V1_JOB_IDS, which now exists. Three reasons, in order of weight:
	//!
	//!  1. THEY ARE DIFFERENT INVARIANTS THAT HAPPEN TO SHARE STRINGS TODAY. LEGACY_V1_JOB_IDS is
	//!     POSITIONAL HISTORY - index -> id for the version 1 save format, frozen for good. These two
	//!     lists are expectations about the config list that is SHIPPING NOW, partitioned by fate. Add
	//!     a thirteenth job one day and the frozen table must NOT grow (it records what version 1 was)
	//!     while SURVIVING_LEGACY_IDS should, so the new job gets the same rename guard. Wiring one to
	//!     the other would make that impossible without unpicking it again.
	//!  2. AN INDEPENDENT WITNESS IS THE WHOLE VALUE OF A GUARD. The realistic mistake is renaming an
	//!     id in a .conf and then "keeping the table in sync" - the frozen table looks like
	//!     configuration until its header is read. With two copies that goes red here, naming the id.
	//!     With one copy it passes, and the rename reaches players as a silently emptied job board.
	//!  3. THE FROZEN TABLE IS NOT LEFT UNGUARDED. Phase 3's Logic-tier case pins LEGACY_V1_JOB_IDS
	//!     against its own literals, world-free. So: that case guards the table, this case guards the
	//!     configs, and neither is asserted against the thing it is checking.
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
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if (!jobs)
		{
			SetResultFailure("OVT_Global.GetJobs() is null - see OVT_TEST_Init_Globals_ManagersResolve for the wiring this depends on.");
			return true;
		}

		int configCount = jobs.GetJobConfigCount();
		if (configCount < 1)
		{
			SetResultFailure("The job manager has no job configs at all (GetJobConfigCount() = %1) - m_aJobConfigs on Prefabs/GameMode/OVT_OverthrowGameMode.et is empty, so every assertion below would pass vacuously", configCount.ToString());
			return true;
		}

		string failure = FindFirstIdError(jobs, configCount);
		if (failure == "")
			failure = FindFirstLegacyResolveError(jobs);

		if (failure != "")
		{
			SetResultFailure("%1", failure);
			return true;
		}

		PrintFormat("Job stable ids: %1 configs, all non-empty, lowercase-kebab, unique and index<->id round-tripping; %2 surviving legacy ids resolve; retired-ids-deleted switch is %3",
			configCount.ToString(), SURVIVING_LEGACY_IDS.Count().ToString(), RETIRED_IDS_ARE_DELETED.ToString());

		SetResultSuccess();
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
