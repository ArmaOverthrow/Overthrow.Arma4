//------------------------------------------------------------------------------------------------
//! One input to the territory solve: a place that projects influence onto the map.
//!
//! Deliberately a dumb record with no behaviour and no engine dependency. The map layer builds
//! these from towns, military bases, radio towers and resistance FOBs at collect time; the solver
//! consumes them and never asks where they came from. That is what keeps the solver world-free and
//! therefore testable, and it is also the seam a future intel epic extends - a site whose control
//! is unknown to the viewing player simply never gets built.
//------------------------------------------------------------------------------------------------
class OVT_TerritorySite : Managed
{
	//! Sentinel for m_fSupportFraction meaning THIS SITE TYPE HAS NO POPULAR SUPPORT TO MEASURE.
	//!
	//! It is a distinct state rather than a value: a base, a radio tower and a FOB are binary-held and
	//! have no support figure at all, which is not the same thing as a town nobody supports. Zero would
	//! conflate the two, and a contested threshold configured at zero would then mark every base on the
	//! map as contested.
	static const float SUPPORT_NONE = -1;

	//! World position of the site. Only X and Z participate in the solve.
	vector m_vPos;

	//! Multiplicative influence weight. A point belongs to the site minimising dist / weight, so a
	//! weight of 2 reaches twice as far against an equal rival. Must be > 0 to be considered.
	//!
	//! THERE IS NO REACH CAP BESIDE IT ANY MORE, and there is nowhere to put one: ground is classified
	//! square by square against every site, so weight decides where a boundary SITS and nothing decides
	//! how far a region may extend. Territory runs until it meets water or a competing site, which is
	//! what D6 asked for and is now structural rather than enforced.
	float m_fWeight;

	//! Index of the controlling faction, as carried by the underlying campaign record.
	int m_iFactionIndex;

	//! Fraction of this site's population that supports the resistance, 0..1, or SUPPORT_NONE where the
	//! type has no support to measure.
	//!
	//! IT REPLACED A HOLD-STRENGTH FIELD FED FROM TOWN STABILITY, and the swap is the point rather than a
	//! rename. Stability drove a continuous per-site fill alpha, which turned an island held by one
	//! faction into a patchwork of differently-shaded blobs; support drives a two-state CONTESTED mark on
	//! ground the occupier still controls, which is a second axis over an accurate first one instead of a
	//! second opinion about who holds what. Nothing in the overlay reads stability any more.
	float m_fSupportFraction;

	//! Site type id ("town" / "base" / "radiotower" / "fob"), matching the layer's config records.
	string m_sTypeId;
}
