//! Manager component for the PLAYER GROUP LIFECYCLE.
[BaseContainerProps(configRoot: true)]
class OVT_PlayerGroupManagerComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Guarantees that every connected player always has a group of their own, and reacts to every
//! group membership change on the server.
//!
//! WHY THIS EXISTS. A live player at GetGroupID() == -1 has no AI commanding, no group indicator and
//! no recruits - that is the whole BUG-088 symptom set, and it is invisible in solo play. Overthrow's
//! model is "your group is yours": you spawn into a private group named after you, and if you ever
//! stop being in a group you are put back into one.
//!
//! IT IS A STATELESS SERVER-SIDE REACTOR. No replicated state is added by this manager; all group
//! state already lives on vanilla's replicated SCR_AIGroup / SCR_PlayerControllerGroupComponent, so
//! there is nothing here to JIP and nothing to persist (group membership is deliberately NOT saved -
//! decision D2).
//!
//! REACHED VIA GetInstance() ONLY - there is no OVT_Global accessor (decision D10: the service-locator
//! half is frozen and nothing outside this manager needs it).
//!
//! Phase 4 adds the recruit-follows-owner half to the two handlers below; this phase deliberately
//! ships the group lifecycle on its own so it can be play-tested without recruits in the picture.
//------------------------------------------------------------------------------------------------
class OVT_PlayerGroupManagerComponent : OVT_Component
{
	//! Singleton instance
	static OVT_PlayerGroupManagerComponent s_Instance;

	//! AI-density budget for ONE group's slave group. 0 (the shipped default) = DISABLED, and nothing
	//! below does anything at all.
	//!
	//! It is a WARNING, never a refusal (task T6.2, risk R7). Refusing the join would leave the joiner
	//! outside a group they asked to be in, and silently dropping recruits would orphan live AI - both
	//! worse than a laggy group. When the projection goes over, the joiner AND the group's leader each
	//! get a text notification and everything proceeds as normal.
	//!
	//! IT IS DELIBERATELY UNMEASURED. The number that matters (6 players x 16 recruits = ~96 agents in
	//! one slave group) needs a live server and two client processes, which the automated spine cannot
	//! produce - see "Needs human verification" in context.md for the procedure. Set this to the measured
	//! figure once it exists; the arithmetic behind it is already pinned by OVT_TEST_Logic_GroupRecruits.
	static const int MAX_SLAVE_AI_PER_GROUP = 0;

	//! Runtime player id -> persistent id, kept ONLY for the few lines of a disconnect (task T6.3).
	//!
	//! WHY IT HAS TO EXIST. OVT_OverthrowGameMode.OnPlayerDisconnected fires m_OnPlayerDisconnected
	//! (:832) with the persistent id, then drops the runtime->persistent mapping with
	//! ClearPlayerIdMappings (:836), and only THEN calls super (:838) - which is what fires
	//! SCR_BaseGameMode's disconnect invoker, which SCR_AIGroup subscribed to at
	//! Entities/SCR_AIGroup.c:1416 and which ends in RemovePlayer. So by the time OnGroupPlayerRemoved
	//! sees a disconnect, the id it needs to find the departing player's recruits is already gone.
	//! Caching it one line earlier is what lets the pull-out run at all.
	//!
	//! BOUNDED: every entry is dropped one frame later by ForgetDepartingPlayer, whether or not a
	//! removal ever arrived (a player who disconnects while in no group produces none).
	protected ref map<int, string> m_mDepartingPersistentIds = new map<int, string>();

	//! Set once the disconnect hook is installed, so the OnPostInit and EOnInit attempts cannot both
	//! subscribe (OVT_Global.GetPlayers() is not guaranteed to resolve at OnPostInit time).
	protected bool m_bDisconnectHookInstalled;

	//------------------------------------------------------------------------------------------------
	//! Get singleton instance
	//! \return The manager on the Overthrow game mode, or null before OnPostInit has run
	static OVT_PlayerGroupManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		s_Instance = this;

		SetEventMask(owner, EntityEvent.INIT);

		if (SCR_Global.IsEditMode())
			return;

		// Tried here AND again in EOnInit: OVT_Global.GetPlayers() resolves another component on this
		// same entity and the init order between them is not something this feature can bet on. The
		// installed flag makes the second attempt a no-op. (OVT_RecruitManagerComponent does the same.)
		InstallDisconnectHook();

