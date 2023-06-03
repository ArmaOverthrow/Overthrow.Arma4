class OVT_MapPlayerLocation : SCR_MapUIBaseComponent
{	
	protected SCR_MapToolEntry m_ToolMenuEntry;
	
	protected ref map<int,ref Widget> m_Widgets;
	
	protected int m_iLocalPlayerId = -1;
		
	[Attribute()]
	protected ResourceName m_Layout;

	override void Update(float timeSlice)
	{
		if (!m_Widgets)
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
		
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player)
		{
			return;
		}
		
		m_iLocalPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		
		if(!m_Widgets) m_Widgets = new map<int,ref Widget>;
		m_Widgets.Clear();
		
		FactionManager faction = GetGame().GetFactionManager();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		
		Faction fac = faction.GetFactionByKey(otconfig.m_sPlayerFaction);
		if(!fac) return;

		array<int> players = new array<int>;
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
