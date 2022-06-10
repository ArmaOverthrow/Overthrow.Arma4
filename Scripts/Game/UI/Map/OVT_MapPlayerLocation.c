class OVT_MapPlayerLocation : SCR_MapUIBaseComponent
{
	protected Widget m_Widget;
	protected ImageWidget m_Image;
	protected SCR_MapToolEntry m_ToolMenuEntry;
	
	[Attribute()]
	protected ResourceName m_Layout;

	override void Update()
	{
		if (!m_Widget)
			return;

		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player)
		{
			m_Widget.RemoveFromHierarchy();
			return;
		}

		vector posPlayer = player.GetOrigin();

		float x, y;
		m_MapEntity.WorldToScreen(posPlayer[0], posPlayer[2], x, y, true);

		x = GetGame().GetWorkspace().DPIUnscale(x);
		y = GetGame().GetWorkspace().DPIUnscale(y);
				
		vector ypr = player.GetYawPitchRoll();	
		
		m_Image.SetRotation(ypr[0]);
		FrameSlot.SetPos(m_Widget, x, y);
	}

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		if (m_Widget)
		{
			return;
		}
		
		FactionManager faction = GetGame().GetFactionManager();
		OVT_OverthrowConfigComponent otconfig = OVT_OverthrowConfigComponent.GetInstance();
		
		Faction fac = faction.GetFactionByKey(otconfig.m_sPlayerFaction);
		if(!fac) return;

		m_Widget = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);

		m_Image = ImageWidget.Cast(m_Widget.FindAnyWidget("Image"));
		if(m_Image)
		{
			m_Image.SetColor(fac.GetFactionColor());
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		SCR_MapToolMenuModule toolMenu = SCR_MapToolMenuModule.Cast(m_MapEntity.GetMapModule(SCR_MapToolMenuModule));
		if (toolMenu)
		{
			//m_ToolMenuEntry = toolMenu.RegisterToolMenuEntry("{14A14368AE7FA04D}UI/Textures/Icons/PlayerMarker.imageset", "PlayerMarker", false, 99);
			//m_ToolMenuEntry.m_OnClick.Insert(ZoomInOnPlayer);
		}
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