		//! SUBSCRIBED UNCONDITIONALLY, GUARDED IN THE BODY. GetOnPlayerAdded/GetOnPlayerRemoved are
		//! STATIC invokers fired from RplRcver.Broadcast RPCs (Entities/SCR_AIGroup.c:1280, :1503), so
		//! they fire on clients as well as on the server - every handler below therefore opens with an
		//! explicit Replication.IsServer() check.
		//!
		//! The subscription itself is NOT gated on Replication.IsServer(): its truth this early in
		//! component init is not something this feature can afford to bet on, and the failure mode is
		//! asymmetric - a wrongly-skipped subscription is a silently dead own-group guarantee (i.e.
		//! BUG-088 again), while a wrongly-kept one costs one comparison per membership change on a
		//! client. Vanilla makes the same trade: SCR_GroupsManagerComponent subscribes to both invokers
		//! unconditionally (Groups/SCR_GroupsManagerComponent.c:1723-1724) and guards its handler bodies
		//! with IsProxy().
		//!
		//! Ordering note: Overthrow subscribes in OnPostInit and vanilla in EOnInit, so for any one
		//! membership event these handlers run BEFORE vanilla's. That is the ordering Phase 4 wants -
		//! recruits are pulled out of a group before vanilla can queue it for deletion.
		ScriptInvoker onPlayerAdded = SCR_AIGroup.GetOnPlayerAdded();
		ScriptInvoker onPlayerRemoved = SCR_AIGroup.GetOnPlayerRemoved();

		if (!onPlayerAdded || !onPlayerRemoved)
		{
			Print("[Overthrow] SCR_AIGroup membership invokers are null - the own-group guarantee is NOT active", LogLevel.ERROR);
			return;
		}

		onPlayerAdded.Insert(OnGroupPlayerAdded);
		onPlayerRemoved.Insert(OnGroupPlayerRemoved);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (!GetGame().InPlayMode())
			return;

