//------------------------------------------------------------------------------------------------
//! TIER A cases - the territory overlay's geometry solver.
//!
//! The overlay is a rendering feature, and almost none of it is visible to an automated tier:
//! colours, alpha, textures, marker occlusion, frame cost and multiplayer agreement are all
//! play-tested. The GEOMETRY is the exception, and OVT_TerritorySolver was split out of the map
//! layer for exactly that reason - it fetches nothing, so it can be built with `new`, fed
//! hand-built sites, and asserted here. These seventeen cases are the entire automated surface of
//! the feature; if the split ever erodes, this file stops compiling in this tier and that is the
//! point.
//!
//! THEY DESCRIBE A GRID, NOT A RAY-MARCH. The representation changed: territory is no longer a
//! radius per angle about each site but an OWNERSHIP GRID - one owner per square of the world - with
//! the fill merged out of it and the border traced through it. The ownership rule and the land
//! predicate are unchanged and are still pinned directly; everything that was about rays, march
//! steps, smoothing exemptions and closed-form takeover radii is gone, and so are the cases that
//! pinned it, because a tested-but-unreachable function is worse than no function.
//!
//! WHAT THEY PIN:
//!  - the land predicate, including the exact boundary, where "just above the shoreline margin"
//!    and "exactly on it" must not be the same answer;
//!  - THE WEIGHTED OWNERSHIP TEST. Two equal-weight sites meet at the midpoint, which every
//!    plausible implementation gets right - so the 2:1 case exists separately, because it is the
//!    ONLY assertion here that can tell a weighted diagram from a plain one. Lose the division and
//!    a military base stops out-projecting a village, silently and everywhere;
//!  - THAT THE GRID COVERS THE WHOLE WORLD, including the partial row and column at its far edge,
//!    and that a square's centre, its edges and its index all agree. An off-by-half here shifts every
//!    region on the map by fifty metres and looks like nothing at all;
//!  - THAT TERRITORY HAS NO MAXIMUM INFLUENCE RADIUS. An isolated site owns every square out to the
//!    far corner of the world, because there is nowhere left to put a reach limit and nothing may
//!    reintroduce one;
//!  - THE COASTLINE, through the virtual land hook, which is the only reason it is virtual. A coast
//!    that stops nothing floods the sea with territory; a coast that stops everything is an overlay
//!    that never appears;
//!  - THAT THE FILL TILES THE DRAWN GROUND EXACTLY - every drawn square covered by exactly one
//!    rectangle, every undrawn square covered by none. This is THE property the whole representation
//!    change exists to buy: gaps and overlaps are not fixed, they are made impossible;
//!  - MARCHING SQUARES, on hand-built fields, including a region with a HOLE and two DISJOINT
//!    regions, because a border tracer that cannot express those is a border tracer that quietly
//!    joins two islands or fills in a lake;
//!  - CHAIKIN CORNER CUTTING, including that zero passes is the identity and that one pass is not,
//!    because "smoothing is tunable to off" and "smoothing does nothing" fail the same assertion;
//!  - THAT A COASTLINE IS NOT A FRONTIER. A border segment earns the neutral band only where the
//!    ground on the far side belongs to ANOTHER FACTION; against water it does not, and against the
//!    same faction there is no border at all. This is the rule two renders got wrong;
//!  - THAT A SITE WHICH IS NOT DRAWN STILL OWNS GROUND. The overlay draws only occupier-held
//!    territory, so a liberated town produces no fill - but it must still take squares off its
//!    occupier neighbours, or occupier colour floods everything the player has liberated and the map
//!    says they have achieved nothing. That is the single most consequential assertion in this file.
//!
//! FLOAT DISCIPLINE. World coordinates are compared with a metre of slack, never with ==. Square
//! indices and counts are integers and ARE compared exactly, because an off-by-one there is a defect
//! rather than a rounding.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Hand-built subjects and the tolerant comparisons shared by the cases below.
//------------------------------------------------------------------------------------------------
class OVT_TEST_Logic_TerritoryFixture
{
	//! A metre. Coarser than any float error produced here and finer than any distance asserted.
	static const float DISTANCE_TOLERANCE = 1;

