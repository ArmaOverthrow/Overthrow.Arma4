//! Base class for map location types using hybrid config/code pattern
//! Follows the same pattern as OVT_BaseUpgrade for extensibility
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sDisplayName")]
class OVT_MapLocationType
{
	[Attribute(defvalue: "Generic Location", desc: "Display name for location type")]
	protected string m_sDisplayName;
	
	[Attribute(defvalue: "1", desc: "Visibility zoom level (0=always visible, higher=visible at closer zoom)")]
	protected float m_fVisibilityZoom;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Icon widget layout", params: "layout")]
	protected ResourceName m_IconLayout;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Info panel layout", params: "layout")]
	protected ResourceName m_InfoLayout;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Icon imageset", params: "imageset")]
	protected ResourceName m_IconImageset;
	
	[Attribute(defvalue: "", desc: "Icon name in imageset")]
	protected string m_sIconName;
	
	[Attribute(defvalue: "true", desc: "Show distance to location")]
	protected bool m_bShowDistance;
	
	[Attribute(defvalue: "false", desc: "Can fast travel to this location type by default")]
	protected bool m_bCanFastTravel;
	
	[Attribute(defvalue: "12", desc: "Icon size when zoomed out")]
	protected int m_iIconSizeSmall;
	
	[Attribute(defvalue: "24", desc: "Icon size when zoomed in")]
	protected int m_iIconSizeLarge;
	
	[Attribute(defvalue: "2", desc: "Zoom level to show location name (0=always, higher=closer zoom)")]
	protected float m_fShowNameZoom;
	
	[Attribute(defvalue: "true", desc: "Show location name when zoomed in")]
	protected bool m_bShowName;
	
	//! Reference to the map UI that owns this location type
	protected OVT_OverthrowMapUI m_MapUI;
	
	//! Cached reference to managers for performance
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_ResistanceFactionManager m_Resistance;
	protected OVT_OccupyingFactionManager m_OccupyingFaction;
	protected OVT_EconomyManagerComponent m_Economy;
	protected OVT_VehicleManagerComponent m_Vehicles;
	protected OVT_PlayerManagerComponent m_Players;
	
	//! Initialize the location type with references to required systems
	void Init(OVT_OverthrowMapUI mapUI)
	{
		m_MapUI = mapUI;
		
		// Cache manager references for performance
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
		m_Resistance = OVT_Global.GetResistanceFaction();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		m_Economy = OVT_Global.GetEconomy();
		m_Vehicles = OVT_Global.GetVehicles();
		m_Players = OVT_Global.GetPlayers();
		
		PostInit();
	}
	
	//! Called after initialization, override for custom setup
	void PostInit()
	{
		// Override in derived classes
	}
	
