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
	protected ImageWidget m_wHighlight;
	
	//! State tracking
	protected bool m_bSelected = false;
	protected bool m_bIsHovered = false;
	protected bool m_bInfoPopupVisible = false;
	
	
	//! Sound attributes
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
		m_wHighlight = ImageWidget.Cast(w.FindAnyWidget("Highlight"));
	}
	
	//! Handle click on this element
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_bVisible || !m_LocationData || !m_LocationType)
			return false;
				
		if (button == 0) // Left mouse button
		{
			// Play click sound
			PlayHoverSound(m_sSoundClick);
			
			// If already selected, deselect and hide info panel
			if (m_bSelected)
			{
				Select(false);
				if (m_ParentMapUI)
					m_ParentMapUI.HideLocationInfo();
			}
			else
			{
				// Select this element
				Select(true);
				
				// Notify parent container of selection
				if (m_Parent)
					m_Parent.OnElementSelected(this);
				
				// Notify parent map UI
				if (m_ParentMapUI)
					m_ParentMapUI.SelectLocation(this);
				
				// Notify location type of click
				m_LocationType.OnLocationClicked(m_LocationData, this);
			}
			
			return true;
		}
		
		return false;
	}
	
	//! Handle mouse hover enter
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (!m_bVisible)
			return false;
		
		m_bIsHovered = true;
		ShowHoverEffects(true);
		PlayHoverSound(m_sSoundHover);
				
		return false;
	}
	
	//! Handle mouse hover leave
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (!m_bVisible)
			return false;
		
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
	
	//! Determine if we should use small icon based on zoom level
	protected bool ShouldUseSmallIcon()
	{
		if (!m_LocationType || !m_MapEntity)
			return true;
		
		float currentZoom = m_MapEntity.GetCurrentZoom();
		float visibilityZoom = m_LocationType.GetVisibilityZoom();
		
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
	
	//! Called when the map zoom level changes
	void OnZoomChanged()
	{
		// Update visibility based on new zoom level
		SetVisible(m_bVisible);
		
		// Update display elements
		UpdateDisplay();
	}
	
	//! Called when the location data is updated
	void OnLocationDataChanged()
	{
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
	override void SetVisible(bool visible)
	{
		if (!m_LocationType)
		{
			super.SetVisible(false);
			return;
		}
		
		// Check zoom level visibility
		bool zoomVisible = true;
		if (m_MapEntity)
		{
			float currentZoom = m_MapEntity.GetCurrentZoom();
			float visibilityZoom = m_LocationType.GetVisibilityZoom();
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