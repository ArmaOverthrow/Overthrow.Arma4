[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative recruit operations (recruit, recruit-from-tent, rename, dismiss) for one player")]
class OVT_RecruitRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative recruit operations, on the per-player OVT_OverthrowController entity.
//!
//! Phase 6 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced four handlers on the legacy comms monolith (deleted in Phase 10): recruit a civilian, recruit
//! from a tent, rename a recruit and dismiss a recruit. Project rule (overthrow-controller.md): every
//! client->server RPC lives on a controller component like this one.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3). RecruitCivilian and RecruitFromTent both took
//!    a playerId that ResolveSenderPlayerId() then overwrote for remote callers. The parameter is DELETED,
//!    not ignored. `recruitId` on rename/dismiss is a TARGET, not the caller, and is validated as one.
//!
//! 2. DISMISS GAINS THE OWNERSHIP CHECK IT NEVER HAD. RpcAsk_DismissRecruit had NO caller validation of
//!    any kind: any modified client could dismiss ANY player's recruit by id, and recruit ids are
//!    broadcast to every client (OVT_RecruitManagerComponent streams the table), so the ids were not even
//!    secret. The handler now requires recruit.m_sOwnerPersistentId to equal the CALLER's persistent id -
//!    deliberately owner-only, with no officer escape hatch, because a recruit is private property in a
//!    way a shared camp is not (contrast P5-4, where officers can delete an abandoned camp).
//!
//! 3. RENAME'S "senderId > 0" BRANCH IS UNCONDITIONAL NOW. The monolith allowed an UNRESOLVED sender
//!    through as trusted, because its game-mode copy carried server-side calls with no player behind
//!    them. A controller component only ever exists on a player's controller, so an unresolvable caller
//!    is now a rejection - the same simplification P4-7 made for the resistance tax.
//!
//! 4. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The monolith's did not, so on a
//!    LISTEN-SERVER HOST - who is the authority - every one of these was an RplRcver.Server Rpc
//!    marshalled by the server and therefore delivered to nobody: recruiting, renaming and dismissing all
//!    silently did nothing for a host (the same class of defect as P2-5).
//!
//! WHAT IS CARRIED VERBATIM AND MUST STAY THAT WAY: the CIV-faction/not-already-a-recruit/not-possessed
//! triple, the 20 m interaction radius on both recruit paths, the EXPLICIT affordability test (see
//! RECRUIT_MAX_DISTANCE and the note on DoTakePlayerMoney below), charging only AFTER the recruit exists,
//! and deleting a tent-spawned civilian when RecruitCivilian() refuses it.
//------------------------------------------------------------------------------------------------
class OVT_RecruitRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the caller may be from the civilian or the tent. Both recruit paths are hold actions
	//! performed while standing at the target, so 20 m is interaction range plus latency/movement slack.
	//! Carried verbatim from the monolith's two handlers - not retuned.
	protected const float RECRUIT_MAX_DISTANCE = 20;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Recruit a civilian standing in the world. The recruiter is the local player and is resolved
	//! server-side; the cost is charged server-side after the recruit exists.
	//! \param[in] civilian The civilian character to recruit.
	void RecruitCivilian(IEntity civilian)
	{
		RplComponent rpl = GetEntityRpl(civilian);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_RecruitCivilian(rpl.Id());
		}else{
			Rpc(RpcAsk_RecruitCivilian, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Recruit a fresh civilian at a resistance tent, at half the usual cost and one town supporter.
	//! \param[in] tentPos The tent's position.
	void RecruitFromTent(vector tentPos)
	{
		if(Replication.IsServer())
		{
			RpcAsk_RecruitFromTent(tentPos);
		}else{
			Rpc(RpcAsk_RecruitFromTent, tentPos);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rename one of the local player's own recruits. Ownership and name length are validated
	//! server-side; the authoritative record is then broadcast to every client.
	//! \param[in] recruitId The recruit's id.
	//! \param[in] newName The requested name.
	void RenameRecruit(string recruitId, string newName)
	{
		if(recruitId == "" || newName == "") return;

		if(Replication.IsServer())
		{
			RpcAsk_RenameRecruit(recruitId, newName);
		}else{
			Rpc(RpcAsk_RenameRecruit, recruitId, newName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Dismiss one of the local player's own recruits: removed from its group, deleted, and struck from
	//! the authoritative table. Ownership is validated server-side.
	//! \param[in] recruitId The recruit's id.
	void DismissRecruit(string recruitId)
	{
		if(recruitId == "") return;

		if(Replication.IsServer())
		{
			RpcAsk_DismissRecruit(recruitId);
		}else{
			Rpc(RpcAsk_DismissRecruit, recruitId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: recruit a civilian.
	//!
	//! CARRIED VERBATIM FROM THE MONOLITH, CHECK FOR CHECK. Only a CIV-faction character may be recruited
	//! (not a soldier, not a player, not another player's recruit); it must not already be a recruit; it
	//! must not be possessed; and the caller must be within 20 m of it.
	//!
	//! THE AFFORDABILITY TEST IS NOT OPTIONAL AND IS NOT REDUNDANT: DoTakePlayerMoney() CLAMPS AT ZERO
	//! rather than refusing, so charging a player who cannot pay silently succeeds and hands out a free
	//! recruit. PlayerHasMoney() must be asked first. The charge is then made only once the recruit
	//! actually exists, so a manager-side refusal cannot bill anyone.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RecruitCivilian(RplId civilianId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ResolveEntity(civilianId));
		if(!character) return;

		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if(!recruitManager) return;

		// Only actual civilians can be recruited - not soldiers, players or other recruits
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if(!factionComp) return;

		Faction faction = factionComp.GetAffiliatedFaction();
		if(!faction || faction.GetFactionKey() != "CIV")
		{
			RejectRecruitRequest(playerId, "recruit civilian", "the target is not a civilian");
			return;
		}

		if(recruitManager.GetRecruitFromEntity(character))
		{
			RejectRecruitRequest(playerId, "recruit civilian", "the target is already a recruit");
			return;
		}

		if(SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(character) > 0)
		{
			RejectRecruitRequest(playerId, "recruit civilian", "the target is a possessed character");
			return;
		}

		// The recruit action is a hold action on the civilian - reject far-away targets
		if(!CallerIsWithin(playerId, character.GetOrigin(), RECRUIT_MAX_DISTANCE))
		{
			RejectRecruitRequest(playerId, "recruit civilian", "the caller is not next to the target");
			return;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		// DoTakePlayerMoney clamps at zero, so an explicit funds check is required
		int cost = config.m_Difficulty.baseRecruitCost;
		if(!economy.PlayerHasMoney(persId, cost))
		{
			OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
			return;
		}

		// Charge server-side, and only once the recruit actually exists
		if(!recruitManager.RecruitCivilian(character, playerId)) return;

		economy.DoTakePlayerMoney(playerId, cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: recruit a fresh civilian at a tent.
	//!
	//! The validation and the spawn live on OVT_RecruitManagerComponent (ValidateTentRecruit /
	//! SpawnTentRecruit) so that the equipped-recruit purchase on OVT_RecruitCommandComponent shares
	//! ONE implementation with this one - which is what makes a placement change land in both places
	//! at once instead of having to be made twice and stay made twice (recruit-ux decision D22).
	//! ValidateTentRecruit keeps the shipped check order (cap, then supporters, then distance), so the
	//! refusal a player sees first is unchanged; the shared spawn ground-clamps the position and runs
	//! it through a collision-checked box instead of the old raw fixed offset.
	//!
	//! Costs half the usual recruit price, and the affordability test is required for the same
	//! clamps-at-zero reason as RpcAsk_RecruitCivilian.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RecruitFromTent(vector tentPos)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if(!townManager || !recruitManager) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		// Cap, supporters and distance-to-tent, in the shipped order - see ValidateTentRecruit().
		// The whole transaction is validated BEFORE anything is spawned: at the cap RecruitCivilian()
		// would bail and orphan the freshly spawned civilian, and TakeSupportersFromNearestTown
		// silently no-ops when the town has no supporters
		int refusal = recruitManager.ValidateTentRecruit(tentPos, playerId);
		if(refusal != OVT_RecruitManagerComponent.TENT_RECRUIT_OK)
		{
			if(refusal == OVT_RecruitManagerComponent.TENT_RECRUIT_AT_CAP)
				RejectRecruitRequest(playerId, "recruit from tent", "the caller is at their recruit cap");
			else if(refusal == OVT_RecruitManagerComponent.TENT_RECRUIT_NO_SUPPORTERS)
				RejectRecruitRequest(playerId, "recruit from tent", "the nearest town has no supporters");
			else if(refusal == OVT_RecruitManagerComponent.TENT_RECRUIT_TOO_FAR)
				RejectRecruitRequest(playerId, "recruit from tent", "the caller is not at the tent");
			return;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return;

		// DoTakePlayerMoney clamps at zero, so an explicit funds check is required
		int cost = Math.Round(config.m_Difficulty.baseRecruitCost * 0.5);
		if(!economy.PlayerHasMoney(persId, cost))
		{
			OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
			return;
		}

		// Spawn recruit at the tent. This action only ever knew a position, so it passes no entity;
		// the shared spawn recovers the tent from the position and uses its authored spawn point.
		// Ownership is handled inside the shared spawn.
		SCR_ChimeraCharacter recruit = recruitManager.SpawnTentRecruit(null, tentPos, playerId);
		if(!recruit) return;

		// Costs are taken only after the recruit actually exists
		townManager.TakeSupportersFromNearestTown(tentPos, 1);
		economy.DoTakePlayerMoney(playerId, cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rename a recruit the caller owns.
	//!
	//! The RPC exists at all because the client-side rename never reached the authority (BUG-052 family):
	//! the menu renamed its local replica and the change died there. The name length rule (1-32) lives in
	//! OVT_RecruitManagerComponent.RenameRecruit() and is NOT duplicated here - the seam's job is to say
	//! who asked, and to refuse anyone who is not the recruit's owner.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RenameRecruit(string recruitId, string newName)
	{
		if(!Replication.IsServer()) return;

		if(recruitId == "" || newName == "") return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if(!recruitManager) return;

		OVT_RecruitData recruit = recruitManager.GetRecruit(recruitId);
		if(!recruit) return;

		if(!CallerOwnsRecruit(playerId, recruit))
		{
			RejectRecruitRequest(playerId, "rename recruit", "the recruit belongs to another player");
			return;
		}

		// RenameRecruit validates name length (1-32)
		if(!recruitManager.RenameRecruit(recruitId, newName)) return;

		recruitManager.BroadcastRecruitUpdate(recruit);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: dismiss a recruit the caller owns.
	//!
	//! THE OWNERSHIP CHECK HERE IS NEW (plan §4/P6). The monolith's handler validated nothing but the
	//! recruit's existence, so any client could dismiss any player's recruit - permanently, since dismiss
	//! deletes the body and strikes the record. Recruit ids are replicated to every client, so this was
	//! reachable with no guesswork.
	//!
	//! The group-removal/delete/RemoveRecruit sequence is carried verbatim: the agent must leave its group
	//! BEFORE the entity dies, or the group keeps a dangling member slot, and RemoveRecruit() is what
	//! broadcasts the removal to every client.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DismissRecruit(string recruitId)
	{
		if(!Replication.IsServer()) return;

		if(recruitId == "") return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if(!recruitManager)
		{
			Print("[OVT_RecruitRequestComponent] Server: Recruit manager not found", LogLevel.ERROR);
			return;
		}

		OVT_RecruitData recruit = recruitManager.GetRecruit(recruitId);
		if(!recruit)
		{
			RejectRecruitRequest(playerId, "dismiss recruit", "no recruit record for that id");
			return;
		}

		if(!CallerOwnsRecruit(playerId, recruit))
		{
			RejectRecruitRequest(playerId, "dismiss recruit", "the recruit belongs to another player");
			return;
		}

		// Find and delete the recruit entity on server
		IEntity recruitEntity = recruitManager.FindRecruitEntity(recruitId);
		if(recruitEntity)
		{
			// Remove from group first
			AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
			if(aiControl)
			{
				AIAgent agent = aiControl.GetAIAgent();
				if(agent && agent.GetParentGroup())
				{
					agent.GetParentGroup().RemoveAgent(agent);
				}
			}

			// Delete entity on server
			SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
		}

		// Remove from manager (this will broadcast to all clients)
		recruitManager.RemoveRecruit(recruitId);

		Print("[OVT_RecruitRequestComponent] Server: Dismissed recruit: " + recruitId, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a recruit belongs to the calling player.
	//!
	//! Owner-only ON PURPOSE - there is no officer override. An officer may delete another player's camp
	//! (P5-4) because an abandoned camp would otherwise be permanent world clutter; a recruit despawns on
	//! its own when its owner is offline, so there is nothing an officer needs to clean up here, and a
	//! recruit is the one thing in the game a player levels up individually.
	//! \param[in] playerId The caller's runtime id.
	//! \param[in] recruit The recruit record.
	//! \return True when the caller is the recorded owner.
	protected bool CallerOwnsRecruit(int playerId, notnull OVT_RecruitData recruit)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return false;

		return recruit.m_sOwnerPersistentId == persId;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller's controlled character is within a radius of a position.
	//!
	//! A caller with NO controlled character fails this, which is correct for both recruit paths: walking
	//! up to a civilian and standing at a tent are both things a body does in the world.
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
	//! Logs a rejected recruit request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//!
	//! Logs rather than notifies because every one of these is already gated client-side by the action or
	//! menu that sends it - a player who has never seen a message here should not start seeing one. The
	//! "CannotAfford" notification the two recruit paths already sent is untouched.
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectRecruitRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_RecruitRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
