//------------------------------------------------------------------------------------------------
//! Draws faction-coloured territory regions beneath the map's location markers.
//!
//! WHAT THIS IS, AND IS NOT. The overlay is a READ-ONLY PROJECTION of campaign state that is already
//! replicated and already drawn as markers. It adds no replicated state, no RPC, no persistence, and
//! writes nothing back to any town, base, radio tower or FOB record. Everything it computes is a
//! rendering artefact that dies with the map close. That boundary is the single most important
//! property of this file.
//!
//! WHAT QUESTION IT ANSWERS. Not "who holds what" - "HOW MUCH DOES THE OCCUPIER STILL HOLD". By default
//! only occupier-held regions are drawn and liberated ground reads as clean map, which on an island
//! that starts almost entirely occupied makes the overlay a progress bar for the whole campaign. A
//! fully liberated island therefore draws NOTHING AT ALL, and that is the intended end state rather
//! than a failure - the solve log's drawn count is what distinguishes it from a broken overlay.
//!
//! THE FILTER IS ON THE EMIT PATH AND MUST STAY THERE. Every site is collected and every site competes
//! for ground, whatever faction holds it, because a liberated town or a resistance FOB is exactly what
//! pushes the occupier's boundary back. Filtering at collect time would let occupier territory flow
//! over ground the player has already taken.
//!
//! HOW THE GEOMETRY IS BUILT, IN THREE STAGES. The solver classifies a world-space GRID - which site
//! owns each square, and which squares are water. The border of each drawn region is traced through
//! that grid with marching squares and rounded with Chaikin corner cutting. The fill is then cut out of
//! THAT SAME CURVE, as a stack of trapezoids between horizontal scan rows whose sides run between the
//! contour's own crossings. Nothing is star-shaped about anything, so bays, inlets and offshore islands
//! need no special case and two regions can neither overlap nor leave a gap.
//!
//! ONE BOUNDARY PER REGION, USED TWICE - TO FILL AND TO BAND. That is the whole reason the fill is not
//! merged squares any more. It was, and the band was offset from the smoothed contour traced through
//! those squares, so one region had two different boundaries: blocky fill jutted past the smooth band
//! in places and fell short of it in others, and no amount of tuning could make two independently
//! derived curves coincide. The merged-square fill survives as the m_bTraceContours-off fallback, where
//! there is no contour to disagree with.
//!
//! HOW THE TWO STATES READ. A region the occupier firmly holds is a SOLID faction-coloured wash; a
//! region it holds while the population has gone over to the resistance is HATCHED with a diagonal
//! texture. Hatching is the conventional visual language for disputed ground, and moving the contested
//! signal onto it frees the fill's alpha, which had to carry the whole meaning on its own and could
//! only do it by making the more interesting region the quieter one. The neutral band along an
//! inter-faction frontier carries the same hatch at its own UV scale.
//!
//! THE WORK SPLIT. The GRID is solved once at map open and again only when the SET of sites changes.
//! Everything that depends on who holds what - colour, hatch, whether a region is drawn, where the
//! rectangles and the contours fall - is rebuilt on the slow refresh tick, which is what makes a
//! capture show up within one interval without re-classifying a single square. Per frame the layer
//! only projects and packs.
//!
//! CONFIG-DRIVEN, NOT CODE-DRIVEN. Every tunable is an attribute and every site type is a config
//! record. Adding a site type is a config entry plus one collector method; the solver and the emitter
//! never learn that a new type exists.
[BaseContainerProps()]
class OVT_MapTerritoryLayer : OVT_MapCanvasLayer
{
	//-----------------------------------------------------------------------
	// ATTRIBUTES - SITE TYPES
	//-----------------------------------------------------------------------

	[Attribute("", UIWidgets.Object, "One record per site type. The collector asks for a record by id, so this array is where relative weight is tuned and where a type is switched off. Territory is not clipped to a reach - it runs until it meets the coast or a competing site.")]
	protected ref array<ref OVT_TerritorySiteConfig> m_aSiteTypes;

	//-----------------------------------------------------------------------
	// ATTRIBUTES - GEOMETRY
	//-----------------------------------------------------------------------

	[Attribute("100", UIWidgets.Auto, "World size in metres of one ownership square. THE ONE KNOB THAT DECIDES BOTH THE RESOLUTION OF THE BOUNDARY AND THE COST OF FINDING IT, and the cost is QUADRATIC - 50 m is four times the work of 100 m. The fill and the border are both cut from the smoothed contour traced through these squares, so this sets how much detail a coastline can have rather than how blocky the fill looks. The base game draws its own map grid at 100 m on the closest zoom layer and 1000 m above it, and the ownership grid is snapped to multiples of this size from world zero so the two coincide when the sizes agree. There is a hard ceiling on the total square count, so a mistyped value produces a coarse overlay rather than a hung client.")]
	protected float m_fGridCellSize;

	[Attribute("2", UIWidgets.Auto, "Chaikin corner-cutting passes applied to every traced border. 0 leaves the raw marching-squares staircase. EACH PASS DOUBLES the border segment count, which is also what it costs to project and draw.")]
	protected int m_iSmoothPasses;

	[Attribute("25", UIWidgets.Auto, "Height in metres of one scan row of the fill. The fill is cut out of the SAME smoothed contour the neutral band is offset from, as a stack of trapezoids whose sides run between the contour's actual crossings at each row's top and bottom - so a slanted border is filled slanted rather than stepped. UNLIKE THE SQUARE SIZE THIS COSTS LINEARLY: halving it is twice the geometry, not four times. Lower it if the fill silhouette reads as stepped along near-horizontal stretches of border; raise it if the per-frame vertex count matters more than the last metre of silhouette.")]
	protected float m_fScanRowHeight;

	[Attribute("1", UIWidgets.Auto, "Trace region borders, and cut the fill out of them. With this OFF there are no contours, no neutral bands and no Chaikin, AND the fill falls back to merged grid squares - a blocky, axis-aligned silhouette that owes nothing to any contour. That path is a clean configuration rather than a degraded one, and it is the fallback if the traced border ever misbehaves.")]
	protected bool m_bTraceContours;

	[Attribute("1", UIWidgets.Auto, "Grow merged fill runs downward into rectangles where whole rows repeat. ONLY REACHED WITH m_bTraceContours OFF, since the contour fill is cut into scan rows instead of merged squares. Purely an optimisation - it changes no pixel - and it typically halves the quad count on the interior of a region while doing nothing for ragged coastal rows.")]
	protected bool m_bMergeRectsVertically;

	[Attribute("1", UIWidgets.Auto, "Stop territory at the shoreline. Rivers and inland lakes deliberately do NOT cut a region - the test compares terrain surface against OCEAN height.")]
	protected bool m_bClipToCoast;

	[Attribute("0.5", UIWidgets.Auto, "Metres of clearance above ocean height a point must have before it counts as land.")]
	protected float m_fShorelineMargin;

	//-----------------------------------------------------------------------
	// ATTRIBUTES - APPEARANCE
	//-----------------------------------------------------------------------

	[Attribute("70", UIWidgets.Auto, "Fill alpha (0-255) used by EVERY region of every faction when contested shading is off. One flat value per faction is the whole point: when one faction holds most of the island, alpha is the only thing left that can vary, and varying it turns a single territory into a patchwork of blobs.")]
	protected int m_iFillAlpha;

	[Attribute("1", UIWidgets.Auto, "Draw occupier-held regions at m_iContestedAlpha instead of m_iFillAlpha once popular support passes m_fContestedSupportThreshold. TWO STATES, never a gradient - a continuous per-site shade is what turned a one-faction island into a patchwork. This is the ONE switch for the contested signal.")]
	protected bool m_bContestedShading;

	[Attribute("95", UIWidgets.Auto, "Fill alpha (0-255) for a CONTESTED region - occupier-held, but with the population behind the resistance. Contested is drawn with the diagonal HATCH, and a hatch covers less than all of the area it fills, so the same alpha through a hatch lays down less ink than a solid fill: this sits ABOVE m_iFillAlpha on purpose, so a disputed region reads as at least as present as a firmly-held one rather than as a faded one. It stays below the band alpha so a frontier still reads as the edge.")]
	protected int m_iContestedAlpha;

	[Attribute("0.5", UIWidgets.Auto, "Fraction of a town's population that must support the resistance before the region reads as contested. 0.5 is half the town. Compared against the same figure the town info panel shows when the player clicks it.")]
	protected float m_fContestedSupportThreshold;

	[Attribute("80", UIWidgets.Auto, "Width in METRES of the neutral band drawn along an inter-faction frontier. It is a world distance, so it scales with zoom exactly as the fill does. KEEP IT BELOW THE SQUARE SIZE: the band is offset INWARD from the border, and a width larger than the border's local radius of curvature - which after smoothing is roughly half a square - folds the inner edge through itself and composites into a darker blob at every tight corner. Widen it only alongside a larger m_fGridCellSize or more smoothing passes.")]
	protected float m_fBandWidth;

	[Attribute("120", UIWidgets.Auto, "Alpha (0-255) of the neutral border band. Above the fill's own alpha on purpose - the band has to read as an edge against its own region. 0 disables the bands entirely.")]
	protected int m_iBandAlpha;

