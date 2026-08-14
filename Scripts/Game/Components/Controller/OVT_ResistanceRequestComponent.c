[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative resistance operations (place, build, officers, garrisons, conversion) for one player")]
class OVT_ResistanceRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative resistance operations, on the per-player OVT_OverthrowController entity.
//!
//! Phase 5 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced six handlers and one owner response on the legacy comms monolith (deleted in Phase 10):
//! place, remove-placed, build, add-officer, add-garrison-at-base and convert-supporter. Project rule
//! (overthrow-controller.md): every client->server RPC lives on a controller component like this one.
//!
//! FOBs AND CAMPS ARE DELIBERATELY NOT HERE - they ride OVT_FOBRequestComponent (plan D4). Both seams
//! delegate to the same OVT_ResistanceFactionManager, which is the one place the "one seam per manager"
//! rule is broken on purpose: `resistance/fob` is a carved-out feature with its own bug list, and a
//! single thirteen-RPC resistance component would be the monolith at quarter scale.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3). Every handler here used to take a
//!    playerId/promoterId that ResolveSenderPlayerId() then overwrote. The parameter is DELETED, not
//!    ignored. What survives is `targetPlayerId` on AddOfficer - a TARGET, not the caller, validated as
//!    one (same BUG-016 same-tick rule the economy seam applies to transfer recipients).
//!
//! 2. PLACE, BUILD AND ADD-GARRISON WERE BARE FORWARDS. All three passed straight through to the manager
//!    with nothing but the laundered id. The manager does carry real checks (index ranges, affordability,
//!    item limits, proximity - see PlaceItem/BuildItem/AddGarrison), so this was never an exploit, but
//!    every rejection inside the manager returns silently and is indistinguishable from a dropped packet.
//!    The seam now re-tests the cheap structural preconditions and LOGS the rejection with a caller and a
//!    reason (quality bar Q9, and the same argument P3-2 made for the warehouse handlers).
//!
//! 3. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The monolith's did not, so on a
//!    LISTEN-SERVER HOST - who is the authority - every one of these was an RplRcver.Server Rpc
//!    marshalled by the server and therefore delivered to nobody. Placing, building, removing, promoting
//!    and buying a garrison all silently did nothing for a host (the same class of defect as P2-5).
//!
//! THE PROXIMITY NUMBERS ARE THE MANAGER'S OWN NUMBERS, ON PURPOSE. PLACE_MAX_DISTANCE and
//! BUILD_MAX_DISTANCE are literally the radii OVT_ResistanceFactionManager.PlaceItem()/BuildItem()
//! already apply (50 m and 250 m). Copying them rather than inventing tighter ones means the seam can
//! never refuse a request the manager would have accepted - which would be a gameplay change (G6) - while
//! still giving the rejection a log line and a name.
//------------------------------------------------------------------------------------------------
class OVT_ResistanceRequestComponent : OVT_ControllerRequestComponent
{
	//! How far a placement may be from the placing player's character. The place UI traces at most ~15 m
	//! from the camera; 50 m is the number OVT_ResistanceFactionManager.PlaceItem() itself enforces
	//! (:687) and this is deliberately identical - see the header note on why it is not tighter.
	protected const float PLACE_MAX_DISTANCE = 50;

	//! How far a build may be from the building player's character. 250 m, matching
	//! OVT_ResistanceFactionManager.BuildItem() (:801) - the build camera detaches much further from the
	//! player than the place trace does, which is why the two numbers differ.
	protected const float BUILD_MAX_DISTANCE = 250;

	//! How far the caller may be from a base before the server refuses a garrison purchase there. The
	//! base menu is opened by OVT_ManageBaseAction, which requires the base entity to be within 10 m of
	//! the base record's location and the player to be at that entity; 50 m is that plus latency slack.
	protected const float BASE_MAX_DISTANCE = 50;

	//! How far away a civilian can be from the converting player's character before the server rejects
	//! the conversion (interaction range plus latency/movement slack). Carried verbatim (BUG-063).
	protected const float CONVERT_MAX_DISTANCE = 10;

	//! Minimum ms between conversion attempts per player. Carried verbatim (BUG-063).
	protected const int CONVERT_COOLDOWN_MS = 2000;

	//! Server-side timestamp of this player's last conversion attempt. Per-player because there is one
	//! controller entity - and therefore one of these components - per player, exactly as the monolith
	//! had one per character.
	protected int m_iLastConvertTick = 0;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Place an item from the placeables config at a position.
	//! \param[in] placeableIndex Index into the placeables config.
	//! \param[in] prefabIndex Index into that placeable's prefab list.
	//! \param[in] pos Where to put it.
	//! \param[in] angles Its orientation.
	void PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles)
	{
		if(Replication.IsServer())
		{
			RpcAsk_PlaceItem(placeableIndex, prefabIndex, pos, angles);
		}else{
			Rpc(RpcAsk_PlaceItem, placeableIndex, prefabIndex, pos, angles);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Remove a placed or built item the local player owns (or any, as an officer).
	//! \param[in] entityId The item's networked id.
	void RemovePlacedItem(RplId entityId)
	{
		if(Replication.IsServer())
		{
			RpcAsk_RemovePlacedItem(entityId);
		}else{
			Rpc(RpcAsk_RemovePlacedItem, entityId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Build a structure from the buildables config at a position.
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \param[in] pos Where to put it.
	//! \param[in] angles Its orientation.
	void BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles)
	{
		if(Replication.IsServer())
		{
			RpcAsk_BuildItem(buildableIndex, prefabIndex, pos, angles);
		}else{
			Rpc(RpcAsk_BuildItem, buildableIndex, prefabIndex, pos, angles);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Promote another player to officer. The promoter is the local player and is resolved server-side.
	//! \param[in] targetPlayerId Runtime id of the player to promote.
	void AddOfficer(int targetPlayerId)
	{
		if(targetPlayerId <= 0) return;

		if(Replication.IsServer())
		{
			RpcAsk_AddOfficer(targetPlayerId);
		}else{
			Rpc(RpcAsk_AddOfficer, targetPlayerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buy a garrison group for a resistance-held base.
	//! \param[in] base The base record the menu was opened on.
	//! \param[in] res The chosen group prefab.
	void AddGarrison(OVT_BaseData base, ResourceName res)
	{
		if(!base) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return;

		OVT_Faction faction = config.GetPlayerFaction();
		if(!faction) return;

		int index = faction.m_aGroupPrefabSlots.Find(res);
		if(index == -1) return;

		if(Replication.IsServer())
		{
			RpcAsk_AddGarrison(base.id, index);
		}else{
			Rpc(RpcAsk_AddGarrison, base.id, index);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Try to convert a civilian into a resistance supporter. The roll, the distance check, the
	//! once-per-civilian gate and the rate limit are all server-side (BUG-063); the outcome hint comes
	//! back through RpcDo_ConvertSupporterResult.
	//! \param[in] civilianId The civilian's networked id.
	void ConvertSupporter(RplId civilianId)
	{
		if(Replication.IsServer())
		{
			RpcAsk_ConvertSupporter(civilianId);
		}else{
			Rpc(RpcAsk_ConvertSupporter, civilianId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: place an item.
	//!
	//! The position is ENTIRELY client-supplied - the placement ghost is a client-side trace - so it is
	//! bounded against the caller's own character here as well as in the manager. Index ranges are tested
	//! before the manager sees them so an out-of-range value is logged rather than returning null into
	//! silence.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		if(!IsPlaceableIndexValid(resistance, placeableIndex, prefabIndex))
		{
			RejectResistanceRequest(playerId, "place item", "placeable/prefab index out of range");
			return;
		}

		if(!CallerIsWithin(playerId, pos, PLACE_MAX_DISTANCE))
		{
			RejectResistanceRequest(playerId, "place item", "the position is not near the caller's character");
			return;
		}

		resistance.PlaceItem(placeableIndex, prefabIndex, pos, angles, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove a placed or built item.
	//!
	//! DELIBERATELY THIN ON OWNERSHIP. OVT_ResistanceFactionManager.RemovePlacedItem() (:863-891) already
	//! resolves the caller's persistent id and refuses unless it matches the placeable's or buildable's
	//! recorded owner, or the caller is an officer. That check was verified during this migration and is
	//! the authority; duplicating it here would create a second copy free to drift. The seam's job is to
	//! say WHO asked - which is precisely what the deleted playerId parameter used to get wrong.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RemovePlacedItem(RplId entityId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		resistance.RemovePlacedItem(entityId, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: build a structure. Same shape as RpcAsk_PlaceItem, with the buildables config and the
	//! build camera's wider radius.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		if(!IsBuildableIndexValid(resistance, buildableIndex, prefabIndex))
		{
			RejectResistanceRequest(playerId, "build item", "buildable/prefab index out of range");
			return;
		}

		if(!CallerIsWithin(playerId, pos, BUILD_MAX_DISTANCE))
		{
			RejectResistanceRequest(playerId, "build item", "the position is not near the caller's character");
			return;
		}

		resistance.BuildItem(buildableIndex, prefabIndex, pos, angles, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: promote a player to officer.
	//!
	//! The promoter is the controller's owner, so it cannot be spoofed - the monolith's second `promoterId`
	//! argument is gone. `targetPlayerId` is a RUNTIME id from the client and runtime ids are reused, so
	//! it is resolved to a persistent id and the record confirmed to exist IN THE SAME TICK (BUG-016/R4).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddOfficer(int targetPlayerId)
	{
		if(!Replication.IsServer()) return;

		if(targetPlayerId <= 0) return;

		int promoterId = ResolveOwningPlayerId();
		if(promoterId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		if(!resistance.IsOfficer(promoterId))
		{
			RejectResistanceRequest(promoterId, "add officer", "the caller is not an officer");
			return;
		}

		if(resistance.IsOfficer(targetPlayerId)) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(targetPlayerId);
		if(persId == "") return;

		if(!players.GetPlayer(persId))
		{
			RejectResistanceRequest(promoterId, "add officer", "no player record for the named target");
			return;
		}

		resistance.AddOfficer(targetPlayerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buy a garrison group for a base.
	//!
	//! WHERE THE AFFORDABILITY RULE ACTUALLY LIVES, AND WHY IT IS NOT REPEATED HERE. The price of a
	//! garrison is (baseRecruitCost + 300) x the group's unit count, and the unit count is only knowable
	//! once SpawnGarrison() has produced the group - so OVT_ResistanceFactionManager.ChargeForGarrison()
	//! (:908-922) is the only place the check can be made, and it already makes it: it refuses, notifies
	//! "CannotAfford" and the caller deletes the group. AddGarrison() also re-tests the base index, the
	//! prefab index (through GetGarrisonPrefab) and the town's supporter stock (BUG-064). This handler
	//! therefore adds what the manager cannot see: WHO asked, and whether they are standing at the base.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrison(int baseId, int prefabIndex)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if(!occupying) return;

		if(baseId < 0 || baseId >= occupying.m_Bases.Count())
		{
			RejectResistanceRequest(playerId, "add garrison", "base id out of range");
			return;
		}

		OVT_BaseData base = occupying.m_Bases[baseId];
		if(!base) return;

		if(!IsGarrisonPrefabIndexValid(prefabIndex))
		{
			RejectResistanceRequest(playerId, "add garrison", "group prefab index out of range");
			return;
		}

		if(!CallerIsWithin(playerId, base.location, BASE_MAX_DISTANCE))
		{
			RejectResistanceRequest(playerId, "add garrison", "the caller is not at the base");
			return;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		resistance.AddGarrison(baseId, prefabIndex, true, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: attempt a civilian conversion.
	//!
	//! CARRIED VERBATIM FROM THE MONOLITH (BUG-063), CHECK FOR CHECK: 10 m from the caller's character, a
	//! 2000 ms per-player rate limit, one attempt per civilian across ALL players
	//! (TryMarkCivilianConvertAttempted), and the diplomacy roll made SERVER-SIDE from the player's own
	//! record. The only difference is where the caller comes from: the controller's owner instead of
	//! GetOwner() being the character.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ConvertSupporter(RplId civilianId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity civilian = ResolveEntity(civilianId);
		if(!civilian) return;

		if(vector.Distance(character.GetOrigin(), civilian.GetOrigin()) > CONVERT_MAX_DISTANCE) return;

		int now = System.GetTickCount();
		if(m_iLastConvertTick != 0 && (now - m_iLastConvertTick) < CONVERT_COOLDOWN_MS) return;
		m_iLastConvertTick = now;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return;

		// One attempt per civilian across all players, tracked server-side
		if(!towns.TryMarkCivilianConvertAttempted(civilianId)) return;

		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		if(!player) return;

		bool converted = s_AIRandomGenerator.RandFloat01() < player.diplomacy;
		if(converted)
		{
			towns.AddSupport(civilian.GetOrigin(), 1);
		}

		// On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
		// ourselves is never delivered - so the handler is called directly and nothing is sent.
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_ConvertSupporterResult(converted);
			return;
		}

		Rpc(RpcDo_ConvertSupporterResult, converted);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: show the conversion outcome to the player who asked, and only to them.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ConvertSupporterResult(bool converted)
	{
		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if(!hints) return;

		if(converted)
		{
			hints.ShowCustom("#OVT-ConvertedSupporter");
		}else{
			hints.ShowCustom("#OVT-NotConvertedSupporter");
		}
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a client-supplied placeable/prefab index pair names a real entry.
	//! \param[in] resistance The resistance manager (already null-checked by the caller).
	//! \param[in] placeableIndex Index into the placeables config.
	//! \param[in] prefabIndex Index into that placeable's prefab list.
	//! \return True when both are in range.
	protected bool IsPlaceableIndexValid(OVT_ResistanceFactionManager resistance, int placeableIndex, int prefabIndex)
	{
		if(!resistance.m_PlaceablesConfig) return false;
		if(placeableIndex < 0 || placeableIndex >= resistance.m_PlaceablesConfig.m_aPlaceables.Count()) return false;

		OVT_Placeable placeable = resistance.m_PlaceablesConfig.m_aPlaceables[placeableIndex];
		if(!placeable) return false;

		return prefabIndex >= 0 && prefabIndex < placeable.m_aPrefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a client-supplied buildable/prefab index pair names a real entry.
	//! \param[in] resistance The resistance manager (already null-checked by the caller).
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \return True when both are in range.
	protected bool IsBuildableIndexValid(OVT_ResistanceFactionManager resistance, int buildableIndex, int prefabIndex)
	{
		if(!resistance.m_BuildablesConfig) return false;
		if(buildableIndex < 0 || buildableIndex >= resistance.m_BuildablesConfig.m_aBuildables.Count()) return false;

		OVT_Buildable buildable = resistance.m_BuildablesConfig.m_aBuildables[buildableIndex];
		if(!buildable) return false;

		return prefabIndex >= 0 && prefabIndex < buildable.m_aPrefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a client-supplied garrison group index names a real slot on the player faction.
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
	//! Whether the caller's controlled character is within a radius of a position.
	//!
	//! A caller with NO controlled character fails this - which is correct for every request that uses
	//! it: placing, building and buying a garrison are all things a body does in the world.
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
	//! Logs a rejected resistance request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//!
	//! Logs rather than notifies because every one of these is already gated client-side by the UI that
	//! sends it - a player who has never seen a message here should not start seeing one. The manager's
	//! own "CannotAfford" notifications are untouched.
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectResistanceRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_ResistanceRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
