[BaseContainerProps()]
class OVT_MapCanvasLayer : SCR_MapModuleBase
{		
	protected Widget m_Widget;
	protected CanvasWidget m_Canvas;
	protected ref array<ref CanvasWidgetCommand> m_Commands;
	
	protected ResourceName m_Layout = "{A6A79ABB08D490BE}UI/Layouts/Map/MapCanvasLayer.layout";
		
	void Draw()
	{		
		
	}
	
	override void Update(float timeSlice)
	{	
		Draw();	
		if(m_Commands.Count() > 0)
		{						
			m_Canvas.SetDrawCommands(m_Commands);			
		}
	}
	
	void DrawCircle(vector center, float range, int color, int n = 36)	
	{
		PolygonDrawCommand cmd = new PolygonDrawCommand();		
		cmd.m_iColor = color;
		
		cmd.m_Vertices = new array<float>;
		
		int xcp, ycp;
		
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		float r = range * m_MapEntity.GetCurrentPixelPerUnit();
		
		for(int i = 0; i < n; i++)
		{
			float theta = i*(2*Math.PI/n);
			float x = (int)(xcp + r*Math.Cos(theta));
			float y = (int)(ycp + r*Math.Sin(theta));
			cmd.m_Vertices.Insert(x);
			cmd.m_Vertices.Insert(y);
		}
		
		m_Commands.Insert(cmd);
	}
	
	void DrawImage(vector center, int width, int height, SharedItemRef tex)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		
		int xcp, ycp;		
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		cmd.m_pTexture = tex;
		cmd.m_Size = Vector(width, height, 0);
		
		m_Commands.Insert(cmd);
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_Commands = new array<ref CanvasWidgetCommand>();
		
		m_Widget = GetGame().GetWorkspace().CreateWidgets(m_Layout);

		m_Canvas = CanvasWidget.Cast(m_Widget.FindAnyWidget("Canvas"));
	}
	
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		
		m_Widget.RemoveFromHierarchy();

	}
}