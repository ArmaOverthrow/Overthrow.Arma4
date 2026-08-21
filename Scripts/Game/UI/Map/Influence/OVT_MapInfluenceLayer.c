//! How a selected source's reach ring is drawn.
//!
//! SWITCHED ON WITH A DEFAULT BRANCH, never treated as a boolean, so a fourth mode slots in without
//! touching the renderer's control flow.
enum OVT_InfluenceRingMode
{
	DASHED,   //!< Alternating open arcs - segments / 2 draw commands. Matches the edges' idiom.
	SOLID,    //!< One enclosed polyline - ONE draw command at any segment count. Still an outline.
	NONE      //!< No ring at all.
}

//! Draws the influence relationships of the currently selected map location.
//!
//! POLL -> DERIVE -> EMIT. Each frame the layer asks the map UI which location the info panel is
//! describing; when that selection changes (or the refresh interval elapses, or a source collection
//! grows) it rebuilds a list of world-space edges and an optional reach ring; every frame it projects
//! and emits them as dashes plus, for a source, an open-arc reach ring.
//!
//! WORLD SPACE IS CACHED, SCREEN SPACE IS NOT, and the split is load-bearing. Edge endpoints are world
//! positions that survive every pan and zoom, so they are derived once and kept; the dash tessellation
//! is inherently per-frame because dash length is a PIXEL quantity and an edge's on-screen length
//! changes with the zoom. The per-frame path is therefore exactly one projection basis, two projected
//! points per edge, a length, a clamped dash count and the emit loop - no manager read, no distance
//! test and no faction lookup.
//!
//! WHAT IT IS NOT. Like every Overthrow canvas layer this is a READ-ONLY PROJECTION: no replicated
//! state, no RPC, no persistence, and no write to any campaign record. Everything it computes dies with
//! the map close.
//!
//! THE SELECTION COMES FROM THE PANEL, not from the map UI's element references. See
//! OVT_OverthrowMapUI.GetPanelLocation for why m_SelectedElement is the wrong field - it is sticky and
//! outlives the panel, which would put lines on screen with nothing to explain them.
//!
//! THE CROSS-CHECK IS WHAT MAKES CLIENT-SIDE DERIVATION SAFE, and it is the one rule the rest of this
//! file is arranged around. An edge is ACTIVE if and only if the geometry qualifies under the shared
//! rule set AND the corresponding support modifier is actually present in the target town's replicated
//! modifier list; otherwise it is SUPPRESSED. The layer therefore cannot assert an influence the
//! campaign has stopped applying, whatever the reason and whether or not this layer understands that
//! reason - which is also why it never asks a radio tower whether it is sabotaged. A sabotaged tower
//! still qualifies geometrically and its modifier is gone, so it comes out SUPPRESSED from the general
//! rule with no special case to write and none to maintain.
//!
//! THE RULES ARE NOT HERE. Every range test, polarity and source-to-modifier mapping comes from
//! OVT_InfluenceRules, the same code the server's modifier tick runs. This file contains no distance
//! comparison and no modifier name.
//!
//! IDENTITY LIVES IN THE CONFIG, exactly as the territory and restricted-area layers do it: draw order,
//! layer id and display name are the base class's attributes and MapOverthrow.conf sets them. Nothing
//! is redeclared here.
[BaseContainerProps()]
class OVT_MapInfluenceLayer : OVT_MapCanvasLayer
{
	//-----------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------

	[Attribute("5", UIWidgets.Auto, "Seconds between forced edge rebuilds while one location stays selected, matching the marker refresh. It is what makes a modifier appearing or expiring on the server show up here without any event to listen to - the campaign's own modifier tick is slower than this, so the overlay follows it rather than leading it.")]
	protected float m_fRefreshInterval;

	[Attribute("220", UIWidgets.Auto, "Alpha of an edge whose modifier the campaign is currently applying, 0-255. Paired with the longer dash rhythm below: the in-effect and suppressed classes are carried by TWO independent cues, because either one alone reads as a rendering artefact rather than as a state.")]
	protected int m_iActiveAlpha;

	[Attribute("70", UIWidgets.Auto, "Alpha of an edge whose relation exists geometrically but whose modifier the campaign is NOT applying, 0-255. It must be dim enough to read as deliberate and bright enough to read at all - a line that disappears at one zoom level is the failure this feature is most exposed to.")]
	protected int m_iSuppressedAlpha;

	[Attribute("2.0", UIWidgets.Auto, "Edge line width in SCREEN pixels, matching vanilla's waypoint lines. It is not projected and must not be: the endpoints move with pan and zoom, the thickness never does.")]
	protected float m_fLineWidth;

	[Attribute("14.0", UIWidgets.Auto, "Dash length in screen pixels for an in-effect edge.")]
	protected float m_fActiveDashLength;

	[Attribute("10.0", UIWidgets.Auto, "Gap length in screen pixels for an in-effect edge.")]
	protected float m_fActiveGapLength;

	[Attribute("6.0", UIWidgets.Auto, "Dash length in screen pixels for a suppressed edge. SHORTER than the in-effect dash on purpose - the second, alpha-independent cue.")]
	protected float m_fSuppressedDashLength;

	[Attribute("14.0", UIWidgets.Auto, "Gap length in screen pixels for a suppressed edge. SPARSER than the in-effect gap on purpose - together with the shorter dash it gives the suppressed class a rhythm a viewer can name without comparing it to anything.")]
	protected float m_fSuppressedGapLength;

	[Attribute("32", UIWidgets.Auto, "Hard upper bound on the number of dashes - and therefore draw commands - one edge may emit. Dash count follows SCREEN length, so the same edge is a couple of hundred pixels zoomed out and several thousand zoomed in; without this bound the layer's command count would be unbounded in exactly the situation the player is most likely to be in. When the bound bites, the dashes get LONGER rather than more numerous.")]
	protected int m_iMaxDashesPerEdge;

	[Attribute("72", UIWidgets.Auto, "Points around the reach ring. Half of them are emitted as arcs and half are left as gaps, so this many segments costs half as many draw commands. Rounded up to an even number, because alternating arcs need pairs. Ignored for command count in SOLID ring mode, which is always one command.")]
	protected int m_iRingSegments;

	[Attribute("0", UIWidgets.ComboBox, "How a selected tower's or base's reach ring is drawn. DASHED matches the edges and costs half the segment count in draw commands. SOLID is ONE command whatever the segment count - an enclosed polyline - and is the cheap degradation to reach for if the command budget is ever missed; it is still an outline, not a disc. NONE draws no ring at all, which also removes the affordance that tells a viewer the layer is alive when a source influences nothing.", enums: ParamEnumArray.FromEnum(OVT_InfluenceRingMode))]
	protected OVT_InfluenceRingMode m_eRingMode;

