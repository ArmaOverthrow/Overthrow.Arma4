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

	//! Seconds remaining until each opted-in location type is re-populated, keyed by type class name.
	//!
	//! Only types with m_fRefreshInterval > 0 ever appear here, and it is rebuilt empty on every map
	//! open so no timer survives a close. Keyed by name rather than held in an array parallel to
	//! m_Config.m_aLocationTypes - see TickRefresh.
	protected ref map<string, float> m_mRefreshTimers;

	//! Whether nearby recruits travel with the player. Opt-OUT: default ON, because legacy fast travel
	//! always brought them (the OVT_MapContext fast-travel branch, removed in map/legacy-retirement) and
	//! defaulting OFF would silently re-ship finding F4.
	//! Reset to true on every map open so no travel state survives a map close.
	protected bool m_bBringRecruits = true;

	//! The location whose info panel is currently built, so the recruit toggle can rebuild the travel
	//! button in place without depending on the selection still being what it was.
	protected ref OVT_MapLocationData m_PanelLocation;

	//! The info panel instance whose travel controls have already been subscribed, so re-entrant
	//! SetupTravelButton calls relabel without touching the invokers.
	protected Widget m_wWiredInfoPanel;


	//! Whether a map selection landed on top of a given widget.
	//!
	//! R6/N12: OnMapSelection's else branch treats "no hovered element" as "clicked empty space" and
	//! unpins, which is what a click on ANY panel floating over the map looks like from the map's point
	//! of view. That was invisible while every panel button closed the map; the recruit toggle must
	//! leave the panel open, and on a controller pressing MapSelect over the toggle is its ONLY input
	//! path. The map layer-filter panel inherits exactly the same hazard, which is why this was
	//! generalised out of IsSelectionOnInfoPanel rather than copied.
	//!
	//! NOT OBSERVED AT RUNTIME: whether the button widget consumes the click before the map's selection
	//! handler runs at all. If it does, every caller of this is inert - which is fine, it is cheap, and
	//! the alternative is a bug that only appears when a player happens to have a panel pinned open.
	//! \param[in] widget The widget to test against. Null, or a zero-sized rect, is never "inside".
	//! \param[in] selectionPos The selected world position.
	//! \return True when the selection's screen position is inside the widget's rect.
	protected bool IsSelectionInsideWidget(Widget widget, vector selectionPos)
	{
		if (!widget || !m_MapEntity)
			return false;

		float screenX, screenY;
		m_MapEntity.WorldToScreen(selectionPos[0], selectionPos[2], screenX, screenY, true);

		float panelX, panelY, panelW, panelH;
		widget.GetScreenPos(panelX, panelY);
		widget.GetScreenSize(panelW, panelH);

		if (panelW <= 0 || panelH <= 0)
			return false;

		if (screenX < panelX || screenX > panelX + panelW)
			return false;

		if (screenY < panelY || screenY > panelY + panelH)
			return false;

		return true;
	}

	//! Whether a map selection landed on top of the open info panel.
	//! \param[in] selectionPos The selected world position.
	//! \return True when the selection's screen position is inside the info panel's rect.
	protected bool IsSelectionOnInfoPanel(vector selectionPos)
	{
		if (!m_bInfoPanelVisible)
			return false;

		return IsSelectionInsideWidget(m_wInfoPanel, selectionPos);
	}

	//! The map layer-filter panel's widget while it is open.
	//!
	//! Resolved on demand rather than cached: this is only ever reached from a click handler, the walk
	//! is over the map entity's handful of active UI components, and resolving fresh means there is no
	//! reference to invalidate when a map closes or when a different map config (the respawn map, which
	//! carries no layers UI) is opened. OVT_MapLayersUI.GetPanelWidget already answers null while the
	//! panel is closed, so a closed panel and an absent component read the same here.
	//! \return The panel widget while it is open, null otherwise.
	protected Widget GetLayersPanelWidget()
	{
		if (!m_MapEntity)
			return null;

		OVT_MapLayersUI layersUI = OVT_MapLayersUI.Cast(m_MapEntity.GetMapUIComponent(OVT_MapLayersUI));
		if (!layersUI)
			return null;

		return layersUI.GetPanelWidget();
	}

	//! Handle map selection events for pinning
	protected void OnMapSelection(vector selectionPos)
	{
		// R6: a click that landed on one of Overthrow's own map panels is not a click on empty map - do
		// not unpin it out from under the player's own button press. Two panels can float over the map:
		// the location info panel, and the layer-filter panel whose rows a player clicks WHILE a panel
		// is pinned (K14).
		if (IsSelectionOnInfoPanel(selectionPos))
			return;

		if (IsSelectionInsideWidget(GetLayersPanelWidget(), selectionPos))
			return;

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

				// This is where a click becomes a click as far as the type contract is concerned.
				// Last, so OnLocationClicked overrides run against a panel that already exists.
				NotifyLocationClicked(m_PinnedElement);
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

	//! Tell a location's type that it was just clicked, and play the element's click sound.
	//!
	//! The single caller is OnMapSelection, because in the shipped interaction model the CONTAINER is
	//! what sees a click - it subscribes to the map entity's selection invoker. The element had its own
	//! click path (HandleSelection) that nothing ever called, which meant the OnLocationClicked virtual
	//! on the OVT_MapLocationType contract never fired and the click sound never played (BUG-137). The
	//! dead path is gone; this is its replacement, on the side of the fence that actually gets the event.
	//! \param[in] element The element that was just pinned.
	protected void NotifyLocationClicked(OVT_MapLocationElement element)
	{
		if (!element)
			return;

		OVT_MapLocationData location = element.GetLocationData();
		if (!location)
			return;

		element.PlayClickSound();

		OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
		if (locationType)
			locationType.OnLocationClicked(location, element);
	}

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		// Recruits come along by default on every fresh map open. This is not an armed mode - it cannot
		// cause a charge on its own - but resetting it here, unconditionally, keeps the "no travel state
		// survives a map close" property whole and checkable (implementation plan Sec 4.5).
		m_bBringRecruits = true;

		// Initialize arrays
		m_aLocations = new array<ref OVT_MapLocationData>;

		// Fresh timers every open, so a type is never refreshed on the strength of a countdown that
		// started in a previous map session
		m_mRefreshTimers = new map<string, float>();

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
		m_PanelLocation = null;
		m_wWiredInfoPanel = null;
		m_mRefreshTimers = null;

		// OnMouseLeave is not guaranteed to run when the map closes by keypress, by death or because
		// another menu took the screen, so the hover survives the close unless it is cleared here. On the
		// next open, OnMapSelection's `if (m_HoveredElement)` branch would pin an element belonging to the
		// PREVIOUS session - whose OVT_MapLocationData describes a world that is gone - and the travel
		// path would then price and request a trip to it. Every other selection field is already cleared
		// by ForceHideLocationInfo above; this one was the odd one out.
		m_HoveredElement = null;
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
	
	//! The configured location types, for callers that ENUMERATE them rather than look one up by name.
	//!
	//! Added for the map layer-filter panel, which builds one row per type and reads each type's live
	//! IsPlayerVisible rather than trusting a stored preference, so the panel can never disagree with
	//! the map.
	//!
	//! Returns the live array rather than a copy, because every caller only reads it. The list is
	//! config-owned: nothing outside this class may insert into or remove from it, since the marker map
	//! and the refresh timers are both keyed against the types it holds.
	//! \return The configured location types, or null when no config was supplied.
	array<ref OVT_MapLocationType> GetLocationTypes()
	{
		if (!m_Config)
			return null;

		return m_Config.m_aLocationTypes;
	}

	//! Re-run every live marker's visibility gates.
	//!
	//! Shaped deliberately identically to OnMapZoom, because it is the same sweep for a different
	//! reason: the layer-filter panel writes one boolean on a location type and then calls this, and
	//! the change lands within a frame.
	//!
	//! NOTHING IS DESTROYED, RECREATED OR REPOPULATED HERE, and that is the point. A toggle that tore
	//! elements down would drop a live hover and a pinned info panel, and would run straight into the
	//! reconciliation hazards the refresh tick already has to be careful about. This only re-asks each
	//! element whether it belongs on screen.
	void RefreshAllVisibility()
	{
		foreach (Widget widget, SCR_MapUIElement element : m_mIcons)
		{
			OVT_MapLocationElement locationElement = OVT_MapLocationElement.Cast(element);
			if (locationElement)
				locationElement.RefreshVisibility();
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

		foreach (OVT_MapLocationData location : m_aLocations)
		{
			// Find the location type for this location
			OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
			if (!locationType)
				continue;

			CreateLocationElement(location, locationType);
		}
		UpdateIcons();
	}

	//! Create one marker widget and register it in the base class icon map.
	//!
	//! Split out of CreateLocationElements so the refresh path can add a marker for a location that did
	//! not exist when the map was opened - a FOB built, a vehicle bought, a base captured mid-session
	//! (BUG-136). Everything it does was previously inline in the open-time loop, unchanged.
	//! \param[in] location The record the marker represents.
	//! \param[in] locationType The record's type.
	//! \return The attached handler, or null if the widget or its handler could not be created.
	protected OVT_MapLocationElement CreateLocationElement(OVT_MapLocationData location, OVT_MapLocationType locationType)
	{
		if (!location || !locationType || m_LocationElementLayout.IsEmpty())
			return null;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		// Create the widget from layout (handler attached via layout)
		Widget widget = workspace.CreateWidgets(m_LocationElementLayout, m_wIconsContainer);
		if (!widget)
			return null;

		// Find the handler attached to the widget
		OVT_MapLocationElement element = OVT_MapLocationElement.Cast(widget.FindHandler(OVT_MapLocationElement));
		if (!element)
		{
			widget.RemoveFromHierarchy();
			return null;
		}

		// Initialize the handler with game data
		element.SetParent(this);
		element.Init(location, locationType, this);

		// Store in base class icon map
		m_mIcons.Set(widget, element);

		// Configure widget positioning
		FrameSlot.SetSizeToContent(widget, true);
		FrameSlot.SetAlignment(widget, 0.5, 0.5);

		return element;
	}

	//! Destroy one marker widget, taking every reference to it down with it.
	//!
	//! The reason this is not just "remove from m_mIcons and RemoveFromHierarchy": four things can be
	//! pointing at an element - m_HoveredElement, m_PinnedElement, m_SelectedElement, and the STATIC
	//! SCR_MapUIElement.s_SelectedElement in the base class. BUG-135 is precisely what one surviving
	//! reference to a dead element costs, and a refresh that reconciles the element set is the one code
	//! path that can destroy an element while the map is still open, so it has to clear all four.
	//! \param[in] widget The marker's root widget, as keyed in m_mIcons.
	protected void DestroyLocationElement(Widget widget)
	{
		if (!widget)
			return;

		OVT_MapLocationElement element = OVT_MapLocationElement.Cast(m_mIcons.Get(widget));

		if (element)
		{
			// Clears the base class's static s_SelectedElement when it is this one
			element.Select(false);

			if (m_HoveredElement == element)
				m_HoveredElement = null;

			if (m_PinnedElement == element)
			{
				m_PinnedElement = null;
				m_bSelectionPinned = false;
			}

			if (m_SelectedElement == element)
				m_SelectedElement = null;
		}

		m_mIcons.Remove(widget);
		widget.RemoveFromHierarchy();
	}


	//! Per-frame tick, dispatched by SCR_MapEntity.UpdateMap for every active map UI component.
	//!
	//! Only live while the map is open, which is the whole window in which a refresh means anything.
	//! \param[in] timeSlice Seconds since the last frame.
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		TickRefresh(timeSlice);
	}

	//! Count down each opted-in type's refresh timer and refresh the ones that come due.
	//!
	//! The map used to be a frozen snapshot for the whole time it was open: PopulateAllLocations ran
	//! once in OnMapOpen and nothing ever re-read the managers, so a town changing hands, a base being
	//! captured or a FOB being built was invisible until the map was closed and reopened (BUG-136).
	//!
	//! POLLING, NOT EVENTS, AND OPT-IN PER TYPE. Event subscriptions would be cheaper at rest but need
	//! per-type wiring against seven different managers' invokers, and every one of those subscriptions
	//! is a teardown obligation on a component whose open/close is the engine's. A timer that re-runs
	//! the type's own PopulateLocations reuses the exact code path the map already trusts, and a type
	//! that leaves m_fRefreshInterval at 0 - the default, and what every type ships with today - pays
	//! nothing at all beyond this one loop.
	//!
	//! Timers are keyed by class name rather than kept in an array parallel to m_aLocationTypes on
	//! purpose; parallel arrays over map data are what BUG-068 was.
	//! \param[in] timeSlice Seconds since the last frame.
	protected void TickRefresh(float timeSlice)
	{
		if (!m_aLocations || !m_mRefreshTimers || !m_Config || !m_Config.m_aLocationTypes)
			return;

		array<OVT_MapLocationType> dueTypes = {};

		foreach (OVT_MapLocationType locationType : m_Config.m_aLocationTypes)
		{
			if (!locationType)
				continue;

			float interval = locationType.GetRefreshInterval();
			if (interval <= 0)
				continue;

			string typeName = locationType.ClassName();

			// A type seen for the first time starts a full interval away, so nothing refreshes on the
			// same frame the map opened - it was just populated.
			float remaining = interval;
			m_mRefreshTimers.Find(typeName, remaining);

			remaining -= timeSlice;

			if (remaining > 0)
			{
				m_mRefreshTimers.Set(typeName, remaining);
				continue;
			}

			m_mRefreshTimers.Set(typeName, interval);
			dueTypes.Insert(locationType);
		}

		if (dueTypes.IsEmpty())
			return;

		foreach (OVT_MapLocationType dueType : dueTypes)
		{
			RefreshLocationType(dueType);
		}

		UpdateIcons();
	}

	//! Re-populate one location type and reconcile its markers against the result.
	//!
	//! RECONCILES rather than rebuilds. Tearing the type's elements down and recreating them would be a
	//! third of the code, but it would destroy the marker under the player's cursor on a timer - killing
	//! the hover, the pin and the open info panel every interval. Matching old records to new ones by
	//! identity keeps a hovered or pinned element alive across the refresh; only genuinely gone
	//! locations lose their marker, and only genuinely new ones gain one.
	//!
	//! m_aLocations is the ownership list for the records, so this type's slice of it is swapped out
	//! wholesale. Elements that survive are re-pointed at their replacement record first, so they hold
	//! the new object by the time the old one is dropped.
	//! \param[in] locationType The type to re-read from its managers.
	protected void RefreshLocationType(OVT_MapLocationType locationType)
	{
		if (!locationType || !m_aLocations)
			return;

		string typeName = locationType.ClassName();

		array<ref OVT_MapLocationData> fresh = {};
		locationType.PopulateLocations(fresh);

		map<string, OVT_MapLocationData> freshByKey = new map<string, OVT_MapLocationData>();

		// The records that will own a marker after this refresh, in one list, so m_aLocations and the
		// element set cannot disagree about what exists.
		array<ref OVT_MapLocationData> kept = {};

		foreach (OVT_MapLocationData record : fresh)
		{
			// A type that emits records tagged as another type would corrupt that type's slice, so the
			// tag is trusted over the type that produced it.
			if (!record || record.m_sTypeName != typeName)
				continue;

			string recordKey = GetLocationKey(record);

			// One marker per identity. Two records that key the same (realistically: two positionless
			// locations rounding to the same metre) would otherwise leave the loser in m_aLocations with
			// no element - a record the map owns and never draws.
			if (freshByKey.Contains(recordKey))
				continue;

			freshByKey.Set(recordKey, record);
			kept.Insert(record);
		}

		// Pass 1: match every existing marker of this type, re-pointing survivors and listing the rest.
		array<Widget> stale = {};
		bool panelNeedsRebuild = false;

		foreach (Widget widget, SCR_MapUIElement icon : m_mIcons)
		{
			OVT_MapLocationElement element = OVT_MapLocationElement.Cast(icon);
			if (!element)
				continue;

			OVT_MapLocationData existing = element.GetLocationData();
			if (!existing || existing.m_sTypeName != typeName)
				continue;

			string key = GetLocationKey(existing);

			OVT_MapLocationData replacement;
			if (!freshByKey.Find(key, replacement))
			{
				stale.Insert(widget);
				continue;
			}

			if (m_PanelLocation == existing)
				panelNeedsRebuild = true;

			element.SetLocationData(replacement);

			// Whatever is left in the map after this pass is new and needs a marker creating
			freshByKey.Remove(key);
		}

		// Pass 2: retire the markers whose locations are gone. Done outside the m_mIcons loop above
		// because DestroyLocationElement removes from that same map.
		foreach (Widget staleWidget : stale)
		{
			OVT_MapLocationElement staleElement = OVT_MapLocationElement.Cast(m_mIcons.Get(staleWidget));
			if (staleElement && m_PanelLocation == staleElement.GetLocationData())
			{
				// The panel is describing a location that no longer exists - close it rather than leave
				// a travel button pointing at somewhere gone.
				ForceHideLocationInfo();
				panelNeedsRebuild = false;
			}

			DestroyLocationElement(staleWidget);
		}

		// m_aLocations owns the records; swap this type's slice for the fresh one.
		for (int i = m_aLocations.Count() - 1; i >= 0; i--)
		{
			OVT_MapLocationData held = m_aLocations.Get(i);
			if (!held || held.m_sTypeName == typeName)
				m_aLocations.Remove(i);
		}

		foreach (OVT_MapLocationData keep : kept)
		{
			m_aLocations.Insert(keep);
		}

		// Pass 3: anything the existing markers did not claim is a location that appeared mid-session.
		foreach (string addedKey, OVT_MapLocationData added : freshByKey)
		{
			CreateLocationElement(added, locationType);
		}

		// The panel renders from the record, so a replaced record leaves it showing stale numbers.
		// Rebuilding it is the same call hovering a marker makes, and it re-reads the new record.
		if (panelNeedsRebuild && m_SelectedElement)
			ShowLocationInfo(m_SelectedElement.GetLocationData());
	}

	//! Stable identity for one location record, used to match markers across a refresh.
	//!
	//! Prefers whichever id the type actually set - the ten shipped types are split between an integer
	//! index (towns, bases, radio towers), an EntityID (houses, ports, POIs, bus stops, vehicles) and an
	//! RplId (shops, gun dealers) - and falls back to a rounded world position for the types that set
	//! none (camps, FOBs, warehouses, waypoints). Position is the LAST resort precisely because it is
	//! the one key that a moving location would break; every type whose records move sets an EntityID.
	//! \param[in] location The record to key.
	//! \return A key unique within one location type.
	protected string GetLocationKey(OVT_MapLocationData location)
	{
		if (!location)
			return "";

		if (location.m_iID >= 0)
			return "i:" + location.m_iID.ToString();

		if (location.m_EntityID)
			return "e:" + location.m_EntityID.ToString();

		if (location.m_RplID.IsValid())
			return "r:" + location.m_RplID.AsString();

		vector pos = location.m_vPosition;
		return "p:" + Math.Round(pos[0]).ToString() + "," + Math.Round(pos[2]).ToString();
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
		m_wWiredInfoPanel = null;
		m_PanelLocation = null;
		
		// Create info panel
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wInfoPanel = workspace.CreateWidgets(m_InfoPanelLayout, m_RootWidget);
		if (!m_wInfoPanel)
			return;
		
		m_wInfoPanel.SetZOrder(20);

		// THE PANEL'S LOCATION, ASSIGNED THE MOMENT THE PANEL EXISTS. It is also assigned in
		// SetupTravelButton and that assignment stays where it is - but that method's early return is
		// travel-shaped, and OVT_RespawnMapUI overrides it without calling super, so on a map config that
		// suppresses travel affordances m_PanelLocation would never be set at all. Assigning it here is
		// what makes GetPanelLocation mean what its name says on every map config. On the living map the
		// value is identical.
		m_PanelLocation = location;

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
		SetupCloseButton();

		// Setup travel button and the recruit opt-out toggle
		SetupTravelButton(location);
		
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
		m_wWiredInfoPanel = null;
		m_PanelLocation = null;
		
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
		m_wWiredInfoPanel = null;
		m_PanelLocation = null;
		
		m_bInfoPanelVisible = false;
		
		// Notify the selected element that info popup is hidden
		if (m_SelectedElement)
			m_SelectedElement.SetInfoPopupVisible(false);
	}
	
	//! The location the info panel is currently describing, or null when no panel is shown.
	//!
	//! THE SELECTION SURFACE FOR CANVAS LAYERS, and deliberately this field rather than any of the
	//! element references beside it. m_SelectedElement is STICKY - neither HideLocationInfo nor
	//! ForceHideLocationInfo clears it - so anything driven from it outlives the panel; m_HoveredElement
	//! is null while a pin is held with the cursor elsewhere, and m_PinnedElement is null during a plain
	//! hover. m_PanelLocation is set when a panel is built, nulled by both hide paths and by map close,
	//! and re-pointed at the fresh record by the refresh reconciliation, so a reader agrees with the
	//! panel by construction instead of by parallel reasoning.
	//!
	//! It returns a RECORD, not a widget or an element, so a caller may hold nothing across frames - the
	//! refresh timer destroys and recreates elements mid-session.
	//! \return The panel's location record, or null.
	OVT_MapLocationData GetPanelLocation()
	{
		return m_PanelLocation;
	}

	//! Show and wire the info panel's close button.
	//!
	//! TWO defects made the panel undismissable, and both are fixed here (BUG-134):
	//!
	//!  1. The button did not exist. This wiring looked up "CloseButton" and
	//!     UI/Layouts/Map/Core/OVT_MapInfoPanel.layout defined no such widget, so FindAnyWidget returned
	//!     null and the guard swallowed it. The layout now carries a WLib_NavigationButtonSmall by that
	//!     name, bound to the OverthrowCloseInfoPanel action so a controller has a path to it too - the
	//!     old wiring was a bare mouse click handler, which on console is no affordance at all.
	//!  2. It was pointed at the wrong method. HideLocationInfo early-returns while m_bSelectionPinned,
	//!     and any panel a player reaches for a close button on is by definition pinned - OnMapSelection
	//!     sets the pin before showing it. So the handler would have been a no-op in exactly the state
	//!     it existed for. ForceHideLocationInfo clears the pin first, which is what an explicit close
	//!     means. HideLocationInfo's guard is deliberately left alone: it is what stops a hover-away
	//!     from tearing down a panel the player pinned on purpose.
	//!
	//! Shown on PINNED panels only. A hover panel evaporates the moment the cursor leaves the marker, so
	//! a close affordance on it would be noise - and hiding the button also retires its keybind, because
	//! SCR_InputButtonComponent.OnInput early-returns on a root that is not visible in hierarchy.
	protected void SetupCloseButton()
	{
		if (!m_wInfoPanel)
			return;

		ButtonWidget closeButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("CloseButton"));
		if (!closeButton)
			return;

		if (!m_bSelectionPinned)
		{
			closeButton.SetVisible(false);
			return;
		}

		closeButton.SetVisible(true);

		SCR_InputButtonComponent closeComp = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
		if (!closeComp)
			return;

		// ShowLocationInfo destroys and recreates the whole panel on every hover and pin, so this
		// component is a fresh instance each time and Clear() is belt-and-braces rather than load
		// bearing - unlike WireTravelHandlers, which guards a control that re-enters its own setup.
		closeComp.m_OnActivated.Clear();
		closeComp.m_OnActivated.Insert(ForceHideLocationInfo);
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
		
		// Set location type (description) - hide if same as name
		TextWidget typeText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("LocationType"));
		if (typeText)
		{
			OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
			string description;
			if (locationType)
				description = locationType.GetLocationDescription(location);
			else
				description = "Unknown";
			
			// Hide type text if it's the same as the location name
			string locationName;
			if (locationType)
				locationName = locationType.GetLocationName(location);
			else
				locationName = location.m_sName;
			
			if (description == locationName)
			{
				typeText.SetVisible(false);
			}
			else
			{
				typeText.SetVisible(true);
				typeText.SetText(description);
			}
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
	
	//! Hide every travel control on the panel. "This location type never supports travel", as distinct
	//! from "you cannot travel right now" (shown, disabled, with a reason).
	protected void HideTravelControls(Widget travelButton, Widget reasonText, Widget recruitsButton)
	{
		if (travelButton)
			travelButton.SetVisible(false);

		if (reasonText)
			reasonText.SetVisible(false);

		if (recruitsButton)
			recruitsButton.SetVisible(false);
	}

	//! Build the travel button and the recruit opt-out toggle for one location.
	//!
	//! Called once when the info panel is created, and again by the recruit toggle so BOTH labels update
	//! live. The verb is chosen at the top; Phase 4 of map/fast-travel adds the bus branch there and in
	//! the availability check below, and nothing else in this function needs to change for it.
	//! \param[in] location The location the panel is describing.
	protected void SetupTravelButton(OVT_MapLocationData location)
	{
		if (!m_wInfoPanel || !location)
			return;

		m_PanelLocation = location;

		ButtonWidget travelButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("FastTravelButton"));
		TextWidget reasonText = TextWidget.Cast(m_wInfoPanel.FindAnyWidget("FastTravelReason"));
		ButtonWidget recruitsButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("BringRecruitsButton"));

		if (!travelButton)
			return;

		OVT_MapLocationType locationType = GetLocationTypeByName(location.m_sTypeName);
		if (!locationType)
		{
			HideTravelControls(travelButton, reasonText, recruitsButton);
			return;
		}

		// The verb is DERIVED from the location type on every panel build - a cast, not a new virtual on
		// map/core's contract (implementation plan K6), and emphatically not a remembered mode.
		int verb = GetTravelVerbFor(locationType);

		string verbLabel = "#OVT-MainMenu_FastTravel";
		if (verb == OVT_TravelVerb.BUS)
			verbLabel = "#OVT-CatchBus";

		// m_bCanFastTravel gates the FAST_TRAVEL verb only. Bus stops deliberately carry
		// m_bCanFastTravel 0 - they are bus destinations, not fast-travel destinations.
		if (verb == OVT_TravelVerb.FAST_TRAVEL && !locationType.m_bCanFastTravel)
		{
			HideTravelControls(travelButton, reasonText, recruitsButton);
			return;
		}

		travelButton.SetVisible(true);

		string playerID = GetCurrentPlayerID();

		// CLIENT-ONLY actor lookup, and the only one on this path. The service owns it so the map UI
		// never grows a second local-entity resolution of its own.
		IEntity actor = OVT_FastTravelService.GetLocalActor();

		int recruitCount = 0;
		if (actor && playerID != "")
			recruitCount = OVT_FastTravelService.CountRecruitsInRadius(playerID, actor.GetOrigin());

		// The count the fare is priced at: zero when the player has opted out, the live count when not.
		// This is the number the server will find and charge for, so the button shows what will be paid.
		int effectiveRecruits = 0;
		if (m_bBringRecruits)
			effectiveRecruits = recruitCount;

		// Advisory only - the server's ValidateTravel decides.
		string reason;
		bool canTravel = EvaluateTravel(verb, locationType, location, playerID, actor, effectiveRecruits, reason);

		string cannotAffordKey = OVT_FastTravelService.ReasonKeyFor(OVT_TravelResult.CANNOT_AFFORD);

		// Tracked separately from `reason` because it is the ONE refusal the recruit toggle can undo, and
		// the toggle's enable state below depends on knowing that.
		bool refusedOnFare = !canTravel && reason == cannotAffordKey;

		// The fare the label shows AND the fare the enable state is judged against - one number, computed once.
		int cost = OVT_FastTravelService.CalculateTravelCost(verb, location.m_vPosition, actor, effectiveRecruits);

		// FAST_TRAVEL only. Its client wrapper CanGlobalFastTravel is stuck pricing a SOLO trip (K1 froze
		// its signature), so a squad fare it cannot see has to be re-tested here. The BUS verb has no such
		// gap: EvaluateTravel runs ValidateTravel directly and ValidateTravel's last rule IS affordability
		// at the full recruit-inclusive fare, so re-testing it would be the identical condition twice and
		// a second PlayerHasMoney lookup on every panel build.
		if (verb == OVT_TravelVerb.FAST_TRAVEL && canTravel && !CanAffordEffectiveFare(playerID, cost))
		{
			canTravel = false;
			reason = cannotAffordKey;
			refusedOnFare = true;
		}

		// Built AFTER the outcome is known: a refused trip must not offer a live control that changes its
		// price. The one exception is a refusal ON that price - switching recruits off is exactly the
		// player's remedy, and disabling the toggle there would be a dead end with no way out but closing
		// the panel.
		bool toggleInteractive = canTravel || refusedOnFare;
		SetupRecruitToggle(recruitsButton, verb, location, actor, recruitCount, toggleInteractive);

		travelButton.SetEnabled(canTravel);

		if (reasonText)
		{
			if (canTravel)
			{
				reasonText.SetVisible(false);
			}
			else
			{
				reasonText.SetText(reason);
				reasonText.SetVisible(true);
			}
		}

		SCR_InputButtonComponent buttonComp = SCR_InputButtonComponent.Cast(travelButton.FindHandler(SCR_InputButtonComponent));
		if (buttonComp)
		{
			// SCR_InputButtonComponent.SetLabel is a bare TextWidget.SetText, so the fare has to be woven
			// into the string BEFORE it gets there. WidgetManager.Translate is the vanilla way to do that
			// for a plain-string sink (SCR_BaseRadialCommand.SetCannotPerformReason does the same), and it
			// keeps the word order in the string table where a translator can reach it instead of hard
			// coding "<verb> ($<n>)" in EnforceScript.
			string buttonText = WidgetManager.Translate(verbLabel);
			if (cost > 0)
				buttonText = WidgetManager.Translate(OVT_FastTravelService.FARE_LABEL_FORMAT, buttonText, cost.ToString());

			buttonComp.SetLabel(buttonText);

			WireTravelHandlers(buttonComp, recruitsButton);
		}
	}

	//! Advisory affordability re-check at the fare the button actually displays.
	//!
	//! It exists because the shared client wrapper CanGlobalFastTravel cannot express a recruit count: K1
	//! locked its signature so map/location-types' five per-type CanFastTravel overrides compile untouched,
	//! so its affordability term always prices a SOLO trip. Without this, a player who can afford the solo
	//! fare but not the squad fare would see an ENABLED button, click it, watch the map close, and only
	//! then be refused by the server with #OVT-CannotAfford - a refusal that arrives after the commitment.
	//!
	//! CALLED FOR THE FAST_TRAVEL VERB ONLY, and the caller is where that is enforced. The BUS verb does
	//! not go through CanGlobalFastTravel at all - EvaluateTravel hands it straight to ValidateTravel,
	//! whose final rule is already affordability at the full recruit-inclusive fare - so calling this for a
	//! bus would re-test an identical condition and buy a second PlayerHasMoney lookup for nothing.
	//!
	//! Client-side advisory only. The server's ValidateTravel remains the authority and is unchanged.
	//! \param[in] playerID Persistent id of the local player, may be "".
	//! \param[in] cost The effective fare, recruits included, as shown on the button.
	//! \return True when the fare is free, unknowable, or the player can pay it.
	protected bool CanAffordEffectiveFare(string playerID, int cost)
	{
		if (cost <= 0 || playerID == "")
			return true;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return true;

		return economy.PlayerHasMoney(playerID, cost);
	}

	//! Which travel verb a location type is travelled to by.
	//!
	//! THERE IS NO BUS MODE. Eligibility for the bus is derived here, from the type of the location the
	//! panel is describing, every single time the panel is built - and the origin check is derived from
	//! the player's LIVE position inside ValidateTravel. BUG-069 part 2 was an m_bBusTravelActive flag
	//! surviving an engine-side map close and silently charging a fare on the next click; that defect
	//! cannot recur here by construction, because there is no flag to leave set (implementation plan K5).
	//! \param[in] locationType The type of the location being described.
	//! \return OVT_TravelVerb.BUS for a bus stop, OVT_TravelVerb.FAST_TRAVEL otherwise.
	protected int GetTravelVerbFor(OVT_MapLocationType locationType)
	{
		if (OVT_MapLocationBusStop.Cast(locationType))
			return OVT_TravelVerb.BUS;

		return OVT_TravelVerb.FAST_TRAVEL;
	}

	//! Advisory availability for one verb, used only to enable/disable the button and label the refusal.
	//!
	//! Fast travel keeps delegating to the per-type CanFastTravel override (unchanged). The bus verb runs
	//! the shared rule table directly, so the panel and the server refuse for the same reasons in the same
	//! words - no wanted, QRF or minimum-distance rule applies to a bus (N6).
	//! \param[in] verb OVT_TravelVerb the button is offering.
	//! \param[in] locationType The location's type.
	//! \param[in] location The location the panel is describing.
	//! \param[in] playerID Persistent id of the local player, may be "".
	//! \param[in] actor The local controlled entity, may be null.
	//! \param[in] effectiveRecruits Recruits the fare is priced for.
	//! \param[out] reason Localization key explaining a refusal.
	//! \return True when the trip looks allowed from here.
	protected bool EvaluateTravel(int verb, OVT_MapLocationType locationType, OVT_MapLocationData location, string playerID, IEntity actor, int effectiveRecruits, out string reason)
	{
		if (verb != OVT_TravelVerb.BUS)
			return locationType.CanFastTravel(location, playerID, reason);

		int result = OVT_TravelResult.NOT_ALLOWED;

		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (playerManager && playerID != "")
		{
			int intPlayerID = playerManager.GetPlayerIDFromPersistentID(playerID);
			if (intPlayerID >= 0)
				result = OVT_FastTravelService.ValidateTravel(OVT_TravelVerb.BUS, location.m_vPosition, intPlayerID, actor, effectiveRecruits);
		}

		reason = OVT_FastTravelService.ReasonKeyFor(result);
		return result == OVT_TravelResult.OK;
	}

	//! Show, label and enable the recruit opt-out toggle.
	//!
	//! Hidden entirely when there are no recruits in range - no dead control. Hiding also kills the
	//! keybind, because SCR_InputButtonComponent.OnInput early-returns on an invisible root; that is the
	//! behaviour we want, not a side effect to work around.
	//!
	//! When travel is refused the toggle stays VISIBLE but goes non-interactive: the fare it names is
	//! still worth reading (it explains what the party would cost), while a live control that adjusts the
	//! price of a trip that cannot happen is not. SetEnabled(false) is what disables it rather than
	//! SetVisible(false) for exactly that reason, and it kills BOTH input paths at once - the same
	//! IsEnabledInHierarchy test guards OnInput (the keybind, SCR_InputButtonComponent.c:731) and OnClick
	//! (the mouse, :247).
	//! \param[in] recruitsButton The toggle widget, may be null on a stale layout.
	//! \param[in] verb OVT_TravelVerb the fare is priced for.
	//! \param[in] location The location the panel is describing.
	//! \param[in] actor The local controlled entity, may be null.
	//! \param[in] recruitCount How many recruits are actually within travel radius.
	//! \param[in] interactive False to show the toggle but refuse both its click and its keybind.
	protected void SetupRecruitToggle(ButtonWidget recruitsButton, int verb, OVT_MapLocationData location, IEntity actor, int recruitCount, bool interactive)
	{
		if (!recruitsButton)
			return;

		if (recruitCount <= 0)
		{
			recruitsButton.SetVisible(false);
			return;
		}

		recruitsButton.SetVisible(true);
		recruitsButton.SetEnabled(interactive);

		SCR_InputButtonComponent toggleComp = SCR_InputButtonComponent.Cast(recruitsButton.FindHandler(SCR_InputButtonComponent));
		if (!toggleComp)
			return;

		// Resolved here rather than concatenated onto a "#OVT-" key, so the count and the surcharge sit in
		// placeholders a translator can move. SetLabel is a bare TextWidget.SetText and does no
		// substitution of its own, so the finished string has to arrive already formatted.
		string label;
		if (m_bBringRecruits)
		{
			// The extra is exactly recruitCount solo fares - ComputeFare multiplies a rounded fare by
			// (1 + N), so there is no rounding drift to account for here.
			int soloCost = OVT_FastTravelService.CalculateTravelCost(verb, location.m_vPosition, actor, 0);
			int extra = soloCost * recruitCount;
			label = WidgetManager.Translate("#OVT-Map_BringRecruits", recruitCount.ToString(), extra.ToString());
		}
		else
		{
			label = WidgetManager.Translate("#OVT-Map_LeaveRecruits", recruitCount.ToString());
		}

		toggleComp.SetLabel(label);
	}

	//! Subscribe the travel button and the recruit toggle, exactly once per info panel instance.
	//!
	//! Clear()-then-Insert() is the anti-double-fire guard: the panel is destroyed and rebuilt on every
	//! hover/pin, so without it the handlers would accumulate. It runs once per panel rather than on
	//! every SetupTravelButton call because the recruit toggle's own handler re-enters
	//! SetupTravelButton, and clearing a ScriptInvoker from inside its own Invoke is not something the
	//! engine documents as safe.
	//! \param[in] travelComp The travel button's input component.
	//! \param[in] recruitsButton The toggle widget, may be null.
	protected void WireTravelHandlers(SCR_InputButtonComponent travelComp, ButtonWidget recruitsButton)
	{
		if (m_wWiredInfoPanel == m_wInfoPanel)
			return;

		m_wWiredInfoPanel = m_wInfoPanel;

		if (travelComp)
		{
			travelComp.m_OnActivated.Clear();
			travelComp.m_OnActivated.Insert(OnTravelClicked);
		}

		if (!recruitsButton)
			return;

		SCR_InputButtonComponent toggleComp = SCR_InputButtonComponent.Cast(recruitsButton.FindHandler(SCR_InputButtonComponent));
		if (toggleComp)
		{
			toggleComp.m_OnActivated.Clear();
			toggleComp.m_OnActivated.Insert(OnToggleRecruitsClicked);
		}
	}

	//! Flip the recruit opt-out and rebuild the travel controls in place.
	//!
	//! Deliberately does NOT close the map or hide the panel - the player is still choosing. That is what
	//! makes R6's OnMapSelection guard load-bearing rather than theoretical.
	protected void OnToggleRecruitsClicked()
	{
		m_bBringRecruits = !m_bBringRecruits;

		if (m_PanelLocation)
			SetupTravelButton(m_PanelLocation);
	}

	//! Handle travel button click
	//!
	//! Requests the trip; it does not perform it. Validation, payment and the teleport all happen on
	//! the server in OVT_TravelRequestComponent (map/fast-travel Phase 2) - nothing is teleported or
	//! debited here, which is what closes findings F1 and F2. The button state that got us here is
	//! advisory only, so the server may still refuse after the map has closed; it reports the reason
	//! back through RpcDo_TravelResult and the player gets a hint.
	protected void OnTravelClicked()
	{
		// Use the currently selected element's location data
		if (!m_SelectedElement)
			return;

		OVT_MapLocationData location = m_SelectedElement.GetLocationData();
		if (!location)
			return;

		// Re-derived from the clicked location, exactly as the button's label was. Nothing about the verb
		// is stored between the panel being built and the click landing.
		int verb = GetTravelVerbFor(GetLocationTypeByName(location.m_sTypeName));

		// Hide info panel and close map first
		HideLocationInfo();
		HideMap();

		OVT_TravelRequestComponent travel = OVT_Global.GetTravelRequests();
		if (!travel)
		{
			// R2: a component that exists in script but not on OVT_OverthrowController.et is a silent
			// no-op - the request never leaves this machine. Say so rather than doing nothing.
			Print("[Overthrow] No OVT_TravelRequestComponent on the local controller - travel request dropped", LogLevel.ERROR);
			return;
		}

		// The player's own opt-out choice, defaulted ON in OnMapOpen. SetupTravelButton prices the button
		// at the same effective recruit count, so the number on the button is the number the server
		// charges - which is what closed Phase 2's interim gap (the fare was hardcoded to bring recruits
		// while the panel still showed the solo fare).
		travel.RequestTravel(verb, location.m_vPosition, m_bBringRecruits);
	}

	//! Update info panel position
	protected void UpdateInfoPanelPosition()
	{
		if (!m_wInfoPanel || !m_SelectedElement)
			return;
		
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		// Gap kept between the panel and any screen edge it is pushed off
		const float SCREEN_MARGIN = 10;

		// Position panel near selected element but not overlapping
		vector pos = m_SelectedElement.GetPos();
		float x, y;
		m_MapEntity.WorldToScreen(pos[0], pos[2], x, y, true);
		x = workspace.DPIUnscale(x);
		y = workspace.DPIUnscale(y);

		// Offset to avoid overlapping with icon
		x += 13;
		y -= 31;

		// Keep within screen bounds
		float panelWidth, panelHeight;
		m_wInfoPanel.GetScreenSize(panelWidth, panelHeight);

		float screenWidth, screenHeight;
		workspace.GetScreenSize(screenWidth, screenHeight);

		// Widget.GetScreenSize answers in PHYSICAL pixels, but x and y were DPI-unscaled into reference
		// units above and FrameSlot.SetPos wants reference units. Both sizes therefore have to come back
		// to reference units before anything can be compared against x/y. The clamp below used them raw,
		// which only happens to be right at DPI scale 1.0.
		panelWidth = workspace.DPIUnscale(panelWidth);
		panelHeight = workspace.DPIUnscale(panelHeight);
		screenWidth = workspace.DPIUnscale(screenWidth);
		screenHeight = workspace.DPIUnscale(screenHeight);

		if (x + panelWidth > screenWidth)
			x = screenWidth - panelWidth - SCREEN_MARGIN;

		// The BOTTOM edge was never clamped - only the right edge and the top. A marker low on the
		// screen therefore got a panel that ran off the bottom of the display, taking its last rows with
		// it, and the taller the panel the more it lost. Shops and gun dealers show it first because
		// their panels are the tallest.
		if (y + panelHeight > screenHeight)
			y = screenHeight - panelHeight - SCREEN_MARGIN;

		// Last, so that a panel too tall to fit at all is pinned to the top and loses its bottom rather
		// than its header - the header is what identifies which location you are looking at.
		if (y < 0)
			y = SCREEN_MARGIN;

		if (x < 0)
			x = SCREEN_MARGIN;

		FrameSlot.SetPos(m_wInfoPanel, x, y);
	}
	
	//! Get current player ID as string
	//! CLIENT-ONLY. Resolved from the local runtime player id rather than from a controlled entity, so it
	//! still answers for a dead player with no character.
	protected string GetCurrentPlayerID()
	{
		return OVT_Global.GetLocalPersistentId();
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
	
	//! Hide the map AND stow the map item.
	//!
	//! Root cause of the "still holding the map after fast travel" defect: Overthrow closed the map by
	//! calling SCR_MapGadgetComponent.SetMapMode(false) directly, which only toggles the map *view*.
	//! Vanilla works the other way round - SCR_GadgetManagerComponent.HandleInput stows the gadget
	//! (SetGadgetMode(..., IN_STORAGE)) and the view closing is a *consequence* of that. Closing the view
	//! on its own therefore left the gadget in hand (and the player controller still flagged gadget-focused).
	//!
	//! Ordering matters. SetGadgetMode's hand->storage branch returns early for EGadgetType.MAP (a vanilla
	//! hotfix against input spamming), so it never closes the view itself - the view would only close later,
	//! when the stow animation completes and ModeClear(IN_HAND) fires ToggleFocused(false). So we clear focus
	//! FIRST (same immediate close timing as before this fix, and it clears m_bFocused so that later ModeClear
	//! does not schedule a second close) and THEN stow. ToggleFocused(false) calls SetMapMode(false) itself,
	//! so no separate SetMapMode call is needed.
	protected void HideMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;

		// Closes the map view (via SetMapMode(false)) and clears the player controller's gadget focus
		comp.ToggleFocused(false);

		StowMapGadget(comp);
	}

	//! Put the map gadget away, but only if it is the gadget actually held in hand
	//!
	//! No-ops when there is no local character, no gadget manager, nothing held, something else is held,
	//! or the character is dead - we never want to stow whatever the player happens to be holding.
	protected void StowMapGadget(SCR_MapGadgetComponent comp)
	{
		if(!comp) return;

		IEntity gadget = comp.GetOwner();
		if(!gadget) return;

		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if(!character) return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if(controller && controller.IsDead()) return;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if(!gadgetManager) return;

		if(gadgetManager.GetHeldGadget() != gadget) return;

		gadgetManager.SetGadgetMode(gadget, EGadgetMode.IN_STORAGE);
	}
	
}