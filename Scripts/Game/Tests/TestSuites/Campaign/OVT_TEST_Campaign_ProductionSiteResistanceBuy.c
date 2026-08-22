//------------------------------------------------------------------------------------------------
//! Buying a production site with RESISTANCE funds moves the treasury, writes the faction as the
//! owner, destroys the stock and opens the store to the whole resistance.
//!
//! WHAT IS AT STAKE. The resistance-funded path is the only one that spends a shared account and the
//! only one that produces an owner no player id can ever equal. Every rung of it - the officer gate,
//! the treasury balance test, the charge, the owner literal and the access predicate that reads it -
//! is separate code from the personal-funds path the Init tier covers, and none of it had automated
//! coverage beyond the pure predicates.
//!
//! THE FOUR CLAIMS ARE ONE CLAIM. A case that only checked the owner would pass on a free purchase;
//! one that only checked the treasury would pass on a site nobody owns afterwards; one that skipped
//! MayAccessStore would pass on a resistance site the resistance cannot open - which is the whole
//! point of buying one with its money.
//!
//! IT DRIVES THE REAL ASK. BuySite() takes its Replication.IsServer() direct branch on this machine,
//! so the ladder runs end to end. Nothing here reaches past the public entry point.
//!
//! RESTORES WHAT IT SPENDS. The treasury is funded with exactly the price, so a successful purchase
//! returns it to the figure it started at; any surplus left by a refused purchase is taken back
//! before the case returns. The site is restored to unowned before anything is judged, so a red case
//! leaves the world the way discovery built it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 60)]
class OVT_TEST_Campaign_ProductionSiteResistanceBuy_MovesTreasuryAndOwner : SCR_AutotestCaseBase
{
	//! The owner literal a resistance purchase writes. OVT_ProductionSiteData.owner spells it the same.
	static const string RESISTANCE_OWNER = "resistance";

	//! A persistent id no campaign can produce: the "somebody else entirely" MayAccessStore is asked
	//! about. A resistance-owned site must admit them even though it is flagged private.
	static const string UNRELATED_VISITOR = "ovt-test-prodsite-visitor";

	//! Comfortably inside OVT_ResourceProductionRequestComponent.USE_RADIUS.
	static const float ARRIVAL_RADIUS = 20;

	//! Frame polls allowed for the teleported body to report its new position. A bound on the
	//! explanation, not a retry budget.
	static const int MAX_ARRIVAL_POLLS = 300;

	static const int PHASE_SETUP = 0;
	static const int PHASE_AWAIT_ARRIVAL = 1;
	static const int PHASE_BUY = 2;

	protected int m_iPhase;
	protected int m_iArrivalPolls;

