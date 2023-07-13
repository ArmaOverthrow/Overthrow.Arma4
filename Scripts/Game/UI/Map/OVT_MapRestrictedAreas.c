[BaseContainerProps()]
class OVT_MapRestrictedAreas : OVT_MapCanvasLayer
{		
	protected ref array<vector> m_Centers;
	protected ref array<vector> m_OccupyingCenters;
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
		}
		
		foreach(int i, vector center : m_OccupyingCenters)
		{			
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
		m_OccupyingCenters = new array<vector>;
		
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
			
			m_OccupyingCenters.Insert(base.location);
			m_Centers.Insert(base.location);
			m_Ranges.Insert(otconfig.m_Difficulty.baseCloseRange);
		}
		
		foreach(OVT_RadioTowerData tower : factionMgr.m_RadioTowers)
		{	
			if(!tower.IsOccupyingFaction()) {				
				continue;
			}
			m_Centers.Insert(tower.location);
			m_Ranges.Insert(20);
		}
			
		Faction faction = otconfig.GetOccupyingFactionData();
		if(faction)
		{
			m_Flag = m_Canvas.LoadTexture(faction.GetUIInfo().GetIconPath());
		}
		
		faction = otconfig.GetPlayerFactionData();
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