//! Overthrow's UX layer over the vanilla Group tab: a confirmation dialog on BOTH join routes, a
//! client-side pre-check that says WHY a join is impossible, and a paragraph explaining the model.
//!
//! WHY THIS CLASS AND NOT A SUBCLASS. SCR_GroupSubMenuBase.JoinSelectedGroup()
//! (Groups/SCR_GroupSubMenuBase.c:259) and AcceptInvite() (:285) are the ONLY two ways a player ever
//! asks to change group in the whole vanilla script tree - re-verified for Phase 5 by grep:
//! JoinSelectedGroup has exactly one caller (:359, the Join button's m_OnActivated) and
//! SCR_PlayerControllerGroupComponent.AcceptInvite has exactly one (:287, inside the method below).
//! Neither of the two subclasses overrides either method - SCR_GroupSubMenuPlayerlist overrides
//! OnMenuUpdate/OnTabCreate/OnTabShow/OnTabHide/UpdateGroups and SCR_GroupSubMenu overrides
//! OnTabCreate/OnTabShow/OnTabHide - so ONE modded class covers the pause-menu Group tab AND the
//! deploy menu's group selector, and the deploy route keeps working unchanged.
//! SCR_GroupInviteStickyNotificationUIComponent is display-only (its OnButton just opens the player
//! list), so it is not a third seam.
//!
//! NOTHING HERE CHANGES MEMBERSHIP. Every path either calls super - i.e. vanilla's own
//! PlayerRequestToJoinPrivateGroup / RequestJoinGroup client request - or sends nothing at all. The
//! dialog only DEFERS the existing vanilla client call behind a confirm; no new RPC crosses the wire
//! in either direction (decision D1).
//!
//! WHY THE INVITEE IS ASKED TOO. Accepting an invite is the moment the INVITEE hands command of their
//! own recruits to somebody else. That is exactly the fact the dialog exists to state, so AcceptInvite
//! gets the same treatment as JoinSelectedGroup (goal G3).
//!
//! VANILLA-UPDATE HAZARD (risk R10): this mirrors vanilla method names and the meaning of
//! GetSelectedGroupID(). See the check-list in docs/features/core/player-groups/context.md.
modded class SCR_GroupSubMenuBase
{
	//! Overthrow's dialog presets. Same conf every other Overthrow dialog opens; the GUID prefix is
	//! carried over from the existing call sites (OVT_CampMenuContext.c:76, OVT_RecruitsContext.c:371),
	//! which each spell it differently and all resolve - the path is what resolves this file.
	protected const ResourceName OVT_GROUP_DIALOGS = "{5D86A311C893DBF0}Configs/UI/Dialogs/DialogPresets_Campaign.conf";

	protected const string OVT_DLG_JOIN_CONFIRM = "JOIN_GROUP_CONFIRM";
	protected const string OVT_DLG_JOIN_CONFIRM_RECRUITS = "JOIN_GROUP_CONFIRM_RECRUITS";
	protected const string OVT_DLG_JOIN_BLOCKED = "JOIN_GROUP_BLOCKED";

	protected const string OVT_KEY_CONFIRM_TITLE = "#OVT-PlayerGroups_JoinConfirmTitle";
	protected const string OVT_KEY_CONFIRM_MSG = "#OVT-PlayerGroups_JoinConfirmMessage";
	protected const string OVT_KEY_CONFIRM_MSG_RECRUITS = "#OVT-PlayerGroups_JoinConfirmRecruitsMessage";
	protected const string OVT_KEY_BLOCKED_TITLE = "#OVT-PlayerGroups_JoinBlockedTitle";
	protected const string OVT_KEY_BLOCKED_FULL = "#OVT-PlayerGroups_JoinBlockedFull";
	protected const string OVT_KEY_BLOCKED_UNAVAILABLE = "#OVT-PlayerGroups_JoinBlockedUnavailable";
	protected const string OVT_KEY_BLOCKED_GONE = "#OVT-PlayerGroups_JoinBlockedGone";
	protected const string OVT_KEY_MODEL_EXPLAINER = "#OVT-PlayerGroups_ModelExplainer";

	//! English fallbacks, used ONLY while a key has not been exported to the runtime
	//! localization_Overthrow.<lang>.conf yet (risk R11). The .st master is the editable source and the
	//! user regenerates the exports in the Workbench; until they do, this is what stops the player from
	//! reading a raw "#OVT-..." key. They cost nothing once the export lands - see OVT_TranslateOrEnglish.
	protected const string OVT_EN_CONFIRM_TITLE = "Join Group";
	protected const string OVT_EN_CONFIRM_MSG = "%1 will be able to command your recruits while you are in their group.";
	protected const string OVT_EN_CONFIRM_MSG_RECRUITS = "Your %1 recruits will move to %2's group and take orders from %2. You still own them - they return to you when you leave the group, change groups, or reconnect.";
	protected const string OVT_EN_BLOCKED_TITLE = "Cannot Join Group";
	protected const string OVT_EN_BLOCKED_FULL = "That group is full. Nobody else can join it until somebody leaves.";
	protected const string OVT_EN_BLOCKED_UNAVAILABLE = "You cannot join that group right now.";
	protected const string OVT_EN_BLOCKED_GONE = "That group no longer exists.";
	protected const string OVT_EN_MODEL_EXPLAINER = "Your group is private and named after you. Your recruits follow you into any group you join - that group's leader commands them while you are there, but you keep ownership and they come back when you leave. To leave a group, use Create new group.";

	//! The paragraph under the group list. Added from OnTabCreate rather than by forking either vanilla
	//! group layout, so a Reforger update to those layouts cannot silently drop it.
	protected const ResourceName OVT_EXPLAINER_LAYOUT = "{6A7D9B2E000000B0}UI/Layouts/Menu/GroupModelExplainer.layout";

	//! Container both vanilla group layouts already provide under the group list
	//! (UI/layouts/Menus/GroupSlection/GroupMenuPlayerlist.layout:143 and SelectGroupMenu.layout:132).
	protected const string OVT_EXPLAINER_PARENT = "GrouplistFooter";
	protected const string OVT_EXPLAINER_TEXT_WIDGET = "OVT_GroupModelExplainerText";

	//! The join the open dialog is asking about. -1 when no dialog is pending.
	protected int m_iOvtPendingJoinGroupId = -1;

	//! True when the pending dialog is for an INVITE (AcceptInvite) rather than a request (Join).
	protected bool m_bOvtPendingJoinIsInvite;

	//! Live confirmation/blocked dialog, kept only so OnTabRemove can detach from its invokers.
	protected SCR_ConfigurableDialogUi m_OvtJoinDialog;

	//------------------------------------------------------------------------------------------------
	//! JOIN ROUTE 1 - the Join / Request-to-join button (and its MenuJoinGroup shortcut, KC_J / pad Y).
	//!
	//! Vanilla sends the request immediately here. Overthrow asks first, and only the dialog's confirm
	//! handler reaches super. Cancel sends nothing at all - the other player never sees a request.
	override protected void JoinSelectedGroup()
	{
		if (!m_PlayerGroupController)
		{
			// Nothing to resolve a group or a recruit count against. Behave exactly like vanilla rather
			// than swallowing the player's input.
			super.JoinSelectedGroup();
			return;
		}

		// Nothing selected at all. Not a failure worth a dialog - the Join button is not even visible in
		// this state (SetSelectedGroupButtonStatus:333) - so behave exactly like vanilla, which no-ops.
		if (m_PlayerGroupController.GetSelectedGroupID() < 0)
		{
			super.JoinSelectedGroup();
			return;
		}

		SCR_AIGroup group = OvtFindSelectedGroup();
		if (!group)
		{
			OvtShowBlockedDialog(OVT_KEY_BLOCKED_GONE, OVT_EN_BLOCKED_GONE);
			return;
		}

		// YOUR OWN GROUP: vanilla already refuses this server-side (RPC_AskJoinGroup returns when the
		// target group is the one you are in), so do not put a "someone else will command your recruits"
		// dialog in front of a no-op. Straight to super, which no-ops the same way it does today.
		if (group.GetGroupID() == m_PlayerGroupController.GetGroupID())
		{
			super.JoinSelectedGroup();
			return;
		}

		if (!OvtCheckCanJoin(group))
			return;

		OvtOpenJoinConfirmDialog(group, false);
	}

	//------------------------------------------------------------------------------------------------
	//! JOIN ROUTE 2 - accepting an invite (GroupAcceptInvite, KC_U / pad Y held).
	//!
	//! The invitee is the one whose recruits change hands, so they get the same dialog. Blocking here
	//! leaves the invite pending on purpose: the player can accept later when a slot frees up.
	override void AcceptInvite()
	{
		if (!m_PlayerGroupController)
		{
			super.AcceptInvite();
			return;
		}

		// Nothing selected: vanilla's AcceptInvite already no-ops on this
		// (Groups/SCR_PlayerControllerGroupComponent.c:436), and the Accept button is hidden in this
		// state anyway. No dialog.
		if (m_PlayerGroupController.GetSelectedGroupID() < 0)
		{
			super.AcceptInvite();
			return;
		}

		SCR_AIGroup group = OvtFindSelectedGroup();
		if (!group)
		{
			// A SELECTED group that no longer resolves. Vanilla bails silently here
			// (Groups/SCR_PlayerControllerGroupComponent.c:443-445); say why instead (goal G9).
			OvtShowBlockedDialog(OVT_KEY_BLOCKED_GONE, OVT_EN_BLOCKED_GONE);
			return;
		}

		if (group.GetGroupID() == m_PlayerGroupController.GetGroupID())
		{
			super.AcceptInvite();
			return;
		}

		if (!OvtCheckCanJoin(group))
			return;

		OvtOpenJoinConfirmDialog(group, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the explanatory paragraph. Runs after super so m_wMenuRoot is set, and independently of
	//! m_PlayerGroupController (vanilla's OnTabCreate returns early without one, but the text is still
	//! worth showing).
	override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
	{
		super.OnTabCreate(menuRoot, buttonsLayout, index);

		OvtCreateModelExplainer();
	}

	//------------------------------------------------------------------------------------------------
	//! Detaches from the dialog. The dialog is a SEPARATE menu and outlives this tab, so a dialog left
	//! open while the Group menu closes would otherwise call back into a dead sub-menu component. The
	//! explainer widget needs no cleanup - it is a child of the tab's own layout and dies with it.
	override void OnTabRemove()
	{
		OvtDetachJoinDialog();

		super.OnTabRemove();
	}

	//------------------------------------------------------------------------------------------------
	//! The group the player currently has selected, or null.
	//! \return Selected group, or null when nothing is selected or the group is gone
	protected SCR_AIGroup OvtFindSelectedGroup()
	{
		if (!m_PlayerGroupController)
			return null;

		SCR_GroupsManagerComponent groupManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupManager)
			return null;

		int selectedGroupId = m_PlayerGroupController.GetSelectedGroupID();
		if (selectedGroupId < 0)
			return null;

		return groupManager.FindGroup(selectedGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT-SIDE PRE-CHECK. Shows a message and returns false when this join cannot work.
	//!
	//! This is the MESSAGE, not the guarantee - the guarantee is server-side (Phase 2 T2.3 rejects a
	//! full-group move before it empties the joiner's current group). It exists because vanilla's own
	//! visibility gate is stale by design: SCR_GroupTileButton.SetupJoinGroupButton (:703-765) hides the
	//! Join button when CanPlayerJoinGroup fails, but it only re-runs on RefreshPlayers - so a group
	//! that filled up since the tile was last drawn still shows an enabled Join button, and pressing it
	//! used to do nothing at all.
	//!
	//! CanPlayerJoinGroup (Groups/SCR_PlayerControllerGroupComponent.c:136-245) is vanilla's own answer
	//! and covers faction, full, already-in-it, commander role and rank in one call. IsFull() is tested
	//! separately only to pick the more specific message.
	//!
	//! \param[in] group Group the player is trying to join
	//! \return True when the request may be sent
	protected bool OvtCheckCanJoin(notnull SCR_AIGroup group)
	{
		if (group.IsFull())
		{
			OvtShowBlockedDialog(OVT_KEY_BLOCKED_FULL, OVT_EN_BLOCKED_FULL);
			return false;
		}

		if (!m_PlayerGroupController)
			return true;

		if (!m_PlayerGroupController.CanPlayerJoinGroup(m_PlayerGroupController.GetPlayerID(), group))
		{
			OvtShowBlockedDialog(OVT_KEY_BLOCKED_UNAVAILABLE, OVT_EN_BLOCKED_UNAVAILABLE);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the confirmation dialog and remembers what it is about. NOTHING IS SENT UNTIL CONFIRM.
	//!
	//! The recruit count is read locally from OVT_RecruitManagerComponent's JIP'd roster - no query
	//! crosses the wire (decision D1).
	//!
	//! \param[in] group Group being joined
	//! \param[in] isInvite True when this came from AcceptInvite rather than the Join button
	protected void OvtOpenJoinConfirmDialog(notnull SCR_AIGroup group, bool isInvite)
	{
		string leaderName = OvtResolveLeaderName(group);
		int recruitCount = OvtLocalRecruitCount();

		string presetTag = OVT_DLG_JOIN_CONFIRM;
		if (recruitCount > 0)
			presetTag = OVT_DLG_JOIN_CONFIRM_RECRUITS;

		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(OVT_GROUP_DIALOGS, presetTag);
		if (!dialog)
		{
			// The preset could not be built. Refusing the player's input silently would be worse than a
			// missing warning, so fall through to the vanilla behaviour and say so in the log.
			Print("[Overthrow] Group join confirmation preset '" + presetTag + "' could not be opened - falling back to the unconfirmed vanilla join", LogLevel.WARNING);
			OvtCallSuperJoin(isInvite);
			return;
		}

		dialog.SetTitle(OvtTranslateOrEnglish(OVT_KEY_CONFIRM_TITLE, OVT_EN_CONFIRM_TITLE));

		if (recruitCount > 0)
			dialog.SetMessage(OvtTranslateOrEnglish2(OVT_KEY_CONFIRM_MSG_RECRUITS, OVT_EN_CONFIRM_MSG_RECRUITS, recruitCount.ToString(), leaderName));
		else
			dialog.SetMessage(OvtTranslateOrEnglish1(OVT_KEY_CONFIRM_MSG, OVT_EN_CONFIRM_MSG, leaderName));

		m_iOvtPendingJoinGroupId = group.GetGroupID();
		m_bOvtPendingJoinIsInvite = isInvite;
		m_OvtJoinDialog = dialog;

		dialog.m_OnConfirm.Insert(OvtOnJoinConfirmed);
		dialog.m_OnCancel.Insert(OvtOnJoinCancelled);
		dialog.m_OnClose.Insert(OvtOnJoinDialogClosed);
	}

	//------------------------------------------------------------------------------------------------
	//! The player confirmed. This is the ONLY place the vanilla join call is reached from the dialog.
	//!
	//! IT IS DELIBERATELY IDEMPOTENT. The pending group id is consumed on the FIRST line and any later
	//! entry falls out at the groupId < 0 test, so even if m_OnConfirm were invoked twice for one press
	//! - a gamepad 'a' is both DialogConfirm and MenuSelect on the focused button, and
	//! SCR_InputButtonComponent.OnInput has no double-fire guard of its own - exactly one join request
	//! is ever sent. Do not "simplify" the two lines below into a single read.
	protected void OvtOnJoinConfirmed()
	{
		int groupId = m_iOvtPendingJoinGroupId;
		bool isInvite = m_bOvtPendingJoinIsInvite;

		m_iOvtPendingJoinGroupId = -1;

		if (groupId < 0)
			return;

		if (!m_PlayerGroupController)
			return;

		SCR_GroupsManagerComponent groupManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupManager)
			return;

		// The group can be emptied, deleted or filled while the dialog is open, so the pre-check is run
		// again here rather than trusted from before it opened.
		//
		// NOTE FOR A REVIEWER: these two calls open a dialog from inside the CLOSING dialog's own confirm
		// invoker (SCR_ConfigurableDialogUi.OnConfirm invokes, THEN calls Close()), so for a moment two
		// dialogs are stacked before the lower one closes itself. It is left synchronous on purpose -
		// deferring through the call queue would outlive this sub-menu component, which is a worse
		// failure than an odd frame of focus, and the fallback if the stacking misbehaves is the silent
		// refusal that shipped in Phase 2. This is a rare race (T6.4 / T6.6 territory), not the normal
		// path, and it is called out for the play-test.
		SCR_AIGroup group = groupManager.FindGroup(groupId);
		if (!group)
		{
			OvtShowBlockedDialog(OVT_KEY_BLOCKED_GONE, OVT_EN_BLOCKED_GONE);
			return;
		}

		if (group.IsFull())
		{
			OvtShowBlockedDialog(OVT_KEY_BLOCKED_FULL, OVT_EN_BLOCKED_FULL);
			return;
		}

		// BOTH vanilla methods act on GetSelectedGroupID(), not on an argument, and a tile can take focus
		// back while the dialog is up (SCR_GroupTileButton.OnFocus calls SetSelectedGroupID). Put the
		// selection back on the group the player actually agreed to before delegating. Client-side UI
		// state only - SetSelectedGroupID just writes m_iUISelectedGroupID and fires a UI invoker.
		if (m_PlayerGroupController.GetSelectedGroupID() != groupId)
			m_PlayerGroupController.SetSelectedGroupID(groupId);

		OvtCallSuperJoin(isInvite);
	}

	//------------------------------------------------------------------------------------------------
	//! The player cancelled. Deliberately does nothing: no request is sent, the invite (if any) stays
	//! pending, and the other player never learns this happened.
	protected void OvtOnJoinCancelled()
	{
		m_iOvtPendingJoinGroupId = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires after confirm OR cancel, once the dialog has closed. Only drops the reference.
	protected void OvtOnJoinDialogClosed()
	{
		m_OvtJoinDialog = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the UNCHANGED vanilla join for whichever route the dialog came from. This is the only place
	//! in Overthrow that reaches either vanilla join call, and it is reached only from the dialog's
	//! confirm handler (or from the "the preset would not open" fallback).
	//!
	//! \param[in] isInvite True to run AcceptInvite, false to run JoinSelectedGroup
	protected void OvtCallSuperJoin(bool isInvite)
	{
		if (isInvite)
			super.AcceptInvite();
		else
			super.JoinSelectedGroup();
	}

	//------------------------------------------------------------------------------------------------
	//! A modal "here is why that did not work" dialog. Deliberately a dialog rather than a HUD
	//! notification: this menu covers the HUD, and a dialog is the one message surface that is
	//! guaranteed to be on top of it and operable with a pad (its single button is on DialogConfirm).
	//!
	//! \param[in] messageKey Localization key for the reason
	//! \param[in] englishFallback English text used until messageKey is exported (risk R11)
	protected void OvtShowBlockedDialog(string messageKey, string englishFallback)
	{
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(OVT_GROUP_DIALOGS, OVT_DLG_JOIN_BLOCKED);
		if (!dialog)
		{
			Print("[Overthrow] Group join refused (" + messageKey + ") but the '" + OVT_DLG_JOIN_BLOCKED + "' dialog preset could not be opened", LogLevel.WARNING);
			return;
		}

		dialog.SetTitle(OvtTranslateOrEnglish(OVT_KEY_BLOCKED_TITLE, OVT_EN_BLOCKED_TITLE));
		dialog.SetMessage(OvtTranslateOrEnglish(messageKey, englishFallback));
	}

	//------------------------------------------------------------------------------------------------
	//! Removes this component's handlers from a still-open dialog.
	protected void OvtDetachJoinDialog()
	{
		m_iOvtPendingJoinGroupId = -1;

		if (!m_OvtJoinDialog)
			return;

		m_OvtJoinDialog.m_OnConfirm.Remove(OvtOnJoinConfirmed);
		m_OvtJoinDialog.m_OnCancel.Remove(OvtOnJoinCancelled);
		m_OvtJoinDialog.m_OnClose.Remove(OvtOnJoinDialogClosed);
		m_OvtJoinDialog = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Who will be commanding the local player's recruits if this join goes through.
	//!
	//! The LEADER, not the group's custom name: Overthrow names a group after the player who created it,
	//! and leadership moves on when that player leaves (SCR_AIGroup.CheckForLeader promotes
	//! m_aPlayerIDs[0]), so the two can legitimately disagree. Falls back to the custom name and then to
	//! the translated callsign so the dialog always names something.
	//!
	//! \param[in] group Group being joined
	//! \return A display name for the group's leader
	protected string OvtResolveLeaderName(notnull SCR_AIGroup group)
	{
		int leaderId = group.GetLeaderID();
		if (leaderId > 0)
		{
			string leaderName = GetGame().GetPlayerManager().GetPlayerName(leaderId);
			if (!leaderName.IsEmpty())
				return leaderName;
		}

		string customName = group.GetCustomName();
		if (!customName.IsEmpty())
			return customName;

		return SCR_GroupHelperUI.GetTranslatedGroupName(group);
	}

	//------------------------------------------------------------------------------------------------
	//! How many recruits the LOCAL player owns, read from the client's own copy of the recruit roster.
	//!
	//! OVT_RecruitManagerComponent ships the whole table to clients through RplSave/RplLoad and keeps it
	//! live with broadcast RPCs, so this needs no server round trip (decision D1). A count of 0 - whether
	//! because the player has no recruits or because the roster has not arrived yet - simply selects the
	//! shorter dialog, which is still true and still warns about command transfer.
	//!
	//! \return Recruit count, 0 when it cannot be resolved
	protected int OvtLocalRecruitCount()
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return 0;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return 0;

		int localPlayerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		if (localPlayerId < 1)
			return 0;

		string persistentId = players.GetPersistentIDFromPlayerID(localPlayerId);
		if (persistentId.IsEmpty())
			return 0;

		return recruits.GetRecruitCount(persistentId);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the explanatory paragraph under the group list.
	//!
	//! Both vanilla group layouts already carry an empty "GrouplistFooter" VerticalLayout below the
	//! group list, so nothing is forked and a layout change upstream degrades to "the text is missing"
	//! rather than to a broken tab.
	protected void OvtCreateModelExplainer()
	{
		if (!m_wMenuRoot)
			return;

		Widget footer = m_wMenuRoot.FindAnyWidget(OVT_EXPLAINER_PARENT);
		if (!footer)
			return;

		// Never stack two copies if this component's tab is ever created twice.
		if (footer.FindAnyWidget(OVT_EXPLAINER_TEXT_WIDGET))
			return;

		Widget explainer = GetGame().GetWorkspace().CreateWidgets(OVT_EXPLAINER_LAYOUT, footer);
		if (!explainer)
			return;

		TextWidget explainerText = TextWidget.Cast(explainer.FindAnyWidget(OVT_EXPLAINER_TEXT_WIDGET));
		if (!explainerText)
			return;

		explainerText.SetText(OvtTranslateOrEnglish(OVT_KEY_MODEL_EXPLAINER, OVT_EN_MODEL_EXPLAINER));
	}

	//------------------------------------------------------------------------------------------------
	//! Localized text, or the English literal while the key is still missing from the runtime exports.
	//!
	//! WidgetManager.Translate() hands an unknown key straight back, which is exactly what would be
	//! shown to the player. Overthrow's .st master is edited by hand and the .<lang>.conf exports are
	//! regenerated separately in the Workbench, so there is always a window where a brand-new key does
	//! not resolve (risk R11). This closes it without hard-coding English into the shipped strings.
	//!
	//! \param[in] key Localization key, including its leading '#'
	//! \param[in] englishFallback Text to show when the key does not resolve
	//! \return Text to display
	protected string OvtTranslateOrEnglish(string key, string englishFallback)
	{
		string translated = WidgetManager.Translate(key);

		if (translated.IsEmpty())
			return englishFallback;

		if (translated == key)
			return englishFallback;

		return translated;
	}

	//------------------------------------------------------------------------------------------------
	//! OvtTranslateOrEnglish with one substitution.
	//! \param[in] key Localization key
	//! \param[in] englishFallback English format string using %1
	//! \param[in] param1 Substitution
	//! \return Text to display
	protected string OvtTranslateOrEnglish1(string key, string englishFallback, string param1)
	{
		string translated = WidgetManager.Translate(key, param1);

		if (translated.IsEmpty() || translated == key)
			return string.Format(englishFallback, param1);

		return translated;
	}

	//------------------------------------------------------------------------------------------------
	//! OvtTranslateOrEnglish with two substitutions.
	//! \param[in] key Localization key
	//! \param[in] englishFallback English format string using %1 and %2
	//! \param[in] param1 First substitution
	//! \param[in] param2 Second substitution
	//! \return Text to display
	protected string OvtTranslateOrEnglish2(string key, string englishFallback, string param1, string param2)
	{
		string translated = WidgetManager.Translate(key, param1, param2);

		if (translated.IsEmpty() || translated == key)
			return string.Format(englishFallback, param1, param2);

		return translated;
	}
}
