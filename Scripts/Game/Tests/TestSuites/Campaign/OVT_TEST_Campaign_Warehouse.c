//------------------------------------------------------------------------------------------------
//! A BUILT warehouse is registered exactly like a PURCHASED one - and the town-control gate refuses
//! it in an occupied town, on the server, not just in the menu.
//!
//! WHAT IS AT STAKE. "Afterwards it is indistinguishable from a warehouse you bought" is the whole
//! promise of the buildable warehouse, and it rests on three things nothing else in the mod re-states:
//! the prefab's PATH contains "Warehouse_01" so OVT_RealEstateManagerComponent.GetConfig matches it by
//! substring, its root class is SCR_DestructibleBuildingEntity so GetConfig's hard class gate lets it
//! through, and SetOwnerPersistentId is actually called on it after the build. Break any one and the
//! building still spawns, still holds items and resources, and is invisible to the map, to ownership,
//! to rent, to the warehouse access rule and to the vehicle menu - with nothing in the log.
//!
//! ZERO LINES OF OVT_RealEstateManagerComponent CHANGED FOR THIS. It is the shipped purchase path,
//! reached with a different owner, which is exactly why the assertions below are its own public
//! answers (GetConfig, GetNearestWarehouse, PlayerMayUseWarehouse) and the map's own PopulateLocations
//! rather than anything this feature wrote.
//!
//! THE GATE IS ASSERTED IN BOTH DIRECTIONS, and that is what makes either half mean anything. The two
//! BuildItem() calls are identical - same index, same position, same player, same wallet - and the
//! ONLY thing that changes between them is town.faction. A refusal alone would also be produced by a
//! funds, distance or item-limit failure; a refusal followed by a success at the same spot cannot be.
//!
//! WHY town.faction IS WRITTEN DIRECTLY. OVT_TownManagerComponent.ChangeTownControl fires three
//! subscribers (the objective director, the occupying faction manager, the tutorial manager) and two
//! notification broadcasts per call, and this case makes three changes. The field is public data, it
//! is put back to what it was before the case returns, and nothing here is asserting anything about
//! town control itself.
//!
//! THE BUILDING IS LEFT STANDING. It sorts last in this suite, deleting a persistence-tracked entity
//! mid-suite drives the transient-untrack retry queue (BUG-118), and the ownership and warehouse
//! records it leaves behind are exactly what buying a warehouse leaves behind. The money is refunded.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 60)]
class OVT_TEST_Campaign_Warehouse_BuiltRegistersLikeAPurchasedOne : SCR_AutotestCaseBase
{
	//! Resolved by name out of buildables.conf. Never an index - entries get appended.
	static const string BUILDABLE_NAME = "Warehouse";

	//! The substring OVT_RealEstateManagerComponent.GetConfig matches a warehouse on. THE prefab-naming
	//! contract of this whole design, asserted below rather than assumed.
	static const string WAREHOUSE_PREFAB_FRAGMENT = "Warehouse_01";

	//! Where the site goes, relative to the player. Inside BuildItem's 250 m distance check by
	//! construction, and clear of the body so the building does not spawn on top of it.
	static const vector BUILD_OFFSET = "0 0 -35";

	//! Where the supplying crate pile goes, relative to the site. Well inside the manager's 30 m
	//! supply radius.
	static const vector PILE_OFFSET = "5 0 0";

	//! Metres a map location may sit from the building and still be that building's marker.
	static const float MAP_MATCH_TOLERANCE = 10;

	static const int PHASE_REFUSE_IN_OCCUPIED_TOWN = 0;
	static const int PHASE_PLACE_SITE = 1;
	static const int PHASE_SUPPLY = 2;
	static const int PHASE_COMPLETE = 3;
	static const int PHASE_ASSERT = 4;

	protected int m_iPhase;

	protected int m_iPlayerId;
	protected string m_sPersId;
	protected int m_iBuildableIndex;
	protected vector m_vBuildPos;
	protected int m_iCost;
	protected int m_iSpent;

	protected OVT_TownData m_Town;
	protected int m_iOriginalTownFaction;
	protected bool m_bTownFactionChanged;

