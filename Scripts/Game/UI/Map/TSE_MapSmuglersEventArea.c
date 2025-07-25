[BaseContainerProps()]
class TSE_MapSmuglersEventArea : OVT_MapCanvasLayer
{
	static TSE_MapSmuglersEventArea s_Instance;

	protected ref array<vector> m_Centers;
	protected ref array<float> m_Ranges;

	void TSE_MapSmuglersEventArea()
	{
		s_Instance = this;
	}

	static TSE_MapSmuglersEventArea GetInstance()
	{
		return s_Instance;
	}

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		m_Centers = new array<vector>;
		m_Ranges = new array<float>;
	}

	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		m_Centers.Clear();
		m_Ranges.Clear();
	}

	void AddEventArea(vector pos, float radius)
	{
		m_Centers.Insert(pos);
		m_Ranges.Insert(radius);
	}

	void ClearAllEventAreas()
	{
		m_Centers.Clear();
		m_Ranges.Clear();
	}

	override void Draw()
	{
		m_Commands.Clear();
		for (int i = 0; i < m_Centers.Count(); i++)
		{
			DrawCircle(m_Centers[i], m_Ranges[i], ARGB(60, 33, 14, 255));
		}
	}
} 