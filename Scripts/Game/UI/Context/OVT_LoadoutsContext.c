class OVT_LoadoutsContext : OVT_UIContext
{
	[Attribute("{5D5C558A6E391692}UI/Layouts/Menu/LoadoutsMenu/LoadoutListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for loadout list items", params: "layout")]
	ResourceName m_LoadoutItemLayout;
	[Attribute("{5D5C558A6E391693}UI/Layouts/Menu/RecruitsMenu/RecruitListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for recruit list items", params: "layout")]
	ResourceName m_RecruitItemLayout;
	
	protected OVT_LoadoutManagerComponent m_LoadoutManager;
	protected ref array<string> m_aPlayerLoadoutNames;
	protected string m_SelectedLoadoutName;
	protected Widget m_wSelectedWidget;
	protected int m_iSelectedIndex = -1;
	protected IEntity m_EquipmentBox;
	protected ref array<IEntity> m_aNearbyRecruits;
	protected IEntity m_SelectedRecruit;
	protected Widget m_wSelectedRecruitWidget;
	protected int m_iSelectedRecruitIndex = -1;
	
	override void PostInit()
	{		
		m_LoadoutManager = OVT_Global.GetLoadouts();
		
		// Initialize arrays
		m_aNearbyRecruits = new array<IEntity>();
		
		// Set the main layout if not already set through attributes
		if (!m_Layout)
		{
			m_Layout = "{5D5C558A6E391692}UI/Layouts/Menu/LoadoutsMenu.layout";
		}
	}
	
	override void OnShow()
	{		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if (closeButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(CloseLayout);
		}
		
		// Set up button click handlers
		Widget applyButton = m_wRoot.FindAnyWidget("ApplyButton");
		if (applyButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(applyButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ApplyLoadout);
		}
		
		Widget deleteButton = m_wRoot.FindAnyWidget("DeleteButton");
		if (deleteButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(deleteButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(DeleteLoadout);
		}
		
		Widget applyToSelectedRecruitButton = m_wRoot.FindAnyWidget("ApplyToSelectedRecruitButton");
		if (applyToSelectedRecruitButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(applyToSelectedRecruitButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ApplyLoadoutToSelectedRecruit);
		}
		
		Widget applyToAllRecruitsButton = m_wRoot.FindAnyWidget("ApplyToAllRecruitsButton");
		if (applyToAllRecruitsButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(applyToAllRecruitsButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ApplyLoadoutToAllRecruits);
		}
		
		// Use the owner entity for applying loadouts
		
		Refresh();
		RefreshRecruits();
	}
	
	override void OnActiveFrame(float timeSlice)
	{
		super.OnActiveFrame(timeSlice);
		
		// Handle loadout list navigation
		if (m_InputManager.GetActionTriggered("MenuUp"))
		{
			SelectPreviousLoadout();
		}
		
		if (m_InputManager.GetActionTriggered("MenuDown"))
		{
			SelectNextLoadout();
		}
		
		if (m_InputManager.GetActionTriggered("MenuSelect"))
		{
			ApplyLoadout();
		}
	}
	
	protected void Refresh()
	{
		if (!m_LoadoutManager)
			return;
			
		// Get player loadouts
		m_aPlayerLoadoutNames = m_LoadoutManager.GetAvailableLoadouts(m_sPlayerID);
		
		// Clear the list
		Widget container = m_wRoot.FindAnyWidget("LoadoutsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		
		// Update help text visibility
		Widget helpText = m_wRoot.FindAnyWidget("NoLoadoutsHelp");
		Widget loadoutInfo = m_wRoot.FindAnyWidget("LoadoutInfo");
		
		if (m_aPlayerLoadoutNames.IsEmpty())
		{
			// Show help text when no loadouts
			if (helpText)
				helpText.SetVisible(true);
			if (loadoutInfo)
				loadoutInfo.SetVisible(false);
				
			// Hide action buttons
			SetButtonsVisible(false);
			
			m_SelectedLoadoutName = "";
			return;
		}
		else
		{
			// Hide help text when has loadouts
			if (helpText)
				helpText.SetVisible(false);
			if (loadoutInfo)
				loadoutInfo.SetVisible(true);
		}
		
		// Populate loadout list
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (int i, string loadoutName : m_aPlayerLoadoutNames)
		{
			Widget itemWidget = workspace.CreateWidgets(m_LoadoutItemLayout, container);
			if (!itemWidget)
				continue;
				
			// Get the handler and populate it
			OVT_LoadoutListEntryHandler handler = OVT_LoadoutListEntryHandler.Cast(itemWidget.FindHandler(OVT_LoadoutListEntryHandler));
			if (handler)
			{
				handler.Populate(loadoutName, i);
				handler.m_OnClicked.Insert(OnLoadoutItemClicked);
			}
			
			// Select first loadout by default
			if (i == 0)
			{
				m_iSelectedIndex = 0;
				SelectLoadout(loadoutName, itemWidget);
			}
		}
	}
	
	protected void OnLoadoutItemClicked(SCR_ButtonBaseComponent button)
	{
		OVT_LoadoutListEntryHandler handler = OVT_LoadoutListEntryHandler.Cast(button);
		if (!handler)
			return;
			
		SelectLoadoutByIndex(handler.m_iIndex);
	}
	
	void SelectLoadout(string loadoutName, Widget widget)
	{
		// Update selection highlight
		if (m_wSelectedWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0);
		}
		
		m_SelectedLoadoutName = loadoutName;
		m_wSelectedWidget = widget;
		
		if (widget)
		{
			ImageWidget bg = ImageWidget.Cast(widget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0.3);
		}
		
		// Update loadout details
		UpdateLoadoutDetails();
	}
	
	protected void UpdateLoadoutDetails()
	{
		if (m_SelectedLoadoutName.IsEmpty())
		{
			SetButtonsVisible(false);
			return;
		}
		
		SetButtonsVisible(true);
		
		// Update name
		TextWidget nameText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedName"));
		if (nameText)
			nameText.SetText(m_SelectedLoadoutName);
		
		// TODO: Add more details like item count, last used, etc.
		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStatus"));
		if (statusText)
			statusText.SetText("Ready to apply");
		
	// Update recruit button visibility when loadout selection changes
	UpdateRecruitButtonVisibility();
	}
	
	protected void SelectPreviousLoadout()
	{
		if (m_aPlayerLoadoutNames.IsEmpty())
			return;
			
		int newIndex = m_iSelectedIndex - 1;
		if (newIndex < 0)
			newIndex = m_aPlayerLoadoutNames.Count() - 1;
			
		SelectLoadoutByIndex(newIndex);
	}
	
	protected void SelectNextLoadout()
	{
		if (m_aPlayerLoadoutNames.IsEmpty())
			return;
			
		int newIndex = m_iSelectedIndex + 1;
		if (newIndex >= m_aPlayerLoadoutNames.Count())
			newIndex = 0;
			
		SelectLoadoutByIndex(newIndex);
	}
	
	protected void SelectLoadoutByIndex(int index)
	{
		if (index < 0 || index >= m_aPlayerLoadoutNames.Count())
			return;
			
		m_iSelectedIndex = index;
		string loadoutName = m_aPlayerLoadoutNames[index];
		
		// Find the widget for this loadout
		Widget container = m_wRoot.FindAnyWidget("LoadoutsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		int i = 0;
		while (child && i <= index)
		{
			if (i == index)
			{
				SelectLoadout(loadoutName, child);
				break;
			}
			child = child.GetSibling();
			i++;
		}
	}
	
	protected void SetButtonsVisible(bool visible)
	{
		array<string> buttonNames = {
			"ApplyButton",
			"DeleteButton"
		};
		
		foreach (string name : buttonNames)
		{
			Widget button = m_wRoot.FindAnyWidget(name);
			if (button)
				button.SetVisible(visible);
		}
	}
	
	protected void ApplyLoadout()
	{
		Print(string.Format("[OVT_LoadoutsContext] ApplyLoadout called - PlayerID: %1, LoadoutName: %2", m_sPlayerID, m_SelectedLoadoutName));
		
		if (m_SelectedLoadoutName.IsEmpty() || !m_Owner || !m_EquipmentBox)
		{
			Print(string.Format("[OVT_LoadoutsContext] ApplyLoadout failed - Empty name: %1, No owner: %2, No equipment box: %3", 
				m_SelectedLoadoutName.IsEmpty(), !m_Owner, !m_EquipmentBox));
			return;
		}
			
		// Apply the selected loadout via server RPC
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			Print(string.Format("[OVT_LoadoutsContext] Sending RPC for loadout: %1", m_SelectedLoadoutName));
			comms.LoadLoadoutFromBox(m_sPlayerID, m_SelectedLoadoutName, m_EquipmentBox, m_Owner);
			CloseLayout();
			// Notification will be sent by the loadout manager after processing
		}
		else
		{
			Print("[OVT_LoadoutsContext] No server comms component found");
		}
	}
	
	//! Set the equipment box entity to use for loadout operations
	void SetEquipmentBox(IEntity equipmentBox)
	{
		m_EquipmentBox = equipmentBox;
	}
	
	protected void DeleteLoadout()
	{
		if (m_SelectedLoadoutName.IsEmpty())
			return;
			
		// Delete the selected loadout via server RPC
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			comms.DeleteLoadout(m_sPlayerID, m_SelectedLoadoutName, false); // Assume personal loadout for now
			// Notification will be sent by the loadout manager after processing
			
			// Refresh the loadout list
			Refresh();
		}
		else
		{
			Print("[OVT_LoadoutsContext] Failed to delete loadout - no server communication");
		}
	}
	
	protected void RefreshRecruits()
	{
		if (!m_Owner)
			return;
		
		// Ensure array is initialized
		if (!m_aNearbyRecruits)
			m_aNearbyRecruits = new array<IEntity>();
		
		m_aNearbyRecruits.Clear();
		
		DiscoverNearbyRecruits();
		
		Widget recruitContainer = m_wRoot.FindAnyWidget("RecruitsList");
		if (!recruitContainer)
			return;
			
		Widget child = recruitContainer.GetChildren();
		while(child)
		{
			recruitContainer.RemoveChild(child);
			child = recruitContainer.GetChildren();
		}
		
		Widget noRecruitsText = m_wRoot.FindAnyWidget("NoRecruitsText");
		Widget recruitsTitle = m_wRoot.FindAnyWidget("RecruitsTitle");
		
		if (m_aNearbyRecruits.IsEmpty())
		{
			if (noRecruitsText)
				noRecruitsText.SetVisible(true);
			if (recruitsTitle)
				recruitsTitle.SetVisible(false);
				
			SetRecruitButtonsVisible(false);
			
			m_SelectedRecruit = null;
			m_iSelectedRecruitIndex = -1;
			UpdateSelectedRecruitDisplay();
			return;
		}
		else
		{
			if (noRecruitsText)
				noRecruitsText.SetVisible(false);
			if (recruitsTitle)
				recruitsTitle.SetVisible(true);
		}
		
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (int i, IEntity recruitEntity : m_aNearbyRecruits)
		{
			Widget itemWidget = workspace.CreateWidgets(m_RecruitItemLayout, recruitContainer);
			if (!itemWidget)
				continue;
				
			string recruitName = GetCharacterName(recruitEntity);
			
			OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(itemWidget.FindHandler(OVT_RecruitListEntryHandler));
			if (handler)
			{
				handler.PopulateFromEntity(recruitEntity, recruitName, i);
				handler.m_OnClicked.Insert(OnRecruitItemClicked);
			}
			
			if (i == 0)
			{
				m_iSelectedRecruitIndex = 0;
				SelectRecruit(recruitEntity, itemWidget);
			}
		}
		
		UpdateRecruitButtonVisibility();
	}
	
	protected void DiscoverNearbyRecruits()
	{
		if (!m_Owner)
			return;
		
		vector playerPos = m_Owner.GetOrigin();
		float searchRadius = 5.0;
		
		GetGame().GetWorld().QueryEntitiesBySphere(playerPos, searchRadius, null, FilterRecruitEntities, EQueryEntitiesFlags.ALL);
	}
	
	protected bool FilterRecruitEntities(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || character == m_Owner)
			return false;
			
		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(entity.FindComponent(OVT_PlayerOwnerComponent));
		if (!ownerComp)
			return false;
			
		string ownerUID = ownerComp.GetPlayerOwnerUid();
		if (ownerUID == m_sPlayerID)
		{
			m_aNearbyRecruits.Insert(entity);
		}
		
		return false;
	}
	
	protected string GetCharacterName(IEntity character)
	{
		// First try to get name from recruit manager
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (recruitManager)
		{
			OVT_RecruitData recruitData = recruitManager.GetRecruitFromEntity(character);
			if (recruitData)
			{
				string recruitName = recruitData.GetName();
				if (!recruitName.IsEmpty())
					return recruitName;
			}
		}
		
		// Fallback to editable character component
		SCR_EditableCharacterComponent editableChar = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
		if (editableChar)
		{
			string displayName = editableChar.GetDisplayName();
			if (!displayName.IsEmpty())
				return displayName;
		}
		
		return "Recruit";
	}
	
	protected void OnRecruitItemClicked(SCR_ButtonBaseComponent button)
	{
		OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(button);
		if (!handler)
			return;
			
		SelectRecruitByIndex(handler.GetIndex());
	}
	
	void SelectRecruit(IEntity recruitEntity, Widget widget)
	{
		if (m_wSelectedRecruitWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedRecruitWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0);
		}
		
		m_SelectedRecruit = recruitEntity;
		m_wSelectedRecruitWidget = widget;
		
		if (widget)
		{
			ImageWidget bg = ImageWidget.Cast(widget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0.3);
		}
		
		UpdateSelectedRecruitDisplay();
		UpdateRecruitButtonVisibility();
	}
	
	protected void SelectRecruitByIndex(int index)
	{
		if (!m_aNearbyRecruits || index < 0 || index >= m_aNearbyRecruits.Count())
			return;
			
		m_iSelectedRecruitIndex = index;
		IEntity recruitEntity = m_aNearbyRecruits[index];
		
		Widget container = m_wRoot.FindAnyWidget("RecruitsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		int i = 0;
		while (child && i <= index)
		{
			if (i == index)
			{
				SelectRecruit(recruitEntity, child);
				break;
			}
			child = child.GetSibling();
			i++;
		}
	}
	
	protected void UpdateSelectedRecruitDisplay()
	{
		TextWidget selectedRecruitText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedRecruitName"));
		if (!selectedRecruitText)
			return;
			
		if (m_SelectedRecruit)
		{
			string recruitName = GetCharacterName(m_SelectedRecruit);
			selectedRecruitText.SetTextFormat("#OVT-Loadouts_SelectedRecruit", recruitName);
		}
		else
		{
			selectedRecruitText.SetText("#OVT-Loadouts_NoRecruitSelected");
		}
	}

	protected void UpdateRecruitButtonVisibility()
	{
		bool hasLoadout = !m_SelectedLoadoutName.IsEmpty();
		bool hasRecruits = m_aNearbyRecruits && !m_aNearbyRecruits.IsEmpty();
		bool hasSelectedRecruit = m_SelectedRecruit != null;
		
		SetRecruitButtonsVisible(hasLoadout && hasRecruits);
		
		Widget applyToSelectedButton = m_wRoot.FindAnyWidget("ApplyToSelectedRecruitButton");
		if (applyToSelectedButton)
			applyToSelectedButton.SetEnabled(hasLoadout && hasSelectedRecruit);
			
		Widget applyToAllButton = m_wRoot.FindAnyWidget("ApplyToAllRecruitsButton");
		if (applyToAllButton)
			applyToAllButton.SetEnabled(hasLoadout && hasRecruits);
	}
	
	protected void SetRecruitButtonsVisible(bool visible)
	{
		array<string> buttonNames = {
			"ApplyToSelectedRecruitButton",
			"ApplyToAllRecruitsButton"
		};
		
		foreach (string name : buttonNames)
		{
			Widget button = m_wRoot.FindAnyWidget(name);
			if (button)
				button.SetVisible(visible);
		}
	}
	
	protected void ApplyLoadoutToSelectedRecruit()
	{
		if (m_SelectedLoadoutName.IsEmpty() || !m_SelectedRecruit)
		{
			Print("[OVT_LoadoutsContext] No recruit selected for loadout application");
			return;
		}
		
		if (!m_EquipmentBox)
		{
			Print("[OVT_LoadoutsContext] No equipment box available for loadout application");
			return;
		}
		
		ApplyLoadoutToRecruit(m_SelectedRecruit);
		CloseLayout();
		// Notification will be sent by the loadout manager after processing
	}
	
	protected void ApplyLoadoutToAllRecruits()
	{
		if (m_SelectedLoadoutName.IsEmpty() || !m_aNearbyRecruits || m_aNearbyRecruits.IsEmpty())
		{
			Print("[OVT_LoadoutsContext] No recruits nearby for loadout application");
			return;
		}
		
		if (!m_EquipmentBox)
		{
			Print("[OVT_LoadoutsContext] No equipment box available for loadout application");
			return;
		}
		
		// Apply loadout to all recruits - each will send its own notification
		foreach (IEntity recruit : m_aNearbyRecruits)
		{
			ApplyLoadoutToRecruit(recruit);
		}
		CloseLayout();
		// Individual notifications will be sent by the loadout manager for each recruit
	}
	
	protected bool ApplyLoadoutToRecruit(IEntity recruitEntity)
	{
		if (!recruitEntity || m_SelectedLoadoutName.IsEmpty() || !m_EquipmentBox)
			return false;
		
		// Apply loadout via server RPC
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			Print(string.Format("[OVT_LoadoutsContext] Sending RPC to apply loadout to recruit: %1", m_SelectedLoadoutName));
			comms.LoadLoadoutFromBox(m_sPlayerID, m_SelectedLoadoutName, m_EquipmentBox, recruitEntity);
			return true;
		}
		else
		{
			Print("[OVT_LoadoutsContext] No server comms component found");
			return false;
		}
	}
}