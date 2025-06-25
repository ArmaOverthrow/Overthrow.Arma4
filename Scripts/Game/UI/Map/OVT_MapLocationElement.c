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
	protected Widget m_wIconContainer;
	protected ImageWidget m_wIcon;
	protected TextWidget m_wDistance;
	protected Widget m_wSelectionHighlight;
	protected Widget m_wFastTravelIndicator;
	
	//! Whether this element is currently selected
	protected bool m_bSelected = false;
	
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
		m_wIconContainer = w.FindAnyWidget("IconContainer");
		m_wIcon = ImageWidget.Cast(w.FindAnyWidget("Icon"));
		m_wDistance = TextWidget.Cast(w.FindAnyWidget("Distance"));
		m_wSelectionHighlight = w.FindAnyWidget("SelectionHighlight");
		m_wFastTravelIndicator = w.FindAnyWidget("FastTravelIndicator");
		
		// Setup initial state
		UpdateIcon();
		UpdateSelection();
		UpdateFastTravelIndicator();
	}
	
	//! Handle click on this element
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_bVisible || !m_LocationData || !m_LocationType)
			return false;
				
		if (button == 0) // Left mouse button
		{
			// Notify parent container of selection
			if (m_Parent)
				m_Parent.OnElementSelected(this);
			
			// Select this element
			if (m_ParentMapUI)
				m_ParentMapUI.SelectLocation(this);
			
			// Notify location type of click
			m_LocationType.OnLocationClicked(m_LocationData, this);
			
			return true;
		}
		
		return false;
	}
	
	//! Handle mouse hover enter
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (!m_bVisible)
			return false;
		
		// Could add hover effects here
		return false;
	}
	
	//! Handle mouse hover leave
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (!m_bVisible)
			return false;
		
		// Could remove hover effects here
		return false;
	}
	
	//! Set selection state
	void SetSelected(bool selected)
	{
		if (m_bSelected == selected)
			return;
		
		m_bSelected = selected;
		UpdateSelection();
		
		// Notify location type of selection change
		if (m_LocationType && selected)
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
		if (!m_wSelectionHighlight)
			return;
		
		m_wSelectionHighlight.SetVisible(m_bSelected);
		
		// Could add animation or color changes here
		if (m_bSelected)
		{
			// Add selection effects
		}
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
	
	//! Called when the map zoom level changes
	void OnZoomChanged()
	{
		UpdateIcon();
		UpdateFastTravelIndicator();
	}
	
	//! Called when the location data is updated
	void OnLocationDataChanged()
	{
		UpdateIcon();
		UpdateFastTravelIndicator();
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