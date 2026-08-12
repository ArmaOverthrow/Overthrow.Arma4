//! Base class for every Overthrow map overlay that draws into the map's CanvasWidget.
//!
//! A layer owns a command "bucket" (m_Commands) which its own Draw() clears and refills. It does NOT
//! hand that bucket to the canvas itself - every layer resolves the same CanvasWidget, so a direct
//! SetDrawCommands would discard every other layer's work. Update submits the bucket to
//! OVT_MapCanvasCompositor instead, which composites all registered layers in m_iDrawOrder and issues
//! the single SetDrawCommands.
//!
//! The registration lifecycle is: OnMapOpen registers, OnMapClose unregisters, SetActive(false)
//! unregisters. A subclass that overrides any of those MUST call super.
[BaseContainerProps()]
class OVT_MapCanvasLayer : SCR_MapModuleBase
{
	//-----------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------

	[Attribute("100", UIWidgets.Auto, "Composite order. Lower is drawn FIRST and therefore sits UNDERNEATH. 100 = territory, 200 = restriction rings.")]
	protected int m_iDrawOrder;

	[Attribute("", UIWidgets.Auto, "Stable id for this layer, used by the layer-toggle UI to address it. Lowercase, no spaces.")]
	protected string m_sLayerId;

	[Attribute("", UIWidgets.Auto, "Localization key for this layer's name in the layer-toggle UI. Nothing renders it yet.")]
	protected string m_sDisplayName;

	[Attribute(defvalue: "", uiwidget: UIWidgets.Object, desc: "Optional per-layer faction colours. Leave UNSET to use the shared default (occupier red, resistance green) - only add one when this layer needs its own shade, e.g. a fill that has to sit under lines drawn in the same hue.")]
	protected ref OVT_MapFactionPalette m_FactionColors;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	protected CanvasWidget m_Canvas;

	//! This layer's command bucket. Owned here, cleared and refilled by this layer's Draw(), and only
	//! borrowed by the compositor during a flush.
	protected ref array<ref CanvasWidgetCommand> m_Commands;

	//! Runtime visibility. Toggling this is instant and reversible, unlike SetActive(false), which
	//! removes the module from the map entity's active list with no script-reachable way back.
	protected bool m_bVisible = true;

	//! Cached affine screen basis, rebuilt by CacheProjection
	protected bool m_bProjectionValid;
	protected float m_fProjWorldX;
	protected float m_fProjWorldZ;
	protected float m_fProjScreenX;
	protected float m_fProjScreenY;
	protected float m_fProjXAxisX;
	protected float m_fProjXAxisY;
	protected float m_fProjZAxisX;
	protected float m_fProjZAxisY;

	//-----------------------------------------------------------------------
	// DRAWING
	//-----------------------------------------------------------------------

	//! Fill m_Commands with this layer's commands for this frame. Override this, never Update.
	void Draw()
	{

	}

	override void Update(float timeSlice)
	{
		if (m_bVisible)
		{
			Draw();
		}
		else if (m_Commands)
		{
			// Hidden: submit an EMPTY bucket rather than skipping the submit, so this layer's commands
			// leave the composite on the very next frame
			m_Commands.Clear();
		}

		// UNCONDITIONAL, and deliberately outside every branch above. A layer that skips its submit -
		// because it is hidden, or because its Draw() bailed early - would freeze the whole composite on
		// its last complete frame, not just its own contribution.
		// This is also why there is no "only flush when the bucket is non-empty" guard here: a layer
		// that clears its bucket to empty must be able to say so.
		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (compositor)
		{
			compositor.SubmitAndFlush(this, m_Canvas);
		}
	}

