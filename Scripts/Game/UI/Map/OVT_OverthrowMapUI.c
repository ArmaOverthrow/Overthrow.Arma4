[BaseContainerProps(configRoot: true)]
class OVT_OverthrowMapConfig
{
	[Attribute("", UIWidgets.Object, "Overthrow Map Location Types")]
	ref array<ref OVT_MapLocationType> m_aLocationTypes;
}

//! Main Overthrow map UI component that manages all interactive map elements
//! Extends base game SCR_MapUIElementContainer for proper integration
class OVT_OverthrowMapUI : SCR_MapUIElementContainer
{
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Location element layout", params: "layout")]
	protected ResourceName m_LocationElementLayout;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Info panel layout", params: "layout")]
	protected ResourceName m_InfoPanelLayout;
	
	[Attribute(defvalue: "OVT_OverthrowMapConfig", UIWidgets.Object, "Overthrow Map Config")]
	protected ref OVT_OverthrowMapConfig m_Config;
	
	//! All location instances managed by this UI
	protected ref array<ref OVT_MapLocationData> m_aLocations;
	
	//! Currently selected location element
	protected ref OVT_MapLocationElement m_SelectedElement;
	
	//! Info panel widget
	protected Widget m_wInfoPanel;
	
	//! Whether the info panel is currently visible
	protected bool m_bInfoPanelVisible = false;
	
	//! Whether the current selection is "pinned" (clicked) vs just hovered
	protected bool m_bSelectionPinned = false;
	
	//! Currently hovered element (for temporary hover info)
	protected ref OVT_MapLocationElement m_HoveredElement;
	
	//! Currently pinned element (for persistent info)
	protected ref OVT_MapLocationElement m_PinnedElement;
	
	
	//! Handle map selection events for pinning
	protected void OnMapSelection(vector selectionPos)
	{
		
		if (m_HoveredElement)
		{
			// Click on hovered element - pin it (replace any existing pin)
			
			// If clicking on a different element than currently pinned, replace it
			if (m_PinnedElement != m_HoveredElement)
			{
				m_PinnedElement = m_HoveredElement;
				m_bSelectionPinned = true;
				
				// Force show info for the new pinned element
				SelectLocation(m_PinnedElement);
				ShowLocationInfo(m_PinnedElement.GetLocationData());
			}
		}
		else
		{
			// Click on empty space - unpin current selection
			m_bSelectionPinned = false;
			m_PinnedElement = null;
			ForceHideLocationInfo();
		}
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		// Initialize arrays
		m_aLocations = new array<ref OVT_MapLocationData>;
		
		// Initialize location types
		InitializeLocationTypes();
		
		// Populate all locations
		PopulateAllLocations();
		
		// Create location elements
		CreateLocationElements();
		
		// Connect to map events
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		m_MapEntity.GetOnSelection().Insert(OnMapSelection);
	}
	
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		
		// Disconnect from map events
		m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
		m_MapEntity.GetOnSelection().Remove(OnMapSelection);
		
		// Force hide info panel (even if pinned)
		ForceHideLocationInfo();
		
