//! Individual interactive map element for Overthrow locations
//! Extends base game SCR_MapUIElement for click detection and interaction
[BaseContainerProps()]
class OVT_MapLocationElement : SCR_MapUIElement
{
	//! The location data this element represents
	protected ref OVT_MapLocationData m_LocationData;
	
	//! The location type handler for this element
	protected ref OVT_MapLocationType m_LocationType;
	
	//! Reference to the parent map UI
	protected OVT_OverthrowMapUI m_ParentMapUI;
	
	//! Reference to the map entity for zoom level access
	protected SCR_MapEntity m_MapEntity;
	
	
	//! Cached widget references for performance
	protected SizeLayoutWidget m_wIconContainer;
	protected ImageWidget m_wIcon;
	protected TextWidget m_wDistance;
	protected TextWidget m_wLocationName;
	protected Widget m_wSelectionHighlight;
	protected Widget m_wFastTravelIndicator;
	protected Widget m_wHoverOverlay;
	protected ImageWidget m_wBackgroundGradient;
	protected Widget m_wHighlight;
	
	//! State tracking
	protected bool m_bSelected = false;
	protected bool m_bIsHovered = false;
	protected bool m_bInfoPopupVisible = false;
	
	
	//! Sound attributes
	//! Played by the container when a click pins this element - see PlayClickSound. It used to be
	//! consumed only by HandleSelection(), which had no callers, so it never actually played (BUG-137).
	[Attribute(SCR_SoundEvent.SOUND_MAP_HOVER_BASE, desc: "Sound played on click")]
	protected string m_sSoundClick;
	
	//! Set parent container (required by base class)
	override void SetParent(SCR_MapUIElementContainer parent)
	{
		m_Parent = parent;
		m_ParentMapUI = OVT_OverthrowMapUI.Cast(parent);
	}
	
	//! Initialize the element with location data and type
	void Init(OVT_MapLocationData locationData, OVT_MapLocationType locationType, OVT_OverthrowMapUI parentMapUI)
	{
		m_LocationData = locationData;
		m_LocationType = locationType;
		m_ParentMapUI = parentMapUI;
		m_MapEntity = SCR_MapEntity.GetMapInstance();
		m_bVisible = true;
		
		// Now that we have location data, update the display
		UpdateDisplay();
	}
	
	//! Get location data for external access
	OVT_MapLocationData GetLocationData()
	{
		return m_LocationData;
	}
	
	//! Get the world position of this element
	override vector GetPos()
	{
		if (m_LocationData)
			return m_LocationData.m_vPosition;
		return vector.Zero;
	}
	
	//! Handle widget attachment and cache references
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		if (!w)
			return;
		
