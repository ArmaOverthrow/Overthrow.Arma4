[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative campaign actions (base capture, medical supplies, loot wanted check, save) for one player")]
class OVT_CampaignRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative campaign-level actions, on the per-player OVT_OverthrowController entity.
//!
//! Phase 8 of the controller migration (docs/features/core/controller-migration/implementation.md §4) -
//! the LAST phase that moved handlers. Replaced four handlers and one owner response on the legacy comms
//! monolith (deleted in Phase 10): start a base capture (QRF), deliver medical supplies, relay a loot
//! event to the authoritative wanted system, and request a save (plus its result reply). Project rule
//! (overthrow-controller.md): every client->server RPC lives on a controller component like this one.
//!
//! WHY THESE FOUR SHARE A COMPONENT. Two of them (medical supplies, loot check) are a domain stretch and
//! the plan says so out loud rather than hiding it (D8): they are one-off campaign interactions with no
//! sibling requests to justify a component each, and every alternative home would have been a worse lie -
//! medical supplies is a town-modifier change delivered by vehicle, and the loot check is a wanted-system
//! relay. The rule "one seam per manager" has no answer for a request that touches a manager exactly once.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3), and StartBaseCapture lost its POSITION
//!    argument too. See RpcAsk_StartBaseCapture - a client-supplied vector was the payload half of
//!    BUG-025 and there is now no way to express it.
//!
//! 2. THE LOOT CHECK RESOLVES ITS CHARACTER, IT DOES NOT ASSUME ONE. The monolith's handler ran on the
//!    caller's own character and read GetOwner(); a controller component's owner is the controller entity,
//!    so the character is resolved playerId -> controlled entity and checked for existence and life.
//!
//! 3. THE DEBUG "INSTANT CAPTURE" CHEAT IS NO LONGER A NETWORK ENDPOINT AT ALL. It moved to
//!    OVT_OverthrowGameMode as a plain #ifdef WORKBENCH method next to the DiagMenu block that is its only
//!    caller (P8-2). It was an RplRcver.Server RPC whose only gate was a client-side DiagMenu read.
//!
//! 4. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The monolith's did not, so on a
//!    LISTEN-SERVER HOST - who is the authority - capturing a base, delivering medical supplies and
//!    REQUESTING A SAVE FROM THE MENU were all RplRcver.Server Rpc()s marshalled by the server and
//!    therefore delivered to nobody. Same class of defect as P2-5.
//------------------------------------------------------------------------------------------------
class OVT_CampaignRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the delivering player may be from the vehicle before the server rejects a medical-supplies
	//! delivery. Deliberately generous: the user action is performed on the vehicle from interaction range,
	//! but the deliverer may also be sitting IN it, and the vehicle's origin is not its cargo door.
	protected const float DELIVERY_MAX_DISTANCE = 30;

	//! Client-side: fired with (bool success) when a save this client asked for has finished on the
	//! server. Subscribe BEFORE calling RequestSave().
	protected ref ScriptInvoker m_OnSaveResult = new ScriptInvoker();

	//! Server-side: which player the in-flight save reply is owed to. Resolved when the request arrives and
	//! read again when the asynchronous save completes, because ShouldRespondLocally() needs an id and the
	//! completion invoker does not carry one.
	protected int m_iPendingSavePlayerId = -1;

	//------------------------------------------------------------------------------------------------
	//! \return Invoker fired with (bool success) when a requested save completes on the server.
	ScriptInvoker GetOnSaveResult()
	{
		return m_OnSaveResult;
	}

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Ask the server to start a QRF for the base the local player is standing at.
	//!
	//! Takes no position on purpose: the server uses the caller's character origin (BUG-025).
	void StartBaseCapture()
	{
		if(Replication.IsServer())
		{
			RpcAsk_StartBaseCapture();
		}else{
			Rpc(RpcAsk_StartBaseCapture);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deliver the medical supplies in a vehicle to the nearest town.
	//! \param[in] vehicle The vehicle the action was performed on.
	void DeliverMedicalSupplies(IEntity vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_DeliverMedicalSupplies(rpl.Id());
		}else{
			Rpc(RpcAsk_DeliverMedicalSupplies, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Relay a local loot action to the server, where wanted state is authoritative (BUG-073).
	void RequestLootWantedCheck()
	{
		if(Replication.IsServer())
		{
			RpcAsk_LootWantedCheck();
		}else{
			Rpc(RpcAsk_LootWantedCheck);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to write a save point.
	//!
	//! ⚠️ THE CALLER MUST SUBSCRIBE TO GetOnSaveResult() BEFORE CALLING THIS (BUG-006). Saving is
	//! asynchronous and can genuinely fail, and on a listen-server host the whole round trip completes
	//! in-process - a subscription made after the call can miss the answer entirely.
	void RequestSave()
	{
		if(Replication.IsServer())
		{
			RpcAsk_RequestSave();
		}else{
			Rpc(RpcAsk_RequestSave);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: start a QRF at the base the caller is standing at.
	//!
	//! THE BUG-025 CHECKS, CARRIED VERBATIM AND THEN STRENGTHENED BY SUBTRACTION:
	//!   - no QRF may already be active;
	//!   - the caller must have a character and must not be dead;
	//!   - the base must currently belong to the occupying faction;
	//!   - the caller must be within the SERVER'S baseCloseRange of it (BUG-078: a client's difficulty
	//!     fields may still hold prefab defaults, so the server's own value is the only trustworthy one).
	//!
	//! The position argument is GONE. The monolith took a vector, then overwrote it with the character's
	//! origin for remote callers and TRUSTED it for its game-mode copy - and the distance check was
	//! likewise skipped whenever no character resolved. On the controller there is always exactly one
	//! answer to "where is the caller", so the payload cannot express a lie and a caller with no character
	//! is a rejection rather than a trusted server-side call.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartBaseCapture()
	{
		if(!Replication.IsServer()) return;

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of || of.m_bQRFActive) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if(!character) return;

		CharacterControllerComponent characterController = character.GetCharacterController();
		if(characterController && characterController.IsDead()) return;

		vector loc = character.GetOrigin();

		OVT_BaseData data = of.GetNearestBase(loc);
		if(!data) return;
		if(!data.IsOccupyingFaction()) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return;

		if(vector.Distance(loc, data.location) > config.m_Difficulty.baseCloseRange)
		{
			RejectCampaignRequest(playerId, "start base capture", "the caller is not at the base");
			return;
		}

		OVT_BaseControllerComponent base = of.GetBase(data.entId);
		if(!base) return;

		of.StartBaseQRF(base);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: unload a vehicle's medical supplies into the nearest town.
	//!
	//! Carried verbatim: only items sold at a DRUG shop count, each is deleted BEFORE its price is added
	//! (so a delete that fails cannot be paid for), the support/stability modifiers are applied one per ten
	//! currency of value, and the unload sound plays at the vehicle.
	//!
	//! ADDED at the seam: the caller must exist, must be near the vehicle, and must be allowed to use it -
	//! the monolith took an RplId and emptied whatever it named from anywhere on the map, which on a locked
	//! vehicle meant another player's cargo.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeliverMedicalSupplies(RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(vector.Distance(player.GetOrigin(), vehicle.GetOrigin()) > DELIVERY_MAX_DISTANCE)
		{
			RejectCampaignRequest(playerId, "deliver medical supplies", "the caller is not at the vehicle");
			return;
		}

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			RejectCampaignRequest(playerId, "deliver medical supplies", "the vehicle is locked to another player");
			return;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_TownData town = towns.GetNearestTown(vehicle.GetOrigin());
		if(!town) return;

		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = SCR_VehicleInventoryStorageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if(!vehicleStorage) return;

		array<IEntity> items = new array<IEntity>;
		vehicleStorage.GetItems(items);
		if(items.Count() == 0) return;

		int cost = 0;
		foreach(IEntity item : items)
		{
			if(!item) continue;

			ResourceName res = OVT_Global.GetPrefabName(item);
			if(!economy.IsSoldAtShop(res, OVT_ShopType.SHOP_DRUG)) continue;
			if(!vehicleStorage.TryDeleteItem(item)) continue;

			cost += economy.GetPriceByResource(res, town.location);
		}

		if(cost == 0) return;

		int townID = towns.GetTownID(town);

		int supportValue = Math.Floor(cost / 10);
		for(int t = 0; t < supportValue; t++)
		{
			towns.TryAddSupportModifierByName(townID, "MedicalSupplies");
			towns.TryAddStabilityModifierByName(townID, "MedicalSupplies");
		}

		SimpleSoundComponent simpleSoundComp = SimpleSoundComponent.Cast(vehicle.FindComponent(SimpleSoundComponent));
		if(simpleSoundComp)
		{
			vector mat[4];
			vehicle.GetWorldTransform(mat);

			simpleSoundComp.SetTransformation(mat);
			simpleSoundComp.PlayStr("UNLOAD_VEHICLE");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: apply the caller's loot event to the authoritative wanted system (BUG-073).
	//!
	//! ⚠️ THE OWNER CHANGED, AND THAT IS THE WHOLE MIGRATION OF THIS HANDLER. On the monolith this ran on
	//! the caller's own CHARACTER, so ChimeraCharacter.Cast(GetOwner()) was both the character and the
	//! proof of who asked. A controller component's owner is the CONTROLLER entity, which has no wanted
	//! component and is not a character at all, so the character is resolved from the caller instead - and
	//! then checked for existence and life, which the monolith never had to do.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LootWantedCheck()
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if(!character) return;

		CharacterControllerComponent characterController = character.GetCharacterController();
		if(characterController && characterController.IsDead()) return;

		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		if(!wanted) return;

		// OnPlayerLoot filters on "is this my own character" (BUG-074), so it must be given the character
		// the component sits on - which is this one.
		wanted.OnPlayerLoot(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write a save point on behalf of the caller.
	//!
	//! ALWAYS ANSWERS ONCE THE CALLER IS KNOWN. The menu subscribes to GetOnSaveResult() and shows nothing
	//! at all until the reply arrives (BUG-006 - it used to claim success the instant the request was
	//! sent), so a path out of here that never replies leaves the player staring at a menu that did
	//! nothing. The reply is deliberately NOT sent when the save is merely started: the save is
	//! asynchronous and its outcome comes from the persistence manager's completion invoker.
	//!
	//! ADDED at the seam: the caller must be an officer. The menu already gates on that client-side
	//! (OVT_MainMenuContext.Save), which is exactly why it needed re-deriving here.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestSave()
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		m_iPendingSavePlayerId = playerId;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance || !resistance.IsOfficer(playerId))
		{
			RejectCampaignRequest(playerId, "save", "the caller is not an officer");
			SendSaveResult(false);
			return;
		}

		OVT_OverthrowGameMode overthrow = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!overthrow)
		{
			SendSaveResult(false);
			return;
		}

		OVT_PersistenceManagerComponent persistence = overthrow.GetPersistence();
		if(!persistence)
		{
			SendSaveResult(false);
			return;
		}

		// The save is asynchronous, so the outcome is reported from its completion invoker rather than
		// assumed here (BUG-006). Remove-then-Insert because this player may ask again after a previous
		// save, and a second subscription would answer the same request twice.
		persistence.GetOnSaveFinished().Remove(OnServerSaveFinished);
		persistence.GetOnSaveFinished().Insert(OnServerSaveFinished);
		persistence.SaveGame();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side handler for OVT_PersistenceManagerComponent.GetOnSaveFinished().
	//! \param[in] success Whether the save point was actually committed.
	protected void OnServerSaveFinished(bool success)
	{
		OVT_OverthrowGameMode overthrow = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(overthrow)
		{
			OVT_PersistenceManagerComponent persistence = overthrow.GetPersistence();
			if(persistence)
				persistence.GetOnSaveFinished().Remove(OnServerSaveFinished);
		}

		SendSaveResult(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reply to the player who asked for the save.
	//!
	//! On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
	//! ourselves is never delivered - so the handler is called directly and nothing is sent.
	//! \param[in] success Whether the save point was committed.
	protected void SendSaveResult(bool success)
	{
		int playerId = m_iPendingSavePlayerId;
		m_iPendingSavePlayerId = -1;

		if(ShouldRespondLocally(playerId))
		{
			RpcDo_SaveResult(success);
			return;
		}

		Rpc(RpcDo_SaveResult, success);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the save this player asked for has finished.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SaveResult(bool success)
	{
		m_OnSaveResult.Invoke(success);
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected campaign request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectCampaignRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_CampaignRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
