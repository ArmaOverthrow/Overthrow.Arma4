//! Closes the "full group strands the joiner with no group at all" hole (Overthrow D4).
//!
//! Vanilla MovePlayerToGroup (Groups/SCR_GroupsManagerComponent.c:109-134) calls
//! previousGroup.RemovePlayer(playerID) at :114 BEFORE it tests newGroup.IsFull() at :119, and then
//! returns -1. RPC_AskJoinGroup (Groups/SCR_PlayerControllerGroupComponent.c:889-895) turns that -1
//! into m_iGroupID = -1 and broadcasts it: a LIVE PLAYER WITH NO GROUP - no commanding, no recruits,
//! the exact BUG-088 failure state this feature exists to prevent.
//!
//! Rejecting up front and returning previousGroupID makes the caller's (groupIDAfter != m_iGroupID)
//! test false, so nothing changes and the player simply stays where they were. Simpler and safer
//! than trying to undo the removal after the fact.
//!
//! IsFull() is (IsMaxMembersLimited() && m_iMaxMembers <= m_aPlayerIDs.Count())
//! (Entities/SCR_AIGroup.c:613-616) and counts PLAYERS only - slave-group AI (recruits) do not
//! consume slots.
modded class SCR_GroupsManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] playerID
	//! \param[in] previousGroupID
	//! \param[in] newGroupID
	//! \return the group the player ended up in - previousGroupID when the move was rejected
	override int MovePlayerToGroup(int playerID, int previousGroupID, int newGroupID)
	{
		SCR_AIGroup newGroup = FindGroup(newGroupID);
		if (!newGroup || newGroup.IsFull())
			return previousGroupID; // reject WITHOUT emptying the player's current group

		return super.MovePlayerToGroup(playerID, previousGroupID, newGroupID);
	}

	//------------------------------------------------------------------------------------------------
	//! Is this group already queued for destruction?
	//!
	//! m_aDeletionQueue (:63) is protected, so only a modded class can answer this. It exists because
	//! OVT_PlayerGroupManagerComponent adopts an EMPTY playable group rather than adding a second one,
	//! and a group emptied earlier in the same frame is already in that queue: DeleteGroupDelayed (:721)
	//! queues it and schedules GetGame().GetCallqueue().Call(DeleteGroups), and DeleteGroups (:1647-1654)
	//! then destroys every queued group NEXT FRAME without re-testing whether it is still empty. Handing
	//! a player a queued group would leave them at GetGroupID() == -1 one frame later - the exact state
	//! the own-group guarantee exists to prevent.
	//!
	//! Named with an OVT suffix on purpose: this is Overthrow's addition to a vanilla class, and a future
	//! Reforger version adding its own IsGroupQueuedForDeletion() must not silently collide with it.
	//!
	//! \param[in] group Group to test
	//! \return True when the group will be destroyed on the next frame
	bool IsGroupQueuedForDeletionOVT(SCR_AIGroup group)
	{
		if (!group)
			return false;

		return m_aDeletionQueue.Contains(group);
	}
}
