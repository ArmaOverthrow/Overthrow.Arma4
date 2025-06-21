class OVT_RecruitsContext : OVT_UIContext
{
	[Attribute("{5D5C558A6E391690}UI/Layouts/Menu/RecruitsMenu/RecruitListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for recruit list items", params: "layout")]
	ResourceName m_RecruitItemLayout;
	
	protected OVT_RecruitManagerComponent m_RecruitManager;
	protected ref array<ref OVT_RecruitData> m_aPlayerRecruits;
	protected OVT_RecruitData m_SelectedRecruit;
	protected Widget m_wSelectedWidget;
	protected int m_iSelectedIndex = -1;
	protected OVT_RecruitData m_CurrentRenamingRecruit;
	protected SCR_ConfigurableDialogUi m_RenameDialog;
	
	override void PostInit()
	{		
		m_RecruitManager = OVT_RecruitManagerComponent.GetInstance();
	}
	
	override void OnShow()
	{			
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
		
		Refresh();
	}
	
	override void OnFrame(float timeSlice)
	{
		super.OnFrame(timeSlice);
		
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
	}
	
	protected void Refresh()
	{
		// Get player recruits
		m_aPlayerRecruits = m_RecruitManager.GetPlayerRecruits(m_sPlayerID);
		
		// Clear the list
		Widget container = m_wRoot.FindAnyWidget("RecruitsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		
		// Update help text visibility
		Widget helpText = m_wRoot.FindAnyWidget("NoRecruitsHelp");
		Widget recruitInfo = m_wRoot.FindAnyWidget("RecruitInfo");
		
		if (m_aPlayerRecruits.IsEmpty())
		{
			// Show help text when no recruits
			if (helpText)
				helpText.SetVisible(true);
			if (recruitInfo)
				recruitInfo.SetVisible(false);
				
			// Hide action buttons
			SetButtonsVisible(false);
			
			m_SelectedRecruit = null;
			return;
		}
		else
		{
			// Hide help text when has recruits
			if (helpText)
				helpText.SetVisible(false);
			if (recruitInfo)
				recruitInfo.SetVisible(true);
		}
		
		// Populate recruit list
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (int i, OVT_RecruitData recruit : m_aPlayerRecruits)
		{
			Widget itemWidget = workspace.CreateWidgets(m_RecruitItemLayout, container);
			if (!itemWidget)
				continue;
				
			// Get the handler and populate it
			OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(itemWidget.FindHandler(OVT_RecruitListEntryHandler));
			if (handler)
			{
				handler.Populate(recruit, i);
				handler.m_OnClicked.Insert(OnRecruitItemClicked);
			}
			
			// Select first recruit by default
			if (i == 0)
			{
				m_iSelectedIndex = 0;
				SelectRecruit(recruit, itemWidget);
			}
		}
	}
	
	protected void OnRecruitItemClicked(SCR_ButtonBaseComponent button)
	{
		OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(button);
		if (!handler)
			return;
			
		SelectRecruitByIndex(handler.m_iIndex);
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
		
		// Update status
		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStatus"));
		if (statusText)
		{
			IEntity recruitEntity = m_RecruitManager.GetRecruitEntity(m_SelectedRecruit.m_sRecruitId);
			if (recruitEntity)
			{
				SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(
					recruitEntity.FindComponent(SCR_CharacterDamageManagerComponent)
				);
				
				if (damageManager && damageManager.GetState() == EDamageState.DESTROYED)
				{
					statusText.SetText("#OVT-Recruit_StatusDead");
					statusText.SetColor(Color.Red);
					
					// Disable action buttons for dead recruits
					SetButtonEnabled("ShowOnMapButton", false);
				}
				else
				{
					float distance = vector.Distance(m_Owner.GetOrigin(), recruitEntity.GetOrigin());
					statusText.SetTextFormat("#OVT-Recruit_StatusActive", Math.Round(distance));
					statusText.SetColor(Color.Green);
					
					// Enable action buttons for alive recruits
					SetButtonEnabled("ShowOnMapButton", true);
				}
			}
			else
			{
				statusText.SetText("#OVT-Recruit_StatusOffline");
				statusText.SetColor(Color.Gray);
				
				// Disable action buttons for offline recruits
				SetButtonEnabled("ShowOnMapButton", false);
			}
		}
	}
	
	protected void SelectPreviousRecruit()
	{
		if (m_aPlayerRecruits.IsEmpty())
			return;
			
		int newIndex = m_iSelectedIndex - 1;
		if (newIndex < 0)
			newIndex = m_aPlayerRecruits.Count() - 1;
			
		SelectRecruitByIndex(newIndex);
	}
	
	protected void SelectNextRecruit()
	{
		if (m_aPlayerRecruits.IsEmpty())
			return;
			
		int newIndex = m_iSelectedIndex + 1;
		if (newIndex >= m_aPlayerRecruits.Count())
			newIndex = 0;
			
		SelectRecruitByIndex(newIndex);
	}
	
	protected void SelectRecruitByIndex(int index)
	{
		if (index < 0 || index >= m_aPlayerRecruits.Count())
			return;
			
		m_iSelectedIndex = index;
		OVT_RecruitData recruit = m_aPlayerRecruits[index];
		
		// Find the widget for this recruit
		Widget container = m_wRoot.FindAnyWidget("RecruitsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		int i = 0;
		while (child && i <= index)
		{
			if (i == index)
			{
				SelectRecruit(recruit, child);
				break;
			}
			child = child.GetSibling();
			i++;
		}
	}
	
	protected void SetButtonsVisible(bool visible)
	{
		array<string> buttonNames = {
			"DismissButton",
			"RenameButton", 
			"ShowOnMapButton"
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
			
		// Find recruit entity
		IEntity recruitEntity = m_RecruitManager.GetRecruitEntity(m_SelectedRecruit.m_sRecruitId);
		if (recruitEntity)
		{
			// Remove from group
			AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
			if (aiControl)
			{
				AIAgent agent = aiControl.GetAIAgent();
				if (agent && agent.GetParentGroup())
				{
					agent.GetParentGroup().RemoveAgent(agent);
				}
			}
			
			// Delete entity
			SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
		}
		
		// Remove from manager
		m_RecruitManager.RemoveRecruit(m_SelectedRecruit.m_sRecruitId);
		
		// Refresh UI
		m_SelectedRecruit = null;
		m_wSelectedWidget = null;
		Refresh();
		
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
		
		// Use the recruit manager to handle all rename logic
		bool success = m_RecruitManager.RenameRecruit(m_CurrentRenamingRecruit.m_sRecruitId, newName);
		
		if (success)
		{
			// Refresh UI to show new name
			UpdateRecruitDetails();
			Refresh();
			ShowHint("#OVT-Recruit_Renamed");
		}
		else
		{
			ShowHint("Invalid name length (1-32 characters)");
		}
		
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
				jobManager.m_vCurrentWaypoint = pos;
			}
		}
		
		CloseLayout();
		ShowHint("#OVT-Recruit_MarkedOnMap");
	}
	
}