		InstallDisconnectHook();
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to Overthrow's own disconnect invoker - the ONE moment a departing player's persistent
	//! id and their group membership are both still true. Idempotent.
	protected void InstallDisconnectHook()
	{
		if (m_bDisconnectHookInstalled)
			return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return;

		players.m_OnPlayerDisconnected.Insert(OnPlayerDisconnecting);
		m_bDisconnectHookInstalled = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A player is leaving the session. SERVER ONLY. This is TASK T6.3, and it is the reason a
	//! disconnect reaches the recruit pull-out at all.
	//!
	//! ORDER, WHICH IS THE WHOLE POINT (OVT_OverthrowGameMode.OnPlayerDisconnected):
	//!   :832  m_OnPlayerDisconnected.Invoke(persId, playerId)   <-- WE ARE HERE, id still resolvable
	//!   :836  ClearPlayerIdMappings(playerId)                   <-- the mapping is dropped
	//!   :838  super  -> SCR_BaseGameMode's disconnect invoker
	//!                -> SCR_AIGroup.OnPlayerDisconnected (:1175) -> RemovePlayer
	//!                -> OnGroupPlayerRemoved, which can no longer look the id up
	//!
	//! So the id is cached here and MoveOwnedRecruitsOut falls back to it a few lines later, INSIDE the
	//! removal frame - which is what keeps the pull-out synchronous, keeps the ex-group's slave group
	//! readable (it is only queued for deletion, drained next frame) and keeps the recruits out of the
	//! ex-leader's hands from the instant their owner goes.
	//!
	//! WHY NOT PULL THE RECRUITS OUT RIGHT HERE. Because the removal has not happened yet: the player is
	//! still a member, vanilla has not yet decided the group is empty, and doing it early would split the
	//! pull-out across two code paths with different guarantees. One site, one frame, one log line.
	//!
	//! RESULTING BEHAVIOUR, WHICH IS THE DESIGN (T6.3): the departed owner is offline, so
	//! OVT_GroupRecruitTransfer.SelectTransferable refuses to transfer anything of theirs anywhere, and
	//! RemoveRecruitsFromGroup takes them out of the group they were in. Their recruits therefore end up
	//! in NO slave group and are commandable by nobody - not the ex-leader, not anyone. The bodies are
	//! then the existing offline flow's problem (OVT_RecruitManagerComponent's offline despawn timer,
	//! ~10 min, BUG-086's reservation rules), which this feature does not touch.
	//!
	//! \param[in] persistentId Departing player's persistent id
	//! \param[in] playerId Departing player's runtime id
	protected void OnPlayerDisconnecting(string persistentId, int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (persistentId.IsEmpty())
			return;

		m_mDepartingPersistentIds.Set(playerId, persistentId);

		// The removal that consumes this happens synchronously inside super.OnPlayerDisconnected, i.e.
		// before this frame ends. Dropping the entry next frame is therefore pure hygiene - it also
		// covers the case where no removal arrives at all (a player who disconnects while in no group).
		GetGame().GetCallqueue().CallLater(ForgetDepartingPlayer, 0, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a cached disconnect id one frame after it was taken. See OnPlayerDisconnecting.
	//! \param[in] playerId Runtime id to forget
	protected void ForgetDepartingPlayer(int playerId)
	{
		m_mDepartingPersistentIds.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Makes sure this player has their own private group and is in it. SERVER ONLY.
	//!
	//! IDEMPOTENT: a player who already has a group is left exactly as they are and their current group
	//! id is returned. That is what lets the spawn path (OVT_SpawnLogic.CreateAndJoinGroup, with its
	//! 500 ms retry ladder) and the reactor's one-frame restore below both call it without either
	//! having to know about the other - whichever gets there first wins and the other no-ops. They can
	//! never both create a group, because both go through this one method on the main thread.
	//!
	//! It does NOT assign the player's faction. SetCivilianFaction stays on the spawn path on purpose:
	//! assigning a faction fires vanilla's SCR_GroupsManagerComponent.OnPlayerFactionChanged
	//! (Groups/SCR_GroupsManagerComponent.c:1101-1140), which REMOVES the player from their group at
	//! :1130 and resets their group ids at :1139. That is correct once, at spawn; doing it from a
	//! restore would tear down the very membership the restore is trying to establish.
	//!
	//! \param[in] playerId Player to give a group to
	//! \return The group the player is in afterwards, or -1 when no group could be established (every
	//!         failure is logged with its reason - nothing here fails silently)
	int EnsureOwnGroup(int playerId)
	{
		if (!Replication.IsServer())
			return -1;

		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
		if (!groupController)
		{
			Print("[Overthrow] EnsureOwnGroup: player " + playerId + " has no group controller (no player controller?) - no group created", LogLevel.WARNING);
			return -1;
		}

		// IDEMPOTENT no-op. Also the reason a mid-move player is never given a second group.
		int existingGroupId = groupController.GetGroupID();
		if (existingGroupId != -1)
			return existingGroupId;

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
		{
			Print("[Overthrow] EnsureOwnGroup: no SCR_GroupsManagerComponent - cannot create a group for player " + playerId, LogLevel.WARNING);
			return -1;
		}

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
		{
			Print("[Overthrow] EnsureOwnGroup: no SCR_FactionManager - cannot create a group for player " + playerId, LogLevel.WARNING);
			return -1;
		}

		// No faction means the faction assignment on the spawn path did not take. Say so instead of
		// returning silently - this is the head of the chain that leaves a player with no group, no
		// commanding and recruits that never join them (BUG-088).
		Faction faction = factionManager.GetPlayerFaction(playerId);
		if (!faction)
		{
			Print("[Overthrow] Player " + playerId + " has no faction - cannot create their group", LogLevel.ERROR);
			return -1;
		}

		bool adopted = true;
		SCR_AIGroup group = FindAdoptableGroup(groupsManager, faction);
		if (!group)
		{
			adopted = false;
			group = groupsManager.CreateNewPlayableGroup(faction);
		}

		if (!group)
		{
			// CreateNewPlayableGroup returns null when the prefab fails to load, when the group has no
			// RplComponent while a commanding manager exists (Groups/SCR_GroupsManagerComponent.c:1275),
			// or when the faction has run out of radio frequencies (AssignGroupFrequency / :1328).
			Print("[Overthrow] Failed to create group for player " + playerId + " (faction " + faction.GetFactionKey() + ") - out of group radio frequencies, or the group prefab has no RplComponent", LogLevel.WARNING);
			return -1;
		}

		BrandOwnGroup(group, playerId);

		int groupID = group.GetGroupID();

		// RequestJoinGroup() is the CLIENT's request API: all it does is marshal RPC_AskJoinGroup as an
		// RplRcver.Server RPC, which only travels when the caller is a proxy. This method runs on the
		// server, so in any replicated session (dedicated OR hosted) the request would go nowhere and the
		// player would never actually join the group we just made for them - leaving an empty, leaderless
		// group and a groupless player. Vanilla itself calls the handler directly from its own server-side
		// code (SCR_PlayerControllerGroupComponent.RpcAsk_CreateGroupWithData:749) - do the same.
		// (The modded RequestJoinGroup override from Phase 2 T2.2 now does this branch too; the explicit
		// branch here documents the hazard and is correct either way.)
		if (Replication.IsServer())
			groupController.RPC_AskJoinGroup(groupID);
		else
			groupController.RequestJoinGroup(groupID);

		// RPC_AskJoinGroup assigns m_iGroupID synchronously on the server
		// (Groups/SCR_PlayerControllerGroupComponent.c:889-895), so the result is readable right now.
		int joinedGroupId = groupController.GetGroupID();
		if (joinedGroupId != groupID)
		{
			Print("[Overthrow] EnsureOwnGroup: player " + playerId + " did NOT join group " + groupID + " (they are in " + joinedGroupId + ") - the group exists but nobody is in it", LogLevel.WARNING);
			return -1;
		}

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		string origin = "created";
		if (adopted)
			origin = "adopted empty";

		Print("[Overthrow] " + origin + " group " + groupID + " for player " + playerName + " (ID: " + playerId + ")", LogLevel.NORMAL);
		Print("[Overthrow] Group faction: " + faction.GetFactionKey() + ", Player entity: " + GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId), LogLevel.NORMAL);

		return groupID;
	}

	//------------------------------------------------------------------------------------------------
	//! An EMPTY playable group of this faction that is safe to hand to a player, if there is one.
	//!
	//! WHY REUSE BEFORE CREATE. Vanilla creates a playable group for a faction the moment its first
	//! player is assigned to it (SCR_Faction.m_bEnableAutoGroupCreationWhenFull defaults to 1, so
	//! SCR_GroupsManagerComponent.OnPlayerFactionChanged:1112-1114 makes one) and joins nobody to it.
	//! Nothing ever deletes a group that never had a player, so that stray sits empty forever - and an
	//! empty faction group makes TryFindEmptyGroup() non-null, which permanently disables BOTH
	//! CanCreateNewGroup() (:1347) and RPC_AskCreateGroup() (:667). "Create new group" is the only way
	//! vanilla lets a player leave a group they joined, so the stray silently traps everyone in whatever
	//! group they are in. Adopting the empty group instead of adding a second one dissolves the stray on
	//! the first spawn, and keeps group churn (and therefore radio-frequency consumption, R12) down for
	//! every later leave/rejoin.
	//!
	//! THE DELETION-QUEUE CHECK IS LOAD-BEARING. A group emptied earlier in this same frame has already
	//! been queued by vanilla's own removal handler (:765) and will be destroyed next frame by
	//! DeleteGroups (:1647-1654) WITHOUT re-testing whether it is still empty. Adopting one would hand
	//! the player a group that disappears a frame later - a live player at GetGroupID() == -1, which is
	//! the exact state this manager exists to prevent.
	//!
	//! \param[in] groupsManager Live groups manager
	//! \param[in] faction Faction the group must belong to
	//! \return An empty, not-doomed playable group, or null when a new one has to be created
	protected SCR_AIGroup FindAdoptableGroup(notnull SCR_GroupsManagerComponent groupsManager, notnull Faction faction)
	{
		SCR_AIGroup emptyGroup = groupsManager.TryFindEmptyGroup(faction);
		if (!emptyGroup)
			return null;

		if (groupsManager.IsGroupQueuedForDeletionOVT(emptyGroup))
			return null;

		return emptyGroup;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a group as this player's own: named after them, private, and not un-privatable.
	//!
	//! SetCustomName is the setter the UI actually reads - the Group tab draws the callsign plus
	//! GetCustomName() (UI/Menu/SCR_GroupHelperUI.c:9-24); SCR_AIGroup has no SetName at all (that is the
	//! engine's ENTITY name setter, which no UI reads). All three calls apply locally AND broadcast
	//! (Entities/SCR_AIGroup.c:744-747, :535-539, :548-552), so all three are correct server entry points.
	//!
	//! SetPrivacyChangeable(false) does two jobs: it stops a player flipping their own group public from
	//! the vanilla attributes UI, and it blocks vanilla's AUTO-UNLOCK
	//! (Groups/SCR_GroupsManagerComponent.c:748 for an emptied group, :759 for the faction's last group),
	//! both of which call SetPrivate(false) only when IsPrivacyChangeable(). Accepted cost:
	//! SCR_GroupTileButton.CheckLeaderOptions (:1220-1228) also hides the RENAME button - fine, the group
	//! is named after its owner by design.
	//!
	//! \param[in] group Group to brand
	//! \param[in] playerId Player the group belongs to
	protected void BrandOwnGroup(notnull SCR_AIGroup group, int playerId)
	{
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		group.SetCustomName(playerName, playerId);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
	}

	//------------------------------------------------------------------------------------------------
	//! A player joined a group. SERVER ONLY (static broadcast invoker - see OnPostInit).
	//!
	//! Fired from RPC_DoAddPlayer (Entities/SCR_AIGroup.c:1277-1281), which the authority entry point
	//! AddPlayer() calls locally BEFORE broadcasting (:1408-1409), and which has already inserted the
	//! player into m_aPlayerIDs - so GetPlayerCount() below includes them.
	//!
	//! \param[in] group Group joined
	//! \param[in] playerID Player who joined
	protected void OnGroupPlayerAdded(SCR_AIGroup group, int playerID)
	{
		if (!Replication.IsServer())
			return;

		if (!group)
			return;

		ClaimGroupIfAlone(group, playerID);

		// Read BEFORE the move so the projection can be built from the pure helper rather than from a
		// count that already includes the arrivals.
		int slaveAiBefore = 0;
		SCR_AIGroup slaveGroup = group.GetSlave();
		if (slaveGroup)
			slaveAiBefore = slaveGroup.GetAgentsCount();

		int recruitsFollowed = MoveOwnedRecruitsIn(group, playerID);

		NotifyLeaderOfArrivingRecruits(group, playerID, recruitsFollowed);

		WarnIfAiBudgetExceeded(group, playerID, slaveAiBefore, recruitsFollowed);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the joiner AND the group's leader when this join takes the group's slave group over the
	//! configured AI budget. TASK T6.2.
	//!
	//! IT WARNS AND NOTHING ELSE. The join is not refused and no recruit is dropped: a refusal strands a
	//! player who asked to be in a group, and a silent drop orphans live AI in the world with nothing
	//! commanding them. Both are worse outcomes than a group that runs heavy, and both are invisible to
	//! the players who caused them.
	//!
	//! DISABLED BY DEFAULT. MAX_SLAVE_AI_PER_GROUP ships at 0 and ExceedsAiBudget() answers false for any
	//! budget <= 0, so nothing here fires until somebody has actually measured the cost (goal G11) and
	//! set a number. The arithmetic is the Logic-tier-pinned helper, not a fresh comparison.
	//!
	//! It also stays silent when nothing arrived: a join that adds no AI cannot have pushed the group
	//! over anything, and repeating the warning for every later joiner would be noise.
	//!
	//! \param[in] group Group that was joined
	//! \param[in] playerID Player who joined
	//! \param[in] slaveAiBefore Agents in the group's slave group before the move
	//! \param[in] recruitsFollowed How many recruits the move actually placed
	protected void WarnIfAiBudgetExceeded(notnull SCR_AIGroup group, int playerID, int slaveAiBefore, int recruitsFollowed)
	{
		if (recruitsFollowed < 1)
			return;

		int projected = OVT_GroupRecruitTransfer.ProjectSlaveAiCount(slaveAiBefore, recruitsFollowed);
		if (!OVT_GroupRecruitTransfer.ExceedsAiBudget(projected, MAX_SLAVE_AI_PER_GROUP))
			return;

		Print("[Overthrow] Group " + group.GetGroupID() + " now holds " + projected + " AI in its slave group, over the configured budget of " + MAX_SLAVE_AI_PER_GROUP + " - warned the joiner and the leader, join allowed", LogLevel.WARNING);

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (!notify)
			return;

		string projectedText = projected.ToString();
		string budgetText = MAX_SLAVE_AI_PER_GROUP.ToString();

		notify.SendTextNotification("GroupAiBudgetExceeded", playerID, projectedText, budgetText);

		int leaderId = group.GetLeaderID();
		if (leaderId > 0 && leaderId != playerID)
			notify.SendTextNotification("GroupAiBudgetExceeded", leaderId, projectedText, budgetText);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the group's LEADER, non-blockingly, that somebody just joined bringing AI they can command.
	//!
	//! Goal G8. This is a text notification, never a dialog: the leader did not ask for this and must not
	//! be interrupted mid-firefight. OVT_NotificationManagerComponent.SendTextNotification is the whole
	//! mechanism - no ENotification enum value is added (the enum lives in vanilla and a modded enum
	//! would also need a vanilla notification preset).
	//!
	//! SILENT WHEN NOTHING ARRIVED. The count is what MoveRecruitsToGroup actually PLACED, not what the
	//! joining player owns on paper, so an owner whose recruits are all offline produces no notification
	//! (requirement F5) and the number the leader reads is the number they can actually order about.
	//!
	//! SILENT WHEN THE JOINER IS THE LEADER. Spawning into your own group, and being promoted into an
	//! empty one, both come through OnGroupPlayerAdded; nobody needs telling about their own recruits.
	//!
	//! \param[in] group Group that was joined
	//! \param[in] playerID Player who joined
	//! \param[in] recruitsFollowed How many of their recruits were placed in this group's slave group
	protected void NotifyLeaderOfArrivingRecruits(notnull SCR_AIGroup group, int playerID, int recruitsFollowed)
	{
		if (recruitsFollowed < 1)
			return;

		int leaderId = group.GetLeaderID();
		if (leaderId < 1)
			return;

		if (leaderId == playerID)
			return;

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (!notify)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerID);

		// playerId argument targets exactly one player: RpcDo_RcvTextNotification is a broadcast RPC that
		// every other client drops on the "not me" check (OVT_NotificationManagerComponent.c:178-182).
		notify.SendTextNotification("PlayerJoinedWithRecruits", leaderId, playerName, recruitsFollowed.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Brands a group the joining player is now alone in as that player's own. SERVER ONLY.
	//!
	//! \param[in] group Group joined
	//! \param[in] playerID Player who joined
	protected void ClaimGroupIfAlone(notnull SCR_AIGroup group, int playerID)
	{
		// A group whose ONLY member is this player IS this player's own group under Overthrow's model,
		// however they got there. The case that matters: vanilla's "create new group" is the only exit
		// from a shared group, and the group it makes is public and unnamed
		// (SCR_PlayerControllerGroupComponent.RPC_AskCreateGroup:675-683 does nothing but create+join).
		// Without this the player would leave a friend's group into an anonymous public one.
		//
		// IsPrivate() is the decisive skip: EnsureOwnGroup() brands BEFORE it joins the owner, and
		// SetPrivate applies m_bPrivate synchronously (RPC_SetPrivate, :1342-1344), so a group this
		// manager made is already private here and is left alone.
		if (group.GetPlayerCount() != 1)
			return;

		if (group.IsPrivate())
			return;

		Print("[Overthrow] Player " + playerID + " is alone in public group " + group.GetGroupID() + " - claiming it as their own", LogLevel.NORMAL);
		BrandOwnGroup(group, playerID);
	}

	//------------------------------------------------------------------------------------------------
	//! RECRUITS FOLLOW THEIR OWNER IN. SERVER ONLY.
	//!
	//! Every online recruit this player owns is placed in this group's slave group, where the group's
	//! LEADER commands them - pure vanilla behaviour, and the whole point of the feature. Ownership is
	//! untouched: dismiss, rename, inventory, loadout, show-on-map and kill XP all stay with the owner
	//! in every group (goal G5). Nothing in this path writes an owner field.
	//!
	//! ON A NORMAL SWITCH THIS IS THE SECOND HALF OF A PULL-THEN-PLACE PAIR. Vanilla MovePlayerToGroup
	//! (Groups/SCR_GroupsManagerComponent.c:109-134) removes from the old group at :114 and adds to the
	//! new one at :125 IN THE SAME FRAME, so OnGroupPlayerRemoved has already pulled these recruits out
	//! of the old group's slave before this runs. Net result is exactly one slave-group membership, and
	//! the two log lines (pulled N out / moved N in) are the running proof of it.
	//!
	//! \param[in] group Group joined - the MASTER group; recruits go into its slave
	//! \param[in] playerID Player who joined
	//! \return How many recruits actually followed the player in - 0 on every bail. Phase 5's leader
	//!         notification keys off this, so a bail must stay silent rather than report a paper count.
	protected int MoveOwnedRecruitsIn(notnull SCR_AIGroup group, int playerID)
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return 0;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return 0;

		string persistentId = players.GetPersistentIDFromPlayerID(playerID);
		if (persistentId.IsEmpty())
		{
			// NORMAL, not WARNING: the very first group a player is put in can be created before their
			// runtime->persistent id mapping is registered, and a player Overthrow has not identified yet
			// has no recruit roster to move either. A returning player's roster is brought back a few
			// seconds later by OVT_RecruitManagerComponent.RespawnRecruitsDelayed regardless.
			Print("[Overthrow] Player " + playerID + " joined group " + group.GetGroupID() + " before they had a persistent id - no recruits moved", LogLevel.NORMAL);
			return 0;
		}

		// A DOOMED GROUP IS NOT A DESTINATION. A group emptied earlier in this same frame is already in
		// vanilla's deletion queue and will be destroyed next frame WITHOUT a re-test of emptiness
		// (DeleteGroups, Groups/SCR_GroupsManagerComponent.c:1647-1654), so placing recruits in its
		// slave group would strand them a frame later. Leaving them where they are costs nothing: the
		// player ends that frame at GetGroupID() == -1, RestoreOwnGroupDeferred gives them their own
		// group, and THAT add comes straight back through this method.
		//
		// The mirror check is deliberately NOT made in the removal path - see MoveOwnedRecruitsOut.
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (groupsManager && groupsManager.IsGroupQueuedForDeletionOVT(group))
		{
			Print("[Overthrow] Group " + group.GetGroupID() + " is queued for deletion - not moving player " + playerID + "'s recruits into it", LogLevel.WARNING);
			return 0;
		}

		// LIVENESS COMES FROM THE ENGINE'S PLAYER MANAGER, NOT FROM THE PLAYER RECORD (decision D9).
		// A recruit's commandability is a question about session state, and the record's own liveness
		// accessor answers from Overthrow's connect/disconnect bookkeeping instead - a different fact
		// that has been out of step with the session before (risk R9). In practice this is true here (a
		// player without a controller cannot be added to a group), and it is resolved rather than
		// assumed so that the offline rule has exactly ONE source in this feature.
		bool ownerOnline = (GetGame().GetPlayerManager().GetPlayerController(playerID) != null);

		return recruits.MoveRecruitsToGroup(persistentId, group, ownerOnline);
	}

	//------------------------------------------------------------------------------------------------
	//! A player left a group. SERVER ONLY (static broadcast invoker - see OnPostInit).
	//!
	//! Fired from RPC_DoRemovePlayer (Entities/SCR_AIGroup.c:1496-1504), which the authority entry point
	//! RemovePlayer() calls locally BEFORE broadcasting (:1538-1539). Every membership loss comes through
	//! here regardless of cause: a group switch (MovePlayerToGroup:114), a faction change
	//! (SCR_GroupsManagerComponent.OnPlayerFactionChanged:1130) and a disconnect
	//! (SCR_AIGroup.OnPlayerDisconnected:1186) all call RemovePlayer.
	//!
	//! THE EX-GROUP IS STILL FULLY READABLE FOR THE REST OF THIS FRAME - VERIFIED. Vanilla's own
	//! subscriber to this same invoker, SCR_GroupsManagerComponent.OnGroupPlayerRemoved
	//! (Groups/SCR_GroupsManagerComponent.c:735, subscribed at EOnInit :1724), calls
	//! DeleteGroupDelayed(group) at :765 once the group is empty. DeleteGroupDelayed (:721-730) only
	//! QUEUES the group and schedules the drain with GetGame().GetCallqueue().Call(DeleteGroups) at :727;
	//! DeleteGroups (:1647-1654) is what actually destroys it, NEXT FRAME. Phase 4 depends on this: it
	//! pulls the leaving player's recruits out of group.GetSlave() synchronously right here, before the
	//! group can go away. The log line below is the running proof (slave non-null on removal).
	//!
	//! \param[in] group Group left
	//! \param[in] playerID Player who left
	protected void OnGroupPlayerRemoved(SCR_AIGroup group, int playerID)
	{
		if (!Replication.IsServer())
			return;

		if (!group)
			return;

		bool hasSlave = (group.GetSlave() != null);
		Print("[Overthrow] Player " + playerID + " removed from group " + group.GetGroupID()
			+ " (" + group.GetPlayerCount() + " player(s) left, slave group readable: " + hasSlave + ")", LogLevel.NORMAL);

		// SYNCHRONOUS, not deferred - this is the half that cannot wait a frame.
		MoveOwnedRecruitsOut(group, playerID);

		// Deferred by exactly one frame - see RestoreOwnGroupDeferred for why re-checking is the whole
		// mechanism.
		GetGame().GetCallqueue().CallLater(RestoreOwnGroupDeferred, 0, false, playerID);
	}

	//------------------------------------------------------------------------------------------------
	//! RECRUITS FOLLOW THEIR OWNER OUT. SERVER ONLY, and SYNCHRONOUS ON PURPOSE.
	//!
	//! This is what guarantees that none of a departing owner's recruits remain in the ex-group's slave
	//! group (goal G4). Deferring it by even one frame would be wrong three ways:
	//!   1. the ex-leader would keep command of somebody else's AI for that frame;
	//!   2. an emptied group is queued for destruction by vanilla's own subscriber to this same invoker
	//!      and destroyed on the next frame (DeleteGroupDelayed -> DeleteGroups,
	//!      Groups/SCR_GroupsManagerComponent.c:721-730, :1647-1654) - the window is exactly this frame;
	//!   3. UnregisterGroup only deletes the slave group if it has NO agents left (:1035-1040), so
	//!      recruits left behind become AI in a leaked slave group with no master. Vanilla's own comment
	//!      there is "mourTodo: handle what the AIs should do in case their master group is deleted".
	//! The "slave group readable" flag logged above is the running proof that the window is open when
	//! this runs.
	//!
	//! THE DELETION-QUEUE GUARD IS DELIBERATELY NOT CONSULTED HERE. A doomed ex-group is precisely the
	//! case where the pull-out matters most; skipping it would strand the recruits it exists to save.
	//!
	//! A DISCONNECT REACHES THIS TOO - THAT IS TASK T6.3, AND IT IS WHY THE ID IS LOOKED UP TWICE BELOW.
	//! A disconnect is a removal like any other (SCR_AIGroup.OnPlayerDisconnected:1175-1187 calls
	//! RemovePlayer), so this handler runs for one - but by then the departing player's
	//! runtime->persistent id mapping is already gone: OVT_OverthrowGameMode.OnPlayerDisconnected drops
	//! it with ClearPlayerIdMappings() at :836 and only calls super at :838, which is what fires
	//! SCR_BaseGameMode's disconnect invoker that SCR_AIGroup subscribed to. OnPlayerDisconnecting()
	//! above caches the id one line earlier, at :832, and the fallback below is what consumes it - so the
	//! pull-out happens with the mapping effectively intact, synchronously, in the removal frame.
	//! RemoveRecruitsFromGroup() needs no player controller precisely so this works for a player who
	//! no longer has one.
	//!
	//! \param[in] group Group left - the MASTER group; recruits come out of its slave
	//! \param[in] playerID Player who left
	protected void MoveOwnedRecruitsOut(notnull SCR_AIGroup group, int playerID)
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return;

		string persistentId = players.GetPersistentIDFromPlayerID(playerID);
		if (persistentId.IsEmpty())
		{
			// T6.3: a disconnect got here first and cleared the mapping. The id was cached one line
			// before that happened - use it rather than abandoning somebody else's AI in this group.
			persistentId = m_mDepartingPersistentIds.Get(playerID);

			if (persistentId.IsEmpty())
			{
				Print("[Overthrow] Player " + playerID + " left group " + group.GetGroupID() + " and no persistent id could be resolved for them (not even a cached disconnect id) - any recruits they own stay in that group's slave group until the offline despawn collects them", LogLevel.WARNING);
				return;
			}

			Print("[Overthrow] Player " + playerID + " disconnected out of group " + group.GetGroupID() + " - pulling their recruits out using the cached persistent id", LogLevel.NORMAL);
		}

		recruits.RemoveRecruitsFromGroup(persistentId, group);
	}

	//------------------------------------------------------------------------------------------------
	//! One frame after a player left a group: if they are still here and still have no group, give them
	//! their own back. SERVER ONLY.
	//!
	//! WHY DEFERRED, AND WHY RE-CHECKED. Vanilla MovePlayerToGroup
	//! (Groups/SCR_GroupsManagerComponent.c:109-134) calls previousGroup.RemovePlayer() at :114 and THEN
	//! newGroup.AddPlayer() at :125, both in the same frame, so a perfectly normal group switch fires
	//! Removed(old) before Added(new). Reacting to the removal immediately would recreate a group for a
	//! player who is merely mid-move. Waiting one frame and re-reading GetGroupID() tells the cases apart
	//! with one test:
	//!
	//!   - normal switch  -> the new group id  -> no-op
	//!   - genuine leave  -> -1                -> restore (goal G6)
	//!   - disconnect     -> no player controller at all -> no-op
	//!
	//! (Vanilla's own in-flight marker m_iMovingPlayerToGroupID (:64) is protected and not reachable from
	//! an invoker subscriber, which is why the state is re-read rather than asked for.)
	//!
	//! THE FOURTH CASE - A REJECTED JOIN - IS CLOSED UPSTREAM, NOT HERE. The plan expected a full-group
	//! rejection to land in this method at -1. Phase 2's T2.3 override rejects BEFORE vanilla empties the
	//! player's current group, so no removal happens, this handler is never scheduled, and the player
	//! simply stays where they were - a strictly better outcome than being restored after the fact. The
	//! residual shape (a player who is ALREADY at -1 and whose join is refused) fires no invoker either
	//! and is covered by OVT_SpawnLogic's own retry, not by this.
	//!
	//! \param[in] playerID Player who was removed from a group one frame ago
	protected void RestoreOwnGroupDeferred(int playerID)
	{
		if (!Replication.IsServer())
			return;

		// Disconnect: the controller is gone, and a departed player must not be given a group (it would
		// be an empty leaderless group nobody deletes).
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerID);
		if (!playerController)
			return;

		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
		if (!groupController)
			return;

		// Normal switch (or anything else that already put them somewhere): nothing to do.
		if (groupController.GetGroupID() != -1)
			return;

		Print("[Overthrow] Player " + playerID + " ended a frame with no group - restoring their own", LogLevel.NORMAL);

		// NOTE: m_OnPlayerGroupCreated is deliberately NOT fired from here (decision D6). That invoker
		// drives a full recruit RESPAWN (OVT_RecruitManagerComponent.OnPlayerGroupCreated), which is the
		// right answer for a player arriving in the world and the wrong one for a player moving between
		// groups. Phase 4 moves live recruits directly from the two membership handlers instead.
		EnsureOwnGroup(playerID);
	}
}
