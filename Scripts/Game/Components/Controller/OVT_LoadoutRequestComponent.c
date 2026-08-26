[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative loadout operations (save, apply-from-box, delete) for one player")]
class OVT_LoadoutRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative loadout operations, on the per-player OVT_OverthrowController entity.
//!
//! Phase 7 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced three handlers on the legacy comms monolith (deleted in Phase 10): save a loadout, apply one
//! from an equipment box, and delete one. Project rule (overthrow-controller.md): every client->server
//! RPC lives on a controller component like this one.
//!
//! THIS IS THE FREE-KIT SURFACE, WHICH IS WHY IT READS THE WAY IT DOES. A loadout apply spawns a saved
//! kit onto a character. BUG-043 closed the version of that which took a client-supplied owner id:
//! ResolveSenderPersistentId() laundered the claimed id back to the sender's own, so "apply player X's
//! loadout" quietly became "apply your own". That laundering is now structural - the identity parameter
//! is GONE from all three signatures (plan G3/D3), so there is no claimed id left to launder.
//!
//! THE ABSENT ENDPOINT IS PART OF THE DESIGN. There is deliberately NO no-box "LoadLoadout" here. The
//! monolith carried one until it was deleted as a free-item endpoint: it spawned a full saved kit from
//! prefabs out of thin air, for any claimed player, with no box and no proximity gate, and had no
//! legitimate caller in the repo. Applying FROM A BOX is the only network path, because the box is what
//! makes the transaction cost something. Do not re-add the other one.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT. See above - this is the whole point of the phase.
//!
//! 2. THE PROXIMITY/OWNERSHIP BRANCH IS UNCONDITIONAL NOW, AND THAT IS AN INTENDED TIGHTENING. The
//!    monolith wrapped the box-distance and target-ownership checks in `if (senderCharacter)`, because
//!    its game-mode copy fielded server-side calls with no player behind them and had to let those
//!    through. A controller component only ever exists on a player's controller, so there is no such
//!    caller: a request whose sender has no character in the world is now a rejection rather than a
//!    trusted one. Same simplification as P4-7 (tax) and P6-3 (rename).
//!
//! 3. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The monolith's did not, so on a
//!    LISTEN-SERVER HOST - who is the authority - each of these was an RplRcver.Server Rpc marshalled by
//!    the server, the pattern every earlier phase of this migration found to be unreliable at best
//!    (P2-5, BUG-161/BUG-162). The branch makes the host path a plain in-process call and removes the
//!    question entirely.
//!
//! WHAT IS CARRIED VERBATIM AND MUST STAY THAT WAY: LOADOUT_BOX_MAX_DISTANCE 20 m measured from BOTH the
//! sender AND the apply target to the box, and "the target is the sender themselves or one of the
//! sender's OWN recruits". Between them those three rules are what stops a client dressing another
//! player's recruit - or a stranger's body - out of a box on the far side of the map.
//------------------------------------------------------------------------------------------------
class OVT_LoadoutRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the sender (and the apply target) may be from the equipment box before the server rejects
	//! a loadout apply: interaction range plus latency/movement slack. Carried verbatim from the
	//! monolith's own constant - not retuned.
	protected const float LOADOUT_BOX_MAX_DISTANCE = 20;

	//! Longest accepted loadout name. Matches the 1-32 rule both save dialogs already enforce
	//! client-side (OVT_SaveLoadoutAction, OVT_SaveOfficerLoadoutAction), so it can never refuse a name
	//! the UI would have offered - it exists because the dialog's check is client-side only and the name
	//! is a persisted map key, i.e. an unbounded string a modified client could grow without limit.
	//!
	//! Applied to SAVE ONLY, on purpose. Apply and delete take a name that must already EXIST to do
	//! anything, so bounding them buys nothing and would risk orphaning a record saved by some earlier
	//! build under a longer name.
	protected const int LOADOUT_NAME_MAX_LENGTH = 32;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Save the local player's currently worn equipment as a named loadout.
	//!
	//! The owner is ALWAYS the calling player, resolved server-side; there is no way to save as somebody
	//! else, which is what BUG-043 was about.
	//! \param[in] loadoutName The name to save under (1-32 characters).
	//! \param[in] description Optional free text shown in the loadout list.
	//! \param[in] isOfficerTemplate True to save a shared officer template (officer-gated server-side).
	void SaveLoadout(string loadoutName, string description = "", bool isOfficerTemplate = false)
	{
		if(loadoutName == "") return;
		if(loadoutName.Length() > LOADOUT_NAME_MAX_LENGTH) return;

		if(Replication.IsServer())
		{
			RpcAsk_SaveLoadout(loadoutName, description, isOfficerTemplate);
		}else{
			Rpc(RpcAsk_SaveLoadout, loadoutName, description, isOfficerTemplate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Apply one of the local player's saved loadouts to a target, taking the items from an equipment box.
	//!
	//! The target may be the calling player or one of their OWN recruits; both the caller and the target
	//! must be standing at the box. All three rules are re-derived server-side.
	//! \param[in] loadoutName Which saved loadout to apply.
	//! \param[in] equipmentBox The box the items come out of.
	//! \param[in] targetEntity Who gets dressed.
	void LoadLoadoutFromBox(string loadoutName, IEntity equipmentBox, IEntity targetEntity)
	{
		if(loadoutName == "") return;

		RplComponent equipmentBoxRpl = GetEntityRpl(equipmentBox);
		RplComponent targetEntityRpl = GetEntityRpl(targetEntity);

		if(!equipmentBoxRpl || !targetEntityRpl)
		{
			Print(string.Format("[OVT_LoadoutRequestComponent] Could not get RplComponent - EquipmentBox: %1, TargetEntity: %2",
				!equipmentBoxRpl, !targetEntityRpl), LogLevel.ERROR);
			return;
		}

		if(Replication.IsServer())
		{
			RpcAsk_LoadLoadoutFromBox(loadoutName, equipmentBoxRpl.Id(), targetEntityRpl.Id());
		}else{
			Rpc(RpcAsk_LoadLoadoutFromBox, loadoutName, equipmentBoxRpl.Id(), targetEntityRpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Delete one of the local player's own saved loadouts.
	//! \param[in] loadoutName Which one.
	//! \param[in] isOfficerTemplate True when deleting a shared officer template.
	void DeleteLoadout(string loadoutName, bool isOfficerTemplate = false)
	{
		if(loadoutName == "") return;

		if(Replication.IsServer())
		{
			RpcAsk_DeleteLoadout(loadoutName, isOfficerTemplate);
		}else{
			Rpc(RpcAsk_DeleteLoadout, loadoutName, isOfficerTemplate);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: save the caller's worn equipment under a name they own.
	//!
	//! The owner key is the CALLER's persistent id and nothing else (BUG-043). The officer gate on
	//! templates is deliberately NOT duplicated here: OVT_LoadoutManagerComponent.SaveOfficerTemplate()
	//! already re-derives it and logs its own refusal, and a second copy of that rule is free to drift
	//! from the manager's (same reasoning as BuySkill in P4).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SaveLoadout(string loadoutName, string description, bool isOfficerTemplate)
	{
		if(!Replication.IsServer()) return;

		if(loadoutName == "") return;
		if(loadoutName.Length() > LOADOUT_NAME_MAX_LENGTH)
		{
			Print(string.Format("[OVT_LoadoutRequestComponent] Rejected save loadout request: name is longer than %1 characters", LOADOUT_NAME_MAX_LENGTH.ToString()), LogLevel.WARNING);
			return;
		}

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		string persId = ResolveCallerPersistentId(playerId);
		if(persId == "") return;

		// Saving copies what the caller is WEARING, so a caller with no body has nothing to save
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!playerEntity)
		{
			RejectLoadoutRequest(playerId, "save loadout", "the caller has no character in the world");
			return;
		}

		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if(!loadoutManager)
		{
			Print("[OVT_LoadoutRequestComponent] Loadout manager not available", LogLevel.ERROR);
			return;
		}

		if(isOfficerTemplate)
		{
			loadoutManager.SaveOfficerTemplate(persId, loadoutName, playerEntity, description);
		}
		else
		{
			loadoutManager.SaveLoadout(persId, loadoutName, playerEntity, description);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: apply one of the caller's own loadouts to the caller or one of their own recruits, out of
	//! an equipment box both of them are standing at.
	//!
	//! CARRIED VERBATIM FROM THE MONOLITH, CHECK FOR CHECK, AND THE ORDER MATTERS BECAUSE EACH ONE CLOSES
	//! A DIFFERENT HOLE:
	//!   - the loadout is looked up under the CALLER's persistent id, so no client can apply another
	//!     player's saved kit (BUG-043);
	//!   - the box must be within LOADOUT_BOX_MAX_DISTANCE of the CALLER, so the request cannot be sent
	//!     from across the map at a box the caller has never seen;
	//!   - the box must be within LOADOUT_BOX_MAX_DISTANCE of the TARGET too, so a recruit left at a
	//!     depot cannot be dressed remotely out of a box the caller happens to be standing at;
	//!   - the target must be the caller or one of the caller's OWN recruits, so nobody can re-kit
	//!     another player's recruit (or another player) out of a shared box.
	//!
	//! The one deliberate change: the monolith wrapped all of that in `if (senderCharacter)`. Here it is
	//! unconditional - a caller with no character is refused rather than trusted.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LoadLoadoutFromBox(string loadoutName, RplId equipmentBoxId, RplId targetEntityId)
	{
		if(!Replication.IsServer()) return;

		if(loadoutName == "") return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		string persId = ResolveCallerPersistentId(playerId);
		if(persId == "") return;

		// The loadout UI is used standing at the box, so the caller must have a body standing there
		IEntity senderCharacter = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!senderCharacter)
		{
			RejectLoadoutRequest(playerId, "apply loadout", "the caller has no character in the world");
			return;
		}

		IEntity equipmentBox = ResolveEntity(equipmentBoxId);
		if(!equipmentBox)
		{
			Print(string.Format("[OVT_LoadoutRequestComponent] Could not find equipment box with RplId: %1", equipmentBoxId), LogLevel.ERROR);
			return;
		}

		IEntity targetEntity = ResolveEntity(targetEntityId);
		if(!targetEntity)
		{
			Print(string.Format("[OVT_LoadoutRequestComponent] Could not find target entity with RplId: %1", targetEntityId), LogLevel.ERROR);
			return;
		}

		if(vector.Distance(senderCharacter.GetOrigin(), equipmentBox.GetOrigin()) > LOADOUT_BOX_MAX_DISTANCE)
		{
			RejectLoadoutRequest(playerId, "apply loadout", "the caller is not standing at the equipment box");
			return;
		}

		if(vector.Distance(targetEntity.GetOrigin(), equipmentBox.GetOrigin()) > LOADOUT_BOX_MAX_DISTANCE)
		{
			RejectLoadoutRequest(playerId, "apply loadout", "the target is not standing at the equipment box");
			return;
		}

		// The target must be the sender themselves or one of their own recruits
		if(targetEntity != senderCharacter)
		{
			OVT_RecruitData recruitData = OVT_RecruitData.GetRecruitDataFromEntity(targetEntity);
			if(!recruitData || recruitData.m_sOwnerPersistentId != persId)
			{
				RejectLoadoutRequest(playerId, "apply loadout", "the target is neither the caller nor one of the caller's own recruits");
				return;
			}
		}

		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if(!loadoutManager)
		{
			Print("[OVT_LoadoutRequestComponent] Loadout manager not available", LogLevel.ERROR);
			return;
		}

		loadoutManager.LoadLoadout(persId, loadoutName, targetEntity, equipmentBox);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: delete one of the caller's own saved loadouts.
	//!
	//! The owner key is the CALLER's persistent id (BUG-043), so "delete loadout X of player Y" is not
	//! expressible any more - the only record this can reach is one the caller owns.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeleteLoadout(string loadoutName, bool isOfficerTemplate)
	{
		if(!Replication.IsServer()) return;

		if(loadoutName == "") return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		string persId = ResolveCallerPersistentId(playerId);
		if(persId == "") return;

		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if(!loadoutManager)
		{
			Print("[OVT_LoadoutRequestComponent] LoadoutManager not found", LogLevel.ERROR);
			return;
		}

		loadoutManager.DeleteLoadout(persId, loadoutName, isOfficerTemplate);
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The caller's PERSISTENT id, which is the key every loadout record is stored under.
	//!
	//! Loadouts are keyed by persistent id rather than runtime id because they outlive a session, so this
	//! is the last step of the identity chain: controller entity -> runtime player id -> persistent id,
	//! all of it server-side and none of it from the payload.
	//! \param[in] playerId The caller's runtime id, already resolved from the controller entity.
	//! \return The persistent id, or an empty string when there is no record (always a rejection).
	protected string ResolveCallerPersistentId(int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return "";

		return players.GetPersistentIDFromPlayerID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected loadout request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//!
	//! Logs rather than notifies because every one of these is already gated client-side by the menu or
	//! action that sends it - a player who has never seen a message here should not start seeing one. The
	//! manager's own LoadoutApplied/LoadoutDeleted notifications are untouched.
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectLoadoutRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_LoadoutRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
