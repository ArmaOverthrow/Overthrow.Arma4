[BaseContainerProps()]
class OVT_MapThreatGrid : OVT_MapCanvasLayer
{
	[Attribute("1000", UIWidgets.Auto, "Size of each grid cell in meters" )]
	protected int m_iGridSize;
	
	protected ref array<vector> m_ThreatCenters;
	protected ref array<int> m_ThreatLevels;
	protected ref array<int> m_ThreatColors;
	protected float m_fGridWidth;
	protected float m_fGridHeight;
	
	override void Draw()
	{
		m_Commands.Clear();
		
		if (!m_ThreatCenters || m_ThreatCenters.IsEmpty())
			return;
		
		// Draw all cached threat cells
		foreach (int i, vector center : m_ThreatCenters)
		{
			DrawRectangle(center, m_fGridWidth, m_fGridHeight, m_ThreatColors[i]);
		}
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_ThreatCenters = new array<vector>;
		m_ThreatLevels = new array<int>;
		m_ThreatColors = new array<int>;
		
		OVT_OccupyingFactionManager factionMgr = OVT_Global.GetOccupyingFaction();
		if (!factionMgr)
			return;
		
		// Get map dimensions
		float mapSizeX = m_MapEntity.GetMapSizeX();
		float mapSizeY = m_MapEntity.GetMapSizeY();
		
		// Calculate grid dimensions
		int gridCountX = Math.Ceil(mapSizeX / m_iGridSize);
		int gridCountY = Math.Ceil(mapSizeY / m_iGridSize);
		
		// Store grid cell dimensions for drawing
		m_fGridWidth = m_iGridSize;
		m_fGridHeight = m_iGridSize;
		
		// Track maximum threat for opacity scaling
		int maxThreatRecorded = 0;
		
		// First pass: calculate all threats and find maximum
		for (int x = 0; x < gridCountX; x++)
		{
			for (int y = 0; y < gridCountY; y++)
			{
				// Calculate center of grid cell
				vector gridCenter = Vector(
					x * m_iGridSize + m_iGridSize / 2,
					0,
					y * m_iGridSize + m_iGridSize / 2
				);
				
				int threat = factionMgr.GetThreatByLocation(gridCenter);
				if (threat > 0)
				{
					m_ThreatCenters.Insert(gridCenter);
					m_ThreatLevels.Insert(threat);
					
					if (threat > maxThreatRecorded)
						maxThreatRecorded = threat;
				}
			}
		}
		
		if (maxThreatRecorded < 5)
			maxThreatRecorded = 5;
		
		// Second pass: calculate colors based on threat levels
		foreach (int threat : m_ThreatLevels)
		{
			// Calculate opacity based on threat level (max 50% opacity)
			float threatRatio = threat / (float)maxThreatRecorded;
			int opacity = Math.Min(threatRatio * 128, 128); // Max 50% opacity (128/255)
			
			// Store red color with calculated opacity
			m_ThreatColors.Insert(ARGB(opacity, 255, 0, 0));
		}
	}
	
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		
		if (m_ThreatCenters)
		{
			m_ThreatCenters.Clear();
			m_ThreatCenters = null;
		}
		
		if (m_ThreatLevels)
		{
			m_ThreatLevels.Clear();
			m_ThreatLevels = null;
		}
		
		if (m_ThreatColors)
		{
			m_ThreatColors.Clear();
			m_ThreatColors = null;
		}
	}
}