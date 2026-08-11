//------------------------------------------------------------------------------------------------
//! THE OWNERSHIP GRID: which site owns each square of the world, and which squares are water.
//!
//! WHY THIS REPLACED A STAR POLYGON PER SITE. The ray-march that came before rested on one property -
//! that a cell is star-shaped about its own site, so a radius per angle describes it completely. That
//! is true of a boundary against a RIVAL and false of a boundary against a COASTLINE: a ray stops at
//! the FIRST water it meets, so land on the far side of a bay that legitimately belongs to a town could
//! never be filled, and two neighbouring polygons inscribed the same curved boundary with different
//! vertices and therefore alternated gap and overlap along it. More rays shrank that; nothing removed
//! it.
//!
//! A GRID HAS NO TOPOLOGY TO GET WRONG. Every square is classified on its own - is it land, and which
//! site wins it under the same weighted ownership test as before - so bays, inlets, isthmuses and
//! offshore islands all work with no special case anywhere. Two regions cannot overlap because a square
//! has exactly one owner, and they cannot leave a gap because the squares tile. Those two failures stop
//! being bugs to fix and become impossible to express.
//!
//! WHAT IT COSTS. (worldSpan / cellSize)^2 squares, each one land test plus one pass over the sites.
//! On a ~12 km world at 100 m that is ~14,400 squares, ~29,000 terrain samples and ~576,000 ownership
//! comparisons - CHEAPER than the march it replaced, which paid rays x steps x sites. The cost is
//! QUADRATIC in the cell size, so halving it is four times the work.
//!
//! WORLD SPACE ONLY. Nothing here is ever screen space; the layer projects at emit time.
//------------------------------------------------------------------------------------------------
class OVT_TerritoryGrid : Managed
{
	//! World X and Z of the grid's minimum corner - the corner of square (0,0), not its centre.
	float m_fOriginX;
	float m_fOriginZ;

	//! EFFECTIVE square size in metres. It may be COARSER than the configured value: the builder grows
	//! it rather than allocate an unbounded grid, so a mistyped cell size costs resolution instead of
	//! hanging the game. Whatever ends up here is what the geometry was actually built at.
	float m_fCellSize;

	int m_iCols;
	int m_iRows;

	//! Owning site index per square, ROW-MAJOR (index = row * cols + col).
	//!
	//! -1 means NOBODY OWNS THIS SQUARE, which covers both water and the (impossible in practice) case
	//! of a land square no site could win. The two are deliberately one value: neither is drawn, and
	//! neither is a frontier.
	ref array<int> m_aOwner;

	//------------------------------------------------------------------------------------------------
	//! Owner of one square, safe outside the grid.
	//!
	//! OUT OF RANGE RETURNS -1, and that is load-bearing rather than defensive: the contour tracer walks
	//! one square PAST every edge so a region touching the edge of the world still closes into a loop,
	//! and it relies on the world outside the grid reading as unowned.
	//! \param[in] c Column.
	//! \param[in] r Row.
	//! \return Site index, or -1 for water, unowned, or outside the grid.
	int OwnerAt(int c, int r)
	{
		if (c < 0 || c >= m_iCols)
			return -1;

		if (r < 0 || r >= m_iRows)
			return -1;

		if (!m_aOwner)
			return -1;

		int index = r * m_iCols + c;
		if (index < 0 || index >= m_aOwner.Count())
			return -1;

		return m_aOwner[index];
	}

	//------------------------------------------------------------------------------------------------
	//! World X of a square's CENTRE, which is where ownership is sampled.
	//! \param[in] c Column.
	//! \return World X.
	float CentreX(int c)
	{
		return m_fOriginX + (c + 0.5) * m_fCellSize;
	}

	//------------------------------------------------------------------------------------------------
	//! World Z of a square's CENTRE.
	//! \param[in] r Row.
	//! \return World Z.
	float CentreZ(int r)
	{
		return m_fOriginZ + (r + 0.5) * m_fCellSize;
	}

	//------------------------------------------------------------------------------------------------
	//! World X of a square's LOW EDGE, which is where the fill quads are cut. Passing cols gives the
	//! high edge of the last column.
	//! \param[in] c Column.
	//! \return World X.
	float EdgeX(int c)
	{
		return m_fOriginX + c * m_fCellSize;
	}

	//------------------------------------------------------------------------------------------------
	//! World Z of a square's LOW EDGE.
	//! \param[in] r Row.
	//! \return World Z.
	float EdgeZ(int r)
	{
		return m_fOriginZ + r * m_fCellSize;
	}

	//------------------------------------------------------------------------------------------------
	//! Total square count.
	//! \return Columns times rows.
	int CellCount()
	{
		return m_iCols * m_iRows;
	}
}

