//! Fixes a server-authority hole in vanilla's group-join path (Overthrow D3, the BUG-088 family).
//!
//! Vanilla RequestJoinGroup (Groups/SCR_PlayerControllerGroupComponent.c:830-833) is the CLIENT
//! request API: all it does is Rpc(RPC_AskJoinGroup, groupID), an RplRcver.Server RPC, which only
//! travels when the caller is a proxy. Several vanilla code paths call it FROM THE SERVER, where
//! the RPC is marshalled by the authority and goes nowhere at all:
//!
//!  - RPC_ConfirmJoinPrivateGroup (:544, itself RplRcver.Server, i.e. server-side) calls
//!    playerComponent.RequestJoinGroup(group.GetGroupID()) at :560 - so on a dedicated server the
//!    group leader approves a join request and NOTHING HAPPENS. This is the live path for
//!    Overthrow, because every Overthrow player group is private (OVT_SpawnLogic.CreateAndJoinGroup).
//!  - SCR_GroupsManagerComponent.RejoinPlayer (Groups/SCR_GroupsManagerComponent.c:1143).
//!
//! Overriding the non-RPC wrapper fixes every server-side caller at once, is byte-identical
//! behaviour on clients (they still marshal the RPC), and avoids having to override an
//! [RplRpc]-attributed method. Same shape as the branch already inlined at OVT_SpawnLogic.c:899-902.
modded class SCR_PlayerControllerGroupComponent
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] groupID group to join
	override void RequestJoinGroup(int groupID)
	{
		if (!Replication.IsServer())
		{
			Rpc(RPC_AskJoinGroup, groupID);
			return;
		}

		RPC_AskJoinGroup(groupID);

		// TASK T6.6 - THE SERVER-REJECTED JOIN THAT USED TO BE SILENT.
		//
		// RPC_ConfirmJoinPrivateGroup (:544) does NOT test IsFull() before approving, so a leader can
		// approve a request into a group that has filled up since it was made. Overthrow's D4 fix
		// (modded SCR_GroupsManagerComponent.MovePlayerToGroup) correctly refuses to strand the
		// requester - it returns their previous group id and nothing changes - but from the requester's
		// side that is a join that simply never happens, with no message at all (goal G9, quality Q4).
		//
		// m_iGroupID is assigned synchronously by RPC_AskJoinGroup (:889-895), so the outcome is
		// readable the instant it returns: still not in the group we were asked to join means refused.
		if (GetGroupID() == groupID)
			return;

		OvtNotifyJoinRefused(groupID);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells THIS player (the requester) why a server-side join produced no membership change.
	//!
	//! Server-side only, and a text notification rather than a dialog: the requester is not in a menu by
	//! this point - they asked to join and went back to playing, and the answer arrives whenever the
	//! leader gets round to approving. OVT_NotificationManagerComponent.SendTextNotification targets one
	//! player; every other client drops it on the "not me" check.
	//!
	//! \param[in] groupID Group the player did not end up in
	protected void OvtNotifyJoinRefused(int groupID)
	{
		int playerId = GetPlayerID();
		if (playerId < 1)
			return;

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
			return;

		SCR_AIGroup group = groupsManager.FindGroup(groupID);

		// A group that is gone is not necessarily a failure worth a message here: the group menu's own
		// pre-check already covers "that group no longer exists" on the client, and this path is also
		// reached by internal server-side callers whose target may legitimately have been replaced.
		if (!group)
		{
			Print("[Overthrow] Player " + playerId + " was not joined to group " + groupID + " - that group no longer exists", LogLevel.WARNING);
			return;
		}

		if (!group.IsFull())
		{
			Print("[Overthrow] Player " + playerId + " was not joined to group " + groupID + " and it is not full - vanilla refused the join for another reason (faction, role or rank)", LogLevel.WARNING);
			return;
		}

		Print("[Overthrow] Player " + playerId + " was approved into group " + groupID + " but it is full - the join was refused and the player told", LogLevel.WARNING);

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (!notify)
			return;

		notify.SendTextNotification("GroupJoinRefusedFull", playerId);
	}
}
