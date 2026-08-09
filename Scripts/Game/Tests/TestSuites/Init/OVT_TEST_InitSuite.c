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
