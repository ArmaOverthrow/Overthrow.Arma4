//------------------------------------------------------------------------------------------------
//! The territory geometry solver: pure maths, no managers, no game mode, no world of its own.
//!
//! Fetches nothing - the world is HANDED IN through SetWorld, and when it is null every point is
//! land. IsLandAt is virtual so a test can subclass it and stub a synthetic coast. Adding a manager
//! or game-mode lookup here would silently cost the feature its only automated coverage, and for
//! the same reason the config knobs are plain fields rather than [Attribute()]s.
//!
//! The solve: lay a square grid over the world bound box, and per square ask two independent
//! questions - is this land, and which site wins it under the multiplicatively-weighted
//! (Apollonius) test. Everything drawn derives from that array: the fill by merging same-appearance
//! squares, the border by marching squares plus Chaikin corner cutting. There is no maximum
//! influence radius; territory extends until it meets water or a competing site. No randomness.
//------------------------------------------------------------------------------------------------
class OVT_TerritorySolver : Managed
{
	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! Square size used when the configured one is non-positive.
	static const float DEFAULT_GRID_CELL_SIZE = 100;

	//! Hard ceiling on the number of squares, whatever the configured size asks for.
	//!
	//! IT IS A SAFETY VALVE, NOT A BUDGET. Cost is quadratic in the cell size, so a mistyped 5 on a
	//! 12 km world is 5.7 MILLION squares and a hung client rather than a slow one. Past this the
	//! builder GROWS the square size until the grid fits and records what it actually used, so the
	//! failure mode is a coarse overlay that says so instead of a frozen game.
	static const int MAX_GRID_CELLS = 250000;

	//! Metres of slack added around the site bounding box when there is no world and no explicit
	//! extent. Only reachable from a test or from a world whose bound box could not be read.
	static const float FALLBACK_GRID_MARGIN = 1000;

	//! Scan row height in metres used when the configured one is non-positive.
	static const float DEFAULT_SCAN_ROW_HEIGHT = 25;

	//! Hard ceiling on the number of scan rows one region's fill may be cut into.
	//!
	//! ⚠ Not the same kind of knob as the grid cell size: the ownership grid costs (span / cellSize)
	//! SQUARED while the scanline costs (span / rowHeight) - LINEAR. This ceiling only fires on a
	//! mistyped value, and grows the row height rather than allocating.
	static const int MAX_SCAN_ROWS = 4000;

	//! How many times a scan row whose top and bottom disagree may be halved before the leftover sliver
	//! is filled as a plain rectangle.
	//!
	//! A row whose ends disagree contains a change of structure - a peninsula tip, the first row of a
	//! hole, a region splitting, or a near-horizontal stretch of border. The leftover is at most
	//! rowHeight / 2^this.
	static const int MAX_ROW_SUBDIVISIONS = 4;

	//------------------------------------------------------------------------------------------------
	// TUNABLES - set by the layer from its config; defaults are the shipped planning values.
	//------------------------------------------------------------------------------------------------

	//! Square size in metres. THE ONE KNOB THAT DECIDES BOTH RESOLUTION AND COST, and the cost is
	//! QUADRATIC: 50 m is four times the work of 100 m. It also sets how blocky the fill looks, because
	//! a fill rectangle is cut on square edges.
	float m_fGridCellSize = DEFAULT_GRID_CELL_SIZE;

	//! Chaikin corner-cutting passes applied to every traced contour. 0 leaves the raw marching-squares
	//! staircase. EACH PASS DOUBLES THE SEGMENT COUNT, which is also what it costs to draw.
	int m_iSmoothPasses = 2;

	//! Metres of surface height above ocean height required before a point counts as land.
	float m_fShorelineMargin = 0.5;

	//! When false, the land test is skipped entirely and every square inside the extent is land.
	bool m_bClipToCoast = true;

	//------------------------------------------------------------------------------------------------
	// WORLD-DERIVED STATE - not knobs. Every one of these is either handed in or read off the world.
	//------------------------------------------------------------------------------------------------

	//! The world used by the land test. Null is legal and means "everything is land".
	protected BaseWorld m_World;

	//! True when the world reports no ocean at all, which short-circuits the land test.
	protected bool m_bWorldHasOcean = true;

	//! The rectangle the grid covers. Set from the world bound box by SetWorld, or by hand through
	//! SetGridExtent - which is what lets the whole geometry be exercised with no world loaded.
	protected bool m_bGridExtentValid;
	protected float m_fGridMinX;
	protected float m_fGridMinZ;
	protected float m_fGridMaxX;
	protected float m_fGridMaxZ;

