class OVT_MapCampaignUI : SCR_MapUIElementContainer
{
	[Attribute("{9B45BD0282167D90}UI/Layouts/Map/BaseElement.layout", params: "layout")]
	protected ResourceName m_sBaseElement;
	
	protected void InitBases()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return;
		
		foreach (OVT_BaseData baseData : of.m_Bases)
		{
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_sBaseElement, m_wIconsContainer);
			OVT_CampaignMapUIBase handler = OVT_CampaignMapUIBase.Cast(w.FindHandler(OVT_CampaignMapUIBase));
			
			if (!handler)
				return;

			handler.SetParent(this);
			handler.InitBase(baseData);
			m_mIcons.Set(w, handler);

			FrameSlot.SetSizeToContent(w, true);
			FrameSlot.SetAlignment(w, 0.5, 0.5);
		}
		
		UpdateIcons();
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		InitBases();
		
		SCR_MapEntity.GetOnMapPan().Insert(UpdateIcons);
		SCR_MapEntity.GetOnMapZoom().Insert(UpdateIcons);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		
		SCR_MapEntity.GetOnMapPan().Remove(UpdateIcons);
		SCR_MapEntity.GetOnMapZoom().Remove(UpdateIcons);

		m_mIcons.Clear();
	}
	
	void ~OVT_MapCampaignUI()
	{
		foreach (Widget w, SCR_MapUIElement i : m_mIcons)
		{
			delete w;
		}
	}
}