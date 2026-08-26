//------------------------------------------------------------------------------------------------
//! One row of the roster, in DISPLAY order: the record it shows and the widget that shows it.
//!
//! WHY THIS EXISTS (decision D16). The screen used to find the widget for recruit N by walking the
//! Nth sibling of a single container. That works only while there is exactly one container holding
//! exactly the recruits and nothing else - which the Active/Inactive split, and the section headers
//! between them, both break. Selection (and therefore MenuUp/MenuDown, and therefore the whole
//! gamepad experience) must never depend on widget-tree order again, so Refresh() builds this flat
//! array once and every selection path indexes it.
//------------------------------------------------------------------------------------------------
class OVT_RecruitEntryRef
{
	//! The record this row shows. Read-only from the UI's point of view - the server owns it.
	OVT_RecruitData m_Recruit;

	//! The row widget created for it, used for the selection highlight.
	Widget m_wWidget;

	//! Which section it was built into, cached at build time so one Refresh() stays self-consistent.
	bool m_bInactive;

	void OVT_RecruitEntryRef(OVT_RecruitData recruit, Widget widget, bool inactive)
	{
		m_Recruit = recruit;
		m_wWidget = widget;
		m_bInactive = inactive;
	}
}

class OVT_RecruitsContext : OVT_UIContext
{
	[Attribute("{AA94A214B16CB2C8}UI/Layouts/Menu/RecruitsMenu/RecruitListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for recruit list items", params: "layout")]
	ResourceName m_RecruitItemLayout;

	//! One text row over each cluster the Inactive section is split into (T9.4). Bare TextWidgetClass
	//! root - no handler component needed, the caller sets its text directly.
	[Attribute("{6B1C3D0000007050}UI/Layouts/Menu/RecruitsMenu/RecruitGroupHeader.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one inactive cluster's sub-header", params: "layout")]
	ResourceName m_GroupHeaderLayout;

	protected OVT_RecruitManagerComponent m_RecruitManager;
	protected ref array<ref OVT_RecruitData> m_aPlayerRecruits;
	//! The flat, ordered display list - actives then inactives. THE selection model (D16).
	protected ref array<ref OVT_RecruitEntryRef> m_aEntries = new array<ref OVT_RecruitEntryRef>();
	//! Debounce for the toggle: the button's SCR_InputButtonComponent fires on its own bound action
	//! AND OnActiveFrame polls the same action (the pattern the other three buttons here use), so one
	//! press can arrive twice. The duplicate is a no-op the server refuses, but it should not be sent.
	protected float m_fLastToggleTime = 0;
	protected const float TOGGLE_DEBOUNCE_MS = 250;
	protected OVT_RecruitData m_SelectedRecruit;
	protected Widget m_wSelectedWidget;
	protected int m_iSelectedIndex = -1;
	protected OVT_RecruitData m_CurrentRenamingRecruit;
	protected SCR_ConfigurableDialogUi m_RenameDialog;

	//! Resolved lazily in OnShow, guarded everywhere it is used - the controller can still be absent
	//! the first frame after possession (the OVT_HighCommandRosterContext precedent).
	protected OVT_HighCommandRequestComponent m_HCRequests;
	protected SCR_InputButtonComponent m_ConvertAction;

	//! True between asking RpcAsk_ConvertRecruitGroup and its answer. RpcDo_HCResult carries the NEW
	//! group id on success and "" on every refusal, so - unlike the dismiss guard this is modelled on -
	//! the in-flight guard is keyed on the ANCHOR RECRUIT ID that was sent, not on groupId.
	protected bool m_bConvertInFlight;
	protected string m_sConvertAnchorId;

	override void PostInit()
	{		
		m_RecruitManager = OVT_RecruitManagerComponent.GetInstance();
	}
	