	[Attribute("1", UIWidgets.Auto, "Master switch for every texture this layer can draw. With it off, a contested region falls back to flat colour at m_iContestedAlpha and the band to flat colour, which is still distinguishable but loses the hatch idiom.")]
	protected bool m_bUseTextures;

	[Attribute("{B7E8255E75EE66ED}UI/Textures/Map/overthrow_map_diagonal.edds", UIWidgets.ResourceNamePicker, "Diagonal hatch drawn over CONTESTED regions. Held regions are solid: hatching is the conventional visual language for disputed ground, and it frees the fill's alpha from having to carry that meaning on its own. Only loaded when m_bUseTextures is on, and a failed load degrades to a flat contested fill rather than crashing.", "edds")]
	protected ResourceName m_sContestedTexture;

	[Attribute("{B7E8255E75EE66ED}UI/Textures/Map/overthrow_map_diagonal.edds", UIWidgets.ResourceNamePicker, "Hatch drawn along the neutral border band. Defaulted to the same art as the contested fill and separated from it by UV SCALE rather than by hue, because hue is spent entirely on faction identity. Only loaded when m_bUseTextures is on, and a failed load degrades to a flat band rather than crashing.", "edds")]
	protected ResourceName m_sBandTexture;

	[Attribute("0.01", UIWidgets.Auto, "UV scale for the contested hatch. The units are undocumented and unused anywhere in vanilla, so this is dialled in by eye. If the hatch SLIDES as the map is panned, that is UVs derived from screen-space vertices and no value here fixes it.")]
	protected float m_fUVScaleContested;

	[Attribute("0.02", UIWidgets.Auto, "UV scale for the neutral band hatch. Deliberately FINER than the contested fill's, because the band is a thin strip and shares its art: scale is what tells the two apart.")]
	protected float m_fUVScaleBand;

	//-----------------------------------------------------------------------
	// ATTRIBUTES - BEHAVIOUR
	//-----------------------------------------------------------------------

	[Attribute("1", UIWidgets.Auto, "Draw ONLY the ground the occupying faction still holds. Liberated ground reads as clean map, which turns the overlay into a progress bar for the campaign. Every other site still COMPETES for squares - it is just not drawn - so a captured town blanks its own region instead of recolouring it. Set 0 to draw every faction.")]
	protected bool m_bOnlyShowOccupying;

	[Attribute("1", UIWidgets.Auto, "Pack many quads into one TriMeshDrawCommand per colour instead of issuing one command per quad. This is what keeps the frame inside the composited command budget, and the geometry is identical either way - the indices are supplied, so the engine never triangulates anything. Set 0 to fall back to one convex PolygonDrawCommand per quad, which needs no engine feature at all and costs hundreds of commands.")]
	protected bool m_bBatchDrawCommands;

	[Attribute("5", UIWidgets.Auto, "Seconds between colour re-reads, matching the marker refresh. Also the interval at which a changed site set triggers a full re-solve.")]
	protected float m_fRefreshInterval;

	[Attribute("0", UIWidgets.Auto, "Print the PER-FRAME emit numbers - a rolling Draw() average and the composited command count - plus the tunables that produced them. The two solve summary lines print regardless, because a silently empty overlay is this feature's worst failure and it costs one log line to see.")]
	protected bool m_bDebugTiming;

	//-----------------------------------------------------------------------
	// CONSTANTS
	//-----------------------------------------------------------------------

	//! Frames averaged before the emit cost is reported.
	//!
	//! IT HAS TO BE A WINDOW, and the reason is the timer: System.GetTickCount is INTEGER
	//! MILLISECONDS, so a Draw() taking half a millisecond measures as 0 or 1 and never as 0.5. Over a
	//! window that quantisation dithers rather than biases - a 0.5 ms Draw straddles a millisecond
	//! boundary about half the time - so the SUM over 60 frames estimates the total honestly even
	//! though no single frame does. A per-frame print would be pure noise.
	protected const int DRAW_SAMPLE_FRAMES = 60;

	//! Quads packed into one batched draw command before a new one is started.
	//!
	//! A SAFETY VALVE AGAINST AN UNMEASURED ENGINE LIMIT, not a tuning knob. Nothing in this build has
	//! established how many vertices one TriMeshDrawCommand may carry, and the failure mode of finding
	//! out the hard way is a silently truncated or corrupt region. At 256 quads a command holds 1024
	//! vertices, which is comfortably ordinary for a mesh, and a whole island's fill still costs single
	//! figures of commands.
	protected const int MAX_QUADS_PER_COMMAND = 256;

	//! Cap on how far a mitred band corner may extend beyond the band's own width. Without it a hairpin
	//! in the border - which a raw unsmoothed staircase is full of - throws the inner vertex an
	//! unbounded distance across the map.
	protected const float MAX_BAND_MITRE = 2.0;

	//! How much further each town size projects. Hard-coded rather than configured deliberately: this
	//! is one multiply that makes a capital out-project a village, and the plan settles it as costing
	//! no new config surface.
	protected const float SIZE_FACTOR_VILLAGE = 0.75;
	protected const float SIZE_FACTOR_TOWN = 1.0;
	protected const float SIZE_FACTOR_CITY = 1.3;
	protected const float SIZE_FACTOR_CAPITAL = 1.6;

	//! Used when a town's size is not readable. See ResolveTownSizeFactor for why that is a real state.
	protected const float SIZE_FACTOR_UNKNOWN = 1.0;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	protected ref array<ref OVT_TerritorySite> m_aSites;
	protected ref OVT_TerritorySolver m_Solver;

	//! THE GEOMETRY. Rebuilt only when the site set changes.
	protected ref OVT_TerritoryGrid m_Grid;

	//! THE PRESENTATION. Rebuilt on every refresh tick, from the grid plus who currently holds what.
	//!
	//! THE CONTOURS ARE THE BOUNDARY AND THE TRAPEZOIDS ARE THE FILL CUT OUT OF THEM, which is why they
	//! are listed together: there is exactly one boundary per drawn region and it decides both. The
	//! rectangles are the m_bTraceContours-off fallback only, and are empty whenever contours are on.
	protected ref array<ref OVT_TerritoryContour> m_aContours;
	protected ref array<ref OVT_TerritoryTrapezoid> m_aTrapezoids;
	protected ref array<ref OVT_TerritoryRect> m_aRects;

	//! Appearance key per SITE index, or -1 where that site's ground is not drawn. This is the only
	//! thing the rect merger and the contour tracer know about colour, which is what keeps both of them
	//! pure integer compares.
	protected ref array<int> m_aSiteAppearance;

	//! The appearance table the keys index into. Small by construction - one entry per (faction, hatch)
	//! combination actually present - so building it by linear search costs nothing and avoids any
	//! chance of two different appearances colliding on a packed key.
	//!
	//! THE FACTION IS PART OF THE IDENTITY EVEN THOUGH IT DOES NOT CHANGE A PIXEL, and it has to be:
	//! regions are traced per appearance, so two factions that happened to resolve to the same colour
	//! would otherwise be traced as ONE region and the frontier between them would stop existing. That is
	//! D7's rule, and the only thing standing between it and a silent regression is this key.
	protected ref array<int> m_aAppearanceFaction;
	protected ref array<int> m_aAppearanceColour;
	protected ref array<bool> m_aAppearanceContested;

	//! Scratch reused by the contour build, one entry per site. Allocated at map open, refilled per
	//! traced faction, never allocated inside a loop.
	protected ref array<bool> m_aInRegion;
	protected ref array<bool> m_aRivalFaction;

	//! Contiguous frontier runs on the contour currently being emitted.
	protected ref array<int> m_aSpanStart;
	protected ref array<int> m_aSpanLength;

	//! World-space band strip scratch: the outer path is the contour itself, the inner path is it offset
	//! inward by the band width along a mitred normal.
	protected ref array<float> m_aStripInnerX;
	protected ref array<float> m_aStripInnerZ;

	//! The batch currently being filled, and its state. Non-null only between StartBatch and EndBatch.
	protected ref TriMeshDrawCommand m_Batch;
	protected int m_iBatchColour;
	protected int m_iBatchQuads;
	protected ref SharedItemRef m_BatchTexture;
	protected float m_fBatchUVScale;

	//! Identity of the current site SET - positions, types, weights and count, but deliberately NOT
	//! faction or support. See HashSites.
	protected int m_iSiteSetHash;

	protected float m_fRefreshTimer;

	//! Rolling emit measurements. Only accumulated when m_bDebugTiming is on - the map is open for a long
	//! time and this is the only path in the feature that runs every frame.
	protected int m_iDrawFrames;
	protected int m_iDrawTicks;

	//! Milliseconds the last CollectSites, grid build and presentation build took, reported together
	//! because all three happen inside transitions the player feels.
	protected int m_iLastCollectMs;
	protected int m_iLastGridMs;
	protected int m_iLastPresentationMs;

	//! Run count before the vertical merge, kept for the report - it is the only way to say what the
	//! merge actually bought on this map. Zero whenever contours are on, because the merger never ran.
	protected int m_iLastRunCount;

	protected ref SharedItemRef m_ContestedTextureRef;
	protected bool m_bContestedTextureAttempted;
	protected bool m_bContestedTextureValid;

