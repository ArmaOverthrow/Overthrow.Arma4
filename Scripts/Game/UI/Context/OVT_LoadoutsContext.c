class OVT_LoadoutsContext : OVT_UIContext
{
	[Attribute("UI/Layouts/Menu/LoadoutsMenu/LoadoutListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for loadout list items", params: "layout")]
	ResourceName m_LoadoutItemLayout;
	
	protected OVT_LoadoutManagerComponent m_LoadoutManager;
	protected ref array<string> m_aPlayerLoadoutNames;
	protected string m_SelectedLoadoutName;
	protected Widget m_wSelectedWidget;
	protected int m_iSelectedIndex = -1;
	protected IEntity m_EquipmentBox;
	
	override void PostInit()
	{		
		m_LoadoutManager = OVT_Global.GetLoadouts();
		
		// Set the main layout if not already set through attributes
		if (!m_Layout)
		{
			m_Layout = "{5D5C558A6E391692}UI/Layouts/Menu/LoadoutsMenu.layout";
		}
	}
	
	override void OnShow()
	{			
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
		
		// Use the owner entity for applying loadouts
		
		Refresh();
	}
	
	override void OnFrame(float timeSlice)
	{
		super.OnFrame(timeSlice);
		
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
		OVT_PlayerCommsComponent comms = OVT_PlayerCommsComponent.Cast(m_Owner.FindComponent(OVT_PlayerCommsComponent));
		if (comms)
		{
			Print(string.Format("[OVT_LoadoutsContext] Sending RPC for loadout: %1", m_SelectedLoadoutName));
			comms.LoadLoadoutFromBox(m_sPlayerID, m_SelectedLoadoutName, m_EquipmentBox, m_Owner);
			CloseLayout();
			ShowHint("#OVT-LoadoutApplied");
		}
		else
		{
			Print("[OVT_LoadoutsContext] No PlayerCommsComponent found on owner");
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
			
		// TODO: Add confirmation dialog and delete functionality
		ShowHint("Delete loadout feature coming soon!");
	}
}