	[Attribute("1.5", UIWidgets.Auto, "Reach ring width in screen pixels. Thinner than an edge deliberately: the ring is context and the edges are the message.")]
	protected float m_fRingWidth;

	[Attribute("0", UIWidgets.Auto, "Print this layer's diagnostics: the two replicated influence ranges at map open - the ONLY direct evidence that the difficulty ranges reached this machine rather than being read from the difficulty preset the game-mode prefab happened to instantiate - and one line per edge rebuild with the per-state breakdown. That second line is the first thing to read when the overlay is empty, because it separates 'nothing is selected' from 'the selection derived no edges'. It ALSO turns on the rolling 60-frame draw report - average emit cost, this layer's own command count and the composited total across all layers - which costs a clock read and a compositor read per frame and is therefore off by default.")]
	protected bool m_bDebugTiming;

	//-----------------------------------------------------------------------
	// CONSTANTS
	//-----------------------------------------------------------------------

	//! Screen length in pixels below which an edge or a ring is not worth emitting at all. Two locations
	//! can project onto the same pixel when the map is zoomed fully out, and a zero-length polyline is
	//! either invisible or a stray dot depending on how the renderer rounds it.
	protected const float MIN_SCREEN_LENGTH = 2.0;

	//! Floor on the dash period, so a misconfigured pair of zero-length attributes divides by this
	//! instead of by zero. It is a guard, not a tunable - the attributes above are the tunables.
	protected const float MIN_DASH_PERIOD = 1.0;

	//! Frames averaged before the emit cost is reported, matching OVT_MapTerritoryLayer's window.
	//!
	//! IT HAS TO BE A WINDOW, and the reason is the timer: System.GetTickCount is INTEGER
	//! MILLISECONDS, so a Draw() taking half a millisecond measures as 0 or 1 and never as 0.5. Over a
	//! window that quantisation dithers rather than biases - a 0.5 ms Draw straddles a millisecond
	//! boundary about half the time - so the SUM over 60 frames estimates the total honestly even
	//! though no single frame does. A per-frame print would be pure noise, and this layer's budget
	//! (0.5 ms) sits entirely inside one tick of the clock measuring it.
	protected const int DRAW_SAMPLE_FRAMES = 60;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	//! The map UI component that owns the info panel. Resolved at map open AND lazily in Draw, because
	//! module init versus UI-component init order is an assumption rather than a guarantee - a null here
	//! on the first frames must heal itself rather than kill the layer for the whole session.
	protected OVT_OverthrowMapUI m_MapUI;

	//! Last selection key printed, so the log carries one line per selection CHANGE instead of one per
	//! frame. A per-frame print floods the log and makes the hover/pin observation unreadable, which is
	//! the whole point of this phase.
	protected string m_sLastSelectionKey;

	//! The derived relations for the current selection, in WORLD space. Strong refs: they are Managed.
	protected ref array<ref OVT_InfluenceEdge> m_aEdges;

	//! Centre of the selected source's reach ring.
	protected vector m_vRingCentre;

	//! Radius of that ring in metres, or 0 when the selection projects no reach - which is every
	//! selection that is not a radio tower or a military base. Zero is the "no ring" signal rather than
	//! a separate flag, because a ring of no size is nothing to draw either way.
	protected float m_fRingRadius;

	//! Faction index controlling the source the ring belongs to, or -1 when there is no ring. The ring is
	//! coloured by the same rule as the edges - the faction EXERTING the influence - so a source's reach
	//! cannot be drawn in a hue its own marker and its own lines disagree with.
	protected int m_iRingFaction;

	//! Whether an unrecognised edge state has already been reported this map session. The renderer runs
	//! every frame, so an unguarded log for a state added later would be a per-frame flood rather than a
	//! diagnostic.
	protected bool m_bUnknownStateReported;

	//! Same one-shot discipline as the edge-state warning above.
	protected bool m_bUnknownRingModeReported;

	//! Selection key the current edge list was built for.
	protected string m_sLastBuildKey;

	//! Seconds since the last rebuild.
	protected float m_fRefreshTimer;

	//! Source collection sizes recorded at the last build. BOTH OCCUPYING-FACTION ARRAYS ARE INSERTED
	//! INTO BY THE JIP STREAM, so a client that opened the map early can watch bases and towers arrive
	//! mid-session; comparing counts is what gets them drawn without waiting out the refresh interval.
	protected int m_iLastTownCount;
	protected int m_iLastBaseCount;
	protected int m_iLastTowerCount;

	//! Selection key an unresolvable id was last reported for, so a bad id produces ONE error rather
	//! than one every refresh tick for as long as the player leaves that location selected.
	protected string m_sLastResolveErrorKey;

