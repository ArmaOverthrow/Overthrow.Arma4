[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative recruit possession + inventory opening for one player")]
class OVT_PossessionRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative recruit possession, on the per-player OVT_OverthrowController entity.
//!
//! Phase 7 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced one handler and one response on the legacy comms monolith (deleted in Phase 10), and carries
//! the whole client-side possession lifecycle with them. Project rule (overthrow-controller.md): every
//! client->server RPC lives on a controller component like this one.
//!
//! WHAT THE FEATURE IS. "Open Inventory" on a recruit in the commanding radial menu is implemented as a
//! brief POSSESSION: the server hands the player control of the recruit, the client opens that
//! character's inventory, and when the inventory closes possession is handed back. There is no other way
//! to drive another character's inventory UI in Reforger.
//!
//! ===============================================================================================
//! THE CLIENT-SIDE LIFECYCLE BELOW IS BUG-147's FIX AND IS CARRIED VERBATIM. DO NOT "CLEAN IT UP".
//! ===============================================================================================
//! Three pieces, all of them non-obvious and all of them load-bearing:
//!
//!   1. m_PossessedInventoryManager - the inventory manager of the possessed recruit for the duration of
//!      one open/close session, so the close subscription can be taken off the RIGHT object even after
//!      the entity handle has moved on.
//!
//!   2. The invoker subscription is ONE-SHOT: it is Insert()ed when the inventory opens and Remove()d in
//!      the handler when it closes. Before that, every Open Inventory stacked another subscription on the
//!      same invoker, so the second session fired two restores, the third fired three, and so on
//!      (§6/F11's "open and close three times, expect exactly three restores" probe exists for this).
//!
//!   3. The 300 ms CallLater before RequestRestorePossession() IS THE FIX, not a smell. The inventory
//!      close event fires at menu-teardown START. Unpossessing under a still-live inventory menu is what
//!      leaves the recruit's facing pinned forever - they then backpedal everywhere, staring at wherever
//!      the player was looking at release. Two earlier iterations (a formation-behaviour look gate, then
//!      OVT_WorldUtils.ResetAIAimState() alone) both failed in play because they ran while the menu was still
//!      up; the deferral was verified in play on 2026-08-13 and is what makes Overthrow's release look
//!      like a vanilla GM release, which has always been clean. ResetAIAimState stays server-side in
//!      SCR_PlayerController.RpcAsk_RestorePossessionOVT as belt-and-braces.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. THE REQUEST GAINS AN OWNERSHIP CHECK, WHICH IT HAS NEVER HAD. RpcAsk_SetPossessedEntityAndOpenInventory
//!    validated NOTHING: it took a claimed playerId and any RplId and called SetPossessedEntity() on the
//!    pair. As a network endpoint that is "any client may take control of any replicated character",
//!    which is considerably worse than an inventory peek - possession is real control, and the client
//!    decides whether to ever open (or close) the inventory. The caller is now the controller's owner,
//!    the target must be one of THAT player's own recruits, must be alive, and must be within
//!    POSSESS_MAX_DISTANCE of the caller's own body.
//!
//! 2. RpcDo_OpenInventory GOES FROM Broadcast TO Owner. It used to be broadcast to every client in the
//!    session with a `localPlayerId != playerId` line at the top of the handler throwing it away again on
//!    all but one of them - a per-player UI command on the wire for everybody, correct only because of a
//!    filter that a future edit could drop. On the controller, Owner routing is genuine (the entity is
//!    owned by that player), so the filter and the playerControllerId that fed it are both deleted. The
//!    monolith's belt-and-braces direct call alongside the broadcast becomes the ShouldRespondLocally()
//!    branch - which is also a fix, because on a dedicated server that direct call ran the CLIENT half
//!    (including OpenInventory()) on the server.
//!
//! 3. RpcAsk_RestorePossessedEntity IS NOT HERE. It was deleted, not migrated (plan §3.7/D6): zero
//!    callers anywhere, and superseded by SCR_PlayerController.RequestRestorePossession(), which is what
//!    the client below actually calls and which can send while possessed.
//------------------------------------------------------------------------------------------------
class OVT_PossessionRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the caller's own body may be from the recruit whose inventory they are opening.
	//!
	//! 20 m, matching OVT_RecruitRequestComponent.RECRUIT_MAX_DISTANCE and the loadout seam's
	//! LOADOUT_BOX_MAX_DISTANCE: every other "manage this recruit of mine" interaction in the tree uses
	//! that number, and the commanding menu's own client-side gate is a cursor trace onto a character the
	//! player is looking at. There was no server-side gate at all before, so this is strictly a
	//! tightening; if a play-test ever shows a legitimate open being refused, the number is the thing to
	//! raise - never the check.
	protected const float POSSESS_MAX_DISTANCE = 20;

	//! CLIENT-SIDE. The possessed recruit's inventory manager while an Open Inventory session is live.
	//! See the one-shot note in the class header - this is what the close subscription is removed from.
	protected SCR_InventoryStorageManagerComponent m_PossessedInventoryManager;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Ask the server to possess one of the local player's own recruits and open its inventory here.
	//!
	//! MUST be called on the requesting player's OWN machine - that is what makes the server able to
	//! resolve the caller from the controller entity the request arrives on. OVT_OpenInventoryCommand
	//! gates on `playerID == SCR_PlayerController.GetLocalPlayerId()` for exactly this reason; see the
	//! comment there for why that is true on exactly one machine in every topology.
	//! \param[in] targetEntity The recruit to possess.
	void OpenPossessedInventory(IEntity targetEntity)
	{
		RplComponent rpl = GetEntityRpl(targetEntity);
		if(!rpl)
		{
			Print("[OVT_PossessionRequestComponent] Target entity has no RplComponent", LogLevel.ERROR);
			return;
		}

		if(Replication.IsServer())
		{
			RpcAsk_SetPossessedEntityAndOpenInventory(rpl.Id());
		}else{
			Rpc(RpcAsk_SetPossessedEntityAndOpenInventory, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLER
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: hand the caller control of one of their own recruits, then tell only that client to open
	//! the recruit's inventory.
	//!
	//! EVERY CHECK IN HERE IS NEW (plan §4/P7) - the monolith's handler had none. Ownership is the
	//! important one and is owner-only with no officer override, for the same reason dismiss is (P6-2): a
	//! recruit is private property, and possession is control, not inspection.
	//!
	//! Proximity is measured against GetMainEntity() rather than the controlled entity ON PURPOSE. A
	//! caller who is ALREADY possessing something controls that thing, so the controlled entity would be
	//! the recruit they are currently wearing and the distance would be measured from the wrong body.
	//! GetMainEntity() is the player's real character in both cases (SCR_PlayerController.c:446).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetPossessedEntityAndOpenInventory(RplId targetEntityId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		SCR_ChimeraCharacter targetEntity = SCR_ChimeraCharacter.Cast(ResolveEntity(targetEntityId));
		if(!targetEntity)
		{
			RejectPossessionRequest(playerId, "the target is not a character (or could not be resolved)");
			return;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		// Owner-only: possession is control of the character, so it answers to nobody but its owner
		OVT_RecruitData recruitData = OVT_RecruitData.GetRecruitDataFromEntity(targetEntity);
		if(!recruitData || recruitData.m_sOwnerPersistentId != persId)
		{
			RejectPossessionRequest(playerId, "the target is not one of the caller's own recruits");
			return;
		}

		// Carried from the command's client-side gate: a dead recruit's inventory is not possessable
		SCR_CharacterControllerComponent characterController = SCR_CharacterControllerComponent.Cast(targetEntity.FindComponent(SCR_CharacterControllerComponent));
		if(!characterController || characterController.GetLifeState() != ECharacterLifeState.ALIVE)
		{
			RejectPossessionRequest(playerId, "the target recruit is not alive");
			return;
		}

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if(!playerController)
		{
			Print("[OVT_PossessionRequestComponent] Player controller not found", LogLevel.ERROR);
			return;
		}

		IEntity callerBody = playerController.GetMainEntity();
		if(!callerBody)
		{
			RejectPossessionRequest(playerId, "the caller has no character in the world");
			return;
		}

		if(vector.Distance(callerBody.GetOrigin(), targetEntity.GetOrigin()) > POSSESS_MAX_DISTANCE)
		{
			RejectPossessionRequest(playerId, "the caller is not next to the recruit");
			return;
		}

		// Set possessed entity on server
		playerController.SetPossessedEntity(targetEntity);

		// On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
		// ourselves is never delivered - so the handler is called directly and nothing is sent.
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_OpenInventory(targetEntityId);
			return;
		}

		Rpc(RpcDo_OpenInventory, targetEntityId);
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT - THE POSSESSION LIFECYCLE (BUG-147). CARRIED VERBATIM; SEE THE CLASS HEADER.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Client: possession has been granted - open the recruit's inventory and arm the close listener.
	//!
	//! Owner-routed, so unlike the monolith's broadcast version there is no "is this for me?" filter: the
	//! controller entity this component sits on is owned by exactly one player and only that player's
	//! machine ever runs this.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OpenInventory(RplId targetEntityId)
	{
		IEntity targetEntity = ResolveEntity(targetEntityId);
		if(!targetEntity)
		{
			Print("[OVT_PossessionRequestComponent] Client: Could not find target entity", LogLevel.ERROR);
			return;
		}

		// Open inventory on client
		SCR_InventoryStorageManagerComponent inventoryManager = SCR_InventoryStorageManagerComponent.Cast(
			targetEntity.FindComponent(SCR_InventoryStorageManagerComponent)
		);

		if (inventoryManager)
		{
			// Set up close listener on the client side (one-shot - removed again in the handler,
			// otherwise every Open Inventory stacks another subscription)
			m_PossessedInventoryManager = inventoryManager;
			inventoryManager.m_OnInventoryOpenInvoker.Insert(OnClientInventoryStateChanged);
			inventoryManager.OpenInventory();
		}
		else
		{
			Print("[OVT_PossessionRequestComponent] Client: No inventory manager found", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side inventory state change handler
	protected void OnClientInventoryStateChanged(bool isOpen)
	{
		Print(string.Format("[OVT_PossessionRequestComponent] Client: Inventory state changed - isOpen: %1", isOpen), LogLevel.NORMAL);

		// When inventory closes on client, notify server to restore possession
		if (!isOpen)
		{
			Print("[OVT_PossessionRequestComponent] Client: Inventory closed, requesting possession restore", LogLevel.NORMAL);

			if (m_PossessedInventoryManager)
			{
				m_PossessedInventoryManager.m_OnInventoryOpenInvoker.Remove(OnClientInventoryStateChanged);
				m_PossessedInventoryManager = null;
			}

			// The close event fires at menu-teardown START; unpossessing under a still-live menu
			// is what leaves the recruit's facing pinned forever (BUG-147 - a vanilla GM
			// possess/release with no menu open is clean). Let the menu finish dying first.
			GetGame().GetCallqueue().CallLater(RequestRestorePossessionDeferred, 300, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Restore possession only after the inventory menu has fully torn down (BUG-147)
	protected void RequestRestorePossessionDeferred()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
		{
			// Use the player controller's method which can send RPCs even when possessed
			playerController.RequestRestorePossession();
		}
		else
		{
			Print("[OVT_PossessionRequestComponent] Client: Could not get player controller", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected possession request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//!
	//! Logs rather than notifies because the commanding menu only offers this command on a recruit the
	//! local player owns (OVT_OpenInventoryCommand.CanBeShown), so a legitimate player never reaches a
	//! rejection and should not start seeing a message for one.
	//! \param[in] playerId The rejected player.
	//! \param[in] reason Why.
	protected void RejectPossessionRequest(int playerId, string reason)
	{
		Print(string.Format("[OVT_PossessionRequestComponent] Rejected open-inventory request from player %1: %2", playerId.ToString(), reason), LogLevel.WARNING);
	}
}
