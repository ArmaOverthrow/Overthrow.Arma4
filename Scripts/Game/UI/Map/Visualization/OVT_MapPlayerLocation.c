class OVT_MapPlayerLocation : SCR_MapUIBaseComponent
{	
	protected SCR_MapToolEntry m_ToolMenuEntry;
	
	protected ref map<int,ref Widget> m_Widgets;
	
	protected int m_iLocalPlayerId = -1;
		
	[Attribute()]
	protected ResourceName m_Layout;

	//! Whether the player-position markers are switched on in the map layer-filter panel.
	//!
	//! A CLIENT-SIDE PRESENTATION PREFERENCE, deliberately not an [Attribute]: it is pushed in from the
	//! filter panel and the per-profile store it loads, never authored in config. Default true, so a
	//! player who never opens that panel sees exactly today's map.
	//!
	//! This component is a SCR_MapUIBaseComponent rather than an OVT_MapCanvasLayer, so it never
	//! appears in the compositor's layer list and its filter row is the panel's one hand-built row.
	//! Reparenting a working, retained component onto the canvas-layer base purely to satisfy a list
	//! builder would risk a live feature to save one special case; the special case is the cheaper
	//! trade, and this comment is its documentation.
	protected bool m_bMarkersVisible = true;

	//! Whether this component actually got as far as creating markers for the CURRENT map session.
	//!
	//! False until OnMapOpen clears every one of its early returns. The filter panel reads it to decide
	//! whether to offer a Players row at all: a session where the difficulty preset has
	//! showPlayerOnMap off, or where there is no controlled entity, draws no markers, and a toggle over
	//! markers that were never going to exist is worse than no row - it reads as a broken control
	//! rather than as an unavailable feature.
	protected bool m_bAvailableThisSession;

	override void Update(float timeSlice)
	{
		if (!m_Widgets)
			return;

		// Nothing to position while the markers are filtered off. Skipping the loop is also what keeps
		// this method's own opacity handling from fighting SetMarkersVisible: the two would otherwise
		// both be deciding whether a marker is on screen, using different widget properties.
		if (!m_bMarkersVisible)
			return;

		for(int i = 0; i < m_Widgets.Count(); i++)
		{
			int playerId = m_Widgets.GetKey(i);
			Widget w = m_Widgets[playerId];
						
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if(!player)
			{
				w.SetOpacity(0);
				continue;
			}
			vector posPlayer = player.GetOrigin();
	
			float x, y;
			m_MapEntity.WorldToScreen(posPlayer[0], posPlayer[2], x, y, true);
	
			x = GetGame().GetWorkspace().DPIUnscale(x);
			y = GetGame().GetWorkspace().DPIUnscale(y);
					
			vector ypr = player.GetYawPitchRoll();	
			
			ImageWidget img = ImageWidget.Cast(w.FindAnyWidget("Image"));
			if(img)
				img.SetRotation(ypr[0]);
			
			FrameSlot.SetPos(w, x, y);
		}
	}

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		// Reset FIRST, before any early return, so a session that bails out below leaves this false and
		// the layer-filter panel omits the Players row entirely. Availability is a property of THIS map
		// session, not of the component, and the component outlives the session.
		m_bAvailableThisSession = false;

		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player)
		{
			return;
		}
		
		if(!OVT_Global.GetConfig().m_Difficulty.showPlayerOnMap)
		{
			return;
		}
		
		m_iLocalPlayerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(player);
		
		if(!m_Widgets) m_Widgets = new map<int,ref Widget>;
		m_Widgets.Clear();
		
		FactionManager faction = GetGame().GetFactionManager();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		
		Faction fac = faction.GetFactionByKey(otconfig.m_sPlayerFaction);
		if(!fac) return;

		// Past every early return: markers are about to be created, so the Players row is worth
		// offering. Set here rather than immediately after the showPlayerOnMap check because the
		// unresolved-faction return above also produces a session with no markers, and a row for it
		// would be exactly the dead toggle this flag exists to prevent.
		m_bAvailableThisSession = true;

		autoptr array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		mgr.GetPlayers(players);
		foreach(int playerId : players){
			Widget widget = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget img = ImageWidget.Cast(widget.FindAnyWidget("Image"));
			if(img)
			{
				img.SetColor(fac.GetFactionColor());
			}
			m_Widgets[playerId] = widget;
		}

		// Re-assert the current preference onto the freshly created widget set. The widgets are new and
		// therefore visible, while m_bMarkersVisible belongs to the component and survives a map close -
		// so without this, a player who hid the markers and then reopened the map would get a new set of
		// visible markers that Update() then refuses to reposition, leaving them parked wherever the
		// layout puts an unpositioned widget. Calling the setter rather than inlining SetVisible keeps
		// one implementation of "show or hide the marker set".
		SetMarkersVisible(m_bMarkersVisible);
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide every player-position marker.
	//!
	//! SetVisible, and deliberately NEVER SetOpacity. Update() already drives opacity for its own
	//! reason - it fades out a marker whose player has no controlled entity - so a filter built on
	//! opacity would be contending with it for the same property on the same widgets, and which one
	//! won would depend on tick order. Visibility is a property nothing else in this class touches.
	//! \param[in] visible True to draw the player markers, false to hide them all
	void SetMarkersVisible(bool visible)
	{
		m_bMarkersVisible = visible;

		if (!m_Widgets)
			return;

		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			Widget w = m_Widgets.GetElement(i);
			if (w)
				w.SetVisible(visible);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the player-position markers are switched on
	bool AreMarkersVisible()
	{
		return m_bMarkersVisible;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this map session actually created player markers - see m_bAvailableThisSession.
	//! \return True when a Players row is worth offering in the layer-filter panel
	bool IsAvailableThisSession()
	{
		return m_bAvailableThisSession;
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		
	}

	//------------------------------------------------------------------------------------------------
	protected void ZoomInOnPlayer()
	{
		// Reset button color so that it doesn't behave like a toggle
		//m_ToolMenuEntry.SetColor(Color.Gray25);

		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player)
		{
			return;
		}
		
		vector posPlayer = player.GetOrigin();

		vector mapSize = m_MapEntity.GetMapWidget().GetSizeInUnits();
		float zoomVal = mapSize[0] / (mapSize[0] * (m_MapEntity.GetMapWidget().PixelPerUnit() * 1.5));

		m_MapEntity.ZoomPanSmooth(zoomVal,posPlayer[0],posPlayer[2]);
	}
};