	protected int m_iPlayerId;
	protected int m_iCost;
	protected int m_iTreasuryBaseline;
	protected int m_iTreasuryBefore;
	protected int m_iStockBefore;
	protected vector m_vRestorePos;
	protected bool m_bFunded;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SETUP)
			return Setup();

		if (m_iPhase == PHASE_AWAIT_ARRIVAL)
			return AwaitArrival();

		return BuyAndJudge();
	}

	//------------------------------------------------------------------------------------------------
	//! Funds the treasury with exactly the price, seeds the stock and moves the officer to the site.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Setup()
	{
		if (!Replication.IsServer())
		{
			SetFailure("This test machine is not the replication authority, so every production call would refuse at its Replication.IsServer() guard and the case would pass by doing nothing");
			return true;
		}

		OVT_ResourceProductionManagerComponent manager;
		OVT_ProductionSiteData site;

		string diagnostic = ResolveSite(manager, site);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		if (site.owner != "")
		{
			SetFailure(string.Format("The site is already owned by '%1' before this case runs, so MayBuySite would refuse the purchase and nothing would be tested. A case that mutates site ownership has to restore it.", site.owner));
			return true;
		}

		m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null, so the buyer has no identity and the officer gate cannot be satisfied");
			return true;
		}

		if (players.GetPersistentIDFromPlayerID(m_iPlayerId) == "")
		{
			SetFailure(string.Format("The player manager has no persistent id for player %1, so SetupPlayer never ran and the officer gate would refuse the purchase", m_iPlayerId.ToString()));
			return true;
		}

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!body)
		{
			SetFailure(string.Format("Player %1 controls no entity, so the ask's 30 m distance gate can never be satisfied", m_iPlayerId.ToString()));
			return true;
		}

		diagnostic = RequireOfficer();
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site.entity);
		if (!production)
		{
			SetFailure("The discovered site entity carries no OVT_ResourceProductionComponent, so it has no authored price");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null, so the real-estate cost multiplier cannot be read and the price cannot be predicted");
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null, so no money can move");
			return true;
		}

		// The SAME static, with the SAME two inputs, that the action's label and RpcAsk_BuySite use.
		m_iCost = OVT_ResourceProductionRules.BuyCost(production.GetBaseCost(), config.GetRealEstateCostMultiplier());
		if (m_iCost <= 0)
		{
			SetFailure(string.Format("BuyCost() answered %1 for a base cost of %2 at a multiplier of %3, so the site would be free and the treasury half of this case would assert nothing",
				m_iCost.ToString(), production.GetBaseCost().ToString(), config.GetRealEstateCostMultiplier().ToString()));
			return true;
		}

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store || !store.GetLedger())
		{
			SetFailure("The site entity carries no usable OVT_ResourceStoreComponent, so 'buying destroys the stock' would pass without proving anything");
			return true;
		}

		manager.ProduceForHours(1);

		m_iStockBefore = store.GetLedger().Count(production.GetResourceId());
		if (m_iStockBefore <= 0)
		{
			SetFailure(string.Format("The site holds no '%1' even after a hand-driven hour, so 'the purchase destroys the stock' would pass without proving anything", production.GetResourceId()));
			return true;
		}

		m_iTreasuryBaseline = economy.GetResistanceMoney();

		economy.AddResistanceMoney(m_iCost);
		m_bFunded = true;

		m_iTreasuryBefore = economy.GetResistanceMoney();
		if (m_iTreasuryBefore < m_iCost)
		{
			SetFailure(string.Format("The treasury holds %1 after being funded %2, so the purchase would be refused for want of funds rather than tested",
				m_iTreasuryBefore.ToString(), m_iCost.ToString()));
			Restore();
			return true;
		}

		m_vRestorePos = body.GetOrigin();

		if (!SCR_Global.TeleportPlayer(m_iPlayerId, site.location))
		{
			SetFailure(string.Format("The buyer could not be moved to the site at %1, so the ask's 30 m distance gate would refuse the purchase and the case would prove nothing", site.location.ToString()));
			Restore();
			return true;
		}

		m_iPhase = PHASE_AWAIT_ARRIVAL;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A precondition, not an assertion: the officer has to be standing at the site before the ask
	//! means anything.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitArrival()
	{
		OVT_ResourceProductionManagerComponent manager;
		OVT_ProductionSiteData site;

		string diagnostic = ResolveSite(manager, site);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			Restore();
			return true;
		}

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!body)
		{
			SetFailure("The buyer lost its character while being moved to the site");
			Restore();
			return true;
		}

		if (vector.Distance(body.GetOrigin(), site.location) <= ARRIVAL_RADIUS)
		{
			m_iPhase = PHASE_BUY;
			return false;
		}

		m_iArrivalPolls += 1;
		if (m_iArrivalPolls > MAX_ARRIVAL_POLLS)
		{
			SetFailure(string.Format("The buyer never arrived at the site: after %1 poll(s) the character is %2 m from %3, outside the %4 m tolerance. SCR_Global.TeleportPlayer reported success, so either the position did not stick or the site record's location is not where the entity is.",
				m_iArrivalPolls.ToString(),
				vector.Distance(body.GetOrigin(), site.location).ToString(),
				site.location.ToString(),
				ARRIVAL_RADIUS.ToString()));
			Restore();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the ask, puts the world back, then judges what it observed.
	//! \return Always true - the case ends here either way.
	protected bool BuyAndJudge()
	{
		OVT_ResourceProductionManagerComponent manager;
		OVT_ProductionSiteData site;

		string diagnostic = ResolveSite(manager, site);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			Restore();
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

		OVT_ResourceProductionRequestComponent requests = OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get();

		if (!production || !store || !ledger || !economy || !requests)
		{
			SetFailure("The site's component, store, ledger, the economy manager or the production request seam went away between arrangement and purchase");
			Restore();
			return true;
		}

		string resourceId = production.GetResourceId();

		requests.BuySite(site.location, true);

		string ownerAfter = manager.GetSiteOwner(site.entity);
		bool privateAfter = manager.IsSitePrivate(site.entity);
		int stockAfter = ledger.Count(resourceId);
		int treasuryAfter = economy.GetResistanceMoney();

		// Put the world back BEFORE judging anything.
		Restore();

		if (ownerAfter != RESISTANCE_OWNER)
		{
			SetFailure(string.Format("After a resistance-funded buy the site's owner is '%1', expected '%2'. Either the ask refused (every refusal answers RpcDo_ProductionError - check the log for the key, and the officer gate is the first one this path adds) or SetSiteOwner was handed the client's position instead of the record's.",
				ownerAfter, RESISTANCE_OWNER));
			return true;
		}

		if (treasuryAfter != m_iTreasuryBefore - m_iCost)
		{
			SetFailure(string.Format("The treasury holds %1 after the purchase, expected %2 - the quoted price was %3. The price charged must be the price BuyCost quotes from the same two inputs the action's label uses, and it must come out of the RESISTANCE account rather than the buyer's.",
				treasuryAfter.ToString(), (m_iTreasuryBefore - m_iCost).ToString(), m_iCost.ToString()));
			return true;
		}

		if (stockAfter != 0)
		{
			SetFailure(string.Format("The site still holds %1 '%2' after being bought (it held %3 before). ClearSiteStock must run in the SAME call that takes the money, or a site can be bought for its inventory.",
				stockAfter.ToString(), resourceId, m_iStockBefore.ToString()));
			return true;
		}

		if (!OVT_ResourceProductionRules.MayAccessStore(UNRELATED_VISITOR, ownerAfter, privateAfter))
		{
			SetFailure(string.Format("A resistance-owned site refused '%1' at its access predicate (owner '%2', private %3). The whole point of spending the faction's money is that the faction can then use the site - and the purchase deliberately sets the private flag, which MayAccessStore must ignore for a resistance owner.",
				UNRELATED_VISITOR, ownerAfter, privateAfter.ToString()));
			return true;
		}

		Print(string.Format("A resistance-funded buy took %1 from the treasury, wrote 'resistance' as the owner, destroyed %2 unit(s) of '%3' and left the store open to the whole faction",
			m_iCost.ToString(), m_iStockBefore.ToString(), resourceId));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The production manager and the world's one site record.
	//! \param[out] manager The manager; untouched on failure.
	//! \param[out] site The site record; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string ResolveSite(out OVT_ResourceProductionManagerComponent manager, out OVT_ProductionSiteData site)
	{
		OVT_ResourceProductionManagerComponent found = OVT_Global.GetProduction();
		if (!found)
			return "OVT_Global.GetProduction() is null. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_ResourceProductionManagerComponent block, so there is no site to buy.";

		array<ref OVT_ProductionSiteData> sites = found.GetSites();
		if (!sites || sites.IsEmpty())
			return "The production manager discovered no sites, so this case has no subject. Either Worlds/MP/OVT_Campaign_Test_Layers/default.layer has lost its OVT_ProductionSite_Sawmill instance or discovery no longer finds it.";

		OVT_ProductionSiteData record = sites[0];
		if (!record || !record.entity)
			return "The discovered site record is null or carries no entity, so nothing can be bought or read";

		manager = found;
		site = record;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The buyer must be an officer or the resistance-funded ask refuses at its second rung. The host
	//! is promoted by OVT_OverthrowGameMode on connect; this promotes them if that has not happened,
	//! because there is no public API to demote and a case must not depend on connect ordering.
	//! \return An empty string when the buyer is an officer, otherwise the sentence to fail with.
	protected string RequireOfficer()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return "OVT_Global.GetResistanceFaction() is null, so the officer gate cannot be satisfied and no resistance-funded purchase is possible";

		if (resistance.IsOfficer(m_iPlayerId))
			return "";

		resistance.AddOfficer(m_iPlayerId);

		if (!resistance.IsOfficer(m_iPlayerId))
			return string.Format("Player %1 is not a resistance officer and AddOfficer() did not make them one, so RpcAsk_BuySite would refuse with #OVT-ProdSite_NotOfficer and this case would report the wrong failure", m_iPlayerId.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the world back: the site goes unowned, the treasury returns to the figure this case found
	//! it at, and the buyer goes home. Safe to call from any step, and safe to call twice.
	protected void Restore()
	{
		OVT_ResourceProductionManagerComponent manager;
		OVT_ProductionSiteData site;

		if (ResolveSite(manager, site) == "")
		{
			manager.SetSiteOwner(site.location, "");
			manager.SetSitePrivacy(site.location, true);
			manager.ClearSiteStock(site.entity);
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (m_bFunded && economy)
		{
			int surplus = economy.GetResistanceMoney() - m_iTreasuryBaseline;
			if (surplus > 0)
				economy.TakeResistanceMoney(surplus);

			m_bFunded = false;
		}

		if (m_vRestorePos != vector.Zero)
		{
			SCR_Global.TeleportPlayer(m_iPlayerId, m_vRestorePos);
			m_vRestorePos = vector.Zero;
		}
	}
}
