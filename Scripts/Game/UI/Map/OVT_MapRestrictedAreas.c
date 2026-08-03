[BaseContainerProps()]
class OVT_MapRestrictedAreas : OVT_MapCanvasLayer
{		
	protected ref array<vector> m_Centers;
	protected ref array<vector> m_OccupyingCenters;
	protected ref array<vector> m_ResistanceCenters;
	protected ref array<int> m_Ranges;
	// Resistance-held bases/towers also block FOB deployment - drawn in a friendly colour
	protected ref array<vector> m_FriendlyCenters;
	protected ref array<int> m_FriendlyRanges;
	protected ref SharedItemRef m_Flag;
	protected ref SharedItemRef m_ResistanceFlag;
	protected bool m_bQRFActive = false;
	protected vector m_QRFCenter;

	override void Draw()
	{
		if(!m_Centers || !m_Ranges || !m_FriendlyCenters || !m_FriendlyRanges || !m_OccupyingCenters || !m_ResistanceCenters)
			return;

		m_Commands.Clear();

		foreach(int i, vector center : m_Centers)
		{
			DrawCircle(center, m_Ranges[i], ARGB(50, 255, 0, 0));
		}

		foreach(int i, vector center : m_FriendlyCenters)
		{
			DrawCircle(center, m_FriendlyRanges[i], ARGB(40, 0, 120, 255));
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
		m_FriendlyCenters = new array<vector>;
		m_FriendlyRanges = new array<int>;
		
		OVT_OccupyingFactionManager factionMgr = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		
		if(factionMgr.m_bQRFActive)
		{
			m_bQRFActive = true;
			m_QRFCenter = factionMgr.m_vQRFLocation;
		}else{
			m_bQRFActive = false;
		}
		
		// Draw the radii the FOB deploy check actually enforces (it rejects near ANY base/tower)
		int baseRestrictedRange = otconfig.m_Difficulty.baseCloseRange + OVT_ResistanceFactionManager.FOB_DEPLOY_BASE_BUFFER;

		foreach(OVT_BaseData base : factionMgr.m_Bases)
		{
			if(!base.IsOccupyingFaction()) {
				m_ResistanceCenters.Insert(base.location);
				m_FriendlyCenters.Insert(base.location);
				m_FriendlyRanges.Insert(baseRestrictedRange);
				continue;
			};
			if(m_bQRFActive && factionMgr.m_iCurrentQRFBase > -1 && factionMgr.m_iCurrentQRFBase == base.id) continue;

			m_OccupyingCenters.Insert(base.location);
			m_Centers.Insert(base.location);
			m_Ranges.Insert(baseRestrictedRange);
		}

		foreach(OVT_RadioTowerData tower : factionMgr.m_RadioTowers)
		{
			if(!tower.IsOccupyingFaction()) {
				m_FriendlyCenters.Insert(tower.location);
				m_FriendlyRanges.Insert(OVT_ResistanceFactionManager.FOB_DEPLOY_TOWER_RANGE);
				continue;
			}
			m_Centers.Insert(tower.location);
			m_Ranges.Insert(OVT_ResistanceFactionManager.FOB_DEPLOY_TOWER_RANGE);
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
		m_FriendlyRanges.Clear();
		m_FriendlyRanges = null;
		m_FriendlyCenters.Clear();
		m_FriendlyCenters = null;
	}
}