	//------------------------------------------------------------------------------------------------
	//! Hands the solver a world for its land test and its extent. Never fetched, always given.
	//! Passing null is a supported mode: IsLandAt then returns true everywhere and the grid falls back
	//! to the sites' own bounding box.
	//! \param[in] world The world to sample surface and ocean heights from, or null.
	void SetWorld(BaseWorld world)
	{
		m_World = world;
		m_bGridExtentValid = false;

		if (!world)
		{
			m_bWorldHasOcean = true;
			return;
		}

		m_bWorldHasOcean = world.IsOcean();

		vector mins, maxs;
		world.GetBoundBox(mins, maxs);

		SetGridExtent(mins[0], mins[2], maxs[0], maxs[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the rectangle the grid covers, in world metres.
	//!
	//! Separate from SetWorld so a test can build a grid of a known size at a known origin. A
	//! degenerate rectangle is rejected rather than clamped - too large only costs squares, too small
	//! silently truncates the map.
	//! \param[in] minX Minimum world X.
	//! \param[in] minZ Minimum world Z.
	//! \param[in] maxX Maximum world X.
	//! \param[in] maxZ Maximum world Z.
	void SetGridExtent(float minX, float minZ, float maxX, float maxZ)
	{
		if (maxX <= minX || maxZ <= minZ)
		{
			m_bGridExtentValid = false;
			return;
		}

		m_fGridMinX = minX;
		m_fGridMinZ = minZ;
		m_fGridMaxX = maxX;
		m_fGridMaxZ = maxZ;
		m_bGridExtentValid = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an explicit or world-derived extent is known.
	//! \return True when SetGridExtent or SetWorld produced a usable rectangle.
	bool HasGridExtent()
	{
		return m_bGridExtentValid;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a world point is land, i.e. whether territory may extend over it.
	//!
	//! VIRTUAL BY DESIGN - a test overrides this to stub a coastline without a world.
	//!
	//! Deliberately compares terrain surface against OCEAN height rather than asking whether there
	//! is water here at all: a river or an inland lake must not chop a town's region in half, and
	//! this comparison ignores both by construction rather than by filtering a water-surface type.
	//! \param[in] x World X.
	//! \param[in] z World Z.
	//! \return True when the point is land.
	bool IsLandAt(float x, float z)
	{
		if (!m_World)
			return true;

		if (!m_bWorldHasOcean)
			return true;

		return IsLand(m_World.GetSurfaceY(x, z), m_World.GetOceanHeight(x, z), m_fShorelineMargin);
	}

	//------------------------------------------------------------------------------------------------
	//! The land predicate itself, as pure arithmetic so it can be asserted with no world at all.
	//! A point exactly ON the margin is NOT land: the margin is the amount of clearance demanded,
	//! so the comparison is strict and the shoreline itself falls on the water side.
	//! \param[in] surfaceY Terrain surface height at the point.
	//! \param[in] oceanY Ocean height at the point.
	//! \param[in] margin Required clearance above the ocean, in metres.
	//! \return True when the surface clears the ocean by more than the margin.
	static bool IsLand(float surfaceY, float oceanY, float margin)
	{
		return surfaceY > oceanY + margin;
	}

	//------------------------------------------------------------------------------------------------
	//! Which site owns a point, under the multiplicatively-weighted (Apollonius) distance test.
	//!
	//! The point belongs to the site minimising dist(q, site) / weight. ⚠ The weight is load-bearing:
	//! drop it - which in the squared form means dropping the cross-multiplication, not a division -
	//! and this collapses to a plain Voronoi diagram where a military base no longer out-projects a
	//! village. Two equal-weight sites meet at the midpoint either way, so an equal-weight case alone
	//! cannot detect its loss. Ties go to the earlier candidate, so the comparison is strict.
	//! \param[in] q The world point to test. Only X and Z matter.
	//! \param[in] sites Every site, indexed as the solve indexes them.
	//! \return Index into sites of the owner, or -1 when nothing can own the point.
	static int OwnsPoint(vector q, array<ref OVT_TerritorySite> sites)
	{
		return OwnsPointXZ(q[0], q[2], sites);
	}

	//------------------------------------------------------------------------------------------------
	//! The ownership test itself, on loose coordinates and without a single square root.
	//!
	//! For non-negative distances and positive weights, squaring is strictly increasing, so
	//!
	//!     dA / wA  <  dB / wB     <=>     dA^2 * wB^2  <  dB^2 * wA^2
	//!
	//! The comparison therefore runs entirely on squared distances and squared weights: no Math.Sqrt,
	//! no vector.DistanceXZ, no divide in the innermost loop, and the ordering including exact ties is
	//! identical to the unsquared one. The loose-coordinate overload exists because the grid tests one
	//! point per square and building a vector per square is pure overhead at 14,000 squares.
	//! \param[in] qx World X of the point to test.
	//! \param[in] qz World Z of the point to test.
	//! \param[in] sites Every site, indexed as the solve indexes them.
	//! \return Index into sites of the owner, or -1 when nothing can own the point.
	static int OwnsPointXZ(float qx, float qz, array<ref OVT_TerritorySite> sites)
	{
		if (!sites)
			return -1;

		int best = -1;
		float bestDistSq = 0;
		float bestWeightSq = 1;

		int count = sites.Count();

		for (int index = 0; index < count; index++)
		{
			OVT_TerritorySite site = sites[index];
			if (!site)
				continue;

			float weight = site.m_fWeight;
			if (weight <= 0)
				continue;

			float dx = qx - site.m_vPos[0];
			float dz = qz - site.m_vPos[2];

			float distSq = dx * dx + dz * dz;
			float weightSq = weight * weight;

			if (best == -1)
			{
				best = index;
				bestDistSq = distSq;
				bestWeightSq = weightSq;
				continue;
			}

			// Strictly better than the incumbent, cross-multiplied. Ties leave the incumbent alone,
			// which is what keeps "ties go to the earlier candidate" true.
			if (distSq * bestWeightSq < bestDistSq * weightSq)
			{
				best = index;
				bestDistSq = distSq;
				bestWeightSq = weightSq;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	// THE GRID BUILD
	//------------------------------------------------------------------------------------------------

	//! Classifies every square of the world: is it land, and which site owns it.
	//!
	//! Run once per map open and again only when the site SET changes. Nothing about how the result is
	//! drawn is decided here, because none of it moves a boundary.
	//!
	//! ⚠ EVERY SITE COMPETES, whoever holds it - filtering a site out before this point makes occupier
	//! colour flow over ground the player has already taken, silently and with a healthy site count.
	//! \param[in] sites The collected sites.
	//! \return The grid, always non-null, possibly with zero squares when no extent could be found.
	OVT_TerritoryGrid BuildGrid(array<ref OVT_TerritorySite> sites)
	{
		OVT_TerritoryGrid grid = new OVT_TerritoryGrid();
		grid.m_aOwner = new array<int>();
		grid.m_iCols = 0;
		grid.m_iRows = 0;

		float minX, minZ, maxX, maxZ;
		if (!ResolveExtent(sites, minX, minZ, maxX, maxZ))
			return grid;

		float size = m_fGridCellSize;
		if (size <= 0)
			size = DEFAULT_GRID_CELL_SIZE;

		// Snapped to a multiple of the square size measured from the WORLD origin, not the bound box
		// corner, so the overlay's squares sit on the base game's own grid lines rather than beside them.
		float originX = Math.Floor(minX / size) * size;
		float originZ = Math.Floor(minZ / size) * size;

		int cols = (int)Math.Ceil((maxX - originX) / size);
		int rows = (int)Math.Ceil((maxZ - originZ) / size);

		if (cols < 1)
			cols = 1;

		if (rows < 1)
			rows = 1;

		// THE SAFETY VALVE. Growing the square size is the only response that cannot hang the client,
		// and recording the size it settled on is what lets the solve log say the overlay is coarse
		// rather than leaving somebody to wonder why. The origin is re-snapped inside the loop because it
		// has to be a multiple of the size the grid was ACTUALLY built at, not of the one configured.
		while (cols * rows > MAX_GRID_CELLS)
		{
			size = size * 2;

			originX = Math.Floor(minX / size) * size;
			originZ = Math.Floor(minZ / size) * size;

			cols = (int)Math.Ceil((maxX - originX) / size);
			rows = (int)Math.Ceil((maxZ - originZ) / size);

			if (cols < 1)
				cols = 1;

			if (rows < 1)
				rows = 1;
		}

		grid.m_fOriginX = originX;
		grid.m_fOriginZ = originZ;
		grid.m_fCellSize = size;
		grid.m_iCols = cols;
		grid.m_iRows = rows;

		for (int r = 0; r < rows; r++)
		{
			float z = grid.CentreZ(r);

			for (int c = 0; c < cols; c++)
			{
				float x = grid.CentreX(c);

				int owner = -1;

				if (!m_bClipToCoast || IsLandAt(x, z))
					owner = OwnsPointXZ(x, z, sites);

				grid.m_aOwner.Insert(owner);
			}
		}

		return grid;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the grid goes: the world bound box when one is known, the sites' own bounding box plus a
	//! margin when it is not.
	//!
	//! The fallback is not a nicety - a solver with no world is the mode the whole Logic tier runs in,
	//! and a zero-square grid there would make every geometry assertion vacuously true.
	//! \param[in] sites The collected sites, used only by the fallback.
	//! \param[out] minX Minimum world X.
	//! \param[out] minZ Minimum world Z.
	//! \param[out] maxX Maximum world X.
	//! \param[out] maxZ Maximum world Z.
	//! \return True when a usable rectangle was found.
	protected bool ResolveExtent(array<ref OVT_TerritorySite> sites, out float minX, out float minZ, out float maxX, out float maxZ)
	{
		if (m_bGridExtentValid)
		{
			minX = m_fGridMinX;
			minZ = m_fGridMinZ;
			maxX = m_fGridMaxX;
			maxZ = m_fGridMaxZ;
			return true;
		}

		if (!sites || sites.IsEmpty())
			return false;

		bool any = false;

		foreach (OVT_TerritorySite site : sites)
		{
			if (!site)
				continue;

			float x = site.m_vPos[0];
			float z = site.m_vPos[2];

			if (!any)
			{
				minX = x;
				maxX = x;
				minZ = z;
				maxZ = z;
				any = true;
				continue;
			}

			if (x < minX)
				minX = x;

			if (x > maxX)
				maxX = x;

			if (z < minZ)
				minZ = z;

			if (z > maxZ)
				maxZ = z;
		}

		if (!any)
			return false;

		minX = minX - FALLBACK_GRID_MARGIN;
		minZ = minZ - FALLBACK_GRID_MARGIN;
		maxX = maxX + FALLBACK_GRID_MARGIN;
		maxZ = maxZ + FALLBACK_GRID_MARGIN;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// FILL: MERGED RECTANGLES
	//------------------------------------------------------------------------------------------------

	//! Merges the grid into as few convex quads as it can: maximal horizontal RUNS of squares that
	//! would be drawn identically, then greedily grown DOWNWARD while a whole run repeats in the row
	//! below. Fourteen thousand squares on a 12 km world collapse to roughly one quad per region per row.
	//!
	//! The key is APPEARANCE, not the owning site: two neighbouring towns of one faction draw the same,
	//! so merging across them is invisible and is the difference between one run per row and one per
	//! town per row. Runs are cut on square boundaries and a square belongs to exactly one run, so the
	//! quads tile the grid exactly - no gap, no overlap.
	//! \param[in] grid The ownership grid.
	//! \param[in] siteAppearance Appearance key per SITE index, or -1 where that site is not drawn.
	//! \param[in] mergeVertically Whether to grow runs downward into rectangles.
	//! \param[out] rects Cleared and refilled with the merged blocks.
	//! \return The run count BEFORE the vertical pass, so the layer can report what merging bought.
	static int BuildRects(OVT_TerritoryGrid grid, array<int> siteAppearance, bool mergeVertically, notnull array<ref OVT_TerritoryRect> rects)
	{
		rects.Clear();

		if (!grid || !grid.m_aOwner)
			return 0;

		int cols = grid.m_iCols;
		int rows = grid.m_iRows;

		if (cols < 1 || rows < 1)
			return 0;

		// Index of each row's first run, plus a trailing sentinel, so the vertical pass can address one
		// row's runs without searching for them
		array<int> rowStart = new array<int>();

		for (int r = 0; r < rows; r++)
		{
			rowStart.Insert(rects.Count());

			int c = 0;

			while (c < cols)
			{
				int key = AppearanceAt(grid, siteAppearance, c, r);

				if (key < 0)
				{
					c++;
					continue;
				}

				int last = c;

				while (last + 1 < cols && AppearanceAt(grid, siteAppearance, last + 1, r) == key)
				{
					last++;
				}

				OVT_TerritoryRect rect = new OVT_TerritoryRect();
				rect.m_iCol0 = c;
				rect.m_iCol1 = last;
				rect.m_iRow0 = r;
				rect.m_iRow1 = r;
				rect.m_iKey = key;

				rects.Insert(rect);

				c = last + 1;
			}
		}

		rowStart.Insert(rects.Count());

		int runCount = rects.Count();

		if (!mergeVertically)
			return runCount;

		array<bool> consumed = new array<bool>();
		for (int i = 0; i < runCount; i++)
		{
			consumed.Insert(false);
		}

		array<ref OVT_TerritoryRect> merged = new array<ref OVT_TerritoryRect>();

		for (int row = 0; row < rows; row++)
		{
			for (int s = rowStart[row]; s < rowStart[row + 1]; s++)
			{
				if (consumed[s])
					continue;

				OVT_TerritoryRect rect = rects[s];
				int lastRow = row;

				while (lastRow + 1 < rows)
				{
					int match = -1;

					for (int t = rowStart[lastRow + 1]; t < rowStart[lastRow + 2]; t++)
					{
						if (consumed[t])
							continue;

						OVT_TerritoryRect below = rects[t];

						if (below.m_iCol0 == rect.m_iCol0 && below.m_iCol1 == rect.m_iCol1 && below.m_iKey == rect.m_iKey)
						{
							match = t;
							break;
						}
					}

					if (match < 0)
						break;

					consumed[match] = true;
					lastRow++;
				}

				rect.m_iRow1 = lastRow;
				merged.Insert(rect);
			}
		}

		// The merged list holds strong references to the very same rect objects, so clearing the output
		// array here releases nothing that is still wanted
		rects.Clear();

		foreach (OVT_TerritoryRect kept : merged)
		{
			rects.Insert(kept);
		}

		return runCount;
	}

	//------------------------------------------------------------------------------------------------
	//! The appearance key of one square, or -1 when it is water, unowned, or owned by a site that is
	//! not drawn.
	//! \param[in] grid The ownership grid.
	//! \param[in] siteAppearance Appearance key per site index.
	//! \param[in] c Column.
	//! \param[in] r Row.
	//! \return The key, or -1.
	protected static int AppearanceAt(OVT_TerritoryGrid grid, array<int> siteAppearance, int c, int r)
	{
		int owner = grid.OwnerAt(c, r);
		if (owner < 0)
			return -1;

		if (!siteAppearance || owner >= siteAppearance.Count())
			return -1;

		return siteAppearance[owner];
	}

	//------------------------------------------------------------------------------------------------
	// BORDER: MARCHING SQUARES
	//------------------------------------------------------------------------------------------------

	//! Traces the outline of one region as closed world-space loops, with each segment classified as a
	//! frontier or not.
	//!
	//! The field is "does this square belong to the region", sampled at square CENTRES, so the contour
	//! runs through the midpoints of edges between neighbouring squares. It walks one square PAST every
	//! grid edge with everything outside reading as not-in-region, so a region touching the world edge
	//! still closes into a loop.
	//!
	//! ⚠ Orientation is load-bearing: every segment is emitted with the region on its LEFT, giving each
	//! contour node exactly one incoming and one outgoing segment. That is what lets the assembly below
	//! be a plain walk rather than a search, and what tells the band which way is inward.
	//!
	//! Saddle cases (two opposite corners in, two out) resolve as FOUR-connected, matching how the fill
	//! merges. It APPENDS rather than clears - the layer calls it once per drawn faction into one list.
	//! \param[in] grid The ownership grid.
	//! \param[in] inRegion Per SITE index, whether squares owned by that site are inside the region.
	//! \param[in] rivalFaction Per SITE index, whether that site counts as a DIFFERENT faction from the
	//!            region being outlined. This is what turns a boundary into a frontier; water is not in
	//!            the array at all and is therefore never one.
	//! \param[out] contours Appended with one entry per closed loop found.
	static void TraceRegion(OVT_TerritoryGrid grid, array<bool> inRegion, array<bool> rivalFaction, notnull array<ref OVT_TerritoryContour> contours)
	{
		if (!grid || !grid.m_aOwner)
			return;

		int cols = grid.m_iCols;
		int rows = grid.m_iRows;

		if (cols < 1 || rows < 1)
			return;

		// Node tables. A node is the midpoint of the edge between two neighbouring squares, and it is
		// registered at most once however many squares reference it.
		map<int, int> nodeIndex = new map<int, int>();
		array<float> nodeX = new array<float>();
		array<float> nodeZ = new array<float>();
		array<bool> nodeFrontier = new array<bool>();

		array<int> segFrom = new array<int>();
		array<int> segTo = new array<int>();

		int strideX = cols + 2;

		for (int r = -1; r < rows; r++)
		{
			for (int c = -1; c < cols; c++)
			{
				bool bl = InRegion(grid, inRegion, c, r);
				bool br = InRegion(grid, inRegion, c + 1, r);
				bool tr = InRegion(grid, inRegion, c + 1, r + 1);
				bool tl = InRegion(grid, inRegion, c, r + 1);

				int mask = 0;

				if (bl)
					mask = mask | 1;

				if (br)
					mask = mask | 2;

				if (tr)
					mask = mask | 4;

				if (tl)
					mask = mask | 8;

				if (mask == 0 || mask == 15)
					continue;

				// The four edge midpoints of this square of centres. B and T straddle a horizontal
				// neighbour pair, L and R a vertical one.
				int keyB = HorizontalNodeKey(strideX, c, r);
				int keyT = HorizontalNodeKey(strideX, c, r + 1);
				int keyL = VerticalNodeKey(strideX, c, r);
				int keyR = VerticalNodeKey(strideX, c + 1, r);

				if (bl != br)
					EnsureHorizontalNode(grid, inRegion, rivalFaction, nodeIndex, nodeX, nodeZ, nodeFrontier, keyB, c, r);

				if (tl != tr)
					EnsureHorizontalNode(grid, inRegion, rivalFaction, nodeIndex, nodeX, nodeZ, nodeFrontier, keyT, c, r + 1);

				if (bl != tl)
					EnsureVerticalNode(grid, inRegion, rivalFaction, nodeIndex, nodeX, nodeZ, nodeFrontier, keyL, c, r);

				if (br != tr)
					EnsureVerticalNode(grid, inRegion, rivalFaction, nodeIndex, nodeX, nodeZ, nodeFrontier, keyR, c + 1, r);

				// The sixteen cases, every one oriented so the region sits on the LEFT of travel. The
				// two saddles emit two independent segments.
				switch (mask)
				{
					case 1:  { AddSegment(segFrom, segTo, keyB, keyL); break; }
					case 2:  { AddSegment(segFrom, segTo, keyR, keyB); break; }
					case 3:  { AddSegment(segFrom, segTo, keyR, keyL); break; }
					case 4:  { AddSegment(segFrom, segTo, keyT, keyR); break; }
					case 5:  { AddSegment(segFrom, segTo, keyB, keyL); AddSegment(segFrom, segTo, keyT, keyR); break; }
					case 6:  { AddSegment(segFrom, segTo, keyT, keyB); break; }
					case 7:  { AddSegment(segFrom, segTo, keyT, keyL); break; }
					case 8:  { AddSegment(segFrom, segTo, keyL, keyT); break; }
					case 9:  { AddSegment(segFrom, segTo, keyB, keyT); break; }
					case 10: { AddSegment(segFrom, segTo, keyR, keyB); AddSegment(segFrom, segTo, keyL, keyT); break; }
					case 11: { AddSegment(segFrom, segTo, keyR, keyT); break; }
					case 12: { AddSegment(segFrom, segTo, keyL, keyR); break; }
					case 13: { AddSegment(segFrom, segTo, keyB, keyR); break; }
					case 14: { AddSegment(segFrom, segTo, keyL, keyB); break; }
				}
			}
		}

		AssembleContours(nodeIndex, nodeX, nodeZ, nodeFrontier, segFrom, segTo, contours);
	}

	//------------------------------------------------------------------------------------------------
	//! Walks the segment soup into closed loops.
	//!
	//! Every node has exactly one outgoing segment - a consequence of the fixed orientation, not an
	//! assumption - so following "where does the segment that starts here end" is guaranteed to return
	//! to its own start, and a step counter guards the walk anyway rather than trusting that proof at
	//! runtime.
	//! \param[in] nodeIndex Node key to table index.
	//! \param[in] nodeX Node world X by table index.
	//! \param[in] nodeZ Node world Z by table index.
	//! \param[in] nodeFrontier Whether the ground on the far side of that node is another faction's.
	//! \param[in] segFrom Segment start node keys.
	//! \param[in] segTo Segment end node keys.
	//! \param[out] contours Appended with one entry per loop.
	protected static void AssembleContours(map<int, int> nodeIndex, array<float> nodeX, array<float> nodeZ, array<bool> nodeFrontier, array<int> segFrom, array<int> segTo, notnull array<ref OVT_TerritoryContour> contours)
	{
		int segCount = segFrom.Count();
		if (segCount < 1)
			return;

		map<int, int> outgoing = new map<int, int>();
		array<bool> used = new array<bool>();

		for (int s = 0; s < segCount; s++)
		{
			outgoing.Set(segFrom[s], s);
			used.Insert(false);
		}

		for (int start = 0; start < segCount; start++)
		{
			if (used[start])
				continue;

			OVT_TerritoryContour contour = new OVT_TerritoryContour();
			contour.m_aX = new array<float>();
			contour.m_aZ = new array<float>();
			contour.m_aFrontier = new array<bool>();

			int current = start;
			int guard = 0;

			while (guard <= segCount)
			{
				guard++;

				if (used[current])
					break;

				used[current] = true;

				int fromNode;
				if (!nodeIndex.Find(segFrom[current], fromNode))
					break;

				int toNode;
				if (!nodeIndex.Find(segTo[current], toNode))
					break;

				contour.m_aX.Insert(nodeX[fromNode]);
				contour.m_aZ.Insert(nodeZ[fromNode]);

				// A SEGMENT IS A FRONTIER ONLY WHEN BOTH ITS ENDS ARE. Either end touching water stops
				// the band there rather than running it along the shore, which is the same rule the
				// ray-march applied to a whole ray, applied to a piece of a border instead.
				contour.m_aFrontier.Insert(nodeFrontier[fromNode] && nodeFrontier[toNode]);

				int next;
				if (!outgoing.Find(segTo[current], next))
					break;

				current = next;
			}

			// Two points cannot enclose anything and nothing can be drawn from them
			if (contour.m_aX.Count() >= 3)
				contours.Insert(contour);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one square belongs to the region being outlined. Outside the grid is always false, which
	//! is what closes a region that runs off the edge of the world.
	//! \param[in] grid The ownership grid.
	//! \param[in] inRegion Per site index.
	//! \param[in] c Column.
	//! \param[in] r Row.
	//! \return True when the square is inside the region.
	protected static bool InRegion(OVT_TerritoryGrid grid, array<bool> inRegion, int c, int r)
	{
		int owner = grid.OwnerAt(c, r);
		if (owner < 0)
			return false;

		if (!inRegion || owner >= inRegion.Count())
			return false;

		return inRegion[owner];
	}

	//------------------------------------------------------------------------------------------------
	//! Node key for the midpoint between squares (c,r) and (c+1,r).
	//! \param[in] strideX Row stride, cols + 2, so the one-square overscan on each side stays in range.
	//! \param[in] c Column of the left square.
	//! \param[in] r Row.
	//! \return The key.
	protected static int HorizontalNodeKey(int strideX, int c, int r)
	{
		return ((r + 1) * strideX + (c + 1)) * 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Node key for the midpoint between squares (c,r) and (c,r+1).
	//! \param[in] strideX Row stride.
	//! \param[in] c Column.
	//! \param[in] r Row of the lower square.
	//! \return The key.
	protected static int VerticalNodeKey(int strideX, int c, int r)
	{
		return ((r + 1) * strideX + (c + 1)) * 2 + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the midpoint between squares (c,r) and (c+1,r) if it is not already known, recording
	//! whether the ground on the OUTSIDE of the region at that point belongs to another faction.
	//! \param[in] grid The ownership grid.
	//! \param[in] inRegion Per site index.
	//! \param[in] rivalFaction Per site index.
	//! \param[inout] nodeIndex Node key to table index.
	//! \param[inout] nodeX Node world X.
	//! \param[inout] nodeZ Node world Z.
	//! \param[inout] nodeFrontier Node frontier flag.
	//! \param[in] key The node key.
	//! \param[in] c Column of the left square.
	//! \param[in] r Row.
	protected static void EnsureHorizontalNode(OVT_TerritoryGrid grid, array<bool> inRegion, array<bool> rivalFaction, map<int, int> nodeIndex, array<float> nodeX, array<float> nodeZ, array<bool> nodeFrontier, int key, int c, int r)
	{
		if (nodeIndex.Contains(key))
			return;

		int outside;
		if (InRegion(grid, inRegion, c, r))
			outside = grid.OwnerAt(c + 1, r);
		else
			outside = grid.OwnerAt(c, r);

		nodeIndex.Set(key, nodeX.Count());
		nodeX.Insert(grid.EdgeX(c + 1));
		nodeZ.Insert(grid.CentreZ(r));
		nodeFrontier.Insert(IsRivalOwner(rivalFaction, outside));
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the midpoint between squares (c,r) and (c,r+1) if it is not already known.
	//! \param[in] grid The ownership grid.
	//! \param[in] inRegion Per site index.
	//! \param[in] rivalFaction Per site index.
	//! \param[inout] nodeIndex Node key to table index.
	//! \param[inout] nodeX Node world X.
	//! \param[inout] nodeZ Node world Z.
	//! \param[inout] nodeFrontier Node frontier flag.
	//! \param[in] key The node key.
	//! \param[in] c Column.
	//! \param[in] r Row of the lower square.
	protected static void EnsureVerticalNode(OVT_TerritoryGrid grid, array<bool> inRegion, array<bool> rivalFaction, map<int, int> nodeIndex, array<float> nodeX, array<float> nodeZ, array<bool> nodeFrontier, int key, int c, int r)
	{
		if (nodeIndex.Contains(key))
			return;

		int outside;
		if (InRegion(grid, inRegion, c, r))
			outside = grid.OwnerAt(c, r + 1);
		else
			outside = grid.OwnerAt(c, r);

		nodeIndex.Set(key, nodeX.Count());
		nodeX.Insert(grid.CentreX(c));
		nodeZ.Insert(grid.EdgeZ(r + 1));
		nodeFrontier.Insert(IsRivalOwner(rivalFaction, outside));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the site on the far side of a boundary node counts as another faction.
	//!
	//! WATER IS NOT A FRONTIER, and that is the whole reason the unowned case is answered here rather
	//! than by arithmetic: an owner index of -1 means water or unowned ground, and a coastline is
	//! nobody's border.
	//! \param[in] rivalFaction Per site index.
	//! \param[in] owner Owning site index on the far side, or -1.
	//! \return True when the far side belongs to a different faction.
	protected static bool IsRivalOwner(array<bool> rivalFaction, int owner)
	{
		if (owner < 0)
			return false;

		if (!rivalFaction || owner >= rivalFaction.Count())
			return false;

		return rivalFaction[owner];
	}

	//------------------------------------------------------------------------------------------------
	//! Records one oriented contour segment.
	//! \param[inout] segFrom Segment start node keys.
	//! \param[inout] segTo Segment end node keys.
	//! \param[in] from Start node key.
	//! \param[in] to End node key.
	protected static void AddSegment(array<int> segFrom, array<int> segTo, int from, int to)
	{
		segFrom.Insert(from);
		segTo.Insert(to);
	}

	//------------------------------------------------------------------------------------------------
	// BORDER: CHAIKIN CORNER CUTTING
	//------------------------------------------------------------------------------------------------

	//! Rounds a closed contour by Chaikin corner cutting, in place.
	//!
	//! Each pass replaces every point with two - a quarter and three quarters of the way to its
	//! successor - so the polygon converges on a quadratic B-spline and the SEGMENT COUNT DOUBLES. Two
	//! passes is four times the border geometry to draw.
	//!
	//! `passes` of 0 or less leaves the contour exactly as traced. ⚠ The loop bound is the only thing
	//! enforcing that - there is deliberately no early return, because a second guard saying the same
	//! thing would make the contract impossible to turn red in a test.
	//!
	//! Frontier flags survive the pass: a new segment inside one old segment inherits its class, while
	//! one spanning an old CORNER is a frontier only when BOTH old segments were, so a band stops
	//! cleanly where a frontier runs into a coastline.
	//! \param[inout] contour The contour, smoothed in place.
	//! \param[in] passes How many corner-cutting passes to apply.
	static void ChaikinSmooth(OVT_TerritoryContour contour, int passes)
	{
		if (!contour || !contour.m_aX || !contour.m_aZ || !contour.m_aFrontier)
			return;

		for (int pass = 0; pass < passes; pass++)
		{
			int count = contour.m_aX.Count();

			if (count < 3)
				return;

			array<float> nextX = new array<float>();
			array<float> nextZ = new array<float>();
			array<bool> nextFrontier = new array<bool>();

			for (int i = 0; i < count; i++)
			{
				int j = i + 1;
				if (j >= count)
					j = 0;

				float x0 = contour.m_aX[i];
				float z0 = contour.m_aZ[i];
				float x1 = contour.m_aX[j];
				float z1 = contour.m_aZ[j];

				nextX.Insert(x0 * 0.75 + x1 * 0.25);
				nextZ.Insert(z0 * 0.75 + z1 * 0.25);

				nextX.Insert(x0 * 0.25 + x1 * 0.75);
				nextZ.Insert(z0 * 0.25 + z1 * 0.75);

				bool here = contour.m_aFrontier[i];
				bool after = contour.m_aFrontier[j];

				// Inside old segment i, then across the corner at old point i+1
				nextFrontier.Insert(here);
				nextFrontier.Insert(here && after);
			}

			contour.m_aX = nextX;
			contour.m_aZ = nextZ;
			contour.m_aFrontier = nextFrontier;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Finds every contiguous run of frontier segments on a closed contour, TREATING THE RING AS
	//! CIRCULAR - a run crossing index 0 is ONE span, not two.
	//!
	//! The scan starts immediately after a segment that is NOT a frontier, so no run can straddle the
	//! start and the ordinary walk-and-close loop is already circular-correct. The one case with no
	//! such starting point - every segment a frontier - is emitted as one whole-ring span up front.
	//! \param[in] frontier Per-segment frontier flags.
	//! \param[out] spanStart Cleared and refilled with each span's first segment index.
	//! \param[out] spanLength Cleared and refilled with each span's segment count.
	static void CollectFrontierSpans(array<bool> frontier, notnull array<int> spanStart, notnull array<int> spanLength)
	{
		spanStart.Clear();
		spanLength.Clear();

		if (!frontier)
			return;

		int count = frontier.Count();
		if (count < 1)
			return;

		int origin = -1;

		for (int i = 0; i < count; i++)
		{
			if (!frontier[i])
			{
				origin = i;
				break;
			}
		}

		if (origin == -1)
		{
			spanStart.Insert(0);
			spanLength.Insert(count);
			return;
		}

		int runStart = -1;
		int runLength = 0;

		// Steps 1..count inclusive, so the walk finishes back ON the non-frontier origin, which closes
		// any open run. That is what makes a wrapping run come out as one span.
		for (int step = 1; step <= count; step++)
		{
			int index = (origin + step) % count;

			if (frontier[index])
			{
				if (runLength == 0)
					runStart = index;

				runLength++;
			}
			else if (runLength > 0)
			{
				spanStart.Insert(runStart);
				spanLength.Insert(runLength);
				runLength = 0;
			}
		}

		// Defensive only: the final step lands on the non-frontier origin, so a run is always closed
		if (runLength > 0)
		{
			spanStart.Insert(runStart);
			spanLength.Insert(runLength);
		}
	}

	//------------------------------------------------------------------------------------------------
	// FILL: TRAPEZOID SCANLINE DECOMPOSITION OF THE SMOOTHED CONTOUR
	//------------------------------------------------------------------------------------------------

	//! Cuts a region's fill out of its own SMOOTHED CONTOUR, as a stack of convex trapezoids, so the
	//! fill and the band share one boundary by construction.
	//!
	//! Sweep horizontal scan rows across the region. At the row's top edge and again at its bottom
	//! edge, intersect every contour segment with the line and sort the crossings; under the EVEN-ODD
	//! rule consecutive pairs are the inside. Each pair becomes a trapezoid whose left and right edges
	//! run between the actual crossings, so slanted border is drawn slanted rather than stepped. Holes
	//! and disjoint components fall out of even-odd and need no special case.
	//!
	//! ⚠ The row count is LINEAR in the row height where the ownership grid's cell size is QUADRATIC;
	//! the two knobs look alike and are not. Segments are bucketed by the rows they can reach, without
	//! which this would be rows times segments.
	//!
	//! It APPENDS, and stamps the appearance key onto every trapezoid so the emit path can batch by
	//! draw state in a single pass.
	//! \param[in] contours Every traced contour; only those carrying this appearance key are used.
	//! \param[in] appearanceKey Which appearance to cut, and the key stamped onto the output.
	//! \param[in] rowHeight Scan row height in metres. Non-positive falls back to the default.
	//! \param[out] traps Appended with the trapezoids covering this appearance's ground.
	static void BuildTrapezoids(array<ref OVT_TerritoryContour> contours, int appearanceKey, float rowHeight, notnull array<ref OVT_TerritoryTrapezoid> traps)
	{
		if (!contours)
			return;

		if (rowHeight <= 0)
			rowHeight = DEFAULT_SCAN_ROW_HEIGHT;

		// Every segment of every contour with this key, flattened into four parallel coordinate arrays.
		// One pass over the objects here is what lets every row below touch floats instead of chasing
		// references, which matters because a row is visited many times over.
		array<float> ax = new array<float>();
		array<float> az = new array<float>();
		array<float> bx = new array<float>();
		array<float> bz = new array<float>();

		float minZ = 0;
		float maxZ = 0;
		bool any = false;

		foreach (OVT_TerritoryContour contour : contours)
		{
			if (!contour || contour.m_iAppearanceKey != appearanceKey)
				continue;

			if (!contour.m_aX || !contour.m_aZ)
				continue;

			int count = contour.m_aX.Count();

			// Two points enclose nothing, so there is no inside to fill
			if (count < 3)
				continue;

			for (int i = 0; i < count; i++)
			{
				int j = i + 1;
				if (j >= count)
					j = 0;

				ax.Insert(contour.m_aX[i]);
				az.Insert(contour.m_aZ[i]);
				bx.Insert(contour.m_aX[j]);
				bz.Insert(contour.m_aZ[j]);

				if (!any)
				{
					minZ = contour.m_aZ[i];
					maxZ = contour.m_aZ[i];
					any = true;
				}

				if (contour.m_aZ[i] < minZ)
					minZ = contour.m_aZ[i];

				if (contour.m_aZ[i] > maxZ)
					maxZ = contour.m_aZ[i];
			}
		}

		if (!any)
			return;

		// ROWS ARE SNAPPED TO MULTIPLES OF THE ROW HEIGHT FROM THE WORLD ORIGIN rather than started at the
		// region's own top edge. That keeps a row boundary in the same world place from one refresh to the
		// next, so a region that gains a square does not shift every trapezoid edge on the map with it.
		float rowOriginZ = Math.Floor(minZ / rowHeight) * rowHeight;

		int rows = (int)Math.Ceil((maxZ - rowOriginZ) / rowHeight);
		if (rows < 1)
			rows = 1;

		// The valve. Linear cost, so this only ever catches a mistyped value.
		while (rows > MAX_SCAN_ROWS)
		{
			rowHeight = rowHeight * 2;

			rowOriginZ = Math.Floor(minZ / rowHeight) * rowHeight;

			rows = (int)Math.Ceil((maxZ - rowOriginZ) / rowHeight);
			if (rows < 1)
				rows = 1;
		}

		array<int> rowStart = new array<int>();
		array<int> rowSegs = new array<int>();

		BucketSegments(az, bz, rowOriginZ, rowHeight, rows, rowStart, rowSegs);

		array<float> topCross = new array<float>();
		array<float> botCross = new array<float>();

		for (int r = 0; r < rows; r++)
		{
			float zTop = rowOriginZ + r * rowHeight;
			float zBot = zTop + rowHeight;

			if (r == 0)
				CollectCrossings(ax, az, bx, bz, rowSegs, rowStart[0], rowStart[1], zTop, topCross);

			CollectCrossings(ax, az, bx, bz, rowSegs, rowStart[r], rowStart[r + 1], zBot, botCross);

			EmitRowTrapezoids(ax, az, bx, bz, rowSegs, rowStart[r], rowStart[r + 1], zTop, zBot, topCross, botCross, 0, appearanceKey, traps);

			// THE BOTTOM OF THIS ROW IS THE TOP OF THE NEXT, and carrying it rather than recomputing it is
			// not only cheaper - it is what makes the two agree EXACTLY. A row seam is precisely where a
			// recomputed crossing would show up as a hairline.
			array<float> swap = topCross;
			topCross = botCross;
			botCross = swap;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buckets segments by the scan rows they can reach, as a compressed row-index table. Without it
	//! the decomposition is rows times segments - millions of comparisons rather than tens of thousands.
	//!
	//! ⚠ The range is deliberately widened by one row at each end. A row is tested at its top edge, its
	//! bottom edge AND at any midline the subdivision introduces, so a tight bucket would be one
	//! rounding away from dropping a segment - and a dropped segment is a span that silently opens.
	//! \param[in] az Segment start Z per segment.
	//! \param[in] bz Segment end Z per segment.
	//! \param[in] rowOriginZ World Z of the first row's top edge.
	//! \param[in] rowHeight Row height in metres.
	//! \param[in] rows How many rows there are.
	//! \param[out] rowStart Cleared and refilled with rows + 1 offsets into rowSegs.
	//! \param[out] rowSegs Cleared and refilled with segment indices, grouped by row.
	protected static void BucketSegments(array<float> az, array<float> bz, float rowOriginZ, float rowHeight, int rows, notnull array<int> rowStart, notnull array<int> rowSegs)
	{
		rowStart.Clear();
		rowSegs.Clear();

		for (int r = 0; r <= rows; r++)
		{
			rowStart.Insert(0);
		}

		int segCount = az.Count();

		// Counting pass: how many segments land in each row, written one slot to the right so the prefix
		// sum below turns the counts into offsets in place.
		for (int s = 0; s < segCount; s++)
		{
			int lo, hi;
			SegmentRowRange(az[s], bz[s], rowOriginZ, rowHeight, rows, lo, hi);

			for (int r1 = lo; r1 <= hi; r1++)
			{
				rowStart[r1 + 1] = rowStart[r1 + 1] + 1;
			}
		}

		for (int r2 = 0; r2 < rows; r2++)
		{
			rowStart[r2 + 1] = rowStart[r2 + 1] + rowStart[r2];
		}

		array<int> cursor = new array<int>();
		for (int r3 = 0; r3 < rows; r3++)
		{
			cursor.Insert(rowStart[r3]);
		}

		int total = rowStart[rows];
		for (int t = 0; t < total; t++)
		{
			rowSegs.Insert(-1);
		}

		// Filling pass, in the same order, so the two agree about how many slots each row was given
		for (int s2 = 0; s2 < segCount; s2++)
		{
			int lo2, hi2;
			SegmentRowRange(az[s2], bz[s2], rowOriginZ, rowHeight, rows, lo2, hi2);

			for (int r4 = lo2; r4 <= hi2; r4++)
			{
				rowSegs[cursor[r4]] = s2;
				cursor[r4] = cursor[r4] + 1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The inclusive range of scan rows one segment can be tested against, clamped to the grid of rows.
	//! \param[in] z0 Segment start Z.
	//! \param[in] z1 Segment end Z.
	//! \param[in] rowOriginZ World Z of the first row's top edge.
	//! \param[in] rowHeight Row height in metres.
	//! \param[in] rows How many rows there are.
	//! \param[out] lo First row.
	//! \param[out] hi Last row, or a value below lo when the segment reaches no row at all.
	protected static void SegmentRowRange(float z0, float z1, float rowOriginZ, float rowHeight, int rows, out int lo, out int hi)
	{
		float zMin = Math.Min(z0, z1);
		float zMax = Math.Max(z0, z1);

		lo = (int)Math.Floor((zMin - rowOriginZ) / rowHeight) - 1;
		hi = (int)Math.Floor((zMax - rowOriginZ) / rowHeight) + 1;

		if (lo < 0)
			lo = 0;

		if (hi > rows - 1)
			hi = rows - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a horizontal scan line at z crosses the region's boundary, sorted left to right.
	//!
	//! ⚠ The half-open rule is what guarantees an EVEN number of crossings, and that guarantee is the
	//! only reason even-odd pairing is safe. A segment counts when the line is at or above its lower
	//! end and STRICTLY BELOW its upper one, so a vertex sitting exactly on the line is counted by one
	//! of the two segments meeting there rather than by both or neither.
	//! \param[in] ax Segment start X per segment.
	//! \param[in] az Segment start Z per segment.
	//! \param[in] bx Segment end X per segment.
	//! \param[in] bz Segment end Z per segment.
	//! \param[in] rowSegs Bucketed segment indices.
	//! \param[in] from First index into rowSegs for this row.
	//! \param[in] to One past the last index into rowSegs for this row.
	//! \param[in] z World Z of the scan line.
	//! \param[out] crossings Cleared and refilled with the crossing X values, ascending.
	protected static void CollectCrossings(array<float> ax, array<float> az, array<float> bx, array<float> bz, array<int> rowSegs, int from, int to, float z, notnull array<float> crossings)
	{
		crossings.Clear();

		for (int i = from; i < to; i++)
		{
			int s = rowSegs[i];
			if (s < 0)
				continue;

			float z0 = az[s];
			float z1 = bz[s];

			bool crosses = false;

			if (z0 <= z && z1 > z)
				crosses = true;
			else if (z1 <= z && z0 > z)
				crosses = true;

			if (!crosses)
				continue;

			float t = (z - z0) / (z1 - z0);

			crossings.Insert(ax[s] + (bx[s] - ax[s]) * t);
		}

		SortAscending(crossings);
	}

	//------------------------------------------------------------------------------------------------
	//! Insertion sort, ascending, in place.
	//!
	//! Written out rather than handed to the array's own Sort because what that does to a float array
	//! is not documented in this build, and a scanline that sorts wrong fills the OUTSIDE of every
	//! span. The arrays hold one entry per crossing on one scan line, so the quadratic bound is moot.
	//! \param[inout] values The array to sort.
	protected static void SortAscending(notnull array<float> values)
	{
		int count = values.Count();

		for (int i = 1; i < count; i++)
		{
			float key = values[i];
			int j = i - 1;

			while (j >= 0 && values[j] > key)
			{
				values[j + 1] = values[j];
				j--;
			}

			values[j + 1] = key;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Turns one row's top and bottom crossings into trapezoids, halving the row where its two ends
	//! describe different structure.
	//!
	//! When the two ends agree this is a straight even-odd pairing. When they disagree the row contains
	//! something that changes the structure - a peninsula tip, the first row of a hole, two regions
	//! merging - and pairing span k with span k would join the wrong edges; halving converges on
	//! wherever that happens, and each half shares its inner edge exactly with the other so the fill
	//! still tiles. Only the last sliver, below the subdivision limit, is filled as a rectangle.
	//!
	//! ⚠ Equal span counts are not enough on their own - a region ending while another begins keeps the
	//! count the same and still pairs the wrong two - so paired spans must also overlap in X.
	//! \param[in] ax Segment start X per segment.
	//! \param[in] az Segment start Z per segment.
	//! \param[in] bx Segment end X per segment.
	//! \param[in] bz Segment end Z per segment.
	//! \param[in] rowSegs Bucketed segment indices.
	//! \param[in] from First index into rowSegs for this row.
	//! \param[in] to One past the last index into rowSegs for this row.
	//! \param[in] zTop World Z of the row's top edge.
	//! \param[in] zBot World Z of the row's bottom edge.
	//! \param[in] topCross Crossings at the top edge, ascending.
	//! \param[in] botCross Crossings at the bottom edge, ascending.
	//! \param[in] depth How many times this row has already been halved.
	//! \param[in] appearanceKey Key stamped onto every trapezoid produced.
	//! \param[out] traps Appended with this row's trapezoids.
	protected static void EmitRowTrapezoids(array<float> ax, array<float> az, array<float> bx, array<float> bz, array<int> rowSegs, int from, int to, float zTop, float zBot, array<float> topCross, array<float> botCross, int depth, int appearanceKey, notnull array<ref OVT_TerritoryTrapezoid> traps)
	{
		int topSpans = topCross.Count() / 2;
		int botSpans = botCross.Count() / 2;

		if (topSpans == 0 && botSpans == 0)
			return;

		if (topSpans == botSpans && SpansOverlap(topCross, botCross))
		{
			for (int k = 0; k < topSpans; k++)
			{
				AddTrapezoid(traps, appearanceKey, zTop, zBot, topCross[k * 2], topCross[k * 2 + 1], botCross[k * 2], botCross[k * 2 + 1]);
			}

			return;
		}

		if (depth < MAX_ROW_SUBDIVISIONS)
		{
			float zMid = (zTop + zBot) * 0.5;

			array<float> midCross = new array<float>();
			CollectCrossings(ax, az, bx, bz, rowSegs, from, to, zMid, midCross);

			EmitRowTrapezoids(ax, az, bx, bz, rowSegs, from, to, zTop, zMid, topCross, midCross, depth + 1, appearanceKey, traps);
			EmitRowTrapezoids(ax, az, bx, bz, rowSegs, from, to, zMid, zBot, midCross, botCross, depth + 1, appearanceKey, traps);

			return;
		}

		// THE RESIDUAL SLIVER, taken from whichever end has MORE spans. Covering ground the region has
		// just left is a smaller error than dropping ground it still holds: the first shows as the fill
		// reaching a metre past its own border, the second as a notch in it, and a notch is what the
		// blocky fill this replaced was complained about for.
		array<float> source = topCross;
		if (botSpans > topSpans)
			source = botCross;

		int spans = source.Count() / 2;

		for (int k2 = 0; k2 < spans; k2++)
		{
			AddTrapezoid(traps, appearanceKey, zTop, zBot, source[k2 * 2], source[k2 * 2 + 1], source[k2 * 2], source[k2 * 2 + 1]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether span k at the top of a row overlaps span k at its bottom, for every k.
	//! \param[in] topCross Crossings at the top edge, ascending.
	//! \param[in] botCross Crossings at the bottom edge, ascending, same count as topCross.
	//! \return True when every paired span shares some X with its partner.
	protected static bool SpansOverlap(array<float> topCross, array<float> botCross)
	{
		int spans = topCross.Count() / 2;

		for (int k = 0; k < spans; k++)
		{
			if (topCross[k * 2] > botCross[k * 2 + 1])
				return false;

			if (botCross[k * 2] > topCross[k * 2 + 1])
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Records one trapezoid, dropping the ones with no area on either edge.
	//!
	//! A span with zero width at both ends is a scan line grazing a vertex. It draws nothing, so keeping
	//! it would only cost four projected vertices a frame, forever.
	//! \param[out] traps Appended.
	//! \param[in] appearanceKey Appearance table index.
	//! \param[in] zTop World Z of the top edge.
	//! \param[in] zBot World Z of the bottom edge.
	//! \param[in] topLeft Contour X at the top edge, left side.
	//! \param[in] topRight Contour X at the top edge, right side.
	//! \param[in] botLeft Contour X at the bottom edge, left side.
	//! \param[in] botRight Contour X at the bottom edge, right side.
	protected static void AddTrapezoid(notnull array<ref OVT_TerritoryTrapezoid> traps, int appearanceKey, float zTop, float zBot, float topLeft, float topRight, float botLeft, float botRight)
	{
		if (topRight - topLeft < 0.001 && botRight - botLeft < 0.001)
			return;

		OVT_TerritoryTrapezoid trap = new OVT_TerritoryTrapezoid();
		trap.m_iKey = appearanceKey;
		trap.m_fTopZ = zTop;
		trap.m_fBottomZ = zBot;
		trap.m_fTopLeftX = topLeft;
		trap.m_fTopRightX = topRight;
		trap.m_fBottomLeftX = botLeft;
		trap.m_fBottomRightX = botRight;

		traps.Insert(trap);
	}

	//------------------------------------------------------------------------------------------------
	// PRESENTATION RULES
	//
	// What the overlay SHOWS, as opposed to where anything IS. They live on the solver because the map
	// layer is invisible to every automated tier. ⚠ None of them may ever reach the grid: a square
	// owned by a site that is not drawn is still owned by it, and still takes ground off its neighbours.
	//------------------------------------------------------------------------------------------------

	//! The band rule: whether the ground on the far side of a piece of border belongs to ANOTHER
	//! FACTION, which is the only thing that earns a neutral band.
	//!
	//! A coastline is answered by the first branch - nobody owns water, and a shoreline is nobody's
	//! frontier. Anything unnameable (no owner, index out of range, a site that has gone) is NOT a
	//! frontier, because an omitted edge is a smaller lie than an invented border.
	//! \param[in] otherOwnerIndex Index of the site owning the ground on the far side, or -1 for water.
	//! \param[in] ownFactionIndex Faction of the region the border belongs to.
	//! \param[in] sites Every site, indexed as the grid indexes them.
	//! \return True when the far side belongs to a DIFFERENT faction.
	static bool IsFrontierSide(int otherOwnerIndex, int ownFactionIndex, array<ref OVT_TerritorySite> sites)
	{
		if (otherOwnerIndex < 0)
			return false;

		if (!sites || otherOwnerIndex >= sites.Count())
			return false;

		OVT_TerritorySite other = sites[otherOwnerIndex];
		if (!other)
			return false;

		return other.m_iFactionIndex != ownFactionIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether ground owned by one site is drawn at all.
	//!
	//! The overlay answers "how much does the occupier still hold", not "who holds what" - liberated
	//! ground reads as clean map.
	//!
	//! ⚠ This is an EMIT filter and must never become a COLLECT filter. Every site still competes for
	//! squares, because competing is what pushes the occupier's boundary back. Filter the sites before
	//! building the grid instead and occupier territory flows over ground the player has already taken,
	//! with no error and no log line.
	//! \param[in] cellFactionIndex Faction controlling the owning site.
	//! \param[in] occupyingFactionIndex Faction index of the occupying force, or negative when it could
	//!            not be resolved.
	//! \param[in] onlyOccupying Whether the overlay is restricted to occupier-held ground.
	//! \return True when the ground should be drawn.
	static bool IsEmittedCell(int cellFactionIndex, int occupyingFactionIndex, bool onlyOccupying)
	{
		if (!onlyOccupying)
			return true;

		// An unresolvable occupier cannot be filtered against, and a filter that guesses would blank the
		// entire overlay - the worst and least visible failure this feature has. Unknown means draw
		// everything, which is only ever noisier, never absent.
		if (occupyingFactionIndex < 0)
			return true;

		return cellFactionIndex == occupyingFactionIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether ground is CONTESTED: still controlled by the occupier, but with the population behind
	//! the resistance.
	//!
	//! Support MARKS the region rather than recolouring it - the occupier still controls it, so it
	//! stays the occupier's colour. It is a threshold, not a gradient: a continuous per-site shade
	//! turns a one-faction island into mottled noise.
	//!
	//! ⚠ Towns only. Bases, radio towers and FOBs pass SUPPORT_NONE, which is rejected explicitly
	//! rather than by arithmetic - a threshold configured at or below zero must not silently mark
	//! every military site on the map as contested.
	//! \param[in] cellFactionIndex Faction controlling the owning site.
	//! \param[in] occupyingFactionIndex Faction index of the occupying force, or negative when unknown.
	//! \param[in] supportFraction Resistance support as a fraction of population, or SUPPORT_NONE.
	//! \param[in] threshold Fraction at or above which the region counts as contested.
	//! \return True when the ground should be drawn as contested.
	static bool IsContestedCell(int cellFactionIndex, int occupyingFactionIndex, float supportFraction, float threshold)
	{
		// Nothing can be marked as slipping out of the occupier's hands when the occupier is unknown.
		if (occupyingFactionIndex < 0)
			return false;

		if (cellFactionIndex != occupyingFactionIndex)
			return false;

		if (supportFraction < 0)
			return false;

		return supportFraction >= threshold;
	}
}