	protected IEntity m_Site;
	protected IEntity m_Building;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_REFUSE_IN_OCCUPIED_TOWN)
			return RefuseInOccupiedTown();

		if (m_iPhase == PHASE_PLACE_SITE)
			return PlaceSite();

		if (m_iPhase == PHASE_SUPPLY)
			return Supply();

		if (m_iPhase == PHASE_COMPLETE)
			return Complete();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves everything the case needs, then asserts the server half of the town-control gate.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool RefuseInOccupiedTown()
	{
		string diagnostic = ResolvePlayer();
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = ResolveBuildable();
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = ResolveTownAndPosition();
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();

		int occupyingFaction = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		SetTownFaction(occupyingFaction);

		int moneyBefore = economy.GetPlayerMoney(m_sPersId);

		IEntity refused = resistance.BuildItem(m_iBuildableIndex, 0, m_vBuildPos, vector.Zero, m_iPlayerId);
		if (refused)
		{
			SetFailure("BuildItem() placed a Warehouse inside a town the occupying faction holds. The server-side town-control gate is missing or is not reached, so a client that skips OVT_BuildContext.CanBuild can build anywhere.");
			SCR_EntityHelper.DeleteEntityAndChildren(refused);
			Restore();
			return true;
		}

		int moneyAfterRefusal = economy.GetPlayerMoney(m_sPersId);
		if (moneyAfterRefusal != moneyBefore)
		{
			SetFailure(string.Format("The refused build still took %1 from the player. A refusal must charge nothing - the money is taken when the site is placed, and no site was placed.",
				(moneyBefore - moneyAfterRefusal).ToString()));
			Restore();
			return true;
		}

		m_iPhase = PHASE_PLACE_SITE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The same call in a resistance-held town, which must now place a construction site.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool PlaceSite()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();

		SetTownFaction(OVT_Global.GetConfig().GetPlayerFactionIndex());

		int moneyBefore = economy.GetPlayerMoney(m_sPersId);

		m_Site = resistance.BuildItem(m_iBuildableIndex, 0, m_vBuildPos, vector.Zero, m_iPlayerId);
		if (!m_Site)
		{
			SetFailure(string.Format("BuildItem() built nothing in a resistance-held town at %1. The town-control gate refuses both ways, or the build was refused for funds, distance or the item limit - the player held %2 and the Warehouse costs %3.",
				m_vBuildPos.ToString(), moneyBefore.ToString(), m_iCost.ToString()));
			Restore();
			return true;
		}

		m_iSpent = moneyBefore - economy.GetPlayerMoney(m_sPersId);

		if (!OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(m_Site))
		{
			SetFailure("BuildItem() returned something with no OVT_ConstructionSiteComponent for a buildable that authors resource requirements. It built the warehouse outright, so the resource half of its price was never charged.");
			Restore();
			return true;
		}

		m_iPhase = PHASE_SUPPLY;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops one crate pile beside the site holding exactly what the site needs.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Supply()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null, so no pile can be dropped and the site can never be finished");
			Restore();
			return true;
		}

		OVT_Buildable buildable = OVT_Global.GetResistanceFaction().GetBuildableAt(m_iBuildableIndex);
		if (!buildable || !buildable.m_aResourceRequirements || buildable.m_aResourceRequirements.IsEmpty())
		{
			SetFailure("The Warehouse entry in buildables.conf authors no resource requirements, so there is nothing to supply and the site branch this case exercises would never be taken");
			Restore();
			return true;
		}

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		OVT_ResourceRequirements.ScaleForDifficulty(buildable.m_aResourceRequirements, need);
		if (need.IsEmpty())
		{
			SetFailure("Scaling the Warehouse's requirements produced an empty list - every authored id is unknown to resources.conf, or every quantity scaled to nothing");
			Restore();
			return true;
		}

		IEntity pile = resources.SpawnOrMergePile(m_Site.GetOrigin() + PILE_OFFSET, need);
		if (!pile)
		{
			SetFailure("SpawnOrMergePile() dropped no crate pile beside the site, so the requirements can never be met");
			Restore();
			return true;
		}

		m_iPhase = PHASE_COMPLETE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns the site into the building through the shipped completion path.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Complete()
	{
		string reason;
		m_Building = OVT_Global.GetResistanceFaction().CompleteSite(m_Site, m_iPlayerId, reason);
		if (!m_Building)
		{
			SetFailure(string.Format("CompleteSite() built no warehouse and answered '%1'. The pile beside the site holds exactly the scaled requirement, so a refusal here means the availability sweep is not seeing it.", reason));
			Restore();
			return true;
		}

		m_Site = null;

		m_iPhase = PHASE_ASSERT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case ends here either way.
	protected bool Assert()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetFailure("OVT_Global.GetRealEstate() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			Restore();
			return true;
		}

		// Claim 1 - the prefab is one real estate recognises AT ALL. Both halves of GetConfig's rule:
		// the SCR_DestructibleBuildingEntity class gate and the "Warehouse_01" substring.
		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(m_Building);
		if (prefab.IndexOf(WAREHOUSE_PREFAB_FRAGMENT) == -1)
		{
			SetFailure(string.Format("The built warehouse's prefab is '%1', whose path does not contain '%2'. Real estate matches a warehouse by that substring and nothing else, so moving or renaming this prefab out of its Warehouse_01 folder silently stops it being a warehouse.",
				prefab, WAREHOUSE_PREFAB_FRAGMENT));
			Restore();
			return true;
		}

		OVT_RealEstateConfig config = realEstate.GetConfig(m_Building);
		if (!config)
		{
			SetFailure(string.Format("OVT_RealEstateManagerComponent.GetConfig() matched no real-estate config for '%1'. Its class is '%2' and GetConfig refuses anything that is not exactly SCR_DestructibleBuildingEntity, so either the class gate or the filter list has stopped recognising it.",
				prefab, m_Building.ClassName()));
			Restore();
			return true;
		}

		if (!config.m_IsWarehouse)
		{
			SetFailure(string.Format("The built warehouse matched the real-estate config '%1', which is not flagged m_IsWarehouse. It would be ownable property but never a warehouse: no stock access rule, no warehouse marker, no vehicle-menu buttons.", config.m_Name));
			Restore();
			return true;
		}

		// Claim 2 - the record SetOwnerPersistentId creates exists and names the builder.
		OVT_WarehouseData record = realEstate.GetNearestWarehouse(m_Building.GetOrigin(), OVT_RealEstateManagerComponent.WAREHOUSE_MATCH_RANGE);
		if (!record)
		{
			SetFailure("No OVT_WarehouseData record exists at the built warehouse. FinishBuild() never called OVT_RealEstateManagerComponent.SetOwnerPersistentId on it, so it is a building with two ledgers that no warehouse consumer in the mod can see.");
			Restore();
			return true;
		}

		if (record.owner != m_sPersId)
		{
			SetFailure(string.Format("The built warehouse's record is owned by '%1', expected the builder '%2'", record.owner, m_sPersId));
			Restore();
			return true;
		}

		if (record.isPrivate)
		{
			SetFailure("The built warehouse came out PRIVATE. A built warehouse is public (D14), so only the builder could open it and every other player on the server would be refused.");
			Restore();
			return true;
		}

		// Claim 3 - it is on the map, through the map's own code rather than a restatement of it.
		string mapDiagnostic = AssertOnTheMap();
		if (mapDiagnostic != "")
		{
			SetFailure(mapDiagnostic);
			Restore();
			return true;
		}

		// Claim 4 - the access rule every warehouse screen and the vehicle menu ask.
		if (!realEstate.PlayerMayUseWarehouse(m_sPersId, m_Building))
		{
			SetFailure(string.Format("PlayerMayUseWarehouse() refuses the BUILDER of the warehouse. The record exists and names '%1', so ownership was written to the warehouse record but not to the owner manager - the two halves of SetOwnerPersistentId have come apart.", m_sPersId));
			Restore();
			return true;
		}

		// The two ledgers a warehouse is for. Their PERSISTENCE is the round-trip suite's claim; that
		// they are on the building at all is this one's, because a buildable variant that lost the
		// delta would still register perfectly and hold nothing.
		if (!OVT_ResourceUtils.GetStore(m_Building))
		{
			SetFailure("The built warehouse has no OVT_ResourceStoreComponent. It inherits one from the Warehouse_01_Base delta, so an absent one means the new prefab is not descending from it.");
			Restore();
			return true;
		}

		if (!OVT_StorageUtils.GetStorage(m_Building))
		{
			SetFailure("The built warehouse has no OVT_StorageComponent, so it registers as a warehouse and then cannot hold a single item");
			Restore();
			return true;
		}

		Print("A BUILT warehouse is registered exactly like a purchased one: real estate matches its prefab, it has an OVT_WarehouseData record owned by the builder and public, it is on the map, PlayerMayUseWarehouse admits the builder, and it carries both ledgers");

		Restore();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the map's own warehouse layer and looks for the building in what it produced.
	//! \return An empty string when the building is on the map, otherwise the sentence to fail with.
	protected string AssertOnTheMap()
	{
		OVT_MapLocationWarehouse layer = new OVT_MapLocationWarehouse();
		layer.Init(null);

		array<ref OVT_MapLocationData> locations = new array<ref OVT_MapLocationData>();
		layer.PopulateLocations(locations);

		foreach (OVT_MapLocationData location : locations)
		{
			if (!location)
				continue;

			if (vector.Distance(location.m_vPosition, m_Building.GetOrigin()) <= MAP_MATCH_TOLERANCE)
				return "";
		}

		return string.Format("OVT_MapLocationWarehouse produced %1 warehouse marker(s) and none of them is at the built warehouse. The ownership entry real estate keyed by position is missing, or GetNearestBuilding cannot find the building from it, so the player has no way to see on the map the warehouse they just paid for.",
			locations.Count().ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string ResolvePlayer()
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return "OVT_Global.GetPlayers() is null - see OVT_TEST_Init_Globals_ManagersResolve";

		if (!OVT_Global.GetEconomy())
			return "OVT_Global.GetEconomy() is null - see OVT_TEST_Init_Globals_ManagersResolve";

		if (!OVT_Global.GetResistanceFaction())
			return "OVT_Global.GetResistanceFaction() is null - see OVT_TEST_Init_Globals_ManagersResolve";

		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		if (connected.IsEmpty())
			return "No player is connected - the autotest client normally spawns a local player (playerId 1), and a build needs a builder with a persistent id";

		m_iPlayerId = connected[0];
		m_sPersId = players.GetPersistentIDFromPlayerID(m_iPlayerId);
		if (m_sPersId == "")
			return string.Format("The player manager has no persistent ID for playerId %1, so there is nobody to own the warehouse", m_iPlayerId.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string ResolveBuildable()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
			return "The resistance faction has no buildables config loaded";

		m_iBuildableIndex = -1;
		for (int i = 0; i < resistance.m_BuildablesConfig.m_aBuildables.Count(); i++)
		{
			OVT_Buildable candidate = resistance.m_BuildablesConfig.m_aBuildables[i];
			if (candidate && candidate.m_sName == BUILDABLE_NAME)
			{
				m_iBuildableIndex = i;
				break;
			}
		}

		if (m_iBuildableIndex < 0)
			return string.Format("No buildable named '%1' in buildables.conf - the entry is missing or renamed", BUILDABLE_NAME);

		OVT_Buildable buildable = resistance.GetBuildableAt(m_iBuildableIndex);
		if (!buildable.m_bBuildInTown)
			return "The Warehouse buildable does not set m_bBuildInTown, so the town-control gate this case exercises is never reached for it";

		if (!buildable.m_aPrefabs || buildable.m_aPrefabs.IsEmpty())
			return "The Warehouse buildable lists no prefab";

		// Asserted on the CONFIG as well as on the built entity: a conf pointed at some other building
		// would fail the entity check below with a much less obvious message.
		if (buildable.m_aPrefabs[0].IndexOf(WAREHOUSE_PREFAB_FRAGMENT) == -1)
		{
			return string.Format("The Warehouse buildable's prefab is '%1', whose path does not contain '%2'. Real estate recognises a warehouse by that substring alone.",
				buildable.m_aPrefabs[0], WAREHOUSE_PREFAB_FRAGMENT);
		}

		m_iCost = OVT_Global.GetConfig().GetBuildableCost(buildable);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the build position and checks the town-control gate will actually be engaged there.
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string ResolveTownAndPosition()
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!character)
			return string.Format("Player %1 controls no entity, so there is nowhere within BuildItem's 250 m distance check to put a warehouse", m_iPlayerId.ToString());

		m_vBuildPos = character.GetOrigin() + BUILD_OFFSET;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			m_vBuildPos[1] = world.GetSurfaceY(m_vBuildPos[0], m_vBuildPos[2]);

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return "OVT_Global.GetTowns() is null, so the town-control gate has nothing to read";

		m_Town = towns.GetNearestTown(m_vBuildPos);
		if (!m_Town)
			return "The world has no towns, so the town-control gate is never engaged and this case would assert nothing";

		if (m_Town.size == 1)
			return "The nearest town to the build position is a VILLAGE. The town-control gate deliberately excludes villages, so this case cannot exercise it here.";

		int range = towns.m_iCityRange;
		if (m_Town.size < 3)
			range = towns.m_iTownRange;

		float distance = vector.Distance(m_Town.location, m_vBuildPos);
		if (distance >= range)
		{
			return string.Format("The build position is %1 m from the town centre, outside its %2 m build range, so the town-control gate is not engaged there and the refusal half of this case would pass for the wrong reason.",
				distance.ToString(), range.ToString());
		}

		m_iOriginalTownFaction = m_Town.faction;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] faction The faction index the town should read as.
	protected void SetTownFaction(int faction)
	{
		if (!m_Town)
			return;

		m_Town.faction = faction;
		m_bTownFactionChanged = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts back everything this case changed. Safe to call from any step.
	protected void Restore()
	{
		if (m_bTownFactionChanged && m_Town)
		{
			m_Town.faction = m_iOriginalTownFaction;
			m_bTownFactionChanged = false;
		}

		// A site that never became a building would otherwise stand in the world holding the money.
		if (m_Site)
		{
			OVT_PersistenceTracking.Untrack(m_Site, false);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Site);
			m_Site = null;
		}

		if (m_iSpent > 0)
		{
			OVT_Global.GetEconomy().AddPlayerMoney(m_iPlayerId, m_iSpent);
			m_iSpent = 0;
		}
	}
}
