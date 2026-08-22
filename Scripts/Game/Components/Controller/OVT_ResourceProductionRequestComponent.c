[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative production-site ownership and privacy requests for one player")]
class OVT_ResourceProductionRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative production-site OWNERSHIP and PRIVACY, on the per-player
//! OVT_OverthrowController entity.
//!
//! Two asks and one reply. Buying a site's accumulated STOCK is not here - that rides the shipped
//! resource checkout on OVT_ResourceRequestComponent as one appended op, so it inherits whole-cart
//! atomicity, the holder gate and the money tripwires rather than re-earning them.
//!
//! IDENTITY IS NEVER A PARAMETER, and neither is state: the caller comes from ResolveOwningPlayerId()
//! and a site is addressed by POSITION, matched onto a record whose SERVER copy of the position is
//! what the mutator is then handed.
//!
//! EVERY REFUSAL ANSWERS. The only silent returns in this file are the !Replication.IsServer() guards.
//------------------------------------------------------------------------------------------------
class OVT_ResourceProductionRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the caller may stand from a site and still act on it. The same 30 m every resource gate
	//! uses, so a purchase is never refused at a spot where the site's storage would have opened.
	protected const float USE_RADIUS = 30;

	//! The owner string a resistance-funded purchase writes. Matches real estate's literal
	//! (OVT_RealEstateRequestComponent.RpcAsk_BuyBuilding) and OVT_ResourceProductionRules' predicates.
	static const string RESISTANCE_OWNER = "resistance";

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//
	// Both take the Replication.IsServer() DIRECT branch first: an RplRcver.Server Rpc marshalled by
	// the authority is delivered to nobody, so without it a listen-server host could not buy a site or
	// flip its privacy at all. The whole reply fan then arrives SYNCHRONOUSLY inside the ask on that
	// machine, so any caller-side latch must be set before the call, never after.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Buy the production site at a position.
	//! \param[in] pos The site's position, as the caller's machine knows it.
	//! \param[in] useResistanceFunds True to pay from (and register ownership to) the resistance account.
	void BuySite(vector pos, bool useResistanceFunds)
	{
		if (Replication.IsServer())
		{
			RpcAsk_BuySite(pos, useResistanceFunds);
			return;
		}

		Rpc(RpcAsk_BuySite, pos, useResistanceFunds);
	}

	//------------------------------------------------------------------------------------------------
	//! Flip a site between private and public.
	//! \param[in] pos The site's position, as the caller's machine knows it.
	//! \param[in] isPrivate ADVISORY ONLY - the server flips from its own record and ignores this.
	void SetSitePrivacy(vector pos, bool isPrivate)
	{
		if (Replication.IsServer())
		{
			RpcAsk_SetSitePrivacy(pos, isPrivate);
			return;
		}

		Rpc(RpcAsk_SetSitePrivacy, pos, isPrivate);
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: buy the site at the claimed position with personal money or resistance funds.
	//!
	//! The ladder refuses before it charges, in this order, and every rung answers: caller resolvable,
	//! manager up, a site really is there, it is unowned, the caller is standing at it, the site's
	//! authored cost is readable, then the account-specific tests. Two clients buying the same site in
	//! the same second are serialised by the server: the loser fails the unowned test BEFORE any money
	//! moves.
	//!
	//! THE STOCK IS DESTROYED IN THE SAME CALL THAT TAKES THE MONEY, so nobody buys a site for the
	//! inventory it accumulated while unowned.
	//! \param[in] pos The claimed site position.
	//! \param[in] useResistanceFunds True to pay from the resistance treasury.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuySite(vector pos, bool useResistanceFunds)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			SendProductionError(playerId, "#OVT-ProdSite_NoPlayer");
			return;
		}

		OVT_ResourceProductionManagerComponent manager = OVT_Global.GetProduction();
		if (!manager)
		{
			SendProductionError(playerId, "#OVT-ProdSite_Failed");
			return;
		}

		OVT_ProductionSiteData rec = manager.GetSite(pos);
		if (!rec)
		{
			SendProductionError(playerId, "#OVT-ProdSite_NoSite");
			return;
		}

		if (!OVT_ResourceProductionRules.MayBuySite(rec.owner))
		{
			SendProductionError(playerId, "#OVT-ProdSite_AlreadyOwned");
			return;
		}

		if (!CallerIsWithin(playerId, rec.location, USE_RADIUS))
		{
			SendProductionError(playerId, "#OVT-Resource_TooFar");
			return;
		}

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(rec.entity);
		if (!production)
		{
			SendProductionError(playerId, "#OVT-ProdSite_NoSite");
			return;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SendProductionError(playerId, "#OVT-ProdSite_Failed");
			return;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SendProductionError(playerId, "#OVT-ProdSite_Failed");
			return;
		}

		// The ONE charge-side evaluation. The action's label calls the same static with the same two
		// inputs, so the price shown and the price taken cannot drift.
		int cost = OVT_ResourceProductionRules.BuyCost(production.GetBaseCost(), config.GetRealEstateCostMultiplier());

		string owner;

		if (useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if (!resistance)
			{
				SendProductionError(playerId, "#OVT-ProdSite_Failed");
				return;
			}

			if (!resistance.IsOfficer(playerId))
			{
				SendProductionError(playerId, "#OVT-ProdSite_NotOfficer");
				return;
			}

			if (!economy.ResistanceHasMoney(cost))
			{
				SendProductionError(playerId, "#OVT-ProdSite_NoFunds");
				return;
			}

			economy.TakeResistanceMoney(cost);
			owner = RESISTANCE_OWNER;
		}
		else
		{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if (!players)
			{
				SendProductionError(playerId, "#OVT-ProdSite_Failed");
				return;
			}

			string persId = players.GetPersistentIDFromPlayerID(playerId);
			if (persId == "")
			{
				SendProductionError(playerId, "#OVT-ProdSite_NoPlayer");
				return;
			}

			if (!economy.PlayerHasMoney(persId, cost))
			{
				SendProductionError(playerId, "#OVT-ProdSite_NoMoney");
				return;
			}

			economy.TakePlayerMoneyPersistentId(persId, cost);
			owner = persId;
		}

		manager.SetSiteOwner(rec.location, owner);
		manager.SetSitePrivacy(rec.location, true);
		manager.ClearSiteStock(rec.entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: flip the site at the claimed position between private and public.
	//!
	//! MayTogglePrivacy is the single predicate: an unowned site has nothing to toggle, a resistance
	//! site answers to officers, and a player-owned one answers to its owner alone.
	//!
	//! THE NEW STATE IS COMPUTED HERE, not taken from the caller. A client one broadcast stale would
	//! otherwise re-send the value the site already holds, and every rung would pass while nothing
	//! changed and nothing was said. ARITY 2 - the parameter stays on the wire, advisory.
	//! \param[in] pos The claimed site position.
	//! \param[in] isPrivate ADVISORY ONLY. Ignored; the flip is taken from the server's record.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetSitePrivacy(vector pos, bool isPrivate)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			SendProductionError(playerId, "#OVT-ProdSite_NoPlayer");
			return;
		}

		OVT_ResourceProductionManagerComponent manager = OVT_Global.GetProduction();
		if (!manager)
		{
			SendProductionError(playerId, "#OVT-ProdSite_Failed");
			return;
		}

		OVT_ProductionSiteData rec = manager.GetSite(pos);
		if (!rec)
		{
			SendProductionError(playerId, "#OVT-ProdSite_NoSite");
			return;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SendProductionError(playerId, "#OVT-ProdSite_Failed");
			return;
		}

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId == "")
		{
			SendProductionError(playerId, "#OVT-ProdSite_NoPlayer");
			return;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
		{
			SendProductionError(playerId, "#OVT-ProdSite_Failed");
			return;
		}

		if (!OVT_ResourceProductionRules.MayTogglePrivacy(persId, rec.owner, resistance.IsOfficer(playerId)))
		{
			SendProductionError(playerId, "#OVT-ProdSite_NotYours");
			return;
		}

		if (!CallerIsWithin(playerId, rec.location, USE_RADIUS))
		{
			SendProductionError(playerId, "#OVT-Resource_TooFar");
			return;
		}

		manager.SetSitePrivacy(rec.location, !rec.isPrivate);
	}

	//------------------------------------------------------------------------------------------------
	// SERVER -> OWNER
	//
	// ⚠ Do not hoist either half of the Rpc pairing: the Rpc() line sits directly under a
	// compiler-checked call to the same handler with the same argument.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: refuse a request. THE ONLY REFUSAL CHANNEL in this component.
	//! \param[in] playerId The player the answer is aimed at; <= 0 when the caller was unresolvable.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void SendProductionError(int playerId, string messageKey)
	{
		if (ShouldRespondLocally(playerId))
		{
			RpcDo_ProductionError(messageKey);
			return;
		}

		Rpc(RpcDo_ProductionError, messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a site request was refused. Reported to the hint, the way every user action in the mod
	//! reports one.
	//! \param[in] messageKey Localization key naming the refusal.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ProductionError(string messageKey)
	{
		if (messageKey.IsEmpty())
			return;

		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if (hints)
			hints.ShowCustom(messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller's controlled character is within a radius of a position. A caller with no
	//! character fails, which is correct: both verbs here are performed by a body standing at a site.
	//!
	//! A local copy of OVT_ResourceRequestComponent.CallerIsWithin. Recorded tech debt - lifting it to
	//! the shared base would edit a walled file.
	//! \param[in] playerId The caller.
	//! \param[in] pos The position to test against.
	//! \param[in] maxDistance The radius in metres.
	//! \return True when the caller has a character and it is close enough.
	protected bool CallerIsWithin(int playerId, vector pos, float maxDistance)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		// SQUARED: vector.Distance is not correctly rounded, so a boundary decision on it is a coin flip.
		return vector.DistanceSq(pos, character.GetOrigin()) <= maxDistance * maxDistance;
	}
}
