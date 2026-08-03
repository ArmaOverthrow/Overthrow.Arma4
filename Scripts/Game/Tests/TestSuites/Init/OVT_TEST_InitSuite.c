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
