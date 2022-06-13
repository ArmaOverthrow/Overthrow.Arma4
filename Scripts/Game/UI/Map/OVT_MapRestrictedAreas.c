[BaseContainerProps()]
class OVT_MapRestrictedAreas : OVT_MapCanvasLayer
{		
	protected ref array<vector> m_Centers;
	protected ref array<int> m_Ranges;
	protected ref SharedItemRef m_Flag;
	
	override void Draw()
	{			
		m_Commands.Clear();
		
		foreach(int i, vector center : m_Centers)
		{			
			DrawCircle(center, m_Ranges[i], ARGB(50, 255, 0, 0));
			DrawImage(center, 25, 25, m_Flag);
		}
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_Centers = new array<vector>;
		m_Ranges = new array<int>;
		
		OVT_OccupyingFactionManager factionMgr = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		
		
		foreach(EntityID id : factionMgr.m_Bases)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(ent.FindComponent(OVT_BaseControllerComponent));
			
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(base.m_iCloseRange);
		}
			
		OVT_Faction faction = otconfig.GetOccupyingFaction();
		if(faction)
		{
			m_Flag = m_Canvas.LoadTexture(faction.GetUIInfo().GetIconPath());
		}
		
		
	}
}