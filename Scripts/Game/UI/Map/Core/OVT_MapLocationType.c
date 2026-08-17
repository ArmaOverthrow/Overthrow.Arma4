//! Base class for map location types using hybrid config/code pattern
//! Hybrid config/code: a subclass carries behaviour and the .conf authors its values
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationType
{
	[Attribute(defvalue: "Location", desc: "Name for location type")]
	protected string m_sName;

	[Attribute(defvalue: "Generic Location", desc: "Display name for location type")]
	protected string m_sDisplayName;
	
	[Attribute(defvalue: "", desc: "PLURAL, localized, player-facing category name for this type's row in the map layer-filter panel, e.g. #OVT-Map_Category_Towns. A THIRD name field on purpose: m_sName is the Workbench editor-tree label and m_sDisplayName is the SINGULAR type line on the info panel. Empty falls back to the display name, then the class name, with a one-time warning.")]
	protected string m_sCategoryName;

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

	[Attribute(defvalue: "", uiwidget: UIWidgets.Object, desc: "Optional per-type faction colours. Leave UNSET to use the shared default (occupier red, resistance green) - only add one when this type's icon needs its own shade against the territory fill beneath it.", category: "Icon")]
	protected ref OVT_MapFactionPalette m_FactionColors;
	
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

	//! Whether the player has this whole category switched on in the map layer-filter panel.
	//!
	//! DELIBERATELY NOT AN [Attribute]. This is a CLIENT-SIDE PRESENTATION PREFERENCE - it is never
	//! authored in config. It is pushed in from the per-profile preference store when the map opens,
	//! and again by the filter panel whenever the player flips a row.
	//!
	//! THIS IS NOT CAMPAIGN VISIBILITY, and nothing may ever make it so. What the campaign chooses to
	//! REVEAL to a player (intel, discovery, fog of war) is a separate concept that belongs to a future
	//! epic and must have its own field: sharing this one would let a cosmetic filter decide what a
	//! player is allowed to know, and would let an intel rule silently override a player's own choice.
	//! Per-record campaign visibility already has a home - OVT_MapLocationData.m_bVisible, read by
	//! ShouldShowLocation.
	//!
	//! Read as the FIRST gate in OVT_MapLocationElement.SetVisible, which early-returns on it, so a
	//! hidden type skips the per-record zoom lookup and ShouldShowLocation's manager reads entirely and
	//! therefore costs LESS per zoom change than a shown one.
	//!
	//! NEVER RESET THIS IN Init(). Init() runs on EVERY map open rather than once, so resetting it
	//! there would silently discard the player's preference every single time they opened the map -
	//! and the symptom (filters that "don't stick") would look like a persistence bug rather than a
	//! lifecycle one. Default true, so a player who never opens the panel sees exactly today's map.
	protected bool m_bPlayerVisible = true;

	//! One-shot latch for the GetCategoryName fallback warning.
	protected bool m_bCategoryFallbackWarned;

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
	
	//! The PLURAL, player-facing category name for this type's row in the map layer-filter panel.
	//!
	//! Three name fields, three audiences: m_sName labels the Workbench editor tree, m_sDisplayName is
	//! the singular type line on the info panel, and this is the plural category a player switches on
	//! and off. Merging any two of them would drag a second surface into every change to either.
	//!
	//! THE FALLBACK CHAIN EXISTS SO A ROW ALWAYS APPEARS. Adding a type to OverthrowMap.conf must add a
	//! toggle with no code change, so a missing category name is a content gap rather than a structural
	//! one; dropping the row instead would leave a whole category of marker on screen with no way to
	//! turn it off, which is the exact inconsistency the panel exists to remove. The warning is what
	//! makes the gap loud.
	//!
	//! Not a hot path - read once per row when the panel is built.
	//! \return m_sCategoryName when set, else GetDisplayName(), else ClassName()
	string GetCategoryName()
	{
		if (!m_sCategoryName.IsEmpty())
			return m_sCategoryName;

		string fallback = GetDisplayName();
		if (fallback.IsEmpty())
			fallback = ClassName();

		WarnCategoryFallbackOnce(fallback);

		return fallback;
	}

	//! Logs the missing-category-name warning exactly once per type instance.
	//! Once, because the panel rebuilds its rows on every open: a per-read warning would be log spam
	//! rather than a diagnosis, and the condition it reports cannot change without a config edit.
	//! \param[in] fallback The label that will be shown instead
	protected void WarnCategoryFallbackOnce(string fallback)
	{
		if (m_bCategoryFallbackWarned)
			return;

		m_bCategoryFallbackWarned = true;

		Print("[Overthrow] " + ClassName() + " has no m_sCategoryName - the map layer-filter row falls back to '" + fallback + "'. Set m_sCategoryName in Configs/Map/OverthrowMap.conf to a localized plural key.", LogLevel.WARNING);
	}

	//! Switch this whole category on or off for the local player.
	//!
	//! Called by the map layer-filter panel and by the per-profile preference store it loads at map
	//! open. The caller is responsible for asking the map UI to re-run its visibility sweep
	//! (OVT_OverthrowMapUI.RefreshAllVisibility) - this setter deliberately does not reach back into
	//! the UI, because the store applies preferences to every type in a batch and one sweep afterwards
	//! is both cheaper and free of half-applied intermediate states.
	//! \param[in] visible True to draw this type's markers, false to hide them all
	void SetPlayerVisible(bool visible)
	{
		m_bPlayerVisible = visible;
	}

	//! Whether the local player has this category switched on. See m_bPlayerVisible - this is a
	//! presentation preference and is NOT campaign visibility.
	//! \return True when this type's markers may be drawn
	bool IsPlayerVisible()
	{
		return m_bPlayerVisible;
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
	
	//! THE FACTION'S OWN ENGINE COLOUR - NOT the colour the map draws it in.
	//!
	//! ⚠️ This is no longer the map's colour source. It is the raw FactionColor from the faction config,
	//! and the map now resolves by CAMPAIGN ROLE through OVT_MapFactionPalette instead, because a map
	//! whose enemy colour changes with its enemy faction is a map the player has to re-learn (US casts
	//! BLUFOR's cyan-blue as the occupier, which reads as friendly). Only OVT_MapFactionPalette's
	//! m_bUseEngineFactionColors A/B switch still reaches it. New map code wants ResolveFactionColor.
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
	//! \return The faction's engine colour, or null when the index is negative or cannot be resolved.
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

	//! Packed ARGB for a faction index at a caller-supplied alpha, through the SHARED DEFAULT palette.
	//!
	//! THIS IS THE PALETTE-LESS ENTRY POINT and it is kept only for callers that have no layer or type of
	//! their own to ask. Anything that does - every canvas layer, every location type - must go through
	//! its own OVT_MapCanvasLayer.ResolveFactionArgb / ResolveFactionColor instead, or a per-layer shade
	//! set in the conf will be silently ignored.
	//! \param[in] factionIndex The controlling faction index, or any negative value for "no faction".
	//! \param[in] alpha Alpha to pack, 0-255, clamped.
	//! \return Packed ARGB, falling back to the default palette's unknown colour (white).
	static int GetFactionArgbByIndex(int factionIndex, int alpha)
	{
		return OVT_MapFactionPalette.GetDefault().GetArgb(factionIndex, alpha);
	}

	//! This type's faction palette: its own if the conf configured one, otherwise the shared default.
	//! Never null.
	OVT_MapFactionPalette GetFactionPalette()
	{
		if (m_FactionColors)
			return m_FactionColors;

		return OVT_MapFactionPalette.GetDefault();
	}

	//! THE MARKER ENTRY POINT for "what colour is faction N", resolved by CAMPAIGN ROLE rather than by
	//! the faction's own engine colour - so a marker is red because its owner occupies, not because its
	//! owner is USSR. The town, base and radio-tower markers all resolve through here, and through the
	//! same palette the canvas layers use, so an icon cannot disagree with the territory beneath it.
	//!
	//! IT RETURNS NULL FOR AN UNROLED FACTION, preserving the contract the three overrides were written
	//! against: they do not agree on what unknown looks like (town black, base and tower white), so the
	//! fallback stays theirs rather than being picked here for everybody.
	//! \param[in] factionIndex The controlling faction index, or any negative value for "no faction".
	//! \return The role colour, or null when the index belongs to no campaign role.
	Color ResolveFactionColor(int factionIndex)
	{
		return GetFactionPalette().GetColorForFactionIndex(factionIndex);
	}

	//! Get faction color based on faction type, through this type's palette.
	protected Color GetFactionColor(OVT_FactionType factionType)
	{
		return GetFactionPalette().GetColorForRole(factionType);
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