	protected ref SharedItemRef m_BandTextureRef;
	protected bool m_bBandTextureAttempted;
	protected bool m_bBandTextureValid;

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	override void OnMapOpen(MapConfiguration config)
	{
		// Super resolves the canvas, allocates the command bucket and registers with the compositor.
		// Nothing below works without it.
		super.OnMapOpen(config);

		m_aSites = new array<ref OVT_TerritorySite>();
		m_aRects = new array<ref OVT_TerritoryRect>();
		m_aContours = new array<ref OVT_TerritoryContour>();
		m_aTrapezoids = new array<ref OVT_TerritoryTrapezoid>();

		m_aSiteAppearance = new array<int>();
		m_aAppearanceFaction = new array<int>();
		m_aAppearanceColour = new array<int>();
		m_aAppearanceContested = new array<bool>();

		m_aInRegion = new array<bool>();
		m_aRivalFaction = new array<bool>();

		m_aSpanStart = new array<int>();
		m_aSpanLength = new array<int>();

		m_aStripInnerX = new array<float>();
		m_aStripInnerZ = new array<float>();

		m_Batch = null;
		m_BatchTexture = null;

		m_ContestedTextureRef = null;
		m_bContestedTextureAttempted = false;
		m_bContestedTextureValid = false;

		m_BandTextureRef = null;
		m_bBandTextureAttempted = false;
		m_bBandTextureValid = false;

		m_fRefreshTimer = 0;
		m_iDrawFrames = 0;
		m_iDrawTicks = 0;
		m_iLastCollectMs = 0;
		m_iLastGridMs = 0;
		m_iLastPresentationMs = 0;
		m_iLastRunCount = 0;

		// Loaded HERE rather than on the first frame, even though Draw would also do it: the canvas is
		// already resolved by super, the load is idempotent behind its attempted flag, and doing it now
		// is what lets the solve report say truthfully whether the hatch is available.
		EnsureContestedTexture();
		EnsureBandTexture();

		BuildSolver();
		CollectSites();
		SolveAndCache();
	}

	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);

		// The map-close discipline the restricted-areas layer set: clear then null EVERY cached array,
		// so nothing survives to be drawn against a stale projection on the next open
		if (m_aSites)
			m_aSites.Clear();
		m_aSites = null;

		if (m_aRects)
			m_aRects.Clear();
		m_aRects = null;

		if (m_aContours)
			m_aContours.Clear();
		m_aContours = null;

		if (m_aTrapezoids)
			m_aTrapezoids.Clear();
		m_aTrapezoids = null;

		if (m_aSiteAppearance)
			m_aSiteAppearance.Clear();
		m_aSiteAppearance = null;

		if (m_aAppearanceFaction)
			m_aAppearanceFaction.Clear();
		m_aAppearanceFaction = null;

		if (m_aAppearanceColour)
			m_aAppearanceColour.Clear();
		m_aAppearanceColour = null;

		if (m_aAppearanceContested)
			m_aAppearanceContested.Clear();
		m_aAppearanceContested = null;

		if (m_aInRegion)
			m_aInRegion.Clear();
		m_aInRegion = null;

		if (m_aRivalFaction)
			m_aRivalFaction.Clear();
		m_aRivalFaction = null;

		if (m_aSpanStart)
			m_aSpanStart.Clear();
		m_aSpanStart = null;

		if (m_aSpanLength)
			m_aSpanLength.Clear();
		m_aSpanLength = null;

		if (m_aStripInnerX)
			m_aStripInnerX.Clear();
		m_aStripInnerX = null;

		if (m_aStripInnerZ)
			m_aStripInnerZ.Clear();
		m_aStripInnerZ = null;

		// Dropped, NOT emptied in place - a command from the final frame may still hold this batch, and
		// nulling the layer's own reference is all this side needs to do
		m_Batch = null;
		m_BatchTexture = null;

		m_Grid = null;
		m_Solver = null;

		m_ContestedTextureRef = null;
		m_bContestedTextureAttempted = false;
		m_bContestedTextureValid = false;

		m_BandTextureRef = null;
		m_bBandTextureAttempted = false;
		m_bBandTextureValid = false;

		m_iSiteSetHash = 0;
		m_fRefreshTimer = 0;
		m_iDrawFrames = 0;
		m_iDrawTicks = 0;
		m_iLastCollectMs = 0;
		m_iLastGridMs = 0;
		m_iLastPresentationMs = 0;
		m_iLastRunCount = 0;
	}

	override void Update(float timeSlice)
	{
		m_fRefreshTimer += timeSlice;

		if (m_fRefreshTimer >= m_fRefreshInterval)
		{
			m_fRefreshTimer = 0;
			RefreshTick();
		}

		// Super runs Draw() and submits this layer's bucket to the compositor. The submit is
		// unconditional there, so nothing above may return early.
		super.Update(timeSlice);
	}

	//-----------------------------------------------------------------------
	// COLLECT
	//-----------------------------------------------------------------------

	//! Rebuilds the site list from live campaign state.
	//!
	//! MANAGERS ARE LOOKED UP HERE, PER CALL, AND NEVER CACHED ON THIS LAYER. A reference cached during
	//! a join-in-progress window when a manager is not yet resolvable stays null for the whole map
	//! session and produces an empty overlay with no error at all; a per-call lookup self-heals on the
	//! next refresh tick, which is the same property the re-solve trigger relies on.
	void CollectSites()
	{
		// Timed unconditionally, reported with the solve. It is two tick reads against four manager
		// walks, and a collect that has gone slow is indistinguishable from a slow grid without it.
		int startTick = System.GetTickCount();

		if (!m_aSites)
			m_aSites = new array<ref OVT_TerritorySite>();

		m_aSites.Clear();

		CollectTowns();
		CollectBases();
		CollectRadioTowers();
		CollectFOBs();

		m_iLastCollectMs = System.GetTickCount() - startTick;

		// CAMPS ARE DELIBERATELY EXCLUDED AND MUST STAY EXCLUDED. A camp belongs to one player and can
		// be marked private; drawing a territory region around one would broadcast that player's
		// private position to every client on the server. That is the exact defect class the map
		// location-type work already had to fix once. This is not an oversight and it is not a
		// to-do - do not add a CollectCamps().

		// AND NOTHING HERE MAY EVER FILTER BY FACTION. The overlay draws only occupier-held ground, but
		// that decision belongs on the emit path and nowhere else: a resistance FOB, a liberated town
		// and a captured base all still have to COMPETE here, because competing is what pushes the
		// occupier's boundary back and makes liberated ground go blank. Drop them from this list and
		// occupier territory flows straight over everything the player has taken - silently, with the
		// solve log still reporting a healthy site count for the sites that remain.
	}

	//! Towns: live controlling faction, popular support, and a size factor so a capital out-projects a
	//! village. Towns are the ONLY site type with support, and therefore the only one that can be drawn
	//! as contested.
	protected void CollectTowns()
	{
		OVT_TerritorySiteConfig typeConfig = GetSiteTypeConfig("town");
		if (!typeConfig)
			return;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns)
			return;

		foreach (OVT_TownData town : towns.m_Towns)
		{
			if (!town)
				continue;

			AddSite(typeConfig, town.location, GetSiteFactionIndex(town.faction), ResolveTownSupportFraction(town), ResolveTownSizeFactor(town));
		}
	}

	//! Military bases: no popular support to measure, so they are binary held and never contested.
	protected void CollectBases()
	{
		OVT_TerritorySiteConfig typeConfig = GetSiteTypeConfig("base");
		if (!typeConfig)
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases)
			return;

		foreach (OVT_BaseData base : occupying.m_Bases)
		{
			if (!base)
				continue;

			AddSite(typeConfig, base.location, GetSiteFactionIndex(base.faction), OVT_TerritorySite.SUPPORT_NONE, 1.0);
		}
	}

	//! Radio towers: no popular support to measure, so they are binary held and never contested.
	protected void CollectRadioTowers()
	{
		OVT_TerritorySiteConfig typeConfig = GetSiteTypeConfig("radiotower");
		if (!typeConfig)
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_RadioTowers)
			return;

		foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
		{
			if (!tower)
				continue;

			AddSite(typeConfig, tower.location, GetSiteFactionIndex(tower.faction), OVT_TerritorySite.SUPPORT_NONE, 1.0);
		}
	}

	//! Resistance FOBs. An FOB record carries NO faction field - FOBs are implicitly resistance-owned
	//! and their marker colour comes from the config path - so the player faction index is used, which
	//! resolves to the same Faction object and therefore the same colour.
	protected void CollectFOBs()
	{
		OVT_TerritorySiteConfig typeConfig = GetSiteTypeConfig("fob");
		if (!typeConfig)
			return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_FOBs)
			return;

		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
			return;

		int resistanceFaction = overthrowConfig.GetPlayerFactionIndex();

		foreach (OVT_FOBData fob : resistance.m_FOBs)
		{
			if (!fob)
				continue;

			AddSite(typeConfig, fob.location, GetSiteFactionIndex(resistanceFaction), OVT_TerritorySite.SUPPORT_NONE, 1.0);
		}
	}

	//! THE ONE PLACE A SITE IS CREATED. Every collector funnels through here, which is the seam a
	//! future intel epic extends: skipping a site whose control the viewing player does not know about
	//! is a change to this method and nothing else. No unknown-state handling ships today.
	//! \param[in] typeConfig The site type's config record.
	//! \param[in] pos World position of the site.
	//! \param[in] factionIndex Controlling faction index.
	//! \param[in] supportFraction Resistance support as a fraction of population, 0..1, or
	//!            OVT_TerritorySite.SUPPORT_NONE for a type that has no support to measure.
	//! \param[in] sizeFactor Per-site multiplier on the type weight. 1 for every non-town type.
	protected void AddSite(OVT_TerritorySiteConfig typeConfig, vector pos, int factionIndex, float supportFraction, float sizeFactor)
	{
		if (!typeConfig || !typeConfig.m_bEnabled)
			return;

		// THE ONLY DISQUALIFICATION, and it must stay the only one: a non-positive weight is the single
		// thing the ownership test refuses to consider, so a site that would be silently invisible to
		// the grid is refused a place in the array the grid's owner indices point into.
		float weight = typeConfig.m_fWeight * sizeFactor;
		if (weight <= 0)
			return;

		OVT_TerritorySite site = new OVT_TerritorySite();
		site.m_vPos = pos;
		site.m_fWeight = weight;
		site.m_iFactionIndex = factionIndex;

		// SUPPORT_NONE deliberately survives the clamp. It is a sentinel meaning "this type has no
		// support to measure", not a value, and clamping it to 0 would make a military base
		// indistinguishable from a town nobody supports.
		float support = supportFraction;
		if (support >= 0)
			support = Math.Clamp(support, 0, 1);

		site.m_fSupportFraction = support;
		site.m_sTypeId = typeConfig.m_sId;

		m_aSites.Insert(site);
	}

	//! THE ONE PLACE A SITE'S CONTROLLING FACTION IS DECIDED. Today it is an identity function over the
	//! record's own faction field, which is what makes territory colour agree with marker colour by
	//! construction rather than by coincidence. The future intel epic returns -1 here for "control
	//! unknown to this player" and touches nothing else.
	//! \param[in] recordFactionIndex The faction index carried by the underlying campaign record.
	//! \return The faction index the overlay should colour this site with.
	protected int GetSiteFactionIndex(int recordFactionIndex)
	{
		return recordFactionIndex;
	}

	//! What fraction of a town's population backs the resistance, 0..1.
	//!
	//! COMPUTED HERE RATHER THAN THROUGH THE TOWN RECORD'S OWN PERCENTAGE HELPER, deliberately. This is
	//! the same expression the town info panel uses to print the percentage the player is shown when
	//! they click the town - support multiplied by a FLOAT literal first, then divided - so the shading
	//! threshold and the number on screen cannot disagree about what "half the town" means. The
	//! multiply-then-divide-back is what forces the promotion; written as a bare support/population it
	//! would be an integer division in a pure-integer expression.
	//!
	//! A town with no population has no support to measure, which is the SUPPORT_NONE state rather than
	//! zero support - and it is also the divide-by-zero guard.
	//! \param[in] town The town record.
	//! \return The support fraction, or OVT_TerritorySite.SUPPORT_NONE.
	protected float ResolveTownSupportFraction(OVT_TownData town)
	{
		if (!town)
			return OVT_TerritorySite.SUPPORT_NONE;

		if (town.population <= 0)
			return OVT_TerritorySite.SUPPORT_NONE;

		float supportPercentage = (town.support * 100.0) / town.population;

		return supportPercentage * 0.01;
	}

	//! How much further a town projects for its size.
	//!
	//! A town's size is NOT serialised - it is populated locally from the town controller prefab - so a
	//! client can legitimately read 0 or an out-of-range value during a join-in-progress window.
	//! Falling back to 1.0 is deliberate: a zero factor would produce a zero weight, which erases that
	//! town's region entirely. A slightly-too-small capital is a far better failure than a missing town.
	//! \param[in] town The town record.
	//! \return The size multiplier applied to the town type weight.
	protected float ResolveTownSizeFactor(OVT_TownData town)
	{
		if (!town)
			return SIZE_FACTOR_UNKNOWN;

		switch (town.size)
		{
			case OVT_TownSize.VILLAGE: return SIZE_FACTOR_VILLAGE;
			case OVT_TownSize.TOWN: return SIZE_FACTOR_TOWN;
			case OVT_TownSize.CITY: return SIZE_FACTOR_CITY;
			case OVT_TownSize.CAPITAL: return SIZE_FACTOR_CAPITAL;
		}

		return SIZE_FACTOR_UNKNOWN;
	}

	//! Looks up a site type's config record by id.
	//! \param[in] id The type id, e.g. "town".
	//! \return The record, or null when the config carries none for this id.
	protected OVT_TerritorySiteConfig GetSiteTypeConfig(string id)
	{
		if (!m_aSiteTypes)
			return null;

		foreach (OVT_TerritorySiteConfig typeConfig : m_aSiteTypes)
		{
			if (!typeConfig)
				continue;

			if (typeConfig.m_sId == id)
				return typeConfig;
		}

		return null;
	}

	//-----------------------------------------------------------------------
	// SOLVE
	//-----------------------------------------------------------------------

	//! Builds the solver and pushes the config knobs onto it. The solver owns no config of its own -
	//! attributes there would make it a container-loaded object and cost it its automated coverage.
	protected void BuildSolver()
	{
		m_Solver = new OVT_TerritorySolver();
		m_Solver.m_fGridCellSize = m_fGridCellSize;
		m_Solver.m_iSmoothPasses = m_iSmoothPasses;
		m_Solver.m_fShorelineMargin = m_fShorelineMargin;
		m_Solver.m_bClipToCoast = m_bClipToCoast;
		m_Solver.SetWorld(GetGame().GetWorld());
	}

	//! Classifies the whole grid and rebuilds everything downstream of it. Once per map open, plus
	//! whenever the site set changes underneath an open map.
	protected void SolveAndCache()
	{
		if (!m_Solver)
			BuildSolver();

		int startTick = System.GetTickCount();

		m_Grid = m_Solver.BuildGrid(m_aSites);

		m_iLastGridMs = System.GetTickCount() - startTick;

		RebuildPresentation();

		m_iSiteSetHash = HashSites();

		ReportSolve();
	}

	//! Re-reads faction, support and visibility for every site, then rebuilds the traced borders and the
	//! fill cut out of them, from the UNCHANGED grid.
	//!
	//! THE SPLIT THIS PRESERVES. Classifying the grid is the expensive half and depends only on where
	//! the sites ARE, so faction is deliberately excluded from the site-set hash and a capture never
	//! re-classifies a square. What a capture does change is which squares are drawn, in what colour,
	//! and where the borders between factions fall - so the contours and the fill are rebuilt here, on
	//! the slow tick, off an array that is already in memory. That is a pass over the grid rather than a
	//! pass over the grid times the sites, which is the order-of-magnitude that matters.
	//!
	//! THE BORDER IS BUILT FIRST BECAUSE THE FILL IS CUT OUT OF IT. That ordering is the whole point of
	//! this pass: one boundary per drawn region, used twice, so the fill and the band cannot disagree.
	//! Only with contours switched off does the fill fall back to merged grid squares, which owe nothing
	//! to any contour and are square-edged by construction.
	protected void RebuildPresentation()
	{
		int startTick = System.GetTickCount();

		ResolveAppearance();

		BuildContours();

		if (m_bTraceContours)
		{
			m_iLastRunCount = 0;
			m_aRects.Clear();

			BuildFillTrapezoids();
		}
		else
		{
			m_aTrapezoids.Clear();

			m_iLastRunCount = OVT_TerritorySolver.BuildRects(m_Grid, m_aSiteAppearance, m_bMergeRectsVertically, m_aRects);
		}

		m_iLastPresentationMs = System.GetTickCount() - startTick;
	}

	//! Cuts every drawn region's fill out of its own smoothed contour.
	//!
	//! ONE CALL PER APPEARANCE, IN KEY ORDER, which is what leaves the output already grouped by draw
	//! state - so the per-frame emit is one pass that opens a batch when the key changes rather than one
	//! pass per appearance over the whole list.
	protected void BuildFillTrapezoids()
	{
		m_aTrapezoids.Clear();

		if (!m_aContours || m_aContours.IsEmpty())
			return;

		for (int key = 0; key < m_aAppearanceColour.Count(); key++)
		{
			OVT_TerritorySolver.BuildTrapezoids(m_aContours, key, m_fScanRowHeight, m_aTrapezoids);
		}
	}

	//! Resolves what each site's ground looks like, and compacts the answers into a small table the
	//! tracer and the merger can compare with a single integer.
	//!
	//! A REGION IS AN APPEARANCE, NOT A SITE, and that is what makes the geometry affordable: two
	//! neighbouring towns of one faction draw identically, so they are one region with no border between
	//! them and a row crossing six of them costs one span rather than six. Keying on the site would
	//! multiply the geometry by the number of sites a row crosses AND draw a border down the middle of
	//! every faction's own ground, for no visible benefit whatsoever.
	protected void ResolveAppearance()
	{
		m_aSiteAppearance.Clear();
		m_aAppearanceFaction.Clear();
		m_aAppearanceColour.Clear();
		m_aAppearanceContested.Clear();

		if (!m_aSites)
			return;

		// PER CALL, NEVER CACHED ON THE LAYER - the same rule every manager lookup in this file follows.
		// A value cached during a join-in-progress window when the config is not yet resolvable would
		// stay wrong for the whole map session; re-reading it heals on the next tick.
		int occupyingFaction = ResolveOccupyingFactionIndex();

		foreach (OVT_TerritorySite site : m_aSites)
		{
			if (!site)
			{
				m_aSiteAppearance.Insert(-1);
				continue;
			}

			if (!OVT_TerritorySolver.IsEmittedCell(site.m_iFactionIndex, occupyingFaction, m_bOnlyShowOccupying))
			{
				// NOT DRAWN - but the grid still gives this site its squares, which is what makes the
				// ground it holds read as clean map instead of as its neighbour's.
				m_aSiteAppearance.Insert(-1);
				continue;
			}

			// RESOLVED ONCE AND CARRIED, not asked twice. The same answer drives the alpha and the hatch,
			// and evaluating the rule separately for each would let the two disagree the moment one call
			// site was edited - a region drawn at contested alpha but without the hatch, or the reverse,
			// reads as a rendering fault rather than as a state.
			bool contested = m_bContestedShading && OVT_TerritorySolver.IsContestedCell(site.m_iFactionIndex, occupyingFaction, site.m_fSupportFraction, m_fContestedSupportThreshold);

			int alpha = ResolveFillAlpha(contested);
			int colour = ResolveFactionArgb(site.m_iFactionIndex, alpha);

			m_aSiteAppearance.Insert(FindOrAddAppearance(site.m_iFactionIndex, colour, contested));
		}
	}

	//! Finds an appearance in the table, adding it if it is new.
	//!
	//! THE FACTION IS PART OF THE KEY EVEN THOUGH IT CHANGES NO PIXEL. Regions are traced per appearance,
	//! so two factions resolving to the same colour - which the white unresolved-faction fallback makes
	//! reachable - would otherwise be traced as one region and the frontier between them would simply
	//! cease to exist. That is D7's rule quietly lost, with nothing on screen to say so.
	//! \param[in] factionIndex Controlling faction.
	//! \param[in] colour Packed ARGB fill colour.
	//! \param[in] contested Whether this appearance carries the hatch.
	//! \return Index into the appearance table.
	protected int FindOrAddAppearance(int factionIndex, int colour, bool contested)
	{
		for (int i = 0; i < m_aAppearanceColour.Count(); i++)
		{
			if (m_aAppearanceFaction[i] == factionIndex && m_aAppearanceColour[i] == colour && m_aAppearanceContested[i] == contested)
				return i;
		}

		m_aAppearanceFaction.Insert(factionIndex);
		m_aAppearanceColour.Insert(colour);
		m_aAppearanceContested.Insert(contested);

		return m_aAppearanceColour.Count() - 1;
	}

	//! Traces one closed border loop set per DRAWN APPEARANCE, and smooths each one.
	//!
	//! WHY THE REGION IS AN APPEARANCE AND NOT A SITE. A border between two towns of the same faction
	//! that draw the same is not a border at all - the two are one territory and must read as one - so
	//! tracing per appearance makes that structural instead of a rule somebody has to remember to apply.
	//!
	//! AND WHY AN APPEARANCE AND NOT A FACTION, which is what it used to be. The fill is cut out of these
	//! contours, and a fill has to know its own colour: a faction's firmly-held ground and its contested
	//! ground are two different washes, so one loop around both of them cannot bound either. Tracing per
	//! appearance gives every drawn thing exactly one boundary, which the fill and the band then share.
	//!
	//! THE BAND RULE IS UNTOUCHED BY THAT SPLIT, and this is the part worth checking rather than
	//! assuming. The seam between one faction's held and contested ground is now a real contour where it
	//! used to be nothing at all - but it is a SAME-FACTION boundary, so IsFrontierSide answers no on
	//! both sides of it and no band is drawn. Everything else on the far side of a contour is still
	//! either water or another faction's, exactly as before.
	protected void BuildContours()
	{
		m_aContours.Clear();

		if (!m_bTraceContours)
			return;

		if (!m_Grid || !m_aSites)
			return;

		int siteCount = m_aSites.Count();
		if (siteCount < 1)
			return;

		for (int key = 0; key < m_aAppearanceColour.Count(); key++)
		{
			int faction = m_aAppearanceFaction[key];

			m_aInRegion.Clear();
			m_aRivalFaction.Clear();

			for (int s = 0; s < siteCount; s++)
			{
				bool inside = m_aSiteAppearance[s] == key;

				m_aInRegion.Insert(inside);

				// THE BAND RULE, evaluated once per site rather than once per contour segment. Water is
				// not in this array at all - it has no owner - so a coastline can never be a frontier.
				m_aRivalFaction.Insert(OVT_TerritorySolver.IsFrontierSide(s, faction, m_aSites));
			}

			int before = m_aContours.Count();

			OVT_TerritorySolver.TraceRegion(m_Grid, m_aInRegion, m_aRivalFaction, m_aContours);

			int bandColour = ResolveFactionArgb(faction, m_iBandAlpha);

			for (int k = before; k < m_aContours.Count(); k++)
			{
				OVT_TerritoryContour contour = m_aContours[k];
				contour.m_iFactionIndex = faction;
				contour.m_iAppearanceKey = key;

				// Resolved here, once per map session per appearance, so the per-frame path never asks
				// the faction manager anything.
				contour.m_iBandColour = bandColour;

				OVT_TerritorySolver.ChaikinSmooth(contour, m_iSmoothPasses);
			}
		}
	}

	//! The occupying faction's index, or -1 when it cannot be resolved right now.
	//!
	//! Negative is a real answer rather than an error: the emit and contested rules both treat an
	//! unknown occupier as "do not filter, do not mark", so a client whose config has not resolved yet
	//! draws every faction rather than an empty map.
	//! \return The occupying faction index, or -1.
	protected int ResolveOccupyingFactionIndex()
	{
		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
			return -1;

		return overthrowConfig.GetOccupyingFactionIndex();
	}

	//! The fill alpha one region is drawn at: TWO VALUES, never a range.
	//!
	//! Fill alpha was once interpolated from each site's stability, which reads well on a contested map
	//! and reads as damage on a settled one: when a single faction holds nearly everything, hue carries
	//! no information at all and alpha is the only variable left, so neighbouring regions that are one
	//! territory in every sense a player cares about render as dozens of differently-shaded blobs.
	//!
	//! WHAT REPLACED IT IS A THRESHOLD, and it survives the objection the gradient did not. A region
	//! whose population has gone over to the resistance while the occupier still garrisons it is still
	//! drawn in the OCCUPIER'S colour, because the occupier does control it. Support does not recolour
	//! the region, it marks it - so this is a second axis over an accurate first one rather than a
	//! second opinion about who holds what.
	//!
	//! AND ALPHA IS NO LONGER THE ONLY THING THAT SAYS IT. Contested regions are drawn with a diagonal
	//! HATCH, so the contested value sits ABOVE the held one: a hatch covers only part of the area it
	//! fills, and equal alphas would make the contested region the quieter one.
	//! \param[in] contested Whether this region counts as contested.
	//! \return Fill alpha, 0-255.
	protected int ResolveFillAlpha(bool contested)
	{
		int alpha = m_iFillAlpha;

		if (contested)
			alpha = m_iContestedAlpha;

		return Math.ClampInt(alpha, 0, 255);
	}

	//! The slow tick that makes the overlay agree with a campaign that is still changing.
	//!
	//! Who holds what changes constantly and is re-read every interval. The GRID depends only on where
	//! the sites are, so it is re-classified solely when the site set itself changed - which is also the
	//! join-in-progress fix: a client whose managers had not replicated at map open sees an empty
	//! overlay fill in within one interval, without closing the map.
	protected void RefreshTick()
	{
		CollectSites();

		int hash = HashSites();
		if (hash != m_iSiteSetHash)
		{
			SolveAndCache();
			return;
		}

		RebuildPresentation();

		if (m_bDebugTiming)
			Print(string.Format("[OVT_Territory] recolour: %1 ms, %2 contours -> %3 trapezoids (%4 runs -> %5 fallback rects)", m_iLastPresentationMs, m_aContours.Count(), m_aTrapezoids.Count(), m_iLastRunCount, m_aRects.Count()), LogLevel.NORMAL);
	}

	//! Identity of the current site SET.
	//!
	//! Composed from the site count and, per site, its quantised world X and Z, its type id and its
	//! quantised weight. It therefore changes when a site is created, destroyed or moved, and when a
	//! town's size finally becomes readable and changes its weight.
	//!
	//! It deliberately does NOT include faction or support: those change on every capture, and folding
	//! them in would re-classify the whole grid for a change that only ever alters which squares are
	//! drawn and in what colour.
	//! \return A hash of the site set.
	protected int HashSites()
	{
		int hash = 17;

		if (!m_aSites)
			return hash;

		hash = hash * 31 + m_aSites.Count();

		foreach (OVT_TerritorySite site : m_aSites)
		{
			if (!site)
				continue;

			hash = hash * 31 + (int)Math.Floor(site.m_vPos[0]);
			hash = hash * 31 + (int)Math.Floor(site.m_vPos[2]);
			hash = hash * 31 + (int)Math.Floor(site.m_fWeight * 100);
			hash = hash * 31 + site.m_sTypeId.Hash();
		}

		return hash;
	}

	//! Two lines per solve saying what the grid found and what came out of it. Print is the only
	//! debugger available, and a silently empty overlay is exactly the failure this feature is hardest
	//! to see.
	//!
	//! IT IS NOT GATED BEHIND m_bDebugTiming, and that is deliberate. Two lines at map open cost
	//! nothing, and they are the only way anyone - user or developer - can tell an overlay that found
	//! no sites from one that found them and drew nothing. The PER-FRAME numbers are gated, because
	//! those genuinely are a measurement artefact and they print forever.
	//!
	//! THE NUMBERS THAT DIAGNOSE THINGS. Sites far above drawn appearances is the overlay working as
	//! designed on a mostly-liberated island; the SITE count itself collapsing is somebody filtering at
	//! collect time, which floods liberated ground with occupier colour. A cell size larger than the one
	//! configured means the square-count ceiling grew it. Trapezoids far above contour points means the
	//! scan rows are finer than the border needs them to be, which is the one lever for the per-frame
	//! vertex count. Trapezoids at ZERO with contours present means the fill lost its boundary, which is
	//! the failure this whole representation exists to make impossible.
	protected void ReportSolve()
	{
		int siteCount = 0;
		if (m_aSites)
			siteCount = m_aSites.Count();

		int cols = 0;
		int rows = 0;
		float cellSize = 0;
		float originX = 0;
		float originZ = 0;
		int ownedCells = 0;
		int unownedCells = 0;

		if (m_Grid)
		{
			cols = m_Grid.m_iCols;
			rows = m_Grid.m_iRows;
			cellSize = m_Grid.m_fCellSize;

			// Reported because it is the only way to see that the grid landed on the map's own grid lines
			// rather than beside them. Both numbers must be whole multiples of the square size.
			originX = m_Grid.m_fOriginX;
			originZ = m_Grid.m_fOriginZ;

			if (m_Grid.m_aOwner)
			{
				foreach (int owner : m_Grid.m_aOwner)
				{
					if (owner >= 0)
						ownedCells++;
					else
						unownedCells++;
				}
			}
		}

		int contourPoints = 0;
		int frontierSegments = 0;

		if (m_aContours)
		{
			foreach (OVT_TerritoryContour contour : m_aContours)
			{
				if (!contour || !contour.m_aFrontier)
					continue;

				contourPoints += contour.m_aX.Count();

				foreach (bool frontier : contour.m_aFrontier)
				{
					if (frontier)
						frontierSegments++;
				}
			}
		}

		Print(string.Format("[OVT_Territory] grid: %1 sites, %2 x %3 squares at %4 m from origin %5, %6 (%7 owned, %8 water/unowned), collect %9 ms", siteCount, cols, rows, cellSize, originX, originZ, ownedCells, unownedCells, m_iLastCollectMs), LogLevel.NORMAL);
		Print(string.Format("[OVT_Territory] draw set: %1 appearances, %2 contours (%3 points, %4 frontier segments) -> %5 trapezoids at %6 m rows, %7 fallback rects, classify %8 ms, build %9 ms", m_aAppearanceColour.Count(), m_aContours.Count(), contourPoints, frontierSegments, m_aTrapezoids.Count(), m_fScanRowHeight, m_aRects.Count(), m_iLastGridMs, m_iLastPresentationMs), LogLevel.NORMAL);

		// The tunables that produced the two lines above, so a pasted-back measurement says what it was
		// measured at rather than needing the conf file alongside it
		if (m_bDebugTiming)
			Print(string.Format("[OVT_Territory] tunables: %1 m squares, %2 m scan rows, %3 smooth passes, contours %4, vertical merge %5, batching %6, band %7 m / alpha %8, occupier only %9", m_fGridCellSize, m_fScanRowHeight, m_iSmoothPasses, m_bTraceContours, m_bMergeRectsVertically, m_bBatchDrawCommands, m_fBandWidth, m_iBandAlpha, m_bOnlyShowOccupying), LogLevel.NORMAL);

		if (m_bDebugTiming)
			Print(string.Format("[OVT_Territory] textures: contested %1 at uv %2, band %3 at uv %4, shading %5, fill alpha %6, contested alpha %7, support threshold %8", m_bContestedTextureValid, m_fUVScaleContested, m_bBandTextureValid, m_fUVScaleBand, m_bContestedShading, m_iFillAlpha, m_iContestedAlpha, m_fContestedSupportThreshold), LogLevel.NORMAL);
	}

	//-----------------------------------------------------------------------
	// DRAW
	//-----------------------------------------------------------------------

	override void Draw()
	{
		int startTick = 0;
		int composited = 0;

		if (m_bDebugTiming)
		{
			startTick = System.GetTickCount();

			// Read BEFORE this frame's commands are built, so it reports the PREVIOUS frame's finished
			// composite - every layer, rings included. Read after our own submit it would only ever see
			// the layers that happen to update before this one.
			OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
			if (compositor)
				composited = compositor.GetCompositedCommandCount();
		}

		if (!m_Commands)
			return;

		m_Commands.Clear();

		// Dropped alongside the bucket it would have been submitted into. Without this, a frame that
		// bailed below could leave a half-filled batch holding LAST frame's screen coordinates, and the
		// next StartBatch would flush it into the new bucket - a stale region drawn where the map used
		// to be.
		m_Batch = null;

		if (!m_Grid || m_Grid.CellCount() < 1)
			return;

		// ONCE per frame, not once per vertex: three WorldToScreen calls buy an affine basis that was
		// measured exact, so every vertex costs two mul-adds instead of a proto call
		CacheProjection();

		EnsureContestedTexture();
		EnsureBandTexture();

		EmitFills();

		if (m_bTraceContours)
			EmitBands();

		if (m_bDebugTiming)
			ReportDraw(System.GetTickCount() - startTick, composited);
	}

	//! Accumulates one frame's emit cost and reports the window average.
	//!
	//! THE AVERAGE IS THE MEASUREMENT, not the per-frame number: System.GetTickCount is integer
	//! milliseconds, so a sub-millisecond Draw reads as 0 or 1 every frame and only the sum over a
	//! window means anything. See DRAW_SAMPLE_FRAMES.
	//! \param[in] elapsedMs This frame's Draw() cost, quantised to whole milliseconds.
	//! \param[in] composited Commands in the previous frame's finished composite, every layer included.
	protected void ReportDraw(int elapsedMs, int composited)
	{
		m_iDrawTicks += elapsedMs;
		m_iDrawFrames++;

		if (m_iDrawFrames < DRAW_SAMPLE_FRAMES)
			return;

		// Assigned to a float BEFORE the division: int / int truncates, and a 0.6 ms average would
		// report as 0
		float averageMs = m_iDrawTicks;
		averageMs = averageMs / DRAW_SAMPLE_FRAMES;
		averageMs = Math.Round(averageMs * 100) / 100.0;

		int rectCount = 0;
		if (m_aRects)
			rectCount = m_aRects.Count();

		int ownCommands = 0;
		if (m_Commands)
			ownCommands = m_Commands.Count();

		Print(string.Format("[OVT_Territory] draw: %1 ms average over %2 frames (%3 ms total), %4 rects, %5 own commands, %6 composited last frame (rings included)", averageMs, DRAW_SAMPLE_FRAMES, m_iDrawTicks, rectCount, ownCommands, composited), LogLevel.NORMAL);

		m_iDrawTicks = 0;
		m_iDrawFrames = 0;
	}

	//! Projects and packs the fill, from whichever of the two representations the config selected.
	protected void EmitFills()
	{
		if (m_bTraceContours)
		{
			EmitTrapezoidFills();
			return;
		}

		EmitRectFills();
	}

	//! Projects and packs every trapezoid of the contour-cut fill.
	//!
	//! ONE PASS, NOT ONE PER APPEARANCE. The trapezoids come out of the build already grouped by key,
	//! because the build runs once per appearance in key order, so a batch is opened whenever the key
	//! changes and the whole fill is walked exactly once a frame.
	//!
	//! EVERY TRAPEZOID IS CONVEX - its two horizontal edges cannot cross and its sides cannot either -
	//! so the draw-state batching into a TriMeshDrawCommand is as valid here as it was for rectangles.
	//! Textures work on that command in this build, so nothing about the hatch has to change either.
	protected void EmitTrapezoidFills()
	{
		if (!m_aTrapezoids || m_aTrapezoids.IsEmpty())
			return;

		int openKey = -1;
		bool open = false;

		foreach (OVT_TerritoryTrapezoid trap : m_aTrapezoids)
		{
			if (trap.m_iKey < 0 || trap.m_iKey >= m_aAppearanceColour.Count())
				continue;

			if (!open || trap.m_iKey != openKey)
			{
				SharedItemRef texture = null;
				float uvScale = 0;

				if (m_bContestedTextureValid && m_aAppearanceContested[trap.m_iKey])
				{
					texture = m_ContestedTextureRef;
					uvScale = m_fUVScaleContested;
				}

				StartBatch(m_aAppearanceColour[trap.m_iKey], texture, uvScale);

				openKey = trap.m_iKey;
				open = true;
			}

			EmitWorldQuad(trap.m_fTopLeftX, trap.m_fTopZ, trap.m_fTopRightX, trap.m_fTopZ, trap.m_fBottomRightX, trap.m_fBottomZ, trap.m_fBottomLeftX, trap.m_fBottomZ);
		}

		if (open)
			EndBatch();
	}

	//! Projects and packs every merged grid rectangle, GROUPED BY APPEARANCE so a whole region's fill
	//! costs a handful of draw commands.
	//!
	//! THE m_bTraceContours-OFF FALLBACK ONLY. This is the square-edged fill that owes nothing to any
	//! contour - the one whose staircase silhouette disagreed with the smooth band drawn over it - and it
	//! is kept because it needs no traced border at all and is therefore the thing to fall back to if the
	//! tracer ever misbehaves.
	//!
	//! The outer loop is over appearances rather than over rectangles because a batch may only carry one
	//! colour and one texture, and the merger does not group its output.
	protected void EmitRectFills()
	{
		if (!m_aRects || m_aRects.IsEmpty())
			return;

		for (int key = 0; key < m_aAppearanceColour.Count(); key++)
		{
			bool anyThisKey = false;

			foreach (OVT_TerritoryRect rect : m_aRects)
			{
				if (rect.m_iKey != key)
					continue;

				if (!anyThisKey)
				{
					SharedItemRef texture = null;
					float uvScale = 0;

					if (m_bContestedTextureValid && m_aAppearanceContested[key])
					{
						texture = m_ContestedTextureRef;
						uvScale = m_fUVScaleContested;
					}

					StartBatch(m_aAppearanceColour[key], texture, uvScale);
					anyThisKey = true;
				}

				float x0 = m_Grid.EdgeX(rect.m_iCol0);
				float x1 = m_Grid.EdgeX(rect.m_iCol1 + 1);
				float z0 = m_Grid.EdgeZ(rect.m_iRow0);
				float z1 = m_Grid.EdgeZ(rect.m_iRow1 + 1);

				EmitWorldQuad(x0, z0, x1, z0, x1, z1, x0, z1);
			}

			if (anyThisKey)
				EndBatch();
		}
	}

	//! Emits the neutral band along every inter-faction frontier run of every traced border.
	//!
	//! THE BAND IS ONLY EVER ON A FRONTIER. A stretch of border against water carries none - a coastline
	//! is nobody's frontier - and a border between two regions of the SAME faction carries none either,
	//! which now covers the seam between a faction's held and contested ground as well as the ground
	//! between two of its towns.
	//!
	//! BATCHED PER APPEARANCE, NOT PER LOOP. Every contour of one appearance shares a band colour and a
	//! texture, and they come out of the build grouped by key, so a whole faction's frontiers cost one
	//! command rather than one per loop. That matters more than it used to: a region with pockets in it
	//! has many loops, and one command each would put the count into the hundreds on a busy map.
	protected void EmitBands()
	{
		if (!m_aContours || m_aContours.IsEmpty())
			return;

		if (m_iBandAlpha <= 0 || m_fBandWidth <= 0)
			return;

		SharedItemRef texture = null;
		float uvScale = 0;

		if (m_bBandTextureValid)
		{
			texture = m_BandTextureRef;
			uvScale = m_fUVScaleBand;
		}

		int openKey = -1;
		bool open = false;

		foreach (OVT_TerritoryContour contour : m_aContours)
		{
			if (!contour || !contour.m_aFrontier)
				continue;

			OVT_TerritorySolver.CollectFrontierSpans(contour.m_aFrontier, m_aSpanStart, m_aSpanLength);

			if (m_aSpanStart.IsEmpty())
				continue;

			if (!open || contour.m_iAppearanceKey != openKey)
			{
				StartBatch(contour.m_iBandColour, texture, uvScale);

				openKey = contour.m_iAppearanceKey;
				open = true;
			}

			for (int s = 0; s < m_aSpanStart.Count(); s++)
			{
				EmitBandSpan(contour, m_aSpanStart[s], m_aSpanLength[s]);
			}
		}

		if (open)
			EndBatch();
	}

	//! Emits one contiguous frontier run as a strip of convex quads between the border and its inward
	//! offset.
	//!
	//! THE OFFSET IS MITRED, not per segment: the inner vertex at each point is pushed along the AVERAGE
	//! of the two adjacent segment normals, so consecutive quads share their inner edge exactly and the
	//! strip reads as one ribbon rather than as a row of tiles with wedges between them. The mitre is
	//! capped, because a hairpin - which an unsmoothed staircase border is full of - would otherwise
	//! throw that vertex an unbounded distance.
	//!
	//! INWARD IS THE LEFT OF TRAVEL, which is a property of the contour tracer rather than a guess: it
	//! emits every segment with the region on its left, so the band lies inside the territory it belongs
	//! to and never bleeds over the neighbour's ground.
	//! \param[in] contour The traced border.
	//! \param[in] start First segment of the run.
	//! \param[in] length How many segments the run covers.
	protected void EmitBandSpan(OVT_TerritoryContour contour, int start, int length)
	{
		int count = contour.m_aX.Count();
		if (count < 3 || length < 1)
			return;

		// A run of L segments touches L+1 points. Both indices wrap, which is what lets a run that
		// crosses the start of the ring be one strip.
		int points = length + 1;

		// A run covering the WHOLE ring has no ends: its first and last points are the same point, so
		// both have to be mitred against both of their neighbours or the strip would step where it
		// closes. That is the case of a region completely surrounded by another faction - a pocket -
		// which is exactly where a visible notch would be least excusable.
		bool closed = length == count;

		m_aStripInnerX.Clear();
		m_aStripInnerZ.Clear();

		for (int k = 0; k < points; k++)
		{
			int index = (start + k) % count;

			float px = contour.m_aX[index];
			float pz = contour.m_aZ[index];

			// The segments meeting at this point. The first point of an OPEN run has no predecessor
			// inside it and the last has no successor, so each falls back to the one normal it has.
			float nx = 0;
			float nz = 0;
			int contributions = 0;

			if (k > 0 || closed)
			{
				float ax, az;
				if (SegmentNormal(contour, (start + k - 1 + count) % count, ax, az))
				{
					nx += ax;
					nz += az;
					contributions++;
				}
			}

			if (k < length || closed)
			{
				float bx, bz;
				if (SegmentNormal(contour, (start + k) % count, bx, bz))
				{
					nx += bx;
					nz += bz;
					contributions++;
				}
			}

			if (contributions == 0)
			{
				m_aStripInnerX.Insert(px);
				m_aStripInnerZ.Insert(pz);
				continue;
			}

			float length2 = Math.Sqrt(nx * nx + nz * nz);

			// A near-zero sum is a segment doubling straight back on itself; there is no meaningful
			// inward direction, so the strip pinches to nothing there rather than exploding.
			if (length2 < 0.0001)
			{
				m_aStripInnerX.Insert(px);
				m_aStripInnerZ.Insert(pz);
				continue;
			}

			nx = nx / length2;
			nz = nz / length2;

			// Mitre: dividing by the half-angle cosine keeps the inner edge parallel to the outer one
			// through a corner. length2 over the contribution count IS that cosine, because the summed
			// normals are unit vectors.
			float scale = 1;
			if (contributions == 2)
			{
				float halfCos = length2 * 0.5;
				if (halfCos > 0.0001)
					scale = 1 / halfCos;

				if (scale > MAX_BAND_MITRE)
					scale = MAX_BAND_MITRE;
			}

			m_aStripInnerX.Insert(px + nx * m_fBandWidth * scale);
			m_aStripInnerZ.Insert(pz + nz * m_fBandWidth * scale);
		}

		for (int e = 0; e < length; e++)
		{
			int i = (start + e) % count;
			int j = (start + e + 1) % count;

			EmitWorldQuad(contour.m_aX[i], contour.m_aZ[i], contour.m_aX[j], contour.m_aZ[j], m_aStripInnerX[e + 1], m_aStripInnerZ[e + 1], m_aStripInnerX[e], m_aStripInnerZ[e]);
		}
	}

	//! The unit normal of one contour segment, pointing INTO the region.
	//! \param[in] contour The traced border.
	//! \param[in] index Segment index.
	//! \param[out] nx Normal X.
	//! \param[out] nz Normal Z.
	//! \return False when the segment has no length and therefore no direction.
	protected bool SegmentNormal(OVT_TerritoryContour contour, int index, out float nx, out float nz)
	{
		int count = contour.m_aX.Count();

		int next = index + 1;
		if (next >= count)
			next = 0;

		float dx = contour.m_aX[next] - contour.m_aX[index];
		float dz = contour.m_aZ[next] - contour.m_aZ[index];

		float len = Math.Sqrt(dx * dx + dz * dz);
		if (len < 0.0001)
			return false;

		dx = dx / len;
		dz = dz / len;

		// Rotate the direction a quarter turn to the LEFT, which is the side the tracer keeps the region
		// on.
		nx = -dz;
		nz = dx;

		return true;
	}

	//-----------------------------------------------------------------------
	// COMMAND BATCHING
	//-----------------------------------------------------------------------

	//! Opens a batch. Every quad pushed until EndBatch shares this colour and this texture.
	//! \param[in] colour Packed ARGB.
	//! \param[in] texture Texture to tile over the quads, or null for a flat fill.
	//! \param[in] uvScale UV scale that goes with the texture.
	protected void StartBatch(int colour, SharedItemRef texture, float uvScale)
	{
		EndBatch();

		m_iBatchColour = colour;
		m_BatchTexture = texture;
		m_fBatchUVScale = uvScale;
		m_iBatchQuads = 0;
		m_Batch = null;
	}

	//! Closes the current batch, submitting whatever it holds.
	protected void EndBatch()
	{
		if (!m_Batch)
			return;

		m_Commands.Insert(m_Batch);
		m_Batch = null;
		m_iBatchQuads = 0;
	}

	//! Projects four world points and adds the quad they describe to the current batch.
	//!
	//! Vertices are supplied in ring order and the two triangles are supplied EXPLICITLY, so the engine
	//! never triangulates anything and a quad is correct by construction. When batching is off each quad
	//! becomes its own convex PolygonDrawCommand instead, which needs no engine feature at all - the
	//! geometry is identical and only the packing differs.
	//! \param[in] ax First corner world X.
	//! \param[in] az First corner world Z.
	//! \param[in] bx Second corner world X.
	//! \param[in] bz Second corner world Z.
	//! \param[in] cx Third corner world X.
	//! \param[in] cz Third corner world Z.
	//! \param[in] dx Fourth corner world X.
	//! \param[in] dz Fourth corner world Z.
	protected void EmitWorldQuad(float ax, float az, float bx, float bz, float cx, float cz, float dx, float dz)
	{
		int sax, say, sbx, sby, scx, scy, sdx, sdy;

		ProjectWorld(ax, az, sax, say);
		ProjectWorld(bx, bz, sbx, sby);
		ProjectWorld(cx, cz, scx, scy);
		ProjectWorld(dx, dz, sdx, sdy);

		if (!m_bBatchDrawCommands)
		{
			PolygonDrawCommand cmd = new PolygonDrawCommand();
			cmd.m_iColor = m_iBatchColour;

			if (m_BatchTexture)
			{
				cmd.m_pTexture = m_BatchTexture;
				cmd.m_fUVScale = m_fBatchUVScale;
			}

			cmd.m_Vertices = new array<float>();
			cmd.m_Vertices.Insert(sax);
			cmd.m_Vertices.Insert(say);
			cmd.m_Vertices.Insert(sbx);
			cmd.m_Vertices.Insert(sby);
			cmd.m_Vertices.Insert(scx);
			cmd.m_Vertices.Insert(scy);
			cmd.m_Vertices.Insert(sdx);
			cmd.m_Vertices.Insert(sdy);

			m_Commands.Insert(cmd);
			return;
		}

		if (m_Batch && m_iBatchQuads >= MAX_QUADS_PER_COMMAND)
			EndBatch();

		if (!m_Batch)
		{
			m_Batch = new TriMeshDrawCommand();
			m_Batch.m_iColor = m_iBatchColour;

			if (m_BatchTexture)
			{
				m_Batch.m_pTexture = m_BatchTexture;
				m_Batch.m_fUVScale = m_fBatchUVScale;
			}

			m_Batch.m_Vertices = new array<float>();
			m_Batch.m_Indices = new array<int>();
			m_iBatchQuads = 0;
		}

		int base = m_iBatchQuads * 4;

		m_Batch.m_Vertices.Insert(sax);
		m_Batch.m_Vertices.Insert(say);
		m_Batch.m_Vertices.Insert(sbx);
		m_Batch.m_Vertices.Insert(sby);
		m_Batch.m_Vertices.Insert(scx);
		m_Batch.m_Vertices.Insert(scy);
		m_Batch.m_Vertices.Insert(sdx);
		m_Batch.m_Vertices.Insert(sdy);

		m_Batch.m_Indices.Insert(base);
		m_Batch.m_Indices.Insert(base + 1);
		m_Batch.m_Indices.Insert(base + 2);

		m_Batch.m_Indices.Insert(base);
		m_Batch.m_Indices.Insert(base + 2);
		m_Batch.m_Indices.Insert(base + 3);

		m_iBatchQuads++;
	}

	//-----------------------------------------------------------------------
	// COLOUR
	//-----------------------------------------------------------------------

	//! THE ONLY PLACE THIS FILE RESOLVES A COLOUR, and it does not implement the lookup - it calls the
	//! SAME shared helper the town, base and radio-tower markers call, so territory colour and marker
	//! colour cannot drift apart. There is exactly one implementation of "what colour is faction N".
	//!
	//! What this method still owns is the ALPHA. Color.PackToInt would bake the faction's own alpha,
	//! which would defeat the contested-versus-held distinction entirely, so the components are unpacked
	//! and recomposed against our alpha instead.
	//!
	//! The unresolved-faction fallback here is WHITE, matching the base and radio-tower markers rather
	//! than the town marker's black: a black wash over the map is far less legible than a pale one, and
	//! this is a region rather than a small icon.
	//! \param[in] factionIndex The controlling faction index.
	//! \param[in] alpha Fill alpha, 0-255.
	//! \return Packed ARGB fill colour, falling back to white when the faction cannot be resolved.
	protected int ResolveFactionArgb(int factionIndex, int alpha)
	{
		int red = 255;
		int green = 255;
		int blue = 255;

		Color colour = OVT_MapLocationType.GetFactionColorByIndex(factionIndex);
		if (colour)
		{
			red = Math.ClampInt(Math.Round(colour.R() * 255), 0, 255);
			green = Math.ClampInt(Math.Round(colour.G() * 255), 0, 255);
			blue = Math.ClampInt(Math.Round(colour.B() * 255), 0, 255);
		}

		return ARGB(Math.ClampInt(alpha, 0, 255), red, green, blue);
	}

	//-----------------------------------------------------------------------
	// TEXTURES
	//-----------------------------------------------------------------------

	//! Loads the contested hatch once, guarded by IsValid before it is ever assigned to a command. A
	//! missing or broken texture must degrade to a flat fill, never crash, so the outcome is recorded
	//! in a flag and every emit path keeps drawing untextured - which leaves contested regions still
	//! distinguishable by their alpha, one half of the signal rather than none of it.
	protected void EnsureContestedTexture()
	{
		if (m_bContestedTextureAttempted)
			return;

		m_bContestedTextureAttempted = true;

		if (!m_bUseTextures)
			return;

		if (m_sContestedTexture.IsEmpty())
		{
			Print("[OVT_Territory] m_bUseTextures is on but no contested hatch texture is configured. Drawing flat contested fills.", LogLevel.ERROR);
			return;
		}

		if (!m_Canvas)
		{
			Print("[OVT_Territory] No CanvasWidget - cannot load the contested hatch texture. Drawing flat contested fills.", LogLevel.ERROR);
			return;
		}

		m_ContestedTextureRef = m_Canvas.LoadTexture(m_sContestedTexture);

		if (!m_ContestedTextureRef)
		{
			Print(string.Format("[OVT_Territory] LoadTexture returned null for '%1'. Drawing flat contested fills.", m_sContestedTexture), LogLevel.ERROR);
			return;
		}

		if (!m_ContestedTextureRef.IsValid())
		{
			m_ContestedTextureRef = null;
			Print(string.Format("[OVT_Territory] LoadTexture returned an invalid SharedItemRef for '%1'. Drawing flat contested fills.", m_sContestedTexture), LogLevel.ERROR);
			return;
		}

		m_bContestedTextureValid = true;
	}

	//! Loads the band hatch once, on exactly the same terms as the fill texture: attempted at most once
	//! per map open, never assigned to a command before IsValid has passed, and every failure path
	//! leaves m_bBandTextureValid false so the bands keep drawing as flat colour.
	//!
	//! AN EMPTY RESOURCE IS SILENT, not an error. It means somebody deliberately turned the band hatch
	//! off in config, which is a configuration rather than a missing asset. A CONFIGURED band texture
	//! that fails to load still logs, because that one really is a fault.
	protected void EnsureBandTexture()
	{
		if (m_bBandTextureAttempted)
			return;

		m_bBandTextureAttempted = true;

		if (!m_bUseTextures)
			return;

		if (m_sBandTexture.IsEmpty())
			return;

		if (!m_Canvas)
		{
			Print("[OVT_Territory] No CanvasWidget - cannot load the band texture. Drawing flat bands.", LogLevel.ERROR);
			return;
		}

		m_BandTextureRef = m_Canvas.LoadTexture(m_sBandTexture);

		if (!m_BandTextureRef)
		{
			Print(string.Format("[OVT_Territory] LoadTexture returned null for '%1'. Drawing flat bands.", m_sBandTexture), LogLevel.ERROR);
			return;
		}

		if (!m_BandTextureRef.IsValid())
		{
			m_BandTextureRef = null;
			Print(string.Format("[OVT_Territory] LoadTexture returned an invalid SharedItemRef for '%1'. Drawing flat bands.", m_sBandTexture), LogLevel.ERROR);
			return;
		}

		m_bBandTextureValid = true;
	}

	//-----------------------------------------------------------------------
	// GETTERS
	//-----------------------------------------------------------------------

	//! The collected sites for the current map session. Null before the map has opened.
	array<ref OVT_TerritorySite> GetSites()
	{
		return m_aSites;
	}

	//! The ownership grid for the current map session. Null before the map has opened.
	OVT_TerritoryGrid GetGrid()
	{
		return m_Grid;
	}

	//! The merged fill rectangles for the current map session. Empty unless contours are switched off,
	//! since the contour-cut fill replaces them. Null before the map has opened.
	array<ref OVT_TerritoryRect> GetRects()
	{
		return m_aRects;
	}

	//! The contour-cut fill trapezoids for the current map session. Null before the map has opened.
	array<ref OVT_TerritoryTrapezoid> GetTrapezoids()
	{
		return m_aTrapezoids;
	}

	//! The traced region borders for the current map session. Null before the map has opened.
	array<ref OVT_TerritoryContour> GetContours()
	{
		return m_aContours;
	}
}
