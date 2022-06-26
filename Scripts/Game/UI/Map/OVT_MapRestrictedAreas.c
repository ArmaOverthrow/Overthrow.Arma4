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
		
		if(factionMgr.m_CurrentQRF)
		{
			m_bQRFActive = true;
			m_QRFCenter = factionMgr.m_CurrentQRF.GetOwner().GetOrigin();
		}else{
			m_bQRFActive = false;
		}
		
		foreach(RplId id : factionMgr.m_Bases)
		{			
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity ent = rpl.GetEntity();
			OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(ent.FindComponent(OVT_BaseControllerComponent));
			if(!base.IsOccupyingFaction()) {
				m_ResistanceCenters.Insert(ent.GetOrigin());
				continue;
			};
			if(m_bQRFActive && factionMgr.m_CurrentQRFBase && factionMgr.m_CurrentQRFBase == base) continue;
			
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(base.m_iCloseRange);
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