	//------------------------------------------------------------------------------------------------
	//! Builds one site with every field set explicitly - `new` applies no attribute defaults, and a
	//! zero weight would silently disqualify the site from owning anything.
	//! \param[in] x World X.
	//! \param[in] z World Z.
	//! \param[in] weight Multiplicative influence weight.
	//! \return The site.
	static OVT_TerritorySite MakeSite(float x, float z, float weight)
	{
		OVT_TerritorySite site = new OVT_TerritorySite();
		site.m_vPos = Vector(x, 0, z);
		site.m_fWeight = weight;
		site.m_iFactionIndex = 0;

		// SUPPORT_NONE, not zero: these sites stand in for whatever type a case needs, and "no support
		// to measure" is the state every type but a town is actually in.
		site.m_fSupportFraction = OVT_TerritorySite.SUPPORT_NONE;
		site.m_sTypeId = "test";
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! MakeSite, with the controlling faction chosen. Separate rather than a fourth parameter on
	//! MakeSite, so the cases that do not care about faction keep the one-faction world they were
	//! written against.
	//! \param[in] x World X.
	//! \param[in] z World Z.
	//! \param[in] weight Multiplicative influence weight.
	//! \param[in] factionIndex Controlling faction index.
	//! \return The site.
	static OVT_TerritorySite MakeFactionSite(float x, float z, float weight, int factionIndex)
	{
		OVT_TerritorySite site = MakeSite(x, z, weight);
		site.m_iFactionIndex = factionIndex;
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! A solver with every knob set explicitly and no world at all, so every point is land unless a
	//! subclass says otherwise.
	//! \return The solver.
	static OVT_TerritorySolver MakeSolver()
	{
		OVT_TerritorySolver solver = new OVT_TerritorySolver();
		Configure(solver);
		return solver;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the shared knob values to a solver.
	//! \param[in] solver The solver to configure.
	static void Configure(OVT_TerritorySolver solver)
	{
		solver.m_fGridCellSize = 100;
		solver.m_iSmoothPasses = 0;
		solver.m_fShorelineMargin = 0.5;
		solver.m_bClipToCoast = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds an ownership grid straight from a literal owner array, with no solving at all.
	//!
	//! This is what lets the merger and the tracer be asserted against a field somebody wrote out by
	//! hand, rather than against whatever a solve happened to produce. A tracer tested only on solved
	//! input can never be shown a hole or two disjoint regions on purpose.
	//! \param[in] cols Columns.
	//! \param[in] rows Rows.
	//! \param[in] cellSize Square size in metres.
	//! \param[in] owners Owner index per square, row-major, -1 for water.
	//! \return The grid.
	static OVT_TerritoryGrid MakeGrid(int cols, int rows, float cellSize, array<int> owners)
	{
		OVT_TerritoryGrid grid = new OVT_TerritoryGrid();
		grid.m_fOriginX = 0;
		grid.m_fOriginZ = 0;
		grid.m_fCellSize = cellSize;
		grid.m_iCols = cols;
		grid.m_iRows = rows;
		grid.m_aOwner = new array<int>();

		foreach (int owner : owners)
		{
			grid.m_aOwner.Insert(owner);
		}

		return grid;
	}

	//------------------------------------------------------------------------------------------------
	//! Tolerant distance comparison. Never compare world coordinates with ==.
	//! \param[in] actual Observed value.
	//! \param[in] expected Expected value.
	//! \return True when the two agree within DISTANCE_TOLERANCE.
	static bool Near(float actual, float expected)
	{
		return Math.AbsFloat(actual - expected) <= DISTANCE_TOLERANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! An axis-aligned rectangular contour loop, wound so the enclosed area is positive.
	//!
	//! The scanline decomposition works on the EVEN-ODD rule, so it does not care which way a loop is
	//! wound - which is exactly why a hole and a second island are the same input to it. These helpers
	//! therefore do not bother reversing anything, and a case that wants a hole simply adds a second
	//! rectangle inside the first.
	//! \param[in] x0 Minimum world X.
	//! \param[in] z0 Minimum world Z.
	//! \param[in] x1 Maximum world X.
	//! \param[in] z1 Maximum world Z.
	//! \param[in] key Appearance key the loop belongs to.
	//! \return The contour.
	static OVT_TerritoryContour MakeRectContour(float x0, float z0, float x1, float z1, int key)
	{
		OVT_TerritoryContour contour = new OVT_TerritoryContour();
		contour.m_aX = new array<float>();
		contour.m_aZ = new array<float>();
		contour.m_aFrontier = new array<bool>();
		contour.m_iAppearanceKey = key;

		contour.m_aX.Insert(x0); contour.m_aZ.Insert(z0);
		contour.m_aX.Insert(x1); contour.m_aZ.Insert(z0);
		contour.m_aX.Insert(x1); contour.m_aZ.Insert(z1);
		contour.m_aX.Insert(x0); contour.m_aZ.Insert(z1);

		for (int i = 0; i < 4; i++)
		{
			contour.m_aFrontier.Insert(false);
		}

		return contour;
	}

	//------------------------------------------------------------------------------------------------
	//! A contour from literal coordinates, for the shapes a rectangle cannot express.
	//! \param[in] xs World X per point.
	//! \param[in] zs World Z per point, same length as xs.
	//! \param[in] key Appearance key the loop belongs to.
	//! \return The contour.
	static OVT_TerritoryContour MakeContour(array<float> xs, array<float> zs, int key)
	{
		OVT_TerritoryContour contour = new OVT_TerritoryContour();
		contour.m_aX = new array<float>();
		contour.m_aZ = new array<float>();
		contour.m_aFrontier = new array<bool>();
		contour.m_iAppearanceKey = key;

		for (int i = 0; i < xs.Count(); i++)
		{
			contour.m_aX.Insert(xs[i]);
			contour.m_aZ.Insert(zs[i]);
			contour.m_aFrontier.Insert(false);
		}

		return contour;
	}

	//------------------------------------------------------------------------------------------------
	//! Every trapezoid whose top edge sits at a given world Z, in the order they were produced.
	//!
	//! Filtering by the row rather than asserting on the whole list is what lets a case pick a row it
	//! knows the exact answer for and ignore the rows where the region begins or ends - which are
	//! legitimately subdivided, and whose sub-rows carry different top edges.
	//! \param[in] traps Every trapezoid.
	//! \param[in] topZ World Z of the row's top edge.
	//! \param[out] found Cleared and refilled with the matching trapezoids.
	static void TrapezoidsAtRow(array<ref OVT_TerritoryTrapezoid> traps, float topZ, notnull array<ref OVT_TerritoryTrapezoid> found)
	{
		found.Clear();

		foreach (OVT_TerritoryTrapezoid trap : traps)
		{
			if (Near(trap.m_fTopZ, topZ))
				found.Insert(trap);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Total area of a set of trapezoids.
	//!
	//! THIS IS THE ASSERTION THAT CATCHES A STEPPED FILL. A decomposition that took each row's width
	//! from one of its edges and drew a rectangle would tile just as cleanly, have exactly as many
	//! quads, and be wrong by half the area under every slanted stretch of border - which is the whole
	//! defect the trapezoids exist to remove. Only the area says so.
	//! \param[in] traps The trapezoids.
	//! \return Total area in square metres.
	static float TrapezoidArea(array<ref OVT_TerritoryTrapezoid> traps)
	{
		float total = 0;

		foreach (OVT_TerritoryTrapezoid trap : traps)
		{
			float top = trap.m_fTopRightX - trap.m_fTopLeftX;
			float bottom = trap.m_fBottomRightX - trap.m_fBottomLeftX;

			total += (top + bottom) * 0.5 * (trap.m_fBottomZ - trap.m_fTopZ);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a value is a whole multiple of a step.
	//!
	//! Deliberately far tighter than the metre of slack world coordinates get. "Near a multiple of 100"
	//! is satisfied by every value on the map; the whole point of snapping is that the origin lands ON a
	//! multiple, and only an exact test says that.
	//! \param[in] value The value.
	//! \param[in] step The step.
	//! \return True when value / step is a whole number.
	static bool IsMultipleOf(float value, float step)
	{
		if (step <= 0)
			return false;

		float steps = value / step;

		return Math.AbsFloat(steps - Math.Round(steps)) < 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! Twice the signed area of a closed contour, by the shoelace formula.
	//!
	//! THE SIGN IS THE INTERESTING PART, not the magnitude: the tracer emits every segment with the
	//! region on its LEFT, so an outer boundary comes out counter-clockwise and POSITIVE while the
	//! boundary of a HOLE comes out clockwise and NEGATIVE. That one number is how a case can tell a
	//! lake from an island without inspecting a single vertex.
	//! \param[in] contour The contour.
	//! \return Signed area in square metres.
	static float SignedArea(OVT_TerritoryContour contour)
	{
		float total = 0;
		int count = contour.m_aX.Count();

		for (int i = 0; i < count; i++)
		{
			int j = i + 1;
			if (j >= count)
				j = 0;

			total += contour.m_aX[i] * contour.m_aZ[j] - contour.m_aX[j] * contour.m_aZ[i];
		}

		return total * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Paints every rectangle onto a coverage map and reports the first square that is wrong.
	//!
	//! THIS IS THE ASSERTION THE WHOLE REPRESENTATION CHANGE EXISTS FOR. A square that should be drawn
	//! and is covered twice is an overlap - two semi-transparent fills compositing into a darker
	//! patch. A square that should be drawn and is covered zero times is a gap - the pale seams that
	//! made three renders read as splotchy. A square that should NOT be drawn and is covered at all is
	//! territory spilling into the sea or over liberated ground.
	//! \param[in] grid The ownership grid.
	//! \param[in] siteAppearance Appearance key per site index, -1 where not drawn.
	//! \param[in] rects The merged rectangles.
	//! \param[out] failure A description of the first fault found, empty when there is none.
	//! \return True when every square is covered exactly as it should be.
	static bool CoversExactlyOnce(OVT_TerritoryGrid grid, array<int> siteAppearance, array<ref OVT_TerritoryRect> rects, out string failure)
	{
		failure = "";

		array<int> cover = new array<int>();
		array<int> keyAt = new array<int>();

		int total = grid.m_iCols * grid.m_iRows;

		for (int i = 0; i < total; i++)
		{
			cover.Insert(0);
			keyAt.Insert(-1);
		}

		foreach (OVT_TerritoryRect rect : rects)
		{
			for (int r = rect.m_iRow0; r <= rect.m_iRow1; r++)
			{
				for (int c = rect.m_iCol0; c <= rect.m_iCol1; c++)
				{
					int index = r * grid.m_iCols + c;
					cover[index] = cover[index] + 1;
					keyAt[index] = rect.m_iKey;
				}
			}
		}

		for (int row = 0; row < grid.m_iRows; row++)
		{
			for (int col = 0; col < grid.m_iCols; col++)
			{
				int at = row * grid.m_iCols + col;

				int owner = grid.OwnerAt(col, row);
				int expectedKey = -1;
				if (owner >= 0 && owner < siteAppearance.Count())
					expectedKey = siteAppearance[owner];

				int expectedCover = 0;
				if (expectedKey >= 0)
					expectedCover = 1;

				if (cover[at] != expectedCover)
				{
					failure = string.Format("square %1,%2 is covered %3 times, expected %4", col, row, cover[at], expectedCover);
					return false;
				}

				if (expectedCover == 1 && keyAt[at] != expectedKey)
				{
					failure = string.Format("square %1,%2 was drawn with appearance %3 instead of %4", col, row, keyAt[at], expectedKey);
					return false;
				}
			}
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A solver with a stubbed coastline and no world.
//!
//! This subclass is why IsLandAt is virtual. It answers the land question from a synthetic height
//! field - dry land south of a known Z, seabed north of it - and routes that answer through the
//! REAL IsLand predicate, so the grid is classified against the same comparison production uses
//! rather than against a bare boolean.
//------------------------------------------------------------------------------------------------
class OVT_TEST_Logic_TerritoryCoastSolver : OVT_TerritorySolver
{
	//! Everything north of this Z is water.
	static const float COAST_Z = 300;

	//! Synthetic terrain height on the land side.
	static const float LAND_HEIGHT = 10;

	//! Synthetic terrain height on the water side.
	static const float SEABED_HEIGHT = -10;

	//------------------------------------------------------------------------------------------------
	//! \param[in] x World X, unused by the stub.
	//! \param[in] z World Z, which is what the synthetic coast runs along.
	//! \return True when the point is on the land side of the stubbed coast.
	override bool IsLandAt(float x, float z)
	{
		float surfaceY;
		if (z <= COAST_Z)
			surfaceY = LAND_HEIGHT;
		else
			surfaceY = SEABED_HEIGHT;

		return OVT_TerritorySolver.IsLand(surfaceY, 0, m_fShorelineMargin);
	}
}

//------------------------------------------------------------------------------------------------
//! Case 1 - the land predicate, above, below and EXACTLY AT the shoreline margin.
//!
//! The margin is the clearance demanded, so a surface sitting exactly on it is water. Asserting the
//! boundary matters because it is the difference between territory ending at the shore and territory
//! ending a square short of it along every beach on the map.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_LandPredicate : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (!OVT_TerritorySolver.IsLand(10, 0, 0.5))
		{
			SetResultFailure("Terrain ten metres above sea level was treated as water; no region would reach any coast");
			return true;
		}

		if (OVT_TerritorySolver.IsLand(-3, 0, 0.5))
		{
			SetResultFailure("Seabed three metres BELOW sea level was treated as land; territory would extend into open sea");
			return true;
		}

		// Exactly on the margin. The comparison is strict, so this is water.
		if (OVT_TerritorySolver.IsLand(0.5, 0, 0.5))
		{
			SetResultFailure("Terrain sitting EXACTLY on the shoreline margin was treated as land; the margin bounds nothing");
			return true;
		}

		// And the margin must actually participate: a metre of surface clears a half-metre margin.
		if (!OVT_TerritorySolver.IsLand(1, 0, 0.5))
		{
			SetResultFailure("Terrain clearing the shoreline margin was treated as water");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 2 - two EQUAL-weight sites meet at the midpoint of the line joining them, both as a point
//! predicate and as the column where the grid's owner changes.
//!
//! The baseline every weighted diagram must still reproduce. It cannot detect a lost weight
//! division on its own - that is the next case's job - but it is what catches an ownership test
//! that picks the FARTHEST site, which would turn every region inside out.
//!
//! The grid half matters separately from the predicate half: it is what says the sampling actually
//! ASKS the predicate at the square centres it claims to.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_OwnsEqualWeights : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(0, 0, 1));
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(1000, 0, 1));

		int owner = OVT_TerritorySolver.OwnsPoint(Vector(490, 0, 0), sites);
		if (owner != 0)
		{
			SetResultFailure("A point 490 m from an equal-weight site and 510 m from its rival was owned by site %1, not the nearer one", owner.ToString());
			return true;
		}

		owner = OVT_TerritorySolver.OwnsPoint(Vector(510, 0, 0), sites);
		if (owner != 1)
		{
			SetResultFailure("A point past the midpoint between two equal-weight sites was owned by site %1, not the nearer one", owner.ToString());
			return true;
		}

		// And the grid agrees. Square centres along row 0 sit at -450, -350 ... 450, 550, so the
		// boundary at 500 m falls exactly between columns 9 and 10.
		OVT_TerritorySolver solver = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		solver.SetGridExtent(-500, -100, 1500, 100);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		if (grid.OwnerAt(9, 0) != 0)
		{
			SetResultFailure("The square whose centre sits 450 m from the first site was owned by site %1, not by the site it is nearest", grid.OwnerAt(9, 0).ToString());
			return true;
		}

		if (grid.OwnerAt(10, 0) != 1)
		{
			SetResultFailure("The square whose centre sits past the midpoint was owned by site %1; the boundary is not where the ownership test puts it", grid.OwnerAt(10, 0).ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 3 - THE WEIGHTING CASE. Two sites weighted 2:1 meet two thirds of the way toward the
//! weaker one.
//!
//! This is the only assertion in the file that can tell a multiplicatively-weighted diagram from a
//! plain Voronoi one, and it exists because the equal-weight case above stays green when the weight
//! division is dropped. Lose the division and every boundary retreats to the midpoint: bases stop
//! out-projecting towns, capitals stop out-projecting villages, and nothing errors.
//!
//! With sites 900 m apart weighted 2 and 1, the boundary solves dist(A)/2 = dist(B)/1, i.e. 600 m
//! from the strong site and 300 m from the weak one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_OwnsWeighted : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(0, 0, 2));
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(900, 0, 1));

		// Just short of the weighted boundary: still the strong site's, even though the weak site is
		// only 310 m away and the strong one is 590 m away. Unweighted, this point would be the weak
		// site's - which is precisely the failure being pinned.
		int owner = OVT_TerritorySolver.OwnsPoint(Vector(590, 0, 0), sites);
		if (owner != 0)
		{
			SetResultFailure("A point 590 m from a weight-2 site and 310 m from a weight-1 rival was owned by site %1; the weight division is not being applied", owner.ToString());
			return true;
		}

		owner = OVT_TerritorySolver.OwnsPoint(Vector(610, 0, 0), sites);
		if (owner != 1)
		{
			SetResultFailure("A point past the weighted boundary was owned by site %1, not the weak site whose territory it is", owner.ToString());
			return true;
		}

		// And the grid puts the boundary two thirds of the way out rather than at the midpoint. Column
		// 10's centre is 550 m out and column 11's is 650 m, so the change belongs between them.
		OVT_TerritorySolver solver = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		solver.SetGridExtent(-500, -100, 1500, 100);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		if (grid.OwnerAt(10, 0) != 0)
		{
			SetResultFailure("The square 550 m from a weight-2 site was owned by site %1; at the midpoint rule it would change hands here, which is the unweighted answer", grid.OwnerAt(10, 0).ToString());
			return true;
		}

		if (grid.OwnerAt(11, 0) != 1)
		{
			SetResultFailure("The square 650 m from a weight-2 site was owned by site %1; the weighted boundary sits at 600 m and this square is past it", grid.OwnerAt(11, 0).ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 4 - AN ISOLATED SITE OWNS EVERYTHING, out to the far corner of the world.
//!
//! THIS IS THE CASE THAT PINS "TERRITORY HAS NO MAXIMUM INFLUENCE RADIUS". The grid has no concept
//! of a radius at all, which is how the requirement is now met - but "there is nowhere to put one"
//! is only true while nobody puts one back, and a reach limit reintroduced here would look exactly
//! like the first render the user rejected: islands of colour with dead ground between them.
//!
//! The far corner of a 2000 x 2000 extent is 1414 m from a site at its centre, which is well past
//! any radius anybody would be tempted to write down.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_IsolatedSite : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(0, 0, 1));

		OVT_TerritorySolver solver = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		solver.SetGridExtent(-1000, -1000, 1000, 1000);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		if (grid.m_iCols != 20 || grid.m_iRows != 20)
		{
			SetResultFailure("A 2000 m extent at 100 m squares produced %1 x %2 squares instead of 20 x 20", grid.m_iCols.ToString(), grid.m_iRows.ToString());
			return true;
		}

		for (int r = 0; r < grid.m_iRows; r++)
		{
			for (int c = 0; c < grid.m_iCols; c++)
			{
				if (grid.OwnerAt(c, r) == 0)
					continue;

				SetResultFailure("Square %1 was unowned with a single site on an all-land world; territory has no maximum influence radius and nothing may reintroduce one", (r * grid.m_iCols + c).ToString());
				return true;
			}
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 5 - the stubbed coastline decides which squares are ownable, and the switch that disables it
//! actually disables it.
//!
//! Uses the virtual land hook, which is the only reason it is virtual. Both halves matter: a coast
//! that stops nothing leaves territory floating in open sea, and a coast that stops everything is
//! an overlay that never appears.
//!
//! WATER AND UNOWNED ARE THE SAME ANSWER - -1 - and that is deliberate rather than sloppy: neither is
//! drawn and neither is a frontier, so a square in the sea and a square nobody could win are the same
//! thing to everything downstream.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_CoastStop : SCR_AutotestCaseBase
{
	//! Row 4's centre sits at 250 m, on the land side of the stubbed coast at 300 m; row 5's sits at
	//! 350 m and is in the sea.
	static const int LAST_LAND_ROW = 4;
	static const int FIRST_WATER_ROW = 5;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(0, 0, 1));

		OVT_TEST_Logic_TerritoryCoastSolver solver = new OVT_TEST_Logic_TerritoryCoastSolver();
		OVT_TEST_Logic_TerritoryFixture.Configure(solver);
		solver.SetGridExtent(-200, -200, 200, 600);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		if (grid.OwnerAt(0, LAST_LAND_ROW) != 0)
		{
			SetResultFailure("The last square before the shore was unowned; the coast test is rejecting land and the overlay would stop short of every beach");
			return true;
		}

		if (grid.OwnerAt(0, FIRST_WATER_ROW) != -1)
		{
			SetResultFailure("A square in open sea was owned by site %1; territory would be drawn over the water", grid.OwnerAt(0, FIRST_WATER_ROW).ToString());
			return true;
		}

		// The switch has to be the ONLY thing that turns the coast off, or a stubbed land hook can
		// silently stop participating and every case above it goes on passing.
		solver.m_bClipToCoast = false;

		OVT_TerritoryGrid unclipped = solver.BuildGrid(sites);

		if (unclipped.OwnerAt(0, FIRST_WATER_ROW) != 0)
		{
			SetResultFailure("With coast clipping switched off, the sea square was still unowned; the land test is being consulted when it was told not to be");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 6 - THE GRID COVERS THE WHOLE WORLD, its coordinates agree with its indices, and its square
//! count has a ceiling.
//!
//! An off-by-half between a square's centre and its edges shifts every region on the map by fifty
//! metres, and looks like absolutely nothing - the overlay is still plausible, just wrong. A dropped
//! partial column at the far edge leaves a strip of the map permanently unclassified, which reads as
//! a coastline that is not there.
//!
//! THE CEILING IS A SAFETY VALVE AND IT IS TESTED AS ONE. Cost is quadratic in the square size, so a
//! mistyped cell size is a hung client rather than a slow one. The builder grows the square size
//! until the grid fits and records what it settled on, so the failure is a coarse overlay that says
//! so in the log.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_GridIndexing : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(0, 0, 1));

		OVT_TerritorySolver solver = OVT_TEST_Logic_TerritoryFixture.MakeSolver();

		// A span that is NOT a whole number of squares. 1050 m at 100 m squares is ten and a half, and
		// the half has to become an eleventh column or the last 50 m of the world is never classified.
		solver.SetGridExtent(0, 0, 1050, 2000);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		if (grid.m_iCols != 11)
		{
			SetResultFailure("A 1050 m span at 100 m squares produced %1 columns instead of 11; the partial column at the far edge was dropped and that strip of the map is never classified", grid.m_iCols.ToString());
			return true;
		}

		if (grid.m_iRows != 20)
		{
			SetResultFailure("A 2000 m span at 100 m squares produced %1 rows instead of 20", grid.m_iRows.ToString());
			return true;
		}

		if (grid.m_aOwner.Count() != grid.m_iCols * grid.m_iRows)
		{
			SetResultFailure("The grid declared %1 squares but stored %2 owners; every index into it past the shorter of the two is a lie", (grid.m_iCols * grid.m_iRows).ToString(), grid.m_aOwner.Count().ToString());
			return true;
		}

		// A square's centre is half a square past its low edge, and its high edge is the next square's
		// low edge. Everything drawn is cut on edges and everything classified is sampled at centres,
		// so the two have to agree.
		if (!OVT_TEST_Logic_TerritoryFixture.Near(grid.EdgeX(0), 0) || !OVT_TEST_Logic_TerritoryFixture.Near(grid.CentreX(0), 50) || !OVT_TEST_Logic_TerritoryFixture.Near(grid.EdgeX(1), 100))
		{
			SetResultFailure("Square 0 reported low edge %1 and centre %2; a centre must sit half a square inside its own low edge", grid.EdgeX(0).ToString(), grid.CentreX(0).ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(grid.EdgeZ(20), 2000))
		{
			SetResultFailure("The high edge of the last row came out at %1 instead of 2000; the fill would stop short of the world", grid.EdgeZ(20).ToString());
			return true;
		}

		// Outside the grid is UNOWNED, and the contour tracer depends on it: it walks one square past
		// every edge so a region touching the edge of the world still closes into a loop.
		if (grid.OwnerAt(-1, 0) != -1 || grid.OwnerAt(11, 0) != -1 || grid.OwnerAt(0, 20) != -1)
		{
			SetResultFailure("A square outside the grid reported an owner; the border tracer reads one square past every edge and would close a region against ground that does not exist");
			return true;
		}

		// THE CEILING. 600 x 600 squares at 1 m is 360,000, past the cap, so the builder must grow the
		// square size rather than allocate it.
		OVT_TerritorySolver huge = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		huge.m_fGridCellSize = 1;
		huge.SetGridExtent(0, 0, 600, 600);

		OVT_TerritoryGrid coarse = huge.BuildGrid(sites);

		if (coarse.CellCount() > OVT_TerritorySolver.MAX_GRID_CELLS)
		{
			SetResultFailure("A 1 m square size over a 600 m world produced %1 squares, past the %2 ceiling; a mistyped cell size must cost resolution, never a hung client", coarse.CellCount().ToString(), OVT_TerritorySolver.MAX_GRID_CELLS.ToString());
			return true;
		}

		if (coarse.m_fCellSize <= 1)
		{
			SetResultFailure("The grid stayed at a %1 m square size while claiming to be under the ceiling; whatever it actually built at is what the log has to report", coarse.m_fCellSize.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 7 - RUN MERGING: maximal horizontal runs, broken by water and by a change of appearance,
//! merged across a change of OWNER.
//!
//! WHY MERGING ACROSS AN OWNER MATTERS. Two neighbouring towns of one faction draw in exactly the
//! same colour, so a row crossing six of them must cost one quad rather than six. Keying the merge on
//! the owning site instead would multiply the fill geometry by the number of sites a row crosses for
//! no visible difference at all - the map would look identical and cost several times as much.
//!
//! WHY BREAKING ON WATER MATTERS MORE. A run that swallows a water square paints the sea. That is the
//! one direction of this optimisation that is visible, and it is visible as territory in the ocean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_RunMerging : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// One row: two sites of one appearance, two of another, a water square, then more of the
		// second appearance. Sites 0 and 1 share appearance 0; site 2 carries appearance 1.
		array<int> owners = new array<int>();
		owners.Insert(0);
		owners.Insert(0);
		owners.Insert(1);
		owners.Insert(1);
		owners.Insert(2);
		owners.Insert(2);
		owners.Insert(-1);
		owners.Insert(2);

		OVT_TerritoryGrid grid = OVT_TEST_Logic_TerritoryFixture.MakeGrid(8, 1, 100, owners);

		array<int> appearance = new array<int>();
		appearance.Insert(0);
		appearance.Insert(0);
		appearance.Insert(1);

		array<ref OVT_TerritoryRect> rects = new array<ref OVT_TerritoryRect>();
		OVT_TerritorySolver.BuildRects(grid, appearance, false, rects);

		if (rects.Count() != 3)
		{
			SetResultFailure("Eight squares merged into %1 runs instead of 3; expected columns 0-3 as one appearance, 4-5 as another, and 7 alone past the water", rects.Count().ToString());
			return true;
		}

		if (rects[0].m_iCol0 != 0 || rects[0].m_iCol1 != 3)
		{
			SetResultFailure("The first run covered columns %1 to %2 instead of 0 to 3; two neighbouring sites of one appearance must merge into a single quad", rects[0].m_iCol0.ToString(), rects[0].m_iCol1.ToString());
			return true;
		}

		if (rects[1].m_iCol0 != 4 || rects[1].m_iCol1 != 5)
		{
			SetResultFailure("The second run covered columns %1 to %2 instead of 4 to 5; a change of appearance must break a run even when the ground is continuous", rects[1].m_iCol0.ToString(), rects[1].m_iCol1.ToString());
			return true;
		}

		if (rects[2].m_iCol0 != 7 || rects[2].m_iCol1 != 7)
		{
			SetResultFailure("The third run covered columns %1 to %2 instead of column 7 alone; a water square must break a run rather than be painted over", rects[2].m_iCol0.ToString(), rects[2].m_iCol1.ToString());
			return true;
		}

		// AND A SITE THAT IS NOT DRAWN CONTRIBUTES NO GEOMETRY, while still owning its squares. This is
		// the emit filter seen from the merger's side.
		array<int> hidden = new array<int>();
		hidden.Insert(0);
		hidden.Insert(0);
		hidden.Insert(-1);

		OVT_TerritorySolver.BuildRects(grid, hidden, false, rects);

		if (rects.Count() != 1)
		{
			SetResultFailure("With one site's appearance suppressed the merger produced %1 runs instead of 1; ground owned by a site that is not drawn must produce no quad at all", rects.Count().ToString());
			return true;
		}

		if (rects[0].m_iCol1 != 3)
		{
			SetResultFailure("The surviving run reached column %1 instead of 3; suppressing a site must remove its ground, not hand it to its neighbour", rects[0].m_iCol1.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 8 - THE FILL TILES THE DRAWN GROUND EXACTLY, with and without the vertical merge.
//!
//! THIS IS THE CASE THE WHOLE REPRESENTATION CHANGE EXISTS FOR. The ray-march it replaced described
//! each region as a polygon about its own site, and two neighbours inscribing one curved boundary
//! with different vertices alternated GAP and OVERLAP along it - which is the pale seam and the dark
//! hairline three renders complained about. On a grid neither is expressible, and this is the
//! assertion that says so: every drawn square covered by exactly one rectangle, every undrawn square
//! covered by none, on a deliberately ragged field with two appearances and holes in it.
//!
//! The vertical merge is a pure optimisation, so it is asserted twice - once for the property it must
//! not break, and once for the saving it is supposed to buy. An optimisation that quietly changes
//! geometry is the failure this half exists for.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_FillTiling : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	//! A ragged six-by-five field: two appearances, a bay eaten out of one side, two rows that are
	//! IDENTICAL so the vertical merge has something to find, and one row that shares a run's starting
	//! column while ENDING somewhere else so it has something it must refuse to merge.
	//!
	//! That last row is the whole reason the field is shaped this way rather than generated. A merge
	//! that matched runs on their starting column alone would swallow it, and the rectangle it produced
	//! would paint two squares of open water - which is the failure the coverage assertion exists for.
	//! \return Owner index per square, row-major.
	protected array<int> MakeRaggedField()
	{
		array<int> owners = new array<int>();

		// row 0 - a bay at the left, and the second appearance is one square wide
		owners.Insert(-1); owners.Insert(0); owners.Insert(0); owners.Insert(0); owners.Insert(1); owners.Insert(-1);
		// row 1
		owners.Insert(0); owners.Insert(0); owners.Insert(0); owners.Insert(1); owners.Insert(1); owners.Insert(-1);
		// row 2 - IDENTICAL to row 1, and adjacent to it, so both its runs must collapse into row 1's
		owners.Insert(0); owners.Insert(0); owners.Insert(0); owners.Insert(1); owners.Insert(1); owners.Insert(-1);
		// row 3 - the trap: the first run still starts at column 0 but now ENDS at column 1
		owners.Insert(0); owners.Insert(0); owners.Insert(-1); owners.Insert(-1); owners.Insert(1); owners.Insert(1);
		// row 4
		owners.Insert(-1); owners.Insert(-1); owners.Insert(0); owners.Insert(1); owners.Insert(-1); owners.Insert(-1);

		return owners;
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TerritoryGrid grid = OVT_TEST_Logic_TerritoryFixture.MakeGrid(6, 5, 100, MakeRaggedField());

		array<int> appearance = new array<int>();
		appearance.Insert(0);
		appearance.Insert(1);

		array<ref OVT_TerritoryRect> flat = new array<ref OVT_TerritoryRect>();
		OVT_TerritorySolver.BuildRects(grid, appearance, false, flat);

		string failure;

		if (!OVT_TEST_Logic_TerritoryFixture.CoversExactlyOnce(grid, appearance, flat, failure))
		{
			SetResultFailure("Without the vertical merge the fill did not tile the drawn ground: %1. A square covered twice is the dark hairline and a square covered zero times is the pale seam", failure);
			return true;
		}

		array<ref OVT_TerritoryRect> merged = new array<ref OVT_TerritoryRect>();
		int runCount = OVT_TerritorySolver.BuildRects(grid, appearance, true, merged);

		if (!OVT_TEST_Logic_TerritoryFixture.CoversExactlyOnce(grid, appearance, merged, failure))
		{
			SetResultFailure("WITH the vertical merge the fill no longer tiled the drawn ground: %1. The merge is a pure optimisation and may not move one square of geometry", failure);
			return true;
		}

		if (runCount != flat.Count())
		{
			SetResultFailure("The merger reported %1 runs before merging but produced %2 unmerged rectangles; the number the solve log prints has to be the number that existed", runCount.ToString(), flat.Count().ToString());
			return true;
		}

		// AND IT MUST ACTUALLY BUY SOMETHING. Rows 1 and 3 are identical, so at least one pair of runs
		// has to collapse - a merge that is always correct and never fires is dead code.
		if (merged.Count() >= flat.Count())
		{
			SetResultFailure("The vertical merge produced %1 rectangles from %2 runs; two identical rows sit in this field and at least one pair must collapse", merged.Count().ToString(), flat.Count().ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 9 - MARCHING SQUARES traces one closed, correctly oriented loop around a hand-built region.
//!
//! The field is a two-by-two block of owned squares in a sea of nothing, and the answer is exactly
//! knowable: the contour runs through the midpoints of the edges between neighbouring square
//! CENTRES, so it comes out as an octagon with the four corners cut, not as the block's own outline.
//! That is the correct behaviour and it is worth pinning as a shape rather than as a vertex count,
//! because a tracer that emitted the block outline instead would be a tracer working on the wrong
//! lattice.
//!
//! ORIENTATION IS ASSERTED THROUGH THE SIGNED AREA, and it is load-bearing rather than cosmetic: the
//! band offsets INWARD along the left of travel, so a loop wound the other way would draw every
//! neutral band on the neighbour's ground instead of its own.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ContourTrace : SCR_AutotestCaseBase
{
	//! A 200 m square with four 50 m corner triangles cut off it.
	static const float EXPECTED_AREA = 35000;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<int> owners = new array<int>();

		for (int r = 0; r < 4; r++)
		{
			for (int c = 0; c < 4; c++)
			{
				if (c >= 1 && c <= 2 && r >= 1 && r <= 2)
					owners.Insert(0);
				else
					owners.Insert(-1);
			}
		}

		OVT_TerritoryGrid grid = OVT_TEST_Logic_TerritoryFixture.MakeGrid(4, 4, 100, owners);

		array<bool> inRegion = new array<bool>();
		inRegion.Insert(true);

		array<bool> rivalFaction = new array<bool>();
		rivalFaction.Insert(false);

		array<ref OVT_TerritoryContour> contours = new array<ref OVT_TerritoryContour>();
		OVT_TerritorySolver.TraceRegion(grid, inRegion, rivalFaction, contours);

		if (contours.Count() != 1)
		{
			SetResultFailure("A single connected region produced %1 border loops instead of 1; a tracer that cannot close a loop leaves dangling chains that draw as nothing", contours.Count().ToString());
			return true;
		}

		OVT_TerritoryContour contour = contours[0];

		if (contour.m_aX.Count() != 8)
		{
			SetResultFailure("The border of a two-by-two block came out with %1 points instead of 8; it is traced between square CENTRES, so the four corners are cut", contour.m_aX.Count().ToString());
			return true;
		}

		if (contour.m_aFrontier.Count() != contour.m_aX.Count())
		{
			SetResultFailure("A contour carried %1 points and %2 segment flags; the two must be parallel or the band is drawn against the wrong stretch of border", contour.m_aX.Count().ToString(), contour.m_aFrontier.Count().ToString());
			return true;
		}

		float area = OVT_TEST_Logic_TerritoryFixture.SignedArea(contour);

		if (area <= 0)
		{
			SetResultFailure("The border of a region came out with signed area %1; it must wind so the region is on the LEFT, or every neutral band offsets outward onto the neighbour's ground", area.ToString());
			return true;
		}

		if (Math.AbsFloat(area - EXPECTED_AREA) > 100)
		{
			SetResultFailure("The traced border enclosed %1 square metres instead of %2; the loop is not following the edges between square centres", area.ToString(), EXPECTED_AREA.ToString());
			return true;
		}

		// Every step has to be a real neighbour hop. A tracer that joined the wrong pair of nodes would
		// still close a loop and still enclose an area - it would just cross the region diagonally.
		int count = contour.m_aX.Count();

		for (int i = 0; i < count; i++)
		{
			int j = i + 1;
			if (j >= count)
				j = 0;

			float dx = contour.m_aX[j] - contour.m_aX[i];
			float dz = contour.m_aZ[j] - contour.m_aZ[i];

			if (Math.Sqrt(dx * dx + dz * dz) <= 150)
				continue;

			SetResultFailure("Border segment %1 jumped %2 m, further than one square's diagonal; the loop is joining nodes that are not neighbours", i.ToString(), Math.Sqrt(dx * dx + dz * dz).ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 10 - TOPOLOGY: a region with a HOLE produces two loops wound in opposite directions, and two
//! DISJOINT regions produce two loops wound the same way.
//!
//! WHY THIS IS SEPARATE FROM THE TRACE CASE. A tracer that only ever sees one simply-connected blob
//! can be wrong in two ways that never show up: it can join two islands into one loop, and it can
//! miss the boundary of a lake entirely and fill it in. Both look like plausible territory. The
//! signed area tells them apart without inspecting a vertex - an outer boundary is positive, the
//! boundary of a hole is negative, because the region is on the left of travel in both cases and
//! that means opposite senses.
//!
//! THIS IS ALSO WHAT THE OLD REPRESENTATION COULD NOT DO AT ALL. A radius per angle about a site
//! cannot express a hole or a second component, which is exactly why land across a bay could never be
//! filled.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ContourTopology : SCR_AutotestCaseBase
{
	//! A single square hole, traced between centres, is a diamond of diagonal 100 m.
	static const float EXPECTED_HOLE_AREA = 5000;

	//! The five-by-five block's outer boundary: a 500 m square with four 50 m corners cut off it.
	//!
	//! ASSERTED EXACTLY, and that is deliberate rather than fussy. The block fills its whole grid, so
	//! its left and bottom edges only exist because the tracer walks one square OUTSIDE the grid. Drop
	//! that overscan and what comes back is an open chain along the two remaining sides - which still
	//! counts as a loop, still encloses a positive area, and still leaves the hole looking perfect. Only
	//! the number catches it.
	static const float EXPECTED_OUTER_AREA = 245000;

	//! Five boundary nodes per side of the block, and none shared, because the corners are cut.
	static const int EXPECTED_OUTER_POINTS = 20;

	//! A two-by-two block: a 200 m square with four 50 m corners cut off it.
	static const float EXPECTED_BLOCK_AREA = 35000;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// A five-by-five region with the middle square missing.
		array<int> holed = new array<int>();

		for (int r = 0; r < 5; r++)
		{
			for (int c = 0; c < 5; c++)
			{
				if (c == 2 && r == 2)
					holed.Insert(-1);
				else
					holed.Insert(0);
			}
		}

		array<bool> inRegion = new array<bool>();
		inRegion.Insert(true);

		array<bool> rivalFaction = new array<bool>();
		rivalFaction.Insert(false);

		array<ref OVT_TerritoryContour> contours = new array<ref OVT_TerritoryContour>();
		OVT_TerritorySolver.TraceRegion(OVT_TEST_Logic_TerritoryFixture.MakeGrid(5, 5, 100, holed), inRegion, rivalFaction, contours);

		if (contours.Count() != 2)
		{
			SetResultFailure("A region with one hole in it produced %1 border loops instead of 2; a tracer that misses an inner boundary fills the lake in", contours.Count().ToString());
			return true;
		}

		float areaA = OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[0]);
		float areaB = OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[1]);

		float outer = areaA;
		float inner = areaB;
		int outerPoints = contours[0].m_aX.Count();

		if (Math.AbsFloat(areaB) > Math.AbsFloat(areaA))
		{
			outer = areaB;
			inner = areaA;
			outerPoints = contours[1].m_aX.Count();
		}

		// THE OUTER BOUNDARY EXISTS ONLY BECAUSE THE TRACER WALKS OUTSIDE THE GRID. This block fills its
		// whole grid, so its left and bottom edges are found one square past the edge of the world; take
		// that away and an open chain along the other two sides comes back looking entirely plausible.
		if (outerPoints != EXPECTED_OUTER_POINTS)
		{
			SetResultFailure("The outer boundary of a region filling its whole grid came out with %1 points instead of %2; the tracer has to read one square OUTSIDE the grid or a region touching the edge of the world never closes", outerPoints.ToString(), EXPECTED_OUTER_POINTS.ToString());
			return true;
		}

		if (Math.AbsFloat(outer - EXPECTED_OUTER_AREA) > 200)
		{
			SetResultFailure("The outer boundary enclosed %1 square metres instead of %2; a partial loop still encloses an area and still leaves the hole looking perfect, so this number is what catches it", outer.ToString(), EXPECTED_OUTER_AREA.ToString());
			return true;
		}

		if (outer <= 0)
		{
			SetResultFailure("The OUTER boundary of a holed region wound the wrong way, signed area %1", outer.ToString());
			return true;
		}

		if (inner >= 0)
		{
			SetResultFailure("The boundary of a HOLE wound the same way as the outer boundary, signed area %1; a hole and an island have to be distinguishable or the fill and the border disagree about which is which", inner.ToString());
			return true;
		}

		if (Math.AbsFloat(Math.AbsFloat(inner) - EXPECTED_HOLE_AREA) > 100)
		{
			SetResultFailure("The hole's boundary enclosed %1 square metres instead of %2", Math.AbsFloat(inner).ToString(), EXPECTED_HOLE_AREA.ToString());
			return true;
		}

		// TWO DISJOINT BLOCKS: two loops, both outer, both positive.
		array<int> split = new array<int>();

		for (int sr = 0; sr < 7; sr++)
		{
			for (int sc = 0; sc < 7; sc++)
			{
				bool first = sc <= 1 && sr <= 1;
				bool second = sc >= 5 && sr >= 5;

				if (first || second)
					split.Insert(0);
				else
					split.Insert(-1);
			}
		}

		contours.Clear();
		OVT_TerritorySolver.TraceRegion(OVT_TEST_Logic_TerritoryFixture.MakeGrid(7, 7, 100, split), inRegion, rivalFaction, contours);

		if (contours.Count() != 2)
		{
			SetResultFailure("Two disjoint blocks of one faction produced %1 border loops instead of 2; joining them would draw a border across open sea", contours.Count().ToString());
			return true;
		}

		if (OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[0]) <= 0 || OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[1]) <= 0)
		{
			SetResultFailure("One of two disjoint regions was traced as though it were a hole; both are outer boundaries and both must wind the same way");
			return true;
		}

		// AND BOTH ARE WHOLE. One of these blocks sits in the corner of the grid and is therefore the
		// same overscan question again; the other is interior and is the control.
		if (Math.AbsFloat(OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[0]) - EXPECTED_BLOCK_AREA) > 200 || Math.AbsFloat(OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[1]) - EXPECTED_BLOCK_AREA) > 200)
		{
			SetResultFailure("Two identical blocks traced to %1 and %2 square metres instead of %3 each; the one in the corner of the grid needs the tracer to look outside it", OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[0]).ToString(), OVT_TEST_Logic_TerritoryFixture.SignedArea(contours[1]).ToString(), EXPECTED_BLOCK_AREA.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 11 - CHAIKIN CORNER CUTTING moves the border onto the quarter points and carries the
//! frontier classification with it.
//!
//! THIS IS THE SMOOTHING THE REQUIREMENT ASKED FOR - "borders read as organic frontiers rather than
//! straight lines" - and it replaced a moving average over radii that could only ever pull a boundary
//! back toward its own site, and therefore had to be clamped, exempted for same-faction seams and
//! pinned at coastlines to stop it eating them. Corner cutting has none of those problems because it
//! moves points ALONG the curve; every new point is a convex combination of two old ones, so nothing
//! can be pushed anywhere the border was not already.
//!
//! THE CORNER RULE IS THE HALF THAT IS EASY TO GET WRONG. A new segment lying inside one old segment
//! inherits its class; a new segment spanning an old CORNER is a frontier only when BOTH old segments
//! were. Take either one alone instead and a band turns the corner from a frontier onto a coastline,
//! which is the exact defect the frontier classification exists to prevent.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ChaikinSmoothing : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	//! A 400 m square wound counter-clockwise, with alternating frontier flags.
	//! \return The contour.
	protected OVT_TerritoryContour MakeSquare()
	{
		OVT_TerritoryContour contour = new OVT_TerritoryContour();
		contour.m_aX = new array<float>();
		contour.m_aZ = new array<float>();
		contour.m_aFrontier = new array<bool>();

		contour.m_aX.Insert(0);   contour.m_aZ.Insert(0);
		contour.m_aX.Insert(400); contour.m_aZ.Insert(0);
		contour.m_aX.Insert(400); contour.m_aZ.Insert(400);
		contour.m_aX.Insert(0);   contour.m_aZ.Insert(400);

		contour.m_aFrontier.Insert(true);
		contour.m_aFrontier.Insert(false);
		contour.m_aFrontier.Insert(true);
		contour.m_aFrontier.Insert(false);

		return contour;
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TerritoryContour contour = MakeSquare();

		OVT_TerritorySolver.ChaikinSmooth(contour, 1);

		if (contour.m_aX.Count() != 8)
		{
			SetResultFailure("One smoothing pass over a four-point border produced %1 points instead of 8; each pass replaces every point with two and that doubling is also what it costs to draw", contour.m_aX.Count().ToString());
			return true;
		}

		// The first two points of the pass are a quarter and three quarters of the way along the first
		// old segment. Midpoints instead would mean the weights were 0.5/0.5 and the border would lose
		// its corners entirely rather than round them.
		if (!OVT_TEST_Logic_TerritoryFixture.Near(contour.m_aX[0], 100) || !OVT_TEST_Logic_TerritoryFixture.Near(contour.m_aX[1], 300))
		{
			SetResultFailure("Corner cutting put the first two points at %1 and %2 instead of 100 and 300; the quarter weights are what round a corner rather than collapse it", contour.m_aX[0].ToString(), contour.m_aX[1].ToString());
			return true;
		}

		// Every new point is a convex combination of two old ones, so nothing may escape the original
		// outline. This is the property that lets the clamp the old smoothing needed be deleted.
		for (int i = 0; i < contour.m_aX.Count(); i++)
		{
			if (contour.m_aX[i] >= -1 && contour.m_aX[i] <= 401 && contour.m_aZ[i] >= -1 && contour.m_aZ[i] <= 401)
				continue;

			SetResultFailure("Smoothing moved a border point to %1, %2, outside the outline it started from; corner cutting may only ever cut inward", contour.m_aX[i].ToString(), contour.m_aZ[i].ToString());
			return true;
		}

		// THE CLASSIFICATION. Old segments were frontier, not, frontier, not - so the eight new segments
		// are: inside 0 (yes), corner 0-1 (no), inside 1 (no), corner 1-2 (no), inside 2 (yes),
		// corner 2-3 (no), inside 3 (no), corner 3-0 (no).
		if (!contour.m_aFrontier[0])
		{
			SetResultFailure("A new segment lying INSIDE an old frontier segment lost its classification; the band would break up along a border that has not changed");
			return true;
		}

		if (contour.m_aFrontier[1])
		{
			SetResultFailure("A new segment spanning the CORNER between a frontier and a non-frontier was classified as a frontier; a band would turn the corner from a real border onto a coastline");
			return true;
		}

		if (!contour.m_aFrontier[4])
		{
			SetResultFailure("The second frontier segment lost its classification after smoothing");
			return true;
		}

		if (contour.m_aFrontier[2] || contour.m_aFrontier[3] || contour.m_aFrontier[5] || contour.m_aFrontier[6] || contour.m_aFrontier[7])
		{
			SetResultFailure("Smoothing spread the frontier classification onto segments whose old segments were not frontiers; the neutral band would grow with every pass");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 12 - ZERO SMOOTHING PASSES IS THE IDENTITY, and one pass is not.
//!
//! "Smoothing is tunable to off" is a contract: with m_iSmoothPasses at 0 the overlay draws the raw
//! marching-squares staircase, and that is a legitimate configuration rather than a broken one.
//!
//! THE PAIRED ASSERTION IS WHY THIS CASE IS NOT VACUOUS. A file that only asserted "0 passes changes
//! nothing" would stay green if smoothing were deleted outright, which is the more likely accident.
//! Asserting that ONE pass does change something is what makes the zero case mean anything.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ChaikinPassesZero : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	//! A triangle, which is the smallest thing that can be a closed contour.
	//! \return The contour.
	protected OVT_TerritoryContour MakeTriangle()
	{
		OVT_TerritoryContour contour = new OVT_TerritoryContour();
		contour.m_aX = new array<float>();
		contour.m_aZ = new array<float>();
		contour.m_aFrontier = new array<bool>();

		contour.m_aX.Insert(0);   contour.m_aZ.Insert(0);
		contour.m_aX.Insert(300); contour.m_aZ.Insert(0);
		contour.m_aX.Insert(0);   contour.m_aZ.Insert(300);

		contour.m_aFrontier.Insert(true);
		contour.m_aFrontier.Insert(true);
		contour.m_aFrontier.Insert(true);

		return contour;
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TerritoryContour contour = MakeTriangle();

		OVT_TerritorySolver.ChaikinSmooth(contour, 0);

		if (contour.m_aX.Count() != 3)
		{
			SetResultFailure("Zero smoothing passes changed a three-point border into %1 points; the raw staircase is a legitimate configuration and must be reachable", contour.m_aX.Count().ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(contour.m_aX[1], 300) || !OVT_TEST_Logic_TerritoryFixture.Near(contour.m_aZ[2], 300))
		{
			SetResultFailure("Zero smoothing passes moved a border point to %1, %2; nothing may be smoothed when nothing was asked for", contour.m_aX[1].ToString(), contour.m_aZ[2].ToString());
			return true;
		}

		// A negative pass count is the same answer, because a knob somebody types into a config can be
		// mistyped and a negative loop count must not wrap.
		OVT_TerritorySolver.ChaikinSmooth(contour, -1);

		if (contour.m_aX.Count() != 3)
		{
			SetResultFailure("A NEGATIVE smoothing pass count produced %1 points; it has to mean the same as zero", contour.m_aX.Count().ToString());
			return true;
		}

		// AND ONE PASS MUST DO SOMETHING, or the assertions above are satisfied by smoothing that never
		// runs at all.
		OVT_TerritorySolver.ChaikinSmooth(contour, 1);

		if (contour.m_aX.Count() != 6)
		{
			SetResultFailure("One smoothing pass over a three-point border produced %1 points instead of 6; smoothing that never runs would satisfy every other assertion in this case", contour.m_aX.Count().ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 13 - THE FACTION RULE. Ground belonging to a DIFFERENT faction on the far side of a border is
//! a frontier and earns the neutral band; WATER is not, and the same faction is not.
//!
//! This is the rule the first two renders got wrong, and it is worth restating why: "somebody else
//! owns the ground over there" says nothing about FACTION. Two towns of one faction, or a base and
//! its own radio tower, are one territory and a neutral strip down the middle of them says the
//! opposite. Under the grid they do not even produce a border - the region traced is a whole
//! faction's ground - but the predicate still has to answer correctly, because it is what tells the
//! tracer which of the ground OUTSIDE a region is a frontier and which is a shore.
//!
//! THE WATER ANSWER IS THE ONE THAT CHANGED SHAPE. It used to be a stop reason on a ray; it is now
//! simply an owner index of -1, because nobody owns the sea. A coastline is nobody's border.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_FrontierSide : SCR_AutotestCaseBase
{
	static const int OWN_FACTION = 0;
	static const int OTHER_FACTION = 1;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(0, 0, 1, OWN_FACTION));
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(400, 0, 1, OWN_FACTION));
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(-400, 0, 1, OTHER_FACTION));

		if (OVT_TerritorySolver.IsFrontierSide(1, OWN_FACTION, sites))
		{
			SetResultFailure("Ground held by a SAME-FACTION neighbour was classified as a frontier; a neutral band would be drawn between two regions of one faction, which is exactly the defect this case exists for");
			return true;
		}

		if (!OVT_TerritorySolver.IsFrontierSide(2, OWN_FACTION, sites))
		{
			SetResultFailure("Ground held by a DIFFERENT faction was NOT classified as a frontier; real borders would lose their band and the overlay would stop showing where the war is");
			return true;
		}

		// WATER. Nobody owns the sea, so a shoreline is nobody's frontier.
		if (OVT_TerritorySolver.IsFrontierSide(-1, OWN_FACTION, sites))
		{
			SetResultFailure("WATER on the far side of a border was classified as a frontier; every coastline on the map would be banded as though an enemy were standing in the sea");
			return true;
		}

		// An index nothing answers to is not a frontier either. An omitted edge is a smaller lie than
		// an invented border.
		if (OVT_TerritorySolver.IsFrontierSide(99, OWN_FACTION, sites))
		{
			SetResultFailure("An out-of-range owner index was classified as a frontier; unnameable ground must produce no band rather than an invented one");
			return true;
		}

		// And the same border is a frontier from the OTHER faction's side, so a band is not a property
		// of whichever region happens to be traced first.
		if (!OVT_TerritorySolver.IsFrontierSide(0, OTHER_FACTION, sites))
		{
			SetResultFailure("The same border was not a frontier from the other faction's side; a border would be banded on one side only");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 14 - END TO END: a traced border that is part coastline and part frontier is banded only
//! along the frontier.
//!
//! WHY THE PREDICATE CASE ABOVE IS NOT ENOUGH. The rule can be perfectly right and still be wired to
//! the wrong side of the border, or applied to the wrong end of a segment, and the result is a band
//! along a beach - which is precisely the class of defect this feature has shipped twice. So this
//! case builds a field where a region touches open water on three sides and another faction on the
//! fourth, traces it for real, and counts.
//!
//! THE SEGMENT RULE IS "BOTH ENDS", and the field is chosen to make that visible: the frontier is two
//! squares tall, so exactly one segment has a frontier node at each end. Take EITHER end instead and
//! the count comes out three, because the two segments turning the corner onto the coast would be
//! banded too.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ContourFrontierSides : SCR_AutotestCaseBase
{
	static const int OCCUPIER_FACTION = 3;
	static const int RESISTANCE_FACTION = 5;

	//------------------------------------------------------------------------------------------------
	//! A six-by-four field: water all round, an occupier block at columns 1-2 and a resistance block
	//! at columns 3-4, both spanning rows 1-2.
	//! \return Owner index per square, row-major.
	protected array<int> MakeFrontField()
	{
		array<int> owners = new array<int>();

		// row 0 - all water
		for (int a = 0; a < 6; a++)
		{
			owners.Insert(-1);
		}

		// rows 1 and 2 - occupier, occupier, resistance, resistance, between two water squares
		for (int band = 0; band < 2; band++)
		{
			owners.Insert(-1);
			owners.Insert(0);
			owners.Insert(0);
			owners.Insert(1);
			owners.Insert(1);
			owners.Insert(-1);
		}

		// row 3 - all water
		for (int b = 0; b < 6; b++)
		{
			owners.Insert(-1);
		}

		return owners;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts the frontier-flagged segments of every contour in a list.
	//! \param[in] contours The traced borders.
	//! \return How many segments earned a band.
	protected int CountFrontierSegments(array<ref OVT_TerritoryContour> contours)
	{
		int total = 0;

		foreach (OVT_TerritoryContour contour : contours)
		{
			foreach (bool frontier : contour.m_aFrontier)
			{
				if (frontier)
					total++;
			}
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TerritoryGrid grid = OVT_TEST_Logic_TerritoryFixture.MakeGrid(6, 4, 100, MakeFrontField());

		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(150, 150, 1, OCCUPIER_FACTION));
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(350, 150, 1, RESISTANCE_FACTION));

		// The occupier's region: its own site inside, the resistance site outside and a rival.
		array<bool> inRegion = new array<bool>();
		inRegion.Insert(true);
		inRegion.Insert(false);

		array<bool> rivalFaction = new array<bool>();
		rivalFaction.Insert(OVT_TerritorySolver.IsFrontierSide(0, OCCUPIER_FACTION, sites));
		rivalFaction.Insert(OVT_TerritorySolver.IsFrontierSide(1, OCCUPIER_FACTION, sites));

		array<ref OVT_TerritoryContour> contours = new array<ref OVT_TerritoryContour>();
		OVT_TerritorySolver.TraceRegion(grid, inRegion, rivalFaction, contours);

		if (contours.Count() != 1)
		{
			SetResultFailure("The occupier's two-by-two block produced %1 border loops instead of 1", contours.Count().ToString());
			return true;
		}

		if (contours[0].m_aFrontier.Count() != 8)
		{
			SetResultFailure("The border came out with %1 segments instead of 8", contours[0].m_aFrontier.Count().ToString());
			return true;
		}

		int banded = CountFrontierSegments(contours);

		if (banded != 1)
		{
			SetResultFailure("%1 of 8 border segments were banded instead of 1; three sides of this region are open sea and a coastline is nobody's frontier", banded.ToString());
			return true;
		}

		// AND IT IS THE RIGHT SEGMENT. The shared boundary runs up the line x = 300, between the two
		// blocks; a band anywhere else is a band on a beach.
		int count = contours[0].m_aX.Count();

		for (int i = 0; i < count; i++)
		{
			if (!contours[0].m_aFrontier[i])
				continue;

			int j = i + 1;
			if (j >= count)
				j = 0;

			if (OVT_TEST_Logic_TerritoryFixture.Near(contours[0].m_aX[i], 300) && OVT_TEST_Logic_TerritoryFixture.Near(contours[0].m_aX[j], 300))
				break;

			SetResultFailure("The banded segment ran from x=%1 to x=%2, not along the shared boundary at x=300; the band is on a coastline", contours[0].m_aX[i].ToString(), contours[0].m_aX[j].ToString());
			return true;
		}

		// THE MIRROR. The same border is banded from the resistance region's side too, so which faction
		// is traced first cannot decide whether a frontier is marked.
		array<bool> mirrorRegion = new array<bool>();
		mirrorRegion.Insert(false);
		mirrorRegion.Insert(true);

		array<bool> mirrorRival = new array<bool>();
		mirrorRival.Insert(OVT_TerritorySolver.IsFrontierSide(0, RESISTANCE_FACTION, sites));
		mirrorRival.Insert(OVT_TerritorySolver.IsFrontierSide(1, RESISTANCE_FACTION, sites));

		contours.Clear();
		OVT_TerritorySolver.TraceRegion(grid, mirrorRegion, mirrorRival, contours);

		if (CountFrontierSegments(contours) != 1)
		{
			SetResultFailure("The same frontier was banded %1 times from the OTHER faction's side instead of once; a border must read the same whichever region is traced", CountFrontierSegments(contours).ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 15 - FRONTIER SPANS on a closed ring, including a run that CROSSES THE START OF THE ARRAY.
//!
//! A band is drawn as a continuous strip along each run of frontier segments, and a run that wraps
//! past index zero is ONE run, not two. Split it and the strip is drawn as two pieces with a mitre
//! missing where they meet - a visible notch in the border, always in the same place, which reads as
//! an art fault rather than as an index bug.
//!
//! The wrap is handled with no special case at all: the scan starts immediately AFTER a segment that
//! is not a frontier, so no run can straddle the start of the walk. The one configuration with no
//! such starting point - every segment is a frontier - is the whole ring and is answered up front.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_FrontierSpans : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	//! A bool array from a bit pattern, low bit first.
	//! \param[in] count How many entries.
	//! \param[in] bits Bit i set means entry i is a frontier.
	//! \return The array.
	protected array<bool> MakeFlags(int count, int bits)
	{
		array<bool> flags = new array<bool>();

		for (int i = 0; i < count; i++)
		{
			flags.Insert((bits & (1 << i)) != 0);
		}

		return flags;
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<int> spanStart = new array<int>();
		array<int> spanLength = new array<int>();

		// F T T F T - two separate runs.
		OVT_TerritorySolver.CollectFrontierSpans(MakeFlags(5, 2 + 4 + 16), spanStart, spanLength);

		if (spanStart.Count() != 2)
		{
			SetResultFailure("Two separated runs of frontier came out as %1 spans instead of 2", spanStart.Count().ToString());
			return true;
		}

		// T T F T - one run of three, crossing the start of the array. The single non-frontier segment
		// sits at index 2 rather than index 1 on purpose: a scan that anchored on the first FRONTIER
		// segment instead of the first non-frontier one produces the right answer on half the possible
		// rings by luck, and this is one of the halves where it does not.
		OVT_TerritorySolver.CollectFrontierSpans(MakeFlags(4, 1 + 2 + 8), spanStart, spanLength);

		if (spanStart.Count() != 1)
		{
			SetResultFailure("A frontier run that WRAPS past index zero came out as %1 spans instead of 1; the band would be drawn as two pieces with a notch where they meet", spanStart.Count().ToString());
			return true;
		}

		if (spanLength[0] != 3)
		{
			SetResultFailure("The wrapping run was %1 segments long instead of 3", spanLength[0].ToString());
			return true;
		}

		if (spanStart[0] != 3)
		{
			SetResultFailure("The wrapping run started at segment %1 instead of 3; it has to start where the run does, not where the array does", spanStart[0].ToString());
			return true;
		}

		// EVERY segment a frontier - the whole ring, and the one configuration with no boundary to
		// anchor the scan to.
		OVT_TerritorySolver.CollectFrontierSpans(MakeFlags(3, 1 + 2 + 4), spanStart, spanLength);

		if (spanStart.Count() != 1 || spanLength[0] != 3)
		{
			SetResultFailure("A border that is a frontier all the way round came out as %1 spans; it is one closed strip", spanStart.Count().ToString());
			return true;
		}

		// And no frontier at all is no band, not an empty one.
		OVT_TerritorySolver.CollectFrontierSpans(MakeFlags(4, 0), spanStart, spanLength);

		if (!spanStart.IsEmpty())
		{
			SetResultFailure("A border with no frontier on it produced %1 spans; a coastline gets no band at all", spanStart.Count().ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 16 - THE OCCUPIER-ONLY EMIT FILTER, and the trap in it: a site that is NOT DRAWN must still
//! OWN GROUND.
//!
//! WHAT THE OVERLAY ANSWERS. Not "who holds what" but "how much does the occupier still hold".
//! Liberated ground reads as clean map, so on an island that starts almost entirely occupied the
//! fill is a progress bar for the whole campaign and clearing a region shows as the region
//! DISAPPEARING rather than changing colour.
//!
//! THE FAILURE THIS CASE EXISTS FOR is the obvious way to implement that and it is silent. Filter the
//! resistance sites out when the sites are COLLECTED and everything still looks reasonable - one
//! faction's regions, no errors, a healthy site count in the log - except that occupier territory now
//! flows straight over the ground the player has taken, because the sites that were pushing that
//! boundary back are gone. The map tells them they have achieved nothing.
//!
//! So the filter belongs on the EMIT path, and this case pins both halves of that: the classification
//! itself, and the fact that the grid is completely unaware of it. The resistance site owns its half
//! of the world and the fill simply stops at the boundary rather than crossing it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_OccupierOnlyEmit : SCR_AutotestCaseBase
{
	//! Deliberately neither zero nor adjacent, so an index confusion cannot pass by coincidence.
	static const int OCCUPIER_FACTION = 3;
	static const int RESISTANCE_FACTION = 5;

	//! The two sites sit this far apart, so the boundary falls at half of it - between the columns
	//! whose centres are 450 m and 550 m from the occupier.
	static const int LAST_OCCUPIER_COLUMN = 9;
	static const int FIRST_RESISTANCE_COLUMN = 10;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(0, 0, 1, OCCUPIER_FACTION));
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeFactionSite(1000, 0, 1, RESISTANCE_FACTION));

		OVT_TerritorySolver solver = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		solver.SetGridExtent(-500, -100, 1500, 100);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		// THE ASSERTION THAT MATTERS. The liberated site owns its own ground, whether or not anybody
		// intends to draw it.
		if (grid.OwnerAt(FIRST_RESISTANCE_COLUMN, 0) != 1)
		{
			SetResultFailure("The square past the midpoint was owned by site %1 rather than the LIBERATED site; a site that is not drawn must still compete, or occupier colour floods the ground the player has liberated", grid.OwnerAt(FIRST_RESISTANCE_COLUMN, 0).ToString());
			return true;
		}

		if (grid.OwnerAt(LAST_OCCUPIER_COLUMN, 0) != 0)
		{
			SetResultFailure("The square before the midpoint was owned by site %1 rather than the occupier; the boundary has moved", grid.OwnerAt(LAST_OCCUPIER_COLUMN, 0).ToString());
			return true;
		}

		// AND THE FILL STOPS THERE. The occupier is drawn, the resistance is not, and the merged
		// rectangles must reach the boundary and no further.
		array<int> appearance = new array<int>();
		appearance.Insert(0);
		appearance.Insert(-1);

		array<ref OVT_TerritoryRect> rects = new array<ref OVT_TerritoryRect>();
		OVT_TerritorySolver.BuildRects(grid, appearance, true, rects);

		if (rects.IsEmpty())
		{
			SetResultFailure("With one of two factions drawn the fill was empty; the occupier's own ground has to be drawn or the overlay says the campaign is over");
			return true;
		}

		foreach (OVT_TerritoryRect rect : rects)
		{
			if (rect.m_iCol1 <= LAST_OCCUPIER_COLUMN)
				continue;

			SetResultFailure("A drawn rectangle reached column %1, past the boundary at column %2; occupier colour is covering liberated ground", rect.m_iCol1.ToString(), LAST_OCCUPIER_COLUMN.ToString());
			return true;
		}

		bool reachedBoundary = false;

		foreach (OVT_TerritoryRect edge : rects)
		{
			if (edge.m_iCol1 == LAST_OCCUPIER_COLUMN)
				reachedBoundary = true;
		}

		if (!reachedBoundary)
		{
			SetResultFailure("No drawn rectangle reached the boundary column; the occupier's fill is stopping short of ground it holds");
			return true;
		}

		// THE CLASSIFICATION. Occupier drawn, everybody else not.
		if (!OVT_TerritorySolver.IsEmittedCell(OCCUPIER_FACTION, OCCUPIER_FACTION, true))
		{
			SetResultFailure("An OCCUPIER-held region was not drawn; with the filter on, occupier ground is the only thing the overlay shows and the map would be blank");
			return true;
		}

		if (OVT_TerritorySolver.IsEmittedCell(RESISTANCE_FACTION, OCCUPIER_FACTION, true))
		{
			SetResultFailure("A LIBERATED region was drawn anyway; the overlay is supposed to answer how much the occupier still holds, and liberated ground reads as clean map");
			return true;
		}

		// FLAG OFF: every faction is drawn again, which is the pre-existing behaviour and one config
		// value away.
		if (!OVT_TerritorySolver.IsEmittedCell(RESISTANCE_FACTION, OCCUPIER_FACTION, false))
		{
			SetResultFailure("With the occupier-only filter switched OFF, a liberated region was still hidden; the flag is the only thing that may suppress a region");
			return true;
		}

		// UNKNOWN OCCUPIER degrades to drawing everything, never to drawing nothing. An empty overlay is
		// this feature's least visible failure.
		if (!OVT_TerritorySolver.IsEmittedCell(RESISTANCE_FACTION, -1, true))
		{
			SetResultFailure("With the occupying faction unresolvable, the overlay drew nothing; an unknown occupier must degrade to drawing every faction, not to an empty map");
			return true;
		}

		// THE BAND FOLLOWS THE FILL. The occupier's boundary with liberated ground is a real frontier.
		if (!OVT_TerritorySolver.IsFrontierSide(1, OCCUPIER_FACTION, sites))
		{
			SetResultFailure("The edge of occupier control against liberated ground was not classified as a frontier; the one boundary the player is watching would lose its band");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 17 - CONTESTED IS OCCUPIER-HELD AND SUPPORTED, and it is a THRESHOLD rather than a shade.
//!
//! The rule reads: a region the occupier still controls, whose population has gone over to the
//! resistance, is drawn as contested. It is NOT recoloured - the occupier does control it - so this is
//! a second axis over an accurate first one rather than a second opinion about who holds what. That
//! distinction is the whole reason support is allowed to drive this when it was rejected as a driver
//! of the fill colour itself.
//!
//! TWO STATES, NEVER FORTY. A continuous per-site shade is what turned a one-faction island into
//! mottled noise: when hue carries no information the shade is the only variable left and every
//! neighbouring region differs slightly. A threshold cannot do that, and this case pins the boundary
//! of the threshold rather than any value on either side of it.
//!
//! AND IT IS TOWNS ONLY. Bases, radio towers and FOBs have no popular support and carry the
//! SUPPORT_NONE sentinel, which is rejected EXPLICITLY rather than left to the arithmetic. Be honest
//! about what that buys: at any sane threshold the comparison already rejects it, so the guard earns
//! its place by making the sentinel a STATE rather than a value - no configured threshold, including a
//! mistyped negative one, can turn "no support to measure" into "fully supported". The threshold is a
//! float somebody types into a config, so that is a reachable mistake rather than a theoretical one,
//! and the paired zero-threshold assertion is what stops the guard swallowing real data alongside it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ContestedSupport : SCR_AutotestCaseBase
{
	static const int OCCUPIER_FACTION = 3;
	static const int RESISTANCE_FACTION = 5;

	//! Half the town, which is the shipped threshold.
	static const float THRESHOLD = 0.5;

	//! A threshold nobody would configure on purpose, below the SUPPORT_NONE sentinel itself. It is
	//! what makes the sentinel's rejection observable: at any sane threshold the plain comparison
	//! already rejects it, so only this says the sentinel is a state rather than data.
	static const float NONSENSE_THRESHOLD = -2;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Occupier-held and the town has gone over: contested.
		if (!OVT_TerritorySolver.IsContestedCell(OCCUPIER_FACTION, OCCUPIER_FACTION, 0.6, THRESHOLD))
		{
			SetResultFailure("An occupier-held town with 60%% support was not contested; the mark exists to show where the occupier's grip is going");
			return true;
		}

		// Occupier-held and the town is still with them: not contested.
		if (OVT_TerritorySolver.IsContestedCell(OCCUPIER_FACTION, OCCUPIER_FACTION, 0.4, THRESHOLD))
		{
			SetResultFailure("An occupier-held town with 40%% support was marked contested; below the threshold the region is simply held");
			return true;
		}

		// EXACTLY ON THE THRESHOLD is contested. The comparison is inclusive, so half a town counts.
		if (!OVT_TerritorySolver.IsContestedCell(OCCUPIER_FACTION, OCCUPIER_FACTION, THRESHOLD, THRESHOLD))
		{
			SetResultFailure("A town sitting EXACTLY on the support threshold was not contested; the threshold is inclusive and this is the only assertion that says which side it falls on");
			return true;
		}

		// A LIBERATED town is not contested however much support it has - it is not even drawn.
		if (OVT_TerritorySolver.IsContestedCell(RESISTANCE_FACTION, OCCUPIER_FACTION, 0.9, THRESHOLD))
		{
			SetResultFailure("A LIBERATED town was marked contested; contested is a statement about ground the OCCUPIER still holds, and a liberated region draws nothing at all");
			return true;
		}

		// THE SENTINEL, at the only kind of threshold that makes the guard load-bearing: one below the
		// sentinel itself. A base has no support figure at all, and no configured threshold may turn
		// that into "fully supported".
		if (OVT_TerritorySolver.IsContestedCell(OCCUPIER_FACTION, OCCUPIER_FACTION, OVT_TerritorySite.SUPPORT_NONE, NONSENSE_THRESHOLD))
		{
			SetResultFailure("A military site with NO support figure was marked contested; SUPPORT_NONE is a state, not a value, and comparing it as data lights up every base on the map the moment the threshold is mistyped");
			return true;
		}

		if (!OVT_TerritorySolver.IsContestedCell(OCCUPIER_FACTION, OCCUPIER_FACTION, 0, 0))
		{
			SetResultFailure("A town with zero support was not contested at a zero threshold; the sentinel rejection must not have swallowed real data alongside it");
			return true;
		}

		// An unknown occupier marks nothing, and must not match a cell whose faction is also unresolved.
		if (OVT_TerritorySolver.IsContestedCell(-1, -1, 1.0, THRESHOLD))
		{
			SetResultFailure("With the occupying faction unresolvable, a region with unknown control was marked contested; two unknowns comparing equal is not a match");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 18 - EVEN-ODD SPAN PAIRING: a scan row meets the region's own contour, and consecutive pairs
//! of crossings are the inside of it.
//!
//! WHY THE FILL IS CUT FROM THE CONTOUR AT ALL. It used to be merged grid squares while the neutral
//! band was offset from the Chaikin-smoothed contour traced through those same squares - two
//! boundaries for one region, which did not coincide. The blocky fill jutted past the smooth band in
//! places and fell short of it in others, and no amount of tuning could make two independently
//! derived curves agree. Cutting the fill out of the contour is what makes them agree by
//! construction.
//!
//! HOLES AND DISJOINT REGIONS ARE THE SAME QUESTION, and this case asserts them side by side to say
//! so. Four crossings is two spans whether the gap between them is a pocket of liberated ground
//! inside occupier territory or a strait between two islands. A decomposition that needed to know
//! which would need topology, and this one deliberately has none - that is the property the whole
//! representation change bought and it must not quietly acquire an exception.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ScanlineSpans : SCR_AutotestCaseBase
{
	//! Row boundaries land on multiples of this from world zero, so the row asserted on below is
	//! exactly z 100 to 200.
	static const float ROW_HEIGHT = 100;

	//! The row every assertion here uses. It is deliberately in the MIDDLE of every shape: the first
	//! and last rows of a region legitimately subdivide, because that is where it begins and ends.
	static const float ASSERT_ROW = 100;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritoryContour> contours = new array<ref OVT_TerritoryContour>();
		array<ref OVT_TerritoryTrapezoid> traps = new array<ref OVT_TerritoryTrapezoid>();
		array<ref OVT_TerritoryTrapezoid> row = new array<ref OVT_TerritoryTrapezoid>();

		// A PLAIN CONVEX REGION. Two crossings, one span, and the trapezoid covers the whole of it.
		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(0, 0, 400, 400, 0));

		OVT_TerritorySolver.BuildTrapezoids(contours, 0, ROW_HEIGHT, traps);
		OVT_TEST_Logic_TerritoryFixture.TrapezoidsAtRow(traps, ASSERT_ROW, row);

		if (row.Count() != 1)
		{
			SetResultFailure("A scan row across a simple convex region produced %1 spans instead of 1; two crossings pair into exactly one inside", row.Count().ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(row[0].m_fTopLeftX, 0) || !OVT_TEST_Logic_TerritoryFixture.Near(row[0].m_fTopRightX, 400))
		{
			SetResultFailure("The span ran from %1 to %2 instead of 0 to 400; it is not taking its edges from where the contour actually crosses the row", row[0].m_fTopLeftX.ToString(), row[0].m_fTopRightX.ToString());
			return true;
		}

		// A REGION WITH A HOLE IN IT. Four crossings, two spans, and the hole is the gap between them.
		// This is a pocket of liberated ground inside occupier territory, which the representation
		// before the grid could not express at all.
		contours.Clear();
		traps.Clear();

		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(0, 0, 400, 400, 0));
		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(150, 100, 250, 300, 0));

		OVT_TerritorySolver.BuildTrapezoids(contours, 0, ROW_HEIGHT, traps);
		OVT_TEST_Logic_TerritoryFixture.TrapezoidsAtRow(traps, ASSERT_ROW, row);

		if (row.Count() != 2)
		{
			SetResultFailure("A scan row across a region with a HOLE produced %1 spans instead of 2; four crossings pair into an inside, a hole and an inside, and a decomposition that fills the middle one paints over ground the player has liberated", row.Count().ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(row[0].m_fTopRightX, 150) || !OVT_TEST_Logic_TerritoryFixture.Near(row[1].m_fTopLeftX, 250))
		{
			SetResultFailure("The two spans either side of the hole ended at %1 and began at %2 instead of 150 and 250; the hole is not where the contour puts it", row[0].m_fTopRightX.ToString(), row[1].m_fTopLeftX.ToString());
			return true;
		}

		if (row[0].m_fTopRightX > row[1].m_fTopLeftX)
		{
			SetResultFailure("The two spans of one row overlapped; even-odd pairing produces disjoint insides and two fills compositing over each other is the dark hairline this representation exists to make impossible");
			return true;
		}

		// TWO DISJOINT REGIONS. The SAME four crossings and the SAME two spans - the decomposition
		// cannot tell an island from a hole and does not need to.
		contours.Clear();
		traps.Clear();

		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(0, 0, 100, 400, 0));
		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(300, 0, 400, 400, 0));

		OVT_TerritorySolver.BuildTrapezoids(contours, 0, ROW_HEIGHT, traps);
		OVT_TEST_Logic_TerritoryFixture.TrapezoidsAtRow(traps, ASSERT_ROW, row);

		if (row.Count() != 2)
		{
			SetResultFailure("A scan row across TWO DISJOINT regions produced %1 spans instead of 2; joining them would fill the strait between two islands", row.Count().ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(row[0].m_fTopRightX, 100) || !OVT_TEST_Logic_TerritoryFixture.Near(row[1].m_fTopLeftX, 300))
		{
			SetResultFailure("The two disjoint spans ended at %1 and began at %2 instead of 100 and 300", row[0].m_fTopRightX.ToString(), row[1].m_fTopLeftX.ToString());
			return true;
		}

		// AND A CONTOUR BELONGING TO ANOTHER APPEARANCE IS NOT PART OF THIS REGION. Held ground and
		// contested ground are traced separately, so a decomposition that ignored the key would union
		// them and fill a contested town in its neighbour's solid colour.
		contours.Clear();
		traps.Clear();

		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(0, 0, 400, 400, 0));
		contours.Insert(OVT_TEST_Logic_TerritoryFixture.MakeRectContour(1000, 0, 1400, 400, 1));

		OVT_TerritorySolver.BuildTrapezoids(contours, 0, ROW_HEIGHT, traps);
		OVT_TEST_Logic_TerritoryFixture.TrapezoidsAtRow(traps, ASSERT_ROW, row);

		if (row.Count() != 1)
		{
			SetResultFailure("Cutting appearance 0 produced %1 spans instead of 1; a contour belonging to another appearance was treated as part of this region, so one wash would be drawn over ground that belongs to the other", row.Count().ToString());
			return true;
		}

		if (row[0].m_iKey != 0)
		{
			SetResultFailure("A trapezoid came out carrying appearance key %1 instead of 0; the fill and the border it was cut from have to agree about which table they index", row[0].m_iKey.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 19 - THE CASE THIS CHANGE EXISTS FOR: a slanted stretch of border is filled SLANTED, because
//! each trapezoid takes its left and right edges from where the contour crosses that row's own top
//! and bottom.
//!
//! WHY A FINER GRID WAS NOT THE ANSWER. The fill it replaced was cut on ownership square boundaries,
//! so its silhouette was an axis-aligned staircase while the band drawn over it followed a smooth
//! curve. Halving the square size would have made the steps smaller and cost four times the classify
//! - and the two boundaries would still have been different curves. Rectangles per scan row have
//! exactly the same flaw one axis down. Only edges taken from the contour remove it.
//!
//! THE AREA IS THE ASSERTION THAT CANNOT BE FAKED. A decomposition that took each row's width from
//! one edge and drew a rectangle would tile just as cleanly and have exactly as many quads; it would
//! simply be wrong by half the area under every slanted stretch of border. On the shape below that is
//! 55,000 square metres against the true 60,000, and nothing but the area says so.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_ScanlineTrapezoids : SCR_AutotestCaseBase
{
	static const float ROW_HEIGHT = 100;

	//! Shoelace area of the test shape. A stepped fill of the same shape comes out at 55,000.
	static const float EXPECTED_AREA = 60000;

	//! Generous next to the ~5 square metres the subdivision leaves at the far end of the shape, and
	//! twenty-five times tighter than the 5,000 a stepped fill would be out by.
	static const float AREA_TOLERANCE = 200;

	//------------------------------------------------------------------------------------------------
	//! A quadrilateral with a vertical left edge at x = 0 and a right edge slanting from x = 100 at
	//! z = 0 to x = 200 at z = 400, so the contour's X at any scan line is exactly 100 + z / 4.
	//! \return The contour.
	protected OVT_TerritoryContour MakeSlantedRegion()
	{
		array<float> xs = new array<float>();
		array<float> zs = new array<float>();

		xs.Insert(0);   zs.Insert(0);
		xs.Insert(100); zs.Insert(0);
		xs.Insert(200); zs.Insert(400);
		xs.Insert(0);   zs.Insert(400);

		return OVT_TEST_Logic_TerritoryFixture.MakeContour(xs, zs, 0);
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritoryContour> contours = new array<ref OVT_TerritoryContour>();
		contours.Insert(MakeSlantedRegion());

		array<ref OVT_TerritoryTrapezoid> traps = new array<ref OVT_TerritoryTrapezoid>();
		OVT_TerritorySolver.BuildTrapezoids(contours, 0, ROW_HEIGHT, traps);

		array<ref OVT_TerritoryTrapezoid> row = new array<ref OVT_TerritoryTrapezoid>();
		OVT_TEST_Logic_TerritoryFixture.TrapezoidsAtRow(traps, 100, row);

		if (row.Count() != 1)
		{
			SetResultFailure("The row from z=100 to z=200 produced %1 spans instead of 1", row.Count().ToString());
			return true;
		}

		OVT_TerritoryTrapezoid middle = row[0];

		// THE ASSERTION. The right edge is at 125 where the row starts and 150 where it ends, because
		// that is where the contour is - not 125 at both, which is a rectangle, and not 150 at both,
		// which is a wider rectangle.
		if (!OVT_TEST_Logic_TerritoryFixture.Near(middle.m_fTopRightX, 125))
		{
			SetResultFailure("The top of the row met the slanted border at x=%1 instead of 125; the trapezoid is not taking its corner from the contour", middle.m_fTopRightX.ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(middle.m_fBottomRightX, 150))
		{
			SetResultFailure("The bottom of the row met the slanted border at x=%1 instead of 150; the trapezoid is not taking its corner from the contour", middle.m_fBottomRightX.ToString());
			return true;
		}

		if (OVT_TEST_Logic_TerritoryFixture.Near(middle.m_fTopRightX, middle.m_fBottomRightX))
		{
			SetResultFailure("The row's two right corners came out at the same x, %1; a slanted border filled with a straight-sided quad is a rectangle, and a stack of rectangles is the staircase this replaced", middle.m_fTopRightX.ToString());
			return true;
		}

		// And the vertical edge stays vertical, so the slant is a property of the contour rather than
		// something the decomposition adds.
		if (!OVT_TEST_Logic_TerritoryFixture.Near(middle.m_fTopLeftX, 0) || !OVT_TEST_Logic_TerritoryFixture.Near(middle.m_fBottomLeftX, 0))
		{
			SetResultFailure("The straight left edge came out at %1 and %2 instead of 0 at both; only the contour may decide where an edge goes", middle.m_fTopLeftX.ToString(), middle.m_fBottomLeftX.ToString());
			return true;
		}

		// THE ROW SEAM. One row's bottom edge is the next row's top edge, and the two have to be the
		// same numbers or every scan row boundary on the map is a hairline of missing or doubled fill.
		array<ref OVT_TerritoryTrapezoid> above = new array<ref OVT_TerritoryTrapezoid>();
		OVT_TEST_Logic_TerritoryFixture.TrapezoidsAtRow(traps, 0, above);

		if (above.Count() != 1)
		{
			SetResultFailure("The row from z=0 to z=100 produced %1 spans instead of 1", above.Count().ToString());
			return true;
		}

		if (Math.AbsFloat(above[0].m_fBottomRightX - middle.m_fTopRightX) > 0.001 || Math.AbsFloat(above[0].m_fBottomLeftX - middle.m_fTopLeftX) > 0.001)
		{
			SetResultFailure("The bottom of one row sat at %1 while the top of the next sat at %2; a row seam has to be the SAME crossing carried forward, not a second computation of it", above[0].m_fBottomRightX.ToString(), middle.m_fTopRightX.ToString());
			return true;
		}

		// THE AREA. This is what tells a genuinely slanted fill from a stepped one that tiles just as
		// cleanly and has just as many quads.
		float area = OVT_TEST_Logic_TerritoryFixture.TrapezoidArea(traps);

		if (Math.AbsFloat(area - EXPECTED_AREA) > AREA_TOLERANCE)
		{
			SetResultFailure("The fill covered %1 square metres against the shape's own %2; a fill that stepped each row to one of its edges would come out at 55000, which is what this number exists to catch", area.ToString(), EXPECTED_AREA.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 20 - THE OWNERSHIP GRID'S ORIGIN IS A MULTIPLE OF ITS OWN SQUARE SIZE, measured from world
//! zero.
//!
//! WHY THAT MATTERS AT ALL. The base game draws its map grid from the world origin, so an overlay
//! whose squares are the same size as the map's grid squares still sits visibly BESIDE its lines
//! unless it starts on a multiple of that size too. The grid used to start at the world bound box
//! corner, which is not generally a multiple of anything, and the offset was reported from a
//! screenshot.
//!
//! IT HAS TO BE THE SIZE THE GRID WAS ACTUALLY BUILT AT, not the one configured. The square-count
//! ceiling can grow the size, and snapping to the configured value after that leaves the origin off
//! the grid it ended up on - which is the same defect with a different cause and no way to see it in
//! the first assertion below.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Territory_GridOriginSnap : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TerritorySite> sites = new array<ref OVT_TerritorySite>();
		sites.Insert(OVT_TEST_Logic_TerritoryFixture.MakeSite(0, 0, 1));

		// A bound box minimum that is not a multiple of anything, which is what a world bound box
		// actually looks like.
		OVT_TerritorySolver solver = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		solver.SetGridExtent(-1234, -567, 1000, 1000);

		OVT_TerritoryGrid grid = solver.BuildGrid(sites);

		if (!OVT_TEST_Logic_TerritoryFixture.IsMultipleOf(grid.m_fOriginX, grid.m_fCellSize) || !OVT_TEST_Logic_TerritoryFixture.IsMultipleOf(grid.m_fOriginZ, grid.m_fCellSize))
		{
			SetResultFailure("The grid started at %1, %2 with %3 m squares; neither is a whole multiple of the square size, so every square edge on the map sits beside the game's own grid lines instead of on them", grid.m_fOriginX.ToString(), grid.m_fOriginZ.ToString(), grid.m_fCellSize.ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.Near(grid.m_fOriginX, -1300) || !OVT_TEST_Logic_TerritoryFixture.Near(grid.m_fOriginZ, -600))
		{
			SetResultFailure("The grid started at %1, %2 instead of -1300, -600; the snap has to round DOWN, or the strip of world between the bound box and the first square is never classified", grid.m_fOriginX.ToString(), grid.m_fOriginZ.ToString());
			return true;
		}

		// AND IT STILL COVERS THE WHOLE BOX. Snapping down and then not growing the grid back out would
		// leave the far edge of the world unclassified, which reads as a coastline that is not there.
		if (grid.EdgeX(0) > -1234 || grid.EdgeZ(0) > -567)
		{
			SetResultFailure("The first square started at %1, %2, inside the requested extent; snapping may only ever move the origin outward", grid.EdgeX(0).ToString(), grid.EdgeZ(0).ToString());
			return true;
		}

		if (grid.EdgeX(grid.m_iCols) < 1000 || grid.EdgeZ(grid.m_iRows) < 1000)
		{
			SetResultFailure("The grid ended at %1, %2, short of the requested 1000, 1000; snapping the origin down must not cost the far edge of the world", grid.EdgeX(grid.m_iCols).ToString(), grid.EdgeZ(grid.m_iRows).ToString());
			return true;
		}

		// A POSITIVE minimum, because rounding down and truncating toward zero are the same answer for
		// negatives only if one of them is wrong, and they differ here.
		solver.SetGridExtent(150, 250, 1000, 1000);

		OVT_TerritoryGrid positive = solver.BuildGrid(sites);

		if (!OVT_TEST_Logic_TerritoryFixture.Near(positive.m_fOriginX, 100) || !OVT_TEST_Logic_TerritoryFixture.Near(positive.m_fOriginZ, 200))
		{
			SetResultFailure("A bound box starting at 150, 250 produced a grid origin of %1, %2 instead of 100, 200", positive.m_fOriginX.ToString(), positive.m_fOriginZ.ToString());
			return true;
		}

		// THE VALVE. A square size the ceiling has to grow, from a bound box minimum that is a multiple
		// of neither the configured size nor the grown one.
		OVT_TerritorySolver huge = OVT_TEST_Logic_TerritoryFixture.MakeSolver();
		huge.m_fGridCellSize = 1;
		huge.SetGridExtent(3.5, 3.5, 603.5, 603.5);

		OVT_TerritoryGrid coarse = huge.BuildGrid(sites);

		if (coarse.m_fCellSize <= 1)
		{
			SetResultFailure("The square-count ceiling did not grow a 1 m square size over a 600 m world; this half of the case has nothing to assert against", coarse.m_fCellSize.ToString());
			return true;
		}

		if (!OVT_TEST_Logic_TerritoryFixture.IsMultipleOf(coarse.m_fOriginX, coarse.m_fCellSize))
		{
			SetResultFailure("After the ceiling grew the squares to %1 m the grid still started at %2, which is not a multiple of them; the origin has to be snapped to the size the grid was BUILT at, not the one that was configured", coarse.m_fCellSize.ToString(), coarse.m_fOriginX.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}
