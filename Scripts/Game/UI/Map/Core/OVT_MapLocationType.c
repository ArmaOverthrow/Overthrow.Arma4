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

	//! Name of the SizeLayoutWidget wrapping the icon in the element layout.
	//! LAYOUT <-> CODE CONTRACT: UI/Layouts/Map/Core/OVT_MapLocationElement.layout must contain a
	//! SizeLayoutWidget with exactly this name, or zoom-based icon sizing silently does nothing.
	//! This was "IconLayout" - a name no layout has ever defined - which is what made
	//! m_iIconSizeSmall/m_iIconSizeLarge inert on all ten configured types (BUG-133).
	protected static const string ICON_CONTAINER = "IconContainer";

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
	
	[Attribute(defvalue: "false", desc: "Draw ONLY records this player may respawn at (respawn screen). Leave false on the living map - the default is what keeps every existing config entry behaving exactly as it does now.")]
	protected bool m_bRespawnOnly;

	[Attribute(defvalue: "12", desc: "Icon size when zoomed out", category: "Icon")]
	protected int m_iIconSizeSmall;
	
	[Attribute(defvalue: "24", desc: "Icon size when zoomed in", category: "Icon")]
	protected int m_iIconSizeLarge;
	
	[Attribute(defvalue: "2", desc: "Zoom level to show location name (0=always, higher=closer zoom)")]
	protected float m_fShowNameZoom;
	
	[Attribute(defvalue: "true", desc: "Show location name")]
	protected bool m_bShowName;

	[Attribute(defvalue: "0", desc: "Seconds between live refreshes of this type while the map is open. 0 = populate once per map open (no refresh). Re-runs PopulateLocations, so keep it well above the cost of that call.")]
	protected float m_fRefreshInterval;

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
	
	//! Check if this specific location may be respawned at by this player.
	//!
	//! DEFAULTS TO REFUSE. Only the four types that override this are respawnable, and a type that
	//! forgets to override it draws nothing on the respawn map rather than offering a spawn point
	//! nobody validated. Overriding it is the whole opt-in.
	//!
	//! HOT PATH. Once m_bRespawnOnly is set, ShouldShowLocation calls this per element on every zoom
	//! change, exactly as CanFastTravel is called per panel interaction. Map lookups on the record's
	//! own data and comparisons only: no allocation, no manager walk, no entity resolution. Anything
	//! expensive belongs in PopulateLocations, which runs once per map open.
	//!
	//! ADVISORY ONLY, like CanFastTravel. It decides what the client draws; the server re-derives the
	//! eligible set from its own managers in OVT_RespawnService.CollectEligiblePositions and that is
	//! what decides where anybody actually spawns.
	//! \param[in] location The record being tested
	//! \param[in] playerID Persistent id of the local player. Empty means unresolved - refuse.
	//! \param[out] reason Localization key explaining a refusal
	//! \return True when this player may respawn at this location
	bool CanRespawn(OVT_MapLocationData location, string playerID, out string reason)
	{
		reason = "#OVT-Respawn_NotEligible";
		return false;
	}

	//! Handle location selection (when clicked but not activated)
	void OnLocationSelected(OVT_MapLocationData location, OVT_MapLocationElement element)
	{
		// Override in derived classes for custom selection behavior
	}
	
	//! Handle location click/activation
	//!
	//! REACHABLE AS OF BUG-137. It is invoked by OVT_OverthrowMapUI.OnMapSelection at the moment a
	//! click PINS a location. It used to be called only from OVT_MapLocationElement.HandleSelection(),
	//! which itself had no callers, so the virtual never fired at all - a documented extension point
	//! that quietly discarded any override written against it. That dead method is gone; the container
	//! now makes the call, because in the shipped interaction model (hover shows, click pins,
	//! click-empty dismisses) the container is what sees the click.
	//!
	//! The default body is deliberately EMPTY. It used to call ShowLocationInfo, which is now redundant:
	//! by the time this runs the container has already selected the element and built the panel, so a
	//! panel-showing default would build it a second time. Override for per-type click behaviour only.
	//! \param[in] location The record that was just pinned
	//! \param[in] element The element that was just pinned
	void OnLocationClicked(OVT_MapLocationData location, OVT_MapLocationElement element)
	{
		// Override in derived classes
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
	//!
	//! HOT PATH - OVT_MapLocationElement.SetVisible calls this per element on every zoom change.
	//! The respawn gate is one boolean test on a config attribute, taken only when the config asks
	//! for it, so the living map pays a compare and nothing else.
	//! \param[in] location The record being tested
	//! \param[in] playerID Persistent id of the local player
	//! \return True when the element should be drawn
	bool ShouldShowLocation(OVT_MapLocationData location, string playerID)
	{
		if (!location.m_bVisible)
			return false;

		if (!m_bRespawnOnly)
			return true;

		string reason;
		return CanRespawn(location, playerID, reason);
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

	//! Seconds between live refreshes of this type while the map is open, 0 when it never refreshes.
	//!
	//! Opt-in per type, and off by default, because a refresh re-runs PopulateLocations - the exact
	//! per-open cost the map pays once - on a timer. Only turn it on for types whose staleness is
	//! visible to the player and whose population is cheap; a type whose records cannot change during
	//! one map session should stay at 0. See OVT_OverthrowMapUI.TickRefresh (BUG-136).
	//! \return Refresh period in seconds, or 0 for "populate once per map open".
	float GetRefreshInterval()
	{
		return m_fRefreshInterval;
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
	
	//! THE ONE IMPLEMENTATION of "what colour is faction N". Every live-faction map colour in Overthrow
	//! resolves through here: the town, base and radio-tower marker overrides, and the territory
	//! overlay's cell fill. One function is the only way to guarantee the overlay and the markers keep
	//! agreeing as either side changes - a matching constant in two places is agreement by coincidence.
	//!
	//! It takes an INDEX rather than a record because that is what all four site types can supply,
	//! including FOBs, which carry no faction field of their own and hand in the resistance faction
	//! index instead.
	//!
	//! IT RETURNS NULL RATHER THAN A FALLBACK COLOUR, DELIBERATELY. The three marker overrides do not
	//! agree on what "unknown faction" looks like - the town marker falls back to black, the base and
	//! radio-tower markers to white - so baking any single fallback in here would silently restyle
	//! markers. The unresolved case stays the caller's decision.
	//! \param[in] factionIndex Faction index, or -1 / any negative value for "no controlling faction".
	//! \return The faction's colour, or null when the index is negative or cannot be resolved.
	static Color GetFactionColorByIndex(int factionIndex)
	{
		if (factionIndex < 0)
			return null;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return null;

		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return null;

		return faction.GetFactionColor();
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
		SizeLayoutWidget sizeLayout = SizeLayoutWidget.Cast(iconWidget.FindAnyWidget(ICON_CONTAINER));
		if (sizeLayout)
		{
			int size;
			if (isSmall)
				size = m_iIconSizeSmall;
			else
				size = m_iIconSizeLarge;

			// IconContainer sits in a LayoutSlot - its parent is the element's ContentLayout, a
			// VerticalLayoutWidget - so FrameSlot.SetSize could never have sized it even under the
			// right name. SizeLayoutWidget's own overrides are the supported route. The Enable calls
			// are explicit so the resize does not silently depend on the layout keeping
			// AllowWidthOverride/AllowHeightOverride set.
			sizeLayout.EnableWidthOverride(true);
			sizeLayout.EnableHeightOverride(true);
			sizeLayout.SetWidthOverride(size);
			sizeLayout.SetHeightOverride(size);

			// A SizeLayoutWidget override only sets the DESIRED size; a parent that stretches the child
			// wins. In a VerticalLayoutWidget the vertical axis is the main axis (SizeMode Auto, so the
			// height override is honoured) but the HORIZONTAL axis is the cross axis, and it stretches
			// by default. So the height shrank and the width did not, and every icon came out flattened
			// - 32x24 zoomed in, 32x12 zoomed out. The layout now authors HorizontalAlign Center on this
			// slot; this call is the belt to that braces, because a Workbench re-save can re-emit slot
			// defaults and a layout regression here is invisible to tools/compile-check.sh.
			AlignableSlot.SetHorizontalAlign(sizeLayout, LayoutHorizontalAlign.Center);
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
	//! CLIENT-ONLY, like every other map-UI path. Resolved from the local runtime player id rather than
	//! from a controlled entity, so it still answers for a dead player with no character.
	protected string GetCurrentPlayerID()
	{
		return OVT_Global.GetLocalPersistentId();
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