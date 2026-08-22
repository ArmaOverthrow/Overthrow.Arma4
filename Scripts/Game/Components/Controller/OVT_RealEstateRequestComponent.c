[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative real estate and warehouse requests for one player")]
class OVT_RealEstateRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative real estate requests, on the per-player OVT_OverthrowController.
//!
//! Phase 3 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced eight handlers on the legacy comms monolith (deleted in Phase 10): set-home, buy/sell/rent/
//! stop-renting a building, and three warehouse movements. Project rule (overthrow-controller.md):
//! every client->server RPC lives on a controller component like this one.
//!
//! THE THREE WAREHOUSE VERBS ARE GONE (logistics/storage Phase 7). A warehouse building is an ordinary
//! storage holder addressed by RplId now, so every warehouse movement runs through
//! OVT_StorageRequestComponent and the one MayUseHolder gate. No warehouse id crosses this wire.
//!
//! WHY BUILDING REQUESTS CARRY NO BUILDING ARGUMENT. They never did, and that is the good part of the
//! original design: the server resolves the target with re.GetNearestBuilding(caller's origin), so the
//! caller can only ever act on the building they are standing at. The menu resolves the same building
//! locally for its labels and its advisory checks, but the authority never trusts that answer. Carried
//! unchanged - dropping only the client-supplied playerId (plan G3/D3).
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3). The monolith sat on the caller's CHARACTER
//!    and laundered a client-supplied playerId through ResolveSenderPlayerId(). This component sits on
//!    the caller's CONTROLLER ENTITY, so the caller is ResolveOwningPlayerId() and the parameter is
//!    DELETED rather than ignored.
//!
//! 2. RpcAsk_SetBuildingHome IS NOT HERE, AND ITS ABSENCE IS THE POINT (plan §3.7/D6). It had zero
//!    callers repo-wide and no validation whatsoever - it let any client set any player's home to any
//!    replicated entity on the map. Real estate's own IsHome/SetAsHome fix re-adds a validated version
//!    when that work lands; see docs/features/economy/real-estate/context.md.
//!
//! WHY THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). An RplRcver.Server RPC marshalled by
//! the authority is delivered to nobody (the BUG-045/052/088 family), so the monolith's unconditional
//! Rpc() meant a LISTEN-SERVER HOST could not buy, sell, rent or set home at all. The direct-call branch
//! is the project's standard fix, not an optimisation.
//------------------------------------------------------------------------------------------------
class OVT_RealEstateRequestComponent : OVT_ControllerRequestComponent
{
	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Set the local player's home/respawn position to where their character is standing.
	void SetHome()
	{
		if(Replication.IsServer())
		{
			RpcAsk_SetHome();
		}else{
			Rpc(RpcAsk_SetHome);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buy the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to pay from (and register ownership to) the resistance account.
	void BuyBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_BuyBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_BuyBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sell the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to sell a resistance-owned building into the resistance account.
	void SellBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_SellBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_SellBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rent the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to rent on the resistance account.
	void RentBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_RentBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_RentBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Give up the rental on the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to release a resistance-held rental.
	void StopRentingBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_StopRentingBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_StopRentingBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS - REAL ESTATE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: set the caller's home position to their character's position.
	//!
	//! The position is taken from the character, never from the payload, so there is nothing to spoof:
	//! the only remaining precondition is that the caller HAS a character (a request from a dead or
	//! pre-spawn client has no position to mean).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetHome()
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		re.SetHomePos(playerId, player.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buy the building nearest the caller's character.
	//!
	//! Carried verbatim from the monolith: the building is resolved server-side from the caller's own
	//! position; a building that is already owned or rented is not for sale; a resistance-funded purchase
	//! requires the caller to be an officer; and the affordability test runs against whichever account is
	//! actually being charged. The menu performs the same affordability check for UX, but on a client its
	//! answer is advisory only (plan §3.4).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();
		if(re.IsOwned(entId) || re.IsRented(entId)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		int cost = re.GetBuyPrice(building);

		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
			if(!economy.ResistanceHasMoney(cost)) return;

			economy.TakeResistanceMoney(cost);
			re.SetOwnerPersistentId("resistance", building);
		}else{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if(!players) return;

			string persId = players.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
			if(!economy.PlayerHasMoney(persId, cost)) return;

			economy.TakePlayerMoneyPersistentId(persId, cost);
			re.SetOwner(playerId, building);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sell the building nearest the caller's character.
	//!
	//! Carried verbatim, including the "not your last house" rule (m_mOwned[persId].Count() == 1), which
	//! exists because a player with no house has no home to respawn at. The two branches gate different
	//! things: the resistance branch needs an officer AND a resistance-owned building; the personal
	//! branch needs the caller to own it, it not to be their home, and it not to be their only one.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		int cost = re.GetSellPrice(building);

		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
			if(re.GetOwnerID(building) != "resistance") return;

			economy.AddResistanceMoney(cost);
		}else{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if(!players) return;

			string persId = players.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
			if(!re.IsOwner(persId, entId)) return;
			if(re.IsHome(persId, entId)) return;
			if(re.m_mOwned.Contains(persId) && re.m_mOwned[persId].Count() == 1) return;

			economy.AddPlayerMoneyPersistentId(persId, cost);
		}

		re.SetOwner(-1, building);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rent the building nearest the caller's character.
	//!
	//! The lattice is carried verbatim and is subtler than it looks:
	//!   - an owner may "rent" their own building (it costs nothing and just marks it occupied);
	//!   - an officer renting on the resistance account counts as an owner of a resistance-owned house;
	//!   - a building that is the caller's home, is already rented, or is owned by SOMEBODY ELSE, is out.
	//! The charge is therefore conditional on !isOwner, and lands on whichever account was named.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RentBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		EntityID entId = building.GetID();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
		}

		bool isOwner = re.IsOwner(persId, entId);
		if(useResistanceFunds && re.GetOwnerID(building) == "resistance") isOwner = true;

		if(re.IsHome(persId, entId) || re.IsRented(entId) || (re.IsOwned(entId) && !isOwner)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		if(!isOwner)
		{
			int cost = re.GetRentPrice(building);
			if(useResistanceFunds)
			{
				if(!economy.ResistanceHasMoney(cost)) return;
				economy.TakeResistanceMoney(cost);
			}else{
				if(!economy.PlayerHasMoney(persId, cost)) return;
				economy.TakePlayerMoneyPersistentId(persId, cost);
			}
		}

		if(useResistanceFunds)
		{
			re.SetRenterPersistentId("resistance", building);
		}else{
			re.SetRenter(playerId, building);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: release the rental on the building nearest the caller's character.
	//!
	//! Carried verbatim: only the recorded renter may stop the rental, and an officer additionally counts
	//! as the renter of a resistance-held one. There is no refund, so there is no account to charge.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StopRentingBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		EntityID entId = building.GetID();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		bool isRenter = re.IsRenter(persId, entId);
		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
			if(re.GetRenterID(building) == "resistance") isRenter = true;
		}
		if(!isRenter) return;

		re.SetRenter(-1, building);
	}
}
