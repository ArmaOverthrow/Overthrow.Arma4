[BaseContainerProps()]
class OVT_MapRestrictedAreas : OVT_MapCanvasLayer
{		
	protected ref array<vector> m_Centers;
	protected ref array<vector> m_ResistanceCenters;
	protected ref array<int> m_Ranges;
	protected ref SharedItemRef m_Flag;
	protected ref SharedItemRef m_ResistanceFlag;
	protected bool m_bQRFActive = false;
	protected vector m_QRFCenter;
	
	override void Draw()
	{			
		m_Commands.Clear();
		
		foreach(int i, vector center : m_Centers)
		{			
			DrawCircle(center, m_Ranges[i], ARGB(50, 255, 0, 0));
			DrawImage(center, 25, 25, m_Flag);
		}
		
		foreach(int i, vector center : m_ResistanceCenters)
		{			
			DrawImage(center, 25, 25, m_ResistanceFlag);
		}
		
		if(m_bQRFActive)
		{
			DrawCircle(m_QRFCenter, OVT_QRFControllerComponent.QRF_RANGE, ARGB(50, 255, 0, 0));
			DrawCircle(m_QRFCenter, OVT_QRFControllerComponent.QRF_POINT_RANGE, ARGB(50, 255, 0, 0));
		}
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_Centers = new array<vector>;
		m_Ranges = new array<int>;
		m_ResistanceCenters = new array<vector>;
		
		OVT_OccupyingFactionManager factionMgr = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		
		if(factionMgr.m_bQRFActive)
		{
			m_bQRFActive = true;
			m_QRFCenter = factionMgr.m_vQRFLocation;
		}else{
			m_bQRFActive = false;
		}
		
		foreach(OVT_BaseData base : factionMgr.m_Bases)
		{	
			if(!base.IsOccupyingFaction()) {
				m_ResistanceCenters.Insert(base.location);
				continue;
			};
			if(m_bQRFActive && factionMgr.m_iCurrentQRFBase > -1 && factionMgr.m_iCurrentQRFBase == base.id) continue;
			
			m_Centers.Insert(base.location);
			m_Ranges.Insert(base.closeRange);
		}
			
		OVT_Faction faction = otconfig.GetOccupyingFaction();
		if(faction)
		{
			m_Flag = m_Canvas.LoadTexture(faction.GetUIInfo().GetIconPath());
		}
		
		faction = otconfig.GetPlayerFaction();
		if(faction)
		{
			m_ResistanceFlag = m_Canvas.LoadTexture(faction.GetUIInfo().GetIconPath());
		}
	}
	
	override void OnMapClose(MapConfiguration config)
	{	
		super.OnMapClose(config);
			
		m_Ranges.Clear();
		m_Ranges = null;
		m_Centers.Clear();
		m_Centers = null;
	}
}