	//! Populate locations of this type
	//! Override this method to add your location instances to the locations array
	void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		// Override in derived classes
	}
	
	//! Check if specific location allows fast travel
	bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		// Default implementation - just return the configured value
		if (!m_bCanFastTravel)
		{
			reason = "Fast travel not available for this location type";
			return false;
		}
		
		return true;
	}
	
	//! Handle location selection (when clicked but not activated)
	void OnLocationSelected(OVT_MapLocationData location, OVT_MapLocationElement element)
	{
		// Override in derived classes for custom selection behavior
	}
	
	//! Handle location click/activation
	void OnLocationClicked(OVT_MapLocationData location, OVT_MapLocationElement element)
	{
		// Default behavior: show info panel
		if (m_MapUI)
			m_MapUI.ShowLocationInfo(location);
	}
	
	//! Update location-specific UI info panel
	void UpdateInfoPanel(OVT_MapLocationData location, Widget infoPanel)
	{
		if (!infoPanel || !location || m_InfoLayout.IsEmpty())
			return;
		
		// Clear existing content in the content slot
		Widget child = infoPanel.GetChildren();
		while (child)
		{
			infoPanel.RemoveChild(child);
			child = infoPanel.GetChildren();
		}
		
		// Create location-specific info layout in the content slot
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget locationInfoWidget = workspace.CreateWidgets(m_InfoLayout, infoPanel);
		if (!locationInfoWidget)
			return;
		
		// Call derived class to populate location-specific data
		OnSetupLocationInfo(locationInfoWidget, location);
	}
	
	//! Override this in derived classes to populate location-specific info
	protected void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		// Override in derived classes to populate location-specific info
	}
	
	//! Get location name for display
	string GetLocationName(OVT_MapLocationData location)
	{
		return location.m_sName;
	}
	
	//! Get location description
	string GetLocationDescription(OVT_MapLocationData location)
	{
		return m_sDisplayName;
	}
	
	//! Check if location should be visible to the specified player
	bool ShouldShowLocation(OVT_MapLocationData location, string playerID)
	{
		return location.m_bVisible;
	}
	
	//! Get the icon layout resource for this location type
	ResourceName GetIconLayout()
	{
		return m_IconLayout;
	}
	
	//! Get the info panel layout resource for this location type
	ResourceName GetInfoLayout()
	{
		return m_InfoLayout;
	}
	
	//! Get the icon imageset resource
	ResourceName GetIconImageset()
	{
		return m_IconImageset;
	}
	
	//! Get the icon name within the imageset
	string GetIconName()
	{
		return m_sIconName;
	}
	
	//! Get the display name for this location type
	string GetDisplayName()
	{
		return m_sDisplayName;
	}
	
	//! Get the display name for a specific location (can be overridden for location-specific names)
	string GetDisplayNameForLocation(OVT_MapLocationData location)
	{
		return GetDisplayName();
	}
	
	//! Get the visibility zoom level
	float GetVisibilityZoom()
	{
		return m_fVisibilityZoom;
	}
	
	//! Check if this location type should show distance
	bool ShouldShowDistance()
	{
		return m_bShowDistance;
	}
	
	//! Check if this location type should show location name
	bool ShouldShowName()
	{
		return m_bShowName;
	}
	
	//! Get the zoom level at which to show location name
	float GetShowNameZoom()
	{
		return m_fShowNameZoom;
	}
	
	//! Get the icon color for this location (override in derived classes)
	Color GetIconColor(OVT_MapLocationData location)
	{
		return Color.Black; // Default black color for visibility on bright maps
	}
	
	//! Get small icon size
	int GetIconSizeSmall()
	{
		return m_iIconSizeSmall;
	}
	
	//! Get large icon size
	int GetIconSizeLarge()
	{
		return m_iIconSizeLarge;
	}
	
	//! Setup the icon widget for this location
	void SetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall = false)
	{
		if (!iconWidget)
			return;
		
		ImageWidget image = ImageWidget.Cast(iconWidget.FindAnyWidget("Icon"));
		if (image && !m_IconImageset.IsEmpty() && !m_sIconName.IsEmpty())
		{
			image.LoadImageFromSet(0, m_IconImageset, m_sIconName);
			
			// Apply icon color
			Color iconColor = GetIconColor(location);
			image.SetColor(iconColor);
		}
		
		// Set icon size based on zoom level
		SizeLayoutWidget sizeLayout = SizeLayoutWidget.Cast(iconWidget.FindAnyWidget("IconLayout"));
		if (sizeLayout)
		{
			int size;
			if (isSmall)
				size = m_iIconSizeSmall;
			else
				size = m_iIconSizeLarge;
			FrameSlot.SetSize(sizeLayout, size, size);
		}
		
		// Handle distance display
		TextWidget distanceText = TextWidget.Cast(iconWidget.FindAnyWidget("Distance"));
		if (distanceText)
		{
			if (m_bShowDistance)
			{
				float distance = location.GetDistanceFromPlayer();
				if (distance > 0)
				{
					string dis, units;
					SCR_Global.GetDistForHUD(distance, false, dis, units);
					distanceText.SetText(dis + " " + units);
					distanceText.SetVisible(true);
				}
				else
				{
					distanceText.SetVisible(false);
				}
			}
			else
			{
				distanceText.SetVisible(false);
			}
		}
		
		// Allow derived classes to customize icon setup
		OnSetupIconWidget(iconWidget, location, isSmall);
	}
	
	//! Override this for custom icon widget setup
	protected void OnSetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall)
	{
		// Override in derived classes
	}
	
	//! Get the current player's persistent ID
	protected string GetCurrentPlayerID()
	{
		if (!m_Players)
			return "";
		
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
			return "";
		
		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
		return m_Players.GetPersistentIDFromPlayerID(playerID);
	}
}