		// Clear references
		m_aLocations = null;
		m_SelectedElement = null;
	}
	
	override void UpdateIcons()
	{
		super.UpdateIcons();
		
		// Update info panel position if visible (especially important for pinned panels)
		if (m_bInfoPanelVisible && m_wInfoPanel)
		{
			// Use pinned element if available, otherwise selected element
			OVT_MapLocationElement targetElement;
			if (m_PinnedElement)
				targetElement = m_PinnedElement;
			else
				targetElement = m_SelectedElement;
			
			if (targetElement)
			{
				UpdateInfoPanelPosition();
			}
		}
	}
	
	//! Called when the map zoom level changes
	protected void OnMapZoom(float zoomVal)
	{
		// Notify all location elements about zoom change
		foreach (Widget widget, SCR_MapUIElement element : m_mIcons)
		{
			OVT_MapLocationElement locationElement = OVT_MapLocationElement.Cast(element);
			if (locationElement)
				locationElement.OnZoomChanged();
		}
	}
	
	//! Initialize all location types
	protected void InitializeLocationTypes()
	{
		if (!m_Config.m_aLocationTypes)
			return;
		
		foreach (OVT_MapLocationType locationType : m_Config.m_aLocationTypes)
		{
			if (locationType)
				locationType.Init(this);
		}
	}
	
	//! Populate locations from all location types
	protected void PopulateAllLocations()
	{
		if (!m_Config.m_aLocationTypes)
			return;
		
		foreach (OVT_MapLocationType locationType : m_Config.m_aLocationTypes)
		{
			if (locationType)
				locationType.PopulateLocations(m_aLocations);
		}
	}
	
	//! Create UI elements for all locations
	protected void CreateLocationElements()
	{
		if (!m_aLocations || m_LocationElementLayout.IsEmpty())
			return;
		
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (OVT_MapLocationData location : m_aLocations)
		{
			// Find the location type for this location
			OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
			if (!locationType)
				continue;
			
			// Create the widget from layout (handler attached via layout)
			Widget widget = workspace.CreateWidgets(m_LocationElementLayout, m_wIconsContainer);
			if (!widget)
				continue;
			
			// Find the handler attached to the widget
			OVT_MapLocationElement element = OVT_MapLocationElement.Cast(widget.FindHandler(OVT_MapLocationElement));
			if (!element)
				continue;
			
			// Initialize the handler with game data
			element.SetParent(this);
			element.Init(location, locationType, this);
			
			// Store in base class icon map
			m_mIcons.Set(widget, element);
			
			// Configure widget positioning
			FrameSlot.SetSizeToContent(widget, true);
			FrameSlot.SetAlignment(widget, 0.5, 0.5);
		}
		UpdateIcons();
	}
	
	
	//! Get location type by class name
	protected OVT_MapLocationType GetLocationTypeByName(string typeName)
	{
		if (!m_Config.m_aLocationTypes)
			return null;
		
		foreach (OVT_MapLocationType locationType : m_Config.m_aLocationTypes)
		{
			if (locationType && locationType.ClassName() == typeName)
				return locationType;
		}
		
		return null;
	}
	
	//! Select a location element
	void SelectLocation(OVT_MapLocationElement element)
	{
		// Deselect previous
		if (m_SelectedElement && m_SelectedElement != element)
			m_SelectedElement.SetSelected(false);
		
		// Select new
		m_SelectedElement = element;
		if (m_SelectedElement)
			m_SelectedElement.SetSelected(true);
	}
	
	//! Show location info panel
	void ShowLocationInfo(OVT_MapLocationData location)
	{
		if (!location || m_InfoPanelLayout.IsEmpty())
			return;
		
		// Always force hide any existing panel first to ensure only one exists
		if (m_wInfoPanel)
		{
			m_wInfoPanel.RemoveFromHierarchy();
			m_wInfoPanel = null;
		}
		
		// Create info panel
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wInfoPanel = workspace.CreateWidgets(m_InfoPanelLayout, m_RootWidget);
		if (!m_wInfoPanel)
			return;
		
		m_wInfoPanel.SetZOrder(20);
				
		// Setup base info
		SetupLocationInfoBase(location);
		
		// Let location type populate specific info
		OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
		if (locationType)
		{
			Widget contentSlot = m_wInfoPanel.FindAnyWidget("ContentSlot");
			if (contentSlot)
				locationType.UpdateInfoPanel(location, contentSlot);
		}
		
		// Position the panel
		UpdateInfoPanelPosition();
		
		// Setup close button
		ButtonWidget closeButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("CloseButton"));
		if (closeButton)
		{
			SCR_ButtonBaseComponent closeComp = SCR_ButtonBaseComponent.Cast(closeButton.FindHandler(SCR_ButtonBaseComponent));
			if (closeComp)
				closeComp.m_OnClicked.Insert(HideLocationInfo);
		}
		
		// Setup fast travel button
		SetupFastTravelButton(location);
		
		m_bInfoPanelVisible = true;
		
		// Notify the selected element that info popup is visible
		if (m_SelectedElement)
			m_SelectedElement.SetInfoPopupVisible(true);
	}
	
	//! Hide location info panel (only if not pinned)
	void HideLocationInfo()
	{
		// Don't hide if selection is pinned
		if (m_bSelectionPinned)
		{
			return;
		}
		
		if (m_wInfoPanel)
		{
			m_wInfoPanel.RemoveFromHierarchy();
			m_wInfoPanel = null;
		}
		
		m_bInfoPanelVisible = false;
		
		// Notify the selected element that info popup is hidden
		if (m_SelectedElement)
			m_SelectedElement.SetInfoPopupVisible(false);
	}
	
	//! Force hide location info panel (even if pinned)
	void ForceHideLocationInfo()
	{
		m_bSelectionPinned = false;
		m_PinnedElement = null;
		
		if (m_wInfoPanel)
		{
			m_wInfoPanel.RemoveFromHierarchy();
			m_wInfoPanel = null;
		}
		
		m_bInfoPanelVisible = false;
		
		// Notify the selected element that info popup is hidden
		if (m_SelectedElement)
			m_SelectedElement.SetInfoPopupVisible(false);
	}
	
	//! Unpin any pinned location when hovering a new one
	void UnpinOnHover()
	{
		if (m_bSelectionPinned)
		{
			m_bSelectionPinned = false;
			m_PinnedElement = null;
		}
	}
	
	//! Set the currently hovered element
	void SetHoveredElement(OVT_MapLocationElement element)
	{
		m_HoveredElement = element;
	}
	
	//! Get the currently hovered element
	OVT_MapLocationElement GetHoveredElement()
	{
		return m_HoveredElement;
	}
	
	//! Setup base location info (name, distance, etc.)
	protected void SetupLocationInfoBase(OVT_MapLocationData location)
	{
		if (!m_wInfoPanel || !location)
			return;
		
		// Set location name
		TextWidget nameText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("LocationName"));
		if (nameText)
		{
			OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
			string name;
			if (locationType)
				name = locationType.GetLocationName(location);
			else
				name = location.m_sName;
			nameText.SetText(name);
		}
		
		// Set location type (description)
		TextWidget typeText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("LocationType"));
		if (typeText)
		{
			OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
			string description;
			if (locationType)
				description = locationType.GetLocationDescription(location);
			else
				description = "Unknown";
			typeText.SetText(description);
		}
		
		// Set distance
		TextWidget distanceText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("Distance"));
		if (distanceText)
		{
			float distance = location.GetDistanceFromPlayer();
			if (distance > 0)
			{
				string dis, units;
				SCR_Global.GetDistForHUD(distance, false, dis, units);
				distanceText.SetText(dis + " " + units);
			}
			else
			{
				distanceText.SetText("Unknown");
			}
		}
		
		// Set owner information if available
		TextWidget ownerText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("Owner"));
		if (!ownerText)
			return;
		
		string ownerID = location.GetDataString("owner", "");
		if (ownerID.IsEmpty())
		{
			ownerText.SetVisible(false);
			return;
		}
		
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
		{
			ownerText.SetVisible(false);
			return;
		}
		
		string ownerName = playerManager.GetPlayerName(ownerID);
		if (ownerName.IsEmpty())
		{
			ownerText.SetVisible(false);
			return;
		}
		
		ownerText.SetText("#OVT-Owner: " + ownerName);
		ownerText.SetVisible(true);
	}
	
	//! Setup fast travel button functionality
	protected void SetupFastTravelButton(OVT_MapLocationData location)
	{
		if (!m_wInfoPanel || !location)
			return;
		
		ButtonWidget fastTravelButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("FastTravelButton"));
		TextWidget reasonText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("FastTravelReason"));
		
		if (!fastTravelButton)
			return;
		
		// Check if fast travel is available for this location type
		OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
		if (!locationType)
		{
			fastTravelButton.SetVisible(false);
			if (reasonText)
				reasonText.SetVisible(false);
			return;
		}
		
		// Check if the location type supports fast travel at all
		if (!locationType.m_bCanFastTravel)
		{
			fastTravelButton.SetVisible(false);
			if (reasonText)
				reasonText.SetVisible(false);
			return;
		}
		
		// Location type supports fast travel, always show button
		fastTravelButton.SetVisible(true);
		
		// Check if fast travel is currently allowed
		string playerID = GetCurrentPlayerID();
		string reason;
		bool canFastTravel = locationType.CanFastTravel(location, playerID, reason);
		
		// Enable/disable button based on availability
		fastTravelButton.SetEnabled(canFastTravel);
		
		// Show/hide reason text
		if (reasonText)
		{
			if (canFastTravel)
			{
				reasonText.SetVisible(false);
			}
			else
			{
				reasonText.SetText(reason);
				reasonText.SetVisible(true);
			}
		}
		
		// Update button text with cost
		SCR_InputButtonComponent buttonComp = SCR_InputButtonComponent.Cast(fastTravelButton.FindHandler(SCR_InputButtonComponent));
		if (buttonComp)
		{
			// Calculate fast travel cost and update button text
			int cost = GetFastTravelCost(location.m_vPosition);
			string buttonText = "#OVT-MainMenu_FastTravel";
			if (cost > 0)
				buttonText = buttonText + " ($" + cost.ToString() + ")";
			
			buttonComp.SetLabel(buttonText);
			
			// Setup button click handler (always setup, button enable state handles availability)
			buttonComp.m_OnActivated.Clear();
			buttonComp.m_OnActivated.Insert(OnFastTravelClicked);
		}
	}
	
	//! Handle fast travel button click
	protected void OnFastTravelClicked()
	{
		// Use the currently selected element's location data
		if (!m_SelectedElement)
			return;
		
		OVT_MapLocationData location = m_SelectedElement.GetLocationData();
		if (!location)
			return;
		
		// Hide info panel and close map first
		HideLocationInfo();
		HideMap();
		
		// Execute fast travel through the service
		OVT_FastTravelService.ExecuteFastTravel(location.m_vPosition, GetCurrentPlayerIDInt());
	}
	
	//! Get fast travel cost based on distance
	protected int GetFastTravelCost(vector targetPos)
	{
		// Use the centralized cost calculation from the service
		return OVT_FastTravelService.CalculateFastTravelCost(targetPos, GetCurrentPlayerIDInt());
	}
	
	//! Update info panel position
	protected void UpdateInfoPanelPosition()
	{
		if (!m_wInfoPanel || !m_SelectedElement)
			return;
		
		// Position panel near selected element but not overlapping
		vector pos = m_SelectedElement.GetPos();
		float x, y;
		m_MapEntity.WorldToScreen(pos[0], pos[2], x, y, true);
		x = GetGame().GetWorkspace().DPIUnscale(x);
		y = GetGame().GetWorkspace().DPIUnscale(y);
		
		// Offset to avoid overlapping with icon
		x += 13;
		y -= 31;
		
		// Keep within screen bounds
		float panelWidth, panelHeight;
		m_wInfoPanel.GetScreenSize(panelWidth, panelHeight);
		
		float screenWidth, screenHeight;
		GetGame().GetWorkspace().GetScreenSize(screenWidth, screenHeight);
		
		if (x + panelWidth > screenWidth)
			x = screenWidth - panelWidth - 10;
		if (y < 0)
			y = 10;
		
		FrameSlot.SetPos(m_wInfoPanel, x, y);
	}
	
	//! Get current player ID as string
	protected string GetCurrentPlayerID()
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return "";
		
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
			return "";
		
		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
		return players.GetPersistentIDFromPlayerID(playerID);
	}
	
	//! Get current player ID as int
	protected int GetCurrentPlayerIDInt()
	{
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
			return -1;
		
		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
	}
	
	SCR_MapGadgetComponent GetMap()
	{
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
			return null;
			
		SCR_GadgetManagerComponent mgr = SCR_GadgetManagerComponent.Cast(playerEntity.FindComponent(SCR_GadgetManagerComponent));
		if(!mgr) return null;
		
		IEntity ent = mgr.GetQuickslotGadgetByType(EGadgetType.MAP);
		if(!ent) {		
			ent = mgr.GetGadgetByType(EGadgetType.MAP);
		}
		
		if(!ent) return null;
				
		SCR_MapGadgetComponent comp = SCR_MapGadgetComponent.Cast(ent.FindComponent(SCR_MapGadgetComponent));
		if(!comp) return null;
		
		return comp;
	}
	
	//! Hide the map
	protected void HideMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;
		
		comp.SetMapMode(false);
	}
	
}