	void DrawCircle(vector center, float range, int color, int n = 36, SharedItemRef tex = null, float uvScale = 0)
	{
		PolygonDrawCommand cmd = new PolygonDrawCommand();
		cmd.m_iColor = color;

		// Only touch the texture fields when a texture was actually supplied, so untextured call sites
		// produce byte-identical commands
		if (tex)
		{
			cmd.m_pTexture = tex;
			cmd.m_fUVScale = uvScale;
		}

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

	//-----------------------------------------------------------------------
	// COLOUR
	//-----------------------------------------------------------------------

	//! This layer's faction palette: its own if the conf configured one, otherwise the shared default.
	//! Never null.
	OVT_MapFactionPalette GetFactionPalette()
	{
		if (m_FactionColors)
			return m_FactionColors;

		return OVT_MapFactionPalette.GetDefault();
	}

	//! THE ONE ENTRY POINT for "what colour is faction N on this layer, at this alpha".
	//!
	//! The territory fill, the restriction rings and the influence lines all resolve through here, so a
	//! line, a ring and the region beneath them cannot drift apart in hue while every one of them keeps
	//! its own alpha - which is what carries contested-versus-held and in-effect-versus-suppressed.
	//!
	//! IT RESOLVES BY CAMPAIGN ROLE, NOT BY FACTION. See OVT_MapFactionPalette: the occupier is red
	//! whether the campaign cast USSR or US, because a map that changes its enemy colour with its enemy
	//! faction is a map the player has to re-learn.
	//! \param[in] factionIndex The controlling faction index, or any negative value for "no faction".
	//! \param[in] alpha Alpha to pack, 0-255, clamped.
	//! \return Packed ARGB, falling back to the palette's unknown colour (white by default).
	int ResolveFactionArgb(int factionIndex, int alpha)
	{
		return GetFactionPalette().GetArgb(factionIndex, alpha);
	}

	//-----------------------------------------------------------------------
	// PROJECTION
	//-----------------------------------------------------------------------

	//! Derive an affine screen basis from THREE WorldToScreen calls - a world origin and two widely
	//! separated axis points - so a polygon can be projected with two mul-adds per vertex instead of one
	//! WorldToScreen call per vertex. Call once per frame before projecting: the basis moves with pan
	//! and zoom.
	//!
	//! The world-Z to screen-Y sign is DERIVED from the samples, never assumed. DrawCircle's symmetry
	//! and DrawRectangle's two independent projections both hide that sign, and getting it wrong mirrors
	//! a polygon vertically into a plausible-but-wrong shape rather than an obvious bug.
	//!
	//! Widely separated samples matter: WorldToScreen returns integers, so quantisation error in the
	//! axis vectors is divided by the separation.
	void CacheProjection()
	{
		m_bProjectionValid = false;

		if (!m_MapEntity)
			return;

		float spanX = m_MapEntity.GetMapSizeX();
		float spanZ = m_MapEntity.GetMapSizeY();

		if (spanX < 1)
			spanX = 1000;

		if (spanZ < 1)
			spanZ = 1000;

		int originScreenX, originScreenY;
		int axisXScreenX, axisXScreenY;
		int axisZScreenX, axisZScreenY;

		m_MapEntity.WorldToScreen(0, 0, originScreenX, originScreenY, true);
		m_MapEntity.WorldToScreen(spanX, 0, axisXScreenX, axisXScreenY, true);
		m_MapEntity.WorldToScreen(0, spanZ, axisZScreenX, axisZScreenY, true);

		m_fProjWorldX = 0;
		m_fProjWorldZ = 0;

		m_fProjScreenX = originScreenX;
		m_fProjScreenY = originScreenY;

		m_fProjXAxisX = (float)(axisXScreenX - originScreenX) / spanX;
		m_fProjXAxisY = (float)(axisXScreenY - originScreenY) / spanX;

		m_fProjZAxisX = (float)(axisZScreenX - originScreenX) / spanZ;
		m_fProjZAxisY = (float)(axisZScreenY - originScreenY) / spanZ;

		m_bProjectionValid = true;
	}

	//! Project a world position to screen space through the cached basis, with no WorldToScreen call.
	//! Falls back to WorldToScreen when CacheProjection has not run, so a caller that forgets it gets
	//! correct-but-slow output rather than garbage at the origin.
	//! \param wx world X
	//! \param wz world Z
	//! \param sx out screen X
	//! \param sy out screen Y
	void ProjectWorld(float wx, float wz, out int sx, out int sy)
	{
		if (!m_bProjectionValid)
		{
			if (m_MapEntity)
			{
				m_MapEntity.WorldToScreen(wx, wz, sx, sy, true);
			}
			else
			{
				sx = 0;
				sy = 0;
			}

			return;
		}

		float dx = wx - m_fProjWorldX;
		float dz = wz - m_fProjWorldZ;

		sx = (int)(m_fProjScreenX + dx * m_fProjXAxisX + dz * m_fProjZAxisX);
		sy = (int)(m_fProjScreenY + dx * m_fProjXAxisY + dz * m_fProjZAxisY);
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		m_Commands = new array<ref CanvasWidgetCommand>();
		m_bProjectionValid = false;

		m_Canvas = CanvasWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_WIDGET_NAME));

		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (compositor)
		{
			compositor.Register(this);
		}
	}

	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);

		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (compositor)
		{
			compositor.Unregister(this);
		}

		m_bProjectionValid = false;
	}

	override void SetActive(bool active, bool isCleanup = false)
	{
		// Unregister before super: deactivation removes the module from the map entity's active list and
		// this layer will never Update - or submit - again
		if (!active)
		{
			OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
			if (compositor)
			{
				compositor.Unregister(this);
			}
		}

		super.SetActive(active, isCleanup);
	}

	//-----------------------------------------------------------------------
	// GETTERS/SETTERS
	//-----------------------------------------------------------------------

	//! Composite order. Lower is drawn first and therefore sits underneath.
	int GetDrawOrder()
	{
		return m_iDrawOrder;
	}

	//! Stable id used by the layer-toggle UI to address this layer.
	string GetLayerId()
	{
		return m_sLayerId;
	}

	//! Localization key for this layer's name in the layer-toggle UI.
	string GetDisplayName()
	{
		return m_sDisplayName;
	}

	//! Show or hide this layer. Instant, reversible, and the toggle primitive the layer-toggle UI must
	//! use - SetActive(false) is one-way from script and would strand the layer.
	//! \param visible true to draw, false to submit an empty bucket
	void SetLayerVisible(bool visible)
	{
		m_bVisible = visible;
	}

	//! Whether this layer is currently drawing.
	bool IsLayerVisible()
	{
		return m_bVisible;
	}

	//! This layer's command bucket, borrowed by the compositor during a flush. May be null before the
	//! map has opened.
	array<ref CanvasWidgetCommand> GetCommands()
	{
		return m_Commands;
	}
}
