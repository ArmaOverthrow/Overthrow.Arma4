//! Main Overthrow map UI component that manages all interactive map elements
//! Extends base game SCR_MapUIElementContainer for proper integration
class OVT_OverthrowMapUI : SCR_MapUIElementContainer
{
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Location element layout", params: "layout")]
	protected ResourceName m_LocationElementLayout;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Info panel layout", params: "layout")]
	protected ResourceName m_InfoPanelLayout;
	
	[Attribute("", UIWidgets.Object, "Overthrow Map Location Types")]
	protected ref array<ref OVT_MapLocationType> m_aLocationTypes;
	
	//! All location instances managed by this UI
	protected ref array<ref OVT_MapLocationData> m_aLocations;
	
	//! Currently selected location element
	protected ref OVT_MapLocationElement m_SelectedElement;
	
	//! Info panel widget
	protected Widget m_wInfoPanel;
	
	//! Whether the info panel is currently visible
	protected bool m_bInfoPanelVisible = false;
	
	
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
	}
	
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		
		// Hide info panel
		HideLocationInfo();
		
		// Clear references
		m_aLocations = null;
		m_SelectedElement = null;
	}
	
	override void UpdateIcons()
	{
		super.UpdateIcons();
		
		// Update info panel position if visible
		if (m_bInfoPanelVisible && m_wInfoPanel && m_SelectedElement)
		{
			UpdateInfoPanelPosition();
		}
	}
	
	//! Initialize all location types
	protected void InitializeLocationTypes()
	{
		if (!m_aLocationTypes)
			return;
		
		foreach (OVT_MapLocationType locationType : m_aLocationTypes)
		{
			if (locationType)
				locationType.Init(this);
		}
	}
	
	//! Populate locations from all location types
	protected void PopulateAllLocations()
	{
		if (!m_aLocationTypes)
			return;
		
		foreach (OVT_MapLocationType locationType : m_aLocationTypes)
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
		if (!m_aLocationTypes)
			return null;
		
		foreach (OVT_MapLocationType locationType : m_aLocationTypes)
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
		
		// Hide existing panel
		HideLocationInfo();
		
		// Create info panel
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wInfoPanel = workspace.CreateWidgets(m_InfoPanelLayout, m_RootWidget);
		if (!m_wInfoPanel)
			return;
		
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
	
	//! Hide location info panel
	void HideLocationInfo()
	{
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
		
		// Set location type
		TextWidget typeText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("LocationType"));
		if (typeText)
		{
			OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
			string typeName;
			if (locationType)
				typeName = locationType.GetDisplayNameForLocation(location);
			else
				typeName = "Unknown";
			typeText.SetText(typeName);
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
	}
	
	//! Setup fast travel button functionality
	protected void SetupFastTravelButton(OVT_MapLocationData location)
	{
		if (!m_wInfoPanel || !location)
			return;
		
		ButtonWidget fastTravelButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("FastTravelButton"));
		if (!fastTravelButton)
			return;
		
		// Check if fast travel is available
		OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
		if (!locationType)
		{
			fastTravelButton.SetVisible(false);
			return;
		}
		
		string playerID = GetCurrentPlayerID();
		string reason;
		bool canFastTravel = locationType.CanFastTravel(location, playerID, reason);
		
		fastTravelButton.SetVisible(canFastTravel);
		
		if (canFastTravel)
		{
			SCR_ButtonBaseComponent buttonComp = SCR_ButtonBaseComponent.Cast(fastTravelButton.FindHandler(SCR_ButtonBaseComponent));
			if (buttonComp)
			{
				buttonComp.m_OnClicked.Insert(OnFastTravelClicked);
			}
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
		
		// Execute fast travel through the service
		OVT_FastTravelService.ExecuteFastTravel(location.m_vPosition, GetCurrentPlayerIDInt());
		
		// Hide info panel and close map
		HideLocationInfo();
		HideMap();
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
		x += 50;
		y -= 50;
		
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
	
	//! Hide the map
	protected void HideMap()
	{
		// Find and trigger map close
		if (m_MapEntity)
			m_MapEntity.CloseMap();
	}
	
}