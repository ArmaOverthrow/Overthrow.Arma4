//------------------------------------------------------------------------------------------------
//! TIER B - the production seam: the manager is actually ON the game mode prefab, its world query
//! actually found the test world's Sawmill, and the drip actually reaches that site's ledger.
//!
//! WHY THIS FILE EXISTS. Production reaches the game through four text edits with no compile-time
//! link: an OVT_ResourceProductionManagerComponent block on OVT_OverthrowGameMode.et, a
//! Prefabs/Production/OVT_ProductionSite_Sawmill.et instance in the test world layer, the
//! OVT_ResourceProductionComponent + OVT_ResourceStoreComponent pair on that prefab, and an authored
//! resource id that resources.conf has to recognise. Break any one of them and the mod produces no
//! error at all - every site simply drips nothing, forever, and no log line says why.
//!
//! ONE CLAIM PER CASE:
//!   A. the manager resolves through OVT_Global.GetProduction()
//!   B. the test world's Sawmill is discovered as exactly one unowned record
//!   C. that record's entity carries a 20 000 litre store and a production component whose resource
//!      id the catalogue knows
//!   D. one hand-driven hour raises that site's ledger by the authored rate
//!   E. a second CheckProduction() in the same in-game hour adds nothing
//!   F. position matching is bounded - a nearby position names the site, a distant one does not
//!   G. OVT_ResourceProductionRequestComponent sits on THIS player's controller entity
//!   H. a server-side buy sets the owner, empties the stock and takes exactly the quoted price
//!   I. the five access states resolve through OVT_ResourceRequestComponent.MayUseHolder the same
//!      way OVT_ResourceProductionRules.MayAccessStore answers them - Phase 5's window (D13)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Shared subject resolution for the cases below.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ProductionSeamSubject
{
	//! The test world layer carries exactly one production site (a Sawmill). Adding a second is a
	//! deliberate act and should update this number with it.
	static const int EXPECTED_SITE_COUNT = 1;

	//! What OVT_ProductionSite_Base.et authors as m_fCargoVolume, in litres.
	static const int EXPECTED_CAPACITY_LITRES = 20000;

	//! Frame polls allowed for the game mode's components to post-init and for the world query to run.
	static const int MAX_POLLS = 300;

	//------------------------------------------------------------------------------------------------
	//! Resolves the manager with a populated site list, or explains which text edit is missing.
	//! \param[out] manager The manager; untouched when it did not resolve.
	//! \param[out] failure A diagnosis when it did not; "" otherwise.
	//! \return True when the manager AND at least one discovered site exist.
	static bool ResolveManager(out OVT_ResourceProductionManagerComponent manager, out string failure)
	{
		failure = "";

		OVT_ResourceProductionManagerComponent found = OVT_Global.GetProduction();
		if (!found)
		{
			failure = "OVT_Global.GetProduction() is null. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_ResourceProductionManagerComponent block, so no site anywhere produces anything.";
			return false;
		}

		array<ref OVT_ProductionSiteData> sites = found.GetSites();
		if (!sites)
		{
			failure = "OVT_ResourceProductionManagerComponent.GetSites() returned a null array - the manager's constructor did not run.";
			return false;
		}

		if (sites.IsEmpty())
		{
			failure = "The production manager discovered NO sites. Either Worlds/MP/OVT_Campaign_Test_Layers/default.layer has lost its OVT_ProductionSite_Sawmill instance, or that prefab no longer carries an OVT_ResourceProductionComponent, or its entity flags no longer answer an EQueryEntitiesFlags.STATIC query.";
			return false;
		}

		manager = found;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The one site's record, with its entity resolved.
	//! \param[in] manager The manager.
	//! \param[out] site The record; untouched on failure.
	//! \param[out] failure A diagnosis when it did not resolve; "" otherwise.
	//! \return True when the record and its entity both exist.
	static bool ResolveSite(notnull OVT_ResourceProductionManagerComponent manager, out OVT_ProductionSiteData site, out string failure)
	{
		failure = "";

		array<ref OVT_ProductionSiteData> sites = manager.GetSites();

		OVT_ProductionSiteData found = sites[0];
		if (!found)
		{
			failure = "The production manager holds a null site record.";
			return false;
		}

		if (!found.entity)
		{
			failure = "The discovered site record carries no entity, so nothing can ever be produced into it.";
			return false;
		}

		site = found;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] failure "" when this machine is the authority, a diagnosis otherwise.
	//! \return True when the drip may be driven by hand from here.
	static bool RequireAuthority(out string failure)
	{
		failure = "";

		if (Replication.IsServer())
			return true;

		failure = "This test machine is not the replication authority, so every production call would refuse at its Replication.IsServer() guard and the case would pass by doing nothing.";
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The manager resolves off the game mode and carries a site list.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_AManagerResolves : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		PrintFormat("Production seam: the manager resolves with %1 discovered site(s)", manager.GetSites().Count().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The test world's Sawmill is discovered as exactly one record, and it starts unowned.
//!
//! Unowned is the load-bearing half: an owner arriving from discovery rather than from a purchase
//! would make every site private to somebody on the first frame, and the shop actions would never
//! appear on any of them.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_BSawmillIsDiscovered : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		array<ref OVT_ProductionSiteData> sites = manager.GetSites();

		if (sites.Count() != OVT_TEST_ProductionSeamSubject.EXPECTED_SITE_COUNT)
		{
			SetFailure("The world query discovered %1 production site(s), expected %2. Either the test world layer gained one without this case being updated, or one prefab is being counted twice.",
				sites.Count().ToString(),
				OVT_TEST_ProductionSeamSubject.EXPECTED_SITE_COUNT.ToString());
			return true;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		if (site.owner != "")
		{
			SetFailure("The discovered site is already owned by '%1'. Discovery must produce an UNOWNED record - ownership only ever arrives from a purchase, the JIP stream or a save.", site.owner);
			return true;
		}

		// Discovery writes 0; every batch after it re-derives the carry from a fresh sum. The INVARIANT
		// is what is asserted, not the initial value, so this holds whatever order the cases run in.
		if (site.carry < 0 || site.carry >= 1)
		{
			SetFailure("The site's fractional carry is %1, which is outside [0, 1). A carry that grows without bound would make a sub-1 rate produce in bursts and then stop.", site.carry.ToString());
			return true;
		}

		PrintFormat("Production seam: one unowned site discovered at %1", site.location.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The site's entity carries the store and the production component the drip needs, and the resource
//! it is authored to make is one the catalogue actually knows.
//!
//! An id resources.conf does not carry produces no error anywhere: IndexOf() answers -1, the batch
//! returns early, and the site drips nothing for the whole campaign.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_CSiteCarriesStoreAndComponent : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site.entity);
		if (!production)
		{
			SetFailure("The discovered site entity carries no OVT_ResourceProductionComponent, which is impossible unless the record was built by something other than discovery.");
			return true;
		}

		if (!RplComponent.Cast(site.entity.FindComponent(RplComponent)))
		{
			SetFailure("OVT_ProductionSite_Base.et carries no RplComponent, so the site's store has no RplId and its stock can never reach a client (BUG-193's shape).");
			return true;
		}

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
		{
			SetFailure("The site entity carries no usable OVT_ResourceStoreComponent, so there is nowhere to put what it produces. Check m_fCargoVolume on OVT_ProductionSite_Base.et - a volume of 0 means 'no store at all'.");
			return true;
		}

		if (store.GetCapacityLitres() != OVT_TEST_ProductionSeamSubject.EXPECTED_CAPACITY_LITRES)
		{
			SetFailure("The site's store holds %1 litres, expected %2. Capacity is the only throttle production has - an unlimited site would accumulate a whole campaign's stock.",
				store.GetCapacityLitres().ToString(),
				OVT_TEST_ProductionSeamSubject.EXPECTED_CAPACITY_LITRES.ToString());
			return true;
		}

		if (production.GetUnitsPerHour() <= 0)
		{
			SetFailure("The site is authored at %1 units per hour, so it can never produce anything.", production.GetUnitsPerHour().ToString());
			return true;
		}

		if (production.GetBaseCost() <= 0)
		{
			SetFailure("The site is authored at a base cost of %1, so buying it would be free.", production.GetBaseCost().ToString());
			return true;
		}

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null, so there is no catalogue to check the site's resource id against.");
			return true;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs || defs.IndexOf(production.GetResourceId()) == -1)
		{
			SetFailure("The site is authored to produce '%1', which resources.conf does not define. Every batch would return early and the site would drip nothing for the whole campaign.", production.GetResourceId());
			return true;
		}

		PrintFormat("Production seam: the site makes '%1' at %2/hour into a %3 litre store",
			production.GetResourceId(),
			production.GetUnitsPerHour().ToString(),
			store.GetCapacityLitres().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One hand-driven in-game hour raises the site's ledger by the authored rate.
//!
//! This is the whole feature in one assertion: the record, the entity, the component, the store, the
//! catalogue lookup and the ledger write all have to line up for the number to move at all.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_DProduceRaisesLedger : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!OVT_TEST_ProductionSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site.entity);
		if (!production)
		{
			SetFailure("The discovered site entity carries no OVT_ResourceProductionComponent.");
			return true;
		}

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
		{
			SetFailure("The site entity carries no usable OVT_ResourceStoreComponent.");
			return true;
		}

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
		{
			SetFailure("The site's store built no ledger, so nothing can be written into it.");
			return true;
		}

		float rate = production.GetUnitsPerHour();
		int expected = Math.Floor(rate);

		if (Math.AbsFloat(rate - expected) > 0.001)
		{
			SetFailure("The test world's site is authored at a FRACTIONAL rate of %1. This case asserts an exact one-hour delta, which only holds for a whole rate - retune the prefab or rewrite the case against the carry.", rate.ToString());
			return true;
		}

		if (expected < 1)
		{
			SetFailure("The test world's site is authored at %1 units per hour, so a one-hour batch would be indistinguishable from no production at all.", rate.ToString());
			return true;
		}

		string id = production.GetResourceId();
		int before = ledger.Count(id);

		manager.ProduceForHours(1);

		int after = ledger.Count(id);

		if (after - before != expected)
		{
			SetFailure("One hour of production moved the site's '%1' line by %2 units, expected %3. The batch reached the store but wrote the wrong amount, or the store was already full.",
				id,
				(after - before).ToString(),
				expected.ToString());
			return true;
		}

		if (store.GetPackedContents() == "")
		{
			SetFailure("The site produced %1 units but its replicated contents are still empty, so no client would ever see the stock. PublishContents() did not run after the ledger write.", expected.ToString());
			return true;
		}

		PrintFormat("Production seam: one hour raised '%1' from %2 to %3", id, before.ToString(), after.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A second CheckProduction() inside the same in-game hour produces nothing.
//!
//! The tick fires far more often than once an in-game hour by design - what makes production hourly
//! is the LATCH, not the period. Both calls happen inside one script frame, so the clock cannot move
//! between them and the assertion is exact.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_EOncePerHour : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!OVT_TEST_ProductionSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site.entity);
		if (!production)
		{
			SetFailure("The discovered site entity carries no OVT_ResourceProductionComponent.");
			return true;
		}

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
		{
			SetFailure("The site entity carries no usable OVT_ResourceStoreComponent.");
			return true;
		}

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
		{
			SetFailure("The site's store built no ledger.");
			return true;
		}

		string id = production.GetResourceId();

		manager.CheckProduction();
		int afterFirst = ledger.Count(id);

		manager.CheckProduction();
		int afterSecond = ledger.Count(id);

		if (afterSecond != afterFirst)
		{
			SetFailure("A second CheckProduction() in the same in-game hour moved '%1' from %2 to %3. The hour latch is not holding, so production runs once per TICK rather than once per hour.",
				id,
				afterFirst.ToString(),
				afterSecond.ToString());
			return true;
		}

		PrintFormat("Production seam: the hour latch held '%1' at %2 across two ticks", id, afterSecond.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Position matching is bounded: a position at the site names it, a position well outside the match
//! range names nothing.
//!
//! Position IS the identity on every wire and in the save, so an unbounded matcher would let any
//! forged coordinate address the nearest site in the world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_FPositionMatchingIsBounded : SCR_AutotestCaseBase
{
	//! Comfortably outside OVT_ResourceProductionManagerComponent.SITE_MATCH_RANGE.
	static const vector FAR_OFFSET = "500 0 500";

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		if (manager.GetSite(site.location) != site)
		{
			SetFailure("GetSite() did not answer the site's own record for its own position, so no RPC and no save record could ever address it.");
			return true;
		}

		if (manager.GetSite(site.location + FAR_OFFSET))
		{
			SetFailure("GetSite() answered a record for a position %1 away from every known site. An unbounded matcher lets a forged coordinate address the nearest site in the world.", FAR_OFFSET.ToString());
			return true;
		}

		if (manager.GetSiteForEntity(site.entity) != site)
		{
			SetFailure("GetSiteForEntity() did not answer the record the entity was discovered into.");
			return true;
		}

		PrintFormat("Production seam: position matching answers at %1 and refuses %2 away", site.location.ToString(), FAR_OFFSET.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ResourceProductionRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! Same failure mode as every other controller component: written, compiled, and simply never added
//! to Prefabs/GameMode/OVT_OverthrowController.et. No compile error, no runtime error, no log line -
//! buying a site and flipping its privacy just never happen. OVT_TEST_Init_ControllerSeam carries the
//! roster entry; this case carries the second claim the roster cannot make, that what resolves is the
//! instance on THIS player's controller entity rather than some other player's seam.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_GRequestComponentResolves : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player's controller to be spawned and registered.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowController controller = OVT_Global.GetController();
		if (!controller)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_ResourceProductionRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceProductionRequestComponent viaAccessor = OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so buying a production site and toggling its privacy silently never happen.");
			return true;
		}

		OVT_ResourceProductionRequestComponent onEntity = OVT_ResourceProductionRequestComponent.Cast(controller.FindComponent(OVT_ResourceProductionRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every site request would then be sent through another player's seam, and the server would resolve the caller as that player.");
			return true;
		}

		PrintFormat("Production seam: OVT_ResourceProductionRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A server-side buy sets the owner, empties the stock and takes exactly the quoted price - all in
//! one call.
//!
//! THE THREE EFFECTS ARE ONE CLAIM ON PURPOSE. "Buying clears the stock" is a rule about ORDERING
//! (requirement 19): the money and the ownership and the destruction of the stock happen together, so
//! nobody can buy a site for the inventory it accumulated while unowned. Asserting them separately
//! would pass on an implementation that charged for the site and left the stock behind.
//!
//! IT DRIVES THE REAL ASK. BuySite() takes its Replication.IsServer() direct branch on this machine,
//! so the whole ladder runs: caller resolution, the position match, MayBuySite, the distance gate, the
//! shared BuyCost evaluation and the charge. Nothing here reaches past the public entry point.
//!
//! TWO PIECES OF ARRANGEMENT, BOTH DELIBERATE:
//!   - the player is FUNDED with exactly the price first, so the case measures the charge rather than
//!     whatever balance the test world happens to start a player with;
//!   - the player is MOVED to the site, because the ask refuses a purchase from more than 30 m away
//!     and the test world's spawn is nowhere near the Sawmill. The move is a PRECONDITION with a named
//!     failure on expiry, and the body is put back before anything is judged.
//!
//! The site is restored to unowned before any assertion runs, so a red case leaves the world the way
//! discovery built it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_HServerBuyClearsStockAndCharges : SCR_AutotestCaseBase
{
	//! Comfortably inside OVT_ResourceProductionRequestComponent.USE_RADIUS.
	static const float ARRIVAL_RADIUS = 20;

	//! Frame polls allowed for the teleported body to report its new position.
	static const int MAX_MOVE_POLLS = 120;

	protected int m_iPolls;
	protected int m_iMovePolls;
	protected int m_iStage;

	protected int m_iPlayerId;
	protected string m_sPersId;
	protected int m_iCost;
	protected int m_iMoneyBefore;
	protected int m_iStockBefore;
	protected vector m_vRestorePos;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!OVT_TEST_ProductionSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		if (m_iStage == 0)
			return Arrange(manager, site);

		return BuyAndJudge(manager, site);
	}

	//------------------------------------------------------------------------------------------------
	//! Funds the purchase, seeds the stock and moves the buyer to the site.
	//! \param[in] manager The production manager.
	//! \param[in] site The one discovered site.
	//! \return True when the case is finished (always a failure at this stage), false to poll again.
	protected bool Arrange(notnull OVT_ResourceProductionManagerComponent manager, notnull OVT_ProductionSiteData site)
	{
		m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null, so the buyer has no identity and no balance to charge.");
			return true;
		}

		m_sPersId = players.GetPersistentIDFromPlayerID(m_iPlayerId);
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);

		if (m_sPersId == "" || !body)
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("The local player had no persistent id or no character after %1 frames (player id %2), so a purchase cannot be attributed to anybody.",
					m_iPolls.ToString(),
					m_iPlayerId.ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site.entity);
		if (!production)
		{
			SetFailure("The discovered site entity carries no OVT_ResourceProductionComponent, so it has no authored price.");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null, so the real-estate cost multiplier cannot be read and the price cannot be predicted.");
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null, so no money can move.");
			return true;
		}

		// The SAME static, with the SAME two inputs, that the action's label and RpcAsk_BuySite use.
		m_iCost = OVT_ResourceProductionRules.BuyCost(production.GetBaseCost(), config.GetRealEstateCostMultiplier());
		if (m_iCost <= 0)
		{
			SetFailure("BuyCost() answered %1 for a base cost of %2 at a multiplier of %3, so the site would be free.",
				m_iCost.ToString(),
				production.GetBaseCost().ToString(),
				config.GetRealEstateCostMultiplier().ToString());
			return true;
		}

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
		{
			SetFailure("The site entity carries no usable OVT_ResourceStoreComponent.");
			return true;
		}

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
		{
			SetFailure("The site's store built no ledger.");
			return true;
		}

		manager.ProduceForHours(1);

		m_iStockBefore = ledger.Count(production.GetResourceId());
		if (m_iStockBefore <= 0)
		{
			SetFailure("The site holds no '%1' even after a hand-driven hour, so 'the purchase destroys the stock' would pass without proving anything.", production.GetResourceId());
			return true;
		}

		economy.AddPlayerMoneyPersistentId(m_sPersId, m_iCost);
		m_iMoneyBefore = economy.GetPlayerMoney(m_sPersId);

		if (m_iMoneyBefore < m_iCost)
		{
			SetFailure("The buyer holds %1 after being funded %2, so the purchase would be refused for want of money rather than tested.",
				m_iMoneyBefore.ToString(),
				m_iCost.ToString());
			return true;
		}

		m_vRestorePos = body.GetOrigin();

		if (!SCR_Global.TeleportPlayer(m_iPlayerId, site.location))
		{
			SetFailure("The buyer could not be moved to the site at %1, so the ask's 30 m distance gate would refuse the purchase and the case would prove nothing.", site.location.ToString());
			return true;
		}

		m_iStage = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the ask, restores the world, then judges what it observed.
	//! \param[in] manager The production manager.
	//! \param[in] site The one discovered site.
	//! \return True when the case is finished, false to poll again.
	protected bool BuyAndJudge(notnull OVT_ResourceProductionManagerComponent manager, notnull OVT_ProductionSiteData site)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!body)
		{
			SetFailure("The buyer lost its character between being moved to the site and buying it.");
			return true;
		}

		if (vector.Distance(body.GetOrigin(), site.location) > ARRIVAL_RADIUS)
		{
			m_iMovePolls += 1;
			if (m_iMovePolls > MAX_MOVE_POLLS)
			{
				SetFailure("The buyer was still %1 m from the site after %2 frames, so the ask would refuse on distance. SCR_Global.TeleportPlayer reported success, so either the position did not stick or the site record's location is not where the entity is.",
					vector.Distance(body.GetOrigin(), site.location).ToString(),
					m_iMovePolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceProductionRequestComponent requests = OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get();
		if (!requests)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get() returned null, so there is no seam to buy through.");
			return true;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site.entity);
		OVT_ResourceStoreComponent store;
		if (production)
			store = production.GetStore();

		OVT_ResourceLedger ledger;
		if (store)
			ledger = store.GetLedger();

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();

		if (!production || !store || !ledger || !economy)
		{
			SetFailure("The site's component, store, ledger or the economy manager went away between arrangement and purchase.");
			return true;
		}

		string resourceId = production.GetResourceId();

		requests.BuySite(site.location, false);

		// Put the world back BEFORE judging anything, so a red case does not leave a player standing
		// at a site he owns.
		SCR_Global.TeleportPlayer(m_iPlayerId, m_vRestorePos);

		string ownerAfter = manager.GetSiteOwner(site.entity);
		bool privateAfter = manager.IsSitePrivate(site.entity);
		int stockAfter = ledger.Count(resourceId);
		string packedAfter = store.GetPackedContents();
		int moneyAfter = economy.GetPlayerMoney(m_sPersId);

		manager.SetSiteOwner(site.location, "");
		manager.SetSitePrivacy(site.location, true);

		if (ownerAfter != m_sPersId)
		{
			SetFailure("After a personal-funds buy the site's owner is '%1', expected the buyer's persistent id '%2'. Either the ask refused (every refusal answers RpcDo_ProductionError - check the log for the key) or SetSiteOwner was handed the client's position instead of the record's.",
				ownerAfter,
				m_sPersId);
			return true;
		}

		if (!privateAfter)
		{
			SetFailure("A freshly bought site is PUBLIC. Purchase must set it private (requirement 18) - a public default would open the new owner's storage to the whole server the moment he pays for it.");
			return true;
		}

		if (stockAfter != 0)
		{
			SetFailure("The site still holds %1 '%2' after being bought (it held %3 before). ClearSiteStock must run in the SAME call that takes the money, or a site can be bought for its inventory.",
				stockAfter.ToString(),
				resourceId,
				m_iStockBefore.ToString());
			return true;
		}

		if (packedAfter != "")
		{
			SetFailure("The site's ledger is empty but its replicated contents still read '%1'. OVT_ResourceLedger.Clear() fires no change event by design, so ClearSiteStock has to follow it with PublishContents() - without that every other machine keeps showing the stock the buyer just destroyed.", packedAfter);
			return true;
		}

		if (moneyAfter != m_iMoneyBefore - m_iCost)
		{
			SetFailure("The buyer holds %1 after the purchase, expected %2 - the quoted price was %3. The price charged is not the price BuyCost quotes from the same two inputs the action's label uses.",
				moneyAfter.ToString(),
				(m_iMoneyBefore - m_iCost).ToString(),
				m_iCost.ToString());
			return true;
		}

		PrintFormat("Production seam: a server-side buy took %1, set the owner private and destroyed %2 unit(s) of '%3'",
			m_iCost.ToString(),
			m_iStockBefore.ToString(),
			resourceId);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The five ownership states resolve through OVT_ResourceRequestComponent.MayUseHolder the same way
//! OVT_ResourceProductionRules.MayAccessStore answers them.
//!
//! THIS IS D13's WINDOW, CLOSED. Before Phase 5, MayUseHolder carried no site step at all, so any
//! HOLDER_TO_HOLDER or HOLDER_TO_GROUND request naming a site's store as its holder was gated only by
//! MayReachHolder and WarehouseIsAccessible - both of which pass a site through, because it is neither
//! ruined nor a registered warehouse. A forged request from a modified client would have opened any
//! site's storage regardless of ownership.
//!
//! IT DRIVES THE REAL LADDER, NOT THE PREDICATE DIRECTLY. OVT_TEST_Logic_ProductionRules already
//! proves MayAccessStore in isolation; this case proves the SERVER GATE actually calls it, by sending
//! HOLDER_TO_GROUND with the site as both source and dest (the op reads only the source, so this
//! exercises exactly one MayUseHolder call and nothing about a destination holder) and reading the
//! answer off GetOnResourceError() - a listen host runs the whole ask synchronously, so no polling is
//! needed between a mutation and its assertion.
//!
//! NOTHING IS EVER COMMITTED. RequestTransferBegin alone settles MayUseHolder's answer; the checkout
//! this case opens is superseded by the very next Begin ("m_Checkout = null" at the top of
//! RpcAsk_TransferBegin) and is never carried to a Line or a Commit, so no stock and no money ever
//! move.
//!
//! THE SITE IS RESTORED TO UNOWNED before any assertion is judged, so a red case leaves the world the
//! way discovery built it - the same discipline case H follows.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ProductionSeam_ISiteAccessGate : SCR_AutotestCaseBase
{
	//! Comfortably inside OVT_ResourceRequestComponent.GetUseRadius().
	static const float ARRIVAL_RADIUS = 20;

	//! Frame polls allowed for the teleported body to report its new position.
	static const int MAX_MOVE_POLLS = 120;

	//! A persistent id that is never the local player's own - stands in for "somebody else".
	static const string STRANGER_ID = "OVT_TEST_ProductionSeam_Stranger";

	protected int m_iPolls;
	protected int m_iMovePolls;
	protected int m_iStage;

	protected int m_iPlayerId;
	protected string m_sPersId;
	protected vector m_vRestorePos;

	protected bool m_bErrorSeen;
	protected string m_sLastErrorKey;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceProductionManagerComponent manager;
		string failure;

		if (!OVT_TEST_ProductionSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!OVT_TEST_ProductionSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		OVT_ProductionSiteData site;
		if (!OVT_TEST_ProductionSeamSubject.ResolveSite(manager, site, failure))
		{
			SetFailure(failure);
			return true;
		}

		if (m_iStage == 0)
			return Arrange(manager, site);

		return RunLadder(manager, site);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the caller's identity and moves it to the site - MayUseHolder refuses beyond the use
	//! radius, and the test world's spawn is nowhere near the Sawmill.
	//! \param[in] manager The production manager.
	//! \param[in] site The one discovered site.
	//! \return True when the case is finished (always a failure at this stage), false to poll again.
	protected bool Arrange(notnull OVT_ResourceProductionManagerComponent manager, notnull OVT_ProductionSiteData site)
	{
		m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null, so the caller has no persistent id to gate against.");
			return true;
		}

		m_sPersId = players.GetPersistentIDFromPlayerID(m_iPlayerId);
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);

		if (m_sPersId == "" || !body)
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ProductionSeamSubject.MAX_POLLS)
			{
				SetFailure("The local player had no persistent id or no character after %1 frames (player id %2), so the access gate has no caller to test.",
					m_iPolls.ToString(),
					m_iPlayerId.ToString());
				return true;
			}

			return false;
		}

		m_vRestorePos = body.GetOrigin();

		if (!SCR_Global.TeleportPlayer(m_iPlayerId, site.location))
		{
			SetFailure("The caller could not be moved to the site at %1, so MayReachHolder's use-radius step would refuse every state below and the case would prove nothing.", site.location.ToString());
			return true;
		}

		m_iStage = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the teleport to land, then walks the five ownership states in order, restoring the
	//! site before judging.
	//! \param[in] manager The production manager.
	//! \param[in] site The one discovered site.
	//! \return True when the case is finished, false to poll again.
	protected bool RunLadder(notnull OVT_ResourceProductionManagerComponent manager, notnull OVT_ProductionSiteData site)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!body)
		{
			SetFailure("The caller lost its character between being moved to the site and testing the gate.");
			return true;
		}

		if (vector.Distance(body.GetOrigin(), site.location) > ARRIVAL_RADIUS)
		{
			m_iMovePolls += 1;
			if (m_iMovePolls > MAX_MOVE_POLLS)
			{
				SetFailure("The caller was still %1 m from the site after %2 frames, so MayReachHolder would refuse on distance rather than the access gate under test.",
					vector.Distance(body.GetOrigin(), site.location).ToString(),
					m_iMovePolls.ToString());
				return true;
			}

			// Re-issue rather than wait: the preceding case restores the body with its own teleport,
			// and a teleport still in flight clobbers ours, leaving the caller parked at ITS target.
			// Idempotent - the same destination every time.
			SCR_Global.TeleportPlayer(m_iPlayerId, site.location);
			return false;
		}

		OVT_ResourceRequestComponent requests = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if (!requests)
			return Restore(manager, site, "OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get() returned null, so there is no seam to test MayUseHolder through.");

		RplId siteId = OVT_ResourceUtils.GetHolderId(site.entity);
		if (!siteId.IsValid())
			return Restore(manager, site, "The site's holder RplId is invalid, so no request could ever name it.");

		requests.GetOnResourceError().Insert(OnResourceError);

		string failure = "";

		// State 1 - unowned refuses.
		manager.SetSiteOwner(site.location, "");
		manager.SetSitePrivacy(site.location, true);
		if (!Refuses(requests, siteId))
			failure = string.Format("An UNOWNED site (owner '') was NOT refused through MayUseHolder. %1", LastErrorDiagnosis());

		// State 2 - owned by the caller, private, allows.
		if (failure == "")
		{
			manager.SetSiteOwner(site.location, m_sPersId);
			manager.SetSitePrivacy(site.location, true);
			if (!Allows(requests, siteId))
				failure = string.Format("A PRIVATE site owned by the caller was refused through MayUseHolder. %1", LastErrorDiagnosis());
		}

		// State 3 - owned private by another refuses.
		if (failure == "")
		{
			manager.SetSiteOwner(site.location, STRANGER_ID);
			manager.SetSitePrivacy(site.location, true);
			if (!Refuses(requests, siteId))
				failure = string.Format("A PRIVATE site owned by another player was NOT refused through MayUseHolder. %1", LastErrorDiagnosis());
		}

		// State 4 - owned public by another allows.
		if (failure == "")
		{
			manager.SetSitePrivacy(site.location, false);
			if (!Allows(requests, siteId))
				failure = string.Format("A PUBLIC site owned by another player was refused through MayUseHolder. %1", LastErrorDiagnosis());
		}

		// State 5 - resistance-owned allows.
		if (failure == "")
		{
			manager.SetSiteOwner(site.location, "resistance");
			manager.SetSitePrivacy(site.location, true);
			if (!Allows(requests, siteId))
				failure = string.Format("A RESISTANCE-owned site was refused through MayUseHolder. %1", LastErrorDiagnosis());
		}

		requests.GetOnResourceError().Remove(OnResourceError);

		return Restore(manager, site, failure);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one HOLDER_TO_GROUND ask naming the site as its own source and dest and expects the
	//! access gate to refuse it with #OVT-Resource_NoAccess specifically - a refusal for any other
	//! reason (distance, a lock) would be a false pass.
	//! \param[in] requests The request seam.
	//! \param[in] siteId The site's holder RplId.
	//! \return True when the answer was exactly #OVT-Resource_NoAccess.
	protected bool Refuses(notnull OVT_ResourceRequestComponent requests, RplId siteId)
	{
		Ask(requests, siteId);

		return m_bErrorSeen && m_sLastErrorKey == "#OVT-Resource_NoAccess";
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the same ask and expects no refusal at all.
	//! \param[in] requests The request seam.
	//! \param[in] siteId The site's holder RplId.
	//! \return True when the ask was accepted (no error fired).
	protected bool Allows(notnull OVT_ResourceRequestComponent requests, RplId siteId)
	{
		Ask(requests, siteId);

		return !m_bErrorSeen;
	}

	//------------------------------------------------------------------------------------------------
	//! HOLDER_TO_GROUND reads only its source, so passing the site in both slots exercises exactly one
	//! MayUseHolder call. On a listen host RpcAsk_TransferBegin runs synchronously, so the invoker has
	//! already fired by the time this returns.
	//! \param[in] requests The request seam.
	//! \param[in] siteId The site's holder RplId.
	protected void Ask(notnull OVT_ResourceRequestComponent requests, RplId siteId)
	{
		m_bErrorSeen = false;
		m_sLastErrorKey = "";

		requests.RequestTransferBegin(siteId, siteId, EOVT_ResourceOp.HOLDER_TO_GROUND, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] messageKey The refusal key, or "" - matches ScriptInvoker's (string) signature.
	protected void OnResourceError(string messageKey)
	{
		m_bErrorSeen = true;
		m_sLastErrorKey = messageKey;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What the last ask actually answered, for the failure message.
	protected string LastErrorDiagnosis()
	{
		if (!m_bErrorSeen)
			return "The request was accepted instead.";

		return string.Format("The server answered '%1'.", m_sLastErrorKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the caller and the site back the way discovery built them, then judges.
	//! \param[in] manager The production manager.
	//! \param[in] site The one discovered site.
	//! \param[in] failure "" to pass; a diagnosis to fail with.
	//! \return Always true - the case is over either way.
	protected bool Restore(notnull OVT_ResourceProductionManagerComponent manager, notnull OVT_ProductionSiteData site, string failure)
	{
		manager.SetSiteOwner(site.location, "");
		manager.SetSitePrivacy(site.location, true);

		SCR_Global.TeleportPlayer(m_iPlayerId, m_vRestorePos);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Production seam: all five ownership states resolved through MayUseHolder exactly as OVT_ResourceProductionRules.MayAccessStore predicts");
		return true;
	}
}
