[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative FOB and camp requests for one player")]
class OVT_FOBRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative FOB and camp operations, on the per-player OVT_OverthrowController entity.
//!
//! Phase 5 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced seven handlers on the legacy comms monolith (deleted in Phase 10): garrison-at-camp,
//! garrison-at-FOB, deploy, undeploy, set-priority, camp privacy and delete-camp. Project rule
//! (overthrow-controller.md): every client->server RPC lives on a controller component like this one.
//!
//! WHY THIS IS A SEPARATE COMPONENT FROM OVT_ResistanceRequestComponent when both delegate to the same
//! OVT_ResistanceFactionManager: plan D4. It is the one deliberate exception to "one seam per manager",
//! because `resistance/fob` is a carved-out feature with its own bug list and a single thirteen-RPC
//! resistance component would recreate the monolith at quarter scale.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3). Deploy, undeploy and both garrison handlers
//!    took a playerId that ResolveSenderPlayerId() then overwrote; the parameter is DELETED. Note the
//!    manager's DeployFOB/UndeployFOB still take a playerId - they are server-side methods with
//!    server-side callers, and -1 there means "server-initiated, free". This seam only ever passes a
//!    resolved, real player.
//!
//! 2. SET-CAMP-PRIVACY AND DELETE-CAMP HAD **ZERO** VALIDATION. Both were bare forwards: any client
//!    could flip the privacy of any camp, or delete any camp, from anywhere on the map, at any time, with
//!    no ownership test of any kind. Delete was the worse of the two - RemoveCamp() deletes the entity
//!    named by the RplId, so an arbitrary RplId with a valid camp position was an arbitrary-entity
//!    delete. Both now require the caller to own the camp or be an officer, to be standing at it, and
//!    (for delete) the named entity to actually be that camp's entity.
//!
//! 3. SET-PRIORITY GAINS AN OFFICER GATE. It changes which FOB every player's map highlights, so it is a
//!    global effect driven by one client. The action's CanBeShownScript already gates on officer
//!    client-side; that is UX, and this is the authority.
//!
//! 4. DEPLOY AND UNDEPLOY GAIN PROXIMITY + VEHICLE PERMISSION. Both were bare forwards taking an RplId,
//!    so any client could deploy or undeploy any mobile FOB anywhere. The manager's own checks (distance
//!    to enemy bases and radio towers, one-operation-at-a-time, transfer component availability) are
//!    untouched and still run - what is added is "you are there" and "it is yours to use".
//!
//! 5. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The monolith's did not, so on a
//!    LISTEN-SERVER HOST every one of these was an RplRcver.Server Rpc marshalled by the authority and
//!    delivered to nobody - deploying a FOB as a host silently did nothing (same class as P2-5).
//------------------------------------------------------------------------------------------------
class OVT_FOBRequestComponent : OVT_ControllerRequestComponent
{
	//! How far a client-supplied camp/FOB position may be from the nearest registry record before the
	//! purchase is refused. Carried verbatim from the monolith: the point is that the position names a
	//! REAL camp, not that the caller is precisely on it.
	protected const float REGISTRY_MATCH_DISTANCE = 50;

	//! How far the caller may be from a camp or FOB before a request about it is refused. The camp menu
	//! is opened by OVT_ManageCampAction within 15 m of the camp entity and the FOB menu by
	//! OVT_ManageBaseAction within 10 m; 50 m is that plus latency and movement slack, and it is
	//! deliberately the same number as the registry match so no request is refused by one gate that the
	//! other would have passed.
	protected const float SITE_MAX_DISTANCE = 50;

	//! How far the caller may be from a mobile/deployed FOB vehicle before deploy or undeploy is refused.
	//! The same 15 m the vehicle seam applies to lock, claim, upgrade and repair - both are user actions
	//! performed at (usually inside) the vehicle.
	protected const float VEHICLE_MAX_DISTANCE = 15;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Buy a garrison group for a camp.
	//! \param[in] camp The camp record the menu was opened on.
	//! \param[in] res The chosen group prefab.
	void AddGarrisonCamp(OVT_CampData camp, ResourceName res)
	{
		if(!camp) return;

		int index = FindGarrisonPrefabIndex(res);
		if(index == -1) return;

		if(Replication.IsServer())
		{
			RpcAsk_AddGarrisonCamp(camp.location, index);
		}else{
			Rpc(RpcAsk_AddGarrisonCamp, camp.location, index);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buy a garrison group for a FOB.
	//! \param[in] fob The FOB record the menu was opened on.
	//! \param[in] res The chosen group prefab.
	void AddGarrisonFOB(OVT_FOBData fob, ResourceName res)
	{
		if(!fob) return;

		int index = FindGarrisonPrefabIndex(res);
		if(index == -1) return;

		if(Replication.IsServer())
		{
			RpcAsk_AddGarrisonFOB(fob.location, index);
		}else{
			Rpc(RpcAsk_AddGarrisonFOB, fob.location, index);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deploy a mobile FOB vehicle into a static FOB.
	//! \param[in] vehicle The mobile FOB vehicle.
	void DeployFOB(IEntity vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_DeployFOB(rpl.Id());
		}else{
			Rpc(RpcAsk_DeployFOB, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pack a deployed FOB back into a mobile FOB vehicle.
	//! \param[in] vehicle The deployed FOB.
	void UndeployFOB(IEntity vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_UndeployFOB(rpl.Id());
		}else{
			Rpc(RpcAsk_UndeployFOB, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Make a FOB the priority FOB on everyone's map. Officer only, enforced server-side.
	//! \param[in] fobEntity The deployed FOB entity.
	void SetPriorityFOB(IEntity fobEntity)
	{
		RplComponent rpl = GetEntityRpl(fobEntity);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_SetPriorityFOB(rpl.Id());
		}else{
			Rpc(RpcAsk_SetPriorityFOB, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Make a camp private or public.
	//! \param[in] camp The camp record.
	//! \param[in] isPrivate The new state.
	void SetCampPrivacy(OVT_CampData camp, bool isPrivate)
	{
		if(!camp) return;

		if(Replication.IsServer())
		{
			RpcAsk_SetCampPrivacy(camp.location, isPrivate);
		}else{
			Rpc(RpcAsk_SetCampPrivacy, camp.location, isPrivate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Delete a camp.
	//!
	//! The camp ENTITY is found client-side, because that is the only side that has it: the record holds
	//! a position, not an entity reference, and the entity is identified by carrying an
	//! OVT_ManageCampAction. The sphere query is carried verbatim from the monolith for that reason, and
	//! the RPC does not go out until the callback finds the entity.
	//! \param[in] camp The camp record.
	void DeleteCamp(OVT_CampData camp)
	{
		if(!camp) return;

		m_vDeleteCampLocation = camp.location;

		BaseWorld world = GetGame().GetWorld();
		if(!world) return;

		world.QueryEntitiesBySphere(camp.location, 10, null, FindCampEntityCallback, EQueryEntitiesFlags.ALL);
	}

	//! The camp position the in-flight DeleteCamp() sphere query is looking around (client).
	protected vector m_vDeleteCampLocation;

	//------------------------------------------------------------------------------------------------
	//! Sphere-query callback: the first entity carrying an OVT_ManageCampAction is the camp.
	//! \param[in] entity A candidate from the query.
	//! \return True to stop searching.
	protected bool FindCampEntityCallback(IEntity entity)
	{
		if(!entity) return false;

		ActionsManagerComponent actionsManager = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if(!actionsManager) return false;

		array<BaseUserAction> actions = {};
		actionsManager.GetActionsList(actions);

		foreach(BaseUserAction action : actions)
		{
			if(!OVT_ManageCampAction.Cast(action)) continue;

			RplComponent rpl = GetEntityRpl(entity);
			if(!rpl) continue;

			if(Replication.IsServer())
			{
				RpcAsk_DeleteCamp(rpl.Id(), m_vDeleteCampLocation);
			}else{
				Rpc(RpcAsk_DeleteCamp, rpl.Id(), m_vDeleteCampLocation);
			}

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: buy a garrison group for a camp.
	//!
	//! The registry-null guard and the 50 m position match are carried verbatim - the camp registry may
	//! be empty, in which case GetNearestCampData() answers null, and the position is client-supplied so
	//! it must be shown to name a real camp. Added: the group prefab index is range-checked before the
	//! manager sees it, and the caller must be at the camp.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrisonCamp(vector pos, int prefabIndex)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		OVT_CampData camp = resistance.GetNearestCampData(pos);
		if(!camp || vector.Distance(camp.location, pos) > REGISTRY_MATCH_DISTANCE)
		{
			RejectFOBRequest(playerId, "add camp garrison", "no camp at the claimed position");
			return;
		}

		if(!IsGarrisonPrefabIndexValid(prefabIndex))
		{
			RejectFOBRequest(playerId, "add camp garrison", "group prefab index out of range");
			return;
		}

		if(!CallerIsWithin(playerId, camp.location, SITE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "add camp garrison", "the caller is not at the camp");
			return;
		}

		resistance.AddGarrisonCamp(camp, prefabIndex, true, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buy a garrison group for a FOB. Same shape as RpcAsk_AddGarrisonCamp.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrisonFOB(vector pos, int prefabIndex)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		OVT_FOBData fob = resistance.GetNearestFOBData(pos);
		if(!fob || vector.Distance(fob.location, pos) > REGISTRY_MATCH_DISTANCE)
		{
			RejectFOBRequest(playerId, "add FOB garrison", "no FOB at the claimed position");
			return;
		}

		if(!IsGarrisonPrefabIndexValid(prefabIndex))
		{
			RejectFOBRequest(playerId, "add FOB garrison", "group prefab index out of range");
			return;
		}

		if(!CallerIsWithin(playerId, fob.location, SITE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "add FOB garrison", "the caller is not at the FOB");
			return;
		}

		resistance.AddGarrisonFOB(fob, prefabIndex, true, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deploy a mobile FOB.
	//!
	//! ADDED, because this was a bare forward: the caller must be at the vehicle and allowed to use it.
	//! PlayerMayUseVehicle is the shared "unlocked, or locked and yours" rule from the base class - the
	//! same one the trunk sell and the upgrade path use, so those three can never disagree about who owns
	//! a car. Everything else (distance to enemy bases and radio towers, the one-operation-in-flight
	//! latch, the container transfer availability check) stays in the manager where it already lives.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeployFOB(RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(!CallerIsWithin(playerId, vehicle.GetOrigin(), VEHICLE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "deploy FOB", "the caller is not at the vehicle");
			return;
		}

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			RejectFOBRequest(playerId, "deploy FOB", "the vehicle is locked to another player");
			return;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		resistance.DeployFOB(vehicleId, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: undeploy a FOB. Same additions as RpcAsk_DeployFOB.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UndeployFOB(RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(!CallerIsWithin(playerId, vehicle.GetOrigin(), VEHICLE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "undeploy FOB", "the caller is not at the FOB");
			return;
		}

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			RejectFOBRequest(playerId, "undeploy FOB", "the FOB is locked to another player");
			return;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		resistance.UndeployFOB(vehicleId, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: make a FOB the priority FOB.
	//!
	//! OFFICER-GATED, and that is an addition rather than a carry. This changes which FOB is highlighted
	//! on EVERY player's map - a global effect triggered by one client - and the only gate it had was
	//! OVT_SetPriorityFOBAction.CanBeShownScript(), which runs on the requesting client and therefore
	//! protects nothing.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetPriorityFOB(RplId fobEntityId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity fobEntity = ResolveEntity(fobEntityId);
		if(!fobEntity) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		if(!resistance.IsOfficer(playerId))
		{
			RejectFOBRequest(playerId, "set priority FOB", "the caller is not an officer");
			return;
		}

		if(!CallerIsWithin(playerId, fobEntity.GetOrigin(), SITE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "set priority FOB", "the caller is not at the FOB");
			return;
		}

		resistance.SetPriorityFOB(fobEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: flip a camp between private and public.
	//!
	//! HAD NO VALIDATION AT ALL. It resolves the camp from the registry, requires the caller to own it or
	//! be an officer, and requires them to be standing at it. Note that the manager is handed
	//! camp.location, not the client's `pos`: SetCampPrivacy() matches records by exact vector equality,
	//! so passing the SERVER's copy of the position removes a class of near-miss where a client's slightly
	//! different float would silently update nothing while still broadcasting.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetCampPrivacy(vector pos, bool isPrivate)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		OVT_CampData camp = resistance.GetNearestCampData(pos);
		if(!camp || vector.Distance(camp.location, pos) > REGISTRY_MATCH_DISTANCE)
		{
			RejectFOBRequest(playerId, "set camp privacy", "no camp at the claimed position");
			return;
		}

		if(!CallerOwnsCampOrIsOfficer(playerId, camp, resistance))
		{
			RejectFOBRequest(playerId, "set camp privacy", "the caller neither owns the camp nor is an officer");
			return;
		}

		if(!CallerIsWithin(playerId, camp.location, SITE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "set camp privacy", "the caller is not at the camp");
			return;
		}

		resistance.SetCampPrivacy(camp.location, isPrivate);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: delete a camp.
	//!
	//! HAD NO VALIDATION AT ALL, and was the more dangerous of the two unvalidated camp endpoints:
	//! RemoveCamp() deletes whatever entity the RplId names, so an arbitrary RplId paired with any valid
	//! camp position was an arbitrary-entity delete for any client. The named entity is therefore checked
	//! to actually BE at the camp before it is passed on, on top of the ownership and proximity gates.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeleteCamp(RplId campEntityId, vector pos)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		OVT_CampData camp = resistance.GetNearestCampData(pos);
		if(!camp || vector.Distance(camp.location, pos) > REGISTRY_MATCH_DISTANCE)
		{
			RejectFOBRequest(playerId, "delete camp", "no camp at the claimed position");
			return;
		}

		IEntity campEntity = ResolveEntity(campEntityId);
		if(!campEntity)
		{
			RejectFOBRequest(playerId, "delete camp", "the named entity does not resolve");
			return;
		}

		if(vector.Distance(campEntity.GetOrigin(), camp.location) > REGISTRY_MATCH_DISTANCE)
		{
			RejectFOBRequest(playerId, "delete camp", "the named entity is not the camp's entity");
			return;
		}

		if(!CallerOwnsCampOrIsOfficer(playerId, camp, resistance))
		{
			RejectFOBRequest(playerId, "delete camp", "the caller neither owns the camp nor is an officer");
			return;
		}

		if(!CallerIsWithin(playerId, camp.location, SITE_MAX_DISTANCE))
		{
			RejectFOBRequest(playerId, "delete camp", "the caller is not at the camp");
			return;
		}

		resistance.RemoveCamp(campEntityId, camp.location);
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Index of a group prefab in the player faction's slot list, or -1.
	//!
	//! Client-side counterpart of IsGarrisonPrefabIndexValid: the menus pass a ResourceName and the wire
	//! carries the index, exactly as the monolith's wrappers did.
	//! \param[in] res The group prefab.
	//! \return Its slot index, or -1 when the faction does not offer it.
	protected int FindGarrisonPrefabIndex(ResourceName res)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return -1;

		OVT_Faction faction = config.GetPlayerFaction();
		if(!faction) return -1;

		return faction.m_aGroupPrefabSlots.Find(res);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a client-supplied garrison group index names a real slot on the player faction.
	//!
	//! Duplicated from OVT_ResistanceRequestComponent rather than hoisted: the shared base is deliberately
	//! free of domain knowledge, and "what groups can this faction field" is domain. The two copies are
	//! four lines of the same config lookup and neither has any policy in it to drift.
	//! \param[in] prefabIndex Index into OVT_Faction.m_aGroupPrefabSlots.
	//! \return True when it is in range.
	protected bool IsGarrisonPrefabIndexValid(int prefabIndex)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return false;

		OVT_Faction faction = config.GetPlayerFaction();
		if(!faction) return false;

		return prefabIndex >= 0 && prefabIndex < faction.m_aGroupPrefabSlots.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player may administer a camp: they built it, or they are an officer.
	//!
	//! This is the rule OVT_ManageCampAction.CanBeShownScript() applies client-side (owner only) widened
	//! by the officer branch every other resistance permission uses - refusing an officer here would make
	//! an abandoned camp permanently undeletable.
	//! \param[in] playerId The caller.
	//! \param[in] camp The camp record.
	//! \param[in] resistance The resistance manager (already null-checked by the caller).
	//! \return True when the caller may change or delete this camp.
	protected bool CallerOwnsCampOrIsOfficer(int playerId, OVT_CampData camp, OVT_ResistanceFactionManager resistance)
	{
		if(!camp) return false;

		if(resistance.IsOfficer(playerId)) return true;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return false;

		return camp.owner == persId;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller's controlled character is within a radius of a position.
	//!
	//! A caller with NO controlled character fails this, which is correct for every request here: all
	//! seven are user actions or menus reached from a body standing at a camp, a FOB or a vehicle.
	//! \param[in] playerId The caller.
	//! \param[in] pos The position to test against.
	//! \param[in] maxDistance The radius.
	//! \return True when the caller has a character and it is close enough.
	protected bool CallerIsWithin(int playerId, vector pos, float maxDistance)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return false;

		return vector.Distance(character.GetOrigin(), pos) <= maxDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected FOB/camp request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectFOBRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_FOBRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