	//! Rolling emit measurement. ONLY ACCUMULATED WHEN m_bDebugTiming IS ON - Draw is the one path in
	//! this feature that runs every frame for as long as the map is open, so the measurement must not
	//! exist on the normal path at all.
	protected int m_iDrawFrames;
	protected int m_iDrawTicks;

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	override void OnMapOpen(MapConfiguration config)
	{
		// Super resolves the canvas, allocates the command bucket and registers with the compositor.
		// Nothing below works without it.
		super.OnMapOpen(config);

		m_MapUI = ResolveMapUI();
		m_sLastSelectionKey = "";
		m_bUnknownStateReported = false;

		m_aEdges = new array<ref OVT_InfluenceEdge>();
		ClearDerived();

		ReportReplicatedRanges();
	}

	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);

		// Nulled rather than left to be re-used: the next map open builds a fresh widget hierarchy, and a
		// component reference held across the gap would point at the previous session's panel owner.
		m_MapUI = null;
		m_sLastSelectionKey = "";

		ClearDerived();
		m_aEdges = null;
	}

	override void Update(float timeSlice)
	{
		// The interval is accumulated here rather than in Draw because Draw does not receive a time
		// slice. Super runs Draw() and submits this layer's bucket to the compositor; the submit is
		// unconditional there, so nothing above it may return early.
		m_fRefreshTimer += timeSlice;

		super.Update(timeSlice);
	}

	//-----------------------------------------------------------------------
	// DRAW
	//-----------------------------------------------------------------------

	//! Per-frame entry point. The emit itself is EmitFrame; this wrapper is only the measurement.
	//!
	//! THE INSTRUMENTED PATH IS A SEPARATE BRANCH ON PURPOSE. With m_bDebugTiming off nothing here
	//! reads the clock, touches the compositor or accumulates anything - the flag is off in every
	//! shipped configuration and the layer must cost the same as if this code were absent.
	//!
	//! THE EMIT IS WRAPPED RATHER THAN INSTRUMENTED IN PLACE because EmitFrame returns early on four
	//! separate conditions - no bucket, no map UI, no selection - and three of those are the "nothing
	//! selected" case, which is a MEASUREMENT and not an absence: the budget says exactly 0 own
	//! commands with no selection, so the report has to fire there too. Instrumenting in place would
	//! have reported only the frames that drew something.
	override void Draw()
	{
		if (!m_bDebugTiming)
		{
			EmitFrame();
			return;
		}

		int startTick = System.GetTickCount();

		// Read BEFORE this frame's commands are built, so it reports the PREVIOUS frame's FINISHED
		// composite - every layer, territory and restriction rings included. Read after our own submit
		// it would only ever see the layers that happen to update before this one.
		int composited = 0;
		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (compositor)
			composited = compositor.GetCompositedCommandCount();

		EmitFrame();

		ReportDraw(System.GetTickCount() - startTick, composited);
	}

	//! Projects and emits one frame's dashes and ring. The whole of the per-frame render path.
	protected void EmitFrame()
	{
		if (!m_Commands)
			return;

		// CLEARED EVERY FRAME, BEFORE ANY EARLY RETURN BELOW. A layer that bails out with last frame's
		// commands still in the bucket keeps drawing them forever - the contract asks for an EMPTY
		// bucket, not a stale one, and "nothing selected" must contribute exactly nothing.
		m_Commands.Clear();

		if (!m_MapUI)
			m_MapUI = ResolveMapUI();

		if (!m_MapUI)
		{
			ReportSelection("");
			ClearDerived();
			return;
		}

		OVT_MapLocationData selection = m_MapUI.GetPanelLocation();
		if (!selection)
		{
			ReportSelection("");
			ClearDerived();
			return;
		}

		string key = BuildSelectionKey(selection);
		ReportSelection(key);

		if (ShouldRebuild(key))
			BuildEdges(selection);

		// ONCE per frame, before any per-vertex work: three WorldToScreen calls buy a basis that every
		// vertex below then costs two mul-adds against, and every dash of every edge shares it.
		CacheProjection();

		EmitEdges();
		EmitRing();
	}

	//-----------------------------------------------------------------------
	// DERIVATION
	//-----------------------------------------------------------------------

	//! Whether the cached edge list is still the right answer for this frame.
	//!
	//! THE COUNT CHECKS ARE CHEAP AND DELIBERATELY UNCONDITIONAL. Each is a cached static accessor and
	//! an array count, which is what buys a client the ability to notice sources streaming in during a
	//! join window instead of drawing an empty overlay until the interval next elapses.
	//! \param[in] key Selection key for this frame.
	//! \return True when BuildEdges must run.
	protected bool ShouldRebuild(string key)
	{
		if (key != m_sLastBuildKey)
			return true;

		if (m_fRefreshTimer >= m_fRefreshInterval)
			return true;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();

		if (CountTowns(towns) != m_iLastTownCount)
			return true;

		if (CountBases(occupying) != m_iLastBaseCount)
			return true;

		if (CountTowers(occupying) != m_iLastTowerCount)
			return true;

		return false;
	}

	//! Rebuilds the whole edge list and the reach ring for one selection.
	//!
	//! ONE UNIFORM PATH FOR EVERY SOURCE KIND. Geometry, faction, live state and the cross-check are all
	//! re-evaluated for towers, bases and momentum towns alike on every rebuild - there is no fast path
	//! for the kinds whose qualification happens to be static today. A full rebuild is a few dozen
	//! distance tests and a couple of array walks, which is cheaper than the bookkeeping that avoiding it
	//! would need, and an invariant not spent is an invariant a later change cannot break.
	//!
	//! MANAGERS ARE LOOKED UP HERE, PER CALL, AND NEVER CACHED ON THIS LAYER. A reference cached during
	//! a join-in-progress window when a manager is not yet resolvable stays null for the whole map
	//! session and produces an empty overlay with no error at all; a per-call lookup self-heals on the
	//! next rebuild.
	//! \param[in] selection The panel's location record.
	protected void BuildEdges(OVT_MapLocationData selection)
	{
		int startTick = System.GetTickCount();

		if (!m_aEdges)
			m_aEdges = new array<ref OVT_InfluenceEdge>();

		m_aEdges.Clear();
		m_vRingCentre = vector.Zero;
		m_fRingRadius = 0;
		m_iRingFaction = -1;
		m_fRefreshTimer = 0;
		m_sLastBuildKey = BuildSelectionKey(selection);

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		// Recorded even when the build below cannot proceed, so a manager that resolves one frame later
		// changes the count and triggers the rebuild that finally succeeds.
		m_iLastTownCount = CountTowns(towns);
		m_iLastBaseCount = CountBases(occupying);
		m_iLastTowerCount = CountTowers(occupying);

		if (!selection || !towns || !occupying || !config || !config.m_Difficulty)
		{
			ReportBuild(startTick);
			return;
		}

		// THE SUPPORT SYSTEM, NEVER THE STABILITY ONE. The three influence modifiers live in the support
		// config, and fetching the other system resolves every name to -1 - which draws a complete,
		// correctly shaped overlay in which every single edge is dim. That failure looks like a styling
		// bug and gets debugged in the renderer.
		OVT_TownModifierSystem support = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if (!support)
		{
			// Said out loud: without it every edge below resolves to index -1 and is drawn SUPPRESSED.
			// That is the conservative direction - the overlay under-claims rather than asserting an
			// influence it cannot confirm - but it is not something to discover by eye.
			Print("[OVT_Influence] support modifier system unresolved - every edge will draw suppressed", LogLevel.WARNING);
		}

		string typeName = selection.m_sTypeName;

		if (SelectionIs(typeName, OVT_MapLocationTown))
		{
			BuildTownSelectionEdges(selection, towns, occupying, config, support);
		}
		else if (SelectionIs(typeName, OVT_MapLocationRadioTower))
		{
			BuildTowerSelectionEdges(selection, towns, occupying, config, support);
		}
		else if (SelectionIs(typeName, OVT_MapLocationBase))
		{
			BuildBaseSelectionEdges(selection, towns, occupying, config, support);
		}

		// Every other location type - house, shop, FOB, camp, vehicle, bus stop - projects and receives
		// no influence, so it derives nothing and draws nothing. That is the correct output, not a gap.

		ReportBuild(startTick);
	}

	//! Everything that influences ONE SELECTED TOWN, plus the momentum it projects when the player holds
	//! it. A town is the only location that is both a receiver and a projector, which is why this arm is
	//! the long one.
	//! \param[in] selection The selected town's location record.
	//! \param[in] towns Town manager.
	//! \param[in] occupying Occupying faction manager.
	//! \param[in] config Overthrow config, for the difficulty ranges and the faction indices.
	//! \param[in] support Support modifier system, or null.
	protected void BuildTownSelectionEdges(OVT_MapLocationData selection, OVT_TownManagerComponent towns, OVT_OccupyingFactionManager occupying, OVT_OverthrowConfigComponent config, OVT_TownModifierSystem support)
	{
		OVT_TownData town = ResolveTown(towns, selection.m_iID);
		if (!town)
			return;

		float towerRange = config.m_Difficulty.radioTowerRange;
		float baseRange = config.m_Difficulty.baseSupportRange;
		int occupyingFaction = config.GetOccupyingFactionIndex();
		int playerFaction = config.GetPlayerFactionIndex();

		// Towers reaching this town. ONE EDGE PER TOWER, not one per modifier: the campaign collapses
		// them into a single record and which tower is responsible is the question being asked.
		if (occupying.m_RadioTowers)
		{
			foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
			{
				if (!tower)
					continue;

				if (!OVT_InfluenceRules.IsProximitySource(town.location, tower.location, towerRange))
					continue;

				AddEdge(tower.location, town.location, tower.faction, OVT_InfluenceSourceKind.RADIO_TOWER, occupyingFaction, town, support);
			}
		}

		// Bases reaching this town.
		if (occupying.m_Bases)
		{
			foreach (OVT_BaseData militaryBase : occupying.m_Bases)
			{
				if (!militaryBase)
					continue;

				if (!OVT_InfluenceRules.IsProximitySource(town.location, militaryBase.location, baseRange))
					continue;

				AddEdge(militaryBase.location, town.location, militaryBase.faction, OVT_InfluenceSourceKind.MILITARY_BASE, occupyingFaction, town, support);
			}
		}

		if (!towns.m_Towns)
			return;

		// Momentum, in whichever direction this town participates. The SAME predicate decides both: a
		// town that qualifies to receive momentum is a receiver and its neighbours are sources, and a
		// town that does not qualify is one the player already holds, so it is a projector and its
		// neighbours are targets. Nothing here compares faction indices itself.
		bool receivesMomentum = OVT_InfluenceRules.TownQualifiesForMomentum(town.faction, playerFaction);

		foreach (OVT_TownData other : towns.m_Towns)
		{
			if (!other || other == town)
				continue;

			if (!OVT_InfluenceRules.IsMomentumSource(town.location, other.location))
				continue;

			if (receivesMomentum)
			{
				// Sources are the towns the player holds - the ones that do NOT themselves qualify to
				// receive momentum.
				if (OVT_InfluenceRules.TownQualifiesForMomentum(other.faction, playerFaction))
					continue;

				AddEdge(other.location, town.location, other.faction, OVT_InfluenceSourceKind.MOMENTUM_TOWN, occupyingFaction, town, support);
				continue;
			}

			// This town is player-held, so it projects onto every neighbour that can receive.
			if (!OVT_InfluenceRules.TownQualifiesForMomentum(other.faction, playerFaction))
				continue;

			AddEdge(town.location, other.location, town.faction, OVT_InfluenceSourceKind.MOMENTUM_TOWN, occupyingFaction, other, support);
		}
	}

	//! Every town a SELECTED RADIO TOWER reaches, plus its reach ring.
	//! \param[in] selection The selected tower's location record.
	//! \param[in] towns Town manager.
	//! \param[in] occupying Occupying faction manager.
	//! \param[in] config Overthrow config, for the difficulty ranges and the faction indices.
	//! \param[in] support Support modifier system, or null.
	protected void BuildTowerSelectionEdges(OVT_MapLocationData selection, OVT_TownManagerComponent towns, OVT_OccupyingFactionManager occupying, OVT_OverthrowConfigComponent config, OVT_TownModifierSystem support)
	{
		OVT_RadioTowerData tower = ResolveTower(occupying, selection.m_iID);
		if (!tower)
			return;

		float towerRange = config.m_Difficulty.radioTowerRange;

		// The ring is drawn whether or not the tower reaches anything: it is the affordance that tells a
		// player the layer is alive on a source that influences no town.
		m_vRingCentre = tower.location;
		m_fRingRadius = towerRange;
		m_iRingFaction = tower.faction;

		AddEdgesToTownsInRange(tower.location, tower.faction, OVT_InfluenceSourceKind.RADIO_TOWER, towerRange, towns, config, support);
	}

	//! Every town a SELECTED MILITARY BASE reaches, plus its reach ring.
	//! \param[in] selection The selected base's location record.
	//! \param[in] towns Town manager.
	//! \param[in] occupying Occupying faction manager.
	//! \param[in] config Overthrow config, for the difficulty ranges and the faction indices.
	//! \param[in] support Support modifier system, or null.
	protected void BuildBaseSelectionEdges(OVT_MapLocationData selection, OVT_TownManagerComponent towns, OVT_OccupyingFactionManager occupying, OVT_OverthrowConfigComponent config, OVT_TownModifierSystem support)
	{
		OVT_BaseData militaryBase = ResolveBase(occupying, selection.m_iID);
		if (!militaryBase)
			return;

		float baseRange = config.m_Difficulty.baseSupportRange;

		m_vRingCentre = militaryBase.location;
		m_fRingRadius = baseRange;
		m_iRingFaction = militaryBase.faction;

		AddEdgesToTownsInRange(militaryBase.location, militaryBase.faction, OVT_InfluenceSourceKind.MILITARY_BASE, baseRange, towns, config, support);
	}

	//! One edge from a proximity source to every town inside its reach.
	//! \param[in] sourcePos World position of the source.
	//! \param[in] sourceFaction Faction index controlling the source.
	//! \param[in] kind Kind of source.
	//! \param[in] range Reach in metres, as read from the replicated difficulty settings.
	//! \param[in] towns Town manager.
	//! \param[in] config Overthrow config.
	//! \param[in] support Support modifier system, or null.
	protected void AddEdgesToTownsInRange(vector sourcePos, int sourceFaction, OVT_InfluenceSourceKind kind, float range, OVT_TownManagerComponent towns, OVT_OverthrowConfigComponent config, OVT_TownModifierSystem support)
	{
		if (!towns.m_Towns)
			return;

		int occupyingFaction = config.GetOccupyingFactionIndex();

		foreach (OVT_TownData town : towns.m_Towns)
		{
			if (!town)
				continue;

			if (!OVT_InfluenceRules.IsProximitySource(town.location, sourcePos, range))
				continue;

			AddEdge(sourcePos, town.location, sourceFaction, kind, occupyingFaction, town, support);
		}
	}

	//! Builds one edge and applies THE CROSS-CHECK to it.
	//!
	//! EVERY CANDIDATE EDGE COMES THROUGH HERE, whatever kind of source produced it and whichever
	//! direction it runs. The rules decide which modifier a (kind, polarity) pair means; the target
	//! town's replicated list decides whether the campaign is applying it; and a solid edge REQUIRES
	//! that modifier to be present. Nothing about why it might be absent is modelled here, which is
	//! exactly what lets a cause this layer has never heard of render correctly on the day it lands.
	//! \param[in] from World position of the source.
	//! \param[in] to World position of the town being influenced.
	//! \param[in] sourceFaction Faction index controlling the source.
	//! \param[in] kind Kind of source.
	//! \param[in] occupyingFaction Faction index of the occupying faction.
	//! \param[in] targetTown The town whose modifier list decides the state.
	//! \param[in] support Support modifier system, or null.
	protected void AddEdge(vector from, vector to, int sourceFaction, OVT_InfluenceSourceKind kind, int occupyingFaction, OVT_TownData targetTown, OVT_TownModifierSystem support)
	{
		OVT_InfluencePolarity polarity = OVT_InfluenceRules.PolarityForSource(sourceFaction == occupyingFaction);
		string modifierName = OVT_InfluenceRules.ModifierNameFor(kind, polarity);

		int modifierIndex = -1;
		if (support)
			modifierIndex = support.GetModifierIndexByName(modifierName);

		OVT_InfluenceEdgeState state = OVT_InfluenceEdgeState.SUPPRESSED;
		if (TownHasSupportModifier(targetTown, modifierIndex))
			state = OVT_InfluenceEdgeState.ACTIVE;

		m_aEdges.Insert(new OVT_InfluenceEdge(from, to, sourceFaction, kind, state, modifierName, modifierIndex));
	}

	//! Whether a town's replicated support modifier list carries one modifier index.
	//! \param[in] town The target town.
	//! \param[in] index Modifier index, or -1 when the name resolved to nothing.
	//! \return True when the campaign is currently applying it to this town.
	protected bool TownHasSupportModifier(OVT_TownData town, int index)
	{
		if (!town || index < 0 || !town.supportModifiers)
			return false;

		foreach (OVT_TownModifierData modifier : town.supportModifiers)
		{
			if (modifier && modifier.id == index)
				return true;
		}

		return false;
	}

	//! Drops the derived state without touching the command bucket.
	protected void ClearDerived()
	{
		if (m_aEdges)
			m_aEdges.Clear();

		m_vRingCentre = vector.Zero;
		m_fRingRadius = 0;
		m_iRingFaction = -1;
		m_sLastBuildKey = "";
	}

	//-----------------------------------------------------------------------
	// RESOLUTION - EVERY ARRAY INDEX FROM A SELECTION IS GUARDED
	//-----------------------------------------------------------------------

	//! Whether an unresolvable selection id should be reported this time.
	//!
	//! ONE ERROR PER SELECTION, not one per rebuild. The rebuild triggers keep firing while the bad
	//! selection stays up, and an error line every refresh tick buries the rest of the log in the
	//! situation where the log is the only thing there is to read.
	//! \return True the first time a given selection fails to resolve.
	protected bool ShouldReportResolveFailure()
	{
		if (m_sLastResolveErrorKey == m_sLastBuildKey)
			return false;

		m_sLastResolveErrorKey = m_sLastBuildKey;
		return true;
	}

	//! \param[in] towns Town manager.
	//! \param[in] id Town index carried by the selection.
	//! \return The town, or null after one ERROR when the id does not address one.
	protected OVT_TownData ResolveTown(OVT_TownManagerComponent towns, int id)
	{
		if (!towns.m_Towns || id < 0 || id >= towns.m_Towns.Count())
		{
			if (ShouldReportResolveFailure())
				Print(string.Format("[OVT_Influence] selected town id %1 is out of range - no edges derived", id), LogLevel.ERROR);

			return null;
		}

		return towns.m_Towns[id];
	}

	//! \param[in] occupying Occupying faction manager.
	//! \param[in] id Radio tower index carried by the selection.
	//! \return The tower, or null after one ERROR when the id does not address one.
	protected OVT_RadioTowerData ResolveTower(OVT_OccupyingFactionManager occupying, int id)
	{
		if (!occupying.m_RadioTowers || id < 0 || id >= occupying.m_RadioTowers.Count())
		{
			if (ShouldReportResolveFailure())
				Print(string.Format("[OVT_Influence] selected radio tower id %1 is out of range - no edges derived", id), LogLevel.ERROR);

			return null;
		}

		return occupying.m_RadioTowers[id];
	}

	//! \param[in] occupying Occupying faction manager.
	//! \param[in] id Base index carried by the selection.
	//! \return The base, or null after one ERROR when the id does not address one.
	protected OVT_BaseData ResolveBase(OVT_OccupyingFactionManager occupying, int id)
	{
		if (!occupying.m_Bases || id < 0 || id >= occupying.m_Bases.Count())
		{
			if (ShouldReportResolveFailure())
				Print(string.Format("[OVT_Influence] selected base id %1 is out of range - no edges derived", id), LogLevel.ERROR);

			return null;
		}

		return occupying.m_Bases[id];
	}

	//! \param[in] towns Town manager, possibly null.
	//! \return Number of towns known to this machine.
	protected int CountTowns(OVT_TownManagerComponent towns)
	{
		if (!towns || !towns.m_Towns)
			return 0;

		return towns.m_Towns.Count();
	}

	//! \param[in] occupying Occupying faction manager, possibly null.
	//! \return Number of bases known to this machine.
	protected int CountBases(OVT_OccupyingFactionManager occupying)
	{
		if (!occupying || !occupying.m_Bases)
			return 0;

		return occupying.m_Bases.Count();
	}

	//! \param[in] occupying Occupying faction manager, possibly null.
	//! \return Number of radio towers known to this machine.
	protected int CountTowers(OVT_OccupyingFactionManager occupying)
	{
		if (!occupying || !occupying.m_RadioTowers)
			return 0;

		return occupying.m_RadioTowers.Count();
	}

	//! Whether a selection's recorded type name is one particular location type.
	//! \param[in] typeName The selection's type name.
	//! \param[in] locationType The location type to test against.
	//! \return True when they are the same type.
	protected bool SelectionIs(string typeName, typename locationType)
	{
		return typeName == locationType.ToString();
	}

	//-----------------------------------------------------------------------
	// EMIT
	//-----------------------------------------------------------------------

	//! Every derived edge, dashed, in SCREEN pixels.
	protected void EmitEdges()
	{
		if (!m_aEdges)
			return;

		foreach (OVT_InfluenceEdge edge : m_aEdges)
		{
			if (!edge)
				continue;

			EmitEdge(edge);
		}
	}

	//! One edge as a run of short two-vertex lines.
	//!
	//! THE DASH RHYTHM IS A SCREEN QUANTITY AND THE ENDPOINTS ARE A WORLD ONE, which is the whole reason
	//! the tessellation cannot be cached: the same 1500 m edge is a couple of hundred pixels long zoomed
	//! out and several thousand zoomed in, so its dash count would follow the zoom without a bound.
	//!
	//! THE PERIOD IS DERIVED FROM THE CLAMPED COUNT, NOT THE OTHER WAY ROUND. The configured dash and gap
	//! give an IDEAL period; the count that period asks for is clamped to m_iMaxDashesPerEdge; and the
	//! period actually drawn is then the edge's length divided by that clamped count. So once the bound
	//! bites the dashes stretch instead of multiplying, the dash-to-gap PROPORTION is preserved so the
	//! two rhythms stay tellable apart, and this layer's command count is bounded by construction rather
	//! than by hoping nobody zooms in.
	//!
	//! Colour is the SOURCE's faction at the state's alpha. Solid versus dim carries "in effect versus
	//! suppressed" and must never move the hue: an edge that changed colour when it was suppressed would
	//! be claiming a different faction was responsible.
	//! \param[in] edge The relation to draw.
	protected void EmitEdge(OVT_InfluenceEdge edge)
	{
		int alpha;
		float dashLength;
		float gapLength;
		ResolveStyle(edge.m_eState, alpha, dashLength, gapLength);

		int fromX, fromY;
		int toX, toY;

		ProjectWorld(edge.m_vFrom[0], edge.m_vFrom[2], fromX, fromY);
		ProjectWorld(edge.m_vTo[0], edge.m_vTo[2], toX, toY);

		float deltaX = toX - fromX;
		float deltaY = toY - fromY;
		float length = Math.Sqrt(deltaX * deltaX + deltaY * deltaY);

		if (length < MIN_SCREEN_LENGTH)
			return;

		float idealPeriod = dashLength + gapLength;
		if (idealPeriod < MIN_DASH_PERIOD)
			idealPeriod = MIN_DASH_PERIOD;

		int maxDashes = m_iMaxDashesPerEdge;
		if (maxDashes < 1)
			maxDashes = 1;

		int dashes = Math.ClampInt((int)(length / idealPeriod), 1, maxDashes);

		float period = length / dashes;
		float dashFraction = dashLength / idealPeriod;
		float drawnDash = period * dashFraction;

		float unitX = deltaX / length;
		float unitY = deltaY / length;

		int colour = ResolveFactionArgb(edge.m_iSourceFaction, alpha);

		for (int i = 0; i < dashes; i++)
		{
			float startOffset = i * period;
			float endOffset = startOffset + drawnDash;

			EmitSegment(colour, m_fLineWidth, fromX + unitX * startOffset, fromY + unitY * startOffset, fromX + unitX * endOffset, fromY + unitY * endOffset);
		}
	}

	//! The selected source's reach, as ALTERNATING OPEN ARCS around its centre.
	//!
	//! AN OUTLINE, NOT A DISC. OVT_MapCanvasLayer.DrawCircle emits a filled PolygonDrawCommand, which is
	//! what the restricted-area layer wants and the opposite of what this needs - the map has to stay
	//! readable inside the ring, and a filled circle at influence scale would cover a quarter of the
	//! screen.
	//!
	//! ALTERNATE ARCS, NOT ONE ENCLOSED POLYLINE. m_bShouldEnclose would close the loop into a single
	//! CONTINUOUS ring, which is the solid degradation this feature keeps in reserve rather than the
	//! dashed ring it is asked for. Half the segments are drawn and half are left as gaps, so the ring
	//! costs half as many commands as it has segments.
	protected void EmitRing()
	{
		if (m_eRingMode == OVT_InfluenceRingMode.NONE)
			return;

		if (m_fRingRadius <= 0)
			return;

		if (!m_MapEntity)
			return;

		int segments = m_iRingSegments;
		if (segments < 4)
			segments = 4;

		// Pairs are what alternating needs: an odd count would draw two adjacent arcs across the seam and
		// leave one visible flat spot where the rhythm breaks.
		if (segments % 2 != 0)
			segments++;

		int centreX, centreY;
		ProjectWorld(m_vRingCentre[0], m_vRingCentre[2], centreX, centreY);

		// The radius is a WORLD quantity scaled to the screen, unlike the dash rhythm above: the ring is
		// the source's actual reach on the ground and must grow and shrink with the map.
		float radius = m_fRingRadius * m_MapEntity.GetCurrentZoom();
		if (radius < MIN_SCREEN_LENGTH)
			return;

		int colour = ResolveFactionArgb(m_iRingFaction, m_iActiveAlpha);

		switch (m_eRingMode)
		{
			case OVT_InfluenceRingMode.SOLID:
			{
				EmitSolidRing(colour, centreX, centreY, radius, segments);
				break;
			}

			case OVT_InfluenceRingMode.DASHED:
			{
				EmitDashedRing(colour, centreX, centreY, radius, segments);
				break;
			}

			default:
			{
				ReportUnknownRingMode(m_eRingMode);

				EmitDashedRing(colour, centreX, centreY, radius, segments);
				break;
			}
		}
	}

	//! The reach ring as alternating open arcs - segments / 2 commands.
	//! \param[in] colour Packed ARGB.
	//! \param[in] centreX Screen X of the centre.
	//! \param[in] centreY Screen Y of the centre.
	//! \param[in] radius Ring radius in screen pixels.
	//! \param[in] segments Even point count around the circumference.
	protected void EmitDashedRing(int colour, int centreX, int centreY, float radius, int segments)
	{
		float step = 2 * Math.PI / segments;

		for (int i = 0; i < segments; i += 2)
		{
			float thetaStart = i * step;
			float thetaEnd = (i + 1) * step;

			EmitSegment(colour, m_fRingWidth, centreX + radius * Math.Cos(thetaStart), centreY + radius * Math.Sin(thetaStart), centreX + radius * Math.Cos(thetaEnd), centreY + radius * Math.Sin(thetaEnd));
		}
	}

	//! The reach ring as ONE enclosed polyline - a single draw command whatever the segment count.
	//!
	//! THIS IS THE ONLY PLACE m_bShouldEnclose IS USED, and it is what makes the ring nearly free: closing
	//! the loop turns the whole circumference into one continuous stroke instead of one command per arc.
	//! It is still an OUTLINE - a LineDrawCommand strokes its vertices and never fills them, so the map
	//! stays visible through the interior either way. What it costs is the dashed rhythm the ring shares
	//! with the edges, which is why it is a mode rather than the default.
	//! \param[in] colour Packed ARGB.
	//! \param[in] centreX Screen X of the centre.
	//! \param[in] centreY Screen Y of the centre.
	//! \param[in] radius Ring radius in screen pixels.
	//! \param[in] segments Point count around the circumference.
	protected void EmitSolidRing(int colour, int centreX, int centreY, float radius, int segments)
	{
		LineDrawCommand cmd = new LineDrawCommand();
		cmd.m_iColor = colour;
		cmd.m_fWidth = m_fRingWidth;
		cmd.m_fOutlineWidth = 0;
		cmd.m_bShouldEnclose = true;

		cmd.m_Vertices = new array<float>();

		float step = 2 * Math.PI / segments;

		for (int i = 0; i < segments; i++)
		{
			float theta = i * step;

			cmd.m_Vertices.Insert(centreX + radius * Math.Cos(theta));
			cmd.m_Vertices.Insert(centreY + radius * Math.Sin(theta));
		}

		m_Commands.Insert(cmd);
	}

	//! One two-vertex line command, in screen pixels.
	//!
	//! TWO VERTICES IS THE WHOLE DASH. LineDrawCommand.m_Vertices is a flat POLYLINE - consecutive
	//! vertices connect - so a single command can hold no gaps, and N dashes are N commands however they
	//! are arranged. The width is NOT projected and must not be: it is a screen quantity, exactly the way
	//! vanilla's waypoint lines treat theirs.
	//! \param[in] colour Packed ARGB.
	//! \param[in] width Line width in screen pixels.
	//! \param[in] startX Screen X of the first vertex.
	//! \param[in] startY Screen Y of the first vertex.
	//! \param[in] endX Screen X of the second vertex.
	//! \param[in] endY Screen Y of the second vertex.
	protected void EmitSegment(int colour, float width, float startX, float startY, float endX, float endY)
	{
		LineDrawCommand cmd = new LineDrawCommand();
		cmd.m_iColor = colour;
		cmd.m_fWidth = width;
		cmd.m_fOutlineWidth = 0;

		cmd.m_Vertices = new array<float>();
		cmd.m_Vertices.Insert(startX);
		cmd.m_Vertices.Insert(startY);
		cmd.m_Vertices.Insert(endX);
		cmd.m_Vertices.Insert(endY);

		m_Commands.Insert(cmd);
	}

	//! The alpha and dash rhythm one edge state is drawn with.
	//!
	//! SWITCHED, WITH A DEFAULT - never an "if active, else" pair. A third state added later must slot in
	//! without touching this renderer's control flow, and until it does it lands in the default branch
	//! and is drawn suppressed, which is the conservative direction: the overlay under-claims rather than
	//! asserting an influence it does not understand.
	//!
	//! TWO CUES, NOT ONE, and they are deliberately not collapsible into each other. Alpha alone is a
	//! plausible rendering artefact and a dash rhythm alone is easy to miss, so a dim line reads as
	//! "suppressed" only because both change together.
	//! \param[in] state The edge's state.
	//! \param[out] alpha Alpha to draw it at.
	//! \param[out] dashLength Dash length in screen pixels.
	//! \param[out] gapLength Gap length in screen pixels.
	protected void ResolveStyle(OVT_InfluenceEdgeState state, out int alpha, out float dashLength, out float gapLength)
	{
		switch (state)
		{
			case OVT_InfluenceEdgeState.ACTIVE:
			{
				alpha = m_iActiveAlpha;
				dashLength = m_fActiveDashLength;
				gapLength = m_fActiveGapLength;
				break;
			}

			case OVT_InfluenceEdgeState.SUPPRESSED:
			{
				alpha = m_iSuppressedAlpha;
				dashLength = m_fSuppressedDashLength;
				gapLength = m_fSuppressedGapLength;
				break;
			}

			default:
			{
				ReportUnknownState(state);

				alpha = m_iSuppressedAlpha;
				dashLength = m_fSuppressedDashLength;
				gapLength = m_fSuppressedGapLength;
				break;
			}
		}
	}

	//! Reports an edge state this renderer has no style for, ONCE PER MAP SESSION.
	//!
	//! This runs on the per-frame emit path, so an unguarded log would be one line per edge per frame -
	//! which is not a diagnostic, it is a way of hiding every other line in the console.
	//! \param[in] state The unrecognised state.
	protected void ReportUnknownState(OVT_InfluenceEdgeState state)
	{
		if (m_bUnknownStateReported)
			return;

		m_bUnknownStateReported = true;

		Print(string.Format("[OVT_Influence] no style for edge state %1 - drawing it suppressed", state), LogLevel.WARNING);
	}

	//! Warns ONCE that a ring mode has no emitter, then falls back to the dashed ring.
	//! \param[in] mode The unhandled mode.
	protected void ReportUnknownRingMode(OVT_InfluenceRingMode mode)
	{
		if (m_bUnknownRingModeReported)
			return;

		m_bUnknownRingModeReported = true;

		Print(string.Format("[OVT_Influence] no emitter for ring mode %1 - drawing it dashed", mode), LogLevel.WARNING);
	}

	//-----------------------------------------------------------------------
	// DIAGNOSTICS
	//-----------------------------------------------------------------------

	//! Prints the two influence ranges this machine currently holds, ONCE PER MAP OPEN.
	//!
	//! THE ONLY DIRECT EVIDENCE THAT THE RANGES CROSSED THE WIRE. Both live on the difficulty
	//! settings, which a client instantiates from whichever preset the game-mode prefab points at and
	//! then patches IN PLACE from the config component's JIP bitstream. A client that never received
	//! that stream is therefore not empty-handed - it holds a plausible-looking WRONG number, and the
	//! only way to tell the two apart from inside the game is to read the number back. Geometry cannot
	//! do it: an overlay drawn at the wrong radius still looks like an overlay.
	//!
	//! THE DIFFICULTY NAME IS PRINTED ALONGSIDE ON PURPOSE. It travels in the same stream, ahead of
	//! both ranges, so a name matching the server's difficulty with a range that does not means the
	//! two appended values are at fault, while a name that disagrees too means the whole stream never
	//! arrived - most likely a client and server on different builds, which the stream's version stamp
	//! rejects by design. That distinction is what stops a failed play-test turning into a guess.
	//!
	//! THE CONFIG IS LOOKED UP PER CALL AND NEVER CACHED, matching every other manager access in this
	//! layer: a reference taken through a join window stays null for the whole map session and reports
	//! nothing, with no error to explain the silence.
	protected void ReportReplicatedRanges()
	{
		if (!m_bDebugTiming)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
		{
			// Said out loud rather than skipped. Silence is indistinguishable from the flag being off,
			// and this line exists to be read on a machine somebody else is running.
			Print("[OVT_Influence] ranges: difficulty settings unresolved on this machine", LogLevel.WARNING);
			return;
		}

		Print(string.Format("[OVT_Influence] ranges: radioTowerRange %1 m, baseSupportRange %2 m (difficulty '%3')", config.m_Difficulty.radioTowerRange, config.m_Difficulty.baseSupportRange, config.m_Difficulty.name), LogLevel.NORMAL);
	}

	//! Accumulates one frame's emit cost and prints the window average ONCE PER WINDOW.
	//!
	//! THE AVERAGE IS THE MEASUREMENT, not the per-frame number: System.GetTickCount is integer
	//! milliseconds, so a sub-millisecond Draw reads as 0 or 1 every frame and only the sum over a
	//! window means anything. See DRAW_SAMPLE_FRAMES.
	//!
	//! THE EDGE COUNT TRAVELS WITH THE COST because a frame cost with no edge count attached is not a
	//! recordable measurement - the acceptance criterion asks for the numbers together with the
	//! geometry that produced them, and the two are read off the same screen at the same moment.
	//!
	//! WORST-CASE OWN COMMANDS, so the printed number can be judged rather than merely noted: this
	//! layer emits at most m_iMaxDashesPerEdge commands per edge plus m_iRingSegments / 2 arcs for the
	//! reach ring, i.e. 32 * E + 36 at the shipped defaults. That meets the 400-command budget up to
	//! E = 11 edges (388) and misses it at E = 12 (420). A printed count far below the bound just means
	//! the dash caps are not biting at that zoom - zoom in and read it again.
	//!
	//! THE COMPOSITED TOTAL IS RECORDED, NOT BUDGETED, and the print says so out loud. It is the sum
	//! across every registered layer, and the territory overlay - much the largest contributor - never
	//! had its own three numbers measured, so there is no inherited total for this one to be compared
	//! against. Treating it as a pass/fail would be inventing a threshold.
	//! \param[in] elapsedMs This frame's Draw() cost, quantised to whole milliseconds.
	//! \param[in] composited Commands in the previous frame's finished composite, every layer included.
	protected void ReportDraw(int elapsedMs, int composited)
	{
		m_iDrawTicks += elapsedMs;
		m_iDrawFrames++;

		if (m_iDrawFrames < DRAW_SAMPLE_FRAMES)
			return;

		// Assigned to a float BEFORE the division: int / int truncates, and a 0.4 ms average - which is
		// a PASS against this layer's budget - would report as 0
		float averageMs = m_iDrawTicks;
		averageMs = averageMs / DRAW_SAMPLE_FRAMES;
		averageMs = Math.Round(averageMs * 100) / 100.0;

		int edgeCount = 0;
		if (m_aEdges)
			edgeCount = m_aEdges.Count();

		int ownCommands = 0;
		if (m_Commands)
			ownCommands = m_Commands.Count();

		Print(string.Format("[OVT_Influence] draw: %1 ms avg over %2 frames (%3 ms total, budget 0.5 ms) | %4 edges | %5 own commands (budget 400, expect 0 with nothing selected) | %6 composited commands last frame, all layers (RECORDED, NOT BUDGETED - no baseline) | edge build ms: see the 'build' line", averageMs, DRAW_SAMPLE_FRAMES, m_iDrawTicks, edgeCount, ownCommands, composited), LogLevel.NORMAL);

		m_iDrawTicks = 0;
		m_iDrawFrames = 0;
	}

	//! Prints what one rebuild derived, ONE LINE PER REBUILD.
	//!
	//! THE PER-STATE BREAKDOWN IS THE POINT. "The overlay is empty and I do not know why" has three
	//! completely different causes that look identical on screen - nothing selected, a selection that
	//! derives no edges, and edges that derive but do not render - and the counts here separate the
	//! first two from the third in one line. An all-suppressed breakdown on a selection that visibly
	//! has an active modifier chip is the signature of the name lookup failing, which is the other
	//! failure worth being able to read at a glance.
	//! \param[in] startTick Tick count taken at the top of the build.
	protected void ReportBuild(int startTick)
	{
		if (!m_bDebugTiming)
			return;

		int active = 0;
		int suppressed = 0;
		int total = 0;

		if (m_aEdges)
		{
			total = m_aEdges.Count();

			foreach (OVT_InfluenceEdge edge : m_aEdges)
			{
				if (!edge)
					continue;

				// SWITCHED WITH A DEFAULT, never treated as a boolean. A third state added later lands in
				// the default branch and is visible in this line rather than silently counted as one of
				// the two that already exist.
				switch (edge.m_eState)
				{
					case OVT_InfluenceEdgeState.ACTIVE:
					{
						active++;
						break;
					}

					case OVT_InfluenceEdgeState.SUPPRESSED:
					{
						suppressed++;
						break;
					}

					default:
					{
						break;
					}
				}
			}
		}

		string key = m_sLastBuildKey;
		if (key == "")
			key = "none";

		Print(string.Format("[OVT_Influence] build %1: %2 edges (%3 active, %4 suppressed)", key, total, active, suppressed), LogLevel.NORMAL);
		Print(string.Format("[OVT_Influence] build %1: ring radius %2 m, %3 ms", key, m_fRingRadius, System.GetTickCount() - startTick), LogLevel.NORMAL);
	}

	//! Prints the selection ONCE PER CHANGE, never per frame, and only with m_bDebugTiming on.
	//!
	//! The empty key is a real value meaning "nothing is selected", so a hover-away prints too - which is
	//! precisely the transition this phase exists to observe. Printing every frame instead would bury it.
	//! \param[in] key The current selection key, or "" when nothing is selected.
	protected void ReportSelection(string key)
	{
		if (key == m_sLastSelectionKey)
			return;

		m_sLastSelectionKey = key;

		if (!m_bDebugTiming)
			return;

		if (key == "")
		{
			Print("[OVT_Influence] selection: none", LogLevel.NORMAL);
			return;
		}

		Print(string.Format("[OVT_Influence] selection: %1", key), LogLevel.NORMAL);
	}

	//! Identity of a selection, as the type name and integer id the later phases dispatch on.
	//! \param[in] selection The panel's location record.
	//! \return "typeName|id".
	protected string BuildSelectionKey(OVT_MapLocationData selection)
	{
		if (!selection)
			return "";

		return selection.m_sTypeName + "|" + selection.m_iID.ToString();
	}

	//! The map UI component that owns the info panel, or null when it is not resolvable yet.
	//! \return The component, or null.
	protected OVT_OverthrowMapUI ResolveMapUI()
	{
		if (!m_MapEntity)
			return null;

		return OVT_OverthrowMapUI.Cast(m_MapEntity.GetMapUIComponent(OVT_OverthrowMapUI));
	}
}
