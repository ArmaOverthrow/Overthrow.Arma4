[BaseContainerProps()]
class OVT_MapCanvasLayer : SCR_MapModuleBase
{		
	protected CanvasWidget m_Canvas;
	protected ref array<ref CanvasWidgetCommand> m_Commands;
		
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
		float r = range * m_MapEntity.GetCurrentZoom();
		
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
	
	void DrawRectangle(vector worldPos, float worldWidth, float worldHeight, int color)
	{
		PolygonDrawCommand cmd = new PolygonDrawCommand();
		cmd.m_iColor = color;
		cmd.m_Vertices = new array<float>;
		
		// Convert world corners to screen coordinates
		int x1, y1, x2, y2;
		m_MapEntity.WorldToScreen(worldPos[0] - worldWidth/2, worldPos[2] - worldHeight/2, x1, y1, true);
		m_MapEntity.WorldToScreen(worldPos[0] + worldWidth/2, worldPos[2] + worldHeight/2, x2, y2, true);
		
		// Draw rectangle using 4 corners
		cmd.m_Vertices.Insert(x1);
		cmd.m_Vertices.Insert(y1);
		cmd.m_Vertices.Insert(x2);
		cmd.m_Vertices.Insert(y1);
		cmd.m_Vertices.Insert(x2);
		cmd.m_Vertices.Insert(y2);
		cmd.m_Vertices.Insert(x1);
		cmd.m_Vertices.Insert(y2);
		
		m_Commands.Insert(cmd);
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_Commands = new array<ref CanvasWidgetCommand>();

		m_Canvas = CanvasWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_WIDGET_NAME));
	}
}