	override void OnShow()
	{			
		// Set up event listeners for recruit changes
		if (m_RecruitManager)
		{
			m_RecruitManager.m_OnRecruitRemoved.Insert(OnRecruitRemoved);
			m_RecruitManager.m_OnRecruitAdded.Insert(OnRecruitAdded);
			// The server's confirmation that a park/unpark actually happened. The screen never mutates
			// the replica itself, so this is the ONLY thing that moves a row between the two sections.
			m_RecruitManager.m_OnRecruitActiveStateChanged.Insert(OnRecruitActiveStateChanged);
		}
		
		// Set up button click handlers
		Widget showOnMapButton = m_wRoot.FindAnyWidget("ShowOnMapButton");
		if (showOnMapButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(showOnMapButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ShowOnMap);
		}
		
		Widget renameButton = m_wRoot.FindAnyWidget("RenameButton");
		if (renameButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(renameButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(RenameRecruit);
		}
		
		Widget dismissButton = m_wRoot.FindAnyWidget("DismissButton");
		if (dismissButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(dismissButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(DismissRecruit);
		}

		Widget toggleActiveButton = m_wRoot.FindAnyWidget("ToggleActiveButton");
		if (toggleActiveButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(toggleActiveButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ToggleActiveState);
		}

		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if (closeButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(CloseLayout);
		}

		// High Command conversion (T9.5). Resolved lazily like every other controller-side seam in
		// this feature - the controller can still be absent the first frame after possession.
		m_HCRequests = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();
		if (m_HCRequests)
			m_HCRequests.m_OnHCResult.Insert(OnHCResult);

		Widget convertButton = m_wRoot.FindAnyWidget("ConvertButton");
		if (convertButton)
		{
			m_ConvertAction = SCR_InputButtonComponent.Cast(convertButton.FindHandler(SCR_InputButtonComponent));
			if (m_ConvertAction)
				m_ConvertAction.m_OnActivated.Insert(ConvertSelectedGroup);
		}

		m_bConvertInFlight = false;
		m_sConvertAnchorId = "";

		Refresh();
	}

	override void OnClose()
	{
		// Clean up event listeners
		if (m_RecruitManager)
		{
			m_RecruitManager.m_OnRecruitRemoved.Remove(OnRecruitRemoved);
			m_RecruitManager.m_OnRecruitAdded.Remove(OnRecruitAdded);
			m_RecruitManager.m_OnRecruitActiveStateChanged.Remove(OnRecruitActiveStateChanged);
		}

		if (m_HCRequests)
			m_HCRequests.m_OnHCResult.Remove(OnHCResult);

		if (m_ConvertAction)
			m_ConvertAction.m_OnActivated.Remove(ConvertSelectedGroup);

		m_HCRequests = null;
		m_ConvertAction = null;
		m_bConvertInFlight = false;
		m_sConvertAnchorId = "";

		// The rows are gone with the layout; holding widget pointers past OnClose is how a screen ends
		// up selecting a destroyed widget on its second open.
		m_aEntries.Clear();
		m_SelectedRecruit = null;
		m_wSelectedWidget = null;
		m_iSelectedIndex = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The server confirmed a park/unpark. Rebuild so the row moves to the other section.
	//! \param[in] recruit The record that changed. Unused - the whole roster is re-read.
	//! \param[in] inactive Its new state. Unused, same reason.
	protected void OnRecruitActiveStateChanged(OVT_RecruitData recruit, bool inactive)
	{
		Refresh();
	}
	
	//! Event handler for when recruits are removed
	protected void OnRecruitRemoved(OVT_RecruitData recruit)
	{
		// Refresh UI when a recruit is removed
		Refresh();
	}
	
	//! Event handler for when recruits are added
	protected void OnRecruitAdded(OVT_RecruitData recruit)
	{
		// Refresh UI when a recruit is added
		Refresh();
	}
	
	override void OnActiveFrame(float timeSlice)
	{
		super.OnActiveFrame(timeSlice);
		
		// Handle recruit list navigation
		if (m_InputManager.GetActionTriggered("MenuUp"))
		{
			SelectPreviousRecruit();
		}
		
		if (m_InputManager.GetActionTriggered("MenuDown"))
		{
			SelectNextRecruit();
		}
		
		if (m_InputManager.GetActionTriggered("MenuSelect"))
		{
			// Select current recruit (already handled by selection)
		}
		
		// Handle input actions
		if (m_InputManager.GetActionTriggered("OverthrowRecruitsShowOnMap"))
		{
			ShowOnMap();
		}
		
		if (m_InputManager.GetActionTriggered("OverthrowRecruitsRename"))
		{
			RenameRecruit();
		}
		
		if (m_InputManager.GetActionTriggered("OverthrowRecruitsDismiss"))
		{
			DismissRecruit();
		}

		if (m_InputManager.GetActionTriggered("OverthrowRecruitsToggleActive"))
		{
			ToggleActiveState();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Rebuild both sections and the flat entry array that drives every selection path.
	//!
	//! Called on open and on every roster change, including the server's park/unpark confirmation, so
	//! it has to be cheap to run repeatedly and it has to KEEP THE SELECTION: a player who parks the
	//! recruit they were looking at should still be looking at it afterwards, in the other section.
	protected void Refresh()
	{
		if (!m_RecruitManager)
			return;

		// Remember who was selected before the widgets go away.
		string previousSelectedId;
		if (m_SelectedRecruit)
			previousSelectedId = m_SelectedRecruit.m_sRecruitId;

		m_aPlayerRecruits = m_RecruitManager.GetPlayerRecruits(m_sPlayerID);

		Widget activeList = m_wRoot.FindAnyWidget("ActiveList");
		Widget inactiveList = m_wRoot.FindAnyWidget("InactiveList");
		if (!activeList || !inactiveList)
			return;

		ClearContainer(activeList);
		ClearContainer(inactiveList);

		m_aEntries.Clear();
		m_SelectedRecruit = null;
		m_wSelectedWidget = null;
		m_iSelectedIndex = -1;

		UpdateCapacityText();

		// Actives first, then inactives - this order IS the navigation order.
		BuildSection(m_RecruitManager.GetPlayerRecruitsByState(m_sPlayerID, false), activeList, "ActiveHeader", false);
		BuildSection(m_RecruitManager.GetPlayerRecruitsByState(m_sPlayerID, true), inactiveList, "InactiveHeader", true);

		Widget helpText = m_wRoot.FindAnyWidget("NoRecruitsHelp");
		Widget recruitInfo = m_wRoot.FindAnyWidget("RecruitInfo");

		if (m_aEntries.IsEmpty())
		{
			if (helpText)
				helpText.SetVisible(true);
			if (recruitInfo)
				recruitInfo.SetVisible(false);

			SetButtonsVisible(false);
			return;
		}

		if (helpText)
			helpText.SetVisible(false);
		if (recruitInfo)
			recruitInfo.SetVisible(true);

		int index = IndexOfRecruit(previousSelectedId);
		if (index < 0)
			index = 0;

		SelectRecruitByIndex(index);
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
	//! Build one section's rows and append them to the flat entry array.
	//!
	//! An EMPTY SECTION HIDES ITS HEADER AND ITS CONTAINER rather than showing a heading with nothing
	//! under it - the same "no control beats a dead control" rule the map layer panel uses.
	//!
	//! THE INACTIVE SECTION IS SPLIT BY GROUP (T9.4): each cluster BuildInactiveClusters finds gets its
	//! own sub-header before its rows. The active section is unchanged - one flat run, no clusters,
	//! because an active recruit is in its owner's slave group and has no inactive-group host to split
	//! by in the first place.
	//! \param[in] recruits The records for this section, in table order.
	//! \param[in] container The list container to build into.
	//! \param[in] headerName The name of this section's heading widget.
	//! \param[in] inactive Which section this is - passed to the row so it can style itself.
	protected void BuildSection(array<ref OVT_RecruitData> recruits, notnull Widget container, string headerName, bool inactive)
	{
		bool hasAny = false;
		if (recruits && !recruits.IsEmpty())
			hasAny = true;

		Widget header = m_wRoot.FindAnyWidget(headerName);
		if (header)
			header.SetVisible(hasAny);

		container.SetVisible(hasAny);

		if (!hasAny)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		// Null on a client whose controller has not been assigned yet, and on a dedicated server.
		// A missing cache reads as "no status", which every row renders as an empty icon slot.
		OVT_RecruitCommandComponent commands = OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get();

		if (!inactive)
		{
			foreach (OVT_RecruitData recruit : recruits)
			{
				BuildRecruitRow(recruit, container, workspace, commands, false);
			}

			return;
		}

		array<ref array<ref OVT_RecruitData>> clusters = BuildInactiveClusters(recruits);

		foreach (array<ref OVT_RecruitData> cluster : clusters)
		{
			if (!cluster || cluster.IsEmpty())
				continue;

			Widget groupHeaderWidget = workspace.CreateWidgets(m_GroupHeaderLayout, container);
			TextWidget groupHeaderText = TextWidget.Cast(groupHeaderWidget);
			if (groupHeaderText)
				groupHeaderText.SetTextFormat("#OVT-Recruits_GroupHeader", cluster.Count());

			foreach (OVT_RecruitData recruit : cluster)
			{
				BuildRecruitRow(recruit, container, workspace, commands, true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Build one recruit's row and append it to the flat entry array (D16). The active loop and every
	//! inactive cluster both call this, so there is exactly one place that writes m_aEntries.
	//! \param[in] recruit The record to draw. A null entry is skipped, same as the old inline loop.
	//! \param[in] container The list container to build into.
	//! \param[in] workspace The workspace to create the row widget with.
	//! \param[in] commands The client's status cache, or null when it has not resolved yet.
	//! \param[in] inactive Which section this row belongs to.
	protected void BuildRecruitRow(OVT_RecruitData recruit, notnull Widget container, notnull WorkspaceWidget workspace, OVT_RecruitCommandComponent commands, bool inactive)
	{
		if (!recruit)
			return;

		Widget itemWidget = workspace.CreateWidgets(m_RecruitItemLayout, container);
		if (!itemWidget)
			return;

		int statusFlags = 0;
		if (commands)
			statusFlags = commands.GetStatusFlags(recruit.m_sRecruitId);

		OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(itemWidget.FindHandler(OVT_RecruitListEntryHandler));
		if (handler)
		{
			// The row's index is its position in the FLAT list, not in this section.
			handler.Populate(recruit, m_aEntries.Count(), inactive, statusFlags);
			handler.m_OnClicked.Insert(OnRecruitItemClicked);
		}

		m_aEntries.Insert(new OVT_RecruitEntryRef(recruit, itemWidget, inactive));
	}

	//------------------------------------------------------------------------------------------------
	//! Approximate which parked recruits share a real in-world AI group, for display only (T9.4).
	//!
	//! THIS IS AN APPROXIMATION, DELIBERATELY. A client cannot read which live group a body actually
	//! belongs to (that is server-only state), so this clusters purely by the records' own
	//! m_vLastKnownPosition through OVT_RecruitInactiveGrouping.SelectClusterCandidates - the same pure
	//! rule the recruit manager itself uses to pick a host when parking a NEW recruit. It can disagree
	//! with the server's real grouping (implementation.md Phase 9's warning), which is exactly why
	//! Convert always sends the SELECTED recruit's own id and lets the server resolve the real group,
	//! and why the confirm dialog says "this group" rather than promising a count.
	//! \param[in] inactiveRecruits Every parked recruit this owner holds, in table order.
	//! \return Clusters in first-seen order. Never null; every recruit appears in exactly one cluster.
	protected array<ref array<ref OVT_RecruitData>> BuildInactiveClusters(array<ref OVT_RecruitData> inactiveRecruits)
	{
		array<ref array<ref OVT_RecruitData>> clusters = new array<ref array<ref OVT_RecruitData>>();
		if (!inactiveRecruits)
			return clusters;

		map<string, bool> assigned = new map<string, bool>();

		foreach (OVT_RecruitData recruit : inactiveRecruits)
		{
			if (!recruit)
				continue;

			if (assigned.Contains(recruit.m_sRecruitId))
				continue;

			array<ref OVT_RecruitData> cluster = new array<ref OVT_RecruitData>();
			cluster.Insert(recruit);
			assigned.Set(recruit.m_sRecruitId, true);

			array<string> candidateIds = OVT_RecruitInactiveGrouping.SelectClusterCandidates(inactiveRecruits, recruit.m_sRecruitId, recruit.m_vLastKnownPosition, OVT_RecruitInactiveGrouping.DEFAULT_CLUSTER_RADIUS);

			foreach (string candidateId : candidateIds)
			{
				if (assigned.Contains(candidateId))
					continue;

				OVT_RecruitData candidate = FindRecruitById(inactiveRecruits, candidateId);
				if (!candidate)
					continue;

				cluster.Insert(candidate);
				assigned.Set(candidateId, true);
			}

			clusters.Insert(cluster);
		}

		return clusters;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] recruits The table to search.
	//! \param[in] recruitId The id to find.
	//! \return The matching record, or null when it is not on the table.
	protected OVT_RecruitData FindRecruitById(array<ref OVT_RecruitData> recruits, string recruitId)
	{
		foreach (OVT_RecruitData recruit : recruits)
		{
			if (!recruit)
				continue;

			if (recruit.m_sRecruitId == recruitId)
				return recruit;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Find a recruit's position in the flat display list.
	//! \param[in] recruitId The recruit to look for. An empty id never matches.
	//! \return The index, or -1 when it is not on screen.
	protected int IndexOfRecruit(string recruitId)
	{
		if (recruitId.IsEmpty())
			return -1;

		foreach (int i, OVT_RecruitEntryRef entry : m_aEntries)
		{
			if (!entry || !entry.m_Recruit)
				continue;

			if (entry.m_Recruit.m_sRecruitId == recruitId)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Update the "X / 16" capacity line. Parked recruits count too - that is the point of showing it.
	protected void UpdateCapacityText()
	{
		TextWidget capacityText = TextWidget.Cast(m_wRoot.FindAnyWidget("CapacityText"));
		if (!capacityText)
			return;

		if (!m_RecruitManager)
			return;

		capacityText.SetTextFormat("#OVT-Recruits_Capacity", m_RecruitManager.GetRecruitCount(m_sPlayerID), OVT_RecruitManagerComponent.MAX_RECRUITS_PER_PLAYER);
	}
	
	protected void OnRecruitItemClicked(SCR_ButtonBaseComponent button)
	{
		OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(button);
		if (!handler)
			return;
			
		SelectRecruitByIndex(handler.GetIndex());
	}
	
	void SelectRecruit(OVT_RecruitData recruit, Widget widget)
	{
		// Update selection highlight
		if (m_wSelectedWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0);
		}
		
		m_SelectedRecruit = recruit;
		m_wSelectedWidget = widget;
		
		if (widget)
		{
			ImageWidget bg = ImageWidget.Cast(widget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0.3);
		}
		
		// Update recruit details
		UpdateRecruitDetails();
	}
	
	protected void UpdateRecruitDetails()
	{
		if (!m_SelectedRecruit)
		{
			SetButtonsVisible(false);

			TextWidget reasonText = TextWidget.Cast(m_wRoot.FindAnyWidget("ConvertReasonText"));
			if (reasonText)
				reasonText.SetVisible(false);

			return;
		}
		
		SetButtonsVisible(true);
		
		// Update name
		TextWidget nameText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedName"));
		if (nameText)
			nameText.SetText(m_SelectedRecruit.GetName());
		
		// Update level and XP
		TextWidget levelText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedLevel"));
		if (levelText)
		{
			int level = m_SelectedRecruit.GetLevel();
			int currentXP = m_SelectedRecruit.m_iXP;
			int nextLevelXP = m_SelectedRecruit.GetNextLevelXP();
			levelText.SetTextFormat("#OVT-Recruit_LevelXP", level, currentXP, nextLevelXP);
		}
		
		// Update kills
		TextWidget killsText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedKills"));
		if (killsText)
			killsText.SetTextFormat("#OVT-Recruit_Kills", m_SelectedRecruit.m_iKills);
		
		// Update hometown
		TextWidget hometownText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedHometown"));
		if (hometownText)
			hometownText.SetTextFormat("#OVT-Recruit_Hometown", m_SelectedRecruit.GetHometown());
		
		// Update status
		IEntity recruitEntity = m_RecruitManager.GetRecruitEntity(m_SelectedRecruit.m_sRecruitId);
		bool inactive = m_RecruitManager.IsRecruitInactive(m_SelectedRecruit.m_sRecruitId);

		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStatus"));
		if (statusText)
		{
			if (recruitEntity)
			{
				float distance = vector.Distance(m_Owner.GetOrigin(), recruitEntity.GetOrigin());
				if (inactive)
				{
					statusText.SetTextFormat("#OVT-Recruits_StatusHolding", Math.Round(distance));
					statusText.SetColor(Color.Orange);
				}
				else
				{
					statusText.SetTextFormat("#OVT-Recruit_StatusActive", Math.Round(distance));
					statusText.SetColor(Color.Green);
				}
				SetButtonEnabled("ShowOnMapButton", true);
			}
			else
			{
				statusText.SetText("#OVT-Recruit_StatusOffline");
				statusText.SetColor(Color.Gray);

				// Disable action buttons for offline recruits
				SetButtonEnabled("ShowOnMapButton", false);
			}

		}

		UpdateToggleButton(recruitEntity != null, inactive);
		UpdateConvertButton(inactive, recruitEntity != null);
	}

	//------------------------------------------------------------------------------------------------
	//! Point the toggle button at what the selected recruit's state makes possible.
	//!
	//! DISABLED, NOT HIDDEN, when there is no body: the recruit is still on the roster and the control
	//! is still the right one, it just cannot act right now - the same rule the held actions on the
	//! body follow. The label always names the RESULT of pressing it, never the current state.
	//! \param[in] hasBody Whether the recruit has a body in the world.
	//! \param[in] inactive Whether it is currently parked.
	protected void UpdateToggleButton(bool hasBody, bool inactive)
	{
		Widget button = m_wRoot.FindAnyWidget("ToggleActiveButton");
		if (!button)
			return;

		SCR_InputButtonComponent comp = SCR_InputButtonComponent.Cast(button.FindHandler(SCR_InputButtonComponent));
		if (!comp)
			return;

		if (inactive)
			comp.SetLabel("#OVT-Recruits_ToggleFollow");
		else
			comp.SetLabel("#OVT-Recruits_ToggleHold");

		comp.SetEnabled(hasBody);
	}

	//------------------------------------------------------------------------------------------------
	//! Point the Convert button at whether the current selection could actually be converted (T9.5).
	//!
	//! DISABLED, NOT HIDDEN (BUG-102), for the two blockers this screen CAN check locally: an active
	//! recruit is not parked at all, and a parked recruit with no live body has no group for the server
	//! to find. The HC member cap is deliberately NOT checked here - it cannot be pre-checked precisely
	//! client-side (implementation.md Phase 9), so the button stays enabled and a RESULT_CONVERT_AT_CAP
	//! answer is what tells the player.
	//! \param[in] inactive Whether the selected recruit is parked.
	//! \param[in] hasBody Whether it has a live body in the world.
	protected void UpdateConvertButton(bool inactive, bool hasBody)
	{
		Widget button = m_wRoot.FindAnyWidget("ConvertButton");
		if (!button)
			return;

		SCR_InputButtonComponent comp = SCR_InputButtonComponent.Cast(button.FindHandler(SCR_InputButtonComponent));
		if (!comp)
			return;

		TextWidget reasonText = TextWidget.Cast(m_wRoot.FindAnyWidget("ConvertReasonText"));

		if (!inactive)
		{
			comp.SetEnabled(false);

			if (reasonText)
			{
				reasonText.SetText("#OVT-Recruits_ConvertNeedsInactive");
				reasonText.SetVisible(true);
			}

			return;
		}

		if (!hasBody)
		{
			comp.SetEnabled(false);

			if (reasonText)
			{
				reasonText.SetText("#OVT-Recruits_ConvertNeedsBody");
				reasonText.SetVisible(true);
			}

			return;
		}

		comp.SetEnabled(true);

		if (reasonText)
			reasonText.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Confirm before asking the server to convert the selected recruit's parked group (T9.5). ONE WAY:
	//! nothing here or anywhere else turns a High Command group back into recruits.
	protected void ConvertSelectedGroup()
	{
		if (!m_SelectedRecruit || !m_RecruitManager)
			return;

		if (m_bConvertInFlight)
			return;

		if (!m_RecruitManager.IsRecruitInactive(m_SelectedRecruit.m_sRecruitId))
			return;

		if (!m_RecruitManager.GetRecruitEntity(m_SelectedRecruit.m_sRecruitId))
			return;

		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset("{26C9263913A8D1BD}Configs/UI/Dialogs/DialogPresets_Campaign.conf", "CONVERT_RECRUIT_GROUP");
		if (!dialog)
			return;

		dialog.m_OnConfirm.Insert(OnConfirmConvert);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server - it resolves the recruit's real inactive group and converts every member in it,
	//! not just the one selected here. Nothing client-side removes a recruit or creates a group; that
	//! happens on m_OnRecruitRemoved / m_OnHCGroupAdded, which this screen and Manage Groups already
	//! subscribe to independently.
	protected void OnConfirmConvert()
	{
		if (!m_SelectedRecruit || !m_HCRequests)
		{
			ShowHint("#OVT-Recruits_ConvertFailed");
			return;
		}

		m_sConvertAnchorId = m_SelectedRecruit.m_sRecruitId;
		m_bConvertInFlight = true;

		m_HCRequests.RequestConvertRecruitGroup(m_sConvertAnchorId);
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to a convert ask, whatever it says. m_OnHCResult is shared by every High
	//! Command request this controller sends, but only one is ever in flight from THIS screen at a
	//! time, so the in-flight flag alone is the correlation - RpcDo_HCResult never echoes the anchor id.
	//! \param[in] resultCode An OVT_HighCommandRequestComponent.RESULT_* value.
	//! \param[in] charged Unused - conversion never charges.
	//! \param[in] groupId The NEW group's id on success, or "" on every refusal.
	protected void OnHCResult(int resultCode, int charged, string groupId)
	{
		if (!m_bConvertInFlight)
			return;

		m_bConvertInFlight = false;
		m_sConvertAnchorId = "";

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_OK)
		{
			ShowHint("#OVT-Recruits_ConvertSuccess");
			return;
		}

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_CONVERT_AT_CAP)
		{
			ShowHint("#OVT-Recruits_ConvertAtCap");
			return;
		}

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_NO_RECRUIT_GROUP)
		{
			ShowHint("#OVT-Recruits_ConvertNoGroup");
			return;
		}

		ShowHint("#OVT-Recruits_ConvertFailed");
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to park the selected recruit, or to bring it back into the squad.
	//!
	//! ASKS AND NOTHING ELSE. The local replica is NOT touched here: the authoritative transition can
	//! fail in the world (no body, sitting in a vehicle) and the broadcast is the only thing that says
	//! it happened. m_OnRecruitActiveStateChanged is what redraws the roster.
	protected void ToggleActiveState()
	{
		if (!m_SelectedRecruit || !m_RecruitManager)
			return;

		// One press can reach here twice - see m_fLastToggleTime.
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			float now = world.GetWorldTime();
			if (m_fLastToggleTime > 0 && now - m_fLastToggleTime < TOGGLE_DEBOUNCE_MS)
				return;

			m_fLastToggleTime = now;
		}

		IEntity recruitEntity = m_RecruitManager.GetRecruitEntity(m_SelectedRecruit.m_sRecruitId);
		if (!recruitEntity)
		{
			ShowHint("#OVT-Recruit_NoBody");
			return;
		}

		OVT_RecruitCommandComponent commands = OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get();
		if (!commands)
		{
			// Null until the controller has been assigned to this client; nothing to do but say so.
			ShowHint("#OVT-Recruit_CommandFailed");
			return;
		}

		bool inactive = m_RecruitManager.IsRecruitInactive(m_SelectedRecruit.m_sRecruitId);
		commands.RequestSetInactive(m_SelectedRecruit.m_sRecruitId, !inactive);
	}
	
	//------------------------------------------------------------------------------------------------
	//! MenuUp: one step back through the flat list, wrapping. Crosses the section boundary with no
	//! special case, which is exactly why the flat list exists.
	protected void SelectPreviousRecruit()
	{
		if (m_aEntries.IsEmpty())
			return;

		int newIndex = m_iSelectedIndex - 1;
		if (newIndex < 0)
			newIndex = m_aEntries.Count() - 1;

		SelectRecruitByIndex(newIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! MenuDown: one step forward through the flat list, wrapping.
	protected void SelectNextRecruit()
	{
		if (m_aEntries.IsEmpty())
			return;

		int newIndex = m_iSelectedIndex + 1;
		if (newIndex >= m_aEntries.Count())
			newIndex = 0;

		SelectRecruitByIndex(newIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Select the row at this position in the flat display list.
	//!
	//! The widget comes from the entry, never from counting siblings - two containers and two headings
	//! make sibling position meaningless (D16).
	//! \param[in] index Position in the flat list.
	protected void SelectRecruitByIndex(int index)
	{
		if (index < 0 || index >= m_aEntries.Count())
			return;

		OVT_RecruitEntryRef entry = m_aEntries[index];
		if (!entry || !entry.m_Recruit)
			return;

		m_iSelectedIndex = index;
		SelectRecruit(entry.m_Recruit, entry.m_wWidget);
	}

	protected void SetButtonsVisible(bool visible)
	{
		array<string> buttonNames = {
			"DismissButton",
			"RenameButton",
			"ShowOnMapButton",
			"ToggleActiveButton",
			"ConvertButton"
		};
		
		foreach (string name : buttonNames)
		{
			Widget button = m_wRoot.FindAnyWidget(name);
			if (button)
				button.SetVisible(visible);
		}
	}
	
	protected void SetButtonEnabled(string buttonName, bool enabled)
	{
		Widget button = m_wRoot.FindAnyWidget(buttonName);
		if (button)
		{
			SCR_InputButtonComponent comp = SCR_InputButtonComponent.Cast(button.FindHandler(SCR_InputButtonComponent));
			if (comp)
				comp.SetEnabled(enabled);
		}
	}
	
	protected void DismissRecruit()
	{
		if (!m_SelectedRecruit)
			return;
			
		// Show confirmation dialog
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset("{26C9263913A8D1BD}Configs/UI/Dialogs/DialogPresets_Campaign.conf", "DISMISS_RECRUIT");
		if (!dialog)
			return;
			
		dialog.m_OnConfirm.Insert(OnConfirmDismiss);
	}
	
	protected void OnConfirmDismiss()
	{
		if (!m_SelectedRecruit)
			return;
			
		// Ask the server - it validates that this recruit is ours before deleting anything
		OVT_RecruitRequestComponent recruitRequests = OVT_ControllerComponent<OVT_RecruitRequestComponent>.Get();
		if (recruitRequests)
		{
			recruitRequests.DismissRecruit(m_SelectedRecruit.m_sRecruitId);
		}
		
		// Clear UI selection (the actual recruit removal will be handled when server broadcasts)
		m_SelectedRecruit = null;
		m_wSelectedWidget = null;
		
		ShowHint("#OVT-Recruit_Dismissed");
	}
	
	protected void RenameRecruit()
	{
		if (!m_SelectedRecruit)
			return;
		
		// Create configurable dialog with EditBox support
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset("{272B6C4030554E27}Configs/UI/Dialogs/DialogPresets_Campaign.conf", "RENAME_RECRUIT");
		if (!dialog)
		{
			ShowHint("Could not open rename dialog");
			return;
		}
		
		// Try to access EditBox directly through widget finding since cast failed
		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", dialog.GetRootWidget());
		if (editBox)
		{
			// Set current name as initial text
			editBox.SetValue(m_SelectedRecruit.GetName());
		}
		
		// Store current recruit and dialog for the callback
		m_CurrentRenamingRecruit = m_SelectedRecruit;
		m_RenameDialog = dialog;
		
		dialog.m_OnConfirm.Insert(OnRenameConfirmed);
		dialog.m_OnCancel.Insert(OnRenameCancel);
	}
	
	protected void OnRenameConfirmed()
	{
		if (!m_CurrentRenamingRecruit || !m_RenameDialog)
			return;
		
		// Get the EditBox component directly via widget finding
		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", m_RenameDialog.GetRootWidget());
		if (!editBox)
		{
			m_CurrentRenamingRecruit = null;
			m_RenameDialog = null;
			return;
		}
		
		string newName = editBox.GetValue();

		// Validate locally for immediate feedback; the server re-validates
		if (newName.IsEmpty() || newName.Length() > 32)
		{
			ShowHint("Invalid name length (1-32 characters)");
			m_CurrentRenamingRecruit = null;
			m_RenameDialog = null;
			return;
		}

		// Ask the server - it validates ownership, renames the authoritative record and broadcasts
		OVT_RecruitRequestComponent recruitRequests = OVT_ControllerComponent<OVT_RecruitRequestComponent>.Get();
		if (!recruitRequests)
		{
			m_CurrentRenamingRecruit = null;
			m_RenameDialog = null;
			return;
		}

		recruitRequests.RenameRecruit(m_CurrentRenamingRecruit.m_sRecruitId, newName);

		// Apply to the local replica too so the UI updates immediately (the broadcast confirms it)
		m_RecruitManager.RenameRecruit(m_CurrentRenamingRecruit.m_sRecruitId, newName);

		UpdateRecruitDetails();
		Refresh();
		ShowHint("#OVT-Recruit_Renamed");

		m_CurrentRenamingRecruit = null;
		m_RenameDialog = null;
	}
	
	protected void OnRenameCancel()
	{
		m_CurrentRenamingRecruit = null;
		m_RenameDialog = null;
	}
	
	protected void ShowOnMap()
	{
		if (!m_SelectedRecruit)
			return;
			
		IEntity recruitEntity = m_RecruitManager.GetRecruitEntity(m_SelectedRecruit.m_sRecruitId);
		if (!recruitEntity)
			return;
			
		// Add map marker
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerManager)
		{
			// Create a waypoint at recruit location
			vector pos = recruitEntity.GetOrigin();
			OVT_JobManagerComponent jobManager = OVT_Global.GetJobs();
			if (jobManager)
			{
				// Recruit waypoints have their own slot: writing m_vCurrentWaypoint here used to clobber
				// the job waypoint the Jobs menu had set (implementation.md G2).
				jobManager.m_vRecruitWaypoint = pos;
			}
		}
		
		CloseLayout();
		ShowHint("#OVT-Recruit_MarkedOnMap");
	}
	
}