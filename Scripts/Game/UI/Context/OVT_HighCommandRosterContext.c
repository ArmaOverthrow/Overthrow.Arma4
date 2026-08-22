//------------------------------------------------------------------------------------------------
//! One row of the roster, in DISPLAY order: the record it shows and the widget that shows it.
//!
//! THE FLAT MODEL (OVT_RecruitEntryRef's reason, restated). Selection maps to THIS array, never to
//! a widget's position in its container, so gamepad focus order can never desync from what is on
//! screen. This roster has only one section, but it is the same rule for the same reason.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandRosterEntryRef
{
	//! The record this row shows. Read-only from the UI's point of view - the server owns it.
	OVT_HighCommandRecord m_Record;

	//! The row widget created for it, used for the selection highlight.
	Widget m_wWidget;

	void OVT_HighCommandRosterEntryRef(OVT_HighCommandRecord record, Widget widget)
	{
		m_Record = record;
		m_wWidget = widget;
	}
}

//------------------------------------------------------------------------------------------------
//! "Manage Groups" - every High Command group the local player owns, with a n/cap capacity line,
//! per-group status and Show on Map / Dismiss. The OVT_RecruitsContext flat-selection model, minus
//! the active/inactive split this roster has no equivalent of - every group here is always live.
//!
//! NOTHING HERE DELETES A GROUP. Dismiss only ever asks RpcAsk_DismissGroup; the row it was showing
//! disappears when the server's RpcDo_HCRemoved reaches m_OnHCGroupRemoved, which this screen is
//! already subscribed to.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandRosterContext : OVT_UIContext
{
	[Attribute("{6B1C3D0000005040}UI/Layouts/Menu/HighCommandRoster/HighCommandRosterRow.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one owned group row", params: "layout")]
	ResourceName m_RowLayout;

	protected OVT_HighCommandManagerComponent m_Manager;

	//! Resolved lazily in OnShow, guarded everywhere it is used - the controller can still be absent
	//! the first frame after possession (the OVT_HighCommandPurchaseContext precedent).
	protected OVT_HighCommandRequestComponent m_Requests;

	protected ref array<ref OVT_HighCommandRosterEntryRef> m_aEntries = new array<ref OVT_HighCommandRosterEntryRef>();

	protected OVT_HighCommandRecord m_SelectedGroup;
	protected Widget m_wSelectedWidget;
	protected int m_iSelectedIndex = -1;

	protected SCR_InputButtonComponent m_ShowOnMapAction;
	protected SCR_InputButtonComponent m_DismissAction;
	protected SCR_InputButtonComponent m_CloseAction;

	//! True between asking RpcAsk_DismissGroup and its answer - guards OnResult against a stray
	//! m_OnHCResult from anything else this shared controller component was asked (e.g. a map order).
	protected bool m_bDismissInFlight;
	protected string m_sDismissGroupId;

	//! Set by ShowOnMap() while the fullscreen map is still animating open; consumed the moment the
	//! map finishes building (SCR_MapEntity.GetOnMapOpenComplete). See ShowOnMap()'s header comment
	//! for why this subscription is NOT torn down in OnClose.
	protected string m_sPendingMapSelectGroupId;

	//------------------------------------------------------------------------------------------------
	override void PostInit()
	{
		if (!m_Layout)
			m_Layout = "{6B1C3D0000005000}UI/Layouts/Menu/HighCommandRoster.layout";

		m_Manager = OVT_Global.GetHighCommand();
	}

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		m_Requests = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();

		if (m_Manager)
		{
			m_Manager.m_OnHCGroupAdded.Insert(OnGroupTableChanged);
			m_Manager.m_OnHCGroupRemoved.Insert(OnGroupTableChanged);
			m_Manager.m_OnHCGroupUpdated.Insert(OnGroupUpdated);
		}

		if (m_Requests)
			m_Requests.m_OnHCResult.Insert(OnResult);

		Widget showOnMapButton = m_wRoot.FindAnyWidget("ShowOnMapButton");
		if (showOnMapButton)
		{
			m_ShowOnMapAction = SCR_InputButtonComponent.Cast(showOnMapButton.FindHandler(SCR_InputButtonComponent));
			if (m_ShowOnMapAction)
				m_ShowOnMapAction.m_OnActivated.Insert(ShowOnMap);
		}

		Widget dismissButton = m_wRoot.FindAnyWidget("DismissButton");
		if (dismissButton)
		{
			m_DismissAction = SCR_InputButtonComponent.Cast(dismissButton.FindHandler(SCR_InputButtonComponent));
			if (m_DismissAction)
				m_DismissAction.m_OnActivated.Insert(DismissSelectedGroup);
		}

		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if (closeButton)
		{
			m_CloseAction = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (m_CloseAction)
				m_CloseAction.m_OnActivated.Insert(CloseLayout);
		}

		m_bDismissInFlight = false;
		m_sDismissGroupId = "";

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted - three manager invokers, one request invoker, three
	//! button listeners. Widget pointers are dropped too: they die with the layout, and holding one
	//! past OnClose is how a screen ends up selecting a destroyed widget on its second open.
	//!
	//! DELIBERATELY DOES NOT touch m_sPendingMapSelectGroupId or SCR_MapEntity's invoker - see
	//! ShowOnMap()'s header comment. That subscription is created and CloseLayout() is called in the
	//! same call, which runs OnClose() synchronously; removing it here would cancel the very request
	//! ShowOnMap() just made.
	override void OnClose()
	{
		if (m_Manager)
		{
			m_Manager.m_OnHCGroupAdded.Remove(OnGroupTableChanged);
			m_Manager.m_OnHCGroupRemoved.Remove(OnGroupTableChanged);
			m_Manager.m_OnHCGroupUpdated.Remove(OnGroupUpdated);
		}

		if (m_Requests)
			m_Requests.m_OnHCResult.Remove(OnResult);

		if (m_ShowOnMapAction)
			m_ShowOnMapAction.m_OnActivated.Remove(ShowOnMap);

		if (m_DismissAction)
			m_DismissAction.m_OnActivated.Remove(DismissSelectedGroup);

		if (m_CloseAction)
			m_CloseAction.m_OnActivated.Remove(CloseLayout);

		m_Requests = null;
		m_ShowOnMapAction = null;
		m_DismissAction = null;
		m_CloseAction = null;

		m_aEntries.Clear();
		m_SelectedGroup = null;
		m_wSelectedWidget = null;
		m_iSelectedIndex = -1;
		m_bDismissInFlight = false;
		m_sDismissGroupId = "";
	}

	//------------------------------------------------------------------------------------------------
	//! d-pad / left stick move the selection. MenuSelect is deliberately unwired, the
	//! OVT_HighCommandPurchaseContext rule: a player moving through the list must not be able to
	//! fire a destructive action with the pad's confirm button.
	override void OnActiveFrame(float timeSlice)
	{
		super.OnActiveFrame(timeSlice);

		if (m_InputManager.GetActionTriggered("MenuUp"))
			SelectPreviousGroup();

		if (m_InputManager.GetActionTriggered("MenuDown"))
			SelectNextGroup();
	}

	//------------------------------------------------------------------------------------------------
	//! A group appeared or disappeared - rebuild the whole list.
	//! \param[in] record The record that changed. Unused, present to match the invoker's signature.
	protected void OnGroupTableChanged(OVT_HighCommandRecord record)
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A record changed in place - an order or a heartbeat. No rebuild: the widget SET did not
	//! change, only that one row's text and the selected panel, if it is the one showing.
	//! \param[in] record The record that changed.
	protected void OnGroupUpdated(OVT_HighCommandRecord record)
	{
		if (!record)
			return;

		RefreshEntryRow(record);

		if (m_SelectedGroup && record.m_sGroupId == m_SelectedGroup.m_sGroupId)
			UpdateDetails();
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the list from the owner's groups and the flat entry array every selection path reads.
	//! Keeps the selection across the rebuild by group id - the OVT_RecruitsContext.Refresh shape.
	protected void Refresh()
	{
		if (!m_wRoot)
			return;

		string previousSelectedId;
		if (m_SelectedGroup)
			previousSelectedId = m_SelectedGroup.m_sGroupId;

		Widget list = m_wRoot.FindAnyWidget("GroupsList");
		if (!list)
			return;

		ClearContainer(list);

		m_aEntries.Clear();
		m_SelectedGroup = null;
		m_wSelectedWidget = null;
		m_iSelectedIndex = -1;

		UpdateCapacityText();

		Widget helpText = m_wRoot.FindAnyWidget("NoGroupsHelp");
		Widget groupInfo = m_wRoot.FindAnyWidget("GroupInfo");

		array<ref OVT_HighCommandRecord> groups = new array<ref OVT_HighCommandRecord>();
		if (m_Manager)
			groups = m_Manager.GetGroupsByOwner(m_sPlayerID);

		if (groups.IsEmpty())
		{
			// No group at all to Show on Map or Dismiss - hidden, not disabled, is correct here: the
			// buttons live inside GroupInfo, so they go with it. BUG-102 is about hiding a control
			// that WOULD do something; there is nothing for either button to act on in this state.
			if (helpText)
				helpText.SetVisible(true);
			if (groupInfo)
				groupInfo.SetVisible(false);
			return;
		}

		if (helpText)
			helpText.SetVisible(false);
		if (groupInfo)
			groupInfo.SetVisible(true);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		foreach (OVT_HighCommandRecord record : groups)
		{
			if (!record)
				continue;

			Widget itemWidget = workspace.CreateWidgets(m_RowLayout, list);
			if (!itemWidget)
				continue;

			OVT_HighCommandRosterRowHandler handler = OVT_HighCommandRosterRowHandler.Cast(itemWidget.FindHandler(OVT_HighCommandRosterRowHandler));
			if (handler)
			{
				handler.Populate(record, m_aEntries.Count());
				handler.m_OnClicked.Insert(OnRowClicked);
			}

			m_aEntries.Insert(new OVT_HighCommandRosterEntryRef(record, itemWidget));
		}

		int index = IndexOfGroup(previousSelectedId);
		if (index < 0)
			index = 0;

		SelectGroupByIndex(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-populates one row in place, without touching the widget set or the selection - what the
	//! 10 s heartbeat and every order use, so neither churns the whole list.
	//! \param[in] record The group whose row to refresh.
	protected void RefreshEntryRow(notnull OVT_HighCommandRecord record)
	{
		foreach (int i, OVT_HighCommandRosterEntryRef entry : m_aEntries)
		{
			if (!entry || !entry.m_Record)
				continue;

			if (entry.m_Record.m_sGroupId != record.m_sGroupId)
				continue;

			OVT_HighCommandRosterRowHandler handler = OVT_HighCommandRosterRowHandler.Cast(entry.m_wWidget.FindHandler(OVT_HighCommandRosterRowHandler));
			if (handler)
				handler.Populate(record, i);

			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Remove every child of a list container.
	//! \param[in] container The container to empty.
	protected void ClearContainer(notnull Widget container)
	{
		Widget child = container.GetChildren();
		while (child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] groupId The group to look for. An empty id never matches.
	//! \return Its position in the flat display list, or -1.
	protected int IndexOfGroup(string groupId)
	{
		if (groupId.IsEmpty())
			return -1;

		foreach (int i, OVT_HighCommandRosterEntryRef entry : m_aEntries)
		{
			if (!entry || !entry.m_Record)
				continue;

			if (entry.m_Record.m_sGroupId == groupId)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Update the "n / cap" line. Every group the player owns counts, whatever it is doing.
	protected void UpdateCapacityText()
	{
		TextWidget capacityText = TextWidget.Cast(m_wRoot.FindAnyWidget("CapacityText"));
		if (!capacityText)
			return;

		if (!m_Manager)
			return;

		capacityText.SetTextFormat("#OVT-HC_Roster_Capacity", m_Manager.GetMemberCount(m_sPlayerID).ToString(), m_Manager.GetMemberCap().ToString());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRowClicked(SCR_ButtonBaseComponent button)
	{
		OVT_HighCommandRosterRowHandler handler = OVT_HighCommandRosterRowHandler.Cast(button);
		if (!handler)
			return;

		SelectGroupByIndex(handler.m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectPreviousGroup()
	{
		if (m_aEntries.IsEmpty())
			return;

		int index = m_iSelectedIndex - 1;
		if (index < 0)
			index = m_aEntries.Count() - 1;

		SelectGroupByIndex(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectNextGroup()
	{
		if (m_aEntries.IsEmpty())
			return;

		int index = m_iSelectedIndex + 1;
		if (index >= m_aEntries.Count())
			index = 0;

		SelectGroupByIndex(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Select the row at this position in the flat list. The widget comes from the entry, never from
	//! counting siblings (D16's reason, restated).
	//! \param[in] index Position in the flat list.
	protected void SelectGroupByIndex(int index)
	{
		if (index < 0 || index >= m_aEntries.Count())
			return;

		OVT_HighCommandRosterEntryRef entry = m_aEntries[index];
		if (!entry || !entry.m_Record)
			return;

		if (m_wSelectedWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0);
		}

		m_iSelectedIndex = index;
		m_SelectedGroup = entry.m_Record;
		m_wSelectedWidget = entry.m_wWidget;

		if (m_wSelectedWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0.3);
		}

		UpdateDetails();
	}

	//------------------------------------------------------------------------------------------------
	//! Redraw the details panel for the current selection. Both buttons stay visible and enabled
	//! whenever a group is selected - every group this screen lists is the player's own, so there is
	//! never a refusal to explain here.
	protected void UpdateDetails()
	{
		if (!m_SelectedGroup)
			return;

		OVT_HighCommandGroupEntry entry;
		if (m_Manager)
			entry = m_Manager.GetEntryByKey(m_SelectedGroup.m_sEntryKey);

		TextWidget nameText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedName"));
		if (nameText)
		{
			if (entry && entry.m_sTitle != "")
				nameText.SetText(entry.m_sTitle);
			else
				nameText.SetText(m_SelectedGroup.m_sEntryKey);
		}

		TextWidget membersText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedMembers"));
		if (membersText)
			membersText.SetTextFormat("#OVT-HC_Roster_MembersFormat", m_SelectedGroup.m_iAliveMembers.ToString(), m_SelectedGroup.m_iTotalMembers.ToString());

		TextWidget stanceText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStance"));
		if (stanceText)
			stanceText.SetText(OVT_HighCommandRosterRowHandler.StanceLabel(m_SelectedGroup.m_iStance));

		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStatus"));
		if (statusText)
		{
			statusText.SetText(OVT_HighCommandRosterRowHandler.StatusLabel(m_SelectedGroup));
			statusText.SetColor(OVT_HighCommandRosterRowHandler.StatusColor(m_SelectedGroup));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Raise the fullscreen map and select this group on it, through OVT_MapHighCommandLayer's own
	//! selection entry point (SelectGroup, promoted to public for exactly this call - not a second
	//! selection mechanism, per the plan).
	//!
	//! WHY THE SUBSCRIPTION OUTLIVES OnClose(). SCR_MapGadgetComponent.SetMapMode(true) is NOT
	//! synchronous - it carries its own raise-animation delay - so the map's layers do not exist the
	//! instant ShowMap() returns. The selection is deferred to SCR_MapEntity.GetOnMapOpenComplete(),
	//! fired once the map has fully built. CloseLayout() below runs OnClose() in the SAME call, so
	//! tearing this subscription down there would cancel the very request being made here. It is
	//! self-removing on the one answer it is waiting for instead (OnMapReadyForSelection), the
	//! OVT_MainMenuContext.Save()/OnSaveResult shape.
	protected void ShowOnMap()
	{
		if (!m_SelectedGroup)
			return;

		string groupId = m_SelectedGroup.m_sGroupId;

		OVT_MapContext mapContext = OVT_MapContext.Cast(m_UIManager.GetContext(OVT_MapContext));
		if (!mapContext || !mapContext.ShowMap())
		{
			ShowNotification("MustHaveMap");
			return;
		}

		m_sPendingMapSelectGroupId = groupId;
		SCR_MapEntity.GetOnMapOpenComplete().Insert(OnMapReadyForSelection);

		CloseLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! The fullscreen map finished opening - select the pending group on it.
	//! \param[in] config Unused, present to match SCR_MapEntity's invoker signature.
	protected void OnMapReadyForSelection(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapOpenComplete().Remove(OnMapReadyForSelection);

		if (m_sPendingMapSelectGroupId.IsEmpty())
			return;

		string groupId = m_sPendingMapSelectGroupId;
		m_sPendingMapSelectGroupId = "";

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		OVT_MapHighCommandLayer layer = OVT_MapHighCommandLayer.Cast(mapEntity.GetMapUIComponent(OVT_MapHighCommandLayer));
		if (layer)
			layer.SelectGroup(groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Confirm before asking the server to dismiss the selected group.
	protected void DismissSelectedGroup()
	{
		if (!m_SelectedGroup)
			return;

		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset("{26C9263913A8D1BD}Configs/UI/Dialogs/DialogPresets_Campaign.conf", "DISMISS_HC_GROUP");
		if (!dialog)
			return;

		dialog.m_OnConfirm.Insert(OnConfirmDismiss);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server - it deletes the group, its vehicle and its waypoints; nothing here does.
	//! m_OnHCGroupRemoved (already subscribed in OnShow) is what actually drops the row, on the
	//! server's word.
	protected void OnConfirmDismiss()
	{
		if (!m_SelectedGroup || !m_Requests)
			return;

		m_sDismissGroupId = m_SelectedGroup.m_sGroupId;
		m_bDismissInFlight = true;

		m_Requests.RequestDismissGroup(m_sDismissGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to a dismiss ask. Anything else this shared controller component was asked
	//! (a map order, most likely) is ignored by the in-flight guard.
	//! \param[in] resultCode An OVT_HighCommandRequestComponent.RESULT_* value.
	//! \param[in] charged Unused - dismissal never charges.
	//! \param[in] groupId The group the answer is about.
	protected void OnResult(int resultCode, int charged, string groupId)
	{
		if (!m_bDismissInFlight)
			return;

		if (groupId != m_sDismissGroupId)
			return;

		m_bDismissInFlight = false;

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_OK)
		{
			ShowHint("#OVT-HC_Roster_Dismissed");
			return;
		}

		ShowHint("#OVT-HC_Roster_DismissFailed");
	}
}