		// Cache widget references for performance
		m_wIconContainer = SizeLayoutWidget.Cast(w.FindAnyWidget("IconContainer"));
		m_wIcon = ImageWidget.Cast(w.FindAnyWidget("Icon"));
		m_wDistance = TextWidget.Cast(w.FindAnyWidget("Distance"));
		m_wLocationName = TextWidget.Cast(w.FindAnyWidget("LocationName"));
		m_wSelectionHighlight = w.FindAnyWidget("SelectionHighlight");
		m_wFastTravelIndicator = w.FindAnyWidget("FastTravelIndicator");
		m_wHoverOverlay = w.FindAnyWidget("HoverOverlay");
		m_wBackgroundGradient = ImageWidget.Cast(w.FindAnyWidget("BackgroundGradient"));
		m_wHighlight = w.FindAnyWidget("Highlight");
		
	}
	
	//! Handle widget detachment and cleanup
	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);
	}
	
	
	
	
	//! Play this element's click sound.
	//!
	//! Exists because PlayHoverSound is protected on SCR_MapUIElement and the caller is the container:
	//! clicks are handled one level up, by OVT_OverthrowMapUI.OnMapSelection subscribing to the map
	//! entity's selection invoker, not by this element. The element's own click path (HandleSelection,
	//! and GetClickRadius as its hit test) was written, never wired to anything, and removed in BUG-137.
	//!
	//! There is deliberately no click-to-deselect. HandleSelection implemented one, but in the shipped
	//! model hover ALREADY shows the panel, so unpinning under a stationary cursor would leave the panel
	//! showing anyway. The explicit dismissal is the panel's own close button (BUG-134).
	void PlayClickSound()
	{
		PlayHoverSound(m_sSoundClick);
	}

	//! Handle mouse hover enter (works for both mouse and controller)
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (!m_bVisible || !m_LocationData || !m_LocationType)
			return false;

		// Already hovered: the container's hover magnet and the real widget event both route through
		// here, and whichever arrives second must not unpin, re-show the panel or replay the sound
		if (m_bIsHovered)
			return false;


		// Show info panel like campaign map does
		if (m_ParentMapUI)
		{
			// Hovering a new location unpins any previously pinned location
			m_ParentMapUI.UnpinOnHover();
			
			m_ParentMapUI.SetHoveredElement(this);
			m_ParentMapUI.SelectLocation(this);
			m_ParentMapUI.ShowLocationInfo(m_LocationData);
		}
		
		// Show hover effects
		m_bIsHovered = true;
		ShowHoverEffects(true);
		PlayHoverSound(m_sSoundHover);
				
		return false;
	}
	
	//! Handle mouse hover leave (works for both mouse and controller)
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (!m_bVisible)
			return false;

		// The widget event fires the moment the cursor exits the icon box, but while the container's
		// hover magnet still claims this element the hover must survive - the magnet's release (which
		// re-enters here with the claim already reassigned) is what actually ends it
		if (m_ParentMapUI && m_ParentMapUI.GetMagnetElement() == this)
			return false;

		// Clear hover state and hide info panel when leaving - but only while this element still OWNS
		// the container hover: a stale leave arriving after another element already took the hover
		// must not stomp the newer element's panel
		if (m_ParentMapUI && m_ParentMapUI.GetHoveredElement() == this)
		{
			m_ParentMapUI.SetHoveredElement(null);
			m_ParentMapUI.HideLocationInfo();
		}

		// Hide hover effects
		m_bIsHovered = false;
		ShowHoverEffects(false);

		return false;
	}
	
	//! Set selection state
	void SetSelected(bool selected)
	{
		Select(selected);
	}
	
	//! Select this element with proper static tracking
	override void Select(bool select = true)
	{
		// Handle static selection tracking
		if (select && s_SelectedElement)
		{
			SCR_MapUIElement otherElement = SCR_MapUIElement.Cast(s_SelectedElement);
			if (otherElement && otherElement != this)
				otherElement.Select(false);
		}
		
		if (m_bSelected == select)
			return;
		
		m_bSelected = select;
		
		if (select)
		{
			s_SelectedElement = this;
			AnimExpand();
		}
		else
		{
			if (s_SelectedElement == this)
				s_SelectedElement = null;
			AnimCollapse();
		}
		
		UpdateSelection();
		
		// Update name and distance visibility when selection state changes
		UpdateLocationName();
		UpdateDistance();
		
		// Notify location type of selection change
		if (m_LocationType && select)
			m_LocationType.OnLocationSelected(m_LocationData, this);
	}
	
	//! Get selection state
	bool IsSelected()
	{
		return m_bSelected;
	}
	
	
	//! Get the location type
	OVT_MapLocationType GetLocationType()
	{
		return m_LocationType;
	}
	
	//! Unified display update method
	void UpdateDisplay()
	{
		UpdateIcon();
		UpdateSelection();
		UpdateFastTravelIndicator();
		UpdateLocationName();
		UpdateDistance();
	}
	
	//! Update the icon display based on current state
	void UpdateIcon()
	{
		if (!m_LocationType || !m_LocationData || !m_wRoot)
			return;
		
		// Let the location type handle icon setup
		m_LocationType.SetupIconWidget(m_wRoot, m_LocationData, ShouldUseSmallIcon());
	}
	
	//! Update selection highlight
	protected void UpdateSelection()
	{
		if (m_wSelectionHighlight)
			m_wSelectionHighlight.SetVisible(m_bSelected);
		
		if (m_wBackgroundGradient)
			m_wBackgroundGradient.SetVisible(m_bSelected);
	}
	
	//! Animate expansion when selected
	protected override void AnimExpand()
	{
		
		if (m_wSelectionHighlight)
			AnimateWidget.Opacity(m_wSelectionHighlight, 1.0, ANIM_SPEED);
		
		// Scale up animation could be added here in the future
	}
	
	//! Animate collapse when deselected
	protected override void AnimCollapse()
	{
		
		if (m_wSelectionHighlight)
			AnimateWidget.Opacity(m_wSelectionHighlight, 0.0, ANIM_SPEED);
		
		// Scale down animation could be added here in the future
	}
	
	//! Update fast travel indicator
	protected void UpdateFastTravelIndicator()
	{
		if (!m_wFastTravelIndicator || !m_LocationData || !m_LocationType)
			return;
		
		// Check if fast travel is available for this location
		string playerID = GetCurrentPlayerID();
		string reason;
		bool canFastTravel = m_LocationType.CanFastTravel(m_LocationData, playerID, reason);
		
		m_wFastTravelIndicator.SetVisible(canFastTravel);
		m_LocationData.m_bCanFastTravel = canFastTravel;
	}
	
	//! Zoom threshold for THIS record: the per-record OVT_MapDataKeys.VISIBILITY_ZOOM override when the
	//! record writes one, otherwise the type's m_fVisibilityZoom (BUG-138).
	//!
	//! HOT PATH - runs per element on every zoom change, from both ShouldUseSmallIcon and SetVisible.
	//! Keep it a map lookup and a compare: no manager access, no allocation.
	//!
	//! The sentinel is NEGATIVE because 0 is a legitimate threshold meaning "always visible" (Town, Base
	//! and RadioTower all ship 0), so 0 cannot be used to mean "no override".
	//! \return The visibility zoom this element should compare the current zoom against
	protected float GetEffectiveVisibilityZoom()
	{
		if (m_LocationData)
		{
			float recordZoom = m_LocationData.GetDataFloat(OVT_MapDataKeys.VISIBILITY_ZOOM, -1);
			if (recordZoom >= 0)
				return recordZoom;
		}

		return m_LocationType.GetVisibilityZoom();
	}

	//! Determine if we should use small icon based on zoom level
	protected bool ShouldUseSmallIcon()
	{
		if (!m_LocationType || !m_MapEntity)
			return true;

		float currentZoom = m_MapEntity.GetCurrentZoom();
		float visibilityZoom = GetEffectiveVisibilityZoom();

		return currentZoom < visibilityZoom;
	}
	
	//! Check if name should show at current zoom level
	protected bool ShouldShowNameAtCurrentZoom()
	{
		if (!m_LocationType || !m_MapEntity)
			return false;
		
		float currentZoom = m_MapEntity.GetCurrentZoom();
		float showNameZoom = m_LocationType.GetShowNameZoom();
		
		return currentZoom >= showNameZoom;
	}
	
	//! Get current player ID
	//! CLIENT-ONLY. Resolved from the local runtime player id rather than from a controlled entity, so it
	//! still answers for a dead player with no character.
	protected string GetCurrentPlayerID()
	{
		return OVT_Global.GetLocalPersistentId();
	}
	
	
	//! Show or hide hover effects
	protected void ShowHoverEffects(bool show)
	{
		if (m_wHoverOverlay)
			m_wHoverOverlay.SetVisible(show);
		
		if (m_wHighlight && !m_bSelected)
		{
			m_wHighlight.SetVisible(show);
			if (show)
				AnimateWidget.Opacity(m_wHighlight, 0.5, ANIM_SPEED);
			else
				AnimateWidget.Opacity(m_wHighlight, 0.0, ANIM_SPEED);
		}
	}
	
	//! Update location name display
	protected void UpdateLocationName()
	{
		if (!m_wLocationName || !m_LocationData || !m_LocationType)
			return;
		
		// Check if location type should show name
		if (!m_LocationType.ShouldShowName())
		{
			m_wLocationName.SetVisible(false);
			return;
		}
		
		// Check zoom level and selection state
		// Show name if zoom level is sufficient AND element is not selected (selected elements only show popup)
		bool shouldShow = ShouldShowNameAtCurrentZoom() && !m_bSelected;
		m_wLocationName.SetVisible(shouldShow);
		
		if (shouldShow)
		{
			string name = m_LocationType.GetLocationName(m_LocationData);
			m_wLocationName.SetText(name);
		}
	}
	
	//! Update distance display
	protected void UpdateDistance()
	{
		if (!m_wDistance || !m_LocationData || !m_LocationType)
			return;
		
		// Only update if location type shows distance
		if (!m_LocationType.ShouldShowDistance())
		{
			m_wDistance.SetVisible(false);
			return;
		}
		
		// Check zoom level and selection state - distance should show when name shows
		bool shouldShow = ShouldShowNameAtCurrentZoom() && !m_bSelected;
		if (!shouldShow)
		{
			m_wDistance.SetVisible(false);
			return;
		}
		
		// Calculate distance from player
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
		{
			m_wDistance.SetVisible(false);
			return;
		}
		
		vector playerPos = playerEntity.GetOrigin();
		float distance = vector.Distance(playerPos, m_LocationData.m_vPosition);
		
		// Format distance text
		string distanceText;
		if (distance < 1000)
			distanceText = string.Format("%1 m", Math.Round(distance));
		else
			distanceText = string.Format("%1 km", (distance / 1000).ToString(-1, 1));
		
		m_wDistance.SetText(distanceText);
		m_wDistance.SetVisible(true);
	}
	
	//! Re-run every visibility gate against the caller's last intent, without changing that intent.
	//!
	//! THE ONE IMPLEMENTATION of "re-evaluate whether this marker belongs on screen". Three callers
	//! want it for three unrelated reasons - the zoom changed, the record was replaced by a refresh
	//! tick, or the player toggled this whole type in the map layer-filter panel - and all three want
	//! exactly the same thing, so they share this rather than each repeating the SetVisible(m_bVisible)
	//! line. m_bVisible is the base class's record of what the container last asked for; feeding it
	//! back in is what makes this a re-evaluation rather than a show.
	//!
	//! No widget is created or destroyed here, which is what keeps a live hover, a pinned info panel
	//! and the base class's static selection intact across a filter toggle.
	void RefreshVisibility()
	{
		SetVisible(m_bVisible);
	}

	//! Called when the map zoom level changes
	void OnZoomChanged()
	{
		// Update visibility based on new zoom level
		RefreshVisibility();
		
		// Update display elements
		UpdateDisplay();
	}
	
	//! Point this element at a replacement record and redraw it.
	//!
	//! The refresh path (OVT_OverthrowMapUI.RefreshLocationType) re-runs PopulateLocations and gets back
	//! FRESH OVT_MapLocationData objects, so a live element has to be re-pointed rather than mutated in
	//! place. Doing it this way keeps the element - and therefore any hover, pin or selection resting on
	//! it - alive across a refresh, instead of destroying and recreating the marker under the cursor.
	//! \param[in] locationData The replacement record. Ignored when null, so a refresh can never blank
	//!            out a live element.
	void SetLocationData(OVT_MapLocationData locationData)
	{
		if (!locationData)
			return;

		m_LocationData = locationData;
		OnLocationDataChanged();
	}

	//! Called when the location data is updated
	//!
	//! This hook existed with no callers at all - the map populated once per open and never looked at
	//! the managers again (BUG-136). It is now called by SetLocationData on every refresh tick for the
	//! types that opt in via m_fRefreshInterval.
	void OnLocationDataChanged()
	{
		// Visibility is re-evaluated too, not just the drawing: ShouldShowLocation reads live manager
		// state (ownership, discovery, faction) and that is exactly the kind of thing a refresh is for.
		RefreshVisibility();

		UpdateDisplay();
	}
	
	//! Check if info popup is currently visible
	protected bool IsInfoPopupVisible()
	{
		return m_bInfoPopupVisible;
	}
	
	//! Set info popup visibility state
	void SetInfoPopupVisible(bool visible)
	{
		if (m_bInfoPopupVisible == visible)
			return;
		
		m_bInfoPopupVisible = visible;
		
		// Update name and distance visibility when popup state changes
		UpdateLocationName();
		UpdateDistance();
	}
	
	//! Check if this element should be visible at current zoom level
	//!
	//! FOUR GATES, AND THE ORDER IS A DESIGN PROPERTY. The caller's own intent, the player's layer
	//! filter, the zoom threshold and the per-record ShouldShowLocation all have to agree before a
	//! marker is drawn.
	//!
	//! HOT PATH - this runs for every element on every zoom change, alongside ShouldUseSmallIcon.
	override void SetVisible(bool visible)
	{
		if (!m_LocationType)
		{
			super.SetVisible(false);
			return;
		}
		
		// THE PLAYER'S LAYER FILTER, TESTED FIRST ON PURPOSE. It is a per-type constant, so checking it
		// before anything else lets a hidden type skip both GetEffectiveVisibilityZoom's per-record map
		// lookup and ShouldShowLocation's live manager reads. A hidden type therefore costs LESS per
		// zoom change than a shown one - hiding things can never make the sweep slower, which is what
		// lets the filter panel be built on a sweep at all.
		//
		// This gate does NOT belong inside ShouldShowLocation: that is a per-RECORD virtual with
		// manager lookups in its overrides, and this is one boolean that is identical for every record
		// of the type. Putting it there would pay the whole per-record cost to answer a per-type
		// question, and would put a presentation preference inside the campaign-visibility contract.
		if (!m_LocationType.IsPlayerVisible())
		{
			super.SetVisible(false);
			return;
		}

		// Check zoom level visibility
		bool zoomVisible = true;
		if (m_MapEntity)
		{
			float currentZoom = m_MapEntity.GetCurrentZoom();
			float visibilityZoom = GetEffectiveVisibilityZoom();
			zoomVisible = currentZoom >= visibilityZoom;
		}
		
		// Check location-specific visibility
		bool locationVisible = true;
		if (m_LocationData)
		{
			string playerID = GetCurrentPlayerID();
			locationVisible = m_LocationType.ShouldShowLocation(m_LocationData, playerID);
		}
		
		super.SetVisible(visible && zoomVisible && locationVisible);
	}
}