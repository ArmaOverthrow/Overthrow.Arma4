//! Base class for map location types using hybrid config/code pattern
//! Follows the same pattern as OVT_BaseUpgrade for extensibility
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationType
{
	[Attribute(defvalue: "Location", desc: "Name for location type")]
	protected string m_sName;

	[Attribute(defvalue: "Generic Location", desc: "Display name for location type")]
	protected string m_sDisplayName;
	
	[Attribute(defvalue: "1", desc: "Visibility zoom level (0=always visible, higher=visible at closer zoom)")]
	protected float m_fVisibilityZoom;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Icon widget layout", params: "layout", category: "Icon")]
	protected ResourceName m_IconLayout;
	
	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Info panel layout", params: "layout", category: "Info")]
	protected ResourceName m_InfoLayout;

	[Attribute(defvalue: "{6A7E1C4D0000002A}UI/Layouts/Map/Core/OVT_MapInfoRows.layout", UIWidgets.ResourceNamePicker, desc: "Shared data-driven info panel. Used ONLY when m_InfoLayout is empty; a type with its own bespoke layout never reaches it.", params: "layout", category: "Info")]
	protected ResourceName m_SharedInfoLayout;

	//! Name of the vertical layout inside m_SharedInfoLayout that rows are added to.
	//! LAYOUT <-> CODE CONTRACT: UI/Layouts/Map/Core/OVT_MapInfoRows.layout must contain a widget with
	//! exactly this name, or BuildInfoRows is never called and every shared panel silently renders empty.
	protected static const string ROWS_CONTAINER = "Rows";

	//! Layout for one row inside the shared info panel.
	//! Deliberately a constant rather than an attribute: it is a matched pair with
	//! OVT_MapInfoRowHandler's RowLabel/RowValue/RowIcon name contract, so making it swappable from the
	//! conf would invite exactly the silent blank-row failure Q-8 exists to prevent.
	protected static const ResourceName INFO_ROW_LAYOUT = "{6A7E1C4D00000020}UI/Layouts/Map/Core/OVT_MapInfoRow.layout";

	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, desc: "Icon imageset", params: "imageset", category: "Icon")]
	protected ResourceName m_IconImageset;
	
	[Attribute(defvalue: "", desc: "Icon name in imageset", category: "Icon")]
	protected string m_sIconName;
	
	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Default icon color (black)", category: "Icon")]
	protected ref Color m_DefaultIconColor;
	
	[Attribute(defvalue: "false", desc: "Use faction color instead of default color", category: "Icon")]
	protected bool m_bUseFactionColor;
	
	[Attribute(defvalue: OVT_FactionType.OCCUPYING_FACTION.ToString(), UIWidgets.ComboBox, desc: "Faction type for color", "", ParamEnumArray.FromEnum(OVT_FactionType), category: "Icon")]
	protected OVT_FactionType m_FactionType;
	
	[Attribute(defvalue: "false", desc: "Show distance to location")]
	protected bool m_bShowDistance;
	
	[Attribute(defvalue: "false", desc: "Can fast travel to this location type by default")]
	bool m_bCanFastTravel;
	
	[Attribute(defvalue: "12", desc: "Icon size when zoomed out", category: "Icon")]
	protected int m_iIconSizeSmall;
	
	[Attribute(defvalue: "24", desc: "Icon size when zoomed in", category: "Icon")]
	protected int m_iIconSizeLarge;
	
	[Attribute(defvalue: "2", desc: "Zoom level to show location name (0=always, higher=closer zoom)")]
	protected float m_fShowNameZoom;
	
	[Attribute(defvalue: "true", desc: "Show location name")]
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
	//!
	//! TWO PATHS, AND THE FIRST ONE IS UNCHANGED.
	//!  1. m_InfoLayout set  -> instantiate it and call OnSetupLocationInfo. This is byte-for-byte the
	//!     behaviour that shipped, and it is the ONLY path Town, Base and RadioTower can take, because
	//!     all three set m_InfoLayout in Configs/Map/OverthrowMap.conf. They cannot reach the shared
	//!     path and therefore cannot regress from it.
	//!  2. m_InfoLayout empty, m_SharedInfoLayout set -> instantiate the shared row container and let
	//!     BuildInfoRows fill it. This is new, and only types that never had a panel at all take it.
	//! Both empty -> return, exactly as before.
	//! \param[in] location The record being described
	//! \param[in] infoPanel The panel shell's ContentSlot
	void UpdateInfoPanel(OVT_MapLocationData location, Widget infoPanel)
	{
		if (!infoPanel || !location)
			return;

		if (m_InfoLayout.IsEmpty() && m_SharedInfoLayout.IsEmpty())
			return;

		// Clear existing content in the content slot
		Widget child = infoPanel.GetChildren();
		while (child)
		{
			infoPanel.RemoveChild(child);
			child = infoPanel.GetChildren();
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (!m_InfoLayout.IsEmpty())
		{
			// Create location-specific info layout in the content slot
			Widget locationInfoWidget = workspace.CreateWidgets(m_InfoLayout, infoPanel);
			if (!locationInfoWidget)
				return;

			// Call derived class to populate location-specific data
			OnSetupLocationInfo(locationInfoWidget, location);
			return;
		}

		Widget sharedInfoWidget = workspace.CreateWidgets(m_SharedInfoLayout, infoPanel);
		if (!sharedInfoWidget)
			return;

		Widget rowsContainer = sharedInfoWidget.FindAnyWidget(ROWS_CONTAINER);
		if (!rowsContainer)
			return;

		BuildInfoRows(location, rowsContainer);

		// A type that contributed no rows gets its container taken back out, so the panel reads exactly
		// as it did before this mechanism existed instead of growing an empty padded box.
		if (!rowsContainer.GetChildren())
			infoPanel.RemoveChild(sharedInfoWidget);
	}

	//! Override this in derived classes to populate location-specific info
	protected void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		// Override in derived classes to populate location-specific info
	}

	//! Populate the shared info panel with rows for this location.
	//! Called once per panel open, never from CanFastTravel or ShouldShowLocation (both are per-element
	//! hot paths). Everything read here must tolerate partial replication - a missing row always beats
	//! a script error.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The vertical layout named ROWS_CONTAINER inside m_SharedInfoLayout
	protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		// Override in derived classes
	}

	//! Appends one label/value row to the shared info panel.
	//! Pass an empty label for a full-width explanatory line.
	//! \param[in] rows The rows container handed to BuildInfoRows
	//! \param[in] label Row caption, or "" for a full-width line
	//! \param[in] value Row value, or the whole sentence when label is empty
	protected void AddInfoRow(Widget rows, string label, string value)
	{
		AddInfoIconRow(rows, label, value, "", "");
	}

	//! Appends one row carrying a leading glyph.
	//! The glyph is only drawn when BOTH imageset and icon are non-empty; otherwise the row renders
	//! exactly as AddInfoRow would.
	//! \param[in] rows The rows container handed to BuildInfoRows
	//! \param[in] label Row caption, or "" for a full-width line
	//! \param[in] value Row value, or the whole sentence when label is empty
	//! \param[in] imageset Imageset holding the glyph
	//! \param[in] icon Quad name within imageset
	protected void AddInfoIconRow(Widget rows, string label, string value, ResourceName imageset, string icon)
	{
		if (!rows)
			return;

		// A row with neither caption nor value would render as an empty gap
		if (label.IsEmpty() && value.IsEmpty())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget rowWidget = workspace.CreateWidgets(INFO_ROW_LAYOUT, rows);
		if (!rowWidget)
			return;

		OVT_MapInfoRowHandler handler = OVT_MapInfoRowHandler.Cast(rowWidget.FindHandler(OVT_MapInfoRowHandler));
		if (!handler)
		{
			// The row layout has lost its handler. Drop the widget rather than leaving its authored
			// placeholder text ("Label" / "Value") on screen.
			rows.RemoveChild(rowWidget);
			return;
		}

		handler.Init(label, value, imageset, icon);
	}

	//! Removes every row currently in the container.
	//! Not needed by UpdateInfoPanel (which builds into a freshly created container) but required by any
	//! type that rebuilds its rows in place.
	//! \param[in] rows The rows container handed to BuildInfoRows
	protected void ClearInfoRows(Widget rows)
	{
		if (!rows)
			return;

		Widget child = rows.GetChildren();
		while (child)
		{
			rows.RemoveChild(child);
			child = rows.GetChildren();
		}
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
	
	//! Get the icon name for a specific location (can be overridden for location-specific icons)
	string GetIconName(OVT_MapLocationData location)
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
		// If using faction color, try to get it
		if (m_bUseFactionColor)
		{
			Color factionColor = GetFactionColor(m_FactionType);
			if (factionColor)
				return factionColor;
		}
		
		// Use configured default color, or fallback to black
		if (m_DefaultIconColor)
			return m_DefaultIconColor;
			
		return Color.Black; // Final fallback
	}
	
	//! Get faction color based on faction type
	protected Color GetFactionColor(OVT_FactionType factionType)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return Color.Black;
		
		Faction faction;
		switch (factionType)
		{
			case OVT_FactionType.OCCUPYING_FACTION:
				faction = config.GetOccupyingFactionData();
				break;
				
			case OVT_FactionType.RESISTANCE_FACTION:
				faction = config.GetPlayerFactionData();
				break;
				
			case OVT_FactionType.SUPPORTING_FACTION:
				faction = config.GetSupportingFactionData();
				break;
		}
		
		if (faction)
			return faction.GetFactionColor();
			
		return Color.Black;
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
		if (image)
		{
			// Load icon if imageset and icon name are available
			if (!m_IconImageset.IsEmpty())
			{
				string iconName = GetIconName(location);
				if (!iconName.IsEmpty())
				{
					image.LoadImageFromSet(0, m_IconImageset, iconName);
				}
			}
			
			// Always apply icon color regardless of whether icon was loaded
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

//! Custom title class for OVT_MapLocationType
class OVT_MapLocationTypeTitle : BaseContainerCustomTitle
{
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		string displayName;
		if (!source.Get("m_sName", displayName))
			return false;
		
		title = displayName;
		
		float visibilityZoom;
		if (source.Get("m_fVisibilityZoom", visibilityZoom) && visibilityZoom > 0)
		{
			title = title + " (zoom: " + visibilityZoom.ToString(1, 1) + ")";
		}
		
		return true;
	}
}