//------------------------------------------------------------------------------------------------
//! One merged block of same-appearance squares, drawn as a single convex quad.
//!
//! The fill is not one command per square - it is one command per maximal RUN of horizontally adjacent
//! squares that would be drawn identically, optionally grown downward while whole rows match. That is
//! the entire reason a grid fill is affordable: a region a hundred squares across costs one command,
//! not a hundred.
//!
//! BOUNDS ARE INCLUSIVE at both ends, so a single square is col0 == col1 and row0 == row1. The quad it
//! draws spans EdgeX(col0) .. EdgeX(col1 + 1) and EdgeZ(row0) .. EdgeZ(row1 + 1).
//------------------------------------------------------------------------------------------------
class OVT_TerritoryRect : Managed
{
	int m_iCol0;
	int m_iCol1;
	int m_iRow0;
	int m_iRow1;

	//! Index into the layer's appearance table - colour plus whether the block is hatched. Merging is
	//! keyed on this rather than on the owning SITE, because two neighbouring towns of one faction draw
	//! identically and merging them is free; keying on the site would multiply the command count by the
	//! number of sites a row crosses.
	int m_iKey;
}

//------------------------------------------------------------------------------------------------
//! One horizontal slice of a region's FILL, cut between two scan rows and bounded left and right by
//! the region's own smoothed contour.
//!
//! WHY THE FILL IS NOT MERGED SQUARES ANY MORE. It used to be, and the fill and the border were then
//! generated from two different boundaries: the fill from the raw grid runs and the band from the
//! Chaikin-smoothed contour traced through them. They did not coincide - blocky fill jutted past the
//! smooth band in places and fell short of it in others - because nothing made them agree. Deriving
//! BOTH from the smoothed contour makes them agree by construction rather than by tuning.
//!
//! WHY A TRAPEZOID AND NOT A RECTANGLE. A rectangle per scan row would only make the staircase finer.
//! The left and right edges here are taken from where the contour actually crosses the row's TOP and
//! its BOTTOM, so a slanted stretch of border is drawn slanted and the silhouette is exactly the
//! contour, sampled at the row boundaries rather than approximated by steps.
//!
//! EVERY TRAPEZOID IS CONVEX, which is what keeps the draw-state batching valid: right(z) - left(z) is
//! linear in z and non-negative at both ends, so it is non-negative throughout and the ring
//! top-left, top-right, bottom-right, bottom-left is always a simple quad.
//------------------------------------------------------------------------------------------------
class OVT_TerritoryTrapezoid : Managed
{
	//! World Z of the row's top and bottom edges. Both edges are horizontal; only the sides slant.
	float m_fTopZ;
	float m_fBottomZ;

	//! Where the contour crosses the top edge, left and right of the span.
	float m_fTopLeftX;
	float m_fTopRightX;

	//! Where the contour crosses the bottom edge, left and right of the span.
	float m_fBottomLeftX;
	float m_fBottomRightX;

	//! Index into the layer's appearance table - colour plus whether the block is hatched. The same key
	//! the contour it was cut from carries, so the fill and the band cannot end up on different tables.
	int m_iKey;
}

//------------------------------------------------------------------------------------------------
//! One closed boundary loop of a drawn region, in world space, with a per-segment frontier flag.
//!
//! Produced by marching squares over the boolean field "does this square belong to the region being
//! outlined", which is what makes the border independent of the blocky grid underneath it: the grid is
//! square-edged by construction, the border is a traced and smoothed curve.
//!
//! IT IS ALSO THE FILL'S OWN BOUNDARY. The region is scanline-decomposed into trapezoids between these
//! points, so there is exactly ONE boundary per drawn region and it is used twice - once to fill and
//! once to band. A second boundary derived some other way is what made the fill and the band disagree.
//!
//! CLOSED AND IMPLICIT. Segment i runs from point i to point (i + 1) % count, so the loop closes
//! without repeating its first point, and m_aFrontier is exactly as long as m_aX.
//------------------------------------------------------------------------------------------------
class OVT_TerritoryContour : Managed
{
	ref array<float> m_aX;
	ref array<float> m_aZ;

	//! Whether segment i is an inter-faction FRONTIER and therefore earns the neutral band.
	//!
	//! A segment against WATER is not a frontier - a coastline is nobody's border - and neither is a
	//! segment against the same faction. That rule is unchanged from the ray-march it replaces; only the
	//! thing being classified moved, from a ray to a contour segment.
	ref array<bool> m_aFrontier;

	//! Faction this loop outlines, carried so the band can take the region's own hue without a faction
	//! lookup on the per-frame path.
	int m_iFactionIndex;

	//! Index into the layer's appearance table of the region this loop bounds.
	//!
	//! A REGION IS ONE APPEARANCE, not one faction, and that is what lets the fill be cut from the same
	//! curve as the band. A faction's held ground and its contested ground are different appearances, so
	//! they are traced separately and the seam between them is a real contour - but it is a SAME-FACTION
	//! boundary, so it earns no band and the two fills tile against it exactly.
	int m_iAppearanceKey;

	//! Band colour, resolved once when the contour is built. The band is the region's OWN colour at a
	//! heavier alpha, never a third hue: it marks WHOSE frontier this is.
	int m_iBandColour;
}
